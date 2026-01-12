#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <termios.h>
#include <sys/ioctl.h>
#include <asm-generic/ioctls.h>

// #define DAWN_ENABLE_LOGGING
#define DAWN_RUNTIME_MEMORY_BOUNDS_CHECK
#include "dawn/dawn.hpp"

uint64_t _mmio_start = 0x10000000;
uint64_t _mmio_stop  = 0x20000000;

static int is_eofd;

inline int is_kbhit() {
  if (is_eofd) return -1;
  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);
  if (!byteswaiting && write(fileno(stdin), 0, 0) != 0) {
    is_eofd = 1;
    return -1;
  }  // Is end-of-file for
  return !!byteswaiting;
}

inline int read_kbbyte() {
  if (is_eofd) return 0xffffffff;
  char rxchar = 0;
  int  rread  = read(fileno(stdin), (char*)&rxchar, 1);

  if (rread > 0)  // Tricky: getchar can't be used with arrow keys.
    return rxchar;
  else
    return -1;
}

static uint64_t timercmp  = 0;
static uint64_t timer     = 0;
static uint64_t boot_time = 0;

inline uint64_t _mmio_load(uint64_t addr) {
  // std::cout << "load at 0x" << std::hex << addr << '\n';
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
}

inline void _mmio_store(uint64_t addr, uint64_t value) {
  // std::cout << "store at 0x" << std::hex << addr << '\n';
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
}

inline uint64_t get_time_now_us() {
  auto      now_tp = std::chrono::system_clock::now();
  long long microseconds_count =
      std::chrono::duration_cast<std::chrono::microseconds>(
          now_tp.time_since_epoch())
          .count();
  return microseconds_count;
}

inline void _wfi_callback() { timer = get_time_now_us() - boot_time; }

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

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: [linux] [Image] [dtb]\n";
    return -1;
  }

  const uint64_t  offset = 0;
  dawn::machine_t machine{128 * 1024 * 1024, offset};
  machine._wfi_callback = _wfi_callback;
  auto kernel           = read_file(argv[1]);
  std::cout << "kernel size: " << kernel.size() << '\n';
  std::cout << "kernel loaded at: " << offset << '\n';
  machine.memcpy_host_to_guest(offset, kernel.data(), kernel.size());
  machine._pc = offset;

  auto     dtb      = read_file(argv[2]);
  uint64_t dtb_addr = kernel.size() + offset;
  dtb_addr += dtb_addr % 8;
  std::cout << "dtb size: " << dtb.size() << '\n';
  machine.memcpy_host_to_guest(dtb_addr, dtb.data(), dtb.size());
  std::cout << "dtb loaded at: " << std::hex << dtb_addr << '\n';
  machine._reg[10] = 0;
  machine._reg[11] = dtb_addr;

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

  boot_time = get_time_now_us();

  uint64_t instruction_count = 0;
  while (1) {
    const uint64_t num_instructions = 10;
    if (!machine._wfi) machine.step(num_instructions);
    timer = get_time_now_us() - boot_time;
    if (timercmp) {
      if (timer > timercmp) {
        machine._wfi = false;
        machine.handle_trap(dawn::exception_code_t::e_machine_timer_interrupt,
                            0);
      }
    }
  }

  return -1;
}
