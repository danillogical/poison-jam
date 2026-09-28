# `A2h` run-local TARGET record — run 2 of `A2h-arming-coverage-attribution-r2`

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Named at the Advisor's direction** (`docs/reviews/a2h-null-line-yield-advisor-ruling.md`, Q4): *"YES, name
run 2 — as a run-local record with exact boundaries."*
**Run:** `logs/runs/20260928-030924-722-a2h-arming-coverage-on-2`, `--profile strict --seconds 8`, build
`a7e3246…`.

**This record claims a run-local realization and nothing more.** It **names no writer** and **asserts no
read-path conclusion** — per the Advisor, those *"need K ≥ 2 by the same rule."* **Generality: `UNKNOWN`.**

---

## The realization

| Property | Value |
|---|---|
| Terminal | **`0x00000000@0014982E`** — the raw-zero read of the thunk slot |
| Slot | `0x001C4064` (ordinal 277, `RtlEnterCriticalSection`) |
| Classification | **TARGET**, uniquely — exactly one fatal terminal, no competing terminal |
| Deterministic anchors | OOM `598869040` / `12715008` / `50855936`, status `0xC0000017`, identity-1 prefix **`5555`** |
| Install positive control | **`install_raw=80000115`, `install_value=FE000104`, `install_ok=1`**, index 65 |

## Coverage — complete

| Leg | Value |
|---|---|
| **Arms recorded** | **17** — 10 `why=handshake`, 7 `why=create_thread` |
| Reconciliation | `arm_tid_list=17 arm_tid_overflow=0 distinct_armed_tids=17 incomplete=0` |
| Recovery | `recovered_by_sweep=9 failed_never_recovered=0 exited_unarmed=0` |
| Alias census | **`armed=1 mapped=28 protected=28`**, mask `0FFFFFFF/0FFFFFFF` |
| **Alias touches** | **`touched_count=0`, `publish_failed=0`** |
| Pre-mapping bound | `decision=decidable_from_records`, window `NO_PRE_MAPPING_EXIT` |

## Observations

**Zero `GUEST_DR_HIT`** — the canonical DR0 write watch recorded **no canonical-slot write**.

**The terminal zero is real and the mapping is re-proved.** The terminal hook read the live slot at the moment
of the fatal call and recorded `live=00000000`, and the same value is independently corroborated by the
generated `MEM32(0x1C4064)` read that dispatched the call.

## What this record asserts

**Consistent with a read-path discrepancy; writer `UNKNOWN`; generality `UNKNOWN`.**

**Precisely:**
- the slot **was zero** at the terminal read — **observed, not inferred**;
- **no write was trapped** by the DR0 watch, and **no alias was touched**;
- the coverage legs are **complete**, so the absence of a trapped write is **not** a coverage artifact.

**It does NOT assert that no write occurred**, because the watch covers the **canonical** address on
**armed** threads and **one realization cannot establish that the armed set was exhaustive of every writer at
every instant.** **That is exactly the claim the K ≥ 2 requirement exists to gate.**

## Why this record exists

Per the Advisor: the packet's own K=1 language (*"preserves only its run-local row"*) **demands preservation**,
and **the next packet needs this as the agreement comparator and template.** **The successor's early stop fires
only when two coverage-complete TARGETs AGREE — so this record is the first half of that comparison.**

**Its value is that it is the strongest single realization this line has produced:** a coverage-complete
TARGET with a complete 28-mirror census showing zero touches and zero trapped canonical writes.
