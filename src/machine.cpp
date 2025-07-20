#include "machine.hpp"

#include <bitset>
#include <cassert>
#include <cstdint>
#include <elfio/elfio.hpp>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>

#include "elfio/elf_types.hpp"
#include "elfio/elfio_section.hpp"
#include "elfio/elfio_symbols.hpp"
#include "helper.hpp"
#include "types.hpp"

namespace dawn {

machine_t::machine_t(uint64_t memory_size, uint64_t memory_page_size)
    : _memory(memory_size) {
  for (uint32_t i = 0; i < 32; i++) _registers[i] = 0;
  _privilege_mode = privilege_mode_t::e_machine;
}

machine_t::~machine_t() {}

void machine_t::set_syscall(uint64_t number, syscall_t syscall) {
  assert(!_syscalls.contains(number));
  _syscalls[number] = syscall;
}

bool machine_t::load_elf_and_set_program_counter(
    const std::filesystem::path& path) {
  ELFIO::elfio reader;
  if (!reader.load(path)) return false;

  uint64_t guest_base = std::numeric_limits<uint64_t>::max();
  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();

    guest_base = std::min(guest_base, virtual_address);
  }

  _memory._guest_base = guest_base;

  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();

    _memory.memcpy_host_to_guest(
        virtual_address, reinterpret_cast<const void*>(segment->get_data()),
        file_size);
    if (memory_size - file_size) {
      _memory.memset(virtual_address + file_size, 0, memory_size - file_size);
    }
  }
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
        _heap_address = value;
        break;
      }
    }
    assert(_heap_address != 0);
  }
  _program_counter = reader.get_entry();
  _registers[2]    = _memory._size - 8;
  return true;
}

void machine_t::handle_trap(exception_code_t cause_code, uint64_t value) {
  _write_csr(MEPC, _program_counter);
  _write_csr(MCAUSE, (uint64_t)cause_code);
  _write_csr(MTVAL, value);
  uint64_t mstatus         = _read_csr(MSTATUS);
  uint64_t current_mie_bit = (mstatus & MSTATUS_MIE_MASK) ? 1 : 0;
  mstatus                  = (mstatus & ~MSTATUS_MPP_MASK) |
            ((static_cast<uint64_t>(_privilege_mode) << MSTATUS_MPP_SHIFT) &
             MSTATUS_MPP_MASK);
  mstatus = (mstatus & ~MSTATUS_MPIE_MASK) |
            ((current_mie_bit << MSTATUS_MPIE_SHIFT) & MSTATUS_MPIE_MASK);
  mstatus &= ~MSTATUS_MIE_MASK;
  _write_csr(MSTATUS, mstatus);  // Write the updated MSTATUS back
  _privilege_mode       = privilege_mode_t::e_machine;
  uint64_t mtvec        = _read_csr(MTVEC);
  uint64_t mtvec_base   = mtvec & MTVEC_BASE_ALIGN_MASK;
  uint64_t mtvec_mode   = mtvec & MTVEC_MODE_MASK;
  bool     is_interrupt = ((uint64_t)cause_code & MCAUSE_INTERRUPT_BIT) != 0;
  if (mtvec_mode == 0b01 && is_interrupt) {
    uint64_t interrupt_code = (uint64_t)cause_code & ~MCAUSE_INTERRUPT_BIT;
    _program_counter        = mtvec_base + (interrupt_code * 4);
  } else {
    _program_counter = mtvec_base;
  }
}

uint64_t machine_t::_read_csr(uint16_t address) {
  address = address & 0b111111111111;
  switch (address) {
    case MNSTATUS:  // not implemented returning 0
    case SATP:      // not implemented returning 0
    case PMPADDR0:  // not implemented returning 0
    case PMPCFG0:   // not implemented returning 0
    case MHARDID:
      return 0;
    case MSTATUS:
    case MIE:
    case MEDELEG:
    case MIDELEG:
    case MTVEC:
    case MEPC:
    case MCAUSE:
    case MTVAL:
      return _csr[address];
    default:
      std::stringstream ss;
      ss << "Error: csr read not implemented. " << std::hex << address;
      throw std::runtime_error(ss.str());
  }
}
void machine_t::_write_csr(uint16_t address, uint64_t value) {
  address = address & 0b111111111111;
  switch (address) {
    case MNSTATUS:  // not implemented, ignoring writes
    case SATP:      // not implemented, ignoring writes
    case PMPADDR0:  // not implemented, ignoring writes
    case PMPCFG0:   // not implemented, ignoring writes
      break;
    case MSTATUS: {
      uint64_t writeable_mask = 0;
      writeable_mask |= MSTATUS_MIE_MASK;
      writeable_mask |= MSTATUS_MPIE_MASK;
      writeable_mask |= MSTATUS_MPP_MASK;
      _csr[MSTATUS] =
          (_csr[MSTATUS] & ~writeable_mask) | (value & writeable_mask);
    } break;
    case MTVEC: {
      uint64_t mode_bits = value & MTVEC_MODE_MASK;
      uint64_t base_addr = value & MTVEC_BASE_ALIGN_MASK;
      // TODO: trap if mode bits are reserved
      _csr[MTVEC] = base_addr | mode_bits;
    } break;
    case MEPC:
    case MEDELEG:
    case MIDELEG:
    case MCAUSE:
    case MIE:
    case MTVAL:
      _csr[address] = value;
      break;
    default:
      std::stringstream ss;
      ss << "Error: csr write not implemented. " << std::hex << address;
      throw std::runtime_error(ss.str());
  }
}
// TODO: proper privilege checks
uint64_t machine_t::read_csr(uint32_t _instruction) {
  instruction_t instruction;
  reinterpret_cast<uint32_t&>(instruction) = _instruction;
  privilege_mode_t min_privilege_mode =
      static_cast<privilege_mode_t>((instruction.as.i_type.imm() >> 8) & 0b11);
  if (_privilege_mode < min_privilege_mode) {
    handle_trap(exception_code_t::e_illegal_instruction, _instruction);
  }
  return _read_csr(instruction.as.i_type.imm());
}
void machine_t::write_csr(uint32_t _instruction, uint64_t value) {
  instruction_t instruction;
  reinterpret_cast<uint32_t&>(instruction) = _instruction;
  privilege_mode_t min_privilege_mode =
      static_cast<privilege_mode_t>((instruction.as.i_type.imm() >> 8) & 0b11);
  if (_privilege_mode < min_privilege_mode &&
      instruction.as.i_type.rs1() != 0) {
    handle_trap(exception_code_t::e_illegal_instruction, _instruction);
  }
  if ((instruction.as.i_type.imm() >> 10) == 0b11 &&
      instruction.as.i_type.rs1() != 0) {
    handle_trap(exception_code_t::e_illegal_instruction, _instruction);
    return;
  }
  if (instruction.as.i_type.rs1() != 0)
    _write_csr(instruction.as.i_type.imm(), value);
}

void machine_t::simulate(uint64_t steps) {
  for (uint64_t i = 0; i < steps; i++) {
    if (!_running) return;
    auto [instruction, program_counter] =
        fetch_instruction_at_program_counter();
    if (instruction) decode_and_execute_instruction(*instruction);
  }
}

std::pair<std::optional<uint32_t>, uint64_t>
machine_t::fetch_instruction_at_program_counter() {
  // TODO: handle invalid loads
  if (_program_counter % 4 != 0) {
    handle_trap(exception_code_t::e_instruction_address_misaligned,
                _program_counter);
    return {std::nullopt, _program_counter};
  }
  std::optional<uint64_t> value = _memory.load<32>(_program_counter);
  if (!value) {
    handle_trap(exception_code_t::e_load_access_fault, _program_counter);
    return {std::nullopt, _program_counter};
  }
  return {*value, _program_counter};
}
void machine_t::debug_disassemble_instruction(uint32_t      _instruction,
                                              std::ostream& o) {
  instruction_t instruction;
  reinterpret_cast<uint32_t&>(instruction) = _instruction;

  switch (instruction.opcode()) {
    // imm[31:12] rd 0110111 LUI
    case 0b0110111:  // LUI
      o << "lui\n";
      break;
    // imm[31:12] rd 0010111 AUIPC
    case 0b0010111:  // AUIPC
      o << "auipc\n";
      break;
    // imm[20|10:1|11|19:12] rd 1101111 JAL
    case 0b1101111: {  // JAL
      o << "jal\n";
    } break;
    // imm[11:0] rs1 000 rd 1100111 JALR
    case 0b1100111: {  // JALR
      o << "jalr\n";
    } break;

    case 0b1100011:
      switch (instruction.as.b_type.funct3()) {
        // imm[12|10:5] rs2 rs1 000 imm[4:1|11] 1100011 BEQ
        case 0b000:  // BEQ
          o << "beq\n";
          break;
        // imm[12|10:5] rs2 rs1 001 imm[4:1|11] 1100011 BNE
        case 0b001:  // BNE
          o << "bne\n";
          break;
        // imm[12|10:5] rs2 rs1 100 imm[4:1|11] 1100011 BLT
        case 0b100:  // BLT
          o << "blt\n";
          break;
        // imm[12|10:5] rs2 rs1 101 imm[4:1|11] 1100011 BGE
        case 0b101:  // BGE
          o << "bge\n";
          break;
        // imm[12|10:5] rs2 rs1 110 imm[4:1|11] 1100011 BLTU
        case 0b110:  // BLTU
          o << "bltu\n";
          break;
        // imm[12|10:5] rs2 rs1 111 imm[4:1|11] 1100011 BGEU
        case 0b111:  // BGEU
          o << "bgeu\n";
          break;
        default:  // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0000011:
      switch (instruction.as.i_type.funct3()) {
        // imm[11:0] rs1 000 rd 0000011 LB
        case 0b000:  // LB
          o << "lb\n";
          break;
        // imm[11:0] rs1 001 rd 0000011 LH
        case 0b001:  // LH
          o << "lh\n";
          break;
        // imm[11:0] rs1 010 rd 0000011 LW
        case 0b010:  // LW
          o << "lw\n";
          break;
        // imm[11:0] rs1 100 rd 0000011 LBU
        case 0b100:  // LBU
          o << "lbu\n";
          break;
        // imm[11:0] rs1 101 rd 0000011 LHU
        case 0b101:  // LHU
          o << "lhu\n";
          break;
          // imm[11:0] rs1 110 rd 0000011 LWU
        case 0b110:  // LWU
          o << "lwu\n";
          break;
          // imm[11:0] rs1 011 rd 0000011 LD
        case 0b011:  // LD
          o << "ld\n";
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0100011:
      switch (instruction.as.s_type.funct3()) {
        // imm[11:5] rs2 rs1 000 imm[4:0] 0100011 SB
        case 0b000:  // SB
          o << "sb\n";
          break;
        // imm[11:5] rs2 rs1 001 imm[4:0] 0100011 SH
        case 0b001:  // SH
          o << "sh\n";
          break;
        // imm[11:5] rs2 rs1 010 imm[4:0] 0100011 SW
        case 0b010:  // SW
          o << "sw\n";
          break;
        // imm[11:5] rs2 rs1 011 imm[4:0] 0100011 SD
        case 0b011:  // SD
          o << "sd\n";
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0010011:
      switch (instruction.as.i_type.funct3()) {
        // imm[11:0] rs1 000 rd 0010011 ADDI
        case 0b000:  // ADDI
          o << "addi\n";
          break;
        // imm[11:0] rs1 010 rd 0010011 SLTI
        case 0b010:  // SLTI
          o << "slti\n";
          break;
        // imm[11:0] rs1 011 rd 0010011 SLTIU
        case 0b011:  // SLTIU
          o << "sltiu\n";
          break;
        // imm[11:0] rs1 100 rd 0010011 XORI
        case 0b100:  // XORI
          o << "xori\n";
          break;
        // imm[11:0] rs1 110 rd 0010011 ORI
        case 0b110:  // ORI
          o << "ori\n";
          break;
        // imm[11:0] rs1 111 rd 0010011 ANDI
        case 0b111:  // ANDI
          o << "andi\n";
          break;
        // 0000000 shamt rs1 001 rd 0010011 SLLI
        case 0b001:  // SLLI
          o << "slli\n";
          break;

        case 0b101:
          switch (instruction.as.i_type.imm() >> 6) {
            // 000000 shamt rs1 101 rd 0010011 SRLI
            case 0b000000:  // SRLI
              o << "srli\n";
              break;
            // 010000 shamt rs1 101 rd 0010011 SRAI
            case 0b010000:  // SRAI
              o << "srai\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction: ";
              throw std::runtime_error(ss.str());
          }
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0110011:
      switch (instruction.as.r_type.funct3()) {
        case 0b000:
          switch (instruction.as.r_type.funct7()) {
            // 0000000 rs2 rs1 000 rd 0110011 ADD
            case 0b0000000:  // ADD
              o << "add\n";
              break;
            // 0100000 rs2 rs1 000 rd 0110011 SUB
            case 0b0100000:  // SUB
              o << "sub\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;

        // 0000000 rs2 rs1 001 rd 0110011 SLL
        case 0b001:  // SLL
          o << "sll\n";
          break;
        // 0000000 rs2 rs1 010 rd 0110011 SLT
        case 0b010:  // SLT
          o << "slt\n";
          break;
        // 0000000 rs2 rs1 011 rd 0110011 SLTU
        case 0b011:  // SLTU
          o << "sltu\n";
          break;
        // 0000000 rs2 rs1 100 rd 0110011 XOR
        case 0b100:  // XOR
          o << "xor\n";
          break;

        case 0b101:
          switch (instruction.as.r_type.funct7()) {
            // 0000000 rs2 rs1 101 rd 0110011 SRL
            case 0b0000000:  // SRL
              o << "srl\n";
              break;
            // 0100000 rs2 rs1 101 rd 0110011 SRA
            case 0b0100000:  // SRA
              o << "sra\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;

        // 0000000 rs2 rs1 110 rd 0110011 OR
        case 0b110:  // OR
          o << "or\n";
          break;
        // 0000000 rs2 rs1 111 rd 0110011 AND
        case 0b111:  // AND
          o << "and\n";
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    // fm pred succ rs1 000 rd 0001111 FENCE
    case 0b0001111:  // FENCE
      o << "fence\n";
      break;
      // // 1000 0011 0011 00000 000 00000 0001111 FENCE.TSO
      // case 0b0001111:  // FENCE.TSO
      //   break;
      // // 0000 0001 0000 00000 000 00000 0001111 PAUSE
      // case 0b0001111:  // PAUSE
      //   break;

    case 0b1110011:
      switch (instruction.as.i_type.funct3()) {
        case 0b000:
          switch (instruction.as.i_type.imm()) {
            // 000000000000 00000 000 00000 1110011 ECALL
            case 0b000000000000:  // ECALL
              o << "ecall\n";
              break;
            // 000000000001 00000 000 00000 1110011 EBREAK
            case 0b000000000001:  // EBREAK
              o << "ebreak\n";
              break;
            // 001100000010 00000 000 00000 1110011 EBREAK
            case 0b001100000010:  // MRET
              o << "mret\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;
          // csr rs1 001 rd 1110011 CSRRW
        case 0b001:  // CSRRW
          o << "csrrw\n";
          break;
          // csr rs1 010 rd 1110011 CSRRS
        case 0b010:  // CSRRS
          o << "csrrs\n";
          break;
          // csr rs1 011 rd 1110011 CSRRC
        case 0b011:  // CSRRC
          o << "csrrc\n";
          break;
          // csr uimm 101 rd 1110011 CSRRWI
        case 0b101:  // CSRRWI
          o << "csrrwi\n";
          break;
          // csr uimm 110 rd 1110011 CSRRSI
        case 0b110:  // CSRRSI
          o << "csrrsi\n";
          break;
          // csr uimm 111 rd 1110011 CSRRCI
        case 0b111:  // CSRRCI
          o << "csrrci\n";
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0011011:
      switch (instruction.as.i_type.funct3()) {
          // imm[11:0] rs1 000 rd 0011011 ADDIW
        case 0b000:  // ADDIW
          o << "addiw\n";
          break;
          // 0000000 shamt rs1 001 rd 0011011 SLLIW
        case 0b001:  // SLLIW
          o << "slliw\n";
          break;
        case 0b101:
          switch (instruction.as.i_type.imm() >> 5) {
              // 0000000 shamt rs1 101 rd 0011011 SRLIW
            case 0b0000000:  // SRLIW
              o << "srliw\n";
              break;
              // 0100000 shamt rs1 101 rd 0011011 SRAIW
            case 0b0100000:  // SRAIW
              o << "sraiw\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

    case 0b0111011:
      switch (instruction.as.r_type.funct3()) {
        case 0b000:
          switch (instruction.as.r_type.funct7()) {
              // 0000000 rs2 rs1 000 rd 0111011 ADDW
            case 0b0000000:  // ADDW
              o << "addw\n";
              break;
              // 0100000 rs2 rs1 000 rd 0111011 SUBW
            case 0b0100000:  // SUBW
              o << "subw\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;

          // 0000000 rs2 rs1 001 rd 0111011 SLLW
        case 0b001:  // SLLW
          o << "sllw\n";
          break;

        case 0b101:
          switch (instruction.as.r_type.funct7()) {
              // 0000000 rs2 rs1 101 rd 0111011 SRLW
            case 0b0000000:  // SRLW
              o << "srlw\n";
              break;
              // 0100000 rs2 rs1 101 rd 0111011 SRAW
            case 0b0100000:  // SRAW
              o << "sraw\n";
              break;
            default:
              // TODO: raise illegal instruction exception
              std::stringstream ss;
              ss << "Error: illegal instruction";
              throw std::runtime_error(ss.str());
          }
          break;
        default:
          // TODO: raise illegal instruction exception
          std::stringstream ss;
          ss << "Error: illegal instruction";
          throw std::runtime_error(ss.str());
      }
      break;

      // RV32/RV64 Zifencei Standard Extension
      // imm[11:0] rs1 001 rd 0001111 FENCE.I

      // RV32M Standard Extension
      // 0000001 rs2 rs1 000 rd 0110011 MUL
      // 0000001 rs2 rs1 001 rd 0110011 MULH
      // 0000001 rs2 rs1 010 rd 0110011 MULHSU
      // 0000001 rs2 rs1 011 rd 0110011 MULHU
      // 0000001 rs2 rs1 100 rd 0110011 DIV
      // 0000001 rs2 rs1 101 rd 0110011 DIVU
      // 0000001 rs2 rs1 110 rd 0110011 REM
      // 0000001 rs2 rs1 111 rd 0110011 REMU

      // RV64M Standard Extension (in addition to RV32M)
      // 0000001 rs2 rs1 000 rd 0111011 MULW
      // 0000001 rs2 rs1 100 rd 0111011 DIVW
      // 0000001 rs2 rs1 101 rd 0111011 DIVUW
      // 0000001 rs2 rs1 110 rd 0111011 REMW
      // 0000001 rs2 rs1 111 rd 0111011 REMUW

      // RV32F Standard Extension
      // imm[11:0] rs1 010 rd 0000111 FLW
      // imm[11:5] rs2 rs1 010 imm[4:0] 0100111 FSW
      // rs3 00 rs2 rs1 rm rd 1000011 FMADD.S
      // rs3 00 rs2 rs1 rm rd 1000111 FMSUB.S
      // rs3 00 rs2 rs1 rm rd 1001011 FNMSUB.S
      // rs3 00 rs2 rs1 rm rd 1001111 FNMADD.S
      // 0000000 rs2 rs1 rm rd 1010011 FADD.S
      // 0000100 rs2 rs1 rm rd 1010011 FSUB.S
      // 0001000 rs2 rs1 rm rd 1010011 FMUL.S
      // 0001100 rs2 rs1 rm rd 1010011 FDIV.S
      // 0101100 00000 rs1 rm rd 1010011 FSQRT.S
      // 0010000 rs2 rs1 000 rd 1010011 FSGNJ.S
      // 0010000 rs2 rs1 001 rd 1010011 FSGNJN.S
      // 0010000 rs2 rs1 010 rd 1010011 FSGNJX.S
      // 0010100 rs2 rs1 000 rd 1010011 FMIN.S
      // 0010100 rs2 rs1 001 rd 1010011 FMAX.S
      // 1100000 00000 rs1 rm rd 1010011 FCVT.W.S
      // 1100000 00001 rs1 rm rd 1010011 FCVT.WU.S
      // 1110000 00000 rs1 000 rd 1010011 FMV.X.W
      // 1010000 rs2 rs1 010 rd 1010011 FEQ.S
      // 1010000 rs2 rs1 001 rd 1010011 FLT.S
      // 1010000 rs2 rs1 000 rd 1010011 FLE.S
      // 1110000 00000 rs1 001 rd 1010011 FCLASS.S
      // 1101000 00000 rs1 rm rd 1010011 FCVT.S.W
      // 1101000 00001 rs1 rm rd 1010011 FCVT.S.WU
      // 1111000 00000 rs1 000 rd 1010011 FMV.W.X
      //
      // RV64F Standard Extension (in addition to RV32F)
      // 1100000 00010 rs1 rm rd 1010011 FCVT.L.S
      // 1100000 00011 rs1 rm rd 1010011 FCVT.LU.S
      // 1101000 00010 rs1 rm rd 1010011 FCVT.S.L
      // 1101000 00011 rs1 rm rd 1010011 FCVT.S.LU

      // RV32D Standard Extension
      // imm[11:0] rs1 011 rd 0000111 FLD
      // imm[11:5] rs2 rs1 011 imm[4:0] 0100111 FSD
      // rs3 01 rs2 rs1 rm rd 1000011 FMADD.D
      // rs3 01 rs2 rs1 rm rd 1000111 FMSUB.D
      // rs3 01 rs2 rs1 rm rd 1001011 FNMSUB.D
      // rs3 01 rs2 rs1 rm rd 1001111 FNMADD.D
      // 0000001 rs2 rs1 rm rd 1010011 FADD.D
      // 0000101 rs2 rs1 rm rd 1010011 FSUB.D
      // 0001001 rs2 rs1 rm rd 1010011 FMUL.D
      // 0001101 rs2 rs1 rm rd 1010011 FDIV.D
      // 0101101 00000 rs1 rm rd 1010011 FSQRT.D
      // 0010001 rs2 rs1 000 rd 1010011 FSGNJ.D
      // 0010001 rs2 rs1 001 rd 1010011 FSGNJN.D
      // 0010001 rs2 rs1 010 rd 1010011 FSGNJX.D
      // 0010101 rs2 rs1 000 rd 1010011 FMIN.D
      // 0010101 rs2 rs1 001 rd 1010011 FMAX.D
      // 0100000 00001 rs1 rm rd 1010011 FCVT.S.D
      // 0100001 00000 rs1 rm rd 1010011 FCVT.D.S
      // 1010001 rs2 rs1 010 rd 1010011 FEQ.D
      // 1010001 rs2 rs1 001 rd 1010011 FLT.D
      // 1010001 rs2 rs1 000 rd 1010011 FLE.D
      // 1110001 00000 rs1 001 rd 1010011 FCLASS.D
      // 1100001 00000 rs1 rm rd 1010011 FCVT.W.D
      // 1100001 00001 rs1 rm rd 1010011 FCVT.WU.D
      // 1101001 00000 rs1 rm rd 1010011 FCVT.D.W
      // 1101001 00001 rs1 rm rd 1010011 FCVT.D.WU

      // RV64D Standard Extension (in addition to RV32D)
      // 1100001 00010 rs1 rm rd 1010011 FCVT.L.D
      // 1100001 00011 rs1 rm rd 1010011 FCVT.LU.D
      // 1110001 00000 rs1 000 rd 1010011 FMV.X.D
      // 1101001 00010 rs1 rm rd 1010011 FCVT.D.L
      // 1101001 00011 rs1 rm rd 1010011 FCVT.D.LU
      // 1111001 00000 rs1 000 rd 1010011 FMV.D.X
    default:
      // TODO: raise illegal instruction exception
      std::stringstream ss;
      ss << "Error: illegal instruction";
      throw std::runtime_error(ss.str());
  }
}
void machine_t::decode_and_execute_instruction(uint32_t _instruction) {
  instruction_t instruction;
  reinterpret_cast<uint32_t&>(instruction) = _instruction;
  _registers[0]                            = 0;
  switch (instruction.opcode()) {
    // imm[31:12] rd 0110111 LUI
    case 0b0110111:  // LUI
      _registers[instruction.as.u_type.rd()] =
          ::dawn::sext(instruction.as.u_type.imm() << 12, 32);
      _program_counter += 4;
      break;
    // imm[31:12] rd 0010111 AUIPC
    case 0b0010111:  // AUIPC
      _registers[instruction.as.u_type.rd()] =
          _program_counter +
          ::dawn::sext(instruction.as.u_type.imm() << 12, 32);
      _program_counter += 4;
      break;
    // imm[20|10:1|11|19:12] rd 1101111 JAL
    case 0b1101111: {  // JAL
      uint64_t address = _program_counter + instruction.as.j_type.imm_sext();
      if (address % 4 != 0) {
        handle_trap(exception_code_t::e_instruction_address_misaligned,
                    address);
        break;
      }
      _registers[instruction.as.j_type.rd()] = _program_counter + 4;
      _program_counter                       = address;
    } break;
    // imm[11:0] rs1 000 rd 1100111 JALR
    case 0b1100111: {  // JALR
      uint64_t address = _program_counter + 4;
      if (address % 4 != 0) {
        handle_trap(exception_code_t::e_instruction_address_misaligned,
                    address);
        break;
      }
      _program_counter = (_registers[instruction.as.i_type.rs1()] +
                          instruction.as.i_type.imm_sext()) &
                         (~1u);
      _registers[instruction.as.i_type.rd()] = address;
    } break;

    case 0b1100011:
      switch (instruction.as.b_type.funct3()) {
        // imm[12|10:5] rs2 rs1 000 imm[4:1|11] 1100011 BEQ
        case 0b000:  // BEQ
          if (_registers[instruction.as.b_type.rs1()] ==
              _registers[instruction.as.b_type.rs2()]) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        // imm[12|10:5] rs2 rs1 001 imm[4:1|11] 1100011 BNE
        case 0b001:  // BNE
          if (_registers[instruction.as.b_type.rs1()] !=
              _registers[instruction.as.b_type.rs2()]) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        // imm[12|10:5] rs2 rs1 100 imm[4:1|11] 1100011 BLT
        case 0b100:  // BLT
          if (static_cast<int64_t>(_registers[instruction.as.b_type.rs1()]) <
              static_cast<int64_t>(_registers[instruction.as.b_type.rs2()])) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        // imm[12|10:5] rs2 rs1 101 imm[4:1|11] 1100011 BGE
        case 0b101:  // BGE
          if (static_cast<int64_t>(_registers[instruction.as.b_type.rs1()]) >=
              static_cast<int64_t>(_registers[instruction.as.b_type.rs2()])) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        // imm[12|10:5] rs2 rs1 110 imm[4:1|11] 1100011 BLTU
        case 0b110:  // BLTU
          if (_registers[instruction.as.b_type.rs1()] <
              _registers[instruction.as.b_type.rs2()]) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        // imm[12|10:5] rs2 rs1 111 imm[4:1|11] 1100011 BGEU
        case 0b111:  // BGEU
          if (_registers[instruction.as.b_type.rs1()] >=
              _registers[instruction.as.b_type.rs2()]) {
            uint64_t address =
                _program_counter + instruction.as.b_type.imm_sext();
            if (address % 4 != 0) {
              handle_trap(exception_code_t::e_instruction_address_misaligned,
                          address);
              break;
            }
            _program_counter = address;
          } else
            _program_counter += 4;
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0000011:
      // TODO: handle invalid address
      switch (instruction.as.i_type.funct3()) {
        // imm[11:0] rs1 000 rd 0000011 LB
        case 0b000: {  // LB
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<8>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = static_cast<int8_t>(*value);
          _program_counter += 4;
        } break;
        // imm[11:0] rs1 001 rd 0000011 LH
        case 0b001: {  // LH
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<16>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = static_cast<int16_t>(*value);
          _program_counter += 4;
        } break;
        // imm[11:0] rs1 010 rd 0000011 LW
        case 0b010: {  // LW
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<32>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = static_cast<int32_t>(*value);
          _program_counter += 4;
        } break;
        // imm[11:0] rs1 100 rd 0000011 LBU
        case 0b100: {  // LBU
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<8>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = *value;
          _program_counter += 4;
        } break;
        // imm[11:0] rs1 101 rd 0000011 LHU
        case 0b101: {  // LHU
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<16>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = *value;
          _program_counter += 4;
        } break;
          // imm[11:0] rs1 110 rd 0000011 LWU
        case 0b110: {  // LWU
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<32>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = *value;
          _program_counter += 4;
        } break;
          // imm[11:0] rs1 011 rd 0000011 LD
        case 0b011: {  // LD
          uint64_t address = _registers[instruction.as.i_type.rs1()] +
                             instruction.as.i_type.imm_sext();
          // TODO: implement address misaligned trap
          std::optional<uint64_t> value = _memory.load<64>(address);
          if (!value) {
            handle_trap(exception_code_t::e_load_access_fault, address);
            break;
          }
          _registers[instruction.as.i_type.rd()] = *value;
          _program_counter += 4;
        } break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0100011:
      // TODO: handle invalid address
      switch (instruction.as.s_type.funct3()) {
        // imm[11:5] rs2 rs1 000 imm[4:0] 0100011 SB
        case 0b000: {  // SB
          uint64_t address = _registers[instruction.as.s_type.rs1()] +
                             instruction.as.s_type.imm_sext();
          if (!_memory.store<8>(_registers[instruction.as.s_type.rs1()] +
                                    instruction.as.s_type.imm_sext(),
                                _registers[instruction.as.s_type.rs2()])) {
            handle_trap(exception_code_t::e_store_access_fault, address);
            break;
          }
          _program_counter += 4;
        } break;
        // imm[11:5] rs2 rs1 001 imm[4:0] 0100011 SH
        case 0b001: {  // SH
          uint64_t address = _registers[instruction.as.s_type.rs1()] +
                             instruction.as.s_type.imm_sext();
          if (!_memory.store<16>(_registers[instruction.as.s_type.rs1()] +
                                     instruction.as.s_type.imm_sext(),
                                 _registers[instruction.as.s_type.rs2()])) {
            handle_trap(exception_code_t::e_store_access_fault, address);
            break;
          }
          _program_counter += 4;
        } break;
        // imm[11:5] rs2 rs1 010 imm[4:0] 0100011 SW
        case 0b010: {  // SW
          uint64_t address = _registers[instruction.as.s_type.rs1()] +
                             instruction.as.s_type.imm_sext();
          if (!_memory.store<32>(_registers[instruction.as.s_type.rs1()] +
                                     instruction.as.s_type.imm_sext(),
                                 _registers[instruction.as.s_type.rs2()])) {
            handle_trap(exception_code_t::e_store_access_fault, address);
            break;
          }
          _program_counter += 4;
        } break;
        // imm[11:5] rs2 rs1 011 imm[4:0] 0100011 SD
        case 0b011: {  // SD
          uint64_t address = _registers[instruction.as.s_type.rs1()] +
                             instruction.as.s_type.imm_sext();
          if (!_memory.store<64>(_registers[instruction.as.s_type.rs1()] +
                                     instruction.as.s_type.imm_sext(),
                                 _registers[instruction.as.s_type.rs2()])) {
            handle_trap(exception_code_t::e_store_access_fault, address);
            break;
          }
          _program_counter += 4;
        } break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0010011:
      switch (instruction.as.i_type.funct3()) {
        // imm[11:0] rs1 000 rd 0010011 ADDI
        case 0b000:  // ADDI
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()] +
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // imm[11:0] rs1 010 rd 0010011 SLTI
        case 0b010:  // SLTI
          _registers[instruction.as.i_type.rd()] =
              static_cast<int64_t>(_registers[instruction.as.i_type.rs1()]) <
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // imm[11:0] rs1 011 rd 0010011 SLTIU
        case 0b011:  // SLTIU
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()] <
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // imm[11:0] rs1 100 rd 0010011 XORI
        case 0b100:  // XORI
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()] ^
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // imm[11:0] rs1 110 rd 0010011 ORI
        case 0b110:  // ORI
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()] |
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // imm[11:0] rs1 111 rd 0010011 ANDI
        case 0b111:  // ANDI
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()] &
              instruction.as.i_type.imm_sext();
          _program_counter += 4;
          break;
        // 0000000 shamt rs1 001 rd 0010011 SLLI
        case 0b001:  // SLLI
          _registers[instruction.as.i_type.rd()] =
              _registers[instruction.as.i_type.rs1()]
              << instruction.as.i_type.shamt();
          _program_counter += 4;
          break;

        case 0b101:
          switch (instruction.as.i_type.imm() >> 6) {
            // 000000 shamt rs1 101 rd 0010011 SRLI
            case 0b000000:  // SRLI
              _registers[instruction.as.i_type.rd()] =
                  _registers[instruction.as.i_type.rs1()] >>
                  instruction.as.i_type.shamt();
              _program_counter += 4;
              break;
            // 010000 shamt rs1 101 rd 0010011 SRAI
            case 0b010000:  // SRAI
              _registers[instruction.as.i_type.rd()] =
                  static_cast<int64_t>(
                      _registers[instruction.as.i_type.rs1()]) >>
                  instruction.as.i_type.shamt();
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0110011:
      switch (instruction.as.r_type.funct3()) {
        case 0b000:
          switch (instruction.as.r_type.funct7()) {
            // 0000000 rs2 rs1 000 rd 0110011 ADD
            case 0b0000000:  // ADD
              _registers[instruction.as.r_type.rd()] =
                  _registers[instruction.as.r_type.rs1()] +
                  _registers[instruction.as.r_type.rs2()];
              _program_counter += 4;
              break;
            // 0100000 rs2 rs1 000 rd 0110011 SUB
            case 0b0100000:  // SUB
              _registers[instruction.as.r_type.rd()] =
                  _registers[instruction.as.r_type.rs1()] -
                  _registers[instruction.as.r_type.rs2()];
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;

        // 0000000 rs2 rs1 001 rd 0110011 SLL
        case 0b001:  // SLL
          _registers[instruction.as.r_type.rd()] =
              _registers[instruction.as.r_type.rs1()]
              << (_registers[instruction.as.r_type.rs2()] & 0b111111);
          _program_counter += 4;
          break;
        // 0000000 rs2 rs1 010 rd 0110011 SLT
        case 0b010:  // SLT
          _registers[instruction.as.r_type.rd()] =
              static_cast<int64_t>(_registers[instruction.as.r_type.rs1()]) <
              static_cast<int64_t>(_registers[instruction.as.r_type.rs2()]);
          _program_counter += 4;
          break;
        // 0000000 rs2 rs1 011 rd 0110011 SLTU
        case 0b011:  // SLTU
          _registers[instruction.as.r_type.rd()] =
              _registers[instruction.as.r_type.rs1()] <
              _registers[instruction.as.r_type.rs2()];
          _program_counter += 4;
          break;
        // 0000000 rs2 rs1 100 rd 0110011 XOR
        case 0b100:  // XOR
          _registers[instruction.as.r_type.rd()] =
              _registers[instruction.as.r_type.rs1()] ^
              _registers[instruction.as.r_type.rs2()];
          _program_counter += 4;
          break;

        case 0b101:
          switch (instruction.as.r_type.funct7()) {
            // 0000000 rs2 rs1 101 rd 0110011 SRL
            case 0b0000000:  // SRL
              _registers[instruction.as.r_type.rd()] =
                  _registers[instruction.as.r_type.rs1()] >>
                  (_registers[instruction.as.r_type.rs2()] & 0b111111);
              _program_counter += 4;
              break;
            // 0100000 rs2 rs1 101 rd 0110011 SRA
            case 0b0100000:  // SRA
              _registers[instruction.as.r_type.rd()] =
                  static_cast<int64_t>(
                      _registers[instruction.as.r_type.rs1()]) >>
                  (_registers[instruction.as.r_type.rs2()] & 0b111111);
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;

        // 0000000 rs2 rs1 110 rd 0110011 OR
        case 0b110:  // OR
          _registers[instruction.as.r_type.rd()] =
              _registers[instruction.as.r_type.rs1()] |
              _registers[instruction.as.r_type.rs2()];
          _program_counter += 4;
          break;
        // 0000000 rs2 rs1 111 rd 0110011 AND
        case 0b111:  // AND
          _registers[instruction.as.r_type.rd()] =
              _registers[instruction.as.r_type.rs1()] &
              _registers[instruction.as.r_type.rs2()];
          _program_counter += 4;
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    // fm pred succ rs1 000 rd 0001111 FENCE
    case 0b0001111:  // FENCE
      std::cout << "Note: Fence encountered, fence is not implemented!\n";
      _program_counter += 4;
      break;
      // // 1000 0011 0011 00000 000 00000 0001111 FENCE.TSO
      // case 0b0001111:  // FENCE.TSO
      //   break;
      // // 0000 0001 0000 00000 000 00000 0001111 PAUSE
      // case 0b0001111:  // PAUSE
      //   break;

    case 0b1110011:
      switch (instruction.as.i_type.funct3()) {
        case 0b000:
          switch (instruction.as.i_type.imm()) {
            // 000000000000 00000 000 00000 1110011 ECALL
            case 0b000000000000: {  // ECALL
              auto itr = _syscalls.find(_registers[17]);
              if (itr != _syscalls.end()) {
                itr->second(*this);
                _program_counter += 4;
              } else {
                std::stringstream ss;
                ss << "Error: unhandled ecall: " << _registers[17];
                throw std::runtime_error(ss.str());
              }
            } break;
            // 000000000001 00000 000 00000 1110011 EBREAK
            case 0b000000000001:  // EBREAK
              break;
            // 001100000010 00000 000 00000 1110011 EBREAK
            case 0b001100000010: {  // MRET
              uint64_t mstatus = _read_csr(MSTATUS);
              uint64_t mpp = (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
              uint64_t mpie =
                  (mstatus & MSTATUS_MPIE_MASK) >> MSTATUS_MPIE_SHIFT;
              _program_counter = _read_csr(MEPC);
              _privilege_mode  = static_cast<privilege_mode_t>(mpp);
              mstatus =
                  (mstatus & ~MSTATUS_MIE_MASK) | (mpie << MSTATUS_MIE_SHIFT);
              mstatus =
                  (mstatus & ~MSTATUS_MPIE_MASK) | (1U << MSTATUS_MPIE_SHIFT);
              mstatus =
                  (mstatus & ~MSTATUS_MPP_MASK) | (0b11U << MSTATUS_MPP_SHIFT);
              _write_csr(MSTATUS, mstatus);
            } break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;
          // TODO: CSR instructions should use read and write
          // csr rs1 001 rd 1110011 CSRRW
        case 0b001: {  // CSRRW
          uint64_t t = read_csr(_instruction);
          write_csr(_instruction, _registers[instruction.as.i_type.rs1()]);
          _registers[instruction.as.i_type.rd()] = t;
          _program_counter += 4;
        } break;
          // csr rs1 010 rd 1110011 CSRRS
        case 0b010: {  // CSRRS
          uint64_t t = read_csr(_instruction);
          write_csr(_instruction, t | _registers[instruction.as.i_type.rs1()]);
          _registers[instruction.as.i_type.rd()] = t;
          _program_counter += 4;
        } break;
          // csr rs1 011 rd 1110011 CSRRC
        case 0b011:  // CSRRC
          break;
          // csr uimm 101 rd 1110011 CSRRWI
        case 0b101: {  // CSRRWI
          uint64_t t = read_csr(_instruction);
          write_csr(_instruction, instruction.as.i_type.rs1());
          _registers[instruction.as.i_type.rd()] = t;
          _program_counter += 4;
        } break;
          // csr uimm 110 rd 1110011 CSRRSI
        case 0b110:  // CSRRSI
          break;
          // csr uimm 111 rd 1110011 CSRRCI
        case 0b111:  // CSRRCI
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0011011:
      switch (instruction.as.i_type.funct3()) {
          // imm[11:0] rs1 000 rd 0011011 ADDIW
        case 0b000:  // ADDIW
          _registers[instruction.as.i_type.rd()] = ::dawn::sext(
              static_cast<uint32_t>(_registers[instruction.as.i_type.rs1()]) +
                  static_cast<int32_t>(instruction.as.i_type.imm_sext()),
              32);
          _program_counter += 4;
          break;
          // 0000000 shamt rs1 001 rd 0011011 SLLIW
        case 0b001:  // SLLIW
          _registers[instruction.as.i_type.rd()] = ::dawn::sext(
              static_cast<uint32_t>(_registers[instruction.as.i_type.rs1()])
                  << static_cast<uint32_t>(instruction.as.i_type.shamt_w()),
              32);
          _program_counter += 4;
          break;
        case 0b101:
          switch (instruction.as.i_type.imm() >> 5) {
              // 0000000 shamt rs1 101 rd 0011011 SRLIW
            case 0b0000000:  // SRLIW
              _registers[instruction.as.i_type.rd()] =
                  ::dawn::sext(static_cast<uint32_t>(
                                   _registers[instruction.as.i_type.rs1()]) >>
                                   instruction.as.i_type.shamt_w(),
                               32);
              _program_counter += 4;
              break;
              // 0100000 shamt rs1 101 rd 0011011 SRAIW
            case 0b0100000:  // SRAIW
              _registers[instruction.as.i_type.rd()] =
                  ::dawn::sext(static_cast<int32_t>(
                                   _registers[instruction.as.i_type.rs1()]) >>
                                   instruction.as.i_type.shamt_w(),
                               32);
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

    case 0b0111011:
      switch (instruction.as.r_type.funct3()) {
        case 0b000:
          switch (instruction.as.r_type.funct7()) {
              // 0000000 rs2 rs1 000 rd 0111011 ADDW
            case 0b0000000:  // ADDW
              _registers[instruction.as.r_type.rd()] =
                  ::dawn::sext(static_cast<uint32_t>(
                                   _registers[instruction.as.r_type.rs1()]) +
                                   static_cast<uint32_t>(
                                       _registers[instruction.as.r_type.rs2()]),
                               32);
              _program_counter += 4;
              break;
              // 0100000 rs2 rs1 000 rd 0111011 SUBW
            case 0b0100000:  // SUBW
              _registers[instruction.as.r_type.rd()] =
                  ::dawn::sext(static_cast<uint32_t>(
                                   _registers[instruction.as.r_type.rs1()]) -
                                   static_cast<uint32_t>(
                                       _registers[instruction.as.r_type.rs2()]),
                               32);
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;

          // 0000000 rs2 rs1 001 rd 0111011 SLLW
        case 0b001:  // SLLW
          _registers[instruction.as.r_type.rd()] =
              static_cast<int32_t>(_registers[instruction.as.r_type.rs1()])
              << (_registers[instruction.as.r_type.rs2()] & 0b11111);
          _program_counter += 4;
          break;

        case 0b101:
          switch (instruction.as.r_type.funct7()) {
              // 0000000 rs2 rs1 101 rd 0111011 SRLW
            case 0b0000000:  // SRLW
              _registers[instruction.as.r_type.rd()] = ::dawn::sext(
                  static_cast<uint32_t>(
                      _registers[instruction.as.r_type.rs1()]) >>
                      (_registers[instruction.as.r_type.rs2()] & 0b11111),
                  32);
              _program_counter += 4;
              break;
              // 0100000 rs2 rs1 101 rd 0111011 SRAW
            case 0b0100000:  // SRAW
              _registers[instruction.as.r_type.rd()] =
                  static_cast<int64_t>(static_cast<int32_t>(
                      _registers[instruction.as.r_type.rs1()])) >>
                  (_registers[instruction.as.r_type.rs2()] & 0b11111);
              _program_counter += 4;
              break;
            default:
              handle_trap(exception_code_t::e_illegal_instruction,
                          _instruction);
          }
          break;
        default:
          handle_trap(exception_code_t::e_illegal_instruction, _instruction);
      }
      break;

      // RV32/RV64 Zifencei Standard Extension
      // imm[11:0] rs1 001 rd 0001111 FENCE.I

      // RV32M Standard Extension
      // 0000001 rs2 rs1 000 rd 0110011 MUL
      // 0000001 rs2 rs1 001 rd 0110011 MULH
      // 0000001 rs2 rs1 010 rd 0110011 MULHSU
      // 0000001 rs2 rs1 011 rd 0110011 MULHU
      // 0000001 rs2 rs1 100 rd 0110011 DIV
      // 0000001 rs2 rs1 101 rd 0110011 DIVU
      // 0000001 rs2 rs1 110 rd 0110011 REM
      // 0000001 rs2 rs1 111 rd 0110011 REMU

      // RV64M Standard Extension (in addition to RV32M)
      // 0000001 rs2 rs1 000 rd 0111011 MULW
      // 0000001 rs2 rs1 100 rd 0111011 DIVW
      // 0000001 rs2 rs1 101 rd 0111011 DIVUW
      // 0000001 rs2 rs1 110 rd 0111011 REMW
      // 0000001 rs2 rs1 111 rd 0111011 REMUW

      // RV32F Standard Extension
      // imm[11:0] rs1 010 rd 0000111 FLW
      // imm[11:5] rs2 rs1 010 imm[4:0] 0100111 FSW
      // rs3 00 rs2 rs1 rm rd 1000011 FMADD.S
      // rs3 00 rs2 rs1 rm rd 1000111 FMSUB.S
      // rs3 00 rs2 rs1 rm rd 1001011 FNMSUB.S
      // rs3 00 rs2 rs1 rm rd 1001111 FNMADD.S
      // 0000000 rs2 rs1 rm rd 1010011 FADD.S
      // 0000100 rs2 rs1 rm rd 1010011 FSUB.S
      // 0001000 rs2 rs1 rm rd 1010011 FMUL.S
      // 0001100 rs2 rs1 rm rd 1010011 FDIV.S
      // 0101100 00000 rs1 rm rd 1010011 FSQRT.S
      // 0010000 rs2 rs1 000 rd 1010011 FSGNJ.S
      // 0010000 rs2 rs1 001 rd 1010011 FSGNJN.S
      // 0010000 rs2 rs1 010 rd 1010011 FSGNJX.S
      // 0010100 rs2 rs1 000 rd 1010011 FMIN.S
      // 0010100 rs2 rs1 001 rd 1010011 FMAX.S
      // 1100000 00000 rs1 rm rd 1010011 FCVT.W.S
      // 1100000 00001 rs1 rm rd 1010011 FCVT.WU.S
      // 1110000 00000 rs1 000 rd 1010011 FMV.X.W
      // 1010000 rs2 rs1 010 rd 1010011 FEQ.S
      // 1010000 rs2 rs1 001 rd 1010011 FLT.S
      // 1010000 rs2 rs1 000 rd 1010011 FLE.S
      // 1110000 00000 rs1 001 rd 1010011 FCLASS.S
      // 1101000 00000 rs1 rm rd 1010011 FCVT.S.W
      // 1101000 00001 rs1 rm rd 1010011 FCVT.S.WU
      // 1111000 00000 rs1 000 rd 1010011 FMV.W.X
      //
      // RV64F Standard Extension (in addition to RV32F)
      // 1100000 00010 rs1 rm rd 1010011 FCVT.L.S
      // 1100000 00011 rs1 rm rd 1010011 FCVT.LU.S
      // 1101000 00010 rs1 rm rd 1010011 FCVT.S.L
      // 1101000 00011 rs1 rm rd 1010011 FCVT.S.LU

      // RV32D Standard Extension
      // imm[11:0] rs1 011 rd 0000111 FLD
      // imm[11:5] rs2 rs1 011 imm[4:0] 0100111 FSD
      // rs3 01 rs2 rs1 rm rd 1000011 FMADD.D
      // rs3 01 rs2 rs1 rm rd 1000111 FMSUB.D
      // rs3 01 rs2 rs1 rm rd 1001011 FNMSUB.D
      // rs3 01 rs2 rs1 rm rd 1001111 FNMADD.D
      // 0000001 rs2 rs1 rm rd 1010011 FADD.D
      // 0000101 rs2 rs1 rm rd 1010011 FSUB.D
      // 0001001 rs2 rs1 rm rd 1010011 FMUL.D
      // 0001101 rs2 rs1 rm rd 1010011 FDIV.D
      // 0101101 00000 rs1 rm rd 1010011 FSQRT.D
      // 0010001 rs2 rs1 000 rd 1010011 FSGNJ.D
      // 0010001 rs2 rs1 001 rd 1010011 FSGNJN.D
      // 0010001 rs2 rs1 010 rd 1010011 FSGNJX.D
      // 0010101 rs2 rs1 000 rd 1010011 FMIN.D
      // 0010101 rs2 rs1 001 rd 1010011 FMAX.D
      // 0100000 00001 rs1 rm rd 1010011 FCVT.S.D
      // 0100001 00000 rs1 rm rd 1010011 FCVT.D.S
      // 1010001 rs2 rs1 010 rd 1010011 FEQ.D
      // 1010001 rs2 rs1 001 rd 1010011 FLT.D
      // 1010001 rs2 rs1 000 rd 1010011 FLE.D
      // 1110001 00000 rs1 001 rd 1010011 FCLASS.D
      // 1100001 00000 rs1 rm rd 1010011 FCVT.W.D
      // 1100001 00001 rs1 rm rd 1010011 FCVT.WU.D
      // 1101001 00000 rs1 rm rd 1010011 FCVT.D.W
      // 1101001 00001 rs1 rm rd 1010011 FCVT.D.WU

      // RV64D Standard Extension (in addition to RV32D)
      // 1100001 00010 rs1 rm rd 1010011 FCVT.L.D
      // 1100001 00011 rs1 rm rd 1010011 FCVT.LU.D
      // 1110001 00000 rs1 000 rd 1010011 FMV.X.D
      // 1101001 00010 rs1 rm rd 1010011 FCVT.D.L
      // 1101001 00011 rs1 rm rd 1010011 FCVT.D.LU
      // 1111001 00000 rs1 000 rd 1010011 FMV.D.X
    default:
      handle_trap(exception_code_t::e_illegal_instruction, _instruction);
  }
}

}  // namespace dawn
