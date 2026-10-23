# JSRF technical record

The facts the project has established and that are still in force, by subsystem, with the evidence and
limits each rests on. The plan (`plan-jsrf-gameplay.md`) owns status and next action; this file owns
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


## §27. K16: the guest's own USB controller is live, and the pad enumerates (2026-10-10)

**The question.** M15 left the title at `PLEASE PRESS START TO BEGIN` with no way to press Start. The
retired plan recorded that `xbox_OhciInit` and `xbox_InputInit` were never called, and `TR §23.1`
concluded "no input can reach the guest at this stage". That is true of those two functions and
misleading about the guest: the *guest's own* USB path is live.

**The guest path is live, measured.** In the M15 reproduction `20261009-232934-172-title012-m15-repro2`:

- `jsrf_run.log:688` — `[KERNEL] KeConnectInterrupt: vector 1 -> routine 0x001C288F context 0x01092408`.
  That print is unconditional (not budget-gated), and vector 1 is `OHCI_VECTOR` (`ohci.c:52`), so the
  guest's XAPI connected its USB interrupt.
- The guest controller object at that context, read from the run's own dump:
  `0x01092408: FED00000 00001000 80081000` — OHCI base, size `0x1000`, contiguous DMA address.
- `0x001C288F` is the OHCI ISR: it reads `[ecx+0x10]`/`[ecx+0xc]` (HcInterruptEnable/HcInterruptStatus)
  and tests bit 31.

**The histogram is not evidence of absence.** `[KERNEL] summary` prints the top six ordinals and the
per-call `#n:` lines stop at the 200-call budget (`kernel_bridge.c:10074-10098`, `:470`), so a
once-per-boot call to ordinal 44/98/177 cannot appear there. Reading `ordinal 98 x0` as "never called"
is a sampling artifact. Measured instead, in `20261010-022809-116`: `ordinal 44 (slot 92)` returns `1`
with call site `ret=0x001BD137` — the instruction after the `0xFED00000` store in `sub_001BD108`.

**What was actually missing: the model behind the aperture.** With `xbox_OhciInit` never called, the two
4 KiB blocks at `0xFED00000`/`0xFED08000` are plain committed RAM, so `HcRevision` reads 0, no root-hub
port reports a device, and the pad is never enumerated. Two toolkit fixes were needed (ledger L57):

1. **Self-registration.** `xbox_OhciInit` unmapped the two blocks and relied on the embedder's VEH to
   route the faults, documented only in a source comment. It now registers its own handler (`FIRST=1`,
   so it runs before an embedder's crash printer), and the game VEH also carries an explicit
   `0xFED00000` branch so the behaviour does not depend on handler order.

2. **`bus_resolve` decides by the register-programmed range, not by an address test.** The driver hands
   the controller *physical* addresses (`MmGetPhysicalAddress` drops bit 31 for the contiguous arena,
   `kernel_memory.c:182`). JSRF's descriptors are LOW physical addresses — `HcHCCA=0x00081000`,
   `HcControlHeadED=0x000819A0` — which also fall inside `.text`'s VA range purely because the
   contiguous allocator numbers from 0. The old resolver tested the image first, so they resolved to
   code bytes: every ED read back garbage, `ohci_pad_for()` found no device, `ohci_do_td` moved nothing,
   and the controller re-armed the same transfer forever — `setups 0 reports 0` with
   `HcInterruptStatus`'s WritebackDoneHead raised thousands of times (run `20261010-020002-826`).

   Neither address test works, and the counterexample for each is measured. The image test sends
   JSRF's descriptors to code. The "is the window form allocated" test sends the driver's own `.data`
   descriptor buffer `0x002648DC` into the 1200K *surface* allocation, because `0x802648DC` is inside
   it: run `20261010-021943-631` read `0x002648DC` back as zeros while `0x802648DC` held
   `01100112 08000000` (the device descriptor, little-endian). So the range is taken from what the
   driver *programmed*: `HcHCCA`, `HcControlHeadED` and `HcBulkHeadED` each record their allocation's
   extent via `xbox_ContiguousBlockSize`, and `bus_resolve` redirects only inside that. `HCR` clears it.
   This is exact for a driver writing window addresses (DDS9) and for one writing
   `MmGetPhysicalAddress` results (JSRF, Burnout 3).

**Measured after, run `20261010-022809-116-k16-dma-range2`:** the emulated pad enumerates and configures —

```text
SETUP 80 06 value 0100 index 0000 len 8     GET_DESCRIPTOR(DEVICE)
IN ep0 8 bytes (8/8)
SETUP 00 05 value 0001 index 0000 len 0     SET_ADDRESS(1)
SETUP 80 06 value 0200 index 0000 len 80    GET_DESCRIPTOR(CONFIGURATION)
IN ep0 8 bytes (8/8) x4 -> (32/32)          four 8-byte TDs, one descriptor
SETUP 00 09 value 0001 index 0000 len 0     SET_CONFIGURATION(1)
```

**The next stop, and it is a new one (branch F).** The run then dies on
`[ICALL] Failed to resolve VA 0x001BE689` (stop 34, `config/stop-chain.json`), immediately after
`SET_CONFIGURATION`. `0x001BE689` has no function record and no prologue — an indirect-call target, the
same class as stops 29–32. Enumeration working is what made it reachable.

### §27.1 Stops 34 and 35: two more indirect-call targets, and the pad is polled (2026-10-10)

Both were reached only because the previous stop closed, so the chain advanced exactly one address
each time — the discipline that makes these cheap. Both have no analysis-database entry and sit in the
uncovered region `[0x001BD63B, 0x001BE7DE)`; the nearest declared span below is `sub_001BD621`
`[0x001BD621, 0x001BD63B)`.

| stop | address | extent | stack_args | shape |
|---|---|---|---|---|
| 34 | `0x001BE689` | `[0x001BE689, 0x001BE7DE)` | 4 | follows a clean `leave; ret 8` with no padding; takes its argument in ECX, builds a descriptor block at `0x002647D0` (callbacks `0x001BE388` + the ESI context) and submits it through the object in ECX; `pop ebx; ret 4` |
| 35 | `0x001BDE21` | `[0x001BDE21, 0x001BDE5D)` | 0 | follows `pop edi; pop esi; pop ebx; ret 8`; copies 8-byte records `[eax+0x34]`→`[eax+0x14]` for the count at `[eax+0x66]`; bare `ret` |

**Two ABI mistakes, both caught by the harness rather than by a run.** This is the check doing its job,
and both are recorded because the same two mistakes are easy to repeat:

- `stack_args` is the **`RET` immediate**, not the wrapper's expected delta. Stop 34 with `stack_args`
  unset generated `esp += 8; /* ret 4 */` against an expected `+4`:
  `ABI FAILURE 0x001BE689 esp 007BFF64->007BFF6C expected +4`. Setting `stack_args 4` produced
  `esp += 8` against `+8` — consistent. (Sibling `0x00011CE0`, also `ret 4`, has `stack_args 4` and the
  same `+8` body, confirming the convention.)
- The `end` must be one byte **past** the final `ret`. Stop 35 with `end 0x001BDE5C` stopped *at* the
  `ret`, so the body had no epilogue and returned with ESP unmoved:
  `ABI FAILURE 0x001BDE21 esp 00F7FB2C->00F7FB2C expected +4`. `end 0x001BDE5D` emitted
  `esp += 4; return; /* ret */`. Same shape as `0x00159330` (`ret 0xc` at `0x00159416`, end
  `0x00159419`).

**Measured on the durable tree `4370cd6`** (run `20261010-030748-775-k16-stop35-durable`, 189 s):

```text
[RECOVERED] 0x001BE689 returned; ABI verified (ESP/EBX/ESI/EDI)
[RECOVERED] 0x001BDE21 returned; ABI verified (ESP/EBX/ESI/EDI)
[OHCI0] stats: 5153 periodic transfers; reports 5153 setups 8 out0 6
ED 00081970 info=00201081 ...
```

5153 input reports served through `usb_gamepad_report` — the function the keyboard overlay (L58)
feeds — and the guest is polling the pad's interrupt IN endpoint `0x81`. A host keypress therefore has
a live path into the guest; what remains is the measurement that the guest's own title state reacts.

## §29. The D3D11 back end: the GPU rasterises, the title arrives ~3.4× sooner (2026-10-10)

**What it is (L59).** The executor already handed decoded batches to a registered back end
(`nv2a_backend.h`), and none existed. Toolkit `src/video/nv2a_d3d11_backend.c` implements one:
- render targets cached per guest surface;
- stage-0 textures decoded from guest memory, re-hashed per flip;
- a texture at a live render target's address sampled from that target on the GPU;
- a readback of every target drawn since the last flip into guest memory, at the flip.

Vertex work stays on the CPU. Texture filters follow `SET_TEXTURE_FILTER` (MAG 1 and MIN 1/3/5 nearest,
everything else linear; `nv2a_regs.h:1250-1253`).

**Parity** (`jsrf_gpu_backend_parity`, WARP), software against GPU after a flip:
- untextured: 100 %;
- nearest-sampled texture: 98.83 %; the mismatches are one diagonal edge band, a rasteriser edge rule;
- render-to-texture: 100 % for both surfaces;
- negative control: 0 % before the flip, 100 % after.

**Runs** (launched without the collector, see below; hardware Intel Iris Xe):

| | CPU rasteriser (M15 runs) | GPU back end |
|---|---|---|
| presents ≥ 2885 (R3's old crash point) | 1157 s | **347 s** (`gpube-a3`) |
| "PLEASE PRESS START TO BEGIN" | ~1580 s | **by 464 s** (`gpube-a3b`) |
| presents at the end | 3135 at 1591 s, then stop 33 | 4023 at 889 s, no crash |
| executor busy | 95 % pixel fill | 200 s of 751 s; vertex programs 96.8 s, screen-space/FFP 34.3 s, pixel fill 0 |

`gpube-a3`'s back-end totals: 599 368 draws, 128 M triangles, 3 990 flips, 7 979 readbacks (25.8 s), and
518 251 texture-cache hits for 1 326 uploads. The title frames are textured and filtered, closer to xemu
than the CPU path's. The press-start frame is kept on Windows in
`xbr-mac\direct-gpube-a3b\fb\f_p0348.bmp`.

**Known gaps:**
- stage 0 only: no combiners and no stages 1–3;
- culling ignored;
- mip level 0 only;
- runs that create the D3D11 device stall the run collector before `process.dmp`, likely on a
  GPU-driver thread; these runs used a direct launcher instead.

**Phase B: multi-texture and register combiners on the GPU.**
- **Seam:** `Nv2aBatch` gains `extra` (per-vertex specular, fog and `tex[4][4]`), `textures[4]`, `combiner`
  (the executor's live `Nv2aCombiner`) and `fog_color`.
- **Shaders:** the back end generates an HLSL pixel shader that mirrors `nv2a_rc_eval`, cached by the
  shape-defining registers.
- **Parity** (`jsrf_gpu_backend_parity`, program-mode batches, the only ones the CPU path combines):
  - multiply, lerp, two-texture multiply and specular-add cases all at 98.83 %, the same edge band as before;
  - the GPU's combiner multiply equals the fixed-function modulate surface exactly.
- **Run `gpube-b3`** (700 s, hardware, `just title-run` switches plus `RECOMP_GPU_BACKEND=d3d11`):
  - every one of 461 851 draws through 16 combiner shaders (425 ms of compiling, no failures, no fallbacks);
  - no crash; press-start by **526 s**;
  - executor busy 242 s of 694 s (vertex programs 79.8 s, screen-space/FFP 97.0 s).

The press-start frame matches xemu's capture in composition (the station canopy, buses, street). The
skater is drawn as a black silhouette, in the CPU path's M15 frames too, so it predates the back end.

Open:
- the black characters;
- linear textures on stages 1–3, whose CONTROL1/IMAGE_RECT methods the method table does not admit;
- fog on the fixed-function path;
- the collector stall.

## §30. Time to the title: 517 s to 164 s on the GPU path (2026-10-10)

**Method.** WPR CPU traces (`wpr -start CPU`, exported with `xperf -symbols`, the build's PDB plus Microsoft's
public symbols), read per thread. The main host thread runs the guest's main thread and the whole
pushbuffer walk and executor. It was 92-95 % busy for the whole trace, so its CPU is the time to the title.
The APU frame thread and `nv2a_ack_thread` each keep a core busy, on other cores (the host has 20).
PRESS START is found by `find_press_start.py`; it appears at about present 3122-3180 in every scored run.

| step (toolkit / game commit) | main-thread cost removed | PRESS START |
|---|---|---|
| baseline, `ac8951d` | | 517 s |
| method admission table: binary search, not a 600-entry scan (`cb21b8f`, `bab6203`) | 32 % | 377 s |
| DXT textures uploaded as BC1/2/3, not decoded per texel (`07697cc`) | ~6 % | ~350 s (presents) |
| pushbuffer page check kept across walks, SEH-guarded read; TSC executor timers (`1996f28`) | ~9 % + ~4 % | ~314 s (presents) |
| vertex program decoded once, not per vertex (`3958383`) | ~1 % | 311 s |
| `RECOMP_TRACE_BUDGET=0`: no per-call `[TRACE]` lines | file I/O on the guest threads | 167 s |
| one method check per word, with a one-entry cache (`2b7efdc`) | ~2 % | **164 s** |

Runs `direct-it1-bsearch` … `direct-it6-impl` (Windows, `xbr-mac`), all `RECOMP_GPU_BACKEND=d3d11` with
the title-path switches. The press-start and city frames match the earlier GPU runs' look.

**The trace lines were the largest single cost.** `config/trace-functions.json` traces 18 DirectSound
functions; their enter/exit lines were 216 k of a run log's 253 k lines, each `fflush`ed.
`RECOMP_TRACE_BUDGET=0` turns them off, and `play-jsrf.bat` sets it. `just title-run` keeps them, for diagnosis.

**Left on the main thread** (the 311 s profile, before the trace change):
- the CPU vertex-program interpreter, 26 %;
- the walk and commit, ~13 %;
- `xbox_GetMemoryOffset` calls from the executor, ~4.5 %;
- texture re-hashing per flip, ~2.4 %.

The next large step is running vertex programs and the fixed-function transform on the GPU.

### §30.1 Vertex programs on the GPU: 164 s to ~115 s (2026-10-11)

Vertex-program batches that run the combiners now go to the D3D11 back end untransformed, and the program runs as a translated HLSL vertex shader (L60; toolkit `07b723a`, game `71e0075`).
- **Measured:** 298k program draws through 21 shaders in a 200 s run, no compile failures or fallbacks.
- **PRESS START,** frame-confirmed in four runs: 120, 113, 116 and 115 s (`direct-apuirq-1..4`).
- **Main-thread profile:** `nv2a_vsh_run` is gone. The main thread is still ~94 % busy, now spread over attribute fetch (`fetch_attr`, `fetch_position`, `xbox_GetMemoryOffset`, ~21 %), the walk and commit (~16 %), texture hashing (6 %) and the fixed-function path (~5 %).

Two faults that the faster title run exposed were fixed on the way:
- **The packet-cap livelock (L54, toolkit `3648380`).** The cap is now a per-unit yield, which amends L40.
- **An intermittent boot hang at DirectSound init (L61).** The APU's interrupt never reached the guest's ISR.

The next large step toward 90 s is uploading the guest vertex arrays raw instead of fetching attributes on the CPU (Mercenaries `nv2a_pgraph_d3d11.c:3825-3862` builds the input layout from the NV097 formats).

## §31. From the title into the game: Start, the menu, NEW GAME, the opening cutscene (2026-10-11)

**K16 is met.** A real Enter press opens the main menu (NEW GAME / LOAD GAME / VS MODE) through the live keyboard path (L57/L58): `keyscript-k16-r3b` and `keyscript-newgame-2` (`logs/runs/`). The title ignores Start for a while after it first shows, so the driver re-presses every 6 s until the menu detector fires. NEW GAME is Z (Xbox A); A also advances dialogue.

**The driver** is `scripts/key-script-run.py` with `config/key-scripts/` (`title-start.json`, `new-game.json`, `intro.json`): it launches the exe directly (the collector stalls with a D3D11 device), recognises screens by frame content, holds real keys on the framebuffer window, and archives each run under `logs/runs/` with the metadata and source archive `check-stop-chain.py` reads.

**Choosing NEW GAME exposed 13 stops (36-48), one per run,** each closed by recovering the function from the XBE and confirmed by an ABI-verified return in a run built from the repair commit (`config/stop-chain.json`):
- indirect-only methods with no entry: `0x47600`, `0x352D0`, `0x38090`, `0x273C0`, `0x3A900` (a tail-call target), `0x44BA0`, `0x44C90`;
- recovery spans that swallowed another function (the stop-37 and stop-42 patterns): `0x307A0`/`0x30800`, `0x0A5050`/`0x0A5070`, `0x0A5AF0`/`0x0A5B60`;
- spans that cut a switch: `0x10A0E0` (case arm `0x10A0F9`) and its tail-call target `0x10A050`, and `0xF7F00` (case arm `0xF7F21`, with a false split `0xF8000`).

Three static batches were merged to get ahead of the one-per-run rate (all found by scans, not runs): a teardown-table and vtable scan (7), a whole-image function-pointer-table scan (72 recovered, 16 over-wide entries narrowed), and an over-run scan (20 recovered, 18 narrowed; the 33 SEH thunks left out). They repaired five `KNOWN_OPEN` members of `tests/test_recovery_span_ownership.py`.

**Two faults the longer run exposed, fixed in the toolkit:**
- the walk's word-budget yield tested only method headers, so a full unit ending at a jump/call/return pinned GET (L54, second case, `0b1dd40`);
- a heap block the guest made read-only (it does so every frame) kept that protection after `NtFreeVirtualMemory`, so the next allocation's zero-fill faulted (`7acde2c`, test `xbox_vm_reuse`).

**Where it stands:** `keyscript-newgame-16` ran 480 s with no ICALL, ABI failure, exception or walk rejection. `keyscript-intro-1` pressed A through the opening dialogue ("This is the GG's Garage. Hey, where's our pizza?") and reached the in-game HUD (spray-can counter, health bar, trick meter). **The 3D world is black in gameplay** though the back end is busy (746k draws, 157M triangles, no fallbacks, no undecodable textures); the cutscene's 3D background does render, dim. That is the next problem.

### §31.1 The Garage renders: the fade lifts and the floor draws (2026-10-11)

**The black world was not a renderer fault.** The frame trace (`RECOMP_FRAME_TRACE`, now also logging each batch's first vertices) showed every gameplay frame drawing the world, then a fullscreen quad of diffuse `(0,0,0,1)` (the game's world fade, vertex array `0x80083000`) just before the HUD. Depth was correct (matches xemu's `z / 2^24` for both the fixed-function and program paths). With that quad skipped the world showed, so the question was why the game never faded in.

**Cause 1: stale SSE-compare lifts.** The player's warp entry `0x92750` re-ran every frame, re-arming its 60-frame countdown and the fade to black. The warp-trigger test `0x0014C100` rejects a zero-length segment, whose direction is NaN, with `comiss xmm0, 0; je`. On the chip an unordered compare takes `je`. Eight generated bodies in `recomp_0003.c` predate the lifter fix `894ceef` and tested a bare `_fca == _fcb`, so the test reported a hit every frame. Fixed by relifting them (`scripts/relift-selected.py comiss`, `config/relift-comiss.json`; game `fb7aefc`).

**Cause 2: APU DSP scratch on the stage's vertex buffers.** The floor's vertex arrays were written at stage load, then overwritten by 24-bit DSP words. The sound library hands the DSP ordinary-RAM pages by physical address (`MmGetPhysicalAddress`, `0x01430000` up). `apu_translate` sent addresses below the contiguous high-water mark to the contiguous window, which by then held the vertex buffers. `MmGetPhysicalAddress` now records per page whether its latest answer was for ordinary RAM, and the APU honours that first (toolkit `293a97b`; `RECOMP_WATCH_WHO=1` attributes watch hits to a thread, and found the DSP).

**Result** (`20261011-180827-975-keyscript-groundfix1`): the fade lifts by itself, and the Garage draws with its floor, walls, fences and skyline, the player idling. **Still wrong:** characters render as dark silhouettes; stencil isn't implemented, so shadow overlays darken the whole frame; two batches with valid positions draw nothing (a shadow volume and an alpha-tested quad).

### §31.2 Stencil gates the fullscreen shadow darkening (2026-10-11)

Toolkit `4e302def8472fe17425bf28de117675a91a550b0` implements NV2A stencil
state in the executor and D3D11 backend: D24S8 storage, masks, comparison/ref,
fail/depth-fail/pass operations and stencil clears. This supersedes §31.1's
"stencil isn't implemented" observation; it does not settle the other rendering issues.

Exploratory run `20261011-194754-768-keyscript-stencil1` (420 s) shows the brighter
Garage in `gameplay-b.png` once stencil gates the fullscreen shadow darkening.
`result.json` records `keyscript_complete`, `script_ok: true` and zero ICALL,
fatal and walk-rejection counts. Metadata records game `33c3928`, toolkit `d37f47d`
with a dirty patch; the three archived toolkit source files compare equal to `4e302de`
after newline normalization. `check-run-profile.py` reclassifies the run EXPLORATORY
(default GPU acknowledgement enabled), not strict fidelity evidence. Existing title,
input and GPU compatibility paths include L16, L18, L47 and L56–L60.

Characters still appear as dark silhouettes. Shadows remain wrong; the backend still
uses `D3D11_CULL_NONE`. Missing culling is the next task, while the previously observed
shadow-volume and alpha-tested-quad batches remain separate unclosed observations.

### §31.3 Face culling restores character outlines and shadow-volume facing (2026-10-11)

The baseline was fetched and verified clean/synchronized: game
`2e1d6323c274b098184cb26cb065ece6551073e6`, toolkit
`4e302def8472fe17425bf28de117675a91a550b0`. Toolkit
`8e200100949ffb2b253a4ac7884c06a9d71d8713` implements the missing backend state;
the executor and `Nv2aRenderState` already carried NV097 `SET_CULL_FACE_ENABLE`
(`0x0308`), `SET_CULL_FACE` (`0x039C`) and `SET_FRONT_FACE` (`0x03A0`).

Surface y-down coordinates pass through `1-y*2/h` and the D3D11 viewport back
to y-down. CW (`0x900`) therefore maps to `FrontCounterClockwise=FALSE`, CCW
(`0x901`) to `TRUE`. The backend pre-creates cull-none and FRONT/BACK × CW/CCW
rasterizer states with the existing solid-fill, scissor and depth-clip properties,
then binds one on every transformed or GPU-program draw. `FRONT_AND_BACK`
(`0x408`) suppresses the batch without CPU fallback. Clears use cull-none.
This establishes front-of-eye winding; eye-plane-crossing fidelity was not measured.

Game `5bb8c839a468968aaf0d8785bd6935e2fb694a23` adds deciding WARP pixel fixtures:
separate red CW and green CCW triangles each cover 276 pixels. Disabled culling
draws both; FRONT/BACK and winding changes select the expected region; ALL draws
neither. This runs through fixed-function, GPU vertex-program and CPU-transformed
program paths, with same-target state alternation and a partial clear under ALL.
Inverting the production winding selection fails both parity CTests; restoration
passes both. The tests use the actual executor/backend, not a CPU culling oracle.

`20261011-221941-413-keyscript-culling-skate` uses the unchanged `skate.json`
and clean game/toolkit revisions above. Its `gameplay-a.png` compared with the
verified stencil run's `gameplay-a.png`/`gameplay-b.png` shows colored clothing
and skates with a localized shadow, replacing the dark silhouette. Forward motion,
both turns, camera following and an airborne jump pose are visible in the named
pre/post frames. It aborted after the jump on `ABI FAILURE 0x000FFFD0`; backward
steps were skipped. `script_ok` is false and exit code is `0xC0000409`, despite
the driver's ICALL/fatal/walk counters being zero. Those counters do not cover
every ABI abort. Per-frame window dumps improve capture cadence, not guest behavior.

The complete flip-to-flip trace in `20261011-222515-360-keyscript-culling-trace`
joins submitted methods → executor state → actual `RSGetState`/depth-stencil state
→ real draw queries, with an end-of-frame marker. On target `0x80084000`:

- Character body draws `1216629..1216632` request BACK/CW; the dark, texture-off
  3,168-index pass `1216633` requests FRONT/CW and binds D3D11 FRONT/CCW=false.
  Its interpreter reference diffuse is `(0.08,0,0,1)` and its GPU query reports
  823 passing samples. The same vertex buffer and slightly expanded reference
  positions identify the outline pass: drawing both faces previously covered
  the body. This is a separate culling use from the shadow volumes, not evidence
  that lighting or texture stages needed a change.
- Shadow-volume draws `1216699/1216700`, 3,529 strip indices each, request
  FRONT/CW with stencil INCR, then BACK/CW with DECR, depth GEQUAL and ZERO/ONE
  blending. Actual D3D11 states are FRONT then BACK, CCW=false, and matching
  stencil operations. Queries report 811,598 / 760,618 passing samples including
  overdraw. The following stencil-EQUAL fullscreen darkening draw `1216701`
  reports 51,300 passing samples. This is the predicted face-separated stencil
  behavior and agrees with the localized visual shadow. It is not a hardware
  shadow-fidelity comparison or a direct stencil-value readback.
- The historical alpha-tested quad (prim 8, ref 204, texture `0x810CB000`) is
  absent from this complete frame; it is not claimed fixed. Reinspection of the
  old groundfix trace puts its transformed vertices entirely off screen
  (x approximately -3124..-1538, y -2496..-910). Valid input positions and zero
  CPU pixels did not establish an alpha-test defect.

Backend occlusion queries count samples passing depth/stencil, not unique pixels,
and a zero count cannot disprove z-fail stencil effects. A 50 ms timeout is recorded
as UNKNOWN; several queries timed out, including body draw `1216629`. Executor
`cpu_px` is explicitly labelled and is not a GPU-output counter. Program outputs
in the trace are labelled interpreter references, not GPU shader readbacks;
constant-writing programs are never replayed for observation.

The runs remain EXPLORATORY (default GPU acknowledgement), relying on L16, L18,
L47 and L56–L60. Both use verified disposable save roots; original saves/assets
are preserved. Before the adjacent ABI repair, game CTest passed 56/56 and
`just check` passed; standalone toolkit CTest passed 20/20 on recheck (the first
run hit a guest-meter completion-event timing assertion), and its discovered
Python suite passed 235 tests with two skips and one expected failure. Culling
mutation/green receipts and representative frame copies are retained in ignored
`logs/culling-evidence/`.

PUSHED_TO: xboxrecomp / BRANCH: main / COMMIT: 8e200100949ffb2b253a4ac7884c06a9d71d8713 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: success, fast-forward from 4e302de.

PUSHED_TO: poison-jam / BRANCH: master / COMMIT: 5bb8c839a468968aaf0d8785bd6935e2fb694a23 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from 2e1d632; rendering evidence and the adjacent gameplay stop are recorded above.

### §31.4 Complete skate path after the adjacent recovery repair (2026-10-11)

The next measured blocker after culling was the jump/effect path's ABI failure at
`0x000FFFD0` (stop 49), not a further rendering-state divergence. Its recovered
span ended at `0x000FFFE1`, after six prologue instructions and three unmatched
pushes. Original XBE instructions establish the owner through `0x001005B5`,
with restoring `RET 4` exits at `0x001005A4` and `0x001005B2`. Game
`9b0af4eb8ec33cf11b1a191d7079f27a1d0fb6dd` restores that owner with stack_args 4,
removes eight false internal recovery/manual/dispatch entries
(`0x00100041/47/4A/4D/53/55/58/5A`), and removes the now-internal unresolved
dependency `0x00100080`. Recovery-span regressions pass; independent review
confirmed the full owner and both exits. The regenerated local retail-derived
output stays ignored. Five preservation-baseline hashes were refreshed with an
appended history entry after the full suite detected stale pre-repair hashes;
the preservation test was retained and passes.

`20261011-223618-932-keyscript-culling-skate-fixed` launches from clean game
`34553f94cbe7d92c83a0172d941a16024b32196d` (descends from the repair) and clean
toolkit `8e200100949ffb2b253a4ac7884c06a9d71d8713`. Its unchanged `skate.json`
completes all 71 steps by 302 s. The run continues to the 480.3 s diagnostic
deadline, with changing presents through 479 s, no observed ABI failure and zero
ICALL/fatal/walk-rejection counters. It records
`[RECOVERED] 0x000FFFD0 returned; ABI verified (ESP/EBX/ESI/EDI)`.
The deadline kill's exit code 1 is expected, not a normal guest exit.

Named pre/post frames support forward motion, both turning directions and camera
following. `post-06-jump.png` shows airborne/grind behavior, sparks and the
"FarSide Stab Soul" trick with score 100. Backward input changes position and
heading with camera response (`pre/post-08-back.png`); the character turns to
travel in the opposite direction rather than demonstrating literal reverse
skating. Colored clothing/skates and localized shadows persist. Representative
baseline, culling, jump and backward frames are retained in ignored
`logs/culling-evidence/` alongside the original archives.

Tracing was armed only after the final backward snapshot, keeping query stalls
out of all input steps. A complete flip 13616 includes draw `1546609`: the
historical quad's primitive/texture family (prim 8, four vertices,
`0x810CB000`, format 07), now partly on screen. It submits BACK/CW, alpha GREATER
with ref **0**, and binds D3D11 BACK/CCW=false. The real draw query reports
**1,139 passing samples**. Its first three surface positions span
x approximately -686..67 and y -736..17 with positive CW area. This disproves
a blanket claim that the quad family has no GPU output. It does not establish
the historical off-screen ref-204 case as fixed, nor require an alpha-test change.

This is EXPLORATORY evidence under the same L16, L18, L47, L56–L60 shortcuts as
§31.3. The archive verifies its disposable save root; original saves are preserved.
The measured rendering/movement checkpoint is reached. The adjacent measured
blocker was repaired and runtime-confirmed; no further fatal/ABI blocker was
observed within this bound. Graffiti, audio and save/resume remain unvalidated.
Measure them next rather than selecting a lighting/combiner feature from appearance.

Final validation after the boundary repair and baseline refresh: game CTest
**56/56 passed**, `just check` passed, and the focused recovery suite passed
11/11. Toolkit final-tree CTest is **20/20** on recheck and Python discovery is
235 tests (two skips, one expected failure), as qualified in §31.3. Both culling
parity tests pass after the production-mapping mutation is restored. Final
changes after the clean runtime launch only update evidence, stop confirmation
and measured preservation hashes; they do not change executable code.

The four outgoing game commits through `181697f` were audited individually:
13 changed-path entries and 13 reachable blobs, no forbidden paths/retail
signatures, no secret matches, and no blob over 100 MB (largest 3,255,627 bytes).

PUSHED_TO: poison-jam / BRANCH: master / COMMIT: 181697fdc99c210eeacf1449107b3e94fbcf5624 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from 5bb8c83; full skate, stop-49 confirmation and final checks close this checkpoint. Toolkit 8e20010 was pushed first and remains synchronized.

### §31.5 Packet 001: the Garage HUD counter read zero; the tutorial's route to a spray is unestablished (2026-10-12)

**Status: BLOCKED-ROUTE, corrected by Packet 001-FIX.** No graffiti interaction was produced.
The status was originally recorded as "the Garage issues no spray cans and contains no spray
target". Codex review classified that as an overclaim and Packet 001-FIX qualifies it here:
what was actually measured is that the HUD counter read zero on every observed frame, while
the Garage tutorial was never completed, Gum's `TE002` lesson was never obtained, and the
stage transition out of the Garage was never attempted. The route to a spray is therefore
**unestablished**, not established-absent.

**Baseline.** Game `fbb3d9b9807a559aad59a1720c5a5d795ff60abc`, toolkit
`8e200100949ffb2b253a4ac7884c06a9d71d8713`, both clean. All runs are EXPLORATORY under
`docs/jsrf-run-profiles.md`; the relied-on ledger IDs are **L16, L18, L56, L57, L58, L59, L60**
(L47 is inactive: no `RECOMP_FLIP_TRACE`). Run roots are under `logs/runs/20261012-*p001-*`
and are not committed.

**The R-trigger talk works, and its owner is Corn, not Gum.** The `?` balloon NPC beside the
player after skate steps 0–37 answers the R trigger with a speech balloon whose text is
`TE003.bin` — "Why don't you talk to her now?" (read from
`logs/runs/20261012-024119-788-keyscript-p001-e3b/hold-03.png`, the game's font rendering
`y` as `B`). `TE000/1` is the same character: "This is the GG's Garage… Find Gum… Just get
close to her and pull the Right Trigger." So the balloon owner is the NPC who points at Gum;
the earlier plan reading of that frame as "Gum" is corrected here.

**What the Garage opening actually teaches.** The talk scripts are plain on disk
(`game/Media/Event/TalkEvent/TE0NN.bin`). `TE002` is Gum's lesson and its only instruction is
`Press the [A button] to jump.` Across all 100 `TE*.bin` files, the only "Press the … button"
instructions are `TE002`/`TE177` (A: jump), `TE181` (Y: while moving), `TE191`/`TE201` (X:
grind). **No tutorial script teaches spraying, and none mentions graffiti.** The first
graffiti mission is `game/Media/Event/Event/e000.dat` — "This is Shibuya Terminal… cover up
that freaky-looking graffiti!" — a different stage.

**The HUD spray-can counter is zero throughout.** Band rows 84–108, cols 496–540, all
channels > 200. Every gameplay frame in every Packet 001 run reads exactly **140 white
pixels**, the documented `00` glyph pattern; 1954 frames read 140 and 296 read 0 (menus and
fades). The larger counts in that band (950–991, 1125) are not the counter: 1125 is exactly
45×25, the whole band, and those frames are dialogue frames whose speech balloon covers the
band (`talk-held.png`: 1125 px, `is_dialogue` true). No frame was observed at any non-zero
counter value.

**The R-talk follows the game's talk indicator (re-analysed in §31.6).** With `key_until
{vk 51, hold 1.2, until dialogue, every 3, max 4}` at the identical position: E3 opened the
talk on the first press (`talk-held.png`, 28 386 balloon px) and E11 opened eight; E16, E17 and
E18 opened none across six cycles each (`satisfied: false, presses 4`), including E18 which
used E3's exact parameters. This section originally called that "unreliable". §31.6 shows it is
proximity: the game's "talk available" balloon was on screen at press time in all 4 successful
presses and in none of the 38 failures, and a controlled closed-loop comparison opened a talk
2/2 while the open-loop method opened 0/8 with every press demonstrably delivered. The talk
needs the player to be close; the open-loop method rolls out of range first.

**Two real defects were found and fixed in the runner**, each with a negative control:

| Defect | Measurement | Fix |
|---|---|---|
| `is_dialogue` required a letterbox (top/bottom black ≥ 0.95). In-game talks have none. | Corn's TE003 talk: balloon 27 901 px at black fractions 0.066/0.007; cutscene pages 0.998. | `is_dialogue` = balloon count alone (`4737c18`; §31.6 later adds the icon-absent guard). |
| `key_until` released the key, then tested the screen. A talk that only shows while the key is held was never seen. | `[KEY] down vk=0x33` sent, `satisfied: false` in E1 while the balloon was on screen at t=247.4 s. | Test during the hold, then again after release. |

`is_hud` also now excludes dialogue (a talk still draws 830 bar-yellow px, so `key_until …
hud` would otherwise return on the dialogue it waits out), and `key`/`key_until` gained
`snap`/`snap_after` so a transient talk can be captured from inside the press. `hold_keys`
holds 2–4 keys at once. New tests pin the unletterboxed talk, the balloon-icon false
positive, the dialogue-excludes-HUD rule and the `hold_keys` parser; the guest-memory reader
is covered by a `VirtualAlloc`/`PAGE_NOACCESS` test and the search and acceptance checker
have their own synthetic-run suites.

**Tooling added (read-only observation).** `peek`/`ram_snap` read guest memory through
`ReadProcessMemory` at `g_xbox_mem_offset`; the positive control `0x00011000` read
`0x852C518B` exactly in E1, with 0 unreadable pages in a full 64 MiB snapshot.
`scripts/guest-ram-search.py` matches predicates across snapshots with controls, and
`scripts/check-graffiti-run.py` decides an acceptance/control pair. Both are committed with
their tests; they are not specific to graffiti.

**Inference, stated as such, and narrowed by Packet 001-FIX.** The original text inferred that
the Garage contains no spray target from three measurements — the zero counter, the tutorial
scripts' absence of a spray lesson, and the first graffiti mission naming Shibuya Terminal.
Packet 001-FIX review rejects that as overclaimed: the counter is a momentary observation of
one HUD value, the scripts are one class of asset rather than the stage's collision/target
data, and the Shibuya mission text says where the *first mission* is, not that the Garage has
no target. What the evidence supports is that no spray target was *found*, not that none
exists. Whether Gum's `TE002` line is reachable before leaving the Garage is unproven: no run
obtained her talk.

**The next step is a prerequisite, not a stage transition.** Packet 001 recorded leaving the
Garage as the next blocker, but that conclusion rested on the unestablished claim above.
Packet 001-FIX replaces it: the prerequisite is a reproducible Gum interaction and the
game-owned progression it causes, with a no-action control. Only if that prerequisite fails,
or if it succeeds and still yields no spray route, does the stage transition become the
measured blocker.

PUSHED_TO: poison-jam / BRANCH: master / COMMIT: bc35c02e0adacc8252b9f612d3efa9f4b6cc6134 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from fbb3d9b. Validation: CTest 59/59, `just check` all checkers passed, secret audit 0 hits over 216 blobs, no forbidden paths, largest blob 3,255,627 bytes. Toolkit 8e20010 is unchanged and was not pushed.

### §31.6 Packet 001-FIX: the Corn talk needs proximity; Gum not located on the sampled paths (2026-10-12)

**Status: BLOCKED — Gum not located on the sampled paths, with the Packet 001 variability
explained and both reviewed checker defects closed.** No graffiti interaction was produced. Gum's
`TE002` was not obtained. This section supersedes the variability paragraph in §31.5. Its
reachability wording is narrowed by §31.7: the sampled paths were open-loop wall-clock movement
with no camera steering and no guest coordinate read, so "not located" is not "not reachable".

**Baseline.** Game `4737c18bece9719d222f50876dfdf93e77efbffe` plus this packet's commits; toolkit
`8e200100949ffb2b253a4ac7884c06a9d71d8713`, unchanged. EXPLORATORY under
`docs/jsrf-run-profiles.md`; relied-on ledger IDs **L16, L18, L56–L61** (L61 is active because
`DEFAULT_ENV` sets `RECOMP_APU_TRAP=1`; §31.5's list omitted it). Run roots are under
`logs/runs/20261012-*p001fix-*` and are not committed.

**F1 and F2, the reviewed checker defects.** Both were reproduced first, then fixed, then pinned by
regressions that fail on the old checker.

| Defect | Reproduced mutation | Before | After |
|---|---|---|---|
| F1: the mapping control was never validated, only required to exist | `ctl-xbe-text` value 0, `matches` null, in both runs | `failures=[]` | `ctl-xbe-text value 0 is not the XBE control 0x852C518B` |
| F2: spray-block steps matched by kind only | a 1 s sleep in acceptance against 30 s in control, inside the block | `failures=[]` | `spray block step 900 differs: ('sleep', '{"seconds": 1.0}') vs ('sleep', '{"seconds": 30.0}')` |

F1 now checks the VA, size, value, `matches`, single occurrence and result.json/peeks.jsonl
agreement. F2 compares the options that decide what the guest experiences, inside and outside the
block, including the boundary peeks; the only permitted substitution is an action key against a
control sleep of equal duration, and the only normalization is `expect` on declared witness peeks.
The generic half moved to `scripts/keyscript_pair.py`, which the new
`scripts/check-progression-run.py` reuses; that checker also requires identical `exe_sha256` and
`xbe_sha256` and a verified save root, so a pair from different builds is not a control.

**The Corn-talk variability is proximity, not delivery.** The talk indicator is the game's own
"talk available" balloon: a blue face inside a white bubble above and to the right of the player.
Over 3784 archived Packet 001 frames the band reads < 10 blue pixels on 3516, 10–39 on 18 and
>= 40 on 250. Across every `key_until` R press from E2 onward, an icon at press time talked 4/4
times and never failed, while 38 of the 45 presses without one opened no talk — including every
E16/E17/E18 attempt, whose `post-00-fwd1` frame had shown the icon about a second earlier.

A controlled comparison held the executable, XBE, environment, save root, 36-step prefix and
`wait_screen hud` guard fixed, and varied only the positioning method. Every press was delivered
(`key_seen` 1, Δpresents +296 to +429, read from the executable's own `s_key_down` and
`s_present_serial`):

| Method | Runs | Attempts | Icon at press | Talks |
|---|---|---|---|---|
| Closed loop: walk to the icon, then press R | V1, V2 | 2 | present (blue 52, 58) | **2/2** |
| Open loop: hold forward 3.0 s, then press R (Packet 001's method) | V3, V4b | 8 | **absent (0) every time** | **0/8** |

The start-pose R hold, with no proximity, opened no talk in either closed-loop run. So the earlier
reading of "unreliable R-talk" was wrong: the talk is reliable and gated by proximity, and the
open-loop method rolls out of range before the press. That is rule 9c.2 of the plan — normal
prerequisite plus automation state — not an input-delivery or guest failure.

**A one-off crash, recorded and not repaired.** V4 exited at 58.6 s with `exit_code` 0xC0000005
after a `[EXCEPTION first-chance] ... fault=0x10000FFBE (read)` at a host RIP, during the
`TalkEvent` preload. The identical script rerun (V4b) ran the full 480 s with no crash, and V1–V3
read the same `TE007.bin` at the same point, so it is not reproducible. It is recorded rather than
repaired; a new recovery class is out of scope for this packet.

**The survey found no talkable NPC in the Garage.** The icon-guided closed loop was walked out
from the spawn in all four headings, six legs each, capturing the talk from inside each R hold.
No icon and no talk appeared in any leg: forward (G1), left (G4, after the detector fix), right
(G5) and back (G6) all returned `satisfied=False` for all six legs, and the four endpoints are
genuinely distinct places (mutual frame deltas 46–59). A full 360° rotation at the spawn (E6,
`scan-00..07`) shows the icon at exactly one heading — Corn's. Every archived run's icon appears
only at steps 36–41, the spawn. The player reached the open plaza and the Dogenzaka Hill sign, but
no second talkable character. **Gum was not located**, so her `TE002` lesson was not obtained and
no progression witness could be established.

**A detector defect found by using it.** The first version of `is_talk_icon` accepted the blue
Dogenzaka Hill location banner, so G2b "reached an icon" and pressed R at a banner, and G3 did the
same while reporting a successful walk. The count cap added first did not fix it — the banner's
text measures 44 px tall mid-fade, inside the real icon's 37–50 px range. The corrected test is the
bubble: a real indicator carries 583–857 white pixels around the blue blob, the banner carries
zero. Verified: all six archived real icons detected, all three banner frames rejected, and the
E2+ tally unchanged. The two runs spent before this fix (G2b, G3) are invalid as survey evidence.

**Not established.** Whether the Garage grants spray cans or contains a spray target: the zero
counter of §31.5 precedes Gum's lesson, the talk-script text is not stage data, and no stage
collision or target data was read. Whether leaving the Garage is gated: unmeasured. The Shibuya
Terminal transition was not attempted.

**The next blocker, measured.** A talkable NPC other than Corn is not reachable from the Garage
spawn by open-loop movement in any of the four cardinal headings, and the talk-available icon
appears at no other position on those paths. The next packet needs either a named route to Gum
from game data (an event or stage table naming her position) or a scripted search that steers by
the icon and by camera rather than by wall-clock holds.

PUSHED_TO: poison-jam / BRANCH: master / COMMIT: 2b6df90d40828637a77e313939072109aa522f6e / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from 4737c18 over four commits (b1056df, 36d5430, 47baa88, 2b6df90). Validation: CTest 60/60, `just check` all checkers passed, four test files green (18 + 6 + 28 + 12 tests), secret audit 0 hits over 219 blobs, no forbidden paths, largest outgoing blob 139,371 bytes. Toolkit 8e20010 is unchanged and was not pushed.

### §31.7 Packet 001-FIX2: Stg00 assets and Gum's model are resident; her activation state remains unidentified (2026-10-12)

**Status: PARTIAL — no Gum interaction or progression witness was obtained.** The sampled
forward path stopped at a wall; no guest defect or general unreachability was established.
No graffiti interaction was produced. This section narrows §31.6's
reachability wording and adds the data-first NPC/stage evidence that section lacked.

**Baseline.** Game `b0b43c911f3cb58c4a2c9ee77441cdf37851d13e` plus this packet's commit; toolkit
`8e200100949ffb2b253a4ac7884c06a9d71d8713`, unchanged. EXPLORATORY under
`docs/jsrf-run-profiles.md`; relied-on ledger IDs **L16, L18, L56–L61**. Run roots are under
`logs/runs/20261012-*p001fix2-*` and are not committed.

**The pair checkers accepted missing build identity.** `check_identity` compared `exe_sha256`
and `xbe_sha256` with `!=`, so two runs that both omitted the fields compared equal
(`None == None`) and a pair from unknown builds passed as comparable. Reproduced by removing
both fields from both metadata files: `failures=[]`. The fields must now be present and 64 hex
characters, per run, before the equality comparison runs. Negative controls: both fields
missing, both malformed, and a genuinely different executable.

**Stage asset evidence associates Stg00 with the Garage.** The spawn run's log opens
`Media\Stage\Stg00_*` after Stg10/Stg12 paths, and `Cache00.tbl` associates `Stg00` with the
Garage's talk events. These asset reads and the "Dogenzaka Hill" caption are observations;
neither substitutes for reading the active stage ID, which this packet did not establish.

**Gum's assets and talk script are resident; an active NPC instance is unproven.** Measured in the V1 run's
snapshots (`logs/runs/20261012-060300-903-keyscript-p001fix-v1/ram/`):

- `Media\Player\Gum.bin` and `Gum.dat` are opened by the game (23 "Gum" path lines in
  `jsrf_run.log`), and her model's bytes are resident at two stable guest VAs
  (`0x020F568C`, `0x023C192C`) in every snapshot.
- All four tutorial talk scripts are resident: TE002 (Gum) at `0x035611F0`, TE003 (Corn) at
  `0x021E7440`, TE004 at `0x021E7770`, TE005 at `0x0343B5D6`.
- **Corn's live talk slot `0x03562D30` is `0` in all eight non-talk snapshots and holds his
  TE003 text pointer during the talk.** That is a genuine talk-state variable.
- **Slot `0x03558090` holds the TE002 pointer constantly — in the talk and in all eight
  non-talk snapshots.** It is a resident table entry, not an activation state.

These reads establish loaded data, not Gum's active NPC instance or a monitored activation
variable. They cannot establish that her talk never activates. No Gum talk was observed in
the captured frames. Whether she is placed elsewhere in the stage, is gated behind
an unmeasured prerequisite, or is simply not an NPC at this point in the tutorial is **not
established** by this evidence.

**The forward path from spawn dead-ends.** A ten-burst forward walk (`logs/runs/20261012-081405-261-keyscript-p001fix2-past`)
moved through frames 0–6 (successive frame deltas 30–42) and then stopped dead at frame 7
(deltas 0.3, 0.1, 0.1) against a wall in a narrow alley. Only the first R press, at Corn's
position, opened a talk; the nine after it did not. Corn's "?" balloon is visible in the first
frame and no other NPC appears along the path.

**Corrected reachability claim.** §31.6 said no talkable NPC other than Corn "is reachable from
the Garage spawn". The supportable statement is narrower: on the sampled paths — four cardinal
headings and one forward walk-through — no second talkable NPC was located, and the forward path
terminates against geometry. That does not establish absence or unreachability, because the
sampled paths are open-loop wall-clock movement with no camera steering and the guest coordinates
were never read. The plan and §31.6's title are corrected accordingly.

**Not established.** Gum's stage position, her activation condition, whether she is an NPC at
this tutorial point, whether the Garage grants spray cans or contains a spray target, and whether
leaving the Garage is gated. The Shibuya Terminal transition was not attempted.

**The remaining investigation.** The sampled forward route from spawn ends against a wall, and the
sampled side headings show no second NPC. The next packet needs either a route read from game
data (an NPC or event table naming Gum's position, which this packet did not find: `Cache00.tbl`
is the only Media file referencing TE002/TE003, and the talk table at `0x021E7xxx` is a message
table, not placement data), or a steering search that reads the guest camera/position rather than
holding keys for wall-clock durations.

PUSHED_TO: poison-jam / BRANCH: master / COMMIT: 4f0d05b51e12bd7a06a102266e30ac1f3665f356 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from b0b43c9 over two commits (45c27aa, 4f0d05b). Validation: CTest 60/60, `just check` all checkers passed, four test files green (20 + 6 + 28 + 12 tests), secret audit 0 hits over 219 blobs, no forbidden paths, largest outgoing blob 144,676 bytes. Toolkit 8e20010 is unchanged and was not pushed.

### §31.8 Gum located from guest state: a live NPC waiting in her own talk zone (2026-10-12)

**Status: LOCATED. Gum is a live, talkable NPC, and no recomp defect is implicated.** The tutorial is
waiting for the player to enter Gum's talk zone and press R. No earlier run entered that zone; §31.6's
open-loop walks passed outside it. This corrects §31.7's open question about her activation state.

**Baseline.** Game `ee14665`, toolkit `8e20010`, both clean. EXPLORATORY; ledger IDs **L16, L18,
L56–L61**. New run `logs/runs/20261012-164534-838-keyscript-gum-a/` (10 full RAM snapshots across rest,
forward, turn and L-trigger moves); `20261012-060300-903-keyscript-p001fix-v1` re-analysed.

**Guest layout (measured).**

- `obj(id)` = `[[0x0022FCE0] + 0x98 + 4*id]` (7668 slots, read by `0x128C0`). These are heap pointers,
  so they move between runs.
- Player, Corn and Gum are character objects `obj(0x2C/0x2D/0x2E)` (vtable `0x1CCFF8`), each tied to
  its resident model file through `+0x2A4`/`+0xC90`.
- Character fields:
  - position `+0xCA4`, 3×f32 with y up;
  - heading `+0x34C` = (sin θ, 0, cos θ), with θ the low 16 bits of `+0xCC0`;
  - camera eye `+0x328` and forward `+0x334`;
  - state `+0xE50`: 1 for the player, `0x18` for an idle NPC, `0x19` for an NPC talking.
- The left stick is camera-relative, and the L trigger snaps the camera to the heading.
- NPC controllers are `obj(0xC+n)` (vtable `0x1CD570`): Corn `0x11`, Gum `0x12`. Talk sub-state
  `+0x8B4`: 8 idle, 9 talking.
- Talk zones are a list at `[0x0020C754]` (next pointer `+0x18C`), each a 6-plane box at `+0x50`.
- On the player: `+0x300` = 1 means inside a zone (the game's talk icon), and `+0x2FC` is that
  zone's id.
- Game mode is `obj(9)+0x5C` (`0xF` free roam, `0x1E`/`0x1F` scripted talk).
- Progress flags are bit banks in the static object `0x001EFFB0`, encoded per `0x39B50`.
- Corn's "talk slot" `0x03562D30` of §31.7 is the talk-UI object `obj(0x1DDE)+0x1A0`.
- The live mission data is `Media\Mission\mssn0101.bin`.

**Positions.**

| Who | Position (x, y, z) | Talk zone (x, z) |
|---|---|---|
| Player at spawn | (407.97, 0, 489.92), heading 286.9° | — |
| Corn | (243.37, 0, 540), 172 units ahead of the spawn | zone 6: x 194..293, z 490..588 |
| Gum | (−140, −9, 330), 571 units from the spawn, 33° left of the spawn heading, 9 units lower | zone 1: x −190..−90, y −13.8..41, z 280..380 |

**Trace.** Across Corn's talk, Gum stays idle (`0x18`/8) at a fixed position while Corn goes
`0x18` → `0x19` → `0x18`, and **no progress flag changes**. Corn's zone is chat-only: text TE003, and
game mode stays `0xF`.

**What advances the tutorial (static analysis, consistent with the measured flags).**

1. The free-roam frame `0x5BAD0` evaluates the mission's condition table. Pending condition 4 needs
   flag `0x70` (set) and "agent 0 inside the zone with `+0x17C = 1` and R newly pressed" (`0x6C680`,
   with the R edge from `0xA8150`/`0x6C640`).
2. Meeting it sets `0x71`, which arms event 8: a scripted talk through modes `0x1E`/`0x1F`.

The decisive check is therefore closed-loop: stand inside Gum's box (`+0x300 == 1`, `+0x2FC == 1`),
press R, and watch flag `0x71` and the game mode. Only if `0x71` stays clear there does a recomp
defect become a suspect.

**Not established.** Whether Gum's box (9 units below the spawn floor) is reachable on foot, and which
stage owns the Dogenzaka Hill banner (the stage loader isn't resident in gameplay; a snapshot at the
banner diffed against the spawn would decide it).

### §31.9 Gum's tutorial starts from real input: closed-loop steering reaches her platform (2026-10-12)

**Status: Gum's lesson obtained once, by real input; a reproduction run is recorded below.**
The player was steered to Gum's platform, pressed R inside her zone, and the game ran her scripted
talk. The talk ended on her jump lesson: "Let's see how much air you can grab. Press the A button",
then **GO!** with "Press the A button" on the HUD. EXPLORATORY; ledger IDs **L16, L18, L56–L61**.

**Run:** `logs/runs/20261012-203811-675-keyscript-gum-h/`, script `config/key-scripts/gum-tutorial.json`.
It is intro.json's 46 steps plus closed-loop `goto` legs:

1. to the rail at (345, 300);
2. with `jump_when_stuck`, to (180, 290), then (60, 320), then within 45 units of Gum;
3. then R, then eight A presses.

**Progression witness (bank 0, `0x001EFFB0+0x6A14`; flags-0..3 snapshots).**

| Flag | Spawn | At Gum, before R | After R | After the talk |
|---|---|---|---|---|
| `0x70` | 1 | 1 | 0 | 0 |
| `0x71` | 0 | 0 | **1** | 0 |
| `0x75`, `0x78` | 0 | 0 | 0 | **1** |

Game mode `obj(9)+0x5C` read `0x1F` (scripted talk) after R. That is the chain §31.8 predicted:
condition 4 sets `0x71`, which arms event 8, whose end sets the next flags.

**The route, measured from `goto.jsonl` tracks of runs gum-b..gum-h.**

- **Spawn walkway (y 0).** It is bounded to the west by a rail along x ≈ 332. Jumping while
  pushing into the rail sometimes grinds along it in an unpredictable direction.
- **Sunken trench (y −30, z ≈ 384–420, x −296..60).** It runs along the north face of Gum's
  platform. Falling off the walkway's west end (x ≈ −100, z ≈ 457) lands in it. Its west end is
  closed, and east of x ≈ 60 the floor drops to −60.
- **Gum's platform (y −9).** gum-h reached it from the trench: the stuck-jump sequence climbed out
  near (70, −31, 384) and the final leg ended at (−106.6, −9, 332).

The exact climb is not yet characterised; the route depends on where stuck jumps happen.

**Steering additions.** `goto` gained `jump_when_stuck` (a jump toward the target, alternating with
left and right sidesteps) and a stuck warm-up. Pure helpers `goto_stop_reason` and `stuck_action`
are tested in `tests/test_key_script_run.py`.

**Owner decision (2026-10-12).** Reaching Gum and playing her tutorial further is a **human test**.
The next work is XInput support, so the owner can play with a controller.

### §31.10 Controller input and the pause-menu stop chain (2026-10-12)

**XInput (toolkit `e365e04`).** A real Xbox One controller over Bluetooth enumerates as XInput port 0
on the Windows host. Through `RECOMP_USB=1` it drives the emulated pad that the game's own XAPI reads.
The owner's session printed `[INPUT] … buttons=0x0010` for Start pressed on the pad.

- The translation is the pure `xbox_PadFromXInput`, covered by the `pad_map` CTest.
- LB maps to White and RB to Black.
- Empty ports are polled once a second.
- The pad's rumble report `[0, 6, left16, right16]` now reaches `XInputSetState`. The XID
  descriptor's 6-byte output size and `FF FF FF FF` capability mask give the 16-bit layout.
  `RECOMP_NO_RUMBLE=1` turns rumble off (feature enablement, `docs/jsrf-run-profiles.md`).

**The pause menu froze (owner report).** Start in gameplay died on unresolved indirect calls, one
after another. Each stop below was found by the pause script (`C:\Users\logic\xbr-mac\pause.json`:
intro, then Start, Down/Up, Start, move, Start, B).

| Stop | Address | Class | Repair |
|---|---|---|---|
| 50 | `0x0005208A` | span cut short: `0x00052050`'s recorded end `0x00052088` stopped before its `jmp eax` mode-handler dispatch and its `pop esi; ret` | end → `0x0005208C`; `0x52050` removed from `KNOWN_OPEN` in `tests/test_recovery_span_ownership.py` |
| 51 | `0x00079740` | omitted function: after nop padding, absent from the database, a fatal stub | recovered `[0x79740, 0x79B47)`; recursive descent found 8 plain `ret` exits and 9 tail jumps |
| 51 family | `0x78CC0`, `0x79030`, `0x790D0`, `0x79140` | omitted tail-jump targets of `0x79740` | recovered; `0x78CC0`'s five-entry jump table at `0x79014` stays outside its span |
| 52 | `0x00114080` | omitted function, same shape | recovered `[0x114080, 0x1140CC)` in the batch below |

Before the fix, the cut-short `0x00052050` skipped its handler dispatch for **every** mode
through table `0x001F9C48`, not only the pause. The run
`20261012-221821-806-keyscript-pause-fix-2` logs `0x00052050` and `0x00079740` returning with
their ABI verified before stop 52.

**Omitted functions as a class, repaired in one batch.** `config/recovery-unresolved.json` listed
102 fatal stub targets after stop 51. Each was classified by its bytes:

- **25 omitted functions, recovered.** Each sits after a terminator plus padding, with one consistent
  `ret` immediate, no indirect `jmp`, and no database or recovered start inside its recursive-descent
  extent. That includes stop 52, the twelve `0x119C20`..`0x11B2F0` siblings, and DSOUND's
  `0x19F040` (`ret 8`).
- **0 truncated owners.**
- **77 ambiguous, left alone:**
  - 63 interior labels of their own database functions;
  - 3 unbounded indirect `jmp`;
  - 4 non-code bytes after `0x127810`;
  - 5 that are clean apart from a database false split or an overlap: `0x6FE40`, `0x6FFA0`,
    `0x70060`, `0xB8A60`, `0x111C10`;
  - 2 with bad exits: `0x77000`, `0xC1050`.

Regeneration added no new unresolved targets (102 → 77).

**Pause works.** `logs/runs/20261012-225247-299-keyscript-pause-batch` ran the whole pause script with
no `[ICALL]`, ABI failure or fatal, and `0x00052050`, `0x00079740`, `0x00078CC0` and `0x00114080`
returned with their ABI verified. The frames show:

- the pause map (Garage, Shibuya Terminal, Dogenzaka Hill, RETURN TO GAME);
- the side-menu highlight moving;
- gameplay resumed and the player moving;
- a second pause closed by B.

**Owner-reported rendering defects, not yet investigated.**

1. Before the title, the background behind the Smilebit logo is the wrong colour.
2. Dialogue text flickers and drops characters. That includes the opening Corn talk and its
   speaker name "Corn".

### §31.11 Fixes from the other JSRF ports: the Smilebit card, and a baseline for the dialogue clobber (2026-10-13)

The owner asked to port the fixes from andeecollard/jsrf-recomp and nworkers/reJSRF. Each one was
checked against our code first. The plan, with what already exists here and what doesn't apply, is
in the session plan. Records land here as each fix is gated.

**F1: the clear colour (toolkit `e7c2efb`, from jsrf-recomp `e2e739c`).**

- `SET_COLOR_CLEAR_VALUE` arrives already in the surface's format.
- The executor reduced a 16-bit value to 5:6:5 again, and the D3D11 backend used it as a 32-bit
  diffuse, so the white Smilebit card rendered cyan.
- In `20261013-000422-621-keyscript-talk-dwell`, the card reads (255,255,255).
- The dimmer frames of the same card at 5.7 s and 10.2 s, (41,44,41), suggest the logo fades already
  render. F3 (gamma) is to be confirmed before it is worked on.
- `tests/test_gpu_backend_parity.c` clears its 16bpp cases with a real 565 word now, and gained a
  clear-contract check.

**The dialogue-clobber baseline (for F2).** `config/key-scripts/intro.json`'s prefix was run, then
20 snapshots per page, 0.5 s apart, on Corn's first two dialogue pages:

- **The "Corn" name label:** visible in **1 of 40** snapshots (`p0-02`).
- **The balloon text:** wrong in 39 of 40. "He$, Ìhere's our pi%%a?" and "pi%%a gu$" are the page-1
  font's glyphs drawn over page-0 cells.
- The one correct frame is the one where the label shows.

jsrf-recomp traced the same pattern to the fence: `D3D_SetFence` (`0x00191390`) releases the
pre-increment `[dev+0x30]`. Our fence mirror publishes the counter itself (`fence_snapshot_commit`),
so `VertexBuffer_Lock` on the shared font buffer returns before the GPU has drawn it.

**Tooling.**

- `scripts/text-stability.py`: A B A flicker vs page turns, on window or present dumps, with tests.
- `config/key-scripts/e2e.json`: the owner's end-to-end run. Intro, skate forward, jump twice, pause,
  resume, skate, jump twice. It completed in `20261012-234748-502-keyscript-e2e-baseline` with no
  ICALL or fatal; x went 408 → 329 → 292 → 220 → 180.

**F2: the fence, fixed.** Toolkit `xbox_Nv2aFenceRelease`; game `src/main.c` opts in with
`XBOX_FENCE_MIRROR_RELEASES`; ledger L17.

- **How it works now.** The executor writes each `0x1D70` semaphore release (`D3D_SetFence`'s
  pre-increment counter, `0x001913AD`/`0x001913BE`) into `*[[0x19DCE0]+0x34]` in stream order, and
  the fence mirror stands down after the first release.
- **Why that is safe.** Vertex data is copied out of guest RAM synchronously at END on the
  PUT-writing thread, and the D3D11 backend never rereads guest vertex memory, so a release is written
  only after the draws before it have consumed their vertices.

| Run | "Corn" label | Balloon text |
|---|---|---|
| Fixed: `20261013-010408-599-keyscript-talk-dwell-f2b` | 40/40 | correct 40/40 |
| A/B, same build, `RECOMP_FENCE_MIRROR_LIVE=1`: `20261013-011950-899-keyscript-talk-dwell-f2-live` | 1/40 | defect back |

- **e2e:** `20261013-010916-545-keyscript-e2e-f2` completes with no ICALL or fatal.
- **The cost:** about 4–8% fewer presents (11042 vs 12017 over 559 s), since the title now waits
  where it never did. jsrf-recomp measured 1.3%.
- **Tests:** `fence_snapshot` (95 checks) and `jsrf_fence_release_runtime` / `_live` (real
  memory layout, tick and executor hook). The mutations that ignore the latch, or latch in live mode,
  fail them.

**Where the frame time goes (e2e-f2, 550 s), and why Phase S is not a speed fix.**

- **Already on the GPU:** every vertex-program batch (524,775, 0 declined to the CPU, 21 shaders,
  2.85 s total compile) and every combiner draw (593k, 15 shaders, 0.4 s).
- **Executor busy: 339 s.** That includes:
  - 123 s of fixed-function/screen-space work transformed on the CPU;
  - 90 s of per-vertex attribute fetch for program batches.
- **Render-target readbacks: 76.5 s** (21,835, about 2 per flip).

These, not shading, are the performance levers. The owner chose ahead-of-time code over live
translation (Phase S, then a library lift), which removes them at the source.

### §31.12 The vertex programs are static data: the ahead-of-time shader corpus (2026-10-13)

**F3 (gamma ramp) is dropped, measured.** `logs/runs/20261013-023140-810-keyscript-logo-fade` (the
present dumps from boot) shows smooth ramps:

- the SEGA card's mean brightness goes 0 → 11.6 → 0 over about 90 presents;
- the Smilebit card's background goes 0, 8, 16, … 246, 255 and back in steps of 8.

The fades render without a gamma ramp.

**Where the programs live.**

- **The game's own shaders:** 125 XDK vertex-shader functions in the XBE's `.data`, packed between VA
  `0x001FC730` and `0x0020B8F0`.
  - Each is a header dword (low word `0x2078`, high word the instruction count) followed by 16-byte
    NV2A instructions, with the last one FINAL.
  - They are reached through pointer tables at `0x20C508`/`0x20C598` and immediate pointers (e.g.
    `0x698D9`).
  - All of them go through one game wrapper into `D3DDevice_CreateVertexShader` (`0x18FF90`, called
    at `0x14E28A`).
- **Direct3D's own:** three pass-through programs at `0x19A258`, `0x19A318` and `0x19A3C8`, chosen
  at `0x1903A0`.
- **No transformation:** both paths copy the words unchanged (`0x18FC80`, `rep movsd` into
  `SET_TRANSFORM_PROGRAM`).
- **`game/Media/` has none:** all 3,220 files were scanned, and the only hits were one- and
  two-instruction noise.

**Counts.**

| | Programs |
|---|---|
| Static, total | 128 (126 distinct, no hash collisions) |
| Used by an e2e run (`20261013-023551-222-keyscript-e2e-vshcorpus`) | 21 |
| Matched on hash, length and input mask | 21/21 |
| Write `c[]` | 0 |

**Statically extracted programs match live ones.**

- `nv2a_vsh_program_info`'s hash and input mask depend on the instruction words alone.
- `nv2a_vsh_hlsl` fed the in-place XBE words produces byte-identical HLSL to the live run for all 21.
- All 126 compile with `dxc -spirv -T vs_6_0` and pass `spirv-val` (Vulkan SDK 1.4.357 on the host).

**The S2 corpus source is static extraction**, with a logged-miss fallback as the safety net. The
table keys on the hash and checks the word count plus a digest, because the hash is a 32-bit FNV.
`scripts/find-vertex-programs.py` reports offsets, VAs, hashes and masks only, never asset bytes.

**Vulkan notes for Phase V/S:**

- D3D's clip-space y points up, so use a negative viewport height or `-fvk-invert-y`;
- `SV_ClipDistance0` needs `shaderClipDistance`;
- emit `[[vk::location(n)]]` so the input location equals the attribute index.

**Not traced:** a D3D reload of a 12-instruction program from a device-held buffer at `0x198936`.

### §31.13 Phase V: a Vulkan backend (V0–V2, 2026-10-13)
**The owner's direction:** compile-time code over live translation, and Vulkan for a future Android
port. D3D11 stays only as the reference (memory `jsrf-native-vulkan-direction`).

**V0 (toolkit `9f39537`): build plumbing.**

- Vulkan is found from the SDK, and `xbr_add_spirv` compiles `.hlsl` with `dxc -spirv`, validates it
  and embeds it; a shader error fails the build.
- `vk_smoke` runs on the RTX 4070 and on lavapipe 26.2.4. Software-ICD tests need a non-elevated
  ctest (`C:\Users\logic\xbr-mac\ctest-limited.py`).

**V1: the backend core** (`src/video/nv2a_vk_backend.c`, `RECOMP_GPU_BACKEND=vulkan`,
`RECOMP_VK_DEVICE`, ledger L62).

- **Structure:** it mirrors the D3D11 backend function by function:
  - targets backed by guest memory, with load, flush on flip, and flush-before-sample;
  - depth per zeta;
  - the clear, including the clear-colour fix;
  - stage-0 texture × diffuse with alpha test and blend/depth/stencil/cull;
  - a pipeline cache with dynamic viewport and scissor.
- **Shaders:** compiled at build time only. `vulkan-1.dll` is loaded at run time.
- **E2E on the RTX 4070:** `…-112303-640-keyscript-vk-e2e-val` runs with no crash, ICALL or fatal,
  and with 0 validation errors and warnings (synchronization validation too).
- **Matches D3D11:** logos, title and menu, Garage geometry, HUD and the pause map.
- **Still wrong:** the combiner-shaded surfaces (cel-shaded characters, part of the city) are drawn
  as stage 0 × diffuse until V3. That fools `key-script-run.py`'s menu detector, so the stock e2e
  stalls at the title on Vulkan.
- **Speed:** vertex programs run on the CPU until V5, and flips were 6,036 against D3D11's 10,918 on
  the same script.

**V2: parity tests.**

- `tests/test_gpu_backend_parity_vk.c` (`jsrf_gpu_backend_parity_vk`, lavapipe) covers:
  - the clear contract;
  - textured, untextured, alpha-tested, culling, depth and stencil cases;
  - the 16bpp round trip, reshape and eviction;
  - flush-before-sample.
- **Mutations:** the cull winding, the clear expansion and flush-before-sample each fail it.
- **A known difference, an XFAIL:** a 16-bit target first cleared in a frame reads destination alpha
  0 where the CPU path reads 1, a deliberate mirror of D3D11 (`nv2a_vk_backend.c:1096`).

### §31.14 V3: the combiner uber-shader (2026-10-13)

**One pixel shader, compiled before runtime.** `src/video/shaders/nv2a_vk_rc.hlsl` holds `vs_rc` and
`ps_rc`, built twice by `xbr_add_spirv` (dxc + spirv-val at build time; `INVERT_Y` on the vertex entry
only). `ps_rc` interprets the batch's `Nv2aCombiner` from a per-draw uniform buffer statement for
statement with `nv2a_rc_eval`: the same register file `R0..R15`, the same input mappings and output
scale/bias, the same read-before-write order, the general stages, the final combiner, fog and all four
texture stages. There is no per-configuration generation and no runtime compile — `dxc` is never
invoked by the executable.

- **Uniform buffer:** `VbRcConstants`, 528 bytes, every member a 16-byte vector or an array of them, so
  C and HLSL cannot disagree on offsets. Four `static_assert`s pin the size and the offsets of `ts`
  (432), `ctl` (496) and `fin` (512), and the emitted SPIR-V's `OpMemberDecorate` offsets were read
  back and match exactly (0/128/256/384/400/416/432/496/512).
- **Descriptors:** the combiner pipeline has its OWN set layout (`dsl_rc`, bindings 0 uniform-dynamic,
  1-4 sampled images, 5-8 samplers) and pipeline layout, so the stage-0 layout, its shader and the whole
  stage-0 path are byte-identical to V1. The UBO is a slice of the existing 32 MiB stream, reserved
  together with the vertices and the descriptor set so no submission can fall between writing the UBO
  and the draw that reads it, and bound with one dynamic offset.
- **Pipeline kind:** a new `rc` field in `VbPipeKey` selects the 92-byte combiner vertex, the new
  shaders and the new layout. `vb_rc_supported` mirrors `gb_rc_key`'s rejection rule (stage modes above
  5, true cube maps), so an untranslatable batch is drawn the stage-0 way.
- **Counters:** `rc_unsupported` is gone. `rc_draws` counts combiner batches the uber shader drew and
  `rc_fallback` those that could not be translated. `grep rc_unsupported` returns nothing.

**Tests** (`jsrf_gpu_backend_parity_vk`, lavapipe, written by a different worker than the implementation):

- a 39-case sweep whose centre pixel is compared with `nv2a_rc_eval` called directly on the same
  registers. **Worst channel difference 0** (tolerance 1). Independent review subsequently found
  that the cases labelled mux and final complement/clamp did not enable those behaviors; the repaired
  discriminating coverage is recorded in §31.15. This original pass does not prove those semantics.
- six whole-surface cases against the CPU executor: the promoted former XFAIL lerp (98.83%), four
  stages with a linear and two swizzled textures plus an unbound stage (100.00%), three combiner draws
  with different state plus a stage-0 fallback draw in ONE submission (100.00%), PASSTHRU (100.00%),
  CUBEMAP-on-a-2D-texture (100.00%) and CLIPPLANE with both comparison directions (100.00%).
- **Five mutations each fail it**, with the shader restored byte-for-byte afterwards: an output
  scale/bias change, swapped input mappings 2/3, an inverted CLIPPLANE comparison, a removed
  projective divide, and a removed PASSTHRU saturate. The stage-1/stage-2 image bindings were also
  swapped to show the multi-texture case is load-bearing.
- The former combiner XFAIL branch is deleted: a colour disagreement now fails the run.

**E2E on the RTX 4070** (`20261013-185536-693-keyscript-e2e-vk-v3`, 560 s, stock `config/key-scripts/e2e.json`,
`RECOMP_GPU_BACKEND=vulkan` plus the Khronos validation layer): 0 ICALL, 0 fatal, **0 validation errors and
0 warnings**, 12,473 presents against the D3D11 reference run's 11,042, and the last `[GPUVK]` line reads
`rc_draws=1692697 rc_fallback=0` — every combiner batch that reached the back end was drawn by the uber
shader. The real main menu renders correctly (orange NEW GAME entry, JSRF wordmark, city behind), and the
title-screen characters are cel-shaded with their outlines.

**Initial gate failure, corrected by §31.15.** This stock run did not reach the Garage on Vulkan:
step 1's `key_until until=menu` accepted the
attract-mode city frame, which scores 601 orange pixels against a 600 threshold (measured on
`01_key_until_after.png`). The pre-V3 Vulkan run failed the same step the same way with a score of 665, so
this is pre-existing. The script continued on the wrong screens; it did not literally stop at step 1.
The initial diagnosis was incomplete: the title detector also accepted the white Smilebit logo.
Fixing both detectors is plan step 3. The 12,473 presents cannot establish a speed comparison with a
D3D11 run that traversed gameplay.
The other `[GPUVK]` counter that moved is `readback_ms`, 202 s of the 560 s frame budget, which is the
target readback path V1 flagged as future performance work.

PUSHED_TO: xboxrecomp / BRANCH: main / COMMIT: d23ca42964aca8cbb7097ff51271dacdab235c4b / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: success, fast-forward from 4f8a880. Validation: toolkit and game build via `scripts/build-jsrf.py`, full game CTest 68/68 with the Vulkan parity test running on lavapipe (not skipped), the standalone `nv2a_combiner` and `nv2a_vsh` projects green, `just check` all checkers passed. Upstream push URL is DISABLED and was not touched.
PUSHED_TO: poison-jam / BRANCH: master / COMMIT: d74ce42c5f0e0b5446871181cd5b0a17b7b24857 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: success, fast-forward from f842c6f over one commit. Validation: full CTest 68/68 and `just check` all checkers passed on the committed tree, the VK parity test re-run green (11.38 s), secret audit 2 hits over 6,498 blobs both pre-existing self-matches inside `scripts/secret-audit.py` and its documentation (commits 1b56942, 6a97c86; neither blob is in this commit), no forbidden paths, largest outgoing blob 168,662 bytes. Toolkit d23ca42 was pushed first.

### §31.15 V3 review repairs and the detector handoff (2026-10-13)

**Shared, uncommitted work.** Committed bases are game `3f414ba` and toolkit `d23ca42`. The owner
asked for the changes to remain in the shared Windows checkout and for an updated plan/hand-back
prompt, rather than a commit. No new push is claimed here. The original OMP remediation operation
ended `unknown_outcome` after the owner pause (`settle_stalled: queued_message_count=1`); its review
run was automatically stopped and refused a subsequent STOP decision. These independently checked
handoff findings do not rewrite that operation's bridge outcome as success.

**Two detector defects, not a measured renderer regression.** The menu's orange-only test accepted
the attract city. The title's two minimum-white tests also accepted the white Smilebit card: its
sampled white fill is 61–68% in the press band and 100% in the wordmark band, versus 19%/43% in the
measured real title. The repair retains the menu's dark-bar structural witness and requires title
white fill below 40%/70% respectively. Independently re-reading the labelled local corpus rejects
the logo/city negatives and accepts the illuminated real title and selected NEW GAME menu. Blinking
title text need not match on every frame; `wait_title` polls continuously. Other menu selections are
not the selected-NEW-GAME checkpoint this detector intentionally recognizes.

`tests/test_key_script_run.py` now runs **90/90**, including local archive checks (no skip). Independent
in-memory negative controls remove both title upper bounds or replace `is_menu` with its former
orange-only rule: **5 and 7 tests fail**, respectively, with no errors. Shared source bytes are never
modified by these controls. `config/key-scripts/e2e.json` remains byte-unchanged, SHA256
`72c0360f90f9b92d5e5ed1b2cadab2a206717d28139413f84ea0f940aa4f9575`.

**The three combiner-coverage gaps are repaired.**

- The mux cases now set the output mux-enable bit, derive alpha from an explicit alpha source,
  and separate sum, LSB and MSB selection with both outcomes. Oracle-value pins and pairwise output
  separation reject a fixture that stops exercising the behavior in its label.
- Final complement/clamp flags now occupy `final1 & 0xFF`. Separate pairs expose complement-v1,
  complement-r0 and clamp-before-mix; the unclamped result cannot be hidden by final saturation.
- A read-only `nv2a_backend_current()` accessor permits a direct backend-contract fixture with
  **four actually bound distinguishable textures**, followed by a draw combining all four.
  The pushbuffer fixture's real stage-3 admission limitation remains unchanged and documented.

Independent guarded build succeeds. The real lavapipe parity rerun passes in 11.48 s: **45/45**
semantic pixels and **5/5** direct four-texture draws, worst channel difference **0** against the
CPU oracle (tolerance 1), plus the six existing whole-surface cases. The separate worker's full-suite
receipt, inspected independently, reports **68/68** in 200.84 s with Vulkan running rather than
skipping. Scratch-copy shader mutations demonstrably fail the deciding test: ignore mux-enable,
invert mux selection, read final flags from the wrong byte, remove final clamp, bind texture 2 to
stage 3. Raw failures identify the corresponding mux/final/four-texture pixels; the baseline passes.
Production shader SHA256 remains `a895d00737d4727aa2d741271c048f84dc102001625921e8ce3f5b3b14c43571`,
and its bytes match the restored scratch source. No production mutation is retained.

**Garage-reaching stock runs, independently checked.**

| Archive | Backend | Title / HUD checkpoint | Presents / run duration |
|---|---|---|---|
| `20261013-204203-694-keyscript-e2e-vk-v3b` | Vulkan, RTX 4070 Laptop GPU | 213.5 / 449.6 s | 6,024 / 560.5 s |
| `20261013-205232-358-keyscript-e2e-d3d-v3b` | D3D11 reference | 123.7 / 360.1 s | 11,073 / 560.4 s |
| `20261013-211154-309-keyscript-e2e-vk-v3-handoff-val` | Vulkan RTX 4070, Khronos validation loaded | 86.8 / 317.3 s | 13,035 / 560.5 s |

Both archives verify **exploratory**, with the recorded executable SHA256
`1a17261c6a4b6d7eefc6a32849aa6196e833a11db05f490123403933e12cd990`, matching the independent rebuilt
executable. Both have dirty-tree source/patch identities, not new clean-head commits. The archived
runner matches the current repaired source. Both scripts complete with 0 ICALL/fatal counts and no
ABI failure. Bounded process termination is not a guest crash. `start.png`, `paused.png` and
`resumed.png` show the actual Garage, pause map and return to play; player jacket/skin/skate colours,
outlines and shadows agree in the corresponding start views. Movement is also measured by player
coordinates: Vulkan x/z goes 407.971/489.922 → 285.453/527.087 across the scripted phases; D3D11 goes
407.971/489.922 → 199.063/535.004. Different progress is consistent with timed input at different
render rates, not a frame-aligned equivalence result.

Whole-run averages are about 10.8 versus 19.8 presents/s, including loading/intro. These are bounded
observations, not a controlled steady-state comparison or a causal performance diagnosis. CPU vertex
program execution and synchronous submissions/readbacks remain candidates to measure; V5 removes
the former. The earlier 12,473-present title/menu-only run must not be compared as a Vulkan speedup.

The successful Vulkan archive above has **no enabled Khronos-layer setting**. Its zero validation
counters therefore cannot establish a validation pass. The final independent stock run
`20261013-211154-309-keyscript-e2e-vk-v3-handoff-val` explicitly enables the layer; Vulkan loader output
witnesses insertion of `VK_LAYER_KHRONOS_validation` from the SDK DLL. All **72 steps complete** by
360.1 s, including visually checked Garage player shading, a jump, pause map and return to play.
It continues through the 560.5 s bound with **0 ICALL/fatal/ABI failures and 0 validation errors or
warnings**. Last GPU counters: `rc_draws=1440727 rc_fallback=0`; 13,035 presents, readbacks 210.971 s.
The profile verifies **exploratory**, the isolated save root verifies, and the executable SHA256
matches the other runs and independent build. Player x/z moves 407.971/489.922 → 186.056/531.595.
Its roughly 23.3 whole-run presents/s differs substantially from the earlier Vulkan capture, so a
stable slowdown or speedup is not established by these runs. No runtime code changed between them.
Relied-on ledger IDs for the Vulkan path: L16, L17, L18, L47, L56, L57, L58, L61, L62; D3D11 reference
additionally uses L59/L60. These pragmatic exploratory results do not prove hardware fidelity.

**V3's shared-tree gates are verified; next:** V4 randomized combiner tests, V5/V6 static
vertex programs and their independent tests, V7 acceptance and V8 owner play. The shared repair tree
must be preserved and reviewed before committing. Session reports/mutation receipts remain outside
the repository; no retail screenshots, generated SPIR-V or owner assets were added to Git.

### §31.16 V4: randomized register-combiner states on lavapipe (2026-10-14)

**Shared, uncommitted work; no commit or push in this packet.** `tests/test_gpu_backend_parity_vk.c`
gains a randomized suite beside the unchanged 45-case sweep, five four-bound-texture draws and six
whole-surface cases. Production shader/backend source is untouched: `src/video/shaders/nv2a_vk_rc.hlsl`
SHA256 stays `a895d00737d4727aa2d741271c048f84dc102001625921e8ce3f5b3b14c43571`, and the toolkit's only
dirty files remain the two preserved read-only-accessor edits. No CMake change was needed; the suite
lives in the existing `jsrf_gpu_backend_parity_vk` target, and the CTest count stays 68.

- **Generator.** SplitMix64, one stream per case seeded from `(seed << 32) | index`; fixed seeds
  `0x20261013` and `0x9E3779B9`, 256 cases each; no `rand()`, time, environment, locale or
  accumulated-state branching. A case is re-derived from `(seed, index)` alone, which the receipts
  demonstrate: two independent runs print the same inventory `fnv1a64 0xAC0466A6CBF3C5D7` and an
  identical coverage block. 16 cases per submission on one 64x64 A8R8G8B8 target, 32 submissions per
  pass, through the executor's admitted methods only: stages 0-2 in NONE/PROJECT2D/PROJECT3D/
  CUBEMAP-on-2D/PASSTHRU with solid synthetic textures (stage 0 the linear R5G6B5 image addressed in
  texels, stages 1-2 the swizzled images addressed in [0,1]), stage 3 NONE/PASSTHRU because its texture
  methods are not admitted. A per-submission tail returns every combiner and texture register the suite
  touched to the never-programmed zero state, so the CPU pass's later deterministic cases inherit the
  same state the Vulkan pass's did.
- **Validity, derived from the contracts rather than assumed.** `rr_walk` re-derives the readable
  register set from the written words and rejects any state that reads a register no earlier stage
  wrote (only `r0.a` is defined at entry), that uses a forbidden stage mode or mapping, that repeats a
  destination, or that violates the final combiner's flag/byte rules; fog is 0 or 1 only, because
  `SET_FOG_MODE`/`SET_FOG_PARAMS` are not admitted methods. `rr_map_in`/`rr_map_out` are checked against
  `nv2a_rc_eval` itself over 75 probes (0 bad), and the r0.a initialisation rule is checked directly.
  Candidates rejected before counting: invalid 8, mux-selector margin 21, conditioning 8, clear-like 0
  (549 candidates for 512 accepted, so the states are states, not retries).
- **Coverage.** All **98** printed minimums are met, including 0..8 general stages (56-58 each), every
  input mapping (1959-2084), all six RGB and six alpha scale/bias encodings, AB/CD/both dot products,
  all four mux selections, blue-to-alpha both ways, register write/read ordering (388 cases, 4475
  reads), per-stage factors, the final combiner's SUM/EF forms and its 0x20/0x40/0x80 flags, fog on/off,
  and stage modes 0-4. Distinct quantised outputs: **328**. Every accepted case is drawn and compared;
  nothing is silently skipped and `RC_RTOL` stays 1.
- **Result.** Vulkan **512/512** cases drawn, 8192 pixels compared, worst channel difference **1**
  (tolerance 1); CPU executor 512/512, worst **0**; the direct-seam block adds **128/128** accepted
  cases with four bound textures (seed `0x51ED2701`, 2048 pixels, worst 1). The preserved deterministic
  results are unchanged (sweep 45/45 worst 0 in both passes, four textures 5/5 worst 0, six
  whole-surface cases passing). `[GPUVK] rc_draws=698 rc_fallback=1` is exactly the 58 pre-existing
  draws plus the 640 randomized ones, with 0 validation errors/warnings.
- **Mutations (scratch copy `C:\Users\logic\xbr-mac\pv02mut\`; the V3 receipts in `v3mut\` untouched).**
  Both required mutations change only the shader, and both were first measured to **pass** the
  preexisting deterministic suite — so the randomized cases are their only detector, which is the
  packet's explicit criterion:
  - alpha-portion sum computed as a product (`qab + qcd` → `qab * qcd`): **1104** randomized pixel comparisons
    fail (1024 executor-path + 80 direct-seam reported by the suite), first at seed `0x20261013`
    case 10, oracle `0,0,0,24` vs GPU `0,0,0,0`; deterministic-sweep failures: **0**.
  - alpha input mappings 2/3 swapped in `in_a`: **1840** randomized pixel comparisons fail
    (1536 executor-path + 304 direct-seam), first at seed
    `0x20261013` case 1, oracle `141,231,65,0` vs GPU `141,231,65,51`; deterministic-sweep failures:
    **0**.
  After each run the scratch source is restored and asserted equal to the production hash, and the
  restored rebuild passes.
  Codex independently checked the raw summary counters and corrected the initial report's totals:
  its 1025/1537 counts included summary failure lines and omitted direct-seam pixel failures.
- **Gates.** `just build` succeeded (executable SHA256 unchanged at `1a17261c…12cd990`); the targeted
  actual-lavapipe run exits 0 in 11.55 s with no skip; full CTest **68/68** in 207.6 s with the parity
  test run rather than skipped; `just check` exit 0. Exit 77 is never treated as a pass.
- **A negative result worth keeping.** Exchanging only A and B (or only C and D) in the shader is
  **not** a defect and no test can detect it: `A*B == B*A` and `dot(A,B) == dot(B,A)`. A measured
  swap-A-B build passes the whole suite. A genuine input-mapping mutation must change the mapping byte
  or exchange the operand sets across AB and CD; both such mutations fail.
- **Scope.** A fixture-grade host pixel comparison of the uber shader against `nv2a_rc_eval` on
  lavapipe. It is not a guest run under any `docs/jsrf-run-profiles.md` profile and proves no hardware
  fidelity. Randomized textures use nearest filtering only, and CLIPPLANE, modes above 5 and states
  outside the validity rules are deliberately not randomized. Receipts stay outside Git; no retail
  assets, generated SPIR-V or oversized blobs were added.

**V4's randomized combiner gate is measured on the shared tree; next:** V5/V6 static vertex programs and
their independent tests, V7 acceptance and V8 owner play.

### §31.17 V5 planning hold and UNKNOWN bridge timeout (2026-10-14)

Operation `op_0mv7t35cp477c8233c1e140b1` yielded, then ended `unknown_outcome`
with `settle_timeout`; supervision run `run_0mv7n55d5f6128e228923d76d` automatically
stopped. This establishes neither V5 completion nor a renderer defect. The last
parent-session assistant action is a pending planning-workflow question. A separate
Opus planner was stopped after OMP identified the conflict with the skill's ban on
a separate planning process. Its bridge-facing API exposes no plan-mode switch;
OMP's probe of `xd://propose` refused outside active plan mode. The documented
headless alternative was reported by OMP, and requires an explicit owner exception
to the current skill instruction before use.

Independent reconciliation found no V5 production changes or completion report.
The parity source retains SHA256
`af210fe78c8b02ec62b27ad18307eef6c20ad6913a20618187f978c0ec37ed62`;
runner, runner-test and both toolkit accessor hashes also match the preserved tree.
Both origins were fetched and remain synchronized at game `3f414ba` and toolkit
`d23ca42`. V3/V4 acceptance remains intact. No commit or push occurred.

Owner-requested configuration reload is a separate recovery action, not proof of
planning or implementation. Preserve context and edits before host replacement;
do not reuse the stopped supervision run or fabricate an UNKNOWN acceptance.
Retry only after resolving the planning mechanism and creating an authorized run
that retains the unused ceilings: eight further operations, two remediation cycles.
Recovery reports stay outside the repository.

### §31.18 Handoff cleanup and the V5 planning resolution (2026-10-14)

**The planning blocker (§31.17) is resolved by an owner decision, not a tooling change.**
`plan-jsrf-gameplay.md`'s remaining steps (V5, V6, N2, N3, V7, V8, N4, N6) are **pre-approved**, so the
executing agent runs them directly, without its own planning phase or a separate headless planner
(plan rule N1). Each step is sized to finish in one operation (N5).

**What was verified and committed.**

- Before committing, the shared tree passed on Windows:
  - `just build`;
  - the full CTest via `ctest-limited.py`, 68/68, with `jsrf_gpu_backend_parity_vk` running on
    lavapipe (15.6 s);
  - `just check`;
  - `tests/test_key_script_run.py` 90/90.
- **Hashes:** the shader (`a895d007…`), the parity test (`af210fe7…`) and `e2e.json` (`72c0360f…`) match
  the values §§31.15–31.16 quote.
- **Spot-checks of V4:** `rr_walk` rejects reads of registers no earlier stage defined (RGB and alpha
  sets kept separately), stage-3 modes the executor can't bind, non-zero unused stages, duplicate
  destinations and unknown scale encodings. Each randomized batch is pushbuffer-submitted through the
  executor (`build_rr_batch` → `submit`), with sentinel-prefilled surfaces.
- **Commits:**
  - toolkit `4753474`: the `nv2a_backend_current()` accessor;
  - game `2e8b84a`: the detectors;
  - game `434dde1`: the V3 repairs and V4;
  - then the records.

**Cleanup.**

- The completed title-screen and keyboard-input plans are removed; they remain in git history at
  `3f414ba`, and references were reworded.
- `plan-jsrf-gameplay.md` is condensed into the sole plan.
- Ledger L62 is condensed to its contract and status.

**New plan items from what stalled or misled the last run.**

- **N2, a controlled benchmark:** whole-run present totals varied from 10.8 to 23.3/s on Vulkan with
  one build, so they can't support a performance claim.
- **N3, validation discipline:** a run whose loader didn't witness the Khronos layer can't claim a
  validation pass, as with the first successful V3b run.
- **N4, readback and synchronisation:** 202–211 s of each 560 s run is readback.

### §31.19 V5: the static vertex-program corpus on the Vulkan path (2026-10-14)

**The title's 128 static vertex programs (126 distinct) now run from a table compiled before the
process starts, and the Vulkan back end never translates or compiles anything.** V5 of
`plan-jsrf-gameplay.md`; it supersedes §31.12's "still run on the CPU interpreter" and L62's
`draw_program` NULL.

- **Generator (game).** `scripts/gen-shaders.py` imports `find-vertex-programs.py` by path and reuses
  its scan (`xbe_sections`, `scan_functions`, `scan_referenced`, `program_info`, `plausible`) rather
  than copying it, so the extraction cannot drift from the tool the corpus was measured with. It
  aborts on any change to the measured properties: 128 programs / 126 distinct word sequences / 126
  distinct hashes / 0 collisions / 0 programs writing `c[]` / len 1..136 / mask popcount 1..16. It
  runs the toolkit host tool and `dxc`/`spirv-val` offline and writes
  `src/recomp/gen/shaders/nv2a_vsh_programs.{c,h}`, which stay untracked like the rest of
  `src/recomp/gen/`. `just shaders` is the only spelling; CMake refuses to configure without the
  output, naming that recipe.
- **Host tool (toolkit).** `nv2a_vsh_hlsl_tool.c` links `nv2a_vsh_hlsl.c` and adds `--vk`: nine fixed
  rewrites plus one per set mask bit, each required to match exactly once or the tool exits 3. It also
  rejects a `--mask` the instructions do not imply, so a mismatched mask cannot produce an
  unselectable shader. Built by `src/video/CMakeLists.txt` outside `if(XBR_VULKAN_FOUND)`.
- **Corpus identity, independently re-derived.** Every entry carries `hash`, `len`, `input_mask` and a
  64-bit FNV-1a digest over its own words (`nv2a_vsh_aot.h`). `scripts/check-vsh-aot-table.py` is a
  separate read of the artifact: it recomputes each digest from the emitted words, rebuilds each
  shader from those same words with the real host tool and `dxc` and compares the bytes with the
  SPIR-V stored beside them, validates every stored module with `spirv-val`, checks the table is
  strictly ascending with no duplicate, and re-scans the XBE for coverage. **126/126 entries
  recompiled byte-identical; 126 XBE programs, none missing, none extra.** Corpus
  `sha256 31378834c81a819ff8c902ff17dd7a8077c11ce267d01a321491a6f4175ce741`, 4,173,304 bytes,
  1,158,080 SPIR-V bytes. `--check` regenerates byte-identically (determinism). Reproduced: with
  byte 12345 of the corpus XORed with 0xFF, `--check` reports
  `differs (committed ... sha256 7e3fb464..., regenerated ... 31378834...)` and exits 1; the byte was
  then restored and the sha asserted back to `31378834...`.
- **Decorations, verified in the emitted SPIR-V.** `[[vk::binding(9, 0)]]`/`[[vk::binding(10, 0)]]`,
  `[[vk::location(n)]]` on each input at its attribute index, outputs at locations 0..6 matching
  `ps_rc`'s inputs, and `SV_ClipDistance0`. All 126 entries were checked, not a sample.
- **Back end.** `vb_draw_program` looks the batch's program up by hash with a binary search, then
  re-checks `len`, `input_mask`, the entry digest, the digest recomputed from the batch's words and
  the words themselves; any mismatch is an `identity_fail` and a decline, never another program's
  shader. A miss returns 0 so the executor's CPU interpreter draws the batch, and logs the hash once.
  The pipeline key carries `prog_hash`, because 126 programs share only 13 masks and a mask-only key
  would serve one program's shader for another.
- **Three defects found and fixed during V5**, all in the back end and all reported with evidence
  rather than caught by the author's own tests: the stream buffer lacked
  `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` though the program path binds an index buffer from it; the
  descriptor pool reserved 2 dynamic UBOs per set where the program set uses 3 (binding 0, 9, 10), so
  program draws would have begun failing partway through a run; and `VbPipeKey` had no program
  identity, so two same-mask programs shared a pipeline and one was drawn with the other's shader.
- **Gating test, authored by a different worker** (`tests/test_vsh_aot_programs.c`, target
  `jsrf_vsh_aot`, registered in the game's CTest). Six checks: corpus identity against the live
  interpreter; a real table hit through the GPU with positive `hits`/`tris`/`const_uploads`, the
  first-hit latch and a CPU pixel comparison; a miss that falls back with the CPU still drawing and
  the once-per-hash line present; a corrupt `len` and a corrupt digest each declined with
  `identity_fail` and no hit; two same-mask corpus programs producing their own pixels; and two
  negative controls. **Result: PASS on lavapipe (3.2 s), and both negative controls exit 1 with the
  check correctly failing the injected defect.** The controls' own value was reproduced, not merely
  reported: with check 1 neutralized in a scratch copy of the test, control 1 prints
  `check 1 MISSED the simulated defect` and exits 2, and a CTest case using the registered
  `PASS_REGULAR_EXPRESSION`/`FAIL_REGULAR_EXPRESSION` then reports that case as **Failed**, which is
  what `WILL_FAIL` could not have done.
- **External mutation, applied in place to the shared production source** (not a scratch tree), to
  the backend as it stood at `f1609da6`, i.e. before the review's S1 reorder: the pipeline key's program
  identity removed *and* the guard that refuses a key/entry mismatch disabled, so the key silently
  aliases programs. The gating test **fails** with
  `[pair] EC6C788B and F2833A7D produced the SAME surface on the GPU though the interpreter separates
  them: the pipeline cache is serving one program's shader for the other` and
  `gpu surfaces differ in 0 of 4096 pixels`. Source restored to
  `sha256 f1609da67c3889607e699eeac3076dba8b7c447719eb839c9ea9d2067c34ea47` (asserted equal) and the
  restored build passes. The mutation covered the **pre-S1** backend: the S1 lookup reorder landed
  afterwards, and the shipped backend is `32d83dd0`. The gating test was re-run green on that final
  build but was **not** re-mutated there, so the mutation evidence is for `f1609da6`.
- **e2e gate.** The unchanged stock `config/key-scripts/e2e.json` on Vulkan (`e2e-vk-v5`, 560 s):
  script complete, 72 steps, no unsatisfied step, 0 ICALL/ABI/fatal, exploratory profile.
  **`vsh_misses=0` with positive activity: `vsh_hits=1238406`, `vsh_tris=329351108`,
  `vsh_const_uploads=1238406`, `vsh_identity_fail=0`, `vsh_declines=0`**, and the executor's
  independent summary agrees (`1237449` batches / `329098067` triangles drawn by the back end, 0
  declined to the CPU). Zero misses with zero activity would not have been proof, so the activity is
  the gate. The run's `exe_sha256` equals the executable the final tree builds. The earlier run
  `20261014-135941-640-keyscript-e2e-vk-v5` carries the same verdict on the pre-review-fix build.
  `scripts/check-vsh-run.py` encodes exactly that and reads both sources; its fixtures include the
  zero-activity case, the disagreeing-sources case and a spoofed line. Both it and the table checker
  are registered with CTest, so neither can be skipped by not being run.
- **Also fixed while here:** `jsrf_vsh_aot_test` and `just shaders` were added to the checkers'
  expectations (`scripts/check-agent-docs.py` requires the recipe; `AGENTS.md` documents it).
- **What the V5 run does and does not claim.** (Superseded by §31.20: the repaired build's run is
  `20261014-165814-809-keyscript-e2e-vk-v5r`, exe `23fbe93d`, with the same verdict. The text below
  describes the pre-repair run and its own claims.) `20261014-144815-512-keyscript-e2e-vk-v5` set only
  `RECOMP_GPU_BACKEND=vulkan`: no `VK_INSTANCE_LAYERS`, so its `validation_errors=0` counter is
  vacuous and it makes **no** validation-pass claim (plan N3 belongs to V7). Its claim is the
  vertex-program gate: 0 misses with 1.24M corpus-shader draws and 329M triangles. The
  D3D11-matching-shading comparison and the 0-validation-errors result are the **V3 handoff run's**
  (`20261013-211154-309-keyscript-e2e-vk-v3-handoff-val`), not this one.
- **Independent review (a reviewer that wrote none of it) found five defects; all were resolved.**
  - *A fresh checkout could never build* (P1): the generator looked for the host tool only inside
    the game's build tree, and that tree cannot be configured until the corpus exists, so the two
    steps deadlocked. `gen-shaders.py` now configures and builds the toolkit's `nv2a_vsh_hlsl` target
    itself (into gitignored `build/vsh-tool/`) when the tool is absent, and the bootstrap was verified
    by hiding the tool and regenerating.
  - *The table checker never checked an entry's 32-bit hash against its own words* (P2): a table whose
    rows kept their hash columns while their words and SPIR-V were swapped passed it. It now
    recomputes the FNV-1a 32 the back end searches on, requires the array names to match the hash, and
    a fixture pins it; the reviewer's exact swap input is now rejected.
  - *The gating test could report SKIP for an already-failed check* (P2): a failed corpus check
    followed by an unavailable device exited 77, which CTest counts as "Not Run", so a broken corpus
    stayed green. A skip now requires every check so far to have passed.
  - *The gating test never compared its second same-mask program with its own CPU reference* (P2), so
    a table with that program's SPIR-V swapped would still pass. Both programs now get the same
    comparison, and the author demonstrated the new check catching the swap the old one missed.
  - *A program the corpus lacks could be counted as a decline instead of a miss* (P2) when the batch
    also failed a setup check. `nv2a_vk_backend.h` defines `declines` as "found, but this draw could
    not be set up", so the lookup now precedes the setup checks: only a program the corpus carries can
    be a decline.
- **Reviewer verdicts worth keeping:** no translation or compilation at run time on the Vulkan path
  (`nv2a_vsh_hlsl(` is called only by the offline host tool and the D3D11 back end;
  `nv2a_vk_backend.c` has no dxc/D3DCompile/shaderc/glslang and creates its program module from
  `e->spirv`); identity safety holds; the pipeline cache key is sound (20 `uint32_t`, static-asserted,
  no padding); no VUID-class error found, including a gating-test run under
  `VK_LAYER_KHRONOS_validation` reporting 0 errors and 0 warnings.
- **Not claimed:** no hardware-fidelity claim, and V6's synthetic V1-V7 HLSL equality/pixel/digest
  suite and V7's final acceptance are separate steps. The run is exploratory, not strict.

### §31.20 V5 repair: the rejected-identity diagnostic walked the storage it had rejected (2026-10-14)

**An independent Codex review rejected V5 on one defect, and it was real.** `vb_program_lookup` rejects
a corpus entry whose `words` is NULL or whose `len` disagrees with the batch, increments
`identity_fail`, and returns NULL so the CPU interpreter draws the batch. It then called
`vb_program_log_once(p, e)`, whose identity-failure diagnostic computed
`nv2a_vsh_aot_digest(e->words, e->len)` — traversing exactly the storage it had just rejected. NULL
`words` dereferenced NULL; an oversized `len` read far past the entry. Both reproduced as
`0xC0000005` by a probe that compiles the two production functions verbatim with the real headers
(`pv04-identity-probe.py`, receipt under `xbr-mac/pv04-codex-identity-probe/`). This is isolated
function evidence; no guest crash was observed.

- **The fix** removes the digest call entirely. The diagnostic now chooses a short reason from scalar
  fields only (`the instruction count differs` / `the input mask differs` / `the corpus entry has no
  words` / `the corpus entry has no shader` / `the words or digest differ`) and prints the entry's
  `len`, `input_mask` and `digest` beside the batch's. It reads no pointer field's target, does no
  length-derived arithmetic, and adds no clamp or guessed guard. `vb_program_lookup`'s accept/reject
  decision, its counters and its `return NULL` are byte-for-byte unchanged.
- **No second out-of-bounds read.** The reject condition is a short-circuit chain, and every
  dereference is guarded by an earlier term: `e->len != p->program_len` first, then
  `!e->words || !e->spirv || !e->spirv_size`, and only then the digest of `e->words` for `e->len` and
  the `memcmp` of `e->len * 16` bytes. That ordering is load-bearing and is now commented as such.
- **Verification.** The Codex reproducer now exits 0 in all three modes (`valid` selects,
  `null-words` and `oversized-length` give `selected=0 identity_fail=1` with the new reason text).
  A **different worker** authored a regression through the real backend/executor seam
  (`tests/test_vsh_aot_malformed.c`, `jsrf_vsh_aot_malformed`) that copies the corpus in memory,
  corrupts only the copy (`words = NULL`, `len = UINT32_MAX`, `len ± 1`, missing shader), and asserts
  for each: `identity_fail` advances by one, no new `hits`, the executor's CPU interpreter drew the
  batch (real pixels), the process survived, and the real table is untouched — then restores the real
  table and re-asserts the positive control. It **fails on the unfixed `32d83dd0` and passes on the
  repaired `5de9f892`**.
- **Independent reviews of the repair.** A code review found no defects: it walked the short-circuit
  chain term by term and confirmed every dereference is guarded by an earlier one, that rejection
  semantics are unchanged, and that no blanket success or guessed guard was introduced. It also
  flagged that the ordering was load-bearing but undocumented, so the comment above the condition now
  says so explicitly. A security review of the malformed-entry/bounds/diagnostic handling reported
  **no demonstrated and no suspected findings**: every read and write in these functions is bounded by a
  named check (`memcmp` and both digest calls gated behind the `len` and
  `!e->words` terms; the `(size_t)` cast applied before the multiply, so no overflow; the binary
  search clamped to `[0, s_nprograms)`; `va_prog[16]` bounded by the `bit < 16` loop; the miss-log
  array bounded by its own check; the stream arithmetic bounded by `VB_STREAM_BYTES`). One residual
  trust assumption is recorded as such: an entry's `spirv_size` is its own claim, which is why the
  generator invariant matters — I verified all 126 generated `vp_words_<tag>[N][4]` arrays have
  `N == len` exactly (0 mismatches), so the "words shorter than len" case does not occur.
- **The deciding production mutation was re-run on the FINAL repaired source, in a scratch tree**
  (`xbr-mac/v5mut`, its own toolkit copy and build dir; the shared production file was never mutated
  in place). Three states, hashes pinned in the final measured receipt: repaired baseline
  `84f987cd` **PASS**; mutant
  (`dr.key.prog_hash = 0` plus the key/entry guard disabled) `d8a41429` **FAIL** with
  `gpu surfaces differ in 0 of 4096 pixels` and `the pipeline cache is serving one program's shader
  for the other`; restored `84f987cd` **PASS**. The earlier mutation evidence on `f1609da6` was for
  the pre-S1 backend and did not cover the shipped source; this run does.

- **Final identities after the repair.** Toolkit `src/video/nv2a_vk_backend.c`
  `sha256 84f987cd0ba2b39d913e775cf4cf19c7651354cfdce25ae550d794065c54ce6e` (the fix is the earlier
  `5de9f892`; `84f987cd` adds only the ordering comment). Game executable
  `sha256 23fbe93d984fa85d87e5658763c9aa369e3d37b899ae32dd2794472e4eff6c4c`. The malformed regression
  `tests/test_vsh_aot_malformed.c` `sha256 d970cb1153f2d41d...`. Gates on the repaired tree:
  `just build` clean, CTest **74/74** on lavapipe, `just check` all checkers passed, and the e2e run
  `20261014-165814-809-keyscript-e2e-vk-v5r` binds to that executable with **0 misses and 1.22M
  corpus-shader draws** (324.8M triangles), 72 steps, no ICALL/ABI/fatal.

### §31.21 V6: the synthetic vertex-program conformance gate (2026-10-14)

**V6 asks a different question from V5.** V5 (§§31.19–31.20) asks whether the generated retail corpus
and the back end's lookup are self-consistent. V6 asks whether the **offline translation pipeline and
the corpus seam reproduce the interpreter** for the synthetic V1–V7 taxonomy — on the bytes and on the
pixels. It is a separate test (`tests/test_vsh_conformance.c`, `jsrf_vsh_conformance` and its cases)
authored by a worker distinct from every V5 author.

- **Fixtures are the parity test's own programs, not a re-invention.** The V1–V7 taxonomy already
  existed in `tests/test_gpu_backend_parity.c`, between its `VSH-ENCODER-BEGIN`/`END` markers: V1 `MVP`
  (four `dp4` against c[96..99] plus the D3D epilogue), V2 `A0` (`arl a0.x, v1.x` then
  `mov oD0, c[a0.x+20]`), V3 `PAIR` (a MAC and an ILU in one instruction, reading the old R3), V4 `SFU`
  (`lit`/`exp`/`log`/`dst`), V5 `FOG` (`oFog.x = v1.x`), V6 `BEHIND` (the MVP program with a vertex
  behind the eye), V7 (V1's words under `RECOMP_GPU_VSH=0`). CMake extracts that region verbatim at
  configure time, verifies the copy round-trips, and re-extracts whenever the parity test changes, so
  the encoder cannot drift. No fixture code is shared with V5.
- **The SPIR-V fixtures are compiled at build time from the fixtures' own words**, with the same host
  tool, `dxc` flags and `spirv-val` the retail corpus uses; the test recomputes a hash of the words and
  matches the generator's record, so no stored blob is involved. Fixture identities (`len`, mask, hash,
  digest) are computed at run time by `nv2a_vsh_program_info` and `nv2a_vsh_aot_digest`, never guessed.
  The fixtures are verified absent from the 126-entry retail corpus by hash *and* digest.
- **What it checks.** (1) The host tool's plain output is byte-identical to calling `nv2a_vsh_hlsl`
  directly for all ten rows, and the `--vk` form differs only by the expected decorations: exactly two
  bindings (9, 10), an input location per mask bit **at its own bit number**, and each output varying
  asserted **by name** at `ps_rc`'s location (`d0`→0 … `t3`→6) plus `SV_Position` — values, not just
  counts, because the independent verifier showed a moved output location otherwise passed. (2) A
  table hit matches the CPU interpreter's pixels on lavapipe: all 4096 pixels' four channels —
  `chan_diff`'s set is the named indices `{CH_A, CH_R, CH_G, CH_B}`, and it alone decides `match` and
  `region_bad` — at tolerance 4 (the parity tests' `TOL_VSH`). Coverage is judged **RGB-only**
  (`chan_diff_rgb`'s set is `{CH_R, CH_G, CH_B}`), because the clear colour is opaque and an
  alpha-only difference must not count as "this pixel was drawn"; the coverage may differ by at most
  `max(4, perimeter/4)`, and at least 500 pixels must be non-clear. See the channel repair below for
  why this is stated as index sets rather than as "the first n channels". (3) A miss advances
  `misses`, not `hits`, and leaves the interpreter's pixels.
  (4) A corrupt `len` and a corrupt `digest` each advance `identity_fail` with no hit and CPU fallback.
  (5) `RECOMP_GPU_VSH=0` (V7) leaves every counter at zero: the corpus is never consulted.
- **The pixel rule is the established one**, not a new allowance: the boundary band is the
  interpreter's own perimeter from `vs_predict_visible`, and the tolerance is the parity suite's. The
  coverage bound was **tightened** from the perimeter to `max(4, perimeter/4)` after an independent
  probe showed the looser rule accepted a whole-pixel geometric error (V1 shifted 1 px: xor 78 against
  a 28 bound). Every case's measured xor is 0, so the slack is unused.
- **The gate found real coverage holes and closed them.** V2's a0-relative indexing was never
  pixel-compared (it was the miss), so `floor` → truncation would have passed; V2 is now in the table
  and drawn with fractional a0 inputs (2.9999 / 0.9999 / 0.5) where `floor(x+0.001)` and `trunc(x)`
  select different constants. The alpha channel was not compared at all, hiding V4's `dst` `y`/`w`
  results; all four channels are compared now, and `check_alpha_coverage` fails if **no** fixture's
  reference has a drawn pixel whose alpha is not 255 — V4's `dst`/`exp`/`log` model values reach the
  surface only through `oD0.w` (0.54/0.64/0.49, i.e. alpha 138/163/125) — so the alpha half cannot
  become vacuous without the gate saying so. (The earlier text here said the test asserts "V4's alpha
  is non-opaque at 1176 pixels". The check does not assert a count: it requires *at least one* such
  pixel across all fixtures, and prints each fixture's count. V4's printed count is 1176, so the
  number was right and the "asserts" was not.)
  No fixture used MAC op 3 (`add`), so an `add`
  regression would have slipped through; fixture X4 uses it and is pixel-compared. And a
  false-pass hole in the harness itself — the stderr log path could fall back to `.` and read a stale
  file — is fixed with a pid-unique path under `TEMP`/`TMP` and a hard failure if it cannot be opened.
- **Independent verification.** A worker distinct from the author ran **18 mutants** on a scratch
  toolkit copy (production byte-identical before and after) and found **7 false passes**, of which one
  was a real defect and one a real fixture gap — both fixed and re-checked. Its re-check then found
  **six MAC ops with no fixture coverage at all** (`dp3`, `dph`, `min`, `max`, `slt`, `sge`): mutating
  any of them passed the whole gate. All six are now closed by fixtures X5–X10, each a full fixture with
  its own identity, table entry and CPU-vs-GPU comparison, and each mutation-confirmed caught by its own
  fixture (`FAIL: [X5] only 79.10% of pixels agree … worst 44`, and similarly for X6–X10). The fixture
  count is 13 (V1–V5, MISS, X4–X10). The encoder shares one input index across all three sources, so
  `dp3`/`dph` take their second operand from a constant; that is a limit of the test encoder, not the
  ISA, and it is documented in the header. Two of the author's own
  claims were confirmed by construction: check 1 is an equality between the host tool and the same
  linked function, so a change *inside* `nv2a_vsh_hlsl.c` moves both sides (the `m1c` mutant is caught
  by check 2's pixels instead, and the gate as a whole still fails it); and the structural assertion on
  the shader body is the **sole** catcher for a dropped `saturate`. Recorded as accepted limitations:
  check 4 needs **both** digest terms removed to fire (either alone leaves the sibling term rejecting),
  the fog-parameter mutant is value-equivalent, and depth test/write and `SET_FOG_MODE` are unreachable
  in these batches by design. MAC ops 0–13 all have discriminating fixtures; **MAC 14/15 (which the
  translator maps to `A`) and ILU 1 (`mov`) / 4 (`rsqrt`) remain uncovered** — the independent verifier
  mutated those four paths and the gate accepted them, but no retail program is known to use them and no
  defect is suspected behind them, so no fixture was added and the gap is recorded rather than papered
  over.
- **The four-channel pixel predicate was wrong twice, and is now repaired and regression-tested
  (2026-10-15).** An independent review rejected the gate as committed (`19cde77`). Two forms were
  wrong, in different places, and neither was visible to any other check in the file — on a correct
  fixture every compared pixel agrees, so a predicate that silently drops a channel still reports a
  perfect image:
  1. **The committed original** extracted channel `i` with `(a >> (16 - 8 * i))`, a right shift by −8
     at `i = 3`: undefined behaviour, so the fourth channel was whatever the build made of it, and
     the outcome **varies per build**. One clang-cl `/O2` build of the verbatim helper compared
     **alpha only** (an image wrong by 255 in red, green or blue was accepted); a second clang-cl
     build instead rejected an *exact copy* of the reference, because the fourth channel's garbage
     broke the positive control; MSVC 19.51 masked the shift count to 24 and happened to be correct
     there. No single printed value describes this form, which is why its evidence is a
     `clang-cl -fsanitize=shift` trap build (it aborts at the invalid shift) plus the scratch runs,
     not a number. `chan_alpha` printed the right value throughout, but it is only a
     diagnostic: `chan_diff` is what decides a match and the drawn-region mismatch count.
  2. **The first repair** replaced the arithmetic with a shift table `{24,16,8,0}` indexed by a
     channel *count*, so the "RGB-only" set became the first three entries — alpha, red, green. Blue
     was never compared and alpha leaked into the coverage test, where the clear colour is opaque and
     an alpha-only difference therefore counted as "this pixel was drawn". This form is well defined
     and wrong on **every** compiler.
  The repair replaces both with named channel-index sets: `chan_diff` is `{CH_A, CH_R, CH_G, CH_B}`
  and alone decides `match` and `region_bad`; `chan_diff_rgb` is `{CH_R, CH_G, CH_B}` and alone
  decides coverage. `chan8(v, c)` shifts by `24 - 8 * c`, legal for every index `0..3`. Sets are named
  rather than counted, so no caller can get a set by accident. No tolerance or threshold changed.
- **The regression is a separate worker's, and it drives the real predicate.**
  `jsrf_vsh_conformance_channelgate` (a device-free process that exits before the Vulkan back end is
  installed, so it can never report a skip) calls `compare`/`check_cpu` on a real captured CPU
  reference — V2, chosen because its drawn region spans both ends of the range across the four
  channels (measured and printed: alpha 255 in all 4096 pixels, red and green each reach 255 on 4
  pixels, blue is 0 on 1176) — and asserts
  measured quantities, never merely "the call failed": an exact copy is accepted with worst difference
  0; an alpha-only difference of 255 on all 4096 pixels is rejected (worst alpha 255, match 0.00%);
  an alpha difference of 2 is still accepted; red-only, green-only and blue-only images are each
  rejected with `worst_rgb` 255 and the other two RGB channels exactly 0; `covered()` classifies the
  literal boundary words correctly (blue +25 drawn, +24 not, alpha 0 and 0xFB not drawn whatever their
  value); and an alpha-only change leaves `cov_xor` at 0. The positive control gates the negative legs:
  if the exact copy is not accepted, the legs are not evaluated and the check fails, so a broken
  prerequisite cannot be reported as a pass.
- **The new case fails on both bad states and passes on the repair** — but on different compilers,
  because the two states are wrong in different ways. Scratch A/B/C runs (`xbr-mac/pv05r2/ab`,
  `xbr-mac/pv05r-verify`, with only the extraction swapped and
  the interface kept, so the regression compiles against each): the committed-original form exits 0
  under MSVC (the shift count is masked, so it is accidentally correct there) and **fails under
  clang-cl** at the exact-copy control
  (`1176 drawn-region pixels disagree`); the count-indexed form **fails under both** compilers with 7
  gate failures, including `a blue-only difference was not discriminated: worst_rgb 0 (required > 4 and
  equal to the engineered max 255)` and `covered(0x00101010) = 1 but must be 0`; the repaired source
  passes both. So the state that fails under the compiler this gate actually ships with (MSVC) is the
  partial, not the original.
- **Gates.** `just build` clean; the nine V6 CTest cases (the gate, V7, controls 1–6, and the new
  channel gate) all pass on
  lavapipe, never skipped; full CTest **83/83**; `just check` all checkers passed. The controls assert
  the check's own verdict, so a control that misses its defect exits 2 and the case fails rather than
  passing on any nonzero exit.
- **No production change.** V6 is test-only: the toolkit is untouched at `5668278`, so this step adds
  no toolkit commit.

### §31.22 R1: a stale vertex-program corpus is refused at build time (2026-10-15)

**Review status: implementation `9078b47` was rejected by independent Codex review.** The narrative
below records that implementation and its tests; its advisory tool identity and warning-only override
are not the approved R1 acceptance contract. The deciding counterexamples and required repair are in
§31.23. V5/V6 remain accepted.

**What R1 is.** V5 (§§31.19–31.20) proves the runtime identity check over the guest *words* a batch
uploads. It says nothing about which translator or compiler produced those words' SPIR-V. So if
`nv2a_vsh_hlsl.c`, its header, its CLI or dxc's version/flags change after `just shaders` ran, the
game links SPIR-V no current source can produce: wrong pixels, with no signal at run time. Nothing
re-checked the real table — not `just build`, not `just check`, not CTest — and the CMake comment
for the synthetic `jsrf_vsh_aot_table` case implied those `--skip-spirv` fixtures guarded it. R1
records the inputs that produced the corpus and refuses a mismatch before the compiler sees it.

- **The provenance schema** is one definition, `scripts/vsh_aot_provenance.py`: schema version, the
  SHA-256 of the three translator sources (`nv2a_vsh_hlsl.c`, `nv2a_vsh_hlsl.h`,
  `nv2a_vsh_hlsl_tool.c`), the host tool's SHA-256, the tool's and dxc's argument vectors with file
  operands as placeholders, the normalized `dxc --version` string, and the corpus's own
  `programs`/`distinct` counts. The generator emits it into BOTH generated files between
  `JSRF-VSH-AOT-PROVENANCE-BEGIN`/`-END` markers. It carries no absolute path, username or timestamp,
  so the DECIDING fields are identical wherever the same inputs are regenerated — but the recorded
  `tool.sha256` is not, because the executable is not reproducible (see below). That is why
  `gen-shaders.py --check` compares with the advisory lines blanked on both sides and everything
  else exactly, and why an equivalent tool binary does not make a current corpus look stale.
- **One sequence, called twice.** `check_corpus(corpus_dir, want)` in that module is the only
  implementation of the guard: missing file, no block, more than one block, a malformed block, every
  deciding field present and equal, the two files recording the SAME block, and the block's
  `distinct` agreeing with the table's own declared `nv2a_vsh_program_count`. `scripts/build-jsrf.py`
  calls it before the CMake configure step and the fixtures call the same function, so a fixture
  cannot pass against a private copy while the shipped guard drifts.
- **The count check reads the file that defines it.** `nv2a_vsh_program_count = 126` is a definition
  in the `.c`; the `.h` only declares `extern`. A check that scanned the header would be vacuous.
- **`tool.sha256` is recorded but ADVISORY, never decisive.** Measured on this host, the executable is
  not reproducible: three clean rebuilds of byte-identical sources gave three different
  `nv2a_vsh_hlsl.exe` hashes. The cause is the link-time `TimeDateStamp` plus, for the game-tree
  layout, an embedded absolute PDB path — so the hash depends on the clock and the checkout
  location. A decisive tool hash would therefore refuse a corpus `just shaders` had just produced.
  Freshness is enforced where it can actually corrupt the corpus — at GENERATION time, and by
  CONTENT first: the generator reads the existing corpus's recorded `source.*` hashes (a durable
  record of the last generation) and forces a rebuild whenever they differ from the current sources,
  whatever the timestamps say; mtime is the second line of defence for a first generation. This
  closed a falsified claim: with mtime alone, editing a source's content and giving it an mtime
  BELOW the tool's let the generator reuse the old tool and record the new hash — a corpus whose
  provenance named bytes the tool never read, which then passed the guard. An explicit
  `JSRF_NV2A_VSH_HLSL` override cannot be proven fresh at all, so it prints an advisory naming the
  recorded (advisory) `tool.sha256` as the only trace rather than implying verification, and
  `assert_tool_current()` refuses outright when a tool the script owns is older than a source. The
  scratch gate exercised this: after the whitespace edit the generator reported the tool STALE and
  rebuilt it before generating.
- **The real table is now checked.** `just check` runs `scripts/check-vsh-aot-table.py` with no flags
  (~20 s): it re-derives every entry's hash and digest from the stored words, rebuilds each shader
  from those words and compares bytes, validates the stored SPIR-V, and re-scans the XBE for
  coverage — `entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126
  problems=0`. The device-free synthetic `--skip-spirv` CTest stays, but the CMake comment now states
  what it actually guards: the checker's own failure modes, not the currency of the retail table.
- **The gate: a whitespace content edit to the translator makes the build refuse, and regeneration
  clears it.** In a scratch tree (`xbr-mac/r1-scratch`, `game/` junctioned so the original XBE is read
  in place and never copied or altered): baseline `python scripts/build-jsrf.py` exit 0; appending one
  comment line to `nv2a_vsh_hlsl.c` (`c749f0c7…` → `674c7827…`, a CONTENT change, not an mtime
  touch) makes the next build exit 1 before configure, naming both corpus files, the field
  `source.nv2a_vsh_hlsl.c`, both hashes and `just shaders`; offline regeneration then rebuilds the
  stale tool and writes a corpus the guard accepts (exit 0). The refusal is a provenance rejection,
  not a compiler or bootstrap failure. Original assets, saves and the owner's checkout were untouched
  (production translator still `c749f0c7…`, production toolkit clean at `5668278`).
- **The regression is a different worker's and is load-bearing.** `tests/test_vsh_aot_provenance.py`
  (68 tests) calls the production guard, not a copy. It asserts the DECISIVE REASON for every refusal,
  and it is mutation-controlled: 15 weakening edits to scratch copies of the module are each detected
  — including one per `check_corpus` step (missing file, blockless, multi-block, malformed, per-field,
  two-file agreement, count) — while three cosmetic edits stay green. `test_check_corpus_is_the_entry_
  point_the_build_calls` reads `build-jsrf.py` and asserts it calls `prov.check_corpus(`, so the two
  cannot drift apart.
- **Gates.** `just build` clean with `corpus : schema 1, 128 programs, 126 distinct`; 84 CTest cases
  (83 before, plus `jsrf_vsh_aot_provenance`); full CTest **84/84** on lavapipe, no skips; `just check`
  passes including the new real-table step. Corpus program content is unchanged by R1 — only the
  provenance comment was added (the table body is byte-identical). Test-only in effect: no runtime
  path changed, the toolkit is untouched at `5668278`, so no toolkit commit.

**Next:** the independent rejection of this implementation and its repair are in §§31.23–31.24. The
repair closes the three gaps; R1 then awaits Codex acceptance, and N2 (the controlled benchmark)
follows, then N3/V7/V8.



### §31.23 R1 review rejection: tool identity, freshness and toolkit binding (2026-10-15)

Independent checks of the shipped `9078b47` found three gaps against the approved R1 gate:

- **Tool identity is not decisive.** On the real current corpus, production `check_corpus` returns
  `(True, [], [])` for matching provenance. Changing only expected `tool.sha256` to 64 zeroes still
  returns `True` with advisories. Removing that field from both temporary generated-file blocks
  returns `(True, [], [])`. Missing or changed tool identity therefore passes; the approved gate
  requires refusal. Executable non-reproducibility across relinks does not prevent comparison with
  the actual executable that generated the corpus. Stable tool selection must make that comparison
  useful through bootstrap, generation and build.
- **A stale explicit override is accepted.** A private copy of the real host executable, with mtime
  set to epoch 1, is older than all three actual sources. With `JSRF_NV2A_VSH_HLSL` selecting it,
  production `locate_host_tool` returns it with `cannot be PROVEN fresh ... advisory`. The approved
  freshness gate must rebuild a verified owned tool or refuse, while preserving an arbitrary override.
- **The validated toolkit is not bound to the linked one.** The unmodified AST-extracted build-guard
  functions accept temporary corpus provenance for an alternate three-source toolkit with a whitespace
  content change, while preflight and the actual CMake cache still select the sibling toolkit. This
  is a reduced guard test, not a claim that wrong pixels were observed. Align the authoritative paths
  or refuse the mismatch before configure.

These counterexamples used temporary private generated files/tool copies and did not change the
production tree, assets or saves. The independently run provenance/table/build suites pass 130 tests;
real-table validation reports `entries=126 recompiled_and_matched=126 ascending=yes duplicates=0
xbe_distinct=126 problems=0`. Those positive results do not test away the three missing refusals.
Repair forward with a different worker's regression and final-source baseline/mutation/restored
proof, including the actual supported build refusal before configure. R1 remains pending; R2 has not
started. The previously accepted V5/V6 milestones are unchanged.

### §31.24 R1R: decisive tool identity, a refused stale override and one bound toolkit (2026-10-15)

**Status: R1 repaired, awaiting independent Codex acceptance.** V5/V6 remain accepted; R2 has not
started. This section records the repair of the three counterexamples in §31.23, the defects an
independent review of the repair itself found, and the evidence that closes them.

**The three gaps, and what closes each.**

- **Tool identity is now decisive and mandatory.** `scripts/vsh_aot_provenance.py` is at `schema = 2`:
  `tool.sha256` is a deciding field, `ADVISORY_FIELDS` and `strip_advisory` are gone, `expected()`
  requires the existing tool (no `tool_path=None` arm) and `check_corpus` refuses a caller that cannot
  name the tool it expects. A corpus that records a changed hash, omits the field from both files, or
  carries an empty value now refuses naming `tool.sha256`, on both files.
- **The recorded executable is the one the repository owns.** The generating tool is
  `build/vsh-tool/.../nv2a_vsh_hlsl.exe` (either CMake layout); the game build tree's copy is
  deliberately not a candidate, because `just build` relinks it and its non-reproducible PDB
  signature moves its hash without an input moving — and the guard runs before that tree is
  configured at all. Only `just shaders` rebuilds the owned artifact, so the hash comparison is
  between the actual generating binary and the actual binary in use. Deleting or relinking the
  artifact requires `just shaders`; that is the conservative, approved behaviour, not a false
  positive.
- **A stale `JSRF_NV2A_VSH_HLSL` override is refused, not warned about.** `locate_host_tool` refuses
  it when the corpus records different source content and/or it is older than a translator source,
  and the build guard refuses the same states; no code path writes to, rebuilds, renames or copies
  over a path named by the environment. The remedy text distinguishes the two triggers honestly: an
  mtime trigger is cleared by rebuilding the override, while a content trigger can only be cleared by
  UNSETTING the variable (that trigger compares the sources the existing corpus recorded with the
  sources on disk and never looks at the override), after which `just shaders` rebuilds the owned tool
  and records the new sources.
- **One toolkit binds the guard, the generator and the CMake link.** `resolve_toolkit` prefers the
  build tree's cached `XBOXRECOMP_DIR` (what `add_subdirectory` links), then `$XBOXRECOMP_DIR`, then
  the sibling checkout, and REFUSES a cache/environment disagreement naming both paths; the winner
  must be a real checkout (CMakeLists plus the three translator sources). The owned tool tree is bound
  too: its own `CMakeCache.txt` `CMAKE_HOME_DIRECTORY` must name the same toolkit, or both the
  generator and the guard refuse — without that check the generator could force-rebuild
  `build/vsh-tool` from *another* checkout's sources while recording this one's hashes (the same
  defect class one level down, found by the repair's own review). `check-vsh-aot-table.py` resolves
  its default tool through the same function, so `just check` recompiles with the recorded identity.

**Schema 2 and what the corpus actually changed.** The corpus is regenerated: the block now records
`schema = 2` and `tool.sha256 = 375c7fb901109bfd51eecacda919e115db942aeb65e4fb9efbda6c7bb9aefb0e`
(the owned artifact), with the three source hashes `c749f0c7…`/`b774047d…`/`9caf73e9…` unchanged.
The table body is byte-identical to the R1 corpus (provenance block stripped: `ae96c76b…` for the
`.c`, `9c8f306a…` for the `.h`), so R1R changes no program, no SPIR-V byte and no runtime path.
`gen-shaders.py --check` is byte-exact and idempotent, and `--check` is non-mutating: it resolves the
tool through `find_host_tool` and never builds, so a check cannot move the decisive identity.

**The repair's own review.** An independent code review of the repair found a P1 and a P2, both fixed
here: the owned-tool binding above, and `--check`'s ability to force-rebuild the tool (which would
have flipped a passing tree to refused and left the generator and the guard disagreeing). It also
flagged an override remedy that could not clear the trigger it was offered for, an over-narrow
`CMakeCache.txt` entry type (`:PATH` only — now any type, so `:STRING`/`:UNINITIALIZED` are read),
and two comments that justified mtime-only staleness with a relinked executable (a relink changes the
hash and is refused anyway; the tolerated case is newer source mtimes with identical content). The
repair also had to instantiate the provenance module once per process: loading it twice gave the guard
a different `ProvenanceError` class than the generator raised, so two refusals could never be caught
and surfaced as tracebacks. A security review of the change traced every override path use and every
subprocess invocation and found no vulnerability (`shell=False` argument vectors throughout, no write
to any non-owned path, refusals not skippable); its one informational note is the last-resort
`force_tool_rebuild` lever that touches the resolved toolkit's source mtimes when no object files are
found — pre-existing, mtime-only, never the override, never content.

**The regression, and eight mutants it kills.** `tests/test_vsh_aot_provenance.py` (a different
worker from the production author; 126 tests; 166 with `test_vsh_aot_table` and `test_build_jsrf`;
~56 s for the case CTest runs) exercises the SHIPPED scripts, including a scratch-tree run of the
real `scripts/build-jsrf.py` whose stub `CMakeLists.txt` records that configure ran: six negative
cases (changed tool hash, moved source, omitted tool hash, missing tool, stale override, cache versus
environment toolkit) each exit non-zero naming the intended reason and `just shaders` with NO
configure marker and no `logs/configure-current.log`, while the matching-corpus positive control does
reach configure. Its weakening mutants are applied to scratch copies of the scripts and run through
`JSRF_R1_SCRIPTS`: M1 `compare` skips `tool.sha256`, M2 `check_corpus` drops it from the required
expectations, M3 `expected()` accepts no tool, M4 the guard accepts a missing tool, M5 the guard warns
on a stale override, M6 the guard call moves after the configure call, M7 the cache/environment
conflict resolves silently, M8 the owned-tool binding never refuses — **all eight detected**
(2–7 failing cases each, naming the case that caught them) with a comment-only control green.

**The real scratch gate** (`C:/Users/logic/xbr-mac/r1r-scratch`, `game/` junctioned to the real
assets so the XBE is read in place and never copied or altered; scratch toolkit with pristine sources
`c749f0c7…`/`b774047d…`/`9caf73e9…`):

- baseline: `python scripts/build-jsrf.py` exits 0, `corpus : schema 2, 128 programs, 126 distinct,
  tool …build/vsh-tool/…nv2a_vsh_hlsl.exe sha256 cd24fa2e…`.
- mutation: one comment line appended to `nv2a_vsh_hlsl.c` — `c749f0c7…` → `fd72b1bb…`, a CONTENT
  change, not an mtime touch.
- refusal: the next `build-jsrf.py` exits 1 BEFORE configure (the configure log's mtime is unchanged
  and no marker appears), naming `nv2a_vsh_hlsl.c`, both hashes and `just shaders`.
- regeneration: `gen-shaders.py` reports the corpus's recorded content differs, discards 2 object
  files, force-recompiles the owned tool (`99984604…`), and records `source.nv2a_vsh_hlsl.c =
  fd72b1bb…` with that tool hash; the next build is accepted (`exit 0`).
- tool-only replacement: copying the real tree's artifact (`375c7fb9…`) over the scratch one makes
  the build refuse naming `tool.sha256` and both hashes, before configure; `gen-shaders.py` re-records
  it and the build is accepted again.
- toolkit mismatch: `XBOXRECOMP_DIR=<the real toolkit>` with the scratch cache still naming the
  scratch toolkit refuses before configure, naming both paths.
- `--check` non-mutation: with a source mtime touched and its content unchanged, `--check` prints both
  `identical, byte for byte` lines and leaves the artifact's sha256 AND mtime unchanged; with the
  content changed it refuses naming `just shaders`, again leaving the artifact untouched.

Production was untouched by the gate: the real toolkit sources are still
`c749f0c7…`/`b774047d…`/`9caf73e9…`, the toolkit is clean at `5668278`, and the edited translator
exists only under `r1r-scratch/`.

**Gates on the final tree.** `just build` clean (`corpus : schema 2, 128 programs, 126 distinct, tool
… 375c7fb9…`, 45 targets, 84 tests); focused suites **166/166 OK**, no skips; the real table
`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126 problems=0` with
no `--skip-spirv`; `just check` → `check: all checkers passed`; full software-Vulkan CTest
(`ctest-limited.py --output-on-failure`) **84/84 passed**, 296 s, no skips. No gameplay or benchmark
run: this step is build tooling only, and no runtime path, corpus byte or SPIR-V byte changed.

**Limits, stated rather than implied.** (1) The recorded identity is a BUILD identity: replacing,
relinking or deleting `build/vsh-tool`, moving a translator source, or rebinding the toolkit all
require `just shaders` before `just build` accepts — conservative by design, since a different
executable is a different input. (2) An override cannot record a source change the corpus does not
already record; unset it, regenerate, then set it again. (3) Relative `$XBOXRECOMP_DIR` and
`JSRF_NV2A_VSH_HLSL` resolve against the process's current directory while a relative cached value
resolves against the repository root; automation should pass absolute paths. (4) `build/CMakeCache.txt`
and the owned tool tree are trusted local inputs: whoever can write them can already make CMake
configure arbitrary code, and the change does not widen that. (5) The check is currency, not
reproducibility: the recorded hash names the binary that ran, and the toolchain's PDB signature means
two equivalent builds are two identities. (6) The corpus and its provenance stay retail-derived,
untracked and ignored, as V5 established.

**Independent Codex acceptance of R1 repair (`c6eaf6b`).** The original counterexamples were
re-run against production functions: matching corpus accepted; changed tool hash refused for both
files; a tool hash missing from both files refused; an actual executable copied privately and made
older than all three sources refused without changing that executable; cache/environment toolkit
conflict refused. The unmodified AST-extracted build guard accepts the actual current baseline.
The independently run focused suites pass **166 tests**, including the eight weakening mutants
(each detected; M1 through M8: 7/2/2/2/1/6/1/7 failing cases) and a green comment-only control.
The real checker independently matches **126/126** entries. Final scratch script hashes match the
committed files, and inspected logs show baseline build exit 0, content mutation refused exit 1
before configure, regeneration exit 0, restored build exit 0, and tool-only replacement refused
exit 1. Full-suite receipt: 84/84, no skips; final `just check` receipt passed. The actual role
transcripts confirm separate OpenCode Go DeepSeek implementation/test workers, Opus code review
and Muse security review. All 13 outgoing blob versions were independently audited: no forbidden
paths, no secret-pattern hits, largest 234548 bytes. Both repositories were clean and synchronized;
toolkit remains `5668278`. No runtime, validation, fidelity or performance claim is added.
R1 is accepted for continuation to R2; V5/V6 acceptance is unchanged.

### §31.25 R2: a build without the Vulkan toolchain compiles an honest empty corpus (2026-10-15)

**Status: R2 independently accepted by Codex at game `b28b7cb` / toolkit `b20ddba`.** V5/V6 and the R1 repair remain accepted;
toolkit `b20ddba` carries the one supporting change.

**The defect.** Vulkan is optional in the toolkit, but the game's configure demanded
`src/recomp/gen/shaders/*.c` unconditionally: measured before R2, a configure with Vulkan effectively
unavailable and no corpus failed at `src/recomp/gen/shaders is not tracked: … run `just shaders`` —
a checkout without the SDK could not be configured or built at all, even though the back end is a stub
in that configuration. The game also re-derived the toolkit's capability by hand
(`if(XBR_DXC AND XBR_SPIRV_VAL AND Vulkan_LIBRARY)`) because `XBR_VULKAN_FOUND` was a plain variable
in the toolkit subdirectory's scope, and the toolkit's own check accepted a cached `dxc`/`spirv-val`
path that no longer existed.

**One bound decision.** `xboxrecomp/cmake/XbrVulkan.cmake` (toolkit `b20ddba`) runs
`find_package(Vulkan)` unconditionally, requires the loader library, its headers, `dxc` and
`spirv-val` to EXIST, names the first failing check, and publishes the verdict as the cache BOOL
`XBR_VULKAN_FOUND`, FORCE-refreshed on every configure, with `XBR_VULKAN_UNAVAILABLE_REASON` (cache
STRING) naming the failed check. A cache entry is the channel because a subdirectory's plain variable
never reaches its parent; FORCE is what makes a stale verdict impossible. The game's `CMakeLists.txt`
reads that one value instead of re-deriving it.

**Two branches, one corpus.** After `add_subdirectory`, the game's configure branches once:

- **capability ON** — the retail corpus is required (absent = FATAL naming `just shaders`) and its
  provenance is validated INSIDE that configure by the shipped guard
  (`scripts/build-jsrf.py --check-corpus-only --expect-capability-on --toolkit ${XBOXRECOMP_DIR}
  --probe-parent ${CMAKE_BINARY_DIR}`), whose output is quoted in a FATAL when it fails. A real corpus
  therefore cannot reach the compiler unvalidated — not through `just build`, not through
  `just shaders`, and not through a direct `cmake -S . -B build`; `--expect-capability-on` also means a
  disagreement between the guard's own probe and the configure cannot skip the check. Every provenance
  input (the two corpus files, the owned tool in each layout, the three translator sources, an
  environment override) is a `CMAKE_CONFIGURE_DEPENDS` entry, so a later build without a reconfigure
  reconfigures and re-validates instead of compiling a corpus that moved.
- **capability OFF** — a minimal empty table is generated at configure time into
  `<build>/empty-corpus/nv2a_vsh_programs.{c,h}` (`file(CONFIGURE …)`: an unchanged configure does not
  even bump its timestamps) and compiled instead. `nv2a_vsh_program_count = 0` with one sentinel entry
  (a zero-length array is not valid C), no provenance block, the same declarations as the real header,
  and `<build>/empty-corpus` — not the source corpus dir — on the include path. The retail corpus is
  neither globbed nor included nor compiled EVEN WHEN PRESENT, and the corpus/Vulkan-dependent test
  targets are not registered. The status line says so, and that program batches fall back to the
  executor's CPU interpreter.

**The guard's capability is asked fresh, never read from a cache.** `scripts/build-jsrf.py`
configures the shipped probe project `scripts/cmake/vulkan-capability` (a `project(… NONE)` that
includes the toolkit's `XbrVulkan.cmake`) into a fresh `mkdtemp` leaf under `build/vulkan-capability`,
forwarding `XBOXRECOMP_DIR` and the game cache's `XBR_DXC`, `XBR_SPIRV_VAL`, `XBR_VK_SW_ICD`,
`CMAKE_DISABLE_FIND_PACKAGE_Vulkan`, `Vulkan_LIBRARY`, `Vulkan_INCLUDE_DIR`, `Vulkan_DIR` and
`CMAKE_PREFIX_PATH`, and reads the verdict from the probe's own cache (log:
`logs/vulkan-capability-probe.log`). A probe that cannot answer is fail-safe — `capability:
undetermined …` and the retail corpus stays required. A probe PROJECT rather than `cmake -P` is
required because CMake's `FindVulkan` calls `add_library`, which is not scriptable (measured).
`--check-corpus-only [--expect-capability-on]` answers and exits: no preflight, no configure of the
game, no build, no write to the corpus.

**Evidence on the final source** (scratch `xbr-mac/r2-scratch`, `game/` junctioned, toolkit copy
pristine, the ignored lifted tree copied in — disclosed as a prerequisite so an unrelated
generated-code failure cannot pass as the corpus decision):

| case | observed |
| --- | --- |
| OFF, no corpus | configure exit 0; `Vulkan/unavailable: the Vulkan loader and headers were not found …`; `empty-corpus/nv2a_vsh_programs.c` ends `const size_t nv2a_vsh_program_count = 0;`; the exe project compiles exactly that file, 0 `gen\shaders` references; cache `XBR_VULKAN_FOUND:BOOL=FALSE` |
| OFF, real corpus present | the same, and the corpus copy's sha256 and mtime are unchanged; nothing written under `src/recomp/gen/shaders/` |
| OFF, full link | `cmake --build build-off --config Release --target jsrf_recomp` → exit 0, `jsrf_recomp.exe` `d2a8b545…` (17.5 MB): the empty table satisfies the real `nv2a_vsh_aot.h` interface and links into the game |
| ON, corpus + owned tool | configure exit 0; `Vulkan/available: … validated in this configure … compiled (1 file(s) from src/recomp/gen/shaders).`; 0 `empty-corpus` references |
| ON, corpus whose `tool.sha256` moved | configure FAILS, quoting the guard's `tool.sha256 differs` problems and `just shaders` — the corpus cannot be compiled unvalidated |
| ON, no corpus | configure exits 1 at the corpus FATAL naming `just shaders`; `--check-corpus-only` exits 1 with `capability: Vulkan available …` and creates no build tree |
| OFF→ON→OFF, one build tree | cache FALSE→TRUE→FALSE and the compiled corpus source `empty-corpus`→`gen\shaders`→`empty-corpus`, no stale reference left in the current target |

A different worker's regression (`tests/test_optional_vulkan.py`, registered as
`jsrf_optional_vulkan`) drives the real configure, guard and probe in temp trees and asserts those
outcomes plus the partial-toolchain case (loader found, `dxc` at a nonexistent path → OFF with the
reason naming dxc). Its mutants are detected: **M-A** restores the unconditional missing-corpus FATAL
(three cases red, the no-SDK gate among them), **M-B** turns the capability-ON refusal into warnings
(the configure succeeds → the SDK-present/no-corpus case red), **M-C** declares one program in the
empty table, and **M-C2** makes the OFF branch compile the stale corpus as well (both red), with a
comment-only control green. R1's ordering fixture was re-anchored on the game's own configure (R2 adds
the probe's second `cmake -S`), and all eight R1 mutants remain detected.

**Gates on the final tree.** `just build` clean (`corpus : schema 2, 128 programs, 126 distinct, tool
… 375c7fb9…`; 45 targets, 85 tests; `capability: Vulkan available (loader …; dxc …; spirv-val …)`; the
real cache now records `XBR_VULKAN_FOUND:BOOL=TRUE`); focused suites **176/176 OK**, no skips; the real
table 126/126 with no `--skip-spirv`; `just check` → `check: all checkers passed`; full
software-Vulkan CTest **85/85 passed**, 387.6 s, no skips (`jsrf_optional_vulkan` #56, 110.3 s;
`jsrf_vsh_aot_provenance` #55, 44.2 s). No gameplay run: this step is configure/build tooling, and no
runtime path, corpus byte or SPIR-V byte changed.

**Reviews.** Security (Muse 1.3 high): no vulnerability — argument-vector execution throughout, no
interpolation of environment or cache text into command lines, writes confined to the repository's
own build/log output, every undetermined input fail-closed; its one real finding (two concurrent guard
runs deleting each other's probe directory — availability only) is fixed by giving each run a unique
`mkdtemp` leaf and never deleting the shared parent. Code (Opus 5.5): no P0/P1, one real P2 and three
P3s, all fixed — a moved corpus now forces a reconfigure instead of being compiled, the empty table is
written only when its content changes, the probe is asked the same loader-hint questions the configure
answers, `--toolkit`/`--probe-parent` bind the in-configure check to the configure's own toolkit and
build directory, and an uncreatable probe tree is reported as `undetermined` rather than a traceback.

**Limits.** (1) Capability OFF is an honest absence of compiled programs: no fidelity, performance or
validation claim, and it does not rehearse V7's N3 requirement (loader insertion witness and counters)
— program batches run on the CPU interpreter. (2) Capability OFF unregisters the corpus/Vulkan
dependent CTest cases (`jsrf_vsh_aot`, its two controls, `jsrf_vsh_aot_malformed`,
`jsrf_gpu_backend_parity_vk`, and the V6 conformance gate): disclosed rather than reported passing.
(3) `just shaders` still needs `dxc`/`spirv-val` and the SDK hint; it is the offline step that
compiles the retail corpus, and the empty table is never a claim that a retail table passed R1/V5/V6.
(4) The guard's own sources are not configure dependencies, so hardening the guard does not by itself
force a reconfigure, and `CMAKE_CONFIGURE_DEPENDS` entries are paths rather than globs. (5) `build/`
and `logs/` stay trusted local trees, and the pre-existing trust that whoever can write the toolkit
executes code at configure time now includes one extra probe configure. (6) The tests need the ignored
lifted tree; they disclose that prerequisite, and never copy original assets or saves.

**Independent Codex acceptance (2026-10-15).** Both repositories were fetched and found clean,
synchronized at game `b28b7cb` / toolkit `b20ddba`. Codex inspected the production diff and the
actual worker transcripts, then independently ran the final-source suites: 10 optional-Vulkan
tests in 98.7 s and 166 provenance/table/build regressions in 40.7 s, all passing with no skips.
The tests independently reproduced these deciding mutation results: M-A (unconditional corpus
refusal) exit 1 with 3 failing cases; M-B (capable refusal weakened) exit 1 with 1; M-C (empty
count = 1) exit 1 with 2; M-C2 (OFF also compiles retail corpus) exit 1 with 2. The comment-only
control and restored production sources passed. All eight R1 weakening mutants remain detected.

The archived full final-tree CTest receipt records 85/85 passed in 387.57 s; Codex inspected that
receipt and the final `just check` / real-table 126/126 receipts rather than rerunning that suite.
The original no-SDK full link predated review hardening. Codex therefore copied the final tracked
R2 files into the owned private scratch tree, verified the toolkit capability file was identical,
unset `VULKAN_SDK`, and reconfigured with Vulkan discovery disabled and nonexistent shader-tool
paths. Configure and the real `jsrf_recomp` target build both exited 0; the generated table reports
count 0 and the executable remains SHA-256
`d2a8b54573bf123e24aaf76f02bdb0810aa32ceba3bf94fc1e447cd43c91336a`.
This is a configure/link check, not a game run or a Vulkan validation-layer claim.

Actual transcript models: separate fix and test workers used
`opencode-go/deepseek-v4.1-flash:high`; the code reviewer used
`anthropic/claude-opus-5-5:medium`; security used `muse-code/muse-spark-1.3:high`.
No fallback was recorded. Codex audited all outgoing R2 blob versions independently in their
own repositories: game 10 blobs, toolkit 2, zero secret hits or forbidden paths, all below 100 MB;
both commit messages are unsigned and contain no co-author/signature trailers.

PUSHED_TO: origin / BRANCH: main / COMMIT: b20ddbae717336ac6c560bec98c64e15da0d33ed / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: synchronized fast-forward, toolkit pushed first.
PUSHED_TO: origin / BRANCH: master / COMMIT: b28b7cbcdcf53b1c05551d8614538847b767bd1c / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: synchronized fast-forward, after toolkit.

R2 is accepted for continuation to R3 only; the separate R3 accounting gate remains pending.

### §31.26 R3: corpus coverage counts culled program batches (2026-10-15)

**Status: independently accepted by Codex at game `5842b38` / toolkit `a052de5`; paused at the owner's request.** V5/V6 and the R1/R2 repairs remain
accepted; toolkit `a052de5` carries the change.

**The defect.** `vb_draw_program` looked a batch's program up only after the cull FRONT_AND_BACK early
return, so a program used exclusively in fully culled draws was never looked up at all: `misses`
measured the corpus against *visible* batches, and V5's "0 misses" said nothing about the identities of
culled ones. That is a coverage hole in a suite whose whole point is corpus coverage — the culling
decision belongs to the draw, the identity belongs to the batch.

**The change (toolkit `src/video/nv2a_vk_backend.c`).** The lookup now runs after the pointer/length
guard block and **before** the cull check and before every per-draw check, so exactly one lookup
happens per call and coverage is a property of the batch's identity, not of its visibility. The cull-all
branch still increments `cull_all` and `return 1` **regardless of what the lookup found**: nothing
visible is drawn by either path, so the executor must not transform the batch on the CPU (and returning
1 also avoids the double `cull_all` count a fallback would produce). A batch whose program the corpus
does not carry is a MISS and returns 0 for the existing CPU-interpreter fallback whatever its input
mask — the independent review of this change caught that the first revision could count such a batch as
both a miss and a decline, and that is fixed: `declines` stays reserved for a program the corpus
carries that this draw cannot set up. No counter meaning changed; `vb_program_lookup` is untouched,
including the documented load-bearing order of its identity guards; a culled batch still performs no
setup, pipeline, descriptor, reserve, stream or draw work, so a lookup match without a draw fabricates
no hit, triangle or upload.

**The deciding fixture** (`tests/test_vsh_aot_programs.c`, run as `jsrf_vsh_aot`, a different worker
from the change's author). It drives the real executor and Vulkan back end on lavapipe and measures
`nv2a_vk_backend_program_stats` deltas around single batches:

| case | observed |
| --- | --- |
| corpus-carried program, drawn visibly | `hits 0→1`, `tris 0→1`, `const_uploads 0→1`, `first_hit_hash EC6C788B` |
| **cull-all batch, program absent from the corpus** | **`misses 0→1` exactly**, `cull_all = 1` on the counters line, `hits`/`tris`/`const_uploads` unchanged, `declines`/`identity_fail` unchanged, and the surface is byte-untouched (`all 65536 bytes are the sentinel 0x5A`) — nothing drawn by the GPU or by the CPU |
| the same absent program, culling not-all | `misses 1→2` exactly, `declines`/`identity_fail` unchanged, and the CPU interpreter draws it: `100.00% of pixels within 8, worst channel diff 1, coverage cpu=1176 gpu=1176` |
| corpus-carried program, fully culled | `cull_all = 2` (each cull-all batch +1, the visible miss +0), `misses`/`hits`/`tris`/`const_uploads` unchanged, surface sentinel |

**The mutation, re-run by the orchestrator on the final source** (the test author's private scratch
copy of the toolkit with the OLD ordering, `26b036d1…`, next to the fixed copy `873c1e7f…`; both
binaries run with the lavapipe ICD, `RECOMP_GPU_BACKEND=vulkan`, `RECOMP_VK_DEVICE=llvmpipe`):

- final-source build (real tree, from `just build`): **exit 0**, the missing-corpus line reads
  `[r3 cull-all miss] misses 0 -> 1, cull_all (counters line) 1, hits 1 -> 1, tris 1 -> 1,
  const_uploads 1 -> 1, declines 0 -> 0, identity_fail 0 -> 0` and the surface sentinel holds;
- mutant (old ordering): **exit 1**, the only discriminating assertion is
  `FAIL: [r3 cull-all miss] misses advanced by 0, expected exactly 1: a fully culled batch must still
  record that the corpus does not carry its program` (plus the write-once latch symptom
  `first_miss_hash 00000000, expected 80BC1DAA`) — every other R3 assertion and checks 1–5 still pass
  on the mutant, so the miss delta is what separates the orderings, exactly as intended;
- restored final source: **exit 0**, identical deltas.

**Gates on the final tree.** `just build` clean (85 tests, 45 targets; identity recorded); targeted
cases `-R jsrf_vsh_aot` **6/6 passed** (`jsrf_vsh_aot` 14.8 s including the new cases, both negative
controls, `jsrf_vsh_aot_malformed`, the real-table case, the provenance case), `-R jsrf_vsh_conformance`
**9/9 passed**, `-R jsrf_gpu_backend_parity` **3/3 passed**; full software-Vulkan CTest **85/85 passed**,
414.5 s, no skips; `just check` → `check: all checkers passed`.

**Bounded game smoke** (`20261015-225417-747-keyscript-r3-smoke-vk`, `e2e.json` on
`RECOMP_GPU_BACKEND=vulkan`, 560 s, exploratory profile, disposable isolated save): `keyscript_complete`,
runner exit 0, 24,161 presents, `icall 0`/`fatal 0`. Its snapshots (viewed 2026-10-16) show the Garage
with the HUD (`start`), the player skating (`a-forward`), the pause map (`paused`), the return to play beside
Corn (`resumed`) and a later position (`b-after-jump2`); the logged positions change between legs. The jumps
themselves are not visible in the sampled frames, so the run shows movement but not jumps. exe `e116ad4fc329d624e6f3838fbaaab84887a1d3945d3b6606521f835f867d6656`,
xbe `fd190557…`, key script `72c0360f…`. The program path was live throughout — `vsh_hits` climbed past
1,086,000 with `vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `vsh_tris ≈ 288M`, i.e. the
retail corpus covered every program this route uploaded. The run's `cull_all=0`: this script produces
no fully culled program batches, so the R3 path itself is decided by the fixture above, not by the run.
The archived `result.json` separately records `process.exit_code = 1` and
`killed_at_deadline_or_failure = true`: the runner completed its 72-step script and stopped the game
at the 560-second bound. The runner's exit 0 is not a normal game-exit claim.
No validation-layer claim is made (no `VK_INSTANCE_LAYERS` was set), and this smoke is not N2 or V7
acceptance.

**Reviews.** The independent code review found no P0/P1/P2 — safety, counter meanings and the return
contract all hold — and one P3 (the miss/decline double count described above), which is fixed and
re-verified. A security review was not requested: the change adds no trust boundary (it reorders an
in-function lookup; no new input parsing, command execution, credentials or IPC).

**Limits.** (1) `cull_all` is not part of `Nv2aVkProgramStats`; the fixture reads it from the back
end's `[GPUVK]` counters line (emitted on the first flip and then at most once per 10 s), which the case
waits for deliberately. (2) A miss remains a *lookup* outcome: for a culled batch it means "the corpus
does not carry this identity", not "this visibility produced no work". (3) The executor still counts a
cull-all batch's triangles in its own `tris_program_gpu`/`tris_drawn`, which is pre-existing and
unchanged. (4) The first-miss latch is write-once, so the fixture asserts it for consistency only and
decides the ordering from counter deltas. (5) Original V5 coverage numbers are not revised: R3 changes
what future runs measure, and no retrospective coverage is claimed for already-accepted runs.

**Independent Codex review (2026-10-15).** Both repositories fetched clean and synchronized at the
commits above. Codex inspected the implementation, the real executor-backed fixture, the old-ordering
scratch diff and final source identities (backend `873c1e7f…`, fixture `924cb0c3…`). An independent
medium-integrity software-Vulkan run of `jsrf_vsh_aot` passed (CTest exit 0, 15.59 s). The scratch
old-ordering mutant then failed (CTest exit 8, 16.23 s) with
`FAIL: [r3 cull-all miss] misses advanced by 0, expected exactly 1`, plus the first-miss latch symptom.
The final production-source gate passed again afterward; no production source was mutated.
The checked full-suite receipt is 85/85 passed in 414.50 s, no skips; the final `just check` receipt
includes real-table 126/126 with SPIR-V checking. These full-suite receipts were inspected rather
than independently rerun. The smoke archive's profile checker returned EXPLORATORY with zero
UNKNOWN/MISSING archives; its executable SHA-256 matches `e116ad4f…` above. Its actual process
termination is clarified above; no gameplay, performance or validation milestone was accepted here.

Worker transcripts confirm distinct fix/test authors on `opencode-go/deepseek-v4.1-flash:high` and
the code reviewer on `anthropic/claude-opus-5-5:medium`, no recorded fallback. Codex independently
audited every outgoing R3 blob in its own repository (toolkit 1, game 4): zero secret hits, forbidden
paths or oversized blobs. Both original R3 commits are unsigned with no co-author/signature trailers.

PUSHED_TO: origin / BRANCH: main / COMMIT: a052de502f7f167a144460b21943cb80740a97c5 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: synchronized fast-forward, toolkit pushed first.
PUSHED_TO: origin / BRANCH: master / COMMIT: 5842b381f60871e44fb84536eb08d9606d0bf8a4 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: synchronized fast-forward, after toolkit.

R3 is accepted; the owner's requested stopping point is reached. N2 and later steps have not started.
The current supervision lineage retains 10 operation slots and zero remediation cycles; resuming
does not reset those limits. No next operation is authorized while the owner pause remains in force.

### §31.27 N2: a controlled Garage benchmark, on both backends and the same GPU (2026-10-16)

**The owner resumed the work with a Claude session on 2026-10-16.** Whole-run present totals are not
comparable, so performance is now measured in a fixed window.

**Tooling.**

- **`bench` step** in `scripts/key-script-run.py`. For a fixed window and with no input, it polls the
  exe's `s_present_serial` every 2 ms from outside the process (`GuestMemory.host_read`; the game is
  unchanged) and writes `bench.json`.
- **What `bench.json` records:** presents/s; frame-time p50/p90/p99/max/mean, computed from
  increase-to-increase intervals, where a poll that saw +n gives n equal intervals and counts as
  aliased; stalls over 100 ms; the backend, device and exe SHA-256.
- **`config/key-scripts/garage-bench.json`:** the intro prefix, `wait_screen hud`, a 5 s settle, then
  `bench garage 60`.
- **`scripts/bench-summary.py`:** per-backend medians and ranges.
- **Tests from a different worker:** `tests/test_key_script_run.py` (26 new) and
  `tests/test_bench_summary.py` (17, CTest `jsrf_bench_summary`). The four deciding mutations fail
  them:
  - poll-to-poll intervals;
  - percentiles over raw polls;
  - +n giving one interval;
  - the lower-middle median.

**Measurement.**

- **Common conditions:** exe `e116ad4f…` (the R3 build), exploratory profile, disposable saves, one run
  at a time, 560 s bound. The window starts at the Garage spawn: `bench-start.png` shows the spawn
  plaza and its HUD.
- **Poll period:** 2.01–2.05 ms; **0 aliased polls in every run**.

| Backend, device | Runs | Presents/s, median (range) | p50 / p90 / p99 frame (ms), median |
|---|---|---|---|
| D3D11, Intel Iris Xe (its default adapter) | `20261016-093255-486`, `-094405-138`, `-095327-949` `keyscript-n2-d3d11-1..3` | **15.17** (15.10–15.53) | 63.9 / 69.6 / 78.1 |
| Vulkan, Intel Iris Xe (`RECOMP_VK_DEVICE=Intel`) | `20261016-103228-954`, `-104153-357`, `-105117-660` `keyscript-n2-vulkan-intel-1..3` | **12.77** (12.35–12.78) | 77.7 / 86.0 / 97.8 |
| Vulkan, RTX 4070 (its default) | `20261016-100250-586`, `-101213-264`, `-102135-973` `keyscript-n2-vulkan-1..3` | **11.07** (9.90–11.30) | 89.9 / 93.9 / 102.4 |

**Reading (inference, labelled).**

- **Same GPU:** Vulkan is about 16% slower than D3D11.
- **Faster GPU, slower result:** Vulkan on the discrete RTX 4070 is slower than on the integrated GPU.
  The cost therefore sits in host-side synchronisation and readback (one fence wait per submission,
  and every target read back across PCIe at each flip), not in GPU work. That is N4's list.
- **Earlier whole-run presents/s** (10.8–23.3) were not this quantity and must not be compared with
  these.
- **Caveats:**
  - the third RTX 4070 run was slower than the other two, and its 315 "stalls" are an artefact of its
    p50 sitting at the 100 ms threshold;
  - the D3D11 backend cannot be pointed at the RTX 4070, so no D3D11/NVIDIA row exists.
- **From here on,** performance claims use this benchmark (plan N2).

**Comparability, measured 2026-10-16 (N4.1).** A fresh control on the SAME exe (`e116ad4f…`), device
(RTX 4070), driver string and script measured **33.48 presents/s** (median of three 60 s windows:
`20261016-162400-003`, `-163322-757`, `-164245-452`; `readback_ms`/flip 10.6 ms; 21 209 flips in 556 s)
against the 11.07 recorded above (24.4 ms/flip; 8 038 flips). The `bench-start.png` of both is the same
Garage view, so this is not a different scene: the archived row was measured in a roughly 2.3–3× slower
machine state and is **not comparable with fresh numbers**. The three archived rows still compare against
each other; a fresh measurement needs its own same-device control, which is what §31.29 does.

### §31.28 V7: Vulkan acceptance, and V8's play option (2026-10-16)

All runs are exploratory, with disposable saves, on exe `e116ad4f…` (the R3 build, unchanged since).

**`e2e.json` on Vulkan, RTX 4070, with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`**
(`20261016-111544-203-keyscript-v7-vk-val`):

- `keyscript_complete`, with no unsatisfied step;
- 0 ICALL, 0 ABI-failure lines, 0 fatal;
- `validation_errors=0`;
- `vsh_hits=363943`, `vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`.

**The validation layer and its warnings.**

- The run reports `validation_warnings=10`, all one message: `vkCreateGraphicsPipelines(): … Vertex
  attribute at location 1/7/8 not consumed by vertex shader`.
- The program pipeline declares one input per `input_mask` bit, and dxc removed reads the program
  never uses. That is a performance warning, not an error, and it changes no pixel.
- Those messages arrive through the backend's debug messenger, so **they themselves witness that the
  layer was active**. The loader prints no insertion line without `VK_LOADER_DEBUG`.
- The validation layer slows the run (6,663 flips against about 10,700 without it), so this run makes
  no performance claim.

**Frames against D3D11** (`20261016-112507-055-keyscript-v7-d3d`, the same script, also complete):

| Step | Comparison |
|---|---|
| Corn's opening talk (`intro-00`) | the same text and "Corn" label |
| Garage start | the same view, player colours and HUD |
| Pause map | the same |
| Logos | Vulkan's timeline shows SEGA, Smilebit ramping to (255,255,255) and back, then ADX |

The last skate frame differs only in position: timed input at different frame rates.

**Dialogue dwell on Vulkan** (`20261016-114049-947-keyscript-v7-vk-dwell2`):

- the "Corn" label is present in **40/40** snapshots (white count 249 on all of them);
- the balloon text is stable after each page's reveal.
- A first attempt at 280 s reached only 35 snapshots (all with the label) before its deadline.

**N2 medians** (§31.27): Vulkan 12.77 presents/s against D3D11 15.17 on the same Iris Xe, and 11.07 on
the RTX 4070.

**V7 is met.** Everything compiles ahead of time, the corpus covers every program the route uses, the
validation layer reports no errors, the frames match D3D11 at the checked steps, and the dialogue is
stable. Still open: the slower frame rate (N4) and owner acceptance (V8).

**V8:**

- `play-jsrf.bat vulkan` sets `RECOMP_GPU_BACKEND=vulkan`, and every other argument passes through to
  the executable. The default stays D3D11.
- Checked live: started from a helper and stopped after 30 s, the Vulkan mode logged
  `[GPUVK] device: NVIDIA GeForce RTX 4070 Laptop GPU` and the default logged
  `[GPUBE] device: hardware (Intel(R) Iris(R) Xe Graphics …)`.
- Owner play is next.

**V8 follow-up (2026-10-16, owner):** `play-jsrf.bat` now **defaults to Vulkan**. The new
`play-jsrf-d3d11.bat` calls it with `d3d11` and passes every other argument through, so D3D11 stays
available as the reference. `play-jsrf.bat d3d11` does the same. The key-script runner's own default
(`DEFAULT_ENV`) is unchanged, so archived evidence keeps its meaning.

### §31.29 N4.1: bounded frames in flight (2026-10-16)

Plan N4's first bullet, on top of V7. Toolkit `c5f3696` (pushed to origin/main). The pre-change build
is the same checkout's R3 exe `e116ad4f…`; the measured build is `ec4bc150…` (the MSVC link is not
reproducible, so that hash is per-build, not per-source).

**What changed.** The Vulkan back end recorded one command buffer and submitted and waited on it at
every flip, at every target flush and at any stream or descriptor-pool overflow. It now runs a
**bounded ring of three submission slots** (`VB_SLOTS`). Each slot owns one command recording, one
`VkFence`, one `VkDescriptorPool` (2048 sets) and one host-visible 16 MiB upload stream, plus the
`bound_pipe` of its recording; only the slot being recorded is written to. `pending == 0` (its fence
not yet observed; fences are created unsignaled, as `vkQueueSubmit` requires) is the whole definition
of a free slot. A submission is issued WITHOUT waiting for the ones before it; the host reaches a
fence in only three places: reusing an occupied slot, publishing guest memory (a flip's readback, a
target flush or release, install) and a device error, where `g.ok = 0` freezes the back end and
nothing is freed, reset or reused.

**Why the ordering is sound.** Vulkan defines no implicit execution or memory dependency between
submissions (vkspec §7.2), so every submission ends with the existing broad barrier
(`ALL_COMMANDS`/`MEMORY_WRITE` → `ALL_COMMANDS`/`MEMORY_READ|WRITE`) recorded *outside* any render
pass, whose second synchronization scope is every command later in submission order (vkspec,
`vkCmdPipelineBarrier`). That orders an earlier submission's attachment and transfer writes against a
later one's attachment reads and writes, later render-pass `LOAD_OP_LOAD` reads and later
`vkCmdCopyImageToBuffer` reads — including two submissions that blend into one target. The ownership
invariant that makes reuse sound is that **a submission references only its own slot's stream and its
own descriptor pool**: a frame splits only at a draw entry, before that draw's stream slices exist;
`vb_transfer_cmd` never splits; and `vb_stream` never moves the slot — a slice over half a slot's
stream, or one that no longer fits what is left of it, gets a buffer of its own (vertices, indices,
transfer source, uniform block) whose destruction waits for a whole-ring drain. Waiting one slot's
fence is therefore enough before rewriting its stream or resetting its pool. Objects that recorded
commands may still use are held until nothing is being recorded and no submission is in flight,
exactly as before.

**Overlap.** `vb_maybe_split()` runs at the top of `vb_draw`/`vb_clear`/`vb_draw_program` and submits
the recording slot once it holds `VB_SPLIT_BYTES` (2 MiB, an eighth of a slot's stream) or
`VB_SPLIT_SETS` (32 descriptor sets) and the next slot is free, so the device works on the earlier part
of a frame while the host records the rest. `vb_reserve` is the hard bound: a stream or pool overflow
advances the slot (waiting only if the slot it moves into is still in flight). The per-draw vertex cap
and the program-batch reserve bound keep the values they had with one 32 MiB stream. The counters the
records cite kept their meanings; `submits` still counts submitted command buffers, and the new
`splits`, `overflows`, `reuse_waits`, `drain_waits` and `max_inflight` are reported by
`nv2a_vk_backend_ring_stats()` and in the periodic `[GPUVK]` line. They are host-side accounting —
outstanding (submitted, unwaited) slots and fence waits performed — not timestamped proof of
device-side overlap, and `vb_drain` counts one wait per slot.

**The gate** (`jsrf_vk_frames_in_flight`, authored by a different worker, registered in
`CMakeLists.txt`, run on lavapipe): 15 frames of 16 112 cell draws plus auxiliaries. The executor
counted 241 981 batches and the back end drew 241 981 — 1:1, which is what makes the run non-vacuous
(the first version of the fixture drew tiny non-screen-space batches that the executor drops while
still counting them: 17 992 counted against 8 drawn). It checks every frame's guest bytes after the
flip (grids, a 1024² fill, SRC/DST, R32/R16 read-back and re-load, a depth-order probe), per-frame
constants that encode (frame, target, cell) with a no-repeat check, a target sampled as a texture on
the next frame, eviction/release pressure (22 targets > 16, 520 textures > 512, 11 depth buffers > 8,
96 (target, depth) pairs > 64) and compares 270 of 270 surfaces byte-for-byte with the CPU executor
plus 45 of 45 pixel-by-pixel. From `nv2a_vk_backend_ring_stats()` it asserts `max_in_flight >= 2`,
`submissions > frames`, `splits >= 1`, `reuse_waits >= 1`, `drain_waits >= 1`, `frames == flips`,
`in_flight == 0` after each flip's publish and `submissions >= batches / VB_SLOT_SETS - slots`, each
failing with a readable line. Green run: `slots=3 in_flight=0 max_in_flight=3 submissions=376
reuse_waits=60 drain_waits=316 splits=210 overflows=60 frames=15`.

**Deciding mutations** (private scratch `C:\Users\logic\xbr-mac\n4mut`, never committed; each mutant
was copied over the toolkit source, built, run, and the production file restored from a saved copy and
sha256-verified before anything else ran):

| Variant | backend sha256 | Gate |
|---|---|---|
| Baseline | `99c7c21f…` | PASS (ctest exit 0) |
| A: `vb_slot_submit` waits for its own fence (the pre-change behaviour) | `9065f5d1…` | FAIL, ctest exit 8 — `max_in_flight >= 2`, `reuse_waits >= 1` and `drain_waits >= 1` all FAIL, with `max_in_flight=1 submissions=7621 splits=7515 reuse_waits=0 drain_waits=0` |
| B: `vb_slot_wait` skips the fence wait and resets/reuses the slot's pool, stream and held objects anyway | `a93fc0ed…` | FAIL, ctest exit 8 — `***Exception: SegFault 7.39 sec` |
| Restored baseline | `99c7c21f…` | PASS (the same counters as the baseline) |

Mutant A keeps every content check passing and is caught only by the overlap/vacuity claims; mutant B
is caught as a crash from using resources a pending submission still owns.

**Reviews.** A resource-lifetime/ordering review (three passes) found fences created *signaled* and
submitted as such (`VUID-vkQueueSubmit-fence-00063`) — fixed by creating them unsignaled with
`pending == 0` as the only notion of a free slot — and recording into a command buffer after a failed
fence wait — fixed by `g.ok` guards after every wait that can happen inside a draw (`vb_target_for`,
`vb_target_create`, `vb_target_load`, the flush loop, `vb_begin_pass`, `vb_depth_for`, `vb_tex_upload`,
`vb_texture`, the three draw entries) — plus a `poolSizeCount` typo that edit introduced; its final
verdict was `sound`. The configured security review of the changed native-resource lifetime/error
paths reported the same post-wait sites and a `vkBeginCommandBuffer` failure that did not stop the back
end (both fixed), with no premature reuse, host-memory unsafety or unbounded growth on the success
paths. The independent code review found a host slice that could be bound as a **uniform buffer from a
buffer created without uniform usage** (fixed by adding `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT`),
per-upload dedicated allocations when a slot's stream was full (fixed by reserving in
`vb_target_load`/`vb_tex_upload`), and counter wording that overstated what the numbers prove (fixed,
plus a new `overflows` counter).

**N2 (fresh, matched: same device, script, profile and quiet machine; three 60 s Garage windows each;
0 aliased polls and 0 stalls in every run).** Before: exe `e116ad4f…`, runs
`20261016-162400-003`/`-163322-757`/`-164245-452`, **33.48 presents/s median** (33.35–34.67), p50
29.9 ms. After: exe `ec4bc150…`, runs `20261016-183337-668`/`-184300-416`/`-185223-046`, **43.40
presents/s median** (42.07–43.60), p50 22.4 ms: **+29.6 %** in presents/s and −25 % in p50 frame time.
The after runs' own counters show the ring working in the real workload — `splits` ≈ 1.2 per flip,
`overflows` and `reuse_waits` in the hundreds, `max_inflight=3` — and the validation-layer run below
reports `validation_errors=0`. The remaining mandatory wait is the flip's readback itself (guest
memory must be published before its consumers see it); the comparability note in §31.27 explains why
the archived N2 rows cannot be compared with fresh ones.

**The known one-off crash recurred, twice, under load.** Two of the first three after-benchmark
attempts (`20261016-175740-883`, `20261016-180805-012`) exited at 57.7 s and 59.4 s with
`exit_code 0xC0000005` after a `[EXCEPTION first-chance] … fault=0x10000FFBE (read)` during an asset
preload — the same signature, timetable and fault address as the one-off recorded and deliberately not
repaired in this file's V4 survey ("A one-off crash, recorded and not repaired"). Those two attempts
overlapped a subagent's CTest/lavapipe workload; six later 90 s trials on a quiet machine (three with
the split rule, three with it disabled) and all six quiet runs since have been clean, and the same
script's immediate rerun (`20261016-175842-197`) ran the full bound. The event is therefore recorded
here as recurring under heavy concurrent CPU load, not as a new class and not as repaired by this
packet.

**Acceptance runs on the final source (N3).** The stock `e2e.json` completed on both:

- **e2e smoke**: `20261016-190211-665-keyscript-n4-e2e-vk` (580 s bound, Vulkan, exploratory,
  exe `ec4bc150…`): `keyscript_complete`, 0 ICALL lines, 0 ABI failures, 0 fatal, `vsh_misses=0`,
  `vsh_hits=1 470 241`, `rc_fallback=0`, 36 332 flips with 107 068 submissions (`splits=70 072`,
  `overflows=663`, `reuse_waits=663`, `max_inflight=3`).
- **N3 validation run**: `20261016-191154-335-keyscript-n4-n3-val` (620 s bound, Vulkan,
  `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` **and** `VK_LOADER_DEBUG=layer`, exploratory, the
  same exe): `keyscript_complete`, 0 ICALL lines, 0 ABI failures, 0 fatal, `validation_errors=0`,
  `validation_warnings=10` (all `vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8 not
  consumed by vertex shader`, the same ten benign warnings V7 recorded: dxc removes reads the program
  never uses), `vsh_misses=0`, `vsh_hits=1 361 554`, `rc_fallback=0`, 25 663 flips with 75 086
  submissions (`splits=48 760`, `max_inflight=3`). The loader's own insertion lines, quoted from the
  run log:

      [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
      [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

  Both ran on a quiet machine, one at a time, with disposable saves; the performance runs above were
  kept separate (no validation layer, no `VK_LOADER_DEBUG`).

### §31.30 N4.1 independent rejection: a failed wait still permits a pending-stream write (2026-10-16)

**N4.1 is not accepted.** Game `adc2c1b` / toolkit `c5f3696` were fetched clean and synchronized.
The independent software-Vulkan `jsrf_vk_frames_in_flight` run passed in 19.18 s, with the reported
270/270 exact surfaces and 45/45 pixel comparisons. The archived final CTest receipt is 87/87 in
484.54 s; the final checker receipt includes the real corpus 126/126. Codex recomputed the N2
medians from all six actual `bench.json` records (33.4843 before, 43.4007 after), verified the
N3 loader insertion lines in the actual log, and reclassified both acceptance archives as
EXPLORATORY with zero UNKNOWN/MISSING results. None of these success-path checks proves safe
failure handling.

**Deciding defect (P2).** `vb_reserve` can advance to an occupied slot; `vb_slot_wait` times out,
sets `g.ok = 0`, and correctly leaves that slot pending. But `vb_target_load` immediately calls
`vb_stream` and copies the guest pixels into its returned pointer before testing `g.ok`. If the
pending slot still has room, this overwrites its mapped stream while its GPU work can still read it.
The guard before recording the copy is too late. `vb_tex_upload` has the same reserve-then-write
ordering and needs the same audit; draw paths already guard immediately after reserve.

**Reproduction and positive control.** A standalone offline-built harness uses five unmodified
production function bodies (`vb_slot_wait`, `vb_slot_advance`, `vb_reserve`, `vb_stream`,
`vb_target_load`). Only the surrounding submission/driver outcomes are controlled stubs: current
slot full, next slot pending but with stream capacity, a one-pixel target, and `vkWaitForFences`
returning SUCCESS or TIMEOUT. This is a controlled production-code failure-path test, not a live
GPU timeout or an explanation of the two startup crashes recorded in §31.29.

```
SUCCESS: waits=1 reset=1 ok=1 pending=0 stream_changed=1 first_byte=CC copies=1
TIMEOUT: waits=1 reset=0 ok=0 pending=1 stream_changed=1 first_byte=CC copies=0
positive_control=PASS timeout_preserves_pending_stream=FAIL
```

The production-body harness exits 1. A private proposed control adds only an early `g.ok` return
immediately after `vb_reserve` in `vb_target_load`; it exits 0: success still uploads, while TIMEOUT
leaves the pending stream's sentinel byte `5A` unchanged and records no GPU copy. No production
repository code, original assets or saves were changed by the reproduction. This proves a concrete
repair point; it does not claim the full backend is repaired.

**Other evidence.** Actual worker transcripts confirm distinct gate authors on OpenCode DeepSeek,
ordinary/hazard reviewers on Opus 5.5, and security review on Muse 1.3, no recorded fallback.
Codex audited every outgoing candidate blob in the correct repository (game 5, toolkit 2), zero
secret hits, forbidden paths or oversized blobs. The original mutations A/B were verified from
their source copies and full logs: immediate wait fails the overlap assertions; bypassed fence
wait crashes. These do not exercise the timeout-freeze condition reproduced above.

**Required bounded repair before continuation:** stop before touching any slot-owned stream/pool/
command recording after a failed wait; audit both upload paths and failure propagation through
command-begin/transfer helpers; have a different worker add a deciding failed-wait regression,
with a successful-wait positive control and a weakening mutation. Re-run the lifetime/parity gates
and relevant final validation. Keep all other N4 items out of the repair packet.

The current bridge run has 9 operation slots but **zero remediation cycles** remaining. Codex
withholds acceptance and requests owner authorization for one additional repair cycle; neither
rotation nor a successor run may silently reset limits. No batched-readback packet is authorized.

**Owner policy update (2026-10-16).** The owner requested: "Update the plan orchestrator to allow
a reset after confirming the review and adding it's own advice." The installed orchestration skill's
Codex Plan reference now permits an exhausted allowance to be renewed for one bounded in-scope
repair after independent confirmation and Codex's own written assessment, repair advice, gates
and stopping condition. The skill validator passed. Prior decisions and cumulative spending remain;
context rotation alone grants nothing, and hard caps, deadlines and owner stop conditions still apply.

For this finding the confirmation is the five-production-function reproduction and passing private
early-guard control above. Codex's advice is to guard immediately after reserve before stream access
in BOTH target and texture uploads, then audit command-begin/transfer helpers so a failed begin or
wait cannot be followed by slot writes, pool/command reset or invalid recording. Keep initialization
valid without reviving a failed backend. A different worker must add failure-injection regressions
for both upload paths and relevant begin/transfer propagation, with successful-path controls and
mutations removing the decisive guards. Preserve the existing lifetime/parity/V5/V6 gates and
witness final N3 validation. This is the same N4.1 repair, not the batched-readback step.

One renewed repair cycle is assigned to the linked successor's first repair operation and is consumed
by its admission. The closed escalated run and its single review decision remain unchanged; the
successor carries the 9 remaining operation slots, with zero additional unassigned repair cycles.
No operation-limit reset, duplicate decision or direct bridge-journal edit is authorized.

### §31.31 N4.1 repair: a failed wait still permitted a pending-stream write (2026-10-16)

Toolkit `f41c0d2` (repair) on top of `c5f3696` (the ring). The deciding regression is game-side:
`jsrf_vk_ring_fault_gate` (`tests/test_vk_ring_fault_gate.c` with the build-time probe
`scripts/vk-ring-fault-gate.py`). The rejected candidate was toolkit `c5f3696` / game `adc2c1b`.

**The defect.** `vb_reserve` can advance the cursor into a slot that is still `pending` (the GPU's),
and the fence wait that advance performs can fail: `vb_slot_wait` sets `g.ok = 0` and correctly leaves
the slot pending, its fence unreset and its pool/stream accounting intact. `vb_target_load` and
`vb_tex_upload` nevertheless called `vb_stream` and wrote guest pixels into that pending slot's mapped
16 MiB stream; their `g.ok` check came only before recording the copy, which cannot undo a write.
Reproduced twice: by a private harness of five verbatim production functions (normal wait control
passes; an injected `VK_TIMEOUT` leaves `ok=0 pending=1` but changes a sentinel byte, exit 1; a private
early-guard control exits 0), and by the new gate against the pre-fix source, where property 1 reports
`4077 of 16777216 bytes differ` and `stream_off 0 -> 4096`, and property 2 reports 4085 bytes with one
image created for the aborted upload.

**The repair.** (1) `vb_target_load` and `vb_tex_upload` return immediately after their `vb_reserve`
when `g.ok` is 0 — before `vb_stream`, before a byte is written. (2) `vb_stream` refuses at entry once
the failure latch is set, so a latch that appears during a multi-slice draw cannot hand out later
slices, and its oversized-slice branch frees its own fresh buffer and returns NULL when the begin it
performs failed; an independent review found that otherwise `vb_discard` frees that buffer immediately
(a failed begin no longer marks the slot recording) and the branch returned a pointer into freed
memory. (3) A failed `vkBeginCommandBuffer` latches `g.failed` (write-once, with `g.ok = 0`) and leaves
the slot's command buffer untouched; recording goes to `g.dead`, one extra command buffer begun once at
install and never ended, submitted or reset, so any stray `vkCmd*`, transfer or barrier is valid and
discarded. (4) `g.pass_cmd` remembers the buffer that began an open render pass and ends that buffer.
(5) The flush's post-loop barrier goes through `vb_cmd()`. (6) `vb_create_defaults` cannot revive a
latched failure at install (it refuses, and its post-submit check is `g.failed || !g.ok`) and takes its
copy's command buffer from `vb_transfer_cmd()`. `vb_slot_wait`'s failure path is otherwise unchanged:
`pending` stays 1, no fence/pool/stream reset, nothing freed.

**The deciding regression.** `jsrf_vk_ring_fault_gate` (CTest, `TIMEOUT 300`, no `SKIP_RETURN_CODE`, no
`--skip`, no ICD or environment: it always runs and exits 0 or 1, never 77). Its translation unit
`#include`s `src/video/nv2a_vk_backend.c` with `XBR_VK_BACKEND` defined, so the bodies under test are
the production ones and nothing can drift; the probe reads the same bytes at build time and fails the
BUILD if a definition, the slot struct or a ring constant the fixture uses is gone; every Vulkan entry
point is a static function pointer the test points at stubs, and the injected `VK_TIMEOUT`/begin
failures are stub return values, not a real device timeout. Properties: 1 target-load failed wait, 2
texture-upload failed wait, 3a/3b normal controls, 4 failed begin and its control, 5a/5b post-failure
recording and a pending slot with the device gone, 6 an oversized slice whose begin fails, 7 the latch
mid-draw. Green on the repaired source: **9 properties, 105 checks**, exit 0 — property 1 compares the
pending slot's whole 16 MiB stream byte-for-byte and count-asserts no fence reset, no pool reset, no
submit and no copy; property 6 asserts the call returns NULL with exactly one free and one destroy and
nothing live; property 7 asserts both the ring and oversized slices are NULL with no buffer,
allocation, mapping, offset move or write, and nothing begun, reset or submitted. No group is
indeterministic: no device, no clock, no threads, and failure output carries the differing byte count
and both checksums.

**Mutation matrix (private scratch trees; the test prints the sha256 of the bytes it compiled and every
row matched the file built).** Independent worker:

| Variant | backend sha256 | exit | failing properties |
|---|---|---|---|
| repaired (baseline) | `03da3147…` | 0 | none |
| M1a: the callers' early guards deleted only | `dc69aaae…` | 0 | none — measured inert: `vb_stream`'s entry refusal is what stops the write |
| M1: those guards plus `vb_stream`'s entry refusal deleted | `a244b1b6…` | 1 | 1, 2, 7 |
| M2: a failed begin arms the slot and returns its own buffer | `b04f7d58…` | 1 | 4, 5a, 6 |
| M3: the oversized-slice refusal deleted (the dangling-pointer form) | `b5d5d95d…` | 1 | 6 |
| restored | `03da3147…` | 0 | none (run output byte-identical to the baseline) |

Two measured deviations from the brief's expectations are recorded rather than smoothed over: M1 has to
remove `vb_stream`'s own refusal to be decisive for properties 1/2 (M1a alone is inert, which the
test's header predicted), and M2 also reddens property 6 because that fixture keys on the same latch.
The test's own author independently measured the same pattern in a first scratch tree with its own
variant names (pre-fix source red on 1, 2, 4, 5a, 5b, 6, 7; "callers' guards only" inert; "callers plus
the entry refusal" red on 1, 2, 7; "oversized refusal deleted" red on 6; failed-begin mutant red on 4,
5a; restored green), and the second tree's rows each printed the sha256 of the bytes compiled, matching
the file built. No test or shared-tree edit was made by either worker.

**Reviews.** An independent code review found the dangling dedicated-slice path above (fixed, M3 is its
mutant) and two comment/nit items (fixed); its earlier hazard pass had found the signaled-fence and
post-wait-recording defects now fixed in `c5f3696`/`f41c0d2`. The configured security review of the
changed native-resource failure paths ended `sound` (mapped-stream arithmetic, garbage gating, command
buffer state, post-failure API use and install semantics), with one benign note: a single
`vkAllocateDescriptorSets` attempt could still follow a `vb_stream`-internal begin failure — the new
entry refusal removes even that.

**Validation on the final source.** `jsrf_vk_frames_in_flight` passes (17.3 s), the full suite is
**88/88** through `python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure`, and `just check`
passes. The bounded e2e under N3 (`20261016-212431-655-keyscript-n4r-e2e-val2`, 620 s bound,
exploratory, exe `37d9c75e…`, game `47255e5` + the repair patch, device RTX 4070) completed:
`keyscript_complete`, 0 ICALL lines, 0 ABI failures, 0 fatal, `validation_errors=0`,
`validation_warnings=10` (all `vkCreateGraphicsPipelines … Vertex attribute at location 1/7/8 not
consumed by vertex shader`, the same ten benign ones V7 recorded), `vsh_misses=0`, `vsh_hits=1 419 520`,
`rc_fallback=0`, 26 657 flips / 78 056 submissions (`splits=50 736`, `max_inflight=3`). The loader's own
insertion lines, quoted from that run's log:

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

**Runner observation (recorded, not explained).** The first N3 attempt
(`20261016-211121-778-keyscript-n4r-e2e-val`, same command shape) ran `keyscript_complete` with
`validation_errors=0` and the same ten warnings — so the layer was active — but its log carried no
loader debug output at all (0 lines against 158 in the successful runs). It was rerun with
`VK_LOADER_DEBUG=layer` also exported in the invoking shell, which produced the witness above. Whether
the difference is the runner's override handling or something else was not established; the witness
line for the record comes from the rerun.

**No new performance claim.** The 43.40 presents/s N2 figure belongs to `c5f3696`; this repair adds two
`g.ok`/latch tests per slice, two NULL refusals and one command buffer allocated at install — failure
paths only.

**Limits.** No live GPU timeout was exercised: the injected failures are stub return values, which is
what makes the case deterministic; the real-device behaviour of a failed wait (a frozen back end that
publishes nothing further) is unchanged and itself unrehearsed. After a latch nothing drains, so the
garbage list freezes at the failing call's entries (bounded, nothing freed); the dead buffer may
reference objects that are later freed, which is never executed and therefore validation-only surface.

**Independent Codex acceptance (2026-10-16).** Both repositories fetched clean and synchronized at
game `389a03a` / toolkit `f41c0d2`. Codex reviewed the repair, included-production-source fault test
and build-time probe, then independently ran the real software-Vulkan lifetime gate (18.98 s) and
fault gate (0.26 s): both passed, no skips. The latter exercises 9 properties and 105 checks with
controlled driver outcomes; it is not a real GPU timeout. Independently re-executed scratch variants
reported their compiled source identities: baseline/restored `03da3147…` exit 0; callers-only
`dc69aaae…` exit 0 (the entry refusal remains decisive); M1 `a244b1b6…` exit 1, 3 properties / 8 checks
failed; M2 `b04f7d58…` exit 1, 3 properties / 16 checks failed; M3 `b5d5d95d…` exit 1, 1 property /
1 check failed. The reported distinction between redundant caller guards and the combined decisive
mutation is preserved, not counted as a false pass of the behavior gate.

The inspected named final CTest receipt (`final-ctest-n4r.txt`) records 88/88 passed in 443.91 s;
the post-record checker receipt includes the real corpus 126/126. These receipts were inspected,
not independently rerun in full. The final N3 archive `20261016-212431-655-keyscript-n4r-e2e-val2`
reclassifies EXPLORATORY, zero UNKNOWN/MISSING; its recorded backend hash is the actual final
`03da3147b4cdb5828094a09c24feea3890af0e05628edbf79e94ba5b643e7715`, and its executable hash matches
`37d9c75e…` above. Codex verified the actual Khronos insertion lines and final counters: 0 validation
errors, the known 10 warnings, 0 program misses/identity failures/declines. The script completed and
the game was stopped at 620.5 s (process exit 1); this is not a normal game-exit claim.

Actual transcripts confirm different fix/test/mutation workers on OpenCode DeepSeek, code review
on Opus 5.5 and security review on Muse 1.3, no recorded fallback. Every outgoing repair blob was
independently audited in its own repository (game 6, toolkit 1), zero secret hits, forbidden paths or
oversized blobs. Both commits are unsigned, without co-author/signature trailers.

PUSHED_TO: origin / BRANCH: main / COMMIT: f41c0d24295caf3bc62f87b7a64baf08bc14c259 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: synchronized fast-forward, toolkit pushed first.
PUSHED_TO: origin / BRANCH: master / COMMIT: 389a03a5160534a69314963593dc4f749a391e05 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: synchronized fast-forward, after toolkit.

N4.1 is accepted on this repaired state. The next packet may cover only the separate batched-readback
item; the prior 43.40 presents/s measurement remains attributed to `c5f3696`, and no new performance
claim or repair of the unrelated startup crash is inferred. Eight operation slots remain before the
next dispatch; the renewed repair cycle has been consumed, with no extra unassigned cycles.

### §31.32 N4.2: the flip's readback is packed on the GPU (2026-10-17)

Toolkit `891bca2` on top of `f41c0d2`; the game-side gate and these records on top of `479837a`.

**What was already there.** `vb_targets_flush` already queued every dirty target's
`vkCmdCopyImageToBuffer` into one submission and drained once. That batching is NOT this item. What
cost host time was what followed it: for a 16-bit target the published bytes were produced by a
per-pixel host loop (`t->bpp == 2`), and every target owned its own full-size host readback buffer.

**Where the flush's time went before** (counters-only build of the accepted source, exe `1ea785bd`,
run `20261016-223221-992-keyscript-n42-diag-vk-1`, RTX 4070, the Garage window): 27 335 flips,
54 669 readbacks (exactly two dirty targets per flip), 0 stalls, 0 aliased polls. Per flip:
record 0.017 ms, fence wait **5.33 ms**, host publish **0.564 ms** — 5.91 ms of a 22.18 ms p50
frame. Every dirty target was 16-bit (`rb_px16 = 16 794 316 800` pixels, `rb_px32 = 0`, 33.6 GB of
5:6:5 bytes) at ~1.09e9 pixels/s through that loop. So the ceiling this item could buy was ~2.5 %
of frame time, and the drain — 90 % of the flush — was out of scope by the packet's own terms
(no narrower barriers, no deferred publication).

**What changed.** The publication stages into ONE bounded pair of buffers, grown on demand and
never past a 96 MiB cap: a host-visible slice buffer (`g.rb_buf`, `TRANSFER_DST`) and a
device-local pack buffer (`g.pack_buf`, `STORAGE_BUFFER|TRANSFER_SRC`; a compute shader writing
host memory over PCIe can cost more than the loop this removes). A batch is every dirty target
whose 4-byte-aligned slice still fits, recorded as one submission: 4-byte targets copied straight
out of their image with tight rows (`bufferRowLength = rect width`), 16-bit targets dispatched
through `cs_pack` and then moved to the host buffer by one `vkCmdCopyBuffer` per slice — followed
by one barrier, one submit, one fence wait (the whole ring drained, as before) and a per-row
memcpy into guest RAM. A target whose slice cannot be staged keeps `dirty` and is retried; a batch
whose pack descriptor set cannot be allocated is skipped whole (nothing submitted, nothing
published). Replaced staging pairs go through `vb_discard`, so a submission still in flight keeps
the buffers it reads. The per-target `w*h*4` readback buffers and the truncation loop are gone,
and a 16-bit target's PCIe readback traffic falls from 4 to 2 bytes per pixel.

`cs_pack` is new tracked HLSL (`src/video/shaders/nv2a_vk_pack.hlsl`), compiled and
`spirv-val`'d ahead of time by the existing `xbr_add_spirv` rule as `-T cs_6_0` with no invert-y;
nothing is compiled or translated at run time. One thread packs a pixel pair:
`[numthreads(8,8,1)]`, dispatch `(ceil(w/16), ceil(h/8))`, row stride `round_up(rect_w*2, 4)` in
the staging buffer so every store is a 4-byte-aligned uint32 (a 16-bit store would need the
optional 16-bit-storage feature); an odd width leaves the second pixel of the last pair out of
range and its two slack bytes are written but never published.

**Why the bytes are the same.** The kernel computes
`(r8>>3)<<11 | (g8>>2)<<5 | (b8>>3)` from the channels a `B8G8R8A8_UNORM` load returns — the same
TRUNCATION the deleted loop computed from the guest's little-endian `B|G<<8|R<<16|A<<24` word
(`((c>>8)&0xF800)|((c>>5)&0x07E0)|((c>>3)&0x001F)`, derivation in the shader's header comment) —
never a rounding or normalised conversion. The byte recovery `(uint)(saturate(x)*255.0 + 0.5)` is
exact for all 256 values (the float error is ~1e-5, so it survives fp16 fetch precision and FMA
contraction).

**Ring and lifetime rules kept.** `g.ok` is checked after `vb_transfer_cmd()` and after each
`vb_reserve`; each pack takes `vb_reserve(0, 1)` before its descriptor-set allocation so the pool
budget cannot be over-committed; every command is recorded through `vb_cmd()`/`vb_transfer_cmd()`
so a latched failure records into `g.dead`; a batch is published only after its own
`vb_slot_submit()` + `vb_drain()`.

**Capability requirement (one decision, no second path).** The pack needs a compute dispatch, so
`vb_graphics_family` returns the first family with `VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT`
and `vb_pick_device`'s `usable` test requires it; a device whose graphics family has no compute is
reported ` unusable (graphics family has no compute, which the readback's pack kernel needs)` and
passed over, and the executor's CPU rasteriser remains the fallback. There is exactly ONE 16-bit
publication path — a rarely-taken CPU-truncation branch would be untestable here.

**Reviews.** An independent code review and the configured security/native-memory-lifetime review
(Opus 5.5 and Muse 1.3 respectively, no fallback) read the diff. Both found the same decisive
defect: on a `vkAllocateDescriptorSets` failure inside a pack batch, `vb_fail` does not clear
`g.ok`, so the batch still ran its barrier, submit, drain and publish and marked never-recorded
slices clean — stale bytes into guest memory, never retried. Fixed by a batch-local `batch_ok`
that skips the submit and the publish and leaves every target in the batch dirty; `g.ok`/`g.failed`
are deliberately not latched. Review nits fixed: `readback_batches` is now one per published batch
(it was `submits - subs0`, which also counted ring-advance submits), and the pack/copy counters
were moved from the record loop into the publish loop so they cannot report a batch that never
reached its submit — the `Nv2aVkReadbackStats` docs now say all of `readbacks`, `batches`,
`packed`, `packed_pixels`, `copied`, `bytes` describe PUBLISHED work and `wait_ns` is the
publication's own submit+wait. The security review separately verified the staging/guest
arithmetic (4-byte alignment of every slice and copy size, `rect_w*2+3` and `rect_w*4` cannot
overflow, growth doubles to the cap, guest writes bounded to each target's own rectangle, padding
untouched), the mapped-range invalidation, the pool budget, and every failure path (failed wait,
begin, submit, allocation, growth-vs-inflight) with no other defect.

**The deciding regression** (`jsrf_vk_readback_pack`, authored by a worker distinct from the
production change; CTest on lavapipe, no skip taken, 0.26 s in the full suite). It drives the real
back end over two frames and a mid-frame alias flush with seven targets: a 64×64 pitch-128 16-bit
target filled with a truncation-boundary colour plus five 8×8 blocks, an odd 21×13 dirty rectangle
at (3,5), a 16-bit target with four bytes of row padding per row, three 4-byte targets, and a
texture-aliased target published by the mid-frame flush. Every target's whole 32 KB region is
compared byte-for-byte against a plan built from the gate's own rectangles, with `0x5A` sentinels
proving padding, same-row neighbours and the row below stay untouched, and the CPU executor
replays the same pushbuffers as a second pass. Positive controls come from
`nv2a_vk_backend_readback_stats()` with the run's own expected values (`packed == 5`,
`packed_pixels == 8625`, `copied == 5`, `readbacks == 10`, `bytes == 47698`, `batches == 3`,
`frames == 2`), each with its own FAIL line; before any 16-bit publication `packed == copied == 0`.
The boundary constants are recomputed in the fixture (6 constants, 4 of them discriminate against
rounding; `080408` and `FFFFFF` are labelled shared-value controls). Mutants, each with the
compiled file's sha256: rounding pack (shader `b3dc786c…`) fails on a DIFFERING colour —
`T16A: (0,0) is 0x1044, the plan says 0x0823 (from 0xFF0F071F)`; a 16-bit target marked clean
without copying (`1613fdd0…`) fails every immediate byte check (`is 0x5A5A, the plan says
0x0823`); every target published from slice 0 (`e1e35887…`) fails the per-target checks
(`T16B: (3,5) is 0x0823, the plan says 0x1106`); 16-bit rows copied `rect_w*4` bytes (`5b17f802…`)
fails the sentinel checks (`byte 8192 is 0x06, the plan says 0x5A`). Baseline and restored rows
are green with identical sha256.

**The failure-path gate** (`jsrf_vk_readback_fault_gate`, a different worker again, 0.36 s): it
compiles the production bodies verbatim with stubbed Vulkan entry points (the
`jsrf_vk_ring_fault_gate` method, with a build-time probe that fails the BUILD if a definition,
field or constant it binds to moves: 45 of 45 anchors) and injects `VK_ERROR_OUT_OF_POOL_MEMORY`
from `vkAllocateDescriptorSets`. 7 properties, 138 checks: the success control publishes exactly
the two rectangles and counts them; a failure with the 16-bit target first and with the 4-byte
target first submits nothing, publishes nothing, leaves guest memory byte-identical (checksum
compared over 1 MiB) and both targets dirty, and latches nothing; the retry publishes both
rectangles correctly; the staging pair is not replaced or freed under a live submission. The
deciding mutant — the pre-fix plain `break` (`b66a8353…`) — fails 5 of 7 properties (49/138
checks) with `guest memory is byte-identical: nothing was published FAIL (496 of 1048576 bytes
differ)` and `both targets are still dirty FAIL (16-bit dirty 0, 4-byte dirty 0)`; a second
mutant that counts packs/bytes while recording (`296fa6bf…`) fails 3 of 7, which is what made the
counter placement decisive.

**N2 (three quiet 60 s Garage windows each, RTX 4070, same script/profile/settings).** Before, on
the accepted build exe `37d9c75e`: `20261016-220253-829`, `-221216-471`, `-222139-065` →
**45.20 presents/s median** (44.03–46.35), p50 21.76–22.22 ms, 0 stalls, 0 aliased polls. After,
on exe `328fe9bf`: `20261017-011458-923`, `-012421-508`, `-013344-192` → **44.72 presents/s
median** (44.28–45.08), p50 22.05–22.24 ms, 0 stalls, 0 aliased polls.

| run | flips | readbacks | packed | copied | publish ms/flip | wait ms/flip | flush ms/flip |
|---|---|---|---|---|---|---|---|
| before, diag `…-223221-992` | 27 335 | 54 669 | 0 (host loop) | 0 | 0.564 | 5.333 | 5.911 |
| after `…-011458-923` | 27 871 | 55 741 | 55 741 | 0 | 0.132 | 5.268 | 5.434 |
| after `…-012421-508` | 28 092 | 56 183 | 56 183 | 0 | 0.132 | 5.098 | 5.262 |
| after `…-013344-192` | 27 214 | 54 427 | 54 427 | 0 | 0.133 | 5.448 | 5.615 |

**N2 verdict: no measurable change — not a win and not a demonstrable regression.** Every after
run lies inside the before runs' own 44.03–46.35 spread, and the medians differ by −1.1 %, smaller
than that spread; p50 moved +0.4 %. The counters say the host conversion is gone (0.564 → 0.132 ms
per flip) and that the pack's added GPU work did NOT grow the drain (5.333 → 5.268/5.098/5.448 ms
per flip; a growth would have been reported as a regression). The honest reading is that the
0.43 ms/flip removed was absorbed by idle time in the frame loop, so it was not what bounded this
window's cadence — which also means the remaining drain will not move this metric by itself.

**Validation on the final source.** Full CTest through
`python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure`: **100 % of 90 tests passed**
in 490.63 s, including `jsrf_vk_readback_pack` (#15), `jsrf_vk_readback_fault_gate` (#21),
`jsrf_vk_frames_in_flight` (#14), `jsrf_vk_ring_fault_gate` (#20), `jsrf_gpu_backend_parity_vk`
(#13), `jsrf_vsh_aot` (#16) and the V6 conformance cases. `just check` passed, including the real
corpus (`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126
problems=0`) and its SPIR-V validation. The N3 bounded stock e2e on the final exe under the
Khronos validation layer: `20261017-015757-227-keyscript-n42-e2e-val2` (620 s bound, Vulkan,
exploratory, exe `328fe9bf`, game `479837a` + this change, toolkit `f41c0d2` + `891bca2`):
`keyscript_complete`, 0 ICALL lines, 0 ABI failures, 0 fatal, `validation_errors=0`,
`validation_warnings=10` (all `vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8
not consumed by vertex shader`, the same ten benign warnings V7 recorded), `vsh_misses=0`,
`vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`, `tex_undecodable=0`, 25 643 flips /
51 285 readbacks, all 51 285 packed on the GPU with 0 copied and 15.75e9 pixels, `rb_batches ==
flips`, 75 003 submissions (`splits=48 697`, `overflows=662`, `reuse_waits=662`,
`max_inflight=3`), `rb_publish_ms/flips = 0.135`. The overrides are recorded in the run's
`result.json` (`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, `VK_LOADER_DEBUG=layer`) and were
also exported in the invoking shell; the loader's own insertion lines, quoted from that run's log:

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

The game was stopped at the run's 620 s bound (`process.exit_code 1`,
`killed_at_deadline_or_failure: true`, `ran_s 620.4`): this is a bounded, deliberate termination,
not a normal game exit, and it supports no claim beyond the script completing and the counters
above. **Runner observation (recorded, not explained, as in §31.31):** the first attempt of this
same command (`20261017-014407-428-keyscript-n42-e2e-val`) also completed `keyscript_complete`
with 0 ICALL lines, 0 ABI failures, 0 fatal, `validation_errors=0` and the same ten warnings
(26 308 flips, 52 615 readbacks all packed, 76 987 submissions, `max_inflight=3`,
`rb_publish_ms/flips = 0.136`, `rb_wait_ms/flips = 6.551`, `vsh_misses=0`) — so the layer was
active — but its log carried no loader debug output; two 25 s probe runs in between did capture
158 loader lines each, so the difference is not the env override path. No mechanism was
established. Both N3 attempts are on record here — the rerun's own counters are the 25 643 /
51 285 / 0.135 set quoted above — and the acceptance witness is quoted from the rerun because it
is the one that carries the loader's own insertion lines.

**Pre-commit archives vs the commits (reconciled by measurement, not by assumption).** Every
after-bench and N3 archive records the tree state it ran on: `repository_identities.project.
patch_sha256 7691ee7d…` (the game-side gates, scripts and CMake registration) and `…toolkit.
patch_sha256 fa8110d4…`, and both archived patch files hash back to exactly those recorded values.
Measured against the commits: (1) the committed game diff for `CMakeLists.txt tests/ scripts/` is
**byte-identical** to the archived `project.patch` (59 100 bytes, newline-normalised); (2) the
committed toolkit diff `f41c0d2..891bca2` restricted to everything EXCEPT
`src/video/shaders/nv2a_vk_pack.hlsl` is **byte-identical** to the archived `toolkit.patch`
(45 343 bytes both) — the archived patch cannot contain that file at all, because the runner's
`git diff` never shows an untracked file and the archive's own `toolkit-status.txt` lists it as
`?? src/video/shaders/nv2a_vk_pack.hlsl`; (3) that file is cited separately by content, sha256
`a3e1abfb125470ad943a3e29d3c63097dc01caa5acdf94f2d5436382b467ce34` (3 892 bytes), identical in the
tree the runs used and in the commit. The records themselves (this section, L62 and the plan) are
additions on top of the exact trees those runs used, so the archives' recorded source state and the
commits differ only by those records.

**Limits.** No live GPU timeout or allocation failure was exercised: the fixed failure path is
decided by injected stub return values, not by a real OOM. The `>VB_READBACK_MAX` multi-batch
split is unreachable in a fixture (the largest possible single slice is a 4096×4096 4-byte target
= 64 MiB) and is therefore unexercised. The "no per-pixel CPU conversion" half is decided by the
back end's own accounting (publication-only counters) plus a source that no longer contains such a
loop; a back end that converted on the host *and* reported packs would be indistinguishable to a
black-box gate. The load direction is untouched: `vb_target_load` still expands 16→32 bits per
pixel into an upload stream slice, once per target per flip (`rt_loads` ≈ 2 per flip), and that is
a separate, unrequested item. The ring drain (~24 % of frame time) is not addressed here by design.
The Garage's 45 presents/s frame time did not move, so this change is justified by the removed host
work, the halved PCIe readback traffic and the bounded staging, not by a frame-rate result.

PUSHED_TO: origin / BRANCH: main / COMMIT: 891bca220666749fc94fc18c139d30dc97d77e6d / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward f41c0d2..891bca2, toolkit pushed first, synchronized.

### 31.33 N4.2 independent review: failed publication crosses its callers' coherence boundary (2026-10-17)

**Candidate rejected:** game `505f798` / toolkit `891bca2`, fetched clean and synchronized with
origin. The normal-path work and measurements in §31.32 remain candidate evidence. They do not
close the failure-path defect below. N4.1's acceptance remains intact.

`vb_targets_flush` skips a batch when its pack descriptor allocation fails, preserving dirty and
leaving `g.ok` true. Its caller `vb_flip` nevertheless submits/drains, marks every used target
stale and increments the frame. The next target load expands OLD guest bytes into the GPU image
whose newer bytes were never published. Source inspection also shows target replacement/eviction
flushes then releases unconditionally, and the overlapping texture path proceeds to read guest
memory after the unsuccessful flush. A dirty bit alone is not a coherent retry contract.

Codex independently compiled the production bodies through the existing injected-driver fixture,
with a private caller-level main invoking actual `vb_flip` and `vb_target_load` (outside both
repositories). Backend SHA-256 `d26a8ff9355eb5957d6c1959f14929374c35dce3b9b9bb829eb88a9788a0e772`.
Success control: exit 0, frame 1, `ok=1`, two readbacks, zero wrong guest bytes, target clean and
stale as expected. Injected `VK_ERROR_OUT_OF_POOL_MEMORY`: exit 1, frame 1, `ok=1`, zero
readbacks, **496 wrong guest bytes**, target dirty AND stale. Next load: `rt_loads=1`, dirty 1,
stale 0: old guest bytes are uploaded while the newer target remains unpublished. This proves
the CPU-side state/publication defect with a stubbed driver outcome, not a physical GPU OOM.
Private source/build and control/failure logs are under `C:/Users/logic/xbr-mac/n42-codex-boundary`.
The existing real readback and injected fault CTests independently pass **2/2 in 2.93 s** via
the limited-token helper; their manual-flush retry coverage misses the caller sequence.

**Evidence corrections.** The four original success-path mutation logs show red results, but
their `n42mut/run-mutant.sh` copies mutants into the shared toolkit and builds shared game/build,
contrary to the packet's private-scratch rule. The script pins backend `a39b1df4`, before final
`d26a8ff9`. Current trees are restored and clean; no Codex mutation touched production files.
Require private final-source baseline/red/restored proof in the repair. Also, a single target's
64 MiB maximum does not make a 96 MiB multi-target split unreachable: up to 16 dirty targets
contribute. That path remains unproven, rather than established unreachable.

**Codex advice:** make publication success explicit and propagate it through flip, target
replacement/eviction, aliases and CPU consumers. Recover within a bounded coherence boundary or
stop/propagate failure before stale reload, release, guest consumption or normal frame success.
Audit staging allocation failures and partial-command/staging lifetime as well as descriptor
failure. Do not accept preserving dirty alone or CPU fallback against stale guest bytes. A
different worker writes deciding caller-level fault/recovery tests and private final-source
mutants, keeping real exact-byte, ring and fault gates green. Guarded build, final CTest/check,
N2 as applicable and fresh N3 e2e with actual loader insertion witness remain required. Stop after
this repair for Codex acceptance; narrower barriers and every later item remain pending.

**Review-confirmed allowance renewal:** the installed skill's owner-authorized reset rule applies
after the independent reproduction and Codex advice above. Operation `op_0mvbxgq02bf74241d39516bf9`
in run `run_0mvbtl9id73ee084b8486253d` receives one rejecting STOP decision, closing that exhausted
repair allowance without inventing a human blocker. A linked successor carries only the **7
remaining operation slots** and receives **one renewed repair cycle assigned to its first packet**,
consumed on admission (zero additional unassigned cycles). Lineage spending before that packet is
13 operations / 4 repair cycles; after admission it is 14 / 5 with 6 operation slots remaining.
No old decision is rewritten, no unchanged failure is retried in a loop, and rotation itself
grants no budget. All owner stop conditions and Windows toolkit-first push rules remain.

### §31.34 N4.2 repair: a failed publication stops before it crosses a coherence boundary (2026-10-17)

Toolkit `4832f62` (the repair) on top of `891bca2`; the game-side gates at `70ecf0f` on top of
`78cbb39`, and these records in the commit after it. The candidate `891bca2` / `505f798` stays
REJECTED (§31.33): its normal-path measurements and its tests remain candidate evidence, and nothing
here reopens them.

**Hash convention.** Working-tree file hashes below are CRLF, as checked out on Windows; `git show
<rev>:<path>` blob hashes are LF. The pre-fix backend source is therefore `d26a8ff9…` as a
working-tree file and `1d556fa8…` as the LF blob the private overlays compiled.

**The defect.** `vb_targets_flush` skipped a batch whose pack descriptor set could not be allocated,
left its targets dirty and left `g.ok` true. Its callers assumed the batch had been published:
`vb_flip` submitted, drained, marked EVERY used target stale and advanced `g.frame`, so the next
`vb_target_load` uploaded the OLD guest bytes over the newer image that had never been published;
`vb_target_for` flushed and then released a reshaped or LRU-evicted target unconditionally, dropping
the only copy of pixels guest memory never got; `vb_texture`'s overlapping-guest-texture loop flushed
and then decoded and cached a texture from guest bytes a live target had superseded. Latching
`g.ok = 0` and letting the executor's CPU path carry on is not a repair either: it draws a frame
nothing published on top of the last frame the back end did publish.

**The repair.** Publication is now explicit at every coherence boundary. `vb_targets_flush` returns
whether any target in its list is left dirty, with its failure behaviour otherwise unchanged (an
abandoned batch submits nothing, publishes nothing, latches nothing, and leaves every publication
counter unchanged) — that is what keeps the accepted fault gate's expectations valid. A boundary —
the flip, a target release/reshape, an overlapping guest-memory texture — calls it through
`vb_targets_flush_recover`, which on failure refreshes the ring (`vb_slot_submit()` then `vb_drain()`,
resubmitting the abandoned batch's staging-only recording) and retries EXACTLY ONCE; a target still
dirty after the retry is counted as `readback_incomplete` and the boundary stops the back end
(`vb_stop`: one message line, then the `[GPUVK]` stats line carrying the real `rb_incomplete`, with
`g.ok = 0` and `g.failed = 1` latched and nothing freed, reset, submitted or destroyed). So:

- on the flip's stop path nothing is marked stale and `g.frame` does not advance, so no later load can
  upload old guest bytes over an unpublished newer image, and a second flip publishes nothing;
- `vb_target_for` returns NULL without releasing, reshaping or creating when its target cannot be
  published, so the newer image survives;
- `vb_texture` returns `VK_NULL_HANDLE` instead of decoding superseded guest bytes;
- staging-pair failures (`vb_readback_reserve`) are covered exactly like descriptor-set failures;
- the retry can never publish twice (an abandoned batch published nothing, and a published target is
  clean and skipped), never loops and never re-enters.

**CPU consumers.** `Nv2aBackend` gains a trailing optional `int (*stopped)(void)`; the Vulkan back end
reports `g.failed || !g.ok` and D3D11 passes NULL. While it reports stopped the executor performs no
clear (`clear_surface`), no CPU rasterisation (`raster_batch`, `raster_batch_program`) and no present,
present-hash trace or frame dump (`NV097_FLIP_STALL`), because guest memory holds the last frame the
back end published and its unpublished frame supersedes anything the CPU would write. The
guest-visible swap accounting still runs (`flip_read = flip_write`, `flip_stalls++`,
`xbox_Nv2aFrameCounterFlip()`, the host-side window frame statistics), so no guest path hangs.

**Unchanged.** The `vb_reserve`/`vb_stream` freeze-on-failed-wait/begin/submit latch and its "nothing
pending is written, reset or freed" rule; the exact 5:6:5 TRUNCATION; BGRA32; row stride, pitch and
padding; odd rectangles and sentinels; multiple targets and immediate guest publication; the bounded
staging pair and the three-slot ring. No shader is compiled or translated at run time. No narrower
barrier/layout work, sub-allocation, pipeline cache/R4, N6, R5 or D3D11 work. No milestone semantics
changed.

**The deciding caller gate** (`jsrf_vk_readback_callers`): `tests/test_vk_readback_callers.c`
(`231d73ae…`) with `scripts/vk-readback-caller-gate.py` and its CMake block, authored by a worker
distinct from the production change. It `#include`s the toolkit's `src/video/nv2a_vk_backend.c`
verbatim, points every Vulkan entry point the driven paths reach at its own stub, and drives only the
callers (`vb_flip`, `vb_target_load`, `vb_target_for`, `vb_texture`), observing `g.frame`, `g.ok`,
`g.failed`, the targets' `dirty`/`stale`, the publication counters, the recorded command buffers and
the guest bytes. **On the final source `0d6ac8b5…`: 15 properties, 297 checks, green. On the pre-fix
source (`1d556fa8…`): RED — 12 of 15 properties, 114 of 297 checks**, with the defect quoted
(`g.frame 1 (wanted 0)`; `dirty 1, stale 1 (wanted 0)`; one `vkAllocateDescriptorSets` attempt and no
retry; the next load recording `vkCmdCopyBufferToImage`, `rt_loads 0 -> 1`, and the bytes it uploaded
identical to the pre-publication image). Property 0 and the success control pass on both sources, so
a red run is the defect and not the fixture. The injections are stub return values
(`VK_ERROR_OUT_OF_POOL_MEMORY` from `vkAllocateDescriptorSets`, `VK_ERROR_OUT_OF_DEVICE_MEMORY` from
`vkCreateBuffer`), never real GPU OOM.

**The multi-batch split: corrected and covered.** The candidate's statement that the
`>VB_READBACK_MAX` split is unreachable because one target is at most 64 MiB was wrong — one target is
bounded by `vb_target_for`'s 4096-per-dimension cap, but up to 16 dirty targets contribute to a group.
The gate now covers it through a labelled private capacity seam (`GATE_GUEST` 1 MiB → 160 MiB,
`GATE_STAGE` 1 MiB → 96 MiB): property 12 — two 64 MiB dirty 4-byte rectangles (128 MiB, disjoint
footprints) split into two batches that each publish exactly once (2 begins/2 submits/2 waits/2
invalidates; `readbacks`, `batches` and `copies` +2; `bytes` +134 217 728; 0 of 167 772 160 guest bytes
differ; 0 bytes written outside the rectangles; `incomplete` +0; `g.frame` +1), the staging pair
grown once to 64 MiB and reused by both batches (`vkCreateBuffer 2`, `vkMapMemory 1`, no destroy, no
free), with the call trace placing batch 1's publish before batch 2's recording; property 12b — the
same split with `vkAllocateDescriptorSets` failing from the second call on: batch 1's rectangle lands
exactly, the second target stays dirty, `incomplete` +1, `g.frame` 0, nothing stale, nothing released,
the retry visible as three allocation attempts; property 13 — a single over-cap dirty target (a
synthetic 8192×8192 4-byte rectangle, which `vb_target_for` could never produce, at an address outside
the guest arena) is never staged: the flip publishes the other target exactly, leaves the synthetic one
dirty with its image and rectangle intact, counts it `incomplete` +1 and stops. The twelve
pre-existing properties keep identical verdicts and check counts.

**Executor CPU-consumer gate** (`jsrf_nv2a_hal`, new case in `tests/test_nv2a_hal.c`, `ee379459…`, a
third worker): a back end whose `stopped` reports 1 must leave the 64×64 surface byte-identical, count
no clear, rasterise no triangle, write no pixel and present no frame — while the guest's swap still
completes (`flip_stalls` +1) — for both a back end that handles nothing (the CPU fallback) and one
that handles everything; its `stopped = 0` controls must clear, rasterise, count, deliver and present.
Removing the executor's four gate sites in a private toolkit copy turns it RED with exactly nine
failing assertions while every control stays green.

**Private final-source mutation receipts** (`C:/Users/logic/xbr-mac/n42fix/mutants/matrix.tsv`, 11 rows,
every row carrying `revision_sha256 = 0d6ac8b5…`; one mutant at a time against a private copy, never
the shared tree): A1 outcome never reported, A2 unsafe flip, A3 unsafe replacement, A4 unsafe
consumer, A5 no retry/no accounting, B1 skipped publication, B2 wrong slice, B3 overwide rows, B5 the
group never splits, B6 the staging pair re-made per batch — each RED on exactly the property its defect
breaks, with every restored row exit 0 at the same sha256. B4 rounding pack is invisible to the caller
gate (which supplies the staging bytes a copy engine or the pack kernel would have written), so it was
decided by the real-device `jsrf_vk_readback_pack` case in a private build whose generated pack SPIR-V
header was replaced: mutant shader `5555bd35…` → header `857916b9…` → `T16A 3904`, `T16B 273`,
`T16P 4096` of 32 768 bytes wrong, `T16A: (0,0) is 0x1044, the plan says 0x0823`; restored header
`e75bc7af…` → PASS.

**Process correction, restated.** The candidate's four success-path mutants were produced by copying
them into the SHARED toolkit and building the shared `game/build` against backend `a39b1df4`
(`xbr-mac/n42mut/run-mutant.sh`). That violates the private-scratch rule and is not relied on; those
same four mutations were re-run from the final source in a private overlay (B1 skipped publication, B2
wrong slice, B3 overwide rows and B4 the rounding pack), and that is what the matrix above records —
every row is a private-overlay run against `0d6ac8b5…`.

**Reviews.** An independent code review and the configured security/native-memory-lifetime review read
the diff. Findings and dispositions: `rb_incomplete` could never reach a `[GPUVK]` line, because every
flip after a stop returns before the report (fixed: `vb_stop` prints the stats line itself); one
comment misnamed which trace stops (fixed); the periodic report's `dump_surface_bmp()` still reads
guest memory after a stop (comment added — it is a diagnostic dump of the last published frame, not a
presentation); and one hardening note — re-registering a back end after a stop would clear the
executor's guard, which has no production trigger (`nv2a_vk_backend_install` registers once and is
idempotent, and the only other `nv2a_backend_register` callers are test fixtures) and is therefore
recorded as a deliberate non-change rather than a new executor-side latch that would change the
registration semantics the fixtures rely on.

**Validation on the final source.** Guarded build `python -X utf8 scripts/build-jsrf.py` → success;
executable `ac14cce325a87d64870b220ea159d0bea6039eb0cdfd93c8369ff8ee17f54637`; backend `0d6ac8b5…`;
executor `45122e34…`. Targeted gates, all green: the caller gate (15 properties/297 checks; 7.39 s in
the shared suite and 23.55 s in its private harness), `jsrf_vk_readback_fault_gate` (7 properties, 138
checks, expectations unchanged), `jsrf_vk_readback_pack` (real device, lavapipe),
`jsrf_vk_ring_fault_gate`, `jsrf_vk_frames_in_flight` (19.20 s), `jsrf_gpu_backend_parity_vk` (14.77 s),
the V6 conformance cases and the whole AOT suite (16/16, 113.05 s). Full CTest through
`python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure` on the tree that carries these
records: **100 % of 91 tests passed** in 439.53 s, receipt
`C:/Users/logic/xbr-mac/final-ctest-n42r3.txt` (the earlier `final-ctest-n42r.txt` and
`final-ctest-n42r2.txt` runs passed 91/91 on the candidate and pre-record source/test bytes and are kept
as intermediate evidence); `just check` passed on that same frozen tree, receipt
`C:/Users/logic/xbr-mac/final-check-n42r3.txt`, including the real corpus
`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126 problems=0` and its
SPIR-V validation (the earlier `final-check-n42r*.txt` receipts predate the final record wording). No
Skip77/NotRun anywhere, and no unchanged translator output was regenerated.

**N3 bounded e2e on the final source** (`20261017-035814-457-keyscript-n42r-e2e-val`, 620 s bound,
Vulkan, exploratory, exe `ac14cce3…`, backend `0d6ac8b5…`, toolkit `4832f62`, device RTX 4070): the
stock `config/key-scripts/e2e.json` completed (`keyscript_complete`), 0 ICALL lines, 0 ABI failures,
0 fatal, `validation_errors=0`, `validation_warnings=10` (all the known
`vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8 not consumed by vertex shader`),
`vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`, `tex_undecodable=0`,
31 522 flips / 31 522 published batches, 63 043 packed readbacks (all 16-bit: `rb_copies=0`, so this
run does NOT exercise the 4-byte straight-copy publication path — the caller gate's 4-byte targets and
the pack gate's 32-bit targets do), `rb_incomplete=0`, 2 910 178 draws, 31 786 presents. Both overrides
were set AND exported in the invoking shell and are recorded in the run's `result.json`
(`['RECOMP_GPU_BACKEND=vulkan', 'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_LOADER_DEBUG=layer']`);
the loader's own insertion lines, quoted from that run's log (158 loader lines):

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

Disclosed as environment noise, not an override this packet set: the same log's loader output also
lists an inserted `VK_LAYER_OBS_HOOK` instance layer
(`C:\ProgramData\obs-studio-hook\graphics-hook64.dll`), a third-party implicit layer present on this
host. The game was stopped at the run's 620 s bound (`process.exit_code 1`,
`killed_at_deadline_or_failure: true`, `ran_s 620.4`): a bounded, deliberate termination, not a normal
game exit, and it supports no claim beyond the script completing and the counters above. No fidelity
claim is made.

**N2 (three quiet 60 s Garage windows on the RTX 4070, `config/key-scripts/garage-bench.json`, same
runner and instrumentation).** After, on the repaired build exe `ac14cce3…` (Vulkan, exploratory):
`20261017-041027-143-keyscript-n42r-bench-1` **43.08** presents/s, `-041949-827-…-2` **43.87**,
`-042912-299-…-3` **44.50** → **median 43.87** (range 43.08–44.50), p50 22.12–23.37 ms, p90
24.19–24.45, p99 26.35–29.99, 0 stalls and 0 aliased polls in all three. Before: the **archived**
candidate set of §31.32 on exe `328fe9bf` — not a fresh rebuild — 44.72 median (44.28–45.08), p50
22.05–22.24, 0 stalls, 0 aliased polls. The repair's only normal-path addition is an O(1) `stopped`
predicate at each clear/batch entry and each flip (everything else is failure-path only). The after
median is 1.9 % below the before median and after run 1 (43.08) sits below the before set's floor
(44.28); the two sets are not interleaved — the before set was taken earlier the same day — so no
cause is inferred, and an interleaved control would need a rebuild of the candidate and was not run.
**No performance claim is made in either direction**: no counter attributes a cost to the added
predicate, and p50/p90/p99, the zero stalls and the zero aliased polls are close between the sets.

**Limits.** The injected failures are stubbed driver return values, never a real GPU OOM or a real
device loss. The caller gate supplies the staging bytes a copy engine or the pack kernel would have
written, so shader arithmetic is decided only by the real-device pack case. After a stop nothing drains:
the ring's last partial recording is deliberately neither submitted nor drained, and the dead command
buffer may reference objects later freed (never executed, validation-only surface). The periodic
report's `dump_surface_bmp()` and the framebuffer window's `RECOMP_FB_VA` pinned path still read guest
memory on their own clock by design (diagnostic, not presentation). `nv2a_pb_exec_counters.draws` still
advances for a stopped batch because the dispatch is counted before the gate; `tris_drawn`, `pixels`
and `clears` do not. The load direction (16→32-bit expansion per target per flip) and the ring drain
are untouched and were not re-measured.

PUSHED_TO: origin / BRANCH: main / COMMIT: 4832f6247fc9d1de56341f6bcb876125a40c8b37 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 891bca2..4832f62, toolkit pushed first, synchronized.

### 31.35 N4.2 caller repair verified; abandoned-batch staging writes still unordered (2026-10-17)

**Independent review rejects N4.2 pending one ordering repair.** Fetched game `fe09e5e` (gates
`70ecf0f`) and toolkit `4832f62`, clean and synchronized. The prior caller defect IS repaired:
the original private reproduction rebuilt against final backend
`0d6ac8b54f73f160aaba980228a27f912d959dbfba03298337ed09155936c11d` now stops after two injected
allocation failures with frame 0, `ok=0`, dirty retained and stale 0; next load makes no upload.
The 496 unpublished bytes are not presented as a new frame. Independent limited-token CTests:
five Vulkan tests **5/5, 27.56 s**, executor **1/1, 0.03 s**. Inspected final full receipt
`final-ctest-n42r3.txt`: **91/91, 439.53 s**. Private caller baseline is red 12 properties /
114 of 297 checks; repaired green 15 properties / 297 checks. Unsafe-flip mutant A2 is red
4 properties / 29 checks, restored green. Actual N3 archive `20261017-035814-457-keyscript-n42r-e2e-val`
contains Khronos insertion at log lines 857/1288, implicit OBS hook 877/1298, final errors 0,
known warnings 10, program misses 0 and incomplete 0. Those passes do not cover the defect below.

**Remaining defect:** `vb_targets_flush` leaves already-recorded copies in its command buffer
after `!batch_ok`, then continues to a later group whose slices start again at 0. If that next
group begins with a 4-byte copy, `vb_transfer_cmd`/`vb_cmd` introduce no ordering while already
recording. The destination write ranges overlap without an intervening memory dependency; the
final host-read barrier follows both and cannot order the writes relative to one another.
Recovery after the flush returns is too late. The code review noticed this as "Outside this
patch"; it is still in N4.2's implementation and blocks acceptance. Vulkan requires a memory
dependency between write-after-write accesses (official specification, Synchronization and Cache
Control: https://docs.vulkan.org/spec/latest/chapters/synchronization.html).

**Independent deciding experiment:** private `C:/Users/logic/xbr-mac/n42-codex-ordering` includes
actual production bodies through the caller fixture, records real `vb_flip` commands through
controlled Vulkan pointers, and tracks overlapping transfer destinations until a relevant
memory dependency or completed fence. Shapes are within production limits: 4096x4096 BGRA32
(64 MiB), 4096x4096 RGB565 (32 MiB), then 64x64 BGRA32 (16 KiB), disjoint fixture guest ranges.
The first batch fills 96 MiB. Its pack allocation fails once AFTER the first copy was recorded.

- Normal control exit 0: two image copies, zero hazards, three published targets, frame 1.
- Failure exit 1: same command buffer/destination, earlier range `[0,67108864)`, next
  `[0,16384)`, one descriptor failure, **zero submits, zero waits**, no dependency between writes.
  Three targets eventually publish and frame advances, `rb_incomplete=0`; counters miss the race.
- Private control adds `vb_barrier()` only before the abandoned-batch continuation: same injected
  failure exit 0, zero hazards, all targets publish. Control backend SHA-256
  `77ea858c8381b43f14224c9b83f9928a5da30b578767fd9e2a30e40a5813830c`.

This is a deterministic command-order proof, not an observation of physical GPU corruption or a
validation-layer error. No shared source/executable was changed. The earlier split gate fails
the SECOND batch after the first succeeds, and does not decide this reversed failure sequence.

**Codex advice and scope:** order partial staging writes before any subsequent overlapping reuse,
or leave the flush and perform bounded recovery before advancing. Audit host staging and device
pack-buffer dependencies and slot changes. Use a different worker's behavioral range/mask/order
gate with success and first-batch-failure controls, baseline red / private mutant red / restored
green, retaining the existing caller/CPU/exact-byte/ring/fault/parity/AOT gates and final build,
CTest/check and N3 witness. This is a correctness barrier, not the later narrower-barrier item.
Correct the broad "last frame" wording: earlier batches can publish before a later failure;
the stop preserves unpublished targets but does not provide whole-frame atomicity.

**Renewal:** the previous defect is fixed and this newly confirmed dependency gap has a passing
private repair control, so there is meaningful progress rather than an unchanged-failure loop.
Owner's review-confirmed reset grants one bounded repair cycle assigned to the next successor's
first packet, zero additional unassigned cycles, carrying only **6 remaining operation slots**.
Exactly one rejecting STOP decision closes completed operation `op_0mvc7gaus816d239ce8983763` in
run `run_0mvc7ejigbd9edeee476a0012`. Before admission lineage is 14 operations / 5 repair cycles;
after admission 15 / 6, with 5 operation slots left. Preserve prior decisions and owner stop
conditions. Stop after this repair for Codex acceptance; do not start a later N4 item.
PUSHED_TO: origin / BRANCH: master / COMMIT: 70ecf0f (the gates) together with fe09e5ee944a9ae4963a7a6f1d8ddfa39bfe3670 (the §31.34 records) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 78cbb39..fe09e5e, game pushed after the toolkit, synchronized at that checkpoint.

### §31.36 N4.2 ordering repair: an abandoned batch's staging writes are ordered before their reuse (2026-10-17)

**Repair recorded; N4.2 itself stays REJECTED pending independent acceptance.** §31.32's candidate and
§31.34's caller repair remain candidate evidence; §31.35's finding is answered here, and no later N4
item was opened. One added line of executable code in the whole repair, in
`src/video/nv2a_vk_backend.c` inside `vb_targets_flush`'s `if (!batch_ok)` branch, immediately before
`i = j; continue;`: `vb_barrier();` — the file's deliberately broad barrier
(`VK_PIPELINE_STAGE_ALL_COMMANDS_BIT -> VK_PIPELINE_STAGE_ALL_COMMANDS_BIT`,
`VK_ACCESS_MEMORY_WRITE_BIT -> VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT`).

**The defect it answers (from §31.35).** A group whose pack descriptor set could not be allocated left
its already-recorded copies in the open recording and the flush continued to the next group, whose
slice offsets start at 0 again: a 4-byte target's `vkCmdCopyImageToBuffer` — or a 16-bit target's
pack-output `vkCmdCopyBuffer` — wrote the same `g.rb_buf` bytes with no memory dependency between the
two writes, because the batch's own HOST_READ barrier is recorded after BOTH and so cannot order them
against each other. Vulkan requires a memory dependency between write-after-write accesses
(synchronization chapter, https://docs.vulkan.org/spec/latest/chapters/synchronization.html).

**Repair scope.** The added barrier is reached only with `g.ok == 1` (`vb_fail`, which the failed
allocation calls, only counts and prints) and outside any pass, so it records into the live command
buffer of the current slot, not the dead one. It orders every write the abandoned group had recorded —
the 4-byte image copies and the pack path's output copies, on the host-visible slice buffer and the
device-local pack buffer alike — before anything recorded after it, and its stage scopes cover the
pack buffer's read-before-a-later-write class as well. Nothing else changed: no counter, retry, stop,
publication, slot-transition or deferred-ownership semantics, and no success path gained a barrier. No
narrower/targeted barrier was introduced; that remains N4's separate open item. The same pass qualified
the over-broad "guest memory keeps its last published frame" wording in `nv2a_vk_backend.c` (file
header, `readback_incomplete`, the recover preamble, `vb_stop` and its message, `vb_stopped`, and the
two stop messages), `nv2a_vk_backend.h`, `src/kernel/nv2a_backend.h` (`Nv2aBackend.stopped`) and
`src/kernel/nv2a_pb_exec.c`: a flush publishes batch by batch, so earlier batches can already be in
guest memory when a later one fails — the stop preserves the UNPUBLISHED targets, leaves each target
rectangle at the last bytes published for it, and is NOT whole-frame atomicity. No whole-frame
transaction was added and no milestone semantics changed. Identities: backend working-tree sha256
`c710be7bf99a721b6abbf275d549b230da6e7b4b0e48ab833a06663346e9c528` (CRLF; the gate compiles these
bytes), committed blob sha256 `6cae1f824e0f1acb6fdc52f0b27171ccf484881ed4631d67a9b0c5c806abc1cc`
(LF); `nv2a_vk_backend.h` `75f893e8…`/blob `82fd8a21…`; `nv2a_backend.h` `95116006…`;
`nv2a_pb_exec.c` `7694e6bb…`; toolkit commit `cd638d42821c74ee4245ed6c98ba896e5461f3f7`.

**Deciding gate, written by a different worker.** `jsrf_vk_readback_ordering_gate`
(`tests/test_vk_readback_ordering_gate.c` sha256 `104b0573…`, `scripts/vk-readback-ordering-gate.py`
sha256 `9559c0c5…`, one added CMake block, +96 lines) compiles the production backend verbatim
(`XBR_VK_BACKEND` + `#include "nv2a_vk_backend.c"`), stubs every Vulkan entry point as a recording
function pointer, needs no ICD and has no skip path, and fails the BUILD when a seam it binds to moved
(build-time probe, 69 anchors; the repair's own seam is a printed marker, so the pre-fix source still
builds). It records every write, read, barrier, submit and fence wait with a sequence number and
decides, for every pair of overlapping accesses to the same buffer, whether a barrier strictly between
them — or, across recordings, a completed fence wait — orders the later one, quoting stages, access
masks, destination ranges, buffer handles, command buffers and slot/fence state. Write-after-write
needs the barrier's second access scope to cover the later write; write-after-read needs only the stage
scopes, an execution dependency. Fixtures use production-legal dimensions (4096x4096 BGRA32 64 MiB,
4096x4096 RGB565 32 MiB packed to 32 MiB, 4096x2048, 128x64, 64x64) chosen so that a group's slices
fill the 96 MiB `VB_READBACK_MAX` exactly and the next target starts a second group at offset 0 again;
the pack failure is injected through the stub's `vkAllocateDescriptorSets` return value, after the
first copy was recorded, not by real pool exhaustion. Fourteen verdicts, 275 checks, GREEN on the
repaired source; the PRE-FIX baseline is RED on 10 properties, 23 of 275 checks, quoting
`UNORDERED #2/vkCmdCopyImageToBuffer and #4/vkCmdCopyImageToBuffer in recording 1: no barrier strictly
between them orders the later write after the earlier one` and
`WAR UNORDERED read #4/vkCmdCopyBuffer (pack output read) and write #9/vkCmdDispatch …`, with the
probe's abandonment-barrier marker reading `abandonment barrier 0`. Two private weakening mutants of
the FINAL source are RED — the added call replaced by a HOST_READ-only barrier (10 properties, 21
checks: the destination stage cannot cover the later writer, and the destination access cannot cover a
write) and the call deleted (baseline's set) — and the restored source is GREEN at the identical
sha256. Private receipts: `C:/Users/logic/xbr-mac/n42order/receipts/matrix.tsv` and the per-run logs;
no shared source, build tree or executable was mutated for any mutant.

**Independent reviews (two agents, distinct from the production and gate workers).** The code review
found no blocking defect — the barrier is recorded into the live buffer, correctly placed between the
abandoned writes and everything after them, and sufficient under Vulkan's WAW rules — and raised four
non-blocking weaknesses, all resolved or recorded: the gate's ordering predicate accepted any write bit
in the destination access (it now requires the barrier's second access scope to cover the later
access); two hazard classes were undecided (the pack buffer's read-before-later-write class is now
recorded and decided by a third scenario, and the pack buffer's write-against-write reuse is ordered by
the pack path's own barrier by construction and is quoted as a control); a stale comment in the probe
script; and a receipts line count. It also stated, as asked, that cross-recording ordering in the gate
is deliberately stricter than Vulkan (it requires a completed fence wait), so the gate decides this
implementation and fails safe. The security/native-memory-lifetime review found no new hazard and no
lifetime change: the added barrier allocates, frees, resets, submits and waits for nothing; an
abandoned or stopped recording is intentionally not submitted, reset or freed, and every post-stop
free path is unreachable (so the direction is leak, never use-after-free); the changed stderr strings
carry no format-string or injection risk. It recorded one pre-existing, out-of-scope
defence-in-depth gap — the publish `memcpy` has no guest-RAM bounds check — which this repair neither
widens nor narrows; see Limits.

**Gates (all on the frozen tree).** Guarded build `python -X utf8 scripts/build-jsrf.py` succeeded with
the executable `46c4ffe9d98ba876a68b7fba43710dee99263c381e39e1caeecac8f69a786009`
(`receipts/build-final.txt`). Full CTest through
`python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure` — the elevated-session route the
software-ICD tests need — **92/92 in 441.05 s** (`receipts/final-ctest-n42o.txt`), including the new
`jsrf_vk_readback_ordering_gate` (2.82 s) and every retained gate: `jsrf_vk_readback_fault_gate` with
unchanged expectations, `jsrf_vk_readback_callers` (15 properties / 297 checks),
`jsrf_vk_ring_fault_gate`, `jsrf_nv2a_hal`, `jsrf_vk_readback_pack` (real device),
`jsrf_vk_frames_in_flight` (17.40 s), `jsrf_gpu_backend_parity_vk` (14.90 s), the V6 conformance set and
the AOT/provenance set. No `Skip77` and no NotRun anywhere, and no unchanged translator output was
regenerated. `just check` passed with the real retail corpus unchanged
(`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126 problems=0`,
`receipts/final-check-n42o.txt`).

**N3 witness.** Run `20261017-065204-433-keyscript-n42o-final-e2e` — stock `config/key-scripts/e2e.json`,
620 s bound, Vulkan back end, exploratory profile, exe `46c4ffe9…` (identical to the built and pushed
bytes), device RTX 4070, isolated disposable save root. The run started from a CLEAN tree: its archived
`project.patch` and `project-status.txt` are both empty and `project_archived` is true, so the archive
pins the exact repository state. `VK_INSTANCE_LAYERS` and `VK_LOADER_DEBUG` were set AND exported in the
invoking shell and also passed to the runner; the run's `result.json` records the overrides
`['RECOMP_GPU_BACKEND=vulkan', 'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation',
'VK_LOADER_DEBUG=layer']`. The loader's own insertion lines, quoted from that run's `jsrf_run.log`
(57 297 lines):

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

Disclosed, not set by this packet: the same log lists an inserted `VK_LAYER_OBS_HOOK` instance and
device layer (`C:\ProgramData\obs-studio-hook\graphics-hook64.dll`) — a third-party implicit layer
present on this host. Outcome `keyscript_complete`; 0 ICALL lines, 0 ABI failures, 0 fatal, 0
`still_rejecting`; `validation_errors=0` and `validation_warnings=10` (all the known
`vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8 not consumed by vertex shader`);
`vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`, `tex_undecodable=0`,
`rt_copies=0`, `rb_copies=0`, `rb_incomplete=0`; 26 298 flips, 26 517 presents, 2 976 248 draws, 52 595
published readbacks, 76 972 submissions, max 3 in flight. Termination was bounded and deliberate —
`killed_at_deadline_or_failure: true`, `exit_code 1`, `ran_s 620.6` — not a normal game exit, and this
is a loader-witnessed normal-path run, not a fidelity proof.

**Limits.** The injected failures are stubbed driver return values, never real pool exhaustion or a
real device loss. The gate decides the RECORDED command stream; its staging bytes are supplied by the
fixture at each batch's fence wait, so it decides ordering, ranges, masks and slot/fence state, not the
bytes a real copy engine or the pack kernel produces (shader arithmetic is decided by the real-device
`jsrf_vk_readback_pack` case). Cross-recording ordering in the gate is stricter than Vulkan, as above.
This e2e's readbacks were all 16-bit (`rb_copies=0`), so it does not exercise the 4-byte straight-copy
publication path the deciding gate covers with `vkCmdCopyImageToBuffer`. A medium, pre-existing
defence-in-depth gap remains open and out of this packet's scope: the publish `memcpy` writes to
`vb_guest(t->va)` plus rectangle offsets with no guest-RAM bounds check and no cross-check that the
rectangle lies inside the surface, so a hostile pushbuffer crafting pitch/VA/clip could drive a
successful publish outside the intended surface; the repair adds no validation and changes nothing
there. The security review's one low nit is a fail-closed message truncation in the gate's own fixture.
`vb_frames_in_flight`/parity timings and the 43.87 vs 44.72 presents/s comparison of §31.34 are
measurements of their own commits: this repair changes one failure-path barrier, keeps the same
executable bytes on every success path and makes no new performance claim. Models and fallback chains
were not changed and no fallback was observable in any transcript; `.omp/config.yml` is untouched.

PUSHED_TO: origin / BRANCH: main / COMMIT: cd638d42821c74ee4245ed6c98ba896e5461f3f7 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 4832f62..cd638d4, synchronized (origin/main == cd638d4).
PUSHED_TO: origin / BRANCH: master / COMMIT: acf20ff74dc58fa757de1ff716c0cd93f4e2e431 (the ordering gate) and 801df8b8f0374adc6d11119d21df7b77b4935a2d (the §31.36 records) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward d520693..acf20ff..801df8b, game pushed after toolkit, clean synchronized at the independently fetched checkpoint.

### 31.37 N4.2 independently accepted; next narrower barriers (2026-10-17)

**Accepted:** toolkit `cd638d4`, game gate `acf20ff` / records `801df8b`, fetched clean and
synchronized. Both confirmed defects are closed; preserve §§31.33 and 31.35 as review history.
The final production diff adds one executable `vb_barrier()` on the allocation-failure branch,
before staging offsets can be reused. Other changes qualify per-target publication in comments
and diagnostics. This changes no successful-path command or byte-producing code, and claims no
new timing result. It is a correctness barrier; N4's narrower-barrier optimization remains next.

**Independent deciding checks:** Codex rebuilt its original private command-order reproduction
against final backend SHA-256 `c710be7bf99a721b6abbf275d549b230da6e7b4b0e48ab833a06663346e9c528`:
the injected first-batch failure now exits 0 with zero unordered overlaps, three image copies,
three published targets and frame 1. No shared source or owner executable was mutated. Independent
limited-token CTests: **6/6 in 29.68 s**, including real pack, lifetime, ring failure, readback
failure, caller and new ordering gates. Codex also re-ran private baseline / mis-scoped barrier /
deleted barrier / repaired binaries: **1 / 1 / 1 / 0 exit codes**. Baseline and deleted each
fail 10 properties / 23 checks; mis-scoped fails 10 / 21. Each red run has **6 of 27 WAW pairs
and 3 of 6 WAR pairs unordered**; final green has **0 of 27 and 0 of 6**, 14 properties / 275
checks. Inspected stage/access/range/sequence predicates, so the mis-scoped barrier fails despite
being present. Final private repaired backend equals production byte for byte. New test hash
`104b05735d364abb6e25d7a52dd0c111712dca38b4353664992687d1d84f22b6`.

**Receipts independently inspected:** full final **92/92, 441.05 s**, final checker and
post-record doc-sensitive CTests; no redundant full suite run was needed after the focused
independent checks. Distinct production/test/reviewer assignments and review fixes verified from
transcripts; per-agent model fields are unavailable, so routing claims remain configuration-derived.
The gate's cross-recording check is intentionally stricter than general Vulkan, a disclosed
coverage restriction rather than a false pass. Prior private format-mutation evidence applies to
the unchanged byte-producing paths; no claim those older rows were rerun on the one-line fix.

**N3 archive verified:** `20261017-065204-433-keyscript-n42o-final-e2e` reclassifies exploratory,
zero UNKNOWN/MISSING, stock script complete. Actual Khronos instance/device insertion lines
857/1288; implicit OBS hook at 877/1298 disclosed. Errors 0, known warnings 10, program misses /
identity failures / declines / fallback / incomplete 0. Archived backend/executor hashes match
current code and executable `46c4ffe9d98ba876a68b7fba43710dee99263c381e39e1caeecac8f69a786009`.
Clean project archive pins `acf20ff`; later `801df8b` is docs only. Exit 1 at deliberate 620.6 s
deadline is not normal exit or fidelity proof. Prior 43.87 / 44.72 N2 medians remain inconclusive
measurements of their own commits, with no new performance claim for this error-only repair.

**Audit and limits:** Codex independently scanned all changed blobs in each correct object store:
toolkit 4 / game 7, zero secret hits, forbidden paths or oversized blobs; intended origin remotes,
unsigned commits without co-author/signature trailers, upstream push DISABLED. Per-target
publication is not whole-frame atomicity; stubbed failures are not physical GPU OOM observations.
The pre-existing guest-write bounds concern reported by security review is recorded, not claimed
fixed by this bounded ordering change. No new D3D11 work or runtime compilation/translation.

**Next:** exactly one pre-approved operation for N4.3 narrower barriers/layouts, a different
worker's deciding test and private mutants, matched quiet N2 before/after and final N3 witness.
No suballocation, persisted cache/R4, N6 or R5 in that operation. Current run retains 5 operation
slots and zero unassigned repair cycles before next admission (lineage 15 operations / 6 repair
cycles); ordinary CONTINUE authorizes the next increment, no allowance reset or rotation credit.

### §31.38 N4.3: narrower Vulkan barriers and precisely scoped layout transitions (2026-10-17)

**Implemented; STOP for independent review.** The plan's N4 "narrower barriers" subitem, one listed N4
item, no later item. Every normal-path barrier in the toolkit's Vulkan back end now records the stage and
access scopes of the producer and of the consumers it actually orders, instead of the previous
`ALL_COMMANDS -> ALL_COMMANDS`, `MEMORY_WRITE -> MEMORY_READ|MEMORY_WRITE` pair. No layout VALUE changed;
no counter, ring, publication, stop or ownership semantic changed; no new toolkit file; no runtime
translation or compilation.

**The helpers and the scopes.** `vb_dep(src_stage, src_access, dst_stage, dst_access)` records a
`VkMemoryBarrier`; `vb_image_barrier` was replaced by
`vb_image_dep(img, aspect, from, to, src_stage, src_access, dst_stage, dst_access)`, the same layout
transition with explicit scopes. `VB_STAGE_IMAGE_DST` / `VB_ACCESS_IMAGE_DST` name every stage and access
the back end touches a target, depth or texture image from; `VB_STAGE_ATTACH` / `VB_ACCESS_ATTACH_WRITE`
name a render pass's attachment writes; `VB_STAGE_DEPTH` / `VB_ACCESS_DEPTH` the depth-stencil
consumers. `VB_ACCESS_IMAGE_DST` carries `COLOR_ATTACHMENT_READ` because the render pass is created with
`loadOp = LOAD`, so a pass READS its colour attachment's previous contents, and `TRANSFER_WRITE` because
`vb_target_for` creates a target and leaves it `stale`, so `vb_setup` loads it with
`vkCmdCopyBufferToImage` in the SAME submission — a write-after-write the old `MEMORY_READ|WRITE` covered
and the scoped destination set must keep covering. Both were found by review of the first draft, not by
the gate.

**Sites changed (S1–S9 of the packet's dependency argument, kept outside git in the plan N4.3 records).**
`vb_end_pass` (the pass's colour and depth-stencil writes before every later access to that image);
`vb_target_load` (the load copy's transfer write); `vb_target_create` (the create clear, plus its
`UNDEFINED -> GENERAL` transition scoped TOP_OF_PIPE/0 -> TRANSFER/TRANSFER_WRITE); `vb_depth_for` (the
far/stencil clear and its transition, destination scoped to the two fragment-test stages); the readback's
pack `vkCmdDispatch` -> `vkCmdCopyBuffer` pair (COMPUTE_SHADER/SHADER_WRITE -> TRANSFER/TRANSFER_READ);
the readback publish barrier's SOURCE stage (COMPUTE_SHADER|TRANSFER instead of ALL_COMMANDS; its access
masks and HOST destination unchanged); the self-sampling snapshot copy in `vb_texture` (its write before
the draws that sample it, before the next copy's read of the target image and before that copy's write of
the same snapshot image); and all seven layout transitions (texture upload, the 1x1 white texture, the
target/depth/snapshot creates), each naming its own source and destination scope.

**Retained broad, deliberately.** The per-submission barrier in `vb_slot_submit` — the only thing that
makes cross-submission ordering defined, including blending into one target from two submissions and the
readback staging buffer's write-after-write when a later batch restarts its slice offsets at 0 — and the
abandonment barrier in `vb_targets_flush`'s `if (!batch_ok)` branch, which N4.2 accepted. Because the pack
path's own barrier is now scoped to its dispatch -> copy pair, the abandonment barrier is the ONLY
ordering for three classes on the abandoned path (two pack dispatches reusing the same `g.pack_buf` bytes
across an abandoned group; an abandoned group's `g.rb_buf` copy against the next group's pack-output copy;
and the abandoned group's `vkCmdCopyBuffer` read of `g.pack_buf` against the next group's dispatch write),
and the retained ordering gate now decides them against it.

**Why the scoped forms are sufficient.** The Vulkan specification (Synchronization and Cache Control,
https://vulkan.lunarg.com/doc/view/1.4.304.1/windows/antora/spec/latest/chapters/synchronization.html)
says a barrier's first synchronization scope is "the pipeline stages specified in that mask and any
logically earlier stages", while "its first access scope only includes memory accesses performed by
pipeline stages explicitly specified in the source stage mask" (and symmetrically for the destination). So
a write-after-read arrow needs only the stage scopes — FRAGMENT_SHADER is logically earlier than
LATE_FRAGMENT_TESTS and COLOR_ATTACHMENT_OUTPUT, which is what orders a draw's sample of the self-snapshot
image against the next snapshot copy — while availability/visibility needs each access bit named at a
named stage. The render pass's implicit external dependencies carry no access mask, so the explicit
barriers are what a pass's attachment read (`loadOp = LOAD`) and a later copy or sample depend on. The
pack buffer's reuse inside one batch is disjoint by construction, across batches it is separated by that
batch's submit plus `vb_drain`, and across an abandoned group by the retained abandonment barrier.

**Retained gates migrated by a different worker (clean cutover, no shim).** `vb_image_barrier` no longer
exists, so `scripts/vk-ring-fault-gate.py`'s required anchor and the call in
`tests/test_vk_ring_fault_gate.c` moved to `vb_image_dep` with the same semantic content
(UNDEFINED -> GENERAL, TOP_OF_PIPE/0 -> TRANSFER/TRANSFER_WRITE); that gate's nine properties and 105
checks are unchanged and green (private receipt, exit 0). The three checks in
`tests/test_vk_readback_ordering_gate.c` that asserted the pack path's own barrier was the broad one are
re-expressed against the barrier that orders those pairs now — the abandonment barrier, with its broad
ALL_COMMANDS/MEMORY shape asserted explicitly — and the now-dead `gate_barrier_between()` locator is
deleted; the general write-after-write, write-after-read and cross-recording decisions were not touched.
The adapted gate is GREEN on the final source (14 properties, 275 checks, 2.77 s in the suite) and RED on
a private copy with the abandonment `vb_barrier();` deleted (11 of 14 properties, 31 of 275 checks — the
pre-fix red set plus property 1b, which the adaptation promotes from a control to a deciding check) and
RED on a private copy whose abandonment barrier was replaced by a narrower one (11 of 14, 29 checks).

**New deciding gates, each authored by a worker distinct from the production change.**
`jsrf_vk_sync_scope_gate` (`tests/test_vk_sync_scope_gate.c` sha256
`4b73e1f21ad7fcdff54df947c8d50102f0ce13bdc272cb48ea8b0e4ba507fb65`,
`scripts/vk-sync-scope-gate.py` sha256 `ffafa47eaf9a1b00b09b9c938edb9d5e326adfa2535f5e2c5d379d8912de86dd`,
+115 lines of CMake): no GPU, no ICD, no skip path, exit 0/1. It compiles the production back end verbatim
with recording stubs over every Vulkan entry point, drives the production bodies that record each changed
barrier, and decides from the recorded stream that each site's scopes are exactly the production macros'
own values (126/126 anchors, 6/6 macro values; the probe fails the BUILD, exit 2, when one moved or
changed), that every pair of conflicting accesses to the same resource and subresource has an intervening
barrier covering BOTH sides — **115 pairs decided over six hazard classes, 0 unordered** — that every
recorded access's image layout equals the tracked layout and every transition's `oldLayout` equals it (10
transitions, 44 layout-checked accesses, 6 sampled descriptors), and that the only ALL_COMMANDS barriers
are the four per-submission ones. GREEN: **5 properties, 77 checks**. Nine private mutants (one private
backend copy per row, never the shared tree): mis-scoping S2/S1/S3/S4/S5 turns exactly its class red
(C2 / C3 / C4 / C5 / C3 — 1, 18, 1, 3, 1 unordered pairs); removing a layout transition turns the layout
class red (S6: 2 wrong layouts, 4 bad descriptors; S7: 10 / 30 / 4); replacing S1/S7/S8 with the broad
`vb_barrier()` turns the "narrow form is in force" property red (9 broad barriers for 4 submissions) while
every hazard class stays green; and a comment-only control is green.

`jsrf_vk_sync_paths_gate` (`tests/test_vk_sync_paths_gate.c` sha256
`d07c51d813e0aec7d5d6816e0870b783f25e79c32540cbf9be1287ba0a6f9aad`,
`scripts/vk-sync-paths-gate.py` sha256 `2501017e18dc76b0f9095c0786e7eaf0c25cdfc7f2c47539399a2ec770431264`):
the real device (software ICD, lavapipe) installed through the production `nv2a_vk_backend_install()` and
observed through forwarders over the real driver entry points, so the scopes it decides are the ones the
driver really received. It drives repeated draws, a 16-bit and a 4-byte target, depth/stencil ordering, a
texture upload and a magnified sample, a self-sampling snapshot and three flips that pack and copy in one
flush. It requires each changed site to have recorded its exact masks at least once (S1 5, S2 13, S3 3,
S4 3, S5 1, S6 texture 2 / white 2, S7 4, S8 1; targets 4 / depth 1 / snapshot 1 creation transitions; 24
memory and 10 image barriers, **0 unclassified**; one ALL_COMMANDS barrier per submission, always last
before `vkEndCommandBuffer`, none inside an open pass), the reachability counters to be nonzero
(`rt_loads 5, tex_uploads 1, rt_copies 1, rt_aliases …, readback_packs 3, readback_copies 3,
max_inflight ≥ 2`), and the published guest bytes to equal its own exact expectation — the 16-bit
truncation, a partial scissor's untouched halfwords, depth order, replicated texel blocks, the snapshot's
derived value and frame-2/3 load-then-redraw byte-identity. GREEN on lavapipe: **6 properties, 119 checks**
(2.0 s in the suite), exit 0. Eleven private mutants: M1–M5 (mis-scoped S2/S1/S3/S4/S5) and M6/M7
(removed transitions) each turn their property red (4/4/5/5/5 and 2/2 failed checks); M8 comment-only is
green; M9 (broad substitution) fails only the narrow-form properties (9 checks) with every hazard class
green; M10 (one published byte changed in the publication memcpy) is caught ONLY by the byte property, and
M11 (the draw entry no longer splits) ONLY by the reachability property — so the properties are not
redundant with one another. The case reports the back end's own validation counters and makes no
synchronization-validation claim of its own.

**Synchronization validation, with its own positive control (this packet's receipts).** The final-source
paths gate was run with the Khronos layer and synchronization validation enabled
(`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`, modern switch `VK_LAYER_VALIDATE_SYNC=1`, and the
software ICD): exit 0, `PASS: 6 properties, 119 checks`, zero validation errors and warnings in the
fixture window, and the loader's own insertion lines quoted from that run

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

That the layer was really checking synchronization is shown by two private controls of the same fixture:
the mis-scoped mutant M1 makes it report **4 `READ_AFTER_WRITE hazard detected` errors** (three on
`vkCmdCopyImageToBuffer`, one on `vkCmdCopyImage`) in the fixture window, while the broad-barrier mutant
M9 — which is SAFE Vulkan but fails the gate's narrow-form properties — reports **0** errors. So the clean
final-source result is a real check, not ordinary validation wearing its name.

**Reviews, all by agents distinct from the production and gate authors.** The code review confirmed no
uncovered access pair and no VUID-visible mistake, and found two blocking breaks in the GAME repo and
three comment overclaims, each fixed: the ring fault gate's anchor and call (migrated, above); the three
ordering-gate control checks (adapted, above); the pack-dispatch comment now names the abandonment
barrier on the abandoned path; the snapshot/texture transition comments no longer claim a compute sampler
exists (the bits stay, documented as a deliberate superset for the sampler class); and the file's top
comment now states the layout facts per image (a target is attachment + sampled image + copy source; a
depth image is attachment + transfer destination; the snapshot is copy destination + sampled image) and
that some destination scopes are deliberate supersets. The security/native-memory-lifetime review found
no blocking issue: no create/allocate/free/map in the new code, no lifetime or ownership change
(lifetime here is fence-based, so a narrower visibility scope cannot create a use-after-free the broad
one prevented), the failure/stop paths and the executor's `stopped` contract untouched, and the
pre-existing publish `memcpy` guest-bounds gap neither widened nor narrowed (still recorded as
out-of-scope, as N4.2 recorded it). Neither review found a routing or model change; `.omp/config.yml` was
not touched, and no fallback was observable.

**Validation on the frozen tree.** Guarded build `python -X utf8 scripts/build-jsrf.py`: success
(`logs/build-current.log`), executable `11ce8b039f5b5694f3d432867817114c9656c6d2d1f56132464d77992a7e50ba`
(18 814 976 bytes); `build/Release/build-source.json` records this tree's sources, including the toolkit
backend at `a3cd2cec71f6e6264258d8c5b9b28f20f3659546f96b827930db5aa2bb5847bf` and this CMakeLists.txt at
`a3336549840f1d06a6bb27a55a739d3b5d3db33e1b957d4f7c134f020ef4c4ae`, and the vsh tool identity and retail
corpus are unchanged (128 programs, 126 distinct; no regeneration). Full CTest through
`python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure`: **93 of 94 passed in 518.03 s**
(receipt `receipts/final-ctest-n43.txt`), including both new cases (`jsrf_vk_sync_scope_gate` 0.06 s,
`jsrf_vk_sync_paths_gate` 2.01 s — the real-device case RAN, it did not skip), the adapted
`jsrf_vk_readback_ordering_gate` (2.77 s) and the migrated `jsrf_vk_ring_fault_gate` (0.31 s), the retained
exact-byte real-device cases (`jsrf_vk_frames_in_flight` 17.26 s, `jsrf_vk_readback_pack`, the caller and
fault gates), Vulkan parity (14.92 s), the AOT set and the eight V6 conformance cases. The single failure
is `xbox_guest_meter`, the plan's DOCUMENTED known flake (timing-sensitive); it passed on the documented
single rerun (receipt `receipts/flake-rerun-xbox_guest_meter.txt`, exit 0, 0.34 s). No `Skipped` and no
`Not Run` line appears anywhere in the receipt. `just check`: passed (receipt
`receipts/final-check-n43.txt`), including the real retail corpus
`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126 problems=0` and its
SPIR-V validation, and the stop-chain checker. No unchanged translator output was regenerated.

**N2 (three quiet 60 s Garage windows per side, `config/key-scripts/garage-bench.json`, RTX 4070, the
same runner, device and instrumentation, one game at a time, no pooling and no historical control).**
Before, on the accepted exe `46c4ffe9…` (which already contains N4.2): 43.93 / 44.00 / 44.33 →
**median 44.00 presents/s** (range 0.40; p50 22.21, p90 24.18, p99 26.87 ms; 0 stalls and 0 aliased polls
in all three). After, on `11ce8b03…`: 42.87 / 43.95 / 45.30 → **median 43.95 presents/s** (range 2.43;
p50 22.24, p90 24.20, p99 26.24 ms; 0 stalls and 0 aliased polls in all three). **No performance claim in
either direction**: the medians differ by 0.1 %, the percentile medians by about 0.1–0.6 %, and the after
range spans the before range on both sides — the frame's cost is dominated by the readback drain this
item does not touch, so the removed dependency-graph work is not visible in presents/s. One further after
window (`n43-after-bench-1`, 2026-10-17 08:54) crashed at t = 59.4 s with exit `3221225477` during the
pre-bench intro, before the window, so it produced no bench data and is excluded from the three. It is
the plan's DOCUMENTED pre-existing startup crash, not an N4.3 effect: the identical signature
(`3221225477`, `ran_s 59.4`, `keyscript_incomplete`, same phase, same `[GPU]` summary shape, and a healthy
backend line with `validation_errors=0 vsh_misses=0 rb_incomplete=0`) is archived twice on the PRE-change
build (`logs/runs/20261016-175740-883-keyscript-n4-post-vk-1` at 59.4 s and
`20261016-180805-012-keyscript-n4-post-vk-3` at 57.7 s) and in older 2026-10-11/12 archives. The crash is
recorded, not smoothed over. Receipts: the six `bench.json` files of those windows.

**N3 bounded e2e on the final source** (`20261017-092457-314-keyscript-n43-n3-e2e-val`): stock
`config/key-scripts/e2e.json`, 620 s bound, Vulkan back end, exploratory profile, exe `11ce8b03…`
(identical to the built and committed bytes), device RTX 4070, isolated disposable save root. The run
started from a CLEAN tree: its archived `project.patch`, `project-status.txt`, `toolkit.patch` and
`toolkit-status.txt` are all empty and `project_archived` is true, so the archive pins the exact
repository state. `VK_INSTANCE_LAYERS` and `VK_LOADER_DEBUG` were set AND exported in the invoking shell
and also passed to the runner; the run's `result.json` records the overrides
`['RECOMP_GPU_BACKEND=vulkan', 'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_LOADER_DEBUG=layer']`.
The loader's own insertion lines, quoted from that run's `jsrf_run.log` (158 loader lines):

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

Disclosed, not set by this packet: the same log lists an inserted `VK_LAYER_OBS_HOOK` instance and device
layer (`C:\ProgramData\obs-studio-hook\graphics-hook64.dll`) — a third-party implicit layer present on
this host. Outcome `keyscript_complete`; 0 ICALL lines, 0 ABI failures, 0 fatal, 0 `still_rejecting`;
`validation_errors=0` and `validation_warnings=10` (all the known
`vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8 not consumed by vertex shader`);
`vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`, `tex_undecodable=0`,
`rt_copies=0`, `rb_copies=0`, `rb_incomplete=0`; 24 954 flips, 25 159 presents, 2 822 622 draws,
910 777 691 triangles, 49 907 published readbacks (`rb_batches=24 954`, all 16-bit: `rb_packs=49 907`, so
this run does NOT exercise the 4-byte straight-copy publication path — the sync-scope gate's C4/C5 pairs
and the sync-paths gate's own counters, plus the retained pack case's 32-bit targets, do), 72 937
submissions with `max_inflight=3` and 47 320 mid-frame splits. Termination was bounded and deliberate —
`killed_at_deadline_or_failure: true`, `exit_code 1`, `ran_s 620.6` — not a normal game exit, and this is
a loader-witnessed normal-path run, not a fidelity proof.

**Limits.** The injected failures of the retained gates remain stubbed driver return values, never real
pool exhaustion or device loss. The scope gate's staging bytes are supplied by its fixture, so it decides
ordering, ranges, masks and layouts — not the bytes a real copy engine or the pack kernel produces; those
are decided by the real-device cases. The paths gate's fixture is purpose-built, so it decides the
changed sites' scopes and the published bytes for its own scenario, not the whole game's workload (the
retained frames-in-flight and pack cases cover the real workload's bytes). This change makes **no
frame-rate claim**: it removes work from the frame's dependency graph, and whether that shows up as
presents/s is decided by the N2 windows recorded with this packet, not by the argument. The guest-memory
publish bounds concern recorded by N4.2's security review is untouched and NOT fixed here. Sub-allocated
memory, the persisted pipeline cache and R4 remain open in N4, and no later item is accepted by this
record.

PUSHED_TO: origin / BRANCH: main / COMMIT: 5af1c4824697517ad68a4b75213fd6483fd17561 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward cd638d4..5af1c48, toolkit pushed first, synchronized (origin/main == 5af1c48).
PUSHED_TO: origin / BRANCH: master / COMMIT: 4e09019cf5287a9ca81ff0c432353f478ed407d2 (N4.3 gates), 8463caf50b5ff64002820f2c25c982049f9bbce2 (N2/N3 records), 1b0e2914effb9fbe0d2f85052c110a6f5536740b (push receipts) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 1317e26..1b0e291, toolkit first, clean synchronized at the independently fetched checkpoint.

### 31.39 N4.3 independently accepted with timing and crash limits retained (2026-10-17)

**Accepted:** toolkit `5af1c48` / game gate `4e09019`, records `1b0e291`, fetched clean and
synchronized. Reviewed full production diff: stage/access-scoped helpers at the intended sites,
seven migrated image transitions with the same layout values, and both broad submission and
abandoned-batch guards retained. Attachment LOAD reads and transfer clear-to-load writes stay
covered. Ring, ownership, publication and bounded stop/retry semantics are unchanged.

**Independent execution:** limited-token `jsrf_vk_` CTests **8/8, 31.72 s**, no skips, covering
real lifetime/readback plus ring/fault/caller/ordering and both new synchronization gates. Private
scope mutants M1–M9 re-executed: **1/1/1/1/1/1/1/0/1** exit codes. M1–M7 catch the changed access
or layout defect; comment-only M8 stays green. M9's broad replacement has zero unordered hazards
but fails the optimization-positive controls, as it should. Final scope gate: 115 pairs decided,
0 unordered, 5 properties / 77 checks; final real paths gate: 6 properties / 119 checks.

Codex separately ran the real paths fixture under Khronos synchronization validation through the
medium-integrity helper. Final source passes without validation errors; private unsafe M1 fails
and the layer reports **four READ_AFTER_WRITE hazards**. Safe-but-broad M9 has zero validation
errors and fails only narrow-form checks. Thus neither mere barrier presence nor an unverified
validation setting decides this result. Raw independent logs are outside git under
`C:/Users/logic/xbr-mac/n43-codex-tests`. The first disposable harness's non-native ICD path was
corrected to the exact working repository environment; no skip/setup failure counted as a pass.
No production or shared test source/executable was mutated.

Final source SHA-256 `a3cd2cec71f6e6264258d8c5b9b28f20f3659546f96b827930db5aa2bb5847bf` and
exe `11ce8b039f5b5694f3d432867817114c9656c6d2d1f56132464d77992a7e50ba` match the final archive.
Inspected distinct production/test/maintenance and code/security review assignments. Retained
gate migrations fix both review findings without removing their general dependency predicates;
the broader abandonment guard is now explicitly the deciding cover for those classes. Per-agent
model/fallback claims remain configuration-derived because transcripts expose no model field.

**Full-suite accounting:** `final-ctest-n43.txt` is **93/94, 518.03 s**, sole failure the known
`xbox_guest_meter` timing flake. The plan permits one rerun; the named rerun receipt is **1/1,
0.37 s**. This is not a claim the first suite was 94/94. Final checker/corpus 126/126 inspected.
Focused independent checks were rerun; a duplicate full suite was unnecessary.

**N2 verified:** all six copied bench receipts hash-match their archived `bench.json`, same
RTX 4070 Laptop, 60 s windows, exact before `46c4ffe9` and after `11ce8b03` executables. Before
43.9337 / 44.0010 / 44.3335 presents/s, after 42.8670 / 43.9504 / 45.3004: medians **44.00 →
43.95**, wider after spread, zero stalls/aliased polls. No measurable gain or attributed regression.

**Crash qualification:** the excluded after run `20261017-085421-561` exited `0xC0000005` at
59.4 s, before the benchmark step, as its actual result records. Similar startup failures precede
this change, but the same exit code/phase and healthy GPU counters do not prove the same cause
or exclude an N4.3 contribution. Its cause remains unresolved. Three valid after windows and
the final e2e complete; acceptance makes no crash-fix or general stability claim.

**N3 verified:** `20261017-092457-314-keyscript-n43-n3-e2e-val` reclassifies exploratory, no
UNKNOWN/MISSING, stock script complete. Actual Khronos insertion 857/1288, implicit OBS hook
877/1298 disclosed. Final errors 0, known warnings 10, miss/identity/decline/fallback/incomplete
0; 24,954 flips / 49,907 packed readbacks, 72,937 submits / max in flight 3. Source/exe match;
deliberate 620.6 s deadline/exit 1 is not normal exit or fidelity. This e2e uses ordinary validation;
the independent fixture above separately verifies synchronization validation with a negative control.

**Audit/limits:** all changed blob versions independently scanned in the correct stores:
toolkit 1 / game 13, zero secret hits, forbidden paths or oversized blobs. Intended origin branches,
unsigned commits/no co-author or signature trailers, toolkit-first pushes verified. Pre-existing
guest-write bounds risk remains disclosed and unfixed, not widened by these mask changes.

**Next only N4.4 suballocated memory:** different production/test authors, meaningful alignment,
memory-type, lifetime and failure gates with private mutation proofs, fresh quiet N2 controls,
final N3 witness. No persisted cache/R4, N6 or R5 in the operation. Same run retains 4 operations
and 0 unassigned repair cycles before next dispatch; after it 3. No allowance reset. All owner
stop conditions remain.

### §31.40 N4.4: sub-allocated device memory (2026-10-17)

**What changed.** The Vulkan back end allocated one `VkDeviceMemory` per resource, bound at offset
0. It now takes a range out of a pool block. Toolkit `ecbaca49b506854977474660cc0f11fdaaaf21da`
(backend `9e650e633374679aae4186bb9b8b825db58b5ca1ad2255807b74f315c0bea96f`, header
`b6e615e14c68218925b15168a554a944a2cb8938b1a2cf2a67d2d7cbdfa48d5f`); game
`3f4e82f` (new gate `tests/test_vk_suballoc_gate.c` `6eab1487979a349681ec17bcda15a2d05646c26d0a9a9d943244a27ff713406c`,
probe `scripts/vk-suballoc-gate.py` `08b3640589acf94ce97342146635c27cda52b106f610cabc408ec8bc82f0437c`,
the `--suballoc` mode of `tests/test_gpu_backend_parity_vk.c` `c485faae1cfaf279…`,
`CMakeLists.txt` `1954a4f1c07a7a01…`, and the migrated gates `tests/test_vk_ring_fault_gate.c`
`dc6797046d95ce89011eb59d1dd2111f9048111f95d29c805078802e68f9da02` and
`tests/test_vk_sync_scope_gate.c` `074e15bc258e3452…`).

**What is pooled, and what is not.** Pooled: render targets, self-sample snapshots, depth images
and textures -- the churn path, one image per guest-byte hash change -- plus the 1x1 white image, all
optimal-tiling, so in a non-linear pool; and in the linear pools the ring's slot streams, the
standalone slice buffers `vb_stream` allocates for a slice over half the slot stream, and the readback
staging pair (`rb_buf` host-visible cached, `pack_buf` device-local). Honest qualification: the ring's
three 16 MiB streams ARE ranges of the linear host-visible pool but each gets its own exact-size block
(`VB_POOL_SHARE_MAX` is 8 MiB), so pooling changes nothing for them -- the reduction measured on the
game's own workload comes from the image classes and the small slice buffers. Dedicated allocations
survive, each reason counted separately: `requiresDedicatedAllocation` and
`prefersDedicatedAllocation` (from optional `vkGet{Image,Buffer}MemoryRequirements2` queries whose
absence means "not available", the request then being pooled), a range over `VB_POOL_MAX_BLOCK`
(64 MiB), a pool at its six-block or 256 MiB reserve budget, a full pool table, and a host-visible
memory type without `HOST_COHERENT` (a pooled host-visible block is mapped as a whole, and one range
of it could not be flushed without touching its neighbours).

**Contracts.** A range's offset is `max(mr->alignment, VB_POOL_ALIGN)` (256); the type is chosen from
`memoryTypeBits` and the property flags exactly as before (want, then fallback); a pool's identity is
`(memoryTypeIndex, linear|non-linear)`, so an image and a buffer never share a block and
`bufferImageGranularity` can never apply between neighbours; every `VkDeviceSize` addition and round-up
goes through overflow-checked helpers. Ownership is explicit -- `VbAlloc` names the memory object, the
binding offset, the range size and the pool/block indices (-1 for a dedicated allocation bound at 0) --
and a host-visible block is mapped once at creation, so a buffer's pointer is that mapping plus the
range's offset. `vb_mem_flush`/`vb_mem_invalidate` clip to the sub-allocation and widen to whole
`nonCoherentAtomSize` atoms on a non-coherent type, so a neighbouring live range's bytes are never
touched; the publication now invalidates the staging range itself instead of `VK_WHOLE_SIZE`, and a
flush that cannot happen stops the submission rather than letting the device read stale bytes. A range
is handed out again only when nothing that could still read it is recording or in flight; a release
that is not goes to a bounded park list, keeping its block's `used` so the block cannot be retired, and
is folded back after the drain -- an over-budget release is abandoned and never reused. At most one
empty block per pool no larger than `VB_POOL_KEEP_EMPTY` (4 MiB) is retained; the rest are retired, and
only ever when `used == 0`. Allocate/create/bind/map failures release exactly what they took and leave
the caller's allocation unusable. New public `nv2a_vk_backend_memory_stats()` (blocks, ranges, reuse
hits, dedicated per reason, allocation calls, releases, parked/peak/lost, reserved/used/free/largest/
padding/metadata/peak bytes, failure and rollback counts, write-once `first_shared_block` /
`first_reuse_*` latches) is reported in the periodic `[GPUVK]` line, so a run's stderr keeps the
deciding event. The write-once failure latch, the submission ring, N4.2's one bounded retry and
per-target publication, N4.3's scoped barriers and layout values, the CPU fallback, the bit-exact guest
bytes and the executor's `stopped` contract are unchanged. Baseline honesty: one allocation per resource
was correct, only less efficient; the ring stream already sub-allocated slices inside a per-slot buffer
and still does -- N4.4 sub-allocates the device allocations themselves.

**Deciding gate.** `jsrf_vk_suballoc_gate` compiles the production bodies verbatim with every Vulkan
entry point stubbed (no GPU, no ICD, no skip path): **9 properties / 265 checks, 70 of 70 probe anchors
present**, exit 0/1. It decides that eleven images cost ONE `vkAllocateMemory` (cross-checked against
its own stub count), that ranges share a block at distinct alignment-satisfying non-overlapping offsets,
that incompatible `memoryTypeBits` never share one, that an image and a buffer with identical
requirements never share one, that a released range is reused and coalesced (the mixed-alignment case:
a 64 KiB request at alignment 65536 against a released 128 KiB range must leave its head `[4096, 65536)`
free with `padding == 0`), that oversized and dedicated-requiring/preferring requests get a dedicated
allocation carrying their `VkMemoryDedicatedAllocateInfo`, that a range a recording or a pending
submission may still read is never handed out (its mapped bytes must be byte-identical afterwards) and
that an over-budget parked release is never reused, that retirement and the retention policy behave,
that injected allocate/create/bind/map failures roll back, and that the non-coherent flush/invalidate
window is exact. A fixture-owned ledger of the extents the fixture itself took oracles `used_bytes`,
`free_bytes + padding_bytes == Σ live blocks − used`, and `largest_free`. `scripts/vk-suballoc-gate.py`
re-reads the same bytes at build time and fails the build (exit 2) when an anchor moves.
`jsrf_vk_suballoc_device` is the real-driver case (software ICD, skips 77 like its siblings): it ran
(0.95 s in the full suite), compares every guest byte of several targets of both formats, a texture,
depth and the self-sampling snapshot against its own plan and the CPU executor while the texture path
re-creates images, and decides the reduction from counters observed across the driver's entry points
throughout the churn (`8 re-creations: ranges=8 reuse_hits=7 released=8 alloc_calls=0`); the allocator's
own assertions are never the oracle for bytes.

**Independent verification and mutations.** The gates were written by a worker distinct from the
production change, and the retained gates whose meaning pooling changes were migrated by that same
distinct worker with their decisions re-derived, not deleted: the readback fault and ordering gates'
staging pair is now two ranges of one block and one allocation, the ring gate decides
free-list-versus-parked from reuse and the park counters, the caller gate reaches `VbAlloc`, and the
scope gate replaces the old "offset 0" expectation with alignment, kind, block and overlap compliance at
every bind. Private weakening mutants, built outside both repositories with the mutant's own directory
first on the include path, are red on the gate -- m1 alignment ignored, m2 `memoryTypeBits` ignored,
m3 linear/non-linear pools merged, m4 eager reuse, m5 rollback broken, m6 dedicated requirement
ignored, m8 padding double-count, m9 alignment floor removed, m10 retirement removed -- and m7 pooling
disabled is the optimization-positive control: every byte check stays green while its allocation,
sharing and reuse claims fail. The orchestrator's own independent overlay (the free-range containment
check removed) fails property 2 on the free-list extent check with the restored source green.

**Reviews and the repairs they caused.** The code review found one real defect -- `vb_block_take_free`
counted a free range's aligned head as padding while also leaving it on the free list, so
`padding_bytes` was permanently overstated and `free_bytes` understated -- plus an empty-block retention
policy that could never fire (`VB_POOL_KEEP_EMPTY` below the smallest block the sizing rule could make),
a sizing rule that over-reserved large ranges (a 9 MiB slice took a 64 MiB block where the pre-change
code allocated 9 MiB), tautological accounting checks, and gate gaps (no cross-bind overlap check, a
block scan that stopped at `nblocks`, no alignment below the floor, park and retirement untested). All
were repaired in one bounded round and the gate grew the cases that decide them. The
security/native-memory-lifetime review found **no High or Medium defect**: it traced the park-before-
reuse gate over every release path, the retirement rule, the ownership moves through the garbage list,
the parked ranges pinning their blocks, the mapping arithmetic, the rollback paths and the optional
loader lookups as sound; its two Low hardening notes (an unchecked overflow in the atom widen, an index
bounds check in the release) were applied. Neither review found a routing or model change;
`.omp/config.yml` was untouched. Reports: gates/migrations from the test worker, reviews from two
further distinct agents; the production worker owned only the backend.

**Build, CTest and checks on the frozen source.** Guarded build `python -X utf8 scripts/build-jsrf.py`:
success, executable `d657ca5b1ecc4bdcce53d9737f0a80e774eec9bb7d22dfa04beed878ef51d82a`, and
`build/Release/build-source.json` records exactly the two backend hashes above plus the unchanged
retail corpus (128 programs, 126 distinct; nothing regenerated). Full CTest through
`python C:/Users/logic/xbr-mac/ctest-limited.py --output-on-failure`: **96 of 96 passed in 617.18 s**
(receipt `C:/Users/logic/xbr-mac/n44-ctest-final.txt`), including the two new cases (`jsrf_vk_suballoc_gate`
0.01 s, `jsrf_vk_suballoc_device` 0.95 s -- it RAN, it did not skip), the migrated ring/readback/scope
gates, the retained real-device cases, the AOT set and the eight V6 conformance cases; no `Skipped` and
no `Not Run` line appears, and `xbox_guest_meter` passed on its first run (the documented flake rerun was
not needed). `just check` on the final tree: passed (exit 0, receipt
`C:/Users/logic/xbr-mac/n44-check-final.txt`), including the real retail corpus
`entries=126 recompiled_and_matched=126 ascending=yes duplicates=0 xbe_distinct=126 problems=0` and its
SPIR-V validation; no unchanged translator output was regenerated.

**N2 (matched 60 s Garage windows, `config/key-scripts/garage-bench.json`, RTX 4070 Laptop, one game at a
time, same runner/profile/settings/instrumentation).** The windows are reported with the host's own
power state, because it turned out to dominate them: the same *baseline* executable measured 41.37 and
23.13 presents/s in two windows of this session, the second taken while the only CPU consumer was the
game itself (sampled per process: `jsrf_recomp` 13.7-14.4 % of 20 cores, host total ~15 %). Interleaved
controls on a PRIVATE PRESERVED BASELINE (the accepted exe `11ce8b03` and its own `build-source.json`,
copied out of the archived before-bench run; the shared `build/Release` executable was never replaced)
give the matched comparison:

| window | exe | presents/s | p50 | p90 | p99 | stalls | aliased | host CPU (perf%) |
|---|---|---|---|---|---|---|---|---|
| before-bench-1 | `11ce8b03` | 42.82 | 23.62 | 24.82 | 27.55 | 0 | 0/2569 | n/a |
| before-bench-3 | `11ce8b03` | 42.87 | 23.59 | 24.81 | 28.01 | 0 | 0/2572 | n/a |
| before-bench-4 | `11ce8b03` | 43.48 | 22.40 | 24.27 | 26.53 | 0 | 0/2609 | n/a |
| ctrl-baseline-1 | `11ce8b03` | 41.37 | 24.01 | 26.19 | 30.10 | 0 | 0/2482 | 114-123 |
| ctrl-baseline-2 | `11ce8b03` | 23.13 | 43.65 | 47.79 | 51.73 | 0 | 0/1388 | 101-102 |
| after-bench-1 | `d657ca5b` | 47.67 | 20.29 | 22.40 | 25.89 | 0 | 0/2860 | n/a |
| after-bench-2 | `d657ca5b` | 28.90 | 34.23 | 42.29 | 50.73 | 0 | 0/1734 | n/a (host busy: ~13 cores in Chrome) |
| after-bench-3 | `d657ca5b` | 37.27 | 22.39 | 41.93 | 47.74 | 0 | 0/2236 | n/a (host busy: Chrome) |
| after-bench-4 | `d657ca5b` | 26.70 | 38.01 | 43.13 | 48.23 | 0 | 0/1602 | n/a |
| after-bench-5 | `d657ca5b` | 23.75 | 41.90 | 46.31 | 50.34 | 1 | 0/1425 | 100-101 |
| after-bench-8 | `d657ca5b` | 24.65 | 40.32 | 46.30 | 54.03 | 0 | 0/1479 | 100-101 |

Two further runs produced no window and are excluded, preserved not smoothed: `n44-before-bench-2`
(baseline exe, exit `3221225477`, `ran_s 59.1`, `keyscript_incomplete`, 0-5 steps then skipped) and
`n44-after-bench-6`/`n44-after-bench-7` (final exe, exit `3221225477`, `ran_s 56.9`/`54.6`, same
signature). That startup access violation is the PRE-EXISTING one N4.3 disclosed and left causally
unresolved -- it occurs on the baseline executable and on the pre-change builds archived earlier the same
day, so this packet neither fixes it nor attributes it, and its three occurrences today (one before any
edit, two on the final source) are recorded rather than omitted. **No gain and no regression is
claimed:** the machine's power state swings the same executable by 79 % (41.37 -> 23.13), the matched
comparison inside one state is 23.13 (baseline) against 23.75 and 24.65 (final source), and the change's
own activity on this workload is far too small to matter -- **232 ranges, 10 `vkAllocateMemory` calls, 4
pools, 0 dedicated allocations, 0 reuse hits, 2 releases, 0 parked, 0 retired, 80 MiB reserved / 75.4 MiB
used / 4.6 MiB free / 0 padding / 23 392 B metadata, 0 allocation, bind or map failures** over the whole
560 s run (pre-change that was 232 allocations). The allocation reduction is real and is decided by the
gate and the real-device case, not by presents/s. Readback/synchronisation metrics for the same windows:
`submits=57 271`, `max_inflight=3`, `reuse_waits=614`, `drain_waits=56 657`, `splits=36 923`,
`rb_batches=19 733`, `rb_packs=39 465`, `rb_incomplete=0`.

**N3 bounded stock e2e on the final source** (`20261017-131536-178-keyscript-n44-n3-e2e-val`): stock
`config/key-scripts/e2e.json`, 620 s bound, Vulkan back end, exploratory profile, exe `d657ca5b…`, device
RTX 4070, isolated disposable save root (`--save-root` inside the run archive). `VK_INSTANCE_LAYERS` and
`VK_LOADER_DEBUG` were **exported into the game's environment by putting them on the invoking command
itself** (`VK_INSTANCE_LAYERS=… VK_LOADER_DEBUG=layer python … scripts/key-script-run.py …`, which is what
the runner passes to the child) and also passed to the runner as overrides; the run's `result.json`
records the overrides `['RECOMP_GPU_BACKEND=vulkan', 'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_LOADER_DEBUG=layer']`. The loader's
own insertion lines, quoted from that run's `jsrf_run.log` (158 loader lines):

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

A second, identical run (`20261017-134310-654-keyscript-n44-n3-e2e-val2`) was taken with the packet's
literal `export VK_INSTANCE_LAYERS=… VK_LOADER_DEBUG=layer` in the invoking shell plus the same runner
overrides, and the same outcome and counters (`keyscript_complete`, `exit 1`, `ran_s 620.5`,
`validation_errors=0`, `validation_warnings=10`, `vsh_misses=0`, `rb_incomplete=0`, 17 220 flips, 49 740
submissions, 10 `vkAllocateMemory` calls for 232 ranges). That second run's log carries NO loader trace
lines (0 lines containing `Vulkan Loader`) although `VK_LOADER_DEBUG=layer` was exported and passed as an
override, while its validation layer was demonstrably inserted and active (the 10 warnings were counted
through that layer's own debug messenger). The loader trace is therefore quoted from the first run, and
the two runs' environment forms are stated as they were rather than merged.

Disclosed, not set by this packet: the same log lists an inserted `VK_LAYER_OBS_HOOK` instance and device
layer (`C:\ProgramData\obs-studio-hook\graphics-hook64.dll`) and the driver's own `VK_LAYER_NV_present`
and `VK_LAYER_NV_optimus` instance layers. Outcome `keyscript_complete`; 0 ICALL lines, 0 ABI failures,
0 fatal, 0 `still_rejecting`; `validation_errors=0` and `validation_warnings=10` (all the known
`vkCreateGraphicsPipelines(): … Vertex attribute at location 1/7/8 not consumed by vertex shader`);
`vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`, `tex_undecodable=0`,
`rb_incomplete=0`, `rb_copies=0` (so this run does not exercise the 4-byte straight-copy publication
path; the pack case's own 32-bit targets do); 19 733 flips, 2 278 992 draws, 714 719 735 triangles,
39 465 published readbacks (`rb_batches=19 733`, all 16-bit: `rb_packs=39 465`), 57 271 submissions with
`max_inflight=3`, `dropped_tris=8`, `skipped=0`, and the same memory counters as the bench run (10
`vkAllocateMemory` calls for 232 ranges). Termination was bounded and deliberate --
`killed_at_deadline_or_failure: true`, `exit_code 1`, `ran_s 620.6` -- not a normal game exit, and this is
a loader-witnessed normal-path run, not a fidelity proof. Ordinary validation is not synchronization
validation: the N4.3 paths fixture with its negative control remains the evidence for that, and this run
claims neither.

**Limits.** Every injected failure in the gates is a stubbed driver return value, never real pool
exhaustion or device loss; the gate's non-coherent and dedicated paths are driven by controlled stub
memory types and `VkMemoryDedicatedRequirements`, not by a device that really reports them. The
reuse/coalescing properties are decided on the fixture's own workload and by the real-device case's
churn, not by the game's own run -- the game's Garage workload reuses NO range (0 reuse hits), because
its live targets and textures stay cached; the reduction it shows is sharing live ranges in one block.
The pre-existing guest-memory publish bounds concern recorded by N4.2's security review is untouched and
NOT fixed here, and the excluded startup access violation above remains causally unresolved. The persisted
pipeline cache/R4, N6 and R5 remain open, and nothing later than N4.4 was started.

PUSHED_TO: origin / BRANCH: main / COMMIT: ecbaca49b506854977474660cc0f11fdaaaf21da / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 5af1c48..ecbaca4, toolkit pushed first, clean and synchronized (origin/main == ecbaca4); upstream untouched (push URL DISABLED).
PUSHED_TO: origin / BRANCH: master / COMMIT: 3f4e82f (N4.4 gates), 3a3ac7a (N4.4 records) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 9fd8e05..3a3ac7a, clean and synchronized (origin/master == 3a3ac7a). Every outgoing blob version in each repository's own object store was audited for secrets, forbidden paths and blobs over 100 MB: toolkit 2 blobs / game 23 objects, 0 secret hits, no `game/` or retail-derived path, largest blob 397 393 bytes.

### 31.41 N4.4 independent review: parked-release overflow leaks failed-bind allocations (2026-10-17)

**Acceptance withheld.** Reviewed `op_0mvcms9rr136893e495f94ed6`, turn 3 of
`run_0mvccvfhh6a016c316b6eca67`, game `fd09501` / toolkit `ecbaca4`, fetched clean and
synchronized. The implementation report in §31.40 is superseded as an acceptance claim. Last
independently accepted checkpoint remains N4.3, toolkit `5af1c48` / game records `9fd8e05`.

**New independently reproduced defect.** `vb_image_create` gives its allocation back after a
failed bind. While a recording is open, `vb_mem_give` parks at most 64 releases. The excess
branch increments `parked_lost` and zeroes the allocation's only owner. The backend remains
usable. After recording closes, the real `vb_garbage_drain` destroys the images and recovers
only the saved ranges. A lost pooled range pins its block; a lost dedicated handle cannot be
freed. The packet required leak-free rollback, while gate property 6c explicitly expects this
permanent leak. Counting it is not a safe capacity policy.

Codex's private production-body caller proof (`C:/Users/logic/xbr-mac/n44-codex-leak/proof.c`)
uses controlled Vulkan stubs from the existing gate, then calls actual `vb_image_create` 65
times with 64 KiB requirements and injected bind failures and actual `vb_garbage_drain`:

| scenario | allocation/free calls | used bytes after drain | lost ranges | exit |
|---|---|---|---|---|
| idle, pooled | 1 / 0 (bounded empty block retained) | 0 | 0 | 0 |
| recording open, pooled | 2 / 0 | 65,536 | 1 | 1 |
| idle, required dedicated | 65 / 65 | 0 | 0 | 0 |
| recording open, required dedicated | 65 / 64 | 65,536 | 1 | 1 |

All 65 created images are destroyed in each scenario, with no submission left pending and
`g.ok == 1`. Private counterfactual source changes only the failed image-bind rollback to
immediately release its never-bound allocation, then clears that allocation before discarding
the image: all four cases pass, pooled uses one block with no live bytes, dedicated frees all
65 handles. This localizes the ownership error; it is not a complete production repair or
permission to release anything recorded or in flight. Shared production sources/executable
were untouched. Final proof receipts are `proof-0-0.txt`, `proof-1-0.txt`, `proof-0-1.txt`,
`proof-1-1.txt` and corresponding `control-*.txt` in that private directory. Initial proof
development errors (unwired stubs, one-shot requirements and fixture capacity, then control
generator newline matching) were corrected before these results; they are not game failures.

Source SHA-256 `9e650e633374679aae4186bb9b8b825db58b5ca1ad2255807b74f315c0bea96f` matches the
unmodified private source. Existing gate hash
`6eab1487979a349681ec17bcda15a2d05646c26d0a9a9d943244a27ff713406c`; independent proof hash
`4cf02d917a6cdd6d150e3acd7be6204ae3c592cc52bfca641d8b052c9f3e44fa`; private control source
`c2178f1a7ca1b64771b1e2f1aa92114e207d74578b2068a02d8f027ee942f868`.

**Other independently checked evidence.** Ten Vulkan CTests passed through the limited-rights
helper, no skips, 33.59 s. Existing private mutants were re-executed: real baseline exit 0,
M1–M10 each exit 1, restored real exit 0 (9 properties / 265 checks); M5 fails eight checks in
property 7 and M7 is the optimization control. These do not cover the newly demonstrated
capacity defect. The full-suite receipt is 96/96 in 675.46 s (earlier 617.18 s). N3 run
`20261017-131536-178-keyscript-n44-n3-e2e-val` has actual Khronos instance/device insertions at
log lines 857/1288, errors 0, known warnings 10, program misses 0, incomplete readbacks 0.
Archived/current exe hash both
`d657ca5b1ecc4bdcce53d9737f0a80e774eec9bb7d22dfa04beed878ef51d82a`; keyscript complete,
bounded termination exit 1 / 620.6 s, not natural exit or fidelity proof. The second run has no
loader trace and cannot replace the first witness.

**N2 evidence limit.** Three original quiet before windows exist. The late comparison in
§31.40 contains only one baseline and two final windows, and does not complete a matched
three-window gate. Timing drift is observed; its cause is unresolved. Do not assert that
power state caused it, that no regression exists, or that similar startup exits prove one
common crash cause or exclude this change. Retain excluded AV evidence and complete three
controlled baseline/final pairs on the final repair before accepting N4.4.

**Own repair advice and authorized reset accounting.** Repair N4.4 only: preserve ownership at
release capacity, distinguish never-published rollback from live GPU references, use bounded
admission/backpressure or safe retained ownership, and never trade the leak for early reuse,
unbounded growth or stale publication. A different worker must decide >64 failures on real
image/buffer callers, pooled and dedicated, recording/pending/idle, successful controls,
capacity pressure and failed waits. Old source must fail the new oracle; private final-source
capacity/ownership and early-reuse mutants must fail, restored final must pass. Preserve all
prior byte/lifetime gates, full CTest/check, final N3 loader witness and controlled N2 evidence.
Stop after this repair for Codex review; no cache/R4, N6 or R5 yet. Full advice and report stay
outside git at `C:/Users/logic/.omp-bridge/reports/poison-jam/n44-codex-review-20261017.md`.
Record one STOP for the reviewed operation. The owner's review-confirmed reset permits one
new repair allowance for this newly proved defect after meaningful progress. Carry all three
remaining operation slots into the linked successor, zero extra unassigned repair cycles;
the first repair admission consumes the renewed cycle even if bridge bookkeeping calls it a
first operation. Cumulative spending 17 operations / 6 repairs before admission, 18 / 7 after,
two operation slots then remain. Preserve prior decisions and stop conditions; no repeated
reset for an unchanged defect. Windows unsigned audited clean FF pushes remain toolkit first.

PUSHED_TO: origin / BRANCH: master / COMMIT: 59d0e33c9ed10ee7342dd9247dbfe01a1eddb15c / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: successful unsigned clean fast-forward fd09501..59d0e33; three outgoing documentation blobs audited in the game object store, largest 404195 bytes, zero secret hits, only plan/TR/ledger paths. Toolkit ecbaca4 is unchanged, clean and synchronized, so no toolkit commit or push was needed for this records-only review.

### §31.42 N4.4 repair: release ownership survives rollback and a full park list (2026-10-17)

**Starting state and the record correction.** This operation is the ONE renewed repair allowance
of `run_0mvcw0nza216599429433b15c`, admitted after the independent review in §31.41. Correction
carried from that review: the operation the report of the rejected packet should have named is
`op_0mvcms9rr136893e495f94ed6` (generation 7, turn 3 of `run_0mvccvfhh6a016c316b6eca67`), NOT the
preceding N4.3 operation; the reviewer's STOP for it is `dec_0mvcvzxpza270765af4ddb44d`. Last
independently accepted checkpoint remains N4.3 (toolkit `5af1c48` / game records `9fd8e05`).
Starting trees: toolkit `ecbaca4`, game `02945a50b206a8ea67c74b5bf85441a0d9689c01`, both fetched
clean and synchronized. The implementation report in §31.40 stays historical: its statement that
"an over-budget release is abandoned and never reused" (line 4894) describes the code the review
rejected, and §31.41 plus this section supersede it.

**The defect (§31.41) and what the repair changed.** `vb_image_create` gave its allocation back
through `vb_mem_give` after a failed `vkBindImageMemory`. While a recording was open that call
parked the release in the 64-entry `s_pool_pending[]`; the excess branch incremented `parked_lost`
and cleared the caller's only `VbAlloc`, so the range was never reusable and never freed (65
failed binds leaked 65 536 bytes pooled, or one `VkDeviceMemory` handle dedicated). The repair is
a bounded ownership split, not a bigger list:

- **A never-referenced release never parks.** New `vb_mem_give_now(VbAlloc *)` releases at once for
  an allocation nothing recorded or in flight can name: a failed bind (the object is unbound by
  definition) and, after the object is destroyed first, a failed map on the buffer or image that
  call had just made; plus the staging pair `vb_readback_reserve` builds and abandons inside its
  own call (`vb_garbage_free_now`, a thin wrapper over the new `vb_garbage_destroy`). A burst of
  rollbacks therefore cannot fill the bounded list, and the range is reusable immediately.
- **Destroy before release, everywhere.** `vb_host_buffer`/`vb_device_buffer` now destroy the
  possibly-bound buffer BEFORE the range goes back (the pre-repair order released a bound range
  first), and the deferred-destroy queue's own order (framebuffer, pipeline, view, image, buffer,
  then the range) is now explicit and asserted.
- **A full park list refuses instead of dropping.** `vb_mem_give`'s excess branch counts `parked`
  and `parked_lost`, reports through `vb_fail`, then stops the back end write-once through the
  existing `vb_stop` and returns WITHOUT touching the caller's `VbAlloc`. The range keeps being
  counted live in its block (`used`/`live`, or `dedicated_bytes`) and reaches no free list:
  nothing is freed, nothing is handed out, nothing can be reused early. Production reaches this
  branch only through `vb_garbage_free`'s by-value entry (dropped by the drain) or `vb_stream`'s
  dead-recording branch, so it is a bounded safety net, not an observed behaviour: with every
  busy-time release now never-referenced, at most one entry can ever park in a sound process.
- **Counters and documentation.** `released` now counts ADMITTED releases only (a refusal is
  counted by `parked`/`parked_lost` alone); `parked_lost` is documented as a capacity refusal
  that retains the range's ownership and stops the back end, never as a silently abandoned range;
  no counter, struct field, constant or signature was renamed, added or reordered.

**Identities (frozen repair).** Toolkit `src/video/nv2a_vk_backend.c`
`197bf83ecea4c9e8cb1f2f2a8208c603dd3088a36418a9cdabed543e78b00f0f` (pre-repair
`9e650e633374679aae4186bb9b8b825db58b5ca1ad2255807b74f315c0bea96f`), `nv2a_vk_backend.h`
`3b8081fdf88cf9b438e3f443e236926497d2ab4a3f079d869aaf4a97237a5be1`; game
`tests/test_vk_suballoc_gate.c` `9ad4745439bae7f3415eb5f327eb3e7149347fa0f8e1b119ae62172961bb6ad9`,
`scripts/vk-suballoc-gate.py` `00bde834d02d006d73fa724a4369ec3f7faf6b2a51d712c4c6d606c3f5cbe05f`,
`tests/test_gpu_backend_parity_vk.c` `7ddd6eb0f1129990fe1f100dc78ac6e29d9f0c1ce72954da937bcb765e3e2fd2`,
`tests/test_vk_ring_fault_gate.c` `984da5e45be729f97bf38c2ca44506a2148a75ef33b01cdb4a08b79e94a71d60`;
executable `57f69c7ff90d9cff0ee85249c072720ab8e346be76b79ffb255f12fb7ec107ea`. Guarded build
(`python -X utf8 scripts/build-jsrf.py`, 70.04 s, success): the same retail corpus, schema 2,
128 programs / 126 distinct, tool `375c7fb901109bfd51eecacda919e115db942aeb65e4fb9efbda6c7bb9aefb0e`,
nothing regenerated and nothing compiled at run time.

**The deciding regression, on the production bodies.** The gate compiles the toolkit's back end
verbatim with controlled Vulkan stubs. It now drives the REAL callers: `vb_image_create` (pooled
and required-dedicated, recording open and submission pending), `vb_device_buffer` bind failures,
`vb_host_buffer` map failures and `vb_readback_reserve`'s failed pair, each more than
`VB_POOL_PENDING` times; property 6c was replaced by the refusal contract (the range stays live in
the block, on no free list, `used_bytes` keeps it, the back end latched stopped, no lost ledger
entry) and its busy-ring decision is now taken from the block and the ledger BEFORE any
`vb_pool_gc`, so production's stopped early-return cannot satisfy it; property 7k now decides that
a never-referenced failed bind is released, not parked; a stub event log asserts every
destroy-before-release. Verdicts: pre-repair `ecbaca4` source (`9e650e63…`) BUILDS, RUNS and exits
1 — `FAIL: 3 of 10 properties failed (35 of 336 checks failed)`, including the refusal's
keep-ownership check, the stop latch, the lost ledger entry, 10a/10b `parked 65, peak 64, lost 1`
with `used_bytes 65536` after the drain, 10c `65 allocs / 64 frees`, 10d/10e and 10h; repaired
source exits 0 — `PASS: vk_suballoc_gate: 10 properties, 335 checks`. The probe reports 70 of 70
required anchors and every marker present against BOTH sources (exit 0), so the "old source fails"
verdict is behavioural, not a seam refusal. Receipts: `C:/Users/logic/xbr-mac/n44r/old.out`,
`new.out`, `probe-old.log`, `probe-new.log`.

**Private final-source mutants** (`C:/Users/logic/xbr-mac/n44r-mut/MUTATION-RECORD.txt`, 11
targets, nothing written inside either repository). Restored frozen source: PASS 10/335.
`m1-overflow-drop` (the rejected rule restored) red on 6c's ownership, latch and no-lost-entry
checks; `m2-rollback-parked` (the failed bind parks again) red on 7k and 10a/10b/10c;
`m3-eager-release` (release while the ring is busy) red on 17 property-6 checks; `m4-release-before-destroy`
red on exactly the two ordering checks (property 7 and 10); `m5-refusal-clears-owner` red on 6c's
ownership and no-lost-entry checks with the latch check still green; `m6-never-referenced-parked`
red on 10a-10e/10h and 7k. Migrated controls re-run against the repaired source: readback fault
gate restored PASS (7 properties / 138 checks) vs `m7-pooling-disabled` red on the re-derived
"two buffers, two ranges of one pool block, one allocation"; sync scope gate restored PASS (6/80)
vs `m1-alignment-ignored` red on property 5's bind rule. Two expectations stayed green and are
disclosed rather than smoothed: no weakening confined to `vb_mem_give`'s refusal branch can make
property 10 red (its bursts all go through `vb_mem_give_now`; `m6` is the mutant that reaches
them), and the 6a/6b byte-identity checks cannot see an eager release because this fixture writes
nothing into the released range between the give and the next take -- the hand-out checks decide
it. That second gap is a limit of the fixture, recorded below.

**Reviews and the repairs they caused.** The independent code review (the configured `reviewer`
role was rate-limited, HTTP 429 with a ~32 h retry-after, so a general-purpose OMP agent performed
the review -- model fallback disclosed; the security/native-lifetime review ran on its configured
role) confirmed the ownership split, the destroy-before-release ordering and the gate's decisions,
and found no correctness defect: its findings were D1/D2 (documentation that overstated who keeps a
refused range), D3 (a 6c check satisfied by production's early return rather than by the busy
ring), D4 (no accounting coverage of `vb_readback_reserve`'s failed pair), D5 (destroy-before-release
not observable by the fixture) and D6 (a pre-existing garbage-list realloc drop, out of scope). The
security/native-lifetime review found **no High or Medium defect** -- no use-after-free, early
reuse, double release or dangling handle on any release path, and the never-referenced
classifications sound -- with four Low findings, of which L1 (a refused release was counted in
`released`), L2/D1/D2 (wording) and its L3 (= D4) were applied. Applied in one bounded round, with
the frozen hashes above: D1/D2/L2 wording, L1's counter placement, D3's replaced check (shown red
against a release-while-busy mutant instead of by a `vb_pool_gc` early return), D4's new property
10h (12 checks driving the readback reserve's failed pair past the park budget), and D5's event-log
oracle (red against the swapped-order mutant). Not applied, recorded here as limits: review Low L4
(`gate_world` still assumes rather than asserts that no release is parked across worlds) and D6.

**Validation on the frozen tree.** Ten `jsrf_vk_` gates through
`python C:/Users/logic/xbr-mac/ctest-limited.py -R jsrf_vk_`: 10 of 10 passed in 34.48 s, including
`jsrf_vk_suballoc_gate` (10 properties / 335 checks) and the real `jsrf_vk_suballoc_device` case
(0.97 s, it RAN, it did not skip -- byte-exact targets/textures/depth/snapshot through image
churn). Full CTest through the same helper: **96 of 96 passed in 451.21 s**, exit 0, no `Skipped`
and no `Not Run` line, `xbox_guest_meter` green on its first run (no documented-flake rerun
needed); receipt `C:/Users/logic/xbr-mac/n44r-ctest-frozen.txt`. `just check`: exit 0, including
the real retail corpus `entries=126 recompiled_and_matched=126 ascending=yes duplicates=0
xbe_distinct=126 problems=0`; receipt `C:/Users/logic/xbr-mac/n44r-check.txt`.

**N3 bounded stock e2e on the frozen exe** `20261017-152351-649-keyscript-n44r-n3-e2e-val`: stock
`config/key-scripts/e2e.json`, 620 s bound, Vulkan back end, exploratory profile, isolated
disposable save root (verified `disposable: true`), RTX 4070. The invoking shell exported
`VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and `VK_LOADER_DEBUG=layer` and the same two were
passed to the runner, whose `result.json` records the overrides
`['RECOMP_GPU_BACKEND=vulkan', 'VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation', 'VK_LOADER_DEBUG=layer']`.
From THAT run's `jsrf_run.log` (158 loader lines), quoted verbatim:

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

Disclosed, not set by this packet: the same log lists inserted `VK_LAYER_OBS_HOOK`
(`C:\ProgramData\obs-studio-hook\graphics-hook64.dll`) and the driver's `VK_LAYER_NV_present` and
`VK_LAYER_NV_optimus` instance and device layers. Counters: `validation_errors=0`,
`validation_warnings=10` (the known `vkCreateGraphicsPipelines` "attribute not consumed"
warnings), `vsh_misses=0`, `vsh_identity_fail=0`, `vsh_declines=0`, `rc_fallback=0`,
`tex_undecodable=0`, `rb_incomplete=0`, `rb_copies=0`, `icall=0`, `fatal=0`,
`still_rejecting=0`, flips 32 352, draws 2 878 108, submits 95 130, `max_inflight=3`,
`reuse_waits=662`, `drain_waits=94 468`, `splits=62 115`, `dropped_tris=8`, `skipped=0`; memory
`mem_pools=4 mem_blocks=10 mem_alloc_calls=10 mem_ranges=232 mem_dedicated=0 mem_reuse=0
mem_released=2 mem_parked=0 mem_parked_peak=0 mem_parked_lost=0 mem_reserved_bytes=83886080
mem_used_bytes=79101952 mem_free_bytes=4784128 mem_padding_bytes=0 mem_metadata_bytes=23392
mem_alloc_failures=0 mem_bind_failures=0 mem_map_failures=0 mem_rollbacks=0 mem_fallbacks=0`.
Outcome `keyscript_complete`, `script_ok: true`, termination bounded and deliberate
(`killed_at_deadline_or_failure: true`, `exit_code 1`, `ran_s 620.4`) -- not a natural exit and
not a fidelity proof. Ordinary validation is not synchronization validation: the N4.3 fixture with
its negative control remains that evidence, and this run claims neither.

**N2, six valid windows: three fresh interleaved baseline/final pairs.** `n44r-pN-baseline` runs
the accepted N4.3 executable `11ce8b03` (its own private preserved build
`C:/Users/logic/xbr-mac/n44-baseline-build/Release` with its own `build-source.json`) through the
private runner copy `C:/Users/logic/xbr-mac/n44-baseline-runner.py` (SHA-256
`85fc92e8acc9ac6b9d8cc5f7613f82b888188a8b75c6c9390b6f25b44a8ae48e`: the repository's
`scripts/key-script-run.py` with only ROOT/BUILD pointed at that private directory plus a
`sys.path` insert, so runner, settings, profile and script are identical); `n44r-pN-final` runs the
frozen repair `57f69c7f` through `scripts/key-script-run.py`. Both sides use
`config/key-scripts/garage-bench.json`, the RTX 4070, one game at a time, and a 15 s host sampler
(`n44r-hostlog.ps1` -> `n44r-bench-host.log`, 213 samples). The shared `build/Release` executable
was never replaced by the baseline.

| pair | window | exe | presents/s | p50 | p90 | p99 | max | stalls>100 ms | aliased/increments | host cpu_perf | gpu sm MHz | gpu W |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `20261017-153452-024` | `11ce8b03` | 43.4834 | 22.39 | 24.30 | 27.42 | 55.66 | 0 | 0/2609 | 136.2-138.9 | 1005-1110 | 13.7-14.4 |
| 1 | `20261017-154419-438` | `57f69c7f` | 42.9343 | 22.22 | 26.07 | 35.85 | 53.89 | 0 | 0/2576 | 105.0-113.7 | 1005-1080 | 15.2-16.5 |
| 2 | `20261017-155347-742` | `11ce8b03` | 41.6004 | 23.94 | 25.94 | 28.32 | 50.10 | 0 | 0/2496 | 135.4-138.5 | 1065-1110 | 13.7-14.4 |
| 2 | `20261017-160310-378` | `57f69c7f` | 46.5010 | 21.68 | 23.76 | 26.28 | 54.63 | 0 | 0/2790 | 134.3-136.2 | 1035-1125 | 14.3-15.9 |
| 3 | `20261017-161236-358` | `11ce8b03` | 42.3507 | 23.84 | 25.15 | 27.91 | 57.41 | 0 | 0/2541 | 132.5-136.4 | 990-1080 | 13.8-15.1 |
| 3 | `20261017-162158-902` | `57f69c7f` | 45.1677 | 22.04 | 23.90 | 26.01 | 46.87 | 0 | 0/2710 | 133.5-135.0 | 1035-1080 | 15.5-15.7 |

Medians: baseline `11ce8b03` 42.3507 (41.6004-43.4834), p50 23.84 / p90 25.15 / p99 27.91; final
`57f69c7f` 45.1677 (42.9343-46.5010), p50 22.04 / p90 23.90 / p99 26.01. Six valid windows were
attempted and every one produced a window; no run failed and no startup access violation occurred
in any of them (an observation, not evidence about that pre-existing crash). All six are
`keyscript_complete` with `RECOMP_GPU_BACKEND=vulkan`, `max_inflight=3`, 0 stalls over 100 ms and
0 aliased polls. The three final windows' allocation/sync counters are identical (the values in
the N3 paragraph above, `mem_used_bytes` 78 967 808 in the bench runs); the baseline executable
predates the allocator's counters, and its ring/readback counters scale with its frames (pair 1:
submits 80 330, `rb_batches` 27 425, `rb_packs` 54 849, `rb_incomplete=0` against the final's
83 560 / 28 500 / 56 999 / 0). Read as an observation: the host's power state was matched within
1.5 % of `cpu_perf` in pairs 2 and 3, and those windows read 46.5010 and 45.1677 against 41.6004
and 42.3507 with p50 1.8-2.3 ms lower, while the one mismatched pair (the final window in a lower
boost state, 105-114 against 136-139) read 42.9343 against 43.4834. **No gain, no regression and
no power-state causality is claimed**: the cause of that side difference -- a real effect, window
order, or a state variable this harness does not measure -- is not established, and the same
baseline executable measured 41.37 and 23.13 presents/s within one session in §31.40. Everything
here is a stub-free real-device observation at one screen, on the RTX 4070 only.

**Limits.** Every injected failure in the gate and in the mutants is a stubbed driver return
value: no real pool exhaustion, device loss or host OOM was reproduced, so the refusal path is
decided by the fixture and, in production, is unreachable at scale (at most one entry from
`vb_stream`'s dead-recording branch can ever park in a sound process). The 6a/6b byte-identity
checks cannot decide an eager release under this fixture (it writes nothing into the released
range before the next take), so the hand-out checks are what decide it. Two review Lows are left
as recorded limits: `gate_world` assumes rather than asserts that no release is parked across
worlds, and `vb_discard`'s realloc-failure path still drops a whole deferred entry while the back
end keeps running (pre-existing, host-OOM only, the range stays counted live, outside this
repair's contract). The pre-existing startup access violation and the pre-existing guest-write
publish bounds concern are untouched. No persisted pipeline cache/R4, N6 or R5 work was started.

**Owner boundary (2026-10-17).** The owner amended the run's authority: "stop when N4 is
completed". Continuing through N6 and R5 is overridden; only the remaining N4 item -- the
persisted pipeline cache/R4, if Codex dispatches it as its own bounded operation -- may follow,
and once every N4 gate is independently accepted and its commits and pushes are complete the run
records closure and stops. No N6, R5 or later plan item without a new owner instruction. Durable
amendment: `C:/Users/logic/.omp-bridge/reports/poison-jam/owner-stop-after-n4-20261017.md`. This
adds no operation slots, no repair cycles and no planning phase, and accepts no work.

### §31.43 N4.4 accepted: an independent review of the repair (2026-10-17)

**Verdict: accepted** at toolkit `03aaae8` / game `e4cb724`. The reviewer was the Mac-side Claude
session (Opus), a different model than the OMP implementer, working from both commits' full diffs
and §31.42's receipts. It confirmed the four repair rules against the source:

- the fail paths of `vb_host_buffer`/`vb_device_buffer` destroy the buffer before
  `vb_mem_give_now`;
- `vb_image_create`'s bind failure releases through `vb_mem_give_now`, then discards the image;
- `vb_readback_reserve`'s abandoned pair goes through `vb_garbage_free_now`;
- `vb_mem_give` counts `released` only on an admitted release, and its refusal branch reports,
  stops the back end and returns without clearing the caller's `VbAlloc`.

The deciding gate (pre-repair red on 35 of 336 checks, repaired green on 10 properties / 335
checks, six private mutants red) and the 96/96 CTest stand as §31.42 records them.

**Low findings, recorded as limits, not repaired:**

1. **The refusal's announcement is unreachable in production.** `vb_stop` returns at once when
   `g.failed` is set. The only production caller that can reach the park or refuse branch is
   `vb_stream`'s `if (g.failed)` branch, so the back end has already failed there and `vb_stop`
   neither announces nor dumps the stats line. The ownership rule still holds.
2. **The park list is dead in live runs.** With every busy-time rollback now never-referenced,
   the N3 e2e and the six N2 windows all report `mem_parked=0`, `mem_parked_peak=0`. The bounded
   list is a safety net decided only by the fixture.
3. **`VB_GATE_HAS_REFUSE_PARK` is a substring probe** of the toolkit source, not a structural
   check, so a comment containing the marker would satisfy it.

**Scope.** N4.4 is complete; N4 still needs the persisted pipeline cache/R4. The owner's
"stop when N4 is completed" bounded the OMP run. The owner has since directed this session to
finish N4, merge upstream xboxrecomp v0.14.0, and port reJSRF's applicable fixes, each as its
own gated step.

PUSHED_TO: origin / BRANCH: main / COMMIT: 03aaae814dcfb28c6c283eb1856dcf2c1df3ce0a / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward ecbaca4..03aaae8, toolkit pushed first, clean and synchronized; 2 outgoing blobs (largest 239 744 bytes), 0 secret hits; upstream untouched (push URL DISABLED).
PUSHED_TO: origin / BRANCH: master / COMMIT: 2a8b9ea (with e4cb724 repair gate, 3a4155d helpers) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 02945a5..2a8b9ea, clean and synchronized; 10 outgoing blobs (largest 424 527 bytes), 0 secret hits, no `game/` or retail-derived path.

### §31.44 N4 complete: the persisted pipeline cache and R4 (2026-10-18)

**R4, measured first.** `pipelines=` counts creations. It reads 2 / 41 / 42 (start / middle /
end, max 42) in `20261014-165814-809-keyscript-e2e-vk-v5r`, and 2 / 42 / 42 in
`20261017-152351-649-keyscript-n44r-n3-e2e-val`. That is far below `VB_MAX_PIPES` 256, so the LRU
never evicts and the limit stays.

**The change (toolkit `6c3cd84`, `nv2a_vk_backend.c`).**

- **The knob.** `RECOMP_VK_PIPELINE_CACHE`:
  - unset on Windows, the cache lives in
    `%LOCALAPPDATA%\xboxrecomp\vk-pipeline-cache-<vendor>-<device>.bin`;
  - `off` or `0` disables it;
  - any other value is a path, used as given.
- **The file** is our header (magic, version, length, CRC-32) around `vkGetPipelineCacheData`'s
  blob.
- **Load rejects** (counted, and the cache starts empty): a short file, wrong magic or version,
  over 64 MiB, a length mismatch, a CRC mismatch, a malformed Vulkan header, or a different
  vendor/device/UUID. A file the driver refuses is retried empty. A missing file is a quiet cold
  start.
- **Saving.** Runs end by deadline kill, so `vb_flip` saves at most every 10 s after new pipelines:
  - `<path>.<pid>.tmp` is opened first;
  - the image is checked with the loader's own parse;
  - then it is renamed over the target.
- **Save failures.** A blob the loader would reject is never written. Three failures in a row stop
  saving for the run. Log lines are capped at 8.
- **Reporting.** The load prints one line, `[GPUVK] pipeline cache: <path> loaded=<bytes>|cold|rejected(<reason>)`
  or `off`. The stats line gains `pipe_evictions`, `pcache_loaded`, `pcache_saves`,
  `pcache_save_failures` and `pcache_rejects`.

Nothing new is translated or compiled: the driver already built these pipelines from the
ahead-of-time SPIR-V.

**Gates (game `2253927`), written by a worker other than the implementer:**

- **`jsrf_vk_pcache_gate`** drives the production load, save and eviction paths with stubbed
  Vulkan: 11 properties / 202 checks.
- **`jsrf_vk_pcache_device`** (lavapipe) decides that lavapipe's malformed blob is refused before
  it is written. Lavapipe reports a 32-byte size, then writes 20 bytes with a truncated UUID.
  It reports no creation-feedback hits, so the positive reload is not demonstrable there, and the
  test says so.
- **`jsrf_vk_pcache_device_hw`** (RTX 4070) decides the round trip:
  - cold run: 4 of 6 creation-feedback hits (the NVIDIA driver serves some pipelines from its own
    cache);
  - after reload: 6 of 6 hits, `loaded` 38 484 bytes, 0 rejects.
- **Every other real-backend test** runs with `RECOMP_VK_PIPELINE_CACHE=off`.

**Deciding mutants** (each red; the unmodified source is green):

- skip the load (also red on hardware: 4 of 6 hits);
- skip the CRC check;
- drop the eviction count;
- save without the dirty check;
- write the target in place;
- skip the pre-write parse;
- no failure latch;
- a default name without the device ids.

**Review.** An independent code review (Opus; the implementer and the test author were Sonnet)
found no P0. It found two P1s:

- the default file was shared by every device and process;
- the lavapipe case passed without proving a round trip.

It found three P2s:

- a saved blob the loader would reject;
- gaps in the atomic-replace and returned-size tests;
- unbounded failed-save retries and logging.

It also listed P3 nits. All were fixed in one round, then re-gated. Not done:

- a faster CRC (performance is not a goal; about 1.8 ms per MiB at the 64 MiB cap);
- multi-level parent directory creation for a user path.

**Validation on the final tree.** Full CTest 99/99 and `just check` green; executable
`791aeaaeccad5c6da96693a9524d1434dfc18cbffed748246ef9eccc86084622`.

**N3 e2e** `20261018-104754-956-keyscript-n4c-n3-e2e-val2`: stock `e2e.json`, Vulkan on the RTX
4070, cache `off`, exploratory. The loader's own lines, quoted:

    [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
    [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

Result: `keyscript_complete`, `validation_errors=0`, `validation_warnings=10` (the known
"attribute not consumed" warnings), `vsh_misses=0`, `vsh_identity_fail=0`, `pipe_evictions=0`,
icall 0, fatal 0. The bounded deadline kill is deliberate.

An earlier run with the cache on, `20261017-184949-085-keyscript-n4c-n3-e2e-val`, has the same
counters and `pcache_saves=8`. Its log holds no loader lines at all, so validation is not witnessed
there, and the cause is unknown.

**N2: three interleaved cold/warm pairs** on the RTX 4070, `garage-bench.json`, same executable.
Cold runs use a fresh file each; warm runs use the file the cache-on e2e wrote (921 731 bytes,
`loaded=921731`, `pcache_warm: true`).

| pair | cold | warm |
|---|---|---|
| 1 | `20261017-190011-278` 13.67 | `20261017-191013-480` 13.80 |
| 2 | `20261017-192015-957` 13.80 | `20261017-193018-365` 13.73 |
| 3 | `20261017-194020-856` 13.73 | `20261017-195023-318` 13.73 |

Medians: cold 13.73 presents/s (p50 about 72 ms), warm 13.73.

No gain is claimed: all 41 pipelines are created long before the benchmark window, so a warm cache
can only shorten start-up. The absolute rate is a third of §31.42's 42-46. The host was the cause,
not the code: the preserved N4.3 baseline executable, which read 41.6-43.5 in §31.42, read 12.63
the next morning (`20261018-103553-919-keyscript-n4c-drift-baseline`). The host state behind that
drift is not measured.

**N4 is complete:** frames in flight, batched readback, narrowed barriers, sub-allocated memory,
and the persisted pipeline cache with R4. Next, by owner direction (2026-10-17):

1. merge upstream xboxrecomp v0.14.0 and regenerate;
2. port reJSRF's toolkit discovery fixes;
3. triage reJSRF's game-side render and lock fixes against this executor and back end.

N6 and R5 remain in the plan.

PUSHED_TO: origin / BRANCH: main / COMMIT: 6c3cd84c8e5853cc4ca39205526da2a45b5cc9b4 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 03aaae8..6c3cd84, toolkit pushed first, clean and synchronized; 2 outgoing blobs (largest 255 106 bytes), 0 secret hits; upstream untouched (push URL DISABLED).
PUSHED_TO: origin / BRANCH: master / COMMIT: 1dd0b11 (with gate 2253927) / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward de44496..1dd0b11, clean and synchronized; 8 outgoing blobs (largest 431 195 bytes), 0 secret hits, no `game/` or retail-derived path.

### §31.45 C3: static gates pass; AC’97 startup gate remains open (2026-10-18)

The owner resumed cleanup C3–C4 and D1–D5 directly on Windows. N4 remains complete
(§31.44); this does not reopen it. Toolkit `merge-v0.14.0` is `ee2e0cd`: merge
`0828592`, SetAA test seams `a4cd862`, and low-RAM surface-write guard repair
`ee2e0cd`. Toolkit main/origin remain `6c3cd84` until the C4 acceptance gate.

**Regeneration inventory.** The inherited full pass has 5,922 generated functions
and 9,078 dispatch entries (old: 5,740 and 8,917). Its 302 dropped functions are
classified in the ignored C3 archive; 484 are added. The C3 seeds restore 101
alias-folded helpers and `0x101A70`, retire ten stale manual pins, repair the
`0xF800D`/`0xF8041` internal labels, and widen `0x20760`, `0x2D080`, `0x4B0E0`.
The reJSRF #93 leads `0x162490`, `0x1780D0`, `0x7BDD0` resolve; `0x77000` does
not reproduce. Zero unresolved full-pass stubs is distinct from the 72 explicit
recovery traps that remain fatal. Final local recovery count is 3,391 with zero
duplicate definitions; the seven further removals below explain the change from
the inherited 3,398/73 inventory.

**Gate-driven repairs.** The first guarded build linked the game but failed the
11c1 fixture on `xbox_PortOut`. The recovery generator and fixture now use the
merged toolkit's unified `xbox_PortIn(port,width)` / `xbox_PortOut(port,value,width)`
interface. Unexpected fixture OUT aborts; the deciding 11c1 path does not use it.
The fixture reports 2,197 checks. No runtime stub or successful device response
was added to the game.

The first complete CTest run passed 96/99 in 570.58 s. The failures were the
FCMOV old-lift mutation matcher, recovery-span ownership, and hidden-entry
historical replay. The positive FCMOV behavior was already correct; its negative
control now matches the merged lifter's operand-bearing comments and still
compiles/runs the old broken lift, requiring failure.

The span gate exposed a genuine false-recovery chain: `0x41E24`, `0x41E40`,
`0x41E48`, `0x41E4C`, `0x41E50`, `0x41E54`, `0x41E5C` are internal instructions
of the original `sub_00041BF0` (`0x41BF0..0x4259C`). They are retired from the
recovered/manual manifests, not widened into invented functions. The corresponding
aligned data values at `0x1C42F8..0x1C4318` encode count-one NV2A packets (methods
`0x1E24`, `0x1E40..0x1E5C`), not executable-entry evidence. The full generated owner
and its real `0x41E5C`/`0x41E9C` branch labels remain. Regenerating recovery removes
the now-unreferenced `0x41E9C` trap; actual unresolved dependencies stay fatal.
The stale `0x20760` known-open allowance also shrank because its C3 span is fixed.

The hidden-entry self-check now reproduces historical missing resolution in all
three sources: recovery manifest, generated-body set, and dispatch set. Current
full-pass bodies otherwise legitimately change those controls from HIDDEN_ENTRY
to SHADOWED. Normal gate evaluation still uses the actual tree, and SHADOWED
remains a gating class; default CLI output now names its deciding addresses.
A different worker authored the new span and CLI regressions.

**Provenance and overrides.** The strict baseline/manifest still protect 17 local
outputs. The absent unresolved-stub file (the full pass emits it only for a
nonempty unresolved set) and obsolete 11b4b4 fixture are retired with dated reasons;
the two existing AOT shader-corpus C/H files previously omitted from the inventory
are now protected. The independent test inventories actual C/H outputs and detects
omission from both records; presence, hash and ABI-marker checks remain decisive.
No shader corpus was rebuilt during this migration.

Fifteen new upstream environment variables blocked override drift. Their definitions
are now classified in the run-profile document. Only exact `RECOMP_PORT_FIELD_PIN=1`
adds the synthetic field pin and is exploratory; inert spellings remain inert, and
duplicate keys remain UNKNOWN. Host input, save-root and SDL presentation/audio
controls are documented without enabling them or changing any saves. Independent
profile gates passed all 56 tests. Private controls: old classifier 4 failures/8;
final 0; missing classification 4, truthiness 8, case-sensitive key 4, trimmed value
2 assertion failures; restored 0, with no test errors or skips. Mutation copies are
outside the repositories under `xbr-mac/c3-profile-mutations`.

**Measured rebuild requirements.** The successful-path regeneration stages total
715.0 s: toolkit disasm 194.2, funcid 1.3, ABI 1.0, game disasm 284.0, full regeneration
199.2, boundary relift 0.5, recovery 34.5, ABI deltas 0.3. Recorded attempts total
1,158.1 s, including failed/intermediate attempts: this is not one flawless timed
end-to-end run. It requires the original local XBE, fresh toolkit/game analysis,
reviewed seeds, the ignored runtime/recovery output and offline Vulkan corpus tools.
The archived storage sample is 49,596,384 B recomp output, 65,205,668 B game analysis,
67,212,320 B toolkit disassembly, 3,159,365 B funcid, 2,490,306 B ABI and 1,018 B
recomp metadata, with 35,115,036,672 B free. C4 must update AGENTS and the ledgers
with these attempt boundaries and current requirements.

**Current gate state:** final repaired guarded build passed (54 targets, 99 CTests),
zero C4310/compile/link errors, 18 MSB8028 intermediate-directory warnings. Executable
SHA-256 `01a8cda689d9b0c6fc4f30c3fb6a369d292f7e41ce770d3431c0b87860133c28`.
`just check` passed, including all 126 distinct retail vertex programs. Full
limited-rights CTest passed **99/99 in 666.04 s**, without skips or reruns. The
cache-off smoke failed at startup as detailed below. C3/C4 remain unaccepted and
neither repository's cleanup code is pushed.
Existing limits remain: rare recovered fall-through/ABI findings are not proven safe
by zero runtime hits, and exploratory completion cannot prove hardware fidelity.


**Independent mutation evidence.** The separately authored span and hidden-entry
regressions pass on final and restored source. Four private controls each fail
with one assertion failure, zero errors and zero skips: omit generated-body and
dispatch resolution from historical replay; hide SHADOWED deciding addresses;
remove SHADOWED from GATING; restore the seven false recovered/manual entries.
Final checker SHA-256 `c652622d7d3970153a88a74d7685d0d90afe2f6238a25c07f114e49ace5f677a`,
hidden test `f62ab09c0af5bdf07849a98fda70863e747c905c393f81a384d8f076544a4af9`,
span test `a535def7ddb3fb70a4c6aedeff7ab917e4db135df614e49bc4bb553a0aa3cf96`.
The private relocated checker copies have separate hashes in the ignored receipt;
these are not claimed as the production file hashes. No shared source was swapped.

**C3 runtime gate FAILED.** Run `20261018-141756-714-keyscript-c3-regen-e2e-vk`
used the executable above, RTX 4070, stock `e2e.json`, cache off, exploratory profile.
Its requested budget was 620 s, but step 0 failed its 300 s title deadline:
`keyscript_incomplete`, 301.2 s, zero frames/presents, ICALL 0, fatal 0. The runner
then killed only its own PID 35724. The save-root witnesses both verify the private
run directory. Absence of a fatal with no progress is not a pass or coverage proof.

A PID/image-verified diagnostic capture suspended that process for 0.825 s and
resumed it successfully. Frozen thread context places the main thread 31428 at
host `0x7FF61158EEC0`, module base `0x7FF6107F0000`, RVA `0xD9EEC0`.
The archived linker map resolves `sub_001A6F52+0x190`; llvm-symbolizer against the
archived executable/PDB resolves `recomp_0005.c:56083`, the backedge of
`loc_001A6F88`. This diagnostic minidump holds host contexts/stacks/module data,
**not a canonical guest-RAM capture**: it cannot establish guest memory values or
satisfy the dump-mapping gate. No guest-RAM claim is made from it.

Original XBE instructions at `0x1A6F6F` write reset bit 2 at the selected channel's
`0xFEC0010B`-relative register, `0x1A6F7F` reads once into CL, and `0x1A6F88` tests
CL with `jne 0x1A6F88`. That loop does not reload the register. The old backed-up C
and current C function differ only by the regenerated `RECOMP_BACKEDGE()` call.
The archived **C2** executable's native code reads/masks the byte at
`0x140D19C98..0x140D19CA5` and proceeds directly to the following code: its empty
register loop is absent. The current native executable retains the loop at
`0x140D9EEC0..0x140D9EED7`, including the conditional preemption call. This establishes
that old C2 completion did not test that original loop's reset-completion contract;
it does not establish every cause of the device state that now enters the loop.

**Next bounded rung: C3-RR diagnosis, before C4.** Establish the actual register
contract and current ownership against original instructions and primary device
sources, compare the two native binaries, and propose a minimal model repair plus
an independent behavioral/mutation gate. Preserve the §1 AC'97 hunk ruling and
L20 codec-ready relation. Do not remove backedge preemption, force CL to zero,
turn on FIELD_PIN or AC97_READY, or arm the dormant reset trap as an unexplained
completion shortcut. Any change to that trap/model requires its own explicit
packet and compatible handling of tick writes and shared single-step ownership.
No runtime repair or C4 acceptance has happened yet.

### §31.46 C3-RR diagnostic review: cause confirmed, proposal withheld (2026-10-18)

The read-only OMP operation `op_0mvec8cn956cffcfef0e00e1d` confirmed §31.45's
AC'97 startup cause and changed neither production tree. Its ignored report is
`logs/reports/c3-rr-diagnosis-2026-10-18.md`. C3 and C4 remain unaccepted.

Codex independently checked the source, native evidence and pinned primary device
sources. The repair proposal needs correction before implementation:

- `AddVectoredExceptionHandler(1,...)` inserts the newest handler first, per
  Microsoft's API contract. Registering the game handler before layout init does
  not establish the claimed game-first logging storm after a later AC'97 handler
  is registered. Handling independent of order may still be useful, but requires
  its own evidence. The report and existing equal-priority ordering comments
  must not be treated as proof of registration order.
- Pinned xemu `reset_bm_regs` retains prior CR masked by `0x1C`, clearing both run
  and reset bits, and resets the channel's descriptor/index/count/status state.
  The proposal's test explicitly preserving RPBM on reset contradicts its source.
  Clearing only RR after depositing the new byte also loses prior control state.
- Making a shared page writable for one thread's single step permits another
  accessor to miss the trap and latch reset still set. Calling that exposure low
  does not close the exact startup failure contract. Atomic GC/GS updates alone
  do not fix missed access routing.
- Original `0x1A73F8..0x1A740D` reads exactly two startup dwords, `0x10` and `0x70`.
  The adjacent `0x2000` is not proven to be a third table element. Pinned
  `ac97_int.h` defines PI/PO/MC/SO indices 0/1/2/7; valid register ownership is
  established from those definitions and actual caller bounds, not a stride sweep.

Primary references:
[Microsoft VEH insertion](https://learn.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-addvectoredexceptionhandler),
[pinned xemu AC97](https://github.com/xemu-project/xemu/blob/2799183ecc5119269be01c340d0c9465dbb04d02/hw/audio/ac97.c),
[channel definitions](https://github.com/xemu-project/xemu/blob/2799183ecc5119269be01c340d0c9465dbb04d02/hw/audio/ac97_int.h).
The AC97 source hash agrees with §3's pin (`bd2a95fd...ee554b`). These references
supply device behavior; no GPL source is being ported into this MIT project.

The next bounded operation corrects the report/proposal and deterministic gate
contract, without code changes. Codex advice includes assessing the existing
APU/OHCI MMIO decoder with permanently inaccessible guest mapping, private backing
and serialized model access, so no shared writable window is needed. This is a
candidate requiring opcode/width/bounds, reentrancy, reset-state and L20/tick
analysis; it is not a tested implementation or permission for runtime translation.
A later code packet needs a different test author and old/final/mutant/restored
proof, followed by the still-open C3 runtime gate. No C4 work or push is authorized.

**Reviewer identity verified.** The report initially called its runtime identity
unobserved. Codex located native child session `01a150f5-3cf8-72a3-9fe9-536576568b5e`
(`ReviewC3Rr`), linked to the primary task call naming agent `reviewer`. It records
`workbuddy-ai/deepseek-v4.1-flash`, `resolvedModelIsFallback=false`, thinking level
`max`; all 64 assistant messages use that provider/model. This closes the routing
evidence gap without claiming the review was correct. Exact extraction and source
hashes are in the ignored Codex review receipts.

### §31.47 C3-RR corrected proposal review and bounded repair (2026-10-18)

Operation `op_0mveed2p730a66d6607a1a913` completed read-only. Codex fetched both
repositories and verified clean game `e5c16a2` (ahead four of origin `699bc46`)
and toolkit `merge-v0.14.0` at `ee2e0cd`; main/origin remain `6c3cd84`.
No new build, test or runtime result follows from a design report.

The corrected report fixes VEH ordering and eliminates the proposed shared
writable-page window. Its CR prior-state rule is now correct. It is still not
an accepted implementation contract:

- Eight array slots do not establish eight channel controls. The pinned Xbox
  model defines PI/PO/MC/SO at indices 0/1/2/7. Adjacent table data supplies no
  additional caller bound. Reserved locations retain their previous storage
  behavior; they must not receive invented reset semantics.
- Reset's observable register effects cannot be omitted merely because the
  resulting halted status changes a guest branch. Separate register reset from
  absent DMA/audio work. Do not manufacture DMA progress to recover completion.
- The shared decoder calls read and write separately for a memory RMW. Locks
  inside those callbacks do not serialize that whole operation with the tick.
  Use one leaf transaction lock across decoding/service and each host update.
- The proposed removal of legacy A2H/AC97 overlap fixtures is unnecessary to
  introduce a different production owner. Preserve their assertions for this
  bounded change, keeping the dormant legacy trap unarmed in production.
- A non-reset write of `0x1D` already reads back `0x1D` before repair; all six
  proposed rows cannot be red. Prior RR-set state is not constructible through
  the corrected public CR write path. Distinguish valid live sequences from
  private model-state injection. Archived RVA `0xD9EEC0` identifies the old
  executable only, not a mandatory address after relinking.

Codex additionally consulted the [Intel ICH4 datasheet, 290744-001, May 2002]
(https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/82801db-io-controller-hub-4-datasheet.pdf),
§§14.2.1–14.2.7, printed pp.482–486. Its related NABM interface corroborates
reset of channel registers while retaining interrupt enables, self-clearing RR,
and halted status. It is not an MCPX channel-address specification; Xbox mapping
still comes from the pinned xemu source in §31.46. Reset while running is
undefined in Intel's contract; xemu's deterministic handling is an emulator
policy, not hardware fidelity evidence. No GPL implementation is being ported.

The next authorized packet implements only C3-RR using Codex's corrected
contract, with independent test authorship, real Windows ownership, fail-closed
decoding, deterministic concurrency controls and quoted old/final/mutant/restored
results. Preserve POSIX's current mapped GC/GS path unless it is migrated fully;
moving only its tick to private state would disconnect guest writes. Preserve
L20 without adopting unconditional ready bits. Register-visible reset must not
stand in for DMA data, interrupts or audio processing. A new stop must be
classified and returned, not suppressed. C3/C4 remain unaccepted; no cleanup
push is authorized.

**Second reviewer route independently checked:** native child
`01a15126-9b03-7396-8225-3782974340d0` records reviewer/slow,
`workbuddy-ai/deepseek-v4.1-flash:max`, fallback false. At Codex inspection its
120 assistant records all use that provider/model (the report's count 66 was
stale). Its last assistant record has stop reason `stop`. JSONL SHA-256:
`4cece28917d8d6f1d0a1559d72042575532ec734797440ccd79f3f7d82ca8410`.
Configured routing and a completed review do not prove the design correct.

### §31.48 C3-RR repair: the AC'97 NABM owner (2026-10-18)

The bounded repair for the cause established in §31.45–§31.47. Toolkit
`src/platform/mmio_decode.h` and `src/kernel/xbox_memory_layout.c`; the game gains only a test
module and records. No game production source change, no push, C3/C4 still unaccepted.

**The decoder is factored from the service.** `mmio_decode(ctx, mmio_insn_t*)` fills a descriptor
(form, width, direction, operands, the memory operand's EFFECTIVE START, instruction length) and
mutates nothing; `mmio_execute` performs the access and advances `Rip`; `mmio_emulate` is the
two-line composition, so OHCI's call path is unchanged. Unsupported opcodes, register-only forms
(`mod == 3`), segment/LOCK prefixes and ADC/SBB return 0 with no side effect and no `Rip` advance.
The effective address is computed, never assumed to be the fault address, because a boundary fault
names the later page. Three defects were fixed in the same code: the `mod == 0` SIB whose base field
is 5 did not count its disp32; the SIB base dropped REX.B; and the two-operand ALU direction (CMP's
operand order, hence the flags a following jcc reads) is now carried explicitly. The first two were
found by the independent gate's bring-up, the third by review of the refactor.

**The owner.** The 4 KB page at guest `0xFEC00000` is made `PAGE_NOACCESS` and every access is
decoded and applied to a private register file under ONE leaf lock that spans the whole decoded
instruction (including a read-modify-write's read and write) and each host transaction. State, lock
and handler are live before the page is protected, and the page becomes inaccessible before
`xbox_Nv2aAckStart()` creates the ack worker, so the worker's first tick already runs against the
model (see §31.49). `[AC97] serviced/refused/max_service_us` is one bounded counter line (at most
once a second, only when a counter changes); the counters themselves are the record.

Modelled: the four defined bus-master blocks (PI/PO/MC/SO at page `0x100/0x110/0x120/0x170`) — an
RR-carrying control-byte write keeps `prior_cr & 0x1C` (RR and RPBM cleared, interrupt enables
retained), zeroes BDBAR/CIV/LVI/PICB/PIV and leaves the channel halted (SR = DCH, read-only); a write
without RR stores `value & 0x1F`; an SR byte keeps its read-only bits (DCH/CELV) and clears
FIFOE/BCIS/LVBCI on a write of 1, an LVI byte keeps five bits, and a dword BDBAR clears its two low
bits (all three rules are the pinned source's). `GLOB_CNT` is stored as written and `GLOB_STA` is
stored as written except bit 8, which is its bit 1 level (L20), re-derived on every write whose
bytes overlap either global and once per tick. Everything else in the region keeps its previous
storage behaviour, including the reserved CR-shaped bytes `0x3B/0x4B/0x5B/0x6B`, and `peek_readable`
declines the page. Deliberately absent: any DMA engine, descriptor fetch, CIV/PIV movement, audio
output or interrupt delivery — so nothing manufactures progress, and a channel is halted because the
engine behind it does not exist. POSIX keeps the mapped aperture and the tick; the legacy dormant
single-step trap, its seams and the A2H overlap fixture are untouched and never armed in production.
The run-profile admission record and its limits are in `docs/jsrf-run-profiles.md`.

**Independent gate (different author).** `tests/ac97_reset_gate.c`, 89 checks, registered as ONE
CTest case (`jsrf_ac97_reset_gate`; the earlier duplicate same-command registration was removed
because it added no coverage). It drives the production path — real faulting stores and loads at
guest `0xFEC00xxx` against the protected page, with `VirtualQuery` evidence of the protection — and
uses the test seams only for state no public write can produce (a prior control byte with RR set, a
tick-visible split between the globals, and pre-arm/post-shutdown misuse). Deciding rows cover the
RR reset contract and its prior-versus-incoming discrimination, the complete reset register set, the
reserved CR-shaped bytes staying plain storage, SR read-only plus write-one-to-clear, the LVI
five-bit mask, BDBAR's `~3` dword mask, `GLOB_CNT` plain storage, `GLOB_STA` storage plus L20
(change, clear, partial and overlapping writes), the tick re-deriving both directions, the
two-thread serial-outcome rows, and the refusal/refusal-counter rows. Measured final
`checks=89 failed=0 harness_errors=0 skips=0`; restored is identical. The earlier 62-check,
two-case, five-red, nine-mutant and "101/101 twice" claims in this uncommitted draft were superseded
by the resume packet (§31.49) and must not be cited. The gate also caught, in its own authored
source, the C2-era `[reg+disp]`-only probe discipline that exposed the SIB bug.

**Gates.** Final resume-packet receipts are in §31.49: guarded build 55 targets, 100 CTests,
0 compile/link errors, **no C4310**, the 18 known MSB8028 (the three legacy
`xboxrecomp/tests/a2h_slotw_fixture_test.c` warnings were not re-listed because that project was not
recompiled; no new warning came from a changed translation unit), `just check` exit 0 (its
`check-stack-depth.py` reports 58 non-failing `SUSPICIOUS` adjudication items, pre-existing), and
full limited-rights CTest **100/100 in 526.23 s**, no skips. Only one archived full-suite run is
independently confirmed; the earlier "101/101 twice" claim is withdrawn, and the removed duplicate
registration is why the count is 100 rather than 101. One stock Vulkan smoke,
`20261019-002247-673-keyscript-c3-rr-resume-e2e-vk`, executable
`9f6c43b62c7e89762154646890927f14c09146ad50207cf31a8e8ec97c690b5e`, cache off, private saves,
11.8 s.

**The smoke stop, classified.** The AC'97 stall is gone: the owner arms, `[A3A] ac97 witness:
gc=0x00000002 gs=0x00000100` shows the codec-ready relation working through the model, and the run
reaches 11.8 s of guest work with `[AC97] serviced=40 refused=0 max_service_us=0` — forty decoded
AC'97 accesses, none refused — where the old build was stuck at ~3 s with zero frames. It then stops
on **`[ICALL] invalid target 0x00700010 tid=676 esp=007BFF9C return=00193EB7`** (exit `0xE0424943`,
0 frames, 0 fatal). Original-XBE bytes read for this packet: the return address belongs to the code
block at `0x193E40`-`0x193EE1`, reached by the `jmp 0x193e40` at `0x193E32` from the body whose entry
is `0x193D90`, inside the XBE's **D3D** section (the statically linked XDK D3D8 library;
`0x18CB40..0x19E348`), not the DSOUND section (`0x19E340`+). `functions.json` records that entry as
ending at `0x193E34`, so the executed block lies in the recorded gap and is not attributed here to a
separately named function. In that block `esi` is the `ecx` this-pointer, `mov eax, [esi + 0x1c4]` is
at `0x193E62`, the value is tested for zero, and `call eax` is at `0x193EB5`; the field is read after
the GPU handshake loop at `0x193E40-0x193E50` and the indirect call at `0x193E5C`
(`call [0x1c4014]`). The entry at `0x193D90` is called from `0x194300`, `0x194A72`, `0x194EEF` and
`0x196C0B`, all in the same section.
`0x00700010` lies inside the runtime's own fake guest RW-data area (`FAKE_RWDATA_VA` = `0x00700000`),
i.e. a data address, not code. What installed that value in the field is **UNPROVEN** (the dumped
`.text` disassembly does not cover the D3D section, and the result proves an invalid target, not its
cause); the failing access is guest RAM on a worker thread, not the NABM page, and the counters show
no refused or misplaced AC'97 access. The archived C2 build reached the title with 0 ICALL, but that
comparison varies the generator and the toolkit together, so it does not establish causality;
whether the truthful halted SR state changes this D3D8 path is equally UNPROVEN. The control that
would settle it is a diagnostic build holding SR at zero, which the packet forbids as a fix; no such
ablation was built. Per the packet this is a **new horizon to classify, not to suppress**.

**Runtime identity.** The configured reviewer ran as agent `reviewer`, role `slow`, resolved
`workbuddy-ai/deepseek-v4.1-flash:max`, `resolvedModelIsFallback: false`, thinking level `max` — read
from its native child session
(`~/.omp/agent/sessions/-Repos-poison-jam/2026-10-18T21-28-07-043Z_01a150ea-2e83-733d-bb86-5d47e4a37e57/ReviewAc97Repair.jsonl`,
SHA-256 `286b7051e644da5fb87671fde3f38c53b1021634e37fc57c9e011c530ca34e90`), not guessed from the task
return object. It returned before the owner's pause: it confirmed the decoder and owner contracts and
the gate's production path, and **refuted one property** — `ac97m_arm();`/`ac97m_shutdown();` are
called outside the `#if defined(_WIN32)` block that defines them, so the POSIX build breaks (P1).
That is why this packet committed nothing; its full findings, coverage holes and the first resumption
action are in `logs/reports/c3-rr-pause-handoff-2026-10-18.md`.

### §31.49 C3-RR resume: implementation and gate defects closed; C3 still open (2026-10-19)

The owner resumed the paused operation after an OMP upgrade and authorized one bounded repair of the
confirmed implementation and gate defects only (packet
`logs/reports/c3-rr-resume-packet-2026-10-18.md`). Toolkit `merge-v0.14.0` at `ee2e0cd` and game
master `f17b35c` were preserved; no revert, regeneration, C4 work, push or fast-forward happened.

**Implementation defects closed** (toolkit `src/kernel/xbox_memory_layout.c`):

- The Windows-only `ac97m_arm()`/`ac97m_shutdown()` calls are now inside `#if defined(_WIN32)`
  (`:3807-3809`, `:6155-6157`), so a non-Windows build no longer sees an undeclared function. The
  POSIX mapped GC/GS tick is unchanged (`:1953-1990`), as are the whole-transaction leaf lock,
  permanent `PAGE_NOACCESS`, private state, truthful reset/halting and the legacy A2H fixture.
- The init comment no longer claims the ack worker "is already running": the model, lock and VEH
  exist before `xbox_Nv2aAckStart()` creates the worker below, so its first tick already reads the
  model; POSIX keeps the mapped aperture.
- An unrequested `GLOB_STA` dword W1C/read-only policy was removed with its dead constants
  (`AC97M_GS_WCLEAR`, `AC97M_GS_RO`, `AC97M_GS_VALID`). The approved contract is restored: both
  globals are plain storage, and `GLOB_STA` bit 8 is always `GLOB_CNT` bit 1. One post-write overlap
  test re-derives the bit after any supported width (1/2/4/8) whose bytes touch either register,
  which also covers writes that straddle `0x12B`/`0x12E`/`0x131`; no other bit is masked.
- The fixture seams are fail-safe: `Seed` no-ops and `Peek` returns 0 when the model is not armed
  (before init, after shutdown), so the uninitialised lock is never entered, and both validate
  `size` in {1,2,4} and an overflow-safe in-page range. Signatures and the production path are
  unchanged.

**Gate and mutation receipts.** `tests/ac97_reset_gate.c` (different author; 89 checks, ONE CTest
case; the duplicate same-command registration and its "count stability" claim were removed) now
decides, through real faulting guest stores, the SR read-only/write-one-to-clear byte rule, the LVI
five-bit mask, BDBAR's `~3` dword mask, `GLOB_CNT` plain storage and `GLOB_STA` storage plus L20
(change, clear, partial and overlapping writes), both directions of the tick's re-derivation, and
pre-arm/post-shutdown seam misuse. Every deciding mutation was rebuilt outside both repositories
against the FINAL model, FINAL decoder and FINAL gate source (scratch root
`C:/Users/logic/xbr-mac/c3-rr-mutants-resume/`; manifest with per-copy sha256 of layout, decoder and
gate). Final and restored: `checks=89 failed=0 harness_errors=0 skips=0`. The pre-repair control
(plain storage with the seams intact) fails 21 rows as **value** assertions
(`PO CR reset: expected 0x00, got 0x02`), never as a missing API or compile failure. Each mutant
turns a named predicate red with 0 harness errors and 0 skips: no RR mask (12 rows), incoming
instead of prior state (`expected 0x00, got 0x1C`), omitted reset fields
(`reset BDBAR: expected 0, got 0xDEADBEEF`), reserved-byte masking (`expected 0x02, got 0x00`),
split transaction lock (8/8 runs, both thread-start orders, torn `cr=0x1C` through a pre-created
named-event rendezvous), writable page (`Protect=0x4`), forced L20, lost L20 (write path and tick),
tick-sync no-op (both new tick rows red), accepted out-of-page access (`refused 0 -> 0`,
`serviced 149 -> 150`), and the re-added global-status W1C policy
(`expected 0x0000FEFF, got 0x00000000`). Removing the seam guards crashes before any assertion and
is reported as such, not as a predicate. Evidence boundary: the two tick rows are private-state rows
(the split is not constructible through the public write path) and are guarded against the
background ack worker; the tick-sync mutant disables the derivation in both the fixture and worker
ticks. They therefore decide that the derivation exists, not which tick performs it, and no
write-versus-tick divergence mutant was built (no motivating defect). Decoder `mmio_decode.h` sha256
`5ef7de18722c11b2f53b9cd9cf2773b42bf76ce811e0432565ba02e916b2f161`; production layout
`da6273251a41b98314625f2608a2b111e7491896e9db42768e596bc550d775ce`; gate
`d2cbf0b06ac39aec6dfb313ef3cc7ed3fa27755fa37f62238420a95071ac47d6`.

**POSIX.** With a minimal local WSL Ubuntu toolchain (gcc 15.2.0, libc headers), the ACTUAL
`xbox_memory_layout.c` compilation unit was compiled with the toolkit's own non-MSVC policy
(`-Werror=implicit-function-declaration`) and include paths. HEAD `ee2e0cd` and the final source both
report the same 41 pre-existing errors (Windows shim/types: `MemoryBarrier`, `InterlockedOr/And`,
`GetModuleHandleExA`, `RtlCaptureStackBackTrace`, `IMAGE_DOS_HEADER`, `THREADENTRY32`, `CONTEXT`,
`ProcessorCONTEXT.Rip`, ...), so this translation unit did **not** POSIX-compile before this change
either. The deciding A/B/C control: deleting exactly the two guard pairs adds exactly the two
`implicit declaration of function 'ac97m_arm'`/`'ac97m_shutdown'` errors (43 total). The change
therefore adds no POSIX error and closes exactly P1; a full POSIX build remains blocked by the 41
pre-existing gaps, which this packet does not claim to fix. Receipts:
`C:/Users/logic/xbr-mac/c3-rr-posix/`.

**Review.** A configured reviewer distinct from both authors re-ran the final gate
(`checks=89 failed=0`) and checked the implementation hunks, the gate and every mutant log against
the pinned source; it found no blocker and two evidence caveats (a timing-sensitive lost-L20 mutant
and a manifest quote), both resolved before the final gates: the lost-L20 mutant now removes the
derivation everywhere and the tick-sync mutant is decided by the new rows. Its stated limit: it did
not re-audit the inherited `mmio_decode.h` decoder factoring beyond the REX.B fix. The toolkit commit
carries that pre-pause work with the repair (786 insertions / 120 deletions across both files; §31.48
describes it), so the commit's decoder half is preserved-and-gated, not independently re-reviewed in
this packet. Native route read from
its child session
`~/.omp/agent/sessions/-Repos-poison-jam/2026-10-19T05-12-19-939Z_01a15293-2ee3-7504-ae31-d59ac31dad2c/ReviewC3RrRepair2.jsonl`:
`opencode-go/muse-spark-1.3-contributor:xhigh`, `resolvedModelIsFallback: false`.

**Routing correction from Codex (§31.50):** this was a generic native `task`,
not an invocation of the configured reviewer role. Its name and review assignment
did not select that role. The route above is accurate; calling it the configured
reviewer is not. The Workbuddy reviewer setting was unchanged.

**Gates and runtime.** Guarded build: 55 targets, 100 CTests, 0 compile/link errors, no C4310, the 18
known MSB8028, no new warning from a changed translation unit; `just check` all passed; full
limited-rights CTest **100/100 in 526.23 s**, no skips; `jsrf_ac97_reset_gate` 89/0/0/0 inside it.
One stock cache-off Vulkan smoke (`20261019-002247-673-keyscript-c3-rr-resume-e2e-vk`, executable
`9f6c43b62c7e89762154646890927f14c09146ad50207cf31a8e8ec97c690b5e`, 11.8 s) reproduces the newly
classified `0x00700010` / `0x193EB7` ICALL with `serviced=40 refused=0` and the L20 witness live.
**C3 remains failed and unaccepted**: the reset gate and this device repair are not whole-C3
acceptance, and the ICALL's cause is unresolved. No push was performed.

### §31.50 Codex review: accepted qword stores corrupt data; repair not accepted

Operation `op_0mvest7g30d20b8ae72588329` returned clean local game `a79aaf0`
(including `a06b4b5`) and toolkit `c395db3` on `merge-v0.14.0`; neither was pushed.
Codex fetched both origins, checked source hashes against the refreshed manifest,
and independently ran the final/restored gate: **89 checks, 0 failures/errors/skips**.
Selected private controls reproduced assertion failures: incoming-not-prior M2 = 6,
split transaction M5 = 1, restored global W1C M10 = 3, all without harness errors
or skips. The archived full-suite log has 91 ordinary passes and 9 pass-reason
records, no failures. These are real results, but not complete width coverage.

**New deciding defect.** `ac97m_veh` accepts the shared decoder's eight-byte
memory operand. `ac97m_wr` then casts its `uint64_t` value to `uint32_t` before
`ac97m_apply_write`, whose loop shifts that 32-bit value by 32 or more for the
upper bytes. This loses data and invokes undefined shift behavior. Codex compiled
a private probe against the final production toolkit libraries, using a real
faulting volatile store and load at the ordinary-storage page offset `0x200`:

```text
QWORD-REAL-FAULT expected=1122334455667788 actual=5566778855667788
control32=AABBCCDD serviced=4 refused=0
probe_exit=1
```

The page was owned by the production handler, which serviced rather than refused
the access. The 32-bit positive passes; the existing 89-check gate also passes.
This is demonstrated silent corruption, not a claim that the current stock smoke
uses that operand width or that it caused the separate ICALL. Reproduction source,
build/run logs and script are outside git under `xbr-mac/c3-rr-codex-qword/` and
`xbr-mac/c3-rr-codex-qword-probe.py`. No shared production file was modified for
the probe, no game was launched, and no original asset or save was written.

**Reviewer selection was not satisfied.** Native `ReviewC3RrRepair2` session init
identifies `agent=task`, resolved `opencode-go/muse-spark-1.3-contributor:xhigh`,
fallback false. Its parent task call supplied a name/review assignment without
an agent/model-role selector. That is an independent review assignment, but not
the configured reviewer invocation. Implementation and test workers also used
native task/Muse in distinct sessions. Do not infer role selection from task names
or substitute a generic task while claiming the configured reviewer requirement.
The configured Workbuddy/slow setting was not changed.

**Classification independently checked.** Original XBE disassembly confirms
`0x193E62` loads `[esi+0x1c4]` and `0x193EB5` calls EAX, returning to `0x193EB7`.
The archive records one invalid target `0x00700010`, zero frames, 11.8 seconds and
exit `0xE0424943`. Its D3D section attribution is correct; pointer installation
and SR causality remain unproved. The POSIX A/B/C result is a differential repair
of two declarations amid 41 pre-existing compile errors, not a passing POSIX build.
M11 crashes before assertions and must not be included in a blanket claim that
every mutant failed a named value predicate with zero errors.

**Next bounded repair:** preserve full-width values and defined shifts through
the accepted MMIO write path, add a different author's real eight-byte regression
with independent byte/dword readback and narrower-width positives, refresh affected
final-source mutation evidence, and invoke/verify the actual configured reviewer.
Correct these record limits, then final source-bound checks and at most one
changed-source smoke. No new ICALL fix, C4 or push in that packet. Register/reset
repair and whole C3 remain unaccepted. The linked supervision renewal carries
unused operation slots and grants one independently justified repair allowance;
its own assessment/accounting is in the ignored Codex review, not an implicit reset.

### §31.51 C3-RR qword write repair, width gate and reviewer routing (2026-10-19)

The bounded repair for §31.50's demonstrated defect. Toolkit
`src/kernel/xbox_memory_layout.c` only; the game gains only gate rows and records. No game
production change, no push, C3/C4 unaccepted.

**The defect and the fix.** The NABM owner accepted the shared decoder's eight-byte memory operand,
but `ac97m_wr` narrowed the value to `uint32_t` before `ac97m_apply_write`, whose extraction then
shifted a 32-bit value by 32 or more for the upper bytes: the high word was lost and the shifts were
undefined. Repair: `ac97m_apply_write` takes `uint64_t`, `ac97m_wr` passes the decoded value
unchanged, and every byte is `(uint8_t)((val >> (8u * (unsigned)i)) & 0xFFu)` for `i < size` with
`size` in {1,2,4,8} (maximum shift 56), so all widths are defined and ordinary storage keeps all
eight bytes. The BDBAR rule (`val & ~3`) now applies to the first dword of a write starting at a
channel base with `size >= 4`, with the following bytes keeping the normal per-byte rules in
increasing address order; width 4 behaviour is byte-for-byte unchanged. The BDBAR mask is
offset-anchored — it applies when the write STARTS at the channel base — while the GC/GS L20 sync is
range-anchored and fires for any width that overlaps either global; the gate asserts the
starting-offset form only. A write that merely overlaps a BDBAR dword without starting there (e.g. a
qword at page `0x12A` whose byte at `0x12B` is the CR byte) keeps the pre-existing per-byte/plain
behaviour it had before this packet, and that pre-existing asymmetry is recorded, not asserted. The
read path already
composed eight bytes and is unchanged. Also closed: the Windows armed ack-worker branch printed and
flushed the `[A3A]` witness while holding `s_ac97m_lock`; it now decides the once-only flag and
snapshots both register values under the lock, releases it, and only then prints/flushes the same
snapshot exactly once. The printed values themselves vary with gate ordering, because the witness
fires on whichever tick first observes GC bit 1 (observed in the same build:
`gc=0xAA020002 gs=0xAAAA0100` in `final-run.log`, `gc=0x00000002 gs=0xFFFFFFFF` in
`restored-run.log`, `gc=0x00000002 gs=0x00000100` in `old-run.log`); the snapshot semantics and the
once-only property are what the repair fixes, and neither varies.

**Independent gate (different author).** `tests/ac97_reset_gate.c` grew from 89 to **132 checks**,
still ONE CTest case. New rows are real faulting guest accesses through the protected page:
a qword store/load of `0x1122334455667788` at page `0x200`; eight byte, two dword and four word
readbacks of that write; an independent 64-bit read assembled from two 32-bit stores
(`0x0123456789ABCDEF`); `0x8000000000000001`, zero and `0xFFFFFFFFFFFFFFFF` round-trips; 64-bit
neighbour sentinels and untouched `GLOB_CNT`/`GLOB_STA`/CAS; narrower-width positives in both
directions; and qword writes over `GLOB_CNT`/`GLOB_STA` (`0x12C` -> GC `0x00000000`, GS
`0xFFFFFEFF`; straddle at `0x12E` -> GC `0xAA020002`, GS `0xAAAA0100`, CAS low bytes stored) plus a
qword starting at a channel base (BDBAR `0xFFFFFFF8`, `CIV/LVI/SR` dword `0x11011344`). Expectations
are hand-derived from the contract; the seam is used only for setup. Final and restored:
`checks=132 failed=0 harness_errors=0 skips=0`.

**Controls (outside both repositories, `C:/Users/logic/xbr-mac/c3-rr-qword-mutants/`).** The exact
historical source `c395db3` links and runs with the final gate and fails the deciding row as a
**value** assertion: `expected 0x1122334455667788, got 0x5566778855667788`
(`checks=132 failed=18`). New mutants, each one deliberate change, each red on a named predicate with
0 harness errors and skips: truncate-to-32 (`got 0x0000000055667788`), repeated low word
(`got 0x5566778855667788`), zeroed high word (`got 0x0000000055667788`), and read-narrow (qword
loads red while all byte/dword/word readback rows stay green, which is what separates a write defect
from a read defect). The prior control set was refreshed from the final model with each mutation's
semantic idea re-applied and still decides its own row (M0 24, M1 12, M2 6, M3 5, M4 8, M5 1 across
8 deterministic runs with both start orders and the unreachable `cr=0x1C`, M6 43, M7 8, M8 12,
M8b 2, M9 3, M10 3 with the drifted `0xAAAA0000` actual explained by the new qword overlap row, M11
a crash before assertions with no summary — reported as a crash, never as a value predicate).
Hashes: layout `c97e387ff2591d045f5eee8527948d69cd8d8c7f95dd5b15e1b7a84ec6b7083b`, decoder
`5ef7de18722c11b2f53b9cd9cf2773b42bf76ce811e0432565ba02e916b2f161` (identical in every copy), gate
`bdcecfc4399bc3e5f6fd07d170e7f585a686bd21efbc42a30e77cd1a690d1aec`, final gate binary `e8fc91fa…`.

**Reviewer routing (corrected).** The configured reviewer is invoked through the supported per-task
`agent: reviewer` selection; the task tool carries it inside each `tasks[]` item (a top-level
`agent` field is ignored and silently defaults to the generic `task` agent). The first dispatch of
this packet placed it at the top level, resolved to the generic native `task` on
`opencode-go/muse-spark-1.3-contributor:xhigh`, and was **cancelled** (`proc://ReviewQwordRepair/kill`)
with its output discarded: no analysis from it is used or counted anywhere in this packet. The
re-dispatch's native child session (`01a15339-93db-7632-87a8-589845fa0508`, parent
`01a15293-2ee3-7504-ae31-d59ac31dad2c`) records `agent: reviewer`, `agentSource: bundled`,
`modelRole: slow`, resolved `workbuddy-ai/deepseek-v4.1-flash:max`,
`resolvedModelIsFallback: false`, thinking level `max` — the
existing configured reviewer/slow binding, no fallback, no config change. It re-ran the final gate
(132/0/0/0), verified the sources, expectations and every raw log, found no patch-introduced defect
(confidence 0.94), and recorded one pre-existing, patch-independent observation: a write that
straddles a block end (e.g. a qword at page `0x12A`) still bypasses the CR rule for its byte at
`0x12B`, exactly as before this packet and untested by the gate.

**Gates and runtime.** Guarded build: 55 targets, 100 CTests, 0 compile/link errors, no C4310, the 18
known MSB8028, no new warning from a changed translation unit (the gate recompiled; the toolkit TU
was up to date from the implementation author's build of the unchanged source). `just check` passed;
full limited-rights CTest **100/100 in 574.30 s**, no skips, `jsrf_ac97_reset_gate` 132/0/0/0 inside
it. One changed-source cache-off Vulkan smoke,
`20261019-032447-223-keyscript-c3-rr-qword-e2e-vk`, executable
`83823e5ce6cf9731fc0ae7f1fb6dfa7fc11f6756a90add73deb006ff42748c67`, 11.8 s, `[AC97] serviced=40
refused=0`, `[A3A] ac97 witness` once, then the unchanged separately classified stop
`[ICALL] invalid target 0x00700010 tid=22604 esp=007BFF9C return=00193EB7`, 0 frames, exit
`0xE0424943`. The archive's `build-source.json` records this run's own source identity (gate
`bdcecfc4…`, layout `c97e387f…`, decoder `5ef7de18…`). The qword repair neither caused nor fixes
that ICALL, whose cause remains UNPROVEN, and whole C3 remains failed and unaccepted.

### §31.52 C3-ICALL diagnosis: the target's field is BSS, the writer is unproven (2026-10-19)

A bounded read-only diagnosis of the stop in §31.51, with no production edit in either
repository and no game run. Its first draft was independently reviewed and **two of its claims
were rejected**; the corrected findings are below, and the rejected reasoning is recorded so it
is not repeated.

**Verified.** The call site is `0x193EB5 call eax` with `eax = [esi+0x1c4]` loaded at `0x193E62`
and the fatal return at `0x193EB7` (original XBE, `inspect-jsrf.py disasm`). `esi = 0x0019D468`
lies at section offset `0x10928` of the D3D section, beyond its `0x0000E6B0` file-backed size, so
both the object and the field `0x0019D62C` are in the zero-filled BSS tail: static storage with no
file-backed initializer, not a heap object. The containing function `0x00193D90-0x00193EE2` is a
**recovered** function (`config/recovered-functions.json`, section `D3D`), emitted as
`body_00193D90`; no generated chunk covers it. Its bodies, its only caller `0x00196C0B` and the
helper `0x00193D10` are byte-identical between the pre-regeneration archive's own source
(`20261018-121035-472\source.zip`) and the current tree, so the regeneration did not change this
code. The preceding call `0x193E5C call dword ptr [0x1c4014]` resolves through `0x80000091` to
kernel ordinal 145 = `KeSetEvent`, and its first argument is the event header at `esi+0x1C8`, the
dword immediately above the called field. The call site **discards** that call's return value:
after `call eax` the code reloads `dl` from the stack and ends `xor eax,eax` on both branches, so
the ordinal's 0-vs-1 return cannot distinguish runs at guest level — it only reports which bridge
path fired.

**Corrected after review.** (a) The draft claimed the stop was "newly exposed by the C3
regeneration", using `20261018-141756` as a negative control. That control does not hold: that
archive carried **no toolkit patch at all** (`toolkit.patch` empty), and it **did** execute the
call path — `[RECOVERED] 0x00193D90 returned` plus 89 detailed `ordinal 145 ... ret=0x00193E62`
lines over 301.2 s — so it reached the function and read 0 from the field. (b) The draft claimed
AC'97 state was not causal because `141756` had the witness but no ICALL; in fact all three
relevant logs carry the identical `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100`, and neither
`141756` nor `184102` contains any `[AC97] serviced=` line. That stated contrast did not exist.

**What survives.** The qword repair cannot be the cause: `c395db3..fc6e2ea` is one commit on one
file changing only AC'97 register write widths and the `[A3A]` lock scope, and it writes only
`s_ac97m_regs[]` — guest-visible as registers, never a guest code pointer. Scanning all 331 run
archives, `0x00700010` occurs in exactly five, all in the C3-RR era (`20261018-184102`, `185419`,
`190934`, `20261019-002247`, `20261019-032447`) and in none earlier. The stop is **co-extensive
with the presence of an `ac97m_` toolkit patch**: every archive carrying one hits it, and no
archive without one does. That is a correlation over archives that differ in more than one
variable, not a causal chain.

**Unproven.** What wrote `0x00700010` into guest `0x0019D62C` is UNPROVEN: none of the five ICALL
archives contains a `.dmp` or `dump/` directory (the non-ICALL `141756` does ship a 139 MB
`frozen-startup\process.dmp`, a different horizon). `0x00700010 = 0x00700000 + 0x10` and
`0x00700000` is the toolkit's `FAKE_RWDATA_VA`, reached by the guest via
`fs:[0x28] -> 0x00760000 -> [0x00760028]`; a RenderWare-sourced pointer landing there is a
plausible inference, and no toolkit code writes it, so the writer is most likely guest code — both
unproven. Whether the halted AC'97 SR state changes this D3D8 path is likewise UNPROVEN.

**Next instrument.** The ICALL archives ship the exact `jsrf_recomp.exe` and `.pdb`, so a 4-byte
hardware write breakpoint on the host mapping of guest VA `0x0019D62C`, logging the writing RIP and
guest return address (first write wins), names the writer directly. A cheaper partial alternative
already in the driver is `peek` with a pointer `chain`
(`{"peek": {"name": "dev_call", "chain": ["0x0019D468", "0x1C4"], "size": 4}}`), which reads guest
memory without changing guest state and can localise the write to a step interval without naming
the instruction. Neither may be replaced by suppressing the ICALL, bypassing the AC'97 model, or a
synthetic readiness pin. C3 and C4 remain unaccepted.

### §31.53 C3-ICALL: the writer of 0x0019D62C is named (2026-10-19)

One bounded exploratory run named the store that §31.52 left UNPROVEN, using an instrument the
toolkit already ships. No production edit in either repository; the finding is observation only,
and C3/C4 remain unaccepted.

**Instrument.** `JSRF_TRACE_A2H_SLOTW` — the A2h live slot-write watch
(`xboxrecomp/src/kernel/xbox_memory_layout.c:4257-5574`, gate and constants in the header at
`:562-583`). It is not a debug register: it makes the slot's 4 KB page read-only in the canonical
view **and all 28 mirrors** (`:5283-5339`), records the faulting store, single-steps it, and
re-protects. It derives the slot at ARM from `MEM32(0x0019DCE0)` (a runtime pointer to the
software_device object) plus `0x242C` by checked 32-bit addition (`:5238-5252`), re-derives it at
TERMINAL (`:5440-5450`), and publishes a fixed 1024-slot event ledger whose `seq` is written last
(`:4347-4375`). The game's collector archives that ledger by symbol
(`tools/harness/collect.c:2059-2068`, `GUEST_SLOTW_EVENT ... rip=%016llX`), so the deciding
record is a durable artifact rather than a log line. `xbox_A2hSlotWatchTerminal` runs from the
fatal ICALL hook (`src/recomp_manual.c:100-104`) before `RaiseException`.

The two cheaper alternatives were rejected for stated reasons. A `peek` chain samples only at
script step boundaries (`POLL_SECONDS = 0.25` s, `scripts/key-script-run.py:168`), and this run
shows the field read 0 at kernel calls #2/#5 and `0x00700010` at #7 on one thread — a window of a
few guest instructions, which a boundary sampler cannot bracket. A static scan of the generated
and recovered sources finds 78 candidate `MEM32(... + 0x1c4)` stores, which cannot decide.

**Run.** `scripts/run-jsrf.py --seconds 60 --label a2h-slotw-writer --profile exploratory`, with
`JSRF_TRACE_A2H_SLOTW=1` and `RECOMP_KERNEL_LOG_BUDGET=100000`, reproducing the baseline's other
switches (`RECOMP_APU_TRAP=1`, `RECOMP_PB_EXEC=1`, `RECOMP_GPU_BACKEND=vulkan`,
`RECOMP_VK_PIPELINE_CACHE=off`). `RECOMP_WATCH` was deliberately NOT set: `:2578` records that the
two page watchers protect pages of the same table and would fight. Archive
`20261019-213447-432-a2h-slotw-writer`, executable
`83823e5ce6cf9731fc0ae7f1fb6dfa7fc11f6756a90add73deb006ff42748c67` — **the same executable as the
failing §31.51 smoke**, so this is a controlled reproduction, not a different build. It reached
the same stop: `[ICALL] invalid target 0x00700010 tid=26484 esp=007BFF9C return=00193EB7`, exit
`0xE0424943`, 27.7 s.

**VERIFIED — the writer.** The watch armed on exactly the intended slot, all 29 views:
`[A2HSLOTW] armed base=0019B200 slot=0019D62C page=0019D000 off=62C aliases=29/29
mask=1FFFFFFF`. It recorded one slot hit, a consecutive pair on tid 13564 at the same fault
address: the pre-open fault record `pre=00000000 post=00000000` and the post-store step record
`pre=00000000 post=00700010`, both `range=1` (`RANGE_GAME_MODULE`). The terminal read agrees with
the recorded write (`coherence verdict=1`, i.e. COHERENT), the reconciliation is `records=3
counted=3 complete=1`, and `dropped_events=0`, `cross_mismatch=0`, `protect_failed=0`,
`publish_failed=0`.

The native RIP `0x7FF6BE1CB68D` resolves, through the archived `jsrf_recomp.map` at preferred base
`0x140000000` (RVA `0xD0B68D`), to `sub_0018CE30 + 0x5D`. Disassembling the archived executable
there gives `mov dword ptr [rdx + rax], ecx` (`89 0C 02`), preceded by
`lea edx, [rax + 0x242c]` and by the RIP-relative load of `g_xbox_mem_offset` — the canonical
alias, confirming the store went through `XBOX_PTR`. The guest instruction it implements is
`0x0018CE3A mov dword ptr [ecx + 0x242c], eax` in `sub_0018CE30`
(`src/recomp/gen/recomp_0005.c:1255-1263`, `MEM32(ecx + 0x242C) = eax` with
`ecx = MEM32(0x19DCE0)`). That is the **only** store to `+0x242C` in the original XBE and the only
one in the generated tree, and it executed with `eax = 0x00700010`. So the writer of the field is
named at instruction level: **guest code, `sub_0018CE30` at `0x0018CE3A`, canonical alias,
`0x00000000 -> 0x00700010`.**

Two independent checks agree that the facility was looking at the right object. The arm base
`0x0019B200` is `MEM32(0x0019DCE0)`, the software_device pointer, not the faulting `esi`
`0x0019D468` (the context); the two differ by `0x2268` and the header's own identity
`context+0x1C4 == software_device+0x242C` holds exactly (`0x19D468 + 0x1C4 = 0x19D62C` and
`0x19B200 + 0x242C = 0x19D62C`). The facility's own install-write positive also fired
(`installer_control_hits=1`).

**Corrected during review.** (a) `alias=1` is the **canonical** view, not a mirror: the alias index
is `owning_alias`'s `i + 1` over `g_a2h_slotw_pages[]`, whose element 0 is canonical
(`:4814-4822`, `:5313`), and the fault address `0x001AD62C` is the canonical host VA
(`slot + g_memory_offset`). An earlier verbal reading of "mirror alias 1" was wrong. (b) The
`kind=1` record is published **before the page is opened**, not "at the fault before the store":
the value appears only in the `kind=2` step record, and `pre` is the slot pre-read at `:5053`.
(c) `sub_0018CE30`'s callers are exactly two static tail-jumps
(`recomp_0003.c:54653`, `:54666`) **plus one indirect route** through the dispatch table
(`recomp_dispatch.c:8268`); only the static statement is proven.

**UNPROVEN — the value's provenance.** `sub_0018CE30` is a setter, and its only static caller
`sub_0015F9E0` (`recomp_0003.c:54634-54668`) maps argument 0 to `0x15F9D0`, argument `0xFFFFFFFF`
to 0, and passes anything else through. That caller's only static call site
(`recomp_0000.c:3827`, inside `sub_00012210`) pushes `ebx`, which is zeroed at `0x1224F` and not
rewritten before the call at `0x12319`, so **the static path supplies 0, not `0x00700010`**. The
indirect dispatch route was not observed, and an XBE scan for `0x0018CE30` / `0x0015F9E0`
literals finds none (only the installer constant `0x15F9D0` at `0x15F9E9`). What supplied
`0x00700010` is therefore UNPROVEN; `0x00700010 = FAKE_RWDATA_VA + 0x10` remains a plausible
RenderWare origin and nothing more.

**Claim boundary, stated because the run cannot support more.** Absence claims are invalid in this
archive: `concurrent_overlap=15` alone sets `absence_rows_valid=0`
(`tools/harness/collect.c:2109-2112`), and `first_touch_overflow=1` with
`first_touch_dropped=1915` means the page census covers 2427 of 3250 distinct addresses. So "no
other write touched the slot" is unsupported, even though the single recorded positive is sound
(a single-tid, single-instruction capture with `complete=1` and a coherent terminal read). The
`positive_records_valid=1` field is a hardcoded literal in the collector, not a derived verdict,
and is not cited as evidence. Two further latched facts bar promotion of this run:
`unexpected_exception=434` and `range_unknown=2` (RIPs `0x7FF857BDCC71`/`75`, `same_image=0`).
The arm was **deferred** — `MEM32(0x0019DCE0)` was 0 at start (`jsrf_run.log:102`) and the watch
armed later at kernel ordinal 166 (`:830`), before the guest work that faults at `:5066`; that is
an ordering argument, not a proof, and the pre-arm interval is unobservable in principle here.

**Not a defect claim.** The store is a legitimate setter; a bad *value* in a callback slot is not a
bad store. The slot is the D3D poll-callback field `m_pVerticalBlankCallback`, and re-initialisation
is not excluded — though `pre=0` and the absence of a second slot write mean such an explanation
would have to be this same single store. Whether the halted AC'97 SR state changes this D3D8 path
remains UNPROVEN, and this run is **not comparable** to the earlier archive on that variable
(`[AC97] serviced=7` here against `serviced=40` there). No ICALL was suppressed, no AC'97 model was
bypassed, no synthetic readiness pin was enabled (`JSRF_ALLOW_UNRESOLVED` absent, kernel thunk
bridge 120/120 with 0 stubs, profile classified exploratory).

**Next decision, not taken here.** The field's writer is now named, so the remaining question is
the value's provenance: which caller reaches the setter with `0x00700010`, given that the one static
site pushes 0. That is a new bounded diagnosis, not a repair, and C3/C4 stay unaccepted until the
owner or Codex decides.

### §31.54 C3 and C4 accepted: a deleted tail jump, its repair, and the acceptance runs (2026-10-20)

The owner put the Mac-side Claude session back in charge after §31.53. It worked on the laptop over
ssh. Every code step's gate was written by a different worker than the code, and every gate has a
red control. This section supersedes §31.53's "UNPROVEN" value question. The toolkit commit
`35c6537`, `scripts/check-dead-gotos.py` and the generated baseline's update reason cite "TR 31.53"
for the thunk; this section is the record they mean.

**The cause, VERIFIED.**

- **The latch run.** A temporary, uncommitted write-once latch in the generated `sub_0018CE30`
  (`20261019-233947-923-step1-latch`, restored afterwards; `.text` and `.data` identical to the
  archived exe) recorded the single call: `[esp]=0x0001231E`, `esp=0x00F7FF00`, `ebx=0x00700010`.
  So the setter was reached from `sub_00012210`'s only static call site at `0x12319`.
- **The shifted stack.** Kernel-call stack depths place the 8-byte shift inside `sub_00012210`'s
  call to `0x5F350`, after `call 0x116EC0`.
- **The deleted jump.** The ICALL at `0x116F46` pushes 8 bytes and reaches the adjustor thunk
  `sub_00169000` (`sub dword ptr [esp+4],4; jmp 0x168230`). The regenerated lift had emitted that
  as `MEM32(esp+4) -= 4; (void)0; /* goto loc_00168230 - dead code */`, so the callee's `ret 8`
  never ran.
- **The register.** `sub_0005F350`'s `pop edi/esi/ebx` then read shifted slots, and the caller's
  `edi`, `0x00700010`, became the `ebx` pushed into the wrapper `sub_0015F9E0` and stored by the
  setter.
- **Not game behaviour.** The pre-C3 tree lifted the thunk as a tail call. The cause is not
  FAKE_RWDATA/TLS modelling, and the back-edge hooks the C3 pass added (0 before, 3,597 after) are
  inert without `RECOMP_GUEST_SERIAL`.
- **The toolkit cause.** `_is_external_target` (`tools/recomp/lifter.py`) treated targets the
  regenerated database classes `tail_jump_alias` as intra-function, and the translator's label
  validator turned the jump into `(void)0`. The rule predates C3; C3's database reclassified
  `0x168230`, `0x168310` and `0x177B10` (INFERRED for the reclassification).

**Repairs.**

- **Toolkit `35c6537`.** A jump leaving its function becomes a tail call when the target has its
  own symbol (an emitted body, or a manual/recovered definition). This fixed 20 sites: the thunk,
  recovered `body_00169010`, six C++ EH funclets jumping to `0x177B10`, and 12 others.
- **Toolkit `7544264`, after an independent C4 review.**
  - Every remaining untranslatable jump is a fatal `RECOMP_JUMP_TRAP(site, target)`: 28 sites,
    including the three recovered `__except` bodies, which trap again as they did in C2.
  - An alias whose folded owner starts after it gets a fatal wrapper: 11 aliases (ledger L64).
- **Tests.** `6971a4b` and `51fcb85` hold the toolkit tests.
- **Game `ba47e8a`.** The recover and relift scripts follow the same rule.
- **Game `141c23a` (the review's P1s).**
  - **Restored as their own functions:** `0xC42E0` (slot 7 of handler table `0x216150`; it had
    folded onto the constructor at `0xC44D0`) and `0x11B680`.
  - **Recovered and retired:** `0x6FE40..0x70057` recovered; the false `0x70027` retired (its "table"
    is packed 16-bit data).
  - **Boundary fixes:** `0xD58E0..0xD59FE` and `0x11BA00..0x11BAAC`.
  - **Result:** 3,393 recovered entries; 0 silent deleted jumps remain.
- **Game `1211173`: save isolation.** With `RECOMP_SAVE_DIR` unset, v0.14.0's path rule keeps UDATA
  and TDATA in `game\` when the save root has none, so a run that saved would have written
  `game\UDATA`.
  - Runners now set `RECOMP_SAVE_DIR` to the run's private root.
  - `main.c` prints `[SAVE] udata_root`/`tdata_root` and refuses to start when they land outside it.
  - Run verification requires both lines inside the root.
  - Owner play is unchanged.

**Gates.**

| Gate | Green on the fix | Red control |
|---|---|---|
| `tools/recomp/test_tail_jump_to_alias_body.py` | 13 cases pass | 6 fail on `35c6537`; a mutant that folds after-owner aliases fails |
| `check-dead-gotos.py` (in `just check`) | 0 silent, 28 traps, 0 callable | the previous tree: 35 silent |
| `test_alias_dispatch.py` | passes | 14 failures on the previous tree |
| `test_tail_thunk_169000.py` | passes | the pre-fix lift leaves esp 8 bytes low with `calls=0` |
| `test_save_isolation.py` (36 cases) | passes | five mutants red |

- **Suite:** full limited-rights CTest **104/104 in 527.66 s** (`logs\ctest-c4-final.log`);
  `just check` green.
- **Known timeout:** `jsrf_vsh_aot_provenance` takes about 60 s alone and over 120 s beside the rest
  of the suite, so its timeout is now 300 s.

**Acceptance runs.**

- **Slot check** `20261020-013608-530-s4-slotw-check`: the vblank-callback slot now receives
  `0x0015F9D0`, the default callback. This rests on the recorded write; terminal coherence was not
  derived.
- **Final-exe N3 Vulkan e2e** `20261020-141554-623-keyscript-c4-final-n3-e2e-vk`, quoted from its
  `jsrf_run.log`:

      [Vulkan Loader] INFO | LAYER:   Insert instance layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)
      [Vulkan Loader] INFO | LAYER:   Inserted device layer "VK_LAYER_KHRONOS_validation" (C:\VulkanSDK\1.4.357.0\Bin\VkLayer_khronos_validation.dll)

  Result: `keyscript_complete`, `validation_errors=0`, `validation_warnings=10` (the known ones),
  `vsh_misses=0`, `pipe_evictions=0`, 0 ICALL, 0 fatal, no `[JUMP]` or `[ALIAS-ICALL]` line, and
  `[SAVE] udata_root` inside the run's private save root.
- **D3D11 reference e2e** `20261020-015004-763-keyscript-c4-e2e-d3d11`: `keyscript_complete`. Its
  main menu, Corn's dialogue, the Garage start and the pause map match the Vulkan run
  `20261020-013942-411-keyscript-c4-n3-e2e-vk`; the only differences are animation timing. Both
  runs used the executable before `7544264`, whose changes only turn already-unreached deleted
  jumps into traps.
- **Save isolation** `20261020-124155-792-keyscript-save-iso-e2e-vk`: the game's save landed in the
  run's root, and `game\UDATA` listings and timestamps were identical before and after.
- **N2 on the pre-trap executable,** three interleaved pairs on the RTX 4070:
  - final 13.65 / 13.63 / 13.57 presents/s (median 13.63);
  - baseline, the preserved N4.3 executable, 12.53 / 12.77 / 12.62 (median 12.62);
  - 0 aliased polls.

  This is an observation; no performance claim, since the host has drifted between sessions.

**Limits.**

- The 28 traps and 11 fatal wrappers are not reached on the e2e path. The review classed about 19
  traps as data decoded as code; the rest are interior labels of recovered functions.
- `test_check_dead_gotos.py` still skips without a generated tree.
- The archive validator `scripts/jsrf_run_profile.py` does not yet read the new
  `observed_udata_root`.
- Exploratory profile: no fidelity claim.

**C3 and C4 are accepted.** Pushes follow (receipts below). D1 is next.

PUSHED_TO: origin / BRANCH: main / COMMIT: 51fcb8563a384fbe69a8386a14e89f6f5666f030 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 6c3cd84..51fcb85 (131 commits: the v0.14.0 merge, ee2e0cd, the C3-RR AC'97 repair, the tail-jump and trap fixes and their tests), toolkit pushed first, clean and synchronized; 480 outgoing blobs (largest 434 429 bytes), 0 secret hits, no retail-derived path; upstream untouched (push URL DISABLED).
PUSHED_TO: origin / BRANCH: master / COMMIT: cebd1f9 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 699bc46..cebd1f9 (18 commits: the C3 seeds and gates, C3-RR gates and records, the C4 review repairs, save isolation and the C3/C4 records), clean and synchronized; 79 outgoing blobs (largest 3 347 497 bytes), 0 secret hits, no `game/` or retail-derived path.

### §31.55 D1 and D2: the reJSRF discovery fixes are ported and change nothing in JSRF (2026-10-21)

**D1, toolkit `4dec5ea..1394777`.** Five MIT-licensed fixes from `nworkers/xboxrecomp` were ported.
The original SHAs now live on that fork's `rejsrf-v0.13` branch; the ports were taken from the
patch-identical `rejsrf-v0.14` rebase. Attribution is in each commit body.

| Source | Ours | Decision |
|---|---|---|
| `58f7166` + reachability fix `20a269e` | `4dec5ea` | Mostly covered by upstream `34f5e1c`. The late tail-jump pass now also runs after the fork's late orphan passes. |
| `0c32a78` | `69fe559` | The fork's `_pass_cond_branch_orphans_after` already took jcc from final bodies. New: a jcc target whose block ends in an indirect jmp. |
| `982cd28` | `926b44a` | Ported unchanged: recovered switch arms are scanned for switches of their own. |
| `7c58ec6` | `83ead08` | Ported unchanged: a recovered body's arms get entries inside its range. |
| `6a38c01` | `1394777` | Partly ported. Our stubs already report through `recomp_icall_fail_log` and stay fatal (`0xE0424943`). Only the `_stub_definition` helper and a test were taken. The opt-in silent hook was not, because it would make stubs non-fatal by default. |

**Not ported:** the prerequisite pass `10f2f96`. On JSRF it adds only alias entries at `0xB8A60`
(ambiguous, deliberately unresolved) and `0xB9270` (already recovered). A jump into an
unexamined interior already becomes a fatal `RECOMP_JUMP_TRAP`.

**Tests.** Toolkit pytest: 915 passed, 4 skipped, 1 failed. The failure is the known
`test_lifter_rep_compare_flags` negative control, and `main` has the same one. The 15 new tests are
the source authors' tests, adapted; their late-pass and arm-entry cases fail against the previous
toolkit.

**D2.** The disassembler on JSRF gives 8,608 functions, byte-identical output to the previous
toolkit. A full regeneration (`just regen`, boundary relift, recovery, ABI deltas, all exit 0) gives
a generated and recovered tree **byte-identical** to the one before: 16 files, 0 changed. Its JUMPS
and ALIASES reports match §31.54, and no config changed. D1 touched no runtime or template source,
so the guarded build and `just check` pass and no new runtime run is needed. On our base, the JSRF
cases these fixes were written for were already handled by upstream v0.14.0, the fork's own passes
and the game's recovered seeds.

PUSHED_TO: origin / BRANCH: main / COMMIT: 13947770f0ab5e79d109e03e0b4ec16c7c7e2b30 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 51fcb85..1394777, 5 commits, 11 outgoing blobs (largest 205 556 bytes), 0 secret hits; upstream and nworkers untouched (push URLs DISABLED).

Next: D3–D4, triaging reJSRF's game-side fixes against our LLE executor and Vulkan back end.

### §31.56 D4 coverage prerequisite: initial candidate rejected (2026-10-21)

Toolkit `4203fda` / game classification `d5ea04f` are local, unpushed coverage
instrumentation candidates, not accepted work. Codex independently compiled the
candidate's `pb_coverage_batch` and `pb_coverage_note_method` bodies verbatim in
a small state harness, with the production ledger header and admission table.
This tests the actual predicate bodies, not a full pushbuffer submission:

- Stage-0 positive control: texgen=1, texmat=1, first_valid=1, exit 0.
- Stage-1-only texgen (`0x3D0=0x8511`): texgen=0, first_valid=0, exit 1.
- Stage-1-only matrix enable (`0x424=1`): texmat=0, first_valid=0, exit 1.
- After a mode-6 batch claims `first_b`, delivering `0x1B38` changes its
  `bump_seen_mask` from 0 to 2 while valid remains 1: first-state retention fails.

Local xemu source `478b4f496102379c7eaa7f3ec10e714a703c4300`,
`hw/xbox/nv2a/pgraph/pgraph.c`, defines texgen slots by a 16-byte stride, matrix
enable slots by a four-byte stride, and four 16-word texture matrices. The
candidate instead checks only stage 0, interpreting its one enable register as
a four-stage bitmask. The toolkit's unchanged admission table admits both of
the stage-1 methods above. Their misses therefore cannot be excused as absent
admission. Other raw fields really are unavailable through this seam: the same
production table rejects bump matrix/scale/offset methods `0x1B28/38/3C`. Their
zeros must be distinguished from observed zero values; diagnostic work must
not silently change admission or rendering to obtain them.

Codex reran `jsrf_nv2a_registers` through the limited-rights wrapper: 1/1 pass.
The gate-on smoke reclassifies EXPLORATORY, with no unknown/missing profile;
both archived key scripts have `script_ok=false` at their 240 s bound. These
are bounded smoke observations, not completed gameplay coverage or fidelity.
The implementer's native transcript records `opencode-go/deepseek-v4.1-flash`,
thinking high, fallback false. Configured reviewer transcripts record
`workbuddy-ai/step-5-preview`, high, fallback false; both attempts returned 429
and produced no independent review. Codex's reproduction supplies the present
independent rejection, not acceptance of the other predicates.

**Next:** bounded diagnostic repair: all four stage predicates and retained
raw state; a separate current delivery bitmap copied into an immutable first
snapshot; explicit availability for undelivered fields. Preserve admission,
rendering and gate-off behavior. Then a different author's production-path
fixtures and private final-source mutations, followed by the four coverage
scripts. No feature implementation, D completion or candidate push yet.


### §31.57 D4 coverage repair candidate: all-stage predicates and frozen first records (2026-10-21)

Bounded repair of the defects in §31.56. Toolkit candidate `44c59a93c5a2f4c41d804efb833e5e6118059455` (local, unpushed,
ungated) changes only `nv2a_backend.h`, `nv2a_pb_exec.c` and one comment in
`nv2a_vk_backend.c`. It is not accepted work: the different-author gate and the four coverage
scripts are still owed.

**What changed.**

- Texgen now samples all four stages (`0x3C0 + 0x10*s + 0x04*c`, four components), matrix enable
  is a per-stage word (`0x420 + 0x04*s`, not a bitfield), and texture matrices are per stage
  (`0x6C0 + 0x40*s`, 16 dwords). The local xemu `478b4f49` handlers supplied the strides as
  facts only.
- The live delivery map (`s_pb_seen`) is separate from every first record. A claim copies the
  raw values and the per-word `*_seen` masks at that instant and is never rewritten; the version
  is 2.
- Availability is explicit per word: texgen Q, texture matrices for stages 2/3, the
  bump-environment words for stage 0, and every spot/light parameter word are not admitted by
  `nv2a_method_table.c`. Their raw zeros are UNKNOWN, and their seen bits stay clear. Admission,
  rendering, rejected-stream rollback and fallbacks are unchanged.
- The shared probe now reports, for the decided cases: old source red on stage-1 texgen,
  stage-1 matrix and post-claim mask mutation (`567d8f20…` body); repaired source green on the
  same assertions (`9809c563…` body); the restored old source red again.

**Validation.** Guarded build succeeded with no warning in the changed region; focused CTest
`nv2a` 9/9, `vk_` 15/15 and parity 3/3 passed; `just check` passed; `gpu-submit-supported`
reached `probe_gpu` with `normal_exit`; one 240 s cache-off Vulkan smoke with coverage on
(`20261021-130854-752-keyscript-d4-step1-repair-cov-e2e-vk`, EXPLORATORY) showed
`texgen_seen=7777`, `mat_enable_seen=F`, `matrix_seen=…FFFF` (stages 0/1), `enable_seen=7`,
`stage_ge6=0`, `rc_fallback=0`, 0 ICALL/fatal, saves inside the run root, and a bounded
incomplete script. No mode-6+ batch was observed; that is not evidence of absence in gameplay.

**Next:** Codex review of the repair, then the different author's production-path fixtures and
mutations. No feature work, D completion or push yet.

**Independent review: rejected pending selected-word provenance repair.** Codex
reran the earlier source-body assertions: the repaired stage-1 and retention
cases pass; archived old-source assertions still fail with their positive
control passing. A second probe compiles `vp_program_method` and the repaired
latch/note bodies verbatim with production headers. The availability boolean
currently tests whether any of methods `0xB00/04/08/0C` arrived, rather than
whether dword 0 of the selected program slot was written. Uploading slot 0 then
selecting untouched slot 17 gives word 0, seen 1 (expected 0); uploading dword 0
through `0xB10` gives word `0x12345678`, seen 0 (expected 1). The matched slot-0
`0xB00` positive control passes. These are state/provenance predicate
reproductions, not full committed-submission tests. The repair smoke independently
reclassifies EXPLORATORY with zero unknown/missing profiles; `jsrf_nv2a_registers`
again passes 1/1 despite this metadata defect. The repair is not accepted or
pushed. Next bounded repair must track actual accepted upload slot/component,
freeze availability for the selected first word, preserve rendering/admission,
then return for review before the different-author gate.

### §31.58 D4 coverage repair 2: selected program-word provenance and committed-seam raw state (2026-10-21)

Third local candidate for the D4 coverage prerequisite; ungated and unpushed. Toolkit
`2bcdc3081362341b92a9efb8afad00bdfd005c85` changes only `nv2a_backend.h` and
`nv2a_pb_exec.c`. It is not accepted: Codex review, then a different author's
production-path fixtures and mutations, are still owed.

**Defect (§31.57 review).** `first_b.vp_first_word_seen` OR-ed the delivery of methods
`0x0B00/04/08/0C` across every program slot. Selecting an unwritten slot reported an
unobserved zero, a component-0 write through `0x0B10..0x0B70` was missed, and a
component-1..3 write could claim dword 0. It is diagnostic metadata, not a rendering defect.

**Repair.**

- `vp_program_method` marks `s_pb_vp_word0[slot]` only inside its existing
  `prog_load < NV2A_VSH_SLOTS` block and only for `slot % 4 == 0`, i.e. the accepted
  component-0 write and the cursor slot it actually landed in. A dropped out-of-range
  upload or another component cannot set it; a legitimate zero upload does, so an
  observed zero stays distinct from UNKNOWN.
- `first_b` gains `vp_start`; word, slot and availability are read together at the claim
  and frozen. Ledger version 3.
- Coverage counters and first-record raw values now read a committed-seam shadow copied
  by `pb_coverage_note_method` after the nonzero-subchannel filter. `s_reg` is still
  written before that filter (existing renderer behaviour, unchanged), but a filtered
  method can no longer move a counter or seed a first record.
- Rendering, the subchannel filter, the interpreter, the program cursor and method
  admission are unchanged; gate-off writes nothing.

**Local checks.** Codex's selected-word probe, retargeted to the production bodies: old
source red on unwritten-selected-slot, alternate port `0x0B10` and other-component; the
repaired source green on those plus zero-upload, out-of-range and later-update/
current-selection controls. The earlier all-stage and first-record-retention probe stays
green after adapting its injection to the committed seam (assertions unchanged); the
archived old source stays red. A body-level subchannel probe shows the old shadow counting
a filtered-subchannel write (texgen 1) and the repaired shadow not counting it (0). These
are predicate/state-body checks, not committed-submission or gameplay evidence.

**Validation and smoke.** Guarded build succeeded with no warning in the changed region;
focused CTest `nv2a` 9/9, `vk_` 15/15, parity 3/3 passed; `just check` passed;
`gpu-submit-supported` reached `probe_gpu` with `normal_exit`. One 240 s cache-off Vulkan
smoke with coverage on (`20261021-132905-377-keyscript-d4-step1-vp-repair-cov-e2e-vk`,
EXPLORATORY, 0 unknown/missing) showed `texgen_seen=7777`, `mat_enable_seen=F`,
`matrix_seen=000000000000FFFF` (stage 0's sixteen words at that claim only),
`enable_seen=7`, `stage_ge6=0`, `rc_fallback=0`, 0 ICALL/fatal, saves inside the run root.
The run's toolkit patch sha is exactly `git diff 44c59a9 2bcdc30`. The keyscript is
bounded and incomplete (`script_ok=false`), and no mode-6 batch was observed; neither is
evidence of absence in gameplay.

**Next:** Codex review, then the different author's committed-submission fixtures and
private mutations. No feature work, coverage scripts, D completion or push yet.

**Independent repair review: sufficient to advance to the gate.** Codex inspected
the two production-file changes and reran all eight selected-word cases, the
stage-1/first-retention checks and the diagnostic subchannel probe: all pass on
`2bcdc30`. Archived `44c59a9` still fails the selected-word negatives with its
matched positive passing (body `9809c563…`, correcting the repair report's
`567d8f20…` label); archived `4203fda` still fails the stage-1/retention negatives
with its positive passing. These are body-level reproductions. The current
`jsrf_nv2a_hal` CTest independently passes 1/1. The archived smoke's build-source
hashes for the executor and ledger header match the current files; its toolkit
patch hash matches the committed diff, and its profile reclassifies EXPLORATORY
with zero unknown/missing profiles. This verifies repair progress, not whole-D4
acceptance. The next increment is a fresh test author's actual committed-stream
and Vulkan-backend fixtures, weakening mutants in private source copies, and full
regression checks before any candidate push or long coverage run.

### §31.59 D4 step 2: independent committed-stream and Vulkan gate candidate (2026-10-21)

The independent gate for the D4 coverage diagnostics now exists. It was authored by a worker
distinct from the three production/integration authors, and the toolkit stayed frozen at
`2bcdc3081362341b92a9efb8afad00bdfd005c85`. Game candidate
`576fa7af39b13c27c134128f07233297416f5016` is test-only: `tests/test_pbcov_stream.c`,
`tests/test_pbcov_vk.c`, `scripts/pbcov-mutants.py` and 56 CTest registrations. It is not
accepted yet; Codex review is next, and the four coverage scripts remain step 3.

**What it drives, and what decides.**

- `test_pbcov_stream.c` runs 39 cases, one process each, through the real owner path: guest RAM,
  a synthetic section-less XBE for the memory layout, `nv2a_init_standalone`, the pushbuffer
  window, the instance RAMHT binding, the commit consumer and real pushbuffer words through
  `nv2a_submit_pending`. It reads the production `nv2a_pb_exec_coverage()` ledger. Coverage:
  fixed-function texgen and texture matrices for all four stages with per-stage association and
  exact counters/overlap; stage modes >= 6 with batch/incidence/histogram splits and one count
  for GPU and CPU-fallback program batches; lit specular/spot and their overlap; first-record and
  report immutability; selected vertex-program word by accepted slot/component/port; the exact-1
  gate and first-sample rule; admitted-undelivered versus committed-zero availability; rejected
  stream rollback; nonzero-subchannel filtering; stopped/sub-minimum guards; every entry form.
- `test_pbcov_vk.c` runs 9 cases on real lavapipe, includes the production
  `nv2a_vk_backend.c`, installs the production back end and calls its draw/fallback site. It
  asserts `g.st.rc_fallback` equals the production coverage ledger's total, that the two reason
  counters sum to it exclusively, the per-stage/reason priority, first-refusal immutability,
  modes 0..5 supported (mode-3-cube exception), and modes 6/7 still refused.
- `scripts/pbcov-mutants.py` copies the toolkit and game privately, applies one named textual
  mutation at a time, builds only the gate targets, and requires the gate to fail with its named
  semantic assertion (never a compile error, skip or crash), then restores and requires the
  frozen baseline to pass. The shared checkout's HEAD, status and protected-file hashes are
  checked before and after every mutant.

**Evidence run.** Focused gate 56/56 in 2.4 s, zero skipped. Full limited-rights CTest
**160/160 in 444.13 s**, zero skipped, including the 56 new entries. `just check` green.
Private mutants **21/21 decided by their named assertion**, restored baseline PASS, shared
toolkit unchanged PASS. The mutation set covers the historical stage-1-only, first-record
mutability/live availability and selected-word provenance defects, plus dropped hooks,
histogram/batch double counts, report reset, presence-gating, ignored-subchannel pollution and
the Vulkan recording/reason/cube-exception/first-refusal properties. Inherently unobservable:
single-thread payload/valid ordering and torn concurrent reads; the observable equivalents are
first-record immutability and report retention.

**Limits.** Lavapipe only (no hardware GPU); the Vulkan half proves the refusal/recording site,
not pixels. Stream fixtures use a synthetic instance and pushbuffer, not the title. No
production defect was found. These are local gate results; acceptance is Codex's.

### §31.60 D4 gate review: supported mode 5 is untested; work directory is deleted (2026-10-21)

The step-2 gate candidate is rejected for two independently reproduced defects.
Toolkit production `2bcdc30` remains unchanged; these findings concern game test
code `576fa7a` and its mutation helper.

- `case_rc_supported_modes` claims modes 0 through 5 but its two batches only
  select 0, 1, 2, 3 and 4. Codex compiled a private mutation changing the real
  refusal rule from `m > 5` to `m > 4`. The deciding real-Vulkan case still
  passes (`STILL-PASS`, 0/1 mutants detected); the pristine and restored controls
  pass, and the shared toolkit is unchanged. This is a concrete missing
  boundary, not a claim of a production mode-5 defect. The earlier 21-mutant
  results remain their actual results; they do not cover this additional defect.
- `scripts/pbcov-mutants.py --work` recursively deletes an existing target
  directory before source copying. In an explicitly confined disposable probe,
  Codex placed a sentinel in its own scratch target, intercepted the first copy
  call and observed `existing_sentinel_preserved=False`. No original asset,
  save or unrelated user file was touched. A helper for private experiments must
  refuse an existing directory or establish ownership before deleting it.

Codex independently reran the registered coverage gate: 56/56 pass, 0 skips,
2.49 s. The reported full 160/160 receipt was inspected, not rerun in this review.
The fresh gate author's native transcript confirms `opencode-go/deepseek-v4.1-flash`,
high, fallback false. Production, features, scripts and pushes remain pending.

**Next:** a different-from-production test author repairs the supported-mode
fixture, adds the escaping mutant, and makes work-directory preservation a
deciding regression; production stays frozen. Require the old gate to miss the
mode-5 mutant and the repaired gate to reject it, valid-source positive controls,
an unchanged pre-existing sentinel and tested owned scratch cleanup. Then final
focused/full checks and independent review before step 3. The exhausted repair
allowance may be renewed under the skill's confirmed-review rule for this newly
demonstrated gate defect, retaining the six unused operation slots and all prior
spending; no operation-slot or context-based reset is authorized.

### §31.61 D4 gate repair: mode 5 exercised and work-area ownership (2026-10-21)

Test-only repair of §31.60. Toolkit production remains frozen at
`2bcdc3081362341b92a9efb8afad00bdfd005c85`; game candidate
`972665ac66b932060ec142ae989dd129e8e14a42` changes only the Vulkan gate fixture,
the mutation helper and one new helper regression/CTest. It is a candidate
pending Codex review; the four coverage scripts remain step 3.

- `case_rc_supported_modes` now draws every mode 0..5 at every stage 0..3 (24
  draws), one mode per draw, with a per-draw assertion that the production
  `rc_draws` increments by one and that `rc_fallback`, the ledger total/reasons
  and `first_valid` stay zero. The earlier case drew only modes 0–4; the
  supported-mode claim is now actually exercised. Cube-mode3 and mode6/7
  refusal cases are unchanged.
- The mutation table gains `vk-supported-mode5-rejected` (`if (m > 5)` →
  `if (m > 4)` in the real shared header). The old gate (`576fa7a`) passes that
  compilable mutant (`STILL-PASS`, recorded); the repaired gate fails it with
  `supported mode 5 stage 0 draws without fallback`; the restored source
  passes. The earlier 21-mutant results remain their named properties only.
- `pbcov-mutants.py --work` now refuses an existing path without deleting it,
  creates and stamps a new owned root, and removes only that root after
  checking its resolved path, directory identity and marker. The default temp
  root is still fresh and owned.
- `tests/test_pbcov_workdir.py` drives the actual helper entry with the copy
  boundary stubbed: an existing sentinel survives refusal before any copy, a
  failed fresh run removes only its owned root, `--keep` leaves it, and a real
  repository root is never offered to the pre-repair helper. The old helper
  fails these checks (`failures=2, errors=3`); the repaired helper passes all
  six (the real-game-root case is skipped for the old helper by design).

**Evidence.** Focused gate **57/57** in 2.75 s, zero skipped. Full limited-rights
CTest **161/161 in 488.41 s**, zero skipped. Private mutants **22/22** decided
by their named assertion; restored baseline PASS; shared toolkit unchanged
PASS. The mode-5 evidence and old-helper red control are in the ignored
`logs/reports/d4-step2-repair-private/` tree. `just check` is run on the final
tree. No production defect was found; no gameplay coverage is claimed.

**Independent acceptance of the coverage gate.** Codex reviewed the repaired
24-draw supported-mode sweep, refusal assertions, work-area acquisition/release
guards and six-case helper regression. It independently reran the focused suite:
57/57, no skips, 2.83 s. A fresh private mode-5 mutant run now fails its named
semantic assertion with pristine/restored positives and the shared toolkit
unchanged; the previously retained old-fixture run still documents its escape.
The helper regression against the archived old helper again exits 1 (two failed
assertions, three errors and its deliberate real-root skip); the repaired helper
passes. The full 161/161 receipt and final 22/22 mutation receipt were inspected,
not rerun wholesale by Codex. Native gate-repair identity is confirmed as
`opencode-go/deepseek-v4.1-flash`, high, fallback false. The diagnostics at toolkit
`2bcdc30` and game gate `972665a` are accepted for their bounded observational
contract. This does not accept rendering features, gameplay coverage or D4/D5
completion. Next: audited toolkit-first checkpoint, then the four cache-off Vulkan
coverage scripts with source/run identities, counts and retained availability.

PUSHED_TO: origin / BRANCH: main / COMMIT: 2bcdc3081362341b92a9efb8afad00bdfd005c85 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 1394777..2bcdc30, toolkit first; three diagnostic commits, 10 outgoing blobs, largest 268393 bytes, zero secret hits, only five reviewed kernel/video source paths. Upstream and nworkers untouched (push URLs DISABLED).
PUSHED_TO: origin / BRANCH: master / COMMIT: 94b9427a12f16f1f29ba2280e1f8e401f940a6ad / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward ffc9a32..94b9427 after toolkit; 13 commits, 27 outgoing blobs, largest 519650 bytes, zero secret hits, all per-commit paths limited to reviewed test/helper/CMake/plan/TR/profile records. No game assets, generated/recovered retail code or oversized blobs. Clean intended branches; final just check passed, focused gate 57/57 independently rerun, full 161/161 receipt verified.

### §31.62 D3–D4 coverage measured: texgen/matrix present, no bump batch observed (2026-10-21)

The accepted coverage diagnostics (toolkit `2bcdc30`, game `972665a`) were run over the four
existing keyboard scripts on Vulkan with `RECOMP_PB_COVERAGE=1` and the pipeline cache off. All
four are **EXPLORATORY** (`check-run-profile.py`: 0 unknown/missing), same executable
`11a6818a…`, build-source `27e94662…`, toolkit/project revisions matching the accepted trees, and
both SAVE roots inside each run's private archive. They are keyboard routes, not fidelity
evidence.

| Run | Script | `script_ok`/steps | last `[PBCOV]` snapshot (counts are lower bounds within 31–46 flips of the end) |
|---|---|---|---|
| `20261021-152045-759-keyscript-d4-coverage-e2e-vk` | e2e | true / 72 | texgen 255573, texmat 262624, overlap 255573, stage>=6 0, lit specular 71335, spot 0 |
| `20261021-153135-147-keyscript-d4-coverage-skate-vk` | skate | true / 71 | texgen 364240, texmat 369282, overlap 364240, stage>=6 0, lit specular 39724, spot 0 |
| `20261021-154209-085-keyscript-d4-coverage-gameplay-vk` | gameplay | true / 45 | texgen 324430, texmat 328544, overlap 324430, stage>=6 0, lit specular 24466, spot 0 |
| `20261021-155245-468-keyscript-d4-coverage-gum-tutorial-vk` | gum-tutorial | true / 93 | texgen 333990, texmat 338258, overlap 333990, stage>=6 0, lit specular 67939, spot 0 |

- **Texgen and texture matrices are live in all four runs.** `texgen_seen=7777` means S/T/R were
  delivered for all four stages; the Q bits stay clear (TEXGEN_Q is not admitted). The first
  texture matrix/enable snapshot is stage 0 only (`matrix_seen=…FFFF`, enable raw 1); stage 1
  matrix methods are admitted but had not been delivered before that claim, and stages 2/3 are not
  admitted. The first `first_c` state has lighting and specular enabled, light mask 1 (light 0
  infinite), no spot light; the spot parameter words are UNKNOWN (not admitted), not observed
  zeros.
- **No stage mode >= 6 was observed through the last periodic snapshots, and no Vulkan rc
  fallback was reported** (`stage_ge6=0`, `first_b=none`, `rc_fallback=0` in every report).
  The unreported terminal tails remain UNKNOWN; zero snapshots do not prove whole-run absence.
  `RECOMP_GPU_ACK` was absent (enabled by default), so classification is EXPLORATORY. No
  validation layer was enabled and no validation pass is claimed. The handoff's replanning trigger
  applies: fixture-driven BUMPENVMAP remains steps 4/5, and in-game confirmation waits for the
  optional owner-assisted step 8. This is not evidence of absence in unvisited scenes.
- The count (a) texgen/matrix criterion is positive, so the **conditional step-6 criterion is met**;
  no texgen implementation is attempted here.
- Specular lighting is positive but spot lighting is zero; lighting/ADXM stay deferred.
- No fatal, invalid ICALL, `[JUMP]`/`[ALIAS]`, backend stop or validation-error line appeared.
  Scenes actually reached: title/menu/intro 00–12, gameplay start, movement/jump, pause map
  (Garage is only a map label), skate street/rail, and the gum location with talk key presses; the
  viewed gum frames show no visible dialogue box, so a tutorial conversation is not confirmed.
- Corpus/identity: no run was rebuilt from unaccepted sources; no owner save or asset was used.

**Next:** step 4/5, fixture-driven BUMPENVMAP with its own different-author gate. Step 6 (texgen)
waits until bump and its gate land. D is not complete.

PUSHED_TO: origin / BRANCH: master / COMMIT: f49bd325c28ede5e067337db05442a2d8da550a6 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 7115372..f49bd32 (1 commit: the D4 step-3 four-run coverage measurements, TR §31.62 and plan next-step), clean and synchronized; 2 outgoing blobs (largest 523783 bytes), 0 secret hits, only docs/jsrf-technical-record.md and plan-jsrf-gameplay.md, no game/ or retail-derived path; toolkit unchanged at 2bcdc3081362341b92a9efb8afad00bdfd005c85 = origin/main.

**Owner pause (2026-10-21):** Codex independently verified the four measurement archives,
artifact/log hashes, all 250 recorded source inputs, private SAVE roots, profiles and retained
counts, and viewed the pause-map, skate, gameplay and gum captures. Step 3 is accepted only as
exploratory measurement. Pause before step 4; no BUMPENVMAP or texgen implementation has begun.
Resume with fixture-driven CPU/AOT-Vulkan BUMPENVMAP (step 4), then its different-author gate
(step 5), conditional texgen/matrices and separate gate (step 6), then records/final checks (7).
D remains incomplete. No next packet is authorized while the owner pause is in effect.

PUSHED_TO: origin / BRANCH: master / COMMIT: e626e62861fe82af0176e89f8ebeef6a71a84b6e / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 2c13d8c..e626e62; owner pause after independently verified step-3 measurements, final just check passed; clean intended branch, only plan/TR, 2 outgoing blobs (largest 525255 bytes), zero secrets or forbidden/oversized paths. Toolkit unchanged and synchronized at 2bcdc3081362341b92a9efb8afad00bdfd005c85; no toolkit push needed. Bridge STOP dec_0mvizmpv0aac1f9f84eb5bcc0 authorizes no next packet; four normal slots remain, zero repair cycles.

### §31.63 Windows preflight of the macOS changes: runtime green, two test/policy blockers (2026-10-22)

Owner resumed after the D4 pause and ordered Windows validation of the macOS/portability changes before
any step-4 work. Sources: toolkit `110e169e989b8466c84af31872c030b3571c8b4d` (commits `52af52e`,
`9f03bfa`, `110e169`), game `40353d67db8ca5c9be1b0e6688030a1f5de27499` (`401d8c5`, `40353d6`). Both
fetched, clean, 0/0 origin; toolkit unchanged during the packet.

**Build.** `just build` (guarded MSVC Release) succeeded; no error. The full MSBuild log has 46 warning
lines: 18 known MSB8028 shared-intermediate-dir warnings plus 28 compiler warnings (C4018 1, C4100 1,
C4189 3, C4210 11, C4244 5, C4245 2, C4324 1, C4456 3, C4702 1). They are in the touched toolkit files
at lines outside the portability hunks; the added lines in `nv2a_vk_backend.c`, `win32_compat.c` and
`nv2a_mmio_hook.c` produced none. `/WX-` is unchanged.

**Full CTest: 160/161, one failed, zero skipped** (`logs/windows-macos-preflight-ctest.log`).

- **`jsrf_optional_vulkan` failed** (105.6 s), in `test_every_mutant_is_detected` and
  `test_the_mutants_are_copies_with_only_their_edits`, mutant M-A. The game's `JSRF_FIXTURES_ONLY`
  change (`401d8c5`) rewrote the capability branch to
  `if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)`; the fixture's M-A anchor still expects
  `if(XBR_VULKAN_FOUND)\n    # The corpus, generated by scripts/gen-shaders.py`, count 0. All
  non-mutant cases of that test passed. This is a stale test anchor caused by the portability change,
  not a runtime defect.
  **Proposed minimal repair:** update the M-A `subs` anchor to the new condition (keep the `if(TRUE)`
  replacement), rerun `jsrf_optional_vulkan`; a different author's gate must show M-A red, the
  comment-only control and the other mutants unchanged. No production change.
- `xbox_guest_meter` passed; no timing rerun was needed.

**`just check` failed on one finding.** `scripts/check-override-drift.py` reports `RECOMP_VK_DRIVER` is
read by toolkit `src/video/nv2a_vk_backend.c` but is in neither `scripts/jsrf_run_profile.py` nor
`docs/jsrf-run-profiles.md`. The knob selects a device by driver name/info (case-insensitive
substring, together with `RECOMP_VK_DEVICE`) and synthesizes no guest value, so the likely
classification is feature enablement/device selection. **Proposed records-only repair:** document it in
`docs/jsrf-run-profiles.md` as device selection (and classify it in the runner's table if needed), then
rerun `just check`. No production change; the classification decision belongs to the next review.

**Bounded keyscript** `win-macos-portability-e2e-vk` (620 s, e2e, Vulkan, pipeline cache off; no
coverage env): `script_ok=true`, 72 steps, all waits satisfied, runner deadline stop
(`killed=True`, exit 1, 620.2 s), 0 ICALL/fatal, `vsh_misses=0` (1,530,698 hits), `rc_fallback=0`,
42 pipelines, `validation_errors=0`/`validation_warnings=0` **with no validation layer enabled** (not a
validation pass). Profile EXPLORATORY, 0 unknown/missing; both `[SAVE]` roots inside the run archive;
positions move (408,490)→(108,527). Device: `[GPUVK] device: NVIDIA GeForce RTX 4070 Laptop GPU (type
2, api 1.4.351, driver 0x9A170000) driver: NVIDIA (616.92)`, the new driver suffix working on Windows.
Software-device witness: `vk_smoke_sw` and `jsrf_pbcov_vk_rc_unsupported_modes` both report
`llvmpipe (LLVM 23.1.2, 256 bits)`; the latter prints `[GPUVK] device: llvmpipe ... picked by
RECOMP_VK_DEVICE driver: llvmpipe (Mesa 26.2.4 ...)`.
Frames viewed: title, main menu, opening gameplay, pause map (Garage is a map label; no 3D Garage).
Ledger rows: L18, L21–L24, L57/L58, L62. Exploratory only; no fidelity claim.

**Next:** the requested different-worker READ-ONLY independent review of toolkit `52af52e`/`9f03bfa`/
`110e169` and game `401d8c5`/`40353d6` (Windows behavior, honest macOS skips/exclusions, portability
subset). The two findings above are its inputs; step 4 waits. No production or test was patched in this
packet, and D4 is not complete.

PUSHED_TO: origin / BRANCH: master / COMMIT: 4bd44f659d3ee18236bee1681c2d11d545e91fde / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 40353d6..4bd44f6 (1 records commit: the Windows portability preflight findings, TR §31.63 and plan), clean and synchronized; 2 outgoing blobs (largest 529937 bytes), 0 secret hits, only plan-jsrf-gameplay.md and docs/jsrf-technical-record.md, no game/ or retail-derived path; toolkit unchanged at 110e169e989b8466c84af31872c030b3571c8b4d = origin/main.

### §31.64 Preflight repair: honest exclusions, driver classification, and a fixtures-only gate (2026-10-22)

Bounded test/docs repair of the two preflight regressions and the review's disclosure/portability
findings. Toolkit stays `110e169`; game candidate is local and unpushed until Codex verifies it.
No runtime, shim, allocator, suballoc-oracle, concurrency or portability-feature code changed.

- **C1 M-A anchor.** `tests/test_optional_vulkan.py` now anchors on
  `if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)` and replaces it with
  `if(TRUE AND NOT JSRF_FIXTURES_ONLY)`, removing only the capability test and preserving the
  fixtures-only axis; the docstring matches. The focused `jsrf_optional_vulkan` passes
  (122.51 s); the independent test author measured anchor count 1, M-A detected with three failing
  children, comment-only control green, and M-B/M-C/M-C2 unchanged.
- **C2 `RECOMP_VK_DRIVER`.** Added to the feature-enablement table beside `RECOMP_VK_DEVICE`: it
  filters by `driverName`/`driverInfo`, composes with the device filter, is inactive when
  empty/unset, never matches a device that cannot report driver properties, supplies no guest
  value, and no match leaves the CPU path. No classifier weakening; `just check` is green.
- **C3 exclusion honesty.** CMake now prints grouped STATUS messages for the previously silent
  families (`jsrf_save_root`, `jsrf_recovery_11c1`, all `jsrf_pbcov_stream_*`, fence/APU/A2H/
  AC'97/native-GPU) and for the fixtures-only Python omissions. `docs/jsrf-macos-build.md` records
  the full inventory and rewords the configure promise; skips for known defects stay explicit
  debt.
- **C4 suballoc label.** The skip and doc now say the 63,110,912 vs 63,112,704 (~1,792-byte)
  difference is consistent with the allocator's separately reported `padding_bytes` omitted by the
  pool claim, plus a double-counted dedicated allocation in the budget. That is a test-oracle
  finding and a stated inference, not measured on a Mac here; the skip remains until a measured
  investigation. No backend change.
- **C5 pipeline cache.** The run-profile row now documents the actual POSIX defaults:
  `$XDG_CACHE_HOME` when absolute, else `~/Library/Caches` on macOS or `~/.cache` elsewhere, off
  when neither variable nor `HOME` is usable; Windows keeps `%LOCALAPPDATA%`.
- **G1 fixtures-only gate.** New `tests/test_fixtures_only.py`, registered as
  `jsrf_fixtures_only`, copies only 273 tracked files to a temp tree (no `src/recomp/gen`,
  `src/recomp/recovered`, `game/` or retail corpus), runs a real
  `-DJSRF_FIXTURES_ONLY=ON -DBUILD_TESTING=ON` configure with the capability ON, checks the
  generated empty corpus (`nv2a_vsh_program_count = 0`), no `jsrf_recomp`/`jsrf_collect` projects,
  and compiles/runs a consumer of the selected table (`count=0 found=0`). A private line-111
  mutant removing `AND NOT JSRF_FIXTURES_ONLY` and a generated-tree negative control both fail;
  restored baseline passes; shared hashes unchanged. Focused 43.83 s.

**Validation (this host).** Guarded `just build` succeeded, 162 registered tests. Full limited CTest
**162/162, zero failed/skipped, 559.42 s**; `just check` green. The C1 and G1 focused receipts are
above. No Mac/MoltenVK run was made and no Mac outcome is claimed.

**Remaining debt/open findings (not repaired here):** measured MoltenVK suballoc oracle
investigation (padding and dedicated double count); POSIX handle lifetime (`ResetEvent` and
friends; `jsrf_inplace_event_bridge` stays unregistered off Windows); vsh shared-scratch `-j` race
and `jsrf_vsh_aot` log name (lock only non-Windows today); parity warnings around 2760/7170;
fixtures-only POSIX shim test not run by a runner; 16 KiB-page subcase still PASS with a skipped
case. D4 remains incomplete; step 4 waits.

### §31.65 G1 binding and C3 disclosure correction (2026-10-22)

Codex rejected candidate `58cd601` for a G1 evidence hole: a private tracked-only tree with
`set(JSRF_CORPUS_SOURCES "${_jsrf_empty_dir}/nv2a_vsh_programs.c")` cleared to `""` still passed,
because the consumer hard-coded `<build>/empty-corpus/nv2a_vsh_programs.c`; the real configured
`jsrf_vsh_conformance_test.vcxproj` then compiled no corpus source. It also found the new C++
exclusion group was placed in a branch that a fixtures-only configure never evaluates.

**Repair (candidate local, unpushed).**

- `tests/test_fixtures_only.py` now parses the actual configured
  `jsrf_vsh_conformance_test.vcxproj` `ClCompile` entries, requires exactly one entry resolving to
  the generated empty table (missing/duplicate/wrong table fail separately), checks the include
  binding where the project declares one, and compiles/runs the consumer against that derived path.
  A third private control clears `JSRF_CORPUS_SOURCES`; the gate fails with
  `missing corpus binding: jsrf_vsh_conformance_test.vcxproj has no ClCompile entry for the
  generated empty corpus …`. The line-111 capability mutant, the generated-tree negative control,
  source-only input absence and shared-hash controls are retained; baseline/restored pass.
- CMake now prints the C++ exclusion groups in the branch a fixtures-only configure actually
  executes (fence/APU-watch/collect-arming/A2H/AC97/native-GPU), adds `collect_arming_gate_on/off`
  to the off-Windows group, and discloses that `jsrf_fixtures_only` itself is not registered in
  fixtures-only mode (needs a tracked checkout/Windows tool layout; avoids recursion). The macOS
  doc inventory and wording match. The G1 gate asserts those groups appear in the real fixtures-only
  configure output, not merely in CMake source text. No test selection changed.

**Evidence.** Final `tests/test_fixtures_only.py` sha256
`46e0d23999efda2259579154363e09fc0f115c2d458f348fcacdbcab5fd13d74`; CMakeLists
`f873ebfd8f40ec1988b32b36cb103547108cc991a5e55c6c5c3b34cc7082888e`. Focused
`jsrf_fixtures_only` 1/1 in 66.23 s: baseline PASS binding the actual project entry
(`empty-corpus/nv2a_vsh_programs.c`, consumer `count=0 found=0`), line-111/gen-tree/cleared-corpus
mutants rejected, restores PASS, shared hashes unchanged. Focused R2 `jsrf_optional_vulkan` 1/1 in
118.08 s; M-A anchor unchanged. Guarded build succeeded; full limited CTest **162/162, zero
skipped, 677.54 s**; `just check` green on the final tree.

**Status.** `58cd601` remains a rejected local candidate in history; the new candidate is local and
unpushed pending Codex verification. No Mac/MoltenVK run. Remaining debt is unchanged (measured
suballoc oracle, POSIX handle lifetime, vsh scratch race/log, parity warnings, fixtures-only shim
test, 16 KiB subcase). D4 incomplete; step 4 waits.

### §31.66 Preflight gate repair independently accepted (2026-10-22)

Codex independently inspected the final configured-project binding and reran the exact private
counterexample that escaped `58cd601`. With the corpus source cleared, the repaired gate now fails
specifically on `missing corpus binding`; the shared source remained unchanged. The different
gate worker's native transcript confirms the configured task role, high effort and no fallback.

The full limited-rights CTest receipt at `ab0ef14` is **162/162, zero skipped, 677.54 s**, with a
successful guarded build and `just check`. Codex made one final announcement-only integration
correction: the fixtures-only message now names the full-runtime blocks it actually omits on
every host, rather than also calling the Windows save-root and committed-stream fixtures omitted
when their earlier Windows guards still register them. No test selection or runtime code changed.
After that correction, the guarded build passed and Codex reran the complete G1 CTest **1/1 in
76.40 s**: pristine and both restores passed, all three private mutations failed for the intended
reasons, actual conformance-project binding supplied the consumer's table, and the shared input
hashes remained identical. Final CMake SHA-256 is
`649d346cbe60ffa1815bb14a58cc17e3180e220ce9b7dfba76c48be62a2be9b9`; the gate source remains
`46e0d23999efda2259579154363e09fc0f115c2d458f348fcacdbcab5fd13d74`.

The Windows portability test/disclosure repair is accepted. The old rejected candidate is
retained as history; only the repaired final tree is the accepted checkpoint. This accepts no
Mac runtime or new MoltenVK result. Open findings remain: test-oracle padding/dedicated accounting,
POSIX handle safety, shared scratch under parallel CTest and the parity-test warnings. Existing
allocator-gate property 2d already exercises a nonzero padding value with the production stats
function; it is a useful positive control for the next bounded investigation, not a measurement
of the historical MoltenVK 1,792 bytes. D4 remains incomplete.

PUSHED_TO: origin / BRANCH: master / COMMIT: 81a032aaf4884a0326ed6fc03075dbf268a71250 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward f1963fb..81a032a (three retained commits, final repaired tree independently accepted); clean and synchronized after push, 15 outgoing blobs, largest 539185 bytes, zero secrets, only the reviewed CMake/test/profile/macOS-doc/plan/TR paths, no forbidden or oversized assets. Final guarded build, complete G1 with all three negative controls, and `just check` passed after the announcement-only integration correction; full 162/162 receipt is at ab0ef14. Toolkit unchanged and synchronized at 110e169e989b8466c84af31872c030b3571c8b4d; no toolkit push needed.

### §31.67 Test-portability debt: allocator oracles, parallel scratch locks, parity warnings (2026-10-22)

Bounded test-only repair of the Windows-exposed review findings. No runtime/backend/POSIX code
changed; toolkit stays `110e169`. Candidate local/unpushed until Codex verifies.

- **Suballocation oracle (P2a).** The workload gate now asserts
  `reserved == used + free + padding` instead of omitting padding. The old form was short by
  exactly the padding on a controlled production-allocator case: a 256-byte bump followed by a
  2048-aligned take yields **padding 1792**, `4194304 = 2304 + 1792 + 4190208`; the corrected
  oracle passes, the old `free+used==reserved` fails by 1792, and each deliberately
  misaccounted field is rejected. Existing property 2d's 57344-byte padding still passes.
  **The historical MoltenVK 1792 attribution remains INFERENCE** — this controlled Windows case
  did not measure a Mac.
- **Allocation-reduction claim (P2b).** The double count is removed:
  `alloc_calls_delta - dedicated_delta <= ranges_delta/2` (pooled block allocations), with the
  existing identity `alloc_calls == blocks + dedicated` and a new success-path identity
  `dedicated == required+prefer+oversize+noroom+nohost+fallbacks`. On the controlled case the
  old `alloc_calls + dedicated` form fails (3 > 1) and the corrected form passes (1 <= 1).
  Real llvmpipe and NVIDIA `--suballoc` workloads report `padding=0`, `dedicated=0`, so they pass
  both old forms and cannot decide the mutations; they still pass all guest-byte, readback,
  reuse, lifetime and parked-loss checks. The controlled allocator case is the deciding evidence.
- **Parallel scratch.** `RESOURCE_LOCK` now applies on Windows for `^jsrf_vsh_conformance` and
  `^jsrf_vsh_aot($|_control)`; the non-Windows `TEMP` override remains separate. Actual CTest
  metadata shows 12 of 15 vsh tests locked (the malformed/table/provenance tests use different
  paths). Two sequential `ctest -j4 -R "^jsrf_vsh_conformance"` runs passed 9/9 each and the AOT
  family passed 6/6. **Limit:** CTest's `RESOURCE_LOCK` serializes tests within one ctest
  process; two independent ctest processes started concurrently still shared `%TEMP%\
  vsh_conformance` and one observed a deterministic-looking X3 empty-output failure. No
  cross-process claim is made; unique per-process scratch paths or an external lock would be a
  further test-source change.
- **Warnings.** `rr_a_sum_deciding` now uses integer `sum`/`prod` (no `abs(float)`), and the
  unused `_snprintf` argument is dropped; the deciding predicate is unchanged. Installed
  clang-cl 23.1.2 reports no diagnostic at the repaired sites (an old-form control emits
  `-Wabsolute-value` at 2760; it does not diagnose `_snprintf` arity, but MSVC `/W4` reports
  C4474 on the old extra argument and none on the final file). The MSVC build has no warning
  from the file.
- **Evidence.** Focused `jsrf_vk_suballoc_gate` 1/1 (all 11 properties, 357 checks; the
  `--padding-1792` mode 22 checks), `jsrf_vk_suballoc_device` 1/1 on llvmpipe, parallel
  conformance 9/9 twice and AOT 6/6, full limited CTest **162/162, zero skipped, 652.13 s**.
  Private old-predicate copies fail on the controlled case; restored copies and shared hashes
  pass/unchanged. No Mac claim; the MoltenVK skip remains.

**Next:** the separate final preflight operation for POSIX handle-lifetime validation on WSL
(native cc, no CMake install/SSH); then D4 step 4. D4 remains incomplete.

### §31.68 Oracle binding: the controlled gate calls the workload's actual predicates (2026-10-22)

Codex rejected candidate `4793a1d` for an unbound gate, not for backend arithmetic: the workload
oracle fix was mathematically correct and the controlled 1792-padding case was valid, but
`tests/test_vk_suballoc_gate.c` kept its own copies (`gx_bytes_close`, inline
`alloc_calls - dedicated <= ranges/2`) and never called the predicates used by
`tests/test_gpu_backend_parity_vk.c:run_suballoc`. A private change to the actual workload
predicates therefore could not fail the controlled gate.

**Binding.** New test-only `tests/vk_suballoc_oracles.h` owns the three predicates:
`vk_suballoc_bytes_close` (reserved - used - free == padding, overflow-safe),
`vk_suballoc_reduction_close` (alloc_calls >= dedicated and the pooled form), and
`vk_suballoc_dedicated_reasons_close` (one reason per dedicated allocation). `run_suballoc` calls
those exact bodies through `sub_claim_oracle`; property 11 of the controlled gate includes the
header and calls the same bodies as its deciding checks. The old forms remain display-only
(`gx_bytes_close_old`, inline double-count) and are never the positive decider. Both CMake targets
list the header and its include directory, so a header edit recompiles both consumers.

**Private final-source controls (outside `%TEMP%`, because MSBuild there drops incremental header
tracking — MSB8029 was observed).** Mutating only the shared header:

- drop padding (`return used + free == reserved`): both `test_vk_suballoc_gate.c` and
  `test_gpu_backend_parity_vk.c` recompiled (object hashes changed); the controlled gate exits 1
  on 3/24 checks with the shared byte-oracle assertion; restored header passes. The llvmpipe
  workload still passes because it reports `padding=0`/`dedicated=0`, so the controlled case is
  the deciding evidence.
- restore dedicated double count (`alloc_calls + dedicated <= ranges/2`): both consumers recompile;
  the gate exits 1 on the shared reduction assertion (2 - 1 <= 2/2 fails); restored header passes.
- Shared header SHA `9ccaf2216a2debba89e2b44d4207c58bb0c5af94b6d4411303d3c92c52587ea6` unchanged
  after restore; gate source `5eb4b56c65960c2d373df6453c80b6fb738fc895e9ff0db45a94cd1163c19ad9`.

**Real evidence.** `jsrf_vk_suballoc_gate` 11 properties/359 checks; `--padding-1792` 24 checks
with `reserved 4194304 = 2304 + 1792 + 4190208`; real llvmpipe `--suballoc` passes with
`padding=0`, `dedicated=0` and all guest-byte/readback/lifetime checks. Guarded build, full
limited CTest and `just check` are run on the final tree. No Mac/MoltenVK run; the historical
1,792 attribution remains an inference and the MoltenVK skip remains debt. Candidate local and
unpushed; D4 remains incomplete.

### §31.69 Bound allocator-oracle repair independently accepted; POSIX failure reproduced (2026-10-22)

Codex independently verified that both `run_suballoc` and the controlled allocator fixture include
and call `tests/vk_suballoc_oracles.h`. The controlled production-allocator case passes all 24
checks with exactly 1,792 bytes of padding and a dedicated allocation. In a separate native WSL
C test compiled from the actual shared header, pristine/restored inputs pass, dropping padding
fails the byte predicate, and restoring the dedicated double count fails the reduction predicate
(exit sequence 0/1/1/0). This supplements the different author's private Windows header-edit
rebuild receipts: both fixture objects recompile, and the controlled gate rejects each mutation.
The llvmpipe workload remains green on those mutations because its padding and dedicated counts
are zero; it is not the deciding negative control.

The final source receipt at `1c8bc4a` is **162/162 CTests, zero skipped, 695.58 s**, a successful
guarded build and `just check`. The first full run failed G1 while the new shared header was still
untracked; after staging the header, the focused G1 and final full suite passed without weakening
the test. The bound test-debt repair is accepted: correct allocation oracles, shared scratch locks
within one CTest invocation, and the two parity warning fixes. Separate CTest processes remain
uncoordinated; no broader race-freedom or Mac result is claimed. The historical MoltenVK 1,792
bytes remain unverified and its suballocation test remains skipped.

**Next repair, independently reproduced.** With toolkit `110e169`, Codex compiled the actual
`win32_compat.c` plus the exit-hook source using Ubuntu WSL's installed `cc`, with core dumps
disabled and a bounded subprocess alarm. The ordinary event create/wait/reset/set/close control
passes. `ResetEvent((HANDLE)0x03FFFFFE)` terminates the subprocess (WSL exit 11). A separately
allocated zeroed 4 KiB non-object is wrongly accepted by `ResetEvent` (TRUE, last-error 0), because
`K_EVENT == 0` and the shim dereferences the supplied pointer before validating its provenance.
Source SHA-256 is `659f39a0b58314ddcf6c9e5b7bb48d607ee6ae2322168a70aab43cdb384aff1d`.
The safe next increment is lifetime-aware validation against known live shim handles, with
different-author native POSIX regressions; a range check or magic/kind word at an arbitrary pointer
is insufficient. These are Linux observations, not Mac runtime evidence. D4 remains incomplete.

PUSHED_TO: origin / BRANCH: master / COMMIT: 21f1f0df777594aa1cc98919135601b840255bc0 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 9695d81..21f1f0d (three retained commits, final bound gate independently accepted); clean and synchronized, 15 outgoing blobs, largest 548695 bytes, zero secrets, only reviewed CMake/test/macOS-doc/plan/TR paths, no forbidden or oversized blobs. Final code has full limited CTest 162/162 and `just check` green; Codex's actual shared-header controls and production-allocator case passed, and record-only hooks passed before push. Toolkit unchanged and synchronized at 110e169e989b8466c84af31872c030b3571c8b4d; no toolkit push needed.


### §31.70 POSIX repair interrupted by configured provider usage exhaustion (2026-10-22)

The bounded POSIX handle-safety operation ended before implementation or validation when the
configured primary OMP provider returned HTTP 429 (`Go usage limit exceeded`). Its lifecycle
completion is not a completed repair. No different-author gate report or new Windows test result
exists. Game code remains the accepted checkpoint; toolkit `110e169` has a preserved uncommitted
127-line registry-helper draft in `src/platform/win32_compat.c`. None of the helpers is connected
to allocation, public handle APIs or close. It is unfinished and unaccepted; no toolkit code was
committed or pushed.

Codex independently compiled the actual changed shim with Ubuntu WSL's installed cc and replayed
the bounded native probe: ordinary event control passes (exit 0), the invalid address still
terminates (exit 11), and a zeroed non-object is still wrongly accepted (exit 2). Source SHA-256
`669eab0fb7ae0fd0ef56c1c5a9f585c70ccfbbc69cf6d27d41b4450eceed33d2`. These are Linux observations,
not Mac evidence. Preserve the draft for completion; its alias-withdraw helper also requires
review so every successful close releases one public lifetime reference, including non-final
alias closes.

The configured role is blocked for owner handling under the orchestrator skill's human-owned
model policy. No substitute role/provider/model, automatic retry or allowance renewal is
admitted for this unresolved external blocker. The preflight has consumed seven operations and
four repair allowances; none remains. The separate paused D4 lineage retains exactly four normal
operations and zero repair cycles. Resume the POSIX repair and its required independent gate
before D4 steps 4–5 after role availability/authorization and supported bridge reconciliation.
The last accepted Windows checkpoint remains 162/162 CTests; this draft has no validation claim.

PUSHED_TO: origin / BRANCH: master / COMMIT: aea24021469b61e621accd6a63be384a5445bfdc / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward c62e879..aea2402; clean intended documentation checkpoint, two reviewed plan/TR blobs (largest 551336 bytes), zero secrets and no forbidden or oversized paths. Record-only hooks passed; no code or new runtime validation claimed. Toolkit draft preserved uncommitted; no toolkit push.

### §31.71 POSIX handle safety: a live-handle registry with lifetime pins (2026-10-22)

The `ResetEvent` defect in §31.69–31.70 is repaired, with the lifetime design corrected and a
different author's native gate. Sources: toolkit `110e169e989b8466c84af31872c030b3571c8b4d`
plus this repair; game `2ff34cc5405877df1409fd3ec8d2838672aef4b6` plus these records.

**Cause.** On POSIX a `HANDLE` *is* the `w32_object *` the shim allocates, and every public entry
point cast the caller's value and dereferenced it; `CloseHandle` released whatever it was handed.
`K_EVENT == 0` made a zeroed block read as a valid event. Measured with Ubuntu WSL `cc` (gcc 15.2),
Linux only, core dumps disabled, bounded alarms: original source `9fda5b43…` segfaults on
`ResetEvent((HANDLE)0x03fffffe)` (exit 139) and **accepts** a zeroed 4 KiB `calloc` block
(`=1 error=0`, exit 2). The ordinary create/wait/reset/set/close control passed before and after.

**Repair.** `w32_object` carries an intrusive bucket link and two counts; a 256-bucket table keyed
by the handle's own value is the only authority for liveness. `refcount` is total lifetime
references (public aliases plus internal pins: an API call's pin, a thread trampoline, a timer
helper) and `public_refs` counts public aliases only. A pin is taken under the same lock that
decides liveness, so a lookup either wins before a concurrent withdrawal or fails after it — never
pins a freed object — and nothing is dereferenced before the pin succeeds. **Every successful public
close drops exactly one lifetime reference, final or not**, and the last close unlinks the value
before that release. That is the correction to the interrupted draft, whose withdraw returned an
object only on the final close and would have leaked a reference on every non-final alias close;
its helpers were also disconnected from allocation, the APIs and `CloseHandle`. Audited entry
points: event set/reset/pulse, semaphore release, mutex release, single and multiple waits (every
handle pinned once before the polling loop; a bad entry fails the call instead of being skipped),
`GetThreadId`, `GetExitCodeThread`, resume/suspend/terminate, thread priorities, `QueueUserAPC`,
timer-queue change/delete, waitable-timer set/cancel, `MapViewOfFileEx`, the fd/path helpers,
`CloseHandle` and `DuplicateHandle`. Failures are `FALSE`/`NULL`/`(DWORD)-1`/`WAIT_FAILED` with
`ERROR_INVALID_HANDLE` (and `ERROR_INVALID_PARAMETER` for a NULL out-parameter). Pseudo handles,
the process-heap sentinel, the opaque 32-bit guest token mapping and the native Windows path are
unchanged; `thread_pin` resolves the calling thread's own object directly, so `SuspendThread(self)`
and waits on a thread's own value keep working after the guest closed the public handle.

Retained semantics, recorded rather than glossed: `DuplicateHandle` still returns the **same value**
(the guest ABI maps tokens by value), so a duplicate is an alias usable until the last public alias
closes; `w32_handle_fd`/`_path` are **borrowed** views whose result is not lifetime-protected past
the lookup, as with a raw fd; a freed value can be reused by a later allocation at the same address
and then names a different object, which the value-is-pointer scheme makes unavoidable and the
contract does not forbid. No general race freedom is claimed.

**Gate (different author, `PosixGateAuthor`).** `tests/win32_compat_posix/test_handle_lifetime.c`
(175 checks, `sha256 63e061fd…`) and `test_handle_lifetime_asan.c` (22 checks,
`sha256 0750ad9e…`), registered in the existing standalone POSIX project (the ASAN target behind a
real `-fsanitize=address` compile test) and built directly with the installed WSL `cc` because no
CMake exists there. That target compiles the shim sources INTO itself with the sanitizer on
(toolkit `0f3e7af`) instead of linking the `platform` library, which `add_subdirectory` had already
built unsanitized: an instrumented test linked against an uninstrumented shim diagnoses only the
test's own code, and every defect the target exists to catch happens in the shim. In that exact
shape the linked image carries the sanitizer's symbol references and the controls still decide --
pristine 22/0, early-release a LeakSanitizer leak, bypass and no-withdraw an AddressSanitizer
heap-use-after-free. Final shim `sha256 094e7884…`: **plain 175/0, ASAN 22/0** (leaks on, no
suppression), reproduced independently by the orchestrator. Private weakening controls, all
re-run by the orchestrator: **bypass** (lookup skips the registry) → SIGSEGV on the plain binary and
heap-use-after-free under ASAN; **no-withdraw** (entry never unlinked) → SIGSEGV with 5 FAIL rows
including `a second CloseHandle of the same value returns FALSE` and `WaitForSingleObject on the
closed value returns WAIT_FAILED`; **early-release** (non-final close skips the release) → invisible
to the plain binary but **LeakSanitizer: `Direct leak of 59200 byte(s) in 100 object(s)`**, which is
why the ASAN companion is load-bearing rather than decorative. The original committed shim is RED on
both required rows (SIGSEGV on the invalid address; 19 false-accept rows then
`double free or corruption` on the zeroed block) and fails the full plain run at 52 pass / 2 FAIL.
Pre-existing `win32_compat_posix/test_main.c`, `test_interlocked.c` and
`win32_thread_posix/test_main.c` all build and pass against the final shim.

**`jsrf_inplace_event_bridge` integration: unvalidated, still not registered off Windows.** The
fixture and every toolkit source it needs compile, but it cannot be *linked* on the measured
Ubuntu WSL host: the POSIX providers of `xbox_FramebufferWindow*`, `xbox_VideoIsPlaying`/`PlayFile`
and `xa2_*`/`mcpx_apu_*` live in the SDL2 and XAudio2 sources, and that image has no SDL2/epoxy/
OpenSSL headers, no CMake and no pkg-config; 35 undefined references remain. Installing them is out
of scope. **A Mac is untested**: no Mac build or run was attempted, and nothing here claims a Mac
host carrying the SDL2 prerequisites the macOS guide already lists would fail the same way. The **old
exclusion reason is now stale and corrected** in the game `CMakeLists.txt` and
`docs/jsrf-macos-build.md`: the case passes a non-event guest VA as a handle, and the shim now
returns `FALSE`/`WAIT_FAILED` for a value it never published instead of dereferencing it — measured
by the native probe, not by running the fixture, which cannot be built here. **What that fixture's
own header-validation case now returns is untested on this host**; only the shim's rejection of the
value was measured. The registration stays off Windows because of the missing POSIX link providers,
not a crash. No Mac run was made and no Mac result is claimed.

**Validation (this host).** Guarded `just build` succeeded (0 compile/link errors, 0 C4310, 18 known
MSB8028), executable `sha256 c7c6e292…`; targeted Windows `jsrf_inplace_event_bridge` (4.16 s),
`jsrf_dpc_queue_bridge`, `jsrf_callback_reentry`, `jsrf_save_root` and the fence-release pair all
pass; full limited CTest **162/162, zero failed/skipped, 665.83 s**; `just check` green including the
real retail `check-vsh-aot-table` (126/126). One bounded 8 s strict Vulkan smoke
(`20261022-101153-213-posix-handles-smoke`, `RECOMP_GPU_ACK=0`, pipeline cache off, isolated save
root verified): RTX 4070 `driver: NVIDIA (616.92)`, checkpoints `memory_ready`/`guest_entry`,
36 native threads, 325 named frames, **zero ICALL and zero FATAL**, `[AC97] serviced=40 refused=0`,
outcome `diagnostic_deadline` — a liveness check, not a fidelity or e2e claim. The earlier 620 s
NVIDIA e2e remains the previous Windows gameplay evidence. D4 remains incomplete; step 4 waits.

PUSHED_TO: none (candidate held for independent Codex review) / BRANCH: master / COMMIT:
toolkit 7c46fa693ba03a9bf4b05ea609bb307a298a9bfc, 0f3e7af (ASAN target) and b06bd36546e7bec75f17ca9677d7b55a8d59e01a (multi-wait); game 2102a869fa2e0fda8161f6d15ee57ef4409440cf, d67ef8f, aac7ea9, 9673b3d / RESULT: local commits only; no push, no force, toolkit first then game.

### §31.72 Multi-object waits: sentinel classification and self-aware resolution (2026-10-22)

Codex independently **rejected** the §31.71 candidate for a measured regression (R1), and this
bounded repair closes it. Sources: toolkit `0f3e7af` plus this repair; game `9673b3d` plus these
records.

**The defect.** `WaitForMultipleObjects(Ex)` resolved each handle with `obj_pin()` and used a NULL
pin to mean "this entry is satisfied", then rejected every non-NULL input whose pin was NULL. The
supported sentinels `NULL`, `(HANDLE)-2` and `(HANDLE)-1` took that NULL-pin path and tripped the
failure test, so a wait mixing a real event with `NtCurrentThread` returned `WAIT_FAILED` with
last-error **0** — even in a wait-any whose first real event was already signalled — where the
single-object wait has always answered `WAIT_OBJECT_0`. Separately, ordinary values went through
`obj_pin`, which consults only the public registry, while the single wait used `thread_pin`, which
also resolves the calling thread's own object; so after the guest closed the public handle for its
own thread, single waits worked and multi-waits failed. Measured by Codex on the actual shim: 12
mixed pseudo/real rows old 12/12 pass, candidate 8 fail; closed-self single/multi/Ex old
`0x102`/`0x102`/`0x102`, candidate `0x102`/`0xffffffff`/`0xffffffff`. The §31.71 gates passed on that
candidate because **neither test source called `WaitForMultipleObjects` at all** — a hole exactly
where the defect was.

**The repair** (`src/platform/win32_compat.c`, SHA-256 `7d25e6b7…`; pre-repair `094e7884…`). A wait
entry is an explicit classification — `{ w32_object *obj; int sentinel; }` — not a bare pointer.
`wait_entry_resolve` marks `NULL`, `PSEUDO_CURRENT_THREAD` and `PSEUDO_CURRENT_PROCESS` as sentinels
and succeeds for them; every other value goes through **`thread_pin`**, the same self/lifetime-aware
resolution the single wait uses, so an unpublished, forged, dead or wrong-kind value still fails with
`ERROR_INVALID_HANDLE`. `wait_entry_ready` answers a sentinel "ready" and asks a pinned object for its
own state, so a failed lookup can never read as satisfied. The entry array is zeroed before
resolution and `wait_entry_release` runs on **every** exit — success, timeout, alertable completion
and mid-resolution failure — dropping exactly the pins taken. Pins are still acquired once for the
whole wait, so the lookup/close TOCTOU stays closed; the registry, the pseudo single-wait semantics,
the handle ABI and the wait-all consumption model are unchanged, and native Windows is untouched.

**Gate (different author, `PosixMultiwaitGate`).** Both persistent tests extended:
`test_handle_lifetime.c` `sha256 434d1730…` (251 checks) and `test_handle_lifetime_asan.c`
`sha256 bae7d4dd…` (44 checks). Repaired shim: **plain 251/0 and ASAN 44/0** (leaks on, no
suppression); pre-repair source `83e7be99…`: **230/21 FAIL** plain and 40/4 ASAN, first deciding
lines `multi(any, {(HANDLE)-2}) is WAIT_OBJECT_0 FAIL` and the `err=0` index row. New rows cover
every sentinel in any/all × `Ex`/non-`Ex`, alone and mixed with a real event, asserting the return
value *and* that the last error is not `ERROR_INVALID_HANDLE`; anti-"everything ready" controls where
`{unsignalled event, sentinel}` in wait-all must still `WAIT_TIMEOUT`; the own `GetCurrentThread()`
value after its alias closed in any/all/`Ex` with the same value from another thread failing
`WAIT_FAILED`+6; forged, zeroed, `(HANDLE)-3` and closed values in head and tail position of
any/all × `Ex`/non-`Ex` returning `WAIT_FAILED`+`ERROR_INVALID_HANDLE` with live entries still usable
afterwards; `count==0`/`handles==NULL`/`count==65` → `ERROR_INVALID_PARAMETER` with `count==64`
accepted; and under ASAN 100 rounds of each exit path plus a 100-round `WAIT_IO_COMPLETION` exit
driven by a real APC. Private controls, the two deciding ones independently re-run by the
orchestrator: **sentinel fall-through** → 21 FAIL (`multi(any, {NULL}) is WAIT_OBJECT_0 FAIL`);
**`obj_pin` for self** → 3 FAIL (`multi(any, {own value, unsignalled event}) times out after the
close (-1) FAIL`); **skip release on failure** → invisible to the plain binary but
`LeakSanitizer: 118400 byte(s) leaked in 200 allocation(s)`, site `obj_alloc` ← `CreateEventA`.
The orchestrator re-derived the counts from the **committed** blobs rather than trusting the gate
report: the CRLF-normalised `git show HEAD:src/platform/win32_compat.c` is `7d25e6b7…`, and the
committed test sources give plain **251/0** and ASAN **44/0** exit 0. Leak detection was proven live
in that same build shape by running the skip-release mutant, which reports and exits 1 where the
repaired shim reports nothing; two synthetic leak controls did not trip LSan (one stayed reachable
from a live stack slot, one was optimised away), which is why the mutant is the control of record.
Preserved: the original unsafe shim `110e169` is still RED on both required negatives (SIGSEGV on the
invalid address; 19 false-accept rows then `double free or corruption`), the committed test sources
still give 175/0 and 22/0 against the repaired shim, and the three pre-existing POSIX programs pass
(15/0, 28/0, 8/0). Codex's own two probe programs, re-run unmodified against the repair: 12/12 with
`FAILURES=0`, and closed-self `0x102`/`0x102`/`0x102`.

**Evidence corrections (R2).** The macOS guide claimed **both** standalone projects carry the handle
gates; only `tests/win32_compat_posix` does, and `tests/win32_thread_posix` is unchanged. The guide
and the game `CMakeLists.txt` generalised a measured dependency-limited WSL link failure into a macOS
claim: the text now separates what was measured (that WSL image, 35 undefined references against the
SDL2/XAudio2 POSIX providers) from what is untested (a Mac, where nothing was built or run and a host
carrying the documented SDL2 prerequisites is not claimed to fail), keeps the exclusion as explicit
**unvalidated POSIX integration**, and states that the fixture's own header-validation case has still
not been executed on any POSIX host. Two targeted `ctest` receipts from the previous session used
`|` inside `-R` and are **excluded**; every targeted run in this packet is one literal pattern with
its own log and exit code, and the previous report's `665.83 s` "final" figure is corrected there as
superseded by its own session's `558.87 s`.

**Validation (this host).** Guarded build exit 0 (0 compile/link errors, 0 C4310, 18 known MSB8028);
Windows executable byte-identical to the archived smoke's `c7c6e292…` because the repaired file is
POSIX-only. Full limited CTest **exit 0, 162/162, zero failed/skipped, 523.49 s**, full log retained
at `logs/posix-repair-ctest-full.log`. Six targeted runs, each a single `-R` pattern, all exit 0.
`just check` exit 0 including the real retail corpus check (126/126). The earlier 8 s strict Vulkan
smoke carries because the executable and every Windows-relevant input are provably unchanged; no new
gameplay was run. Linux observations only; no Mac result. D4 remains incomplete; step 4 waits.

PUSHED_TO: none (candidate held for independent Codex review) / BRANCH: master / COMMIT:
toolkit b06bd36546e7bec75f17ca9677d7b55a8d59e01a; game a97db34 / RESULT: local commits only; no push, no force, toolkit first then game.


### §31.73 POSIX handle checkpoint independently accepted (2026-10-22)

Codex independently inspected toolkit `b06bd36546e7bec75f17ca9677d7b55a8d59e01a` and the
retained registry/ASAN-target commits. Wait entries explicitly distinguish supported sentinels
from failed real-handle resolution, route ordinary values through the self-aware pin, and drop
acquired pins on success, timeout, alertable completion and partial-resolution failure.

Compiled directly against the actual final shim on Ubuntu WSL, Codex's original 12 pseudo/null
multi-wait rows now pass; its synchronized closed-self probe returns `WAIT_TIMEOUT` in single,
multi and Ex forms. The persistent plain gate passes **251/251**, the instrumented ASAN gate
**44/44**, and pre-existing compat/interlocked/thread tests pass **15/28/8** checks. Against
the pre-repair source the expanded plain gate fails 21 rows. Private final-source controls that
reject sentinels or replace self-aware resolution with ordinary lookup fail 21 and 3 rows;
removing partial-failure unpins leaves all 44 ASAN return-value rows green but LeakSanitizer
reports **118400 bytes in 200 allocations** and exits 1. Shared source is unchanged after these
scratch-only controls. Final compiler-input shim SHA-256
`7d25e6b7d408ea8b262262cf56aace2a0b557fe515d3ad99eda7aa542b62cf3b`.
The separate configured gate author's route and absence of fallback were verified.

The final retained Windows receipt is **162/162, zero failures/skips, 524.32 s**. Guarded build,
individual targeted event/DPC/callback/save/fence receipts and final `just check` pass; the new
packet uses no alternation in CTest `-R`. Codex independently reclassified the archived 8-second
smoke as STRICT with no unknown/missing profile; current and archived executable hashes both
match `c7c6e292e03394b9cbae35930beacaa9d8b9a0805a7b45f2bc1f5c429de45ccb`. Only POSIX runtime
code changed, so the earlier bounded Windows liveness observation carries; no new gameplay or
fidelity claim follows.

Acceptance covers the measured POSIX provenance/lifetime and multi-wait contracts. It does not
prove arbitrary race freedom or Mac behavior. Pointer-value reuse remains possible, borrowed
file accessors require caller synchronization across close, and the full in-place event bridge
fixture remains unvalidated on POSIX. The Mac guide now distinguishes the observed WSL dependency
blocker from untested Mac integration. Historical MoltenVK suballocation accounting and its skip
remain debt, and CTest resource locks still coordinate only one CTest process.

The portability preflight is complete within these bounds. The owner-authorized next operation
is D4 step 4, fixture-driven CPU/AOT-Vulkan BUMPENVMAP and luminance, followed by step 5's
separate-author gate. D4 is incomplete. Carry exactly four normal operations and zero repair
cycles from the paused lineage; the preflight's nine operations/five repair allowances are
separate and fully spent. No model/provider/role/effort/fallback changes are authorized here.

PUSHED_TO: origin / BRANCH: main / COMMIT: b06bd36546e7bec75f17ca9677d7b55a8d59e01a / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward 110e169..b06bd36; three independently accepted source/test commits, eight outgoing blobs (largest 87951 bytes), zero secrets, only reviewed POSIX shim/gate/CMake paths, no forbidden or oversized objects, clean and synchronized. Upstream/nworkers remain push DISABLED.

PUSHED_TO: origin / BRANCH: master / COMMIT: 6d506f1e3f5e9ea074d8270019e9c7523334092c / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 2ff34cc..6d506f1 after toolkit; eight retained source/record commits, nineteen outgoing blobs (largest 570230 bytes), zero secrets, only reviewed CMake/macOS-doc/TR/plan/ledger paths, no forbidden or oversized objects. Final code has full limited CTest 162/162 and just check green; independent native/ASAN controls pass. Record-only acceptance hooks passed; clean and synchronized.


### §31.74 D4 step 4 blocked before implementation by configured provider quota (2026-10-22)

The owner-resumed D4 step-4 operation stopped during source inspection when the configured
WorkBuddy primary returned HTTP 429 usage exhaustion. Codex independently inspected its actual
transcript and both checkouts: no implementation edit, production worker, new build/test/run or
candidate exists. Both trees remained clean and synchronized at toolkit `b06bd36` / game
`d270a73`, the accepted portability checkpoint. Lifecycle completion is not implementation success.

The configured role is unavailable; under the orchestrator skill's human-owned model policy,
restore access or obtain exact owner settings authorization before resuming. No provider/model/
role/effort/fallback substitution, automatic retry or allowance reset was admitted. The attempted
operation consumed one carried D4 slot: cumulative seven D4 operations/three historical repair
allowances, with exactly **three normal operations and zero repair cycles remaining**. The earlier
paused lineage and its sole decisions are preserved; the nine preflight operations/five repair
allowances remain separate and fully spent. D4 is incomplete. Step 4's implementation and step 5's
independent formula/pixel gate remain next; nothing was published as a bump-mapping fix.

PUSHED_TO: origin / BRANCH: master / COMMIT: 76431bf6a276e78cf05f33c3937c2a6d3de42f47 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward d270a73..76431bf; clean documentation-only blocker checkpoint, two reviewed plan/TR blobs (largest 572554 bytes), zero secrets, no forbidden or oversized paths; record-only hooks passed. Toolkit unchanged and synchronized at b06bd36546e7bec75f17ca9677d7b55a8d59e01a; no new runtime validation or implementation claimed.

### §31.75 D4 step 4: BUMPENVMAP and BUMPENVMAP_LUM implemented in the CPU combiner and the AOT Vulkan uber-shader (2026-10-22)

The owner resumed step 4 after selecting `workbuddy-ai/deepseek-v4.1-flash-sg` for every global
role. One configured production worker, `BumpEnvImplementer` (agent class `task`, session
`01a1653d-b7ec-73a7-8523-708a8da7ee7d`, model read from its own session record), wrote the CPU and
Vulkan bodies; the primary coordinated, verified and wrote the records plus one comment-only
clarification. No model/provider/role/effort/fallback setting was changed or substituted.

**Facts, from primary sources, xemu read as FACTS ONLY (GPL; no code copied).** `psh.c` supplies
`sign3` (`x *= 255; x >= 128 ? (x-256)/127 : x/127`), the delta source (the SOURCE texel's blue in
x, green in y, and its unsigned red as the luminance term), the column-major `mat2 * vec2` product,
the post-source perturbation of the stage's 2D coordinate, the all-four-channel
`bumpScale*lum + bumpOffset` multiply, the `input_tex` source rule (stage 1 → 0; stages 2/3 from
`NV_PGRAPH_SHADERCTL`), and the `assert(i >= 1)` that makes stage 0 invalid. `pgraph.c` supplies the
register facts: the stage-0 words are DISCARDED by the handler (so stage 0 keeps the D3D default
0.0), the matrix arrives in the method order m00, m01, m11, m10, and scale/offset are
`0x1B38`/`0x1B3C + 0x40*stage`. The previous diagnostic zeros were UNKNOWN because the methods were
never admitted, not because the state was zero: that is now decided by transport.

**Implementation.** Toolkit `nv2a_combiner.h` gains `bump_mat[4][4]`, `bump_scale[4]`,
`bump_offset[4]` and `other_stage_input`. `nv2a_pb_exec.c` admits `SET_TEXTURE_SET_BUMP_ENV_MAT`
(`0x1B28+4i`), `_SCALE` (`0x1B38`), `_OFFSET` (`0x1B3C`) and `SET_SHADER_OTHER_STAGE_INPUT`
(`0x1E78`), de-swizzles the matrix into the canonical `[m00, m01, m10, m11]` row, discards stage 0,
and implements modes 6/7 inside `rc_stage_fetch`, which now takes every stage's result table. Every
stage's row is initialised to the NONE value `(0,0,0,1)` and filled in ascending order, so a later
bump stage reads a real earlier texel and an absent stage reads NONE rather than stale bytes.
`rc_bump_source` also bounds-checks: a nibble naming a stage that is not strictly earlier means "no
source", so no index can leave the array. `nv2a_vk_backend.c` grows `VbRcConstants` 528 → 640 with
`bumpmat`/`bumpscl`/`bumpofs`/`misc` at 528/592/608/624, asserts `sizeof` and every `offsetof`,
transports all four groups per draw, and widens the stage-0 texture bind from `m > 3` to `m > 7`
because a bump stage samples a texture of its own. `nv2a_vk_rc.hlsl` mirrors the same four groups
in `RcConsts`, adds `sign3`/`bump_src`, implements modes 6/7 in `fetch`, and fills `tt[0..3]`
ascending in `ps_rc`. `nv2a_vk_rc_coverage.h` moves the single shared refusal rule from `m > 5` to
`m > 7`, keeping the `mode_gt5` field name and report label for ABI/report stability with a comment
saying so; the stage-3 cube rule and every other unsupported reason are unchanged. Nothing is
compiled or translated at run time: the uber-shader is still built ahead of time by
`xbr_add_spirv`, and no runtime shader-compilation path was added.

**Game side, feature-required only, by the same author as the implementation.** `tests/test_pbcov_vk.c`
moves its "unsupported" fixtures from mode 6/7 to mode 8 and sweeps modes 0..7 at every stage (rc
draws 24 → 32), preserving each assertion's meaning; `scripts/pbcov-mutants.py`'s
`vk-supported-mode5-rejected` anchor follows the source to `if (m > 7)` → `if (m > 4)` with the same
name, case and purpose; `tests/test_gpu_backend_parity_vk.c` needed two changes the primary made —
a comment correction (its oracle does not model 6/7, so its generator still emits 0..4) and, because
batch A used stage-0 mode 7 purely to force the stage-0 pipeline fallback, that trigger moved to
mode 8, which restores the "stage-0 and combiner pipelines coexist in one frame" property the
fixture documents rather than silently losing it. `BumpEnvImplementer` wrote the toolkit change AND
this test/mutant rebase, so all of it is **same-author test maintenance**: none of it is an
independent gate, and the step-5 formula/pixel gate by a different author still does not exist. No
assets, saves, retail/generated code or corpus changed and no regeneration ran.

The executor's `stage_ge6` / `stage_mode_hist` counters were deliberately NOT changed: they still
count `m >= 6`, which is the accepted measurement history, and `tests/test_pbcov_stream.c`'s
`case_entry_forms` still observes stage-0 mode 6 as a bump batch. Only their comments changed, to
say the bucket is diagnostic over the title's state — modes 6/7 are now interpreted and 8..31 are
still refused — so a later reader cannot mistake it for a refusal count.

**Development evidence, explicitly not the gate.** Two throwaway probes in gitignored
`build/probe-bump/`: `probe.c` links the real `nv2a_pb_exec.c` bodies and `probe_vk.c` drives the
real back end on the RTX 4070; both report `PROBE PASSED (0 failures)` on the same fixture. The
fixture is decisive about the du/dv assignment (source texel `(L, G=64, B=32)` with a 4× matrix
samples texel `(5,6)`, where an earlier swapped oracle also landed on texel 4 and could not
discriminate), about the canonical row versus its transpose (`m01` moves V only, `m10` moves U only),
about the all-channel luminance multiply, and about stage 0's discarded registers. The mutation
harness, run against a private copy because it refuses a dirty shared checkout, decides
**22/22 mutants by their named assertions** with the restored baseline green and the shared toolkit
unchanged. Both probes were written by the same author as the implementation, so their agreement is
a cross-check, not an independent oracle.

**Windows receipts.** `just build` succeeded (57 targets / 162 tests derived, identity recorded,
Vulkan capability present); full limited CTest **162/162 in 518.59 s**, exit 0, full log retained
(the 517.47 s in the first version of this record was a transcription error: the retained log at
`logs/d4step4-ctest-full.log` ends with its own `Total Test time (real) = 518.59 sec`);
`just check` green including the real corpus re-derivation (126/126). Bounded 8 s strict Vulkan
smoke `20261022-142342-065-d4step4-bump-smoke2` (`RECOMP_GPU_ACK=0`, pipeline cache off, runner-owned
isolated save root): `diagnostic_deadline` at the requested deadline, checkpoints passed, save root
verified, GPU report ok, one snapshot, 333 named frames, RTX 4070, no fatal/ICALL/backend stop, no
validation layer inserted and therefore no validation claim. **Limit:** `RECOMP_PB_EXEC` is unset in
that profile and the back end says so, so a second 8 s strict smoke with `RECOMP_PB_EXEC=1`
(`20261022-144033-946-d4step4-bump-smoke3-pbexec`) was run: executor registered, same deadline, same
checkpoints and save-root verification, no fatal/ICALL, and no bump or rc counters in its log. A
first attempt in this operation (`20261022-142308-537-d4step4-bump-smoke`) is DISCARDED: it set
`RECOMP_SAVE_DIR` outside the runner's own root, so the verifier reported `save_root_verified: false`
and it is not a strict run and has no horizon row — a self-inflicted invocation error, not a bump or
runner defect. These smokes show the new paths do not break launch, admission or the back end; they
show NOTHING about the title's own bump state. A third 8 s strict smoke,
`20261022-150316-836-d4step4-bump-smoke4-final` (exe `3a5a59fd`), was run after the record commits with
both `patch_sha256` values empty, so its receipt matches the durable tree: same deadline, same
checkpoints and save-root verification, no fatal or ICALL, and again no bump or rc counters.

**Reference rule and the correction it forced (2026-10-22 repair).** `psh.c` initializes `pT0..3`
directly from `vtxT0..3` with no prior texture-q divide, gives PROJECT2D its own projected path
(`textureProj`), and has BUMPENVMAP/BUMPENVMAP_LUM sample `pT.xy + dsdt` with no q division. That is
a deliberate per-mode rule, NOT internally inconsistent reference paths. The first version of this
record instead described it as an inconsistency and implemented modes 6/7 by dividing the stage
coordinate by q before adding the delta, matching modes 1/2. An independent review measured that
against the actual CPU translation unit: with an 8x8 linear gradient, stage 1 mode 6/7, zero matrix
and coordinate `(6,4,0,q)`, q=1 selected the reference texel `(6,4)` (literal RGB 192/128/10) while
q=2 selected `(3,2)` (96/64/5). The repair adds the delta to the stage's own incoming xy in both the
CPU executor and the HLSL, leaves modes 1/2 (and 3/4/5) untouched, and a different-author focused
gate now decides it (TR §31.76). Remaining step-5 items are unchanged: signed (snorm) bump inputs,
cube/volume bump stages, rect-remap interaction, and the full committed-stream formula/pixel gate.

**Divergences step 5 must still decide.** (1) xemu uses the source texel's `bg` directly for signed (snorm)
bump inputs and `sign3` otherwise; this repository's `rc_texel` decodes UNORM only, so `sign3` is
uniform. (2) Cube textures on a bump stage use the face-0 projection precedent from modes 1/2/4;
xemu asserts 2D/3D there. (3) Rect-texture remap: xemu applies its `norm%d` remap to the perturbed
coordinate, while this back end applies the delta in the stage coordinate's own space and lets
`ts.xy` convert a linear format to normalized units afterwards. Those are NOT in conflict for the
two format classes this repository decodes — `rc_texel` samples a swizzled format at the coordinate
directly after scaling it by width/height, and samples a linear format in texel units, while
`ts.xy` is 1.0 for a swizzled texture and 1/width for a linear one, so the bump stage's
`(t.xy + dsdt) * ts.xy` and `rc_texel`'s own scaling agree in both cases — but a rect texture is the
one class where the two conventions could diverge, and no fixture here uses one. (4) Sampler
clamping/border behaviour is unchanged and unclaimed.

**Step 5, a different author, must** write an independent formula oracle without reading these
bodies, prove CPU/Vulkan pixel agreement on a bump fixture where the perturbed texel is visible in
the readback rather than only in a counter, and fail with a named assertion on each negative control
(zero matrix, transpose, du/dv swapped, luminance scale dropped, stage-0 discarded registers), plus
an explicit statement that modes 8..31 and stage-3 cube still take the fallback. No in-game bump
correction, D4 completion, D5 completion or hardware-fidelity
claim is made. This first admission consumed one carried normal slot: D4 stood at **8 operations /
3 historical repair allowances**, leaving 2 normal operations / 0 repair cycles at that point; the nine
preflight operations/five repair allowances stay separate and closed; the paused lineage and the
failed attempt's sole REQUEST_HUMAN decision are preserved. **Superseded by §31.76**, whose repair
admission consumed the remaining carried slot and the granted repair allowance, leaving 1 normal
operation / 0 repair cycles.

**Deliberately unpushed.** The candidate commits are local only and have NO `PUSHED_TO` line, by
instruction: toolkit `2a3c1e9` (modes 6/7) and `5e38bb4` (`stage_ge6` bucket comment), game
`0ccd335` (test/mutant rebase and records), `270279b` (stage-0 fallback fixture and record
corrections) and `08103d7` (durable-tree smoke receipt). They stay unpushed until step 5's
different-author deciding gate and Codex acceptance; their absence from origin is intended, not a
lost or failed push. The outgoing range was audited per repository rather than over the whole history:
game `origin/master..HEAD` is 22 blobs with a 593,731-byte largest blob and toolkit `origin/main..HEAD`
is 20 blobs with a 274,770-byte largest blob, both well under the 100 MB limit, and
`scripts/secret-audit.py` over exactly those blobs reports `TOTAL HITS: 0` for each. (A whole-history
scan for comparison reports two `openssh_key` hits, both literal pattern text inside
`scripts/secret-audit.py` and one old review document quoting it, in no outgoing commit.) No asset,
save, generated retail code or corpus blob is in either range. The `PUSHED_TO:` line above belongs to the earlier blocked-checkpoint commit.

### §31.76 D4 step 4 repair: the bump coordinate stops dividing by texture q, and the binding predicate becomes the sampled-mode set (2026-10-22)

Codex rejected the §31.75 candidate (`op_0mvjxma8fc27368285ca28b24` / run
`run_0mvjxl3bpc8b0e8dc8eb386e2`, sole STOP `soleSTOPdec_0mvk3pb0h4c42082ca8a9d576`) with two
demonstrated defects, recorded in `logs/reports/d4-step4-q-codex-review-2026-10-22.md` and repaired in
`logs/reports/d4-step4-q-binding-repair-2026-10-22.md`. This is a bounded correctness repair, not
acceptance; full step 5 stays a separate, differently-authored gate.

**R1, the q division.** The selected reference `xemu` `478b4f49…c4300` `psh.c` initialises `pT0..3`
directly from `vtxT0..3` with no prior divide, projects only in PROJECT2D, and has the bump modes
sample `pT.xy + dsdt`. That is a per-mode rule, and §31.75's reading of it as an internal inconsistency
was wrong. Measured against the actual CPU translation unit with an 8x8 linear gradient, stage 1 mode
6/7, zero matrix and coordinate `(6,4,0,q)`, q=1 selected the reference texel `(6,4)` (RGB 192/128/10)
while q=2 selected `(3,2)` (96/64/5). The repair adds the delta to the stage's own incoming xy in both
`rc_stage_fetch` and the HLSL `fetch`; modes 1/2 keep their divide and 3/4/5 are untouched.

**R2, the binding predicate.** `vb_rc_prepare` had widened the sampled-mode membership from 1..3 to
1..7, doing image and sampler work for PASSTHRU (4) and CLIPPLANE (5), which sample no image. Restored
to the exact set `{1,2,3,6,7}`.

**Focused different-author gate.** New game tests `tests/test_bumpenv_q_cpu.c`,
`tests/test_bumpenv_q_vk.c` and `scripts/bumpenv-q-controls.py`, authored by a worker distinct from
both the production author and the primary. Expectations are hand-derived from a gate-defined 8x8
linear gradient (R identifies x, G identifies y), with `sign3` deltas `du = 104/127`, `dv = 48/127` and
`lum = 64/255`: zero matrix → `(6,4)` = `0xFF8038B0`; matrix `[-0.25,0.125,0.125,0.5]` → `(5,4)` =
`0xFF7038A0`; matrix `[0.5,0.125,0.5,0.5]` → `(6,4)`; mode 7 → `0xC0602A84`. The CPU half is a labelled
DIRECT HELPER SEAM (real `rc_stage_fetch`, no rasteriser, no committed stream); the Vulkan half drives
the real `vb_draw` + `vb_flip` on lavapipe and reads the texel out of guest memory, with one labelled
stand-in decoder that reads a linear 32-bit texel exactly as production `sample_tex` does. No shader is
compiled at run time.

GREEN 8/8, exit 0: q=1 and q=2 select the SAME hand-derived texel for the zero, displacement, positive
delta and luminance cases in both the CPU seam and the AOT Vulkan path; PROJECT2D still lands on
`0xFF502870` at q=2, so globally deleting the divide cannot pass; the production binding witness shows
modes 4/5 upload 0 and leave no cache entry while modes 1/2/3/6/7 upload and bind, witnessed from
`g.st.tex_uploads`, `g.st.tex_hits` and `g.texs[]` after a warm-up that makes stage 0 a cache hit.
Private controls (`scripts/bumpenv-q-controls.py`, a hand-run harness like `scripts/pbcov-mutants.py`,
deliberately not CTest-registered, run against a private copy of the toolkit outside both repositories),
two consecutive `--control all` runs both `RESULT: PASS`: restoring the bump q divide goes RED on named
assertions in the CPU bump cases and the Vulkan bump rows while `project2d_control` stays green;
restoring the `m > 7` range guard goes RED on the two named mode 4/5 upload assertions plus the two
cache-entry assertions. Shared source hashes are identical before and after both controls (`b87f0ad0…`,
`d13cbb54…`, `313509a3…`). Primary review found and had the gate author fix four defects in its own
files: a header that derived `du` with the wrong sign (no assertion depended on it); a displacement
check written `chk(got_q1 != got_q2 ? 0 : 1, …)` that failed exactly when the two q agreed; a harness
scratch directory under `%TEMP%`, where MSBuild's own `MSB8029` warning says incremental builds are
unreliable, which let a restore half run a stale mutated object and report a false `expected GREEN`
failure; and — the durable cause of that staleness — the `#include`d production files were not declared
dependencies of the test objects, so an edit to the file under test need not recompile the object that
includes it. `OBJECT_DEPENDS` now states both dependencies (verified to force a recompile by a
whitespace-only edit to a private copy), and the harness forces a clean rebuild before every build and
refuses to emit a verdict when consecutive builds are byte-identical. The earlier controls' RED halves
are sound — each first build already contained the mutation — but their GREEN halves rested on an
`os.utime` mtime bump and are superseded by the two runs above.

**Windows receipts.** Guarded build exit 0, 59 targets / 170 tests derived, identity recorded. Focused
`ctest -R "jsrf_bumpenv_q"` 8/8 in 1.46 s. Full limited CTest **170/170 in 550.19 s**, exit 0
(`logs/d4step4q-ctest-full3.log`, on the final tree after the `OBJECT_DEPENDS` change; an earlier run on
the pre-change tree was 170/170 in 521.25 s); a first run was 168/170 because G1 stages tracked files only and the
two new tests were untracked — reproduced in a tracked-only scratch tree, fixed by staging, G1 then
PASS including its mutants and negative control — and because `xbox_guest_meter` flaked once on its own
lock timing (a toolkit test with no dependency on these files; 3/3 alone, 12/12 under
`--repeat until-fail:12`, and green in the clean run). `just check` exit 0 including the real corpus
re-derivation (126/126). One 8 s strict isolated Vulkan smoke
`20261022-173449-960-d4step4q-repair-smoke` (`RECOMP_GPU_ACK=0`, `RECOMP_PB_EXEC=1`, pipeline cache off,
runner-owned save root): `diagnostic_deadline`, checkpoints passed, save root verified,
`gpu_report_ok` true, executor registered, no fatal or ICALL, 323 named frames, Intel Iris Xe this run.
It reaches no bump batch and logs no bump or rc counters, so it proves launch, admission and executor
liveness only — never executed bump sampling. No validation layer was inserted and no validation claim
is made.

**Remaining limits, unchanged.** Signed (snorm) bump inputs (UNORM-only `rc_texel`, so `sign3` is
uniform), cube/volume bump stages (face-0 precedent, unmeasured), rect-texture remap (no fixture), and
modes 8..31 / stage-3 cube still falling back. The full committed-stream formula and pixel oracle, the
complete bump matrix/source/luminance/format coverage and the old-red/final-green sequence are step 5's,
written by a further different author. No in-game bump correction, D4 or D5 completion, or
hardware-fidelity claim is made.

**Accounting.** This first repair admission consumed one carried normal slot AND the single granted
repair allowance, even though the bridge counted it as a first turn. The grant is the owner's own
repair-packet instruction for this operation ("SkillconfirmedNEWreview renews ONLY1exhausted
repairallowance; carries EXACT2existingnormaloperation slots … THIS FIRSTrepairadmission consumes
1existingnormalslot AND1grantedrepairallowance despite bridgefirstturnrem0"), not a renewal asserted
here. Cumulative D4 after it: **9
operations / 4 historical repair allowances**, leaving **1 normal operation / 0 repair cycles** for the
separate full step 5. No refund, reset or extra slot was taken, and no additional operation slot exists.
The nine preflight operations and five repair allowances stay separate and closed. The paused lineage
(`run_0mvimyj9w60c7e52804ec4fd0` / `op_0mvioc9a16ceedfaee328d140` /
`soleSTOPdec_0mvizmpv0aac1f9f84eb5bcc0`), the failed read-only attempt's
`sole REQUEST_HUMAN dec_0mvjx6nil57f7620b42083b1c`, and the rejected candidate's
`soleSTOPdec_0mvk3pb0h4c42082ca8a9d576` are all preserved.

**Deliberately unpushed.** This repair's candidate commits are local only, with no `PUSHED_TO` line, by
instruction: toolkit `d9153fd` (the bump coordinate and binding repair), game `eacd525` (the
different-author focused gate, its `OBJECT_DEPENDS` registration and the private-control harness) and
the records commit that carries this section. They stay unpushed until step 5's different-author
deciding gate and independent Codex acceptance; their absence from origin is intended, not a lost or
failed push.

### 31.77 — Codex confirms the production repair; rejects two new fixture/tool defects

Codex independently reviewed operation `op_0mvk3sc0ufb39663d99cd2f3a` under
`run_0mvk3qrksee43402c8979f9fe`, fetched both origins and verified clean local
game `0a1ba973bda33e22a7bbff7085421df057e6d048` (ahead7) and toolkit
`d9153fd55b89954ae9ce3fc567e377388c4266d6` (ahead3), with owner-fork origins
and toolkit upstream/nworkers pushes disabled. Nothing was pushed.

The q/binding production repair is sound within its measured scope. Codex rebuilt
its own actual-production CPU probe: all four mode6/7 q1/q2 literal-RGB rows now
pass and all 18 legal stage1..3 bump methods remain admitted. Codex reran all
eight focused Windows cases, 8/8 in1.46s; the actual Vulkan driver is pinned
llvmpipe LLVM23.1.2. Bump q1/q2, displacement, luminance, PROJECT2D projection and
the exact sampled binding membership are witnessed by actual pixels/state.
The CPU half still uses a direct-helper seam; this is not the full committed-
stream pixel gate. The retained 170/170,550.19s suite/checker receipts were
inspected, not independently repeated in full. The strict smoke reaches no bump
batch and carries no bump/fidelity claim. Actual configured parent and both
workers use workbuddy-ai/deepseek-v4.1-flash-sg/high without observed substitution.

**R3, reproduced portability regression.** The new CPU fixture is inside
BUILD_TESTING/XBR_VULKAN_FOUND without a WIN32 guard, but its source explicitly
emits `#error` off Windows. WSL GCC preprocessing with the actual fixtures-only
template include and forced compatibility header exits1 at that directive.
The supported Vulkan-enabled macOS fixtures-only build therefore encounters the
same explicit refusal; no Mac run is claimed. A Windows-only G1 configure does
not test this host condition. Prefer an actual native build/run of the helper
with the artificial refusal removed; otherwise document and guard a demonstrated
remaining host dependency rather than declaring a green portable gate.

**R4, reproduced scratch-data deletion.** The new manual mutation helper accepts
an existing explicit/default root, then removes its existing toolkit-<control>
child without establishing ownership. Codex's actual-main-path probe created a
harmless pre-existing child/sentinel and stopped before toolkit copying/building;
the helper had already deleted both. No user data was probed or removed. Repair
must validate paths before mutation, use fresh owned roots or verified ownership,
preserve existing roots/children on refusal and refuse cleanup after ownership or
resolved identity changes. The existing pbcov helper's ownership contract is the
local reference. These findings concern new gate tooling, not the production
q/binding formulas.

Sole STOP decision `dec_0mvkahpy93eec0dd14c7fb9a8`; own detailed assessment,
concrete advice, required safety/native/Windows/control checks and stopping
condition: `logs/reports/d4-step4-fixture-safety-codex-review-2026-10-22.md`.
Deciding probes: `logs/reports/codex-bump-repair-review.py`/`.log`, artifacts
`codex-bump-repair-review-0scdnjam`. Shared executor/HLSL/backend bytes remained
unchanged. No production redesign, texgen, full step5 or push is authorized by
this bounded repair.

**Accounting correction and next admission.** The earlier review-confirmed
allowance was granted by Codex under the skill's reset rule, not by a new human
repair instruction as §31.76's wording says. This NEW independently confirmed
review permits renewal of only one exhausted repair allowance, carrying the one
remaining normal slot and adding no operation slot. The linked successor's first
repair consumes both even if the bridge counts a first turn: cumulative D4 after
admission10operations/5historical repair allowances, zero normal slots/zero repair
cycles. Full step5 remains a separate pending gate with no automatic normal-slot
renewal. Preserve all earlier runs and their sole decisions; preflight accounting
stays separate. Candidate commits remain local and unpushed.

### 31.78 — The CPU bump fixture is genuinely portable, and the control harness owns only what it creates

Bounded test-tool repair under run `run_0mvkakqvk5b481bf948a47013`, after Codex
rejected the §31.77 candidate for two new defects. The R1/R2 production repair is
untouched and still stands on Codex's own rebuilt probe and the eight focused
Windows cases. One configured gate-tool worker (`BumpFocusedGate`) owned the
fixture/helper changes, their two regressions and the minimal registration; the
primary owned the independent reproductions, integration, every Windows
validation run and this record. No production bump, format, refusal or texgen
change; no model, provider, effort, role or fallback change.

**R3 — the artificial refusal is gone, and the fixture really builds off
Windows.** Current source, real WSL GCC 15.2.0, the configure's fixtures-only
include set plus the forced `tests/compat/jsrf_test_compat.h`: exit1 with
`#error "this fixture follows the repository's Windows-only test harness"` at
`tests/test_bumpenv_q_cpu.c:111`. Because the target is registered inside
`BUILD_TESTING` and `XBR_VULKAN_FOUND` with no `WIN32` guard, a supported
Vulkan-enabled macOS fixtures-only build fails deterministically. The refusal is
deleted; the only real host dependency was three symbols
(`QueryPerformanceCounter`, `QueryPerformanceFrequency`,
`InterlockedCompareExchange64`) that the toolkit's `platform` library already
provides off Windows — the same library `jsrf_service_chain_test` links. The
CMake change is one `if(NOT WIN32)` link line for that library; the target's
position, its five cases and the whole Vulkan half are unchanged, and Windows
still links nothing extra. All five CPU cases now compile, link and pass from a
real Linux binary against the same production `rc_stage_fetch`, so the portable
build decides the same rule the Windows build does. No Mac run is claimed: the
macOS path is the same shipped branch, argued from the condition, not measured.

The new `tests/test_bumpenv_fixture_portability.py` decides this rather than
asserting it. It parses the shipped target's source list and its five `add_test`
names (a missing target or case is a hard FAIL), resolves a real POSIX compiler,
compiles the fixture and every extracted source with the target's flags and
include dirs, links, and requires a `PASS bumpenv_q_cpu[<case>]` line from a real
binary per case. A compile or link failure fails the test; only a host with no
POSIX compiler skips, and CTest is given `SKIP_RETURN_CODE 77` so a skip is never
a pass. Stale prose was corrected at the same time: B crosses off `(6,4)` to
`(5,4)` and C stays on `(6,4)` — the assertions already said so.

**R4 — the helper no longer deletes a scratch child it did not create.**
Reproduced through the real `main`/`run_control` path with only the copy boundary
stubbed: a pre-existing `--workdir` holding
`toolkit-bind-guard/preexisting-unrelated-sentinel.txt` lost both, and the copy
boundary was still reached. The cause was `mkdir(exist_ok=True)` on an existing
root, containment validated only after creation, and an unconditional `rmtree`
of an existing `toolkit-<control>` child; the fixed default sibling had the same
exposure. `scripts/bumpenv-q-controls.py` now follows the accepted
`pbcov-mutants.py` ownership contract without importing or reopening it: an
existing `--workdir` (directory, file or symlink) is refused before anything is
modified; the default is a fresh unique sibling created with `exist_ok=False`
outside both repos and outside `%TEMP%`; resolved containment, ancestor, `%TEMP%`
and root/anchor constraints are validated before any `mkdir`; an ownership
marker plus `(st_dev, st_ino)` identity gates removal, which is skipped and
reported when ownership or resolved identity changed; and a pre-existing
`toolkit-<name>`/`build-<name>` inside the owned root is a harness error rather
than a path to clean. Every return path releases through `finally`, `--keep`
exists, and all mutation specs, marker strings, case names, exit codes and the
shared-hash printing are unchanged. Both controls were then rerun from their own
fresh owned roots and both were `RESULT: PASS` with named RED assertions, distinct
baseline/mutated/restored executable hashes, forced object/SPIR-V rebuilds and
byte-identical shared toolkit hashes before and after.

The new `tests/test_bumpenv_controls_workdir.py` has eleven cases and detects the
defect it exists for: against `HEAD`'s helper it is 6 failures / 4 errors with the
named one being `"reached the copy boundary after deleting the caller's child" !=
2`, and against the repaired helper it is 11/11 OK. It covers refusal of an
existing root, its child and sentinel, a file, a disposable repo root, the real
game root and an ancestor, a path inside the repo and a path under `%TEMP%` with
nothing created, cleanup blocked by a changed or missing marker, a mocked-runner
success that removes only the owned root, `--keep`, and the default root's
placement. Its success case is labelled a mocked runner: no compile and no
control verdict is claimed from it.

**Receipts on the settled tree.** Guarded build exit0, 59 targets / 172 tests
derived, corpus 128/126 unchanged. Focused Windows 8/8 in 1.63 s on pinned
llvmpipe LLVM 23.1.2 type4, with the unchanged CPU/Vulkan rows and binding
witness (modes 4/5 uploads 0 cached 0; 1/2/3/6/7 upload and cache once). Both new
tests pass under CTest. Full limited CTest **172/172 in 570.38 s**, exit0.
`just check` green including `entries=126 recompiled_and_matched=126
ascending=yes duplicates=0 xbe_distinct=126 problems=0`. One earlier attempt used
a `-R` regex containing `|`, which `cmd.exe` consumed so no ctest ran; it was
discarded rather than read as a result, and the two tests were rerun as separate
single-name regexes. The strict smoke was **carried, not rerun**, on verified
identical runtime identity (`jsrf_recomp.exe ca550e78…`, `jsrf_collect.exe
cb90b7e3…`, `game/default.xbe fd190557…`, all matching run
`20261022-173449-960-d4step4q-repair-smoke`): it proves bounded launch and
executor-registration liveness only and reaches no bump batch, so no fidelity
claim follows.

**Limits.** No Mac run. The portability test's POSIX branch was exercised only
structurally; WSL was the branch that ran. The workdir success case is mocked.
Full step 5 remains pending and unclaimed, with signed bump inputs, cube/volume
textures on a bump stage and rect-texture remap still open, and modes 8..31 and
stage-3 cube still on the fallback. No D4 or D5 completion, no in-game bump
correction, no hardware fidelity.

**Accounting.** This admission consumed both the one carried normal slot and the
one renewed repair allowance: cumulative D4 **10 operations / 5 historical repair
allowances**, **zero normal slots, zero repair cycles**. The renewal came from
Codex's independently confirmed review under the skill's reset rule, not from a
new human repair instruction; §31.77's reading of the prior grant is corrected
here, as is the notion that its "not step 5" opening excluded a differently
authored focused gate. No refund, reset or extension; the preflight 9/5 stays
separate; every earlier run and sole decision is preserved; commits remain local
and unpushed. Report: `logs/reports/d4-step4-fixture-safety-repair-2026-10-22.md`.

### 31.79 — Codex confirms native fixture progress; forced rebuild bypasses current ownership

Codex reviewed `op_0mvkao1vnf743041d9de6007a` / `run_0mvkakqvk5b481bf948a47013`:
both origins fetched, clean game33c15cf (ahead9), toolkitd9153fd (ahead3), owner
forks correct and toolkit upstream/nworkers pushes disabled. Codex independently
reran all ten focused CTests,10/10 in10.66s, including actual WSL compilation and
five CPU cases. The original existing-root/child/sentinel probe now exits2 and
preserves the data. The full retained172/172570.38s/build/check logs were inspected,
not rerun in full. Actual runtime exe/collector/XBE hashes match the carried strict
smoke, whose scope remains bounded launch liveness with no bump batch or fidelity.

The candidate is still rejected for a new deciding cleanup path in the repaired
helper: final root release checks current marker/identity, but `force_rebuild`
checks only whether `build_dir` resolves lexically under the original root path.
Codex sequentially moved the owned root aside, created a replacement at that path
with a foreign marker and disposable target-dir sentinel, and called both paths.
`_still_owned()` was false and root release refused correctly; forced rebuild
nevertheless recursively removed the replacement target directory and sentinel.
A private helper copy adding an early current-ownership check refuses and preserves
the sentinel. Shared helper bytes stayed unchanged. Only Codex-created probe data
was involved; no real user directory or source was tested or removed. A separate
junction probe was refused by Python's removal routine, so no foreign deletion is
claimed for that case. The defect is sequential, not a race-freedom claim.

Sole STOP `dec_0mvkd0r9590a81f646c06f28d`. Own assessment, concrete bounded repair,
required final checks and stopping condition:
`logs/reports/d4-step4-rebuild-ownership-codex-review-2026-10-22.md`; own actual
helper probes and private prototype are linked there. Repair only the existing
helper's current-ownership/exact-target checks and existing regression file; keep
the verified native fixture and production rendering changes intact. Correct scope
claims: the native gate manually compiles parsed sources and does not execute a
non-Windows CMake graph; forced rebuild does recursively remove private target
dirs, so its ownership proof must precede those removals too. No full step5 or push.

Under the skill's NEW confirmed-review rule, renew only one exhausted operation
slot plus one repair allowance for this bounded cleanup repair. Its admission
consumes both: cumulative D4 **11operations/6historicalrepair allowances**, then
**zero normal slots/zero repair cycles**. Fullstep5/later texgen normal allowances
are not renewed. Preserve all earlier sole decisions and separate preflight9/5.

### 31.80 — Forced cleanup now requires current ownership and exact targets

Bounded test-tool repair under run `run_0mvkd2iugf018a7ecc0c8ff52`, closing the
deciding path of §31.79. No production change: the R1/R2 rendering repair, the
portable CPU fixture and the scratch-root ownership contract are untouched, and
the shared toolkit stayed byte-identical (`b87f0ad0…`, `313509a3…`, `d13cbb54…`
before and after). One configured gate-tool worker (`BumpFocusedGate`) owned the
helper's cleanup checks and the focused additions to the existing ownership
module; the primary owned the independent reproduction, every Windows validation
run and this record. No model, provider, effort, role or fallback change.

**The defect, reproduced by the primary before any edit.** `force_rebuild` ran
`owned.contains(build_dir)` — a lexical containment test against the original
root pathname — and then removed each focused `*.dir` recursively plus the
executables and generated SPIR-V. Move the owned root aside and put a different
directory at the same pathname holding a foreign marker and a focused build tree,
and containment still held while identity and marker no longer did. With the
pre-repair helper preserved as `logs/d4step4g/prerepair-bumpenv-q-controls.py`
(sha256 `7e9ae9ac…`), the primary's own probe through the real `acquire_work_area`
and `force_rebuild` reported
`{"refused": false, "still_owned": false, "release_removed_root": false,
"sentinel_survives": false, "dir_survives": false, "exe_survives": false}`: root
release refused the replacement while forced cleanup deleted its `.dir`, sentinel
and executable. Only disposable probe data was involved.

**Repair.** `force_rebuild` now, in order and before the first removal: refuses a
missing ownership proof (`owned=None` is a named `HarnessError`, no longer a
bypass); refuses when `_still_owned()` is false, naming the root and the changed
identity/path/marker; keeps the containment check; plans every exact deletion
target (each `*.dir`, each focused `.exe`, each `xbr_nv2a_vk_rc_*`); refuses a
recursive target that is itself a symlink or Windows junction instead of following
it; resolves every target and refuses one that lands outside the valid owned root;
and only then removes. A removal `OSError` becomes a `HarnessError` carrying the
path and message, so a failed cleanup keeps its diagnosis instead of a traceback.
A genuinely owned fresh root gets exactly the previous removals, the same
`forced recompile: removed …` line and the same return. The docstring now says the
cleanup removes private `*.dir` trees recursively and calls this a sequential
invariant, not race freedom. The module's ownership paragraph no longer claims
nothing inside the root is ever removed.

**The same invariant in the other removal path.** Reviewing the helper's exception
paths for the same rule found `acquire_work_area`'s stamp-failure handler doing an
unconditional `rmtree(root, ignore_errors=True)`, with no proof that `root` was
still the directory it had just created. `stamp()` now records the identity before
it writes the marker (so a failed marker write still leaves a usable proof), a new
`_same_directory()` checks link/resolved-path/identity without requiring the marker,
and the failure path removes the root only while that holds; otherwise it prints
`work area NOT removed (ownership changed)` and leaves it. `--keep` and the raised
`WorkAreaRefused` message are unchanged, so a fix that simply stopped removing
cannot silently leak scratch roots either.

**Regression, and that it decides the defect.** The existing
`tests/test_bumpenv_controls_workdir.py` gained six cases and five helpers; the
eleven earlier cases are unchanged. Against the pre-repair helper it is
`Ran 17 tests … FAILED (failures=4)`: the replaced-root case reports `None` rather
than a `HarnessError` with the replacement's sentinel, exe and `.dir` all absent
(`(False, False, False)`); the missing-proof case the same; the junction case
raises a bare `OSError('Cannot call rmtree on a symbolic link')` from Python's own
removal routine rather than the named refusal; and the stamp-failure case fails on
outcome too — the assertion text is "a replacement directory at the same pathname
must not be removed" — because the pristine unconditional `rmtree` deleted the
replacement. Two of the new cases are controls that pass on both helpers and are
labelled as such: the normal fresh-owned `force_rebuild` case, and the
stamp-failure-with-intact-identity case (a fix that stopped removing would leak
scratch roots). On the repaired helper it is `Ran 17 tests … OK` with no skips —
the junction case really creates a `mklink /J` here — and the primary's probe
reports `{"refused": true, "still_owned": false, "sentinel_survives": true,
"dir_survives": true, "exe_survives": true}` with the refusal naming
`ownership proof failed for the work area …`.

**Receipts on the settled tree.** Focused Windows 10/10 in 11.02 s on the pinned
software ICD with device `llvmpipe (LLVM 23.1.2, 256 bits) (type 4, api 1.4.354)`,
including the WSL compile/link/run of the five CPU cases; the `vk_bump_q` verbose
run shows `readbacks=1 rb_copies=1 validation_errors=0`. Both private controls ran
from their own fresh owned roots and each was `RESULT: PASS`, exit 0: `q-divide`
RED on `bump q=2 selects the SAME hand-derived texel (no PROJECT2D divide)` (three
CPU cases) and `mode7 q=2 R is the same texel and the same luminance` plus the two
Vulkan texel assertions, with `project2d_control` GREEN under the mutation; then
all six GREEN after restore. `bind-guard` RED on `mode 4 must not upload its stage
texture` and `mode 5 must not upload its stage texture`, then GREEN on
`vk_bind_witness`. Baseline/mutated/restored executable hashes differ in both
runs (`a41a992c…`/`edee2f45…`/`8c0f1357…` CPU and
`ddb6d796…`/`f28b558a…`/`77675552…` VK for `q-divide`; `66796a3a…`/`d97681f6…`/
`6feb62f6…` and `9fff4334…`/`e164179a…`/`ba2e8943…` for `bind-guard`), so neither
verdict can be a stale binary, and the forced rebuild printed the recursive
`*.dir/` removals before every build. Guarded build exit 0, 59 targets / **172**
tests derived, corpus 128/126 unchanged. Full limited CTest **172/172 in 529.14 s**,
exit 0, on the exact final tree; earlier runs of the same source were 172/172 in
530.38 s, 531.64 s and 564.95 s. `just check` green including `entries=126 recompiled_and_matched=126
ascending=yes duplicates=0 xbe_distinct=126 problems=0`.

**Scope claims corrected.** `tests/test_bumpenv_fixture_portability.py` now states
what it decides: it parses the shipped source list and registration, supplies the
`platform` translation unit itself and invokes a real POSIX compiler, so it
compiles the fixture's sources but does **not** configure or build a non-Windows
CMake target graph — that `if(NOT WIN32)` branch is source-inspected only. The
earlier prose claiming it exercised the CMake branch is corrected; no new CMake
harness was added.

**Limits.** The strict smoke was **carried, not rerun**, on verified identical
runtime identity (`jsrf_recomp.exe ca550e78…`, `jsrf_collect.exe cb90b7e3…`,
`game/default.xbe fd190557…`, all matching run
`20261022-173449-960-d4step4q-repair-smoke`): bounded launch and
executor-registration liveness only, reaching no bump batch, so no fidelity claim
follows. The cleanup invariant is sequential: validation and removal are separate
steps and a directory swapped between them is not detected; no race freedom is
claimed. Only the deletion target itself is validated; a junction nested deeper
inside a `*.dir` tree is not separately checked, and the measured reason that is
not a foreign-deletion path here is Python's own behaviour: `shutil.rmtree` removes
such a junction entry rather than descending into its target (probed directly — the
foreign sentinel survived and the tree was removed). That is a measured
observation about this runtime, not a guarantee about every future one. Full step 5
remains pending and unclaimed, with signed bump inputs, cube/volume textures on a
bump stage and rect-texture remap still open, and modes 8..31 and stage-3 cube
still on the fallback.

**Accounting.** This admission consumed the one renewed operation slot and the one
renewed repair allowance: cumulative D4 **11 operations / 6 historical repair
allowances**, **zero normal slots, zero repair cycles**. No refund, extension or
new run; the preflight 9/5 stays separate; every earlier run and sole decision is
preserved; the commits remain local and unpushed. Report:
`logs/reports/d4-step4-rebuild-ownership-repair-2026-10-22.md`.

### 31.81 — Codex independently accepts the bounded cleanup repair

Sole ACCEPT `dec_0mvkhiiu73f9b2c5a25ae3c9f` closes operation
`op_0mvkd50m8ba933c7b3a87dd1a`, run `run_0mvkd2iugf018a7ecc0c8ff52`.
Own complete review: `logs/reports/d4-step4-cleanup-codex-acceptance-2026-10-22.md`.
Both origins fetched, clean game31321ee (ahead12) and toolkitd9153fd (ahead3),
owner forks and disabled upstream/nworkers pushes verified. The initial game
fetch connection reset was resolved by one successful bounded retry. Bridge tools
briefly disappeared then returned before the sole decision; no alternative
journal mutation, duplicate review or operation was used. Actual parent/worker
routes are configured workbuddy-ai/deepseek-v4.1-flash-sg/high without substitution.

Codex reran its original actual-helper probe: an existing root is refused intact,
root release and forced rebuild both refuse a replacement directory, and an actual
Windows junction target is refused with link and foreign sentinel preserved.
Codex separately ran the final seventeen ownership cases on the committed old
helper and final helper: old exits1 with four semantic failures; final17/17 OK,
no skips. Intact-identity stamp cleanup and ordinary fresh-owned forced rebuild
positive controls still pass. The LF Git blob and CRLF Windows receipt were
verified text-identical after Codex corrected an initial raw-hash assertion.
Own probe data only; no real user directory or source was offered for removal.

Independent focused CTest10/10 in12.30s includes actual WSL compilation/all five
CPU cases, the seventeen ownership cases and real llvmpipe q/luminance/projection/
binding witnesses. Retained full limited CTest172/172529.14s, guarded build,
just-check126/126 corpus and both fresh-scratch private q-divide/bind-guard named
RED/restoredGREEN controls were inspected, not rerun in full by Codex. Current
runtime exe/collector/XBE hashes independently match the carried strict smoke;
no new bump batch, gameplay, fidelity or horizon claim follows.

The acceptance is limited to the bounded repair. Current ownership and exact
targets are validated before forced cleanup; failed stamping uses recorded root
identity. Arbitrary concurrent path swaps remain outside the sequential contract.
Nested junction behavior relies on this runtime's observed removal semantics.
Native source compilation is measured on WSL, not a Mac or a non-Windows CMake
graph. Fullstep5's independent committed-stream/formula/CPU+Vulkan pixel gate,
signed bump inputs, cube/volume and rect remap remain pending. No feature
publication, D4 or D5 completion. All outgoing candidate commits remain local.

Cumulative D4 is11operations/6historicalrepair allowances, zero normal slots/zero
repair cycles; preflight9/5 remains separate. Successful acceptance renews no
allowance. The next fullstep5 gate needs one newly authorized normal operation;
do not silently start a new run or change human-owned model/provider/role settings.
