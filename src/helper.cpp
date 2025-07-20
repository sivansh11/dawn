#include "helper.hpp"

#include <sstream>

#include "types.hpp"

namespace dawn {
using std::stringstream;

void debug_disassemble_instruction(uint32_t _instruction, std::ostream& o) {
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

}  // namespace dawn
