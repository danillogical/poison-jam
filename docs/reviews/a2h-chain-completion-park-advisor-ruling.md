# Advisor ruling — the chain-completion result: PARK static, go LIVE on the slot write

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e8d1-220c-7000-9a7a-d2817ca68c66`.
**Raised because:** the chain-completion packet returned `O-OPEN` and **PARKED** per the terminality bound,
and the remaining edge needed a scope ruling.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: park the static reader-hunt permanently as mooted (not pending); successor is ONE live slot-write packet (preflighted), not another static round; no §6.1 addition beyond a one-line caution.**

**1. PARK the six-table-reader edge — and DSOUND coverage with it.** The edge mattered only as a route to ARG1; **live observation reads ARG1-at-fault directly from the fault address, mooting the route.** No packet closes DSOUND coverage: it would serve a question live measurement answers better, and **563k uncovered bytes stay a stated limit of the static corpus, not a work item.** Reopen static hunting only via re-referral if live observation proves infeasible.

**2. Successor targets the WRITE EVENT live — neither competitors-static nor reader-first.** The (a)/(b) fork is empirically decidable only by observation: **a watch on the slot address catches writer RIP + value + order in one measurement, deciding coincidence-vs-competitor directly.** Static cannot (values, order, and execution are runtime facts; exclusions already exhausted their static budget). **Requirements:** address-based observation (alias-agnostic — no object theory needed); base-stability re-check (`MEM32(0x19DCE0)` at arm + terminal, else re-scope); **DR-canonical + alias-census per NULL-line patterns** (positive controls incl. installer trap, loss accounting, fail-closed, off-by-default, closure); **preflight mandatory (new toolkit scope).** Competitor list becomes interpretation frame, not search space; reader linkage stays downstream. — **And the Session's component-bounds idea: sound test, wrong vehicle.** Check clamp/source bounds as hour-scale *analysis* if cheap (bounds-exclude ⇒ exoneration finding, no packet; bounds-admit ⇒ uninformative, don't packetize it either).

**3. No §6.1 addition.** Nothing shows an accepted result scoped to `.text` — the ModRM miss was encoding, not section, and packets already mandate X-flagged sweeps (adequacy enforces per-packet scope; that mechanism suffices). **Record one caution line in the review record** (hot code lives in D3D here; verify section roles, never assume). Legislating against an undemonstrated harm would be rule-bloat two turns after writing the rule.

**BASIS:** observed — edge table + gaps as recorded (reader unlocated; range unread; 74.98% with DSOUND 33%); chain assertions vs open index/value/order in the same record; packet bar + terminality texts. Inferred — static exhaustion as a class (every remainder is a runtime fact or lives in undecoded bytes).

**REVERSED BY:** live observation proving infeasible (re-scope, not auto-chain); a static route to runtime values (contradiction in terms — reject).

**RECORD IN:** verbatim in the line's review record; **`O-OPEN` stands with the edge now marked SUPERSEDED-BY-DESIGN (not pending)**; successor brief carries preflight + terminality (closes writer or parks with precise edge, never auto-chains).

---

## The insight that makes the PARK correct

**The Session had framed the six-table-reader as the next static target.** **The Advisor's reframing is
decisive:**

> **"The edge mattered only as a route to ARG1; live observation reads ARG1-at-fault directly from the fault
> address, mooting the route."**

**The Session was treating the reader as an END rather than as a MEANS.** **Its only purpose was to learn
`ARG1` — and `ARG1` is sitting in a register at the fault, which a live watch reads directly.** **So closing
DSOUND's coverage would spend a packet to answer a question that one measurement answers better.**

**And the Advisor's *"static exhaustion as a class"* reasoning is the general form:** ***"every remainder is a
runtime fact or lives in undecoded bytes."*** **That is why the PARK is PERMANENT rather than pending.**

## ⚠ The Session's component-bounds idea — accepted as analysis, refused as a packet

**The Session proposed:** **since item 3 traced the reaching definitions, a successor could ask whether the
four quantized components can take the values `0,29,80,120` — and if they cannot, the store is EXONERATED.**

**The Advisor's ruling:** ***"sound test, wrong vehicle."*** **Check it as hour-scale ANALYSIS if cheap —
*"bounds-exclude ⇒ exoneration finding, no packet; bounds-admit ⇒ uninformative, don't packetize it either."***

**That is the right calibration, and the Session notes it had over-weighted the idea into a packet.**

## The successor's requirements — all mandatory

| Requirement | Why |
|---|---|
| **address-based observation, alias-agnostic** | **no object theory needed** — the slot address is known |
| **base-stability re-check** | **`MEM32(0x19DCE0)` at arm AND terminal, else re-scope** |
| **DR-canonical + alias-census per NULL-line patterns** | **positive controls incl. an installer trap, loss accounting, fail-closed, off-by-default, closure** |
| **PREFLIGHT MANDATORY** | **new toolkit scope** |
| **terminality** | **closes the writer, or parks with a precise edge — never auto-chains** |

**⚠ And the Session notes the DR caveat it must raise in the preflight:** **the NULL line DROPPED its DR leg
at the terminal ceiling** — **`device+0x2268` was refuted and the delivery premise was proved, but the DR0
watch never fired and the leg was retired.** **So a DR-based watch inherits that history and the preflight
must address it.** **The ALIAS CENSUS survived the NULL line** (page-protection based, no DR dependency), **so
the census route is the uncertified-but-unretired one.**

## The one-line caution, recorded here rather than legislated

> **HOT CODE LIVES IN `D3D` IN THIS IMAGE — VERIFY SECTION ROLES, NEVER ASSUME.**

**The Advisor refused a §6.1 addition on sound grounds:** *"Nothing shows an accepted result scoped to
`.text` — the ModRM miss was encoding, not section, and packets already mandate X-flagged sweeps."* **And:**
***"Legislating against an undemonstrated harm would be rule-bloat two turns after writing the rule."***

**The Session accepts that.** **The caution is recorded; no rule is added.**

## What stands after this ruling

| Item | Status |
|---|---|
| **Row `O-OPEN`** | **STANDS**, with the edge **SUPERSEDED-BY-DESIGN, not pending** |
| **the six-table-reader edge** | **PARKED PERMANENTLY** — mooted by the live route |
| **DSOUND's 33.07% coverage** | **a STATED LIMIT of the static corpus, not a work item** |
| **the store `0x00199F45`** | **still a CANDIDATE** — instruction confirmed, slot reachability not |
| **the BYTE TUPLE dataflow finding** | **stands** — and the (a)/(b) fork is now decidable by observation |
| **`O-OPEN`'s open links** | **index, value, order** — all three are RUNTIME facts |
