# Jet Set Radio Future: Windows port plan — refresh

**Status: PROPOSED (2026-09-29).** `plan-jsrf-bare-minimum.md` remains the execution authority
until the owner adopts this file (adoption: replace `plan-jsrf-bare-minimum.md` with this content
and delete this file). Every decision taken while writing it is recorded, with its reason and what
would reverse it, in `report-jsrf-bare-minimum-refresh.md`.

Authorities are unchanged: `docs/agent-workflow.md` owns roles and the packet lifecycle,
`docs/jsrf-run-profiles.md` owns evidence profiles, `docs/jsrf-technical-record.md` ("TR §n") owns
established facts. Where this plan proposes a change to one of them, it is a task (§6), not an edit.

---

## 0. What changed from the current plan

- **Built on the whole history**, not only the last packet: the original milestone ladder
  (`docs/jsrf-operating-history.md:1806-1880`), every closed packet, the fork audit and fixes
  (TR §7), and three reviews of the archive at `e73e495` (summarised in the report).
- **Phase 0 re-baselines on Windows first.** The toolkit on `jsrf/fork-fixes` changes strict-path
  behaviour (kernel memory, data-export thunks) and, after regeneration, generated code. Nothing
  inherits the old horizon until it is re-measured.
- **The A2h successor is replaced by one Time Travel Debugging recording** (C1). The archive shows
  ~70,000 words spent making watch instruments trustworthy against ~13,000 for the answer; a TTD
  trace is lossless by construction and a reviewer can re-run the same query.
- **Tooling is planned work** (§5): TTD, XbSymbolDatabase, an xemu oracle, Windows CI, clang-cl,
  `just`, pre-commit, DuckDB log queries, a checked-in enumerator and a citation lint.
- **Workflow fixes are tasks** (§6), each tied to a measured failure pattern, with DeepSeek kept as
  Session and workers and the limited senior models spent only at named gates.
- **Acceptance criteria are measurable:** every milestone names its profile, artifact, oracle and
  PASS/FAIL predicate (§8). Rendering milestones use xemu reference frames where available.
- **Upstream changes re-scope milestones:** input is now largely toolkit-provided (verify, not
  build); audio carries upstream ADPCM/mixdown fixes; the pushbuffer executor is an exploratory
  preview only; a rendering-architecture decision (C5) gates every strict graphics milestone.
- **The strict horizon becomes the progress metric** (§3). It moved twice in nine days (09-24
  AC'97, 09-27 GP DSP); 21 of 530 game commits moved any stop, and 287 commits followed the last
  move without moving it again.

## 1. Objective and definition of done

Port JSRF to Windows by static recompilation. **Minimum playable slice** — all of the following,
each under the **strict** profile unless the row says otherwise and the run-profile document admits
it:

| DoD | Criterion | Measured by |
|---|---|---|
| DoD-BOOT | Launch to the title screen with no synthetic-completion switch; the title frame is presented. | M15 |
| DoD-INPUT | A host controller drives menu navigation through the guest's own XAPI input path. | M16–M17 |
| DoD-PLAY | New game loads the opening area; the player skates, turns, jumps and the camera follows. | M18–M21 |
| DoD-GRAFFITI | One graffiti interaction completes and the game's own progression state records it. | M22 |
| DoD-AUDIO | Sound effects and music are audible at the correct pitch through the modelled APU path. | M23–M24 |
| DoD-SAVE | Save, exit, restart and resume to the saved location/progression; a missing save is handled. | M25 |
| DoD-STABLE | A 15-minute soak with no invalid indirect call, no ABI violation and bounded memory. | M26 |

A window opening is not the slice; one playable scene is not the game.

## 2. Baseline

| Item | State |
|---|---|
| Toolkit | branch `jsrf/fork-fixes` at `2a349c8` (base `db96e30` = upstream `ea60cfa` + local), pushed; **not built or run on Windows** |
| Game | branch `jsrf/fork-fixes`; generated tree is still the 2026-09-28 v0.12 regeneration |
| Last measured strict horizon (old toolkit `db96e30`) | ~5 s: `call dword ptr [0x1C4064]` at `0x00149828` (in the title allocator `sub_001497DC`) reads 0 → `[ICALL] invalid target` `0xE0424943`; the slot is ordinal 277 `RtlEnterCriticalSection`; the last bridge call before it was ordinal 294 `RtlLeaveCriticalSection` (TR §5, run `20260928-185612-449-regen-v012-strict`) |
| Established | AC'97 codec-ready model (TR §3); GP DSP56300 port and the GP clearing the DSP pending word (TR §4); CRT 64-bit divide helpers (TR §2); the slot writer `sub_00038530` (TR §5, row `O-OPEN`) |
| Inferred, to verify | XDK D3D device fields `+0x242C` vblank callback, `+0x2430` vblank event, `+0x2440` busy-block event, and five function names (TR §7) |

---

## 3. How work runs under this plan

- **DeepSeek Session and workers execute everything by default** (unlimited). A senior call
  (the Planner, Advisor and Acceptance reviewer of `docs/agent-workflow.md` §1) is made only at
  a gate a task names, and each task states a **senior-call budget**; exceeding it stops the
  task for an Advisor continue/stop decision.
- **Three task classes.** *Chore* — owner-directed mechanical work (builds, syncs, regeneration,
  tooling, record fixes): no packet; recorded in the TR with commands and results; may not change
  admitted evidence semantics except behind a switch classified in `docs/jsrf-run-profiles.md`
  (pending W7, chores run as owner-directed changes, as the syncs and regeneration already did).
  *Discovery* and *change* — packets under `docs/agent-workflow.md` §5.
- **Acceptance criteria** name: profile · artifact path · oracle (independent of the
  implementation) · PASS predicate · FAIL/UNKNOWN predicate. The rows in this plan give the profile,
  artifact, oracle and PASS predicate; the packet that executes a row adds its FAIL/UNKNOWN rules and
  controls. A packet criterion without all five is not ready to freeze.
- **Values in records come from tools**, never hand transcription, once T10 lands; until then a
  second DeepSeek worker re-reads every value against the artifact before it is used (W5).
- **The strict horizon is the progress metric.** Each session a DeepSeek worker appends one line
  to `docs/reviews/strict-horizon-ledger.md`: date, toolkit/game revisions, run ID, stop site,
  stop time. A line of work that goes **3 packets or 4 hours** without moving the horizon or
  producing an accepted finding on the critical path gets one Advisor ceiling call (continue with
  a stated bound, or stop) (W14).
- **Before any Planner call** the DeepSeek premise checklist runs (W2): disassemble past the
  failing instruction; the evidence runs are strict and postdate known fixes; the instrument's
  positive control fires on a known event; upstream, forks and reference decompilations are
  searched for the same symptom.

---

## 4. Phase 0 — verify and re-baseline on Windows (chores; DeepSeek; 0 senior calls)

**V1 — Build and test the branch.**
- Do: check out `jsrf/fork-fixes` in both repositories (toolkit first); `python -X utf8
  scripts\build-jsrf.py`; `ctest` in the game build and in a standalone toolkit build.
- PASS: build exit 0; every ctest passes, including the toolkit's `xbox_kmem`, `xbox_guest_meter`,
  `nv2a_actions`, and the standalone `tests/kernel_data_exports` and `tests/kernel_file_status`
  projects; test counts recorded. FAIL: any build or test failure → fix before V2 (a toolkit
  failure is fixed on the branch; its commit cited).

**V2 — Regenerate with the branch's lifter.**
- Do: the TR §2 command, unchanged inputs; if MSVC runs out of memory, `--split 250` (upstream's
  recommendation for 15 GB hosts). Re-apply `relift-selected.py boundaries`, the ABI deltas, the
  A4b2 hooks; re-record provenance (`check-generation-provenance.py --write`).
- PASS: build + ctest pass; the generator's `FLAGS:` report lists ≤ 9 leftover `_flags` reads, each
  named; function count and dispatch count recorded against 5740 / 8928; provenance `--check` ok.
  FAIL: more `FLAGS:` sites than 9, or any new `[UNIMPL]` reached in V3.

**V3 — Re-baseline the strict horizon.**
- Do: (a) strict run `RECOMP_GPU_ACK=0 RECOMP_APU_TRAP=1`, log budget 100000, label
  `rebaseline-strict`; (b) the same with `RECOMP_KMEM_LEGACY=1` (exploratory A/B); (c) the same as
  (a) with `RECOMP_GUEST_METER=1` (observation).
- PASS: `check-run-profile.py` classifies (a) and (c) strict, (b) exploratory; the stop site,
  time and last 20 kernel calls are recorded for each; the `[KMEM] summary` counters (reserve hint
  at `0x1495E3`, commit at `0x14961B`), every `data export ordinal` line, and the `[GMETER]` line are
  archived; TR §5 "Where strict runs end" is updated with the new horizon. UNKNOWN: a run that
  cannot be classified.

**V4 — Read-only checks.**
- Do: `inspect-jsrf.py disasm` at `0x0018CE30`, `0x0018CE50`, `0x00193D90`, `0x00194210`,
  `0x00193F70`; compute `sub_00038530`'s object base from the A2h artifacts; count
  `RtlRaiseException`/`0xE06D7363`/`[UNIMPL]` in the V3 logs.
- PASS: each TR §7 inferred name marked CONFIRMED or REFUTED with the bytes quoted; the base and
  whether it overlaps `g_Device` (`0x0019B200`–`0x0019DCE0`) or the page of `0x001C4064` recorded.

**V5 — Recovered-functions audit (discovery-sized chore).**
- Do: for each entry in `config/recovered-functions.json`, check whether the v0.12+ translator
  (switch-arm entries, returning-body probe, opt-in `--coalesce-functions`) now produces it natively.
- PASS: a table entry → {still needed, obsolete, unknown} with the generated-code evidence;
  retirement of obsolete entries is a later change packet.

## 5. Phase 1 — tooling (chores; DeepSeek; senior calls only where named)

| ID | Tool | PASS (each with a control) |
|---|---|---|
| **T1** | **WinDbg TTD**: `just ttd-record <label>` records a strict run; `tools/ttd/writes.js` (dx query) lists every write to a guest VA **across all 29 aliases** (base + 28 mirrors) with thread, native IP, symbol, position | on one trace, the query finds the runtime's own thunk-install write to `0x001C4064` (known positive), and zero writes to an address never written (known negative) |
| **T2** | **XbSymbolDatabase** (MIT, external CLI) → `config/xdk-symbols.json`; names merged into `inspect-jsrf.py` output and linker-map symbolization | ≥ 300 names (DanielJVoxSmart measured 363 on this XBE); five spot checks against known functions (`__aulldiv` `0x0017D4D0`, etc.) agree |
| **T3** | **xemu oracle** (requires the owner's BIOS, MCPX ROM, HDD image): gdbstub recipe that dumps guest memory/registers at a named guest PC; `scripts/xemu-diff.py` compares with the same checkpoint in a recomp run | the diff of a checkpoint against itself is empty; a seeded one-byte change is found. If the images are unavailable: BLOCKED (owner assets) and milestones fall back to non-xemu oracles |
| **T4** | **Windows CI** for the toolkit fork (GitHub Actions `windows-latest`, MSVC, ctest) | green on `jsrf/fork-fixes`; a deliberately failing test turns it red |
| **T5** | **clang-cl + MSVC `/analyze`** configurations of the toolkit | baseline warning counts recorded; the known `%lld`-with-`int` class and implicit declarations are reported by at least one of them |
| **T6** | **`just`** recipes: `build`, `test`, `regen`, `strict-run`, `explore-run`, `check`, `ttd-record`, `doctor`, `analyze` | every recipe runs on the Windows host; AGENTS.md "Build and run" points at them; `check-agent-docs.py` verifies the named recipes exist |
| **T7** | **pre-commit** hooks: `check-agent-docs.py --check`, `secret-audit.py` on staged blobs, no `game/` path, run-profile tests, `check-merge-structure.py` when a merge is in progress | a staged `game/` path and a planted fake token are both refused |
| **T8** | **DuckDB log queries**: `scripts/logq.py` loads kernel/`[KMEM]`/`[ALIAS-ICALL]`/`[GMETER]`/`[ICALL]` lines into tables; saved queries under `tools/queries/` | reproduces a known count from an archived run (e.g. the 15,498 sampled bridge boundaries of A2h-null-slot-triage) |
| **T9** | **Enumerator**: `scripts/enumerate-accesses.py` — operands normalised to `uint32`, recursive-descent from the entry and dispatch seeds, raw-byte fallback, each run printing its own known-answer controls | reproduces `PIO_FREE` = 28 sites (10 hex + 18 decimal spellings) and the vtable base = 3 references (TR §6) |
| **T10** | **Citation tool + lint**: `scripts/cite.py` records value, artifact, command, hash; the memory reader prints both byte orders and an offset-shift control; a lint rejects hex literals in records that no cited output contains | the lint flags a seeded transposition (`0x00193D62` for `0x00193D96`, the recorded historical error) |
| **T11** | **Review capture**: `record-review.py` defaults to the current reviewer route (GPT-6 Sol child) instead of the retired Hy4 route | records a Sol child's review from its session log with its hash; the existing DSH tests still pass |
| **T12** | **Doctor per run**: `tools/doctor.py --runtime-log` writes `doctor.json` into every archived run | present in the V3 runs |
| **T13** | **Startup receipt generator**: fills `docs/session-start-template.md` from harness metadata and live route checks | a generated receipt passes `check-agent-docs.py` |
| **T14** | **Run-log retention**: `scripts/logs-reclaim-plan.py` + `scripts/disk-usage.py` as a pre-run gate (refuse to launch below a free-space floor) and a retention policy: keep every run cited by the TR, the plan, a packet or a ruling, plus the last 20; archive or delete the rest **after owner approval of the policy** (deletion is an owner decision) | a launch below the floor is refused with a clear message; the policy lists how many runs/GB it would reclaim on the current host before anything is removed |

Order: T14's pre-run gate, T6, T7, T4 first (they make every later step mechanical and stop the disk
failure mode recurring), then T1, T2, T8, T9, T10, T11, T12, T13, T5; T3 whenever the owner
provides the images. Senior calls: T1's admission ruling (W11) only.

## 6. Phase 2 — workflow changes (tasks; each edits the owning document and adds a check)

Each row names the measured failure it answers (report §2) and is accepted when the document is
edited, `check-agent-docs.py` passes, and the named check exists with a failing control.

| ID | Change | Answers | Check |
|---|---|---|---|
| **W1** | Blockers carry `criterion-ID` and a category; the same criterion ID blocking two consecutive INADEQUATE verdicts forces redesign or a discovery packet; only the Advisor exempts | patch-the-patch loops (A3a AC2 r6–r10, P0.2 AC2, A4b watch→ledger→`[GPIN]`, A4b2 AC-BOOT) | `check-recorded-reviews.py` flags a repeated ID |
| **W2** | **Qualification gate** before any Planner call, run by DeepSeek: disassemble past the failing instruction; premise artifact vs later fix commits (`git merge-base --is-ancestor`) and strict profile; the positive control firing on the target build; a dry run of every measurement; a search of upstream, forks and reference decompilations for the symptom | unqualified premises/instruments (A2h-r6 six revisions on a fixed failure; the 571 MB premise ~5 days, refuted by the `test eax`/`jl` four instructions after the call; 15 DR0 ON runs before zero hits were noticed; the slot field named by the Halo XDK layout only after the owner's audit) | gate script output attached to the brief |
| **W3** | Freezing requires a DeepSeek **dry-run transcript**: every command executed on the target host and tree, output hashed in the packet | frozen commands that never ran (A4s-r4 anchors, A4s-r5 PowerShell 5.1 grep; 7 of 12 A4s Advisor rulings) | promotion refuses a packet without a transcript |
| **W4** | Reviews return **BLOCKING findings only**, dispositions `ACCEPT`/`NOT ACCEPTED` only; the review stores the evidence hash and a hash change flips it to pending; record hygiene is a lint that must pass before review | churn on prose (A3a r12–r24; OOM slice 5 rounds on sweeps); 7 of 10 A2h acceptances edited after review | record schema + hash check |
| **W5** | Values transcribed by hand into a table or record, by any role, are re-checked row by row by a second DeepSeek worker against the artifact before a criterion or ruling uses them (until T10 makes tool citation the only path) | 12 recorded extraction errors; a byte reversal that became a "permanent" refutation for 35 minutes | `verified-by` field required |
| **W6** | **Ruling ledger per question** (`docs/reviews/rulings/<question>.md`): a new ruling lists the hazards of earlier rulings it supersedes | A2h page-guard rejected at 00:38, adopted at 09:32, then failed on the hazard cited at 00:38 | ledger lint |
| **W7** | **Chore class** in §5.8 for owner-directed mechanical work (§3 above), with a scripted gate instead of Planner and acceptance review: build, ctest, and a strict A/B run showing the same stop (or the change in stop recorded as the finding) | the v0.11 sync as packet A4s took 19.6 h and six revisions; the owner's direct v0.12 sync (121 upstream commits) landed as one merge commit and one record commit seven minutes apart (`2925f0b`, `32680d7`); regeneration was deferred a week and then took ~30 min between commits (`0f7ef9c` → `e73e495`) and exposed the dropped `rcr` | table row + gate script |
| **W8** | **Lazy route checks and pre-authorised fallbacks**: verify a route at first real use; no Advisor probe turns at startup; the owner names one fallback route per senior role in §1 (owner decision), and the session allow-list includes it; during a senior-route outage DeepSeek continues pre-authorised chores and discovery execution | probes passed 13/13 while all 5 outages happened mid-session; ~45 h of outage-adjacent gaps; on 09-25 the owner-authorised fallback was missing from the allow-list and needed a new session | T13 receipt |
| **W9** | **Control-first instruments**: `run-jsrf.py` refuses a second ON run, and checkers refuse to emit a row, until the raw log shows the named positive-control event at an independently derived address | 13 A2h instrument defects; a positive control silently weakened to a value comparison | fixture test |
| **W10** | Each packet lists its load-bearing **premises with byte-level commands**; the reviewer re-runs them first; a lint rejects values cited from a `CONTENT_MISMATCH` dump or a run with tracing off | false ACCEPTs on false premises (OOM slice, named-producer-frame) | lint |
| **W11** | Advisor ruling: **TTD query output as a decision input** (lossless, reproducible) recorded in `docs/jsrf-run-profiles.md` | enables C1 without new watch instruments | the ruling text in the owning document |
| **W12** | Session "verification" prose is replaced by script JSON; review records keep rulings and packets, which are the only records later work reused | 31 of 36 session verification records never cited (38,972 words) | record lint |
| **W13** | Per-task **senior-call budget** (§3), recorded in the packet; exceeding it is an Advisor continue/stop decision | change packets consumed 15–27 senior calls; discovery 2–4 | packet template field |
| **W14** | **Strict-horizon ledger and ceiling rule** (§3): one line per session; 3 packets or 4 h without moving the horizon or an accepted critical-path finding → one Advisor ceiling call | the NULL-line ceiling came only after 5 packets; 287 commits after the last horizon move | ledger lint: a session with a run and no ledger line fails |
| **W15** | **Single-source, drift-checked text**: override descriptions in `AGENTS.md`/run profiles derived from `scripts/jsrf_run_profile.py`; `check-agent-docs.py` fails on override names the runtime no longer reads and on retired routes (add kimi-k3, claude-opus-5-5, hy4-preview-f to `RETIRED_NAMES`, updating the test fixtures) | `AGENTS.md:92` and `docs/jsrf-run-profiles.md:51,54` still describe `RECOMP_AC97_READY`/`RECOMP_APU_DSP_ACK` as live (removed in toolkit `c97ce2c`/`8f8f6e4`); stale `RETIRED_NAMES` | checker control: a planted stale name fails |
| **W16** | **Stage explicit paths only**; the Session never runs `git add -A` while another role may be writing; hashes are pinned only after the writer reports done | `git add -A` swept a Planner's in-progress draft twice (`141cb7e`); a hash pinned mid-write (`d82f388`); a lost ACCEPT record (`a90b8ab`) | pre-commit warns on staged draft packets not named in the commit message |

Senior calls: W11 is one Advisor ruling; the rest are owner-approved document edits (0).

---

## 7. Phase 3 — critical path to boot (after V3)

**C1 — Attribute the slot write with TTD (discovery).** Replaces the specified A2h successor.
- Question: which code writes the value the terminal read sees at `[0x1C4064]`, and which writes
  device `+0x242C`? If V3 moved the stop, C1 targets the new stop's first bad value instead.
- Experiment: T1 recording of one strict run; the T1 query over all 29 aliases of each address;
  map native IPs to guest functions with the linker map and T2 names.
- Outcomes: **O-GUEST-FILL** (a lowered `rep stos`/`memset` in guest function F; next: why F's
  destination overlaps — heap/allocator packet); **O-ALIAS** (a write through a mirror VA above
  64 MB; next: the allocation that handed out that range); **O-HOST** (a toolkit function; next:
  a toolkit fix); **O-DATA-CALL** (the slot holds `0x001D5078`-style data; next: the object
  overlap with `g_Device` from V4); **O-UNKNOWN** (no write found with the positive control
  present → the read path, not a writer; re-plan).
- Acceptance: the query artifact, its positive control (the install write) present, and the row
  selected by rule. Senior budget: 1 Planner (self-review), 1 acceptance review.

**C2 — Kernel memory follow-ups (change, only if V3 or C1 implicates them):**
`NtQueryVirtualMemory` consulting the region registry; partial `MEM_RELEASE`; whether to keep the
reserve clamp; heap blocks untracked after 65,536 entries; KeSystemTime/KeInterruptTime advancing
(a new autonomous-clock model → admission under the run-profile rules).
Accepted when: each implicated item has a `kmem_test` case that fails before and passes after, and a
strict run shows the implicating symptom gone (`[KMEM] summary` counter or stop site named in the
packet).

**C3 — NV2A action methods (owner decision).** Evidence: toolkit
`docs/technical/nv2a-action-methods.md`. Before any run: recover `0x00193F70` (`SoftwareMethod`)
and confirm JSRF's `DEBUG_3` value (bit 20). Then an exploratory run with `RECOMP_NV2A_ACTIONS=1`.
An Advisor ruling is needed on criterion 4 for semaphore release (what "work" means for a
state-capture model) before it can replace the synthetic fence mirror. Senior budget: ≤ 2 Advisor.

**C4 — D3D resource release re-check (discovery).** With contiguous frees now real, does the
regenerated build enter `sub_00192830`; which of its ten callers decides; which build is faithful.
Accepted when: a strict run's trace (`config/trace-functions.json` entry for `sub_00192830`, positive
control on another traced function) records entered/not entered, and the deciding caller's branch is
named from its disassembly; the faithful build is decided against the xemu oracle (T3) or recorded
UNKNOWN.

**C5 — Rendering architecture (Advisor shape preflight, then discovery).** Strict runs capture
NV2A state but render nothing; the pr-b executor renders but only on the synthetic-ack path.
Proposed target: the strict model's **committed** methods drive the pr-b render back end
(`nv2a_backend.h`), so completion stays real and frames are produced. Discovery: which committed
methods of JSRF's first frames the back end lacks. Senior budget: 1 Advisor preflight, 1 Planner.
Accepted when: the Advisor's `SHAPE` ruling is recorded, and the discovery lists, from a strict run's
committed stream, each method the back end handles or lacks, with counts.

**C6 — Game implicit declarations (chore):** declare `recomp_dispatch_init`,
`recomp_delta_allowed`, `dr_tid_exited` and the two test stubs; pick up the runtime template's
port-I/O prototypes at V2; then adopt `/we4013` in the game CMake.
Accepted when: the game builds under MSVC with `/we4013` and the full ctest passes.

**C7 — A4b2 P4 transfer bridge (carried):** re-establish only when a packet inherits P4.

**C8 — Upstream and fork cadence (chore):** on each upstream release, a merge chore with
`check-merge-structure.py` and the run-profile merge rules; every two weeks a DeepSeek fork scan by
content (the TR §7 method) that lists candidate fixes with licence and strict-path effect.
Accepted when: each merge records its hunk inventory and a strict A/B (W7 gate); each scan leaves a
dated list in the TR with a disposition per candidate.

---

## 8. Milestone ladder — bare-minimum slice (07–26)

Rows 00–05 are done; 06a done; 06b ("implement reached imported kernel semantics") is closed into
the fork fixes and re-opens only on a measured unbridged call. Profiles are strict unless stated.
"xemu ref" means a T3 capture of the same checkpoint; without T3, the fallback oracle is named.

| M | Milestone | Acceptance (artifact → PASS predicate) | Notes / upstream |
|---|---|---|---|
| **07** | CRT and game initialization complete | strict run log → `verify-initializers.py` passes, the title's main-loop entry VA (named via T2/doctor) is reached, zero `[ICALL] invalid target`, zero ABI violations | the current stop is inside this milestone (C1) |
| **08** | Paths and first real asset read | strict run → an `NtCreateFile`+`NtReadFile` on a `Media` file whose byte count equals the file size and whose read bytes hash (T10) equals the retail file; a missing file returns `STATUS_OBJECT_NAME_NOT_FOUND` | decide `RECOMP_ASYNC_IO` from JSRF's open flags (upstream async reads); DVD media check is answered upstream |
| **09** | Allocation and ownership | `[KMEM] summary` over a boot to the title: `commit_rejected=0`, `release_failed=0`, `region_table_full=0`; `xbox_kmem` ctest passes | largely delivered by the fork fixes; C2 if not |
| **10** | Timers, threads, synchronization | `[GMETER] anomalies=0`; every wait on the boot path satisfied by a modelled cause (vblank count, event signal) in the log; no timer-thread spin | DPC drain fix (`cde1ccb`); KeSystemTime is static until C2 |
| **11** | Graphics interception decided | C5 decision recorded; a fixture feeds committed methods to the back end | gates 12–15, 19–20 as strict |
| **12** | Window and clear | strict run → ≥ 60 presented frames whose clear colour follows the guest's own `SET_COLOR_CLEAR_VALUE`/`CLEAR_SURFACE`; frame hashes archived; oracle: xemu ref of the first clear, else the method parameters themselves | the executor may preview this exploratorily |
| **13** | One game-owned UI primitive | first UI frame vs xemu ref: SSIM ≥ 0.95 on the primitive's bounding box; vertex format/viewport/blend recorded | fallback oracle: the pushbuffer's own vertex data rendered by an independent reference rasteriser |
| **14** | One menu texture | the decoded texture bytes (swizzle/format) equal the xemu ref dump, or the title's own source data decoded by an independent decoder | |
| **15** | Title screen | 60 consecutive frames SSIM ≥ 0.90 vs xemu ref; intro FMV handling stated (decoded, or skipped and recorded as exploratory) | Sofdec; prior art: phobos665 Outrun 2 Sofdec work |
| **16** | Controller input | with `RECOMP_USB`, a host pad press produces the guest's own XID report and the title's input state changes (observation hook); `RECOMP_PAD_PRESS` (synthetic) is not admissible | now toolkit-provided: upstream OHCI 4 ports, gamepad enumeration, GET_REPORT, keyboard stand-in; BearddOddity DATA UNDERRUN fix — **verify, not build** |
| **17** | Main menu navigation | host pad drives start/options/back; the title's menu-state global changes as expected; zero invalid indirect calls/ABI violations | |
| **18** | New game loads the opening area | the loader completes; the list of files read with sizes and hashes is archived; the object count global > 0; the transition completes | |
| **19** | Opening scene and character rendered | spawn frame SSIM ≥ 0.90 vs xemu ref; depth and transforms correct at three camera positions | |
| **20** | JSRF's distinctive rendering | cel shading and outlines at three reference views, SSIM ≥ 0.90 vs xemu ref | executor pixel-shader/combiner work (phobos665) is prior art |
| **21** | Skating and camera | position changes monotonically with a held stick; jump and landing observed; frame-time p99 recorded and ≤ 2× the median | |
| **22** | One graffiti interaction | the title's tag-completion progression flag is set and paint count decrements (named globals) | |
| **23** | Sound effects | APU output non-silent at the expected rate; one known effect's PCM cross-correlates ≥ 0.9 with an xemu capture (fallback: the decoded ADPCM source) | GP port follow-ups (NDEBUG asserts, EP routing) and unmodelled AC'97 registers precede this; upstream ADPCM and mixdown fixes are in |
| **24** | Music and streaming | 5 minutes of music with the underrun counter unchanged and memory bounded | determine JSRF's music format first (ADX/other) |
| **25** | Save and resume | save file in the isolated save root; restart resumes location and progression; missing save handled; a regression test covers JSRF's save enumeration calls | watch upstream's `NtQueryDirectoryFile` change reported to break another title (fearkov `341cb66`) |
| **26** | Stability | 15-minute strict soak: zero invalid indirect calls, zero ABI violations, `[KMEM]` live bytes bounded, `[GMETER] anomalies=0`, frame-time p99 bounded | DoD-STABLE |

Graphics, input and audio may be reordered when the boot path demands it (audio already did: the
title restarts itself without a working audio device).

## 9. After the slice (27–36, unchanged scope)

27 area transition · 28 second character and challenge · 29 cutscenes and FMV (Sofdec) · 30 all
areas · 31 missions and encounters · 32 full playthrough · 33 optional content · 34 Windows
hardening · 35 performance (prior art: DanielJVoxSmart's performance analyses) · 36 reproducible
package. Each gets criteria in the same five-part form when it becomes next.

## 10. Carried open items

| Item (current plan) | Disposition |
|---|---|
| D3D resource release changed with the regeneration | → C4 |
| `P0.1-AC1` reopened (moved line numbers) | kept; chore: re-cite by symbol after V2 |
| `PIO_FREE` deferred at `O-OPEN` | kept, with its reopen conditions (TR §4) |
| A4b2 P4 transfer bridge | → C7 |
| Classifier: move `RECOMP_AC97_READY` to `RETIRED_OVERRIDES` | kept; chore with T7 |
| GP port follow-ups (NDEBUG asserts, EP routing) | kept; before M23 |
| AC'97 registers `0xFEC0017C`, `0xFEC00100` | kept; before M23 |
| Game implicit declarations | → C6 |
| Kernel memory open points | → C2 |
| DSP provenance record (A4b2-NR instrumentation not listed) | kept; chore |
| Review capture (`record-review.py` default) | → T11 |
| (new) Stale override text in `AGENTS.md`/run profiles; stale `RETIRED_NAMES` | → W15 |
| (new) Run logs filling the disk (167 GB across 1,172 run directories on 09-28, `6a97c86`) | → T14 |

## 11. Removed or retired

- **The specified A2h successor** (bounded page-watch runs; splitting `unknown` into
  host-identifiable vs unplaceable) → replaced by C1. The split's premise is wrong: the
  `VCRUNTIME140` writes are most likely guest `rep stos`/`rep movs` lowered to host calls (TR §7).
  The page-watch machinery stays in the toolkit as observation only.
- **Milestone 06b** → closed into the fork fixes (data exports, memory, file status).
- **The old A1–A5 roadmap and the pre-reform milestone diaries** → provenance only (history).
- **Startup Advisor probe turns** → W8.
- **The second acceptance stage** → already removed (workflow `e6397ed`).

## 12. Evidence and decision rules (kept)

- `MEASURED` = inspected source/artifact evidence with an identity/procedure; `INFERRED` = a
  hypothesis or expected consequence.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS.
- Only a verified strict run supports strict integration/boot/liveness/device claims; exploratory
  and fixture evidence stays bounded to what it measured.
- A failed measurement cannot be converted to PASS by any role; a post-review change reopens the
  affected criterion.
- Original assets and existing saves remain unchanged.
- Pending W11: TTD query output as a lossless decision input. Pending W5/T10: tool-cited values only.

## 13. Next action

1. **T14's free-space gate**, then **V1 → V2 → V3 → V4** on the Windows host (DeepSeek chores;
   0 senior calls); start the strict-horizon ledger (W14) with V3's result.
2. Alongside, on any host: **T6, T7, T4**, then **T1, T2, T8–T13, T5**; T3 when the owner supplies
   the images. Owner approves the W-row document edits as they land (W7, W8, W15 first).
3. Then **C1** as the first packet (after W11), with C5's Advisor preflight in parallel.

If no packet is promoted in the authoritative plan file, packet implementation is BLOCKED; chores
listed here run as owner-directed changes once the owner adopts this plan.
