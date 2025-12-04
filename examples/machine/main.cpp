#include <cstdio>
#include <iostream>
#include <stdexcept>

#include "dawn/machine.hpp"

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("[machine] [elf]!");
  auto m = dawn::machine_t::load_elf(argv[1]);
  if (!m) {
    throw std::runtime_error("failed to load elf");
  }
  auto machine = *m;

  machine.add_syscall(93, [](dawn::machine_t& machine) {
    machine._running = false;
    if (machine._reg[10] == 0)
      std::cout << "passed\n";
    else
      std::cout << "failed\n";
    exit(machine._reg[10]);
  });

  machine.simulate(10000);
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

  return machine._running ? 1 : machine._reg[10];
  return -1;
}
