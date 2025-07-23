#ifndef DAWN_MACHINE_HPP
#define DAWN_MACHINE_HPP

#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <unordered_map>

#include "memory.hpp"
#include "types.hpp"

namespace dawn {

struct machine_t;

using syscall_t = std::function<void(machine_t&)>;

class machine_t {
 public:
  machine_t(uint64_t memory_size, uint64_t memory_page_size);
  ~machine_t();

  void set_syscall(uint64_t number, syscall_t syscall);

  bool load_elf_and_set_program_counter(const std::filesystem::path& path);

  std::pair<std::optional<uint32_t>, uint64_t>
       fetch_instruction_at_program_counter();
  void decode_and_execute_instruction(uint32_t instruction);

  void simulate(uint64_t steps = std::numeric_limits<uint64_t>::max());

  void insert_external_memory(void* ptr, size_t size,
                              memory_protection_t protection);

  // TODO: maybe these shouldnt be public ?
  std::array<uint64_t, 32> _registers;
  uint64_t                 _program_counter;
  // TODO: a better and faster unordered_map
  std::unordered_map<uint64_t, uint64_t> _csr;
  uint64_t                               _heap_address;
  bool                                   _running = true;
  memory_t                               _memory;

 private:
  void     handle_trap(exception_code_t cause_code, uint64_t value);
  uint64_t _read_csr(uint16_t address);
  void     _write_csr(uint16_t address, uint64_t value);
  uint64_t read_csr(uint32_t instruction);
  void     write_csr(uint32_t instruction, uint64_t value);

  privilege_mode_t _privilege_mode;
  // TODO: a better and faster unordered_map
  std::unordered_map<uint64_t, syscall_t> _syscalls;
  // internal
};

}  // namespace dawn

#endif  // !DAWN_MACHINE_HPP
