# `A4b2-r6` execution — Advisor ruling on routing and `AC-CLEAR`'s evidence binding

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Advisor:** persistent handle **`muse_FkNhGaXtV9P5`**, model **`muse-spark-1.3-contributor`**,
**`reasoningEffort: max`** (reported by the turn; turn `01a0e3fd-b692-7000-908f-eb2f043c8656`).
**Consulted under:** §2.3, §4.1 `Session -> Advisor`, §5.4 (frozen-packet revision after `ADEQUATE`).
**Context:** `A4b2-r6` executed and selected **`R2-UNKNOWN`** because gate **G2** failed for R1. Evidence:
`docs/reviews/a4b2-execution-evidence.md`.

Recorded **verbatim** as §3.3 and §4.4 require.

---

## Ruling (verbatim)

```text
RULING:

Q1 — Routing: R2-UNKNOWN → Planner STANDS. It is not a stop. Packet:60 ("a failed
gate → R2-UNKNOWN after the one R1 rerun, not PASS") plus packet:119 ("any gate
fails") matches exactly; the one rerun was used and stands; no Readiness Stop-if
(packet:32) matches and P1/P2/P3 all pass. Cause attribution does not change row
selection — rows select on evidence state, first match wins — and inventing a new
stop reason post-hoc would reinterpret the frozen contract (§2.2). R2-UNKNOWN is
precisely the row for "blocked by something outside this packet": it carries the
failing gate as the brief to the Planner. The Session executed faithfully,
claimed nothing, and r6's UNKNOWN is terminal for r6: failure stays failure, no
rescoring, no PASS survives from r6 artifacts.

Q2 — AC-CLEAR's dump binding: YES, a §5.4(1) defect — a blocking finding from
execution with a concrete scenario (false UNKNOWN). NOT §5.4(2): no load-bearing
premise was invalidated; G2 fired exactly as designed, and displacement was a
known condition since 2026-09-22. The finding: the PASS path binds deciding
address B to the post-mortem dump although (a) the dump is displaced by a known
guest defect the no-edit control reproduces; (b) the same address is carried by
lossless write-once log records (validated sge0_va, anchor va); (c) B is the ONLY
deciding dump read on the PASS path (Wf/F are corroboration-only; F comes from
stacks.txt, unaffected); (d) the decided event predates the corruption. Literal
execution therefore yields UNKNOWN on evidence that decides the claim — a false
UNKNOWN under §3.1, concretely realized, not hypothetical. G2 itself is sound (it
correctly flagged a corrupt dump); per the A2h standing rule its purpose is to
qualify dump-based claims — and the repair removes its protectee.
Mandated repair shape (Planner writes the predicates):
  1. B rebound to validated log evidence (e.g. sge0_va from the every-instance-
     validated [GPBOOT] block(s), cross-checked against anchor va − 0x810;
     multi-boot sge0_va disagreement → UNKNOWN). AC-BOOT's sge0_va=B conjunct
     must be restructured with it (else tautological).
  2. No PASS-path dependence on dump integrity: G2 as a PASS-gate goes with it
     (delete or demote, Planner's choice, one-line reason recorded).
  3. Displaced-dump Wf (reads 0) must not be recorded as meaningful
     corroboration — qualify by mapping integrity or drop it.
  4. r6 run artifacts are STALE for any revision (§2.2.5, §2.4.7): the revision
     re-executes (new R1/R0) under the new contract; r6 logs are leads and
     premise evidence only.

Q3 — Sequencing: the A2h displacement does NOT need fixing before A4b2 can be
decided. Next is a §5.4(1) revision of A4b2 → full fresh §5.3 adequacy review by
a non-authoring Planner → re-execution → decision from log evidence. The
A2h-writer investigation stays OPEN (never closed; A2h-r6's retirement was a
different refuted premise) and remains on the critical path for LATER
progress-past-spin claims — the trapped run dies at ~4.9 s — but it does not
gate this claim. Recommendation, not order: A4b2 revision first (subject already
observed positive, deterministic, cheap), A2h-writer next; the Planner owns the
order.
Boundary policed: the Session's "A4b1 is the only identified difference" must not
become a finding against accepted work. The identical crash signature exists in a
PRE-A4b1 archive (2026-09-22 a2g), which affirmatively contradicts A4b1 sole
causation. A4b1-r4 stays accepted and closed. Why A4a's run went clean is UNKNOWN
and immaterial here.

BASIS:
- Observed: r6 frozen/promoted/ADEQUATE (plan:9-18; evidence:4-6, SHA match);
  G2 R1-fail/R0-pass with G1/G3/G4 pass (evidence:81-86); gate→UNKNOWN rule and
  row text (packet:60,119); Stop-if list with no match (packet:32).
- Observed: control crashes identically without edits, both genuineness proofs
  (evidence:118-142); displacement exactly 0x37608 (evidence:155-159); same
  signature in pre-A4b1 2026-09-22 archive (evidence:144-149).
- Observed: A2h is a guest defect, live-log-settled, writer unknown
  (history:1020-1033); standing rule — .text control before any dump-based claim
  (history:1040-1044); A2h-r6 retired on a different premise (plan:1112-1124).
- Observed: B/Wf dump-defined (packet:61); PASS needs va=B+0x810 (packet:89);
  Wf/F corroboration-only (packet:89,96); per-block sge0_va=B (packet:74,76).
- Observed: GP_CLEAR va=803C0810/observed=3/insns>0/dsp_addr (evidence:103);
  anchor seq immediately below (evidence:102-103,111); rerun identical seq
  (evidence:113-114); image 371/371 with differing control (evidence:100); ack
  absent, both searches + env (evidence:105-106); R1 dump B=FFFFFFFF, Wf=0
  (evidence:191-195).
- Inferred: the decided event predates the corruption (value-correct live reads
  across sge0_va/anchor/CLEAR, deterministic reproduction ×3, control without
  instrumentation reaching the same CLEAR); dump-B's lost protection is weak
  (same pointer read post-mortem — temporal consistency only), while the log
  chain (validated sge0_va + anchor live-match + image equality + G4 + P1 + A4a
  oracle) carries address identity without it.
- Uncertain: why A4a's trapped run went clean (immaterial); the A2h writer
  (open, out of scope for this ruling).

REVERSED BY: evidence that live guest RAM was already displaced at GP_CLEAR time
(impeaches the log values — current values contradict this); a second GP DMA
write path bypassing the latch point (standing predecessor reverser); a concrete
false-PASS scenario that dump-B prevents and the log chain cannot (restores the
dump requirement and forces A2h-first).

RECORD IN: new A4b2 review record, verbatim with Advisor handle/model/
reasoningEffort (§3.3); §5.4(1) ground and repair shape recorded there as a case
ruling. No edits to prior rulings. Session updates CURRENT PACKET per the
Planner's next packet; A2h-writer remains an open lead, cross-referenced, not
newly mandated.
```

## Session correction — the Advisor policed a Session overreach

The Session's evidence record states, under "Boundary of this measurement", that *"The A4b1 GP port is the
only difference identified"* between the clean A4a baseline and the crashing trapped run. **The Advisor
ruled that this must not become a finding against accepted work**, because the identical crash signature
exists in a **pre-`A4b1`** archive (`20260922-224429-003-a2g-304f0-span`, 2026-09-22), which affirmatively
contradicts sole causation by `A4b1`.

**The Session accepts the correction.** Its own evidence record already contains the contradicting
measurement in the very next paragraph (§7's pre-`A4b1` archive row), so the "only difference identified"
sentence was an overreach that its own data refutes. Recorded here so the two statements are not later read
as a claim against `A4b1-r4`, which **remains accepted and closed**. Why the A4a trapped run went clean is
**UNKNOWN** and immaterial to this packet.

## Consequential obligations

1. **`A4b2-r6`'s `R2-UNKNOWN` is terminal for `r6`.** Failure stays failure; no rescoring; no PASS survives
   from `r6` artifacts. `r6` is **not** reopened as a contract — the next revision supersedes it.
2. **`A4b2` is revised under §5.4(1)** — a blocking finding from execution with a concrete false-UNKNOWN
   scenario. **Not** §5.4(2): no load-bearing premise was invalidated.
3. **The mandated repair shape has four parts**, listed in the ruling. The **Planner writes the predicates**;
   the Session does not.
4. **`r6`'s run artifacts are STALE for the revision** (§2.2.5, §2.4.7). The revision re-executes new
   R1/R0 runs under the new contract; the `r6` logs are leads and premise evidence only.
5. **The A2h-writer investigation stays OPEN** and remains on the critical path for later
   progress-past-spin claims — the trapped run dies at ~4.9 s — but it **does not gate** this claim.
6. **`A4b1-r4` stays accepted and closed.** The Session's causation remark is withdrawn per the correction
   above.
