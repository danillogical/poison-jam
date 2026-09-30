# JSRF compatibility ledger

**Why this exists (owner decision, 2026-09-30):** the bare minimum is pragmatic. Take the path of
least resistance to the title screen, and record here every way the port departs from running the
original code on real hardware. A shortcut is allowed; an **unrecorded** shortcut is not. The plan
(`plan-jsrf-bare-minimum.md`) owns what to do next; this file owns what the port currently is.

## Classes

A path can carry more than one class (a translated call can enter a reimplemented bridge that wraps a
host API); list the one that decides fidelity first.

| Class | Meaning | Example here |
|---|---|---|
| **Reimplemented** | Independently written host code supplies the operation | kernel bridges, CRT divide helpers |
| **Translated** | Guest code or data converted to another representation | lifted x86 → C; pushbuffer → host draws |
| **Emulated** | Explicit state reproduces hardware behaviour over time | NV2A registers, GP DSP, vblank pulse |
| **Wrapped** | A similar host capability behind an adapter | host files, XAudio2, keyboard |
| **Stubbed** | Interface exists without full behaviour | `PIO_FREE` always `0x80` |
| **Patched** | One guest path redirected or corrected | recovered function spans |
| **Approximated** | Observable behaviour supplied without full fidelity | GPU busy-bit acks, fence mirror, stereo fold |
| **Intentionally ignored** | Input or state deliberately unused | `.rdata` page protection |

## Rules

- **Every run a milestone relies on** lists, in its record, the ledger IDs of the non-*Emulated*,
  non-*Translated* paths it exercised (switches set, patches applied). A path the result depends on
  that is not in this file blocks acceptance until it is added.
- **Add the entry in the same commit** as the code or switch that creates the path; change its row
  when the path changes; move it to *Retired* when removed. Never delete history.
- **Least resistance means cheapest honest class:** if emulating a blocker takes more than about a
  day, approximate, stub or patch it, record it, and move on. Upgrade later only when it blocks
  something.
- Evidence profiles (`docs/jsrf-run-profiles.md`) still label every run; a *strict* run is now a
  diagnostic, not the gate for bare-minimum milestones.

## Entries

`Active` says when the path is in effect. Paths are toolkit (`xboxrecomp/`) unless marked game.

### CPU and translation

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L01 | Lifted guest code, x86 → C | Translated | game `src/recomp/gen/` | always | regenerate with the toolkit lifter (TR §2) |
| L02 | Reviewed recovered bodies (3,074 entries) and 7 boundary fixes | Patched | game `config/recovered-functions.json`, `config/boundary-fixes.json`, `src/recomp/recovered/recovered.c` | always | spans the translator got wrong; retire entries the translator now produces (plan V5) |
| L03 | CRT `memmove` | Reimplemented | game `src/jsrf_crt.c` | always | verified replacement |
| L04 | CRT 64-bit divides `__alldiv`/`__aulldiv`/`__aullrem`/`__aulldvrm` | Reimplemented | game `src/jsrf_crt.c`, `tests/test_crt_divide.c` | always | the old lifter dropped `rcr` (TR §2); remove with their manual entries if generated code owns them again |
| L05 | COM error tail `sub_00162B9D` | Reimplemented | game `src/recomp_manual.c:19` | always | the v0.12 translator folds it (TR §2) |
| L06 | Untranslated instructions execute as no-ops, reported `[UNIMPL]` | Stubbed | game `src/recomp_manual.c:121` | always | none reached in V3; translate any that becomes reached |

### Kernel, memory, threads

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L07 | Kernel imports through the ordinal bridge | Reimplemented | `src/kernel/kernel_bridge.c` | always | upgrade per ordinal when a run depends on it |
| L08 | File I/O (`NtCreateFile`, `NtReadFile`, …) on host files | Wrapped | `src/kernel/kernel_file.c` | always | `RECOMP_ASYNC_IO` adds pending/async completion |
| L09 | Guest virtual memory (region registry), heap and contiguous arena | Reimplemented | `src/kernel/kmem.c`, `kernel_vm.c`, `xbox_memory_layout.c` | always | `RECOMP_KMEM_LEGACY=1` restores the old semantics |
| L10 | Kernel data exports JSRF imports (ordinals 16, 40, 156, 164, 259, 322, 323, 325, 354, 356) | Reimplemented | `kernel_data_init` in `src/kernel/kernel_bridge.c` | always | values synthesised; object types use a self-address convention (TR §5, V3) |
| L11 | Six guest threads run on host threads; the GPU interrupt handler and all DPCs run on the timer thread every 10 ms with no IRQL gate | Approximated | `src/kernel/kernel_bridge.c:2628-2630` | always | the Xbox has one CPU; if races block progress, add a serialised-guest mode (defect D4) |
| L12 | `.rdata` page protection | Intentionally ignored | `src/kernel/xbox_memory_layout.c` | always | `.rdata` shares host pages with neighbouring sections |
| L13 | `KeTickCount` advance | Emulated | kernel clock worker | always | admitted autonomous clock (run profiles) |

### Graphics

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L14 | NV2A registers, PFIFO and the strict submission walk | Emulated | `src/nv2a/nv2a_core.c` | always | captures state, renders nothing |
| L15 | Vblank pulse on the display clock | Emulated | `src/nv2a/nv2a_core.c:184`, `nv2a_mmio_hook.c:584` | always | admitted periodic device event |
| L16 | GPU busy-bit acknowledgements and DMA_GET mirroring (`RECOMP_GPU_ACK`, on unless `0`) | Approximated | ack thread, `src/kernel/xbox_memory_layout.c:1044` | default on | answers polls without GPU work; the price of using the executor (L18) |
| L17 | Fence mirror: device `+0x30` copied to `*(device+0x34)` | Approximated | game `src/main.c:577` | always | D3D's fence without a semaphore release; replace with L19 only if it matters |
| L18 | Pushbuffer executor → host renderer (`RECOMP_PB_EXEC`) | Translated | `src/kernel/nv2a_pb_exec.c`, `nv2a_pb_scan.c` | opt-in, needs L16 | fastest route to visible frames (plan fast path F4) |
| L19 | NV2A action methods: semaphore release, software-method trap, `FLIP_STALL` (`RECOMP_NV2A_ACTIONS=1`) | Emulated | `src/nv2a/nv2a_core.c` | dormant | not needed on the executor path |

### Audio

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L20 | AC'97 codec-ready bit, `GS.bit8 := GC.bit1` | Emulated | `src/kernel/xbox_memory_layout.c` | always | admitted (TR §3) |
| L21 | GP DSP56300 (xemu port) | Emulated | `src/apu/dsp/` | with `RECOMP_APU_TRAP` | clears DirectSound's pending word (TR §4) |
| L22 | APU voice processor | Emulated | `src/apu/apu_vp.c` | with `RECOMP_APU_TRAP` | see defect D2 |
| L23 | `PIO_FREE` answers `0x80` | Stubbed | `src/apu/apu_vp.c:556-557` | with `RECOMP_APU_TRAP` | "pretend queue is empty"; model it only if a wait depends on its value (TR §4) |
| L24 | GP/EP register reads answer zero | Stubbed | APU register map | with `RECOMP_APU_TRAP` | run profiles, feature enablement |
| L25 | Speaker mixdown: even bins left, odd bins right | Approximated | `src/apu/apu_mixdown.c:52` | always | port a 5.1 fold if audio sounds wrong |
| L26 | Host audio output | Wrapped | XAudio2 path | always | — |

### Input

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L27 | USB OHCI and XID gamepad (`RECOMP_USB`) | Emulated | `src/usb/ohci.c` | opt-in | upstream v0.12 |
| L28 | Host keyboard as a pad (`RECOMP_KEYBOARD`) | Wrapped | upstream input | opt-in | — |

## Known defects (not classes — fix or record a class when they bite)

| ID | Defect | Where | Effect |
|---|---|---|---|
| D1 | Every DMA_PUT write loses bit 16: the mask `0x1FFEFFFF` came from reading `0x100410` (`NV_PFB_WBC`, the write-buffer flush) as DMA_PUT | `src/nv2a/nv2a_regs.h:171`, `nv2a_core.c:1699-1735`; game `docs/jsrf-kick-get-contract.md:60` has the same mislabel | once the ring passes 64 KB, PUT `0x12764` is walked as `0x2764`; fix before any rendering milestone |
| D2 | Voice-processor DMA uses low RAM, not the contiguous window | `src/apu/apu_shim.h:167-191` (flagged at `apu_watch.c:455-462`) | a voice-processor write can land in the XBE image |
| D3 | NV097 method parameters are stored into the PGRAPH register array | `src/nv2a/nv2a_core.c:1630` | some methods would overwrite interrupt/status registers the GPU handler reads; latent |
| D4 | ISR and DPC work runs concurrently with guest threads, no IRQL gate (L11) | `src/kernel/kernel_bridge.c:2628-2630` | `[GMETER] max=4`; the first fault site varies run to run (TR §5) |

## Retired

None yet.
