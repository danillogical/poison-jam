# \A2h-arming-coverage-repeat-r1\ — execution evidence: **K = 3, agreement complete, ROW \O-COVERAGE\** (Planner reversed \O-READ-PATH\)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-arming-coverage-repeat-r1`, frozen
**`4AC455E7F427ABCD9AEB6A81FA34B2A570A826964A6FB76EC3478ADA4B1174A1`** (15 lines).

## Identity — verified BEFORE any ON run, per packet line 5

| Artifact | Pinned | Measured | Result |
|---|---|---|---|
| `jsrf_recomp.exe` | `A7E32464…ACDEAD9` | **identical** | **MATCH** |
| `jsrf_collect.exe` | `95440EC4…676B3EF7` | **identical** | **MATCH** |
| `default.xbe` | `FD190557…F0F3EF9C` | **identical** | **MATCH** |

**Source deltas since `d369880` (game) and `571982d` (toolkit) are EMPTY for `src/`, `tools/` and
`CMakeLists.txt`** — documentation-only, so **no binary-affecting change.** **The OFF control legitimately
carries; no fresh OFF was taken**, exactly as the packet and the Advisor required.

## The five new ON runs

| # | Run | Terminal | Anchors | Coverage | Class |
|---|---|---|---|---|---|
| 1 | `…035337-386-a2h-repeat-on-1` | **none** — `diagnostic_deadline` (exit 3) | **NOT ANCHORED** (no OOM) | complete | **NOT A REALIZATION** |
| 2 | `…035402-315-a2h-repeat-on-2` | **`0x00000000@0014982E`** | PASS | complete | **TARGET** |
| 3 | `…035409-942-a2h-repeat-on-3` | `0x41200000@00147D36` | PASS | complete | NON-TARGET |
| 4 | `…035417-757-a2h-repeat-on-4` | `0x00000000@00147DBC` | PASS | complete | NON-TARGET |
| 5 | `…035425-633-a2h-repeat-on-5` | **`0x00000000@0014982E`** | PASS | complete | **TARGET** |

**Run 1 is `diagnostic_deadline`, not a terminal failure** — capture was bounded at 8 s **before any fatal
terminal**, so it never reached the OOM and has **no identity-1 prefix.** **It is not a realization in the
packet's sense and is excluded**, not scored as a NON-TARGET. **Recorded because the distinction matters: a
bounded capture is not a failure mode.**

**A Session parsing bug, caught and corrected:** the first classification script read
`GUEST_DR_ARM_RECONCILE`'s **group 1 (`arm_tid_list`)** while intending **`arm_tid_overflow`**, so **every run
with a non-zero tid list was scored "coverage incomplete."** **The records were always fine; my check was
wrong.** Corrected, and the corrected test is the one reported above.

## Coverage — complete in every anchored run

`GUEST_DR_ARM_TERMINAL arms_recorded=17 armed_at_or_after_handshake=17 arm_attempt_failures=9
collision=0 create_thread_events=16`, and `GUEST_DR_ARM_RECONCILE arm_tid_list=17 arm_tid_overflow=0
distinct_armed_tids=17`. **Census `armed=1 mapped=28 protected=28`, `touched=0`, `publish_failed=0`** in all.
**Install control positive** (`80000115 → FE000104`, `ok=1`) in all. **Zero `GUEST_DR_HIT`** in all.

## K = 3 — and the mechanical agreement is COMPLETE

**Per the Advisor's K-scope ruling (i), the pinned r2 run 2 supplies K = 1**, and this set adds **two** new
coverage-complete TARGETs (runs 2 and 5). **K = 3 ≥ 2.**

| Component | Pinned r2 run 2 | New run 2 | New run 5 | Agree |
|---|---|---|---|---|
| **Terminal triple** (slot, target, live) | `001c4064 / 00000000 / 00000000` | **same** | **same** | **YES** |
| Site | `0014982E` | `0014982E` | `0014982E` | **YES** |
| **Install control** | `80000115 → FE000104`, `ok=1` | **same** | **same** | **YES** |
| **Census** | `armed=1, 28, 0FFFFFFF/0FFFFFFF, touched=0` | **same** | **same** | **YES** |
| DR hits | 0 | 0 | 0 | YES |
| **Positive writer-class witness** | **none** | **none** | **none** | *(see below)* |

**So the three realizations agree on every observable the instrument records**, and the agreement is
**mechanical and pre-specified** as the packet requires — assessed from **both raw artifacts**, not from prose.

---

## ⚠ ROW **SUPERSEDED** — the Planner REVERSED `O-READ-PATH` to **`O-COVERAGE`**

**The Planner's first ruling (below) selected `O-READ-PATH`. It then REVERSED that ruling on the
`PREMISE_CHANGED` from the DR0 positive control failure.** **The Session verified both of the Planner's source
citations and they are exact.**

**The predecessor frozen packet ALREADY required a native DR0 record and said the software control cannot
substitute** — `docs/packets/a2h-slot-read-path-displacement.md:27`:

> *"The install write **MUST cause a native DR0 record** … **the frozen latch's own `install_seen=1`,
> `install_ok=1`, raw=`0x80000115` and installed=`0xFE000104` are required IN ADDITION TO the trapped write**,
> not inferred from a printed line. The old `jsrf_slot_latch_install` callback at `:9325-31` occurs **BEFORE**
> that store and **cannot itself count as a trapped-install witness**."*

**And a missing trapped install selects `O-COVERAGE` — the packet SAW THIS COMING** —
`docs/packets/a2h-slot-within-run-attribution.md:19`:

> *"**A missing canonical install trap is `O-COVERAGE`, never no-write: live `#DB` chance semantics and
> all-thread arming have not yet been proven in the game.**"*

**So the frozen contract anticipated exactly this outcome and prescribed the row in advance.**

**The Planner's own words:** *"I incorrectly treated software `install_ok` as a positive DR firing control.
Aliases + reads remain certified but cannot satisfy the required canonical/DR leg; K=3 reproducibility of
terminal/control/census, not a certified writer row."*

### The corrected row

| Row | Status |
|---|---|
| **`O-READ-PATH`** | **WITHDRAWN** — it required the DR leg to contribute, and the DR leg has never certified anything |
| **`O-COVERAGE` → `A2h-slot-write-coverage-provenance`** | **SELECTED** — per the packet's own line 19/25 trigger |

**What `K = 3` still buys, unchanged: REPRODUCIBILITY** of the terminal triple, the install control and the
census across three independent realizations. **It does not buy a certified writer row, and it never did.**

**The first ruling is retained below, superseded, per this line's rule that corrections name what they
supersede.**

---

## Superseded — the Planner's first ruling, `O-READ-PATH` *(withdrawn above)*

**Planner ruling (frozen-contract interpretation):** `docs/reviews/a2h-repeat-row-interpretation.md`,
committed `1b0a7b8`. **The Session escalated the tension rather than deciding it; the Planner ruled, and its
reasoning is the substantive part:**

> *"leg 3 is conditionally reconciled DR/mapping evidence, not a requirement for a terminal-gap DR hit; a
> post-hit read is required only if a hit occurs. Positive trapped install/17 arms/28 protected aliases/terminal
> mapping and record reconciliation certify the observation surface independently in all three runs; zero hits
> alone do not."*

**And the decisive point, which the Session had missed when it escalated:** *"Requiring a terminal-gap DR hit to
select the expressly no-zero-write-observed audit row makes that row unreachable in its intended case."*
**A row defined as "we observed no zero-write, with coverage proven" cannot require a zero-write observation as
its own precondition.** **The gate the Session read as unmet was a gate that would have made the row
self-defeating.**

### The intended reading — a COVERAGE claim, not a no-write claim

> *"'No-zero-write coverage' was intended as coverage within which no zero-write was observed, not a no-write
> assertion; writer and mechanism `UNKNOWN`, no claim of miscompile or actual discrepancy."*

**So `O-READ-PATH` asserts:** the terminal zero is **consistent with a discrepancy warranting audit**, with
**complete positively certified coverage** of the observation surfaces, **within which no zero-write was
observed.**

**It does NOT assert:** that the slot **was never written**; **who** wrote it; or that a **read-path fault or
miscompile** is proven. **Writer `UNKNOWN`. Mechanism `UNKNOWN`.** **Generality is limited to the agreed row,
not extended to a no-write conclusion.**

### Why K = 3 matters even though the row is silent on the writer

**Three independent realizations — the pinned r2 run 2 and new runs 2 and 5 — agree on the terminal triple,
the install positive control, and the full census.** **That establishes REPRODUCIBILITY of the observation
surface**, which is what licenses the audit referral. **It does not upgrade silence into attribution**, and the
ruling says so explicitly: *"K=3 establishes agreement on that limited row, not a no-write conclusion."*

### A prospective wording fix the Planner requires

**The Planner confirms the frozen wording is *"genuinely ambiguous"* and needs correction.** **The Session is
the second reader to conflate the row predicate with its gate**, and the predicate and gate live in
**different packets**. **The fix, for future packets:**

> state explicitly that **"no-zero-write" means *no zero-write OBSERVED WITHIN POSITIVELY CERTIFIED COVERAGE***,
> and that **post-hit reconciliation is CONDITIONAL on a hit.**

**Recorded as a durable wording requirement rather than a silent correction** — the frozen packets are not
edited, and the clarification belongs in the successor's contract.

### The general lesson the Planner drew, recorded because this line keeps meeting it

> **"A positive demonstration that observation channels were continuously available makes 'nothing observed on
> certified channels' narrower than 'nothing happened.'"**

**That is the absent-record-as-negative boundary stated precisely**, and it is the third time this line has had
to draw it — including **twice by the Session in one afternoon.**

**No sixth run** — `N = 5` was pre-specified and this set is complete. **No synthetic completion.** The
producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not**
reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
