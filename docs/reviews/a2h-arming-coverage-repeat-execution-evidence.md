# `A2h-arming-coverage-repeat-r1` — execution evidence: **K = 3, agreement complete, ROW PENDING INTERPRETATION**

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

## ⚠ THE ROW IS NOT SELECTED — a genuine interpretation question, escalated rather than decided

**The packet says in terms: *"never infer a no-write or named writer from no DR hit"* and *"shared silence is
insufficient."***

**Every writer-facing observable is SILENT in all three runs:** no DR hit, no alias touch, **no positive
class witness** (`GUEST_SLOT_CLASS` absent). **The agreement is therefore agreement on silence plus the
terminal triple and coverage controls — which the packet explicitly says cannot certify a writer.**

**But the inherited row set contains `O-READ-PATH`, whose predicate is: *"Complete continuous no-zero-write
coverage, stable mapping, generated target AND hook read zero, DR/mapping leg reconciled"* — and every one of
those components IS present:**

- **complete continuous coverage** — 17/17 arms, no gaps, no overflow;
- **stable mapping** — re-proved at the terminal;
- **generated target AND hook read zero** — `target=00000000` **and** `live=00000000`, two independent reads;
- **DR/mapping leg reconciled** — `arm_tid_list == arms_recorded`, overflow 0.

**So there is a real tension between two rules the packet states:**
- the row predicate for `O-READ-PATH` **appears satisfied**, and
- the packet **forbids inferring "no write" from no DR hit**, and `O-READ-PATH`'s first clause **is** a
  no-write claim.

**Both readings are defensible and they select different rows** (`O-READ-PATH` → `A2h-terminal-read-path-audit`
versus staying `O-COVERAGE`). **The Session is NOT deciding this privately.** Per `docs/agent-workflow.md`, a
frozen-contract interpretation question goes to the Planner, and to the final adjudicator if it remains open.
**The evidence above is what that decision needs, and it is complete.**

## What is NOT in question

**K = 3 ≥ 2 is MET**, so the packet's central requirement is **satisfied for the first time in this line.**
**The instrument is coverage-complete, the identity held, the OFF carried, and three independent realizations
agree on the terminal triple, the install control and the full census.**

**No sixth run** — `N = 5` was pre-specified and this set is complete. **No synthetic completion.** The
producer line stays **PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not**
reopened; `0xFFFFB3` stays **`UNRESOLVED`**.
