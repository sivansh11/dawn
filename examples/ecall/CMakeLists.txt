cmake_minimum_required(VERSION 3.10)

project(ecall)

add_executable(ecall main.cpp)

target_link_libraries(ecall PUBLIC dawn)
