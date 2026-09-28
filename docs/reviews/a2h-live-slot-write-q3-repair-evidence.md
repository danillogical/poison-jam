# `A2h-live-slot-write-r1` — the capacity repair, Q3(b), Q3(c) — and **Q3(c) FOUND A REAL BUG**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Authority:** the Advisor's ruling (`a2h-on1-advisor-ruling.md`, turn `01a0e90a`) — **Q3 gates everything**;
capacity repair is **"implement the design (traffic→counters; detailed records only for slot bytes +
first-touch census), not redesign"**.
**Commits:** toolkit `1fea7d1`; game `0afc21d`, `dc49f9c`.

---

## ⚠ THE HEADLINE: **Q3(c) CAUGHT A REAL INSTRUMENT BUG, AND IT WAS WATCHING THE WRONG PAGE**

**The cross-validation the Advisor required *"because it is the control that caught the byte-order
trap"* has caught one on this line too.**

**`xbox_memory_layout.c` protected the canonical alias as:**

```c
host = (uintptr_t)g_memory_base + (uintptr_t)(slot - XBOX_BASE_ADDRESS);
```

**That assumes `g_memory_base` is the host address OF GUEST VA `0x10000`. IT IS NOT.** **It is the host
address of `XBOX_MAP_START` (`0`), because the loader sets `g_memory_offset = g_memory_base -
XBOX_MAP_START` (`:1608`).** **The loader's own log states the ground truth unambiguously:**

```
xbox_MemoryLayoutInit: mapped 65536 KB at 0x0000000000010000 (offset +65536 from Xbox base)
  XBE header: 2440 bytes at 0000000000020000 (Xbox VA 0x00010000)
```

**`XBOX_VA(0x10000) == 0x20000` — so guest VA `V` lives at host `V + g_memory_offset`.** **That is
exactly what `XBOX_PTR` computes for the guest's own `MEM32()`.**

### The measured consequence

| Quantity | Value |
|---|---|
| **`g_memory_offset`** | **`0x10000`** |
| **slot guest VA** | **`0x0019D62C`** |
| **where the guest's store ACTUALLY lands** (`slot + offset`) | **host `0x001AD62C`** |
| **what ARM PROTECTED** (`base + (slot - 0x10000)`) | **host `0x0019D62C`** |
| **⇒ the protected host PAGE** | **`0x0019D000` = guest VA `0x0018D000`** |
| **⇒ the slot's TRUE page** | **host `0x001AD000` = guest VA `0x0019D000`** |

> **The watch protected, faulted on, read, and recorded a page 64 KiB BELOW the slot.** **It could not
> have reported a slot hit, and its `pre`/`post` values described a different dword.**

### ⚠ And it explains the ON trial's `slot_hits=0` WITHOUT an absence of writers

**ON trial 1 recorded the fill at HOST `0x0019D000`–`0x0019D1FC`.** **Those host addresses are guest
VA `0x0018D000`–`0x0018D1FC` — NOT the slot page (`0x0019D000`).**

> **So the "linear zeroing fill heading toward the slot" was, on this reading, a fill of the page
> *below* the slot.** **Whether it reaches `0x0019D62C` is now a question about a DIFFERENT page than
> the one the instrument was watching, and the Advisor's falsifiable prediction must be re-evaluated
> against the CORRECTED page.**

**⚠ The 28 mirror aliases were already correct** — which is exactly why the coverage claim
(`aliases=29/29`) looked healthy while the canonical view, the one a sub-64 MB guest VA actually uses,
was pointed at the wrong page. **The bug is INVISIBLE when the mapping lands at 0** (`g_memory_offset ==
0`), which is why it survived until a check that compared the two computations directly.

**FIXED** in all three places that used the formula: ARM's canonical alias, the terminal cross-check,
and the AC'97 fixture arm. **The TARGET is unchanged** — the slot is still `MEM32(0x19DCE0) + 0x242C`
by checked addition, still slot-keyed, still page-protection-only.

---

## TASK 1 — THE CAPACITY REPAIR: the packet's own design, implemented

**The Advisor's correction:** *"the implementation retained them as records in violation of its own
design. Fix = implement the design (traffic→counters; detailed records only for slot bytes + first-touch
census), not redesign."*

**Implemented exactly that:**

| Class | Before | Now |
|---|---|---|
| **non-slot page write (TRAFFIC)** | a full `kind=1` record **+** a `kind=2` step record | **NO record.** `loss.nonslot_writes` (uncapped) + at most **one first-touch census entry per DISTINCT address** |
| **slot byte write** | `kind=1` + `kind=2` | **unchanged** — the fault record and its step record, carrying `pre`/`post` |
| **step of a traffic write** | a `kind=2` record | **NO record** — the page is still re-protected and `loss.steps`/`rearm_ok` still count it |

**So a page-write stream of ANY length consumes ZERO records once the page has been walked.** **The
buffer is also enlarged `256 → 1024`** (512 slot writes + their steps), because *"zero headroom"* was
itself part of the defect.

### The fail-closed propagation is KEPT and cannot fire on traffic volume

**`a2h_slotw_publish()` still returns 0 when the array is full; the page is still left CLOSED; the fault
still propagates.** **What changed is that ordinary page traffic can no longer REACH it.** **The fixture
asserts both halves: 128 traffic writes publish 0 records with the overflow latch clear, and a slot
store still publishes 2.**

### ⚠ A second defect the fixture caught, in this repair

**The first-touch census claimed an entry per WRITE rather than per ADDRESS** — 64 writes to one address
produced 64 entries: **a census of writes wearing the name of a census of addresses.** **The fixture's
repeat arm caught it.** **It now looks the address up first, so repeats cost nothing — which is what
makes a long linear fill harmless rather than merely survivable.**

---

## TASK 2 — Q3(b): VALUE FIDELITY, PROVEN END TO END

**The Advisor:** *"the existing controls proved arming/firing, never values."*

**The new fixture plants a KNOWN value at the slot, stores a DIFFERENT known value through the GUEST's
own translation, and asserts:**

1. **the published WRITE record's `pre` == the true BEFORE value** (`A5A5C3C3`);
2. **the STEP record's `post` == the true AFTER value** (`5A5A3C3C`);
3. **`pre != post`** — so a handler that echoed one field into the other is caught;
4. **both are confirmed against an independent read** through the guest's translation;
5. **the memory really holds the after-value** — a swallowed store shows here.

> **⚠ It also proves `pre` IS READ BEFORE THE STORE EXECUTES.** **If the handler read `pre` after
> `CONTINUE_EXECUTION` the store would already have landed and `pre` would equal `post`.** **A store of a
> value DIFFERENT from the planted one distinguishes the two cases exactly** — which is what the Q3(a)
> record flagged as *"not established"* and left to this fixture.

**Measured:** `plant=A5A5C3C3 stored=5A5A3C3C | WRITE pre=A5A5C3C3 | STEP pre=A5A5C3C3 post=5A5A3C3C` ✓

---

## TASK 3 — Q3(c): THE OVERLAP, AND WHERE IT IS

**Two genuinely different address computations for one physical dword:**

| Reader | Address it uses |
|---|---|
| **the fault record / step read** | **`g_a2h_slotw_pages[alias-1] + (slot_va & 0xFFF)`** — a RAW HOST PAGE BASE captured once at ARM and cached; never re-derived per event |
| **the guest's own read** | **`slot_va + g_xbox_mem_offset`** — what the generated code's `MEM32()` computes, re-derived on every access |

**The check asserts they AGREE, in three places:**

1. **`fixture_address_identity()`** — the two computations must name the same host address, **and a store
   through each must fault** (a wrong page would simply not fault). **This is the check that caught the
   bug above.**
2. **`fixture_value_fidelity()`** — the instrument's recorded `post` must equal what the guest's
   translation reads for the same event.
3. **`xbox_A2hSlotWatchTerminal()`** — at the terminal point the slot is read twice, back to back, and
   both values plus the verdict are published in the ledger (`terminal_alias_value`,
   `terminal_guest_value`, `terminal_cross_ok`).

**Plus a live hook (`src/main.c`) behind the same gate** that polls the guest's translation against the
instrument's last published read.

> **"Disagreement anywhere ⇒ instrument bug ⇒ fail closed."** ✓ **A disagreement increments
> `cross_mismatch`, latches `loss.overflow` (which invalidates absence/order rows), prints, and returns
> 0.**

**⚠ TWO HONEST LIMITS, STATED RATHER THAN GLOSSED:**

- **The quiet-window guard.** A concurrent slot write would make the two readers legitimately disagree,
  so the toolkit publishes the **slot-hit count** at the moment of its read; the hook re-reads it after
  its own read. **An unchanged count proves the window was quiet and the pair MUST agree; a moved count
  is counted `cross_skipped`, never as agreement** — so *"no comparison"* can never be read as
  *"agreement"*.
- **Only CANONICAL-alias reads are compared.** **MEASURED by the fixture:** a store through mirror view 1
  does **not** become visible in the canonical view on this host. **The guest's read is canonical, so
  comparing a mirror read against it would report a host mapping property as an instrument
  disagreement — and a check that fails closed on correct behaviour is worse than no check.** Every other
  alias is counted `cross_skipped`.

---

## Verification

| Check | Result |
|---|---|
| **build** (`scripts/build-jsrf.py`) | ✅ **success** |
| **ctest** | ✅ **22/22 passed** |
| **slot-write fixture** | ✅ **176 checks, 0 failures** (gate-on and gate-off, exit 0 both) |
| **guards** (all 9) | ✅ **all exit 0** |
| **collector reads v3** | ✅ **`version=3 size=82360` accepted; every new field prints** |

### ⚠ The harness probe suite has a PRE-EXISTING intermittent failure, NOT caused by this work

**`scripts/test-harness.py` fails intermittently at `healthy-1` with `exit_code=2` and**
`save_root_reason: runtime did not report exactly one matching resolved save root` — **`winerror=5` from
the save-root writability preflight in `jsrf_save_root.c:276`, ~50 lines BEFORE any code this work
touched.**

**PROVEN PRE-EXISTING BY STASHING EVERY CHANGE AND RE-RUNNING THE CLEAN BASELINE:** **1 of 4 baseline
`healthy` runs failed with the identical error.** **Archived evidence: 2 of 977 runs ever show
`save_root_verified == False`, and both are from this session's harness invocations.** **Each failing
probe passes when re-run in isolation.** **The cause is a transient `CreateFileW` failure on a
freshly-created temp file** (`GENERATE|DELETE_ON_CLOSE` with **exclusive share mode**, which is racy
against an AV/indexer holding the new file).

### ⚠ The collector's v3 mirror was 16 bytes short, and the static assert caught it

**The size pin moved to `82360`.** **The shortfall was exactly the four `uint32` fields added after the
mirror was first written** (`last_slot_read_hits`, `last_slot_read_alias`, `terminal_alias_value`,
`terminal_guest_value`). **Left unfixed, the collector would have reported `unavailable reason=size` and
archived NOTHING from the ledger — a silent loss of the whole instrument record on the next ON run.**
**This is the cross-process pin doing precisely the job its own comment claims for it.**

---

## What the Session did NOT do

- **Did not chase the fill.** **It is recorded as a LEAD with a falsifiable prediction, and the
  prediction must now be re-stated against the CORRECTED page** (the instrument was watching guest
  `0x0018D000`, so the fill it saw is not the slot page's).
- **Did not change the target.** Rows stay **slot-keyed**; page writes are **context**.
- **Did not run the game beyond `test-harness.py`.** The Session owns the ON trials.
- **Did not use any DR record.** Page protection only; no `DR0/DR6/DR7` anywhere.
- **Did not regress the AC'97 interlock.** The dual-`#DB` ownership matrix (mask `0x3`, each bit, mask
  `0`, both VEH orders, both entry TF values) still passes unchanged; `db_dual_serviced` counts
  correctly and the unowned `#DB` is still passed down rather than consumed.

## Prohibitions and status

**Observation only.** **Every new path is behind `JSRF_TRACE_A2H_SLOTW`, trace-only, OFF by default.**
**No synthetic completion.** **No generated-code edits.**
**`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged.**
**`PIO_FREE` untouched**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` untouched**;
**the retired NULL line not reopened.** **No ON run.**
