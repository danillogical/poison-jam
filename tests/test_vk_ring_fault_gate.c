/* Plan N4.1's repair, as a deciding failure-injection regression: the bounded ring of
 * three Vulkan submission slots must never write into a slot whose fence wait failed, and
 * a recording that cannot be begun must be discarded, not handed back.
 *
 * The defect this case exists for
 * -------------------------------
 * vb_reserve can advance the cursor into a slot that is still `pending` (the GPU's), and
 * the fence wait that advance performs can then fail: vb_slot_wait reports VK_TIMEOUT,
 * sets g.ok = 0, and leaves that slot pending. The pre-fix vb_target_load and vb_tex_upload
 * went on to call vb_stream and memcpy guest pixels into exactly that pending slot's mapped
 * 16 MiB stream -- overwriting bytes the in-flight submission may still be reading. No
 * later guard can undo a write, so the check has to be before it.
 *
 * The production bodies are the toolkit's own
 * ------------------------------------------
 * This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with
 * XBR_VK_BACKEND defined, so every function driven here -- vb_reserve, vb_stream,
 * vb_slot_wait, vb_slot_advance, vb_slot_submit, vb_cmd, vb_transfer_cmd, vb_barrier,
 * vb_image_barrier, vb_target_load, vb_tex_upload -- is compiled from the production
 * source, verbatim. Nothing is copied, so nothing can drift: the build recompiles these
 * bodies whenever that file changes, and tests/../scripts/vk-ring-fault-gate.py probes the
 * same bytes at build time and FAILS THE BUILD (exit 2) if a definition, the slot struct,
 * or one of the ring's constants the fixture computes with is gone. That script also
 * derives the seam markers in vk_ring_fault_gate_seam.h from those bytes; the pre-fix
 * source must still build -- it is what the case reports against -- so a missing latch or
 * discard buffer is a FAILED CHECK naming the marker, never a skipped case.
 *
 * Nothing of the toolkit is linked: the file's five undefined names are defined at the
 * bottom of this TU (xbox_GetMemoryOffset, nv2a_backend_register,
 * nv2a_backend_decode_texture, nv2a_backend_texture_span, nv2a_rc_unpack), and every
 * Vulkan entry point it uses is a static function pointer (VK_NO_PROTOTYPES) that this TU
 * points at controlled stubs before the first call.
 *
 * The injected results are STUBBED DRIVER OUTCOMES
 * ------------------------------------------------
 * There is no GPU here and no ICD is needed: vkWaitForFences returns the VkResult this
 * fixture selects, vkBeginCommandBuffer likewise, and the recording entry points only
 * count and record which command buffer they were handed. A VK_TIMEOUT below is a stub's
 * return value, NOT a real device timeout; the "[GPUVK] submission wait (10 s timeout)
 * failed" lines on stderr are the production code reporting the injected outcome.
 *
 * The properties
 * --------------
 *   1. target load, failed wait  -- the pending slot's stream is byte-identical, it stays
 *      pending, no fence/pool reset, no submit, no copy, g.ok == 0 and the failure latched.
 *   2. texture upload, failed wait -- the same, with the decode/create path stubbed.
 *   3. controls, wait succeeds -- each path writes its slice and commands its copy exactly
 *      once, the pending slot is cleared, and the stream holds exactly the expected bytes.
 *   4. failed begin -- vkBeginCommandBuffer fails: recording stays 0, g.ok == 0, the latch
 *      is set, the returned buffer is a valid non-submitted one (not the slot's), and the
 *      slot's command buffer is not ended (nor reset again). A control where begin
 *      succeeds returns the slot's own command buffer with recording == 1.
 *   5. after a failure, recording stays valid -- vb_cmd, vb_transfer_cmd, vb_barrier and
 *      vb_image_barrier all record into the same discard buffer and never touch a slot's
 *      command buffer; and a current slot that is still pending after a failed wait is
 *      neither reset nor begun into.
 *   6. an oversized slice whose begin fails -- the dedicated-buffer branch of vb_stream
 *      performs a begin of its own (so the discard outlives the submission); when that
 *      begin fails the call must return NULL with its own buffer destroyed and its memory
 *      freed exactly once, leaving nothing live and no pointer a caller could write into.
 *   7. the latch, mid-draw -- with the failure latched, vb_stream hands out neither a ring
 *      slice nor a dedicated buffer, and moves no slot's stream offset and writes no slot's
 *      mapped bytes.
 *
 * Expected verdicts
 * -----------------
 * The repaired source passes every group. The PRE-FIX source (toolkit c5f3696) is expected
 * to FAIL properties 1, 2, 4, 5a and 5b -- the failed-wait write into a still-pending
 * slot's stream (1, 2), the failed begin that armed the slot and handed back its own
 * unusable command buffer (4), and the recording that then kept using that buffer instead
 * of the discard one (5a, 5b) -- and to PASS the controls 3a and 3b, which are the sanity
 * anchor that separates "the defect" from "the fixture".
 *
 * A mutation that only deletes the two callers' guards is NOT the deciding mutant for
 * properties 1 and 2 on the repaired source: once vb_stream carries its own latch guard
 * (`if (g.failed) return NULL;`), those guards are defence in depth and the write cannot
 * happen either way. The deciding mutant for that pair has to remove vb_stream's latch
 * guard as well; for property 6 it has to remove the oversized branch's own refusal
 * (`if (g.failed) { vb_garbage_free(&e); return NULL; }`), whose absence is exactly the
 * dangling-pointer defect property 6 measures; for property 7 the same removal of the latch
 * guard is decisive. Property 4's deciding mutant is unchanged: a failed begin that arms
 * the slot (`recording = 1`) and/or returns the slot's own command buffer.
 *
 * There is no skip mode and no SKIP_RETURN_CODE in the build: the seam is inside this
 * translation unit, so the case always runs, and anything it cannot do (a missing seam, a
 * failed allocation) is a FAILURE, exit 1.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"          /* the production bodies, verbatim */

#include "vk_ring_fault_gate_seam.h"  /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture ─────────────────────────────────────────────────────────────────────── */

#define GATE_VA      0x00008000u               /* guest address of the fixture's pixels */
#define GATE_W       64u
#define GATE_H       16u
#define GATE_PITCH   (GATE_W * 4u)             /* bytes per row of guest memory */
#define GATE_SLICE   ((size_t)GATE_W * GATE_H * 4u)
#define GATE_GUEST   (1u << 20)                /* the stand-in for guest RAM */
#define GATE_DECODE  0xB0C00000u               /* what the stubbed decoder fills */
#define GATE_SEED    0x51ED270Bu

static uint8_t *g_guest;                       /* xbox_GetMemoryOffset()'s answer */
static uint8_t *g_stream[VB_SLOTS];            /* one mapped stream per slot */
static uint8_t *g_snap;                        /* a stream, copied before a call */
static uint8_t *g_expect;                      /* what a written slice must hold */
static uint8_t *g_map_arena;                   /* what vkMapMemory hands back */

static uintptr_t g_handle = 0x4000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

/* Every Vulkan call the fixture's paths can reach, and what it saw. */
typedef struct {
    unsigned waits;                  /* vkWaitForFences */
    VkResult wait_result;            /* the injected outcome of the next wait */
    unsigned fence_resets;           /* vkResetFences */
    unsigned pool_resets;            /* vkResetDescriptorPool */
    unsigned submits;                /* vkQueueSubmit */
    VkCommandBuffer submitted[8];
    unsigned begins;                 /* vkBeginCommandBuffer */
    VkResult begin_result;           /* the injected outcome of the next begin */
    VkCommandBuffer began;
    unsigned ends;                   /* vkEndCommandBuffer */
    VkCommandBuffer ended;
    unsigned resets;                 /* vkResetCommandBuffer */
    VkCommandBuffer reset_cb;
    unsigned barriers;               /* vkCmdPipelineBarrier with a memory barrier */
    VkCommandBuffer barrier_cb;
    unsigned imgbarriers;            /* vkCmdPipelineBarrier with an image barrier */
    VkCommandBuffer imgbarrier_cb;
    unsigned copies;                 /* vkCmdCopyBufferToImage */
    VkCommandBuffer copy_cb;
    VkBuffer copy_buf;
    VkImage copy_img;
    VkImageLayout copy_layout;
    VkBufferImageCopy copy_region;
    unsigned images;                 /* vkCreateImage */
    VkImage last_image;
    unsigned allocs;                 /* vkAllocateMemory */
    unsigned frees;                  /* vkFreeMemory */
    unsigned buffers;                /* vkCreateBuffer (a dedicated slice buffer) */
    unsigned buffer_destroys;        /* vkDestroyBuffer */
    unsigned maps;                   /* vkMapMemory */
    VkDeviceSize last_buffer_size;   /* what the last created buffer asked for */
    unsigned decodes;                /* nv2a_backend_decode_texture */
    unsigned spans;                  /* nv2a_backend_texture_span */
    unsigned registered;             /* nv2a_backend_register */
} GateCtl;

static GateCtl c;

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

static VkResult VKAPI_CALL gate_submit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits,
                                       VkFence fence)
{
    (void)queue; (void)fence;
    c.submits++;
    if (c.submits <= 8u && count && submits[0].commandBufferCount)
        c.submitted[c.submits - 1u] = submits[0].pCommandBuffers[0];
    return VK_SUCCESS;
}

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

static void VKAPI_CALL gate_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
                                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                                    uint32_t mbc, const VkMemoryBarrier *mb,
                                    uint32_t bbc, const VkBufferMemoryBarrier *bb,
                                    uint32_t ibc, const VkImageMemoryBarrier *ib)
{
    (void)src; (void)dst; (void)flags; (void)mbc; (void)mb; (void)bbc; (void)bb;
    c.barrier_cb = cmd;
    if (ibc && ib) {
        c.imgbarriers++;
        c.imgbarrier_cb = cmd;
    } else {
        c.barriers++;
    }
}

static void VKAPI_CALL gate_copy_to_image(VkCommandBuffer cmd, VkBuffer src, VkImage dst,
                                          VkImageLayout layout, uint32_t count,
                                          const VkBufferImageCopy *regions)
{
    c.copies++;
    c.copy_cb = cmd;
    c.copy_buf = src;
    c.copy_img = dst;
    c.copy_layout = layout;
    if (count)
        c.copy_region = regions[0];
}

static VkResult VKAPI_CALL gate_create_image(VkDevice device, const VkImageCreateInfo *info,
                                             const VkAllocationCallbacks *alloc, VkImage *image)
{
    (void)device; (void)info; (void)alloc;
    c.images++;
    c.last_image = (VkImage)hnew();
    *image = c.last_image;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_image_requirements(VkDevice device, VkImage image,
                                               VkMemoryRequirements *req)
{
    (void)device; (void)image;
    req->size = 64u * 1024u;
    req->alignment = 64u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_allocate(VkDevice device, const VkMemoryAllocateInfo *info,
                                         const VkAllocationCallbacks *alloc, VkDeviceMemory *mem)
{
    (void)device; (void)info; (void)alloc;
    c.allocs++;
    *mem = (VkDeviceMemory)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_bind_image(VkDevice device, VkImage image, VkDeviceMemory mem,
                                           VkDeviceSize offset)
{
    (void)device; (void)image; (void)mem; (void)offset;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_view(VkDevice device, const VkImageViewCreateInfo *info,
                                            const VkAllocationCallbacks *alloc, VkImageView *view)
{
    (void)device; (void)info; (void)alloc;
    *view = (VkImageView)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_free(VkDevice device, VkDeviceMemory mem,
                                 const VkAllocationCallbacks *alloc)
{
    (void)device; (void)mem; (void)alloc;
    c.frees++;
}

static void VKAPI_CALL gate_destroy_image(VkDevice device, VkImage image,
                                          const VkAllocationCallbacks *alloc)
{
    (void)device; (void)image; (void)alloc;
}

static void VKAPI_CALL gate_destroy_view(VkDevice device, VkImageView view,
                                         const VkAllocationCallbacks *alloc)
{
    (void)device; (void)view; (void)alloc;
}

static void VKAPI_CALL gate_destroy_buffer(VkDevice device, VkBuffer buf,
                                           const VkAllocationCallbacks *alloc)
{
    (void)device; (void)buf; (void)alloc;
    c.buffer_destroys++;
}

/* The dedicated slice buffer of vb_stream's oversized branch: a host-visible, mapped
 * buffer, tracked so a pointer into one that was freed can be told from a live one. */
static VkResult VKAPI_CALL gate_create_buffer(VkDevice device, const VkBufferCreateInfo *info,
                                              const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)alloc;
    c.buffers++;
    c.last_buffer_size = info ? info->size : 0;
    *buf = (VkBuffer)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_buffer_requirements(VkDevice device, VkBuffer buf,
                                                VkMemoryRequirements *req)
{
    (void)device; (void)buf;
    req->size = c.last_buffer_size ? c.last_buffer_size : 64u;
    req->alignment = VB_STREAM_ALIGN;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_bind_buffer(VkDevice device, VkBuffer buf, VkDeviceMemory mem,
                                            VkDeviceSize offset)
{
    (void)device; (void)buf; (void)mem; (void)offset;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_map(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    (void)device; (void)mem; (void)offset; (void)size; (void)flags;
    c.maps++;
    *ppData = g_map_arena;           /* the fixture's stand-in for a persistently mapped buffer */
    return VK_SUCCESS;
}

/* Point the back end's own entry points at the stubs above. The pointers are static in
 * the translation unit, which is why the file is compiled here. */
static void gate_wire(void)
{
    vkWaitForFences = gate_wait;
    vkResetFences = gate_fence_reset;
    vkResetDescriptorPool = gate_pool_reset;
    vkQueueSubmit = gate_submit;
    vkBeginCommandBuffer = gate_begin;
    vkEndCommandBuffer = gate_end;
    vkResetCommandBuffer = gate_cmd_reset;
    vkCmdPipelineBarrier = gate_barrier;
    vkCmdCopyBufferToImage = gate_copy_to_image;
    vkCreateImage = gate_create_image;
    vkGetImageMemoryRequirements = gate_image_requirements;
    vkAllocateMemory = gate_allocate;
    vkBindImageMemory = gate_bind_image;
    vkCreateImageView = gate_create_view;
    vkFreeMemory = gate_free;
    vkDestroyImage = gate_destroy_image;
    vkDestroyImageView = gate_destroy_view;
    vkDestroyBuffer = gate_destroy_buffer;
    vkCreateBuffer = gate_create_buffer;
    vkGetBufferMemoryRequirements = gate_buffer_requirements;
    vkBindBufferMemory = gate_bind_buffer;
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
    printf("      %-56s %s", what, cond ? "ok" : "FAIL");
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

/* The pixels of the fixture's target, as vb_target_load must copy them for bpp == 4. */
static void expect_target_rows(uint8_t *dst)
{
    uint32_t y;

    for (y = 0; y < GATE_H; y++)
        memcpy(dst + (size_t)y * GATE_W * 4u, g_guest + GATE_VA + (size_t)y * GATE_PITCH,
               (size_t)GATE_W * 4u);
}

static void gate_world(void)
{
    unsigned i;

    free(g.scratch);                     /* what a previous property's decode allocated */
    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    c.wait_result = VK_SUCCESS;
    c.begin_result = VK_SUCCESS;

    g.dev = (VkDevice)hnew();
    g.queue = (VkQueue)hnew();
    /* One memory type that satisfies every request, so vb_alloc's probes always land. */
    g.mp.memoryTypeCount = 1;
    g.mp.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                      | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                                      | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                      | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    g.mp.memoryTypes[0].heapIndex = 0;
    g.ok = 1;
#if VB_GATE_HAS_DEAD_BUFFER
    g.dead = (VkCommandBuffer)hnew();    /* as install leaves it: allocated and begun */
#endif
    for (i = 0; i < VB_SLOTS; i++) {
        g.slots[i].cmd = (VkCommandBuffer)hnew();
        g.slots[i].fence = (VkFence)hnew();
        g.slots[i].dpool = (VkDescriptorPool)hnew();
        g.slots[i].stream = (VkBuffer)hnew();
        g.slots[i].stream_ptr = g_stream[i];
        fill_bytes(g_stream[i], VB_SLOT_STREAM, GATE_SEED + i);
    }
    fill_bytes(g_guest, GATE_GUEST, GATE_SEED + 0x40u);
}

static void wire_target(VbTarget *t)
{
    memset(t, 0, sizeof *t);
    t->used = 1;
    t->va = GATE_VA;
    t->pitch = GATE_PITCH;
    t->bpp = 4;
    t->w = GATE_W;
    t->h = GATE_H;
    t->stale = 1;
    t->im.img = (VkImage)hnew();
}

static int was_submitted(VkCommandBuffer cmd)
{
    unsigned i;

    for (i = 0; i < c.submits && i < 8u; i++)
        if (c.submitted[i] == cmd)
            return 1;
    return 0;
}

/* ── 1. target load, failed wait ─────────────────────────────────────────────────────── */

static void prop1(void)
{
    VbTarget *t;
    uint64_t before, after;
    unsigned submits_before, loads_before;
    int chk = g_checks, bad = g_check_failures;

    section("1. target load, failed wait: the pending slot's stream must not be touched");
    gate_world();
    c.wait_result = VK_TIMEOUT;

    t = &g.targets[0];
    wire_target(t);

    /* The recording slot has no room left, so the load's vb_reserve must advance -- and the
     * slot it advances into is still in flight. */
    g.cur = 0;
    g.slots[0].stream_off = VB_SLOT_STREAM;
    g.slots[0].pending = 0;
    g.slots[0].recording = 0;
    g.slots[1].pending = 1;
    g.slots[1].stream_off = 0;
    g.slots[1].sets_used = 7;
    g.npending = 1;

    memcpy(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM);
    before = hash_bytes(g.slots[1].stream_ptr, VB_SLOT_STREAM);
    submits_before = (unsigned)g.st.submits;
    loads_before = (unsigned)g.st.rt_loads;

    vb_target_load(t);

    after = hash_bytes(g.slots[1].stream_ptr, VB_SLOT_STREAM);
    check(g.cur == 1, "the reserve advanced into the pending slot", "cur %u", g.cur);
    check(g.st.overflows == 1, "the reserve took its overflow path", "overflows %llu",
          (unsigned long long)g.st.overflows);
    check(memcmp(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM) == 0,
          "the pending slot's whole 16 MiB stream is byte-identical",
          "checksum 0x%016llX -> 0x%016llX, %zu of %u bytes differ",
          (unsigned long long)before, (unsigned long long)after,
          count_diff(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM), (unsigned)VB_SLOT_STREAM);
    check(g.slots[1].pending == 1, "the slot the reserve moved into is still pending",
          "pending %d", g.slots[1].pending);
    check(g.slots[1].stream_off == 0, "its stream offset is unchanged", "stream_off %llu",
          (unsigned long long)g.slots[1].stream_off);
    check(g.slots[1].sets_used == 7, "its descriptor-set count is unchanged", "sets_used %u",
          g.slots[1].sets_used);
    check(g.ok == 0, "the back end reports the lost device", "g.ok %d", g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 1, "the failure is latched", "g.failed %d", g.failed);
#else
    check(0, "the source carries no g.failed latch: the repair is not in this source",
          "VB_GATE_HAS_FAILED_LATCH 0");
#endif
    check(c.fence_resets == 0, "no fence was reset", "%u reset(s)", c.fence_resets);
    check(c.pool_resets == 0, "no descriptor pool was reset", "%u reset(s)", c.pool_resets);
    check(c.submits == 0 && g.st.submits == submits_before, "nothing was submitted",
          "%u submit(s), g.st.submits %llu -> %llu", c.submits,
          (unsigned long long)submits_before, (unsigned long long)g.st.submits);
    check(c.copies == 0, "no GPU copy was commanded", "%u copy(ies)", c.copies);
    check(g.st.rt_loads == loads_before, "the load did not run", "rt_loads %llu",
          (unsigned long long)g.st.rt_loads);
    check(t->stale, "the target is still stale: the copy never happened", "stale %d", t->stale);
    verdict("1 target load, failed wait", chk, bad);
}

/* ── 2. texture upload, failed wait ─────────────────────────────────────────────────── */

static void prop2(void)
{
    Nv2aTexture tx;
    VbTex *e;
    uint64_t before, after;
    unsigned submits_before, uploads_before;
    int chk = g_checks, bad = g_check_failures;

    section("2. texture upload, failed wait: the pending slot's stream must not be touched");
    gate_world();
    c.wait_result = VK_TIMEOUT;

    memset(&tx, 0, sizeof tx);
    tx.offset = GATE_VA;
    tx.width = GATE_W;
    tx.height = GATE_H;
    tx.pitch = GATE_PITCH;
    tx.color = 0x12u;                    /* LIN_A8R8G8B8: not DXT, so the decode path runs */
    e = &g.texs[0];
    memset(e, 0, sizeof *e);

    g.cur = 0;
    g.slots[0].stream_off = VB_SLOT_STREAM;
    g.slots[0].pending = 0;
    g.slots[0].recording = 0;
    g.slots[1].pending = 1;
    g.slots[1].stream_off = 0;
    g.slots[1].sets_used = 9;
    g.npending = 1;

    memcpy(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM);
    before = hash_bytes(g.slots[1].stream_ptr, VB_SLOT_STREAM);
    submits_before = (unsigned)g.st.submits;
    uploads_before = (unsigned)g.st.tex_uploads;

    vb_tex_upload(e, &tx);

    after = hash_bytes(g.slots[1].stream_ptr, VB_SLOT_STREAM);
    check(c.decodes == 1, "the fixture reached the decode (its scenario is the real one)",
          "%u decode(s)", c.decodes);
    check(g.cur == 1, "the reserve advanced into the pending slot", "cur %u", g.cur);
    check(g.st.overflows == 1, "the reserve took its overflow path", "overflows %llu",
          (unsigned long long)g.st.overflows);
    check(memcmp(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM) == 0,
          "the pending slot's whole 16 MiB stream is byte-identical",
          "checksum 0x%016llX -> 0x%016llX, %zu of %u bytes differ",
          (unsigned long long)before, (unsigned long long)after,
          count_diff(g_snap, g.slots[1].stream_ptr, VB_SLOT_STREAM), (unsigned)VB_SLOT_STREAM);
    check(g.slots[1].pending == 1, "the slot the reserve moved into is still pending",
          "pending %d", g.slots[1].pending);
    check(g.slots[1].stream_off == 0, "its stream offset is unchanged", "stream_off %llu",
          (unsigned long long)g.slots[1].stream_off);
    check(g.slots[1].sets_used == 9, "its descriptor-set count is unchanged", "sets_used %u",
          g.slots[1].sets_used);
    check(g.ok == 0, "the back end reports the lost device", "g.ok %d", g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 1, "the failure is latched", "g.failed %d", g.failed);
#else
    check(0, "the source carries no g.failed latch: the repair is not in this source",
          "VB_GATE_HAS_FAILED_LATCH 0");
#endif
    check(c.fence_resets == 0, "no fence was reset", "%u reset(s)", c.fence_resets);
    check(c.pool_resets == 0, "no descriptor pool was reset", "%u reset(s)", c.pool_resets);
    check(c.submits == 0 && g.st.submits == submits_before, "nothing was submitted",
          "%u submit(s), g.st.submits %llu -> %llu", c.submits,
          (unsigned long long)submits_before, (unsigned long long)g.st.submits);
    check(c.copies == 0, "no GPU copy was commanded", "%u copy(ies)", c.copies);
    check(c.images == 0, "no image was created for the aborted upload", "%u image(s)", c.images);
    check(g.st.tex_uploads == uploads_before, "the upload did not run", "tex_uploads %llu",
          (unsigned long long)g.st.tex_uploads);
    verdict("2 texture upload, failed wait", chk, bad);
}

/* ── 3. the controls: the same paths with a wait that succeeds ──────────────────────── */

static void prop3(void)
{
    VbTarget *t;
    Nv2aTexture tx;
    VbTex *e;
    uint32_t i;
    size_t bad;
    int chk = g_checks, bad_checks = g_check_failures;

    section("3. controls: a wait that succeeds writes the slice and commands one copy");
    note("the injected outcome is VK_SUCCESS here; nothing else is changed");

    /* 3a: target load. The slot being recorded into is still pending, so the successful
     * wait inside vb_cmd must clear it before the load is recorded. */
    gate_world();
    c.wait_result = VK_SUCCESS;
    t = &g.targets[0];
    wire_target(t);
    expect_target_rows(g_expect);
    g.cur = 0;
    g.slots[0].pending = 1;
    g.slots[0].stream_off = 0;
    g.npending = 1;

    vb_target_load(t);

    check(c.waits == 1, "3a: the path waited the slot it was about to record into", "%u wait(s)",
          c.waits);
    check(g.st.overflows == 0 && g.cur == 0, "3a: no overflow, so the load stayed in slot 0",
          "overflows %llu, cur %u", (unsigned long long)g.st.overflows, g.cur);
    check(c.copies == 1, "3a: the slice was copied to the target exactly once", "%u copy(ies)",
          c.copies);
    check(c.copy_cb == g.slots[0].cmd, "3a: the copy was recorded in the slot's own buffer",
          "copy cb %p, slot cmd %p", (void *)c.copy_cb, (void *)g.slots[0].cmd);
    check(c.copy_buf == g.slots[0].stream && c.copy_img == t->im.img,
          "3a: the copy reads the slot's stream into the target's image", "");
    check(c.copy_region.bufferOffset == 0 && c.copy_region.bufferRowLength == GATE_W
          && c.copy_region.imageExtent.width == GATE_W
          && c.copy_region.imageExtent.height == GATE_H,
          "3a: the copy region is the fixture's, at the slice's offset", "offset %llu, row %u, %ux%u",
          (unsigned long long)c.copy_region.bufferOffset, c.copy_region.bufferRowLength,
          c.copy_region.imageExtent.width, c.copy_region.imageExtent.height);
    check(c.copy_layout == VK_IMAGE_LAYOUT_GENERAL, "3a: the target is copied in GENERAL", "%d",
          (int)c.copy_layout);
    bad = count_diff(g.slots[0].stream_ptr, g_expect, GATE_SLICE);
    check(bad == 0, "3a: the slot's stream holds exactly the target's pixels",
          "%zu of %zu bytes differ", bad, GATE_SLICE);
    check(g.slots[0].pending == 0 && g.npending == 0, "3a: the pending slot was cleared",
          "pending %d, npending %u", g.slots[0].pending, g.npending);
    check(c.fence_resets == 1 && c.pool_resets == 1,
          "3a: the successful wait reset the fence and the descriptor pool",
          "fences %u, pools %u", c.fence_resets, c.pool_resets);
    check(g.slots[0].recording == 1 && c.began == g.slots[0].cmd,
          "3a: the slot's own buffer was begun and is recording", "recording %d",
          g.slots[0].recording);
    check(c.barriers == 1 && c.barrier_cb == g.slots[0].cmd,
          "3a: the enclosing barrier was recorded in the same buffer", "%u barrier(s)", c.barriers);
    check(t->stale == 0 && g.st.rt_loads == 1, "3a: the load completed", "stale %d, rt_loads %llu",
          t->stale, (unsigned long long)g.st.rt_loads);
    check(c.submits == 0 && g.ok == 1, "3a: nothing was submitted and the ring is healthy",
          "submits %u, ok %d", c.submits, g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 0, "3a: no failure was latched", "g.failed %d", g.failed);
#endif
    verdict("3a target load, wait succeeds", chk, bad_checks);

    /* 3b: texture upload. */
    chk = g_checks;
    bad_checks = g_check_failures;
    gate_world();
    c.wait_result = VK_SUCCESS;
    memset(&tx, 0, sizeof tx);
    tx.offset = GATE_VA;
    tx.width = GATE_W;
    tx.height = GATE_H;
    tx.pitch = GATE_PITCH;
    tx.color = 0x12u;
    e = &g.texs[0];
    memset(e, 0, sizeof *e);
    for (i = 0; i < GATE_W * GATE_H; i++)
        ((uint32_t *)g_expect)[i] = GATE_DECODE | (i & 0xFFFFFu);
    g.cur = 0;
    g.slots[0].pending = 1;
    g.slots[0].stream_off = 0;
    g.npending = 1;

    vb_tex_upload(e, &tx);

    check(c.decodes == 1, "3b: the stubbed decoder ran once", "%u decode(s)", c.decodes);
    check(c.waits == 1, "3b: the path waited the slot it was about to record into", "%u wait(s)",
          c.waits);
    check(c.images == 1 && e->im.img == c.last_image && c.allocs == 1,
          "3b: the texture's image was created and bound", "%u image(s), %u allocation(s)",
          c.images, c.allocs);
    check(c.copies == 1, "3b: the slice was copied into the texture exactly once", "%u copy(ies)",
          c.copies);
    check(c.copy_cb == g.slots[0].cmd, "3b: the copy was recorded in the slot's own buffer",
          "copy cb %p, slot cmd %p", (void *)c.copy_cb, (void *)g.slots[0].cmd);
    check(c.copy_buf == g.slots[0].stream && c.copy_img == e->im.img,
          "3b: the copy reads the slot's stream into the texture's image", "");
    check(c.copy_region.bufferOffset == 0 && c.copy_region.imageExtent.width == GATE_W
          && c.copy_region.imageExtent.height == GATE_H,
          "3b: the copy region is the fixture's, at the slice's offset", "offset %llu, %ux%u",
          (unsigned long long)c.copy_region.bufferOffset, c.copy_region.imageExtent.width,
          c.copy_region.imageExtent.height);
    bad = count_diff(g.slots[0].stream_ptr, g_expect, GATE_SLICE);
    check(bad == 0, "3b: the slot's stream holds exactly the decoded texels",
          "%zu of %zu bytes differ", bad, GATE_SLICE);
    check(g.slots[0].pending == 0 && g.npending == 0, "3b: the pending slot was cleared",
          "pending %d, npending %u", g.slots[0].pending, g.npending);
    check(c.fence_resets == 1 && c.pool_resets == 1,
          "3b: the successful wait reset the fence and the descriptor pool",
          "fences %u, pools %u", c.fence_resets, c.pool_resets);
    check(c.imgbarriers == 2 && c.imgbarrier_cb == g.slots[0].cmd,
          "3b: both image barriers were recorded in the same buffer", "%u image barrier(s)",
          c.imgbarriers);
    check(g.st.tex_uploads == 1, "3b: the upload completed", "tex_uploads %llu",
          (unsigned long long)g.st.tex_uploads);
    check(c.submits == 0 && g.ok == 1, "3b: nothing was submitted and the ring is healthy",
          "submits %u, ok %d", c.submits, g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 0, "3b: no failure was latched", "g.failed %d", g.failed);
#endif
    verdict("3b texture upload, wait succeeds", chk, bad_checks);
}

/* ── 4. a failed vkBeginCommandBuffer ───────────────────────────────────────────────── */

static void prop4(void)
{
    VkCommandBuffer cb;
    int chk = g_checks, bad = g_check_failures;

    section("4. a failed vkBeginCommandBuffer leaves the slot unrecordable");
    note("the injected outcome is VK_ERROR_OUT_OF_HOST_MEMORY, not a real device error");

    gate_world();
    c.begin_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    cb = vb_cmd();

    check(c.begins == 1 && c.began == g.slots[0].cmd,
          "the begin was attempted on the slot's own command buffer", "%u begin(s)", c.begins);
    check(g.slots[0].recording == 0, "recording stays 0: the failed begin did not arm the slot",
          "recording %d", g.slots[0].recording);
    check(g.ok == 0, "the back end reports the failure", "g.ok %d", g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 1, "the failure is latched", "g.failed %d", g.failed);
#else
    check(0, "the source carries no g.failed latch: the repair is not in this source",
          "VB_GATE_HAS_FAILED_LATCH 0");
#endif
    check(cb != VK_NULL_HANDLE, "the returned command buffer is a valid, non-null handle", "%p",
          (void *)cb);
    check(cb != g.slots[0].cmd, "it is NOT the slot's own command buffer",
          "returned %p, slot cmd %p", (void *)cb, (void *)g.slots[0].cmd);
#if VB_GATE_HAS_DEAD_BUFFER
    check(cb == g.dead, "it is the discard buffer g.dead", "returned %p, g.dead %p",
          (void *)cb, (void *)g.dead);
#else
    check(0, "the source carries no g.dead discard buffer: the repair is not in this source",
          "VB_GATE_HAS_DEAD_BUFFER 0");
#endif
    check(c.ends == 0, "the slot's command buffer was not ended", "%u end(s)", c.ends);
    check(c.resets <= 1 && c.reset_cb == g.slots[0].cmd,
          "the slot's command buffer was not reset again after the failure",
          "%u reset(s), last %p", c.resets, (void *)c.reset_cb);
    check(c.submits == 0 && !was_submitted(cb), "the returned buffer was never submitted",
          "%u submit(s)", c.submits);

    /* The control: with the begin succeeding, the slot's own buffer is handed back and is
     * recording. */
    gate_world();
    c.begin_result = VK_SUCCESS;
    cb = vb_cmd();
    check(cb == g.slots[0].cmd, "control: a successful begin returns the slot's own command buffer",
          "returned %p, slot cmd %p", (void *)cb, (void *)g.slots[0].cmd);
    check(g.slots[0].recording == 1, "control: the slot is recording", "recording %d",
          g.slots[0].recording);
    check(g.ok == 1, "control: the back end is healthy", "g.ok %d", g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 0, "control: no failure was latched", "g.failed %d", g.failed);
#endif
    check(c.submits == 0, "control: nothing was submitted", "%u submit(s)", c.submits);
    verdict("4 failed begin, and its control", chk, bad);
}

/* ── 5. after a failure, recording calls stay valid ─────────────────────────────────── */

static void prop5(void)
{
    VkCommandBuffer dead, a, b;
    unsigned resets_after, ends_after, begins_after;
    int chk = g_checks, bad = g_check_failures;

    section("5. after a failure, every recording call goes to the discard buffer");

    /* 5a: a failure observed through the seam itself -- a begin that fails -- then the calls
     * a draw would make. */
    gate_world();
    c.begin_result = VK_ERROR_DEVICE_LOST;
    dead = vb_cmd();
    check(dead != g.slots[0].cmd && dead != VK_NULL_HANDLE,
          "the failed begin did not hand back the slot's own buffer",
          "returned %p, slot cmd %p", (void *)dead, (void *)g.slots[0].cmd);
    resets_after = c.resets;
    ends_after = c.ends;
    begins_after = c.begins;

    a = vb_cmd();
    b = vb_transfer_cmd();
    vb_barrier();
    vb_image_barrier((VkImage)hnew(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_GENERAL);

    check(a == dead && b == dead, "vb_cmd and vb_transfer_cmd return the same buffer", "");
    check(c.barriers == 1 && c.barrier_cb == dead, "vb_barrier records into it", "%u barrier(s)",
          c.barriers);
    check(c.imgbarriers == 1 && c.imgbarrier_cb == dead, "vb_image_barrier records into it",
          "%u image barrier(s)", c.imgbarriers);
    check(c.resets == resets_after && c.ends == ends_after && c.begins == begins_after,
          "no slot's command buffer was reset, ended or begun again",
          "resets %u->%u, ends %u->%u, begins %u->%u", resets_after, c.resets, ends_after,
          c.ends, begins_after, c.begins);
    check(c.reset_cb != dead && c.ended != dead,
          "the discard buffer itself is never reset or ended", "");
    check(g.slots[0].recording == 0, "no slot was left recording", "recording %d",
          g.slots[0].recording);
    check(c.submits == 0, "nothing was submitted", "%u submit(s)", c.submits);
#if VB_GATE_HAS_DEAD_BUFFER
    check(dead == g.dead && g.dead != VK_NULL_HANDLE, "the buffer is g.dead, as install left it",
          "returned %p, g.dead %p", (void *)dead, (void *)g.dead);
#else
    check(0, "the source carries no g.dead discard buffer: the repair is not in this source",
          "VB_GATE_HAS_DEAD_BUFFER 0");
#endif
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 1, "the failure is latched", "g.failed %d", g.failed);
#endif
    verdict("5a recording after a failure", chk, bad);

    /* 5b: the other way vb_cmd reaches the discard buffer -- the current slot is still
     * pending after a failed wait and the device is gone. Its command buffer belongs to the
     * in-flight submission: it must not be reset, begun or recorded into. */
    chk = g_checks;
    bad = g_check_failures;
    gate_world();
    g.ok = 0;
    g.cur = 0;
    g.slots[0].pending = 1;
    a = vb_cmd();
    check(a != g.slots[0].cmd && a != VK_NULL_HANDLE,
          "5b: a pending slot's buffer is not handed back",
          "returned %p, slot cmd %p", (void *)a, (void *)g.slots[0].cmd);
    check(c.resets == 0 && c.begins == 0,
          "5b: the pending slot's buffer was neither reset nor begun", "resets %u, begins %u",
          c.resets, c.begins);
    check(c.ends == 0 && c.submits == 0, "5b: it was neither ended nor submitted",
          "ends %u, submits %u", c.ends, c.submits);
    check(c.waits == 0, "5b: no fence was waited on a device that is gone", "%u wait(s)", c.waits);
    check(g.slots[0].pending == 1 && g.slots[0].recording == 0,
          "5b: the slot is still pending and not recording", "pending %d, recording %d",
          g.slots[0].pending, g.slots[0].recording);
#if VB_GATE_HAS_DEAD_BUFFER
    check(a == g.dead, "5b: the discard buffer is handed back instead", "returned %p, g.dead %p",
          (void *)a, (void *)g.dead);
#else
    check(0, "5b: the source carries no g.dead discard buffer: the repair is not in this source",
          "VB_GATE_HAS_DEAD_BUFFER 0");
#endif
    verdict("5b a pending slot with the device gone", chk, bad);
}

/* ── 6. an oversized slice whose begin fails hands out nothing ──────────────────────── */

static void prop6(void)
{
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceSize off = 0;
    uint8_t *p;
    uint64_t before, after;
    int chk = g_checks, bad = g_check_failures;

    section("6. an oversized slice whose begin fails must not hand back freed memory");
    note("the injected outcome is a failed vkBeginCommandBuffer inside vb_stream's own "
         "dedicated-buffer branch, with nothing recording and nothing pending");
    gate_world();
    c.begin_result = VK_ERROR_OUT_OF_HOST_MEMORY;
    g.cur = 0;
    g.slots[0].recording = 0;
    g.slots[0].pending = 0;
    g.slots[0].stream_off = 0;
    memcpy(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM);
    before = hash_bytes(g.slots[0].stream_ptr, VB_SLOT_STREAM);

    /* Larger than half a slot's stream, so vb_stream cannot use the slot's own stream: it
     * allocates a dedicated buffer and then calls vb_cmd -- whose begin fails here. */
    p = vb_stream(VB_SLOT_STREAM, &buf, &off);

    after = hash_bytes(g.slots[0].stream_ptr, VB_SLOT_STREAM);
    check(c.buffers == 1 && c.allocs == 1 && c.maps == 1,
          "the branch allocated, bound and mapped exactly one dedicated buffer",
          "%u buffer(s), %u allocation(s), %u mapping(s)", c.buffers, c.allocs, c.maps);
    check(g.ok == 0 && c.begins == 1, "the begin that branch performs really failed",
          "begins %u, g.ok %d", c.begins, g.ok);
#if VB_GATE_HAS_FAILED_LATCH
    check(g.failed == 1, "the failure is latched", "g.failed %d", g.failed);
#else
    check(0, "the source carries no g.failed latch: the repair is not in this source",
          "VB_GATE_HAS_FAILED_LATCH 0");
#endif
    check(p == NULL, "the call returned NULL: no caller-visible slice at all", "%p", (void *)p);
    check(c.frees == 1, "its memory was freed exactly once before it returned", "%u free(s)",
          c.frees);
    check(c.buffer_destroys == 1, "its buffer was destroyed exactly once", "%u destruction(s)",
          c.buffer_destroys);
    check(c.buffers == c.buffer_destroys && c.allocs == c.frees,
          "nothing that branch allocated is still live", "buffers %u/%u, memory %u/%u",
          c.buffers, c.buffer_destroys, c.allocs, c.frees);
    check(g.slots[0].stream_off == 0 && memcmp(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM) == 0,
          "no slot's stream was touched by the failed branch",
          "stream_off %llu, checksum 0x%016llX -> 0x%016llX, %zu bytes differ",
          (unsigned long long)g.slots[0].stream_off, (unsigned long long)before,
          (unsigned long long)after, count_diff(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM));
    verdict("6 oversized slice, failed begin", chk, bad);
}

/* ── 7. the latch, once set, can never hand out a slice again ───────────────────────── */

static void prop7(void)
{
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceSize off = 0;
    uint8_t *p;
    uint64_t before, after;
    unsigned y, moved;
    int chk = g_checks, bad = g_check_failures;

    section("7. with the failure latched, vb_stream hands out nothing at all");
    note("the state a failure mid-draw leaves: the latch set, and a ring slot free");

    gate_world();
#if VB_GATE_HAS_FAILED_LATCH
    g.failed = 1;
#else
    check(0, "the source carries no g.failed latch: the repair is not in this source",
          "VB_GATE_HAS_FAILED_LATCH 0");
#endif
    g.ok = 0;
    c.begin_result = VK_ERROR_DEVICE_LOST;   /* how the latch got set in the first place */
    g.cur = 0;
    for (y = 0; y < VB_SLOTS; y++) {
        g.slots[y].recording = 0;
        g.slots[y].pending = 0;
        g.slots[y].stream_off = 0;
    }
    memcpy(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM);
    before = hash_bytes(g.slots[0].stream_ptr, VB_SLOT_STREAM);

    /* A slice that would fit the ring's stream: a slot that is free must still not hand it
     * out once the failure is latched. */
    p = vb_stream(4096u, &buf, &off);
    check(p == NULL, "the ring slice is NULL, not a slice of a slot's stream", "%p", (void *)p);

    /* And one that would need a buffer of its own: allocating one now would be the same
     * defect one indirection further out. */
    p = vb_stream(VB_SLOT_STREAM, &buf, &off);
    check(p == NULL, "the oversized slice is NULL too", "%p", (void *)p);
    check(c.buffers == 0 && c.allocs == 0 && c.maps == 0,
          "neither call allocated a buffer or mapped memory", "%u buffer(s), %u allocation(s), "
          "%u mapping(s)", c.buffers, c.allocs, c.maps);

    moved = 0;
    for (y = 0; y < VB_SLOTS; y++)
        if (g.slots[y].stream_off != 0)
            moved++;
    check(moved == 0, "no slot's stream offset was moved", "%u of %u slots moved", moved,
          (unsigned)VB_SLOTS);
    after = hash_bytes(g.slots[0].stream_ptr, VB_SLOT_STREAM);
    check(memcmp(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM) == 0,
          "no slot's mapped bytes were written", "checksum 0x%016llX -> 0x%016llX, %zu bytes differ",
          (unsigned long long)before, (unsigned long long)after,
          count_diff(g_snap, g.slots[0].stream_ptr, VB_SLOT_STREAM));
    check(c.begins == 0 && c.resets == 0 && c.submits == 0,
          "nothing was begun, reset or submitted on the way", "begins %u, resets %u, submits %u",
          c.begins, c.resets, c.submits);
    verdict("7 the latch, mid-draw", chk, bad);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ───────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)g_guest;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    (void)backend;
    c.registered++;
}

int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t i, n = tex->width * tex->height;

    for (i = 0; i < n; i++)
        argb_out[i] = GATE_DECODE | (i & 0xFFFFFu);
    c.decodes++;
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    (void)tex;
    c.spans++;
    return 0;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* ── The seam manifest, and the run ─────────────────────────────────────────────────── */

static void marker(const char *what, int present)
{
    printf("      %-56s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

static void seam_section(void)
{
    section("0. the source under test, probed from the same bytes at build time");
    printf("      production bodies compiled from: %s\n", VB_GATE_SOURCE);
    printf("      sha256 %s\n", VB_GATE_SOURCE_SHA256);
    printf("      %u bytes\n", (unsigned)VB_GATE_SOURCE_BYTES);
    note("every function driven below is that file's own, compiled into this translation unit "
         "by #include; scripts/vk-ring-fault-gate.py rejects the BUILD when a definition, the "
         "slot struct or a ring constant this fixture uses is gone");
    marker("the early return after vb_reserve, target load", VB_GATE_HAS_TARGET_LOAD_GUARD);
    marker("the early return after vb_reserve, texture upload", VB_GATE_HAS_TEX_UPLOAD_GUARD);
    marker("the write-once failure latch (g.failed)", VB_GATE_HAS_FAILED_LATCH);
    marker("the discard buffer (g.dead)", VB_GATE_HAS_DEAD_BUFFER);
    marker("ring size, checked against VB_SLOTS", VB_SLOTS == 3);
    note("an ABSENT marker is reported by the property that needs it, as a failed check; the "
         "injected VkResults below are stubs' return values, not a real device timeout");
}

int main(void)
{
    unsigned i;

    printf("vk_ring_fault_gate: the bounded ring's failure path, against the toolkit's own "
           "nv2a_vk_backend.c\n");
    printf("no GPU, no ICD and no skip path: every Vulkan entry point the paths reach is a "
           "controlled stub\n");

    g_guest = (uint8_t *)malloc(GATE_GUEST);
    g_snap = (uint8_t *)malloc(VB_SLOT_STREAM);
    g_expect = (uint8_t *)malloc(GATE_SLICE);
    g_map_arena = (uint8_t *)malloc(VB_SLOT_STREAM);
    for (i = 0; i < VB_SLOTS; i++)
        g_stream[i] = (uint8_t *)malloc(VB_SLOT_STREAM);
    if (!g_guest || !g_snap || !g_expect || !g_map_arena
        || !g_stream[0] || !g_stream[1] || !g_stream[2]) {
        fprintf(stderr, "FAIL: vk_ring_fault_gate: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();

    seam_section();

    section("preconditions of the fixture");
    check(sizeof(g.slots) / sizeof(g.slots[0]) == VB_SLOTS,
          "the ring has VB_SLOTS slots", "%zu", sizeof(g.slots) / sizeof(g.slots[0]));
    check(VB_SLOT_STREAM == (16u << 20) && GATE_SLICE <= VB_SLOT_STREAM / 2u,
          "the fixture's slice is small enough for vb_stream to use the slot's own stream",
          "slice %zu of %u bytes", GATE_SLICE, (unsigned)VB_SLOT_STREAM);

    prop1();
    prop2();
    prop3();
    prop4();
    prop5();
    prop6();
    prop7();

    free(g.scratch);
    for (i = 0; i < VB_SLOTS; i++)
        free(g_stream[i]);
    free(g_snap);
    free(g_expect);
    free(g_map_arena);
    free(g_guest);

    printf("\n");
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_ring_fault_gate: %d of %d properties failed (%d of %d checks "
                        "failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_ring_fault_gate: %d properties, %d checks -- a failed wait writes nothing "
           "into the pending slot, a failed begin is not recorded into and hands out no freed "
           "slice, and every later recording goes to the discard buffer\n", g_props, g_checks);
    return 0;
}
