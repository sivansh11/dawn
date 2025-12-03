cmake_minimum_required(VERSION 3.10)

project(linux)

add_executable(linux main.cpp)

target_link_libraries(linux PUBLIC dawn)
