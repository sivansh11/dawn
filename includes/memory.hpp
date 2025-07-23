#ifndef DAWN_MEMORY_HPP
#define DAWN_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <flat_set>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace dawn {

// TODO: maybe change this to uint8_t ?
enum class memory_protection_t : uint32_t {
  e_none  = 0,
  e_write = 1u << 0,
  e_read  = 1u << 1,
  e_exec  = 1u << 2,

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

std::string to_string(memory_protection_t protection);

struct memory_t {
  struct range_t {
    uintptr_t           _start;
    uintptr_t           _end;
    memory_protection_t _protection = memory_protection_t::e_none;
    bool operator<(const range_t &other) const { return _start < other._start; }
    bool operator==(const range_t &other) const {
      return _start == other._start && _end == other._end &&
             _protection == other._protection;
    }
    bool overlaps_with(const range_t &other) const {
      return std::max(_start, other._start) < std::min(_end, other._end);
    }
    bool is_adjacent_to(const range_t &other) const {
      return _end == other._start || other._end == _start;
    }
    bool contains(const range_t &other) const {
      return _start <= other._start && _end >= other._end;
    }
  };

  memory_t(uint64_t size);
  ~memory_t();

  uint64_t translate_host_to_guest_virtual(uintptr_t address);
  uint64_t translate_guest_virtual_to_guest_physical(uint64_t virtual_address);
  uint64_t translate_guest_virtual_to_host(uint64_t virtual_address);

  void insert_memory(uintptr_t address, size_t size);
  void insert_memory(uintptr_t address, size_t size,
                     memory_protection_t protection);
  bool is_region_in_memory(uintptr_t address, size_t size);
  bool is_region_in_memory(uintptr_t address, size_t size,
                           memory_protection_t protection);

  void memcpy_host_to_guest(uint64_t dst, const void *src, uint64_t size);
  void memcpy_guest_to_host(void *dst, uint64_t src, uint64_t size);
  void memset(uint64_t dst, int value, uint64_t size);

  // clang-format off
  template <size_t size>
  std::optional<uint64_t> load(uint64_t virtual_address) {
    if constexpr (size == 8) return _load_8(virtual_address);
    else if constexpr (size == 16) return _load_16(virtual_address);
    else if constexpr (size == 32) return _load_32(virtual_address);
    else if constexpr (size == 64) return _load_64(virtual_address);
    else throw std::runtime_error("Error: unknown size load");
  }
  template <size_t size>
  bool store(uint64_t virtual_address, uint64_t value) {
    if constexpr (size == 8) return _store_8(virtual_address, value);
    else if constexpr (size == 16) return _store_16(virtual_address, value);
    else if constexpr (size == 32) return _store_32(virtual_address, value);
    else if constexpr (size == 64) return _store_64(virtual_address, value);
    else throw std::runtime_error("Error: unknown size store");
  }
  template <size_t size>
  std::optional<uint64_t> fetch(uint64_t virtual_address) {
    if constexpr (size == 32) return _fetch_32(virtual_address);
    else throw std::runtime_error("Error: unknown size load");
  }
  // clang-format on

  std::optional<uint64_t> _load_8(uint64_t virtual_address);
  std::optional<uint64_t> _load_16(uint64_t virtual_address);
  std::optional<uint64_t> _load_32(uint64_t virtual_address);
  std::optional<uint64_t> _load_64(uint64_t virtual_address);

  bool _store_8(uint64_t virtual_address, uint64_t value);
  bool _store_16(uint64_t virtual_address, uint64_t value);
  bool _store_32(uint64_t virtual_address, uint64_t value);
  bool _store_64(uint64_t virtual_address, uint64_t value);

  std::optional<uint64_t> _fetch_32(uint64_t virtual_address);

  std::flat_set<range_t> _ranges;
  std::flat_set<range_t> _ranges_no_protection;

  uint64_t _size;
  uint64_t _guest_base;
  uint8_t *_host_base;
};

std::ostream &operator<<(std::ostream &o, const memory_t::range_t &range);

}  // namespace dawn

#endif  // !DAWN_MEMORY_HPP
