# `A2h` — the named edge's PARENT: `sub_00153790` is entry 39 of a 40-entry DISPATCH TABLE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the writer trace's `O-OPEN` names **the third argument to the forwarding thunk
`sub_00153790`** as the smallest uncovered edge. **The Session traced that thunk's own provenance offline and
found its parent, which changes the edge's shape.**

---

## The finding: the thunk has NO direct caller — it is a DISPATCH-TABLE ENTRY

| Search | Result |
|---|---|
| **rel32 callers of `0x00153790`** | **ZERO** |
| **absolute dword references** | **exactly ONE** — file `0x1D13DC` → **`.rdata` VA `0x001E133C`** |
| **`0x00199DB0`** (the real worker the thunk forwards to) | **ZERO absolute references** |

**And the reference sits inside a contiguous run of 40 code-VA entries:**

```
table span: 0x001E12A0 .. 0x001E133C   = 40 entries
index 39 of 40  ->  0x00153790          <== THE LAST ENTRY
```

**The targets are NOT ascending**, so it is **not a jump table**:

```
first 8: 0x00152AE0 0x001526A0 0x00152740 0x00011C80 0x00152780 0x001525D0 0x00152360 0x0014E1F0
last  8: 0x00153240 0x001532A0 0x001537C0 0x001537B0 0x00153850 0x00153760 0x00153780 0x00153790
```

**And the targets cluster in a `0x670`-byte span (`0x001531E0..0x00153850`) — closely-spaced small
functions.** **That is a VTABLE or dispatch table, not a switch table.**

## Why this materially changes the edge

**The writer trace named *"the third argument to `sub_00153790`"* as the deciding value.** **That framing
implied a CALLER passing an argument.** **But there is no caller — the thunk is INVOKED THROUGH A TABLE.**

> **So the deciding quantity is not an argument at a call site. It is whatever the DISPATCHER supplies as the
> third argument when it selects table entry 39.** **The search must therefore move to the DISPATCHER, not to
> a caller.**

**And `0x00199DB0` having ZERO absolute references while its forwarding thunk has exactly one is itself
informative:** **the thunk exists precisely to adapt the table's calling convention to the worker's.** **That is
what "forwarding thunk" means here — `mov eax,[esp+0x10] / mov ecx,[esp+0xc] / mov edx,[esp+8] / push / push /
push / call` re-orders three stack arguments.**

**So the table's dispatcher pushes three arguments; the thunk reverses their order; the worker takes them.**
**The third argument at the THUNK is the FIRST argument the DISPATCHER pushed.**

## What this does and does NOT establish

**Established:**

1. **`sub_00153790` has zero direct callers and one absolute reference — it is a table entry.**
2. **The table is `0x001E12A0..0x001E133C`, 40 entries, non-ascending — a vtable/dispatch table.**
3. **`0x00153790` is the LAST entry (index 39).**
4. **`0x00199DB0` has zero absolute references**, so it is reached only through the thunk.

**NOT established:**

- **Which dispatcher reads this table**, and **what it passes as the three arguments.**
- **Whether the table is indexed by a device field, a command code, or something else.**
- **Whether entry 39 is ever selected** — **and if it is not, the `0x00199F45` store never executes.**

> **So the edge is not closed; it is RELOCATED.** **The Session records it as a better-posed question:
> *what dispatcher reads the table at `0x001E12A0`, and what does it pass as the first of the three arguments
> it pushes for entry 39?***

## The method note, because this is the pattern that keeps paying

**The Session found this by asking a question the packet did not: *"who calls the named edge's parent?"***
**The writer trace correctly identified the deciding VALUE but framed it as a call-site argument.** **One
step of provenance on that framing revealed the thunk is a table entry.**

**Recorded because it is the same move that found the producer: follow the named edge one level up before
writing the successor packet.**

## The shape observation, held at the same arm's length

**The writer trace noted `0x001D5078` is a filename in a table whose base has ZERO references** — **the
signature of type confusion.** **This finding does not change that, and the Session does NOT promote it.**
**A vtable entry being reached with a filename as an argument is CONSISTENT with type confusion, and
consistency is not evidence.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
