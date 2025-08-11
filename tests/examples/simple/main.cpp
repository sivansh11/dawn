#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <utility>

// macro to generate syscall
// taken from https://github.com/libriscv/libriscv
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

// custom syscall to get shared memory
// NOTE: no need to define newlib syscalls as they are called internally by the
// compiler
define_syscall(1000, get_mapped_memory, void *());

int main() {
  // get shared memory
  uint8_t *mapped_memory = reinterpret_cast<uint8_t *>(get_mapped_memory());

  // print the initial contents of the shared memory
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

  // 0 to indicate success
  return 0;
}
