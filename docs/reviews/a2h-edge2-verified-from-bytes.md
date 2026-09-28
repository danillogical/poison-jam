# `A2h` — EDGE 2 verified from the bytes: `0x0018DF59` is a REAL second open store

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the stage-1 reviewer falsified the executor's exclusion of this store, and the acceptance
record carries it as **EDGE 2**. **The Session verified it independently from the original XBE, and the
bytes confirm the reviewer.**

---

## The store, from the bytes

```
0018DF53  mov   ebx, dword ptr [esp + 0x1c]     ; ebx = a CALLER-SUPPLIED WORD
0018DF57  test  ebx, ebx
0018DF59  mov   dword ptr [edi + esi*4 + 0xa78], ebx    <== THE STORE
0018DF60  jne   0x18dfa7
```

**And the base `edi` is `MEM32(0x19DCE0)`** — the executor's own record establishes that, and the reviewer
confirmed it.

## Why the exclusion was FALSE

**The executor excluded this store on the claim that `0x242C − 0xa78 = 0x19B4` is not divisible by 4.**
**The Session verified: `0x19B4 = 6580`, and `6580 / 4 = 1645` EXACTLY.**

> **So `esi = 1645` makes `edi + 1645·4 + 0xa78 = software_device + 0x242C` — the slot.**

## Why this is a WORSE store than EDGE 1, not merely a second one

| Property | EDGE 1 (`0x00199F45`) | **EDGE 2 (`0x0018DF59`)** |
|---|---|---|
| base | `esi = MEM32(0x19DCE0)` | **`edi = MEM32(0x19DCE0)`** |
| index | `ebp` = **arg1** | **`esi` = arg1, UNBOUNDED** |
| **value written** | **`eax`** | **`ebx` = `[esp+0x1c]` — an ARBITRARY CALLER WORD** |
| reach at | **`arg1 = 2064`** | **`arg1 = 1645`** |

> **EDGE 1 writes `eax`; EDGE 2 writes an arbitrary caller-supplied word.** **So EDGE 2 can place ANY VALUE
> into the callback slot — including a `.rdata` filename pointer — with no constraint from the callee at
> all.**

**That makes EDGE 2 the more plausible candidate for the observed `0x001D5078`, and the Session records that
as an OBSERVATION about the two stores' shapes, not as an attribution.** **Static resemblance does not select
`O-DATA-AS-CALL`, and the Session is not selecting it.**

## The ARG1 correction, re-verified at the bytes

**The acceptance correction says `ebp` derives from ARG1, not arg3. The Session reproduced it:**

```
00199DC0  mov  ecx, [eax + 8]
00199DC3  mov  eax, [esp + 0x20]     ; esp = E-0x14 -> [E+0xC] = ARG3
00199DC7  test eax, eax
00199DC9  mov  [esp + 8], esi
00199DCD  mov  [esp + 0x10], ecx
00199DD1  jbe  0x19a030
00199DD7  push ebx                   ; esp = E-0x18
00199DD8  push ebp                   ; esp = E-0x1C
00199DD9  mov  ebp, [esp + 0x20]     ; esp = E-0x1C -> [E+4] = ARG1   <== ARG1
00199DDD  push edi                   ; esp = E-0x20
00199DDE  mov  edi, [esp + 0x28]     ; esp = E-0x20 -> [E+8] = ARG2
```

> **So `ebp = ARG1` and `edi = ARG2`.** **The `[esp+0x20]` displacement reads ARG3 at `0x00199DC3` and ARG1 at
> `0x00199DD9` because two pushes intervened** — **and `0x00199DDE`'s `[esp+0x28]` reading ARG2 confirms the
> shift arithmetic across all three pushes.**

**The confirmation is now three-deep:** **ARG3 at one push, ARG1 at three pushes, ARG2 at four pushes — all
from the same frame, all consistent with `E+0xC`, `E+4`, `E+8` respectively.** **That is a stronger
verification than the reviewer's single-pair argument, and it settles the correction beyond doubt.**

## What this means for the successor

**The successor must carry BOTH stores and trace ARG1 at each site.** **And the Session notes the two
reachability indices are close — 2064 and 1645 — so a single ARG1 value could in principle trigger both.**
**Recorded as a hypothesis to test, not a finding.**

**And EDGE 2's `ebx = [esp+0x1c]` gives the successor a second, independent thing to trace:** **not just
whether `esi` can reach 1645, but what supplies the arbitrary word at `[esp+0x1c]`.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
