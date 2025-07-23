#include "memory.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ios>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace dawn {

std::string to_string(memory_protection_t protection) {
  switch (protection) {
    case memory_protection_t::e_none:
      return "none";
    case memory_protection_t::e_write:
      return "write";
    case memory_protection_t::e_read:
      return "read";
    case memory_protection_t::e_exec:
      return "exec";
    default:
      throw std::runtime_error("Error: unknown protection");
  }
}

memory_t::memory_t(uint64_t size) : _size(size), _guest_base(0) {
  _host_base = new uint8_t[_size];
}
memory_t::~memory_t() { delete[] _host_base; }

uint64_t memory_t::translate_host_to_guest_virtual(uintptr_t address) {
  return address - reinterpret_cast<uint64_t>(_host_base) + _guest_base;
}
uint64_t memory_t::translate_guest_virtual_to_guest_physical(
    uint64_t virtual_address) {
  assert(virtual_address >= _guest_base);
  return virtual_address - _guest_base;
}

uint64_t memory_t::translate_guest_virtual_to_host(uint64_t virtual_address) {
  return reinterpret_cast<uint64_t>(_host_base) +
         translate_guest_virtual_to_guest_physical(virtual_address);
}

void memory_t::insert_memory(uintptr_t address, size_t size,
                             memory_protection_t protection) {
  assert(size);
  range_t new_range{address, address + size, protection};
  // TODO: optimise
  for (const auto &range : _ranges) {
    if (new_range.overlaps_with(range))
      throw std::runtime_error("Error: overlap detected");
  }
  _ranges.insert(new_range);
}
bool memory_t::is_region_in_memory(uintptr_t address, size_t size) {
  assert(_ranges.size());  // atleast 1 range should be inserted
  assert(size);
  range_t range{address, address + size};

  auto itr = _ranges.lower_bound(range);

  if (itr != _ranges.end())
    if (itr->contains(range)) return true;
  if (itr == _ranges.begin()) return false;
  itr = std::prev(itr);
  if (itr->contains(range)) return true;
  return false;
}
// TODO: remove code duplication ?
bool memory_t::is_region_in_memory(uintptr_t address, size_t size,
                                   memory_protection_t protection) {
  assert(_ranges.size());  // atleast 1 range should be inserted
  assert(size);
  range_t range{address, address + size, protection};

  auto itr = _ranges.lower_bound(range);

  if (itr != _ranges.end())
    if (itr->contains(range) && has_all(itr->_protection, protection))
      return true;
  if (itr == _ranges.begin()) return false;
  itr = std::prev(itr);
  if (itr->contains(range) && has_all(itr->_protection, protection))
    return true;
  return false;
}

void memory_t::memcpy_host_to_guest(uint64_t dst, const void *src,
                                    uint64_t size) {
  assert(src);
  assert(is_region_in_memory(translate_guest_virtual_to_host(dst), size));
  std::memcpy(reinterpret_cast<void *>(translate_guest_virtual_to_host(dst)),
              src, size);
}
void memory_t::memcpy_guest_to_host(void *dst, uint64_t src, uint64_t size) {
  assert(dst);
  assert(is_region_in_memory(translate_guest_virtual_to_host(src), size));
  std::memcpy(dst,
              reinterpret_cast<void *>(translate_guest_virtual_to_host(src)),
              size);
}
void memory_t::memset(uint64_t dst, int value, uint64_t size) {
  assert(is_region_in_memory(translate_guest_virtual_to_host(dst), size));
  std::memset(reinterpret_cast<void *>(translate_guest_virtual_to_host(dst)),
              value, size);
}

template <typename T>
T read_as(const uint8_t *src) {
  // TODO: add check for T is u8/u16/u32/u64
  // TODO: assert src
  T result;
  std::memcpy(&result, src, sizeof(T));
  return result;
}

template <typename T>
void write_as(uint8_t *dst, T value) {
  // TODO: add check for T is u8/u16/u32/u64
  // TODO: assert dst
  std::memcpy(dst, &value, sizeof(T));
}

std::optional<uint64_t> memory_t::_load_8(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 1,
                           memory_protection_t::e_read)) {
    return std::nullopt;
  }
  return read_as<uint8_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
std::optional<uint64_t> memory_t::_load_16(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 2,
                           memory_protection_t::e_read)) {
    return std::nullopt;
  }
  return read_as<uint16_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
std::optional<uint64_t> memory_t::_load_32(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 4,
                           memory_protection_t::e_read)) {
    return std::nullopt;
  }
  return read_as<uint32_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
std::optional<uint64_t> memory_t::_load_64(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 8,
                           memory_protection_t::e_read)) {
    return std::nullopt;
  }
  return read_as<uint64_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}

bool memory_t::_store_8(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 1,
                           memory_protection_t::e_write)) {
    return false;
  }
  write_as<uint8_t>(reinterpret_cast<uint8_t *>(
                        translate_guest_virtual_to_host(virtual_address)),
                    value);
  return true;
}
bool memory_t::_store_16(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 2,
                           memory_protection_t::e_write)) {
    return false;
  }
  write_as<uint16_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
  return true;
}
bool memory_t::_store_32(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 4,
                           memory_protection_t::e_write)) {
    return false;
  }
  write_as<uint32_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
  return true;
}
bool memory_t::_store_64(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 8,
                           memory_protection_t::e_write)) {
    return false;
  }
  write_as<uint64_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
  return true;
}

std::optional<uint64_t> memory_t::_fetch_32(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address), 4,
                           memory_protection_t::e_exec)) {
    return std::nullopt;
  }
  return read_as<uint32_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}

}  // namespace dawn
