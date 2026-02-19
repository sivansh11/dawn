/* compiled using
 * riscv64-unknown-elf-g++ simple_riscv_main.cpp
 */

#include <sched.h>
#include <unistd.h>
#include <cstdint>
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

// custom syscall to get number of pages mapped.
define_syscall(1000, get_number_of_pages_mapped, uint64_t());
// Note: These custom syscalls can be alot alot more complex, for example render
// a model at location x, y, z. There is no hard limit for number of paramters
// that can be passed, but we do need to follow the riscv abi

// NOTE: no need to define newlib syscalls as they are handle by the
// compiler

int main() {
  // performs custom sycall
  uint64_t number_of_pages_mapped = get_number_of_pages_mapped();

  // Note: std::cout works because write syscall, ie syscall number 64 is
  // handled by the exmaple
  std::cout << "hello wolrd, in riscv\n";
  std::cout << "there are " << number_of_pages_mapped
            << " pages in current riscv proccess\n";

  // Note: this wont work since fork syscall, ie syscall 57 is not handled by
  // this example (exmaples/simple/main.cpp)
  //
  // fork();

  // 0 to indicate success
  return 0;
}
