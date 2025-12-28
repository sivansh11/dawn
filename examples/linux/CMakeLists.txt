cmake_minimum_required(VERSION 3.10)

project(linux)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  elfio
  GIT_REPOSITORY https://github.com/serge1/ELFIO.git
  GIT_TAG main
)
FetchContent_MakeAvailable(elfio)

add_executable(linux main.cpp)

target_link_libraries(linux
  PUBLIC elfio
)

target_link_libraries(linux PUBLIC dawn)
