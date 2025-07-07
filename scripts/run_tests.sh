#!/bin/bash

if [ ! -d "tests" ]; then
  git clone --recursive https://github.com/riscv-software-src/riscv-tests.git ./tests
  if [ -z "$RISCV" ]; then
    echo "Error: RISCV environment variable is not set."
    exit 1
  fi

  if ! command -v riscv64-unknown-elf-gcc &>/dev/null; then
    echo "Error: riscv64-unknown-elf-gcc could not be found."
    exit 1
  fi
  cd tests
  ./configure --prefix=$RISCV/target
  make
  cd ..
fi

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <path_to_your_binary> <test_pattern>"
  exit 1
fi

BINARY_PATH=$1
TEST_PATTERN=$2

# ANSI Color Codes
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

find tests/isa -type f -name "${TEST_PATTERN}*" -executable -not -name "*.dump" | while read test_file; do
  echo "--- Running test: $test_file ---"
  $BINARY_PATH "$test_file"
  if [ $? -eq 0 ]; then
    echo -e "${GREEN}Test passed: $test_file${NC}"
  else
    echo -e "${RED}Test failed: $test_file${NC}"
  fi
done

echo "--- All tests finished. ---"
