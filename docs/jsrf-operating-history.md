# JSRF operating history

A dated outline of how the project got here; **historical only**, never a status or staffing authority (the plan and `docs/agent-workflow.md` are).
The full old text (3,090 lines) is `git show 9d5257f:docs/jsrf-operating-history.md`.
Lessons drawn from it are in `docs/jsrf-lessons-learned.md`.

| Dates | What moved | Key commits / pointer |
|---|---|---|
| 2026-09-12 | Baseline, CRT `memmove` fix, harness audit and the 01a-01d harness work (reproducible run artifacts, all-thread crash/hang collection, bounded thread history, concurrency probes). | TR harness sections |
| 2026-09-12 to 09-21 | Translation and initializer checkpoints, the GPU boundary and register/capture work, NV2A aperture owner and PTIMER (11b1-11b4), GPU harness completion (01f). | `docs/jsrf-gpu-setup-contract.md` |
| 2026-09-21 | Git history rebuilt after `.git/objects` went missing (history starts at `e336a1c`); the first guest stop traced to an alias fold that deleted a static initializer, not an SEH defect. | `e336a1c` |
| 2026-09-22 to 09-23 | A1-A5 audit: vblank delivery repaired, false/phantom entry ends (A2b, A2f, A2g), pointer-table-only functions (A2c), APU MMIO `rep movsd` (A2d), displaced RAM (A2h), A2h-r6 retired as refuted; P0.1-P0.7 repaired the evidence loop (strict/exploratory launch classification, durable reviews, guarded build, provenance). | TR; plan snapshot archived 2026-09-23 |
| 2026-09-27 to 09-28 | A2h OOM shown handled; the NULL-slot line retired at its ceiling and the work pivoted to `0x001D5078`; advisor practice note on scheduling stated uncertainties. | `git show d6a1bc0~1:docs/reviews/a2h-null-line-closure.md` |
| 2026-09-30 to 10-02 | Title-screen fast path (F0-F4b): SEGA logo gate located, 600 s D2 observations, no-APU classification, L02 recovery and kernel-name chores, observer parked; plan section 13 moved here, then to the pointer below. | `git show 9d5257f` for the text |
| 2026-10-05 to 10-06 | Checkout renamed to `poison-jam`; plan condensed (1,952 lines); turns title-001 to 004: stops 1-25, stack-depth CFG validator, hidden-entry detector, dispatch count gated. | `git show dcc93ab:plan-jsrf-bare-minimum.md` |
| 2026-10-06 to 10-07 | Owner-authorised staffing substitution (recorded in the workflow doc); method-inventory generator fails closed; six witnessed methods admitted, blocker beyond them is eight more. | toolkit `46b3265`; TR section 22 |
| 2026-10-09 (Mac review) | Review of title-007..010 fixed two walk defects; upstream v0.13.1 merged as one commit with upstream's pushbuffer executor adopted. | toolkit `fafe0f6`, `409c635`; game `51bc1ff`; `git show 9d5257f:docs/reviews/upstream-v0.13.1-merge.md` |
| 2026-10-09 (plan condensed) | Plan cut from 1,012 lines; turns title-005 to 010 summarised (stale method-table entry `0x1810`, fail-closed generator, host clock overflow L53, ADX worker deaths retired); title screen not reached. | `git show 2a580f7:plan-jsrf-bare-minimum.md`; toolkit `5d6ebbd`, `a5e2762`; game `c10d1a2` |
| 2026-10-09 (Mac over ssh) | Merge gate passed (CTest 49/49, 15/15) but merged executor slower; telemetry epoch fixed; two executor instruments added; `0x9188C` closed (stop 32); transition ends in the game's fatal path; bare-minimum plan retired for `plan-jsrf-title-screen.md`. | toolkit `de39fb1`; game `2d47805`, `57af797`, `d6b8336`, `9d5257f`; TR sections 25-26 |

## Push receipts

AGENTS.md requires a `PUSHED_TO: / BRANCH: / COMMIT: / REMOTE_URL: / RESULT:` receipt for each push, recorded in the record for the work it closes; only the latest are kept here.

Toolkit
`PUSHED_TO: origin / BRANCH: main / COMMIT: de39fb1a76a9b2e57b447695bd8c91c2ead94654 / REMOTE_URL: https://github.com/danillogical/xboxrecomp.git / RESULT: fast-forward a5e2762..de39fb1 main -> main`

Game
`PUSHED_TO: origin / BRANCH: master / COMMIT: 2d478050564a5d34e5fe9538a3134d9fb7804291 / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward a2f185e..2d47805 master -> master`

Game
`PUSHED_TO: origin / BRANCH: master / COMMIT: 57af797ea45fa6f86847e0a5f13920d08001254b / REMOTE_URL: https://github.com/danillogical/poison-jam.git / RESULT: fast-forward 38aab04..57af797 master -> master`
