cmake_minimum_required(VERSION 3.16)
project(SILOS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include tests)

set(CORE_SOURCES
    src/Box.cpp
    src/Slot.cpp
    src/Pallet.cpp
    src/Shuttle.cpp
    src/Aisle.cpp
    src/AisleContainer.cpp
    src/Robot.cpp
    src/InputBelt.cpp
    src/Scheduler.cpp
    src/simulation.cpp
)

# ── standalone demo executable ───────────────────────────────────────────────
add_executable(silos ${CORE_SOURCES} src/main.cpp)

# ── benchmark binary ─────────────────────────────────────────────────────────
add_executable(bench ${CORE_SOURCES} src/benchmark.cpp src/benchmark_main.cpp)

# ── unit tests ────────────────────────────────────────────────────────────────
add_executable(test_aisle          ${CORE_SOURCES} tests/test_aisle.cpp)
add_executable(test_aisle_heuristic ${CORE_SOURCES} tests/test_aisle_heuristic.cpp)
add_executable(test_metadata_update ${CORE_SOURCES} tests/test_metadata_update.cpp)
add_executable(test_robot          ${CORE_SOURCES} tests/test_robot.cpp)

# ── integration tests ─────────────────────────────────────────────────────────
add_executable(test_integration           ${CORE_SOURCES} tests/integration/test_integration.cpp)
add_executable(test_integration_retrieval ${CORE_SOURCES} tests/integration/test_integration_retrieval.cpp)

# ── AisleContainer routing tests (findBestInputSlot cost + multi-aisle routing) ─
add_executable(test_aisle_container ${CORE_SOURCES} tests/test_aisle_container.cpp)

target_compile_options(silos PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── pybind11 Python extension (only built when pybind11 is available) ────────
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(scheduler_cpp ${CORE_SOURCES} bindings/bindings.cpp)
    target_include_directories(scheduler_cpp PRIVATE include)
    target_compile_options(scheduler_cpp PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    )
    message(STATUS "pybind11 found — building scheduler_cpp Python module")
else()
    message(STATUS "pybind11 not found — skipping Python module (install with: pip install pybind11)")
endif()
