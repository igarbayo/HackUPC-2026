cmake_minimum_required(VERSION 3.16)
project(SILOS CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(CORE_SOURCES
    src/Box.cpp
    src/Slot.cpp
    src/Pallet.cpp
    src/Shuttle.cpp
    src/Aisle.cpp
    src/Robot.cpp
    src/InputBelt.cpp
    src/Scheduler.cpp
    src/simulation.cpp
)

# ── standalone demo executable ───────────────────────────────────────────────
add_executable(silos ${CORE_SOURCES} src/main.cpp)

# ── robot unit/integration tests ─────────────────────────────────────────────
add_executable(test_robot ${CORE_SOURCES} tests/test_robot.cpp)


# ── aisle heuristic tests (hackupc.pdf desk-test scenario) ───────────────────
add_executable(test_aisle_heuristic ${CORE_SOURCES} tests/test_aisle_heuristic.cpp)

# ── aisle / box-selection tests ───────────────────────────────────────────────
add_executable(test_aisle ${CORE_SOURCES} tests/test_aisle.cpp)

# ── metadata update tests (countByFamily, avgDistanceByFamily) ────────────────
add_executable(test_metadata_update ${CORE_SOURCES} tests/test_metadata_update.cpp)

# ── aisle output tests  ───────────────────
add_executable(test_aisle_output ${CORE_SOURCES} tests/test_aisle_output.cpp)

# ── integration: retrieval sequence B, A, A ──────────────────────────────────
add_executable(test_integration_retrieval ${CORE_SOURCES} tests/integration/test_integration_retrieval.cpp)
# ── end-to-end integration tests (input heuristic → robot decide → output) ───
add_executable(test_integration ${CORE_SOURCES} tests/test_integration.cpp)

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
