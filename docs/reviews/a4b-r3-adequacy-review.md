# A4b-r3 adequacy review — INADEQUATE (§5.5 redesign trigger met)

**Packet:** `docs/packets/a4b-gp-dsp-engine.md`, revision `A4b-r3`, class **change**
**Revision reviewed:** SHA-256 `682B67536707F41D5021EB9D4A7A5F9DF369A7B25DE27A5BA26E24C927708EEC`
(269 lines; verified by the reviewer with `Get-FileHash`; committed at game `6e6ac24`)
**Reviewer:** Planner child `9fdbbdc0-07d8-42bb-a03b-db7176dd72fe`, route
`claude` / `claude-opus-5-5` @ `high` — a fresh Planner that wrote none of the revisions
**Verdict:** **INADEQUATE** — two blocking defects. `PREMISE_FRESHNESS: PASS`.

## Reviewer's block (verbatim)

```text
REVISION:          A4b-r3, SHA-256 682B67536707F41D5021EB9D4A7A5F9DF369A7B25DE27A5BA26E24C927708EEC (Get-FileHash, matches; 269 lines)
READ:              whole packet; a4b-r2-adequacy-review.md; a4b-r2-session-checks.md; agent-workflow.md §3.1, §5.5, §5.6; git log -3 (HEAD 6e6ac24 = r3); recomp_0005.c 6746-6751;
                   XBE (read with inspect-jsrf.py): disasm 1A4E00-1A4E7F, 1A34D0-1A356F, 1A35AA-1A35DE, 1A3700-1A374E, 1A2A70-1A2AB9, 1A38C5-1A395C, 1A3630-1A3689, 1A6100-1A6142, 1A1B7C-1A1BAE, 1A5100-1A5136, 19E90D-19E982, 19EE50-19EE9B, 19EFB0-19EFD3, 19F0D4-19F0ED, 19F144-19F15D, 1A0F09-1A0F2A;
                   data 1C40E8 (800000AD/AF), 1C4060 (80000126 = ord 294);
                   find for each of the 13 containing functions of the edx sites, and for 19E90D/19E962/19EE50/19EF88/19F0D4/19F144/1A3630;
                   direct-caller counts from the generated tree; toolkit kernel_thunks.c cases 173 and 294, kernel.h prototypes.
                   NOT read: the r1 attempt, Q1 ruling, planning rulings (I relied on the r2 reviewer's reading), the pin record, toolkit APU source.
PREMISE_FRESHNESS: PASS. recomp_0005.c 6746/6748/6751 still match what the packet cites (observed). R1/R0 are unchanged since the r2 review, which verified them. G1-G4 and the pre-check fail closed.
BLOCKING:
  B1 (AC-PIO exit-trace rule, lines 195-215; third blocking defect in this mechanism).
     Scenario, observed: at site 0x1A38F4 (r = edx) in sub_001A38C5, the exit path runs xor eax,eax / test ecx,ecx / jbe 1A3950.
       - It then runs lea ecx,[ebp-8] / call 1A1BAF. The KfLowerIrql import reads cl, and under the clobber clause edx stays in T.
       - It continues xor eax,eax / pop esi / leave / ret with T = {edx}, which flows into caller sub_0019E90D at 19E926 (ret 8).
       - That returns into sub_0019EE50 at 19EE86-19EE9B. The call [0x1C4060] there is ordinal 294, RtlLeaveCriticalSection (stdcall; its argument is the pushed 0x1BA050), so edx is not used and stays in T. Then ret 8.
       - That returns into sub_0019F0D4 at 19F0ED, which is a 4th caller level, so the site is UNKNOWN.
     The sibling path 19E962 → 19EF88 → 19F144 ends the same way. Nothing on either path overwrites edx. None of these entries appears in the XBE as data (0 hits each), so the rule really does walk them.
     A second case: site 0x1A34EA (edx). Its jbe 1A3562 path returns with edx live into sub_001A3630 at 1A363E. If [esi+0x70] and [esi+0x74] are both 0, the path reaches `call [ecx+8]` at 1A366A (a vtable call) with T = {edx}, which the indirect-call clause makes UNKNOWN.
     Result: AC-PIO is UNKNOWN, so every execution selects R-UNKNOWN, after the GPL port and both runs. This is a deterministic false UNKNOWN, because edx at those points is dead scratch under the ABI.
     Cause: the rule never kills a volatile register, since "eax/ecx/edx not assumed overwritten" by an import and "ret with any register in T flows into every caller". Together these carry dead scratch registers upward until the 3-level bound fires.
     What holds: the rule is sound. It has no false-PASS path, and exceeding the bound or reaching an indirect call fails safe to UNKNOWN, so r2 defect B2 is closed. The rule is not complete.
     Required outcome: a sound liveness convention at ret and call boundaries (for example, when a volatile register is dead at a function boundary), or a change to what AC-PIO measures, decided through the §5.5 path. Before the next review, the author must apply it to all 28 exits and show that each one terminates.
  B2 (step 6 watch-line cap, line 52, combined with the AC-CLEAR deciding-line rule, lines 149-155).
     `[GPDMA] watch` lines are capped at the first 16 occurrences, counting every covering write, including writes with a non-zero payload.
     Scenario: the GP DMA-writes its status block over B+0x810 16 or more times with a non-zero payload (for example, writing back 3 each frame) before its zeroing write. The cas=ok write happens but is never logged. W = 0 at freeze gives "no deciding line and W = 0", which selects R-CPU: a false FAIL, misattributed as Q1 reversal (c).
     Required outcome: always log the first payload=0 watch line (cap only the non-zero payloads, or keep a separate counter), or route "no deciding line while the cap was exhausted" to UNKNOWN.
DEFERRED:
  D1. AC-RUN has no outcome when frames ran and insns>0 but tone=1 on every line. Add "otherwise UNKNOWN".
  D2. `se_frame_after_boot` is printed as a separate line, so the PASS conjunction "some line has se_frame_after_boot ≥ 1, insns > 0" cannot hold on one line. Pair the two explicitly.
  D3. The exit-trace rule has no visited-state or fixpoint rule for back-edges on a path (the inner loops at 1A3500 and 1A390A). Termination is implied but not stated.
  D4. r2 advisory D2 is still open: AC-NOCPU's UNKNOWN text conflicts with the R-UNKNOWN row. It fails closed.
  D5. The import clause says "r" where it means "any member of T".
DECISIONS:
  - r2 B1 import clause: repaired. It is decidable, resolves by ordinal at the exact slot (0x1C4004 = 0x800000A1 = KfLowerIrql, fastcall on cl, observed by the Session), and the 0x1C3FF8 decoy is excluded. Would reverse on: a thunk slot outside kernel_thunks.c.
  - r2 B2: closed for false PASS. ret or an out-of-function jmp with T live is traced or marked UNKNOWN, and the 3-level bound fails to UNKNOWN. The defect has moved to completeness (new B1).
  - r2 B3: repaired, apart from the logging cap (new B2). The first-line rule is exhaustive: cas=ok implies observed=3; cas=fail with observed=0 selects R-CPU; any other value is UNKNOWN; no line splits on W. PASS and R-CPU cannot both hold.
  - r2 B4: repaired. R-NOEXEC exists. I checked the 16 cells and the AC-BOOT FAIL line against the first-match row order and found no cell matching zero or two rows (inferred from the row text).
  - r2 D1 (the 001A3FDC sentence): fixed.
  - I sampled 13 edx sites plus 1A4E4C, 1A611C, 1A35AA and 1A3710. The eax/ecx sites I sampled end with T cleared by overwrite (for example `movzx ecx`, and the caller of 1A4E12 at 1A5119 overwrites eax). Only edx scratch registers escape.
  - B1 is left to the Advisor rather than given a patch, because this is the third blocking round on AC-PIO's exit/callee handling (§5.5).
VERDICT:           INADEQUATE
```

## §5.5 trigger is met — this goes to the Advisor, not to a fourth patch

`A4b-r2` and `A4b-r3` are **consecutive `INADEQUATE` verdicts with blocking defects in the
same mechanism**: `AC-PIO`'s exit/callee handling. (The r1 attempt was `pending — reviewer
unavailable`, not a verdict, so it does not count.) §5.5: *"When two consecutive INADEQUATE
verdicts have blocking defects in the same criterion or mechanism, the method is probably the
wrong shape. The Planner must then redesign that criterion (change what it measures, split
it, or delete it) or take the methodology to the Advisor. A third patch of the same shape is
not allowed."*

The reviewer itself reached that conclusion and named two options: a soundness-preserving
liveness convention for dead volatile registers at `ret`/`call` boundaries, or **changing
what `AC-PIO` measures**. The Session is taking it to the Advisor.

**What is genuinely repaired** (the reviewer's own decision lines): the r2 import clause, the
r2 `ret`-with-`T`-live false PASS, the r2 `AC-CLEAR` ordering, the r2 missing row, and the
`001A3FDC` sentence. The rule is **sound** — no false-PASS path — but **not complete**.

**B2 is a separate, cheap fix** in step 6 (the watch-line cap) and can go into the same
revision; it is not part of the redesign question.

## Reviewer's stated coverage gaps

It did not trace all 28 exits; it sampled 13 `edx` sites plus `1A4E4C`, `1A611C`, `1A35AA`,
`1A3710`. `0x1A4E4C` is clean (`eax` overwritten in its only caller at `1A5119`). It did not
check whether `edx`/`ecx` escape at the `ebx`/`esi` sites or the remaining `A1` sites, and it
relied on the r2 reviewer for the Q1 and planning rulings.

## Next step

**SUPERSEDED — see `docs/reviews/a4b-pio-methodology-ruling.md`.** The §5.5 trigger recorded
here was taken to the Advisor, which ruled that `AC-PIO` changes both its **shape** and its
**home**: the r3 exit-trace rule is sound but asks for a whole-program dataflow proof, so it
is replaced by a locally-checked calling-convention rule (C1–C4) and moves into a new
**discovery packet `A4p`** that runs before `A4b` is promoted. `A4b` then deletes `AC-PIO`
and `R-PIO-DATA` and cites `A4p`'s accepted `O-GATE` as a precondition. **`A4b-r3` is not
patched a third time.**

`A4b-r3` is **not** frozen and **not** promoted; `CURRENT PACKET` remains `none`.
