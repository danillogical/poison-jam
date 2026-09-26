# A4b1-r2: premise re-check at the accepted A4s baseline

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Purpose:** identify **which A4b1-r3 premises actually depended on the old toolkit baseline**
(`0d7929c`), which are unchanged, and which measurements must be repeated — so the Planner's
`A4b1-r2` brief carries measured facts rather than assumptions. This is **reconnaissance for planning**,
not the investigation itself; behaviour questions belong in the packet.
**Old baseline:** toolkit `0d7929c86771dd0b971941592fd4f15436116e82` (what `A4b1-r3` assumed).
**New baseline:** toolkit **`M` = `3f8bf67c450861aefcbc376698750bc1446bc9dd`** (A4s accepted, pushed).
**Scripts:** `logs/a4b1/premise-recheck.py`, `premise-classification.py`, `premise-semantics.py`,
`mixdown-guest-visibility.py`, `apu-state-host-only.py`, `mmgetphysical-return.py`.

## What A4s actually changed that A4b1 cares about

`A4b1` rewrites `src/apu/apu_dsp.c`, `apu_core.c`, `apu_state.h`, `apu/CMakeLists.txt`, `apu.h` and
`apu_mmio_hook.c`. Against that list, A4s changed exactly **two** files:

| File A4b1 touches | Changed by A4s? |
|---|---|
| `src/apu/apu_dsp.c` | **CHANGED** (+56/−3) |
| `src/apu/CMakeLists.txt` | **CHANGED** (+7/−1) |
| `src/apu/apu_core.c` | unchanged (blob identical) |
| `src/apu/apu_state.h` | unchanged (blob identical) |
| `src/apu/apu.h` | unchanged |
| `src/apu/apu_mmio_hook.c` | unchanged |

**So the APU surface A4b1 rewrites is almost entirely untouched** — which is why this is a premise
re-check and not a redesign.

## Premise classification

| # | Premise (from `a4b1-gp-core-port.md:20-32`) | Verdict at `M` | Measured |
|---|---|---|---|
| P-A | `apu_dsp.c` has no DSP core and carries the synthetic `dsp_ack_frame` | **HOLDS** (file grew 167→219) | `dsp_ack_frame`/`dsp_ack_init`/`RECOMP_APU_DSP_ACK` each still present (2 occurrences); `dsp_run`/`dsp_start_frame` still **absent**; A4s **added** `mcpx_apu_mixdown_all()` + `RECOMP_APU_MIXDOWN_ALL` |
| P-B | GP/EP writes dropped, reads return 0 (`apu_core.c:618,637`) | **UNCHANGED** | `apu_core.c` **byte-identical** |
| P-C | `apu_state.h` carries stale `DSPState` layouts | **UNCHANGED** | `apu_state.h` **byte-identical** |
| P-D | Guest MMIO reaches the APU only under `RECOMP_APU_TRAP` | **HOLDS** (line numbers shifted 1728→2115) | the trap gate is textually identical; A4s changed the AC'97 model around it, not the gate |
| P-E | The `0x80000000` window is separate storage, not an alias | **HOLDS** | window-mapping lines unchanged; occurrence count rose 5→7 via the AC'97 region |
| P-F | `MmGetPhysicalAddress` returns the VA (`kernel_bridge.c:1700-1707`) | **CHANGED IN FORM — premise STRENGTHENED** | see below |
| P-G | The GP runs on the APU frame thread (`apu_core.c:557`) | **UNCHANGED** | `apu_core.c` byte-identical |
| P-STOP | The strict stop is still the `loc_001A18D0` spin | **UNCHANGED** | A4s `R-SAME`: `B=0x803C0000`, `W=3`, `F=2` |
| **P-NEW-1** | **`RECOMP_APU_MIXDOWN_ALL` is default-ON inside `apu_dsp.c`** — the file A4b1 rewrites | **NEW PREMISE** | resolved below |
| **P-NEW-2** | `RECOMP_USB_PORT` (`ohci.c`) | **NEW, OUT OF A4b1 SCOPE** | inventoried only; `ohci.c` is not in A4b1's write scope |

### P-F — the one premise whose *form* changed, and it moved in A4b1's favour

**Old (`0d7929c`)** — the bridge returned the VA itself, and its comment warned against the call:

```c
static void bridge_MmGetPhysicalAddress(void)
{
    uint32_t addr = STACK_ARG(0);
    /* Xbox uses identity mapping (physical == virtual) for the lower 64MB.
     * Just return the Xbox VA as-is. Don't call xbox_MmGetPhysicalAddress
     * which would return a native pointer. */
    g_eax = addr;
}
```

**New (`M`)** — the bridge delegates, and the warning is gone:

```c
    g_eax = (uint32_t)xbox_MmGetPhysicalAddress((PVOID)(uintptr_t)addr);
```

**Why this mattered enough to check.** `A4b1` Device semantics 3 requires one translation function
whose comment *"names it the inverse of `bridge_MmGetPhysicalAddress`"*. The deleted comment claimed the
delegate *"would return a native pointer"* — which, if true, would have destroyed the premise A4b1's
whole address model rests on, and forced a packet redesign.

**It is not true.** The delegate (`kernel_memory.c:166-185`) is:

```c
ULONG_PTR __stdcall xbox_MmGetPhysicalAddress(PVOID BaseAddress)
{
    uint32_t va = (uint32_t)(uintptr_t)BaseAddress;
    return (ULONG_PTR)((va >= XBOX_CONTIG_BASE &&
                        (uint64_t)va < (uint64_t)XBOX_CONTIG_BASE + XBOX_CONTIG_SIZE)
                     ? va - XBOX_CONTIG_BASE : va);
}
```

It returns **the VA unchanged outside the contiguous arena**, and `va - XBOX_CONTIG_BASE` inside it —
**no native pointer**. So:

- A4b1's premise *"`MmGetPhysicalAddress` returns the VA"* **holds for the region A4b1 cares about**;
- the address model is now **better founded**, because A4s removed a real inconsistency: the comment
  says the two paths *"used to disagree, with the bridge translating and this returning its argument
  unchanged as a placeholder, so the answer a title got depended on which dispatch path it took"*;
- **but** the inverse is now `va - XBOX_CONTIG_BASE` inside the contiguous window, so A4b1's
  `apu_guest_dma_ptr` must be the inverse of **that** function, not merely "return the VA". This is a
  **wording/precision change to Device semantics 3**, not a redesign.

### P-NEW-1 — the `MIXDOWN_ALL` question is RESOLVED by source read, not deferred

The plan (`plan-jsrf-bare-minimum.md:490-493`) recorded this as **uncertain and load-bearing**:

> *"Uncertain and load-bearing: whether `MIXDOWN_ALL`'s default-on path writes anything the guest reads
> back — the Advisor did not verify that `monitor.frame_buf` is guest-invisible. If it is guest-visible,
> it is new default-on device behaviour, and `A4b1` (which rewrites `apu_dsp.c`) must classify it before
> any strict APU claim."*

**Measured: `monitor.frame_buf` is host-only storage. It is NOT guest-visible.**

- it is declared `int16_t frame_buf[256][2]` at `src/apu/apu_state.h:500`, inside the `monitor` struct
  (L498-503) of `MCPXAPUState` — a **host** struct;
- `MCPXAPUState` instances are host allocations: `static MCPXAPUState *g_state` (`apu_core.c:33`),
  `MCPXAPUState *g_apu_state` (`apu_mmio_hook.c:16`), and `mcpx_apu_init_standalone(uint8_t *ram_ptr)`
  (which takes the *guest RAM base* as an argument, rather than returning a guest VA);
- **zero** hits for `XBOX_TO_NATIVE(...monitor...)`, `MEM8/16/32(...monitor...)`,
  `frame_buf ... BRIDGE_MEM`/`MEM32`, `monitor.*guest`, `guest.*monitor`;
- its only consumers are host-side: `memcpy` into the audio output buffer (`apu_core.c:302, 352`) and
  `mixer_render`.

**Consequence for `A4b1-r2`:** the `MIXDOWN_ALL` default-on path writes **only** host monitor storage.
It is therefore **not** new default-on *device* behaviour, and it does **not** require a new A4b1
criterion. It remains a `jsrf-run-profiles.md` **classification** item (a new unclassified variable),
which is a **documentation** follow-up already recorded in the plan — not a packet obligation.

## What this means for the Planner (handed to it as measured input)

- **Re-measure (cheap, at the baseline):** the four Readiness facts of `a4b1-gp-core-port.md:26` at `M`,
  since the packet's own text says to; and the baseline exe SHA / ctest count, which the packet already
  requires at promotion.
- **Re-read, do not re-measure:** the `apu_dsp.c` region A4b1 rewrites, because A4s added ~56 lines to it
  — **line numbers in the packet are now stale** and every anchor must be re-derived from text.
- **Admissible without repetition:** every `A4a` R0/R1 run observation, the strict stop, the watch-ledger
  ruling, the Q1/Q2 rulings, the checkpoint-40 constraints, and the xemu pin record — none of these
  depended on the toolkit baseline's APU internals.
- **Genuinely new and needing a decision:** (i) Device semantics 3's inverse must account for
  `XBOX_CONTIG_BASE`; (ii) `MIXDOWN_ALL` must be *classified* somewhere, and since it lives in the file
  A4b1 rewrites, `A4b1-r2` is the natural place to record the disposition even though no criterion
  changes.
- **Do NOT** treat "A4s changed 4814 lines under `src/kernel/`" as a reason to redesign `A4b1`: the
  kernel changes are the event/AC'97/NABM work, and `A4b1`'s own write scope is the APU.

## Limits of this record

- It is a **source read**, not an execution. It establishes what the code *says*, which is what the
  planning question needed.
- It does **not** establish that JSRF's GP/audio behaviour is unchanged, and it makes **no** guest-run
  claim. Those are `A4b2`'s.
- It does **not** re-run any A4s acceptance or merge check; it re-measures only premises whose truth can
  affect the `A4b1` decision.
