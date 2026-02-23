#!/bin/bash

cmake --build build --target user
if [ $? -ne 0 ]; then
  echo "Build failed"
  exit
fi
./build/examples/user/user ./examples/user/a.out
