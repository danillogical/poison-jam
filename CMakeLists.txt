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
    src/jsrf_save_root.c
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
    shell32
)

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /LARGEADDRESSAWARE /MAP)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

add_executable(jsrf_collect tools/harness/collect.c src/jsrf_save_root.c)
target_include_directories(jsrf_collect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(jsrf_collect PRIVATE dbghelp shell32)
target_compile_definitions(jsrf_collect PRIVATE _CRT_SECURE_NO_WARNINGS)

include(CTest)
if(BUILD_TESTING)
    add_executable(jsrf_save_root_test
        tests/test_save_root.c
        src/jsrf_save_root.c
        ${XBOXRECOMP_DIR}/src/kernel/kernel_path.c
        ${XBOXRECOMP_DIR}/src/kernel/kernel_io.c)
    target_include_directories(jsrf_save_root_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${XBOXRECOMP_DIR}/src
        ${XBOXRECOMP_DIR}/src/kernel)
    target_link_libraries(jsrf_save_root_test PRIVATE shell32 advapi32)
    add_test(NAME jsrf_save_root COMMAND jsrf_save_root_test)
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
    # A4b1 step 8 (AC-FIX).  Registered TWICE, on purpose: the trace is read
    # once and cached (apu_watch_trace_enabled), so one process can only ever
    # exercise one arm.  The trace-on arm checks the line-vs-snapshot rule; the
    # trace-off arm is the negative control -- the ledger and the [GPIN] arrays
    # are always computed, so every snapshot/PRAM/memory check must still hold
    # while the emission is absent (only the unconditional [GPDMA] unmapped line
    # survives).
    add_executable(jsrf_apu_watch_fixture_test
        ${XBOXRECOMP_DIR}/tests/apu_watch_fixture_test.c)
    target_include_directories(jsrf_apu_watch_fixture_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/apu
        ${XBOXRECOMP_DIR}/src/apu/dsp ${XBOXRECOMP_DIR}/src/apu/dsp/shim
        ${XBOXRECOMP_DIR}/src/kernel
        ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen)
    target_link_libraries(jsrf_apu_watch_fixture_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_apu_watch_fixture_trace_on COMMAND jsrf_apu_watch_fixture_test)
    set_tests_properties(jsrf_apu_watch_fixture_trace_on PROPERTIES
        TIMEOUT 60 ENVIRONMENT "RECOMP_APU_TRACE=1")
    add_test(NAME jsrf_apu_watch_fixture_trace_off COMMAND jsrf_apu_watch_fixture_test)
    set_tests_properties(jsrf_apu_watch_fixture_trace_off PROPERTIES TIMEOUT 60)

    # A4b2-NR (xii): the diagnostic GP input perturbation selectors. The mode is
    # cached read-once by apu_watch_perturb_mode(), so one process can only ever
    # exercise ONE mode -- hence one registration per mode, each process-isolated.
    # The `absent` arm is the trace_off registration above (it sets no perturb
    # variable), so inertness is covered there as well as here.
    #
    # A4b2-NR-next-edge-followup closure: the `bad-output` known-bad control has
    # been REMOVED. Its bite was preserved before removal -- archived run
    # 20260927-130142-334-a4b2-nr-badoutput shows GP_CLEAR=0 and GP_NONZERO_OVER=1
    # -- and the comparator validity it demonstrated is now carried by the
    # retained archived record plus the fixture arms below.
    foreach(_nr_mode "zero:0" "max:0" "prng:1")
        string(REPLACE ":" "_" _nr_tag "${_nr_mode}")
        add_test(NAME jsrf_apu_watch_fixture_perturb_${_nr_tag}
                 COMMAND jsrf_apu_watch_fixture_test)
        set_tests_properties(jsrf_apu_watch_fixture_perturb_${_nr_tag} PROPERTIES
            TIMEOUT 60
            ENVIRONMENT "RECOMP_APU_TRACE=1;RECOMP_APU_GP_INPUT_PERTURB=${_nr_mode}")
    endforeach()

    # A4b2-NR-next-edge-followup (xiii): the P-memory write watch. Process-isolated
    # because the gate is read once. The `absent` arm is the trace_off registration
    # above; this arm exercises the watch and asserts an image-I write, an above-I
    # write, and terminal reconciliation.
    #
    # The PASS_REGULAR_EXPRESSION is the point: the first version of this arm passed
    # VACUOUSLY (722 checks green) while the watch recorded ZERO events, because the
    # fixture passed a DSPState* where a dsp_core_t* was required and every write was
    # filtered as non-GP. The fixture now reads its own artifact back and prints this
    # marker only when the counts reconcile, so a vacuous pass cannot satisfy it.
    add_test(NAME jsrf_apu_watch_fixture_pwrite_watch
             COMMAND jsrf_apu_watch_fixture_test)
    set_tests_properties(jsrf_apu_watch_fixture_pwrite_watch PROPERTIES
        TIMEOUT 60
        ENVIRONMENT "RECOMP_APU_TRACE=1;RECOMP_APU_PWRITE_WATCH=1"
        PASS_REGULAR_EXPRESSION "AC-PWRITE events=2 in_image_i=1 above_image_i=1 ngp_skipped=0 invalid=0")
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
