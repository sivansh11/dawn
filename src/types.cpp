#include "types.hpp"

std::ostream& operator<<(std::ostream&              o,
                         const dawn::instruction_t& instruction) {
  return o;
}

std::ostream& operator<<(std::ostream&                o,
                         const dawn::exception_code_t exception) {
  switch (exception) {
    case dawn::exception_code_t::e_instruction_address_misaligned:
      o << "instruction_address_misaligned";
    case dawn::exception_code_t::e_instruction_access_fault:
      o << "instruction_access_fault";
    case dawn::exception_code_t::e_illegal_instruction:
      o << "illegal_instruction";
    case dawn::exception_code_t::e_breakpoint:
      o << "breakpoint";
    case dawn::exception_code_t::e_load_address_misaligned:
      o << "load_address_misaligned";
    case dawn::exception_code_t::e_load_access_fault:
      o << "load_access_fault";
    case dawn::exception_code_t::e_store_address_misaligned:
      o << "store_address_misaligned";
    case dawn::exception_code_t::e_store_access_fault:
      o << "store_access_fault";
    case dawn::exception_code_t::e_ecall_u_mode:
      o << "ecall_u_mode";
    case dawn::exception_code_t::e_ecall_s_mode:
      o << "ecall_s_mode";
    case dawn::exception_code_t::e_ecall_m_mode:
      o << "ecall_m_mode";
    default:
      throw std::runtime_error("Error: unknown exception");
  }
}
