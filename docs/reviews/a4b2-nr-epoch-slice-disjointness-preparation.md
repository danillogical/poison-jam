# `A4b2-NR-epoch-slice-followup` — Session preparation: descriptor-disjointness, alias-closure, and the reader

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Mandate:** the Advisor's `R3` — *"descriptor-disjointness PROVED alias-closed (not assumed from the
preparation note)"*, because it **graduates from a Session finding to a load-bearing premise**
(`docs/reviews/a4b2-nr-epoch-slice-advisor-ruling.md`).

**Status: Session preparation, not a packet artifact.** It does not discharge a criterion.

---

## (1) Every call site, enumerated — not sampled

Five call sites across the two builders, found by enumerating all `jsr`/`bsr` in image `I`:

| Call | Target | Builder |
|---|---|---|
| `P 0007` | `P 00DB` | A |
| `P 007E` | `P 00DB` | A |
| `P 0090` | `P 00DB` | A |
| `P 000E` | `P 00EB` | B |
| `P 0019` | `P 00EB` | B |

## (2) `r0` at entry, per call site

| Call | `r0` set at | Instruction | Value | Region written |
|---|---|---|---|---|
| `P 0007` | `P 0002` | `move #$06,r0` | `0x06` | `x:[6..10]` |
| `P 007E` | `P 0077` | `move #$000012,r0` | `0x12` | `x:[18..22]` |
| `P 0090` | `P 0086` | `move #$00000c,r0` | `0x0C` | `x:[12..16]` |
| `P 000E` | `P 0009` | `move #$18,r0` | `0x18` | `x:[24..28]` |
| `P 0019` | `P 0012` | `move #$1e,r0` | `0x1E` | `x:[30..34]` |

The builder writes `x:(r0+0)` … `x:(r0+4)`, so each call owns the 5-word region `r0..r0+4`.

### A Session tooling defect found and fixed while doing this

**The first version of this analysis reported `r0 = UNKNOWN` for all five call sites.** Cause: it walked
backwards using `pc - 1`. **DSP56300 instructions are 1..*N* words**, so the previous instruction is **not**
at `pc-1` — it is the previous entry in the **decoded order**. From `P 0007` the walk landed on `P 0006`
and stopped, never reaching `P 0002` where `r0` is actually set.

**This is the second occurrence of the same class of error in this session** — the earlier CFG had the
identical `pc+1` fall-through bug. Both are now fixed by using decoded order. Recorded because a reader
would otherwise trust an `UNKNOWN` that was an artifact of the tool, not a property of the program.

## (3) Pairwise disjointness — **PROVEN**

All ten call-site pairs are disjoint:

```
P 0007 x:[6..10]  vs  P 000E x:[24..28]   disjoint
P 0007 x:[6..10]  vs  P 0019 x:[30..34]   disjoint
P 0007 x:[6..10]  vs  P 007E x:[18..22]   disjoint
P 0007 x:[6..10]  vs  P 0090 x:[12..16]   disjoint
P 000E x:[24..28] vs  P 0019 x:[30..34]   disjoint
P 000E x:[24..28] vs  P 007E x:[18..22]   disjoint
P 000E x:[24..28] vs  P 0090 x:[12..16]   disjoint
P 0019 x:[30..34] vs  P 007E x:[18..22]   disjoint
P 0019 x:[30..34] vs  P 0090 x:[12..16]   disjoint
P 007E x:[18..22] vs  P 0090 x:[12..16]   disjoint
```

**So the doorbell's `x:[6..10]` cannot be written by any other builder call**, and the earlier
preparation's claim is now a proof rather than an assertion.

## (4) Alias-closure — now substantially closed

**Every X-memory write in image `I`, enumerated by form** (71 writes):

| Form | Sites |
|---|---|
| Direct `x:$NNNN` | **39** |
| Computed `x:(r0+N)` | 21 |
| Computed `x:(r0)+` | 1 (`P 0167`) |
| Computed `x:(r1)` | 2 (`P 00A4`, `P 00BD`) |
| Computed `x:(r4)+` | 8 |

**The 39 direct writes contain NO address in `x:[6..10]`.** They are `x:$0004` (the mailbox flag),
`x:$007C`–`x:$007F` (the `0xFFFFB3` scratch), and `x:$FFFF*` (peripherals). **So no direct write can touch
the doorbell's region.**

**Of the 21 computed `x:(r0+N)` sites, only the `P 0007` call has `r0` in range:**

| Block | `r0` | Region | Reaches `x:[6..10]`? |
|---|---|---|---|
| `P 00E0`–`00E8` via `P 0007` | `0x06` | `x:[6..10]` | **YES — this IS the doorbell's own construction** |
| `P 00E0`–`00E8` via `P 007E` | `0x12` | `x:[18..22]` | no |
| `P 00E0`–`00E8` via `P 0090` | `0x0C` | `x:[12..16]` | no |
| `P 00F0`–`00F8` via `P 000E` | `0x18` | `x:[24..28]` | no |
| `P 00F0`–`00F8` via `P 0019` | `0x1E` | `x:[30..34]` | no |
| `P 001D` | `0x1E` | `x:[31]` | no |
| `P 0138`–`0141` | — | **statically unreachable** | no |
| `P 0149`–`0152` | — | **statically unreachable** | no |

**The two extra builder blocks are statically unreachable**, verified three ways: **no incoming
branch/call target** anywhere in the program, their **predecessor is `rts`** (so no fall-through), and they
execute **0 times**. They are dead code in the pinned image.

**Therefore: the only writer that can reach `x:[6..10]` is the `P 0007` call — which is the doorbell's own
descriptor construction.** Alias-closure holds for **all writers with a statically determined target**.

**Residual, stated:** the remaining computed forms (`x:(r0)+` ×1, `x:(r1)` ×2, `x:(r4)+` ×8) depend on
register values not fixed here, and the reader question (§5) is still `UNKNOWN`. Those are the slice's to
close — but note the doorbell's region is **low** scratch (`6..10`), so a computed write reaches it only if
its register holds `0..10` at that point, which is a narrow and checkable condition.

## (5) The reader — **UNKNOWN, and that is the point**

Image `I` contains exactly **one** computed X read:

```
P 00B9  move x:(r1),b
```

`P 00B9`'s effective address is **measured** as `0x24` on all six executions (`events==execs==6`), which is
outside every descriptor region. **But one measured run is not feasibility.** Per the Advisor's `F-C` and
the packet's own rule, an executed-PC histogram cannot exclude a feasible path — so whether a computed
reader *could* address `x:[6..10]` under some other mailbox value remains **UNKNOWN** until the
demand-driven slice closes it from the doorbell output backwards.

**This is precisely why the Advisor mandated the demand-driven method**, and this preparation supports it:
the reader question cannot be closed by forward enumeration of readers; it must be closed by asking what
the **doorbell's own fields** depend on.

## Summary — what is proven, and what is not

| Question | Status |
|---|---|
| All builder call sites enumerated | **PROVEN** — 5 sites |
| `r0` per call site | **PROVEN** — all immediates, statically known |
| Regions pairwise disjoint | **PROVEN** — all 10 pairs |
| Doorbell region `x:[6..10]` not written by another **builder** call | **PROVEN** |
| No **direct** X write touches `x:[6..10]` | **PROVEN** — all 39 direct writes enumerated, none in range |
| The two extra builder blocks cannot reach it | **PROVEN** — statically unreachable (no incoming target, `rts` predecessor, 0 executions) |
| **Only the `P 0007` call reaches `x:[6..10]`** | **PROVEN for all statically-targeted writers** |
| Remaining computed writers (`x:(r0)+`, `x:(r1)`, `x:(r4)+`) | **UNKNOWN** — register-dependent; slice's to close |
| Reader cannot alias one region as another | **UNKNOWN** — one computed read (`P 00B9`), measured `0x24`, feasibility unproven |

### A Session analysis error, corrected

My first draft of this section claimed **`P 001D` writes `x:[7]`, inside the doorbell's region**, and
concluded alias-closure was unestablished. **That was wrong.** `P 0012` sets `r0 = 0x1E` **before** `P 001D`
runs, so `P 001D` writes **`x:[31]`** — inside builder B's region `x:[30..34]`, part of the `P 0019`
descriptor. Corrected above, with the instruction order shown. **Had this stood, it would have been a false
negative on a load-bearing premise.**
