# ⚠ `A2h` — Q3(c) CAUGHT A REAL INSTRUMENT BUG: the watch protected the WRONG PAGE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Found by:** the Q3(c) cross-validation the Advisor required *"because it caught the byte-order trap."*
**Worker:** `74dcf9c0-20d9-4b3e-aecc-746a004ab729`, commits `ee2f48c` (game) / `4f06907` (toolkit).

> ## **The Advisor required Q3(c) as a correctness gate. It found a real bug, and the bug INVALIDATES the ON trial's central reading.**

---

## The bug, verified independently

**ARM's canonical alias used:**

```c
g_memory_base + (slot - XBOX_BASE_ADDRESS)
```

**which assumes `g_memory_base` is the host address OF GUEST VA `0x10000`.** **IT IS NOT** — it is the host
address of `XBOX_MAP_START` (0), **because `g_memory_offset = g_memory_base - XBOX_MAP_START`.**

**The loader's own log states ground truth:** **`XBE header: 2440 bytes at 0x0000000000020000 (Xbox VA
0x00010000)`** — **so `g_memory_offset = 0x10000`, and guest VA `V` lives at host `V + g_memory_offset`**,
which is what `XBOX_PTR` computes.

**The Session verified the arithmetic:**

| | Value |
|---|---|
| **the slot's guest VA** | **`0x0019D62C`** |
| **its CORRECT host address** (`slot + g_memory_offset`) | **`0x001AD62C`** |
| **the BUGGY address** (`g_memory_base + (slot − 0x10000)`) | **`0x0019D62C`** |
| **DIFFERENCE** | **`0x10000` = 65536 = 64 KiB** |

> ## **So the watch protected host `0x0019D000` = GUEST `0x0018D000`, while the slot is guest `0x0019D000` = host `0x001AD000`.** **The canonical alias was protecting a page 64 KiB BELOW the slot.**

**And the ON trial's recorded events match the BUGGY address exactly:** **`fault_va=0019D000` … `0x0019D1FC`.**

## ⚠ WHAT THIS DOES TO THE ON TRIAL'S READING

**The Worker's own statement:** *"ON trial 1's `slot_hits=0` is EXPLAINED BY THE WRONG PAGE, not by an absence
of writers. The fill it recorded at host `0x0019D000–0x0019D1FC` was guest `0x0018D000–0x0018D1FC` — **NOT the
slot page**."*

> **So the Session's ON-trial record is PARTLY WRONG, and the Session corrects it here:**

| Session's earlier claim | Status |
|---|---|
| **`slot_hits=0`** | **TRUE but UNINFORMATIVE** — **the watched page was not the slot's** |
| **"the slot was `B8077500` before and after all 128 fills"** | **⚠ VOID** — **those reads were of the WRONG page** |
| **"the fill was 267 dwords short of the slot"** | **⚠ VOID** — **a gap to the WRONG page** |
| **"a linear fill heading toward the slot"** | **⚠ RE-STATED: it headed toward the WRONG page** |
| **"the watch demonstrably works"** | **PARTLY TRUE** — **it fired 129 times and re-armed 128/128, but on the wrong page** |
| **`aliases=29/29`** | **TRUE and MISLEADING** — **the 28 mirrors were CORRECT, which is why it looked healthy** |

**⚠ THE ADVISOR'S FALSIFIABLE FILL PREDICTION** — *"continued linear zeroing reaches `0x1D62C`"* — **MUST BE
RE-STATED against the corrected page before it is used to confirm or refute anything.** **It was never a
statement about the page the instrument was watching.**

## Why the bug was invisible, and why that matters

**The Worker:** *"The bug is INVISIBLE when the mapping lands at 0 (offset==0)."*

**And the Session adds the sharper point:** **`aliases=29/29` was TRUE.** **The 28 mirror aliases were computed
by a different path and were CORRECT.** **So a reader checking alias coverage would see full health while the
CANONICAL view — the one a sub-64 MB guest VA actually uses — was pointed at the wrong page.**

> **A coverage metric that is true while the primary path is broken is worse than no metric, because it
> manufactures confidence.** **That is the second time in this line that a healthy-looking count masked a
> defect.**

## ⚠ And Q3(c) is what caught it — the Advisor's requirement vindicated

**The Advisor:** *"cross-validation where fault-record and hook reads overlap (same address+time must agree —
**the control that caught the byte-order trap**)."*

**The Worker's implementation:** **two INDEPENDENT address computations for one physical dword** — the
instrument's cached raw alias base, **and the guest's own `slot_va + g_xbox_mem_offset` re-derived each
access.** **`fixture_address_identity` asserts both name the same host address AND that a store through each
must fault.**

> **That check is what caught it.** **The instrument's cached base and the guest's own translation disagreed,
> and the disagreement was the bug.** ✓

**So Q3(c) — required as a value-trust gate — turned out to be a STRUCTURAL correctness gate as well.**
**The Advisor's insistence was decisive, and the Session records that plainly.**

## The other results

### Task 1 — the capacity repair, per the packet's OWN design

**Traffic→counters.** **Non-slot page writes publish NO record** (uncapped `loss.nonslot_writes` + **at most
ONE first-touch census entry per DISTINCT address**); **their STEP publishes none either.** **Full records
remain ONLY for slot-byte writes.** **Buffer 256 → 1024.**

**Fixture-measured: 128 traffic writes → 0 event records; a slot store → exactly 2.** ✓

**⚠ And the fixture caught a SECOND defect in the Worker's OWN repair:** *"the first-touch census claimed an
entry per WRITE not per ADDRESS (64 writes to one address → 64 entries)."* ✓

**Fail-closed KEPT, NOT WEAKENED:** *"`a2h_slotw_publish()` still returns 0 when full; the page is still left
CLOSED; the fault still propagates. What changed is that ordinary traffic can no longer REACH it."* ✓

### Task 2 — value fidelity PROVEN, including the item Q3(a) left open

**The fixture plants a known value at the slot, stores a DIFFERENT known value through the GUEST's own
translation, and asserts `pre` == the true before-value and `post` == the true after-value.**

**Measured:** **`plant=A5A5C3C3 stored=5A5A3C3C | WRITE pre=A5A5C3C3 | STEP pre=A5A5C3C3 post=5A5A3C3C`.** ✓

> **And it ALSO proves `pre` is read BEFORE the store executes** — *"a store of a different value makes
> pre==post impossible if the read were post-`CONTINUE_EXECUTION`."* ✓ **That is exactly the item Q3(a)
> explicitly left NOT ESTABLISHED.**

### Task 3 — the cross-validation, with two honest limits

**Two further defects found and fixed:**
1. **the collector's v3 mirror was 16 BYTES SHORT** — *"unfixed, the collector would have reported
   `unavailable reason=size` and archived NOTHING from the ledger — silent loss of the whole instrument
   record on the next ON run."* ✓
2. **the collector's reconciliation expected a record per relevant AV** — *"which under the packet's own
   design can never hold. It now expects `2*slot_hits + read_samples` — a number INDEPENDENT of page
   traffic."* ✓

**And the two limits are stated rather than smoothed:**
- **a quiet-window guard:** *"unchanged count ⇒ must agree, moved count ⇒ counted `cross_skipped`, NEVER as
  agreement."* ✓
- **only canonical-alias reads are compared** — *"the fixture MEASURED that a store through mirror view 1
  does not become visible canonically on this host, so comparing across aliases would report a mapping
  property as an instrument disagreement (**a check that fails closed on correct behaviour is worse than no
  check**)."* ✓

## The pre-existing harness flake, and the Worker proved it is not theirs

**The Worker:** *"`scripts/test-harness.py` fails ~1 in 4 at `healthy-1` with exit code 2, save-root
`winerror=5`, ~50 lines BEFORE any code I touched. **PROVEN pre-existing by stashing EVERY change and
reproducing it on the clean baseline (1 of 4 runs).**"*

**The Session independently hit this flake and reached the same diagnosis** — **`[SAVE] root rejected:
requested directory is not writable (winerror=5)`** — **before reading the Worker's note.** ✓
**Cause: a transient `CreateFileW` failure on a freshly-created temp file, racy against an AV/indexer.**

**The final full harness run PASSED cleanly.** ✓

## Q3 status after this work

| # | Requirement | Status |
|---|---|---|
| **(a)** | static inspection of the pre-read path | ✅ **CORRECT** |
| **(b)** | **a committed fixture proving value fidelity end-to-end** | ✅ **PROVEN** — incl. pre-before-store |
| **(c)** | **cross-validation where reads overlap** | ✅ **BUILT — and it caught the wrong-page bug** |

**Q3 IS DISCHARGED — and it paid for itself by finding the bug.** ✓

## What remains before an ON run

1. **The corrected page must be confirmed** — **the live cross-validation hook has only run where the device
   was never allocated (`samples=0`), so its LIVE path is fixture-proven but unexercised.** **The Worker
   states this.** ✓
2. **The fill prediction must be RE-STATED against guest page `0x0019D000` (host `0x001AD000`)** — **the
   previously observed 267-dword gap is a gap to the WRONG page and must NOT be reused.**
3. **Fresh OFF + NEW N=5** (a different experiment, per the ruling).

## Prohibitions and status

**Observation only.** **Page-protection only — no DR0/DR6/DR7 anywhere.** **No DR record cited.**
**No synthetic completion.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged.
**TARGET UNCHANGED:** still `MEM32(0x19DCE0)+0x242C` by checked addition, still slot-keyed.
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened.** **No ON run.**
