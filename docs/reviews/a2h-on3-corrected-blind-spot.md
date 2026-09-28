# ⚠⚠ `A2h` — ON trial 3 CORRECTED: the instrument has a **SINGLE-STEP WINDOW BLIND SPOT**, and the slot's evidence is **CONTRADICTORY**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Corrects:** `docs/reviews/a2h-live-slot-write-on3-evidence.md` (`6ba5430`), **which the Session wrote and which
OVER-CLAIMED.**
**Why:** the Session re-examined ON trial 3's own numbers and found a contradiction it had read past.

---

## ⚠ THE CONTRADICTION

| Observation | Value |
|---|---|
| **the slot's ONLY observed write** | **`post=0x0015F9D0`** — the installer's constant |
| **the hook's slot read** (`last_slot_read`) | **`0x0015F9D0`** |
| **the terminal call target** | **`0x001D5078`** |

> **`0x001D5078` CANNOT have come from the slot as observed.** **The slot read `0x0015F9D0` and the call
> used `0x001D5078`.** **So SOMETHING wrote the slot that the watch did not see.**

**The Session's ON-3 record said *"no competitor wrote the slot"*** — **that claim is WITHDRAWN.** **The
correct statement is: the watch saw only the installer's write, AND the terminal proves another write
happened.**

## ⚠ THE CAUSE: THE SINGLE-STEP WINDOW IS A BLIND SPOT

**The trap sequence, per write:**

```
1. page is READONLY  -> a write FAULTS              (the watch sees it)
2. handler publishes the record
3. VirtualProtect -> PAGE_READWRITE                 (page OPEN)
4. EFlags.TF set -> single-step the faulting write
5. #DB -> handler reads post, re-protects READONLY
```

> ## **STEP 3 IS THE BLIND SPOT.** **While the page is open, it is WRITABLE — so ANY OTHER THREAD writing
> the slot in that window DOES NOT FAULT and is NEVER RECORDED.**

**The run's own numbers show how wide that is:**

| Counter | Value |
|---|---|
| **`steps`** | **9 035** — **9 035 open windows** |
| **`unprotected_intervals`** | **9 078** |
| **`concurrent_overlap`** | **106** — **faults seen while another thread was mid-step** |

**⚠ AND `concurrent_overlap` DOES NOT COVER THIS.** **It counts faults observed during a window — but a
write INTO an open window produces NO FAULT AT ALL, so it is invisible to that counter too.**

**The Session verified the mechanism from the implementation's own sequence:** **the page must be opened to
let the faulting store execute, and it stays open until the `#DB`.** **That window is unavoidable for this
design — and it is exactly the window a concurrent writer needs.**

## ⚠ SO THE ROW IS `O-OPEN`, AND THE HONEST STATEMENT IS NARROWER THAN THE SESSION FIRST WROTE

**What ON trial 3 establishes:**

| Claim | Status |
|---|---|
| **the instrument arms, fires, re-arms and cross-validates** | ✅ **9 078 AVs, 9 035 steps, `rearm_failed=0`, cross clean** |
| **the corrected page is right** | ✅ **the trap fired at host `0x001AD62C`** |
| **the installer wrote the slot** | ✅ **`post=0x0015F9D0` observed** |
| **the `0x001D5078` terminal reproduced** | ✅ **with the ICALL 4-cycle and the same registers** |
| **~~the candidate `0x00199F45` did not write the slot~~** | **⚠ WITHDRAWN — the observation is INCOMPLETE** |
| **~~no competitor wrote the slot~~** | **⚠ WITHDRAWN — the terminal PROVES one did** |

> **The correct statement:** **the watch observed exactly one slot write — the installer's — and the terminal
> proves at least one more occurred that the watch could not see.** **So the instrument's slot coverage is
> NOT complete, and no absence claim is available from it.**

**This is a `COVERAGE FAILURE` of the same class the packet's own loss rules describe** — **and the Session
records it rather than the absence it first claimed.**

## ⚠ AND A SECOND DEFECT THE SESSION FOUND WHILE CHECKING

**The positive control did NOT register:**

```
installer_control_hits=0
```

**Yet the observed write's `post=0x0015F9D0` IS the installer's constant, and the Session computed the image
base from the event's native RIP:**

```
event rip = 0x00007FF64CFFD5ED
if the event is the installer (guest 0x0018CE3A):  base = 0x00007FF64CE707B3
base aligned to 0x10000:                           0x00007FF64CE70000
difference: 0x7B3
```

**So the RIP-minus-guest arithmetic gives a base with a `0x7B3` residual** — **not a plausible image base.**
**⚠ The Session does NOT conclude the mapping is wrong; it notes the residual is unexplained and that the
classifier's RIP→guest mapping needs verifying.**

**⚠ AND THE PACKET REQUIRED THE CONTROL: *"no hit ⇒ `INFRA FAILURE`, fail closed, no outcome claim."***
**`installer_control_hits=0` means the control did NOT register — so by the packet's own rule this run is
`INFRA FAILURE`.**

> **The Session's ON-3 record treated the control as green because `post=0x0015F9D0` matched the installer's
> constant.** **That was the Session's INFERENCE, not the instrument's classification.** **The packet requires
> the CONTROL to fire, and it did not.** **Withdrawn.**

## ⚠ AND A THIRD: THE FOURTH-READ LATCH NEVER FIRED

```
GUEST_SLOTW_FOURTH reached=0  read_count=75  value=00000000
  terminal_seen=1  terminal_target=3E800000
read_samples=1
```

**`read_samples=1` while the read site ran 75 times** — **so the hook sampled the slot ONCE, not at the
terminal read.** **And `terminal_target=3E800000`, the float-bit sibling, not `0x001D5078`.**

> **So the `0x001D5078` call was NEVER TIED TO A SLOT READ by the instrument.** **The Session's ON-3 record
> noted the `3E800000` mismatch but did not draw the consequence: THE CHAIN IS UNTIED.** **The terminal and
> the slot observation are two facts from the same run, not a connected chain.** **Withdrawn.**

## What survives — and it is still substantial

**The instrument works.** **It arms, fires, re-arms, cross-validates cleanly, and survives a full run.** **And
it OBSERVED the installer's write to the slot** — **a real observation, the first of its kind on this line.**

**What does NOT survive is the INFERENCE the Session drew from it.** **The Session had:**
1. **treated a value-match as a control firing, when the packet requires the CONTROL to classify;**
2. **treated "not observed" as "did not happen," when the terminal proves otherwise;**
3. **treated the terminal and the slot read as a chain, when the latch never tied them.**

**All three are the same error: promoting an observation into a conclusion the instrument did not support.**

## The disposition

> **Row `O-OPEN`.** **The run is `INFRA FAILURE` by the packet's own control rule.** **The instrument's slot
> coverage is INCOMPLETE due to the single-step window.** **No absence claim, no attribution.**

**The Session PARKED ON-3 and will NOT run trials 4–5 until it has consulted the Advisor**, because the
blind spot is a **design property of the trap**, not a tuning issue — **and a further run would reproduce it.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **No further ON run pending the ruling.**
