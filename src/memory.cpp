#include "dawn/memory.hpp"

#include <cassert>
#include <cstring>
#include <flat_set>
#include <optional>

namespace dawn {

memory_t memory_t::create(void* ptr, size_t size) {
  memory_t memory{};
  memory._host_base = ptr;
  memory.insert_memory(ptr, size, memory_protection_t::e_none, nullptr,
                       nullptr);
  memory._size = size;
  return memory;
}

address_t memory_t::translate_host_to_guest(void* ptr) const {
  return reinterpret_cast<address_t>(ptr) -
         reinterpret_cast<address_t>(_host_base) + guest_base;
}
void* memory_t::translate_guest_to_host(address_t addr) const {
  return reinterpret_cast<void*>(reinterpret_cast<address_t>(_host_base) -
                                 guest_base + addr);
}

void memory_t::insert_memory(
    void* ptr, size_t size, memory_protection_t protection,
    std::function<void(address_t, uint32_t)> write_callback,
    std::function<uint32_t(address_t)>       read_callback) {
  // TODO: merge adjacent ranges with smae protection
  memory_range_t range = memory_range_t::create_from_start_and_size(
      ptr, size, protection, write_callback, read_callback);
  dawn::flat_set<memory_range_t> new_ranges;
  new_ranges.insert(range);
  for (const auto& _range : _ranges) {
    if (range.overlaps_with(_range)) {
      // R1 == range
      // R2 == _range
      if (range.contains(_range)) {
        // R1 ----------------
        // R2     --------
        // Do nothing
      }
      if (_range.contains(range)) {
        // R1     --------
        // R2 ----------------
        // Break R2 into 2 subranges
        if (_range._start != range._start)
          new_ranges.insert(
              memory_range_t{_range._start, range._start, _range._protection});
        if (range._end != _range._end)
          new_ranges.insert(
              memory_range_t{range._end, _range._end, _range._protection});
      }
      if (_range._start > range._start && _range._end > range._end) {
        // R1 ------------
        // R2       ------------
        // Break R2
        new_ranges.insert(
            memory_range_t{range._end, _range._end, _range._protection});
      }
      if (range._start > _range._start && range._end > _range._end) {
        // R1       ------------
        // R2 ------------
        // Break R2
        new_ranges.insert(
            memory_range_t{_range._start, range._start, _range._protection});
      }
    } else {
      new_ranges.insert(_range);
    }
  }
  _ranges = new_ranges;
}

bool memory_t::is_region_in_memory(void* ptr, size_t size,
                                   memory_protection_t protection) const {
  assert(size);
  memory_range_t range = memory_range_t::create_from_start_and_size(
      ptr, size, protection, nullptr, nullptr);
  auto itr = _ranges.lower_bound(range);
  if (itr != _ranges.begin()) itr--;

  void* current = ptr;
  void* end     = reinterpret_cast<void*>(reinterpret_cast<size_t>(ptr) + size);
  while (current < end) {
    while (itr != _ranges.end() && itr->_end <= current) itr++;
    if (itr == _ranges.end() || itr->_start > current) return false;
    if (!has_all(itr->_protection, protection)) return false;
    current = itr->_end;
  }
  return true;
}

std::optional<memory_range_t> memory_t::find_memory_range(
    void* ptr, size_t size, memory_protection_t protection) const {
  memory_range_t range = memory_range_t::create_from_start_and_size(
      ptr, size, protection, nullptr, nullptr);
  auto itr = _ranges.lower_bound(range);
  if (itr != _ranges.begin() && itr->_start > ptr) {
    itr--;
  }
  if (itr != _ranges.end() && itr->_start <= ptr &&
      itr->_end >= reinterpret_cast<char*>(ptr) + size &&
      has_all(itr->_protection, protection)) {
    return *itr;
  }
  return std::nullopt;
}

bool memory_t::memcpy_host_to_guest(address_t dst, const void* src,
                                    size_t size) const {
  // NOTE: no protection checking!
  if (!is_region_in_memory(translate_guest_to_host(dst), size, {}))
    return false;
  std::memcpy(translate_guest_to_host(dst), src, size);
  return true;
}
bool memory_t::memcpy_guest_to_host(void* dst, address_t src,
                                    size_t size) const {
  // NOTE: no protection checking!
  if (!is_region_in_memory(translate_guest_to_host(src), size, {}))
    return false;
  std::memcpy(dst, translate_guest_to_host(src), size);
  return true;
}
bool memory_t::memset(address_t addr, int value, size_t size) const {
  // NOTE: no protection checking!
  if (!is_region_in_memory(translate_guest_to_host(addr), size, {}))
    return false;
  std::memset(translate_guest_to_host(addr), value, size);
  return true;
}

std::optional<uint32_t> memory_t::fetch_32(address_t addr) const {
  uint32_t    value;
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_exec);
  if (!memory_range) return std::nullopt;
  if (memory_range->read_callback)
    return memory_range->read_callback(addr);
  else {
    std::memcpy(&value, translate_guest_to_host(addr), sizeof(value));
    return value;
  }
}

std::optional<uint8_t> memory_t::load_8(address_t addr) const {
  uint8_t     value;
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return std::nullopt;
  if (memory_range->read_callback)
    return memory_range->read_callback(addr);
  else {
    std::memcpy(&value, translate_guest_to_host(addr), sizeof(value));
    return value;
  }
}
std::optional<uint16_t> memory_t::load_16(address_t addr) const {
  uint16_t    value;
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return std::nullopt;
  if (memory_range->read_callback)
    return memory_range->read_callback(addr);
  else {
    std::memcpy(&value, translate_guest_to_host(addr), sizeof(value));
    return value;
  }
}
std::optional<uint32_t> memory_t::load_32(address_t addr) const {
  uint32_t    value;
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return std::nullopt;
  if (memory_range->read_callback)
    return memory_range->read_callback(addr);
  else {
    std::memcpy(&value, translate_guest_to_host(addr), sizeof(value));
    return value;
  }
}
std::optional<uint64_t> memory_t::load_64(address_t addr) const {
  uint64_t    value;
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return std::nullopt;
  if (memory_range->read_callback)
    return memory_range->read_callback(addr);
  else {
    std::memcpy(&value, translate_guest_to_host(addr), sizeof(value));
    return value;
  }
}

bool memory_t::store_8(address_t addr, uint8_t value) const {
  assert(is_region_in_memory(translate_guest_to_host(addr), 1,
                             memory_protection_t::e_read));
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return false;
  if (memory_range->write_callback) {
    memory_range->write_callback(addr, value);
    return true;
  } else {
    std::memcpy(translate_guest_to_host(addr), &value, sizeof(value));
    return true;
  }
}
bool memory_t::store_16(address_t addr, uint16_t value) const {
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return false;
  if (memory_range->write_callback) {
    memory_range->write_callback(addr, value);
    return true;
  } else {
    std::memcpy(translate_guest_to_host(addr), &value, sizeof(value));
    return true;
  }
}
bool memory_t::store_32(address_t addr, uint32_t value) const {
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return false;
  if (memory_range->write_callback) {
    memory_range->write_callback(addr, value);
    return true;
  } else {
    std::memcpy(translate_guest_to_host(addr), &value, sizeof(value));
    return true;
  }
}
bool memory_t::store_64(address_t addr, uint64_t value) const {
  const auto& memory_range =
      find_memory_range(translate_guest_to_host(addr), sizeof(value),
                        memory_protection_t::e_read);
  if (!memory_range) return false;
  if (memory_range->write_callback) {
    memory_range->write_callback(addr, value);
    return true;
  } else {
    std::memcpy(translate_guest_to_host(addr), &value, sizeof(value));
    return true;
  }
}

}  // namespace dawn

std::ostream& operator<<(std::ostream&             o,
                         dawn::memory_protection_t protection) {
  if (dawn::has_all(protection, dawn::memory_protection_t::e_write))
    o << "w";
  else
    o << "-";
  if (dawn::has_all(protection, dawn::memory_protection_t::e_read))
    o << "r";
  else
    o << "-";
  if (dawn::has_all(protection, dawn::memory_protection_t::e_exec))
    o << "x";
  else
    o << "-";
  return o;
}
std::ostream& operator<<(std::ostream& o, dawn::memory_range_t range) {
  o << range._protection << " " << std::hex << range._start << " -> "
    << range._end << std::dec;
  return o;
}
std::ostream& operator<<(std::ostream& o, dawn::memory_t memory) {
  for (const auto& range : memory._ranges) {
    o << range << '\n';
  }
  return o;
}
