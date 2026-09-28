# Advisor ruling — the `0x001D5078` identity gate: `O-OPEN`, audit first, caller-trace queued

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e82b-ec09-7000-8e8e-1c23b975e56b`.
**Raised because:** the identity gate answered NO — the accepted packet's `device+0x2268` alias does not bind
the failing call — and the row selection plus the 23/24 integrity finding both needed a ruling.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: `O-OPEN` — refutation is not identification. Audit packet first (new, bounded, read-only), caller-trace second. `A4b2-r8` explicitly unaffected.**

**1. Row: `O-OPEN`, confirmed — your reading is correct.** `O-ALTERNATE-PATH` requires *verified distinct context identity* **plus** routes/mismatch defeating the path, and its next action is a packet *"on the REAL context object"* — which cannot be scoped around an unknown. What you delivered (alias mismatch: no site constructs `device+0x2268`; `[device+0x2268]=0xFD` in one clean snapshot) **defeats the device-slot branch permanently — no future packet may re-litigate the alias without new evidence** — but **`edi`'s actual identity is still `UNKNOWN`, so the row's positive half is unmet.** Select `O-OPEN` with the named edge (`edi` identity); note the successor **IS** the identification (caller-trace), not conditioned on it.

**2. Bounded audit PACKET first — not a mere caveat, and the order matters.** Two reasons, one structural and one earned: **(a) the trace packet's register inputs may themselves be tainted, and only the audit can clear them — dependency, not hygiene; (b) with three self-caught misreads on artifact interpretation, a self-assessed inventory must not stand unreviewed.** Scope (read-only, no runs): **every *accepted A2h row only*, scored by decision-input source** — log/trace/code-static vs guest-memory-from-dump (+ that run's mapping result) vs live hook reads (timestamped) vs inference. **`O-OPEN`/`UNKNOWN` rows get a label check (they assert nothing); any *positive* claim consuming failing-run guest memory gets flagged with exact input/run/status for remediation.** Other lines excluded (their own gates already cover their runs — no sprawl). **Mandate known-answer controls for every extraction** (the `djv000_0.adx` control that just saved this line, fourth instance of the pattern).

**3. Then caller-trace, not concurrently.** Discriminator-1-one-level-up is the right next static step, **scoped to mapping-clean inputs + XBE bytes + log lines only.** It waits for the audit because **its register inputs are exactly what the audit clears** — running it first risks building on tainted values for zero time saved (the audit is read-only and fast).

**Also recorded:** the `0xFD`/layout values are **single-snapshot corroboration** (static no-construction-site carries the refutation); **the 23/24 census must not be cited for any *specific* row (per-row primary mapping checks required)**; **`A4b2-r8` is clear** (no R1 dump on its PASS path by prior ruling; R0 dump verified clean; acceptance recorded the mapping fail explicitly) — **as are `A4b1` (no guest-memory reads) and `PIO_FREE` demand-bound (offline XBE bytes)**.

**BASIS:** observed — packet row texts (`:16`–17, `:19`); Session's refutation + caveats as briefed (leads, backstopped by acceptance). Inferred — audit-before-trace dependency; reviewability requirement from the misread record.

**REVERSED BY:** audit finding zero tainted inputs (trace proceeds unscoped) or a positive row consuming failing-run memory (remediation packet, not silent absorption).

**RECORD IN:** verbatim in the callback packet's review record (`O-OPEN` selection + named edge); new audit packet authorized above (read-only, A2h accepted rows); caller-trace queued behind it with input restrictions.

---

## The three substantive points, and why each matters

**Point 1 confirms the Session's own reading and adds a durable consequence.** The Session wrote that it
*"would rather record `O-OPEN` with a precise named edge than claim `O-ALTERNATE-PATH` on a refutation
alone."* **The Advisor agrees — and adds something the Session had not drawn out: the refutation is
PERMANENT.** *"No future packet may re-litigate the alias without new evidence."* **So the device-slot branch
is closed for good, which is a real narrowing even though the row is `O-OPEN`.**

**Point 2 is the one the Session had underweighted, and the Advisor's reasoning is sharper than the Session's.**
The Session framed the 23/24 finding as *"a recorded caveat"* or possibly its own packet. **The Advisor makes
it a DEPENDENCY:** *"the trace packet's register inputs may themselves be tainted, and only the audit can clear
them — dependency, not hygiene."* **That reorders the work: the caller-trace cannot be trusted until the audit
says its inputs are clean.**

**And the second reason is aimed at the Session's own record:** *"with three self-caught misreads on artifact
interpretation, a self-assessed inventory must not stand unreviewed."* **That is a fair and specific criticism
— the Session has made three interpretation errors this session, and an inventory it both performs and
self-approves would repeat the pattern.**

**Point 3 refuses concurrency for a stated reason, not as caution:** *"running it first risks building on
tainted values for zero time saved (the audit is read-only and fast)."*

## What the Session must NOT do, per the ruling

- **Do not cite the 23/24 census for any SPECIFIC row.** **Per-row primary mapping checks are required.**
  **This is important: the census is a population statement, and the Advisor has explicitly forbidden using it
  as evidence about any individual row.**
- **Do not re-litigate the alias** without new evidence.
- **Do not sprawl into other lines** — their own gates already cover their runs.
- **Do not run the caller-trace concurrently.**

## The clearances, recorded because they close questions

| Line | Status |
|---|---|
| **`A4b2-r8`** | **CLEAR** — no R1 dump on its PASS path by prior ruling; R0 dump verified clean; acceptance recorded the mapping fail explicitly |
| **`A4b1`** | **CLEAR** — no guest-memory reads |
| **`PIO_FREE`** | **CLEAR** — demand-bound, offline XBE bytes |

## The methodological mandate that outlives this line

> **"Mandate known-answer controls for every extraction."**

**The Advisor calls the `djv000_0.adx` control *"the fourth instance of the pattern"*** — i.e. the Session has
now four times been saved by, or should have used, a known-answer check on a tool's output. **This is now a
standing requirement rather than a lesson: every extraction gets a known-answer control.**
