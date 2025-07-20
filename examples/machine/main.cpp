#include <iostream>
#include <stdexcept>

#include "machine.hpp"

static bool running = true;

int main(int argc, char** argv) {
  if (argc < 3) throw std::runtime_error("Error: [dawn] [memory size] [elf]");
  dawn::machine_t machine{std::stoul(argv[1]), 1024};
  // newlib exit
  machine.set_syscall(93, [](dawn::machine_t& machine) {
    running = false;
    exit(machine._registers[10]);
  });
  // newlib brk
  machine.set_syscall(214, [](dawn::machine_t& machine) {
    uint64_t new_end = machine._registers[10];
    std::cout << "new_end: " << std::hex << std::setw(8) << new_end
              << " _heap_address: " << std::setw(8) << machine._heap_address
              << " program_counter: " << std::setw(8)
              << machine._program_counter << std::dec << '\n';
    if (new_end < machine._heap_address) {
      new_end = machine._heap_address;
    }
    machine._registers[10] = new_end;
  });
  // newlib write
  machine.set_syscall(64, [](dawn::machine_t& machine) {
    int      vfd     = machine._registers[10];
    uint64_t address = machine._registers[11];
    size_t   len     = machine._registers[12];
    if (vfd == 1 || vfd == 2) {
      for (uint64_t i = address; i < address + len; i++) {
        std::cout << (char)*machine._memory.load<8>(i);
      }
      machine._registers[10] = len;
    } else {
      machine._registers[10] = -9;
    }
  });
  // newlib
  machine.set_syscall(
      57, [](dawn::machine_t& machine) { machine._registers[10] = 0; });
  // newlib fstat
  machine.set_syscall(
      80, [](dawn::machine_t& machine) { machine._registers[10] = -38; });
  // my_print
  machine.set_syscall(1000, [](dawn::machine_t& machine) {
    uint64_t i = 0;
    std::cout << "[ERROR]: ";
    while (char ch = *machine._memory.load<8>(machine._registers[10] + i++)) {
      std::cout << ch;
    }
  });
  machine.load_elf_and_set_program_counter(argv[2]);
  std::cout << std::hex << machine._program_counter << std::dec << '\n';
  while (running) {
    auto [instruction, program_counter] =
        machine.fetch_instruction_at_program_counter();
    if (instruction) machine.decode_and_execute_instruction(*instruction);
  }
  return 0;
}
