# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics;
**`docs/jsrf-technical-record.md` holds the established technical facts** ("TR §n" below).

## CURRENT PACKET — none

The last packet, `A2h-slot-writer-attribution-r2`, is **ACCEPTED** (stage 1, final; row `O-OPEN`):
`sub_00038530` writes `0x001D5078` (a pointer to an ADX filename) into the D3D callback slot
`software_device+0x242C`, K≥2, but no run's terminal matched, so the row is withheld (TR §5).

**Owner-directed toolkit fixes landed on toolkit `main` (`db96e30..2a349c8`) and game `master` (TR §7),
not yet verified on Windows.** They change strict-path behaviour (kernel memory semantics,
kernel data-export thunks) and, at the next regeneration, generated code (flag joins that were always
false, narrow multiply/divide). Before the A2h successor inherits anything, in order:

1. **Windows build of `main`/`master`** (MSVC), `ctest` in both repositories, including the toolkit's new
   `xbox_kmem`, `xbox_guest_meter`, `kernel_data_exports`, `kernel_file_status` and NV2A tests.
2. **Regenerate** with the new lifter (same command and inputs as TR §2) and compare as TR §2 did;
   expect `FLAGS:` ≈ 9 leftover sites and newly live branches in `sub_00015130` and `sub_00130FD0`.
3. **Re-baseline the strict horizon:** one strict run on the new toolkit and one with `RECOMP_KMEM_LEGACY=1`
   (exploratory by presence) as the A/B; read `[KMEM] summary` (the reserve at `0x1495E3`, the commit at
   `0x14961B`), the `data export ordinal` lines, and whether the `[0x1C4064]` stop moves.
4. **Read-only checks** (no packet needed): confirm TR §7's inferred D3D field names against JSRF's
   bytes at `0x0018CE30`, `0x0018CE50`, `0x00193D90`, `0x00194210`; compute `sub_00038530`'s object
   base and whether it overlaps `g_Device` or the page holding `0x001C4064`.
5. **Owner decision:** whether to admit the NV2A action methods on the evidence in the toolkit's
   `docs/technical/nv2a-action-methods.md` (dormant behind `RECOMP_NV2A_ACTIONS`). TR §7 records the
   criteria: the NOP trap is closest; semaphore release fails criterion 4 as written and needs a ruling
   on what "work" means for a state-capture model before it could replace the synthetic fence mirror.
   Any of it needs `0x00193F70` recovered and JSRF's `DEBUG_3` value confirmed (bit 20).

Then the Planner designs the A2h successor the Advisor specified (TR §5, "The specified successor"):
bounded runs, pre-specified N with early stop, for writer-observed **and** terminal-match with controls
green; zero qualifying ⇒ report and re-refer; test the CRT-`memset` lead, never assert it. **Re-refer one
input first:** the specified split of `unknown` into host-identifiable and truly unplaceable would class
`VCRUNTIME140` writes as HOST, but those are most likely guest `rep stos`/`rep movs` lowered to host
calls (TR §7, corrections) — attribute by native return address. `RECOMP_GUEST_METER=1` is available to
observe the unobserved final gap. **Baseline:** toolkit `2a349c8` and this `master` once steps 1–3 pass (until then toolkit
`db96e30`, game `6251ccc`); re-derive native RVAs per run.

## Current strict horizon

A strict run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`, log budget 100000) ends at about 5 s with
`[ICALL] invalid target 0x00000000 … return=0014982E` → `0xE0424943`
(`logs/runs/20260928-185612-449-regen-v012-strict`). The ~571 MB allocation failure before it is
handled by the guest and is not the cause; `[0x1C4064]` holds its installed thunk at every sampled bridge
boundary until the terminal read of `0` (TR §5).

## Open items (not authorized until promoted in a packet)

- **D3D resource release changed with the regeneration:** the new build never enters `sub_00192830`;
  the old build freed 54 contiguous buffers through it. Which is faithful is not established (TR §2).
- **`P0.1-AC1` is reopened** by A3a: its accepted evidence cites moved line numbers in
  `xbox_memory_layout.c`; re-review against symbols. `P0.1-AC3` is affected in principle only.
- **`PIO_FREE` deferred at `O-OPEN`,** with named reopen conditions (TR §4).
- **A4b2's P4 transfer bridge** must be re-established before anything inherits P4, since the toolkit
  advanced (TR §4).
- **Classifier follow-up:** move `RECOMP_AC97_READY` into `RETIRED_OVERRIDES` (`removed_commit` is
  toolkit `c97ce2c`; find the `added_commit`) and update `tests/test_run_profiles.py`.
- **GP port follow-ups:** `NDEBUG`-elided asserts in `src/apu/dsp/` that change GP state or inputs; EP
  routing (TR §4).
- **AC'97 registers not modelled:** `0xFEC0017C`, `0xFEC00100`.
- **Game implicit declarations** (TR §7): declare `recomp_dispatch_init`, `recomp_delta_allowed`,
  `dr_tid_exited` and the two test stubs before the game's CMake adopts `/we4013` as the toolkit has.
- **Kernel memory open points** (toolkit `1d85934`): partial `MEM_RELEASE` refused, `NtQueryVirtualMemory`
  ignores the region registry, the reserve clamp kept; KeSystemTime/KeInterruptTime are set once and
  not advanced (toolkit `4b4a62d`) — a new clock model would need admission.
- **Review capture:** `scripts/record-review.py` reads DSH child-session logs (`--child`,
  `session.v3.jsonl.zstd`) but defaults `--requested-model` to the retired Hy4 route. The reviewer is
  a DSH child again (GPT-6 Sol, workflow §1), so the fix is that default plus a capture test on a
  Sol child's log before the next acceptance review.
- **DSP provenance record:** `xboxrecomp/src/apu/dsp/PROVENANCE.md` lists the A4b1 modifications only;
  the A4b2-NR instrumentation in `interp/dsp_cpu.c` (marked `A4b2-NR`, plus `a9188d9`'s forward
  declaration) is not in its table.

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
| Toolkit sync to v0.12.0+ | done (owner) | merge `2925f0b`, stop unchanged | §1 |
| CRT 64-bit divide helpers | done (owner) | hand-written, unit-tested | §2 |
| Regeneration with v0.12 lifter | done (owner) | same stop; D3D release difference open | §2 |
| Fork audit + owner-directed toolkit fixes | committed to `main`/`master`, unverified on Windows | TR §7 table | §7 |

`A2h-r6` was retired (premise refuted; the failure was already fixed by `cb7cae2`).

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
