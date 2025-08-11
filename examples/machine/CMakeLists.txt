cmake_minimum_required(VERSION 3.10)

project(machine)

add_executable(machine main.cpp)

target_link_libraries(machine PUBLIC dawn)
