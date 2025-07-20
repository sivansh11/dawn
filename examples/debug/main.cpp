#include <bitset>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
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
  machine.load_elf_and_set_program_counter(argv[1]);
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
    if (instruction) {
      log << std::hex << program_counter << ' ' << std::dec << std::hex
          << std::setfill('0') << std::setw(8) << *instruction << std::dec
          << " ";
      log2 << std::hex << program_counter << '\n';
      log2.flush();
      machine.debug_disassemble_instruction(*instruction, log);
      log << "Program Counter: " << std::hex << machine._program_counter
          << '\n';
      log << "Registers: \n";
      for (uint32_t i = 0; i < 32; i++) {
        log << '\t' << std::setw(2) << std::dec << i << " : " << std::hex
            << machine._registers[i] << '\n';
      }
      log.flush();
      if (!step) {
        if (program_counter == address) {
          getchar();
          step = true;
        }
      } else {
        getchar();
      }
      machine.decode_and_execute_instruction(*instruction);
    } else {
      throw std::runtime_error("Error: failed to fetch instruction");
    }
  }
  return 0;
}
