# Dawn: RiscV Runtime and Virtual Machine
Dawn is a lightweight RiscV emulator designed to serve as a portable scripting bytecode for [Horizon](https://github.com/sivansh11/horizon)

# Key Features
- Target: implements rv64ima target (with more extensions planned for the future).
- Modes: implements (M)achine and (U)ser modes (no Supervisor mode).
- VM call: supports a vmcall/hypercall system by "hooking" ecalls.
    This is required if the user script needs to interact with the underlying game engine or needs to perform os activities, for example opening a file.
    This is sandboxed, so if the game engine chooses not to provide the capabilities to read/write to a file, all they need to do is modify the ecall handler/hook.


# How Does it work ?
Dawn is designed to be flexible, it supports 2 distinct operational mode.
```
+-----------------------------+  +----------------------+  +-------------------------+
|      Mode A: OS Image       |  |  Internal Privilege  |  |      Raw Resources      |
|  (e.g., Linux / Image)      |--|  [Machine Mode (M)]  |--|     (MMIO handlers)     |
+-----------------------------+  +----------------------+  +-------------------------+
               ^                            ^                        ^
               |                            |                        |
[ Guest RISC-V Code ]            [ Dawn Runtime ]          [ Host System ]

               |                            |                        |
               v                            v                        v
+-----------------------------+  +----------------------+  +-------------------------+
|    Mode B: ELF Binary       |  |   Hypercall Layer    |  |    Sandboxed Engine     |
| (e.g., Script / Newlib)     |--|   [User Mode (U)]    |--| (Custom ecall Handlers) |
+-----------------------------+  +----------------------+  +-------------------------+
```

## Full System Emulation
- In this mode, dawn acts as a hardware provider, it doesnt assume any underlying OS.
- The guest OS is responsible for transitioning from (M)achine to (U)ser mode, handling all syscalls.
- Here, the host needs to provide all necessary peripherals, like uart, keyboard, mouse, display output etc.
- See https://github.com/sivansh11/dem for more information
## Application Emulation
- This is the main reason dawn was created, the guest code is typically compiled to a lightweight ELF binary using [NewLib](https://en.wikipedia.org/wiki/Newlib)
- In this, there is no guest OS, instead, any/all syscalls done by the guest script/binary is handled by the host.
- For example, in newlib (and linux) the ecall number for write syscall is 64 and read is 63, if the host wants to provide the guest the capability to perform file system read/write, the host needs to register syscall callbacks.
- When dawn detects a syscall, it calls the respective callback.
- This method can be used to provide custom syscalls/game engine calls to the script/binary.
- TODO: add simple example (not yet added).
for an example, check out (host, native) examples/simple/main.cpp and (guest, riscv) tests/simple/main.cpp. tests/simple/main.cpp is compiled using a crosscompiler, in this case riscv64-unknown-elf-g++, to create the a.out binary

# Integration
To use dawn in your project
```cmake
include(FetchContent)
FetchContent_Declare(
  dawn
  GIT_REPOSITORY https://github.com/sivansh11/dawn
  GIT_TAG main
)
FetchContent_MakeAvailable(dawn)

target_link_libraries(your_target PUBLIC dawn)
```


# Planned for the future
- Jitted runtime.
- f, v, c extension
