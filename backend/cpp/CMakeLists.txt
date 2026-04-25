cmake_minimum_required(VERSION 3.16)
project(SILOS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/Box.cpp
    src/Slot.cpp
    src/Pallet.cpp
    src/Shuttle.cpp
    src/Aisle.cpp
    src/Robot.cpp
    src/InputBelt.cpp
    src/Scheduler.cpp
    src/main.cpp
)

add_executable(silos ${SOURCES})

target_compile_options(silos PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
