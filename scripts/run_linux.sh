#!/bin/bash

cmake --build build
riscv64-unknown-elf-objdump -M numeric -M no-aliases -d ../linux-6.18/vmlinux > vmlinux_dump 

cd examples/linux
dtc -I dts -O dtb -o dt.dtb dt.dts
cd ../../

# ./build/examples/linux/linux ../linux-6.18/arch/riscv/boot/Image examples/linux/dt.dtb n
./build/examples/linux/linux ../buildroot/output/images/Image examples/linux/dt.dtb y
