# `A2h-live-slot-write-r1` — ON trial 1: **INFRA FAILURE** by the packet's own rule — and the WATCH WORKS

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-live-slot-write-r1`, frozen
**`8F3C6291C42ED3D9C3B96D879A9BB9D46391E5FEEA3F4BDC1E8712F46FFF7D4F`**.
**Runs:** fresh OFF `…102040-264-a2h-slotw-inert-off` (**inert**) + ON `…102127-113-a2h-slotw-on-1`.

---

## ✅ THE HEADLINE: THE PAGE-PROTECTION WATCH **FIRES**

**This is the first write-watch on this line that has demonstrably worked.** **The DR0 watch never fired once
across ~14k debug events per run; this one fired 129 times in a partial run.**

```
GUEST_SLOTW address=… version=2 size=14680 armed=1 arm_reason=1
  arm_base=0019B200 term_base=00000000 arm_slot=0019D62C term_slot=00000000
  stable=0 term_base_ok=0 page_offset=6
GUEST_SLOTW_LOSS relevant_av=129 slot_hits=0 nonslot_writes=129
  steps=128 rearm_ok=128 rearm_failed=0 protected_intervals=157 unprotected_intervals=128
  concurrent_overlap=0
```

| Result | Value |
|---|---|
| **ARM** | **`arm_base=0019B200`, `arm_slot=0019D62C`, `page_offset=6`** ✓ |
| **aliases protected** | **`aliases=29/29 mask=1FFFFFFF`** ✓ |
| **AVs observed** | **`relevant_av=129`** |
| **single-steps + re-arms** | **`steps=128`, `rearm_ok=128`, `rearm_failed=0`** ✓ |
| **concurrent overlap** | **`concurrent_overlap=0`** ✓ |
| **the deferred ARM worked** | `[A2HSLOTW] device not yet allocated (MEM32(0x0019DCE0)=00000000); ARM deferred to the census poll` → **`deferred ARM completed base=0019B200`** ✓ |

> **So the mechanism the preflight authorized is PROVEN: RO protection delivers write faults, the
> `ExceptionInformation[0]==1` filter admits writes, single-step re-arm succeeds 128/128, and 29/29 aliases
> are covered.** **`stable=0 term_base_ok=0` is EXACTLY what the implementation record predicted.**

## ⚠ THE TRIAL IS **INFRA FAILURE** — by the packet's own rule

**The packet:** *"no hit ⇒ `INFRA FAILURE`, fail closed, no outcome claim."*

| Required for QUALIFYING | Observed |
|---|---|
| **writer-observed slot RIP/value/order** | **`slot_hits=0`** ✗ |
| **matching terminal `0x001D5078`** | **NO terminal** — `Failed to resolve VA` count = **0** ✗ |
| **installer trap green** | **`[A2HSLOT]` lines = 0** — the installer never ran ✗ |
| **the fourth read** | **`GUEST_SLOTW_FOURTH reached=0 read_count=0`** ✗ |

**So: `O-OPEN`, and the run does not qualify.** **No outcome claim.**

## ⚠ THE CAUSE IS AN INSTRUMENT CAPACITY DEFECT — and the fail-closed path killed the run

```
GUEST_SLOTW_OVERFLOW latch=1 dropped=1 event_overflow=1 thread_overflow=0 event_count=257 thread_count=18
GUEST_SLOTW_RECONCILE records=256 counted=257 writes=129 steps=128 reads=0 slot_hits=0
  hits_in_records=0 complete=0 overflow=1 absence_rows_valid=0 positive_records_valid=1
```

**The event buffer holds 256 records; each observed write consumes TWO (the AV and its step).** **So ~128
writes exhaust it.** **The run reached 129.**

**And then the fail-closed path fired — correctly, by design:**

```
[A2HSLOTW] write NOT published alias=1 fault=000000000019D200 -- page left CLOSED (coverage failure)
```

**And the code's own comment explains the choice:** *"a page that is open without a record is a hole in the
census, and an unrecorded write is the one outcome that would make a zero-touch conclusion false. … the fault
is left to propagate rather than being converted into a silent absence."*

> **So the instrument correctly refused to produce a silent absence — and the propagated fault ENDED THE
> RUN.** **The last log line is the fault:** `[EXCEPTION first-chance] tid=65720 code=0xC0000005
> RIP=0x7FF6BD4DF928 fault=0x19D200 (write)`.

**The design is RIGHT and the CAPACITY is wrong.** **`absence_rows_valid=0` / `positive_records_valid=1` is
the reconciliation working exactly as the packet specified** — **the loss invalidates absence rows, and a
positive record would still stand.** ✓

## ⚠ NEW STRUCTURAL FINDING: a SEQUENTIAL ZEROING FILL heading toward the slot

**The 128 recorded writes are not scattered — they are a LINEAR 4-byte fill:**

```
index=0   fault_va=0019D000  pre=B8077500 post=00000000
index=2   fault_va=0019D004  pre=B8077500 post=00000000
index=4   fault_va=0019D008  pre=B8077500 post=00000000
index=6   fault_va=0019D00C  pre=B8077500 post=00000000
   …  incrementing by 4, all pre=B8077500, all post=00000000
index=254 fault_va=0019D1FC  pre=B8077500 post=00000000
then      fault_va=0019D200  ← the 129th write, where publication failed
```

> **Something is ZEROING page `0x0019D000` from its base upward, one dword at a time, overwriting a
> `0xB8077500` pattern.** **And THE SLOT IS AT `0x0019D62C` — 395 dwords above the start.**

**So the fill was ~267 dwords short of the slot when the run died.**

**And the interleaved `kind=2` events read THE SLOT and show it UNCHANGED:**

```
index=1 seq=2 kind=2 fault_va=0019D62C pre=B8077500 post=B8077500
```

**So in this run the slot holds `0xB8077500` — neither the installed `0x0015F9D0` nor `0x001D5078` — and it
has NOT yet been zeroed.**

**⚠ The Session records this as a LEAD, NOT as an attribution.** **A linear fill that would reach the slot is
a plausible COMPETITOR — and it is exactly the shape the watch was built to find.** **But the run died before
the fill reached `0x0019D62C`, so nothing is established about whether it would have.**

**And one thing needs explaining rather than asserting:** **`pre=B8077500` is IDENTICAL across all 128 writes
at different addresses.** **Either the page genuinely held that pattern throughout, or `pre` is read from a
fixed location.** **The Session does NOT resolve it and flags it as a question for the successor.**

## What the Session did NOT do, and why

**The packet authorizes up to `N ≤ 5` ON runs, and this is run 1.** **The Session is NOT running the
remaining four, because:**

1. **the failure is STRUCTURAL, not stochastic** — **a 256-record buffer against ~129 writes is a capacity
   arithmetic that will recur**, and the observed traffic is a **deterministic linear fill**, so further runs
   would be near-identical;
2. **the packet's terminality bound says:** *"zero qualifying in N ⇒ report + RE-REFER, never extend"* — **and
   *"opens another gap ⇒ PARK with the precise edge and RE-REFER for scope"***;
3. **spending 4 more runs to confirm the same overflow would be exactly the L1 pattern the Advisor's bound
   exists to prevent.**

> **So: `O-OPEN`, PARKED, with the precise edge = the event capacity, and RE-REFERRED for scope.**

**The Session notes it is NOT claiming *"zero qualifying in N"*** — **it used 1 of 5 and stopped for a stated
structural reason.** **That distinction is recorded so the report is not read as exhausting the bound.**

## The OFF was inert

**Zero `[A2HSLOTW]` lines, and `GUEST_SLOTW unarmed
reason=never_initialised note=gate_unset_or_ARM_refused_ledger_is_all_zero`.** **Strict profile confirmed.**
✓ **An all-zero ledger is reported as UNARMED, not as a layout mismatch — which is the correct handling.**

## What the successor needs, stated precisely

1. **A larger event capacity, or a SLOT-ONLY filter** — **the page carries ~129 writes in a partial run, and
   the slot is 395 dwords in.** **The packet's own design records every non-slot page write as TRAFFIC; the
   defect is that traffic is retained as full records rather than counted.**
2. **Keep the fail-closed propagation** — **it is correct, and it is what made the defect visible rather than
   silent.** **But it must not fire on ordinary traffic volume.**
3. **Resolve the `pre=B8077500` question** before the value field is relied on.
4. **The linear fill is the new lead** — **whether it reaches `0x0019D62C` is the next question, and it is
   answerable by the same instrument with adequate capacity.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR0/DR6/DR7 anywhere.** **No DR record cited.**
**No synthetic completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged.
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **No second ON run.**
