#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "machine.hpp"
#include "memory.hpp"

// Define the memory size for the emulated machine. 1MB.
static const uint64_t memory_size = 1024 * 1024 * 1;
// Define the page size. This is not currently used for memory protection but is
// planned for future use.
static const uint64_t page_size = 1024;

// newlib syscall handlers
// exit_handler called when syscall 93 encountered
void exit_handler(dawn::machine_t& machine) { machine._running = false; }

// brk_handler called when syscall 214 encountered
void brk_handler(dawn::machine_t& machine) {
  uint64_t new_end = machine._registers[10];
  if (new_end < machine._heap_address) new_end = machine._heap_address;
  machine._registers[10] = new_end;
}

// write_handler called when syscall 64 encountered
void write_handler(dawn::machine_t& machine) {
  int      vfd     = machine._registers[10];
  uint64_t address = machine._registers[11];
  size_t   len     = machine._registers[12];
  if (vfd == 1 || vfd == 2) {
    for (uint64_t i = address; i < address + len; i++) {
      std::cout << (char)*machine._memory.load<8>(i);
    }
    machine._registers[10] = len;
  } else {
    machine._registers[10] = -9;  // Corresponds to EBADF
  }
}

// close_handler called when syscall 57 encountered
void close_handler(dawn::machine_t& machine) { machine._registers[10] = 0; }

// fstat_handler called when syscall 80 encountered
void fstat_handler(dawn::machine_t& machine) { machine._registers[10] = -38; }

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("Error: [simple] [elf]");

  // handles the emulation
  dawn::machine_t machine{memory_size, page_size};

  // install syscall callbacks (these are special syscalls that are defined by
  // newlib)
  machine.set_syscall(93, exit_handler);   // exit
  machine.set_syscall(214, brk_handler);   // brk
  machine.set_syscall(64, write_handler);  // write
  machine.set_syscall(57, close_handler);  // close
  machine.set_syscall(80, fstat_handler);  // fstat

  // shared memory between host and guest
  uint8_t* shared_memory = new uint8_t[64];
  // memory inserted into guest with read_write protection
  machine.insert_external_memory(shared_memory, 64,
                                 dawn::memory_protection_t::e_read_write);
  // essentially just std::memset(shared_memory, 0xff, 64);
  // memory's memset memcpy* functions take guest virtual address rather than
  // host address
  machine._memory.memset(machine._memory.translate_host_to_guest_virtual(
                             reinterpret_cast<uintptr_t>(shared_memory)),
                         0xff, 64);

  // custom syscall to give guest address of shared memory
  machine.set_syscall(1002, [shared_memory](dawn::machine_t& machine) {
    // The guest needs its own virtual address for the shared memory, not the
    // host's raw pointer. This translates the host address to a guest-visible
    // virtual address.
    machine._registers[10] = machine._memory.translate_host_to_guest_virtual(
        reinterpret_cast<uintptr_t>(shared_memory));
  });

  // load elf
  machine.load_elf_and_set_program_counter(argv[1]);

  // simulate, can give number of instructions to simulate
  machine.simulate(/* 100 */);  // uncomment 100 to process 100 instructions

  // After the simulation, print the contents of the shared memory to observe
  // any changes made by the guest program.
  std::cout << "After machine:\n";
  for (uint32_t i = 0; i < 64; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (uint32_t)shared_memory[i] << ' ';
    if ((i + 1) % 4 == 0) std::cout << " ";
    if ((i + 1) % 8 == 0) std::cout << '\n';
  }
  std::cout << (const char*)(shared_memory) << '\n';

  // The exit code of the emulated program is typically in register a0 (x10).
  return machine._registers[10];
}
