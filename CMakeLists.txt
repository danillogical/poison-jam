cmake_minimum_required(VERSION 3.20)

project(jsrf_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Fixtures-only builds the tests that need no game data and no generated or recovered
# retail-derived code (so no jsrf_recomp and no jsrf_collect), against the toolkit's own
# runtime header and the empty vertex-program corpus.  It is OFF by default on every
# platform: leaving it OFF is exactly the build this file has always described.
option(JSRF_FIXTURES_ONLY "Build only tests that need no game data or generated code" OFF)
if(JSRF_FIXTURES_ONLY)
    # Lets the few sources that name the generated header by path take the toolkit's instead.
    add_compile_definitions(JSRF_FIXTURES_ONLY)
endif()

if(MSVC)
    add_compile_options(/Zi)
    add_link_options(/DEBUG:FULL /INCREMENTAL:NO)
endif()

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# The provenance guard resolves the ONE toolkit from this cache, so the default must follow
# $ENV{XBOXRECOMP_DIR} rather than silently overriding it.
if(DEFINED ENV{XBOXRECOMP_DIR} AND NOT "$ENV{XBOXRECOMP_DIR}" STREQUAL "")
    set(_jsrf_toolkit_default "$ENV{XBOXRECOMP_DIR}")
else()
    set(_jsrf_toolkit_default "${CMAKE_CURRENT_SOURCE_DIR}/../xboxrecomp")
endif()
set(XBOXRECOMP_DIR "${_jsrf_toolkit_default}" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)
target_compile_definitions(xbox_kernel PRIVATE RECOMP_DIAGNOSTICS)

# Where tests find recomp_types.h: the generated tree, or (fixtures-only) the toolkit template
# it is generated from.  Fixtures-only never globs, includes or reads src/recomp/gen.
if(JSRF_FIXTURES_ONLY)
    set(RECOMP_GEN_SOURCES "")
    set(JSRF_GEN_INCLUDE_DIR "${XBOXRECOMP_DIR}/templates/runtime")
else()
    set(JSRF_GEN_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen")
    file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")
endif()

# The code lifted from the XBE is not tracked (owner decision, 2026-09-30), so a
# fresh checkout has none until it is rebuilt from game/default.xbe.
if(NOT JSRF_FIXTURES_ONLY AND (NOT RECOMP_GEN_SOURCES OR NOT EXISTS
   "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/recovered/recovered.c"))
    message(FATAL_ERROR
        "src/recomp/gen and src/recomp/recovered are not tracked: rebuild them "
        "from game/default.xbe first (AGENTS.md, \"Rebuilding the lifted code\").")
endif()

# The interpreter the Python CTests run under: Windows keeps the bare `python` it always used;
# elsewhere `python` may be absent or Python 2, so use the Python 3 CMake finds.
if(WIN32)
    set(JSRF_PYTHON python)
else()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(JSRF_PYTHON "${Python3_EXECUTABLE}")
    else()
        set(JSRF_PYTHON python3)
    endif()
endif()
if(JSRF_FIXTURES_ONLY)
    # The Vulkan gate tests' build-time seam probes (add_custom_command below) run under this one.
    set(_jsrf_python "${JSRF_PYTHON}")
endif()

# Fixture sources belong to test targets only.  A *_test.c left in the globbed
# directory is compiled into the game as well, and because a fixture defines a
# deliberate subset of the same trap addresses the production stubs define, the
# link fails with a wall of LNK2005 and a final LNK1169.  That is how
# recomp_stubs_recovery_test.c broke jsrf_recomp once the production stub file
# recovered its real 274 bodies.  Filter rather than trust the directory.
list(FILTER RECOMP_GEN_SOURCES EXCLUDE REGEX "recomp_.*_test\\.c$")

# ── The NV2A vertex-program corpus: the bound capability selects exactly ONE ─────────────────────
#
# Vulkan is OPTIONAL.  The toolkit's cmake/XbrVulkan.cmake decides ONE capability and
# publishes it as the FORCE-refreshed CACHE bool XBR_VULKAN_FOUND, with the reason it is
# FALSE in the cache STRING XBR_VULKAN_UNAVAILABLE_REASON.  The CACHE is the channel
# because a subdirectory's plain variable never reaches its parent (which is why this file
# used to re-derive the decision by hand, with a slightly different rule, at the V6 block
# below), while a cache entry is visible to every directory here, is recorded in
# <build>/CMakeCache.txt, and is recomputed on every configure -- so this branch and every
# target under it read ONE value that no earlier configure can have left behind.
#
# Capability ON -- the loader, its headers, dxc and spirv-val are all present: the real,
# retail-derived corpus is REQUIRED, validated inside THIS configure by the shipped R1
# guard, and compiled.  Exactly one corpus reaches the compiler and it is a validated one.
# Capability OFF -- they are not: a generated empty table (nv2a_vsh_program_count = 0) is
# compiled instead, and the retail corpus is neither globbed, nor included, nor compiled,
# EVEN WHEN IT IS PRESENT in the source tree.  The back end is already a stub without
# XBR_VK_BACKEND (src/video/nv2a_vk_backend.c), so program batches fall back to the
# executor's CPU interpreter -- nothing at run time translates or compiles a program.
#
# Either way, ONE variable pair describes the corpus; the exe and every test target below
# use it, so the two branches cannot drift.
#
# JSRF_FIXTURES_ONLY takes the capability-OFF branch even where the capability is ON: there is
# no retail corpus to require or validate, but the Vulkan back end and its fixture tests are
# still built against the empty table.
if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)
    # The corpus, generated by scripts/gen-shaders.py into the same ignored tree.  It is
    # retail-derived and untracked for the same reason src/recomp/gen is, so a fresh
    # checkout has none until `just shaders` runs; the game then links the table and
    # registers it with the Vulkan back end (src/main.c).
    file(GLOB RECOMP_SHADER_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/shaders/*.c")
    if(NOT RECOMP_SHADER_SOURCES)
        message(FATAL_ERROR
            "src/recomp/gen/shaders is not tracked: the ahead-of-time NV2A "
            "vertex-program corpus must be generated first -- run `just shaders`. "
            "(Vulkan, dxc and spirv-val are all available (XBR_VULKAN_FOUND=TRUE), so "
            "this configure requires the retail corpus; it is only omitted where the "
            "shader toolchain is absent, and there it is replaced by a generated "
            "empty table.)")
    endif()

    # The provenance of the corpus is validated INSIDE this configure, with the shipped R1
    # guard, so a real corpus cannot reach the compiler unvalidated -- not through
    # `just build`, not through `just shaders`, and not through a direct
    # `cmake -S . -B build`.  It re-derives the provenance from the toolkit sources and the
    # executable the repository owns, and refuses a corpus those have moved past.  One
    # check, no build, no write to the corpus.  `--expect-capability-on` makes the corpus
    # check UNCONDITIONAL, so a disagreement between the guard's own capability probe and
    # the decision above can never skip the validation.  XBOXRECOMP_DIR is forwarded in the
    # child environment, so a hand-run guard resolves the same toolkit this configure links.
    #
    # The guard validates the corpus in the configure THAT COMPILES it -- this one -- and the
    # `CMAKE_CONFIGURE_DEPENDS` list below is what keeps that true: a later change to a corpus
    # file, to the executable whose sha256 the corpus records, or to a toolkit translator
    # source whose content it records forces a RECONFIGURE, which re-runs this guard, rather
    # than being compiled on the strength of this configure's verdict.
    #
    # `--toolkit` and `--probe-parent` are passed so this check binds ITSELF to the toolkit
    # and the build tree of THIS configure.  Without them the guard resolves the toolkit
    # through the MAIN build tree's cache (`build/CMakeCache.txt`), so a second build tree
    # configured with a different -DXBOXRECOMP_DIR could be refused with a conflict naming a
    # toolkit it never mentioned, and its throwaway capability probe would be created under
    # `build/` -- a build tree this configure does not own.  A `--toolkit` path is still
    # validated as a real toolkit checkout, and it is the tree the guard validates against
    # and the probe is asked about.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        set(_jsrf_python "${Python3_EXECUTABLE}")
    else()
        find_program(_jsrf_python NAMES python python3)
    endif()
    if(NOT _jsrf_python)
        message(FATAL_ERROR
            "The NV2A vertex-program corpus guard needs a Python 3 interpreter "
            "(scripts/build-jsrf.py) and none was found. Install Python 3, or pass "
            "-DPython3_EXECUTABLE=<python>; Vulkan and its shader tools are available, so "
            "the retail corpus must be validated before it can be compiled.")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "XBOXRECOMP_DIR=${XBOXRECOMP_DIR}"
                "${_jsrf_python}" -X utf8 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-jsrf.py"
                --check-corpus-only --expect-capability-on
                --toolkit "${XBOXRECOMP_DIR}"
                --probe-parent "${CMAKE_BINARY_DIR}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        RESULT_VARIABLE _jsrf_corpus_result
        OUTPUT_VARIABLE _jsrf_corpus_stdout
        ERROR_VARIABLE _jsrf_corpus_stderr)
    if(NOT _jsrf_corpus_result EQUAL 0)
        message(FATAL_ERROR
            "The NV2A vertex-program corpus did not pass its provenance guard "
            "(scripts/build-jsrf.py --check-corpus-only --expect-capability-on exited "
            "${_jsrf_corpus_result}), so this configure will not compile it:\n"
            "${_jsrf_corpus_stdout}${_jsrf_corpus_stderr}"
            "The corpus is retail-derived generated output; do not edit it. Regenerate it "
            "with `just shaders` and re-run this configure.")
    endif()
    list(LENGTH RECOMP_SHADER_SOURCES _jsrf_corpus_files)
    message(STATUS
        "Vulkan/available: the retail NV2A vertex-program corpus is required, its "
        "provenance is validated in this configure by scripts/build-jsrf.py, and it is "
        "compiled (${_jsrf_corpus_files} file(s) from src/recomp/gen/shaders).")

    # Every input the guard above compared, listed as a configure dependency of this
    # directory: a change to ANY of them must force a RECONFIGURE -- which re-runs the guard,
    # in the very configure that compiles the corpus -- instead of being compiled on the
    # strength of this configure's verdict.
    #
    # The `CONFIGURE_DEPENDS` glob above only notices the corpus FILE LIST changing, so the
    # two files it already matches are named explicitly: editing one of them leaves the list
    # identical, and the compiler would then read a table no configure validated.  The
    # executable is the one whose sha256 the corpus records: the OWNED tool tree under this
    # source tree, in any layout it can take (`scripts/gen-shaders.py` searches
    # `<game>/build/vsh-tool` and never this build tree's copy, because `just build` relinks
    # that one and its hash would move without an input moving).  The three sources are
    # `vsh_aot_provenance.SOURCE_NAMES`, whose CONTENT the corpus records, read from the
    # toolkit this configure links.
    #
    # A path that does not exist is inert here: CMake neither reconfigure-loops on it nor
    # treats it as present, so the layouts and sources that are absent are simply never
    # triggers.
    set(_jsrf_provenance_inputs
        "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen/shaders/nv2a_vsh_programs.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen/shaders/nv2a_vsh_programs.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/src/video/Release/nv2a_vsh_hlsl.exe"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/src/video/nv2a_vsh_hlsl.exe"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/xboxrecomp/src/video/Release/nv2a_vsh_hlsl.exe"
        "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/xboxrecomp/src/video/nv2a_vsh_hlsl.exe"
        "${XBOXRECOMP_DIR}/src/video/nv2a_vsh_hlsl.c"
        "${XBOXRECOMP_DIR}/src/video/nv2a_vsh_hlsl.h"
        "${XBOXRECOMP_DIR}/src/video/nv2a_vsh_hlsl_tool.c")
    if(NOT WIN32)
        # The same owned-tool layouts without the `.exe` suffix, which only Windows adds.
        list(APPEND _jsrf_provenance_inputs
            "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/src/video/Release/nv2a_vsh_hlsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/src/video/nv2a_vsh_hlsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/xboxrecomp/src/video/Release/nv2a_vsh_hlsl"
            "${CMAKE_CURRENT_SOURCE_DIR}/build/vsh-tool/xboxrecomp/src/video/nv2a_vsh_hlsl")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${_jsrf_provenance_inputs})
    # A `JSRF_NV2A_VSH_HLSL` override names the tool the guard will hash when it is set (a
    # corpus recording another tool is refused, and this is the tool in force), so it is a
    # provenance input for the same reason and is watched the same way -- listed as the
    # environment spells it, which is how the guard resolves it too.  Empty: not listed.
    if(NOT "$ENV{JSRF_NV2A_VSH_HLSL}" STREQUAL "")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "$ENV{JSRF_NV2A_VSH_HLSL}")
    endif()

    set(JSRF_CORPUS_SOURCES ${RECOMP_SHADER_SOURCES})
    set(JSRF_CORPUS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen/shaders")
else()
    # The empty corpus is GENERATED BUILD OUTPUT under <build>, never the source tree: it
    # is not retail-derived and it must never be able to look like a validated table (no
    # provenance block, and count = 0).  A zero-length array is not valid C, so the table
    # is one sentinel entry that count = 0 keeps out of reach of every reader.
    set(_jsrf_empty_dir "${CMAKE_BINARY_DIR}/empty-corpus")
    file(MAKE_DIRECTORY "${_jsrf_empty_dir}")
    # `file(CONFIGURE ...)` rather than `file(WRITE ...)`: the output -- and with it the
    # timestamp the compiler and the linker compare against -- is only written when the
    # content actually differs, so a no-change reconfigure (every OFF configure of a build
    # that is already up to date) recompiles nothing and relinks nothing.  `@ONLY` keeps the
    # content literal; the default newline style is what `file(WRITE ...)` produced byte for
    # byte, so the generated table is unchanged.
    string(CONCAT _jsrf_empty_h
        "/*\n"
        " * The NV2A vertex-program corpus seam, generated by the game's CMakeLists.txt\n"
        " * because Vulkan and its shader tools are NOT available here.\n"
        " *\n"
        " * This is generated BUILD OUTPUT, not a retail-derived table: it carries no\n"
        " * provenance block and nv2a_vsh_program_count is 0.  It exists only so that\n"
        " * src/main.c compiles and links unchanged -- nv2a_vk_backend_set_programs() gets\n"
        " * an empty table, the Vulkan back end is not built at all, and every program\n"
        " * batch is drawn by the executor's CPU interpreter.\n"
        " *\n"
        " * Where Vulkan and its shader tools are available, `just shaders` generates the\n"
        " * real corpus in src/recomp/gen/shaders and this configure compiles that instead.\n"
        " */\n"
        "#ifndef JSRF_NV2A_VSH_PROGRAMS_H\n"
        "#define JSRF_NV2A_VSH_PROGRAMS_H\n\n"
        "#include <stddef.h>\n\n"
        "#include \"nv2a_vsh_aot.h\"\n\n"
        "/* Ascending by hash; the back end binary-searches it.  Empty: count is 0. */\n"
        "extern const Nv2aVshAotEntry nv2a_vsh_programs[];\n"
        "extern const size_t nv2a_vsh_program_count;\n\n"
        "#endif /* JSRF_NV2A_VSH_PROGRAMS_H */\n")
    file(CONFIGURE OUTPUT "${_jsrf_empty_dir}/nv2a_vsh_programs.h"
        CONTENT "${_jsrf_empty_h}" @ONLY)
    string(CONCAT _jsrf_empty_c
        "/*\n"
        " * The empty NV2A vertex-program corpus: generated build output, NOT a\n"
        " * retail-derived table.  See nv2a_vsh_programs.h beside this file.\n"
        " *\n"
        " * The public interface is the real one (same declarations, same types), so\n"
        " * src/main.c compiles and links unchanged, but count = 0 means no program can\n"
        " * ever be looked up: nv2a_vk_backend_set_programs() receives an empty table and\n"
        " * every program batch falls back to the executor's CPU interpreter.\n"
        " */\n"
        "#include \"nv2a_vsh_aot.h\"\n\n"
        "/* One sentinel entry: a zero-length array is not valid C, and count = 0 keeps\n"
        " * this entry out of reach of every reader (the back end's lookup, the corpus\n"
        " * tests, and any future one). */\n"
        "const Nv2aVshAotEntry nv2a_vsh_programs[] = {\n"
        "    { 0u, 0u, 0u, 0u, NULL, NULL, 0u },\n"
        "};\n\n"
        "const size_t nv2a_vsh_program_count = 0;\n")
    file(CONFIGURE OUTPUT "${_jsrf_empty_dir}/nv2a_vsh_programs.c"
        CONTENT "${_jsrf_empty_c}" @ONLY)
    set(JSRF_CORPUS_SOURCES "${_jsrf_empty_dir}/nv2a_vsh_programs.c")
    set(JSRF_CORPUS_INCLUDE_DIR "${_jsrf_empty_dir}")
    if(XBR_VULKAN_FOUND)
        message(STATUS
            "JSRF_FIXTURES_ONLY: Vulkan is available, but this build compiles the generated "
            "empty vertex-program corpus at ${_jsrf_empty_dir} (nv2a_vsh_program_count = 0) and "
            "does not require or validate the retail corpus.")
    else()
    message(STATUS
        "Vulkan/unavailable: ${XBR_VULKAN_UNAVAILABLE_REASON}. This build compiles the "
        "generated empty vertex-program corpus at ${_jsrf_empty_dir} "
        "(nv2a_vsh_program_count = 0); the retail corpus in src/recomp/gen/shaders is "
        "neither included nor compiled, and program batches fall back to the executor's "
        "CPU interpreter. Where Vulkan and its shader tools are available, `just shaders` "
        "generates the retail corpus and this configure compiles it instead.")
    endif()
endif()

# The game and the collector need the lifted code and the Windows host; fixtures-only has neither.
if(NOT JSRF_FIXTURES_ONLY)
add_executable(${PROJECT_NAME} WIN32
    src/main.c
    src/jsrf_save_root.c
    src/recomp_manual.c
    src/jsrf_crt.c
    src/diagnostics.c
    src/jsrf_fatal_observe.c
    src/recomp/recovered/recovered.c
    tests/harness_probes.c
    tests/video_probes.c
    tests/gpu_probes.c
    ${RECOMP_GEN_SOURCES}
    ${JSRF_CORPUS_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp)
target_compile_definitions(${PROJECT_NAME} PRIVATE RECOMP_DIAGNOSTICS)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    # The corpus header (nv2a_vsh_programs.h), which src/main.c includes: the source
    # corpus dir when Vulkan and its shader tools are available, and the generated
    # empty corpus under <build> when they are not.
    ${JSRF_CORPUS_INCLUDE_DIR}
    ${XBOXRECOMP_DIR}/include
    # nv2a_vsh_aot.h (the corpus's entry type and digest), reached by the
    # generated table and by nv2a_vk_backend.h.
    ${XBOXRECOMP_DIR}/src/video
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        d3d11
        dxgi
        dxguid
        xinput
        winmm
        dbghelp
        shell32
    )
endif()

if(MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE /LARGEADDRESSAWARE /MAP)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

add_executable(jsrf_collect tools/harness/collect.c src/jsrf_save_root.c)
target_include_directories(jsrf_collect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(jsrf_collect PRIVATE dbghelp shell32)
endif()
target_compile_definitions(jsrf_collect PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# MSVC CRT spellings (_putenv_s, _snprintf, <io.h>, ...) the portable tests use: mapped onto
# POSIX by a header force-included on non-Windows hosts only, so Windows compiles them unchanged.
if(NOT WIN32)
    # -fno-asm: the tests use `asm` as an identifier, which MSVC allows and GNU C reserves.
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-fno-asm>
        $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/tests/compat/jsrf_test_compat.h>)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tests/compat)
endif()

include(CTest)
if(BUILD_TESTING)
    # The Vulkan device tests pin one ICD and select one device.  Windows (and any host that
    # sets XBR_VK_SW_ICD) keeps the software ICD and llvmpipe exactly as before.  On macOS with
    # no software ICD they run on MoltenVK: the SDK's ICD, picked by driver name (RECOMP_VK_DEVICE
    # is set EMPTY so a test's own llvmpipe default, which only applies when it is unset, cannot
    # add a device-name constraint no Apple device meets).
    set(JSRF_VK_TEST_ICD "${XBR_VK_SW_ICD}")
    set(JSRF_VK_TEST_SELECT "RECOMP_VK_DEVICE=llvmpipe")
    set(JSRF_VK_TEST_MOLTENVK FALSE)
    if(APPLE AND NOT JSRF_VK_TEST_ICD AND XBR_VULKAN_FOUND)
        set(_jsrf_mvk_icd "")
        if(XBR_VK_TEST_DRIVER AND EXISTS "${XBR_VK_TEST_DRIVER}")
            set(_jsrf_mvk_icd "${XBR_VK_TEST_DRIVER}")
        else()
            # Probed afresh each configure (no cache entry) so a moved SDK is never pinned stale.
            get_filename_component(_jsrf_vk_libdir "${Vulkan_LIBRARY}" DIRECTORY)
            foreach(_dir "$ENV{VULKAN_SDK}/share/vulkan/icd.d" "${_jsrf_vk_libdir}/../share/vulkan/icd.d"
                    /usr/local/share/vulkan/icd.d /opt/homebrew/share/vulkan/icd.d)
                if(EXISTS "${_dir}/MoltenVK_icd.json")
                    get_filename_component(_jsrf_mvk_icd "${_dir}/MoltenVK_icd.json" ABSOLUTE)
                    break()
                endif()
            endforeach()
        endif()
        if(_jsrf_mvk_icd)
            set(JSRF_VK_TEST_ICD "${_jsrf_mvk_icd}")
            set(JSRF_VK_TEST_SELECT "RECOMP_VK_DEVICE=;RECOMP_VK_DRIVER=MoltenVK")
            set(JSRF_VK_TEST_MOLTENVK TRUE)
            message(STATUS "Vulkan device tests run on MoltenVK: ${JSRF_VK_TEST_ICD}")
        endif()
    endif()
    if(WIN32)
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
    else()
        message(STATUS "jsrf_save_root is not registered off Windows -- it exercises the Win32 "
                       "save-root path layer and links shell32/advapi32")
    endif()
    add_executable(jsrf_crt_test tests/test_memmove.c src/jsrf_crt.c)
    target_include_directories(jsrf_crt_test PRIVATE src)
    add_test(NAME jsrf_memmove COMMAND jsrf_crt_test)
    add_executable(jsrf_crt_divide_test tests/test_crt_divide.c src/jsrf_crt.c)
    target_include_directories(jsrf_crt_divide_test PRIVATE src)
    if(JSRF_FIXTURES_ONLY)
        target_include_directories(jsrf_crt_test PRIVATE ${JSRF_GEN_INCLUDE_DIR})
        target_include_directories(jsrf_crt_divide_test PRIVATE ${JSRF_GEN_INCLUDE_DIR})
    endif()
    add_test(NAME jsrf_crt_divide COMMAND jsrf_crt_divide_test)
    add_executable(jsrf_fatal_ret_name_test tests/test_fatal_ret_name.c src/jsrf_fatal_observe.c)
    target_include_directories(jsrf_fatal_ret_name_test PRIVATE src)
    add_test(NAME jsrf_fatal_ret_name COMMAND jsrf_fatal_ret_name_test)
    add_executable(jsrf_lifter_test tests/lifter-regressions.c)
    add_test(NAME jsrf_lifter_flags COMMAND jsrf_lifter_test)
    add_executable(jsrf_nv2a_test tests/test_nv2a_contract.c
        ${XBOXRECOMP_DIR}/src/nv2a/nv2a_core.c
        ${XBOXRECOMP_DIR}/src/nv2a/nv2a_method_table.c)
    target_include_directories(jsrf_nv2a_test PRIVATE ${XBOXRECOMP_DIR}/src/nv2a)
    target_link_libraries(jsrf_nv2a_test PRIVATE platform)
    add_test(NAME jsrf_nv2a_registers COMMAND jsrf_nv2a_test)
    if(WIN32 AND NOT JSRF_FIXTURES_ONLY)
    add_executable(jsrf_recovery_11c1_test tests/test_recovery_11c1.c
        src/recomp/recovered/recovered_11c1_test.c
        src/recomp/gen/fixtures/recomp_stubs_recovery_test.c)
    target_include_directories(jsrf_recovery_11c1_test PRIVATE
        ${JSRF_GEN_INCLUDE_DIR})
    add_test(NAME jsrf_recovery_11c1 COMMAND jsrf_recovery_11c1_test)
    else()
        message(STATUS "jsrf_recovery_11c1 is not registered here (off Windows or fixtures-only) "
                       "-- it needs the recovered/generated retail seed and Win32 semantics")
    endif()
    add_executable(jsrf_service_chain_test tests/test_service_chain.c)
    add_test(NAME jsrf_service_chain COMMAND jsrf_service_chain_test)
    set_tests_properties(jsrf_service_chain PROPERTIES TIMEOUT 15)
    if(NOT WIN32)
        target_link_libraries(jsrf_service_chain_test PRIVATE platform)
    endif()
    add_executable(jsrf_callback_reentry_test tests/test_callback_reentry.c)
    add_test(NAME jsrf_callback_reentry COMMAND jsrf_callback_reentry_test)
    set_tests_properties(jsrf_callback_reentry PROPERTIES TIMEOUT 15)
    if(NOT WIN32)
        target_link_libraries(jsrf_callback_reentry_test PRIVATE platform)
    endif()
    # The HAL PCI and committed-method contracts go through the toolkit's NV2A MMIO hook
    # (src/nv2a/nv2a_mmio_hook.c), which is a stub that always returns false off Windows.
    if(WIN32)
    add_executable(jsrf_nv2a_hal_test tests/test_nv2a_hal.c)
    target_include_directories(jsrf_nv2a_hal_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_nv2a_hal_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_nv2a_hal COMMAND jsrf_nv2a_hal_test)
    else()
        message(STATUS "jsrf_nv2a_hal is not registered off Windows -- its HAL PCI and "
                       "committed-method contracts need the NV2A MMIO hook, a stub there")
    endif()
    # ── D4 coverage diagnostics, the committed-stream deciding gate ──────────────────────────
    # Authored by a worker distinct from the production change. It drives the real owner path
    # (guest RAM / XBE layout / instance RAMHT / commit consumer / real pushbuffer words) and
    # reads the executor's own nv2a_pb_exec_coverage() ledger. One process per case because the
    # gate is sampled once per process: the environment variants below are therefore separate
    # CTest entries, not one case that tries to flip the environment. The exact-1 gate must be
    # the environment's decision, so the on cases carry RECOMP_PB_COVERAGE=1 and the off
    # variants carry unset / empty / 0 / 01 / true.
    if(WIN32)
    add_executable(jsrf_pbcov_stream_test tests/test_pbcov_stream.c)
    target_include_directories(jsrf_pbcov_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_pbcov_stream_test PRIVATE xboxrecomp)
    set(PBCOV_STREAM_CASES
        ff_neither ff_texgen_only ff_texgen_zero_seen ff_texmat_only ff_both_stage_assoc
        ff_stage1_only ff_stage2_enable_no_matrix ff_stage3_enable_only
        ff_stage3_texgen_only
        stage6_isolated stage7_isolated stage6_7_multi stage_batch_vs_incidence
        stage_program_path_count stage_first_retained
        lit_nonqualifying lit_specular_only lit_spot_only lit_both_and_immutable
        first_records_immutable report_retained
        vp_port_0b00 vp_port_0b10 vp_port_0b70 vp_cursor_advance
        vp_component_unavailable vp_zero_observed vp_upload_past_end
        vp_start_out_of_range
        gate_on_identity subchannel_guard rejected_stream_rollback
        guards_stopped guards_sub_minimum entry_forms vtable_legacy_init)
    foreach(_case IN LISTS PBCOV_STREAM_CASES)
        add_test(NAME jsrf_pbcov_stream_${_case}
                 COMMAND jsrf_pbcov_stream_test --case ${_case})
        set_tests_properties(jsrf_pbcov_stream_${_case} PROPERTIES TIMEOUT 120
            ENVIRONMENT "RECOMP_PB_COVERAGE=1")
    endforeach()
    # The first-sample cases set/clear the variable themselves, but their CTest entries still
    # start from an off environment so the fixture, not the parent, decides the first sample.
    add_test(NAME jsrf_pbcov_stream_gate_first_on_wins
             COMMAND jsrf_pbcov_stream_test --case gate_first_sample_on_wins)
    set_tests_properties(jsrf_pbcov_stream_gate_first_on_wins PROPERTIES TIMEOUT 120
        ENVIRONMENT "RECOMP_PB_COVERAGE=1")
    add_test(NAME jsrf_pbcov_stream_gate_first_off_wins
             COMMAND jsrf_pbcov_stream_test --case gate_first_sample_off_wins)
    set_tests_properties(jsrf_pbcov_stream_gate_first_off_wins PROPERTIES TIMEOUT 120)
    # Off variants: unset, empty, 0, 01, true. The case also checks that CTest's value is not
    # the exact-1 gate, so a mis-wired environment cannot pass as an "off" case.
    if(NOT CMAKE_VERSION VERSION_LESS 3.22)
        add_test(NAME jsrf_pbcov_stream_gate_unset
                 COMMAND jsrf_pbcov_stream_test --case gate_off)
        set_tests_properties(jsrf_pbcov_stream_gate_unset PROPERTIES TIMEOUT 120
            ENVIRONMENT_MODIFICATION "RECOMP_PB_COVERAGE=unset:")
    else()
        add_test(NAME jsrf_pbcov_stream_gate_unset
                 COMMAND jsrf_pbcov_stream_test --case gate_off)
        set_tests_properties(jsrf_pbcov_stream_gate_unset PROPERTIES TIMEOUT 120
            ENVIRONMENT "RECOMP_PB_COVERAGE=")
    endif()
    foreach(_variant empty: 0 01 true)
        if(_variant STREQUAL "empty:")
            set(_env "RECOMP_PB_COVERAGE=")
            set(_name jsrf_pbcov_stream_gate_empty)
        else()
            set(_env "RECOMP_PB_COVERAGE=${_variant}")
            set(_name jsrf_pbcov_stream_gate_${_variant})
        endif()
        add_test(NAME ${_name} COMMAND jsrf_pbcov_stream_test --case gate_off)
        set_tests_properties(${_name} PROPERTIES TIMEOUT 120 ENVIRONMENT "${_env}")
    endforeach()
    else()
        message(STATUS "jsrf_pbcov_stream_* (the D4 committed-stream coverage gate, all cases) is "
                       "not registered off Windows -- it drives the Win32 guest-memory/window "
                       "layout, VEH guards and the commit consumer; the real-Vulkan pbcov cases "
                       "remain registered separately")
    endif()
    # ── BUMPENVMAP / BUMPENVMAP_LUM, the committed-stream D4 step-5 deciding gate ────────────
    # Authored by a worker distinct from the production change and from the step-4 focused
    # gate. Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) drive the
    # executor's method decoding, its CPU vertex-program interpreter and the production CPU
    # rasteriser and sampler from the linked toolkit library; each pixel read back from the
    # guest surface must equal a hand-derived literal (tests/bumpenv_stream_cases.h). The Vulkan
    # half (jsrf_bumpenv_stream_vk_*, registered with the other Vulkan gates) replays the
    # executor's own batch into the production vb_draw and checks the same literals. One
    # process per case: the executor and the NV2A core are process-wide.
    set(BUMPENV_STREAM_CASES
        mode6_signed_du mode6_signed_dv mode6_asymmetric_matrix mode6_stage2_source
        mode7_luminance mode7_saturation mode7_stage3_source
        bump_q_ignored project2d_q_divides
        refusal_dot_modes refusal_cube_mode3
        limitation_stage3_texture limitation_rect_bump limitation_signed_source
        limitation_cube_volume_dest)
    if(WIN32)
    add_executable(jsrf_bumpenv_stream_test tests/test_bumpenv_stream.c
        tests/bumpenv_stream_fixture.c)
    target_include_directories(jsrf_bumpenv_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_bumpenv_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS BUMPENV_STREAM_CASES)
        add_test(NAME jsrf_bumpenv_stream_${_case}
                 COMMAND jsrf_bumpenv_stream_test --case ${_case})
        set_tests_properties(jsrf_bumpenv_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_bumpenv_stream_* (the D4 step-5 committed-stream bump gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_pbcov_stream_* does")
    endif()
    # ── Fixed-function texgen and texture matrices, the committed-stream D4 step-6 gate ──────
    # Authored by a worker distinct from the production change. Real pushbuffer words through
    # nv2a_submit_pending (RECOMP_PB_EXEC=1) set the composite (so batches are fixed-function),
    # model-view, inverse model-view, normals, texgen modes and planes, texture-matrix enables and
    # words, and textures; the executor's decoding and CPU fixed-function rasteriser come from the
    # linked toolkit library, and each pixel read back from the guest surface must equal a
    # hand-derived literal (tests/texgen_stream_cases.h). The Vulkan half (jsrf_texgen_stream_vk_*,
    # registered with the other Vulkan gates) replays the executor's own batch into the production
    # vb_draw and checks the same literals. One process per case.
    set(TEXGEN_STREAM_CASES
        mode_eye_linear mode_object_linear mode_sphere_map mode_normal_map mode_reflection_map
        mode_disable_passthrough jsrf_observed_state texmat_asymmetric imv_nonuniform_scale
        normalization_enable eye_plane_modelview texmat_enable stage1_texgen
        vertex_program_unaffected texel_scaling texmat_q_projective
        limitation_texgen_q limitation_stage23_matrix limitation_viewer_model)
    if(WIN32)
    add_executable(jsrf_texgen_stream_test tests/test_texgen_stream.c
        tests/texgen_stream_fixture.c)
    target_include_directories(jsrf_texgen_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_texgen_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS TEXGEN_STREAM_CASES)
        add_test(NAME jsrf_texgen_stream_${_case}
                 COMMAND jsrf_texgen_stream_test --case ${_case})
        set_tests_properties(jsrf_texgen_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_texgen_stream_* (the D4 step-6 committed-stream texgen gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_bumpenv_stream_* does")
    endif()
    # ── NV2A window clip, the committed-stream gate (test first) ─────────────────────────────
    # Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) clear a 64x64 surface,
    # write SET_WINDOW_CLIP_TYPE / _HORIZONTAL / _VERTICAL, and draw one solid untextured quad;
    # the executor's CPU rasteriser comes from the linked toolkit library, and each named pixel
    # read back from the guest surface must equal a hand-derived literal
    # (tests/wclip_stream_cases.h). The Vulkan half (jsrf_wclip_stream_vk_*, registered with the
    # other Vulkan gates) replays the executor's own batch into the production vb_draw and
    # labels its one-scissor limitations. One process per case.
    set(WCLIP_STREAM_CASES single full exclusive beyond_surface_clip aa2x2)
    if(WIN32)
    add_executable(jsrf_wclip_stream_test tests/test_wclip_stream.c
        tests/wclip_stream_fixture.c)
    target_include_directories(jsrf_wclip_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_wclip_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS WCLIP_STREAM_CASES)
        add_test(NAME jsrf_wclip_stream_${_case}
                 COMMAND jsrf_wclip_stream_test --case ${_case})
        set_tests_properties(jsrf_wclip_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_wclip_stream_* (the NV2A window-clip committed-stream gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_texgen_stream_* does")
    endif()
    # ── NV2A X8R8G8B8 texture alpha, the committed-stream gate (test first) ──────────────────
    # Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) clear a 32x32 render
    # target to alpha 0x40, then draw a quad textured by that target's own address and pitch as
    # LIN_X8R8G8B8 (0x1E) or LIN_A8R8G8B8 (0x12), through fixed function or the register
    # combiners; each named pixel read back from the guest surface must equal a hand-derived
    # literal (tests/x8alias_stream_cases.h): 0x1E reads alpha 0xFF, 0x12 the stored alpha. The
    # CPU half passes today; the Vulkan half (jsrf_x8alias_stream_vk_*, registered with the other
    # Vulkan gates) is red on the 0x1E render-target alias paths. One process per case.
    set(X8ALIAS_STREAM_CASES x8_ff a8_ff x8_rc a8_rc x8_self)
    if(WIN32)
    add_executable(jsrf_x8alias_stream_test tests/test_x8alias_stream.c
        tests/x8alias_stream_fixture.c)
    target_include_directories(jsrf_x8alias_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_x8alias_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS X8ALIAS_STREAM_CASES)
        add_test(NAME jsrf_x8alias_stream_${_case}
                 COMMAND jsrf_x8alias_stream_test --case ${_case})
        set_tests_properties(jsrf_x8alias_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_x8alias_stream_* (the NV2A X8R8G8B8 alpha committed-stream gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_wclip_stream_* does")
    endif()
    # ── NV2A depth per zeta address, the committed-stream gate (test first) ──────────────────
    # Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) draw into one colour
    # target with depth test LESS, switching SET_SURFACE_ZETA_OFFSET between a draw, a depth-only
    # clear and a second draw; each named pixel read back from the guest surface must equal a
    # hand-derived literal (tests/zeta_stream_cases.h): a different zeta address keeps its own
    # depth (`distinct`), the same address shares it (`same`); `_vp` cases draw in program mode.
    # The CPU fixed-function rasteriser has no depth test, so its `distinct` expects a labelled
    # LIMITATION literal; the other cases hold the hardware rule on both halves. One process per case.
    set(ZETA_STREAM_CASES distinct same distinct_vp same_vp)
    if(WIN32)
    add_executable(jsrf_zeta_stream_test tests/test_zeta_stream.c
        tests/zeta_stream_fixture.c)
    target_include_directories(jsrf_zeta_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_zeta_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS ZETA_STREAM_CASES)
        add_test(NAME jsrf_zeta_stream_${_case}
                 COMMAND jsrf_zeta_stream_test --case ${_case})
        set_tests_properties(jsrf_zeta_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_zeta_stream_* (the NV2A per-zeta depth committed-stream gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_x8alias_stream_* does")
    endif()
    # ── NV2A fixed-function vertex lighting, the committed-stream gate (test first) ──────────
    # Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) draw one untextured lit
    # quad (SET_LIGHTING_ENABLE, one infinite light) with a chosen model-view, inverse model-view,
    # normalisation and light direction; each named pixel read back from the guest surface must
    # equal a hand-derived literal (tests/light_stream_cases.h): the normal comes from the inverse
    # model-view rows, is normalised only when enabled, the light direction is normalised, and a
    # normal facing away from the eye is flipped (ledger L82). The Vulkan half
    # (jsrf_light_stream_vk_*) replays the executor's own batch into the production vb_draw.
    # One process per case.
    set(LIGHT_STREAM_CASES sign_toward sign_away imv_scale_off imv_scale_on imv_rows dir_unit flip)
    if(WIN32)
    add_executable(jsrf_light_stream_test tests/test_light_stream.c
        tests/light_stream_fixture.c)
    target_include_directories(jsrf_light_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_light_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS LIGHT_STREAM_CASES)
        add_test(NAME jsrf_light_stream_${_case}
                 COMMAND jsrf_light_stream_test --case ${_case})
        set_tests_properties(jsrf_light_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_light_stream_* (the NV2A vertex-lighting committed-stream gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_zeta_stream_* does")
    endif()
    # ── NV2A fixed-function SPECULAR, the committed-stream gate (test first) ─────────────────
    # Real pushbuffer words through nv2a_submit_pending (RECOMP_PB_EXEC=1) draw one untextured
    # lit quad with the register combiners programmed so the final combiner emits either oD1
    # (D = V1) or oD0 (C = V0). That is the only way to observe the specular output. Each named
    # pixel read back from the guest surface must equal a hand-derived literal
    # (tests/spec_stream_cases.h): lit oD1.rgb is 0 when SET_SPECULAR_ENABLE is off and 0 when
    # N.L <= 0, and with SEPARATE_SPECULAR set oD0 is the Q5a lighting result unchanged. The
    # cases that need SET_SPECULAR_PARAMS (0x09E0..0x09F4) are PENDING: the walk does not admit
    # those methods, so they cannot be delivered without a runtime witness. The Vulkan half
    # (jsrf_spec_stream_vk_*) replays the executor's own batch into the production vb_draw and
    # combiner and is the deciding half; the CPU fixed-function rasteriser has no combiners, so
    # its cases carry separate labelled CPU literals. One process per case.
    set(SPEC_STREAM_CASES pc_unlit_d1 pc_unlit_d0 lit_d0_separate lit_d1_spec_off lit_d1_nl_negative)
    if(WIN32)
    add_executable(jsrf_spec_stream_test tests/test_spec_stream.c
        tests/spec_stream_fixture.c)
    target_include_directories(jsrf_spec_stream_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    target_link_libraries(jsrf_spec_stream_test PRIVATE xboxrecomp)
    foreach(_case IN LISTS SPEC_STREAM_CASES)
        add_test(NAME jsrf_spec_stream_${_case}
                 COMMAND jsrf_spec_stream_test --case ${_case})
        set_tests_properties(jsrf_spec_stream_${_case} PROPERTIES TIMEOUT 120)
    endforeach()
    else()
        message(STATUS "jsrf_spec_stream_* (the NV2A specular committed-stream gate) is not "
                       "registered off Windows -- it drives the Win32 guest-memory layout and "
                       "the commit consumer, as jsrf_light_stream_* does")
    endif()
    # This one runs the pushbuffers through the Direct3D 11 back end and exits 0 after printing
    # SKIP when it cannot create a device, which off Windows is always: a green no-op, so it is
    # not registered there (jsrf_gpu_backend_parity_vk is the Vulkan sibling that runs).
    if(WIN32)
    add_executable(jsrf_gpu_backend_parity_test tests/test_gpu_backend_parity.c)
    target_include_directories(jsrf_gpu_backend_parity_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_gpu_backend_parity_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_gpu_backend_parity COMMAND jsrf_gpu_backend_parity_test)
    set_tests_properties(jsrf_gpu_backend_parity PROPERTIES TIMEOUT 60)
    # The back end reads RECOMP_GPU_VSH once per process, so the CPU-transform fallback of the
    # vertex-program path is its own process: the same binary, V7 only.
    add_test(NAME jsrf_gpu_backend_parity_vsh_off
             COMMAND jsrf_gpu_backend_parity_test --vsh-off-only)
    set_tests_properties(jsrf_gpu_backend_parity_vsh_off PROPERTIES
        TIMEOUT 60 ENVIRONMENT "RECOMP_GPU_VSH=0")
    else()
        message(STATUS "jsrf_gpu_backend_parity and jsrf_gpu_backend_parity_vsh_off are not "
                       "registered off Windows -- they need a Direct3D 11 device and would only SKIP")
    endif()
    # The Vulkan sibling: the same pushbuffers against the Vulkan back end on a software driver
    # (lavapipe, XBR_VK_SW_ICD). It skips (77) when no software ICD is configured or when the back end
    # cannot be installed -- notably in an elevated process, where the loader ignores VK_DRIVER_FILES.
    # Registered only with the capability on: without Vulkan the back end is a stub that reports
    # failure, so there is nothing for this test to ask, and an unregistered test is honest where a
    # green no-op would not be.
    if(XBR_VULKAN_FOUND)
        add_executable(jsrf_gpu_backend_parity_vk_test tests/test_gpu_backend_parity_vk.c
            tests/vk_suballoc_oracles.h)
        target_include_directories(jsrf_gpu_backend_parity_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${JSRF_GEN_INCLUDE_DIR})
        target_link_libraries(jsrf_gpu_backend_parity_vk_test PRIVATE xboxrecomp)
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd)
            add_test(NAME jsrf_gpu_backend_parity_vk COMMAND jsrf_gpu_backend_parity_vk_test)
            set_tests_properties(jsrf_gpu_backend_parity_vk PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd};VK_ICD_FILENAMES=${_vk_icd};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            # The submission-ring lifetime gate, its own case because it asks a different question
            # from the parity case above: the ring is only sound if submissions really overlap, and
            # the guest-visible result is still right while targets, textures and depth buffers are
            # evicted and released. It drives the real seam (installs the back end itself), runs
            # 12 + slots frames of thousands of draws a frame -- sized from the ring's own slot
            # count, read at run time -- asserts the overlap claim from nv2a_vk_backend_ring_stats()
            # and the reach of the workload from the same counters, and then replays the identical
            # frames against the CPU executor so every frame's pixels are checked against that
            # reference as well. A generous timeout: it is much longer than the parity case. It
            # skips (77) exactly like its sibling.
            add_test(NAME jsrf_vk_frames_in_flight
                     COMMAND jsrf_gpu_backend_parity_vk_test --frames-in-flight)
            set_tests_properties(jsrf_vk_frames_in_flight PROPERTIES TIMEOUT 900 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd};VK_ICD_FILENAMES=${_vk_icd};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            # N4.2's own gate, written by a different worker from the change it decides: the
            # flip's readback publishes every dirty target through one bounded staging buffer
            # and, for a 16-bit target, truncates it to 5:6:5 in a GPU compute pack kernel
            # instead of a per-pixel loop on the host. It drives the real seam, checks every
            # byte of seven targets (padded pitches, odd dirty rectangles, both formats in one
            # flip, the mid-frame flush of an aliased texture's target, and a second frame's
            # re-load) against its own plan and against the CPU executor, and decides the
            # negative from the new nv2a_vk_backend_readback_stats() counters -- CPU-converted
            # bytes and a GPU-truncated pack differ only in the low bits, so the truncation
            # boundary colours it draws are what make the claim decidable. It skips (77)
            # exactly like its siblings.
            add_test(NAME jsrf_vk_readback_pack
                     COMMAND jsrf_gpu_backend_parity_vk_test --readback-pack)
            set_tests_properties(jsrf_vk_readback_pack PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd};VK_ICD_FILENAMES=${_vk_icd};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            # N4.4's own real-device case, written by a different worker from the change it
            # decides: the memory sub-allocator's pooling has to hold on a real driver, not only
            # against stubs. It drives the same readback fixture's frames -- several targets of
            # both formats, a texture the sampler reads, a depth buffer and the self-sampling
            # snapshot -- compares every guest byte against the gate's own plan and against the
            # CPU executor, forces the texture path to re-create its image repeatedly so pool
            # churn and reuse are genuinely reached, and decides the allocation-call reduction
            # from nv2a_vk_backend_memory_stats() observed across that churn (ranges taken while
            # alloc_calls grows by strictly fewer than the re-creations, plus the write-once
            # first_shared_block latch). It skips (77) exactly like its siblings.
            if(JSRF_VK_TEST_MOLTENVK)
                # Two independent test-oracle defects, not a proven allocator leak: the pool claim
                # free+used==reserved omits the backend's separately reported padding (1792 bytes
                # is consistent with padding, inferred, not measured on Mac here), and the budget
                # adds dedicated allocations to alloc_calls while a later assertion already
                # includes them (double count; measured 24 > 5). Keep the skip until the oracle is
                # repaired and a measured MoltenVK run decides it; the backend is not changed.
                add_test(NAME jsrf_vk_suballoc_device
                         COMMAND jsrf_gpu_backend_parity_vk_test --skip
                         "MoltenVK test-oracle debt: pool accounting omits padding and the budget double-counts dedicated allocations (measured: 24 allocations where <= 5 is required); not a proven backend defect")
                set_tests_properties(jsrf_vk_suballoc_device PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
            else()
            add_test(NAME jsrf_vk_suballoc_device
                     COMMAND jsrf_gpu_backend_parity_vk_test --suballoc)
            set_tests_properties(jsrf_vk_suballoc_device PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd};VK_ICD_FILENAMES=${_vk_icd};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            endif()
        else()
            add_test(NAME jsrf_gpu_backend_parity_vk
                     COMMAND jsrf_gpu_backend_parity_vk_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_gpu_backend_parity_vk PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
            add_test(NAME jsrf_vk_frames_in_flight
                     COMMAND jsrf_gpu_backend_parity_vk_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vk_frames_in_flight PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
            add_test(NAME jsrf_vk_readback_pack
                     COMMAND jsrf_gpu_backend_parity_vk_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vk_readback_pack PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
            add_test(NAME jsrf_vk_suballoc_device
                     COMMAND jsrf_gpu_backend_parity_vk_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vk_suballoc_device PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
    else()
        message(STATUS "Vulkan/unavailable: jsrf_gpu_backend_parity_vk, jsrf_vk_frames_in_flight, "
                       "jsrf_vk_readback_pack and jsrf_vk_suballoc_device are not registered -- "
                       "they run against the Vulkan back end, which is a stub in this build")
    endif()
    # V5's gating test: the ahead-of-time vertex-program corpus against the live Vulkan back end.
    # Authored by a different worker from the two production authors (plan rule N5). It links the
    # corpus, so it needs `just shaders` like the game itself -- and, like every target below that
    # links the corpus or the Vulkan back end, it is registered only with the capability on.
    # Without Vulkan/dxc there is no such target to run: the build's corpus is the generated empty
    # table, the back end is a stub, and an unregistered test is honest where a green no-op is not.
    if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)
        add_executable(jsrf_vsh_aot_test tests/test_vsh_aot_programs.c ${JSRF_CORPUS_SOURCES})
        target_include_directories(jsrf_vsh_aot_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JSRF_GEN_INCLUDE_DIR}
            ${JSRF_CORPUS_INCLUDE_DIR})
        target_link_libraries(jsrf_vsh_aot_test PRIVATE xboxrecomp)
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_aot)
            add_test(NAME jsrf_vsh_aot COMMAND jsrf_vsh_aot_test)
            set_tests_properties(jsrf_vsh_aot PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd_aot};VK_ICD_FILENAMES=${_vk_icd_aot};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            # The gating test's own negative controls, as separate cases: they prove the
            # checks are load-bearing. Each reports the CHECK's verdict as its exit status --
            # 1 when the check correctly failed the injected defect (the expected, load-bearing
            # result) and 2 when it missed it. WILL_FAIL cannot express that: it would also
            # accept exit 2 (a control that missed the defect) and any crash, so the case is
            # asserted on the exact success line instead, which only the exit-1 path prints.
            add_test(NAME jsrf_vsh_aot_control1
                     COMMAND jsrf_vsh_aot_test --negative-control 1)
            add_test(NAME jsrf_vsh_aot_control2
                     COMMAND jsrf_vsh_aot_test --negative-control 2)
            set_tests_properties(jsrf_vsh_aot_control1 jsrf_vsh_aot_control2 PROPERTIES
                TIMEOUT 300
                PASS_REGULAR_EXPRESSION "correctly FAILED the simulated defect"
                FAIL_REGULAR_EXPRESSION "MISSED the simulated defect"
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd_aot};VK_ICD_FILENAMES=${_vk_icd_aot};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
        else()
            add_test(NAME jsrf_vsh_aot COMMAND jsrf_vsh_aot_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vsh_aot PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
        # The malformed-entry regression: a corpus entry whose `words` is NULL or whose `len` is wrong
        # must be REJECTED and logged, not walked. Registered with the capability on and then
        # UNCONDITIONALLY within it -- no --skip path and no SKIP_RETURN_CODE: it never returns 77,
        # and a device it cannot install on is a hard failure, because "the check did not run" must
        # not be able to look like a pass. The software-ICD environment is added when one is
        # configured, so the case runs on lavapipe here.
        add_executable(jsrf_vsh_aot_malformed_test
            tests/test_vsh_aot_malformed.c ${JSRF_CORPUS_SOURCES})
        target_include_directories(jsrf_vsh_aot_malformed_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JSRF_GEN_INCLUDE_DIR}
            ${JSRF_CORPUS_INCLUDE_DIR})
        target_link_libraries(jsrf_vsh_aot_malformed_test PRIVATE xboxrecomp)
        add_test(NAME jsrf_vsh_aot_malformed COMMAND jsrf_vsh_aot_malformed_test)
        set_tests_properties(jsrf_vsh_aot_malformed PROPERTIES TIMEOUT 300)
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_mal)
            set_tests_properties(jsrf_vsh_aot_malformed PROPERTIES
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd_mal};VK_ICD_FILENAMES=${_vk_icd_mal};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
        endif()
    elseif(JSRF_FIXTURES_ONLY)
        message(STATUS "JSRF_FIXTURES_ONLY: jsrf_vsh_aot, jsrf_vsh_aot_control1/2 and "
                       "jsrf_vsh_aot_malformed are not registered -- they assert on the retail "
                       "vertex-program corpus (126 entries), and this build compiles the empty one")
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vsh_aot, jsrf_vsh_aot_control1/2 and "
                       "jsrf_vsh_aot_malformed are not registered -- they link and exercise the "
                       "retail vertex-program corpus, which this build does not compile")
    endif()
    # ── N4.1's repair, as a deciding failure-injection regression ────────────────────────────
    # The bounded three-slot submission ring was rejected for one defect: vb_reserve can advance
    # the cursor into a slot that is still pending (the GPU's), and the fence wait that advance
    # performs can fail -- and the upload paths then wrote guest pixels into that pending slot's
    # mapped stream, destroying bytes an in-flight submission may still be reading. This case
    # drives that exact scenario (the wait returns VK_TIMEOUT from a controlled stub) and asserts
    # the pending slot's whole 16 MiB stream is byte-identical afterwards, with no fence reset, no
    # descriptor pool reset, no submit and no copy; that a failed vkBeginCommandBuffer leaves the
    # slot unrecordable; and that every later recording call goes to the discard buffer instead.
    #
    # It compiles NO COPY of the algorithm: its translation unit #includes the toolkit's
    # src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so the bodies under test are the
    # production ones, and the toolkit is NOT linked (the five external symbols it references are
    # stubbed in the test). scripts/vk-ring-fault-gate.py reads the same file at BUILD time, fails
    # the BUILD (not a skip) when a definition, the slot struct or a ring constant the fixture uses
    # is gone, and derives the seam markers the test compiles against -- the pre-fix source must
    # still build, because the case has to run against it and report the defect per property.
    #
    # No ICD and no environment: every Vulkan entry point the paths reach is a stub. There is no
    # --skip and no SKIP_RETURN_CODE -- the seam is inside the translation unit, so the case always
    # runs, and a seam it cannot find is a FAILED CHECK, never Not Run.
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the ring fault-gate test compiles "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_RING_GATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-ring-fault-gate")
        set(VK_RING_GATE_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_RING_GATE_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-ring-fault-gate.py")
        set(VK_RING_GATE_HEADER "${VK_RING_GATE_DIR}/vk_ring_fault_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_RING_GATE_HEADER}" "${VK_RING_GATE_DIR}/vk_ring_fault_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_RING_GATE_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_RING_GATE_PROBE}"
                    --source "${VK_RING_GATE_SOURCE}"
                    --header "${VK_RING_GATE_HEADER}"
                    --manifest "${VK_RING_GATE_DIR}/vk_ring_fault_gate_seam.txt"
            DEPENDS "${VK_RING_GATE_SOURCE}" "${VK_RING_GATE_PROBE}"
            COMMENT "vk_ring_fault_gate: probing nv2a_vk_backend.c for the seam the regression binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_ring_fault_gate_probe DEPENDS "${VK_RING_GATE_HEADER}")
        add_executable(jsrf_vk_ring_fault_gate_test tests/test_vk_ring_fault_gate.c)
        # xbox_video is what generates the SPIR-V headers the back end includes; the probe target
        # writes the seam header the test includes.
        add_dependencies(jsrf_vk_ring_fault_gate_test jsrf_vk_ring_fault_gate_probe xbox_video)
        target_include_directories(jsrf_vk_ring_fault_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_RING_GATE_DIR}")
        target_link_libraries(jsrf_vk_ring_fault_gate_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_ring_fault_gate COMMAND jsrf_vk_ring_fault_gate_test)
        set_tests_properties(jsrf_vk_ring_fault_gate PROPERTIES TIMEOUT 300)
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_ring_fault_gate is not registered -- it "
                       "compiles the toolkit's Vulkan back end, which this build does not "
                       "(${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── N4.4's memory sub-allocation, as its deciding gate ──────────────────────────────────
    # The back end allocated one VkDeviceMemory per resource, bound at offset 0. N4.4 pools
    # them: several images or buffers with compatible requirements live in ONE block at
    # distinct, alignment-satisfying, non-overlapping ranges; a released range is handed out
    # again when nothing that could still read it is recording or in flight; a request that
    # cannot be pooled (oversized, requires/prefers dedicated, no coherent host type, pool
    # budgets exhausted) becomes a dedicated allocation with its VkMemoryDedicatedAllocateInfo
    # in pNext. This case decides all of it from the allocator's own state and from
    # nv2a_vk_backend_memory_stats(): the sharing and the reduction in vkAllocateMemory calls
    # (cross-checked against the fixture's own stub count), memory-type and alignment
    # correctness at every bind, the image/buffer pool separation, reuse and coalescing with
    # honest byte counters, the dedicated reasons and their pNext, delayed reuse while a
    # recording or a pending submission is live (the fixture writes a pattern into the parked
    # range's mapped bytes and requires them byte-identical), rollback on injected
    # vkAllocateMemory / vkCreateImage / vkCreateBuffer / vkBind*Memory / vkMapMemory failures,
    # and the non-coherent flush/invalidate widening to whole nonCoherentAtomSize atoms.
    #
    # It compiles NO COPY of the algorithm: its translation unit #includes the toolkit's
    # src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so the bodies under test are the
    # production ones, and the toolkit is NOT linked (the five external symbols it references
    # are stubbed in the test, and every Vulkan entry point -- including the optional
    # vkGet*MemoryRequirements2 / flush / invalidate / unmap lookups through
    # vkGetDeviceProcAddr -- is a static function pointer the test points at its own stubs).
    # scripts/vk-suballoc-gate.py re-reads the same bytes at BUILD time: a type, field,
    # constant, function signature or counter the fixture binds to that is gone fails the BUILD
    # (exit 2), and the markers it derives are reported by the property that needs them.
    #
    # No ICD and no environment: every Vulkan entry point is a stub and every allocation is a
    # handle the fixture made up. There is no --skip and no SKIP_RETURN_CODE -- the seam is
    # inside the translation unit, so the case always runs, and an injected
    # VK_ERROR_OUT_OF_DEVICE_MEMORY is a stub's return value, never real device loss.
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the sub-allocation gate compiles "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_SUBALLOC_GATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-suballoc-gate")
        set(VK_SUBALLOC_GATE_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_SUBALLOC_GATE_DECLS "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.h")
        set(VK_SUBALLOC_GATE_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-suballoc-gate.py")
        set(VK_SUBALLOC_GATE_HEADER "${VK_SUBALLOC_GATE_DIR}/vk_suballoc_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_SUBALLOC_GATE_HEADER}" "${VK_SUBALLOC_GATE_DIR}/vk_suballoc_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_SUBALLOC_GATE_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_SUBALLOC_GATE_PROBE}"
                    --source "${VK_SUBALLOC_GATE_SOURCE}"
                    --decls "${VK_SUBALLOC_GATE_DECLS}"
                    --header "${VK_SUBALLOC_GATE_HEADER}"
                    --manifest "${VK_SUBALLOC_GATE_DIR}/vk_suballoc_gate_seam.txt"
            DEPENDS "${VK_SUBALLOC_GATE_SOURCE}" "${VK_SUBALLOC_GATE_DECLS}"
                    "${VK_SUBALLOC_GATE_PROBE}"
            COMMENT "vk_suballoc_gate: probing nv2a_vk_backend.c for the seam the gate binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_suballoc_gate_probe DEPENDS "${VK_SUBALLOC_GATE_HEADER}")
        add_executable(jsrf_vk_suballoc_gate_test tests/test_vk_suballoc_gate.c
            tests/vk_suballoc_oracles.h)
        # xbox_video is what generates the SPIR-V headers the back end includes; the probe target
        # writes the seam header the test includes.
        add_dependencies(jsrf_vk_suballoc_gate_test jsrf_vk_suballoc_gate_probe xbox_video)
        target_include_directories(jsrf_vk_suballoc_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            "${VK_SUBALLOC_GATE_DIR}")
        target_link_libraries(jsrf_vk_suballoc_gate_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_suballoc_gate COMMAND jsrf_vk_suballoc_gate_test)
        set_tests_properties(jsrf_vk_suballoc_gate PROPERTIES TIMEOUT 300)

        # The persisted VkPipelineCache and the pipeline table's eviction counter, decided from the
        # specification against stubbed Vulkan. Like the gate above it compiles NO COPY of the
        # algorithm: its translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with
        # XBR_VK_BACKEND defined and drives the production callers (vb_create_pipeline_objects,
        # vb_pipeline, vb_flip), so a symbol, field or counter that moves breaks the BUILD. It
        # decides every rejection reason of the cache file and the load line, the valid round trip,
        # the off / 0 knob and the device-specific default name, the save gating (dirty, interval,
        # one write, the temporary file opened before the fetch and atomically renamed over the
        # target), the pre-write parse, the second call's size handling, the three-failure latch,
        # the log cap, and the 257th pipeline key evicting exactly one. No ICD and no environment:
        # there is no skip path.
        add_executable(jsrf_vk_pcache_gate_test tests/test_vk_pcache_gate.c)
        add_dependencies(jsrf_vk_pcache_gate_test xbox_video)
        target_include_directories(jsrf_vk_pcache_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_vk_pcache_gate_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_pcache_gate COMMAND jsrf_vk_pcache_gate_test)
        set_tests_properties(jsrf_vk_pcache_gate PROPERTIES TIMEOUT 300)

        # The same feature on a real driver. jsrf_vk_pcache_device installs the production back end
        # on the software ICD: a driver whose cache blob is well-formed must round-trip it (the
        # pipeline-cache hit bit of VkPipelineCreationFeedback after a reload from the file), one
        # whose blob is malformed -- lavapipe's is -- must have the save refused (no file, one
        # failure, saving stopped), and the case prints which of the two it decided.
        # jsrf_vk_pcache_device_hw is the hardware case: the loader's default devices with
        # RECOMP_VK_DEVICE naming a GPU (XBR_VK_HW_DEVICE, default NVIDIA), it must decide the full
        # round trip with creation-feedback hits, and it skips (77) when the selected device is a CPU
        # device or no device can be installed, like vk_smoke_hw.
        add_executable(jsrf_vk_pcache_device_test tests/test_vk_pcache_device.c)
        add_dependencies(jsrf_vk_pcache_device_test xbox_video)
        target_include_directories(jsrf_vk_pcache_device_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_vk_pcache_device_test PRIVATE ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_pcache)
            add_test(NAME jsrf_vk_pcache_device COMMAND jsrf_vk_pcache_device_test)
            set_tests_properties(jsrf_vk_pcache_device PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd_pcache};VK_ICD_FILENAMES=${_vk_icd_pcache};${JSRF_VK_TEST_SELECT}")
        else()
            add_test(NAME jsrf_vk_pcache_device
                     COMMAND jsrf_vk_pcache_device_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vk_pcache_device PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
        if(NOT XBR_VK_HW_DEVICE)
            set(XBR_VK_HW_DEVICE "NVIDIA")
        endif()
        if(JSRF_VK_TEST_MOLTENVK)
            # The hardware case requires the driver to report pipeline-cache hits in
            # VkPipelineCreationFeedback; MoltenVK reports none even on a warm cache (measured:
            # creates=6 valid=6 hits=0), so there is no round trip for it to decide.
            add_test(NAME jsrf_vk_pcache_device_hw
                     COMMAND jsrf_vk_pcache_device_test --skip
                     "MoltenVK reports no pipeline-cache hits in VkPipelineCreationFeedback, which the hardware case requires")
            set_tests_properties(jsrf_vk_pcache_device_hw PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        else()
        add_test(NAME jsrf_vk_pcache_device_hw COMMAND jsrf_vk_pcache_device_test --hw)
        set_tests_properties(jsrf_vk_pcache_device_hw PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77
            ENVIRONMENT "RECOMP_VK_DEVICE=${XBR_VK_HW_DEVICE}")
        # A caller's own ICD override must not leak into the hardware variant.
        if(NOT CMAKE_VERSION VERSION_LESS 3.22)
            set_tests_properties(jsrf_vk_pcache_device_hw PROPERTIES
                ENVIRONMENT_MODIFICATION "VK_DRIVER_FILES=unset:;VK_ICD_FILENAMES=unset:")
        endif()
        endif()

        # ── BUMPENVMAP q-independence, the focused D4 step-4 gate ────────────────────────────
        # Authored by a worker distinct from the production change. Modes 6/7 must sample
        # pT.xy + dsdt with NO PROJECT2D texture-q division, while mode 1 must still divide.
        # The CPU half #includes the production src/kernel/nv2a_pb_exec.c and asserts the real
        # rc_stage_fetch (a DIRECT HELPER SEAM: no rasteriser, no committed stream). The
        # Vulkan half #includes the production src/video/nv2a_vk_backend.c, drives vb_draw +
        # vb_flip on the software ICD, and reads the perturbed texel back out of guest memory.
        # It also carries the production binding witness: modes 4/5 must not upload or bind
        # their stage texture, modes 1/2/3/6/7 must, witnessed from g.st.tex_uploads /
        # g.st.tex_hits and the texture cache, per stage.
        # The #included production files are NOT listed as sources (listing them would compile
        # them twice), so MSBuild has no reason to recompile the test object when they change:
        # its compiler include-tracking is unreliable and is disabled under %TEMP% (MSB8029).
        # OBJECT_DEPENDS states the dependency the #include implies, so an edit to the file under
        # test always rebuilds the object that includes it. The HLSL/SPIR-V the VK half consumes
        # is already covered by add_dependencies(... xbox_video) below.
        set_source_files_properties(tests/test_bumpenv_q_cpu.c PROPERTIES
            OBJECT_DEPENDS "${XBOXRECOMP_DIR}/src/kernel/nv2a_pb_exec.c")
        set_source_files_properties(tests/test_bumpenv_q_vk.c PROPERTIES
            OBJECT_DEPENDS "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        # The v0.16.0 executor also calls the flip hold, the host clock, and three runtime
        # counters; the first two link for real, the counters come from a seam file.
        add_executable(jsrf_bumpenv_q_cpu_test tests/test_bumpenv_q_cpu.c
            tests/bumpenv_q_cpu_seams.c
            ${XBOXRECOMP_DIR}/src/kernel/nv2a_vsh_interp.c
            ${XBOXRECOMP_DIR}/src/kernel/nv2a_combiner.c
            ${XBOXRECOMP_DIR}/src/kernel/nv2a_flip_hold.c
            ${XBOXRECOMP_DIR}/src/platform/host_time.c)
        target_include_directories(jsrf_bumpenv_q_cpu_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
            ${XBOXRECOMP_DIR}/src/nv2a ${JSRF_GEN_INCLUDE_DIR})
        target_link_libraries(jsrf_bumpenv_q_cpu_test PRIVATE ${CMAKE_DL_LIBS})
        if(NOT WIN32)
            # QueryPerformanceCounter/Frequency and InterlockedCompareExchange64 live in the
            # toolkit's POSIX primitive layer, which is the same `platform` library the other
            # portable fixtures link; Windows gets them from the CRT and links nothing extra.
            target_link_libraries(jsrf_bumpenv_q_cpu_test PRIVATE platform)
        endif()
        foreach(_case bump_zero_matrix bump_displacement bump_positive_delta bump_lum
                project2d_control)
            add_test(NAME jsrf_bumpenv_q_cpu_${_case}
                     COMMAND jsrf_bumpenv_q_cpu_test --case ${_case})
            set_tests_properties(jsrf_bumpenv_q_cpu_${_case} PROPERTIES TIMEOUT 120)
        endforeach()
        add_executable(jsrf_bumpenv_q_vk_test tests/test_bumpenv_q_vk.c)
        add_dependencies(jsrf_bumpenv_q_vk_test xbox_video)
        target_include_directories(jsrf_bumpenv_q_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_bumpenv_q_vk_test PRIVATE ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_bump)
            set(_bump_vk_env
                "VK_DRIVER_FILES=${_vk_icd_bump};VK_ICD_FILENAMES=${_vk_icd_bump};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case vk_bump_q vk_project2d_control vk_bind_witness)
                add_test(NAME jsrf_bumpenv_q_vk_${_case}
                         COMMAND jsrf_bumpenv_q_vk_test --case ${_case})
                set_tests_properties(jsrf_bumpenv_q_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_bump_vk_env}")
            endforeach()
        else()
            foreach(_case vk_bump_q vk_project2d_control vk_bind_witness)
                add_test(NAME jsrf_bumpenv_q_vk_${_case}
                         COMMAND jsrf_bumpenv_q_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_bumpenv_q_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        # ── BUMPENVMAP / BUMPENVMAP_LUM, the D4 step-5 gate's Vulkan half ────────────────────
        # Its input is the batch the production executor built from the committed stream: the
        # fixture submits real pushbuffer words, a capture vtable at the executor's back-end seam
        # records each batch and hands that same pointer to the production vb_draw (#included
        # with XBR_VK_BACKEND), and the pixel read back after vb_flip must equal the literal the
        # CPU half also checks. The production texture decoder is linked, not stubbed. Refusals
        # are read from g.st.rc_fallback and the production refusal ledger, whose reason needs
        # RECOMP_PB_COVERAGE=1. The #included production files get OBJECT_DEPENDS for the same
        # reason as the q gate above.
        set_source_files_properties(tests/test_bumpenv_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_bumpenv_stream_vk_test tests/test_bumpenv_stream_vk.c
            tests/bumpenv_stream_fixture.c)
        add_dependencies(jsrf_bumpenv_stream_vk_test xbox_video)
        target_include_directories(jsrf_bumpenv_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_bumpenv_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_bstream)
            set(_bstream_vk_env
                "VK_DRIVER_FILES=${_vk_icd_bstream};VK_ICD_FILENAMES=${_vk_icd_bstream};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off;RECOMP_PB_COVERAGE=1")
            foreach(_case IN LISTS BUMPENV_STREAM_CASES)
                add_test(NAME jsrf_bumpenv_stream_vk_${_case}
                         COMMAND jsrf_bumpenv_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_bumpenv_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_bstream_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS BUMPENV_STREAM_CASES)
                add_test(NAME jsrf_bumpenv_stream_vk_${_case}
                         COMMAND jsrf_bumpenv_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_bumpenv_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_bumpenv_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── Fixed-function texgen and texture matrices, the D4 step-6 gate's Vulkan half ──────
        # Its input is the batch the production executor built from the committed stream (texgen
        # and the texture matrix applied by the executor); a capture vtable records each batch's
        # per-vertex texture coordinates and hands the same pointer to the production vb_draw
        # (#included with XBR_VK_BACKEND), and the pixel read back after vb_flip must equal the
        # hand-derived literal. OBJECT_DEPENDS as for the step-5 half.
        set_source_files_properties(tests/test_texgen_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_texgen_stream_vk_test tests/test_texgen_stream_vk.c
            tests/texgen_stream_fixture.c)
        add_dependencies(jsrf_texgen_stream_vk_test xbox_video)
        target_include_directories(jsrf_texgen_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_texgen_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_tstream)
            set(_tstream_vk_env
                "VK_DRIVER_FILES=${_vk_icd_tstream};VK_ICD_FILENAMES=${_vk_icd_tstream};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS TEXGEN_STREAM_CASES)
                add_test(NAME jsrf_texgen_stream_vk_${_case}
                         COMMAND jsrf_texgen_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_texgen_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_tstream_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS TEXGEN_STREAM_CASES)
                add_test(NAME jsrf_texgen_stream_vk_${_case}
                         COMMAND jsrf_texgen_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_texgen_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_texgen_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── NV2A window clip, the committed-stream gate's Vulkan half ────────────────────────────
        # As the texgen half: the batch the production executor built from the committed stream is
        # captured at the back-end seam and handed unchanged to the production vb_draw
        # (#included with XBR_VK_BACKEND); the pixel read back after vb_flip must equal the
        # hand-derived literal, with the one-scissor limitations labelled in the cases header.
        set_source_files_properties(tests/test_wclip_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_wclip_stream_vk_test tests/test_wclip_stream_vk.c
            tests/wclip_stream_fixture.c)
        add_dependencies(jsrf_wclip_stream_vk_test xbox_video)
        target_include_directories(jsrf_wclip_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_wclip_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_wstream)
            set(_wstream_vk_env
                "VK_DRIVER_FILES=${_vk_icd_wstream};VK_ICD_FILENAMES=${_vk_icd_wstream};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS WCLIP_STREAM_CASES)
                add_test(NAME jsrf_wclip_stream_vk_${_case}
                         COMMAND jsrf_wclip_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_wclip_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_wstream_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS WCLIP_STREAM_CASES)
                add_test(NAME jsrf_wclip_stream_vk_${_case}
                         COMMAND jsrf_wclip_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_wclip_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_wclip_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── NV2A X8R8G8B8 texture alpha, the committed-stream gate's Vulkan half ─────────────────
        # As the wclip half: the clear and the batch the production executor built from the
        # committed stream are captured at the back-end seam and handed unchanged to the production
        # vb_clear / vb_draw (#included with XBR_VK_BACKEND); the pixel read back after vb_flip
        # must equal the hand-derived literal, and the render-target alias counters prove the draw
        # sampled the live target.
        set_source_files_properties(tests/test_x8alias_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_x8alias_stream_vk_test tests/test_x8alias_stream_vk.c
            tests/x8alias_stream_fixture.c)
        add_dependencies(jsrf_x8alias_stream_vk_test xbox_video)
        target_include_directories(jsrf_x8alias_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_x8alias_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_x8alias)
            set(_x8alias_vk_env
                "VK_DRIVER_FILES=${_vk_icd_x8alias};VK_ICD_FILENAMES=${_vk_icd_x8alias};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS X8ALIAS_STREAM_CASES)
                add_test(NAME jsrf_x8alias_stream_vk_${_case}
                         COMMAND jsrf_x8alias_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_x8alias_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_x8alias_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS X8ALIAS_STREAM_CASES)
                add_test(NAME jsrf_x8alias_stream_vk_${_case}
                         COMMAND jsrf_x8alias_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_x8alias_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_x8alias_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── NV2A depth per zeta address, the committed-stream gate's Vulkan half ────────────────
        # As the x8alias half: the clears and batches the production executor built from the
        # committed stream are captured at the back-end seam and handed unchanged to the production
        # vb_clear / vb_draw (#included with XBR_VK_BACKEND); the pixel read back after vb_flip
        # must equal the hand-derived literal, and the live depth images (g.depths) are counted.
        set_source_files_properties(tests/test_zeta_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_zeta_stream_vk_test tests/test_zeta_stream_vk.c
            tests/zeta_stream_fixture.c)
        add_dependencies(jsrf_zeta_stream_vk_test xbox_video)
        target_include_directories(jsrf_zeta_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_zeta_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_zeta)
            set(_zeta_vk_env
                "VK_DRIVER_FILES=${_vk_icd_zeta};VK_ICD_FILENAMES=${_vk_icd_zeta};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS ZETA_STREAM_CASES)
                add_test(NAME jsrf_zeta_stream_vk_${_case}
                         COMMAND jsrf_zeta_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_zeta_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_zeta_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS ZETA_STREAM_CASES)
                add_test(NAME jsrf_zeta_stream_vk_${_case}
                         COMMAND jsrf_zeta_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_zeta_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_zeta_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── NV2A fixed-function vertex lighting, the committed-stream gate's Vulkan half ─────────
        # As the zeta half: the batch the production executor built from the committed stream
        # (lit vertex colours included) is captured at the back-end seam and handed unchanged to
        # the production vb_draw (#included with XBR_VK_BACKEND); the pixel read back after
        # vb_flip must equal the same hand-derived literal as the CPU half.
        set_source_files_properties(tests/test_light_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_light_stream_vk_test tests/test_light_stream_vk.c
            tests/light_stream_fixture.c)
        add_dependencies(jsrf_light_stream_vk_test xbox_video)
        target_include_directories(jsrf_light_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_light_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_light)
            set(_light_vk_env
                "VK_DRIVER_FILES=${_vk_icd_light};VK_ICD_FILENAMES=${_vk_icd_light};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS LIGHT_STREAM_CASES)
                add_test(NAME jsrf_light_stream_vk_${_case}
                         COMMAND jsrf_light_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_light_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_light_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS LIGHT_STREAM_CASES)
                add_test(NAME jsrf_light_stream_vk_${_case}
                         COMMAND jsrf_light_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_light_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_light_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── NV2A fixed-function SPECULAR, the committed-stream gate's Vulkan half ───────────────
        # As the lighting half: the batch the production executor built from the committed stream
        # is captured at the back-end seam and handed unchanged to the production vb_draw
        # (#included with XBR_VK_BACKEND), which runs the production combiner. The pixel read back
        # after vb_flip must equal the hardware literal from tests/spec_stream_cases.h. This half
        # is the DECIDING one: the CPU fixed-function rasteriser has no combiners.
        set_source_files_properties(tests/test_spec_stream_vk.c PROPERTIES
            OBJECT_DEPENDS
            "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c;${XBOXRECOMP_DIR}/src/video/nv2a_vk_rc_coverage.h")
        if(WIN32)
        add_executable(jsrf_spec_stream_vk_test tests/test_spec_stream_vk.c
            tests/spec_stream_fixture.c)
        add_dependencies(jsrf_spec_stream_vk_test xbox_video)
        target_include_directories(jsrf_spec_stream_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${JSRF_GEN_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_spec_stream_vk_test PRIVATE xboxrecomp ${CMAKE_DL_LIBS})
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_spec)
            set(_spec_vk_env
                "VK_DRIVER_FILES=${_vk_icd_spec};VK_ICD_FILENAMES=${_vk_icd_spec};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS SPEC_STREAM_CASES)
                add_test(NAME jsrf_spec_stream_vk_${_case}
                         COMMAND jsrf_spec_stream_vk_test --case ${_case})
                set_tests_properties(jsrf_spec_stream_vk_${_case} PROPERTIES TIMEOUT 300
                    SKIP_RETURN_CODE 77 ENVIRONMENT "${_spec_vk_env}")
            endforeach()
        else()
            foreach(_case IN LISTS SPEC_STREAM_CASES)
                add_test(NAME jsrf_spec_stream_vk_${_case}
                         COMMAND jsrf_spec_stream_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_spec_stream_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
        else()
            message(STATUS "jsrf_spec_stream_vk_* is not registered off Windows -- its "
                           "committed-stream fixture drives the Win32 guest-memory layout")
        endif()
        # ── D4 Vulkan rc-refusal coverage, the real-device deciding gate ─────────────────────
        # Authored by a worker distinct from the production change. It #includes the production
        # src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, installs on the software ICD,
        # and drives the production vb_draw/fallback site: `g.st.rc_fallback` must equal the
        # production ledger's total, the two reason counters must sum to it exclusively, and the
        # first refusal must be frozen. One process per case because both gates (the executor's
        # and the header's) are sampled once per process; the RECOMP_PB_COVERAGE variants are
        # therefore separate CTest entries. A failed install when the environment names an ICD is
        # a FAILURE in the case itself, never a skip.
        add_executable(jsrf_pbcov_vk_test tests/test_pbcov_vk.c)
        add_dependencies(jsrf_pbcov_vk_test xbox_video)
        target_include_directories(jsrf_pbcov_vk_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv")
        target_link_libraries(jsrf_pbcov_vk_test PRIVATE ${CMAKE_DL_LIBS})
        set(PBCOV_VK_CASES
            rc_unsupported_modes rc_cube_mode3 rc_supported_modes rc_priority
            rc_first_report rc_vtable)
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_pbcov)
            set(_pbcov_vk_env
                "VK_DRIVER_FILES=${_vk_icd_pbcov};VK_ICD_FILENAMES=${_vk_icd_pbcov};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
            foreach(_case IN LISTS PBCOV_VK_CASES)
                add_test(NAME jsrf_pbcov_vk_${_case}
                         COMMAND jsrf_pbcov_vk_test --case ${_case})
                set_tests_properties(jsrf_pbcov_vk_${_case} PROPERTIES TIMEOUT 300
                    ENVIRONMENT "${_pbcov_vk_env};RECOMP_PB_COVERAGE=1")
            endforeach()
            # Gate-off variants: the renderer still refuses, the diagnostics stay empty.
            # unset / empty / 0 / 01 / true are all inert; the case also checks that CTest's
            # value is not the exact-1 gate, so a mis-wired entry cannot pass as "off".
            if(NOT CMAKE_VERSION VERSION_LESS 3.22)
                add_test(NAME jsrf_pbcov_vk_rc_gate_unset
                         COMMAND jsrf_pbcov_vk_test --case rc_gate_off)
                set_tests_properties(jsrf_pbcov_vk_rc_gate_unset PROPERTIES TIMEOUT 300
                    ENVIRONMENT "${_pbcov_vk_env}"
                    ENVIRONMENT_MODIFICATION "RECOMP_PB_COVERAGE=unset:")
            endif()
            add_test(NAME jsrf_pbcov_vk_rc_gate_empty
                     COMMAND jsrf_pbcov_vk_test --case rc_gate_off)
            set_tests_properties(jsrf_pbcov_vk_rc_gate_empty PROPERTIES TIMEOUT 300
                ENVIRONMENT "${_pbcov_vk_env};RECOMP_PB_COVERAGE=")
            foreach(_variant 0 01 true)
                add_test(NAME jsrf_pbcov_vk_rc_gate_${_variant}
                         COMMAND jsrf_pbcov_vk_test --case rc_gate_off)
                set_tests_properties(jsrf_pbcov_vk_rc_gate_${_variant} PROPERTIES TIMEOUT 300
                    ENVIRONMENT "${_pbcov_vk_env};RECOMP_PB_COVERAGE=${_variant}")
            endforeach()
            # First-sample cases set the variable themselves; the CTest environment deliberately
            # starts on the opposite side, so the process, not the parent, decides the first sample.
            add_test(NAME jsrf_pbcov_vk_rc_first_sample_on_wins
                     COMMAND jsrf_pbcov_vk_test --case rc_first_sample_on_wins)
            set_tests_properties(jsrf_pbcov_vk_rc_first_sample_on_wins PROPERTIES TIMEOUT 300
                ENVIRONMENT "${_pbcov_vk_env};RECOMP_PB_COVERAGE=1")
            add_test(NAME jsrf_pbcov_vk_rc_first_sample_off_wins
                     COMMAND jsrf_pbcov_vk_test --case rc_first_sample_off_wins)
            set_tests_properties(jsrf_pbcov_vk_rc_first_sample_off_wins PROPERTIES TIMEOUT 300
                ENVIRONMENT "${_pbcov_vk_env};RECOMP_PB_COVERAGE=0")
        else()
            foreach(_case IN LISTS PBCOV_VK_CASES
                    rc_gate_unset rc_gate_empty rc_gate_0 rc_gate_01 rc_gate_true
                    rc_first_sample_on_wins rc_first_sample_off_wins)
                add_test(NAME jsrf_pbcov_vk_${_case}
                         COMMAND jsrf_pbcov_vk_test --skip "XBR_VK_SW_ICD is not set")
                set_tests_properties(jsrf_pbcov_vk_${_case} PROPERTIES TIMEOUT 20
                    SKIP_RETURN_CODE 77)
            endforeach()
        endif()
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_suballoc_gate is not registered -- it "
                       "compiles the toolkit's Vulkan back end, which this build does not "
                       "(${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── N4.2's readback publication, as its failure-path gate ────────────────────────────────
    # The success-path gate (jsrf_vk_readback_pack) runs the pack kernel on a real device and
    # compares pixels; it cannot reach the branch this one exists for. In vb_targets_flush a
    # batch's pack descriptor set can fail to allocate (vkAllocateDescriptorSets returns
    # VK_ERROR_OUT_OF_POOL_MEMORY), and the pre-fix code simply broke out of the record loop:
    # vb_fail does not clear g.ok, so the batch still ran its barrier, its vkQueueSubmit, its
    # fence wait and its publish loop, marking every staged target clean from staging bytes that
    # nothing had written -- for the 16-bit target because its dispatch was never recorded, and
    # for every target after it because its copy never ran. This case injects exactly that
    # VkResult from a stub and asserts the batch submits nothing, publishes nothing, leaves guest
    # memory byte-identical and leaves both targets dirty; that a succeeding retry then publishes
    # both rectangles exactly; and that the staging pair is neither freed nor replaced under the
    # recording the failure left open. No success-path run can reach any of it -- the project
    # rejected the N4.1 back end for precisely this class of defect (docs/jsrf-technical-record.md
    # §31.30), so the failure path needs its own deciding evidence.
    #
    # It compiles NO COPY of the algorithm: its translation unit #includes the toolkit's
    # src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so the bodies under test are the
    # production ones, and the toolkit is NOT linked (the five external symbols it references are
    # stubbed in the test, and every Vulkan entry point is a static function pointer the test
    # points at its own stubs). scripts/vk-readback-fault-gate.py re-reads the same bytes at BUILD
    # time: a definition, struct field or global the fixture binds to that is gone fails the BUILD
    # (exit 2), and the three markers it derives are what the pre-fix source is reported against
    # (the pre-fix source must still build, so it is a failed check, never a skip).
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the readback fault-gate test compiles "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_RB_GATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-readback-fault-gate")
        set(VK_RB_GATE_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_RB_GATE_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-readback-fault-gate.py")
        set(VK_RB_GATE_HEADER "${VK_RB_GATE_DIR}/vk_readback_fault_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_RB_GATE_HEADER}" "${VK_RB_GATE_DIR}/vk_readback_fault_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_RB_GATE_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_RB_GATE_PROBE}"
                    --source "${VK_RB_GATE_SOURCE}"
                    --header "${VK_RB_GATE_HEADER}"
                    --manifest "${VK_RB_GATE_DIR}/vk_readback_fault_gate_seam.txt"
            DEPENDS "${VK_RB_GATE_SOURCE}" "${VK_RB_GATE_PROBE}"
            COMMENT "vk_readback_fault_gate: probing nv2a_vk_backend.c for the seam the regression binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_readback_fault_gate_probe DEPENDS "${VK_RB_GATE_HEADER}")
        add_executable(jsrf_vk_readback_fault_gate_test tests/test_vk_readback_fault_gate.c)
        # xbox_video is what generates the SPIR-V headers the back end includes; the probe target
        # writes the seam header the test includes.
        add_dependencies(jsrf_vk_readback_fault_gate_test jsrf_vk_readback_fault_gate_probe xbox_video)
        target_include_directories(jsrf_vk_readback_fault_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_RB_GATE_DIR}")
        target_link_libraries(jsrf_vk_readback_fault_gate_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_readback_fault_gate COMMAND jsrf_vk_readback_fault_gate_test)
        set_tests_properties(jsrf_vk_readback_fault_gate PROPERTIES TIMEOUT 300)
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_readback_fault_gate is not registered -- it "
                       "compiles the toolkit's Vulkan back end, which this build does not "
                       "(${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── N4.2's readback coherence, as its CALLERS see it ─────────────────────────────────────
    # The gate above decides the flush's own failure behaviour: a batch whose pack descriptor set
    # could not be allocated submits nothing, publishes nothing and leaves its targets dirty. It
    # cannot decide what the CALLERS did with that outcome, and that was the second half of the
    # defect: vb_flip submitted, drained, marked every used target stale and advanced g.frame even
    # though the publication had failed, so the next vb_target_load uploaded old guest bytes over
    # the target whose newer pixels were never published; vb_target_for flushed and then released
    # the target unconditionally on a reshape or an LRU eviction, destroying the only copy of
    # pixels guest memory never got; and vb_texture's overlapping-guest-texture loop flushed and
    # carried on, decoding and caching a texture from guest bytes a live target had superseded.
    # This case drives those four callers and asserts, for each injection, that a failed
    # publication is retried exactly once and then stops the back end: no frame advance, no stale
    # mark, no release, no decode, no load of the bytes it never published.
    #
    # Built exactly like the fault gate: NO copy of the algorithm. Its translation unit #includes
    # the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so the bodies under
    # test are the production ones, and the toolkit is NOT linked (the five external symbols it
    # references are stubbed in the test, and every Vulkan entry point the driven paths reach is a
    # static function pointer the test points at its own stubs; both injected failures --
    # vkAllocateDescriptorSets and vkCreateBuffer -- are stub return values, not real GPU OOM).
    # scripts/vk-readback-caller-gate.py re-reads the same bytes at BUILD time: a definition,
    # struct field or global the fixture binds to that is gone fails the BUILD (exit 2). Unlike
    # the fault gate's probe it also runs against the PRE-FIX source, so the repair's own seam is
    # probed as markers the test prints rather than anchors it requires; the four callers'
    # signatures are the interface and are required.
    #
    # No SKIP_RETURN_CODE and no skip path: the seam is inside the translation unit, so the case
    # always runs and exits 0 or 1.
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the readback caller-gate test compiles "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_RB_CALLERS_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-readback-callers")
        set(VK_RB_CALLERS_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_RB_CALLERS_HDRSOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.h")
        set(VK_RB_CALLERS_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-readback-caller-gate.py")
        set(VK_RB_CALLERS_HEADER "${VK_RB_CALLERS_DIR}/vk_readback_caller_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_RB_CALLERS_HEADER}" "${VK_RB_CALLERS_DIR}/vk_readback_caller_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_RB_CALLERS_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_RB_CALLERS_PROBE}"
                    --source "${VK_RB_CALLERS_SOURCE}"
                    --backend-header "${VK_RB_CALLERS_HDRSOURCE}"
                    --header "${VK_RB_CALLERS_HEADER}"
                    --manifest "${VK_RB_CALLERS_DIR}/vk_readback_caller_gate_seam.txt"
            DEPENDS "${VK_RB_CALLERS_SOURCE}" "${VK_RB_CALLERS_HDRSOURCE}" "${VK_RB_CALLERS_PROBE}"
            COMMENT "vk_readback_callers: probing nv2a_vk_backend.c for the seam the caller regression binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_readback_callers_probe DEPENDS "${VK_RB_CALLERS_HEADER}")
        add_executable(jsrf_vk_readback_callers_test tests/test_vk_readback_callers.c)
        # xbox_video is what generates the SPIR-V headers the back end includes; the probe target
        # writes the seam header the test includes.
        add_dependencies(jsrf_vk_readback_callers_test jsrf_vk_readback_callers_probe xbox_video)
        target_include_directories(jsrf_vk_readback_callers_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_RB_CALLERS_DIR}")
        target_link_libraries(jsrf_vk_readback_callers_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_readback_callers COMMAND jsrf_vk_readback_callers_test)
        set_tests_properties(jsrf_vk_readback_callers PROPERTIES TIMEOUT 300)
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_readback_callers is not registered -- it "
                       "compiles the toolkit's Vulkan back end, which this build does not "
                       "(${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── N4.2's readback publication, as its ORDERING sees it ─────────────────────────────────
    # The fault gate above decides that a batch whose pack descriptor set could not be allocated
    # submits nothing and publishes nothing, and that its targets stay dirty. It cannot decide the
    # third thing that failure path has to get right: what the group had ALREADY recorded. Those
    # copies stay in the open recording and are submitted with the next submission the back end
    # makes -- which is the NEXT group's -- and the next group's slice offsets start at 0 again,
    # because a group's slices are laid out from its own first target. So its offset-0 write lands
    # on the same g.rb_buf bytes the abandoned recording's LAST write wrote, and that last write is
    # one of two classes: a 4-byte target's own image copy, or a 16-bit target's pack-output buffer
    # copy (g.pack_buf -> g.rb_buf). Either way it is a write-after-write pair inside one command
    # buffer, which Vulkan requires a memory dependency to order, and nothing else provides one --
    # the batch's own barrier is into VK_PIPELINE_STAGE_HOST_BIT with VK_ACCESS_HOST_READ_BIT, and
    # it is recorded after BOTH writes anyway. The pack kernel's own write into g.pack_buf is NOT
    # the vulnerable class: the pack path records its broad vb_barrier() between its dispatch and
    # the buffer copy that reads it, so pack-buffer-versus-pack-buffer reuse is ordered by
    # construction, and the case quotes that barrier as its control.
    #
    # The same abandonment leaves a second, harder-to-see dependency on the DEVICE pack buffer: the
    # abandoned recording's buffer copy READ g.pack_buf (the packed slice it moved to the staging
    # buffer), and the next group's first target -- when it is 16-bit -- records a pack dispatch
    # that WRITES those same pack buffer bytes, because a group's slice offsets start at 0 again.
    # That is a write-after-read pair on g.pack_buf, and what it needs from a barrier is an
    # EXECUTION dependency: stages only, no access mask. A log of writes alone cannot see it at all,
    # so this case records reads too and decides that arrow separately.
    #
    # This case decides exactly that, from the recorded command stream: it drives three three- or
    # four-target scenarios -- one whose abandoned recording ends in a 4-byte image copy, one in a
    # 16-bit target's pack-output buffer copy, and one whose abandoned recording's buffer copy read
    # the pack buffer that the next group rewrites -- through the flush, the recover boundary and
    # the real flip, makes one pack-set allocation fail (at a chosen call, or every one of them),
    # records every read, write, barrier, submit and fence wait with a sequence number, and reports
    # for every pair of overlapping accesses to the same buffer whether a barrier strictly between
    # them -- or a submission whose fence wait completed, across recordings -- orders the later one
    # after the earlier. The deciding check is that the reuse really happened AND that nothing is
    # left unordered, for the write-after-write and the write-after-read arrows alike. Cross-
    # recording ordering is deliberately stricter than Vulkan (it requires a completed fence wait),
    # so the gate decides this implementation and fails safe rather than accepting every valid
    # alternative.
    #
    # Built exactly like the fault gate: NO copy of the algorithm. Its translation unit #includes
    # the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so the bodies under
    # test are the production ones, and the toolkit is NOT linked (the five external symbols it
    # references are stubbed in the test, and every Vulkan entry point the driven paths reach is a
    # static function pointer the test points at its own recording stubs; the injected allocation
    # failure is a stub return value, not real GPU OOM). scripts/vk-readback-ordering-gate.py
    # re-reads the same bytes at BUILD time: a definition, struct field or global the fixture binds
    # to that is gone fails the BUILD (exit 2). Like the callers' probe it also runs against the
    # PRE-FIX source, so the repair's own seam is probed as markers the test prints rather than
    # anchors it requires.
    #
    # No SKIP_RETURN_CODE and no skip path: the seam is inside the translation unit, so the case
    # always runs and exits 0 or 1.
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the readback ordering-gate test compiles "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_RB_ORDER_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-readback-ordering-gate")
        set(VK_RB_ORDER_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_RB_ORDER_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-readback-ordering-gate.py")
        set(VK_RB_ORDER_HEADER "${VK_RB_ORDER_DIR}/vk_readback_ordering_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_RB_ORDER_HEADER}" "${VK_RB_ORDER_DIR}/vk_readback_ordering_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_RB_ORDER_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_RB_ORDER_PROBE}"
                    --source "${VK_RB_ORDER_SOURCE}"
                    --header "${VK_RB_ORDER_HEADER}"
                    --manifest "${VK_RB_ORDER_DIR}/vk_readback_ordering_gate_seam.txt"
            DEPENDS "${VK_RB_ORDER_SOURCE}" "${VK_RB_ORDER_PROBE}"
            COMMENT "vk_readback_ordering_gate: probing nv2a_vk_backend.c for the seam the ordering regression binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_readback_ordering_gate_probe DEPENDS "${VK_RB_ORDER_HEADER}")
        add_executable(jsrf_vk_readback_ordering_gate_test
                       tests/test_vk_readback_ordering_gate.c)
        # xbox_video is what generates the SPIR-V headers the back end includes; the probe target
        # writes the seam header the test includes.
        add_dependencies(jsrf_vk_readback_ordering_gate_test
                         jsrf_vk_readback_ordering_gate_probe xbox_video)
        target_include_directories(jsrf_vk_readback_ordering_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_RB_ORDER_DIR}")
        target_link_libraries(jsrf_vk_readback_ordering_gate_test PRIVATE ${CMAKE_DL_LIBS})
        add_test(NAME jsrf_vk_readback_ordering_gate COMMAND jsrf_vk_readback_ordering_gate_test)
        set_tests_properties(jsrf_vk_readback_ordering_gate PROPERTIES TIMEOUT 300)
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_readback_ordering_gate is not registered -- "
                       "it compiles the toolkit's Vulkan back end, which this build does not "
                       "(${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── N4.3: the synchronization-scope and sync-paths gates (plan-jsrf-gameplay.md) ─────────
    # N4.3 narrowed every normal-path barrier in the toolkit's Vulkan back end to the stage and
    # access scopes of the producer and of the consumers it actually orders, and left two sites
    # deliberately broad (the per-submission barrier in vb_slot_submit, which is what makes
    # cross-submission ordering defined, and the readback abandonment barrier in vb_targets_flush).
    # These are the two cases that decide it, both written by workers distinct from the production
    # change and from each other's verification:
    #
    #   jsrf_vk_sync_scope_gate -- NO GPU, no ICD, no skip path. Its translation unit #includes the
    #   toolkit's src/video/nv2a_vk_backend.c verbatim with recording stubs over every Vulkan entry
    #   point, drives the production bodies that record each changed barrier (target load, pass end,
    #   target/depth create, texture upload, self-snapshot, the pack dispatch -> buffer copy pair and
    #   the publish barrier), and decides from the recorded stream: each site's scopes are exactly
    #   the ones its producer/consumer argument names (bound to the production macros' own values);
    #   for every pair of conflicting accesses to the same resource and subresource an intervening
    #   barrier covers BOTH sides, per hazard class, each with a positive count so no class can pass
    #   vacuously; every recorded access's image layout equals the tracked layout and every
    #   transition's oldLayout equals it; and the narrow form is really in force -- the only
    #   ALL_COMMANDS barriers are the per-submission ones, one per submitted recording.
    #
    #   jsrf_vk_sync_paths_gate -- the real device (software ICD, lavapipe), installed through the
    #   production nv2a_vk_backend_install() and observed through forwarders over the real driver
    #   entry points, so the masks it decides are the ones the driver really received. It drives
    #   repeated draws, a 16-bit and a 4-byte target, depth/stencil, a texture upload and a sample,
    #   a self-sampling snapshot and a flip that packs and copies in one flush; it requires each
    #   changed site to have recorded its exact masks at least once, requires the reachability
    #   counters to be nonzero, and checks the published guest bytes against its own exact
    #   expectation (the 16-bit truncation included).
    #
    # Built like the readback ordering gate: the probe scripts re-read the same backend bytes at
    # BUILD time and fail the build (exit 2) when an anchor moved or a scope macro's value changed;
    # a marker that is 0 is a printed failed check, never a build error, so the pre-change source
    # still builds and is reported against. The private mutation receipts for these cases (9 mutants
    # for the scope gate, 11 for the paths gate, with a comment-only control green in each) are in
    # the plan N4.3 records.
    if(XBR_VULKAN_FOUND)
        find_package(Vulkan QUIET)
        if(NOT Vulkan_INCLUDE_DIRS)
            message(FATAL_ERROR
                "Vulkan is available (XBR_VULKAN_FOUND) but find_package(Vulkan) found no "
                "include directory here; the N4.3 sync-gate tests compile "
                "src/video/nv2a_vk_backend.c, which needs <vulkan/vulkan.h>.")
        endif()
        set(VK_SYNC_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-sync-scope-gate")
        set(VK_SYNC_SOURCE "${XBOXRECOMP_DIR}/src/video/nv2a_vk_backend.c")
        set(VK_SYNC_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-sync-scope-gate.py")
        set(VK_SYNC_HEADER "${VK_SYNC_DIR}/vk_sync_scope_gate_seam.h")
        # A non-zero exit here fails the build, which is the point: a seam that moved must break
        # the build rather than quietly turn the case into something else.
        add_custom_command(
            OUTPUT "${VK_SYNC_HEADER}" "${VK_SYNC_DIR}/vk_sync_scope_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_SYNC_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_SYNC_PROBE}"
                    --source "${VK_SYNC_SOURCE}"
                    --header "${VK_SYNC_HEADER}"
                    --manifest "${VK_SYNC_DIR}/vk_sync_scope_gate_seam.txt"
            DEPENDS "${VK_SYNC_SOURCE}" "${VK_SYNC_PROBE}"
            COMMENT "vk_sync_scope_gate: probing nv2a_vk_backend.c for the seam and scope macros this gate binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_sync_scope_gate_probe DEPENDS "${VK_SYNC_HEADER}")
        add_executable(jsrf_vk_sync_scope_gate_test tests/test_vk_sync_scope_gate.c)
        add_dependencies(jsrf_vk_sync_scope_gate_test
                         jsrf_vk_sync_scope_gate_probe xbox_video)
        target_include_directories(jsrf_vk_sync_scope_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_SYNC_DIR}")
        target_link_libraries(jsrf_vk_sync_scope_gate_test PRIVATE ${CMAKE_DL_LIBS})
        # No SKIP_RETURN_CODE and no skip path: the seam is inside the translation unit, so the case
        # always runs and exits 0 or 1.
        add_test(NAME jsrf_vk_sync_scope_gate COMMAND jsrf_vk_sync_scope_gate_test)
        set_tests_properties(jsrf_vk_sync_scope_gate PROPERTIES TIMEOUT 300)

        set(VK_SYNC_PATHS_DIR "${CMAKE_CURRENT_BINARY_DIR}/vk-sync-paths-gate")
        set(VK_SYNC_PATHS_PROBE "${CMAKE_CURRENT_SOURCE_DIR}/scripts/vk-sync-paths-gate.py")
        set(VK_SYNC_PATHS_HEADER "${VK_SYNC_PATHS_DIR}/vk_sync_paths_gate_seam.h")
        add_custom_command(
            OUTPUT "${VK_SYNC_PATHS_HEADER}" "${VK_SYNC_PATHS_DIR}/vk_sync_paths_gate_seam.txt"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${VK_SYNC_PATHS_DIR}"
            COMMAND "${_jsrf_python}" -X utf8 "${VK_SYNC_PATHS_PROBE}"
                    --source "${VK_SYNC_SOURCE}"
                    --header "${VK_SYNC_PATHS_HEADER}"
                    --manifest "${VK_SYNC_PATHS_DIR}/vk_sync_paths_gate_seam.txt"
            DEPENDS "${VK_SYNC_SOURCE}" "${VK_SYNC_PATHS_PROBE}"
            COMMENT "vk_sync_paths_gate: probing nv2a_vk_backend.c for the seam this real-device gate binds to"
            VERBATIM)
        add_custom_target(jsrf_vk_sync_paths_gate_probe DEPENDS "${VK_SYNC_PATHS_HEADER}")
        add_executable(jsrf_vk_sync_paths_gate_test tests/test_vk_sync_paths_gate.c)
        add_dependencies(jsrf_vk_sync_paths_gate_test
                         jsrf_vk_sync_paths_gate_probe xbox_video)
        target_include_directories(jsrf_vk_sync_paths_gate_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/video
            ${Vulkan_INCLUDE_DIRS}
            "${CMAKE_BINARY_DIR}/xboxrecomp/src/video/spirv"
            "${VK_SYNC_PATHS_DIR}")
        target_link_libraries(jsrf_vk_sync_paths_gate_test PRIVATE ${CMAKE_DL_LIBS})
        # It skips (77) exactly like its siblings when no software ICD is configured or when the
        # back end cannot be installed -- notably in an elevated process, where the loader ignores
        # VK_DRIVER_FILES. The skip is the case's own decision and prints its reason.
        if(JSRF_VK_TEST_ICD)
            file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_sync)
            add_test(NAME jsrf_vk_sync_paths_gate COMMAND jsrf_vk_sync_paths_gate_test)
            set_tests_properties(jsrf_vk_sync_paths_gate PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77
                ENVIRONMENT "VK_DRIVER_FILES=${_vk_icd_sync};VK_ICD_FILENAMES=${_vk_icd_sync};${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
        else()
            add_test(NAME jsrf_vk_sync_paths_gate
                     COMMAND jsrf_vk_sync_paths_gate_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vk_sync_paths_gate PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()
    else()
        message(STATUS "Vulkan/unavailable: jsrf_vk_sync_scope_gate and jsrf_vk_sync_paths_gate are "
                       "not registered -- they compile the toolkit's Vulkan back end, which this "
                       "build does not (${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()
    # ── V6: the vertex-program conformance gate (plan-jsrf-gameplay.md, step V6) ─────────────
    # Authored by a worker distinct from V5's authors, and it asks a different question from V5's
    # gate: not "is the generated retail corpus self-consistent" but "does the offline translation
    # pipeline and the corpus seam reproduce the INTERPRETER for the synthetic V1..V7 taxonomy",
    # on the bytes and on the pixels. Its fixtures are the parity test's own V1..V6 programs: the
    # region between that file's VSH-ENCODER-BEGIN/END markers is extracted verbatim here at
    # configure time (re-extracting whenever the file changes), so the encoder cannot drift from
    # the one the parity test uses. The fixture SPIR-V is compiled at BUILD time from those same
    # words with the same dxc/flags scripts/gen-shaders.py uses for the retail corpus. What is
    # checked at run time is narrower than "the blob came from those words": the test recomputes
    # the header's words hash from the words it uploads and the header's spirv_hash from the bytes
    # of the blob it carries, so a hand-edited or half-regenerated header is caught. The generation
    # step (the generator writes each fixture's words and hands that file to the tool) plus the
    # pixel comparison are what actually tie the shader to the program.
    #
    # Registered only when the Vulkan shader toolchain is present, like the toolkit's vk_smoke
    # suite: without dxc there is no gate to run, and an unregistered test is honest where a
    # green no-op would not be. With a toolchain but no device, the test exits 77 (Not Run) unless
    # one of its checks has already failed, in which case it is a hard failure.
    #
    # XBR_VULKAN_FOUND is the toolkit's ONE bound capability decision, published as a
    # FORCE-refreshed CACHE bool precisely so it reaches here (a plain variable in the toolkit
    # subdirectory's scope does not); the hand-written re-derivation that used to stand here --
    # `if(XBR_DXC AND XBR_SPIRV_VAL AND Vulkan_LIBRARY)`, a second and slightly different rule --
    # is gone, so this gate cannot disagree with the branch that chose the corpus above.
    if(XBR_VULKAN_FOUND)
        set(VSH_CONF_DIR "${CMAKE_CURRENT_BINARY_DIR}/vsh-conformance")
        file(MAKE_DIRECTORY "${VSH_CONF_DIR}" "${VSH_CONF_DIR}/scratch")
        set(_vsh_parity "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gpu_backend_parity.c")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_vsh_parity}")
        file(READ "${_vsh_parity}" _vsh_text)
        # file(READ) already collapses CRLF to LF; be explicit so the markers and the
        # post-write comparison below are unambiguous whatever the file's endings are.
        string(REPLACE "\r\n" "\n" _vsh_text "${_vsh_text}")
        string(FIND "${_vsh_text}" "/* VSH-ENCODER-BEGIN */" _vsh_b)
        string(FIND "${_vsh_text}" "/* VSH-ENCODER-END */" _vsh_e)
        if(_vsh_b EQUAL -1 OR _vsh_e EQUAL -1 OR _vsh_e LESS _vsh_b)
            message(FATAL_ERROR
                "V6: the VSH-ENCODER-BEGIN/END markers are not in ${_vsh_parity}; the "
                "conformance gate's fixtures come from that region and cannot be extracted")
        endif()
        string(LENGTH "/* VSH-ENCODER-END */" _vsh_tail)
        math(EXPR _vsh_len "${_vsh_e} + ${_vsh_tail} - ${_vsh_b}")
        string(SUBSTRING "${_vsh_text}" ${_vsh_b} ${_vsh_len} _vsh_region)
        file(WRITE "${VSH_CONF_DIR}/vsh_encoder_region.inc" "${_vsh_region}")
        # Verify the copy really is the region: read it back, normalize its endings the same way,
        # and compare. A mismatch fails the CONFIGURE, so a drifted extraction cannot reach a
        # build. (file(WRITE) writes the host's line endings; the region's content is unchanged,
        # which is what matters for the compiler and for the drift guarantee.)
        file(READ "${VSH_CONF_DIR}/vsh_encoder_region.inc" _vsh_back)
        string(REPLACE "\r\n" "\n" _vsh_back "${_vsh_back}")
        if(NOT _vsh_back STREQUAL _vsh_region)
            message(FATAL_ERROR
                "V6: the extracted encoder region does not round-trip (${_vsh_len} bytes "
                "extracted, ${_vsh_back} read back); the fixtures would not be the parity test's")
        endif()
        message(STATUS "V6: extracted ${_vsh_len} bytes of the V1..V7 encoder from "
                       "tests/test_gpu_backend_parity.c and verified the copy")

        # The generator: the same source, compiled as a build-time tool (VSH_CONFORMANCE_EMIT_FIXTURES).
        add_executable(jsrf_vsh_conformance_gen tests/test_vsh_conformance.c)
        target_include_directories(jsrf_vsh_conformance_gen PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JSRF_GEN_INCLUDE_DIR}
            ${VSH_CONF_DIR})
        target_compile_definitions(jsrf_vsh_conformance_gen PRIVATE
            VSH_CONFORMANCE_EMIT_FIXTURES)
        target_link_libraries(jsrf_vsh_conformance_gen PRIVATE xboxrecomp)

        add_custom_command(
            OUTPUT "${VSH_CONF_DIR}/vsh_fixtures.inc"
            COMMAND jsrf_vsh_conformance_gen
                    --out "${VSH_CONF_DIR}/vsh_fixtures.inc"
                    --work "${VSH_CONF_DIR}/scratch"
                    --tool "$<TARGET_FILE:nv2a_vsh_hlsl>"
                    --dxc "${XBR_DXC}"
                    --spirv-val "${XBR_SPIRV_VAL}"
            DEPENDS jsrf_vsh_conformance_gen nv2a_vsh_hlsl "${_vsh_parity}"
            COMMENT "V6: compiling the V1..V6 fixture programs to SPIR-V from their own words"
            VERBATIM)

        add_executable(jsrf_vsh_conformance_test
            tests/test_vsh_conformance.c ${JSRF_CORPUS_SOURCES}
            "${VSH_CONF_DIR}/vsh_fixtures.inc")
        target_include_directories(jsrf_vsh_conformance_test PRIVATE
            ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/nv2a ${XBOXRECOMP_DIR}/src/video
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${JSRF_GEN_INCLUDE_DIR}
            ${JSRF_CORPUS_INCLUDE_DIR}
            ${VSH_CONF_DIR})
        target_link_libraries(jsrf_vsh_conformance_test PRIVATE xboxrecomp)

        file(TO_NATIVE_PATH "${JSRF_VK_TEST_ICD}" _vk_icd_conf)
        set(_vsh_conf_env
            "VK_DRIVER_FILES=${_vk_icd_conf};VK_ICD_FILENAMES=${_vk_icd_conf};RECOMP_GPU_BACKEND=vulkan;${JSRF_VK_TEST_SELECT};RECOMP_VK_PIPELINE_CACHE=off")
        set(_vsh_conf_tool "$<TARGET_FILE:nv2a_vsh_hlsl>")

        if(JSRF_VK_TEST_ICD)
            add_test(NAME jsrf_vsh_conformance
                     COMMAND jsrf_vsh_conformance_test --tool "${_vsh_conf_tool}")
            set_tests_properties(jsrf_vsh_conformance PROPERTIES TIMEOUT 600
                SKIP_RETURN_CODE 77 ENVIRONMENT "${_vsh_conf_env}")
            # V7 is V1's scene with RECOMP_GPU_VSH=0, and the executor reads that variable once
            # per process, so it is its own case.
            add_test(NAME jsrf_vsh_conformance_v7
                     COMMAND jsrf_vsh_conformance_test --v7-only --tool "${_vsh_conf_tool}")
            set_tests_properties(jsrf_vsh_conformance_v7 PROPERTIES TIMEOUT 300
                SKIP_RETURN_CODE 77
                ENVIRONMENT "RECOMP_GPU_VSH=0;${_vsh_conf_env}")
            # The gate's own negative controls, as separate cases: they prove the checks are
            # load-bearing. Each reports the CHECK's verdict as its exit status -- 1 when the check
            # correctly failed the injected defect (the expected, load-bearing result) and 2 when it
            # missed it. WILL_FAIL cannot express that: it would also accept exit 2 (a control that
            # missed the defect) and any crash, so the case is asserted on the exact success line
            # instead, which only the exit-1 path prints.
            add_test(NAME jsrf_vsh_conformance_control1
                     COMMAND jsrf_vsh_conformance_test --negative-control 1 --tool "${_vsh_conf_tool}")
            add_test(NAME jsrf_vsh_conformance_control2
                     COMMAND jsrf_vsh_conformance_test --negative-control 2 --tool "${_vsh_conf_tool}")
            add_test(NAME jsrf_vsh_conformance_control3
                     COMMAND jsrf_vsh_conformance_test --negative-control 3 --tool "${_vsh_conf_tool}")
            add_test(NAME jsrf_vsh_conformance_control4
                     COMMAND jsrf_vsh_conformance_test --negative-control 4 --tool "${_vsh_conf_tool}")
            # Control 5 is the one that feeds check_cpu -- the predicate behind checks 2, 3 and 4 --
            # a real mismatch: V3's table entry pointed at V5's SPIR-V, which shares its input_mask,
            # so the identity check still passes and only the pixels are wrong.
            add_test(NAME jsrf_vsh_conformance_control5
                     COMMAND jsrf_vsh_conformance_test --negative-control 5 --tool "${_vsh_conf_tool}")
            # Control 6 feeds check 1's output-location assertion a moved varying: t1 from
            # location 4 to 7. The location COUNT is unchanged and the combiner reads d0 alone, so
            # no pixel check can see it -- which is why the locations are asserted by name. Needs
            # no device, like control 1.
            add_test(NAME jsrf_vsh_conformance_control6
                     COMMAND jsrf_vsh_conformance_test --negative-control 6 --tool "${_vsh_conf_tool}")
            set_tests_properties(jsrf_vsh_conformance_control1 jsrf_vsh_conformance_control2
                                 jsrf_vsh_conformance_control3 jsrf_vsh_conformance_control4
                                 jsrf_vsh_conformance_control5 jsrf_vsh_conformance_control6
                                 PROPERTIES TIMEOUT 300
                PASS_REGULAR_EXPRESSION "correctly FAILED the simulated defect"
                FAIL_REGULAR_EXPRESSION "MISSED the simulated defect"
                ENVIRONMENT "${_vsh_conf_env}")
        else()
            add_test(NAME jsrf_vsh_conformance
                     COMMAND jsrf_vsh_conformance_test --skip "XBR_VK_SW_ICD is not set")
            set_tests_properties(jsrf_vsh_conformance PROPERTIES TIMEOUT 20 SKIP_RETURN_CODE 77)
        endif()

        # The channel gate: the four-channel predicate that accepts or rejects an image, driven
        # directly (an alpha-only image must be rejected, an in-tolerance one accepted, each of red,
        # green and blue must discriminate, and coverage must stay RGB-only). It is DELIBERATELY
        # outside the XBR_VK_SW_ICD branch above: the process exits before the Vulkan back end is
        # installed, so it needs no ICD, no device and no environment, and it can never report a
        # skip -- there is no SKIP_RETURN_CODE, and none should be added.
        add_test(NAME jsrf_vsh_conformance_channelgate
                 COMMAND jsrf_vsh_conformance_test --channel-gate --tool "${_vsh_conf_tool}")
        set_tests_properties(jsrf_vsh_conformance_channelgate PROPERTIES TIMEOUT 120)
    else()
        message(STATUS "V6: the vertex-program conformance gate is not registered -- Vulkan, dxc "
                       "and spirv-val are all needed to compile its fixtures (${XBR_VULKAN_UNAVAILABLE_REASON})")
    endif()

    # Off Windows this fixture is NOT registered: unvalidated POSIX integration. Measured on a
    # dependency-limited Ubuntu WSL host, the fixture and every toolkit source it needs compile but
    # linking leaves 35 undefined references, because the POSIX providers of xbox_FramebufferWindow*,
    # xbox_VideoIsPlaying/PlayFile, xa2_* and mcpx_apu_* live in the SDL2/XAudio2 sources and that
    # image has no SDL2, epoxy or OpenSSL headers, no CMake and no pkg-config. A Mac is UNTESTED: this
    # guard makes no claim that a Mac with its declared SDL2 prerequisites would fail the same way.
    # The crash this guard used to name -- the toolkit's POSIX ResetEvent dereferencing the non-event
    # guest VA the header-validation case passes as a handle -- is repaired as measured by the native
    # probe: a value the shim never published is rejected with ERROR_INVALID_HANDLE / WAIT_FAILED
    # (TR §31.71). The fixture's own case has not been executed on any POSIX host. Re-registering it
    # needs actual integration evidence, not a repaired crash reason.
    if(WIN32)
    add_executable(jsrf_inplace_event_test
        ${XBOXRECOMP_DIR}/tests/kernel_inplace_event_test.c)
    target_include_directories(jsrf_inplace_event_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_inplace_event_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_inplace_event_bridge COMMAND jsrf_inplace_event_test)
    set_tests_properties(jsrf_inplace_event_bridge PROPERTIES TIMEOUT 20)
    else()
        message(STATUS "jsrf_inplace_event_bridge is not registered off Windows -- unvalidated POSIX "
                       "integration: on the measured Ubuntu WSL host the fixture's SDL2/OpenSSL/"
                       "qemu-shim POSIX providers are absent and linking fails; a Mac is untested. "
                       "(The POSIX ResetEvent crash it used to name is repaired, TR §31.71.)")
    endif()
    add_executable(jsrf_dpc_queue_test
        ${XBOXRECOMP_DIR}/tests/kernel_dpc_queue_test.c)
    target_include_directories(jsrf_dpc_queue_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_dpc_queue_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_dpc_queue_bridge COMMAND jsrf_dpc_queue_test)
    set_tests_properties(jsrf_dpc_queue_bridge PROPERTIES TIMEOUT 20)
    if(NOT JSRF_FIXTURES_ONLY)
    if(WIN32)
    # A semaphore release (0x1D70) writes its value into a release-opted fence
    # mirror's word and the mirror tick then leaves it alone; live mode never
    # latches. Needs the whole kernel, so it links the game's xboxrecomp target.
    # One process per mode: RECOMP_FENCE_MIRROR_LIVE is read once.
    add_executable(jsrf_fence_release_runtime_test
        ${XBOXRECOMP_DIR}/tests/fence_release_runtime_test.c)
    target_include_directories(jsrf_fence_release_runtime_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_fence_release_runtime_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_fence_release_runtime COMMAND jsrf_fence_release_runtime_test normal)
    set_tests_properties(jsrf_fence_release_runtime PROPERTIES TIMEOUT 60)
    add_test(NAME jsrf_fence_release_runtime_live COMMAND jsrf_fence_release_runtime_test live)
    set_tests_properties(jsrf_fence_release_runtime_live PROPERTIES TIMEOUT 60
        ENVIRONMENT "RECOMP_FENCE_MIRROR_LIVE=1")
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
        ${JSRF_GEN_INCLUDE_DIR})
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
    endif()
    add_test(NAME jsrf_gpu_inspection COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gpu_inspection.py)
    add_test(NAME jsrf_host_tools COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_host_tools.py)
    add_test(NAME jsrf_logq COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_logq.py)
    add_test(NAME jsrf_key_script_run COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_key_script_run.py)
    add_test(NAME jsrf_bench_summary COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bench_summary.py)
    add_test(NAME jsrf_text_stability COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_text_stability.py)
    add_test(NAME jsrf_guest_ram_search COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_guest_ram_search.py)
    add_test(NAME jsrf_check_graffiti_run COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_graffiti_run.py)
    add_test(NAME jsrf_check_progression_run COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_progression_run.py)
    add_test(NAME jsrf_fcmov_clamp COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fcmov_clamp.py)
    # The mutation harness owns only the work area it creates. This drives its
    # actual main() with the copy boundary stubbed: an existing --work path is
    # never deleted, and a failed fresh run removes exactly its own root.
    add_test(NAME jsrf_pbcov_workdir_helper
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pbcov_workdir.py)
    set_tests_properties(jsrf_pbcov_workdir_helper PROPERTIES TIMEOUT 120)
    # The BUMPENVMAP control harness owns only the work area it creates. This
    # drives its real main() with the copy/build boundary stubbed: an existing
    # --workdir (including one holding a toolkit-<control>/ child) is refused
    # byte-unchanged, and a fresh owned root is removed exactly once.
    add_test(NAME jsrf_bumpenv_controls_workdir
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bumpenv_controls_workdir.py)
    set_tests_properties(jsrf_bumpenv_controls_workdir PROPERTIES TIMEOUT 120)
    # The CPU fixture must build OFF Windows, so this decides that for real: it
    # compiles, links and runs the shipped fixture sources with a POSIX compiler
    # (WSL GCC on Windows) and requires all five cases to print PASS. It skips
    # (return 77) only when no POSIX compiler exists at all.
    add_test(NAME jsrf_bumpenv_fixture_portability
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_bumpenv_fixture_portability.py)
    set_tests_properties(jsrf_bumpenv_fixture_portability PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    # The NV2A method-inventory generator writes TWO tracked files (this repo's
    # docs/jsrf-nv2a-method-inventory.md and the toolkit's nv2a_method_table.c)
    # and parses argv by hand.  `--help` used to fall through that hand parsing,
    # so the run-name list took its default of ONE old run and the table was
    # rewritten with ~86 methods dropped -- a destructive no-op that looked like
    # a successful run (2026-10-06).  These tests pin the guards that make bad
    # input fail closed instead.
    add_test(NAME jsrf_nv2a_inventory_guard
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_gen_nv2a_method_inventory_guard.py)
    set_tests_properties(jsrf_nv2a_inventory_guard PROPERTIES TIMEOUT 300)
    add_test(NAME jsrf_recovery_span_ownership COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_recovery_span_ownership.py)
    # Stack-depth CFG validation over the recovered manifest.  Registered because
    # the four defects it detects (0x80BD0, 0xBB7B0, 0x7DA30, 0x74C70) each cost a
    # game run to find, and every one was a stack-contract contradiction visible
    # statically from the original bytes.  The test re-runs the checker's own
    # historical controls, asserts the corrected entries are clean, asserts the
    # gate passes with NO baseline, and proves the gate can fail by feeding it a
    # manifest with one proven entry reverted.  Pure Python over the XBE and the
    # manifest; no build target needed.
    # TIMEOUT is generous because it walks every manifest entry several times.
    add_test(NAME jsrf_stack_depth
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stack_depth.py)
    set_tests_properties(jsrf_stack_depth PROPERTIES TIMEOUT 600)
    # Hidden-entry detection over the recovered manifest.  Registered because the
    # motivating defect (0x7DAE0 declared [0x7DAE0, 0x7DE20) consuming the 190-
    # instruction function at 0x7DBD0) has an `end` that is simultaneously its own
    # declared end AND the next database function start, so check-entry-extents.py
    # cannot see it -- and 48 more of the same shape were live.  The test re-runs
    # the checker's real-defect controls, requires the 0x96F60 indirect-exit case to
    # stay UNQUALIFIED, asserts the gate passes with NO baseline, and proves the gate
    # can fail by feeding it a manifest with the motivating defect re-injected.
    # Pure Python over the XBE and the manifest; no build target needed.
    add_test(NAME jsrf_hidden_entries
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_hidden_entries.py)
    set_tests_properties(jsrf_hidden_entries PROPERTIES TIMEOUT 900)
    # A jump to an entry with its own symbol must be a tail call, never a deleted goto or
    # a trap. check-dead-gotos.py scans the generated and recovered trees: any `(void)0`
    # deleted-jump comment fails, as does any RECOMP_JUMP_TRAP whose target has a symbol of
    # its own; its test pins each shape against a synthetic tree written in the spellings the
    # generator uses and asserts the current tree is clean. The thunk test compiles the real
    # sub_00169000 (`sub [esp+4],4; jmp 0x168230`) against a stand-in callee with `ret 8` and
    # checks the callee is entered and esp ends 12 above entry; it fails on the pre-fix lift.
    add_test(NAME jsrf_check_dead_gotos
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_check_dead_gotos.py)
    add_test(NAME jsrf_tail_thunk_169000
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tail_thunk_169000.py)
    set_tests_properties(jsrf_tail_thunk_169000 PROPERTIES TIMEOUT 120)
    # An alias folded onto an owner that starts after it would dispatch to a different
    # function. The test pins 0xC42E0/0x11B680 as their own dispatch entries and compiles each
    # after-owner alias wrapper as generated: it must abort and never reach the owner.
    add_test(NAME jsrf_alias_dispatch
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_alias_dispatch.py)
    set_tests_properties(jsrf_alias_dispatch PROPERTIES TIMEOUT 120)
    # Save isolation: the runner's observation parser, its RECOMP_SAVE_DIR default and
    # caller-override recording (through the real main() with the launch replaced), and the
    # refusal block compiled out of src/main.c.
    add_test(NAME jsrf_save_isolation
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_save_isolation.py)
    set_tests_properties(jsrf_save_isolation PROPERTIES TIMEOUT 120)
    # P0.7 generation provenance.  Registered here because it was NOT: the checker was
    # wired into `just check` only, so `just test` could not catch a provenance failure,
    # and the gate stayed red for 73 commits without a session noticing (measured
    # 2026-09-30).  It is a pure-Python unittest over the recorded manifest, the
    # preservation baseline and the live generated tree, so it needs no build target.
    # TIMEOUT is generous because it hashes the generated tree and runs `--check-only`.
    add_test(NAME jsrf_generation_provenance
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_generation_provenance.py)
    set_tests_properties(jsrf_generation_provenance PROPERTIES TIMEOUT 300)
    # Run-coverage check.  Registered because this session recorded an entry as
    # "run-confirmed" by a run that never dispatched it (Turn Reviewer finding B3):
    # f25 was credited for 0x00032610 and has zero ABI-verified return lines for it,
    # while f27 genuinely does.  The controls assert that the two real runs disagree,
    # so the check reads a return event rather than matching text.  Pure Python over
    # archived logs; no build target needed.
    add_test(NAME jsrf_run_exercised
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_run_exercised.py)
    # Dispatch-table count/array consistency.  Registered because a real host crash
    # shipped through a fully green suite: `remove-b5f3a-dispatch` removed one tuple
    # from `g_recomp_table[]` but left `g_recomp_table_size = 8928` while the array
    # held 8927, so `recomp_dispatch_init()` read one entry past the end and the
    # process died with an access violation BEFORE `guest_entry` (runs g08/g08b).
    # The count is now DERIVED from the array (`sizeof`), so the class is structurally
    # impossible; this test is the independent gate for trees produced by any other
    # route, and its first control re-injects the exact 8928-vs-8927 mismatch and
    # requires the real script to exit nonzero.  Pure Python over the generated unit.
    add_test(NAME jsrf_dispatch_table
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_dispatch_table_size.py)
    # Stop-chain record consistency.  Registered because the stop chain has twice
    # claimed progression its archived runs did not establish: 0x00032610 was
    # credited to a run that never dispatched it (f25, Turn Reviewer finding B3),
    # and 0xB5EB0 was repaired and then credited by two runs that never executed
    # it.  The gate binds every row to its cited archives -- the ABI-verified
    # return line, the log's own hash, and the run revision's descent from the
    # repair commit -- and refuses a row that claims more than they show.  Its
    # first control takes a genuine confirming row and cites a run with no return
    # line, which must fail.  Pure Python over archived logs and git ancestry.
    add_test(NAME jsrf_stop_chain
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stop_chain.py)
    set_tests_properties(jsrf_stop_chain PROPERTIES TIMEOUT 300)
    # V5's corpus and run-gate checkers, registered like the other pure-Python tests
    # so a broken corpus cannot pass merely because nobody ran them by hand.
    #
    # What jsrf_vsh_aot_table actually covers, precisely: its fixtures build
    # SYNTHETIC tables and run the checker with --skip-spirv --skip-coverage, so
    # they are device-free unit tests of the CHECKER -- that it rejects an unsorted
    # table, a wrong digest, a wrong size field and so on.  They compile no shader
    # and never read the real generated corpus, so they do NOT guard it, and an
    # earlier version of this comment implied they did.  The real table's 126
    # entries, recompiled from their own words and compared byte for byte against
    # the retail XBE, are guarded by `just check` (scripts/check-vsh-aot-table.py
    # with no flags, ~20 s) -- not by this test.  The run gate reads a completed
    # run's two independent counter sources.
    add_test(NAME jsrf_vsh_aot_table
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vsh_aot_table.py)
    add_test(NAME jsrf_vsh_run_gate
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vsh_run_gate.py)
    set_tests_properties(jsrf_vsh_aot_table jsrf_vsh_run_gate PROPERTIES TIMEOUT 120)
    # R1's provenance fixtures, registered for the same reason: the gate that refuses a
    # stale or unverifiable corpus is only as good as the proof that it refuses.  Six
    # claims are pinned, each through the SHIPPED script that makes it.
    #
    # `tool.sha256` is recorded AND DECISIVE (schema 2).  The recorded executable is the
    # owned artifact under build/vsh-tool -- the game build tree's copy is deliberately
    # not a candidate, because `just build` relinks it with a non-reproducible PDB
    # signature -- so a corpus recording a different tool, or omitting the field from
    # BOTH files, refuses; `expected(tool_path=None)` refuses too, and a `want` that
    # lacks tool.sha256 refuses naming the missing expectation.  Moving a source hash,
    # dxc version, dxc/tool args, programs, distinct or schema, a missing deciding field,
    # a malformed block, a missing or blockless corpus file, two blocks in one file, the
    # two files recording different provenance, and a `distinct` that disagrees with the
    # table's declared `nv2a_vsh_program_count` all refuse as well, each naming itself.
    #
    # ONE toolkit binds the guard, the generator and the CMake link: the build tree's
    # cached XBOXRECOMP_DIR wins, the environment and the sibling are the fallbacks, a
    # cache/environment disagreement is a refusal naming BOTH paths, and a root that is
    # not a toolkit checkout is refused naming the missing files.  A `JSRF_NV2A_VSH_HLSL`
    # override is freshness-verified (content first, mtime second) and REFUSED when
    # stale, with both remedies and the rule that the script never rebuilds or replaces a
    # path the environment named -- it is not an advisory.
    #
    # The OWNED tool tree carries its own binding: `build/vsh-tool`'s CMakeCache records
    # the toolkit CMake configured THAT tree against (CMAKE_HOME_DIRECTORY, any entry
    # type), and a tree configured against a different toolkit than the one this build
    # resolves is refused -- by the generator and by the guard -- naming the tree, the
    # toolkit it was configured against, the resolved toolkit and the delete-and-re-run
    # remedy; a missing cache, a missing entry and the same tree in another spelling are
    # not refusals.  `--check` is non-mutating: the tool is FOUND (`check_host_tool`
    # -> `find_host_tool`, never built), the shared `staleness_refusal` rule applies
    # (content refuses for every tool, mtime only for a JSRF_NV2A_VSH_HLSL override, so an
    # owned artifact whose recorded source content still matches is returned untouched),
    # and no tool at all refuses naming `just shaders`.
    #
    # The build entry refuses BEFORE the CMake configure step, and that is measured, not
    # inferred from the source text: the fixtures run the real scripts/build-jsrf.py in a
    # scratch tree whose stub CMakeLists.txt writes build/R1R_CONFIGURE_RAN.txt as its
    # first action, so a refusal that lets configure run leaves the marker behind.  Seven
    # negative cases (changed tool hash, moved source hash, tool hash omitted from both
    # files, deleted owned tool, stale override, cache/environment conflict, and the owned
    # tool tree bound to another toolkit) must exit non-zero naming the reason and
    # `just shaders` with neither the marker nor logs/configure-current.log present, and a
    # matching scratch corpus must reach configure and print `corpus   : schema 2, ...`
    # naming the fake tool's hash.
    #
    # Finally the gates are shown to DETECT their own weakenings: a mutation harness
    # copies the shipped scripts, applies one localized substitution, and re-runs the
    # focused assertions in a child process with JSRF_R1_SCRIPTS pointed at the mutant.
    # Eight mutants must turn it red (compare skipping tool.sha256, check_corpus dropping
    # it from the required expectations, expected() accepting no tool, the guard
    # accepting "no tool found", the guard warning instead of refusing a stale override,
    # the guard call moved after `cmake -S`, resolve_toolkit silently resolving the
    # cache/environment conflict, and toolkit_binding_conflict never refusing a
    # differently-bound tool tree), and a comment-only edit must stay green.
    # Pure Python, no device, ~55 s (real corpus + real cmake configure in a scratch
    # tree, plus one real `gen-shaders.py --check` over the real game/ directory); the
    # fixtures are temp-dir isolated and never write the repository.
    add_test(NAME jsrf_vsh_aot_provenance
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vsh_aot_provenance.py)
    # It takes about 60 s alone and over 120 s beside the rest of the suite.
    set_tests_properties(jsrf_vsh_aot_provenance PROPERTIES TIMEOUT 300)
    # The input-mask rule's Python copy (find-vertex-programs.py) against the host tool's, which
    # shares the interpreter's single C definition.  Needs the built tool and nothing else.
    if(TARGET nv2a_vsh_hlsl)
        add_test(NAME jsrf_vsh_mask_rule
            COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vsh_mask_rule.py
                    "$<TARGET_FILE:nv2a_vsh_hlsl>")
        set_tests_properties(jsrf_vsh_mask_rule PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    endif()
    # R2's gating test: the corpus seam of a build WITHOUT the Vulkan toolchain.  Registered
    # UNCONDITIONALLY, and deliberately so: it is the gate of the branch that exists BECAUSE
    # the capability can be OFF, and gating it on XBR_VULKAN_FOUND would leave exactly that
    # branch -- the one an SDK-less host configures -- with no gate at all.  It needs no
    # build target, no device and no ICD: it configures the SHIPPED CMakeLists.txt (and the
    # shipped scripts/build-jsrf.py, and the capability probe that guards it) in scratch
    # trees under the temp directory, with CMake's own absence controls for the missing
    # toolchain and a COPY of the retail corpus plus a copy of the owned tool for the
    # present one, and it compiles and links one tiny consumer of the generated empty table
    # with the real compiler.  The owner's build tree, corpus, SDK and assets are never
    # written, and `python -X utf8 tests/test_optional_vulkan.py` is exactly what CTest runs.
    # Measured on this host: ~117 s for its ten cases (seven configures of the real project,
    # one small compile+link, four mutant child runs and one control child run), so the
    # TIMEOUT below is a little over two and a half times the measured runtime.
    add_test(NAME jsrf_optional_vulkan
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_optional_vulkan.py)
    set_tests_properties(jsrf_optional_vulkan PROPERTIES TIMEOUT 300)
    # G1: exercise the SHIPPED `JSRF_FIXTURES_ONLY=ON` configure in a tracked-source-only
    # scratch tree (no generated/recovered/game/retail inputs). It runs the real configure,
    # inspects the selected empty corpus and compiles/runs a consumer of that table, then
    # privately mutates the capability branch and the fixtures-only branch and requires both
    # to fail. Its own configure needs only CMake, the real toolkit and the Vulkan SDK.
    add_test(NAME jsrf_fixtures_only
        COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_fixtures_only.py)
    set_tests_properties(jsrf_fixtures_only PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
    if(WIN32)
    # A2h-arming-coverage-attribution C1/C2.  The fixture lives in the toolkit's tests/
    # directory, like tests/apu_watch_fixture_test.c and tests/kernel_inplace_event_test.c,
    # because it is a standalone collector fixture and the game repo keeps no fixture of
    # its own under tests/.  Registered TWICE, on purpose: the DR gate
    # (JSRF_TRACE_A2H_DR) is read once and cached, so one process can only ever exercise
    # one arm.  The gated arm asserts every new record; the unset arm is the inertness
    # control and asserts an EMPTY artifact plus unchanged counters -- record-level
    # inertness, not merely log silence.
    add_executable(jsrf_collect_arming_test
        ${XBOXRECOMP_DIR}/tests/collect_arming_fixture_test.c)
    # collect.c is compiled here for its gated branch bodies; its own main() is excluded so the
    # fixture's main() is the entry point. No game source is linked, so nothing of the collector
    # beyond the instrument under test is pulled in.
    target_sources(jsrf_collect_arming_test PRIVATE tools/harness/collect.c)
    target_include_directories(jsrf_collect_arming_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(jsrf_collect_arming_test PRIVATE dbghelp shell32)
    target_compile_definitions(jsrf_collect_arming_test PRIVATE
        _CRT_SECURE_NO_WARNINGS JSRF_COLLECT_NO_MAIN)
    add_test(NAME jsrf_collect_arming_gate_on COMMAND jsrf_collect_arming_test)
    set_tests_properties(jsrf_collect_arming_gate_on PROPERTIES
        TIMEOUT 30 ENVIRONMENT "JSRF_TRACE_A2H_DR=1")
    add_test(NAME jsrf_collect_arming_gate_off COMMAND jsrf_collect_arming_test)
    set_tests_properties(jsrf_collect_arming_gate_off PROPERTIES TIMEOUT 30)
    # A2h-live-slot-write-r1. The mechanism is page protection, so the fixtures perform REAL stores
    # and REAL loads against REAL PAGE_READONLY pages and let the REAL vectored handlers run: a
    # fixture that called the handler by hand would prove nothing about whether the host delivers a
    # write AV and withholds one for a read.
    #
    # Registered TWICE, on purpose, for the same reason the AC'97 arms above are: the gate
    # (JSRF_TRACE_A2H_SLOTW) is read once and cached, so one process can only ever exercise one arm.
    # The gate-on arm asserts every property; the gate-off arm is the inertness control and asserts
    # that ARM is REFUSED and no page is protected -- record-level inertness, not log silence.
    add_executable(jsrf_a2h_slotw_fixture_test
        ${XBOXRECOMP_DIR}/tests/a2h_slotw_fixture_test.c)
    target_include_directories(jsrf_a2h_slotw_fixture_test PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_a2h_slotw_fixture_test PRIVATE xboxrecomp)
    add_test(NAME jsrf_a2h_slotw_fixture_gate_on COMMAND jsrf_a2h_slotw_fixture_test
        ${CMAKE_CURRENT_SOURCE_DIR}/game/default.xbe)
    set_tests_properties(jsrf_a2h_slotw_fixture_gate_on PROPERTIES
        TIMEOUT 60 ENVIRONMENT "JSRF_TRACE_A2H_SLOTW=1")
    add_test(NAME jsrf_a2h_slotw_fixture_gate_off COMMAND jsrf_a2h_slotw_fixture_test)
    set_tests_properties(jsrf_a2h_slotw_fixture_gate_off PROPERTIES TIMEOUT 60)
    # C3-RR AC'97 channel-reset gate. Drives the PRODUCTION NABM owner (real faulting
    # stores/loads through the guest mapping at guest 0xFEC00xxx against the protected
    # page) and asserts the values read back. Seams are only for state the public
    # register interface cannot reach. Exactly one CTest case is registered.
    add_executable(jsrf_ac97_reset_gate tests/ac97_reset_gate.c)
    target_include_directories(jsrf_ac97_reset_gate PRIVATE
        ${XBOXRECOMP_DIR}/src ${XBOXRECOMP_DIR}/src/kernel
        ${JSRF_GEN_INCLUDE_DIR})
    target_link_libraries(jsrf_ac97_reset_gate PRIVATE xboxrecomp)
    add_test(NAME jsrf_ac97_reset_gate COMMAND jsrf_ac97_reset_gate
        ${CMAKE_CURRENT_SOURCE_DIR}/game/default.xbe)
    set_tests_properties(jsrf_ac97_reset_gate PROPERTIES TIMEOUT 120)

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
    else()
        message(STATUS "jsrf_fence_release_runtime*, jsrf_apu_watch_fixture_*, "
                       "jsrf_collect_arming_gate_on/off, jsrf_a2h_slotw_fixture_*, "
                       "jsrf_ac97_reset_gate and jsrf_native_gpu_warp are not registered off "
                       "Windows -- they need Win32 VEH/page-protection semantics, the Windows "
                       "APU/collector/D3D host paths, or a Direct3D 11 device")
    endif()
    else()
        # Fixtures-only: the pure-Python CTests whose inputs are all tracked.  The rest read the
        # retail XBE, the lifted tree, archived runs or the retail corpus (or, for
        # jsrf_optional_vulkan and jsrf_vsh_aot_provenance, a Windows tool layout), so they are
        # not registered here.  A case that needs an optional module (duckdb) skips itself.
        message(STATUS "Fixtures-only: the full-runtime C++ fixture blocks are not registered -- "
                       "jsrf_fence_release_runtime*, jsrf_apu_watch_fixture_*, "
                       "jsrf_collect_arming_gate_on/off, jsrf_a2h_slotw_fixture_*, "
                       "jsrf_ac97_reset_gate and jsrf_native_gpu_warp -- because fixtures-only "
                       "skips the generated/recovered and Win32 guest-memory/VEH/collector/D3D "
                       "blocks. jsrf_fixtures_only itself is also not registered in this mode: it "
                       "configures a scratch checkout of the shipped tree and needs the Windows "
                       "tool layout, and registering it here would recurse")
        message(STATUS "Fixtures-only: these Python CTests are not registered -- "
                       "key_script_run, fcmov_clamp, pbcov_workdir_helper, nv2a_inventory_guard, "
                       "recovery_span_ownership, stack_depth, hidden_entries, check_dead_gotos, "
                       "tail_thunk_169000, alias_dispatch, generation_provenance, dispatch_table, "
                       "stop_chain, vsh_aot_provenance and optional_vulkan; each reads retail "
                       "XBE/generated/archived inputs or the Windows tool layout")
        set(_jsrf_pytests gpu_inspection logq bench_summary check_graffiti_run
            check_progression_run vsh_aot_table vsh_run_gate save_isolation run_exercised)
        # These two import numpy at module level, so they are only registered where it exists.
        execute_process(COMMAND "${JSRF_PYTHON}" -c "import numpy"
            RESULT_VARIABLE _jsrf_numpy_rc OUTPUT_QUIET ERROR_QUIET)
        if(_jsrf_numpy_rc EQUAL 0)
            list(APPEND _jsrf_pytests text_stability guest_ram_search)
        else()
            message(STATUS "jsrf_text_stability and jsrf_guest_ram_search are not registered -- "
                           "numpy is not importable under ${JSRF_PYTHON}")
        endif()
        foreach(_jsrf_pytest IN LISTS _jsrf_pytests)
            add_test(NAME jsrf_${_jsrf_pytest}
                COMMAND ${JSRF_PYTHON} -X utf8 ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_${_jsrf_pytest}.py)
            set_tests_properties(jsrf_${_jsrf_pytest} PROPERTIES TIMEOUT 120)
        endforeach()
    endif()

    # Several tests build a capture-file path as "%s\\name" from $TEMP, a Windows variable that
    # elsewhere is usually unset.  Point it at a directory under the build tree (trailing slash
    # so the backslash lands in the file name, not in a sibling of the directory).
    if(NOT WIN32)
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-tmp")
    endif()
    get_directory_property(_jsrf_all_tests TESTS)
    foreach(_jsrf_t IN LISTS _jsrf_all_tests)
        if(NOT WIN32 AND _jsrf_t MATCHES "^jsrf_")
            set_property(TEST ${_jsrf_t} APPEND PROPERTY
                ENVIRONMENT "TEMP=${CMAKE_BINARY_DIR}/test-tmp/")
        endif()
        # These families write one shared scratch path under TEMP, so `ctest -j` would let
        # them overwrite each other. The lock must apply on EVERY host: Windows uses the
        # process TEMP directory just as POSIX does, and the previous NOT WIN32 scope left
        # the Windows parallel case unprotected.
        if(_jsrf_t MATCHES "^jsrf_vsh_conformance")
            set_property(TEST ${_jsrf_t} APPEND PROPERTY RESOURCE_LOCK jsrf_vsh_conformance_scratch)
        elseif(_jsrf_t MATCHES "^jsrf_vsh_aot($|_control)")
            set_property(TEST ${_jsrf_t} APPEND PROPERTY RESOURCE_LOCK jsrf_vsh_aot_scratch)
        endif()
    endforeach()
endif()
