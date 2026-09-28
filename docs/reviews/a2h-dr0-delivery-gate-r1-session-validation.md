# `A2h-dr0-delivery-gate-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-dr0-delivery-gate.md`, authored by Planner
`a9935da2-c78f-47fc-bec8-6143dc5f0119` (`codex/gpt-6-sol` @ `high`), committed `0f31daa`.
**Authority:** the Advisor shape preflight `docs/reviews/a2h-fix-packet-advisor-shape-preflight.md`.
**Adequacy:** the Planner's §5.3 review — **`ADEQUATE`**, `BLOCKING: NONE`, `DEFERRED: NONE`.

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-dr0-delivery-gate-r1`** |
| Lines | **16** |
| **SHA-256 (frozen)** | **`12A0B68EB550A96DCD4D2393DBC81262AC0CEE2838CC403A9FD99ED18D3BE537`** |

## The packet implements the Advisor's two-phase ruling faithfully

| Advisor requirement | Packet |
|---|---|
| **Phase 1 gates Phase 2** | **C2 is the gate; C4 runs *"ONLY after C2 PASS"*** |
| **Lossless record — "no hit" must mean non-firing, not non-recorded** | **C1**: write-once `install_executed` latch **plus uncapped counters**, and **`install_executed=1` with a complete raw-event count of 0 ⇒ "no hit / non-firing"**, while **missing latch or incomplete accounting ⇒ "not recorded / UNKNOWN"** |
| **A software install comparison alone NEVER passes** | **C2 states it in terms** |
| **Discriminate second-chance / swallow / context-handling** | **C3**, with a stated discriminator for each and the rule that *"a hypothesis is not a certified cause without a discriminating control"* |
| **Drop with an explicit ceiling; never carry as decoration** | **Stop/drop** states the ceiling in full |
| **One packet, bounded** | *"**one packet only** … no serial speculative repairs"* |

**The losslessness split is the packet's most important line**, and it is exactly right: it separates
**non-firing** from **not-recorded**, which is the §6.1.6 discipline this line has repeatedly needed — and
which the Session itself violated twice today.

## The Session verified the finding's load-bearing link before freezing

**The entire finding rests on the claim that no `EXCEPTION_SINGLE_STEP` was delivered.** That inference
depends on the collector **printing every exception debug event**. **If that print were conditional or
filtered, zero single-step *lines* would mean only that they were not *printed*, and the finding would be
substantially weaker.**

**Verified at `collect.c:1156`:** the `DEBUG_EXCEPTION` `fprintf` sits **at the top of the exception branch,
before any filtering, gating or routing** — the `EXCEPTION_SINGLE_STEP` check is at **`:1163`, seven lines
later.** **So every exception reaching the debugger is printed, and the inference holds.**

**Recorded deliberately:** this is the link everything else rests on, **and the Session had twice earlier
drawn a conclusion from an artifact whose own summary line was true while its reading of it was false.**
**This time the reading was checked against the source before the finding was relied upon.**

## One thing the packet does that the Session wants to flag for the executor

**C3 requires DR6/DR7 and thread context readback "before/after `ContinueDebugEvent`".** **The collector's
existing `dr_arm_thread` readback happens immediately after `SetThreadContext` and before the continue**
(`collect.c:287-301`). **The packet asks for a readback on the far side of the continue as well**, which is
**new** — and it is the right thing to ask for, because **a context that reverts across `ContinueDebugEvent`
would explain zero hits while every existing check passes.** **The Session flags this as the single most
informative new measurement the packet requires**, not as a defect.

## Freeze decision

**The packet implements the ruling without expanding scope, its declared write scope is precise, and the
finding it rests on has been stress-tested.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class (`discovery`), its row set, its bounds, or its prohibitions.** **No synthetic
completion.** The producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4`
are **not** reopened; `0xFFFFB3` stays **`UNRESOLVED`**.

## Execution order

1. **Fixture-check the decision-input semantics** (C1's latches/counters) and run the collector harness probes
   and documentation guards.
2. **A fresh gated-OFF inertness control** *if* the collector changes invalidate the carried one.
3. **One bounded strict ON installation trial.**
4. **Apply the C2 gate**; `P1-PASS` unlocks C4, `P1-NONFIRING` or `P1-UNKNOWN` do not.
5. **§5.8 acceptance.**
