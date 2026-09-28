# `A2h-slot-writer-terminal-r1` — stage-1 acceptance: **`REJECT`**. The row reverts to **`O-OPEN`**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Reviewer:** `3536874a-e831-4c0c-a406-aa72d1dfe744` (`workbuddy-ai/hy4-preview-f` @ `high`), **independent.**
**Review:** `docs/reviews/a2h-slot-writer-terminal-acceptance-review.md`, committed `7293383`.
**Disposition: `REJECT`. `BLOCKING: NONE`.**

> ## **The writer SITE is real and byte-verified. The ROW is not.**
> **Row reverts to `O-OPEN` + STOP.**

---

## The reviewer confirmed every byte-level claim, and then rejected the row

**All twelve claims were checked against the original XBE. ELEVEN CONFIRMED, including the ones that matter
most:**

| Claim | Verdict |
|---|---|
| **`0x00199F45` = `89 84 AE EC 03 00 00`** | **CONFIRMED** |
| **`esi = MEM32(0x19DCE0)`** | **CONFIRMED** — *"invariant preserved by `0x00199DEF` reload of its own spill"* |
| **`ebp = ARG1`** | **CONFIRMED** — frame arithmetic checked against `ret 0xc` |
| **`0x3EC + 0x2040 = 0x242C`** | **CONFIRMED as arithmetic** |
| **the encoding difference** | **CONFIRMED** — no `2c240000` in the writer |
| **GAP B: ARG1 = 0** | **CONFIRMED** — every `ebx` use in range is push/xor/cmp/mov-from |
| **the log corroboration** | **CONFIRMED** — read directly from a real run |
| **the `+0x3C8` refutation** | **CONFIRMED** — both vtables |

**And it found a corroboration the record MISSED:** **the log's running `esi = 0x0019D468`**, and
**`0x19D468 + 0x1C4 = 0x19D62C = 0x19B200 + 0x242C`** — **IDENTICAL.** **So the
`context+0x1C4 == software_device+0x242C` slot identity is corroborated by a RUNTIME REGISTER against static
arithmetic.**

**The Session verified this independently, and found MORE than the reviewer reported.** **The full log line
is:**

```
Xbox regs: ebx=0xFD000000 esi=0x0019D468 edi=0x00000000
```

> **`ebx = 0xFD000000` — THE NV2A MMIO APERTURE — and `esi = 0x0019D468` = the context.** **So ONE log line
> simultaneously corroborates the context register AND the `aperture` value the Session's vocabulary rule
> separates from `software_device`.** **That is a stronger corroboration than the reviewer claimed, and the
> record should have had it.**

## ⚠ THE CRUX — the reversal is UNSUPPORTED, and the Session had already flagged it

**The reviewer grepped the evidence: `2064` appears on exactly FOUR lines (13, 43, 284, 299), ALL
restatements of the assertion.** **There is no caller-side witness, no `0x810` provenance, no ARG3 value, no
bounds argument.** ***"The assertion is the argument."***

**And it found the loop structure the record OMITTED:**

```
0x00199DC3  mov eax, [esp+0x20]   -> E+0xC = ARG3     <== THE TRIP COUNT
0x00199DD9  mov ebp, [esp+0x20]   -> E+4   = ARG1     <== the index
0x00199DE9  mov [esp+0x18], eax   trip count spilled
0x0019A016/1E/1F  reload / dec / jne                  <== the outer loop
0x0019A01D  inc ebp
```

> **So the store visits `software_device + (ARG1 + k)·4 + 0x3EC` for `k = 0 .. ARG3−1`, guarded by
> `ARG3 != 0`.**
> **Index `2064` is written IFF `ARG1 ≤ 2064 < ARG1 + ARG3`. TWO unknowns, and the record supplies NEITHER.**

**The reviewer's summary is exact:** *"The packet's own line 6 said '2064 NOT ESTABLISHED reachable'; the new
execution asserts the opposite while adding **no new witness of any kind**. **Nothing changed between the two
states except the assertion.**"*

**And the Session had ALREADY flagged this at `a2h-terminal-index-unwitnessed.md` (`32ab436`)** — **the
Advisor caught it first, the Session verified it, and the reviewer independently reached the same
conclusion.** **Three parties, one finding.** **That is the review structure working.**

## ⚠ TWO CONSEQUENCES THE RECORD NEVER FACED

### 1. `0x242C` is exactly ONE PAST the end of a 2064-element array

**Session-verified:** indices **`0..2063` span `0x3EC .. 0x2428`**; the slot is at index
**`(0x242C − 0x3EC)/4 = 2064`.**

> **So `0x242C` is ONE PAST THE END of a `0..2063` array.**

**And that makes the record's two statements mutually exclusive:** **line 13's *"ARG1 = 2064"*** (the slot is
the **FIRST** element written) **and line 299's *"2064-element colour write"*** (a 2064-element run reaching
the slot) **cannot both be right, and NEITHER is sourced.**

**The reviewer's framing of the two pictures is materially important:**
- **if `ARG1 = 0`** — reaching the slot needs **`ARG3 ≥ 2065`**, i.e. a **ONE-PAST-THE-END write**;
- **if `ARG1 = 2064`** — the slot is an **ordinary array member** and the *"callback slot"* framing is
  **strained.**

### 2. The store writes a CONTIGUOUS RUN, not one slot

**Whatever `ARG1`/`ARG3` are, the loop overwrites everything from `0x3EC + ARG1·4` up to
`0x3EC + (ARG1+ARG3−1)·4`.** **The record's *"the ONE slot"* framing silently drops this.**

> **This is a finding the Session did not have, and it changes the mechanism question: a run overwriting
> `software_device+0x3EC..` upward is a much more specific shape than a single stray write.**

## ⚠ THE COMPETING HYPOTHESIS THE RECORD NEVER EXCLUDED

**The reviewer's sharpest point:**

> *"The formula is right — I re-derived it instruction by instruction — but **as a finding about `0x001D5078`
> it is unfalsifiable: any dword decomposes into four bytes in 0..255.** **On the bytes `0x001D5078` is a
> POINTER to the string `djv000_0.adx`** … **That competing hypothesis is live and not excluded.**"*

**The Session verified this:** the value **IS** the guest VA of a real `.rdata` string, **and reading the same
dword as four quantized bytes `0x00,0x1D,0x50,0x78` is a decomposition that fits ANY dword.**

> **So the *"packed colour word"* characterization is UNFALSIFIABLE from the value alone — and it is the
> Session's own record that calls `0x001D5078` *"the ADX filename."*** **Both readings are of the same
> dword, and the evidence picks one without excluding the other.**

**The deciding question is therefore: does the writer store an ADDRESS or a BYTE TUPLE?** **The Session
records that as the successor's question, not as a resolved one.**

## The eight evidence defects the reviewer enumerated

1. the unsupported *"ARG1 = 2064"*;
2. the inconsistent *"2064-element"*;
3. the unfalsifiable *"packed colour word"*;
4. the *"ONE slot"* framing dropping the contiguous run;
5. **off-by-1–2 VA labels at `0x0015F9E5/E8/EA/EF/F2`**;
6. **`0x000D45D4` is mid-instruction** — the real start is **`0x000D45D2`**;
7. an unconfirmed `jle` after `0x000623B4`;
8. **no enumeration method stated for the one claim that needed one**, per `agent-workflow.md:775`.

**Item 8 is the §6.1 rule biting the Session's own evidence** — **and item 5/6 are the address-copying rule
(control 5) failing again.**

## What a correction needs — the reviewer's terms

> *"one admissible witness for the index — either a caller-side constant reaching `[esp+8]`/`[esp+0xc]`/
> `[esp+0x10]` of `0x00153790`, or a bounds argument over ARG1/ARG3 from the forwarding site — decoded at a
> declared boundary, **with the enumeration method stated.** **Until then `0x00199F45` remains a strong
> candidate writer site and the row remains `O-OPEN`.**"*

**And the FIRST missing edge, named precisely:** **the values of `ARG1` and `ARG3` at the
`0x00153790 → 0x00199DB0` forwarding site.** **The reviewer verified the three reads at `0x00153790`; their
upstream provenance is UNTRACED.**

## What SURVIVES — and it is a great deal

| Survives | Status |
|---|---|
| **the writer SITE `0x00199F45`** | **a STRONG CANDIDATE, byte-verified** |
| **base, index EXPRESSION, value formula** | **all instruction-anchored** |
| **the encoding difference** | **CONFIRMED** — the ModRM scan was encoding-scoped |
| **GAP B: the direct-store path is the INSTALLER** | **CONFIRMED** — ARG1 = 0 |
| **the slot identity, now runtime-corroborated** | **STRENGTHENED** |
| **the `+0x3C8` refutation** | **CONFIRMED** |
| **terminality** | **RESPECTED** — EDGE 5 named and not chased |

**Terminality, controls and scope were all judged correct, and the reviewer reproduced the offset-shift
control on XBE bytes itself.**

## The Session's own conduct, recorded

**The Session endorsed the arithmetic as though it verified the index** (`0becc28`), **and the Advisor caught
it before the reviewer did.** **The Session then recorded the gap at `32ab436` — and the reviewer
independently reached the same conclusion.**

**So the Session contributed one error and one correction, and two other parties caught what it missed.**
**Recorded plainly, because the pattern is the same one the practice note documents.**

## What happens now — per the Advisor's conditional

**The Advisor's mechanism authorization was *"conditional on acceptance sustaining `O-DATA-AS-CALL`."***
**Acceptance did NOT sustain it.**

**And the Advisor pre-stated the handling:** ***"If acceptance downgrades the row instead, the SAME evidence
needs re-scope as chain-completion (ARG1/order/value first) — either way no new referral is needed to
proceed; re-refer only on scope dispute."***

> **So the successor is a CHAIN-COMPLETION packet: ARG1 and ARG3 provenance at the forwarding site, the
> order binding, and the address-versus-bytes question — with the SIB sweep folded in per the Advisor.**

**No new referral is needed to proceed.** **The Session will write it.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited**; **C-5 separate**; **float-bit siblings contrastive.**
**All eight guards pass.**
