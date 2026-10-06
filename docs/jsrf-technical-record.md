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

### Boot mailbox on 2026-10-04 (measured, not a new acceptance)

The pending word is still `MEM32(0x001BA858)+0x810`. On the runs below that anchor was
`0x803BC000`, so the word was `0x803BC810`, not the earlier run's `0x803C0810`. The SGE table
was at `0x803C8000` and entry 0 was physical `0x003BC000`. Contiguous high-water on the stuck
run was `0x56C000`, above both offsets, so `apu_translate` case 2 accepts them. No
`[GPDMA] unmapped` line was logged.

`20261004-174418-621-f6-fatal-caller` (1200 s, exploratory, `diagnostic_deadline`) stopped with
that word still 3. The main guest thread was in `sub_001A1769` at `loc_001A18D0`, holding
critical section `0x1BA050` (acquired by `0x19E438`, which is `RtlEnterCriticalSection` ordinal
277, and released only after the spin returns). The APU object is heap and was not in the
minidump, so GPRST was not readable. This run never opened a framebuffer window.

`20261004-182724-545-f7-apuwait` (183 s, same exploratory environment, plus a capped host log
on the APU frame thread, toolkit `712f70d`) did not stick. The first sample already had `GPRST=3`,
`gp.realtime=1`, `GPSADDR=0x003C8000`, SGE entry 0 `0x003BC000` → `0x803BC000`, and the word
0. The word was 3 at frame-thread call 225 and 0 at call 226, after the GP had run 84326
cycles on that frame. Boot then opened the framebuffer window, opened `Beat.bin`, touched
`JSRF_CACHE_COMPLETE.CMP`, and the GPU log reached 655 flips. At the deadline the main thread
was in `nv2a_submit_pending`, not in the spin.

What that does and does not say: when GPRST is already 3 and the frame thread is running the
GP, this mailbox clears within one audio frame and the guest leaves the spin. It does not say
why f6 stayed at 3 for 1200 s. It does not re-establish the A4b2 strict `GP_CLEAR` latch, and
`RECOMP_DSP_ACK` was not set. The log does not write guest memory.

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

**IMPLEMENTED 2026-09-30 in toolkit `1f9309a`.** The method is admitted through the measured, generated
inventory rather than by loosening the unknown-method policy: the walk still rejects anything absent
from `src/nv2a/nv2a_method_table.c`, and `0x1720` is now in it because a real submission contained it.
Seven NV097 methods were admitted, every one measured — `0x1720`, `0x172C`, `0x1730`, `0x1744` (the
vertex-data-array-offset slots the title uses) and `0x1800`, `0x1804`, `0x1808` (PGRAPH antialiasing,
blend, blend-colour). It is deliberately **not** the whole `0x1720..0x175C` array: the array is
indexed, so a blanket range would admit slots the title never submits; only the four measured slots are
admitted, and the first unmeasured slot (`0x1724`) still rejects. Admitted methods flow through the
existing state path (`pgraph_method` stores them in `PGRAPHState.methods`); **no execution semantics
were invented** for the method.

**Two generator defects had to be fixed first**, both in `scripts/gen-nv2a-method-inventory.py`, and
each is why the method was invisible to the tool that builds the table:

- its decode **budget was 4096 words**, but `0x1720` first appears at word **8124** of the F4 ring's
  72,353 — so a decode that reported "reached PUT" for the older ring could never reach the method the
  walk was stuck on;
- it derived the table from **one** ring. The F4 ring alone would have **dropped 148 methods** the
  older ring contributes, because the two rings overlap only partly. The table is now the **union** of
  the rings named on the command line, and every entry is still something a real submission contained.

**Focused tests** (five new functions in the toolkit's `tests/nv2a_actions_test.c`): the measured
command is accepted and staged with GET advancing past it; the indexed-range control (four measured
slots accepted, `0x1724` still rejected); unrelated unknown methods still rejected with GET unmoved;
the same method on an `NV_MEMCPY`-bound subchannel and on an unbound subchannel still rejected; and a
stream through the `0x1720` block commits. Verified both ways — all pass with the fix, and **15
failures without it**, with GET pinned at `0x1000` and `unsupported_method` — so the tests exercise the
change rather than merely coexisting with it. No existing test was weakened: the `0x0104` rejection
case in `test_semaphore_written_only_on_commit` still uses a method absent from the table.

**What this does NOT establish: that frames exist.** Whether the walk now advances beyond GET `0x8EF0`,
and what the next stop or first draw/flip event is, is the next measurement — and it is a separate
step, not a conclusion of this one.

**MEASURED 2026-09-30 (smoke run `20261001-004608-186-f4-smoke-1720-admitted`): the blocker MOVED, and
GET did NOT advance.** The `unsupported_method` diagnostic is **gone entirely** (0 occurrences, where
the previous run had 54), so the `0x1720` admission works as intended. But the walk now stops with a
*different* diagnostic at the *same* address:

- `[PFIFO] submit #12 diag=sink_capacity get=00008EF0 put=0000A440`, and all 52 submissions after it
  report the same `get=00008EF0` while `put` advances to `0x47A84`. Max GET is still `0x00008EF0`.
- **Cause, decoded from the ring:** the failing submission's window is `0x8EF0..0xA440` (1364 words,
  259 packets, 0 jump words), and it stages **1109 methods**. The sink is a per-submission staging
  array of **1024** entries (`nv2a_core.c`, `sink[1024]`), reset at the start of each submission
  (`nv2a_core.c:1468`), so the cap is hit **within one submission** — the walk overflows at packet
  #239, having staged 1025. This is not accumulation across submissions.
- Integrity is unchanged and clean: 0 invalid ICALLs, 0 exceptions, 0 ABI failures, 0 `[UNIMPL]`.
  `FLIP`, `present` and `FB_DUMP` are still all 0 — no frames, as expected while the walk is stopped.
- **A stale comment at `nv2a_core.c:1461-1467` says "its 256 cap"** while the array and the test are
  1024. Worth correcting when that code is next touched; it is a comment, not behaviour.

**So the next blocker is a capacity limit, not a missing method.** It is deliberately **not** fixed in
this pass: the instruction was to record the new measured state and stop so the next packet can be
reviewed. What a fix would have to decide — and what this measurement does not decide — is whether the
right answer is a larger sink, a sink that drains as it fills, or whether staging 1109 methods in one
submission means the walk should be committing incrementally. That is a design question about what the
sink is *for*, not a constant to raise.

**F4 capacity ruling, 2026-10-01 (owner-directed chore; implemented and accepted; F4 MET exploratory).** The Persistent
Advisor chose A′: size both `staged[]` and `sink[]` to the existing 4096-word budget, place staging in
PFIFO state rather than the stack, and preserve all-or-nothing submission admission and the other
rejection rules. This is cheapest within the existing architecture, not hardware-faithful incremental
PFIFO→PGRAPH dispatch. L40 records the deliberate atomicity approximation; it creates no new modeled
hardware cause. The exact response, observed/inferred basis, focused regressions, next-stop procedure
and reversal conditions are in `docs/reviews/rulings/f4-submission-capacity.md`. Advisor child
`4e6d87e1-f748-48b3-a0a4-a6e5728bfeee`, `claude/claude-opus-5-5` @ `high`, continuity PASS.

**Focused regression measured.** Game `tests/test_nv2a_contract.c` replaces only the old
`USER sink capacity` rejection with acceptance of 1109 methods over 555 packets (1664 words;
same method count, not the real packet shape) and a single count-1025 packet (1026 words).
Assertions cover acceptance, method count, GET=PUT and one successful commit; the multi-packet case
also checks packet count. With only the worker's toolkit patch reversed, those new cases produce
9 assertion failures (exit 1); after restoring it, all 344 register/clock contracts pass (exit 0).
Toolkit `nv2a_actions_test` reports all checks passed. Exact commands are the Release CMake targets
`jsrf_nv2a_test` and `nv2a_actions_test`, followed by their binaries; no guest run is implied by these
fixtures. The 2048-word fixture cannot represent a >4096-word budget stop, so the existing packet-
budget rejection remains and static assertions pin both method capacities to the word budget.
Toolkit commit `e8a6e03`. Full validation measured: toolkit Release build exit 0, CTest 5/5,
30 lifter unittests; game `just check` exit 0 and `just test` build plus 29/29 CTest.
`xbox_guest_meter` passed both suites.

**Bounded smoke measured:** `20261001-020407-358-f4-capacity-fix-smoke` on game `3be0adb`
(archived record-only row-98 citation diff) / toolkit `e8a6e03`; Session directly read result and
metadata. Requested 45 s, actual 48.336403 s, `diagnostic_deadline`, exit 3, dump/profile/checkpoints
valid. Exploratory default-on GPU_ACK plus APU_TRAP, PB_EXEC, FB_WINDOW and log budget 100000.
Active ledger L14–L18, L20–L25, L39, L40; L19 dormant. All 64 printed submissions (#0–63) report
OK and GET=PUT through `0x47A84`; frozen GET=PUT `0x16648`. Ring wrap/continued progress beyond
logged submission 63 is INFERRED, not a complete trace. No sink/budget diagnostic or 32-address
budget trace; no observed rejected submission. **Draw/indices/triangles/pixels/clear, clip/surface
and flip/present are UNKNOWN for this capture, not zero** — corrected by the Advisor fault-diagnosis
ruling (2026-10-01): the only `[GPU]` executor report lines in the log are 34–43, printed during
`xbox_MemoryLayoutInit` before the guest ran (line 39 is the `memory_ready` checkpoint), and the
runtime report fires only with `RECOMP_NV2A_TRACE` or VERBOSE, neither of which was set; `submit`
printing also stops at #63. The capacity stop is gone in exploratory scope; **F4 frames are
not satisfied and the strict horizon did not move**. Mapping gate: 1 match, 0 content mismatch.
Advisor independently inspected artifacts and ruled **W14 CONTINUE** until 11:00 UTC or two more
smokes, whichever comes first, then another ceiling call unless a finding is accepted or a strict
horizon moves. **Initial next assignment (superseded below):** a read-only worker answers deadline
main/render wait sites first; GPU-specific flip/interrupt survey only if those waits point at GPU;
no second smoke or fix authorized at that point.

**Post-smoke premise correction (read-only archive, no rerun).** Main tid 6984 was live
at the deadline: it made kernel calls through #30073, at log line 111298 (Session direct
read of `jsrf_run.log:111290–111300`), and that call returned. Advisor independently grouped
last-4000-line kernel calls by ordinal/return/tid and noted recurring helpers. Worker confirmed
277/294 as `RtlEnterCriticalSection`/`RtlLeaveCriticalSection` from the toolkit table. Original
XBE mapping places the hot helpers `0x0019E438`, `0x0019F266`, `0x001A0480`, the lock
`0x001BA050`, and candidate global `0x001BA04C` in **DSOUND**, not renderer setup.
The mapped direct absolute write `mov [0x001BA04C],1` is at **`0x001A2317`** (file `0x18FFD7`),
not the worker's withdrawn approximate address. Global `0x001BA04C` reads 0 at capture; this
neither proves the writer never ran nor excludes alias/reset writes. Its semantics and causal
relation to no frames are **not established**. Global `0x001BA6F0` has no XBE file backing but is
valid guest RAM, holding live heap pointer `0x01120004`; absence of file backing does not invalidate
its runtime state. Named helper ranges have no local backedge, but their enclosing loop is unread;
no inference about frame/time-bounded outer progress follows. Two other threads wait on DSOUND
event `0x0019D630`; signaler unknown. No GPU wait observed for main, not a general exclusion.
Final queue history is insufficient: the 64-word preview supports neither "no flip was ever
submitted" nor "an unacted flip is excluded". Neither claim is made. Artifact:
`logs/workers/f4-drained-no-frames-brief.md` (corrected).

**Fault-diagnosis ruling returned (Advisor, 2026-10-01) — this supersedes the initial read-only
assignment above; the ruling text is recorded verbatim in `docs/reviews/rulings/f4-submission-capacity.md`.**
The guest looks like a **running game loop**, not a stalled one. Observed: main tid 6984 sits in game
code `sub_00161C20 → 161A90 → 161920 → 1669A0 →` XAPILIB `XGetDevices` (`0x001BD5FF`, +0xFF) polling
input — ordinary per-frame activity — with `DirectSoundDoWork` (`0x0019F260`) and the DSOUND critical
section (`0x0019E438` = `DirectSoundEnterCriticalSection`, CS `0x001BA050`) repeating. `0x001BA04C` is
DSOUND library state, **not** a render-arming flag (48 references, 43 `cmp …,0`, one write
`mov [0x001BA04C],1` at `0x001A2317` inside a DSOUND method calling `0x1A1C8A`): **dropped as the F4
lead**. Thread 59696 is in D3D `BlockUntilVerticalBlank` (ret `0x0018CE73`), and that return site
appears **1,622** times, so vblank delivery is INFERRED to work and the interrupt-delivery hypothesis
drops to **low**. INFERRED from raw stack words (not unwound): D3D state-call addresses (`SetStateUP`,
`UpdateProjectionViewportTransform`, `SetScissors`) lie in main's live stack region, suggesting a
render path. **The open question is only whether it renders and presents, and this capture cannot
answer it.**

**Observation run `20261001-023335-656-f4-observation-60s` (2026-10-01): the instrumentation was
inert — architecture A.** Run with `RECOMP_NV2A_TRACE=1` and `RECOMP_PB_SCAN=1` on the smoke's
exploratory set, game `c01e292` / toolkit `e8a6e03`, `exe_sha256 1ecf8363…` (byte-identical to the
capacity smoke's binary): 62.580312 s `diagnostic_deadline`, exit 3, dump/checkpoints/
`gpu_report_ok` good, 21 threads, 165 named frames. The run produced **no post-guest render output**:
zero `[PB]` lines, and no line after `guest_entry` (log line 75) matching draw/flip/pixel/surface.
**Root cause, MEASURED in source (Advisor):** `src/main.c:529` installs the MMIO state owner →
`src/nv2a/nv2a_mmio_hook.c:681` `xbox_Nv2aClaimRegisterOwner()` →
`src/kernel/xbox_memory_layout.c:173-179` clears `g_nv2a_ack_enabled`; the legacy GPU body
`:1048-1177` (busy-bit acks, DMA_GET mirroring, pushbuffer scan, executor call, periodic report) is
inside that enabled check and therefore never runs. `RECOMP_PB_EXEC`/`RECOMP_PB_SCAN`/
`RECOMP_NV2A_TRACE` are **set but inert** on this build — a **new instrument defect**, not a guest
finding. **MEASURED:** 64 `[PFIFO] submit` lines all `diag=ok`, GET=PUT through `0x47A84`, so
committed methods are real; the six `[GPU]` lines are all at log 32-38, before `guest_entry`.
**INFERRED, not measured:** that the executor is never called under the owner, and hence that no
render work executes — a code-path reachability inference. **The run's runtime counters are UNKNOWN**,
and must not be recorded as measured zero. Stop = `metadata.json` `started_utc`
`2026-10-01T09:33:36.447445+00:00` + `result.json` `duration_seconds` 62.580312 =
**2026-10-01 09:34:39.027757**.

**Ruling A (Advisor) and the approved design preflight — FINAL GO; Architecture A IMPLEMENTED AND
ACCEPTED.** Toolkit `a71f9374ddb2a6685b790493855c835842228212` (9 files, +329/−10): the Advisor read
the diff itself and ruled **ACCEPT/GO** with **final validation green** — core 5/5 in 2.34 s, 30
lifter unittests, game 29/29 CTest in 28.81 s, all checks pass, conformant to the approved design.
The **actual `FrameCounter` audit found 0 host registrants** (evidence cite "Advisor grep 2026-10-01":
0 game host registrations, definition/header only in the toolkit), which differs from the worker's
backend-only audit; that worker item stays pending and **does not block**. **Runtime is still pending
and no frames claim is made**; the clean pair is committed and pushed **before** the smoke. The fix
must reach the owner's
NV097→`PB_EXEC` path with **no second walk and no second GET**; a rejection must stay **atomic**
(reject without executing); fixtures must pin **clear/flip counts**; and the next run must show a
**post-guest periodic `[GPU]` report** as positive proof the path is live. W14 is extended **until
A's first smoke plus one diagnostic**. **No more inert reruns.** The final order is **capture per
entry class → bindings → `action_commit` → consumer (ordered all committed classes; the kernel
executes the NV097 subset) → last method → GET**.
**Interface refinement (Advisor APPROVED):** the seam takes **four arguments
`(subch, class_id, method, param)`** and **all committed entries reach the core callback**; the
**kernel wrapper** filters to NV097 and keeps the skip count — this **replaces the earlier
3-argument NV097-only-core-consumer** shape, since a policy-free core is the better factoring (the
core sees what the executor sees, not a pre-filtered subset). Order and atomicity are unchanged
(callback still after `action_commit`, inside the ok path); tests must show a **non-NV097 entry
leaving the `EXEC` counters unchanged** while the **wrapper's skip count increments**, the core's
local-callback tests must check the **correct class including across a rebind**, and the **skip count
is read for the report under the same PFIFO lock**. Not a new shortcut, and **no new ledger class
entry**. The runtime seam is a **core static function plus a setter** — no env var, no weak symbol,
and the point of "no `extern`" is that the **core holds no `extern` reference to the executor** while
the **kernel does register the seam**; registered **before the guest starts**, with `PB_EXEC`
presence as the only trigger. Integration uses the **existing HAL target links across the whole
toolkit** (the fallback is a dedicated real-exec minimal stub target only if the HAL cannot be a
fixture); the earlier "A2 new targets" proposal is **superseded**. Standalone core tests cover order,
atomicity and rebind with a local callback. The owner lock uses a **free getter on the existing
active flag** (not a submission snapshot, not lock coupling), and the legacy guard **logs once and
skips `EXEC`**. The **report runs only under the consumer lock at a 10 s cadence**. Flip accounting
adds a **NEW `flip_stalls` counter**: `0x0130` is a completed swap that calls `FrameCounterFlip` but
**does not increment the existing `s_gpu.flips`**, while **`0x012C` is the `s_gpu.flips++`**. A
**game-side registrant audit is mandatory**, with **no blocking callbacks under the lock**. **L18's
edit lands at the coordinated cross-repository checkpoint** — the toolkit code commit plus the game
ledger record citing that toolkit SHA, pushed **toolkit first** (Advisor-ACKed binding: "same commit"
means that same checkpoint, since the two repositories are separate and cannot share one commit
object). **Until the code is accepted, L18 keeps its old entry** — no speculative acceptance.
Recorded durably in `docs/reviews/rulings/f4-submission-capacity.md`.

**Push checkpoint (2026-10-01, capacity fix and smoke record).** Both clean and fast-forward;
public game outgoing audit 0 secret hits, no asset/lifted-code additions or blob >100 MB.
Toolkit first, then game; remote branch SHAs independently matched local HEADs after push.

```text
PUSHED_TO: origin (toolkit)
BRANCH: main
COMMIT: e8a6e03793d417106993b863fa3240a3abae0caa
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT: exit 0; 1f9309a..e8a6e03 main -> main; ls-remote equals HEAD

PUSHED_TO: origin (game)
BRANCH: master
COMMIT: d86c8418adf27c6a03521218c9069716d567596b
REMOTE_URL: https://github.com/danillogical/poison-jam.git
RESULT: exit 0; 2a7324b..d86c841 master -> master; ls-remote equals HEAD
```

**Architecture A push checkpoint (2026-10-01).** Toolkit first, then game; no force, no `upstream`;
both trees clean at the pushed commits. Gate results: toolkit max outgoing blob 281,957 B, 0 behind /
1 ahead, intended 9 source paths only; game 7 intended paths (five records, two tests), no `game/` or
asset path, max outgoing blob 91,095 B, **7-blob outgoing secret audit 0 hits, exit 0**. Both pushes
were re-verified by `git ls-remote` against local HEAD.

```text
PUSHED_TO: origin (toolkit)
BRANCH: main
COMMIT: a71f9374ddb2a6685b790493855c835842228212
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT: exit 0; e8a6e03..a71f937 main -> main; ls-remote equals HEAD

PUSHED_TO: origin (game)
BRANCH: master
COMMIT: f676b1c3f2543a46e31240c793f707699923cc45
REMOTE_URL: https://github.com/danillogical/poison-jam.git
RESULT: exit 0; c01e292..f676b1c master -> master; ls-remote equals HEAD
```

**F4 — MET, exploratory (2026-10-01; Advisor-attributed summary — the Advisor's verbatim ruling is in
the appendix of `docs/reviews/rulings/f4-submission-capacity.md`).** Frames exist: the 60 s run
`20261001-033129-276-f4-a-smoke-60s` (game `f676b1c` / toolkit `a71f937`) rendered a clean,
correctly coloured "Presented by SEGA®" logo into its draw surface — **OBSERVED from the image**, not
inferred. Draws rising across successive reports (147 → 840) plus that coherent image is the
**semantic liveness witness** the profiles doc requires; `diagnostic_deadline` alone was not. Ledger
IDs: L14, L15, L17, L18 (via the commit consumer), L20–L25, L39, L40; **L16 set but inert under the
owner**; L19 dormant. **No strict or fidelity claim**; the unhandled-method count is advisory (a
picture is correct, effects may be missing). Acceptance is carried with the **F6 milestone Review**,
not separately. **W14 reset at 10:32:32 UTC** on this first critical-path frame finding; the earlier
extension is consumed and closed. The 180 s observation run `20261001-033805-242-f5-sequence-180s`
(same pair plus `RECOMP_FB_DUMP`) is recorded as **observed** in the strict-horizon ledger and the
ruling: 25 BMPs with 2 distinct hashes in ordered blocks (8 black, then 17 SEGA); pixels are the
**guest draw surface** (`dma_resolve(drawn_offset ? drawn_offset : color_offset)`), not the window;
one separate extensionless window-buffer capture at 10:38:19 (one-shot, `frames==600`, `fb_present.c:341`,
accepted by the F5 ruling as SEGA at that instant only); counts are **lower bounds**. The 180 s run's
last report is `L205921–205927` (**clears 2943, draws 2943, flips 987, stalls 987**) with its own
`[GPU] clear #3000` at **L207880** of 209257; the **60 s** run's last report is `L115261–115267`
(**clears 840, draws 840, flips 286, stalls 286**) with `clear #900` at **L117696** of 119913 — the
two runs are **not** to be mixed. That run produced **no horizon move and no W14 change**; the F5
consult ruling that followed is below.

**F5 consult ruling (2026-10-01; Advisor-attributed summary — verbatim in the F5 appendix of
`docs/reviews/rulings/f4-submission-capacity.md`).** **The logo phase is not a stall.** The guest is
doing its **first-boot HDD cache fill**, slowly. **OBSERVED** (Advisor read the 180 s run): **360
`[PATH]` opens of `\Device\Harddisk0\Partition5\Media\…~`, 184 distinct**, steady at 20–33 per 10 s
report through the end of the log (last line 209119, `e010.bin`); the save-root cache holds
`Cache00-02.tbl`, `DmCache00-02.tbl` and **`JSRF_CACHE_COMPLETE00.CMP` written 10:39:41** (~96 s in)
with **Cache02 started 10:39:42**; payload **182 small files, 67.9 MB**; copy rate **~1.0–1.3 distinct
files/s in every recent run, with or without the executor** (48 s: 64; 60 s: 82 and 80; 180 s: 184),
so **the executor does not limit it**.
*(Population clarification, dated 2026-10-01 — the figures above are the Advisor's and are retained as
written; the 182 is its broader observation and is **not** re-designated.)* Re-measured at the **same
population**, this 180 s run's roots are **`Cache` 181/23**, **`Cache/Media` 181/22**, **tables root
7/0**; the later retry1 run shows **227/23**, **227/22**, **28/0**. The byte figure **70,021,120 across
227 files includes the 28 tables/markers**, so it is a **TOTAL Cache byte count, not a payload-only
byte count** (payload file count 199); subtract table sizes before calling anything a payload size.
**INFERRED:** the SEGA screen covers the fill, which goes in
stages (00 complete, then 01–03). **UNCERTAIN:** whether JSRF really **gates** the logo on fill
completion. The word **"stop" is REJECTED** for the frozen sample: a single frozen `VirtualQuery`
sample in `submit_read_word` is a **capture-time location, not a stop** — the guest is live; the
per-word `VirtualQuery` is a **perf lead only, not a defect**. The window claim is accepted only as
**one-shot at 10:38:19.247 (`frames==600`, `fb_present.c:341`), SEGA**, with nothing claimed after it.
**NEXT (authorized):** ONE **600 s** observation run, same clean pair and profile (the records commit
changes only the game SHA), plus `RECOMP_FB_DUMP` and **`RECOMP_FB_WINDOW_DUMP_EVERY=600`** (both
observation-only), with the **≥15 GB disk gate** checked first (each save-root is ~5.3 GB of partition
images); readout per-10 s draws/flips + BMP hash + window hash, the `JSRF_CACHE_COMPLETE*.CMP` and
`Cache0N.tbl` mtimes, and the cached-file count per interval; three decision rows as recorded in the
appendix. **A seeded cache is PREPARED, NOT AUTHORISED** — no owner decision now and no
implementation. **W14 clock unchanged at 10:32:32.** Reversals as listed in the appendix.

**600 s observation run TAKEN — `20261001-043629-961-f5-observe-600s-c4bcd2b-retry1` (2026-10-01;
exploratory; NOT a title and NOT a horizon move).** Second attempt after the invalid truncated one;
parent-owned managed job `pwsh-2573`, collected with `job_output(wait)`. Same pushed pair as authorized for
the replacement — game **`c4bcd2b9b323ff94a711cc3f89fafea72af36b4d`**, toolkit
**`a71f9374ddb2a6685b790493855c835842228212`** — fresh empty root, no seed,
`exe_sha256 7027fafad9cd706981ea2f4c0fd898935db133b09f2da99fb5c7e8559a43b303`
(unchanged: no build change). Profile **exploratory** (requested exploratory;
`RECOMP_GPU_ACK` absent, effective default enabled). `diagnostic_deadline`,
exit **3**, `dump_ok` true, 21 threads, **166 named frames**, 1 snapshot, 0 dropped,
`save_root_verified` true, `missing_checkpoints []`, `checkpoints_passed` true, `gpu_report_ok` true.
**Stop (UTC) = `metadata.started_utc` `11:36:30.743170` + `result.duration_seconds` `603.170746` =
`11:46:33.913916`** — computed from metadata, **not** the script's launch line (`11:36:29Z`).
Helper hashes (full, as the owner requested observer source SHAs): **census helper**
`55E10B51F58B59569567BA4EC540E646208576A7EEBB8940635720912F7195A9`; **window watcher**
`0801459DDF6CB35CECD2DC007C6BEA539364F1CDF363470E84FCBD35B4332EC9`; **launch script**
`73B186402E04D06A1C64B60F7C42C8BB514A08CACF54722C569A2B916D92AFB8`.
**Ledger IDs for this run:** L14, L15, **L16** (legacy ack body retired; executor feed replaced by the
consumer), L17, **L18** (owner consumer, active), L20–L25, L39, L40; **L19 dormant**. **No
strict-horizon move; W14 reset unchanged at 10:32:32** — as of **12:10 UTC the clock has elapsed
1 h 38 m**. Outer shell job reported `1` while
WRAP/result is `3` — **statuses only, no cause claimed**.

**Exact typed counters (final row, primary source `logs/workers/f5-retry1/gpu-reports-v2.csv`; v2 used
for all fields):** `clears` 10500,
`draws` 10500, `with_coordinates` 10500, `indices` 52482, `flips` 3506, `flip_stalls` 3506,
`unhandled_methods` 1178547, `distinct_unhandled` 242, `non_NV097_skipped` 14. **Counts rise across
the 61 blocks; the images are static SEGA by eye — the rising counts are not new images. Still not the
title.**

**Window vs draw-surface comparison — NOT pixel-equivalent.** The **60 near-time pairs** have
**0 equal pixel pairs**. The separate full pixel comparison of `w0000.bmp` with `f066.bmp` found
**55157 differing channel bytes out of 921600 (5.98%)**, **max channel delta 7**. This is not an
aggregate count across 60 pairs. The difference is small but real; the paired data goes to the
**Advisor**, and **no admissibility, fidelity, or transform claim is made here**.

**Tables and markers (measured):** all **19 tables have `distinct_mtimes = 1`** over 10 s samples
("not observed to change", not "written once"); only the **nine zero-byte `.CMP` markers** change
after first appearing. **Exact padding fact** — from the UTF-8 raw
`logs/workers/f5-retry1/table-prefix-test-utf8.txt`: **19/19 save-root tables are the DVD bytes
followed by zero padding rounded up to a 512-byte sector** (3786→4096, 5869→6144, 72→512, 938→1024,
688→1024; deltas [86, 275, 310, 336, 440]). Whether that padding is benign is the Advisor's call, not
asserted here. `Cache09`/`CMP09` are **absent on both sides** — an observed consistency of this
title's cache layout, **not** an expected shape and not a guest-count claim.

**Not done and not claimed:** **no new run and no seed** until the CMP/table check loop is explained
from retry1's artifacts (Advisor no-rerun ruling, verbatim in the ruling appendix). **No wrapper
argument mapping, no `IoStatus` reading, and no loop-cause interpretation is recorded** — those remain
pending the Advisor's judgment. The tiny-table byte read was a **separate read-only authorization
whose exact text is not yet recovered**; it is not presented as covered by the census ruling.

**Advisor lineage (§4.4 recovery provenance, 2026-10-01).** The **three rulings above were produced by
replacement child `c0ecc88b-756e-4256-9852-1bd8b7398735`**, and **their metadata stands unchanged**.
That child then **failed twice with no error text and no closing message**, so a **further same-route
§4.4 replacement** was spawned — child **`c623447b-19f2-4abf-83bb-bdd85719556e`**, `claude`/
`claude-opus-5-5` @ `high`, **parent-pinned**, briefed from files, **not a fallback**; its continuity
marker **`ADVISOR-RECOVERY-1210-F5`** was **ACKed in a received message**: it acknowledged the
pushed pair, the prior no-rerun rule, and reading the workflow and recovery brief. The ACK does not
self-verify route/effort; those are recorded by the spawn. No new design ruling from it is yet acted on.
That child then **completed three turns with zero turn errors but delivered no substantive answer**, so
a **further same-route §4.4 replacement**
was spawned — **current child `63c4869f-3689-41b2-89b8-f5429f8b5927`**, same route / `high`,
parent-pinned, **ACK 1 and ACK 2 both received**, and it **delivered the F5 directory-probe ruling
(A–E) and reply 3** — recorded verbatim in the ruling appendix. **Total Advisor children: 4** —
original `4e6d87e1…`, `c0ecc88b…`, `c623447b…`, current `63c4869f…` — alongside **3 workers + 1 startup
Reviewer** (no fresh Reviewer yet). The **CMP caller cross-reference** work is ongoing at the original
worker. **Observations integration remains authorized; no new run.**

**F5 D1 probe — INCONCLUSIVE (2026-10-01).** The read-only directory-context probe did **not** reach
the candidate table: `s_dir_contexts` live VA `0x00007FF708EB7780` has **0 ranges containing it** in
the dump, **0/4928** readable 8-byte slots, and the dump's **113 memory ranges** cover **0.96%** of the
exe image (`.data` 1.31%, `.text` 0.01%; `.rdata`/`.pdata`/`.rsrc`/`.reloc` absent). **Cause is NOT
proven.** Precisely: the **table's occupancy in the live process is UNKNOWN** — the capture simply
**does not include the array** — so D1 shows *absence from the dump*, **not** absence from the process.
The pre-check's "0 overlap" line is **superseded** by the coverage measurement. Raw:
`logs/workers/f5-retry1/symbol-locate-utf8.txt`, `dump-coverage-utf8.txt`.
*(Had not yet approved D2 at the time of the D1 inspection; D2 was subsequently approved — see below.)*
**Current D2 authorization is recorded below; no new run until its RED/GREEN and test gate passes.**

**F5 D2 — design ruling received and packet APPROVED (2026-10-01).** The same child
(`63c4869f…`) delivered the **F5 D2 design ruling** by `send_message` (full text verbatim in the
ruling appendix; design source `logs/workers/f5-directory-context-d2-design-verbatim.md`). It rules the
fix shape: **one release function** (`xbox_dir_context_release`) called at **both `xbox_NtClose` sites
(`kernel_file.c:316`, `:907`) before `CloseHandle`** *and* at **`bridge_NtClose`
(`kernel_bridge.c:747-749`)**, because **`bridge_NtClose` does not go through `xbox_NtClose`** — hooking
only the latter would give a **false GREEN on the guest path**. Release semantics free the slot for
every entry matching the handle; invalid/NULL/synthetic handles are a no-op; **query semantics, the
dot-directory filtering, the `FindNextFile`-failure cleanup and line `:234` are unchanged**. The
**deterministic RED** is a 64-open/65th-query test plus a 200-round churn assertion — **VERIFIED RED on
`a71f937` in both modes: 2/2 CTest tests failed, rc 8, 0.29 s**, with **64 opened / 64 queried**, the
**65th query failing with status `0x80000006`** and **churn failing at round 1 of 200** with the same
status. **Parent-read RED evidence (current):** the parent read `red-direct.log` (initial ~70 lines),
the load-bearing counts in both modes from `red-ctest.log`, and — after the worker's conversion — the
**converted raws `red-direct-raw-utf8.txt` (lines 73–82)** and **`red-bridge-raw-utf8.txt` (lines
138–149)**, which **confirm the same counts and the same `0x80000006` status** in both modes; the
**UTF-16 originals are preserved** with their raw/BOM and hash. *(Historical qualification: at the time
of the failure the raws were UTF-16 and unreadable; they are **now converted and verified**.)* A worker
**5/5-per-mode** claim is **worker-reported**; the **2/2 result is parent-verified**. The earlier
**8-run 4-fail/4-pass result is superseded and NOT an accepted RED** (`RestartScan 1` masked the
stale-context path), and **cause is NOT proven** by this RED. The bridge test uses **`NtOpenFile`
(ordinal 202), not `NtCreateFile`**; **three actual test-seam wrappers** were built — **open, query,
close** — so the **"fourth wrapper" wording is historical** (it came from the design-stage harness
discussion, where `NtCreateFile` was a possible extra) and the **`NtCreateFile` wrapper is not
required** (Advisor harness-plan ACK, verbatim in the ruling appendix; no direct-only GREEN). **Packet:
`docs/packets/f5-directory-context-close.md` — **ACCEPTED** (RED verified, GREEN verified, 300 s smoke
PASS-F5 criterion (a)): the **unnumbered `JSRF_CACHE_COMPLETE.CMP` was created** (0 B,
`CreationTimeUtc == LastWriteTimeUtc == 2026-10-01T13:27:49.9575858Z`), the **CMP loop stopped**, and
state 0's nine-probe check completed. **W14 RESET from that actual marker event: horizon
`13:27:49.9575858Z`, ceiling `17:27:49.9575858Z`, reset count 0** — the eligible event is the **marker,
not a frame**; **no title claim** (the frame is still SEGA, so criterion (c) is not met). **New stop:
F6 candidate, UNCLASSIFIED** — the earlier "`1A03` loop/poll" reading is **superseded**: the **277/294
traffic is PER-FRAME RENDER WORK** (277@19E452 = 12 × 515; 294 sites = 3 × 515 and 1 × 515; IRQL pairs
≈515; **E_FAIL paths 0 times**), i.e. **≈515 frames over ≈90 s ≈ 5.7 fps** (rate estimate only). At
capture the root stack runs `… → 13A80 → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0` with **PFB_WBC = 0**
and **GET == PUT == 0x5B50C** — a **snapshot fact, not proof of no stall at any other time**. **(A)** slow
frame/time-counted sequence vs **(B)** step gated on an event (audio/movie/thread with
`RECOMP_APU_TRAP=1`) is **not yet decided; not called a stall or hang**. **Next: read-only** —
disassemble `13A80`'s vtable dispatch from **`13CB2–13EE4`** plus **`14D090`**, then read the object's
step/timer fields from the new dump (**mapping gate already passed**); **no run, no fix, no seeding**.
**GREEN gates
(measured):** focused 2/2 in 0.19 s; toolkit CTest
**7/7 in 2.71 s**; lifter **Ran 134, OK (skipped=1)** in 14.460 s; game CTest **31/31 in 31.32 s**;
game `just check` all passed. **Advisor GREEN: APPROVED**, scope matching D2; **the untracked
`tests/dir_context_release_test.c` must be committed with the fix before any toolkit push.**
**SMOKE RULING (verbatim in the appendix): `--seconds 300`, EXPLORATORY**, identical retry1
environment (`RECOMP_APU_TRAP=1`, `RECOMP_FB_WINDOW=1`, `RECOMP_FB_WINDOW_DUMP_EVERY=600`,
`RECOMP_KERNEL_LOG_BUDGET=100000`, `RECOMP_NV2A_TRACE=1`, `RECOMP_PB_EXEC=1`, `RECOMP_PB_SCAN=1`,
**`RECOMP_GPU_ACK` absent/default**), `RECOMP_FB_DUMP` pointed at a **new** directory, fresh isolated
disposable `--save-root`, **no seed**, **no `:234` change**, Release build via `just build` with the
identity check passing, and the run on a committed tree (or the toolkit dirty state recorded — the
runner archives `toolkit.patch`). **30 s and 60 s are rejected**: retry1's first marker appeared at
**~205 s**, so they never reach F5. **Ledger IDs L14–L18, L20–L25, L39, L40; L19 dormant** (parent
verifies against the ledger). **Interpretation:** PASS-F5 (unnumbered marker created, OR ordinal-207
results no longer alternating `0x80000006` with marker creates bounded, OR new progress past the SEGA
screen); **FAIL-SAME** (nine numbered markers recreated repeatedly and ordinal 207 still returning
`0x80000006`) — stop, do not extend, bring the counts; **FAIL-DIFFERENT** (207 all `0` but markers still
loop) — bring the `25040` state evidence. Mapping gate before any guest-VA dump read. **The ordinal-207
counts are qualified per the Advisor's audit ACK: 1244 strict calls (the 1245 included the summary
line, a false positive), and 1022/539/482/1 with the window starting at `COMPLETE00` (the earlier
1021/538/482/1 was off by one at the boundary). Adjacency matching is approximate because threads
interleave, so "alternating 0/0x80000006" is supporting evidence only, not per-call proof. The 205 s
figure comes from retry1 save-root file CreationTime (host timestamps, not guest log time).** **No
W14 reset at that time**; the reset came later from the actual marker event. **No guest run until GREEN;
no seeding; no `:234` change.** Risks
recorded, not fixed here: the lazy `InitializeCriticalSection` race, and the query using `ctx` outside
the lock after lookup. **The "other `CloseHandle`-of-taken-token paths" item is a write-audit
requirement, not a standing risk:** the **worker's grep found no other such close site**. The parent independently searched
`kernel_bridge.c` for `CloseHandle` and `bridge_take_handle` and read the token-removal helper:
`bridge_NtClose` is the sole caller of `bridge_take_handle`; the other native closes belong to
thread/event paths, not owned file tokens. **D1 was inconclusive
(occupancy UNKNOWN), so no causality claim is made** — the mechanism rests on the source reading.

**Records push (actual, 2026-10-01).** Toolkit first, then game; both exit 0, trees clean, remote
equality verified by `git ls-remote`:

```text
PUSHED_TO: origin (toolkit, first)
BRANCH: main
COMMIT: a71f9374ddb2a6685b790493855c835842228212
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT: exit 0; "Everything up-to-date"; local HEAD == ls-remote

PUSHED_TO: origin (game, second)
BRANCH: master
COMMIT: d1f30e1ec43e11d55cc47a4d5de0debe20a0d114
REMOTE_URL: https://github.com/danillogical/poison-jam.git
RESULT: exit 0; c4bcd2b..d1f30e1 master -> master; local HEAD == ls-remote
```

**D2 toolkit push (actual, 2026-10-01).** The accepted D2 code, toolkit first:

```text
PUSHED_TO: origin
BRANCH: main
COMMIT: a8262014eec9cd8d720184f7f2fb7dce4652105d
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT: SUCCESS — fast-forward a71f937 -> a826201; parent job pwsh-3305 collected, rc 0;
        outgoing 5 files, 571 insertions / 1 deletion, test file included;
        5 blobs scanned, 0 hits; 0 secrets; largest object 407267 B
```

**D2 game push (actual, 2026-10-01).** The records closure, game second — this closes the D2 work:

```text
PUSHED_TO: origin
BRANCH: master
COMMIT: e7e9a4a19f50171048708ab0a2a6a0601c91da42
REMOTE_URL: https://github.com/danillogical/poison-jam.git
RESULT: SUCCESS — fast-forward d1f30e1 -> e7e9a4a; parent job pwsh-3372 collected; tree clean
        before the push; outgoing 8 DOC blobs (commits 15d8af1 and e7e9a4a);
        zero assets, zero secrets, none over 100 MB; largest object 125040 B
```

**Deferred optional test advisory.** Do not add a diagnostic-is-OK assertion after the measured
red/green runs merely for churn; reopen if a future failure of the 1109-method case fails without
naming its diagnostic. Advisor accepted this deferral after independently inspecting both diffs.

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

## 8. Disc-error timeout and the boot clock (2026-10-05)

The pending-I/O path in `0x25400` is not a 15-second timeout. Measured from the original bytes and the
runtime that serves them:

- `0x145560` is `rdtsc` into an 8-byte guest buffer (`ret 4`). The lifter emits
  `xbox_ReadTimeStampCounter()`, which scales `QueryPerformanceCounter` to `XBOX_TSC_HZ`
  (733,333,333). The constant the title compiles in is the same value: `0x145571` stores
  `0x2BB5C755` and returns 1, and the 1500 s dump holds that pair at `[0x20CC50]`
  (`check-dump-mapping.py` matched `.text` at `0x11000`).
- `0x6E910` (`ret 0x14`) subtracts two TSC samples, multiplies by `0xF4240` (1,000,000) via
  `0x17CA70` (`_allmul`), and divides by the 64-bit frequency at `[ecx+8]` via `0x17C9C0`
  (`_alldiv`). Callers pass `ecx = 0x20CC48`. The quotient is microseconds of wall time when the
  scaler matches that frequency.
- `0x25400` takes the fatal call at `0x255AD` only when `[esi+0x64] == 0x103` (`STATUS_PENDING`,
  written by `0x146078` into the overlapped block) and that quotient's high half is positive or its
  low half is at least `0xE4E1C0` (240,000,000). The start sample is `[esi+0x188]` / `[esi+0x18C]`,
  not the `+0x190` / `+0x194` pair `0x25310` uses. The same 240,000,000 threshold is what `0x25310`
  compares before its tail jump to `0x6F730`. The path string lives at `[esi+0x78]` (the copy starts
  with `'Z'`) and a `~` from `[0x1C4DF0]` is appended for the side file.

Which of the four `0x6F730` entries fires at the ~950 s marker is not established by this section.
The ~2 updates/s logo hold is not this scaler running slow: the scaler is what makes the 240 s
threshold 240 s of wall time. `RECOMP_PB_EXEC`'s `FLIP_STALL` returns as soon as it is asked
(`nv2a_pb_exec.c`), and the vblank thread targets 60 Hz unless the mode timing is 40–240 Hz, so
neither clock is a 2 Hz source.

## 9. The alias-fold misdispatch class: wrong return value → corrupt loop bound → wild write (2026-10-05)

**A single misdispatched function can corrupt 363 KiB of guest memory, and the mechanism is now
fully accounted for.** This is a defect class, not a one-off, and it is why an unresolved-looking
symptom (`[ICALL] Failed to resolve VA 0xFFC00000`, a NaN bit pattern rather than an address) was
actually a *return-value* defect.

**The chain, every link observed from run `20261005-011634-413-f23-7da30`'s dump and the original
bytes:**

1. **The misdispatch.** `0x9CC40` at `0x9CCC0` does `push i; push 3; push ebp; push 1; call
   0x25700`. `0x25700` is a table dispatcher: `cmp edx,0x21` bounds the index, then
   `mov edx,[edx*4+0x1EC200]` and `call edx` with 3 stack arguments. Index 1 selects
   `[0x1EC204] = 0x00032610`. That address had **no body of its own** — the 2026-09-21 translation
   pass folded it as a `tail_jump_alias` into `sub_00033800` — so the call ran `sub_00033800`,
   which is `mov eax,1; ret 4`. That is the `[ALIAS-ICALL] target=0x00032610 owner=0x00033800`
   line, seen in f23 only.
2. **Two effects, one silent.** `ret 4` against a real `ret 0xc` leaves guest `esp` 8 bytes low
   inside `0x9CC40`; and the wrong **return value `eax = 1`** flows onward.
3. **The wrong value becomes a pointer.** At `0x9CCCF` the `1` goes into `ebp`, passes a nonzero
   test, `new(0x118)` runs, and constructor `0x15420` stores its argument — the `1` — at
   `[obj+0x38]`, where an object pointer belongs. The object is `0x034D5110`.
4. **The loop runs away.** `0x15D90` derives its loop counts from `[esi+0x38]`. With `[esi+0x38] =
   1`, `[esi+0x110]` reads `(count-1)/1` with `count = 0`, i.e. **`0xFFFFFFFF`**; the saved outer
   counter is **31,739**. The inner loop walks a 12-byte float3 array based at `0x231D40`
   (`mov edi,0x231d40` at `0x15E17`) and a paired heap buffer based at `0x034C3E40`.
5. **Why the fill is NaN.** The loop normalises each vector through `0x14C3B0` / `0x14C460`.
   Normalising a **zero** vector gives `0 · rsqrt(0) = 0 · inf`, and the SSE default quiet NaN is
   **`0xFFC00000`** — the title's own "no value" sentinel, which is why this first looked like a
   deliberate sentinel fill rather than computed garbage.
6. **The arithmetic closes exactly.** `0x231D40 + 12 × 31,739 = 0x28ED04`, which is both the
   guest's `edi` at capture and the exact end of the observed fill; and
   `0x034C3E40 + 0x5C × 31,740 = 0x0378CCD0`, the paired `ebx`. The fill is therefore **computed**,
   not a `memset`, and it is the writer's own loop rather than a wild store.

**The observed damage:** a uniform `0xFFC00000` run at `0x233ED0..0x28ED04` (93,057 of 93,069
words) that overwrote the `DOLBY` section image (`0x27E080`), live globals including `0x251D6C`,
and the thread-trampoline control block — so the trampoline's `mov eax,[0x25efb8]; test eax,eax;
je` saw non-zero and called `0xFFC00000`, producing the misleading `[ICALL] Failed to resolve`
fatal. Writing past the end of `.data` into `DOLBY` is a consequence, not a separate defect:
enforcing XBE section write-protection would only fault earlier. (Whether the retail loader
enforces section write flags at all is **inferred, not verified**; treat it as a backlog diagnostic
idea, not a fidelity defect.)

**Why it is not a regression and not a race.** The fill follows **deterministically** from the
misdispatch once the path is taken. The *path* is what varies run to run: `f24` never reached
`0x9CC40`, going idle in the `0x13F80` presenter loop and taking the game's own fatal path after 9
presents. So a clean run proves nothing unless it reaches the path.

**Acceptance criterion this implies (path-aware).** For this class, a run counts only if it logs
`[RECOVERED] 0x00032610 returned`, has **no** `ALIAS-ICALL target=0x00032610`, shows **no** NaN at
`0x27E080` or `0x25EFB8`, and stops **beyond** `0x9CC40`. A run that takes the f24 branch has
exercised nothing and must be recorded as not exercised and rerun.

**What would change this account:** a run that logs the ABI-verified return for `0x00032610` and
*still* shows the fill — that would mean `[obj+0x38]` has a second source.

**Generalisation.** The generated dispatch carries **134 alias tuples**. Any of them reached
through a data table runs its *owner's body from the start*, which is wrong whenever the alias
target is a genuine function. Ten unrecovered ones are referenced from data sections and were
audited: `0xE9A40` (ref `0x1CEE84`, its rets are 4 and its owner's are 0), `0x102700`
(`0x1D1DD4`), `0x1199C0` (`0x1D7B6C`), `0x13A340` (`0x1DEA64`) — all `.rdata` and therefore
highest priority — plus `.data` references to `0x40002`, `0x100AB0`, `0x1A2078`, `0x1BD800`,
`0x1C3800`; `0xB090B` is referenced only from `$$XTIMAGE` and is probably coincidental. Each must
be verified from the bytes before recovery, because some data references will be coincidental.

## 10. Stack-contract contradictions are statically decidable, and 20 were live (2026-10-05)

**Four defects in one session each cost a game run and every one was decidable from the original
bytes.** `scripts/check-stack-depth.py` is the detector: a reachable CFG walk over every manifest
entry that computes `d = ESP − ESP_at_entry` and compares the declared `stack_args` with what the
reachable exits actually do.

**The identity the whole thing rests on.** `scripts/recover-functions.py` asserts
`g_esp == before_stack + 4 + stack_args` after a recovered body, and a `ret N` leaves
`esp = entry_esp + d + 4 + N` (the convention is `push` -> `d -= 4`, so `d`
is `ESP_at_ret − ESP_entry`). Equating them gives

```
stack_args = N + d          at a reachable `ret N`
```

So at a reachable `ret N` reached at `d = 0`, `stack_args` must be `N`. That is the class the gate
fails on, and the restriction to `d = 0` is load-bearing: the general `N + d`
form was implemented and **rejected on measurement**. With the walk required to be fully resolved,
**29** entries have a resolved nonzero-depth `ret N`, and in **every one** `d > 0` (min 4, max 100):
the `ret` is reached with *more* stack than at entry, i.e. the walk popped registers the body never
pushed. That is the signature of a mid-function entry or of an over-wide span whose walk ran into a
neighbouring function, so a nonzero `d` is only as trustworthy as the span extent — and asserting the
extent is right in order to conclude the value is wrong is circular. All 29 also disagree with their
declared value, and none has a verified return in any archived run, so nothing is hidden by leaving
them to `RET_DEPTH`, which is the code that exists for exactly that adjudication.

**The conclusion is about this entry; the depth may rest on callee summaries.** An earlier draft of
this section said the rule "mentions no callee", which is false. A `call` contributes its callee's
own `ret N` immediate, derived by walking the callee, and finding that callee's extent uses the
manifest and the analysis database. Measured proof that this matters: treating every `call` as
depth-neutral finds **zero** defects on the pre-turn manifest, because all twenty need their callee's
cleanup resolved before the walk reaches the `ret` at depth 0. The precise claim is that the
*conclusion* is a statement about this entry's declared value, while the *depth* it depends on is
computed from other functions.

**A false-negative in the first shipped rule hid four more defects, and it was found twice
independently.** The first rule required *every* reachable exit to be a `ret N` at depth 0. Because
`stack_args = N + d` holds on each path separately, one `ret` site reached at depth 0 by one path and
at UNKNOWN by another (through an indirect call) failed the all-depths test, so the whole entry
reported only `UNKNOWN/PARTIAL` — a live defect hidden behind an unrelated unresolved path. The rule
now gates on **any** reachable `ret N` at depth 0. Measured: 20 defects before, **24** on the
pre-turn manifest, 4 on the current one, all repaired:

| entry | declared | correct | why |
|---|---|---|---|
| `0x00021010` | 0 (key ABSENT) | **16** | only exit is `ret 0x10` at `0x21101` at depth 0; wrapper checked `+4`, body emits `esp += 20` |
| `0x000F4FF0` | 0 | **4** | every exit is `ret 4`; two are at depth 0 |
| `0x00102490` | 0 | **4** | `ret 4` at `0x102588` at depth 0 |
| `0x00152BC0` | 8 | **24** | all six exits are `ret 0x18`; `0x152DCF` is at depth 0 |

Two of the four were also swallowing a whole function each, so both spans were tightened and the
swallowed functions recovered: `0x102490` covered `0x1025B0` (`sub esp,0x1c; push esi; push edi` …
`pop edi; pop esi; add esp,0x1c; ret`, `stack_args 0`), and `0x152BC0` covered `0x152DE0`
(`mov edx,[esp+8]; cmp edx,[0x264e74]; push edi` … `pop edi; ret 8`, `stack_args 8`).
**`0x152BC0`'s wrong 8 was inherited from `0x152DE0`'s own correct `ret 8`** — the `0x74C70`
pattern of a value belonging to a different body. `0x1BCB14` remains the control the rule must not
touch: a single `ret 4` declared as 12. It **starts mid-function** and pops `esi`/`ebx` it never
pushed, so `d = +8` and `N + d = 4 + 8 = 12` — exactly the declared value, which is why it is right.
It has **no** depth-0 path, so the gate never touches it, and it is run-verified in 106 archived runs.

**The 20 live defects.** Twenty entries declared `stack_args 0` (four by an ABSENT key, so the
generated wrapper used the `0` default) while their bodies end in `ret 4` — `ret 0x14` for
`0x00080028`, the one exception. The wrappers checked `+4` where the bodies really net `+8`
(`+24` for `0x00080028`), so **every one of them would have aborted with `[RECOVERED] ABI FAILURE
… expected +4` the moment it ran**, and none ever ran: the twenty addresses appear in 0 of 122
archived `jsrf_run.log` files, in no `returned; ABI verified` line, in no `ABI FAILURE` line and
in no textual mention. They are `0x246E0`, `0x42CA0`, `0x80028`, `0x86180`, `0xA5050`, `0xCD890`,
`0xD03F0`, `0xD62A0`, `0xDB820`, `0xE2050`, `0xE2A00`, `0xE3700`, `0xEDA10`, `0xF4C60`, `0xF8AF0`,
`0x11B660`, `0x120400`, `0x124B00`, `0x134D50`, `0x139B30`. All twenty are `detection_method:
tail_jump_alias` in the analysis database, i.e. they are exactly the addresses their manifest
entries exist to un-fold (§9's class); a scan of every `.text` byte for `E8`/`E9`/`EB`/`7x`/`0F 8x`
finds **zero** transfers to any of them, so the wrapper's contract is a call contract and no
tail-jump argument rescues the old value.

**Why the other four classes do not gate, measured rather than asserted.** On the committed
manifest the census is 20 `STACK_ARGS`, 24 `RET_DEPTH`, 26 `FALL_OFF_END`, 71 `CUT_EPILOGUE` and
2 `TRUNCATED`. The span classes overlap `scripts/check-span-exits.py`'s existing 360-finding
CUT-TARGET population (156 entries) — a class the project already knows about and deliberately does
not gate — and the rest need per-entry boundary adjudication. Gating them would have required
freezing 71+ live defects in a baseline, which is exactly how `0x000307A0` stayed hidden inside
`config/entry-extent-baseline.json` while `just check` stayed green. They are counted, named and
reported as `SUSPICIOUS` instead.

**Two models were implemented and discarded on evidence.** Treating calls as depth-neutral, and
consuming a call's argument pushes, both fail: the second cannot tell a prologue save from an
argument push and produced depth `−52` on `0x0007DA30` where the truth is `0`. A heuristic that
reports confident nonsense is worse than no model, so a `call` now contributes its callee's own
`ret N` immediate, derived by walking the callee, and an unresolvable callee makes the depth
`UNKNOWN` — never a silent zero.

**What the validator cannot decide, stated rather than hidden.** 1051 of 3105 entries are
`UNKNOWN`: an unresolved indirect call or jump, or an `esp` written from a register. `0x7DA30`'s
own corrected span is one of them — its three `call dword ptr [...]` sites leave the pre-`ret`
depth unresolved, so the hidden `ret 4` is caught by the *truncated-span* form (the form the run
actually hit) and not by the depth arithmetic.

**Controls.** `--selfcheck` replays all seven historical defects (`0x80BD0`, `0x7DA30` twice,
`0x74C70`, `0xBB7B0`, `0x307A0`, `0x246E0`) at their **pre-fix** spans and asserts the verdict
*and* code each was diagnosed under, plus the negative half that every corrected entry is clean —
a control that fired on both the bad and the good span would prove nothing.
`tests/test_stack_depth.py` (CTest `jsrf_stack_depth`) adds that the gate passes with **no**
baseline, that it can actually fail (a reverted entry is rejected and named), that two runs agree
byte for byte, and that the JSON report covers every entry.

**Tool defect found on the way.** `scripts/check-generation-provenance.py --write` records only
the measured axes and silently erases the hand-maintained `amendments` and `regenerations`
history; it dropped 34 amendments. They were re-attached from `HEAD`. A session that runs
`--write` without noticing loses the provenance narrative, so this is recorded as a backlog item.

## 11. The over-wide `tail_jump_alias` record is now the dominant stop class (2026-10-05)

**Run g03 cleared `0x00094AB0` on an exercised path and immediately produced the same defect
again.** Its log holds exactly one `[RECOVERED] 0x00094AB0 returned; ABI verified (ESP/EBX/ESI/EDI)`
line — so the return is path-exercised, not merely present — and then stops at
`[ICALL] Failed to resolve VA 0x000496E0`. That address appears in **no** earlier archived run, so
it is a new stop rather than a re-observation.

**The shape, measured twice now.** Both `0x94AB0` and `0x496E0` are complete functions with **no
analysis-database entry of their own**, living inside an over-wide `tail_jump_alias` record:

| address | swallowed by | its own body | exits | `stack_args` | how the value was obtained |
|---|---|---|---|---|---|
| `0x94AB0` | *nothing* — an unanalyzed gap `0x94AA3..0x95FC0` | `0x94AB0..0x95FB2` | 3 × plain `ret` at depth 0 | 0 | **PROVED** by the gate |
| `0x496E0` | `sub_00049520 [0x49520, 0x4A6F0)` | `0x496E0..0x497D6` | 1 × `ret 8` | 8 | **INFERRED** — the gate says `UNKNOWN` |

**`0x496E0`'s value is inferred, and an earlier draft of this section overstated it.** The gate
reports that entry `UNKNOWN/PARTIAL`, not `PROVED`, because its body makes **seven** indirect
`call dword ptr [...]` calls (`[ecx+0x11c]`, `[edx+0x148]`, `[edi+0x144]`, `[edx+0x154]` twice,
`[ecx+0x154]`, `[edx+0x144]`), so the depth at the `ret 8` is not statically resolvable and there is
no depth-0 path to gate on. The value 8 comes from the byte *pattern* instead: no call is followed by
an `add esp,N` fix-up, so each callee removes its own arguments (stdcall/thiscall). The frame is
`push esi` (`0x496E7`), `push edi` (`0x49744`), `pop edi` (`0x4977B`) and `pop esi` (`0x497B7`) --
**there is no `sub esp,N` anywhere in the body and no `ebx`/`ebp` save** -- then a final argument
block (`push ecx`, `push 1`, `push eax` at `0x497BF`..`0x497CA`), `call dword ptr [edx+0x144]` at
`0x497CB`, and `ret 8` at `0x497D1`. The callee removes the three argument words, so the depth is
back to 0 at that `ret 8`. A single `ret N` after a balanced frame makes 8 the only value consistent
with that pattern, but the depth-0 claim is a convention here, not an observation.

*(An earlier revision of this paragraph gave the epilogue as `pop edi; pop esi; pop ebp; pop ebx;
add esp,0x50; ret 8`. That is `0x94AB0`'s epilogue, pasted in by mistake, and it is corrected here.
The mistake was caught by adversarial review and is recorded rather than quietly removed, because
"the value was right but the cited bytes were another function's" is exactly the kind of error that
makes an inference look like an observation.)*

`0x5C840` is recorded the same way for the same reason. The distinction
matters because the two cases have different failure modes: a `PROVED` value is forced by the bytes,
whereas an `INFERRED` one is a strong hypothesis that a run can still falsify.

`0x496E0`'s swallowing record overruns four other real functions: the manifest already owns
`0x497E0`, `0x49A80`, `0x49E80` and `0x4A6C0` as separate entries, so the `0x49520` span is wrong
for the same reason `0x139B30`'s was — it is an alias-parent extent, not a function extent.

**Why the validator did not already flag `0x496E0`.** It is not in the manifest, and
`check-stack-depth.py` validates manifest entries. The detector for this class is
`scripts/check-table-targets.py`, which reports **120 candidates** and had already flagged
`0x94AB0` as UNCOVERED. The actionable reading is that the next several stops are likely to come
from that population and can be cleared from the bytes before spending a run — `0x496E0` was
recovered, gated, regenerated, built and tested with no run at all, and only needs one to confirm.

**The title screen is still not reached.** Presents stop at exactly 1000 in g03 as in every prior
run, and the disclaimer hash `5bdaea576b8509f5` is unchanged throughout. The two newest stops are
in code the disclaimer hold reaches, not in the presenter, so they do not yet bear on the
1000-present ceiling; that ceiling remains the open question it was.

**Two runs were lost to environment faults, recorded rather than hidden.** `g01` was killed by a
tool interruption mid-run and left no `result.json`. `g02` then failed in 2.7 s at
`[SAVE] root rejected: requested directory is not writable (winerror=5)` without reaching
`guest_entry` — a stale-state artifact of the interrupted run, not a code regression; the
save-root was verified writable by hand and `g03` ran normally. Neither loss is evidence about any
code change, and neither is counted as a stop.

**Run g04 took a different path, and that is the nondeterminism, not a regression.**
`20261005-175848-754-g04-496e0` (exploratory, 250 s) exercised `0x94AB0` again — one
`[RECOVERED] 0x00094AB0 returned; ABI verified` line — and then stopped at
`[ICALL] Failed to resolve VA 0x0005C840`, an address in no earlier run. So g04 **neither confirms
nor refutes** the `0x496E0` repair: it never reached that address. Two runs of the same executable
have now taken different paths through the same boot region (g03 → `0x496E0`, g04 → `0x5C840`),
which is the third independent instance of the run-to-run variation this project records as a
first-class constraint, and the reason a single run cannot establish progress.

**`0x5C840` is the third instance of the class, and it needed a byte derivation the model could not
do.** A complete function with no analysis-database entry of its own, sitting before the over-wide
`tail_jump_alias` records `sub_0005C990` and `sub_0005CB90` (both `[0x5C990, 0x5D3B0)`, both
overrunning real functions). It has **zero rel32 callers** and exactly one reference in the image,
the `.data` dword at `0x001FA1C0` — the `0x80BD0`/`0x94AB0` pattern. `scripts/check-stack-depth.py`
reports it `UNKNOWN`, not `PROVED`, because two calls on its walk are not statically resolvable.
Its `stack_args 0` is therefore derived from the byte *pattern* rather than the model: neither call
is followed by an `add esp,N` fix-up, the direct call at `0x5C86C` to `0x1BAB20` has no pushes
before it (its arguments arrive in ecx/edx), and the vtable call at `0x5C87E` has exactly two. With
those two edges read off, both resolvable exits — `0x5C979 jmp 0x1BAA50` (target cleanup 0) and
`0x5C982` plain `ret` — sit at depth 0 and total 4. **The record keeps the distinction: this value
is inferred from a byte pattern on a partially-resolved walk, not proved by the gate.**

**The class is now large enough to be worth naming.** Three consecutive stops
(`0x94AB0`, `0x496E0`, `0x5C840`) are all "complete function missing its own entry, swallowed by or
adjacent to an over-wide `tail_jump_alias` record", and `scripts/check-table-targets.py` reports
**120 candidates** of exactly this shape. `0x496E0` and `0x5C840` were each recovered, gated,
regenerated, built and tested with **no run at all**, so the remaining population can be worked
from the bytes in batches rather than one run per stop. What a run is still needed for is
confirming that the *dispatched* address was the one repaired — and g04 shows even that is not
guaranteed on the first attempt.




## 12. The missing-entry population is a table-level defect, and the detector is blind to part of it (2026-10-05)

**Three consecutive runtime stops were the same defect, so the population was censused rather than
worked one stop at a time.** `scripts/check-table-targets.py` reports **117** candidates (not 120:
the three already-recovered addresses `0x94AB0`, `0x496E0`, `0x5C840` each had an aligned
`.data`/`.rdata` dword and have left the list). The sweep behind that number, measured from the XBE:
4117 aligned `.data`/`.rdata` dwords whose value lands in `.text`, of which 3632 already resolve at
runtime (**no work**), 485 do not, and 117 pass the boundary filter. Classification: **53 swallowed
by a manifest span, 10 swallowed by a database span only, 54 UNCOVERED true gaps.**

**24 of the 117 are pure noise**, and that matters because they inflate the apparent backlog: 13
have a padding byte (`0x90`) *at* the candidate VA, and 11 are not 4-byte aligned. Ten of the 13 come
from a single 16-bit word array at `.rdata:0x0022E1B4` whose bytes read `...6000 0200 6100 0800
6200 0800...`, so the filter passes only because the preceding word's high byte happens to be `0x90`.
Tightening the filter to require "not a padding byte at the VA" and "4-byte aligned" takes 117 to
**93**. That test should be added.

**Four live defects of the section-9 class are invisible to that census, and this is a scoping
limit rather than a bug in the checker.** `check-table-targets.py` filters on `runtime_starts()`,
which asks "can the runtime resolve this address", and that is the *correct* predicate for its
stated purpose: finding addresses that reach an indirect call and **do not resolve at all**, i.e. a
trap. A `tail_jump_alias` folded into its parent **does** resolve — the dispatch tuple names an
`recomp_alias_XXXX` shim — so it is outside that purpose by construction. But the shim runs a
*different function*, so the address is still wrong, just wrong in the section-9 way (a wrong body
rather than no body):

```
static void recomp_alias_000E9A40(void) { recomp_alias_observe(77u); sub_000E9D80(); }
{ 0x000E9A40u, (recomp_func_t)recomp_alias_000E9A40 },
```

`0xE9A40`'s own body is a complete function with a 0x12-case switch and `ret 4`; the shim runs
`sub_000E9D80` instead. Verified against `src/recomp/gen/recomp_dispatch.c`: there are **134** alias
shims and **every one** calls a `sub_` that is not its own address, so this is a property of the
whole alias-fold mechanism rather than of these four. Re-running the sweep with `genuine_starts()`
instead of `runtime_starts()` gives **121** rather than 117, and the four it surfaces are
`0xE9A40`, `0x100AB0`, `0x1199C0` and `0x13A340` — each a complete body with its own switch table
and `ret`, reachable through a `.rdata`/`.data` pointer.

**So the correct statement is not "the checker is blind" but "traps and misdispatches are two
populations and only one is censused."** What is needed is a *separate* detector for the
misdispatch class — one that asks whether an address's dispatch entry runs its own body — rather
than changing this checker's predicate, because changing it would mix the two populations and make
the trap list noisy. Of the 134 shims, the actionable subset is the aliases whose own address is a
genuine function (a real prologue and its own `ret`); the rest are genuine mid-body labels for which
running the parent's body from the start is correct. `0xE9A40`, `0x100AB0`, `0x1199C0` and
`0x13A340` are four such, and §9's list of ten data-referenced aliases is the same population. This
is the next detector change, and it is a new checker rather than an edit to this one.

**Some defects are in existing entries rather than missing ones, and the documented span convention
is what hides them.** `0x7DBD0` is a complete 190-instruction function swallowed by the manifest
entry `0x7DAE0–0x7DE20`, whose declared end is **both** that entry's own end *and* the next database
function start — so by the documented convention the entry is "correct" and still hides a function.
The same shape appears in `0x1FF90` (its own `ret 4` at `0x1FFE7`, declared end `0x200A5`, 189 bytes
of over-run swallowing `0x1FFF0`) and `0x91C00` (its own `ret` at `0x91C23`, declared end `0x91EB0`,
swallowing `0x91C30` and `0x91D70`). So "end at the next function start" is necessary but not
sufficient: an entry whose own reachable body ends well before that start is over-wide, and the
bytes between are someone else's function.

**Whole tables are missing members, so this is not a run of accidents.** Sampled function-pointer
tables and their missing-entry counts: `0x001F97B4` (the `0x496E0` family) has only 6 of 13 entries
as database starts; `0x00215530` 40 of 46; `0x0020D8A8` 24 of 27; `0x001EC288` 26 of 33;
`0x001CD2E0` 92 of 96; `0x001CA6C8` 61 of 64; `0x001CAC60` 78 of 80; `0x001EBCF4` 24 of 26;
`0x002165F0` 25 of 29; `0x001CD7F8` 47 of 48. Every sampled table has missing members.

**A cheap, strong discriminator was found and should be folded into the detectors.** For a candidate
inside a container span, walk the *container's* own reachable control flow and ask whether the
candidate is an instruction boundary reachable from the container's entry. If it is, the address is
an internal label of the container, not a separate function — the `0x30508`/`0x3060E` situation. Of
the 117, only **2** are reachable-from-container, and both are legitimate recoveries because their
containers are bogus `gap_prologue` database entries with garbage bodies (`sub_00028826` decodes to
`mov edi,edi; sub eax,[ebp-0x7a9cfffe]; …`). The other 115 are not reachable from their container's
entry, i.e. genuinely separate entry points the container merely covers.

**A method that looked authoritative was rejected.** A callee-aware stack-depth tracker produced
depth conflicts at 47–830 distinct addresses per candidate and nonsensical exit depths (`−7120`,
`−10260`), because a LIFO worklist visits a join point by whichever path happens to arrive first.
It was discarded in favour of single-exit-immediate plus prologue/epilogue frame balance, with
everything else labelled **INFERRED** rather than overclaimed. This is the same lesson as §10's
discarded call models: a depth model that cannot keep joins consistent reports confident nonsense,
and nonsense is worse than an explicit unknown.


## 13. The this-adjusting thunk: a fourth missing-entry shape, and an observed stack_args (2026-10-05)

**Run g05 cleared two stops and immediately produced a class the previous three had not shown.**
`20261005-185514-638-g05-confirm` (exploratory, 253 s) logged exactly one
`[RECOVERED] 0x000496E0 returned; ABI verified` and one for `0x0005C840`, so stops 18 and 19 are
confirmed by an exercised path rather than merely found. It then logged

```
[RECOVERED] ABI FAILURE 0x00154540 esp 00F7FD30->00F7FD40 expected +24
```

**The shape.** `0x154540` and `0x154520` are *this-adjusting thunks*:

```
0x154540  mov eax, [esp+4]        ; eax = the object
          mov ecx, [eax]          ; ecx = the object's vtable
          mov edx, [esp+8]
          add edx, 2              ; adjust the second argument
          mov [esp+8], edx
          mov [esp+4], eax        ; this-adjust
          jmp dword ptr [ecx+0x6c]   ; indirect TAIL jump to the real method
```

Both bodies are seven instructions, end at `0x154558` / `0x154538`, and are followed by 8 NOPs. The
`add edx,N` differs (2 versus 6), which is the whole point of the pair: they are the two adjustor
entries for the same virtual method at different base offsets.

**Why this needs a different rule.** A tail jump reuses the frame, so the cleanup the *caller*
performs is the **tail target's** `ret N`, not anything in the thunk. So the thunk's `stack_args`
must equal its target's cleanup — and that is only a well-defined constant if the thunk lives in
exactly one vtable.

**Settled by observation rather than inference.** The run's frozen dump, with its mapping verified
by `scripts/check-dump-mapping.py` before any guest memory was read, gives the whole chain:

| step | value |
|---|---|
| wrapper entry esp (from the ABI failure line) | `0x00F7FD30` |
| `[esp+4]` — the object | `0x0106C870` |
| `[0x0106C870]` — its vtable | `0x001E0F00` |
| `[0x001E0F00 + 0x6c]` — the real method | `0x00154420` |
| `0x00154420`'s reachable exits | four `ret 0xc` (`C2 0C 00`) at `0x154440`, `0x15445D`, `0x1544FA`, `0x15450E` |
| therefore the expected delta | `4 + 12 = 16` |
| the measured delta | `0x00F7FD40 − 0x00F7FD30 = 16` |

**The arithmetic closes exactly**, which is what makes this an observation rather than a plausible
story. Both thunks are corrected 20 → 12.

**Where the wrong 20 came from, again.** `0x154540`'s declared span `[0x154540, 0x1548E0)` had
swallowed a complete function at `0x154560` — prologue `sub esp,0x60; push ebp`, two exits
`ret 0x14` = 20 — so the declared 20 was *that* function's value. This is the third instance of the
`0x74C70` pattern (a `stack_args` inherited from a neighbouring body, after `0x152BC0`), and it is
now the single most common way a wrong value enters this manifest. The span is tightened to
`0x154558` and the swallowed function recovered as `[0x154560, 0x1548DC)` with `stack_args 20`.

**Why a single constant is not universally safe for this shape.** A `jmp [reg+0x6c]` is generic: a
scan of all 126 vtable-like runs whose `+0x6c` slot resolves into `.text` finds cleanups of **0
(64 cases), 4 (15), 8 (3), 12 (2) and 20 (2)**, plus 40 unresolvable. So "the target cleans up 12"
is a fact about *these two thunks' single vtable*, not a rule about the instruction. Both thunks
occur in exactly one aligned dword each (`0x1E0F74` and `0x1E0F70` in the table based at
`0x1E0F00`), which is what makes the value well-defined, and that is stated in the evidence rather
than assumed.

**A sign error in the identity, found by adversarial review.** The convention in
`scripts/check-stack-depth.py` is `push` → `d -= 4`, so `d = ESP_at_ret − ESP_entry` and a `ret N`
leaves `esp = entry + d + 4 + N`. Equating with the wrapper's `entry + 4 + stack_args` gives

```
stack_args = N + d
```

**not** `N − d`, which earlier revisions of §10, the plan and the checker's own comments said. The
gate was never wrong — the two forms agree at `d == 0`, and the gate only uses `d == 0` — but the
prose was, and so was the *stated reason* for not gating the general form. The corrected reason is
measured: of the **29** entries with a resolved nonzero-depth `ret N`, **every one** has `d > 0`
(min 4, max 100). A positive `d` means the `ret` is reached with more stack than at entry, i.e. the
walk popped registers the body never pushed — the signature of a mid-function entry or of an
over-wide span whose walk ran into a neighbouring function. That is an **extent** question, and
asserting the extent is right in order to conclude the declared value is wrong would be circular.
The earlier text's "negative values are the tell" was an artifact of the wrong sign: under `N − d`,
25 of those 29 appeared negative; under `N + d`, none do.

`0x1BCB14` is the entry this correction explains: it **starts mid-function** and pops `esi`/`ebx` it
never pushed, so `d = +8` and `N + d = 4 + 8 = 12`, exactly its declared value. It is run-verified in
106 archived runs and must never be gated.

**`RET_DEPTH` is now reported per path.** It previously required a fully resolved walk, which hid 14
of those 29 entries behind an unrelated unresolved path — the same false-negative shape as the
gating rule, one class down. Findings went 24 → 39 and it remains `SUSPICIOUS`, so widening it cannot
block the gate; it only makes the population visible.


## 14. The hidden-entry detector: a span can be over-wide while its `end` is "correct" (2026-10-05)

**The failure class, and why the existing detectors cannot see it.** `scripts/check-entry-extents.py`
fails when a declared `end` lands mid-instruction, and the documented convention for that end is "the
next function entry". Both can be satisfied while the entry is still **too wide**. The motivating
case:

```
0x0007DAE0   declared [0x7DAE0, 0x7DE20)
             its own reachable body ends at 0x7DBCB `ret`, then 4 NOPs
             a complete 190-instruction function begins at 0x7DBD0
0x7DE20      is BOTH this entry's declared end AND the end of the function at 0x7DBD0
```

So an end-versus-next-start check calls `0x7DAE0` correct. It is not: `0x7DBD0`'s only reference in
the whole image is the aligned `.rdata` dword at `0x0020D3C8`, no span owns it, and dispatch is an
**exact-match** binary search (`recomp_lookup`), so an indirect call through that dword traps with
`[ICALL] Failed to resolve VA 0x0007DBD0`. §12 recorded the same shape for `0x1FF90` (consumes
`0x1FFF0`) and `0x91C00` (consumes `0x91C30` and `0x91D70`) but had no detector for it.

**The invariant, and why each half is load-bearing.**

> A manifest entry's declared span must not extend past the end of its own reachable body into an
> address that has independent evidence of being a separate executable entry.

*Half 1 — the body provably ends early.* `Analyzer.walk` is reused from
`scripts/check-stack-depth.py`, not reimplemented. The finding requires the walk to be **fully
enumerated**: no truncated decode, **no fall-off at all**, no `indirect`/`terminal` exit, and the last
reachable instruction a terminator. That combination is what turns "the walk never visited this
address" into a proof — every path was followed to a known successor, so the reachable set is
complete and the address is not in it.

This is the `0x80BD0` lesson used in the safe direction, and the distinction is the whole point.
`0x80BD0` has **zero** rel32 callers and is a real standalone SEH function reached only through the
`.rdata` dword at `0x1CD000`; a previous session widened `0x80340` to `0x81853` on the reasoning
"no rel32 branch references these interior addresses" and was wrong. So the test here is **not**
caller absence. It is that *this entry's own control flow was completely enumerated and did not
arrive at the address*.

*Half 2 — the address has independent entry evidence.* Two sources, both measured: an aligned
`.data`/`.rdata` dword whose value lands in `.text` (how these functions are dispatched at all), and
an address another manifest entry already starts at (then the two spans overlap and one is wrong by
construction). A raw pointer sweep is noisy, so the candidate must also be plausible as an
instruction start: 4-byte aligned, not itself a padding byte, decodable, and preceded by an
alignment-padding boundary. The last two are the tests §12 measured as taking the sibling census from
117 to 93.

**Verdicts, and the three gating classes.**

| verdict | meaning | gate |
|---|---|---|
| `HIDDEN_ENTRY` | the consumed address resolves nowhere | **FAILS** |
| `OVERLAP` | it is another manifest entry | **FAILS** |
| `SHADOWED` | it already has its own generated body | **FAILS** |
| `MISDISPATCH` | it resolves to a *different* symbol | reported |
| `OVER_RUN` | the over-run holds no evidenced entry | reported |
| `UNQUALIFIED` | the walk has an opaque exit, so separation is not provable | reported |

The gate needs **no baseline**, for the same reason §10's gate gates only its `STACK_ARGS` class:
each gating class is a property of this entry's own bytes plus one other address's independent
reference, with no whole-program reasoning. `UNQUALIFIED` is never silently clean — `0x96F60` ends in
`jmp dword ptr [eax+8]`, so its over-run into `0x96F80` is real but *not provable*, and a control
asserts it stays undecided rather than being reported as a separation.

`MISDISPATCH` is the population §12 said needed a *separate* detector from `check-table-targets.py`:
an alias shim **does** resolve, so the trap checker cannot see it, but the symbol answering it is its
parent's, so entering it runs the wrong body. It is reported rather than gated because §12 leaves the
actionable subset open ("the rest are genuine mid-body labels for which running the parent's body
from the start is correct").

**Census on the pre-repair manifest: 50 containers, 63 consumed addresses.** 45 `HIDDEN_ENTRY`, 5
`OVERLAP`, 2 `SHADOWED`, 2 `UNQUALIFIED` (the `0x96F60`/`0xAEE80` pair), 265 `OVER_RUN`. Every
container is a `tail_jump_alias` record, which is the same dominant stop class §11 names. The
consumed bodies are substantial, not stubs: sizes 50 to 1213 bytes, **median 360**.

**The repairs, and what each value rests on.** 50 spans tightened to their own reachable end; 48 new
reviewed entries; 5 consumed addresses already owned an entry and 2 (`0x556D0`, `0xC42E0`) already had
a generated body, so those got no entry. Of the 48 additions, **38 are `PROVED`** — a fully enumerated
walk reaches a `ret N` at depth 0, so `stack_args = N` by §10's identity — and **10 are `INFERRED`**,
where the walk is enumerated and every reachable `ret` agrees on `N` but no path reaches one at depth
0. The record keeps that distinction rather than flattening it.

**Three real defects were caught by tests rather than by reasoning, and each is now a guard.** They
are recorded because the first two were *silent*:

1. **`apply()` keyed additions to the container's start**, so all 50 new entries were dropped while
   the run printed `wrote 3110 entries` and the checker still passed — because removing the additions
   removes the finding. It was caught by counting `set(after) - set(before)` instead of trusting the
   exit code. Additions are now keyed by their own start and a duplicate-start check runs before the
   write.
2. **Two additions collided with generated bodies** (`0x556D0`, `0xC42E0`): the generated chunk
   already defines `sub_<va>`, so the link failed with
   `LNK2005: sub_000556D0 already defined in recovered.obj`. The first version of the checker decided
   "resolves at runtime" from a set that defaults to empty, which misclassified both. `SHADOWED` is
   that class, read from `recomp_dispatch.c`'s own-symbol tuples.
3. **`0xB3C30` was given an extent one tail-jump short.** Bounded at its first candidate end
   (`0xB3D67`) its walk looks complete, but two of its exits are tail jumps to `0xB3DCF`/`0xB3DD0`,
   which are internal to the real body ending at `0xB3DD4`. The generated body then called a fatal
   stub for its own continuation, and `tests/test_recovery_span_ownership.py` caught it.
   `repair-hidden-entries.py:resolve()` now rejects any bound whose exits include a tail to an address
   that is not a boundary.

**Independent reproduction of the population.** A prototype written before the checker agreed with
`check-table-targets.py`'s existing in-span rule on **50 of 52** candidates, the two differences being
exactly the two opaque-exit containers. The detector also re-finds `0x96F80`, which §12 had already
recorded as a known open instance of the class, without being told about it.

**A jump-table under-read cannot produce a false finding.** Of the 50 gate containers, 6 have a
reachable jump-table `jmp`; none has a table that stopped at `MAX_JUMP_TABLE` or whose next dword was
still executable code, and no dropped arm lands on a consumed address. Any `jmp` the walk cannot fully
resolve is recorded opaque, which degrades the entry to `UNQUALIFIED` rather than to a finding.

**A known tool defect was hit twice and worked around, not fixed.** `check-generation-provenance.py
--write` records only the measured axes and erases the hand-maintained `amendments` and
`regenerations` history — it dropped 39 and 1 respectively this session (the plan already records this
from a prior turn). They were re-attached from the pre-write copy both times, so the file now holds 40
amendments and 2 regenerations. **The tool is still wrong**; the workaround is not a fix.

**Verification.** `just check` green; CTest **38/38** (was 37; `jsrf_hidden_entries` added); stack-depth
`--selfcheck` **10/10**; hidden-entry `--selfcheck` 5/5 controls plus 17 unit tests including a
deciding negative control that re-injects the motivating defect into a temporary manifest and requires
the real gate to exit nonzero; `recovered.c` regenerated (3158 functions); preservation baseline
re-recorded with its `updates` history preserved. **The title screen is still not reached and M15 is
not claimed.**


## 15. Stop 20 is runtime-confirmed, and g07 exposed the mirror-image span defect (2026-10-05)

**Stop 20 is confirmed by an exercised path, not merely found.** Run g07
(`20261005-211627-927-g07-thunk`, exploratory, 900 s budget, ended `unhandled_exception` at 241 s) logs
exactly one

```
[RECOVERED] 0x00154540 returned; ABI verified (ESP/EBX/ESI/EDI)
```

which is the confirmation g05 found but g06 missed. The distinction the previous turn insisted on was
correct and is now discharged: the thunk repair — two this-adjusting thunks corrected 20 → 12, with the
swallowed `0x154560` recovered — is **runtime-confirmed**. The same run also exercised `0x5BF00`/
`0x5C840`, `0x496E0` and `0x488B0`.

**Honest reading of the same run.** Presents still froze at exactly **1000** with the disclaimer hash
`5bdaea576b8509f5` unchanged, and the thunk executed *after* that freeze: the `[FBPRESENT] presents=1000`
line is 75471 and the ABI-verified return is 105311, in a 105,337-line log. So g07 confirms the repair
and advances the stop chain; **it is not title progress**, and the 1000-present ceiling is untouched by
it.

**g07 then produced a stop of a class the new detector does not cover — the mirror image of §14.**
`[ICALL] Failed to resolve VA 0x000B5F82`. §14's class is an **over-wide** span that consumes a
neighbour; this is an **under-wide** span that cuts its own function:

```
0x000B5EB0  declared [0x000B5EB0, 0x000B5F3A)   <- tightened by an earlier pass
            its OWN jump table at 0x000B5F0F has 9 arms
            8 of those arms lie beyond 0xB5F3A: 0xB5F82, 0xB632D, 0xB6574, 0xB670E
            so the lifter emitted RECOMP_ITAIL instead of a resolved switch
            the guest took the 0xB5F82 arm and trapped
```

The emitted C states the mechanism exactly. With the tightened end the body is one line —
`g_seh_ebp = ebp; RECOMP_ITAIL(MEM32(eax * 4 + 0xB6734)); return;` — naming no arm at all. With the
real end it is a `switch` whose five distinct arms all resolve to `loc_` labels, and the body contains
no unresolved `sub_` call.

**`0x000B5F3A` is not an entry, and four independent observations say so:**

| observation | detail |
|---|---|
| its owner's own table | `0xB5EB0`'s jump table has an arm at `0xB5F16` that **falls through** into `0xB5F3A`; the `call` at `0xB5F35` is immediately followed by it |
| its "pointer" | the only aligned dword naming it is `0x228214`, inside a packed `.data` run whose neighbours read `0x61510442`, `0x00985100`, `0x38009753` — data, not a table |
| its exit | its walk ends at the same `0xB672F ret 4` as `0xB5EB0`, i.e. the same function |
| its arm set | every arm of `0xB5EB0`'s table lies inside `[0xB5EB0, 0xB6732)`, so the widened body owns the address |

**A discriminator was tried and discarded, and the reason is worth keeping.** The
"reads a register before writing it" test that proved `sub_000BBA04` a false entry **does not
generalise**: 684 genuine manifest entries trip it (330 read `ecx` first, 321 read `ebp` — a thiscall
entry legitimately reads `ecx`, and a `mov [esp+N], ebp` prologue legitimately reads `ebp`). Reporting
those as false entries would have been confident nonsense. The owner-reachability and jump-table-arm
evidence above is what the repair rests on instead.

**The repair, and the second link failure it caused.** `0x000B5EB0` is restored to its real end
`0x000B6732` and the false split `0x000B5F3A` removed. That alone does not link: `recomp_dispatch.c` is
translation-owned and still carried `{ 0x000B5F3Au, (recomp_func_t)sub_000B5F3A }`, so the build failed
with `LNK2001: unresolved external symbol sub_000B5F3A`. `config/generated-patches.json` gains
`remove-b5f3a-dispatch` (**L02**), following the `remove-54750-stub` precedent: the patch system
requires a non-empty replacement, so the tuple is replaced by a comment rather than deleted, and
`patch-generated.py` re-applies it after every regeneration. `recovered.c` 3157 functions; manifest
entries 3158 → 3157.

**The class is now named, and it is not yet detected.** §14's detector looks for a span that over-runs
its own body; this defect is a span that stops *before* its own jump table's arms. The two share a
witness — the owner's own reachable CFG — but run in opposite directions, and a span can be wrong in
both at once. The candidate population for the under-wide half was measured as **71** adjacent-entry
pairs where the owner's walk reaches the next entry's start, but that population is dominated by
legitimate splits (the `0x200A5`/`0x200A8`/`0x200AD` micro-fragment run is a chain of real
continuations), so it is **recorded as an open measurement, not as a gate**. A detector for it needs a
discriminator that separates "the owner's table arm reaches this address" from "this address is simply
the next function", and that discriminator is not established here.

**Verification.** CTest **38/38**; `just check` green; both static gates pass; provenance manifest and
preservation baseline re-recorded with their hand-maintained history preserved (41 amendments,
3 regenerations). Commit `64945a3`. **The title screen is still not reached and M15 is not claimed.**


## 16. A host crash this turn shipped, found by a control rather than a gate (2026-10-05)

**The defect, and why no checker saw it.** `remove-b5f3a-dispatch` removed the
`0x000B5F3A` dispatch tuple but left

```c
static const size_t g_recomp_table_size = 8928;   /* the array now holds 8927 */
```

`recomp_dispatch_init` loops `for (i = 0; i < g_recomp_table_size; i++)`, so it read one entry past the
end of `g_recomp_table` and wrote `g_flat_table[...]` from the garbage it read. The process died with a
host access violation (`0xC0000005`, **write**) at `recomp_dispatch.c:9299` inside
`recomp_dispatch_init`, **before `guest_entry`**, in runs g08 and g08b: 4 s,
`missing_checkpoints=[guest_entry]`, and the minidump resolved `RIP` to
`recomp_dispatch_init+0xC3`.

**Every static gate passed on that tree.** `just check`, the full CTest suite and
`check-merge-structure.py` all accept a dispatch table whose declared size exceeds its contents — there
is no checker that compares `g_recomp_table_size` to the tuple count. This is the sharpest instance so
far of the pattern the plan already records: a green suite is not a statement about a class no checker
covers.

**The control that localized it, instead of guessing.** g07's **archived** binary was re-run in the same
environment. It reached `guest_entry`; the new build died at the same log line (75 → 76). That ruled out
environment, disk and nondeterminism *before* any code was touched, and it is why the fix took one
attempt rather than a bisect. The same technique is what the plan asks for when a run regresses: compare
against the archived binary, not against memory of the previous run.

**The fix.** A companion patch, `fix-dispatch-table-size` (L02), decrements the count to 8927.
`PATCH_FIELDS` permits one `before`/`after` per patch, so this is a second patch rather than a second
edit of the first. g08c then reached `guest_entry` with `checkpoints_passed: true`.

**The durable lesson, stated as a rule.** A patch that removes an entry from a generated table must
correct that table's declared count in the same change. `patch-generated.py` cannot enforce it — it
checks that `before` matched, not that the surrounding arithmetic stayed consistent — so it belongs in
the review checklist for any future table-editing patch.


## 17. The Advisor ruling, and two of this turn's measurements it corrected (2026-10-05)

**Route.** The Persistent Advisor failed to launch **three times** on `claude/claude-opus-5-5` @ xhigh,
each child dying before finishing with no closing message. Per workflow §1 that was reported rather than
silently replaced, and per §7 the owner directed the route change to **`codex/gpt-6.1-sol` @ xhigh**.
This section is therefore the first *real* Advisor ruling of the turn, and it is attributed to that
route. The Turn Planner and the Turn Reviewer both ran normally, so the earlier failure was specific to
that spawn rather than to the route being unavailable.

**Forward erratum for commit `64945a3`.** That commit's message says the patch
`remove-b5f3a-dispatch` carries ledger **`(L42)`**. That is **wrong**. Its ledger is **L02**
("Reviewed recovered bodies … and boundary fixes"), the entry its own precedent `remove-54750-stub`
cites; L42 is "File completion APCs run at the next alertable wait", which is kernel APC timing and has
nothing to do with this patch. Turn Review 1 caught it, and because `scripts/patch-generated.py` only
checks that a ledger ID *exists*, `just check` had passed regardless. The published commit is **not**
amended or force-pushed; this paragraph is the erratum. The correction landed in
`config/generated-patches.json` (ledger `L02`), the L02 row, and every active citation.

**Correction 1: the 192/234 "fatal stub" count was a spelling count, not a fatal count.** Scanning
bodies that contain a `RECOMP_ITAIL` for `g_seh_ebp = ebp; sub_<va>(); return;` finds **192 bodies and
234 distinct targets** — and the Advisor reproduced those numbers exactly. But that spelling is also how
a *normal* recovered call is emitted, so the population is **generic external tail targets**, not fatal
stubs and not table arms. Intersecting against the actual production trap definitions gives
**27 bodies / 50 distinct targets**. The turn's earlier position breakdown (115 manifest / 74 dispatched
/ 30 unevidenced / 15 before-start) was measured on the inflated set and **should not be relied on**.

**Correction 2: `0xFC370` is not an unrelated-guard example.** The turn used it to argue that a located
`cmp/ja` may belong to a different switch, because its table read produced the implausible arm
`0x20200`. The original bytes show the real shape is a **two-level selector map**:

```asm
0xFC387  cmp   eax, 8
0xFC38A  ja    0xFC47A
0xFC490  ...   ; nine selector bytes {0,2,2,0,2,0,2,1,0}
0xFC484  ...   ; three dword slots, reached via movzx eax, byte [eax+0xFC490]
```

So `0x20200` is selector-map data misread as a fourth pointer, and the emitted body correctly recognises
three local targets. The general lesson is the Advisor's: a guard must be tied to the **actual index
value at the jump**, including any byte/word remapping, not to a register name or the nearest `cmp`.
Requiring `N + 1` to equal the run of consecutive in-`.text` dwords is neither necessary nor sufficient —
`0xFC370` needs nine selector bytes and three dwords, while `0xB5EB0` needs exactly nine dwords.

**What the Advisor recommended, and what this turn does with it.**

| recommendation | disposition |
|---|---|
| do **not** gate "out-of-span arm" alone: table tails and shared continuations can legitimately leave a span, and pointer membership, decodability and alignment do not prove a table's extent | **accepted** — no under-wide gate is added |
| gate a narrower *certified lost continuation* class, zero-baseline, repairing every qualifying finding first | **accepted as backlog**, with the required proof rule recorded: prove a reachable guard-to-jump path, prove the guard tests the same index value (through any remap), verify no intervening clobber and that the default edge bypasses the jump, and read exactly the reachable slots from file-backed bytes; anything else is `UNPROVEN`, printed, never `CLEAN` |
| keep the broad census **mandatory and visible**; do not add the population to a frozen baseline | **accepted** — `tests/test_recovery_span_ownership.py`'s `KNOWN_OPEN` subtraction is explicitly *not* closure |
| `0xB06E0` is a **proved** static defect and should be repaired as a small unit, not left in an indefinite backlog | **accepted** — see below; the turn did not repair it, and records that |

**`0xB06E0`, independently re-verified from the original bytes.** The Advisor's witness is stronger than
a table-extent argument and the turn reproduced it:

```asm
0xB06E0  push esi
0xB06E1  mov  esi, ecx
0xB06E3  mov  eax, [esi+0x128]
0xB06E9  cmp  eax, -1
0xB06EC  je   0xB09DC
...
0xB09D9  pop  edi ; pop ebp ; pop ebx
0xB09DC  pop  esi
0xB09DD  ret  4
```

The manifest span is `[0xB06E0, 0xB0811)`, so the function's **own epilogue** at `0xB09DC` lies outside
it, and the emitted body calls the fatal stub `sub_000B09DC` **twice** and `sub_000B09D9` **four**
times. A
valid object with `[ecx+0x128] == 0xFFFFFFFF` selects a plain return requiring no table heuristic at
all. It is a **proved static production defect**. It is **not** repaired in this turn — the remediation
scope is records-only plus the crash fix — and it is recorded as the next static work with its proof
rule, not as a deferred unknown. **No g07/g08 path has been observed to reach it**, which affects its
priority and not whether it is broken.

**A correction to this turn's own under-wide prose.** §15 called the 71-pair adjacent-entry population
"dominated by legitimate splits" and cited the `0x200A5`/`0x200A8`/`0x200AD` run as "a chain of real
continuations". The Advisor's point stands: those nine micro-entries are contained by `0x1FFF0`, most
have no `stack_args`, their only "pointers" are consecutive dwords in a packed `.data` run, and calling
them real continuations is **inference the bytes argue against**. They are **identity-unproven**, not
legitimate, and are recorded that way.


## 18. Errata and the under-wide class's own census (2026-10-06)

**Erratum: the `0xB06E0` trap-stub counts were reversed.** §17, the plan and commit `892dd1e`'s
message all said the pre-repair body called `sub_000B09DC` four times and `sub_000B09D9` twice. The
measured counts are the **opposite**: `sub_000B09DC` **twice** (`0xB06EC`, `0xB06FF`) and
`sub_000B09D9` **four** times (`0xB0720`, `0xB0743`, `0xB07CC`, `0xB07D8`). Re-measured from the
archived pre-repair body in `logs/runs/20261005-223404-818-g08-b5eb0-fixed/source.zip`, which is the
authority for what that run was built from, and independently by the Persistent Advisor. The counts
are corrected in §17, the plan and `config/stop-chain.json` row 23; the published commit is **not**
amended, so this paragraph is the erratum. The defect and its repair are unaffected.

**Why the hidden-entry detector missed `0xB06E0`.** `HiddenEntryFinder.body()` returns `None` (hence
`CLEAN`) whenever the walk falls off its declared end, and the old span fell off at `0xB0811` at depth
`-16`. The detector is for **over-wide** spans by construction; `0xB06E0` was **under-wide**, the
mirror class. `check-stack-depth.py` did see it, but only as `SUSPICIOUS/CUT_EPILOGUE`, which does not
gate. That is the honest statement of the coverage gap, and it is why the under-wide class needs its
own evidence rather than an extension of the over-wide gate.

**The under-wide class, measured structurally.** The Orchestrator's census walks every manifest entry
over its own declared span and asks two questions. It finds:

| measure | count |
|---|---|
| entries whose own fully-enumerated walk reaches a **fatal** trap stub | **128** |
| entries where a **conditional** branch leaves the span to an address that is neither a manifest start nor inside any genuine span, and extending to the next manifest start certifies one single `ret N` | **150 PROVED, 4 INFERRED** |
| of the certified set, entries whose certified `N` disagrees with the declared `stack_args` | **10** |

The conditional and unconditional populations are kept **separate**: for an unconditional `jmp` out
of span, "this function's own continuation" and "a legitimate tail call to a separate function" are
not distinguishable from the bytes alone, so that subset is a measurement and not a gate. The
conditional subset is the defensible one, and its first prediction was correct in advance — see below.

**The census's first call, and it was right.** Before any run reached it, the census reported
`0x000AE560-0x000AE5F1 -> end 0x000AE659 N=4`, listing seven conditional branches to `0xAE655`. Run
`20261006-003520-133-f9-underwide-batch-pb` then logged `[ICALL] Failed to resolve VA 0x000AE655`
with `0x000AE560` as ICALL-history frame 15, immediately after that body's ABI-verified return.
`0xAE655` is the shared epilogue (`pop esi` at `0xAE5ED`, `ret 4` at `0xAE5EE`) that the declared end
cut off. Repaired to end `0x000AE659`; `stack_args 4` was already right, since both exits are `ret 4`
at depth 0.

**Stops 21 and 22 are runtime-confirmed.** The same run is the first ever to execute either repaired
address, and it logs both `[RECOVERED] 0x000B5EB0 returned; ABI verified` and
`[RECOVERED] 0x00048DB0 returned; ABI verified`. So `0xB5EB0` — repaired from the bytes and then
missed by two consecutive runs — is finally confirmed, and the found-versus-confirmed distinction is
discharged for both.

**The certified-continuation gate, and two holes in it that were found by review.** `L43` and
`config/stop-chain.json` bind each stop row to the archived runs that establish it. Review found two
producer-side holes, both real and both now closed with a control taken from a real archive:

1. **The return line is not exclusive with failure.** `20260930-225440-580-f3-alias-fix-strict` logs
   `0x00026780 returned; ABI verified` at line 75094 and `ABI FAILURE 0x00026780 ... expected +4` at
   line 77608, and `check-run-exercised.py` reported **PASS** for it. A confirming role now also
   requires that the address has no ABI-failure line in that log, and that the run did not set
   `JSRF_ABI_CONTINUE` (which turns the check into a report, so its returns prove less).
2. **Descent from the repair commit is necessary but not sufficient.** A later commit can move the
   span again. The archive carries `source.zip`, so the run's own
   `config/recovered-functions.json` is available in 134 of 135 archives; a confirming role now also
   requires the archived `(end, stack_args)` tuple to equal the current one. `0xAE560` is the live
   example: f9 executed `(end 0x000AE5F1, stack_args 4)` and the tree now says
   `(end 0x000AE659, stack_args 4)`.

Both rules are enforced, both have a real-archive control, and all seven current confirming citations
were re-verified against them.

**The dispatch gate counted spelling, not rows.** `check-dispatch-table.py` matched row text without
stripping comments, so a tuple commented out rather than replaced by prose left the array one row
short of its declared count and the checker **passed**. The table body is now comment-stripped before
counting, with the commented-tuple case as a control and the two real archived crash trees
(`20261005-222801-316-g08-b5eb0` and `20261005-222917-439-g08b-repro`, both declared 8928 against
8927 rows) as end-to-end controls. The structural fix — deriving the count with `sizeof` — is
unaffected and remains the primary defence.

**A latent hazard that is NOT a live defect, and how the difference was settled.** Ten addresses have
both a recovered body (`sub_<va>` in `recovered.c`, with a `case` in `jsrf_lookup_recovered`) and an
alias shim in the generated dispatch table that routes them to a **different** symbol:

```text
0x00027B00 -> recomp_alias_00027B00 -> sub_00027CD0
0x0002C360 -> recomp_alias_0002C360 -> sub_0002D1E0
0x00032610 -> recomp_alias_00032610 -> sub_00033800
0x00032C70 -> recomp_alias_00032C70 -> sub_00033800
0x00033C50 -> recomp_alias_00033C50 -> sub_000355B0
0x00034200 -> recomp_alias_00034200 -> sub_000355B0
0x000348A0 -> recomp_alias_000348A0 -> sub_000355B0
0x00035640 -> recomp_alias_00035640 -> sub_000360D0
0x00037550 -> recomp_alias_00037550 -> sub_00038530
0x0014FEF0 -> recomp_alias_0014FEF0 -> sub_00150231
```

A spelling-level census of the archive suggests these shims fire *after* their recovery: 44
`[ALIAS-ICALL]` lines name one of these ten targets across 28 runs, including runs whose commit is
descended from the recovery. **That reading is wrong, and the way it was refuted is the point.**

`RECOMP_ICALL` tries `recomp_lookup_manual` **first**, and that function returns
`jsrf_lookup_recovered(va)`, which has a `case` for every recovered body. So once an address is
recovered the shim is **unreachable**. The decisive test is not git ancestry (which only bounds the
*commit*) but each run's **own archived `recovered.c`**, which is the artifact the run was built
from:

| firings where the run's own build already had the `case` | **0** |
|---|---|
| firings where the build did not yet have it | 44 |

Every firing predates that address's recovery. So the class is **closed by recovery**, not live, and
these ten shim tuples are a **latent hazard rather than a current defect**: they are dead code today,
and they would become a wrong-body misdispatch only if the recovered `case` were ever removed while
the tuple stayed.

Two lessons, both already paid for elsewhere in this record. **A text search over archived logs is
not evidence about which build produced them** — the `0x00032610`/f25 error was exactly that, and this
is the same mistake in the opposite direction: a true-looking positive instead of a false one. And
**ancestry bounds a commit, not an artifact**: `recovered.c` is untracked, so the only authority for
what a run executed is the copy inside its own `source.zip`.



