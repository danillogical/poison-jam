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

Where a block omits tests, configure prints a grouped `STATUS` naming the family and the reason;
the lists below are the full inventory. A test that is skipped for a known defect stays visible as
debt and is never counted as passing.

**Vulkan device tests on macOS.** Without a software ICD, they run on MoltenVK, pinned with
`VK_DRIVER_FILES` and `RECOMP_VK_DRIVER=MoltenVK`. Two drivers are visible on a Mac, and both are
named after the GPU, so choose with `RECOMP_VK_DRIVER=MoltenVK` or `RECOMP_VK_DRIVER=kosmic`. The
`[GPUVK] device:` line ends with the chosen driver.

## Known gaps on macOS

- **`jsrf_vk_suballoc_device`** skips. The skip's stated causes are two test-oracle defects, not a
  proven allocator leak: the pool claim `free_bytes + used_bytes == reserved_bytes` omits the
  allocator's separately reported `padding_bytes` (the reported 1,792-byte difference — 63,110,912
  vs 63,112,704 — is consistent with padding, but this is an inference and has not been measured on
  a Mac here), and the allocation budget adds dedicated allocations to the allocation-call delta
  while a later assertion already includes them, counting them twice. The skip remains until the
  oracle is repaired and a measured MoltenVK run decides it; read it as explicit debt, not as
  "allocator fine". The backend's accounting code is not changed by this note.
- **`jsrf_vk_pcache_device_hw`** skips: MoltenVK reports no pipeline-cache hits.
- **Not registered:**
  - `jsrf_nv2a_hal`: the NV2A MMIO hook is Windows-only.
  - `jsrf_inplace_event_bridge`: the POSIX `ResetEvent` dereferences a non-event handle.
  - the Direct3D 11 parity tests;
  - the `vsh_aot` tests, which need the retail corpus.
  - `jsrf_save_root`: Win32 save-root path layer and shell32/advapi32.
  - `jsrf_recovery_11c1`: recovered/generated retail seed and Win32 semantics.
  - all `jsrf_pbcov_stream_*` cases (the D4 committed-stream coverage gate): Win32 guest
    memory/window layout, VEH guards and the commit consumer. The real-Vulkan `jsrf_pbcov_vk_*`
    cases remain registered where Vulkan is available.
  - `jsrf_fence_release_runtime*`, `jsrf_apu_watch_fixture_*`,
    `jsrf_collect_arming_gate_on/off`, `jsrf_a2h_slotw_fixture_*`, `jsrf_ac97_reset_gate` and
    `jsrf_native_gpu_warp`: Win32 VEH/page-protection semantics, the Windows APU/collector/D3D
    host paths, or a Direct3D 11 device.
  - in a fixtures-only configure, `jsrf_fixtures_only` itself: it configures a scratch checkout
    of the shipped tree and needs the Windows tool layout; registering it there would recurse.
  - in a fixtures-only configure, these Python tests are omitted because they read the retail
    XBE, generated/archived inputs or a Windows tool layout: `key_script_run`, `fcmov_clamp`,
    `pbcov_workdir_helper`, `nv2a_inventory_guard`, `recovery_span_ownership`, `stack_depth`,
    `hidden_entries`, `check_dead_gotos`, `tail_thunk_169000`, `alias_dispatch`,
    `generation_provenance`, `dispatch_table`, `stop_chain`, `vsh_aot_provenance`,
    `optional_vulkan`.
- **Page size:** `jsrf_nv2a_registers` skips one 4 KiB-page case, because Apple Silicon uses
  16 KiB pages. The subcase prints `SKIP:` and the CTest still reports PASS; it is disclosed debt,
  not executed coverage.

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

**Suballocation note (2026-10-22).** The pool oracle now includes the allocator's separately
reported padding, and the workload gate no longer double-counts dedicated allocations; a
controlled Windows allocator case produces exactly 1,792 bytes of padding and decides both old
forms. This does **not** clear the MoltenVK skip: its dedicated-preference behaviour still defeats
the pooling-budget claim, and no measured MoltenVK run has re-evaluated it. The historical 1,792
attribution remains an inference, and the skip stays explicit debt until a Mac run decides it.
