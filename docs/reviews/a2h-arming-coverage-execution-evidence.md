# `A2h-arming-coverage-attribution-r2` — execution evidence: **`O-COVERAGE`** on the K ≥ 2 requirement

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-arming-coverage-attribution-r2`, frozen
**`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`** (23 lines).
**Runs:** one **fresh OFF** + **five** ON, same build, `--profile strict --seconds 8`.

| # | Run directory | Terminal set | Class |
|---|---|---|---|
| 1 | `…030855-953-a2h-arming-coverage-on-1` | unresolved `0x001D5078` | **NON-TARGET** |
| 2 | `…030924-722-a2h-arming-coverage-on-2` | **`0x00000000@0014982E`** | **TARGET** |
| 3 | `…030932-467-a2h-arming-coverage-on-3` | `0x3E800000@00147DE2` | **NON-TARGET** |
| 4 | `…030940-467-a2h-arming-coverage-on-4` | `0x41200000@00147D36` | **NON-TARGET** |
| 5 | `…030948-272-a2h-arming-coverage-on-5` | unresolved `0x001D5078` | **NON-TARGET** |

**All five share the deterministic anchors** — OOM `598869040` / `12715008` / `50855936`, status
`0xC0000017`, identity-1 prefix `5555`. **Install control passed in all five** (`install_raw=80000115`,
`install_value=FE000104`, `install_ok=1`). **Census complete in all five** (`armed=1 mapped=28
protected=28`).

---

## THE FIX WORKED — arming is now fully visible, and it was 17 all along

**This is the headline, and it settles the question the previous packet could not.**

| Record | Value, **identical in all five runs** |
|---|---|
| `GUEST_DR_ARM_TERMINAL` | `arms_recorded=17 armed_before_handshake=0 armed_at_or_after_handshake=17 arm_attempt_failures=9 collision=0 create_thread_events=16` |
| `GUEST_DR_ARM_RECONCILE` | `arm_tid_list=17 arm_tid_overflow=0 disarm_cleared=17 distinct_armed_tids=17` |
| **`GUEST_DR_ARM_OK` lines** | **17 per run** — 10 `why=handshake`, **7 `why=create_thread`** |

**Seventeen threads were armed, and seven of those arms were POST-HANDSHAKE and therefore completely
invisible in the old record.** The old `armed=10` was exactly the handshake snapshot; the truth was 17.

**The Session had twice failed to establish this number by inference — first reading the tid list as the
armed set (undercounts), then reading `cleared=17` as the armed set (overcounts).** **The reporting fix
settles it directly: `arms_recorded=17` with 17 individual `GUEST_DR_ARM_OK` records, each carrying its own
DR7 readback.** **The Advisor's ruling that "only positive per-arm records will do" is vindicated by
measurement.**

**`disarm_cleared=17` now happens to equal the armed count — and the record explicitly refuses to let that
be read as the arm count** (`disarm_cleared_is_not_an_arm_count=1`). **The coincidence is exactly why the
guard is needed.**

**`GUEST_DR_ARM_PRE_MAPPING_BOUND`** reports `decision=decidable_from_records` in all five, with
`births_before_mapping=9 births_before_handshake=9 exits_before_handshake=0 pre_mapping_exits=0`. **The
pre-mapping-exit window is EMPTY**, consistent with the Session's pre-run measurement — and the record
carries the `EXIT_THREAD_DEBUG_EVENT_is_not_delivered_to_a_DEBUG_ONLY_THIS_PROCESS_debugger` caveat rather
than implying completeness.

**Zero `GUEST_DR_HIT` in all five runs** — the DR0 watch recorded **no canonical-slot write**.

---

## The row: **`O-COVERAGE`** — on the K ≥ 2 requirement

**Observed TARGETs: 1 of 5. Coverage-complete TARGETs: K = 1. Coverage-disqualified: 0.**

**The packet requires K ≥ 2 for general attribution**, and: *"K=1 preserves only its run-local row and
`UNKNOWN` generality."* **So no attribution row is selected, and generality is `UNKNOWN`.**

**Why this differs from the previous packet's 3/5:** **the terminal outcome is nondeterministic**, as the
earlier work established. The previous five draws gave three targets; these five gave one. **The target rate
is not stable across run sets, which is precisely why the packet pre-specified a bounded N and an
anti-optional-stopping rule rather than assuming a yield.**

**Critically: this is NOT a coverage failure.** **Every one of the five runs is coverage-complete** — 17/17
arms visible, census 28/28, install trap positive, reconciliation complete, pre-mapping bound decided.
**The instrument now does what the packet asked of it.** **What failed is the YIELD: only one of five draws
was a TARGET.**

**And the packet's own rule applies exactly:** *"If zero targets in five, report observed and qualifying
rates `0/5`, STOP and re-refer; never extend N without Advisor referral."* **Here K = 1, not 0 — so the rule
for K=1 governs: preserve the run-local row, `UNKNOWN` generality, and re-refer.** **No sixth run.**

---

## What run 2 (the single TARGET) shows

**Run 2 is a coverage-complete TARGET realization.** Its per-run facts:

- terminal `0x00000000@0014982E`, the raw-zero read of the slot the whole A2h line is about;
- **17/17 arms recorded**, census **28/28, zero touches**, install trap positive;
- **zero DR hits** — no canonical-slot write was trapped;
- reconciliation complete, pre-mapping bound decided.

**Per the packet, that is a run-local finding and not a general one.** **No writer is named, and no
attribution row is selected**, because **K = 1 cannot support a general claim and the packet forbids
forcing one.** **Recorded as contrastive evidence toward a future, better-yielding design.**

---

## A latent break found while satisfying the packet — and it was mine

**The packet required rerunning the collector harness probes. They FAILED** — and **not because of this
packet.**

**`scripts/test-harness.py` pinned the registry version twice, both correct only while the version was 1:**

1. its regex matched the literal `version=1`, so a v2/v3 registry produced **`missing registry or registry
   overflow`** — **an assertion whose message blames the DATA when the TEST is stale**;
2. it asserted the registry's first dump word `== 1` — **and the struct begins with `uint32 version`, so
   that word IS the version.**

**The version moved 1 → 2 at `009f624` and 2 → 3 at `f5b709d`, so the harness has been failing since
`009f624` — BEFORE this packet — and nothing surfaced it because the guard suite never runs it.**

**Fixed** (`2c8765c`) to capture the version rather than match a literal, and to compare the first word
against **the version the collector itself reported** — **strictly stronger than a constant**, because it
ties the dump contents to the archived text. **All 19 harness probes now PASS.**

**Recorded as a process finding:** **a script the guard suite does not run can rot silently across many
commits.** This one was found only because a packet happened to require it. **The Session flags whether
`test-harness.py` belongs in the guard set as a workflow decision, and does not make it unilaterally.**

---

## Prohibitions and status

**No synthetic completion.** No guest semantics changed; the APU trap and `0x80` untouched; no allocation
faked; arena not widened; NULL call not bypassed; guest error handling not edited. **No `src/recomp/gen/*.c`
edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**. **No sixth run.**
