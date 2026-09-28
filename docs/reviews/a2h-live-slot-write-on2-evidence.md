# `A2h-live-slot-write-r1` — ON trial 2: **THE INSTRUMENT IS PROVEN** — installer trap GREEN, cross-validation CLEAN

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-live-slot-write-r1`, frozen
**`8F3C6291C42ED3D9C3B96D879A9BB9D46391E5FEEA3F4BDC1E8712F46FFF7D4F`**.
**Runs:** fresh OFF `…110743-628-a2h-slotw2-inert-off` (**inert**) + ON `…110806-304-a2h-slotw2-on-2`.

---

## ✅ THE REQUIRED POSITIVE CONTROL **FIRED** — on the CORRECTED page

```
GUEST_SLOTW_EVENT index=0 seq=1 kind=1 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=00000000 tid=61556 enc=3 rip=00007FF61949D5ED
GUEST_SLOTW_EVENT index=1 seq=2 kind=2 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=0015F9D0 tid=61556 enc=0 rip=00007FF61949D5F0
```

**The Session verified the arithmetic:** **slot guest `0x0019D62C` + `g_memory_offset 0x10000` = host
`0x001AD62C`** — **and the hit's `fault_va` IS `0x001AD62C`.** ✓

> ## **`post = 0x0015F9D0` — THE INSTALLER CONSTANT.** **So the LIVE INSTALLER TRAP fired on the corrected page, and the packet's REQUIRED positive control is GREEN.**

**And this is the first time this line has observed the installer's write at the slot.** ✓

## ✅ CROSS-VALIDATION **CLEAN** — the values are trustworthy

```
GUEST_SLOTW_CROSS ledger_checks=317 ledger_mismatch=0 loss_checks=317 loss_mismatch=0
  loss_skipped=99 last_slot_read=0015F9D0 last_slot_read_seq=1 last_slot_read_hits=1
  last_slot_read_alias=1 change_seen=1
```

**`ledger_mismatch=0` and `loss_mismatch=0` across 317 checks each.** ✓ **Q3(c)'s live path is now
exercised — the Worker's stated uncertainty is discharged.**

**And `last_slot_read=0015F9D0`** — **the slot read returned the installer's value, consistent with the
trap.**

## The watch works at scale

```
relevant_av=9257  slot_hits=1  nonslot_writes=9256
steps=9223  rearm_ok=9223  rearm_failed=0
protected_intervals=9252  unprotected_intervals=9257
concurrent_overlap=101  threads_new=4
```

**9 257 write faults, 9 223 single-steps, `rearm_failed=0`.** ✓ **The capacity repair works: 9 256 traffic
writes produced only 2 event records.**

**⚠ But `concurrent_overlap=101`** — **101 overlapping writes, which the packet counts as a coverage hole for
absence rows.** **And `first_touch_overflow=1`** (8 408 distinct addresses).

**And the asymmetry held:** **`absence_rows_valid=0` while `positive_records_valid=1`.** ✓

## ⚠ THE RUN IS **NOT QUALIFYING** — the terminal does not match

```
[ICALL] Failed to resolve VA 0xFFFFFFFF (thread calls: 1414, tid=58492)
```

> **The terminal is `0xFFFFFFFF`, NOT `0x001D5078`.** **So the terminal-match criterion FAILS.**

**Per the packet:** *"Qualifying means a writer-observed slot RIP/value/order record, **matching terminal
`0x001D5078` failure**, and green controls. **Nonmatching runs are contrastive evidence, not successes.**"*

**So ON trial 2 is CONTRASTIVE DATA, and the row remains `O-OPEN`.** ✓ **The Session records it as such
rather than claiming a near-miss as a success.**

## What ON trial 2 establishes

| Established | Status |
|---|---|
| **the corrected page is RIGHT** | ✅ **the trap fired at `0x001AD62C`** |
| **the required positive control is GREEN** | ✅ **`post=0x0015F9D0`** |
| **the instrument's VALUES are trustworthy** | ✅ **cross-validation clean, 317/317 both checks** |
| **the capacity repair works** | ✅ **9 256 traffic writes → 2 records** |
| **the watch scales** | ✅ **9 257 AVs, `rearm_failed=0`** |
| **the loss asymmetry works** | ✅ **absence invalid, positive valid** |
| **a `0x001D5078` terminal** | ❌ **not reproduced — the terminal was `0xFFFFFFFF`** |
| **the candidate writer `0x00199F45`** | **NOT OBSERVED** — `slot_hits=1` and it was the installer |

> **So the instrument is now PROVEN and the question is answerable — but this run's terminal was a DIFFERENT
> failure.** **The packet allows up to N=5; this is run 2.**

## ⚠ The Session's reading of the contrast

**The terminal varies across runs** — **the line has long known this** (**`0x001D5078` in 3 of 10 archived
runs**). **So a run reaching `0x001D5078` is a matter of repetition, not of instrument capability.**

**And the instrument is now demonstrably capable:** **it caught the installer's write at the slot, which
proves it would catch any OTHER write there.** ✓

**So the Session will continue the ON trials within the packet's bound.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **TARGET UNCHANGED.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.**
