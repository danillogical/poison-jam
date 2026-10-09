# Jet Set Radio Future: Windows port plan

**Status: execution authority** (adopted by the owner 2026-09-29; condensed 2026-10-06). The text
before condensation — every per-run narrative, the Phase 0/1/2 tables and the TTD line — is
`git show dcc93ab:plan-jsrf-bare-minimum.md`; the condensation and the turns it summarises are
recorded in `docs/jsrf-operating-history.md` ("2026-10-06 — plan condensed").

**Authorities, so this file does not repeat them.** `docs/agent-workflow.md` owns roles and turns;
`docs/jsrf-run-profiles.md` owns evidence profiles; `docs/jsrf-technical-record.md` ("TR §n") owns
established facts; `docs/jsrf-compatibility-ledger.md` owns every shortcut (ledger IDs `Lnn`/`Dn`);
`config/stop-chain.json` owns the runtime stop chain; `AGENTS.md` owns build and repository
discipline. This plan owns the objective, the milestones and what to do next.

---

## Direction (owner decision, 2026-09-30): the bare minimum is pragmatic

- **Take the path of least resistance to the title screen** (DoD-BOOT, M15), then to the rest of the
  slice. The fast path below supersedes any older ordering until the title screen is reached.
- **Every departure from running the original code on real hardware is recorded** in the ledger,
  classed as reimplemented, translated, emulated, wrapped, stubbed, patched, approximated or
  intentionally ignored. A shortcut is allowed; an unrecorded shortcut is not.
- **Exploratory runs may satisfy bare-minimum milestones** when the run record lists the ledger IDs
  it relied on (`docs/jsrf-run-profiles.md` §"Pragmatic bare minimum"). Strict runs remain the
  diagnostic for fidelity questions.
- **The Orchestrator runs the fast path**; the Turn Reviewer reproduces a milestone claim, with its
  ledger IDs, rather than accepting the description.

## Current work (state 2026-10-09: title-010, then the Windows gate of the v0.13.1 merge)

**The v0.13.1 merge passed its Windows gate, and it costs speed, not function** (2026-10-09, run from
the Mac over ssh; TR §1 "v0.13.1 sync", TR §26). The Mac-side toolkit changes after title-010 — the
review fixes `fafe0f6` and the upstream merge `409c635` (upstream's executor as the base, the fork's
consumer, present tracker, flip trace, stage-0 gate and fixed-function paths ported in; L18) — build
and pass every suite on Windows (game CTest 49/49, toolkit CTest 15/15, the four standalone toolkit
test projects, `just check`). The 300 s title-run A/B on one game commit: neither binary crashes, but
the merged executor reaches **960** presents at t = 120 s against **1750** for `fafe0f6`, with late
re-arms **27.6 %** against **0.4 %** and a 192 ms owner-lock hold. The new executor-time instrument
attributes it: **pixel fill is 207 s of the executor's 218 s busy time** (the executor is busy 74 % of
the run, under the owner lock), vertex programs 46 ms; turning vertex programs off moves the same pixels onto the
screen-space path and stays slow. The owner's call: a performance regression on a non-working
prototype is not a revert criterion, so the merge stays and the cost is next action 2's subject.
The same session fixed the telemetry clock's per-file epoch and showed the guest itself disables
stage 0 (16230 `SET_TEXTURE_CONTROL0` writes, 7581 with ENABLE clear; TR §26).

**THE ARCHIVED "ADX WORKER DEATHS" ARE NOT DEATHS, AND THE POPULATION DOES NOT REPRODUCE ON THE
FIXED BINARY.** This retires the previous turn's critical hypothesis. Two independent lines agree.

*The detector that produced the population was unsound.* The title-009 contingency
(`verify_contingency.py`, which reproduces its recorded 41.7 % vs 12.3 % exactly) attributed a
`[KERNEL] summary` block to a thread by **exact `esp` string** against
`{'01220F50','012A0F8C','01320F88','007BFFCC'}`, and took `worker_last = MAX(t)` over that set.
Two measured defects: **`0x007BFFCC` is the MAIN thread** — the loader logs `Stack: 8192 KB at Xbox VA
0x00780000 (ESP = 0x00F7FFF0)`, so the main stack is `0x00780000..0x00F80000` and that address is
inside it — so `worker_last` was normally the main thread's own last sample and a "death" was
reported only when the main thread happened not to print after its last present; and exact-esp
matching **splits one thread across keys** (the worker at stack `0x01220FF0` appears at both
`0x01220F50` and `0x01220F48`, and in `title007-noadmit` at six addresses). Re-attributed by owning
stack (`logs/workers/title010/orch/recompute_contingency.py`): wrapped **8 L / 16 K = 33.3 %**,
unwrapped **0 L / 60 K = 0.0 %** — **all ten "unwrapped deaths" disappear**, their gaps collapsing
from 37–455 s to 0. The plan's cited unwrapped list (`long3d`, `units`, `units4`, `blacktrace`,
`trace-noadmit`, `admitted39`, `title005-admit3`) shows 10–23 s gaps under **both** detectors and is
not reproduced by the cited script at all.

*What the workers actually are.* They are **live threads parked on the D3D vertical-blank event
`0x0019D630`**, not exits: every captured run shows `state=1` and a native stack in
`xbox_KeWaitInplaceEvent` ← `bridge_KeWaitForSingleObject` ← `sub_0018CE50` ← `body_0013B1C0`, and
**zero** `worker thread returned` / `PsTerminateSystemThread` lines. The new `[WAIT]` instrument
confirms it directly: all 256 logged long waits are `timeout=INFINITE` on `0x0019D630`.

*On the fixed binary there is no death population.* R1 (`…155241`, 904.3 s) and R2 (`…185536`,
906.2 s) each contain an unsigned QPC wrap (+763.1 s, +854.6 s) and each kept **every** worker
through and past it, with zero exits. That is **Case D** — workers are off the M15 critical path.
Predictions for R1 were written before its artifacts were read
(`logs/workers/title010/orch/WRAP_CROSSING_PREDICTION.md`); 3 of 4 held and the fourth failed in an
informative direction (below).

**A REAL SECOND DEFECT, MEASURED AND NOW LARGELY FIXED: the walk's per-word `VirtualQuery`.**
`submit_read_word` (`nv2a_core.c:1261-1282`) called `VirtualQuery` **once per pushbuffer word**,
under `g_mmio_owner_lock` — which the ptimer thread needs in order to pulse vblank. Measured on this
host at the base of a 64 MB mapped view (`logs/workers/title010/orch/vq_bench.py`): **12 µs**
untouched but **378 µs** once resident, because `VirtualQuery` walks the region's page-descriptor
chain. The arithmetic corroborates the run's own telemetry from a different instrument: R1's
`walk_words_max` 8144 × 185 µs = **1507 ms** against its measured max hold of **1636 ms** (8 %).

*Measured effect of caching the validated span per walk* (toolkit `5d6ebbd`, `nv2a_read_guard` test):

| statistic | R1 (no cache) | R2 (no cache) | **R3 (cache)** |
|---|---|---|---|
| late re-arms / passes | 2941/4256 = **69.1 %** | 2961/4123 = **71.8 %** | **0 / 11924 = 0.0 %** |
| max owner-lock hold | 1636 ms | 1219 ms | **68 ms** |
| max pulse-to-pulse gap | 13 995 ms | 14 791 ms | **78 ms** |
| pulses per pass | 0.297 | 0.275 | **0.979** |
| presents at matched `t=120 s` | 628 | 641 | **2568 (4.1×)** |

**R3 terminated early** at 236.5 s with `outcome=unhandled_exception` on the fatal unresolved call
`0xE0424943` at VA **`0x9188C`**. The crash stack attributes it to a **pre-existing latent defect,
not the cache**: `sub_0009188C` is a generated recovery stub whose whole body is
`recomp_icall_fail_log(0x0009188C); abort();`, reached from `body_00091830` — and TR §20 already
records `0x91830` as an entry whose internal target `0x9188C` decodes as ordinary code and was
**not repaired**. The stack contains no frame from the walk, the cache or the owner lock. What is
**not** yet shown is that the hole is merely *newly reached* rather than newly created: the control
(`20261009-142848-669-title011-R3-control-5fd62cb`, the pre-cache toolkit, 900 s) ran clean but reached
only **1320** presents, still on the Dolby card, against R3's **2885** at its crash — **inconclusive**
(TR §25.5).

**THE PRESENT-RATE WALL IS FLIP FREQUENCY, NOT RASTERISER THROUGHPUT — a correction to TR §24.3.**
The guest's own flip counter (`flips == presents` exactly, so it is the swap count and not a model
proxy) collapses from **~2.77 /s** early to **0.035–0.047 /s** in the tail — a **59–80× collapse
that reproduces across four runs and three binaries** — while `[GPU] draws` and triangles keep
climbing at ~485 draws and ~28 000 triangles per 10 s report. And **draws-per-flip is stable**
(542.6 middle vs 541.9 late in `clockfix-1800`), which means **one flip per frame**: the guest does
swap, once per frame, and the frame rate itself has collapsed to ~0.04 frames/s. So the rasteriser
explains frame *content*, not frame *count*.

**THE PRESENTER IS USUALLY RIGHT, SO THE DRAW→PRESENT QUESTION IS RE-FRAMED.** At **324 of 370**
traced flips (87.6 %) `present_track_flip` hands the window the draw surface `0x80084000`, always by
`reason=drawn_this_frame`. The plan's "the presentation path sends `0x8011C000`/`0x801B2000`" is
true of `clockfix-1800`'s end state (its city hash `31b1469f9c922c32` appears **0** times among 711
distinct published hashes) but is **not** a general property. Structural gaps remain and are
recorded, not fixed: the guest's own `flip_read`/`flip_write`/`flip_modulo` index
(`flip_modulo=3`) is stored and read only by two debug prints, never by `present_track_flip`; the
report's "draw surface" line prints the *same variable* the presenter prefers, so it can never
reveal a selection error; and the commit consumer drops every non-NV097 class (14 methods in
`clockfix-1800`) with a count that could not name a blit — `NV_IMAGE_BLIT` (0x9F) is now identified
explicitly by a census added this turn (toolkit `5fd62cb`).

**Also corrected:** the Planner's "WHOLE-GUEST STALL" class (`long3d`, `units`, `units4`,
`blacktrace`, `trace-noadmit`, `admitted39`, `admit3`) is **wrong**. Measured from the rings, the
main thread is the **most recently active thread in the capture** (lag 0.00 s, 128 events in
0.3–0.4 s) and `[FBPRESENT]` continues to within **7–18 s** of the actual run end (long3d: 697 s of
704.4 s). Those are ordinary live runs whose workers are parked 13–25 s behind main.

**Instrumentation added this turn** (toolkit `2be32ad`, `e487f78`, `5fd62cb`, `5d6ebbd`; game
`9526ea2`): a process-start-relative monotonic clock (`nv2a_mono_clock.h`) stamped on the worker
spawn, heartbeat, return and wait events — because `[CHECKPOINT]` cannot be a timeline (a whole
1800 s run has **3** checkpoint lines, all in its first 78 lines) and `[FBPRESENT] t=` is
first-present-relative; vblank re-arm/pass/gap telemetry published by the ptimer thread itself so it
cannot freeze with the walk; owner-lock hold/wait maxima with the holder's tid; and a bounded
`[WAIT]` print with **unbounded totals** (the print cap saturated at 10.4 % of a run before that was
fixed). Two independent clocks agree to **0.08 s** on R1's worker/main liveness gap, which is what
makes the new telemetry comparable to the archived rings.

**Not established, and deliberately not claimed:** that the cache fix raises the heavy-phase flip
rate (R3 crashed before that phase, and its matched-time gain was 2.6–6.7× against a predicted
~10×); that the `0x9188C` hole is newly reached rather than newly created; the in-game average walk
length, so the walk's share of wall time is a **bound**; and whether the vblank shortfall slows the
guest's *pacing* — note **presents exceed vblank pulses** on R1 (2458 vs 1264, ratio **1.94**), so
presents are **not** 1:1 gated on vblank and the shortfall does not gate the renderer.

**M15 is NOT reached.** No title screen — no emblem, no `PLEASE PRESS START TO BEGIN` — has been
observed on the recomp.

## Current work (state 2026-10-08, turn title-009) — SUPERSEDED by the section above

Condensed 2026-10-09 (474 lines of a closed turn's narrative, including the title-008 blocks it kept
for provenance). The full text is `git show 2a580f7:plan-jsrf-bare-minimum.md`. Its facts live in the
TR: §22 (`0x1810`, the six methods, the 1000 → 888 count), §23 (M15 criterion, `0x84000`, the 39/29/38
admissions, capacity bound, same-flip trace and ring, viewport constants §23.8, the indirect-call
recoveries §23.9-§23.12), §24.1 (`budget_exhausted`), §24.2 (clock overflow; its worker-death
reading is withdrawn by §25), §24.3 (city drawn but not presented, the 93.8 % regime; its
present-rate wall is corrected by §25.6) and §25 (turn title-010). Ledger: L39, L40, L46-L49, L50-L55.
Items that were still open moved into "Next actions, in order" and "Backlog" below; settled results
are the "Settled — do not re-run" list. The turn-by-turn summary is in
`docs/jsrf-operating-history.md` ("2026-10-09 — plan condensed again").

## Fast path to the title screen

| Step | State | Acceptance / pointer |
|---|---|---|
| F0–F0b | done | 15 GB disk gate; copy `src/recomp/` aside before pulling an untracking commit; rebuild and test on a new toolkit before a run |
| F1 | done 2026-09-30 | `RECOMP_RDATA_GUARD=1` named the kernel-thunk-table writer (D5) |
| F2, F2b | done 2026-09-30 | D1 (DMA_PUT bit 16), ML3 (file flags) |
| F3 | done 2026-09-30 | strict horizon closed: two wrong `tail_jump_alias` folds recovered |
| F4 | met (exploratory) 2026-10-01 | SEGA card renders through the owner's NV097 consumer (toolkit `a71f937`) |
| F4b | done 2026-10-04 | the no-op `fcmove`/`fcmovne` lift held the SEGA fade; toolkit `671ab0a` translates them |
| F5 | parked | intro movies: if Sofdec blocks, skip and ledger it (*patched*/*intentionally ignored*); decoding is M29 |
| F7 | done through stop 28 | the dispatch stop chain above; residue in the backlog |
| **F8** | **cleared 2026-10-06** (toolkit `505cda5`) | the present ceiling: `unsupported_method 0x1810`, one stale table entry; the table now carries it and a run with neither the live-mirror nor the admit-unknown switch reaches presents 2410 with zero rejections (TR §22) |
| **F8b** | **done 2026-10-07** (toolkit `46b3265`) | the same class: the six runtime-witnessed methods `0BB0`/`0BB4`/`0BB8`/`0BBC`/`1724`/`1728`, admitted from the `[PFIFO] admit-unknown` witness (+6 on class `0x97`, zero removals). Ordered delivery pinned by a mutation-validated contract. Whether the post-admission run exercised them is **not demonstrated**: zero rejections is not proof (the pre-admission run also had zero), and the present counts were at unequal durations on different binaries — a matched-duration A/B on one binary is owed (Backlog) |
| **F8c** | **admission CLEARED 2026-10-07** (toolkit witness-capacity fix) | the 39 NV097 methods are now **runtime-witnessed**, not decode-derived: the `[PFIFO] admit-unknown` witness was truncated at 16 by its own cap and is fixed, so one run logs 39. Table regenerated **+39, zero removals** (NV097 376 → 415). A no-switch run then moved the stop from `unsupported_method 0x0420` to **`sink_capacity`** (TR §23.3). **Corroborating decode is the NO-ADMIT run's report (39), not the witness run's (35)** — Turn Review corrected the earlier citation |
| **F8d** | **capacity bound CLEARED 2026-10-07** (toolkit `1f86fbb`, `e85f331`) | the submission walk now commits in **units at whole-packet boundaries** (per-unit atomicity, L40), so a 5732-word submission is consumed instead of rejected. **Measured:** GET advanced `0x50B1C` → `0x5A060` (9553 words, 2.33 budgets) and the stop moved off `sink_capacity`. **Three real defects were found by tests/review, not by inspection:** `sink_count` was reset once per walk so a second unit overran `sink[]`; the old header check bounded a unit by a submission-wide count so it fired before the yield; and `admitted_unknown` re-added every earlier unit at each unit commit (published 10227 for 6137 — found by Turn Review, fixed in `e85f331`). Raising the 4096 cap was NOT the fix |
| **F8e** | **CLEARED — the black was the missing vertex-program viewport constants** (toolkit this turn, TR §23.8) | the XDK vertex programs end with `MUL o0.xyz = r12 * c[58]` / `MAD o0.xyz = r12 * r1 + c[59]`, and **the executor never loaded `c[58]`/`c[59]`**: `0x0A20` reached only the fixed-function `s_gpu.vp_offset`, and **`0x0AF0` was unhandled entirely** (`x28024` in the report's unhandled top ten). With both slots zero, **every vertex-program vertex collapses to the screen origin**, the triangles have zero area, and the batch draws nothing — so `0x84000` was cleared to black each frame and never refilled, and the composite then faithfully copied black. **Fix:** `0x0AF0-0x0AFC` → `s_vp.c[58]` and `0x0A20-0x0A2C` → `s_vp.c[59]` as well as `vp_offset`; plus stage 0's `SET_TEXTURE_CONTROL0` enable bit honoured. **Measured** (`20261007-124149-033-title008-d1-viewport` vs the pre-fix `…ring-admitted`; the two also differ in trace budget and guest path, so this is a before/after with confounders): distinct `[FBPRESENT]` hashes **10 → 658**, `0x0AF0` gone from the unhandled list, the transition window (flips 2426–2440) going from one repeated black hash to **15 distinct hashes** (15 observations; 2425 and 2441 remain black); `0x80084000` = `efddce3b5bb02ab1` (93.5 % non-black, 2677 colours) renders a **full 3D city scene**, and `0x8011C000` = `eaaa65df05fa3144` (100 % non-black) a **"Now Loading"** screen. **Not M15** |
| F6 = M15 | open | **criterion corrected 2026-10-07** (TR §23): the title screen itself, identified by CONTENT and confirmed by eye, with its hash recorded — **not** "a hash that is neither of two listed ones". The old blacklist was unsound: `87683a748e27d071` is the blue Dolby card, and the graffiti disclaimer renders in **four** hashes, three of them unlisted, so a run ending on the disclaimer (as the ceiling-clearing run did) satisfied the old letter. **An xemu reference of the real title now exists** (`logs/workers/title007/xemu/deliverable/`: emblem + "PLEASE PRESS START TO BEGIN" over a perspective city street, 640×480, reached with no input) and is the comparator. Also needs the run record with ledger IDs and Turn Reviewer reproduction |

## Next actions, in order

−1. **Done 2026-10-09** (TR §1 "v0.13.1 sync"): the merge builds and passes every suite on Windows; the
   A/B shows no crash and no functional regression in 300 s but a pixel-fill slowdown (next action 2);
   the R3 control ran and was inconclusive (next action 0).

0. **`0x9188C` is closed (stop 32, TR §26.4); find why the merged build takes the game's fatal path at the
   transition, then test the flip-rate prediction.** The first run on the repaired tree stopped at presents
   2429 in `JSRF_FATAL.ERR` without reaching the address. With `RECOMP_NO_COMBINERS=1` there is no fatal
   path and `title.adx` is read, but presents then hold at 2435. That is the old "Now Loading" hold, which
   R1, R2 and B also show; only R3 ever passed it (TR §26.4). Two questions remain:
   - why the merged executor with combiners on takes the fatal path;
   - what releases the hold. R3, the one run that passed, had ~49 vblank pulses/s and zero late re-arms,
     against ≤ 23 /s elsewhere (INFERRED, n = 1). A run with vblank delivery restored, e.g. the fill taken
     off the owner lock, would test it.

   Background: R3 ended at 236.5 s on the fatal
   unresolved call `0xE0424943` at VA `0x9188C`: `sub_0009188C` is a generated stub
   (`recomp_icall_fail_log(...); abort();`) reached from `body_00091830`, and TR §20 already records
   `0x91830`'s internal target `0x9188C` as ordinary code, not repaired — no walk/cache frame is on the
   stack. The pre-cache control (`…142848-669-title011-R3-control-5fd62cb`) reached only 1320 presents
   in 900 s against R3's 2885 at the crash, so it cannot attribute the hole (TR §25.5); a no-cache control
   would need several times longer. A run built from the repair commit that passes R3's crash point
   confirms stop 32 and settles the cache attribution. The pre-registered prediction (heavy-phase flips/s up ~10×; R3's matched-time gain was 2.6-6.7×) is
   untested.
1. **A city scene is drawn but not presented — the structural draw→present questions** (TR §24.3 as
   corrected by §25.7). On the fixed binary `0x80084000` holds a full 3D city
   (`logs/workers/title009/clockfix/0x80084000.png`), hash `31b1469f9c922c32`, published in **0** of
   711 distinct hashes of `clockfix-1800`; `efddce3b5bb02ab1` (93.6 % non-black, 2677 colours) is
   published in none of 188 archived runs. But the presenter picks `0x80084000` at 324 of 370 traced
   flips (`drawn_this_frame`), so "wrong source" is not the general case. Measure before choosing among:
   the guest's `flip_read`/`flip_write`/`flip_modulo` (=3) index, stored and read only by two debug
   prints and never by `present_track_flip`; the report's "draw surface" line printing the variable the
   presenter prefers (so it can never show a selection error); and the commit consumer dropping all
   non-NV097 classes (14 methods in `clockfix-1800`), with `NV_IMAGE_BLIT` (`0x9F`) now named by the
   census (toolkit `5fd62cb`). The instrument is the same-flip trace (`RECOMP_FLIP_TRACE`, L47). One
   concrete alternative is still unresolved, not a demonstrated defect: at flip 2425 an earlier
   composite writes `0x11C000` sampling `0x84000`, then a later `self=1` batch writes `0x84000`, and the
   tracker selects `0x84000` because it prefers the last surface drawn.
2. **Executor throughput and frame count.** The present-rate wall is flip frequency: `flips == presents`
   falls ~2.77 /s → 0.035-0.047 /s (59-80×, four runs, three binaries) while draws keep climbing and
   draws-per-flip stays at ~542, i.e. one flip per frame (TR §25.6). **Measured on the merged executor
   (TR §26):** `[GPU] executor time` puts **pixel fill at 207 s of 218 s busy** in a 300 s run (vertex
   programs 46 ms, triangle setup 41 ms), ~1.3-1.5 billion pixels from ~13-15 k mostly full-screen
   triangles, ~110-160 ns per pixel, all under the owner lock (late re-arms 28.8 %). The pre-merge
   executor wrote at least as many pixels and reached 1750 presents at t = 120 s against 940-960. Levers when it
   gates M15: take the fill off the owner lock, a cheaper per-pixel path for the composite/blur passes,
   or the raster pool's thresholds. The owner's direction: performance on a non-working prototype is not
   a blocker by itself.
3. **A run must reach past presents 2424 to be informative.** The disclaimer is a timed hold, presents
   1461 → 2424 (t≈408-420 s); the 300 s `just title-run` ends at presents ~1450-2435 (2210-2240 on the
   merged executor), at or before the transition. Use ≥540 s.
4. **Hold: no runs on the dispatch stop chain** until the ceiling path is finished. Stop 28 (`0x81860`)
   is still not exercised by any run (`scripts/check-run-exercised.py` reports NOT EXERCISED for every
   post-fix run), so `config/stop-chain.json` is unchanged.
5. **When a new scene appears, read the present lines before trusting the window.** The present choice
   falls back to the buffer the frame cleared or targeted and logs `[FBPRESENT] presenting targeted 0x…`
   the first 8 times, so a targeted frame can show only its clear colour; an unchanged or blank picture
   is not proof the guest did not advance. On the merged executor vertex-program batches are rasterised
   (`raster_batch_program`), so `batches_untransformed` in the `[GPU]` report now counts only batches with
   no program loaded or non-screen-space fixed-function vertices; re-read it on the first `409c635` run
   rather than carrying the pre-merge reading forward. Watch the `0x0680` `composite_set` latch (TR §23.3): it is
   never cleared, so a later 2D overlay sent as screen-space vertices could be transformed as 3D and go
   missing.
6. **M15** as F6 states — the title screen by content against the captured xemu reference — then M16
   onward.

### Settled — do not re-run

- **ADX "worker deaths" — retired** (TR §25.1-§25.3). Live threads parked on the D3D vblank event
  `0x0019D630` (`xbox_KeWaitInplaceEvent` ← `sub_0018CE50` ← `body_0013B1C0`; 256 `[WAIT]` lines, all
  `INFINITE`); the detector keyed on exact `esp` against a set containing the main thread `0x007BFFCC`.
  By owning stack: wrapped 8 L / 16 K = 33.3 %, unwrapped 0 / 60 = 0.0 %. R1 (`…155241`) and R2
  (`…185536`) each crossed an unsigned QPC wrap and kept every worker (Case D).
- **Withdrawn, re-derive before citing** (TR §25.9): the "6 kept / 7 lost" lists, `death_vs_onset`, "0 of
  16 within ±5 s", 41.7 % vs 12.3 %, and anything keyed on exact `esp` or `[KERNEL] summary` cadence.
  The Planner's "whole-guest stall" class is wrong (TR §25.4).
- **Clock overflow** — fixed on its own merit (`nv2a_qpc_to_ns`, L53, TR §24.2); signed wrap 922.337 s,
  unsigned 1844.674 s. Timing comparisons must align epochs first: `QPC − GetTickCount64 = +4.2 s`
  (TR §24.2); `[FBPRESENT] t=` is first-present-relative and `[CHECKPOINT]` is not a timeline (TR §25.8).
- **Walk `VirtualQuery` per word — fixed** (toolkit `5d6ebbd`, TR §25.5): late re-arms 69.1 % → 0.0 %,
  max owner-lock hold 1636 → 68 ms, max vblank gap 13 995 → 78 ms, presents at t=120 s 628 → 2568.
- **`budget_exhausted` — Case A, benign resumable chunking** (TR §24.1, L54): the classification rests on
  the per-stop recovery lines of control `20261008-032308-212-title009-cap1024` (17 stops, all drained,
  verified by Turn Review). The 900 s run's 63 matched / 0 mismatched came from the pre-repair,
  near-vacuous audit and carry no weight; the repaired-binary run and the cap-scope livelock are in the
  Backlog.
- **93.8 % "no usable stage" regime** (TR §24.3): `clockfix-1800` reports 4187/4187 untexturable batches
  as `stage disabled`, zero as `no offset` / `no dimensions` / `unusable format` / `other`. The counter
  cannot tell "guest disabled" from "model cleared or never received the bit" — that discriminator is in
  the Backlog; do not re-run the same counter.
- **Black interval — cleared** (TR §23.8, L48): vertex-program viewport constants `c[58]`/`c[59]` from
  `0x0AF0`/`0x0A20`, stage 0 enable honoured; distinct `[FBPRESENT]` hashes 10 → 658.
- **Method admissions — from a runtime witness only, never a decode** (`config/nv2a-runtime-witnessed-methods.json`;
  the generator diverges from the walk, TR §22): `0x1810` (F8); the six `0BB0`/`0BB4`/`0BB8`/`0BBC`/`1724`/`1728`
  (370 → 376, TR §22.1); 39 (376 → 415, TR §23.3); 29 `0580-05AC`/`06C0-06FC`/`1964` (415 → 444, L46);
  38 `1A30` class (444 → 482, L49); 25 `0298` class (482 → 507, L52, TR §23.12). Every delta mechanically
  verified: zero removals, no other class changed. Capacity bound cleared in whole-packet units
  (toolkit `1f86fbb`, L40, TR §23.4).
- **Three indirect-call recoveries** `0x00159330` (`ret 0xc`, `stack_args 12`), `0x000C2730`, `0x000C3410`
  (stops 29-31, L50/L51, TR §23.9-§23.12): the abutting-alias pair needed recovery **and** a narrowed
  container end (`0x000C2700` → `0x000C272D`, `0x000C33C0` → `0x000C3408`) and are exercised; run
  `20261007-235846-887-title008-c3410-fixed` ran 900 s with zero `[ICALL] Failed`.
- **Input is not the blocker** (TR §23.1): xemu reaches the full title with no input and the recomp boots to
  the transition without it. Delivery is dead anyway (`xbox_OhciInit`, `xbox_InputInit` never called) — M16's subject.
- **Eliminated premises** (TR §23.1, §23.2): the `sub_0019E438` "spin" is DirectSound lock traffic (16356
  calls all return 1; enter/leave 24702/24702); `RECOMP_FB_VA=0x8011C000` composites are accumulation
  artifacts and pinned vs unpinned hash counts are not comparable (`fb_present.c:66-69` early-returns);
  `0x11C000`/`0x1B2000` are cleared (`clear surface` 45× and 43×; the clear-colour line prints only the
  first 8 distinct colours, `seen[8]`).
- **Same-flip trace** (TR §23.6, §23.7): 242/242 flips published the selected surface's own bytes
  (`drawn_this_frame`) — copy/publication consistency only; "Case C" and the "uniform 16-batch frame" are retracted.
- **Presenter is usually right** (TR §25.7): 324/370 traced flips select `0x80084000`.
- **M15 criterion** (TR §23): content, not a hash blacklist — `87683a748e27d071` is the Dolby card; the
  disclaimer renders in four hashes (`5bdaea576b8509f5`, `089fe3b826bbc18d`, `cf836ec8430ffb6d`, `8205f3a6d2e48df5`).
- **Dispatch stop chain** cleared through stop 27; stop 28 is repaired but not exercised (next action 4)
  (`config/stop-chain.json`, L43); static gates are in `just check`.

## Backlog (not on the critical path until it blocks)

- **From the 2026-10-09 Mac review — need the XBE or a Windows run:** `config/recovered-functions.json`
  entry `0x000C3410..0x000C3670` contains the separately recovered `0x000C3500..0x000C3670` (two bodies
  over one range — the `0xC2700` class; run `check-hidden-entries.py` and disassemble before choosing
  which end is right; a config edit needs a provenance amendment); `config/stop-chain.json` stops 29–31
  have states that contradict their notes (stop 30 `REPAIRED` with `repair_commit: null` while stop 31 says
  it was exercised; stop 29 says the repair is committed but names none); the `0x000C2700` evidence text
  still quotes the pre-tightening body end; `tests/test_nv2a_hal.c` vertex-offset asserts compare raw
  offsets that `dma_resolve` may rewrite. The stop-chain rows need evidence rows that
  `check-stop-chain.py` verifies against the run archives; their repair commits are stop 28 `49bfd4e`,
  stop 29 `0cba32a`, stops 30–31 `d63e792`. Toolkit, Windows-only: the long-wait instrument
  (`kernel_bridge.c`, `bridge_log_long_wait`) logs only after a wait returns, so a permanently parked
  thread is silent — publish an "in wait since T on X" slot.

- **Packet-cap zero-commit livelock** (L40, L54, L55; TR §24.1). `unit_words` resets per unit but `packets`
  never does, and `if (packets >= 1024)` runs before the yield, so a packet-dense stream reaches the cap
  with zero units committed; GET is assigned inside `if (ok)`, so it pins and every retry repeats the
  walk. Measured: `20261008-041233-960-title009-cap128` stopped on the first boot submission, GET 0
  through submits #0-#63, zero `[FBPRESENT]`, `successes=3`. The shipped margin is exactly zero
  (`walk_packet_max = 1024` vs a boot kick of 1024 words). The fix (per-unit yield plus a real cycle
  guard, or accepting the margin) needs an L40 amendment and is deferred; `budget_exhausted` fired 12
  times in the 25-method witness run vs once before, so track its count per run first. A run on the
  repaired binary is still owed to confirm `budget_resume_stalled` behaves as designed at scale (TR §24.1).
  The "`ret` lost across a rejection" hazard is latent (`ret` is 0 at every latched stop).
- **Stage 0 is disabled by the guest** (TR §24.3, TR §26): the CONTROL0 latch shows 16230 stage-0
  `SET_TEXTURE_CONTROL0` writes, 7581 with ENABLE clear, last `0x00000000`, so `stage disabled` is guest
  intent, not a lost bit. Still open: what the disabled-stage batches should draw, and whether the city
  scene should have been published from another surface.
- **Inline per-vertex diffuse colour dropped** (render gap behind the `0x1A30` admission): attribute 3
  component 0 of the 4-float inline vertex family was acted on only for `attr==0` and `attr==9`
  (`nv2a_pb_exec.c:2922-2934` before the v0.13.1 merge — re-check). Admitted methods the executor only
  captures: `0x03A8-0x03BC`, `0x0A10-0x0A18`, `0x1000-0x103C`.
- **`0x00159330` runtime confirmation absent** (TR §23.12): `check-run-exercised.py` reports it not
  exercised, so its stop-chain row stays `REPAIRED` with no `repair_commit`. Latent `OVER_RUN` siblings of
  the abutting-alias class, recorded not fixed: `0x000C002C-0x000C004A` (body ends `0x000C003F`, next
  prologue `0x000C0050`) and `0x000CD890-0x000CDAC0` (body ends `0x000CD8AE`, next function `0x000CD8B0`);
  the gate does not call them failures and no run has reached them.
- **Same-flip trace limits** (TR §23.6, §23.7): timing non-perturbation is unqualified (no matched
  trace-on/off control); candidate cap 8; hashes use the current clip/pitch, the bound-texture hash the
  surface geometry; `_CHANGE` is a decision-only key; hashes are taken at flip time, after every batch, so
  "source was black" and "source was cleared after the read" are indistinguishable and
  sampling/UV/blend/ordering are reopened (largely moot given TR §23.8, but not positively established);
  `flip_modulo=3` proves a flip-index ring, not three scanout-buffer roles.
- **F8b exercise** (TR §22.1): whether the six admitted methods are exercised, and that their admission
  did not regress presents, needs a matched-duration A/B on one binary or a contemporaneous transcript.
- **Not explained:** the `Now Loading` hold in archived runs — the clock overflow is fixed (L53) and the
  workers are parked rather than dead (TR §25), but what held the loading loop is not shown; and the
  1000 → 888 present-count change, a hypothesis consistent with the overwrite-rescue mechanism (TR §19, §22).

- **Stop-chain residue:** 210 span-exit `CUT-TARGET` (TR §20), 24 `KNOWN_OPEN` (`tests/test_recovery_span_ownership.py`), 67 SUSPICIOUS,
  `0x96F80` (`UNQUALIFIED`, named proof gap, TR §18), the 10 dead alias shims (a latent hazard, not a
  live defect, TR §18), the alias-shim census's actionable classes (12 `SWALLOWED_FUNCTION`, 59
  outside-owner shims; census commit `0cc6d5d`, counts in `git show dcc93ab:plan-turn-updated-title-004.md`
  "B5"), and the `gap_prologue` root cause (needs a full regeneration; batch the 12 sibling spans, each
  verified by decode before landing).
- **Toolkit review findings (2026-10-06): exercised on Windows this turn.** The four `dc04dc0`
  behaviours were judged: the **fence mirror** holds back-pressure as designed (a rejected walk leaves
  GET pinned and the guest in the ring-space wait, with a `[PFIFO] reject` line present) and the
  `RECOMP_FENCE_MIRROR_LIVE=1` A/B reproduces the overwrite-rescue mechanism, so the 1000/888 count
  is **not** evidence of a regression in the new mirror (TR §22). CTest
  `kernel_file_apc_test` (its `STATUS_USER_APC` assertions), `kmem`, `fence_snapshot`,
  `nv2a_present_track`, `nv2a_submit_diag` all pass; `just test` 44/44 and `just check` clean. Still
  open: the `[APUWAIT]` line cap, and **stop 28 (`0x81860`) is still not exercised** by any run.
  **Fence-ordering audit done (static, read-only):** `0x1912A0` has exactly 5 direct callers, found by
  two independent methods; no raw dword spelling of the address was found in the image, which is **not**
  proof that a computed or indirect call cannot exist.
  The literal comment "D3D advances the counter before it writes PUT" is **false for two of them**
  (`0x19167C` on its `0x19165C` path, and the `0x192440` bring-up kick), but neither can manufacture a
  false ceiling: `0x191390` returns the **pre**-advance counter as the fence and then advances by 2, so
  the live counter is always ≥ any recorded fence. A real staleness path exists in principle —
  `0x191390` with arg bit 1 advances *without* kicking, and `nv2a_retry_stalled_walk` can commit with no
  new kick — but it is not what happened here, and it is distinguishable at runtime (a lagging mirror
  produces no `[PFIFO] reject` and no rise in `consecutive_rejections`; a rejected walk does both).
- **Undiagnosed:** the XBE `DOLBY` section being written although marked read-only (section protection
  is not enforced; seen with the `0xFFC00000` fill, which TR §9 explains as the `0x32610` misdispatch,
  closed by its recovery), and the second cause of the disc-error dialog at ~950 s with no OOM
  (f5-long; TR §8) — the L41 caller logs are in place for the next long run.
- **Carried chores:** re-cite `P0.1-AC1` by symbol (its line numbers moved); the DSP provenance record
  (the A4b2-NR instrumentation is not listed).
- **Carried technical items:** C2 kernel-memory follow-ups, C4 D3D resource release, C6 `/we4013` on
  Windows, T16's `--coalesce-functions` half, T19 retail-byte oracles, ML4 APU interrupt delivery,
  ML7 D3D11 renderer (cel shading for M19–M20), GP port follow-ups and the AC'97 registers
  `0xFEC0017C`/`0xFEC00100` before M23, `PIO_FREE` (deferred at `O-OPEN`, TR §4).
- **Owner items:** removing the lifted code from history needs a force-push
  (`docs/reviews/lifted-code-history-scrub.md`); the repository licence; W8 fallback routes.

## Definition of done — the minimum playable slice

All of the following under **any** run profile, provided every path the result relies on that is not
*emulated* or *translated* is in the ledger and the run record lists those ledger IDs.

| DoD | Criterion | Measured by |
|---|---|---|
| DoD-BOOT | Launch to the title screen; the title frame is presented. | M15 |
| DoD-INPUT | A host controller drives menu navigation through the guest's own XAPI input path. | M16–M17 |
| DoD-PLAY | New game loads the opening area; the player skates, turns, jumps and the camera follows. | M18–M21 |
| DoD-GRAFFITI | One graffiti interaction completes and the game's own progression state records it. | M22 |
| DoD-AUDIO | Sound effects and music are audible at the correct pitch. | M23–M24 |
| DoD-SAVE | Save, exit, restart and resume to the saved location/progression; a missing save is handled. | M25 |
| DoD-STABLE | A 15-minute soak with no invalid indirect call, no ABI violation and bounded memory. | M26 |

A window opening is not the slice; one playable scene is not the game.

## Milestones (M00–M06 done; 06b closed into the fork fixes)

Any profile is acceptable for these; each acceptance record lists its ledger IDs. "xemu ref" is a T3
capture of the same checkpoint. SSIM/correlation thresholds guide the judgement; they are not a strict
gate.

| M | Milestone | Acceptance (artifact → PASS) | Status 2026-10-06 |
|---|---|---|---|
| 07 | CRT and game initialization | `verify-initializers.py` passes; the main-loop entry is reached; zero invalid indirect calls / ABI violations | boot reaches the logo loop (exploratory); not accepted |
| 08 | Paths and first real asset read | a `Media` read whose byte count and hash equal the retail file; a missing file returns `STATUS_OBJECT_NAME_NOT_FOUND` | reads succeed (`title.adx`, cache files); criterion not measured |
| 09 | Allocation and ownership | `[KMEM] summary` over a boot: `commit_rejected=0`, `release_failed=0`, `region_table_full=0` | heap bookkeeping fixed (toolkit `dbeb284`), zero OOM since; not measured |
| 10 | Timers, threads, synchronization | `[GMETER] anomalies=0`; every boot-path wait satisfied by a modelled cause | open |
| 11 | Graphics interception decided | the decision recorded; a fixture feeds committed methods to the back end | **decided**: executor path (C5, L16/L18) |
| 12 | Window and clear | ≥ 60 presented frames whose clear follows the guest's own clear methods | logos presented (exploratory); not accepted |
| 13 | One game-owned UI primitive | first UI frame vs xemu ref, SSIM ≥ 0.95 on its bounding box | open |
| 14 | One menu texture | decoded texture bytes equal the xemu ref dump or an independent decode | open |
| **15** | **Title screen** | 60 consecutive frames vs xemu ref (SSIM ≥ 0.90); intro FMV handling stated | **open — F8d (capacity), then F6.** The xemu reference is captured (emblem + "PLEASE PRESS START TO BEGIN" over a perspective city street, 640×480, no input needed); the recomp's 3D scene does **not** match it yet |
| 16 | Controller input | with `RECOMP_USB`, a host pad press produces the guest's own XID report and the title's input state changes; `RECOMP_PAD_PRESS` is not admissible | open (toolkit-provided: verify, not build). **Not on M15's critical path:** xemu reaches the full title with no input |
| 17 | Main menu navigation | host pad drives start/options/back; the menu-state global changes | open |
| 18 | New game loads the opening area | loader completes; files read archived with sizes and hashes; object count > 0 | open |
| 19 | Opening scene and character | spawn frame vs xemu ref SSIM ≥ 0.90; depth/transforms at three camera positions | open (needs ML7 or executor shading) |
| 20 | Distinctive rendering | cel shading and outlines at three views, SSIM ≥ 0.90 | open |
| 21 | Skating and camera | position follows a held stick; jump and landing; frame-time p99 ≤ 2× median | open |
| 22 | One graffiti interaction | tag-completion flag set and paint count decrements (named globals) | open |
| 23 | Sound effects | APU output non-silent at rate; one effect's PCM correlates ≥ 0.9 with an xemu capture | open |
| 24 | Music and streaming | 5 minutes of music, underrun counter unchanged, memory bounded | open (JSRF streams ADX) |
| 25 | Save and resume | isolated save root; restart resumes; missing save handled; a regression test covers save enumeration | open |
| 26 | Stability | 15-minute soak: zero invalid indirect calls / ABI violations, bounded `[KMEM]`, `[GMETER] anomalies=0` | open (DoD-STABLE) |

Graphics, input and audio may be reordered when the boot path demands it. **After the slice
(27–36, scope unchanged):** area transition, second character and challenge, cutscenes and FMV
(Sofdec), all areas, missions, full playthrough, optional content, Windows hardening, performance,
reproducible package — each gets criteria in the same form when it becomes next.

## IDs other files cite

Records, scripts and the TR cite these IDs; their full text is in
`git show dcc93ab:plan-jsrf-bare-minimum.md` (old section numbers §0–§14 likewise).

| IDs | What | State |
|---|---|---|
| V1–V5 | Phase 0 re-baseline on Windows (build, regenerate, strict re-baseline, read-only checks, recovered-functions audit) | done 2026-09-29/30 |
| T1–T14 | Tooling: TTD (T1), XbSymbolDatabase (T2), xemu oracle (T3), Windows CI (T4), clang-cl//analyze (T5), `just` (T6), pre-commit (T7), DuckDB `logq` (T8), access enumerator (T9), citation lint (T10), review capture (T11), per-run doctor (T12), startup receipts (T13), disk gate and retention (T14) | done; T11/T13's scripts retired with the packet lifecycle (2026-10-03); T8 gained the present/PFIFO tables 2026-10-06 |
| T15–T19 | Mercenaries-derived tools: generated-body parity/overlay (T15), `just regen` inputs (T16), content-sniffing asset guard (T17), post-generation patches (T18), retail-byte function oracles (T19) | T15, T17, T18 done; T16 half done (`--coalesce-functions` needs a Windows regeneration); T19 not started |
| W1–W16 | Workflow checks tied to measured failure patterns | done, each with its check; W8 (fallback routes) owner-reserved; the packet-only checks (W1, W3/W10, W6, W7, W13) retired 2026-10-03; W15 gained the reverse direction 2026-10-06 |
| C1 | Attribute the kernel-thunk-table write | superseded: F1 found the writer (D5) |
| C2, C4 | Kernel-memory follow-ups; D3D resource-release re-check | open, backlog |
| C3 | NV2A action methods (L19) | parked until after the slice |
| C5 | Rendering architecture | decided for the bare minimum: the executor path (L16, L18) |
| C6 | Implicit declarations | declarations in; `/we4013` and its MSVC build on Windows remain |
| C7, C8 | A4b2 P4 transfer bridge; upstream/fork cadence | carried; chore on each upstream release |
| ML1–ML10 | Lifts from Mercenaries-Recompiled (MIT root; its `src/apu`, `src/nv2a` are xemu-derived LGPL; never its xemu DSP oracle) | ML1 serial guest mode done, opt-in (L34); ML3 file I/O done (L30, L31); ML4 mostly done (APU interrupt delivery open); ML2 heap replacement, ML5/ML6 fallbacks, ML7 D3D11 renderer (for M19–M20), ML8 ISO tooling, ML9 input/options (after M15), ML10 unified physical memory — not started |

## Tooling for the fast path

`just --list` is the authority on recipes; these are the ones the next actions use.

| Question | Tool |
|---|---|
| Build, test, check | `just build`, `just test`, `just check` |
| A comparable title-path run | `just title-run <label> [seconds]` (sets `RECOMP_APU_TRAP`, `RECOMP_PB_EXEC`, `RECOMP_FB_WINDOW`, `RECOMP_FB_PRESENT_DUMP_EVERY`) |
| Why the GPU stopped consuming | `just gpu-report <run>` (pending stream, missing methods, predicted diagnostic, `g_nv2a_submit_state`); `[PFIFO] reject`/`still rejecting`/`recovered`/`admit-unknown` log lines |
| Present counts and PFIFO history without hand-grepping | `python -X utf8 scripts/logq.py <run> --saved present-ceiling` (tables `presents`, `pfifo`, `gpu_flips`; `just logq <run> "<sql>"` for ad-hoc SQL) |
| Did a run exercise an address | `scripts/check-run-exercised.py` |
| Span and ABI defects from the bytes | `just stack-depth`, `just hidden-entries`, `scripts/check-entry-extents.py`, `scripts/check-table-targets.py`, `scripts/check-span-exits.py` |
| Reading a dump | `scripts/check-dump-mapping.py` first, then `scripts/inspect-jsrf.py`, `scripts/read-host-symbol.py` |
| The xemu oracle | `scripts/xemu-oracle.py`, `scripts/xemu-gdbstub.py`, `scripts/xemu-diff.py` (T3) |
| Admitting new NV2A methods | `scripts/gen-nv2a-method-inventory.py` (pass `--put=`) |

## Owner decisions in force

- The bare minimum is pragmatic, every shortcut ledgered (2026-09-30).
- Lifted code (`src/recomp/gen/`, `recovered.c`) is untracked and rebuilt locally (2026-09-30).
- Ordinary runs need 15 GB free (`scripts/check-disk-gate.py`); TTD recordings keep the 50 GB
  expectation; deleting old runs is an owner decision.
- The strict-horizon ledger's scope is from 2026-09-29 onward (`--since all` for the archive).
- C3 is parked and C5 decided (the executor) for the bare minimum.
- The packet lifecycle and its tooling were retired (2026-10-03); existing packets, reviews and rulings
  stay as history.
- Commit and push policy, and the public-repository rules, are in `AGENTS.md`.

## Evidence rules (kept)

- `MEASURED` = inspected source or artifact with an identity and procedure; `INFERRED` = a hypothesis.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS; a failed
  measurement cannot be converted to PASS by any role; a post-review change reopens the criterion.
- Bare-minimum milestones accept exploratory runs whose records list their ledger IDs; a *fidelity*
  claim still needs a strict run, and exploratory or fixture evidence stays bounded to what it measured.
- A run that does not reach the changed address is NOT EXERCISED, not a pass
  (`scripts/check-run-exercised.py`).
- Run-to-run variation is first-class: same-binary runs take different paths, and a run without the
  four title-path switches is not comparable. Runs on different executables are compared only after
  re-checking `exe_sha256`, and a pair with differing switch windows is not a clean A/B.
- Values transcribed by hand into a record are re-checked by a second worker against the artifact
  (W5) until tool citation (T10) is the only path; TTD query output is a lossless decision input only
  under the W11 conditions in `docs/jsrf-run-profiles.md`.
- Original assets and existing saves remain unchanged.

## Closed packets

| Packet | Result | Claim (limits in TR) | TR |
|---|---|---|---|
| P0.S, P0.1–P0.7 | ACCEPTED | evidence loop: runner, profiles, dumps, review records, provenance | `docs/reviews/p0-3-to-p0-7-acceptance.md` |
| `A3a-r25` | ACCEPTED | AC'97 codec-ready modelled as `GS.bit8 := GC.bit1` | §3 |
| `A4a-r2` | ACCEPTED (discovery) | GP start handshake observed; row `O-6` | §4 |
| `A4p-r1` | ACCEPTED (discovery) | `PIO_FREE` gate-only at 28 direct reads (`O-GATE`) | §4 |
| `A4s-r6` | ACCEPTED | toolkit synced to v0.11.0 | §1 |
| `A4b1-r4` | ACCEPTED (stage 2) | GP DSP56300 core ported, GPL-2.0-or-later | §4 |
| `A4b2-r8` | ACCEPTED | the GP engine's DMA write clears the DSP pending word in a strict run | §4 |
| `A4b2-NR` (discovery) | `O-TWO-LEG` | the stub inputs do not reach the clearing descriptor | §4 |
| `A2h-slot-writer-attribution-r2` | ACCEPTED, `O-OPEN` | writer found, row withheld | §5 |

Owner-direct work outside packets (toolkit syncs to v0.12+, CRT divide helpers, the v0.12
regeneration, the fork audit and fixes) is in TR §1, §2 and §7. `A2h-r6` was retired (its premise was
refuted; the failure was already fixed by `cb7cae2`).
