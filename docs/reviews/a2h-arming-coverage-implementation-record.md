# `A2h-arming-coverage-attribution-r2` — implementation record

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-arming-coverage-attribution-r2`, frozen
**`80E9425977547BC1EE44DD95645F72ACF4F7941857A39B6DB67B427E7DD81D84`**.
**Implemented by:** Worker `f5fbab71-db53-4489-ab02-4a1bd1ed8df1` (`workbuddy-ai/deepseek-v4.1-flash` @ `max`).
**Authority:** `docs/reviews/a2h-arming-coverage-advisor-reruling.md` — the **reporting-only** ruling.

| Item | Value |
|---|---|
| Game commits | `9231969` (C1/C2) · `0ee4d7e` (terminal `last_fail_reason`) |
| Toolkit commit | `571982d` (fixture `tests/collect_arming_fixture_test.c`) |
| **Session-verified build** | **success** on a settled tree |
| **Session-verified ctest** | **20/20** (was 18; the two new arms are `jsrf_collect_arming_gate_on` / `_gate_off`) |
| **Session-verified guards** | **all nine `OK`** |

## The fix is reporting-only — **Session-verified, not taken on trust**

**No new arming mechanism.** The Session diffed the mechanism lines old vs new:

```
git -C xboxrecomp diff 5528d00..571982d -- src/  | grep -E "SetThreadContext|A2H_DR7|Dr0 =|dr_arm_all|dr6"
git diff be2149b..HEAD -- tools/harness/collect.c | grep "^-" | grep -E "SetThreadContext|A2H_DR7|Dr0 =|dr_arm_all|dr6 &"
```

**Both empty.** `SetThreadContext`, the `Dr0`/`Dr7` writes, the DR7-owned readback compare, the
`dr6 & 0x1` ownership test and `dr_arm_all()` are **identical**; only `fprintf`s and a relocated
`dr_arm_thread` call differ. **No queue, no drain, no second sweep.**

## New record families

| Line | Purpose |
|---|---|
| **`GUEST_DR_ARM_OK`** | **every SUCCESSFUL arm at the moment it happens** — with `seq`, `tid`, `why`, `dr0`, **`dr7_readback`**, `canonical`, `ticks`, **`phase`** |
| **`GUEST_DR_ARM_TERMINAL`** | the **terminal full-list summary** over the whole process lifetime |
| **`GUEST_DR_ARM_RECONCILE`** | **refuses `cleared=` as an arm count** (`disarm_cleared_is_not_an_arm_count=1`) |
| **`GUEST_DR_ARM_PRE_MAPPING_BOUND`** | the derived pre-mapping-exit decision |
| **`GUEST_DR_BIRTH` / `_ROW` / `_DROPPED`** | every `CREATE_THREAD` event, one row per lifecycle event |
| **`GUEST_DR_ARM_TID_TERMINAL`** | terminal per-arm list with source |

**Existing shapes unchanged** (`GUEST_DR_ARM`, `GUEST_DR_ARM_TID`, `GUEST_DR_ARM_FAIL`, `GUEST_DR_HIT*`), and
`GUEST_DR_DISARM` gained `cleared_means=live_threads_zeroed_at_teardown_not_arms`.

**`phase=` is deliberately distinct from `why=`** — `why=` names **which path** armed
(`handshake`/`create_thread`), `phase=` names **when** (`at_handshake`/`post_handshake`). **The sweep's arms
read `at_handshake`**, because calling them `post_handshake` would blur the exact population that used to be
invisible. **The coverage claim is `armed_at_or_after_handshake`.**

## Session verification of the fixture — all substantive cases pass

Run directly with the gate on, **exit 0**, including the cases that matter most:

- **post-handshake silent success made visible** — *"the record is named `post_handshake`, the population
  that was silent"*, with **its own DR7 readback**, and *"the real thread really is armed"*;
- **recovery** — *"the deferred tid has BOTH a deferred birth record and a handshake arm record"*;
- **live sweep** — *"the pre-existing live thread carries DR0 and L0 after the handshake"*;
- **DR ownership/collision** — *"the other owner's DR1 is intact and DR0 was never taken"*;
- **readback** — *"the record carries the DR7 actually read back, not the value written"*;
- **pre-mapping exit** — *"yields `PRE_MAPPING_EXIT_UNCOVERED`, never absence"*;
- **compatibility** — *"the pre-existing `GUEST_DR_ARM_TID` line shape is unchanged"*.

### Record-level inertness — **Session-verified by measurement, not by assertion**

```
gate OFF -> artifact: 0 bytes
gate ON  -> artifact: 4829 bytes
```

**The gate drives every record, and with it off the artifact is empty** — inertness at the **record** level,
which is the standard the packet demands rather than log silence.

## The Worker's three findings — each recorded because it would have shipped wrong

**1. The fixture caught a real defect in the Worker's own first draft.** It had classified a tid that
**exited before it could be armed** as `failed_never_recovered` — **the same conflation C1(4) forbids**. A third
population, **`exited_unarmed`**, now exists. **Without the fixture this would have shipped as an overcount of
terminal unarmed tids — the exact error class of the Advisor's re-ruling.** *The Session had just committed
that error class twice by hand; the fixture caught it mechanically.*

**2. `phase=` vs `why=`** — see above.

**3. The handshake gets its own seq, stamped BEFORE the sweep**, so the sweep's own arms are correctly ordered
after it. Consistent with the Session's measurement that the handshake sits at log line ~48 of ~35,000.

## Carried uncertainties — stated, not hidden

**1. `EXIT_THREAD_DEBUG_EVENT` is not delivered to a `DEBUG_ONLY_THIS_PROCESS` debugger**, so **exit rows are
not expected in a real run**, and **the exit population is therefore not provably complete.** The bound
**defaults to `UNKNOWN`** and the record **says so explicitly rather than implying coverage.** **The Session's
measurement (zero pre-handshake dispatch in all five runs) is consistent with `EMPTY`/`NO_PRE_MAPPING_EXIT`,
but the record defaults to `UNKNOWN` so it cannot be read as a guarantee for future runs.** **Correct
behaviour.**

**2. The fixture proves the instrument's RECORDS, not live-game coverage.** It cannot show the real collector
delivers these records against the real debuggee — **only a run can, and the Session owns the runs.**

**3. The Worker did not run the game or `scripts/test-harness.py`** (it launches `run-jsrf.py`, so it is a game
run). **Harness probes remain unrun.**

**4. Fixture tids `70002`/`70003` are synthetic by design** (a fabricated exit and a collision state); **the
recovery, sweep and post-handshake cases use REAL thread ids**, which is why those three are load-bearing.
