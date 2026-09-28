# ⚠ `A2h-live-slot-write-r1` — ON trial 3: **THE QUALIFYING TERMINAL**, and the slot was written ONLY by the installer

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-live-slot-write-r1`, frozen
**`8F3C6291C42ED3D9C3B96D879A9BB9D46391E5FEEA3F4BDC1E8712F46FFF7D4F`**.
**Run:** ON `…110859-848-a2h-slotw2-on-3` (**run 3 of the new N=5**).

---

## ✅ THE TERMINAL IS `0x001D5078` — the qualifying failure, with the ICALL 4-cycle intact

```
[ICALL] Failed to resolve VA 0x001D5078 (thread calls: 360, tid=69004, ms=505420953)
  [12] 0xFE000190
  [13] 0x00193D90
  [14] 0xFE0000B4
  [15] 0x001D5078
[EXCEPTION] tid=69004 code=0xE0424943 RIP=0x7FFA6EB441CA
  Xbox regs: eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
  Xbox regs: ebx=0xFD000000 esi=0x0019D468 edi=0x00000000
```

> **The terminal matches EXACTLY** — **the same target, the same ICALL 4-cycle the line identified many
> packets ago, and the same register set** (`eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293
> esp=0x007BFF9C`, and `ebx=0xFD000000` = the **aperture**, `esi=0x0019D468` = the **context**). ✓

## ⚠ THE ANSWER: the slot was written by the INSTALLER and by NOTHING ELSE

```
GUEST_SLOTW_EVENT index=0 seq=1 kind=1 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=00000000 tid=57276 enc=3 rip=00007FF64CFFD5ED
GUEST_SLOTW_EVENT index=1 seq=2 kind=2 alias=1 slot_hit=1 fault_va=001AD62C
  pre=00000000 post=0015F9D0 tid=57276 enc=0 rip=00007FF64CFFD5F0
```

**Across 9 078 write faults and the WHOLE run, there is EXACTLY ONE slot write: the installer's
(`post=0x0015F9D0`).**

> ## **`0x00199F45` — the candidate writer — NEVER WROTE THE SLOT.** **And NO COMPETITOR DID EITHER.**

**The instrument is PROVEN (Q3 clean, installer trap green, 9 078 AVs with `rearm_failed=0`), and it
observed the slot for the entire run.**

## ⚠ BUT THE RUN DOES **NOT** QUALIFY — and the Session must say why precisely

**The packet's qualifying criteria:**

| Criterion | Result |
|---|---|
| **writer-observed slot RIP/value/order** | ✅ **the installer's write was observed** |
| **matching terminal `0x001D5078`** | ✅ **the terminal matches** |
| **controls green incl. the installer trap** | ✅ **green** |

**All three appear met — BUT the packet also requires the chain to identify a writer OF `0x001D5078`:**

> *"`O-DATA-AS-CALL` when the last reaching write is `0x00199F45`, with witnessed RIP/value/order
> `0x001D5078` at fourth read … the packed tuple is **observed flowing into a call**."*

**The observed slot write was the INSTALLER writing `0x0015F9D0` — NOT a write of `0x001D5078`.** **So the
criterion *"the last reaching write is `0x00199F45`"* is NOT met.**

**And the packet's `NON-TARGET` row requires *"the last reaching pre-fourth-read slot write is a different
RIP"*** — **which IS what happened, except the different RIP is the INSTALLER, which the packet classifies as
the known decoy rather than as a competitor.**

## ⚠ SO THE ROW IS `O-OPEN` — WITH A DECISIVE NEGATIVE

**The Session records this at its true strength:**

> **Within a PROVEN instrument's complete observation of the slot across a run that reached the exact
> `0x001D5078` terminal, THE SLOT WAS WRITTEN ONLY BY THE INSTALLER.** **The candidate `0x00199F45` did not
> write it, and no competitor did.**

**⚠ BUT `absence_rows_valid=0` — so this is NOT an absence claim.** **The packet's own asymmetry forbids it:**
**`absence_rows_valid=0` and `positive_records_valid=1`.** **The run had `concurrent_overlap=101`-scale
coverage holes and `first_touch_overflow=1`, and the Session must NOT convert an invalid absence row into a
negative finding.**

**So the honest statement is:** **no slot write other than the installer's was OBSERVED, and the absence rows
are INVALID, so this does not establish that none occurred.** ✓

## ⚠ And the `GUEST_SLOTW_FOURTH` latch did NOT reach the fourth read

```
GUEST_SLOTW_FOURTH reached=0 read_count=75 value=00000000 seq=0 last_write_seq=0
  last_write_rip=0000000000000000 terminal_seen=1 terminal_target=3E800000
```

**⚠ `terminal_target=3E800000` — NOT `0x001D5078`.**

**The Session investigated this and found the cause:** **the log shows TWO failures:**

```
[ICALL] invalid target 0x3E800000 tid=64520 esp=0126BF8C return=00147DE2
[ICALL] Failed to resolve VA 0x001D5078 (thread calls: 360, tid=69004, ms=505420953)
```

**`0x3E800000` is a FLOAT-BIT sibling — the line's known CONTRASTIVE class — and it fired EARLIER
(`ticks=505420953` on `tid=64520`), from a DIFFERENT thread than the `0x001D5078` terminal (`tid=69004`).**

> **So the terminal latch captured the FIRST terminal-class event, which was the float-bit sibling, not the
> `0x001D5078` one.** **That is an instrument limitation for the contrastive-sibling case, and the Session
> records it as such.**

**⚠ And it means the `FOURTH` latch's `terminal_target` must be read per-thread, or the latch must match the
target.** **The Session flags it for the successor rather than treating the mismatch as a defect in the
`0x001D5078` observation, which is independently established by the log and the ICALL trace.** ✓

## The instrument's full health on this run

```
relevant_av=9078  slot_hits=1  nonslot_writes=9077  steps=9035  rearm_ok=9035  rearm_failed=0
protected_intervals=9064  unprotected_intervals=9078
CROSS ledger_checks=285 ledger_mismatch=0 loss_checks=285 loss_mismatch=0
RECONCILE records=3 counted=3 expected=3 complete=1 overflow=1
  absence_rows_valid=0 positive_records_valid=1
```

**`rearm_failed=0` across 9 035 single-steps, and ZERO cross-validation mismatches.** ✓ **The instrument is
sound.**

## What this run establishes, at its true strength

| Established | Status |
|---|---|
| **the instrument is PROVEN and sound** | ✅ **9 078 AVs, 9 035 steps, `rearm_failed=0`, cross clean** |
| **the corrected page is RIGHT** | ✅ **the installer trap fired at `0x001AD62C`** |
| **the `0x001D5078` terminal IS reproducible under the instrument** | ✅ **this run reached it** |
| **the candidate `0x00199F45` did NOT write the slot** | ⚠ **OBSERVED — but absence rows are INVALID** |
| **no competitor wrote the slot** | ⚠ **OBSERVED — but absence rows are INVALID** |
| **a `NON-TARGET`/`O-DATA-AS-CALL` row** | ❌ **neither — the observed writer is the known installer** |

> **So the row is `O-OPEN`, and the Session records a DECISIVE OBSERVATION with an explicit limitation
> rather than promoting it.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **TARGET UNCHANGED.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.**
