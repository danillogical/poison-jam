# `A2h-integrity-audit-r1` — **`A-TAINTED`**: the Session's verification of R-1

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-integrity-audit-r1`, frozen
**`03F53E0AF09283EBD2B14886DC3FA625C25402875107D0759537A7C9D1B242F9`**.
**Executor:** `3b5a9174-c58c-45a9-8da1-5b8ea08637ac`, commit `06e2f8b`.
**Record:** `docs/reviews/a2h-integrity-audit-execution-evidence.md` (408 lines).

---

## Audit row: **`A-TAINTED`** — one remediation item, **R-1**

**The Session verified the taint independently and it reproduces exactly.**

| Element | Verified |
|---|---|
| **the run exists** | yes — `logs/runs/20260927-160330-655-a4b2-gp-trap-trace` |
| **its per-run gate** | **`CONTENT_MISMATCH`**, `matches: 0 / content-mismatch: 1 / unreadable: 0 / missing: 0`, displaced prefix `83c41456ff5010eb5d8b44240c85c06a` |
| **the tainted row** | **`A2h-named-producer-frame-r1`**, label `O-OPEN`, accepted **`ACCEPT`** + 3 corrections |
| **the consumed input** | `inspect-jsrf.py memory <run> 0x00F7FE60 0xA0` — the guest stack window `0x00F7FE8C–0x00F7FEAC` |
| **the affected claim** | the **`E=0x00F7FEA0` corroboration table** at `a2h-named-producer-frame-acceptance-review.md:112-120`, values marked **"exact"**, under an **AGREED** Criterion 3 |

## The finding's real shape — and it is the ASYMMETRY

**The executor named it precisely, and the Session confirms it from the files:** **the SAME evidence record
correctly REFUSED this SAME run's dump for the slot question** —

> `a2h-named-producer-frame-evidence.md:312-314`: *"the apparent 0 at `0x001C4060` in the dump is not
> admissible evidence"*

**— and then read that dump as "exact" for the frame question.** **That is the finding: the record knew the
dump was inadmissible and applied the rule in one place but not the other.**

**And the Session's own verification shows the acceptance review even listed the `CONTENT_MISMATCH` in its own
table** (`check-dump-mapping.py … CONTENT_MISMATCH, matches: 0`) **while still marking the window values
"exact".** **So the gate result was PRESENT and its consequence was not drawn.** **That is a reading failure,
not a missing check.**

## What survives, and the executor got the scope right

**The executor stated it correctly:** *"The frame arithmetic itself survives on XBE bytes/registers/three
arithmetic routes — **the dump was corroboration, so this is a degradation, not a disproof.**"*

**So R-1 is a corroboration-integrity defect, not a refutation of the frame finding.** **The Session accepts
that scoping** — **and notes it is exactly the kind of distinction this line has repeatedly had to draw.**

## The census finding — the strongest methodological result of the audit

> **The tainted run is `20260927-160330-655-a4b2-gp-trap-trace`. It is an `a4b2` run, NOT an `a2h` run.**
> **It was therefore NEVER IN THE 23/24 CENSUS, which globbed `2026*-a2h-*`.**

**The Session verified this directly: the census glob matches 24 runs; this run is not one of them.**

> **So the census could not have found the one real taint. The Advisor's insistence on per-row primary
> checks — and the explicit ban on citing the census per-row — is what found it.**

**That is a vindication of the ruling, and it should be recorded as the audit's most durable lesson.**

## The four cleared rows, and why the reasoning is sound

**The executor's non-finding is as valuable as the finding:** **`scripts/a2h-read-registry.py` reads a 64-bit
HOST address (`0x00007FF745DF9000`) in the minidump, NOT guest memory.** **The gate tests XBE-backed guest
content at guest VA `0x00011000`, so it does not reach a separately allocated host struct.**

**And the extraction carries its own independent known-answer control** (measured signature distance ==
`LATCH_OFF` == 538648). **So the four `O-COVERAGE`/triage rows are `NO-GUEST-MEMORY`, not tainted.**

**This confirms the Session's recorded prediction** (`a2h-integrity-audit-session-crosscheck.md`) **rather than
overturning it** — **and the Session notes the prediction was recorded BEFORE the audit ran, so it was
falsifiable rather than adjusted.**

**The executor also honestly bounded it:** *"which I reasoned from the tool's own header and its 538648 layout
control rather than from a dedicated host-memory gate (none exists)."* **That is the right caveat.**

## The two inventories agreed

**The executor's inventory (7 accepted rows) agrees exactly with the Session's independent cross-check**, and
**the executor reports the agreement explicitly**: *"Agrees exactly with your independent cross-check."*
**Per the packet, a disagreement would have been `A-IDENTITY`; there is none.**

## Controls: all six passed, none failed or absent

**Including the worked dump control on the ONE gate-passing run** — `001D5078 → 30766A64 305F3030 7864612E`,
**matching the packet exactly** — **the `.text` prefix in all 24 invocations, the code literal at
`recomp_0004.c:40150`, and a log control drawn from the SAME stream as the `[A2HSLOT]` terminal line.**

## The verdict on the motivating case

**`a2h-callback-slot-writer-r1` is NOT in the accepted universe (acceptance pending), and the executor checked
it anyway:** **its dump reads came from `20260928-035337-386-a2h-repeat-on-1`, which PASSES the gate.** **So no
taint arises there, and the Advisor's *"single-snapshot corroboration"* ruling holds.**

## Sequencing — what happens now

**Per packet line 16:** *"any `A-TAINTED` requires a separate remediation decision before dependent trace."*
**So the `edi` caller-trace WAITS.**

**The executor's own note is correct and worth carrying:** *"the caller-trace's own register inputs (`edi`
identity) came from the PASSING run and are not tainted by this finding; the sequencing requirement is
procedural, not evidential."*

**The Session is escalating for the remediation decision rather than choosing it.**

## Prohibitions and status

**Read-only throughout** — no game run, build, test, instrumentation or source edit. **No synthetic
completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` untouched. **`PIO_FREE` stays
DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired
NULL line was not reopened and no DR record was cited**; **the frozen packet was not edited.**
**All eight guards pass.**
