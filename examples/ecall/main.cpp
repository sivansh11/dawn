#include <cstdint>
#include <iostream>

#include "dawn/helper.hpp"
#include "dawn/machine.hpp"
#include "dawn/memory.hpp"

int main(int argc, char** argv) {
  if (argc != 2) dawn::error("Error: [ecall] [elf]");
  dawn::machine_t machine;
  // TODO: load_elf should take in initial memory along with size
  if (auto opt = dawn::machine_t::load_elf(argv[1])) machine = *opt;

  machine.add_syscall(93,
                      [](dawn::machine_t& machine) { exit(machine._reg[10]); });

  machine._memory.insert_memory(
      machine._memory.translate_guest_to_host(0x10000000), 8,
      dawn::memory_protection_t::e_read_write,
      [](dawn::address_t addr, uint64_t value) {
        printf("%c", (int)value);
        fflush(stdout);
      },
      nullptr);

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
  //   getchar();
  // }

  return 0;
}
