/* N4.2's readback publication, as a deciding failure-injection regression: a batch whose pack
 * descriptor set could not be allocated must submit NOTHING and publish NOTHING, and every
 * target it had staged must stay dirty and be retried.
 *
 * The defect this case exists for
 * -------------------------------
 * vb_targets_flush records a batch of dirty targets -- each 4-byte target a vkCmdCopyImageToBuffer
 * straight out of its image, each 16-bit target a cs_pack dispatch plus the buffer copy that reads
 * it -- and then submits the whole batch once, waits for it, invalidates the host-visible staging
 * buffer and memcpy's each staged rectangle into guest memory. The pack branch's
 * vkAllocateDescriptorSets can fail (VK_ERROR_OUT_OF_POOL_MEMORY on a transient pool), and the
 * pre-fix code then simply `break`t out of the record loop: vb_fail does not clear g.ok, so the
 * batch went on to run its barrier, vb_slot_submit(), vb_drain() and its publish loop anyway. Every
 * staged target of that batch was then marked clean from staging bytes that nothing had written --
 * for the 16-bit target because its dispatch was never recorded, for every target after it because
 * its copy never ran -- so the guest rectangle was filled with stale or uninitialised bytes and
 * never retried. No success-path run can reach that branch: the only way to decide it is to make
 * the allocation fail on purpose.
 *
 * The production bodies are the toolkit's own
 * ------------------------------------------
 * This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND
 * defined, so every function driven here -- vb_targets_flush, vb_readback_slice,
 * vb_readback_reserve, vb_transfer_cmd, vb_cmd, vb_reserve, vb_slot_submit, vb_drain,
 * vb_slot_wait -- is compiled from the production source, verbatim. Nothing is copied, so nothing
 * can drift: the build recompiles these bodies whenever that file changes, and
 * tests/../scripts/vk-readback-fault-gate.py probes the same bytes at build time and FAILS THE
 * BUILD (exit 2) if a definition, a struct field or a global the fixture binds to is gone. That
 * script also derives the seam markers in vk_readback_fault_gate_seam.h from those bytes; the
 * pre-fix source must still build -- it is what the case reports against -- so a missing
 * batch-local flag is a FAILED CHECK naming the marker, never a skipped case.
 *
 * Nothing of the toolkit is linked: the file's five undefined names are defined at the bottom of
 * this TU (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack), and every Vulkan entry point it uses is a static
 * function pointer (VK_NO_PROTOTYPES) that this TU points at controlled stubs before the first
 * call.
 *
 * The injected result is a STUBBED DRIVER OUTCOME
 * -----------------------------------------------
 * There is no GPU here and no ICD is needed: vkAllocateDescriptorSets returns the VkResult this
 * fixture selects, vkWaitForFences returns VK_SUCCESS, and the recording entry points only count
 * and record which command buffer they were handed. VK_ERROR_OUT_OF_POOL_MEMORY below is a stub's
 * return value, NOT a real pool exhaustion; the "[GPUVK] vkAllocateDescriptorSets failed" line on
 * stderr is the production code reporting the injected outcome. The fixture also stands in for the
 * GPU's own writes: the staging buffer vkMapMemory hands back is filled by the test with the bytes
 * a copy engine (4-byte target) or the pack kernel (16-bit target) would have produced, so what
 * guest memory must hold afterwards is exact, byte for byte.
 *
 * The properties
 * --------------
 *   0. the source and its seam        -- the file compiled here is the repaired one (its sha256 and
 *      the three seam markers are printed), and the fixture's constants are the source's.
 *   1. the success control            -- two dirty targets (16-bit and 4-byte, padded pitches and
 *      non-trivial rectangles, in both orders) record their copy, dispatch and buffer copy into one
 *      command buffer, submit once, wait once, invalidate once, and publish both rectangles: the
 *      exact guest bytes inside and outside the rectangles, both targets clean, and every
 *      publication counter advanced by exactly the amount that was published.
 *   2. allocation failure, 16-bit first -- nothing was staged when the failure hit: no submit, no
 *      wait, no invalidate, no publish; guest memory byte-identical (checksum and diff count), both
 *      targets still dirty, g.ok and g.failed unchanged, and all eight publication counters
 *      unchanged.
 *   3. allocation failure, 4-byte first -- the 4-byte copy was already recorded when the failure
 *      hit: the same assertions, so the whole batch is skipped. This is the case the fix exists
 *      for.
 *   4. the retry                      -- a second call that succeeds publishes both rectangles
 *      exactly and leaves the counters as if only the successful call had happened.
 *   5. the staging pair's lifetime     -- the failed call frees nothing and replaces nothing under
 *      the recording it left open: one pair is made, its handles are the same before and after the
 *      failure, and the successful retry copies into that same live pair.
 *
 * Expected verdicts
 * -----------------
 * The repaired source passes every property. The PRE-FIX source (the working tree's fix reverted:
 * the allocation failure does a plain `break`, with no batch_ok) is measured to FAIL properties 0,
 * 2, 3, 4 and 5 -- 49 of 138 checks -- and to PASS the two success controls 1a and 1b: the batch
 * that recorded nothing usable is still submitted and still published, so the stale staging bytes
 * land in both guest rectangles (496 of the guest's 1048576 bytes, 256 and 240 of them inside the
 * two rectangles), both targets are marked clean, and readbacks/batches/packed/packed_pixels/
 * copied/bytes all advance by exactly what nothing published; the retry then finds nothing dirty
 * and publishes nothing at all. The controls passing is the sanity anchor that separates "the
 * defect" from "the fixture".
 *
 * The counters' PLACEMENT is decisive too, and is mutated separately: with
 * readback_packs/packed_pixels/readback_copies moved out of the publish loop and into the record
 * loop (guest memory and the targets still correct) the source is measured to FAIL properties 0, 3
 * and 4 -- the structural marker in 0, and in 3 and 4 the 4-byte target's `copied` advancing by one
 * for a copy that was recorded and never published -- while property 2 still passes, because there
 * the failure precedes every per-target counter. Both rows are in the report; the gate cannot tell
 * "counted too early" from "published" by guest bytes alone, which is why the counters are bound
 * per counter and checked as exact deltas.
 *
 * There is no skip mode and no SKIP_RETURN_CODE in the build: the seam is inside this translation
 * unit, so the case always runs, and anything it cannot do (a missing seam, a failed allocation) is
 * a FAILURE, exit 1.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */

#include "vk_readback_fault_gate_seam.h"        /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture ─────────────────────────────────────────────────────────────────────── */

#define GATE_GUEST   (1u << 20)                 /* the stand-in for guest RAM */
#define GATE_STAGE   (1u << 20)                 /* what vkMapMemory hands back: the staging */
#define GATE_SEED    0x1D4C0000u
#define GATE_STALE   (GATE_SEED + 3u)           /* the pattern an unwritten staging buffer holds */
#define GATE_RB_CAP  (1u << 20)                 /* vb_readback_reserve's first capacity */

/* The 16-bit target: a padded pitch (44 bytes, not a multiple of the 32-byte packed row stride)
 * and an odd left edge, so a row's slack and the pitch's padding are visible in guest memory if
 * the wrong width is copied or the wrong offset used. 16x8 pixels, 32-byte rows, 256-byte slice. */
#define T16_VA      0x00001000u
#define T16_PITCH   44u
#define T16_BPP     2u
#define T16_DX0     3u
#define T16_DY0     2u
#define T16_DX1     19u
#define T16_DY1     10u
#define T16_RW      (T16_DX1 - T16_DX0)
#define T16_RH      (T16_DY1 - T16_DY0)
#define T16_STRIDE  (((T16_RW * 2u) + 3u) & ~3u)
#define T16_SLICE   ((VkDeviceSize)T16_STRIDE * T16_RH)

/* The 4-byte target: 12x5 pixels, tight 48-byte rows, a 240-byte slice. */
#define T4_VA       0x00040000u
#define T4_PITCH    320u
#define T4_BPP      4u
#define T4_DX0      1u
#define T4_DY0      1u
#define T4_DX1      13u
#define T4_DY1      6u
#define T4_RW       (T4_DX1 - T4_DX0)
#define T4_RH       (T4_DY1 - T4_DY0)
#define T4_STRIDE   (T4_RW * 4u)
#define T4_SLICE    ((VkDeviceSize)T4_STRIDE * T4_RH)

/* Both rectangles' byte counts equal their staging slices' -- a packed 16-bit row is exactly
 * rw*2 bytes and a 4-byte row exactly rw*4 -- which is what makes "the whole rectangle changed
 * and nothing else did" an exact statement below. */
#define GATE_PUBLISHED (T16_SLICE + T4_SLICE)

static uint8_t *g_guest;                        /* xbox_GetMemoryOffset()'s answer */
static uint8_t *g_snap;                         /* guest memory, copied before a call */
static uint8_t *g_expect;                       /* what guest memory must hold afterwards */
static uint8_t *g_stage;                        /* what vkMapMemory hands back */
static uint8_t *g_stream[VB_SLOTS];             /* one mapped slot stream each, as install leaves them */

static uintptr_t g_handle = 0x8000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

/* Every Vulkan call the publication's paths can reach, and what it saw. */
typedef struct {
    unsigned begins;                 /* vkBeginCommandBuffer */
    VkResult begin_result;
    VkCommandBuffer began;
    unsigned ends;                   /* vkEndCommandBuffer */
    VkCommandBuffer ended;
    unsigned resets;                 /* vkResetCommandBuffer */
    VkCommandBuffer reset_cb;
    unsigned submits;                /* vkQueueSubmit */
    VkCommandBuffer submitted[4];
    unsigned waits;                  /* vkWaitForFences */
    VkResult wait_result;
    unsigned fence_resets;           /* vkResetFences */
    unsigned pool_resets;            /* vkResetDescriptorPool */
    unsigned ds_allocs;              /* vkAllocateDescriptorSets */
    unsigned ds_failures;            /* allocations that returned the injected failure */
    unsigned ds_fail_at;             /* the 1-based call this fixture makes fail; 0: never */
    VkResult ds_result;              /* what that failure returns */
    VkDescriptorPool ds_pool;        /* the pool the last allocation asked */
    unsigned updates;                /* vkUpdateDescriptorSets */
    unsigned pipebinds;              /* vkCmdBindPipeline */
    VkPipeline bound_pipe;
    VkPipelineBindPoint bind_point;
    unsigned setbinds;               /* vkCmdBindDescriptorSets */
    unsigned pushes;                 /* vkCmdPushConstants */
    VbPackPush push;                 /* the pack block, copied as the production recorded it */
    unsigned dispatches;             /* vkCmdDispatch */
    VkCommandBuffer dispatch_cb;
    uint32_t dispatch_group[3];
    unsigned imgcopy;                /* vkCmdCopyImageToBuffer */
    VkCommandBuffer imgcopy_cb;
    VkImage imgcopy_img;
    VkBuffer imgcopy_buf;
    VkImageLayout imgcopy_layout;
    VkBufferImageCopy imgcopy_region;
    unsigned bufcopy;                /* vkCmdCopyBuffer */
    VkCommandBuffer bufcopy_cb;
    VkBuffer bufcopy_src, bufcopy_dst;
    VkBufferCopy bufcopy_region;
    unsigned bars;                   /* vkCmdPipelineBarrier with a memory barrier */
    unsigned hostbars;               /* ... whose destination stage is HOST */
    VkCommandBuffer bar_cb;
    unsigned invalidates;            /* vkInvalidateMappedMemoryRanges */
    VkMappedMemoryRange invalidate_range;
    unsigned buffers;                /* vkCreateBuffer */
    unsigned buffer_destroys;        /* vkDestroyBuffer */
    unsigned allocs;                 /* vkAllocateMemory */
    unsigned frees;                  /* vkFreeMemory */
    unsigned maps;                   /* vkMapMemory */
} GateCtl;

static GateCtl c;

static VkResult VKAPI_CALL gate_begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info)
{
    (void)info;
    c.begins++;
    c.began = cmd;
    return c.begin_result;
}

static VkResult VKAPI_CALL gate_end(VkCommandBuffer cmd)
{
    c.ends++;
    c.ended = cmd;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_cmd_reset(VkCommandBuffer cmd, VkCommandBufferResetFlags flags)
{
    (void)flags;
    c.resets++;
    c.reset_cb = cmd;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits,
                                       VkFence fence)
{
    (void)queue; (void)fence;
    c.submits++;
    if (c.submits <= 4u && count && submits[0].commandBufferCount)
        c.submitted[c.submits - 1u] = submits[0].pCommandBuffers[0];
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_wait(VkDevice device, uint32_t count, const VkFence *fences,
                                     VkBool32 all, uint64_t timeout)
{
    (void)device; (void)count; (void)fences; (void)all; (void)timeout;
    c.waits++;
    return c.wait_result;
}

static VkResult VKAPI_CALL gate_fence_reset(VkDevice device, uint32_t count, const VkFence *fences)
{
    (void)device; (void)count; (void)fences;
    c.fence_resets++;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_pool_reset(VkDevice device, VkDescriptorPool pool,
                                           VkDescriptorPoolResetFlags flags)
{
    (void)device; (void)pool; (void)flags;
    c.pool_resets++;
    return VK_SUCCESS;
}

/* The one outcome this case injects: the batch's pack descriptor set cannot be allocated. */
static VkResult VKAPI_CALL gate_ds_alloc(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                         VkDescriptorSet *set)
{
    (void)device;
    c.ds_allocs++;
    c.ds_pool = info ? info->descriptorPool : VK_NULL_HANDLE;
    if (c.ds_fail_at && c.ds_allocs >= c.ds_fail_at) {
        c.ds_failures++;
        *set = VK_NULL_HANDLE;
        return c.ds_result;
    }
    *set = (VkDescriptorSet)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_ds_update(VkDevice device, uint32_t writes,
                                      const VkWriteDescriptorSet *w, uint32_t copies,
                                      const VkCopyDescriptorSet *cp)
{
    (void)device; (void)w; (void)copies; (void)cp;
    c.updates++;
}

static void VKAPI_CALL gate_bind_pipeline(VkCommandBuffer cmd, VkPipelineBindPoint point,
                                          VkPipeline pipe)
{
    (void)cmd;
    c.pipebinds++;
    c.bind_point = point;
    c.bound_pipe = pipe;
}

static void VKAPI_CALL gate_bind_sets(VkCommandBuffer cmd, VkPipelineBindPoint point,
                                      VkPipelineLayout layout, uint32_t first, uint32_t count,
                                      const VkDescriptorSet *sets, uint32_t dyn_count,
                                      const uint32_t *dyn)
{
    (void)cmd; (void)point; (void)layout; (void)first; (void)count; (void)sets; (void)dyn_count;
    (void)dyn;
    c.setbinds++;
}

static void VKAPI_CALL gate_push_constants(VkCommandBuffer cmd, VkPipelineLayout layout,
                                           VkShaderStageFlags stage, uint32_t offset,
                                           uint32_t size, const void *values)
{
    (void)cmd; (void)layout; (void)stage; (void)offset;
    c.pushes++;
    memset(&c.push, 0, sizeof c.push);
    if (values && size)
        memcpy(&c.push, values, size < sizeof c.push ? size : sizeof c.push);
}

static void VKAPI_CALL gate_dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z)
{
    c.dispatches++;
    c.dispatch_cb = cmd;
    c.dispatch_group[0] = x;
    c.dispatch_group[1] = y;
    c.dispatch_group[2] = z;
}

static void VKAPI_CALL gate_copy_to_buffer(VkCommandBuffer cmd, VkImage src,
                                           VkImageLayout layout, VkBuffer dst, uint32_t count,
                                           const VkBufferImageCopy *regions)
{
    c.imgcopy++;
    c.imgcopy_cb = cmd;
    c.imgcopy_img = src;
    c.imgcopy_layout = layout;
    c.imgcopy_buf = dst;
    if (count)
        c.imgcopy_region = regions[0];
}

static void VKAPI_CALL gate_copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
                                        uint32_t count, const VkBufferCopy *regions)
{
    c.bufcopy++;
    c.bufcopy_cb = cmd;
    c.bufcopy_src = src;
    c.bufcopy_dst = dst;
    if (count)
        c.bufcopy_region = regions[0];
}

static void VKAPI_CALL gate_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
                                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                                    uint32_t mbc, const VkMemoryBarrier *mb, uint32_t bbc,
                                    const VkBufferMemoryBarrier *bb, uint32_t ibc,
                                    const VkImageMemoryBarrier *ib)
{
    (void)flags; (void)bb; (void)ib;
    if (!(mbc && mb))
        return;                                 /* this fixture commands no image barriers */
    c.bars++;
    c.bar_cb = cmd;
    if (dst & VK_PIPELINE_STAGE_HOST_BIT)
        c.hostbars++;
    (void)src;
    (void)ibc;
}

static VkResult VKAPI_CALL gate_invalidate(VkDevice device, uint32_t count,
                                           const VkMappedMemoryRange *ranges)
{
    (void)device;
    c.invalidates++;
    if (count)
        c.invalidate_range = ranges[0];
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_end_pass(VkCommandBuffer cmd)
{
    (void)cmd;
}

static VkResult VKAPI_CALL gate_create_buffer(VkDevice device, const VkBufferCreateInfo *info,
                                              const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)info; (void)alloc;
    c.buffers++;
    *buf = (VkBuffer)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_buffer_requirements(VkDevice device, VkBuffer buf,
                                                VkMemoryRequirements *req)
{
    (void)device; (void)buf;
    req->size = 64u;
    req->alignment = 16u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_bind_buffer(VkDevice device, VkBuffer buf, VkDeviceMemory mem,
                                            VkDeviceSize offset)
{
    (void)device; (void)buf; (void)mem; (void)offset;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_destroy_buffer(VkDevice device, VkBuffer buf,
                                           const VkAllocationCallbacks *alloc)
{
    (void)device; (void)buf; (void)alloc;
    c.buffer_destroys++;
}

static VkResult VKAPI_CALL gate_allocate(VkDevice device, const VkMemoryAllocateInfo *info,
                                         const VkAllocationCallbacks *alloc, VkDeviceMemory *mem)
{
    (void)device; (void)info; (void)alloc;
    c.allocs++;
    *mem = (VkDeviceMemory)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_free(VkDevice device, VkDeviceMemory mem,
                                 const VkAllocationCallbacks *alloc)
{
    (void)device; (void)mem; (void)alloc;
    c.frees++;
}

/* The staging buffer the host publishes from: the same stand-in arena every time, so the test
 * can fill it with the bytes the GPU would have written there. */
static VkResult VKAPI_CALL gate_map(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    (void)device; (void)mem; (void)offset; (void)size; (void)flags;
    c.maps++;
    *ppData = g_stage;
    return VK_SUCCESS;
}

/* Point the back end's own entry points at the stubs above. The pointers are static in the
 * translation unit, which is why the file is compiled here. */
static void gate_wire(void)
{
    vkBeginCommandBuffer = gate_begin;
    vkEndCommandBuffer = gate_end;
    vkResetCommandBuffer = gate_cmd_reset;
    vkQueueSubmit = gate_submit;
    vkWaitForFences = gate_wait;
    vkResetFences = gate_fence_reset;
    vkResetDescriptorPool = gate_pool_reset;
    vkAllocateDescriptorSets = gate_ds_alloc;
    vkUpdateDescriptorSets = gate_ds_update;
    vkCmdBindPipeline = gate_bind_pipeline;
    vkCmdBindDescriptorSets = gate_bind_sets;
    vkCmdPushConstants = gate_push_constants;
    vkCmdDispatch = gate_dispatch;
    vkCmdCopyImageToBuffer = gate_copy_to_buffer;
    vkCmdCopyBuffer = gate_copy_buffer;
    vkCmdPipelineBarrier = gate_barrier;
    vkInvalidateMappedMemoryRanges = gate_invalidate;
    vkCmdEndRenderPass = gate_end_pass;
    vkCreateBuffer = gate_create_buffer;
    vkGetBufferMemoryRequirements = gate_buffer_requirements;
    vkBindBufferMemory = gate_bind_buffer;
    vkDestroyBuffer = gate_destroy_buffer;
    vkAllocateMemory = gate_allocate;
    vkFreeMemory = gate_free;
    vkMapMemory = gate_map;
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
    printf("  --> %-52s %s (%d checks, %d failed)\n", prop, bad ? "FAIL" : "PASS",
           g_checks - checks_before, bad);
    fflush(stdout);
}

static void check(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    g_checks++;
    if (!cond)
        g_check_failures++;
    printf("      %-62s %s", what, cond ? "ok" : "FAIL");
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

static void marker(const char *what, int present)
{
    printf("      %-62s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

/* ── Fixture state ───────────────────────────────────────────────────────────────────── */

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

static void stats(Nv2aVkReadbackStats *out)
{
    memset(out, 0, sizeof *out);
    nv2a_vk_backend_readback_stats(out);
}

/* A staging-buffer pattern whose bytes never equal the guest marker's: an unwritten staging
 * buffer holds one (0x8x), what the fixture's "GPU" wrote holds another (0x5x/0x6x), and the
 * guest's rectangles hold a third (0x1x/0x2x). That is what makes "the whole rectangle changed
 * and nothing outside it did" an exact count instead of a statistical one. */
static void fill_stage_slice(uint8_t *p, size_t n, uint32_t base)
{
    size_t i;

    for (i = 0; i < n; i++)
        p[i] = (uint8_t)(base + (i & 0x0Fu));
}

/* The guest's two rectangle windows, marked so a publish of anything else is visible. */
static void guest_markers(void)
{
    uint32_t r, x;

    for (r = 0; r < (uint32_t)T16_RH; r++)
        for (x = 0; x < (uint32_t)T16_RW * T16_BPP; x++)
            g_guest[T16_VA + (size_t)(T16_DY0 + r) * T16_PITCH + (size_t)T16_DX0 * T16_BPP + x]
                = (uint8_t)(0x10u + (x & 0x0Fu));
    for (r = 0; r < (uint32_t)T4_RH; r++)
        for (x = 0; x < (uint32_t)T4_RW * T4_BPP; x++)
            g_guest[T4_VA + (size_t)(T4_DY0 + r) * T4_PITCH + (size_t)T4_DX0 * T4_BPP + x]
                = (uint8_t)(0x20u + (x & 0x0Fu));
}

/* The whole back end's state as install/first use leaves it, with the ring idle. */
static void gate_world(void)
{
    unsigned i;

    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    c.begin_result = VK_SUCCESS;
    c.wait_result = VK_SUCCESS;
    c.ds_result = VK_ERROR_OUT_OF_POOL_MEMORY;
    c.ds_fail_at = 0;

    g.dev = (VkDevice)hnew();
    g.queue = (VkQueue)hnew();
    /* One memory type that satisfies every request, so the allocator's probes always land
     * (host-visible and coherent, so a pooled host range is coherent and never dedicated). */
    g.mp.memoryTypeCount = 1;
    g.mp.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    g.mp.memoryTypes[0].heapIndex = 0;
    g.ok = 1;
    g.dsl_pack = (VkDescriptorSetLayout)hnew();
    g.layout_pack = (VkPipelineLayout)hnew();
    g.pipe_pack = (VkPipeline)hnew();
    for (i = 0; i < VB_SLOTS; i++) {
        g.slots[i].cmd = (VkCommandBuffer)hnew();
        g.slots[i].fence = (VkFence)hnew();
        g.slots[i].dpool = (VkDescriptorPool)hnew();
        g.slots[i].stream = (VkBuffer)hnew();
        g.slots[i].stream_ptr = g_stream[i];
        fill_bytes(g_stream[i], VB_SLOT_STREAM, GATE_SEED + i);
    }
    fill_bytes(g_guest, GATE_GUEST, GATE_SEED + 7u);
    guest_markers();                                    /* the two rectangle windows */
    fill_stage_slice(g_stage, GATE_STAGE, GATE_STALE);  /* nothing has written the staging yet */
}

static void wire_target(VbTarget *t, uint32_t va, uint32_t pitch, uint32_t bpp,
                        uint32_t dx0, uint32_t dy0, uint32_t dx1, uint32_t dy1)
{
    memset(t, 0, sizeof *t);
    t->used = 1;
    t->va = va;
    t->pitch = pitch;
    t->bpp = bpp;
    t->w = pitch / bpp;
    t->h = dy1 + 4u;
    t->dirty = 1;
    t->dx0 = dx0;
    t->dy0 = dy0;
    t->dx1 = dx1;
    t->dy1 = dy1;
    t->im.img = (VkImage)hnew();
    t->im.view = (VkImageView)hnew();
}

/* One batch: the two dirty targets in the order the property asks for, and whether the batch's
 * pack descriptor-set allocation fails. The slice offsets follow from that order and are the
 * production's own: the first staged target's slice is at 0, the second's after it. */
typedef struct {
    VbTarget *ts[2];
    VbTarget *t16;
    VbTarget *t4;
    VkDeviceSize off16, off4;
    unsigned n;
} Scenario;

static void scenario(Scenario *s, int sixteen_first, unsigned fail_first_alloc)
{
    gate_world();
    c.ds_fail_at = fail_first_alloc;
    wire_target(&g.targets[0], T16_VA, T16_PITCH, T16_BPP, T16_DX0, T16_DY0, T16_DX1, T16_DY1);
    wire_target(&g.targets[1], T4_VA, T4_PITCH, T4_BPP, T4_DX0, T4_DY0, T4_DX1, T4_DY1);
    s->t16 = &g.targets[0];
    s->t4 = &g.targets[1];
    if (sixteen_first) {
        s->ts[0] = s->t16;
        s->ts[1] = s->t4;
        s->off16 = 0;
        s->off4 = T16_SLICE;
    } else {
        s->ts[0] = s->t4;
        s->ts[1] = s->t16;
        s->off4 = 0;
        s->off16 = T4_SLICE;
    }
    s->n = 2;
}

/* The GPU's half of the batch, as the fixture supplies it: what the copy engine writes for a
 * 4-byte target and what cs_pack's dispatch writes for a 16-bit one. The bases are arguments so
 * a property that publishes twice can stage different bytes the second time and still expect
 * every rectangle byte to change. */
static void stage_gpu(const Scenario *s, uint32_t base16, uint32_t base4)
{
    fill_stage_slice(g_stage + s->off16, (size_t)T16_SLICE, base16);
    fill_stage_slice(g_stage + s->off4, (size_t)T4_SLICE, base4);
}

/* What guest memory must hold after a successful publication: the pre-call bytes, with each
 * rectangle replaced by its staging slice's rows. */
static void expect_guest(const Scenario *s)
{
    unsigned k, row;

    memcpy(g_expect, g_snap, GATE_GUEST);
    for (k = 0; k < s->n; k++) {
        const VbTarget *t = s->ts[k];
        VkDeviceSize off = (t == s->t16) ? s->off16 : s->off4;
        uint32_t stride = (uint32_t)(t->bpp == 2 ? T16_STRIDE : T4_STRIDE);
        uint32_t rw = t->dx1 - t->dx0;

        for (row = 0; row < t->dy1 - t->dy0; row++)
            memcpy(g_expect + t->va + (size_t)(t->dy0 + row) * t->pitch
                                    + (size_t)t->dx0 * t->bpp,
                   g_stage + off + (VkDeviceSize)row * stride, (size_t)rw * t->bpp);
    }
}

static size_t diff_inside(const uint8_t *a, const uint8_t *b, const VbTarget *t)
{
    size_t bad = 0;
    uint32_t row, rw = t->dx1 - t->dx0;

    for (row = 0; row < t->dy1 - t->dy0; row++)
        bad += count_diff(a + t->va + (size_t)(t->dy0 + row) * t->pitch
                                      + (size_t)t->dx0 * t->bpp,
                          b + t->va + (size_t)(t->dy0 + row) * t->pitch
                                      + (size_t)t->dx0 * t->bpp,
                          (size_t)rw * t->bpp);
    return bad;
}

/* ── The shared property helpers ────────────────────────────────────────────────────── */

/* What a call must have published, as deltas of the counters nv2a_vk_backend_readback_stats
 * reports. */
typedef struct {
    uint64_t readbacks, batches, packed, packed_pixels, copied, bytes;
} RbWant;

static void want_published(RbWant *w)
{
    w->readbacks = 2;                           /* one per target */
    w->batches = 1;                             /* one submission, one wait */
    w->packed = 1;                              /* the 16-bit target, packed by cs_pack */
    w->packed_pixels = (uint64_t)T16_RW * T16_RH;
    w->copied = 1;                              /* the 4-byte target, straight out of its image */
    w->bytes = (uint64_t)GATE_PUBLISHED;
}

static void want_nothing(RbWant *w)
{
    memset(w, 0, sizeof *w);
}

/* Every publication counter, checked one by one so a failure names the one that moved. */
static void check_counters(const char *tag, const Nv2aVkReadbackStats *b,
                           const Nv2aVkReadbackStats *a, const RbWant *w, int exact_times)
{
    char what[110];

    snprintf(what, sizeof what, "%s readbacks published (targets written to guest memory)", tag);
    check(a->readbacks == b->readbacks + w->readbacks, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->readbacks, (unsigned long long)a->readbacks,
          (unsigned long long)w->readbacks);
    snprintf(what, sizeof what, "%s batches published (submissions that reached a publish)", tag);
    check(a->batches == b->batches + w->batches, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->batches, (unsigned long long)a->batches,
          (unsigned long long)w->batches);
    snprintf(what, sizeof what, "%s packed 16-bit targets published", tag);
    check(a->packed == b->packed + w->packed, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->packed, (unsigned long long)a->packed,
          (unsigned long long)w->packed);
    snprintf(what, sizeof what, "%s packed pixels published", tag);
    check(a->packed_pixels == b->packed_pixels + w->packed_pixels, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->packed_pixels, (unsigned long long)a->packed_pixels,
          (unsigned long long)w->packed_pixels);
    snprintf(what, sizeof what, "%s 4-byte targets copied out of their image", tag);
    check(a->copied == b->copied + w->copied, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->copied, (unsigned long long)a->copied,
          (unsigned long long)w->copied);
    snprintf(what, sizeof what, "%s bytes written into guest memory", tag);
    check(a->bytes == b->bytes + w->bytes, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->bytes, (unsigned long long)a->bytes,
          (unsigned long long)w->bytes);
    if (exact_times) {
        snprintf(what, sizeof what, "%s the two time counters are untouched", tag);
        check(a->wait_ns == b->wait_ns && a->publish_ns == b->publish_ns, what,
              "wait_ns %llu -> %llu, publish_ns %llu -> %llu",
              (unsigned long long)b->wait_ns, (unsigned long long)a->wait_ns,
              (unsigned long long)b->publish_ns, (unsigned long long)a->publish_ns);
    } else {
        /* These two are clocks, so only their direction is a property; the counters above are
         * the exact ones. */
        snprintf(what, sizeof what, "%s the two time counters advanced (clocks: direction only)", tag);
        check(a->wait_ns >= b->wait_ns && a->publish_ns >= b->publish_ns, what,
              "wait_ns %llu -> %llu, publish_ns %llu -> %llu",
              (unsigned long long)b->wait_ns, (unsigned long long)a->wait_ns,
              (unsigned long long)b->publish_ns, (unsigned long long)a->publish_ns);
    }
}

/* Where the first few differing bytes are, for the FAIL line's evidence. */
static void note_diffs(const char *tag, const uint8_t *a, const uint8_t *b)
{
    unsigned shown = 0;
    size_t i;

    printf("      note: %s first differences (a->b): ", tag);
    for (i = 0; i < GATE_GUEST && shown < 8u; i++)
        if (a[i] != b[i]) {
            printf("%s0x%05zX %02X->%02X", shown ? ", " : "", i, a[i], b[i]);
            shown++;
        }
    if (!shown)
        printf("none");
    putchar('\n');
    fflush(stdout);
}

/* Guest memory: exactly the published rectangles, or exactly what it held before. */
static void check_guest(const char *tag, const Scenario *s, int published)
{
    size_t total = count_diff(g_snap, g_guest, GATE_GUEST);
    size_t in16 = diff_inside(g_snap, g_guest, s->t16);
    size_t in4 = diff_inside(g_snap, g_guest, s->t4);
    size_t outside = total - in16 - in4;        /* the two rectangles do not overlap */
    char what[110];

    if (published) {
        size_t wrong;

        expect_guest(s);
        wrong = count_diff(g_expect, g_guest, GATE_GUEST);
        snprintf(what, sizeof what, "%s guest memory holds exactly the published rectangles", tag);
        check(wrong == 0, what, "%zu of %u bytes differ from the expected image "
                                "(guest checksum 0x%016llX, expected 0x%016llX)", wrong,
              (unsigned)GATE_GUEST, (unsigned long long)hash_bytes(g_guest, GATE_GUEST),
              (unsigned long long)hash_bytes(g_expect, GATE_GUEST));
        if (wrong) {
            note_diffs("guest vs expected", g_expect, g_guest);
            note_diffs("guest vs pre-call", g_snap, g_guest);
        }
        snprintf(what, sizeof what, "%s both rectangles were overwritten from their slices", tag);
        check(in16 == (size_t)T16_SLICE && in4 == (size_t)T4_SLICE, what,
              "16-bit %zu of %llu bytes changed, 4-byte %zu of %llu",
              in16, (unsigned long long)T16_SLICE, in4, (unsigned long long)T4_SLICE);
        snprintf(what, sizeof what, "%s no byte outside the rectangles was written", tag);
        check(outside == 0, what, "%zu of %u bytes differ outside (%zu of %u in total)",
              outside, (unsigned)GATE_GUEST, total, (unsigned)GATE_GUEST);
    } else {
        snprintf(what, sizeof what, "%s guest memory is byte-identical: nothing was published", tag);
        check(total == 0, what, "checksum 0x%016llX -> 0x%016llX, %zu of %u bytes differ",
              (unsigned long long)hash_bytes(g_snap, GATE_GUEST),
              (unsigned long long)hash_bytes(g_guest, GATE_GUEST), total, (unsigned)GATE_GUEST);
        snprintf(what, sizeof what, "%s neither rectangle was written", tag);
        check(in16 == 0 && in4 == 0, what, "16-bit %zu bytes, 4-byte %zu bytes changed "
                                           "(they are %llu and %llu bytes)",
              in16, in4, (unsigned long long)T16_SLICE, (unsigned long long)T4_SLICE);
        snprintf(what, sizeof what, "%s no byte outside the rectangles changed either", tag);
        check(outside == 0, what, "%zu bytes differ outside", outside);
    }
}

static void check_dirty(const char *tag, const Scenario *s, int want)
{
    char what[110];

    snprintf(what, sizeof what, "%s both targets are %s (the flush's retry decision)", tag,
             want ? "still dirty" : "clean");
    check(s->t16->dirty == want && s->t4->dirty == want, what,
          "16-bit dirty %d, 4-byte dirty %d", s->t16->dirty, s->t4->dirty);
}

/* The state a batch that recorded nothing usable must leave behind. */
static void check_nothing_submitted(const char *tag)
{
    char what[110];

    snprintf(what, sizeof what, "%s nothing was submitted", tag);
    check(c.submits == 0 && g.st.submits == 0, what, "vkQueueSubmit %u, g.st.submits %llu",
          c.submits, (unsigned long long)g.st.submits);
    snprintf(what, sizeof what, "%s no command buffer was ended, no fence waited or reset", tag);
    check(c.ends == 0 && c.waits == 0 && c.fence_resets == 0 && c.pool_resets == 0, what,
          "ends %u, waits %u, fence resets %u, descriptor-pool resets %u", c.ends, c.waits,
          c.fence_resets, c.pool_resets);
    snprintf(what, sizeof what, "%s the host never read the staging pair (no invalidate)", tag);
    check(c.invalidates == 0, what, "vkInvalidateMappedMemoryRanges %u", c.invalidates);
    snprintf(what, sizeof what, "%s no slot was left in flight", tag);
    check(g.slots[0].pending == 0 && g.npending == 0, what, "slot 0 pending %d, npending %u",
          g.slots[0].pending, g.npending);
}

/* The injected outcome really was the one the case is about, and it really is not a device loss. */
static void check_injection(const char *tag)
{
    char what[110];

    snprintf(what, sizeof what, "%s the pack set allocation was attempted and really failed", tag);
    check(c.ds_allocs >= 1u && c.ds_failures == 1u, what,
          "vkAllocateDescriptorSets %u, returning VK_ERROR_OUT_OF_POOL_MEMORY %u, pool %p",
          c.ds_allocs, c.ds_failures, (void *)c.ds_pool);
    snprintf(what, sizeof what, "%s the failure was reported (vb_fail), not latched", tag);
    check(g.fails == 1u && g.ok == 1 && g.failed == 0, what,
          "g.fails %u, g.ok %d, g.failed %d", g.fails, g.ok, g.failed);
}

/* ── 0. the source under test, and the seam this case binds to ─────────────────────── */

static void prop0(void)
{
    Nv2aVkReadbackStats st;
    int chk = g_checks, bad = g_check_failures;

    section("0. the source under test, probed from the same bytes at build time");
    printf("      production bodies compiled from: %s\n", VB_RB_GATE_SOURCE);
    printf("      sha256 %s\n", VB_RB_GATE_SOURCE_SHA256);
    printf("      %u bytes\n", (unsigned)VB_RB_GATE_SOURCE_BYTES);
    note("every function driven below is that file's own, compiled into this translation unit by "
         "#include; scripts/vk-readback-fault-gate.py rejects the BUILD when a definition, a struct "
         "field or a global this fixture binds to is gone");
    marker("the batch-local success flag (int batch_ok = 1)", VB_RB_GATE_HAS_BATCH_FLAG);
    marker("the whole-batch skip before the submit (if (!batch_ok))", VB_RB_GATE_HAS_BATCH_SKIP);
    marker("the publication counters incremented after that submit",
           VB_RB_GATE_COUNTERS_AFTER_SUBMIT);

    check(VB_RB_GATE_HAS_BATCH_FLAG && VB_RB_GATE_HAS_BATCH_SKIP
          && VB_RB_GATE_COUNTERS_AFTER_SUBMIT,
          "the source is the repaired one this case decides",
          "batch flag %d, batch skip %d, counters after the submit %d",
          VB_RB_GATE_HAS_BATCH_FLAG, VB_RB_GATE_HAS_BATCH_SKIP,
          VB_RB_GATE_COUNTERS_AFTER_SUBMIT);
    check(VB_READBACK_MAX == (96u << 20) && VB_MAX_TARGETS >= 2u && VB_SLOTS == 3u,
          "the fixture's two slices and one batch fit the source's own bounds",
          "VB_READBACK_MAX %llu, VB_MAX_TARGETS %u, VB_SLOTS %u, batch %llu bytes",
          (unsigned long long)VB_READBACK_MAX, (unsigned)VB_MAX_TARGETS, (unsigned)VB_SLOTS,
          (unsigned long long)GATE_PUBLISHED);
    check((uint64_t)GATE_PUBLISHED <= VB_READBACK_MAX
          && (uint64_t)T16_SLICE <= VB_READBACK_MAX && (uint64_t)T4_SLICE <= VB_READBACK_MAX,
          "the fixture's slices are inside the staging cap, so one batch carries both",
          "16-bit %llu, 4-byte %llu, cap %llu", (unsigned long long)T16_SLICE,
          (unsigned long long)T4_SLICE, (unsigned long long)VB_READBACK_MAX);

    memset(&st, 0, sizeof st);
    nv2a_vk_backend_readback_stats(&st);
    check(st.readbacks == 0 && st.batches == 0 && st.packed == 0 && st.packed_pixels == 0
          && st.copied == 0 && st.bytes == 0,
          "the published-work counters start at zero in a fresh world",
          "readbacks %llu, batches %llu, packed %llu, pixels %llu, copied %llu, bytes %llu",
          (unsigned long long)st.readbacks, (unsigned long long)st.batches,
          (unsigned long long)st.packed, (unsigned long long)st.packed_pixels,
          (unsigned long long)st.copied, (unsigned long long)st.bytes);
    verdict("0 the source and its seam", chk, bad);
}

/* ── 1. the success control ─────────────────────────────────────────────────────────── */

static void success_case(int sixteen_first, const char *tag)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    RbWant w;
    char what[110];
    uint32_t groups_x = (T16_RW + 15u) / 16u, groups_y = (T16_RH + 7u) / 8u;
    int chk = g_checks, bad = g_check_failures;

    section("1. the control: a succeeding batch publishes both rectangles exactly");
    note("%s: the injected outcome is VK_SUCCESS here; the staging buffer is filled with what the "
         "copy engine and cs_pack would have written", tag);

    scenario(&s, sixteen_first, 0);
    stage_gpu(&s, 0x50u, 0x60u);
    memcpy(g_snap, g_guest, GATE_GUEST);
    stats(&b);

    vb_targets_flush(s.ts, s.n);

    stats(&a);

    /* the fixture reached both recording paths */
    check(c.ds_allocs == 1u && c.ds_failures == 0u,
          "the pack path was reached and its descriptor set was allocated",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);
    check(c.begins == 1u && c.resets == 1u && c.began == g.slots[0].cmd,
          "the whole batch was recorded into one command buffer, begun once",
          "begins %u, resets %u, command buffer %p (slot 0 %p)", c.begins, c.resets,
          (void *)c.began, (void *)g.slots[0].cmd);

    /* the 4-byte target: its own rectangle, at its own slice offset, straight from its image */
    check(c.imgcopy == 1u && c.imgcopy_cb == g.slots[0].cmd,
          "the 4-byte target's copy was recorded in that buffer exactly once",
          "vkCmdCopyImageToBuffer %u, command buffer %p", c.imgcopy, (void *)c.imgcopy_cb);
    check(c.imgcopy_img == s.t4->im.img && c.imgcopy_buf == g.rb_buf
          && c.imgcopy_layout == VK_IMAGE_LAYOUT_GENERAL,
          "it reads the 4-byte target's image into the host-visible staging buffer",
          "image %p (target %p), buffer %p, layout %d", (void *)c.imgcopy_img,
          (void *)s.t4->im.img, (void *)c.imgcopy_buf, (int)c.imgcopy_layout);
    check(c.imgcopy_region.bufferOffset == s.off4 && c.imgcopy_region.bufferRowLength == T4_RW
          && c.imgcopy_region.imageOffset.x == (int32_t)T4_DX0
          && c.imgcopy_region.imageOffset.y == (int32_t)T4_DY0
          && c.imgcopy_region.imageExtent.width == T4_RW
          && c.imgcopy_region.imageExtent.height == T4_RH
          && c.imgcopy_region.imageExtent.depth == 1u,
          "its region is the dirty rectangle, at the slice the batch reserved for it",
          "offset %llu (wanted %llu), row %u, image (%d,%d) %ux%u depth %u",
          (unsigned long long)c.imgcopy_region.bufferOffset, (unsigned long long)s.off4,
          c.imgcopy_region.bufferRowLength, c.imgcopy_region.imageOffset.x,
          c.imgcopy_region.imageOffset.y, c.imgcopy_region.imageExtent.width,
          c.imgcopy_region.imageExtent.height, c.imgcopy_region.imageExtent.depth);

    /* the 16-bit target: the pack dispatch, its push block, and the buffer copy that reads it */
    check(c.dispatches == 1u && c.dispatch_cb == g.slots[0].cmd
          && c.dispatch_group[0] == groups_x && c.dispatch_group[1] == groups_y
          && c.dispatch_group[2] == 1u,
          "the 16-bit target's pack dispatch was recorded once, over its own rectangle",
          "dispatches %u, groups %ux%ux%u (wanted %ux%ux1)", c.dispatches, c.dispatch_group[0],
          c.dispatch_group[1], c.dispatch_group[2], groups_x, groups_y);
    check(c.pushes == 1u && c.push.rect[0] == T16_DX0 && c.push.rect[1] == T16_DY0
          && c.push.rect[2] == T16_RW && c.push.rect[3] == T16_RH
          && c.push.dst[0] == (uint32_t)s.off16 && c.push.dst[1] == T16_STRIDE
          && c.push.dst[2] == 0u && c.push.dst[3] == 0u,
          "its push block is the rectangle and the slice's offset and packed row stride",
          "rect {%u,%u,%u,%u} dst {%u,%u,%u,%u} (wanted {%u,%u,%u,%u} {%llu,%u,0,0})",
          c.push.rect[0], c.push.rect[1], c.push.rect[2], c.push.rect[3], c.push.dst[0],
          c.push.dst[1], c.push.dst[2], c.push.dst[3], T16_DX0, T16_DY0, T16_RW, T16_RH,
          (unsigned long long)s.off16, (unsigned)T16_STRIDE);
    check(c.pipebinds == 1u && c.bound_pipe == g.pipe_pack
          && c.bind_point == VK_PIPELINE_BIND_POINT_COMPUTE,
          "the pack compute pipeline was bound for it", "binds %u, pipeline %p (pack %p), point %d",
          c.pipebinds, (void *)c.bound_pipe, (void *)g.pipe_pack, (int)c.bind_point);
    check(c.bufcopy == 1u && c.bufcopy_cb == g.slots[0].cmd && c.bufcopy_src == g.pack_buf
          && c.bufcopy_dst == g.rb_buf && c.bufcopy_region.srcOffset == s.off16
          && c.bufcopy_region.dstOffset == s.off16
          && c.bufcopy_region.size == (VkDeviceSize)T16_STRIDE * T16_RH,
          "the packed slice was copied from the pack buffer to the staging buffer in place",
          "copies %u, src %p dst %p, offsets %llu/%llu (wanted %llu), size %llu (wanted %llu)",
          c.bufcopy, (void *)c.bufcopy_src, (void *)c.bufcopy_dst,
          (unsigned long long)c.bufcopy_region.srcOffset,
          (unsigned long long)c.bufcopy_region.dstOffset, (unsigned long long)s.off16,
          (unsigned long long)c.bufcopy_region.size,
          (unsigned long long)((VkDeviceSize)T16_STRIDE * T16_RH));

    /* the ordering and the submit */
    check(c.bars == 3u, "every ordering barrier is in that one recording",
          "memory barriers %u (the pack's, the batch's host barrier, the submit's)", c.bars);
    check(c.hostbars == 1u, "the batch's host-visibility barrier was recorded before the submit",
          "barriers into VK_PIPELINE_STAGE_HOST_BIT %u", c.hostbars);
    check(c.submits == 1u && c.submitted[0] == g.slots[0].cmd && c.ends == 1u
          && c.ended == g.slots[0].cmd && g.st.submits == 1u,
          "the batch was ended and submitted exactly once",
          "submits %u (g.st.submits %llu), ends %u, ended %p (slot 0 %p)", c.submits,
          (unsigned long long)g.st.submits, c.ends, (void *)c.ended, (void *)g.slots[0].cmd);
    check(c.waits == 1u && c.fence_resets == 1u && c.pool_resets == 1u
          && g.slots[0].pending == 0 && g.npending == 0 && g.st.drain_waits == 1u,
          "it was drained exactly once, and the slot is free again",
          "fence waits %u, fence resets %u, pool resets %u, slot 0 pending %d, npending %u, "
          "drain_waits %llu", c.waits, c.fence_resets, c.pool_resets, g.slots[0].pending,
          g.npending, (unsigned long long)g.st.drain_waits);
    /* N4.4: the publication invalidates its own range (vb_mem_invalidate(&g.rb_mem, 0,
     * g.rb_mem.size)), not the whole VkDeviceMemory -- the pair no longer owns one. The same
     * decision (the batch invalidated the staging bytes once, over exactly the pair's range,
     * before the memcpys) now names the range's offset and size. */
    check(c.invalidates == 1u && c.invalidate_range.memory == g.rb_mem.mem
          && c.invalidate_range.offset == g.rb_mem.off
          && c.invalidate_range.size == g.rb_mem.size,
          "the staging range was invalidated exactly once, whole, before the memcpys",
          "invalidates %u, memory %p (pair %p), offset %llu (pair %llu), size %llu (pair %llu)",
          c.invalidates, (void *)c.invalidate_range.memory, (void *)g.rb_mem.mem,
          (unsigned long long)c.invalidate_range.offset, (unsigned long long)g.rb_mem.off,
          (unsigned long long)c.invalidate_range.size, (unsigned long long)g.rb_mem.size);

    /* the staging pair */
    /* N4.4: g.rb_mem / g.pack_mem are VbAllocs, and a pooled host-visible range's pointer is
     * its block's single mapping plus the range's offset -- no longer necessarily the arena
     * itself, and no longer necessarily offset 0. */
    check(g.rb_buf != VK_NULL_HANDLE && g.pack_buf != VK_NULL_HANDLE
          && g.rb_mem.mem != VK_NULL_HANDLE && g.pack_mem.mem != VK_NULL_HANDLE
          && g.rb_ptr == g_stage + g.rb_mem.off
          && g.rb_mem.off % VB_POOL_ALIGN == 0
          && g.rb_cap == (VkDeviceSize)GATE_RB_CAP,
          "one staging pair was made, and its pointer is the block's mapping plus its range's "
          "aligned offset",
          "rb %p/%p, pack %p/%p, rb_ptr %p (arena %p + %llu), capacity %llu", (void *)g.rb_buf,
          (void *)g.rb_mem.mem, (void *)g.pack_buf, (void *)g.pack_mem.mem, (void *)g.rb_ptr,
          (void *)g_stage, (unsigned long long)g.rb_mem.off,
          (unsigned long long)g.rb_cap);
    /* N4.4 re-derivation: the pair's two buffers are both linear and, in this world's one
     * memory type, of the same pool -- so they take two ranges of ONE block and the driver is
     * handed one allocation for the pair, mapped once (the block's type is host-visible).
     * Nothing is freed: the block outlives both ranges. */
    check(c.buffers == 2u && c.allocs == 1u && c.maps == 1u && c.frees == 0u
          && c.buffer_destroys == 0u && g.rb_mem.pool >= 0 && g.rb_mem.pool == g.pack_mem.pool
          && g.rb_mem.mem == g.pack_mem.mem && g.rb_mem.off != g.pack_mem.off
          && g.pack_mem.off % VB_POOL_ALIGN == 0,
          "exactly two buffers, two ranges of one pool block, one allocation, nothing freed",
          "vkCreateBuffer %u, vkAllocateMemory %u, vkMapMemory %u, frees %u, destroys %u, "
          "rb %p@%llu, pack %p@%llu, pool %d", c.buffers, c.allocs, c.maps, c.frees,
          c.buffer_destroys, (void *)g.rb_mem.mem, (unsigned long long)g.rb_mem.off,
          (void *)g.pack_mem.mem, (unsigned long long)g.pack_mem.off, g.rb_mem.pool);

    /* the publication itself */
    check_guest(tag, &s, 1);
    check_dirty(tag, &s, 0);
    want_published(&w);
    check_counters(tag, &b, &a, &w, 0);
    snprintf(what, sizeof what, "%s the public accessor reports what the back end counted", tag);
    check(a.readbacks == g.st.readbacks && a.batches == g.st.readback_batches
          && a.packed == g.st.readback_packs && a.packed_pixels == g.st.readback_pixels
          && a.copied == g.st.readback_copies && a.bytes == g.st.readback_bytes,
          what, "stats %llu/%llu/%llu/%llu/%llu/%llu, g.st %llu/%llu/%llu/%llu/%llu/%llu",
          (unsigned long long)a.readbacks, (unsigned long long)a.batches,
          (unsigned long long)a.packed, (unsigned long long)a.packed_pixels,
          (unsigned long long)a.copied, (unsigned long long)a.bytes,
          (unsigned long long)g.st.readbacks, (unsigned long long)g.st.readback_batches,
          (unsigned long long)g.st.readback_packs, (unsigned long long)g.st.readback_pixels,
          (unsigned long long)g.st.readback_copies, (unsigned long long)g.st.readback_bytes);
    check(g.ok == 1 && g.failed == 0,
          "the back end is healthy and the flush counted its own time",
          "g.ok %d, g.failed %d, readback_ns %llu (a clock: printed, not asserted)",
          g.ok, g.failed, (unsigned long long)g.st.readback_ns);

    verdict(tag, chk, bad);
}

/* ── 2/3. an allocation failure: nothing may be submitted or published ─────────────── */

static void failure_case(int sixteen_first, const char *tag)
{
    Scenario s;
    Nv2aVkReadbackStats b, a;
    RbWant w;
    uint64_t stage_before = 0, stage_after = 0;
    char what[110];
    int chk = g_checks, bad = g_check_failures;

    section(sixteen_first
            ? "2. vkAllocateDescriptorSets fails, 16-bit target first: nothing was recorded yet"
            : "3. vkAllocateDescriptorSets fails, 4-byte target first: a copy was already recorded");
    note("the injected outcome is VK_ERROR_OUT_OF_POOL_MEMORY from the stub, not real pool "
         "exhaustion: once per batch, on the pack branch's vkAllocateDescriptorSets");

    scenario(&s, sixteen_first, 1);
    memcpy(g_snap, g_guest, GATE_GUEST);
    stage_before = hash_bytes(g_stage, GATE_STAGE);
    stats(&b);

    vb_targets_flush(s.ts, s.n);

    stats(&a);

    check_injection(tag);
    snprintf(what, sizeof what, "%s how far the batch had recorded when the failure hit", tag);
    check(c.imgcopy == (sixteen_first ? 0u : 1u) && c.dispatches == 0u && c.bufcopy == 0u, what,
          "4-byte copies %u (wanted %u), pack dispatches %u, pack buffer copies %u",
          c.imgcopy, sixteen_first ? 0u : 1u, c.dispatches, c.bufcopy);
    snprintf(what, sizeof what, "%s the batch was left recording, not submitted", tag);
    check(g.slots[0].recording == 1 && c.began == g.slots[0].cmd && c.submits == 0u, what,
          "slot 0 recording %d, command buffer %p, submits %u", g.slots[0].recording,
          (void *)c.began, c.submits);
    check_nothing_submitted(tag);
    stage_after = hash_bytes(g_stage, GATE_STAGE);
    snprintf(what, sizeof what, "%s the host wrote nothing into the staging buffer either", tag);
    check(stage_before == stage_after, what, "checksum 0x%016llX -> 0x%016llX",
          (unsigned long long)stage_before, (unsigned long long)stage_after);

    check_guest(tag, &s, 0);
    check_dirty(tag, &s, 1);
    want_nothing(&w);
    check_counters(tag, &b, &a, &w, 1);

    verdict(tag, chk, bad);
}

/* ── 4. the retry ───────────────────────────────────────────────────────────────────── */

static void retry_case(void)
{
    Scenario s;
    Nv2aVkReadbackStats b, mid, a;
    RbWant w;
    char what[110];
    int chk = g_checks, bad = g_check_failures;

    section("4. the retry: the next flush publishes what the failed one left dirty");
    note("the 4-byte target is first, so its copy had already been recorded when the failure hit; "
         "the second call clears the injected failure and succeeds");

    scenario(&s, 0, 1);
    memcpy(g_snap, g_guest, GATE_GUEST);
    stats(&b);

    vb_targets_flush(s.ts, s.n);                /* fails */
    stats(&mid);

    snprintf(what, sizeof what, "the failed call published nothing, so the retry has work to do");
    check(mid.readbacks == b.readbacks && mid.batches == b.batches && mid.bytes == b.bytes
          && mid.packed == b.packed && mid.copied == b.copied,
          what, "readbacks %llu/%llu, batches %llu/%llu, packed %llu/%llu, copied %llu/%llu, "
                "bytes %llu/%llu", (unsigned long long)mid.readbacks,
          (unsigned long long)b.readbacks, (unsigned long long)mid.batches,
          (unsigned long long)b.batches, (unsigned long long)mid.packed,
          (unsigned long long)b.packed, (unsigned long long)mid.copied,
          (unsigned long long)b.copied, (unsigned long long)mid.bytes,
          (unsigned long long)b.bytes);
    snprintf(what, sizeof what, "both targets are still dirty, so both will be staged again");
    check(s.t16->dirty == 1 && s.t4->dirty == 1, what, "16-bit dirty %d, 4-byte dirty %d",
          s.t16->dirty, s.t4->dirty);
    check(c.imgcopy == 1u && c.submits == 0u && c.dispatches == 0u,
          "the failed call recorded one copy and never submitted it",
          "4-byte copies %u, submits %u, pack dispatches %u", c.imgcopy, c.submits, c.dispatches);

    c.ds_fail_at = 0;                           /* the pool has room again */
    stage_gpu(&s, 0x50u, 0x60u);
    stats(&b);                                  /* the counters as the failed call left them */
    vb_targets_flush(s.ts, s.n);
    stats(&a);

    check(c.ds_allocs == 2u && c.ds_failures == 1u,
          "the retry's own allocation succeeded", "vkAllocateDescriptorSets %u, failures %u",
          c.ds_allocs, c.ds_failures);
    check(c.dispatches == 1u && c.imgcopy == 2u,
          "the retry recorded the pack dispatch the first call could not, and re-recorded the "
          "4-byte copy (the failed one was never submitted)",
          "pack dispatches %u, 4-byte copies %u over both calls", c.dispatches, c.imgcopy);
    check(c.submits == 1u && c.waits == 1u && c.invalidates == 1u
          && c.submitted[0] == g.slots[0].cmd,
          "the retry is the call that submitted, drained and invalidated",
          "submits %u, fence waits %u, invalidates %u", c.submits, c.waits, c.invalidates);

    check_guest("4 the retry", &s, 1);
    check_dirty("4 the retry", &s, 0);
    want_published(&w);
    check_counters("4 over both calls (the failed one contributed nothing)", &b, &a, &w, 0);
    snprintf(what, sizeof what, "4 over both calls the counters match the one publish that happened");
    check(a.readbacks - mid.readbacks == 2u && a.batches - mid.batches == 1u
          && a.bytes - mid.bytes == (uint64_t)GATE_PUBLISHED,
          what, "readbacks %llu -> %llu, batches %llu -> %llu, bytes %llu -> %llu",
          (unsigned long long)mid.readbacks, (unsigned long long)a.readbacks,
          (unsigned long long)mid.batches, (unsigned long long)a.batches,
          (unsigned long long)mid.bytes, (unsigned long long)a.bytes);
    check(g.ok == 1 && g.failed == 0,
          "the injected failure never became a device loss", "g.ok %d, g.failed %d", g.ok,
          g.failed);

    verdict("4 the retry after a failed batch", chk, bad);
}

/* ── 5. the staging pair's lifetime across the failed call ─────────────────────────── */

static void pair_case(void)
{
    Scenario s;
    VkBuffer rb0 = VK_NULL_HANDLE, pk0 = VK_NULL_HANDLE;
    VbAlloc rm0, pm0;                          /* N4.4: the pair's memory is a VbAlloc */
    memset(&rm0, 0, sizeof rm0);
    memset(&pm0, 0, sizeof pm0);
    uint8_t *ptr0 = NULL;
    VkDeviceSize cap0 = 0;
    char what[110];
    int chk = g_checks, bad = g_check_failures;

    section("5. the failed call must not free or replace the live staging pair");
    note("a first, successful flush makes the pair; the failing call then runs while a recording "
         "is open, which is exactly when vb_discard would hold a pair it cannot free");

    scenario(&s, 0, 0);
    s.t16->dirty = 0;                           /* the warm-up publishes the 4-byte target alone */
    stage_gpu(&s, 0x50u, 0x60u);
    vb_targets_flush(s.ts, s.n);

    rb0 = g.rb_buf; pk0 = g.pack_buf; rm0 = g.rb_mem; pm0 = g.pack_mem;
    ptr0 = g.rb_ptr; cap0 = g.rb_cap;
    check(c.buffers == 2u && rb0 != VK_NULL_HANDLE && pk0 != VK_NULL_HANDLE
          && rm0.mem != VK_NULL_HANDLE && pm0.mem != VK_NULL_HANDLE
          && cap0 == (VkDeviceSize)GATE_RB_CAP,
          "the warm-up publication made exactly one staging pair and nothing since",
          "vkCreateBuffer %u, rb %p/%p@%llu, pack %p/%p@%llu, capacity %llu", c.buffers,
          (void *)rb0, (void *)rm0.mem, (unsigned long long)rm0.off, (void *)pk0,
          (void *)pm0.mem, (unsigned long long)pm0.off, (unsigned long long)cap0);
    check(g.rb_ptr == g_stage + g.rb_mem.off && c.frees == 0u && c.buffer_destroys == 0u,
          "its pointer is the arena plus its range's offset, and nothing of it was freed",
          "rb_ptr %p (arena %p), frees %u, buffer destroys %u", (void *)g.rb_ptr,
          (void *)g_stage, c.frees, c.buffer_destroys);

    /* the failing batch: both targets dirty again, the pair already live, a recording open. */
    s.t16->dirty = 1;
    s.t4->dirty = 1;
    memcpy(g_snap, g_guest, GATE_GUEST);
    c.ds_fail_at = 1;
    vb_targets_flush(s.ts, s.n);

    check(g.rb_buf == rb0 && g.pack_buf == pk0 && g.rb_mem.mem == rm0.mem
          && g.rb_mem.off == rm0.off && g.pack_mem.mem == pm0.mem
          && g.pack_mem.off == pm0.off && g.rb_ptr == ptr0 && g.rb_cap == cap0,
          "the failed call left the pair's handles, pointer and capacity exactly as they were",
          "rb %p/%p, pack %p/%p, rb_ptr %p/%p, capacity %llu/%llu", (void *)g.rb_buf,
          (void *)rb0, (void *)g.pack_buf, (void *)pk0, (void *)g.rb_ptr, (void *)ptr0,
          (unsigned long long)g.rb_cap, (unsigned long long)cap0);
    check(c.buffers == 2u, "no further buffer was created: there is still exactly one pair",
          "vkCreateBuffer %u (a replaced pair would be 4)", c.buffers);
    check(c.frees == 0u && c.buffer_destroys == 0u && g.ngarbage == 0u,
          "nothing was freed, destroyed or queued for destruction under the open recording",
          "frees %u, buffer destroys %u, held garbage %llu", c.frees, c.buffer_destroys,
          (unsigned long long)g.ngarbage);
    check(g.slots[0].recording == 1,
          "the failed call really did leave a recording open over the pair",
          "slot 0 recording %d", g.slots[0].recording);
    check_guest("5 after the failed call", &s, 0);
    check_dirty("5 after the failed call", &s, 1);

    /* and the retry copies through that same live pair; the staged bytes differ this time, so
     * "every rectangle byte changed" stays an exact count */
    c.ds_fail_at = 0;
    stage_gpu(&s, 0x90u, 0x70u);
    vb_targets_flush(s.ts, s.n);

    check(c.imgcopy_buf == rb0 && c.bufcopy_dst == rb0 && c.bufcopy_src == pk0
          && c.imgcopy == 3u && c.bufcopy == 1u,
          "the successful retry copies into and through that same live pair",
          "copy destination %p (rb %p), buffer copy %p -> %p (pack %p -> rb %p), image copies %u "
          "(the warm-up's, the failed call's, the retry's), buffer copies %u",
          (void *)c.imgcopy_buf, (void *)rb0, (void *)c.bufcopy_src, (void *)c.bufcopy_dst,
          (void *)pk0, (void *)rb0, c.imgcopy, c.bufcopy);
    check(c.buffers == 2u && c.frees == 0u && c.buffer_destroys == 0u,
          "still one pair over the whole property, and none of it destroyed",
          "vkCreateBuffer %u, frees %u, buffer destroys %u", c.buffers, c.frees,
          c.buffer_destroys);
    check(g.rb_ptr == ptr0 && g.rb_cap == cap0 && g.rb_buf == rb0 && g.pack_buf == pk0,
          "the pair the retry published from is the one the warm-up made",
          "rb_ptr %p/%p, capacity %llu/%llu, rb %p/%p, pack %p/%p", (void *)g.rb_ptr,
          (void *)ptr0, (unsigned long long)g.rb_cap, (unsigned long long)cap0,
          (void *)g.rb_buf, (void *)rb0, (void *)g.pack_buf, (void *)pk0);
    check_guest("5 through the retry", &s, 1);
    check_dirty("5 through the retry", &s, 0);

    verdict("5 the staging pair across the failed call", chk, bad);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ───────── */

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
    (void)tex; (void)argb_out;
    return 0;
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

/* ── The run ────────────────────────────────────────────────────────────────────────── */

int main(void)
{
    unsigned i;

    printf("vk_readback_fault_gate: the readback publication's failure path, against the "
           "toolkit's own\nnv2a_vk_backend.c\n");
    printf("no GPU, no ICD and no skip path: every Vulkan entry point the paths reach is a "
           "controlled stub,\nand the staging buffer vkMapMemory hands back is the fixture's "
           "own bytes\n");

    g_guest = (uint8_t *)malloc(GATE_GUEST);
    g_snap = (uint8_t *)malloc(GATE_GUEST);
    g_expect = (uint8_t *)malloc(GATE_GUEST);
    g_stage = (uint8_t *)malloc(GATE_STAGE);
    for (i = 0; i < VB_SLOTS; i++)
        g_stream[i] = (uint8_t *)malloc(VB_SLOT_STREAM);
    if (!g_guest || !g_snap || !g_expect || !g_stage
        || !g_stream[0] || !g_stream[1] || !g_stream[2]) {
        fprintf(stderr, "FAIL: vk_readback_fault_gate: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();

    prop0();
    success_case(1, "1a the success control, 16-bit target first");
    success_case(0, "1b the success control, 4-byte target first");
    failure_case(1, "2 allocation failure, 16-bit target first");
    failure_case(0, "3 allocation failure, 4-byte target first");
    retry_case();
    pair_case();

    for (i = 0; i < VB_SLOTS; i++)
        free(g_stream[i]);
    free(g_stage);
    free(g_expect);
    free(g_snap);
    free(g_guest);

    printf("\n");
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_readback_fault_gate: %d of %d properties failed (%d of %d "
                        "checks failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_readback_fault_gate: %d properties, %d checks -- a batch whose pack set could "
           "not be allocated submits nothing and publishes nothing, its targets stay dirty and are "
           "published exactly once by the retry\n", g_props, g_checks);
    return 0;
}
