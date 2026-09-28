# `A2h-integrity-audit` — the Session's INDEPENDENT inventory (a cross-check for the auditor's)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why this exists:** the Advisor required that *"a self-assessed inventory must not stand unreviewed."*
**The Session built this independently of the audit executor** so the two can be compared. **It is a
CROSS-CHECK, not the audit.**

---

## The accepted-row universe — enumerated from primary files

**Acceptance artifacts present** (`docs/reviews/a2h-*acceptance*`, 11 files) and **packets** (14). **The
Session read each acceptance artifact's DISPOSITION line directly rather than trusting the plan's summary.**

| Packet | Final disposition | Rows |
|---|---|---|
| **`a2h-arming-coverage-attribution`** | **`ACCEPT`**, `BLOCKING: NONE` | `O-COVERAGE` |
| **`a2h-arming-coverage-repeat`** | **`ACCEPT-WITH-CORRECTIONS`** | `O-COVERAGE` |
| **`a2h-named-producer-frame`** | **`ACCEPT`** + three corrections | frame rows |
| **`a2h-null-slot-triage`** | **`ACCEPT`** | triage rows |
| **`a2h-slot-within-run-attribution`** | **`ACCEPT-WITH-CORRECTIONS`** | `O-COVERAGE`-family |
| **`a2h-oom-causal-slice`** | **`ACCEPT` at STAGE-2 ROUND 5** — after **stage-1 `NOT ACCEPTED`** and **stage-2 rounds 1–4 `NOT ACCEPTED`** | OOM rows |
| **`a2h-rdata-call-target`** | **`ACCEPT-WITH-CORRECTIONS`** | **`O-OPEN`** |
| **`a2h-callback-slot-writer`** | **row selected `O-OPEN`**; **no stage-1 review yet** | **`O-OPEN`** |

**Packets with NO acceptance artifact:** `a2h-dr0-delivery-gate`, `a2h-dr0-terminal-snapshot` (**ended
`P1-UNKNOWN`, DR leg dropped**), `a2h-live-oom-arg2-bridge`, `a2h-slot-read-path-displacement` (row withdrawn),
`a2h-writer-investigation`, `a2h-integrity-audit` (the current packet).

## The Session's own preliminary read — offered for comparison, NOT as the audit

**The Session's expectation, stated before seeing the auditor's result so it can be falsified:**

**Most A2h decision inputs are NOT dump reads.** **They are:**
- **log lines** (`[A2HSLOT]`, `GUEST_DR_*`, `[ICALL]`, `[EXCEPTION]`, the OOM slice line);
- **live hook reads** taken by the collector **during** the run (the census, the latch, the install
  control, the terminal reads) — **timestamped, not from a dump**;
- **code-static** facts (XBE bytes, generated source, section tables).

**The dump is used mainly for `inspect-jsrf.py memory` reads** — **which is exactly what the Session used
for the identity gate, and exactly where the integrity failure bit.**

**So the Session's preliminary expectation is that the audit finds FEW OR NO positive rows consuming
failing-run dump memory** — **but that is a PREDICTION, and the audit is what decides it.** **Recorded here
so the Session's expectation is on the record and can be checked rather than quietly adjusted.**

## The two rows the Session most expects to be scrutinised

**1. The identity-gate result itself (`a2h-callback-slot-writer`).** **The Session read
`MEM32(0x19DCE0)`, `MEM32(device+0x2268)` and `MEM32(device+0x242C)` from a dump.** **It used the ONE run
that passes the gate** — **but the Advisor has already ruled the `0xFD`/layout values are *"single-snapshot
corroboration"* and that *"the static no-construction-site carries the refutation."*** **So this row's
positive half does not rest on the dump.** **The audit should confirm that reading.**

**2. The 23/24 census.** **The Session must not have cited it per-row — and the Advisor forbade it.**
**The audit should verify the Session's own records do not lean on it for a specific row.**

## What the Session is NOT doing

- **Not executing the audit** — that is the executor's job, and the Advisor required it be independent of
  the Session's self-assessment.
- **Not running `check-dump-mapping.py` per row here** — **the Session's earlier census run was the
  population statement, and the packet forbids substituting it for per-row checks.** **The per-row results
  must come from the audit.**
- **Not drawing a verdict.** **The verdict is the audit's.**

## Status

**Recorded as a cross-check so the auditor's inventory can be compared against an independently built one.**
**If the two disagree on the row universe, that disagreement is `A-IDENTITY` and must be resolved before any
taint verdict.**
