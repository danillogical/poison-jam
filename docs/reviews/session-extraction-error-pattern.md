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
| **6** | Wrote the callee's context load as **`mov esi,ecx` at `0x00193D62`** in the promoted plan | **the actual address is `0x00193D96`** — **the digits are transposed** | **the Session, while re-verifying against the bytes for an unrelated check** |

**Instance 6 was caught only because the Session re-read the bytes for a different purpose.** **It is the same
mechanism again — a value written from memory rather than copied from a verified read** — **and it is a useful
data point: the error survived one commit and a promotion, and nothing in the review structure caught it.**

## The mechanism, which instances 3, 4, 5 and 6 share

**All four are the same failure: a value produced by a LOOSE OR UNVERIFIED STEP in an extraction the Session
wrote or trusted, with NO per-item verification against the underlying artifact.**

- **#3:** the Session trusted its own reading of a tool's output format **without a control on the format.**
- **#4:** the Session trusted a **scope** it had been handed **without checking the geometry** — whether other
  claims fell inside the same window.
- **#5:** the Session trusted a **regex** — `edi, ecx` matched a stack store because the pattern's fallback
  branch was too broad.
- **#6:** the Session **wrote an address from memory** instead of copying it from the verified read it had
  already made. **The correct value was in the Session's own earlier output.**

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
- **NEW, from instance 6: ADDRESSES AND VALUES ARE COPIED, NEVER RETYPED.** **Every hex address or measured
  value that reaches a record will be copied from the verified output that produced it, not written from
  memory.** **Instance 6 was a transposition of two digits in an address the Session had ALREADY verified
  correctly elsewhere — the information was present and the Session did not use it.**

## The honest summary

**Six errors, four of them the same mechanism, caught by three different parties** — **the Session once, the
Advisor twice, the Planner once.** **The line's review structure is what caught them, and that is the system
working.** **But the Session's own contribution to catching its errors is the weakest link, and the mechanism
above is where it fails.**

---

# INSTANCE 7 — the byte-reversal, and it is a NEW and more dangerous class

**Added 2026-09-28 after the binding re-read (`a2h-binding-reread-executed.md`).**

**The Session reported `MEM32(device+0x2268) = 0x000000FD` and concluded it was *"a small INTEGER, not a
pointer"* — hence a counter, not a device sub-object — and built a "permanent" refutation on that.**
**The true value is `0xFD000000`: THE NV2A MMIO APERTURE BASE, a hardware constant documented 29 times in
this repository.**

## Why this class is different from instances 3–6

**Instances 3–6 were loose matches, a scope error, and a transposition.** **This one is:**

> **The Session applied a TRANSFORMATION that produced a PLAUSIBLE-LOOKING value, then reasoned confidently
> from the transformed value. The transformation was invisible because its OUTPUT was well-formed.**

**`0x000000FD` looks exactly like a counter. `0xD0F91500` looks exactly like a junk pointer.** **Neither
prompted doubt — and the second one made the Session report the OPPOSITE of the truth:** it said
`MEM32(device+0x242C)` was **not** the refcount thunk, when the true value `0x0015F9D0` **is**.

## The control that catches this class

**An OFFSET-SHIFT test.** **Read a known string at `VA` and at `VA+1`:**

| Read | LE-DWORD-VALUE reader | Memory-order reader |
|---|---|---|
| `0x001D5078` | `30766A64` | `646A7630` |
| **`0x001D5079`** | **`3030766A`** | `6A763030` |

**A byte-reversal does NOT shift correctly, so the second read discriminates.** **The Session's original
control (`0x001D5078` alone) was satisfied by BOTH readings and never discriminated.**

**This is why the Advisor made the offset-shift control BINDING rather than advisory.** **It is now the
standard for every memory-derived value.**

## The reframing the Session accepts

**The Advisor wrote:** *"seven extraction failures with controls catching each is now a validated method, not
just confessions."*

**That is a fair reading and the Session adopts it.** **The controls caught all seven** — **one by the
Session, twice by the Advisor, once by the Planner, and instance 7 by the Advisor's mandated offset-shift
test.** **The practice note's value is not the confession; it is the CONTROL SET that emerged:**

| # | Control | Catches |
|---|---|---|
| 1 | **Known-answer control for every extraction** | format errors (instance 3) |
| 2 | **OFFSET-SHIFT control for every memory read** | **transformation errors (instance 7)** |
| 3 | **Per-row verification against the underlying artifact** | loose matches (instance 5) |
| 4 | **Geometry check: does another claim fall inside the same scope?** | scope errors (instance 4) |
| 5 | **Addresses and values are COPIED, never retyped** | transpositions (instance 6) |
| 6 | **A derived table is a HYPOTHESIS until each row is verified** | the whole family |
| 7 | **ANCHOR EVERY DECODE TO A DECLARED FUNCTION BOUNDARY, not a section start** | **instance 12 — section-start decoding DRIFTED and produced 9945 plausible instructions while never reaching a known site** |

**SEVEN controls, and instance 12 (the drifted section-start decode) added control 7.** **Each control exists
because a specific error got past everything else.**
**That is the systemic form the Advisor identified, and it is now written down as a SET rather than as
anecdotes.**

**Recorded because the Advisor's mandate is necessary and was not sufficient, and the gap is now identified
rather than merely lamented.**
