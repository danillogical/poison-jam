# Turn start plan — `title-007`

Turn Planner baseline (immutable once execution starts). Repos at plan time: game `master@be8ad48`,
toolkit `main@07e7dac`, both clean. Plans guide; evidence decides.

## 1. Objective (owner)

Continue toward **M15, the title screen identified by content**. The **`0x00084000` drawn-surface
measurement is the first decision point**; then follow whichever critical path it selects. This is
not a one-measurement turn: a working dump is not a stopping point.

## 2. What the planner measured before writing this (read-only, archived runs; no new runs)

These change the starting picture, so they come first. **The Orchestrator should re-verify the
load-bearing ones (Phase 0) before relying on them.** Scratch output, outside the repo:
`%TEMP%\title007-scratch\` (`survey.py`, PNGs).

**M1 — `0x00084000` is already readable offline, without perturbing anything.** The executor's
`dma_resolve` maps surface offset `0x84000` to guest VA **`0x80084000`** (the contiguous window).
`inspect-jsrf.py memory <run> 0x80084000 614400 --out f.bin` gives the 640×480 RGB565 surface
(pitch 1280, format matches `[GPU] … 2bpp`). Convert it the same way `fb_present.c` does and apply
the FNV-1a used by `fb_hash_rgb`. **Trap:** `memory <run> 0x00084000` returns XBE `.text`, not the
surface.

**M2 — at the end of every 300-s run, the drawn surface equals the presented frame.** Across 16
dumps (the mapping gate passed on those checked), `0x80084000`, `0x8011C000` and `0x801B2000` are
**byte-identical** in every 300-s title-run, and their hash equals that run's **last `[FBPRESENT]`
hash**. Examples: `frames` → `87683a74…` (Dolby), `input` → `5bdaea57…` (disclaimer), `ex1`/`pad`/
`padpress2` → `d0e84b1c…`. So the guest is **not** drawing a title that the presenter fails to show;
in those runs, drawn and presented are both pre-title.
*Limit:* a frozen dump is one instant, not flip-synchronised, and a static image cannot discriminate
between pipelines.

**M3 — the disclaimer is a timed hold, not a freeze, and it ends with no switch and no input.**
- In `20261006-211635-913-title005-m15` (switches: `APU_TRAP`, `PB_EXEC`, `FB_WINDOW`, the dump
  switches; **no** `ADMIT_UNKNOWN`) and in `…-title005-admit3`, the disclaimer starts at
  **presents 1461** (t≈244–257 s). It **leaves at presents 2424** (t≈408–420 s): first
  `e886cadf…`, then black `156ed408…` at 2425.
- At about the same present count, the runs that get there open `Media\Disp\SprNorm1.*` and
  `Z_ADX\BGM\title.adx`, read `UDATA\…\SaveMeta.xbx`, and reload `Player\Corn/Beat/Gum/Yoyo` — a
  title-stage asset load.
- **Every "stuck on the disclaimer" run simply ended first.** That includes every 300-s
  `just title-run` and `title005-fixed` (417 s, presents 2410). Presents advance at only ~6 per
  second.

**M4 — just after the transition the GPU walk rejects, and a new 3D scene is sitting in
`0x80084000`.**
- **What each run hit:**
  - `m15`: `unsupported_method 0BB0` at successes 3498, pre-`46b3265`.
  - `admit3`: `budget_exhausted` at 3519.
  - `20261007-014848-201-title006-budgetcatch` (606 s, `ADMIT_UNKNOWN=1`, toolkit `76c76fd`):
    admit-unknown `0420–042C` and `0480–04AC`, then **`sink_capacity`** at 3523. That is the header
    check at `nv2a_core.c:1796` (sink + staged + count > 4096); it is not `budget_exhausted`.
- **What the surfaces held:** budgetcatch's `0x80084000` holds a **new 3D scene** (skyline, clouds,
  pillars; hash `3b0dfcc68ba437ff`), while `0x8011C000`/`0x801B2000` are black. **Whether this is
  the title screen is NOT established**; it needs an xemu reference.
- **The witness was truncated.** The admit-unknown witness is capped at `NV2A_ADMIT_PENDING = 16`
  (`nv2a_core.c:28`), and exactly 16 lines were logged, so the 39-method decode list is neither
  confirmed nor refuted.
- **The present hashes are missing.** Budgetcatch had no `FB_PRESENT_DUMP_EVERY`, so its present
  hashes at the transition are unknown.

**M5 — a role question about `0x00084000`.** `[TEXUSE] 0x80084000 640x480 fmt 0x11 lin` counts
equal `[GPU] flips` exactly in every run (for example 1311/1311 and 2390/2390). So `0x84000` is
**sampled as a texture once per frame**. A pipeline of the form "render into `0x84000`, then draw
one full-screen quad from it into the swap buffer" would explain three things:
- why only `0x84000` is cleared;
- why the three surfaces are identical when the image is static;
- why `0x11C000`/`0x1B2000` are black while `0x84000` holds the new scene (the quad not yet drawn,
  or faded to 0).

This is **INFERRED**, not measured. It is a reason to measure surface *roles*, not to revisit
retracted hypothesis 3.

**Consequence.** On archived evidence the decision point resolves as follows:
- **Outcome B at the 300-s snapshot**: nothing title-like is drawn yet.
- **Outcome C at ~present 2424+**: a transition and a new scene, drawn but not yet presented, with
  the GPU walk rejecting right there.

The most likely critical path is therefore **F8c at the title transition**, not presentation
plumbing and not USB input. Reaching the title screen should not need input; leaving it (M16) does.

## 3. Hypotheses to keep alive

| ID | Hypothesis | Supporting | Would refute |
|---|---|---|---|
| H1 | The title is reached by time alone; the walk's rejections at the transition (missing NV097 methods, then sink/budget) stop it from being drawn or presented | M3, M4 | a ≥480-s run on the current tree passes 2424 with zero rejections and still presents no new scene |
| H2 | `0x84000` is an offscreen target composited by a full-screen quad, so "drawn but not presented" can arise from a composite or fade that never ran | M5 | the per-flip role trace shows the presented surface is `0x84000` itself, or the composite quad draws but samples something else |
| H3 | The guest waits for input *before* the title is drawn | (only the dead input seam) | M3 already argues against it for the disclaimer; refuted again if the post-transition scene appears with no input |
| H4 | The transition point (present 2424) varies with run speed or timing, not with a frame count | none yet | the transition lands at the same present count across binaries and speeds (it did in `m15` and `admit3`) |
| H5 | The 3D scene in M4 is an attract or demo scene, not the title | content | the xemu reference shows the same backdrop under the title logo |

Do **not** revive the five retracted items in the brief (the `0x19E438` spin, the `RECOMP_KEYBOARD`
test, the pinned composites as a title, pinned-vs-unpinned counts, and novel hash = title).

## 4. Capture design: draw-state vs presentation-state, without perturbing presentation

- **Offline (zero perturbation, available now):** read `0x80084000`, `0x8011C000` and `0x801B2000`
  from the dump (M1). Use this for every run, but treat it as one unsynchronised instant.
- **Live, read-only, per flip (needed for the transition window).** Add an observation-only
  instrument in the executor's `NV097_FLIP_STALL` arm (or immediately after a successful commit).
  It must **never** go through `RECOMP_FB_VA` or change what `xbox_FramebufferWindowPresent`
  receives. On each flip, or on change plus every N flips, record:
  - the time and present serial, plus the flip and flip-stall counts and PFIFO successes and
    rejections;
  - the presented surface offset, and **why** it was chosen (drawn / targeted / previous-drawn /
    fallback, from `present_track_flip`);
  - `color_offset`, pitch, surface format, and clip w×h;
  - per surface for this frame: clears, draws, triangles, and whether it was bound as a texture
    (the `0x84000` texture bind is the composite witness);
  - the FNV hash of the presented copy, plus the FNV hash of **each** of the three surfaces taken
    at the same flip with the same conversion;
  - `flip_read`/`flip_write` and `PCRTC_START`.

  Write a PNG or BMP of each surface whose hash changed, capped. It is observation only, so list
  it in `docs/jsrf-run-profiles.md` §Observation (`check-override-drift.py` enforces this). A
  unit test should pin that it changes no present or guest state.
- **Distinguishing the two states:**
  - **Draw-state** is the content of the surfaces the guest clears and draws, taken at the flip.
  - **Presentation-state** is the copy the window published, plus which surface was chosen and why.
  - **Title drawn but not presented** looks like this: at the same flip, a surface hash classifies
    as title content while the presented hash does not, and the trace names the reason (a
    composite never drawn, the wrong surface chosen, or no flip).
  - **Not drawn** looks like this: no surface ever holds title content, and the walk stops or the
    guest idles.
- **Classification.** Hashes **identify** frames; images **classify** them. Every new hash gets a
  rendered image looked at by eye and labelled from a closed set: SEGA, Smilebit, ADX, Dolby,
  disclaimer, logo-atlas (`e886…`, `d0e8…`), black, 3D scene, title, other. Title content
  additionally needs a match against an **xemu reference** of the title, with SSIM as a guide and
  the M15 criterion of 60 frames, SSIM ≥ 0.90. Record the hash; never use hash membership as the
  criterion.

## 5. Execution order

**Phase 0: verify the planner's survey (cheap, parallel).**
- **Worker A:** reproduce M2–M5 from the archived runs, add the remaining long dumps, and produce a
  table per run: last present hash, the three surface hashes with labels, the transition present
  count, the first reject, and the asset-load markers.
- **Worker B:** produce the **xemu title reference** (outside the repo, owner assets never copied).
  Capture the title-screen image, the boot-to-title time, what precedes the title (does a 3D city
  backdrop appear, is there a fade from black), and whether the title waits on "Press Start". This
  settles H5 and gives M15 its comparator.

**Phase 1: one long correlated run on the durable current tree (the decision point proper).**
- **Setup:** exploratory `just title-run`, **≥ 540 s**, with `RECOMP_FB_PRESENT_DUMP_EVERY` (the
  recipe sets it) and the Phase-4 surface instrument if it is ready. If it is not, run without it
  rather than wait, and rely on the offline dump plus `[FBPRESENT]`. Verify the binary identity
  before comparing with earlier runs.
- **Expected:** the disclaimer leaves at ≈2424. Then a new reject is latched by the first-stop
  instrument (`4b6cc2f`), most likely `unsupported_method` on `0x0420` or a matrix slot, since the
  six are already admitted.
- **Branch on what the run shows:**
  - **(a)** The transition happens, a reject follows, and the scene is in `0x84000` → **F8c branch
    (§6.1).** This is the expected case.
  - **(b)** The transition happens, there are no rejects, and a new scene is presented → **classify
    against xemu**; if it is the title, go to the M15 closure (§6.4).
  - **(c)** The transition happens, there are no rejects, and the scene is drawn but the presented
    frame stays black or stale → **presentation branch (§6.2).**
  - **(d)** No transition by ~600 s on the current tree → a regression or variation against `m15`
    and `admit3`. Bisect with the binary identity first; input (§6.3) only after that is excluded.

Game runs are **serialised**: host contention changes timing.

**Phase 2: iterate the critical path** that Phase 1 selected until the title is presented, or a
blocker of a new class is named with evidence.

**Phase 3, in parallel from the start: run-speed A/B (iteration cost).** Each title attempt costs
≥ 8 minutes at ~6 presents/s. About 70% of log lines are `[TRACE]` from `config/trace-functions.json`.
`RECOMP_TRACE_BUDGET=0` is observation-only and stops printing. Do one matched-duration A/B on one
binary and compare presents/s and the transition time. Adopt it only if it is a pure speed change
(same frame sequence, same transition count).

**Phase 4 (toolkit, can start at turn start):** the surface/role instrument from §4.

## 6. Branch plans

### 6.1 F8c at the transition (expected)
1. **Witness the missing methods at runtime; never from the decode.** The admit-unknown witness
   needs two fixes first:
   - **Truncation:** it was cut off at 16 (`NV2A_ADMIT_PENDING`; also check `NV2A_ADMIT_LOG_MAX`).
     Make the queue drain completely, or log at first sight.
   - **The sink stop:** the `sink_capacity` stop ended witnessing. Witnessed lines only appear after
     a commit, so an admit run that then rejects on sink/budget under-reports.

   Then run with `ADMIT_UNKNOWN=1` past the transition and union the witness into
   `config/nv2a-runtime-witnessed-methods.json`, exactly as the six were. Check that the executor
   handles the matrices (`0x0480–04BC`, `0x0680–06BC`): admission must activate real behaviour or
   be ledgered as captured-state-only.
2. **`sink_capacity` / `budget_exhausted`:** apply the L40 direction — bounded, resumable prefix
   dispatch that commits up to the last complete packet boundary (and carries a packet split by
   the budget), preserves order and rollback, and does not publish the fence for the original PUT
   until it is consumed. **Raising the 4096 cap is not the fix.** The sink case fires at a header,
   so a prefix-commit at the previous header boundary may be the simpler half. Pin it with a
   deterministic test, as `76c76fd` did. This is the best **Advisor** point (§8).
3. Re-run ≥ 540 s. Success means the walk continues past successes ~3523 with zero rejections, and
   the presented stream leaves black.

### 6.2 Presentation path (title drawn but not presented)
Use the role trace to choose among these:
- whether the composite quad from `0x84000` was drawn at all — including untransformed or
  vertex-program batches that are not rasterised (`batches_untransformed`; plan item 5);
- whether `present_track_flip` picked the wrong surface;
- a fade/alpha factor of 0 in the composite (blend state, `0x24650`/`0x24700`-style fade objects
  already documented for SEGA);
- a missing flip;
- present-before-draw ordering.

Fix only the demonstrated cause. Do not wire input in this branch.

### 6.3 Guest progression / input (only if §6.1/6.2 show no drawn title, the guest is idle on a
stable scene, and the evidence points at a press)
- **Wiring:** `xbox_OhciInit` (and the XAPI device enumeration it must precede) has to be called
  where the toolkit's USB model expects. Find the intended call site from upstream and prior art
  (Mercenaries/halo-ce) first.
- **Proof of delivery must be guest-visible:** a controlled run with **no input** against one with
  **one known button pulse at a known time**, showing a difference at a guest read or state. Good
  places to look:
  - the guest's `XInputGetState` packet number or buttons;
  - the logo object's input latch `+0xA0` (documented in `docs/reviews/owner-sega-600-observations.md`);
  - the title's own input state.

  Never use "PAD: synthesising" lines as proof, and never press all buttons.
  `RECOMP_PAD_PRESS` is diagnostic only and not admissible for M16.

### 6.4 M15 closure
Present the title frame(s) — 60 consecutive frames against the xemu reference, SSIM ≥ 0.90 — with
the intro-FMV handling stated. The run record lists ledger IDs: title-run's L14–L18, L20–L25,
L39, L40, plus L44 if `ADMIT_UNKNOWN` was used, plus anything new. Validate on the **durable final
tree**, then hand to the Turn Reviewer for reproduction.

## 7. Worker tasks (DeepSeek 4.1 Flash @ high; bounded, disjoint)

| W | Objective | Writes | Done when |
|---|---|---|---|
| A | Reproduce and extend the planner's archived survey (M2–M5) for all long dumps; render and label every distinct surface | scratch only | a table with observed/inferred tags; PNGs looked at |
| B | xemu title reference: image, boot-to-title time, precursors, "Press Start" behaviour | `%TEMP%` only, no assets | reference frames and a short description |
| C | Toolkit: per-flip surface/role observation instrument, its test, and its run-profiles row | `src/kernel/nv2a_pb_exec.c` (+ new header), test, game doc row | test passes; inert when off |
| D | Toolkit: make the admit-unknown witness complete (no 16-cap loss; reports survive a later reject) and its test | `src/nv2a/nv2a_core.c` witness path only, test | a mutation-validated test shows >16 distinct methods reported |
| E | Static: the executor's coverage of `0x0420–042C`, `0x0480–04BC`, `0x0680–06BC`, `0x1748`, `0x1B40/44`, so admission activates real behaviour | read-only report | per-method handled / ignored / missing |
| F | (after the Advisor) the sink/budget prefix-commit implementation and its deterministic test | `nv2a_core.c` walk/commit, test | test plus a ≥540-s run past the stop |

C and D touch different files. D and F share `nv2a_core.c`: serialise them, or give F the file
after D lands. Worker summaries are leads; check the load-bearing facts.

## 8. Where the Persistent Advisor helps most

1. **Before F:** the design of the resumable prefix commit for `sink_capacity`/`budget_exhausted`,
   under the owner's L40 constraint (carry, order, rollback, fence publication).
2. **Interpreting the role trace,** if drawn and presented disagree (H2): composite, fade or present
   choice, and which fix is faithful.
3. **If Phase 1 lands in (d)** (no transition on the current tree): run-to-run variation against a
   regression, before any input work.
4. **Judging a candidate title frame** against xemu when the content is partial (C-type
   intermediate states).

## 9. Deciding measurements (progress toward M15)

- **D1:** the current tree passes present ≈2424 with no admit switch in a ≥540-s run (reproduces M3).
- **D2:** the first-stop latch names the post-transition reject (class, method, position).
- **D3:** at the same flip, the surface hashes against the presented hash across presents
  2300–2450, with images. This is where draw-state is separated from presentation-state.
- **D4:** after each F8c fix, PFIFO successes pass the previous stop with zero rejections, and the
  presented stream gains a non-black, non-logo, non-disclaimer frame.
- **D5:** that frame matches the xemu title (by eye plus SSIM), then 60 frames for M15.
- **Only in §6.3:** a guest-visible input delta between a no-press and a one-press run.

## 10. Completion criteria

**The turn is complete** when one of these holds:
- **(i)** M15 is met and handed to the Turn Reviewer with its ledger IDs;
- **(ii)** the title-transition walk is unblocked through every stop of the F8c class, and the next
  blocker is of a new class, named with evidence (a run, the latched stop, images);
- **(iii)** a measured contradiction of M3/M4 that redirects the path, with the new path started,
  not just named.

**Continue rather than stop when:**
- a fix moves PFIFO successes past the old stop but presentation is still pre-title;
- the next stop is the same class (another method, another sink/budget site);
- the role trace shows "drawn, not presented" with a nameable cause.

**Not completion:**
- the dump works;
- one method batch admitted;
- a new hash seen;
- a 3D frame seen without the xemu comparison.

Durable findings — M2–M5 once verified, the transition count, and the surface roles — go to the
plan's Current work and TR §23.x. The "freeze on the disclaimer" framing in the plan and TR §23.1
should be corrected **if D1 confirms M3**.
