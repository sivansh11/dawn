#include <cassert>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <vector>

#include "dawn/machine.hpp"
#include "dawn/memory.hpp"

std::vector<uint8_t> read_file(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read file: " + file_path);
  }
  return buffer;
}

static dawn::machine_t machine;

int main(int argc, char** argv) {
  if (argc < 3) throw std::runtime_error("[linux] [elf] [dtb]!");

  uint64_t offset = 0x80000000;
  machine         = dawn::machine_t::create(1024 * 1024 * 8, offset - 0x1000);
  auto kernel     = read_file(argv[1]);
  if (!machine._memory.memcpy_host_to_guest(offset, kernel.data(),
                                            kernel.size()))
    throw std::runtime_error("failed to copy kernel to guest");
  machine._pc = offset;

  auto dtb = read_file(argv[2]);
  if (!machine._memory.memcpy_host_to_guest(offset - 0x1000, dtb.data(),
                                            dtb.size()))
    throw std::runtime_error("failed to copy dtb to guest");
  std::cout << "DTB loaded at: " << std::hex << offset + kernel.size() << '\n';
  machine._reg[11] = offset - 0x1000;

  machine._memory.insert_memory(
      machine._memory.translate_guest_to_host(0x10000000), 256,
      dawn::memory_protection_t::e_read_write,
      [](dawn::address_t addr, uint64_t value) {
        printf("%c", (int)value);
        fflush(stdout);
      },
      [](dawn::address_t addr) -> uint64_t { return 'a'; });

  machine.simulate();

  // std::ofstream log{"log", std::ios::trunc};
  // assert(log.is_open());
  //
  // while (true) {
  //   auto instruction = machine._memory.fetch_32(machine._pc);
  //   log << std::hex << machine._pc << '\n';
  //   log.flush();
  //   // std::cout << "pc: " << std::hex << machine._pc << "\n" << std::dec;
  //   // if (instruction)
  //   //   std::cout << "instruction: " << std::hex << *instruction << '\n';
  //   // else
  //   //   std::cout << "Error getting instruction\n";
  //   if (instruction) machine.decode_and_exec_instruction(*instruction);
  //   // for (uint32_t i = 0; i < 32; i++) {
  //   //   if (machine._reg[i] != 0)
  //   //     std::cout << "x" << i << ": " << std::hex << machine._reg[i] <<
  //   //     std::dec
  //   //               << '\n';
  //   // }
  //   // std::cout << '\n';
  //   // std::cout.flush();
  //   // getchar();
  // }

  return machine._running ? 1 : machine._reg[10];
}
