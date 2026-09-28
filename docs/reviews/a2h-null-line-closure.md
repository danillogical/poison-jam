# `A2h` NULL line — CEILING REACHED and RETIRED; pivot to `0x001D5078` due

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e7f9-883f-7000-b718-3feb12510352`.
**Raised because:** the terminal-snapshot packet hit the Advisor's **terminal ceiling** — the DR leg is
dropped, and the line's disposition (*pivot or defer*) is an Advisor decision.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: drop CONFIRMED — no re-referral for the contradiction (both branches terminate the leg); pivot to `0x001D5078` now DUE; NULL line exhausted with the honest statement below.**

**1. Ceiling stands.** The contradiction's specificity is real but not decisive: (a) genuine-clear explains zero-`#DB` yet opens an unbounded who-cleared question while leaving the leg uncertifiable; (b) read-artifact voids the snapshot for exactly the thread that matters. **Explanatory power is not decidability — both branches end at drop.** No third round: five packets plus preflights with each fix revealing the next caveat is the diminishing-returns pattern the ceiling exists to stop.

**2. Pivot due.** The triggers resolve as: MISS never occurred, HIT occurred, fix failed at its ceiling — and no NULL route remains (read-path needs complete canonical coverage, now unachievable with any available instrument: **DR dead, page-guard redirected for single-step races; so read-path is DEAD, not deferred**). `0x001D5078` next: characterized (3/10, `.rdata` string, deterministic registers), **independent of the poisoned instrument**, static-first with instrumentation only on gaps. Scope it to that terminal alone — float-bit siblings stay contrastive; per-terminal classification before any family claim.

**3. Honest statement, confirmed with two sharpens:** the slot read zero at **two independent terminal reads**; **zero alias touches across a complete 28/28 census**; **canonical-write channel UNCERTIFIABLE** (never *"no write occurred"* — **the distinction is the whole finding**). Sharpens: (i) **delivery premise PROVED on-host**, so the failure is **game-context-specific**, closing that line of inquiry permanently; (ii) **producer stays PARKED** (no link evidence either way), **triage/`O-NO-BOUNDARY` rows stand** (never touched DR), **install software control stands**.

**BASIS:** observed — terminal snapshot contradiction (named thread, both values), zero-`#DB`-ever-delivered + host delivery proof, census/reads/install as cited; repair-gate failure per its own bound. Inferred — nothing load-bearing.

**REVERSED BY:** a sound canonical-write instrument (none exists today; page-guard would need its race solved, DR its delivery + state-readback — **neither is a small delta anymore**).

**RECORD IN:** verbatim in the line-closure record; Session writes the ceiling entry (gains + limits + pivot trigger consumed), opens the sibling packet sketch, and **retires the NULL line without deleting its evidence**.

---

## The line's honest final statement

> **The slot read zero at two independent terminal reads. Zero alias touches were observed across a complete
> 28/28 page-protection census. The canonical-write channel is UNCERTIFIABLE — which is not the same as
> "no write occurred," and that distinction is the whole finding.**

**The Advisor's sharpenings matter and are recorded verbatim above.** **The delivery premise being proved
on-host is the one that closes a line of inquiry permanently:** the failure is **game-context-specific**, not
a host or collector incapability. **And the producer line stays PARKED** — **nothing in this work produced
link evidence either way.**

## Why the ceiling was the right call, in the Advisor's own terms

> *"Explanatory power is not decidability — both branches end at drop."*

**That sentence is the whole disposition.** The install-thread contradiction is **specific and interesting** —
a named thread whose DR state disagrees with its own successful arm record — **and the Session explicitly
offered it as possibly worth another look.** **The Advisor refused, and the reasoning is sound:**
- **branch (a)**, a genuine clear, **explains zero `#DB` but opens an unbounded "who cleared it" question
  while leaving the leg uncertifiable**;
- **branch (b)**, a read artifact, **voids the snapshot for exactly the thread that matters.**

**Either way the leg ends uncertified, so another round would buy explanation, not decidability.**

**And the diminishing-returns diagnosis is exact:** *"five packets plus preflights with each fix revealing the
next caveat."* **That is what happened** — and **the ceiling existed precisely to stop it.** **The Session
accepts this without reservation.**

## The read-path leg is DEAD, not deferred — a distinction worth recording

**The Advisor's parenthetical is a finding in its own right:** read-path needs **complete canonical coverage**,
which is **now unachievable with any available instrument** — **DR is dead, and the page-guard was already
redirected away for its single-step races.** **So `A2h-terminal-read-path-audit` is not merely unreached; it is
unreachable on this line.** **That should be stated rather than left as a silent gap.**

## What survives — the complete list

| Survives | Status |
|---|---|
| **28-alias page-protection census** | **complete** — `armed=1 mapped=28 protected=28`, `touched_count=0`, **no DR dependency** |
| **Mapping stability** | certified, re-proved at the terminal |
| **Two independent terminal software zero reads** | the generated read and the hook read, both zero |
| **Install software control** | `80000115 → FE000104`, `install_ok=1` |
| **`O-NO-BOUNDARY-TRANSITION` and the triage rows** | **stand** — they never touched DR |
| **Producer line** | **PARKED** — no link evidence either way |

| Unreachable on this line | Reason |
|---|---|
| **Canonical-write attribution** (guest / host / transient-as-write) | the only instrument for it is uncertifiable |
| **The DR leg of the read-path audit** | **DEAD, not deferred** |

**Per the Advisor: *"never carry the channel as decoration."*** **No row may cite a DR record in either
direction — including the zero-`#DB` series.**

## What happens to the DR code itself — a disposition, not a deletion

**The Advisor's rule governs ROWS, not code.** **The DR instrumentation remains in the tree and is INERT:**

| Property | State |
|---|---|
| **Default** | **OFF** — every DR path is behind `JSRF_TRACE_A2H_DR`, read once and cached |
| **Behaviour with the gate absent** | **identical to before the instrument existed** — verified by record-level inertness in every OFF run |
| **Effect on any current or future row** | **none permitted** — no row may cite a DR record in either direction |
| **Authorized for removal?** | **NO** — deleting a large body of instrumentation is a **destructive change requiring its own packet**, and no packet has authorized it |

**So the code stays, inert and unused, and the *decision* rule is what prevents it from lending false
authority to a row.** **That is the operative meaning of "never carry the channel as decoration."**

**Recorded because the two readings differ materially:** removing the code would be a **destructive action
taken without authorization**, while leaving it gated off satisfies the Advisor's rule exactly.
**The Session does not delete it.**

## The durable gains — recorded because they are real

**Five packets and their preflights produced, beyond the negative result:**

1. **The `#DB` delivery premise is PROVED on this host** — a genuine debugger-delivered `#DB` claimed from
   `DR6.B0`. **A long-standing open question is closed permanently.**
2. **The gate defect is fixed and demonstrated** — `complete=1` where it was structurally 0, and a fixture
   yielding `decision=NON_FIRING` that was **impossible** before.
3. **The contradiction is LOCALISED to a named thread** — sharper than *"zero hits, cause unknown."*
4. **Six defects were found by measurement** across the line, including **route-counter pollution**
   (`ss_routed_generic_first=14405` against `raw_single_step=0`), a **toolkit gate coupling** that would have
   made `NON_FIRING` unreachable *again* through a second env var, the **`cleared=17` overcount**, the
   **handshake-fatal VEH**, the **always-true DR6 test**, and the **wrong terminal-witness gate**.
5. **A durable practice**, now in `docs/jsrf-operating-history.md`: *a frozen packet's stated uncertainty is a
   scheduled verification task, not a carried caveat.* **The frozen packet predicted this failure in writing
   and it sat as a caveat through three packets.**
