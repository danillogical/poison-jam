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
- A patch to generated code is an entry in `config/generated-patches.json` and names the ledger ID
  it implements; `scripts/patch-generated.py` refuses an ID this file does not have (plan T18).

## Entries

`Active` says when the path is in effect. Paths are toolkit (`xboxrecomp/`) unless marked game.

### CPU and translation

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L01 | Lifted guest code, x86 → C | Translated | game `src/recomp/gen/` | always | regenerate with the toolkit lifter (TR §2) |
| L02 | Reviewed recovered bodies (3,076 entries) and 7 boundary fixes | Patched | game `config/recovered-functions.json`, `config/boundary-fixes.json`, `src/recomp/recovered/recovered.c` | always | spans the translator got wrong; retire entries the translator now produces (plan V5). Was 3,074 until the F3 fix added `0x00037550` (2026-09-30); owner recovery added `0x0014FEF0..0x00150104`, stack_args12, restoring the render-state method instead of the folded E_INVALIDARG tail (2026-10-02; owner SEGA review). Not a fade bypass or demonstrated fade-cause fix. The V5 audit's 3,074 figures are historical and were true when it ran. 2026-10-04: `0x001403B0` end widened `0x00140485` -> `0x00140540` (the old end was mid-function; f8 aborted in the checked wrapper). Same day: `0x0013F9E0` end `0x0013FBC0` -> `0x0013FAD0`, and `0x0013FAD0..0x0013FBC0` added. The old span swallowed the `.data` `0x0022DB38` slot `+0x20` method; f14 died on `call [vtable+0x20]` at `0x0013C0C1` with `[ICALL] Failed to resolve VA 0x0013FAD0`. The body was already lifted inside `sub_0013F9E0` and moved unchanged. 3095 functions. Not a bypass. 2026-10-05: 3,095 -> 3,102 entries across the session; the run-driven chain added `0x0013B750`, `0x0007AF90`, `0x0006A770`, `0x00032610`, `0x00054750` and split `0x000C2630`/`0x00047850` out of their swallowing neighbours, and corrected six defective spans (`0x000BB7B0` widened to own its shared epilogue, `0x000B0210`, `0x000C25D0`, `0x00047820`, `0x0007DA30`, `0x00032610`) plus three `stack_args` values (`0x00074C70` 4->0, `0x0007DA30` 0->4, `0x00047820` 0->4). Twelve further spans were widened as one batch to own their own release arms, resolving 65 fatal trap call sites; `tests/test_recovery_span_ownership.py` `KNOWN_OPEN` shrank 80 -> 66 and now equals the measured set exactly, and `scripts/check-span-exits.py` findings fell 431 -> 363. `0x00054750` needed one generated patch (`remove-54750-stub`) because the translation pass had also emitted a "not detected" trap for it, and the two definitions collided at link time. 2026-10-05 (F6): 3,102 -> 3,157 entries. `scripts/check-hidden-entries.py` (new gate, no baseline) proved 50 spans whose declared `end` over-ran their own reachable body into a separately evidenced function, and tightened all 50; 48 of the consumed addresses had no body and gained one (38 PROVED, 10 INFERRED), 5 already owned an entry, and 2 (`0x556D0`, `0xC42E0`) already had generated bodies. Run g07 then exposed the mirror-image defect: `0x000B5EB0` had been tightened to `[0x000B5EB0, 0x000B5F3A)`, which cut its own jump table at `0x000B5F0F` (8 of 9 arms beyond the end), so the lifter emitted `RECOMP_ITAIL` and the guest trapped at `[ICALL] Failed to resolve VA 0x000B5F82`. The span is restored to the body's real end `0x000B6732` and the false split `0x000B5F3A` removed as an internal label (fallthrough from arm `0xB5F16`, its only raw dword in packed `.data`, no branch to it). That removal needed a second generated patch, `remove-b5f3a-dispatch`, because `recomp_dispatch.c` still named the deleted symbol and the link failed with LNK2001. 3157 functions. |
| L03 | CRT `memmove` | Reimplemented | game `src/jsrf_crt.c` | always | verified replacement |
| L04 | CRT 64-bit divides `__alldiv`/`__aulldiv`/`__aullrem`/`__aulldvrm` | Reimplemented | game `src/jsrf_crt.c`, `tests/test_crt_divide.c` | always | the old lifter dropped `rcr` (TR §2); remove with their manual entries if generated code owns them again |
| L05 | COM error tail `sub_00162B9D` | Reimplemented | game `src/recomp_manual.c:19` | always | the v0.12 translator folds it (TR §2) |
| L06 | Untranslated instructions execute as no-ops, reported `[UNIMPL]` | Stubbed | game `src/recomp_manual.c:121` | always | none reached in V3; translate any that becomes reached. 2026-10-04: x87 mnemonics without a case now report too; in the lifter (671ab0a) `fcmovcc`/`fisttp` are translated, `fnclex`/`fnop`/`fwait` are explicit no-ops, `fldenv` reports; JSRF was relifted only for the two fcmov bodies, so `fisttp` (sub_000FDDA2), `fnclex` (sub_0017F02C, recovered.c) and `fldenv` still run as no-ops there |

### Kernel, memory, threads

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L07 | Kernel imports through the ordinal bridge | Reimplemented | `src/kernel/kernel_bridge.c` | always | upgrade per ordinal when a run depends on it |
| L08 | File I/O (`NtCreateFile`, `NtReadFile`, …) on host files | Wrapped | `src/kernel/kernel_file.c` | always | the read is still synchronous. `RECOMP_ASYNC_IO` only rewrites a successful status to `STATUS_PENDING`; it does not time the completion APC. That timing is L42 |
| L29 | `NtReadFile` reads into a host buffer and copies it into guest memory, so the store is user-mode and visible to TTD and page protection | Wrapped | `src/kernel/kernel_bridge.c:3638` | always; `RECOMP_READ_DIRECT=1` reads straight into guest memory | the `[READ]` line warns when a read lands in a read-only section |
| L30 | `FILE_NO_INTERMEDIATE_BUFFERING` treated as a caching hint, not `FILE_FLAG_NO_BUFFERING` (whose sector alignment guest buffers do not meet) | Intentionally ignored | `src/kernel/kernel_file.c:191` | always | Mercenaries-Recompiled does the same |
| L31 | `GENERIC_ALL` mapped to read, write and delete rather than Win32 `GENERIC_ALL` | Wrapped | `src/kernel/kernel_file.c:97` | always | avoids demanding owner/security rights a title file never needs |
| L09 | Guest virtual memory (region registry), heap and contiguous arena | Reimplemented | `src/kernel/kmem.c`, `kernel_vm.c`, `xbox_memory_layout.c` | always | `RECOMP_KMEM_LEGACY=1` restores the old semantics |
| L10 | Kernel data exports JSRF imports (ordinals 16, 40, 156, 164, 259, 322, 323, 325, 354, 356) | Reimplemented | `kernel_data_init` in `src/kernel/kernel_bridge.c` | always | values synthesised; object types use a self-address convention (TR §5, V3) |
| L11 | Six guest threads run on host threads; the GPU interrupt handler and all DPCs run on the timer thread every 10 ms with no IRQL gate | Approximated | `src/kernel/kernel_bridge.c:2780-2781` | unless `RECOMP_GUEST_SERIAL=1` | the Xbox has one CPU; L34 is the serialised alternative (defect D4) |
| L12 | `.rdata` page protection | Intentionally ignored | `src/kernel/xbox_memory_layout.c` | unless `RECOMP_RDATA_GUARD=1` | `.rdata` shares host pages with neighbouring sections; L32 is the diagnostic that enforces it |
| L13 | `KeTickCount` advance | Emulated | kernel clock worker | always | admitted autonomous clock (run profiles) |
| L32 | Read-only tripwire: pages wholly covered by read-only XBE sections are write-protected in the canonical view and all 28 mirrors after the thunk table is installed; each store is single-stepped, reported (VA, section, view, old/new dword, RIP, guest return chain) and allowed | Emulated | `src/kernel/xbox_memory_layout.c:1950` | `RECOMP_RDATA_GUARD=1` (diagnostic) | four reports per page, 256 in all; refuses to arm with the A2h traces |
| L33 | Host timer period of 1 ms (`timeBeginPeriod(1)`), as xemu requests | Wrapped | `src/kernel/kernel_bridge.c:9886` | always | the default 15.6 ms period made the 10 ms tick and guest waits wake late |
| L34 | Serialised guest mode: one host thread runs guest code at a time; the GPU ISR, DPCs and timer DPCs run in one atomic section per tick, deferred while the guest's IRQL blocks them; a waiter runs anyway after a bounded wait and is counted as an overrun | Approximated | `src/kernel/guest_meter.c:88`, `src/kernel/kernel_bridge.c:2716` | `RECOMP_GUEST_SERIAL=1` (timeout `RECOMP_GUEST_SERIAL_TIMEOUT_MS`, default 100) | `[GSERIAL]` counts overruns; loop back-edges yield through `RECOMP_BACKEDGE()` once the tree is regenerated with `--backedge-yield` (toolkit `86113c7`; `just regen` passes it), until then a guest spin loop costs one timeout |
| L35 | DPC queue: one queue drained by the timer thread every 10 ms; a DPC is queued at most once, `KeRemoveQueueDpc` cancels, and a DPC queued from a DPC runs on the next drain | Approximated | `src/kernel/kernel_bridge.c:2256`, `:2410` | always | no per-processor list or importance ordering |
| L36 | `KeRaiseIrqlToSynchLevel` raises to `SYNCH_LEVEL` = `DISPATCH_LEVEL` (uniprocessor NT) and is tracked; `KeGetCurrentIrql` reports the tracked per-thread level | Reimplemented | `src/kernel/kernel_hal.c` (`xbox_KeRaiseIrqlToSynchLevel`) | always | was: returned PASSIVE and recorded nothing, invisible to the interrupt gates (toolkit `92715dc`) |
| L37 | Exact-delta ABI checks in generated code: `RECOMP_ABI_CALL` checks each call's ESP delta against `recomp_abi_deltas.c` and allows per-site register exemptions | Patched | `config/generated-patches.json` (`abi-delta-*`), game `src/diagnostics.c` | builds with `RECOMP_ABI_CHECK` | re-applied to `recomp_types.h` after every regeneration (T18) |
| L38 | A4b watch-ledger stores: `jsrf_watch_store` records six guest stores before they happen | Patched | `config/generated-patches.json` (`a4b-watch-*`), game `src/diagnostics.c` | always (the forwarder is inert unless its trace is on) | observation instrumentation; retire with the A4b packet's records |
| L43 | The runtime stop chain is a checked record (`config/stop-chain.json`), not prose | Patched | game `config/stop-chain.json`, `scripts/check-stop-chain.py`, `tests/test_stop_chain.py` | always | a record-consistency gate, not a fidelity claim. Each row cites the archived runs that establish it, and the gate refuses a row that claims more than they show: a `confirmed` role needs the address's ABI-verified return line AND a run revision descending from the row's `repair_commit`; a `not_exercised` role needs the return line to be ABSENT; a `discovered` role needs a defect line or an ICALL-history frame; and a `RUNTIME_CONFIRMED` state needs at least one confirming role. Every row also re-checks its cited run's log hash against that run's own `metadata.json`, so a citation cannot be satisfied by editing a log afterwards. It exists because the chain twice claimed progression its archives did not establish: `0x00032610` was credited to f25, which never dispatched it, and `0xB5EB0` was repaired and then credited by two runs that never executed it. Zero baseline; 21 controls, the first being a genuine confirming row that cites a run with no return line and must fail. It does not prove any run reached the title screen |
| L42 | File completion APCs run at the next alertable wait on the issuing thread, not inside `NtReadFile`/`NtWriteFile` | Reimplemented | `src/kernel/kernel_bridge.c` (`bridge_complete_file_io`, `bridge_drain_file_apcs`) | always | the bytes and the status block are written, and the event is signalled, before the read returns. The APC runs when that thread enters `KeDelayExecutionThread`, `KeWaitForSingleObject`, `KeWaitForMultipleObjects`, `NtWaitForSingleObject`, `NtWaitForSingleObjectEx`, or `NtWaitForMultipleObjectsEx` with Alertable set, including a zero interval. An APC queued by one of those routines waits for the next alertable wait. A full per-thread queue (32) delivers that one APC inline and logs it. The wait still runs after the APC, so an APC does not abort a long sleep. A direct APC routine pops only its dummy return, and `deliver_one_apc` then pops the three arguments. A kernel-export routine is `kernel_thunk_dispatch`, which already pops that stdcall frame (NtUserIoApcDispatcher is ordinal 232, 12 bytes); popping the 12 again leaves the waiter 12 bytes high. JSRF's poller delay `0x145C28` then reloads `esi` from the shifted frame. f14 and f15, with that second pop removed, store slot `0x273780` as `byte+1 = 1` and `+0x18 = 0x19` on the kernel sample after caller `0x00145C59`. Retire when a title needs the APC to have run before the read returns, or needs it to abort the wait. `xbox_file_apc_test` fails if the APC runs before the alertable delay, and fails if the dispatcher delivery does not restore `g_esp` |
| L41 | Disc-error caller observation: a host log at the entry of `0x6F730` and on the `0x2537E` tail before `esi` is popped, plus a guest-stack dump when a created path contains `JSRF_FATAL`. f9 measured the caller as `0x116EAD`; the same hook now also prints the four ADX slots at that entry | Patched | `config/generated-patches.json` (`fatal-ctor-*`), `scripts/recover-functions.py` (the `0x25310` anchor), `src/diagnostics.c`, `src/jsrf_fatal_observe.c`; toolkit `src/kernel/kernel_bridge.c` | always (the logs do not write guest memory; the file dump fires only for that name) | observation. The caller is measured. Keep the hook until the ADX slot that stored `-1` is named, then remove it. Not a shortcut the boot depends on |

### Graphics

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L14 | NV2A registers, PFIFO and the strict submission walk | Emulated | `src/nv2a/nv2a_core.c` | always | captures state, renders nothing |
| L15 | Vblank pulse on the display clock | Emulated | `src/nv2a/nv2a_core.c:184`, `nv2a_mmio_hook.c:584` | always | admitted periodic device event |
| L16 | GPU busy-bit acknowledgements and DMA_GET mirroring (`RECOMP_GPU_ACK`, on unless `0`) | Approximated | ack thread, `src/kernel/xbox_memory_layout.c:1044` | default on **except when the owner claims the aperture, then inert** | answers polls without GPU work; **the set register mutations are inert under the owner** (Architecture A, toolkit `a71f937`). Before A the owner already retired this whole body, including the guest-memory DMA_GET mirrors; **the defect was that this also retired the executor feed and reports, without a replacement.** The legacy "price of using the executor (L18)" rationale no longer applies: L18 reaches the executor through the owner's consumer, not this thread |
| L17 | Fence mirror: device `+0x30` copied to `*(device+0x34)` | Approximated | game `src/main.c:577` | always | D3D's fence without a semaphore release; replace with L19 only if it matters |
| L18 | Pushbuffer executor → host renderer (`RECOMP_PB_EXEC`) | Translated | **active:** `src/kernel/nv2a_pb_exec.c`; **legacy/historical:** `src/kernel/nv2a_pb_scan.c` | opt-in, **active under `PB_EXEC` via the owner's NV097 consumer** (toolkit `a71f937`); **no L16 dependency** | fastest route to visible frames (plan fast path F4). The former "needs L16" dependency described the legacy worker shape only; `nv2a_pb_scan.c` is the **legacy/historical** survey path, not the active one |
| L19 | NV2A action methods: semaphore release, software-method trap, `FLIP_STALL` (`RECOMP_NV2A_ACTIONS=1`) | Emulated | `src/nv2a/nv2a_core.c` | dormant | not needed on the executor path |
| L40 | NV2A submission walk commits each submission all-or-nothing, with a per-walk budget of 4096 words / 1024 packets; hardware dispatches per method | Approximated | `src/nv2a/nv2a_core.c` submission walk | always, no switch | deliberate local admission/rollback contract, not hardware behavior; remove with an incremental-commit packet. F4 A′ ruling: `docs/reviews/rulings/f4-submission-capacity.md`; staging and sink sized to the existing word budget; focused regression: 9 failures without fix, 344 register/clock contracts pass with fix (toolkit `e8a6e03`; full validation passed: toolkit 5/5 CTest and 30 lifter unittests, game check and 29/29 CTest; exploratory smoke `20261001-020407-358-f4-capacity-fix-smoke`: 64 logged submits OK through GET=PUT `0x47A84`, final GET=PUT `0x16648`, no sink/budget stop; **no frame evidence in that capture** — its `[GPU]` counters were pre-guest initialisation values, so render status is UNKNOWN, not zero — and no strict horizon claim) |
| L39 | NV2A method admission: the walk accepts a method only if it appears in the generated table, and rejects the whole stream otherwise | Stubbed | `src/nv2a/nv2a_method_table.c` (generated), `nv2a_core.c` `nv2a_method_implemented` | always | the accepted set is the union of methods measured in real submission rings, so a method the title submits but no decoded ring contained stops the walk at GET. Adding one means regenerating from a ring that contains it — never a blanket range (see the `0x1720` case, toolkit `1f9309a`) |

### Audio

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L20 | AC'97 codec-ready bit, `GS.bit8 := GC.bit1` | Emulated | `src/kernel/xbox_memory_layout.c` | always | admitted (TR §3) |
| L21 | GP DSP56300 (xemu port) | Emulated | `src/apu/dsp/` | with `RECOMP_APU_TRAP` | clears DirectSound's pending word (TR §4) |
| L22 | APU voice processor | Emulated | `src/apu/apu_vp.c` | with `RECOMP_APU_TRAP` | VP memory accesses use the GP's translation (`src/apu/apu_watch.c:540`); an unmapped access reads zero, writes nothing and logs `[VPDMA]` |
| L23 | `PIO_FREE` answers `0x80` | Stubbed | `src/apu/apu_vp.c:556-557` | with `RECOMP_APU_TRAP` | "pretend queue is empty"; model it only if a wait depends on its value (TR §4) |
| L24 | GP/EP register reads answer zero | Stubbed | APU register map | with `RECOMP_APU_TRAP` | run profiles, feature enablement |
| L25 | Speaker mixdown: 5.1 fold of the DirectSound speaker buses (centre and rear −3 dB, LFE −6 dB, 3D front pair as is, 3D rear pair −3 dB); effect sends from bin 10 up are dropped | Approximated | `src/apu/apu_mixdown.c:101` | always; `RECOMP_APU_MIXDOWN_ALL=2` even/odd, `0` two bins | Mercenaries-Recompiled's fold for when the EP does not run |
| L26 | Host audio output | Wrapped | XAudio2 path | always | — |

### Input

| ID | Path | Class | Where | Active | Note / upgrade when |
|---|---|---|---|---|---|
| L27 | USB OHCI and XID gamepad (`RECOMP_USB`) | Emulated | `src/usb/ohci.c` | opt-in | upstream v0.12 |
| L28 | Host keyboard as a pad (`RECOMP_KEYBOARD`) | Wrapped | upstream input | opt-in | — |

## Known defects (not classes — fix or record a class when they bite)

| ID | Defect | Where | Effect | Status |
|---|---|---|---|---|
| D1 | Every DMA_PUT write lost bit 16: the mask `0x1FFEFFFF` came from reading `0x100410` (`NV_PFB_WBC`, the write-buffer flush) as DMA_PUT | `src/nv2a/nv2a_core.c` `NV_USER_DMA_PUT` write; game `docs/jsrf-kick-get-contract.md:60` | once the ring passes 64 KB the walk stops 64 KB short | **Fixed** 2026-09-30, toolkit `b857665`: PUT stored as written, latch removed; contract doc corrected |
| D2 | Voice-processor DMA used low RAM, not the contiguous window | `src/apu/apu_shim.h`, `src/apu/apu_vp.c` | a voice-processor write could land in the XBE image | **Fixed** 2026-09-30, toolkit `1c6641a`: shared translation (L22) |
| D3 | NV097 method parameters were stored into the PGRAPH register array, which is indexed by byte offset, so method `0x500` landed on `NV_PGRAPH_INTR_EN` and `0x520` on `NV_PGRAPH_CTX_USER` | `src/nv2a/nv2a_core.c` (the walk and `pgraph_method`) | some methods would overwrite interrupt/trap registers the model reads; latent (no accepted method collided) | **Fixed** 2026-09-30, toolkit `9fd83c6`: parameters go to `PGRAPHState.methods`; the game's NV2A contract tests and GPU probes read it |
| D4 | ISR and DPC work runs concurrently with guest threads, no IRQL gate (L11) | `src/kernel/kernel_bridge.c:2780-2781` | `[GMETER] max=4`; the first fault site varies run to run (TR §5) | **Mitigation, opt-in**: L34 (`RECOMP_GUEST_SERIAL=1`), not yet run on the title |
| D5 | The translator folds real functions into their abutting neighbours as `tail_jump_alias` and deletes their bodies. Two entries of the function-pointer table at `.data 0x001EC0F8..` were affected: `0x00037550` (folded into `sub_00038530`) and `0x00026780` (folded into `sub_000278F0`). A call through the table then entered the wrong function, and `sub_00038530`'s `rep movsd` copied the XBE image from `+0x37608` over `.text` and `.rdata`, overwriting the kernel thunk table | `config/recovered-functions.json` (entries added/retargeted), `src/recomp/gen/recomp_dispatch.c` (the alias pairs) | the strict horizon: the table was clobbered and the next thunk call raised `0xE0424943` | **Fixed** 2026-09-30. Recovery entries for both addresses; spans end at each function's own jump table (`0x00037FB4`, `0x0002730C`). Strict run `20260930-225739-446-f3-alias-fix-2-strict`: 93.0 s, 0 invalid ICALLs, `check-dump-mapping.py` `matches: 1`, thunk table intact (TR §5) |

## Retired

None yet.
