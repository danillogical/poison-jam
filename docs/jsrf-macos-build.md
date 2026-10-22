# Building and testing on macOS (compile and test only)

macOS arm64 can build the toolkit and the game's portable tests, with the Vulkan back end on
MoltenVK. It cannot run the game: there are no game files on the Mac, and there is no POSIX game
entry point yet. Real runs, the full CTest suite and acceptance stay on Windows.

## Prerequisites

- Xcode command-line tools, CMake, Ninja, `uv`, and Homebrew `sdl2` and `libepoxy`.
- The LunarG Vulkan SDK for macOS. It ships the loader, `dxc`, `spirv-val`, MoltenVK and
  KosmicKrisp. Export `VULKAN_SDK=$HOME/VulkanSDK/<version>/macOS`.

## Toolkit

```sh
cmake -S ../xboxrecomp -B /tmp/xbr-mac-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/xbr-mac-build
ctest --test-dir /tmp/xbr-mac-build -R vk_ --output-on-failure   # vk_smoke_hw, vk_smoke_moltenvk
python3 ../xboxrecomp/tools/posix_check.py native python cross
```

- `posix_check`'s native and Python steps should pass, with only its listed known failure.
- The cross step builds the whole toolkit for Windows with zig MinGW. Two targets beyond its three
  known ones also fail: `nv2a_core.c` and `vm_reuse_test.c`, both because of `__try`. They fail the
  same way without the macOS changes.
- `posix_check` does not run `tests/win32_compat_posix` or `tests/win32_thread_posix`; build those
  separately.

## Game: portable tests only

```sh
cmake -S . -B /tmp/pj-mac -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DJSRF_FIXTURES_ONLY=ON -DXBOXRECOMP_DIR=$HOME/src/xboxrecomp
cmake --build /tmp/pj-mac
ctest --test-dir /tmp/pj-mac -j8 --output-on-failure
```

`JSRF_FIXTURES_ONLY` (default OFF everywhere) needs neither generated code nor the shader corpus:

- it does not build the game executable or the collector;
- it uses the empty vertex-program corpus;
- it registers only the tests that need no game files.

Configure prints each excluded test and the reason.

**Vulkan device tests on macOS.** Without a software ICD, they run on MoltenVK, pinned with
`VK_DRIVER_FILES` and `RECOMP_VK_DRIVER=MoltenVK`. Two drivers are visible on a Mac, and both are
named after the GPU, so choose with `RECOMP_VK_DRIVER=MoltenVK` or `RECOMP_VK_DRIVER=kosmic`. The
`[GPUVK] device:` line ends with the chosen driver.

## Known gaps on macOS

- **`jsrf_vk_suballoc_device`** skips. MoltenVK prefers dedicated allocations, which breaks the
  test's allocation budget, and the same run reported a 1,792-byte readback accounting difference.
  It has not been investigated yet.
- **`jsrf_vk_pcache_device_hw`** skips: MoltenVK reports no pipeline-cache hits.
- **Not registered:**
  - `jsrf_nv2a_hal`: the NV2A MMIO hook is Windows-only.
  - `jsrf_inplace_event_bridge`: the POSIX `ResetEvent` dereferences a non-event handle.
  - the Direct3D 11 parity tests;
  - the `vsh_aot` tests, which need the retail corpus.
- **Page size:** `jsrf_nv2a_registers` skips one 4 KiB-page case, because Apple Silicon uses
  16 KiB pages.

## Running the game on macOS later

The other macOS JSRF port, `andeecollard/jsrf-recomp` (MIT, Metal), shows the run-time techniques
we would still need:

- guest memory placed at an OS-chosen base plus an offset, because `__PAGEZERO` blocks the low 4 GB;
- `mach_vm_map(VM_FLAGS_FIXED)` before every fixed mapping;
- discipline around 16 KiB host pages;
- SIGSEGV/SIGBUS MMIO traps that decode the faulting AArch64 load or store
  (`mmio_decode_a64.h` already exists in our toolkit);
- the guest on the main thread, with SDL events pumped from kernel waits;
- `-O2 -fwrapv -fno-strict-aliasing` for the generated code.

Here, that also means:

- a POSIX game `main`;
- arm64 MMIO models for NV2A and AC'97;
- a POSIX save root;
- a replacement for the Windows-only collector.
