# C3-RR AC'97 reset gate: standalone mutant harness.
#
# Builds the SAME gate source (tests/ac97_reset_gate.c from the game repo)
# against an ARBITRARY toolkit source tree, so each scratch mutant of
# src/kernel/xbox_memory_layout.c can be exercised without touching either
# repo's tracked files.
#
# Usage:
#   cmake -S <this-dir> -B <build-dir> [-DXBOXRECOMP_DIR=<toolkit>] [-DGAME_XBE=<xbe>]
#   cmake --build <build-dir> --config Release
#   ctest --test-dir <build-dir> -C Release
#
# Defaults: XBOXRECOMP_DIR honours $ENV{XBOXRECOMP_DIR}, else ../../..//xboxrecomp
# (the suggested sibling layout); GAME_XBE defaults to the game repo's
# game/default.xbe next to the gate source.
cmake_minimum_required(VERSION 3.20)
project(ac97_reset_gate_mutant_harness C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# The toolkit under test. Mutant harnesses pass -DXBOXRECOMP_DIR=<mutant>/xboxrecomp.
if(DEFINED ENV{XBOXRECOMP_DIR} AND NOT "$ENV{XBOXRECOMP_DIR}" STREQUAL "")
    set(_gate_toolkit_default "$ENV{XBOXRECOMP_DIR}")
else()
    set(_gate_toolkit_default "${CMAKE_CURRENT_SOURCE_DIR}/../../..//xboxrecomp")
endif()
set(XBOXRECOMP_DIR "${_gate_toolkit_default}" CACHE PATH
    "Toolkit source tree under test (mutant copy or clean checkout)")

# The gate source lives in the game repo; this dir ships beside it.
set(GATE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../ac97_reset_gate.c")
set(GAME_XBE "${CMAKE_CURRENT_SOURCE_DIR}/../../game/default.xbe" CACHE FILEPATH
    "Real XBE image the gate initialises the layout from (read-only)")

if(NOT EXISTS "${XBOXRECOMP_DIR}/src/kernel/xbox_memory_layout.c")
    message(FATAL_ERROR "XBOXRECOMP_DIR does not look like a toolkit checkout: "
        "${XBOXRECOMP_DIR}/src/kernel/xbox_memory_layout.c missing")
endif()
if(NOT EXISTS "${GATE_SOURCE}")
    message(FATAL_ERROR "gate source missing: ${GATE_SOURCE}")
endif()

# Same umbrella the game build consumes, but rooted at the toolkit under test.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp-under-test)

add_executable(ac97_reset_gate_standalone ${GATE_SOURCE})
target_include_directories(ac97_reset_gate_standalone PRIVATE
    ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel)
target_link_libraries(ac97_reset_gate_standalone PRIVATE xboxrecomp)

include(CTest)
add_test(NAME ac97_reset_gate_standalone
    COMMAND ac97_reset_gate_standalone ${GAME_XBE})
set_tests_properties(ac97_reset_gate_standalone PROPERTIES TIMEOUT 120)
