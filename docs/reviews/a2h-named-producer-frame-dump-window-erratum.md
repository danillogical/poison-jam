# ERRATUM — `A2h-named-producer-frame-r1`: dump-window corroboration is **NON-ADMISSIBLE**

**Append-only dated erratum to an accepted record.** **The original record and its bytes are PRESERVED
UNCHANGED** — this note corrects the reading, not the artifact. **No re-review is required (§3.2).**
**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** Advisor ruling `docs/reviews/a2h-integrity-audit-remediation-advisor-ruling.md`, turn
`01a0e83a`; audit evidence `docs/reviews/a2h-integrity-audit-execution-evidence.md` (R-1).

---

## What is withdrawn

**The run `logs/runs/20260927-160330-655-a4b2-gp-trap-trace` fails `check-dump-mapping.py` with
`CONTENT_MISMATCH`** (`matches: 0 / content-mismatch: 1 / unreadable: 0 / missing: 0`; displaced prefix
`83c41456ff5010eb5d8b44240c85c06a`).

**Therefore every value read from its dump is UNINTERPRETABLE.** **Two claims in
`docs/reviews/a2h-named-producer-frame-acceptance-review.md` rest on that dump and are WITHDRAWN:**

### (a) The `E = 0x00F7FEA0` corroboration table — lines 112–120

**All six rows, including the three marked "exact"** (`E−16 = 001804A0`, `E−12 = 001E0BE8`, `E = 0017C926`).
**The "exact" markings are void.**

### (b) The **caller return-address binding** — line 119

> `| `E` = `0x00F7FEA0` | **`0017C926`** | return address = `call@0x0017C921 + 5` — **exact** |`

**`0x00F7FEA0` lies INSIDE the tainted window `0x00F7FE8C..0x00F7FEAC`**, so **the caller binding read from
the dump is equally inadmissible.** **The caller attribution DROPS TO A CANDIDATE SET.**

> **The Session's remediation brief OMITTED this second item, and the Advisor caught it.** **Recorded so the
> omission is on the record rather than silently repaired.**

## The asymmetry that makes this indefensible

**The SAME evidence record correctly refused this SAME run's dump for the slot question** —
`a2h-named-producer-frame-evidence.md:312-314`: *"the apparent 0 at `0x001C4060` in the dump is not admissible
evidence"* — **and then read that dump as "exact" for the frame question.**

**And the acceptance review LISTED the `CONTENT_MISMATCH` in its own table** while still marking the window
values "exact". **So the gate result was present and its consequence was not drawn.** **This is a reading
failure, not a missing check.**

## What STANDS — mapping-immune, and unaffected

| Claim | Why it stands |
|---|---|
| **The frame arithmetic** | **two independent log-`esp` readings + the log ordinal** — **no dump involved** |
| **Call-site argument counts** | **static** — from XBE bytes |
| **The no-writer proof** | **static search** — no dump involved |
| **Row `O-OPEN`** | **STANDS, and is STRENGTHENED** — the missing-value ground is intact, and its *"recorded value at `0x00F7FEAC`"* specificity **survives verbatim via the arithmetic** |

**So this is a CORROBORATION-INTEGRITY DEFECT, not a disproof.** **The frame finding does not depend on the
dump.**

## Re-derivation is FORECLOSED, not declined

**The Session's brief offered *"re-derive from a mapping-clean run"* as an option.** **The Advisor ruled it
impossible rather than unnecessary:**

> **"Re-derivation is foreclosed, not merely declined: returned frames leave no trace and the crash activation
> overwrote (proven in-record) — no archive can contain it."**

**So there is nothing obtainable to re-derive, and the remedy is purely documentary.** **A live capture is the
row's NAMED SUCCESSOR, not a re-derivation.**

## Preserved unexamined

**The crash-register match is PRESERVED UNEXAMINED as the lead back**, should anyone ever need the window —
**with provenance of those registers established FIRST.** **Recorded so the lead is not lost and not
mistaken for established evidence.**

## Why this is an erratum and not a packet

**Per the ruling:** *"The remedy is documentation — nothing obtainable to re-derive, no criterion to decide,
row unchanged."* **Append-only, original bytes preserved — the same form as the earlier OOM correction.**
**No re-review.**

## What the audit's zero-`CLEAN` result means

**Per the ruling, the summary as framed is honest and needs no stronger statement:**

> **one `TAINTED`, three `ASSERTS-NOTHING`, four `NO-GUEST-MEMORY`, ZERO `CLEAN`.**

**`CLEAN`-zero is the correct output of the rule** — **no accepted row stakes positives on passing-run dump
memory** — **not a hygiene deficit.** **`NO-GUEST-MEMORY` stands on HOST-ADDRESS provenance**, and the
acceptance review verifies that classification.

## Handoff the brief omitted, now recorded

> **`A2h-callback-slot-writer-r1` is PENDING acceptance, hence OUTSIDE the audit's scope.** **Its acceptance
> MUST verify its register inputs' provenance.** **The audit's silence on it is SCOPE, not clearance.**

## Status

**Row unchanged: `O-OPEN`.** **No source, generated file, packet or acceptance artifact was edited** — this
erratum is a new record that corrects a reading. **No synthetic completion.** **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited.**
