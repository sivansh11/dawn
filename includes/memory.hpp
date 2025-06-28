#ifndef DAWN_MEMORY_HPP
#define DAWN_MEMORY_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace dawn {

/*
 * invalid_address will be returned if
 * translate_virtual_address_to_physical_address fails
 */
constexpr uint64_t invalid_address = std::numeric_limits<uint64_t>::max();

/*
 * create a memory of size and define pages of page_size
 */
struct memory_t {
  memory_t(uint64_t size, uint64_t page_size);
  ~memory_t();

  /*
   * returns virtual_page_number, offset and physical_address
   */
  std::tuple<uint64_t, uint64_t, uint64_t>
  _translate_virtual_address_to_physical_address(uint64_t virtual_address);
  /*
   * helper, calls _translate_virtual_address_to_physical_address internally,
   * ignores physical_page_number and offset
   */
  uint64_t translate_virtual_address_to_physical_address(
      uint64_t virtual_address);

  /*
   * copies host memory to guest, returns false if it failes for any reason
   */
  bool memcpy_host_to_guest(uint64_t    guest_dst_virtual_address,
                            const void *host_src_address, uint64_t size);
  /*
   * copies guest memory to host, returns false if it failes for any reason
   */
  bool memcpy_guest_to_host(void    *host_dst_address,
                            uint64_t guest_src_virtual_address, uint64_t size);

  // clang-format off
  template <size_t size>
  uint64_t load(uint64_t virtual_address) {
    if constexpr (size == 8) return _load_8(virtual_address);
    else if constexpr (size == 16) return _load_16(virtual_address);
    else if constexpr (size == 32) return _load_32(virtual_address);
    else if constexpr (size == 64) return _load_64(virtual_address);
    else throw std::runtime_error("Error: unknown size load");
  }
  template <size_t size>
  void store(uint64_t virtual_address, uint64_t value) {
    if constexpr (size == 8) _store_8(virtual_address, value);
    else if constexpr (size == 16) _store_16(virtual_address, value);
    else if constexpr (size == 32) _store_32(virtual_address, value);
    else if constexpr (size == 64) _store_64(virtual_address, value);
    else throw std::runtime_error("Error: unknown size store");
  }
  // clang-format on

  uint64_t _load_8(uint64_t virtual_address);
  uint64_t _load_16(uint64_t virtual_address);
  uint64_t _load_32(uint64_t virtual_address);
  uint64_t _load_64(uint64_t virtual_address);

  void _store_8(uint64_t virtual_address, uint64_t value);
  void _store_16(uint64_t virtual_address, uint64_t value);
  void _store_32(uint64_t virtual_address, uint64_t value);
  void _store_64(uint64_t virtual_address, uint64_t value);

  uint64_t _size;
  uint64_t _page_size;
  uint8_t *_data;
  // TODO: look for better implementations of _page_table, and a better page
  // allocation scheme
  std::unordered_map<uint64_t, uint64_t> _page_table;
  uint64_t                               _next_free_page = 0;
};

}  // namespace dawn

#endif  // !DAWN_MEMORY_HPP
