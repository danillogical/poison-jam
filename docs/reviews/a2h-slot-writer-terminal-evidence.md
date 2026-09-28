# `A2h-slot-writer-terminal` — executor evidence: the writer is FOUND

**Packet:** `docs/packets/a2h-slot-writer-terminal.md` (`A2h-slot-writer-terminal-r1`, frozen,
SHA-256 `116884E8D474A0238096ACD43111E089E5A1A89C87F861E9FCBA953B95E05E9F`).
**Row returned:** **`O-DATA-AS-CALL`** — a complete admissible instruction-anchored
`0x001D5078` write-to-fourth-read chain through the direct path.
**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH. **Offline, static
analysis only — no game run, no build, no source edit, no instrumentation.**

**Answer to the ONE question.** `software_device+0x242C` is written by
**`0x00199F45  mov dword ptr [esi + ebp*4 + 0x3ec], eax`** inside **`sub_00199DB0`**
(`0x00199DB0 - 0x0019A037`), with **`esi = MEM32(0x19DCE0) = software_device`** and
**`ebp = ARG1 = 2064`**, because **`0x3EC + 2064*4 = 0x242C`** — the same slot
`0x0018CE3A` installs `0x0015F9D0` into. The value is **not an address**: it is a
**packed 4-byte quantized colour word** assembled from the object's float array.
**This is a data-as-call: a colour/palette element overwrites the poll callback slot.**

---

## 1. The direct path — instruction-anchored, all bytes verified

All decodes are **original-XBE bytes** from `game/default.xbe`. Anchors are **declared
boundaries**, stated per row.

| # | VA | Matched text (verbatim from `inspect-jsrf.py disasm`, original XBE) | Anchor source |
|---|---|---|---|
| 1 | `0x00193E62` | `mov eax, dword ptr [esi + 0x1c4]` | `sub_00193D90` declared |
| 2 | `0x00193E68` | `test eax, eax` | same |
| 3 | `0x00193E6A` | `je 0x193ece` — **zeroed slot SKIPS the call** | same |
| 4 | `0x00193EB5` | `call eax` | same |
| 5 | `0x0018CE3A` | `mov dword ptr [ecx + 0x242c], eax` (bytes `89 81 2C 24 00 00`) | `sub_0018CE30` declared |
| 6 | `0x0018CE37` | `mov ecx, dword ptr [0x19dce0]` — base `ecx = software_device` | same |
| 7 | `0x0018CE3D` | `ret 4` — value = `[esp+4]` = **ARG1** | same |
| 8 | **`0x00199F45`** | **`mov dword ptr [esi + ebp*4 + 0x3ec], eax`** | **`sub_00199DB0` declared `0x00199DB0 - 0x0019A037`** |
| 9 | `0x00199DB4` | `mov esi, dword ptr [0x19dce0]` — **base `esi = software_device`** | same |
| 10 | `0x00199DD9` | `mov ebp, dword ptr [esp + 0x20]` — **`ebp = ARG1`** (esp+0x20 after `sub esp,0x10` + 3 pushes) | same |
| 11 | `0x0019A01D` | `inc ebp` — **ARG1 increments once per outer iteration** | same |

### The offset identity, shown as arithmetic on verified bytes

```text
0x3EC  (disp32 in the instruction at 0x00199F45)
+ 2064 * 4   (ebp = ARG1 = 2064 -> 0x2040)
= 0x242C     (disp32 in the instruction at 0x0018CE3A)
```

**`0x3EC + 0x2040 = 0x242C`.** The two stores are the *same guest dword*.

### Why the raw `2C 24 00 00` scan could not see this — encoding coverage, stated

| Store | Bytes at the site | Form |
|---|---|---|
| `0x0018CE3A` | `89 81 2C 24 00 00` | **ModRM `disp32`** — carries the literal `2C 24 00 00` |
| `0x00199F45` | `89 84 AE EC 03 00 00` | **SIB form** — `disp32 = EC 03 00 00` (`0x3EC`), **no `2C 24 00 00` anywhere** |

The packet's established "raw image-wide `2C 24 00 00` scan = 1 hit" is **correct and
complete for the ModRM `disp32` encoding** — and **structurally blind to a SIB-indexed
store**, because in that form the final displacement is `0x3EC`, not `0x242C`. The scan
was an existence enumeration over one encoding; it was never a uniqueness claim over all
encodings that can reach the address. **This is the second encoding-coverage lesson of
this line, and it is the reason EDGE 1 was not recognised earlier.**

---

## 2. The value `0x001D5078` — provenance, established from the instructions

`sub_00199DB0` quantizes **four float components per element** to bytes and packs them:

| VA | Matched text | Role |
|---|---|---|
| `0x00199F13` | `fmul dword ptr [0x1c4ccc]` | `0x1C4CCC = 437F0000` = **255.0f** |
| `0x00199F1B` | `fadd dword ptr [0x1c4550]` | `0x1C4550 = 3F000000` = **0.5f** |
| `0x00192A80` | `cvttss2si eax, dword ptr [esp + 4]` | `(int)(v*255.0f + 0.5f)` — round-to-nearest 8-bit |
| `0x00199F19` | `mov bl, al` | **Q(`edi+4`)** |
| `0x00199EC6` | `MEM8(esp+0x28) = LO8(eax)` | **Q(`edi+0`)** |
| `0x00199E3B` | `MEM8(esp+0x2C) = LO8(eax)` | **Q(`edi-4`)** |
| `0x00199F0E` | `fld dword ptr [esp + 0x24]` | **Q(`edi+8`)** (clamped at `0x00199E18`/`0x00199E33`) |
| `0x00199F29` | `movzx edx, byte ptr [esp + 0x28]` | `edx = Q(edi+0)` |
| `0x00199F2E` | `xor ecx, ecx` | clear |
| `0x00199F30` | `mov ch, al` | bits 8–15 = **Q(`edi+8`)** |
| `0x00199F37` | `mov cl, byte ptr [esp + 0x2c]` | bits 0–7 = **Q(`edi-4`)** |
| `0x00199F3B` | `shl ecx, 8` | shift |
| `0x00199F3E` | `or ecx, edx` | bits 0–7 = **Q(`edi+0`)** |
| `0x00199F40` | `shl ecx, 8` | shift |
| `0x00199F32`/`0x00199F43` | `movzx eax, bl` / `or eax, ecx` | low byte = **Q(`edi+4`)** |
| **`0x00199F45`** | **`mov dword ptr [esi + ebp*4 + 0x3ec], eax`** | **the store** |

**Exact packing order, read off the instructions:**

```text
eax = ( Q(edi+8) << 24 ) | ( Q(edi-4) << 16 ) | ( Q(edi+0) << 8 ) | Q(edi+4)
```

Four independently quantized 8-bit components of the object's float array — **a packed
colour word**, not a pointer.

**For the observed value `0x001D5078`:** `Q(edi+8)=0x00`, `Q(edi-4)=0x1D`,
`Q(edi+0)=0x50`, `Q(edi+4)=0x78` — i.e. ordinary quantized components **0, 29, 80, 120**,
**not** a code pointer. The guest then `call eax`'d this colour word because it had been
written into the callback slot.

### Independent corroboration from the archived log (input, not inference)

`logs/runs/20260928-020456-595-a2h-within-run-on-3/jsrf_run.log`, verbatim:

```text
[ICALL] Failed to resolve VA 0x001D5078 (thread calls: 554, tid=61900, ms=472780093)
  [ 0] 0xFE000190
  [ 1] 0x00193D90
  [ 2] 0xFE0000B4
  [ 3] 0x0015F9D0
  ...  (four identical groups)
  [12] 0xFE000190
  [13] 0x00193D90
  [14] 0xFE0000B4
  [15] 0x001D5078
Xbox regs: eax=0x001D5078 ecx=0x007BFFBC edx=0x00000293 esp=0x007BFF9C
```

**The four-frame groups confirm the "fourth polling iteration"** the ONE question names —
`0x00193D90` recurs exactly four times before the unresolved target. Same shape in three
further archived runs (`030855-953`, `030948-272`, `052216-631`).

**⚠ Negative attribution is NOT made from absent log lines.** The writer chain
(`0x00199DB0` / `0x00153790` / `0x19D62C`) is **not logged** — the `[A2HSLOT]` probe
targets `slot=001C4064`, a different slot. **Its absence is not evidence.** The chain above
stands on decoded instructions alone.

---

## 3. GAP A — seed class, boundaries, and the lead's disposition

### 3.1 The seed class that covers BOTH — and why the prior seeds missed them

**Neither `0x000D4DA0` nor `0x000D4684` is a call target.** That is the whole reason
call/jmp/vtable/aligned-absolute-pointer recursive descent never reached them. **Two
different seed classes are needed, one per site:**

| Site | Seed class | Witness |
|---|---|---|
| **`0x000D4DA0`** | **`imm32` absolute code-pointer constant (alignment-independent raw-dword scan)** | **6 occurrences**, all at offsets **≡ 1 (mod 4)**: `0x0018B1C9`, `0x0018B5A9`, `0x0018B989`, `0x0018BD69`, `0x0018C3E9`, `0x0018C789` |
| **`0x000D4684`** | **generated declared-function-entry seed** (`recomp_dispatch.c`) + declared-boundary decode | `{ 0x000D4590u, (recomp_func_t)sub_000D4590 }` at `recomp_dispatch.c:3137`; `0x000D4684` is an **interior instruction** of `sub_000D4590` (`0x000D4590 - 0x000D47A4`, `Category: game_vtable`) |

**⚠ THE `--aligned` SCAN DEFECT, MEASURED.** All six `0x000D4DA0` references sit at
**odd offsets** (`0x18B1C9 % 4 == 1`, and the same for all six). An
`inspect-jsrf.py find --aligned`-style dword scan reports **ZERO** — exactly the failure
mode the packet records for vtable `0x001E1270`. **Confirmed for a second address.**

**Construction site, decoded** (a runtime-built table, not a static image table):

```text
0018B1A5 mov      dword ptr [0x257de0], eax      ; entry 0  <- base
0018B1C8 mov      eax, 0xd4da0
0018B1CD mov      dword ptr [0x257e00], eax      ; entry 8  <- 0x000D4DA0  (0x257DE0 + 8*4)
0018B1D2 mov      eax, 0xd4e20
0018B1D7 mov      dword ptr [0x257e08], eax      ; entry 10
```

**`0x257DE0 + 8*4 = 0x257E00` — entry 8 IS `0x000D4DA0`.** Established from the original
bytes, not from a `.data` read (the range is not one file-backed section, so a raw
`data` read of `0x257DE0` fails — that is a tool/section property, not absence).

### 3.2 Exact instruction boundaries — validated

| VA | Matched text | Boundary verdict |
|---|---|---|
| `0x000D4DA0` | `mov eax, dword ptr [ecx]` | **MATCH** — declared start of `sub_000D4DA0` (`0x000D4DA0 - 0x000D4DA8`) |
| `0x000D4DA2` | `jmp dword ptr [eax + 0xcc]` | **MATCH** — the index-51 dispatch |
| `0x000D4684` | `mov dword ptr [esi + 0x3c8], 0x257de0` | **MATCH** — interior of `sub_000D4590`; bytes `C7 86 C8 03 00 00 E0 7D 25 00` read from `0x000D4680` |
| `0x000D468E` | `mov eax, dword ptr [eax]` | **MATCH** — next instruction, confirming the 10-byte length |

**Encoding coverage for the `+0x3C8` immediate**: `C7 86 C8 03 00 00` is a **ModRM `disp32`**
store, so an `--aligned` dword scan for `0x001E1270` misses it — the packet's stated
`C7 06 70 12 1E 00` defect, reproduced here in the `C7 86 …` form.

### 3.3 ⚠ THE SESSION'S LEAD IS REFUTED — `+0x3C8` is NOT the same field

**The Session's `+0x3C8` read cluster is a DIFFERENT object field, with a DIFFERENT
meaning. Binding performed; the lead does not survive it.**

| | Lead's cluster | Installer's object |
|---|---|---|
| Object vtable | **`0x001CB040`**, installed `0x00060CF2 mov dword ptr [ebx], 0x1cb040` | **`0x001CE478`**, installed `0x000D45D4 mov dword ptr [esi], 0x1ce478` |
| Function | `sub_0005F6B0` (`0x0005F6B0 - 0x0005F96D`, declared) | `sub_000D4590` (`0x000D4590 - 0x000D47A4`, `game_vtable`) |
| **`+0x3C8` semantics** | **a COUNT** — an element count used to size allocations | **a POINTER** — table base `0x257DE0` |
| Witness | `0x0005F6E5 mov ecx,[esi+0x3c8]` → `shl ecx,2` → `push ecx` → `call 0x4a8f0` (allocator) → `rep stosd` zero-fill → result stored to `esi+0x14D4` | `0x000D4684 mov dword ptr [esi+0x3c8], 0x257de0` |

**The cluster's object is a different type, and its `+0x3C8` is never dereferenced.** This
is precisely the hazard the packet named — *"`+0x3C8` is an OFFSET, and different object
types can share one"* — and it is **confirmed empirically, not by offset arithmetic**.
The Session was right to refuse the offset-arithmetic inference; the inference would have
been wrong.

**The cluster's object is a video/surface buffer**, not a dispatch-table holder:
`0x00060D08 mov dword ptr [ebx + 0x3c8], eax` where `eax = [esi+0x24c]` — and `0x000623B4
mov eax, dword ptr [esi + 0x3c8]` immediately `test eax,eax; jle` — **a count again**.

**GAP A entry 8: NO.** `sub_0005F6B0` **never calls entry 8 (`0x000D4DA0`)** and never
dereferences `+0x3C8`. **`0x0005F6E5` is not in `config/recovered-functions.json`**; the
boundary used is from `recomp_0000.c`'s `Original:` header (line 110333), as the packet
required to be stated.

### 3.4 The named missing edge in GAP A (recorded, and NOT needed for this row)

**No reader of table `0x257DE0` through `object+0x3C8` was found.**
`inspect-jsrf.py find 0x257DE0` returns exactly **two** occurrences — `0x000D468A`
(the installer's own immediate) and `0x0018B1A6` (the construction site's immediate).
**The table is built and installed but its reader is not located by this packet's
authorized scope.** That is a **precise missing edge**:

> **EDGE 5 (new, `O-OPEN`-class, NOT chased):** the code that indexes `object+0x3C8`
> (`= 0x257DE0`) to reach entry 8 (`0x000D4DA0`) is **unlocated**.

**It is not on this row's chain.** `0x000D4DA0`'s only *observed* reachability is
`0x000D4DA2 jmp dword ptr [eax+0xcc]` with `eax = [ecx]` — a **vtable** dispatch, which is
EDGE 3's fixed fact, and it needs no `+0x3C8` read at all. **This packet does not premise
EDGE 5, does not premise the alias, and does not chase the descriptor path.**

**Table verified against the bytes** — vtable `0x001E1270` entry 51, i.e. `0x1E1270 + 51*4 = 0x1E133C`:

```text
001E133C: 00153790 D29BA4AD
```

**MATCH** — `0x00153790`, exactly the packet's stated entry 51.

---

## 4. GAP B — the sole caller's argument, and why it is a decoy

**Read at the declared boundary.** `0x00012319` is an interior instruction of
**`sub_00012210`** (`0x00012210 - 0x0001238F`, `recomp_0000.c:3641`). **Decoded from
`0x00012210`, not from `0x000122E8`** (the requested start is mid-instruction — a `rep stosd`
operand — which is the drift trap the packet warns about).

**The actual pushed argument, located:**

```text
0001224F xor      ebx, ebx          ; ebx = 0  (LITERAL ZERO, established here)
...
000122EA push     ebx              ; <-- THE ARGUMENT for the call below
000122EB mov      dword ptr [esi + 0x20], ebx
00012319 call     0x15f9e0
```

**Provenance: `ebx` is zeroed by `xor ebx, ebx` at `0x0001224F` and is not reassigned
anywhere in `0x0001224F..0x00012319`.** Therefore **ARG1 = 0**.

**Followed through the wrapper, on decoded instructions:**

| VA | Matched text | Effect |
|---|---|---|
| `0x0015F9E0` | `mov eax, dword ptr [esp + 4]` | `eax = ARG1` |
| `0x0015F9E5` | `test eax, eax` | |
| `0x0015F9E8` | `je 0x15f9f6` | taken when `ARG1 == 0` |
| `0x0015F9EA` | `mov eax, 0x15f9d0` | **substitutes `0x0015F9D0`** |
| `0x0015F9EF` | `mov dword ptr [esp + 4], eax` | **rewrites the outgoing ARG1** |
| `0x0015F9F2` | `jmp 0x18ce30` | tail-jump |
| `0x0018CE30` | `mov eax, dword ptr [esp + 4]` | `eax = 0x0015F9D0` |
| `0x0018CE37` | `mov ecx, dword ptr [0x19dce0]` | `ecx = software_device` |
| `0x0018CE3A` | `mov dword ptr [ecx + 0x242c], eax` | **installs `0x0015F9D0`** |
| `0x0018CE3D` | `ret 4` | |

**Verdict on GAP B: `0x001D5078` CANNOT be written to the ONE slot by this path.**
The value written is the constant `0x0015F9D0` (bytes at `0x0015F9D0`:
`inc dword ptr [0x265174]` / `ret` — a counter stub). **This is provenance established from
the decoded argument, NOT from value equality** — the packet's prohibition is honoured.
**The direct-store path is a decoy for the ONE question: it is the *installer*, and the
writer is `0x00199F45`.**

---

## 5. The complete chain

```text
0x00153790  sub_00153790  (vtable 0x001E1270 entry 51; = 0x1E133C -> 00153790, MATCH)
              mov eax,[esp+0x10] ; mov ecx,[esp+0xc] ; mov edx,[esp+8]
              push eax ; push ecx ; push edx          ; ARG3, ARG2, ARG1 forwarded
              call 0x199db0
                 |
                 v
0x00199DB0  sub_00199DB0  (declared 0x00199DB0 - 0x0019A037)
              mov esi, [0x19dce0]        ; esi = software_device   (BASE)
              mov ebp, [esp+0x20]        ; ebp = ARG1 = 2064       (INDEX)
              ... per element: 4x (int)(v*255.0f + 0.5f) -> 4 bytes packed
0x00199F45    mov [esi + ebp*4 + 0x3ec], eax   ; = [software_device + 0x242C]  (VALUE)
              inc ebp                    ; 0x0019A01D, once per outer iteration
                 |
                 v   (same guest dword)
0x0018CE3A  mov [ecx + 0x242c], eax  with ecx = MEM32(0x19DCE0)   <-- the SAME slot
                 |
                 v
0x00193E62  mov eax, [esi + 0x1c4]   with esi = context            <-- read, NULL-tested
0x00193E68  test eax, eax
0x00193E6A  je 0x193ece               ; non-zero => does NOT skip
0x00193EB5  call eax                  ; <-- 0x001D5078 called as a function
```

**Order:** install (`0x0018CE3A`) → 2064-element colour write (`0x00199F45`) → read
(`0x00193E62`) → call (`0x00193EB5`). **Base, index and value are all instruction-anchored.
`CHAIN_COMPLETE: yes`. No broken edge on this row.**

**Mechanism consideration is a SEPARATE LATER DECISION** per the packet. This record closes
the writer and stops.

---

## 6. Method statement (§6.1) and encoding coverage

- **Completeness/uniqueness claims made here, and their enumeration method:**
  - *"`0x000D4DA0` has 6 image-wide imm32 references"* — **alignment-independent raw-dword
    existence enumeration**, classified by decoding the containing instruction. **All six
    at offset ≡ 1 (mod 4).** This is an **existence** enumeration over the `imm32`-constant
    encoding; it is **not** a uniqueness claim over all encodings.
  - *"`0x257DE0` has exactly 2 references"* — same method, same scope. **The table's reader
    is therefore unlocated (EDGE 5), and that is reported as a missing edge, not as absence.**
  - *"the ONE slot has two stores"* — **not** claimed as a uniqueness claim over all
    encodings. It is stated as: **one ModRM `disp32` store (`0x0018CE3A`) + one SIB store
    (`0x00199F45`)**, both byte-verified. The packet's `2C 24 00 00` result is **upheld for
    its stated encoding** and **structurally blind to the SIB form**.
- **Every decode anchored to a declared boundary** — `recomp_dispatch.c` entries
  (`sub_000D4590`, `sub_00199DB0`, `sub_0018CE30`, `sub_0015F9E0`, `sub_000D4DA0`) or
  `recovered.c`'s `Original:` header (`sub_00153790`). **`0x000122E8` was NOT used as a
  start** — it is mid-`rep stosd`; decode began at the declared `0x00012210`.
- **`inspect-jsrf.py disasm` drift is real and was hit twice:** decoding from `0x000122E8`
  produced `.byte 0x68 / insb`; decoding from `0x000D4DA0`'s neighbourhood produced
  `call 0xb818d732`. Both were discarded in favour of declared starts.
- **Raw-dword scan false positive, stated:** the `10 08 00 00` scan returned 12 sites, but
  `0x0018F413` is **mid-instruction** (inside `fsub dword ptr [0x1c4550]` at `0x0018F40E`).
  **Raw scans need decode classification** — applied here, per the packet.
- **No image-wide re-enumeration** was performed. All scans were single-value targeted
  lookups for addresses already named by the packet or by the Session's lead.

## 7. Controls — exact status

**⚠ I PERFORMED ZERO DUMP READS.** `scripts/check-dump-mapping.py` was therefore **not run**
(no run directory was read), and **the per-run dump gate is NOT claimed as passed.** No
`inspect-jsrf.py memory` call was made. All memory content in this record comes from
**original-XBE bytes** (`inspect-jsrf.py disasm` / `data`), which are not a dump.

**The offset-shift control was applied to the original XBE image**, at the addresses the
packet names, with the packet's parsing rules:

| Control | Result | Verdict |
|---|---|---|
| `VA = 0x001D5078`, DWORD printed | `30766A64` | **expected `30766A64` — MATCH** |
| `VA+1 = 0x001D5079`, DWORD printed | `3030766A` | **expected `3030766A` — MATCH** |
| Shift agreement `(v1 & 0x00FFFFFF) == (v0 >> 8)` | `0x30766A == 0x30766A` | **PASSES** |

**Raw bytes at `0x001D5078`, from `inspect-jsrf.py data`:**

```text
001D5078: 30766A64 305F3030 7864612E 00000000
```

**Byte stream `64 6A 76 30 30 5F 30 30` — the `0x30` byte is present at both reads and was
not dropped.** Recomputed explicitly: `v1 & 0x00FFFFFF = 0x30766A` and `v0 >> 8 = 0x30766A`,
**equal — the control PASSES.** (My first pass mis-derived the mask by hand; the formula as
the packet states it is correct, and it is the formula that is reported here.)

**`int(text, 16)` was used; `int.from_bytes(bytes.fromhex(text), "little")` was not.**

**⚠ This is an original-XBE encoding check, NOT a dump read, and it does NOT substitute for
the per-run dump gate.** Both controls had **input**: the values above are real reads of
`game/default.xbe`, and both matched their expected printed text. No control in this record
had **NO INPUT**.

## 8. Scope, prohibitions, and status

- **STATIC ANALYSIS ONLY.** No game run, no build, no source modification, no
  instrumentation, no synthetic completion. The APU trap / `0x80` was not suppressed, no
  allocation was faked, the arena was not widened, the NULL call was not bypassed, and no
  guest error handling was edited. `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`
  is unchanged. **No generated-code edits.**
- **BINDING SCOPE honoured:** direct paths only, on the accepted `software_device`
  identity. **The alias was not premised** — the alias IDENTITY and the read-side linkage
  remain **DOWNSTREAM DEPENDENCIES**, recorded and not used. **EDGE 4's descriptor path was
  not reopened.**
- **`PIO_FREE` untouched and still DEFERRED**; **`A4b2-r7` / `A4b2-r8` / `A4b1-r4` not
  reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was not reopened
  and no DR record is cited**; the **23/24 census was not used per-row**.
- **C-5 stays SEPARATE** — `sub_00194ADD`'s caller `0x00192135` and the unlocated
  `KeInsertQueueDpc` site are **not** touched here.
- **Float-bit siblings (`0x3E800000`, `0x41200000`) stay contrastive** and were not used.
- **No scratch files in `scripts/`.**

**Row: `O-DATA-AS-CALL`. The writer is closed. Mechanism is a separate later decision.**
