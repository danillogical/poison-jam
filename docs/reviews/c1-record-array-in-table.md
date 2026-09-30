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
