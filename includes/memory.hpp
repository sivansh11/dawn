#ifndef DAWN_MEMORY_HPP
#define DAWN_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace dawn {

struct memory_t {
  struct range_t {
    uintptr_t _start;
    uintptr_t _end;
    bool operator<(const range_t &other) const { return _start < other._start; }
  };

  memory_t(uint64_t size);
  ~memory_t();

  uint64_t translate_host_to_guest_virtual(uintptr_t address);
  uint64_t translate_guest_virtual_to_guest_physical(uint64_t virtual_address);
  uint64_t translate_guest_virtual_to_host(uint64_t virtual_address);

  void insert_memory(uintptr_t address, size_t size);
  bool is_region_in_memory(uintptr_t address, size_t size);

  // clang-format off
  template <size_t size>
  uint64_t load(uint64_t virtual_address) {
    if constexpr (size == 8) return _load_8(virtual_address);
    else if constexpr (size == 16) return _load_16(virtual_address);
    else if constexpr (size == 32) return _load_32(virtual_address);
    else if constexpr (size == 64) return _load_64(virtual_address);
    else throw std::runtime_error("Error: unknown size load");
  }
  template <size_t size>
  void store(uint64_t virtual_address, uint64_t value) {
    if constexpr (size == 8) _store_8(virtual_address, value);
    else if constexpr (size == 16) _store_16(virtual_address, value);
    else if constexpr (size == 32) _store_32(virtual_address, value);
    else if constexpr (size == 64) _store_64(virtual_address, value);
    else throw std::runtime_error("Error: unknown size store");
  }
  // clang-format on

  uint64_t _load_8(uint64_t virtual_address);
  uint64_t _load_16(uint64_t virtual_address);
  uint64_t _load_32(uint64_t virtual_address);
  uint64_t _load_64(uint64_t virtual_address);

  void _store_8(uint64_t virtual_address, uint64_t value);
  void _store_16(uint64_t virtual_address, uint64_t value);
  void _store_32(uint64_t virtual_address, uint64_t value);
  void _store_64(uint64_t virtual_address, uint64_t value);

  std::vector<range_t> _ranges;

  uint64_t _size;
  uint64_t _guest_base;
  uint8_t *_host_base;
};

}  // namespace dawn

#endif  // !DAWN_MEMORY_HPP
