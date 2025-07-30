#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>

#include "memory.hpp"

namespace {
bool test_passed = true;
void test_case(const std::string& name, std::function<void()> test) {
  try {
    test();
    std::cout << "[ \033[32mpass\033[0m ] " << name << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "[ \033[31mfail\033[0m ] " << name << ": " << e.what()
              << std::endl;
    test_passed = false;
  }
}
}  // namespace

int main(int, char**) {
  test_case("memory_range_t::create_from_start_and_size", [] {
    uint8_t mem[16];
    auto    range = dawn::memory_range_t::create_from_start_and_size(
        mem, 16, dawn::memory_protection_t::e_read);
    assert(range._start == mem);
    assert(range._end == mem + 16);
    assert(range._protection == dawn::memory_protection_t::e_read);
  });

  test_case("memory_t::create", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    assert(memory._host_base == mem);
    assert(memory._ranges.size() == 1);
    const auto& range = *memory._ranges.begin();
    assert(range._start == mem);
    assert(range._end == mem + 64);
    assert(range._protection == dawn::memory_protection_t::e_none);
  });

  test_case("address translation", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x1000;

    void*           host_addr  = mem + 10;
    dawn::address_t guest_addr = memory.translate_host_to_guest(host_addr);
    assert(guest_addr == 0x1000 + 10);

    void* translated_host_addr = memory.translate_guest_to_host(guest_addr);
    assert(translated_host_addr == host_addr);
  });

  test_case("insert_memory: overlap", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem + 16, 16, dawn::memory_protection_t::e_read);
    assert(memory._ranges.size() == 3);
  });

  test_case("insert_memory: fully contains another range", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem, 16, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 16, 16, dawn::memory_protection_t::e_write);
    // this should overwrite the original e_none range
    memory.insert_memory(mem, 64, dawn::memory_protection_t::e_exec);
    assert(memory._ranges.size() == 1);
    assert(memory._ranges.begin()->_protection ==
           dawn::memory_protection_t::e_exec);
  });

  test_case("insert_memory: contained within another range (split)", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem + 16, 32, dawn::memory_protection_t::e_read_write);
    assert(memory._ranges.size() == 3);

    auto it = memory._ranges.begin();
    assert(it->_start == mem && it->_end == mem + 16 &&
           it->_protection == dawn::memory_protection_t::e_none);
    it++;
    assert(it->_start == mem + 16 && it->_end == mem + 48 &&
           it->_protection == dawn::memory_protection_t::e_read_write);
    it++;
    assert(it->_start == mem + 48 && it->_end == mem + 64 &&
           it->_protection == dawn::memory_protection_t::e_none);
  });

  test_case("insert_memory: overlap end", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 16, 32, dawn::memory_protection_t::e_write);
    assert(memory._ranges.size() == 3);
  });

  test_case("is_region_in_memory", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem, 16, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 16, 16, dawn::memory_protection_t::e_write);
    memory.insert_memory(mem + 32, 16, dawn::memory_protection_t::e_exec);
    memory.insert_memory(mem + 48, 16, dawn::memory_protection_t::e_read_write);

    assert(
        memory.is_region_in_memory(mem, 16, dawn::memory_protection_t::e_read));
    assert(!memory.is_region_in_memory(mem, 17,
                                       dawn::memory_protection_t::e_read));
    assert(memory.is_region_in_memory(mem + 16, 16,
                                      dawn::memory_protection_t::e_write));
    assert(memory.is_region_in_memory(mem + 48, 16,
                                      dawn::memory_protection_t::e_read));
    assert(memory.is_region_in_memory(mem + 48, 16,
                                      dawn::memory_protection_t::e_write));
    assert(memory.is_region_in_memory(mem + 48, 16,
                                      dawn::memory_protection_t::e_read_write));
    assert(!memory.is_region_in_memory(mem + 48, 16,
                                       dawn::memory_protection_t::e_read_exec));
  });

  test_case("memory access: load/store", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x4000;
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 32, 32, dawn::memory_protection_t::e_write);

    // test read from read-only
    assert(memory.load_8(0x4000).has_value());
    assert(memory.load_16(0x4000).has_value());
    assert(memory.load_32(0x4000).has_value());
    assert(memory.load_64(0x4000).has_value());
    assert(memory.load_64(0x4018).has_value());
    assert(memory.load_64(0x4019).has_value() ==
           false);  // 1 byte over in w only zone
    assert(memory.load_64(0x401a).has_value() ==
           false);  // 2 bytes over in w only zone

    // test write to read-only (should fail)
    assert(!memory.store_8(0x4010, 0xab));

    // test write to write-only
    assert(memory.store_8(0x4020, 0xcd));
    assert(memory.store_16(0x4022, 0xefbe));
    assert(memory.store_32(0x4024, 0xaddebeaf));
    assert(memory.store_64(0x4028, 0xaddebeafaddebeaf));

    // test read from write-only (should fail)
    assert(!memory.load_8(0x4020).has_value());
  });

  test_case("memory access: load/store 2", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x4000;
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 32, 32, dawn::memory_protection_t::e_read);

    assert(memory.load_64(0x4018).has_value());
    assert(memory.load_64(0x4019).has_value());  // 1 byte in next r zone
    assert(memory.load_64(0x401a).has_value());  // 2 bytes in next r zone
  });

  test_case("memory access: fetch", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x8000;
    memory.insert_memory(mem, 64, dawn::memory_protection_t::e_read_exec);

    assert(memory.fetch_32(0x8000).has_value());
    assert(!memory.fetch_32(0x9000).has_value());  // out of bounds
  });

  test_case("memcpy and memset", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x1000;
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 32, 32, dawn::memory_protection_t::e_write);

    std::memset(mem, 0xaa, 64);

    uint8_t buffer[16];
    assert(memory.memcpy_guest_to_host(buffer, 0x1000, 16));
    assert(std::memcmp(buffer, mem, 16) == 0);
    assert(
        !memory.memcpy_guest_to_host(buffer, 0x1020, 16));  // dst is write-only

    uint8_t data[] = "hello world";
    assert(memory.memcpy_host_to_guest(0x1020, data, sizeof(data)));
    assert(std::memcmp(mem + 32, data, sizeof(data)) == 0);
    assert(!memory.memcpy_host_to_guest(0x1000, data,
                                        sizeof(data)));  // dst is read-only

    assert(memory.memset(0x1020, 0xcc, 16));
    for (int i = 0; i < 16; ++i) {
      assert(mem[32 + i] == 0xcc);
    }
    assert(!memory.memset(0x1000, 0, 16));  // dst is read-only
  });

  test_case("insert_memory: adjacent ranges not merged", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 32, 32, dawn::memory_protection_t::e_read);
    assert(memory._ranges.size() == 2);
  });

  test_case("memory access: load_64 spanning protection change", [] {
    uint8_t        mem[64];
    dawn::memory_t memory = dawn::memory_t::create(mem, 64);
    memory.guest_base     = 0x4000;
    memory.insert_memory(mem, 32, dawn::memory_protection_t::e_read);
    memory.insert_memory(mem + 32, 32, dawn::memory_protection_t::e_write);

    // this should fail because it tries to read from 0x401c to 0x4024,
    // crossing from a read-only to a write-only boundary.
    assert(memory.load_64(0x401c).has_value() == false);
    // this should succeed as it's fully in the readable region.
    assert(memory.load_64(0x4000).has_value() == true);
  });

  return test_passed ? 0 : 1;
}
