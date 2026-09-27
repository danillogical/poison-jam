# V2 result — cross-boundary enumeration: **CLEAN**

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Ordered by:** the Advisor's terminal `L1` ruling (`docs/reviews/a4b2-epoch-slice-terminal-ruling.md`),
V2 — *"cross-boundary enumeration over at-exchange second-image bytes, BOTH directions … Any live
edge/write, or any indirect/unresolvable → precise `UNKNOWN` → row does not stand, back to me."*
**Constraint honoured:** **static artifacts only — no new runs, no new packet.**

> ## ⚠ A first version of this record concluded the OPPOSITE, and the error is recorded below
>
> I first reported V2 **NOT CLEAN** with 296 unresolved computed writes and "2 147 second-image PCs
> executing before the exchange". **That was wrong**, and the cause is instructive: the B9 trace dumps a
> PC histogram at **every** terminal, and I read the **last** histogram in the artifact — which is
> **post-exchange**. The pre-exchange record is the block following the **`first-exchange`** terminal.
> See §"The error" below.

---

## The decisive fact: at the exchange, **only image `I` had executed**

From the histogram block immediately following the `first-exchange` terminal
(`logs/runs/20260927-134343-777-a4b2-nrf-b9-arch/gpb9_trace.txt`, lines 1195–1583):

```
# exec_total=37895 gp_exec_total=37895 ngp_exec_total=0 first_pc=0000 is_gp_seen=1
  gp_pc_range=0000..0172 ngp_pc_range=0000..0000 gp_first_high=0000
```

| Field | Value | Meaning |
|---|---|---|
| `gp_pc_range` | **`0000..0172`** | the highest PC executed at the exchange is **`0x172`** — inside image `I` |
| `gp_first_high` | **`0000`** | **no** PC at or above `0x173` was ever executed |
| `#G` entries | **193** | exactly the image-`I` PCs, matching the earlier slice |
| `exec_total` | 37 895 | |

**The second image had NOT executed when the exchange fired.** The `gp_pc_range` header is unambiguous and
independent of my parsing: its high bound is `0x172`, below the second image's first PC (`0x173`).

**The second image begins executing *after* the exchange** — confirmed by the next dump, which still reads
`0000..0172`, and by the final dump (`0000..0F28`), which is post-exchange.

## Therefore V2 is clean in all four directions

| V2 direction | Result |
|---|---|
| **(out)** image-I static transfers targeting ≥ `0x173` | **0** |
| **(out)** computed/indirect transfers in image `I` | **0** |
| **(in)** second-image transfers targeting slice nodes or dead-block PCs | **0** |
| **(data)** second-image direct X writes into a slice-read word | **0** (60 direct, none in range) |
| **(data)** second-image computed X writes | **MOOT** — the second image does not execute before the exchange |

**The 296 computed writes are moot**: they belong to code that runs *after* the exchange, so they cannot
affect it. **The `r5` analysis is retained as a corroborating bound** (all `r5` writes are immediates
≥ `0x2AA`, far above the slice words ≤ `0x25`), but it is no longer load-bearing.

**No live edge, no live write, and no unresolved item that can affect the exchange.**

## The error, recorded rather than hidden

**My first V2 record was wrong and is superseded by this one.** The cause:

- The B9 instrumentation emits a histogram dump **on every terminal**, and there are **12** such dumps in
  the artifact (at each `counts` emission and at the `first-exchange` freeze).
- I took the **last** dump — `exec_total=14659333`, `gp_pc_range=0000..0F28` — and read it as the
  pre-exchange record.
- It is the **run-end** record, after the guest ran on past the exchange to frame 768.
- Reading the correct block (immediately after `first-exchange`) gives `0000..0172`.

**This is the seventh self-caught error in this analysis**, and it is the same class as the others: a
plausible-looking artifact read that happened to be the wrong *instance* of a repeated record. It is
recorded because it produced a **false `UNKNOWN`** that would have triggered an unnecessary re-referral and
possibly an unnecessary instrumented run.

**A check that would have caught it immediately, and which I should have applied first:** the header
carries `gp_first_high`, whose whole purpose is to report the first PC at or above `0x200` — and it read
`0000` in the pre-exchange block. **Reading the field designed to answer the question is faster than
inferring the answer from a PC list.**

## Consequence under the Advisor's ruling

The ruling's conditional was: *"SELECT `O-TWO-LEG` upon V2 clean (Session records it without
re-referral)."*

**V2 is clean. So per the ruling, the Session selects `O-TWO-LEG` and records it without re-referral** —
which is what the Advisor explicitly authorized for this outcome.

**`A4b2-r8` therefore becomes the next authorized packet**, per the ruling's Q3 carry list.

**`A4b2-r7` remains `R2-EXPL-INPUT`. No strict criterion is discharged** — this is discovery acceptance
(artifacts + row selection), not strict acceptance.
