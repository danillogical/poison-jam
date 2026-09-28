# `A2h-dr0-terminal-snapshot-r1` — Phase 1 trial: **`P1-UNKNOWN` ⇒ DROP THE DR LEG** (terminal ceiling)

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-dr0-terminal-snapshot-r1`, frozen
**`969E8827CC3D36AA8AFBF7D4FF7D43609BB750312723AAFBCE7ADFA5CA48F7EE`**.
**Runs:** fresh OFF `…052203-982-a2h-snapshot-inert-off` + one ON `…052216-631-a2h-snapshot-on-1`, both strict.

---

## ✅ THE REPAIR WORKED — the gate is functional again

**`complete=1`.** **On the previous build `complete` was structurally 0 and `NON_FIRING` was unreachable by
construction; it is now 1 and the instrument reaches a defined verdict.** **`decision=CONTEXT_LOST` — a
defined verdict, not `UNKNOWN_NOT_RECORDED`.** **The repair achieved exactly what it was authorized to do.**

**And the delivery premise is now PROVED ON THIS HOST by a genuine native control** — the Worker built a
fixture where the collector is the **real debugger of a child that raises the real handshake, is armed while
stopped, then performs a REAL store to the watched address**, producing a **genuine debugger-delivered `#DB`**
(`code=80000004`, `dr6=00000000FFFF0FF1`) claimed from `DR6.B0`. **`delivery_premise=PROVED`.**
**So "this host cannot deliver `#DB` to `WaitForDebugEvent`" is now RULED OUT as an explanation.**

## Fresh OFF — record-level inert

Zero `[A2HSLOT]`, **zero `GUEST_DR_*`**, zero `install_exec`. The pre-existing `GUEST_SLOT_LATCH` and
`GUEST_SLOT_WATCH` lines are **all-zero** (`install_seen=0`, `install_ok=0`, `armed=0`, `touched=0`).

## The live trial — a CONTRADICTION, and it is on the one thread that matters

```
GUEST_DR_DELIVERY_TERMINAL raw_events=14446 raw_single_step=0 ss_first_chance=0 ss_second_chance=0
  ss_routed_handler=0 ss_routed_generic_first=0 ss_routed_terminal_second=0 ss_unrouted=0
  continue_reconciled=1 ss_reconciled=1 store_evidence=1 arm_evidence=1 snapshot_evidence=1
  native_ingress=0 premise_proved=0 delivery_premise=UNPROVED install_hit=0
  canonical=00000000001D4064 complete=1 decision=CONTEXT_LOST

GUEST_DR_TERM_SNAPSHOT runs=1 snapshot_ok=1 seen=17 open_ok=17 suspend_ok=17 context_ok=17
  resume_ok=17 void_unsuspended=0 verified_armed=16 changed_set=1 already_suspended=1
  unreadable=0 unarmed=0 armed_tids_exited=0 armed_tids_missing=0 premature_disarm=0
```

**The snapshot worked correctly: 17 threads seen, all opened, all suspended, all read, all resumed, zero
void-unsuspended reads.** **And exactly one thread's DR state did not match — `index=0`, `tid=69420`, which is
THE INSTALL THREAD:**

```
GUEST_DR_TERM_ROW index=0 tid=69420 armed=1 suspended=1 read_ok=1
                 dr0=0000000000000000 dr7=0000000000000000 dr6=0000000000000000
```

**Every other thread reads armed** — 5 at `dr7=0D0001` and 11 at `dr7=0D0401` (the same watch; bit 10 is
reserved and CPU-forced). **But the install thread — the one that performed the store — reads ALL ZERO.**

**And this directly contradicts the arm record:**

```
GUEST_DR_ARM_OK seq=20 tid=69420 why=handshake dr0=00000000001D4064 dr7_readback=00000000000D0001
GUEST_DR_ARM_TID index=0 tid=69420
```

**The collector armed `69420`, read `DR0`/`DR7` back successfully, and listed it. At the terminal snapshot the
same tid reads zero.** **`already_suspended=1` — exactly one thread was already suspended, and it is this one.**

**Two candidates the evidence CANNOT separate:**
- **(a)** the install thread's DR state was **genuinely cleared** after the arm — which would **fully explain
  zero `#DB` for the store**;
- **(b)** a thread **already suspended at the debug event** reads differently from one the snapshot suspends
  itself, so the zero is a **read artifact** for exactly this thread.

**`CONTEXT_LOST` is the instrument's own verdict on that contradiction, and it is honest.**

## The disposition — the ceiling applies

**Per the packet's own rules:**

- **`P1-PASS`** requires the native install hit — **NOT met** (`install_hit=0`).
- **`P1-NONFIRING`** requires the **delivery-completeness premise** — and the packet is explicit:
  ***"If the `#DB` delivery-completeness premise is unproved, report `P1-UNKNOWN` rather than promote observed
  zero to decision-grade `NON_FIRING`."*** **`delivery_premise=UNPROVED` in the live run.**
- **Line 11:** *"Missing/contradictory store, snapshot, identity, event or publication evidence is
  `P1-UNKNOWN` ⇒ same drop."* **The arm-vs-snapshot contradiction on the install thread IS contradictory
  evidence.**

> ## **`P1-UNKNOWN` ⇒ INFRA FAILURE ⇒ DROP THE DR LEG + coverage-provenance. TERMINAL.**

**The Advisor's ceiling, verbatim in effect:** *"failure to establish the install trap, loss/contradiction, or
any new instrument defect ⇒ DROP the DR leg + coverage-provenance, with **NO third round absent
re-referral**."* **This is loss/contradiction.** **The Session is NOT proposing a third round.**

## What the DR leg's drop means — stated explicitly, not implied

**Per the Advisor:** *"Census/software reads/install software control survive; **no row reads DR records in
either direction**; **canonical-write attribution and the DR read-path leg remain unreachable.**"*

| Survives | Unreachable on this line |
|---|---|
| **28-alias page-protection census** (`armed=1 mapped=28 protected=28 touched=0`) | **canonical-write attribution** (guest / host / transient-as-write) |
| **Mapping stability** | **the DR leg of the read-path audit** |
| **The two terminal software zero reads** | **any DR-based absence or presence claim** |
| **The install software control** (`80000115 → FE000104`) | |

**And per the Advisor: *"never carry the channel as decoration."*** **No future row in this line may cite a
DR record — including the zero-`#DB` series, which is an observation about an instrument of unproven
coverage, not about the slot.**

## What this trial nevertheless added — genuinely new and durable

1. **The delivery premise is PROVED on this host** by a real native `#DB`. **So the long-standing
   "maybe `#DB` never arrives" hypothesis is now settled — it CAN arrive.**
2. **The gate defect is fixed and demonstrated**: `complete=1`, and a fixture (`absent`) that yields
   `complete=1 decision=NON_FIRING` — **impossible on the old code.**
3. **The contradiction is localised to the install thread**, which is a **sharper** finding than the previous
   line's "zero hits, cause unknown."
4. **Three further defects were found by measurement while implementing**, including route-counter pollution
   that had inflated `ss_routed_generic_first` to 14405 against `raw_single_step=0`, and a toolkit gate
   coupling that would have made `NON_FIRING` unreachable *again* via a second env var.

## Prohibitions and status

**No synthetic completion.** No guest semantics changed; the APU trap and `0x80` untouched; no allocation
faked; arena not widened; NULL call not bypassed; guest error handling not edited. **No `src/recomp/gen/*.c`
edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**. **No second ON run** —
the ceiling is terminal.
