Essential Missing Features:
- S Mode (Supervisor Mode) - Linux expects to run in supervisor mode, not machine mode
- A Extension - Atomic operations for kernel synchronization
- Complete CSR Set - Many privileged CSRs are missing or stubbed
- Timer/Counter CSRs - TIME, CYCLE, INSTRET for scheduling
- Interrupt Controller - Basic PLIC or equivalent
- Device Emulation - UART for console, storage for root filesystem


https://popovicu.com/posts/789-kb-linux-without-mmu-riscv/
