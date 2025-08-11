#include "dawn/state.hpp"

#include <cassert>
#include <cinttypes>
#include <elfio/elf_types.hpp>
#include <elfio/elfio.hpp>
#include <elfio/elfio_section.hpp>
#include <elfio/elfio_symbols.hpp>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "dawn/helper.hpp"
#include "dawn/memory.hpp"
#include "dawn/types.hpp"

namespace dawn {

std::optional<state_t> state_t::load_elf(const std::filesystem::path& path) {
  ELFIO::elfio reader;
  if (!reader.load(path)) return std::nullopt;

  state_t state{};

  address_t guest_base = std::numeric_limits<address_t>::max();
  address_t guest_max  = std::numeric_limits<address_t>::min();
  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();

    guest_base = std::min(guest_base, virtual_address);
    guest_max  = std::max(guest_max, virtual_address + memory_size);
  }
  state._memory =
      memory_t::create(new uint8_t[1024 * 1024 * 4], 1024 * 1024 * 4);

  state._memory.guest_base = guest_base;

  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();
    bool              is_read         = segment->get_flags() & ELFIO::PF_R;
    bool              is_write        = segment->get_flags() & ELFIO::PF_W;
    bool              is_exec         = segment->get_flags() & ELFIO::PF_X;

    memory_protection_t protection{};
    if (is_read) protection = protection | memory_protection_t::e_read;
    if (is_write) protection = protection | memory_protection_t::e_write;
    if (is_exec) protection = protection | memory_protection_t::e_exec;

    state._memory.memcpy_host_to_guest(
        virtual_address, reinterpret_cast<const void*>(segment->get_data()),
        file_size);
    state._memory.insert_memory(
        state._memory.translate_guest_to_host(virtual_address), file_size,
        protection);
    if (memory_size - file_size) {
      state._memory.memset(virtual_address + file_size, 0,
                           memory_size - file_size);
      state._memory.insert_memory(
          reinterpret_cast<void*>(
              reinterpret_cast<size_t>(
                  state._memory.translate_guest_to_host(virtual_address)) +
              file_size),
          memory_size - file_size, memory_protection_t::e_read_write);
    }
  }
  state._memory.insert_memory(state._memory.translate_guest_to_host(guest_max),
                              state._memory._size - guest_max,
                              memory_protection_t::e_read_write);
  // TODO: insert a byte with memory_protection_t::e_none to prevent
  // stack overflow
  for (uint32_t i = 0; i < reader.sections.size(); i++) {
    ELFIO::section* section = reader.sections[i];
    if (section->get_type() != ELFIO::SHT_SYMTAB) continue;
    const ELFIO::symbol_section_accessor symbols(reader, section);

    for (uint32_t j = 0; j < symbols.get_symbols_num(); j++) {
      std::string       name;
      ELFIO::Elf64_Addr value;
      ELFIO::Elf_Xword  size;
      unsigned char     bind;
      unsigned char     type;
      ELFIO::Elf_Half   section_index;
      unsigned char     other;

      symbols.get_symbol(j, name, value, size, bind, type, section_index,
                         other);
      if (name == "_end") {
        state._heap_address = value;
        break;
      }
    }
    assert(state._heap_address != 0);
  }
  state._pc     = reader.get_entry();
  state._reg[2] = state._memory._size - 8;

  return state;
}

bool state_t::add_syscall(uint64_t number, syscall_t syscall) {
  auto itr = _syscalls.find(number);
  if (itr != _syscalls.end()) return false;
  _syscalls[number] = syscall;
  return true;
}

bool state_t::del_syscall(uint64_t number) {
  auto itr = _syscalls.find(number);
  if (itr == _syscalls.end()) return false;
  _syscalls.erase(number);
  return true;
}

std::optional<uint32_t> state_t::fetch_instruction() {
  if (_pc % 4 != 0) {
    handle_trap(riscv::exception_code_t::e_instruction_address_misaligned, _pc);
    return std::nullopt;
  }
  auto instruction = _memory.fetch_32(_pc);
  if (!instruction) {
    handle_trap(riscv::exception_code_t::e_instruction_access_fault, _pc);
    return std::nullopt;
  }
  return instruction.value();
}

void state_t::_write_csr(uint16_t address, uint64_t value) {
  // TODO: more involved csr writes
  _csr[address] = value;
}
uint64_t state_t::_read_csr(uint16_t address) {
  // TODO: more involved csr reads
  return _csr[address];
}

bool state_t::write_csr(uint32_t instruction, uint64_t value) {
  riscv::instruction_t riscv_instruction;
  reinterpret_cast<uint32_t&>(riscv_instruction) = instruction;
  if ((riscv_instruction.as.i_type.imm() >> 10) == 0b11 &&
      riscv_instruction.as.i_type.rs1() != 0) {
    handle_trap(riscv::exception_code_t::e_illegal_instruction, instruction);
    return false;
  }
  _write_csr(riscv_instruction.as.i_type.imm(), value);
  return true;
}
// TODO: maybe this doesnt need to be an optional ?
std::optional<uint64_t> state_t::read_csr(uint32_t instruction) {
  riscv::instruction_t riscv_instruction;
  reinterpret_cast<uint32_t&>(riscv_instruction) = instruction;
  return _read_csr(riscv_instruction.as.i_type.imm());
}

void state_t::handle_trap(riscv::exception_code_t cause, uint64_t value) {
  _write_csr(riscv::MEPC, value);
  _write_csr(riscv::MCAUSE, value);
  _write_csr(riscv::MTVAL, value);

  uint64_t mstatus         = _read_csr(riscv::MSTATUS);
  uint64_t current_mie_bit = (mstatus & riscv::MSTATUS_MIE_MASK) ? 1 : 0;
  mstatus                  = (mstatus & ~riscv::MSTATUS_MPP_MASK) |
            ((static_cast<uint64_t>(11) << riscv::MSTATUS_MPP_SHIFT) &
             riscv::MSTATUS_MPP_MASK);
  mstatus = (mstatus & ~riscv::MSTATUS_MPIE_MASK) |
            ((current_mie_bit << riscv::MSTATUS_MPIE_SHIFT) &
             riscv::MSTATUS_MPIE_MASK);
  mstatus &= ~riscv::MSTATUS_MIE_MASK;

  _write_csr(riscv::MSTATUS, mstatus);
  uint64_t mtvec      = _read_csr(riscv::MTVEC);
  uint64_t mtvec_base = mtvec & riscv::MTVEC_BASE_ALIGN_MASK;
  uint64_t mtvec_mode = mtvec & riscv::MTVEC_MODE_MASK;

  bool is_interrupt =
      (static_cast<uint64_t>(cause) & riscv::MCAUSE_INTERRUPT_BIT) != 0;
  if (mtvec_mode == 0b01 && is_interrupt) {
    uint64_t interrupt_code =
        static_cast<uint64_t>(cause) & ~riscv::MCAUSE_INTERRUPT_BIT;
    _pc = mtvec_base + (interrupt_code * 4);
  } else {
    _pc = mtvec_base;
  }
  if (_pc == 0) {
    switch (cause) {
      default:
        std::stringstream ss;
        ss << "Error: " << cause << '\n';
        ss << "at: " << std::hex << _pc << std::dec << '\n';
        error(ss.str().c_str());
    }
  }
}

bool state_t::decode_and_exec_instruction(uint32_t instruction) {
  riscv::instruction_t inst;
  reinterpret_cast<uint32_t&>(inst) = instruction;
  switch (inst.as.base.opcode()) {
    case riscv::op_t::e_lui: {
      _reg[inst.as.u_type.rd()] = ::dawn::sext(inst.as.u_type.imm() << 12, 32);
      _pc += 4;
    } break;
    case riscv::op_t::e_auipc: {
      _reg[inst.as.u_type.rd()] =
          _pc + ::dawn::sext(inst.as.u_type.imm() << 12, 32);
      _pc += 4;
    } break;
    case riscv::op_t::e_jal: {
      address_t addr = _pc + inst.as.j_type.imm_sext();
      if (addr % 4 != 0) {
        // TODO: verify if instruction address misaligned is correct trap here
        handle_trap(riscv::exception_code_t::e_instruction_address_misaligned,
                    addr);
        break;
      }
      _reg[inst.as.j_type.rd()] = _pc + 4;
      _pc                       = addr;
    } break;
    case riscv::op_t::e_jalr: {
      address_t addr = _pc + 4;
      if (addr % 4 != 0) {
        // TODO: verify if instruction address misaligned is correct trap here
        handle_trap(riscv::exception_code_t::e_instruction_address_misaligned,
                    addr);
        break;
      }
      _pc = (_reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext()) & ~1u;
      _reg[inst.as.i_type.rd()] = addr;
    } break;
    case riscv::op_t::e_branch: {
      switch (inst.as.b_type.funct3()) {
        case riscv::branch_t::e_beq: {
          if (_reg[inst.as.b_type.rs1()] == _reg[inst.as.b_type.rs2()]) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;
        case riscv::branch_t::e_bne: {
          if (_reg[inst.as.b_type.rs1()] != _reg[inst.as.b_type.rs2()]) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;
        case riscv::branch_t::e_blt: {
          if (static_cast<int64_t>(_reg[inst.as.b_type.rs1()]) <
              static_cast<int64_t>(_reg[inst.as.b_type.rs2()])) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;
        case riscv::branch_t::e_bge: {
          if (static_cast<int64_t>(_reg[inst.as.b_type.rs1()]) >=
              static_cast<int64_t>(_reg[inst.as.b_type.rs2()])) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;
        case riscv::branch_t::e_bltu: {
          if (_reg[inst.as.b_type.rs1()] < _reg[inst.as.b_type.rs2()]) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;
        case riscv::branch_t::e_bgeu: {
          if (_reg[inst.as.b_type.rs1()] >= _reg[inst.as.b_type.rs2()]) {
            address_t addr = _pc + inst.as.b_type.imm_sext();
            if (addr % 4 != 0) {
              // TODO: verify if instruction address misaligned is correct trap
              // here
              handle_trap(
                  riscv::exception_code_t::e_instruction_address_misaligned,
                  addr);
              break;
            }
            _pc = addr;
          } else {
            _pc += 4;
          }
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_load: {
      switch (inst.as.i_type.funct3()) {
        case riscv::i_type_func3_t::e_lb: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_8(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = static_cast<int8_t>(*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lh: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_16(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = static_cast<int16_t>(*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lw: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_32(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = static_cast<int32_t>(*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lbu: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_8(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = (*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lhu: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_16(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = (*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lwu: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_32(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = (*value);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_ld: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          auto value = _memory.load_64(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] = (*value);
          _pc += 4;
        } break;
        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_store: {
      switch (inst.as.s_type.funct3()) {
        case riscv::store_t::e_sb: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (!_memory.store_8(addr + inst.as.s_type.imm_sext(),
                               inst.as.s_type.imm_sext())) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sh: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (!_memory.store_16(addr + inst.as.s_type.imm_sext(),
                                inst.as.s_type.imm_sext())) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sw: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (!_memory.store_32(addr + inst.as.s_type.imm_sext(),
                                inst.as.s_type.imm_sext())) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sd: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (!_memory.store_64(addr + inst.as.s_type.imm_sext(),
                                inst.as.s_type.imm_sext())) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_i_type: {
      switch (inst.as.i_type.funct3()) {
        case riscv::i_type_func3_t::e_addi: {
          _reg[inst.as.i_type.rd()] =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_slti: {
          _reg[inst.as.i_type.rd()] =
              static_cast<int64_t>(_reg[inst.as.i_type.rs1()]) <
              inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_sltiu: {
          _reg[inst.as.i_type.rd()] =
              _reg[inst.as.i_type.rs1()] < inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_xori: {
          _reg[inst.as.i_type.rd()] =
              _reg[inst.as.i_type.rs1()] ^ inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_ori: {
          _reg[inst.as.i_type.rd()] =
              _reg[inst.as.i_type.rs1()] | inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_andi: {
          _reg[inst.as.i_type.rd()] =
              _reg[inst.as.i_type.rs1()] & inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_slli: {
          _reg[inst.as.i_type.rd()] = _reg[inst.as.i_type.rs1()]
                                      << inst.as.i_type.imm_sext();
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_srli_or_srai: {
          switch (
              static_cast<riscv::srli_or_srai_t>(inst.as.i_type.imm() >> 6)) {
            case riscv::srli_or_srai_t::e_srli: {
              _reg[inst.as.i_type.rd()] =
                  _reg[inst.as.i_type.rs1()] >> inst.as.i_type.imm_sext();
              _pc += 4;
            } break;
            case riscv::srli_or_srai_t::e_srai: {
              _reg[inst.as.i_type.rd()] =
                  static_cast<int64_t>(_reg[inst.as.i_type.rs1()]) >>
                  inst.as.i_type.imm_sext();
              _pc += 4;
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_i_type_32: {
      switch (inst.as.i_type.funct3()) {
        case riscv::i_type_func3_t::e_addiw: {
          _reg[inst.as.i_type.rd()] =
              sext(static_cast<uint32_t>(_reg[inst.as.i_type.rs1()] +
                                         inst.as.i_type.imm_sext()),
                   32);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_slliw: {
          _reg[inst.as.i_type.rd()] =
              sext(static_cast<uint32_t>(
                       _reg[inst.as.i_type.rs1()]
                       << static_cast<uint32_t>(inst.as.i_type.shamt_w())),
                   32);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_srliw_or_sraiw: {
          switch (
              static_cast<riscv::srliw_or_sraiw_t>(inst.as.i_type.imm() >> 5)) {
            case riscv::srliw_or_sraiw_t::e_srliw: {
              _reg[inst.as.i_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.i_type.rs1()] >>
                                             inst.as.i_type.shamt_w()),
                       32);
              _pc += 4;
            } break;
            case riscv::srliw_or_sraiw_t::e_sraiw: {
              _reg[inst.as.i_type.rd()] =
                  sext(static_cast<int32_t>(_reg[inst.as.i_type.rs1()] >>
                                            inst.as.i_type.shamt_w()),
                       32);
              _pc += 4;
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_r_type: {
      switch (inst.as.r_type.funct3()) {
        case riscv::r_type_func3_t::e_add_or_sub: {
          switch (static_cast<riscv::add_or_sub_t>(inst.as.r_type.funct7())) {
            case riscv::add_or_sub_t::e_add: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] + _reg[inst.as.r_type.rs2()];
              _pc += 4;
            } break;
            case riscv::add_or_sub_t::e_sub: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] - _reg[inst.as.r_type.rs2()];
              _pc += 4;
            } break;
            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::r_type_func3_t::e_sll: {
          _reg[inst.as.r_type.rd()] =
              _reg[inst.as.r_type.rs1()]
              << (_reg[inst.as.r_type.rs2()] & 0b111111);
          _pc += 4;
        } break;
        case riscv::r_type_func3_t::e_slt: {
          _reg[inst.as.r_type.rd()] =
              static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) <
              static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);
          _pc += 4;
        } break;
        case riscv::r_type_func3_t::e_sltu: {
          _reg[inst.as.r_type.rd()] =
              _reg[inst.as.r_type.rs1()] < _reg[inst.as.r_type.rs2()];
          _pc += 4;
        } break;
        case riscv::r_type_func3_t::e_xor: {
          _reg[inst.as.r_type.rd()] =
              _reg[inst.as.r_type.rs1()] ^ _reg[inst.as.r_type.rs2()];
          _pc += 4;
        } break;
        case riscv::r_type_func3_t::e_srl_or_sra: {
          switch (static_cast<riscv::srl_or_sra_t>(inst.as.r_type.funct7())) {
            case riscv::srl_or_sra_t::e_srl: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] ^
                  (_reg[inst.as.r_type.rs2()] & 0b111111);
              _pc += 4;
            } break;
            case riscv::srl_or_sra_t::e_sra: {
              _reg[inst.as.r_type.rd()] =
                  static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) ^
                  (_reg[inst.as.r_type.rs2()] & 0b111111);
              _pc += 4;
            } break;
            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::r_type_func3_t::e_or: {
          _reg[inst.as.r_type.rd()] =
              _reg[inst.as.r_type.rs1()] | _reg[inst.as.r_type.rs2()];
          _pc += 4;
        } break;
        case riscv::r_type_func3_t::e_and: {
          _reg[inst.as.r_type.rd()] =
              _reg[inst.as.r_type.rs1()] & _reg[inst.as.r_type.rs2()];
          _pc += 4;
        } break;
        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_r_type_32: {
      switch (inst.as.r_type.funct3()) {
        case riscv::r_type_func3_t::e_addw_or_subw: {
          switch (static_cast<riscv::addw_or_subw_t>(inst.as.r_type.funct7())) {
            case riscv::addw_or_subw_t::e_addw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) +
                           static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]),
                       32);
              _pc += 4;
            } break;
            case riscv::addw_or_subw_t::e_subw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) -
                           static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]),
                       32);
              _pc += 4;
            } break;
            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
          break;
          case riscv::r_type_func3_t::e_sllw: {
            _reg[inst.as.r_type.rd()] =
                sext(static_cast<int32_t>(_reg[inst.as.r_type.rs1()])
                         << (_reg[inst.as.r_type.rs2()] & 0b11111),
                     32);
            _pc += 4;
          } break;
          case riscv::r_type_func3_t::e_srlw_or_sraw: {
            switch (
                static_cast<riscv::srlw_or_sraw_t>(inst.as.r_type.funct7())) {
              case riscv::srlw_or_sraw_t::e_srlw: {
                _reg[inst.as.r_type.rd()] =
                    sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) >>
                             (_reg[inst.as.r_type.rs2()] & 0b11111),
                         32);
                _pc += 4;
              } break;
              case riscv::srlw_or_sraw_t::e_sraw: {
                _reg[inst.as.r_type.rd()] =
                    sext(static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) >>
                             (_reg[inst.as.r_type.rs2()] & 0b11111),
                         32);
                _pc += 4;
              } break;
              default:
                handle_trap(riscv::exception_code_t::e_illegal_instruction,
                            instruction);
            }
          } break;

          default:
            handle_trap(riscv::exception_code_t::e_illegal_instruction,
                        instruction);
        }
      }
    } break;
    case riscv::op_t::e_fence: {
      std::cerr << "Note: Fence encountered, fence is not implemented or "
                   "required\n";
      _pc += 4;
    } break;
    case riscv::op_t::e_system: {
      switch (inst.as.i_type.funct3()) {
        case riscv::i_type_func3_t::e_sub_system: {
          switch (static_cast<riscv::sub_system_t>(inst.as.i_type.imm())) {
            case riscv::sub_system_t::e_ecall: {
              auto itr = _syscalls.find(_reg[17]);
              if (itr != _syscalls.end()) {
                itr->second(*this);
                _pc += 4;
              } else {
                std::cout << "Error: unhandled ecall - " << _reg[17] << '\n';
              }
            } break;
            case riscv::sub_system_t::e_ebreak: {
              std::cout << "TODO: implement EBREAK\n";
            } break;
            case riscv::sub_system_t::e_mret: {
              uint64_t mstatus = _read_csr(riscv::MSTATUS);
              uint64_t mpp     = (mstatus & riscv::MSTATUS_MPP_MASK) >>
                             riscv::MSTATUS_MPP_SHIFT;
              uint64_t mpie = (mstatus & riscv::MSTATUS_MPIE_MASK) >>
                              riscv::MSTATUS_MPIE_SHIFT;
              _pc     = _read_csr(riscv::MEPC);
              mstatus = (mstatus & ~riscv::MSTATUS_MIE_MASK) |
                        (mpie << riscv::MSTATUS_MIE_SHIFT);
              mstatus = (mstatus & ~riscv::MSTATUS_MPIE_MASK) |
                        (1u << riscv::MSTATUS_MPIE_SHIFT);
              mstatus = (mstatus & ~riscv::MSTATUS_MPP_MASK) |
                        (0b11U << riscv::MSTATUS_MPP_SHIFT);
              _write_csr(riscv::MSTATUS, mstatus);
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::i_type_func3_t::e_csrrw: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, _reg[inst.as.i_type.rs1()])) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrs: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, *t | _reg[inst.as.i_type.rs1()])) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrc: {
        } break;
        case riscv::i_type_func3_t::e_csrrwi: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, inst.as.i_type.rs1())) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrsi: {
        } break;
        case riscv::i_type_func3_t::e_csrrci: {
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    default:
      handle_trap(riscv::exception_code_t::e_illegal_instruction, instruction);
  }
  return true;
}

bool state_t::decode_and_jit_basic_block(uint32_t instruction) {}

void state_t::simulate(uint64_t num_instructions) {
  for (uint64_t instructions = 0; instructions < num_instructions;
       instructions++) {
    if (!_running) return;
    auto instruction = fetch_instruction();
    if (instruction) decode_and_exec_instruction(*instruction);
  }
}

}  // namespace dawn
