#include <iostream>
#include <stdexcept>

#include "dawn/state.hpp"

int main(int argc, char **argv) {
  if (argc < 2) throw std::runtime_error("[state] [elf]!");
  dawn::state_t state = *dawn::state_t::load_elf(argv[1]);

  while (true) {
    auto instruction = state.fetch_instruction();
    if (!instruction) throw std::runtime_error("Failed to fetch instruction");
    std::cout << std::hex << *instruction << std::dec << '\n';
    state.decode_and_exec_instruction(*instruction);
  }

  return 0;
}
