.section .text
.global _start

_start:
  .option push
  .option norelax
  la gp, __global_pointer$
  .option pop
  li sp, 0x200000
  la t0, trap_handler
  csrw mtvec, t0


  li a7, 1000 // 1000 custom print ecall
  la a1, string
  ecall

  li a7, 1000 // 1000 custom print ecall
  la a1, string2
  ecall

  li a7, 93 // host call exit
  ecall

  j infinite_loop

infinite_loop:
  j infinite_loop

trap_handler:
  addi sp, sp, -16  
  sd a7, 0(sp)      
  sd a1, 8(sp)      

  li t0, 1000 // only handles 1000 ecall
  bne a7, t0, trap_handler_return

  li t2, 0x10000000
  call print_loop

trap_handler_return:
  ld a7, 0(sp)     
  ld a1, 8(sp)     
  addi sp, sp, 16  

// advance pc
  csrr t0, mepc
  addi t0, t0, 4
  csrw mepc, t0
  mret

print_loop:
  lb t0, 0(a1)
  beq t0, zero, print_done
  sb t0, 0(t2)
  addi a1, a1, 1
  j print_loop

print_done:
  ret

.section .data
string:
  .ascii "Hello World\n\0"
string2:
  .ascii "From RISCV\n\0"
