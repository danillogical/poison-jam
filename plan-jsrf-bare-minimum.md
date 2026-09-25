# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## CURRENT PACKET — none (A4a-r2 ACCEPTED 2026-09-24)

**Next action:** the `A4a-r2` outcome row **`O-6`** selects the next packet — **`A4b`, the
GP DSP56300 engine** — so the Planner designs that against a frozen brief
(`docs/agent-workflow.md` §5.1). Nothing is executable until an adequacy review returns
`ADEQUATE` and that frozen revision is promoted here.

**BLOCKED on an owner decision (workflow §3.4).** Q1 is answered (below), but **Q2 is a
licensing choice reserved to the owner**: whether to port xemu's **GPL-2.0-or-later**
DSP56300 core (`hw/xbox/mcpx/apu/dsp/`) into this **MIT** toolkit (which already carries
**LGPL-2.1-or-later** APU files), or to implement a core independently. That choice
determines A4b's entire shape, so A4b cannot be designed until it is made. The Session
stopped here rather than guessing.

## Last closed packet — `A4a-r2` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4a-dsp-pending-word.md`, revision `A4a-r2`, class
  **discovery**, frozen SHA-256
  `2366E18C583B3ED82124F0E5D3AAD14DB38B4D3CA493F1695E44FFE779722FBC`.
- **Question:** is the next packet the GP DSP56300 engine (`A4b`), and what must its scope
  include? Unknowns U1–U4: the last `GPRST` value; whether the title reads GP/EP at all;
  whether scratch page 0 holds the XBE `0x001BA0A0` image with the pending word `3` at
  `+0x810`; whether the stop holds on the instrumented build.
- **Result — selected outcome row `O-6`** ("the start write is the only GP interaction; the
  image is in place"), from two strict runs on game `5b58d13` / toolkit `0d7929c`:
  - `U1` answered: `last_GPRST = 0x00000003` (`&3 = 3`), and the trace now reports the
    values the guest actually wrote. The value oracle matched the original XBE immediates
    exactly: `write 0x3FF14 = 000000FF` (`0x001A582A`) and `write 0x3FFFC = 00000003`
    (`0x001A585E`), where the pre-change build logged `00000000` for both.
  - `U2` answered: **zero** GP/EP reads, with a coverage witness (the once-per-block read
    note is unbounded by the trace cap and never fired).
  - `U3` answered: `G = 803CC000`, `S0 = MEM32(G) = 803C0000 = B`, and all `0x5CC` bytes
    equal the XBE image at file offset `0x1A7D60`.
  - `U4` answered: both runs `diagnostic_deadline` with `F = 2` on the offset-unpinned
    frame pattern, i.e. the stop holds with and without the trap.
- **Instrumentation:** toolkit `0d7929c` (`src/apu/apu_mmio_hook.c` only, 39+/6−). T1
  reports the value the access moved; T2 names the 400-line cap when reached. Both sit
  inside the existing `RECOMP_APU_TRACE` block, off by default; the R0 leak check is their
  witness (0/0/0). exe `87971604…7ef` → `9597ff7c…c553`; `ctest` 12/12.
- **Claim limits:** a discovery packet claims observations only and never satisfies a
  strict criterion (§5.8). R1 reached the spin only after its `0xFE820010` polls were
  answered by the `PIO_FREE` stub constant `0x80`, so **its arrival at the spin is
  exploratory-grade however R1 classifies** (Advisor ruling B). Nothing here shows the GP
  would clear `+0x810` if it ran — only real GP DSP56300 execution can (ruling D).
- **Records:** adequacy `docs/reviews/a4a-r2-adequacy-review.md` (`ADEQUATE`,
  `BLOCKING: NONE`); execution `docs/reviews/a4a-execution-evidence.md`; acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4a-r2-acceptance-review.md`; revision log
  `docs/reviews/a4a-revision-history.md` (non-authoritative). Superseded `A4a-r1` is
  preserved at game commit `5e739f6`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **Gates `A4b`:** **Q1 — ANSWERED** (Advisor, 2026-09-24): the upstream `PIO_FREE` stub
  dependence does **not** contaminate A4b's strict claim, and a `PIO_FREE` model is neither
  a prerequisite for A4b nor part of it. Case ruling recorded verbatim in
  `docs/reviews/a4b-q1-advisor-ruling.md` (Advisor child
  `407c54a3-6ca4-4a65-835b-faf5355195cd`, route `claude/claude-opus-5-5` @ `high`); its
  general clarification is in `docs/jsrf-run-profiles.md` §"Feature enablement". A4b must
  still establish the ruling's four conditions (who wrote the 0; the GP's input
  provenance; that `PIO_FREE` only gates; the claim limits) and must cite the ruling.
  **Q2 — OPEN, owner decision (§3.4):** licensing of a DSP56300 core. The only known
  implementation, xemu `hw/xbox/mcpx/apu/dsp/` (`dsp.c`, `interp/dsp_cpu.c`), carries
  **GPL-2.0-or-later** headers (verified this session by fetching the sources), while this
  toolkit is **MIT** with **LGPL-2.1-or-later** APU files (`LICENSE`, `NOTICE`,
  `LICENSES/README.md`). GPL-2.0 is stronger copyleft than the LGPL already in the tree, so
  importing it is a licensing choice reserved to the owner. A4b cannot be designed until
  this is decided, because it determines whether A4b ports an existing core or implements
  one independently.
- **`PIO_FREE` model packet (Advisor-directed prerequisite):** required before the **first**
  strict liveness or boot-progress criterion past the spin (e.g. "the title reaches
  <checkpoint> in a strict run"). It is its own packet, placed before that criterion, not
  inside A4b. **Finding for it:** xemu master `hw/xbox/mcpx/apu/vp/vp.c` `vp_read` returns
  `0x80` for `NV1BA0_PIO_FREE` with the comment *"we don't simulate the queue for now,
  pretend to always be empty"* — the obvious secondary source describes itself as a
  pretence, so it cannot corroborate `0x80` as device state, and our `apu_vp.c` is
  xemu-derived and therefore not independent either. The `jsrf-run-profiles.md` reversal
  condition (ii) most likely needs a real FIFO/free-count model or a primary source, not a
  citation.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written
  (`0x3FF00`, `0x3FF04`, `0x3FF10`, `0x3FF14`, `0x3FFFC`×3). Recorded as an acceptance
  advisory on `A4a-r2`.
- `gp_ep_reads` (as defined in `A4a-r2`) counts only `[APUMMIO] read` lines, so a GP
  read-modify-write logged as a write would not register. It did not bite here. Tighten if
  the row is reused.
- Archive raw `ctest` output in run directories; the packet's Closure named it but only
  `CTestCostData.txt` survives.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

**Measured blocker (unchanged):** the guest spins at `loc_001A18D0`
(`recomp_0005.c:6748-6751`, live frame in `sub_001A1769`) on a DSP pending word at
`+0x810` that nothing clears. The run ends `diagnostic_deadline`.
`RECOMP_APU_DSP_ACK` stays forbidden as acceptance evidence. **Caution:** A3a's predicted
next stop (`div@0x001A2BFC`) came from an exploratory run with `RECOMP_GPU_ACK` enabled,
whose synthetic completion cleared this word; it is not a prediction of strict behaviour.

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
