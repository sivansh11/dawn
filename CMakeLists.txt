cmake_minimum_required(VERSION 3.10)

project(dawn)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  elfio
  GIT_REPOSITORY https://github.com/serge1/ELFIO.git
  GIT_TAG main
)
FetchContent_MakeAvailable(elfio)

FetchContent_Declare(
  unordered_dense
  GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
  GIT_TAG main
)
FetchContent_MakeAvailable(unordered_dense)

file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_library(dawn ${SRC_FILES})

set_target_properties(dawn PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(dawn PUBLIC -fno-omit-frame-pointer)

target_include_directories(dawn
  PUBLIC includes
)

target_link_libraries(dawn
  PUBLIC elfio
  # PUBLIC asmjit
  PUBLIC unordered_dense
)

if (DAWN_PROFILER)
  target_compile_options(dawn
    PUBLIC -pg
  )
endif()

if (DAWN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
