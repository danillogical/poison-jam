# Turn plan (living) — `title-007`

Owner: Orchestrator (`workbuddy-ai/deepseek-v4-flash` @ high). Baseline:
`plan-turn-start-title-007.md` (Planner, Claude Opus 5.5 @ high). This file is the living
execution plan; it is updated as evidence moves.

Repos at turn start: game `master@be8ad48`, toolkit `main@07e7dac`, both clean.

## 1. Objective

Continue toward **M15 (the title screen, identified by content)**, making the `0x00084000`
drawn-surface measurement the first decision point, then follow whichever critical path the
measurement selects.

## 2. What the first decision point actually showed

The plan's stated measurement was taken, and it **eliminated the presentation hypothesis**.

- `0x00084000` is a raw physical `color_offset`; `dma_resolve` maps it to guest VA
  **`0x80084000`**. Reading `0x00084000` returns XBE `.text`.
- Read offline from archived minidumps (zero perturbation — no `RECOMP_FB_VA`), all three
  surfaces were hashed with the model's own conversion and FNV-1a. In
  `20261007-001831-252-…-sixadmitted`: `0x8011C000` and `0x801B2000` both hash
  `8205f3a6d2e48df5`, which **is** the run's last `[FBPRESENT]` hash, and rendering it shows
  the graffiti disclaimer. **Drawn = presented.**
- So this is **not** Outcome A (title drawn, presentation broken) and **not** Outcome B
  (nothing but pre-title content) as originally framed. It is **Outcome C**: at the snapshot
  nothing title-like is drawn; at the transition a **new 3D scene** appears in `0x80084000`
  while the other two surfaces are black, and the submission walk rejects right there.

**PLAN_CHANGE:**
- **Changed:** the turn's technical focus moved from "capture `0x84000`" to "clear the
  submission-walk stops at the title transition". The capture was cheap (offline dump read)
  and conclusive, so it did not consume the turn.
- **Evidence:** the surface hashes above; the transition at presents 1461→2424 reproduced in
  three runs; the reject at the transition reproduced twice (archived + my own fresh run).
- **Why:** the plan's own branch rule says "if the title is NOT drawn, ... move to the input
  path". Measurement refuted the input path as the *current* blocker (see §3), and instead
  localized a walk stop. Evidence decides.

## 3. Corrected premises (two of the plan's and TR §23.1's claims were wrong)

1. **The disclaimer is a timed hold, not a freeze.** It starts at presents **1461** and is
   left at presents **2424** (t≈408–420 s) with **no switch and no input**, in
   `title005-m15` (no admit switch), `title005-admit3`, and my own `title007-long3d`. Every
   run that looked "stuck" **ended first** (presents advance ~6/s; the 300 s recipe ends at
   ~1450–1800). The §23.1 "the picture stops changing while the guest keeps drawing" framing
   is corrected in TR §23.2.
2. **"`0x11C000`/`0x1B2000` are never cleared" is FALSE.** Across the archive the
   `clear surface` field takes `0x0011C000` 45× and `0x001B2000` 43×. The "only `0x84000`
   and `0x0` are cleared" reading came from the clear-colour line, which prints only the
   first 8 **distinct clear colours**, not the first 8 clears.
3. **The 39-method list was not "provenance NOT established".** It was **truncated by the
   witness's own 16-entry cap**. See §4.

**Not revived:** the `sub_0019E438` spin, the `RECOMP_KEYBOARD` test, the pinned composites
as a title, pinned-vs-unpinned counts, novel-hash-is-title. The last two remain retracted.

## 4. Work landed

- **Witness-capacity fix (toolkit, `nv2a_core.c` + test).** Deleted the redundant
  `NV2A_ADMIT_PENDING 16` and sized the three witness arrays and both guards by the existing
  `NV2A_ADMIT_LOG_MAX` (256). The 16 was strictly redundant: an entry reaches
  `g_admit_pending` only after surviving the dedupe check, which already refuses at 256.
  `admitted_total` (the published count) increments outside the guard, so only the witness was
  truncated. **Mutation-validated:** a new test with 20 unknown methods in one walk fails at
  16 (`admit-unknown lines: 16, want 20`) and passes at 256; I reproduced that mutation check
  myself. Toolkit CTest 11/11, Python 213 OK.
- **The 39 methods are now runtime-witnessed, not decode-derived.** With the fix, one run
  logged **39** distinct methods and that set is **exactly** the decoded set (39 = 39, zero
  difference either way). Recorded in `config/nv2a-runtime-witnessed-methods.json` with run,
  log SHA-256 and line number.
- **Table regenerated from the witness union: +39, zero removals** (NV097 376 → 415),
  verified programmatically per class.
- **Result: the stop moved class.** A run with **no** admit switch went from
  `unsupported_method 0x0420` (`get=0x50810`) to **`sink_capacity`** (`get=0x50B1C`), with
  `missing_methods` now **empty**. Ordering: **method admission first, then capacity.**
- **xemu title reference captured** and verified by me:
  `logs/workers/title007/xemu/deliverable/` — boot → Smilebit → ADX → Dolby → disclaimer →
  fade to black → 3D city backdrop → JSRF emblem → **"PLEASE PRESS START TO BEGIN"**, 640×480,
  reached with **no input**. This is M15's comparator. The recomp's 3D scene does **not** match
  it as rendered (47.8 % pure-black sky vs 1.4–15.1 %; 0.0 % green-dominant vs 8.8–16.1 %).
- **Records:** TR §23.2 and §23.3; plan "Current work", "Next actions", F8c/F8d and the
  M15/M16 rows; ledger L44 updated and **L45 added** (the admitted set, classified per
  method, with two recorded gaps).

## 5. Advisor consultation (used, and it corrected me)

Consulted Claude Opus 5.5 @ xhigh three times. It **corrected two of my premises**: the
rejected submission is **23,332 bytes = 5,833 words (~1.4 budgets)**, not "5.7 budgets"
(I had conflated bytes with words), and the capacity predicate at `:1796` is a genuine
`staged[]` array bound, not a diagnostic artifact. It also replayed the model's own packet
rules over the submission from the dump and established it is jump-free/call-free and splits
at **whole-packet boundaries** into exactly two units, the second ending exactly at PUT. It
ruled that admitting state-only methods is acceptable (recording a register's value *is* the
hardware effect), that the partial-matrix hazard is resolved by admitting complete spans
(matrix uploads are single 16-count packets), and that the fix is **bounded prefix commit at
whole-packet boundaries with no in-packet carry**, publishing the fence only when the final
unit reaches the original PUT — flagging two hazards: `sink_count` is reset only per walk
(two units >4096 methods would **overflow `sink[]`**), and the removed caps were the cycle
backstop (needs a per-call word bound).

## 6. Landed after Review 1 (FIX) — remediation

Review 1 (`plan-turn-review-1-title-007.md`, codex GPT-6.1-Sol @ high) returned **FIX** with three blocking
findings. All three are addressed; the Reviewer's scope was the pre-remediation baseline (`1f86fbb` /
`b349768`), and its findings were correct.

- **B1 — multi-unit admission overcounted.** Real defect. `admitted_unknown += admitted_total` ran at every
  unit commit while `admitted_total` was never cleared, so each unit re-added every earlier unit: a two-unit
  stream with 6137 unknown occurrences published **10227**. Fixed in toolkit **`e85f331`** (count per unit,
  publish then clear), with a mutation-validated regression that drives two units in which *every* staged
  method is an unknown admission and asserts exactly 6137; removing the clear fails it with
  `admitted_unknown=10227, want 6137`. The verified 39 distinct witness methods are unaffected — deduplicated
  witness identity is a separate measurement.
- **B2 — the presentation claim was overstated.** Corrected in the plan and TR §23.2: the frozen surfaces and
  the published hash are **not paired at the same flip**, and the frozen draw surface was black while others
  held the disclaimer. The claim is now limited to "the surfaces holding content held the disclaimer and that
  hash matches the last published sample", and **presentation/source-role hypotheses are explicitly kept
  open**.
- **B3 — the live plan was stale, and the redirected path was only named.** Fixed: "Current work" and "Next
  actions" no longer call capacity the blocker or ask to finish a cleared F8d, and the black-interval
  measurement was **actually run**, not merely proposed. Result below.

## 7. The discriminating black-interval measurement (started, and it discriminates)

`20261007-085324-850-title007-blacktrace` — 700 s, **no** admit switch, standard title switches. It stops at
`unsupported_method 0x1964` (`successes=3529`, `GET=0x5A700 != PUT=0x4D9B8`), and the presented stream is
black from presents **2425 to 2442** (t=519–688 s, ~170 s). Surfaces at the dump (mapping gate passed):

| surface | hash | non-black | colours |
|---|---|---|---|
| `0x80084000` | `d4fa6747357b9e60` | **0.523** | **1417** |
| `0x8011C000` | `156ed4086987e325` | 0.000 | 1 |
| `0x801B2000` | `156ed4086987e325` | 0.000 | 1 |

**The guest is drawing a rich 3D city scene — including the green elevated highway the xemu title backdrop
shows — while both swap surfaces are pure black.** So the black is **downstream of the draw**, and the
earlier "all three surfaces zero" reading came from the admit run whose `GET == PUT` made its dump an
empty-queue instant. Rendered: `logs/workers/title007/surf/bt_084000.png`.

**This is deliberately not yet a presentation-bug claim.** One instant does not establish whether
`0x84000` is the intended present source, whether a composite/resolve from it should have run, whether a
flip is missing, or whether `present_track_flip` chose a black surface. The next step is the **same-flip
trace**: hash every candidate surface *and* the published copy at one `NV097_FLIP_STALL`, record
`present_track_flip`'s choice and reason, keyed on `(flip_stalls, present serial)` — never timestamps, never
`RECOMP_FB_VA`, never a guest/presenter mutation.

**M15 remains NOT claimed.** The scene now shares the xemu backdrop's most distinctive feature (1.9 % vs
0.0 % green-dominant pixels), which makes the earlier "different scene" reading *weaker*, but the comparison
is still non-like-for-like (1417 vs 69,391 colours) and the draw→present path is unresolved.

## 8. Branch rules from here

- **Walk clears and a new frame is presented** → classify by content against the xemu
  reference; if it is the title, go to M15 closure (60 frames, SSIM ≥ 0.90, ledger IDs, run
  record).
- **Walk clears but presentation stays black** → **this is the current state, now localized to the
  draw→present path** (§7). Go to the same-flip trace: at one `FLIP_STALL`, hash every candidate surface
  *and* the published copy and record `present_track_flip`'s choice and reason, keyed on
  `(flip_stalls, present serial)` — never timestamps. That separates **refusal**
  (`surface_write_refused` / the `dma_resolve` image redirect), **redirection**, **clearing** and
  **source selection**. Do **not** treat the presentation branch as eliminated (Review B2).
- **Next stop is another missing method** → same F8c class; take it from a runtime witness.
- **Next stop is the same capacity class** → the fix is incomplete; do not raise the cap.

## 9. Deciding measurements and completion criteria

Deciding: the transition at presents 2424 (D1, done); the latched first stop (D2, done —
`unsupported_method 0x0420`, then `sink_capacity`, then `unsupported_method 0x1964`); the surface hashes
(D3 — **only a partial, unsynchronized measurement**, per Review B2: the surfaces holding content held the
disclaimer and matched the last published sample, but that is **not** a same-flip comparison and does not
identify the present source); PFIFO successes passing the previous stop with zero rejections plus a new
non-black, non-logo, non-disclaimer presented frame (D4 — **successes passed, but no new presented frame**);
the black interval localized to the draw→present path (§7); that frame matching the xemu title (D5).

**Completion:** M15 met with its run record and ledger IDs and handed to the Turn Reviewer;
**or** the title-transition walk unblocked through every stop of the F8c/F8d class with the
next blocker named with evidence (a run, the latched stop, images); **or** a measured
contradiction that redirects the path, with the new path actually started.

**Not completion:** the dump works; one method batch admitted; a new hash seen; a 3D frame
seen without the xemu comparison. **M15 is not claimed.**
