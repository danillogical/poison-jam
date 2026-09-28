# `A2h` — Q3(a) executed: the `pre`-read path is **CORRECT**, and the identical value is EXPLAINED

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** the Advisor's ruling (`a2h-on1-advisor-ruling.md`, turn `01a0e90a`) — **Q3 gates everything**;
**Q3(a) is *"static inspection of the pre-read path (immediate)."***
**Result: the path is CORRECT.** **And the `pre=B8077500` puzzle is resolved — it is not a bug.**

---

## The path, read from the bytes

```
L2947  if (ep->ExceptionRecord->ExceptionInformation[0] != 1) { ... return CONTINUE_SEARCH; }
L2951  fault = (uintptr_t)ep->ExceptionRecord->ExceptionInformation[1];
L2952  alias = a2h_slotw_owning_alias(fault);
L2958  slot_va = a2h_slotw_slot_va();
L2959  off = slot_va & (XBOX_A2H_SLOTW_PAGE_SIZE - 1);
L2960  slot_hit = (fault == (uintptr_t)g_a2h_slotw_pages[alias - 1] + off) ? 1u : 0u;
...
L2976  pre = *(volatile uint32_t *)((uintptr_t)g_a2h_slotw_pages[alias - 1] + off);
```

> ## **`pre` is read at `g_a2h_slotw_pages[alias-1] + off` — the faulting ALIAS's own page base plus the SLOT's offset.**

**⚠ AND THAT IS THE KEY: `off` is derived from `slot_va`, NOT from `fault`.**

**So `pre` is ALWAYS the value at the SLOT's offset within whichever alias faulted — regardless of which address in the page faulted.**

## ⚠ THE SESSION'S `pre=B8077500` PUZZLE IS RESOLVED — and it was a misreading, not a bug

**The Session flagged:** *"`pre=B8077500` is IDENTICAL across all 128 writes at DIFFERENT addresses. Either
the page genuinely held that pattern throughout, or `pre` is read from a fixed location."*

**The second hypothesis is CORRECT in a precise sense — and it is BY DESIGN:**

> **`pre` IS read from a fixed OFFSET (the slot's, `0x62C`), though not from a fixed ADDRESS (the alias base
> varies).** **So every event's `pre` reports THE SLOT'S CURRENT VALUE at the moment of the fault — which is
> exactly the field's purpose: it is the slot's pre-write value, recorded alongside the faulting address so a
> reader can see the slot's state when any page write occurred.**

**So `pre=B8077500` identical across 128 events means THE SLOT GENUINELY HELD `B8077500` THROUGHOUT** — **it
was not changing while the fill advanced through the lower page.** ✓

**The Session had read `pre` as *"the value at the faulting address"*, which is what its NAME suggests.** **It
is actually *"the slot's value at the fault"*, which is what the FIELD is for.** **A naming ambiguity, not a
defect.**

## ⚠ And the `post` field is a DIFFERENT thing — which the Session also misread

**Looking at the recorded events:**

```
index=0   kind=1 fault_va=0019D000  pre=B8077500 post=00000000
index=1   kind=2 fault_va=0019D62C  pre=B8077500 post=B8077500
```

**`kind=1` is the WRITE event and `kind=2` is the STEP event.** **The `post=00000000` on `kind=1` is NOT the
slot's post-value — it is the write record's second field, and the STEP record (`kind=2`) is what reads the
slot AFTER the write.**

**So the correct reading of the pair is:**

| Event | `fault_va` | `pre` | `post` | Meaning |
|---|---|---|---|---|
| **`kind=1`** (`A2H_SLOTW_EV_WRITE`, `:2706`) | **`0x0019D000`** (the zeroing write) | **the SLOT's value** | **`0`** | **a write to `0x0019D000` occurred; the slot was `B8077500`** |
| **`kind=2`** (`A2H_SLOTW_EV_STEP`, `:2707`) | **`0x0019D62C`** (the SLOT) | **the SLOT's value** | **`B8077500`** | **the step re-read the slot through the same alias; it is UNCHANGED** |

**Session-verified from the code:** **`A2H_SLOTW_EV_WRITE = 1` (`:2706`)** and **`A2H_SLOTW_EV_STEP = 2`
(`:2707`)**; **the step handler at `:2857` reads `post` at `base + (slot_va & (PAGE_SIZE-1))` through the SAME
alias the store used** — **which its own comment explains:** *"a store that landed through a mirror is read
back through that mirror -- an alias whose write is read back canonically would report the wrong value if the
views ever diverged."* ✓

> **So the slot was `B8077500` before and after every one of the 128 fills.** **The fill was NOT touching the
> slot.** ✓ **And the `kind=2` reads at `fault_va=0019D62C` are the STEP events reading the slot after each
> write — which is the instrument doing its job.**

## What this establishes, and what it does NOT

**ESTABLISHED:**

1. **`pre` is the SLOT's value at the fault, read from the faulting alias's page base plus the slot's
   offset.** **Correct by construction.**
2. **`off` derives from `slot_va`, not from `fault`** — so `pre` is alias-consistent, which is right for a
   slot-keyed ledger.
3. **`post` on a `kind=1` record is not the slot's post-value**; **the `kind=2` step record is.**
4. **Therefore `pre=B8077500` repeated 128 times is CORRECT and means the slot held that value throughout
   the fill.**

**NOT established:**

- **Whether `pre` is read BEFORE the write executes.** **It is read in the fault handler, so before
  `CONTINUE_EXECUTION` — but the Session notes the value is read at a moment when the page is protected and
  the write has NOT yet landed, so it should be the pre-value.** **Q3(b)'s fixture is what must PROVE this
  end-to-end.**
- **Whether the aliased read is coherent with the canonical view** — **the implementation record already
  measured that a store through mirror view 1 does NOT become visible in the canonical view, and the
  instrument reads through the SAME alias the store used, which is the correct choice.** ✓

## Q3 status

| # | Requirement | Status |
|---|---|---|
| **(a)** | **static inspection of the pre-read path** | ✅ **EXECUTED — the path is CORRECT** |
| **(b)** | **a COMMITTED positive-control fixture proving value fidelity end-to-end** | **NOT YET — this is what remains** |
| **(c)** | **cross-validation where fault-record and hook reads overlap** | **NOT YET** |

> **Q3(a) DISCHARGES THE STATIC QUESTION AND DOES NOT DISCHARGE THE GATE.** **The Advisor required all three,
> and (b) and (c) remain.** **The Session records (a)'s result and does NOT treat it as sufficient.**

**And the Session notes the honest boundary:** **Q3(a) establishes that the CODE is correct; it cannot
establish that the VALUE is correct at runtime.** **That is exactly why the Advisor required a fixture and a
cross-validation.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation change.** **No synthetic completion.**
**No DR record cited.** **`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**;
**`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened.** **No further ON run.**
