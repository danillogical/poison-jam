# `A2h-arming-coverage-attribution-r2` — stage-1 acceptance: **`ACCEPT`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Reviewer:** `a7a39c65-9b75-41b4-9321-5c03743fdf57` (`workbuddy-ai/hy4-preview-f` @ `high`), **independent** —
it neither authored the packet nor executed the runs.
**Review:** `docs/reviews/a2h-arming-coverage-acceptance-review.md`, committed `a7a6757`.
**Disposition: `ACCEPT`. `BLOCKING: NONE`. `EVIDENCE OUTRUNNING CLAIMS: NONE.`**

**All three mandatory criteria `AGREED`.** Per `docs/agent-workflow.md` §2.2, **a first-stage `ACCEPT` is
final and is not passed to stage 2** — so **no second review is spent on this packet.**

---

## What the reviewer verified independently

**Criterion 1 — artifacts.** All six run dirs complete; **`jsrf_run.log` SHA-256 recomputed and matched against
`metadata.run_log_sha256` in all six.**

**Criterion 2 — commands.** ON runs carry `JSRF_TRACE_A2H_DR=1` + `JSRF_TRACE_A2H_SLOT=1`; **the OFF run carries
neither.** All six: `RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, `RECOMP_KERNEL_LOG_BUDGET=100000`, and **none** of
the four forbidden variables. **`check-run-profile.py` returns STRICT for all six.** exe / collector / XBE
hashes **byte-identical across all six.**

**Criterion 3 — row.** The reviewer classified every run itself and **reproduced the Session's result
exactly**: run 2 the only TARGET, runs 1/5 unresolved `0x001D5078`, run 3 `0x00147DE2`, run 4 `0x00147D36`,
**exactly one `[EXCEPTION]` and one fatal `[ICALL]` per run with no competing terminal**, and the anchors
present in all six **including the OFF**. **`K = 1`, observed `1/5` ⇒ `O-COVERAGE` on K ≥ 2 is correct.**

## The arming claim — `CONFIRMED`, verified three independent ways

**The reviewer counted 17 `GUEST_DR_ARM_OK` per run, split 10 `why=handshake` / 7 `why=create_thread` in
each**, and cross-checked run 2 three ways:

1. the 17 `ARM_TID_TERMINAL` entries are `source=handshake` for indices 0–9 and `source=create_thread` for
   10–16;
2. the 26 `BIRTH_ROW`s are **9 `state=deferred reason=no_mapping_offset` + 17 `state=armed`**;
3. **all nine `ARM_FAIL` tids reappear as handshake `ARM_OK` rows**, with `recovered_by_sweep=9
   failed_never_recovered=0 exited_unarmed=0`.

**Zero `GUEST_DR_HIT` in all five**, and the exit-rule caveat correctly reproduced.

## The fresh OFF — record-level inertness `CONFIRMED`

**0 `[A2HSLOT]` in the OFF log** (ON runs ~14.9k–15.2k). **0 `GUEST_DR_` in the OFF `stacks.txt` AND in the OFF
log** (ON runs: 101 in `stacks.txt`). **The reviewer extracted the registry itself**: version 3, latch
all-zero, **and the WATCH all-zero including `armed=0`, `class_witness_count=0`, all six class entries
`valid=0`.**

## The harness fix — `VERIFIED`, and the "broken before this packet" claim `CONFIRMED` by dates

`python -X utf8 scripts/test-harness.py` **passes, 19 probes, exit 0.** `git show 2c8765c^:scripts/test-harness.py`
**still contains both pins.** **The version moved 1→2 at `009f624` (00:14:28) and 2→3 at `f5b709d` (01:31:27),
while `test-harness.py` was last changed at `e336a1c` (2026-09-21)** — so **both bumps postdate the harness edit
and predate this packet's promotion (`be2149b`, 02:54:21).** **The break was the Session's and it predates the
packet.**

**And the reviewer confirmed nothing would have surfaced it:** `git grep -ln "test-harness" -- tests ctest
CMakeLists.txt` returns **nothing**, and none of the twelve `tests/test_*.py` invoke it. **It also found that
`tests/test_harness_permissions.py` exists but does NOT execute `scripts/test-harness.py`** — **a filename that
can mislead a reader into thinking the guard suite covers the harness.** **Recorded as a wording hazard, not a
defect.**

## No over-claiming

**The reviewer examined the Session's "NOT a coverage failure" framing and agreed it is fair**, on the
packet's own terms: **the `O-COVERAGE` triggers are unattempted dispatching tid / uncovered interval /
unresolved lifecycle / event-loss / publication disagreement, and none appear** (`arms_recorded=17`,
`distinct_armed_tids=17`, `arm_tid_overflow=0`, `failed_never_recovered=0`, `exited_unarmed=0`,
`incomplete=0`). **The shortfall is yield.** **No writer named, no row selected, generality `UNKNOWN`,
run-local row only** — and it **correctly does NOT read zero `GUEST_DR_HIT` as a no-write finding.**

**One scope note the reviewer raised and did not score as a disagreement:** the evidence cites the **K = 0**
"STOP and re-refer" clause **by analogy** for K = 1, when **the directly governing K = 1 sentence gives the
same outcome.** **The Session accepts the note**: the conclusion is right and the citation was the looser one.

## Synthetic completion — `ABSENT`

No forbidden env in any run. **`git diff --stat be2149b HEAD -- src/recomp/gen` is EMPTY.** `0xE0424943`
appears at **exactly two unchanged sites** (`src/recomp_manual.c:33,69`). Tree clean. `PIO_FREE` deferred;
`A4b2-r7`/`A4b2-r8`/`A4b1-r4`/`0xFFFFB3` not reopened.

## Carried uncertainties — the reviewer's, stated rather than smoothed

- **Guest liveness beyond the archive** — no game was run, by instruction.
- **Per-tid guest-dispatch exhaustiveness** (the packet's operational dispatch criterion) — it confirmed
  **17 armed vs `native_threads: 17`** in every `result.json` with `exit_events=0`/`exited_unarmed=0`, **but did
  not enumerate every dispatching tid against the dispatch witness set.** **It notes this cannot change the row,
  since `O-COVERAGE` is already selected on K ≥ 2** — **which is correct, and worth stating because it means the
  packet's central new requirement was validated only partially.**
- **`arm_tid_list` cross-check done in run 2 only**, not all five.
- **Legacy decisions confirmed untouched only via the empty generated-code diff and clean status.**

**The Session records the second and third as genuine residual gaps**, both non-blocking for this row.

---

## Outcome

**Packet accepted.** The instrument's reporting defect is **fixed and independently verified**; the row is
**`O-COVERAGE` on K ≥ 2**; **the arming question that two prior packets could not settle is now answered by
direct measurement — 17 threads armed, 7 of them post-handshake and previously invisible.**

**The next question is a PLANNING one, not an execution one:** the instrument is coverage-complete but the
**TARGET yield is low and variable** (3/5, then 1/5). **`N` cannot be extended without Advisor referral**, and
the packet's own rule for K = 1 is *"preserve the run-local row, `UNKNOWN` generality, re-refer."*
