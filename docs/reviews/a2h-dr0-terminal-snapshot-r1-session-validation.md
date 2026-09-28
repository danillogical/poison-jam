# `A2h-dr0-terminal-snapshot-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-dr0-terminal-snapshot.md`, authored by Planner
`ca0c0cb7-2887-4f3f-b1a8-9afb098ed28f` (`codex/gpt-6-sol` @ `high`), committed `e80a718`.
**Authority:** `docs/reviews/a2h-dr0-gate-repair-advisor-ruling.md` — *"one bounded repair packet, reshaped."*
**Adequacy:** the Planner's §5.3 review — **`ADEQUATE`**, `BLOCKING: NONE`, `PREMISE_FRESHNESS: BOUNDED`.

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-dr0-terminal-snapshot-r1`** |
| Lines | **16** |
| **SHA-256 (frozen)** | **`969E8827CC3D36AA8AFBF7D4FF7D43609BB750312723AAFBCE7ADFA5CA48F7EE`** |

## The Advisor's mandated shape — every element present

| Mandated | Packet |
|---|---|
| **array → counters-or-nothing + terminal snapshot, no per-event suspend loop** | **C1** removes the array, its capacity/overflow predicate and all after-continue reads, retaining **write-once latches + uncapped counters**; **C2** adds the single terminal snapshot |
| **terminal DR snapshot, suspend-all-once** | **C2**: *"enumerate/open ALL target threads, `SuspendThread` each ONCE before `GetThreadContext`"* |
| **compared against the armed set** | **C2**: *"compare with the armed tid/address/DR7 set and install-trap/hit records"* |
| **fresh OFF + one ON** | **Execution bounds**, with the carry rule stated explicitly |
| **install-trap positive control gates everything** | **Positive-control priority**: *"only PASS unlocks predecessor C4/Phase 2"* |
| **failure ⇒ drop + coverage-provenance, terminal** | **Terminal ceiling**: *"this is the LAST authorized repair"* |
| **`NON_FIRING` reachable, but delivery-completeness still required** | **Gate restated**, and the Advisor's *"Strong ≠ decidable"* is honored: *"If the `#DB` delivery-completeness premise is unproved, report `P1-UNKNOWN` rather than promote observed zero to decision-grade `NON_FIRING`."* |

## The Session's finding is incorporated as an explicit precondition — and correctly framed

**The Session verified that `capture()` does NOT suspend before reading context**, which meant a snapshot
bolted onto it would **inherit exactly the defect being repaired.** **The packet states this at line 7 in
terms:** *"the existing `capture()` does NOT suspend"* — and makes suspension a **requirement**, with
**"any unsuspended read is VOID (`UNKNOWN_NOT_RECORDED`)."**

**The Planner's caveat is honest and correct:** *"I did not independently verify current collector thread
enumeration/suspend implementation; your finding was incorporated as explicit precondition."* **That is the
right way to handle a relayed source fact — state it as a precondition rather than as verified behaviour**,
and it means **the executor must confirm it rather than assume it.**

## The Session verified the snapshot's feasibility independently

| Precondition | Verified |
|---|---|
| the collector reaches a post-fatal point | **`collect.c:1752-1759`** — the `!dwFirstChance` branch calls `capture(...)` at `:1758`, then `finished = 1` |
| it enumerates and opens target threads | `capture()` uses `CreateToolhelp32Snapshot` + `Thread32First` + `OpenThread(… GET_CONTEXT …)` + `GetThreadContext(CONTEXT_ALL)` |
| the **armed set** is readable for comparison | **`dr_arm_tids[…]`** is a static array **in the same process**, and **`dr_was_armed(tid)`** already exists (`:724`) |

**And the Phase 1 run's `stacks.txt` confirms capture runs after the fatal exception** — it carries
`GUEST_DR_ARM_TID` (27), `GUEST_DR_DELIVERY_TERMINAL` (1), `GUEST_THREAD` (5) and `GUEST_DR_DISARM` (1).

**So the repair is feasible, and the one thing that could silently reproduce the bug — an unsuspended read —
is now an explicit, void-if-violated requirement.**

## Two things the packet gets right that are easy to get wrong

**1. It does not weaken the gate while repairing it.** The Advisor warned that *"the underlying evidence is
strong but not decision-grade"* and that *"declaring `NON_FIRING` by fiat would substitute judgment for the
packet's decision rule."* **The packet keeps the delivery-completeness premise as a REQUIREMENT** and routes
an unproved premise to `P1-UNKNOWN` — **so `NON_FIRING` becomes *reachable*, not *automatic*.** **That is the
distinction the repair had to preserve.**

**2. It states the soundness limit rather than overclaiming.** Line 8: *"A terminal match verifies persistence
at the snapshot, not uninterrupted arming throughout the run or a delivered `#DB`; no snapshot value alone
proves the install trap."* **That is exactly the boundary the Session has had to redraw repeatedly in this
line, stated in advance this time.**

## Freeze decision

**The packet implements the ruling verbatim, its declared write scope is the same three files, and its
ceiling is terminal.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`), its question, its bounds, or its prohibitions.** **No synthetic
completion.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4`
are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## Execution order

1. **Implement C1 + C2** — including the explicit **suspend-before-read**, and **verify that the existing
   `capture()` really does not suspend**, since the Planner relayed that as a precondition rather than a
   verified fact.
2. **Fixtures** — including **real debugger-delivered `#DB` ingress**, not an injected imitation.
3. **Guards, build, ctest, harness probes.**
4. **Fresh OFF** (the collector changes) → **one bounded strict ON trial.**
5. **The gate** → §5.8 acceptance. **Failure ⇒ drop the leg, coverage-provenance, terminal.**
