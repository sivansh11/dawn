/* compiled using
 * riscv64-unknown-elf-g++ simple_riscv_main.cpp
 */

#include <sched.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

// macro to simplify creating custom syscall functions
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

// Note: These custom syscalls can be alot alot more complex, for example render
// a model at location x, y, z. There is no hard limit for number of paramters
// that can be passed, but we do need to follow the riscv abi
define_syscall(1001, get_mapped_memory, void *());

// NOTE: no need to define newlib syscalls as they are handle by the
// compiler

int main() {
  // Note: this wont work since fork syscall, ie syscall 57 is not handled by
  // this example (exmaples/simple/main.cpp)
  //
  // fork();

  uint8_t *mapped_memory = reinterpret_cast<uint8_t *>(get_mapped_memory());

  // Note: std::cout works because write syscall, ie syscall number 64 is
  // handled by the exmaple
  std::cout << "initial mapped memory as seen in guest\n";
  for (uint32_t i = 0; i < 64; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (uint32_t)mapped_memory[i] << ' ';
    if ((i + 1) % 4 == 0) std::cout << " ";
    if ((i + 1) % 8 == 0) std::cout << '\n';
  }

  // the string to be copied into the shared memory
  const char *msg = "hello world, from riscv";

  // copy string to mapped memory
  std::memcpy(mapped_memory, msg, std::strlen(msg) + 1);

  std::cout << "mapped memory after writes as seen in guest\n";
  for (uint32_t i = 0; i < 64; i++) {
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (uint32_t)mapped_memory[i] << ' ';
    if ((i + 1) % 4 == 0) std::cout << " ";
    if ((i + 1) % 8 == 0) std::cout << '\n';
  }

  // 0 to indicate success
  return 0;
}
