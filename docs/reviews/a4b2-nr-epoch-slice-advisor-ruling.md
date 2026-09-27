# Advisor ruling — §5.5 pattern referral: the `L1` chain, method redesign, and terminality

**Advisor:** Persistent Advisor, `docs/agent-workflow.md` §2.3. **Handle:** `muse_FkNhGaXtV9P5`
(`.muse-workers.md`). **Model:** `muse-spark-1.3-contributor`. **Effort:** `max` (reported by the plugin).

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Trigger:** Session §5.5 referral after **four consecutive** `O-INCONCLUSIVE` rows on the same underlying
criterion (`L1`), each legitimately advancing knowledge but none closing it.
**Basis:** `docs/reviews/a4b2-nr-next-edge-followup-phase1-evidence.md`; `plan-jsrf-bare-minimum.md`
`CURRENT PACKET`; `docs/reviews/a4b2-nr-next-edge-advisor-ruling.md`.

---

## RULING (verbatim)

> All three reads done. No further verification needed — the row mechanics are undisputed and every load-bearing premise I rule on is in these records. This is the methodology ruling the referral asks for.
>
> ## RULING
>
> **Preamble — the pattern, named precisely.** Four `O-INCONCLUSIVE`s, four *distinct* measured blockers (512-window → 2340 PCs → second image → loader/epochs), each round closing its target. The Session's reading is correct and I confirm it. But the SHAPE repeated underneath: forward whole-program enumeration, each round discovering more program. That shape diverges by construction on a growing program — which is exactly what happened. The fix is not another enumeration round and not a weaker bar. It is a different direction of analysis.
>
> ### Q1 — Legitimate chain, not a §5.5 trigger; method redesign + terminal bound now
>
> **Decision: CONTINUE for exactly one packet, under a REDESIGNED method (Q2). No escalation.**
>
> - Not §5.5 in letter (no repeated same-defect `INADEQUATE`s) or spirit (no churn — each round's blocker was new and each target closed). Discovery chaining is the §5.8 lifecycle working, and every round was faithfully executed.
> - But the method must change now, because forward enumeration has a structural defect this sequence exposed: it scales with the program, and the program kept growing. Four rounds is enough evidence of that. The next packet uses the backward method (Q2), and **it is the last L1 packet either way**: `PROVEN` → `A4b2-r8`; `REFUTED` → death branch; stable precise unknown → back to me for a final scope/defer/retire decision, **no auto-chained fifth packet**. That bound is part of this ruling.
> - No escalation: no §3.4 trigger — claim scope unchanged, no fidelity tradeoff, method is Advisor terrain.
>
> ### Q2 — Residual IS closable; the bar as written is CORRECT; the method was wrong
>
> **The bar** ("closure of all feasible named-output/guard reaching definitions against both input classes") **was always interface-shaped. The Sessions kept building forward whole-program CFGs** — the "full feasible CFG / every feasible route" packet language pushed them there. That is the mis-specification: packet language, not bar strength. Do not narrow the claim. Redesign the analysis:
>
> **Backward demand-driven slicing from the named output.** Start from the five doorbell field-definitions + trigger guards; expand ONLY through reaching definitions; classify every frontier leaf as immediate / guest-written / modeled (closes) vs stub-derived (`REFUTED`) vs unresolvable (`INCONCLUSIVE` with the precise unknown). The second image enters ONLY via chains that cross into it. The mixbin path is analyzed ONLY at its interface (X-words/registers it writes that slice nodes read).
>
> **The two concretes, answered directly:**
> - **(b) Callers/guards: answerable in one packet.** Entry = reset vector (measured `first_pc=0000`, to be established incl. interrupt vectors) + second-image entries INTO slice nodes (bounded target enumeration, not whole-program analysis). The ×1 path counts make the image-I portion small; the cross-boundary portion is demand-driven.
> - **(a) Full mixbin-table def-use is NOT required.** Required instead: descriptor-disjointness PROVED alias-closed (not assumed from the preparation note) + doorbell-field reaching-defs + guard enumeration + shared-state (X/registers) reaching-defs. The Session's instinct is confirmed — but "disjoint regions" graduates from finding to load-bearing premise, so it must be proven inside the packet.
>
> **Required next-packet elements:** fail-closed `UNKNOWN` leaves (never silent-benign); **interrupt scoping** (enabled? handlers where? effects on slice state? — the recorded CFG gap, now mandatory); B3 site enumeration over correct bytes with demand-driven def-use (sites all enumerated — cheap; deep analysis only for sites reaching the slice); entry-set proof; zero/fill decodability (for two-version feasibility); **second-image byte provenance IF any chain lands on second-image values** (the one new leaf class — guest-staged firmware admits as guest-written; unresolvable provenance → `UNKNOWN` leaf); executed counts as cross-check ONLY (×1 ≠ feasible-once).
>
> ### Q3 — Name stands, shape replaced
>
> **`A4b2-NR-epoch-slice-followup`** — follow the row's routing, but it is a **backward-slice closure packet** per Q2, not another enumeration attempt. It must establish what this round did not: the reaching-definitions closure computation itself (this round built the universe + B3 walks; the closure is the remaining mechanical work), the cross-boundary interface proof, and interrupt scoping. Terminal outcomes only: closed frontier (`PROVEN`), concrete chain (`REFUTED`), or stable precise unknown (`INCONCLUSIVE` → Advisor, chain ends).
>
> ### Q4 — No to banking now. The criterion is right and the proof is one packet away
>
> **Direct answer: keep going for exactly one packet, redesigned — do not record non-reliance as established yet.** Three reasons: (1) the residual is bounded, mechanical, and small under the backward method — stopping now banks an unproven claim with the proof in reach; (2) "every measurement agrees" is compatible with exactly what L1 exists to exclude (a narrow untested-value branch — L2's honestly-recorded sampling limit); a hedge here would poison every downstream citation, each inheriting the `UNKNOWN` — that serves the playable-game objective worse than one more packet; (3) paired with the Q1 terminality commitment, this is not an open-ended chain. After that packet, I decide finally: accept on proof, scope honestly on a stable unknown, or retire to VP-implementation. No fifth attempt round exists.
>
> **REQUIRE:** next packet = backward-slice closure per Q2 (frontier discipline, fail-closed leaves, interrupt scoping, B3 enumeration, entry-set proof, second-image-value provenance if reached); terminality (either-way ends the L1 chain).
> **FORBID:** forward whole-program enumeration as the slice method; assuming (vs proving) descriptor disjointness; citing ×1 execution as feasibility exclusion; a fifth L1 attempt round.
> **WITHDRAW:** nothing new — prior withdraws stand; the "forward full-program CFG" as L1's shape is superseded by this ruling.
> **CARRY:** L2; image-I stability watch; loader finding; B3 image-I walks; ×1 counts as cross-check data; F1–F3 in force.
>
> **BASIS:** observed — phase-1/2 evidence as cited (loader `P 011C`, monotone zero-rewrite load, image-I zero-write watch, 239/194/0 slice, ×1 doorbell path, B3 walks, closure control, ctest 18/18); plan CURRENT + previous-packet blocks; my prior ruling's Q2-establishment conditions (now satisfied for image I: stability-by-watch ✓, L2 ✓, slice-partial). Inferred — forward enumeration diverges on growing programs (four rounds' shape); backward slicing converges demand-driven from the claim; interface-shaped sufficiency (doorbell event determined by image-I path + shared-state inputs + cross-boundary entries). Uncertain — nothing load-bearing; second-image internals deliberately out of scope (demand-driven only).
>
> **REVERSED BY:** backward slice escaping pervasively into unresolvable second-image edges (bar unreachable as written → I re-rule scope/defer/retire); a stub-derived chain found (→`REFUTED`, contained death branch); a P-write watch bypass discovered (reopens stability, contained by F3).
>
> **RECORD IN:** this ruling verbatim with handle/model/effort in the epoch-slice review record; Q2 method + Q1 terminality bind the next Planner brief; supersedes prior L1 method language (forward-CFG shape) only.

---

## Session obligations arising from this ruling

| # | Obligation | Status |
|---|---|---|
| R1 | **Method redesign**: the next packet is a **backward demand-driven slice from the named output**, not forward whole-program enumeration | **Brief for Planner** |
| R2 | **Frontier discipline**: start from the five doorbell field-definitions + trigger guards; expand ONLY through reaching definitions; classify each leaf **immediate / guest-written / modeled** (closes) vs **stub-derived** (`REFUTED`) vs **unresolvable** (`INCONCLUSIVE` + precise unknown) | **Brief for Planner** |
| R3 | **Descriptor-disjointness must be PROVED alias-closed**, not assumed — it graduates from finding to load-bearing premise | **Brief for Planner** |
| R4 | **Interrupt scoping** now **mandatory**: enabled? handlers where? effects on slice state? | **Brief for Planner** |
| R5 | **Entry-set proof** (reset vector + interrupt vectors + second-image entries INTO slice nodes) | **Brief for Planner** |
| R6 | B3 **site enumeration** over correct bytes; deep def-use only for sites reaching the slice | **Brief for Planner** |
| R7 | **Fail-closed `UNKNOWN` leaves** — never silent-benign | **Brief for Planner** |
| R8 | **Second-image byte provenance** IF any chain lands on second-image values | **Brief for Planner** |
| F-A | **FORBIDDEN**: forward whole-program enumeration as the slice method | **Recorded** |
| F-B | **FORBIDDEN**: assuming rather than proving descriptor disjointness | **Recorded** |
| F-C | **FORBIDDEN**: citing ×1 execution counts as feasibility exclusion | **Recorded** |
| F-D | **FORBIDDEN**: a fifth `L1` attempt round | **Recorded** |
| T1 | **TERMINALITY**: this is the **last `L1` packet either way** — `PROVEN` → `A4b2-r8`; `REFUTED` → death branch; stable precise unknown → **back to the Advisor** for a final scope/defer/retire decision, **no auto-chained fifth packet** | **Binding** |
| W5 | **Withdrawn**: the "forward full-program CFG" as `L1`'s shape | **Recorded** |
| C1 | **Carried**: `L2`; image-`I` stability watch; loader finding; B3 image-`I` walks; ×1 counts as cross-check data; `F1`–`F3` in force | **Recorded** |
| Q4 | **Do NOT bank non-reliance now** — keep going for exactly one redesigned packet | **Recorded** |

**§5.5 referral answered: legitimate chain, not a trigger. Method redesigned. Chain bounded to one more
packet.**
