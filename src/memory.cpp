#include "memory.hpp"

namespace dawn {

memory_t memory_t::create(void* ptr, size_t size) {
  memory_t memory{};
  memory._host_base = ptr;
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

void memory_t::insert_memory(void* ptr, size_t size,
                             memory_protection_t protection) {}

bool memory_t::is_region_in_memory(void* ptr, size_t size,
                                   memory_protection_t protection) {}
void memory_t::memcpy_host_to_guest(address_t dst, void* src, size_t size) {}
void memory_t::memcpy_guest_to_host(void* dst, address_t src, size_t size) {}
void memory_t::memset(address_t addr, int value, size_t size) {}

std::optional<uint32_t> memory_t::fetch_32(address_t addr) {}

std::optional<uint8_t>  memory_t::load_8(address_t addr) {}
std::optional<uint16_t> memory_t::load_16(address_t addr) {}
std::optional<uint32_t> memory_t::load_32(address_t addr) {}
std::optional<uint64_t> memory_t::load_64(address_t addr) {}

void memory_t::store_8(address_t addr, uint8_t value) {}
void memory_t::store_16(address_t addr, uint16_t value) {}
void memory_t::store_32(address_t addr, uint32_t value) {}
void memory_t::store_64(address_t addr, uint64_t value) {}

}  // namespace dawn
