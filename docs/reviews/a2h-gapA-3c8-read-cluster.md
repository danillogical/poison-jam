# `A2h` — GAP A progress: a `+0x3C8` read cluster in `.text`, found by alignment-independent enumeration

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Why recorded:** the terminal packet's **GAP A** is *"find a seed class that covers `0x000D4DA0`/`0x000D4684`,
then locate the code that reads the table installed at `object+0x3C8` and calls entry 8."*
**The Session attacked it by a DIFFERENT method than the Worker's, and found a lead the Worker's probe
missed.**

---

## The method, chosen deliberately to avoid the drift defect

**The Worker's probe was recursive descent from call/jmp/vtable/absolute-pointer seeds, and it returned ZERO
sites across 39 functions touching `+0x3C8` — while itself measuring that `0x000D4684` is unreachable from
those seeds.**

**So the Session used an ALIGNMENT-INDEPENDENT enumeration instead:** a **raw byte scan for the disp32
encoding `C8 03 00 00`** (the little-endian form of `0x3C8`), **then classifying each occurrence by decoding
the instruction that CONTAINS it** (trying instruction starts 1–3 bytes before, since a ModRM `disp32`
operand is preceded by opcode + ModRM).

**This is the §6.1 rule applied correctly:** **a byte-pattern scan is alignment-independent for existence**,
**and the Session states the encoding coverage — the ModRM `disp32` form, tried at three possible starts.**

## What it found: 76 occurrences, and a dense READ cluster

| Section | Mnemonic | Count |
|---|---|---|
| **`.text`** | `mov` | **64** |
| `.text` | UNDECODED | 6 |
| `.text` | `push` / `cmp` / `lea` / `add` | 1 each |
| `D3D` | `mov` | 1 |
| `.rdata` | UNDECODED | 1 |

**And the object-field accesses are overwhelmingly READS, clustered in ONE region:**

```
0x0005F6E5  mov ecx, [esi + 0x3c8]      0x0005F7C4  mov eax, [esi + 0x3c8]
0x0005F6F4  mov ecx, [esi + 0x3c8]      0x0005F7ED  mov eax, [esi + 0x3c8]
0x0005F715  mov eax, [esi + 0x3c8]      0x0005F809  mov eax, [esi + 0x3c8]
0x0005F729  mov eax, [esi + 0x3c8]      0x0005F81D  mov eax, [esi + 0x3c8]
0x0005F74B  mov eax, [esi + 0x3c8]      0x0005F836  mov edx, [esi + 0x3c8]
0x0005F75A  mov ecx, [esi + 0x3c8]      0x0005F84A  mov eax, [esi + 0x3c8]
0x0005F797  mov eax, [esi + 0x3c8]      0x0005F859  mov ecx, [esi + 0x3c8]
0x0005F7A6  mov ecx, [esi + 0x3c8]      0x0005F87A  mov eax, [esi + 0x3c8]
                                         0x0005F889  mov ecx, [esi + 0x3c8]
                                         0x0005F8A7  mov eax, [esi + 0x3c8]
                                         0x0005F8B6  mov ecx, [esi + 0x3c8]
                                         0x0005F8D7  mov eax, [esi + 0x3c8]
                                         0x0005F8E6  mov ecx, [esi + 0x3c8]
                                         0x0005F907  mov eax, [esi + 0x3c8]
                                         0x0005F919  mov ecx, [esi + 0x3c8]
                                         0x0005F92B  mov ecx, [esi + 0x3c8]
```

> **That is 25 reads of `[esi + 0x3c8]` inside a ~0x250-byte window — the signature of a DISPATCH TABLE
> BEING INDEXED REPEATEDLY**, not of a field read.

### The cluster resolves to ONE generated function

**The Session located the DECLARED boundaries from `src/recomp/gen/recomp_0000.c`:**

| Function | Range | Size |
|---|---|---|
| `sub_0005F590` | `0x0005F590..0x0005F6AB` | 283 B |
| **`sub_0005F6B0`** | **`0x0005F6B0..0x0005F96D`** | **701 B** |
| `sub_0005F970` | `0x0005F970..0x0005FAE3` | 371 B |
| `sub_0005FAF0` | `0x0005FAF0..0x0005FC3D` | 333 B |

> **`sub_0005F6B0` contains 24 of the 25 `[esi+0x3c8]` reads** — **it is a 701-byte function whose body is
> dominated by repeated reads of one object field.** **That is the dispatcher candidate, and it has a
> DECLARED boundary, so it is decodable without drift.**

**⚠ But note: `0x0005F6E5` was NOT found in `config/recovered-functions.json`** — **the manifest does not
carry these functions**, **so their boundaries come from the GENERATED source's `Original:` headers, which
are themselves declared.** **The successor should state which source it took the boundary from.**

**Plus two `[ebx + 0x3c8]` accesses — one READ at `0x0005F973` and one WRITE at `0x0005F994`** — and a
second write at `0x00060D08`. **And `0x00061F10 mov eax,[ecx+0x3c8]`.**

## What this does and does NOT establish

**ESTABLISHED:**
1. **`+0x3C8` is accessed 76 times image-wide**, by an **alignment-independent** enumeration with **stated
   encoding coverage.**
2. **25 of those are READS of `[esi+0x3c8]` clustered in `0x0005F6E5..0x0005F92B`** — **a pattern consistent
   with a dispatch table indexed from a base held at `+0x3C8`.**
3. **The Worker's recursive-descent probe returned ZERO sites for this shape**, **so its seeding gap is
   REAL and this enumeration finds accesses it could not.** **That CONFIRMS the Worker's own caveat.**

**NOT ESTABLISHED — and these are the successor's work:**
- **Whether this `+0x3C8` is the SAME field** as the one `0x000D4684` installs `0x257DE0` into. **`+0x3C8` is
  an offset, and different object types can share an offset.** **The Session has NOT established that the
  object carrying vtable `0x1E1270` is the `esi` at `0x0005F6E5`.**
- **Whether the cluster calls entry 8 (`0x000D4DA0`)** — **the reads are followed by whatever the surrounding
  code does, which the Session has not decoded.**
- **Whether the cluster is reachable from the failing path.**

> **So this is a LEAD with a named gap, not an identification.** **The Session records it as exactly that.**

## Why the Session is recording a lead rather than pushing further

**The terminal packet is about to be frozen with GAP A as its scope, and its executor should have this
lead.** **Continuing now would duplicate the packet's work.** **And the Session notes the risk it is
avoiding: concluding that `+0x3C8` is ONE field because it has one offset is exactly the kind of
offset-arithmetic inference the Advisor barred for `device+0x242C`.**

**So the lead is recorded with its unproven step stated, and the successor must bind the object.**

## The method note, which is the durable part

**This is the second time this session that an ALIGNMENT-INDEPENDENT enumeration found what a
control-flow-following enumeration missed.** **The first was the raw scan for `2C 24 00 00`; this is the raw
scan for `C8 03 00 00`.**

> **The §6.1 rule the Advisor just placed says byte-pattern scans are alignment-independent for EXISTENCE,
> and that is exactly the role they played here: they established that accesses to `+0x3C8` EXIST in
> places recursive descent could not reach.**

**The Session does NOT claim the 76-occurrence count is a UNIQUENESS claim** — **it is an existence
enumeration over the ModRM `disp32` encoding**, **and other encodings (`disp8` with a different ModRM, or a
register-indexed form) could carry the same offset.** **Stated per the rule.**

## Prohibitions and status

**Offline and read-only.** **No run, no source edit, no instrumentation.** **No synthetic completion.**
**`PIO_FREE` stays DEFERRED**; **`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays
`UNRESOLVED`**; **the retired NULL line was not reopened and no DR record was cited.**
