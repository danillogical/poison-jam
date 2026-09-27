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

## (4) Alias-closure against other writers — **the honest limit**

The builder body itself writes through `x:(r0+N)` with computed `r0`, so "which instruction writes these
regions" cannot be answered by address matching alone. **21 candidate writers** touch a `x:(r0+N)` form in
image `I`:

- the two builder bodies `P 00DB`–`P 00EA` and `P 00EB`–`P 00FA`;
- **three more builder-shaped blocks** at `P 0138`–`P 0141`, `P 0149`–`P 0150` (and `P 00F0`–`P 00F8`);
- `P 001D  move x0, x:(r0 + 1)` — **not a builder**, a separate store through `r0`.

**`P 001D` is the interesting one, and my first reading of it was WRONG — corrected here.** I initially
wrote that `r0` at `P 001D` comes from the entry sequence's `P 0002` (`r0=6`), making `x:(r0+1)` land on
`x:[7]` **inside the doorbell's region**. **That is false.** The instruction order is:

```
P 0010  jsr p:$009e        ; the call
P 0012  move #$1e,r0       ; r0 := 0x1E   <- runs BEFORE P 001D
P 0013  move #$001560,r1
P 0015  move #$00b000,r2
P 0017  move #$000280,r3
P 0019  jsr p:$00eb        ; builder B, region x:[30..34]
P 001B  move #$0049e2,x0
P 001D  move x0, x:(r0 + 1)  ; writes x:[0x1E+1] = x:[31]
```

So `P 001D` writes **`x:[31]`, inside builder B's region `x:[30..34]`** — it is part of the `P 0019`
descriptor's construction, **not** a write into the doorbell's `x:[6..10]`. **The doorbell's region is not
touched by it.**

**Corrected statement:** on this evidence, no instruction outside the builder call for `P 0007` writes into
`x:[6..10]`. **Alias-closure is therefore substantially stronger than my first draft claimed** — but it is
still not a *proof* here, because the three further builder-shaped blocks (`P 00F0`–`P 00F8`,
`P 0138`–`P 0141`, `P 0149`–`P 0150`) are reached by calls this preparation did not enumerate, and their
`r0` values are not fixed here. The demand-driven slice must close that.

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
| Doorbell region not written by any **other** instruction | **Not proven here, but no counterexample found.** The one candidate (`P 001D`) was **misread on first pass and corrected**: it writes `x:[31]`, inside builder B's region, not the doorbell's. Three further builder-shaped blocks remain unenumerated. |
| Reader cannot alias one region as another | **UNKNOWN** — one computed read (`P 00B9`), measured `0x24`, feasibility unproven |
