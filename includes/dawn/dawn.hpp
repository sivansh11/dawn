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
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace dawn {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
static_assert(false "dawn can only work on little endian architecture");
#endif

#ifndef DAWN_RISCV64
using register_t  = uint32_t;
using sregister_t = int32_t;
#else
using register_t  = uint64_t;
using sregister_t = int64_t;
#endif

struct mmio_handler_t;

typedef register_t (*mmio_load_callback)(const mmio_handler_t *handler,
                                         register_t);
typedef void (*mmio_store_callback)(const mmio_handler_t *handler, register_t,
                                    register_t);

struct mmio_handler_t {
  register_t          start;
  register_t          stop;
  mmio_load_callback  load;
  mmio_store_callback store;
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
constexpr inline sregister_t sext(uint32_t val) {
  struct internal_t {
    sregister_t val : sign_bit;
  } s;
  return s.val = val;
}

enum class exception_code_t : register_t {
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

  e_machine_software_interrupt = 3 | (1ull << (sizeof(register_t) * 8 - 1)),
  e_machine_timer_interrupt    = 7 | (1ull << (sizeof(register_t) * 8 - 1)),
  e_machine_external_interrupt = 11 | (1ull << (sizeof(register_t) * 8 - 1)),
};

inline std::ostream &operator<<(std::ostream &o, exception_code_t cause) {
  switch (cause) {
    case exception_code_t::e_instruction_address_misaligned:
      o << "instruction_address_misaligned";
      break;
    case exception_code_t::e_instruction_access_fault:
      o << "instruction_access_fault";
      break;
    case exception_code_t::e_illegal_instruction:
      o << "illegal_instruction";
      break;
    case exception_code_t::e_breakpoint:
      o << "breakpoint";
      break;
    case exception_code_t::e_load_address_misaligned:
      o << "load_address_misaligned";
      break;
    case exception_code_t::e_load_access_fault:
      o << "load_access_fault";
      break;
    case exception_code_t::e_store_address_misaligned:
      o << "store_address_misaligned";
      break;
    case exception_code_t::e_store_access_fault:
      o << "store_access_fault";
      break;
    case exception_code_t::e_ecall_u_mode:
      o << "ecall_u_mode";
      break;
    case exception_code_t::e_ecall_s_mode:
      o << "ecall_s_mode";
      break;
    case exception_code_t::e_ecall_m_mode:
      o << "ecall_m_mode";
      break;
    case exception_code_t::e_machine_software_interrupt:
      o << "machine_software_interrupt";
      break;
    case exception_code_t::e_machine_timer_interrupt:
      o << "machine_timer_interrupt";
      break;
    case exception_code_t::e_machine_external_interrupt:
      o << "machine_external_interrupt";
      break;
    default:
      throw std::runtime_error("unknown cause");
  }
  return o;
}

constexpr register_t MHARDID = 0xf14;

constexpr register_t MNSTATUS = 0x744;

constexpr register_t MEDELEG = 0x302;
constexpr register_t MIDELEG = 0x303;
constexpr register_t MIE     = 0x304;

constexpr register_t MIP = 0x344;
constexpr register_t MIP_MEIP_MASK =
    1u << (static_cast<register_t>(
               exception_code_t::e_machine_external_interrupt) &
           ((1ull << (sizeof(register_t) * 8 - 1)) - 1));
constexpr register_t MIP_MSIP_MASK =
    1u << (static_cast<register_t>(
               exception_code_t::e_machine_software_interrupt) &
           ((1ull << (sizeof(register_t) * 8 - 1)) - 1));
constexpr register_t MIP_MTIP_MASK =
    1u << (static_cast<register_t>(
               exception_code_t::e_machine_timer_interrupt) &
           ((1ull << (sizeof(register_t) * 8 - 1)) - 1));

constexpr register_t MSTATUS            = 0x300;
constexpr register_t MSTATUS_MIE_SHIFT  = 3;
constexpr register_t MSTATUS_MIE_MASK   = 1u << MSTATUS_MIE_SHIFT;
constexpr register_t MSTATUS_MPIE_SHIFT = 7;
constexpr register_t MSTATUS_MPIE_MASK  = 1u << MSTATUS_MPIE_SHIFT;
constexpr register_t MSTATUS_MPP_SHIFT  = 11;
constexpr register_t MSTATUS_MPP_MASK   = 0b11u << MSTATUS_MPP_SHIFT;

constexpr register_t MTVEC                 = 0x305;
constexpr register_t MTVEC_MODE_MASK       = 0b11;
constexpr register_t MTVEC_BASE_ALIGN_MASK = ~0b11;

constexpr register_t MEPC = 0x341;

constexpr register_t MCAUSE = 0x342;

constexpr register_t MCAUSE_INTERRUPT_BIT =
    (1ull << (sizeof(register_t) * 8 - 1));

constexpr register_t MTVAL = 0x343;

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
  constexpr sregister_t imm_sext() const { return sext<21>(imm()); }
                        operator uint64_t() const {
    return *reinterpret_cast<const uint32_t *>(this);
  }
};

// TODO: add raw access
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

// TODO: figure out is this is required for 32 bits
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

#define do_trap(__cause, __value) \
  do {                            \
    trap_cause = __cause;         \
    trap_value = __value;         \
    goto _do_trap;                \
  } while (false)

static const register_t invalid_page_number =
    std::numeric_limits<register_t>::max();

enum page_metadata_t : register_t {
  e_none = 0,
  e_r    = static_cast<register_t>(1) << (sizeof(register_t) * 8 - 1),
  e_w    = static_cast<register_t>(1) << (sizeof(register_t) * 8 - 2),
  e_x    = static_cast<register_t>(1) << (sizeof(register_t) * 8 - 3),
  e_rw   = e_r | e_w,
  e_rx   = e_r | e_x,
  // TODO: verify if I need to add e_m here as well, and any other meta data I
  // might add in future
  e_m    = static_cast<register_t>(1) << (sizeof(register_t) * 8 - 4),
  e_rwm  = e_rw | e_m,
  e_rwx  = e_r | e_w | e_x,
  e_mask = e_r | e_w | e_x | e_m,
};

inline page_metadata_t operator|(page_metadata_t l, page_metadata_t r) {
  return (page_metadata_t)((uint64_t)l | (uint64_t)r);
}
inline page_metadata_t &operator|=(page_metadata_t &l, page_metadata_t r) {
  l = l | r;
  return l;
}

struct mmio_page_data_t {
  mmio_handler_t              mru_mmio = {};
  std::vector<mmio_handler_t> mmios{};
};

register_t mmio_page_data_load(mmio_page_data_t &mmio_page_data,
                               register_t        addr) {
  // TODO: add mru
  for (const auto &mmio : mmio_page_data.mmios) {
    if (mmio.start <= addr && addr < mmio.stop) {
      mmio_page_data.mru_mmio = mmio;
      return mmio.load(&mmio, addr);
    }
  }
  throw std::runtime_error("reached unreachable");
}

void mmio_page_data_store(mmio_page_data_t &mmio_page_data, register_t addr,
                          register_t value) {
  if (mmio_page_data.mru_mmio.start <= addr &&
      addr < mmio_page_data.mru_mmio.stop) {
    mmio_page_data.mru_mmio.store(&mmio_page_data.mru_mmio, addr, value);
    return;
  }
  for (const auto &mmio : mmio_page_data.mmios) {
    if (mmio.start <= addr && addr < mmio.stop) {
      mmio_page_data.mru_mmio = mmio;
      mmio.store(&mmio, addr, value);
      return;
    }
  }
  throw std::runtime_error("reached unreachable");
}

struct page_t {
  void *ptr = nullptr;
  // TODO: rename this to something more sensible
  // it is actually the page number + meta data
  register_t page_number = invalid_page_number;

  inline register_t number() const {
    return page_number & ~page_metadata_t::e_mask;
  }
  inline bool has_perms(const page_metadata_t metadata) const {
    return page_number & metadata;
  }
};

struct memory_t {
  // TODO: make this configurable
  static const register_t bits_per_page = 12;
  static_assert(bits_per_page <= sizeof(register_t) * 8 - 4,
                "bits_per_page needs enough space for metadata handling");
  static const register_t bytes_per_page = 1 << bits_per_page;
  // TODO: make this configurable
  static const register_t direct_cache_size = 32;
  const register_t        memory_limit_bytes;
  uint8_t *(*allocate_callback)(void *, uint64_t);
  void (*deallocate_callback)(void *, uint8_t *);
  void           *allocator_state;
  page_metadata_t default_page_metadata;

  constexpr register_t page_number(register_t addr) const {
    return addr >> bits_per_page;
  }
  constexpr register_t page_offset(register_t addr) const {
    return addr & (bytes_per_page - 1);
  }
  constexpr register_t cache_index(register_t page_number) const {
    return page_number % direct_cache_size;
  }
  constexpr uint8_t *allocate_frame() {
    if (allocated_bytes + bytes_per_page > memory_limit_bytes) return nullptr;
    uint8_t *frame = allocate_callback(allocator_state, bytes_per_page);
    if (frame) allocated_bytes += bytes_per_page;
    return frame;
  }
  constexpr page_t create_page(register_t page_number, uint8_t *ptr,
                               page_metadata_t metadata) {
    page_t new_page{.ptr = ptr, .page_number = page_number};
    new_page.page_number |= metadata;
    return new_page;
  }
  constexpr page_t allocate_page(register_t      page_number,
                                 page_metadata_t metadata) {
    uint8_t *new_frame = allocate_frame();
    if (!new_frame) [[unlikely]] {
      return page_t{};
    }
    return create_page(page_number, new_frame, metadata);
  }
  constexpr void invalidate_caches() {
    mru_page = fetch_mru_page = page_t{};
    for (uint32_t i = 0; i < direct_cache_size; i++) {
      direct_cache[i] = fetch_direct_cache[i] = page_t{};
    }
  }

  memory_t(register_t memory_limit_bytes, void *allocator_state,
           uint8_t *(*allocate_callback)(void *, uint64_t),
           void (*deallocate_callback)(void *, uint8_t *),
           page_metadata_t default_page_metadata)
      : memory_limit_bytes(memory_limit_bytes),
        allocator_state(allocator_state),
        allocate_callback(allocate_callback),
        deallocate_callback(deallocate_callback),
        default_page_metadata(default_page_metadata) {}

  register_t allocated_bytes                       = 0;
  page_t     mru_page                              = {};
  page_t     direct_cache[direct_cache_size]       = {};
  page_t     fetch_mru_page                        = {};
  page_t     fetch_direct_cache[direct_cache_size] = {};
  // page_number -> page
  // NOTE: the page_number inside page has metadata
  // TODO: try some faster map implementations
  std::unordered_map<register_t, page_t> page_table;
};

page_t slow_get_page_fetch(memory_t &memory, register_t addr) {
  register_t page_number = memory.page_number(addr);
  register_t cache_index = memory.cache_index(page_number);
  page_t     page{};
  if (memory.fetch_direct_cache[cache_index].number() == page_number)
      [[likely]] {
    if (memory.fetch_direct_cache[cache_index].has_perms(page_metadata_t::e_x))
        [[likely]] {
      memory.fetch_mru_page = memory.fetch_direct_cache[cache_index];
      page                  = memory.fetch_mru_page;
    } else {
      page = {};
    }
    return page;
  }
  auto itr = memory.page_table.find(page_number);
  if (itr != memory.page_table.end()) [[likely]] {
    if (itr->second.has_perms(page_metadata_t::e_x)) [[likely]] {
      memory.fetch_mru_page                  = itr->second;
      memory.fetch_direct_cache[cache_index] = itr->second;
      page                                   = memory.fetch_mru_page;
    } else {
      page = {};
    }
    return page;
  } /* allocate new page */
  page_t new_page =
      memory.allocate_page(page_number, memory.default_page_metadata);
  if (!new_page.ptr) [[unlikely]] {
    page = {};
    return page;
  }
  memory.page_table[page_number]         = new_page;
  memory.fetch_direct_cache[cache_index] = new_page;
  memory.fetch_mru_page                  = new_page;
  if (new_page.has_perms(page_metadata_t::e_x)) [[likely]]
    page = memory.fetch_mru_page;
  else
    page = {};
  return page;
}

#define __get_page_fetch(__memory, __addr, __page)                       \
  do {                                                                   \
    register_t __page_number = __memory.page_number(__addr);             \
    /* mru */                                                            \
    if (__memory.fetch_mru_page.number() == __page_number) [[likely]] {  \
      /* Note: saving cycles, maybe dont hide ? */                       \
      assert(__memory.fetch_mru_page.has_perms(page_metadata_t::e_x));   \
      /* if (__memory.fetch_mru_page.has_perms(page_metadata_t::e_x)) */ \
      /*[[likely]]*/                                                     \
      __page = __memory.fetch_mru_page;                                  \
      /*else*/                                                           \
      /*__page = {};*/                                                   \
      break;                                                             \
    }                                                                    \
    register_t __cache_index = __memory.cache_index(__page_number);      \
    /* cache */                                                          \
    /* page_table */                                                     \
    __page = slow_get_page_fetch(__memory, __addr);                      \
  } while (false)

page_t slow_get_page(memory_t &memory, page_metadata_t metadata,
                     register_t addr) {
  register_t page_number = memory.page_number(addr);
  register_t cache_index = memory.cache_index(page_number);
  page_t     page{};
  if (memory.direct_cache[cache_index].number() == page_number) [[likely]] {
    if (memory.direct_cache[cache_index].has_perms(metadata)) [[likely]] {
      memory.mru_page = memory.direct_cache[cache_index];
      page            = memory.mru_page;
    } else {
      page = {};
    }
    return page;
  }
  auto itr = memory.page_table.find(page_number);
  if (itr != memory.page_table.end()) [[likely]] {
    if (itr->second.has_perms(metadata)) [[likely]] {
      memory.direct_cache[cache_index] = itr->second;
      memory.mru_page                  = itr->second;
      page                             = memory.mru_page;
    } else {
      page = {};
    }
    return page;
  } /* allocate new page */
  page_t new_page =
      memory.allocate_page(page_number, memory.default_page_metadata);
  if (!new_page.ptr) [[unlikely]] {
    page = {};
    return page;
  }
  if (new_page.has_perms(metadata)) [[likely]] {
    memory.page_table[page_number]   = new_page;
    memory.direct_cache[cache_index] = new_page;
    memory.mru_page                  = new_page;
    page                             = memory.mru_page;
  } else {
    page = {};
  }
  return page;
}
#define __get_page(__memory, __metadata, __addr, __page)            \
  do {                                                              \
    register_t __page_number = __memory.page_number(__addr);        \
    /* mru */                                                       \
    if (__memory.mru_page.number() == __page_number) [[likely]] {   \
      if (__memory.mru_page.has_perms(__metadata)) [[likely]]       \
        __page = __memory.mru_page;                                 \
      else                                                          \
        __page = {};                                                \
      break;                                                        \
    }                                                               \
    register_t __cache_index = __memory.cache_index(__page_number); \
    /* cache */                                                     \
    /* page_table */                                                \
    __page = slow_get_page(__memory, __metadata, __addr);           \
  } while (false)

// TODO: maybe make all load/store/fetch straddling into 1 function ?
template <typename type>
std::pair<bool, type> load_straddling(memory_t &memory, register_t addr) {
  type                 value        = 0;
  uint8_t             *value_ptr    = reinterpret_cast<uint8_t *>(&value);
  constexpr register_t type_size    = sizeof(type);
  register_t           remaining    = type_size;
  register_t           current_addr = addr;
  while (remaining > 0) {
    page_t page;
    __get_page(memory, page_metadata_t::e_r, current_addr, page);
    if (page.ptr) return {false, value};
    register_t current_offset = memory.page_offset(current_addr);
    register_t chunk_size     = memory.bytes_per_page - current_offset;
    if (chunk_size > remaining) chunk_size = remaining;
    std::memcpy(value_ptr + (type_size - remaining),
                static_cast<uint8_t *>(page.ptr) + current_offset, chunk_size);
    current_addr += chunk_size;
    remaining -= chunk_size;
  }
  return {true, value};
}

#define __load(__type, __memory, __addr, __value)                             \
  do {                                                                        \
    constexpr register_t __type_size   = sizeof(__type);                      \
    register_t           __page_number = __memory.page_number(__addr);        \
    register_t           __offset      = __memory.page_offset(__addr);        \
    page_t               __page;                                              \
    uint8_t             *__value_ptr = reinterpret_cast<uint8_t *>(&__value); \
    __get_page(__memory, page_metadata_t::e_r, __addr, __page);               \
    if (!__page.ptr) do_trap(exception_code_t::e_load_access_fault, __addr);  \
    if (__page.has_perms(page_metadata_t::e_m)) [[unlikely]] {                \
      mmio_page_data_t *mmio_page_data =                                      \
          reinterpret_cast<mmio_page_data_t *>(__page.ptr);                   \
      assert(__type_size == sizeof(register_t));                              \
      __value = mmio_page_data_load(*mmio_page_data, __addr);                 \
    } else if (__offset + __type_size > __memory.bytes_per_page)              \
        [[unlikely]] {                                                        \
      /* straddling access */                                                 \
      auto __straddling_result = load_straddling<__type>(__memory, __addr);   \
      if (!__straddling_result.first) {                                       \
        do_trap(exception_code_t::e_load_access_fault, __addr);               \
      }                                                                       \
      __value = __straddling_result.second;                                   \
    } else {                                                                  \
      /* single page access */                                                \
      std::memcpy(__value_ptr, static_cast<uint8_t *>(__page.ptr) + __offset, \
                  __type_size);                                               \
    }                                                                         \
  } while (false)

template <typename type>
std::pair<bool, type> fetch_straddling(memory_t &memory, register_t addr) {
  type                 value        = 0;
  uint8_t             *value_ptr    = reinterpret_cast<uint8_t *>(&value);
  constexpr register_t type_size    = sizeof(type);
  register_t           remaining    = type_size;
  register_t           current_addr = addr;
  while (remaining > 0) {
    page_t page;
    __get_page_fetch(memory, current_addr, page);
    if (page.ptr) return {false, value};
    register_t current_offset = memory.page_offset(current_addr);
    register_t chunk_size     = memory.bytes_per_page - current_offset;
    if (chunk_size > remaining) chunk_size = remaining;
    std::memcpy(value_ptr + (type_size - remaining),
                static_cast<uint8_t *>(page.ptr) + current_offset, chunk_size);
    current_addr += chunk_size;
    remaining -= chunk_size;
  }
  return {true, value};
}

#define __fetch(__type, __memory, __addr, __value)                            \
  do {                                                                        \
    constexpr register_t __type_size   = sizeof(__type);                      \
    register_t           __page_number = __memory.page_number(__addr);        \
    register_t           __offset      = __memory.page_offset(__addr);        \
    page_t               __page;                                              \
    uint8_t             *__value_ptr = reinterpret_cast<uint8_t *>(&__value); \
    __get_page_fetch(__memory, __addr, __page);                               \
    if (!__page.ptr)                                                          \
      do_trap(exception_code_t::e_instruction_access_fault, __addr);          \
    /* Note: ignoring fetch for mmio to be more performant */                 \
    /* TODO: maybe dont ignore ? and do a guest trap ? */                     \
    assert(!__page.has_perms(page_metadata_t::e_m));                          \
    /* if (__page.has_perms(page_metadata_t::e_m)) [[unlikely]] {             \
      mmio_page_data_t *mmio_page_data =                                      \
          reinterpret_cast<mmio_page_data_t *>(__page.ptr);                   \
      assert(__type_size == 8);                                               \
      __value = static_cast<__type>(                                          \
          mmio_page_data_load64(*mmio_page_data, __addr));                    \
    } else */                                                                 \
    /* TODO: test without straddling accesses, I dont think this is possible  \
     */                                                                       \
    if (__offset + __type_size > __memory.bytes_per_page) [[unlikely]] {      \
      /* straddling access */                                                 \
      auto __straddling_result = fetch_straddling<__type>(__memory, __addr);  \
      if (!__straddling_result.first) {                                       \
        do_trap(exception_code_t::e_instruction_access_fault, __addr);        \
      }                                                                       \
      __value = __straddling_result.second;                                   \
    } else {                                                                  \
      /* single page access */                                                \
      std::memcpy(__value_ptr, static_cast<uint8_t *>(__page.ptr) + __offset, \
                  __type_size);                                               \
    }                                                                         \
  } while (false)

template <typename type>
bool store_straddling(memory_t &memory, register_t addr, register_t value) {
  constexpr register_t type_size   = sizeof(type);
  register_t           page_number = memory.page_number(addr);
  register_t           offset      = memory.page_offset(addr);
  page_t               page;
  const type           _value    = value;
  const uint8_t       *value_ptr = reinterpret_cast<const uint8_t *>(&_value);
  {
    register_t probe_addr    = addr;
    register_t bytes_checked = 0;
    while (bytes_checked < type_size) {
      __get_page(memory, page_metadata_t::e_w, probe_addr, page);
      if (!page.ptr) [[unlikely]]
        return false;
      register_t offset = memory.page_offset(probe_addr);
      register_t chunk  = memory.bytes_per_page - offset;
      bytes_checked += chunk;
      probe_addr += chunk;
    }
  }
  register_t remaining    = type_size;
  register_t current_addr = addr;
  while (remaining > 0) {
    __get_page(memory, page_metadata_t::e_w, current_addr, page);
    if (!page.ptr) return false;
    register_t current_offset = memory.page_offset(current_addr);
    register_t chunk_size     = memory.bytes_per_page - current_offset;
    if (chunk_size > remaining) chunk_size = remaining;
    std::memcpy(static_cast<uint8_t *>(page.ptr) + current_offset,
                value_ptr + (type_size - remaining), chunk_size);
    current_addr += chunk_size;
    remaining -= chunk_size;
  }
  return true;
}

#define __store(__type, __memory, __addr, __value)                            \
  do {                                                                        \
    constexpr register_t __type_size   = sizeof(__type);                      \
    register_t           __page_number = __memory.page_number(__addr);        \
    register_t           __offset      = __memory.page_offset(__addr);        \
    page_t               __page;                                              \
    const __type         _value = __value;                                    \
    const uint8_t *__value_ptr  = reinterpret_cast<const uint8_t *>(&_value); \
    __get_page(__memory, page_metadata_t::e_w, __addr, __page);               \
    if (!__page.ptr) do_trap(exception_code_t::e_store_access_fault, __addr); \
    if (__page.has_perms(page_metadata_t::e_m)) [[unlikely]] {                \
      mmio_page_data_t *mmio_page_data =                                      \
          reinterpret_cast<mmio_page_data_t *>(__page.ptr);                   \
      assert(__type_size == sizeof(register_t));                              \
      mmio_page_data_store(*mmio_page_data, __addr, __value);                 \
    } else if (__offset + __type_size > __memory.bytes_per_page)              \
        [[unlikely]] {                                                        \
      /* straddling access */                                                 \
      auto __should_trap =                                                    \
          store_straddling<__type>(__memory, __addr, __value);                \
      if (!__should_trap) {                                                   \
        do_trap(exception_code_t::e_store_access_fault, __addr);              \
      }                                                                       \
    } else {                                                                  \
      /* single page access */                                                \
      std::memcpy(static_cast<uint8_t *>(__page.ptr) + __offset, __value_ptr, \
                  __type_size);                                               \
    }                                                                         \
  } while (false)

// TODO: flip value and addr locations in macro
#define __load8(__memory, __value, __addr) \
  __load(uint8_t, __memory, __addr, __value)
#define __load16(__memory, __value, __addr) \
  __load(uint16_t, __memory, __addr, __value)
#define __load32(__memory, __value, __addr) \
  __load(uint32_t, __memory, __addr, __value)
#ifdef DAWN_RISCV64
#define __load64(__memory, __value, __addr) \
  __load(uint64_t, __memory, __addr, __value)
#endif
#define __load8i(__memory, __value, __addr) \
  __load(int8_t, __memory, __addr, __value)
#define __load16i(__memory, __value, __addr) \
  __load(int16_t, __memory, __addr, __value)
#define __load32i(__memory, __value, __addr) \
  __load(int32_t, __memory, __addr, __value)

#if 0
#define __fetch16(__memory, __value, __addr) \
  __fetch(uint16_t, __memory, __addr, __value)
#endif
#define __fetch32(__memory, __value, __addr) \
  __fetch(uint32_t, __memory, __addr, __value)

#define __store8(__memory, __addr, __value) \
  __store(uint8_t, __memory, __addr, __value)
#define __store16(__memory, __addr, __value) \
  __store(uint16_t, __memory, __addr, __value)
#define __store32(__memory, __addr, __value) \
  __store(uint32_t, __memory, __addr, __value)
#ifdef DAWN_RISCV64
#define __store64(__memory, __addr, __value) \
  __store(uint64_t, __memory, __addr, __value)
#endif

typedef void (*trap_callback_t)(void *, exception_code_t cause,
                                register_t value);

// TODO: accurate runtime memory bounds checking (account for size of
// load/store)
struct machine_t {
  machine_t(size_t ram_size, const std::vector<mmio_handler_t> mmios,
            void *allocator_state,
            uint8_t *(*allocate_callback)(void *, uint64_t),
            void (*deallocate_callback)(void *, uint8_t *),
            page_metadata_t default_page_metadata)
      : _memory(ram_size, allocator_state, allocate_callback,
                deallocate_callback, default_page_metadata),
        _mmios(mmios) {
    for (const auto &mmio : mmios) {
      register_t start = _memory.page_number(mmio.start);
      register_t stop  = _memory.page_number(mmio.stop - 1);

      page_t page;
      for (; start <= stop; start++) {
        auto itr = _memory.page_table.find(start);
        if (itr == _memory.page_table.end()) {
          page.page_number                 = start | page_metadata_t::e_rwm;
          mmio_page_data_t &mmio_page_data = _mmio_page_data.emplace_back();
          page.ptr                         = &mmio_page_data;
          _memory.page_table[start]        = page;
        } else {
          page = itr->second;
        }
        mmio_page_data_t &mmio_page_data =
            *reinterpret_cast<mmio_page_data_t *>(page.ptr);
        mmio_page_data.mmios.push_back(mmio);
      }
    }
  }
  ~machine_t() {}

  // TODO: a more involved csr read
  inline register_t read_csr(uint16_t csrno) { return _csr[csrno]; }
  // TODO: a more involved csr write
  inline void write_csr(uint16_t csrno, register_t value) {
    _csr[csrno] = value;
  }

  inline bool memcpy_host_to_guest(register_t dst_addr, const void *src_ptr,
                                   size_t size) {
    register_t     remaining    = size;
    register_t     current_addr = dst_addr;
    const uint8_t *src          = reinterpret_cast<const uint8_t *>(src_ptr);
    while (remaining > 0) {
      page_t page;
      __get_page(_memory, page_metadata_t::e_mask, current_addr, page);
      if (!page.ptr) return false;
      if (page.has_perms(page_metadata_t::e_m))
        throw std::runtime_error("cannot memcpy_host_to_guest mmio");
      register_t offset     = _memory.page_offset(current_addr);
      register_t chunk_size = _memory.bytes_per_page - offset;
      if (chunk_size > remaining) chunk_size = remaining;
      std::memcpy(static_cast<uint8_t *>(page.ptr) + offset,
                  src + (size - remaining), chunk_size);
      current_addr += chunk_size;
      remaining -= chunk_size;
    }
    return true;
  }
  inline bool memcpy_guest_to_host(void *dst_ptr, register_t src_addr,
                                   size_t size) {
    register_t remaining    = size;
    register_t current_addr = src_addr;
    uint8_t   *dst          = reinterpret_cast<uint8_t *>(dst_ptr);
    while (remaining > 0) {
      page_t page;
      __get_page(_memory, page_metadata_t::e_mask, current_addr, page);
      if (!page.ptr) return false;
      if (page.has_perms(page_metadata_t::e_m))
        throw std::runtime_error("cannot memcpy_guest_to_host mmio");
      register_t offset     = _memory.page_offset(current_addr);
      register_t chunk_size = _memory.bytes_per_page - offset;
      if (chunk_size > remaining) chunk_size = remaining;
      std::memcpy(dst + (size - remaining),
                  static_cast<uint8_t *>(page.ptr) + offset, chunk_size);
      current_addr += chunk_size;
      remaining -= chunk_size;
    }
    return true;
  }
  inline bool memset(register_t addr, int value, size_t size) {
    register_t remaining    = size;
    register_t current_addr = addr;
    while (remaining > 0) {
      page_t page;
      __get_page(_memory, page_metadata_t::e_w, current_addr, page);
      if (!page.ptr) return false;
      if (page.has_perms(page_metadata_t::e_m))
        throw std::runtime_error("cannot memset mmio");
      register_t offset     = _memory.page_offset(current_addr);
      register_t chunk_size = _memory.bytes_per_page - offset;
      if (chunk_size > remaining) chunk_size = remaining;
      std::memset(static_cast<uint8_t *>(page.ptr) + offset, value, chunk_size);
      current_addr += chunk_size;
      remaining -= chunk_size;
    }
    return true;
  }
  inline bool insert_memory(register_t dst_addr, const void *src_ptr,
                            register_t size, page_metadata_t metadata) {
    if (metadata & page_metadata_t::e_m)
      throw std::runtime_error("mmio should not be a part of insert_memory");
    register_t     remaining    = size;
    register_t     current_addr = dst_addr;
    const uint8_t *src          = reinterpret_cast<const uint8_t *>(src_ptr);
    while (remaining > 0) {
      register_t page_number = _memory.page_number(current_addr);
      auto       itr         = _memory.page_table.find(page_number);
      page_t     page;
      if (itr == _memory.page_table.end()) {
        page_t new_page = _memory.allocate_page(page_number, metadata);
        if (!new_page.ptr) return false;
        _memory.page_table[page_number] = new_page;
        page                            = new_page;
      } else {
        itr->second.page_number =
            itr->second.page_number & ~page_metadata_t::e_mask;
        itr->second.page_number = itr->second.page_number | metadata;
        page                    = itr->second;
      }
      assert(page.ptr);
      register_t offset     = _memory.page_offset(current_addr);
      register_t chunk_size = _memory.bytes_per_page - offset;
      if (chunk_size > remaining) chunk_size = remaining;
      std::memcpy(static_cast<uint8_t *>(page.ptr) + offset,
                  src + (size - remaining), chunk_size);
      current_addr += chunk_size;
      remaining -= chunk_size;
    }
    // invalidate everything
    _memory.invalidate_caches();
    return true;
  }
  inline bool set_memory(register_t dst_addr, int value, uint64_t size,
                         page_metadata_t metadata) {
    if (metadata & page_metadata_t::e_m)
      throw std::runtime_error("mmio should not be a part of set_memory");
    register_t remaining    = size;
    register_t current_addr = dst_addr;
    while (remaining > 0) {
      register_t page_number = _memory.page_number(current_addr);
      auto       itr         = _memory.page_table.find(page_number);
      page_t     page;
      if (itr == _memory.page_table.end()) {
        page_t new_page = _memory.allocate_page(page_number, metadata);
        if (!new_page.ptr) return false;
        _memory.page_table[page_number] = new_page;
        page                            = new_page;
      } else {
        itr->second.page_number =
            itr->second.page_number & ~page_metadata_t::e_mask;
        itr->second.page_number = itr->second.page_number | metadata;
        page                    = itr->second;
      }
      assert(page.ptr);
      register_t offset     = _memory.page_offset(current_addr);
      register_t chunk_size = _memory.bytes_per_page - offset;
      if (chunk_size > remaining) chunk_size = remaining;
      std::memset(static_cast<uint8_t *>(page.ptr) + offset, value, chunk_size);
      current_addr += chunk_size;
      remaining -= chunk_size;
    }
    // invalidate everything
    _memory.mru_page = _memory.fetch_mru_page = page_t{};
    for (uint32_t i = 0; i < _memory.direct_cache_size; i++) {
      _memory.direct_cache[i] = _memory.fetch_direct_cache[i] = page_t{};
    }
    return true;
  }

  // TODO: test with and without inline
  // TODO: test with a macro
  inline bool handle_trap(exception_code_t cause, register_t value) {
    // hack
    if (_trap_callback) {
      _trap_callback(_trap_usr_data, cause, value);
      return true;
    }

    bool is_interrupt =
        (static_cast<register_t>(cause) & MCAUSE_INTERRUPT_BIT) != 0;
    register_t &mstatus    = _csr[MSTATUS];
    bool        global_mie = (mstatus & MSTATUS_MIE_MASK) != 0;

    _csr[MEPC]   = _pc;
    _csr[MCAUSE] = static_cast<register_t>(cause);
    _csr[MTVAL]  = value;

    mstatus = (mstatus & ~MSTATUS_MPP_MASK) |
              ((static_cast<register_t>(_mode) << MSTATUS_MPP_SHIFT) &
               MSTATUS_MPP_MASK);
    mstatus = (mstatus & ~MSTATUS_MPIE_MASK) |
              ((global_mie << MSTATUS_MPIE_SHIFT) & MSTATUS_MPIE_MASK);
    mstatus &= ~MSTATUS_MIE_MASK;

    register_t mtvec      = _csr[MTVEC];
    register_t mtvec_base = mtvec & MTVEC_BASE_ALIGN_MASK;
    register_t mtvec_mode = mtvec & MTVEC_MODE_MASK;

    if (mtvec_mode == 0b01 && is_interrupt) {
      register_t interrupt_code =
          static_cast<register_t>(cause) & ~MCAUSE_INTERRUPT_BIT;
      _pc = mtvec_base + (interrupt_code * 4);
    } else {
      _pc = mtvec_base;
    }
    _mode        = 0b11;
    _is_reserved = false;

    // generally will only happen if mtvec is not set
    if (_pc == 0) [[unlikely]] {
      switch (cause) {
        default:
          std::stringstream ss;
          ss << "Error: " << static_cast<register_t>(cause) << '\n';
          ss << "at: " << std::hex << _pc << std::dec << '\n';
          throw std::runtime_error(ss.str());
      }
    }

    return true;
  }

  std::optional<uint8_t> at(register_t addr) {
    exception_code_t trap_cause;
    register_t       trap_value;
    uint8_t          value;
    __load8(_memory, value, addr);
    return value;
  _do_trap:
    return std::nullopt;
  }

  // TODO: remove this, unused
  bool insert_page(register_t page_number, uint8_t *ptr,
                   page_metadata_t metadata) {
    page_t new_page = _memory.create_page(page_number, ptr, metadata);
    _memory.page_table[page_number] = new_page;
    if (_memory.mru_page.number() == page_number) {
      _memory.mru_page = new_page;
    }
    if (_memory.fetch_mru_page.number() == page_number) {
      _memory.fetch_mru_page = new_page;
    }
    register_t cache_index = _memory.cache_index(page_number);
    if (_memory.direct_cache[cache_index].number() == page_number) {
      _memory.direct_cache[cache_index] = new_page;
    }
    if (_memory.fetch_direct_cache[cache_index].number() == page_number) {
      _memory.fetch_direct_cache[cache_index] = new_page;
    }
    return true;
  }

  // TODO: remove this, unused
  bool insert_new_page(register_t page_number, page_metadata_t metadata) {
    page_t new_page = _memory.allocate_page(page_number, metadata);
    _memory.page_table[page_number] = new_page;
    if (_memory.mru_page.number() == page_number) {
      _memory.mru_page = new_page;
    }
    if (_memory.fetch_mru_page.number() == page_number) {
      _memory.fetch_mru_page = new_page;
    }
    register_t cache_index = _memory.cache_index(page_number);
    if (_memory.direct_cache[cache_index].number() == page_number) {
      _memory.direct_cache[cache_index] = new_page;
    }
    if (_memory.fetch_direct_cache[cache_index].number() == page_number) {
      _memory.fetch_direct_cache[cache_index] = new_page;
    }
    return true;
  }

  struct cached_instruction_t {
    void    *label;
    uint32_t instruction;
    uint32_t tag_number;  // Note: should be 64 - 12 = 48 bits, but shouldnt be
                          // a problem ?
  };

  // TODO: all register accesses need to be converted into register_t
  inline uint64_t step(uint64_t n) {
    static void *dispatch_table[256] = {nullptr};

    // initialize
    static bool initialized = false;
    if (!initialized) [[unlikely]] {
      initialized = true;
      for (auto &entry : dispatch_table) entry = &&_do_unknown_instruction;

#define register_range(op, label)                                             \
  do {                                                                        \
    for (uint32_t i = 0; i < 8; i++) dispatch_table[op | (i << 5)] = &&label; \
  } while (false)
#define register_instr(op, func3, label)       \
  do {                                         \
    dispatch_table[op | func3 << 5] = &&label; \
  } while (false)

      register_range(0b01101, _do_lui);
      register_range(0b00101, _do_auipc);
      register_range(0b11011, _do_jal);
      register_range(0b11001, _do_jalr);
      register_instr(0b11000, 0b000, _do_beq);
      register_instr(0b11000, 0b001, _do_bne);
      register_instr(0b11000, 0b100, _do_blt);
      register_instr(0b11000, 0b101, _do_bge);
      register_instr(0b11000, 0b110, _do_bltu);
      register_instr(0b11000, 0b111, _do_bgeu);
      register_instr(0b00000, 0b000, _do_lb);
      register_instr(0b00000, 0b001, _do_lh);
      register_instr(0b00000, 0b010, _do_lw);
      register_instr(0b00000, 0b100, _do_lbu);
      register_instr(0b00000, 0b101, _do_lhu);
      register_instr(0b01000, 0b000, _do_sb);
      register_instr(0b01000, 0b001, _do_sh);
      register_instr(0b01000, 0b010, _do_sw);
      register_instr(0b00100, 0b000, _do_addi);
      register_instr(0b00100, 0b010, _do_slti);
      register_instr(0b00100, 0b011, _do_sltiu);
      register_instr(0b00100, 0b100, _do_xori);
      register_instr(0b00100, 0b110, _do_ori);
      register_instr(0b00100, 0b111, _do_andi);
#ifdef DAWN_RISCV64
      register_instr(0b00000, 0b110, _do_lwu);
      register_instr(0b00000, 0b011, _do_ld);
      register_instr(0b01000, 0b011, _do_sd);
#endif
      register_instr(0b00100, 0b001, _do_slli);
      register_instr(0b00100, 0b101, _do_srli_or_srai);
#ifdef DAWN_RISCV64
      register_instr(0b00110, 0b000, _do_addiw);
      register_instr(0b00110, 0b001, _do_slliw);
      register_instr(0b00110, 0b101, _do_srliw_or_sraiw);
      register_instr(0b01110, 0b000, _do_addw_or_subw_or_mulw);
      register_instr(0b01110, 0b001, _do_sllw);
      register_instr(0b01110, 0b100, _do_divw);
      register_instr(0b01110, 0b101, _do_srlw_or_sraw_or_divuw);
      register_instr(0b01110, 0b110, _do_remw);
      register_instr(0b01110, 0b111, _do_remuw);
#endif
      register_instr(0b01100, 0b000, _do_add_or_sub_or_mul);
      register_instr(0b01100, 0b001, _do_sll_or_mulh);
      register_instr(0b01100, 0b010, _do_slt_or_mulhsu);
      register_instr(0b01100, 0b011, _do_sltu_or_mulhu);
      register_instr(0b01100, 0b100, _do_xor_or_div);
      register_instr(0b01100, 0b101, _do_srl_or_sra_or_divu);
      register_instr(0b01100, 0b110, _do_or_or_rem);
      register_instr(0b01100, 0b111, _do_and_or_remu);
      register_instr(0b00011, 0b000, _do_fence);
      register_instr(0b00011, 0b001, _do_fence);
      register_instr(0b11100, 0b000, _do_system);  // ecall ebreak mret wfi
      register_instr(0b11100, 0b001, _do_csrrw);
      register_instr(0b11100, 0b010, _do_csrrs);
      register_instr(0b11100, 0b011, _do_csrrc);
      register_instr(0b11100, 0b101, _do_csrrwi);
      register_instr(0b11100, 0b110, _do_csrrsi);
      register_instr(0b11100, 0b111, _do_csrrci);
      register_instr(0b01011, 0b010, _do_atomic_w);
#ifdef DAWN_RISCV64
      register_instr(0b01011, 0b011, _do_atomic_d);
#endif
    }

    uint32_t           _inst;
    instruction_t      inst;
    constexpr uint64_t cached_instruction_mask = (1ull << 32) - 1;

    // f f f o o o o o (f is func3, o is op)
    // note, we ignore the first 2 bits of op since we dont implement compressed
    // instructions
#define dispatch()                                                           \
  do {                                                                       \
    if (_wfi) [[unlikely]]                                                   \
      return n;                                                              \
    _reg[0] = 0;                                                             \
    if (n-- == 0) [[unlikely]]                                               \
      return 0;                                                              \
    uint32_t cached_instruction_index =                                      \
        (_pc >> 2) & (_num_instruction_cache - 1);                           \
    if (_cached_instructions[cached_instruction_index].tag_number ==         \
        ((_pc >> _bits_per_instruction_cache) & cached_instruction_mask))    \
        [[likely]] {                                                         \
      _inst = _cached_instructions[cached_instruction_index].instruction;    \
      reinterpret_cast<uint32_t &>(inst) = _inst;                            \
      goto *_cached_instructions[cached_instruction_index].label;            \
    } else {                                                                 \
      __fetch32(_memory, _inst, _pc);                                        \
      const uint32_t dispatch_index = extract_bit_range(_inst, 2, 7) |       \
                                      extract_bit_range(_inst, 12, 15) << 5; \
      _cached_instructions[cached_instruction_index].instruction = _inst;    \
      _cached_instructions[cached_instruction_index].label =                 \
          dispatch_table[dispatch_index];                                    \
      _cached_instructions[cached_instruction_index].tag_number =            \
          (_pc >> _bits_per_instruction_cache) & cached_instruction_mask;    \
      reinterpret_cast<uint32_t &>(inst) = _inst;                            \
      goto *_cached_instructions[cached_instruction_index].label;            \
    }                                                                        \
    /* TODO: write a macro to disable and enable instruction cache           \
      __fetch32(_memory, _inst, _pc);                                        \
      const uint32_t dispatch_index = extract_bit_range(_inst, 2, 7) |       \
                                      extract_bit_range(_inst, 12, 15) << 5; \
      reinterpret_cast<uint32_t &>(inst) = _inst;                            \
      goto *dispatch_table[dispatch_index];                                  \
      */                                                                     \
  } while (false)

#ifdef DAWN_ENABLE_LOGGING
    auto logger = [&]() {
      _log << "pc: " << std::hex << _pc;
      for (uint32_t i = 0; i < 32; i++) {
        if (_reg[i] != 0) {
          _log << "    x" << std::dec << i << ": " << std::hex << _reg[i];
        }
      }
      _log << '\n';
      _log.flush();
    };

#define do_dispatch() \
  do {                \
    dispatch();       \
  } while (false)
#else
#define do_dispatch() dispatch()
#endif

    exception_code_t trap_cause;
    register_t       trap_value;

    // check pending interrupts
    register_t pending_interrupts = _csr[MIP] & _csr[MIE];
    if (pending_interrupts) {
      _wfi = false;
      if (_mode < 0b11 || _csr[MSTATUS] & MSTATUS_MIE_MASK) {
        if (pending_interrupts & MIP_MEIP_MASK) {
          do_trap(exception_code_t::e_machine_external_interrupt, 0);
        } else if (pending_interrupts & MIP_MSIP_MASK) {
          do_trap(exception_code_t::e_machine_software_interrupt, 0);
        } else if (pending_interrupts & MIP_MTIP_MASK) {
          do_trap(exception_code_t::e_machine_timer_interrupt, 0);
        }
        throw std::runtime_error("interrupt pending, but not handled");
      }
    }

    // no need to check every loop, checking once is enough since jump/branch
    // handle misaligned addresses
    if (_pc % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_instruction_address_misaligned, _pc);
    }

    do_dispatch();

  _do_lui: {
    _reg[inst.as.u_type.rd()] = static_cast<sregister_t>(
        static_cast<int32_t>(inst.as.u_type.imm() << 12));
    _pc += 4;
  }
    do_dispatch();

  _do_auipc: {
    _reg[inst.as.u_type.rd()] =
        _pc + static_cast<int32_t>(inst.as.u_type.imm() << 12);
    _pc += 4;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_jal: {
    register_t addr = _pc + inst.as.j_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_instruction_address_misaligned, addr);
    }
    _reg[inst.as.j_type.rd()] = _pc + 4;
    _pc                       = addr;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_jalr: {
    register_t target  = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    register_t next_pc = target & ~1ull;
    if (next_pc % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_instruction_address_misaligned, next_pc);
    }
    _reg[inst.as.i_type.rd()] = _pc + 4;
    _pc                       = next_pc;
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_beq: {
    if (_reg[inst.as.b_type.rs1()] == _reg[inst.as.b_type.rs2()]) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_bne: {
    if (_reg[inst.as.b_type.rs1()] != _reg[inst.as.b_type.rs2()]) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_blt: {
    if (static_cast<sregister_t>(_reg[inst.as.b_type.rs1()]) <
        static_cast<sregister_t>(_reg[inst.as.b_type.rs2()])) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_bge: {
    if (static_cast<sregister_t>(_reg[inst.as.b_type.rs1()]) >=
        static_cast<sregister_t>(_reg[inst.as.b_type.rs2()])) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_bltu: {
    if (_reg[inst.as.b_type.rs1()] < _reg[inst.as.b_type.rs2()]) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

    // TODO: verify pc is in memory bounds before do_dispatch
  _do_bgeu: {
    if (_reg[inst.as.b_type.rs1()] >= _reg[inst.as.b_type.rs2()]) {
      register_t addr = _pc + inst.as.b_type.imm_sext();
      if (addr % 4 != 0) [[unlikely]] {
        do_trap(exception_code_t::e_instruction_address_misaligned, addr);
      }
      _pc = addr;
    } else {
      _pc += 4;
    }
  }
    do_dispatch();

  _do_lb: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lb x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    int8_t   value;
    __load8i(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    // uint8_t *ptr;
    // __get_page_frame(_memory, addr, ptr);
    // if (!ptr) throw std::runtime_error("failed to get frame ptr");
    // uint64_t offset = _memory.page_offset(addr);
    // _log << std::hex << "addr: " << (void *)(ptr + offset);
    // _log << "\t";
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << int64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = static_cast<sregister_t>(value);
    _pc += 4;
  }
    do_dispatch();

  _do_lh: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lh x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 2 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_load_address_misaligned, addr);
    }
    int16_t value;
    __load16i(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << int64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = static_cast<sregister_t>(value);
    _pc += 4;
  }
    do_dispatch();

  _do_lw: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lw x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_load_address_misaligned, addr);
    }
    int32_t value;
    __load32i(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << int64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = static_cast<sregister_t>(value);
    _pc += 4;
  }
    do_dispatch();

  _do_lbu: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lbu x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    uint8_t  value;
    __load8(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << uint64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  _do_lhu: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lhu x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 2 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_load_address_misaligned, addr);
    }
    uint16_t value;
    __load16(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << uint64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

  _do_sb: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "sb x" << std::dec << inst.as.s_type.rs2() << ","
         << inst.as.s_type.imm_sext() << "(x" << inst.as.s_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
#ifdef DAWN_ENABLE_LOGGING
    _log << std::hex << addr << " <-- " << std::dec
         << _reg[inst.as.s_type.rs2()] << '\n';
#endif
    __store8(_memory, addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  _do_sh: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "sh x" << std::dec << inst.as.s_type.rs2() << ","
         << inst.as.s_type.imm_sext() << "(x" << inst.as.s_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
#ifdef DAWN_ENABLE_LOGGING
    _log << std::hex << addr << " <-- " << std::dec
         << _reg[inst.as.s_type.rs2()] << '\n';
#endif
    if (addr % 2 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_store_address_misaligned, addr);
    }
    __store16(_memory, addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  _do_sw: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "sw x" << std::dec << inst.as.s_type.rs2() << ","
         << inst.as.s_type.imm_sext() << "(x" << inst.as.s_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
#ifdef DAWN_ENABLE_LOGGING
    _log << std::hex << addr << " <-- " << std::dec
         << _reg[inst.as.s_type.rs2()] << '\n';
#endif
    if (addr % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_store_address_misaligned, addr);
    }
    __store32(_memory, addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();

  _do_addi: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_slti: {
    _reg[inst.as.i_type.rd()] =
        static_cast<sregister_t>(_reg[inst.as.i_type.rs1()]) <
        inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_sltiu: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] < inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_xori: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] ^ inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_ori: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] | inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_andi: {
    _reg[inst.as.i_type.rd()] =
        _reg[inst.as.i_type.rs1()] & inst.as.i_type.imm_sext();
    _pc += 4;
  }
    do_dispatch();

  _do_lwu: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "lwu x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 4 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_load_address_misaligned, addr);
    }
    uint32_t value;
    __load32(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << uint64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();

#ifdef DAWN_RISCV64
  _do_ld: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "ld x" << std::dec << inst.as.i_type.rd() << ","
         << inst.as.i_type.imm_sext() << "(x" << inst.as.i_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext();
    if (addr % 8 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_load_address_misaligned, addr);
    }
    uint64_t value;
    __load64(_memory, value, addr);  // may fault
#ifdef DAWN_ENABLE_LOGGING
    _log << "x" << std::dec << inst.as.i_type.rd() << " <-- " << uint64_t(value)
         << " <-- " << std::hex << addr << '\n';
#endif
    _reg[inst.as.i_type.rd()] = value;
    _pc += 4;
  }
    do_dispatch();
#endif

#ifdef DAWN_RISCV64
  _do_sd: {
#ifdef DAWN_ENABLE_LOGGING
    _log << "sw x" << std::dec << inst.as.s_type.rs2() << ","
         << inst.as.s_type.imm_sext() << "(x" << inst.as.s_type.rs1() << ")\n";
#endif
    uint64_t addr = _reg[inst.as.s_type.rs1()] + inst.as.s_type.imm_sext();
#ifdef DAWN_ENABLE_LOGGING
    _log << std::hex << addr << " <-- " << std::dec
         << _reg[inst.as.s_type.rs2()] << '\n';
#endif
    if (addr % 8 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_store_address_misaligned, addr);
    }
    __store64(_memory, addr, _reg[inst.as.s_type.rs2()]);  // may fault
    _pc += 4;
  }
    do_dispatch();
#endif

  _do_slli: {
    constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
    _reg[inst.as.i_type.rd()]     = _reg[inst.as.i_type.rs1()]
                                << (inst.as.i_type.imm() & shamt_mask);
    _pc += 4;
  }
    do_dispatch();

  _do_srli_or_srai: {
    switch (inst.as.i_type.imm() >> 6) {
      case 0b000000: {  // srli
        constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
        _reg[inst.as.i_type.rd()] =
            _reg[inst.as.i_type.rs1()] >> (inst.as.i_type.imm() & shamt_mask);
        _pc += 4;
        do_dispatch();
      } break;
      case 0b010000: {  // srai
        constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
        sregister_t        rs1_val =
            static_cast<sregister_t>(_reg[inst.as.i_type.rs1()]);
        uint32_t shamt            = inst.as.i_type.imm() & shamt_mask;
        _reg[inst.as.i_type.rd()] = rs1_val >> shamt;
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_addiw: {
    _reg[inst.as.i_type.rd()] = static_cast<int32_t>(static_cast<uint32_t>(
        _reg[inst.as.i_type.rs1()] + inst.as.i_type.imm_sext()));
    _pc += 4;
  }
    do_dispatch();

  _do_slliw: {
    _reg[inst.as.i_type.rd()] = static_cast<int32_t>(static_cast<uint32_t>(
        _reg[inst.as.i_type.rs1()]
        << static_cast<uint32_t>(inst.as.i_type.shamt_w())));
    _pc += 4;
  }
    do_dispatch();

  _do_srliw_or_sraiw: {
    switch (inst.as.i_type.imm() >> 5) {
      case 0b0000000: {  // srliw
        _reg[inst.as.i_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.i_type.rs1()]) >>
            inst.as.i_type.shamt_w());
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0100000: {  // sraiw
        _reg[inst.as.i_type.rd()] = static_cast<int32_t>(
            static_cast<int32_t>(_reg[inst.as.i_type.rs1()]) >>
            inst.as.i_type.shamt_w());
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_addw_or_subw_or_mulw: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // addw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) +
            static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]));
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0100000: {  // subw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            (static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) -
             static_cast<uint32_t>(_reg[inst.as.r_type.rs2()])));
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // mulw
        _reg[inst.as.r_type.rd()] =
            static_cast<sregister_t>(static_cast<int32_t>(
                static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) *
                static_cast<uint32_t>(_reg[inst.as.r_type.rs2()])));
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_sllw: {
    _reg[inst.as.r_type.rd()] = static_cast<sregister_t>(
        static_cast<int32_t>(static_cast<uint32_t>(_reg[inst.as.r_type.rs1()])
                             << (_reg[inst.as.r_type.rs2()] & 0b11111)));
    _pc += 4;
  }
    do_dispatch();

  _do_divw: {
    int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
    int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs1 == std::numeric_limits<sregister_t>::min() && rs2 == -1) {
      _reg[inst.as.r_type.rd()] = std::numeric_limits<sregister_t>::min();
    } else if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = ~register_t(0);
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = static_cast<register_t>(rs1 / rs2);
    }
    _reg[inst.as.r_type.rd()] = static_cast<sregister_t>(
        static_cast<int32_t>(_reg[inst.as.r_type.rd()]));
    _pc += 4;
  }
    do_dispatch();

  _do_srlw_or_sraw_or_divuw: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // srlw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & 0b11111));
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0100000: {  // sraw
        _reg[inst.as.r_type.rd()] = static_cast<int32_t>(
            static_cast<int32_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & 0b11111));
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // divuw
        uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
        uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
        if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~0u;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = rs1 / rs2;
        }
        _reg[inst.as.r_type.rd()] =
            static_cast<sregister_t>(static_cast<int32_t>(
                static_cast<uint32_t>(_reg[inst.as.r_type.rd()])));
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_remw: {
    int32_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
    int32_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs1 == std::numeric_limits<sregister_t>::min() && rs2 == -1) {
      _reg[inst.as.r_type.rd()] = 0;
    } else if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = rs1;
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = static_cast<register_t>(rs1 % rs2);
    }
    _reg[inst.as.r_type.rd()] = static_cast<sregister_t>(
        static_cast<int32_t>(_reg[inst.as.r_type.rd()]));
    _pc += 4;
  }
    do_dispatch();

  _do_remuw: {
    uint32_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
    uint32_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
    if (rs2 == 0) {
      _reg[inst.as.r_type.rd()] = rs1;
    } else [[likely]] {
      _reg[inst.as.r_type.rd()] = rs1 % rs2;
    }
    _reg[inst.as.r_type.rd()] = static_cast<sregister_t>(
        static_cast<int32_t>(static_cast<uint32_t>(_reg[inst.as.r_type.rd()])));
    _pc += 4;
  }
    do_dispatch();

  _do_add_or_sub_or_mul: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // add
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] + _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0100000: {  // sub
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] - _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // mul
        uint64_t rs1              = _reg[inst.as.r_type.rs1()];
        uint64_t rs2              = _reg[inst.as.r_type.rs2()];
        _reg[inst.as.r_type.rd()] = rs1 * rs2;
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_sll_or_mulh: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // sll
        constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()]
            << (_reg[inst.as.r_type.rs2()] & shamt_mask);
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // mulh
#ifdef DAWN_RISCV64
        sregister_t rs1 = static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]);
        sregister_t rs2 = static_cast<sregister_t>(_reg[inst.as.r_type.rs2()]);
        uint64_t    result[2];
        mul_64x64_u(rs1, rs2, result);
        uint64_t result_hi = result[1];
        if (rs1 < 0) result_hi -= rs2;
        if (rs2 < 0) result_hi -= rs1;
        _reg[inst.as.r_type.rd()] = result_hi;
#else
        int64_t rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
        int64_t rs2 = static_cast<int32_t>(_reg[inst.as.r_type.rs2()]);
        _reg[inst.as.r_type.rd()] = static_cast<uint32_t>((rs1 * rs2) >> 32);
#endif
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_slt_or_mulhsu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // slt
        _reg[inst.as.r_type.rd()] =
            static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]) <
            static_cast<sregister_t>(_reg[inst.as.r_type.rs2()]);
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // mulhsu
#ifdef DAWN_RISCV64
        sregister_t rs1 = static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]);
        uint64_t    rs2 = _reg[inst.as.r_type.rs2()];
        uint64_t    result[2];
        mul_64x64_u(rs1, rs2, result);
        uint64_t result_hi = result[1];
        if (rs1 < 0) result_hi -= rs2;
        _reg[inst.as.r_type.rd()] = result_hi;
#else
        int64_t  rs1 = static_cast<int32_t>(_reg[inst.as.r_type.rs1()]);
        uint64_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
        _reg[inst.as.r_type.rd()] = static_cast<uint32_t>((rs1 * rs2) >> 32);
#endif
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_sltu_or_mulhu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // sltu
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] < _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // mulhu
#ifdef DAWN_RISCV64
        uint64_t rs1 = _reg[inst.as.r_type.rs1()];
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        uint64_t result[2];
        mul_64x64_u(rs1, rs2, result);
        _reg[inst.as.r_type.rd()] = result[1];
#else
        uint64_t rs1 = static_cast<uint32_t>(_reg[inst.as.r_type.rs1()]);
        uint64_t rs2 = static_cast<uint32_t>(_reg[inst.as.r_type.rs2()]);
        _reg[inst.as.r_type.rd()] = static_cast<uint32_t>((rs1 * rs2) >> 32);
#endif
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_xor_or_div: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // xor
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] ^ _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // div
        sregister_t rs1 = static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]);
        sregister_t rs2 = static_cast<sregister_t>(_reg[inst.as.r_type.rs2()]);
        if (rs1 == std::numeric_limits<sregister_t>::min() && rs2 == -1) {
          _reg[inst.as.r_type.rd()] = std::numeric_limits<sregister_t>::min();
        } else if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~register_t(0);
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = static_cast<register_t>(rs1 / rs2);
        }
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_srl_or_sra_or_divu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // srl
        constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
        _reg[inst.as.r_type.rd()]     = _reg[inst.as.r_type.rs1()] >>
                                    (_reg[inst.as.r_type.rs2()] & shamt_mask);
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0100000: {  // sra
        constexpr uint32_t shamt_mask = (sizeof(register_t) * 8) - 1;
        _reg[inst.as.r_type.rd()] =
            static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]) >>
            (_reg[inst.as.r_type.rs2()] & shamt_mask);
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // divu
        uint64_t rs1 = _reg[inst.as.r_type.rs1()];
        uint64_t rs2 = _reg[inst.as.r_type.rs2()];
        if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = ~register_t(0);
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = rs1 / rs2;
        }
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_or_or_rem: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // or
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] | _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
      } break;
      case 0b0000001: {  // rem
        sregister_t rs1 = static_cast<sregister_t>(_reg[inst.as.r_type.rs1()]);
        sregister_t rs2 = static_cast<sregister_t>(_reg[inst.as.r_type.rs2()]);
        if (rs1 == std::numeric_limits<sregister_t>::min() && rs2 == -1) {
          _reg[inst.as.r_type.rd()] = 0;
        } else if (rs2 == 0) {
          _reg[inst.as.r_type.rd()] = rs1;
        } else [[likely]] {
          _reg[inst.as.r_type.rd()] = static_cast<register_t>(rs1 % rs2);
        }
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_and_or_remu: {
    switch (inst.as.r_type.funct7()) {
      case 0b0000000: {  // and
        _reg[inst.as.r_type.rd()] =
            _reg[inst.as.r_type.rs1()] & _reg[inst.as.r_type.rs2()];
        _pc += 4;
        do_dispatch();
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
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

  _do_fence: {
    // fence not required ?
    _memory.invalidate_caches();
    std::memset(_cached_instructions, 0xff, sizeof(_cached_instructions));
    _pc += 4;
  }
    do_dispatch();

  _do_system: {
    switch (inst.as.i_type.imm()) {
      case 0b000000000000: {  // ecall
        if (_mode == 0b11) {
          do_trap(exception_code_t::e_ecall_m_mode, _pc);
        } else {
          do_trap(exception_code_t::e_ecall_u_mode, _pc);
        }
      }

      case 0b000000000001: {  // ebreak
        do_trap(exception_code_t::e_breakpoint, _pc);
      }

      case 0b001100000010: {  // mret
        register_t &mstatus = _csr[MSTATUS];
        register_t  mpp     = (mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT;
        register_t  mpie = (mstatus & MSTATUS_MPIE_MASK) >> MSTATUS_MPIE_SHIFT;
        _mode            = mpp;
        _pc              = _csr[MEPC];
        mstatus = (mstatus & ~MSTATUS_MIE_MASK) | (mpie << MSTATUS_MIE_SHIFT);
        mstatus = (mstatus & ~MSTATUS_MPIE_MASK) | (1u << MSTATUS_MPIE_SHIFT);
        mstatus = (mstatus & ~MSTATUS_MPP_MASK) | (0b00u << MSTATUS_MPP_SHIFT);
        do_dispatch();  // goto next instruction no need to break
      } break;

      case 0b000100000101: {  // wfi
        _wfi = true;
        if (_wfi_callback) [[likely]]
          _wfi_callback();
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();  // technically not needed, just putting for the sake of
                    // continuity

  _do_csrrw: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }

    _csr[addr] = _reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  _do_csrrs: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }
    _csr[addr] = csr | _reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  _do_csrrc: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }
    _csr[addr] = csr & ~_reg[rs1];
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  _do_csrrwi: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }
    _csr[addr] = rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

  _do_csrrsi: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }
    _csr[addr] = csr | rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();
  _do_csrrci: {
    // TODO: can reading csr fail ?
    uint16_t addr = inst.as.i_type.imm();
    uint64_t csr  = _csr[addr];

    uint8_t rs1 = inst.as.i_type.rs1();
    if ((addr >> 10) == 0b11 && rs1 != 0) [[unlikely]] {
      do_trap(exception_code_t::e_illegal_instruction, inst);
    }
    _csr[addr] = csr & ~rs1;
    // write old value to rd
    _reg[inst.as.i_type.rd()] = csr;
    _pc += 4;
  }
    do_dispatch();

    // TODO: fix all traps, it should be store traps, not load traps
  _do_atomic_w: {
    switch (inst.as.a_type.funct5()) {
      case 0b00010: {  // lr
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        _reservation_address      = addr;
        _is_reserved              = true;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00011: {  // sc
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_store_address_misaligned, addr);
        }
        if (_is_reserved && _reservation_address == addr) {
          __store32(_memory, addr, static_cast<uint32_t>(rs2));
          _reg[inst.as.a_type.rd()] = 0;
        } else {
          _reg[inst.as.a_type.rd()] = 1;
        }
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00001: {  // amoswap
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr, static_cast<uint32_t>(rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00000: {  // amoadd
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr, static_cast<uint32_t>(value + rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00100: {  // amoxor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr, static_cast<uint32_t>(value ^ rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b01100: {  // amoand
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr, static_cast<uint32_t>(value & rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b01000: {  // amoor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr, static_cast<uint32_t>(value | rs2));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b10000: {  // amomin
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr,
                  static_cast<uint32_t>(std::min(static_cast<int32_t>(value),
                                                 static_cast<int32_t>(rs2))));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b10100: {  // amomax
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(_memory, addr,
                  static_cast<uint32_t>(std::max(static_cast<int32_t>(value),
                                                 static_cast<int32_t>(rs2))));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b11000: {  // amominu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(
            _memory, addr,
            std::min(static_cast<uint32_t>(value), static_cast<uint32_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b11100: {  // amomaxu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 4;  // 4 for w
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint32_t value;
        __load32(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = sext<32>(value);
        __store32(
            _memory, addr,
            std::max(static_cast<uint32_t>(value), static_cast<uint32_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();

#ifdef DAWN_RISCV64
  _do_atomic_d: {
    switch (inst.as.a_type.funct5()) {
      case 0b00010: {  // lr
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        _reservation_address      = addr;
        _is_reserved              = true;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00011: {  // sc
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_store_address_misaligned, addr);
        }
        if (_is_reserved && _reservation_address == addr) {
          // no e_store_access_fault in this implementation
          __store64(_memory, addr, rs2);
          _reg[inst.as.a_type.rd()] = 0;
        } else {
          _reg[inst.as.a_type.rd()] = 1;
        }
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00001: {  // amoswap
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr, rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00000: {  // amoadd
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr, value + rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b00100: {  // amoxor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr, value ^ rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b01100: {  // amoand
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr, value & rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b01000: {  // amoor
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr, value | rs2);
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b10000: {  // amomin
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr,
                  std::min(static_cast<sregister_t>(value),
                           static_cast<sregister_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b10100: {  // amomax
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr,
                  std::max(static_cast<sregister_t>(value),
                           static_cast<sregister_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b11000: {  // amominu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr,
                  std::min(static_cast<register_t>(value),
                           static_cast<register_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;
      case 0b11100: {  // amomaxu
        const uint64_t rs1       = _reg[inst.as.a_type.rs1()];
        const uint64_t rs2       = _reg[inst.as.a_type.rs2()];
        const uint64_t addr      = rs1;
        const uint32_t alignment = 8;  // 8 for d
        if (addr % alignment != 0) [[unlikely]] {
          do_trap(exception_code_t::e_load_address_misaligned, addr);
        }
        uint64_t value;
        __load64(_memory, value, addr);  // may fault
        _reg[inst.as.a_type.rd()] = value;
        __store64(_memory, addr,
                  std::max(static_cast<register_t>(value),
                           static_cast<register_t>(rs2)));
        _is_reserved         = false;
        _reservation_address = 0;
        _pc += 4;
        do_dispatch();
      } break;

      default:
        goto _do_unknown_instruction;
    }
  }
    do_dispatch();
#endif

  _do_unknown_instruction:
    do_trap(exception_code_t::e_illegal_instruction, _inst);

  _do_trap:
    handle_trap(trap_cause, trap_value);
    do_dispatch();
  }

  // memory
  memory_t _memory;
  // const size_t _ram_size;
  // uint8_t     *_data;
  // uint64_t     _offset{};
  // uint8_t     *_final{};

  static const register_t _bits_per_instruction_cache = 12;
  static const register_t _num_instruction_cache =
      1 << _bits_per_instruction_cache;
  cached_instruction_t _cached_instructions[_num_instruction_cache];

  const std::vector<mmio_handler_t> _mmios;
  std::list<mmio_page_data_t>       _mmio_page_data;

  bool _wfi = false;
  typedef void (*wfi_callback_t)();
  wfi_callback_t _wfi_callback = 0;

  register_t _reg[32] = {0};
  register_t _pc{0};
  register_t _mode{0b11};
  register_t _reservation_address;
  bool       _is_reserved = false;

#ifdef DAWN_ENABLE_LOGGING
  std::ofstream _log{"/tmp/dawn", std::ios::trunc};
#endif

  // hack
  std::function<void(void *, exception_code_t, register_t)> _trap_callback;
  void *_trap_usr_data = nullptr;

  // TODO: optimise csr
  std::map<uint16_t, register_t> _csr;
};

}  // namespace dawn
#endif
