#include <iostream>
#include <stdexcept>

#include "machine.hpp"

static bool running = true;

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("Error: [dawn] [elf]");
  dawn::machine_t machine{1024 * 1024 * 1, 1024};
  // newlib exit
  machine.set_syscall(93, [](dawn::machine_t& machine) {
    running = false;
    exit(machine._registers[10]);
  });
  // my_print
  machine.set_syscall(1000, [](dawn::machine_t& machine) {
    uint64_t i = 0;
    while (char ch = machine._memory.load<8>(machine._registers[10] + i++)) {
      std::cout << ch;
    }
  });
  machine.load_elf_and_set_program_counter(argv[1]);
  while (running) {
    auto [instruction, program_counter] =
        machine.fetch_instruction_at_program_counter();
    machine.decode_and_execute_instruction(instruction);
  }
  return 0;
}
