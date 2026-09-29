# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

Condensed 2026-09-28: completed packets are summarised below with one record each; their
full narrative and intermediate records are in git history (last full plan: `e73e495`).

## CURRENT PACKET — none

The last packet, `A2h-slot-writer-attribution-r2`, is **ACCEPTED** (stage 1, final; row `O-OPEN`).
Record: `docs/reviews/a2h-slot-writer-attribution-r2-acceptance-record.md`.

**Next action:** the Planner designs the A2h successor specified by the Advisor in
`docs/reviews/a2h-competitor-finding-row-ruling.md` — bounded runs (pre-specified N, early stop) for
writer-observed **and** terminal-match with controls green; zero qualifying runs ⇒ report and re-refer; the
`unknown` class split into host-identifiable (module-range classified, e.g. `VCRUNTIME140` → `HOST`) and
truly-unplaceable (fail closed); `ledger_mismatch` runs are contrast only; the CRT-`memset` lead is
**tested, never asserted**. **New baseline:** toolkit `2925f0b`, game at or after `e73e495`. The
generated tree was regenerated, so native RVAs and the executable hash in older A2h records describe the
old binary; the successor re-derives them from each run's own map.

## Current strict horizon

A strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, log budget 100000) ends at about 5 s with
`[ICALL] invalid target 0x00000000 … return=0014982E` → `0xE0424943`
(`logs/runs/20260928-185612-449-regen-v012-strict`).

- **The terminal event is a zeroed kernel-thunk slot.** `call dword ptr [0x1C4064]` at `0x00149828` reads
  `0`; the XBE holds `0x80000115` there (the ordinal-277 thunk) and the runtime patches that table.
- **The ~571 MB allocation failure just before it is handled by the guest** (`0x00149E56 test eax,eax` /
  `jl`), not the cause. The producer line that chased its size is **parked**; reopen only if the slot death
  proves downstream of that error handling, or a later gate needs the size explained
  (`docs/reviews/a2h-critical-path-advisor-ruling.md`).
- **Established (K≥2):** the recompiled body of `sub_00038530` writes `0x001D5078` into
  `software_device+0x242C`; it is not the static candidate `0x00199F45`
  (`docs/reviews/a2h-competitor-identity-sub-00038530.md`). `VCRUNTIME140`'s `memset` also writes the
  watched page (`docs/reviews/a2h-unknown-rips-are-crt-memset.md`).
- **Binding discipline for the A2h line:** no hand counts in decision inputs (tool-computed, named
  artifact, positive control, loss accounting); attribute a terminal event by reading the instructions
  past the failing call, never by co-occurrence; carry-forward lists are re-justified per packet; harness
  `winerror=5` failures were disk exhaustion (`docs/reviews/a2h-save-root-failure-was-disk-exhaustion.md`).

## Open items (not authorized until promoted in a packet)

- **D3D resource release changed with the regeneration.** The regenerated build never enters
  `sub_00192830`; the old build freed 54 contiguous buffers through it. Which build is faithful is not
  established. Discovery: trace its ten callers in both builds to the first divergent branch
  (`docs/reviews/regeneration-v012.md`).
- **`P0.1-AC1` is reopened** by A3a: its accepted evidence cites moved line numbers in
  `xbox_memory_layout.c`; re-review against symbols. `P0.1-AC3` is affected in principle only (measured
  impact zero).
- **`PIO_FREE` is deferred at `O-OPEN`** by Advisor ruling (`docs/reviews/pio-free-advisor-scope-defer-ruling.md`):
  the `0x80` stub passes 13 constant gates and is unproven for 15 variable ones. Reopen only on newly
  admitted hardware source evidence, or a strict claim traversing named gates (then demand-driven per gate).
- **Classifier follow-up:** move `RECOMP_AC97_READY` into `RETIRED_OVERRIDES` (its `removed_commit` is
  toolkit `c97ce2c`; the `added_commit` must be found) and update `tests/test_run_profiles.py`.
- **AC'97 registers not modelled:** `0xFEC0017C`, `0xFEC00100`.

## Closed packets

| Packet | Result | Claim (limits in the record) | Record |
|---|---|---|---|
| P0.S, P0.1–P0.7 | ACCEPTED | evidence loop: runner, profiles, dumps, review records, provenance | `docs/reviews/p0-3-to-p0-7-acceptance.md` (and `p0-1-*`, `p0-2-acceptance.md`) |
| `A3a-r25` | ACCEPTED | AC'97 codec-ready modelled as `GS.bit8 := GC.bit1`; one strict codec wait satisfied | `docs/reviews/a3a-r25-acceptance-review.md` |
| `A4a-r2` | ACCEPTED (discovery) | GP start handshake observed; row `O-6` → A4b | `docs/reviews/a4a-r2-acceptance-review.md` |
| `A4p-r1` | ACCEPTED (discovery) | `PIO_FREE` is gate-only at the 28 direct reads (`O-GATE`) | `docs/reviews/a4p-r1-acceptance-review.md` |
| `A4s-r6` | ACCEPTED | toolkit synced to v0.11.0 | `docs/reviews/a4s-r6-acceptance-review-stage1.md` |
| `A4b1-r4` | ACCEPTED | GP DSP56300 core ported (GPL-2.0-or-later, owner-approved), fixtures green | `docs/reviews/a4b1-r4-stage1-acceptance-review.md` |
| `A4b2-r8` | ACCEPTED | the GP engine's own DMA write makes the DSP pending word's `3→0` transition in a strict run | `docs/reviews/a4b2-r8-acceptance-record.md` |
| `A4b2-NR` (discovery) | `O-TWO-LEG` | the two stub inputs do not reach the doorbell; A4b2-r8 depends on it | `docs/reviews/a4b2-nr-epoch-slice-execution-evidence.md` |
| `A2h-slot-writer-attribution-r2` | ACCEPTED, `O-OPEN` | writer found; row withheld (no matching terminal) | see CURRENT PACKET |
| Toolkit sync to v0.12.0+ | done (owner) | merge `2925f0b`, stop unchanged | `docs/reviews/toolkit-sync-v012.md` |
| CRT 64-bit divide helpers | done (owner) | hand-written, unit-tested | `docs/reviews/crt-64bit-divide.md` |
| Regeneration with v0.12 lifter | done (owner) | same stop; D3D release difference open | `docs/reviews/regeneration-v012.md` |

`A2h-r6` was retired (premise refuted; the failure was already fixed by `cb7cae2`).

**Carried constraints from A4b2:** the accepted claim is the GP-engine `3→0` transition under its bounded
contract — not guest observation of the zero, spin exit, or liveness. The non-reliance discovery is a
dependency unless one of its pinned premises changes. **P4 is toolkit-identity-sensitive:** the toolkit has
advanced (`2925f0b`), so the discovery-transfer bridge must be re-established before P4 is inherited.

## Evidence and decision rules

- `MEASURED` means inspected source/artifact evidence with an identity/procedure.
  `INFERRED` means a hypothesis or expected consequence.
- Missing, malformed, stale, unexercised or `CANNOT VERIFY` evidence is never PASS.
- Only a verified **strict** run can support strict integration/boot/liveness/device
  claims. Exploratory/fixture evidence remains bounded to what it actually measured.
- A reviewer verifies or refutes each mandatory criterion.
- A disagreement goes to the Persistent advisor, whose ruling is final within the
  evidence invariants of `docs/agent-workflow.md` §2.4.
- A failed measurement cannot be converted to PASS by Planner, Session, reviewer or
  advisor interpretation.
- A post-review code/evidence change reopens the affected criterion.
- Original assets and existing saves remain unchanged.

## Roadmap policy

Older sequences, milestone diaries and legacy status tables live in
`docs/jsrf-operating-history.md` and git history. They are provenance only.

New executable work follows the packet lifecycle in `docs/agent-workflow.md` §5: the
Planner designs the packet, an adequacy review of the exact revision returns
`ADEQUATE`, and that frozen revision is promoted into **CURRENT PACKET** in the same
step. Historical evidence may motivate a packet, but a historical failure does not
imply the same failure exists in the target revision.

If no packet is explicitly promoted in this file, implementation is **BLOCKED**.
