#ifndef DAWN_MACHINE_HPP
#define DAWN_MACHINE_HPP

#include <bitset>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace dawn {

struct mmio_handler_t;

typedef uint64_t (*load64)(const mmio_handler_t *handler, uint64_t);
typedef void (*store64)(const mmio_handler_t *handler, uint64_t, uint64_t);

struct mmio_handler_t {
  uint64_t _start;
  uint64_t _stop;
  load64   _load64;
  store64  _store64;
};

// [start, end)
constexpr inline uint32_t extract_bit_range(uint32_t value, uint8_t start,
                                            uint8_t end) {
  constexpr uint8_t total_bits = sizeof(uint32_t) * 8;
  uint8_t           length     = end - start;
  uint32_t          shifted    = value >> start;
  uint32_t          mask =
      (length >= total_bits) ? ~uint32_t(0) : (uint32_t(1) << length) - 1;
  return shifted & mask;
}

template <uint32_t sign_bit>
constexpr inline int64_t sext(uint32_t val) {
  struct internal_t {
    int64_t val : sign_bit;
  } s;
  return s.val = val;
}

enum class exception_code_t : uint64_t {
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

  e_machine_software_interrupt = 3 | (1ull << 63),
  e_machine_timer_interrupt    = 7 | (1ull << 63),
  e_machine_external_interrupt = 11 | (1ull << 63),
};

constexpr uint64_t MHARDID = 0xf14;

constexpr uint64_t MNSTATUS = 0x744;

constexpr uint64_t MEDELEG = 0x302;
constexpr uint64_t MIDELEG = 0x303;
constexpr uint64_t MIE     = 0x304;

constexpr uint64_t MIP = 0x344;
constexpr uint64_t MIP_MEIP_MASK =
    1u << (static_cast<uint64_t>(
               exception_code_t::e_machine_external_interrupt) &
           ((1ull << 63) - 1));
constexpr uint64_t MIP_MSIP_MASK =
    1u << (static_cast<uint64_t>(
               exception_code_t::e_machine_software_interrupt) &
           ((1ull << 63) - 1));
constexpr uint64_t MIP_MTIP_MASK =
    1u << (static_cast<uint64_t>(exception_code_t::e_machine_timer_interrupt) &
           ((1ull << 63) - 1));

constexpr uint64_t MSTATUS            = 0x300;
constexpr uint64_t MSTATUS_MIE_SHIFT  = 3;
constexpr uint64_t MSTATUS_MIE_MASK   = 1u << MSTATUS_MIE_SHIFT;
constexpr uint64_t MSTATUS_MPIE_SHIFT = 7;
constexpr uint64_t MSTATUS_MPIE_MASK  = 1u << MSTATUS_MPIE_SHIFT;
constexpr uint64_t MSTATUS_MPP_SHIFT  = 11;
constexpr uint64_t MSTATUS_MPP_MASK   = 0b11u << MSTATUS_MPP_SHIFT;

constexpr uint64_t MTVEC                 = 0x305;
constexpr uint64_t MTVEC_MODE_MASK       = 0b11;
constexpr uint64_t MTVEC_BASE_ALIGN_MASK = ~0b11ull;

constexpr uint64_t MEPC = 0x341;

constexpr uint64_t MCAUSE               = 0x342;
constexpr uint64_t MCAUSE_INTERRUPT_BIT = (1ull << 63);

constexpr uint64_t MTVAL = 0x343;

struct base_t {
  uint32_t _opcode : 7;   // 0-6
  uint32_t _pad    : 25;  // 7-31

  constexpr uint32_t opcode() const { return _opcode; }
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
  constexpr int32_t  imm_sext() const { return sext<12>(imm()); }
  constexpr uint32_t shamt() const { return extract_bit_range(imm(), 0, 6); }
  constexpr uint32_t shamt_w() const { return extract_bit_range(imm(), 0, 5); }
                     operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
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
  constexpr int32_t  imm_sext() const { return sext<12>(imm()); }
                     operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
};

struct u_type_t {
  uint32_t _opcode : 7;   // 0-6
  uint32_t _rd     : 5;   // 7-11
  uint32_t _imm    : 20;  // 12-31

  constexpr uint32_t opcode() const { return _opcode; }
  constexpr uint32_t rd() const { return _rd; }
  constexpr uint32_t imm() const { return _imm; }
  constexpr int32_t  imm_sext() const { return sext<20>(imm()); }
                     operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
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
                     operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
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
                     operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
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
  constexpr int32_t imm_sext() const { return sext<13>(imm()); }
                    operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
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
  constexpr int64_t imm_sext() const { return sext<21>(imm()); }
                    operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
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
  operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
};
static_assert(sizeof(instruction_t) == 4, "instruction size should be 4 bytes");

constexpr inline void mul_64x64_u(uint64_t a, uint64_t b, uint64_t result[2]) {
  const uint64_t mask_32    = 0xffffffffull;
  uint64_t       a_h        = a >> 32;
  uint64_t       a_l        = a & mask_32;
  uint64_t       b_h        = b >> 32;
  uint64_t       b_l        = b & mask_32;
  uint64_t       p0         = a_l * b_l;
  uint64_t       p1         = a_l * b_h;
  uint64_t       p2         = a_h * b_l;
  uint64_t       p3         = a_h * b_h;
  uint64_t carry_to_high_32 = (p0 >> 32) + (p1 & mask_32) + (p2 & mask_32);
  result[0]                 = (p0 & mask_32) | (carry_to_high_32 << 32);
  result[1] = p3 + (p1 >> 32) + (p2 >> 32) + (carry_to_high_32 >> 32);
}

// TODO: accurate runtime memory bounds checking (account for size of
// load/store)
struct machine_t {
  machine_t(size_t ram_size, uint64_t offset,
            const std::vector<mmio_handler_t> mmios)
      : _ram_size(ram_size), _offset(offset), _mmios(mmios) {
    _data  = new uint8_t[ram_size];
    _final = reinterpret_cast<uint8_t *>(reinterpret_cast<uintptr_t>(_data) -
                                         _offset);
  }
  ~machine_t() { delete[] _data; }

  // TODO: a more involved csr read
  inline uint64_t read_csr(uint16_t csrno) { return _csr[csrno]; }
  // TODO: a more involved csr write
  inline void write_csr(uint16_t csrno, uint64_t value) { _csr[csrno] = value; }

#define fetch32(res, addr) res = *reinterpret_cast<uint32_t *>(_final + addr)

#define _dawn_load(res, addr)                                             \
  do {                                                                    \
    if (_mru_mmio._start <= addr && addr < _mru_mmio._stop) {             \
      res = _mru_mmio._load64(&_mru_mmio, addr);                          \
      break;                                                              \
    }                                                                     \
    bool is_mmio = false;                                                 \
    for (auto &mmio : _mmios) {                                           \
      if (mmio._start <= addr && addr < mmio._stop) [[unlikely]] {        \
        _mru_mmio = mmio;                                                 \
        res       = mmio._load64(&mmio, addr);                            \
        is_mmio   = true;                                                 \
        break;                                                            \
      }                                                                   \
    }                                                                     \
    if (is_mmio) [[unlikely]]                                             \
      break;                                                              \
    if ((addr < _offset) || addr >= (_offset + _ram_size)) [[unlikely]] { \
      handle_trap(exception_code_t::e_load_access_fault, addr);           \
      do_dispatch();                                                      \
    }                                                                     \
    res = *reinterpret_cast<decltype(res) *>(_final + addr);              \
  } while (false)

#define load8(res, addr)   _dawn_load(res, addr)
#define load16(res, addr)  _dawn_load(res, addr)
#define load32(res, addr)  _dawn_load(res, addr)
#define load64(res, addr)  _dawn_load(res, addr)
#define load8i(res, addr)  _dawn_load(res, addr)
#define load16i(res, addr) _dawn_load(res, addr)
#define load32i(res, addr) _dawn_load(res, addr)

#define _dawn_store(type, addr, value)                                    \
  do {                                                                    \
    if (_mru_mmio._start <= addr && addr < _mru_mmio._stop) {             \
      _mru_mmio._store64(&_mru_mmio, addr, value);                        \
      break;                                                              \
    }                                                                     \
    bool is_mmio = false;                                                 \
    for (auto &mmio : _mmios) {                                           \
      if (mmio._start <= addr && addr < mmio._stop) [[unlikely]] {        \
        _mru_mmio = mmio;                                                 \
        mmio._store64(&mmio, addr, value);                                \
        is_mmio = true;                                                   \
        break;                                                            \
      }                                                                   \
    }                                                                     \
    if (is_mmio) [[unlikely]]                                             \
      break;                                                              \
    if ((addr < _offset) || addr >= (_offset + _ram_size)) [[unlikely]] { \
      handle_trap(exception_code_t::e_store_access_fault, addr);          \
      do_dispatch();                                                      \
    }                                                                     \
    *reinterpret_cast<type *>(_final + addr) = value;                     \
  } while (false)

#define store8(addr, value)  _dawn_store(uint8_t, addr, value)
#define store16(addr, value) _dawn_store(uint16_t, addr, value)
#define store32(addr, value) _dawn_store(uint32_t, addr, value)
#define store64(addr, value) _dawn_store(uint64_t, addr, value)

  inline void memcpy_host_to_guest(uint64_t dst, const void *src, size_t size) {
    std::memcpy(_final + dst, src, size);
  }
  inline void memcpy_guest_to_host(void *dst, uint64_t src, size_t size) {
    std::memcpy(dst, _final + src, size);
  }
  inline void memset(uint64_t addr, int value, size_t size) {
    std::memset(_final + addr, value, size);
  }
  inline uint8_t *at(uint64_t addr) { return _final + addr; }

  // TODO: test with and without inline
  // TODO: test with a macro
  inline bool handle_trap(exception_code_t cause, uint64_t value) {
    // hack
    if (cause == exception_code_t::e_ecall_m_mode ||
        cause == exception_code_t::e_ecall_u_mode) [[unlikely]] {
      auto itr = _syscalls.find(_reg[17]);
      if (itr != _syscalls.end()) {
        itr->second(*this);
        _pc += 4;
        return true;
      }
    }

    bool is_interrupt =
        (static_cast<uint64_t>(cause) & MCAUSE_INTERRUPT_BIT) != 0;
    uint64_t &mstatus    = _csr[MSTATUS];
    bool      global_mie = (mstatus & MSTATUS_MIE_MASK) != 0;

    _csr[MEPC]   = _pc;
    _csr[MCAUSE] = static_cast<uint64_t>(cause);
    _csr[MTVAL]  = value;

    mstatus = (mstatus & ~MSTATUS_MPP_MASK) |
              ((static_cast<uint64_t>(_mode) << MSTATUS_MPP_SHIFT) &
               MSTATUS_MPP_MASK);
    mstatus = (mstatus & ~MSTATUS_MPIE_MASK) |
              ((global_mie << MSTATUS_MPIE_SHIFT) & MSTATUS_MPIE_MASK);
    mstatus &= ~MSTATUS_MIE_MASK;

    uint64_t mtvec      = _csr[MTVEC];
    uint64_t mtvec_base = mtvec & MTVEC_BASE_ALIGN_MASK;
    uint64_t mtvec_mode = mtvec & MTVEC_MODE_MASK;

    if (mtvec_mode == 0b01 && is_interrupt) {
      uint64_t interrupt_code =
          static_cast<uint64_t>(cause) & ~MCAUSE_INTERRUPT_BIT;
      _pc = mtvec_base + (interrupt_code * 4);
    } else {
      _pc = mtvec_base;
    }
    _mode = 0b11;

    // generally will only happen if mtvec is not set
    if (_pc == 0) [[unlikely]] {
      switch (cause) {
        default:
          std::stringstream ss;
          ss << "Error: " << static_cast<uint64_t>(cause) << '\n';
          ss << "at: " << std::hex << _pc << std::dec << '\n';
          throw std::runtime_error(ss.str());
      }
    }

    return true;
  }

  inline uint64_t step(uint64_t n) {
    static void *dispatch_table[256] = {nullptr};

    // initialize
    static bool initialized = false;
    if (!initialized) [[unlikely]] {
      initialized = true;
      for (auto &entry : dispatch_table) entry = &&do_unknown_instruction;

#define register_range(op, label)                                             \
  do {                                                                        \
    for (uint32_t i = 0; i < 8; i++) dispatch_table[op | (i << 5)] = &&label; \
  } while (false)
#define register_instr(op, func3, label)       \
  do {                                         \
    dispatch_table[op | func3 << 5] = &&label; \
  } while (false)

      register_range(0b01101, do_lui);
      register_range(0b00101, do_auipc);
      register_range(0b11011, do_jal);
      register_range(0b11001, do_jalr);
      register_instr(0b11000, 0b000, do_beq);
      register_instr(0b11000, 0b001, do_bne);
      register_instr(0b11000, 0b100, do_blt);
      register_instr(0b11000, 0b101, do_bge);
      register_instr(0b11000, 0b110, do_bltu);
      register_instr(0b11000, 0b111, do_bgeu);
      register_instr(0b00000, 0b000, do_lb);
      register_instr(0b00000, 0b001, do_lh);
      register_instr(0b00000, 0b010, do_lw);
      register_instr(0b00000, 0b100, do_lbu);
      register_instr(0b00000, 0b101, do_lhu);
      register_instr(0b01000, 0b000, do_sb);
      register_instr(0b01000, 0b001, do_sh);
      register_instr(0b01000, 0b010, do_sw);
      register_instr(0b00100, 0b000, do_addi);
      register_instr(0b00100, 0b010, do_slti);
      register_instr(0b00100, 0b011, do_sltiu);
      register_instr(0b00100, 0b100, do_xori);
      register_instr(0b00100, 0b110, do_ori);
      register_instr(0b00100, 0b111, do_andi);
      register_instr(0b00000, 0b110, do_lwu);
      register_instr(0b00000, 0b011, do_ld);
      register_instr(0b01000, 0b011, do_sd);
      register_instr(0b00100, 0b001, do_slli);
      register_instr(0b00100, 0b101, do_srli_or_srai);
      register_instr(0b00110, 0b000, do_addiw);
      register_instr(0b00110, 0b001, do_slliw);
      register_instr(0b00110, 0b101, do_srliw_or_sraiw);
      register_instr(0b01110, 0b000, do_addw_or_subw_or_mulw);
      register_instr(0b01110, 0b001, do_sllw);
      register_instr(0b01110, 0b100, do_divw);
      register_instr(0b01110, 0b101, do_srlw_or_sraw_or_divuw);
      register_instr(0b01110, 0b110, do_remw);
      register_instr(0b01110, 0b111, do_remuw);
      register_instr(0b01100, 0b000, do_add_or_sub_or_mul);
      register_instr(0b01100, 0b001, do_sll_or_mulh);
      register_instr(0b01100, 0b010, do_slt_or_mulhsu);
      register_instr(0b01100, 0b011, do_sltu_or_mulhu);
      register_instr(0b01100, 0b100, do_xor_or_div);
      register_instr(0b01100, 0b101, do_srl_or_sra_or_divu);
      register_instr(0b01100, 0b110, do_or_or_rem);
      register_instr(0b01100, 0b111, do_and_or_remu);
      register_instr(0b00011, 0b000, do_fence);
      register_instr(0b00011, 0b001, do_fence);
      register_instr(0b11100, 0b000, do_system);  // ecall ebreak mret wfi
      register_instr(0b11100, 0b001, do_csrrw);
      register_instr(0b11100, 0b010, do_csrrs);
      register_instr(0b11100, 0b011, do_csrrc);
      register_instr(0b11100, 0b101, do_csrrwi);
      register_instr(0b11100, 0b110, do_csrrsi);
      register_instr(0b11100, 0b111, do_csrrci);
      register_instr(0b01011, 0b010, do_atomic_w);
      register_instr(0b01011, 0b011, do_atomic_d);
    }

    uint32_t      _inst;
    instruction_t inst;

    // f f f o o o o o (f is func3, o is op)
    // note, we ignore the first 2 bits of op since we dont implement compressed
    // instructions
#define dispatch()                                                         \
  do {                                                                     \
    if (_wfi) [[unlikely]]                                                 \
      return n;                                                            \
    _reg[0] = 0;                                                           \
    if (n-- == 0) [[unlikely]]                                             \
      return 0;                                                            \
    fetch32(_inst, _pc);                                                   \
    const uint32_t dispatch_index = extract_bit_range(_inst, 2, 7) |       \
                                    extract_bit_range(_inst, 12, 15) << 5; \
    reinterpret_cast<uint32_t &>(inst) = _inst;                            \
    goto *dispatch_table[dispatch_index];                                  \
  } while (false)

#ifdef DAWN_ENABLE_LOGGING
    auto logger = [&]() {
      _log << std::hex                       //
           << "pc: " << _pc << '\n'          //
           << "\tra: " << _reg[1] << '\n'    //
           << "\tsp: " << _reg[2] << '\n'    //
           << "\tx9: " << _reg[9] << '\n'    //
           << "\tx14: " << _reg[14] << '\n'  //
           << "\tx15: " << _reg[15] << '\n';

      _log.flush();
    };

#define do_dispatch() \
  do {                \
    logger();         \
    dispatch();       \
  } while (false)
#else
#define do_dispatch() dispatch()
#endif

    // check pending interrupts
    uint64_t pending_interrupts = _csr[MIP] & _csr[MIE];
    if (pending_interrupts) {
      _wfi = false;
      if (_mode < 0b11 || _csr[MSTATUS] & MSTATUS_MIE_MASK) {
        if (pending_interrupts & MIP_MEIP_MASK) {
          handle_trap(exception_code_t::e_machine_external_interrupt, 0);
          do_dispatch();
        } else if (pending_interrupts & MIP_MSIP_MASK) {
          handle_trap(exception_code_t::e_machine_software_interrupt, 0);
          do_dispatch();
        } else if (pending_interrupts & MIP_MTIP_MASK) {
          handle_trap(exception_code_t::e_machine_timer_interrupt, 0);
          do_dispatch();
        }
        throw std::runtime_error("interrupt pending, but not handled");
      }
    }

    // no need to check every loop, checking once is enough since jump/branch
    // handle misaligned addresses
    if (_pc % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_instruction_address_misaligned, _pc);
      do_dispatch();
    }

    do_dispatch();

  do_lui: {
    _reg[inst.as.u_type.rd()] =
        static_cast<int64_t>(static_cast<int32_t>(inst.as.u_type.imm() << 12));
    _pc += 4;
  }
    do_dispatch();

  do_auipc: {
    _reg[inst.as.u_type.rd()] =
        _pc + static_cast<int32_t>(inst.as.u_type.imm() << 12);
    _pc += 4;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_jal: {
    uint64_t addr = _pc + inst.as.j_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
      do_dispatch();
    }
    _reg[inst.as.j_type.rd()] = _pc + 4;
    _pc                       = addr;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_jalr: {
    uint64_t target  = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    uint64_t next_pc = target & ~1ull;
    if (next_pc % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_instruction_address_misaligned, next_pc);
      do_dispatch();
    }
    _reg[inst.as.i_type.rd()] = _pc + 4;
    _pc                       = next_pc;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_beq: {
    if (_reg[inst.as.b_type.rs1()] == _reg[inst.as.b_type.rs2()]) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_bne: {
    if (_reg[inst.as.b_type.rs1()] != _reg[inst.as.b_type.rs2()]) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_blt: {
    if (static_cast<int64_t>(_reg[inst.as.b_type.rs1()]) <
        static_cast<int64_t>(_reg[inst.as.b_type.rs2()])) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_bge: {
    if (static_cast<int64_t>(_reg[inst.as.b_type.rs1()]) >=
        static_cast<int64_t>(_reg[inst.as.b_type.rs2()])) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_bltu: {
    if (_reg[inst.as.b_type.rs1()] < _reg[inst.as.b_type.rs2()]) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  do_bgeu: {
    if (_reg[inst.as.b_type.rs1()] >= _reg[inst.as.b_type.rs2()]) {
      uint64_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        handle_trap(exception_code_t::e_instruction_address_misaligned, addr);
        do_dispatch();
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

  do_lb: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    int8_t   value;
    load8i(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = static_cast<int64_t>(value);
    _pc += 4;
  }
    do_dispatch();

  do_lh: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 2 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_load_address_misaligned, addr);
      do_dispatch();
    }
    int16_t value;
    load16i(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = static_cast<int64_t>(value);
    _pc += 4;
  }
    do_dispatch();

  do_lw: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_load_address_misaligned, addr);
      do_dispatch();
    }
    int32_t value;
    load32i(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = static_cast<int64_t>(value);
    _pc += 4;
  }
    do_dispatch();

  do_lbu: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    uint8_t  value;
    load8(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  do_lhu: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 2 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_load_address_misaligned, addr);
      do_dispatch();
    }
    uint16_t value;
    load16(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  do_sb: {
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
    store8(addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  do_sh: {
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
    if (addr % 2 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_store_address_misaligned, addr);
      do_dispatch();
    }
    store16(addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  do_sw: {
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_store_address_misaligned, addr);
      do_dispatch();
    }
    store32(addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  do_addi: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_slti: {
    _reg[inst.as.i_type.rd()] =
        static_cast<int64_t>(_reg[inst.as.i_type.rs1()]) <
        inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_sltiu: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] < inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_xori: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] ^ inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_ori: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] | inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_andi: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] & inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  do_lwu: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_load_address_misaligned, addr);
      do_dispatch();
    }
    uint32_t value;
    load32(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  do_ld: {
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 8 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_load_address_misaligned, addr);
      do_dispatch();
    }
    uint64_t value;
    load64(value, addr);  // may fault
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  do_sd: {
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
    if (addr % 8 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_store_address_misaligned, addr);
      do_dispatch();
    }
    store64(addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  do_slli: {
    _reg[inst.as.i_type.rd()] = _reg[inst.as.i_type.rs1()]
                                << (inst.as.i_type.imm() & 0x3f);
    _pc += 4;
  }
    do_dispatch();

  do_srli_or_srai: {
    switch (inst.as.i_type.imm() >> 6) {
      case 0b000000: {  // srli
        _reg[inst.as.i_type.rd()] =
            _reg[inst.as.i_type.rs1()] >> (inst.as.i_type.imm() & 0x3f);
        _pc += 4;
      } break;
      case 0b010000: {  // srai
        int64_t  rs1_val = static_cast<int64_t>(_reg[inst.as.i_type.rs1()]);
        uint32_t shamt   = inst.as.i_type.imm() & 0x3f;
        _reg[inst.as.i_type.rd()] = rs1_val >> shamt;
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_addiw: {
    _reg[inst.as.i_type.rd()] = static_cast<int32_t>(static_cast<uint32_t>(
        _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext()));
    _pc += 4;
  }
    do_dispatch();

  do_slliw: {
    _reg[inst.as.i_type.rd()] = static_cast<int32_t>(static_cast<uint32_t>(
        _reg[inst.as.i_type.rs1()]
        << static_cast<uint32_t>(inst.as.i_type.shamt_w())));
    _pc += 4;
  }
    do_dispatch();

  do_srliw_or_sraiw: {
    switch (inst.as.i_type.imm() >> 5) {
      case 0b0000000: {  // srliw
        _reg[inst.as.i_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.i_type.rs1()]) >>
            inst.as.i_type.shamt_w());
        _pc += 4;
      } break;
      case 0b0100000: {  // sraiw
        _reg[inst.as.i_type.rd()] = static_cast<int32_t>(
            static_cast<int32_t>(_reg[inst.as.i_type.rs1()]) >>
            inst.as.i_type.shamt_w());
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_addw_or_subw_or_mulw: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // addw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) +
            static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]));
        _pc += 4;
      } break;
      case 0b0100000: {  // subw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            (static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) -
             static_cast<uint32_t>(_reg[inst.as.r_type.rs2()])));
        _pc += 4;
      } break;
      case 0b0000001: {  // mulw
        _reg[inst.as.r_type.rd()] = static_cast<int64_t>(
            static_cast<int32_t>(_reg[inst.as.r_type.rs1()]) *
            static_cast<int32_t>(_reg[inst.as.r_type.rs2()]));
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_sllw: {
    _reg[inst.as.r_type.rd()] =
        static_cast<int32_t>(static_cast<int32_t>(_reg[inst.as.r_type.rs1()])
                             << (_reg[inst.as.r_type.rs2()] & 0b11111));
    _pc += 4;
  }
    do_dispatch();

  do_divw: {
    int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
    int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs1 == INT32_MIN && rs2 == -1) {
      _reg[inst.as.r_type.rd()] = INT32_MIN;
    } else if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = ~0ull;
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 / rs2);
    }
    _reg[inst.as.r_type.rd()] =
        static_cast<int64_t>(static_cast<int32_t>(_reg[inst.as.r_type.rd()]));
    _pc += 4;
  }
    do_dispatch();

  do_srlw_or_sraw_or_divuw: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // srlw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & 0b11111));
        _pc += 4;
      } break;
      case 0b0100000: {  // sraw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<int32_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & 0b11111));
        _pc += 4;
      } break;
      case 0b0000001: {  // divuw
        uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
        uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
        if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~0u;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = rs1 / rs2;
        }
        _reg[inst.as.r_type.rd()] = static_cast<int64_t>(static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.r_type.rd()])));
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_remw: {
    int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
    int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs1 == INT32_MIN && rs2 == -1) {
      _reg[inst.as.r_type.rd()] = 0;
    } else if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = rs1;
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 % rs2);
    }
    _reg[inst.as.r_type.rd()] =
        static_cast<int64_t>(static_cast<int32_t>(_reg[inst.as.r_type.rd()]));
    _pc += 4;
  }
    do_dispatch();

  do_remuw: {
    uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
    uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = rs1;
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = rs1 % rs2;
    }
    _reg[inst.as.r_type.rd()] = static_cast<int64_t>(
        static_cast<int32_t>(static_cast<uint32_t>(_reg[inst.as.r_type.rd()])));
    _pc += 4;
  }
    do_dispatch();

  do_add_or_sub_or_mul: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // add
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] + _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0100000: {  // sub
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] - _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0000001: {  // mul
        uint64_t rs1              = _reg[inst.as.r_type.rs1()];
        uint64_t rs2              = _reg[inst.as.r_type.rs2()];
        _reg[inst.as.r_type.rd()] = rs1 * rs2;
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_sll_or_mulh: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // sll
        _reg[inst.as.r_type.rd()] = _reg[inst.as.r_type.rs1()]
                                    << (_reg[inst.as.r_type.rs2()] & 0b111111);
        _pc += 4;
      } break;
      case 0b0000001: {  // mulh
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

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_slt_or_mulhsu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // slt
        _reg[inst.as.r_type.rd()] =
            static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) <
            static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);
        _pc += 4;
      } break;
      case 0b0000001: {  // mulhsu
        int64_t  rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        uint64_t result[2];
        mul_64x64_u(rs1, rs2, result);
        uint64_t result_hi = result[1];
        if (rs1 < 0) result_hi -= rs2;
        _reg[inst.as.r_type.rd()] = result_hi;
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_sltu_or_mulhu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // sltu
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] < _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0000001: {  // mulhu
        uint64_t rs1 = _reg[inst.as.r_type.rs1()];
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        uint64_t result[2];
        mul_64x64_u(rs1, rs2, result);
        _reg[inst.as.r_type.rd()] = result[1];
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_xor_or_div: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // xor
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] ^ _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0000001: {  // div
        int64_t rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
        int64_t rs2 = static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);
        if (rs1 == INT64_MIN && rs2 == -1) {
          _reg[inst.as.r_type.rd()] = INT64_MIN;
        } else if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~0ull;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 / rs2);
        }
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_srl_or_sra_or_divu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // srl
        _reg[inst.as.r_type.rd()] = _reg[inst.as.r_type.rs1()] >>
                                    (_reg[inst.as.r_type.rs2()] & 0b111111);
        _pc += 4;
      } break;
      case 0b0100000: {  // sra
        _reg[inst.as.r_type.rd()] =
            static_cast<int64_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & 0b111111);
        _pc += 4;
      } break;
      case 0b0000001: {  // divu
        uint64_t rs1 = _reg[inst.as.r_type.rs1()];
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~0ull;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = rs1 / rs2;
        }
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_or_or_rem: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // or
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] | _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0000001: {  // rem
        int64_t rs1 = static_cast<int64_t>(_reg[inst.as.r_type.rs1()]);
        int64_t rs2 = static_cast<int64_t>(_reg[inst.as.r_type.rs2()]);
        if (rs1 == INT64_MIN && rs2 == -1) {
          _reg[inst.as.r_type.rd()] = 0;
        } else if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = rs1;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = static_cast<uint64_t>(rs1 % rs2);
        }
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_and_or_remu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // and
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] & _reg[inst.as.r_type.rs2()];
        _pc += 4;
      } break;
      case 0b0000001: {  // remu
        uint64_t rs1 = _reg[inst.as.r_type.rs1()];
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = rs1;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = rs1 % rs2;
        }
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_fence: {
    // fence not required ?
    _pc += 4;
  }
    do_dispatch();

  do_system: {
    switch (inst.as.i_type.imm()) {
      case 0b000000000000: {  // ecall
        if (_mode == 0b11)
          handle_trap(exception_code_t::e_ecall_m_mode, _pc);
        else
          handle_trap(exception_code_t::e_ecall_u_mode, _pc);
      }
        do_dispatch();  // goto next instruction no need to break

      case 0b000000000001: {  // ebreak
        handle_trap(exception_code_t::e_breakpoint, _pc);
      }
        do_dispatch();  // goto next instruction no need to break

      case 0b001100000010: {  // mret
        uint64_t &mstatus = _csr[MSTATUS];
        uint64_t  mpp     = (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
        uint64_t  mpie    = (mstatus & MSTATUS_MPIE_MASK) >> MSTATUS_MPIE_SHIFT;
        _mode             = mpp;
        _pc               = _csr[MEPC];
        mstatus = (mstatus & ~MSTATUS_MIE_MASK) | (mpie << MSTATUS_MIE_SHIFT);
        mstatus = (mstatus & ~MSTATUS_MPIE_MASK) | (1u << MSTATUS_MPIE_SHIFT);
        mstatus = (mstatus & ~MSTATUS_MPP_MASK) | (0b00u << MSTATUS_MPP_SHIFT);
      }
        do_dispatch();  // goto next instruction no need to break

      case 0b000100000101: {  // wfi
        _wfi = true;
        if (_wfi_callback) [[likely]]
          _wfi_callback();
        _pc += 4;
      }
        do_dispatch();

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();  // technically not needed, just putting for the sake of
                    // continuity

  do_csrrw: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }

    _csr[addr] = _reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  do_csrrs: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }
    _csr[addr] = csr | _reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  do_csrrc: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }
    _csr[addr] = csr & ~_reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  do_csrrwi: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }
    _csr[addr] = rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  do_csrrsi: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }
    _csr[addr] = csr | rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();
  do_csrrci: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      handle_trap(exception_code_t::e_illegal_instruction, inst);
      do_dispatch();
    }
    _csr[addr] = csr & ~rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  do_atomic_w: {
    switch (inst.as.a_type.funct5()) {
      case 0b00010: {  // lr
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        _reservation_address      = addr;
        _is_reserved              = true;
        _pc += 4;
      } break;
      case 0b00011: {  // sc
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_store_address_misaligned, addr);
          do_dispatch();
        }
        if (_is_reserved && _reservation_address == addr) {
          store32(addr, static_cast<uint32_t>(rs2));
          _reg[inst.as.a_type.rd()] = 0;
        } else {
          _reg[inst.as.a_type.rd()] = 1;
        }
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00001: {  // amoswap
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, static_cast<uint32_t>(rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00000: {  // amoadd
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, static_cast<uint32_t>(value + rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00100: {  // amoxor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, static_cast<uint32_t>(value ^ rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b01100: {  // amoand
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, static_cast<uint32_t>(value & rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b01000: {  // amoor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, static_cast<uint32_t>(value | rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b10000: {  // amomin
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr,
                static_cast<uint32_t>(std::min(static_cast<int32_t>(value),
                                               static_cast<int32_t>(rs2))));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b10100: {  // amomax
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr,
                static_cast<uint32_t>(std::max(static_cast<int32_t>(value),
                                               static_cast<int32_t>(rs2))));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b11000: {  // amominu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, std::min(static_cast<uint32_t>(value),
                               static_cast<uint32_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b11100: {  // amomaxu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint32_t value;
        load32(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        store32(addr, std::max(static_cast<uint32_t>(value),
                               static_cast<uint32_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_atomic_d: {
    switch (inst.as.a_type.funct5()) {
      case 0b00010: {  // lr
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        _reservation_address      = addr;
        _is_reserved              = true;
        _pc += 4;
      } break;
      case 0b00011: {  // sc
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_store_address_misaligned, addr);
          do_dispatch();
        }
        if (_is_reserved && _reservation_address == addr) {
          // no e_store_access_fault in this implementation
          store64(addr, rs2);
          _reg[inst.as.a_type.rd()] = 0;
        } else {
          _reg[inst.as.a_type.rd()] = 1;
        }
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00001: {  // amoswap
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00000: {  // amoadd
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, value + rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b00100: {  // amoxor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, value ^ rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b01100: {  // amoand
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, value & rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b01000: {  // amoor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, value | rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b10000: {  // amomin
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, std::min(static_cast<int64_t>(value),
                               static_cast<int64_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b10100: {  // amomax
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, std::max(static_cast<int64_t>(value),
                               static_cast<int64_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b11000: {  // amominu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, std::min(static_cast<uint64_t>(value),
                               static_cast<uint64_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;
      case 0b11100: {  // amomaxu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          handle_trap(exception_code_t::e_load_address_misaligned, addr);
          do_dispatch();
        }
        uint64_t value;
        load64(value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        store64(addr, std::max(static_cast<uint64_t>(value),
                               static_cast<uint64_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
      } break;

      default:
        goto do_unknown_instruction;
    }
  }
    do_dispatch();

  do_unknown_instruction:
    handle_trap(exception_code_t::e_illegal_instruction, _inst);
    do_dispatch();
  }

  // memory
  const size_t _ram_size;
  uint8_t     *_data;
  uint64_t     _offset{};
  uint8_t     *_final{};

  const std::vector<mmio_handler_t> _mmios;
  mmio_handler_t                    _mru_mmio = {};

  bool _wfi = false;
  typedef void (*wfi_callback_t)();
  wfi_callback_t _wfi_callback = 0;

  uint64_t _reg[32] = {0};
  uint64_t _pc{0};
  uint64_t _mode{0b11};
  uint64_t _reservation_address;
  bool     _is_reserved = false;

#ifdef DAWN_ENABLE_LOGGING
  std::ofstream _log{"/tmp/dawn", std::ios::trunc};
#endif

  // hack
  std::map<uint64_t, std::function<void(machine_t &)>> _syscalls;

  // TODO: optimise csr
  std::map<uint16_t, uint64_t> _csr;
};

}  // namespace dawn
#endif
