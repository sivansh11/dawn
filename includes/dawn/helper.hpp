#ifndef DAWN_HELPER_HPP
#define DAWN_HELPER_HPP

#include <cstdint>
#include <iostream>

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

constexpr inline void mul_64x64_u(uint64_t a, uint64_t b, uint64_t result[2]) {
  const uint64_t mask_32 = 0xffffffffull;

  uint64_t a_h = a >> 32;
  uint64_t a_l = a & mask_32;
  uint64_t b_h = b >> 32;
  uint64_t b_l = b & mask_32;

  uint64_t p0 = a_l * b_l;

  uint64_t p1 = a_l * b_h;

  uint64_t p2 = a_h * b_l;

  uint64_t p3 = a_h * b_h;

  uint64_t carry_to_high_32 = (p0 >> 32) + (p1 & mask_32) + (p2 & mask_32);

  result[0] = (p0 & mask_32) | (carry_to_high_32 << 32);
  result[1] = p3 + (p1 >> 32) + (p2 >> 32) + (carry_to_high_32 >> 32);
}

inline void error(const char* msg) {
#ifndef NDEBUG
  throw std::runtime_error(msg);
#else
  std::cerr << msg;
  abort();
#endif
}

}  // namespace dawn

#endif  // !DAWN_HELPER_HPP
