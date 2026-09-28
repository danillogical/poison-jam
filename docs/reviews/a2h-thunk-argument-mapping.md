# `A2h` — the thunk's argument mapping: worker ARG1 is the caller's **SECOND** pushed argument

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the chain-completion packet's **item 1** is *"`ARG1`/`ARG3` provenance at the
`0x00153790 → 0x00199DB0` forwarding site."* **The Session worked out the frame semantics offline, and they
narrow the search in a way the record did not have.**

---

## The mapping, derived from the instruction forms

**The vcall thunk is a `jmp`, not a `call`:**

```
000D4DA0  mov eax, [ecx]        ; eax = *this = the vtable
000D4DA2  jmp dword ptr [eax+0xcc]   ; entry 51 = sub_00153790
```

> **Because it is a `JMP`, `esp` is UNCHANGED from the original caller's frame.** **So the thunk sees the
> caller's stack exactly as the caller left it, with `[esp]` = the return address.**

**If the caller did `push a4 ; push a3 ; push a2 ; push a1 ; mov ecx,obj ; call 0x000D4DA0`, then:**

| Slot | Holds |
|---|---|
| `[esp+4]` | **`a1`** — the **FIRST** pushed (last in memory) |
| `[esp+8]` | **`a2`** |
| `[esp+0xc]` | **`a3`** |
| `[esp+0x10]` | **`a4`** |

**The thunk reads `[esp+8]`, `[esp+0xc]`, `[esp+0x10]` = `a2`, `a3`, `a4`, and forwards them in reverse:**

```
mov eax,[esp+0x10] ; mov ecx,[esp+0xc] ; mov edx,[esp+8]
push eax ; push ecx ; push edx ; call 0x199db0
```

**So at the callee, `[esp+4] = a2`, `[esp+8] = a3`, `[esp+0xc] = a4`:**

| Worker argument | Is the caller's |
|---|---|
| **ARG1** (the INDEX, `ebp`) | **the caller's SECOND pushed argument** |
| **ARG2** (the float array, `edi`) | **the caller's THIRD** |
| **ARG3** (the TRIP COUNT) | **the caller's FOURTH** |

## ⚠ The structural consequence the record did not have

> **THE CALLER'S FIRST PUSHED ARGUMENT IS IGNORED BY THE THUNK ENTIRELY.**

**That is the shape of a `__thiscall`-style wrapper where the first stack slot is reserved or vestigial and
the thunk skips it.** **It means:**

1. **The search for `ARG1` must look at the caller's SECOND pushed value**, not its first. **A successor
   looking at the first argument would find the wrong thing** — **the same class of error as the ARG1/ARG3
   displacement confusion that produced the REJECT.**
2. **The caller's first argument is still a REAL pushed value** — **so it appears in the frame and can be
   misread as ARG1 by anyone counting from the wrong end.**

## What this does and does NOT establish

**ESTABLISHED — from instruction forms only:**

- **the thunk is a `jmp`, so `esp` is preserved;**
- **the thunk reads three slots and forwards them reversed;**
- **therefore worker ARG1 = the caller's second pushed argument.**

**NOT established:**

- **The caller's identity.** **`sub_00153790` has NO direct caller** — **it is vtable entry 51 of the 52-entry
  vtable at `0x001E1270`.** **So the "caller" is whatever code performs a virtual call through
  `object+0x00` at index 51.**
- **The caller's actual argument values.** **That remains item 1's open question.**
- **Whether the ignored first slot carries meaning.** **The Session notes it as a shape, not a claim.**

**So this NARROWS item 1 without closing it:** **the successor knows WHICH slot to read and that one slot is
skipped.**

## The method note

**The Session derived this from the instruction FORMS — a `jmp` preserves `esp`, a `call` would not — rather
than from a convention.** **That is the discipline the line has needed: the mapping follows from the bytes,
not from an assumed ABI.**

**And it is the same trap-class the REJECT exposed:** **the reviewer's crux was that `[esp+0x20]` reads ARG3
at one instruction and ARG1 at another because pushes intervened.** **This finding is the SAME arithmetic
applied one level up, at the thunk, and it yields a structurally different answer — the worker's ARG1 is not
even the caller's ARG1.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
