# `A2h-callback-slot-writer-trace-r1` — independent stage-1 acceptance review

**Reviewer:** independent stage-1 reviewer (did not author the evidence, did not execute it).
**Method:** static only. No game run, no build, no source edit. Scratch tooling in `%TEMP%`
(`a2h_rev_scan.py`, `a2h_rev_cap.py`, `a2h_rev2.py`, `a2h_rev3.py`), plus the packet's own
`python -X utf8 scripts/inspect-jsrf.py disasm`.
**Under review:** `docs/reviews/a2h-callback-slot-writer-trace-evidence.md` (Worker commit
`cb2715b`), session verification `fc0b389`, packet `docs/packets/a2h-callback-slot-writer-trace.md`
(SHA-256 `C5FD9CAA…CD32A76`).
**Vocabulary:** `software_device = 0x0019B200`, `aperture = 0xFD000000`.

**Disposition: ACCEPT-WITH-CORRECTIONS.** The row selection (`O-OPEN`) survives, and the two
load-bearing results — one direct store, and the exact arithmetic `4×0x810 + 0x3EC = 0x242C` —
are confirmed. Two defects are falsified: the shift-check (Worker wrong, Session right) and the
`ebp` provenance/range derivation plus one of the four Claim-8 exclusions.

---

## What I verified myself, and how

Disassembly was produced two independent ways: (a) the packet's own
`inspect-jsrf.py disasm <start> <end>`, and (b) a capstone `CS_MODE_32` linear sweep that decodes
**each section from its own start** and restarts after an invalid byte. Where the two agreed I
treated the result as sound. This matters: my own `disasm 0x192115 0x192140` returned
`dec dword ptr [ebx + 0x19ded815]` / `retf 0x7f7f` — a **misaligned listing** — while the
section-anchored sweep at the same address returned `mov esi,[esp+8]` / `or edx,0x7f7f` /
`add esi,0x2268` / `mov ecx,esi` / `call 0x194add`. **The instrumentation warning is real.**

---

## Claim-by-claim

### Claim 1 — exactly ONE store covers `software_device+0x242C`: **CONFIRMED**

```text
0018CE30 8b442404   mov eax, dword ptr [esp + 4]
0018CE34 8b0de01900 mov ecx, dword ptr [0x19dce0]
0018CE3A 89812c240000 mov dword ptr [ecx + 0x242c], eax   <-- THE ONLY STORE
0018CE40 c20400     ret 4
```

`ecx = MEM32(0x19DCE0)` two instructions earlier, no intervening write. My own image-wide sweep
over every X-flagged section, filtering write-access memory operands on `disp == 0x242C`, returned
**exactly one**: `0x0018CE3A` in `D3D`. Base reaching definition complete.

### Claim 2 — raw-byte backstop, 1 occurrence: **CONFIRMED**

Scanning every byte offset of the whole file for `2C 24 00 00`: **1 hit**, file `0x17D2FC` →
VA `0x0018CE3C` (section `D3D`), context `190089812c240000c2040090`. Matches the Worker exactly.
This is genuinely alignment-independent and it agrees with the instruction-level result.

### Claim 3 — `sub_0018CE30` has two image-wide references: **CONFIRMED**

A rel32 (`E8`/`E9`) scan across all sections for target `0x0018CE30` returned exactly two, both
`E9`:

| VA | bytes | section |
|---|---|---|
| `0x0015F9F1` | `e93ad40200` | `.text` |
| `0x0015FA01` | `e92ad40200` | `.text` |

The installer body also reproduces exactly (`mov eax,0x15f9d0` on the NULL path at `0x0015F9E8`).

### Claim 4 — single caller `0x00012319`, `ebx = 0`: **CONFIRMED**

rel32 scan for `0x0015F9E0`: **exactly one**, `0x00012319` (`e8c2d61400`, `.text`).
Backward slice: `0x0001224F xor ebx,ebx` (`33db`) is the nearest preceding definition. I
enumerated every instruction in `[0x0001224F, 0x0001231C)` that mentions `ebx` and checked
write-access on the operand: **26 mentions, zero writes to `ebx`** — all are `mov [mem], ebx`
stores or `cmp`/`push` reads. `ebx = 0` at the call. NULL path → installs `0x0015F9D0`.

### Claim 5 — `0x19DCE0` written from two sites: **CONFIRMED**

```text
0018CE15 a3e0dc1900             mov dword ptr [0x19dce0], eax
0018E4A0 c705e0dc190000b21900   mov dword ptr [0x19dce0], 0x19b200
```

My image-wide filter on absolute `disp == 0x19DCE0` write operands returned exactly these two,
both in `D3D`.

### Claim 6 — the OPEN computed store: **CONFIRMED in part, REFUTED in part**

**Confirmed — the instruction and the arithmetic.** Section-anchored decode:

```text
00199DB4 8b35e0dc1900   mov esi, dword ptr [0x19dce0]      ; esi = software_device
00199F45 8984aeec030000 mov dword ptr [esi + ebp*4 + 0x3ec], eax   ; <-- THE OPEN STORE
```

**`4 × 0x810 + 0x3EC = 0x242C` exactly — CONFIRMED.** `ebp` needed `= (0x242C − 0x3EC)/4 =
0x810 = 2064`. This is the packet's whole result and it holds.

**Confirmed — the base.** I checked a concern the Worker did not surface: `esi` is **reloaded
inside the loop** at `0x00199DEF mov esi, dword ptr [esp + 0x14]`. That reload is not a
counter-example. Recomputing the frame (`sub esp,0x10` then four pushes → `esp = E−0x20`), the
slot `[esp+0x14]` is `E−0xC`, a **local**, and it is written once at
`0x00199DC9 mov dword ptr [esp + 8], esi` (`esp` there `= E−0x14`, so `E−0xC` — the same local)
with `esi = MEM32(0x19DCE0)`. **The base is `software_device` on every iteration. Reaching
definition intact.** The Worker's conclusion is right; its justification was silent on the reload.

**REFUTED — `ebp`'s provenance and therefore the range.** The Worker states `ebp = arg3`, from
`0x00199DD9 mov ebp, dword ptr [esp + 0x20]`, and derives `ebp ∈ [arg3, 2·arg3 − 1]`. Two pushes
intervene that the Worker's account does not net out. With entry `esp = E` (so `arg1 = E+4`,
`arg2 = E+8`, `arg3 = E+0xC`):

| Insn | `esp` at insn | `[esp+0x20]` resolves to |
|---|---|---|
| `0x00199DC3 mov eax,[esp+0x20]` | `E−0x14` (after `sub esp,0x10`, `push esi`) | `E+0xC` = **arg3** |
| `0x00199DD9 mov ebp,[esp+0x20]` | `E−0x1C` (after additionally `push ebx`, `push ebp`) | `E+4` = **arg1** |

Cross-check that the frame is right: `0x00199DDE mov edi,[esp+0x28]` at `esp = E−0x20` gives
`E+8` = arg2, consistent. And the trip count is separately `arg3`: `eax` from `0x00199DC3` is
stored to `[esp+0x18]` at `0x00199DE9` and `dec eax` / `jne` at `0x0019A01E`/`0x0019A027`, with
`test eax,eax; jbe 0x19a030` at `0x00199DC7`/`0x00199DD1` guarding `arg3 == 0`.

**So `ebp` is initialised from arg1, not arg3, and the reachable range is
`ebp ∈ [arg1, arg1 + arg3 − 1]` — not `[arg3, 2·arg3 − 1]`.** The slot condition
`4·ebp + 0x3EC = 0x242C` is unchanged (`ebp = 0x810` is still the one hitting index), but the
condition on the *arguments* is "arg1 ≤ 2064 ≤ arg1 + arg3 − 1", not the Worker's
"arg3 ∈ [1033, 2064]". **The Worker names the thunk's THIRD argument as the open edge; on the
bytes the open edge is the thunk's FIRST argument (with arg3 as a co-condition).** A successor
resolving the wrong argument would close nothing. This is a defect in the evidence record, not
in the arithmetic.

### Claim 7 — the forwarding thunk: **CONFIRMED**

```text
00153790 mov eax, dword ptr [esp + 0x10]
00153794 mov ecx, dword ptr [esp + 0xc]
00153798 mov edx, dword ptr [esp + 8]
0015379C push eax
0015379D push ecx
0015379E push edx
0015379F call 0x199db0     e80c660400
001537A4 xor eax, eax
001537A6 ret 0x10
```

rel32 scan for `0x00199DB0`: **exactly one** reference image-wide, `0x0015379F` (`.text`).
Raw-dword scan for the little-endian value `0x00199DB0`: **0 occurrences** (no vtable/dispatch
entry). Correct — and `push` order confirms callee `arg1` = thunk `[esp+8]`.

### Claim 8 — the four exclusions: **REFUTED as stated (one exclusion is arithmetically false)**

I evaluated the exact slot condition `base + disp + 4·idx = 0x242C` for each:

| VA | disp | `4·idx` needed | `idx` | Worker's stated ground | My verdict |
|---|---|---|---|---|---|
| `0x0018CBAE` | `0x814`, base = context (`+0x2268`) | negative (`0x2268+0x814 > 0x242C`) | — | index ∈ {0,1} | **excluded, correct** |
| `0x0018DF59` | `0xA78`, base = device | `0x19B4` | **`0x66D` = 1645** | "**not divisible by 4**" | **FALSE — excluded improperly** |
| `0x0018DF87` | `0xC`, base = device | `0x2420` | `0x908` = 2312 | "writes the constant `0xffffffff`" | **excluded, but on the value ground only** |
| `0x001913F3` | `0x0`, base = device | `0x242C` | `0x90B` = 2315 | index ∈ [0x15,0x54] | **excluded, correct** |

**`0x19B4 = 6580 = 4 × 1645`. It IS divisible by 4.** The Worker's parenthetical
"(`4*esi = 0x19B4` (not divisible by 4))" is flatly wrong, and the prose that follows it
("the displacement here is the hardcoded literal `0xa78`, which is far below `0x242C`") is not a
proof — a displacement below the slot is exactly what a positive index compensates for.

This matters, because `0x0018DF59` is **not** a bounded store:

```text
0018DF10 sub esp, 8
0018DF15 mov esi, dword ptr [esp + 0x14]   ; esp = E-0x10 -> E+4 = arg1: UNBOUNDED
0018DF1A mov edi, dword ptr [0x19dce0]     ; edi = software_device
0018DF53 mov ebx, dword ptr [esp + 0x1c]   ; value written = arbitrary caller word
0018DF59 mov dword ptr [edi + esi*4 + 0xa78], ebx   899cb7780a0000
```

Base is `software_device`, index is an unbounded caller argument, and the stored value `ebx` is an
arbitrary caller word. **At `arg1 = 1645` this store writes an arbitrary value straight into
`software_device+0x242C`.** That is a **second genuinely open device-based computed store**, and
the Worker excluded it. The evidence's headline "the computed-store class is closed to a single
unbounded index on a single store" is therefore not supported: there are at least two
(`0x0018DF59` and `0x00199F45`), plus `0x0018DF87` which reaches the slot arithmetically but can
only ever write `0xffffffff`.

Note `0x0018DF87`'s exclusion survives on a sound ground (constant `0xffffffff` cannot be
`0x001D5078`) — but not on the ground the Worker gave for it.

### Claim 9 — `add ecx,0x2268` then `mov [eax+0xc], ecx`: **CONFIRMED**

```text
0019255E mov ecx, dword ptr [esp + 8]
00192562 add ecx, 0x2268        81c668220000
00192568 mov dword ptr [eax + 0xc], ecx   89480c
```

Confirmed on a section-anchored decode from `0x00192100`. The Worker's caveat that this creates a
context alias invisible to a `0x2268` scan is sound and is correctly carried as residual.

### Claim 10 — 76 writes to `[reg+0x1C4]`, none in D3D: **CANNOT VERIFY the count; substance supported**

I could not reproduce **76**. My write-access filter on memory operands with `disp == 0x1C4`
returned **1** (`0x0018D98F mov dword ptr [esp + 0x1c4], eax`, `D3D`, stack-relative); a
mnemonic-position heuristic crashed before completing. The count depends on the Worker's own
definition of "write", which I did not reconstruct. **The count is unverified.**

The **substantive** half I can support independently: my raw byte scan for `68 22 00 00`
returned **15 occurrences, all in `D3D`** (the Worker reports 13 — a minor discrepancy in
grouping, since `0x0019867F/ED/00` are listed as one row). Every site I decoded is a **read**
(`mov reg,[reg+0x2268]`), a `lea`, or an `add reg,0x2268` forming the alias — **none is a
`[context+0x1C4]` write**. So "no `[context+0x1C4]` writer traces to the context" is supported;
the number 76 is not.

### Claim 11 — C-5 ledger: **CONFIRMED**

- Caller of `sub_00194ADD`: rel32 scan → **exactly one**, `0x00192135` (`e8a3290000`, `D3D`).
- `KeInitializeDpc`: `0x00194AF4 call dword ptr [0x1c4020]`, preceded by
  `push esi` / `push 0x194480` / `lea eax,[esi+0x84]` / `push eax` — reproduced exactly.
- `KeInsertQueueDpc`: I did not locate one either. **UNLOCATED** stands.

**C-5 separation: RESPECTED.** The ledger is recorded in its own section, explicitly labelled
"not this question", and no inference about the slot is drawn from it.

---

## The shift-check disagreement: **the SESSION is right; the WORKER mis-computed it**

From the original XBE at `0x001D5078` (not a dump), the bytes are confirmed:

```text
0x001D5078: 646a763030305f302e61647800000000   "djv000_0.adx"
```

My arithmetic:

```text
v0 = 0x30766A64        (dword at 0x001D5078)   -- required printed value ✔
v1 = 0x3030766A        (dword at 0x001D5079)   -- required printed value ✔
(v1 & 0x00FFFFFF) = 0x0030766A
(v0 >> 8)         = 0x0030766A
(v1 & 0x00FFFFFF) == (v0 >> 8)  ->  True
```

**The check PASSES.** The Worker reported `0x00766A` for `v1 & 0x00FFFFFF`; that is wrong — it
dropped the `0x30` byte. `0x3030766A & 0x00FFFFFF` keeps the low six hex digits, `0x30766A`, which
is exactly `0x30766A64 >> 8`. There is no "3-byte window" subtlety to appeal to: the mandated
expression holds exactly as written, at 4-byte granularity, from the known string.

The Worker's rationalisation ("the failure is informative rather than a defect … the binding form
is exactly right for a 3-byte window … the high byte differs by construction") is not a
description of the bytes — it is an **ex post explanation of its own arithmetic error**, and it
concludes that a binding control "PASSED on the binding criterion" while simultaneously reporting
that the binding expression failed. The packet makes the shift agreement a gate; a reviewer
reading the evidence as written would believe a mandated control failed when it did not. This is a
defect in the **evidence record**. The Session's "by construction" determination is correct.

---

## Row: `O-OPEN` is **CORRECT**

The packet requires a **COMPLETE instruction-anchored reaching-definition chain** from a concrete
write to the slot through the fourth-iteration read of `0x001D5078`. What exists: a proven direct
store that installs `0x0015F9D0`, and computed stores whose indices are unresolved. No chain to
`0x001D5078` is established, and no temporal order is established. `O-DATA-AS-CALL` is
unavailable. **The Worker did not stop short — `O-OPEN` is right**, and my Claim-8 finding
(an additional open store) makes it *more* clearly right, not less.

## Controls

| Control | Worker | My finding |
|---|---|---|
| `VA=0x001D5078` prints `30766A64` | ✅ | ✅ confirmed from XBE bytes |
| `VA+1=0x001D5079` prints `3030766A` | ✅ | ✅ confirmed |
| `int(text,16)` parsing | claimed | plausible — both required values are correct, which is inconsistent with the double-reversal the packet warns about |
| Shift agreement | ❌ **reported FAIL — wrong** | ✅ **PASSES** |
| Per-run gate run for the run read | `check-dump-mapping.py` on one run, PASS | not re-run by me (no run executed) |
| 23/24 census avoided | claimed avoided | consistent with the record |

**Gap:** the shift-check is the packet's discriminating control and the Worker recorded a false
FAIL for it.

## Scope and source: **RESPECTED / UNCHANGED**

- `git show --stat cb2715b` → **1 file, 663 insertions**, `docs/reviews/…evidence.md` only.
- `git show --stat fc0b389` → **1 file, 136 insertions**, `docs/reviews/…session-verification.md` only.
- **No source edits in either commit.** No game run by me. No DR record cited by the Worker.
  Float-bit siblings are contrastive only; the retired NULL line is not reopened.
  `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`, `PIO_FREE`, `A4b2-r7/r8`, `A4b1-r4`
  and `0xFFFFB3` are untouched by both commits.

## Over-claiming: **NONE**

The `0x001D5078` filename finding is explicitly fenced: *"Recorded as shape only; not asserted as
the mechanism, and not used to select `O-DATA-AS-CALL`."* It is repeated as residual item 6 with
the same fence. It does not creep into any mechanism claim, and it is not used to select the row.
The `.rdata`/`.data` `X`-flag observation is used only to justify widening the sweep — correctly.

## The instrumentation warning: **REAL, and it affects nothing cited**

Independently confirmed: `inspect-jsrf.py disasm 0x192115 0x192140` returned
`dec dword ptr [ebx + 0x19ded815]` / `retf 0x7f7f`, while the section-anchored decode returns
`mov esi,[esp+8]` / `or edx,0x7f7f` / `add esi,0x2268` / `call 0x194add`. The warning is real and
worth carrying. **It does not undermine any cited claim**: every claim I could verify I verified
by section-anchored decode, by raw byte scan, or by both agreeing. The `.rdata`/`.data` `X` flags
are confirmed from `mygame_analysis.json` and were correctly swept.

---

## Evidence outrunning claims

1. **Claim 8 / `0x0018DF59` is excluded on a false arithmetic assertion.** `0x19B4` is divisible
   by 4 (`= 4 × 1645`). The store has an unbounded caller-argument index, a
   `software_device` base and an arbitrary stored value — it is a **second open store**, and the
   "closed to a single store" claim and the "smallest uncovered edge is `0x00199F45` alone"
   naming are both unsupported.
2. **Claim 6 / `ebp` derives from arg1, not arg3.** Range is `[arg1, arg1 + arg3 − 1]`. The named
   open edge points at the wrong argument.
3. **The shift-check is recorded as FAIL when it PASSES** — a mandated control recorded as failed.
4. **Claim 10's "76" is unverified** by my method; the substantive "none traces to the context"
   is supported.
5. **`esi` reload at `0x00199DEF`** is unaddressed by the Worker's reaching-definition argument
   (the conclusion still holds — via the `[esp+0x14]` local written at `0x00199DC9` — but the
   chain as written is incomplete).
6. Minor: my `0x2268` raw scan returns **15** occurrences; the Worker tabulates **13** sites
   (grouping three as one row). Both are "all in `D3D`".

## Blocking

**NONE.** No correction here changes the row. `O-OPEN` stands, and the packet's result — one
direct store plus the `4×0x810 + 0x3EC = 0x242C` arithmetic — is intact. The corrections change
*which* edges a successor must resolve, and they should be carried into the successor packet.

## Uncertain / cannot verify

- The exact count **76** for `[reg+0x1C4]` writes (method-dependent).
- Whether `arg1` of the thunk `sub_00153790` can actually reach 1645 or 2064 at run time — the
  Worker did not resolve it and neither did I; no run was executed.
- `KeInsertQueueDpc` — not located; deferred by the packet.
- Generic range proof for the 1331 `rep` sites (Worker lists this as residual; I did not attempt
  it either).
- The per-run dump gate was not re-executed by me; I read the original XBE rather than any dump.
