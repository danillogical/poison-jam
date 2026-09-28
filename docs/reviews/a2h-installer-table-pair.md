# `A2h` — item 1 progress: `+0x3C4` and `+0x3C8` are installed as an ADJACENT TABLE PAIR

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the chain-completion packet's **item 1** is the ARG1/ARG3 provenance, and its **item 4** is
the folded-in sweep. **The Session examined the installer's context offline and found a structure that
sharpens both.**

---

## The finding, decoded from a DECLARED boundary

**The installer at `0x000D4684` is an interior instruction of `sub_000D4590`
(`0x000D4590..0x000D47A4`, from `src/recomp/gen/recomp_0001.c`'s `Original:` header — a DECLARED boundary, so
this decode does not drift):**

```
000D466E  mov edx, [0x216d64]
000D4674  mov [esi + 0x144], edx
000D467A  mov dword ptr [esi + 0x3c4], 0x257cf0     <== table A
000D4684  mov dword ptr [esi + 0x3c8], 0x257de0     <== table B  (entry 8 = 0x000D4DA0)
000D468E  mov eax, [eax]
000D4690  push 1
000D4692  push eax
000D4693  call 0x5e790
000D4698  mov ecx, [esi + 0x18c]
000D469E  mov [esi + 0x19c], eax
```

> ## **`+0x3C4` and `+0x3C8` are written ADJACENTLY, each with a table base.**
>
> **That is a TABLE PAIR — two 4-byte pointers at consecutive offsets, installed in one straight-line
> sequence.**

## Why this matters for the successor

**1. It gives item 4's sweep a structural target.** **A dispatcher for these tables would index ONE of the
pair, and the two bases (`0x257CF0` and `0x257DE0`) are `0xF0` apart — so they are two separate tables, not
one.** **The Session records the pair so a sweep does not treat `+0x3C8` in isolation.**

**2. It refines the object-binding question.** **The Session's earlier `+0x3C8` lead was refuted because a
DIFFERENT object uses `+0x3C8` as a count.** **This installer's object uses BOTH `+0x3C4` and `+0x3C8` as
table bases** — **so the two objects are distinguishable by the PAIR, not by `+0x3C8` alone.**

**3. It suggests the dispatch shape.** **Two adjacent table pointers on one object is the classic
`{vtable, vtable}` or `{dispatch, data}` layout** — **and the `call 0x5e790` immediately after is a
candidate consumer.** **The Session records `0x5e790` as a LEAD, not a finding** — **it was not traced.**

## What this does NOT establish

- **Whether `0x5e790` reads `+0x3C4` or `+0x3C8`.** **Untraced.**
- **Whether either table is indexed with the worker's ARG1.** **Untraced — and that is item 1's question.**
- **Whether `0x257CF0` and `0x257DE0` are related beyond adjacency.** **Untraced.**
- **Anything about ARG1 or ARG3's values.** **Item 1 remains OPEN.**

**So this NARROWS item 4 and gives item 1 a lead, and closes nothing.**

## The method note

**The Session found this by asking what ELSE the installer sets on the same object** — **a question neither
the packet nor the previous evidence asked.** **The previous record examined `0x000D4684` as a single
instruction; the Session decoded its surrounding sequence from a DECLARED boundary and found a pair.**

**And the boundary source matters:** **`0x000D4684` is NOT in `config/recovered-functions.json`**, **so its
declared boundary comes from the generated source's `Original:` header.** **The Session states which source it
used, as the packet requires.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
