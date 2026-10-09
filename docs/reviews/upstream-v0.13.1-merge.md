# Upstream v0.13.1 merge — inventory and resolution

**Toolkit merge commit `409c635`** (parents: fork `fafe0f6`, upstream `193e299` = v0.13.1 "Cut Short",
which includes v0.13.0 "Given Back"); merge-base `ea60cfa`; 100 upstream commits. Governing rules:
`docs/jsrf-run-profiles.md` §"Upstream merges never silently change admitted evidence semantics".
Summary for the technical record: TR §1 "v0.13.1 sync".

## Owner decisions

1. **The merge lands on main**, not a branch (2026-10-08). It is one commit, revertable as a unit with
   `git revert -m 1 409c635`; the review fixes landed before it (toolkit `fafe0f6`, game `51bc1ff`) and
   these records after it.
2. **Adopt upstream's executor now** (2026-10-08). The inventory below reached its stop condition —
   the two `src/kernel/nv2a_pb_exec.c` files are separate rewrites of one base and cannot be merged hunk
   by hunk — and recommended keeping the fork's executor and porting the renderer later (R1–R5). The
   owner chose instead to take upstream's executor as the base and port the fork's integration into it.

## Resolution as applied (differences from the inventory are marked)

**Executor — upstream base, fork integration ported** (overrides inventory §3 row 1 and §4's
recommendation). Kept from upstream: vertex programs (`nv2a_vsh_interp.c`, `RECOMP_NO_VSH`), register
combiners (`nv2a_combiner.c`, `RECOMP_NO_COMBINERS`), depth, near-plane clipping, four texture stages,
TENT filtering, P8 palettes, all immediate attributes, threaded raster (`RECOMP_RASTER_THREADS`; workers
take no runtime locks, but the walk thread still waits on them while holding the owner lock), frame
trace, flip dumps, window frame stats. Ported from the fork: the owner commit consumer (NV097 only),
the non-NV097 class census, `report_tick` as the only report caller under the owner; the present tracker
fed at all four draw sites (screen-space/fixed-function, program path, both back-end batches), at colour
clears and at every batch end, and the fork's FLIP_STALL publication — upstream's "present the biggest
surface drawn" rule is **not taken**; the L47 flip trace from every batch kind; the L48 stage-0
`CONTROL0` gate (upstream had none) with `c[58]`/`c[59]` written in one place; `nv2a_pb_exec_counters`
and `nv2a_pb_exec_vp_view` backed by upstream's constant storage, with program/constant uploads
addressed per component as the fork and xemu do (upstream counted words, which the game's
`tests/test_nv2a_hal.c` "ordered" test rejects — shown by mutation); the fixed-function, lighting, AA and
back-end paths upstream lacks; the fork's 32-bit indices and 65,536-entry batches. Every NV097 method the
fork executor handled is handled (checked by sending `0x0000–0x1FFC` to both executors). `GET_REPORT` is
counted, never written. The fork's VP interpreter and `RECOMP_VP` are retired. `nv2a_pb_scan.c` stays
the fork's.

**Every other conflicted file — inventory §3 applied**, with these deviations, each from reading the
code: `kernel_hal.c` keeps `xbox_memory_layout.h` (the adopted holder code needs `RECOMP_TLS`);
`tests/kernel_irql_tracking_test.c` defines `g_esp`/`g_xbox_mem_offset` for the adopted holder code; a
second self-recursive `heap_alloc_locked` (a clean hunk the inventory missed) removed; the whole fork
`test_lifter_direction_flag.py` kept (upstream's clean hunks asserted its `rep movs` text); the fork's
`tests/apu_mixdown` kept; `xbox_HeapReclaimEnabled` declaration removed; `test_flag_join.py` keeps LF
endings; the lifter adopts upstream's `_analyze_switch_table` fallback but retries from slot one only
under the fork's rule (`79a0070`), so both sides' tests pass; two kernel test fixtures gained window
stubs to link. The toolkit README carries a note that the four dropped switch families and #159 are not
in this fork.

**Upstream switches classified** (`docs/jsrf-run-profiles.md`): observation — `RECOMP_FB_DUMP_FLIPS`,
`RECOMP_FRAME_TRACE`, `RECOMP_FRAME_TRACE_METHODS`, `RECOMP_VSH_TRACE`, `RECOMP_VSH_DUMP`,
`RECOMP_PB_REPORT_MS`, `RECOMP_USB_STATS`; feature enablement — `RECOMP_NO_VSH`, `RECOMP_NO_COMBINERS`,
`RECOMP_RASTER_THREADS`, `RECOMP_USB_PADS`; exploratory — `RECOMP_PAD_SCRIPT`, `RECOMP_PAD_LIVE`.
Dropped with their code: `RECOMP_EXT_VMA`, `RECOMP_GUEST_LOCK`, `RECOMP_HEAP_RECLAIM`,
`RECOMP_TITLE_KEVENTS`.

## Gate measured on macOS, and the gate owed on Windows

On macOS (no game assets): `tools/posix_check.py` native 12 suites PASS, cross PASS (the whole tree
compiles and links for Windows; only the 3 known `d3d8_smoke` failures), python 714 passed + the known
failure; `scripts/check-merge-structure.py --rev 409c635`: 130 files, 152 switches, 0 findings; the
rule-3 scan over `src/`/`include/`: no `getenv` of `RECOMP_APU_DSP_ACK`/`RECOMP_AC97_READY`/
`RECOMP_VBLANK`, no call of `mcpx_apu_dsp_ack_poll` or `ac97_arm_write_trap`;
`scripts/check-override-drift.py`: no findings. Upstream's `nv2a_vsh` and `nv2a_combiner` unit tests and
the game's `test_nv2a_hal.c` consumer/counter/clear/ordered contracts were built and run natively by
hand (the HAL PCI contract fails identically before and after the merge on macOS).

**Owed on Windows** (the W7 chore gate): toolkit and game build, both CTests (including `kernel_regressions`,
`nv2a_vsh`, `nv2a_combiner`, `fp_precision`, `jsrf_nv2a_hal`), `just check`, and an exploratory
`just title-run` A/B of `409c635` against `fafe0f6` — draws/triangles/`batches_ffp`, `[TEXUSE]`, the
`[FBPRESENT]` reason mix, flips == presents, owner-lock maximum hold and late re-arms, `[HEAP] free #1`
passing, the APU trap-range line and `[APUWAIT]` 3→0 — with the six risks in §6 below. The
inventory's renderer packets R1–R5 are superseded by the port; its risk list still applies.

---

## Inventory (written before resolution)

The scratch evidence it cites under `/tmp/xbr-review/` (diff3 copies, clean-hunk diffs, drift outputs)
was not kept; every claim below names the commit, file and line it was read from.

---

## 0. Headline findings

1. **The executor cannot be resolved hunk by hunk** (OBSERVED). The fork's `nv2a_pb_exec.c` comes from
   the BearddOddity PR-b lineage (`a253876`, `27191d1`, which upstream never merged) plus the fork's evidence
   instrumentation. Upstream's comes from sp00nz's own rewrite (`5dc1e34`..`59fd56a`). Both rewrote the
   same base file (fork +1665/-82, upstream +1517/-163). Beyond the 17 conflicts, **45 clean hunks
   (652 changed lines)** already rewrote fork-owned code to upstream's data model: `s_gpu.tex` became
   `s_gpu.texs[0]`, the fork's `case NV097_SET_TEXTURE_*` handlers were deleted, and blending, `sample_texture`,
   immediate mode and the BMP dump were replaced (`clean/src_kernel_nv2a_pb_exec.c.cleandiff`). Taking
   the fork side in every conflict therefore does not give the fork's executor back.
2. **Clean upstream `case` labels silently shadow fork handlers** (OBSERVED, `nv2a_pb_exec.c:4738-4759`,
   `:4401`). `case 0x1E94/0x1E98/0x1E9C/0x1EA0/0x1EA4` pre-empt the fork's default-branch
   `NV097_SET_TRANSFORM_EXEC_MODE` and `vp_method` (program load/start, constant load). `case
   0x030C/0x0354/0x035C/0x0214/0x1D8C/0x0300/0x033C/0x0340/0x0358/0x034C/0x0350` pre-empt the fork's
   `capture_render_state`. At `:4401`, `tex_stage_method` takes every `0x1B00-0x1BFC` method before the switch.
   `check-merge-structure.py`'s dup-case test cannot see any of this, because the fork handles these
   methods in `default:`. The game's own test `tests/test_nv2a_hal.c:566-602` ("ordered", which sends
   `CONSTANT_LOAD 0x1EA4` through the walk) would fail.
3. **Clean hunks introduce a guaranteed self-deadlock in the heap** (OBSERVED). Upstream's lock is
   inserted *inside* the fork's `heap_block_size_locked` (`xbox_memory_layout.c:6299`) and
   `heap_free_locked` (`:6330`). Their callers already hold `g_heap_lock` (`HEAD:xbox_memory_layout.c:5964,5993`).
   SRW locks are not recursive, so the first `xbox_HeapFree` hangs with no error. There is also a second
   `static SRWLOCK g_heap_lock` (`:5610` and `:5630`).
4. **Rule-3 scan** (OBSERVED, SCOPE = `src/`, `include/`):
   - `RECOMP_APU_DSP_ACK`: `getenv` at `src/apu/apu_dsp.c:61` (that file is deleted in the fork but its
     upstream content is present in the tree). **Its arming call site `mcpx_apu_dsp_ack_poll(d)` at
     `src/apu/apu_core.c:659` merged CLEAN**, and its declaration is in `apu_state.h:489` (upstream side).
     This is **FAIL as merged**. Even with `apu_dsp.c` kept deleted, the call stays. The obvious link fix,
     restoring the file, reintroduces the deleted override.
   - `RECOMP_AC97_READY`: `xbox_memory_layout.c:3070`, plus the `ac97_arm_write_trap()` arming call, both
     on the upstream side of conflict `2987-3117` only. **FAIL if that side is taken**, PASS on the fork
     side. `HEAD:templates/new-game/src/main.c:362` is an unbuilt template that already exists in the
     fork: inventoried, not failed. `src/apu/GP-INTEGRATION.md:31` is a doc mention: inventoried.
   - `RECOMP_VBLANK`: no hit anywhere in `src/` of the merge tree. **PASS.** Upstream still reads it at
     `upstream/main:src/kernel/kernel_bridge.c:2261`, but that region merged to the fork's deletion.
5. **A clean hunk silently changes `RECOMP_APU_TRAP`** (rule 1/4, OBSERVED, `xbox_memory_layout.c:3093`):
   `VirtualProtect(g_mcpx_memory, APU_TRAP_BYTES, ...)`. `APU_TRAP_BYTES` (0x30000) is defined only on the
   upstream side (`:3090`). If someone "fixes" the compile error by defining it, the GP/EP DSP memory
   (0x30000-0x7FFFF) is left as plain RAM. That bypasses the pinned GP DSP model (L21) that title runs
   (`just title-run` sets `RECOMP_APU_TRAP`) depend on.
6. **Upstream's executor and the fork's owner commit consumer can coexist at the interface**: same entry
   point, and upstream does not touch `src/nv2a/` except one `qemu_shim.h` guard. They **cannot be
   merged as files**. A redesign of the consumer is not needed. Adopting the renderer *inside this merge*
   is the stop condition (section 4).

---

## 1. Inventory by area (every upstream commit; merge commits only where they carry content)

### A. APU / audio (title path: `just title-run` sets `RECOMP_APU_TRAP=1`, `justfile:79`)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `bdfd497` | Under `RECOMP_AC97_READY`, traps only APU 0x00000-0x2FFFF and leaves GP/EP DSP memory as RAM | conflict `xbox_memory_layout.c:2987-3117`; **the `VirtualProtect(..., APU_TRAP_BYTES` line at `:3093` is CLEAN** | **drop** (restore `0x00080000u` at `:3093`) | rule 1/3: `AC97_READY` is deleted; the fork routes 0x30000-0x3FFFF to the pinned `gp_ops` (`HEAD:src/apu/apu_core.c` `mcpx_apu_dispatch_mmio`), so untrapping bypasses the GP model. **Flag: clean hunk, rule 1/4.** |
| `57a7ffd` | Moves the `RECOMP_APU_DSP_ACK` doorbell poll out of `se_frame` into the frame-thread loop | `apu_dsp.c` DU; `apu_state.h:454-490` conflict; **`apu_core.c:659` call CLEAN** | **drop** (keep `apu_dsp.c` deleted, delete the call at `:659` and the declaration) | rule 3: arming call site of a deleted synthetic-completion override. **FAIL as merged.** |
| `591dcb9` | `mcpx_apu_phys()` resolves APU DMA physical addresses into the contiguous window; `update_irq` raises `s_irq_line`; `apu_deliver_irq` calls the guest ISR (vector 5) from the frame thread | `apu_shim.h` 7 conflicts, `apu_vp.c:846-855` conflict; **`apu_core.c:84-185,683-687` CLEAN** | address translation: **local wins** (`apu_phys_ptr`, fork `1c6641a`, ledger D2/L22). IRQ delivery: **drop for this merge** (record as an audio-packet candidate) | rule 2: new device behaviour (an interrupt delivered to DirectSound's ISR) that would be **armed** in every `RECOMP_APU_TRAP` run. **Flag: clean hunk, rule 2/4.** |
| `1041f71` (APU part) | Frame thread drops `d->lock` and calls `SwitchToThread()` once per frame (`apu_core.c:698-701`); `voice_process` breaks after 5 empty resamples (`apu_vp.c:1061`) | CLEAN | `SwitchToThread`: **drop for this merge** (scheduling change on the title path, interacts with GP DSP timing; INFERRED). Underrun break: **adopt** | the break only shortens what would otherwise be an endless spin under `d->lock` (INFERRED: no completing run can depend on the spin) |
| `7985053` | `tests/apu_mixdown` links the whole runtime (for IRQ delivery) and stubs `recomp_lookup` | CLEAN | **drop** (keep the fork's `xbox_apu`-only test, `5d3466b`) | follows from dropping IRQ delivery |

### B. NV2A executor, `pb_scan`, present path (ledger L18 translated rendering, L47, L48)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `31efd26` | P8 textures sampled through the stage palette | clean hunks in `sample_tex` | **adopt** in renderer packet R1 | L18 translation |
| `5dc1e34` | Vertex programs through new `nv2a_vsh_interp.c`; depth buffer; `raster_batch_program` takes every `xf_mode==2` batch unless `RECOMP_NO_VSH` | new files clean (compiled: `src/kernel/CMakeLists.txt:34-35`); executor conflicts `2707-3566`, `3578-3590` | files: **adopt dormant** (compiled plus their unit tests, not called). Executor wiring: renderer packet R3 | it replaces the fork's `vp_run` path (RECOMP_VP, L48) for every VP batch; needs A/B |
| `fcea264` | "Present the biggest surface drawn" (`note_drawn`, `drawn_*`, `drawn_stale`) | conflicts `2353-2358`, `4762-4848`; clean struct fields | **drop**; record as an alternative not taken | the fork's present tracker (`nv2a_present_track.h`) plus FLIP_STALL publication is the evidenced path (plan: 324/370 flips `drawn_this_frame`) |
| `0556e46` | VSH: a paired ILU op always writes R1 | new file, clean | **adopt dormant** (with the file) | — |
| `0e9cff9` | Viewport constants c[58]/c[59] (same as fork L48), GL blend factors, draw diagnostics | clean (`blend_factor`, `put_pixel`) and conflicts | blending: R1; constants: keep fork L48 until R3 | the fork already writes c[58]/c[59] (`d690d54`) |
| `7a9cf46` | `pb_scan` follows CALL to RETURN | conflicts `nv2a_pb_scan.c` x4, clean 29 lines | **local wins** | the fork's DMA-engine walker (`f9258cd`) already follows JUMP/CALL/RETURN; scan execution is suppressed under the owner (`scan_exec_suppressed`) |
| `a25ce28` | `GET_REPORT` (`0x17D0`) **writes a 16-byte report into guest memory**; `CLEAR_REPORT_VALUE`, `ZPASS` count; `SET_COLOR_MASK` | **CLEAN** (`nv2a_pb_exec.c:4468-4489`) | colour mask: R1. **GET_REPORT write: adopt dormant** (count only, never write guest memory) | rule 2: new device behaviour the guest consumes (`GetVisibilityTestResult`). `0x17C8-0x17D0` are not in `nv2a_method_table.c`, so they are reachable only under `RECOMP_NV2A_ADMIT_UNKNOWN`, whose L44 contract says admitted methods are "not executed". **Flag: clean hunk, rule 2/4.** |
| `bf24dfe`, `5c3a42c` (trace part) | `RECOMP_FRAME_TRACE` frame trace (flag-file re-armable) | clean plus conflicts | R5 (observation) | — |
| `13c64d3` | Four texture stages plus register combiners (`nv2a_combiner.c`), fog, alpha test | new files clean; executor clean plus conflicts | files: **adopt dormant**; wiring: R2 | upstream's `tex_update_valid` has **no `SET_TEXTURE_CONTROL0` enable test**. The fork's stage-0 gate (`d690d54`, L48) must be ported or JSRF batches sample the surface they draw into |
| `5c3a42c` | All 16 immediate-mode attributes | clean (`imm_*`) | R1 | — |
| `c951e0c` | TENT bilinear, sign-aware wrap | clean | R1 | — |
| `aa95044` | Near-plane clipping, **threaded rasteriser** (`RECOMP_RASTER_THREADS`, default `nprocs-4`), hot-path fixes | executor plus combiner, conflicts plus clean | R3 (clipping) / R4 (threads) | the walk thread waits `INFINITE` on raster workers while holding the owner lock (`upstream/main:nv2a_pb_exec.c:1954-1980`) |
| `777c1cd` | `surface_bpp` from the surface format | **CLEAN** (`nv2a_pb_exec.c:1034-1060`) | R1 | changes which batches are refused (L18). **Flag: clean renderer change.** |
| `59fd56a` | `RECOMP_FB_DUMP_FLIPS` dumps every flip | conflict `4762-4848` | R5 (observation) | — |
| `331d95b` | Window title shows the XBE title, FPS and draws (`fb_present.c`, `xbox_memory_layout.c:2536-2549`, `nv2a_pb_exec.c` FLIP_STALL `FrameStats`) | conflicts `fb_present.c:525-543,574-580`; rest CLEAN | **adopt** (union in `fb_present.c`) | host window only; writes no guest state |
| `447a7fc` | `qemu_shim.h`: no `__builtin_ctz` redefinition under clang-cl | CLEAN | **adopt** | build only; the L53 clock code in the same header is untouched (OBSERVED) |

### C. Memory / allocators (ledger L09)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `9547d39` | Span reservation of base plus 28 mirrors becomes POSIX-only (`#ifndef _WIN32`, `xbox_memory_layout.c:2380`) | CLEAN | **adopt** | host-side Win32 bug (partial `MEM_RELEASE` fails). Changes the host layout, not guest VAs. A/B item 9 |
| `423263b` | Contiguous arena starts at `XBOX_CONTIG_BASE + 0x1000` | conflict `5827-5885` | **local wins** for this merge; defer to the input packet (M16) | shifting the arena moves every contiguous VA; JSRF records key on `0x80001000` (ring) and `0x80084000` (surface). INFERRED: the XPP USB arena collision it fixes is real once `RECOMP_USB` is used |
| `700cf1e`, `843337f` | `RECOMP_HEAP_RECLAIM`: split-on-reuse, coalesce past empty slots, `NtFreeVirtualMemory` returns heap blocks, decommit zeroes, contiguous free/reuse, `MmQueryAllocationSize` | conflicts `5893-6382`, `kernel_bridge.c:954-967,1294-1359,5115-5124`; **clean: `xbox_HeapReclaimEnabled` (`:5640`), `heap_carve_free`, `int reclaim` (`:5890`), heap locks (`:6299,:6330`), second `g_heap_lock` (`:5630`)** | **drop entirely** (challenge to the orchestrator default) | the fork's always-on `kmem` already does all of it: `kmem_heap_reuse` with split, `kmem_heap_free` (whose comment names the empty-slot bug), `kmem_arena_*` for contiguous memory, `xbox_VmFree`, and `vm_zero`/`pages_zeroed` in `kernel_vm.c:81`. Both sides define `g_contig_blocks` with different types (`:5836` vs `:5870`), and `xbox_ContiguousFree`/`BlockSize` twice (`:5969/:6012`, `:5990/:6045`). Keeping upstream's version behind its switch would mean a second allocator over the same tables. Its tests assert the opposite of the fork's semantics (see I). **Clean deadlock: risk 3.** |
| `39f2b8a` | `RECOMP_EXT_VMA`: `guest_vmem.c` tracks pages above the mirrors; `NtAllocate/Query/FreeVirtualMemory` hooks; free-region size to the next boundary | new files clean, CMake conflict `24-29`; **`kernel_bridge.c:1005,1229,1260` CLEAN**, `:1325` conflict; **`xbox_memory_layout.c:3214-3220,5466` CLEAN** | **drop** (or adopt dormant, classified exploratory, only if a JSRF need appears) | duplicates the fork's region registry (`kernel_vm.c`, L09) for a range JSRF has not been shown to use (INFERRED) |

### D. Scheduling, IRQL, DPC, events (ledger L11, L34, L35, L36)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `2111207` | `RECOMP_GUEST_LOCK`: global guest-CPU critical section, released around every kernel call | conflict `kernel_bridge.c:586-593`; **CLEAN: dispatch wrapper `:10420-10500`, `guest_cpu_join()` at `:10843`, decls `:558-559`** | **drop** | a second serialiser beside `RECOMP_GUEST_SERIAL` (L34). Both wrap `kernel_thunk_dispatch`; set together they would nest two locks (INFERRED deadlock risk). If kept: exploratory |
| `1041f71` (IRQL part), `32a67a3` | ISR at IRQL 16 and DPC at 2 around host-run routines (`xbox_IrqlEnterInterrupt/Leave`); **publishes `KPCR.Irql` (fs:[0x24]) on every IRQL change**; IRQL holders carry guest ra/esp | brackets in conflicts (`kernel_bridge.c:2485-2491,2788-2796`, `ohci.c:900-909`, `kernel.h:864-871`); **CLEAN: `kernel_hal.c:193` `irql_publish`, calls at `:374,:384,:404,:441,:453`, `apu_core.c:182`** | holder ra/esp: **adopt** (observation). `irql_publish` plus brackets: **adopt dormant / defer** to a single-variable IRQL packet | a guest-visible kernel-state write (unconditional) that changes ledger rows L11/L36. It also interacts with the fork's serial-mode gate `xbox_IrqlBlocksDeviceInterrupts`, and the fork's `xbox_KeRaiseIrqlToSynchLevel` (`kernel_hal.c:461`) would not publish, which is inconsistent. Possibly valuable for DirectSound (upstream measured its lock reading fs:[0x24]; INFERRED to apply to JSRF's XDK) |
| `1dc92c8` | DPC queue under a `CRITICAL_SECTION`; `KDPC.Inserted` is the truth; drain follows the live tail | conflicts `kernel_bridge.c:2541-2925`; **CLEAN second `bridge_KeRemoveQueueDpc` at `:2737`** | **local wins**; delete `:2737` | fork `e2872a1`, plus the snapshot drain. Following the live tail spins forever on a self-requeueing graphics DPC and starves vblank (the fork's own comment, conflict `2882-2906`) |
| `8b8fcee`, `882378e` | `RECOMP_TITLE_KEVENTS`: guest-built KEVENTs get host events (`ke_guest_event`) | conflicts `1760-1896`, `1913-1963`, `7937-7964`; **CLEAN: `:1708` decl, `:7149` body, `KePulseEvent` `:7877`** | **drop** | the fork's always-on in-place KEVENT bridge (`guest_va_is_inplace_kevent`, `c98dbdc`) covers the same objects. Two models of one object would fight. If kept: exploratory |
| `30b66ef` | `[EXIT]` lines on stderr for `HalReturnToFirmware` and bug checks | CLEAN (`kernel_hal.c`) | **adopt** | observation |

### E. Input / USB (ledger L27/L28; not on the title path; M16 open)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `bd87c4f` | OHCI resolves physical addresses through the contiguous window | CLEAN | **adopt** (behind `RECOMP_USB`) | device model fix, off by default |
| `1041f71` (OHCI part), `e0c1973` | One done queue per service pass; `RECOMP_USB_STATS` 5 s line | CLEAN (`ohci.c` 222 lines); ISR bracket conflict `900-909` | **adopt**; at `900-909` keep the fork's `xbox_GuestSerialBeginAtomic` | — |
| `967f379`, `68b3db9`, `bf0d02d`, `278d88a` | `RECOMP_PAD_SCRIPT` / `RECOMP_PAD_LIVE` scripted presses and sticks; rumble no longer ends a control transfer | CLEAN (`usb_gamepad.c`) | **adopt**, scripts classified exploratory | synthetic input, same class as `RECOMP_PAD_PRESS` |
| `e66cba2` | Up to four pads (`RECOMP_USB_PADS`) | CLEAN | **adopt** | feature enablement; composes with `RECOMP_USB_PORT` (`ohci.c:1067-1095`, OBSERVED) |

### F. Files and paths (L08)

| Commit | What it does | Merge state | Disposition | Rule / reason |
|---|---|---|---|---|
| `a4e0f36` | Release directory searches on `NtClose` (`xbox_dir_context_drop`) | conflicts `kernel_file.c:320-324`, `kernel_bridge.c:811-820`; **CLEAN definition `kernel_file.c:666`, decl `kernel.h:688-690`** | **local wins** (fork `a826201` `xbox_dir_context_release`); delete the clean def and decl | duplicate fix |
| `1f93052` | Save directory made absolute before `SHCreateDirectoryExW` (`kernel_path.c:316`) | CLEAN | **adopt** | only changes a run whose save dir was relative and did not exist (INFERRED) |
| `cc86935`, `bc2fac4` | `g_xbox_path_hook` (NULL by default) | CLEAN | **adopt** | observation hook |

### G. Lifter / translator / disasm / templates: see section 5. Summary dispositions

`21d9ce7`, `f4e3186`, `c576d24` (switch tables): **local wins** in the translator conflict `1467-1483`,
and keep the lifter consistent with it. `64796df`, `5d507e7` (x87 PC=24): **adopt**, regen-gated.
`894ceef`, `a006905`, `206b733` (lahf, unordered compares, SSE compares, single cmps/scas): **adopt**,
union in the `_flags` declaration. `fc54a43` (float compares join): **adopt**. `57f47ff` + `64bd733`
(#159 per-edge joins) and `f4e7f6e` (its tests): **drop** (duplicate of fork `ca4257c`). `4c08bee`:
**adopt** (union at `521-566`). `1409a7d`, `2dd0287`, `1446779`, `ca86c0f`: **adopt**. `1ba591e`:
**adopt** (union at `436-442`). `7f1b263`: **adopt**. `d88cbea`: **adopt**. `5e1d2ed`: **local wins**
(fork `5364747`). `85d4f30`: **adopt**. `7f47071`, `3791259`, `f2f5bcf`: **adopt**, game-contract check
needed. `4e737de`, `e5043b3`, `368b16d`: **adopt**, high churn for JSRF. `c0029d0` (compile-time
`RECOMP_CALL_PROFILE`): **adopt** (inert unless defined).

### H. Platform / build
`9b310cc`, `248e692` (GCC/Linux `win32_compat`, `xbox_winnt.h`): **adopt**. No `_WIN32` effect
(OBSERVED: `__linux__` guards). INFERRED: re-run the fork's macOS `tools/posix_check.py`, because
`xbox_winnt.h` moved `PCONTEXT` under `#ifdef __linux__ ... #else`.

### I. Tests added or changed by upstream

| Test | Disposition | Reason |
|---|---|---|
| `tests/kernel_regressions` (`192e118`) | **adopt** | checks fixes the fork already has (`HEAD:kernel_bridge.c:3139` 0xC0000018->487, pseudo-handles at `:3267`) |
| `tests/memory_regressions` | **drop** | its default mode asserts "reuse hands a freed block over whole" and "contiguous memory is never reused" (`test_main.c:244,315`). The fork's kmem does the opposite by design |
| `tests/kernel_events` | **drop** | tests `RECOMP_TITLE_KEVENTS`; its default mode asserts that waits on guest-built events fail |
| `tests/nv2a_vsh`, `tests/nv2a_combiner`, `tests/fp_precision` | **adopt** | unit tests of adopted or dormant code |
| `tools/recomp/test_flag_join.py` (clean) | **edit**: drop the `_jf_`/`_edge_flag_plan` asserts (`:81-103`) | #159 is dropped |
| `tools/recomp/test_rep_movs_mmio.py` (new) | **drop or rewrite** to the fork's `recomp_range_is_mmio` form | fork `5364747` wins |
| other new `tools/recomp|disasm/test_*.py` | **adopt** with their features | — |

### J. Docs
`afeb69f`, `b3700e1`, `193e299` (README / CONTRIBUTORS): **adopt** upstream text; keep fork-specific
README sections. Docs only.

### 1b. Clean hunks that silently change evidence semantics or liveness (rule 1/4)

| Where (merge tree) | Effect if left | Action |
|---|---|---|
| `xbox_memory_layout.c:3093` `APU_TRAP_BYTES` | redefines what `RECOMP_APU_TRAP` traps (L21-L24) | restore `0x00080000u` |
| `apu_core.c:659` `mcpx_apu_dsp_ack_poll(d)` | arming call of the deleted `RECOMP_APU_DSP_ACK` (**rule 3 FAIL**) | delete |
| `apu_core.c:84-185,683-687` IRQ delivery | arms APU interrupts in every trapped run (rule 2) | delete (dormant) |
| `apu_core.c:698-701` `SwitchToThread` | APU frame-thread scheduling on the title path | delete for this merge |
| `nv2a_pb_exec.c:4468-4489` `GET_REPORT` | guest-memory write (rule 2; L44 "not executed") | not ported |
| `nv2a_pb_exec.c:4401`, `:4738-4759` | shadow the fork's texture, VP and render-state handlers | gone with `--ours` |
| `nv2a_pb_exec.c` 45 clean hunks | renderer rewrite (L18) mixed into fork code | gone with `--ours` |
| `kernel_hal.c:193,374,384,404,441,453` | unconditional `KPCR.Irql` writes (L36) | delete for this merge |
| `kernel_bridge.c:10420-10500,10843` | guest-lock wrapper on every kernel call | delete |
| `kernel_bridge.c:1005,1229,1260` | EXT_VMA hooks (inert when off) | delete with EXT_VMA |
| `kernel_bridge.c:7149,7877`, `:2737` | title-kevent model; duplicate DPC cancel | delete |
| `xbox_memory_layout.c:6299,6330`, `:5630` | **self-deadlock**; duplicate lock | delete |
| `xbox_memory_layout.c:5640-5700,5890` | HEAP_RECLAIM scaffolding | delete |
| `translator.py` clean `@@-2481/-2493` hunks (block_lines/edge_sets) | dangling #159 pieces | delete |
| `lifter.py` clean `_analyze_switch_table` (`@@-2729/-2747`) | diverges from the translator's table rule (fork side) | revert or adopt together, measured at regen |

---

## 2. Environment switches upstream reads that the fork does not

All 17 come from the drift-check output (OBSERVED). A full quoted-literal diff of `src/` found no others
except `RECOMP_APU_DSP_ACK`, `RECOMP_VBLANK` and `RECOMP_PB_WRAP_TRACE`, which the fork deleted and which
are not read in the merge tree's `src/` apart from `apu_dsp.c`. `RECOMP_ICALL_SAFE_*` and
`RECOMP_CALL_PROFILE` are code macros, not environment variables.

| Name | upstream/main file:line (merge tree) | What it does | Default | Proposed class | Survives the recommended resolution? |
|---|---|---|---|---|---|
| `RECOMP_EXT_VMA` | `guest_vmem.c:72` (same) | page tracker above the mirrors; changes NtAllocate/Query/Free answers | off | **exploratory** | no (dropped) |
| `RECOMP_FB_DUMP_FLIPS` | `nv2a_pb_exec.c:3136` (`:4711`) | BMP per flip (`1`, or while a flag file exists); suppresses report/after-draw dumps | off | **observation** | only after R5 |
| `RECOMP_FRAME_TRACE` | `nv2a_pb_exec.c:2384` (`:3729`) | flag-file-armed per-frame `[FTRACE]` | off | **observation** | after R5 |
| `RECOMP_FRAME_TRACE_METHODS` | `nv2a_pb_exec.c:2902` (`:4395`) | every method of the traced frame | off | **observation** | after R5 |
| `RECOMP_GUEST_LOCK` | `kernel_bridge.c:9365` (`:10453`) | one guest thread at a time, released per kernel call | off (`=1`) | **exploratory** (scheduling, like `GUEST_SERIAL`) | no (dropped) |
| `RECOMP_HEAP_RECLAIM` | `xbox_memory_layout.c:2873` (`:5644`) | alternative allocator semantics | off | **exploratory** | no (dropped) |
| `RECOMP_NO_COMBINERS` | `nv2a_pb_exec.c:2000` (`:3290`) | disables combiners; **combiners run by default once the title programs them** (`use_rc = rc_seen && !no_rc`) | combiners ON | **feature enablement** (negative switch; L18) | after R2 |
| `RECOMP_NO_VSH` | `nv2a_pb_exec.c:2287` (`:3584`) | disables vertex programs; **VSH runs by default for every `xf_mode==2` batch** | VSH ON | **feature enablement** (L18). Would supersede the fork's `RECOMP_VP` (documented feature switch), which needs a doc and classifier change | after R3 |
| `RECOMP_PAD_LIVE` | `usb_gamepad.c:461` (same) | presses appended to a file while running | off | **exploratory** (synthetic input, `PAD_PRESS` class) | yes, needs classifying |
| `RECOMP_PAD_SCRIPT` | `usb_gamepad.c:411` (same) | timed button/stick script | off | **exploratory** | yes, needs classifying |
| `RECOMP_PB_REPORT_MS` | `xbox_memory_layout.c:997` (`:1220`) | cadence of the ack-thread `nv2a_pb_scan_report` (floor 100 ms) | 10000 | **observation**. Under the owner the executor report stays consumer-only (`HEAD:nv2a_pb_scan.c:184-198`) | yes, needs classifying |
| `RECOMP_RASTER_THREADS` | `nv2a_pb_exec.c:1933` (`:3223`) | raster worker count | **ON**: `nprocs-4` threads (1 = off) | **feature enablement** (performance). Changes owner-lock hold time and pacing, not pixel results (rows are disjoint; INFERRED) | after R4 |
| `RECOMP_TITLE_KEVENTS` | `kernel_bridge.c:6220` (`:7156`) | host events for guest-built KEVENTs; changes wait semantics | off (`=1`) | **exploratory** | no (dropped) |
| `RECOMP_USB_PADS` | `ohci.c:1266` (`:1276`) | 1-4 emulated pads | 1 | **feature enablement** (like `RECOMP_USB_NDP`) | yes, needs classifying |
| `RECOMP_USB_STATS` | `ohci.c:796` (`:797`) | 5 s OHCI stats line | off | **observation** | yes, needs classifying |
| `RECOMP_VSH_DUMP` | `nv2a_vsh_interp.c:319` (same) | dumps loaded programs | off | **observation** | **yes even while dormant** (the file is compiled) |
| `RECOMP_VSH_TRACE` | `nv2a_pb_exec.c:2201` (`:3491`) | n traced program batches | off | **observation** | after R3 |

Other upstream behaviour that is ON by default with no switch (OBSERVED): "present the biggest surface"
(`fcea264`); `GET_REPORT` writes (`a25ce28`); APU IRQ delivery under `RECOMP_APU_TRAP`; the doorbell poll
call; KPCR.Irql publication and ISR/DPC IRQL brackets; `KDPC.Inserted` as truth; the contiguous arena
skipping page 0; the POSIX-only span (always on Windows); the save dir made absolute.

---

## 3. Resolution policy per conflicted file

The general rule: the fork's structure wins everywhere. Upstream pieces are ported only where listed.
After resolving, run `check-merge-structure.py`, `check-override-drift.py` and the rule-3 scan over
SCOPE **before** building.

| File | Hunks (merge tree) | Winning structure | Port from upstream | Clean hunks to remove |
|---|---|---|---|---|
| `src/kernel/nv2a_pb_exec.c` | 17 | **Whole file from the fork: `git checkout --ours`** (not hunk by hunk, see headline 1) | nothing in this merge | all 45 (they disappear with `--ours`) |
| `src/kernel/nv2a_pb_scan.c` | 4 | `--ours` | nothing | the 29 clean lines go with it |
| `src/kernel/kernel_bridge.c` | 16 | fork in all 16. `586-593`: fork's `recomp_diag_thread_start`, no `guest_cpu_*`. `811-820`: `xbox_dir_context_release`. `954-967`: fork's `xbox_KmemLegacy` branch. `1294-1359`: fork's `xbox_VmFree`. `1760-1896`: fork's `KeResetEvent`, APC probe and `[WAIT]` telemetry. `1913-1963`: fork's `[WAIT]` logging. `2485-2491`/`2788-2796`: fork's guest-meter wrap. `2541-2925`: fork's DPC lock, insert, remove and snapshot drain. `5115-5124`: fork's `MmQueryAllocationSize`. `7937-7964`: fork (empty) | `[EXIT]` and holder changes live in `kernel_hal.c` | `:30` `guest_vmem.h` include (with EXT_VMA), `:558-559`, `:952` extern (harmless; keep or drop), `:1005-1030`, `:1229-1270`, `:1708`, `:2737-2755`, `:7149-7220`, `:7877-7886`, `:10420-10500` (restore the single `kernel_thunk_dispatch`), `:10843` |
| `src/kernel/xbox_memory_layout.c` | 9 | fork in all 9 (`17-23` keeps `recomp_diagnostics.h`, `kmem.h`, `fence_snapshot.h`; `2987-3117` keeps the admitted AC'97 comment and the `RECOMP_APU_TRAP` block; `5827-6382` keeps kmem) | keep CLEAN `RECOMP_PB_REPORT_MS` (`:1214-1226`), span `#ifndef _WIN32` (`:2362-2387`), cert title (`:2536-2549`) | **`:3093` back to `0x00080000u`**; `:3214-3220` and `:5466` (guest_vmem); `:5630` duplicate lock; `:5640-5700` (reclaim, insert, carve); `:5890` `int reclaim`; **`:6299/:6309` and `:6330/:6384`** (recursive locks) |
| `src/kernel/kernel_hal.c` | 1 | fork includes (`guest_meter.h`, `nv2a_mmio_hook.h`) | holder `guest_ra/guest_esp` (needs `g_esp` extern, already in the clean hunk) and the `[EXIT]` lines | `irql_publish` (`:193`) and its 5 calls; `xbox_IrqlEnterInterrupt/Leave` (`:355-386`); then the `xbox_memory_layout.h` include is unneeded |
| `src/kernel/kernel.h` | 1 | fork (`xbox_IrqlBlocksDeviceInterrupts`) | `g_xbox_path_hook` decl (clean, keep); `STATUS_CONFLICTING_ADDRESSES` guard (clean, harmless) | `xbox_dir_context_drop` decl (`:688-690`) |
| `src/kernel/kernel_file.c` | 1 | fork `xbox_dir_context_release(Handle)` | — | `xbox_dir_context_drop` definition (`:662-687`) |
| `src/kernel/CMakeLists.txt` | 1 | union **without** `guest_vmem.c`: `kmem.c`, `kernel_vm.c` | keep clean `nv2a_vsh_interp.c`, `nv2a_combiner.c` (dormant) | — |
| `src/apu/apu_dsp.c` | DU | **stays deleted** (`git rm`) | the fork's GP DSP port (`dsp/gp_ep.c`) needs nothing from it (OBSERVED: upstream's change only moves the synthetic ack) | — |
| `src/apu/apu_state.h` | 1 | fork (no DSP stubs, no `mcpx_apu_dsp_ack_poll`) | — | — |
| `src/apu/apu_shim.h` | 7 | fork (`apu_phys_ptr`, null-safe) | — | — |
| `src/apu/apu_vp.c` | 1 | fork (`apu_phys_ptr`) | keep clean underrun break `:1061` | — |
| `src/apu/apu_core.c` (clean file) | 0 | fork | — | `:27` include (if unused), `:71-185` (`mcpx_apu_phys`, `s_irq_line`, `apu_deliver_irq` and externs), restore the fork's `update_irq` body, `:654-660` doorbell call, `:680-701` (`set_irq` delivery and `SwitchToThread`) |
| `src/usb/ohci.c` | 1 | fork (`xbox_GuestSerialBeginAtomic`) | all clean OHCI changes | — |
| `src/video/fb_present.c` | 2 | **union**: fork's `fb_present_observe()` and upstream's title-bar block; both stub sets | — | — |
| `tools/recomp/translator.py` | 6 | `120-281`: fork (drop `_edge_flag_plan`/`_REG_TOKEN`). `521-566`: **union** (`_mark_back_edges` and `load_label_db`). `1467-1483`: fork. `2455-2460`: **union** (fcmov **and** cmps/scas). `2664-2683`: fork (`materialise`/`_fc_`), **but keep** upstream's `unseen_entries` lines only if another adopted feature needs them (OBSERVED: only #159 uses them, so drop). `2703-2730`: fork | — | clean `@@-2481/-2493` (`block_lines`, edge sets) |
| `tools/recomp/lifter.py` | 3 | `436-442`: **union** (`wbinvd` **and** `pushad`,`popal`,`popad`). `3086-3128`: fork's three `rep movs` bodies (`recomp_range_is_mmio`); optionally adopt upstream's 64-bit range arithmetic inside them. `4448-4512`: fork's `_track_flag_state` call, **and port** upstream's two new leading branches (`_BARE_STRING_COMPARES` without an xmm operand; `_SSE_CMP_NAMED` -> preserve) into `_track_flag_state` | — | `_lift_rep_movs` (clean, `@@-2924`) unused if fork wins; switch-table hunk, see section 5 |
| `tools/recomp/test_flag_join_backedge.py` | 3 | fork | — | — |
| `tools/recomp/test_lifter_direction_flag.py` | 1 | fork assertion (MEM8 loop over `_n`) | — | — |

**Executor follow-up packets (the renderer port), in order; each a Windows A/B against the merged
binary:** R1 pure fixes (`surface_bpp`, P8, TENT, blending, colour mask, immediate attributes).
R2 four texture stages plus combiners, **keeping the stage-0 `CONTROL0` gate (L48)**. R3 vertex
programs: choose one interpreter (recommend upstream's `nv2a_vsh_interp`; keep
`nv2a_pb_exec_vp_view` and L48 semantics so `tests/test_nv2a_hal.c` passes; map `RECOMP_VP=0` to
`RECOMP_NO_VSH` or document the change); depth and near-plane clipping. Every draw site in
`raster_batch_program` must call `present_track_drawn` and feed `trace_batch`. R4 threaded raster.
R5 trace and dump switches. **Never** port: `GET_REPORT` guest writes (count only), the "biggest
surface" present rule, `drawn_stale`.

---

## 4. Coexistence: upstream executor vs the owner commit consumer

**Interface: yes, no redesign** (OBSERVED). The walk (`src/nv2a/nv2a_core.c`: unit commits, admission
table, kick observer, `NV2A_COMMIT_PARTIAL`, per-walk VirtualQuery cache, subroutine-return carry) is
untouched by upstream (`git diff --stat ea60cfa upstream/main -- src/nv2a` shows only `qemu_shim.h`, 5
lines). The consumer is `nv2a_set_commit_consumer(pb_exec_commit_consumer)`, which filters to `NV097`
and then calls `nv2a_pb_exec_method(subch, method, param)`. Upstream keeps that exact entry point. Its
new handlers fire only for methods the table admits.

**File: no.** It is two divergent executors (headline 1), so the port is a design task rather than a
merge resolution. Constraints the port must honour:
1. **Consumer contract** (`HEAD:nv2a_pb_exec.c:1351-1356`: under the PFIFO lock, no locks, no blocking,
   no walk callbacks). Threaded raster blocks the walk thread on its own workers (`WaitForSingleObject(done,
   INFINITE)`) while the owner lock is held. There is no lock inversion: the workers take no runtime
   locks (OBSERVED in `raster_worker`). But per-pixel work grows (combiners, depth, four stages), and the
   fork just measured owner-lock holds starving vblank (title-010: max hold 1636 ms to 68 ms after the
   cache). A faulting worker would hang the walk thread with the lock held (INFERRED).
2. **L44**: under `RECOMP_NV2A_ADMIT_UNKNOWN`, unknown methods reach the consumer. Upstream's executor
   *executes* some of them (`GET_REPORT` writes guest memory), contradicting "not executed".
3. **Present**: upstream decides the present target from `note_drawn`. The fork's evidence (L47 flip
   trace, `present_track_*`) needs every program-path draw reported to the tracker.
4. **Report cadence**: the fork makes the consumer the only executor-report caller. Upstream's
   ack-thread report path must stay suppressed under the owner (it is, through `nv2a_pb_scan_report`'s
   `scan_exec_suppressed`).
5. **Test APIs** `nv2a_pb_exec_counters` and `nv2a_pb_exec_vp_view`, which the game's `tests/test_nv2a_hal.c` uses.

**Stop condition:** "adopt upstream's renderer inside this merge" cannot be done without a redesign of
the executor file. Choices: (a) **recommended**: merge with the fork executor (`--ours`), upstream's
vsh/combiner files dormant, then packets R1-R5. (b) Take upstream's executor and port the fork's
consumer, present tracker, L47/L48, census, counters, vp_view and FFP/lighting/AA/backend. More fork
evidence code is at risk, and the FFP path (fork `eeaaa0d`/`bd414f9`) has no upstream equivalent.
(c) Keep both renderers side by side behind a switch. That means two state models fed by one method
stream, so no.

---

## 5. Lifter / translator / disasm / template changes

None of these affects the game until the next **full regeneration** (Windows; provenance amendment;
`config/generated-patches.json` re-applied; T18 anchors must still match the new `recomp_types.h`).
The toolkit tests are the only immediate effect.

| Change | Alters generated code? | Interaction with fork lifter fixes |
|---|---|---|
| `64796df`, `5d507e7` x87 precision control (`RECOMP_FP_PC`, PC=24 rounds the significand) | **yes**, every x87 arithmetic result under PC=24 | none textual. Behavioural: JSRF's D3D/game FP results change, so A/B gameplay and physics (INFERRED) |
| `894ceef` lahf, comiss unordered; `a006905` fcomi/sahf unordered map | yes | **the fork's `fcmovcc` (`671ab0a`) uses `_make_condition`**, so `fcmovu`/`fcmovnu`/`fcmovb` become unordered-correct. A positive change, but a behaviour change |
| `206b733` all SSE compares, single cmps/scas (previously `RECOMP_UNIMPL`, L06) | yes | **`_flags` declaration conflict `translator.py:2455-2460`: must be the union** (the fork's fcmov fallback reads `_flags`; upstream's bare cmps/scas write it). Either side alone produces undeclared `_flags` in some functions. Flag-tracking conflict `lifter.py:4448-4512`: port into `_track_flag_state` |
| `fc54a43` float compares snapshot at joins | yes | composes with fork `ca4257c` (more joins answered by a plain merge; INFERRED) |
| `57f47ff` + `64bd733` (#159) per-edge join conditions | yes | **duplicate of fork `ca4257c`** (`_fc_<key>`, liveness-based) vs upstream's `_jf_<addr>` (first reader only). Drop upstream's, including the clean pieces and tests |
| `21d9ce7`, `f4e3186` (lifter), `c576d24` (disasm engine) switch tables | yes, plus the analysis DB | conflicts with fork `69842cf`/`79a0070` slot-one retry rule (`translator.py:1467-1483`). The lifter's clean `_analyze_switch_table` hunks must agree with the translator's rule |
| `5e1d2ed` rep movs element-sized | yes | same goal as fork `5364747`; fork wins (`recomp_range_is_mmio` covers NV2A plus APU; upstream bounds at `0xFD000000`, which is broader and also covers MCPX/USB). Possible follow-up: widen `recomp_range_is_mmio` |
| `1ba591e` PUSHAD/POPAD | yes (previously unimplemented) | union at `lifter.py:436-442` |
| `d88cbea` `RECOMP_ICALL_SAFE_CC` after `call; add esp,N` | yes (failed-ICALL paths only; fatal in JSRF unless `JSRF_ALLOW_UNRESOLVED`) | template grows the macros; check the L37 ABI-delta patches |
| `85d4f30` functions extended over trailing inline jump tables | yes (function ends) | may overlap reviewed `config/boundary-fixes.json` entries (L02) |
| `4e737de`, `e5043b3`, `368b16d` owners that reach a protected (manual) entry stay split | yes | **high churn for JSRF**: about 3,076 manual/recovered entries are "protected". Interacts with the fork's tail_jump_alias folding (`58a9cf9`, `ff4d442`; D5) |
| `7f47071`, `3791259`, `f2f5bcf` wrapper routing, `sub_X_gen`, manual list excludes wrapped bodies | yes, and **`config/manual-functions.json` content** | game-facing contract (`--exclude-manual src/recomp_manual.c`, `recover-functions.py`); check before regenerating |
| `7f1b263` manual entry hooks survive boundary repair | yes when hooks exist | game boundary workflow |
| `4c08bee` string-reference labels never name code | yes (names) | default `labels.json` is read (`__main__.py:46`); no `str_` names found in tracked game config (OBSERVED) |
| `2dd0287`, `1409a7d`, `1446779`, `ca86c0f` disasm | via the analysis DB at the next disassembly | boundary churn |
| `c0029d0` `RECOMP_CALL_PROFILE` | inert unless defined | — |

---

## 6. Risk list: hunks most likely to break the JSRF title path if resolved wrongly

| # | Hunk | Failure if wrong | Windows A/B: what to compare (merged vs pre-merge binary, same seconds and labels) |
|---|---|---|---|
| 1 | `nv2a_pb_exec.c` resolution (shadowing cases `:4401`, `:4738-4759`; mixed clean rewrite) | VP batches stop drawing (`xform_mode` never set), textures unbound, render state lost | game `ctest` (`test_nv2a_hal` "ordered"); `[GPU]` draws, tris, `batches_ffp`; `[TEXUSE]` lines; `[GPU] PB executor registered`; the frame hashes of `0x80084000` |
| 2 | Present path (`fcea264` vs `present_track`) | the window shows the wrong or a stale surface | `[FBPRESENT]` reason mix (baseline 324/370 `drawn_this_frame`); `RECOMP_FLIP_TRACE` hashes; flips == presents |
| 3 | Heap locks `xbox_memory_layout.c:6299,6330` | silent hang at the first heap free | boot reaches `[HEAP] free #1` and continues; a hang with no error at that point is this |
| 4 | APU: `:3093` trap size, `apu_core.c:659`, IRQ delivery | GP DSP model bypassed; synthetic ack reintroduced; DirectSound ISR armed | `APU: 0xFE800000..0xFE880000 trapped`; `[APUWAIT]` word 3->0 transition; `[GPRUN]`; **no** `[APU] interrupt delivered` line; rule-3 scan clean |
| 5 | DPC drain `kernel_bridge.c:2882-2906` | self-requeueing DPC loops forever and vblank starves | vblank pulses per pass, late re-arms, `[GSERIAL]`, timer-thread liveness |
| 6 | IRQL publish and brackets (if adopted) | DirectSound lock path and serial-mode gate change | `[IRQLHOLD]`; DSound progress; `[GMETER]`; do it as its own single-variable packet |
| 7 | Contiguous page 0 (`423263b`, if adopted) | every contiguous VA shifts +0x1000; address-keyed records invalid | `[KERNEL] MmAllocateContiguousMemory` VAs; ring base `0x80001000`; surface `0x80084000` |
| 8 | Renderer cost under the owner lock (R2-R4) | longer holds, so vblank late and flip rate down | owner-lock max hold (R3 baseline 68 ms), late re-arms (0/11924), presents at t=120 s (2568) |
| 9 | Span POSIX-only (`9547d39`) | the host layout changes; the contiguous-window mapping could differ | memory-layout init lines; no error 487; `check-dump-mapping.py` control read at `0x00011000` |
| 10 | Translator `_flags` union and the #159 drop (at regen) | generated C fails to compile, or joins read an unassigned `_flags` | regen: compile; `flag_gaps` report count; diff of function count and boundaries; `[UNIMPL]` count |

---

## Noticed (out of scope)
- `HEAD:src/kernel/xbox_memory_layout.c:526` `ac97_arm_write_trap` is defined in the fork with no call site (dead code; its comment still says "alongside RECOMP_AC97_READY").
- `HEAD:src/kernel/nv2a_pb_exec.c:464` `nv2a_pb_set_semaphore_target` has no caller, so the executor's own `0x1D70` guest write is dormant. It is undocumented in the L19 row.
- `check-merge-structure.py` cannot detect default-branch shadowing (headline 2), and its `TOOLKIT` path is hard-coded to `~/src/xboxrecomp`, so it cannot be pointed at a scratch worktree.
