# A4b1-r4 — AC-PORT step 4 enumeration

**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-26.
**Toolkit HEAD `3a3c7c1fa9461d6a9cc7279180aacaaaf979ab7d`.**

> ## CORRECTION — this file was factually wrong and is revised
>
> The **stage-1 acceptance reviewer** (`fb109c49-2039-434d-b665-a604e84cacae`) returned
> **`NOT ACCEPTED`** on `AC-PORT`, blocking on this file. It was right.
>
> **What was wrong.** The first version recorded `ep_scratch_rw` as **not** reaching the choke point,
> with the reason *"`scatter_gather_rw` writes only when `dir` is set and the transfer is the GP's"*.
> The second half of that reason was **false**: `scatter_gather_rw` (`gp_ep.c:85-159`) contains **no
> `is_gp` gate at all**. The only `is_gp` uses in `gp_ep.c` are `:351` (a comment), `:364`
> (`apu_watch_gp_bootstrap_done(dsp->is_gp)`) and `:366` (the bootstrap trace block) — **none in the
> write path**.
>
> **It was worse than the reviewer found.** Re-deriving the whole call graph showed the error was not
> one cell but **three**: `gp_fifo_rw` and `ep_fifo_rw` also reach the choke point, through
> `circular_scatter_gather_rw`. **All four pinned write callbacks reach it**, not one.
>
> **Root cause.** The generating script (`logs/a4b1/acport-step4.py`) *printed* the callback lines but
> **computed no reachability** — so the "reaches the choke point?" column was **hand-written**, and a
> hand-written column can be wrong independently of the code it describes. That is also the reviewer's
> advisory 2. The replacement (`logs/a4b1/acport-step4-derived.py` and the grep edges below) **derives
> the edges from the source**, so the column cannot drift from the code.
>
> **Why this matters beyond bookkeeping.** The criterion asks for *every* pinned write callback because
> "every" is a completeness claim. This packet has now had **two** such claims be wrong — the missing
> `FIFO_READ` hook (`AC-FIX (viii)`) and this enumeration. Both were caught by adversarial review rather
> than by the Session's own checking, which is precisely what the review stage exists for.

`AC-PORT` step 4 (packet line 246) requires the Session to **record**:

> the choke-point function and every pinned write callback that calls it, with source lines; every GP
> input path (peripheral read, DMA read, FIFO read, mix-buffer read) with the line where it calls the
> recording hook; and the static per-offset `PERIPH` classification and per-FIFO source classification
> (Device semantics 6).

---

## 1. The choke point

**`apu_gp_dma_write`** — defined at **`src/apu/apu_watch.c:385`**, declared at `src/apu/apu_watch.h:341`.
It does **not** distinguish GP from EP; it is called by both.

**Exactly one call site in production code:**

| Call site | Containing function |
|---|---|
| **`src/apu/dsp/gp_ep.c:144`** | `scatter_gather_rw` (defined `gp_ep.c:85`) |

## 2. Every pinned write callback, and whether it reaches the choke point

**Derived from the source, not asserted.** The call edges, each read from `gp_ep.c`:

| Edge | Line | Meaning |
|---|---|---|
| `gp_scratch_rw` → `scatter_gather_rw` | `:166` | GP scratch callback |
| `ep_scratch_rw` → `scatter_gather_rw` | `:175` | **EP** scratch callback |
| `gp_fifo_rw` → `circular_scatter_gather_rw` | `:261` | GP fifo callback |
| `ep_fifo_rw` → `circular_scatter_gather_rw` | `:327` | **EP** fifo callback |
| `circular_scatter_gather_rw` → `scatter_gather_rw` | `:196` | forwards `dir` |
| `scatter_gather_rw` → `apu_gp_dma_write` | `:144` | **the choke point**, when `dir` is set |

**The registered callbacks** (`dsp_init(d, gp_scratch_rw, gp_fifo_rw, true)` at `gp_ep.c:676`;
`dsp_init(d, ep_scratch_rw, ep_fifo_rw, false)` at `gp_ep.c:680`):

| Pinned write callback | Definition | Reaches the choke point? | Path |
|---|---|---|---|
| `gp_scratch_rw` | `gp_ep.c:161` | **YES** | `:166` → `:144` |
| **`ep_scratch_rw`** | `gp_ep.c:170` | **YES** *(was wrongly "no")* | `:175` → `:144` |
| **`gp_fifo_rw`** | `gp_ep.c:212` | **YES** *(was not listed)* | `:261` → `:196` → `:144` |
| **`ep_fifo_rw`** | `gp_ep.c:281` | **YES** *(was not listed)* | `:327` → `:196` → `:144` |

**All four reach it, and each only when `dir` is set** — the write arm is `if (dir) { apu_gp_dma_write(...) }`
at `:141-148`; the read arm takes `apu_gp_dma_read` instead.

**So `DS5`'s "every GP DMA write to guest memory, from every pinned write callback, passes through one
function" holds**, and more strongly than the first version of this file claimed: there is one choke
point, one call site, and **four** callbacks feeding it, of which two are the EP's.

**No other write path exists in `src/apu/**`** — **re-derived by the second-stage reviewer**
(`f3f02108-…`), who is the authority for this: every guest-memory writer in `src/apu/**` is
`stl/stw/stb_*_phys` (`apu_shim.h:181-192`), used **only** at `apu_core.c:165` (`FEMEMDATA`) and
`apu_vp.c:57,58,119,432,445,527` (VP) — exactly the two the packet permits (line 170). No
`ram_ptr[...] =` writes and no `MEM32` stores exist; `apu_vp.c:846` is a `memcpy` **from** `ram_ptr`,
i.e. a read, correctly excluded; the remaining `g_apu_ram_ptr` sites (`apu_watch.c:413/426/488`) are
inside the choke point itself, and `:994` is a read for the trace line. (The first version of this file
**asserted** this by reference to another document rather than showing it — the reviewer flagged that,
and re-derived it.)

### Consequence the reviewer identified, and the Session confirms

Because `ep_scratch_rw` and `ep_fifo_rw` reach the choke point, **an enabled EP can land an exchange on
`W_va` and take `GP_CLEAR`.** The EP is gated on `EPRST` being set (`gp_ep.c:650-651`) and runs only
every 8th frame (`:652`), but the path is real.

**This is a `A4b2` decision input and must be carried forward, not silently absorbed.** It is recorded
here and added to the plan's follow-ups. The Session does **not** rule on whether it is benign — that is
exactly the private judgment the reviewer declined to make, and `DS5`'s text ("every pinned write
callback") already requires the choke point to be shared, which it is.

---

## 3. Every GP input path, with the line that calls its recording hook

| Path | Recording function | **Call site(s)** |
|---|---|---|
| **`PERIPH`** — peripheral register read | `apu_gpin_periph_read` (`apu_watch.c:575`) | **`src/apu/dsp/dsp.c:87`** in `read_peripheral` |
| **`DMA_READ`** — guest-memory DMA read | `apu_gp_dma_read` (`apu_watch.c:508`), `apu_watch_boot_scratch_read` (`:547`) | **`gp_ep.c:110`** SGE descriptor fetch; **`gp_ep.c:146`** data read; **`gp_ep.c:135`** bootstrap scratch read (`apu_watch_boot_scratch_read`, the `AC-BOOT` presence witness) |
| **`FIFO_READ`** — FIFO read | `apu_gpin_fifo_read` (`apu_watch.c:647`) | **`gp_ep.c:252`** the `gp_fifo_rw` hook under `!dir`; **`dsp_dma.c:333`** the read-arm hook added by Advisor ruling (C) |
| **`MIXBUF`** — mix-buffer read | `apu_gpin_mixbuf_read` (`apu_watch.c:620`) | **`dsp_cpu.c:916`** and **`:922`** — **two** call sites in the pinned interpreter |

**Two facts this enumeration makes visible that a summary hides:**

1. **`MIXBUF` has TWO call sites** (`dsp_cpu.c:916`, `:922`).
2. **`FIFO_READ` has two call sites**, the second (`dsp_dma.c:333`) existing **only because `AC-FIX
   (viii)` found the first insufficient** — `gp_ep.c:252` is unreachable at this pin. See
   `a4b1-r4-execution-rulings.md`.

## 4. Static per-offset `PERIPH` classification

From the ported `read_peripheral` (`src/apu/dsp/dsp.c:52-91`). The function initialises
`uint32_t v = 0xababa` (`dsp.c:54`) and overwrites it only for the modelled offsets:

| Offset | Line | Classification |
|---|---|---|
| `0xFFFFB3` | `dsp.c:56` | **modelled** — `v = 0` ⚠️ *decision class superseded — see the note below* |
| `0xFFFFC5` | `dsp.c:59` | **modelled** — the interrupt register, from tracked state |
| `0xFFFFD4` | `dsp.c:65` | **modelled** — `dsp_dma_read(DMA_NEXT_BLOCK)` |
| `0xFFFFD5` | `dsp.c:68` | **modelled** — `dsp_dma_read(DMA_START_BLOCK)` |
| `0xFFFFD6` | `dsp.c:71` | **modelled** — `dsp_dma_read(DMA_CONTROL)` |
| `0xFFFFD7` | `dsp.c:74` | **modelled** — `dsp_dma_read(DMA_CONFIGURATION)` |
| **all other offsets in `[0, 128)`** | — | **stub/unknown** — the `0xababa` sentinel is returned unchanged |

**6 of 128 modelled; 122 stub/unknown.** The index is `address - DSP_PERIPH_BASE` (`0xFFFF80`), so index
0 is `0xFFFF80`; the universe is `DSP_PERIPH_SIZE = 128` (`dsp_cpu_regs.h:121`).

> ### ⚠️ Note — `0xFFFFB3`'s **decision class** is superseded
>
> **Added 2026-09-26 at the Advisor's request** (`docs/reviews/a4b2-r4-planning-rulings.md`, ruling (b)).
>
> The **table above describes the source**, and for `0xFFFFB3` that description is accurate: the ported
> `read_peripheral` does return a computed-looking `v = 0`. **But it is not a model of anything.**
> `dsp.c:57` reads `v = 0; // core->num_inst; // ??` — **upstream's own `// ??` marks it an unknown
> placeholder.**
>
> **For `AC-INPUTS` decision purposes the class is therefore STUB, not modelled.** A GP read of
> `0xFFFFB3` before the clear gives **FAIL → `R2-EXPL-INPUT`**, which names the input and routes to the
> Advisor — the honest result if the GP's clear followed a read of a placeholder.
>
> **The modelled set for decision purposes is FIVE offsets: `0x45` and `0x54`–`0x57`.**
>
> **Why this reopens nothing:** the "modelled" label above was a **Session record, not an Advisor ruling**,
> and `A4b1`'s acceptance did not decide `A4b2`'s `AC-INPUTS` semantics. **No A4b1 text or code changes.**
>
> **Claim limit inherited with the five:** `0x56` (`DMA_CONTROL`) is upstream's **read-count completion
> heuristic** (RUNNING→STOPPED after 3 reads) — a **timing heuristic, not a hardware model**.

## 5. Static per-FIFO source classification

Recorded in `src/apu/apu_watch.h` beside the universe definition, per Advisor ruling (C)(c):

| Index | Meaning | Source classification |
|---|---|---|
| **0..1** (`GP_INPUT_FIFO_COUNT` = 2) | the GP **input** FIFOs | **stub/unknown** — *"none modelled at the pin"*: the pinned read arm does not implement `buf_id` 0..3, so a read-direction transfer naming one falls through (its `assert` is `NDEBUG`-elided) and the DSP consumes the **stale intermediate buffer**. Any count > 0 is `AC-INPUTS` FAIL. Written by `dsp_dma.c:333` (gated on the DMA's `is_gp`) and `gp_ep.c:252` |
| **2..5** (the 4 output FIFOs' reserved places) | **GP-produced, not inputs** | **never recorded.** The fixture asserts they stay 0 |

**The two index spaces collide by design in `gp_fifo_rw`** — an output fifo 0 and an input fifo 0 both
arrive as `index == 0` (`gp_ep.c:219-233`). That is why the read arm uses `buf_id` directly for the input
slots while the output slots stay empty.

**Stated inference:** that a read-arm `buf_id` of 0..1 denotes input FIFO 0..1 mirrors the write arm's
`buf_id` 0..3 → output FIFO, and **no pinned source states it**. The classification does not depend on
it. A read-arm `buf_id` outside 0..1 and `0xE`/`0xF` goes to `GPIN_OUT_OF_UNIVERSE`, failing closed.
