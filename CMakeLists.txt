cmake_minimum_required(VERSION 3.10)

project(dawn)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_library(dawn ${SRC_FILES})

target_include_directories(dawn
  PUBLIC includes
)

# target_compile_options(dawn PUBLIC -fsanitize=address)
# target_link_options(dawn PUBLIC -fsanitize=address)

if (DAWN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
