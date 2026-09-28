# Advisor ruling — `A2h` successor after Run 1 (OFF) selected `O-COVERAGE`

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, `reasoningEffort: max`,
turn `01a0e734-1bf1-7000-b6c5-7f742821fe0c`.
**Raised because:** Run 1 (OFF) selected `O-COVERAGE` — the packet's own comparability gate fired — and the
Session found the divergence **pre-existing** (three archived runs, identical `jsrf_recomp.exe`, divergent
terminal sites). That falsifies the packet's OFF-control premise, so a `PREMISE_CHANGED` re-referral was
required under `docs/agent-workflow.md` §4.3.
**Authority:** `docs/agent-workflow.md` §2.3, §4.3.

**Ruling recorded verbatim below.** The first two rulings (superseded DR0-only, and the corrected DR0+alias
census design) remain in `docs/reviews/a2h-slot-read-path-advisor-shape-preflight.md` as history.

---

## RULING

**Row confirmed, instrument kept, comparison design abandoned.** `O-COVERAGE` was correctly selected (packet line 40's gate fired exactly as written; not launching ON was obedience, not caution). The packet failed closed on a violated premise rather than producing an uninterpretable attribution — that is the contract working, and the Session's refusal to run ON anyway is commended without qualification.

### Q1 — `O-NO-BOUNDARY-TRANSITION` stands, unweakened in content, narrowed in reuse. Confirmed with one refinement

The row is run-scoped observations about Run 2's artifact (15498 installed samples, terminal zero, no intervening bridge call, per-thread completeness). Cross-run variance cannot falsify within-run observations, and acceptance verified selection from frozen rows. **No §5.4(2)** — no frozen premise asserted cross-run determinism.

The refinement: variance *does* partially weaken one *use* of the triage evidence — behavioral inertness via terminal-match (Run 1 vs Run 2 agreeing on `0x0014982E` may have been luck, not proof). Mechanism inertness (zero records when off) is untouched, and the row never depended on the match. Durable lesson, binding on this line henceforth: **no run serves as a cross-run control for terminal behavior; controls must be structural (gates-off silence) or prefix-scoped (deterministic anchors only).**

### Q2 — Instrument stands; comparison design is what failed. Correct not to launch ON

DR0 + alias census + latch + terminal hook measures within-run facts whose soundness never depended on reproducibility (built: 18/18 ctest, guards OK, 43 extractor tests, v2 polarity preserved; proven inert at record level in Run 1). What failed was purely the OFF-vs-baseline comparability gate — which assumed what the archive already contradicted. No code changes needed for the successor's measurement side.

### Q3 — Successor: within-run attribution + bounded repetition. Pin-by-construction REJECTED. Coverage-provenance as conceived WITHDRAWN

- **Within-run rows only.** Each realization classified FIRST by observed terminal event (target `0x0014982E` raw-zero vs other). Attribution rows apply only to target realizations with complete coverage; all others recorded in full as contrastive data, never forced into rows.
- **Bounded repetition, pre-specified:** N ON runs, same build (suggest N≤5, Planner justifies); need K≥2 target realizations; attribute each independently; general claims require agreement — disagreement → report both + `UNKNOWN` generality (per-realization findings stand as witnessed mechanisms). Zero targets in N → report rate + STOP + re-refer (anti-optional-stopping; no extension without referral). This supersedes the old two-run bound.
- **Validity ≠ terminal matching.** Per-run validity: deterministic anchors only (OOM size/site/status + main-thread dispatch prefix through OOM — identical across all five known runs). Terminal explicitly EXCLUDED from validity. Anchor mismatch → `O-COVERAGE` for that realization.
- **OFF control:** existing Run-1 inertness evidence CARRIES iff same build + no code changes (verify exe hash); new build ⇒ fresh OFF. No other OFF runs.
- **Coverage per realization** (arming census, no gaps, mapping stability, install trap), read-path three legs, closure discipline: unchanged, applied per run.
- **Second-slot observations** (`0x1C4078`, `0x3E800000`, `0x1` targets): contrastive data only. NO second-slot attribution scope — resist creep; pivot condition: target rare + siblings dominate across N → re-refer for line pivot.
- **Routing:** the successor is NOT `A2h-slot-write-coverage-provenance` as conceived (that scope predicated reproducible failure — falsified). It is a within-run attribution packet (name: Planner's choice); record explicitly: *"O-COVERAGE row named X; Advisor redirected scope to Y because Z."* Coverage questions survive as per-realization input checks, not as the packet.
- **No `DONE:no`.** Contrast PIO_FREE is instructive and cuts the other way: there, no consumer + no unblock path; here the instrument exists, the question is answerable, and it sits on the critical path. Proceed.

**BASIS:** observed — packet gate/rows/successor text (`:40`, `:45`), OFF evidence tables (inertness zeros, OOM identical, site/prefix divergence), archive trio same-exe (`ddd7e353…`) with divergent terminals, implementation record (tests, guards, DR6/DR7 fixes, carried uncertainties #1/#3/#4); run-scoped triage row content (verified last turn). Inferred — scheduling as variance mechanism (consistent, unproven; ruling independent of mechanism). Uncertain — terminal-site distribution (N≈4, too few for rates; hence bounded repetition, never statistical claims).

**REVERSED BY:** controlled same-build evidence that variance is instrument-caused (currently contradicted by the archive trio + accepted ON/OFF agreement); deterministic-prefix divergence in successor runs (→ instrument suspect or deeper nondeterminism → fail closed per design).

**RECORD IN:** verbatim with handle/model/effort in the successor's planning/review record; O-COVERAGE stands as selected; routing note as above; Session updates CURRENT PACKET (the OOM-size/site + main-thread-prefix anchor list goes in the plan as the validity gate).

---

## The Session's evidence this ruling rests on

**The archive trio, same `jsrf_recomp.exe` (`ddd7e353e769e073…`), divergent terminals:**

| Run | Gates | Terminal target | Site | Thread |
|---|---|---|---|---|
| `20260928-001502-101` accepted OFF | none | `0x00000000` | `0x0014982E` | identity 1 |
| `20260928-001520-474` accepted ON | `A2H_SLOT`, `KERNEL_WATCH*` | `0x00000000` | `0x0014982E` | identity 1 |
| `20260928-011149-570` latch-print-verify | `A2H_SLOT` only | `0x3E800000` | `0x00147DE2` | identity 4 |

**Run 1 (OFF), `logs/runs/20260928-014526-583-a2h-slot-write-inert-off`:** instrument **inert at record
level** (zero gated lines; latch `install_seen=0 install_ok=0 claimed=0 overflow=0 partial=0`); **OOM
identical**; **terminal site and per-thread prefix divergent** (main thread identity 1 **exactly identical at
5555 dispatches**; identities 3/4/5 **765/124/116** vs **872/143/131**).

**The validity anchors, measured across all five known runs:** OOM `requested 598869040, used
12715008/50855936`, status `0xC0000017`; **main-thread (identity 1) dispatch prefix through the OOM = 5555**.
**These are the deterministic anchors. The terminal event is NOT one of them.**
