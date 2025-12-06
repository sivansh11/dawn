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

std::vector<uint8_t> load(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open: " + path);
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("failed to read: " + path);
  }

  return buffer;
}

static dawn::machine_t machine;

int main(int argc, char** argv) {
  if (argc < 3) throw std::runtime_error("[linux] [elf] [dtb]!");
  auto m = dawn::machine_t::load_binary(argv[1], 1024 * 1024 * 8, 0x80000000);
  if (!m) {
    throw std::runtime_error("failed to load elf");
  }
  machine = *m;

  auto     dtb_data   = load(argv[2]);
  uint8_t* dtb_memory = new uint8_t[dtb_data.size()];
  std::memcpy(dtb_memory, dtb_data.data(), dtb_data.size());
  machine._memory.insert_memory(dtb_memory, dtb_data.size(),
                                dawn::memory_protection_t::e_all, nullptr,
                                nullptr);
  // TODO: this
  // dawn::address_t dtb_guest_addr =
  //     machine._memory.translate_host_to_guest(dtb_memory);
  // std::cout << "DTB loaded at guest address: 0x" << std::hex <<
  // dtb_guest_addr
  //           << std::dec << std::endl;
  // machine._reg[11] = dtb_guest_addr;

  machine._memory.insert_memory(
      machine._memory.translate_guest_to_host(0x10000000), 256,
      dawn::memory_protection_t::e_read_write,
      [](dawn::address_t addr, uint64_t value) {
        printf("%c", (int)value);
        fflush(stdout);
      },
      [](dawn::address_t addr) -> uint64_t { return 'a'; });

  // machine.simulate();

  std::ofstream log{"log", std::ios::trunc};
  assert(log.is_open());

  while (true) {
    auto instruction = machine._memory.fetch_32(machine._pc);
    log << std::hex << machine._pc << '\n';
    log.flush();
    // std::cout << "pc: " << std::hex << machine._pc << "\n" << std::dec;
    // if (instruction)
    //   std::cout << "instruction: " << std::hex << *instruction << '\n';
    // else
    //   std::cout << "Error getting instruction\n";
    if (instruction) machine.decode_and_exec_instruction(*instruction);
    // for (uint32_t i = 0; i < 32; i++) {
    //   if (machine._reg[i] != 0)
    //     std::cout << "x" << i << ": " << std::hex << machine._reg[i] <<
    //     std::dec
    //               << '\n';
    // }
    // std::cout << '\n';
    // std::cout.flush();
    // getchar();
  }

  return machine._running ? 1 : machine._reg[10];
}
