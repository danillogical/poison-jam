# JSRF technical record

The facts the project has established and that are still in force, by subsystem, with the evidence and
limits each rests on. The plan (`plan-jsrf-title-screen.md`) owns status and next action; this file owns
the technical facts behind them. Toolkit-side design and licence records live in the toolkit:
`xboxrecomp/src/apu/dsp/PROVENANCE.md` and `xboxrecomp/src/apu/GP-INTEGRATION.md`.

**Condensed record (owner-approved deep clean, 2026-10-09).** Narrative, process history, superseded
readings and push receipts were removed; every section number a live file cites is kept. **The full
previous text is `git show 9d5257f:docs/jsrf-technical-record.md`**; the review records it consolidated
are in git at `d6a1bc0`. Retractions and process lessons are in `docs/jsrf-lessons-learned.md`.

Profiles: "strict" means `RECOMP_GPU_ACK=0` with no synthetic-completion switch
(`docs/jsrf-run-profiles.md`). Runs are under `logs/runs/` (gitignored, on the Windows box).

---

## 1. Toolkit relationship and syncs

The toolkit is the owner's fork `danillogical/xboxrecomp` of `sp00nznet/xboxrecomp`; remotes and push
rules are in `AGENTS.md`.

### v0.11.0 sync (packet A4s-r6, accepted 2026-09-25)

Merged upstream `766ecef`; ten files changed on both sides. Hunk rules, in order: **H1 containment** (if
one side's additions and deletions contain the other's, take it); **H2 disjoint edit union** (no base
line changed by both sides: apply both, local insertions first at a shared point); **H3 version/comment
only** (take upstream). Clean hunks can still produce defects — the merge created a duplicate `case 138`
and a duplicate `bridge_KeResetEvent` — so `scripts/check-merge-structure.py` (conflict markers,
duplicate `case` labels, duplicate file-scope definitions over the evidence binary's build inputs) runs
on the resolved tree **before** the build. The rule that upstream merges never silently change admitted
evidence semantics is in `docs/jsrf-run-profiles.md`.

**AC'97 hunk ruling (Advisor).** Upstream kept a set-only codec-ready write under
`getenv("RECOMP_AC97_READY")`, coupled the APU unmap to it, and added a NABM "RR" write trap. Resolved to
the **local** side: the accepted A3a model owns `GS.bit8`; `RECOMP_APU_TRAP` gates only the APU 512K
unmap; no `getenv("RECOMP_AC97_READY")` and no `|= MCPX_AC97_CODEC_READY` anywhere. Upstream's
`ac97_clear_reset_bits`/`ac97_write_veh`/`ac97_arm_write_trap` may exist but **nothing may call
`ac97_arm_write_trap`**: arming it is its own change packet and must change the model, because the trap
makes page `0xFEC00000–0xFEC00FFF` (holding `GLOB_CNT` and `GLOB_STA`) read-only, so the tick thread's
`InterlockedOr/And` would fault every tick; the cleaner design evaluates `GS.bit8 := GC.bit1` in the
VEH's single-step half after each trapped guest write. **Merge-check scope** is the evidence binary's
build inputs, matched in the form that carries semantics (quoted env names, non-comment tokens, call
sites).

### v0.12.0+ sync (owner, 2026-09-28)

Merged upstream `ea60cfa` as `2925f0b` (121 upstream commits). Three files conflicted (7 hunks):

| File | Resolution |
|---|---|
| `kernel_hal.c` includes | both sides |
| `xbox_memory_layout.c` worker tick | upstream's removal of the early `DMA_GET = DMA_PUT` (the advance now follows the pushbuffer scan: a lost-command race fix); local's mirror ticks outside the GPU-ack gate; upstream's `dsp_ack_tick()`/`poke_tick()` beside them (no-ops unless `RECOMP_DSP_ACK`/`RECOMP_POKE`) |
| `xbox_memory_layout.c` worker start | local `RECOMP_GPU_ACK` read + upstream `dsp_ack_init()`/`poke_init()` |
| `lifter.py` ×2 | independent additions, both sides |
| `lifter.py` ×2 | the same `rep cmps/scas` width fix on both sides, upstream's form |

AC'97 invariants held; structural check clean; build, `ctest`, toolkit tests pass; a strict run reached
the baseline's stop. Upstream's nine new switches are classified in `docs/jsrf-run-profiles.md`
(`RECOMP_DSP_ACK`, `RECOMP_POKE`, `RECOMP_FORCE_RETURN`, `RECOMP_PAD_PRESS` are synthetic → exploratory).
ADPCM's reserved header byte is no longer validated and the step index is clamped: JSRF ends every ADPCM
buffer in a `0x08` pad, and 3.5–4.4 % of its blocks had been silenced.

### v0.13.1 sync (Mac session, 2026-10-09)

Merged upstream `193e299` (v0.13.0 "Given Back" + v0.13.1 "Cut Short", 100 commits) into the fork's
reviewed main `fafe0f6` as toolkit `409c635`; merge-base `ea60cfa`. Eighteen files conflicted (~75 hunks).
Per-hunk inventory, dispositions and the owner's two decisions:
`git show 9d5257f:docs/reviews/upstream-v0.13.1-merge.md` (the record below is the live summary).

- **Executor (owner decision): upstream's `nv2a_pb_exec.c` is the base** — the two files are separate
  rewrites of one base. Upstream's vertex programs, register combiners, depth, near-plane clipping, four
  texture stages and threaded raster come in; the fork's owner commit consumer, present tracker and
  FLIP_STALL publication, L47 flip trace, L48 stage-0 gate, counters/`vp_view` test APIs and
  fixed-function/lighting/AA paths were ported into it. `GET_REPORT` is counted, never written (L44,
  rule 2). The fork's VP interpreter and `RECOMP_VP` are retired.
- **Everywhere else the fork's structure wins**; upstream's lifter/translator/disasm fixes, OHCI and pad
  scripts, window title and build fixes are taken. Not taken: `RECOMP_HEAP_RECLAIM`, `RECOMP_EXT_VMA`,
  `RECOMP_TITLE_KEVENTS`, `RECOMP_GUEST_LOCK`, `KPCR.Irql` publication, APU interrupt delivery, the
  contiguous page-0 skip, #159's per-edge joins.
- **Clean hunks that would have shipped defects**, fixed: recursive `g_heap_lock` acquisitions inside
  `heap_*_locked` (a hang on the first heap free) and a second self-recursive `heap_alloc_locked`; the
  arming call of the deleted `RECOMP_APU_DSP_ACK` (a rule-3 FAIL); `APU_TRAP_BYTES` shrinking
  `RECOMP_APU_TRAP`'s range so the GP DSP model would be bypassed.
- **Verified on macOS:** `tools/posix_check.py` native (12 suites), cross (whole tree compiles and links
  for Windows; the 3 known `d3d8_smoke` failures only) and python (714 passed); structural check clean on
  `409c635` (130 files, 152 switches); rule-3 scan clean; upstream's 13 surviving switches classified in
  `docs/jsrf-run-profiles.md`.
- **Windows gate (2026-10-09, toolkit `a5e2762` = the merge plus a no-behaviour tidy, game `a2f185e`).**
  `just build`; game CTest **49/49**; toolkit CTest **15/15**; the four standalone toolkit test projects
  (`tests/kernel_regressions` 16/16, `nv2a_vsh`, `nv2a_combiner`, `fp_precision` 1/1 each — none is in
  either CTest, so each is configured with `cmake -S tests/<name>`); `just check` all passed.
- **Title-run A/B, 300 s each, one game commit, identical switches:** A =
  `20261009-141651-461-title011-ab-a5e2762`, B = `20261009-142247-172-title011-ab-fafe0f6` (different
  `exe_sha256`). Both reach the deadline with zero `[ICALL] Failed`/`[EXCEPTION]`, `[HEAP] free #1`
  present, the APU trap range present and `[APUWAIT]` 4 in each (R3 also has 4). **A is slower:** presents
  at t = 120 s **960 vs 1750** (B passes the disclaimer transition at ~2427; A ends at 2240, still
  climbing); late re-arms **1587/5745 = 27.6 % vs 56/13363 = 0.4 %**; owner-lock max hold **192 vs 83
  ms**; max pulse gap 204 vs 91 ms. A wrote no more pixels: `[GPU] … pixels written` A 1.14e9 against B
  1.51e9 (that counter undercounts under the threaded raster, per `put_pixel`'s note; A's program-path
  count is 1.32e9), so the cost is per pixel (§26.2). **Owner ruling: a performance regression on a
  non-working prototype is not a revert criterion; the merge stays.** `[FBPRESENT]` lines carry no reason
  field; the presenter's reason is only in `RECOMP_FLIP_TRACE` output.

---

## 2. Generated code

### Regeneration with the v0.12 lifter (owner, 2026-09-28)

The full-pass command is in `AGENTS.md` (it now adds `--backedge-yield`).

- **Analysis inputs are the toolkit's** gitignored `tools/{disasm,func_id,abi_analysis}/output` (dated
  2026-09-21). Every previously generated function is in the toolkit's 8768-entry `functions.json`; 713
  are absent from the game's 8437-entry one (which `relift-selected.py` and `recover-functions.py` use).
  The provenance manifest records the toolkit files as `toolkit:` inputs.
- **Re-applied after a pass:** `relift-selected.py boundaries` (7 reviewed boundary fixes: 5 in generated
  code, 2 in `recovered.c`); the project's exact-delta ABI additions to `recomp_types.h`
  (`recomp_delta_ok`, `recomp_delta_allowed`, `recomp_abi_regs_exempt`, `jsrf_trace_delta_mismatch`,
  `jsrf_trace_seq`, the delta-checking `RECOMP_ABI_CALL`); the six A4b2 `jsrf_watch_store` hooks via
  `scripts/apply-a4b2-hooks.py` (anchors on the guest label or store, refuses an anchor that does not
  resolve exactly once, idempotent; a missing observation hook fails no build, it empties the artifact).
- **Result:** 5580 → 5740 generated functions (+161 switch-arm entries, −1 folded tail); dispatch 8768 →
  8928; zero dropped `rcl`/`rcr`; untranslated instructions emit `RECOMP_UNIMPL`.
- **Two fixes it required.** `sub_00162B9D` (`mov eax, 0x800401F0; ret 0xc`, a COM error tail of two
  recovered parents) is folded by the translator, so it is hand-written in `src/recomp_manual.c` (L05).
  `recomp_unimpl` is defined there per the toolkit template: a no-op reported as `[UNIMPL] … REACHED`;
  `RECOMP_UNIMPL_TRAP=1` aborts at the first one.
- **Comparison** (strict, `RECOMP_APU_TRAP=1`, 8 s; old `20260928-183145-568-crt-divide-fix-strict`, new
  `20260928-185612-449-regen-v012-strict`): same A3a witness, allocation and terminal event; the 18 traced
  DirectSound functions have identical entry counts.
- **Open difference — D3D resource release.** Both builds make the same 116 contiguous allocations
  (ordinal 166). The old build frees 54 through `sub_00192830` (ordinal 171 from `0x001928BB`); the new
  never enters it (trace `20260928-190035-939-regen-trace-192830`, positive control 634 other entries), so
  one of its ten callers decides differently. Which build is faithful is not established.
- **Phase 0 re-baseline on Windows (2026-09-29, game `a62b5ce`→`44becd4`, toolkit `2a349c8`):** game ctest
  26/26, standalone `tests/kernel_data_exports` 5/5 and `tests/kernel_file_status` 5/5, built with
  **Visual Studio 18 2026** (the host has no VS 2022). Regeneration 5740/8928 (0 failed), 653342 lines, 7
  unresolved targets stubbed, 23 unimplemented instructions (17 mnemonics). `FLAGS: 10 conditional(s) in
  7 function(s)`: nine are the jcc-form reads toolkit `ca4257c` predicted, the tenth a `loope` in
  `sub_0010634E`, byte-identical to the pre-regeneration tree; the 8 sites `ca4257c` called live bugs
  (`sub_00015130` `loc_000153A9`, `sub_00130FD0`, `sub_000A0F10`, `sub_001C0B86`) use materialised
  `_fc_*` conditions. Re-verified 2026-09-30 (`20260930-001405-390-v1-verified-strict`). V3/V4: §5, §7.

### CRT 64-bit divide helpers (owner, 2026-09-28)

The old generated tree dropped every `rcr` in the MSVC CRT divide helpers' normalisation loop: old
`__aulldiv` gave `0x2540BE4000 / 0x100000001` = `0x40BE3FFF` (correct `0x25`). They are hand-written in
`src/jsrf_crt.c`, listed in `config/manual-functions.json`, and tested by `tests/test_crt_divide.c` (2116
cases, stack and preserved registers) — the **recovered CRT table**:

| VA | Helper | Result | Preserves |
|---|---|---|---|
| `0x0017C9C0` | `__alldiv` | signed quotient `edx:eax` | `ebx esi edi` |
| `0x0017D4D0` | `__aulldiv` | unsigned quotient `edx:eax` | `ebx esi` |
| `0x0017D2C0` | `__aullrem` | unsigned remainder `edx:eax` | `ebx` |
| `0x001816B0` | `__aulldvrm` | quotient `edx:eax`, remainder `ebx:ecx` | `esi` |

All stdcall `ret 0x10`, dividend low/high then divisor low/high; 24 call sites via `RECOMP_ABI_CALL`.
Removal gate (L04): delete the manual entries and the `jsrf_crt.c` bodies together. `0x17CA70` is
`_allmul` (§8).

---

## 3. AC'97 codec-ready model (packet A3a-r25, accepted 2026-09-24)

**Model** (toolkit `c97ce2c`, `src/kernel/xbox_memory_layout.c`, `nv2a_ack_thread`; L20):
`GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated every tick, atomic, outside the
`g_apu_mmio_trapped` gate; `GC` is read only. It replaced the synthetic `RECOMP_AC97_READY` override.
Witness: `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100`.

**Result.** Strict run `20260924-100502-623-a3a-codec-model` selected `R-PASS`: no relaunch (the baseline
self-relaunched via `HalReturnToFirmware`), the codec poll succeeded (`ret=0x001A7432`), the vector-6 ISR
`0x001A72E0` was connected, and the guest reached DSP initialisation. **Limits:** one strict codec wait
satisfied by modelled state; not faithful hardware behaviour, audio, liveness or boot. W1C on `GLOB_STA`
is not modelled (its consumer `sub_001A71B3` is reachable only from the vector-6 ISR). The packet
`docs/packets/a3a-ac97-codec-model.md` is kept because `scripts/ac2-provenance.py` reads its pins.

**Admission evidence** (secondary-source path of `docs/jsrf-run-profiles.md` §"Unconditional modeled
hardware causes"; no public MCPX/ACI datasheet exists):

- **xemu:** `hw/audio/ac97.c` @ `2799183ecc5119269be01c340d0c9465dbb04d02` (SHA-256 `BD2A95FD…E554B`):
  L65 `#define GS_S0CR (1 << 8) /* ro */`, L135 `GLOB_STA = 0x30`, L785 `val = s->glob_sta | GS_S0CR;`
  (unconditional). `hw/xbox/mcpx/aci.c` @ `704ece9ac661f325aa51bb0b28d326063633227b` (SHA-256
  `9A784826…986D2`): NAM at `+0x0`, NABM at `+0x100`; `hw/xbox/xbox.c:334` instantiates `mcpx-aci`. NABM
  `0x100 + 0x2C/0x30` puts `GLOB_CNT`/`GLOB_STA` at `0xFEC0012C`/`0xFEC00130`.
- **Linux** `sound/pci/intel8x0.c` @ `v6.6` (SHA-256 `F5F1AE46…B5FFC`): L140 `ICH_AC97COLD 0x00000002`,
  L163 `ICH_PCR 0x00000100 /* primary (AC_SDIN0) codec ready */`; L2360–2361 sets `ICH_AC97COLD` when it
  reads 0 to finish the cold reset (bit 1 is active-low `Cold Reset#`); L2295–2298 clears it to re-arm.
- **Limits:** xemu's Xbox-specific part is the address map; its bit-8 behaviour is generic QEMU code with
  a `TODO` for reset requests. Both descend from the Intel AC'97/ICH specification.
- **Guest corroboration (not a source):** `sub_001A6C94` sets `GC` bit 1, clears bits 2–3, then polls
  `GS` bit 8 up to 1000 times — coherent only under the active-low reading.

---

## 4. APU and the GP DSP

**The spin (A4a-r2, 2026-09-24).** The guest writes `3` to the DSP pending word `W =
MEM32(0x001BA858)+0x810` and spins at `loc_001A18D0` (`sub_001A1769`) until it is cleared. Observed
(`20260924-191833-331-a4a-r2-trap-trace`): GPSADDR `0x02040 = 803CC000`, GPSMAXSGE `0x020D4 = 8`, GPRST
`0x3FFFC` written `1` then `3`; SGE[0] points at block `B = 803C0000`, whose `0x5CC` bytes equal the XBE
image at file offset `0x1A7D60` (`0x001BA0A0`) with `3` at `B+0x810`. The word's address varies per run
(`0x803C0810` then, `0x803BC810` on 2026-10-04).

**Port (A4b1-r4, 2026-09-26; L21).** xemu's DSP56300 core vendored byte-exact at
`67cc79e663038d1f55448c0f566b37dde016adf6` (17 files) with 29 marked local modifications; the combined
work is GPL-2.0-or-later (owner approved). The synthetic `RECOMP_APU_DSP_ACK` path was deleted. The APU
library builds with `NDEBUG`, so xemu's `assert(!"Unhandled dsp dma buffer")` is compiled out and an
unimplemented `buf_id` read consumes stale bytes as GP input (now accounted, gated on `is_gp`). Open:
enumerate `NDEBUG`-elided asserts in `src/apu/dsp/` that change GP state or inputs (known: `unk2`/`unk13`,
the `format` default, `dsp_offset` out of range); EP routing.

**The GP clears the pending word (A4b2-r8, accepted 2026-09-27).** In one strict run (`RECOMP_GPU_ACK=0`,
`RECOMP_APU_TRAP=1`) the `3→0` transition at `B+0x810` was performed by the GP engine's memory-write path
executing the validated image, with no synthetic ack or competing CPU zero (`AC-CLEAR`: a `GP_CLEAR` latch
from a successful compare-exchange `3→0`). It depends on no stub: descriptor block 24 (`[GPDMADESC]
GP_CLEAR produced by block_addr=0018`) is all immediates; the `0xFFFFB3` reads feed only loop-control
scratch `x:$007c..$007f`; only image I (`P 0000..0172`) had executed. **Not established:** that the guest
observed the zero or progressed; DSP56300 correctness; GP→CPU interrupts; EP execution. A4b2's P4
condition is toolkit-identity-sensitive and must be re-established before anything inherits it.

**Boot mailbox (2026-10-04).** `20261004-174418-621-f6-fatal-caller` (1200 s) stopped with the word still
3, main thread in `sub_001A1769` at `loc_001A18D0` holding critical section `0x1BA050` (acquired by
`0x19E438`, `RtlEnterCriticalSection` ordinal 277). `20261004-182724-545-f7-apuwait` (toolkit `712f70d`)
did not stick: `GPRST=3`, `gp.realtime=1`, `GPSADDR=0x003C8000`; the word was 3 at frame-thread call 225
and 0 at call 226, after 84326 GP cycles. When GPRST is already 3 and the frame thread runs the GP, the
mailbox clears within one audio frame; why f6 stayed at 3 is not established.

### `PIO_FREE` (`0xFE820010`)

- **Gate-only at the 28 direct reads (A4p-r1, 2026-09-24, `O-GATE`).** Every direct read in `DSOUND` is a
  threshold re-poll whose value reaches no use, under an explicit x86 calling-convention premise (C1:
  `ecx`/`edx` dead after calls; C2: register-argument checks at calls, one level; C3: `ecx` dead at `ret`,
  `edx` dead unless a caller reads it). The population came from the XBE, **reconciled by normalised
  value: 10 hex-spelled + 18 signed-decimal-spelled sites** — a one-spelling grep found 10 of 28
  (`AGENTS.md`, §6). Limits: direct reads only; not computed access, timing, or whether `0x80` is the true
  device value.
- **It does not contaminate A4b2:** stub dependence is judged one wait and one value at a time; the
  `PIO_FREE` answers decide whether and when the guest reaches the spin, not the data the GP acts on.
- **Deferred at `O-OPEN` (L23).** The `0x80` stub passes all 13 constant gates (two, `0x001A3EB3` and
  `0x001A3FDB`, at exact equality) and is unproven for 15 variable gates (demand `k × byte[+0x64]` needs
  byte ≤ `32/k`). Reopen only on admitted hardware source evidence, or a strict claim traversing named
  gates.

---

## 5. The A2h line: the strict terminal event

**Closed 2026-09-30.** The strict horizon was the kernel thunk table being overwritten by a misdispatched
image copy; fixing two alias folds moved a strict run from a fault at ~6 s to its 93 s deadline.

- **The table.** `0x001C3F60..0x001C413F`, 120 slots of 4 bytes, the head of `.rdata` (`VA=0x001C3F60
  vsize=161792 raw=0x001B4000`). In the original XBE every slot holds an `0x80000NNN` kernel thunk —
  `0x001C4064` = `0x80000115` = ordinal 277 (slot 65) — and **slot *N* is at `0x1C3F60 + N*4`** with
  ordinal `value & 0x1FF`. At runtime the slots hold the installed `FE000000 FE000004 FE000008 …` thunks
  (`[0x1C4064]` = `0xFE000104`).
- **The terminal event was the whole table overwritten**, after which whichever thread next called
  through any thunk raised `0xE0424943`. The table held a 40-byte-stride record array (index field `0x79`
  at `0x1C3E08`, `0x80` at `0x1C3F18`, `0x88` at `0x1C4058`; constant fields `0x3E800000` (0.25f),
  `0x41200000` (10.0f), `0xFFFFFFFF`, `1`, `0x001FA1D8`); 0 of 120 slots patched, 31 holding `0`. Every
  invalid target was a record field at the slot its call read:

  | Slot | Slot VA | XBE value (ordinal) | Read as | Failing call returns to |
  |---|---|---|---|---|
  | 46 | `0x001C4018` | `0x8000009F` (159) | `0x00000001` | `0x0018CE73` |
  | 65 | `0x001C4064` | `0x80000115` (277) | `0x00000000` | `0x0014982E` |
  | 68 | `0x001C4070` | `0x8000007C` (124) | `0x41200000` | `0x00147D36` |
  | 70 | `0x001C4078` | `0x800000E7` (231) | `0x00000000` | `0x00147DBC` |
  | 71 | `0x001C407C` | `0x800000E0` (224) | `0x3E800000` | `0x00147DE2` |

- **The first fault site varies run to run; that is the race, not the horizon** (D4). Six strict V3 runs
  on one build gave four first sites: `0x00149828` 3/6, `0x00147D30` 1/6, `0x00147DDC` 1/6, `0x00147DB6`
  1/6; a worker thread can die before main. The 2026-09-28 baseline
  (`20260928-185612-449-regen-v012-strict`) holds the identical array.
- **The writer (F1).** `sub_00038530+0x398` (`rip=exe+0x5361B8`), a `rep movsd` image copy
  (`20260930-221404-630-f1b-rdata-guard`, `RECOMP_RDATA_GUARD=1`, L32; all 256 reports, the guard's cap,
  from tid 24300, starting at `0x00011000`). The dump at guest VA `V` equals the XBE at `V + 0x37608`
  (431 of 436 sampled pages `SHIFTED`, 0 original); the 128 bytes at `0x001C3F60` are byte-identical to
  the XBE's `.data` at `0x001FB568`. Shifted provenance establishes byte origin; it is never a correction
  (`AGENTS.md`).
- **The cause (F3, D5).** Two entries of the function-pointer table at `.data 0x001EC0F8..` had been
  folded into abutting neighbours by the translator's `tail_jump_alias` rule, their bodies deleted, so a
  call through the table entered an unrelated function (`[ALIAS-ICALL] target=0x00037550
  owner=0x00038530`, 19 lines before the first guard write):

  | Address | Table slot | Folded into | Its own code ends | Its jump table |
  |---|---|---|---|---|
  | `0x00037550` | `0x001EC108` | `sub_00038530` | `0x00037603` (bare `ret`) | `0x00037FB4` |
  | `0x00026780` | `0x001EC10C` | `sub_000278F0` | `0x00026816`+epilogue | `0x0002730C` |

  `0x00037550` occurs once as a pointer, has 0 direct callers, and the database gave it
  `tail_jump_alias`, `confidence: 0.88`, span `0x00037550..0x00038530`. Its `stack_args: 0` is correct:
  the decoder reaches only the bare `ret` at `0x00037603`; the `ret 4` at `0x00038525` belongs to
  `0x00038460`/`0x0003848E`; the runtime ABI check passed.
- **The fix.** A recovery entry for each address, span ending at the function's own jump table
  (`recomp_lookup_manual` consults `jsrf_lookup_recovered` before the alias table), plus two spans
  tightened to their `ret` (`0x000BCF40` end `0x000BD8D0`→`0x000BD8B0`; `0x001063A0`
  `0x00106580`→`0x00106560`).

  | Run | Outcome | Evidence |
  |---|---|---|
  | `20260930-221054-913-f0b-first-run-new-toolkit` (before) | `0xE0424943` at 5.8 s | table held the record array |
  | `20260930-225440-580-f3-alias-fix-strict` (`0x00037550` fixed) | `0xC0000409` at 14.6 s, `[RECOVERED] ABI FAILURE 0x00026780` | the next slot in the same table |
  | `20260930-225739-446-f3-alias-fix-2-strict` (both fixed) | `diagnostic_deadline` at **93.0 s** | **0** invalid ICALLs, **0** `0xE0424943`, **0** exceptions, **0** ABI failures, **0** `[UNIMPL]` |

  Dump gate after: `matches: 1, content-mismatch: 0`; `.text` at `0x00011000` is `852C518B 30418BD2 …`,
  byte-identical to the XBE. Every archived dump before the fix is `CONTENT_MISMATCH`.
- **The ~571 MB allocation failure** before the fault (`NtAllocateVirtualMemory`, `0xC0000017`) is handled
  by the guest (`0x00149E56 test eax,eax` / `jl 0x149eec`); it was not the cause.
- **Phase 0 V3 runs** (game `44becd4`, toolkit `2a349c8`, 8 s, `RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1`):
  `20260929-231110-868-rebaseline-strict` (STRICT, `0x00147D36`), `20260929-231200-429-rebaseline-kmem-legacy`
  (EXPLORATORY, `RECOMP_KMEM_LEGACY=1`, `0x00147DBC`), `20260929-231211-023-rebaseline-gmeter` (STRICT,
  `0x0014982E`). The strict two print `[KMEM] reject` `kind=reserve_failed base=0x00000000 size=0x23B20430
  type=0x801000 status=0xC0000017`. **All three print the same ten kernel data exports (L10): ordinals 16,
  40, 156, 164, 259, 322, 323, 325, 354, 356 (slots 15, 30, 62, 58, 67, 87, 38, 39, 40, 55 →
  `0x00740000..0x007404A0`).** `[GMETER]`: `max=4 inside=1 contended=2954 host_kcalls=2 anomalies=0` (D4).
- **V5 recovered-functions audit (2026-09-30):** of the then 3074 entries, **122 OBSOLETE** (58 under the
  default pass, 64 only under `--coalesce-functions`), **2952 STILL_NEEDED, 0 UNKNOWN**. OBSOLETE means "a
  body is emitted", not "the span is identical" (53 exact, 66 wider, 3 shorter; `0x00162AB0`'s emitted
  body overlaps `sub_00162A20` by the `sub_00162B9D` tail). None retired; a retirement packet must
  re-check each span.
- **The A2h slot watch was a different address:** `g_Device+0x242C` = `0x0019D62C`; its
  `last_write=001D5078` (`20260928-121142-929-a2h-attrib-exp2-3b`) is a pointer to `"djv000_0.adx"` in an
  `.rdata` table of 59 code→filename entries, not the terminal event; the slot's installer is
  `sub_0018CE30` (§7). Instrument facts: classify a native RIP by module range; a linker-map symbol is
  right but its offset is not guest-meaningful (generated C is ~5.5× the guest size); `dr_disarm_all`
  zeroes DR0/DR7 on every live thread, so its disarm count is not an arm count.

### F4/F5: the first frames and the HDD cache fill (2026-09-30 – 2026-10-01; exploratory)

- **F4.** The walk stopped on `unsupported_method` `0x1720` (`NV097_SET_VERTEX_DATA_ARRAY_OFFSET`) at
  `get=00008EF0` (`20260930-230206-594-f4-frames-after-horizon-fix`); toolkit `1f9309a` admitted seven
  measured methods (`0x1720`, `0x172C`, `0x1730`, `0x1744`, `0x1800`, `0x1804`, `0x1808`; `0x1724` still
  rejects) and made the table the union of the named rings (L39). Next `sink_capacity`: 1364 words, 259
  packets, 1109 methods against `sink[1024]`; toolkit `e8a6e03` sized staging to the 4096-word budget (L40;
  units since, §23.4; ruling `docs/reviews/rulings/f4-submission-capacity.md`).
- **Architecture A (toolkit `a71f937`, L18).** Under the MMIO state owner the legacy GPU body never ran, so
  `RECOMP_PB_EXEC`/`RECOMP_PB_SCAN`/`RECOMP_NV2A_TRACE` were inert. The core now calls a seam `(subch,
  class_id, method, param)` for every committed entry after `action_commit`; the kernel wrapper filters
  NV097; `0x0130` counts `flip_stalls`, `0x012C` is `s_gpu.flips++`. First frames:
  `20261001-033129-276-f4-a-smoke-60s` drew the "Presented by SEGA®" logo.
- **F5: the logo phase is the first-boot HDD cache fill** (`\Device\Harddisk0\Partition5\Media\…~`, ~1.0–1.3
  distinct files/s with or without the executor). F5 D2 (toolkit `a826201`,
  `git show 9d5257f:docs/packets/f5-directory-context-close.md`): directory contexts were never released at close, so the
  65th query failed with `0x80000006`; `xbox_dir_context_release` now runs at both `xbox_NtClose` sites and
  at `bridge_NtClose` (which does not go through `xbox_NtClose`). Open risks: the lazy
  `InitializeCriticalSection` race, and the query using `ctx` outside the lock after lookup.
- **Main-thread facts:** `0x0019E438` = `DirectSoundEnterCriticalSection` (CS `0x001BA050`), `0x0019F260` =
  `DirectSoundDoWork`; `0x001BA04C` is DSOUND state (one write, `mov [0x001BA04C],1` at `0x001A2317`).

---

## 6. Method lessons that apply beyond one packet

- **Enumerate by value, not spelling.** The lifter spells one address two ways (see `AGENTS.md`); a
  one-spelling grep found 10 of 28 sites.
- **Linear disassembly drifts** at the first data island; completeness claims need a recursive-descent
  walk (the A2h analysis used 30,118 seeds → 673,726 instruction starts). **Aligned dword scans miss
  unaligned immediates** (`mov dword ptr [esi], imm32` put `0x001E1270` at a non-4-aligned address);
  raw-byte scans are the fallback.
- **No hand counts in decision inputs:** tool-computed, from a named artifact, with a positive control and
  loss accounting.
- **Attribute a terminal event** by reading the instructions past the failing call, never by temporal
  co-occurrence.
- **Read what ships:** build configuration (`NDEBUG`), line endings (`core.autocrlf` changes hashes of
  checked-out files), and which analysis database actually produced generated code.

---

## 7. Fork audit and owner-directed toolkit fixes (2026-09-28/29)

Toolkit `db96e30..2a349c8`, first built and run on Windows in Phase 0 (§2). **Fork audit:** all 23 forks of
`sp00nznet/xboxrecomp` and two second-level forks compared by content against `db96e30`. Reuse bounds:
DanielJVoxSmart is GPL-3.0 since `88cde2c` (ideas only); Tiptup300's semaphore code is from an unnamed
source (ideas only); many NoRain211 commits carry an "Antigravity" bot identity. DanielJVoxSmart's own
JSRF bring-up stalled in `sub_001497DC` on two root causes already fixed here (flags lost across `lock
xadd`, so every COM `Release()` destroyed the object; the missed function `0x00154DAA`).

**XDK D3D device fields (Phase 0 V4).** From `~/src/halo-ce-universal` (Halo CE, XDK ~3911; `libs/d3d8` is
GPL-3.0 and RXDK-derived, **facts only, no code**): `libs/d3d8/device_layout.h:180-182`,
`d3dbase.cpp:142`. The miniport context starts at device `+0x2268`; `g_Device` is
`0x0019B200`–`0x0019DCE0` (`+0x2AE0`), pointer at `[0x19DCE0]`.

| JSRF device offset | XDK field (Halo name) | Verdict | Bytes |
|---|---|---|---|
| `+0x242C` | `m_pVerticalBlankCallback` | **CONFIRMED** | `0x0018CE30: mov eax,[esp+4]; mov ecx,[0x19DCE0]; mov [ecx+0x242C],eax; ret 4` — the only store to `+0x242C` |
| `+0x2430` | `m_VerticalBlankEvent` (KEVENT) | **CONFIRMED** | `0x0018CE67: add eax,0x2430; push eax; call [0x1C4018]` — the ordinal-159 `KeWaitForSingleObject` thunk; log `ordinal 159 (slot 46) … ret=0x0018CE73` |
| `+0x2434` | that event's `Header.SignalState` | **CONFIRMED** | `0x0018CE5B: mov dword ptr [eax+0x2434],0` |
| `+0x2440` | `m_BusyBlockEvent` | **INFERRED-LOCATED** | `0x00191497: mov [edi+0x2444],ebp`; `0x00191501: lea esi,[edi+0x2440]` → `0x00191519` wait via the same thunk; base from `0x00191446 mov edi,[0x19DCE0]`; never observed executing (`ret=0x0019151B` 0 times) |

Role names, each confirmed by its bytes: `0x0018CE30` `SetVerticalBlankCallback`; `0x0018CE50`
`BlockUntilVerticalBlank` (clears `+0x2434`, waits on `+0x2430`); `0x00193D90` `CMiniport::VBlank` (`call
0x193C40` = `rdtsc`; `[esi+0x208]`, `[esi+0x20C]`, `[esi+0x1F4]` — frame counter and time base);
`0x00194210` `ServiceGrInterrupt` (clears PGRAPH `0x400720`; reads `0x400100`, `0x400704`, `0x400108`);
`0x00193F70` `SoftwareMethod` (`jmp dword ptr [edx*4+0x1941B4]`, eight in-module targets). A KEVENT field
need not have a literal-offset store (`+0x2430` has none). `sub_00038530` does **not** write `+0x242C`.

**Corrections that stand.** The lifter lowers guest `rep stos`/`rep movs` to host `memset`/`memcpy` (95
`memset(` and 464 `memcpy(` in `src/recomp/gen/recomp_000*.c`; 114 and 652 with `recovered.c`), so a host
CRT RIP is usually guest code. `0x1B00/0x1B04/0x1B08/0x1BC8/0x1BCC` are texture-stage methods. The game
has implicit function declarations of its own (`xbox_inb`/`xbox_outb` in generated chunks,
`recomp_dispatch_init` in `src/main.c`, `recomp_delta_allowed` in `src/diagnostics.c`, `dr_tid_exited` in
`tools/harness/collect.c`, `sub_00193D10`/`sub_00196C83` in `tests/test_recovery_11c1.c`); the toolkit
builds with `/we4013`, the game must not until these are declared.

| Commit | Change | Strict-path effect |
|---|---|---|
| `c4adb9b` | BearddOddity `pr-a-small-fixes`: 64-bit per-thread kernel call counter, pseudo-handle sign extension, `STATUS_CONFLICTING_ADDRESSES` → `ERROR_INVALID_ADDRESS`, OHCI DATA UNDERRUN | handle/USB/log fixes |
| `a253876` | BearddOddity `pr-b-pushbuffer-executor`: CPU executor (FFP, vertex programs, lighting), DMA-engine walker | under `RECOMP_PB_EXEC` only |
| `cde1ccb` | Revan67: a DPC queued during a drain runs on the next timer pass | a self-requeueing DPC can no longer spin the timer thread |
| `4b4a62d` | All 34 kernel DATA exports patch to backed data; 88, 89, 102, 120, 154, 162, 240, 245, 249, 321 were function thunks; their old storage overlapped the IDE channel object at `0x500` | thunk values change for those ten if imported; 277 and KeTickCount unchanged; 120/154 set once, not advanced |
| `1534c4a` | `NtCreateFile`: host `ERROR_FILE_EXISTS` → `STATUS_OBJECT_NAME_COLLISION` | that one failure code |
| `305efd1` | Guest concurrency meter, `RECOMP_GUEST_METER=1` | observation only |
| `093174c`, `787b7d7`, `1d85934` | Heap blocks split on reuse; `MmFreeContiguousMemory` frees in the contiguous arena; reservations tracked as regions with NT semantics (reserve hint honoured or refused with `STATUS_CONFLICTING_ADDRESSES`, commit must land in a region, `NtFreeVirtualMemory` reads 32-bit guest values); `[KMEM] summary` | **yes** — allocation addresses and failure codes change; `RECOMP_KMEM_LEGACY=1` restores the old behaviour for A/B |
| `123ca65` | One SRW lock around every heap entry point | removes an unsynchronised shared table |
| `a9188d9`, `b6cea28`, `e8972a1`, `69842cf`, `79a0070`, `caeee80`, `73974ab`, `f61a0af` | `/we4013`; narrow 8/16-bit `mul`/`imul`/`div`/`idiv` (4 JSRF sites, e.g. `sub_000307F8`); jump table from slot 1 when slot 0 is unusable; `movsx` from `bp`/`sp` (`0x0005D58D`); alias indirect calls counted (`[ALIAS-ICALL]`, first 8); runtime template declares `xbox_in*`/`xbox_out*` | at regeneration |
| `ca4257c` | A join whose predecessors disagree computes its condition per edge; leftover `_flags` reads reported (`FLAGS:`), `--strict-flags` fails on them; 8 of 17 annotated sites were live always-false branches; the 144 bare `if (!_flags)` are REP-compare loops and correct | at regeneration |
| `822c9de` | D3D11 translator keys on real NV097 method numbers | none for JSRF |
| `6864f1f`, `30d7322`, `aa650ca` | Dormant unless `RECOMP_NV2A_ACTIONS` is exactly `1`: semaphore release (`0x1A4`, `0x1D6C`, `0x1D70` written only if the stream commits); non-zero NOP trap with PGRAPH `DEBUG_3` bit 20; `0x1D8C`/`0x1D90` → `0x401A88`/`0x40186C`; `FLIP_STALL` holds while READ == WRITE, released by the guest's `0x40071C` write | none when unset (6000-stream differential driver matched) |
| `2a349c8` | Admission evidence: `docs/technical/nv2a-action-methods.md` | — |

Against `docs/jsrf-run-profiles.md` §"Admission criteria": semaphore release fails criterion 4 (it can
stand in for rendering the strict model does not do); NOP trap meets 2–4 except two points where the
sources disagree; FLIP_STALL meets 3–4.

---

## 8. Disc-error timeout and the boot clock (2026-10-05)

The pending-I/O path in `0x25400` is a 15-second wall-clock timeout (corrected 2026-10-09). `0x145560` is `rdtsc` into an 8-byte guest
buffer; `xbox_ReadTimeStampCounter()` scales QPC to `XBOX_TSC_HZ` (733,333,333), the same value the title
stores at `0x145571` (`0x2BB5C755`; held at `[0x20CC50]`). `0x6E910` (`ret 0x14`) subtracts two samples,
multiplies by `0xF4240` via `0x17CA70` (`_allmul`) and divides by the frequency at `[ecx+8]` via
`0x17C9C0` (`_alldiv`), `ecx = 0x20CC48`: microseconds of wall time. `0x25400` takes the fatal call at
`0x255AD` only when `[esi+0x64] == 0x103` (`STATUS_PENDING`, written by `0x146078`) and the quotient is ≥
`0xE4E1C0` (15,000,000 µs, i.e. 15 s; L56 disables it); start sample `[esi+0x188]`/`[esi+0x18C]`. `0x25310` compares the same threshold
before its tail jump to `0x6F730`; the path string is at `[esi+0x78]`, and `~` from `[0x1C4DF0]` is
appended for the side file. L41 observes `0x6F730` and the `0x2537E` tail (§26.4).

## 9. The alias-fold misdispatch class: wrong return value → corrupt loop bound → wild write (2026-10-05)

`20261005-011634-413-f23-7da30`: `0x9CC40` calls dispatcher `0x25700` (`cmp edx,0x21`; `mov
edx,[edx*4+0x1EC200]`; 3 stack args); index 1 is `[0x1EC204] = 0x00032610`, folded into `sub_00033800`
(`mov eax,1; ret 4`). The `1` became `[obj+0x38]` of `0x034D5110`, `0x15D90`'s loop count `0xFFFFFFFF`
(outer 31,739), and normalised zero vectors the SSE quiet NaN `0xFFC00000` over `0x233ED0..0x28ED04`
(93,057 of 93,069 words; `0x231D40 + 12 × 31,739 = 0x28ED04`), overwriting the `DOLBY` image
(`0x27E080`), `0x251D6C` and the thread trampoline (`mov eax,[0x25efb8]` then called `0xFFC00000`).
**Path-aware criterion:** a run counts only if it logs `[RECOVERED] 0x00032610 returned`, no `ALIAS-ICALL
target=0x00032610`, no NaN at `0x27E080`/`0x25EFB8`, and stops beyond `0x9CC40`. The dispatch carries
**134 alias tuples**; data-referenced unrecovered aliases: `0xE9A40` (`0x1CEE84`), `0x102700`
(`0x1D1DD4`), `0x1199C0` (`0x1D7B6C`), `0x13A340` (`0x1DEA64`) in `.rdata`; `0x40002`, `0x100AB0`,
`0x1A2078`, `0x1BD800`, `0x1C3800` in `.data`; `0xB090B` only from `$$XTIMAGE`.

## 10. Stack-contract contradictions are statically decidable, and 20 were live (2026-10-05)

`scripts/check-stack-depth.py` walks every manifest entry's reachable CFG with `d = ESP − ESP_at_entry`
(`push` → `d -= 4`). The wrapper asserts `g_esp == before_stack + 4 + stack_args` and a `ret N` leaves
`esp = entry + d + 4 + N`, so **`stack_args = N + d`**; the gate fails on any reachable `ret N` at **`d =
0`** whose `N` differs. The general form is not gated: all **29** entries with a resolved nonzero-depth
`ret N` have `d > 0` (min 4, max 100) — an extent question. A `call` contributes its callee's own `ret N`;
an unresolvable callee makes the depth `UNKNOWN`. Repaired: `0x00021010` 0→**16**, `0x000F4FF0` 0→**4**,
`0x00102490` 0→**4** (`0x1025B0` recovered, `stack_args 0`), `0x00152BC0` 8→**24** (`0x152DE0` recovered,
`stack_args 8`), and 20 entries declared 0 whose bodies end `ret 4` (`ret 0x14` for `0x00080028`):
`0x246E0`, `0x42CA0`, `0x80028`, `0x86180`, `0xA5050`, `0xCD890`, `0xD03F0`, `0xD62A0`, `0xDB820`,
`0xE2050`, `0xE2A00`, `0xE3700`, `0xEDA10`, `0xF4C60`, `0xF8AF0`, `0x11B660`, `0x120400`, `0x124B00`,
`0x134D50`, `0x139B30`. Control `0x1BCB14` starts mid-function, `d = +8`, `4 + 8 = 12` = its declared
value, run-verified in 106 runs; never gated. Other classes are `SUSPICIOUS` (`RET_DEPTH` per path,
`FALL_OFF_END`, `CUT_EPILOGUE`, `TRUNCATED`). Tests: `--selfcheck`, `tests/test_stack_depth.py` (CTest
`jsrf_stack_depth`). `check-generation-provenance.py --write` erases the hand-maintained
`amendments`/`regenerations` history; re-attach it from `HEAD`.

## 11. The over-wide `tail_jump_alias` record is the dominant stop class (2026-10-05)

Stops 17–19 were complete functions with no database entry of their own, inside or beside an over-wide
`tail_jump_alias` record: `0x94AB0` (gap `0x94AA3..0x95FC0`; three plain `ret` at depth 0; `stack_args 0`
PROVED); `0x496E0` (in `sub_00049520 [0x49520, 0x4A6F0)`; body `0x496E0..0x497D6`, one `ret 8`;
`stack_args 8` INFERRED from the frame `push esi` `0x496E7`, `push edi` `0x49744`, `pop edi` `0x4977B`,
`pop esi` `0x497B7`); `0x5C840` (before `sub_0005C990`/`sub_0005CB90`, both `[0x5C990, 0x5D3B0)`; one
reference, `.data` `0x001FA1C0`; `stack_args 0` INFERRED). g05 (`20261005-185514-638-g05-confirm`)
confirmed `0x496E0` and `0x5C840`. The detector for unresolvable table targets is
`scripts/check-table-targets.py`.

## 12. The missing-entry population is a table-level defect, and the detector is blind to part of it (2026-10-05)

- **The sweep, from the XBE:** 4117 aligned `.data`/`.rdata` dwords land in `.text`; 3632 already resolve
  at runtime, 485 do not, **117** pass the boundary filter (53 swallowed by a manifest span, 10 by a
  database span only, 54 UNCOVERED gaps).
- **The noise filter:** 24 of the 117 are noise — 13 have a padding byte (`0x90`) at the candidate VA (ten
  from one 16-bit word array at `.rdata:0x0022E1B4`), 11 are not 4-byte aligned. Requiring "not a padding
  byte at the VA" and "4-byte aligned" takes **117 to 93**: an unaligned dword value is not an entry.
- **Traps and misdispatches are two populations; only one is censused.** `check-table-targets.py` filters
  on `runtime_starts()` (does the address resolve at all) — correct for traps. A folded
  `tail_jump_alias` resolves to a shim that runs a different function, e.g. `static void
  recomp_alias_000E9A40(void) { recomp_alias_observe(77u); sub_000E9D80(); }`; all 134 shims call a `sub_`
  other than their own address. With `genuine_starts()` the sweep gives 121; the four extra are
  `0xE9A40`, `0x100AB0`, `0x1199C0`, `0x13A340`. The actionable subset is aliases whose own address is a
  genuine function; the rest are mid-body labels for which running the parent from its start is correct.
  So the **MISDISPATCH** population needs its own detector (§14 reports it, ungated).
- **Over-wide existing entries:** `0x7DBD0` (190 instructions) swallowed by `0x7DAE0–0x7DE20`, whose end is
  also the next database start; `0x1FF90` (own `ret 4` at `0x1FFE7`, declared end `0x200A5`, swallowing
  `0x1FFF0`); `0x91C00` (own `ret` at `0x91C23`, declared end `0x91EB0`, swallowing `0x91C30`, `0x91D70`).
  "End at the next function start" is necessary but not sufficient.
- **Whole tables miss members:** `0x001F97B4` 6 of 13 entries are database starts; `0x00215530` 40/46;
  `0x0020D8A8` 24/27; `0x001EC288` 26/33; `0x001CD2E0` 92/96; `0x001CA6C8` 61/64; `0x001CAC60` 78/80;
  `0x001EBCF4` 24/26; `0x002165F0` 25/29; `0x001CD7F8` 47/48.
- **Discriminator:** a candidate reachable from its container's own entry is an internal label; of the
  117 only 2 are, both in bogus `gap_prologue` containers (`sub_00028826`).

## 13. The this-adjusting thunk: a fourth missing-entry shape, and an observed stack_args (2026-10-05)

Stop 20: `[RECOVERED] ABI FAILURE 0x00154540 esp 00F7FD30->00F7FD40 expected +24`. `0x154540`/`0x154520`
are adjustor thunks (`add edx,2`/`6`; `jmp dword ptr [ecx+0x6c]`), so `stack_args` equals the tail
target's cleanup: object `0x0106C870`, vtable `0x001E0F00`, method `0x00154420` with four `ret 0xc`; 4 +
12 = 16 = measured. Both corrected 20 → 12; the 20 came from the swallowed `0x154560` (`ret 0x14`),
recovered as `[0x154560, 0x1548DC)` `stack_args 20`. A `+0x6c` slot's cleanup is not a rule (126 vtable
runs: 0 ×64, 4 ×15, 8 ×3, 12 ×2, 20 ×2, 40 unresolvable).

## 14. The hidden-entry detector: a span can be over-wide while its `end` is "correct" (2026-10-05)

`scripts/check-hidden-entries.py`: a manifest span must not extend past its own reachable body into an
address with independent evidence of being a separate entry. The body must be fully enumerated (no
truncation, no fall-off, no `indirect`/`terminal` exit) — not caller absence (`0x80BD0` has zero rel32
callers and is real). Evidence: an aligned `.data`/`.rdata` dword landing there, or another manifest
start, passing §12's filter.

| verdict | meaning | gate |
|---|---|---|
| `HIDDEN_ENTRY` | the consumed address resolves nowhere | **FAILS** |
| `OVERLAP` | it is another manifest entry | **FAILS** |
| `SHADOWED` | it already has its own generated body | **FAILS** |
| `MISDISPATCH` | it resolves to a *different* symbol | reported |
| `OVER_RUN` | the over-run holds no evidenced entry | reported |
| `UNQUALIFIED` | the walk has an opaque exit, so separation is not provable | reported |

No baseline. First census: 50 containers, 63 consumed addresses (45 `HIDDEN_ENTRY`, 5 `OVERLAP`, 2
`SHADOWED`, 2 `UNQUALIFIED` — `0x96F60`/`0xAEE80`; 265 `OVER_RUN`); 50 spans tightened, 48 entries added
(38 PROVED, 10 INFERRED). Guards: additions keyed by their own start; `SHADOWED` read from
`recomp_dispatch.c` (the `0x556D0`/`0xC42E0` LNK2005); `repair-hidden-entries.py:resolve()` rejects a bound
whose exits tail to a non-boundary (`0xB3C30`, real end `0xB3DD4`).

## 15. Stop 20 is runtime-confirmed, and g07 exposed the mirror-image span defect (2026-10-05)

g07 (`20261005-211627-927-g07-thunk`) logged `[RECOVERED] 0x00154540 returned; ABI verified`, then
`[ICALL] Failed to resolve VA 0x000B5F82`: `0x000B5EB0` had been tightened to `[0x000B5EB0, 0x000B5F3A)`,
cutting its own 9-arm jump table at `0x000B5F0F`, so the lifter emitted `RECOMP_ITAIL`. Restored to end
`0x000B6732`; the false split `0x000B5F3A` removed with `config/generated-patches.json`
`remove-b5f3a-dispatch` (**L02**). This is the **under-wide** class, the mirror of §14.

## 16. A host crash this turn shipped, found by a control rather than a gate (2026-10-05)

`remove-b5f3a-dispatch` left `g_recomp_table_size = 8928` over 8927 rows, so `recomp_dispatch_init` read
past the array (`0xC0000005` at `recomp_dispatch_init+0xC3`, before `guest_entry`). Fixed by patch
`fix-dispatch-table-size` (L02). **Rule:** a patch that removes an entry from a generated table corrects
its declared count in the same change; `check-dispatch-table.py` counts comment-stripped rows (§18).

## 17. The Advisor ruling, and two measurements it corrected (2026-10-05)

- **Ledger erratum:** commit `64945a3`'s message gives `remove-b5f3a-dispatch` ledger `(L42)`; it is
  **L02** (`scripts/patch-generated.py` only checks that an ID exists). Not amended; this is the erratum.
- **The 192/234 "fatal stub" count was a spelling count.** `g_seh_ebp = ebp; sub_<va>(); return;` in
  `RECOMP_ITAIL` bodies is also how a normal recovered call is emitted; intersected with production trap
  definitions it is **27 bodies / 50 distinct targets**.
- **`0xFC370` is a two-level selector map** (`cmp eax,8` at `0xFC387`, nine selector bytes at `0xFC490`,
  three dword slots at `0xFC484`); `0x20200` is selector data. A guard must be tied to the actual index
  value at the jump, including any remap.
- **Accepted rulings:** no gate on "out-of-span arm" alone; a *certified lost continuation* gate is backlog
  with its proof rule (reachable guard-to-jump path; the guard tests the same index value through any
  remap; no intervening clobber; the default edge bypasses the jump; exactly the reachable slots read from
  file-backed bytes; else `UNPROVEN`); the broad census stays visible
  (`tests/test_recovery_span_ownership.py` `KNOWN_OPEN` is not closure).
- **`0xB06E0` is a proved static production defect** (stop 23): `push esi; mov esi,ecx; mov
  eax,[esi+0x128]; cmp eax,-1; je 0xB09DC` … `0xB09DC pop esi; 0xB09DD ret 4`. Its span `[0xB06E0,
  0xB0811)` ended at a `tail_jump_alias` start inside its own body, so the epilogue lay outside it and the
  emitted body called the fatal trap stub `sub_000B09DC` **twice** and `sub_000B09D9` **four** times
  (counts corrected in §18). Since repaired by F7c (end `0x000B09E0`, `stack_args 4`).
- **The `0x200A5`/`0x200A8`/`0x200AD` micro-entries** contained by `0x1FFF0` are identity-unproven.

## 18. Errata and the under-wide class's own census (2026-10-06)

- **Erratum:** the `0xB06E0` stub counts are `sub_000B09DC` **twice** (`0xB06EC`, `0xB06FF`) and
  `sub_000B09D9` **four** times (`0xB0720`, `0xB0743`, `0xB07CC`, `0xB07D8`), re-measured from
  `logs/runs/20261005-223404-818-g08-b5eb0-fixed/source.zip`; corrected in §17 and `config/stop-chain.json`
  row 23 (commit `892dd1e`'s message had them reversed). The hidden-entry detector is for over-wide spans
  and returns `CLEAN` when a walk falls off its end; `check-stack-depth.py` saw `0xB06E0` only as
  `SUSPICIOUS/CUT_EPILOGUE`.
- **The under-wide census:**

  | measure | count |
  |---|---|
  | entries whose own fully-enumerated walk reaches a **fatal** trap stub | **128** |
  | entries where a **conditional** branch leaves the span to an address that is neither a manifest start nor inside any genuine span, and extending to the next manifest start certifies one single `ret N` | **150 PROVED, 4 INFERRED** |
  | of the certified set, entries whose certified `N` disagrees with the declared `stack_args` | **10** |

  Unconditional out-of-span `jmp`s are a measurement, not a gate. First prediction confirmed:
  `0x000AE560-0x000AE5F1 -> end 0x000AE659 N=4`, hit in `20261006-003520-133-f9-underwide-batch-pb`
  (`0xAE655` is the shared epilogue). That run also confirmed stops 21 (`0x000B5EB0`) and 22
  (`0x00048DB0`). F7c repaired 152 and F7d 7 (§20); disagreeing-`N` candidates were skipped.
- **Confirming-role rules (L43, `check-run-exercised.py`):** a return line is not exclusive with failure
  (`20260930-225440-580-f3-alias-fix-strict` logs both for `0x00026780`), so a confirming run must have no
  ABI-failure line for the address and must not set `JSRF_ABI_CONTINUE`; and the run's archived `(end,
  stack_args)` (from `source.zip`) must equal the current one.
- **Dispatch gate:** `check-dispatch-table.py` counts comment-stripped rows; controls are the archived
  crash trees `20261005-222801-316-g08-b5eb0` and `20261005-222917-439-g08b-repro` (8928 declared, 8927
  rows).
- **Latent, not live:** ten addresses have both a recovered body and an alias shim to another symbol
  (`0x00027B00→sub_00027CD0`, `0x0002C360→sub_0002D1E0`, `0x00032610→sub_00033800`,
  `0x00032C70→sub_00033800`, `0x00033C50→sub_000355B0`, `0x00034200→sub_000355B0`,
  `0x000348A0→sub_000355B0`, `0x00035640→sub_000360D0`, `0x00037550→sub_00038530`,
  `0x0014FEF0→sub_00150231`). `RECOMP_ICALL` tries `recomp_lookup_manual` → `jsrf_lookup_recovered`
  first, so the shim is unreachable once recovered: 0 of 44 archived `[ALIAS-ICALL]` firings came from a
  build whose own `recovered.c` had the `case`. Only the run's own `source.zip` says what it executed.
- **`0x96F80` stays `UNQUALIFIED`:** container `0x96F60` `[0x96F60, 0x97190)` ends in a plain `ret` at
  `0x96F7A` but also has an indirect tail `jmp dword ptr [eax+8]` at `0x96F76`; `0x96F80` has two `.rdata`
  dwords (`0x001CD36C`, `0x001CD3EC`) and a clean 153-instruction walk to `ret 4` at `0x97187`. Pinned by
  `tests/test_hidden_entries.py::test_indirect_exit_is_not_gated`.

## 19. A present-count change correlated with F7c (2026-10-06)

Superseded by §22: the 1000/888 present ceilings were the walk rejecting `0x1810`. Still useful:
same-binary variation is documented by the retained pairs f25/f26 (`ca867957d99a`, presents 960 / 193)
and f11/f12 (`f2c67aec053c`, 888 / 888, returns 440 / 556).

## 20. The under-wide residue: 5 measured, 1 certifiable, 4 explicitly not (2026-10-06)

After F7c/F7d, `check-stack-depth.py` reported 13 `CUT_EPILOGUE` and 11 `FALL_OFF_END` (the 11 are a
different question). Each residual `CUT_EPILOGUE` under the certificate below:

| entry | epilogue | widen to next start | enumerated? | distinct `ret` immediate | certifiable? |
|---|---|---|---|---|---|
| `0x43EC0` | `pop edi; pop esi; pop ebx; ret 4` | `0x44400` | **yes** | `0x4`, at depth 0 | **yes** — but it crosses `0x44000` |
| `0x52050` | `pop esi; ret` | `0x52090` | no | — | no |
| `0x5BAD0` | not an epilogue: `0x5BB31` decodes as `push edi; lea edi,…` | `0x5BB90` | no | — | no |
| `0x91830` | not an epilogue: `0x9188C` decodes as `mov eax,[esp+0xc]; inc esp; …` | `0x918B0` | yes | `0x0` | no — **superseded by §26.4** (repaired; the `ret` at `0x918A0` is at depth 0) |
| `0xD4860` | not an epilogue: `0xD487A` decodes as `push ebx; push edi; …` | `0xD5150` | no | — | no |

`0x43EC0` vs `0x44000` is unresolved (`0x44000`: `.data` dword at `0x0022A394` in a packed run, database
`tail_jump_alias`, but its own manifest entry, dispatch tuple and clean walk); not repaired. The Turn
Reviewer re-derived all **159** certificates (152 F7c, 7 F7d) from the XBE; 0 failed; 81 have at least one
exit of `UNKNOWN` depth (76 + 5).

**The certificate, stated once and in full.** For a repaired under-wide span:

> The walk is **fully enumerated** (no truncated decode, no fall-off, no opaque exit); **every exit is a
> `ret`**; **exactly one distinct immediate `N`** appears across those exits; and **at least one** exit
> is reached at **depth 0**. That depth-0 witness is what licenses `stack_args = N`. The depths of the
> other exits are **not claimed** and may be `UNKNOWN`.

Where `evidence` strings, the preservation baseline or the plan say an entry was certified "at depth 0",
they mean that witness exists, not that every path's depth was resolved.

## 21. The `0x96xxx` vtable family: a sibling found by runtime, then closed by census (2026-10-06)

The `.rdata` vtables at `0x001CD2xx`–`0x001CD4xx` hold 25 distinct pointers into `0x96000`–`0x98000`.
Stop 26 `0x00096560` (`[0x96560, 0x967AA)`, `stack_args` 4) and stop 27 `0x00096B60` (`[0x96B60,
0x96DBA)`, `stack_args` 4) share a prologue (`mov eax,[esp+4]; sub esp,0x14; cmp eax,4`), one `.rdata`
reference each (`0x001CD32C`, `0x001CD3AC`) and zero direct callers. 24 of 25 own a span; the exception
is `0x00096F80` (§18).

## 22. The present ceiling was one stale method-table entry: `0x1810` (2026-10-06)

- **Cause.** The walk rejected the whole stream with `unsupported_method` on `0x1810` (`NV097_DRAW_ARRAYS`),
  absent from the generated admission table (`xboxrecomp/src/nv2a/nv2a_method_table.c`, L39) though the
  executor implemented it: `[PFIFO] reject … method=1810 subch=0 param=03000000 at=00008EF0 get=00008EF0
  put=0000A6D4 successes=15 rejections=1` (`20261006-203929-286-title005-ceiling`); the packet at
  `0x80009ADC` is `header 0x40041810`. The five archived target dumps (f9, g06, f10, f12, f13) all show it
  as the first rejection. With the pre-2026-10-06 live fence mirror (L17) D3D kept writing the ring after
  a rejection, so archived rings at GET are later frames' bytes; the A/B
  `20261006-205306-080-title005-ceiling-ab-live` (`RECOMP_FENCE_MIRROR_LIVE=1`) moved past `0x8EF0` to
  `at=00033D04`. The first `admit-unknown` enumeration, `20261006-205823-760-title005-admit-unknown`,
  listed exactly `class=97 method=1810 param=03000000 at=00009ADC`.
- **Fix: toolkit `505cda5b96e5b2a70492825df34151594805a1e5`**, the generated table +`0x1810` only (380 →
  381; NV097 370), generated from the three runs the committed table comes from:

  ```text
  python -X utf8 scripts/gen-nv2a-method-inventory.py \
    20260922-110235-244-spanfix-1185b0 \
    20260930-230206-594-f4-frames-after-horizon-fix \
    20261006-203929-286-title005-ceiling
  ```

  Each run is decoded only to its own log-derived ring top; the first two alone reproduce the old table
  byte-for-byte.
- **Cleared:** `20261006-210559-268-title005-fixed` (420 s, `just title-run`): 0 rejects, `last walk ok`,
  presents **2410** (old 888); confirm `20261006-215642-480-title005-confirm`: 1680 presents at 297 s.
- **The decoder is not the model's walk.** The generator expands `m = method + 4 * i` unconditionally;
  `nv2a_core.c` increments only when the non-incrementing bit (`h & 0x40000000u`) is clear and rejects an
  incrementing span past `0x1FFC` (`NV2A_SUBMIT_METHOD_RANGE`):

  | synthetic packet | generator reports | model does |
  |---|---|---|
  | `0x400C1810` (non-incrementing, 3 params) | methods `1810`, `1814`, `1818`; reached PUT | three writes to `1810` |
  | `0x00081FFC` (incrementing, count 2) | methods `1FFC`, `2000`; reached PUT | rejects: method-range overflow |

  It also takes the ring top as the maximum PUT in the log, fixes `GET` at `0x1000` (no ring wrap), and
  derives a subchannel's class from a hardcoded table. On a wrapped ring
  (`20261006-211635-913-title005-m15`) it stops at `bad_target 0x00100000` and inflates the union 381 →
  434 with a fake dense `0x1848`–`0x18F8` run (e.g. `header 0x1C200000`, count 1800).
- **Provenance rule:** admit a method only from a **runtime committed witness** (`[PFIFO] admit-unknown`,
  queued only inside the successful-commit block) or a faithful replay of the frozen pending stream, never
  because a decode reached its own PUT. Witnesses live in `config/nv2a-runtime-witnessed-methods.json`
  (run, log line, log SHA-256); the generator unions them (`--witness=`).

### §22.1 The six witnessed methods, and why admitting them is not the end of the blocker

- `0x0BB0`/`0x0BB4`/`0x0BB8`/`0x0BBC` and `0x1724`/`0x1728`, witnessed by
  `20261006-213505-255-title005-admit3`, admitted in toolkit `46b3265` / game `779c6a0` (381 → 387, NV097
  370 → 376, zero removals); all six were already implemented. (`20261006-212529-647-title005-admit2`
  took a different path and fired no banner.)
- `run_ordered_constant_contract` (`tests/test_nv2a_hal.c`, via `nv2a_pb_exec_vp_view`, toolkit `ec98ffe`)
  pins ordered constant delivery (component `((method - 0x0B80)/4) % 4`, index advances on `0x0BBC`); a
  last-value-only handler fails 15 assertions.
- Counting traps: `[PFIFO] submit` logging stops at 64 lines, and `[FBPRESENT]` lines are sampled (on
  change, every 10 presents, or every 10 s) — read `gpu-snapshots.jsonl` and the `presents=` counter. The
  published `at`/`get` of a budget stop are the rollback origin; the "last 32 visit addresses" array is
  written only at headers. The 39-method follow-on is settled in §23.3.

## §23 The M15 frame criterion was unsound: the disclaimer renders in four hashes, and one blacklisted hash is a logo

**`87683a748e27d071` is the blue Dolby card** (94.8 % saturated blue; `logs/workers/title005/tp0011.bmp`),
and the graffiti disclaimer renders in four hashes, so the old blacklist criterion ("neither
`5bdaea576b8509f5` nor `87683a748e27d071`") admitted disclaimer frames:

| hash | content | on the old blacklist |
|---|---|---|
| `5bdaea576b8509f5` | graffiti disclaimer | **yes** |
| `089fe3b826bbc18d` | graffiti disclaimer | no |
| `cf836ec8430ffb6d` | graffiti disclaimer | no |
| `8205f3a6d2e48df5` | graffiti disclaimer | no |

They are one artwork that fades and shifts (at a fixed threshold `089fe3b8…` and `5bdaea57…` agree on
100.00 % of pixels; Dolby vs disclaimer 11 %). Frame hashes are FNV-1a over the published frame's pixels
top-down, each pixel `(r<<16)|(g<<8)|b` (`fb_hash_rgb`, `xboxrecomp/src/video/fb_present.c`); 9 of 9
dumped frames reproduce their logged hash. `20261007-001831-252-20261007-title006-sixadmitted` ended on
`8205f3a6d2e48df5` and met the old wording with no title on screen. **M15 is identified by content** (the plan's criterion). The
boot sequence advances rather than loops (`20261007-022608-843-…-frames`: 10 of 15 distinct states first
appear in the second half).

## §23.1 What the guest is doing when it stops

`sub_0019E438` dominates `[TRACE]` only because `config/trace-functions.json` lists it; it is the DSOUND
conditional-lock helper (`call dword ptr [0x1c4064]` = ordinal 277 `RtlEnterCriticalSection`); Enter/Leave
exactly balanced (24702/24702). The toolkit publishes no IRQL into the guest TIB (`fs:[0x24]` stays 0),
which costs time, not correctness. **No input can reach the guest at this stage:** keyboard state is
written only by `WM_KEYDOWN`/`WM_SYSKEYDOWN`; `RECOMP_PAD_PRESS` (`src/usb/usb_gamepad.c`) is reached only
through OHCI, and `xbox_OhciInit` and `xbox_InputInit` are never called. xemu reaches the title with no
input (§23.2). `RECOMP_FB_VA` makes `xbox_FramebufferWindowPresent` return early, so a pinned run's hashes
are not comparable with an unpinned run's.

## §23.2 The `0x00084000` measurement: drawn = presented, and the disclaimer is a timed hold, not a freeze

- **Reading surfaces from a dump.** `0x00084000` is the raw `color_offset`; `dma_resolve` maps it to
  **`0x80084000`** in the 64 MB contiguous window. Read offline with `scripts/inspect-jsrf.py memory <run>
  0x80084000 614400 --out f.bin` after `check-dump-mapping.py` passes (`memory <run> 0x00084000` returns
  XBE `.text`, `0x11000..0x18CB30`); decode 640×480 RGB565 at pitch 1280 as `fb_convert` does. The three
  surfaces are `0x80084000` (offscreen render target), `0x8011C000`, `0x801B2000`.
- **The disclaimer is a timed hold that ends by itself:** it begins at presents **1461** and is left at
  **2424** (→ `e886cadf72766a64`, then black `156ed4086987e325`):

  | run | disclaimer starts | leaves | run ends |
  |---|---|---|---|
  | `20261006-211635-913-title005-m15` | t=244 s, p=1461 | t=408 s, p=2424 → `e886cadf…` | t=418 s, p=2425 |
  | `20261006-213505-255-title005-admit3` | t=257 s, p=1461 | t=420 s, p=2424 → `e886cadf…` | t=496 s, p=2439 |
  | `20261007-054545-206-title007-long3d` | t=245 s, p=1461 | t=408 s, p=2424 → `e886cadf…` | t=697 s, p=2441 |

  Presents advance ~6/s through the logos, so a run must pass ~2424–2430 presents to say anything about
  the title.
- **After the transition the guest loads title assets:** `Media\Disp\SprNorm1.bin/.dat`,
  `Media\Z_ADX\BGM\title.adx`, `UDATA\…\SaveMeta.xbx`, and reloads `Player\Corn/Beat/Gum/Yoyo`.
- **The xemu reference** (`logs/workers/title007/xemu/deliverable/`): boot → Smilebit → ADX → Dolby →
  graffiti disclaimer (42.5–52.5 s) → fade to black (57.5 s) → 3D city backdrop (60 s) → JSRF emblem
  assembling → **"PLEASE PRESS START TO BEGIN" (≈85–95 s)**, 640×480, **no input**; a perspective street
  view with a green elevated highway, fully presented.
- Dump reads are single frozen instants; §23.6 pairs draw and present at one flip.

## §23.3 The 39-method list was truncated by the witness's own 16-entry cap, and is now runtime-witnessed

- **Defect.** The `[PFIFO] admit-unknown` queue was capped by `NV2A_ADMIT_PENDING 16` while the dedupe log
  allowed `NV2A_ADMIT_LOG_MAX 256` (`xboxrecomp/src/nv2a/nv2a_core.c`), so a submission with more than 16
  unknown methods logged 16 and dropped the rest. The truncation was in the log, not the commit.
- **Fix:** the second constant deleted; the witness is sized by `NV2A_ADMIT_LOG_MAX`. A test with 20
  unknown methods in one walk fails at 16 (`admit-unknown lines: 16, want 20`) and passes at 256.
- **Result:** `20261007-061613-201-title007-witness-full` logged **39** methods — `0x0420-0x042C,
  0x0480-0x04BC, 0x0680-0x06BC, 0x1748, 0x1B40, 0x1B44` — exactly the set
  `20261007-060314-395-title007-noadmit`'s `gpu-report.json` derives (the witness run's own report lists
  35, having admitted `0x0420`–`0x042C` earlier). L45.
- **What each did (pre-merge executor audit):**

  | methods | classification | consumer |
  |---|---|---|
  | `0x0680-0x06BC` | **CONSUMED** | sets `s_gpu.composite`; `fetch_position` uses it as the fixed-function transform |
  | `0x0480-0x04AC` | **CONSUMED** | `lit_color` reads `s_reg[0x0480/4 .. +11]`, gated on lighting enable `s_reg[0x0314]` and a normal attribute |
  | `0x04B0-0x04BC` | captured only | model-view row 3; the data is `(0,0,0,1)` |
  | `0x0420-0x042C` | captured only | `NV097_SET_TEXTURE_MATRIX_ENABLE`; the witnessed params are all 0 |
  | `0x1748` | **CONSUMED** | `0x1720+10*4`: vertex-array offset for attribute 10 |
  | `0x1B40`, `0x1B44` | captured only — render gap then | texture stage 1 offset/format; only stage 0 fed `s_gpu.tex` (the v0.13.1 executor brings four stages, §1; not re-audited) |

  `0x0420` is not a transform constant (those are `0x0B80-0x0BFC`); `0x0480` is
  `NV097_SET_MODEL_VIEW_MATRIX`, a 16-dword span. The `0x0680` `composite_set` latch is never cleared
  (INFERRED render risk).
- **Admission is additive and clears the stop:** NV097 376 → 415, zero removals; the stop moved from
  `unsupported_method 0x0420` at `get=0x50810` to `sink_capacity` at `get=0x50B1C` (§23.4).

## §23.4 The capacity bound was per-submission, and a real title kick is 1.4 budgets

A title-transition kick is **5732 words** (`get=0x50810 put=0x561A0`) against `NV2A_SUBMIT_MAX_WORDS` =
4096. **Fix (toolkit `1f86fbb`, L40):** the walk consumes the ring in **units** that end at a whole-packet
boundary and each commit all-or-nothing; a unit ends at a header when `unit_words + 1 + count >
NV2A_SUBMIT_MAX_WORDS`. A packet is at most 2047 parameters, so a unit always fits one and a packet is
never split. `sink_count` resets per unit (two units staging 6138 methods had overflowed `sink[]`). `GET`
advances per unit; `NV2A_COMMIT` is published only on reaching the original PUT; a retry re-delivers only
the failing unit. Measured: GET `0x50B1C` → `0x5A060` (9553 words, 2.33 budgets), `successes` 3529; next
stop the 29-method class (`0x0580-0x05AC`, `0x06C0-0x06FC`, `0x1964`; L46, §23.6).

## §23.5 After the capacity fix: the presented stream is black

Superseded by §23.8 (cause found and fixed). In `20261007-081204-797-title007-witness2` all three
surfaces read zero at the dump and the presented stream was black from presents 2450 to 3120. Kept:
`missing_methods = 0` with `GET == PUT` is an
empty-queue artifact, so check `GET == PUT` first; archived `build-source.json` hashes are of the CRLF
on-disk file, so they differ from LF git blobs.

## §23.6 The same-flip trace: copy/publication consistency, and the guest drew the black

**Instrument** (toolkit `52e6d12`, `RECOMP_FLIP_TRACE`, L47, off by default): at one `NV097_FLIP_STALL`,
keyed on `(flip_stalls, present serial)`, it records every surface named by `SET_SURFACE_COLOR_OFFSET`
with a content hash (same FNV-1a-64 and RGB conversion as `fb_present.c`), the hash of the bytes handed
to the window, the surface `present_track_flip` selected and the branch that chose it, and a ring of the
frame's last batches. Read-only; `RECOMP_FB_VA` not used.

**`20261007-110933-718-title008-trace-noadmit`** (620 s, `RECOMP_FLIP_TRACE=400 FROM=2200 CHANGE=1`; 242
flips, 2200→2441):

| Check | Result |
|---|---|
| published hash **==** selected-surface hash, at the same flip | **242 / 242** |
| branch that selected it | `drawn_this_frame` **242 / 242** |
| selected surface in the candidate set | **242 / 242** |
| published matched **no** candidate | **0** |
| `flip_stalls == present serial` | **242 / 242** |

The candidate set is exactly `0x00084000`, `0x0011C000`, `0x001B2000`; `flip_modulo=3` is a three-entry
flip index ring, not three equivalent scanout buffers.

| flip | selected → published VA | published | `0x84000` | `0x1B2000` | `0x11C000` | bound texture |
|---|---|---|---|---|---|---|
| 2423 | `0x84000` | `8205f3a6d2e48df5` | `8205…` | `8205…` | `8205…` | `8205…` |
| 2424 | `0x84000` | `e886cadf72766a64` | `e886…` | `8205…` | `8205…` | `e886…` |
| 2425 | `0x84000` | `156ed4086987e325` | `156e…` | **`8205…`** | **`e886…`** | `156e…` |
| 2426 | `0x1B2000` | `156ed4086987e325` | `156e…` | `156e…` | **`e886…`** | `156e…` |
| 2427–2441 | alternating | `156ed4086987e325` | `156e…` | `156e…` | `156e…` | `156e…` |

**Classification:** copy/publication mismatch **eliminated** in the observed window; the guest drew black
into the buffer it then selected. **Semantic source selection is UNQUALIFIED**: `reason` restates the same
heuristic, and at flip 2425 a composite writes `0x11C000` while sampling `0x84000` before a later `self=1`
batch writes `0x84000`, which the tracker then prefers; no selection bug is claimed. **Timing
non-perturbation is UNQUALIFIED** (no matched trace-on/off control on one binary). **Limitations:** the
registry caps at 8 offsets; candidate hashes use the current clip/pitch and the bound-texture hash uses
surface geometry; `_CHANGE` keys on the decision tuple, not content.

**The 29 methods are witnessed and admitted:** `20261007-113354-706-title008-trace-admit`
(`RECOMP_NV2A_ADMIT_UNKNOWN=1`) logged 67 `admit-unknown` lines including all 29 (log SHA-256
`c80b467985c80a924eb38bc6e85dc582be3696f656e36a06410d7264d7d0c853`); NV097 415 → 444, zero removals.
`0x1964` is genuine: header `0x00041964` (count 1, subchannel 0, incrementing), param `FF000000` —
`NV097_SET_VERTEX_DATA4UB + 0x24` (attribute 9, component 0) — found at `0x8005DAE0` in
`20261007-085324-850-title007-blacktrace` and `0x80004540` in trace-noadmit.
`20261007-112201-379-title008-admit-witness` reached presents 2850 with zero rejects and black surfaces,
so the black did not depend on the `0x1964` reject. The three runs share `exe_sha256 9818c346…` but differ
in settings; do not A/B them. Path divergence: `admit-witness` and `witness2` never read `title.adx` after
opening it (0 `[READ]`, 0 `[ADXIO]`); trace-noadmit, trace-admit and blacktrace read it twice (51200 +
800768 bytes, 6 `[ADXIO]`).

## §23.7 The batch ring: what it establishes, and what it does NOT

`20261007-115809-590-title008-ring-admitted` (620 s, the 29 admitted, `RECOMP_FLIP_TRACE=900 FROM=2400
CHANGE=1`, 42 flips): every traced frame contains a textured batch that writes a swap surface while
sampling `0x80084000` with `px=307200` (`640*480`). **`px` counts `put_pixel` write calls, not unique
coverage.**

```
flip 2425 (published 156ed4086987e325, black):
  b[0] target=0x0011c000  tex=0x80084000  self=0 tris=1 px=307200
  b[1] target=0x00084000  tex=0x80084000  self=1 tris=2 px=306081
```

**Every surface hash in the trace is taken at flip time, after every batch has run**, so it cannot tell
"the composite read black" from "the source was cleared after the composite read it". Writer shape of the
selected surface over the 42 flips: `self=0 px=307200 tris=1` 15, `self=0 px=306081 tris=2` 25, `self=1
px=306081 tris=2` 2. The run stopped at `[PFIFO] reject diag=unsupported_method method=1A30 … at=00059420
get=00059420 put=0005C7B4 successes=3560`. **The 38-method class is witnessed** by
`20261007-122836-322-title008-witness-1A30-long` (900 s; log SHA-256
`db0ec342e12cbbd7e38157645af4ebf6e285d28c918f7bc20e11ea9f9906b066`; the 620 s
`20261007-120946-505-title008-witness-1A30` logged none) and admitted (L49):

```
1A30 1A34 1A38 1A3C 1A40 1A44 1A48 1A4C   1968   0700 0704 0708 070C 0710 0714 0718 071C
0720 0724 0728 072C 0730 0734 0738 073C   1518 151C 1520 1524   1734   17F8   18C8 18CC
1B80 1B84   1E20 1E24   1E74
```

`0x1A30` is `NV097_SET_VERTEX_DATA4F_M + 0x30` (attribute 3 diffuse, component 0); the pre-merge executor
decoded that family only for attributes 0 and 9.

## §23.8 The black had a concrete cause: the vertex-program viewport constants were never loaded

The XDK vertex programs end with `MUL o0.xyz = r12 * c[58]` / `MAD o0.xyz = r12 * r1 + c[59] FINAL`;
`c[58]`/`c[59]` are `NV_IGRAPH_XF_XFCTX_VPSCL = 0x3a` / `VPOFF = 0x3b`, loaded by `SET_VIEWPORT_SCALE`
(`0x0AF0`) and `SET_VIEWPORT_OFFSET` (`0x0A20`). The executor never loaded them (`0x0AF0` unhandled,
`x28024 last=0x43A00000 (320.0)`), so every vertex-program vertex collapsed to the origin and drew nothing.
**Fix (toolkit `d690d54`, L48):** `0x0AF0-0x0AFC` → `s_vp.c[58]`, `0x0A20-0x0A2C` → `s_vp.c[59]` and
`s_gpu.vp_offset`; stage 0's enable bit (`SET_TEXTURE_CONTROL0` `0x1B0C`, bit 30) honoured, default
enabled (scene batches disable stage 0 while a texture offset is bound, which would otherwise be a
feedback read).

| measure | before (`…115809-590-…ring-admitted`) | after (`20261007-124149-033-title008-d1-viewport`) |
|---|---|---|
| distinct `[FBPRESENT]` hashes over the run | **10** | **658** |
| `0x0AF0` in the unhandled list | yes (`x28024`) | absent |
| triangles rasterised (final report) | 58378 | 40966 |
| flips 2426–2440 | one repeated black hash | 15 distinct hashes |

At the dump `0x80084000` = `efddce3b5bb02ab1` (0.935 non-black, 2677 colours, a full 3D city scene) and
`0x8011C000` = `eaaa65df05fa3144` (a "Now Loading" screen). Flips 2425 and 2441 are still black. This is a
before/after with confounders (the D1 patch on `52e6d12`, executable `0311303e…` vs clean `764f66ae…`;
trace budget 400 vs 900, non-binding), not a controlled A/B; both stop on the same `0x1A30` reject.

## §23.9 `0x00159330`: a function reachable only by an indirect call, and the recovery

`[ICALL] Failed to resolve VA 0x00159330` (first in `…113354-706-title008-trace-admit`, again in
`…122836-322-…-witness-1A30-long`). No database entry at all (nothing calls it directly). Bytes: `ret 4` at
`0x0015931E`, padding, prologue `push ecx; push esi` at `0x00159330`, `ret 0xc` at `0x00159416`, next
function `0x00159420`. Recovered `[0x00159330, 0x00159419)`, `stack_args: 12` (stop 29, L50).
`20261007-200329-265-title008-recovered-159330` (900 s) ran to presents 2808 with 1023 distinct hashes and
no `[ICALL] Failed`/`[EXCEPTION]`/reject; **runtime confirmation absent** (no clean ABI-verified return;
entry coverage UNQUALIFIED).

## §23.10 `0x000C2730`: the abutting-alias class again, and what the frame dumps really covered

`20261007-223953-965-title008-frames-late` died on `[ICALL] Failed to resolve VA 0x000C2730` (presents
2442, 588 s): five overlapping `tail_jump_alias` entries (`sub_000C2480`, `…C2500`, `…C2560`, `…C25D0`,
`…C2700`) end at `0x000C276F`, and manifest entry `0x000C2700` declared that end. Bytes: `ret` at
`0x000C272C`, padding, prologue `sub esp,0x18; push esi; mov esi,ecx` at `0x000C2730`, `ret` at
`0x000C276E`. Recovered `[0x000C2730, 0x000C276F)`, `stack_args 0`; `0x000C2700` narrowed to `0x000C272D`
(stop 30). **The window-thread frame dump is capped at 400 files** (`fb_present.c`, `writes < 400`; in that
run hit at t=169 s / presents 655), so use `RECOMP_FB_PRESENT_DUMP_AFTER_S` for late capture.

## §23.11 Two more abutting-alias recoveries, both now EXERCISED, and the latent siblings

| run | fatal target | mechanism | result |
|---|---|---|---|
| `…223953-965-…-frames-late` | `0x000C2730` | 5 overlapping `tail_jump_alias` entries end at `0x000C276F`; manifest entry `0x000C2700` declared the same end | recovered, entry end narrowed to `0x000C272D` |
| `…233030-481-…-c2730-fixed` | `0x000C3410` | 7 overlapping `tail_jump_alias` entries end at `0x000C3670`; manifest entry `0x000C33C0` declared `0x000C3500` | recovered, entry end narrowed to `0x000C3408` |

Stops 30 and 31 (L51). `20261007-235846-887-title008-c3410-fixed` logged a clean return for both and ran
the full 900 s with no fatal call (presents 2444). Latent `OVER_RUN` siblings, not fixed:
`0x000C002C-0x000C004A` (body ends `0x000C003F`; prologue at `0x000C0050`) and `0x000CD890-0x000CDAC0`
(body ends `0x000CD8AE`; function at `0x000CD8B0`).

## §23.12 The indirect-call chain is cleared, and the stop moved back to method admission

The c3410-fixed run then rejected `unsupported_method method=0298` (`NV097_SET_COLOR_MATERIAL`) at
`get=00078320 put=0007BEFC successes=3636`. `20261008-001451-422-title008-witness-0298` (log SHA-256
`d3a7419dcfc09cde4d5b9b63dfdcd14c3bcdc59b27b0ab9aca71a4faf0a0d865`) witnessed 25 methods — `0x0298`,
`0x03A8-0x03BC`, `0x0A10-0x0A18`, `0x1000-0x103C` — admitted: NV097 482 → 507, zero removals (L52).
`20261008-003812-419-title008-507` (900 s): 0 rejects (was 3), last present **3717** (was 2444), 1525
distinct hashes (was 138), no fatal call; at the dump all three surfaces show "Now Loading"
(`0x8011C000` = `b3d20bc079d1e6e4`, the others `5132208e7524c004`).

## §24.1 `budget_exhausted` is benign resumable chunking, and its real defect is a zero-commit livelock

**Case A, on the drain evidence** (per-stop ring `NV2ABudgetEvent`): control
`20261008-032308-212-title009-cap1024` (900 s, default cap) has 17 budget stops, each followed by
`[PFIFO] recovered after 1 rejections get=<PUT> put=<PUT>` with `successes` +1, at 17 distinct addresses.
The 12 title-phase stop addresses of `…001451-422-…-witness-0298` equal the control's first 12
(`0x705B0, 0x428F4, 0x1BB4C, 0x70BC0, 0x49F10, 0x23D50, 0x5920C, 0x332F0, 0x1457C, 0x69550, 0x49ED8,
0x239E8`). Every archived budget event is `LIMIT=packets(1024)`, none the word limit; anchor counts on
the literal `[PFIFO] reject diag=budget_exhausted`.

**The livelock (L54).** `unit_words` resets per unit but `packets` never does, and `if (packets >= 1024)`
runs before the yield, so a packet-dense stream can reach the cap with **zero** units committed and GET
pinned. Shown with the cap at 128 (`20261008-041233-960-title009-cap128`, `RECOMP_NV2A_PACKET_CAP`, L55):
the first boot submission (`get=0 put=0x1000`) livelocked; zero `[FBPRESENT]`, `successes=3`. At the
shipped cap the margin is zero (`walk_packet_max = 1024` against a 1024-word boot kick). Tests:
`test_packet_cap_can_pin_get_without_a_commit`, `test_budget_stop_resumes_at_committed_boundary`; the
resume audit records `budget_resume_stalled`. The packet term protects nothing architectural; changing it
is deferred. `20261008-043848-133-title009-clockfix-1800` (1804.7 s): 63 budget stops, `successes` 6903,
`walk_words_max` 11612, 0 `still rejecting`.

## §24.2 The `Now Loading` hold: a host clock overflow exists and is FIXED; the wrap is ASSOCIATED with worker loss, but the mechanism is not established

**Established (L53).** `qemu_clock_get_ns` formed `count * 1000000000LL / freq` in a signed 64-bit
temporary:

| wrap | count | uptime at 10 MHz | consumer sees (`uint64_t now_ns`) |
|---|---|---|---|
| signed product overflow | `2^63/1e9` = 9223372037 | **922.337 s** | jumps **FORWARD** |
| unsigned product wrap | `2^64/1e9` = 18446744074 | **1844.674 s** | jumps **BACKWARD** to near zero |

The backward jump stops `ptimer_service_thread`'s vblank (no pulse, no PCRTC interrupt, no `KeSetEvent`).
Fix: `nv2a_qpc_to_ns` in `src/nv2a/host_clock.h` (`(c/f)*1e9 + (c%f)*1e9/f`), used by `qemu_clock_get_ns`
and `apu_shim.h`'s `qemu_clock_get_us`; the vblank loop re-arms on a backward step and caps its wait at
four frames. Tests: `host_clock_wrap_test` (calls the shipped conversions; mutation-validated),
`vblank_clock_step_test` (0 vs 9 pulses in 10 frames). **Withdrawn:** that the wrap was shown to cause the
hold or the "worker deaths"; per §25 the deaths are workers parked on the vblank event, the unwrapped ones
were a detector artifact, and the fixed binary shows none (§25.3). Clock zeros differ: QPC, `[CHECKPOINT]
ms=` (GetTickCount64; QPC − GetTickCount64 = +4.2 s on this host) and `[FBPRESENT] t=`
(first-present-relative). Vblank delivery in the clock-fixed runs was ~30–80× below a nominal of at least
40 Hz (`clockfix-1800` 1337 pulses over ~1780 s = 0.75 Hz); lock starvation via the shared
`g_mmio_owner_lock` was the candidate (measured in §25.5).

## §24.3 The guest's real blocker after the clock fix is executor throughput, not the walk

With the clock fixed the guest opens and reads `title.adx`, then enters a heavy 3D phase:

| phase | presents | triangles rasterised per report | present rate |
|---|---|---|---|
| logos / loading (to t≈420 s) | 1 → 2439 | ~359 median | ~5.5 /s |
| title 3D phase (t≈480 s →) | 2439 → 2461 | **~31347 median** | **~0.05 /s** |

On `20261008-043848-133-title009-clockfix-1800` the present rate falls 110× (5.53 → 0.05 /s) where the
per-report triangle count rises 87×; ~1.8 billion pixel writes, ~1.9 million triangles, 875 pixels per
triangle. (The present wall is flip frequency, §25.6; executor time is pixel fill, §26.2.)

**A city scene drawn but not presented in that run.** The final `0x80084000` (rendered to
`logs/workers/title009/clockfix/0x80084000.png`) is a full 3D city backdrop like xemu's at t=60 s
(`logs/workers/title007/xemu/run2/client/0023_00060.0s.png`), by eye only:

| surface | hash | published in this run? |
|---|---|---|
| `0x80084000` (the city) | `31b1469f9c922c32` | **no** |
| `0x8011C000` | `9c7539413e3b8e4a` | yes |
| `0x801B2000` | `c95814fe744a3c90` | yes |

The city `efddce3b5bb02ab1` (§23.8) is published in none of 188 archived runs. This end state is not
general (§25.7).

**Texture stage loss at the transition.** Batch counters show ~17–25 % "texcoords but no usable stage"
through the logos, then a steady **93.8 %** at present ~2457 (`dNoStage 435` against `dTex 29`); every
untexturable batch is `stage disabled by the guest` (final `no-stage cause` 4187 = the `batches:` line's
4187). **§26.3 settles it: the guest itself writes stage 0's CONTROL0 with ENABLE clear.** What those
batches should draw is open.

## §25. The ADX "worker deaths" are parked waiters, not deaths; the walk's per-word `VirtualQuery` was the real cost (turn title-010)

### §25.1 The inherited worker-death population is an artifact of its detector

`logs/workers/title009/orch/verify_contingency.py` counted `0x007BFFCC` as a worker, but it lies in the
**main** thread's stack (`0x00780000..0x00F80000`, `ESP = 0x00F7FFF0`), so it measured main's print
timing; exact-`esp` matching also split one worker across keys. By owning stack
(`logs/workers/title010/orch/recompute_contingency.py`):

| detector | wrapped | unwrapped |
|---|---|---|
| prior (exact esp) | 5 L / 19 K = 20.8 % | **10 L / 50 K = 16.7 %** |
| corrected (stack) | 8 L / 16 K = 33.3 % | **0 L / 60 K = 0.0 %** |

### §25.2 The workers are live waiters on the D3D vblank event

The two ADX threads are `state=1` in `xbox_KeWaitInplaceEvent` ← `bridge_KeWaitForSingleObject` ←
`sub_0018CE50` ← `body_0013B1C0`; no run logs a worker return or `PsTerminateSystemThread`. On
`20261008-155241-052-title010-R1-telemetry` all 256 logged long waits are `timeout=INFINITE` on
`0x0019D630`. A "death" is the vblank event ceasing to be signalled.

### §25.3 No death population on the fixed binary (Case D)

`…155203-preflight` (24.2 s, no wrap), `…155241-R1` (904.3 s, wrap at +763.1 s) and `…185536-R2` (906.2 s,
wrap at +854.6 s): 0 worker exits; every worker printed heartbeats after the wrap.

### §25.4 A correction: the "whole-guest stall" class is wrong

For `long3d`, `units`, `units4`, `blacktrace`, `trace-noadmit`, `admitted39`, `admit3` the main thread is the
most recently active thread (lag 0.00 s) and `[FBPRESENT]` continues to within 7–18 s of the end
(`logs/workers/title010/orch/ring_liveness.py`).

### §25.5 The walk's per-word `VirtualQuery` is a first-order cost

`submit_read_word` called `VirtualQuery` once per pushbuffer word under `g_mmio_owner_lock`. Measured
(`logs/workers/title010/orch/vq_bench.py`, 64 MB view): private commit 1.63 µs untouched / **184.9 µs** all
touched; **mapped view** 12.0 µs / **378.1 µs**. R1: `walk_words_max` 8144 × 185 µs = 1507 ms against a max
lock hold of 1636 ms. **Fix (toolkit `5d6ebbd`):** a walk-scoped cache of the validated span (bounds,
alignment, the full protection predicate incl. `PAGE_NOACCESS`/`PAGE_GUARD`, and the region end still gate;
a failing page is never cached); test `nv2a_read_guard` against a real `PAGE_GUARD` page.

| statistic | R1 (no cache) | R2 (no cache) | **R3 (cache)** |
|---|---|---|---|
| late re-arms / passes | 2941/4256 = **69.1 %** | 2961/4123 = **71.8 %** | **0 / 11924 = 0.0 %** |
| max owner-lock hold | 1636 ms | 1219 ms | **68 ms** |
| max pulse-to-pulse gap | 13 995 ms | 14 791 ms | **78 ms** |
| pulses per pass | 0.297 | 0.275 | **0.979** |
| presents at matched `t=120 s` | 628 | 641 | **2568 (4.1×)** |

**R3** (`20261008-191232-038-title010-R3-vqcache-AB`) passed the transition (2885 presents) and died at
236.5 s on `0xE0424943` at VA **`0x9188C`**, a pre-existing hole in `0x91830` (repaired §26.4), with no
frame of the walk, cache or lock on the stack. The control `20261009-142848-669-title011-R3-control-5fd62cb`
(pre-cache toolkit `5fd62cb`, R3's switches, 900 s) reached only 1320 presents (last hash
`87683a748e27d071`) and is inconclusive.

### §25.6 The present-rate wall is flip frequency, not rasteriser throughput

`flips == presents` exactly (2494 = 2494 in `clockfix-1800`). The flip rate collapses from ~2.77 /s to
0.035–0.047 /s late (59–80×) across four runs and three binaries while draws and triangles keep climbing
(~485 draws, ~28 000 triangles per 10 s report); draws-per-flip is stable (542.6 vs 541.9).

### §25.7 The presenter is usually right; the remaining questions are structural

Over the five archived `[FLIPTRACE]` runs, **324 of 370** traced flips (87.6 %) select the draw surface
`0x80084000`, all `reason=drawn_this_frame`; `clockfix-1800`'s end state (city hash `31b1469f9c922c32` 0
times among 711 distinct published hashes, §24.3) is not general. Structural gaps, not fixed (pre-merge
executor; the v0.13.1 merge ported the tracker and flip-trace paths, §1, not re-audited):

- the guest's `flip_read`/`flip_write`/`flip_modulo` (`modulo=3`) are only printed, never read by
  `present_track_flip`;
- the `[GPU]` report's "draw surface" prints `s_gpu.drawn_offset`, the variable `present_track_flip`
  prefers, so it agrees by construction, and it is sampled after the last flip;
- the commit consumer drops every class != 0x97; a census (toolkit `5fd62cb`) names each class and flags
  `NV_IMAGE_BLIT` (0x9F);
- at flip 2426 in `…110933-trace-noadmit` a black surface was published while `cand[2] 0x8011C000` =
  `e886cadf72766a64` existed; flip-time hashing (§23.6/§23.7) cannot separate the causes.

### §25.8 Instrumentation added (and one limitation found by using it)

A monotonic clock (`nv2a_mono_clock.h`; one epoch per process since §26.1) stamps worker spawn, heartbeat,
return and wait events; vblank re-arm/pass/gap telemetry is published by the ptimer thread itself, so it
cannot freeze with the walk; owner-lock hold/wait maxima with holder tid; `[WAIT]` prints are bounded but
totals are not (toolkit `e487f78`; the 256-print cap had covered only 93 950 ms of a 904 300 ms run).
`[CHECKPOINT] ms=` is not a timeline (3 lines per 1800 s run, all in the first 78 lines).

### §25.9 Withdrawn

Do not cite: the `death_vs_onset` null result; anything keyed on exact `esp` or on `[KERNEL] summary`
cadence; the "6 kept / 7 lost" lists; "0 of 16 deaths within ±5 s of a wrap"; the "whole-guest stall"
class. The clock repair (§24.2, L53) and Case A (§24.1, L54) stand.

---

## §26. Executor time, the telemetry epoch and the stage-0 writes (2026-10-09, after the merge gate)

### §26.1 The telemetry clock had one zero per file, now one per process

`nv2a_mono_now_ns` kept its anchor in a function-local static of a `static inline` header, so each
translation unit had its own zero (`kernel_bridge.c`, `nv2a_mmio_hook.c`, `nv2a_pb_exec.c`). Fixed by one
`g_nv2a_mono_anchor_count` defined in `nv2a_core.c`; pinned by `nv2a_mono_clock` (native and CTest: two
translation units 60 ms apart; it failed on the old header with a first reading of 0 ns). Cross-file stamp
comparisons made before this fix (§25.8) carry an unknown offset and should be re-checked before being
extended.

### §26.2 Where executor time goes: pixel fill

Always-on QPC buckets in `nv2a_pb_exec.c`, printed as `[GPU] executor time:` and exported through
`nv2a_pb_exec_timing`: `exec` (whole commit-consumer call), `vsh` (vertex-program transform loop), `tri`
(program-path triangle raster), `fill` (`xf_rows_parallel`, inside `tri`, wall time on the executor thread
including the raster pool's wait) and `ffp` (screen-space path). Merged executor,
`20261009-145632-182-title011-instr` (300 s, title-run switches), final report at t_ms = 292 945: **busy
217 611 ms; vertex programs 46 ms; triangle setup 41 ms; pixel fill 207 409 ms; screen-space 0**; presents
at t = 120 s 940 against the uninstrumented A's 960. With `RECOMP_NO_VSH=1`
(`20261009-150139-277-title011-instr-novsh`): busy 182 488 ms of which screen-space 171 232 ms, presents at
t = 120 s 1290, late re-arms 1250/8144 = 15.3 %, max hold 167 ms. **So the merge's slowdown is per-pixel
cost**, not vertex-program execution: ~1.3-1.5 billion pixels per 300 s at ~110-160 ns each, inside the
walk's owner lock, which starves the vblank pulse. The pre-merge executor wrote at least as many pixels
(§1, B) faster; its per-pixel cost was not instrumented.

### §26.3 The guest disables stage 0 itself

The stage-0 `SET_TEXTURE_CONTROL0` latch (`nv2a_pb_exec_tex0_control`; the `CONTROL0 stage 0:` report
line): in the instrumented run the guest wrote stage 0's CONTROL0 **16 230** times, **7 581** with ENABLE
clear, last value `0x00000000`; the no-VSH run 18 168 and 8 485. Every `stage disabled` batch follows a
guest write that cleared the bit; it is not a bit the model lost or never received. What those batches
should draw is open. The HAL fixture `jsrf_nv2a_hal` pins the latch (two stage-0 writes, one disabling, a
stage-1 write that must not count) and the timing buckets' nesting invariants. Both instruments are
observation only (no ledger entry or run-profile classification).

### §26.4 `0x9188C` repaired; the next run stops in the game's own fatal-error path

**The repair (stop 32).** `0x9188C` is the target of `je 0x9188c` at `0x91878` inside `0x91830`, a complete
method of the `.data` table at `0x20D8D0`: `sub esp,0xc`, three pushes, …, `pop edi/esi/ebx`, `add
esp,0xc`, `ret` at `0x918A0`, NOP padding to `0x918AF`, next function `0x918B0`. Its recovered entry had
ended at `0x91882`, the database's `tail_jump_alias` entry `sub_00091882` (a `push ecx` mid-function with
no caller; its only raw dword match lies inside DSOUND data), so the branch reached an abort stub. Widened
to `[0x91830, 0x918A1)`: the branch is now `goto loc_0009188C`; `0x9188C` left `recovery-unresolved.json`;
`0x91830` left the span-ownership `KNOWN_OPEN` set (25 → 24); stack-depth `CUT_EPILOGUE` 14 → 13. The
`ret` at `0x918A0` is at depth 0 (correcting §20).

**What the regeneration brought.** `recover-functions.py` re-lifts every recovered function with the
current (v0.13.1) lifter, which emits `RECOMP_FP_PC` (x87 precision control) and
`RECOMP_ICALL_SAFE_AT_CC`; the game's `recomp_types.h` was refreshed from the toolkit template and the four
header patches re-applied by `scripts/patch-generated.py` (14/14 applied). The preservation baseline and
provenance manifest were re-recorded. All 3164 recovered functions carry the merged lifter's semantics;
the generated chunks keep the pre-merge lift.

**The first run on the repaired tree did not reach the address.**
`20261009-160259-224-title011-9188C-fixed` (900 s, uncommitted tree) has no crash line, but presents stop at
**2429** at t ≈ 353 s:
- after the `Media\Player\Gum*` loads, a loading job (`job=01330060`, `+98=30000074`) enters the
  disc-error path three times (`[FATAL-TAIL]`/`[FATAL-CTOR]`, L41);
- the guest then opens `Z:\Media\Cache\JSRF_FATAL.ERR`;
- the main thread then sleeps under `sub_00145C28` ← `sub_00145CA6` ← `sub_00013F80` to the end;
- the GPU is idle: `GET == PUT`, last walk ok.

The same fatal path appears in the `RECOMP_NO_VSH` run (§26.2), not in R1, R2, R3, B or the control.
**Not established:** the trigger. The runs that took the path never read `title.adx` (no `[ADXIO]`), while
B and R3 do at this transition.

**Register combiners off avoids the fatal path, but not the hold.**
`20261009-163657-288-title011-nocombiners` (600 s, `RECOMP_NO_COMBINERS=1`, game `57af797`, toolkit
`de39fb1`): no `[FATAL-*]` line and no crash; `title.adx` read (6 `[ADXIO]` lines, as in B); presents hold
at **2435** from about 250 s to the end, the same hold B shows; stop 32 not reached.

**Three outcomes at this transition, measured over seven runs:**

| outcome | runs | evidence |
|---|---|---|
| passes | R3 only (toolkit `5d6ebbd`) | 2885 presents by 227 s |
| holds after reading `title.adx` | R1, R2 (title-010 toolkits), B (`fafe0f6`), no-combiners | reach ~2430 (R1/R2 at ~480 s), then creep to 2435-2458 to the end; 6-9 `[ADXIO]` lines; no `[FATAL-*]` |
| fatal path, no `title.adx` read | the repair run and no-VSH (merged executor, combiners on) | `JSRF_FATAL.ERR` at presents ~2429 |

- **The hold is not new.** It is the "Now Loading" hold the plan lists as unexplained, and R1/R2 show it on
  older toolkits.
- **R3 is the outlier.** It also has by far the best vblank delivery: ~49 pulses/s and zero late re-arms,
  against ≤ 23 /s for every other run and ~1.3 /s for R1/R2. That is consistent with a vblank-paced
  loading step, but one run cannot show it (**INFERRED**).
- **What is new is the fatal path,** on the merged executor with register combiners enabled. Its trigger is
  open. The `5d6ebbd` bisect could not build against this game tree.

### §26.5 The fatal path was the title's own 15 s load timeout; skipping it leaves only the hold

**Mechanism (MEASURED, disassembly).** `0x25310` sends a job to `0x6F730` only when all of these hold:
- `[esi+0x44] == 1`;
- the type's poll (`[0x1EC0F0 + 4*type]`) returned 0;
- elapsed microseconds since the job's start sample `+190/+194` are ≥ `0xE4E1C0` (15 s).

`+98` plays no part. The same 15 s test sits in `0x25400` (`0x255AD`) and `0x66440` (`0x6650B`, setting
bit 0 of `[esi+0xA4]`, which `0x664B0` turns into a `0x6F730` call). These are the only three
`0xE4E1C0` immediates in the XBE.

In `20261009-204648-520-title012-hold-a`, job `0x01330060` (type 1, poll `0x320A0`) had a start sample of
262.12 s of rdtsc, and the tail fired between `t_ms` 277 557 and 278 056 (INFERRED: just past 15 s;
two clocks with different origins).

That run used `RECOMP_NO_COMBINERS=1` and still went fatal, so the earlier "combiners on" correlation
(§26.4) was variation.

**L56 skips all three tests.** `20261009-213955-475-title012-timeout-off` (900 s, `just title-run`):
- no `[FATAL-*]` line, no crash;
- `title.adx` read (6 `[ADXIO]`);
- presents reach 2430 at 396 s, then hold at 2435 to the end (6 distinct hashes after 2430);
- stop 32 still not reached.

### §26.6 The "Now Loading" hold was APU lock starvation; fixed, the title screen renders

**Mechanism (MEASURED, frozen stacks of `20261009-213955-475-title012-timeout-off`):**
- The main guest thread is blocked entering the DirectSound critical section `0x001BA050`
  (`sub_0019E438` ← `sub_001A040A` ← …).
- That section is held by the audio thread (start `0x0013B1C0`). It is blocked in an APU MMIO write
  (`sub_001A3570+0x1DF` → `fe_method` → `voice_lock`, `apu_vp.c:148`) on the APU state mutex `d->lock`.
- The frame thread `mcpx_apu_frame_thread` holds that mutex for its whole loop. It releases it only in
  `throttle()`'s timed wait, which runs only when the thread is ahead of schedule. Our DSP emulation runs
  behind real time, and Windows locks are not fair.
- The audio thread starved for 89 s, then 381 s; the main thread only got the section between those
  holds. R3's single pass was a lucky schedule.

**Fix (toolkit `60bf20a`).** `apu_lock_handoff.h`: guest-reachable takers announce themselves
(`apu_lock_contended`), and the frame thread yields the lock to them once per frame (`apu_lock_handoff`,
bounded). Native test `apu_lock_handoff`: waiter latency 0.0 ms with the hand-off, 66–101 ms without it
on macOS. It is not a shortcut, so it has no ledger line.

**Result.** `20261009-221513-243-title012-apu-handoff` (900 s, `just title-run`) passes 2430 presents at
345 s and reaches 2747, with 318 distinct frames after the transition, 192 `[ADXIO]` lines, no crash
and no `[FATAL-*]` line. Its final dump, rendered as 640×480 R5G6B5, shows **the title screen**:
- surfaces `0x8011C000` and `0x801B2000` carry the JSRF emblem, the "JETSETRADIOFUTURE" logo with ™,
  and "Original Game © SEGA / © Smilebit/SEGA, 2002" over the city flythrough;
- the layout matches xemu's `jsrf-title-screen-xemu.png`;
- `0x80084000` is the bare city scene drawn beneath.

The reproduction `20261009-223657-233-title012-m15-repro` (900 s) passes 2430 at 359 s and reaches
2728. Its present dumps from 330 s show the logo assembling, still unfinished at 898 s, so the two
runs reached different points of the animation. Stop 32 has still not been exercised.

**M15, first observation.** `20261009-225354-960-title012-m15-long` (1800 s; present dumps after 900 s):
- passes 2885 presents at 1157 s, R3's old crash point;
- `0x00091830` returns with a clean ABI check (stop 32 confirmed);
- the present dumps from ~1580 s (`f_p0384`–`f_p0389`) show **"PLEASE PRESS START TO BEGIN"** with the
  emblem, logo and copyright line, as in xemu's press-start capture.

Kept on Windows in the run's `m15\` folder; `f_p0389.bmp` sha256
`5e663a97bd5f11d1596b16d74bb77ebf090d931141aaff0f4a3fb5387fa4d3b3`.

The run then ends at 1598 s / presents 3135 on `[ICALL] Failed to resolve VA 0x000BE190` (stop 33).

**M15 reproduced.** `20261009-232934-172-title012-m15-repro2` (1800 s, same build, dumps after 1300 s)
shows the same press-start frame (`m15\f_p0209.bmp`, sha256 `71ebdbd5…c419`). The six best press-start
band scores match the first run's exactly (522, 509, 498, 484, 471, 467). That run also ends on stop 33
(`0x000BE190`), at 1696 s / presents 3135.

