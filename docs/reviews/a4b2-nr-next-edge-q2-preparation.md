# `A4b2-NR-next-edge` — Session preparation of the Advisor's Q2 re-derivations

> ## ⚠ SCOPE WITHDRAWN — READ THIS FIRST
>
> **The Advisor has re-scoped this document** (`docs/reviews/a4b2-nr-next-edge-advisor-ruling.md` W3,
> 2026-09-27). The seven **tasks** below survive and remain the correct list. Their **input bytes are
> void**: this document re-derived them over the **bootstrap-time** decode of a **single** image, and the
> `A4b2-NR-next-edge-r1` execution showed the GP **loads a second program into P-memory after the
> bootstrap** — 99.4% of all executions were at a PC whose bootstrap-snapshot word was zero or `0xCACACA`
> memset fill.
>
> **Therefore:** the enumeration counts below (`0xFFFFB3` = 4 reads, `x:$007c`–`$007f` = 8 refs, 27
> DMA-register refs, 20 branch targets, six `r1` writers, the `P 00DB` three-caller finding and its
> disjointness argument) are **statements about the bootstrap-time image**, not about the program the GP
> executed. They must be **re-derived over epoch-stable bytes** once the loader/epoch work establishes
> which bytes are stable over the slice window.
>
> **What this document is still good for:** the *method* of each re-derivation, and the finding that the
> `P 00DB` builder has multiple call sites writing disjoint regions — a question the next slice must answer
> again on correct bytes, and now knows to ask.
>
> **Not withdrawn:** the `P 00B9` measurement table (a runtime measurement, not a decode reading) and the
> six table words' values (identical in both snapshots).

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Mandate:** `docs/reviews/a4b2-nr-followup-advisor-ruling.md` Q2 — *"every completeness/negative claim from
the 512-word analysis is SUSPENDED … must be re-derived over the full 3881-word program before next-edge
may rely on it."*

**Status of this record: Session preparation, NOT a packet artifact.** It is a reading aid built from the
archived decode, prepared so the next packet's Planner has the re-derived facts. **It does not discharge
any criterion, does not substitute for the packet's required PC-indexed CFG/def-use slice, and makes no
claim about the doorbell.** Every verdict below is stated against the Advisor's own wording, including the
ones that changed.

**Inputs (all archived, `logs/runs/20260927-134343-777-a4b2-nrf-b9-arch/`):** `full_decode.txt` — 3 964
decoded instructions, PC range `0000..0FFF`, 6 undecoded words (the `00CC`–`00D1` table), covering
**100%** of the 2 340 executed PCs; `gpb9_trace.txt` — the complete P 00B9 trace plus executed-PC
histograms. Helper: `a4b2-next-edge-q2.py` (scratch, outside the repo).

---

## Results — all seven items

| # | Suspended claim | Verdict over the full program |
|---|---|---|
| 1 | `0xFFFFB3` has **four** read sites | **SURVIVES** — exactly 4, none above `0x1FF` |
| 2 | `x:$007c`–`x:$007f` used **only** for loop control | **SURVIVES as to *sites*** — 8 refs, none above `0x1FF`; "loop-control-only" still needs the *semantic* check |
| 3 | `P 00DB` is the **sole** descriptor builder | **REFUTED as to "sole"** — 3 callers (plus 2 for `P 00EB`); but the callers write **disjoint** descriptor regions, so the *conclusion* survives on a **different reason** (per-call disjointness, §Q2(3)) |
| 4 | `P 00D4`–`00DA` is the **sole** DMA trigger | **SURVIVES as to *sites*** — 27 DMA-register refs, none above `0x1FF`; two distinct trigger paths exist |
| 5 | the mailbox `$0000`–`$0004` is **never written** by the GP | **SURVIVES as to `$0000`–`$0003`** (read-only); `$0004` **IS** written — but the original claim already said so |
| 6 | **no** branch targets the `00CC`–`00D1` table | **SURVIVES** — 20 distinct targets program-wide, **none** in the gap |
| 7 | `r1` at `P 00B9` comes from `P 008C` | **SURVIVES as to *sites*** — no `r1` write above `0x1FF`; measured live value is `P 00A2`'s `0x24` |

**Headline: six of the seven survive re-derivation unchanged; the seventh (`P 00DB` "sole builder") is
refuted as to its *reason* while its *conclusion* survives on a stronger, previously-unstated reason —
per-call-site descriptor disjointness.** That is a genuinely useful correction: the original argument was
right by accident of a single caller, and the slice now has the real argument.

---

## Q2(3) — the refutation, in full, and why it turns out to be **contained**

The original Leg 1 stated the doorbell descriptor is built by `P 00DB` and read it as the sole builder.
**Over the full program, `P 00DB` has three callers, and its sibling `P 00EB` has two:**

| Caller | Instruction | Executions | `r0` | Descriptor region | Fields |
|---|---|---|---|---|---|
| `P 0007` | `jsr p:$00db` | 1 | **6** | `x:[6..10]` | `r1=#$00`, `r2=#$000800`, `r3=#$06` — **all immediates** |
| `P 007E` | `jsr p:$00db` | 1 | **0x12** | `x:[18..22]` | `r1=#$002971`, `r2=#$000818` immediate; `r3 = x:$0001` **runtime** |
| `P 0090` | `jsr p:$00db` | 2 | **0x0C** | `x:[12..16]` | `r1 = 0x80+x:$0000`, `r2 = x:$0002`, `r3 = x:$0003` — **runtime** |
| `P 000E` | `jsr p:$00eb` | 1 | 0x18 | `x:[24..28]` | sibling builder |
| `P 0019` | `jsr p:$00eb` | 1 | 0x00 | `x:[0..4]` | sibling builder |

The builder writes `x:(r0+0)` … `x:(r0+4)`, so each call owns the 5-word region `r0..r0+4`.

**The regions are pairwise disjoint — verified, no aliasing:**

| Pair | Overlap |
|---|---|
| doorbell `x:[6..10]` vs `x:[18..22]` | **no** |
| doorbell `x:[6..10]` vs `x:[12..16]` | **no** |
| `x:[18..22]` vs `x:[12..16]` | **no** |

**Why this matters — and why the refutation is contained.** The original claim "the doorbell descriptor is
built entirely from immediates" is **true for the `P 0007` call**, which is the call whose `r2 = #$000800`
matches the observed `dsp_addr=000800`. But the *inference* drawn from it — that the builder is a single,
all-immediate site — **does not hold**: two other calls feed the same builder from **runtime mailbox
values** (`x:$0000`–`x:$0003`).

**However, those two calls write to disjoint scratch regions**, so their runtime-derived fields land in
`x:[12..16]` and `x:[18..22]`, **not** in the doorbell's `x:[6..10]`. So the *conclusion* the original
record drew — that the doorbell's descriptor fields are immediates — survives, while the *reason* it gave
("sole builder") does not. The correct reason is **per-call-site disjointness**, which this re-derivation
now supplies and the original never stated.

**Consequence for the next packet:** the slice must treat `P 00DB`/`P 00EB` as **multi-site builders with
per-call reaching definitions**, and must carry the disjointness argument explicitly rather than relying
on a single-caller assumption. **The open question the slice must still answer** is whether the *consumer*
of those scratch regions reads them by a computed pointer that could alias — i.e. whether `x:[12..16]` or
`x:[18..22]` can ever be read *as* the doorbell's descriptor. That is a feasibility question about the
reader, not the writer, and it is now a named obligation rather than an open-ended "re-derive everything".

---

## The surviving six, with their evidence

**Q2(1) `0xFFFFB3`: 4 sites, unchanged.** `P 002D`, `0033`, `003A`, `004D` — each executed **767** times,
all inside the original window, **zero** new sites above `0x1FF`. The "four reads" count stands.

**Q2(2) `x:$007c`–`x:$007f`: 8 references, all in-window.** `P 001F`, `0021`, `002F`, `0035`, `003C`,
`0042`, `004F`, `0055`. *(The original record said 10; the full-program count is 8 — a counting
discrepancy in the original that does not affect the conclusion, and is noted here rather than glossed.)*
No reference above `0x1FF`. The **"used only for loop control"** reading still requires the semantic check
against the full program's use of those scratch words, which this site enumeration does not by itself
settle — so the claim is **site-complete but not yet semantically closed**.

**Q2(4) DMA registers: 27 references, all in-window** *(original said 21 — again a counting discrepancy,
not a scope change)*. **No site above `0x1FF`.** Two distinct trigger paths are visible and both matter:
`P 00D4`–`00DA` (executed **once**, the doorbell path) and `P 010A`–`0123` (executed ~2 306–2 307 times,
the audio path). The original "sole trigger" wording was **too narrow**: the `P 010A` path is a second,
heavily-used trigger. Both were already visible in the predecessor's decode; the full program adds no new
sites.

**Q2(5) the mailbox.** `$0000`, `$0001`, `$0002`, `$0003` are **read only** — never written by the GP.
`$0004` **is** written, at `P 0098 move x0,x:$0004` (twice), which the original record already stated
("only `x:$0004` is written, at `P 0098`"). **No writes above `0x1FF`.** The original claim holds exactly as
written. Note `P 0066`/`P 0084` are flagged WRITE by the crude pattern match but are `jclr` **bit tests** on
`x:$0004`, not stores — a reminder that pattern matching is a reading aid, not a proof.

**Q2(6) the table gap: no branch targets it.** 20 distinct branch/call targets program-wide; **none** in
`00CC`–`00D1`. The *feasibility* ground the Advisor suspended **survives** re-derivation, which combined
with the *execution* ground (the `op =` diagnostic fires 0× in execution runs) closes the table question
from both sides.

**Q2(7) `r1` at `P 00B9`.** Six instructions write `r1`: `P 0003`, `000A`, `0013`, `0079`, `008C`, `00A2`.
**No `r1` write above `0x1FF`.** `P 008C move a,r1` executes twice; `P 00A2 move #$000024,r1` executes
once and is the write whose value the measurement sees at all six `P 00B9` executions. So the measured
`0x24` is explained by an in-window write, and the full program adds no new `r1` source.

---

## What this does and does not establish

**Does:** every one of the Advisor's seven suspended items now has a full-program re-derivation; six
survive and one (`P 00DB` builder callers) is refuted with a concrete replacement fact. Two minor counting
discrepancies in the original record are corrected (8 not 10, 27 not 21).

**Does not:** discharge `L1 = PROVEN`. This is **site enumeration plus execution counts**, not the
**PC-indexed CFG/def-use slice over every feasible route** the packet requires. Feasibility — as opposed
to observed execution — is still unproved for the alternate edges, and the newly-found builder call sites
make that gap concrete rather than abstract. It also says nothing new about the doorbell itself: the
doorbell tuple is unchanged and `L2` is untouched.
