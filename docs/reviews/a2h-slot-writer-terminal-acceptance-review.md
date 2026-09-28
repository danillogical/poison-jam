# Acceptance review — `a2h-slot-writer-terminal` (`O-DATA-AS-CALL`)

**Reviewer:** independent stage-1 acceptance reviewer (did not author the evidence, did not
execute the packet). **Static analysis only — the game was not run.**
**Packet reviewed:** `docs/packets/a2h-slot-writer-terminal.md` (r1, frozen
`116884E8D474A0238096ACD43111E089E5A1A89C87F861E9FCBA953B95E05E9F`, 15 lines).
**Evidence:** `docs/reviews/a2h-slot-writer-terminal-evidence.md` (commit `f63dc53`).
**Session verification:** `docs/reviews/a2h-slot-writer-terminal-session-verification.md`
(commit `0becc28`).

**Verdict up front: the writer *site* is real and byte-verified; the row is not.**
The load-bearing link — **that the store at `0x00199F45` ever writes index `2064`** — is
asserted in the record and is **not established by anything in it**. I could not reproduce
a single line of argument for it, and the instruction I decoded shows the record's own
account of the loop is incomplete in a way that bears directly on the claim. Under the
packet's own rule ("failure … unexercised/partial evidence or new gap all mean `O-OPEN` +
STOP"), the row must be **`O-OPEN`**, not `O-DATA-AS-CALL`.

---

## 1. What I verified myself (original XBE, `scripts/inspect-jsrf.py`)

| # | Claim | My result | Basis |
|---|---|---|---|
| 1 | `0x00199F45` = `89 84 AE EC 03 00 00` = `mov [esi+ebp*4+0x3ec], eax` | **CONFIRMED** | `data 0x00199F45 7` → `89 84 ae ec 03 00 00`; `disasm 0x00199F00..0x00199F60` prints `mov dword ptr [esi + ebp*4 + 0x3ec], eax` at `0x00199F45` |
| 2 | `esi = MEM32(0x19DCE0)` at `0x00199DB4` | **CONFIRMED** | `disasm 0x00199DB0..` → `00199DB4 mov esi, dword ptr [0x19dce0]`; invariant preserved by `0x00199DEF mov esi,[esp+0x14]` (that slot is `esi`'s own spill at `0x00199DC9`) |
| 3 | `ebp = ARG1` at `0x00199DD9` | **CONFIRMED** | see §2 below |
| 4 | `0x3EC + 2064*4 = 0x242C` | **CONFIRMED as arithmetic** | `0x3EC + 0x2040 = 0x242C`. Correct — but it is arithmetic *about an unestablished index* |
| 5 | **ARG1 = 2064 REACHABLE** | **CANNOT VERIFY — and the record offers no argument** | see §3 (the crux) |
| 6 | packed colour word, `(Q(edi+8)<<24)\|(Q(edi-4)<<16)\|(Q(edi+0)<<8)\|Q(edi+4)` | **CONFIRMED as the pack formula**; the *"colour"* / *"not an address"* reading is **NOT established** | see §4 |
| 7 | encoding difference `89 81 2C 24 00 00` vs `89 84 AE EC 03 00 00`; no `2c240000` in the writer | **CONFIRMED** | `data 0x0018CE3A 6` → `89 81 2c 24 00 00`; writer bytes contain no `2c 24 00 00` |
| 8 | GAP B: ARG1 at `0x00012319` is 0 (`xor ebx,ebx` at `0x0001224F`, pushed at `0x000122EA`) | **CONFIRMED** | decode anchored at declared `0x00012210`; every `ebx` appearance in `0x00012210..0x00012330` is `push ebx` / `xor ebx,ebx` / `cmp eax,ebx` / `mov [..], ebx` — **no reassignment** |
| 9 | `0x0015F9E0` substitutes `0x0015F9D0` on the zero path | **CONFIRMED** (semantics; VAs in the record are off by 1–2, see §6) | `0015F9E0 mov eax,[esp+4]` / `test` / `jne 0x15f9f6` / `mov eax,0x15f9d0` / `mov [esp+4],eax` / `jmp 0x18ce30` |
| 10 | chain install → write → read → call | **PARTIAL**: three of four sites byte-verified; the **write→read** link is the one that is unverified | see §5 |
| 11 | four identical `[FE000190, 00193D90, FE0000B4, 0015F9D0]` groups then `[15] 0x001D5078` | **CONFIRMED** | I read `logs/runs/20260928-020456-595-a2h-within-run-on-3/jsrf_run.log` directly: `[0..3]`, `[4..7]`, `[8..11]` are identical, `[12..15] = FE000190 / 00193D90 / FE0000B4 / 001D5078` |
| 12 | `+0x3C8` refutation: vtables `0x001CB040` vs `0x001CE478` | **CONFIRMED (the load-bearing part)**; the `jle` detail **CANNOT VERIFY** | `data 0x00060CF2` → bytes `C7 03 40 B0 1C 00` = `mov dword ptr [ebx], 0x1cb040`; `data 0x000D45D4` → `78 E4 1C 00` = low bytes of `0x001CE478` |

**Constants:** `0x1C4CCC = 437F0000` ✓, `0x1C4550 = 3F000000` ✓, `0x1E133C = 00153790` ✓
(vtable `0x001E1270` entry 51). `0x000D4684` bytes `C7 86 C8 03 00 00 E0 7D 25 00` ✓.

---

## 2. Claim 3 — `ebp = ARG1`: CONFIRMED, and the frame arithmetic matters

`ret 0xc` ⇒ 3 args. At entry `esp = E`, args at `E+4`, `E+8`, `E+0xC`.

```text
00199DB0 sub  esp, 0x10        esp = E-0x10
00199DB3 push esi              esp = E-0x14
00199DC3 mov  eax, [esp+0x20]  -> E+0xC  = ARG3     <-- TRIP COUNT
00199DD7 push ebx ; 00199DD8 push ebp   esp = E-0x1C
00199DD9 mov  ebp, [esp+0x20]  -> E+4    = ARG1     <-- confirmed
00199DDD push edi              esp = E-0x20
00199DDE mov  edi, [esp+0x28]  -> E+8    = ARG2 (float array)
00199DE9 mov  [esp+0x18], eax  trip count spilled
0019A016 mov  eax, [esp+0x18] ; 0019A01E dec eax ; 0019A01F jne 0x199def
0019A01D inc  ebp
```

So `ebp = ARG1` is **CONFIRMED** and the record's parenthetical is right. **But the same
displacement `0x20` at `0x00199DC3` reads ARG3, and ARG3 is the loop trip count.** The
record never mentions this. It is not a cosmetic omission — it is the crux (§3).

---

## 3. THE CRUX — Claim 5, "ARG1 = 2064 is reachable": NOT ESTABLISHED

**What the store actually visits.** From the bytes above, per outer iteration `k`:

```text
address written = software_device + (ARG1 + k)*4 + 0x3EC    for k = 0 .. ARG3-1
```

with a guard: `0x00199DC7 test eax,eax` / `0x00199DD1 jbe 0x19a030` — **ARG3 == 0 skips the
loop entirely.** So index `2064` is written **iff `ARG1 <= 2064 < ARG1 + ARG3`**. Two
unknowns, and the record supplies neither.

**What the record supplies instead.** I grepped the whole evidence file: `2064` occurs on
exactly four lines (13, 43, 284, 299), **all of them restatements of the assertion**
("`ebp = ARG1 = 2064`", "`+ 2064 * 4`", "`2064-element colour write`"). There is no
caller-side witness, no `0x810` provenance, no ARG3 value, no bounds argument. **The
assertion is the argument.**

**The reversal is therefore unexplained, not reconciled.** The packet's established
disposition (line 6) is: *"EDGE 1 `0x00199F45` takes ARG1 three-deep, but `2064` not
established reachable (three reachable `0x810` immediate uses, none an argument)."* The new
execution asserts the opposite while adding **no new witness of any kind**. Nothing
changed between the two states except the assertion. That is a reversal without evidence,
and it is exactly the thing this review was asked to falsify.

**The record is also internally inconsistent about what "2064" means here.** Line 299 calls
it a *"2064-element colour write"*, but if `ARG1 = 2064` the loop starts **at** index 2064
and writes `ARG3` elements *upward* from it — not 2064 elements reaching it. These two
readings cannot both be right, and neither is sourced.

**Two further things the record does not say, which a reader needs:**

1. **`0x242C` is exactly one element past the end of a 2064-element array.** Indices
   `0..2063` span `0x3EC .. 0x2428`. So if `ARG1 = 0`, reaching the slot requires
   `ARG3 >= 2065` — i.e. a **one-past-the-end write**, not a palette write that happens to
   land on a callback. If instead `ARG1 = 2064`, the slot is the *first* element written,
   which makes `0x242C` an ordinary member of the array and the "callback slot" framing
   strained. **These are materially different pictures and the record does not choose
   between them — it cannot, because it never measured ARG1 or ARG3.**
2. **The store writes a contiguous run of dwords, not one slot.** Whatever ARG1/ARG3 are,
   the loop also overwrites everything from `0x3EC + ARG1*4` up to `0x3EC +
   (ARG1+ARG3-1)*4`. The record's "the ONE slot" framing silently drops this.

**Verdict: CANNOT VERIFY, and on the packet's own terms this alone forces `O-OPEN`.**
The index link in a "COMPLETE instruction-anchored reaching-definition chain" is missing.

---

## 4. Claim 6 — the pack formula is right; "packed colour word" over-reads it

**The formula is exactly right — I re-derived it instruction by instruction:**

```text
00199E7A call 0x192a80 -> al = Q(edi-4) ; 00199E81 mov [esp+0x2c], al
00199EC1 call 0x192a80 -> al = Q(edi+0) ; 00199EC9 mov [esp+0x28], al
00199F09 call 0x192a80 -> al = Q(edi+4) ; 00199F19 mov bl, al
00199F24 call 0x192a80 -> al = Q(edi+8)
00199F30 mov ch, al                 ; bits 8-15  = Q(edi+8)
00199F37 mov cl, [esp+0x2c]         ; bits 0-7   = Q(edi-4)
00199F3B shl ecx, 8 ; 00199F3E or ecx, edx (edx = [esp+0x28] = Q(edi+0))
00199F40 shl ecx, 8                 ; ecx = (Q(edi+8)<<24)|(Q(edi-4)<<16)|(Q(edi+0)<<8)
00199F32 movzx eax, bl ; 00199F43 or eax, ecx   ; | Q(edi+4)
```

Stride `0x10` per element (`0x0019A01A add ecx, 0x10`), four floats in, four bytes out —
a float4→RGBA-style quantizer. `(int)(v*255.0f + 0.5f)` with `437F0000` / `3F000000` ✓.

**But the *"it is a colour word, not an address"* conclusion does not follow, and it is not
falsifiable as posed.** Any 32-bit value decomposes into four bytes in `0..255`, so
"0x001D5078 = four ordinary quantized bytes (0, 29, 80, 120)" would be equally true of any
dword in the image. Meanwhile the dword the record is explaining is, on the bytes, a
**pointer to a filename string**:

```text
001D5078: 64 6A 76 30 30 5F 30 30 2E 61 64 78 00   -> "djv000_00.adx"
```

The session verification itself calls `0x001D5078` *"the ADX filename"*. So the competing
hypothesis — that a **pointer** to `djv000_00.adx` reached the slot — is live and is not
excluded by anything in the record. Claim 6 as a *formula* is CONFIRMED; as a *finding about
this value* it is an inference dressed as a finding. **This is the over-claim.**

---

## 5. Chain completeness

| Link | Status |
|---|---|
| **BASE** `esi = MEM32(0x19DCE0)` | **anchored** (`0x00199DB4`) |
| **INDEX** `ebp = ARG1`, and `ARG1` such that `ARG1 <= 2064 < ARG1+ARG3` | **ASSUMED** — no value, no witness |
| **VALUE** `eax` = packed quantizer output | **anchored** (formula verified) |
| **ORDER** install → write → read → call | **partially assumed** — see below |
| **READ side** `0x00193E62 mov eax,[esi+0x1c4]` / `test` / `je 0x193ece` / `0x00193EB5 call eax` | **anchored** — I decoded all four |

**On order, one genuine corroboration I found (and it is not in the record):** the log's
`Xbox regs: esi = 0x0019D468`. With the packet's `software_device = 0x0019B200`:
`0x19B200 + 0x242C = 0x19D62C` and `0x19D468 + 0x1C4 = 0x19D62C`. **Identical.** The
running `esi` matches the computed `context` exactly, so the `context+0x1C4 ==
software_device+0x242C` slot identity is corroborated by a runtime register against static
arithmetic. (Alias *identity* stays a downstream dependency; this corroborates the read
side only.) The log's three `0x0015F9D0` polls followed by `0x001D5078` corroborates that
**the slot's value changed between poll 3 and poll 4** — it does **not** corroborate *which
store* changed it.

**CHAIN COMPLETE?: NO.** One assumed link (index), and one partially assumed link (order).
A chain with an assumed index is not a "complete instruction-anchored reaching-definition
chain", which is what `O-DATA-AS-CALL` requires. Per packet line 13 the correct row is
**`O-OPEN` + STOP**, with the first missing edge named: *the value(s) of ARG1 and ARG3 at
the `0x00153790` → `0x00199DB0` forwarding site* (`0x00153790` reads `[esp+8]`, `[esp+0xc]`,
`[esp+0x10]` and forwards them — I verified those three reads; their provenance upstream is
untraced).

---

## 6. Evidence-record defects (distinct from execution defects)

1. **`sub_0015F9E0` VA labels are off by 1–2 bytes.** Record: `0x0015F9E5 test`,
   `0x0015F9E8 je`, `0x0015F9EA mov eax,0x15f9d0`, `0x0015F9EF mov [esp+4],eax`,
   `0x0015F9F2 jmp 0x18ce30`. Actual: `0x0015F9E4 test eax,eax`, `0x0015F9E6 jne 0x15f9f6`,
   `0x0015F9E8 mov eax,0x15f9d0`, `0x0015F9ED mov [esp+4],eax`, `0x0015F9F1 jmp 0x18ce30`.
   Semantics identical; the addresses as printed are not instruction boundaries.
2. **`0x000D45D4 mov dword ptr [esi], 0x1ce478`** — `0x000D45D4` is **mid-instruction**; the
   instruction starts at `0x000D45D2` (`C7 46 00 78 E4 1C 00`). The immediate is genuinely at
   `0x000D45D4`, so the wrong VA is an aligned-dword hit, not a decoded boundary.
3. **Claim 12's `jle`** at `0x000623B4`'s neighbourhood: I confirmed
   `mov eax,[esi+0x3c8]` and a following `test eax,eax`, but decoding from `0x000623B4` and
   from `0x000623B0` disagree downstream, so `jle` is **not confirmed**. The refutation of
   the `+0x3C8` lead rests on the two **vtable values**, which I did confirm — so the
   conclusion survives, the detail does not.
4. **Method statement gap:** §6 lists completeness claims and their enumerations, but the
   one claim that needed an enumeration — *ARG1 = 2064 is reachable* — is stated with
   **no enumeration method at all**, violating `docs/agent-workflow.md:775`.

---

## 7. Terminality, controls, scope

- **TERMINALITY: RESPECTED.** The record closes the writer, names EDGE 5 (reader of table
  `0x257DE0` unlocated) and does **not** chase it, does not premise the alias or EDGE 4, and
  does not open mechanism. Mild drift only: *"palette-quantization loop"*, *"2064-element"*
  and *"data-as-call"* shade into mechanism language without being premises.
- **CONTROLS: CORRECT HANDLING.** Zero dump reads ⇒ `check-dump-mapping.py` not run and the
  per-run gate **not** claimed — stated explicitly rather than claimed with no input. ✔
  I reproduced the offset-shift control on XBE bytes: `0x001D5078 → 30766A64`,
  `0x001D5079 → 3030766A`, both MATCH; `(v1 & 0x00FFFFFF) = 0x30766A = (v0 >> 8)` PASSES,
  `0x30` retained. ✔ And the record is right to label this an **original-XBE encoding
  check, not a dump read and not a substitute for the dump gate**. ✔
- **SCOPE/SOURCE: RESPECTED.** `git show --stat f63dc53` → **1 file, +387**,
  `docs/reviews/a2h-slot-writer-terminal-evidence.md` only; **no source edits**. Alias
  identity and read-side linkage recorded as downstream dependencies, not premised; EDGE 4
  not reopened; no DR record cited; `PIO_FREE` / `A4b2-r7` / `A4b2-r8` / `A4b1-r4` /
  `0xFFFFB3` untouched; C-5 separate; float-bit siblings contrastive; no strict/liveness
  claim; no negative attribution from absent log lines (stated explicitly). ✔

---

## 8. Result

```text
DISPOSITION: REJECT
CLAIM 1 (the store + bytes): CONFIRMED
CLAIM 3 (ebp = ARG1): CONFIRMED
CLAIM 4 (arithmetic): CONFIRMED (as arithmetic; does not establish the index)
CLAIM 5 (ARG1 = 2064 REACHABLE): CANNOT VERIFY — this is the crux, and it is UNSUPPORTED
CLAIM 6 (packed colour word): CONFIRMED as a formula; REFUTED as a finding about 0x001D5078
CLAIM 7 (encoding difference): CONFIRMED
CLAIM 8 (GAP B, ARG1 = 0): CONFIRMED
CLAIM 11 (log corroboration): CONFIRMED
CLAIM 12 (+0x3C8 refutation): CONFIRMED (vtable values); the `jle` detail CANNOT VERIFY
ROW CORRECT?: NO — should be O-OPEN + STOP. O-DATA-AS-CALL requires a COMPLETE chain;
              the INDEX link is assumed, not established (§3). Packet line 13 is explicit.
CHAIN COMPLETE?: NO — INDEX assumed (ARG1/ARG3 unknown); ORDER partially assumed
                 (log shows the slot's value changed between polls 3 and 4, not which
                 store changed it). BASE, VALUE and the whole READ side are anchored.
TERMINALITY RESPECTED?: YES (EDGE 5 named and not chased; alias/EDGE 4 not premised)
CONTROLS: CORRECT — zero dump reads, gate honestly NOT claimed, offset-shift control
          reproduced by me on XBE bytes and correctly labelled as not a dump gate
SCOPE/SOURCE: RESPECTED / UNCHANGED (f63dc53 = 1 doc file, +387, no source edits)
OVER-CLAIMING: (1) "ebp = ARG1 = 2064" with no provenance; (2) "2064-element colour write"
          contradicts ARG1 = 2064; (3) "packed colour word, not an address" is unfalsifiable
          as posed and 0x001D5078 is on the bytes a pointer to "djv000_00.adx";
          (4) "the ONE slot" — the store writes a contiguous run ARG3 dwords long
EVIDENCE OUTRUNNING CLAIMS:
  1. "ebp = ARG1 = 2064" (lines 13, 43, 284) — assertion with zero provenance; packet line 6
     said the opposite and nothing new is offered
  2. "2064-element colour write" (line 299) — inconsistent with ARG1 = 2064; no element count
     was ever measured (ARG3 is the trip count, never read)
  3. "a packed colour word landed in the poll callback slot" — value decomposition proves
     nothing about provenance; alternative (a pointer to an ADX filename) not excluded
  4. "the ONE slot has two stores" framing — drops that the store writes ARG3 consecutive
     dwords from 0x3EC + ARG1*4 upward
  5. VA labels at 0x0015F9E5/E8/EA/EF/F2 are not instruction boundaries (off by 1-2)
  6. 0x000D45D4 is mid-instruction (starts 0x000D45D2)
  7. `jle` after 0x000623B4 not confirmed (local decode drift)
  8. No enumeration method stated for the one claim that needed one (workflow §775)
BLOCKING: NONE
UNCERTAIN / CANNOT VERIFY:
  - ARG1 and ARG3 values at the 0x00153790 -> 0x00199DB0 forwarding site (the crux)
  - whether 0x242C is an in-range array element or one past a 2064-element array
  - which store wrote the slot (log shows only that the value changed between polls 3 and 4)
  - the `jle` in the +0x3C8 count argument
  - the 0x000D4DA0 imm32 / 0x257DE0 / entry-8 GAP A material: I re-verified the
    `0x257DE0` construction arithmetic and `0x001E1270` entry 51 only; the six-offset
    ≡1 (mod 4) enumeration and the `find 0x257DE0` = 2 result were not re-run
```

**What a correction would need:** one admissible witness for the index — either a
caller-side constant reaching `[esp+8]`/`[esp+0xc]`/`[esp+0x10]` of `0x00153790`, or a
bounds argument over ARG1/ARG3 from the forwarding site — decoded at a declared boundary,
with the enumeration method stated. Until then the writer *site* remains a strong candidate
and the row remains `O-OPEN`.

*(End of file)*
