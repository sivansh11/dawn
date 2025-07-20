#include <cstring>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "machine.hpp"

// Define the memory size for the emulated machine. 1MB.
static const uint64_t memory_size = 1024 * 1024 * 1;
// Define the page size. This is not currently used for memory protection but is
// planned for future use.
static const uint64_t page_size = 1024;

/**
 * @brief Handles the 'exit' syscall (syscall 93).
 *
 * This function is called when the emulated program executes an `ecall` with
 * syscall number 93. It stops the machine's execution loop. The exit code is
 * expected to be in register a0 (x10).
 *
 * @param machine The machine instance.
 */
void exit_handler(dawn::machine_t& machine) { machine._running = false; }

/**
 * @brief Handles the 'brk' syscall (syscall 214).
 *
 * This function is called for the `brk` syscall, which is used to change the
 * program's data segment size. It adjusts the heap pointer. The new program
 * break is passed in register a0 (x10).
 *
 * @param machine The machine instance.
 */
void brk_handler(dawn::machine_t& machine) {
  uint64_t new_end = machine._registers[10];
  if (new_end < machine._heap_address) new_end = machine._heap_address;
  machine._registers[10] = new_end;
}

/**
 * @brief Handles the 'write' syscall (syscall 64).
 *
 * This function is called for the `write` syscall. It supports writing to
 * stdout (vfd 1) and stderr (vfd 2). The arguments are passed in registers:
 * a0 (x10): file descriptor
 * a1 (x11): buffer address
 * a2 (x12): buffer length
 *
 * @param machine The machine instance.
 */
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

/**
 * @brief Handles the 'close' syscall (syscall 57).
 *
 * This is a stub handler for the `close` syscall. It doesn't do anything but
 * returns 0 to indicate success.
 *
 * @param machine The machine instance.
 */
void close_handler(dawn::machine_t& machine) { machine._registers[10] = 0; }

/**
 * @brief Handles the 'fstat' syscall (syscall 80).
 *
 * This is a stub handler for the `fstat` syscall. It returns -38 (ENOSYS) to
 * indicate that the function is not implemented.
 *
 * @param machine The machine instance.
 */
void fstat_handler(dawn::machine_t& machine) { machine._registers[10] = -38; }

int main(int argc, char** argv) {
  if (argc < 2) throw std::runtime_error("Error: [simple] [elf]");

  // Initialize the RISC-V machine with the specified memory and page sizes.
  dawn::machine_t machine{memory_size, page_size};

  // Register handlers for newlib syscalls. These are invoked by `ecall`.
  machine.set_syscall(93, exit_handler);   // exit
  machine.set_syscall(214, brk_handler);   // brk
  machine.set_syscall(64, write_handler);  // write
  machine.set_syscall(57, close_handler);  // close
  machine.set_syscall(80, fstat_handler);  // fstat

  // Allocate a 64-byte shared memory region on the host.
  uint8_t* shared_memory = new uint8_t[64];
  std::memset(shared_memory, 0xFF, 64);

  // Map the shared memory into the machine's address space.
  // This makes the host memory accessible to the guest program.
  // Accessing memory outside of mapped regions will cause a runtime error.
  machine._memory.insert_memory(reinterpret_cast<uintptr_t>(shared_memory), 64);

  // Register a custom syscall (1002) to get the guest virtual address of the
  // shared memory.
  machine.set_syscall(1002, [shared_memory](dawn::machine_t& machine) {
    // The guest needs its own virtual address for the shared memory, not the
    // host's raw pointer. This translates the host address to a guest-visible
    // virtual address.
    machine._registers[10] = machine._memory.translate_host_to_guest_virtual(
        reinterpret_cast<uintptr_t>(shared_memory));
  });

  // Load the ELF executable into the machine's memory and set the program
  // counter to the entry point.
  machine.load_elf_and_set_program_counter(argv[1]);

  // Start the simulation. The machine will fetch, decode, and execute
  // instructions until `machine._running` is set to false (e.g., by the exit
  // syscall).
  machine.simulate();

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
