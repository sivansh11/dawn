#include <iostream>
#include <stdexcept>

#include "dawn/state.hpp"

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("[state] [elf]!");
  auto s = dawn::state_t::load_elf(argv[1]);
  if (!s) {
    throw std::runtime_error("failed to load elf");
  }
  auto state = *s;

  state.add_syscall(93, [](dawn::state_t& state) {
    state._running = false;
    if (state._reg[10] == 0)
      std::cout << "passed\n";
    else
      std::cout << "failed\n";
  });

  state.simulate(1000000);

  return state._running ? 1 : state._reg[10];
}
