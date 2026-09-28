# Advisor ruling — the competitor is a **FINDING**, the row stays `O-OPEN`, and the window closes without execution

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e979-6241-7000-85b2-474098d1f738`.
**Raised because:** the competitor was found with K≥2 agreement, the `unknown` rule's premise was refuted, and
the row question needed adjudication before the owner's window closed.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: Session record suffices (no selected row consumes the refuted premise — `O-OPEN` stands on the independent terminal-mismatch ground); competitor recorded as a FINDING with row withheld; successor runs for writer + terminal-match with corrected classifier. No execution in the remaining window.**

**1. Record, not revision.** The `unknown` rule's basis (all page writes are guest) is falsified — **CRT `memset` writes too, three runs running.** But **no selected row depends on it: `O-OPEN` rests independently on terminal mismatch** (writer observed ≠ terminal observed), **so the frozen contract needs no amendment.** What changes is **future row design: split `unknown`-class into host-identifiable (module-range classified, e.g. `VCRUNTIME140` → HOST with module cited) vs truly-unplaceable (fail closed).** **That correction goes in the SUCCESSOR's design under this ruling — frozen contracts aren't edited.**

**2. (a) — and the bar must NOT be relaxed to (b).** **Rows are frozen predicates, not free prose; minting a "scoped row" post hoc violates §2.2 and pollutes the vocabulary future packets inherit.** **Findings ≠ rows: the competitor (`sub_00038530` @ RVA `0x52FE38`, value `0x001D5078`, K≥2 agreeing) is RECORDED at full force; the row stays `O-OPEN`.** **The conjunctive bar (writer + terminal match) is load-bearing, not bureaucratic — Exp2-3b/Exp2-2 prove writer-without-terminal answers "who *can* write," not "who caused *this* death."** **Successor: bounded runs (pre-specified N, early stop) for writer-observed + terminal-match + controls green; zero qualifying → report + re-refer.** **`ledger_mismatch` runs (Exp2-2) contribute contrast only, never decision inputs.** **The CRT-`memset`-zeroes-slot hypothesis is now the testable lead (memset write immediately preceding terminal-zero by tick order) — successor tests it, never asserts it.** **NULL line untouched.**

**Do not launch in the remaining ~35 minutes.** **Bounded runs that cannot complete *with verification* inside the window corrupt N accounting and reviewability — partial evidence is worse than none here.** **The successor is fully specified above and writable next window from this ruling alone.**

**BASIS:** observed — K≥2 agreed pair (same RVA, same value), three-run CRT writes, terminal mismatches, ledger-mismatch catch. Inferred — nothing load-bearing.

**REVERSED BY:** a run with writer-observed + terminal-match + controls green (that's the successor's success case, not a reversal).

**RECORD IN:** verbatim in the line's review record; **Session records the competitor as a named finding with row withheld, plus the `unknown`-class correction for successor design.**

---

## The distinction the Advisor is protecting, and it is the important one

**The Session offered option (b): *"accept a row scoped to what IS observed, with the terminal mismatch
stated as a limit."*** **The Advisor refuses it, and the reason is structural:**

> **"Rows are frozen predicates, not free prose; minting a 'scoped row' post hoc violates §2.2 and pollutes
> the vocabulary future packets inherit."**

**And the bar's purpose is stated:**

> **"The conjunctive bar (writer + terminal match) is load-bearing, not bureaucratic — Exp2-3b/Exp2-2 prove
> writer-without-terminal answers 'who CAN write,' not 'who caused THIS death.'"**

> ## **So the Session's instinct toward (a) was right, and the reason is sharper than the Session gave: the Session wanted (a) because the criteria are conjunctive; the Advisor wants (a) because a post-hoc row REWRITES THE VOCABULARY.**

**⚠ AND THE SESSION RECORDS THAT IT OFFERED (b) AT ALL.** **The Session was one ruling away from minting a row
shape the contract does not contain** — **and it offered it while believing (a) was correct, which is exactly
the kind of "helpful" improvisation that erodes a frozen contract over many packets.** ✓

## Why no amendment is needed — and the correction's proper home

**The Session had framed the `unknown` refutation as possibly requiring a packet revision.** **The Advisor's
answer is that NO SELECTED ROW CONSUMES THE PREMISE:**

> **"`O-OPEN` rests independently on terminal mismatch (writer observed ≠ terminal observed), so the frozen
> contract needs no amendment."**

**And the correction is routed to the successor's DESIGN:**

**split `unknown` into:**
- **host-identifiable** (module-range classified, e.g. **`VCRUNTIME140` → `HOST` with the module cited**);
- **truly-unplaceable** (fail closed).

✓ **That is the right home: a future packet's design, not a past packet's text.**

## The successor is fully specified

| Element | Specification |
|---|---|
| **Goal** | **bounded runs for writer-observed + terminal-match + controls green** |
| **Bound** | **pre-specified N, early stop; zero qualifying ⇒ report + RE-REFER** |
| **Classifier** | **with the `unknown`-class split (host-identifiable vs truly-unplaceable)** |
| **`ledger_mismatch` runs** | **contrast only, NEVER decision inputs** |
| **The CRT lead** | **test it, never assert it** — *"memset write immediately preceding terminal-zero by tick order"* |
| **NULL line** | **untouched** |

## And the window closes without execution — for a stated methodological reason

> **"Bounded runs that cannot complete *with verification* inside the window corrupt N accounting and
> reviewability — partial evidence is worse than none here."**

**⚠ The Session notes this is a rule it would have been tempted to break:** **with ~35 minutes left and a
fully-instrumented, control-green facility, launching one more run is cheap and the terminal might match.**
**The Advisor forbids it because a run that cannot be VERIFIED AND RECORDED inside the window is not evidence
— it is an unaccounted draw against N.** ✓

**And: *"The successor is fully specified above and writable next window from this ruling alone."*** ✓

## The competitor, recorded as a FINDING at full force

**Per the ruling, the Session records it prominently and withholds the row:**

> ## **FINDING: the recompiled body of guest function `sub_00038530` (`0x00038530`) writes `0x001D5078` into `software_device+0x242C`, reproducibly, in two independent runs at native RVA `0x52FE38`, with the identical value, and it is NOT the static candidate `0x00199F45`.**

**Row: `O-OPEN`.** **The conjunctive criteria are not met** — **writer observed ✅, controls green ✅, matching
terminal ❌.**

## The Session's final state

**The Session will record this ruling, update the plan, verify both trees clean, and STOP.** **No further ON
run. No packet edit. No row promotion.**
