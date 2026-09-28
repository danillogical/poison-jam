# Session practice note — the extraction-script failure mode (five instances, one cause)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the Advisor made **known-answer controls for every extraction** a standing mandate after
calling the `djv000_0.adx` control *"the fourth instance of the pattern."* **The Session then made a fifth
error whose cause is identical to the fourth, which means the mandate as written was not sufficient.**
**This note identifies the actual mechanism so the control can be aimed at it.**

---

## The five instances

| # | What the Session did | What was actually true | Caught by |
|---|---|---|---|
| **1** | Claimed *"at least five of six guest threads never attempted"* | **overran the evidence** | the Session, on review |
| **2** | Read `cleared=17` as *"17 armed"* | **`dr_disarm_all` zeroes every live thread and counts clean readback** | **the Advisor** |
| **3** | Read `inspect-jsrf.py memory` output as memory-order bytes | **the tool prints little-endian DWORD VALUES** — the Session concluded `MEM32(0x19DCE0) = "djv0"`, **which would have collapsed the entire object identity** | **the Session, via a known-string control** |
| **4** | Treated the corroboration **table** as the whole affected claim | **the caller return-address binding at `0x00F7FEA0` sits INSIDE the same tainted window** | **the Advisor** |
| **5** | Published a field-attribution table claiming `edi = ecx` at four of five sites and filing `+0x100` as a CONTEXT offset | **`sub_00194300` holds the context in `[esp+8]`, and `[esi+0x100]` is `DEVICE+0x100`** — **the Session's detection script matched `mov dword ptr [esp + 8], ecx` as if it were `edi = ecx`** | **the Planner** |

## The mechanism, which instances 3, 4 and 5 share

**All three are the same failure: a LOOSE MATCH CONDITION in an extraction the Session wrote or trusted, with
NO per-item verification against the underlying artifact.**

- **#3:** the Session trusted its own reading of a tool's output format **without a control on the format.**
- **#4:** the Session trusted a **scope** it had been handed **without checking the geometry** — whether other
  claims fell inside the same window.
- **#5:** the Session trusted a **regex** — `edi, ecx` matched a stack store because the pattern was
  `mov\s+edi,\s*ecx` tested against `mov dword ptr [esp + 8], ecx` via a fallback branch that was too broad.

**In each case the artifact was CORRECT and the extraction was WRONG.** **And in each case the Session
published the extracted result as a finding rather than as an extraction awaiting verification.**

## Why the existing mandate did not prevent #5

**The Advisor's mandate was *"known-answer controls for every extraction."*** **The Session applied it to TOOL
output** — and it worked: **the `djv000_0.adx` control caught #3.** **But #5 was not a tool output; it was a
table the Session DERIVED from its own script.**

> **A control on a tool does not validate a table derived from that tool.**

**So the mandate needs a second half.**

## The corrected discipline

**For any table or list the Session DERIVES — not reads — from a tool, a script, or a decode:**

1. **The control must be applied PER ROW, against the underlying artifact** — not once for the extraction as a
   whole. **In #5, checking one row against the bytes would have exposed the `edi` error immediately.**
2. **A derived table is a HYPOTHESIS until each row is verified.** **It must not be published with the same
   confidence as a direct read.**
3. **When a script's match condition has a fallback or a broad pattern, the fallback is the risk.** **In #5 the
   broad branch silently produced a plausible-looking column.**
4. **Prefer a NARROW pattern that fails loudly over a broad one that succeeds wrongly.** **A script that
   returned `no match` for `sub_00194300` would have prompted the Session to look; the broad pattern returned
   `yes` and stopped the enquiry.**

## What the Session is changing

- **Derived tables now carry an explicit per-row verification requirement before publication.**
- **Where a script produces a table, the Session will print the MATCHED TEXT alongside the verdict**, so a
  loose match is visible rather than silent. **In #5, printing `matched: "mov dword ptr [esp + 8], ecx"` next
  to `edi ← ecx: yes` would have made the error obvious at a glance.**
- **The Session will state a derived table's provenance as DERIVED, distinct from READ**, so a reviewer knows
  where to aim.

## The honest summary

**Five errors, three of them the same mechanism, caught by three different parties** — **the Session once, the
Advisor twice, the Planner once.** **The line's review structure is what caught them, and that is the system
working.** **But the Session's own contribution to catching its errors is the weakest link, and the mechanism
above is where it fails.**

**Recorded because the Advisor's mandate is necessary and was not sufficient, and the gap is now identified
rather than merely lamented.**
