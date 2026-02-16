#include <chrono>
#include <compare>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
using namespace std::string_literals;  // for ""s suffix

#include <asm-generic/ioctls.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <libfdt.h>
#include <libfdt_env.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "dawn/dawn.hpp"

static dawn::machine_t *machine;

std::string to_hex_string(uint64_t val) { return std::format("{:#x}", val); }
std::string to_hex_string_without_0x(uint64_t val) {
  std::stringstream ss;
  ss << std::hex << val;
  return ss.str();
}

std::vector<uint8_t> read_file(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + file_path);
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(size);
  if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
    throw std::runtime_error("Failed to read file: " + file_path);
  }
  return buffer;
}

inline uint64_t get_time_now_us() {
  auto      now_tp = std::chrono::system_clock::now();
  long long microseconds_count =
      std::chrono::duration_cast<std::chrono::microseconds>(
          now_tp.time_since_epoch())
          .count();
  return microseconds_count;
}

static bool is_eofd_;
int         is_kbhit() {
  if (is_eofd_) return -1;
  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);
  if (!byteswaiting && write(fileno(stdin), 0, 0) != 0) {
    is_eofd_ = true;
    return -1;
  }
  return !!byteswaiting;
}
int read_kbbyte() {
  if (is_eofd_) return 0xffffffff;
  char rxchar = 0;
  int  rread  = read(fileno(stdin), (char *)&rxchar, 1);
  if (rread > 0)
    return rxchar;
  else
    return -1;
}

constexpr uint64_t plic_mmio_start = 0x0c000000;
constexpr uint64_t plic_mmio_stop  = 0x10000000;
struct plic_t {
  uint32_t _priority[1024];  // 1024 priority of each source
  uint32_t _pending[32];     // 1024 pending bits
  uint32_t _enable[32];      // 1024 enable bits per context (only 1 context)
  uint32_t _threshold;       // minimum priority to trigger an interrupt
};
static plic_t                  plic{};
constexpr dawn::mmio_handler_t plic_handler{
    .start = plic_mmio_start,
    .stop  = plic_mmio_stop,
    .load = [](const dawn::mmio_handler_t *handler, uint64_t addr) -> uint64_t {
      uint64_t offset = addr - plic_mmio_start;
      if (offset < 0x1000) {  // priority
        return plic._priority[offset >> 2];
      } else if (offset >= 0x1000 && offset < 0x1080) {  // pending
        return plic._pending[(offset - 0x1000) >> 2];
      } else if (offset >= 0x2000 && offset < 0x2100) {  // enable
        return plic._enable[(offset & 0x7f) >> 2];
      } else if (offset >= 0x200000) {  // threashold & claim/complete
        uint64_t reg_type = offset & 0xfff;
        if (reg_type == 0) return plic._threshold;
        if (reg_type == 4) {  // claim
          uint32_t best_id      = 0;
          uint32_t max_priority = plic._threshold;
          for (uint32_t id = 1; id < 1024; id++) {  // id 0 is reserved/null
            uint32_t word_idx   = id / 32;
            uint32_t bit_mask   = 1 << (id % 32);
            bool     is_pending = (plic._pending[word_idx] & bit_mask) != 0;
            bool     is_enabled = (plic._enable[word_idx] & bit_mask) != 0;
            if (is_pending && is_enabled) {
              uint32_t current_priority = plic._priority[id];
              if (current_priority > max_priority) {
                max_priority = current_priority;
                best_id      = id;
              }
            }
          }
          if (best_id > 0) {  // clear pending
            plic._pending[best_id / 32] &= ~(1 << (best_id % 32));
          }
          return best_id;
        }
      }
      return 0;
    },
    .store =
        [](const dawn::mmio_handler_t *handler, uint64_t addr, uint64_t value) {
          uint64_t offset = addr - plic_mmio_start;
          uint32_t val32  = static_cast<uint32_t>(value);
          if (offset < 0x1000) {  // priority
            uint32_t source = offset >> 2;
            if (source > 0 && source < 1024) {
              plic._priority[source] = val32;
            }
          } else if (offset >= 0x1000 && offset < 0x1080) {
            // readonly, ignore writes
          } else if (offset >= 0x2000 && offset < 0x2100) {
            uint32_t reg_idx      = (offset & 0x7f) >> 2;
            plic._enable[reg_idx] = val32;
          } else if (offset >= 0x200000) {
            uint32_t reg_type = offset & 0xfff;
            if (reg_type == 0) {
              plic._threshold = val32;
            } else if (reg_type == 4) {
              // do nothing on complete
            }
          }
        }};

constexpr uint64_t             uart_mmio_start    = 0x10000000;
constexpr uint64_t             uart_mmio_stop     = 0x10000100;
constexpr uint64_t             timebase_frequency = 1000000;
constexpr dawn::mmio_handler_t uart_handler{
    .start = uart_mmio_start,
    .stop  = uart_mmio_stop,
    .load = [](const dawn::mmio_handler_t *handler, uint64_t addr) -> uint64_t {
      if (addr == uart_mmio_start && is_kbhit()) {  // data
        return read_kbbyte();
      } else if (addr == uart_mmio_start + 0x5) {  // status
        return 0x60 | is_kbhit();
      }
      return 0;
    },
    .store =
        [](const dawn::mmio_handler_t *handler, uint64_t addr, uint64_t value) {
          if (addr == uart_mmio_start) {  // data
            printf("%c", (int)value);
            fflush(stdout);
          }
        }};

static uint64_t                timercmp         = 0;
static uint64_t                timer            = 0;
static uint64_t                boot_time        = 0;
constexpr uint64_t             clint_mmio_start = 0x11000000;
constexpr uint64_t             clint_mmio_stop  = 0x11010000;
constexpr dawn::mmio_handler_t clint_handler{
    .start = clint_mmio_start,
    .stop  = clint_mmio_stop,
    .load = [](const dawn::mmio_handler_t *handler, uint64_t addr) -> uint64_t {
      if (addr == clint_mmio_start) {  // msip
        return (machine->read_csr(dawn::MIP) >> 3) & 1;
      } else if (addr == clint_mmio_start + 0x4000) {  // mtimercmp
        return timercmp;
      } else if (addr == clint_mmio_start + 0xbff8) {  // mtimer
        return timer;
      }
      return 0;
    },
    .store =
        [](const dawn::mmio_handler_t *handler, uint64_t addr, uint64_t value) {
          if (addr == clint_mmio_start) {  // msip
            if (value & 1)
              machine->_csr[dawn::MIP] |= (1ull << 3);
            else
              machine->_csr[dawn::MIP] &= ~(1ull << 3);
          } else if (addr == clint_mmio_start + 0x4000) {  // mtimercmp
            timercmp = value;
            if (timer >= timercmp) {
              machine->_csr[dawn::MIP] |= (1ull << 7);
            } else {
              machine->_csr[dawn::MIP] &= ~(1ull << 7);
            }
          } else if (addr == clint_mmio_start + 0xbff8) {  // mtimer
            timer = value;
            // TODO: remove this
            throw std::runtime_error("wrote to mtimer");
          }
        }};

constexpr uint64_t framebuffer_mmio_start = 0x50000000;
constexpr uint64_t width                  = 1200;
constexpr uint64_t height                 = 800;
constexpr uint64_t stride                 = width * 4;
constexpr uint64_t framebuffer_mmio_stop =
    framebuffer_mmio_start + (width * height * 4);
uint8_t                        framebuffer[width * height * 4];
constexpr dawn::mmio_handler_t framebuffer_handler{
    .start = framebuffer_mmio_start,
    .stop  = framebuffer_mmio_stop,
    .load = [](const dawn::mmio_handler_t *handler, uint64_t addr) -> uint64_t {
      uint64_t offset = addr - framebuffer_mmio_start;
      return *reinterpret_cast<uint64_t *>(&framebuffer[offset]);
    },
    .store =
        [](const dawn::mmio_handler_t *handler, uint64_t addr, uint64_t value) {
          uint64_t offset = addr - framebuffer_mmio_start;
          *reinterpret_cast<uint64_t *>(&framebuffer[offset]) = value;
        }};

const std::string bootargs =
    "earlycon=uart8250,mmio," + to_hex_string(uart_mmio_start) + "," +
    std::to_string(timebase_frequency) + " console=ttyS0";
constexpr uint64_t offset   = 0x80000000;
constexpr uint64_t ram_size = 1024 * 1024 * 1024;

static bool should_close = false;
void        x11_framebuffer_thread() {
  Display *display = XOpenDisplay(nullptr);
  if (!display) {
    throw std::runtime_error("Failed to open X11 display");
  }

  int    screen = DefaultScreen(display);
  Window root   = RootWindow(display, screen);

  XVisualInfo vinfo;
  if (!XMatchVisualInfo(display, screen, 24, TrueColor, &vinfo)) {
    throw std::runtime_error("Failed to find 24-bit visual");
  }

  XSetWindowAttributes attr;
  attr.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
  attr.border_pixel     = 0;
  attr.background_pixel = 0;
  attr.event_mask       = StructureNotifyMask;

  Window window = XCreateWindow(
      display, root, 0, 0, width, height, 0, vinfo.depth, InputOutput,
      vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWEventMask,
      &attr);

  XMapWindow(display, window);
  XStoreName(display, window, "DEM");

  GC gc = DefaultGC(display, screen);

  std::vector<uint8_t> image_buffer(width * height * 4);
  XImage *image = XCreateImage(display, vinfo.visual, vinfo.depth, ZPixmap, 0,
                                      reinterpret_cast<char *>(image_buffer.data()),
                                      width, height, 32, width * 4);
  if (!image) {
    throw std::runtime_error("Failed to create XImage");
  }

  XFlush(display);
  XSync(display, False);

  std::cout << "X11 window created: " << width << "x" << height
            << ", depth: " << vinfo.depth << ", stride: " << stride
            << std::endl;

  const uint64_t frame_duration_us = 33333;
  uint64_t       last_frame_us     = get_time_now_us();

  while (!should_close) {
    uint64_t now_us = get_time_now_us();
    if (now_us - last_frame_us >= frame_duration_us) {
      const uint8_t *fb_data = framebuffer;
      for (int i = 0; i < width * height; i++) {
        uint32_t pixel = *reinterpret_cast<const uint32_t *>(fb_data + i * 4);
        // Convert a8r8g8b8 to x8r8g8b8 (ignore alpha)
        uint8_t  b      = (pixel >> 0) & 0xFF;
        uint8_t  g      = (pixel >> 8) & 0xFF;
        uint8_t  r      = (pixel >> 16) & 0xFF;
        uint8_t  a      = (pixel >> 24) & 0xFF;
        uint32_t xpixel = (a << 24) | (r << 16) | (g << 8) | b;
        *reinterpret_cast<uint32_t *>(image_buffer.data() + i * 4) = xpixel;
      }

      int result =
          XPutImage(display, window, gc, image, 0, 0, 0, 0, width, height);

      XFlush(display);
      last_frame_us = now_us;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }

  XDestroyImage(image);
  XDestroyWindow(display, window);
  XCloseDisplay(display);
}

void setup_fdt_root_properties(void *fdt) {
  if (fdt_setprop_string(fdt, 0, "compatible", "riscv-minimal-nommu"))
    throw std::runtime_error("failed to set compatible property");
  if (fdt_setprop_string(fdt, 0, "model", "riscv-minimal-nommu,dawn"))
    throw std::runtime_error("failed to set model property");
  if (fdt_setprop_cell(fdt, 0, "#address-cells", 2))
    throw std::runtime_error("failed to set #address-cells property");
  if (fdt_setprop_cell(fdt, 0, "#size-cells", 2))
    throw std::runtime_error("failed to set #size-cells property");
}

int add_fdt_chosen_node(void *fdt) {
  int chosen = fdt_add_subnode(fdt, 0, "chosen");
  if (chosen < 0) throw std::runtime_error("failed to add chosen subnode");

  if (fdt_setprop_string(fdt, chosen, "bootargs", bootargs.c_str()))
    throw std::runtime_error("failed to set bootargs property");
  return chosen;
}

int add_fdt_memory_node(void *fdt, uint64_t ram_size) {
  int memory = fdt_add_subnode(
      fdt, 0, ("memory@"s + to_hex_string_without_0x(offset)).c_str());
  if (memory < 0) throw std::runtime_error("failed to add memory subnode");
  if (fdt_setprop_string(fdt, memory, "device_type", "memory"))
    throw std::runtime_error("failed to set memory device_type property");
  uint64_t memory_reg[] = {cpu_to_fdt64(offset), cpu_to_fdt64(ram_size)};
  if (fdt_setprop(fdt, memory, "reg", memory_reg, sizeof(memory_reg)))
    throw std::runtime_error("failed to set memory reg property");
  return memory;
}

int add_fdt_cpus_node(void *fdt) {
  int cpus = fdt_add_subnode(fdt, 0, "cpus");
  if (cpus < 0) throw std::runtime_error("failed to add cpus subnode");
  if (fdt_setprop_cell(fdt, cpus, "#address-cells", 1))
    throw std::runtime_error("failed to set cpus #address-cells property");
  if (fdt_setprop_cell(fdt, cpus, "#size-cells", 0))
    throw std::runtime_error("failed to set cpus #size-cells property");
  if (fdt_setprop_cell(fdt, cpus, "timebase-frequency", timebase_frequency))
    throw std::runtime_error("failed to set timebase-frequency property");
  return cpus;
}

int add_fdt_cpu_node(void *fdt, int cpus) {
  // TODO: maybe make hart id 0 configurable ?
  int cpu0 = fdt_add_subnode(fdt, cpus, "cpu@0");
  if (cpu0 < 0) throw std::runtime_error("failed to add cpu@0 subnode");
  if (fdt_setprop_string(fdt, cpu0, "device_type", "cpu"))
    throw std::runtime_error("failed to set cpu device_type property");
  if (fdt_setprop_cell(fdt, cpu0, "reg", 0))
    throw std::runtime_error("failed to set cpu reg property");
  if (fdt_setprop_string(fdt, cpu0, "status", "okay"))
    throw std::runtime_error("failed to set cpu status property");
  if (fdt_setprop_string(fdt, cpu0, "compatible", "riscv"))
    throw std::runtime_error("failed to set cpu compatible property");
  if (fdt_setprop_string(fdt, cpu0, "riscv,isa", "rv64ima"))
    throw std::runtime_error("failed to set cpu riscv,isa property");
  if (fdt_setprop_string(fdt, cpu0, "mmu-type", "riscv,none"))
    throw std::runtime_error("failed to set cpu mmu-type property");
  return cpu0;
}

int add_fdt_interrupt_controller(void *fdt, int cpu0) {
  int intc = fdt_add_subnode(fdt, cpu0, "interrupt-controller");
  if (intc < 0)
    throw std::runtime_error("failed to add interrupt-controller subnode");
  if (fdt_setprop_cell(fdt, intc, "#interrupt-cells", 1))
    throw std::runtime_error(
        "failed to set interrupt-controller #interrupt-cells property");
  if (fdt_setprop(fdt, intc, "interrupt-controller", nullptr, 0))
    throw std::runtime_error("failed to set interrupt-controller property");
  if (fdt_setprop_string(fdt, intc, "compatible", "riscv,cpu-intc"))
    throw std::runtime_error(
        "failed to set interrupt-controller compatible property");
  uint32_t intc_phandle = 1;
  if (fdt_setprop_cell(fdt, intc, "phandle", intc_phandle))
    throw std::runtime_error(
        "failed to set interrupt-controller phandle property");
  return intc_phandle;
}

int add_fdt_soc_node(void *fdt) {
  int soc = fdt_add_subnode(fdt, 0, "soc");
  if (soc < 0) throw std::runtime_error("failed to add soc subnode");
  if (fdt_setprop_cell(fdt, soc, "#address-cells", 2))
    throw std::runtime_error("failed to set soc #address-cells property");
  if (fdt_setprop_cell(fdt, soc, "#size-cells", 2))
    throw std::runtime_error("failed to set soc #size-cells property");
  if (fdt_setprop_string(fdt, soc, "compatible", "simple-bus"))
    throw std::runtime_error("failed to set soc compatible property");
  if (fdt_setprop(fdt, soc, "ranges", NULL, 0))
    throw std::runtime_error("failed to set soc ranges property");
  return soc;
}

int add_fdt_plic_node(void *fdt, int soc, uint32_t intc_phandle) {
  std::string plic_node_name =
      "plic@" + to_hex_string_without_0x(plic_mmio_start);
  int plic = fdt_add_subnode(fdt, soc, plic_node_name.c_str());
  if (plic < 0) throw std::runtime_error("failed to add plic subnode");
  uint64_t plic_reg[] = {cpu_to_fdt64(plic_mmio_start),
                         cpu_to_fdt64(plic_mmio_stop - plic_mmio_start)};
  if (fdt_setprop(fdt, plic, "reg", plic_reg, sizeof(plic_reg)))
    throw std::runtime_error("failed to set plic reg property");
  if (fdt_setprop_string(fdt, plic, "compatible", "sifive,plic-1.0.0"))
    throw std::runtime_error("failed to set plic compatible property");
  if (fdt_appendprop_string(fdt, plic, "compatible", "riscv,plic0"))
    throw std::runtime_error("failed to set plic compatible property");
  if (fdt_setprop_cell(fdt, plic, "#interrupt-cells", 1))
    throw std::runtime_error("failed to set plic #interrupt-cells property");
  if (fdt_setprop(fdt, plic, "interrupt-controller", nullptr, 0))
    throw std::runtime_error(
        "failed to set plic interrupt-controller property");
  if (fdt_setprop_cell(fdt, plic, "riscv,ndev", 32))
    throw std::runtime_error("failed to set plic riscv,ndev property");
  uint32_t plic_intr[] = {cpu_to_fdt32(intc_phandle), cpu_to_fdt32(11)};
  if (fdt_setprop(fdt, plic, "interrupts-extended", plic_intr,
                  sizeof(plic_intr)))
    throw std::runtime_error("failed to set plic interrupts-extended property");
  uint32_t plic_phandle = 2;
  if (fdt_setprop_cell(fdt, plic, "phandle", plic_phandle))
    throw std::runtime_error("failed to set plic phandle property");
  return plic_phandle;
}

int add_fdt_uart_node(void *fdt, int soc) {
  std::string uart_node_name =
      "uart@" + to_hex_string_without_0x(uart_mmio_start);
  int uart = fdt_add_subnode(fdt, soc, uart_node_name.c_str());
  if (uart < 0) throw std::runtime_error("failed to add uart subnode");
  uint64_t uart_reg[] = {cpu_to_fdt64(uart_mmio_start),
                         cpu_to_fdt64(uart_mmio_stop - uart_mmio_start)};
  if (fdt_setprop_cell(fdt, uart, "clock-frequency", timebase_frequency))
    throw std::runtime_error("failed to set uart clock-frequency property");
  if (fdt_setprop(fdt, uart, "reg", uart_reg, sizeof(uart_reg)))
    throw std::runtime_error("failed to set uart reg property");
  if (fdt_setprop_string(fdt, uart, "compatible", "ns16550a"))
    throw std::runtime_error("failed to set uart compatible property");
  return uart;
}

int add_fdt_clint_node(void *fdt, int soc, uint32_t intc_phandle) {
  std::string clint_node_name =
      "clint@" + to_hex_string_without_0x(clint_mmio_start);
  int clint = fdt_add_subnode(fdt, soc, clint_node_name.c_str());
  if (clint < 0) throw std::runtime_error("failed to add clint subnode");
  uint64_t clint_reg[] = {cpu_to_fdt64(clint_mmio_start),
                          cpu_to_fdt64(clint_mmio_stop - clint_mmio_start)};
  if (fdt_setprop(fdt, clint, "reg", clint_reg, sizeof(clint_reg)))
    throw std::runtime_error("failed to set clint reg property");
  if (fdt_setprop_string(fdt, clint, "compatible", "sifive,clint0"))
    throw std::runtime_error("failed to set clint compatible property");
  // on the same line as ^
  if (fdt_appendprop_string(fdt, clint, "compatible", "riscv,clint0"))
    throw std::runtime_error("failed to append clint compatible property");
  uint32_t clint_intr[] = {cpu_to_fdt32(intc_phandle), cpu_to_fdt32(3),
                           cpu_to_fdt32(intc_phandle), cpu_to_fdt32(7)};
  if (fdt_setprop(fdt, clint, "interrupts-extended", clint_intr,
                  sizeof(clint_intr)))
    throw std::runtime_error(
        "failed to set clint interrupts-extended property");
  return clint;
}

int add_fdt_framebuffer_node(void *fdt, int soc) {
  std::string fb_node_name =
      "framebuffer@" + to_hex_string_without_0x(framebuffer_mmio_start);
  int      fb_node  = fdt_add_subnode(fdt, soc, fb_node_name.c_str());
  uint64_t fb_reg[] = {
      cpu_to_fdt64(framebuffer_mmio_start),
      cpu_to_fdt64(framebuffer_mmio_stop - framebuffer_mmio_start)};
  if (fdt_setprop_string(fdt, fb_node, "compatible", "simple-framebuffer"))
    throw std::runtime_error("failed to set framebuffer compatible property");
  if (fdt_setprop(fdt, fb_node, "reg", fb_reg, sizeof(fb_reg)))
    throw std::runtime_error("failed to set framebuffer reg property");
  if (fdt_setprop_cell(fdt, fb_node, "width", width))
    throw std::runtime_error("failed to set framebuffer width property");
  if (fdt_setprop_cell(fdt, fb_node, "height", height))
    throw std::runtime_error("failed to set framebuffer height property");
  if (fdt_setprop_cell(fdt, fb_node, "stride", stride))
    throw std::runtime_error("failed to set framebuffer stride property");
  if (fdt_setprop_string(fdt, fb_node, "format", "a8r8g8b8"))
    throw std::runtime_error("failed to set framebuffer format property");
  return fb_node;
}

std::vector<uint8_t> generate_dtb() {
  std::vector<uint8_t> blob(64 * 1024);

  void *fdt = blob.data();
  if (fdt_create_empty_tree(fdt, blob.size()))
    throw std::runtime_error("failed to create empty fdt tree");

  setup_fdt_root_properties(fdt);

  int chosen  = add_fdt_chosen_node(fdt);
  int memory  = add_fdt_memory_node(fdt, ram_size);
  int cpus    = add_fdt_cpus_node(fdt);
  int cpu0    = add_fdt_cpu_node(fdt, cpus);
  int intc    = add_fdt_interrupt_controller(fdt, cpu0);
  int soc     = add_fdt_soc_node(fdt);
  int plic    = add_fdt_plic_node(fdt, soc, intc);
  int uart    = add_fdt_uart_node(fdt, soc);
  int clint   = add_fdt_clint_node(fdt, soc, intc);
  int fb_node = add_fdt_framebuffer_node(fdt, soc);

  blob.resize(fdt_totalsize(fdt));
  return blob;
}

void patch_dtb(std::vector<uint8_t> &blob, uint64_t initrd_addr,
               uint64_t initrd_size) {
  void *fdt    = blob.data();
  int   chosen = fdt_path_offset(fdt, "/chosen");
  if (chosen < 0) throw std::runtime_error("failed to get chosen subnode");
  if (fdt_setprop_cell(fdt, chosen, "linux,initrd-start", initrd_addr))
    throw std::runtime_error("failed to set linux,initrd-start property");
  if (fdt_setprop_cell(fdt, chosen, "linux,initrd-end",
                       initrd_addr + initrd_size))
    throw std::runtime_error("failed to set linux,initrd-end property");
}

int main(int argc, char **argv) {
  if (argc != 3) throw std::runtime_error("[dem] [Image] [initrd]");

  machine = new dawn::machine_t(
      ram_size, offset,
      {framebuffer_handler, uart_handler, clint_handler, plic_handler});

  // read kernel
  auto kernel = read_file(argv[1]);

  // read initrd
  auto initrd = read_file(argv[2]);

  // generate dtb
  auto dtb = generate_dtb();

  std::cout << "kernel size: " << kernel.size() << '\n';
  std::cout << "kernel loaded at: " << offset << '\n';
  machine->memcpy_host_to_guest(offset, kernel.data(), kernel.size());
  machine->_pc = offset;

  uint64_t dtb_addr = kernel.size() + offset;
  dtb_addr += dtb_addr % 8;
  uint64_t initrd_addr = dtb_addr + dtb.size();
  initrd_addr += initrd_addr % 8;
  patch_dtb(dtb, initrd_addr, initrd.size());

  std::cout << "dtb size: " << dtb.size() << '\n';
  machine->memcpy_host_to_guest(dtb_addr, dtb.data(), dtb.size());
  std::cout << "dtb loaded at: " << std::hex << dtb_addr << '\n';
  machine->_reg[10] = 0;
  machine->_reg[11] = dtb_addr;

  std::cout << "initrd size: " << initrd.size() << '\n';
  std::cout << "initrd loaded at: " << initrd_addr << '\n';
  machine->memcpy_host_to_guest(initrd_addr, initrd.data(), initrd.size());

  std::cout << "bootargs: " << bootargs << '\n';

  // setup terminal for uart
  std::atexit([]() {
    struct termios term;
    tcgetattr(0, &term);
    term.c_lflag |= ICANON | ECHO;
    tcsetattr(0, TCSANOW, &term);
    should_close = true;
  });

  signal(SIGINT, [](int sig) { exit(0); });

  struct termios term;
  tcgetattr(0, &term);
  term.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &term);

  std::thread framebuffer_thread{x11_framebuffer_thread};

  boot_time                   = get_time_now_us();
  uint64_t total_instructions = 0;
  uint64_t ips                = 1;
  while (1) {
    uint64_t instructions_in_loop = 0;
    uint64_t loop_start           = get_time_now_us();
    while (instructions_in_loop < 1000) {
      uint64_t num_instructions = 10;
      if (timercmp && timercmp > timer) {
        uint64_t time_left = timercmp - timer;
        num_instructions   = (time_left * ips) / 1;
        num_instructions   = std::max(num_instructions, (uint64_t)1);
        num_instructions   = std::min(num_instructions, (uint64_t)100000);
      }
      if (!machine->_wfi) {
        machine->step(num_instructions);
        instructions_in_loop += num_instructions;
        total_instructions += num_instructions;
      } else {
        // need to run step 0 since pending interrupts are handled in step
        machine->step(0);
        if (timercmp && timercmp > timer) {
          uint64_t time_left = timercmp - timer;
          std::this_thread::sleep_for(std::chrono::microseconds(time_left));
        }
      }
      // timer
      timer = get_time_now_us() - boot_time;
      if (timercmp && timer > timercmp) {
        machine->_csr[dawn::MIP] |= (1ull << 7);  // set mtip
      } else {
        machine->_csr[dawn::MIP] &= ~(1ull << 7);  // set mtip
      }
      // plic
      bool is_plic_pending = false;
      for (uint32_t id = 1; id < 1024; id++) {
        uint32_t word_idx = id / 32;
        uint32_t bit_mask = 1 << (id % 32);
        if ((plic._pending[word_idx] & bit_mask) &&
            (plic._enable[word_idx] & bit_mask) &&
            (plic._priority[id] > plic._threshold)) {
          is_plic_pending = true;
          break;
        }
      }
      if (is_plic_pending)
        machine->_csr[dawn::MIP] |= (1ull << 11);
      else
        machine->_csr[dawn::MIP] &= ~(1ull << 11);
    }

    uint64_t elapsed = get_time_now_us() - loop_start;
    if (elapsed > 0 && instructions_in_loop > 0) {
      ips = (ips * 8 + (instructions_in_loop / elapsed) * 2) / 10;
    }
  }

  return 0;
}
