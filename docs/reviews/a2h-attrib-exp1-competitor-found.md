# ✅✅ `A2h-slot-writer-attribution-r2` — **Exp1: THE COMPETITOR IS FOUND** — a second writer put `0x001D5078` in the slot

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-attribution-r2`, frozen
**`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`**.
**Runs:** fresh OFF `…120818-632-a2h-attrib-inert-off` (**inert**) + **Exp1** `…120832-477-a2h-attrib-exp1`.

---

## ✅ THE REQUIRED POSITIVE CONTROL **FIRED** — for the first time on this line

```
GUEST_SLOTW_EVENT index=0 seq=1 kind=1 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=00000000 tid=27444 range=1 form=1 rip=00007FF60716D82D
GUEST_SLOTW_EVENT index=1 seq=2 kind=2 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=0015F9D0 tid=27444 range=1 form=1 rip=00007FF60716D830
```

**`installer_control_hits=2`.** **`range=1` = `GAME_MODULE`.** **`post=0x0015F9D0` = the installer's
constant.** ✓

> **So the range-based classifier placed the installer's RIP correctly, the control fired, and the packet's
> *"no hit ⇒ INFRA FAILURE"* rule is SATISFIED.** **That is the FIRST time in this entire line the control has
> been green.** ✓

## ✅✅ THE ANSWER: **A SECOND WRITER WROTE `0x001D5078` INTO THE SLOT**

```
GUEST_SLOTW_EVENT index=2 seq=3 kind=1 alias=1 slot_hit=1 fault_va=001AD62C
  pre=0015F9D0 post=00000000 tid=27444 range=1 form=1 rip=00007FF606B5FE38
GUEST_SLOTW_EVENT index=3 seq=4 kind=2 alias=1 slot_hit=1 fault_va=001AD62C
  pre=0015F9D0 post=001D5078 tid=27444 range=1 form=1 rip=00007FF606B5FE3C
```

> ## **THE SLOT RECEIVED EXACTLY TWO WRITES: the installer's `0x0015F9D0`, then a SECOND WRITER's `0x001D5078` — THE CALL TARGET.**

**And the hook READ IT BACK:**

```
GUEST_SLOTW_CROSS ledger_checks=353 ledger_mismatch=0 loss_checks=353 loss_mismatch=0 loss_skipped=104
  last_slot_read=001D5078 last_slot_read_seq=3 last_slot_read_hits=2 last_slot_read_alias=1 change_seen=1
```

**`last_slot_read=0x001D5078` at `seq=3`** — **the slot read returned the second writer's value, and the
cross-validation is CLEAN (zero mismatches across 353 checks).** ✓

## So the `(a)`/`(b)` fork that began this packet is **ANSWERED — and the answer is neither**

**The fork was:** **(a)** the store at `0x00199F45` wrote a coincidental packed byte tuple; **(b)** a competitor
wrote a pointer.

> **THE ANSWER IS (b)-SHAPED BUT NOT THE CANDIDATE: a competitor wrote `0x001D5078`, and the competitor is
> `rip=0x00007FF606B5FE38` — NOT `0x00199F45`.** **And the value is a POINTER, not a packed tuple.**

**The two writers, by native offset from the image base:**

| Writer | Native RIP | Offset | Value written |
|---|---|---|---|
| **the installer** | **`0x00007FF60716D82D`** | **`0xB3D82D`** | **`0x0015F9D0`** |
| **THE COMPETITOR** | **`0x00007FF606B5FE38`** | **`0x52FE38`** | **`0x001D5078`** |

**Both `range=1` (`GAME_MODULE`) and both `form=1` (a store).** ✓ **So the second writer is RECOMPILED GUEST
CODE, not host code and not a toolkit path.**

## ⚠ AND THE COHERENCE GATE FIRED — correctly

```
GUEST_SLOTW_FOURTH reached=0 read_count=1 terminal_seen=1 terminal_target=00000000
term_base=30766A64 term_base_ok=0 term_slot=00000000 stable=0
```

**The terminal was `invalid target 0x00000000`** — **NOT `0x001D5078`.** **And the coherence gate's verdict:**

> **`MISMATCH` — `last_write=001D5078` vs `terminal=00000000` ⇒ `UNKNOWN`.**

**⚠ AND THE GATE IS RIGHT: the values GENUINELY differ.** **The slot held `0x001D5078` at `seq=3`, and by the
terminal it held `0` — so something ZEROED it between.** **The gate correctly refuses to call that coherent.**

**So the row is NOT a clean `O-DATA-AS-CALL`:** **the writer of `0x001D5078` IS observed (RIP, value, order),
but the run's terminal is a DIFFERENT failure (`0x00000000`), so the packet's *"matching terminal
`0x001D5078`"* criterion is NOT met.** ✓

## The instrument's health — the best yet

| Counter | Value |
|---|---|
| **`relevant_av`** | **10 592** |
| **`slot_hits`** | **2** |
| **`steps` / `rearm_ok` / `rearm_failed`** | **10 562 / 10 562 / 0** |
| **`installer_control_hits`** | **2** ✅ |
| **`range`** | **`game=9249 host=1079 unknown=264 unavailable=0`** |
| **`form`** | **`store=8523 not_store=1960 undecoded=109`** |
| **cross-validation** | **353 checks each, ZERO mismatches** ✅ |
| **`RECONCILE`** | **`records=5 counted=5 expected=5 complete=1`** |
| **`absence_rows_valid=0` / `positive_records_valid=1`** | ✓ **the asymmetry holding** |

**⚠ And `unknown=264`** — **264 RIPs the classifier could not place, which the packet says is `INFRA
FAILURE`.** **The Session records that: by the packet's own rule, `unknown > 0` is a failure condition, so
this run is not clean even though the control fired.** **The Session does NOT claim otherwise.**

## What this establishes, at its true strength

| Claim | Status |
|---|---|
| **the range-based classifier works** | ✅ **the control fired; `game=9249 host=1079`** |
| **the installer wrote the slot** | ✅ **`post=0x0015F9D0`** |
| **A SECOND WRITER wrote `0x001D5078` to the slot** | ✅ **observed with RIP, value and order** |
| **the second writer is RECOMPILED GUEST CODE** | ✅ **`range=1`, `form=1`** |
| **the candidate `0x00199F45` was NOT that writer** | ✅ **the RIP is `0x00007FF606B5FE38`** |
| **the hook read `0x001D5078` back** | ✅ **`last_slot_read=001D5078`, cross clean** |
| **a matching `0x001D5078` terminal** | ❌ **the terminal was `0x00000000`** |
| **a clean run** | ❌ **`unknown=264` is `INFRA FAILURE` by the packet's rule** |

> **So the Session records the FINDING — the competitor's RIP and value — and does NOT promote a row, because
> the terminal did not match and `unknown > 0`.** **Both disqualifiers are stated.**

## ⚠ AND THIS ANSWERS THE LINE'S OLDEST QUESTION IN THIS AREA

**The chain the line established statically was:** **install `0x0018CE3A` → read `0x00193E62` (NULL-tested) →
`0x00193EB5 call eax`, with `eax = 0x001D5078`.**

**The Session has now OBSERVED the slot receiving `0x0015F9D0` (the installer) and then `0x001D5078` (a second
recompiled writer).**

> **So `0x001D5078` reaches the slot by a SECOND WRITE, not by the installer's — and the static candidate
> `0x00199F45` is NOT the writer.** **That is a genuine, first-of-its-kind observation for this line.** ✓

## What the successor needs

1. **The competitor's RIP must be mapped to a GUEST VA** — **`0x00007FF606B5FE38`, native offset `0x52FE38`**
   — **and that needs the recompiler's own mapping, not arithmetic on a guessed base.** **⚠ And per the
   paradigm correction, do NOT derive it by subtracting a base.**
2. **A run with `unknown=0`** — **264 unplaceable RIPs is `INFRA FAILURE` by the packet's own rule.**
3. **A run whose terminal IS `0x001D5078`** — **so the coherence gate can return `COHERENT`.**
4. **And the zeroing between `seq=3` and the terminal** — **`term_slot=00000000`** — **is a NEW observation
   worth naming: something zeroed the slot after `0x001D5078` was written.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** **No fault-RIP encoding cited.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
unchanged. **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.**
