#include <cstdio>
#include <iostream>
#include <stdexcept>

#include "dawn/machine.hpp"
#include "dawn/memory.hpp"

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("[linux] [elf]!");
  auto m = dawn::machine_t::load_elf(argv[1]);
  if (!m) {
    throw std::runtime_error("failed to load elf");
  }
  auto machine = *m;

  machine._memory.insert_memory(
      machine._memory.translate_guest_to_host(0x10000000), 256,
      dawn::memory_protection_t::e_read_write,
      [](dawn::address_t addr, uint64_t value) {
        printf("%c", (int)value);
        fflush(stdout);
      },
      [](dawn::address_t addr) -> uint64_t { return 'a'; });

  machine.simulate();

  // while (true) {
  //   auto instruction = machine._memory.fetch_32(machine._pc);
  //   std::cout << "pc: " << std::hex << machine._pc << "\n" << std::dec;
  //   if (instruction)
  //     std::cout << "instruction: " << std::hex << *instruction << '\n';
  //   else
  //     std::cout << "Error getting instruction\n";
  //   if (instruction) machine.decode_and_exec_instruction(*instruction);
  //   for (uint32_t i = 0; i < 32; i++) {
  //     if (machine._reg[i] != 0)
  //       std::cout << "x" << i << ": " << std::hex << machine._reg[i] <<
  //       std::dec
  //                 << '\n';
  //   }
  //   std::cout << '\n';
  //   std::cout.flush();
  //   // getchar();
  // }

  return machine._running ? 1 : machine._reg[10];
}
