#include "dawn/types.hpp"

std::ostream& operator<<(std::ostream&                     o,
                         const dawn::riscv::instruction_t& inst) {
  o << std::hex << reinterpret_cast<const uint32_t&>(inst) << "   ";
  o.flush();
  switch (inst.as.base.opcode()) {
    case dawn::riscv::op_t::e_lui:
      o << "lui x" << inst.as.u_type.rd() << ", " << std::hex
        << inst.as.u_type.imm();
      break;
    case dawn::riscv::op_t::e_auipc:
      o << "auipc x" << inst.as.u_type.rd() << ", " << std::hex
        << inst.as.u_type.imm();
      break;
    case dawn::riscv::op_t::e_jal:
      o << "jal x" << inst.as.j_type.rd() << ", " << std::hex
        << inst.as.j_type.imm_sext();
      break;
    case dawn::riscv::op_t::e_jalr:
      o << "jalr x" << inst.as.i_type.rd() << ", x" << inst.as.i_type.rs1()
        << ", " << std::hex << inst.as.i_type.imm_sext();
      break;
    case dawn::riscv::op_t::e_branch: {
      switch (inst.as.b_type.funct3()) {
        case dawn::riscv::branch_t::e_beq:
          o << "beq x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;
        case dawn::riscv::branch_t::e_bne:
          o << "bne x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;
        case dawn::riscv::branch_t::e_blt:
          o << "blt x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;
        case dawn::riscv::branch_t::e_bge:
          o << "bge x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;
        case dawn::riscv::branch_t::e_bltu:
          o << "bltu x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;
        case dawn::riscv::branch_t::e_bgeu:
          o << "bgeu x" << std::dec << inst.as.b_type.rs1() << ", x"
            << inst.as.b_type.rs2() << ", " << std::hex
            << inst.as.b_type.imm_sext();
          break;

        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_load: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_lb:
          o << "lb x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_lh:
          o << "lh x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_lw:
          o << "lw x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_lbu:
          o << "lbu x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_lhu:
          o << "lhu x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_lwu:
          o << "lwu x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        case dawn::riscv::i_type_func3_t::e_ld:
          o << "ld x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm_sext() << "(x" << std::dec
            << inst.as.i_type.rs1() << ")";
          break;
        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_store: {
      switch (inst.as.s_type.funct3()) {
        case dawn::riscv::store_t::e_sb:
          o << "sb x" << std::dec << inst.as.s_type.rs2() << ", " << std::hex
            << inst.as.s_type.imm_sext() << "(x" << std::dec
            << inst.as.s_type.rs1() << ")";
          break;
        case dawn::riscv::store_t::e_sh:
          o << "sh x" << std::dec << inst.as.s_type.rs2() << ", " << std::hex
            << inst.as.s_type.imm_sext() << "(x" << std::dec
            << inst.as.s_type.rs1() << ")";
          break;
        case dawn::riscv::store_t::e_sw:
          o << "sw x" << std::dec << inst.as.s_type.rs2() << ", " << std::hex
            << inst.as.s_type.imm_sext() << "(x" << std::dec
            << inst.as.s_type.rs1() << ")";
          break;
        case dawn::riscv::store_t::e_sd:
          o << "sd x" << std::dec << inst.as.s_type.rs2() << ", " << std::hex
            << inst.as.s_type.imm_sext() << "(x" << std::dec
            << inst.as.s_type.rs1() << ")";
          break;

        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_i_type: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_addi:
          o << "addi x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_slti:
          o << "slti x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_sltiu:
          o << "sltiu x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_xori:
          o << "xori x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_ori:
          o << "ori x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_andi:
          o << "andi x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_slli:
          o << "slli x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex << inst.as.i_type.imm();
          break;
        case dawn::riscv::i_type_func3_t::e_srli_or_srai: {
          switch (static_cast<dawn::riscv::srli_or_srai_t>(
              inst.as.i_type.imm() >> 6)) {
            case dawn::riscv::srli_or_srai_t::e_srli:
              o << "srli x" << std::dec << inst.as.i_type.rd() << ", x"
                << inst.as.i_type.rs1() << ", " << std::hex
                << (inst.as.i_type.imm() & 0x3F);
              break;
            case dawn::riscv::srli_or_srai_t::e_srai:
              o << "srai x" << std::dec << inst.as.i_type.rd() << ", x"
                << inst.as.i_type.rs1() << ", " << std::hex
                << (inst.as.i_type.imm() & 0x3F);
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;

        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_i_type_32: {
      switch (inst.as.i_type.funct3()) {
        case dawn::riscv::i_type_func3_t::e_addiw:
          o << "addiw x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << inst.as.i_type.imm_sext();
          break;
        case dawn::riscv::i_type_func3_t::e_slliw:
          o << "slliw x" << std::dec << inst.as.i_type.rd() << ", x"
            << inst.as.i_type.rs1() << ", " << std::hex
            << (inst.as.i_type.imm() & 0x1F);
          break;
        case dawn::riscv::i_type_func3_t::e_srliw_or_sraiw: {
          switch (static_cast<dawn::riscv::srliw_or_sraiw_t>(
              inst.as.i_type.imm() >> 5)) {
            case dawn::riscv::srliw_or_sraiw_t::e_srliw:
              o << "srliw x" << std::dec << inst.as.i_type.rd() << ", x"
                << inst.as.i_type.rs1() << ", " << std::hex
                << (inst.as.i_type.imm() & 0x1F);
              break;
            case dawn::riscv::srliw_or_sraiw_t::e_sraiw:
              o << "sraiw x" << std::dec << inst.as.i_type.rd() << ", x"
                << inst.as.i_type.rs1() << ", " << std::hex
                << (inst.as.i_type.imm() & 0x1F);
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;

        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_r_type: {
      switch (inst.as.r_type.funct7()) {
        case dawn::riscv::r_type_func7_t::e_0000000: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_add:
              o << "add x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_sll:
              o << "sll x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_slt:
              o << "slt x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_sltu:
              o << "sltu x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_xor:
              o << "xor x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_srl:
              o << "srl x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_or:
              o << "or x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_and:
              o << "and x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::r_type_func7_t::e_0100000: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_sub:
              o << "sub x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_sra:
              o << "sra x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::r_type_func7_t::e_0000001: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_mul:
              o << "mul x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_mulh:
              o << "mulh x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_mulhsu:
              o << "mulhsu x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_mulhu:
              o << "mulhu x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_div:
              o << "div x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_divu:
              o << "divu x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_rem:
              o << "rem x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_remu:
              o << "remu x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_r_type_32: {
      switch (inst.as.r_type.funct7()) {
        case dawn::riscv::r_type_func7_t::e_0000000: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_addw:
              o << "addw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_sllw:
              o << "sllw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_srlw:
              o << "srlw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::r_type_func7_t::e_0100000: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_subw:
              o << "subw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_sraw:
              o << "sraw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::r_type_func7_t::e_0000001: {
          switch (inst.as.r_type.funct3()) {
            case dawn::riscv::r_type_func3_t::e_mulw:
              o << "mulw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_divw:
              o << "divw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_divuw:
              o << "divuw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_remw:
              o << "remw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;
            case dawn::riscv::r_type_func3_t::e_remuw:
              o << "remuw x" << std::dec << inst.as.r_type.rd() << ", x"
                << inst.as.r_type.rs1() << ", x" << inst.as.r_type.rs2();
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_fence:
      o << "fence " << std::hex << (inst.as.i_type.imm() >> 8) << ", "
        << std::hex << (inst.as.i_type.imm() & 0xFF);
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
            case dawn::riscv::sub_system_t::e_wfi:
              o << "wfi";
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::i_type_func3_t::e_csrrw:
          o << "csrrw x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", x" << std::dec
            << inst.as.i_type.rs1();
          break;
        case dawn::riscv::i_type_func3_t::e_csrrs:
          o << "csrrs x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", x" << std::dec
            << inst.as.i_type.rs1();
          break;
        case dawn::riscv::i_type_func3_t::e_csrrc:
          o << "csrrc x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", x" << std::dec
            << inst.as.i_type.rs1();
          break;
        case dawn::riscv::i_type_func3_t::e_csrrwi:
          o << "csrrwi x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", " << std::dec << inst.as.i_type.rs1();
          break;
        case dawn::riscv::i_type_func3_t::e_csrrsi:
          o << "csrrsi x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", " << std::dec << inst.as.i_type.rs1();
          break;
        case dawn::riscv::i_type_func3_t::e_csrrci:
          o << "csrrci x" << std::dec << inst.as.i_type.rd() << ", " << std::hex
            << inst.as.i_type.imm() << ", " << std::dec << inst.as.i_type.rs1();
          break;

        default:
          throw std::runtime_error("unknown instruction\n");
      }
    } break;
    case dawn::riscv::op_t::e_a_type: {  // 0101111
      switch (inst.as.a_type.funct3()) {
        case dawn::riscv::a_type_func3_t::e_w: {  // 010
          switch (inst.as.a_type.funct5()) {
            case dawn::riscv::a_type_func5_t::e_lr:
              o << "lr.w x" << std::dec << inst.as.a_type.rd() << ", (x"
                << std::dec << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_sc:
              o << "sc.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoswap:
              o << "amoswap.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoadd:
              o << "amoadd.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoxor:
              o << "amoxor.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoand:
              o << "amoand.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoor:
              o << "amoor.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomin:
              o << "amomin.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomax:
              o << "amomax.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amominu:
              o << "amominu.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomaxu:
              o << "amomaxu.w x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
        case dawn::riscv::a_type_func3_t::e_d: {  // 011
          switch (inst.as.a_type.funct5()) {
            case dawn::riscv::a_type_func5_t::e_lr:
              o << "lr.d x" << std::dec << inst.as.a_type.rd() << ", (x"
                << std::dec << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_sc:
              o << "sc.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoswap:
              o << "amoswap.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoadd:
              o << "amoadd.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoxor:
              o << "amoxor.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoand:
              o << "amoand.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amoor:
              o << "amoor.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomin:
              o << "amomin.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomax:
              o << "amomax.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amominu:
              o << "amominu.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;
            case dawn::riscv::a_type_func5_t::e_amomaxu:
              o << "amomaxu.d x" << std::dec << inst.as.a_type.rd() << ", x"
                << std::dec << inst.as.a_type.rs2() << ", (x" << std::dec
                << inst.as.a_type.rs1() << ")";
              break;

            default:
              throw std::runtime_error("unknown instruction\n");
          }
        } break;
      }
      break;

      default:
        throw std::runtime_error("unknown instruction\n");
    }
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
    case dawn::riscv::exception_code_t::e_machine_timer_interrupt:
      o << "machine_timer_interrupt";
      break;
    default:
      dawn::error("Error: unknown exception");
  }
  return o;
}
