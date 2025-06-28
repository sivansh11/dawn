#ifndef DAWN_TYPES_HPP
#define DAWN_TYPES_HPP

#include <cstdint>

#include "helper.hpp"

namespace dawn {

struct base_t {
  uint32_t opcode : 7;   // 0-6
  uint32_t pad    : 25;  // 7-31
};

struct i_type_t {
  uint32_t _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _funct3 : 3;   // 12-14
  uint32_t _rs1    : 5;   // 15-19
  uint32_t _imm    : 12;  // 20-31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t imm() const { return _imm; }
  constexpr int32_t  imm_sext() const { return ::dawn::sext(imm(), 12); }
  constexpr uint32_t shamt() const {
    return ::dawn::extract_bit_range(imm(), 0, 5);
  }
  constexpr uint32_t shamt_w() const {
    return ::dawn::extract_bit_range(imm(), 0, 4);
  }
};

struct s_type_t {
  uint32_t _opcode : 7;  // 0-6
  uint32_t _imm1   : 5;  // 7-11
  uint32_t _funct3 : 3;  // 12-14
  uint32_t _rs1    : 5;  // 15-19
  uint32_t _rs2    : 5;  // 20-24
  uint32_t _imm2   : 7;  // 25-31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t rs2() const { return _rs2; }
  constexpr uint32_t imm() const { return (_imm2 << 5) | _imm1; }
  constexpr int32_t  imm_sext() const { return ::dawn::sext(imm(), 12); }
};

struct u_type_t {
  uint32_t _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _imm    : 20;  // 12-31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t imm() const { return _imm; }
  constexpr int32_t  imm_sext() const { return ::dawn::sext(imm(), 20); }
};

struct r_type_t {
  uint32_t _opcode : 7;  // 0-6
  uint32_t _rd     : 5;  // 7-11
  uint32_t _funct3 : 3;  // 12-14
  uint32_t _rs1    : 5;  // 15-19
  uint32_t _rs2    : 5;  // 20-24
  uint32_t _funct7 : 7;  // 25-31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t rs2() const { return _rs2; }
  constexpr uint32_t funct7() const { return _funct7; }
};

struct b_type_t {
  uint32_t _opcode : 7;  // 0-6
  uint32_t _imm1   : 1;  // 7
  uint32_t _imm2   : 4;  // 8-11
  uint32_t _funct3 : 3;  // 12-14
  uint32_t _rs1    : 5;  // 15-19
  uint32_t _rs2    : 5;  // 20-24
  uint32_t _imm3   : 6;  // 25-30
  uint32_t _imm4   : 1;  // 31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t imm1() const { return _imm1; }
  constexpr uint32_t imm2() const { return _imm2; }
  constexpr uint32_t funct3() const { return _funct3; }
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
  uint32_t _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _imm1   : 8;   // 12-19
  uint32_t _imm2   : 1;   // 20
  uint32_t _imm3   : 10;  // 21-30
  uint32_t _imm4   : 1;   // 31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t imm1() const { return _imm1; }
  constexpr uint32_t imm2() const { return _imm2; }
  constexpr uint32_t imm3() const { return _imm3; }
  constexpr uint32_t imm4() const { return _imm4; }
  constexpr uint32_t imm() const {
    return _imm4 << 20 | _imm1 << 12 | _imm2 << 11 | _imm3 << 1;
  }
  constexpr int32_t imm_sext() const { return ::dawn::sext(imm(), 20); }
};

struct a_type_t {
  uint32_t _opcode : 7;
  uint32_t _rd     : 5;
  uint32_t _funct3 : 3;
  uint32_t _rs1    : 5;
  uint32_t _rs2    : 5;
  uint32_t _rl     : 1;
  uint32_t _aq     : 1;
  uint32_t _funct5 : 5;

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t funct3() const { return _funct3; }
  constexpr uint32_t rs1() const { return _rs1; }
  constexpr uint32_t rs2() const { return _rs2; }
  constexpr uint32_t rl() const { return _rl; }
  constexpr uint32_t aq() const { return _aq; }
  constexpr uint32_t funct5() const { return _funct5; }
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
    // a_type_t a_type;
  } as;
  constexpr uint32_t opcode() const { return as.base.opcode; }
};

static_assert(sizeof(instruction_t) == 4, "instruction size should be 4 bytes");

enum class privilege_mode_t : uint32_t {
  e_user       = 0b00,
  e_supervisor = 0b01,
  e_machine    = 0b11,
};

// TODO: add others
enum class exception_code_t : uint32_t {
  e_instruction_address_misaligned = 0,
  e_instruction_access_fault       = 1,
  e_illegal_instruction            = 2,
  e_breakpoint                     = 3,
  e_ecall_u_mode                   = 8,
  e_ecall_s_mode                   = 9,
  e_ecall_m_mode                   = 11,
};

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

}  // namespace dawn

#endif  // !DAWN_TYPES_HPP
