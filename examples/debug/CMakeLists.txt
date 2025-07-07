cmake_minimum_required(VERSION 3.10)

project(debug)

add_executable(debug main.cpp)

target_link_libraries(debug PUBLIC dawn)
