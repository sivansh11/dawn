// riscv64-unknown-elf-g++ -march=rv32i -mabi=ilp32 print.cpp

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

define_syscall(1000, my_print, void(const char *));

int main() {
  int i = 64;
  std::cout << "Test\n";
  std::cout << "i: " << i << '\n';
  my_print("hello, world! from riscv\n");
  return 0;
}
