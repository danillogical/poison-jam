# Advisor ruling — `A4b2-NR-followup-r1` execution: scope defect, re-derive list, `L2` admissibility, `is_gp` audit

**Advisor:** Persistent Advisor, `docs/agent-workflow.md` §2.3. **Handle:** `muse_FkNhGaXtV9P5`
(`.muse-workers.md`). **Model:** `muse-spark-1.3-contributor`. **Effort:** `max` (plugin default;
the send reported no override).

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Trigger:** Session ruling request after executing `A4b2-NR-followup-r1` and finding that the
predecessor's Leg 1 reasoned over 380 of 2 340 executed PCs (512 of 3 881 words, ~16%).
**Basis for the request:** `docs/reviews/a4b2-nonreliance-discovery-evidence.md` §3, §4, §8;
`plan-jsrf-bare-minimum.md` `CURRENT PACKET`.

**Protocol note.** The first `muse_session_send` timed out at 600 s. Per §4.4 the turn was collected
with `muse_session_read` **before any further send** on the handle; the second read returned the
completed text. No concurrent send was made.

---

## RULING (verbatim)

> All load-bearing premises verified firsthand, including the full `is_gp` reader/writer audit. One correction to your §8 account is needed (see Q4) — it strengthens your conclusion rather than weakening it.
>
> ## RULING
>
> **Row call: `O-INCONCLUSIVE` CONFIRMED.** `O-TWO-LEG` requires `L1=PROVEN` ∧ `L2=INVARIANT`; L2 is satisfied and L1 is not. No strict criterion is discharged; `A4b2-r7` stays `R2-EXPL-INPUT`. The `P 00B9` edge may no longer be cited as open (closed by complete measurement, `events == execs == 6`), and the first B9 run was correctly discarded as non-evidence (it measured the discriminator bug, not the program).
>
> ### Q1 — No contract impact; recording is sufficient; no `PREMISE_CHANGED` anywhere
>
> **Decision:** your position is correct. The scope defect touches neither frozen contract.
>
> - The r2 packet mandated decoding exactly the 371-word image (packet:12) and set `PROVEN` = full slice over every feasible path. It never premised "the program is 512 words" — the window was the Session's analysis choice. The Session claimed neither `PROVEN` nor whole-program coverage and selected the faithful row. A wrong row would be a contract problem; the row was right then and remains right now.
> - Followup-r1 closed gap 1 on its own contract terms (actual-address measurement, which is independent of the gap's original framing) and kept `O-INCONCLUSIVE` per its table.
> - `§5.4(2)` requires an invalidated load-bearing packet premise relied on downstream. Nothing consumed a Leg-1 `PROVEN` — `A4b2-r7` stays `EXPL-INPUT` *because* L1 is open, and the scope defect only strengthens that direction. The evidence record (§8) + plan block recording suffices. No revision, no re-review, no escalation.
>
> ### Q2 — Re-derive list (suspended until done) vs carry list (stands)
>
> **Principle:** local in-window readings STAND; every completeness/negative claim from the 512-word analysis is SUSPENDED. The decoder bytes for `0x000`–`0x1FF` did not change — what died is every "only / never / every / no-other" uttered over a 16% enumeration.
>
> **Carries without re-derivation:** the 512-word decode bytes; the `op =` execution-absence (window-independent — see below); the §8 `P 00B9` measurement table (observed-exchange closure); the six table words' values + bin-alignment arithmetic; the doorbell tuple; literal readings (`P 0004`/`P 000B`, `dor #$0006`); per-instruction semantics within the window; L2 per Q3.
>
> **Must be re-derived over the full 3881-word program before next-edge may rely on it:**
> 1. All `0xFFFFB3` read sites — the "four reads" count is **forbidden** until re-enumerated;
> 2. Full def-use of `x:$007c`–`x:$007f` — "loop-control-only" suspended (upper code may read them);
> 3. All callers of builder `P 00DB` + all reaching definitions of `r0`–`r3` at entry — "sole builder / all-immediate" suspended;
> 4. All DMA-register writes + trigger sequences — "sole trigger" suspended;
> 5. All X-writes to `$0000`–`$0004` — "mailbox never written" suspended;
> 6. All branch/call targets program-wide — the "none targets `00CC`–`00D1`" *feasibility* ground suspended (its *execution* ground stands, next paragraph);
> 7. `r1` reaching-definitions at `P 00B9` including `P 008C` and any upper writers — feasibility closure of the measured edge (the trace answers this run's reads, not all feasible mailbox values).
>
> **Why the table's execution ground stands:** the interpreter decodes on opcache miss during execution (your two crashing runs prove runtime decode calls exist), so every executed PC was decoded; `op =` fired 0× across execution runs against the decode run's 6× positive control — no executed PC fetched an undecodable word, window-independently. The 2340-PC histogram may be relied on only as a cross-check lower bound (executed ⊆ slice-covered); the `PROVEN` bar remains feasibility-based per the packet.
>
> **Forbidden:** citing any 512-word-era "only/never/every" claim; citing `P 00B9` as open; treating trace absence as feasibility proof (the packet's own rule).
>
> ### Q3 — Admissible as corroboration + transfer bridge; not as perturbation evidence; no fresh full series now, conditional later
>
> **Decision:** your position is half-right, and the half that matters needs a boundary.
>
> - The eight runs test **instrumentation/build-invariance**, a different counterfactual than **input-invariance**. They cannot satisfy the adversarial leg. **Forbid** citing them *as* perturbation evidence; **permit** citing them as cross-build transfer + robustness evidence.
> - The r2 four-arm same-exe series STANDS as the L2 core — window-independent, nothing impeaches it.
> - Transfer logic (observed): every build delta since `11D33FC9` is env-gated-inert (absent-gate runs reproduce the tuple bit-identical *including* `insns=124652`; all diagnostic tags 0) or behavior-preserving (NULL guard; opaque fix is B9-gated). The 8-run bridge therefore carries r2's L2 to the next identity. Require only ONE fresh absent-gate baseline at whatever identity next-edge pins (closure control doubles as this).
> - **Condition:** if next-edge commits any toolkit change that executes on the GP path with gates absent, a fresh same-exe mini-series is required at that identity (baseline + one changed-input arm with changed-counters + comparator-validity recheck).
> - `bad-output` removal authorized per followup closure (bite preserved and re-verified).
>
> ### Q4 — No threat to `A4b1-r4`; do not touch it. One correction to your §8 account
>
> **Decision:** A4b1 acceptance unaffected; no `§5.4(2)`; your "will not touch" is confirmed correct. Full audit (observed, complete `is_gp` occurrence list, no truncation):
>
> - **A4b1 paths are clean.** Every live classifier reads `DSPState.is_gp` (`dsp.c:143`, never overwritten: `:203`, `dsp_c.c:111`, `gp_ep.c:364,366`) or `dma.is_gp` (`dsp.c:152`, never overwritten: `dsp_dma.c:331`). No accepted-work path touches either core field.
> - **Correction:** there are TWO core structs, and your "permanently 0" understates the damage while your conclusion holds. `dsp_init` (`dsp.c:144`) *does* set the VM field `dsp->core.is_gp` correctly — but `dsp_c_sync_to_vm` (`dsp_c.c:215`) **overwrites it with the interpreter core's unwritten 0 on every traced GP frame** (`gp_ep.c:651`), while `dsp_c_sync_from_vm` (sole writer of the interpreter field, `:252`) has **zero callers** (verified across `src/`). So in traced runs *both* fields read 0. Contained, because: the VM field has **zero readers** anywhere in `src/`; the interpreter field's only readers are the corrupting copy itself, the compiled-out trace macro (`dsp_cpu.c:1009` → single `((void)0)` define, verified), and your fixed B9 helper. Write-write-never-read plus dead registration: unobservable, behavior-neutral.
> - **Decoder NULL (defect a):** pinned latent; A4b1 never invokes the decoder (compiled out pre-NR); the guard is behavior-preserving for valid opcodes. No impact.
> - **Required guardrails** (Planner places in next-edge scope or a bounded commit): source comments at both field decls (`dsp_cpu.h:49`: never populated, do not read; `dsp.h:105`: clobbered by `sync_to_vm` in traced runs, do not read) + canonical-GP-test note (`DSPState.is_gp` via `opaque`; `dma.is_gp` in DMA paths). **Forbid** future classification by either core field. **Permitted, not required:** deleting the dead `is_gp` copies in both sync functions (zero observable change; Planner's call).
>
> **BASIS:** observed — r2 packet mandate + `PROVEN` bar (packet:12,16-21); §3/§4/§8 contents as cited above; plan CURRENT PACKET (plan:9-74); `dsp.c:139-152,203`, `dsp_c.c:206-256,279-327`, `gp_ep.c:651`, `dsp_cpu.c:1009`, shim `:40`, complete `is_gp`/sync/trace occurrence lists (all searches bounded, none truncated). Inferred — opcache-miss decoding makes `op = 0×` an execution-complete non-execution proof (strong: crash evidence shows runtime decode calls); transfer-bridge validity from gated-inertness measurements. Uncertain — nothing load-bearing (next-identity delta covered by Q3's condition).
>
> **REVERSED BY:** a reader of `dsp->core.is_gp` found anywhere (reopens Q4 containment); evidence `op =` does not fire on all runtime undecodable fetches (reopens table non-execution); a feasible upper-program stub-to-doorbell path (routes to the r2 death branch — contained outcome, not a reversal of this ruling).
>
> **RECORD IN:** this ruling verbatim with handle/model/effort in the followup/next-edge review record; the Q2 re-derive + forbid lists bind the next-edge Planner brief; the Q4 guardrail obligation tracked to placement.

---

## Session obligations arising from this ruling

| # | Obligation | Status |
|---|---|---|
| O1 | `O-INCONCLUSIVE` row call **confirmed** — no change to the recorded row | **No action** (already recorded) |
| O2 | **Q2 re-derive list (7 items)** binds the `A4b2-NR-next-edge` Planner brief | **Pending** — must be written into the brief |
| O3 | **Q2 forbid list** binds the next packet | **Pending** — same |
| O4 | Q3: **one** fresh absent-gate baseline at the next pinned identity | **Pending** (next-edge closure control doubles as it) |
| O5 | Q3 condition: if next-edge changes GP-path behaviour with gates absent → fresh same-exe mini-series | **Conditional** — watch next-edge's diff |
| O6 | Q3: `bad-output` removal **authorized** | **Pending** — next packet |
| O7 | Q4 guardrails: source comments at `dsp_cpu.h:49` and `dsp.h:105` + canonical-GP-test note; **forbid** classification by either core field | **Pending** — Planner places it |

**Session correction accepted.** My §8 account said `core->is_gp` is "permanently 0". The Advisor's
audit shows the accurate picture is worse and differently shaped: `dsp_init` sets the **VM** field
correctly, but `dsp_c_sync_to_vm` overwrites it with the interpreter core's unwritten `0` on every
traced GP frame, so **both** fields read `0` in traced runs. The conclusion (contained,
behaviour-neutral) holds because the VM field has zero readers and the interpreter field's only
readers are the corrupting copy, a compiled-out trace macro, and my fixed helper. **The evidence
record's §8 wording is corrected accordingly rather than left as written.**
