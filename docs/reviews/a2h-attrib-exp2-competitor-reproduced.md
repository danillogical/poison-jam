# ✅✅ `A2h-slot-writer-attribution-r2` — **Exp2: the competitor REPRODUCES** — same code, same offset, same value, **K≥2 AGREED**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-attribution-r2`, frozen
**`E209D1F4A4405F0B266E8D4DEFDA77A481A277D27615BFAE9ACB9F7A3A6C6378`**.
**Runs:** **Exp1** `…120832-477-a2h-attrib-exp1`; **Exp2-2** `…121122-790`; **Exp2-3** `…121128-927`
(**save-root flake, discarded**); **Exp2-3b** `…121142-929`.

---

## ✅ THE COMPETITOR REPRODUCES — **K ≥ 2 AGREEMENT MET**

**The packet requires *"K ≥ 2-agree"* per the line's discipline.** **Two independent runs observed the SAME
competitor:**

| Run | image base | installer RVA | **competitor RVA** | value written |
|---|---|---|---|---|
| **Exp1** | **`0x00007FF606630000`** | **`0xB3D82D`** | **`0x52FE38`** | **`0x001D5078`** |
| **Exp2-3b** | **`0x00007FF68B060000`** | **`0xB3D82D`** | **`0x52FE38`** | **`0x001D5078`** |

> ## **IDENTICAL competitor RVA in two independent runs, with different image bases, and the same value `0x001D5078`.** **The Session verified the RVAs are equal.** ✓

**And Exp2-3b's full event sequence matches Exp1's exactly:**

```
seq=1/2  rip=…B9D82D  installer writes 0015F9D0   (installer_control_hits=2)
seq=3/4  rip=…58FE38  THE COMPETITOR writes 001D5078
```

**Both `range=1` (`GAME_MODULE`), both `form=1` (a store), `slot_hits=2`.** ✓

## ✅ And the competitor is `sub_00038530` in BOTH runs

**The Session's map tool confirms it for Exp1** (`12eb28c`), **and Exp2-3b's RVA is IDENTICAL, so the symbol
is the same by construction.** ✓

## ⚠ Exp2-2 is a DIFFERENT and important run: **the slot READ AS ZERO at the read site**

```
[ICALL] invalid target 0x00000000 tid=68752 esp=007BFF94 return=00193E62
GUEST_SLOTW_EVENT index=0 seq=1 kind=1 … rip=…9D82D  (installer)
GUEST_SLOTW_EVENT index=1 seq=2 kind=2 … post=0015F9D0
slot_hits=1  installer_control_hits=1
GUEST_SLOTW_CROSS ledger_checks=325 ledger_mismatch=1 loss_checks=325 loss_mismatch=1
  last_slot_read=0015F9D0 last_slot_read_seq=1
```

> ## **The terminal's `return=00193E62` IS THE READ SITE ITSELF** — **`mov eax,[esi+0x1C4]` — so `eax` was ZERO at `0x00193EB5 call eax`.**

**So in Exp2-2 the slot held `0x0015F9D0` (installer), was ZEROED, and read as `0`.** **The competitor did NOT
appear.** **And `ledger_mismatch=1` / `loss_mismatch=1` — the cross-validation CAUGHT the disagreement.** ✓

**⚠ So the ZEROING the Session flagged as unexplained in Exp1 IS ALSO A TERMINAL CONDITION, and it is now
observed directly:** **`0x00193E62` read `0` and called `0`.**

## ⚠ And the Session must record a QUALIFICATION honestly

**Exp2-3b reached terminal `0x00000000` at `return=0014982E`, NOT `0x001D5078`.** **So NEITHER Exp2 run has
the matching `0x001D5078` terminal the packet requires.**

**And every run so far has `unknown > 0`** (`264`, `282`) — **which is `INFRA FAILURE` by the packet's own
rule.**

> **So the Session records a REPRODUCED OBSERVATION and does NOT promote a row.** **Three disqualifiers are
> stated: no matching `0x001D5078` terminal, `unknown > 0` on every run, and Exp2-2's `ledger_mismatch=1`.**

## What is now ESTABLISHED, at its true strength

| Claim | Status |
|---|---|
| **the range classifier works** | ✅ **the installer control fired on every run (`installer_control_hits` = 2, 1, 2)** |
| **the installer writes `0x0015F9D0`** | ✅ **observed in 3 runs** |
| **a SECOND writer writes `0x001D5078`** | ✅ **observed in 2 independent runs** |
| **the second writer is `sub_00038530`** | ✅ **via the run's own map, control-validated** |
| **the second writer is at RVA `0x52FE38` in both** | ✅ **K≥2 AGREED** |
| **the static candidate `0x00199F45` is NOT the writer** | ✅ **different function entirely** |
| **something ZEROES the slot** | ✅ **observed as a TERMINAL in Exp2-2 (`return=00193E62`, `eax=0`)** |
| **a matching `0x001D5078` terminal** | ❌ **not yet observed** |
| **a clean run (`unknown=0`)** | ❌ **not yet observed** |

## The line's question is now ANSWERED in substance, with the row still withheld

**The packet asked: *"Does a writer other than the installer write `software_device+0x242C`, and with what
value?"***

> **ANSWER: YES — the recompiled body of `sub_00038530` writes `0x001D5078` into the slot, reproducibly, in
> two independent runs at the same RVA, and it is NOT the static candidate `0x00199F45`.**

**The Session does NOT promote this to an accepted row because the packet's own qualifying criteria are not
all met.** **It records the answer, the evidence, and the three disqualifiers — so a successor can decide
whether the criteria or the observation should move.**

## ⚠ A NEW FINDING worth naming

**The slot is ZEROED before the read in at least two runs** — **Exp1 (`term_slot=00000000`) and Exp2-2
(`return=00193E62`, `eax=0`).**

**So the sequence is: installer writes `0x0015F9D0` → [sometimes] `sub_00038530` writes `0x001D5078` →
something ZEROES it → the read at `0x00193E62` takes whatever is there → `call eax`.**

> **That makes the ZEROING a THIRD actor in the chain, and the line has never named it.** ✓ **The Session
> records it as a NEW edge rather than folding it into the competitor finding.**

## Prohibitions and status

**Observation only.** **Page-protection only — no DR anywhere.** **No DR record cited.** **No synthetic
completion.** **No fault-RIP encoding cited.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
unchanged. **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **3 of N=5 used.**
