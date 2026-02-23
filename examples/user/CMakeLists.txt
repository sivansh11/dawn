cmake_minimum_required(VERSION 3.10)

project(user)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

FetchContent_Declare(
  elfio
  GIT_REPOSITORY https://github.com/serge1/ELFIO.git
  GIT_TAG main
)
FetchContent_MakeAvailable(elfio)

add_executable(user main.cpp)

target_link_libraries(user
  PUBLIC elfio
)

target_link_libraries(user PUBLIC dawn)
