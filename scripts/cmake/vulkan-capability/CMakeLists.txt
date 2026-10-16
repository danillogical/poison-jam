# The Vulkan-capability probe (plan step R2, contract §C).
#
# `scripts/build-jsrf.py` configures THIS project, in a fresh directory, to ask one
# question: with the controls this build will actually see, what capability does the
# toolkit's `cmake/XbrVulkan.cmake` decide RIGHT NOW?  The answer is read back out of the
# probe's own `CMakeCache.txt` -- `XBR_VULKAN_FOUND`, and `XBR_VULKAN_UNAVAILABLE_REASON`
# when it is FALSE -- so the guard never trusts, and can never be fooled by, the game build
# tree's cached verdict from an earlier configure.
#
# Why a project and not `cmake -P`: `find_package(Vulkan)` is not script-mode-safe
# (CMake's FindVulkan calls `add_library`, which is not scriptable), while a
# `project(... NONE)` tree asks the same question with no compiler and nothing to build.
#
# The toolkit is named by `XBOXRECOMP_DIR`, exactly as the game's configure names it: a
# probe that resolved a different toolkit would answer about different code.  The calling
# guard passes it, along with the controls the game's cache holds (`XBR_DXC`,
# `XBR_SPIRV_VAL`, `XBR_VK_SW_ICD`, `CMAKE_DISABLE_FIND_PACKAGE_Vulkan`, and the loader
# discovery itself -- `Vulkan_LIBRARY`, `Vulkan_INCLUDE_DIR`, `Vulkan_DIR`,
# `CMAKE_PREFIX_PATH` -- so the answer is about the loader that configure will use too).
#
# Nothing here decides anything: the decision, its existence checks and its reason all live
# in the included toolkit file, so this probe cannot drift from the configure it speaks for.
cmake_minimum_required(VERSION 3.20)
project(jsrf_vulkan_capability_probe NONE)

if(NOT XBOXRECOMP_DIR)
    message(FATAL_ERROR
        "this is the game's Vulkan-capability probe and it needs -DXBOXRECOMP_DIR=<toolkit "
        "root>; scripts/build-jsrf.py always passes it, and the answer is only meaningful "
        "for a named toolkit.")
endif()

if(NOT EXISTS "${XBOXRECOMP_DIR}/cmake/XbrVulkan.cmake")
    message(FATAL_ERROR
        "the toolkit '${XBOXRECOMP_DIR}' has no cmake/XbrVulkan.cmake, so it cannot answer "
        "the capability question this probe exists to ask -- it is not the toolkit a "
        "configure of this game would link.")
endif()

include("${XBOXRECOMP_DIR}/cmake/XbrVulkan.cmake")

# The verdict in the log as well as in the cache, so a human reading a build log sees the
# same answer the guard read.
if(XBR_VULKAN_FOUND)
    message(STATUS "XBR capability probe: XBR_VULKAN_FOUND=TRUE (loader ${Vulkan_LIBRARY}; dxc ${XBR_DXC})")
else()
    message(STATUS "XBR capability probe: XBR_VULKAN_FOUND=FALSE -- ${XBR_VULKAN_UNAVAILABLE_REASON}")
endif()
