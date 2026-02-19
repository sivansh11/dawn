#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <elfio/elfio.hpp>
#include <limits>
#include <sstream>
#include <stdexcept>

#include "dawn/dawn.hpp"

uint8_t* allocate(void*, uint64_t size) { return new uint8_t[size]; }
void     deallocate(void*, uint8_t* ptr) { delete[] ptr; }

struct data_t {
  dawn::machine_t machine;
  uint64_t        heap_start;
  uint64_t        heap_end;
  uint64_t        stack_top;
  uint64_t        stack_bottom;
};

data_t* load_elf(const std::filesystem::path& path) {
  ELFIO::elfio reader;
  if (!reader.load(path)) return nullptr;

  uint64_t guest_base = std::numeric_limits<uint64_t>::max();
  uint64_t guest_max  = std::numeric_limits<uint64_t>::min();
  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();

    guest_base = std::min(guest_base, virtual_address);
    guest_max  = std::max(guest_max, virtual_address + memory_size);
  }

  data_t* data =
      new data_t{.machine = dawn::machine_t{16 * 1024 * 1024,
                                            {},
                                            nullptr,
                                            allocate,
                                            deallocate,
                                            dawn::page_permission_t::e_none}};

  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();
    bool              is_read         = segment->get_flags() & ELFIO::PF_R;
    bool              is_write        = segment->get_flags() & ELFIO::PF_W;
    bool              is_exec         = segment->get_flags() & ELFIO::PF_X;

    dawn::page_permission_t permission{};
    if (is_read) permission |= dawn::page_permission_t::e_r;
    if (is_write) permission |= dawn::page_permission_t::e_w;
    if (is_exec) permission |= dawn::page_permission_t::e_x;

    if (!data->machine.insert_memory(
            virtual_address, reinterpret_cast<const void*>(segment->get_data()),
            file_size, permission))
      return nullptr;
    if (memory_size - file_size) {
      if (!data->machine.set_memory(virtual_address + file_size, 0,
                                    memory_size - file_size,
                                    dawn::page_permission_t::e_rw))
        return nullptr;
    }

    data->machine.memcpy_host_to_guest(
        virtual_address, reinterpret_cast<const void*>(segment->get_data()),
        file_size);
    if (memory_size - file_size) {
      data->machine.memset(virtual_address + file_size, 0,
                           memory_size - file_size);
    }
  }
  // TODO: add a empty frame with no permission for preventing stack overflow

  // TODO: handle heap address
  for (uint32_t i = 0; i < reader.sections.size(); i++) {
    ELFIO::section* section = reader.sections[i];
    if (section->get_type() != ELFIO::SHT_SYMTAB) continue;
    const ELFIO::symbol_section_accessor symbols(reader, section);

    for (uint32_t j = 0; j < symbols.get_symbols_num(); j++) {
      std::string       name;
      ELFIO::Elf64_Addr value;
      ELFIO::Elf_Xword  size;
      unsigned char     bind;
      unsigned char     type;
      ELFIO::Elf_Half   section_index;
      unsigned char     other;

      symbols.get_symbol(j, name, value, size, bind, type, section_index,
                         other);
      if (name == "_end") {
        data->heap_start = value;
        break;
      }
    }
    assert(data->heap_start != 0);
  }
  data->machine._pc     = reader.get_entry();
  data->machine._reg[2] = std::numeric_limits<uint64_t>::max() - 15;
  data->machine._mode   = 0b00;

  data->stack_top    = data->machine._reg[2];
  data->stack_bottom = data->stack_top - (8 * 1024);

  return data;
}

void trap_callback(void* usr_data, dawn::exception_code_t cause,
                   uint64_t value) {
  data_t* data = reinterpret_cast<data_t*>(usr_data);
  switch (cause) {
    case dawn::exception_code_t::e_ecall_u_mode:
      switch (data->machine._reg[17]) {
        case 57:  // close
          data->machine._reg[10] = 0;
          data->machine._pc += 4;
          break;
        case 64: {  // write
          int      vfd     = data->machine._reg[10];
          uint64_t address = data->machine._reg[11];
          size_t   len     = data->machine._reg[12];
          if (vfd == 1 || vfd == 2) {
            // TODO: optimise this, read the whole section at a time
            for (uint64_t i = address; i < address + len; i++) {
              char res;
              if (!data->machine.memcpy_guest_to_host(&res, i, 1))
                throw std::runtime_error("something went wrong");
              std::cout << res;
            }
            data->machine._reg[10] = len;
          } else {
            data->machine._reg[10] = -9;
          }
          data->machine._pc += 4;
        } break;
        case 80:  // fstat
          data->machine._reg[10] = -38;
          data->machine._pc += 4;
          break;
        case 93: {  // exit
          data->machine._pc += 4;
          exit(data->machine._reg[10]);
        } break;
        case 214: {  // brk
          uint64_t requested_brk = data->machine._reg[10];
          if (requested_brk >= data->heap_start &&
              requested_brk < data->stack_bottom) {
            data->heap_end = requested_brk;
          }
          data->machine._reg[10] = data->heap_end;
          data->machine._pc += 4;
        } break;
        case 1000: {  // can implement any game engine function like this
          // for now return how many pages have been allocated by the machine
          data->machine._reg[10] = data->machine._memory.page_table.size();
          data->machine._pc += 4;
        } break;
        default:
          std::stringstream ss;
          ss << "unknown syscall number " << data->machine._reg[17];
          throw std::runtime_error(ss.str());
      }
      break;
    case dawn::exception_code_t::e_load_access_fault:
    case dawn::exception_code_t::e_store_access_fault: {
      bool is_stack = (value > data->stack_bottom && value <= data->stack_top);
      bool is_heap  = (value < data->heap_end && value >= data->heap_start);
      if (is_stack || is_heap) {
        uint64_t     page_number = data->machine._memory.page_number(value);
        dawn::page_t new_page    = data->machine._memory.allocate_page(
            page_number, dawn::page_permission_t::e_rw);
        data->machine._memory.page_table[page_number] = new_page;
        // TODO: maybe be smarter about this, patch/fix the cache properly
        // invalidate all caches
        data->machine._memory.mru_page = data->machine._memory.fetch_mru_page =
            dawn::page_t{};
        for (uint32_t i = 0; i < data->machine._memory.direct_cache_size; i++) {
          data->machine._memory.direct_cache[i] =
              data->machine._memory.fetch_direct_cache[i] = dawn::page_t{};
        }
      } else {
        std::stringstream ss;
        ss << "error at: " << std::hex << data->machine._pc << '\n';
        std::cout << ss.str();
        throw std::runtime_error("something weird happened");
      }
    } break;
    default:
      std::stringstream ss;
      ss << cause << " not implemented\n";
      throw std::runtime_error(ss.str());
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: [simple] [elf]\n";
    return -1;
  }

  data_t* data = load_elf(argv[1]);
  if (!data) return -1;  // TODO: throw

  data->machine._trap_callback = trap_callback;
  data->machine._trap_usr_data = data;

  while (1) {
    // std::cout << "pc: " << std::hex << data->machine._pc << '\n';
    data->machine.step(1);
    // for (uint32_t i = 0; i < 32; i++) {
    //   if (data->machine._reg[i] != 0)
    //     std::cout << "\tx" << std::dec << i << ": " << std::hex
    //               << data->machine._reg[i] << '\n';
    // }
    // std::cout.flush();
    // getchar();
  }

  return -1;
}
