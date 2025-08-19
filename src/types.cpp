#include "dawn/types.hpp"

std::ostream& operator<<(std::ostream&                     o,
                         const dawn::riscv::instruction_t& inst) {
  switch (inst.as.base.opcode()) {
    case dawn::riscv::op_t::e_lui:
      o << "lui";
      break;
    case dawn::riscv::op_t::e_auipc:
      o << "auipc";
      break;
    case dawn::riscv::op_t::e_jal:
      o << "jal";
      break;
    case dawn::riscv::op_t::e_jalr:
      o << "jalr";
      break;
    case dawn::riscv::op_t::e_branch: {
      switch (inst.as.b_type.funct3()) {
        case dawn::riscv::branch_t::e_beq:
          o << "beq";
          break;
        case dawn::riscv::branch_t::e_bne:
          o << "bne";
          break;
        case dawn::riscv::branch_t::e_blt:
          o << "blt";
          break;
        case dawn::riscv::branch_t::e_bge:
          o << "bge";
          break;
        case dawn::riscv::branch_t::e_bltu:
          o << "bltu";
          break;
        case dawn::riscv::branch_t::e_bgeu:
          o << "bgeu";
          break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_load: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_lb:
          o << "lb";
          break;
        case dawn::riscv::i_type_func3_t::e_lh:
          o << "lh";
          break;
        case dawn::riscv::i_type_func3_t::e_lw:
          o << "lw";
          break;
        case dawn::riscv::i_type_func3_t::e_lbu:
          o << "lbu";
          break;
        case dawn::riscv::i_type_func3_t::e_lhu:
          o << "lhu";
          break;
        case dawn::riscv::i_type_func3_t::e_lwu:
          o << "lwu";
          break;
        case dawn::riscv::i_type_func3_t::e_ld:
          o << "ld";
          break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_store: {
      switch (inst.as.s_type.funct3()) {
        case dawn::riscv::store_t::e_sb:
          o << "sb";
          break;
        case dawn::riscv::store_t::e_sh:
          o << "sh";
          break;
        case dawn::riscv::store_t::e_sw:
          o << "sw";
          break;
        case dawn::riscv::store_t::e_sd:
          o << "sd";
          break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_i_type: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_addi:
          o << "addi";
          break;
        case dawn::riscv::i_type_func3_t::e_slti:
          o << "slti";
          break;
        case dawn::riscv::i_type_func3_t::e_sltiu:
          o << "sltiu";
          break;
        case dawn::riscv::i_type_func3_t::e_xori:
          o << "xori";
          break;
        case dawn::riscv::i_type_func3_t::e_ori:
          o << "ori";
          break;
        case dawn::riscv::i_type_func3_t::e_andi:
          o << "andi";
          break;
        case dawn::riscv::i_type_func3_t::e_slli:
          o << "slli";
          break;
        case dawn::riscv::i_type_func3_t::e_srli_or_srai: {
          switch (static_cast<dawn::riscv::srli_or_srai_t>(
              inst.as.i_type.imm() >> 6)) {
            case dawn::riscv::srli_or_srai_t::e_srli:
              o << "srli";
              break;
            case dawn::riscv::srli_or_srai_t::e_srai:
              o << "srai";
              break;

            default:
              o << "unknown";
          }
        } break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_i_type_32: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_addiw:
          o << "addiw";
          break;
        case dawn::riscv::i_type_func3_t::e_slliw:
          o << "slliw";
          break;
        case dawn::riscv::i_type_func3_t::e_srliw_or_sraiw: {
          switch (static_cast<dawn::riscv::srliw_or_sraiw_t>(
              inst.as.i_type.imm() >> 5)) {
            case dawn::riscv::srliw_or_sraiw_t::e_srliw:
              o << "srliw";
              break;
            case dawn::riscv::srliw_or_sraiw_t::e_sraiw:
              o << "sraiw";
              break;

            default:
              o << "unknown";
          }
        } break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_r_type: {
      switch (inst.as.r_type.funct3()) {
        case dawn::riscv::r_type_func3_t::e_add_or_sub: {
          switch (
              static_cast<dawn::riscv::add_or_sub_t>(inst.as.r_type.funct7())) {
            case dawn::riscv::add_or_sub_t::e_add:
              o << "add";
              break;
            case dawn::riscv::add_or_sub_t::e_sub:
              o << "sub";
              break;

            default:
              o << "unknown";
          }
        } break;
        case dawn::riscv::r_type_func3_t::e_sll:
          o << "sll";
          break;
        case dawn::riscv::r_type_func3_t::e_slt:
          o << "slt";
          break;
        case dawn::riscv::r_type_func3_t::e_sltu:
          o << "sltu";
          break;
        case dawn::riscv::r_type_func3_t::e_xor:
          o << "xor";
          break;
        case dawn::riscv::r_type_func3_t::e_srl_or_sra: {
          switch (
              static_cast<dawn::riscv::srl_or_sra_t>(inst.as.r_type.funct7())) {
            case dawn::riscv::srl_or_sra_t::e_srl:
              o << "srl";
              break;
            case dawn::riscv::srl_or_sra_t::e_sra:
              o << "sra";
              break;

            default:
              o << "unknown";
          }
        } break;
        case dawn::riscv::r_type_func3_t::e_or:
          o << "or";
          break;
        case dawn::riscv::r_type_func3_t::e_and:
          o << "and";
          break;

        default:
          o << "unknown";
      }
    } break;
    case dawn::riscv::op_t::e_r_type_32: {
      switch (inst.as.r_type.funct3()) {
        case dawn::riscv::r_type_func3_t::e_addw_or_subw: {
          switch (static_cast<dawn::riscv::addw_or_subw_t>(
              inst.as.r_type.funct7())) {
            case dawn::riscv::addw_or_subw_t::e_addw:
              o << "addw";
              break;
            case dawn::riscv::addw_or_subw_t::e_subw:
              o << "subw";
              break;

            default:
              o << "unknown";
          }
          break;
          case dawn::riscv::r_type_func3_t::e_sllw:
            o << "sllw";
            break;
          case dawn::riscv::r_type_func3_t::e_srlw_or_sraw: {
            switch (static_cast<dawn::riscv::srlw_or_sraw_t>(
                inst.as.r_type.funct7())) {
              case dawn::riscv::srlw_or_sraw_t::e_srlw:
                o << "srlw";
                break;
              case dawn::riscv::srlw_or_sraw_t::e_sraw:
                o << "sraw";
                break;

              default:
                o << "unknown";
            }
          } break;

          default:
            o << "unknown";
        }
      }
    } break;
    case dawn::riscv::op_t::e_fence:
      o << "fence";
      break;
    case dawn::riscv::op_t::e_system: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_sub_system: {
          switch (
              static_cast<dawn::riscv::sub_system_t>(inst.as.i_type.imm())) {
            case dawn::riscv::sub_system_t::e_ecall:
              o << "ecall";
              break;
            case dawn::riscv::sub_system_t::e_ebreak:
              o << "ebreak";
              break;
            case dawn::riscv::sub_system_t::e_mret:
              o << "mret";
              break;

            default:
              o << "unknown";
          }
        } break;
        case dawn::riscv::i_type_func3_t::e_csrrw:
          o << "csrrw";
          break;
        case dawn::riscv::i_type_func3_t::e_csrrs:
          o << "csrrs";
          break;
        case dawn::riscv::i_type_func3_t::e_csrrc:
          o << "csrrc";
          break;
        case dawn::riscv::i_type_func3_t::e_csrrwi:
          o << "csrrwi";
          break;
        case dawn::riscv::i_type_func3_t::e_csrrsi:
          o << "csrrsi";
          break;
        case dawn::riscv::i_type_func3_t::e_csrrci:
          o << "csrrci";
          break;

        default:
          o << "unknown";
      }
    } break;
    default:
      o << "unknown";
  }
  return o;
}

std::ostream& operator<<(std::ostream&                       o,
                         const dawn::riscv::exception_code_t exception) {
  switch (exception) {
    case dawn::riscv::exception_code_t::e_instruction_address_misaligned:
      o << "instruction_address_misaligned";
      break;
    case dawn::riscv::exception_code_t::e_instruction_access_fault:
      o << "instruction_access_fault";
      break;
    case dawn::riscv::exception_code_t::e_illegal_instruction:
      o << "illegal_instruction";
      break;
    case dawn::riscv::exception_code_t::e_breakpoint:
      o << "breakpoint";
      break;
    case dawn::riscv::exception_code_t::e_load_address_misaligned:
      o << "load_address_misaligned";
      break;
    case dawn::riscv::exception_code_t::e_load_access_fault:
      o << "load_access_fault";
      break;
    case dawn::riscv::exception_code_t::e_store_address_misaligned:
      o << "store_address_misaligned";
      break;
    case dawn::riscv::exception_code_t::e_store_access_fault:
      o << "store_access_fault";
      break;
    case dawn::riscv::exception_code_t::e_ecall_u_mode:
      o << "ecall_u_mode";
      break;
    case dawn::riscv::exception_code_t::e_ecall_s_mode:
      o << "ecall_s_mode";
      break;
    case dawn::riscv::exception_code_t::e_ecall_m_mode:
      o << "ecall_m_mode";
      break;
    default:
      dawn::error("Error: unknown exception");
  }
  return o;
}
