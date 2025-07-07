#include <bitset>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "machine.hpp"

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("Error: [dawn] [elf]");
  dawn::machine_t machine{1024 * 1024 * 1, 1024};
  machine.set_syscall(
      93, [](dawn::machine_t& machine) { exit(machine._registers[10]); });
  machine.set_syscall(1000, [](dawn::machine_t& machine) {
    uint64_t i = 0;
    while (char ch = machine._memory.load<8>(machine._registers[10] + i++)) {
      std::cout << ch;
    }
  });
  machine.load_elf_and_set_program_counter(argv[1]);
  bool     running = true;
  uint32_t address = std::numeric_limits<uint32_t>::max();
  if (argc == 3) {
    address = static_cast<uint32_t>(std::stoul(argv[2], nullptr, 16));
  }
  bool          step = false;
  std::ofstream log{"log", std::ios_base::trunc};
  if (!log.is_open())
    throw std::runtime_error("Error: could not open log file");
  std::ofstream log2{"log2", std::ios_base::trunc};
  if (!log2.is_open())
    throw std::runtime_error("Error: could not open log2 file");
  while (running) {
    auto [instruction, program_counter] =
        machine.fetch_instruction_at_program_counter();
    log << std::hex << program_counter << ' ' << std::dec << std::hex
        << std::setfill('0') << std::setw(8) << instruction << std::dec << " ";
    log2 << std::hex << program_counter << '\n';
    machine.debug_disassemble_instruction(instruction, log);
    log << "Program Counter: " << std::hex << machine._program_counter << '\n';
    log << "Registers: \n";
    for (uint32_t i = 0; i < 32; i++) {
      log << '\t' << std::setw(2) << std::dec << i << " : " << std::hex
          << machine._registers[i] << '\n';
    }
    log.flush();
    log2.flush();
    if (!step) {
      if (program_counter == address) {
        getchar();
        step = true;
      }
    } else {
      getchar();
    }
    machine.decode_and_execute_instruction(instruction);
  }
  return 0;
}
