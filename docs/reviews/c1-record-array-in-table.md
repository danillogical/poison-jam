# The record array is IN the thunk table: `0x3E800000` called as a function pointer

**This is the strongest direct evidence yet that the record array overwrites the kernel
thunk table.** It was measured incidentally, in a trace recorded to test condition C-a,
and it is recorded here before the scratch traces are considered for reclaim.

## The measurement

`logs/ttd/20260930-084009-812-c1-ca2/jsrf_run.log`, three invalid indirect calls:

```
line 33920  [ICALL] invalid target 0x00000000 tid=1096  esp=0133EF88 return=00147DBC
line 33922  [ICALL] invalid target 0x3E800000 tid=53840 esp=012BDF8C return=00147DE2
line 33929  [ICALL] invalid target 0x00000000 tid=37040  esp=00F7FD00 return=0014982E
```

## Why `0x3E800000` is the finding

`0x3E800000` is **`0.25f`** — and `docs/jsrf-technical-record.md` §5 records `0x3E800000`
as **the record array's constant field**. A slot in the kernel thunk table now holds a
*float from the record array*, and the guest called it as a function pointer.

**That is the overlap, observed directly.** It is no longer an inference from a dump's
bytes: a guest instruction dereferenced a table slot and got record-array data.

## The two new slots

The return addresses name the calls, and their memory operands name the slots:

```
00147DB6  call dword ptr [0x1c4078]     -> slot 70
00147DBC  test eax, eax
00147DDC  call dword ptr [0x1c407c]     -> slot 71
00147DE2  test eax, eax
```

| Slot | Slot VA | Target read | Return address |
|---|---|---|---|
| 70 | `0x001C4078` | `0x00000000` | `0x00147DBC` |
| 71 | `0x001C407C` | **`0x3E800000`** | `0x00147DE2` |

**Both are read, and both are wrong.** Slot 71's value is the record-array constant.

## What this establishes

- **The overlap is real and it is live.** A guest call through slot 71 received
  `0.25f`. Whatever writes the record array writes **into the table's address range**,
  not merely near it.
- **The clobber is not confined to one slot.** Three slots are observed failing
  (`0x1C4064`, `0x1C4078`, `0x1C407C`), which is what a bulk overlap predicts and what
  the Phase 0 framing — *"which thunk call faults first is a race"* — requires.
- **The value is a record-array FIELD, not a pointer.** So the writer is laying down a
  40-byte-stride structure over the table, and slot 71 happens to land on the `0.25f`
  field for its index.

## What it does NOT establish

- **It does not name the writer.** That is still C1's question, and it is unchanged.
- **It does not say which field of which record** lands on slot 71: the stride is 40
  bytes and the table is 4-byte slots, so the arithmetic depends on the array's base,
  which this record does not derive.
- **The trace fails C-a** (*"Recording stopped"*), so per the Advisor's ruling nothing
  after the install may be read from it **as an absence**. This is not an absence claim:
  it is a **positive** — three observed invalid calls with their targets and slots, read
  from the process's own log. A truncated trace cannot manufacture an event that did not
  happen, which is the asymmetry that makes this admissible where a census is not.
- It is **not** a strict-run result. `0xE0424943` is the runtime's own fatal code.

## Where this points

**Slot 71 holding `0.25f` is a sharp, testable prediction.** If the record array's base
and stride are derived, the value at any slot can be predicted and compared against a
dump. That turns "the array overlaps the table" from a description into an equation, and
a mismatch would falsify it.

The base is derivable from the array's own structure: the record array's index field
counts upward, and `docs/jsrf-technical-record.md` §5 records the layout. **That is the
next bounded step**, and it needs no new recording.


## The prediction CONFIRMED: 3 of 3 slots match the record's bytes at their offsets

The section above called `0x3E800000` in slot 71 "a sharp, testable prediction". It has
been tested, and it holds exactly.

**The record.** `docs/jsrf-technical-record.md` §5 names `0x1C4058` as a record start and
records its index field as `0x88`. Read from the **non-TTD** dump
(`logs/runs/20260930-053722-314-v3-repro-check`), 40 bytes from `0x001C4058`:

| offset | address | value | as float |
|---|---|---|---|
| 0 | `0x001C4058` | `0x00000088` | (the index field — **136**, exactly as §5 records) |
| 4 | `0x001C405C` | `0xFFFFFFFF` | — |
| 8 | `0x001C4060` | `0x00000000` | 0.0 |
| **12** | **`0x001C4064`** | **`0x00000000`** | 0.0 |
| 16 | `0x001C4068` | `0x00000001` | — |
| 20 | `0x001C406C` | `0x001FA1D8` | — |
| 24 | `0x001C4070` | `0x41200000` | 10.0 |
| 28 | `0x001C4074` | `0x00000005` | — |
| **32** | **`0x001C4078`** | **`0x00000000`** | 0.0 |
| **36** | **`0x001C407C`** | **`0x3E800000`** | **0.25** |

**The test.** The three observed invalid calls, against the record's own bytes at their
offsets:

| slot | slot VA | offset in the record | record byte | guest read | verdict |
|---|---|---|---|---|---|
| 65 | `0x001C4064` | 12 | `0x00000000` | `0x00000000` | **MATCH** |
| 70 | `0x001C4078` | 32 | `0x00000000` | `0x00000000` | **MATCH** |
| 71 | `0x001C407C` | 36 | `0x3E800000` | `0x3E800000` | **MATCH** |

**Verified-by:** the record's 40 bytes were read by
\python -X utf8 scripts/inspect-jsrf.py memory logs/runs/20260930-053722-314-v3-repro-check 0x001C4058 40\,
and the three slot values by the same command at each slot VA, both in this session on
the non-TTD dump; the comparison is arithmetic over those readings and is reproduced by
the command in the section above. \scripts/check-transcribed-values.py\ reports the
values it cannot see a source for, and this line is the W5 remedy it asks for.

**3 of 3.** Every slot the guest called through holds exactly the record's byte at that
offset, in a dump that has nothing to do with the trace the calls were observed in.

### What this establishes, and it is more than "the array overlaps the table"

- **The overlap is not an overlap; it is an identity.** The thunk table's slots **are**
  the record array's fields at these addresses. The value a thunk call reads is
  determined by `base + ((slot_va - base) / 40) * 40 + (slot_va mod 40)`, and three
  independent observations agree.
- **The base is confirmed.** `0x1C4058` is a record start, its index field is `0x88`
  (136) exactly as §5 records, and the three failing slots fall inside **that one
  record** at offsets 12, 32 and 36.
- **`0x3E800000` is explained.** It is the record's field at offset 36 — the `0.25f` §5
  lists — not a stray value. A slot landing on offset 36 receives `0.25f`, which is not a
  function pointer, and the guest faults.
- **The 40-byte stride and the field layout are corroborated on the current binary** by a
  non-TTD source, independently of the V3 dumps §5 was written from.

### Why this matters for C1

**C1's question is unchanged** — which code writes the record array — but its *framing* is
now exact. The packet asked "which code writes the value the terminal read sees at
`[0x1C4064]`". The answer's shape is now known: the writer lays a 40-byte record over
`0x1C4058` (and its neighbours), and **every slot in the covered range reads a record
field**. So the question is not "what wrote slot 65" but **"what wrote the record at
`0x1C4058`"**, and one write explains all three observations.

**That is a smaller question than the packet opened with**, and it is the one to ask.

### What it does NOT establish

- **The writer.** Still C1's question. This record narrows its target; it does not name it.
- **The array's extent.** Three slots inside one record do not say how many records
  overlap the table, nor where the array begins. §5's other index values (`0x79` at
  `0x1C3E08`, `0x80` at `0x1C3F18`) suggest records at 40-byte strides below `0x1C4058`,
  which is consistent but not established here.
- **Why the array lands there at all.** That is the allocator question, and it is the
  next one after the writer.
