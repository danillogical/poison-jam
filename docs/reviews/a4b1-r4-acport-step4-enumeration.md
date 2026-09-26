# A4b1-r4 — AC-PORT step 4 enumeration

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.
**Derived from source** by `logs/a4b1/acport-step4.py`, not asserted from a summary. Toolkit HEAD
`3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`.

`AC-PORT` step 4 (packet line 246) requires the Session to **record**:

> the choke-point function and every pinned write callback that calls it, with source lines; every GP
> input path (peripheral read, DMA read, FIFO read, mix-buffer read) with the line where it calls the
> recording hook; and the static per-offset `PERIPH` classification and per-FIFO source classification
> (Device semantics 6).

This is an **enumeration** requirement, not an existence claim. The Session found its own execution
evidence recorded only *"exactly one call site: `gp_ep.c:144`"* — true, but not the enumeration the
criterion asks for. This file supplies it.

---

## 1. The choke point, and every pinned write callback that reaches it

**The one choke point:** `apu_gp_dma_write`, defined at **`src/apu/apu_watch.c:385`**, declared at
`src/apu/apu_watch.h:341`.

**Every call site in the tree — there is exactly one:**

| Call site | Callback chain that reaches it |
|---|---|
| **`src/apu/dsp/gp_ep.c:144`** | `gp_scratch_rw` (`gp_ep.c:161`) → `scatter_gather_rw` (`:85`) → `apu_gp_dma_write` (`:144`) |

**The pinned write callbacks that reach the choke point:**

| Callback | Line | Reaches the choke point? |
|---|---|---|
| `gp_scratch_rw` | `gp_ep.c:161` | **yes** — it is the GP's scratch callback, registered by `dsp_init(d, gp_scratch_rw, gp_fifo_rw, true)` at `gp_ep.c:676` |
| `ep_scratch_rw` | `gp_ep.c:170` | **no** — the EP's callback; `scatter_gather_rw` writes only when `dir` is set *and* the transfer is the GP's |
| `circular_scatter_gather_rw` | `gp_ep.c:179` | calls `scatter_gather_rw` at `:196`; reached from `gp_fifo_rw` (`:261`) and `ep_fifo_rw` (`:327`) |

**So: one choke point, one call site, reached through the pinned `gp_scratch_rw` → `scatter_gather_rw`
path.** No other write path exists in `src/apu/**` — verified in
`a4b1-r4-implementation-verification.md` item 8, where the only other writers are `FEMEMDATA`
(`apu_core.c:165`) and VP (`apu_vp.c`), both pre-existing and permitted by the packet.

## 2. Every GP input path, with the line that calls its recording hook

| Path | Recording function | **Call site(s)** |
|---|---|---|
| **`PERIPH`** — peripheral register read | `apu_gpin_periph_read` (`apu_watch.c:575`) | **`src/apu/dsp/dsp.c:87`** — `apu_gpin_periph_read(address - DSP_PERIPH_BASE, v)` in `read_peripheral` |
| **`DMA_READ`** — guest-memory DMA read | `apu_gp_dma_read` (`apu_watch.c:508`), `apu_watch_boot_scratch_read` (`:547`) | **`src/apu/dsp/gp_ep.c:110`** — the SGE descriptor fetch; **`gp_ep.c:146`** — the data read; **`gp_ep.c:135`** — the bootstrap scratch read (`apu_watch_boot_scratch_read`, the `AC-BOOT` presence witness) |
| **`FIFO_READ`** — FIFO read | `apu_gpin_fifo_read` (`apu_watch.c:647`) | **`src/apu/dsp/gp_ep.c:252`** — the `gp_fifo_rw` hook under `!dir`; **`src/apu/dsp/dsp_dma.c:333`** — the read-arm hook added by Advisor ruling (C) |
| **`MIXBUF`** — mix-buffer read | `apu_gpin_mixbuf_read` (`apu_watch.c:620`) | **`src/apu/dsp/interp/dsp_cpu.c:916`** and **`:922`** — **two** call sites in the pinned interpreter |

**Two facts this enumeration makes visible that the evidence previously did not:**

1. **`MIXBUF` has TWO call sites**, not one (`dsp_cpu.c:916` and `:922`). Both are in the pinned
   interpreter's mix-buffer read; `:922` is the `0xc00`-offset arm. A "one hook per kind" summary would
   have hidden the second.
2. **`FIFO_READ` has two call sites**, and the second (`dsp_dma.c:333`) **exists only because `AC-FIX
   (viii)` found the first one insufficient**. The original implementation had only `gp_ep.c:252`, which
   is unreachable at this pin — see `a4b1-r4-execution-rulings.md`. This is direct evidence that
   "every input path" is a claim that was **wrong once** and was corrected by the fixture, which is
   exactly why the criterion demands an enumeration rather than an assertion.

## 3. Static per-offset `PERIPH` classification

From the ported `read_peripheral` (`src/apu/dsp/dsp.c:52-91`). The function initialises
`uint32_t v = 0xababa` (`dsp.c:54`) and overwrites it only for the modelled offsets:

| Offset | Line | Classification |
|---|---|---|
| `0xFFFFB3` | `dsp.c:56` | **modelled** — `v = 0` (the core's instruction counter, itself commented `// ??` upstream) |
| `0xFFFFC5` | `dsp.c:59` | **modelled** — the interrupt register, computed from tracked state (`dsp->interrupts`, `dsp->dma.eol`) |
| `0xFFFFD4` | `dsp.c:65` | **modelled** — `dsp_dma_read(DMA_NEXT_BLOCK)` |
| `0xFFFFD5` | `dsp.c:68` | **modelled** — `dsp_dma_read(DMA_START_BLOCK)` |
| `0xFFFFD6` | `dsp.c:71` | **modelled** — `dsp_dma_read(DMA_CONTROL)` |
| `0xFFFFD7` | `dsp.c:74` | **modelled** — `dsp_dma_read(DMA_CONFIGURATION)` |
| **all other offsets in `[0, 128)`** | — | **stub/unknown** — the `0xababa` sentinel is returned unchanged |

**So 6 of the 128 peripheral offsets are modelled; the remaining 122 are stub/unknown.** The offset is
`address - DSP_PERIPH_BASE` (`0xFFFF80`), so index 0 is `0xFFFF80`, and the universe is
`DSP_PERIPH_SIZE = 128` (`dsp_cpu_regs.h:121`). Any read of a stub/unknown offset is a GP input this
port does not model — which is what `AC-INPUTS` exists to detect in `A4b2`.

## 4. Static per-FIFO source classification

Recorded in `src/apu/apu_watch.h` beside the universe definition, per Advisor ruling (C)(c):

| Index | Meaning | Source classification |
|---|---|---|
| **0..1** (`GP_INPUT_FIFO_COUNT` = 2) | the GP **input** FIFOs | **stub/unknown** — *"none modelled at the pin"*: the pinned read arm does not implement `buf_id` 0..3, so a read-direction transfer naming one falls through (its `assert` is `NDEBUG`-elided) and the DSP consumes the **stale intermediate buffer**. Any count > 0 is `AC-INPUTS` FAIL. Written by `dsp_dma.c:333` (gated on the DMA's `is_gp`) and `gp_ep.c:252` |
| **2..5** (the 4 output FIFOs' reserved places) | **GP-produced, not inputs** | **never recorded.** The fixture asserts they stay 0, as a guard against a future hook recording output traffic there |

**The two index spaces collide by design in `gp_fifo_rw`** — an output fifo 0 and an input fifo 0 both
arrive as `index == 0` (`gp_ep.c:219-233`). That is why the read arm uses `buf_id` directly for the
input slots while the output slots stay empty.

**Stated inference:** that a read-arm `buf_id` of 0..1 denotes input FIFO 0..1 mirrors the write arm's
`buf_id` 0..3 → output FIFO, and **no pinned source states it**. The classification does not depend on
it — the consumed data is the stale buffer whatever the id means, and both input slots are stub/unknown,
so any count > 0 fails `AC-INPUTS` either way. A read-arm `buf_id` outside 0..1 and `0xE`/`0xF` goes to
`GPIN_OUT_OF_UNIVERSE`, which fails closed to `UNKNOWN`.

---

## Why this record exists

The criterion asks for an enumeration because **"every path" is a completeness claim, and this packet
has already seen one such claim be wrong**: `AC-FIX (viii)` found that the FIFO read path had no hook at
all. An assertion that "the choke point is unique and every path is hooked" is exactly the kind of claim
that reads as satisfied while being false. Deriving the enumeration from source is what makes it
checkable — and it immediately surfaced the second `MIXBUF` call site and the second `FIFO_READ` call
site that a summary had hidden.
