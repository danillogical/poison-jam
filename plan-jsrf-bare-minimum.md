# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## CURRENT PACKET — `A4a-r2` (discovery, promoted 2026-09-24)

- **Packet:** `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r2`, class
  **discovery**, frozen SHA-256
  `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`.
- **Question:** is the next packet the GP DSP56300 engine (`A4b`), and what must its
  scope include? Unknowns U1–U4: the last `GPRST` value; whether the title reads GP/EP at
  all; whether scratch page 0 holds the XBE `0x001BA0A0` image with the pending word `3`
  at `+0x810`; whether the stop holds on the instrumented build.
- **What it does:** two trace-only additions inside the existing `RECOMP_APU_TRACE` block
  of toolkit `src/apu/apu_mmio_hook.c` (off by default), then two strict runs — `R1`
  (trap + trace) and `R0` (default path).
- **Records:** adequacy `docs/reviews/a4a-r2-adequacy-review.md` (`ADEQUATE`,
  `BLOCKING: NONE`); revision log `docs/reviews/a4a-revision-history.md`
  (non-authoritative). Superseded `A4a-r1` is preserved at game commit `5e739f6`.
- **Claim limits:** a discovery packet establishes observations only. It never satisfies a
  strict criterion and never claims anything works (`docs/agent-workflow.md` §5.8).
  Reaching the spin under trap is **exploratory-grade however R1 classifies**, because the
  guest's `0xFE820010` polls are answered by the `PIO_FREE` stub constant `0x80`
  (`docs/jsrf-run-profiles.md` §"Feature enablement", Advisor ruling B). Only real GP
  DSP56300 execution can clear `+0x810` (ruling D).
- **Outcome rows hand off directly:** `O-3` → `A4c` (scratch upload path), `O-4` → `A4d`
  (GP start path), `O-5`/`O-6` → `A4b` (GP engine brief), `O-1` → Advisor ruling first,
  `O-2` → Session repairs once, `O-UNKNOWN` → rerun once then re-plan. IDs `A4b`/`A4c`/
  `A4d` are placeholders assigned when briefed.

**Measured blocker (unchanged):** the guest spins at `loc_001A18D0`
(`recomp_0005.c:6748-6751`, live frame in `sub_001A1769`) on a DSP pending word at
`+0x810` that nothing clears. The run ends `diagnostic_deadline`.
`RECOMP_APU_DSP_ACK` stays forbidden as acceptance evidence. **Caution:** A3a's predicted
next stop (`div@0x001A2BFC`) came from an exploratory run with `RECOMP_GPU_ACK` enabled,
whose synthetic completion cleared this word; it is not a prediction of strict behaviour.

**Next action:** the Session executes `A4a-r2` exactly, collects the artifacts, selects
the outcome row, and runs two-stage acceptance (§5.8: the reviewer confirms the artifacts
exist, match the commands, and that the recorded row selection is right). The selected
row's next packet then becomes the next frozen brief (§5.1).

**Baseline (2026-09-24).** The accepted A3a work is committed: toolkit `c97ce2c`
(`src/kernel/xbox_memory_layout.c`) and game `a16350f` (`scripts/jsrf_run_profile.py`,
`scripts/ac2-provenance.py`, `tests/test_ac2_provenance.py`). Both trees were clean
after the baseline commits; any later dirty file is new work.

## Last closed packet — `A3a-r25` (ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a3a-ac97-codec-model.md`, revision `A3a-r25`, SHA-256
  `6F907A42EAC6FD02392E11EADEF127AE840D97A245E771CD15B63A387F0E5BDC`.
- **Change:** the environment-gated `RECOMP_AC97_READY` override was replaced by an
  always-on model in the `nv2a_ack_thread` loop:
  `GS(0xFEC00130).bit8 := GC(0xFEC0012C).bit1`, level-evaluated each tick, atomic,
  outside the `g_apu_mmio_trapped` gate; `GC` is read only.
- **Result:** one **strict** run selected `R-PASS`; `AC1`–`AC6` all PASS. The codec
  poll succeeds, the vector-6 ISR is connected, and the guest proceeds into DSP
  initialisation, where it now spins (the blocker above). The strict baseline
  previously self-relaunched before reaching the poll.
- **Claim limits:** one strict run's codec-presence wait was satisfied by modelled
  device state. This does not establish faithful hardware behaviour (secondary
  sources only), audio, a complete DSP handshake, liveness, or strict boot.
- **Records:** adequacy `docs/reviews/a3a-r25-adequacy-review.md`; execution
  `docs/reviews/a3a-execution-evidence.md`; acceptance (`ACCEPT`, no disagreements)
  `docs/reviews/a3a-r25-acceptance-review.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- `P0.1-AC1` is reopened by A3a (see P0 below).
- Move `RECOMP_AC97_READY` into the classifier's `RETIRED_OVERRIDES` registry and
  update `tests/test_run_profiles.py:560`. Its `removed_commit` boundary now exists
  (toolkit `c97ce2c`); the `added_commit` must be found in toolkit history.
- `RECOMP_APU_TRAP` belongs with the DSP packet.
- The AC'97 registers A3a does not model (`0xFEC0017C`, `0xFEC00100`) wait for a
  later packet.

## Retired packet — `A2h-r6` (premise refuted, 2026-09-23)

`docs/packets/a2h-writer-investigation.md` was retired at adequacy review, never
executed. Record: `docs/reviews/a2h-r6-adequacy-review.md`. Its premise — an invalid
target `0x00700010` dispatched from `0x0017DC23` — was refuted: that failure came
through the static-initializer walker's `tail_jump_alias` entry (`0x0017E58F`) and
was already fixed by commit `cb7cae2`; every run that logged it predates the fix.
`sub_0017DBBD`'s execution status is simply uninstrumented (UNKNOWN). Do not revive
this area without new evidence. Two lessons stand for any future packet that must
observe **inside** a generated function: it first needs a trace seam outside the
provenance-guarded files and a trace mode that keeps the **last** N records; and the
collector's call history plus the frozen dump should be tried first, since they
distinguish call sites with no code change.

## P0 — ACCEPTED

**P0.S and P0.1–P0.7 are accepted.** They are completed prerequisites, not active
implementation instructions. Do not reopen them unless a later edit invalidates an
affected accepted criterion; then reopen only that criterion and re-review it.

Durable evidence:

- `docs/reviews/p0-1-execution.md`
- `docs/reviews/p0-1-vblank-adjudication.md`
- `docs/reviews/p0-2-acceptance.md`
- `docs/reviews/p0-3-to-p0-7-acceptance.md`

The original P0 contracts under `docs/packets/` are frozen provenance; their old
`proposed`, `pending`, `next packet` and `TOOLING REQUIRED` wording is not current
work authorization.

### Reopened: `P0.1-AC1` (by `A3a-r25`, 2026-09-24)

Re-review only `P0.1-AC1` against the new runtime.

1. **Cited sites moved.** Its accepted evidence names `xbox_memory_layout.c:684-686,1613`
   (`docs/reviews/p0-1-vblank-adjudication.md:265`,
   `docs/packets/p0-acceptance-contract.md:78`). A3a changed that file, so both ranges
   shifted. The re-review should cite **symbols**, not line numbers.
2. **`P0.1-AC3` is affected in principle.** `_valid_new_archive` compares recorded
   reasons verbatim (`scripts/jsrf_run_profile.py:585-586`) while `CLASSIFIER_VERSION`
   stays `jsrf-run-profile/1`, so editing the reason string at `:391` makes an archive
   recorded under the old string unreclassifiable.
3. **Measured impact today is zero.** Of 650 archived runs, exactly one carries a
   `run_profile`, and it holds no AC97 reason. The classifier still treats the retired
   name as exploratory, which can never produce a false `STRICT`.

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

Older A1–A5 sequences, GPU/audio milestone diaries, historical `next packet`
statements, old model assignments and legacy status tables live in
`docs/jsrf-operating-history.md`. They are provenance only.

New executable work follows the packet lifecycle in `docs/agent-workflow.md` §5: the
Planner designs the packet, an adequacy review of the exact revision returns
`ADEQUATE`, and that frozen revision is promoted into **CURRENT PACKET** in the same
step. Historical evidence may motivate a packet, but a historical failure does not
imply the same failure exists in the target revision.

If no packet is explicitly promoted in this file, implementation is **BLOCKED**.
