#include "memory.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ios>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace dawn {

memory_t::memory_t(uint64_t size) : _size(size), _guest_base(0) {
  _host_base = new uint8_t[_size];
  insert_memory(reinterpret_cast<uintptr_t>(_host_base), _size);
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

void memory_t::insert_memory(uintptr_t addr, size_t size) {
  assert(size);  // size cant be 0
  uintptr_t start = addr;
  uintptr_t end   = start + size;
  range_t   range{start, end};

  auto itr = std::upper_bound(_ranges.begin(), _ranges.end(), range);
  // check for overlaps
  if (itr != _ranges.begin()) {
    auto prev_itr = itr - 1;
    if (std::max(range._start, prev_itr->_start) <
        std::min(range._end, prev_itr->_end))
      throw std::runtime_error("Error: Overlap detected in memory");
  }
  if (itr != _ranges.end()) {
    if (std::max(range._start, itr->_start) < std::min(range._end, itr->_end))
      throw std::runtime_error("Error: Overlap detected in memory");
  }
  // check if can merge
  if (itr != _ranges.begin()) {
    auto prev_itr = itr - 1;
    if (prev_itr->_end == range._start) {
      range._start = prev_itr->_start;
      itr          = _ranges.erase(prev_itr);
    }
  }
  if (itr != _ranges.end()) {
    if (range._end == itr->_start) {
      range._end = itr->_end;
      itr        = _ranges.erase(itr);
    }
  }
  _ranges.insert(itr, range);
}

bool memory_t::is_region_in_memory(uintptr_t addr, size_t size) {
  assert(size);  // size cant be 0
  uintptr_t start = addr;
  uintptr_t end   = start + size;

  uintptr_t current = start;

  auto itr =
      std::lower_bound(_ranges.begin(), _ranges.end(),
                       range_t{start, std::numeric_limits<uintptr_t>::max()});
  if (itr != _ranges.begin()) {
    if ((itr - 1)->_end > start) {
      itr--;
    }
  }
  while (current < end) {
    if (itr == _ranges.end() || itr->_start > current) {
      return false;
    }
    if (current >= itr->_start && current < itr->_end) {
      current = std::min(end, itr->_end);
    } else
      itr++;
  }
  return true;
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

uint64_t memory_t::_load_8(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           1)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 8");
  }
  return read_as<uint8_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
uint64_t memory_t::_load_16(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           2)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 16");
  }
  return read_as<uint16_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
uint64_t memory_t::_load_32(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           4)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 32");
  }
  return read_as<uint32_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}
uint64_t memory_t::_load_64(uint64_t virtual_address) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           8)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 64");
  }
  return read_as<uint64_t>(reinterpret_cast<uint8_t *>(
      translate_guest_virtual_to_host(virtual_address)));
}

void memory_t::_store_8(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           1)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 8");
  }
  write_as<uint8_t>(reinterpret_cast<uint8_t *>(
                        translate_guest_virtual_to_host(virtual_address)),
                    value);
}
void memory_t::_store_16(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           2)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 16");
  }
  write_as<uint16_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
}
void memory_t::_store_32(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           4)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 32");
  }
  write_as<uint32_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
}
void memory_t::_store_64(uint64_t virtual_address, uint64_t value) {
  if (!is_region_in_memory(translate_guest_virtual_to_host(virtual_address),
                           8)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 64");
  }
  write_as<uint64_t>(reinterpret_cast<uint8_t *>(
                         translate_guest_virtual_to_host(virtual_address)),
                     value);
}

}  // namespace dawn
