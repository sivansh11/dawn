#!/bin/bash

cmake --build build --target linux
if [ $? -ne 0 ]; then
  echo "Build failed"
  exit
fi
./build/examples/linux/linux ./examples/linux/Image ./examples/linux/rootfs.cpio 
