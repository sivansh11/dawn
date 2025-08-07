#ifndef DAWN_HELPER_HPP
#define DAWN_HELPER_HPP

#include <cstdint>
#include <ostream>

namespace dawn {

constexpr inline uint32_t extract_bit_range(uint32_t value, uint32_t start,
                                            uint32_t end) {
  uint32_t num_bits = end - start + 1;
  uint32_t mask     = ((uint32_t)1u << num_bits) - 1;
  return (value >> start) & mask;
}

// constexpr inline int32_t sext(uint32_t value, uint32_t imm_bit_width) {
//   return (int32_t)(value << (32 - imm_bit_width)) >> (32 - imm_bit_width);
// }

constexpr inline int64_t sext(uint64_t value, uint32_t imm_bit_width) {
  return (int64_t)(value << (64 - imm_bit_width)) >> (64 - imm_bit_width);
}

}  // namespace dawn

#endif  // !DAWN_HELPER_HPP
