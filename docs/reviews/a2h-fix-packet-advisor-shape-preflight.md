# Advisor shape preflight — the `A2h` fix packet, re-scoped around the DR0 positive control failure

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e7b8-1acd-7000-a6f9-254803978c78`.
**Raised because:** the DR0 positive control failure is a **mechanism-level** finding that affects the
instrument, so per `docs/agent-workflow.md` §4.3 the Advisor's HIT-branch authorization (*"fix packet"*) had to
be re-referred rather than executed as originally scoped.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: fix packet next, re-scoped — Phase 1 is the install-trap gate, Phase 2 attribution only if it passes; DR records inadmissible for any row until a live install trap passes; if unrepairable in bound, drop the leg with stated ceiling; no re-openings.**

**1. Still next, sharpened.** HIT authorized the fix packet; the finding narrows its first job to making DR fire, not hunting writers. Phase 1 (install-trap positive control, same run) gates Phase 2 (attribution reads). If Phase 1 fails in bounded effort → Q3, never attribution on a dead channel.

**2. Yes — install store is the ideal control**, and your gate is the right one: known canonical write, armed watch, every run, deterministic, terminal-independent. Require: `#DB` hit at install (before/after, site, tid, mapped VA) via lossless record (write-once latch + counters, so "no hit" means non-firing, not non-recorded) — else DR leg non-functional and every DR-based claim `UNKNOWN`. The reviewer's strengthening (zero `0x80000004` ever delivered across ~14k events/run) corroborates delivery failure over record loss.

**3. Repair boundedly, then drop if unrepaired.** One packet to diagnose delivery (second-chance vs swallowed vs context handling) against the install target. Ceiling if dropped, stated explicitly: census + software reads + install control stand; **no row reads DR records in either direction**, and canonical-write attribution (guest/host/transient-as-write) plus the DR leg of read-path become permanently unreachable. Never carry the channel as decoration.

**4. Nothing re-opens.** Accepted rows were run-scoped observations; the Planner's self-reversal followed the frozen contract, which predicted exactly this (`:27` requires the trapped install; `:19`/`:25` prescribe `O-COVERAGE` on its absence). Triage `O-NO-BOUNDARY-TRANSITION` stands (rested on `[A2HSLOT]` sampling, never DR). No §5.4(2): no frozen contract premised DR-fires.

**Practice: yes, record it** — as: *"a frozen packet's stated uncertainty is a scheduled verification task, not a carried caveat — test it at scoping time, before writing criteria around it, with a named owner."* Planner adequacy DECISIONS should map each uncertainty to its verification point. Operating-history entry suffices; no policy rewrite needed.

**BASIS:** observed — failure table (`a2h-dr0-positive-control-failure.md:15-27`), census/software/install unaffected (`:42-56`), frozen-contract prediction (`:73-96`), packet gates (displacement `:27`, `:45`; within-run `:19`, `:25`); ICALL raise path verified last turn. Inferred — nothing load-bearing.

**REVERSED BY:** install trap firing live (restores the DR leg under packet rules); evidence the DR silence has a benign configured cause that preserves completeness (re-scope, don't drop).

**RECORD IN:** verbatim in the fix packet's planning record; Session appends the practice note to operating history.

---

## What each part of the ruling requires

**The two-phase structure is the substantive design decision.** **Phase 1 is a GATE, not an investigation** —
it asks one question: **does a `#DB` hit arrive at the install store, a known canonical write under an armed
watch?** **Phase 2 attribution reads happen only if Phase 1 passes.** **So the fix packet cannot produce a
writer attribution on a channel that has not first been shown to work** — which is exactly the discipline the
failure demands.

**The losslessness requirement is carried forward correctly:** *"via lossless record (write-once latch +
counters, so 'no hit' means non-firing, not non-recorded)."* **That distinguishes the two failure modes the
§6.1.6 rule exists to separate**, and it is why the reviewer's debug-stream count matters: **zero
`0x80000004` events means the breakpoint never reached the debugger, not that a record was dropped.**

**The drop ceiling is stated explicitly and is severe, which is the point.** If the DR leg cannot be repaired,
**no row in this line may read DR records in either direction**, and **canonical-write attribution and the DR
leg of read-path become permanently unreachable.** **The Advisor's phrase — *"never carry the channel as
decoration"* — is the rule that prevents an unfired instrument from lending false authority to a row.**

**And the confirmation that nothing re-opens is grounded, not asserted:** the accepted rows were **run-scoped
observations**, the Planner's self-reversal **followed the frozen contract**, and **`O-NO-BOUNDARY-TRANSITION`
rested on `[A2HSLOT]` sampling, never on DR.** **No §5.4(2)** — no frozen contract premised that DR fires.

## The practice note, adopted

> **"A frozen packet's stated uncertainty is a scheduled verification task, not a carried caveat — test it at
> scoping time, before writing criteria around it, with a named owner."**

**Planner adequacy decisions should map each uncertainty to its verification point.** **The Session records
this in `docs/jsrf-operating-history.md`, per the Advisor's direction that an operating-history entry suffices
and no policy rewrite is needed.**

**Why it is worth recording:** **the frozen packet predicted this failure in writing** — *"live `#DB` chance
semantics … have not yet been proven in the game"* — **and the prediction sat as a caveat through three
packets.** **It was tested only when the Session asked what the NEXT packet would target.** **The caveat was a
scheduled test that nobody had scheduled.**
