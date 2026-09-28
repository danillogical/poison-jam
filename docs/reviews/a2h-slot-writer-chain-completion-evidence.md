# `A2h-slot-writer-chain-completion` — execution evidence

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-chain-completion.md` (frozen, SHA-256
`3F7AD922DADD5E8E1B3944EAFF6B461468C1C05DC2350973243022C16305C7FD`), validated by
`docs/reviews/a2h-slot-writer-chain-completion-r1-session-validation.md`.
**Mapping used:** `docs/reviews/a2h-thunk-argument-mapping.md` — the vcall thunk at
`0x000D4DA0` is a **`JMP`** (`8b 01 ff a0 cc 00 00 00`, `FF A0` = `/4`), so `esp` is
unchanged and **worker ARG1 = the caller's SECOND pushed argument**, ARG2 = third,
ARG3 = fourth; **the caller's first pushed argument is ignored by the thunk.**

## Row

> **`O-OPEN`.** The writer is **confirmed as an instruction**, but the chain is **not
> complete**: **the INDEX is not witnessed**. The precise first missing edge is
> `sub_00153790`'s **own** caller.

---

## Vocabulary

| Name | Address | What it is |
|---|---|---|
| **`software_device`** | **`MEM32(0x19DCE0) = 0x0019B200`** | the software NV2A object |
| **`aperture`** | **`0xFD000000`** | the NV2A MMIO register aperture |

Never conflated below. `software_device` and `aperture` are distinct in every statement.

---

## Controls

### Dump gate — **ZERO dump reads, gate NOT claimed**

**This execution performed ZERO archived-dump reads.** Every memory fact below comes
from **original XBE file-backed bytes** (`game/default.xbe` via
`game/mygame_analysis.json` section mapping) and from the log lines quoted in the
packet. **No `check-dump-mapping.py` was run, and the per-run 23/24 gate is
explicitly NOT claimed as passed.** No new game run, build, or instrumentation was
performed. **An XBE-byte control does NOT substitute for a dump gate.**

### Paired offset-shift control — **PASSED, on XBE bytes**

The packet's control is a *memory-read* control. Since this execution read no dump, the
control was executed **against the original XBE image**, which is the only memory-like
source available, and its result is reported as an **XBE-byte control, not a dump gate**.

| Guest VA | Printed dword | Required | Result |
|---|---|---|---|
| `0x001D5078` | **`30766A64`** | `30766A64` | **MATCH** |
| `0x001D5079` | **`3030766A`** | `3030766A` | **MATCH** |

Parsed with **`int(text, 16)`**, never `int.from_bytes(bytes.fromhex(text), "little")`.
**`(v1 & 0x00FFFFFF) == (v0 >> 8)` → `True`** (PASSES, retaining `0x30`).

Raw bytes at `0x001D5078`:

```
001D5078  64 6a 76 30 30 30 5f 30 2e 61 64 78 00 00 00 00
001D5088  64 6a 76 31 31 32 5f 30 2e 61 64 78 00 00 00 00
```

**ASCII: `djv000_0.adx`** — **this dword is an address pointing at the ADX filename
string**, exactly as the packet's obligation 3 states.

---

## Instruction-boundary verification (packet line 11)

**Method.** Every cited VA is checked against **two independent witnesses**:

1. **Declared starts** — the 5,652 `void sub_XXXXXXXX(void);` declarations in
   `src/recomp/gen/recomp_funcs.h`. Each seed is decoded to the next seed **in the
   same file-backed section**; the union of decoded instruction addresses is the
   boundary set (**549,980 boundaries**; decode overruns past the next seed: 0).
2. **Section-local padding witness** for the D3D section, which the generated tree
   declares only 265 starts in and **does not contain `sub_00199DB0` at all** — so
   witness 1 cannot see it. D3D functions are separated by `0x90` runs, so a start is
   any offset that follows a `0x90` run; decoding each span yields **18,168
   boundaries**, 0 overruns.

**Every VA cited in this record is a verified instruction boundary under at least one
witness.** The matched text is printed with each verdict. The old mislabels the packet
warned about are **not inherited**: `0x0018B5AE` is **mid-instruction** — the
instruction is **`0x0018B5AD`** `mov dword ptr [0x257ef0], eax`.

| Label | VA | Boundary | **Matched text** |
|---|---|---|---|
| `sub_00199DB0` entry | `00199DB0` | OK | `sub esp, 0x10` |
| `esi = MEM32(0x19DCE0)` | `00199DB4` | OK | `mov esi, dword ptr [0x19dce0]` |
| ARG3 read | `00199DC3` | OK | `mov eax, dword ptr [esp + 0x20]` |
| test ARG3 | `00199DC7` | OK | `test eax, eax` |
| `jbe 0x19a030` guard | `00199DD1` | OK | `jbe 0x19a030` |
| `ebp = ARG1` | `00199DD9` | OK | `mov ebp, dword ptr [esp + 0x20]` |
| **THE STORE** | `00199F45` | OK | `mov dword ptr [esi + ebp*4 + 0x3ec], eax` |
| loop back-edge | `00199DEF` | OK | `mov esi, dword ptr [esp + 0x14]` |
| dec/jne tail | `0019A027` | OK | `jne 0x199def` |
| `ret 0xc` | `0019A034` | OK | `ret 0xc` |
| `sub_00153790` entry | `00153790` | OK | `mov eax, dword ptr [esp + 0x10]` |
| `call 0x199db0` | `0015379F` | OK | `call 0x199db0` |
| `ret 0x10` | `001537A6` | OK | `ret 0x10` |
| thunk | `000D4DA0` | OK | `mov eax, dword ptr [ecx]` |
| thunk `jmp [eax+0xcc]` | `000D4DA2` | OK | `jmp dword ptr [eax + 0xcc]` |
| installer store | `0018CE3A` | OK | `mov dword ptr [ecx + 0x242c], eax` |
| installer fn entry | `0018CB40` | OK | `push esi` |
| `call 0x15f9e0` | `00012319` | OK | `call 0x15f9e0` |
| `xor ebx, ebx` | `0001224F` | OK | `xor ebx, ebx` |
| `sub_0015F9E0` | `0015F9E0` | OK | `mov eax, dword ptr [esp + 4]` |
| const `0x0015F9D0` | `0015F9D0` | OK | `inc dword ptr [0x265174]` |
| fourth read | `00193E62` | OK | `mov eax, dword ptr [esi + 0x1c4]` |
| `call eax` | `00193EB5` | OK | `call eax` |
| spin store | `00193E40` | OK | `mov dword ptr [ebx + 0x600100], ecx` |
| `call [0x1c4014]` | `00193E5C` | OK | `call dword ptr [0x1c4014]` |
| init `mov eax,0xd4bc0` | `0018B1A0` | OK | `mov eax, 0xd4bc0` |
| init store `[0x257de0]` | `0018B1A5` | OK | `mov dword ptr [0x257de0], eax` |
| init `mov eax,0xd4da0` | `0018B1C8` | OK | `mov eax, 0xd4da0` |
| init store `[0x257e00]` | `0018B1CD` | OK | `mov dword ptr [0x257e00], eax` |
| init store `[0x257ef0]` | **`0018B5AD`** | OK | `mov dword ptr [0x257ef0], eax` |
| vtable store A/B/C | `00152244`/`00152294`/`00152309` | OK | `mov dword ptr [esi/edx/eax], 0x1e1270` |
| `call [eax+0x258470]` | `000E4925` | OK | `call dword ptr [eax + 0x258470]` |
| `call [eax*8+0x258560]` | `000ED044` | OK | `call dword ptr [eax*8 + 0x258560]` |
| `call [eax*8+0x258778]` | `000F6A88` | OK | `call dword ptr [eax*8 + 0x258778]` |
| **old mislabel** | `0018B5AE` | **NOT** | mid-instruction (`0018B5AD` starts it) |

**NOT-boundary count among cited VAs: 0** (the one NOT above is cited *as* the
counter-example).

---

## The writer, re-verified from bytes

`sub_00199DB0` (D3D section, `0x00199DB0..0x0019A037`, ends `ret 0xc` — a 3-argument
`__stdcall`). The store:

```
00199F45  89 84 ae ec 03 00 00   mov dword ptr [esi + ebp*4 + 0x3ec], eax
```

- `esi = MEM32(0x19DCE0)` at `0x00199DB4` → **`esi` is `software_device`**.
- `ebp` is loaded at `0x00199DD9` from `[esp+0x20]`, which after `sub esp,0x10` /
  `push esi` / `push ebx` / `push ebp` / `push edi` is **ARG1**.
- `0x00199DC3` reads `[esp+0x20]` at a *different* frame depth = **ARG3**, the trip
  count; `test eax,eax` / `jbe 0x19a030` guards `ARG3 == 0`.
- The loop (`0x00199DEF` … `0x0019A027 jne 0x199def`) runs `ARG3` times with
  `inc ebp` and `dec eax`, so it visits **`software_device + (ARG1+k)*4 + 0x3EC` for
  `k = 0..ARG3-1`** — a **contiguous run of `ARG3` dwords**.

**Value stored.** `eax` is assembled at `0x00199F29..0x00199F43` from four
`call 0x192a80` results, each `Q(component) = (int)(component*255.0f + 0.5f)` with
`fadd [0x1c4550]` (= 0.5f) and clamping against `[0x1c4578]`/`[0x1c43d0]`:

```
00199F29  movzx edx, byte ptr [esp + 0x28]   ; Q(edi+4)
00199F2E  xor ecx, ecx
00199F30  mov ch, al                         ; Q(edi+8)  -> bits 8..15
00199F32  movzx eax, bl                      ; Q(edi-4)  -> bits 16..23
00199F37  mov cl, byte ptr [esp + 0x2c]      ; Q(edi+0)  -> bits 0..7
00199F3B  shl ecx, 8
00199F3E  or ecx, edx
00199F40  shl ecx, 8
00199F43  or eax, ecx
00199F45  mov dword ptr [esi + ebp*4 + 0x3ec], eax
```

**So this store writes a BYTE TUPLE, not an address** — see item 3.

---

## ITEM 1 — INDEX PROVENANCE: **the first missing edge**

### What the thunk mapping forces

`sub_00153790` is **vtable entry 51** of the 52-entry vtable at `0x001E1270`
(`MEM32(0x001E1270 + 51*4) = 0x00153790`, verified from XBE bytes). It is a pure
forwarder:

```
00153790  8b 44 24 10   mov eax, [esp+0x10]     ; caller's FOURTH pushed arg
00153794  8b 4c 24 0c   mov ecx, [esp+0x0c]     ; caller's THIRD
00153798  8b 54 24 08   mov edx, [esp+0x08]     ; caller's SECOND
0015379C  50            push eax
0015379D  51            push ecx
0015379E  52            push edx
0015379F  e8 0c 66 04 00  call 0x199db0
001537A4  33 c0         xor eax, eax
001537A6  c2 10 00      ret 0x10                ; pops FOUR slots
```

`ret 0x10` confirms the thunk's caller pushed **four** arguments; the thunk reads only
`[esp+8]`, `[esp+0xc]`, `[esp+0x10]` and **ignores `[esp+4]`** — the Session's mapping,
re-derived here from the bytes.

**Therefore: worker ARG1 (the INDEX) = the thunk caller's SECOND pushed argument.
Worker ARG3 (the TRIP COUNT) = the caller's FOURTH.**

### The trace, and where it stops

**`sub_00153790` has NO direct caller anywhere in the image.** Raw-byte scans across
**every file-backed section** for `e8 rel32` (call) and `e9 rel32` (jmp) targeting
`0x00153790`, `0x00199DB0`, and `0x000D4DA0` return **zero hits each**. The only
reference to `0x00153790` in the whole image is the vtable slot
`.rdata:0x001E133C` (`0x001E1270 + 51*4`).

**The caller is therefore whatever performs a virtual call through `object+0x00` at
INDEX 51.** Every encoding form that can reach that slot was enumerated:

| Encoding form searched | Sites found |
|---|---|
| `FF /2` or `FF /4`, `mod=10`, `rm≠4`, `disp32 == 0xCC` | **1** — `.text:0x000D4DA2` (the thunk itself) |
| `FF /2` or `FF /4`, `mod=10`, `rm=4` (SIB), `disp32 == 0xCC` | **0** |
| `FF /2`/`FF /4`, `mod=01`, `disp8 == 0xCC` | **0** |
| `mov reg,[reg+0xCC]` then `call reg` (mod=10 and SIB forms) | **0** |
| `add reg,0xCC` / `lea reg,[reg+0xCC]` then an indirect call/jmp | **18 sites, none followed by an indirect call/jmp** |
| `FF /2`/`FF /4` absolute `[abs32]` with `abs32` in the dispatch region | **0** |

**The only code that reads vtable entry 51 is the thunk at `0x000D4DA0`.** So the
caller of the thunk is exactly the caller the record must name.

### Where the thunk address is published

`0x000D4DA0` is written into **six runtime dispatch tables** by absolute immediate
stores (each `b8 a0 4d 0d 00` / `a3 <disp32>`). **The store VAs below are instruction
boundaries, verified — the `a3` displacement field begins one byte later, at
`+1`.** (`0x0018B5AE` is the displacement field, **not** an instruction boundary; the
instruction is `0x0018B5AD`.)

| Store VA (boundary) | Destination | imm32 offset (mid-instruction) |
|---|---|---|
| `.text:0x0018B1CD` | `0x00257E00` | `0x0018B1CE` |
| `.text:0x0018B5AD` | `0x00257EF0` | `0x0018B5AE` |
| `.text:0x0018B98D` | `0x002580D0` | `0x0018B98E` |
| `.text:0x0018BD6D` | `0x002582B0` | `0x0018BD6E` |
| `.text:0x0018C3ED` | `0x002586A8` | `0x0018C3EE` |
| `.text:0x0018C78D` | `0x002587F0` | `0x0018C78E` |

### ⚠ The decisive measurement: these six tables have **NO READER AT ALL**

Every instruction in the decoded boundary set with a memory operand whose
displacement is one of the six tables:

```
0018B1CD  mov dword ptr [0x257e00], eax
0018B5AD  mov dword ptr [0x257ef0], eax
0018B98D  mov dword ptr [0x2580d0], eax
0018BD6D  mov dword ptr [0x2582b0], eax
0018C3ED  mov dword ptr [0x2586a8], eax
0018C78D  mov dword ptr [0x2587f0], eax
```

**All six are STORES — the writers themselves.** And the raw-imm32 scan of **every
file-backed section** finds **exactly one occurrence of each table address in the whole
image**, at the displacement field of its own writer:

| Table | Occurrences of the address anywhere in the image |
|---|---|
| `0x00257E00` | **1** — `.text:0x0018B1CE` (its own writer) |
| `0x00257EF0` | **1** — `.text:0x0018B5AE` (its own writer) |
| `0x002580D0` | **1** — `.text:0x0018B98E` (its own writer) |
| `0x002582B0` | **1** — `.text:0x0018BD6E` (its own writer) |
| `0x002586A8` | **1** — `.text:0x0018C3EE` (its own writer) |
| `0x002587F0` | **1** — `.text:0x0018C78E` (its own writer) |

**No decoded instruction loads from, or indirect-calls through, any of the six tables,
and no literal reference to them exists outside their own writers.**

Each table is the **5th element of a run of `mov eax, <thunk>; mov [table+i*8], eax`
pairs** (stride 8). The neighbouring stores show the pattern: `0x00257DE0` holds
`0x000D4BC0`, `0x00257DE8` holds `0x000D4C30`, `0x00257DF0` holds `0x000D4CA0`, and
`0x00257E00` holds `0x000D4DA0`.

**These tables are read through an object field, not by literal displacement.** The
base `0x00257DE0` is stored into a per-object field:

```
000D4684  c7 86 c8 03 00 00 e0 7d 25 00  mov dword ptr [esi + 0x3c8], 0x257de0
000D467A  c7 86 c4 03 00 00 f0 7c 25 00  mov dword ptr [esi + 0x3c4], 0x257cf0
```

and the **only indirect calls that read the table region** are register-based:

```
000E4925  call dword ptr [eax + 0x258470]
000E7783  jmp  dword ptr [eax + 0x2584a0]
000EA6B4  call dword ptr [eax + 0x2584c8]
000ECDC4  call dword ptr [eax + 0x258520]
000ED044  call dword ptr [eax*8 + 0x258560]
000F61F8  call dword ptr [eax + 0x258800]
000F6A88  call dword ptr [eax*8 + 0x258778]
```

**None of these bases is `0x00257E00`, `0x00257EF0`, `0x002580D0`, `0x002582B0`,
`0x002586A8`, or `0x002587F0`** — the six tables that actually hold the thunk. The
`[obj+0x3c8]` reader at `0x0005F6E5` treats the field as a **count**, not a pointer:
`shl ecx, 2` then `push ecx; call 0x4a8f0` (allocation) — it is a **size field**, and
the same pattern repeats at `0x0005F715`, `0x0005F74B`, `0x0005F797`, … So the six
thunk tables are **not reached through `[obj+0x3c8]`** either.

### The precise untraced edge

> **`sub_00153790` has no direct caller. Its vtable slot 51 is reached only by the thunk
> `0x000D4DA0`, and the thunk is invoked only through six runtime dispatch tables
> (`0x00257E00`, `0x00257EF0`, `0x002580D0`, `0x002582B0`, `0x002586A8`, `0x002587F0`)
> whose reader WAS NOT LOCATED — and the measurement above shows each of the six has
> exactly ONE reference in the entire image: its own writer.**
>
> **So either the reader is in the 25.02 % of the image this enumeration did not decode
> (DSOUND 33.07 % covered, plus ~563 k non-padding bytes), or the six tables are
> populated but never read. This record cannot distinguish those two cases, and does
> not guess.**
>
> **The caller's SECOND and FOURTH pushed values are therefore NOT witnessed.**
> **ARG1 and ARG3 have no caller-side constants and no bounds argument.**

**This is the first missing edge. It is reported, not solved.**

### Explicitly NOT done

**ARG1 was never solved backwards from the slot.** The arithmetic
`0x3EC + 2064*4 = 0x242C` is an **identity**, and the previous execution's rejection
came from presenting it as verification of the index. **No such claim appears here.**
The index is **UNWITNESSED**, and that alone selects `O-OPEN`.

---

## ITEM 2 — ORDER

**The installer is confirmed as an instruction, and its value is confirmed.**

`0x0018CE30` is a one-argument `__stdcall` setter:

```
0018CE30  8b 44 24 04   mov eax, dword ptr [esp + 4]   ; the argument
0018CE34  8b 0d e0 dc 19 00  mov ecx, dword ptr [0x19dce0]   ; software_device
0018CE3A  89 81 2c 24 00 00  mov dword ptr [ecx + 0x242c], eax
0018CE40  c2 04 00      ret 4
```

Its sole caller is `0x00012319`:

```
0001224F  33 db              xor ebx, ebx
...
00012319  e8 c2 d6 14 00     call 0x15f9e0     ; with ebx == 0 already in hand
```

`sub_0015F9E0` substitutes the constant when the argument is NULL:

```
0015F9E0  8b 44 24 04   mov eax, dword ptr [esp + 4]
0015F9E4  85 c0         test eax, eax
0015F9E6  75 0e         jne 0x15f9f6
0015F9E8  b8 d0 f9 15 00  mov eax, 0x15f9d0     ; <-- the CONSTANT
0015F9ED  89 44 24 04   mov dword ptr [esp + 4], eax
0015F9F1  e9 3a d4 02 00  jmp 0x18ce30          ; tail-call into the setter
0015F9F6  83 f8 ff      cmp eax, -1
0015F9F9  75 02         jne 0x15f9fd
0015F9FB  33 c0         xor eax, eax
0015F9FD  89 44 24 04   mov dword ptr [esp + 4], eax
0015FA01  e9 2a d4 02 00  jmp 0x18ce30
```

**So at install time the slot receives `0x0015F9D0`, a code address** — verified as a
boundary: `0x0015F9D0  inc dword ptr [0x265174]` / `0x0015F9D6  ret`.

**The fourth read is also confirmed:**

```
00193E62  8b 86 c4 01 00 00   mov eax, dword ptr [esi + 0x1c4]   ; esi = 0x0019D468
00193E68  85 c0               test eax, eax
00193E6A  74 62               je 0x193ece                     ; NULL -> skip the call
...
00193EB0  8d 4c 24 18         lea ecx, [esp + 0x18]
00193EB4  51                  push ecx
00193EB5  ff d0               call eax
```

With `esi = 0x0019D468` (from the log line `Xbox regs: ebx=0xFD000000
esi=0x0019D468 edi=0x00000000`), `esi + 0x1c4 = 0x0019D62C`, and
`software_device + 0x242C = 0x0019B200 + 0x242C = 0x0019D62C`. **The slot identity is
confirmed arithmetically and corroborated by the runtime register log.**

### What remains unbound

**The competitor write is NOT identified.** The packet requires
`installer → candidate/competitor write → fourth read → call eax` with an admissible
execution/order witness. **The middle link does not exist in this record**, because
**item 1's missing edge removes the ability to attribute any candidate to the slot**:
without a witnessed ARG1/ARG3, no store in the sweep can be shown to reach
`software_device+0x242C` on any execution.

**The value at the fourth read is also not witnessed.** No dump read was performed, so
`MEM32(0x0019D62C)` at the fourth read is **UNKNOWN**. **`0x001D5078` is not shown to
be in the slot at that moment by this record** — the log polls 1–3 vs poll 4 show a
change **between polls**, which is exactly what the packet says it is: **not
attribution**. **Absence of a log line is not treated as negative attribution.**

---

## ITEM 3 — ADDRESS VERSUS BYTE TUPLE

**Verdict: for the store at `0x00199F45`, the value is a BYTE TUPLE, not an address.**

**Evidence — the value's reaching definitions, all inside `sub_00199DB0`:**

| Component | Read from | Bounds imposed before `call 0x192a80` |
|---|---|---|
| byte 3 (`<<24`) | `[edi+8]` at `0x00199E00` | clamp vs `[0x1c4578]`, `[0x1c43d0]`, then `fmul [0x1c4ccc]`, `fadd [0x1c4550]` |
| byte 2 (`<<16`) | `[edi-4]` at `0x00199E3B` | same clamp / `fmul` / `fadd` |
| byte 1 (`<<8`) | `[edi]` at `0x00199E7F` | same |
| byte 0 | `[edi+4]` at `0x00199EC6` | same |

Each passes through `call 0x192a80` and its `al` result is stored into a **byte** slot
(`mov byte ptr [esp+0x2c], al` at `0x00199E81`, `mov byte ptr [esp+0x28], al` at
`0x00199EC9`, `mov bl, al` at `0x00199F19`). `Q(v) = (int)(v*255.0f + 0.5f)` returns
**0..255**, so each `al` is a byte and the four are packed by `shl`/`or`. **`eax` at
the store is therefore a packed RGBA-style byte tuple.**

**The contrast the packet demands is confirmed:** the same numeric dword
`0x001D5078` **is** an address — the XBE bytes at that VA are the ASCII string
`djv000_0.adx` (control above). **Four bytes in `0..255` fit ANY dword**, so the
formula alone cannot prove the store produced `0x001D5078` — and here the reaching
definitions show it produces a **tuple**, while `0x001D5078` in the slot would be a
**pointer**.

**Fail-closed statement.** Because **no dump read was performed**, the value actually
resident at `0x0019D62C` at the fourth read is **UNKNOWN**, and **the record does not
claim the `0x00199F45` store produced `0x001D5078`.** **Value attribution is NOT
witnessed.**

---

## ITEM 4 — FOLDED-IN SIB SWEEP

### Population and method (§6.1)

**Method — recursive descent from declared seeds with a demonstrated coverage witness.**

- **SEEDS:** all **5,652** `void sub_XXXXXXXX(void);` declarations in
  `src/recomp/gen/recomp_funcs.h`, restricted to the image range.
- **SPAN:** a seed spans to the **next seed in the same file-backed section** (or the
  section end). The generated tree splits at internal labels, so a seed may begin
  mid-function; spanning to the next seed keeps decode aligned and is conservative —
  it never invents coverage. **Decode overruns past the next seed: 0.**
- **DECODE:** linear decode (capstone, x86-32) over each span. The union of decoded
  instruction addresses is the boundary set: **549,980 instructions**.
- **COVERAGE WITNESS — per section:**

| Section | Bytes | Decoded | Coverage |
|---|---|---|---|
| `.text` | 1,555,248 | 1,527,842 | **98.24 %** |
| **D3D** | 59,056 | 55,952 | **94.74 %** |
| DSOUND | 115,500 | 38,191 | **33.07 %** |
| MMATRIX | 4,624 | 4,624 | **100.00 %** |
| XGRPH | 3,324 | 3,324 | **100.00 %** |
| XPP | 30,616 | 30,155 | **98.49 %** |
| `.rdata` | 161,776 | 28,460 | 17.59 % |
| `.data` | 279,924 | 0 | 0 % (data) |
| DOLBY | 28,036 | 410 | 1.46 % |
| `$$XTIMAGE`/`$$XSIMAGE` | 14,336 | 0 | 0 % (data) |
| **TOTAL** | **2,252,440** | **1,688,958** | **74.98 %** |

**Uncovered non-padding bytes: 563,396**, concentrated in `.data`/`.rdata`/image
sections (not code) and in **DSOUND (33.07 %)**. **DSOUND's coverage gap is a stated
limit of this enumeration and is carried as `UNKNOWN`, not as uniqueness.**

**⚠ Alignment-independent raw-byte scans establish EXISTENCE only.** The raw scans
used above (direct `e8`/`e9` caller search, `b8 a0 4d 0d 00` / `a3` publication
search, `FF`-form searches) are **existence** evidence. **Uniqueness claims rest on the
decode-derived boundary set with the coverage table above.**

### Encoding coverage — every form that can encode the pattern

| Form | Searched | Result |
|---|---|---|
| ModRM direct displacement | decoded mem operands, `index == 0` | `disp == 0x242C`: **exactly 1 site — `0x0018CE3A`** |
| SIB indexed (`base + index*scale + disp`) | decoded mem operands, `index != 0` | reachability test `(0x242C - disp) % scale == 0` |
| Register-indirect / computed / alias | covered by the same operand test (base/index registers, any value) | see table |
| Absolute `[abs32]` | `base == 0 and index == 0` | none reaching the slot |
| `A1` moffs / `A3` moffs | raw scan of all sections | none reaching the slot |

**Reachability population (base-register agnostic — "could this store land on
`base+0x242C` for SOME base register value"):**

| Index cap | Stores reaching `base+0x242C` | of which base ≠ `esp` (base-ambiguous) |
|---|---|---|
| ≤ `0x1000` | **1,008** | **956** |
| ≤ `0x10000` | **3,048** | **2,931** |
| ≤ `0x100000` | **3,048** | **2,931** |
| ≤ `0xFFFFFFFF` | **3,220** | **3,074** |

**Memory-write instruction population: 65,888** (write mnemonics only; `fld`/`fcom`/
`fsub`/`fadd`/`fmul`/`fdiv`/`cmp`/`test`/`push`/`pop` excluded, `fst`/`fstp` included).

### Competitors

**The sweep is NOT clean.** `0x00199F45` is one of **3,048 stores whose addressing can
reach `base+0x242C` with index ≤ `0x10000`**, and **2,931 of those are base-ambiguous**
— their base register is not `esp`, so the base could be `software_device` for all this
record shows.

The competitors with the **smallest required index** (i.e. the ones whose index is
plausible for a 2064-element array) include:

```
00012883  mov dword ptr [ecx + eax*4 + 0x98], edx     index=2277
00014049  mov dword ptr [esi + eax*4 + 0x70], edi     index=2287
00015175  mov dword ptr [esi + ecx*4 + 4], eax        index=2314
000178FD  mov dword ptr [ecx + edx*4 + 0x484], esi    index=2026
0018CBAE  mov dword ptr [edx + eax*4 + 0x814], 1      index=1798
00198EF8  mov dword ptr [edi + edx*4 + 0x2068], eax   index=241
00199F45  mov dword ptr [esi + ebp*4 + 0x3ec], eax    index=2064
```

`0x00198EF8` is notable: **required index 241**, base `edi`, disp `0x2068`. Nothing in
this record excludes it.

**Each competitor received the same treatment — and the same failure:**

- **ARG1/ARG3 reachability:** **not established for any competitor**, for the same
  reason it is not established for `0x00199F45` — **the caller's second and fourth
  pushed values are untraced (item 1)**.
- **Value:** **not witnessed for any competitor** — no dump read.
- **Order:** **not witnessed for any competitor** — no execution witness.

**No competitor is claimed and none is excluded.** **The sweep's unresolved coverage
(DSOUND 33.07 %, plus the ~563 k non-padding bytes) is `UNKNOWN`, not uniqueness.**

---

## Summary of the four obligations

| # | Obligation | Outcome |
|---|---|---|
| **1** | INDEX provenance | **NOT WITNESSED.** Precise untraced edge: `sub_00153790` has no direct caller; its vtable slot 51 is reached only via the thunk, which is invoked only through six runtime dispatch tables, each of which has **exactly one reference in the whole image — its own writer**. **ARG1/ARG3 have no caller-side constant and no bounds argument.** |
| **2** | ORDER | **PARTIAL.** Installer confirmed (`0x0018CE30`, called from `0x00012319`, constant `0x0015F9D0` after `xor ebx,ebx` at `0x0001224F`) and fourth read confirmed (`0x00193E62` → `0x00193EB5 call eax`, slot `0x0019D62C`). **The competitor write is NOT identified and the value at the fourth read is NOT witnessed.** |
| **3** | ADDRESS vs BYTES | **BYTE TUPLE** for the `0x00199F45` store — the four `Q()` byte results are packed by `shl`/`or`. **`0x001D5078` is separately confirmed to be an ADDRESS pointing at `djv000_0.adx`.** **Value attribution to the slot is NOT witnessed (fail closed).** |
| **4** | SIB sweep | **DONE, NOT CLEAN.** Population 65,888 writes / 549,980 decoded instructions; coverage stated per section; **3,220 stores can reach `base+0x242C`, 2,931 base-ambiguous, none given a value or order witness.** Unresolved coverage is `UNKNOWN`. |

---

## Terminality

**Row `O-OPEN`.** The writer at `0x00199F45` is **confirmed as an instruction with a
confirmed byte-tuple value form**, but **it is a CANDIDATE, not a confirmed writer of
`software_device+0x242C`**, because **the index is unwitnessed**.

**STOP. PARK with the precise edge:** *the reader of the six runtime dispatch tables
that hold `0x000D4DA0` (`0x00257E00`, `0x00257EF0`, `0x002580D0`, `0x002582B0`,
`0x002586A8`, `0x002587F0`) — and hence the caller of the thunk at `0x000D4DA0`, and
hence the caller's second and fourth pushed arguments. Each of the six tables has
exactly one reference in the whole image, its own writer, so the reader is either in
the undecoded 25.02 % or does not exist.* **RE-REFER for scope. No auto-chain.**

**EDGE 5 (the reader of table `0x00257DE0`) is recorded, not chased** — it is noted
above only because the table layout at `0x00257DE0` is the direct neighbour of the
thunk slot at `0x00257E00`; the reader of `0x00257DE0` was **not** pursued. **C-5 stays
separate.** **Float-bit siblings used only contrastively.** **The retired NULL line was
not reopened and no DR record is cited.**

**`PIO_FREE` untouched. `A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened. `0xFFFFB3` stays
`UNRESOLVED`. No synthetic completion. No generated-code edits.
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, ...)` unchanged.**
