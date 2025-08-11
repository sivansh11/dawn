cmake_minimum_required(VERSION 3.10)

project(simple)

add_executable(simple main.cpp)

target_link_libraries(simple PUBLIC dawn)
