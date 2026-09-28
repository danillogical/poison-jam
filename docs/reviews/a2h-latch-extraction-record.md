# `A2h` latch extraction — the archived decision record is now READABLE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why this record exists:** both the stage-1 acceptance reviewer and the successor Planner flagged that the
latch's own fields (`install_ok`, `claimed`, `slots[]`, `overflow`) are **not printed by the collector** — it
prints the *thread* registry's `claimed`, not the latch's. The Planner made it a freeze condition: *"need
verify terminal snapshot extraction before approval."*

**Resolved: the latch IS in the archive and is now extractable.** Tool:
`scripts/a2h-read-registry.py` (**17 self-tests OK**).

---

## The gap, and why it was real

`tools/harness/collect.c` adds the whole registry to the minidump's extra-memory set:

```c
extra_memory[extra_count].base = address; extra_memory[extra_count++].size = sizeof(*registry);
```

so the latch bytes **are** archived. But the registry lives at a **64-bit HOST address**
(`0x00007FF745DF9000`), while `scripts/inspect-jsrf.js memory` reads **guest** addresses and rejects a 64-bit
VA outright:

```
Inspection failed: guest read must fit 32-bit VA space and the 1 MiB inspection limit
```

**So no checked-in tool could read the latch**, even though it was present. That is the gap the Planner
identified, and it was real.

## The tool, and how its layout was pinned

The tool computes the `JsrfRegistry` layout from the C struct and **validates that layout against real
bytes** rather than trusting the arithmetic:

1. the registry header `(version, claimed, overflow, reserved)` must occur **exactly once** in the dump;
2. the latch's install record `(install_seen=1, install_raw=0x80000115, install_value=0xFE000104,
   install_ok=1)` must occur **at most once**;
3. **the DISTANCE between the two must equal the layout-derived `LATCH_OFF` exactly.**

**Check 3 is what makes it sound:** the two signatures are independent, so a wrong struct layout would put
them at the wrong distance.

### A Session error this caught — the layout was wrong

The first version used **`EVENT_SIZE = 24`**. The correct value is **32**: `struct JsrfEvent` is
`{u32 sequence, kind; u64 ticks; u32 target, site, esp, value}`, the `u64` forces 8-byte alignment, and the
struct rounds to 32. **That single wrong constant shifted every derived offset**, so the reader reported the
latch as **absent from a dump that contains it** — an absent-record-read-as-negative error, the same class
this project keeps meeting.

**Corrected, and measured against a real archive:**

| Quantity | Value |
|---|---|
| `EVENT_SIZE` | **32** (was 24) |
| `THREAD_SIZE` | **4208** |
| `LATCH_OFF` | **538648** |
| `REGISTRY_SIZE` | **543800** |
| **Measured** distance between the two signatures in Run 2 | **538648 — exact match** |

**The measurement is now a test**, so the constant cannot silently drift again.

## The extracted records — Run 2 (authoritative) and Run 1 (inertness control)

| Field | **Run 1 (gates OFF)** | **Run 2 (gates ON)** |
|---|---|---|
| registry `version` | **2** | **2** |
| thread records | 5 | 5 |
| **install record present** | **NO** | **YES** |
| `install_seen` | 0 | **1** |
| `install_raw` | — | **`0x80000115`** |
| `install_value` | — | **`0xFE000104`** |
| **`install_ok`** | — | **1 — the positive control PASSED** |
| latch `claimed` | 0 | **0** |
| latch `overflow` | 0 | **0** |
| latch `partial` | 0 | **0** |
| recorded transitions | 0 | **0** |

**Three things this establishes that were previously unverifiable:**

1. **The install positive control is confirmed from the frozen archive itself**, not only from a log line —
   `install_ok = 1` is the latch's own verdict, written by the code that performed the observation.
2. **Run 1 is confirmed inert at the RECORD level, not just the log level:** the install callback never ran
   (no record), and no transition was ever claimed. **That is a stronger inertness control than "no log
   lines appeared."**
3. **`overflow = 0` and `partial = 0` in both runs** — no latch slot was exhausted and no publication was
   torn, so the latch's own fail-closed indicators are clean.

**And it confirms the row independently:** **zero transitions claimed** in the authoritative run means the
latch never observed a first `nonzero→0` transition for any thread — which is exactly what
`O-NO-BOUNDARY-TRANSITION` requires, now read from the **decision record** rather than inferred from logs.

## What this does NOT change

**The row is unchanged** (`O-NO-BOUNDARY-TRANSITION`), and **nothing here weakens the row's stated limit: it
does not establish that the slot was never zero.** The latch samples only at bridge boundaries; the terminal
read is outside every window it observes. **The successor packet's question is untouched.**

**`install_ok = 1` also does not upgrade any claim** — it confirms the install pair matched prediction, which
was already established from the log. Its value here is that the **archive itself** now carries the verdict.

## Remaining advisory (carried, not fixed)

**The collector does not PRINT the latch's fields.** `stacks.txt` shows the thread registry's `claimed`, so a
reader must use this tool rather than reading `stacks.txt`. **The successor packet should require the
collector to print a latch summary line** — that is a small, durable improvement, and without it the
extraction tool is the only path to a decision record that a human reviewer would reasonably expect to find in
the archive's own text.
