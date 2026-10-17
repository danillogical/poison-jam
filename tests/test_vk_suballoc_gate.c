/* Plan N4.4's gate: the Vulkan back end's memory sub-allocator, decided on its own bodies.
 *
 * What this case decides
 * ----------------------
 * N4.4 replaces one `vkAllocateMemory` per resource with pooled sub-allocation: several
 * images or buffers with compatible requirements live in ONE `VkDeviceMemory` block at
 * distinct, aligned, non-overlapping ranges, and a released range is handed out again when
 * nothing that could still read it is recording or in flight. This case drives those
 * decisions from the allocator's own state (`g.pools`, the blocks' free lists, the write-once
 * latches) and from the counters `nv2a_vk_backend_memory_stats()` publishes:
 *
 *   1. sharing and the allocation reduction -- three images' binds land in one block at
 *      distinct, 256-byte-aligned, non-overlapping offsets, the block's free-list arithmetic
 *      adds up, `blocks == 1` and `alloc_calls == 1` while eleven images were created, and
 *      the same claim is made for a host-visible byte workload whose bytes really are
 *      written and read back through the pooled ranges. The counters' `alloc_calls` is
 *      cross-checked against the fixture's own `vkAllocateMemory` stub count, so the number
 *      the allocator reports is the number the driver was really handed.
 *   2. memory types and alignment -- two requests whose `memoryTypeBits` select the same
 *      type share a block; a third whose bits cannot select that type does NOT; every range
 *      is aligned to its own requirement (65536 here, deliberately above `VB_POOL_ALIGN`),
 *      and `used`/`padding`/`free`/`largest_free`/`reserved` agree exactly with what those
 *      two allocations did to a virgin block.
 *   3. linear and non-linear pools -- an image and a buffer with IDENTICAL requirements never
 *      share a block, and the two pools' `nonlinear` flags differ. That separation is what
 *      makes `VkPhysicalDeviceLimits::bufferImageGranularity` inapplicable between
 *      neighbours.
 *   4. reuse and coalescing -- a released middle range is the range handed out next (the
 *      write-once `first_reuse_off`/`first_reuse_block` latch says which), `reuse_hits`
 *      counts it, three released adjacent ranges come back as one range wide enough for all
 *      three, and the free lists never double-count.
 *   5. dedicated allocations -- a range over `VB_POOL_MAX_BLOCK`, a
 *      `requiresDedicatedAllocation` and a `prefersDedicatedAllocation` request all end as a
 *      dedicated `VkDeviceMemory` with the matching `VkMemoryDedicatedAllocateInfo` in
 *      `VkMemoryAllocateInfo.pNext` (the fixture's stub reads the pNext), a request that
 *      cannot get a coherent host type is dedicated too (`dedicated_nohost`), five
 *      `VB_POOL_MAX_BLOCK` requests exhaust the pool's `VB_POOL_RESERVE` and the pool stops
 *      growing there, and every dedicated reason counter sums to `dedicated`.
 *   6. delayed reuse -- a range released while a recording is open, and one released while a
 *      submission is pending, is NOT handed out again; the fixture writes a pattern into that
 *      range's mapped bytes before the release and requires them byte-identical afterwards;
 *      once the recording is closed, the ring drained and `vb_pool_gc()` has run, the range
 *      IS the one handed out next.
 *   7. rollback -- injected `vkAllocateMemory`, `vkCreateImage`/`vkCreateBuffer`,
 *      `vkBindImageMemory`/`vkBindBufferMemory` and `vkMapMemory` failures each release
 *      exactly what was taken, leave the caller's `VbAlloc` unusable, free nothing twice, and
 *      leave no range dangling: the next successful take gets the same range back, or the
 *      next dedicated allocation gets a new object.
 *   8. host-visible mapping and the atom-size widening -- a pooled host-visible range's
 *      pointer is its block's single mapping plus the range's offset; a coherent range's
 *      flush/invalidate covers exactly the requested sub-range and cannot reach its
 *      neighbour; a non-coherent allocation's window is widened to whole
 *      `nonCoherentAtomSize` atoms and clipped to the allocation, so it never covers a byte
 *      the allocation does not own.
 *
 * The production bodies are the toolkit's own
 * ------------------------------------------
 * This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with
 * XBR_VK_BACKEND defined, so every function driven here -- vb_mem_take, vb_mem_give,
 * vb_pool_gc, vb_mem_flush, vb_mem_invalidate, vb_mem_requirements, vb_host_buffer,
 * vb_device_buffer, vb_image_create, vb_image_discard, vb_discard -- is compiled from the
 * production source, verbatim. Nothing is copied, so nothing can drift: the build recompiles
 * these bodies whenever that file changes, and scripts/vk-suballoc-gate.py probes the same
 * bytes at build time and FAILS THE BUILD (exit 2) if a definition, a struct, a field, a
 * constant or a counter this fixture computes with is gone. That script also derives the
 * seam markers in vk_suballoc_gate_seam.h from those bytes; a marker that is 0 is reported by
 * the property that needs it as a FAILED CHECK naming the marker, never as a skip.
 *
 * Nothing of the toolkit is linked: the file's five undefined names are defined at the bottom
 * of this TU (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack), and every Vulkan entry point it uses is a
 * static function pointer (VK_NO_PROTOTYPES) that this TU points at controlled stubs before
 * the first call -- including vkGetDeviceProcAddr, through which the allocator resolves its
 * optional vkGet*MemoryRequirements2, vkFlushMappedMemoryRanges, vkInvalidateMappedMemoryRanges
 * and vkUnmapMemory.
 *
 * The injected results are STUBBED DRIVER OUTCOMES
 * ------------------------------------------------
 * There is no GPU here, no ICD, and no Vulkan allocation of any kind: vkAllocateMemory returns
 * a handle this fixture made up, vkMapMemory returns an arena this fixture allocated, and a
 * VK_ERROR_OUT_OF_DEVICE_MEMORY below is a stub's return value, NOT real device loss or a real
 * out-of-memory. The "[GPUVK] ... failed" lines on stderr are the production code reporting
 * those injected outcomes.
 *
 * Expected verdicts
 * -----------------
 * The N4.4 source passes every property. The pre-N4.4 source does not build this case at all
 * (the probe's anchors are the allocator's own names), which is the honest verdict for a gate
 * that binds to a design that does not exist yet. Each weakening mutant of the N4.4 source
 * named in the packet -- alignment ignored, memoryTypeBits ignored, linear and non-linear
 * pools merged, a range reused while a recording or a submission is live, a broken rollback,
 * a dedicated requirement ignored -- fails the property that decides it, and a source that
 * disables pooling (one allocation per resource) loses properties 1's reduction checks while
 * every byte check stays green: the allocation-reduction lines are named "alloc:" and the
 * byte lines "bytes:" so that distinction is mechanical.
 *
 * There is no skip mode and no SKIP_RETURN_CODE in the build: the seam is inside this
 * translation unit, so the case always runs, and anything it cannot do is a FAILURE, exit 1.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"           /* the production bodies, verbatim */

#include "vk_suballoc_gate_seam.h"     /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture's own numbers ───────────────────────────────────────────────────────── */

#define GX_ATOM       64u                    /* the atom size the fixture's device reports */
#define GX_IMGS       11u                    /* resources property 1's reduction is claimed over */
#define GX_BUFS       8u                     /* the byte workload's buffer count */
#define GX_PATTERN    0x9E3779B97F4A7C15ull
#define GX_MAX_MEM    128
#define GX_MAX_OBJ    128
#define GX_MAX_LIVE   256
#define GX_MAX_BIND   128

#define GX_BIT_DEV    0x1u                   /* memory type 0: device-local */
#define GX_BIT_HOST   0x2u                   /* memory type 1: host-visible, coherent, cached */
#define GX_BIT_NC     0x4u                   /* memory type 2: host-visible, NOT coherent */
#define GX_BIT_DEVHOST 0x8u                  /* memory type 3: device-local and host-visible */

#define GX_WANT_DEV   (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
#define GX_WANT_HOST  (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | \
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
#define GX_WANT_HOSTVIS (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)

/* ── The fixture's stub state ────────────────────────────────────────────────────────── */

typedef struct {
    VkDeviceMemory mem;
    VkDeviceSize size;
    uint8_t *arena;                       /* what vkMapMemory handed out, NULL once unmapped */
    unsigned frees;                        /* times this object was freed */
    int registered;
} GxMem;

typedef struct {                           /* a created image or buffer, with the requirements
                                            * the stub reports for it */
    int is_image;
    VkImage img;
    VkBuffer buf;
    VkDeviceSize size, align;
    uint32_t bits;
    int requires_ded, prefers_ded;
} GxObj;

typedef struct { VkDeviceMemory mem; VkDeviceSize off, size; } GxLive;

typedef struct {
    /* what the fixture asked for */
    VkDeviceSize size, align;
    uint32_t bits;
    int requires_ded, prefers_ded;
} GxSpec;

typedef struct {
    /* call counts */
    unsigned allocs, frees, maps, unmaps, procs;
    unsigned creates_img, destroys_img, creates_buf, destroys_buf, creates_view, destroys_view;
    unsigned binds_img, binds_buf, flush_calls, inval_calls, reqs2_calls;
    /* injected outcomes -- STUB return values, not a device's */
    unsigned alloc_fail_at;                /* the 1-based vkAllocateMemory call that fails */
    unsigned alloc_fail_times;             /* how many calls from there fail (0 = none) */
    VkResult alloc_fail_result;
    VkResult create_image_result, create_buffer_result;
    VkResult bind_image_result, bind_buffer_result;
    VkResult map_result, create_view_result;
    VkResult flush_result, inval_result;
    unsigned reqs2_available;              /* 0: the resolver answers NULL for the *2 names */
    /* what the stubs saw */
    VkDeviceSize last_alloc_size;          /* the size the last vkAllocateMemory was asked for */
    uint32_t last_alloc_type;
    unsigned last_alloc_had_dedicated;     /* pNext was a VkMemoryDedicatedAllocateInfo */
    VkImage last_alloc_dedicated_image;
    VkBuffer last_alloc_dedicated_buffer;
    unsigned type_calls[8];
    unsigned dedicated_pnext_seen;
    unsigned flushes_after_free, maps_after_free, double_frees;
    VkDeviceSize bind_img_off[64], bind_buf_off[64];
    VkDeviceMemory bind_img_mem[64], bind_buf_mem[64];
    VkImage bind_img_img[64];
    VkBuffer bind_buf_buf[64];
    unsigned bind_violations;              /* a bind that is unaligned, out of its block, of a
                                            * memory type its requirement forbids, or that
                                            * OVERLAPS another live range of the same block */
    char bind_violation[256];
    unsigned bind_n;                       /* every bind this fixture saw, in order: the
                                            * cross-bind overlap check is over these */
    VkDeviceMemory bind_mem[GX_MAX_BIND];
    VkDeviceSize bind_off[GX_MAX_BIND], bind_size[GX_MAX_BIND];
    int bind_is_image[GX_MAX_BIND];
    VkMappedMemoryRange flush[16], inval[16];
    unsigned n_flush, n_inval;
} Gx;

static Gx sx;
static GxMem g_mem[GX_MAX_MEM];
static GxObj g_obj[GX_MAX_OBJ];
static unsigned g_nobj;
static GxLive g_live[GX_MAX_LIVE];
static unsigned g_nlive;
static GxSpec g_spec;                      /* what the NEXT created object reports */
static GxSpec g_spec_default;
static uint8_t *g_snap;                    /* a range's bytes, copied before a call */

/* ── The fixture's own ledger of what it took ──────────────────────────────────────────
 *
 * Every take this fixture makes is recorded here with the EXACT range the allocator handed
 * back, and removed when it is given back while nothing is recording or pending (the
 * allocator's own rule, which the fixture sets and therefore knows). It is the gate's
 * independent oracle for `used_bytes`, for the largest hole it can see, and for overlaps --
 * quantities the counters would otherwise only be compared with themselves. */

typedef struct {
    VkDeviceMemory mem;
    VkDeviceSize off, size;
    int parked;                             /* released while the ring was busy */
    int lost;                               /* the park list could not hold it: never reusable */
} GxLedger;

static GxLedger g_ledger[GX_MAX_LIVE];
static unsigned g_nledger;

static void ledger_add(const VbAlloc *a)
{
    if (a->mem == VK_NULL_HANDLE || g_nledger >= GX_MAX_LIVE)
        return;
    g_ledger[g_nledger].mem = a->mem;
    g_ledger[g_nledger].off = a->off;
    g_ledger[g_nledger].size = a->size;
    g_ledger[g_nledger].parked = 0;
    g_ledger[g_nledger].lost = 0;
    g_nledger++;
}

/* The fixture's own mirror of the rule it decides: the release that the bounded park list
 * cannot hold is never reusable, so it stays in the ledger for good. */
static void ledger_lost(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nledger; i++)
        if (g_ledger[i].mem == a->mem && g_ledger[i].off == a->off) {
            g_ledger[i].lost = 1;
            return;
        }
}

static uint64_t ledger_sum(void)
{
    uint64_t sum = 0;
    unsigned i;

    for (i = 0; i < g_nledger; i++)
        sum += g_ledger[i].size;
    return sum;
}

static int ledger_has(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nledger; i++)
        if (g_ledger[i].mem == a->mem && g_ledger[i].off == a->off && g_ledger[i].size == a->size)
            return 1;
    return 0;
}

/* A give the fixture performed itself. While the back end is idle the allocator releases the
 * range at once, so the ledger drops it; while a recording is open or a submission is pending
 * the allocator parks it, so the ledger keeps it -- and vb_pool_gc folds it only once the
 * fixture has cleared both. */
static void ledger_give(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nledger; i++) {
        if (g_ledger[i].mem != a->mem || g_ledger[i].off != a->off)
            continue;
        if (g_ledger[i].lost)
            return;                     /* never handed out again, never freed */
        if (g.slots[g.cur].recording || g.npending) {
            g_ledger[i].parked = 1;     /* still live: the allocator parked it */
            return;
        }
        g_ledger[i] = g_ledger[--g_nledger];
        return;
    }
}

/* The fixture's own gc: once nothing is recording and nothing is pending, what it parked is
 * folded back by vb_pool_gc, so the ledger drops those and keeps only what is really live. */
static void ledger_drain(void)
{
    unsigned i;

    if (g.slots[g.cur].recording || g.npending)
        return;
    for (i = 0; i < g_nledger; i++) {
        if (g_ledger[i].lost)
            continue;                   /* never reusable, never freed: it stays */

        if (g_ledger[i].parked) {
            g_ledger[i] = g_ledger[--g_nledger];
            i--;
        }
    }
}

/* The widest hole the fixture can see inside `b`, from the extents it took out of it: the gaps
 * between the live ranges it recorded, plus the untouched tail above the block's bump. */
static VkDeviceSize ledger_widest_hole(const VbBlock *b, VkDeviceSize bump)
{
    VkDeviceSize start = 0, best = 0;
    unsigned i;

    for (;;) {
        VkDeviceSize next = bump, j;
        int found = 0;

        for (i = 0; i < g_nledger; i++) {
            if (g_ledger[i].mem != b->mem || g_ledger[i].off < start)
                continue;
            if (!found || g_ledger[i].off < next) {
                next = g_ledger[i].off;
                found = 1;
            }
        }
        if (!found)
            break;
        if (next - start > best)
            best = next - start;
        /* the end of the range at `next` */
        j = next;
        for (i = 0; i < g_nledger; i++)
            if (g_ledger[i].mem == b->mem && g_ledger[i].off == next && g_ledger[i].off + g_ledger[i].size > j)
                j = g_ledger[i].off + g_ledger[i].size;
        start = j > next ? j : next;
        if (start >= bump)
            break;
    }
    if (b->size - bump > best)
        best = b->size - bump;
    return best;
}

static uintptr_t g_handle = 0x6000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

static void fill_bytes(uint8_t *p, size_t n, uint32_t seed)
{
    uint32_t w = 0x9E3779B9u ^ seed;
    size_t i;

    for (i = 0; i + 4u <= n; i += 4u) {
        memcpy(p + i, &w, 4);
        w = w * 1664525u + 1013904223u;
    }
    for (; i < n; i++)
        p[i] = (uint8_t)(w >> 24);
}

static uint64_t hash_bytes(const uint8_t *p, size_t n)
{
    uint64_t h = 0xCBF29CE484222325ull;
    size_t i;

    for (i = 0; i < n; i++)
        h = (h ^ p[i]) * 0x100000001B3ull;
    return h;
}

static size_t count_diff(const uint8_t *a, const uint8_t *b, size_t n)
{
    size_t i, bad = 0;

    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            bad++;
    return bad;
}

static GxObj *obj_by_image(VkImage img)
{
    unsigned i;

    for (i = 0; i < g_nobj; i++)
        if (g_obj[i].is_image && g_obj[i].img == img)
            return &g_obj[i];
    return NULL;
}

static GxObj *obj_by_buffer(VkBuffer buf)
{
    unsigned i;

    for (i = 0; i < g_nobj; i++)
        if (!g_obj[i].is_image && g_obj[i].buf == buf)
            return &g_obj[i];
    return NULL;
}

static GxMem *mem_by_handle(VkDeviceMemory mem)
{
    unsigned i;

    for (i = 0; i < GX_MAX_MEM; i++)
        if (g_mem[i].registered && g_mem[i].mem == mem)
            return &g_mem[i];
    return NULL;
}

static void spec(VkDeviceSize size, VkDeviceSize align, uint32_t bits, int req, int pref)
{
    g_spec.size = size;
    g_spec.align = align;
    g_spec.bits = bits;
    g_spec.requires_ded = req;
    g_spec.prefers_ded = pref;
}

/* ── The Vulkan entry points the allocator's bodies reach ────────────────────────────── */

static VkResult VKAPI_CALL gx_allocate(VkDevice device, const VkMemoryAllocateInfo *ai,
                                       const VkAllocationCallbacks *alloc, VkDeviceMemory *mem)
{
    unsigned i;

    (void)device; (void)alloc;
    sx.allocs++;
    if (sx.alloc_fail_at && sx.allocs >= sx.alloc_fail_at
        && sx.allocs < sx.alloc_fail_at + sx.alloc_fail_times) {
        *mem = VK_NULL_HANDLE;
        return sx.alloc_fail_result;
    }
    for (i = 0; i < GX_MAX_MEM; i++) {
        if (!g_mem[i].registered)
            break;
    }
    if (i == GX_MAX_MEM || !ai) {
        *mem = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    g_mem[i].registered = 1;
    g_mem[i].mem = (VkDeviceMemory)hnew();
    g_mem[i].size = ai->allocationSize;
    g_mem[i].arena = NULL;
    g_mem[i].frees = 0;
    *mem = g_mem[i].mem;

    sx.last_alloc_size = ai->allocationSize;
    sx.last_alloc_type = ai->memoryTypeIndex;
    if (ai->memoryTypeIndex < 8u)
        sx.type_calls[ai->memoryTypeIndex]++;
    sx.last_alloc_had_dedicated = 0;
    sx.last_alloc_dedicated_image = VK_NULL_HANDLE;
    sx.last_alloc_dedicated_buffer = VK_NULL_HANDLE;
    if (ai->pNext) {
        const VkBaseInStructure *b = (const VkBaseInStructure *)ai->pNext;

        if (b->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO) {
            const VkMemoryDedicatedAllocateInfo *d =
                (const VkMemoryDedicatedAllocateInfo *)ai->pNext;

            sx.last_alloc_had_dedicated = 1;
            sx.dedicated_pnext_seen++;
            sx.last_alloc_dedicated_image = d->image;
            sx.last_alloc_dedicated_buffer = d->buffer;
        }
    }
    return VK_SUCCESS;
}

static void VKAPI_CALL gx_free(VkDevice device, VkDeviceMemory mem,
                               const VkAllocationCallbacks *alloc)
{
    GxMem *m = mem_by_handle(mem);

    (void)device; (void)alloc;
    if (!m) {
        sx.double_frees++;
        return;
    }
    if (m->frees) {
        sx.double_frees++;
        return;
    }
    m->frees++;
    sx.frees++;
    free(m->arena);
    m->arena = NULL;
}

static VkResult VKAPI_CALL gx_map(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                  VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    GxMem *m = mem_by_handle(mem);

    (void)device; (void)size; (void)flags;
    if (!m) {
        sx.maps_after_free++;
        *ppData = NULL;
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    if (m->frees) {
        sx.maps_after_free++;
        *ppData = NULL;
        return VK_ERROR_MEMORY_MAP_FAILED;
    }
    if (sx.map_result != VK_SUCCESS) {
        *ppData = NULL;
        return sx.map_result;
    }
    if (!m->arena) {
        m->arena = (uint8_t *)calloc(1, (size_t)m->size);
        if (!m->arena) {
            *ppData = NULL;
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        }
    }
    /* The allocator maps the whole block at offset 0 */
    if (offset) {
        sx.bind_violations++;
        snprintf(sx.bind_violation, sizeof sx.bind_violation,
                 "vkMapMemory was called at offset %llu, not 0",
                 (unsigned long long)offset);
    }
    sx.maps++;
    *ppData = m->arena + offset;
    return VK_SUCCESS;
}

static void VKAPI_CALL gx_unmap(VkDevice device, VkDeviceMemory mem)
{
    (void)device; (void)mem;
    sx.unmaps++;
}

static VkResult VKAPI_CALL gx_flush(VkDevice device, uint32_t count,
                                    const VkMappedMemoryRange *ranges)
{
    uint32_t i;

    (void)device;
    sx.flush_calls++;
    if (sx.flush_result != VK_SUCCESS)
        return sx.flush_result;
    for (i = 0; i < count && sx.n_flush < 16u; i++)
        sx.flush[sx.n_flush++] = ranges[i];
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gx_invalidate(VkDevice device, uint32_t count,
                                         const VkMappedMemoryRange *ranges)
{
    uint32_t i;

    (void)device;
    sx.inval_calls++;
    if (sx.inval_result != VK_SUCCESS)
        return sx.inval_result;
    for (i = 0; i < count && sx.n_inval < 16u; i++)
        sx.inval[sx.n_inval++] = ranges[i];
    return VK_SUCCESS;
}

/* The two optional *MemoryRequirements2 lookups: the fixture reports the same object's
 * requirements, plus its VkMemoryDedicatedRequirements when the caller passes one in pNext. */
static void VKAPI_CALL gx_image_reqs2(VkDevice device, const VkImageMemoryRequirementsInfo2 *info,
                                      VkMemoryRequirements2 *out)
{
    GxObj *o;

    (void)device;
    sx.reqs2_calls++;
    o = info ? obj_by_image(info->image) : NULL;
    if (!o || !out)
        return;
    out->memoryRequirements.size = o->size;
    out->memoryRequirements.alignment = o->align;
    out->memoryRequirements.memoryTypeBits = o->bits;
    if (out->pNext) {
        VkMemoryDedicatedRequirements *d = (VkMemoryDedicatedRequirements *)out->pNext;

        d->requiresDedicatedAllocation = o->requires_ded ? VK_TRUE : VK_FALSE;
        d->prefersDedicatedAllocation = o->prefers_ded ? VK_TRUE : VK_FALSE;
    }
}

static void VKAPI_CALL gx_buffer_reqs2(VkDevice device,
                                       const VkBufferMemoryRequirementsInfo2 *info,
                                       VkMemoryRequirements2 *out)
{
    GxObj *o;

    (void)device;
    sx.reqs2_calls++;
    o = info ? obj_by_buffer(info->buffer) : NULL;
    if (!o || !out)
        return;
    out->memoryRequirements.size = o->size;
    out->memoryRequirements.alignment = o->align;
    out->memoryRequirements.memoryTypeBits = o->bits;
    if (out->pNext) {
        VkMemoryDedicatedRequirements *d = (VkMemoryDedicatedRequirements *)out->pNext;

        d->requiresDedicatedAllocation = o->requires_ded ? VK_TRUE : VK_FALSE;
        d->prefersDedicatedAllocation = o->prefers_ded ? VK_TRUE : VK_FALSE;
    }
}

static VkResult VKAPI_CALL gx_create_image(VkDevice device, const VkImageCreateInfo *info,
                                           const VkAllocationCallbacks *alloc, VkImage *image)
{
    (void)device; (void)info; (void)alloc;
    sx.creates_img++;
    if (sx.create_image_result != VK_SUCCESS) {
        *image = VK_NULL_HANDLE;
        return sx.create_image_result;
    }
    if (g_nobj < GX_MAX_OBJ) {
        GxObj *o = &g_obj[g_nobj++];

        memset(o, 0, sizeof *o);
        o->is_image = 1;
        o->img = (VkImage)hnew();
        o->size = g_spec.size;
        o->align = g_spec.align;
        o->bits = g_spec.bits;
        o->requires_ded = g_spec.requires_ded;
        o->prefers_ded = g_spec.prefers_ded;
        *image = o->img;
    } else {
        *image = (VkImage)hnew();
    }
    g_spec = g_spec_default;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gx_create_buffer(VkDevice device, const VkBufferCreateInfo *info,
                                            const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)alloc;
    sx.creates_buf++;
    if (sx.create_buffer_result != VK_SUCCESS) {
        *buf = VK_NULL_HANDLE;
        return sx.create_buffer_result;
    }
    if (g_nobj < GX_MAX_OBJ) {
        GxObj *o = &g_obj[g_nobj++];

        memset(o, 0, sizeof *o);
        o->is_image = 0;
        o->buf = (VkBuffer)hnew();
        o->size = g_spec.size ? g_spec.size : (info ? info->size : 64u);
        o->align = g_spec.align;
        o->bits = g_spec.bits;
        o->requires_ded = g_spec.requires_ded;
        o->prefers_ded = g_spec.prefers_ded;
        *buf = o->buf;
    } else {
        *buf = (VkBuffer)hnew();
    }
    g_spec = g_spec_default;
    return VK_SUCCESS;
}

static void VKAPI_CALL gx_get_image_reqs(VkDevice device, VkImage img, VkMemoryRequirements *mr)
{
    GxObj *o;

    (void)device;
    o = obj_by_image(img);
    mr->size = o ? o->size : 64u;
    mr->alignment = o ? o->align : 64u;
    mr->memoryTypeBits = o ? o->bits : 1u;
}

static void VKAPI_CALL gx_get_buffer_reqs(VkDevice device, VkBuffer buf, VkMemoryRequirements *mr)
{
    GxObj *o;

    (void)device;
    o = obj_by_buffer(buf);
    mr->size = o ? o->size : 64u;
    mr->alignment = o ? o->align : 64u;
    mr->memoryTypeBits = o ? o->bits : 1u;
}

/* Every bind this fixture sees is checked against the object's own requirement: the offset
 * must be a multiple of its alignment (and of the pooling floor), the range must sit inside a
 * block that really exists and was really allocated, and the block's memory type must be one
 * the requirement's memoryTypeBits allows. A violation is recorded, not raised: the property
 * that binds asserts the count is zero. */
static int gx_check_bind(int is_image, VkDeviceMemory mem, VkDeviceSize off, VkDeviceSize size,
                         VkDeviceSize align, uint32_t bits)
{
    unsigned p, b;
    int found = 0;
    int linear = !is_image;

    if (off % VB_POOL_ALIGN || (align && off % align)) {
        snprintf(sx.bind_violation, sizeof sx.bind_violation,
                 "%s bound at offset %llu: not a multiple of %llu (pool floor %u)",
                 is_image ? "an image" : "a buffer", (unsigned long long)off,
                 (unsigned long long)(align ? align : (VkDeviceSize)VB_POOL_ALIGN),
                 (unsigned)VB_POOL_ALIGN);
        sx.bind_violations++;
        return 0;
    }
    /* Every slot of every pool, not the first `nblocks`: a retirement leaves a hole at a low
     * index while a valid bind can still name a higher one. */
    for (p = 0; p < VB_POOLS && !found; p++) {
        for (b = 0; b < VB_POOL_BLOCKS && !found; b++) {
            if (g.pools[p].blocks[b].mem == VK_NULL_HANDLE || g.pools[p].blocks[b].mem != mem)
                continue;
            found = 1;
            if (!(bits & (1u << g.pools[p].type_index))) {
                snprintf(sx.bind_violation, sizeof sx.bind_violation,
                         "%s's requirement bits 0x%X do not admit the block's memory type %u",
                         is_image ? "an image" : "a buffer", bits, g.pools[p].type_index);
                sx.bind_violations++;
            }
            if (g.pools[p].nonlinear == linear) {
                snprintf(sx.bind_violation, sizeof sx.bind_violation,
                         "a%s resource is bound into a %s pool",
                         is_image ? "n image" : " buffer",
                         g.pools[p].nonlinear ? "non-linear (image)" : "linear (buffer)");
                sx.bind_violations++;
            }
            if (off + size > g.pools[p].blocks[b].size) {
                snprintf(sx.bind_violation, sizeof sx.bind_violation,
                         "the range [%llu, %llu) runs past the block's %llu bytes",
                         (unsigned long long)off, (unsigned long long)(off + size),
                         (unsigned long long)g.pools[p].blocks[b].size);
                sx.bind_violations++;
            }
        }
    }
    if (!found) {
        snprintf(sx.bind_violation, sizeof sx.bind_violation,
                 "%s bound to memory %p at %llu, which is no live block of this back end",
                 is_image ? "an image" : "a buffer", (void *)mem, (unsigned long long)off);
        sx.bind_violations++;
    }
    /* Cross-bind overlap: no two ranges of one block may overlap. The size recorded here is
     * the object's REQUIREMENT size, which is a lower bound on its range -- so this can only
     * miss an overlap of the alignment padding, never invent one; the take-level ledger below
     * carries the exact range sizes. */
    {
        unsigned i;

        for (i = 0; i < sx.bind_n && i < GX_MAX_BIND; i++) {
            if (sx.bind_mem[i] != mem || (sx.bind_off[i] == off && sx.bind_size[i] == size))
                continue;
            if (off < sx.bind_off[i] + sx.bind_size[i] && sx.bind_off[i] < off + size) {
                snprintf(sx.bind_violation, sizeof sx.bind_violation,
                         "two ranges of one block overlap: a %s at [%llu, %llu) and a %s at "
                         "[%llu, %llu)", sx.bind_is_image[i] ? "image" : "buffer",
                         (unsigned long long)sx.bind_off[i],
                         (unsigned long long)(sx.bind_off[i] + sx.bind_size[i]),
                         is_image ? "image" : "buffer", (unsigned long long)off,
                         (unsigned long long)(off + size));
                sx.bind_violations++;
            }
        }
        if (sx.bind_n < GX_MAX_BIND) {
            sx.bind_mem[sx.bind_n] = mem;
            sx.bind_off[sx.bind_n] = off;
            sx.bind_size[sx.bind_n] = size;
            sx.bind_is_image[sx.bind_n] = is_image;
            sx.bind_n++;
        }
    }
    return found;
}

static VkResult VKAPI_CALL gx_bind_image(VkDevice device, VkImage img, VkDeviceMemory mem,
                                         VkDeviceSize off)
{
    GxObj *o = obj_by_image(img);

    (void)device;
    sx.binds_img++;
    if (sx.binds_img <= 64u) {
        sx.bind_img_mem[sx.binds_img - 1u] = mem;
        sx.bind_img_off[sx.binds_img - 1u] = off;
        sx.bind_img_img[sx.binds_img - 1u] = img;
    }
    if (o)
        gx_check_bind(1, mem, off, o->size, o->align, o->bits);
    else
        gx_check_bind(1, mem, off, 0, 0, 0xFFu);
    return sx.bind_image_result;
}

static VkResult VKAPI_CALL gx_bind_buffer(VkDevice device, VkBuffer buf, VkDeviceMemory mem,
                                          VkDeviceSize off)
{
    GxObj *o = obj_by_buffer(buf);

    (void)device;
    sx.binds_buf++;
    if (sx.binds_buf <= 64u) {
        sx.bind_buf_mem[sx.binds_buf - 1u] = mem;
        sx.bind_buf_off[sx.binds_buf - 1u] = off;
        sx.bind_buf_buf[sx.binds_buf - 1u] = buf;
    }
    if (o)
        gx_check_bind(0, mem, off, o->size, o->align, o->bits);
    else
        gx_check_bind(0, mem, off, 0, 0, 0xFFu);
    return sx.bind_buffer_result;
}

static VkResult VKAPI_CALL gx_create_view(VkDevice device, const VkImageViewCreateInfo *info,
                                          const VkAllocationCallbacks *alloc, VkImageView *view)
{
    (void)device; (void)info; (void)alloc;
    sx.creates_view++;
    if (sx.create_view_result != VK_SUCCESS) {
        *view = VK_NULL_HANDLE;
        return sx.create_view_result;
    }
    *view = (VkImageView)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gx_destroy_image(VkDevice device, VkImage img,
                                        const VkAllocationCallbacks *alloc)
{
    (void)device; (void)img; (void)alloc;
    sx.destroys_img++;
}

static void VKAPI_CALL gx_destroy_buffer(VkDevice device, VkBuffer buf,
                                         const VkAllocationCallbacks *alloc)
{
    (void)device; (void)buf; (void)alloc;
    sx.destroys_buf++;
}

static void VKAPI_CALL gx_destroy_view(VkDevice device, VkImageView view,
                                       const VkAllocationCallbacks *alloc)
{
    (void)device; (void)view; (void)alloc;
    sx.destroys_view++;
}

/* The allocator resolves its optional entry points through vkGetDeviceProcAddr. The fixture
 * answers every name it can serve, and answers NULL for the four optional ones when the
 * scenario asks for an allocator that cannot see them (marker VB_GATE_HAS_REQS2). */
static PFN_vkVoidFunction VKAPI_CALL gx_device_proc(VkDevice device, const char *name)
{
    (void)device;
    sx.procs++;
    if (!name)
        return NULL;
    if (!strcmp(name, "vkGetImageMemoryRequirements2"))
        return sx.reqs2_available ? (PFN_vkVoidFunction)gx_image_reqs2 : NULL;
    if (!strcmp(name, "vkGetBufferMemoryRequirements2"))
        return sx.reqs2_available ? (PFN_vkVoidFunction)gx_buffer_reqs2 : NULL;
    if (!strcmp(name, "vkFlushMappedMemoryRanges"))
        return (PFN_vkVoidFunction)gx_flush;
    if (!strcmp(name, "vkInvalidateMappedMemoryRanges"))
        return (PFN_vkVoidFunction)gx_invalidate;
    if (!strcmp(name, "vkUnmapMemory"))
        return (PFN_vkVoidFunction)gx_unmap;
    if (!strcmp(name, "vkAllocateMemory"))
        return (PFN_vkVoidFunction)gx_allocate;
    if (!strcmp(name, "vkFreeMemory"))
        return (PFN_vkVoidFunction)gx_free;
    if (!strcmp(name, "vkMapMemory"))
        return (PFN_vkVoidFunction)gx_map;
    if (!strcmp(name, "vkCreateImage"))
        return (PFN_vkVoidFunction)gx_create_image;
    if (!strcmp(name, "vkCreateBuffer"))
        return (PFN_vkVoidFunction)gx_create_buffer;
    if (!strcmp(name, "vkBindImageMemory"))
        return (PFN_vkVoidFunction)gx_bind_image;
    if (!strcmp(name, "vkBindBufferMemory"))
        return (PFN_vkVoidFunction)gx_bind_buffer;
    if (!strcmp(name, "vkGetImageMemoryRequirements"))
        return (PFN_vkVoidFunction)gx_get_image_reqs;
    if (!strcmp(name, "vkGetBufferMemoryRequirements"))
        return (PFN_vkVoidFunction)gx_get_buffer_reqs;
    return NULL;
}

/* The allocator resolves its optional entry points through vkGetDeviceProcAddr (the
 * production loader does it once, at install). This fixture never calls install, so it wires
 * the pointers itself -- and can take them away again, which is how the "an absent lookup
 * means pooled" rule is decided. */
static void set_optional(int present)
{
    vkGetImageMemoryRequirements2 = present ? gx_image_reqs2 : NULL;
    vkGetBufferMemoryRequirements2 = present ? gx_buffer_reqs2 : NULL;
    vkFlushMappedMemoryRanges = present ? gx_flush : NULL;
    vkInvalidateMappedMemoryRanges = present ? gx_invalidate : NULL;
    vkUnmapMemory = present ? gx_unmap : NULL;
}

static void gate_wire(void)
{
    vkAllocateMemory = gx_allocate;
    vkFreeMemory = gx_free;
    vkMapMemory = gx_map;
    vkCreateImage = gx_create_image;
    vkDestroyImage = gx_destroy_image;
    vkGetImageMemoryRequirements = gx_get_image_reqs;
    vkBindImageMemory = gx_bind_image;
    vkCreateImageView = gx_create_view;
    vkDestroyImageView = gx_destroy_view;
    vkCreateBuffer = gx_create_buffer;
    vkDestroyBuffer = gx_destroy_buffer;
    vkGetBufferMemoryRequirements = gx_get_buffer_reqs;
    vkBindBufferMemory = gx_bind_buffer;
    vkGetDeviceProcAddr = gx_device_proc;
    /* The base names above are wired directly so a re-resolve cannot silently reach a NULL
     * pointer; the five optional ones are the ones the resolver exists for. */
    set_optional(1);
}

/* ── Reporting ───────────────────────────────────────────────────────────────────────── */

static int g_checks, g_check_failures, g_props, g_prop_failures;

static void section(const char *title)
{
    printf("\n== %s ==\n", title);
    fflush(stdout);
}

static void verdict(const char *prop, int checks_before, int failures_before)
{
    int bad = g_check_failures - failures_before;
    g_props++;
    if (bad)
        g_prop_failures++;
    printf("  --> %-58s %s (%d checks, %d failed)\n", prop, bad ? "FAIL" : "PASS",
           g_checks - checks_before, bad);
    fflush(stdout);
}

static void check(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    g_checks++;
    if (!cond)
        g_check_failures++;
    printf("      %-64s %s", what, cond ? "ok" : "FAIL");
    if (fmt && fmt[0]) {
        printf("  (");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        printf(")");
    }
    putchar('\n');
    fflush(stdout);
}

static void note(const char *fmt, ...)
{
    va_list ap;
    printf("      note: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
    fflush(stdout);
}

/* ── The world ───────────────────────────────────────────────────────────────────────── */

static void gate_world(void)
{
    unsigned i;

    free(g.scratch);                       /* what a previous property's decode allocated */
    memset(&g, 0, sizeof g);
    /* The allocator's counters are cumulative and it has no reset of its own; every world of
     * this fixture is a scenario of its own, so the fixture clears them with the state they
     * describe. (Nothing else is touched: s_pool_pending is emptied by the gc the worlds run,
     * and a world never starts with anything parked.) */
    memset(&s_mem, 0, sizeof s_mem);
    memset(&sx, 0, sizeof sx);
    memset(g_mem, 0, sizeof g_mem);
    memset(g_obj, 0, sizeof g_obj);
    g_nobj = 0;
    g_nlive = 0;
    g_nledger = 0;

    g.dev = (VkDevice)hnew();
    g.queue = (VkQueue)hnew();
    g.pd = (VkPhysicalDevice)hnew();
    g.atom = GX_ATOM;

    /* Four memory types, so that a request's memoryTypeBits can decide which one it gets:
     * 0 device-local, 1 host-visible coherent cached, 2 host-visible NOT coherent,
     * 3 device-local and host-visible coherent. */
    g.mp.memoryTypeCount = 4;
    g.mp.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    g.mp.memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    g.mp.memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    g.mp.memoryTypes[3].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    g.mp.memoryHeaps[0].size = (VkDeviceSize)1u << 34;
    g.mp.memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    for (i = 0; i < 4u; i++)
        g.mp.memoryTypes[i].heapIndex = 0;

    g.ok = 1;
    for (i = 0; i < VB_SLOTS; i++) {
        g.slots[i].cmd = (VkCommandBuffer)hnew();
        g.slots[i].fence = (VkFence)hnew();
        g.slots[i].stream = (VkBuffer)hnew();
        g.slots[i].dpool = (VkDescriptorPool)hnew();
    }
    g.cur = 0;

    g_spec_default.size = 0;
    g_spec_default.align = VB_POOL_ALIGN;
    g_spec_default.bits = GX_BIT_DEV;
    g_spec_default.requires_ded = 0;
    g_spec_default.prefers_ded = 0;
    g_spec = g_spec_default;

    sx.alloc_fail_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    sx.create_image_result = VK_SUCCESS;
    sx.create_buffer_result = VK_SUCCESS;
    sx.bind_image_result = VK_SUCCESS;
    sx.bind_buffer_result = VK_SUCCESS;
    sx.map_result = VK_SUCCESS;
    sx.create_view_result = VK_SUCCESS;
    sx.flush_result = VK_SUCCESS;
    sx.inval_result = VK_SUCCESS;
    sx.reqs2_available = 1;
    set_optional(1);
}

static VkMemoryRequirements mr_of(VkDeviceSize size, VkDeviceSize align, uint32_t bits)
{
    VkMemoryRequirements mr;

    mr.size = size;
    mr.alignment = align;
    mr.memoryTypeBits = bits;
    return mr;
}

static int take(const VkMemoryRequirements *mr, VkMemoryPropertyFlags want,
                VkMemoryPropertyFlags fallback, int nonlinear, VbAlloc *out)
{
    memset(out, 0, sizeof *out);
    return vb_mem_take(mr, want, fallback, nonlinear, 0, 0, NULL, NULL, out);
}

static int take_named(const VkMemoryRequirements *mr, VkMemoryPropertyFlags want,
                      VkMemoryPropertyFlags fallback, int nonlinear, int req, int pref,
                      const VkImage *img, const VkBuffer *buf, VbAlloc *out)
{
    memset(out, 0, sizeof *out);
    return vb_mem_take(mr, want, fallback, nonlinear, req, pref, img, buf, out);
}

/* The block an allocation lives in, or NULL for a dedicated allocation. The memory handle is
 * what identifies it: a pooled `VbAlloc` names its block, and this walks the pools to find it
 * so a check can compare the two against what the structures themselves say. */
static VbBlock *block_of(const VbAlloc *a, VbPool **pool_out)
{
    unsigned p, b;

    if (a->pool < 0 || a->pool >= VB_POOLS || !a->mem)
        return NULL;
    p = (unsigned)a->pool;
    for (b = 0; b < VB_POOL_BLOCKS; b++) {
        if (g.pools[p].blocks[b].mem == a->mem) {
            if (pool_out)
                *pool_out = &g.pools[p];
            return &g.pools[p].blocks[b];
        }
    }
    return NULL;
}

static void live_add(const VbAlloc *a)
{
    if (g_nlive < GX_MAX_LIVE) {
        g_live[g_nlive].mem = a->mem;
        g_live[g_nlive].off = a->off;
        g_live[g_nlive].size = a->size;
        g_nlive++;
    }
}

static int live_overlaps(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nlive; i++) {
        VkDeviceSize lo, hi, lo2, hi2;

        if (g_live[i].mem != a->mem)
            continue;
        lo = g_live[i].off;
        hi = g_live[i].off + g_live[i].size;
        lo2 = a->off;
        hi2 = a->off + a->size;
        if (lo2 < hi && lo < hi2)
            return 1;
    }
    return 0;
}

static int live_holds(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nlive; i++)
        if (g_live[i].mem == a->mem && g_live[i].off == a->off && g_live[i].size == a->size)
            return 1;
    return 0;
}

static void live_del(const VbAlloc *a)
{
    unsigned i;

    for (i = 0; i < g_nlive; i++) {
        if (g_live[i].mem == a->mem && g_live[i].off == a->off) {
            g_live[i] = g_live[--g_nlive];
            return;
        }
    }
}

/* The free lists of every live block, checked against the block's own numbers: the ranges
 * never overlap, never leave the block, and together with `used` and `padding` account for
 * exactly the bump. One check line per call, naming the block if it disagrees. */
static void check_free_lists(const char *what)
{
    unsigned p, b, i, j;
    int ok = 1;
    char why[192];

    why[0] = 0;
    for (p = 0; p < VB_POOLS && ok; p++) {
        VbPool *pl = &g.pools[p];

        for (b = 0; b < VB_POOL_BLOCKS && ok; b++) {
            VbBlock *bl = &pl->blocks[b];
            VkDeviceSize sum = 0;

            if (bl->mem == VK_NULL_HANDLE)
                continue;               /* a retired slot: not a block any more */
            if (bl->nfree > VB_POOL_FREE_MAX) {
                ok = 0;
                snprintf(why, sizeof why, "pool %u block %u tracks %u free ranges, over %u",
                         p, b, bl->nfree, (unsigned)VB_POOL_FREE_MAX);
                break;
            }
            for (i = 0; i < bl->nfree && ok; i++) {
                if (bl->free_list[i].size == 0
                    || bl->free_list[i].off + bl->free_list[i].size > bl->bump) {
                    ok = 0;
                    snprintf(why, sizeof why,
                             "pool %u block %u free range [%llu, %llu) is not inside [0, %llu)",
                             p, b, (unsigned long long)bl->free_list[i].off,
                             (unsigned long long)(bl->free_list[i].off + bl->free_list[i].size),
                             (unsigned long long)bl->bump);
                    break;
                }
                for (j = i + 1u; j < bl->nfree && ok; j++) {
                    if (bl->free_list[i].off < bl->free_list[j].off + bl->free_list[j].size
                        && bl->free_list[j].off < bl->free_list[i].off + bl->free_list[i].size) {
                        ok = 0;
                        snprintf(why, sizeof why,
                                 "pool %u block %u free ranges %u and %u overlap", p, b, i, j);
                    }
                }
                sum += bl->free_list[i].size;
            }
            if (ok && sum + bl->used + bl->padding != bl->bump) {
                ok = 0;
                snprintf(why, sizeof why,
                         "pool %u block %u: free %llu + used %llu + padding %llu != bump %llu",
                         p, b, (unsigned long long)sum, (unsigned long long)bl->used,
                         (unsigned long long)bl->padding, (unsigned long long)bl->bump);
            }
            if (ok && bl->bump > bl->size) {
                ok = 0;
                snprintf(why, sizeof why, "pool %u block %u: bump %llu past size %llu",
                         p, b, (unsigned long long)bl->bump, (unsigned long long)bl->size);
            }
        }
    }
    check(ok, what, "%s", ok ? "" : why);
}

static uint64_t pools_reserved(void)
{
    unsigned p, b;
    uint64_t sum = 0;

    for (p = 0; p < VB_POOLS; p++)
        for (b = 0; b < VB_POOL_BLOCKS; b++)
            if (g.pools[p].blocks[b].mem != VK_NULL_HANDLE)
                sum += g.pools[p].blocks[b].size;
    return sum;
}

static uint64_t pools_padding(void)
{
    unsigned p, b;
    uint64_t sum = 0;

    for (p = 0; p < VB_POOLS; p++)
        for (b = 0; b < VB_POOL_BLOCKS; b++)
            if (g.pools[p].blocks[b].mem != VK_NULL_HANDLE)
                sum += g.pools[p].blocks[b].padding;
    return sum;
}

/* ── 0. the source under test ────────────────────────────────────────────────────────── */

static void marker(const char *what, int present)
{
    printf("      %-64s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

static int seam_ready;

static void seam_section(void)
{
    int all = 1;

    section("0. the source under test, probed from the same bytes at build time");
    printf("      production bodies compiled from: %s\n", VB_GATE_SOURCE);
    printf("      sha256 %s\n", VB_GATE_SOURCE_SHA256);
    printf("      %u bytes; counters declared in %s\n", (unsigned)VB_GATE_SOURCE_BYTES,
           VB_GATE_DECLS);
    note("every function driven below is that file's own, compiled into this translation unit "
         "by #include; scripts/vk-suballoc-gate.py rejects the BUILD when a definition, a "
         "struct, a field, a constant or a counter this fixture binds to is gone");
    printf("      %-64s %d\n", "required anchors the probe found in the source",
           (int)VB_GATE_REQUIRED_ANCHORS);
    marker("a block tracks its free ranges (VB_GATE_HAS_FREE_LIST)", VB_GATE_HAS_FREE_LIST);
    marker("releases are parked while the ring is busy (VB_GATE_HAS_PARK)", VB_GATE_HAS_PARK);
    marker("a dedicated allocation carries its pNext (VB_GATE_HAS_DEDICATED_PNEXT)",
           VB_GATE_HAS_DEDICATED_PNEXT);
    marker("the optional *MemoryRequirements2 lookups (VB_GATE_HAS_REQS2)", VB_GATE_HAS_REQS2);
    marker("the non-coherent atom widening (VB_GATE_HAS_ATOM_MATH)", VB_GATE_HAS_ATOM_MATH);
    marker("the write-once sharing latch (VB_GATE_HAS_SHARED_LATCH)", VB_GATE_HAS_SHARED_LATCH);
    marker("the write-once reuse latch (VB_GATE_HAS_REUSE_LATCH)", VB_GATE_HAS_REUSE_LATCH);
    marker("block retirement under pressure (VB_GATE_HAS_RETIRE)", VB_GATE_HAS_RETIRE);
    marker("one mapping per block, handed out as map + off (VB_GATE_HAS_BLOCK_MAP)",
           VB_GATE_HAS_BLOCK_MAP);
    marker("the no-coherent-host-type reason (VB_GATE_HAS_NOHOST_REASON)",
           VB_GATE_HAS_NOHOST_REASON);
    all = VB_GATE_HAS_FREE_LIST && VB_GATE_HAS_PARK && VB_GATE_HAS_DEDICATED_PNEXT
       && VB_GATE_HAS_REQS2 && VB_GATE_HAS_ATOM_MATH && VB_GATE_HAS_SHARED_LATCH
       && VB_GATE_HAS_REUSE_LATCH && VB_GATE_HAS_BLOCK_MAP && VB_GATE_HAS_NOHOST_REASON
       && VB_GATE_HAS_RETIRE;
    note("an ABSENT marker is reported by the property that needs it as a failed check; the "
         "VkResults injected below are stubs' return values, not a real device's");
    seam_ready = all;
}

/* ── 1. sharing, and the allocation reduction ────────────────────────────────────────── */

static void prop1(void)
{
    Nv2aVkMemoryStats m;
    VbImage im[GX_IMGS];
    unsigned i, j, shared = 0;
    int chk = g_checks, bad = g_check_failures;

    section("1. several resources in one block, and the allocation-call reduction");

    if (!seam_ready)
        check(0, "the source under test carries the allocator's whole seam", "");
    if (!VB_GATE_HAS_SHARED_LATCH)
        check(0, "the write-once sharing latch exists (VB_GATE_HAS_SHARED_LATCH)", "");

    /* 1a: eleven images, all with the same requirement, through the resource path. */
    gate_world();
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    memset(im, 0, sizeof im);
    for (i = 0; i < GX_IMGS; i++) {
        spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
        if (!vb_image_create(&im[i], 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                             | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT))
            break;
    }
    check(i == GX_IMGS, "1: every image was created and bound", "%u of %u", i, GX_IMGS);
    check(sx.creates_img == GX_IMGS && sx.binds_img == GX_IMGS,
          "1: the fixture's stubs saw the creations and the binds", "%u image(s), %u bind(s)",
          sx.creates_img, sx.binds_img);
    check(sx.bind_violations == 0,
          "1: every bind is inside its block, aligned and of a memory type its bits allow",
          "%u violation(s): %s", sx.bind_violations, sx.bind_violation);
    nv2a_vk_backend_memory_stats(&m);
    check(sx.allocs == 1u && m.alloc_calls == sx.allocs,
          "alloc: 11 images cost ONE vkAllocateMemory, and the allocator's own counter agrees",
          "fixture saw %u call(s), alloc_calls %llu, blocks %llu",
          sx.allocs, (unsigned long long)m.alloc_calls, (unsigned long long)m.blocks);
    check(sx.allocs * 2u <= GX_IMGS,
          "alloc: the reduction is meaningful (calls at most half the resource count)",
          "%u call(s) for %u resource(s) -- one per resource would be %u",
          sx.allocs, (unsigned)GX_IMGS, (unsigned)GX_IMGS);
    check(m.blocks == 1 && m.ranges >= GX_IMGS && m.pools >= 1,
          "1: one block holds every range", "blocks %llu, ranges %llu, pools %llu",
          (unsigned long long)m.blocks, (unsigned long long)m.ranges,
          (unsigned long long)m.pools);
    check(m.first_shared_block != (uint32_t)-1 && m.first_shared_block_kind == 1u,
          "1: the write-once sharing latch names the pool whose block holds two or more live "
          "ranges, with the images' own kind",
          "first_shared_block %u, kind %u", m.first_shared_block,
          m.first_shared_block_kind);

    /* The sharing itself, from the binds the fixture saw: the same memory, distinct aligned
     * non-overlapping offsets. */
    for (i = 0; i < GX_IMGS && i < sx.binds_img; i++) {
        if (sx.bind_img_mem[i] != sx.bind_img_mem[0]) {
            shared = 1;
            break;
        }
    }
    check(shared == 0, "1: every image's bind names the same VkDeviceMemory", "");
    if (sx.binds_img >= 2u) {
        int distinct = 1, aligned = 1, disjoint = 1;

        for (i = 0; i < GX_IMGS && i < sx.binds_img; i++) {
            if (sx.bind_img_off[i] % VB_POOL_ALIGN)
                aligned = 0;
            for (j = i + 1u; j < GX_IMGS && j < sx.binds_img; j++) {
                if (sx.bind_img_off[i] == sx.bind_img_off[j])
                    distinct = 0;
                if (sx.bind_img_off[i] < sx.bind_img_off[j] + 64u * 1024u
                    && sx.bind_img_off[j] < sx.bind_img_off[i] + 64u * 1024u)
                    disjoint = 0;
            }
        }
        check(distinct, "1: the offsets are distinct", "off[0] %llu, off[1] %llu, off[2] %llu",
              (unsigned long long)sx.bind_img_off[0], (unsigned long long)sx.bind_img_off[1],
              (unsigned long long)sx.bind_img_off[2]);
        check(aligned, "1: every offset is a multiple of the pooling alignment",
              "VB_POOL_ALIGN %u", (unsigned)VB_POOL_ALIGN);
        check(disjoint, "1: the 64 KiB ranges do not overlap", "");
    } else {
        check(0, "1: at least two binds to compare", "%u bind(s)", sx.binds_img);
    }
    check_free_lists("1: the block's free list, used and padding account for its bump exactly");

    /* 1b: the same claim on a host-visible byte workload, where the bytes themselves are
     * written through the pooled ranges and read back. These are the gate's byte checks: they
     * must stay green under a source that pools nothing (the optimization-positive control). */
    {
        VbAlloc mem[GX_BUFS];
        VkBuffer buf[GX_BUFS];
        uint8_t *ptr[GX_BUFS];
        uint64_t h_before, h_after;
        unsigned n = 0, good = 1, mapped = 1, identity = 1, allocs_before = sx.allocs;
        uint64_t used_before;

        memset(mem, 0, sizeof mem);
        memset(buf, 0, sizeof buf);
        memset(ptr, 0, sizeof ptr);
        nv2a_vk_backend_memory_stats(&m);
        used_before = m.used_bytes;         /* the eleven images are live in their own pool */
        for (i = 0; i < GX_BUFS; i++) {
            spec(4096u, VB_POOL_ALIGN, GX_BIT_HOST, 0, 0);
            if (!vb_host_buffer(4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, &buf[i], &mem[i],
                                &ptr[i]))
                break;
            n++;
        }
        check(n == GX_BUFS, "bytes: every host-visible buffer was created, bound and mapped",
              "%u of %u", n, GX_BUFS);
        for (i = 0; i < n; i++) {
            VbBlock *bl;
            VbPool *pl = NULL;

            if (!ptr[i]) {
                mapped = 0;
                continue;
            }
            fill_bytes(ptr[i], 4096u, (uint32_t)(0x100u + i));
            bl = block_of(&mem[i], &pl);
            if (bl && bl->map && ptr[i] != bl->map + mem[i].off)
                identity = 0;
        }
        check(mapped, "bytes: every buffer has a mapping", "");
        check(identity, "bytes: each pointer is its block's single mapping plus the range's "
              "offset (or the allocation is dedicated)", "");
        /* Read every buffer's bytes back and compare with the pattern its own seed produced. */
        for (i = 0; i < n; i++) {
            uint8_t *want = (uint8_t *)malloc(4096u);

            if (!want) {
                good = 0;
                break;
            }
            fill_bytes(want, 4096u, (uint32_t)(0x100u + i));
            if (memcmp(want, ptr[i], 4096u) != 0)
                good = 0;
            /* And the neighbours are untouched: writing one range must not have moved
             * another's bytes. */
            for (j = 0; j < n; j++) {
                if (j == i)
                    continue;
                if (mem[i].mem == mem[j].mem
                    && mem[i].off < mem[j].off + mem[j].size
                    && mem[j].off < mem[i].off + mem[i].size)
                    good = 0;
            }
            free(want);
        }
        check(good, "bytes: every pooled range holds exactly the bytes written through its own "
              "pointer, and no two ranges overlap", "%u range(s) of 4096 bytes", n);
        h_before = hash_bytes(ptr[0], 4096u);
        fill_bytes(ptr[0], 4096u, 0xABCu);
        h_after = hash_bytes(ptr[0], 4096u);
        check(h_before != h_after, "bytes: the mapping really is writable", "0x%016llX -> "
              "0x%016llX", (unsigned long long)h_before, (unsigned long long)h_after);

        nv2a_vk_backend_memory_stats(&m);
        check(sx.allocs - allocs_before <= GX_BUFS / 2u,
              "alloc: 8 host-visible buffers cost at most half as many calls as resources",
              "%u call(s) for %u buffer(s) -- one per resource would be %u",
              sx.allocs - allocs_before, (unsigned)GX_BUFS, (unsigned)GX_BUFS);
        check(m.alloc_calls == sx.allocs,
              "alloc: the allocator's alloc_calls equals the calls the fixture's stub counted",
              "alloc_calls %llu, stub %u", (unsigned long long)m.alloc_calls, sx.allocs);
        check(m.used_bytes >= (uint64_t)n * 4096u,
              "alloc: used_bytes accounts for the live ranges", "used_bytes %llu for %u ranges",
              (unsigned long long)m.used_bytes, n);

        /* Release them through the production release path and require the accounting to come
         * back to where it started. */
        for (i = 0; i < n; i++) {
            VbGarbage e;

            memset(&e, 0, sizeof e);
            e.mem = mem[i];
            e.buf = buf[i];
            vb_discard(e);
        }
        nv2a_vk_backend_memory_stats(&m);
        check(sx.destroys_buf == n && sx.double_frees == 0,
              "bytes: every buffer was destroyed exactly once and nothing was freed twice",
              "%u destroyed of %u, %u double free(s)", sx.destroys_buf, n, sx.double_frees);
        check(m.used_bytes == used_before && m.parked == 0u,
              "1: releasing every buffer returns the block's bytes to the free accounting "
              "(and nothing was parked: nothing was recording)",
              "used_bytes %llu -> %llu (the images' own %llu), parked %llu",
              (unsigned long long)used_before, (unsigned long long)m.used_bytes,
              (unsigned long long)used_before, (unsigned long long)m.parked);
        check_free_lists("1: after every release the block's numbers still add up");
    }
    verdict("1 sharing and the allocation reduction", chk, bad);
}

/* ── 2. memory types, alignment and honest accounting ────────────────────────────────── */

static void prop2(void)
{
    Nv2aVkMemoryStats m;
    VkMemoryRequirements mr;
    VbAlloc a, b, c, d, e;
    VbBlock *b1 = NULL, *b2 = NULL;
    VbPool *p1 = NULL, *p2 = NULL;
    VkDeviceSize sa = 100000u, sb = 120000u;
    int chk = g_checks, bad = g_check_failures;

    section("2. memoryTypeBits decides the pool; alignment and the byte counters are exact");

    /* 2a: two requests whose bits both admit type 0 share one block (the positive control),
     * and a third whose bits cannot admit it does not. */
    gate_world();
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "2: the first device-local range was taken", "");
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV | GX_BIT_HOST);
    check(take(&mr, GX_WANT_DEV, 0, 1, &b) == 1, "2: the second range was taken", "");
    check(a.mem == b.mem && a.off != b.off,
          "2: bits {0} and {0,1} both select type 0, so they share a block",
          "mem %p/%p, off %llu/%llu", (void *)a.mem, (void *)b.mem,
          (unsigned long long)a.off, (unsigned long long)b.off);
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEVHOST);
    check(take(&mr, GX_WANT_DEV, 0, 1, &c) == 1, "2: the third range was taken", "");
    check(c.mem != a.mem,
          "2: bits {3} cannot select type 0, so the range is never put in that block",
          "third mem %p vs first %p (its bits 0x%X do not include type 0)",
          (void *)c.mem, (void *)a.mem, GX_BIT_DEVHOST);
    b1 = block_of(&a, &p1);
    b2 = block_of(&c, &p2);
    check(b1 && p1 && (GX_BIT_DEV & (1u << p1->type_index)) != 0,
          "2: the shared block's memory type is one the first requirement's bits allow",
          "type_index %u, bits 0x%X", p1 ? p1->type_index : 99u, GX_BIT_DEV);
    check(b2 && p2 && (GX_BIT_DEVHOST & (1u << p2->type_index)) != 0,
          "2: the third block's memory type is one ITS requirement's bits allow",
          "type_index %u, bits 0x%X", p2 ? p2->type_index : 99u, GX_BIT_DEVHOST);
    check(p1 && p2 && p1->type_index != p2->type_index,
          "2: the two blocks really are different memory types",
          "type_index %u vs %u", p1 ? p1->type_index : 99u, p2 ? p2->type_index : 99u);

    /* 2b: alignment above VB_POOL_ALIGN, and the virgin block's accounting. */
    gate_world();
    mr = mr_of(sa, 65536u, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &d) == 1, "2: a 100000-byte range at alignment 65536",
          "");
    mr = mr_of(sb, 65536u, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1, "2: a second one, same requirement", "");
    check(d.off % 65536u == 0 && e.off % 65536u == 0,
          "2: both offsets are multiples of the requirement's own 65536-byte alignment",
          "off %llu and %llu", (unsigned long long)d.off, (unsigned long long)e.off);
    check(e.off >= d.off + sa,
          "2: the second range starts after the first one ends", "off %llu + %llu <= %llu",
          (unsigned long long)d.off, (unsigned long long)sa, (unsigned long long)e.off);
    check(d.size >= sa && e.size >= sb,
          "2: every range is at least as large as its requirement",
          "sizes %llu/%llu for %llu/%llu", (unsigned long long)d.size,
          (unsigned long long)e.size, (unsigned long long)sa, (unsigned long long)sb);
    check(!live_overlaps(&d) && !live_overlaps(&e),
          "2: the ranges never overlap", "");
    live_add(&d);
    live_add(&e);
    ledger_add(&d);
    ledger_add(&e);
    b1 = block_of(&d, &p1);
    check(b1 != NULL && b1->size >= d.off + d.size && b1->size >= e.off + e.size,
          "2: both ranges fit inside the block they name",
          "block %llu bytes for ranges ending at %llu and %llu",
          b1 ? (unsigned long long)b1->size : 0ull,
          (unsigned long long)(d.off + d.size), (unsigned long long)(e.off + e.size));
    nv2a_vk_backend_memory_stats(&m);
    if (b1) {
        check(b1->used == d.size + e.size,
              "2: the block's used count is exactly the two live ranges",
              "used %llu for %llu + %llu", (unsigned long long)b1->used,
              (unsigned long long)d.size, (unsigned long long)e.size);
        check(b1->padding == d.off + (e.off - (d.off + d.size)),
              "2: the block's padding count is exactly the bytes alignment cost it (the gap "
              "before the first range plus the gap between the two)",
              "padding %llu, expected %llu (first at %llu, second at %llu after %llu bytes)",
              (unsigned long long)b1->padding,
              (unsigned long long)(d.off + (e.off - (d.off + d.size))),
              (unsigned long long)d.off, (unsigned long long)e.off,
              (unsigned long long)d.size);
        check(b1->bump == e.off + e.size && b1->nfree == 0,
              "2: the bump is the high-water mark and nothing is free yet",
              "bump %llu, nfree %u", (unsigned long long)b1->bump, b1->nfree);
        check(m.reserved_bytes == pools_reserved() && m.padding_bytes == pools_padding(),
              "2: the published reserved and padding bytes are the pools' own sums",
              "reserved %llu vs %llu, padding %llu vs %llu",
              (unsigned long long)m.reserved_bytes,
              (unsigned long long)pools_reserved(), (unsigned long long)m.padding_bytes,
              (unsigned long long)pools_padding());
        check(m.reserved_bytes == b1->size && m.ranges == 2,
              "2: one block, two ranges", "reserved %llu, ranges %llu",
              (unsigned long long)m.reserved_bytes, (unsigned long long)m.ranges);
        check(m.used_bytes == d.size + e.size && m.largest_free == b1->size - b1->bump
              && m.free_bytes == b1->size - b1->bump,
              "2: free, used and largest_free are the untouched tail of the block",
              "free %llu, largest %llu, tail %llu", (unsigned long long)m.free_bytes,
              (unsigned long long)m.largest_free, (unsigned long long)(b1->size - b1->bump));
        check(m.reserved_bytes == m.used_bytes + m.padding_bytes + m.free_bytes,
              "2: reserved = used + padding + free", "%llu = %llu + %llu + %llu",
              (unsigned long long)m.reserved_bytes, (unsigned long long)m.used_bytes,
              (unsigned long long)m.padding_bytes, (unsigned long long)m.free_bytes);
        /* The same identity from the fixture's OWN numbers -- the block sizes it can read out
         * of the allocator's structures and the extents it took itself -- so the published
         * used/free/padding are compared with something they were not computed from. */
        check(m.free_bytes + m.padding_bytes == pools_reserved() - m.used_bytes,
              "2: free + padding is exactly the observed blocks minus used_bytes",
              "%llu + %llu vs %llu - %llu", (unsigned long long)m.free_bytes,
              (unsigned long long)m.padding_bytes, (unsigned long long)pools_reserved(),
              (unsigned long long)m.used_bytes);
        check(m.used_bytes == ledger_sum(),
              "2: used_bytes is exactly the sum of the extents the fixture took",
              "used_bytes %llu, the fixture's own ledger %llu",
              (unsigned long long)m.used_bytes, (unsigned long long)ledger_sum());
        check(m.largest_free >= ledger_widest_hole(b1, b1->bump),
              "2: largest_free covers the widest hole the fixture can see from the offsets it "
              "took",
              "largest_free %llu, the fixture's widest hole %llu",
              (unsigned long long)m.largest_free,
              (unsigned long long)ledger_widest_hole(b1, b1->bump));
    }
    check_free_lists("2: the free lists, used and padding add up");

    /* 2e: the pooling alignment floor. A request whose own alignment is 4 (and one at 16)
     * still lands on a VB_POOL_ALIGN multiple: the floor is not the requirement's alignment.
     * Removing the floor packs the second range at 100, which is not a multiple of 256. */
    gate_world();
    mr = mr_of(100u, 4u, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &d) == 1, "2e: a 100-byte range at alignment 4", "");
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1, "2e: a second one", "");
    check(d.off % VB_POOL_ALIGN == 0 && e.off % VB_POOL_ALIGN == 0 && e.off % 4u == 0,
          "2e: both offsets are multiples of VB_POOL_ALIGN, not merely of the requirement's 4",
          "offsets %llu and %llu, VB_POOL_ALIGN %u", (unsigned long long)d.off,
          (unsigned long long)e.off, (unsigned)VB_POOL_ALIGN);
    check(d.size >= 100u && e.size >= 100u && e.off >= d.off + d.size,
          "2e: the ranges satisfy their requirement and do not overlap",
          "sizes %llu/%llu, offsets %llu/%llu", (unsigned long long)d.size,
          (unsigned long long)e.size, (unsigned long long)d.off, (unsigned long long)e.off);

    /* 2c: with the *Requirements2 lookups unavailable (the resolver answers NULL), the request
     * is pooled and no dedicated reason is counted -- the interface's "NULL means not
     * available" rule. This runs in a world of its own, and before any world that offers the
     * lookups, so a memoised pointer cannot colour it. */
    gate_world();
    sx.reqs2_available = 0;
    set_optional(0);                       /* an absent lookup means "not available" */
    if (!VB_GATE_HAS_REQS2) {
        check(0, "2: the source looks the *Requirements2 entry points up "
                 "(VB_GATE_HAS_REQS2)", "");
    } else {
        VbImage im;

        memset(&im, 0, sizeof im);
        /* requires AND prefers: if the allocator could see them, this would be dedicated. */
        spec(8192u, VB_POOL_ALIGN, GX_BIT_DEV, 1, 1);
        check(vb_image_create(&im, 16u, 16u, VK_FORMAT_B8G8R8A8_UNORM,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              VK_IMAGE_ASPECT_COLOR_BIT) == 1,
              "2: the image was created with the lookups unavailable", "");
        nv2a_vk_backend_memory_stats(&m);
        check(sx.dedicated_pnext_seen == 0 && m.dedicated == 0,
              "2: with no *Requirements2 lookup the request is pooled, and no dedicated "
              "allocation or reason is counted",
              "pNext seen %u, dedicated %llu", sx.dedicated_pnext_seen,
              (unsigned long long)m.dedicated);
        check(im.mem.pool >= 0 && im.mem.mem != VK_NULL_HANDLE,
              "2: the allocation is a pooled one", "pool %d, block %d", im.mem.pool,
              im.mem.block);
    }
    /* 2d: padding is reported honestly. Two 4 KiB ranges fill the block's first 8 KiB; the
     * second is released; a request whose own alignment is 65536 cannot use that 4 KiB hole,
     * so the bump cursor jumps from 8192 to 65536 -- 57344 bytes the block spent aligning.
     * The block's own counter and the published one must both say so. */
    gate_world();
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &d) == 1, "2d: the first 4 KiB range", "");
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1, "2d: the second 4 KiB range", "");
    ledger_add(&d);
    ledger_add(&e);
    b1 = block_of(&d, &p1);
    ledger_give(&e);
    vb_mem_give(&e);                          /* the hole [4096, 8192) */
    mr = mr_of(4096u, 65536u, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1,
          "2d: a range whose alignment the hole cannot satisfy", "");
    ledger_add(&e);
    nv2a_vk_backend_memory_stats(&m);
    check(e.off == 65536u,
          "2d: it was taken from the bump cursor, past the hole", "off %llu",
          (unsigned long long)e.off);
    check(b1 && b1->padding == 65536u - 8192u,
          "2d: the block counts exactly the bytes alignment cost it",
          "padding %llu, expected %llu", b1 ? (unsigned long long)b1->padding : 0ull,
          (unsigned long long)(65536u - 8192u));
    check(m.padding_bytes == (uint64_t)(65536u - 8192u),
          "2d: and the published padding_bytes is that same number -- the counter is honest, "
          "not always zero",
          "padding_bytes %llu, the block's own padding %llu",
          (unsigned long long)m.padding_bytes, b1 ? (unsigned long long)b1->padding : 0ull);
    check(m.reserved_bytes == m.used_bytes + m.padding_bytes + m.free_bytes,
          "2d: reserved = used + padding + free still holds with padding in it",
          "%llu = %llu + %llu + %llu", (unsigned long long)m.reserved_bytes,
          (unsigned long long)m.used_bytes, (unsigned long long)m.padding_bytes,
          (unsigned long long)m.free_bytes);
    check(m.free_bytes + m.padding_bytes == pools_reserved() - m.used_bytes
          && m.used_bytes == ledger_sum(),
          "2d: and from the fixture's own numbers (observed blocks and the extents it took)",
          "free+padding %llu+%llu vs %llu-%llu; used %llu vs ledger %llu",
          (unsigned long long)m.free_bytes, (unsigned long long)m.padding_bytes,
          (unsigned long long)pools_reserved(), (unsigned long long)m.used_bytes,
          (unsigned long long)m.used_bytes, (unsigned long long)ledger_sum());
    check_free_lists("2d: the free list, used and padding still account for the bump");
    verdict("2 memory types, alignment and the byte counters", chk, bad);
}

/* ── 3. linear and non-linear pools are separate ─────────────────────────────────────── */

static void prop3(void)
{
    Nv2aVkMemoryStats m;
    VkMemoryRequirements mr;
    VbAlloc img, buf;
    VbPool *pi = NULL, *pb = NULL;
    int chk = g_checks, bad = g_check_failures;

    section("3. an image and a buffer with identical requirements never share a block");

    gate_world();
    mr = mr_of(16384u, VB_POOL_ALIGN, GX_BIT_DEV | GX_BIT_DEVHOST);
    check(take(&mr, GX_WANT_DEV, 0, 1, &img) == 1, "3: the image's range was taken", "");
    check(take(&mr, GX_WANT_DEV, 0, 0, &buf) == 1, "3: the buffer's range was taken", "");
    block_of(&img, &pi);
    block_of(&buf, &pb);
    check(img.mem != buf.mem,
          "3: identical requirements, one image and one buffer: different blocks",
          "image mem %p, buffer mem %p", (void *)img.mem, (void *)buf.mem);
    check(pi && pb && pi->nonlinear == 1 && pb->nonlinear == 0,
          "3: the image's pool is non-linear and the buffer's is linear",
          "image pool nonlinear %d, buffer pool nonlinear %d", pi ? pi->nonlinear : -1,
          pb ? pb->nonlinear : -1);
    check(pi && pb && pi->nblocks && pb->nblocks && pi->type_index == pb->type_index,
          "3: the separation is the KIND, not a different memory type",
          "type_index %u vs %u", pi ? pi->type_index : 99u, pb ? pb->type_index : 99u);
    check(sx.allocs == 2,
          "alloc: two kinds cost two allocations (one block each, not one per resource)",
          "%u call(s)", sx.allocs);
    /* The control: two buffers (or two images) with those requirements DO share. */
    {
        VbAlloc buf2;

        check(take(&mr, GX_WANT_DEV, 0, 0, &buf2) == 1, "3: a second buffer's range was taken",
              "");
        check(buf2.mem == buf.mem && buf2.off != buf.off,
              "3: two buffers with the same requirements share the linear block", "");
        check(sx.allocs == 2, "alloc: and the second buffer cost no further allocation",
              "%u call(s)", sx.allocs);
    }
    nv2a_vk_backend_memory_stats(&m);
    check(m.pools == 2, "3: two pools are in use (image and buffer)",
          "pools %llu", (unsigned long long)m.pools);
    check(m.first_shared_block_kind <= 1u,
          "3: the sharing latch's kind is one of the two kinds",
          "kind %u", m.first_shared_block_kind);
    verdict("3 linear and non-linear pools", chk, bad);
}

/* ── 4. reuse, coalescing, and the free lists ────────────────────────────────────────── */

static void prop4(void)
{
    Nv2aVkMemoryStats m, m0;
    VkMemoryRequirements mr;
    VbAlloc a, b, c, d, e;
    VbBlock *b1 = NULL;
    VbPool *p1 = NULL;
    VkDeviceSize b_off = 0;
    VkDeviceMemory b_mem = VK_NULL_HANDLE;
    const VkDeviceSize S = 4096u;
    int chk = g_checks, bad = g_check_failures;

    section("4. a released range is handed out again, and released neighbours coalesce");

    if (!VB_GATE_HAS_REUSE_LATCH)
        check(0, "the write-once reuse latch exists (VB_GATE_HAS_REUSE_LATCH)", "");

    gate_world();
    mr = mr_of(S, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "4: range A", "");
    check(take(&mr, GX_WANT_DEV, 0, 1, &b) == 1, "4: range B", "");
    check(take(&mr, GX_WANT_DEV, 0, 1, &c) == 1, "4: range C", "");
    live_add(&a);
    live_add(&b);
    live_add(&c);
    check(b.off == a.off + S && c.off == b.off + S,
          "4: three equal ranges are packed adjacently, with no padding",
          "offsets %llu, %llu, %llu (each %llu bytes)", (unsigned long long)a.off,
          (unsigned long long)b.off, (unsigned long long)c.off, (unsigned long long)S);
    {
        VbBlock *bl = block_of(&a, NULL);

        check(bl && bl->padding == 0, "4: nothing was padded", "padding %llu",
              bl ? (unsigned long long)bl->padding : 0ull);
    }
    nv2a_vk_backend_memory_stats(&m0);
    check(sx.allocs == 1, "alloc: all three ranges came from one block", "%u call(s)",
          sx.allocs);

    /* Release the middle one: it must be the range handed out next. */
    b_off = b.off;
    b_mem = b.mem;
    vb_mem_give(&b);
    live_del(&b);
    nv2a_vk_backend_memory_stats(&m);
    check(m.released >= m0.released + 1u, "4: the release was counted",
          "released %llu -> %llu", (unsigned long long)m0.released,
          (unsigned long long)m.released);
    check(m.first_reuse_block == ~(uint64_t)0, "4: nothing has been reused yet",
          "first_reuse_block 0x%llX", (unsigned long long)m.first_reuse_block);
    check_free_lists("4: the released range is tracked as free");

    check(take(&mr, GX_WANT_DEV, 0, 1, &d) == 1, "4: range D", "");
    live_add(&d);
    check(d.off == b_off && d.mem == b_mem,
          "4: D is the range B released -- the free range, not the bump",
          "D.off %llu, B.off %llu (the bump was at %llu)",
          (unsigned long long)d.off, (unsigned long long)b_off,
          (unsigned long long)(c.off + c.size));
    nv2a_vk_backend_memory_stats(&m);
    if (VB_GATE_HAS_REUSE_LATCH) {
        check(m.reuse_hits >= 1u && m.first_reuse_off == b_off,
              "4: reuse_hits counts it and the write-once latch names the range",
              "reuse_hits %llu, first_reuse_off %llu", (unsigned long long)m.reuse_hits,
              (unsigned long long)m.first_reuse_off);
    }
    check(sx.allocs == 1 && m.blocks == 1,
          "alloc: reuse cost no allocation at all", "%u call(s), blocks %llu", sx.allocs,
          (unsigned long long)m.blocks);

    /* Release A, C and D: the three adjacent ranges must come back as one. */
    vb_mem_give(&a);
    live_del(&a);
    vb_mem_give(&c);
    live_del(&c);
    vb_mem_give(&d);
    live_del(&d);
    nv2a_vk_backend_memory_stats(&m);
    check(m.used_bytes == 0, "4: nothing is live in the block any more", "used_bytes %llu",
          (unsigned long long)m.used_bytes);
    check(m.largest_free >= 3u * S,
          "4: the three released ranges coalesce into one range wide enough for all three",
          "largest_free %llu, three ranges are %llu", (unsigned long long)m.largest_free,
          (unsigned long long)(3u * S));
    check(m.free_bytes >= 3u * S, "4: free bytes cover them too", "free_bytes %llu",
          (unsigned long long)m.free_bytes);
    check_free_lists("4: the coalesced free list still adds up");

    mr = mr_of(3u * S, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1, "4: a range as wide as all three", "");
    check(e.off == 0 && e.size >= 3u * S,
          "4: it was carved out of the coalesced free range",
          "off %llu, size %llu", (unsigned long long)e.off, (unsigned long long)e.size);
    nv2a_vk_backend_memory_stats(&m);
    check(m.reuse_hits >= 2u && m.used_bytes >= 3u * S,
          "4: reuse_hits counts it and used_bytes covers it", "reuse_hits %llu, used %llu",
          (unsigned long long)m.reuse_hits, (unsigned long long)m.used_bytes);
    check(sx.allocs == 1, "alloc: and still one allocation for the whole exercise",
          "%u call(s)", sx.allocs);
    check_free_lists("4: after the coalesced range is handed out");

    /* 4e: the aligned head of a split free range stays free. This is the case the head-vs-
     * padding repair exists for, and the one a "count the aligned head as padding" mutant
     * cannot pass: A (4 KiB), B (128 KiB) and C (4 KiB) fill the block's first 139264 bytes;
     * B is released, so [4096, 135168) is free; a 64 KiB request at alignment 65536 can only
     * start at 65536, which splits that free range into the head [4096, 65536) and the tail
     * [131072, 135168). The head is still free -- it is neither live nor padding -- so the
     * block's numbers must still add up, and it must still be able to serve a request. */
    gate_world();
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "4e: range A (4 KiB)", "");
    mr = mr_of(131072u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &b) == 1, "4e: range B (128 KiB)", "");
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &c) == 1, "4e: range C (4 KiB, so B is not last)", "");
    check(a.off == 0 && b.off == a.off + a.size && c.off == b.off + b.size,
          "4e: the three ranges are packed in order",
          "A %llu, B %llu, C %llu", (unsigned long long)a.off, (unsigned long long)b.off,
          (unsigned long long)c.off);
    ledger_add(&a);
    ledger_add(&b);
    ledger_add(&c);
    b1 = block_of(&a, &p1);
    ledger_give(&b);
    vb_mem_give(&b);                          /* [4096, 135168) is free now */
    mr = mr_of(65536u, 65536u, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &d) == 1, "4e: a 64 KiB range at alignment 65536", "");
    ledger_add(&d);
    check(d.off == 65536u && d.size >= 65536u,
          "4e: it starts at the first 65536-aligned byte inside the free range",
          "off %llu, size %llu", (unsigned long long)d.off, (unsigned long long)d.size);
    {
        unsigned i;
        int head = 0;
        VkDeviceSize sumfree = 0;

        for (i = 0; b1 && i < b1->nfree; i++) {
            sumfree += b1->free_list[i].size;
            if (b1->free_list[i].off == 4096u && b1->free_list[i].size == 61440u)
                head = 1;
        }
        check(head == 1,
              "4e: the aligned head [4096, 65536) is STILL on the block's free list",
              "nfree %u, sum %llu", b1 ? b1->nfree : 0u, (unsigned long long)sumfree);
        check(sumfree == 65536u,
              "4e: the free list is exactly the head plus the split's tail",
              "sum %llu (head 61440 + tail 4096)", (unsigned long long)sumfree);
    }
    check(b1 && b1->padding == 0,
          "4e: the head did not become padding: the block spent nothing aligning this take",
          "padding %llu", b1 ? (unsigned long long)b1->padding : 0ull);
    nv2a_vk_backend_memory_stats(&m);
    check(m.padding_bytes == 0,
          "4e: and the published padding_bytes did not absorb it either",
          "padding_bytes %llu", (unsigned long long)m.padding_bytes);
    check(m.used_bytes == ledger_sum(),
          "4e: used_bytes is exactly the fixture's own live extents (A, C and the new range)",
          "used_bytes %llu, ledger %llu (B's 131072 was released)",
          (unsigned long long)m.used_bytes, (unsigned long long)ledger_sum());
    check(m.free_bytes + m.padding_bytes == pools_reserved() - m.used_bytes,
          "4e: free + padding is the observed blocks minus used, with the head counted as free",
          "%llu + %llu vs %llu - %llu", (unsigned long long)m.free_bytes,
          (unsigned long long)m.padding_bytes, (unsigned long long)pools_reserved(),
          (unsigned long long)m.used_bytes);
    check(m.largest_free >= 61440u,
          "4e: largest_free can still serve the head (and the untouched tail)",
          "largest_free %llu, the head is 61440", (unsigned long long)m.largest_free);
    check_free_lists("4e: free-list sum + used + padding is still the bump, head included");
    /* And it can really be served: the head's own 61440 bytes take a 48 KiB request. */
    mr = mr_of(49152u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &e) == 1, "4e: a 48 KiB request after the split", "");
    check(e.off == 4096u,
          "4e: it came out of the head, so the head was free and usable",
          "off %llu (the head starts at 4096)", (unsigned long long)e.off);
    nv2a_vk_backend_memory_stats(&m);
    check(m.padding_bytes == 0 && m.reuse_hits >= 1u,
          "4e: still no padding, and the take was counted as a free-range reuse",
          "padding_bytes %llu, reuse_hits %llu", (unsigned long long)m.padding_bytes,
          (unsigned long long)m.reuse_hits);

    /* The fragmentation controls the packet asks to be reported: they are printed, and the
     * numbers they name are also required to satisfy their own definition. */
    note("counters: reserved %llu, used %llu, free %llu, largest_free %llu, padding %llu, "
         "metadata %llu", (unsigned long long)m.reserved_bytes,
         (unsigned long long)m.used_bytes, (unsigned long long)m.free_bytes,
         (unsigned long long)m.largest_free, (unsigned long long)m.padding_bytes,
         (unsigned long long)m.metadata_bytes);
    check(m.reserved_bytes == m.used_bytes + m.padding_bytes + m.free_bytes,
          "4: reserved = used + padding + free, with the fragmentation reported honestly",
          "%llu = %llu + %llu + %llu", (unsigned long long)m.reserved_bytes,
          (unsigned long long)m.used_bytes, (unsigned long long)m.padding_bytes,
          (unsigned long long)m.free_bytes);
    verdict("4 reuse, coalescing and the free lists", chk, bad);
}

/* ── 5. dedicated allocations ────────────────────────────────────────────────────────── */

static void prop5(void)
{
    Nv2aVkMemoryStats m, m0;
    VkMemoryRequirements mr;
    VbAlloc a;
    VkImage dimg = (VkImage)0x1234u;
    VkBuffer dbuf = (VkBuffer)0x5678u;
    uint64_t reasons;
    unsigned i;
    int chk = g_checks, bad = g_check_failures;

    section("5. oversized, required, preferred, no-coherent-host and out-of-room go dedicated");

    if (!VB_GATE_HAS_DEDICATED_PNEXT)
        check(0, "the dedicated allocation carries its pNext "
                 "(VB_GATE_HAS_DEDICATED_PNEXT)", "");
    if (!VB_GATE_HAS_NOHOST_REASON)
        check(0, "a request with no coherent host type is a dedicated reason "
                 "(VB_GATE_HAS_NOHOST_REASON)", "");

    gate_world();

    /* 5a: a range over VB_POOL_MAX_BLOCK cannot be pooled. */
    mr = mr_of((VkDeviceSize)VB_POOL_MAX_BLOCK + (1u << 20), VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "5: the oversized request succeeded", "");
    check(a.pool == -1 && a.block == -1 && a.off == 0 && a.mem != VK_NULL_HANDLE,
          "5: it is a dedicated allocation: pool -1, block -1, offset 0",
          "pool %d, block %d, off %llu", a.pool, a.block, (unsigned long long)a.off);
    check(a.size >= mr.size, "5: and it owns at least the requirement's bytes",
          "size %llu for %llu", (unsigned long long)a.size, (unsigned long long)mr.size);
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_oversize == 1 && m.dedicated == 1 && m.blocks == 0,
          "5: counted once, as the oversize reason, and no pool block was made",
          "oversize %llu, dedicated %llu, blocks %llu",
          (unsigned long long)m.dedicated_oversize, (unsigned long long)m.dedicated,
          (unsigned long long)m.blocks);

    /* 5b: requiresDedicatedAllocation. */
    mr = mr_of(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take_named(&mr, GX_WANT_DEV, 0, 1, 1, 0, &dimg, NULL, &a) == 1,
          "5: the requires-dedicated request succeeded", "");
    check(a.pool == -1 && sx.last_alloc_had_dedicated
          && sx.last_alloc_dedicated_image == dimg && sx.last_alloc_dedicated_buffer
          == VK_NULL_HANDLE,
          "5: its VkMemoryAllocateInfo.pNext names the image, to the stub",
          "pNext seen %u, image %p (wanted %p), buffer %p", sx.last_alloc_had_dedicated,
          (void *)sx.last_alloc_dedicated_image, (void *)dimg,
          (void *)sx.last_alloc_dedicated_buffer);
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_required == 1, "5: counted as the requires reason", "required %llu",
          (unsigned long long)m.dedicated_required);

    /* 5c: prefersDedicatedAllocation. */
    check(take_named(&mr, GX_WANT_DEV, 0, 1, 0, 1, NULL, &dbuf, &a) == 1,
          "5: the prefers-dedicated request succeeded", "");
    check(a.pool == -1 && sx.last_alloc_had_dedicated && sx.last_alloc_dedicated_buffer == dbuf,
          "5: its pNext names the buffer, to the stub",
          "pNext seen %u, buffer %p (wanted %p)", sx.last_alloc_had_dedicated,
          (void *)sx.last_alloc_dedicated_buffer, (void *)dbuf);
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_prefer == 1, "5: counted as the prefers reason", "prefer %llu",
          (unsigned long long)m.dedicated_prefer);

    /* 5d: no coherent host type. The only type these bits admit is host-visible and NOT
     * coherent, so the request cannot be pooled. */
    mr = mr_of(100u, VB_POOL_ALIGN, GX_BIT_NC);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &a) == 1,
          "5: the host request with no coherent type succeeded", "");
    check(a.pool == -1 && a.coherent == 0 && a.host_visible == 1,
          "5: it is a dedicated, host-visible, non-coherent allocation",
          "pool %d, coherent %d, host_visible %d", a.pool, a.coherent, a.host_visible);
    check(a.size >= mr.size, "5: sized to the requirement", "size %llu for %llu",
          (unsigned long long)a.size, (unsigned long long)mr.size);
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_nohost == 1, "5: counted as the no-coherent-host-type reason",
          "nohost %llu", (unsigned long long)m.dedicated_nohost);

    /* 5e: the pool's VB_POOL_RESERVE bounds it: five VB_POOL_MAX_BLOCK requests cannot all be
     * pooled, and the ones that are not are counted for a reason, not fabricated. */
    nv2a_vk_backend_memory_stats(&m0);
    mr = mr_of(VB_POOL_MAX_BLOCK, VB_POOL_ALIGN, GX_BIT_DEV);
    for (i = 0; i < 5u; i++) {
        VbAlloc big;
        char what[96];

        snprintf(what, sizeof what, "5: VB_POOL_MAX_BLOCK request %u of five succeeded", i + 1u);
        check(take(&mr, GX_WANT_DEV, 0, 1, &big) == 1, what,
              "pool %d, %llu bytes", big.pool, (unsigned long long)big.size);
    }
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_noroom >= m0.dedicated_noroom + 1u,
          "5: the pool stops growing at VB_POOL_RESERVE, and the request that cannot fit is "
          "dedicated for the no-room reason, not fabricated into a pooled range",
          "noroom %llu -> %llu", (unsigned long long)m0.dedicated_noroom,
          (unsigned long long)m.dedicated_noroom);
    /* reserved_bytes counts the dedicated allocations too (each reserves itself), so the
     * bound is the pools' own reservation: the blocks' whole sizes. */
    check(pools_reserved() <= (uint64_t)VB_POOL_RESERVE,
          "5: the pools' own reservation never exceeds VB_POOL_RESERVE (%u)",
          "the blocks hold %llu bytes (reserved_bytes %llu includes the dedicated ones)",
          (unsigned)VB_POOL_RESERVE, (unsigned long long)pools_reserved(),
          (unsigned long long)m.reserved_bytes);

    /* 5f: every dedicated reason is counted, and they sum to `dedicated`. */
    reasons = m.dedicated_required + m.dedicated_prefer + m.dedicated_oversize
            + m.dedicated_noroom + m.dedicated_nohost;
    check(reasons == m.dedicated,
          "5: dedicated == required + prefer + oversize + noroom + nohost (per-reason counters)",
          "%llu == %llu + %llu + %llu + %llu + %llu",
          (unsigned long long)m.dedicated, (unsigned long long)m.dedicated_required,
          (unsigned long long)m.dedicated_prefer, (unsigned long long)m.dedicated_oversize,
          (unsigned long long)m.dedicated_noroom, (unsigned long long)m.dedicated_nohost);
    check(m.alloc_calls == m.blocks + m.dedicated,
          "5: the allocator's own alloc_calls is the blocks plus the dedicated allocations",
          "%llu == %llu + %llu", (unsigned long long)m.alloc_calls,
          (unsigned long long)m.blocks, (unsigned long long)m.dedicated);
    check(sx.allocs == m.alloc_calls,
          "alloc: every vkAllocateMemory the allocator reports is one the driver's entry "
          "point really received (the fixture counted them at the stub)",
          "stub %u, alloc_calls %llu", sx.allocs, (unsigned long long)m.alloc_calls);

    /* 5g: a dedicated allocation is freed with its resource, exactly once. */
    {
        VbGarbage e;
        unsigned frees_before = sx.frees;

        memset(&e, 0, sizeof e);
        mr = mr_of(96u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
        spec(96u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
        if (take_named(&mr, GX_WANT_DEV, 0, 1, 0, 1, NULL, &dbuf, &a) == 1) {
            e.mem = a;
            vb_discard(e);
            check(sx.frees == frees_before + 1u && sx.double_frees == 0,
                  "5: the dedicated allocation was freed exactly once",
                  "%u free(s) for one dedicated range, %u double free(s)",
                  sx.frees - frees_before, sx.double_frees);
        } else {
            check(0, "5: the dedicated control allocation succeeded", "");
        }
    }
    verdict("5 dedicated allocations", chk, bad);
}

/* ── 6. delayed reuse ────────────────────────────────────────────────────────────────── */

static void prop6(void)
{
    Nv2aVkMemoryStats m;
    VkMemoryRequirements mr;
    VbAlloc a, b;
    VbBlock *bl;
    VbPool *pl = NULL;
    uint64_t before, after;
    VkDeviceSize a_off = 0;
    VkDeviceMemory a_mem = VK_NULL_HANDLE;
    const VkDeviceSize S = 4096u;
    int chk = g_checks, bad = g_check_failures;

    section("6. a range released while a recording is open, or a submission is pending, is "
            "not handed out");

    if (!VB_GATE_HAS_PARK)
        check(0, "releases are parked while the ring is busy (VB_GATE_HAS_PARK)", "");

    /* 6a: a recording is open. */
    gate_world();
    mr = mr_of(S, VB_POOL_ALIGN, GX_BIT_HOST);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &a) == 1, "6: range A", "");
    bl = block_of(&a, &pl);
    check(bl && bl->map && pl && pl->nblocks,
          "6: the host-visible block is mapped as a whole, so its bytes can be watched",
          "map %p", bl ? (void *)bl->map : NULL);
    if (!bl || !bl->map) {
        verdict("6 delayed reuse", chk, bad);
        return;
    }
    fill_bytes(bl->map + a.off, (size_t)S, 0xC0FFEEu);
    memcpy(g_snap, bl->map + a.off, (size_t)S);
    before = hash_bytes(g_snap, (size_t)S);

    a_off = a.off;
    a_mem = a.mem;
    g.slots[g.cur].recording = 1;             /* a recording is open */
    g.npending = 0;
    vb_mem_give(&a);
    nv2a_vk_backend_memory_stats(&m);
    check(m.parked_peak >= 1u && m.parked_lost == 0,
          "6: the release was parked, not handed out (and the park list did not overflow)",
          "parked %llu, peak %llu, lost %llu", (unsigned long long)m.parked,
          (unsigned long long)m.parked_peak, (unsigned long long)m.parked_lost);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &b) == 1, "6: range B", "");
    check(b.off != a_off || b.mem != a_mem,
          "6: the parked range is NOT what the next request gets",
          "B.off %llu vs A.off %llu", (unsigned long long)b.off, (unsigned long long)a_off);
    after = hash_bytes(bl->map + a.off, (size_t)S);
    check(before == after && memcmp(g_snap, bl->map + a.off, (size_t)S) == 0,
          "6: the parked range's bytes are byte-identical -- nothing that may still read them "
          "was overwritten", "checksum 0x%016llX -> 0x%016llX, %zu of %llu bytes differ",
          (unsigned long long)before, (unsigned long long)after,
          count_diff(g_snap, bl->map + a.off, (size_t)S), (unsigned long long)S);
    check(g.slots[g.cur].recording == 1, "6: the fixture's recording is still open", "");

    /* Drain: close the recording, empty the ring, and let the pool collect. */
    g.slots[g.cur].recording = 0;
    g.npending = 0;
    vb_pool_gc();
    nv2a_vk_backend_memory_stats(&m);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &b) == 1, "6: range C, after the drain",
          "");
    check(b.off == a_off && b.mem == a_mem,
          "6: once the recording is closed and the ring drained, the range IS reused",
          "C.off %llu, A.off %llu", (unsigned long long)b.off, (unsigned long long)a_off);

    /* 6b: a submission is pending -- no recording, but the GPU's work is in flight. */
    gate_world();
    mr = mr_of(S, VB_POOL_ALIGN, GX_BIT_HOST);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &a) == 1, "6: range A (pending case)", "");
    bl = block_of(&a, &pl);
    if (!bl || !bl->map) {
        check(0, "6: the host-visible block is mapped", "");
        verdict("6 delayed reuse", chk, bad);
        return;
    }
    fill_bytes(bl->map + a.off, (size_t)S, 0xBEEF01u);
    memcpy(g_snap, bl->map + a.off, (size_t)S);
    before = hash_bytes(g_snap, (size_t)S);
    a_off = a.off;
    a_mem = a.mem;
    g.slots[g.cur].recording = 0;
    g.npending = 1;                            /* a submission is in flight */
    vb_mem_give(&a);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &b) == 1, "6: range B (pending case)", "");
    check(b.off != a_off || b.mem != a_mem,
          "6: a range a pending submission may still read is not handed out either",
          "B.off %llu vs A.off %llu", (unsigned long long)b.off, (unsigned long long)a_off);
    after = hash_bytes(bl->map + a.off, (size_t)S);
    check(before == after && memcmp(g_snap, bl->map + a.off, (size_t)S) == 0,
          "6: and the pending submission's bytes are byte-identical afterwards",
          "checksum 0x%016llX -> 0x%016llX, %zu of %llu bytes differ",
          (unsigned long long)before, (unsigned long long)after,
          count_diff(g_snap, bl->map + a.off, (size_t)S), (unsigned long long)S);
    g.npending = 0;
    vb_pool_gc();
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &b) == 1, "6: range C (pending case)", "");
    check(b.off == a_off && b.mem == a_mem,
          "6: after the fence and the drain it is reused", "C.off %llu, A.off %llu",
          (unsigned long long)b.off, (unsigned long long)a_off);
    nv2a_vk_backend_memory_stats(&m);
    check(m.parked_lost == 0, "6: nothing was parked permanently", "parked_lost %llu",
          (unsigned long long)m.parked_lost);

    /* 6c: a full park list. VB_POOL_PENDING releases can be parked; the one after them cannot
     * be held, and the allocator's rule is that it is then never handed out again and never
     * freed -- the one leak it may have. The fixture fills the list while a submission is
     * pending, checks the excess range is not reusable, and checks that nothing was freed. */
    gate_world();
    {
        VbAlloc r[VB_POOL_PENDING + 1];
        VbBlock *bl = NULL;
        VkDeviceSize lost_off = 0;
        unsigned i, n = VB_POOL_PENDING + 1u;
        int ok_take = 1;

        mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_HOST);
        for (i = 0; i < n; i++) {
            if (take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &r[i]) != 1) {
                ok_take = 0;
                break;
            }
            ledger_add(&r[i]);
        }
        check(ok_take == 1, "6c: every range was taken (one block holds them all)",
              "%u of %u, blocks %llu", i, n,
              (unsigned long long)(i ? 1u : 0u));
        nv2a_vk_backend_memory_stats(&m);
        check(m.blocks == 1u && sx.allocs == 1u,
              "alloc: VB_POOL_PENDING + 1 ranges cost one block", "blocks %llu, %u call(s)",
              (unsigned long long)m.blocks, sx.allocs);
        lost_off = r[n - 1u].off;             /* the last one is the one the list cannot hold */
        bl = block_of(&r[0], NULL);           /* the block, before the gives zero the VbAllocs */
        ledger_lost(&r[n - 1u]);              /* the fixture mirrors the bounded-list rule */
        g.npending = 1;                       /* a submission is in flight: gives park */
        for (i = 0; i < n; i++) {
            ledger_give(&r[i]);
            vb_mem_give(&r[i]);
        }
        nv2a_vk_backend_memory_stats(&m);
        check(m.parked_peak == (uint64_t)VB_POOL_PENDING && m.parked == (uint64_t)n,
              "6c: the park list filled to its budget and then refused the excess",
              "parked %llu, peak %llu, budget %u", (unsigned long long)m.parked,
              (unsigned long long)m.parked_peak, (unsigned)VB_POOL_PENDING);
        check(m.parked_lost == 1u,
              "6c: the range the list could not hold is counted as lost, not fabricated",
              "parked_lost %llu", (unsigned long long)m.parked_lost);
        check(sx.frees == 0u && m.blocks == 1u,
              "6c: nothing was freed and no block was retired for it", "%u free(s), blocks %llu",
              sx.frees, (unsigned long long)m.blocks);
        check(m.used_bytes == ledger_sum(),
              "6c: the lost range still counts as live bytes (it is never handed out again)",
              "used_bytes %llu, ledger %llu", (unsigned long long)m.used_bytes,
              (unsigned long long)ledger_sum());

        /* Drain: the parked ones fold back, the lost one must not. */
        g.npending = 0;
        vb_pool_gc();
        ledger_drain();
        nv2a_vk_backend_memory_stats(&m);
        check(m.used_bytes == 4096u && ledger_sum() == 4096u,
              "6c: after the drain only the lost range is still live",
              "used_bytes %llu, ledger %llu", (unsigned long long)m.used_bytes,
              (unsigned long long)ledger_sum());
        {
            unsigned k;
            int covered = 0;

            for (k = 0; bl && k < bl->nfree; k++)
                if (bl->free_list[k].off < lost_off + 4096u
                    && lost_off < bl->free_list[k].off + bl->free_list[k].size)
                    covered = 1;
            check(covered == 0,
                  "6c: no free range covers the lost range: it is never reusable",
                  "lost [%llu, %llu), nfree %u", (unsigned long long)lost_off,
                  (unsigned long long)(lost_off + 4096u), bl ? bl->nfree : 0u);
        }
        check_free_lists("6c: the block's numbers still add up with the lost range in used");
        check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &r[0]) == 1,
              "6c: a following request still succeeds", "");
        nv2a_vk_backend_memory_stats(&m);
        check(r[0].off != lost_off && m.reuse_hits >= 1u,
              "6c: and it is served from the folded parked ranges, not the lost one",
              "off %llu vs the lost %llu, reuse_hits %llu", (unsigned long long)r[0].off,
              (unsigned long long)lost_off, (unsigned long long)m.reuse_hits);
    }
    verdict("6 delayed reuse", chk, bad);
}

/* ── 7. rollback on injected failures ────────────────────────────────────────────────── */

static void clear_latch(void)
{
    /* The fixture's own scenario, not production's: a latched failure would otherwise stop
     * the next take in a gate that is deliberately driving failures. */
#if VB_GATE_HAS_FAILED_LATCH
    g.failed = 0;
#endif
    g.ok = 1;
}

static void prop7(void)
{
    Nv2aVkMemoryStats m, m0;
    VkMemoryRequirements mr;
    VbAlloc a, b;
    VbImage im;
    VkBuffer buf = VK_NULL_HANDLE;
    VbAlloc mem;
    uint8_t *ptr = NULL;
    VkDeviceSize first_off;
    int chk = g_checks, bad = g_check_failures;

    section("7. injected failures release exactly what was taken");
    note("every VkResult below is a stub's return value: no device is lost and no allocation "
         "of this process ever failed");

    /* 7a: vkAllocateMemory fails for the block AND for the once-permitted fallback: the take
     * fails, nothing is registered, nothing leaks. */
    gate_world();
    sx.alloc_fail_at = 1u;
    sx.alloc_fail_times = 8u;
    mr = mr_of(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 0,
          "7a: the take fails when no allocation can be made", "returned %d", 0);
    check(a.mem == VK_NULL_HANDLE,
          "7a: the caller's VbAlloc names no memory (nothing usable is handed back)", "");
    check(sx.allocs >= 1u && sx.frees == 0u && sx.double_frees == 0,
          "7a: nothing that failed to allocate was freed, and nothing was freed twice",
          "%u allocation attempt(s), %u free(s), %u double free(s)", sx.allocs, sx.frees,
          sx.double_frees);
    nv2a_vk_backend_memory_stats(&m);
    check(m.blocks == 0 && m.reserved_bytes == 0 && m.alloc_failures >= 1u,
          "7a: no block was registered and the failure was counted",
          "blocks %llu, reserved %llu, alloc_failures %llu",
          (unsigned long long)m.blocks, (unsigned long long)m.reserved_bytes,
          (unsigned long long)m.alloc_failures);
    check(m.rollbacks == 0 || m.blocks == 0, "7a: and no half-made block is left behind",
          "rollbacks %llu, blocks %llu", (unsigned long long)m.rollbacks,
          (unsigned long long)m.blocks);

    /* 7b: the block's allocation fails once, and the fallback dedicated allocation succeeds:
     * one fallback, counted, and the range is a dedicated one. */
    gate_world();
    sx.alloc_fail_at = 1u;
    sx.alloc_fail_times = 1u;
    mr = mr_of(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1,
          "7b: a failed block allocation falls back once, and the request still succeeds", "");
    check(a.mem != VK_NULL_HANDLE && a.pool == -1,
          "7b: what came back is the dedicated allocation, not a pooled range",
          "pool %d, mem %p", a.pool, (void *)a.mem);
    nv2a_vk_backend_memory_stats(&m);
    check(m.fallbacks == 1u, "7b: the fallback was counted once", "fallbacks %llu",
          (unsigned long long)m.fallbacks);
    check(sx.allocs == 2u && sx.frees == 0u,
          "7b: exactly two allocation attempts and nothing freed", "%u attempt(s), %u free(s)",
          sx.allocs, sx.frees);
    check(a.size >= mr.size, "7b: the fallback still satisfies the requirement",
          "size %llu for %llu", (unsigned long long)a.size, (unsigned long long)mr.size);

    /* 7c: vkCreateImage fails: nothing is allocated, the caller's image is unusable. */
    gate_world();
    sx.create_image_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    memset(&im, 0, sizeof im);
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 0,
          "7c: the image creation reports failure", "");
    check(im.img == VK_NULL_HANDLE && im.mem.mem == VK_NULL_HANDLE,
          "7c: the caller's VbImage names no image and no memory", "");
    check(sx.allocs == 0 && sx.binds_img == 0 && sx.destroys_img == 0 && sx.double_frees == 0,
          "7c: nothing was allocated, bound or destroyed", "%u alloc(s), %u bind(s), %u "
          "destroy(s)", sx.allocs, sx.binds_img, sx.destroys_img);

    /* 7d: vkCreateBuffer fails: same, for the host-visible maker. */
    gate_world();
    sx.create_buffer_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    spec(4096u, VB_POOL_ALIGN, GX_BIT_HOST, 0, 0);
    check(vb_host_buffer(4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, &buf, &mem, &ptr) == 0,
          "7d: the host buffer creation reports failure", "");
    check(buf == VK_NULL_HANDLE && mem.mem == VK_NULL_HANDLE && ptr == NULL,
          "7d: no buffer, no memory and no pointer is handed back", "ptr %p", (void *)ptr);
    check(sx.allocs == 0 && sx.maps == 0 && sx.destroys_buf == 0,
          "7d: nothing was allocated, mapped or destroyed", "%u alloc(s), %u map(s), %u "
          "destroy(s)", sx.allocs, sx.maps, sx.destroys_buf);

    /* 7e: vkBindImageMemory fails on a DEDICATED range: the dedicated memory is freed once,
     * the image is destroyed, nothing dangles. */
    gate_world();
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 1, 0);   /* requires dedicated */
    sx.bind_image_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    memset(&im, 0, sizeof im);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 0,
          "7e: a failed bind fails the image creation", "");
    check(sx.allocs == 1u && sx.frees == 1u && sx.double_frees == 0,
          "7e: the dedicated memory was taken and released exactly once",
          "%u alloc(s), %u free(s), %u double free(s)", sx.allocs, sx.frees, sx.double_frees);
    check(sx.destroys_img == 1u && sx.creates_view == 0u,
          "7e: the half-made image was destroyed and no view was made",
          "%u destroy(s), %u view(s)", sx.destroys_img, sx.creates_view);
    check(im.img == VK_NULL_HANDLE && im.mem.mem == VK_NULL_HANDLE,
          "7e: the caller's VbImage is unusable, with no dangling VbAlloc", "");
    nv2a_vk_backend_memory_stats(&m0);
    check(m0.bind_failures >= 1u && m0.rollbacks >= 1u,
          "7e: the bind failure and the rollback are both counted", "bind_failures %llu, "
          "rollbacks %llu", (unsigned long long)m0.bind_failures,
          (unsigned long long)m0.rollbacks);
    /* The next successful creation gets a NEW object, and the old one is not freed again. */
    clear_latch();
    sx.bind_image_result = VK_SUCCESS;
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 1, 0);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 1,
          "7e: a following creation succeeds", "");
    check(sx.allocs == 2u && sx.frees == 1u && sx.double_frees == 0,
          "7e: it took a second allocation, and the first one was still freed only once",
          "%u alloc(s), %u free(s), %u double free(s)", sx.allocs, sx.frees, sx.double_frees);

    /* 7f: vkBindImageMemory fails on a POOLED range: the range goes back to the pool, the
     * block is not freed, and the next request gets that same range. A filler range is taken
     * first, so the image's own range is NOT at offset 0 and the reuse is observable. */
    gate_world();
    mr = mr_of(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "7f: a filler range, so the image's is not "
          "at 0", "");
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    sx.bind_image_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    memset(&im, 0, sizeof im);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 0, "7f: the creation failed", "");
    first_off = sx.bind_img_off[0];
    nv2a_vk_backend_memory_stats(&m0);
    check(first_off == 64u * 1024u,
          "7f: the failed bind named the range after the filler", "offset %llu",
          (unsigned long long)first_off);
    check(m0.used_bytes == 64u * 1024u && m0.parked == 0u,
          "7f: the image's range is back (only the filler's 64 KiB is live) and it was not "
          "parked: nothing was recording",
          "used_bytes %llu, parked %llu", (unsigned long long)m0.used_bytes,
          (unsigned long long)m0.parked);
    check(sx.allocs == 1u && sx.frees == 0u,
          "7f: the pool's block is NOT freed for one failed bind", "%u alloc(s), %u free(s)",
          sx.allocs, sx.frees);
    check(im.mem.mem == VK_NULL_HANDLE && sx.destroys_img == 1u,
          "7f: the image was destroyed and the caller holds nothing", "%u destroy(s)",
          sx.destroys_img);
    clear_latch();
    sx.bind_image_result = VK_SUCCESS;
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 1,
          "7f: a following creation succeeds", "");
    check(sx.bind_img_off[1] == first_off,
          "7f: and it binds to the SAME offset -- the range really came back, nothing dangles",
          "second bind at %llu, first at %llu", (unsigned long long)sx.bind_img_off[1],
          (unsigned long long)first_off);
    check(sx.allocs == 1u && sx.double_frees == 0,
          "alloc: the rollback needed no further allocation", "%u call(s)", sx.allocs);

    /* 7g: vkBindBufferMemory fails in the device-local buffer maker. */
    gate_world();
    spec(4096u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    sx.bind_buffer_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    check(vb_device_buffer(4096u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &buf, &mem) == 0,
          "7g: the device buffer creation reports failure", "");
    check(buf == VK_NULL_HANDLE && mem.mem == VK_NULL_HANDLE,
          "7g: no buffer and no memory is handed back", "");
    check(sx.destroys_buf == 1u && sx.double_frees == 0,
          "7g: its buffer was destroyed once", "%u destroy(s)", sx.destroys_buf);

    /* 7h: vkMapMemory fails on a pooled host-visible range: the buffer is destroyed, the
     * range is released, and no pointer is handed back. */
    gate_world();
    spec(4096u, VB_POOL_ALIGN, GX_BIT_HOST, 0, 0);
    sx.map_result = VK_ERROR_MEMORY_MAP_FAILED;
    check(vb_host_buffer(4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, &buf, &mem, &ptr) == 0,
          "7h: the mapping failure fails the host buffer creation", "");
    check(buf == VK_NULL_HANDLE && mem.mem == VK_NULL_HANDLE && ptr == NULL,
          "7h: nothing usable is handed back: no buffer, no memory, no pointer", "%p",
          (void *)ptr);
    check(sx.destroys_buf == 1u && sx.double_frees == 0,
          "7h: the buffer was destroyed exactly once", "%u destroy(s), %u double free(s)",
          sx.destroys_buf, sx.double_frees);
    nv2a_vk_backend_memory_stats(&m);
    check(m.map_failures >= 1u, "7h: the mapping failure is counted", "map_failures %llu",
          (unsigned long long)m.map_failures);
    check(m.used_bytes == 0 || m.parked >= 1u,
          "7h: the range went back with the failure", "used_bytes %llu, parked %llu",
          (unsigned long long)m.used_bytes, (unsigned long long)m.parked);

    /* 7i: the map failure of a DEDICATED range: the memory is freed too. */
    gate_world();
    spec(4096u, VB_POOL_ALIGN, GX_BIT_NC, 0, 0);
    sx.map_result = VK_ERROR_MEMORY_MAP_FAILED;
    check(vb_host_buffer(4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, &buf, &mem, &ptr) == 0,
          "7i: the same failure on a dedicated (non-coherent, so unpooled) allocation", "");
    check(sx.allocs == 1u && sx.frees == 1u && sx.double_frees == 0,
          "7i: its memory was freed exactly once", "%u alloc(s), %u free(s), %u double free(s)",
          sx.allocs, sx.frees, sx.double_frees);
    check(mem.mem == VK_NULL_HANDLE, "7i: no dangling allocation is handed back", "");
    check(sx.maps_after_free == 0, "7i: nothing was mapped after a free",
          "%u map(s) after free", sx.maps_after_free);

    /* 7k: the same rollback with a recording open. The range cannot go back to the free list
     * while a recording may still read it, so it must be PARKED: it still counts as live, the
     * next request does not get it, and only after the recording is closed and the pool has
     * collected is it handed out again. */
    gate_world();
    mr = mr_of(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &a) == 1, "7k: a filler range", "");
    ledger_add(&a);
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    sx.bind_image_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    g.slots[g.cur].recording = 1;             /* a recording is open */
    memset(&im, 0, sizeof im);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 0, "7k: the creation failed", "");
    first_off = sx.bind_img_off[0];
    check(first_off == 64u * 1024u, "7k: the failed bind named the range after the filler",
          "offset %llu", (unsigned long long)first_off);
    nv2a_vk_backend_memory_stats(&m);
    check(m.used_bytes == 128u * 1024u && m.parked >= 1u && m.parked_peak >= 1u,
          "7k: the range is PARKED, not reused: it still counts as live bytes",
          "used_bytes %llu (the filler plus the parked range), parked %llu, peak %llu",
          (unsigned long long)m.used_bytes, (unsigned long long)m.parked,
          (unsigned long long)m.parked_peak);
    check(sx.frees == 0u && m.blocks == 1u,
          "7k: nothing was freed", "%u free(s), blocks %llu", sx.frees,
          (unsigned long long)m.blocks);
    clear_latch();
    check(take(&mr, GX_WANT_DEV, 0, 1, &b) == 1, "7k: a request while the recording is open", "");
    check(b.off != first_off,
          "7k: it does NOT get the parked range", "off %llu vs the parked %llu",
          (unsigned long long)b.off, (unsigned long long)first_off);
    /* Drain: close the recording, let the pool collect, and the parked range comes back. */
    g.slots[g.cur].recording = 0;
    g.npending = 0;
    vb_pool_gc();
    check(take(&mr, GX_WANT_DEV, 0, 1, &b) == 1, "7k: a request after the drain", "");
    check(b.off == first_off && b.mem == a.mem,
          "7k: now it IS the parked range -- parked, then folded, then reused",
          "off %llu (the parked %llu), mem %p (the block %p)", (unsigned long long)b.off,
          (unsigned long long)first_off, (void *)b.mem, (void *)a.mem);

    /* 7j: the control: every one of these paths succeeds when nothing is injected. */
    gate_world();
    spec(64u * 1024u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    check(vb_image_create(&im, 64u, 64u, VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT) == 1
          && im.mem.mem != VK_NULL_HANDLE && sx.destroys_img == 0,
          "7j: control -- the same image creation succeeds with nothing injected", "");
    spec(4096u, VB_POOL_ALIGN, GX_BIT_HOST, 0, 0);
    check(vb_host_buffer(4096u, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 0, &buf, &mem, &ptr) == 1
          && ptr && mem.mem != VK_NULL_HANDLE,
          "7j: control -- the host buffer is created, bound and mapped", "");
    spec(4096u, VB_POOL_ALIGN, GX_BIT_DEV, 0, 0);
    check(vb_device_buffer(4096u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &buf, &mem) == 1
          && mem.mem != VK_NULL_HANDLE,
          "7j: control -- the device buffer is created and bound", "");
    check(sx.double_frees == 0 && sx.maps_after_free == 0,
          "7j: control -- nothing was freed twice or mapped after a free", "%u double free(s)",
          sx.double_frees);
    (void)b;
    verdict("7 rollback on injected failures", chk, bad);
}

/* ── 8. mapping identity, and the non-coherent atom arithmetic ───────────────────────── */

static void prop8(void)
{
    Nv2aVkMemoryStats m;
    VkMemoryRequirements mr;
    VbAlloc a, b, nc;
    VbBlock *bl = NULL;
    VbPool *pl = NULL;
    VkDeviceSize lo, hi;
    int chk = g_checks, bad = g_check_failures;

    section("8. a pooled pointer is its block's mapping, and the flush windows are exact");

    if (!VB_GATE_HAS_ATOM_MATH)
        check(0, "the non-coherent atom widening exists (VB_GATE_HAS_ATOM_MATH)", "");
    if (!VB_GATE_HAS_BLOCK_MAP)
        check(0, "one mapping per host-visible block (VB_GATE_HAS_BLOCK_MAP)", "");

    /* 8a: the pointer identity, and a coherent range's flush: exactly the requested bytes. */
    gate_world();
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_HOST);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &a) == 1, "8: range A", "");
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &b) == 1, "8: range B (A's neighbour)",
          "");
    bl = block_of(&a, &pl);
    if (bl && bl->map) {
        uint8_t *p = bl->map + a.off;

        check(p != bl->map + b.off && b.off == a.off + a.size,
              "8: the two ranges are neighbours in the one mapped block",
              "A [%llu, %llu), B [%llu, %llu)", (unsigned long long)a.off,
              (unsigned long long)(a.off + a.size), (unsigned long long)b.off,
              (unsigned long long)(b.off + b.size));
    }
    check(sx.maps == 1u,
          "8: the pool mapped the whole block exactly once, not once per range",
          "%u map(s) for %u ranges", sx.maps, 2u);
    check(a.coherent == 1 && a.host_visible == 1,
          "8: the pooled range's memory type is host-visible and coherent",
          "coherent %d, host_visible %d", a.coherent, a.host_visible);
    /* A coherent range needs no flush at all: the write is already visible, so the call is a
     * no-op that reaches no driver entry point -- and cannot widen over a neighbour. */
    sx.n_flush = sx.n_inval = 0;
    check(vb_mem_flush(&a, 100u, 200u) == 1 && sx.flush_calls == 0 && sx.n_flush == 0u,
          "8: a coherent range's flush is a no-op -- no driver call, nothing widened",
          "flush_calls %u", sx.flush_calls);
    /* An invalidate IS issued for coherent memory (the publication always did), and its window
     * is exactly the requested sub-range: no atom widening on a coherent type. */
    check(vb_mem_invalidate(&a, 100u, 200u) == 1 && sx.n_inval == 1u,
          "8: the coherent invalidate reached the driver once", "%u call(s)", sx.inval_calls);
    if (sx.n_inval >= 1u) {
        check(sx.inval[0].memory == a.mem && sx.inval[0].offset == a.off + 100u
              && sx.inval[0].size == 200u,
              "8: a coherent range's window covers EXACTLY the requested sub-range, with no "
              "atom widening",
              "mem %p offset %llu size %llu (wanted %p, %llu, 200)",
              (void *)sx.inval[0].memory, (unsigned long long)sx.inval[0].offset,
              (unsigned long long)sx.inval[0].size, (void *)a.mem,
              (unsigned long long)(a.off + 100u));
        check(sx.inval[0].offset + sx.inval[0].size <= a.off + a.size
              && (a.off + a.size <= b.off),
              "8: and it cannot reach the neighbouring live range's bytes",
              "window ends at %llu, A ends at %llu, B starts at %llu",
              (unsigned long long)(sx.inval[0].offset + sx.inval[0].size),
              (unsigned long long)(a.off + a.size), (unsigned long long)b.off);
    }
    sx.n_inval = 0;
    check(vb_mem_invalidate(&a, 0u, 4096u) == 1 && sx.n_inval == 1u,
          "8: the whole range's invalidate covers it exactly once", "%u call(s)",
          sx.inval_calls);
    if (sx.n_inval >= 1u)
        check(sx.inval[0].offset == a.off && sx.inval[0].size == a.size,
              "8: exactly the range, byte for byte",
              "offset %llu size %llu for a range at %llu of %llu",
              (unsigned long long)sx.inval[0].offset, (unsigned long long)sx.inval[0].size,
              (unsigned long long)a.off, (unsigned long long)a.size);
    /* The clip: a request that runs past the range's end is clipped to the range. */
    sx.n_inval = 0;
    check(vb_mem_invalidate(&a, a.size - 10u, 1000u) == 1, "8: an overlong request succeeds",
          "");
    if (sx.n_inval >= 1u)
        check(sx.inval[0].offset >= a.off
              && sx.inval[0].offset + sx.inval[0].size <= a.off + a.size,
              "8: it is clipped to the range, never past it",
              "[%llu, %llu) inside [%llu, %llu)", (unsigned long long)sx.inval[0].offset,
              (unsigned long long)(sx.inval[0].offset + sx.inval[0].size),
              (unsigned long long)a.off, (unsigned long long)(a.off + a.size));

    /* 8b: a non-coherent allocation (never pooled, so dedicated) is widened to whole atoms
     * and clipped to the allocation. */
    gate_world();
    g.atom = GX_ATOM;
    mr = mr_of(100u, VB_POOL_ALIGN, GX_BIT_NC);
    check(take(&mr, GX_WANT_HOST, GX_WANT_HOSTVIS, 0, &nc) == 1,
          "8: the non-coherent request succeeded", "");
    check(nc.coherent == 0 && nc.pool == -1 && nc.off == 0 && nc.size >= 100u,
          "8: it is a dedicated, non-coherent allocation at offset 0",
          "coherent %d, pool %d, off %llu, size %llu", nc.coherent, nc.pool,
          (unsigned long long)nc.off, (unsigned long long)nc.size);
    sx.n_flush = sx.n_inval = 0;
    check(vb_mem_flush(&nc, 1u, 3u) == 1 && sx.n_flush == 1u,
          "8: the non-coherent flush reached the driver once", "%u call(s)", sx.flush_calls);
    if (sx.n_flush >= 1u) {
        lo = 0;                                     /* floor(1, 64) */
        hi = GX_ATOM;                               /* ceil(4, 64) */
        if (hi > nc.size)
            hi = nc.size;
        check(sx.flush[0].memory == nc.mem && sx.flush[0].offset == nc.off + lo
              && sx.flush[0].size == hi - lo,
              "8: [1, 4) is widened to exactly [0, 64) -- whole nonCoherentAtomSize atoms",
              "got offset %llu size %llu, wanted offset %llu size %llu",
              (unsigned long long)sx.flush[0].offset, (unsigned long long)sx.flush[0].size,
              (unsigned long long)(nc.off + lo), (unsigned long long)(hi - lo));
    }
    sx.n_flush = 0;
    check(vb_mem_flush(&nc, 60u, 10u) == 1, "8: a flush straddling an atom boundary", "");
    if (sx.n_flush >= 1u) {
        lo = 0;                                     /* floor(60, 64) */
        hi = 2u * GX_ATOM;                          /* ceil(70, 64), then clipped below */
        if (hi > nc.size)
            hi = nc.size;
        check(sx.flush[0].offset == lo && sx.flush[0].size == hi - lo,
              "8: [60, 70) widens to [0, 128) and is clipped to the allocation's 100 bytes",
              "got offset %llu size %llu, wanted offset %llu size %llu",
              (unsigned long long)sx.flush[0].offset, (unsigned long long)sx.flush[0].size,
              (unsigned long long)lo, (unsigned long long)(hi - lo));
    }
    sx.n_inval = 0;
    check(vb_mem_invalidate(&nc, 64u, 36u) == 1, "8: an invalidate ending at the boundary", "");
    if (sx.n_inval >= 1u) {
        lo = 64u;
        hi = GX_ATOM * 2u;
        if (hi > nc.size)
            hi = nc.size;
        check(sx.inval[0].offset == lo && sx.inval[0].size == hi - lo
              && hi == nc.size,
              "8: [64, 100) is widened to [64, 128) and clipped to the range's end, so it "
              "covers no byte the allocation does not own",
              "got offset %llu size %llu, wanted offset %llu size %llu (allocation %llu)",
              (unsigned long long)sx.inval[0].offset, (unsigned long long)sx.inval[0].size,
              (unsigned long long)lo, (unsigned long long)(hi - lo),
              (unsigned long long)nc.size);
    }
    {
        unsigned i;
        int inside = 1;

        for (i = 0; i < sx.n_flush + sx.n_inval; i++) {
            const VkMappedMemoryRange *r = i < sx.n_flush ? &sx.flush[i] : &sx.inval[i - sx.n_flush];

            if (r->memory == nc.mem && (r->offset < nc.off || r->offset + r->size > nc.off + nc.size))
                inside = 0;
        }
        check(inside, "8: every non-coherent window stays inside the allocation", "");
    }
    nv2a_vk_backend_memory_stats(&m);
    check(m.dedicated_nohost >= 1u && m.parked_lost == 0,
          "8: the non-coherent request was dedicated and nothing was parked permanently",
          "nohost %llu, parked_lost %llu", (unsigned long long)m.dedicated_nohost,
          (unsigned long long)m.parked_lost);
    verdict("8 mapping identity and the atom arithmetic", chk, bad);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ────────── */

/* ── 9. retirement and the retention policy ─────────────────────────────────────────────
 *
 * VB_POOL_KEEP_EMPTY bounds what a pool may keep: an empty block no larger than it is kept for
 * reuse, a larger one is retired (vkFreeMemory) the next time the pool collects. The sizing
 * rule makes those two classes easy to reach: a 9 MiB range gets a 9 MiB block (over the
 * budget) and a small range gets a 4 MiB one (exactly the budget). This property decides that
 * both happen, that the retired bytes really reach vkFreeMemory, and that the kept block is
 * still usable -- so deleting the retention or the retirement code fails it. */
static void prop9(void)
{
    Nv2aVkMemoryStats m;
    VkMemoryRequirements mr;
    VbAlloc big, small, again;
    VkDeviceMemory kept_mem = VK_NULL_HANDLE;
    uint64_t reserved_before, allocs_before;
    int chk = g_checks, bad = g_check_failures;

    section("9. an over-budget empty block is retired, an at-budget one is kept");

    if (!VB_GATE_HAS_RETIRE)
        check(0, "the source retires empty blocks under a retention budget "
                 "(VB_GATE_HAS_RETIRE)", "");

    gate_world();
    mr = mr_of((VkDeviceSize)9u << 20, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &big) == 1, "9: a 9 MiB range", "");
    mr = mr_of(4096u, VB_POOL_ALIGN, GX_BIT_DEV);
    check(take(&mr, GX_WANT_DEV, 0, 1, &small) == 1, "9: a 4 KiB range", "");
    check(big.mem != small.mem && big.pool == small.pool,
          "9: the two ranges are in different blocks of one pool",
          "mem %p/%p, pool %d/%d", (void *)big.mem, (void *)small.mem, big.pool, small.pool);
    {
        VbBlock *bb = block_of(&big, NULL), *sb = block_of(&small, NULL);

        check(bb && sb && bb->size > (VkDeviceSize)VB_POOL_KEEP_EMPTY
              && sb->size <= (VkDeviceSize)VB_POOL_KEEP_EMPTY,
              "9: one block is over the retention budget and the other is within it",
              "sizes %llu and %llu, VB_POOL_KEEP_EMPTY %u",
              bb ? (unsigned long long)bb->size : 0ull,
              sb ? (unsigned long long)sb->size : 0ull, (unsigned)VB_POOL_KEEP_EMPTY);
    }
    reserved_before = pools_reserved();
    allocs_before = sx.allocs;
    kept_mem = small.mem;
    nv2a_vk_backend_memory_stats(&m);
    check(m.blocks == 2u && m.blocks_retired == 0u && sx.frees == 0u,
          "9: two blocks, nothing retired or freed while they are in use",
          "blocks %llu, retired %llu, %u free(s)", (unsigned long long)m.blocks,
          (unsigned long long)m.blocks_retired, sx.frees);

    vb_mem_give(&big);
    vb_mem_give(&small);
    vb_pool_gc();                             /* nothing is recording or pending */
    nv2a_vk_backend_memory_stats(&m);
    check(m.blocks_retired == 1u,
          "9: exactly the over-budget empty block was retired", "blocks_retired %llu",
          (unsigned long long)m.blocks_retired);
    check(sx.frees == 1u && sx.double_frees == 0,
          "9: the retired block's memory reached vkFreeMemory exactly once", "%u free(s), "
          "%u double free(s)", sx.frees, sx.double_frees);
    check(pools_reserved() == reserved_before - ((VkDeviceSize)9u << 20),
          "9: the pool's reservation dropped by the retired block's size",
          "reserved %llu -> %llu", (unsigned long long)reserved_before,
          (unsigned long long)pools_reserved());
    check(m.reserved_bytes == pools_reserved() && m.used_bytes == 0u,
          "9: the published reservation is the kept block and nothing is live",
          "reserved_bytes %llu vs %llu, used_bytes %llu",
          (unsigned long long)m.reserved_bytes, (unsigned long long)pools_reserved(),
          (unsigned long long)m.used_bytes);
    check_free_lists("9: the kept block's numbers still add up");
    check(take(&mr, GX_WANT_DEV, 0, 1, &again) == 1,
          "9: a following small request still succeeds", "");
    check(again.mem == kept_mem && again.off == 0u,
          "9: it reuses the kept block (the at-budget one), at its first offset",
          "mem %p (the kept %p), off %llu", (void *)again.mem, (void *)kept_mem,
          (unsigned long long)again.off);
    check(sx.allocs == allocs_before && m.blocks == 2u,
          "alloc: no new block was needed: the kept one was reused",
          "%u call(s) (was %u), blocks %llu", sx.allocs, (unsigned)allocs_before,
          (unsigned long long)m.blocks);
    verdict("9 retirement and the retention policy", chk, bad);
}

static uint8_t *g_guest;

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)g_guest;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    (void)backend;
}

int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t i, n = tex->width * tex->height;

    for (i = 0; i < n; i++)
        argb_out[i] = 0xB0C00000u | (i & 0xFFFFFu);
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    (void)tex;
    return 0;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* ── The run ─────────────────────────────────────────────────────────────────────────── */

int main(void)
{
    unsigned i;

    printf("vk_suballoc_gate: the Vulkan back end's memory sub-allocator, against the "
           "toolkit's own nv2a_vk_backend.c\n");
    printf("no GPU, no ICD, no skip: every Vulkan entry point the allocator's bodies reach is "
           "a controlled stub, and every injected VkResult is a stub's return value\n");

    g_guest = (uint8_t *)calloc(1, 1u << 20);
    g_snap = (uint8_t *)malloc(1u << 20);
    if (!g_guest || !g_snap) {
        fprintf(stderr, "FAIL: vk_suballoc_gate: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();
    gate_world();                          /* the fixture's own device, before the preconditions */
    seam_section();

    section("preconditions of the fixture");
    check(g.mp.memoryTypeCount == 4,
          "the fixture's device has the four memory types the scenarios select between", "");
    check(sizeof(VbAlloc) >= sizeof(VkDeviceMemory),
          "a VbAlloc is at least a memory handle wide", "%zu", sizeof(VbAlloc));
    check(VB_POOL_ALIGN == 256u && VB_POOL_MIN_BLOCK == (1u << 20)
          && VB_POOL_MAX_BLOCK == (64u << 20) && VB_POOL_RESERVE == (256u << 20)
          && VB_POOL_SHARE_MAX == (8u << 20) && VB_POOL_KEEP_EMPTY == (4u << 20),
          "the pool constants are the ones this fixture sizes its requests from",
          "align %u, min %u, max %u, reserve %u, share %u, keep-empty %u",
          (unsigned)VB_POOL_ALIGN, (unsigned)VB_POOL_MIN_BLOCK, (unsigned)VB_POOL_MAX_BLOCK,
          (unsigned)VB_POOL_RESERVE, (unsigned)VB_POOL_SHARE_MAX,
          (unsigned)VB_POOL_KEEP_EMPTY);
    check(GX_ATOM > 1u && (GX_ATOM & (GX_ATOM - 1u)) == 0u && GX_ATOM <= (unsigned)VB_POOL_ALIGN,
          "the fixture's atom size is a power of two no larger than the pooling alignment "
          "(so a widened window is observable inside one range)",
          "atom %u, VB_POOL_ALIGN %u", (unsigned)GX_ATOM, (unsigned)VB_POOL_ALIGN);

    prop1();
    prop2();
    prop3();
    prop4();
    prop5();
    prop6();
    prop7();
    prop8();
    prop9();

    for (i = 0; i < GX_MAX_MEM; i++)
        free(g_mem[i].arena);
    free(g_snap);
    free(g_guest);
    free(g.scratch);

    printf("\n");
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_suballoc_gate: %d of %d properties failed (%d of %d checks "
                        "failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_suballoc_gate: %d properties, %d checks -- resources share blocks at "
           "alignment-satisfying, non-overlapping offsets with a real reduction in "
           "vkAllocateMemory calls, memory types and kinds never mix, released ranges are "
           "reused and coalesced with honest counters, dedicated requests carry their pNext, "
           "a range a recording or a pending submission may still read is never handed out, "
           "every injected failure rolls back cleanly, and the flush windows are exact\n",
           g_props, g_checks);
    return 0;
}
