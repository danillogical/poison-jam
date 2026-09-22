cmake_minimum_required(VERSION 3.20)

project(jsrf_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zi)
    add_link_options(/DEBUG:FULL /INCREMENTAL:NO)
endif()

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)
target_compile_definitions(xbox_kernel PRIVATE RECOMP_DIAGNOSTICS)

file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

# Fixture sources belong to test targets only.  A *_test.c left in the globbed
# directory is compiled into the game as well, and because a fixture defines a
# deliberate subset of the same trap addresses the production stubs define, the
# link fails with a wall of LNK2005 and a final LNK1169.  That is how
# recomp_stubs_recovery_test.c broke jsrf_recomp once the production stub file
# recovered its real 274 bodies.  Filter rather than trust the directory.
list(FILTER RECOMP_GEN_SOURCES EXCLUDE REGEX "recomp_.*_test\\.c$")

add_executable(${PROJECT_NAME} WIN32
    src/main.c
    src/recomp_manual.c
    src/jsrf_crt.c
    src/diagnostics.c
    src/recomp/recovered/recovered.c
    tests/harness_probes.c
    tests/video_probes.c
    tests/gpu_probes.c
    ${RECOMP_GEN_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)
target_compile_definitions(${PROJECT_NAME} PRIVATE RECOMP_DIAGNOSTICS)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${XBOXRECOMP_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    dxguid
    xinput
    winmm
    dbghelp
)

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /LARGEADDRESSAWARE /MAP)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

add_executable(jsrf_collect tools/harness/collect.c)
target_link_libraries(jsrf_collect PRIVATE dbghelp)
target_compile_definitions(jsrf_collect PRIVATE _CRT_SECURE_NO_WARNINGS)

include(CTest)
if(BUILD_TESTING)
    add_executable(jsrf_crt_test tests/test_memmove.c src/jsrf_crt.c)
    target_include_directories(jsrf_crt_test PRIVATE src)
    add_test(NAME jsrf_memmove COMMAND jsrf_crt_test)
    add_executable(jsrf_lifter_test tests/lifter-regressions.c)
    add_test(NAME jsrf_lifter_flags COMMAND jsrf_lifter_test)
    add_executable(jsrf_nv2a_test tests/test_nv2a_contract.c
        ${XBOXRECOMP_DIR}/src/nv2a/nv2a_core.c
        ${XBOXRECOMP_DIR}/src/nv2a/nv2a_method_table.c)
    target_include_directories(jsrf_nv2a_test PRIVATE ${XBOXRECOMP_DIR}/src/nv2a)
    target_link_libraries(jsrf_nv2a_test PRIVATE platform)
    add_test(NAME jsrf_nv2a_registers COMMAND jsrf_nv2a_test)
    add_executable(jsrf_recovery_11c1_test tests/test_recovery_11c1.c
        src/recomp/recovered/recovered_11c1_test.c
        src/recomp/gen/fixtures/recomp_stubs_recovery_test.c)
    target_include_directories(jsrf_recovery_11c1_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen)
    add_test(NAME jsrf_recovery_11c1 COMMAND jsrf_recovery_11c1_test)
    add_executable(jsrf_service_chain_test tests/test_service_chain.c)
    add_test(NAME jsrf_service_chain COMMAND jsrf_service_chain_test)
    set_tests_properties(jsrf_service_chain PROPERTIES TIMEOUT 15)
    add_executable(jsrf_callback_reentry_test tests/test_callback_reentry.c)
    add_test(NAME jsrf_callback_reentry COMMAND jsrf_callback_reentry_test)
    set_tests_properties(jsrf_callback_reentry PROPERTIES TIMEOUT 15)
    add_executable(jsrf_nv2a_hal_test tests/test_nv2a_hal.c)
    target_include_directories(jsrf_nv2a_hal_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen)
    target_link_libraries(jsrf_nv2a_hal_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_nv2a_hal COMMAND jsrf_nv2a_hal_test)
    add_executable(jsrf_inplace_event_test
        ${XBOXRECOMP_DIR}/tests/kernel_inplace_event_test.c)
    target_include_directories(jsrf_inplace_event_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen)
    target_link_libraries(jsrf_inplace_event_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_inplace_event_bridge COMMAND jsrf_inplace_event_test)
    set_tests_properties(jsrf_inplace_event_bridge PROPERTIES TIMEOUT 20)
    add_test(NAME jsrf_gpu_inspection COMMAND python -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gpu_inspection.py)
    add_executable(jsrf_gpu_smoke tests/native_gpu_smoke.c)
    target_link_libraries(jsrf_gpu_smoke PRIVATE d3d11 dxgi dxguid)
    find_path(RENDERDOC_INCLUDE_DIR renderdoc_app.h PATHS "$ENV{ProgramFiles}/RenderDoc")
    if(RENDERDOC_INCLUDE_DIR)
        target_include_directories(jsrf_gpu_smoke PRIVATE ${RENDERDOC_INCLUDE_DIR})
        target_compile_definitions(jsrf_gpu_smoke PRIVATE JSRF_HAVE_RENDERDOC)
    endif()
    # WARP gives an explicit portable reference; hardware is run by the capture script.
    add_test(NAME jsrf_native_gpu_warp COMMAND jsrf_gpu_smoke --warp)
    set_tests_properties(jsrf_native_gpu_warp PROPERTIES TIMEOUT 15)
endif()
