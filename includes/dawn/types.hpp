#ifndef DAWN_TYPES_HPP
#define DAWN_TYPES_HPP

#include <cstdint>
#include <iostream>

#include "helper.hpp"

namespace dawn {

namespace riscv {

enum class op_t : uint32_t {
  e_lui       = 0b0110111,
  e_auipc     = 0b0010111,
  e_jal       = 0b1101111,
  e_jalr      = 0b1100111,
  e_branch    = 0b1100011,
  e_load      = 0b0000011,
  e_store     = 0b0100011,
  e_i_type    = 0b0010011,
  e_i_type_32 = 0b0011011,
  e_r_type    = 0b0110011,
  e_r_type_32 = 0b0111011,
  e_fence     = 0b0001111,
  e_system    = 0b1110011,
  e_a_type    = 0b0101111,  // a type doesnt exists, its just A ext using r type
};

enum class a_type_func3_t : uint32_t {
  e_w = 0b010,
  e_d = 0b011,
};

enum class a_type_func5_t : uint32_t {
  e_lr      = 0b00010,
  e_sc      = 0b00011,
  e_amoswap = 0b00001,
  e_amoadd  = 0b00000,
  e_amoxor  = 0b00100,
  e_amoand  = 0b01100,
  e_amoor   = 0b01000,
  e_amomin  = 0b10000,
  e_amomax  = 0b10100,
  e_amominu = 0b11000,
  e_amomaxu = 0b11100,
};

enum class branch_t : uint32_t {
  e_beq  = 0b000,
  e_bne  = 0b001,
  e_blt  = 0b100,
  e_bge  = 0b101,
  e_bltu = 0b110,
  e_bgeu = 0b111,
};

enum class srli_or_srai_t : uint32_t {
  e_srli = 0b000000,
  e_srai = 0b010000,
};

enum class srliw_or_sraiw_t : uint32_t {
  e_srliw = 0b0000000,
  e_sraiw = 0b0100000,
};

enum class sub_system_t : uint32_t {
  e_ecall  = 0b000000000000,
  e_ebreak = 0b000000000001,
  e_mret   = 0b001100000010,
};

// TODO: cleanup and add func7 enum
enum class i_type_func3_t : uint32_t {
  e_lb  = 0b000,
  e_lh  = 0b001,
  e_lw  = 0b010,
  e_lbu = 0b100,
  e_lhu = 0b101,
  e_lwu = 0b110,
  e_ld  = 0b011,

  e_addi         = 0b000,
  e_slti         = 0b010,
  e_sltiu        = 0b011,
  e_xori         = 0b100,
  e_ori          = 0b110,
  e_andi         = 0b111,
  e_slli         = 0b001,
  e_srli_or_srai = 0b101,

  e_addiw          = 0b000,
  e_slliw          = 0b001,
  e_srliw_or_sraiw = 0b101,

  e_sub_system = 0b000,
  e_csrrw      = 0b001,
  e_csrrs      = 0b010,
  e_csrrc      = 0b011,
  e_csrrwi     = 0b101,
  e_csrrsi     = 0b110,
  e_csrrci     = 0b111,
};

enum class store_t : uint32_t {
  e_sb = 0b000,
  e_sh = 0b001,
  e_sw = 0b010,
  e_sd = 0b011,
};

enum class r_type_func7_t : uint32_t {
  e_0000000 = 0b0000000,
  e_0100000 = 0b0100000,
  e_0000001 = 0b0000001,
};

enum class r_type_func3_t : uint32_t {
  // e_add_or_sub = 0b000,
  e_add  = 0b000,
  e_sub  = 0b000,  // *
  e_sll  = 0b001,
  e_slt  = 0b010,
  e_sltu = 0b011,
  e_xor  = 0b100,
  // e_srl_or_sra = 0b101,
  e_srl = 0b101,
  e_sra = 0b101,  // *
  e_or  = 0b110,
  e_and = 0b111,

  // e_addw_or_subw = 0b000,
  e_addw = 0b000,
  e_subw = 0b000,  // *
  e_sllw = 0b001,
  // e_srlw_or_sraw = 0b101,
  e_srlw = 0b101,
  e_sraw = 0b101,  // *

  e_mul    = 0b000,
  e_mulh   = 0b001,
  e_mulhsu = 0b010,
  e_mulhu  = 0b011,
  e_div    = 0b100,
  e_divu   = 0b101,
  e_rem    = 0b110,
  e_remu   = 0b111,

  e_mulw  = 0b000,
  e_divw  = 0b100,
  e_divuw = 0b101,
  e_remw  = 0b110,
  e_remuw = 0b111,
};

struct base_t {
  op_t     _opcode : 7;   // 0-6
  uint32_t _pad    : 25;  // 7-31

  constexpr op_t opcode() const { return _opcode; }
};

struct i_type_t {
  op_t           _opcode : 7;   // 0-6
  uint32_t       _rd     : 5;   // 7-11
  i_type_func3_t _funct3 : 3;   // 12-14
  uint32_t       _rs1    : 5;   // 15-19
  uint32_t       _imm    : 12;  // 20-31

  constexpr op_t           opcode() const { return _opcode; }
  constexpr uint32_t       rd() const { return _rd; }
  constexpr i_type_func3_t funct3() const { return _funct3; }
  constexpr uint32_t       rs1() const { return _rs1; }
  constexpr uint32_t       imm() const { return _imm; }
  constexpr int32_t        imm_sext() const { return ::dawn::sext(imm(), 12); }
  constexpr uint32_t       shamt() const {
    return ::dawn::extract_bit_range(imm(), 0, 5);
  }
  constexpr uint32_t shamt_w() const {
    return ::dawn::extract_bit_range(imm(), 0, 4);
  }
};

struct s_type_t {
  op_t     _opcode : 7;  // 0-6
  uint32_t _imm1   : 5;  // 7-11
  store_t  _funct3 : 3;  // 12-14
  uint32_t _rs1    : 5;  // 15-19
  uint32_t _rs2    : 5;  // 20-24
  uint32_t _imm2   : 7;  // 25-31

  constexpr op_t     opcode() const { return _opcode; }
  constexpr store_t  funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t rs2() const { return _rs2; }
  constexpr uint32_t imm() const { return (_imm2 << 5) | _imm1; }
  constexpr int32_t  imm_sext() const { return ::dawn::sext(imm(), 12); }
};

struct u_type_t {
  op_t     _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _imm    : 20;  // 12-31

  constexpr op_t     opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t imm() const { return _imm; }
  constexpr int32_t  imm_sext() const { return ::dawn::sext(imm(), 20); }
};

struct r_type_t {
  op_t           _opcode : 7;  // 0-6
  uint32_t       _rd     : 5;  // 7-11
  r_type_func3_t _funct3 : 3;  // 12-14
  uint32_t       _rs1    : 5;  // 15-19
  uint32_t       _rs2    : 5;  // 20-24
  r_type_func7_t _funct7 : 7;  // 25-31

  constexpr op_t           opcode() const { return _opcode; }
  constexpr uint32_t       rd() const { return _rd; }
  constexpr r_type_func3_t funct3() const { return _funct3; }
  constexpr uint32_t       rs1() const { return _rs1; }
  constexpr uint32_t       rs2() const { return _rs2; }
  constexpr r_type_func7_t funct7() const { return _funct7; }
};

struct a_type_t {
  op_t           _opcode : 7;
  uint32_t       _rd     : 5;
  a_type_func3_t _funct3 : 3;
  uint32_t       _rs1    : 5;
  uint32_t       _rs2    : 5;
  uint32_t       _rl     : 1;
  uint32_t       _aq     : 1;
  a_type_func5_t _funct5 : 5;

  constexpr op_t           opcode() const { return _opcode; }
  constexpr uint32_t       rd() const { return _rd; }
  constexpr a_type_func3_t funct3() const { return _funct3; }
  constexpr uint32_t       rs1() const { return _rs1; }
  constexpr uint32_t       rs2() const { return _rs2; }
  constexpr uint32_t       rl() const { return _rl; }
  constexpr uint32_t       aq() const { return _aq; }
  constexpr a_type_func5_t funct5() const { return _funct5; }
};

struct b_type_t {
  op_t     _opcode : 7;  // 0-6
  uint32_t _imm1   : 1;  // 7
  uint32_t _imm2   : 4;  // 8-11
  branch_t _funct3 : 3;  // 12-14
  uint32_t _rs1    : 5;  // 15-19
  uint32_t _rs2    : 5;  // 20-24
  uint32_t _imm3   : 6;  // 25-30
  uint32_t _imm4   : 1;  // 31

  constexpr op_t     opcode() const { return _opcode; }
  constexpr uint32_t imm1() const { return _imm1; }
  constexpr uint32_t imm2() const { return _imm2; }
  constexpr branch_t funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t rs2() const { return _rs2; }
  constexpr uint32_t imm3() const { return _imm3; }
  constexpr uint32_t imm4() const { return _imm4; }
  constexpr uint32_t imm() const {
    return _imm4 << 12 | _imm1 << 11 | _imm3 << 5 | _imm2 << 1;
  }
  constexpr int32_t imm_sext() const { return ::dawn::sext(imm(), 13); }
};

struct j_type_t {
  op_t     _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _imm1   : 8;   // 12-19
  uint32_t _imm2   : 1;   // 20
  uint32_t _imm3   : 10;  // 21-30
  uint32_t _imm4   : 1;   // 31

  constexpr op_t     opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t imm1() const { return _imm1; }
  constexpr uint32_t imm2() const { return _imm2; }
  constexpr uint32_t imm3() const { return _imm3; }
  constexpr uint32_t imm4() const { return _imm4; }
  constexpr uint32_t imm() const {
    return _imm4 << 20 | _imm1 << 12 | _imm2 << 11 | _imm3 << 1;
  }
  constexpr int64_t imm_sext() const { return ::dawn::sext(imm(), 21); }
};

struct instruction_t {
  union as_t {
    base_t   base;
    i_type_t i_type;
    s_type_t s_type;
    u_type_t u_type;
    r_type_t r_type;
    b_type_t b_type;
    j_type_t j_type;
    a_type_t a_type;
  } as;
  constexpr op_t opcode() const { return as.base.opcode(); }
};
static_assert(sizeof(instruction_t) == 4, "instruction size should be 4 bytes");

enum class exception_code_t : uint32_t {
  e_instruction_address_misaligned = 0,
  e_instruction_access_fault       = 1,
  e_illegal_instruction            = 2,
  e_breakpoint                     = 3,
  e_load_address_misaligned        = 4,
  e_load_access_fault              = 5,
  e_store_address_misaligned       = 6,
  e_store_access_fault             = 7,
  e_ecall_u_mode                   = 8,
  e_ecall_s_mode                   = 9,
  e_ecall_m_mode                   = 11,
};

constexpr uint32_t MHARDID = 0xf14;

// unimplemented
constexpr uint32_t MNSTATUS = 0x744;
constexpr uint32_t SATP     = 0x180;
// TODO: better way to define PMPADDRs
constexpr uint32_t PMPADDR0 = 0x3b0;
// TODO: better way to define PMPCFGs
constexpr uint32_t PMPCFG0 = 0x3a0;

constexpr uint32_t MEDELEG = 0x302;
constexpr uint32_t MIDELEG = 0x303;
constexpr uint32_t MIE     = 0x304;

constexpr uint32_t MSTATUS            = 0x300;
constexpr uint32_t MSTATUS_MIE_SHIFT  = 3;
constexpr uint32_t MSTATUS_MIE_MASK   = 1u << MSTATUS_MIE_SHIFT;
constexpr uint32_t MSTATUS_MPIE_SHIFT = 7;
constexpr uint32_t MSTATUS_MPIE_MASK  = 1u << MSTATUS_MPIE_SHIFT;
constexpr uint32_t MSTATUS_MPP_SHIFT  = 11;
constexpr uint32_t MSTATUS_MPP_MASK   = 0b11u << MSTATUS_MPP_SHIFT;

constexpr uint32_t MTVEC                 = 0x305;
constexpr uint32_t MTVEC_MODE_MASK       = 0b11;
constexpr uint32_t MTVEC_BASE_ALIGN_MASK = ~0b11u;

constexpr uint32_t MEPC = 0x341;

constexpr uint32_t MCAUSE               = 0x342;
constexpr uint32_t MCAUSE_INTERRUPT_BIT = 0x80000000;

constexpr uint32_t MTVAL = 0x343;

}  // namespace riscv

}  // namespace dawn

std::ostream& operator<<(std::ostream&                     o,
                         const dawn::riscv::instruction_t& instruction);

std::ostream& operator<<(std::ostream&                       o,
                         const dawn::riscv::exception_code_t exception);

#endif  // !DAWN_TYPES_HPP
