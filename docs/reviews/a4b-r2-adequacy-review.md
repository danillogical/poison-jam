# A4b-r2 adequacy review — INADEQUATE

**Packet:** `docs/packets/a4b-gp-dsp-engine.md`, revision `A4b-r2`, class **change**
**Revision reviewed:** SHA-256 `AB009923B0CF6993FEDE9DABD9072035CFDD49FD8303F2DEB66613EE2F07E7F1`
(verified by the reviewer with `Get-FileHash`; matches the committed revision at game `b429ee8`)
**Reviewer:** Planner child `38e2f0ab-a67a-4fd8-a3ce-9aeb31d0da75`, route
`claude` / `claude-opus-5-5` @ `high` — a fresh Planner that wrote neither revision (§5.1.5)
**Verdict:** **INADEQUATE** — four blocking defects, all in `AC-PIO` and `AC-CLEAR`'s
predicates and the decision rows. `PREMISE_FRESHNESS: PASS`.

## Reviewer's block (verbatim)

```text
REVISION:          A4b-r2, SHA-256 AB009923B0CF6993FEDE9DABD9072035CFDD49FD8303F2DEB66613EE2F07E7F1 (Get-FileHash, matches)
READ:              packet (full, 234 lines); agent-workflow.md §3.1-3.2, §5.3-5.6, §6.1; a4b-q1-advisor-ruling.md (full incl. PREMISE_CHANGED addendum); a4b-planning-rulings.md (full); a4b-r1-adequacy-attempt-1.md (full); XBE via inspect-jsrf.py: `find 0xFE820010` (28 hits, reproduced), disasm of all 28 poll loops + exits, 0x1A1BAF-0x1A1BC5, 0x1A1F70-0x1A1FC7, 0x1A1769-0x1A18D3 (filtered), exit targets 0x1A3562, 0x1A3950, 0x1A4A78, 0x1A41F3, 0x1A3FA9, 0x1A4E39-0x1A4E7C, 0x1A611C-0x1A6142; recomp_0005.c 6521-6527, 6743-6752, 8085-8091; recomp_0000.c 135280-135871 (3 `+0x810 =` stores); src/diagnostics.c includes; CMakeLists.txt:39; R1 jsrf_run.log GPRST/GPSADDR lines (L2759, L3188-3201). NOT read: a4b-xemu-pin.md, a4b-q2-owner-decision.md, A4a packet/evidence, toolkit source.
PREMISE_FRESHNESS: PASS — R1 L3188/3189/3190/3201 observed as cited; recomp_0005.c 6524/6746/6748/6751/8088 observed as cited; G1-G4 and the 6748/6751 pre-check fail closed if the baseline moved.
BLOCKING:
  B1. AC-PIO step 4 (callee rule, import clause). Scenario (observed disasm): at 0x1A611C (the packet's own known-good control), 0x1A4E4C and 0x1A4181 (via jbe→0x1A41F3), r = eax is live at `call 0x1A1BAF`, whose non-je path does `call [0x1C4004]` (import). The clause says an import counts as not reading r only when r ∈ {ebx,esi,edi,ebp}; eax is not, so a literal executor MUST mark these sites UNKNOWN → AC-PIO UNKNOWN → R-UNKNOWN on every execution, after the whole GPL port and runs. That is a deterministic false UNKNOWN (eax is never an argument register under stdcall/fastcall) and it contradicts the criterion's own control. The same happens for edx sites whose jbe exit reaches 0x1A1BAF with edx live (observed: 0x1A34EA→0x1A3562, 0x1A38F4→0x1A3950, 0x1A4A34→0x1A4A78; inferred likely also 0x1A3847, 0x1A39AE, 0x1A3C21, 0x1A4242); for edx, UNKNOWN is only correct if the import's signature cannot be shown. Required outcome: the import clause must be decidable and correct — e.g. an import cannot read r as input when r ∉ {ecx, edx}; for ecx/edx, the executor resolves the import at 0x1C4004 (name/ordinal from the XBE kernel thunk table) and its documented fastcall/stdcall arity, and the site is UNKNOWN only if that cannot be done. Same fix must say what an import does to r afterwards (treat eax/ecx/edx as clobbered by the import, or not), because that decides the next point.
  B2. AC-PIO "not used after the loop" has no rule for `ret` with r live. Scenario (observed): 0x1A4E4C exits → `call 0x1A1BAF` → its `je` path (`pop esi; ret`) leaves eax = PIO_FREE&~3 untouched → loop `jne 0x1A4E39` not taken → `ret 0xC` with eax still holding the polled value, i.e. it becomes the function's return value. The use-definition lists only guest store / MMIO store / branch condition / address computation, so a literal executor ends the path at `ret` and records "not used" — a false-PASS path that is currently masked only by B1's over-strict clause and would open as soon as B1 is fixed. Required outcome: define that reaching `ret`/`jmp` out of the function with r live and not a callee-save pop is a flow into the caller, which must be traced into every caller (or the site is UNKNOWN).
  B3. AC-CLEAR PASS/FAIL predicates are not ordered. Scenario: the GP program rewrites its status block each frame (plausible; AC-FIX (e) itself describes the second write reporting `cas=fail observed=00000000` as normal behaviour). R1 then logs a first watch line `payload=0 cas=ok observed=3` and a later one `payload=0 cas=fail observed=0`. PASS ("at least one line has cas=ok") and FAIL→R-CPU ("a watch line with payload=0, cas=fail, observed=0") both hold; the rows put R-CPU before R-PASS, so the run is classified "the 0 did not come from the GP" and escalated as Q1 reversal (c) — a false FAIL with misattribution. The same applies to UNKNOWN (`observed` neither 0 nor 3 on a later line). Required outcome: decide AC-CLEAR on the FIRST `payload=00000000` watch line for va=B+0x810 by log order (ok+observed=3 → PASS; fail+observed=0 → R-CPU; fail+other → UNKNOWN); later lines are recorded only.
  B4. Decision rows do not cover "frames run, insns = 0 throughout". AC-RUN says that case is FAIL "→ R-NOCLEAR evaluation continues", but R-NOCLEAR requires "AC-BOOT and AC-RUN PASS" and R-NOFRAMES requires the frames failure, so with AC-CLEAR FAIL/W=3 (and every other AC PASS/FAIL-free), no row matches. Plausible: a ported core halted at reset or a halt-requested loop is exactly what AC-RUN guards against. Required outcome: R-NOCLEAR (or a new row) explicitly includes AC-RUN FAIL-with-insns=0 with its next packet; confirm every AC-RUN/AC-CLEAR combination hits exactly one row.
DEFERRED:
  D1. AC-PIO step 1 says "Both candidates decode at hit 001A3FDC"; hit−2 (0x1A3FDA) is 0x65 (disp of `mov [esi+0x65],al`, observed), so only A1 decodes. The label tie-break still yields 0x1A3FDB, so harmless; fix the sentence in the next revision.
  D2. AC-NOCPU UNKNOWN text ("does not override a cas=ok… Advisor decides") conflicts with R-UNKNOWN ("any AC UNKNOWN → R-UNKNOWN"); fail-closed either way. Also "before the first cas=ok" is undefined when no cas=ok exists.
  D3. R-UNKNOWN does not say whether landed/GPL code is reverted or kept (AC-LIC is mandatory "for every row in which the GPL core remains").
  D4. Cross-thread log order ([A4BSTORE] vs [GPDMA]) is only approximately causal; fine as corroboration.
  D5. `cas=ok` proves the GP write made one 3→0 transition; it does not by itself exclude an earlier CPU 0 then a guest re-write of 3 (6746 control would show >1 `value=3` line). Consider requiring exactly one pre-clear 6746 line as corroboration.
DECISIONS:
  - AC-PIO population completeness is fixed: the byte-level `find` count (28, reproduced; VAs match packet), frozen list, value-normalised one-to-one reconciliation, and "any mismatch/undecoded/unlabelled → UNKNOWN, never PASS" make a subset-PASS impossible; non-A1/non-8B-mod00 forms fall to UNKNOWN. Reversed by: a literal-address encoding the byte find cannot see (split/computed), already stated as a limit.
  - The atomic witness is sound in kind (ICX(dest,0,3) success ⇒ the GP write itself changed 3→0 and nothing stored between its read and write); AC-FIX (e) exercises ok and fail/observed=0. Blocking only on predicate ordering (B3). Reversed by: evidence the DMA write path can split the dword.
  - AC-NOCPU is not vacuous: it has a live FAIL (non-control site stores 0 before first cas=ok) and a live UNKNOWN; its 3(iii) half is primary. The 0x1A1FA7 offset-form store (r1 Finding 2) is now instrumented via recomp_0005.c:8088.
  - Write scope covers step 6 (extern in recomp_0000/0005, definition in src/diagnostics.c which is at CMakeLists.txt:39). One-session executability accepted with R-PORT-FAIL/R-LAND as the scale valve.
  - B1+B2 are the same mechanism (callee/exit rule); fix them together, not as two patches, to avoid a §5.5 churn loop on AC-PIO.
VERDICT:           INADEQUATE
```

## What the reviewer confirmed as fixed

The **blocking defect that killed the previous cycle is genuinely repaired**: `AC-PIO` can
no longer pass over a subset. The reviewer reproduced the byte-level `find` count (28), the
frozen VA list, the value-normalised one-to-one reconciliation, and the "any mismatch,
undecoded hit or missing label → `UNKNOWN`, never PASS" rule, and concluded a subset-PASS is
now impossible. It also judged the **atomic witness sound in kind** —
`InterlockedCompareExchange(dest, 0, 3)` succeeding means the GP's own write made the 3→0
transition with nothing stored between its read and its write — with only the predicate
*ordering* defective (B3). And it judged `AC-NOCPU` **not vacuous**.

## Why the four defects matter

All four are false-result paths in the operative contract, which is the §3.1 blocking test:

- **B1 and B2 are a matched pair**, and B1 is the more damaging: its import clause makes a
  literal executor mark `eax`-live sites `UNKNOWN`, including the criterion's *own*
  known-good control, so **every execution would land in `R-UNKNOWN` after the full GPL port
  and both runs** — a deterministic false `UNKNOWN`. Fixing B1 alone would *open* B2's
  false-PASS path, which is why the reviewer required them repaired together (§5.5).
- **B3** is a false FAIL with misattribution: a later watch line would make the run report
  "the 0 did not come from the GP" and escalate as Q1 reversal (c).
- **B4** leaves a reachable state matching no decision row at all.

## Session duties arising

The reviewer named three checks for the Session. Recorded as evidence for the Planner's
repair; the Session does not author the rule:

1. Which import sits at `0x1C4004` (the reviewer infers `cl`-only, possibly `KfLowerIrql`,
   and marks it **not verified**).
2. Whether callers of the function containing `0x1A4E4C` use its return value.
3. Applying the fixed rule once to all 28 exits, so no further site turns `UNKNOWN` and the
   repair is not a third patch of the same shape.

## Next step

Per §5.4 the **authoring Planner** repairs only the blocking defects, and may fix cheap
operative advisories (D1 is a one-sentence correction) without adding narrative about the
repairs. Re-review then covers the blocking defects, the changed regions, and a
contract-completeness check. `A4b-r2` is **not** frozen and **not** promoted; `CURRENT
PACKET` remains `none`.

**§5.5 note:** this is the **first** `INADEQUATE` verdict on `A4b` (the earlier attempt was
`pending — reviewer unavailable`, not a verdict), so the two-consecutive-INADEQUATE
redesign trigger is not met. But B1 and B2 are two blocking defects in the *same mechanism*
(`AC-PIO`'s callee/exit rule), and the reviewer's own decision line requires them fixed as
one redesign rather than two patches. The Session will brief the repair accordingly.
