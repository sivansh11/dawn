#include "memory.hpp"

#include <cstring>
#include <stdexcept>
#include <tuple>

namespace dawn {

memory_t::memory_t(uint64_t size, uint64_t page_size)
    : _size(size), _page_size(page_size) {
  _data = new uint8_t[_size];
}
memory_t::~memory_t() { delete[] _data; }

std::tuple<uint64_t, uint64_t, uint64_t>
memory_t::_translate_virtual_address_to_physical_address(
    uint64_t virtual_address) {
  uint64_t virtual_page_number = virtual_address / _page_size;
  uint64_t offset              = virtual_address % _page_size;
  auto     itr                 = _page_table.find(virtual_page_number);
  if (itr != _page_table.end()) {
    // found page in page table
    uint64_t physical_page_number = itr->second;
    uint64_t physical_address = (physical_page_number * _page_size) + offset;
    if (physical_address >= _size)
      return {virtual_page_number, offset, invalid_address};
    return {virtual_page_number, offset, physical_address};
  }
  uint64_t physical_page_number    = _next_free_page++;
  _page_table[virtual_page_number] = physical_page_number;
  uint64_t physical_address = (physical_page_number * _page_size) + offset;
  if (physical_address >= _size)
    return {virtual_page_number, offset, invalid_address};
  return {virtual_page_number, offset, physical_address};
}

uint64_t memory_t::translate_virtual_address_to_physical_address(
    uint64_t virtual_address) {
  auto [virtual_page_number, offset, physical_address] =
      _translate_virtual_address_to_physical_address(virtual_address);
  return physical_address;
}

bool memory_t::memcpy_host_to_guest(uint64_t    guest_dst_virtual_address,
                                    const void *host_src_address,
                                    uint64_t    size) {
  uint64_t       bytes_remaining = size;
  const uint8_t *current_host_src =
      reinterpret_cast<const uint8_t *>(host_src_address);
  uint64_t current_guest_virtual_address = guest_dst_virtual_address;

  while (bytes_remaining) {
    auto [virtual_page_number, offset, physical_address] =
        _translate_virtual_address_to_physical_address(
            current_guest_virtual_address);
    if (physical_address == invalid_address) return false;
    uint64_t bytes_copiable_in_current_page = _page_size - offset;
    uint64_t bytes_to_copy_in_current_page =
        std::min(bytes_remaining, bytes_copiable_in_current_page);
    if (physical_address + bytes_to_copy_in_current_page > _size) return false;
    std::memcpy(_data + physical_address, current_host_src,
                bytes_to_copy_in_current_page);
    current_host_src += bytes_to_copy_in_current_page;
    current_guest_virtual_address += bytes_to_copy_in_current_page;
    bytes_remaining -= bytes_to_copy_in_current_page;
  }
  return true;
}

bool memory_t::memcpy_guest_to_host(void    *host_dst_address,
                                    uint64_t guest_src_virtual_address,
                                    uint64_t size) {
  if (!host_dst_address) return false;
  uint64_t bytes_remaining  = size;
  uint8_t *current_host_dst = reinterpret_cast<uint8_t *>(host_dst_address);
  uint64_t current_guest_virtual_address = guest_src_virtual_address;

  while (bytes_remaining) {
    auto [virtual_page_number, offset, physical_address] =
        _translate_virtual_address_to_physical_address(
            current_guest_virtual_address);
    // if a page does not exist, does that mean we read nothing ?
    if (physical_address == invalid_address) return false;
    uint64_t bytes_copiable_in_current_page = _page_size - offset;
    uint64_t bytes_to_copy_in_current_page =
        std::min(bytes_remaining, bytes_copiable_in_current_page);
    if (physical_address + bytes_to_copy_in_current_page > _size) return false;
    std::memcpy(current_host_dst, _data + physical_address,
                bytes_to_copy_in_current_page);
    current_host_dst += bytes_to_copy_in_current_page;
    current_guest_virtual_address += bytes_to_copy_in_current_page;
    bytes_remaining -= bytes_to_copy_in_current_page;
  }
  return true;
}

uint64_t memory_t::_load_8(uint64_t virtual_address) {
  uint8_t value;
  if (!memcpy_guest_to_host(&value, virtual_address, 1)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 8");
  }
  return value;
}
uint64_t memory_t::_load_16(uint64_t virtual_address) {
  uint16_t value;
  if (!memcpy_guest_to_host(&value, virtual_address, 2)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 16");
  }
  return value;
}
uint64_t memory_t::_load_32(uint64_t virtual_address) {
  uint32_t value;
  if (!memcpy_guest_to_host(&value, virtual_address, 4)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 32");
  }
  return value;
}
uint64_t memory_t::_load_64(uint64_t virtual_address) {
  uint64_t value;
  if (!memcpy_guest_to_host(&value, virtual_address, 8)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to load 64");
  }
  return value;
}

void memory_t::_store_8(uint64_t virtual_address, uint64_t value) {
  uint8_t value_8 = static_cast<uint8_t>(value);
  if (!memcpy_host_to_guest(virtual_address, &value_8, 1)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 8");
  }
}
void memory_t::_store_16(uint64_t virtual_address, uint64_t value) {
  uint16_t value_16 = static_cast<uint16_t>(value);
  if (!memcpy_host_to_guest(virtual_address, &value_16, 2)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 16");
  }
}
void memory_t::_store_32(uint64_t virtual_address, uint64_t value) {
  uint32_t value_32 = static_cast<uint32_t>(value);
  if (!memcpy_host_to_guest(virtual_address, &value_32, 4)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 32");
  }
}
void memory_t::_store_64(uint64_t virtual_address, uint64_t value) {
  if (!memcpy_host_to_guest(virtual_address, &value, 8)) {
    // TODO: propagate error to caller
    throw std::runtime_error("Error: failed to store 64");
  }
}

}  // namespace dawn
