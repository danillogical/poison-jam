# `A2h-rdata-call-target` — the identity hop is NOT just inferred; the poller's structure argues against it

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Status:** **offline finding that REDIRECTS the successor's first task.**
**Why recorded:** the accepted packet's `O-OPEN` names *"who writes `device+0x242C`"* as the next question.
**The Session's analysis suggests that question may be premature** — the object identity it rests on is
**weaker than "inferred."**

---

## What the poller actually does

**The manifest's own evidence note for `sub_00196C0B` describes the loop, and the disassembly confirms it
exactly:**

```
00196C0D  mov  edi, ecx              ; edi = the CONTEXT  (the poller's `this`)
00196C0F  mov  esi, dword ptr [edi]  ; esi = *context    = THE DEVICE
00196C11  mov  eax, [esi + 0x400700] ; device+0x400700   -- the "while nonzero" test
00196C1C  mov  ebx, [esi + 0x100]    ; device+0x100      -- the snapshot
00196C22  test bh, 0x10              ; bit 0x1000        -> call 0x00194210
00196C27  mov  ecx, edi              ; ECX = the CONTEXT, not the device
00196C29  call 0x194210
00196C2E  test ebx, 0x1000000        ; bit 0x01000000    -> call 0x00193D90
00196C36  mov  ecx, edi              ; ECX = the CONTEXT, not the device
00196C38  call 0x00193D90            ; <== THE FAILING CALL SITE
```

**The manifest's note says it in words:** *"ECX=context whose first dword is the device pointer."*

**So the failing call receives `ecx = edi = THE CONTEXT`, and the device is `[edi]` — a DIFFERENT object.**

## The contradiction with the accepted packet

**The predecessor concluded the context is `device + 0x2268`** and derived:

```
0x2268 + 0x1C4 = 0x242C
```

**from `0x0018CB60`'s `lea edx,[eax+0x2268]`.** **That alias is ARITHMETIC — it says the two offsets coincide
IF `esi` IS the device.**

**But the poller shows `esi = [edi]`, so `esi` is the device only if `edi` is an object whose FIRST DWORD IS
THE DEVICE.** **And `edi` is `device+0x2268` only if `[device+0x2268] == device`.**

> **Those are consistent only under that unestablished condition.**

**And the Session notes the structure argues against it:** **the code reads device fields through `esi = [edi]`
and passes `edi` itself as `ecx` to two callees.** **If `edi` were `device+0x2268`, then the device would be
reached by dereferencing a pointer stored *inside* the device at `+0x2268`** — **possible, but not
established, and a design that would make the "context" a device-owned object pointing back at its owner.**

## Why this matters for the successor

**The accepted packet's next question is *"who writes `device+0x242C`?"*** **But if `edi` is NOT
`device+0x2268`, then:**

- **the slot being read is `edi+0x1C4`, NOT `device+0x242C`;**
- **the entire installer chain (`sub_0018CE30` → `device+0x242C`) is IRRELEVANT to this call;**
- **and searching for a writer of `device+0x242C` would answer the wrong question.**

> **So the successor's FIRST task is not the writer. It is: WHAT OBJECT IS `edi`?**

## The checkable discriminators

1. **What is `[device+0x2268]`?** **If the device is initialised such that `device+0x2268` begins with a
   pointer back to the device, the predecessor's alias holds.** **Check `0x0018CB60`'s callers — what do they
   pass as the `esi` source?** **If the copied data begins with the device pointer, the alias is confirmed.**
2. **What is `device+0x2268`'s first dword in an archived dump?** **The `[A2HSLOT]`/collector archives carry
   guest RAM; `device` is `MEM32(0x19DCE0)`, so `MEM32(device+0x2268)` is readable offline from a run.**
   **That is a direct, cheap test.**
3. **Does any other code pass `device+0x2268` as a `this` to `sub_00193D90`?** **The reviewer found five
   distinct call sites reach it** (`0x194419`, `0x1944d3`, `0x194ab8`, `0x194f71`, `0x196c38`). **If others
   pass a literal `lea ecx,[ebx+0x2268]`, the identity is corroborated; if they pass something else, the
   context is a distinct object type.**

**Discriminator 3 is the cheapest and the most direct**, and the Session flags it as the successor's first
static step.

## What this does NOT change

- **The call site remains unique and provable** — the acceptance reviewer confirmed it independently.
- **The table remains `{u32 id, char* name}` with no callable entry** — confirmed.
- **`O-OPEN` remains the correct row** — the chain still does not close, and this **widens** the gap rather
  than narrowing it.
- **The installer chain remains corroborated AS AN INSTALLER** — `sub_0015F9E0`'s single caller does pass
  `ebx = 0` and does install `0x15F9D0` **into `device+0x242C`**. **What is now uncertain is whether that
  field is the one this call reads.**

**Recorded so the successor does not spend its effort answering a question whose premise may be wrong.**
