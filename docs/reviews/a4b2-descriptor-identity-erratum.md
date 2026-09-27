# ERRATUM — the "doorbell descriptor" identifier is wrong across several records

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Raised by:** the `A4b2-NR-epoch-slice-followup-r1` execution, which **measured** the consumer.
**Authority:** `docs/reviews/a4b2-nr-epoch-slice-execution-evidence.md`.

---

## The error

Several records — including four **frozen packets** — refer to `x:[6..10]`, the region built by the
**`P 0007`** call with `r0 = 6`, as *"the doorbell descriptor"*.

**That descriptor does not produce the `B+0x810` exchange.** Measured:

```
[GPDMADESC] GP_CLEAR produced by block_addr=0018 (dsp_addr=000800)
```

`block_addr` is printed with **`%04X` — hex** (`apu_watch.c:657`), so `0018` is **hex `0x18` = block 24
decimal** — the descriptor built by the **`P 000E`** call (builder B, `P 00EB`), whose fields are `r1 = 0`,
`r2 = 0x000800`, `r3 = 6`, **all immediates**, and whose `scratch_offset = 0x800` reproduces the observed
`dsp_addr=000800` exactly.

> **RADIX NOTE (Advisor-ordered correction).** An earlier version of this erratum said `block_addr` is
> **decimal**. **That was wrong** — the format is `%04X`, i.e. hex, and DSP immediates are hex throughout.
> The **value 24 is correct** (`0x18` = 24), but the *reasoning* was backwards, and a reader doing decimal
> arithmetic would re-derive **block 18** → region `[18..22]` → **the wrong descriptor again**. The system
> closes consistently under hex: chain `0x06 → 0x25 → 0x1E`, `r0 = #$18` = 24, `#$000012` = `0x12` = 18 →
> `[18..22]`.

**The correct identifier: block 24 (`0x18`), built by the `P 000E` call.**

## Why it went unnoticed for four packets

The `x:[6..10]` descriptor **is** in the same DMA chain and **is read** (measured: 255 doorbell-region reads
vs 254 mixbin-region reads, cycling `6 → 0x25 → 0x1E`). It was a plausible identification, it was never
contradicted by the doorbell tuple, and the two descriptors share the same builder body — so every static
reading of "the descriptor's fields" was *self-consistent* while pointing at the wrong block. Only
measuring which block produces the exchange distinguished them.

## Affected records

**Frozen packets — DO NOT EDIT.** A frozen contract is the artifact a revision executed under; editing one
would invalidate its own provenance. Listed so a reader knows the identifier inside is stale:

| Record | Note |
|---|---|
| `docs/packets/a4b2-nonreliance-discovery.md` | `A4b2-NR-r2` |
| `docs/packets/a4b2-nr-followup.md` | `A4b2-NR-followup-r1` |
| `docs/packets/a4b2-nr-next-edge.md` | `A4b2-NR-next-edge-r1` |
| `docs/packets/a4b2-nr-epoch-slice-followup.md` | `A4b2-NR-epoch-slice-followup-r1` |

**Evidence and review records — pointer added where it matters most:**

| Record | Status |
|---|---|
| `docs/reviews/a4b2-nr-epoch-slice-disjointness-preparation.md` | **erratum added at the top** |
| `docs/reviews/a4b2-nr-epoch-slice-execution-evidence.md` | contains the **correct** measurement and both self-corrections |
| others (see below) | the identifier appears, but the analysis does not depend on it |

## What is NOT affected

- **The disjointness and alias-closure analysis.** It is a statement about which writer can reach which X
  region, and it holds for `x:[6..10]` exactly as written. The five call sites, their `r0` values, the
  pairwise disjointness, the 39 enumerated direct X writes, and the two statically-unreachable blocks are
  all still correct.
- **The five field values themselves.** Both descriptors' fields are immediates; block 24's are the ones
  that matter, and they are immediates too.
- **`L2 = INVARIANT`**, the doorbell tuple, image-`I` stability by watch, the entry-set proof, interrupt
  exclusion, and the reader/writer closures. None depends on the descriptor's identity.
- **No `§5.4(2)` is raised.** No frozen contract *premised* the descriptor's identity; the identifier was
  descriptive shorthand inside analysis, and every criterion those packets set was about the doorbell's
  *fields and guards*, which block 24 satisfies.

## Instruction to downstream work

**Do not carry the phrase "the doorbell descriptor `x:[6..10]`" forward.** Cite **block 24** and the
measurement in `a4b2-nr-epoch-slice-execution-evidence.md`. `A4b2-r8`, if it is opened, should record this
correction explicitly so the wrong identifier does not propagate further.

## The broader lesson, stated plainly

This is the **sixth** self-caught error in this analysis (after `pc+1` fall-through, `pc-1` backward walk,
readers-vs-writers, 4-digit address truncation, and an ungated GP/EP trace). **Five of the six were static
reasoning errors; the one that survived four packets was corrected only by measuring.** The pattern is
consistent: static readings of this program are self-consistent enough to hide their own errors, and
measurement is what exposes them.
