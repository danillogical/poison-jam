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

## Current work (state 2026-10-06)

**What boots.** With the title-path switches (`just title-run`, ledger L14–L18, L20–L25, L39, L40):
SEGA → Smilebit → ADX → Dolby → the graffiti disclaimer (frame hashes `5bdaea576b8509f5` and
`87683a748e27d071`, both states seen in one run). Then presents stop at a fixed count: **1000** in
30 of 35 runs before game `0445a80`, **888** in all four after it (TR §19). Runs with **zero**
dispatch failures stop there too (g06 `20261005-192455-691-g06-thunk`, 905 s; f9
`20261004-185558-235-f9-frames`, 1203 s). **The title screen is not reached and M15 is not claimed.**

**The present ceiling was one stale method-table entry, and it is cleared** (TR §22). It was the
submission walk rejecting the whole stream with `unsupported_method` on method `0x1810`
(`NV097_DRAW_ARRAYS`), which was missing from the generated admission table (L39) although the
executor already implemented it (`xboxrecomp/src/kernel/nv2a_pb_exec.c`, `case NV097_DRAW_ARRAYS`).
Three independent witnesses named it: the `[PFIFO] reject` line, `g_nv2a_submit_state` in
`just gpu-report`, and the guest's own bytes at GET. **All five archived dumps already logged it as
their first rejection at submit #12**, so it was never a regression. The table was regenerated from a
ring that contains the method (exactly `+0x1810`, nothing removed) and the ceiling is gone: a
no-switch run reaches **presents = 2410** against the old 888 with **zero rejections** and
`last walk ok`, independently reproduced by `20261006-215642-480-title005-confirm`. **M15 is still not
claimed** — the run still ends on the graffiti disclaimer.

**The next blocker is the same class, and then a different one** (TR §22):

- `unsupported_method method=0BB0` (an `NV097_SET_TRANSFORM_CONSTANT` slot) after 3498 successes, with
  the pending region also carrying `0BB4`, `0BB8`, `0BBC`, `1724`, `1728` — six register-state
  methods, confirmed at runtime by `RECOMP_NV2A_ADMIT_UNKNOWN=1`
  (`20261006-213505-255-title005-admit3`). They need regenerating into the table.
- Once those are admitted the same run hits `budget_exhausted` (L40's per-walk word budget), which is
  a **different** class: its own fix (walk bounds / incremental commit), no switch, and never a
  relaxed rejection (L39).
- **Provenance caution.** Those later rings have **wrapped**, so the generator's decode and the
  model's walk can diverge: decoding `20261006-211635-913-title005-m15` from `0x1000` stops with
  `bad_target 0x00100000` and would inflate the table by a dense `0x1848`–`0x18F8` run the real walk
  never required. Take a new entry only from a decode that **reached PUT**, or from an
  `[PFIFO] admit-unknown` line of a run that exercised it.

Standing facts from this turn's measurements:

- The submission walk commits all-or-nothing (L40), so a rejected walk moves no GET, publishes no flip
  and calls no commit consumer — "flips and presents stop together and the 10 s `[GPU]` report never
  resumes" is what a rejection produces. The earlier reading "both stopped, so the guest stopped
  submitting" does not follow.
- **The pre-2026-10-06 fence mirror (L17) removed back-pressure**: it reported every fence complete
  whether or not the walk consumed the commands, so after a rejection D3D kept writing the ring and an
  archived dump's bytes at GET are a *later* frame's — the first `[PFIFO] reject` line names the
  original blocker, and the `RECOMP_FENCE_MIRROR_LIVE=1` A/B (`20261006-205306-080`) shows the walk
  moving past `0x8EF0` to a later blocker. Since toolkit 2026-10-06 the mirror publishes the fence of
  the last consumed kick and a rejected walk is re-tried every 100 ms; a sticky rejection leaves the
  guest in D3D's ring-space wait at `0x1914F0`, as on hardware.
- `GET ≠ PUT` alone does not mean "unknown method": budget, loop, sink, invalid handle, bad target
  and reserved-opcode rejections also pin GET. Branch on the diagnostic.
- The 1000 → 888 change (TR §19) is explained by the same mechanism: it was how far the live mirror's
  accidental rescue got, not a property of the guest's scene.

**The dispatch stop chain (F7) is cleared through stop 28.** Each stop was a missing, swallowed or
mis-sized recovered function found by one run; the class is now mostly found statically.

| Stops | What they were | Record |
|---|---|---|
| 1–16 | unowned gaps, spans that swallowed a later function, spans cut at a folded-alias start, a shared tail epilogue, the alias-folded job handler `0x32610`, `0x80340`/`0x80BD0` | TR §9–§10; `git show dcc93ab:plan-jsrf-bare-minimum.md` "Current work" |
| 17–28 | missing entries (`0x94AB0` class), this-adjusting thunks, under-wide spans (`0xB5EB0`, `0x48DB0`), the `0x96xxx` vtable family | `config/stop-chain.json` (gate `check-stop-chain.py`, L43): 9 runtime-confirmed (17–22, 25–27), 23 and 24 static-only, **28 (`0x81860`) repaired, not yet exercised**; TR §11–§21 |

**Static gates in `just check`:** entry extents, stack depth (TR §10; gates `stack_args = N` at a
depth-0 `ret N`), hidden entries (TR §14), dispatch-table count = array (TR §16), the stop-chain
record (L43). Measured residue (TR §18–§20): stack-depth SUSPICIOUS **67**, span-exit CUT-TARGET
**210**, span-ownership `KNOWN_OPEN` **25**, zero fatal trap-call sites in `recovered.c`.

**Run-to-run variation is first-class.** Same-binary runs take different paths (g03–g06, f25/f26);
a run that does not reach a changed address proves nothing about it (`scripts/check-run-exercised.py`),
and a run without the four title-path switches is not comparable (f8 of 2026-10-06).

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
| **F8** | **cleared 2026-10-06** | the present ceiling: `unsupported_method 0x1810`, one stale table entry; the table now carries it and a no-switch run reaches presents 2410 with zero rejections (TR §22) |
| **F8b** | **next** | the same class again — `unsupported_method 0x0BB0` plus `0BB4`/`0BB8`/`0BBC`/`1724`/`1728` — then `budget_exhausted`, which needs walk bounds (TR §22) |
| F6 = M15 | open | a frame dump of the title screen plus the run record with its ledger IDs; its hash is neither disclaimer hash; compared by eye with an xemu screenshot of the same screen (T3); reproduced by the Turn Reviewer |

## Next actions, in order

1. **Admit the six measured methods, from sound provenance.** `0x0BB0`, `0x0BB4`, `0x0BB8`, `0x0BBC`,
   `0x1724`, `0x1728` are confirmed at runtime by `[PFIFO] admit-unknown` in
   `20261006-213505-255-title005-admit3`. Regenerate the table through
   `scripts/gen-nv2a-method-inventory.py`, taking each entry only from a decode that **reached PUT**
   or from an `admit-unknown` line: the later rings have **wrapped**, and including
   `20261006-211635-913-title005-m15`'s full decode stops at `bad_target 0x00100000` and invents a
   dense `0x1848`–`0x18F8` run the real walk never required (TR §22). They are register-state methods,
   so capture is the implementation; the executor's action handling is unaffected.
2. **Then fix `budget_exhausted`** (L40's per-walk word budget), which the same run hits once the six
   are admitted. This is a different class: walk bounds or incremental commit, its own measurement,
   **no switch**, and never a relaxed rejection (L39). Measure first whether the guest really sends one
   over-budget submission or the walk is running away on a mis-parse.
3. **Do not spend runs on the dispatch stop chain** until the ceiling path is finished; stop 28
   (`0x81860`) is still **not exercised** by any run this turn (`scripts/check-run-exercised.py`
   reports NOT EXERCISED for every post-fix run), so `config/stop-chain.json` is unchanged.
4. **When a new scene appears, read the present lines before trusting the window.** Until 2026-10-06
   the window was handed `drawn_offset`, which only the software rasteriser's triangles update, so a
   frame of clears or untransformed (vertex-program) batches presented the previous buffer. The present
   choice now falls back to the buffer the frame cleared or targeted, and logs
   `[FBPRESENT] presenting targeted 0x…` the first 8 times it does. Untransformed batches are still not
   rasterised (`batches_untransformed` in the `[GPU]` report), so a targeted frame can show only its
   clear colour: an unchanged or blank picture after the ceiling clears is not proof the guest did not
   advance.
5. **M15** as F6 states, then M16 onward.

## Backlog (not on the critical path until it blocks)

- **Stop-chain residue:** 25 `KNOWN_OPEN` (`tests/test_recovery_span_ownership.py`), 67 SUSPICIOUS,
  `0x96F80` (`UNQUALIFIED`, named proof gap, TR §18), the 10 dead alias shims (a latent hazard, not a
  live defect, TR §18), the alias-shim census's actionable classes (12 `SWALLOWED_FUNCTION`, 59
  outside-owner shims; census commit `0cc6d5d`, counts in `git show dcc93ab:plan-turn-updated-title-004.md`
  "B5"), and the `gap_prologue` root cause (needs a full regeneration; batch the 12 sibling spans, each
  verified by decode before landing).
- **Toolkit review findings (2026-10-06): exercised on Windows this turn.** The four `dc04dc0`
  behaviours were judged: the **fence mirror** holds back-pressure as designed (a rejected walk leaves
  GET pinned and the guest in the ring-space wait, with a `[PFIFO] reject` line present) and the
  `RECOMP_FENCE_MIRROR_LIVE=1` A/B reproduces the old behaviour, so the mirror is sound on Windows —
  the 1000/888 count was a property of the old mirror, not a regression (TR §22). CTest
  `kernel_file_apc_test` (its `STATUS_USER_APC` assertions), `kmem`, `fence_snapshot`,
  `nv2a_present_track`, `nv2a_submit_diag` all pass; `just test` 44/44 and `just check` clean. Still
  open: the `[APUWAIT]` line cap, and **stop 28 (`0x81860`) is still not exercised** by any run.
  **Fence-ordering audit done (static, read-only):** `0x1912A0` has exactly 5 direct callers (two
  independent derivations, and the address occurs as a raw dword 0 times, so no indirect site exists).
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
| **15** | **Title screen** | 60 consecutive frames vs xemu ref (SSIM ≥ 0.90); intro FMV handling stated | **open — F8, then F6** |
| 16 | Controller input | with `RECOMP_USB`, a host pad press produces the guest's own XID report and the title's input state changes; `RECOMP_PAD_PRESS` is not admissible | open (toolkit-provided: verify, not build) |
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
- A run that does not reach the changed address is NOT EXERCISED, not a pass.
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
