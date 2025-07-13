# A RISC-V Backend for a Scripting Engine

Seeing this title, you may have a lot of questions, like "What? Why? Why not WASM?" All of these are valid questions, and in this article, I will be answering them and a lot more!

# Core Principles

The end goal of this project is to have a portable, performant, safe, and non-opinionated scripting backend, primarily for use in a game engine.

*   **Portable:** Scripts should be compiled once and run anywhere the engine runs. We should be able to share the compiled script binaries without worrying about the host's architecture or operating system.
*   **Performant:** The scripts must execute at the highest possible performance with little to no overhead.
*   **Safe:** As the system is intended to support modding, it must be a secure sandbox. Scripts should not be able to affect the host system outside of the controlled environment I provide.
*   **Non-Opinionated:** The system should not restrict developers to a single language. Any language that can target the chosen architecture, like C, C++, or Rust, should be usable.

# Why Not WASM?

To be honest, WASM fits most of these criteria. It was designed to be portable and safe, has performant runtimes, and can be targeted by most system languages.

However, WASM is a stack-based virtual machine. Real hardware is almost universally register-based. This mismatch means that it's challenging for hardware to efficiently pipeline and execute stack-based instructions. While this is often solved with JIT (Just-In-Time) compilation, it adds complexity and can make the unoptimized, interpreted performance slower. I want my interpreter to be as fast as possible from the start.

For a great explanation of stack vs. register machines, check out [this video](https://www.youtube.com/watch?v=cMMAGIefZuM).

# Introducing RISC-V

This is where RISC-V comes in. It's an open-standard instruction set architecture (ISA) that perfectly aligns with our core principles.

*   **It's a Register Machine:** As a modern RISC architecture, it's designed to map efficiently to hardware, which allows for a more performant interpreter.
*   **It's an Open Standard:** The base instruction set is frozen and has a wide variety of standard extensions (like for multiplication/division, atomic operations, etc.). This means we can create a minimal, custom "profile" for our scripts to target, ensuring portability.
*   **It's Simple:** The base integer ISA is so simple you can fit it on a single sheet of paper. This drastically reduces the complexity of building an emulator.

# The Execution Environment

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

Compiled programs are typically stored in the ELF (Executable and Linkable Format). An ELF file contains the machine code, data, and metadata about how to load the program into memory. One crucial piece of metadata is the **virtual address**. In a typical OS, the loader and hardware work together to map these virtual addresses to actual physical addresses in RAM.

My emulator leverages this. It reads the ELF file and loads the program segments into its own emulated memory. However, since I control the entire environment, I can simplify memory management. By using a **custom linker script**, I can instruct the compiler to produce an ELF file where the virtual addresses are either identical to the physical addresses or follow a simple, predictable mapping. This avoids the need for a complex page table system, making the emulator faster and simpler.

As a potential future optimization, one could even abandon ELF in favor of a simpler format like **BFLT (Binary Flat Format)**. BFLT is a lightweight format that doesn't use virtual addresses, making it trivial to load.

### System Calls: The Bridge to the Host

A sandboxed script is useless if it can't interact with the host. It needs to be able to print messages, request memory, or interact with game world objects. In RISC-V, the `ecall` (environment call) instruction is used for this purpose.

When a script executes an `ecall`, the emulator traps it and pauses execution. It then inspects the script's registers to determine what it wants to do (e.g., register `a7` might hold the syscall number, and `a0-a2` might hold arguments).

This creates a secure and well-defined bridge between the script and the engine. Here’s a practical example:

**Host (C++) side, setting up the syscall handler:**
```cpp
// A more complete host-side example (from examples/machine/main.cpp)
// This demonstrates setting up the machine, defining syscalls,
// loading a guest binary, and running the emulation loop.
#include <iostream>
#include <stdexcept>
#include <iomanip> // Required for std::setw and std::hex

#include "machine.hpp"

static bool running = true;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " [memory size] [elf_file_path]";
    return 1;
  }

  // Initialize the machine with specified memory size and stack size
  // The memory size is taken from argv[1], stack size is 1024 bytes
  dawn::machine_t machine{std::stoul(argv[1]), 1024};

  // Syscall 93: exit (used by newlib)
  machine.set_syscall(93, [](dawn::machine_t& machine) {
    running = false;
    // The exit code is typically in register a0 (x10)
    exit(machine._registers[10]);
  });

  // Syscall 64: write (used by newlib for stdout/stderr)
  machine.set_syscall(64, [](dawn::machine_t& machine) {
    int      vfd     = machine._registers[10]; // File descriptor
    uint64_t address = machine._registers[11]; // Address of buffer
    size_t   len     = machine._registers[12]; // Length of buffer

    // Only handle stdout (fd 1) and stderr (fd 2)
    if (vfd == 1 || vfd == 2) {
      for (uint64_t i = address; i < address + len; i++) {
        std::cout << (char)machine._memory.load<8>(i);
      }
      // Return the number of bytes written in a0 (x10)
      machine._registers[10] = len;
    } else {
      // Return an error code for unsupported file descriptors
      machine._registers[10] = -9; // EBADF (Bad file descriptor)
    }
  });

  // Syscall 1000: log_error (custom syscall for guest to log errors)
  machine.set_syscall(1000, [](dawn::machine_t& machine) {
    // a0 (x10) holds the first argument, which is the address of the string
    uint64_t address = machine._registers[10]; 
    std::cout << "[GUEST ERROR]: ";
    // Read the null-terminated string from emulated memory and print it
    uint64_t i = 0;
    while (char ch = machine._memory.load<8>(address + i++)) {
      std::cout << ch;
    }
    std::cout << '\n';
  });

  // Load the guest ELF binary into the emulated memory
  // The path to the ELF file is taken from argv[2]
  machine.load_elf_and_set_program_counter(argv[2]);

  std::cout << "Starting emulation at program counter: 0x" 
            << std::hex << machine._program_counter << std::dec << '\n';

  // Main emulation loop
  while (running) {
    // Fetch the instruction at the current program counter
    auto [instruction, program_counter] =
        machine.fetch_instruction_at_program_counter();
    // Decode and execute the fetched instruction
    machine.decode_and_execute_instruction(instruction);
  }

  std::cout << "Emulation finished.\n";
  return 0;
}
```

**Guest (C++) side, making the `ecall`:**
This more advanced example uses a C++ macro to automatically generate the assembly for a syscall and wrap it in a type-safe C++ function. This makes defining and using new syscalls much cleaner.
```cpp
// From tests/examples/print/print.cpp
#define define_syscall(code, name, signature)                 \
  asm(".pushsection .text\n"                                  \
      ".func sys_" #name                                      \
      "\n"                                                    \
      "sys_" #name                                            \
      ":\n"                                                   \
      "   li a7, " #code                                      \
      "\n"                                                    \
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

// Use the macro to create a 'log_error' function for syscall 1000
define_syscall(1000, log_error, void(const char *));

int main() {
  // The familiar std::cout still works because the emulator
  // implements the necessary syscalls for newlib.
  std::cout << "test\n";

  // And our custom syscall is now available as a C++ function.
  log_error("This is a custom error message from the script!");
  return 0;
}
```

This same mechanism is used to provide a minimal C standard library (`newlib`) implementation, handling `exit`, memory allocation (`brk`), and other essential functions.

# Compiling Guest Code

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

# Future Work

This project is just getting started. Here are some of the next steps I have planned:

*   **Implement More Extensions:** The first priority is to implement the 'M' and 'F' extension for integer multiplication, division and floating point arithmetic, which is crucial for most programs.
*   **Performance Optimizations:** I plan to explore more advanced interpreter designs, to speed up the instruction dispatch loop. Further down the line, a JIT compiler could be a possibility.
