# ⚠ `A2h-slot-writer-terminal` — the row rests on an **UNWITNESSED INDEX**, and the Session must say so

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Raised by:** the Advisor (turn `01a0e8af`), **which caught what the Session had accepted.** **The Session
verifies the catch and records it against its own verification.**

---

## What the Advisor caught

> *"`ARG1 = 2064` appears only as assertion (evidence `:13`, `:43`, `:284`), while its supplier is the
> record's own named missing edge (EDGE 5, `:211-217`); runtime components (`0,29,80,120`) are unwitnessed
> (zero dump reads by design); writer execution/order is unbound (static-only, no trace of the worker).
> Against a bar requiring 'complete … proving base/index/value and order,' **mechanism-fit + exact
> value-match is strong abduction, not closure.**"*

**The Session verified it, and it is correct.**

## The evidence's own text, read carefully

**Line 284 presents the index as a comment:**

```
mov ebp, [esp+0x20]        ; ebp = ARG1 = 2064       (INDEX)
```

**But line 13 says WHY `2064` was chosen:**

> *"**`ebp = ARG1 = 2064`**, **because `0x3EC + 2064*4 = 0x242C`** — the same slot"*

> ## **`2064` was SOLVED BACKWARDS FROM THE TARGET ADDRESS.** **It is the index that WOULD land on the slot — not a value anyone observed ARG1 to take.**

**And the Session's own verification record repeated the error**, endorsing *"`0x3EC + 2064·4 = 0x242C`,
Session-verified EXACT"* **as though the arithmetic verified the INDEX. It verifies only that `2064` is the
index that would reach the slot.**

**This is the same failure the Advisor barred for `device+0x242C`** — *"no attribution by offset
arithmetic"* — **arriving through a different door.** **The Session had explicitly written that `ebp = ARG1 =
2064` was *"established from a decoded instruction, not inferred from the offset, which is what makes it
admissible."** **That was WRONG:** **the DECODE establishes `ebp = ARG1`; the ARITHMETIC supplies `2064`.**

## What IS established, and what is NOT

| Link | Status |
|---|---|
| **`0x00199F45` is `mov [esi+ebp*4+0x3ec], eax`** | **ESTABLISHED** — bytes `89 84 AE EC 03 00 00` |
| **`esi = MEM32(0x19DCE0)` = `software_device`** | **ESTABLISHED** — `0x00199DB4` |
| **`ebp` = ARG1** | **ESTABLISHED** — `0x00199DD9`, verified three-deep |
| **`0x3EC + 2064·4 = 0x242C`** | **ESTABLISHED ARITHMETIC** — but it is an **identity**, not an observation |
| **ARG1's runtime value** | **⚠ UNWITNESSED** |
| **whether ARG1 can REACH 2064** | **⚠ NOT ESTABLISHED** |
| **the packed value `0,29,80,120`** | **⚠ UNWITNESSED** — derived from the quantization arithmetic, no dump read |
| **writer execution / order** | **⚠ UNBOUND** — static only, no trace of the worker |

**So `CHAIN_COMPLETE: yes` at line 301 OVERSTATES.** **The correct statement is: the base is witnessed, the
index EXPRESSION is witnessed, the arithmetic identity is exact — and the index VALUE is a solution, not an
observation.**

## The Advisor's framing, which is the right one

> **"mechanism-fit + exact value-match is strong abduction, not closure."**

**And its instruction to acceptance is the correct handling:** ***"Acceptance must rule each link
witnessed-vs-inferred explicitly — a row sustained on inference alone must say so, not pass silently."***

**The Session has passed this to the acceptance reviewer as the crux question (claim 5), and it records the
gap here rather than waiting for the review to find it.**

## Credit where the Advisor gave it, and the Session confirms

> **"these gaps are visible ONLY because the record marks them honestly instead of burying them."**

**The Worker DID mark them:** **it named EDGE 5 as a new missing edge, it stated `CHAIN_COMPLETE: yes` on the
basis of the arithmetic, and it performed ZERO dump reads and said so.** **The overstatement is in the
SUMMARY LINE, not in a buried assumption.** **That is why the gap is findable in one reading.**

**The Session's own contribution to the error was worse:** **it endorsed the arithmetic as though it
verified the index, and it wrote the sentence that made the inference sound admissible.**

## What the Session is doing about it

1. **This record**, so the gap is on the record regardless of the acceptance outcome.
2. **The acceptance reviewer already has claim 5 as its crux question**, with the instruction to reconcile
   the reversal against the earlier evidence that said `2064` was *"NOT ESTABLISHED reachable."*
3. **No row change yet** — **the acceptance reviewer rules, not the Session.**
4. **The Advisor's conditional stands:** *"this authorization is conditional on acceptance sustaining
   `O-DATA-AS-CALL`."* **If acceptance downgrades the row, the SAME evidence re-scopes as
   chain-completion (ARG1/order/value first) — and per the Advisor, *"either way no new referral is needed to
   proceed."***

## The mechanism packet is authorized, with the writer-side-first scope

**Per the Advisor's Q1, if the row stands:**

- **(a)** **dispatch caller + ARG1/trip-count — closes EDGE 5**;
- **(b)** **component provenance**;
- **(c)** **SIB-sweep exclusivity — FOLDED IN, not standalone**;
- **(d)** **order binding.**
- **Reader-side only IF (a) EXONERATES the writer** — *"in-spec behavior ⇒ surprise moves to reader/slot
  contract."*
- **TERMINAL: closes mechanism or PARKS with a precise edge; no auto-chain.**

**And the Advisor's Q2 requirement is a correction to the Session's instinct:** **the Session had suggested
NOT running a SIB sweep. The Advisor rules it IS warranted, inside the mechanism packet, never standalone:**
*"found-writer does not close exclusivity (ModRM scan is encoding-scoped; no image-wide SIB sweep exists)."*
**And it must be encoding-complete per §6.1 — ModRM + SIB + computed/alias forms — with any competitor
getting the same ARG1/value/order treatment.**

**The Session accepts this: its own instinct would have left an encoding-scoped claim standing as
exclusivity, which is the same error the line just made.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**No row change by the Session** — acceptance rules. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited.**
