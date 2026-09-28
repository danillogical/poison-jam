# `A2h-slot-writer-four-edges-r1` — execution evidence: **`O-OPEN`**, and the FOUR edges collapsed to **ONE**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `A2h-slot-writer-four-edges-r1`, frozen
**`6C207AC90B853744A9C89B71C03187F5E000F0820C2D5D881994B4183A13FB9E`**.
**Worker:** `3e4bfa82-7c9f-47e9-a987-ca3b7aaa0210`, commit `f36b21a`.
**Finding:** `docs/reviews/a2h-slot-writer-four-edges-evidence.md`.

**Row: `O-OPEN`. `WRITER: UNKNOWN`.** **And the four edges are now ONE deciding quantity.**

---

## ⚠ THE METHOD CORRECTION THAT GOVERNS EVERY PRIOR "COMPLETE SWEEP"

**The Worker measured a defect the Session has been making repeatedly:**

> **A linear capstone decode of a section DRIFTS at the first data island, so every later "instruction
> boundary" is wrong.**

**Measured, and the Session verified the class:**

| Address | Naive owner lookup | Reality |
|---|---|---|
| `0x00199F45`, `0x000D4DA2`, `0x000D4684`, `0x000D4DA0` | **"not an instruction boundary"** | **all are real instructions** |
| `0x0018DF59` | **drifted start `0x0018DF06`** | **true start `0x0018DF10`** |

**So the Worker redid every completeness claim by RECURSIVE DESCENT: 30 118 seeds → 673 726 exact
instruction starts.**

### The Session reproduced the drift independently

| Test | Result |
|---|---|
| **linear decode of `.text` from its own start** | **29 548 instructions, and it does NOT reach `0x000D4DA0`** |
| **the same decode** | **does NOT reach `0x00199F45`** |
| **raw bytes at `0x000D4DA0`** | **`8b 01 ff a0 cc 00 00 00` — UNAMBIGUOUS, no decode needed** |

> **CONFIRMED: section-start linear decoding drifts, and the drift is not hypothetical — it misses two
> instructions this packet's whole result depends on.**

***(One nuance the Session records for accuracy: `0x0018DF59` IS reached by a section-start decode, so the
Worker's claim there is about the drifted START `0x0018DF06` versus the true `0x0018DF10` — a different and
consistent observation, not a miss.)***

**This is the Session's own control 7 — *anchor every decode to a declared function boundary* — generalised
into a full-image method and MEASURED.** **The Worker's number quantifies why control 7 was needed.**

**⚠ CONSEQUENCE FOR THE LINE: every earlier *"complete sweep"* and *"exactly ONE"* claim in this line that
rested on a LINEAR decode must be treated as SUSPECT unless it was made by recursive descent or by a
raw-byte scan.** **The Session records this as a standing qualification, not a correction to any single row.**

**And the Session notes the mitigation that happened to save several of its own findings: its key results
came from RAW BYTE scans** (`2C 24 00 00`, the thunk reference, `MEM32(0x001E133C)`), **which are
alignment-independent.** **That is why the single-store and vtable findings survived.**

## ⚠ A second trap: `--aligned` dword scans miss non-4-aligned immediates

**The Worker reports:** *"an `--aligned` dword scan reports 0 references to vtable base `0x001E1270` where
the record's 'THREE references' is CORRECT — the installs are `C7 06 70 12 1E 00`, so the immediate sits at
`0x00152246`, not 4-aligned."*

**So a scan that assumes 4-alignment would have REFUTED the Session's own correct finding.** **Recorded
because the Session's raw-byte scans happened to be unaligned-safe, and a successor might not be.**

## ✅ The verification the Session performed independently

**The index-51 dispatch, checked by RAW BYTES (alignment-independent):**

```
bytes at 0x000D4DA0:  8b 01 ff a0 cc 00 00 00
                      mov eax,[ecx]   = 8B 01              ✓
                      jmp [eax+0xcc]  = FF A0 CC 00 00 00   ✓
and 0xCC = 204 = 51 × 4                                     ✓
```

**And `MEM32(0x001E1270 + 51·4) = MEM32(0x001E133C) = 0x00153790`** — the thunk. ✓

> ### **So `0x000D4DA2 jmp dword ptr [eax+0xcc]` IS the index-51 dispatch, and it selects the thunk.**

## ⚠ The Session's PACKET was WRONG about EDGE 2, and the Worker corrected it

**The packet said `ebx = [esp+0x1c]` is *"an ARBITRARY CALLER WORD."*** **The Worker says it is ARG2.**
**The Session verified the stack arithmetic:**

```
sub esp,8        -> esp = E2-8
push ebx         -> esp = E2-12
push esi         -> esp = E2-16 ; [esp+0x14] = E2+4  = ARG1  (esi = the INDEX)   ✓
push edi         -> esp = E2-20 ; [esp+0x1c] = E2+8  = ARG2  (ebx = the VALUE)
```

> **The Worker is right: `[esp+0x1c]` is ARG2, not an arbitrary word.** **The packet's wording was wrong, and
> the Session wrote it.**

**And the Worker found that EDGE 2's ARG1 is passed a LITERAL `0/1/2/3` at SEVEN of its twelve callers, so
`1645` is NOT REACHABLE.** **So EDGE 2 is closed by unreachability rather than by the false divisibility
argument — and the value written would be ARG2 anyway.**

## The four edges' fates

| Edge | Fate |
|---|---|
| **1 — `0x00199F45`** | **ARG1 confirmed three-deep from the bytes; `2064` NOT ESTABLISHED as reachable.** Only three reachable uses of immediate `0x810` exist image-wide and **none is an argument** |
| **2 — `0x0018DF59`** | **`1645` UNREACHABLE** — seven of twelve callers pass a literal `0/1/2/3`; **and the value is ARG2, not an arbitrary word** |
| **3 — vtable index 51** | **DISPATCH LOCATED at `0x000D4DA2`** — **and it IS EDGE 1's ARG1** |
| **4 — the context alias** | **THE DESCRIPTOR PATH IS REFUTED** — the alias exists but **does not reach the slot**; **118 stores at disp `0x1C4` image-wide, and the ONLY read of the slot uses `esi = context` DIRECTLY, never through the alias** |

> **⚠ PRECISION, per the Advisor (turn `01a0e89e`): *"edge-4 mechanism-refuted ≠ alias resolved."***
> **What is refuted is that the DESCRIPTOR PATH reaches the slot.** **The alias's own IDENTITY — whether the
> object carrying `[eax+0xc]` is what it appears to be — was never established and remains OPEN.** **The
> Session's earlier one-word *"REFUTED"* was too broad, and the successor records the alias as a DOWNSTREAM
> DEPENDENCY rather than as a closed question.**

> ## **So EDGE 3's ARG1 IS EDGE 1's ARG1 — the four edges collapsed to ONE quantity.**

## The Worker falsified its OWN finding, which is the strongest signal in this record

**It reported a new candidate — `0x0018E03E mov [edi+esi*4+0xc], ebx`, needing index 2312 — and then
FALSIFIED it itself:**

> *"`0x0018E00C mov edi,[esp+0x10]` reloads `edi` from the stack, overwriting `mov edi,[0x19dce0]`. **My
> 'base came from `MEM32(0x19DCE0)` somewhere in the function' heuristic is UNSOUND for reassigned
> registers.**"*

**That is the correct instinct and it is exactly the discipline this line has needed.** **The Session
endorses it and records it.**

## The new structural datum, held at arm's length

**`0x001D5078` is the ADX filename `"djv000_0.adx"`, the second field of an 8-byte-stride `(code, name)`
`.rdata` table at `0x001D4BD4..0x001D4DA4`** — **59 references, all `.rdata`, codes `0x281..0x2BB`
CONSECUTIVE (verified numerically), terminated by `FFFFFFFF`/`0` at `0x001D4DA8`.**

**The table has ZERO direct references**, and **the code references nearby target `0x001D4DB0` — the BASE OF
THE NEXT table.** **So the table is indexed by a computed base no scan has pinned.**

**The Worker records this as consistent with the type-confusion shape *"and it is NOT promoted here."***
**Correct.**

## The controls — and the Worker's honest non-claim

**The Worker performed ZERO dump reads, so controls 1–3 and the per-run gate had no input.** **It states:**

> *"I explicitly do NOT claim them as passed."*

**That is exactly right.** **A control with no input is neither passed nor failed, and claiming it would be
false.** **Recorded because it is the honest handling of a control whose input never arose.**

**The alignment control and the X-flagged-section sweep WERE applied and caught three real defects.**

## The smallest missing edge

**EDGE 3's dispatch CALLER.** **The Worker notes the entry path it found:**

```
0018B1A0  mov eax, 0xd4da0
0018B1CD  mov [0x257e00], eax     ; index 8 of table base 0x257DE0
000D4684  mov [esi+0x3c8], 0x257de0
```

**And it reports that a 20-instruction dataflow probe across all 39 reachable functions touching `+0x3C8`
returned ZERO sites — so the dispatch caller is NOT located.**

**With two caveats the Worker MEASURED rather than assumed:**
1. **`0x000D4DA0` and `0x000D4684` are THEMSELVES not reachable by recursive descent from its seeds** — **so
   its seeding has a gap and a dispatcher through `+0x3C8` may exist and be missed.**
2. **The direct-store path still needs one unread range: the caller's source at `0x000122E8..0x00012320`,
   read THROUGH `0x0018CE30`'s argument slot.**

**The Session records both as the successor's targets.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited**; **C-5 stayed entirely separate and did not select the row.**
**All eight guards pass.**
