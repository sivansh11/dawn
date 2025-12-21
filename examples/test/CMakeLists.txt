cmake_minimum_required(VERSION 3.10)

project(test)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  elfio
  GIT_REPOSITORY https://github.com/serge1/ELFIO.git
  GIT_TAG main
)
FetchContent_MakeAvailable(elfio)

add_executable(test main.cpp)

target_link_libraries(test
  PUBLIC elfio
)

target_link_libraries(test PUBLIC dawn)
