# JSRF technical record

What the project has established, by subsystem, with the evidence and limits each result rests on.
It consolidates the review records of closed packets (their full text is in git history; the last
tree holding them is `d6a1bc0`). The plan (`plan-jsrf-bare-minimum.md`) owns current status and
next action; this file owns the durable technical facts behind them. Toolkit-side design and licence
records live in the toolkit itself: `xboxrecomp/src/apu/dsp/PROVENANCE.md` and
`xboxrecomp/src/apu/GP-INTEGRATION.md`.

Profiles: "strict" means `RECOMP_GPU_ACK=0` with no synthetic-completion switch
(`docs/jsrf-run-profiles.md`). Runs are under `logs/runs/` (gitignored, local).

---

## 1. Toolkit relationship and syncs

The toolkit is the owner's fork `danillogical/xboxrecomp` of `sp00nznet/xboxrecomp`; remotes and push
rules are in `AGENTS.md`.

### v0.11.0 sync (packet A4s-r6, accepted 2026-09-25)

Merged upstream `766ecef` into local main; ten files changed on both sides. Hunk resolution rules, in
order: **H1 containment** (if one side's additions and deletions contain the other's, take it);
**H2 disjoint edit union** (no base line changed by both sides: apply both sides' edits, local
insertions first at a shared point); **H3 version/comment only** (take upstream for those lines).
Clean hunks can still produce defects — the merge created a duplicate `case 138` and a duplicate
`bridge_KeResetEvent` — so `scripts/check-merge-structure.py` (conflict markers, duplicate `case`
labels, duplicate file-scope definitions over the evidence binary's build inputs) runs on the resolved
tree **before** the build. The general rule that upstream merges never silently change admitted
evidence semantics is in `docs/jsrf-run-profiles.md`.

**AC'97 hunk ruling (Advisor).** Upstream kept a set-only codec-ready write under
`getenv("RECOMP_AC97_READY")`, coupled the APU unmap to it, and added a NABM "RR" write trap. Resolved
to the **local** side: the accepted A3a model owns `GS.bit8`; `RECOMP_APU_TRAP` gates only the APU 512K
unmap; no `getenv("RECOMP_AC97_READY")` and no `|= MCPX_AC97_CODEC_READY` anywhere. Upstream's
`ac97_clear_reset_bits`/`ac97_write_veh`/`ac97_arm_write_trap` may exist but **nothing may call
`ac97_arm_write_trap`**. Arming it is its own change packet, and must change the model: the trap makes
page `0xFEC00000–0xFEC00FFF` read-only, which holds `GLOB_CNT` and `GLOB_STA`, so the tick thread's
`InterlockedOr/And` would fault every tick. The cleaner design evaluates `GS.bit8 := GC.bit1` in the
VEH's single-step half after each trapped guest write. Merge-check scope is the evidence binary's build
inputs, matched in the form that carries semantics (quoted env names, non-comment tokens, call sites).

### v0.12.0+ sync (owner, 2026-09-28)

Merged upstream `ea60cfa` into local main as `2925f0b` (121 upstream commits), pushed to the fork.
Three files conflicted (7 hunks):

| File | Resolution |
|---|---|
| `kernel_hal.c` includes | both sides |
| `xbox_memory_layout.c` worker tick | upstream's removal of the early `DMA_GET = DMA_PUT` (the advance now follows the pushbuffer scan: a lost-command race fix); local's mirror ticks outside the GPU-ack gate; upstream's `dsp_ack_tick()`/`poke_tick()` beside them (no-ops unless `RECOMP_DSP_ACK`/`RECOMP_POKE`) |
| `xbox_memory_layout.c` worker start | local `RECOMP_GPU_ACK` read + upstream `dsp_ack_init()`/`poke_init()` |
| `lifter.py` ×2 | independent additions, both sides |
| `lifter.py` ×2 | the same `rep cmps/scas` width fix on both sides, upstream's form |

AC'97 invariants held; structural check clean; build, `ctest`, toolkit tests pass; a strict run with
the baseline's settings reached the same stop. Upstream's nine new switches are classified in
`docs/jsrf-run-profiles.md` (`RECOMP_DSP_ACK`, `RECOMP_POKE`, `RECOMP_FORCE_RETURN`, `RECOMP_PAD_PRESS`
are synthetic → exploratory). Upstream's APU change of note: ADPCM's reserved header byte is no longer
validated and the step index is clamped — JSRF ends every ADPCM buffer in a `0x08` pad, and 3.5–4.4 % of
its blocks had been silenced.

---

## 2. Generated code

### Regeneration with the v0.12 lifter (owner, 2026-09-28)

Command (from the game root, toolkit on `PYTHONPATH`):
`python -m tools.recomp game/default.xbe --all --split 1000 --gen-dir src/recomp/gen --game-name "Jet
Set Radio Future" --manual-functions config/manual-functions.json --exclude-manual src/recomp_manual.c
--trace-functions config/trace-functions.json`.

- **Analysis inputs are the toolkit's** gitignored `tools/{disasm,func_id,abi_analysis}/output` (dated
  2026-09-21, the inputs of the previous pass). Every previously generated function is in the toolkit's
  8768-entry `functions.json`; 713 are absent from the game's 8437-entry one (which `relift-selected.py`
  and `recover-functions.py` use). A 2026-09-22 attempt with the game's database was reverted as
  unattributable. The provenance manifest records the toolkit files as `toolkit:` inputs.
- **Re-applied after the pass:** `relift-selected.py boundaries` (7 reviewed boundary fixes: 5 in
  generated code, 2 in `recovered.c`); the project's exact-delta ABI additions to `recomp_types.h`
  (three-way merge against the template at toolkit `484887b`); the six A4b2 `jsrf_watch_store` hooks
  (anchored on guest label and store). `recovered.c` untouched (recovery-owned).
- **Result:** 5580 → 5740 generated functions (+161 switch-arm entries, −1 folded tail); dispatch
  8768 → 8928; zero dropped `rcl`/`rcr`; untranslated instructions now emit `RECOMP_UNIMPL`.
- **Two fixes it required.** `sub_00162B9D` (`mov eax, 0x800401F0; ret 0xc`, a COM error tail of two
  recovered parents) is folded by the new translator and no longer emitted, so it is hand-written in
  `src/recomp_manual.c` (manual list and lookup). `recomp_unimpl` is defined there per the toolkit
  template: the instruction stays a no-op, is reported as `[UNIMPL] … REACHED`, and
  `RECOMP_UNIMPL_TRAP=1` aborts at the first one.
- **Comparison** (strict, `RECOMP_APU_TRAP=1`, 8 s; old `20260928-183145-568-crt-divide-fix-strict`,
  new `20260928-185612-449-regen-v012-strict`): both STRICT, same A3a witness, same allocation and
  terminal event; the 18 traced DirectSound functions have identical entry counts.
- **Open difference — D3D resource release.** Both builds make the same 116 contiguous allocations
  (ordinal 166). The old build frees 54 through `sub_00192830` (ordinal 171 from `0x001928BB`); the new
  build never enters `sub_00192830` (diagnostic trace `20260928-190035-939-regen-trace-192830`,
  positive control 634 other entries). Its own translation is equivalent in both trees, so one of its
  ten callers decides differently; main-thread critical-section pairs drop by ~160. Which build is
  faithful is not established. Rollback point: game `0f7ef9c`.

### Phase 0 re-baseline on Windows (owner-directed chores, 2026-09-29)

Plan §4 V1–V4, run on the Windows host at game `a62b5ce`→`44becd4`, toolkit `2a349c8`. This is the first
time the fork fixes `db96e30..2a349c8` were built and run on Windows.

**V1 — build and test.** `scripts/build-jsrf.py` exit 0. Game `ctest`: **26/26 passed**, including the
toolkit's `xbox_kmem`, `xbox_guest_meter` and `nv2a_actions`. Standalone toolkit projects, both built
with the host's generator (**Visual Studio 18 2026** — the host has no VS 2022, so a `Visual Studio 17
2022` configure fails; this is an environment fact, not a code defect): `tests/kernel_data_exports`
**5/5** and `tests/kernel_file_status` **5/5**, each including the same three toolkit tests.

**V2 — regeneration.** 5740/8928 functions (0 failed), 653342 lines of C; 7 unresolved targets stubbed;
23 unimplemented instructions (17 mnemonics); `recomp_funcs.h` and `recomp_stubs_unresolved.c`
byte-identical to the previous pass. Build and ctest pass again (26/26); provenance `--check` ok.
The pass reverted two hand-applied project deltas, which were restored: the ABI additions in
`recomp_types.h` (`recomp_delta_ok`, `recomp_delta_allowed`, `recomp_abi_regs_exempt`,
`jsrf_trace_delta_mismatch`, `jsrf_trace_seq`, and the delta-checking `RECOMP_ABI_CALL`), and the six
A4b2 `jsrf_watch_store` hooks. `scripts/apply-a4b2-hooks.py` now does the hooks by anchoring on the
**guest label or store** the translator emits rather than a line number, refuses an anchor that does not
resolve exactly once, and is idempotent — hand-editing them is how a hook silently disappears, and
because they are observation-only a missing one does not fail a build; it empties the watch artifact,
which reads as "the guest never wrote there". The pass also picked up the toolkit's new port-I/O
declarations (C6's prerequisite) automatically.

`FLAGS: 10 conditional(s) in 7 function(s)`. Nine are the jcc-form reads toolkit `ca4257c` predicted
(4 live into the function, 5 where an `adc` must answer `jl`/`jg`/`jo` in `adc [eax],al` byte runs); the
tenth is a `loope` in `sub_0010634E` that `ca4257c`'s census did not count because it counted jcc sites
only, and which is **byte-identical** to the pre-regeneration tree — not a regression. The 8 sites the
census called live bugs are fixed: `sub_00015130` (`loc_000153A9`), `sub_00130FD0`, `sub_000A0F10`,
`sub_001C0B86` carry no fallback read and use materialised `_fc_*` conditions. The plan's V2 criterion
read "≤ 9", which compared a jcc-only census against a wider report; it is now tied to the named sites
(Advisor ruling, recorded in the plan).

**V3/V4 — see §5** (the strict horizon) and §7 (the device-field verdicts).

### CRT 64-bit divide helpers (owner, 2026-09-28)

The previous generated tree dropped every `rcr` in the MSVC CRT divide helpers' normalisation loop, so
64-bit divides with divisors above 32 bits were wrong. Measured on the old generated `__aulldiv`:
`0x2540BE4000 / 0x100000001` gave `0x40BE3FFF` (correct `0x25`); `1000 / 7` was right. They are now
hand-written in `src/jsrf_crt.c`, listed in `config/manual-functions.json`, and tested by
`tests/test_crt_divide.c` (2116 cases, stack and preserved registers):

| VA | Helper | Result | Preserves |
|---|---|---|---|
| `0x0017C9C0` | `__alldiv` | signed quotient `edx:eax` | `ebx esi edi` |
| `0x0017D4D0` | `__aulldiv` | unsigned quotient `edx:eax` | `ebx esi` |
| `0x0017D2C0` | `__aullrem` | unsigned remainder `edx:eax` | `ebx` |
| `0x001816B0` | `__aulldvrm` | quotient `edx:eax`, remainder `ebx:ecx` | `esi` |

All stdcall `ret 0x10`, dividend low/high then divisor low/high; 24 call sites via `RECOMP_ABI_CALL`.
They did not cause the A2h allocation (same request after the fix). Removal gate: delete the manual
entries and the `jsrf_crt.c` bodies together if generated code should own them again.

---

## 3. AC'97 codec-ready model (packet A3a-r25, accepted 2026-09-24)

**Model** (toolkit `c97ce2c`, `src/kernel/xbox_memory_layout.c`, `nv2a_ack_thread`):
`GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated every tick, atomic, outside the
`g_apu_mmio_trapped` gate; `GC` is read only. It replaced the synthetic `RECOMP_AC97_READY` override.
Witness line: `[A3A] ac97 witness: gc=0x00000002 gs=0x00000100`.

**Result.** One strict run (`20260924-100502-623-a3a-codec-model`) selected `R-PASS`: no relaunch
(the baseline self-relaunched via `HalReturnToFirmware`), the codec poll succeeded (`ret=0x001A7432`),
the vector-6 ISR `0x001A72E0` was connected, and the guest reached DSP initialisation. **Limits:** one
strict codec wait satisfied by modelled state; not faithful hardware behaviour (secondary sources only),
audio, DSP handshake, liveness or boot. W1C on `GLOB_STA` is not modelled (its consumer `sub_001A71B3`
is reachable only from the vector-6 ISR). The packet (`docs/packets/a3a-ac97-codec-model.md`) is kept
because `scripts/ac2-provenance.py` reads its pins.

**Admission evidence** (the secondary-source path of `docs/jsrf-run-profiles.md` §"Unconditional
modeled hardware causes"; no public MCPX/ACI datasheet exists):

- **xemu** (Xbox-specific address map): `hw/audio/ac97.c` @ `2799183ecc5119269be01c340d0c9465dbb04d02`
  (SHA-256 `BD2A95FD…E554B`): L65 `#define GS_S0CR (1 << 8) /* ro */`, L135 `GLOB_STA = 0x30`, L785
  `val = s->glob_sta | GS_S0CR;` (unconditional; no `#ifdef XBOX` encloses it). `hw/xbox/mcpx/aci.c`
  @ `704ece9ac661f325aa51bb0b28d326063633227b` (SHA-256 `9A784826…986D2`): NAM at `+0x0`, NABM at
  `+0x100`; `hw/xbox/xbox.c:334` instantiates `mcpx-aci`. NABM `0x100 + 0x2C/0x30` puts `GLOB_CNT`/
  `GLOB_STA` at `0xFEC0012C`/`0xFEC00130`.
- **Linux** `sound/pci/intel8x0.c` @ tag `v6.6` (SHA-256 `F5F1AE46…B5FFC`): L140 `ICH_AC97COLD
  0x00000002`, L163 `ICH_PCR 0x00000100 /* primary (AC_SDIN0) codec ready */`; L2360–2361 sets
  `ICH_AC97COLD` when it reads 0 to *finish* the cold reset (so bit 1 is active-low `Cold Reset#`);
  L2295–2298 clears it to re-arm the reset.
- **Limits of the evidence:** xemu's Xbox-specific part is the address map; its bit-8 behaviour is
  generic QEMU code with a `TODO` for reset requests. Both descend from the Intel AC'97/ICH
  specification. The toolkit extracts other subsystems from xemu but has no AC'97 model of its own.
- **Guest corroboration (not counted as a source):** `sub_001A6C94` sets `GC` bit 1, clears bits 2–3
  (turning the link on), then polls `GS` bit 8 up to 1000 times — coherent only under the active-low
  reading.

---

## 4. APU and the GP DSP

### The spin (A4a-r2, discovery, accepted 2026-09-24)

The guest writes `3` to the DSP pending word `W = MEM32(0x001BA858)+0x810 = 0x803C0810` and spins at
`loc_001A18D0` (`sub_001A1769`) until it is cleared. With observation-only APU tracing
(`20260924-191833-331-a4a-r2-trap-trace`): GPSADDR `0x02040 = 803CC000`, GPSMAXSGE `0x020D4 = 8`, GPRST
`0x3FFFC` written `1` then `3`; zero GP/EP reads; SGE[0] points at block `B = 803C0000`, whose `0x5CC`
bytes equal the XBE image at file offset `0x1A7D60` (`0x001BA0A0`) with `3` at `B+0x810`. Row `O-6`:
the next packet is the GP DSP56300 engine.

### Port (A4b1-r4, accepted 2026-09-26)

xemu's DSP56300 core vendored byte-exact at `67cc79e663038d1f55448c0f566b37dde016adf6` (17 files) with
29 marked local modifications; the combined work is GPL-2.0-or-later (the owner approved the licence).
Provenance, per-file hashes, licences and modifications: `xboxrecomp/src/apu/dsp/PROVENANCE.md`.
Device semantics DS1–DS7 (bootstrap, per-frame run, guest-DMA translation, the atomic GP write choke
point, the lossless watched-word ledger, input accounting, trace): `xboxrecomp/src/apu/GP-INTEGRATION.md`.
The synthetic `RECOMP_APU_DSP_ACK` path was deleted. Acceptance: stage 1 `NOT ACCEPTED` on `AC-PORT`,
corrected, stage 2 `AGREED`; row `R1-PASS`. Lesson recorded twice in that packet: read the shipped
configuration, not just source — the APU library builds with `NDEBUG`, so xemu's
`assert(!"Unhandled dsp dma buffer")` is compiled out and an unimplemented `buf_id` read consumes stale
bytes as GP input (now accounted, gated on `is_gp`).

Follow-ups recorded then: enumerate `NDEBUG`-elided asserts in `src/apu/dsp/` that change GP state or
inputs (known: `unk2`/`unk13`, the `format` default, `dsp_offset` out of range); EP routing; the game
repository has no licence file.

### The GP clears the pending word (A4b2-r8, accepted 2026-09-27)

**Establishes:** in one strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, observation-only trace)
the `3→0` transition at `B+0x810` was performed by the GP engine's memory-write path while executing
the validated image, after the anchored guest store was recorded, with no synthetic ack or instrumented
competing CPU zero (`AC-CLEAR`: a `GP_CLEAR` latch from a successful compare-exchange `3→0`).
**Does not establish:** that the guest observed the zero, exited the spin or progressed; boot,
liveness, DirectSound or audio; DSP56300 instruction-level correctness; GP→CPU interrupts; EP execution.
Stage 1 `ACCEPT`, all ten criteria agreed; the reviewer rebuilt the `[GPIN]` blocks independently.

**Input qualifier — the exchange depends on no stub.** The descriptor that produces the exchange is
block 24, measured directly (`[GPDMADESC] GP_CLEAR produced by block_addr=0018`): control `0x59E2`
(`P 00F1`), count `6` (`P 000D`), DSP offset `0` (`P 000A`), scratch offset `0x800` (`P 000B`),
destination `0x4018` (`P 0009`/builder `P 00EB`), trigger `0x25` masked (`P 00C1`/`P 00D6`) — all
immediates. The two stub inputs do not reach it: `0xFFFFB3`'s four reads feed only loop-control scratch
`x:$007c..$007f`; VP `MIXBUF` feeds the separate audio-output descriptor. Alias/entry checks closed
(disjoint builder regions, 39 direct X writes miss the descriptor, the sole computed reader `P 00B9`
reads `0x24` on all six executions). At the exchange only image I (`P 0000..0172`) had executed; a
second image was loaded (3510 P writes above `0x172`) but not executed — loaded ≠ executed.
Non-reliance discovery row: `O-TWO-LEG` (`L1 = PROVEN`, `L2 = INVARIANT`).

**Carried constraint:** A4b2's P4 condition is toolkit-identity-sensitive. The toolkit has advanced
(`2925f0b`), so the discovery-transfer bridge must be re-established before anything inherits P4.

### `PIO_FREE` (`0xFE820010`)

- **Gate-only at the 28 direct reads (A4p-r1, discovery, accepted 2026-09-24, `O-GATE`).** Every
  direct read in `DSOUND` is a threshold re-poll whose value reaches no use, under an explicit x86
  calling-convention premise checked at every boundary relied on (C1: `ecx`/`edx` dead after calls,
  checked read-before-write; C2: register-argument checks at calls, one level; C3: `ecx` dead at `ret`,
  `edx` dead unless a caller reads it; C4 per the ruling). The population came from the XBE, reconciled
  by normalised value: 10 hex-spelled + 18 signed-decimal-spelled sites. Limits: the premise is
  inferred; direct reads only; not register-indirect/computed access, timing, or whether `0x80` is the
  true device value.
- **Why it does not contaminate A4b2 (Advisor Q1).** Stub dependence is judged one wait and one value
  at a time: the `PIO_FREE` answers decide whether and when the guest reaches the spin, not the data the
  GP acts on.
- **Deferred at `O-OPEN` (Advisor ruling).** The `0x80` stub passes all 13 constant gates (two,
  `0x001A3EB3` and `0x001A3FDB`, at exact equality) and is unproven for 15 variable gates (demand
  `k × byte[+0x64]` needs byte ≤ `32/k`). Hardware semantics are unknown and unsourced. Object-identity
  analysis was ruled out of proportion. **Reopen only** on newly admitted hardware source evidence, or a
  specific strict claim traversing named gates (then demand-driven per gate, never a forward census).

---

## 5. The A2h line: the strict terminal event

### Where strict runs end

**The kernel thunk table is overwritten, and the first thunk call after that faults.** This replaces the
single-stop-site framing that stood until 2026-09-29 (Phase 0 V3). The measured terminal event is not one
slot dying: it is the whole table being clobbered, after which *whichever thread next calls through any
thunk* raises `0xE0424943`.

- **The table.** `0x001C3F60..0x001C413F`, 120 slots of 4 bytes, the head of `.rdata`
  (`VA=0x001C3F60 vsize=161792 raw=0x001B4000`). In the original XBE every slot holds an
  `0x80000NNN` kernel thunk — `0x001C4064` = `0x80000115` = ordinal 277, and slot *N* is at
  `0x1C3F60 + N*4` with ordinal `value & 0x1FF`.
- **What replaces it (MEASURED, V3 dumps).** A record array of 40-byte stride whose index field counts
  upward: `0x79` at `0x1C3E08`, `0x80` at `0x1C3F18`, `0x88` at `0x1C4058`. The other fields are
  constant-looking — `0x3E800000` (0.25f), `0x41200000` (10.0f), `0xFFFFFFFF`, `1`, `0x001FA1D8`.
  In the V3(a) dump **0 of the 120 slots hold a patched `0xFE……` thunk**: 31 hold `0`, 89 hold
  record fields.
- **Every observed invalid target is a record field at the slot its call read**, which is what makes
  this one event rather than several:

  | Slot | Slot VA | XBE value (ordinal) | Read as | Failing call returns to |
  |---|---|---|---|---|
  | 46 | `0x001C4018` | `0x8000009F` (159) | `0x00000001` | `0x0018CE73` (the `ret` after `0x0018CE6D call [0x1C4018]`) |
  | 65 | `0x001C4064` | `0x80000115` (277) | `0x00000000` | `0x0014982E` |
  | 68 | `0x001C4070` | `0x8000007C` (124) | `0x41200000` | `0x00147D36` |
  | 70 | `0x001C4078` | `0x800000E7` (231) | `0x00000000` | `0x00147DBC` |
  | 71 | `0x001C407C` | `0x800000E0` (224) | `0x3E800000` | `0x00147DE2` |

- **Which site is first varies, and that is the race, not the horizon.** Six strict V3 runs on one
  build produced four different first sites: `0x00149828` 3/6, `0x00147D30` 1/6, `0x00147DDC` 1/6,
  `0x00147DB6` 1/6. A worker thread can die before the main thread (`0x00147D30` in V3(a), tid 63012,
  main thread at 5175 kernel calls). **The old site did not move**: `0x00149828` still fires in half
  the runs. The set is evidence of the race; the horizon is the clobber.
- **The old baseline had it too.** The 2026-09-28 dump (`20260928-185612-449-regen-v012-strict`) holds
  the identical record array at the same addresses, so the lifter and kernel-memory changes did **not**
  move this horizon. Its terminal event was `0x0014982E`, the same slot 65.
- **Bearing on the A2h line.** The terminal read is a slot in a table that is overwritten wholesale, so
  the A2h attribution watched a different address from the terminal event: its slot `0x0019D62C`
  (`base 0x0019B200 + 0x242C`) is in `g_Device`, not in `.rdata`. Its `last_write=001D5078` record
  (`20260928-121142-929-a2h-attrib-exp2-3b`) is real but is not this event. C1 is retargeted to the
  writer of the record array; the obvious instrument is a write watch on `0x1C3F60`.

### The writer of the record array (F1, 2026-09-30) — `O-OPEN` closed

**The array is not constructed at the table: it is copied there, from the XBE's own `.data`.** The
writer is `sub_00038530+0x398`, reached as `rip=exe+0x5361B8`, and it is a `rep movsd` image copy.
Measured in `20260930-221404-630-f1b-rdata-guard` (strict; `RECOMP_RDATA_GUARD=1`, ledger L32).

- **The writer, from the guard.** All **256** `[RDATA-GUARD] write` reports — the guard's total cap,
  `RO_GUARD_TOTAL_REPORTS` — carry `rip=exe+0x5361B8` on tid 24300. Symbolised against
  `build/Release/jsrf_recomp.map`: `sub_00038530` at `0x140535E20`, so `+0x398`. That function's
  generated body contains `rep movsd`/`rep movsb` idioms (`src/recomp/gen/recomp_0000.c`), and it is
  the slot writer already named by A2h (TR §5, row `O-OPEN`) — the same function, a different site.
- **The copy is image-wide, not a table poke.** The guard's reports run from `0x00011000` upward, four
  dwords per 4 KB page, all from the same writer. They **stop at `0x0005000C`** because the guard hit
  its 256-report total cap after 64 pages — so the guard proves the copy's *start, direction and
  writer*, and it does **not** by itself prove the copy reaches the table. The reach to the table is
  established by the shifted-provenance map below, which covers the whole range.
- **Shifted byte provenance (AGENTS.md: this establishes provenance, and is never a correction).**
  The dump at guest VA `V` equals the original XBE at `V + 0x37608`. Sampled at 4 KB granularity over
  `0x00011000..0x001C5000`: **431 of 436 pages are `SHIFTED`, 0 are original**, 5 unreadable. Spot
  checks over `.text` match exactly (`0x00011000`, `0x00018000`, `0x00020000`, `0x00030000`,
  `0x00080000`, `0x00100000`, `0x00140000`, `0x00180000`). The page holding the table, `0x001C4000`,
  is in the `SHIFTED` set.
- **The record array is the XBE's `.data` at `0x001FB568`.** The dump's 128 bytes at the table VA
  `0x001C3F60` are **byte-identical** to the original XBE's bytes at `0x001FB568`. Applying the same
  `+0x37608` shift to the table destination gives exactly that address. So the array is not produced by
  the guest at run time; it is *read from the image* and written over the table.
- **What the table should hold.** The original XBE at `0x001C3F60` holds `800000BB 800000BE 80000121
  800000EC …` — all `0x80000NNN` kernel ordinals. **0** dwords of that shape survive anywhere in
  `0x001C0000..0x001C8000` in the dump.
- **Consequence for the terminal event.** The horizon is unchanged: the table is overwritten and the
  next thunk call faults. The two F1 runs differ in *which* call faults first (`0x0014982E` tid 47532
  without the guard; `0x00147CF8` tid 26240 with it) because the fault races the copy's progress —
  the same race TR §5 already records. The guard reports stop at log line 20892 and the fault is at
  20919, so the copy was still running when the first thunk call read a half-written slot.
- **The mechanism to fix.** A `rep movsd` whose destination should not be `0x00011000` and whose
  length should not be 1.70 MB. `sub_00038530` computes both; the cheapest honest fix is at the
  translation or the argument, not at the table.

### F4: the GPU walk stops on the first method it does not know (2026-09-30)

**No frames are produced, and the reason is not the executor's rendering — it is that the submission
walk never consumes a command.** Measured on the fixed build in
`20260930-230206-594-f4-frames-after-horizon-fix` (exploratory; `RECOMP_PB_EXEC=1`, `RECOMP_FB_WINDOW=1`,
`RECOMP_GPU_ACK` default-on, 123.3 s, 629,781 log lines, 0 invalid ICALLs, 0 exceptions, 0 ABI failures,
0 `[UNIMPL]`).

- **The walk is stuck.** The run logs **64** `[PFIFO] submit` lines. The first ten advance `get` normally
  (`0x1000`, `0x2764`, `0x344C`, … `0x8B5C`). Every one of the remaining **54** reports the *same*
  `get=00008EF0` while `put` keeps advancing — the walk is not moving.
- **The first failure is a method the model does not handle.** Submits #12 onward carry
  `diag=unsupported_method … method=1720 … at=00008EF0`. `0x1720` is
  `NV097_SET_VERTEX_DATA_ARRAY_OFFSET` (`nv2a_regs.h:1141`). The submit record names the method and the
  parameter (`param=003CA000`, a guest VA), so the command is decoded; the walk simply has no case for
  it and stops at that address.
- **Consequence.** `get` never passes `0x8EF0`, so nothing after that command in the pushbuffer is ever
  interpreted. `FLIP`, `present` and `FB_DUMP` are all **0**: there is nothing for the executor or the
  window to draw, and enabling them cannot help until the walk advances.

**So F4's next step is not in the renderer.** The cheapest honest class is to give the strict walk a
case for `NV097_SET_VERTEX_DATA_ARRAY_OFFSET` — store the offset as method state, as `9fd83c6` already
does for other NV097 parameters (`PGRAPHState.methods`, ledger D3) — and re-run to see whether the walk
then reaches a draw method or stops on the next unknown. That is a toolkit change with its own test.

**Not established.** Whether `0x1720` is the *only* blocker or the first of a series: the walk stops at
the first unknown method, so the population of unhandled methods JSRF's first frames need is unknown
until the walk advances past this one. The `[PFIFO]` line is a bounded log, so it bounds this run, not
the title's method set.

### F3: the horizon is closed — two wrong tail-jump aliases, fixed (2026-09-30)

**The A2h/C1 terminal event is closed.** The kernel thunk table is no longer overwritten, `.text` is
no longer displaced, and a strict run now reaches its 93-second deadline instead of faulting at ~6 s.

**Cause.** Two entries of the function-pointer table at `.data 0x001EC0F8..` were folded into their
abutting neighbours by the translator's `tail_jump_alias` rule and had their bodies deleted, so a call
through the table entered an unrelated function:

| Address | Table slot | Folded into | Its own code ends | Its jump table |
|---|---|---|---|---|
| `0x00037550` | `0x001EC108` | `sub_00038530` | `0x00037603` (bare `ret`) | `0x00037FB4` |
| `0x00026780` | `0x001EC10C` | `sub_000278F0` | `0x00026816`+epilogue | `0x0002730C` |

**The `0x00037550` span and ABI were re-verified after review, and `stack_args: 0` is correct.** The
review question was whether the `ret 4` at `0x00038525` belongs to this function, since the recovery
entry declares no stack arguments and `recover-functions.py` derives its expectation as
`stack_delta = 4 + stack_args` (`scripts/recover-functions.py:77-86`). It does **not** belong to it.
Three independent measurements agree:

1. **The translator's control-flow-following decoder reaches exactly one `ret` from `0x00037550`: a
   bare `ret` at `0x00037603`** (`c3`), the end of the function's own SEH epilogue
   (`xor eax,eax` / `mov ecx,[esp+0x218]` / `pop edi/esi/ebp/ebx` / `mov fs:[0],ecx` /
   `add esp,0x214` / `ret`). Every switch arm jumps to `0x000375E9`/`0x000375EB` and leaves there.
2. **`0x00038525` is not reachable from `0x00037550` at all.** The decoder's set ends at `0x00037FAD`;
   `0x00038525` is 0x5B8 bytes beyond it and is only reached from a different function. Decoding
   `0x00038460..0x00038530` and `0x0003848E..0x00038530` both reach it — and both are `ret 4`
   functions of their own, so the `ret 4` belongs to them.
3. **The runtime ABI check passed on the fixed build.** Run
   `20260930-225739-446-f3-alias-fix-2-strict` logs
   `[RECOVERED] 0x00037550 returned; ABI verified (ESP/EBX/ESI/EDI)`, and there is no
   `ABI FAILURE 0x00037550` line anywhere in it. The generated wrapper asserts
   `g_esp == before_stack + 4`, so the guest's own call convention confirms the bare `ret`.

An earlier version of this table and of the bullet below said the function "ends `ret 4` at
`0x00038525`". That was wrong — it described the fold's span end, not the function's own code — and it
is corrected here rather than deleted, because it is the kind of error that would otherwise justify a
wrong `stack_args` in the next recovery entry.

`sub_00038530`'s `rep movsd` copied the XBE image from `+0x37608` over `.text` and `.rdata`, 1.70 MB
from `0x00011000` to the thunk table. The generated dispatch named the first substitution itself:
`[ALIAS-ICALL] target=0x00037550 owner=0x00038530`.

**Fix.** A recovery entry for each address in `config/recovered-functions.json`, with the span ending
at the function's own jump table — the table is data, not code. `recomp_lookup_manual` consults
`jsrf_lookup_recovered` before the alias table, so the recovered body wins. The spans were chosen from
the bytes and verified before use: all table targets lie inside the span, every one carries a label in
the emitted body, and there are 0 dangling gotos and 0 `RECOMP_UNIMPL` markers.

**Two further config spans were tightened**, because each ran past its function's terminator into bytes
that decode as instructions, which aborts `scripts/recover-functions.py` and blocked the regeneration:
`0x000BCF40` (`end` `0x000BD8D0`→`0x000BD8B0`, its `ret`; the old value decoded padding as `aaa`) and
`0x001063A0` (`0x00106580`→`0x00106560`, its `ret`). With all four edits the regeneration completes —
**3075 functions**, the first successful run of `scripts/recover-functions.py` in this session.

**Measured effect, strict profile, `RECOMP_APU_TRAP=1`, budget 100000:**

| Run | Outcome | Evidence |
|---|---|---|
| `20260930-221054-913-f0b-first-run-new-toolkit` (before) | `0xE0424943` at 5.8 s | 17,906 log lines; table held the record array |
| `20260930-225440-580-f3-alias-fix-strict` (`0x00037550` fixed) | `0xC0000409` at 14.6 s, `[RECOVERED] ABI FAILURE 0x00026780` | the **next slot in the same table**; 77,608 lines |
| `20260930-225739-446-f3-alias-fix-2-strict` (both fixed) | `diagnostic_deadline` at **93.0 s** | 487,394 lines; **0** invalid ICALLs, **0** `0xE0424943`, **0** exceptions, **0** ABI failures, **0** `[UNIMPL]` |

The decisive check is the dump's own integrity gate, which reads guest VA `0x00011000`:

- **Before:** `CONTENT_MISMATCH` in every run — the dump held the XBE's bytes from `+0x37608`.
- **After:** `matches: 1, content-mismatch: 0`. `.text` at `0x00011000` is `852C518B 30418BD2 …`,
  byte-identical to the original XBE, and the thunk table at `0x001C3F60` holds the runtime's installed
  `FE000000 FE000004 FE000008 …` thunks rather than the record array.

**This confirms the F3 diagnosis by its own stated test** — "fixing the alias should move the strict
horizon" — and the prediction held twice, once per alias.

**Not established.** Why the guest's call reaches these table entries in the first place, and whether
any *other* `tail_jump_alias` fold still deletes a table-referenced entry. The census that would answer
the latter is not yet a valid instrument (see plan §13). The 93-second run ended at its own deadline
with no fault, so the next stop is unknown and is F3's continuing subject.

### F3 evidence: why the `0x00037550` fold is fatal (2026-09-30)

**The guest called `0x00037550`; the port ran `sub_00038530` instead.** That substitution is the whole
defect, and it is measured end to end.

- **The substitution is observed, not inferred.** `[ALIAS-ICALL] target=0x00037550
  owner=0x00038530` is printed by the generated dispatch's own `recomp_alias_observe`
  (`src/recomp/gen/recomp_dispatch.c`). It appears in **both** F1 runs, and in the guard run it is at
  log line 17896 — **19 lines before** the first `[RDATA-GUARD] write` at 17915, on the same thread
  (tid 24300). The copy is what the guest got *instead of* calling `0x00037550`.
- **`0x00037550` is a real function with its own body.** Its own code ends at a bare `ret` at
  `0x00037603`, the end of its SEH epilogue; `0x00038525` holds a `ret 4` that belongs to
  `0x00038460`/`0x0003848E`, not to this function (see the re-verification above). It has its own SEH
  prologue at its start (`push -1` / `mov eax, fs:[0]` / `push 0x1870EE`) and a jump table at
  `0x00037FB4` on `[esi+0x15DC]`.
- **It is a function entry by definition.** It occurs **exactly once** as a 32-bit pointer in the XBE,
  in a run of distinct `.text` addresses at `.data VA 0x001EC108` — neighbours `0x00036640`,
  `0x00028500`, `0x00037550`, `0x00026780`, `0x00038890`, `0x0002AD60`. It has **0** direct
  `call`/`jmp` sites landing on it, so that table is the only way it is reached — which is exactly
  why a fold is fatal here and invisible in the call graph.
- **The translator folded it.** `tools/disasm/output/functions.json` classifies `0x00037550` as
  `detection_method: tail_jump_alias`, `confidence: 0.88`, spanning `0x00037550..0x00038530` — i.e. it
  gives the function the *fold target's* start as its end and deletes its body. `sub_00038530` is
  separately `call_target`, `0.9`, `0x00038530..0x0003885D`.
- **No generated body exists for it.** `sub_00037550` appears **0** times as a definition or call in
  `src/recomp/gen/*.c`; it exists only as the alias pair in `recomp_dispatch.c` and as the dispatch
  table's entry.
- **This is the same defect class the config already records.** `config/recovered-functions.json`'s
  entry for `0x000BCF40` describes it in the same words: *"The translation pass read that table as a
  switch table, classified this address `detection_method=tail_jump_alias` and folded it into
  `sub_000BD8D0` under the ff4d442 abutting-alias rule, deleting the body and pointing the dispatch
  tuple `0x000BCF40` at `sub_000BD8D0`. A virtual call then enters the wrong function."* `0x000BCF40`
  was fixed by a recovery entry plus a boundary fix, and the alias table then named the recovered
  body.
- **The fix path is therefore known and precedented.** `recomp_lookup_manual` consults
  `jsrf_lookup_recovered` before the alias table, so a recovery entry for `0x00037550` takes precedence
  and restores the real body. It was applied as recorded above; at the time of this measurement
  `jsrf_lookup_recovered` had **3074** cases and **none** for `0x00037550`.

**Bearing on the horizon.** This is upstream of the clobber: the fold is why the wrong function runs,
and the wrong function's `rep movsd` is what overwrites the image. Fixing the alias should move the
strict horizon, and that is the test of this diagnosis — it was applied, and the horizon moved.

**Not established.** Why the guest's call reaches this table entry in the first place, and whether any
*other* `tail_jump_alias` fold in this image still deletes a table-referenced function entry. The
latter needs a census, and the one attempted in this session was not a valid instrument (plan §13).

The ~571 MB allocation failure that precedes the fault (`NtAllocateVirtualMemory`, `0xC0000017`)
**is handled** by the guest (`0x00149E56 test eax,eax` / `jl 0x149eec`, clean return through
`__SEH_epilog`), so it is not the cause (Advisor critical-path ruling, 2026-09-27); V3(a) and V3(c)
reproduce that return and still fault afterwards. The producer line that chased the allocation size is
parked; reopen only if the clobber proves downstream of that error handling, or a later gate needs the
size explained.

**Phase 0 V3 measurements** (game `44becd4`, toolkit `2a349c8`, 8 s, `RECOMP_GPU_ACK=0
RECOMP_APU_TRAP=1 RECOMP_KERNEL_LOG_BUDGET=100000`). All three classified as the plan requires;
`[UNIMPL]` lines: **0** in all seven V3 logs; `RtlRaiseException` and `0xE06D7363`: **0**.

| Run | Label | Profile | First terminal site | Kernel calls | Stop (UTC) |
|---|---|---|---|---|---|
| (a) | `20260929-231110-868-rebaseline-strict` | STRICT | `0x00147D36` (tid 63012) | 6456 | 06:11:16.686 |
| (b) | `20260929-231200-429-rebaseline-kmem-legacy` | EXPLORATORY | `0x00147DBC` (tid 28756) | 6431 | 06:12:06.024 |
| (c) | `20260929-231211-023-rebaseline-gmeter` | STRICT | `0x0014982E` (tid 46508) | 6321 | 06:12:16.688 |

`[KMEM] summary` (a): `legacy=0 regions=13 hint_ok=0 hint_conflict=0 reserve_ok=15 reserve_fail=0
alloc_invalid=0 commit_region=52 commit_heap_block=0 commit_rejected=0 pages_zeroed=734 decommit_ok=0
decommit_failed=0 release_ok=2 release_failed=0 free_bad_type=0 region_table_full=0 contig_free_ok=0
contig_free_unknown=0 contig_untracked=0 contig_split_skipped=0 heap_split=3 heap_split_full=0
heap_carve_ok=0 heap_carve_busy=0 heap_carve_full=0`; (c) is identical except `commit_region=54
pages_zeroed=738`. (b) prints `legacy=1` alone, which is the override's own switch. (a) and (c) print the
same two rejects — `kind=release_failed … type=0x8000 status=0xC00000A0` and `kind=reserve_failed
base=0x00000000 size=0x23B20430 type=0x801000 status=0xC0000017` — and **(b) prints no `[KMEM] reject`
line at all**, which is the legacy path's behaviour (the reserve hint is not evaluated, so the
`reserve_failed` reject is not emitted). All three print the same ten data exports: ordinals 16, 40, 156,
164, 259, 322, 323, 325, 354, 356 (slots 15, 30, 62, 58, 67, 87, 38, 39, 40, 55 →
`0x00740000..0x007404A0`). `[GMETER]` (c) only: `max=4 inside=1 contended=2954 host_kcalls=2
anomalies=0`, entries/contended/nested/exits `kernel=4426/2856/0/4432`, `isr=60/51/0/59`,
`dpc=59/43/0/59`.

**(b) is the same event, not a different one.** The legacy kernel-memory semantics change *when* the
clobber lands, not whether it does: (b)'s dump holds the identical record array (0 of 120 slots patched,
the same five slot values). What differs is which thunk call happens first *after* the clobber — in (b)
`0x00147D36` is still a working call (`#809: ordinal 124 (slot 68) ret=0x00147D36`) and the first fault
is one call later at `0x00147DBC`; in (a) `0x00147D36` is the first fault. The two runs are not
comparable beyond that (`docs/jsrf-run-profiles.md`: a strict run may stop earlier than an exploratory
one), and no claim about the clobber's *cause* is drawn from the A/B.

**Independent re-verification (2026-09-30, owner-directed startup session).** Phase 0 V1–V4 were already
executed and committed by the preceding session at this same revision pair (`44becd4`/`56372dc`). Rather
than re-run them, this session re-measured their recorded values against the archived artifacts and the
current tree, and executed the outstanding V5. Every value below was re-derived, not copied:

| V | Recorded value | Re-measured this session | Result |
|---|---|---|---|
| V1 | build exit 0; game ctest 26/26; `kernel_data_exports` 5/5; `kernel_file_status` 5/5 | `build-jsrf.py` exit 0; ctest **26/26**; both standalone projects rebuilt and **5/5** each | agrees |
| V2 | `FLAGS: 10 conditional(s) in 7 function(s)`; 5740/8928 | full pass re-run into `logs/v2repro/gen`: **`FLAGS: 10 conditional(s) in 7 function(s)`, 5 `state: none` + 5 `adc cannot answer`**, the same ten named sites; chunks sum **5740** (banners) with **5739** bodies (`sub_00162B9D` is folded and hand-written in `recomp_manual.c`); dispatch **8928 unique**; provenance `--check` ok | agrees |
| V2 | "no site outside the pre-regeneration set" | the leftover-`_flags` function sets at `e73e495` and at `44becd4` are **identical (3868 functions; NEW = NONE, dropped = NONE)** | agrees |
| V2 | the 8 sites `ca4257c` called live bugs read no fallback | `sub_00015130`, `sub_00130FD0`, `sub_000A0F10`, `sub_001C0B86`: bare `if (!_flags)` reads **0** each, materialised `_fc_` conditions present (10/16/4/4) | agrees |
| V3 | (a) strict, (b) exploratory, (c) strict | re-classified: **STRICT / EXPLORATORY / STRICT** | agrees |
| V3 | horizon = the thunk table clobber; first site varies | fresh strict run `20260930-001405-390-v1-verified-strict` on the binary built this session: **STRICT**, `0x0014982E` (tid 64532), 6488 kernel calls, 0 `[UNIMPL]`, the same ten data exports, `legacy=0`, same two `[KMEM] reject` lines | agrees |
| V4 | the five disassembly sites and their bytes | all five re-disassembled and byte-matched; the only `+0x242C` store is `sub_0018CE30` at `recomp_0004.c:54015` | agrees |
| V4 | `sub_00038530` object base / overlap | `arm_base=0x0019B200` (= `g_Device`), `+0x242C` → `0x0019D62C`; extent ends `0x0019DCE0`; overlaps `g_Device` **yes**, overlaps the page of `0x001C4064` **no** | agrees |
| V4 | `+0x2440` REFUTED | **does not hold** — see the `+0x2440` REOPENED note in §7 | **corrected** |

Two limits on the re-verification, stated rather than left implicit:

- **All five archived dumps are `CONTENT_MISMATCH`** under `scripts/check-dump-mapping.py` (the 2026-09-28
  baseline too). They remain structurally readable at their actual guest VAs, and the values above were
  read that way; no value here is an image-content claim, and no shifted read was used as a repair.
- The `[KMEM]` reserve-hint counter at `0x1495E3` and commit counter at `0x14961B` are **not** printed by
  the runtime as counters. Both are call sites of the **same** thunk slot: `0x001495F3 call [0x1C3F88]`
  (reserve) and `0x0014962C call [0x1C3F88]` (commit), and `0x001C3F88` is slot 10 → ordinal 184. The run
  log shows them as `#9: ordinal 184 (slot 10) … ret=0x001495F9` and `#10: ordinal 184 (slot 10) …
  ret=0x00149632`, **exactly one each** in every run examined (the three V3 runs and the 2026-09-30
  verification run). So the per-run value is 1 reserve-hint call and 1 commit call; there is no aggregate
  counter to quote, and `NtAllocateVirtualMemory`/`NtFreeVirtualMemory` are not individually named by the
  runtime's kernel log. Recorded because an owner instruction asked for the counter.

**V5 — recovered-functions audit (2026-09-30, one DeepSeek worker, read-only).** For all 3074 entries of
`config/recovered-functions.json`, does the current translator now emit that address's body natively?
**Answer: 122 OBSOLETE, 2952 STILL_NEEDED, 0 UNKNOWN.** The verdict set is exactly the config entry set
(3074 addresses, no duplicates).

| class | mechanism | count |
|---|---|---|
| OBSOLETE | default full pass (no option) | 58 |
| OBSOLETE | `--coalesce-functions` | 64 |
| STILL_NEEDED | no current option emits a body for it | 2952 |
| UNKNOWN | — | 0 |

- **Evidence is generated-code pointers, not reasoning.** OBSOLETE requires `{ 0x<ADDR>u,
  (recomp_func_t)sub_<ADDR> }` in a scratch pass's `recomp_dispatch.c` **and** an `Original: 0x<ADDR> -`
  banner over `void sub_<ADDR>(void)`. STILL_NEEDED is either a fold (the dispatch tuple names a
  `recomp_alias_<ADDR>` wrapper whose body is `<OWNER>();`) or no dispatch at all.
- **The committed `src/recomp/gen/` tree cannot answer this**, in either direction: it defines **0** of the
  3074 addresses because `config/manual-functions.json` lists all 3074. All evidence came from three
  scratch full passes under `logs/` (recorded-database, fresh-database, and `--coalesce-functions`).
- **Controls, 0 failures:** 25 ordinary generated functions classify `OWN_BODY` (positive — what a native
  body looks like); 15 sampled folds stay folded; 7 data addresses are `NO_DISPATCH` in both trees
  (negative — what "no body" looks like).
- **Two of the three mechanisms the plan named do not retire anything.** `jump_table_entry_starts` is 60
  (recorded db) / 54 (fresh db), and **0** of the 3074 addresses are in that set, so switch-arm recovery is
  not the mechanism. The returning-body probe is not either: it reports true for 2487 of the 3074, but it
  was already true when the entries were written — being true is *why* the detector classifies them
  `tail_jump_alias` instead of standalone functions. Only `--coalesce-functions` retires entries (64).
- **Recorded database vs current sources:** the two passes agree on 3073 of 3074; the single difference
  (`0x00040001`) is STILL_NEEDED either way, so the table is unaffected.

Artifacts (all under gitignored `logs/`): `v5-recovered-audit.md` (full per-entry table, 1.4 MB),
`v5/verdicts.json` (the 3074 verdicts), `v5/controls.log`, `v5/coalesce-sweep.json`,
`v5-gen/`, `v5-gen-fresh/`, `v5-gen-coalesce/`. **No entry was retired** — retirement is later work.

**Qualification for the retirement packet.** OBSOLETE means "a body for this address is emitted", not "the
reviewed span is identical". Comparing emitted spans with the reviewed `end`: **53 exact**, **66 wider**
(the body carries the reviewed span plus a neighbour's tail or padding), **3 shorter** (`0x00190FB0`,
`0x001910C0`, `0x001912A0`). The three shorter ones are benign and byte-checked: each emitted span ends on
the routine's own `ret` (e.g. `0x0019101D-0x0019101F` = `c2 0c 00`, the `ret 0xC` the entry's evidence
names), and the address the reviewed `end` reached is a *separate* recovered entry with its own emitted
body — the reviewed `end` was widened over the next routine. A retirement packet must therefore re-check
each of the 122 spans rather than assume the reviewed `end` was right.

**One OBSOLETE row's emitted body genuinely overlaps another's** — the one case a retirement packet must
handle rather than merely re-check. Of the 122: 57 are enclosed by exactly one emitted body, **64 are the
`--coalesce-functions` rows whose address is not inside the *default* pass's bodies at all** (they are
emitted only under that option), and **1 overlaps**:

| row | emitted body | conflict |
|---|---|---|
| `0x00162AB0` | `sub_00162AB0` `[0x00162AB0, 0x00162BA5)` | starts strictly inside `sub_00162A20` `[0x00162A20, 0x00162B9D)` and runs **8 bytes past its end** |

Those 8 bytes are the `sub_00162B9D` tail (`0x00162B9D: mov eax,0x800401F0; ret 0xc`) — the same COM error
tail §2 records as folded by the new translator and hand-written in `src/recomp_manual.c`. Both
`0x00162A20` and `0x00162AB0` are OBSOLETE rows in `config/recovered-functions.json`, so one region carries
two entries and an overlapping emitted span: retiring either alone must first establish which body the
runtime actually needs there.

*Method note.* This overlap came from the V5 worker's own final consistency check, which passed its three
defect gates (0 sweep/fold disagreements, 0 verdict-rule mismatches, 0 `OWN_BODY` rows lacking their own
banner) and reported the nested bodies as a residual observation. Re-checked here with half-open interval
semantics, which separates a genuine overlap from ordinary **adjacency** (one body ending exactly where the
next begins — normal in a chunked translation, not a hazard). Two of the three addresses that check flagged
(`0x00190FB0`, `0x001910C0`) are adjacent, not overlapping; only `0x00162AB0` genuinely overlaps.

**Null-slot triage (A2h-null-slot-triage-r1, accepted, `O-NO-BOUNDARY-TRANSITION`).** `[0x1C4064]`
read its installed value `0xFE000104` (raw `0x80000115`, index 65) at every one of 15,498 sampled
kernel-bridge boundaries on all six threads — per-thread series complete, no gap or duplicate. The
last boundary (`#5555`, ordinal 294) still read `0xFE000104`, and the next event is the terminal read of
`0`, with no bridge call between. Macro mismatch, torn read, static displacement and stack-over-data
were refuted offline. No instrument observed the guest code in that final gap.

### Slot-writer attribution (A2h-slot-writer-attribution-r2, accepted 2026-09-28, row `O-OPEN`)

The line then watched the D3D device's callback slot: `software_device = MEM32(0x19DCE0)` (observed
`0x0019B200`), slot `+0x242C` = `0x0019D62C`. The packet's terminal oracle is a genuinely exercised
fourth read at `0x00193E62` tied to `0x00193EB5 call eax`; every run's terminal target was `0`. How this
slot's value relates to the terminal read is exactly what the successor must establish.

- **Finding (K≥2):** the recompiled body of `sub_00038530` (`0x00038530..0x0003885D`, 813 bytes, a
  structure initialiser) writes `0x001D5078` into the slot, reproducibly (same native RVA `0x52FE38` in
  two runs with different image bases and threads), and it is **not** the static candidate
  `0x00199F45` (in `sub_00199DB0`). The installer is `sub_0018CE30` (guest write at `0x0018CE3A`).
- **`0x001D5078` is data:** a pointer to the ADX filename `"djv000_0.adx"` in an `.rdata` table of 59
  code→filename entries — a data-as-call if it is ever called.
- **Third actor:** `VCRUNTIME140.dll`'s `memset`/`memmove` also writes the watched page (the same two
  RIPs, 4 bytes apart, in three runs with three image bases). It may be what zeroes the slot between the
  competitor's write and the terminal read — a hypothesis to **test** ("memset write immediately
  preceding terminal-zero by tick order"), never assert.
- **Row withheld:** the bar is writer observed **and** matching terminal **and** controls green; the
  terminal read `0` in every run. A post-hoc "scoped row" was refused: rows are frozen predicates.
- **Advisories carried to the successor:** `installer_control_hits` counts any game-module slot store
  (over-inclusive name); `term_base_ok=0 / base_changed=1` in every ON run (the terminal slot could not
  be re-derived); `unexpected_exception` (~780) and `first_touch_dropped` (~9000) are unanalysed.
- **Static chain (four-edges analysis):** the slot has exactly one direct store; the index-51 dispatch
  `0x000D4DA2 jmp dword ptr [eax+0xcc]` selects thunk `sub_00153790`, which forwards its first argument
  as `sub_00199DB0`'s index. The store hits the slot only for index 2064 (`0x810`), and no reachable code
  materialises that value; the smallest missing edge is that dispatch's caller.

### The specified successor (Advisor ruling, 2026-09-28)

Bounded runs (pre-specified N, early stop) for writer-observed + terminal-match + controls green;
zero qualifying ⇒ report and re-refer. Split the classifier's `unknown` class into host-identifiable
(module-range classified, e.g. `VCRUNTIME140` → `HOST` with the module named) and truly unplaceable
(fail closed). `ledger_mismatch` runs are contrast only. Test the CRT-`memset` lead. A run that cannot
be verified and recorded inside the session window is not evidence — it is an unaccounted draw
against N. New baseline: toolkit `2925f0b`, game ≥ `e73e495`; re-derive native RVAs per run.

### Instrument facts

- Page-protection watch only (no debug registers). Every write to a watched page, slot or not, is
  recorded, single-stepped and the page re-armed read-only — page sharing makes any "leave RW" shortcut
  blind to the slot.
- A native RIP is classified by range (recompiled-module bounds vs host image), never by decoding guest
  bytes at it. Mapping a RIP to a guest function uses the run's own linker map; the **symbol** is right
  but the **offset** is not guest-meaningful (generated C is ~5.5× the guest size; the installer's
  native offset was `0x5D` for a guest offset of `0x3A`).
- Debug-register arming coverage: `dr_disarm_all` (`tools/harness/collect.c`) zeroes DR0/DR7 on every
  live thread, so its disarm count is not an arm count; every successful arm and every create-thread
  event must be printed as it happens.
- Harness `winerror=5` failures at 0.51 GB free were **disk exhaustion** (full pass at ~62 GB free). A
  clean-baseline reproduction excludes the diff, not the machine.

---

## 6. Method lessons that apply beyond one packet

- **Enumerate by value, not spelling.** The lifter spells one address two ways (see `AGENTS.md`); a
  one-spelling grep found 10 of 28 sites.
- **Linear disassembly drifts** at the first data island; completeness claims need a recursive-descent
  walk (the A2h analysis used 30,118 seeds → 673,726 instruction starts). **Aligned dword scans miss
  unaligned immediates** (`mov dword ptr [esi], imm32` put `0x001E1270` at a non-4-aligned address);
  raw-byte scans are the fallback.
- **No hand counts in decision inputs:** tool-computed, from a named artifact, with a positive control
  and loss accounting.
- **Attribute a terminal event** by reading the instructions past the failing call, never by temporal
  co-occurrence; an untested lead must not harden into accepted fact.
- **Read what ships:** build configuration (`NDEBUG`), line endings (`core.autocrlf` changes hashes of
  checked-out files), and which analysis database actually produced generated code.
- **Carry-forward lists are re-justified per packet;** a brief cannot add criteria to a frozen contract.

---

## 7. Fork audit and owner-directed toolkit fixes (2026-09-28/29)

**Status: committed to toolkit `main` (`db96e30..2a349c8`) and game `master`, and not verified on
Windows.** No MSVC build, no `ctest`, no regeneration and no JSRF run has used this toolkit revision.
Verification here was a MinGW-w64 cross-build of every toolkit target (zig 0.16, clang 21; only the
three `d3d8_smoke` executables fail, at link, for want of `d3dcompiler`), the toolkit's pytest suite
(643 passed, 2 skipped, 1 macOS-only undefined-behaviour negative control), and host-native runs of
the new pure-logic C tests.

### The audit

All 23 forks of `sp00nznet/xboxrecomp` and two second-level forks were compared **by content** against
the fork's `main` `db96e30` (= upstream `ea60cfa` + local work); upstream squash-merges fork PRs, so
ancestry overstates what is missing. Upstream's own `work/*` branches hold nothing beyond `main`.
Licences that bound reuse: DanielJVoxSmart is GPL-3.0 since `88cde2c` (ideas only); Tiptup300's
semaphore code is from an unnamed source (ideas only); many NoRain211 commits carry an "Antigravity"
bot identity (noted in the commits that took them, per upstream `CONTRIBUTING.md`).

DanielJVoxSmart brought JSRF up on its own HLE runtime (`docs/technical/third-title-jsrf.md` at
`DanielJVoxSmart/main` `505ae8e`, 2026-09-20). It stalled in `sub_001497DC` — the allocator that holds
our strict terminal call at `0x00149828` — and its two root causes (flags lost across `lock xadd`, so
every COM `Release()` destroyed the object; the missed function `0x00154DAA`) are already fixed in this
toolkit and game. It then died on two indirect calls to non-code with an unreconciled contradiction
between two of its own measurements. Provenance only; not evidence about this build.

### XDK D3D device fields — verified against JSRF bytes (Phase 0 V4, 2026-09-29)

From `~/src/halo-ce-universal` (Halo CE Xbox, XDK ~3911, a matching decompilation; `libs/d3d8` is
GPL-3.0 and RXDK-derived, so **facts only, no code**): `libs/d3d8/device_layout.h:180-182` and
`d3dbase.cpp:142` (`SetVerticalBlankCallback` stores `g_pDevice->m_Miniport.m_pVerticalBlankCallback`).
Matched to JSRF offsets by role, anchored on measurements already in `docs/jsrf-kick-get-contract.md`,
`jsrf-callback-reentry-contract.md` and §5; the miniport context starts at device `+0x2268`.

V4 disassembled each site in the original XBE. The offsets and the five names are now **CONFIRMED**;
the inference that `sub_00038530` writes this slot is **REFUTED** (see below the table).

| JSRF device offset | XDK field (Halo name) | Verdict | Bytes |
|---|---|---|---|
| `+0x242C` | `m_pVerticalBlankCallback` | **CONFIRMED** | `0x0018CE30: mov eax,[esp+4]; mov ecx,[0x19DCE0]; mov [ecx+0x242C],eax; ret 4` — the only store to `+0x242C` in the whole generated tree |
| `+0x2430` | `m_VerticalBlankEvent` (KEVENT) | **CONFIRMED** | `0x0018CE67: add eax,0x2430; push eax; call [0x1C4018]` — `[0x1C4018]` is the ordinal-159 `KeWaitForSingleObject` thunk (`0x8000009F`), and the run log shows `ordinal 159 (slot 46) … ret=0x0018CE73` |
| `+0x2434` | that event's `Header.SignalState` | **CONFIRMED** | `0x0018CE5B: mov dword ptr [eax+0x2434],0` — cleared before the wait, as `KeClearEvent` inline does |
| `+0x2440` | `m_BusyBlockEvent` | **REOPENED — INFERRED-LOCATED** (was REFUTED; see below) | `0x00191497: mov [edi+0x2444],ebp` (SignalState) and `0x00191501: lea esi,[edi+0x2440]` → `0x00191519: call [0x1c4018]` (the same ordinal-159 `KeWaitForSingleObject` thunk as `+0x2430`). `sub_0018CE80` is a table copy, not a second event — that part stands |

Role names, each **CONFIRMED** by its own bytes: `0x0018CE30` `SetVerticalBlankCallback` (19 bytes,
stores its first argument at `+0x242C` and returns `ret 4`); `0x0018CE50` `BlockUntilVerticalBlank`
(clears `+0x2434`, then waits on `+0x2430` through the ordinal-159 thunk); `0x00193D90`
`CMiniport::VBlank` (`sub esp,0x14`; `call 0x193C40` = `rdtsc`; reads/writes `[esi+0x208]`,
`[esi+0x20C]`, `[esi+0x1F4]` — a frame counter and time base); `0x00194210` `ServiceGrInterrupt`
(`mov [esi+0x400720],0` clears PGRAPH `0x400720`; reads `0x400100`, `0x400704` and `0x400108`, i.e.
PGRAPH INTR/TRAPPED_ADDR/TRAPPED_DATA); `0x00193F70` `SoftwareMethod` (`sub esp,0x14`; `lea
edx,[ecx-1]; jmp dword ptr [edx*4+0x1941B4]` — a method-index switch, and the table at `0x001941B4`
holds eight in-module VAs `0x00193F87`, `0x0019401C`, `0x0019412A`, `0x0019412A`, `0x00194144`,
`0x0019415D`, `0x0019415D`, `0x00194173`).

**`+0x2440` REOPENED (2026-09-30, V4 re-verification).** The row above previously read **REFUTED**, on the
criterion "no store to `+0x2440` exists anywhere in the generated tree". That criterion does not
discriminate: the field it sits beside, `+0x2430`, **also** has no store anywhere in the tree, and V4
CONFIRMED it on its *wait* site. Re-run mechanically over both spellings the lifter emits (hex and signed
decimal, per `AGENTS.md`):

| offset | stores (hex form) | stores (dec form) |
|---|---|---|
| `+0x242C` | 1 | 0 |
| `+0x2430` | **0** | 0 |
| `+0x2434` | 1 | 0 |
| `+0x2440` | **0** | 0 |
| `+0x2444` | 1 | 0 |

Absence of a store therefore cannot separate a real event field from a non-field, and `+0x2440` has the
same evidence *shape* as the confirmed `+0x2430`:

- **It is waited on through the same thunk.** `0x00191501 lea esi,[edi+0x2440]` … `0x00191519 call edi`
  where `0x00191507 mov edi,[0x1C4018]` — `[0x1C4018]` is the ordinal-159 `KeWaitForSingleObject` thunk
  (`0x8000009F`) that `+0x2430` also uses.
- **Its base is `g_Device`.** At `0x00191446 mov edi,[0x19DCE0]`, `edi` is the device pointer; no
  instruction writes `edi` between there and the only branch to the wait (`0x001914CB jae 0x191501` —
  `0x001914E5` and `0x001914FC` are on the `0x001914E2`/`0x001914FC` paths that `ret` at `0x001914FE`
  without reaching `0x00191501`). So the wait target is `g_Device+0x2440`.
- **It has a SignalState field at `+4`.** `0x00191497 mov [edi+0x2444],ebp`, exactly as the confirmed
  event has `mov [eax+0x2434],0`. The two offsets are `0x10` apart, one `KEVENT`
  (`DISPATCHER_HEADER`) apart.

**Verdict: INFERRED-LOCATED, not CONFIRMED.** The *offset and its wait* are measured; the *name*
`m_BusyBlockEvent` still comes from the Halo XDK layout by role-matching, which is the same kind of
inference V4 accepted for the three CONFIRMED rows. It is not raised to CONFIRMED here because the
role-match itself has not been re-derived from the reference this session, and the `+0x2440` wait site
has **not been observed executing**: `ordinal 159 … ret=0x0019151B` appears **0** times in all four V3
runs and in the 2026-09-30 verification run, while the `+0x2430` wait (`ret=0x0018CE73`) appears 124–140
times. An unexecuted path is not a refuted one, but it is also not a measurement of the field.

*Method note for W2/W10.* This is the second recorded case of a refutation that came from the **absence**
of a witness rather than from a positive measurement — `docs/jsrf-run-profiles.md` §"Evidence rule" already
forbids that ("Absence of a witness is never a positive attribution"), and the criterion here failed for a
sharper reason: the witness it demanded is not expected to exist for this class of field at all, because a
KEVENT is armed by *whoever signals it* through a pointer, not by a literal-offset store. The same
V4 pass that refuted `+0x2440` for having no store had CONFIRMED `+0x2430` — which has no store either.

**`sub_00038530` does NOT write `+0x242C` — REFUTED.** V4 asked whether the object that function
initialises overlaps `g_Device`. It does not reach the slot: the A2h ARM record
(`20260928-121142-929-a2h-attrib-exp2-3b`: `armed base=0019B200 slot=0019D62C page=0019D000 off=62C
aliases=29/29`) gives the watched object base as `0x0019B200`, and `0x0019B200 + 0x242C = 0x0019D62C`
exactly. That base **is** `g_Device` (`0x0019B200`–`0x0019DCE0`, `+0x2AE0`), and the object's extent
ends at `0x0019DCE0` — so it does **not** overlap the page holding `0x001C4064` (`0x001C4000`–`0x001C4FFF`).
The `+0x242C` store the A2h packet attributed to `sub_00038530` is `MEM32(ecx + 0x242C) = eax` at
`recomp_0004.c:54015`, which is inside **`sub_0018CE30`** — the legitimate installer — not `sub_00038530`.
The current `sub_00038530` body stores only up to `+0xA4` directly plus indexed stores through pointers
read from `[edi+0x50/0x14/0x28/0x3C/0x64/0x78/0xA4]`, so the A2h write reached the slot through one of
those pointers, not by a literal offset. **The `0x001D5078` write is therefore a data-driven store into
`g_Device`, not evidence that a JSRF object overlaps `g_Device`.** The A2h packet's own coherence verdict
for that run was already `MISMATCH => UNKNOWN`.

**The A2h slot is not the terminal event** (see §5): the terminal read is a slot of the kernel thunk
table in `.rdata`, which is overwritten wholesale, while the A2h watch sat on `g_Device` at
`0x0019D62C`. The two addresses are unrelated.

### Corrections to earlier records

- **The "`VCRUNTIME140` memset/memmove" writer (§5) is most likely guest code.** The lifter lowers
  guest `rep stos`/`rep movs` to host `memset`/`memcpy` (`xboxrecomp/tools/recomp/lifter.py`, the rep
  string paths; `grep -o` over the committed tree finds 95 `memset(` and 464 `memcpy(` in
  `src/recomp/gen/recomp_000*.c`, 114 and 652 with `recovered.c`), and `src/jsrf_crt.c` routes
  guest memmove to host `memmove`. The successor's specified "`VCRUNTIME140` → HOST" split would
  misattribute it; attribution must use the native return address into the recompiled module. This
  changes an Advisor-specified input, so it goes back to the Advisor before the successor is designed.
- **`0x1B00/0x1B04/0x1B08/0x1BC8/0x1BCC` are texture-stage methods** (`xboxrecomp/src/nv2a/nv2a_regs.h`
  NV097 texture block), not "the programmable-vertex path" as `docs/jsrf-operating-history.md` (2026-09-22)
  says. Vertex-program evidence is `0x1E94` and the `0x0B80+` constants.
- **The game has implicit function declarations of its own** (found by the clang cross-build; MSVC
  only warns, C4013): `xbox_inb`/`xbox_outb` in `src/recomp/gen/recomp_0002.c`, `_0004.c`, `_0005.c`
  (harmless — `SET_LO8` masks the assumed `int` — and fixed in the toolkit template for the next
  regeneration), `recomp_dispatch_init` (`src/main.c:634`), `recomp_delta_allowed`
  (`src/diagnostics.c:324`), `dr_tid_exited` (`tools/harness/collect.c:732`, a static defined after
  use), `sub_00193D10`/`sub_00196C83` (`tests/test_recovery_11c1.c`). The toolkit now builds with
  `/we4013`; the game must not inherit it until these are declared.

### Toolkit changes `db96e30..2a349c8`

| Commit | Change | Strict-path effect |
|---|---|---|
| `c4adb9b` | Merge BearddOddity `pr-a-small-fixes` (split of upstream PR #128, rebased on v0.12.0): 64-bit per-thread kernel call counter, pseudo-handle sign extension, `STATUS_CONFLICTING_ADDRESSES` → `ERROR_INVALID_ADDRESS`, OHCI DATA UNDERRUN | handle/USB/log fixes; A2h counters stay `RECOMP_TLS`, printed digits unchanged |
| `a253876` | Merge BearddOddity `pr-b-pushbuffer-executor`: CPU executor (FFP, vertex programs, lighting), DMA-engine walker | none: runs only in the GPU-ack thread body (`RECOMP_GPU_ACK≠0`) under `RECOMP_PB_EXEC`; `xbox_Nv2aAckBusyBits` checks the ack gate itself |
| `a9188d9` | Toolkit C builds with `/we4013` / `-Werror=implicit-function-declaration`; the two existing violations declared (A2h alias census, DSP P-write watch) | none (same code generated) |
| `cde1ccb` | Revan67: a DPC queued during a drain runs on the next timer pass | a self-requeueing DPC can no longer spin the timer thread |
| `4b4a62d` | All 34 kernel DATA exports (nxdk `xboxkrnl.exe.def`, Cxbx-Reloaded `KernelThunk.cpp`) patch to backed data; 88, 89, 102, 120, 154, 162, 240, 245, 249, 321 were function thunks. Their old storage overlapped the IDE channel object at `0x500` | **thunk values change** for those ten ordinals if imported (each printed at init); 277/`[0x1C4064]` and KeTickCount unchanged; 120/154 are set once and not advanced |
| `1534c4a` | `NtCreateFile`: host `ERROR_FILE_EXISTS` → `STATUS_OBJECT_NAME_COLLISION` | that one failure code |
| `305efd1` | Guest concurrency meter, `RECOMP_GUEST_METER=1` | none when unset (observation only) |
| `093174c`, `787b7d7`, `1d85934` | Heap blocks split on reuse; `MmFreeContiguousMemory` frees in the contiguous arena (it called `xbox_HeapFree` on an arena address); reservations tracked as regions with NT semantics — a reserve hint is honoured or refused with `STATUS_CONFLICTING_ADDRESSES`, a commit must land in a region (zero-filled after decommit), `NtFreeVirtualMemory` reads the 32-bit guest values (it dereferenced them as 8-byte host values and failed every call) and really decommits/releases. `[KMEM] summary` counters | **yes** — allocation addresses and failure codes change; `RECOMP_KMEM_LEGACY=1` restores the old behaviour exactly for A/B |
| `123ca65` | One SRW lock around every heap entry point | removes an unsynchronised shared table |
| `b6cea28`, `e8972a1` | HeatXD: 8/16-bit `mul`/`imul`/`div`/`idiv` in their narrow forms (4 JSRF sites, e.g. `sub_000307F8`) | at next regeneration |
| `69842cf`, `79a0070` | NoRain211: jump table read from slot 1 when slot 0 is unusable, not when pointers precede slot 0 | at next regeneration |
| `caeee80` | `movsx` from `bp`/`sp` (1 JSRF site, `0x0005D58D`) | at next regeneration |
| `73974ab` | Indirect calls through alias entries counted (`g_recomp_alias_icall_count`), first 8 printed as `[ALIAS-ICALL]` | at next regeneration; `recomp_lookup(alias)` then returns a counting wrapper, so `src/main.c`'s A2h start set grows by up to 135 (cap 16384) |
| `ca4257c` | A join whose predecessors disagree computes its condition on each edge; leftover `_flags` reads are reported (`FLAGS:`) and `--strict-flags` fails on them. Of 17 annotated sites in the generated tree, 8 were live bugs (always-false branches: `loc_000153A9` in `sub_00015130`, five in `sub_00130FD0`, `sub_000A0F10`, `sub_001C0B86`); 9 remain and are reported (flags live into the function, or data decoded as code). The 144 bare `if (!_flags)` are REP-compare loops and correct | at next regeneration; previously dead branches become live |
| `f61a0af` | Runtime template declares `xbox_in*`/`xbox_out*` | at next regeneration |
| `822c9de` | The D3D11 translator keys on the real NV097 method numbers (`nv2a_regs.h`; eight local constants were wrong) and reads a subchannel by its bound class | none: JSRF never reaches that translator |
| `6864f1f`, `30d7322`, `aa650ca` | **Dormant unless `RECOMP_NV2A_ACTIONS` is exactly `1`:** semaphore release (`0x1A4` handle via RAMHT, `0x1D6C` offset, `0x1D70` written only if the stream commits); a non-zero NOP with PGRAPH `DEBUG_3` bit 20 set traps (TRAPPED_ADDR/DATA, NSOURCE, INTR ERROR, FIFO access off, vector 3) and holds the walk until the guest both clears ERROR and re-enables `0x400720`, GET left past the NOP as in xemu; `0x1D8C`/`0x1D90` land in `0x401A88`/`0x40186C`; `FLIP_STALL` holds while the flip READ index equals WRITE, released only by the guest's `0x40071C` write | none when unset — a differential driver fed 6000 random streams plus ISR register writes through the old (`a9188d9`) and new cores and every state field matched; the synthetic fence mirror writing the semaphore's word is logged and counted |
| `2a349c8` | Admission evidence: `docs/technical/nv2a-action-methods.md` | — |

**Admission status of the NV2A action methods** (the toolkit doc has the sources: xemu `f9b1403`,
envytools `f102b82`, nxdk pbkit `58427c0`, Linux v6.6 nouveau, each with lines and SHA-256; JSRF's ISR and
the XDK reconstruction are corroboration only). Against §"Admission criteria" of
`docs/jsrf-run-profiles.md`: criterion 1 is unmet for all three by design (they are behind a switch until
admitted). **Semaphore release fails criterion 4 as written**: the release tells D3D that the rendering
before it finished, and the strict model does not render, so it can stand in for work the guest then
relies on; only its DMA-object lookup has two sources. **NOP trap** meets criteria 2–4 except two points
where the sources disagree (whether the data check is gated by `DEBUG_3` bit 20 — a non-zero NOP with the
bit clear stops as `software_method_unchecked` rather than guessing — and the exact NSOURCE bit).
**FLIP_STALL** meets 3–4; its counters have two sources, its stall condition only xemu. None of it can run
end to end in JSRF until `0x00193F70` (`SoftwareMethod`) is recovered.
