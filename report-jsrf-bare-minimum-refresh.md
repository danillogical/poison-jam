# Report: JSRF plan refresh (2026-09-29)

Companion to `plan-jsrf-bare-minimum-refresh.md`. The owner asked for a refreshed plan built from a
review of all work since the project began, with every decision recorded here for review afterwards.
This file is an owner-requested decision record, not a session report and not an authority.

**Produced:** `plan-jsrf-bare-minimum-refresh.md` (PROPOSED) and this report, committed on branch
`jsrf/fork-fixes` of the game repository, not pushed.

**How the review was done.** Three read-only analyses of the full pre-cleanup archive (`e73e495`:
381 files, 557,030 words; 530 game commits), run one at a time: packet lifecycle, execution and
acceptance, and project-level history. Plus my own reading of `docs/agent-workflow.md`, the current
plan, the technical record, the original milestone ladder
(`docs/jsrf-operating-history.md:1806-1880`), and the fork audit and fixes (TR §7). Figures from the
analyses are **measured** where they cite a file, line or commit, and marked ≈ where they are
estimates. I spot-checked the ones the plan relies on: 287 commits after the last horizon move,
`AGENTS.md:92`, `docs/jsrf-run-profiles.md:51,54`, the 167 GB / 1,172 run directories in `6a97c86`,
the v0.12 sync commit times, and ordinals 277/294 (`RtlEnterCriticalSection`/`RtlLeaveCriticalSection`).

---

## 1. Findings

### 1.1 Progress vs process

- **The strict horizon moved twice**: 09-24 (AC'97 codec model, `a16350f`) and 09-27 (GP DSP clears
  the pending word, `5c5ffa5`). The second move only regained the stop exploratory runs had reached on
  09-22 22:44. It has not moved since; 287 game commits followed (verified), 207 of them A2h.
- 21 of 530 commits (4%) moved any stop, 19 of them in the first 36 hours on the exploratory profile.
  443 (84%) touch only docs; 84 (16%) are self-corrections. (Commit categories are the analysis's
  heuristic, ±5%.)
- The reset when exploratory runs were found to be mislabelled strict (09-22, `c5f5b80`) cost 4.7 days
  to regain the same stop.
- Owner-direct chores were fast where packets were slow: the v0.11 sync as packet A4s took 19.6 h and
  six revisions; the v0.12 sync landed as one merge plus one record commit seven minutes apart;
  regeneration, deferred all week, took ~30 min between commits and exposed the dropped `rcr`.

### 1.2 Packet lifecycle (≈108 blocking defects categorised)

| Pattern | Cost (≈ senior calls) | Examples |
|---|---|---|
| Patching the patch on one criterion or mechanism | 45 | A3a AC2 r6–r10 ("each was introduced by the previous repair"); P0.2 AC2; A4b watch log → ledger → `[GPIN]`; A4b2 AC-BOOT r3–r5; NR L1 four rounds |
| Premise or instrument never qualified before planning | 40 | A2h-r6: six revisions on a failure fixed by `cb7cae2` before r1; DR0 watch: six packets, never fired; page-guard rejected at 00:38 and adopted at 09:32 by the same Advisor handle |
| Review churn on non-blocking prose | 25 | A3a r12–r24, 13 ADEQUATE revisions with zero blockers; A4s blocking:non-blocking 4:29 |
| Enumeration by spelling recurring despite written rules | (13 defects) | PIO_FREE 10 of 28 sites; "exactly three writers" were 185 |
| Frozen commands never run | 15 | A4s-r4 duplicate anchors; A4s-r5 PowerShell 5.1 grep; 7 of 12 A4s Advisor rulings interpreted frozen text |

Discovery packets after the reform used 2–4 senior calls; change packets 15–27.

**The Session orchestrated well; its errors were one narrow kind, fixed by tools.** The Session ran
every build, run and review round, validated frozen commands (16/16 and 36/36), and caught defects in
commands that senior Planners had frozen (the PowerShell 5.1 grep, the CRLF hash hazard). Its recorded
errors are values written into records by hand — 12 extraction errors, e.g. "a transposed address I
wrote from memory" (`251d770`) — and git bookkeeping (`141cb7e`, `d82f388`). The records do not name
the model behind each error, and a Codex Session route existed alongside DSH, so they are not
attributable to DeepSeek specifically. The remedy is tooling (T10 citation lint, W16 explicit staging,
T13 receipts), not moving orchestration: the plan keeps DeepSeek as orchestrator and widens its lane
(chores without senior sign-off, work through senior outages, the pre-Planner gate). Most of the
expensive failures — unqualified premises, instrument designs, a reversed ruling, false ACCEPTs —
were approved at senior gates.

### 1.3 Execution and acceptance

- 21 acceptance reviews covered 25 packets; 6 first-stage non-ACCEPTs, **none overturned on the
  merits**; the second stage ran twice; the Sol adjudicator was never used (the role has since
  been removed). So removing the second stage (workflow `e6397ed`) loses little measured value.
- The costly failures were **false ACCEPTs and unqualified instruments**: the OOM slice was accepted
  on a false premise found 78 minutes later; named-producer-frame cited dump reads as exact against
  its own `CONTENT_MISMATCH` table; 7 of 10 A2h acceptances were edited after review, 2 of them
  changing the selected row, with one confirmation pass that found a half-applied correction.
- At least 13 distinct A2h instrument defects (wrong page by 64 KiB, DR6 test always true, armed the
  wrong threads, native bytes decoded as guest instructions…). ~70,000 words went into making
  instruments trustworthy against ~13,000 for the attribution that produced the answer.
- Startup probes passed 13/13 while all 5 senior-route outages happened mid-session.
- 134 of 349 records were never cited by another packet's files; 31 of 36 session verification
  records were never cited. Rulings and packets are what later work reused.
- `scripts/record-review.py` captured only the three P0 reviews; everything later was transcribed.

### 1.4 Incidents

- The DSH roster changed 7 times in 6.5 days; ~45 h of gaps around senior-route outages; on 09-25 an
  owner-authorised fallback was missing from the session allow-list.
- Lost state: an unrecorded ACCEPT repeated a diagnosis (`a90b8ab`); `git add -A` swept a Planner
  draft twice (`141cb7e`); a hash pinned mid-write (`d82f388`).
- Disk: 93 GB of logs flagged with "no retention script" on 09-22 (`6c085a3`); exhaustion misread as a
  race on 09-28 (`6a97c86`: 0.51 GB free, 167 GB across 1,172 run directories).
- Stale policy: `AGENTS.md:92` and `docs/jsrf-run-profiles.md:51,54` still describe `RECOMP_AC97_READY`
  and `RECOMP_APU_DSP_ACK` as live (removed in toolkit `c97ce2c`/`8f8f6e4`); `RETIRED_NAMES` lacks
  kimi-k3, claude-opus-5-5 and hy4.
- Dead ends: the 571 MB allocation as terminal (~5 days; refuted by `test eax`/`jl` four instructions
  after the call); the static slot-writer chain (~8 packets; a "writer found" rejected 14 minutes
  later, then the live watch found a different writer).

### 1.5 The workflow document itself (my reading)

- It is ~950 lines of dense prose with many cross-references, executed literally by a cheaper model
  that must escalate on ambiguity. Several rules were added reactively ("Measured cost of not doing
  this") and some sit before the checklist they belong to (§6.1). A formatting slip in §6.1 fuses
  "about*every*". Rules that can be scripts should be scripts; the checks that already are scripts
  (profile classifier, dump `.text` control, merge-structure scan) are the ones that held.
- There is no class for owner-directed mechanical work, so every sync, regeneration and import ran
  outside the lifecycle or through a full packet.
- Any unavailable route blocks accepted work, including work DeepSeek could do alone.
- With the Planner, Advisor and reviewer all on Muse Spark since `e6397ed`, independence was procedural
  only (separate handles). **Superseded the same day by owner staffing decisions:** the Advisor is now
  Claude Opus 5.5 (a continuable child) and the final adjudicator role is removed, with
  frozen-criterion interpretation disputes going to the Advisor; the Planner and reviewer stay Muse
  Spark on separate handles.

### 1.6 What worked (kept)

Enumerating a whole class instead of one instance (44 CRT initializers and 1,111 vtable methods
recovered in hours on 09-21); refuting from archived artifacts before running anything; machine checks
with controls; independent acceptance (the terminal REJECT stopped a false "writer found"); Advisor
ceilings; and mining other forks and decompilations (TR §7), which found an `NtFreeVirtualMemory`
failing every call and always-false branches that the packet process had not.

---

## 2. Upstream and merge impact on the plan

| Change | Plan effect |
|---|---|
| Toolkit `jsrf/fork-fixes` kernel memory semantics (`1d85934` etc.) and data-export thunks (`4b4a62d`) | strict horizon must be re-measured before anything inherits it (V3); M09 largely delivered; C2 for leftovers |
| Lifter fixes (`_flags` joins, narrow mul/div, `movsx`, jump tables) | regeneration required (V2); 8 always-false JSRF branches become live |
| Runtime template port-I/O prototypes (`f61a0af`) | picked up at V2; game implicit declarations cleaned in C6 |
| NV2A action methods behind `RECOMP_NV2A_ACTIONS` (`6864f1f`…) | C3 owner admission decision; fence mirror retirement depends on it |
| BearddOddity pr-b executor (`a253876`) | exploratory rendering preview only; C5 decides the strict rendering path using its back end |
| BearddOddity pr-a (`c4adb9b`): OHCI DATA UNDERRUN, pseudo-handles, `STATUS_CONFLICTING_ADDRESSES` | input (M16) and heap-grow retries |
| Upstream v0.12 USB: 4 ports, gamepad enumeration, GET_REPORT, keyboard stand-in | M16 re-scoped to verification |
| Upstream async reads (`RECOMP_ASYNC_IO`), DVD media check | M08 decides async I/O from JSRF's open flags |
| Upstream ADPCM reserved-byte fix, mixbin mixdown | M23 carries them; JSRF's `0x08` pad no longer silences 3.5–4.4% of blocks |
| Upstream `--split 250` recommendation (#109) | V2 fallback if MSVC runs out of memory |
| Upstream function coalescence (opt-in), switch-arm entries, returning-body probe | V5 audits `recovered-functions.json` for obsolete workarounds |
| Upstream icall site guards (#130), `[UNIMPL]` markers | better diagnostics at the stop; V3/V4 read them |
| Upstream `NtQueryDirectoryFile`/counted paths (reported to break another title, fearkov `341cb66`) | M25 regression test on JSRF's save enumeration |
| Upstream provenance policy (#132) | our GPL DSP port and GPL-3.0 fork ideas cannot go upstream; C8 fork scans record licence |

---

## 3. Decisions

Each: **decision** — reason — what would reverse it.

- **D-01 The refresh is PROPOSED; `plan-jsrf-bare-minimum.md` stays the authority until adopted.** —
  The workflow names one plan file as the execution authority and `check-agent-docs.py` checks it;
  switching silently would confuse the next DeepSeek session. — The owner adopts it (replace the old
  file and delete the refresh).
- **D-02 Base: the current plan plus the original milestone ladder 00–36**, not the 4,992-line
  pre-cleanup plan. — The pre-cleanup plan is a packet diary with no milestone ladder; the ladder and
  its acceptance checks live in the operating history. — The owner wants the pre-cleanup structure.
- **D-03 Phase 0 re-baselines before any packet.** — The fork fixes change strict behaviour and are
  unverified on Windows; the history shows a mislabelled baseline cost 4.7 days. — V1–V3 show the same
  stop and counters as `db96e30`, in which case Phase 0 shrinks to V1.
- **D-04 The specified A2h successor is replaced by C1 (one TTD recording).** — ~70,000 words and 13
  defects went into watch instruments; a TTD trace is lossless and reproducible by a reviewer; the
  successor's host/guest split rests on a wrong premise (the memset writer is lowered guest code). —
  TTD cannot record the process usefully (trace size, VEH interplay, anti-debug); then the specified
  successor runs under W9's control-first rule.
- **D-05 TTD output as a decision input goes to the Advisor (W11); I did not amend the run profiles.**
  — Evidence admissibility is Advisor policy (workflow §3.3). — The owner rules directly.
- **D-06 Tooling tasks are chores with zero senior calls.** — They are mechanical, which is where
  DeepSeek is strong. — A tool that changes admitted evidence semantics becomes a change packet.
- **D-07 Workflow changes are plan tasks (W1–W16), not edits made now.** — The request was a plan;
  each change needs its check built alongside it, and staffing/policy edits are owner-visible
  decisions. — The owner asks for them to be applied now.
- **D-08 Every DoD row stays strict; strict graphics is gated by C5.** — Run-profile rules; the
  executor answers kickoffs synthetically. — The owner decides exploratory rendering is acceptable
  for the bare minimum (an objective decision, workflow §3.4).
- **D-09 Proposed rendering target: the strict model's committed methods drive the pr-b back end.** —
  Keeps kick/GET completion real (the contract) while reusing working rendering code; the alternative
  needs synthetic acks. — The Advisor preflight redirects, or discovery shows the back end lacks too
  much of JSRF's first frames.
- **D-10 xemu is the oracle for graphics and audio acceptance, with named fallbacks.** — The oracle must
  be independent of the implementation (§6.1.7). Needs the owner's BIOS/MCPX ROM/HDD image. — No
  images (fallbacks apply) or the owner rejects xemu.
- **D-11 M16 input is re-scoped to verification.** — Upstream v0.12 and BearddOddity deliver the USB
  gamepad path. — M16 shows JSRF's XAPI input fails on the toolkit model.
- **D-12 Milestone 06b is closed into the fork fixes.** — Data exports, memory and file-status fixes
  cover the known gaps. — A measured call to an unbridged import.
- **D-13 Starting thresholds: SSIM ≥ 0.95 (UI primitive) / ≥ 0.90 (scenes), audio correlation ≥ 0.9,
  15-minute soak, frame-time p99 ≤ 2× median.** — Measurable starting points; thresholds are technical
  choices the Planner/Advisor may tune, not owner decisions. — First reference measurements show noise
  above them.
- **D-14 Senior-call budgets per task (C1: 1 Planner + 1 acceptance; C3: ≤ 2 Advisor; C5: 1 Advisor +
  1 Planner; chores: 0).** — Derived from measured per-packet costs. — The Advisor revises them.
- **D-15 Ceiling rule at 3 packets or 4 hours without horizon movement.** — The NULL-line ceiling came
  after 5 packets; 287 commits without a move. — The Advisor sets a different bound.
- **D-16 Fork scans every two weeks and upstream merges per release, as chores.** — The fork audit
  found fixes the process missed; the v0.12 sync was fast as a chore. — The owner prefers another
  cadence.
- **D-17 Log deletion stays an owner decision; T14 proposes a policy and adds a free-space gate.** —
  Deleting runs is destructive (workflow §3.4) and runs are evidence. — The owner approves the policy.
- **D-18 Owner-named fallback routes per senior role (W8).** — Staffing is an owner decision; the
  09-25 allow-list gap. — The owner prefers BLOCKED on outage.
- **D-19 I did not fix the stale documents** (`AGENTS.md:92`, run profiles `:51,54`, `RETIRED_NAMES`,
  `record-review.py` default, the §6.1 formatting slip); they are W15 and T11. — Scope was the plan
  and report. — The owner asks for them now.
- **D-20 This report lives at the repository root** despite AGENTS.md's "do not keep session reports
  in the repository". — The owner asked for it by name; it is a decision record, not a session report.
  — The owner moves or deletes it after review.
- **D-21 Carried open items keep their dispositions** as listed in the plan's §10 (none dropped
  without a destination).
- **D-22 Committed on `jsrf/fork-fixes`, not pushed.** — The standing instruction for this line of
  work is commit without push. — The owner pushes.
- **D-23 The analyses ran one subagent at a time; their figures are used as reported, with the
  load-bearing ones spot-checked (list above).** — Owner instruction; re-deriving every count would
  repeat their work. — A figure the plan relies on is found wrong; the plan row is corrected.

## 4. Limits

- Nothing in the plan has been run on Windows; V1–V4 are the first measurements.
- The TR §7 D3D field names are inferred from Halo's XDK and must be confirmed (V4).
- The ≈ figures (senior-call counts, word counts per sub-line, commit categories) are estimates from
  the analyses; the measured ones cite a file, line or commit.
- The rendering target (D-09) and TTD admission (D-05) are proposals pending Advisor rulings.
