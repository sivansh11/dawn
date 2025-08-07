cmake_minimum_required(VERSION 3.10)

project(state)

add_executable(state main.cpp)

target_link_libraries(state PUBLIC dawn)
