# `A2h` — the reviewer's C-5 edge: `sub_00194ADD` and the DPC activation

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the stage-1 reviewer's **C-5** found *"the producer chain is not closed at the top: no
caller of `sub_00194ADD` is traced, and no `KeInsertQueueDpc` site is located."* **The successor packet must
record this as a SEPARATE edge from the `software_device+0x242C` writer, and this establishes what IS and is
NOT present.**

---

## What the Session verified offline (XBE bytes + generated source only)

### The DPC initialisation, confirmed at the bytes

```
00194ADD  push ebp
00194ADE  mov  ebp, esp
00194AE0  sub  esp, 0x10
00194AE3  push ebx
00194AE4  push esi
00194AE5  mov  esi, ecx              ; esi = the CONTEXT
00194AE7  push esi                  ; DeferredContext = context
00194AE8  push 0x194480             ; DeferredRoutine = sub_00194480
00194AED  lea  eax, [esi + 0x84]    ; Dpc = context + 0x84
00194AF3  push eax
00194AF4  call dword ptr [0x1c4020] ; KeInitializeDpc
```

**The reviewer's byte check 4 is reproduced exactly.** **And the push order gives the argument order
unambiguously** — `KeInitializeDpc(Dpc, DeferredRoutine, DeferredContext)`.

### The IAT slot decodes to a concrete ordinal

| Slot | Thunk index | Image value | **Ordinal** |
|---|---|---|---|
| **`0x001C4020`** | **48** | **`0x8000006B`** | **107** |

**So the DPC initialiser is kernel ordinal 107.** **The reviewer identified it as `KeInitializeDpc`, and the
ordinal is now on the record so a successor can confirm the name independently.**

### `sub_00194ADD` HAS NO CALL SITE — and it is a DISPATCH TARGET

| Search | Result |
|---|---|
| **call sites in generated + recovered source** | **only `extern void sub_00194ADD(void);` declarations — ZERO calls** |
| **`recomp_dispatch.c`** | **`{ 0x00194ADDu, (recomp_func_t)sub_00194ADD }` at line 8025** |
| **`config/recovered-functions.json`** | **`0x00194ADD..0x00194C3F`** — *"GPU hardware-setup parent confirmed by original disassembly. ECX=context, EBP frame, RET 0, EAX boolean. **Initializes DPC at +0x84, Type-0 dispatchers at +0x1C8/+0x1D8**, calls recovered 0x0019460A/0x001…"* |

> **So `sub_00194ADD` is reached through the DISPATCH TABLE, not by a direct call.** **That is why no caller
> exists in the source — and it means the reviewer's C-5 is precisely correct: the entry point is a
> dispatch-table target whose own caller is not traced.**

### And the manifest names the object's role

**The same manifest entry says `sub_00194ADD` initialises:**
- **the DPC at `+0x84`** — confirmed at the bytes;
- **Type-0 dispatchers at `+0x1C8`/`+0x1D8`** — **matching the context fields the producer evidence listed**;
- **and calls recovered `0x0019460A`** — **which is the function that writes `[context] = 0xFD000000`, the
  APERTURE.**

> **So `sub_00194ADD` is the context's INITIALISER: it sets the aperture pointer, the DPC and the
> dispatchers.** **That makes it the natural place for `+0x1C4` to be set as well — and the producer evidence
> says `+0x1C4` is NOT written there statically.**

## What this establishes, and what remains open

| Question | Status |
|---|---|
| **Is the DPC initialised?** | **YES — confirmed at the bytes, ordinal 107** |
| **Is the DPC ever QUEUED?** | **UNKNOWN — no `KeInsertQueueDpc` site located** |
| **Who calls `sub_00194ADD`?** | **UNKNOWN — it is a dispatch-table target; its own caller is untraced** |
| **Is `+0x1C4` written by `sub_00194ADD`?** | **NO — the producer evidence found no static write there** |
| **The one remaining writer edge** | **still the writer of `software_device+0x242C`** |

## Why this is a SEPARATE edge, and why the distinction matters

**A DPC that is INITIALISED but never QUEUED never runs.** **So without the queue site, the poll loop's
ACTIVATION is unproven** — **and if the loop never runs, the terminal's fourth iteration never happened.**

**But this is NOT the same question as *"who wrote the callback slot?"*** **Conflating them would produce a
packet that cannot answer either.** **So:**

- **The successor packet's question is the WRITER of `software_device+0x242C`.**
- **C-5 is recorded as a separate, named edge: the caller of `sub_00194ADD` and the `KeInsertQueueDpc` site.**
- **And the Session notes a THIRD unproven link the reviewer also flagged: the runtime dispatch through the
  unresolved import thunk at `0x001C40D8`.**

## The honest summary

**The reviewer's C-5 was a real gap neither the Session nor the Advisor had named.** **The Session's
verification CONFIRMS it and sharpens it:** **`sub_00194ADD` is not merely uncalled — it is a DISPATCH target,
which is a structurally different kind of gap.** **And the manifest's own evidence note makes it the context's
initialiser, which is why its omission of `+0x1C4` is informative rather than incidental.**

**Recorded so the successor packet does not absorb C-5 into its own question, and so a reviewer can see the
three distinct unproven links separately.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
