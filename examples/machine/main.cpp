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
  });

  machine.simulate(1000000);

  return machine._running ? 1 : machine._reg[10];
}
