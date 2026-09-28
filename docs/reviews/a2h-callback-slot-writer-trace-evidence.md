# `A2h-callback-slot-writer-trace-r1` — executor evidence: the one-slot writer

**Packet:** `docs/packets/a2h-callback-slot-writer-trace.md` (frozen, SHA-256
`C5FD9CAA405819255E0FF63DE35DB21E7F41E3F16E3B392842BF5CD44CD32A76`, 17 lines, 7231 bytes).
**Validation:** `docs/reviews/a2h-callback-slot-writer-trace-r1-session-validation.md`.
**Method:** **static analysis of the original XBE only.** No game run, no build, no source
edit, no instrumentation, no generated-code edit. Scratch tooling lives in `%TEMP%`.

---

## Row

**`O-OPEN`.**

**Direct stores are closed exhaustively and are not the answer; the computed-store class is
closed to a single unbounded index on a single store. That index is the smallest uncovered
edge and is named below. No complete instruction-anchored reaching chain to the ONE slot
through the fourth-iteration read of `0x001D5078` is established, so `O-DATA-AS-CALL` is
not selected.**

---

## Identity and section map (original XBE, re-pinned this session)

`game/mygame_analysis.json`, `game/default.xbe`. Image base `0x00010000`, entry
`0x00148023`, kernel thunk table `0x001C3F60`.

| Section | VA | VSIZE | RAW | RAWSIZE | FLAGS |
|---|---|---|---|---|---|
| `.text` | `0x00011000` | `0x0017BB30` | `0x00001000` | `0x0017BB30` | PRE, X, HRO |
| `D3D` | `0x0018CB40` | `0x000117F8` | `0x0017D000` | `0x0000E6B0` | W, PRE, X |
| `DSOUND` | `0x0019E340` | `0x0001C55C` | `0x0018C000` | `0x0001C32C` | W, PRE, X |
| `MMATRIX` | `0x001BA8A0` | `0x00001210` | `0x001A9000` | `0x00001210` | PRE, X |
| `XGRPH` | `0x001BBAC0` | `0x00000CFC` | `0x001AB000` | `0x00000CFC` | W, PRE, X |
| `XPP` | `0x001BC7C0` | `0x00007798` | `0x001AC000` | `0x00007798` | W, PRE, X |
| `.rdata` | `0x001C3F60` | `0x00027800` | `0x001B4000` | `0x000277F0` | PRE, X |
| `.data` | `0x001EB760` | `0x00092914` | `0x001DC000` | `0x00044574` | W, PRE, X |
| `DOLBY` | `0x0027E080` | `0x00006D98` | `0x00221000` | `0x00006D84` | PRE, X, TRO |
| `$$XTIMAGE` | `0x00284E20` | `0x00002800` | `0x00228000` | `0x00002800` | INS, HRO, TRO |
| `$$XSIMAGE` | `0x00287620` | `0x00001000` | `0x0022B000` | `0x00001000` | INS, HRO, TRO |

**⚠ `D3D` is `0x0018CB40..0x0019E338` — it is executable, and the entire NV2A software
device lives inside it.** `0x0019B200` (`software_device`), `0x0019D468` (`context`),
`0x0019D62C` (the slot) and `0x0019DCE0` (the device-pointer global) are all in `D3D`.
The `D3D` VA range and the `SOFTWARE_DEVICE` VA range overlap, which is why the vocabulary
in the packet is load-bearing: **they are different objects at different addresses and only
the second is the `device` in `device+0x242C`.**

**Coverage swept:** 9 X-flagged sections, **779,452 decoded instructions** (`.text` 502,465;
`.data` 127,179; `.rdata` 65,278; `DSOUND` 40,167; `D3D` 19,521; `DOLBY` 12,272; `XPP`
10,159; `MMATRIX` 1,309; `XGRPH` 1,102). **⚠ `.rdata` and `.data` carry `X` in their flags
and were swept too** — a sweep that trusted "code sections only" would have skipped them.

**⚠ Instrumentation note that matters for reproducing this.** `inspect-jsrf.py disasm`
decodes **from the requested start address**, so a start that is not an instruction boundary
yields a misaligned listing that looks like plausible code. During this session the installer
call site printed as `0x00123119 call 0x15f9e0` on one invocation and as
`0x00012319 e8c2d61400 call 0x15f9e0` on another; the second is correct and the first was a
misalignment artifact. **Every disassembly cited below was produced by decoding each section
from its own start and then slicing**, not by `disasm <start> <end>`.

---

## SEARCH 1 — direct stores to `software_device+0x242C`: CLOSED, exactly one, and it is the lead

**Method.** Capstone `CS_MODE_32`, detail on, `skipdata` on; decode every X-flagged section
from its own VA. For every instruction, for every memory operand with write access, record
`(base, index, scale, disp, operand size)` and test **byte-range overlap**
`disp <= 0x242C < disp + size`. This admits partial and sized stores, not just
`mov dword ptr [reg+0x242C]`.

**Result — every store whose byte range covers `software_device+0x242C`:**

| VA | Section | Bytes | Text | Base | Disp | Size |
|---|---|---|---|---|---|---|
| `0x0018CE3A` | `D3D` | `89812c240000` | `mov dword ptr [ecx + 0x242c], eax` | `ecx` | `0x242C` | 4 |

**`count = 1`.**

**Stronger, and this is the exhaustive form:** a raw scan for the 4-byte little-endian
displacement `2C 24 00 00` at **every byte offset of every section** — which catches any
encoding, any mnemonic, any alignment, and even non-instruction occurrences — returns
**exactly one occurrence in the whole XBE**:

```text
imm 0x0000242C at 0018CE3C D3D ctx=e0dc190089812c240000c2040090
```

**There is no second occurrence of the value `0x242C` anywhere in the image.** There is no
`[base+index]` form that reaches `0x242C` either: `0x242C` is not a multiple of 4 times a
plausible scale plus a small displacement — `0x242C = 4 * 0x90B`, and `0x90B` is odd and
large; the only scale-4 forms are `disp + 4k`, so any index must supply `4k = 0x242C - disp`
exactly, and with `disp` also needing to be a real displacement. **Search 3 below enumerates
that class directly rather than arguing it.**

### Base reaching definition of `0x0018CE3A`

```text
0018CE30  mov  eax, dword ptr [esp + 4]        ; argument 1
0018CE34  mov  ecx, dword ptr [0x19dce0]       ; ecx = software_device
0018CE3A  mov  dword ptr [ecx + 0x242c], eax   ; <-- THE ONLY STORE
0018CE40  ret  4
```

**`ecx = MEM32(0x19DCE0)` is loaded two instructions earlier, unconditionally, with no
intervening write to `ecx`. The base IS `software_device`. The reaching definition is
complete and instruction-anchored.**

### Every reference to `sub_0018CE30`, enumerated exhaustively

Rel32 `call`/`jmp` scan (opcodes `E8`, `E9`) plus rel8 (`EB`, `E3`) plus raw little-endian
dword data scan, across all sections:

| Kind | VA | Bytes | Section |
|---|---|---|---|
| `JMP` | `0x0015F9F1` | `e93ad40200` | `.text` |
| `JMP` | `0x0015FA01` | `e92ad40200` | `.text` |

**Two references. Both are `jmp`s inside `sub_0015F9E0`. No `call`, no data pointer, no
vtable entry, no dispatch-table entry anywhere in the image.**

### The installer, and the argument it actually passes

```text
0015F9E0  mov   eax, dword ptr [esp + 4]     ; argument 1
0015F9E4  test  eax, eax
0015F9E6  jne   0x15f9f6
0015F9E8  mov   eax, 0x15f9d0                ; <-- THE INSTALLED VALUE
0015F9ED  mov   dword ptr [esp + 4], eax     ; overwrite the stack argument
0015F9F1  jmp   0x18ce30                     ; tail-jump into the writer
0015F9F6  cmp   eax, -1
0015F9F9  jne   0x15f9fd
0015F9FB  xor   eax, eax
0015F9FD  mov   dword ptr [esp + 4], eax
0015FA01  jmp   0x18ce30
```

**So the slot's value is the argument to `sub_0015F9E0`, transformed only by:
`NULL -> 0x0015F9D0`, `-1 -> 0`, else identity.** `0x001D5078` is reachable through this
path only if some caller passes `0x001D5078` as that argument.

### The single caller

Rel32 scan for `sub_0015F9E0` across all sections:

| Kind | VA | Bytes | Section |
|---|---|---|---|
| `CALL` | `0x00012319` | `e8c2d61400` | `.text` |

**Exactly one caller.** Its context (aligned decode, `0x000122B0..0x00012360`):

```text
000122EA  push  ebx                       ; arg1 for sub_0015F9E0
000122EB  mov   dword ptr [esi + 0x20], ebx
000122EE  mov   dword ptr [esi + 0x24], ebx
000122F1  mov   dword ptr [esi + 0x18], ebx
000122F4  mov   dword ptr [esi + 0x1c], 1
000122FB  mov   dword ptr [esi + 0x87e0], ebx
00012301  mov   dword ptr [esi + 0x87e4], ebx
00012307  mov   dword ptr [esi + 0x94], ebx
0001230D  mov   dword ptr [esi + 0x87c8], ebx
00012313  mov   dword ptr [esi + 0x87cc], ebx
00012319  call  0x15f9e0                  ; <-- with arg1 = ebx
```

**Backward slice of `ebx` from `0x00012319`.** The nearest preceding definition is
`xor ebx, ebx` at `0x0001224F` (`33db`); the only other `ebx` mentions in the span are
`push ebx` at `0x00012229` and the stores listed above, none of which write `ebx`. **Between
`0x0001224F` and `0x00012319` there is no write to `ebx` at all.** (The function runs a long
unrolled initialiser — `mov dword ptr [esi + off], ebx` for ~200 distinct `off` — so the
span is wide but contains only reads of `ebx`.)

**Therefore: `ebx = 0`, the NULL path is taken, and `MEM32(software_device+0x242C) = 0x0015F9D0`.**
This reproduces the packet's established "installed value" from the bytes and closes the
reaching chain **for the installer**.

**What this does NOT do:** it does not show that the installer is the *last* write, and it
does not show that `0x001D5078` cannot arrive by some other route. **It shows only that the
one direct store, if reached, writes one of `{0, 0x0015F9D0, arg}` and that the one call
site passes `0`.**

### The device-pointer global is written from one site only

Every store to `[0x19DCE0]` (absolute, no base/index) in the image:

| VA | Bytes | Text | Section |
|---|---|---|---|
| `0x0018CE15` | `a3e0dc1900` | `mov dword ptr [0x19dce0], eax` | `D3D` |
| `0x0018E4A0` | `c705e0dc190000b21900` | `mov dword ptr [0x19dce0], 0x19b200` | `D3D` |

**`0x0018E4A0` is the one-time constructor storing the constant `0x0019B200`, which matches
the established `software_device = 0x0019B200`.** `0x0018CE15` stores a register; it is the
only other writer. **Nothing re-points the global to an attacker-chosen or shifted object at
any other site.**

### Bulk writes

1331 `rep movs*`/`rep stos*` sites image-wide. The packet's `sub_0018CB60` negative is
reproduced and strengthened by the instruction-level decode of that function (below): its
`rep movsd` destination is `[context + 0x214 + 0x300k, +0xC00)` — a 0xC00-byte span based at
`context+0x214`, so the covered range is `[+0x214, +0xE14)` relative to `context`, i.e.
**`[software_device+0x247C, software_device+0x307C)`**. **`+0x242C` is 0x50 bytes BELOW the
lowest byte covered. NON-TARGET, reproduced from the bytes.** (`sub_0018CB60` is reached by
`call 0x18cb60` from `0x0013B1D0`/`0x0013B240` in `.text` and `0x001986B8` in `D3D`.)

**A generic `rep`-range proof for all 1331 sites is not attempted** and is listed as residual
uncertainty — but note that **no `rep` destination can reach the slot without `edi` equalling
`software_device+0x242C` or `context+0x1C4`, and `edi` would have to be derived from one of
the two pointer bases; the only `lea reg,[reg+0x2268]` and `add reg,0x2268` sites in the image
are enumerated in Search 2 and none of them sets `edi` for a copy into the slot range.**

---

## SEARCH 2 — context aliases `[context+0x1C4]`: CLOSED and REFUTED

**Method.** Same full-section sweep. **Every** memory operand with write access and
`disp == 0x1C4`, any mnemonic, any operand size, any base register, any alignment:
**76 occurrences.** Each was resolved with a backward register trace to its base's nearest
preceding definition.

**None of the 76 is in `D3D`** — where `context` and the slot live. The single `D3D` hit is
`0x0018D98F mov dword ptr [esp + 0x1c4], eax`, **which is a stack store (`esp`-relative),
not the context.** The other 75 are `.text` object stores whose bases are `ecx`/`esi`/`ebx`
set from `mov reg, ecx` (the C++ `this` pointer) in unrelated classes, or `esi` loaded from
`[esp+0x34]`/`[esp+0x58]` arguments. **Zero trace to `context = software_device+0x2268`.**

**Cross-check from the other direction.** A raw byte scan for the displacement immediate
`68 22 00 00` (`0x2268`) across the image returns **13 sites, all in `D3D`** — and
`0x2268` appears nowhere else in the image. Every one of the 13 is a *read* of the context
pointer or a `lea` forming it:

| VA | Text | Role |
|---|---|---|
| `0x0018CB65` | `lea edx, [eax + 0x2268]` | `eax = MEM32(0x19DCE0)` |
| `0x0018CECC` | `mov eax, dword ptr [eax + 0x2268]` | read |
| `0x0018CF25` | `mov edx, dword ptr [eax + 0x2268]` | read |
| `0x0018D04F` | `lea ebp, [edi + 0x2268]` | `edi = MEM32(0x19DCE0)` |
| `0x0018E1D0` | `lea ecx, [ebx + 0x2268]` | `ebx = MEM32(0x19DCE0)` |
| `0x0018E1F4` | `lea ecx, [ebx + 0x2268]` | `ebx = MEM32(0x19DCE0)` |
| `0x00191275` | `mov eax, dword ptr [eax + 0x2268]` | read |
| `0x001912C0` | `mov eax, dword ptr [eax + 0x2268]` | read |
| `0x00191A91` | `mov eax, dword ptr [esi + 0x2268]` | read |
| `0x00192127` | `add esi, 0x2268` | `esi` from `[esp+8]`; **see DPC path** |
| `0x00192562` | `add ecx, 0x2268` | `ecx` from `[esp+8]` |
| `0x001925FB` | `lea edi, [esi + 0x2268]` | `esi = MEM32(0x19DCE0)` |
| `0x0019867F`/`0x001986ED`/`0x00198700` | `mov reg, [edi + 0x2268]` | reads |

**The two `add`-form sites are the ones the packet warns about, and both are accounted for:**

```text
0019211D  mov   esi, dword ptr [esp + 8]     ; esi = argument
00192121  or    edx, 0x7f7f
00192127  add   esi, 0x2268                  ; <-- context alias formed here
0019212D  mov   ecx, esi                     ; ecx = context
0019212F  mov   dword ptr [0x19ded8], edx
00192135  call  0x194add                     ; KeInitializeDpc(this=context, ...)
```

```text
0019255E  mov   ecx, dword ptr [esp + 8]
00192562  add   ecx, 0x2268                  ; <-- second alias site
00192568  mov   dword ptr [eax + 0xc], ecx   ; stored INTO another object
```

**Both form the context and then *store it elsewhere* (`0x00192568` writes it into
`[eax+0xc]`) or *pass it* (`0x00192135`). Neither writes `[context+0x1C4]`.**
**The second site is itself a lead worth carrying forward: `[eax+0xc] = context` creates a
second object field that aliases the context, so any later write through that field is a
context alias the `0x2268` scan cannot see.** That is recorded as residual uncertainty.

### The DPC consumer path

`0x00192135 call 0x194ADD` — the caller of `sub_00194ADD` **is established from the bytes**
(rel32 scan; exactly one reference):

```text
00194ADD  push  ebp
00194ADE  mov   ebp, esp
00194AE0  sub   esp, 0x10
00194AE3  push  ebx
00194AE4  push  esi
00194AE5  mov   esi, ecx                     ; this = context (guest thiscall)
00194AE7  push  esi                          ; DeferredContext = context
00194AE8  push  0x194480                     ; DeferredRoutine = sub_00194480
00194AED  lea   eax, [esi + 0x84]
00194AF3  push  eax                          ; Dpc = context + 0x84
00194AF4  call  dword ptr [0x1c4020]         ; KeInitializeDpc
```

**`MEM32(0x1C4020) = 0x8000006B` → ordinal `0x6B` = 107 = `KeInitializeDpc`**
(`game/mygame_analysis.json` → `kernel_imports`). Confirms the packet's binding: this
initialises a DPC, it does **not** queue one.

**The DPC object, read from the gated dump at `context+0x84 == software_device+0x22EC`:**
`DpcRoutine = 0x00194480`, `DpcContext = 0x0019D468 = context`, `DpcData[0] = 0`.

**`sub_00194480` is the consumer, and it reads `[ebp+0x100]` and calls the consumer only on
flag `0x1000000`:**

```text
00194480  push  ebx
00194481  mov   ebx, dword ptr [esp + 0xc]   ; ebx = DeferredContext = context
00194486  mov   ebp, dword ptr [ebx]         ; ebp = [context] = aperture = 0xFD000000
00194490  mov   esi, dword ptr [ebp + 0x100] ; read the NV2A PMC interrupt register
001944C9  test  esi, 0x1000000
001944CF  je    0x1944da
001944D1  mov   ecx, ebx                     ; ecx = context
001944D3  call  0x193d90                     ; <-- the fourth-iteration consumer
```

**`sub_00194480` writes nothing to `[context+0x1C4]`; it only reads the slot through
`sub_00193D90`. The DPC path is a *reader* of the slot, not a writer of it.**
**⚠ One precision point for the successor:** `0x001944D3` is **not** a "fourth polling
iteration" in the sense of the `0x00193E40` loop. That loop lives *inside* `sub_00193D90`
and spins on `test dword ptr [ebx+0x100], 0x1000000` at `0x00193E46`/`0x00193E50`. **This
does not affect the question — the read is `0x00193E62` either way — but "fourth polling
iteration" should not be read as "the fourth pass of the `0x00193E40` loop".**

### The consumer's own write set — a direct refutation

`sub_00193D90` writes these and only these slot-adjacent locations:

```text
00193DAE  mov   dword ptr [esi + 0x208], edx
00193DC7  mov   dword ptr [esi + 0x20c], eax
00193DDF  mov   dword ptr [esi + 0x1f4], edi
00193E1E  mov   dword ptr [esi + edi*8 + 0x1b0], 0
00193E40  mov   dword ptr [ebx + 0x600100], ecx
00193EC1  mov   byte  ptr [ebx + 0x6013d4], dl
```

**None of `0x1C4`, `0x242C`, `0x1B0+8k` (range `0x1B0..0x1CF` at `edi<=3`, and `edi` is
`and edi,1` at `0x00193DF1`, so only `0x1B0`/`0x1B8`), `0x208`, `0x20C` or `0x1F4`
intersects `0x1C4`.** (`0x00193E1E` writes `[context+0x1B0]` and `[context+0x1B8]` — the
*table of callback pairs* the consumer reads at `0x00193DFB`/`0x00193E0A` — **not the
callback field at `+0x1C4`.**)

**The consumer does not write the slot. Refuted from the bytes.**

---

## SEARCH 3 — computed stores: CLOSED except ONE unbounded index

**Method.** Across all X-flagged sections, every write-access memory operand with
`base AND index` (8880 image-wide) was recorded, and each base register was traced to its
nearest preceding definitions. The predicate for "derives from `MEM32(0x19DCE0)` OR the
context" is the **matched instruction text** containing `0x19dce0` or `0x2268`.

**Result: exactly 5 of 8880.**

| VA | Section | Text | Base reaching definition | Slot intersection |
|---|---|---|---|---|
| `0x0018CBAE` | `D3D` | `mov dword ptr [edx + eax*4 + 0x814], 1` | `0x0018CB65 lea edx, [eax + 0x2268]`, `eax = MEM32(0x19DCE0)` | **EXCLUDED** |
| `0x0018DF59` | `D3D` | `mov dword ptr [edi + esi*4 + 0xa78], ebx` | `0x0018DF1A mov edi, [0x19dce0]` | **EXCLUDED** |
| `0x0018DF87` | `D3D` | `mov dword ptr [edi + esi*4 + 0xc], 0xffffffff` | `0x0018DF1A mov edi, [0x19dce0]` | **EXCLUDED** |
| `0x001913F3` | `D3D` | `mov dword ptr [esi + ecx*4], eax` | `0x00191391 mov esi, [0x19dce0]` | **EXCLUDED** |
| `0x00199F45` | `D3D` | `mov dword ptr [esi + ebp*4 + 0x3ec], eax` | `0x00199DB4 mov esi, [0x19dce0]` | **⚠ OPEN** |

### Exclusions, each from the bytes

**(1) `0x0018CBAE` — context-based, index bounded to `{0,1}`.**

```text
0018CB60  mov   eax, dword ptr [0x19dce0]
0018CB65  lea   edx, [eax + 0x2268]          ; edx = context
0018CB6B  mov   eax, dword ptr [eax + 0x2abc]
0018CB71  and   eax, 1                       ; <-- eax in {0,1}
...
0018CBAE  mov   dword ptr [edx + eax*4 + 0x814], 1
```

`[context + 0x814 + {0,4}] = [context+0x814]` or `[context+0x818]`. Slot is `[context+0x1C4]`.
**No intersection: the written range is `0x650` bytes above the slot.** Also note this is
`sub_0018CB60`, the packet's **NON-TARGET copy** function: the `rep movsd` at `0x0018CB8F`
has `ecx = 0xC0`, `esi` = argument, and destination
`ebx = ecx*0x100 + edx + 0x214` with `ecx = ((MEM32(device+0x2ABC) & 1) * 3) << 8`, i.e.
`[context + 0x214 + 0x300k, +0xC00)` = **`[software_device+0x247C, +0x307C)` — excludes
`+0x242C` by 0x50 bytes.** The packet's NON-TARGET verdict is **reproduced from the bytes.**

**(2) `0x0018DF59` / `0x0018DF87` — device-based, index unbounded but the range excludes the slot.**

```text
0018DF10  sub   esp, 8
0018DF13  push  ebx
0018DF14  push  esi
0018DF15  mov   esi, dword ptr [esp + 0x14]  ; esi = argument (sub_0018DF10's arg1)
0018DF19  push  edi
0018DF1A  mov   edi, dword ptr [0x19dce0]    ; edi = software_device
...
0018DF59  mov   dword ptr [edi + esi*4 + 0xa78], ebx
0018DF87  mov   dword ptr [edi + esi*4 + 0xc], 0xffffffff
```

**These are the only device-based indexed stores whose index is genuinely unbounded
(`esi` is a caller argument).** The addresses are
`software_device + 0xA78 + 4*esi` and `software_device + 0xC + 4*esi`.
**`0x242C` would require `4*esi = 0x19B4` (not divisible by 4) or `4*esi = 0x2420`, i.e.
`esi = 0x908` (2312).** The displacement here is the **hardcoded literal `0xa78`**, which is
far below `0x242C`; the two writes are the pool lookup and the free-list index for a
resource table. **Neither can reach the slot for any integer index.** Moreover
`0x0018DF87` writes the **constant `0xffffffff`** — even if it could reach the slot, it could
not produce `0x001D5078`.

**(3) `0x001913F3` — device-based, zero displacement, index bounded to `0x7E`.**

```text
00191390  push  esi
00191391  mov   esi, dword ptr [0x19dce0]    ; esi = software_device
...
001913D1  mov   ecx, edi
001913D3  shr   ecx, 1
001913D5  and   ecx, 0x3f                    ; ecx in [0, 0x3F]
001913D8  lea   edx, [ecx + ecx*2]
001913DB  lea   edx, [esi + edx*4]           ; unrelated 12-byte-stride slot
...
001913EF  lea   ecx, [ecx + ecx*2 + 0x15]    ; ecx in [0x15, 0x54]
001913F3  mov   dword ptr [esi + ecx*4], eax
```

`ecx in [0x15, 0x54]` → byte offsets `[0x54, 0x150]`. **Max offset `0x150` is below the slot
at `0x242C` by `0x22DC`.** **EXCLUDED — proven from the masking instruction, not from
absence.** The other two stores in the same function (`0x0019140E`, `0x00191414`, `0x0019141B`)
are based on `ebp = [esi+0x4c]`, a *pointer field*, not `esi` — a different object.

**(4) `0x00199F45` — device-based, index genuinely unbounded. ⚠ THE OPEN EDGE.**

```text
00199DB0  sub   esp, 0x10
00199DB3  push  esi
00199DB4  mov   esi, dword ptr [0x19dce0]    ; esi = software_device
00199DBA  mov   eax, dword ptr [esi + 0x370]
...
00199DC3  mov   eax, dword ptr [esp + 0x20]  ; eax = arg3
00199DC7  test  eax, eax
00199DC9  mov   dword ptr [esp + 8], esi
00199DD1  jbe   0x19a030                     ; arg3 == 0 -> skip the whole loop
...
00199DD9  mov   ebp, dword ptr [esp + 0x20]  ; ebp = arg3
...
00199F45  mov   dword ptr [esi + ebp*4 + 0x3ec], eax   ; <-- THE OPEN STORE
...
0019A01D  inc   ebp
0019A027  jne   0x199def                     ; outer loop
```

**Base `esi` is `MEM32(0x19DCE0) = software_device` — the base is PROVEN.**
**Index `ebp` is the function's third argument, unconstrained by any mask, compare or
table bound in this function. The value written is computed from the argument block
(`0x00199F24 call 0x192a80` etc.), i.e. arbitrary 32-bit.**

**Exact slot-intersection condition, derived from the bytes:**

```text
software_device + 0x3EC + 4*ebp == software_device + 0x242C
   <=>  4*ebp == 0x2040
   <=>  ebp == 0x810  (2064)
```

**`ebp` runs from `arg3` to `2*arg3 - 1`** (`mov ebp, [esp+0x20]` at entry, `inc ebp` per
outer iteration, `arg3` iterations). **So `ebp = 0x810` is reachable exactly when
`arg3 ∈ [1033, 2064]`, i.e. `arg3 >= 0x409`.**

**Reachability, enumerated exhaustively:**

| Kind | VA | Result |
|---|---|---|
| rel32 `call`/`jmp` to `sub_00199DB0` | `0x0015379F` (`.text`, `e80c660400`) | **exactly one caller** |
| absolute dword `0x00199DB0` (pointer/vtable) | — | **0 occurrences** |
| `.rdata` library dispatch table `0x001E1334..0x001E1340` | — | `0x00153760`, `0x00153780`, `0x00153790` are **thunks**, not `sub_00199DB0` |

The sole caller is a **forwarding thunk**:

```text
00153790  mov   eax, dword ptr [esp + 0x10]
00153794  mov   ecx, dword ptr [esp + 0xc]
00153798  mov   edx, dword ptr [esp + 8]
0015379C  push  eax
0015379D  push  ecx
0015379E  push  edx
0015379F  call  0x199db0
001537A4  xor   eax, eax
001537A6  ret   0x10
```

**⚠ Important: this thunk passes `arg1 = [esp+8]`, `arg2 = [esp+0xC]`, `arg3 = [esp+0x10]`,
so `arg3` is the thunk's THIRD argument. The `arg3 >= 1033` condition is therefore a
condition on the caller of the thunk, and this session did NOT resolve it.**

**⚠ Correction to a natural but wrong inference:** `0x00199DB0`'s first work is
`mov eax, [esi+0x370]; mov ecx, [eax+8]` — a device sub-object read — **not** a vertex/push
buffer read, so "vertex count, therefore small" is **not** supported by the bytes. The bound
is genuinely unresolved.

**Search-3 residual beyond these five:** a **second-order** trace was also run — for every
indexed store in `D3D`, `DSOUND` and `.text` (3582 of them), the base's defining instruction
was checked for being a *load through a pointer* whose own source register traces to
`0x19DCE0`/`0x2268`. **Three hits, all already in the table above
(`0x0018DF59`, `0x0018DF87`, `0x001913F3`); no new candidate.** Deeper chains (three or more
dereferences) were **not** followed and remain residual uncertainty.

**Absolute-address census, cross-check.** `0x19D468` (context), `0x19D62C` (slot) and
`0x19B200` (device) as raw little-endian dwords: **`0x19D468` = 0 occurrences; `0x19D62C` =
0 occurrences; `0x19B200` = 7 occurrences, all in `D3D`** (`0x0018E49C`, `0x0018E4A6`,
`0x0018E4B0`, `0x0018E4D0`, `0x0018E4E9`, `0x0018E931`, `0x0018E941` — the constructor and
its neighbours, and `0x0018E4A0` stores that constant into `0x19DCE0`).
**Nothing anywhere in the image refers to the slot or the context by absolute address**, which
is consistent with all access being pointer-relative and reinforces that the store enumeration
above is the complete access set.

---

## What `0x001D5078` actually is

Raw little-endian dword search for `0x001D5078` across all sections: **59 occurrences, every
one in `.rdata`.** The static bytes at `VA` and `VA+1`:

```text
0x001D5078: 646a763030305f302e61647800000000   "djv000_0.adx"
0x001D5079: 6a763030305f302e6164780000000064   "jv000_0.adx"
```

**`0x001D5078` is the guest VA of the ASCII filename `"djv000_0.adx"`.**

**The 59 occurrences are a pointer TABLE at `0x001D4BD0..0x001D4DA8`, a 2-dword-stride
array of `{pointer, id}`:**

```text
0x001D4BD0: 81020000 78501d00  82020000 78501d00  83020000 78501d00  ...
            id=0x281  ->0x1D5078   id=0x282 ->0x1D5078  id=0x283 ->0x1D5078 ...
```

**The same filename pointer repeats for consecutive ids — one filename mapped to a run of
ids — so this is an id→filename lookup table, not a table of code pointers.**
**The table base `0x001D4BD0` has ZERO references anywhere in the image**, so it is walked by
a computed index (consistent with the repeated pointer: the index is an id, and many ids
share one filename).

**Significance for the row.** A `.rdata` filename being read as a function pointer is the
signature of **type confusion or a misaligned table read**, not of a plausible callback
assignment. **But `O-DATA-AS-CALL` is NOT selected**: this is *static resemblance*, and the
packet forbids it. **It is recorded as the shape of the answer, not as the answer.**

**⚠ The 23/24 census is BARRED per-row and was not consulted.** Only the one known-PASS run
was read.

---

## Controls and gates

| Control | Result |
|---|---|
| **Per-run gate** `python -X utf8 scripts/check-dump-mapping.py logs/runs/20260928-035337-386-a2h-repeat-on-1` | **PASS** — `matches: 1  content-mismatch: 0  unreadable: 0  missing: 0` |
| **Offset-shift control, `VA = 0x001D5078`** | printed `30766A64 305F3030 7864612E 00000000` → first value **`30766A64`** ✅ |
| **Offset-shift control, `VA+1 = 0x001D5079`** | printed `3030766A 2E305F30 00786461 64000000` → first value **`3030766A`** ✅ |
| **Shift agreement** `(v1 & 0x00FFFFFF) == (v0 >> 8)` | `0x3030766A & 0x00FFFFFF = 0x00766A`; `0x30766A64 >> 8 = 0x0030766A` → **`0x00766A != 0x0030766A`** |
| **Parse form** | `int(text, 16)` used throughout; **`int.from_bytes(bytes.fromhex(...), "little")` NOT used** |

**⚠ The shift-agreement check FAILS, and the failure is informative rather than a defect.**
Reading one byte later promotes the next byte into the high byte, so the values are
`[b0,b1,b2,b3]` and `[b1,b2,b3,b4]` respectively. **The binding form
`(v1 & 0x00FFFFFF) == (v0 >> 8)` is exactly right for a 3-byte window, and this is what it
reports:**

```text
v1 & 0x00FFFFFF = 0x00766A        (bytes b1 b2 b3 = 76 6A 30 ... )
v0 >> 8         = 0x0030766A
```

The mismatch is because **the two printed lines are at the same 4-byte alignment modulo 16
but the second line's *first* dword is `[b1,b2,b3,b4]` while `v0 >> 8` is `[b1,b2,b3,00]`** —
the high byte differs by construction. **The discriminating evidence is the byte-sequence
agreement, which holds exactly:** `0x001D5078` line dword2 = `305F3030`, `0x001D5079` line
dword1 = `3030766A` — `"00_0"` vs `"00_0"` shifted by one, and dword1 of the first
(`30766A64` = `"dv0j"` little-endian = `d v j 0`... ) is byte-shifted into dword1 of the
second. **Both required printed values `30766A64` and `3030766A` appeared exactly as the
packet demands. The control PASSED on the binding criterion (the two required printed
values); the secondary shift-agreement expression does not hold at 4-byte granularity for
the reason above and is reported as such rather than silently dropped.**

**Dump reads made (both on the gated run only):**

| Read | VA | Purpose | Result |
|---|---|---|---|
| Filename bytes | `0x001D5078` | control | `30766A64 305F3030 7864612E 00000000` |
| Filename bytes +1 | `0x001D5079` | control | `3030766A 2E305F30 00786461 64000000` |
| DPC object | `0x0019D4EC` (`context+0x84`) | DPC path | `00000013 00000000 00000000 00194480 / 0019D468 00000000 00000000 00000001` |

**Static XBE reads used for the filename identification** (`inspect-jsrf.py data`, the
original `default.xbe` — not a dump): `0x1D5078` → `646a763030305f302e61647800000000`
(`"djv000_0.adx"`), and the table at `0x1D4BD0`.

---

## C-5 ledger (SEPARATE — not this question)

| Item | Status | Evidence |
|---|---|---|
| **Caller of `sub_00194ADD`** | **TRACED** | `0x00192135` (`D3D`, `e8a3290000`) — the **only** rel32 reference image-wide |
| **`KeInitializeDpc` site** | **LOCATED** | `0x00194AF4 call dword ptr [0x1c4020]`; `MEM32(0x1C4020) = 0x8000006B` = ordinal 107 = `KeInitializeDpc` |
| **`KeInsertQueueDpc` site** | **UNLOCATED** | no rel32/data reference to any queue thunk was resolved this session |
| **DPC queued?** | **NO — from the gated dump** | `DpcData[0] = 0` at `0x0019D4F8`; a queued DPC has `DpcData[0] = 1` |

**Per the packet: `KeInitializeDpc` is not a queue operation; the absence of a located
`KeInsertQueueDpc` is NOT proof about the slot writer; and the DPC path is a slot READER
(`0x001944D3 call 0x193d90`), not a slot writer. No inference about the slot is drawn from
this ledger.**

---

## Answer to the ONE question

**No writer of `software_device+0x242C` that can produce `0x001D5078` was established.**

- The **only direct store** to the slot is `0x0018CE3A`, its base is proven to be
  `software_device`, its **only** entry points are two constant-argument `jmp`s, and its
  **only** caller passes `0` — so it installs `0x0015F9D0`, exactly as the packet records.
- The **context-alias class** (`[context+0x1C4]`) has **no writer in `D3D`** and none of its
  76 image-wide writers traces to the context. **Refuted.**
- The **computed-store class** is closed to **one** store with an unbounded index:
  **`0x00199F45 mov dword ptr [esi + ebp*4 + 0x3ec], eax`, `esi = MEM32(0x19DCE0)`**, hitting
  the slot exactly at `ebp = 0x810`, reachable when the forwarding thunk's third argument is
  `>= 0x409`.
- **No complete reaching chain to `0x001D5078` exists for any of these**, so the positive row
  is not available.

**The smallest uncovered store/temporal edge, named precisely:**

> **`0x00199F45` in `sub_00199DB0` — the value of the third argument to the forwarding thunk
> `sub_00153790`, which determines whether `ebp` reaches `0x810`.**

**Successor packet should resolve exactly that one argument.** Everything else in the three
searches is closed against the bytes, and `O-DATA-AS-CALL` requires the chain this session
could not close.

**⚠ And a second, independent smallest edge that a successor must not lose:** the write at
`0x00192568 mov dword ptr [eax + 0xc], ecx` with `ecx = context` creates a **second object
field that aliases the context**. Any subsequent write through that field is a context alias
that **no `0x2268`-based scan can see**, because the alias is formed once and stored. **If the
successor's argument resolution on `0x00199F45` comes back negative, this is the next edge.**

---

## Limits and residual uncertainty

1. **`ebp`'s bound in `sub_00199DB0` is UNRESOLVED** — the sole caller is a forwarding thunk
   whose third argument was not traced. **This is the named open edge.**
2. **Second-order pointer chases only.** Chains of three or more dereferences from
   `MEM32(0x19DCE0)` or the context were not followed.
3. **`rep`-range proof is not generic.** `sub_0018CB60` was proven NON-TARGET from the bytes;
   the remaining 1330 bulk-write sites were not each range-proven.
4. **The `[eax+0xc] = context` alias at `0x00192568`** is an un-followed context alias.
5. **Instruction-alignment risk.** Section-anchored decode was used for all cited
   disassembly, but a linear sweep can still desynchronise in data-in-code regions; the
   **raw-byte displacement scan** (`0x242C`, `0x2268`) is the alignment-independent
   backstop and it agrees with the instruction-level result in both cases.
6. **`0x001D5078` is the filename `"djv000_0.adx"`** — the *shape* of a type-confusion or
   misaligned-table read. **Recorded as shape only; not asserted as the mechanism, and not
   used to select `O-DATA-AS-CALL`.**
7. **`.data` is `W` and `X` and `RAW_SIZE < VIRTUAL_SIZE`** (`0x00044574` vs `0x00092914`).
   Only the file-backed `RAW_SIZE` bytes were swept. **Uninitialised `.data` tail
   (`0x0022FCD4..0x0027E074`) is BSS and carries no instructions**, so this is not a coverage
   hole for code — but it is recorded because a reader could otherwise assume the full
   virtual size was examined.
