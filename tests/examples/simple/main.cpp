#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

// This macro defines a wrapper for a syscall. It generates the necessary
// assembly to perform an `ecall` with the specified syscall number. It also
// creates a C++ function with the correct signature to call the syscall.
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

// Define the `get_mapped_memory` syscall (number 1002) which takes no arguments
// and returns a void pointer.
define_syscall(1002, get_mapped_memory, void *());

int main() {
  // Call the syscall to get the guest virtual address of the shared memory.
  uint8_t *mapped_memory = reinterpret_cast<uint8_t *>(get_mapped_memory());

  // Print the initial contents of the shared memory.
  // This is to show what was in the memory before the guest program modified
  // it.
  for (uint32_t i = 0; i < 64; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (uint32_t)mapped_memory[i] << ' ';
    if ((i + 1) % 4 == 0) std::cout << " ";
    if ((i + 1) % 8 == 0) std::cout << '\n';
  }

  // The string to be copied into the shared memory.
  const char *msg = "hello world, from riscv";

  // Copy the string into the shared memory. The +1 includes the null
  // terminator. The host will be able to see this change after the emulation is
  // finished.
  std::memcpy(mapped_memory, msg, std::strlen(msg) + 1);

  // Return 0 to indicate successful execution.
  return 0;
}
