#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cstdlib>
#include <termios.h>
#include <sys/ioctl.h>
#include <asm-generic/ioctls.h>

#include "dawn/machine.hpp"
#include "dawn/memory.hpp"
#include "dawn/types.hpp"

std::vector<uint8_t> read_file(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read file: " + file_path);
  }
  return buffer;
}

static dawn::machine_t machine;

int main(int argc, char** argv) {
  if (argc < 4) throw std::runtime_error("[linux] [elf] [dtb] [logging]!");

  uint64_t size   = 1024 * 1024 * 128;
  uint64_t offset = 0;
  machine         = dawn::machine_t::create(size, offset);
  auto kernel     = read_file(argv[1]);
  std::cout << "kernel size: " << kernel.size() << '\n';
  std::cout << "kernel loaded at: " << offset << '\n';
  if (!machine._memory.memcpy_host_to_guest(offset, kernel.data(),
                                            kernel.size()))
    throw std::runtime_error("failed to copy kernel to guest");
  machine._pc = offset;

  auto     dtb      = read_file(argv[2]);
  uint64_t dtb_addr = kernel.size() + offset;
  dtb_addr += dtb_addr % 8;
  std::cout << "dtb size: " << dtb.size() << '\n';
  if (!machine._memory.memcpy_host_to_guest(dtb_addr, dtb.data(), dtb.size()))
    throw std::runtime_error("failed to copy dtb to guest");
  std::cout << "dtb loaded at: " << std::hex << dtb_addr << '\n';
  machine._reg[10] = 0;
  machine._reg[11] = dtb_addr;

  std::cout << "max non mmio'ed memory: " << std::hex << size << '\n';

  static uint64_t timercmp  = 0;
  static uint64_t timer     = 0;
  static uint64_t boot_time = 0;

  std::atexit([]() {
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(0, TCSANOW, &term);
  });

  signal(SIGINT, [](int sig) { exit(0); });

  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag &= ~(ICANON | ECHO);  // Disable echo as well
  tcsetattr(0, TCSANOW, &term);

  static int is_eofd;

  auto is_kbhit = []() -> int {
    if (is_eofd) return -1;
    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    if (!byteswaiting && write(fileno(stdin), 0, 0) != 0) {
      is_eofd = 1;
      return -1;
    }  // Is end-of-file for
    return !!byteswaiting;
  };

  auto read_kbbyte = []() -> int {
    if (is_eofd) return 0xffffffff;
    char rxchar = 0;
    int  rread  = read(fileno(stdin), (char*)&rxchar, 1);

    if (rread > 0)  // Tricky: getchar can't be used with arrow keys.
      return rxchar;
    else
      return -1;
  };

  machine._memory.insert_memory(
      machine._memory.translate_guest_to_host(0x10000000),
      0x20000000,  // arbitary for range for now
      dawn::memory_protection_t::e_read_write,
      [&](dawn::address_t addr, uint64_t value) {
        if (addr == 0x10000000) {
          printf("%c", (int)value);
          fflush(stdout);
        } else {
          if (addr == 0x11004000) {
            timercmp = value;
          } else if (addr == 0x1100BFF8) {
            timer = value;
            throw std::runtime_error("wrote to timer");
          }
        }
      },
      [&](dawn::address_t addr) -> uint64_t {
        if (addr == 0x10000005) {
          return 0x60 | is_kbhit();
        } else if (addr == 0x10000000 && is_kbhit()) {
          return read_kbbyte();
        } else if (addr == 0x11004000) {
          return timercmp;
        } else if (addr == 0x1100BFF8) {
          return timer;
        }
        return 0;
      });

  std::cout << "mmio start: " << std::hex << 0x10000000 << '\n';
  std::cout << "mmio end: " << std::hex << 0x20000000 << '\n';

  auto get_time_now_us = []() -> uint64_t {
    auto      now_tp = std::chrono::system_clock::now();
    long long microseconds_count =
        std::chrono::duration_cast<std::chrono::microseconds>(
            now_tp.time_since_epoch())
            .count();
    return microseconds_count;
  };

  boot_time = get_time_now_us();

  uint64_t instruction_count = 0;

  while (true) {
    auto instruction = machine.fetch_instruction();
    instruction_count++;
    if (timercmp) {
      // get timer every 100 or so instructions since getting time is an
      // expensive call
      if (instruction_count > 100) {
        instruction_count = 0;
        timer             = (get_time_now_us() - boot_time);
      }

      if (timer > timercmp) {
        machine._paused = false;
        if (machine.handle_trap(
                dawn::riscv::exception_code_t::e_machine_timer_interrupt, 0)) {
          continue;
        }
      }
    }
    if (machine._paused) continue;

    if (instruction) machine.decode_and_exec_instruction(*instruction);
  }

  return machine._running ? 1 : machine._reg[10];
}
