# Advisor ruling — ON trial 1: bounded repair, stop-at-1 confirmed, and **Q3 GATES EVERYTHING**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e90a-e3c3-7000-b87d-f0aa4581e17f`.
**Raised because:** ON trial 1 returned INFRA FAILURE, the watch demonstrably worked, and a linear fill
heading toward the slot needed a scope ruling.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: bounded repair (implement the packet's own traffic→counters design, slot target unchanged); stop-at-1 confirmed; Q3 gates everything — no ON runs until value-trust is proven.**

**1. Capacity fix is bounded repair, target unchanged.** The packet already says non-slot writes are TRAFFIC — **the implementation retained them as records in violation of its own design.** Fix = **implement the design** (traffic→counters; detailed records only for slot bytes + first-touch census), **not redesign.** The fill does NOT move the target: **rows stay slot-keyed (page writes ≠ slot writes)**; the fill is recorded as **context + prime suspect with a falsifiable prediction** (continued linear zeroing reaches `0x1D62C`), which fixed-instrument runs will **CONFIRM or REFUTE**. **New N=5 for the new instrument (different experiment); prior consumption noted, not deducted.**

**2. Stop-at-1 confirmed on all three grounds** — structural (deterministic fill vs fixed buffer reproduces overflow, not information), bound-compliant (early-stop-for-cause + re-referral is more conservative than extension), **with the determinism caveat stated honestly: IF the fill is nondeterministic, runs 2–5 might have been clean — accepted cost, dwarfed by fix-first value** (and *"zero qualifying in N"* was correctly NOT claimed). **The aborted run counts toward N** (excluding it would license re-rolling until lucky — exactly what N bounds prevent).

**3. Yes — Q3 gates everything, because the kind=2 third-state observation makes it load-bearing**, not academic: **slot reading `B8077500` (neither installed nor terminal value) is uninterpretable until values are trusted.** Require before any ON run relies on a value:
**(a)** static inspection of the pre-read path (**immediate**);
**(b)** a **COMMITTED positive-control fixture proving value fidelity end-to-end** (existing controls proved arming/firing, **never values**);
**(c)** **cross-validation where fault-record and hook reads overlap** (same address+time must agree — **the control that caught the byte-order trap**).
**Disagreement anywhere ⇒ instrument bug ⇒ fail closed.**

**BASIS:** observed — capacity arithmetic (129×2>256), fail-closed kill, fill pattern + 267-dword gap, kind=2 third value, packet's own traffic-design text. Inferred — fill-determinism caveat (stated, not assumed).

**REVERSED BY:** fill shown nondeterministic across runs (reopen N-sizing, not the fix); pre-path proven correct + fixture green (Q3 discharges normally).

**RECORD IN:** verbatim in the successor's planning record; Session appends the Q3 gate + N-accounting to the trial evidence.

---

## The correction the Session should absorb

**The Session framed the capacity defect as *"the design is RIGHT and the CAPACITY is wrong."*** **The
Advisor's version is sharper and less generous:**

> **"The packet already says non-slot writes are TRAFFIC — the implementation retained them as records in
> violation of its own design."**

**So it is not a capacity-tuning problem; it is the implementation FAILING TO IMPLEMENT ITS OWN SPEC.** **The
fix is to *"implement the design (traffic→counters; detailed records only for slot bytes + first-touch
census), not redesign."** **That is a materially different framing and it makes the repair bounded.** ✓

**And the fill does NOT move the target:** *"rows stay slot-keyed (page writes ≠ slot writes)."* **The Session
had asked whether the fill changes the scope; the Advisor refuses:** **the fill is *"context + prime suspect
with a falsifiable prediction (continued linear zeroing reaches `0x1D62C)`"*** — **a PREDICTION the
fixed-instrument runs will confirm or refute.** ✓

## The determinism caveat, stated honestly rather than argued away

**The Session's stop-at-1 reasoning rested on *"the fill is deterministic."*** **The Advisor confirms the stop
but refuses to let the premise go unqualified:**

> **"IF the fill is nondeterministic, runs 2–5 might have been clean — accepted cost, dwarfed by fix-first
> value."**

**So the Session's reason was *probably* right and is now recorded as an ASSUMPTION with a stated cost rather
than as a fact.** ✓ **And *"zero qualifying in N"* was correctly NOT claimed.**

**And the N-accounting rule:** **"The aborted run counts toward N (excluding it would license re-rolling until
lucky — exactly what N bounds prevent)."** **The Session had used 1 of 5; the Advisor confirms the run COUNTS,
and grants a NEW N=5 for the new instrument because it is a different experiment.** ✓

## ⚠ Q3 GATES EVERYTHING — and the Session must discharge it before any ON run

**The Advisor's reason is the one the Session had half-formed:** **the `kind=2` events read the slot as
`B8077500` — a THIRD value, neither the installed `0x0015F9D0` nor the terminal `0x001D5078`.** **So the slot
readings are UNINTERPRETABLE until the values are trusted.**

**Three requirements, all before any ON run relies on a value:**

| # | Requirement | Status |
|---|---|---|
| **(a)** | **static inspection of the pre-read path** | **IMMEDIATE — the Session executes this now** |
| **(b)** | **a COMMITTED positive-control fixture proving value fidelity end-to-end** | **existing controls proved ARMING/FIRING, never VALUES** |
| **(c)** | **cross-validation where fault-record and hook reads overlap** — same address+time must agree | **"the control that caught the byte-order trap"** |

**"Disagreement anywhere ⇒ instrument bug ⇒ fail closed."** ✓

**The Session notes requirement (c) is the Session's OWN control set applied to the instrument** — **the
byte-order trap was caught by exactly this kind of overlap check, and the Advisor is requiring it
structurally.** ✓

## The Session's next actions, in order

1. **Execute Q3(a) now** — inspect the `pre`-read path statically.
2. **Then Q3(b)** — a committed fixture proving value fidelity end-to-end.
3. **Then Q3(c)** — the overlap cross-validation.
4. **Then the bounded capacity repair** (traffic→counters per the packet's own design).
5. **Then fresh OFF + new N=5.**
