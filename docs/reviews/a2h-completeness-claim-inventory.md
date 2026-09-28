# `A2h` — INVENTORY of completeness/uniqueness claims, and the §6.1 method rule

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** Advisor ruling `a2h-four-edges-method-advisor-ruling.md` (turn `01a0e896`), **point 1** —
*"The Session commits an inventory as analysis (no packet) … Re-refer with the inventory; I rule close
(recorded qualification) or packet per flagged item."*
**Purpose:** **hygiene review, not §5.4(2).** **No accepted row is reopened by this.**

---

## The inventory — every accepted A2h row's completeness claims and their enumeration method

**Seven accepted rows** (the integrity audit's reconciled inventory). **For each: does it make a
completeness/uniqueness claim, by what method, and is a later sound-method result already superseding it?**

| Row | Completeness claim? | Enumeration method | Flagged? |
|---|---|---|---|
| **`A2h-oom-causal-slice-r1`** (`O-OPEN`) | **NO** — no *"exactly one / complete sweep / N sites"* claim | **`loc_`-anchored** | **NO** |
| **`A2h-named-producer-frame-r1`** (`O-OPEN`) | **one** — *"my independent walk agrees on all 13 sites"* | **LINEAR DECODE** | **NO — see below** |
| **`A2h-null-slot-triage-r1`** (`O-NO-BOUNDARY-TRANSITION`) | **NO** | — | **NO** |
| **`A2h-slot-within-run-attribution-r1`** (`O-COVERAGE`) | **NO** — *"exactly one fatal terminal"* is an **event count from a LOG**, not a code sweep | **log-derived** | **NO** |
| **`A2h-arming-coverage-attribution-r2`** (`O-COVERAGE`) | **NO** | — | **NO** |
| **`A2h-arming-coverage-repeat-r1`** (`O-COVERAGE`) | **NO** | — | **NO** |
| **`A2h-rdata-call-target-r1`** (`O-OPEN`) | **YES** — *"4 sites, exactly one with `ecx−esp = 0x20`"* | **RAW-BYTE REGEX** over every executable section for `8D 4C 24 N 51 FF Dx` | **NO — alignment-independent** |

### Result: **ZERO flagged items**

**No accepted row carries a linear-derived, load-bearing completeness claim.**

## The two claims examined in detail, and why neither is flagged

### 1. `a2h-rdata-call-target-r1` — the uniqueness claim, and it is SOUND

**The claim:** *"a whole-executable sweep for `ecx ← [esp+N]` then `call reg` returns 4 sites, exactly ONE
with `ecx−esp = 0x20`."*

**The method, from the acceptance review line 64:** *"**Raw-byte regex** over every executable section for
the sequence `8D 4C 24 N 51 FF Dx`"* — **and the reviewer independently reproduced it**, adding that
**the `disp32` variant `8D 8C 24` returns ZERO**, *"so the sweep is complete."*

> **A raw-byte pattern scan is ALIGNMENT-INDEPENDENT, so the drift defect does not apply.** **And the
> reviewer's completeness argument is exactly the right form:** **it states the ENCODING COVERAGE** — the
> `disp8` form and the `disp32` form — **rather than assuming one.**

**That is the Advisor's refinement already satisfied in practice.** **NOT FLAGGED.**

### 2. `a2h-named-producer-frame-r1` — *"all 13 sites"*, and it is NOT a code-completeness claim

**The claim:** *"my independent walk agrees on all 13 sites."*

**This is a count of CALL SITES WITHIN ONE ENUMERATED SET** — the sites the packet's own evidence listed —
**not a claim that the image contains exactly 13.** **The acceptance review reached it by re-walking the
SAME set, not by claiming exhaustive coverage of the image.**

**So it is a WITHIN-SET agreement, and the set's own completeness was not claimed.** **NOT FLAGGED.**

**The Session records this distinction explicitly** because *"all 13"* reads like a completeness claim and
is not one. **A future reader should not treat it as one.**

## The §6.1 METHOD RULE — the Advisor's text, for insertion

**The Advisor supplied the rule verbatim, to be appended as a bullet alongside the existing enumeration
rule:**

> **"Completeness and uniqueness claims state their enumeration method. Linear-sweep decode is inadmissible
> for completeness without a drift control (known instruction addresses demonstrably reached); use recursive
> descent or equivalent control-flow-following enumeration. Byte-pattern scans are alignment-independent for
> existence; for uniqueness ('exactly one') state the encoding coverage over all instruction forms that could
> carry the pattern."**

**And the refinement is important:** **byte scans prove EXISTENCE alignment-free, but UNIQUENESS only with
stated encoding coverage** — **the same reference can wear different bytes, and aligned-only scans are
inadmissible for uniqueness**, as demonstrated by the **0-versus-3** discrepancy on vtable base `0x001E1270`.

## The two empirical facts the rule rests on

**Both measured this session:**

1. **Linear-sweep drift:** a linear decode of `.text` from its own start yields **29 548 instructions and
   reaches NEITHER `0x000D4DA0` NOR `0x00199F45`** — **both load-bearing for the four-edge result.**
2. **Aligned-only scan inadequacy:** an `--aligned` dword scan reports **ZERO** references to vtable base
   `0x001E1270` **where the correct count is THREE** — because the installs are `C7 06 70 12 1E 00` and
   **the immediate is not 4-aligned.**

## Why the Session's own surviving findings were sound

**The Session's key results came from RAW BYTE scans or declared boundaries**, both of which are
alignment-independent:

| Finding | Method |
|---|---|
| **exactly ONE direct store to `software_device+0x242C`** | **raw scan for displacement `2C 24 00 00`** — **1 hit image-wide** |
| **the thunk's single reference** | **raw dword scan for `0x00153790`** — **1 hit** |
| **`MEM32(0x001E133C) = 0x00153790`** | **direct read at a computed address** |
| **the vtable's extent** | **contiguous code-VA run detection** |
| **`0x000D4DA2 jmp [eax+0xcc]`** | **raw bytes `8b 01 ff a0 cc 00 00 00`** |

**So the drift defect did not reach the findings this line's current packet rests on** — **but it DID reach
the Session's failed thunk census**, **and the Worker's measurement is what quantified it.**

## Recommendation, per the Advisor's framework

**ZERO items flagged ⇒ the Session recommends CLOSE with a recorded qualification**, not a packet.

**The qualification to record:**
- **the §6.1 rule above becomes standing**;
- **any FUTURE completeness claim must state its method**, and **linear-sweep completeness is inadmissible
  without a drift control**;
- **and no accepted row is reopened**, because **none carries a flagged claim.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**No accepted row reopened.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**;
**`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
