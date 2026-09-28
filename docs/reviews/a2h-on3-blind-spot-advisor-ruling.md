# Advisor ruling — ON-3: park confirmed, the blind spot is **NARROWABLE**, and absence rows are DEAD

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e937-82a0-7000-8a31-cabd36633634`.
**Raised because:** the Session found the single-step window blind spot, withdrew three of its own claims, and
parked at ON-3.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: park at ON-3 confirmed (trials 4–5 would be uninterpretable, not merely low-value); blind spot real but narrowable — never eliminable; absence rows DEAD under this instrument, positive attribution viable; new packet required.**

**1. Partial observer — with the scope drawn tighter than briefed.** Recorded writer RIP+value+order stands as observed fact; ***nothing* absence-shaped survives (read-path-via-watch included — drop those rows, don't starve them).** The narrowing that makes this sound: **non-slot bytes fault → record first-touch → LEAVE RW, NEVER RE-ARM OR STEP** (census needs first touch only — **kills ~99% of windows: 9035 → handful**); **slot bytes keep full record + step + rearm**; **every window logged open/close with ticks.** **Residual same-bytes race covered by terminal-value coherence** (last-recorded vs terminal; mismatch → `UNKNOWN`). **No exclusion, no emulation — the fix is *less* machinery, not more.**

**2. Park confirmed — stronger than stated.** Not just low-value: **with `installer_control_hits=0`, *every* run is `INFRA FAILURE` per the packet's own rule, *including a lucky writer catch*.** **Trials 4–5 could not have produced a rowable result under any outcome.** **Park was mandatory.**

**3. Classifier first, and it needs a new packet** (code change: mapping fix + window narrowing; row change: absence rows dropped — **frozen contract absorbs neither**). **Phased:** **Exp0** classifier unit proof on known RIP↔guest pairs **incl. the `0x7B3` residual (offline)** → **Exp1** control run (**installer trap MUST fire** + loss accounting) → **Exp2** bounded attribution runs (**N≤5 pre-specified, early-stop on qualifying writer, K≥2-agree** per line discipline; zero qualifying → report + re-refer). **No third-run extensions, same anti-fishing bound.**

**BASIS:** observed — 9035 windows + 106 overlaps (race real, not theoretical); triple withdrawal record; zero-control-fires + packet's INFRA rule. Inferred — narrow-to-slot-bytes sufficiency (census needs first-touch only).

**REVERSED BY:** writer-observed run with full control green under the narrowed design (**then attribution proceeds — this ruling enables, not blocks, that outcome**).

**RECORD IN:** verbatim in the line's review record; **Session appends the three withdrawals + blind-spot analysis as decided here**; **Planner writes the new packet (no second preflight if this shape applied verbatim)**.

---

## ⚠ The narrowing is the important part — and the Session had not seen it

**The Session framed the blind spot as a LIMIT: *"it can prove writes happened and cannot prove they did
not."*** **The Advisor accepts that framing for absence rows and then NARROWS THE WINDOW ITSELF:**

> **non-slot bytes fault → record first-touch → LEAVE RW, NEVER RE-ARM OR STEP**
>
> **slot bytes keep full record + step + rearm**

**Why this works:** **the first-touch census only needs to know WHICH addresses were touched — not their
values.** **So a non-slot write does not need the single-step at all: the handler can record the touch and
leave the page writable.**

**And the effect is dramatic:** **`steps=9035` → "a handful."** **The 9 035 open windows become a handful,
because only SLOT writes open a window.**

> **So the blind spot is not eliminated — a slot write still opens a window — but it shrinks from 9 035
> windows to roughly the number of slot writes.** **That is the difference between an instrument that cannot
> answer the question and one that can.**

**And the Advisor's framing is exact:** ***"the fix is LESS machinery, not more."*** **The Session had been
looking for a way to close the window; the Advisor removes most of the reason the window exists.** ✓

## And the residual race is covered rather than ignored

> **"Residual same-bytes race covered by terminal-value coherence (last-recorded vs terminal; mismatch →
> `UNKNOWN`)."**

**So if a write lands in a remaining window, the terminal value will DISAGREE with the last recorded value —
and that disagreement is the detector.** **The Session notes this is the SAME pattern as Q3(c): two
independent observations that must agree, with disagreement meaning `UNKNOWN` rather than a claim.** ✓

## The park is confirmed on STRONGER grounds than the Session gave

**The Session's reason:** *"the blind spot is a design property, so a further run would reproduce it."*

**The Advisor's reason is stronger and independent of the blind spot:**

> **"with `installer_control_hits=0`, *every* run is `INFRA FAILURE` per the packet's own rule, *including a
> lucky writer catch*."**

> ## **So trials 4–5 could not have produced a rowable result UNDER ANY OUTCOME.** **Park was MANDATORY, not a judgement call.**

**The Session's reasoning was right by accident** — **it parked for the blind spot, and the control failure
would have forced the park anyway.** **Recorded because the Session's stated reason was not the operative
one.** ✓

## The phased plan — Exp0 / Exp1 / Exp2

| Phase | What | Gate |
|---|---|---|
| **Exp0** | **classifier unit proof on known RIP↔guest pairs**, incl. the **`0x7B3` residual** | **OFFLINE** |
| **Exp1** | **control run** — **the installer trap MUST fire** + loss accounting | **control green** |
| **Exp2** | **bounded attribution runs** — **N≤5 pre-specified, early-stop on a qualifying writer, K≥2-agree** | **zero qualifying ⇒ report + RE-REFER** |

**And: "No third-run extensions, same anti-fishing bound."** ✓

## What the new packet must change — and why it cannot be a patch

> **"code change: mapping fix + window narrowing; row change: absence rows dropped — frozen contract absorbs
> neither."**

**So BOTH a code change and a ROW-SET change are needed, and the frozen packet permits neither.** **That is
why a new packet is required rather than a repair.** ✓

**And: "no second preflight if this shape applied verbatim."** ✓

## The Session's next actions

1. **Append the three withdrawals and the blind-spot analysis** — **done** (`2f2be3c`).
2. **Have the Planner write the new packet** with this shape verbatim.
3. **Then Exp0 offline, Exp1 control, Exp2 bounded attribution.**
