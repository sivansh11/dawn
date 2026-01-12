#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <elfio/elfio.hpp>

#include "dawn/dawn.hpp"

dawn::machine_t* load_elf(const std::filesystem::path& path) {
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

  dawn::machine_t* machine =
      new dawn::machine_t{16 * 1024 * 1024, guest_base, {}};

  for (uint32_t i = 0; i < reader.segments.size(); i++) {
    const ELFIO::segment* segment = reader.segments[i];
    if (segment->get_type() != ELFIO::PT_LOAD) continue;

    ELFIO::Elf64_Addr virtual_address = segment->get_virtual_address();
    ELFIO::Elf_Xword  file_size       = segment->get_file_size();
    ELFIO::Elf_Xword  memory_size     = segment->get_memory_size();
    bool              is_read         = segment->get_flags() & ELFIO::PF_R;
    bool              is_write        = segment->get_flags() & ELFIO::PF_W;
    bool              is_exec         = segment->get_flags() & ELFIO::PF_X;

    machine->memcpy_host_to_guest(
        virtual_address, reinterpret_cast<const void*>(segment->get_data()),
        file_size);
    if (memory_size - file_size) {
      machine->memset(virtual_address + file_size, 0, memory_size - file_size);
    }
  }

  // // TODO: some how get stack overflow protection
  // for (uint32_t i = 0; i < reader.sections.size(); i++) {
  //   ELFIO::section* section = reader.sections[i];
  //   if (section->get_type() != ELFIO::SHT_SYMTAB) continue;
  //   const ELFIO::symbol_section_accessor symbols(reader, section);
  //
  //   for (uint32_t j = 0; j < symbols.get_symbols_num(); j++) {
  //     std::string       name;
  //     ELFIO::Elf64_Addr value;
  //     ELFIO::Elf_Xword  size;
  //     unsigned char     bind;
  //     unsigned char     type;
  //     ELFIO::Elf_Half   section_index;
  //     unsigned char     other;
  //
  //     symbols.get_symbol(j, name, value, size, bind, type, section_index,
  //                        other);
  //     if (name == "_end") {
  //       state._heap_address = value;
  //       break;
  //     }
  //   }
  //   assert(state._heap_address != 0);
  // }
  machine->_pc     = reader.get_entry();
  machine->_reg[2] = machine->_ram_size - 8;
  machine->_mode   = 0b00;

  return machine;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Usage: [test] [elf]\n";
    return -1;
  }

  dawn::machine_t* machine = load_elf(argv[1]);
  if (!machine) return -1;  // TODO: throw

  machine->_syscalls[93] = [](dawn::machine_t& machine) {
    if (machine._reg[10] == 0)
      std::cout << "passed\n";
    else
      std::cout << "failed\n";
    exit(machine._reg[10]);
  };

  while (1) {
    // std::cout << "pc: " << std::hex << machine->_pc << '\n';
    machine->step(1);
    // for (uint32_t i = 0; i < 32; i++) {
    //   if (machine->_reg[i] != 0)
    //     std::cout << "\tx" << std::dec << i << ": " << std::hex
    //               << machine->_reg[i] << '\n';
    // }
  }

  return -1;
}
