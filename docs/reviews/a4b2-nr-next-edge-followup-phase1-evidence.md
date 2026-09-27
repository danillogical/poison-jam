# `A4b2-NR-next-edge-followup` — phase 1 evidence: the loader is identified, image `I` is stable

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-27, DSH.
**Mandate:** the Persistent Advisor's `N1`–`N3` (`docs/reviews/a4b2-nr-next-edge-advisor-ruling.md`) —
loader/epochs **first**, slice **second**; no slice on unproven-stable bytes.
**Instrument:** `RECOMP_APU_PWRITE_WATCH` (new, this packet). Run
`logs/runs/20260927-140605-352-a4b2-nrf-pwrite`.

**Status: Session phase-1 work, not a packet artifact.** It does not discharge a criterion.

---

## N1 — a second GPRST bootstrap is **ruled OUT**, from existing logs, as mandated

Before any new run, from run `20260927-135115-102-a4b2-nrf-decode2`:

| Check | Value |
|---|---|
| `[GPWATCH] counts … boots=` | **1** |
| `[GPBOOT] n=` blocks | **1** — `n=1 sge0=003C0000 sge0_va=803C0000 gprst=00000003 prev=00000001` |

**So the second image is not loaded by a second bootstrap.** It is loaded by something else — which is
what the watch was built to find.

## The loader is identified: **`P 011C`**, a DMA-triggered bulk load

The watch recorded **3 512 P-memory writes** between bootstrap and the first exchange:

| Quantity | Value |
|---|---|
| Total writes | **3 512** |
| Into image `I` (`0x000`–`0x172`) | **2** |
| Above image `I` (`0x173`–`0x0F28`) | **3 510** |
| Distinct addresses written | 3 510 above + 2 in |
| **Addresses written more than once** | **0** |
| Distinct writing PCs | **1** — `P 011C` |
| Terminal | `events=3512 in_image_i=2 above_image_i=3510 invalid=0` |

**The entire second image is written by a single PC, `P 011C`, once per address, sequentially** from
`0x171` upward to `0x0F28`:

```
ord=2   pc=011C addr=0173  000000 -> 021594     <- 0x173 was ZERO, now code
ord=3   pc=011C addr=0174  000000 -> 0CC4A0
...
ord=3504 pc=011C addr=0F21  CACACA -> 44E400     <- 0x800+ was memset fill, now code
ord=3511 pc=011C addr=0F28  CACACA -> 00000C
```

The `old` values confirm the two-image finding independently: the writes above `0x173` overwrite **zeros**
(below `0x800`) and **`0xCACACA`** (at and above `0x800`), exactly the fill pattern `dsp_c_init` lays down.

**What `P 011C` is.** In the boot image it decodes as `movep #$000001,x:$ffffd6` — a write of **1** to
`x:$FFFFD6`, which the toolkit models as **DMA_CONTROL**. So the load is a **DMA transfer triggered by the
GP program itself**: the GP writes DMA_CONTROL, the DMA engine moves a block into P-memory, and every
word of that transfer arrives through `write_memory_raw` (verified below). The PC recorded is the
triggering instruction, which is why one PC accounts for all 3 510.

**Consequence for the doorbell question:** the second image is loaded by **the GP's own code**, from a
source the GP sets up — not by a host-side or guest-side write into PRAM. That is a materially different
provenance from "the guest firmware pokes PRAM", and it is the fact the packet's phase 2 needs.

## Image `I` stability — **established, and the doorbell path is untouched**

**The two writes that land inside image `I` are at `0x171` and `0x172`** — the last two words of the boot
image, i.e. the **boundary** where the second image begins:

| ord | pc | addr | old | new |
|---|---|---|---|---|
| 0 | `011C` | `0171` | `000000` | `65F400` |
| 1 | `011C` | `0172` | `00000C` | `000080` |

**No write lands anywhere in `0x000`–`0x170`.** Verified against the doorbell path's own instruction
addresses (`P 0000`, `0002`, `0004`, `0006`, `0007`, `0009`, `000B`, `000D`, `000E`, `00B9`, `00BB`,
`00DB`, `00E8`, `00EA`): **NONE was written.**

**This is what closes the Advisor's Q2 transient-modification gap.** The earlier evidence was that the
doorbell instructions were byte-identical in **two samples**; the watch now shows they were **not written
at all** between bootstrap and the first exchange — which is the stronger statement the Advisor required,
and it is a **watch**, not a second sample.

## Coverage audit — the Planner's caveat, verified, and the residual now **instrumented**

The Planner raised two hazards against the first version of the watch. **Both were correct**, and both are
now fixed in the instrument rather than argued away in prose.

### Hazard 1 — the watch logged every core and merely *recorded* `is_gp`; it did not *filter*

**Correct.** The first version wrote an `is_gp` column but still counted EP writes as events, which would
corrupt a quiescence argument if the EP ever wrote PRAM. **Fixed:** the watch now resolves
`DSPState.is_gp` through the core's `opaque` back-pointer (not `core->is_gp`, which is never populated) and
**returns early for non-GP cores**. Non-GP writes are **counted, not silently dropped** —
`ngp_skipped` appears in the terminal, so "the EP was quiet" is *visible* rather than assumed.

**Measured on the re-run** (`20260927-140820-853-a4b2-nrf-pwrite2`):
`events=3512 in_image_i=2 above_image_i=3510 ngp_skipped=0` — so all 3 512 events are GP, and the
filter changes no number. The point is that the instrument is now **correct by construction** rather than
correct because the EP happened not to run.

### Hazard 2 — PRAM mutation paths that bypass `write_memory_raw`

**Correct, and the more serious of the two.** A watch at one choke point cannot see writes that never
reach it. All PRAM mutation paths in the toolkit, with their disposition:

| # | Path | Covered by the watch? |
|---|---|---|
| 1 | `write_memory_raw()` P branch | **YES** — the watch site |
| 2 | DMA writes → `c_dma_mem_write` → `dsp56k_write_memory` → `write_memory_raw` (`dsp_c.c:45-49`, `dsp_cpu.c:1579-1585`) | **YES** — verified by reading the chain |
| 3 | `dsp_c_bootstrap` `scratch_rw` into `core->pram` (`dsp_c.c:121`) | **NO — now INSTRUMENTED** (`dsp56k_pwrite_note_bootstrap_bulk`) |
| 4 | `dsp_c_sync_from_vm` `memcpy(core->pram, …)` (`dsp_c.c:262`) | **NO — now INSTRUMENTED** (`dsp56k_pwrite_note_sync_bulk`); also has **zero callers** |
| 5 | `dsp_c_init` `memset(core->pram, 0xCA, …)` (`dsp_c.c:299`) | **NO** — init-time, before the watch's epoch; positively confirmed as the source of the observed `0xCACACA` |

**Paths 3 and 4 now emit an explicit marker into the watch artifact** when they run, so a bypass is
**recorded rather than silent**. The re-run's artifact contains:

```
# BOOTSTRAP_BULK_LOAD words=2048 (bypasses write_memory_raw)
```

with **no** `SYNC_FROM_VM_BULK` marker — exactly as expected (`boots=1`; `sync_from_vm` has no callers).

**The honest residual, stated rather than hidden.** This is a **source-level** audit plus instrumentation
of the two known bypasses. It does not mechanically prove that no *future* or *conditional* path writes
PRAM outside the watch, and path 5 is excluded by timing rather than by a hook. The Advisor's `F3` governs:
if PRAM is later shown not to quiesce, the answer is per-epoch slicing or `O-INCONCLUSIVE` — **never a
fudged single-image slice**.

## Closure — performed and verified

**`bad-output` arm removed.** The known-bad control is gone from the perturbation selector, the
choke-point classification, the header enum and the fixture, and its CTest registration was replaced by
the watch arm (count unchanged at 18). **Its bite was preserved before removal** and remains archived:
run `20260927-130142-334-a4b2-nr-badoutput` — `GP_CLEAR` latches **0**, `GP_NONZERO_OVER` latches **1**,
two control log lines. The remaining modes (`zero`/`max`/`prng`) are untouched: they substitute the two
stub inputs and never touched that classification.

**New fixture arm (xiii) for the write watch — and it passed vacuously on its first version.** The arm
writes one word inside image `I` (`0x40`) and one above it (`0x900`) through the same interpreter-core
route the live run uses, then reads its own artifact back and asserts the terminal's counts.

The first version reported **722 checks green while the watch recorded ZERO events**: the fixture passed
`d->gp.dsp` (a `DSPState *`) where `dsp56k_write_memory` requires a `dsp_core_t *`. That compiles through a
`void *` extern, and every write was classified non-GP and filtered — `ngp_skipped=2`, `events=0`. **A
green test that measured nothing.** Fixed by fetching `DSPState.backend` (`dsp.h:117`) and passing the real
core. The arm now asserts the **recorded counts** (`events=2 in_image_i=1 above_image_i=1 ngp_skipped=0
invalid=0`) rather than merely that the call returned, and prints an `AC-PWRITE` marker that the game
`CMakeLists.txt` matches via `PASS_REGULAR_EXPRESSION`.

Two further details worth recording: the marker goes to **stdout, not stderr**, because the fixture
`freopen`s its own stderr — a stderr-based `PASS_REGULAR_EXPRESSION` silently cannot match. And the guard
was verified by **negative control**: with the gate absent, the marker is **not printed at all**.

**Closure control — PASSED.** One fresh absent-gate baseline at the final identity
(`logs/runs/20260927-141552-900-a4b2-nr-next-edge-followup-inert`), profile `EXPLORATORY`:

| Check | Result |
|---|---|
| `[GPPERTURB]` lines | **0** |
| `[GPDECODE]` lines | **0** |
| `[GPB9]` lines | **0** |
| `[GPWRITE]` lines | **0** |
| artifacts created | **none** (`gpwrite_watch.txt`, `gpb9_trace.txt` both absent) |
| doorbell tuple | `seq=198852 va=803C0810 observed=00000003 payload=00000000 dsp_addr=000800` — **unchanged** |

So every diagnostic gate is inert when unset, no watch is left behind, and the final identity reproduces
the baseline behaviour exactly. **ctest 18/18 green.**

## What this establishes, and what it does not

**Establishes:**
- The second image is loaded by a **DMA transfer triggered by the GP's own `P 011C`** — one writer, 3 510
  distinct addresses, **no address written twice**, so the load is a single clean bulk pass.
- **Image `I` is stable from bootstrap to the first exchange**: exactly 2 writes, both at the boundary
  (`0x171`, `0x172`), and **zero** writes to `0x000`–`0x170`.
- **The doorbell path's instructions were not modified in that window** — the Q2 transient gap is closed.
- The two-image finding is **independently confirmed** by the watch's `old` values (zeros below `0x800`,
  `0xCACACA` above), which is a third, different measurement of the same fact.

**Does not establish:**
- Whether PRAM **quiesces after** the first exchange. The watch's window ends there by design; a slice
  covering later execution needs its own epoch evidence.
- The **source** of the DMA transfer (guest memory at what VA, or a device-side buffer) — the watch sees
  the destination writes, not the source bytes.
- Anything about the doorbell's **dependence** on the stub inputs. Per `F1`, the doorbell instructions'
  survival is **not** path-independence: who calls `P 0000`–`0007` and what guards the trigger remain open.
