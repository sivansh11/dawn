// riscv64-unknown-elf-g++ -march=rv32i -mabi=ilp32 print.cpp

#include <cstdint>
#include <iostream>
#include <utility>

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

define_syscall(1000, log_error, void(const char *));
define_syscall(1001, get_host_shared_memory, void *());

int main() {
  uint32_t *ptr = (uint32_t *)get_host_shared_memory();
  ptr[0]        = 5;
  ptr[1]        = 6;
  ptr[2]        = 69;
  return 0;
}
