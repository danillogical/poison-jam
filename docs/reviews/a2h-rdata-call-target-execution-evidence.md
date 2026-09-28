# `A2h-rdata-call-target` — execution evidence: **`O-OPEN`** on the reaching definition of `[esi+0x1C4]`

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH (execution Worker).
**Packet:** `docs/packets/a2h-rdata-call-target.md`, frozen r1 (22 lines).
**Method:** **STATIC ANALYSIS ONLY.** No run, no build, no source modification, no instrumentation.
**Inputs:** identity-pinned `game/default.xbe` (2 281 472 bytes), `game/mygame_analysis.json` section
map, `src/recomp/gen/`, `src/recomp/recovered/`, `config/recovered-functions.json`.

**Row: `O-OPEN`.** The call site is **unique and `loc_`-anchored**, the candidate table is **validated
and refuted as a code table**, and the object and field are **identified by a proved alias**. The chain
**does not close**: the writer of the code-target field is **not statically determinable**, because the
generated dispatch path for the object's own constructor is absent from the translation and the field
has an indirect write surface. Per the packet's own rule (line 17) and the row table, a partial chain
selects `O-OPEN`, never a positive row.

---

## 1. The call site — `0x00193EB5`, anchored two independent ways

**The terminal is `loc_00193EB0` / `0x00193EB5` in D3D function `sub_00193D90`.**

Original XBE bytes (`game/default.xbe`, section `D3D` VA `0x18CB40`):

| VA | Bytes | Instruction |
|---|---|---|
| `0x00193EB0` | `8D 4C 24 18` | `lea ecx, [esp + 0x18]` |
| `0x00193EB4` | `51` | `push ecx` |
| **`0x00193EB5`** | **`FF D0`** | **`call eax`** |
| `0x00193EB7` | `8A 54 24 17` | `mov dl, byte ptr [esp + 0x17]` |

Generated/recovered agreement — `src/recomp/recovered/recovered.c:347479-347484`, whose header block
carries `Original: 0x00193D90 - 0x00193EE2 (338 bytes, 98 insns)`:

```c
loc_00193EB0: ;
    ecx = esp + 0x18;
    { uint32_t _icall_esp = g_esp;
    PUSH32(esp, ecx);
    { uint32_t _icall_target = eax; PUSH32(esp, 0x00193EB7u); RECOMP_ICALL_SAFE(_icall_target, _icall_esp); } /* indirect call */
    }
```

**`recovered.c:347517` defines `sub_00193D90`, and `recomp_dispatch.c:8006` maps
`{ 0x00193D90u, (recomp_func_t)sub_00193D90 }`** — the boundary is a *declared recovered function*,
not an inferred one. `config/recovered-functions.json` records it as
`{"start":"0x00193D90","end":"0x00193EE2","section":"D3D","kind":"routine","stack_args":0,"test_unit":"11c1"}`
with the evidence string *"cdecl-calls context+0x1C4 with {1F4,1F0,reason} or skips if null"*.
**This is the `loc_` anchor the packet demands; no byte-scan from an inferred boundary was used.**

### 1.1 The four-register realization is reproduced exactly, from the ABI alone

`ecx − esp = 0x20` at the call is **derived, not assumed**:

```
esp0                        ; entry, after the caller's `push 0x00193E1E` + `ret` (return address in frame)
esp0 - 0x14                 ; sub esp, 0x14            (0x00193D90)
esp0 - 0x20                 ; push ebx, push ebp, push esi, push edi   (4 x 4)
esp0 - 0x24                 ; push eax                 (0x00193E11)
                            ; push edx  -- consumed by `call 0x00193D10` (callee `ret 8`)
                            ; ... 0x00193D10 returns with esp = esp0 - 0x20
                            ; ... `push 0` ... `push ecx` (2 x 4)      (0x00193E52)
esp0 - 0x24 + 4             ; + return address pushed by the generated ICALL sequence
ecx = esp + 0x18 = (esp0 - 0x24) + 0x18 = esp0 - 0x0C
=> ecx - esp = (esp0 - 0x0C) - (esp0 - 0x2C) = 0x20   ✓  matches the recorded ecx=0x007BFFBC / esp=0x007BFF9C
```

**A whole-`.text`/D3D/DSOUND/XPP/… sweep for the signature "`ecx` loaded from `[esp+N]` then
`call reg`" returns exactly FOUR sites, and exactly ONE with `ecx − esp = 0x20`:**

```
D3D lea@00193EB0 call@00193EB5 eax   ecx-esp=0x20      <-- the terminal
```

**The terminal is therefore uniquely the `0x001D5078` call** — `NON-TARGET` is refuted.

### 1.2 The traced 4-cycle maps onto this function exactly

The parent's ICALL trace (strict 4-cycle `FE000190, 00193D90, FE0000B4, 0015F9D0` breaking at
position 3) is reproduced by `sub_00193D90`'s own instruction sequence, in order:

| Cycle pos | Target | Site in `sub_00193D90` |
|---|---|---|
| 0 | `0xFE000190` | `0x00193E5C` `call dword ptr [0x1C4014]` — **the 3-push `KeSetEvent(context+0x1C8, 1, FALSE)`**, per the recovered-functions evidence string |
| 1 | `0x00193D90` | the function itself, entered from the polling loop `0x00196C38` |
| 2 | `0xFE0000B4` | `0x00193CF7` `call dword ptr [0x1C40F0]` inside `sub_00193CC0`, reached via `sub_00193D10` |
| **3** | **`0x001D5078`** | **`0x00193EB5` `call eax`, `eax = MEM32(esi+0x1C4)`** |

---

## 2. The candidate table — **VALIDATED as a table, REFUTED as a code-target table**

The packet's LEAD was *"59 repetitions of `0x001D5078`, ALL stride 8, table beginning guest
`0x001D4BD4`"*. **The repetitions and the stride are confirmed; the boundary is interior.**

`scripts/inspect-jsrf.py find 0x1D5078` returns **exactly 59** hits, `0x001D4BD4 … 0x001D4DA4`,
**every stride 8** — the Session's raw-byte measurement is reproduced exactly.

**But the first hit is interior, exactly as the packet warned.** Reading the `.rdata` region as
`{u32 id, char* name}` pairs:

| VA | id | name |
|---|---|---|
| `0x001D4BC8` | `0x0280` | `0x001D5088` |
| `0x001D4BD0` | `0x0281` | `0x001D5078` |
| `0x001D4BD8` | `0x0282` | `0x001D5078` |
| … | … | … |
| `0x001D4C60` | **`0x0293`** | **`0x001D5078`** |
| … | … | … |
| `0x001D4DA0` | `0x02BB` | `0x001D5078` |
| `0x001D4DA8` | `0xFFFFFFFF` | `0x00000000` ← **terminator** |

**`0x001D4BD4` is not an entry boundary at all — it is the `name` field of the entry at
`0x001D4BD0`.** The array is terminated by an `id = 0xFFFFFFFF` sentinel at `0x001D4DA8`; the entry
`id = 0x0293` — **the value of `edx` in the recorded realization** — is at **`0x001D4C60`**, whose
`name` is `0x001D5078` = `b'djv000_0.adx\x00\x00\x00\x00'`.

**What the entries hold — `loc_`-anchored, from generated expressions:**

```c
/* recomp_0002.c:48985, 48992-48993 — sub_001160D0, loc_00116133 / loc_00116140 */
edx = MEM32(eax * 8 + 0x1D4DB0);                                   /* the id   field of entry eax */
_fa = MEM32(eax * 8 + 0x1D4DB0) ... cmp ... ecx                     /* searched as an ID       */
/* recomp_0002.c:50714-50717 — loc_00116B9C, and 51661-51664 — loc_00117261 */
eax = MEM32(edi * 8 + 0x1D4DB4);                                   /* the name field of entry edi */
PUSH32(esp, eax); ... RECOMP_ABI_CALL(0x0013AEB0u, sub_0013AEB0);  /* consumed as a char*      */
```

**The entries hold `{u32 id, char* filename}` — names, never functions.** `sub_001160D0` is an
id → index lookup (a switch on a class selector, then a linear scan comparing the `id` field);
`sub_00116B74` and `sub_00117240` index by the returned slot and pass the **`name`** field to
`sub_0013AEB0`, which treats it as a path string (`0x0013AEC6` reads `[esi+0x10]`, pushes it and
calls `0x0013C390`). **The array is a filename catalogue: no entry is callable, and nothing in the
generated source loads an entry as a code pointer.**

**Consequence for the terminal:** the failing target is **not** an out-of-range table index that
landed in a function-pointer table. The `.rdata` address arrived in `eax` as a **data value**, and
the 59-hit neighbourhood is a red herring for *this* mechanism — the count 59 is a property of how
many soundtrack ids share the `djv000_0.adx` placeholder, not of the call site.

---

## 3. Back-slice of `eax` — the reaching definition is `[esi+0x1C4]`, and it is guarded

```c
/* recovered.c:347436-347441 */
loc_00193E62: ;
    eax = MEM32(esi + 0x1C4);
    if (TEST_Z(_fa, _fb)) goto loc_00193ECE;   /* je: equal / zero  -> skip the call entirely */
```

**`eax` is not an immediate and not a table load. It is a field of the object in `esi`, tested for
NULL before use** — so the guest believed the field held a valid function pointer. The value
`0x001D5078` passed the NULL test and was dispatched.

`esi` is established at `0x00193D96` (`mov esi, ecx`, `recovered.c:347350`) — **`esi` is the incoming
`ecx`, i.e. the context pointer**, and `ebx = MEM32(esi)` (`recovered.c:347351`) is the device pointer.

**Writer search (whole executable section sweep, normalised, all spellings):**

* `mov dword ptr [reg + 0x1c4], …` — **many sites, none in D3D**, and **none in this object's
  family**; they belong to unrelated `.text` classes (float fields, small enums).
* `call dword ptr [reg + 0x1c4]` — **five** sites (`0x00043FCB`, `0x00044220`, `0x0005F4D5`,
  `0x0010D4CB`, `0x0010D7C3`), **all in unrelated `.text` classes**.
* **Within D3D, `[reg + 0x1C4]` has exactly ONE access in the entire section: the read at
  `0x00193E62`.** *(Corrected per acceptance §4(c): this omits **two `[esp+0x1C4]` STACK-LOCAL hits** at
  `0x18D97D` and `0x18D98F`, which may be linear-disassembly artefacts. **The operative claim — that no
  static write targets the OBJECT field — still holds.**)*

**There is no static write to the field anywhere in the executable.** That is the first unclosed edge,
and it is a genuine one, not a search failure: see §4.

---

## 4. The object, its field, and the alias that identifies it

`ecx`/`esp` bound a **guest stack frame**, not an object; the object is **`esi`**, and `esi` is the
caller-supplied context.

**The context is `device + 0x2268`, proved from the original instructions:**

```asm
; D3D 0x0018CB60 — the device's own accessor
0018CB60  mov  eax, dword ptr [0x19dce0]     ; the D3D device singleton
0018CB65  lea  edx, [eax + 0x2268]           ; <-- the sub-object base
...
; D3D 0x0018E1D0 / 0x0018E1F4 — both pass exactly that base as the context
0018E1D0  lea  ecx, [ebx + 0x2268]
0018E1D6  call 0x194c3f
0018E1F4  lea  ecx, [ebx + 0x2268]
0018E1FA  call 0x194e2d
```

**The decisive arithmetic:**

```
context = device + 0x2268
field   = context + 0x1C4
        = device + 0x2268 + 0x1C4
        = device + 0x242C
```

**And `0x0018CE30` — the only function in the whole XBE that writes `[reg + 0x242C]` — writes exactly
that:**

```c
/* recomp_0004.c — sub_0018CE30, Original: 0x0018CE30 - 0x0018CE43 */
loc_0018CE30: ;
    eax = MEM32(esp + 4);
    ecx = MEM32(0x19DCE0);          /* the device singleton */
    MEM32(ecx + 0x242C) = eax;      /* == context + 0x1C4 */
    esp += 8; return; /* ret 4 */
```

**`[reg + 0x242C]` has exactly ONE access in the entire executable section — this store.**
(`D3D 0x0018CE3A`.) **Nothing in the XBE reads it as data.** A field that is written once and never
read as data, at the exact offset of a field that is *called*, is a **code-target field by
construction.**

### 4.1 The installer's handler argument is the NULL sentinel — and it resolves to `0x15F9D0`

`0x0015F9E0` is the only thing that tail-jumps to `0x0018CE30`:

```c
/* recomp_0003.c:49260-49269 — sub_0015F9E0, Original: 0x0015F9E0 - 0x0015FA06 */
loc_0015F9E0: ;
    eax = MEM32(esp + 4);
    if (TEST_NZ(...)) goto loc_0015F9F6;
loc_0015F9E8: ;
    eax = 0x15F9D0;                      /* <-- the DEFAULT HANDLER */
    MEM32(esp + 4) = eax;
    g_seh_ebp = ebp; sub_0018CE30(); return; /* tail jmp 0x0018CE30 */
```

**`sub_0015F9E0` is called from exactly ONE site in the entire XBE — `.text 0x00123319` — inside
`sub_00012210` (`recomp_0000.c:3645`).** At that call (`recomp_0000.c:3743`) the argument pushed is
**`ebx`, and `ebx = 0`** (`recomp_0000.c:3673`, `ebx = 0; /* xor self */`, with no intervening write).

**So the NULL-substitution path is taken, `0x0015F9D0` is installed into `device+0x242C`, and
`0x15F9D0` is exactly the refcount thunk the trace shows at cycle position 3.** The installer chain is
therefore consistent with the trace end-to-end.

**Independent corroboration of the object's identity:** `0x00193E5C` calls
`dword ptr [0x1C4014]` with `{&context[0x1C8], 1, 0}` — a 3-argument stdcall. By the same alias,
`context + 0x1C8 = device + 0x2430`, which is **exactly the second store in `0x0018CE30`'s
neighbour** (`0x0018CE67 add eax, 0x2430`, `0x0018CE5B mov dword ptr [eax+0x2434], 0`). The recovered-
functions evidence independently names `0x00193D90`'s site as *"stdcall KeSetEvent(context+0x1C8, 1,
FALSE) through IAT 0x001C4014"*. **Two independent derivations of the same object layout agree.**

---

## 5. Why the row is `O-OPEN` — the smallest missing static edge

**The field `device+0x242C` is written by `sub_0018CE30` and by nothing else statically, and the
generated source contains NO caller of `sub_0018CE30` other than the two tail jumps inside
`sub_0015F9E0`.** The remaining question — *why did the fourth iteration observe `0x001D5078` instead
of `0x15F9D0`* — requires the object's **constructor**, which is not present in the translation:

> ### ⚠ THREE CORRECTIONS APPLIED (acceptance review §4, and the Session independently found the first)
>
> **(a) `0x0018CB60` is `rep movsd`, NOT `rep stosd` — it COPIES, it does not zero.** Verified:
> `0018CB88 mov ecx,0xc0` / `0018CB8D mov edi,ebx` / **`0018CB8F rep movsd`**, with **`esi = [esp+0x10]`, a
> caller-supplied source.** **So the original "it zeroes the sub-object" statement and the entire
> "a zeroing constructor does not explain the value" argument are VOID.** **The replacement reason is
> STRONGER:** the context is **populated from caller data**, i.e. **an indirect write surface into the same
> object** — which independently supports `O-OPEN`. **The destination range `[+0x214, +0x514)` does not
> literally cover `+0x1C4`**, so it does not itself write the field, **but it establishes that the object IS
> populated by non-constant data, which is precisely why the chain cannot close statically.** *(The Session
> reached the same correction independently: `0x2268+0x214 = 0x247C`, and the `0x300`-byte copy spans
> `0x247C..0x277C`, which **excludes** the callback field at `0x242C`.)*
>
> **(b) "the constructor/populator … is absent from the generated and recovered translation" is FALSE.**
> **`sub_0018CB60` IS present** — `recomp_0004.c:36665` (*"Original: 0x0018CB60 - 0x0018CBBD"*), declared
> `recovered.c:5336`, called `recovered.c:254405`. **What is genuinely absent is a CALLER of
> `sub_0018CE30`** (only the two `jmp`s, no `call`). **Stating the populator is absent OVER-CLAIMS the gap:
> the gap is absence of a WRITER PATH, not absence of the constructor.**
>
> **(c) "Within D3D, `[reg + 0x1C4]` has exactly ONE access in the entire section" omits two `[esp+0x1C4]`
> stack-local hits** (`0x18D97D`, `0x18D98F`, possibly linear-disassembly artefacts). **The operative claim —
> no write to the OBJECT field — still holds.**
>
> **NET EFFECT ON THE ROW: NONE. `O-OPEN` stands, and the gap is real and independently reproduced — but the
> stated JUSTIFICATION is partly unsound and is corrected above.**

* *(superseded text, retained so the correction is legible)* `0x0018CB60` computes `lea edx, [eax + 0x2268]`
  and then `mov ecx, 0xC0` / `rep stosd` — it **zeroes** the sub-object. A zeroed `[+0x1C4]` would make
  `loc_00193E62`'s NULL test branch to `loc_00193ECE` and **skip the call**, so a zeroing constructor does not
  by itself explain the value.
* The recovered-functions manifest names `sub_00196C0B` as the polling loop and lists its dependencies
  as *"direct dependencies remain unresolved in production"* — the very functions that populate the
  context are the ones the translation does not carry.

**First unclosed edge, stated precisely** *(corrected per acceptance §4(b) — the populator is PRESENT; what is
absent is a WRITER PATH)*:

> **The reaching definition of `MEM32(device + 0x242C)` on the fourth polling iteration is not
> statically determinable, because (a) the field's only static writer is `sub_0018CE30`, (b) its only
> static callers pass the constant `0x15F9D0`, and (c) **the context IS populated from caller-supplied
> data — `sub_0018CB60` is a `rep movsd` COPY from `[esp+0x10]` — so the object has an indirect write
> surface**, and any write from that path is invisible to static analysis.

**A second, independent reason the chain cannot close statically:** `0x0013AEB0` — the consumer of the
`name` field of the very table in §2 — stores into `[esi+8]` (`0x0013AED9 mov dword ptr [esi+8], eax`)
and `[esi+0x14]` (`0x0013AEC6`). Whether the `djv000_0.adx` pointer reaches a code-target field by a
**non-static route** (an indirect/registration write into a context the translation does not model)
therefore cannot be excluded. Per packet line 17, *"a positive row needs an actual terminal witness and
complete, cross-checked static path"* — **the path is not complete.**

**`O-DATA-AS-CALL` is NOT selected**, and the packet's own validity rule forbids it here: *"A broken
call, empty source search, unexercised branch, stale build, sampled loss or non-target cannot select
`O-DATA-AS-CALL`"* (line 22). **The mechanism is strongly suggested — a filename pointer occupying a
code-target field — but "strongly suggested" is exactly what the packet says must not be promoted.**

**`O-ALTERNATE-PATH` is also not selected:** there is **one** call site, **one** field, and **one**
static writer; the packet requires *"verified distinct feasible field/writer paths"*, and the
alternate paths here are *unverified*, not verified-distinct.

---

## 6. Contrastive siblings (scope respected)

`0x3E800000` and `0x41200000` are **not** analysed here and **no family claim** is made. They remain
contrastive per packet line 19. **The retired NULL line was not reopened, and no DR record was cited
as a row input.**

---

## 7. Prohibitions and status

**Static analysis only — the game was not run.** No source file was modified; the only file written is
this record. **No synthetic completion:** the APU trap and `0x80` untouched, no allocation faked, arena
not widened, the NULL call not bypassed, guest error handling not edited. **No `src/recomp/gen/*.c`
edit**; `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE`,
`A4b2-r7`, `A4b2-r8`, `A4b1-r4`, `0xFFFFB3` untouched.** **No scratch file in `scripts/`.** No push.

### Reproduction

```powershell
python -X utf8 scripts/inspect-jsrf.py find 0x1D5078          # 59 hits, stride 8, 0x1D4BD4..0x1D4DA4
python -X utf8 scripts/inspect-jsrf.py disasm 0x193E60 0x193EC0   # 00193EB5  call eax
python -X utf8 scripts/inspect-jsrf.py data 0x1D4C50 0x20         # id 0x293 -> 0x001D5078
python -X utf8 scripts/inspect-jsrf.py disasm 0x18CB60 0x18CB70   # lea edx, [eax + 0x2268]
```
