# Advisor ruling — `A2h` DR0 gate: one bounded repair, reshaped around a terminal snapshot

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e7d4-fd20-7000-a8e0-26c2bdf3f3e9`.
**Raised because:** the packet's own stop rule fired — *"irreconcilable/lossy evidence — STOP → Q3/Advisor, no
serial speculative repairs"* — after Phase 1 returned `P1-UNKNOWN` **because of a defect in the gate rather
than a dead channel.**
**Authority:** §2.3, §4.3.

**Route note:** one attempt failed with `MODEL_UNAVAILABLE` (stream idle timeout); served on retry at `low`
effort. Recorded so a future reader does not misdiagnose a route outage.

---

## RULING

**RULING: one bounded repair packet, reshaped — terminal snapshot replaces per-event readback; gate must still say `NON_FIRING` (underlying evidence alone is insufficient); post-continue array void in both directions; fresh packet, no fresh preflight (this ruling covers shape if applied verbatim).**

**1. Repair, not drop — last repair authorization.** Both defects are precisely understood with small standard fixes, and the trial eliminated everything except delivery. Ceiling applies AFTER this repair if it fails (then drop the leg + coverage-provenance, no third round without re-referral).

**2. Defect 1 forces neither drop nor declaration.** The underlying evidence is strong but **not decision-grade**: zero-`#DB` needs the delivery-completeness premise (no evidence `#DB`s enter this debugger's stream at all — the untested positive control) plus loss accounting, which only sound instrumentation provides. Strong ≠ decidable. Declaring `NON_FIRING` by fiat would substitute judgment for the packet's decision rule.

**3. Fixable — but NOT by per-event suspend-read.** Suspending 14k times is material timing perturbation on a timing-sensitive multithreaded failure, to no benefit. Instead: **terminal DR snapshot** (suspend-all-once at/after the fatal exception — zero behavioral concern, the run is dead), compared against the armed set, plus install-trap and hit records. **Confirmed:** `post_continue_reverted` — and the whole array, *including any no-revert reading* — is void this trial. Surviving independently: store executed, exact address match, zero-`#DB` as observed series (not as completeness).

**4. Fresh PACKET, no fresh preflight.** Shape mandated: (i) array → counters-or-nothing + terminal snapshot, no per-event suspend loop; (ii) **fresh OFF + one ON** — new code breaks the carry rule, so "one ON trial" alone is insufficient; (iii) install-trap positive control gates everything (no fire ⇒ infra failure ⇒ drop, no attribution reads); (iv) failure or new defects ⇒ drop + coverage-provenance, terminal.

**BASIS:** observed — defect table (capacity-16 vs 14 414 events; 2-vs-13210 race signature with 1216 intermittent successes); established API semantics (context valid only while suspended). Inferred — terminal snapshot sufficiency (persistent-state check needs no per-event sampling).

**REVERSED BY:** evidence the terminal snapshot cannot distinguish armed-state (then per-event design returns — re-refer, do not improvise).

**RECORD IN:** verbatim in the repair packet's planning record; Session appends the void-array + carry-rule corrections to the trial evidence.

---

## The three substantive corrections, and why each matters

**Correction 2 is the most important, and it rejects the Session's own leaning.** The Session suggested the
underlying evidence might be *"sufficient for a `NON_FIRING` finding without the broken `complete` flag."*
**The Advisor refused: *"Strong ≠ decidable."*** **A zero-`#DB` observation needs the delivery-completeness
premise** — and **there is no evidence that `#DB`s enter this debugger's stream at all**, which is precisely
the untested positive control. **Declaring `NON_FIRING` by fiat would substitute judgment for the packet's own
decision rule**, which is exactly the discipline this line has enforced on itself all session. **The Session
accepts this without reservation.**

**Correction 3 replaces a fix the Session had proposed with a better one.** The Session proposed
**suspend → read → resume per event.** **The Advisor refused on grounds the Session had not weighed:
*"Suspending 14k times is material timing perturbation on a timing-sensitive multithreaded failure, to no
benefit."*** **The terminal DR snapshot — suspend all threads once, at or after the fatal exception — is
free of that objection because *"the run is dead."*** **That is a strictly better design and the Session had
missed the perturbation argument entirely.**

**Correction 3 also voids the array in BOTH directions, which is stricter than the Session's reading.** The
Session said `post_continue_reverted=13198` may measure the measurement. **The Advisor voids the whole array
*"including any no-revert reading"*** — because **`post_continue_ok=1216` does not establish that those 1216
were meaningful either.** **A race does not produce trustworthy positives just because it sometimes returns
the expected value.**

**Correction 4 adds a requirement the Session had missed:** *"fresh OFF + one ON — new code breaks the carry
rule, so 'one ON trial' alone is insufficient."* **The Session had proposed only an ON trial.**

## What survives from the trial, per the ruling

| Fact | Status |
|---|---|
| **The store executed, with correct values** | **SURVIVES** |
| **The store wrote the EXACT watched address** (`host == canonical`) | **SURVIVES** — rules out address mismatch |
| **Zero `#DB` as an OBSERVED SERIES** | **SURVIVES — but NOT as completeness** |
| `post_continue_reverted` / the whole array | **VOID, both directions** |
| `complete=` / `decision=` | **VOID — structurally 0 by construction** |
