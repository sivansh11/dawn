cmake_minimum_required(VERSION 3.10)

project(memory)

add_executable(memory main.cpp)

target_link_libraries(memory PUBLIC dawn)
