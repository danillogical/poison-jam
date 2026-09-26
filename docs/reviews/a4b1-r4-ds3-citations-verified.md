# A4b1-r4: the DS3 citations are verified — with one ordering nuance

**Prepared by:** Session `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`, 2026-09-25.
**Script:** `logs/a4b1/verify-ds3-citations.py`. **Source read at `M`.** Supports
`docs/reviews/a4b1-r4-planning-rulings.md` part 4(b).

The Advisor's part 4(b) tells DS3 to cite *"the toolkit precedent `nv2a_pb_exec.c dma_resolve` and not
only `xbox_memory_layout.c:843`"*, and its rule uses `xbox_ContiguousAllocatedBytes()` as the
high-water mark. All three are checked rather than carried forward.

## All three exist and do what the ruling assumes

**`dma_resolve` — `src/kernel/nv2a_pb_exec.c:88`** (with a substantial comment at L67-87):

```c
static uint32_t dma_resolve(uint32_t offset)
{
    extern uint32_t xbox_ContiguousAllocatedBytes(void);
    if (offset < xbox_ContiguousAllocatedBytes())
        return XBOX_CONTIG_BASE + offset;
    if (!surface_hits_image(offset, 1))
        return offset;
    if ((uint64_t)offset < XBOX_CONTIG_SIZE)
        return XBOX_CONTIG_BASE + offset;
    return offset;                         /* nothing better to offer */
}
```

**`xbox_ContiguousAllocatedBytes` — `src/kernel/xbox_memory_layout.c:2694`** (declared in
`xbox_memory_layout.h:198`):

```c
/* How much of the window has been handed out.
 *
 * Lets a caller holding a physical address decide whether it names contiguous
 * memory this runtime allocated. … */
uint32_t xbox_ContiguousAllocatedBytes(void)
{
    return g_contig_next - XBOX_CONTIG_BASE;
}
```

**The `:843` precedent** — unchanged, as previously recorded.

**So the ruling's premise holds:** the toolkit already has the physical-address → window rule, with the
high-water mark available as a function, and `dma_resolve`'s own comment explains the exact failure mode
the ruling is guarding against (*"Half-Life 2's colour surface is physical 0x00A6C000, which clears the
image by 700 KB. So it looked like an ordinary VA, and the executor cleared 1.2 MB of black straight
through the guest heap"*).

## The ordering nuance the Planner must not miss

**The two rules check their conditions in a different order, and that is deliberate:**

| | `dma_resolve` (a physical offset in) | the Advisor's DS3 rule (either form in) |
|---|---|---|
| 1st | `offset < high-water` → `XBOX_CONTIG_BASE + offset` | **inside the window → use as is** |
| 2nd | `!surface_hits_image` → identity | `P < high-water` → `XBOX_CONTIG_BASE + P` |
| 3rd | `offset < XBOX_CONTIG_SIZE` → window | mapped low-RAM → identity |
| 4th | `return offset` | fail closed |

`dma_resolve` can put the high-water test first **because its input is always a physical offset**.
DS3's inverse cannot: it must **accept both forms**, so it has to recognise an already-a-window-VA
*before* testing the high-water mark — otherwise a window VA below the high-water mark would be
double-translated. The Advisor states exactly this reason: rule 1 *"keeps the `0d7929c` form working."*

**Consequence for the packet:** DS3 should **cite** `dma_resolve` as the precedent for the high-water
rule and the failure mode, **not copy its ordering.** Copying the order would break the `0d7929c` form
that `A4a`'s R1 observed.

## Also confirmed

The `surface_hits_image` test in `dma_resolve` is an **NV2A/framebuffer concern** (does the offset land
on the title's image?), which has no counterpart in the APU DMA path. DS3's rule 4 ("otherwise fail
closed as r3 already specifies") is the right analogue, and the Advisor did not ask DS3 to adopt
`surface_hits_image`. Recorded so the omission is a decision, not an oversight.

## Limits

Source read only; no execution. It establishes that the cited symbols exist and behave as the ruling
assumes. It does **not** establish anything about the guest's runtime behaviour.
