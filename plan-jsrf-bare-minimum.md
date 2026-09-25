# Jet Set Radio Future: Windows port milestone plan

This file is the **execution authority**. It owns the current packet, current
blocker, packet status, next action, and milestone acceptance. Do not discover work
by scanning historical documents or old status tables. `docs/agent-workflow.md` owns
roles, the packet lifecycle and escalation; `AGENTS.md` owns operating/build/runtime
discipline; `docs/jsrf-run-profiles.md` owns evidence-profile semantics.

## CURRENT PACKET — none (`A4p-r1` ACCEPTED 2026-09-24)

**Next action — TOOLKIT SYNC, by owner instruction (2026-09-24).** Before any `A4b` code is
written, the next packet is a **change packet that syncs the toolkit with upstream**:

- merge `origin/main` (**v0.11.0**, `766ecef`) into local `main`, resolving conflicts;
- rebuild **without regenerating `src/recomp/gen`**;
- run both repositories' tests;
- rerun **one strict baseline** to show whether the stop is still the DSP pending-word spin;
- **do not push anything to origin.**

The Planner designs it. **If the strict stop moves, that is the next brief.** Verified facts
and the conflict surface: `docs/reviews/toolkit-sync-instruction.md`.

**Why the ordering matters (Session-observed, not in the instruction):** upstream also
changed **`src/apu/apu_dsp.c`** (+56 lines, 148→199) and **`src/apu/CMakeLists.txt`** — the
exact files `A4b1` modifies. So the sync is a genuine prerequisite: `A4b1`'s baseline
(`9597ff7c…`) and its `src/apu` starting state are both invalidated by the merge. The 24
local commits include `c97ce2c` (`A3a-r25`'s accepted AC'97 change) and `0d7929c` (`A4a-r2`'s
trace fix), so the merge must preserve accepted work.

**Then:** `A4b1`/`A4b2` (drafted, split by a fresh Planner from `A4b`) resume with
re-established baselines. Their in-flight adequacy verdicts remain useful as design
feedback. `A4b1` = port, licence, fixtures, unchanged default path, no guest-run claim,
toolkit-only writes; `A4b2` = the strict trap+trace run (boot, run, clear, no-CPU, inputs),
game-only writes, with preconditions P1 (`A4p-r1` ACCEPTED `O-GATE`) and P2 (`A4b1`
ACCEPTED).

## Draft packets — `A4b1`/`A4b2` (split of `A4b`; awaiting the sync)

- **`A4b1`:** `docs/packets/a4b1-gp-core-port.md`, `A4b1-r1`, SHA-256
  `90FA44103FAD4077411E9137775C2934E755E392CE6E33B6AC2399F7F4BE7233`. Claim: the pinned xemu
  GP core, GP MMIO routing and address-translated GP DMA are in tree with per-file
  provenance; synthetic ack removed; licence recorded; build+ctest green including a fixture;
  one strict default run matches A4a R0. No guest GP-behaviour claim; toolkit-only writes.
- **`A4b2`:** `docs/packets/a4b2-gp-clears-pending-word.md`, `A4b2-r1`, SHA-256
  `BD3718E639A6484216F89E245D185F2245CC5231788953F77B735C6EC528CBC1`. Claim: in one strict
  run (`RECOMP_GPU_ACK=0`, `RECOMP_APU_TRAP=1`) the `loc_001A18D0` wait was satisfied by
  modelled GP execution of the guest's own command. Preconditions P1/P2. Game-only writes.
- **Split decision (Planner, one line):** split, because the port's scale risk is settled by
  build+ctest+one default run and should not wait for, or be reviewed with, the run criteria.
- **`AC-PIO` and `R-PIO-DATA` are deleted** from both — `A4p`'s `O-GATE` discharged Q1
  condition 3, so the criterion is retired rather than repaired.
- The former `docs/packets/a4b-gp-dsp-engine.md` (`A4b-r3`) is **superseded** by this split
  and retained as provenance only.

## Last closed packet — `A4p-r1` (discovery, ACCEPTED 2026-09-24)

- **Packet:** `docs/packets/a4p-pio-gate-analysis.md`, revision `A4p-r1`, class
  **discovery**, frozen SHA-256
  `B8BDBFAEC31213AA43D7A2BC8771326A73BE6CC124B6E2993BD2D33C57687A4B`.
- **Question:** are the 28 direct reads of `0xFE820010` in `DSOUND` gate-only under the
  locally-checked calling-convention rule C1–C4?
- **Result — selected outcome row `O-GATE`:** E0–E2 clean, **all 28 sites PASS**, 0 FAIL,
  0 UNKNOWN, E3 clean. Every loop is a threshold re-poll with no store; on every exit path
  `T` empties before any use, by immediate overwrite, by a callee-save `pop`, or by C1 at a
  call. **C3 was invoked at 0 of 28 sites.** The `0xFE800000` table resolves to voice-list
  registers (`0x2054…0x2074`), not `PIO_FREE`, and the two stray constants are unreferenced.
- **This discharges Q1 condition 3**, and therefore **retires `AC-PIO`**: `PIO_FREE` is
  gate-only, so the stub decides whether the guest reaches a point but not the data it
  writes. `A4b` cites this row as a precondition.
- **Claim limits:** a discovery packet never satisfies a strict criterion and never claims
  anything works (§5.8). The result rests on the **inferred** premise that DSOUND follows
  the standard x86 convention, checked at every boundary the analysis relies on but not
  everywhere; it cannot see a custom `edx`-return convention passed on untouched; it covers
  only the 28 direct reads, not register-indirect or computed access beyond E3, timing, or
  whether `0x80` is the true device value.
- **Records:** adequacy `docs/reviews/a4p-r1-adequacy-review.md` (`ADEQUATE`);
  execution `docs/reviews/a4p-execution-evidence.md` (SHA-256
  `3DF5D833E89863E5E9BD0264CC64154C6FEADBD6FE10FC1806F6323FF20981F9`); acceptance
  (`ACCEPT`, first stage, final) `docs/reviews/a4p-r1-acceptance-review.md`; revision log
  `docs/reviews/a4p-revision-history.md`.

**Follow-ups carried forward (not authorized until promoted in a packet):**

- **`A4b` ordinary repairs** (§5.4, different mechanism from the retired `AC-PIO`): the
  `[GPDMA] watch` cap must exempt the deciding `payload=0` line (Advisor ruling (d)), plus
  r3 deferred D1, D2, D4, D5. D3 is superseded by C4 and is discharged by `A4p`.
- **C3 caller-enumeration text** (in the frozen `A4p-r1`, deferred): it searches
  `sub_<ENTRY>(`, which matches only the definition; any future C3 use must enumerate
  callers by value — XBE `call rel32` to the entry, cross-checked by the normalised target
  literal in `RECOMP_ABI_CALL(0x<ENTRY>u, sub_<ENTRY>)`. Second instance of the
  `AGENTS.md` rule against enumerating by one spelling.
- **`PIO_FREE` model packet:** still required before the **first strict liveness or
  boot-progress criterion past the spin**. Finding for it: xemu's own `vp_read` calls its
  `0x80` a pretence, so the obvious secondary source cannot corroborate it.
- **Scope `A4b` to the whole GP block, not just GPRST:** R1 shows five GP offsets written.
- `gp_ep_reads` counts only `[APUMMIO] read` lines, so a GP read-modify-write logged as a
  write would not register. Tighten if the row is reused.
- Archive raw `ctest` output in run directories.
- `P0.1-AC1` re-review; moving `RECOMP_AC97_READY` into the classifier's
  `RETIRED_OVERRIDES` registry; the unmodelled AC'97 registers `0xFEC0017C` / `0xFEC00100`.
- Owner-reserved, unchanged: the `0xFE820010` "GP sample counter" / `PIO_FREE` naming
  conflict (`docs/jsrf-run-profiles.md:288-292`).

## `A4b` — parked, awaiting `A4p`'s outcome

**`A4b`, the GP DSP56300 engine**, is the packet `A4a-r2`'s row `O-6` selects, and both its
gates are answered (Q1 by the Advisor, Q2 by the owner). It is **not** promoted and has had
three revisions and two `INADEQUATE` verdicts, both on its `AC-PIO` criterion — which is why
the §5.5 redesign moved that criterion into `A4p`. When `A4p` returns `O-GATE`, the next
`A4b` revision deletes `AC-PIO` and `R-PIO-DATA`, adds the `A4p` precondition, and folds in
the ordinary repairs (different mechanism, §5.4): the `[GPDMA] watch` cap must exempt the
deciding `payload=0` line, plus r3 deferred D1, D2, D4 and D5. D3 is superseded by C4 and
lives in `A4p`. Its other criteria are otherwise unaffected.

**Records:** `docs/reviews/a4b-r3-adequacy-review.md` (second `INADEQUATE`, verbatim);
`docs/reviews/a4b-r2-adequacy-review.md` (first `INADEQUATE`); `docs/reviews/a4b-r2-session-checks.md`
(the import at `0x1C4004` = kernel ordinal 161 `KfLowerIrql`); `docs/reviews/a4b-r1-adequacy-attempt-1.md`
(`pending — reviewer unavailable`); `docs/reviews/a4b-q1-advisor-ruling.md` (Q1 plus the
PREMISE_CHANGED addendum and the condition-3 handoff to `A4p`); `docs/reviews/a4b-planning-rulings.md`
(checkpoint-40 plus the 3(ii) amendment); `docs/reviews/a4b-xemu-pin.md` (xemu pin
`67cc79e663038d1f55448c0f566b37dde016adf6`); `docs/reviews/a4b-q2-owner-decision.md`;
`docs/reviews/a4b-revision-history.md` (non-authoritative).

**General rules recorded** (Advisor-directed): (`docs/agent-workflow.md` §5.5) a criterion
whose evaluation is itself a multi-step static or dynamic analysis the Planner cannot
complete by reading belongs in a discovery packet that runs first, and the change packet
cites its accepted outcome as a precondition; (§6.1) a criterion claiming something about
*every* access to an address must derive its population from the original XBE with operands
normalised to `uint32`, reconcile by normalised **value** rather than spelling, freeze the
count and generating command, condition PASS on `count == frozen count`, and state what the
method cannot see. The lifter spelling fact is operating knowledge in `AGENTS.md` under
"Generated-source rules".

**General rule recorded** (Advisor-directed, `docs/agent-workflow.md` §6.1): a criterion
claiming something about *every* access to an address must derive its population from the
original XBE with operands normalised to `uint32`, reconcile against the generated code by
normalised **value** rather than spelling, freeze the count and generating command, condition
PASS on `count == frozen count`, and state what the method cannot see. A text search is a
lead, never a completeness witness. The lifter spelling fact (an `A1` moffs load → hex; a
ModRM `disp32` → signed decimal; every address ≥ `0x80000000` exposed) is now operating
knowledge in `AGENTS.md` under "Generated-source rules".

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
  **Q2 — ANSWERED, owner decision (§3.4), 2026-09-24:** the owner accepts
  **GPL-2.0-or-later** for this open-source project, so porting xemu's DSP56300 core is
  permitted. Recorded in `docs/reviews/a4b-q2-owner-decision.md`. The licensing obstacle is
  removed; whether A4b ports the existing core or implements one independently is now a
  **technical** choice for the Planner. **Mechanical consequence for A4b's closure:** a
  binary linking a GPL-2.0-or-later core is a combined work that must ship under
  GPL-2.0-or-later, so closure must update `NOTICE` and the licence files (verbatim GPL text
  alongside the existing LGPL text). That is bookkeeping following from the decision, not a
  further owner question. A4b should also **pin** the xemu commit it ports; the sources
  fetched this session were `master` and are not pinned.
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
