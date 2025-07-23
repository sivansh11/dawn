#include "types.hpp"

namespace dawn {

std::string to_string(const exception_code_t exception) {
  switch (exception) {
    case exception_code_t::e_instruction_address_misaligned:
      return "instruction_address_misaligned";
    case exception_code_t::e_instruction_access_fault:
      return "instruction_access_fault";
    case exception_code_t::e_illegal_instruction:
      return "illegal_instruction";
    case exception_code_t::e_breakpoint:
      return "breakpoint";
    case exception_code_t::e_load_address_misaligned:
      return "load_address_misaligned";
    case exception_code_t::e_load_access_fault:
      return "load_access_fault";
    case exception_code_t::e_store_address_misaligned:
      return "store_address_misaligned";
    case exception_code_t::e_store_access_fault:
      return "store_access_fault";
    case exception_code_t::e_ecall_u_mode:
      return "ecall_u_mode";
    case exception_code_t::e_ecall_s_mode:
      return "ecall_s_mode";
    case exception_code_t::e_ecall_m_mode:
      return "ecall_m_mode";
    default:
      throw std::runtime_error("Error: unknown exception");
  }
}

}  // namespace dawn
