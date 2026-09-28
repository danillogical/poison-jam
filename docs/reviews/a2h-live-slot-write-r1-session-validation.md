# `A2h-live-slot-write-r1` — Session validation and freeze

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Packet:** `docs/packets/a2h-live-slot-write.md`, authored by Planner
`464f275d-69e3-4ad1-93e8-c8bbe3a29b17` (`codex/gpt-6-sol` @ `high`), committed `4cd115d`.
**Authority:** the approved preflight `a2h-live-slot-write-preflight-ruling.md` (`52d327a`) — **apply
verbatim, no second preflight if applied verbatim.**

## Frozen

| Item | Value |
|---|---|
| Revision | **`A2h-live-slot-write-r1`** |
| Lines | **17** (18 with the trailing newline) |
| Bytes | **8397** |
| **SHA-256 (frozen)** | **`8F3C6291C42ED3D9C3B96D879A9BB9D46391E5FEEA3F4BDC1E8712F46FFF7D4F`** |

## Validation — 36 of 36 checks pass

**⚠ Two checks reported MISSING on the first pass, and BOTH were the Session's check strings again** — one
tested `ExceptionInformation[0]==1` through an escape-mangled regex, the other tested a phrasing the packet
renders differently. **The Session read lines 8 and 11 in full and confirmed both requirements ARE present,
verbatim.** **That is instances 14 and 15 of the extraction family, and the response was the same: read the
artifact rather than trust the check.**

**And the Session notes the pattern honestly: this is now a RECURRING check-string failure, not a one-off.**
**The Session's checks are literal-match based and the packet's prose is paraphrased — the mismatch is
structural, and a substring check is the wrong instrument for verifying that a packet encodes a requirement.**
**The right instrument is reading the requirement and its rendering side by side, which is what the Session
did.**

### The preflight is applied verbatim

| Preflight element | Packet |
|---|---|
| **page-protection primary** | *"page-protection primary, DR EXCLUDED entirely (not corroboration)"* ✓ |
| **DR excluded, not demoted** | ✓ — and line 16 adds *"no DR record used"* |
| **RO pages let reads pass** | **line 8:** *"RO pages let reads (including poll volume) pass without fault"* ✓ |
| **`ExceptionInformation[0]==1` PROVEN, not assumed** | **line 11:** *"fixtures demonstrate … `ExceptionInformation[0]==1` admits writes and rejects reads/other exceptions"* ✓ |
| **installer trap REQUIRED** | **line 11:** *"the live installer trap at `0x0018CE3A` must fire … no hit ⇒ `INFRA FAILURE`, fail closed, no outcome claim"* ✓ |
| **dual-mid-step `#DB` ownership, bit-exact** | **line 9:** per-thread `own-TF=0x1`, `AC97-TF=0x2`, saved `EFlags.TF=0x100`, **service every pending owner exactly once**, *"do not consume an unowned or ambiguous `#DB`"* ✓ |
| **synthetic overlap, both VEH orders** | **line 9:** *"Synthetic overlap MUST test mask `0x3` under both VEH orders, both entry TF values, each individual bit and mask `0`"* ✓ |
| **NO THREAD EXCLUSION, EVER** | **line 7:** *"do not … exclude any thread"*; **line 9:** *"no thread exclusion"* ✓ |
| **fresh OFF + N ≤ 5, stop at first qualifying** | **line 13** ✓ |
| **zero qualifying ⇒ RE-REFER, never extend** | **line 15:** *"STOP + PARK/RE-REFER, never extend N"* ✓ |
| **carry-forward RE-VERIFIED, never inherited** | **line 12** — all five elements ✓ |

### The carry-forward gates are all present and correctly framed

**Line 12:** *"tiled/contiguous non-overlap, `g_xbox_mem_offset` read again at TERMINAL, base
`MEM32(0x19DCE0)` re-read at ARM **and** TERMINAL and slot re-derived both times (**changed base ⇒
re-scope, never silently compare old VA**), no-debugger condition, VEH order and alias/threads census."* ✓

**And line 7 refuses to preselect the address:** *"Do not preselect `0x0019D62C`, infer a writer from object
theory, or exclude any thread."* ✓ **That is the base-stability rule applied at the ARM step.**

### ⚠ The two design points the Session especially endorses

**1. The loss rule distinguishes absence from positive records, and that is exactly right.**

**Line 10:** *"overflow or any unreconciled interval **invalidates absence/order rows, not a positive
self-contained writer record**."*

> **That is the correct asymmetry** — **a positive record stands on its own evidence; an ABSENCE claim
> requires complete coverage.** **It is the same distinction the line has had to redraw repeatedly, stated in
> advance this time.**

**2. The candidate's own earlier hit cannot be used as attribution.**

**Line 14:** *"candidate's earlier hit, if any, is **contrastive, not attribution**"*; **line 15:** *"the
packed tuple is **observed flowing into a call, not deduced from byte shape**."*

> **That directly answers the REJECT's over-claim** — **the previous execution deduced the packed-colour
> reading from the byte shape; the packet requires it be OBSERVED flowing into the call.** ✓

### The write scope is declared and narrow

**Line 6:** toolkit **`src/kernel/xbox_memory_layout.c`** and its declaration header *"only if needed"*; game
**`src/main.c`** for read/terminal markers and hook coordination; **`tools/harness/collect.c`** for
all-thread census/collection; **diagnostic fixture source under the existing test layout only.** **No
generated-code edits.** ✓

**⚠ And the Session flags for the owner:** **this is the FIRST packet in this line to touch TOOLKIT source
since the NULL line's DR work — and that work is currently UNPUSHED in a no-push state**
(`a2h-dr0-repair-push-decision.md`). **The new instrumentation will be built ON TOP of those unpushed
commits.** **The Session records that so the dependency is visible: `37226b2` local, `571982d` pushed.**
**No action required — but a successor reader must know the local tree carries uncommitted-to-remote
history.**

## Freeze decision

**The packet is one bounded observation, applies the approved preflight verbatim, and every element the
Session checked is present.** **It is `ADEQUATE`, validated, and frozen.**

**Nothing changes its class, rows or prohibitions.** **No synthetic completion.** The producer line stays
**PARKED**; `PIO_FREE` stays **DEFERRED**; `A4b2-r7`/`A4b2-r8`/`A4b1-r4` are **not** reopened; `0xFFFFB3`
stays **`UNRESOLVED`**; **the retired NULL line is not reopened and no DR record may be cited.**
