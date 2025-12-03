#include <cstdint>
#include <iostream>

#include "dawn/helper.hpp"
#include "dawn/machine.hpp"
#include "dawn/memory.hpp"

int main(int argc, char** argv) {
  if (argc != 2) dawn::error("Error: [simple] [elf]");
  dawn::machine_t machine;
  // TODO: load_elf should take in initial memory along with size
  if (auto opt = dawn::machine_t::load_elf(argv[1])) machine = *opt;

  // newlib close handler
  machine.add_syscall(57,
                      [](dawn::machine_t& machine) { machine._reg[10] = 0; });

  // newlib write handler
  machine.add_syscall(64, [](dawn::machine_t& machine) {
    int      vfd     = machine._reg[10];
    uint64_t address = machine._reg[11];
    size_t   len     = machine._reg[12];
    if (vfd == 1 || vfd == 2) {
      for (uint64_t i = address; i < address + len; i++) {
        std::cout << (char)*machine._memory.load_8(i);
      }
      machine._reg[10] = len;
    } else {
      machine._reg[10] = -9;
    }
  });

  // newlib fstat handler
  machine.add_syscall(80,
                      [](dawn::machine_t& machine) { machine._reg[10] = -38; });

  // newlib exit handler
  machine.add_syscall(93, [](dawn::machine_t& machine) {
    machine._running = false;
    // dont exit, since you will instantly exit host
    // exit(machine._reg[10]);
  });

  // newlib brk handler
  machine.add_syscall(214, [](dawn::machine_t& machine) {
    uint64_t new_end = machine._reg[10];
    if (new_end < machine._heap_address) new_end = machine._heap_address;
    machine._reg[10] = new_end;
  });

  // shared memory
  uint8_t* shared_memory = new uint8_t[64];
  machine._memory.insert_memory(shared_memory, 64,
                                dawn::memory_protection_t::e_read_write,
                                nullptr, nullptr);

  // could also have directly called memset(shared_memory, 0xff, 64);
  // this is equivalent
  machine._memory.memset(machine._memory.translate_host_to_guest(shared_memory),
                         0xff, 64);

  // custom syscall provides host with address of shared memory
  machine.add_syscall(1000, [shared_memory](dawn::machine_t& machine) {
    machine._reg[10] = machine._memory.translate_host_to_guest(shared_memory);
  });

  machine.simulate();

  std::cout << "After machine:\n";
  for (uint32_t i = 0; i < 64; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (uint32_t)shared_memory[i] << ' ';
    if ((i + 1) % 4 == 0) std::cout << " ";
    if ((i + 1) % 8 == 0) std::cout << '\n';
  }
  std::cout << (const char*)(shared_memory) << '\n';

  return 0;
}
