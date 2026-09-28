# `A2h-rdata-call-target` — the named missing edge, characterised

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the executed packet's `O-OPEN` names *"the constructor/populator of the `device+0x2268`
context"* as the smallest closable edge. **A successor packet should start from what is ACTUALLY missing, and
"absent from the translation" turns out not to be quite right.**

---

## What the Session found

### 1. `0x0018CB60` **IS** in the translation

It appears in **both** `src/recomp/gen/recomp_0003.c` **and** `src/recomp/recovered/recovered.c` — **so the
Worker's phrase *"absent from the generated and recovered translation"* needs refinement.** **The function is
translated; what is absent is a function that WRITES the field.**

### 2. `0x0018CB60` is a `rep movsd` COPY, not a zeroing

```
0018CB60  mov   eax, dword ptr [0x19dce0]      ; the device singleton
0018CB65  lea   edx, [eax + 0x2268]            ; edx = device + 0x2268  <== THE CONTEXT
0018CB6B  mov   eax, dword ptr [eax + 0x2abc]  ; a device flag
0018CB71  and   eax, 1
0018CB75  lea   ecx, [eax + eax*2]
0018CB78  shl   ecx, 8                          ; ecx = (flag&1) * 0x300  -- a DOUBLE-BUFFER selector
0018CB7C  mov   esi, dword ptr [esp + 0x10]     ; the SOURCE argument
0018CB80  lea   ebx, [ecx + edx + 0x214]        ; ebx = device + 0x2268 + 0x214 + selector
0018CB88  mov   ecx, 0xc0                        ; 0xC0 dwords = 0x300 bytes
0018CB8F  rep movsd dword ptr es:[edi], dword ptr [esi]   ; COPY 0x300 bytes
0018CB91  test  byte ptr [esp + 0x10], 2
```

**So this function COPIES `0xC0` dwords (`0x300` bytes) FROM a caller-supplied source INTO
`device + 0x2268 + 0x214 + selector`.** **It is not a zeroing constructor — it is a data-transfer into the
context, with a double-buffer selector.**

### 3. Why this matters for the missing edge

**`0x2268 + 0x214 = 0x247C`**, and the copy spans **`0x300` bytes from `0x247C`**, i.e.
**`device+0x247C .. device+0x277C`**.

**The callback field is `device+0x242C`** — which is **BELOW `0x247C`** and therefore **OUTSIDE the copied
range.**

> **So this copy does NOT write the callback field.** **The Worker's conclusion stands — the field's writer is
> still unaccounted for — but the reason is sharper than "the constructor is absent": the one context-writing
> function that IS translated writes a DIFFERENT, higher region.**

**That is a materially better statement of the gap:** it is not that the translation lacks the function; **it is
that the translated function's write range excludes the field.**

### 4. The manifest confirms the region IS translated

**`config/recovered-functions.json` carries 3074 entries**, including the D3D device region
(`0x0018C000..0x0018C900`, `0x0018CB40..0x0018CB57`, `0x0018E410..0x0018E43D`). **So the device's code is
substantially present.**

**And `config/recovery-unresolved.json` lists unresolved dependencies** — so **some** called functions are
absent, which is a **known, enumerated** set rather than an unknown.

## What a successor should do with this

1. **Stop looking for a "missing constructor."** **The gap is a MISSING WRITE, not necessarily a missing
   function** — the field is written by **exactly one translated function** (`sub_0018CE30`) whose callers pass
   a constant, **and the region-copying function writes a disjoint range.**
2. **The productive question is therefore: what OTHER code can reach `device+0x242C`?** **The Worker already
   showed `[reg+0x1C4]` has exactly one access in D3D — the read.** **So the write, if any, comes from a
   section that was NOT swept, or through a computed base.**
3. **A concrete, checkable next step:** **sweep ALL sections (not just D3D) for any store to an address that
   could resolve to `0x242C` via a device-derived base** — including `.text`, which the Worker noted has
   *"writes belonging to unrelated classes."* **Whether any of those can alias the device is the question.**
4. **And a cheaper structural check:** the double-buffer selector at `0x0018CB78` shows the context is
   **buffered**. **If the callback field sits in a region the guest treats as part of a buffer pair, an
   off-by-one or selector error could make a write land at `0x242C`.** **That is a hypothesis to test, not a
   finding.**

**Recorded as a refinement of the named edge, so a successor does not begin by hunting for a function that is
in fact present.** **No row is changed by this: `O-OPEN` stands, and the field's writer remains unestablished.**
