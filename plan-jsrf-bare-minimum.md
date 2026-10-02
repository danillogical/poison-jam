# Jet Set Radio Future: Windows port plan — refresh

**Status: ADOPTED by the owner 2026-09-29; this file is the execution authority.** Every decision
taken while writing it is recorded, with its reason and what would reverse it, in
`report-jsrf-bare-minimum-refresh.md`.

Authorities are unchanged: `docs/agent-workflow.md` owns roles and the packet lifecycle,
`docs/jsrf-run-profiles.md` owns evidence profiles, `docs/jsrf-technical-record.md` ("TR §n") owns
established facts. Where this plan proposes a change to one of them, it is a task (§6), not an edit.

---

## Direction (owner decision, 2026-09-30): the bare minimum is pragmatic

- **Take the path of least resistance to the title screen** (DoD-BOOT, M15), then to the rest of
  the slice. The route is §13's **title-screen fast path**; it supersedes the ordering of Phase 3
  until the title screen is reached.
- **Every departure from running the original code on real hardware is recorded** in
  `docs/jsrf-compatibility-ledger.md`, classed as reimplemented, translated, emulated, wrapped,
  stubbed, patched, approximated or intentionally ignored. A shortcut is allowed; an unrecorded
  shortcut is not.
- **Exploratory runs may satisfy bare-minimum milestones** when the run's record lists the ledger IDs
  it relied on (`docs/jsrf-run-profiles.md` §"Pragmatic bare minimum"). Strict runs remain available
  as a diagnostic for fidelity questions.
- **Fast-path steps run as chores** (W7), not packets: no Planner, adequacy review or acceptance
  review per step. The Reviewer checks the milestone (M15) with its ledger IDs.

## 0. What changed from the current plan

- **Built on the whole history**, not only the last packet: the original milestone ladder
  (`docs/jsrf-operating-history.md:1806-1880`), every closed packet, the fork audit and fixes
  (TR §7), and three reviews of the archive at `e73e495` (summarised in the report).
- **Phase 0 re-baselines on Windows first.** The toolkit fixes (`db96e30..2a349c8`) change strict-path
  behaviour (kernel memory, data-export thunks) and, after regeneration, generated code. Nothing
  inherits the old horizon until it is re-measured.
- **The A2h successor is replaced by one Time Travel Debugging recording** (C1). The archive shows
  ~70,000 words spent making watch instruments trustworthy against ~13,000 for the answer; a TTD
  trace is lossless by construction and a reviewer can re-run the same query.
- **Tooling is planned work** (§5): TTD, XbSymbolDatabase, an xemu oracle, Windows CI, clang-cl,
  `just`, pre-commit, DuckDB log queries, a checked-in enumerator and a citation lint.
- **Workflow fixes are tasks** (§6), each tied to a measured failure pattern, with DeepSeek
  workers doing most execution and the limited models (Session, senior roles) spent where only
  they can act.
- **Acceptance criteria are measurable:** every milestone names its profile, artifact, oracle and
  PASS/FAIL predicate (§8). Rendering milestones use xemu reference frames where available.
- **Upstream changes re-scope milestones:** input is now largely toolkit-provided (verify, not
  build); audio carries upstream ADPCM/mixdown fixes; the pushbuffer executor is an exploratory
  preview only; a rendering-architecture decision (C5) gates every strict graphics milestone.
- **The strict horizon becomes the progress metric** (§3). It moved twice in nine days (09-24
  AC'97, 09-27 GP DSP); 21 of 530 game commits moved any stop, and 287 commits followed the last
  move without moving it again.
- **Mercenaries-Recompiled is prior art** (2026-09-30): a playable Windows port of another Xbox title
  on an early fork of the same toolkit. Its lifts are planned in §7a; its comparison also exposed
  four defects in our toolkit (ledger D1–D4).

## CURRENT PACKET — none

No packet is promoted. **The fail-fast observer attempt is PARKED by the owner — STOP UNKNOWN, no
retry, no further correction, consultation or list extension.** It stopped before the final corrective
build: the initial real-library link succeeded but 0/12 controls passed, and the final source audit
found an elapsed-threshold mismatch and an unmapped assertion against the closed correction list. No
passing controls, no new guest run, no observer/causal classification. See
[F5 fail-fast observer stop](docs/reviews/rulings/f5-failfast-observer-stop.md) and historical
[F5 pre-freeze stop](docs/reviews/rulings/f5-fade-observation-stop.md). The chores now running are the
owner-directed records cleanup and exploratory runs in §13's current-state block, **not** a resume of
the broader goal, which stays paused.
Phase 0 (§4) and the chores of §5–§6 run as owner-directed chores, which
need no packet. The first packet is C1 (§7); it is promoted here, by exact revision and hash, only
after its adequacy review returns `ADEQUATE` (`docs/agent-workflow.md` §5).

## 1. Objective and definition of done

Port JSRF to Windows by static recompilation. **Minimum playable slice** — all of the following,
under **any** run profile, provided every path the result relies on that is not *emulated* or
*translated* is recorded in `docs/jsrf-compatibility-ledger.md` and the run's record lists those
ledger IDs:

| DoD | Criterion | Measured by |
|---|---|---|
| DoD-BOOT | Launch to the title screen; the title frame is presented; every shortcut used is in the ledger. | M15 |
| DoD-INPUT | A host controller drives menu navigation through the guest's own XAPI input path. | M16–M17 |
| DoD-PLAY | New game loads the opening area; the player skates, turns, jumps and the camera follows. | M18–M21 |
| DoD-GRAFFITI | One graffiti interaction completes and the game's own progression state records it. | M22 |
| DoD-AUDIO | Sound effects and music are audible at the correct pitch. | M23–M24 |
| DoD-SAVE | Save, exit, restart and resume to the saved location/progression; a missing save is handled. | M25 |
| DoD-STABLE | A 15-minute soak with no invalid indirect call, no ABI violation and bounded memory. | M26 |

A window opening is not the slice; one playable scene is not the game.

## 2. Baseline

| Item | State |
|---|---|
| Toolkit | `main` (fork fixes on top of upstream `ea60cfa`), built and tested on Windows in Phase 0 (V1) |
| Game | `master`; Phase 0 V1–V4 done (TR §5 "Phase 0 V3 measurements") |
| Current horizon | ~5–7 s: the kernel thunk table `0x1C3F60..0x1C413F` is overwritten by a 40-byte-stride record array, and whichever thread next calls through a thunk faults; the first site varies run to run (TR §5) |
| Established | AC'97 codec-ready model (TR §3); GP DSP56300 port and the GP clearing the DSP pending word (TR §4); CRT 64-bit divide helpers (TR §2); the XDK D3D vblank fields `+0x242C`/`+0x2430`/`+0x2434` and five function names (V4, TR §7) |
| Refuted | that `sub_00038530` writes the vblank-callback slot (V4, TR §7) |
| Prior art | Mercenaries-Recompiled (`https://github.com/KraftMacAndChee/Mercenaries-Recompiled`; playable; toolkit base upstream `25cf8a6`, a sibling fork — lift ideas and patches, never merge); halo-ce-universal (`https://github.com/cybersecurity/halo-ce-universal`) (XDK facts); the toolkit forks (TR §7) |

---

## 3. How work runs under this plan

- **DeepSeek workers execute by default** (unlimited); the Session directs, integrates and
  records (`docs/agent-workflow.md` §2.2 "worker-first"). Planner and Reviewer calls are
  made at the gates a task names; the Advisor is consulted at those gates **and** on every
  `docs/agent-workflow.md` §4.2 trigger. Each task states a **senior-call budget**; exceeding
  it stops the task for an Advisor continue/stop decision. Quick consults (§4.3) do not count
  against it.
- **Three task classes.** *Chore* — owner-directed mechanical work (builds, syncs, regeneration,
  tooling, record fixes): no packet; recorded in the TR with commands and results; may not change
  admitted evidence semantics except behind a switch classified in `docs/jsrf-run-profiles.md`
  (pending W7, chores run as owner-directed changes, as the syncs and regeneration already did).
  *Discovery* and *change* — packets under `docs/agent-workflow.md` §5.
- **Least resistance, recorded.** For each blocker, take the cheapest honest class: if emulating it
  would take more than about a day, approximate, stub or patch it, add the ledger entry in the same
  commit, and move on. Upgrade a path only when it blocks something.
- **Acceptance criteria** name: profile · artifact path · oracle (independent of the
  implementation) · PASS predicate · FAIL/UNKNOWN predicate; for bare-minimum milestones the profile
  may be exploratory, and the record lists its ledger IDs. The rows in this plan give the profile,
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

**V1 — Build and test `main`/`master`.**
- Do: pull toolkit `main` and game `master` (toolkit first); `python -X utf8
  scripts\build-jsrf.py`; `ctest` in the game build and in a standalone toolkit build.
- PASS: build exit 0; every ctest passes, including the toolkit's `xbox_kmem`, `xbox_guest_meter`,
  `nv2a_actions`, and the standalone `tests/kernel_data_exports` and `tests/kernel_file_status`
  projects; test counts recorded. FAIL: any build or test failure → fix before V2 (a toolkit
  failure is fixed on `main`; its commit cited).

**V2 — Regenerate with the new lifter.**
- Do: the TR §2 command, unchanged inputs; if MSVC runs out of memory, `--split 250` (upstream's
  recommendation for 15 GB hosts). Re-apply `relift-selected.py boundaries`, the ABI deltas, the
  A4b2 hooks; re-record provenance (`check-generation-provenance.py --write`).
- PASS: build + ctest pass; the generator's `FLAGS:` report names exactly the 10 known leftover
  `_flags` reads — 5 `state: none` (4 that read flags live into the function, plus the `loope` at
  `sub_0010634E`) and 5 `adc cannot answer` — and **no site outside the pre-regeneration set**, checked
  by diffing the listed sites against `HEAD`'s `gen/`; the 8 sites `ca4257c` called live bugs read no
  fallback; function count and dispatch count recorded against 5740 / 8928; provenance `--check` ok.
  FAIL: any listed `FLAGS:` site that is not in that set, or any new `[UNIMPL]` reached in V3.
- *Corrected 2026-09-29 (Advisor ruling, Phase 0 V2).* The criterion previously read "≤ 9", taken from
  toolkit `ca4257c`'s census. That census counted **jcc-form sites only** (17 sites, 8 fixed, 9 left),
  while the generator's `flag_gaps()` also counts LOOPE/LOOPNE, SETcc and CMOVcc preloads. The two
  numbers were never comparable, so "≤ 9" was unsatisfiable-by-construction rather than a defect: the
  tenth site is a `loope` byte-identical to the pre-regeneration tree. The criterion is now tied to
  **named sites**, not to a count taken under a different scope.

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
- **DONE 2026-09-30.** 3074 entries: **122 OBSOLETE** (58 by the default pass, 64 by
  `--coalesce-functions`), **2952 STILL_NEEDED**, **0 UNKNOWN**; the verdict set is exactly the config
  entry set. Controls 0 failures. Switch-arm entries and the returning-body probe retire nothing.
  Full table in `logs/v5-recovered-audit.md`; recorded in `docs/jsrf-technical-record.md` §5. No entry
  was retired.

## 5. Phase 1 — tooling (chores; DeepSeek; senior calls only where named)

| ID | Tool | PASS (each with a control) |
|---|---|---|
| **T1** | **WinDbg TTD**: `just ttd-record <label>` records a strict run; `tools/ttd/writes.js` (dx query) lists every write to a guest VA **across all 29 aliases** (base + 28 mirrors) with thread, native IP, symbol, position | on one trace, the query finds the runtime's own thunk-install write to `0x001C4064` (known positive), and zero writes to an address never written (known negative) — **controls PASS 2026-09-30** (`20260930-030513-184-c1-probe`); **but see the W11 finding: the traced run does not reach the horizon, so the artifact is not admitted and T1 is REOPENED for the conditions in `docs/reviews/rulings/ttd-query-decision-input.md` S1–S8** |
| **T2** | **XbSymbolDatabase** (MIT, external CLI) → `config/xdk-symbols.json`; names merged into `inspect-jsrf.py` output and linker-map symbolization | ≥ 300 names (DanielJVoxSmart measured 363 on this XBE); five spot checks against known functions (`__aulldiv` `0x0017D4D0`, etc.) agree |
| **T3** | **xemu oracle** (requires the owner's BIOS, MCPX ROM, HDD image): gdbstub recipe that dumps guest memory/registers at a named guest PC; `scripts/xemu-diff.py` (planned) compares with the same checkpoint in a recomp run | the diff of a checkpoint against itself is empty; a seeded one-byte change is found. If the images are unavailable: BLOCKED (owner assets) and milestones fall back to non-xemu oracles |
| **T4** | **Windows CI** for the toolkit fork (GitHub Actions `windows-latest`, MSVC, ctest) | green on `main`; a deliberately failing test turns it red |
| **T5** | **clang-cl + MSVC `/analyze`** configurations of the toolkit | baseline warning counts recorded; the known `%lld`-with-`int` class and implicit declarations are reported by at least one of them |
| **T6** | **`just`** recipes: `build`, `test`, `regen`, `strict-run`, `explore-run`, `check`, `ttd-record`, `doctor`, `analyze` | every recipe runs on the Windows host; AGENTS.md "Build and run" points at them; `check-agent-docs.py` verifies the named recipes exist |
| **T7** | **pre-commit** hooks: `check-agent-docs.py --check`, `secret-audit.py` on staged blobs, no `game/` path, run-profile tests, `check-merge-structure.py` when a merge is in progress | a staged `game/` path and a planted fake token are both refused |
| **T8** | **DuckDB log queries**: `scripts/logq.py` (planned) loads kernel/`[KMEM]`/`[ALIAS-ICALL]`/`[GMETER]`/`[ICALL]` lines into tables; saved queries under `tools/queries/` | reproduces a known count from an archived run (e.g. the 15,498 sampled bridge boundaries of A2h-null-slot-triage) |
| **T9** | **Enumerator**: `scripts/enumerate-accesses.py` (planned) — operands normalised to `uint32`, recursive-descent from the entry and dispatch seeds, raw-byte fallback, each run printing its own known-answer controls | reproduces `PIO_FREE` = 28 sites (10 hex + 18 decimal spellings) and the vtable base = 3 references (TR §6) |
| **T10** | **Citation tool + lint**: `scripts/cite.py` (planned) records value, artifact, command, hash; the memory reader prints both byte orders and an offset-shift control; a lint rejects hex literals in records that no cited output contains | the lint flags a seeded transposition (`0x00193D62` for `0x00193D96`, the recorded historical error) |
| **T11** | **Review capture**: `record-review.py` defaults to the current reviewer route (GPT-6 Sol child) instead of the retired Hy4 route | records a Sol child's review from its session log with its hash; the existing DSH tests still pass |
| **T12** | **Doctor per run**: `tools/doctor.py --runtime-log` writes `doctor.json` into every archived run | present in the V3 runs |
| **T13** | **Startup receipt generator**: fills `docs/session-start-template.md` from harness metadata and live route checks | a generated receipt passes `check-agent-docs.py` |
| **T14** | **Run-log retention**: `scripts/logs-reclaim-plan.py` + `scripts/disk-usage.py` as a pre-run gate (refuse to launch below a free-space floor) and a retention policy: keep every run cited by the TR, the plan, a packet or a ruling, plus the last 20; archive or delete the rest **after owner approval of the policy** (deletion is an owner decision) | a launch below the floor is refused with a clear message; the policy lists how many runs/GB it would reclaim on the current host before anything is removed |

| **T15** | **Function-level parity audit and overlay bisection** (from Mercenaries `audit_generated_function_parity.py` + `make_generated_overlay.py`, MIT): hash each generated body by guest address across two trees; restore baseline bodies in an address range to bisect "which function moved the stop" | finds a seeded one-function change between two trees; an overlay that restores it moves the stop back |
| **T16** | **`just regen` matches the recorded regeneration**: pass `--trace-functions config/trace-functions.json` (TR §2 has it, the recipe drops it) and reviewed spans through the toolkit's `--coalesce-functions` | a regeneration reproduces TR §2's inputs; the 7 boundary fixes appear without the post-hoc relift |
| **T17** | **Content-sniffing asset guard** in pre-commit: refuse `XBEH` headers, Xbox volume magic, nested archives, and JSRF's `.text` control bytes `8b512c85…` (copied guest RAM) | a staged file carrying each signature is refused |
| **T18** | **Post-generation patch script** (Mercenaries `Patch-Generated.py` design, rewritten): exact-once text patches applied after every regeneration, each with a ledger ID, failing loudly when a site is missing (no `--allow-missing`) | a regeneration + patch run is idempotent; a missing site fails the run |
| **T19** | **Retail-byte function oracles**: run original x86 (capstone) against the lifted body with poisoned registers and stack-balance checks, extending `generate-lifter-tests.py` | catches a seeded callee-saved-register clobber |

Order: T14's pre-run gate, T6, T7, T4 first (they make every later step mechanical and stop the disk
failure mode recurring), then T1, T2, T8, T9, T10, T11, T12, T13, T5; T3 whenever the owner
provides the images. Senior calls: T1's admission ruling (W11) only.

### Phase 1 status — 2026-09-30 session

**T14, T6, T7, T4, T1, T2, T8, T9, T10, T11, T12, T13, T5 are DONE; T3 is DONE** (re-verified
against a live xemu running the title, with the four criteria at line 190 all passing —
see `docs/reviews/t3-gdbstub-register-layout.md`). **T3's gdbstub had a defect found during
that re-run and fixed:** it decoded the register block as x86-64 where the guest is 32-bit,
so every register value it had produced was wrong while every memory read was right. The
verification that has teeth is a cross-check against the original XBE: the live `eip` holds
bytes identical to `inspect-jsrf.py data` at that address. Each landed with
its controls exercised, and the defects those controls found are recorded in the commits and in the
tools themselves. Measured highlights, all reproducible from the named command:

| Task | Evidence |
|---|---|
| T1 | `20260930-030513-184-c1-probe`: 29 aliases queried, install positive FOUND (`0xFE000104` to slot 65), mirror negative PASS, trace-live PASS. **Reopened and re-closed** against W11's S1–S8 — see §7 C1. |
| T2 | 363 symbols (D3D8 163, DSOUND 137, XAPILIB 59, XGRAPHC 4); `XInputOpen` and `D3DDevice_SetRenderState_Simple` agree with the independent record. **`__aulldiv` is OUT_OF_SCOPE**, not failing: XbSymbolDatabase ships no CRT library. |
| T3 | xemu's own config resolves all five assets; gdbstub reachable; guest `0x00011000` reads the recorded `.text` control byte-for-byte; `xemu-diff` MATCH against an archived recomp run; self-vs-self empty and a seeded byte found. |
| T4 | `main` green; a deliberately failing test turned the **CTest** step red on a throwaway branch, which was then deleted. |
| T5 | clang-cl 6358 diagnostics, MSVC `/analyze` 355; the `%lld`-with-`int` class reported by **both**; `implicit_declaration` reported by neither, with a seeded control proving the detector works and `/we4013` explaining the absence. |
| T6 | 22 recipes; `check-agent-docs.py` fails on a missing recipe. |
| T7 | A staged `game/` path, a planted token and a conflicted merge are all refused, each with a known-good case. |
| T8 | 6488 kernel calls and 344 recovered returns match independent grep counts exactly. |
| T9 | `PIO_FREE` = 28 sites (10 A1-moffs + 18 ModRM), vtable base = 3; both controls PASS. |
| T10 | Seeded transposition `0x00193D62` flagged, known-good control clean. |
| T11 | Default is `codex/gpt-6.1-sol` @ `high`, read back from the §1 roster by a control. |
| T12 | `doctor.json` written per run; MSBuild's duplicate-case hazard and disk headroom reported. |
| T13 | Generated receipt; regeneration preserves probe results. |
| T14 | A launch below the floor is refused before any child starts; the retention plan is citation-aware and deletes nothing. |

**Mac session, 2026-09-30 (no game assets; nothing here has run on Windows).**
- **T15 DONE** as `scripts/generated-parity.py` (`audit`, `overlay`; MIT notice carried): 6 controls,
  including a seeded one-function change, a merged start reported as subsumed, an overlay limited to
  its range, and the real tree (5,739 functions) parsing. Still open: building an overlaid tree and
  watching the stop move back, which needs the Windows host.
- **T16 half done:** `just regen` and AGENTS.md now pass `--trace-functions config/trace-functions.json`,
  matching TR §2 and the provenance checker. The `--coalesce-functions` half needs a Windows
  regeneration to show the 7 boundary fixes appear without the post-hoc relift.
- **T17 DONE** in `scripts/precommit-staged-paths.py`: XBE, FATX, minidump, XDVDFS, archive and
  compressed-stream signatures and the raw `.text` control bytes; 14 refusal cases, a prose
  known-good case, and a check that the gate's own sources pass it.
- **T18 DONE** as `scripts/patch-generated.py` and `config/generated-patches.json` (empty). `just regen`
  applies it and `just check` runs `--check`; 8 controls cover exact-once application, a second run
  changing nothing, function scope, a missing or ambiguous site, and an unknown ledger ID. **Candidate
  patches** (need a fresh regeneration to capture their exact text): the
  `jsrf_watch_store` declaration every chunk header says to re-apply, and the exact-delta ABI
  additions to `recomp_types.h` (TR §2).
- **T19** not started (Windows; needs the XBE).
- The pre-commit and AC2 tests now turn commit signing off in their scratch repositories: a global
  `commit.gpgsign` hung them on a signing prompt.

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
| **W11** | Advisor ruling: **TTD query output as a decision input** (lossless, reproducible) recorded in `docs/jsrf-run-profiles.md` | enables C1 without new watch instruments | the ruling text in the owning document — **DONE 2026-09-30**: TTD traces admitted as a lossless *class* under conditions S1–S8; the current artifact **NOT admitted** (the traced run exits `0xC0000409` at 577 log lines and never reaches the horizon, so W-a cannot hold). Recorded in `docs/jsrf-run-profiles.md` §"TTD trace query (W11)" and `docs/reviews/rulings/ttd-query-decision-input.md` |
| **W12** | Session "verification" prose is replaced by script JSON; review records keep rulings and packets, which are the only records later work reused | 31 of 36 session verification records never cited (38,972 words) | record lint |
| **W13** | Per-task **senior-call budget** (§3), recorded in the packet; exceeding it is an Advisor continue/stop decision | change packets consumed 15–27 senior calls; discovery 2–4 | packet template field |
| **W14** | **Strict-horizon ledger and ceiling rule** (§3): one line per session; 3 packets or 4 h without moving the horizon or an accepted critical-path finding → one Advisor ceiling call | the NULL-line ceiling came only after 5 packets; 287 commits after the last horizon move | ledger lint: a session with a run and no ledger line fails |
| **W15** | **Single-source, drift-checked text**: override descriptions in `AGENTS.md`/run profiles derived from `scripts/jsrf_run_profile.py`; `check-agent-docs.py` fails on override names the runtime no longer reads and on retired routes (add kimi-k3, claude-opus-5-5, hy4-preview-f to `RETIRED_NAMES`, updating the test fixtures) | `AGENTS.md:92` and `docs/jsrf-run-profiles.md:51,54` still describe `RECOMP_AC97_READY`/`RECOMP_APU_DSP_ACK` as live (removed in toolkit `c97ce2c`/`8f8f6e4`); stale `RETIRED_NAMES` | checker control: a planted stale name fails |
| **W16** | **Stage explicit paths only**; the Session never runs `git add -A` while another role may be writing; hashes are pinned only after the writer reports done | `git add -A` swept a Planner's in-progress draft twice (`141cb7e`); a hash pinned mid-write (`d82f388`); a lost ACCEPT record (`a90b8ab`) | pre-commit warns on staged draft packets not named in the commit message |

Senior calls: W11 is one Advisor ruling; the rest are owner-approved document edits (0).

### Phase 2 status — 2026-09-30 session

**W1, W2, W11, W14, W15, W16 are DONE, each with its named check and controls.**
**W8 remains OWNER-RESERVED**: it requires the owner to name one fallback route per senior role in
§1, and no fallback model/provider may be invented (the generic receipt machinery that W8 also asks
for is done, as T13). The remaining W rows are document edits whose checks now exist.

| Row | Check that now exists | What it found on the real tree |
|---|---|---|
| W1 | `scripts/check-review-recurrence.py` + 13 controls | 0 packets to examine (no revision records on disk); controls are fixtures and say so |
| W2 | `scripts/qualify-premise.py` + 18 controls, `just qualify` | on the real C1 premise: 5 PASS, 1 UNKNOWN, verdict `INCOMPLETE` |
| W3/W10 | `scripts/check-packet-transcript.py` + 12 controls, `just packet-check` | 6 closed packets exempt by name; no blocking findings |
| W4/W12 | `scripts/check-record-hygiene.py` + 21 controls, `just record-check` | 3 real instances, then clean after the exemptions were scoped |
| W5 | `scripts/check-transcribed-values.py` + 10 controls, `just transcribed-check` | 4 `rechecked`, 2 `undecidable`, 0 blocking |
| W6 | `scripts/check-ruling-ledger.py` + 11 controls, `just ruling-check` | the W11 ruling passes its own lint |
| W7 | `scripts/chore-gate.py` + 12 controls, `just chore`; §5.8's three-class table | `STOP_REMOVED` on the two real runs |
| W8 | `scripts/check-route-allowlist.py` + 8 controls, `just route-check` | both roster routes are in the allow-list; fallbacks owner-reserved |
| W9 | `scripts/check-instrument-controls.py` + 14 controls, `just instrument-check` | no instrumented runs recorded |
| W11 | `docs/jsrf-run-profiles.md` §"TTD trace query (W11)"; ruling ledger; T1 evaluates S1–S8 mechanically and exits nonzero | the delivered artifact is `NOT ADMITTED`, with the two reasons the Advisor named |
| W13 | packet template's `**Senior-call budget:**` field, checked by W3/W10's lint | template carries the field |
| W14 | `scripts/check-horizon-ledger.py` + 9 controls, `just horizon-check`, wired into `just check` | found 4 runs cited only by suffix; the ledger now names them fully |
| W15 | `scripts/check-override-drift.py` + 9 controls, `just override-check` | found the exact `AGENTS.md:92` stale text the plan names; fixed |
| W16 | `scripts/precommit-repo-checks.py` draft-packet warning + 6 controls | no draft staged |

`just check` runs ten checkers; each has its own recipe and each recipe is named in
`check-agent-docs.py`'s `REQUIRED_RECIPES`, so a rename fails the check rather than
silently breaking a record that cites it.
| W16 | `scripts/precommit-repo-checks.py` draft-packet warning + 6 controls |

---

## 7. Phase 3 — critical path to boot (after V3)

**C1 — Attribute the record-array write (discovery).** Replaces the specified A2h successor.
**Pragmatic order (2026-09-30):** fast-path step F1 (a `[READ] dst=` check on an ordinary run) goes
first; the TTD recording below runs only if F1 finds no file read landing on the table.
**INSTRUMENT: TTD, RULED 2026-09-30.** The Advisor's §2.3 ruling is in
`docs/reviews/rulings/ttd-query-decision-input.md` ("C1 instrument (2026-09-30)"):
**W11 stands unchanged and TTD remains C1's instrument.** The Session's contrary
conclusion — that TTD cannot see the write — rested on a trace that **fails S1 because
it was truncated at its size cap** (verified: the `.run` is exactly 8192 MB, and
`ttd-output.txt` says "Recording stopped after 43375ms" where a completed recording says
"Process exited with exit code …"). The process outlived the recorder, so its log carries
the terminal that the trace does not. **That conclusion is withdrawn**, and the record
says so.

**C1 must meet C-a…C-e before any row may be selected:**

- **C-a (S1).** The recorder output shows *"Process exited with exit code 0xE0424943"*
  and the trace is **below** `-maxFile`. *"Recording stopped"*, or a size at or above the
  cap, is a FAIL, and nothing after the install may then be read from the trace. Size the
  cap from the T14 disk gate.
- **C-b (S2).** W-a is established **from the trace itself**: the trace contains TTD's
  exception event with code `0xE0424943` on the faulting thread, and **P is that event's
  position**, not `!tt 100`. A log-only terminal is a FAIL. `ttd-query.py` now requires
  `--terminal-in-trace` explicitly.
- **C-c.** **W-c is still MISSING and is not waived.** A mirror positive needs a store
  actually made **through a mirror VA**, found at its alias index. "All 28 mirrors of a
  canonically written range return 0" is a negative, and a broken query returns 0 too.
- **C-d.** **W-d is still MISSING.** It needs the destination VA of one `[READ]` and a
  query at that buffer. The toolkit now logs `dst=` (`xboxrecomp` `1572256`), which is
  what made the control possible.
- **C-e.** The census and "last write before P" are **recomputed on an admitted trace**,
  and W-b decides: equal means `ATTRIBUTED`; mismatch means `UNATTRIBUTED`, which then
  triggers an **in-process last-write latch keyed by destination region** — not the A2h
  alias census, which is a *first-touch* census and so selects the earliest writer where
  C1 needs the last.

The experiments already run are recorded in
`docs/reviews/c1-terminal-slot-finding.md`; its later sections are **NOT ADMITTED** and
carry a withdrawal block. The non-TTD minidump A/B survives as corroboration: the
original XBE holds `0x80000NNN` ordinals at `0x001C3F60` while the runtime holds the
40-byte-stride record array whose constants match TR §5 exactly, and slot 65 reads `0`
there — agreeing with the guest's own `[ICALL]`.

**The next action is a TTD recording that satisfies C-a**, then C-b…C-e in order.

Measured this session, and still valid because it does not depend on the truncated
trace:

- **The horizon IS reachable under recording.** The earlier "TTD changes the guest's
  behaviour" reading was wrong: `RECOMP_APU_TRAP=1` is the difference, and with it a
  traced run reaches the horizon at 23,484 log lines, same site (`return=0014982E`,
  slot 65, `exit_code=0xE0424943`). `just ttd-record` now sets that environment.
- **The clobber is confirmed from a NON-TTD source.** The original XBE at `0x001C3F60`
  holds `800000BB 800000BE …` (ordinals); the non-TTD minidump of the
  horizon-reaching run holds the 40-byte-stride **record array** whose constants match
  TR §5 exactly. Slot 65 reads `0x00000000` there, agreeing with the guest's own
  `[ICALL] invalid target 0x00000000`.
- **The complete write census of the table** (1080 writes): 480 byte-at-a-time fills
  from `VCRUNTIME140!memset_repstos` at position 4005, then 120 dword installs from
  `jsrf_recomp` ending at 483399. **Nothing writes the table after the install.**
- **TTD CANNOT SEE THE WRITE CLASS THAT MOST PLAUSIBLY PRODUCED THE `0`.** 400,000
  writes scanned across the low 4 GB, **zero** outside `jsrf_recomp.exe` and
  `VCRUNTIME140` — so kernel-mode writes are invisible. W11's exclusion (d) is
  **confirmed rather than suspected**, and the trace's log shows 14 `[READ]` lines
  where `NtReadFile` delivered into guest buffers with none of those bytes appearing
  as a write.
- **Withdrawn by the ruling.** The claims that "W-c is satisfied", "W-d is answered",
  "H-ALIAS is excluded" and "TTD cannot see the write" all rested on the cap-truncated
  trace and are **NOT ADMITTED**. The mirror sweep over a canonically written range is a
  **negative**, and a broken mirror query returns zero too, so it does not supply W-c.

**Why C1 is not ready to promote.** Its experiments ran against a trace that fails S1,
so their results are not admissible. **Promoting it unchanged would violate §5.3's
premise-freshness rule.** The instrument is ruled (TTD), and the packet is ready to
promote once its `### Experiments` are re-run against a trace meeting C-a and C-b.

- Question: **which code writes the record array** (not "which writes slot 65" — the
  census shows the table is written wholesale, so the unit is the array).
- Experiment: see the packet's `### Experiments`, and re-run steps 2-4 against a trace
  satisfying **C-a** (process-exit end, below the cap).
- Acceptance: the query artifact, its positive control (the install write) present,
  and the row selected by rule — **and, per W11, witnesses W-a (terminal-in-trace) and
  W-b (value consistency), with `O-UNKNOWN` selectable only under W-b equality**.
  Senior budget: 1 Planner (self-review), 1 acceptance review.

**C2 — Kernel memory follow-ups (change, only if V3 or C1 implicates them):**
`NtQueryVirtualMemory` consulting the region registry; partial `MEM_RELEASE`; whether to keep the
reserve clamp; heap blocks untracked after 65,536 entries; KeSystemTime/KeInterruptTime advancing
(a new autonomous-clock model → admission under the run-profile rules).
Accepted when: each implicated item has a `kmem_test` case that fails before and passes after, and a
strict run shows the implicating symptom gone (`[KMEM] summary` counter or stop site named in the
packet).

**C3 — NV2A action methods — PARKED (2026-09-30).** The fast path renders through the executor
(F4), which does not need them; revisit after the slice. Evidence: toolkit
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

**C5 — Rendering architecture — DECIDED for the bare minimum (2026-09-30): the executor path.**
`RECOMP_GPU_ACK` stays on (ledger L16) and `RECOMP_PB_EXEC` renders (L18); the strict-model back
end below is post-slice work. Original design note: strict runs capture
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
*Status 2026-09-30:* the declarations are in (`src/main.c`, `src/diagnostics.c`,
`tools/harness/collect.c`, `tests/test_recovery_11c1.c`); a MinGW cross-compile with
`-Wimplicit-function-declaration` finds none left in the 22 hand-written C files (three stop early on
MSVC-only constructs). Remaining, on Windows: `/we4013` and the MSVC build and ctest.

**C7 — A4b2 P4 transfer bridge (carried):** re-establish only when a packet inherits P4.

**C8 — Upstream and fork cadence (chore):** on each upstream release, a merge chore with
`check-merge-structure.py` and the run-profile merge rules; every two weeks a DeepSeek fork scan by
content (the TR §7 method) that lists candidate fixes with licence and strict-path effect.
Accepted when: each merge records its hunk inventory and a strict A/B (W7 gate); each scan leaves a
dated list in the TR with a disposition per candidate.

---

## 7a. Lifts from Mercenaries-Recompiled (2026-09-30)

Ideas and code from Mercenaries-Recompiled (`https://github.com/KraftMacAndChee/Mercenaries-Recompiled`; MIT root with no copyright line: add an
attribution header; its `src/apu` and `src/nv2a` are xemu-derived LGPL: keep per-file notices; never
take `tools/recomp/xemu_dsp_oracle/`, which builds xemu's GPL interpreter). Each lift adds its ledger
entry in the same commit. "When" ties it to the fast path (§13).

| ID | Lift | Why for JSRF | Ledger class | Effort | When |
|---|---|---|---|---|---|
| ML1 | **Serialised guest execution** behind a switch: one guest thread in lifted code at a time (a lock at the guest-meter brackets, released across kernel calls), ISRs and DPCs delivered under the lock only when IRQL allows, a yield at loop back-edges | our timer thread runs the GPU ISR and every DPC beside up to 4 guest threads (D4, `[GMETER] max=4`); the prime suspect for the table overwrite | approximated | moderate (runtime + one translator hook, regenerate) | F1 fallback (b) |
| ML2 | **Replacement XAPI heap**: `RtlAllocateHeap`/`RtlFreeHeap`/`RtlReAllocateHeap`/`_msize` on a host-tracked guest arena (their `recomp_manual.c:15333-16060`) | JSRF's `sub_001497DC` is the same routine they replaced after the lifted heap rejected valid allocations | reimplemented | moderate; needs JSRF's heap addresses (T2 names) | F3 |
| ML3 | **File I/O**: Xbox no-buffering as a caching hint (we pass `FILE_FLAG_NO_BUFFERING`, which demands sector-aligned I/O), their streaming read cache, `GENERIC_ALL` → data access | asset reads (M08) and saves (M25) | wrapped | small | F2b, before asset loading |
| ML4 | **APU interrupts and timing**: IRQ delivery with vector `0x30+n` / IRQL `27−n`, `timeBeginPeriod(1)`, the 5.1 fold, voice-processor DMA through the physical model (D2) | we raise no APU interrupt; coarse timers slow audio clocks; D2 can write the XBE image | emulated / approximated | small–moderate | F3 as needed; before M23 |
| ML5 | **Vblank fallback**: on each host vblank, signal the device's vblank event (`+0x2430`) and advance its counter | only if `BlockUntilVerticalBlank` hangs; fields confirmed by V4 | approximated | small | F3 fallback |
| ML6 | **Executor-path shortcuts**: GET=PUT when the walk errors or runs out of budget, acknowledge software-method NOPs, a non-holding `FLIP_STALL` | only if a GPU stall appears on the executor path | approximated | small each | F4 fallback |
| ML7 | **D3D11 renderer transplant** (~24k lines: combiners and vertex programs → HLSL, formats/swizzle, surface cache, AA/stencil, flip-ordered presentation, shader cache) | our executor renders on the CPU and passes no combiner state, so JSRF's cel shading cannot appear | translated | large; feasibility study first | after F4, when the title screen or M19–M20 needs it |
| ML8 | **ISO extraction and build-from-ISO** (xdvdfs, hash-checked) and later the first-launch launcher | new-machine setup; M36 packaging | wrapped | small / moderate | any time; M36 |
| ML9 | **Input and options**: keyboard/mouse bindings with prompts, SDL/XInput, stick outer-rim calibration, resolution/aspect/FPS-cap options, F8 log marker | M16 onward and quality of life | wrapped / approximated | moderate | after M15 |
| ML10 | **Unified physical memory** (`0x80000000+` aliases the same RAM, as on hardware) | removes the D2 class of device-DMA bugs; an architecture change | emulated | large | only if more D2-type bugs appear |

Not lifted: their DirectSound mailbox patch (our GP DSP already clears that wait), XMV playback (JSRF
uses Sofdec), Lua/mission/UI/bird/PS2 title fixes (W21–W42).

**Status 2026-09-30 (Mac session; cross-built and unit-tested, not run on the title):**
- **ML1 DONE, opt-in:** toolkit `179439b`, `RECOMP_GUEST_SERIAL=1` (ledger L34, exploratory by
  presence). The meter test's serial cases pass natively. The loop back-edge yield is in the
  translator (`86113c7`, `--backedge-yield`, which `just regen` now passes); it takes effect at the
  next full regeneration, on Windows. Until then a guest spin loop costs one bounded wait.
- **ML3 DONE:** toolkit `e43e9bf` (ledger L30, L31).
- **ML4 mostly done:** `timeBeginPeriod(1)` (`55acf60`, L33), the 5.1 fold (`29f13d0`, L25) and
  voice-processor DMA through the GP's translation (`1c6641a`, D2 fixed, L22). APU interrupt
  delivery (vector `0x30+n`) is still open.
- **Later the same day:** `KeRaiseIrqlToSynchLevel` is tracked and `KeGetCurrentIrql` reports the
  tracked level (`92715dc`, L36), so the IRQL gates see every raise; D3 is fixed (`9fd83c6`,
  NV097 method state moved out of the PGRAPH register array); and `tools/posix_check.py`
  (`c58ed2f`) runs the portable tests natively, cross-builds for Windows and runs the toolkit's
  pytest in one command. Its first run found two stale tests, both fixed: the guest-meter audit
  did not know serial mode's bracket (`104e9d8`), and `tests/apu_mixdown` crashed on every host
  since the GP DSP port (`5d3466b`).
- **Also landed:** DPC queue semantics (`e2872a1`, L35: at-most-once insert, `KeRemoveQueueDpc`
  cancels, locked queue), `rep movsb`'s element-wise path through volatile `MEM8` (`5364747`), and
  the blind-spot tools (`7b6839a`): the `NtReadFile` bounce buffer (L29) and the read-only tripwire
  `RECOMP_RDATA_GUARD=1` (L32).

## 8. Milestone ladder — bare-minimum slice (07–26)

Rows 00–05 are done; 06a done; 06b ("implement reached imported kernel semantics") is closed into
the fork fixes and re-opens only on a measured unbridged call. **Any profile is acceptable**; each
acceptance record lists the ledger IDs its run relied on. SSIM and correlation thresholds are
guidance for judging the frame or sound, not a strict gate.
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
| Game implicit declarations | → C6 (declarations in; `/we4013` on Windows) |
| Kernel memory open points | → C2 |
| DSP provenance record (A4b2-NR instrumentation not listed) | kept; chore |
| Review capture (`record-review.py` default) | → T11 |
| (new) Stale override text in `AGENTS.md`/run profiles; stale `RETIRED_NAMES` | → W15 |
| (new) Run logs filling the disk (167 GB across 1,172 run directories on 09-28, `6a97c86`) | → T14; the owner cleared the old runs on 2026-09-30 |
| (new) W2 gate under pragmatism: `scripts/qualify-premise.py` item 2 requires strict premise runs, and item 5 should name the prior-art set (Mercenaries-Recompiled, halo-ce-universal, toolkit forks) | **DONE 2026-09-30:** `--purpose bare-minimum` accepts an exploratory run with `--ledger-id`s the ledger has (`fidelity`, the default, stays strict); item 5 searches `PRIOR_ART` checked out beside the repository, and a missing checkout is UNKNOWN; 7 new controls |
| (new) Horizon-ledger scope in the bare invocation | **DONE 2026-09-30:** `check-horizon-ledger.py` defaults to `--since 2026-09-29` (`--since all` for the archive) and prints the scope |
| (new) **Owner decision:** the public game repository tracks 38 MB of lifted game code (`src/recomp/gen/`, `recovered.c`) and has no LICENSE file; Mercenaries keeps generated code out of its public source | **DECIDED 2026-09-30:** the lifted code is untracked (`52dc97b`) after its hand edits became patches (`8331a6d`); removing it from history is planned in `docs/reviews/lifted-code-history-scrub.md` and waits for the owner's go-ahead to force-push. The licence question is still open |

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
- Bare-minimum milestones accept exploratory runs whose records list their ledger IDs; a claim about
  *fidelity* (the port behaves as the hardware does) still needs a strict run, and exploratory or
  fixture evidence stays bounded to what it measured.
- A failed measurement cannot be converted to PASS by any role; a post-review change reopens the
  affected criterion.
- Original assets and existing saves remain unchanged.
- Pending W11: TTD query output as a lossless decision input. Pending W5/T10: tool-cited values only.

## 13. Next action

### Current state (owner, 2026-10-01; read this block first)

1. **Observer parked, no retry** — the fail-fast observer is STOP UNKNOWN by owner decision; no retry, correction, consultation, list extension or further fixture work.
2. **D2 accepted** — the directory-context cleanup closed as a bounded unit and stays accepted.
3. **SEGA after the marker is UNCLASSIFIED** — the eligible W14 event was the unnumbered `JSRF_CACHE_COMPLETE.CMP`; the visible frame is still the SEGA logo; no title claim.
4. **Checkpoint 2 observed:** `20261002-014152-190-owner-d2-600`, 603.03 s deadline; 59 timed window captures identical SEGA, about 401 s after CMP marker; fresh root, no strict-horizon move. [Run record](docs/reviews/owner-sega-600-observations.md).
5. **Checkpoint 3 only if still SEGA:** same 600 s run without `RECOMP_APU_TRAP`; record changed ledger dependencies, then stop and report.
6. **Owner waiver:** W14 bounds do not block these runs; no new fixtures, observer harnesses, bounded units or history scrub; at most one quick Advisor consult per step, ambiguity recorded without further investigation.
7. **Checkpoint 1 first:** ruling metadata/check gates and plan cleanup, `just check` green, commit/push; subsequent run records include ledger IDs and horizon-ledger rows; toolkit pushed first each checkpoint.

### Owner checkpoint 1 — records and gates (2026-10-02)

Toolkit fast-forwarded to `348a0e38fdf3fd940d6e5f79ad24cf25fbcf2c41`: only Mac runner tooling, no runtime change. Eight ruling-lint findings closed using the rulings' own content and one quick metadata Advisor consult (`4ec43a66-99c2-4d6b-b981-5a558f8e6d54`, Claude Opus 5.5 high). Missing technical reversal conditions and the historical fail-fast Advisor attribution remain explicitly absent rather than reconstructed. Observer stays parked; historical process debt is not waived.

The existing ruling checker now runs in `just check` and CTest, without a new fixture/build target. `just ruling-check`, `just check`, dedicated ruling CTest and full CTest **32/32** passed. CMake registration changed the source fingerprint: initial identity verification refused it, so `just build` refreshed identity through the guarded path (no regeneration); production EXE remains SHA-256 `74377ac6130e812b7a830492d4995bdc58efb8899ff5c1d6c23b7ee6c0c0424e`. Plan worker's stale run identity/conditional-step errors were found and corrected by Session before closure.

Run preparation decision: use the existing runner's **fresh empty disposable root**; it has no seed option and refuses nonempty roots. No runner modification or copy-race. The allowed copy was optional; prior D2 root and original assets remain untouched. The 600 seconds therefore includes cache filling; report the measured post-marker interval, not a claimed 600 seconds after marker. This adds no seed shortcut/ledger entry. Run profile and its ledger IDs remain the D2 profile until conditional APU removal.

Staffing correction: Session mistakenly dispatched content review on `codex/gpt-6.1-sol` medium (`49b63b11-7402-48ab-a628-b6852da7fadb`); it is not roster-compliant Reviewer acceptance. Fresh authoritative `claude/claude-opus-5-5` medium Reviewer (`3084f0a2-73d3-4dbf-b449-761555bacc4a`) accepted checkpoint 1 with no blocking defects; independently verified ruling lint, dedicated CTest registration/pass and unchanged executable identity. Parent-held spawn receipt pins route/effort; roster unchanged.

Push receipt is reported at this checkpoint and in the next durable run record; final receipt names the resulting commit rather than a self-referential hash.

### Title-screen fast path (owner direction, 2026-09-30) — supersedes the entries below

**Superseded as current status by the block above; kept as history.** The naming note below applies to
every F-number in this section and in the historical material that follows.

*Naming note: the F-numbers are the names these steps were recorded under. **F5 names two different
things in the history** — the intro movies (its current meaning, below) and, in the superseded
2026-10-01 material, the fade/logo/cache observation work that later took the names **F4b** (the
logo/cache observation) and **F5 fail-fast observer** (the parked attempt). Old text keeps its old F5
wording; read it through this note rather than rewriting it. **F5 is reserved for the intro movies.***

Chores, run by the Session in order; each step adds or updates ledger entries in the same
commit and one strict-horizon-ledger line per session. Stop and report only when a step's fallback
list is exhausted.

- **F0 — Unblock (decided, see below).** Ordinary runs gate at **15 GB** free (TTD recordings still
  need about 50 GB; check with `just disk` first). No reclaim is needed for F1–F6.
- **F0a — Before pulling on Windows, copy `src/recomp/` aside.** Game `52dc97b` untracks the lifted
  code, so the pull deletes it; copy it back afterwards (it is ignored from then on). If it is lost,
  rebuild it as `AGENTS.md` "Rebuilding the lifted code" says, and record what that took.
- **F0b — Rebuild on the new toolkit first.** `just build` then `just test` (new or changed:
  `jsrf_dpc_queue_bridge`, the NV2A contract's PUT cases, the guest-meter serial cases). Toolkit
  `b857665..179439b` changed D1, D2, file I/O, DPCs, the timer period and the mixdown, so the
  first run after this may stop somewhere new; record it in the horizon ledger before chasing it.
- **F1 — Find the table writer the cheap way.** Three single runs, cheapest first; stop at the first
  that names the writer. All may be exploratory (`RECOMP_KERNEL_LOG_BUDGET=100000`).
  - **(a) Reads.** List every `[READ] … dst=0x… got=N` line whose `[dst, dst+got)` intersects the thunk
    table `0x001C3F60..0x001C4140`; if none does, widen to `0x001C2B20`, where index 0 would sit if the
    40-byte-stride record array (index `0x79` at `0x1C3E08`, TR §5) starts at index 0. Reads now go
    through a host bounce buffer (L29), and the line warns when a read lands in a read-only section.
    A hit names the file, offset and caller: fix why that buffer address is wrong.
  - **(b) Tripwire.** `RECOMP_RDATA_GUARD=1` (L32, observation). The first `[RDATA-GUARD] write`
    inside `0x001C3F60..0x001C4140` gives the value, the module-relative RIP (symbolise it against
    the PDB) and the guest return chain. Do not combine with `JSRF_TRACE_A2H_DR`/`_SLOTW`.
  - **(c) Where the array belongs.** In xemu (T3), stop at the same horizon and search guest RAM for
    the record pattern (`0x3E800000`, `0x41200000`, `0xFFFFFFFF`, `1`, `0x001FA1D8` at a 40-byte
    stride). Its address on real hardware says which base pointer the port computes wrongly.
  No answer → `RECOMP_GUEST_SERIAL=1` (L34, D4) and re-run (b); then the C1 TTD recording with
  `ttd -stop`. (The old fallback "(a) VP DMA" is gone: D2 is fixed.)
  - **DONE 2026-09-30, at step (b), and the writer is named.** (a) was answered from the already
    archived strict run `20260930-081601-393-read-dst-verify`: 14 `[READ] dst=` lines, **0** intersecting
    the table or the widened range; no `[READ] WARNING` in either run. (b) then named the writer:
    `rip=exe+0x5361B8` = **`sub_00038530+0x398`**, the slot writer A2h already named, at a different
    site — a `rep movsd` image copy running upward from `0x00011000`. The array is not built at the
    table: it is **copied there from the XBE's own `.data`** (the dump at `V` equals the XBE at
    `V + 0x37608` for 431 of 436 sampled 4 KB pages, 0 original; the dump's bytes at the table VA are
    byte-identical to the XBE's at `0x001FB568`). Recorded as **D5** in the compatibility ledger and in
    `docs/jsrf-technical-record.md` §5. (c) was therefore not needed and was not run.
  - **The fix, and it is F3's first job:** the copy's **destination and length** are wrong, not the
    table. `sub_00038530` computes both. Not yet established: why the copy runs at all, and which of
    its callers is responsible. Cheapest honest class first (ledger Rules); do not patch the table.
  - **SUPERSEDED — the three paragraphs below record F3's intermediate states (cause found, then
    blocked, then fixed). They are kept as history; the authoritative outcome is the F3 DONE block
    below, which follows these historical notes and precedes F4.** The reasoning that follows is what
    the fix rested on, and the "BLOCKED" wording in it describes a state that no longer holds.
  - *(historical)* **F3 CAUSE FOUND 2026-09-30 — it is a wrong tail-jump alias.** The guest called
    `0x00037550`, but the translator classified that address `detection_method: tail_jump_alias` and
    folded it into `sub_00038530`, deleting its body. `0x00037550` is a real function (its own SEH
    prologue, its own bare `ret` at `0x00037603`, its own jump table at `0x00037FB4`) and occurs
    exactly once as a pointer in a function-pointer table at `.data VA 0x001EC108` with **0** direct
    call sites — so the table is its only route, and the fold silently enters the wrong function. The
    generated dispatch's own `[ALIAS-ICALL] target=0x00037550 owner=0x00038530` appears in **both**
    runs, and in the guard run it is 19 log lines before the first `[RDATA-GUARD] write`, on the same
    thread. This is the same defect class `config/recovered-functions.json` already documents for
    `0x000BCF40`, which was fixed by a recovery entry plus a boundary fix.
    **ATTEMPTED 2026-09-30 and BLOCKED on a toolkit limitation — this state is SUPERSEDED; the fix
    landed later the same session (see the F3 DONE block).** The config edit was written and tested,
    then reverted at that point; the tree was left clean and `recovered.c` unchanged. Three measured
    results, all still valid:
    1. **`recover-functions.py` aborted on exactly 2 of 3075 entries**, enumerated by translating every
       entry: `0x001063A0` (`/* TODO: arpl word ptr [eax], dx */` at `0x00106565`, past its `ret` at
       `0x00106560`) and the then-new `0x00037550`. Both were spans that ran past a terminator into
       non-code bytes. Tightening `0x000BCF40`'s end to its real `ret` at `0x000BD8B0` **worked** —
       the abort moved on to `0x001063A0` — so that part of the fix was sound and reusable.
    2. **`0x00037550`'s four TODO markers all sit inside its own 36-entry jump table at
       `0x00037FB4`** (`outsb`, `sti`, `popfd`, `aad`), which the translator decoded as instructions.
       The table's 36 targets are *all* inside the span, so the in-function-goto path applies.
    3. **The toolkit detects the table but the recovery path never uses it.** `_recover_cfg` for this
       address returns **3 jump tables** (41/41/5 targets) and decodes **0** instructions inside the
       table region — the CFG recovery is correct. But `translate_function` reaches its instructions
       through `decode_function`, which consults `self._recovered_cfg`, and that dict is populated
       **only by the coalescence path** (`translator.py:740`). `recover-functions.py` translates a
       reviewed entry directly, so `recovered is None`, the CFG knowledge is discarded, and
       `decode_function` falls back to a linear `disassemble_function` over the whole span — which
       walks into the table.
    **The way through was to end the span at the jump table**, so the linear decode never reaches the
    table's bytes. That needs no toolkit change and is what landed.
  - **Follow-up worth a census:** whether any *other* `tail_jump_alias` fold deletes a
    table-referenced function entry. `0x000BCF40`, `0x00037550` and `0x00026780` are three instances of
    one rule, and the rule's population has never been enumerated.
    **The census was attempted on 2026-09-30 and the instrument was NOT valid.** Scanning the XBE for
    each folded address as a raw 4-byte pattern reported 3501 of 3508 folded addresses as
    "referenced", which cannot be true — a 4-byte pattern matches by chance constantly. Recorded as a
    **failed instrument**, not as a population count. A usable census needs the function-pointer tables
    identified first (the way `0x000BCF40`'s and `0x00037550`'s entries were), then membership tested
    against those tables, not a raw dword search.
- **F2 — DONE 2026-09-30:** the DMA_PUT bit-16 mask (D1) is removed in toolkit `b857665`, and
  `docs/jsrf-kick-get-contract.md:60` records that `0x100410` is `NV_PFB_WBC`.
- **F2b — DONE 2026-09-30:** ML3 (toolkit `e43e9bf`).
- **F3 — DONE 2026-09-30. THE STRICT HORIZON IS CLOSED.** The kernel thunk table is no longer
  overwritten and `.text` is no longer displaced. Two wrong `tail_jump_alias` folds were fixed by
  recovery entries whose spans end at each function's own jump table:
  `0x00037550` (slot `.data 0x001EC108`, folded into `sub_00038530`, span now `..0x00037FB4`) and
  `0x00026780` (slot `.data 0x001EC10C`, folded into `sub_000278F0`, span now `..0x0002730C`). Two
  further config spans that ran past their terminators were tightened so the recovery pass could run
  at all: `0x000BCF40` (`..0x000BD8B0`) and `0x001063A0` (`..0x00106560`).
  **Measured, strict profile, `RECOMP_APU_TRAP=1`, budget 100000:**

  | Run | Outcome |
  |---|---|
  | `20260930-221054-913-f0b-first-run-new-toolkit` (before) | `0xE0424943` at 5.8 s, 17,906 lines |
  | `20260930-225440-580-f3-alias-fix-strict` (alias 1) | `0xC0000409` at 14.6 s, ABI failure at `0x00026780` |
  | `20260930-225739-446-f3-alias-fix-2-strict` (both) | **`diagnostic_deadline` at 93.0 s**, 487,394 lines, 0 invalid ICALLs, 0 `0xE0424943`, 0 exceptions, 0 ABI failures, 0 `[UNIMPL]` |

  `check-dump-mapping.py` went from `CONTENT_MISMATCH` in every prior run to **`matches: 1,
  content-mismatch: 0`**: `.text` at `0x00011000` is byte-identical to the XBE and `0x001C3F60` holds
  the runtime's installed `FE000000+` thunks. Full finding in `docs/jsrf-technical-record.md` §5.
  **Two caveats recorded with it.** (a) The recovery regeneration rewrote **3042 of 3075** bodies — a
  generator-version effect (`uint32_t ebp = 0`, `RECOMP_FCMP_CC`), not only the two alias repairs; the
  provenance record states this and the two intermediate runs isolate the aliases as what moved the
  horizon. (b) `0x00037550`'s span and `stack_args: 0` were re-verified after review and are correct:
  its own code ends at a bare `ret` at `0x00037603`, the `ret 4` at `0x00038525` belongs to
  `0x00038460`/`0x0003848E`, and the runtime logged
  `[RECOVERED] 0x00037550 returned; ABI verified (ESP/EBX/ESI/EDI)`.
- **F3's continuing subject.** The 93-second run ended at its own deadline with no fault, so the next
  stop is unknown. Fallbacks unchanged: the guest heap keeps failing → ML2 (replacement XAPI heap,
  *reimplemented*); a vblank wait hangs → ML5; missing APU interrupts or slow audio clocks → ML4.
- **F4 — Frames.** *(Historical — this was the active next step when recorded; §13's current-state
  block above supersedes it. The logo/cache observation work carried in this entry is **F4b** under the
  naming note above.)* **The `0x1720` walk blocker is IMPLEMENTED (toolkit `1f9309a`);
  the next measurement is whether the walk now advances past GET `0x8EF0`.**
  **Diagnosed 2026-09-30: the submission walk stops on the first method the model does not know.**
  On the fixed build (`20260930-230206-594-f4-frames-after-horizon-fix`, exploratory, 123.3 s, 629,781
  lines, 0 invalid ICALLs/exceptions/ABI failures/`[UNIMPL]`), the run logs **64** `[PFIFO] submit`
  lines: the first ten advance `get` (`0x1000` … `0x8B5C`), and **all 54 remaining report the same
  `get=00008EF0`** while `put` keeps advancing. Submits #12 onward carry
  `diag=unsupported_method … method=1720 … at=00008EF0`. `0x1720` is
  **`NV097_SET_VERTEX_DATA_ARRAY_OFFSET`**. `FLIP`, `present` and `FB_DUMP` are all 0 because nothing
  past that command is ever interpreted.
  **IMPLEMENTED 2026-09-30 in toolkit `1f9309a`, by the measured-inventory route, not a bypass.** The
  walk still rejects anything absent from the generated table; `0x1720` is now *in* it, because a real
  submission contained it. Seven NV097 methods were admitted, every one measured:
  `0x1720 0x172C 0x1730 0x1744` (the vertex-data-array-offset slots the title uses) and
  `0x1800 0x1804 0x1808` (PGRAPH antialiasing / blend / blend-colour). It is deliberately **not** the
  whole `0x1720..0x175C` array: the array is indexed, so a blanket range would admit slots the title
  never submits. Only the four measured slots are admitted and the first unmeasured slot (`0x1724`)
  still rejects — a test pins that asymmetry. Admitted methods flow through the existing state path
  (`pgraph_method` stores them in `PGRAPHState.methods`); no execution semantics were invented.
  **Two generator defects had to be fixed first**, both in `scripts/gen-nv2a-method-inventory.py`, and
  both are why the method was invisible: its decode budget was **4096 words** while `0x1720` first
  appears at word **8124** of the F4 ring's 72,353, and it derived the table from **one** ring, so the
  F4 ring alone would have **dropped 148 methods** the older ring contributes. The table is now the
  union of the rings named on the command line.
  **Tests:** five new functions in the toolkit's `tests/nv2a_actions_test.c` — accepted and staged,
  GET advances past it, the indexed-range control, unrelated methods still rejected, wrong-class and
  unbound still rejected, and a stream through the block commits. Verified both ways: all pass with
  the fix, and **15 failures without it** (GET pinned at `0x1000` with `unsupported_method`).
  **What this does NOT establish: that frames exist.** The next runtime question is the one below.
  **SMOKE-MEASURED 2026-09-30 (`20261001-004608-186-f4-smoke-1720-admitted`, 47.3 s): the blocker
  MOVED, and GET did NOT advance.** `unsupported_method` is gone (0 occurrences, was 54), so the
  admission works. The walk now stops at the **same** `get=00008EF0` with
  `diag=sink_capacity`, on all 52 submissions after #12, while `put` advances to `0x47A84`.
  **Cause, decoded from the ring:** the failing submission (`0x8EF0..0xA440`, 1364 words, 259 packets,
  0 jump words) stages **1109 methods**, and the sink is a per-submission array of **1024**
  (`nv2a_core.c`, reset per submission at `:1468`), so it overflows **within one submission** at packet
  #239 — not by accumulating across submissions. Integrity stays clean (0 invalid ICALLs, 0 exceptions,
  0 ABI failures, 0 `[UNIMPL]`); `FLIP`/`present`/`FB_DUMP` are still 0.
  **SUPERSEDED 2026-10-01 — owner-directed F4 sink ruling recorded.** Advisor chose A′: size both
  staging and sink to the existing 4096-word walk budget, move staging into PFIFO state, retain
  whole-submission atomicity and all other rejection rules. The local atomicity approximation is
  recorded as L40; hardware/prior art dispatch per method. Verbatim decision, basis, tests and reversal
  conditions: `docs/reviews/rulings/f4-submission-capacity.md` (TR §5). **Implemented; focused tests
  measured:** both sink and PFIFO-owned staging hold 4096 entries, both statically asserted against the
  word budget; 1024-packet budget and action/rejection atomicity unchanged. New tests accept 1109
  methods over 555 packets (1664 words, same method count but different shape from the real stream)
  and one count-1025 packet (1026 words). Reversing only the toolkit fix produces **9 assertion
  failures**; restoring it produces **344 passing register/clock contracts**. Toolkit action checks
  pass. The fixture holds only 2048 words, so no >4096-word budget rejection fixture was added;
  the static assertions pin method-capacity ≥ word-budget. Toolkit commit `e8a6e03`.
  **Full validation passed:** toolkit Release build, 5/5 CTest, 30 lifter unittests; game `just check`
  and `just test` (29/29 CTest). `xbox_guest_meter` passed both runs.
  **Bounded smoke measured:** `20261001-020407-358-f4-capacity-fix-smoke`, game `3be0adb`
  (archived record-only citation diff), toolkit `e8a6e03`; exploratory default-on GPU_ACK plus
  APU_TRAP/PB_EXEC/FB_WINDOW/log budget 100000. Requested 45 s, actual 48.336403 s,
  diagnostic deadline. Logged submissions #0–63 all OK, GET=PUT through `0x47A84`, beyond
  old `0x8EF0`; final GET=PUT `0x16648`. No sink/budget rejection. **Draw/clear/flip/present are
  UNKNOWN for this capture, not zero** — corrected 2026-10-01 by the Advisor fault-diagnosis
  ruling: the only `[GPU]` executor lines are printed during `xbox_MemoryLayoutInit`, before the
  guest ran, and `RECOMP_NV2A_TRACE` was unset, so no end-of-run counters exist. F4 frames remain
  unsatisfied; no strict horizon claim.
  Active ledger L14–L18, L20–L25, L39, L40; L19 dormant. Advisor **W14 CONTINUE until
  11:00 UTC or two more smoke iterations, whichever first**. Next: read-only deadline
  main/render wait-site diagnosis on this archive first; GPU-specific survey only if waits
  point to GPU. **Archive diagnosis corrected (Advisor fault-diagnosis ruling, 2026-10-01):** the
  guest looks like a running game loop — main tid 6984 polls input via `XGetDevices` in game code
  (`sub_00161C20`), with `DirectSoundDoWork` (`0x0019F260`) and the DSOUND critical section
  (`0x0019E438`, CS `0x001BA050`) repeating. Candidate flag `0x001BA04C` is DSOUND library state
  (**not** a render-arming flag; drop it as the F4 lead): 48 references, 43 `cmp …,0`, and one
  write `mov [0x001BA04C],1` at `0x001A2317` inside a DSOUND method calling `0x1A1C8A`. Thread
  59696 is in D3D `BlockUntilVerticalBlank` (ret `0x0018CE73`) and that return site appears 1,622
  times, so vblank delivery is INFERRED to work and the interrupt-delivery hypothesis drops to
  low. INFERRED from raw stack words (not unwound): D3D state-call addresses (`SetStateUP`,
  `UpdateProjectionViewportTransform`, `SetScissors`) sit in main's live stack region, suggesting a
  render path. **Open question: whether it renders and presents — this capture cannot answer it.**
  **ANSWERED 2026-10-01 — the instrumentation was inert, and the answer needs a fix first.**
  Observation run `20261001-023335-656-f4-observation-60s` (game `c01e292`, toolkit `e8a6e03`,
  `exe_sha256 1ecf8363…` identical to the capacity smoke) ran with `RECOMP_NV2A_TRACE=1` and
  `RECOMP_PB_SCAN=1`: 62.580312 s `diagnostic_deadline`, dump/checkpoints/GPU report good, but
  **zero `[PB]` lines and no post-`guest_entry` render output**. Root cause (Advisor, MEASURED in
  source): `src/main.c:529` installs the MMIO state owner → `nv2a_mmio_hook.c:681`
  `xbox_Nv2aClaimRegisterOwner()` → `xbox_memory_layout.c:173-179` clears `g_nv2a_ack_enabled`, and
  the legacy GPU body `:1048-1177` (acks, DMA_GET mirroring, pushbuffer scan, executor call,
  periodic report) sits inside that check. So **L16/L18 are set but inert on this build** and the
  switch is itself a **new instrument defect**. MEASURED: 64 `[PFIFO] submit` lines all `diag=ok`,
  GET=PUT through `0x47A84` (committed methods are real); six `[GPU]` lines all at log 32-38, before
  `guest_entry` (line 75). **Runtime counters are UNKNOWN for this capture, not measured zero**;
  that the executor never runs under the owner is **INFERRED from the code path**.
  **Ruling A (Advisor), FINAL GO design:** the owner's NV097→`PB_EXEC` path must be reached with
  **no second walk and no second GET**; rejection stays **atomic** (reject without executing);
  fixtures must pin **clear/flip counts**; the run must show a **post-guest periodic `[GPU]` report**
  as positive proof. Order: **capture per entry class → bindings → `action_commit` → consumer
  (ordered all committed classes; the kernel executes the NV097 subset) → last method → GET**. **Interface refinement (Advisor APPROVED):** the seam
  takes **four args `(subch, class_id, method, param)`** and **all committed entries reach the core
  callback**; the **kernel wrapper** filters to NV097 and keeps the skip count — replacing the earlier
  3-arg NV097-only-core-consumer shape (a policy-free core is the better factoring). Order and
  atomicity unchanged (callback after `action_commit`, inside the ok path); tests must show a
  **non-NV097 entry leaving `EXEC` counters unchanged** while the **wrapper skip count increments**;
  core local-callback tests check the **correct class including across a rebind**; the **skip count is
  read for the report under the same PFIFO lock**. Not a new shortcut, **no new ledger class entry**.
  Runtime seam: **core static fn + setter**, no env var, **no weak symbol**; "no `extern`" means the
  **core holds no `extern` executor reference** while the **kernel does register the seam**;
  registered before guest start, `PB_EXEC` presence the only trigger. Integration: **existing HAL
  target links across the whole toolkit** (dedicated real-exec minimal stub target only as fallback);
  earlier "A2 new targets" **superseded**. Standalone core tests: order, atomicity, rebind with a
  local callback. Owner lock: **free getter on the existing active flag**; legacy guard **logs once
  and skips `EXEC`**. Report only under the **consumer lock, 10 s cadence**. Flip accounting adds a
  **NEW `flip_stalls`** counter: `0x0130` is a completed swap that calls `FrameCounterFlip` but
  **does not increment the existing `s_gpu.flips`**; **`0x012C` is the `s_gpu.flips++`**.
  **Game-side registrant audit mandatory — no blocking callbacks under the lock.** W14 is extended
  **until A's first smoke plus one diagnostic**; **no more inert reruns**; **L18's edit lands at the
  coordinated cross-repository checkpoint** with the accepted toolkit code (the toolkit SHA recorded
  in the game's ledger record — separate repositories, so not literally one git commit), and until
  then L18 keeps its old entry. **Architecture A is IMPLEMENTED AND ACCEPTED** — toolkit
  `a71f9374ddb2a6685b790493855c835842228212` (9 files, +329/−10); the Advisor read the diff itself and
  ruled ACCEPT/GO with final validation green (core 5/5 in 2.34 s, 30 lifter unittests, game 29/29
  CTest in 28.81 s, all checks pass, conformant to the approved design). **F4 IS MET, exploratory:**
  the 60 s run `20261001-033129-276-f4-a-smoke-60s` produced a coherent guest image (the
  "Presented by SEGA" card), so F4's frame criterion is satisfied under the exploratory profile —
  **not** a milestone acceptance, which is carried with the **F6 milestone Review**. **W14 reset at
  10:32:32 UTC** on that first critical-path frame finding; the earlier extension is closed. The
  180 s observation run `20261001-033805-242-f5-sequence-180s` (same pair + `RECOMP_FB_DUMP`) is
  recorded as **observed** in the strict-horizon ledger and the ruling: 25 BMPs, 2 distinct hashes in
  ordered blocks (8 black, then 17 SEGA), pixels from the **guest draw surface** (not the window), one
  separate window-dump artifact at 10:38:19, counts as **lower bounds**. That run produced **no horizon
  move and no W14 change**; the F5 consult ruling that followed is recorded next. Commit and push the
  clean pair **before** the next run.
  **F5 consult ruling (2026-10-01, Advisor; verbatim in the F5 appendix of
  `docs/reviews/rulings/f4-submission-capacity.md`): the logo phase is NOT a stall** — the guest is
  doing its **first-boot HDD cache fill**, slowly. OBSERVED **360 `[PATH]` opens of
  `\Device\Harddisk0\Partition5\Media\…~`, 184 distinct**, 20–33 per 10 s report to the end of the log
  (last line 209119); the cache holds `Cache00-02.tbl`/`DmCache00-02.tbl` with
  **`JSRF_CACHE_COMPLETE00.CMP` written 10:39:41** and **Cache02 started 10:39:42**; payload **182
  files, 67.9 MB**; rate **~1.0–1.3 files/s in every recent run, with or without the executor** (48 s:
  64; 60 s: 82 and 80; 180 s: 184), so **the executor does not limit it**. **INFERRED** the SEGA screen
  covers the fill in stages (00 then 01–03); **UNCERTAIN** whether the logo is really **fill-gated**.
  The word **"stop" is REJECTED** — the frozen `VirtualQuery` sample in `submit_read_word` is a
  **capture-time location, not a stop** (guest live; the per-word `VirtualQuery` is a perf lead, not a
  defect). Window accepted only as one-shot at 10:38:19.247 (`frames==600`, `fb_present.c:341`), SEGA.
  **NEXT (authorized): ONE 600 s observation run**, same clean pair and profile plus `RECOMP_FB_DUMP`
  and `RECOMP_FB_WINDOW_DUMP_EVERY=600` (both observation-only), with the **≥15 GB disk gate** checked
  first (~5.3 GB save-root per run). **Seeded cache PREPARED, NOT AUTHORISED** — no owner decision now
  and no implementation. **W14 clock unchanged at 10:32:32.** Reversals in the appendix.
  **Duration cap raised to 600 — IMPLEMENTED (Advisor duration-cap ruling, replacement child; verbatim
  in the same ruling file):** one **shared constant** `MAX_RUN_SECONDS = 600` in
  `scripts/jsrf_run_profile.py` (bound **:676**), imported by `scripts/run-jsrf.py` (bound/message
  **:219-220**) — the ruling's own `:218-219`/`:668` citations are its verbatim text and stand as
  written. Measured boundaries: `1 -> 1`, `300 -> 300`, `600 -> 600`, `601`/`0`/`-5` -> `SystemExit 2`.
  **RED** (unmodified 300 caps): **4 failed, 2 passed, 6 subtests passed**, exit 1. **GREEN:** focused
  **4 passed, 10 subtests passed**; full `test_run_profiles.py` **38 passed, 100 subtests passed** in
  **1.29 s**; `just check` **exit 0**; `build-identity.py verify` **rc 0** (exe unchanged). Guards
  preserved (identity verify, empty save-root, disk gate, archive, classification). Commit title
  `harness: permit bounded 600-second cache observations`. **300 s
  is not a substitute** — two 300 s fresh roots each restart the first-boot fill and cannot show the
  >300 s decision rows. **The first 600 s attempt FAILED to launch**: `2026-10-01T11:01:26Z`, **rc 2**
  on the **dual 300 cap** — a **harness launch fact, not a runtime or guest fact**: no game run
  occurred, no run artifact exists, only launch logs; recorded as a launch failure and **not** a
  horizon entry.
  **600 s run TAKEN — `20261001-043629-961-f5-observe-600s-c4bcd2b-retry1` (exploratory; NOT a title,
  NOT a horizon move).** Parent-owned job `pwsh-2573`; same pair game `c4bcd2b` / toolkit `a71f937`,
  fresh empty root, no seed, `exe_sha256 7027fafa…b303` unchanged. `diagnostic_deadline`, exit 3,
  166 named frames, 1 snapshot 0 dropped, `gpu_report_ok` true. **Stop 11:46:33.913916** =
  `metadata.started_utc` 11:36:30.743170 + `result.duration_seconds` 603.170746. Exact typed counters
  (`gpu-reports-v2.csv`): clears 10500, draws 10500, with_coordinates 10500, indices 52482, flips 3506,
  flip_stalls 3506, unhandled_methods 1178547, distinct_unhandled 242, non_NV097_skipped 14.
  **Counts rise while the images stay static SEGA by eye — not new images, still not the title.**
  19 tables `distinct_mtimes = 1` ("not observed to change"); only the nine zero-byte `.CMP` markers
  change; **19/19 tables = DVD bytes + zero padding to a 512-byte sector** (exact padding fact).
  **Observer/helper full hashes: see the TR's 600 s paragraph (single authority — not duplicated here).**
  **No new run and no seed** until the CMP/table check loop is explained. **No wrapper-argument or
  loop-cause interpretation recorded** — *as recorded at the 600 s checkpoint; **superseded by D1/D2
  below*** (the source candidate is now explained and its test is pending; the no-seed decision is
  unchanged). Ledger IDs L14, L15, L16 (legacy ack
  retired; feed replaced), L17, L18 (owner consumer), L20–L25, L39, L40; L19 dormant. **W14 reset
  unchanged at 10:32:32; no strict-horizon move** — as of **12:10 UTC the clock has elapsed 1 h 38 m**.
  **W14 RESET (2026-10-01, superseding the above):** the D2 300 s smoke produced the **actual eligible
  event — the unnumbered `JSRF_CACHE_COMPLETE.CMP`** (0 B, `CreationTimeUtc == LastWriteTimeUtc ==`
  **`2026-10-01T13:27:49.9575858Z`**) — so the **horizon moves to `13:27:49.9575858Z`** and the
  **ceiling to `17:27:49.9575858Z`**, **reset count 0**. The old `14:32:32` is **historical and
  superseded**. The eligible event is the **actual marker, not a frame**. **F4b CMP loop resolved; the
  new stop is after the marker phase.** **No title claim** (the frame is still SEGA).
  **Owner-resume W14 ceiling call (2026-10-01 18:19 UTC): CONTINUE with a fixed bound.** Same-route replacement Persistent Advisor `356aed7e-07f5-4327-aa32-88bdfe21ac8b` passed independent-read/second-message continuity under workflow §4.4 after prior child repeatedly failed delivery. Conservatively count wall time; owner pause does not reset the clock. No accepted source-only finding/reset, and no strict-horizon claim from the exploratory CMP event. Bound: **U1 plus conditional U2 reported, 20:15:00Z, or one packet, whichever first**. At that bound without an accepted critical-path finding/new eligible event: **STOP for owner decision, no second ceiling call**. U1: one source-only worker ≤45 min checking reset writers/update gates against original XBE. U2 only on a complete negative U1: existing binary/profile, canonical object captures ≥60 s apart and advancing frame-count control, no code/instrumentation/seeding; unchanged harness or explicitly authorized weaker 240 s/300 s cross-run captures. Full oracle, predicates and constraints: `docs/reviews/rulings/f4-submission-capacity.md` §W14 ceiling ruling on owner resume. **A/B still UNCLASSIFIED; no new run/patch/build yet.**
  **FINAL W14 STOP — owner decision required (2026-10-01 ~18:29–18:31 UTC).** U1 returned **UNKNOWN**: unresolved per-frame receiver/alias/inline writers, not proven immaterial. Bound (a) reached, **U2 not authorized, no second ceiling call or clock reset**. Replacement Advisor delivered STOP and accepted corrected branch/reachability wording. No new build/run/patch; D2 accepted, A/B UNCLASSIFIED, no F6/title. Parent original checks preserve only frozen eligibility: all four mode words zero permits `11070`; any nonzero branch skips it via JMP `124C3`; stage8 with ID1DDA present skips `666F0`. Worker raw-byte scan/address-reference overclaims rejected; no new cause or exhaustive writer proof. Owner options: recommended small observation-only packet for same-object within-run reads with RED/GREEN/Reviewer gates; weaker one-run cross-capture candidate; explicitly ledgered pragmatic shortcut; or pause/end. **None is authorized until owner chooses.** Full ruling/qualifications in the ruling record's §U1 UNKNOWN and final W14 STOP.
  **New stop — F6 candidate, UNCLASSIFIED after the cache.** ***Superseded reading:*** the earlier
  "`1A03` loop / poll" interpretation was the **sampled callee region** and the Advisor's **initial
  hypothesis**, **not the actual poll head** — **kept as history**. **Current classification
  (Advisor, own reads; read-only):** the **277/294 traffic is PER-FRAME RENDER WORK, not a poll** —
  **277@19E452 = 12 × 515**, **294 sites = 3 × 515 and 1 × 515**, IRQL pairs ≈ 515 each, **E_FAIL paths
  0 times**, all lock-stepping at **≈515 = one set per frame**; so **≈515 frames after the marker over
  ≈90 s ≈ 5.7 fps** (a **rate estimate only** — nothing timestamps individual frames). **Capture
  state:** root stack `… → 13A80 (game main loop) → 14D090 → 198F10 → 198ED0 → 191390 → 1912A0`
  (push-buffer kickoff), with **PFB_WBC = 0** and **GET == PUT == 0x5B50C**, so the flush and queue were
  **not stuck at capture** — a **snapshot fact, not proof of no stall at any other time**. **F6 candidate
  remains UNCLASSIFIED:** **(A)** a slow frame/time-counted sequence vs **(B)** a step gated on an event
  that has not happened (audio/movie/thread, given `RECOMP_APU_TRAP=1`). **Not called a stall or a
  hang.** **Next action — READ-ONLY, one stop (parent-assigned to the worker, in progress):**
  disassemble **`13A80`**'s per-frame dispatch, the vtable calls (`call [edi+0x14]`, `[edi+0x1c8]`,
  `[ecx+0x20]`) from **`13CB2–13EE4`**, with **`14D090`**; identify the active logo/scene object and its
  **step/timer field**, then read those fields from the new dump (the **mapping gate has already
  passed** for `062415`). **No run, no fix, no seeding** until (A)/(B) is classified. **The D2 closure
  is independent and stays accepted.**
  **Log evidence (parent-read, own):** lines **234619–622** show the **unnumbered marker `CREATE`** with
  return **`1456CB`**, the **`PATH`**, and **status 0** (the accompanying token in that record is not
  interpreted here).
  **Strict-path audit (qualified):** the strict-path counts came out **12 vs 474 matches**, and that
  set **includes the demo**; the **marker creates fall in an adjacent 6-line window**, which is
  **qualified — not a tid proof**.
  ***Historical — F4b D1 and the D2 design before GREEN.*** *As recorded then:* the F4b D1 probe was
  **INCONCLUSIVE** (`s_dir_contexts` live VA with **0 containing ranges**, **0/4928** readable slots,
  coverage **0.96%** over 113 ranges; **cause NOT proven**, occupancy UNKNOWN); the D2 design was
  **approved** with **no new run until its RED/GREEN and test gate passed**; the Advisor
  **`63c4869f…`** (ACK 1 + ACK 2) had delivered the **A–E ruling, reply 3, the D2 design ruling and the
  harness-plan ACK** (all verbatim in the ruling appendix); and the **bounded unit**
  (`docs/packets/f5-directory-context-close.md`) was **RED-only, parent-authorised**, with no ledger
  entry because the fix was **not yet introduced**.
  ***Current — ACCEPTED, tested, toolkit pushed.*** The bounded unit is **ACCEPTED**: RED verified →
  GREEN verified (focused 2/2 0.19 s; toolkit CTest 7/7 2.71 s; lifter 134 OK skipped=1 14.460 s; game
  31/31 31.32 s; `just check` passed) → **300 s smoke PASS-F5 (a)** with the **unnumbered
  `JSRF_CACHE_COMPLETE.CMP`** created; **W14 reset to horizon `13:27:49.9575858Z` / ceiling
  `17:27:49.9575858Z`**; **toolkit pushed** as **`a8262014eec9cd8d720184f7f2fb7dce4652105d`**
  (fast-forward `a71f937 → a826201`, 5 files, 571+/1−, test included, 0 blobs/secrets). **CURRENT
  PACKET remains none** — this is a bounded F5 unit, **not** a workflow promotion.
  Corrected evidence in TR and `logs/workers/f4-drained-no-frames-brief.md`.
  **Historical 2026-09-30 question (answer recorded then; superseded by the accepted capacity work
  and now by Architecture A):** Whether the answer is a larger sink,
  a sink that drains as it fills, or incremental commit during the walk is a design question about what
  the sink is *for*, not a constant to raise. Do not start the next long run until that is decided.
  *(Also noted: the comment at `nv2a_core.c:1461-1467` says "its 256 cap" while the array and its test
  are 1024 — a stale comment, not behaviour.)*
  **The next runtime question (historical, ANSWERED 2026-09-30):** *does the submission walk advance
  beyond GET `0x8EF0`, and if so, what is the next measured stop or the first draw/flip event?*
  **ANSWERED: no, it did not advance; the next stop was `sink_capacity` at the same GET (see the smoke
  measurement above).** That capacity blocker is **closed** (A′ accepted, toolkit `e8a6e03`).
  *(Superseded history: the Architecture A question — whether the owner path yields a post-guest
  `[GPU]` report and whether GET passes `0x8EF0` — was **ANSWERED** by the 600 s run above, which did
  produce post-guest reports. The **active** question is now F5's: the CMP/table check loop, pending
  the Advisor.)* Fallbacks once
  frames exist: missing draw forms/formats are fixed in the executor; a GPU stall on this path → ML6;
  if the title needs register combiners the CPU executor cannot show, start ML7's feasibility study.
- **F5 — Intro movies.** If the Sofdec intros block, skip them (ledger: *patched* or *intentionally
  ignored*); decoding them is post-slice (M29).
- **F6 — Title screen (M15).** Acceptance: a frame dump of the title screen plus the run record with
  its ledger IDs; compare by eye with an xemu screenshot of the same screen (T3). One Acceptance
  review for the milestone.

**Decisions taken with this direction (2026-09-30), each reversible by the owner:**
- The disk floor for ordinary runs is 15 GB (`scripts/check-disk-gate.py`); a run measured a few GB.
  TTD recordings keep the 50 GB expectation. Deleting old runs stays an owner decision and is not
  needed now.
- The strict-horizon ledger's scope is **from 2026-09-29 onward**, now the lint's default
  (`--since all` covers the archive); the 36 earlier runs are not backfilled.
- C3 is parked and C5 is decided (executor) for the bare minimum, as recorded in §7.

### Earlier entries

1. **T14's free-space gate**, then **V1 → V2 → V3 → V4** on the Windows host (DeepSeek chores;
   0 senior calls); start the strict-horizon ledger (W14) with V3's result.
2. Alongside, on any host: **T6, T7, T4**, then **T1, T2, T8–T13, T5**; T3 when the owner supplies
   the images. Owner approves the W-row document edits as they land (W7, W8, W15 first).
3. Then **C1** as the first packet (after W11), with C5's Advisor preflight in parallel.

**Updated 2026-09-30 (second entry, end of session).** Supersedes the note above.

**Phase 1 is complete** (T14, T6, T7, T4, T1, T2, T3, T5, T8–T13), and **Phase 2's
checks are implemented** (W1, W2, W3/W10, W4/W12, W5, W6, W7, W9, W11, W13, W14, W15,
W16; **W8's fallback half is owner-reserved**). `just check` runs ten checkers.

**The critical path moved twice more, and both moves are measurements:**

1. The `0xC0000409`-under-TTD reading was **wrong**: it was a missing
   `RECOMP_APU_TRAP=1` in the launch environment. With it, a traced run reaches the
   horizon (23,484 log lines, same site, same exit code).
2. **TTD cannot see the write class C1 is looking for.** 400,000 writes scanned, zero
   outside `jsrf_recomp.exe` and `VCRUNTIME140` — kernel-mode writes are invisible —
   and the clobber is confirmed from a **non-TTD** minidump. So C1's instrument choice
   is re-opened with the Advisor under §2.3.

**The §2.3 ruling has landed** (`docs/reviews/rulings/ttd-query-decision-input.md`,
"C1 instrument (2026-09-30)"): **W11 stands, TTD remains C1's instrument**, and the
Session's contrary conclusion was withdrawn because it rested on a trace truncated at
its size cap.

**C1's conditions are now all measured, and they reduce to one recording problem.**

| Condition | State | Evidence |
|---|---|---|
| C-a (process-exit end, below cap) | **achievable, but it excludes the horizon** | `--seconds 12` gave `Process exited … after 6797ms` at 240 MB — exiting `0xC0000409` **before** the horizon at ~7.3 s |
| C-b (terminal in the trace) | **FAILS on every existing trace** | 58 recorded reads of slot 65, **0** of them zero (`docs/reviews/c1-slot-reads-in-trace.md`) |
| C-c (mirror positive) | **MISSING** | a real store through a mirror VA is still required |
| C-d (kernel-write control) | **MISSING**, now possible | the toolkit logs `dst=` (`xboxrecomp` `1572256`) |
| C-e (census on an admitted trace) | blocked on C-a **and** the horizon | — |

**The trade is measured, not assumed:** a bound short enough to end by process exit ends
at 6.8 s with `0xC0000409`, before the horizon; a bound long enough to reach the horizon
is capped before the process exits. **`ttd -stop` is the only path that satisfies both**,
which S1 already anticipates — *"A trace ended by `ttd -stop` is out of scope unless the
terminal position precedes the stop."* The trigger must be a signal from outside, not a
timeout or a cap.

**The Session is BLOCKED on the disk floor before it can take that step.**
`scripts/check-disk-gate.py` reports **33.9 GB free against a 50 GB floor**, and T14 makes
clearing it an **owner decision** (*"archive or delete the rest after owner approval of
the policy"*). The reclaim plan reports 1,131 candidate runs holding 163 GB, of which the
808 `test` runs alone are **111 GB** — more than twice the shortfall. **None was created
by this session**; every run this session created is cited by a durable record and kept.

**So the next action is an owner decision, not a Session step:** approve a reclaim of the
archived `test` runs (or another subset), after which the Session can record the
`ttd -stop` trace, evaluate C-b on it, and promote
`docs/packets/c1-slot-write-attribution.md`.

**A second owner decision is recorded** in `docs/reviews/ttd-recording-termination.md`:
`check-horizon-ledger.py` without `--since` reports **36 pre-2026-09-29 runs** with no
ledger row, while `just check` passes because it passes `--since 2026-09-29`. Either the
ledger's scope is "from inception onward" and the bare invocation should say so, or the
rule is retroactive and 36 rows need backfilling — a packet-sized job.

Until a packet is promoted, packet implementation remains BLOCKED and the chores run
as owner-directed work.

### Follow-up leads from the 2026-09-30 F0/F1 session (recorded, not acted on)

- **`check-horizon-ledger.py`'s "last horizon move" reads a stale row, and always has.**
  It computes `moved = [row for row in rows if not row['not_reached']]` and takes the
  last one (`check-horizon-ledger.py:222-223`), but the ledger's **"Prior horizon
  (superseded 2026-09-29)"** table sits *below* the session rows, so its 2026-09-28
  entry is the last non-`NOT REACHED` row. Measured: at `HEAD~5` (before this session)
  and in the current tree alike, the reported value is **2026-09-28
  `20260928-185612-449-regen-v012-strict`**, even though rows for 2026-09-30 exist and
  this session moved the horizon twice. **This is pre-existing, not introduced here**,
  and it is a checker defect, not a ledger defect: the ledger itself is right. It
  matters because W14's ceiling rule ("3 packets or 4 h without moving the horizon")
  is judged from that number, so the rule has been measuring the wrong date. A fix
  would scan only the session rows, or exclude the superseded table explicitly.
- **`tests/test_generation_provenance.py` is not registered in CTest.** `CMakeLists.txt`
  names no such target and `ctest -N` lists 28 tests without it, so `just test` cannot
  catch a provenance failure — only a bare `just check` can. That is why the gate was red
  for 73 commits without a session noticing. Registering it is a small chore; it is a
  lead here because it changes what `just test` means.
- **`scripts/recover-functions.py` cannot complete a from-nothing rebuild.** It aborts at
  `0x000BCF40` (`RuntimeError: Incomplete translation`): the configured span
  `0x000BCF40..0x000BD8D0` runs 0x20 bytes past that function's real `ret` at
  `0x000BD8B0` into padding that decodes as `aaa`, which the lifter reports as
  `/* TODO: aaa */`. The live `recovered.c` predates the check and is unaffected. This is
  the first concrete blocker for the rebuild AGENTS.md records as never done end to end.
  **Diagnosed and fixed-in-principle 2026-09-30.** The abort is **not** a regression from the
  `86113c7` pull: a worktree at the pre-pull `1572256` emits the same `/* TODO: aaa */`
  for the same entry, measured directly. The committed `body_000BCF40` stops at
  `loc_000BD8AB` (its last label; no label at or after `0x000BD8B0`), so the generation
  that produced it never walked the padding — the config's `end` and the check have been
  inconsistent since `ddb0697` tightened bounds.
  **The fix is one field and it is verified in memory:** ending the entry at
  `0x000BD8B0` (the `ret`) yields a body with **0** TODO markers and all real labels
  preserved (`000BD889`, `000BD8A9`, `000BD8AB`); `0x000BD8B1` and the other candidates
  also clear it, but `0x000BD8B0` is the instruction boundary. Applying it is part of the
  F3 recovery work below, because it must be followed by a regeneration and a re-run.
  The boundary is confirmed against the XBE's own bytes: `.text` at file offset `0xAD8AB`
  holds `5e 5d 83 c4 10 c3 8d 49 00 90` — `pop esi` / `pop ebp` / `add esp,0x10` / `ret`
  at `0x000BD8B0` / `lea ecx,[ecx]` / `nop`, then padding.
  *(A note here first claimed `inspect-jsrf.py data` and `disasm` disagreed about the byte
  at `0x000BD8B0`. They do not: `data` prints little-endian dword **values**, so its
  `C4835D5E` is the bytes `5e 5d 83 c4`. The claim was a misreading of the output format
  and is withdrawn; it is recorded because this is exactly the transcription-error class
  plan W5/T10 exists for.)*

If no packet is promoted in the authoritative plan file, packet implementation is BLOCKED; chores
listed here run as owner-directed changes once the owner adopts this plan.

## 14. Closed packets

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
| Toolkit sync to v0.12.0+ | done (owner) | merge `2925f0b`, stop unchanged | §1 |
| CRT 64-bit divide helpers | done (owner) | hand-written, unit-tested | §2 |
| Regeneration with v0.12 lifter | done (owner) | same stop; D3D release difference open | §2 |
| Fork audit + owner-directed toolkit fixes | committed to `main`/`master`, unverified on Windows | TR §7 table | §7 |

`A2h-r6` was retired (premise refuted; the failure was already fixed by `cb7cae2`).
