#ifndef DAWN_MEMORY_HPP
#define DAWN_MEMORY_HPP

#include <cstdint>
// #include <flat_set>
#include <optional>
#include <ostream>

#include <dawn/flat_set.hpp>

namespace dawn {

// TODO: maybe change this to uint8_t
enum class memory_protection_t : uint32_t {
  e_none       = 0,
  e_write      = 1u << 0,
  e_read       = 1u << 1,
  e_exec       = 1u << 2,
  e_read_write = e_read | e_write,
  e_read_exec  = e_read | e_exec,
  e_write_exec = e_write | e_exec,
  e_all        = e_read | e_write | e_exec,
};

inline memory_protection_t operator|(memory_protection_t a,
                                     memory_protection_t b) {
  return static_cast<memory_protection_t>(static_cast<uint32_t>(a) |
                                          static_cast<uint32_t>(b));
}
inline memory_protection_t operator&(memory_protection_t a,
                                     memory_protection_t b) {
  return static_cast<memory_protection_t>(static_cast<uint32_t>(a) &
                                          static_cast<uint32_t>(b));
}
inline bool has_all(memory_protection_t container, memory_protection_t check) {
  return (container & check) == check;
}
inline bool has_any(memory_protection_t container, memory_protection_t check) {
  return static_cast<uint32_t>(container & check) != 0u;
}

struct memory_range_t {
  void*                 _start;
  void*                 _end;
  memory_protection_t   _protection = memory_protection_t::e_none;
  static memory_range_t create_from_start_and_size(
      void* ptr, size_t size, memory_protection_t protection) {
    memory_range_t range{._start = ptr, ._protection = protection};
    range._end = reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + size);
    return range;
  }
  inline bool operator<(const memory_range_t& other) const {
    return _start < other._start;
  }
  inline bool operator==(const memory_range_t& other) const {
    return _start == other._start && _end == other._end &&
           _protection == other._protection;
  }
  inline bool overlaps_with(const memory_range_t& other) const {
    return std::max(_start, other._start) < std::min(_end, other._end);
  }
  inline bool is_adjacent_to(const memory_range_t& other) const {
    return _end == other._start || other._end == _start;
  }
  inline bool contains(const memory_range_t& other) const {
    return _start <= other._start && _end >= other._end;
  }
};

// vm address, not host address
using address_t = uint64_t;

// memory_t is more of a memory management unit, it doesnt contain the memory
// it just defines accessible memory ranges
struct memory_t {
  static memory_t create(void* ptr, size_t size);

  address_t translate_host_to_guest(void* ptr) const;
  void*     translate_guest_to_host(address_t addr) const;

  // if there is overlap with different memory protection, it cuts the affected
  // ranges into sub ranges and inserts new range with appropriate protection
  void insert_memory(void* ptr, size_t size, memory_protection_t protection);
  bool is_region_in_memory(void* ptr, size_t size,
                           memory_protection_t protection) const;
  bool memcpy_host_to_guest(address_t dst, const void* src, size_t size) const;
  bool memcpy_guest_to_host(void* dst, address_t src, size_t size) const;
  bool memset(address_t addr, int value, size_t size) const;

  // checks for exec memory protection
  std::optional<uint32_t> fetch_32(address_t addr) const;

  // checks for read memory protection
  std::optional<uint8_t>  load_8(address_t addr) const;
  std::optional<uint16_t> load_16(address_t addr) const;
  std::optional<uint32_t> load_32(address_t addr) const;
  std::optional<uint64_t> load_64(address_t addr) const;

  // checks for write memory protection
  bool store_8(address_t addr, uint8_t value) const;
  bool store_16(address_t addr, uint16_t value) const;
  bool store_32(address_t addr, uint32_t value) const;
  bool store_64(address_t addr, uint64_t value) const;

  size_t                        _size;
  dawn::flat_set<memory_range_t> _ranges{};  // ranges with memory protection
  void*                         _host_base{};
  address_t
      guest_base{};  // guest_base is set by the function loading the elf script
};

}  // namespace dawn

std::ostream& operator<<(std::ostream& o, dawn::memory_protection_t protection);
std::ostream& operator<<(std::ostream& o, dawn::memory_range_t range);
std::ostream& operator<<(std::ostream& o, dawn::memory_t memory);

#endif  // !DAWN_MEMORY_HPP
