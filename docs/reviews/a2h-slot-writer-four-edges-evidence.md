# `A2h-slot-writer-four-edges-r1` — execution evidence

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a` (Worker), 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-slot-writer-four-edges.md` — revision `A2h-slot-writer-four-edges-r1`,
frozen at SHA-256 `6C207AC90B853744A9C89B71C03187F5E000F0820C2D5D881994B4183A13FB9E`.
**Method:** STATIC ANALYSIS ONLY. No run, no build, no source edit, no instrumentation.
**Row: `O-OPEN`.** **`WRITER: UNKNOWN`.**

## 0. What was measured, and the one method correction that mattered

Every claim below rests on bytes of `game/default.xbe` located through
`game/mygame_analysis.json`'s section map. **Section map (name, VA, raw size, flags):**

```
.text    0x00011000 1555248  PRE,X,HRO     D3D      0x0018CB40  71672  W,PRE,X
DSOUND   0x0019E340  116060  W,PRE,X       MMATRIX  0x001BA8A0   4624  PRE,X
XGRPH    0x001BBAC0    3324  W,PRE,X       XPP      0x001BC7C0  30616  W,PRE,X
.rdata   0x001C3F60  161792  PRE,X         .data    0x001EB760 600340  W,PRE,X
DOLBY    0x0027E080   28056  PRE,X,TRO     $$XTIMAGE/$$XSIMAGE (INS,HRO,TRO)
```

**⚠ THE FINDING THAT CHANGES EVERY PRIOR "COMPLETE SWEEP": a linear decode of a section
drifts at the first data island, and then every later "instruction boundary" is wrong.**
Measured directly:

| Probe | Verdict on the requested VA |
|---|---|
| `owner 0x00199F45` (D3D) | `not an instruction boundary` |
| `owner 0x0018DF59` (D3D) | `start=0x0018DF06` — **drifted**: the true start is `0x0018DF10` |
| `owner 0x000D4DA2` (.text) | `not an instruction boundary` |
| `owner 0x000D4684` (.text) | `not an instruction boundary` |
| `owner 0x000D4DA0` (.text) | `not an instruction boundary` |

So a linear `capstone.disasm(section)` sweep **cannot** support a completeness claim in
either direction. **All completeness claims below were redone with a recursive-descent
walk** (seeds: section starts, every `E8`/`E9` rel32 target, every absolute dword pointing
into code, every `mov reg,IMM` whose immediate points into code, every recovered-function
start, every indirect-jump-table entry) yielding **673,726 exact instruction starts** from
**30,118 seeds**. Alignment for each queried VA was then confirmed by presence in that set.

**A second method trap, confirmed on the packet's own numbers.** An *aligned* dword scan
misses vtable installs:

```
find 0x001E1270 --aligned   -> 0 reference(s)
find 0x001E1270             -> 0x00152246  .text
                               0x00152296  .text
                               0x0015230B  .text
                               3 reference(s)
```

Cause, from raw bytes `00152240: 00 00 75 1D C7 06 70 12 1E 00 ...` — `C7 06` is
`mov dword ptr [esi], imm32`, so the immediate sits at `0x00152246`, **not 4-aligned**.
The record's "THREE references" is **correct**; only an aligned scan says otherwise. The
same non-alignment holds for the six references at `0x0018B1C9`, `0x0018B5A9`,
`0x0018B989`, `0x0018BD69`, `0x0018C3E9`, `0x0018C789`.

## 1. EDGE 1 — `0x00199F45` — **ARG1 confirmed by three-deep stack accounting; value UNREACHABLE**

Decoded from the recursive-descent set (`0x00199DB0` is the true function start; it has
ZERO absolute references and exactly ONE rel32 caller, `0x0015379F`):

```
00199DB0 sub      esp, 0x10            ; E = esp on entry
00199DB3 push     esi                  ; E-4
00199DB4 mov      esi, dword ptr [0x19dce0]
00199DC3 mov      eax, dword ptr [esp + 0x20]   ; (E-4)+0x20 = [E+0x1C] = ARG3  <- trip count
00199DD7 push     ebx                  ; E-8
00199DD8 push     ebp                  ; E-12
00199DD9 mov      ebp, dword ptr [esp + 0x20]   ; (E-12)+0x20 = [E+4]  = ARG1  <- INDEX
00199DDD push     edi                  ; E-16
00199DDE mov      edi, dword ptr [esp + 0x28]   ; (E-16)+0x28 = [E+8]  = ARG2
00199F45 mov      dword ptr [esi + ebp*4 + 0x3ec], eax
```

**`ebp = ARG1`, not ARG3 — reproduced from bytes.** `esi = MEM32(0x19DCE0)` at
`0x00199DB4`, so the store hits the slot iff `ebp = (0x242C − 0x3EC)/4 = 0x810 = 2064`.

**Entry path is unique and forced.** `0x00199DB0` has zero absolute references; its sole
rel32 caller is the forwarding thunk `sub_00153790`, which re-orders the stack:

```
00153790 mov eax, [esp + 0x10]   ; thunk ARG3
00153794 mov ecx, [esp + 0xc]    ; thunk ARG2
00153798 mov edx, [esp + 8]      ; thunk ARG1
0015379C push eax
0015379D push ecx
0015379E push edx                ; pushed LAST -> becomes worker ARG1
0015379F call 0x199db0
```

**So worker ARG1 = the thunk's FIRST argument.** (The pre-correction record's phrasing
"the third argument to `sub_00153790`" is superseded by the packet; confirmed.)

**Can ARG1 reach 2064? NOT ESTABLISHED.** No reachable code materialises 2064 as an
argument. The complete set of reachable uses of the immediate `0x810` is three, and none
is an argument:

```
0x0017F712  test word ptr [ebp - 4], 0x810
0x0017F77F  test word ptr [ebp - 4], 0x810
0x001A18C6  add  ebx, 0x810
```

## 2. EDGE 2 — `0x0018DF59` — **ARG1 = ARG1, `[esp+0x1c]` = ARG2 (packet correction); 1645 UNREACHABLE**

```
0018DF10 sub      esp, 8              ; E2 = esp after this;  E = caller's esp
0018DF13 push     ebx                 ; E2-4
0018DF14 push     esi                 ; E2-8
0018DF15 mov      esi, [esp + 0x14]   ; (E2-8)+0x14 = [E2+0xC] = [E+4]  = ARG1  <- INDEX
0018DF19 push     edi                 ; E2-12
0018DF1A mov      edi, dword ptr [0x19dce0]
0018DF53 mov      ebx, [esp + 0x1c]   ; (E2-12)+0x1c = [E2+0x10] = [E+8] = ARG2  <- VALUE
0018DF59 mov      dword ptr [edi + esi*4 + 0xa78], ebx
```

**⚠ CORRECTION TO THE PACKET.** Line 7 says "`ebx` arbitrary caller word" — true — but the
task statement calls `ebx = [esp+0x1c]` "an ARBITRARY CALLER WORD" without fixing its
argument position. **It is ARG2.** Three-deep: `sub esp,8` + `push ebx` + `push esi` +
`push edi` puts `[esp+0x1c]` at `[E2+0x10] = [E+8]`, the second argument.
`0x242C − 0xA78 = 0x19B4 = 6580 = 4 × 1645` — **confirmed: ARG1 = 1645 hits the slot.**

**ARG1 suppliers, from the 12 rel32 callers of `0x0018DF10`** (9 decoded; matched text
printed so each verdict is checkable against the bytes):

| Call site | Matched text | ARG1 pushed |
|---|---|---|
| `0x0014FE00` | `push eax` / `push edx` | **eax = `[0x264f68][idx]`** (table entry) — UNRESOLVED |
| `0x0014FE12` | `push eax` (`xor eax,eax`) / `push edx` | **0** |
| `0x00150FFC` | `push ecx` / `push 3` | **3** |
| `0x001510D4` | `push 0` / `push 3` | **3** |
| `0x00154AE4` | `push 0` / `push 0` | **0** |
| `0x00154AED` | `push 0` / `push 1` | **1** |
| `0x00154AF6` | `push 0` / `push 2` | **2** |
| `0x00154B31` | `push edi` (`xor edi,edi`) / `push edi` | **0** |
| `0x00154B39` | `push edi` / `push 1` | **1** |
| `0x00154B41` | `push edi` / `push 2` | **2** |
| `0x001989EB` | `push eax` (`[esi+0x2100]`) / `push 0` | **0** |
| `0x00198C58` | `push ecx` (`[esi+0x207c]`) / `push edi` | **edi — UNRESOLVED** |

**Verdict: ARG1 = 1645 is NOT REACHABLE at any of the nine examined sites.** Seven pass a
literal 0, 1, 2 or 3. Two pass a variable: `0x0014FE00` (`eax` = an entry of the table at
`0x264f68`, bounded by `cmp eax,[0x264f70]`) and `0x00198C58` (`edi`, whose provenance is
upstream of `0x00198BF1 xor edi,edi`). Even if 1645 were reachable, **the value written
would be ARG2**, and no caller was shown to pass `0x001D5078`. **EDGE 2 is not closed, but
its reachability is now positively bounded to two unresolved suppliers.**

## 3. EDGE 3 — **the index-51 dispatch is LOCATED** — and it IS EDGE 1's ARG1

**The dispatch site, exact:**

```
000D4DA0 mov      eax, dword ptr [ecx]
000D4DA2 jmp      dword ptr [eax + 0xcc]      ; 0xCC = 51*4
```

`0x000D4DA0` is one of a perfectly regular farm of 16-byte vcall thunks
`mov eax,[ecx]; jmp [eax+disp]` spanning `0x000D4C60`–`0x000D4DBF` (disps `0x60,0x124,
0x104,0xe4,0xc4,0xa4,0x84,0x64,0x44,0x128,0x108,0xe8,0xc8,0xa8,0x88,0x68,0x48,0x12c,
0x10c,0xec,0xcc,0xac`). **A displacement sweep over the entire recursive-descent set finds
exactly ONE indirect call/jmp at `0xCC`:**

```
0x000D4DA2  jmp      dword ptr [eax + 0xcc]
1 reachable instruction(s) with disp 0xCC
```

(of 361 reachable operands with displacement `0xCC`, all others being `mov`/`lea`/`fld`.)

**With `ecx` = the object whose `+0x00` = `0x1E1270` (installed by `0x00152244
mov dword ptr [esi],0x1e1270`), index 51 selects vtable entry `0x001E133C` = `0x00153790`
= the thunk, which calls the worker with its first argument as worker ARG1.**

> ### **Therefore EDGE 1's ARG1 and EDGE 3's ARG1 are the SAME quantity — the index-51
> ### dispatch supplies `ebp`, the `0x199F45` index.**

**The thunk's entry path (new):** `0x000D4DA0` is referenced at six non-4-aligned `.text`
immediates, all inside table-builder code. The builder at `0x0018B1A0` is representative:

```
0018B1A0 mov eax, 0xd4bc0
0018B1A5 mov dword ptr [0x257de0], eax
0018B1AA mov eax, 0xd4c30
0018B1AF mov dword ptr [0x257de8], eax
...
0018B1C8 mov eax, 0xd4da0
0018B1CD mov dword ptr [0x257e00], eax     ; index 8 of table base 0x257DE0
```

and that table is installed on an object field:

```
000D4684 mov      dword ptr [esi + 0x3c8], 0x257de0
000D467A mov      dword ptr [esi + 0x3c4], 0x257cf0
```

**Verdict on the edge's own question — `UNKNOWN`.** A 20-instruction dataflow probe
(`mov R,[base+0x3C8]` … `call/jmp [R+k]` or `call R`) over **all 39 reachable functions
that touch `+0x3C8`** returned **0 sites**. **No dispatcher through the `0x257DE0` table is
located, so the value the dispatch caller places in ARG1 is not established.**
**This is the broken edge.**

## 4. EDGE 4 — **the alias exists, but it does NOT reach the slot**

```
00192531 mov      eax, dword ptr [eax + 0x20]    ; the ALIASED OBJECT (descriptor)
00192534 test     eax, eax
00192536 je       0x192585
00192538 mov      ecx, dword ptr [esp + 8]       ; the CONTEXT object
0019253C add      ecx, 0x2458
00192542 mov      dword ptr [eax], ecx           ; descriptor+0x00
00192548 add      edx, 0x2abc
0019254E mov      dword ptr [eax + 4], edx       ; descriptor+0x04
0019255B mov      dword ptr [eax + 8], edx       ; descriptor+0x08
00192562 add      ecx, 0x2268
00192568 mov      dword ptr [eax + 0xc], ecx     ; descriptor+0x0C = CONTEXT
```

**Aliased object = `MEM32(obj+0x20)`. Field = `descriptor+0x0C`. Value = `obj+0x2268` = the
CONTEXT (established identity).** A 40-instruction forward dataflow probe over every
`mov reg,[base+0x20]` in the reachable set found **0** uses of `[reg+0xC]`.

**And the decisive structural fact:** a store sweep over the entire recursive-descent set
finds **118 stores at displacement `0x1C4`**, and the ONLY instruction anywhere in the
image that reads the ONE slot is the established callback read:

```
0x00193E62  mov      eax, dword ptr [esi + 0x1c4]     ; in sub_00193D90, esi = ecx = context
```

That read uses **`esi = context` directly**, never through the descriptor. **So EDGE 4 is
not the mechanism: no write through the `descriptor+0x0C` alias reaches `context+0x1C4`.**
The builder `0x00192530` is itself **not reachable** by recursive descent.

## 5. The unique direct store, and the arithmetic-reachability superset

**Uniqueness re-verified at exact alignment** (not by a raw byte scan): exactly **one**
reachable operand with literal displacement `0x242C` —

```
0x0018CE3A  WRITE  mov dword ptr [ecx + 0x242c], eax
1 literal 0x242C operand(s)
```

`ecx = MEM32(0x19DCE0)` at `0x0018CE34`. **Reaching definition of `eax`:**

```
0018CE30 mov      eax, dword ptr [esp + 4]     ; ARG1
0018CE34 mov      ecx, dword ptr [0x19dce0]
0018CE3A mov      dword ptr [ecx + 0x242c], eax
0018CE40 ret      4
```

**The slot's ONLY entry path (complete):**

```
0x00012319  call 0x15f9e0        ; sole rel32 caller of the setter
  0015F9E0  mov eax, [esp+4]
  0015F9E4  test eax, eax
  0015F9E6  jne 0x15f9f6
  0015F9E8  mov eax, 0x15f9d0     ; 0 -> 0x15F9D0   (the INSTALLED value)
  0015F9ED  mov [esp+4], eax
  0015F9F1  jmp 0x18ce30
  0015F9F6  cmp eax, -1
  0015F9F9  jne 0x15f9fd
  0015F9FB  xor eax, eax          ; -1 -> 0
  0015F9FD  mov [esp+4], eax
  0015FA01  jmp 0x18ce30
```

`call 0x0015F9E0` has exactly **1** caller and `0x0018CE30` has exactly **2** entry paths
(both the jmps above) and **0** byte references. **So the slot's value is fixed by the
single caller at `0x00012319`.**

**⚠ A packet exclusion that is incomplete — measured, not argued.** The packet excludes
`0x0018DF87` on the VALUE ground that it writes only `0xFFFFFFFF`. An arithmetic sweep
(`index*4 + disp == 0x242C`, `index ≥ 0`) over the reachable set finds candidate
computed writers, and the packet's exclusion list covers only one of them:

| VA | Matched text | Required index | In packet's exclusions? |
|---|---|---|---|
| `0x0018DF59` | `mov dword ptr [edi + esi*4 + 0xa78], ebx` | 1645 | yes (EDGE 2) |
| `0x0018DF87` | `mov dword ptr [edi + esi*4 + 0xc], 0xffffffff` | 2312 | yes (value `0xFFFFFFFF`) |
| **`0x0018E03E`** | **`mov dword ptr [edi + esi*4 + 0xc], ebx`** | **2312** | **NO** |
| `0x0018E02B` | `mov eax, dword ptr [edi + esi*4 + 0xc]` | 2312 | read |
| `0x0018DEDB` | `lea esi, [ecx + eax*4 + 0xa78]` | 1645 | EDGE 2 entry |
| `0x00197201`…`0x0019724E` | 12 × `mov dword ptr [eax + ecx*4 + 0x3c8..0x3f4], edx` | 2073…2058 | NO (uniform constant fill) |

**I then falsified my own `0x0018E03E` hit rather than publishing it.** The base `edi` is
NOT `software_device` at that point: `0x0018E00C mov edi, dword ptr [esp + 0x10]` reloads
`edi` from the stack, overwriting the `mov edi,[0x19dce0]` at `0x0018DF1A`. My sweep's
"base came from `MEM32(0x19DCE0)` somewhere in the function" heuristic is **unsound for
registers that are reassigned**, and `0x0018E03E` is a **false positive of my own tool**.
The `0x00197201` block is a uniform 12-entry table fill with a single constant `edx` and is
likewise not a slot writer. **No computed writer of the slot was positively established.**

## 6. What `0x001D5078` is — a new, structural datum

A raw dword scan for `0x001D5078` returns **59 references, all in `.rdata`, at
`0x001D4BD4`…`0x001D4DA4`, stride 8** — i.e. the *second* field of an 8-byte-stride
`(code, name-pointer)` table:

```
001D4BC0: 7F 02 00 00 98 50 1D 00     ; code 0x27F -> 0x001D5098 "djv111_03.adx"
001D4BD0: 81 02 00 00 78 50 1D 00     ; code 0x281 -> 0x001D5078
001D4BD8: 82 02 00 00 78 50 1D 00
...
001D4DA0: BB 02 00 00 78 50 1D 00     ; code 0x2BB -> 0x001D5078
001D4DA8: FF FF FF FF 00 00 00 00     ; TERMINATOR
```

and the bytes at that VA are the mandated control string:

```
001D5078: 64 6A 76 30 30 30 5F 30 2E 61 64 78 00 00 00 00   ->  "djv000_0.adx"
001D5088: 64 6A 76 31 31 32 5F 30 2E 61 64 78 00 00 00 00   ->  "djv112_0.adx"
001D5098: 64 6A 76 31 31 31 5F 30 33 2E 61 64 78 00 00 00   ->  "djv111_03.adx"
```

**`0x001D5078` is the ADX audio filename `"djv000_0.adx"`**, and 59 consecutive code
values (`0x281`–`0x2BB`) map to it — verified numerically, not by eye. **This is consistent
with the type-confusion shape the record already holds at arm's length, and it is NOT
promoted here.** The table has **zero direct references**; the three code references near
it (`0x00116136`, `0x00116143`, `0x00116B9F`, `0x00117264`) all target `0x001D4DB0`, the
*base of the next table*, not this one — so the table is indexed by a computed base that no
scan has yet pinned.

## 7. Row, and the smallest uncovered edge

**Row `O-OPEN`.** No complete instruction-anchored reaching-definition chain from a
concrete `0x001D5078` write into the ONE slot through the fourth-iteration read exists.
Resemblance and value equality were not used as a substitute.

**`SMALLEST_MISSING_EDGE` — EDGE 3's dispatch caller.** It is now the single quantity that
decides the whole question, because **EDGE 3's ARG1 *is* EDGE 1's ARG1**:

> Locate the code that reads the table installed at `object+0x3C8` (base `0x257DE0`) and
> calls entry 8 (`0x000D4DA0`) with `ecx` = the object carrying vtable `0x1E1270`; then
> read the value it places in the worker's ARG1 slot and test it against 2064.

**Two caveats a successor must carry, both measured here:** `0x000D4DA0` and `0x000D4684`
are themselves **not reachable** by recursive descent from call/jmp/vtable/absolute-pointer
seeds, which proves my seeding has a gap — a dispatcher through `+0x3C8` may exist and be
missed by it. And the direct-store path still needs one unread range: the caller's source
at `0x000122E8..0x00012320`, read *through* `0x0018CE30`'s argument slot.

## 8. Controls — each run, and its result

| # | Control | Result |
|---|---|---|
| 1 | **OFFSET-SHIFT `VA`/`VA+1`** (`30766A64` then `3030766A`) | **NOT RUN.** I performed **zero** `inspect-jsrf.py memory` reads, so no dump-derived address was interpreted and the control had no input. **Not claimed as passed.** |
| 2 | **`int(text,16)` not `int.from_bytes(...)`** | **NOT APPLICABLE.** No `memory` command output was parsed. All values came from `data`/`find`/direct XBE bytes as little-endian dwords. **Not claimed as passed.** |
| 3 | **Shift agreement `(v1 & 0x00FFFFFF) == (v0 >> 8)`** | **NOT APPLICABLE** — no `VA`/`VA+1` pair was read. **Not claimed as passed.** |
| 4 | **Per-run `check-dump-mapping.py` gate** | **NOT RUN — no dump was read.** The 23/24 census was not used and no run row was selected. |
| 5 | **Disasm alignment** | **APPLIED, and it caught three real defects** — see §0: aligned-vs-unaligned vtable refs (`0` vs `3`, record's `3` confirmed), linear-decode drift in `D3D` before `0x00199F45` and in `.text` before `0x000D4DA2`/`0x000D4684`, and the six non-aligned thunk-table immediates. All completeness claims redone with recursive descent. |
| 6 | **X-flagged sections swept** | **DONE.** `.text`, `D3D`, `DSOUND`, `MMATRIX`, `XGRPH`, `XPP`, `.rdata`, `.data`, `DOLBY` — all carry `X` and all were scanned. |
| 7 | **Every table row verified against the bytes, matched text printed** | **DONE.** §2's 12-row caller table and §5's candidate table each print the matched instruction text beside the verdict. One published-looking hit (`0x0018E03E`) was **falsified and withdrawn** rather than reported as a writer. |
| 8 | **`0x242C` uniqueness re-verified at exact alignment** | **DONE** — exactly one reachable operand, `0x0018CE3A` (write). |

## 9. Boundaries observed

Read-only static analysis. **No game run, no build, no execution test, no source edit, no
instrumentation, no generated-code edit.** `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, ...)`
untouched. No synthetic completion of any kind. `PIO_FREE`, `A4b2-r7`, `A4b2-r8`,
`A4b1-r4` and `0xFFFFB3` not touched; the retired NULL line was not reopened and no DR
record was cited. **C-5 was kept separate** — the `sub_00194ADD` caller and the
`KeInsertQueueDpc` site were not searched for and did not select this row. Float-bit
siblings `0x3E800000` / `0x41200000` stayed contrastive. **No run-profile or strictness
claim is made.**
