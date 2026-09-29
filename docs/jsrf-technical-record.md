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

`call dword ptr [0x1C4064]` at `0x00149828` reads `0` and raises `0xE0424943`. The XBE holds
`0x80000115` there (the ordinal-277 kernel thunk); the runtime patches that table. The ~571 MB
allocation failure just before it (`NtAllocateVirtualMemory`, returned `0xC0000017`) **is handled** by
the guest (`0x00149E56 test eax,eax` / `jl 0x149eec`, clean return through `__SEH_epilog`), so it is not
the cause (Advisor critical-path ruling, 2026-09-27). The producer line that chased the allocation size
is parked; reopen only if the slot death proves downstream of that error handling, or a later gate
needs the size explained.

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

**Status: committed on branch `jsrf/fork-fixes` in both repositories, not pushed, and not verified on
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

### XDK D3D device fields — INFERRED, verify against JSRF bytes

From `~/src/halo-ce-universal` (Halo CE Xbox, XDK ~3911, a matching decompilation; `libs/d3d8` is
GPL-3.0 and RXDK-derived, so **facts only, no code**): `libs/d3d8/device_layout.h:180-182` and
`d3dbase.cpp:142` (`SetVerticalBlankCallback` stores `g_pDevice->m_Miniport.m_pVerticalBlankCallback`).
Matched to JSRF offsets by role, anchored on measurements already in `docs/jsrf-kick-get-contract.md`,
`jsrf-callback-reentry-contract.md` and §5; the miniport context starts at device `+0x2268`:

| JSRF device offset | XDK field (Halo name) | Basis |
|---|---|---|
| `+0x242C` | `m_pVerticalBlankCallback` | only store is `0x0018CE3A`, in a function shaped like `D3DDevice_SetVerticalBlankCallback` |
| `+0x2430` | `m_VerticalBlankEvent` (KEVENT) | `KeWaitForSingleObject` target in `sub_0018CE50` (`BlockUntilVerticalBlank`) |
| `+0x2434` | that event's `Header.SignalState` | cleared before the wait (`KeClearEvent` is inline) |
| `+0x2440` | `m_BusyBlockEvent` | second event, same initialisation |

Role matches, also inferred: `0x0018CE30` SetVerticalBlankCallback, `0x0018CE50` BlockUntilVerticalBlank,
`0x00193D90` `CMiniport::VBlank`, `0x00194210` `ServiceGrInterrupt`, `0x00193F70` `SoftwareMethod`.
**Consequence for A2h (a lead, not a finding):** the only legitimate writer of `+0x242C` is
SetVerticalBlankCallback, so `sub_00038530` writing `0x001D5078` there means the object it initialises
overlaps `g_Device` (`0x0019B200`–`0x0019DCE0`), or its argument is mistranslated. JSRF's CDevice is
probably `0x2AE0` bytes (the `g_Device`→`g_pDevice` gap); Halo's is `0x2B90`, so offsets do not carry
over by arithmetic.

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

### Toolkit changes on `jsrf/fork-fixes` (base `db96e30`)

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
