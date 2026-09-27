# `A4b2-NR-epoch-slice-followup` — Session preparation: interrupt scoping

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Mandate:** the Advisor's `R4` — interrupt scoping is **now MANDATORY** for the next packet
(`docs/reviews/a4b2-nr-epoch-slice-advisor-ruling.md`): *"enabled? handlers where? effects on slice state? —
the recorded CFG gap, now mandatory."*

**Status: Session preparation, not a packet artifact.** It answers the scoping question from **already
archived** data. It does not discharge a criterion.

---

## The recorded gap this addresses

The image-`I` CFG reaches **194 of 2 340** executed PCs from a single entry. The Advisor named interrupt
entry as the leading explanation and made scoping mandatory, because a slice that ignores interrupt entry
cannot claim to cover the feasible routes to the doorbell.

## What the archived artifacts answer

Both images were scanned for interrupt machinery — the interpreter's vector addresses (`0x00`, `0x04`,
`0x08`… `0x34`), interrupt-control peripheral registers, status-register writes, and the interrupt-return /
stack instructions (`rti`, `ssi`, `ssl`).

### Image `I` (239 instructions, bootstrap)

| Check | Result |
|---|---|
| Interrupt-relevant peripheral writes (`x:$FFFFF*`, `x:$FFFFC*`) | **0** |
| SR (status register) writes | **0** |
| `rti` / `ssi` / `ssl` instructions | **0** |
| Vector addresses executed | **only `0x0000`** (×1) — the **reset/entry** vector |
| `0x0004`, `0x0008`, `0x000C`, `0x0014`–`0x0034` | **not executed** |
| `0x0010` executed | ×1 — but that is `jsr p:$009e`, an **ordinary instruction** at that address, not a vector fetch |

### The second image (1 015 instructions, at the first exchange)

| Check | Result |
|---|---|
| `rti` / `ssi` / `ssl` | **0** |
| SR writes | **0** |
| Interrupt / peripheral-register references | **0** |

## The finding, stated carefully

**Neither image contains any interrupt machinery.** No interrupt-control register is written, the status
register is never written, there is no `rti` anywhere, and the only vector address ever reached is `0x0000`
— which is the **reset entry**, not an interrupt dispatch. `0x0010` is executed but as ordinary code
(`jsr p:$009e`), not as a vector.

**So on this evidence, GP interrupt entry contributes nothing to the 194-vs-2 340 reachability gap**, and a
slice over image `I` need not model interrupt handlers **provided** this is confirmed rather than assumed.

## Device-side confirmation — the limit above is now closed

The limit I flagged (a device-side interrupt could dispatch without any *program* instruction writing a
control register) is **resolved from the toolkit source**:

```
static const dsp_interrupt_t dsp_interrupt[4] = {
    { DSP_INTER_RESET,       0x00, 0, "Reset" },
    { DSP_INTER_ILLEGAL,     0x04, 0, "Illegal" },
    { DSP_INTER_STACK_ERROR, 0x02, 0, "Stack Error" },
    { DSP_INTER_TRAP,        0x08, 0, "Trap" },
};
```

**The interrupt table has exactly four entries, and none of them is a peripheral, DMA, timer or
audio-interrupt source.** Every raiser of `dsp56k_add_interrupt` in the toolkit is:

| Raiser | Site | Kind |
|---|---|---|
| `dsp56k_add_interrupt(dsp, DSP_INTER_STACK_ERROR)` | `dsp_cpu.c:1734`, `1778`, `1814` | internal stack fault |
| `dsp56k_add_interrupt(dsp, DSP_INTER_ILLEGAL)` | `dsp_emu.c.inc:6847` | illegal opcode |

**There is no device-side interrupt source wired for the GP at all**, and `gp_ep.c` contains no
interrupt-raise call. So the two remaining vectors that could fire — Illegal and Stack Error — are *fault*
paths, and their vectors (`0x04`, `0x02`) were **not executed** in the measured run.

**Revised statement:** on both the program side (no interrupt machinery in either image) and the device
side (no interrupt source wired), **GP interrupt entry contributes nothing to the reachability gap for this
run**. A slice over image `I` need not model interrupt handlers.

**Residual, stated precisely:** this holds for the pinned configuration. If a later packet wires a real
peripheral interrupt source, the scoping must be redone — and the vector addresses to re-check are `0x00`
(reset), `0x02` (stack error), `0x04` (illegal), `0x08` (trap).

## So what explains the 194-of-2 340 gap?

With interrupts excluded, the gap is **cross-image**: the bootstrap CFG cannot reach code above `0x173`
because **that code did not exist when the bootstrap decode was taken**. The ~2 146 executed-but-unreached
PCs live in the second image, which the loader writes after bootstrap.

**That is exactly why the Advisor's demand-driven design is the right one:** the second image enters the
slice **only via chains that cross into it**, so those ~2 146 PCs need not be enumerated at all to close a
frontier that never reaches them. This preparation therefore supports the redesign rather than complicating
it.

## What this does and does not establish

**Does:** removes interrupt entry as the explanation for the reachability gap, on direct evidence from both
images; and supports the demand-driven design by showing the gap is *cross-image*, not *interrupt-driven*.

**Does not:** prove GP interrupts are never raised (device-side check still required); discharge `L1`; or
establish anything about the doorbell's dependence on the stub inputs.
