## A RISC-V Backend for a Scripting Engine

Seeing this title, you may have a lot of questions, like "What? Why? Why not WASM?" All of these are valid questions, and in this article, I will be answering them and a lot more!

## Core Principles

The end goal of this project is to have a portable, performant, safe, and non-opinionated scripting backend, primarily for use in a game engine.

*   **Portable:** Scripts should be compiled once and run anywhere the engine runs. We should be able to share the compiled script binaries without worrying about the host's architecture or operating system.
*   **Performant:** The scripts must execute at the highest possible performance with little to no overhead.
*   **Safe:** As the system is intended to support modding, it must be a secure sandbox. Scripts should not be able to affect the host system outside of the controlled environment I provide.
*   **Non-Opinionated:** The system should not restrict developers to a single language. Any language that can target the chosen architecture, like C, C++, or Rust, should be usable.

## Why Not WASM?

To be honest, WASM fits most of these criteria. It was designed to be portable and safe, has performant runtimes, and can be targeted by most system languages.

However, WASM is a stack-based virtual machine. Real hardware is almost universally register-based. This mismatch means that it's challenging for hardware to efficiently pipeline and execute stack-based instructions. While this is often solved with JIT (Just-In-Time) compilation, it adds complexity and can make the unoptimized, interpreted performance slower. I want my interpreter to be as fast as possible from the start.

For a great explanation of stack vs. register machines, check out [this video](https://www.youtube.com/watch?v=cMMAGIefZuM).

## Introducing RISC-V

This is where RISC-V comes in. It's an open-standard instruction set architecture (ISA) that perfectly aligns with our core principles.

*   **It's a Register Machine:** As a modern RISC architecture, it's designed to map efficiently to hardware, which allows for a more performant interpreter.
*   **It's an Open Standard:** The base instruction set is frozen and has a wide variety of standard extensions (like for multiplication/division, atomic operations, etc.). This means we can create a minimal, custom "profile" for our scripts to target, ensuring portability.
*   **It's Simple:** The base integer ISA is so simple you can fit it on a single sheet of paper. This drastically reduces the complexity of building an emulator.

## The Execution Environment

To run RISC-V binaries, we need an emulator that acts as a virtual machine. This emulator is our sandbox, giving us complete control over how the script executes and interacts with the outside world.

A diagram of the architecture:
```
+--------------------------------+
|       Host Application         |
|        (Game Engine)           |
+--------------------------------+
             ^
             | C++ Function Calls
             v
+--------------------------------+
|       Dawn RISC-V Emulator     |
|  (Manages Memory & `ecall`s)   |
+--------------------------------+
             ^
             | `ecall` instruction
             v
+--------------------------------+
|        Guest Script            |
|      (RISC-V Binary)           |
+--------------------------------+
```

### Privilege and Machine Mode

The RISC-V specification defines several privilege levels (User, Supervisor, and Machine). A full-fledged operating system would use these to isolate processes from each other and from the kernel. However, for a scripting environment, this is unnecessary complexity.

My emulator implements only **Machine Mode**, the highest privilege level. While this sounds like it gives the script a lot of power, it's confined entirely within the emulator. The script thinks it has full control of the "hardware," but that hardware is completely virtual and managed by the host application. This simplifies the design immensely—there's no need to emulate a complex Memory Management Unit (MMU) or handle context switches.

### The ELF Format and Memory

ELF (Executable and Linkable Format) is a widely used standard for storing compiled programs. An ELF file contains the machine code, data, and metadata crucial for the program's execution.

A key aspect of ELF is its organization into loadable sections. These sections represent different parts of the program, such as:

- .text: Contains the executable machine code instructions.

- .data: Holds initialized global and static variables.

- .rodata: Stores read-only data, like string literals.

- .bss: Reserves space for uninitialized global and static variables (these don't take up space in the file but are allocated at runtime and zeroed out).

The ELF file also includes an ELF header and a program header table. The ELF header provides a "roadmap" to the file's structure, including information about the entry point, which indicates where the program execution should begin. The program header table, in particular, describes how the various loadable sections (often grouped into segments) should be placed into memory when the program is run. Each entry in this table specifies the size, file offset, and memory location for a particular segment.

While ELF offers robust capabilities for complex memory management and shared libraries, simpler execution environments might benefit from alternative formats. One such alternative is BFLT (Binary Flat Format). BFLT is a lightweight format that streamlines the loading process by typically consolidating all code and data into a single, contiguous block. This eliminates the need for intricate memory mapping mechanisms, making it trivial to load and potentially faster for specific use cases, especially in environments without a Memory Management Unit (MMU).

### Memory Management and Sharing

The guest's memory is not one single, monolithic block. Instead, the emulator manages a collection of memory ranges. This design allows for powerful and efficient ways for the host application to interact with the guest script.

The core of this system is the `memory_t::insert_memory(uintptr_t address, size_t size)` function. The host can allocate a piece of its own memory (e.g., using `new` or `malloc`) and then call `insert_memory` to make that specific block of host memory accessible to the guest. The emulator maintains a list of these valid memory ranges.

When the guest program tries to access a memory address, the emulator first checks if the address falls within any of the registered ranges. This includes the main emulated RAM (which is itself just another range inserted at startup) and any shared memory blocks the host has inserted.

This approach provides a highly efficient mechanism for sharing data. Instead of copying data back and forth, the host and guest can operate on the exact same memory. The process looks like this:

1.  **Host Allocates Memory:** The host application allocates a block of memory. This memory might contain game state, a frame buffer, or any other data structure.
2.  **Host Inserts Memory:** The host calls `machine._memory.insert_memory(...)` with the pointer and size of the allocated block. The emulator adds this host memory region to the guest's list of accessible memory ranges.
3.  **Host Provides Address to Guest:** The raw host pointer is meaningless to the guest. The host must use a function like `machine._memory.translate_host_to_guest_virtual(...)` to get a guest-visible virtual address for the shared block. This virtual address is then passed to the guest, typically via a custom syscall.
4.  **Guest Accesses Memory:** The guest can now use standard pointer operations to read from and write to the shared memory block using the virtual address it received. The emulator transparently handles the address translation, ensuring the guest's operations are safely applied to the correct host memory region.

### System Calls: The Bridge to the Host

A sandboxed script is useless if it can't interact with the host. It needs to be able to print messages, request memory, or interact with game world objects. In RISC-V, the `ecall` (environment call) instruction is used for this purpose.

When a script executes an `ecall`, the emulator traps it and pauses execution. It then inspects the script's registers to determine what it wants to do (e.g., register `a7` might hold the syscall number, and `a0-a2` might hold arguments).

This creates a secure and well-defined bridge between the script and the engine. Here’s a practical example:

**Host (C++) side, setting up shared memory and a custom syscall:**
This example demonstrates how the host can allocate a block of memory and make it accessible to the guest program. This is a powerful feature for allowing scripts to interact with host data structures.
```cpp
// Host-side example from examples/simple/main.cpp
#include <cstring>
#include <iomanip>
#include <iostream>
#include "machine.hpp"

// Syscall handlers for exit
void exit_handler(dawn::machine_t& machine) { machine._running = false; }

int main(int argc, char** argv) {
  if (argc < 2) return 1;

  // Initialize the machine
  dawn::machine_t machine{1024 * 1024, 1024};

  // Register standard syscalls
  machine.set_syscall(93, exit_handler);    // exit

  // Allocate a 64-byte shared memory region on the host.
  uint8_t* shared_memory = new uint8_t[64];
  std::memset(shared_memory, 0xFF, 64);

  // Map the shared memory into the machine's address space.
  machine._memory.insert_memory(reinterpret_cast<uintptr_t>(shared_memory), 64);

  // Register a custom syscall (1002) for the guest to get the
  // virtual address of the shared memory.
  machine.set_syscall(1002, [shared_memory](dawn::machine_t& machine) {
    machine._registers[10] = machine._memory.translate_host_to_guest_virtual(
        reinterpret_cast<uintptr_t>(shared_memory));
  });

  // Load the guest ELF binary
  machine.load_elf_and_set_program_counter(argv[1]);

  // Start the simulation
  machine.simulate();

  // After simulation, print the shared memory to see guest's changes
  std::cout << "After machine:\n" << (const char*)(shared_memory) << '\n';

  return machine._registers[10]; // Return guest's exit code
}
```

**Guest (C++) side, accessing the shared memory:**
The guest code uses the same `define_syscall` macro to create a C++ function for our custom syscall. It calls this function to get the pointer to the shared memory, writes a string into it, and then exits.
```cpp
// Guest-side example from tests/examples/simple/main.cpp
#include <cstdint>
#include <cstring>

// This macro defines a wrapper for a syscall.
#define define_syscall(code, name, signature)                 \
  asm(".pushsection .text\n"                                  \
      ".func sys_" #name "\n"                                 \
      "sys_" #name ":\n"                                      \
      "   li a7, " #code "\n"                                 \
      "   ecall\n"                                            \
      "   ret\n"                                              \
      ".endfunc\n"                                            \
      ".popsection .text\n");                                 \
  using name##_t = signature;                                 \
  extern "C" __attribute__((used, retain)) void sys_##name(); \
  template <typename... args_t>                               \
  static inline auto name(args_t &&...args) {                 \
    auto fn = (name##_t *)sys_##name;                         \
    return fn(std::forward<args_t>(args)...);                 \
  }

// Define the `get_mapped_memory` syscall (number 1002) which takes no arguments
// and returns a void pointer.
define_syscall(1002, get_mapped_memory, void *());

int main() {
  // Call the syscall to get the guest virtual address of the shared memory.
  uint8_t *mapped_memory = reinterpret_cast<uint8_t *>(get_mapped_memory());

  // The string to be copied into the shared memory.
  const char *msg = "hello world, from riscv";

  // Copy the string into the shared memory. The host will see this change.
  std::memcpy(mapped_memory, msg, std::strlen(msg) + 1);

  // Return 0 to indicate successful execution.
  return 0;
}
```

This same mechanism is used to provide a minimal C standard library (`newlib`) implementation, handling `exit`, memory allocation (`brk`), and other essential functions.
For a more complete look at host and guest code, take a look at simple [host](https://github.com/sivansh11/dawn/blob/main/examples/simple/main.cpp) and simple [guest](https://github.com/sivansh11/dawn/blob/main/tests/examples/simple/main.cpp).

## Compiling Guest Code

To compile C/C++ code (or any other language) for our RISC-V emulator, we need a **cross-compiler**. A cross-compiler runs on one architecture (e.g., x86-64 Linux) but generates executable code for a different architecture (e.g., RISC-V 64-bit).

For RISC-V, the standard cross-compiler toolchain is typically prefixed with `riscv64-unknown-elf-`. This indicates that it targets a 64-bit RISC-V architecture, for an "unknown" operating system (meaning it's a bare-metal or embedded target, without a full OS like Linux), and produces ELF-formatted executables.

Currently, when compiling guest code, you must specify the instruction set architecture and application binary interface. Since the emulator currently only supports the base integer instruction set, the following flags are required:

```bash
-march=rv64i -mabi=lp64
```

*   `-march=rv64i`: Specifies the target architecture as RISC-V 64-bit with only the Integer (I) extension.
*   `-mabi=lp64`: Specifies the Application Binary Interface (ABI) as LP64, which means `long` and pointers are 64-bit, and `int` is 32-bit.

**Getting the Cross-Compiler:**

You can obtain the necessary RISC-V GNU Toolchain, which includes the `riscv64-unknown-elf-gcc` cross-compiler, from the official GitHub repository:

[https://github.com/riscv-collab/riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)

## Future Work

This project is just getting started. Here are some of the next steps I have planned:

*   **Implement More Extensions:** The first priority is to implement the 'M' and 'F' extension for integer multiplication, division and floating point arithmetic, which is crucial for most programs.
*   **Performance Optimizations:** I plan to explore more advanced interpreter designs, to speed up the instruction dispatch loop. Further down the line, a JIT compiler could be a possibility.

### What are your thoughts?

I'd love to hear your feedback, questions, or ideas in the comments below! If you found this article interesting, please consider sharing it.

You can also check out the project's source code (if it's public) on [GitHub](https://github.com/sivansh11/dawn).
