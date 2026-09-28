# `A2h-rdata-call-target` — stage-1 acceptance review

**Reviewer:** independent stage-1 acceptance reviewer (did not author, did not execute).
**Date:** 2026-09-28.
**Packet:** `docs/packets/a2h-rdata-call-target.md`, frozen r1, 22 lines.
**Verified SHA-256:** `31343C167746872A501A21C6D558F52B897A7CD571E43C9EB016D42341152593` — **matches the
frozen value in the brief**. Class: discovery.
**Execution evidence:** `docs/reviews/a2h-rdata-call-target-execution-evidence.md` (312 lines, Worker
commit `eca26ed`).
**Method of this review:** STATIC only. The game was **not run**. Verification used the original
`game/default.xbe` (raw-byte and capstone disassembly), `game/mygame_analysis.json` section map,
`config/recovered-functions.json`, `src/recomp/gen/`, `src/recomp/recovered/`.

**Disposition: ACCEPT-WITH-CORRECTIONS.** The row (`O-OPEN`) is correct, the load-bearing
identification claims hold, and the packet's prohibitions were respected. Two factual defects in the
evidence record are corrected below; **neither changes the row**, and one of them actually
*strengthens* the `O-OPEN` rationale.

---

## 1. Criterion 1 — artifact exists and is complete

**AGREED.** The record exists (312 lines), names a single row (`O-OPEN`, in the title and §1), and
covers the packet's four static-first steps in dedicated sections:

| Packet step | Evidence section | Covered |
|---|---|---|
| 1 — locate dispatch/caller from declared labels, no byte-scan from inferred boundary | §1 (call site, `loc_00193EB0`, `config/recovered-functions.json`) | yes |
| 2 — validate candidate `.rdata` table boundary, entry width, both field meanings | §2 (59 hits reproduced, boundary shown interior, `{u32 id, char* name}`, sentinel) | yes |
| 3 — back-slice last `eax` definition with original VAs/bytes and generated `loc_` | §3 (`eax = MEM32(esi+0x1C4)`, NULL-guarded, VAs and generated lines given) | yes |
| 4 — bound the object, argument slot, field offset, every feasible reaching write | §4 (object, field, alias, writer search) and §5 (named first unclosed edge) | yes |

The record also carries a reproduction block (§"Reproduction") with concrete commands.

## 2. Criterion 2 — row correctly selected

**AGREED — `O-OPEN` is the right row, and the positive rows are genuinely unreachable.**

* **`O-DATA-AS-CALL`** requires *a complete reaching-definition chain*. The evidence affirmatively
  demonstrates an incomplete chain (§5) and I independently confirmed the incompleteness: the only
  writer of the candidate field, `sub_0018CE30`, has **no `call` to it anywhere in the executable** —
  my `E8/E9` rel32 sweep found only two **`jmp`** sites, `0x0015F9F1` and `0x0015FA01`, both inside
  `sub_0015F9E0`. Its argument on both paths is a constant (`0x15F9D0` or `0`). So the field's only
  static writer cannot produce the observed `0x001D5078`, and the chain is genuinely open. Correctly
  not selected.
* **`O-ALTERNATE-PATH`** requires *verified distinct* feasible paths. The evidence shows one call
  site (confirmed: see §3), one field, one static writer. Nothing verified-distinct. Correctly not
  selected.
* **`O-IDENTITY` / `NON-TARGET`** are not selected and I found no identity defect: the XBE, the
  section map and the translation agree on the site.
* The packet's own rule (line 17/22) puts a partial chain in `O-OPEN`. Applied correctly.

**Is the missing edge REAL, or did the Worker stop short?** — **YES, it is real.** I re-derived it:
`0x00193E62` reads `eax = [esi+0x1C4]` and `0x00193E6A` branches on it, so the field is
caller/context-supplied; the only store to the aliased slot `[ecx+0x242C]` is `0x0018CE3A` inside
`sub_0018CE30`; that function is reachable only by the two tail jumps with constant arguments. The
gap is a property of the code, not of the search. (The Worker's *stated reason* for the gap is partly
wrong — see §4 — but the gap itself is independently reproduced.)

## 3. Central claims — verified one by one

### Call site `0x00193EB5` is `call eax`, and it is anchored to a DECLARED boundary — **CONFIRMED**

Raw-byte regex over every executable section for the sequence `8D 4C 24 N 51 FF Dx`
(`lea ecx,[esp+N]; push ecx; call reg`) returns exactly the four patterns whose first is:

```
D3D  0x00193EB0  8d 4c 24 18 51 ff d0      lea ecx,[esp+0x18]; push ecx; call eax
```

and live disassembly confirms `00193EB0 lea ecx,[esp+0x18]` / `00193EB4 push ecx` /
`00193EB5 call eax` / `00193EB7 mov dl,[esp+0x17]`. **The claimed bytes and operand are exact.**

Boundary: `config/recovered-functions.json` carries
`{"start":"0x00193D90","end":"0x00193EE2","section":"D3D","kind":"routine",...}`;
`src/recomp/recovered/recovered.c:347329` header `Original: 0x00193D90 - 0x00193EE2 (338 bytes, 98
insns)`; `src/recomp/gen/recomp_dispatch.c:8006` maps `{ 0x00193D90u, (recomp_func_t)sub_00193D90 }`.
**Declared, not inferred.** The site is additionally reachable from five distinct `call` sites
(`0x194419`, `0x1944d3`, `0x194ab8`, `0x194f71`, `0x196c38`).

### Uniqueness: 4 sites, exactly one with `ecx−esp = 0x20` — **CONFIRMED (independently reproduced)**

My independent sweep found the same four sites and no others:

| Section | VA | disp |
|---|---|---|
| `.text` | `0x13E269` | `0x10` |
| `.text` | `0x13E350` | `0x04` |
| `.text` | `0x140A62` | `0x14` |
| **D3D** | **`0x193EB0`** | **`0x18`** |

(The `disp32` encoding `8D 8C 24 …` variant returns **zero** sites, so the disp8 sweep is complete.)
The `ecx−esp` arithmetic also checks out from the recorded realization without assumption: with
`ecx = 0x007BFFBC` and `esp = 0x007BFF9C`, `lea` gives `esp_at_lea = 0x7BFFBC − 0x18 = 0x7BFFA4`;
`push ecx` → `0x7BFFA0`; the `call` pushes the return address → `0x7BFF9C`, which **is** the recorded
`esp`. So `ecx − esp = 0x20` and the site is consistent with the archived registers. **This is the
load-bearing uniqueness claim and I reproduce it exactly.**

### Table `{u32 id, char* name}`, `0x001D37C8` … sentinel `id=0xFFFFFFFF` at `0x001D4DA8` — **CONFIRMED**

Reads from the XBE:

```
0x001D37C8: 41 00 00 00 42 00 00 00 43 00 00 00 ...   <- table base, ids 'A','B','C',...
0x001D4BC0: 0000027F 001D5098  00000280 001D5088
0x001D4BD0: 00000281 001D5078  00000282 001D5078     <- 0x001D4BD4 is the NAME field of entry 0x001D4BD0
0x001D4C60: 00000293 001D5078  00000294 001D5078
0x001D4DA0: 000002BB 001D5078  FFFFFFFF 00000000     <- sentinel
0x001D5078: b'djv000_0.adx\x00\x00\x00\x00'
```

The packet's LEAD (`0x001D4BD4`) is indeed **interior** — it is the `name` field of the entry at
`0x001D4BD0`, exactly as the evidence says. The `id=0xFFFFFFFF` sentinel at `0x001D4DA8` is present
and exact. Entry width 8, `{u32 id, char* name}` confirmed.

### `edx=0x293` indexes to entry `0x001D4C60` holding `name=0x001D5078` — **CONFIRMED**

`base + index*8 = 0x001D4C60` → `index = (0x001D4C60 − 0x001D37C8)/8 = 0x1498/8 = 0x293` — **exactly
the recorded `edx = 0x00000293`**. The entry's `id` field is `0x00000293` and its `name` field is
`0x001D5078` = `"djv000_0.adx"`. Arithmetic and entry both check out.

### Object identity: `callback = MEM32(esi+0x1C4)`, `0x2268+0x1C4 = 0x242C` — **CONFIRMED, with one scoping caveat**

* `0x0018CB60`: `mov eax,[0x19DCE0]` … `lea edx,[eax+0x2268]` — **confirmed**, the device-singleton
  base plus `0x2268`.
* `0x0018E1D0` `lea ecx,[ebx+0x2268]` and `0x0018E1F4` `lea ecx,[ebx+0x2268]` — **confirmed**, two
  sites passing that base as `ecx`.
* `0x2268 + 0x1C4 = 0x242C` — **arithmetic correct.**
* `sub_0018CE30` writes `mov dword ptr [ecx+0x242C], eax` at `0x0018CE3A` with
  `ecx = [0x19DCE0]` — **confirmed**; it is the **only** `+0x242C` access in all executable sections
  (my disp32 sweep over `.text`, `D3D`, `DSOUND`, `MMATRIX`, `XGRPH`, `XPP`, `DOLBY` found exactly one).
* `0x2268` also appears in the translation independently: `recovered.c:346506 esi = esi + 0x2268`,
  `recovered.c:346934 ecx = ecx + 0x2268`, `recovered.c:345221` / `345293 eax = MEM32(eax + 0x2268)`.

**Caveat (not a defect, but a limit):** the immediate polling caller `0x00196C38` passes
`mov ecx, edi` — i.e. the value in `edi`, **not** a literal `lea ecx,[ebx+0x2268]`. So the identity
"`esi` at `0x00193D90` **is** `device+0x2268`" is established by the accessor/alias cluster and the
offset arithmetic, **not** proved directly at this call site. The alias is sound; the last hop is
inferred. I note this because the evidence presents the object identity as "closed".

### Installer corroboration: `sub_0015F9E0` has one caller (`0x00123319`) passing `ebx = 0` — **CONFIRMED (minor reservation)**

* `E8/E9` rel32 sweep: `0x0015F9E0` has **exactly one** reference in the whole executable —
  `.text 0x00123319`, `call 0x15f9e0`. **Single caller confirmed.**
* Body confirmed: `mov eax,[esp+4]`; `test eax,eax`; `jne 0x15f9f6`; `mov eax,0x15f9d0`;
  `mov [esp+4],eax`; `jmp 0x18ce30`. The NULL branch installs `0x15F9D0`. **Confirmed.**
* `recomp_0000.c:3743` `PUSH32(esp, 0x0001231Eu); RECOMP_ABI_CALL(0x0015F9E0u, sub_0015F9E0);` inside
  `sub_00012210`, and `recomp_0000.c:3673 ebx = 0; /* xor self */` — **both confirmed**; lines 3738-3742
  only *read* `ebx` (`MEM32(esi+…) = ebx`), so no `ebx` clobber is visible at the push.
  *Reservation:* I did not exhaustively read every line between 3673 and 3743 to exclude an
  intervening `ebx` write; the immediately preceding lines are consistent with `ebx = 0`.
* `0x18CB60` **is** called from two sites (`0x001521BC`, `0x0015221A`).

### `[reg+0x1C4]` has exactly ONE access in the D3D section — **CONFIRMED in substance, imprecise as written**

My disp32 sweep found **three** `+0x1C4` accesses in `D3D`: `0x0018D97D`
(`mov eax,[esp+0x1c4]`), `0x0018D98F` (`mov [esp+0x1c4],eax`), and `0x00193E62`
(`mov eax,[esi+0x1c4]`). The two extras are **`[esp+0x1C4]`** — stack locals, not object fields — and
they decode in a region whose preceding bytes do not disassemble cleanly, so they may be
misalignment artefacts. The **operative** content of the claim — *no write to the object field
`[reg+0x1C4]` exists anywhere in D3D, and `0x00193E62` is its only object-field access* — **holds**.
Separately, the evidence's list of five `call [reg+0x1C4]` sites is exactly right:
`0x43FCB`, `0x44220`, `0x5F4D5`, `0x10D4CB`, `0x10D7C3`, all in `.text`.

---

## 4. Criterion 4 — no over-claiming

The evidence is disciplined about the *row*: it explicitly declines `O-DATA-AS-CALL` and
`O-ALTERNATE-PATH`, says the mechanism is "strongly suggested" but "must not be promoted", and states
that the writer edge does not close. **No mechanism is asserted.** It does not under-claim either:
the object identity is claimed as closed and is (mostly) supported.

**But two statements in the evidence record are factually wrong.** Both are in the *evidence record*,
not in the execution's conclusions:

**(a) `0x0018CB60` does NOT zero the sub-object — it COPYIES into it.** The record (§4.1/§5) states
`0x0018CB60` computes `lea edx,[eax+0x2268]` and then `mov ecx, 0xC0` / **`rep stosd`** — "it
**zeroes** the sub-object", and builds an argument on that ("A zeroed `[+0x1C4]` would make the NULL
test branch … a zeroing constructor does not by itself explain the value"). The actual instruction is:

```
0018CB88 mov ecx, 0xc0
0018CB8D mov edi, ebx
0018CB8F rep movsd dword ptr es:[edi], dword ptr [esi]     <- COPY, not zero-fill
```

with `esi = [esp+0x10]`, the caller-supplied source. So the populator **copies 0xC0 dwords from
caller-supplied memory** into `device+0x2268 + [(idx*3)<<8] + 0x214`. The zeroing argument is
therefore **void**. This is a real correction: it *replaces* one reason for the gap with a stronger
one — the context is populated from caller data, i.e. **an indirect write surface into the same
object**, which independently supports `O-OPEN` and the record's own §5 second reason. Destination
range `[+0x214, +0x514)` does not literally cover `+0x1C4`, so it does not by itself write the field —
but it establishes that the object *is* populated by non-constant data, which is precisely why the
chain cannot close statically. **Net effect on the row: none. The rationale gets stronger.**

**(b) "The constructor/populator … is absent from the generated and recovered translation" is
false.** `sub_0018CB60` **is** present: `src/recomp/gen/recomp_0004.c:36665`
(`Original: 0x0018CB60 - 0x0018CBBD (93 bytes, 29 insns)`), declared at `recovered.c:5336` and
called at `recovered.c:254405`. What is genuinely absent is a **caller of `sub_0018CE30`** (only the
two `jmp`s, no `call`), which I verified. Stating the populator is absent over-claims the gap: the
gap is absence of a *writer path*, not absence of the constructor. Since §5 lists this as one of two
independent reasons, and (a) voids the neighbouring argument, the record's stated justification for
the gap is **partly unsound even though the gap is real**.

**(c) Minor:** "Within D3D, `[reg + 0x1C4]` has exactly ONE access in the entire section" omits the
two `[esp+0x1C4]` stack-local hits (see §3).

None of (a)–(c) promotes a mechanism or changes the row. They are evidence-record defects.

## 5. Criterion 5 — scope discipline

**Respected.**

* Only the `0x001D5078` terminal is analysed; a dedicated §6 states the float-bit siblings
  `0x3E800000` and `0x41200000` are **not** analysed and no family claim is made — contrastive only.
* §6 states explicitly: "The retired NULL line was not reopened, and no DR record was cited as a row
  input." The record contains no DR-record citation and no NULL-line instrumentation.
* No `PIO_FREE`, `A4b2-r7`, `A4b2-r8`, `A4b1-r4` or `0xFFFFB3` work appears anywhere in the record.
* No instrumentation was proposed or used; §7 states static-only.

## 6. Criterion 6 — no synthetic completion, no source edits

**Confirmed.**

* `git show --stat eca26ed` → **`1 file changed, 312 insertions(+)`**, the single file
  `docs/reviews/a2h-rdata-call-target-execution-evidence.md`. **No source file, no generated file, no
  config file was touched by the execution.**
* No `src/recomp/gen/*.c` edit (confirms the packet's prohibition).
* No scratch helper was left in `scripts/` (my own review sweeps were written to `logs/`, which is
  gitignored, and are not committed).
* The APU trap / `0x80`, allocation, arena, NULL-call bypass and guest error handling were not
  touched — consistent with a documentation-only commit, which cannot have modified
  `RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, ...)` or any of the named gate sites.
* No push.

## 7. Evidence outrunning its claims

None found. The evidence does not claim less than it proved in any direction that would change the
row; if anything it claims *more* than it proved in the two spots listed in §4. The one place where it
leans past its proof is calling the object identity "closed" while the final hop (poller passes
`ecx = edi`, not a literal `device+0x2268`) is inferred — see §3.

## 8. Blocking

**NONE.** The corrections in §4 are record-accuracy issues on a discovery-class packet whose row is
unchanged by them. I recommend the record be amended for `rep movsd` and the presence of
`sub_0018CB60`, and the row left at `O-OPEN`.

## 9. Uncertain / cannot verify

1. **The final identity hop** — that `esi` at `0x00193D90` is literally `device+0x2268` at runtime.
   The poller `0x00196C38` passes `mov ecx, edi`; I did not trace `edi` back to the device singleton.
   The alias arithmetic is right; the binding at this call site is inferred.
2. **`ebx = 0` at the `0x00123319` call** — `recomp_0000.c:3673` sets it and `3743` pushes it, and the
   five preceding lines only read it, but I did not exhaustively read lines 3674-3742.
3. **`0x15F9D0` is "the refcount thunk"** — not disassembled; the record's characterisation of that
   target is unverified by me.
4. **§2's claim that no generated source loads a table entry as a code pointer** — I verified the
   table structure and the `{u32 id, char* name}` interpretation, but did not re-derive every
   generated consumer cited (`sub_001160D0`, `sub_00116B74`, `sub_00117240`, `sub_0013AEB0`).
5. **The two `D3D [esp+0x1C4]` hits** (`0x18D97D`, `0x18D98F`) — possibly linear-disassembly
   artefacts; unresolved.

## 10. Reviewer's own provenance

Verification commands used (all read-only):

```powershell
(Get-FileHash -Algorithm SHA256 docs\packets\a2h-rdata-call-target.md).Hash
git show --stat eca26ed
python -X utf8 scripts/inspect-jsrf.py disasm 0x193E60 0x193EC0
python -X utf8 scripts/inspect-jsrf.py disasm 0x18CB60 0x18CB70
python -X utf8 scripts/inspect-jsrf.py disasm 0x18CE30 0x18CE48
python -X utf8 scripts/inspect-jsrf.py disasm 0x15F9E0 0x15FA10
python -X utf8 scripts/inspect-jsrf.py disasm 0x18E1D0 0x18E200
python -X utf8 scripts/inspect-jsrf.py disasm 0x196C20 0x196C40
python -X utf8 scripts/inspect-jsrf.py data 0x1D4C50 0x20
python -X utf8 scripts/inspect-jsrf.py data 0x1D4DA0 0x18
python -X utf8 scripts/inspect-jsrf.py data 0x1D4BC0 0x20
# plus capstone byte-pattern / disp32 sweeps over .text, D3D, DSOUND, MMATRIX, XGRPH, XPP, DOLBY
```

**The game was not run.** No source file was modified by this review; the only file written is this
record. No push.
