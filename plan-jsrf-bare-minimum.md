# Jet Set Radio Future: Windows port plan — refresh

**Status: ADOPTED by the owner 2026-09-29; this file is the execution authority.** Every decision
taken while writing it was recorded, with its reason and what would reverse it, in the session
report `report-jsrf-bare-minimum-refresh.md`, removed 2026-10-02 (reports are not kept in the
repository); read it with `git show 84e7a93:report-jsrf-bare-minimum-refresh.md`.

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
  review per step. The Packet reviewer checks the milestone (M15) with its ledger IDs.

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
- **Workflow fixes are tasks** (§6), each tied to a measured failure pattern, with DeepSeek kept as
  Session and workers and the limited models spent only at named gates and Advisor triggers.
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

- **DeepSeek Session and workers execute everything by default** (unlimited). Planner, Muse
  guardrail and reviewer calls are made at the gates `docs/agent-workflow.md` names; the Advisor is consulted at those gates **and** on every
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

### Current state (2026-10-02; read this block first)

1. **The SEGA card is held by an unfinished fade.** Located with the xemu oracle (T3): the logo
   update `0x7E360` reaches phase 2 and waits for `0x24650() != 0`, the done flag at `+0xC0` of the
   subsystem-6 fade object. On hardware the card holds for more than 120 update frames, then fades at
   1/120 per update and moves on to the Smilebit logo. In the port the logo sits in phase 2 with its
   hold counter at 121 and the fade at alpha 0, target 1, done 0. The fade update `0x24700` is
   translated correctly; the leading explanation is that the armed fade instance is never updated,
   cause not yet established. Evidence: `docs/reviews/owner-sega-600-observations.md`.
2. **Ruled out** by 600 s runs and archive reads: a slow logo (static image for 400+ s after the cache
   marker), file I/O or a stalled movie (no file opened or read after the marker), the do-nothing
   kernel bridges (none first called after the marker), and folded aliases (all 134 alias counters
   zero after the `0x14FEF0` recovery).
3. **`RECOMP_APU_TRAP` (L21) is required to reach the logo:** without it the guest spins in
   DirectSound initialisation at `0x001A18D0`.
4. **Latest changes:** L02 recovery of the render-state method `0x14FEF0` (game `5c1d7d6`); run dumps
   capture the executable's globals, read with `scripts/read-host-symbol.py` (game `d385e37`,
   `651595f`); the 67 do-nothing kernel bridges name their first call (toolkit `929856f`).
5. **Latest run:** `20261002-174731-263-owner-l02-d2-600`, exploratory D2 profile with
   `RECOMP_APU_TRAP=1`, 602.8 s deadline, still on the SEGA card. Ledger IDs L14–L18, L20–L25, L39,
   L40 (L16 inert under the MMIO owner, L19 dormant).
6. **Parked by the owner:** the F5 fail-fast observer; no retry.
7. **Next:** find what should update the armed fade and why it does not. In xemu, the caller chain
   of `0x24700` during the **phase-2** fade (the recorded chain `1108A → 11096 → 11096 → 124C3 →
   13B24 → 13F9E → 6FA41` is from the phase-0 fade); in the port, whether those callers run during
   the hold.
8. **Answered 2026-10-03 (owner-directed discovery, records only).** The **phase-2 chain is the
   phase-0 chain**: all 10 phase-2 xemu hits at `0x24700` carry the identical return chain
   `1108A → 11096 → 11096 → 124C3 → 13B24 → 13F9E → 6FA41` and the identical fade object
   (`ECX 0x00600E60`, vtable `0x1C4D10`, flags `0x00010003`). The first phase-2 hit *is* the port's
   stuck state (alpha 0, target 1, done 0, logo phase 2, hold 121) and the next is alpha `1/120`, so
   hardware updates the fade on the frame phase 2 is entered. Within the sampled interval (the
   phase-0 fade-in plus 10 phase-2 hits, budget-reached) **no second producer and no second fade
   object were observed**; the sample does not exclude one that was never hit.
9. **The archived port capture answers reachability but cannot decide the object-specific half.** The
   armed fade `0x15F0E60` **is** in the walked list (`0x108FF40 → 0x1340060 → 0x108FFA0 →
   0x15F0E60 → … → 0x143EE60`), its `+4` flags `0x00010003` are non-negative, `app+0xB0` (the
   subsystem-6 slot `0x24650` reads) is that same object, and the tree corresponds to the oracle's
   node for node over that span (one oracle-only sibling after the logo, `0x52EE60`, is a
   later-scene object with negative flags in the successor snapshots and is not on the fade's path).
   The frozen main-thread frame (`sub_00013A80+0x38D2` = `recomp_0000.c:8175`, the `0x13F2A`
   indirect call) **positively establishes** `0x13A80`, `0x13F80` and `0x6F9E0` on the chain, with
   the traversal's gate open (`app+0x40/+0x44/+0x48/+0x4C` all 0). What it does not establish is that
   *that* frame updated *this* object, and the reason is ordering: the walk is preorder and reaches
   the fade (child of `0x108FFA0`) **before** the logo (later sibling), and on the arming tick
   `done` is still 1 — set either by the completion store at `0x2494A` or by the immediate-set path
   at `0x24493`, which this record does not try to distinguish — so `0x24700` returns at
   `0x24703` without writing `+0x98`; the logo then arms it further along the same walk at `0x7E4AF`
   → `0x24540`, which sets `done = 0` and the target and never writes `+0x98`. The frozen state is
   therefore **exactly what the arming frame leaves behind**, so that account is *compatible* with
   everything observed (quick Advisor consult 2; an earlier "live contradiction" reading is
   retracted) — but it is **not** established that the captured activation *is* the arming tick,
   because phase 2 does not increment the hold, so `phase 2 / hold 121 / alpha 0 / done 0` is equally
   the state of every later frame that also failed to update the fade. **Also corrected:** `0x123E0`
   selects exactly **one** of five traversals per activation by priority (`app+0x40` → `0x114D0`,
   else `+0x44` → `0x112A0`, else `+0x48` → `0x11700`, else `+0x4C` → `0x11930`, else the default
   `0x11070`) — not "five times", and the non-default routes do **not** skip the walk: the fade's
   `+0x10`/`+0x1C`/`+0x28`/`+0x34` slots all point at the `0xAECC0` thunk (`mov eax,[ecx]; jmp
   [eax+4]`) back to the same `0x24700`, so all five can reach the update. The `[RECOVERED]
   0x00024700` line is **first-call-only** (`recovered.c:15365`, printed ~136,000 log lines before
   the cache marker) and the walker's sole indirect call `0x11087` is lifted to
   `RECOMP_ICALL_SAFE_AT`, which publishes **no** guest event by construction, so the earlier "no
   retained match" was an instrument artifact, not evidence. The activation counter `app+0x87E0`
   (loaded `0x13F5C`, stored `0x13F6A`, after both the traversal and the Present call, no `ret`
   between) reads 321 / 1016 / 3532 at 62.6 / 182.7 / 603.2 s for the same executable
   `7027fafad9cd7069`, each ending at phase 2 / hold 121 / alpha 0 — so those runs completed
   **different totals** of activations while ending in the same terminal state, and a "loop stopped
   at the arming frame" account is unsupported. An endpoint snapshot is **not** an interval history,
   so this does not show what the loop did between arming and capture. **No cause claimed.**
10. **Awaiting owner approval of one bounded run (not executed).** One exploratory run, **no code
    change and no rebuild**: `RECOMP_WATCH=0x15F0EF8` — the fade's **alpha** field, literal VA,
    because the `[[…]+…+…]` pointer form misparses under the real grammar and would arm on a readable
    address nothing writes — plus `RECOMP_WATCH_RAW=1`, bound `--seconds 150` (a concrete cap,
    **not** established as the smallest sufficient one), otherwise the D2 profile and ledger IDs
    unchanged, fresh disposable save root.
    **Alpha, not the `done` flag** (quick Advisor consult 4 reversed the previous draft's choice):
    `done` carries **no per-step history** — the constructor sets it to 1 (`0x246C1`), the arm clears
    it (`0x24553`), and **both** the completion store (`0x2494A`) and the immediate setter
    (`0x24493`) set it back to 1 — so its value cannot separate alpha **stepping** by `1/120` from
    `0x24700` against alpha **jumping** to 0 through the immediate-set path at `0x24480`.
    Attribution comes from the raw frame (`raw[esp+N]`, `xbox_memory_layout.c:1593–1601`), whose two
    candidate writers have **disjoint** innermost slots: `0x244B8` (alpha set from the argument)
    runs inside `0x24480` before any push, so `raw[esp+0] = 0x2461D`
    ([recomp_0000.c:41101](src/recomp/gen/recomp_0000.c#L41101)); `0x24748` (the `1/120` add) runs
    inside `0x24700` after `push esi`, so `raw[esp+0]` is the saved node `0x15F0E60` and
    `raw[esp+4] = 0x1108A` ([recomp_0000.c:121](src/recomp/gen/recomp_0000.c#L121)). Read those raw
    slots, not the filtered chain, which prints code-looking words and can carry stale values.
    **Positive control: the constructor's alpha `0 → 1.0` store at `0x244B8`**, reached from the logo
    constructor's `0x24600(0xFF000000)` at `0x7E752`–`0x7E757` (`raw[esp+0] = 0x2461D`, caller return
    `0x7E75C`). Two earlier control proposals are withdrawn as unreliable or insufficient: the
    phase-0 fade-in (`72 − 61 = 11` activations lie outside the captured phase-1 hold, and the fade is
    unarmed at capture), and the `done` flag (cannot separate stepping from a jump).
    `watch_report` suppresses unchanged values (`xbox_memory_layout.c:1567–1568`), so this is a
    **changed-value** watch, not a write log: a `1/120` step with `raw[esp+4] = 0x1108A` proves the
    update ran, and a jump to 0 with `raw[esp+0] = 0x2461D` proves the setter ran — but **silence is
    not decisive**, because a skipped FPU branch, a failed watch (protection lost, alias write) and a
    change falling outside the 150 s window all look identical.
    **Stronger no-rebuild alternative, recorded not adopted:** `just ttd-record` runs on the current
    build and shows every `sub_00024700` entry with its `ECX`, `[esi+0xC0]` and the branch taken; it
    needs an **elevated** process, a large trace (recipe caps at `--max-file-mb 20480`) and a
    strict-profile run, so it is the escalation if the watch is ambiguous.
    `RECOMP_WATCHDOG_SECS` is stripped by the runner and `RECOMP_PEEK` cannot separate the cases.
    Details, artifacts and hashes: `docs/reviews/owner-sega-600-observations.md`,
    "Owner-directed discovery: what should update the armed fade (2026-10-03)".

### Fast path status

- **F0, F0a, F0b — done.** Runs gate at 15 GB free; copy `src/recomp/` aside before pulling a commit
  that untracks it; rebuild and test on a new toolkit before the first run.
- **F1 — done 2026-09-30.** `RECOMP_RDATA_GUARD=1` named the kernel thunk table's writer (D5).
- **F2, F2b — done 2026-09-30.** D1 (DMA_PUT bit 16) and ML3 (file flags).
- **F3 — done 2026-09-30.** The strict horizon is closed: two wrong `tail_jump_alias` folds were
  recovered, and strict runs reach their deadline with an intact `.text` and thunk table.
- **F4 — met, exploratory, 2026-10-01.** The SEGA card renders through the owner's NV097 consumer
  (Architecture A, toolkit `a71f937`).
- **F4b — logo and cache phase, in progress.** The first-boot cache fill completes after the
  directory-context fix (toolkit `a826201`); the hold after it is current state item 1.
- **F5 — intro movies.** If the Sofdec intros block, skip them (ledger: *patched* or *intentionally
  ignored*); decoding them is post-slice (M29).
- **F6 — title screen (M15).** Acceptance: a frame dump of the title screen plus the run record with
  its ledger IDs, compared by eye with an xemu screenshot of the same screen (T3); one Packet review
  for the milestone.

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

The full record of F0–F4b, the owner checkpoints and the follow-up leads, as this section held it
until 2026-10-02, is in `docs/jsrf-operating-history.md`, "2026-10-02 — Plan §13 history moved out
of the active plan".

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
