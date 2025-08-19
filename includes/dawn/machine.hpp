#ifndef DAWN_STATE_HPP
#define DAWN_STATE_HPP

#include <ankerl/unordered_dense.h>
#include <asmjit/core.h>
#include <asmjit/core/codeholder.h>
#include <asmjit/core/compiler.h>
#include <asmjit/x86/x86compiler.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>

#include "memory.hpp"
#include "types.hpp"

namespace dawn {

struct machine_t;

using syscall_t = std::function<void(machine_t&)>;
typedef void (*jitted_func_t)(machine_t*);

struct machine_t {
  static std::optional<machine_t> load_elf(const std::filesystem::path& path);
  bool                    add_syscall(uint64_t number, syscall_t syscall);
  bool                    del_syscall(uint64_t number);
  std::optional<uint32_t> fetch_instruction();
  bool                    decode_and_exec_instruction(uint32_t instruction);

  void simulate(
      uint64_t num_instructions = std::numeric_limits<uint64_t>::max());

 private:
  bool                    write_csr(uint32_t instruction, uint64_t value);
  std::optional<uint64_t> read_csr(uint32_t instruction);
  void handle_trap(riscv::exception_code_t cause, uint64_t value);

  bool decode_and_jit_basic_block(address_t pc);

  void     _write_csr(uint16_t address, uint64_t value);
  uint64_t _read_csr(uint16_t address);

 public:
  uint64_t                                          _reg[32];
  address_t                                         _pc;
  ankerl::unordered_dense::map<uint64_t, uint64_t>  _csr;
  ankerl::unordered_dense::map<uint64_t, syscall_t> _syscalls;
  memory_t                                          _memory;
  // TODO: maybe expose this to the user while loading an elf, then the user can
  // manually manage _brk syscall
  address_t                           _heap_address;
  bool                                _running = true;
  std::unique_ptr<asmjit::JitRuntime> _jitruntime{};
  ankerl::unordered_dense::map<address_t, std::pair<jitted_func_t, uint64_t>>
      _jitted_func{};
};

}  // namespace dawn

#endif  // !DAWN_STATE_HPP
