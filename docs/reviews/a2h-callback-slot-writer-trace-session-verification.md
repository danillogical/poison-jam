# `A2h-callback-slot-writer-trace-r1` — execution evidence: **`O-OPEN`**, and the edge is now a SINGLE VALUE

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-callback-slot-writer-trace-r1`, frozen
**`C5FD9CAA405819255E0FF63DE35DB21E7F41E3F16E3B392842BF5CD44CD32A76`**.
**Worker:** `4e5bfeb3-d583-484e-8890-fc4b8c2cb30f`, commit `cb2715b`.
**Finding:** `docs/reviews/a2h-callback-slot-writer-trace-evidence.md`.

**Row: `O-OPEN`** — correctly selected, and the search **collapsed an open question into ONE unresolved
value.**

---

## The Session verified the load-bearing claims independently

| Claim | Session verification |
|---|---|
| **exactly ONE store covers `software_device+0x242C`** | **CONFIRMED** — `0x0018CE34 mov ecx,[0x19dce0]` then **`0x0018CE3A mov [ecx+0x242c], eax`** |
| **the alignment-independent backstop** | **CONFIRMED** — a raw scan for the displacement bytes **`2C 24 00 00`** returns **EXACTLY ONE occurrence in the entire XBE**, at file `0x17D2FC` → **VA `0x0018CE3C`** |
| **the open computed store** | **CONFIRMED arithmetically** — **`4×0x810 + 0x3EC = 0x242C`** exactly |
| **the forwarding thunk `0x00153790`** | **CONFIRMED** — `mov eax,[esp+0x10] / mov ecx,[esp+0xc] / mov edx,[esp+8] / push eax / push ecx / push edx / call 0x199db0` |

**The raw-byte backstop is the strongest single result.** **It is alignment-independent**, so it does not
depend on decoding from a correct boundary — **and it returns exactly one hit.** **That converts "no other
static writer found" into "no other static writer exists in the image," which is a materially stronger
claim.**

## ⚠ The Session's mandated shift check is CORRECT — and the Worker mis-computed it

**The Worker reported:** *"The secondary expression `(v1 & 0x00FFFFFF)==(v0>>8)` does NOT hold at 4-byte
granularity (`0x00766A` vs `0x0030766A`)."*

**The Session settled it BY CONSTRUCTION from the known string bytes, with no tool involved:**

```
v0 = le32(bytes[0:4]) = 0x30766A64     (the tool printed 30766A64 ✓)
v1 = le32(bytes[1:5]) = 0x3030766A     (the tool printed 3030766A ✓)

v1 & 0x00FFFFFF = 0x0030766A
v0 >> 8         = 0x0030766A
EQUAL: True
```

**And it holds identically by algebra:** `v0 = b0 | b1<<8 | b2<<16 | b3<<24` and
`v1 = b1 | b2<<8 | b3<<16 | b4<<24`, so **`v0>>8` and `v1 & 0x00FFFFFF` are both `b1 | b2<<8 | b3<<16`.**

> **So the check is correct, and the Worker's `0x00766A` is a mis-computation** — it appears to have dropped a
> byte. **The control PASSED; the Worker's secondary arithmetic did not.**

**Recorded because the Worker honestly reported a failing check rather than dropping it** — **which is the
right instinct — and because the correct response is to settle it by construction rather than accept either
party's arithmetic.** **This is the Session's own control set applied to a disagreement about the control.**

## The result: the question is now ONE VALUE

**8880 indexed stores image-wide; exactly 5 have a device/context-derived base. FOUR are EXCLUDED from the
bytes** (indices bounded to `{0,1}` or `[0x15,0x54]`; displacements `0xa78`/`0xc` that cannot reach `0x242C`
for any index).

**ONE remains open:**

```
0x00199F45  mov dword ptr [esi + ebp*4 + 0x3ec], eax     esi = MEM32(0x19DCE0)
```

**It hits the slot iff `4·ebp + 0x3EC == 0x242C`, i.e. `ebp == 0x810`.** **And `ebp` starts at the third
argument and increments per iteration, so it is reachable exactly when that argument lies in
`[1033, 2064]`.**

> **The sole caller is `0x0015379F` inside the forwarding thunk `sub_00153790`, and the deciding quantity is
> THE THIRD ARGUMENT TO THAT THUNK.** **That single value decides whether this store can reach the callback
> slot.**

**So the Session's question has gone from *"who writes the slot?"* to *"what is one argument?"*** **That is a
large, concrete narrowing.**

## The second, independent edge — recorded so it is not lost

**The Worker found something no `0x2268`-based scan could see:**

```
0x00192562  add ecx, 0x2268
0x00192568  mov [eax + 0xc], ecx      ; stores the CONTEXT into ANOTHER object
```

> **So a SECOND object holds the context at `+0xc`.** **Any later write through that field is a context
> ALIAS, formed once and stored — invisible to any scan keyed on the `0x2268` displacement.**

**This is a genuinely independent edge, and the Worker flagged it as the fallback if the first comes back
negative.** **Recorded as such.**

## What `0x001D5078` is — the Worker's shape observation, correctly held at arm's length

**`0x001D5078` is the guest VA of the ASCII filename `"djv000_0.adx"`**, with **59 occurrences image-wide, all
in `.rdata`, forming an id→filename pointer table at `0x001D4BD0` whose base has ZERO references.**

**A filename being called as code is the signature of type confusion or a misaligned table read, not a
plausible callback assignment.** **The Worker recorded that as the SHAPE of the answer only, and explicitly
declined to let static resemblance select `O-DATA-AS-CALL`.** **That is exactly right, and the Session
endorses the restraint.**

## The instrumentation warning — a real trap, now on the record

> **`inspect-jsrf.py disasm <start> <end>` decodes FROM THE REQUESTED START**, so **a start that is not an
> instruction boundary yields a plausible-looking MISALIGNED listing.**

**The Worker saw the installer call site print as `0x00123119` on one invocation and `0x00012319` on
another — and only the second is real.** **All its cited disassembly came from decoding each section from its
own start and slicing.**

**And `.rdata`/`.data` both carry `X` in their section flags**, so **a sweep trusting "code sections only"
would have skipped them.**

**Both are recorded as durable tooling facts**, and the misalignment trap is the same class as the
`disasm`-boundary rule this project already enforces.

## The C-5 ledger — traced, and correctly kept separate

| Item | Status |
|---|---|
| **caller of `sub_00194ADD`** | **TRACED — `0x00192135`** (the only rel32 reference image-wide) |
| **`KeInitializeDpc` site** | **LOCATED — `0x00194AF4`**, ordinal **107** |
| **`KeInsertQueueDpc`** | **UNLOCATED** |
| **the DPC object in the gated dump** | `DpcRoutine=0x00194480`, `DpcContext=0x0019D468` = context, **`DpcData[0]=0` — NOT queued** |

**The Session's earlier C-5 record said the caller was untraced; the Worker traced it.** **And the dump shows
the DPC is INITIALISED BUT NOT QUEUED**, which is consistent with that run never reaching the terminal.

**No inference is drawn from any of it about the slot writer — exactly as the packet required.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited**; **float-bit siblings contrastive.** **All eight guards pass.**
