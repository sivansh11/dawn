#include "dawn/machine.hpp"

#include <cassert>
#include <cinttypes>
#include <cstdint>
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

std::optional<machine_t> machine_t::load_elf(
    const std::filesystem::path& path) {
  ELFIO::elfio reader;
  if (!reader.load(path)) return std::nullopt;

  machine_t state{};

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

  uint8_t* base            = new uint8_t[1024 * 1024 * 4];
  state._memory            = memory_t::create(base, 1024 * 1024 * 4);
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

    if (!state._memory.memcpy_host_to_guest(
            virtual_address, reinterpret_cast<const void*>(segment->get_data()),
            file_size))
      return std::nullopt;
    state._memory.insert_memory(
        state._memory.translate_guest_to_host(virtual_address), file_size,
        protection, nullptr, nullptr);
    if (memory_size - file_size) {
      if (!state._memory.memset(virtual_address + file_size, 0,
                                memory_size - file_size))
        return std::nullopt;
      state._memory.insert_memory(
          reinterpret_cast<void*>(
              reinterpret_cast<size_t>(
                  state._memory.translate_guest_to_host(virtual_address)) +
              file_size),
          memory_size - file_size, memory_protection_t::e_read_write, nullptr,
          nullptr);
    }
  }
  state._memory.insert_memory(state._memory.translate_guest_to_host(guest_max),
                              state._memory._size - guest_max,
                              memory_protection_t::e_read_write, nullptr,
                              nullptr);
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

machine_t machine_t::create(uint64_t size, uint64_t offset) {
  machine_t machine{};

  uint8_t* base              = new uint8_t[size];
  machine._memory            = memory_t::create(base, size);
  machine._memory.guest_base = offset;

  machine._memory.insert_memory(base, size, memory_protection_t::e_all, nullptr,
                                nullptr);

  return machine;
}

bool machine_t::add_syscall(uint64_t number, syscall_t syscall) {
  auto itr = _syscalls.find(number);
  if (itr != _syscalls.end()) return false;
  _syscalls[number] = syscall;
  return true;
}

bool machine_t::del_syscall(uint64_t number) {
  auto itr = _syscalls.find(number);
  if (itr == _syscalls.end()) return false;
  _syscalls.erase(number);
  return true;
}

std::optional<uint32_t> machine_t::fetch_instruction() {
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

void machine_t::_write_csr(uint16_t address, uint64_t value) {
  // TODO: more involved csr writes
  // TODO: log
  _csr[address] = value;
}
uint64_t machine_t::_read_csr(uint16_t address) {
  // TODO: more involved csr reads
  // TODO: log
  return _csr[address];
}

bool machine_t::write_csr(uint32_t instruction, uint64_t value) {
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
std::optional<uint64_t> machine_t::read_csr(uint32_t instruction) {
  riscv::instruction_t riscv_instruction;
  reinterpret_cast<uint32_t&>(riscv_instruction) = instruction;
  return _read_csr(riscv_instruction.as.i_type.imm());
}

bool machine_t::handle_trap(riscv::exception_code_t cause, uint64_t value) {
#if 1
  // hack for host system calls
  if (cause == riscv::exception_code_t::e_ecall_m_mode ||
      cause == riscv::exception_code_t::e_ecall_u_mode) {
    auto itr = _syscalls.find(_reg[17]);
    if (itr != _syscalls.end()) {
      itr->second(*this);

      _pc += 4;
      return true;
    }
  }
#endif
  if (cause == riscv::exception_code_t::e_illegal_instruction ||
      cause == riscv::exception_code_t::e_instruction_access_fault) {
    std::cerr << "got instruction: " << std::hex << value
              << " at pc: " << std::hex << _pc << '\n';
    std::cerr.flush();
    throw std::runtime_error("illegal instruction or instruction access fault");
  }

  bool is_interrupt =
      (static_cast<uint64_t>(cause) & riscv::MCAUSE_INTERRUPT_BIT) != 0;

  uint64_t mstatus    = _read_csr(riscv::MSTATUS);
  bool     global_mie = (mstatus & riscv::MSTATUS_MIE_MASK) != 0;

  if (is_interrupt) {
    uint64_t mie_reg = _read_csr(riscv::MIE);
    uint64_t interrupt_bit =
        1ull << (static_cast<uint64_t>(cause) & ~riscv::MCAUSE_INTERRUPT_BIT);
    bool enabled_individually = (mie_reg & interrupt_bit) != 0;

    // Spec rule: Interrupts are globally enabled if:
    // (current_mode < M_MODE) OR (current_mode == M_MODE AND mstatus.MIE == 1)
    bool interrupts_globally_enabled =
        (_mode < 0b11) || (_mode == 0b11 && global_mie);

    if (!interrupts_globally_enabled || !enabled_individually) {
      return false;
    }
  }

  _is_reserved         = false;
  _reservation_address = 0;

  _write_csr(riscv::MEPC, _pc);
  _write_csr(riscv::MCAUSE, static_cast<uint64_t>(cause));
  _write_csr(riscv::MTVAL, value);

  mstatus = (mstatus & ~riscv::MSTATUS_MPP_MASK) |
            ((static_cast<uint64_t>(_mode) << riscv::MSTATUS_MPP_SHIFT) &
             riscv::MSTATUS_MPP_MASK);
  mstatus =
      (mstatus & ~riscv::MSTATUS_MPIE_MASK) |
      ((global_mie << riscv::MSTATUS_MPIE_SHIFT) & riscv::MSTATUS_MPIE_MASK);
  mstatus &= ~riscv::MSTATUS_MIE_MASK;

  _write_csr(riscv::MSTATUS, mstatus);
  uint64_t mtvec      = _read_csr(riscv::MTVEC);
  uint64_t mtvec_base = mtvec & riscv::MTVEC_BASE_ALIGN_MASK;
  uint64_t mtvec_mode = mtvec & riscv::MTVEC_MODE_MASK;

  if (mtvec_mode == 0b01 && is_interrupt) {
    uint64_t interrupt_code =
        static_cast<uint64_t>(cause) & ~riscv::MCAUSE_INTERRUPT_BIT;
    _pc = mtvec_base + (interrupt_code * 4);
  } else {
    _pc = mtvec_base;
  }
  _mode = 0b11;
  if (_pc == 0) {
    switch (cause) {
      default:
        std::stringstream ss;
        ss << "Error: " << cause << '\n';
        ss << "at: " << std::hex << _pc << std::dec << '\n';
        error(ss.str().c_str());
    }
  }
  return true;
}

bool machine_t::decode_and_exec_instruction(uint32_t instruction) {
  _reg[0] = 0;
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
        handle_trap(riscv::exception_code_t::e_instruction_address_misaligned,
                    addr);
        break;
      }
      _reg[inst.as.j_type.rd()] = _pc + 4;
      _pc                       = addr;
    } break;
    case riscv::op_t::e_jalr: {
      address_t target = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
      address_t next_pc = target & ~1ull;
      if (next_pc % 4 != 0) {
        handle_trap(riscv::exception_code_t::e_instruction_address_misaligned,
                    next_pc);
        break;
      }
      _reg[inst.as.i_type.rd()] = _pc + 4;
      _pc                       = next_pc;
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
          _reg[inst.as.i_type.rd()] =
              static_cast<int64_t>(static_cast<int8_t>(*value));
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lh: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          if (addr % 2 != 0) {
            handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                        addr);
            break;
          }
          auto value = _memory.load_16(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] =
              static_cast<int64_t>(static_cast<int16_t>(*value));
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_lw: {
          address_t addr =
              _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
          if (addr % 4 != 0) {
            handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                        addr);
            break;
          }
          auto value = _memory.load_32(addr);
          if (!value) {
            handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
            break;
          }
          _reg[inst.as.i_type.rd()] =
              static_cast<int64_t>(static_cast<int32_t>(*value));
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
          if (addr % 2 != 0) {
            handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                        addr);
            break;
          }
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
          if (addr % 4 != 0) {
            handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                        addr);
            break;
          }
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
          if (addr % 8 != 0) {
            handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                        addr);
            break;
          }
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
          if (!_memory.store_8(addr, _reg[inst.as.s_type.rs2()])) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sh: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (addr % 2 != 0) {
            handle_trap(riscv::exception_code_t::e_store_address_misaligned,
                        addr);
            break;
          }
          if (!_memory.store_16(addr, _reg[inst.as.s_type.rs2()])) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sw: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (addr % 4 != 0) {
            handle_trap(riscv::exception_code_t::e_store_address_misaligned,
                        addr);
            break;
          }
          if (!_memory.store_32(addr, _reg[inst.as.s_type.rs2()])) {
            handle_trap(riscv::exception_code_t::e_store_access_fault, addr);
            break;
          }
          _pc += 4;
        } break;
        case riscv::store_t::e_sd: {
          address_t addr =
              _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
          if (addr % 8 != 0) {
            handle_trap(riscv::exception_code_t::e_store_address_misaligned,
                        addr);
            break;
          }
          if (!_memory.store_64(addr, _reg[inst.as.s_type.rs2()])) {
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
                                      << (inst.as.i_type.imm() & 0x3f);
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_srli_or_srai: {
          switch (
              static_cast<riscv::srli_or_srai_t>(inst.as.i_type.imm() >> 6)) {
            case riscv::srli_or_srai_t::e_srli: {
              _reg[inst.as.i_type.rd()] =
                  _reg[inst.as.i_type.rs1()] >> (inst.as.i_type.imm() & 0x3f);
              _pc += 4;
            } break;
            case riscv::srli_or_srai_t::e_srai: {
              int64_t rs1_val =
                  static_cast<int64_t>(_reg[inst.as.i_type.rs1()]);
              uint32_t shamt            = inst.as.i_type.imm() & 0x3F;
              _reg[inst.as.i_type.rd()] = rs1_val >> shamt;
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
                  sext(static_cast<uint32_t>(_reg[inst.as.i_type.rs1()]) >>
                           inst.as.i_type.shamt_w(),
                       32);
              _pc += 4;
            } break;
            case riscv::srliw_or_sraiw_t::e_sraiw: {
              _reg[inst.as.i_type.rd()] =
                  sext(static_cast<int32_t>(_reg[inst.as.i_type.rs1()]) >>
                           inst.as.i_type.shamt_w(),
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
      switch (inst.as.r_type.funct7()) {
        case riscv::r_type_func7_t::e_0000000: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_add: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] + _reg[inst.as.r_type.rs2()];
              _pc += 4;
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
            case riscv::r_type_func3_t::e_srl: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] >>
                  (_reg[inst.as.r_type.rs2()] & 0b111111);
              _pc += 4;
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
        case riscv::r_type_func7_t::e_0100000: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_sub: {
              _reg[inst.as.r_type.rd()] =
                  _reg[inst.as.r_type.rs1()] - _reg[inst.as.r_type.rs2()];
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_sra: {
              _reg[inst.as.r_type.rd()] =
                  static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) >>
                  (_reg[inst.as.r_type.rs2()] & 0b111111);
              _pc += 4;
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::r_type_func7_t::e_0000001: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_mul: {
              uint64_t rs1              = _reg[inst.as.r_type.rs1()];
              uint64_t rs2              = _reg[inst.as.r_type.rs2()];
              _reg[inst.as.r_type.rd()] = rs1 * rs2;
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_mulh: {
              int64_t  rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
              int64_t  rs2 = static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);
              uint64_t result[2];
              mul_64x64_u(rs1, rs2, result);
              uint64_t result_hi = result[1];
              if (rs1 < 0) result_hi -= rs2;
              if (rs2 < 0) result_hi -= rs1;
              _reg[inst.as.r_type.rd()] = result_hi;
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_mulhsu: {
              int64_t  rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
              uint64_t rs2 = _reg[inst.as.r_type.rs2()];
              uint64_t result[2];
              mul_64x64_u(rs1, rs2, result);
              uint64_t result_hi = result[1];
              if (rs1 < 0) result_hi -= rs2;
              _reg[inst.as.r_type.rd()] = result_hi;
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_mulhu: {
              uint64_t rs1 = _reg[inst.as.r_type.rs1()];
              uint64_t rs2 = _reg[inst.as.r_type.rs2()];
              uint64_t result[2];
              mul_64x64_u(rs1, rs2, result);
              _reg[inst.as.r_type.rd()] = result[1];
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_div: {
              int64_t rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
              int64_t rs2 = static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);

              if (rs1 == INT64_MIN && rs2 == -1) {
                _reg[inst.as.r_type.rd()] = INT64_MIN;
              } else if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = ~0ull;
              } else {
                _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 / rs2);
              }
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_divu: {
              uint64_t rs1 = _reg[inst.as.r_type.rs1()];
              uint64_t rs2 = _reg[inst.as.r_type.rs2()];
              if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = ~0ull;
              } else {
                _reg[inst.as.r_type.rd()] = rs1 / rs2;
              }
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_rem: {
              int64_t rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
              int64_t rs2 = static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);

              if (rs1 == INT64_MIN && rs2 == -1) {
                _reg[inst.as.r_type.rd()] = 0;
              } else if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = rs1;
              } else {
                _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 % rs2);
              }
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_remu: {
              uint64_t rs1 = _reg[inst.as.r_type.rs1()];
              uint64_t rs2 = _reg[inst.as.r_type.rs2()];
              if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = rs1;
              } else {
                _reg[inst.as.r_type.rd()] = rs1 % rs2;
              }
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
    case riscv::op_t::e_r_type_32: {
      switch (inst.as.r_type.funct7()) {
        case riscv::r_type_func7_t::e_0000000: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_addw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) +
                           static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]),
                       32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_sllw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<int32_t>(_reg[inst.as.r_type.rs1()])
                           << (_reg[inst.as.r_type.rs2()] & 0b11111),
                       32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_srlw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) >>
                           (_reg[inst.as.r_type.rs2()] & 0b11111),
                       32);
              _pc += 4;
            } break;
            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::r_type_func7_t::e_0100000: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_subw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) -
                           static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]),
                       32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_sraw: {
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<int32_t>(_reg[inst.as.r_type.rs1()]) >>
                           (_reg[inst.as.r_type.rs2()] & 0b11111),
                       32);
              _pc += 4;
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::r_type_func7_t::e_0000001: {
          switch (inst.as.r_type.funct3()) {
            case riscv::r_type_func3_t::e_mulw: {
              _reg[inst.as.r_type.rd()] = ::dawn::sext(
                  static_cast<int32_t>(_reg[inst.as.r_type.rs1()]) *
                      static_cast<int32_t>(_reg[inst.as.r_type.rs2()]),
                  32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_divw: {
              int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
              int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
              if (rs1 == INT32_MIN && rs2 == -1) {
                _reg[inst.as.r_type.rd()] = INT32_MIN;
              } else if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = ~0ull;
              } else {
                _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 / rs2);
              }
              _reg[inst.as.r_type.rd()] =
                  ::dawn::sext(_reg[inst.as.r_type.rd()], 32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_divuw: {
              uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
              uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
              if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = ~0u;
              } else {
                _reg[inst.as.r_type.rd()] = rs1 / rs2;
              }
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rd()]), 32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_remw: {
              int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
              int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);

              if (rs1 == INT32_MIN && rs2 == -1) {
                _reg[inst.as.r_type.rd()] = 0;
              } else if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = rs1;
              } else {
                _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 % rs2);
              }
              _reg[inst.as.r_type.rd()] =
                  ::dawn::sext(_reg[inst.as.r_type.rd()], 32);
              _pc += 4;
            } break;
            case riscv::r_type_func3_t::e_remuw: {
              uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
              uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
              if (rs2 == 0) {
                _reg[inst.as.r_type.rd()] = rs1;
              } else {
                _reg[inst.as.r_type.rd()] = rs1 % rs2;
              }
              _reg[inst.as.r_type.rd()] =
                  sext(static_cast<uint32_t>(_reg[inst.as.r_type.rd()]), 32);
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
    case riscv::op_t::e_fence: {
#ifndef NDEBUG
      // std::cerr << "Note: Fence encountered, fence is not implemented or "
      //              "required\n";
#endif
      _pc += 4;
    } break;
    case riscv::op_t::e_system: {
      switch (inst.as.i_type.funct3()) {
        case riscv::i_type_func3_t::e_sub_system: {
          switch (static_cast<riscv::sub_system_t>(inst.as.i_type.imm())) {
            case riscv::sub_system_t::e_ecall: {
              if (_mode == 0b11)
                handle_trap(riscv::exception_code_t::e_ecall_m_mode, 0);
              if (_mode == 0b00)
                handle_trap(riscv::exception_code_t::e_ecall_u_mode, 0);
            } break;
            case riscv::sub_system_t::e_ebreak: {
              // TODO: make this debug only
              // std::cout << "pc: " << std::hex << _pc << "\n" << std::dec;
              // for (uint32_t i = 0; i < 32; i++) {
              //   if (_reg[i] != 0)
              //     std::cout << "x" << i << ": " << std::hex << _reg[i]
              //               << std::dec << '\n';
              // }
              // std::cout << '\n';
              // std::cout.flush();
              handle_trap(riscv::exception_code_t::e_breakpoint, instruction);
            } break;
            case riscv::sub_system_t::e_mret: {
              uint64_t mstatus = _read_csr(riscv::MSTATUS);
              uint64_t mpp     = (mstatus & riscv::MSTATUS_MPP_MASK) >>
                             riscv::MSTATUS_MPP_SHIFT;
              uint64_t mpie = (mstatus & riscv::MSTATUS_MPIE_MASK) >>
                              riscv::MSTATUS_MPIE_SHIFT;
              _mode   = mpp;
              _pc     = _read_csr(riscv::MEPC);
              mstatus = (mstatus & ~riscv::MSTATUS_MIE_MASK) |
                        (mpie << riscv::MSTATUS_MIE_SHIFT);
              mstatus = (mstatus & ~riscv::MSTATUS_MPIE_MASK) |
                        (1u << riscv::MSTATUS_MPIE_SHIFT);
              mstatus = (mstatus & ~riscv::MSTATUS_MPP_MASK) |
                        (0b00u << riscv::MSTATUS_MPP_SHIFT);
              _write_csr(riscv::MSTATUS, mstatus);
            } break;
            case riscv::sub_system_t::e_wfi: {
              _paused = true;
              _pc += 4;
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
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, *t & ~_reg[inst.as.i_type.rs1()])) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrwi: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, inst.as.i_type.rs1())) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrsi: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, *t | inst.as.i_type.rs1())) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;
        case riscv::i_type_func3_t::e_csrrci: {
          auto t = read_csr(instruction);
          if (!t) break;
          if (!write_csr(instruction, *t & ~inst.as.i_type.rs1())) break;
          _reg[inst.as.i_type.rd()] = *t;
          _pc += 4;
        } break;

        default:
          handle_trap(riscv::exception_code_t::e_illegal_instruction,
                      instruction);
      }
    } break;
    case riscv::op_t::e_a_type: {  // 0101111
      switch (inst.as.a_type.funct3()) {
        case riscv::a_type_func3_t::e_w: {  // 010
          switch (inst.as.a_type.funct5()) {
            case riscv::a_type_func5_t::e_lr: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              _reservation_address      = addr;
              _is_reserved              = true;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_sc: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_store_address_misaligned,
                            addr);
                break;
              }
              if (_is_reserved && _reservation_address == addr) {
                if (!_memory.store_32(addr, static_cast<uint32_t>(rs2))) {
                  handle_trap(riscv::exception_code_t::e_store_access_fault,
                              addr);
                  break;
                }
                _reg[inst.as.a_type.rd()] = 0;
              } else {
                _reg[inst.as.a_type.rd()] = 1;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoswap: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr, static_cast<uint32_t>(rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoadd: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    static_cast<uint32_t>(*value + rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoxor: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    static_cast<uint32_t>(*value ^ rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoand: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    static_cast<uint32_t>(*value & rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoor: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    static_cast<uint32_t>(*value | rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomin: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr, static_cast<uint32_t>(std::min(
                                              static_cast<int32_t>(*value),
                                              static_cast<int32_t>(rs2))))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomax: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr, static_cast<uint32_t>(std::max(
                                              static_cast<int32_t>(*value),
                                              static_cast<int32_t>(rs2))))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amominu: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    std::min(static_cast<uint32_t>(*value),
                                             static_cast<uint32_t>(rs2)))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomaxu: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 4;  // 4 for w
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_32(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = sext(*value, 32);
              if (!_memory.store_32(addr,
                                    std::max(static_cast<uint32_t>(*value),
                                             static_cast<uint32_t>(rs2)))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;

            default:
              handle_trap(riscv::exception_code_t::e_illegal_instruction,
                          instruction);
          }
        } break;
        case riscv::a_type_func3_t::e_d: {  // 011
          switch (inst.as.a_type.funct5()) {
            case riscv::a_type_func5_t::e_lr: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              _reservation_address      = addr;
              _is_reserved              = true;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_sc: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_store_address_misaligned,
                            addr);
                break;
              }
              if (_is_reserved && _reservation_address == addr) {
                if (!_memory.store_64(addr, rs2)) {
                  handle_trap(riscv::exception_code_t::e_store_access_fault,
                              addr);
                  break;
                }
                _reg[inst.as.a_type.rd()] = 0;
              } else {
                _reg[inst.as.a_type.rd()] = 1;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoswap: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, rs2)) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoadd: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, *value + rs2)) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoxor: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, *value ^ rs2)) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoand: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, *value & rs2)) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amoor: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, *value | rs2)) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomin: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, static_cast<uint64_t>(std::min(
                                              static_cast<int64_t>(*value),
                                              static_cast<int64_t>(rs2))))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomax: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, static_cast<uint64_t>(std::max(
                                              static_cast<int64_t>(*value),
                                              static_cast<int64_t>(rs2))))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amominu: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, std::min(*value, rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
              _pc += 4;
            } break;
            case riscv::a_type_func5_t::e_amomaxu: {
              const uint64_t  rs1       = _reg[inst.as.a_type.rs1()];
              const uint64_t  rs2       = _reg[inst.as.a_type.rs2()];
              const address_t addr      = rs1;
              const uint32_t  alignment = 8;  // 8 for d
              if (addr % alignment != 0) {
                handle_trap(riscv::exception_code_t::e_load_address_misaligned,
                            addr);
                break;
              }
              auto value = _memory.load_64(addr);
              if (!value) {
                handle_trap(riscv::exception_code_t::e_load_access_fault, addr);
                break;
              }
              _reg[inst.as.a_type.rd()] = *value;
              if (!_memory.store_64(addr, std::max(*value, rs2))) {
                handle_trap(riscv::exception_code_t::e_store_access_fault,
                            addr);
                break;
              }
              _is_reserved         = false;
              _reservation_address = 0;
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

    default:
      handle_trap(riscv::exception_code_t::e_illegal_instruction, instruction);
  }
  return true;
}

void machine_t::simulate(uint64_t num_instructions) {
  for (uint64_t instructions = 0; instructions < num_instructions;
       instructions++) {
    if (!_running) return;
    auto instruction = fetch_instruction();
    if (instruction) decode_and_exec_instruction(*instruction);
  }
}

}  // namespace dawn
