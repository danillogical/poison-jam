/* N4.2's coherence boundaries, as a deciding regression on their CALLERS: a publication that
 * fails must not advance the frame, must not mark anything stale, and must not let the next
 * load or texture read the guest bytes that publication never updated.
 *
 * The defect this case exists for
 * -------------------------------
 * vb_targets_flush could not publish a batch (its pack descriptor set could not be allocated,
 * or the staging pair could not be made) and reported that only by leaving its targets dirty.
 * The callers did not consult it:
 *
 *   - vb_flip submitted, drained, marked EVERY used target stale and advanced g.frame, so the
 *     next vb_target_load uploaded the OLD guest bytes over the target whose newer pixels were
 *     never published -- the newer image was silently discarded;
 *   - vb_target_for flushed and then released the target unconditionally on a reshape or an
 *     LRU eviction, so the newer image was destroyed without ever reaching guest memory;
 *   - vb_texture's overlapping-guest-texture loop flushed and carried on, so a texture was
 *     decoded and cached from guest bytes a live target had already superseded.
 *
 * This case drives exactly those four callers -- vb_flip, vb_target_load, vb_target_for,
 * vb_texture -- and observes g.frame, g.ok, g.failed, the targets' dirty/stale flags, the
 * Nv2aVkReadbackStats counters, the recorded command buffers and the guest bytes. The
 * publications it injects are STUBBED DRIVER OUTCOMES: vkAllocateDescriptorSets returns
 * VK_ERROR_OUT_OF_POOL_MEMORY and vkCreateBuffer returns VK_ERROR_OUT_OF_DEVICE_MEMORY, from
 * this fixture's own stubs. Neither is a real GPU OOM.
 *
 * The production bodies are the toolkit's own
 * ------------------------------------------
 * This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND
 * defined, so every function driven here -- the four callers above, and behind them
 * vb_targets_flush, vb_readback_reserve, vb_target_release, vb_target_create, vb_tex_upload,
 * vb_slot_submit, vb_drain -- is compiled from the production source, verbatim. Nothing is
 * copied, so nothing can drift: the build recompiles these bodies whenever that file changes,
 * and scripts/vk-readback-caller-gate.py probes the same bytes at build time and FAILS THE
 * BUILD (exit 2) if a definition, a struct field or a global the fixture binds to is gone.
 *
 * Nothing of the toolkit is linked: the file's five undefined names are defined at the bottom of
 * this TU (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack), and every Vulkan entry point the driven paths can
 * reach is a static function pointer (VK_NO_PROTOTYPES) that this TU points at a controlled stub
 * before the first call.
 *
 * Built against BOTH sources, on purpose
 * --------------------------------------
 * This case must compile and run against the repaired back end AND against the pre-fix one
 * (git 891bca2:src/video/nv2a_vk_backend.c), because the second is what reports the defect. It
 * therefore names none of the new seam -- no vb_stop, no vb_targets_flush_recover, no
 * vb_stopped, no flush return value, no `stopped` -- and reads nothing a caller could not read
 * before the repair. The new seam appears only in the markers the probe prints.
 *
 * The properties
 * --------------
 *   0. the source under test     -- the file compiled here (printed with its sha256), the
 *      seam markers, and the fixture's constants against the source's own bounds.
 *   1. the success control       -- one vb_flip() of two dirty targets (16-bit and 4-byte,
 *      padded pitches and odd rectangles) publishes both rectangles exactly, marks both clean
 *      and stale, advances g.frame by one, stops nothing, counts no incomplete target; the next
 *      vb_target_load then uploads the NEWLY published bytes, never the pre-publication ones.
 *      The control passing is the sanity anchor that separates "the defect" from "the fixture".
 *   2. permanent descriptor failure through vb_flip() -- the allocation is attempted twice (the
 *      bounded retry really happens), then the back end stops: g.frame unchanged, both targets
 *      still dirty and NOT stale, g.ok 0, g.failed 1, guest memory byte-identical, every
 *      publication counter unchanged, both targets counted incomplete, and every image handle
 *      and dirty rectangle intact (nothing released).
 *   3. transient descriptor failure through vb_flip() -- one publication of the exact newer
 *      bytes: clean + stale, frame advanced, no stop, nothing incomplete.
 *   4. permanent staging failure through vb_flip() -- the same outcome as 2, from
 *      vb_readback_reserve's pair instead of the pack set.
 *   5. transient staging failure through vb_flip() -- the same outcome as 3.
 *   6. vb_target_for, same address and a new shape, permanent -- NULL, and the target is NOT
 *      released: image handle alive, dirty rectangle intact, nothing destroyed.
 *   7. vb_target_for, same address and a new shape, transient -- published exactly once, then
 *      replaced: the guest holds the newer image, the old image was released and a new one made.
 *   8. vb_target_for, LRU eviction, permanent and transient -- the same two outcomes for the
 *      victim, which is a different address from the one being looked up.
 *   9. vb_texture over a dirty target, permanent -- VK_NULL_HANDLE and no guest-byte decode at
 *      all (the fixture's decoder counts its calls and records the bytes it was shown), so no
 *      texture image can be made from superseded guest bytes.
 *  10. vb_texture over a dirty target, transient -- published first, then a live view: the
 *      decoder was shown the published bytes, never the pre-publication ones.
 *  11. after the stop               -- a second vb_flip() publishes nothing, does not advance
 *      the frame and marks nothing stale; vb_target_load records no vkCmdCopyBufferToImage,
 *      moves no stream bytes and counts no rt_load even though the target's newer image was
 *      never published.
 *  12. the capacity seam, split      -- two dirty 4-byte targets whose slices together exceed
 *      VB_READBACK_MAX (64 MiB each, different shapes with the same slice size) make ONE flip
 *      become TWO batches: two recordings, two submits, two waits, two invalidates, two
 *      publications, and both rectangles exact row by row with their pitch padding and every
 *      byte outside them untouched. The cap splits the group, it does not fail it, and the
 *      second batch's slice fits the same staging pair the first one grew to 64 MiB -- one
 *      vkCreateBuffer pair, one vkMapMemory, one destination buffer across both batches. The
 *      first batch's publication precedes the second batch's recording, which the fixture
 *      decides by the order of the calls it sees (the first invalidate, whose host memcpy
 *      follows it, before the second recording's begin), not by guessing.
 *  12b. the capacity seam, split and half-published -- the same two-batch split as 12, with the
 *      SECOND batch's pack set unable to allocate: the first batch's rectangle lands exactly, the
 *      second target stays dirty, and the boundary counts it as left unpublished and stops -- no
 *      frame advance, nothing marked stale, nothing released. This is the half of the seam the
 *      caller-propagation mutants decide.
 *  13. the capacity seam, over-cap   -- a synthetic dirty target whose rectangle alone (256 MiB)
 *      exceeds VB_READBACK_MAX, beside a normal dirty one: the flip publishes the normal target
 *      exactly, leaves the over-cap one dirty with its image, address and rectangle intact,
 *      counts it as incomplete and STOPS. The over-cap branch is a stop -- never a silent skip
 *      and never a partial copy -- and the target is wired by hand because vb_target_for's
 *      4096-per-dimension cap could not produce that shape, with an address outside the
 *      fixture's guest arena so that "never published" is also "unreachable".
 *
 * Properties 12, 12b and 13 need bigger arenas than the rest: guest memory is 160 MiB (two 64 MiB
 * rectangles, their pitch padding, and room to spare) and the staging arena 96 MiB, which is the
 * most any single batch can ask for (VB_READBACK_MAX).
 *
 * Every property prints its own FAIL line with the values that decided it. There is no skip
 * mode and no SKIP_RETURN_CODE: exit 0 or 1 only.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */

#include "vk_readback_caller_gate_seam.h"       /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture ─────────────────────────────────────────────────────────────────────── */

/* The fixture's three arenas. Guest memory and the staging arena are sized for the two
 * capacity-seam properties (12 and 13): a 4-byte render target's dirty rectangle is at most
 * VB_READBACK_MAX (96 MiB) per batch, and two of them (64 MiB each) must both fit, disjoint, in
 * guest memory -- 160 MiB holds the pair plus their pitch padding -- while the staging arena only
 * ever has to hold ONE batch's slice, which is bounded by the same 96 MiB cap. */
#define GATE_GUEST   (160u << 20)               /* the stand-in for guest RAM */
#define GATE_STAGE   (96u << 20)                /* what vkMapMemory hands back: the staging */
#define GATE_SEED    0x1D4C0000u
#define GATE_STALE   (GATE_SEED + 3u)           /* the pattern an unwritten staging buffer holds */
#define GATE_RB_CAP  (1u << 20)                 /* vb_readback_reserve's first capacity */
#define GATE_LOAD_WORDS (64u * 64u)             /* the largest load this fixture's targets ask for */
#define GATE_EVENTS  64                         /* entries in the fixture's call-order trace */

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
#define T16_H       14u                          /* the target's backing height, dy1 + 4 */

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
#define T4_H        10u                          /* the target's backing height, dy1 + 4 */

/* Both rectangles' byte counts equal their staging slices' -- a packed 16-bit row is exactly
 * rw*2 bytes and a 4-byte row exactly rw*4 -- which is what makes "the whole rectangle changed
 * and nothing else did" an exact statement below. */
#define GATE_PUBLISHED (T16_SLICE + T4_SLICE)

/* The overlapping texture of properties 9 and 10: its texels are the FIRST ROW of the 16-bit
 * target's dirty rectangle (its rows start at byte T16_VA + dy0*pitch + dx0*bpp, 32 bytes long),
 * so a decode of guest memory before the target is published reads bytes that publication
 * supersedes, and a decode after it reads the published ones. It is not the target's address and
 * not its pitch, so it never takes the alias branch. */
#define TX_OFFSET   (T16_VA + T16_DY0 * T16_PITCH + T16_DX0 * T16_BPP)
#define TX_SPAN     T16_STRIDE
#define TX_PITCH    TX_SPAN
#define TX_W        16u
#define TX_H        1u

/* The address vb_target_for is asked for in the eviction properties: no target's address, and
 * above the filler targets' own range (T4_VA + n*0x10000), so it really is the LRU branch that
 * is taken. */
#define EVICT_NEW_VA 0x00200000u

/* ── The capacity seam (properties 12 and 13) ───────────────────────────────────────── */

/* Property 12's two dirty targets. Each one's dirty rectangle is exactly 64 MiB (67,108,864
 * bytes = VB_READBACK_MAX * 2/3) and neither fits beside the other: the flush's group scan takes
 * a target while `total + slice <= VB_READBACK_MAX`, so 64 + 64 = 128 MiB over the 96 MiB cap
 * splits the frame into two batches of one target each. The two are different shapes with the
 * same slice size (a 4096x4096 and an 8192x2048 rectangle, 16,384- and 32,768-byte rows), so the
 * two batches' published rows are laid out differently even though each batch's slice starts
 * at the staging buffer's offset 0, with their own row layouts. The pitches carry 256 bytes of
 * padding per row, the guest
 * footprints do not overlap, and both fit the 160 MiB arena. */
#define BIG_A_VA    0x00100000u                 /* 1 MiB   .. ~66 MiB */
#define BIG_A_PITCH 16640u                      /* 4096 * 4 + 256 */
#define BIG_A_RW    4096u
#define BIG_A_RH    4096u
#define BIG_B_VA    0x04800000u                 /* 72 MiB  .. ~136 MiB */
#define BIG_B_PITCH 33024u                      /* 8192 * 4 + 256 */
#define BIG_B_RW    8192u
#define BIG_B_RH    2048u
#define BIG_BPP     4u
#define BIG_SLICE   ((VkDeviceSize)BIG_A_RW * BIG_BPP * BIG_A_RH)   /* 64 MiB, and the same for B */
#define BIG_TOTAL   (BIG_SLICE * 2u)                                /* 128 MiB, over the cap */

/* Property 12b's two 16-bit targets: 8192x3584 pixels each, 16,384-byte packed rows, 56 MiB of
 * slice each -- so their group (112 MiB) also exceeds the 96 MiB cap and splits, but each batch
 * needs a pack descriptor set, which is what lets the fixture fail the SECOND batch's publication
 * without touching the first's. The pair still only has to grow to 64 MiB (the doubling), so the
 * second batch reuses it. */
#define MID_BPP     2u
#define MID_A_VA    0x00100000u                 /* 1 MiB  .. ~57.9 MiB */
#define MID_B_VA    0x04000000u                 /* 64 MiB .. ~120.9 MiB */
#define MID_PITCH   16640u                      /* 8192 * 2 + 256 */
#define MID_RW      8192u
#define MID_RH      3584u
#define MID_H       MID_RH
#define MID_SLICE   ((VkDeviceSize)(((MID_RW * MID_BPP) + 3u) & ~3u) * MID_RH)   /* 56 MiB */

/* Property 13's synthetic target: a dirty rectangle of 8192x8192 4-byte pixels = 256 MiB, two and
 * two-thirds times VB_READBACK_MAX, so no batch can ever stage it. vb_target_for could not produce
 * it -- its 4096-per-dimension cap bounds one target at 64 MiB -- so the fixture wires it by hand,
 * which is the point: it is the capacity seam itself, not a shape the guest can ask for. Its
 * address lies OUTSIDE the fixture's guest arena on purpose: the flush reaches guest memory only
 * for a target it staged, so an address nothing can write is the strongest form of "this target
 * must never be published", and it keeps the arena at the size the two 64 MiB targets need. */
#define OVERCAP_VA    0x20000000u               /* 512 MiB, past the arena */
#define OVERCAP_PITCH 32768u                    /* = 8192 * 4, tight rows */
#define OVERCAP_DX1   8192u
#define OVERCAP_DY1   8192u
#define OVERCAP_H     8192u
#define OVERCAP_SLICE ((VkDeviceSize)OVERCAP_DX1 * BIG_BPP * OVERCAP_DY1)   /* 256 MiB */

static uint8_t *g_guest;                        /* xbox_GetMemoryOffset()'s answer */
static uint8_t *g_pre;                          /* guest memory, copied before a call */
static uint8_t *g_expect;                       /* what guest memory must hold afterwards */
static uint8_t *g_stage;                        /* what vkMapMemory hands back */
static uint8_t *g_stream[VB_SLOTS];             /* one mapped slot stream each, as install leaves them */
static uint32_t g_loaded[GATE_LOAD_WORDS];      /* what a target load's slice must hold */
static uint32_t g_loaded_before[GATE_LOAD_WORDS];/* ... and what the pre-publication bytes would give */

static uintptr_t g_handle = 0x8000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

/* Every Vulkan call the driven paths can reach, what it saw, and the three injected outcomes. */
typedef struct {
    /* the injected outcomes (all three default to success) */
    unsigned ds_allocs;              /* vkAllocateDescriptorSets */
    unsigned ds_failures;            /* ... that returned the injected failure */
    unsigned ds_fail_n;              /* the first this many calls fail; 0: none do */
    unsigned ds_fail_from;           /* every call from this one on fails; 0: none do */
    VkResult ds_result;              /* what those failures return */
    VkDescriptorPool ds_pool;        /* the pool the last allocation asked */
    unsigned buffers;                /* vkCreateBuffer */
    unsigned buffer_failures;
    unsigned buffer_fail_n;          /* the first this many calls fail; 0: none do */
    VkResult buffer_result;
    /* recordings */
    unsigned begins, ends, resets, submits, waits, fence_resets, pool_resets;
    VkResult begin_result, wait_result;
    VkCommandBuffer began, ended, reset_cb;
    VkCommandBuffer submitted[4];
    /* the publication and load commands */
    unsigned imgcopy;                /* vkCmdCopyImageToBuffer */
    VkCommandBuffer imgcopy_cb;
    VkImage imgcopy_img;
    VkBuffer imgcopy_buf;
    VkImageLayout imgcopy_layout;
    VkBufferImageCopy imgcopy_region;
    unsigned bufcopy;                /* vkCmdCopyBuffer */
    VkBuffer bufcopy_src, bufcopy_dst;
    VkBufferCopy bufcopy_region;
    unsigned copy2img;               /* vkCmdCopyBufferToImage */
    VkCommandBuffer copy2img_cb;
    VkBuffer copy2img_buf;
    VkImage copy2img_img;
    VkImageLayout copy2img_layout;
    VkBufferImageCopy copy2img_region;
    unsigned copyimg;                /* vkCmdCopyImage */
    unsigned clearcolor;             /* vkCmdClearColorImage */
    VkImage clear_img;
    unsigned dispatches;             /* vkCmdDispatch */
    VkCommandBuffer dispatch_cb;
    uint32_t dispatch_group[3];
    unsigned pipebinds, setbinds, pushes, ds_updates;
    VkPipeline bound_pipe;
    VkPipelineBindPoint bind_point;
    VbPackPush push;
    unsigned bars;                   /* vkCmdPipelineBarrier with a memory barrier */
    unsigned hostbars;               /* ... whose destination stage is HOST */
    unsigned invalidates;            /* vkInvalidateMappedMemoryRanges */
    VkMappedMemoryRange invalidate_range;
    /* images and buffers */
    unsigned images, imageviews;     /* vkCreateImage, vkCreateImageView */
    unsigned image_destroys, imageview_destroys, fb_destroys, pipe_destroys;
    unsigned buffer_destroys;        /* vkDestroyBuffer (the staging pair is never destroyed) */
    unsigned allocs, frees, maps;    /* vkAllocateMemory, vkFreeMemory, vkMapMemory */
    VkImage last_image;
    VkImageView last_view;
    /* the fixture's own readers */
    unsigned spans;                  /* nv2a_backend_texture_span calls */
    unsigned decodes;                /* nv2a_backend_decode_texture calls */
    uint32_t decoded_offset;         /* the guest address it was handed */
    uint32_t decoded_span;
    uint64_t decoded_hash;           /* the guest bytes it read there */
    /* A short trace of the calls that decide WHEN each batch's publication happened, in the
     * order the back end made them. The host's memcpy loop itself is not interceptable, but it
     * sits between the batch's vkInvalidateMappedMemoryRanges (which precedes it) and the next
     * batch's first vkBeginCommandBuffer (which follows it), so the order of those two trace
     * entries is what makes "the first batch's bytes were already in guest memory before the
     * second batch recorded" observable rather than guessed. */
    unsigned events[GATE_EVENTS];
    unsigned nevents;
    /* The staging buffer every publication command wrote into, in call order: the pair's identity
     * across batches is exactly "every one of these is the same handle". */
    VkBuffer copy_dst[8];
    unsigned ncopy_dst;
} GateCtl;

static GateCtl c;

enum { EV_BEGIN = 1, EV_END, EV_SUBMIT, EV_WAIT, EV_INVALIDATE, EV_DS_ALLOC, EV_CREATE_BUFFER,
       EV_COPY_IMG };

static void ev(unsigned code)
{
    if (c.nevents < GATE_EVENTS)
        c.events[c.nevents++] = code;
}

/* The trace's index of the `k`-th (1-based) entry with this code, or -1. */
static int ev_nth(unsigned code, unsigned k)
{
    unsigned i, seen = 0;

    for (i = 0; i < c.nevents; i++)
        if (c.events[i] == code && ++seen == k)
            return (int)i;
    return -1;
}

static VkResult VKAPI_CALL gate_begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info)
{
    (void)info;
    c.begins++;
    c.began = cmd;
    ev(EV_BEGIN);
    return c.begin_result;
}

static VkResult VKAPI_CALL gate_end(VkCommandBuffer cmd)
{
    c.ends++;
    c.ended = cmd;
    ev(EV_END);
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
    ev(EV_SUBMIT);
    if (c.submits <= 4u && count && submits[0].commandBufferCount)
        c.submitted[c.submits - 1u] = submits[0].pCommandBuffers[0];
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_wait(VkDevice device, uint32_t count, const VkFence *fences,
                                     VkBool32 all, uint64_t timeout)
{
    (void)device; (void)count; (void)fences; (void)all; (void)timeout;
    c.waits++;
    ev(EV_WAIT);
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

/* The first injected outcome: the batch's pack descriptor set cannot be allocated. */
static VkResult VKAPI_CALL gate_ds_alloc(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                         VkDescriptorSet *set)
{
    (void)device;
    c.ds_allocs++;
    ev(EV_DS_ALLOC);
    c.ds_pool = info ? info->descriptorPool : VK_NULL_HANDLE;
    if ((c.ds_fail_n && c.ds_allocs <= c.ds_fail_n)
        || (c.ds_fail_from && c.ds_allocs >= c.ds_fail_from)) {
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
    c.ds_updates++;
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
    ev(EV_COPY_IMG);
    if (c.ncopy_dst < 8u)
        c.copy_dst[c.ncopy_dst++] = dst;
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
    if (c.ncopy_dst < 8u)
        c.copy_dst[c.ncopy_dst++] = dst;
    c.bufcopy_src = src;
    c.bufcopy_dst = dst;
    if (count)
        c.bufcopy_region = regions[0];
}

/* The command a target load records: the guest pixels' stream slice into the target's image. */
static void VKAPI_CALL gate_copy_buffer_to_image(VkCommandBuffer cmd, VkBuffer src, VkImage dst,
                                                 VkImageLayout layout, uint32_t count,
                                                 const VkBufferImageCopy *regions)
{
    c.copy2img++;
    c.copy2img_cb = cmd;
    c.copy2img_buf = src;
    c.copy2img_img = dst;
    c.copy2img_layout = layout;
    if (count)
        c.copy2img_region = regions[0];
}

static void VKAPI_CALL gate_copy_image(VkCommandBuffer cmd, VkImage src, VkImageLayout src_layout,
                                       VkImage dst, VkImageLayout dst_layout, uint32_t count,
                                       const VkImageCopy *regions)
{
    (void)cmd; (void)src; (void)src_layout; (void)dst; (void)dst_layout; (void)count;
    (void)regions;
    c.copyimg++;
}

static void VKAPI_CALL gate_clear_color(VkCommandBuffer cmd, VkImage img, VkImageLayout layout,
                                        const VkClearColorValue *value, uint32_t count,
                                        const VkImageSubresourceRange *ranges)
{
    (void)layout; (void)value; (void)count; (void)ranges;
    c.clearcolor++;
    c.clear_img = img;
    (void)cmd;
}

static void VKAPI_CALL gate_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
                                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                                    uint32_t mbc, const VkMemoryBarrier *mb, uint32_t bbc,
                                    const VkBufferMemoryBarrier *bb, uint32_t ibc,
                                    const VkImageMemoryBarrier *ib)
{
    (void)cmd; (void)flags; (void)bb;
    if (ibc && ib)
        return;                                 /* this fixture commands no image barriers */
    if (!(mbc && mb))
        return;
    c.bars++;
    if (dst & VK_PIPELINE_STAGE_HOST_BIT)
        c.hostbars++;
    (void)src;
}

static VkResult VKAPI_CALL gate_invalidate(VkDevice device, uint32_t count,
                                           const VkMappedMemoryRange *ranges)
{
    (void)device;
    c.invalidates++;
    ev(EV_INVALIDATE);
    if (count)
        c.invalidate_range = ranges[0];
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_end_pass(VkCommandBuffer cmd)
{
    (void)cmd;
}

/* The second injected outcome: the staging pair's vkCreateBuffer cannot be made. */
static VkResult VKAPI_CALL gate_create_buffer(VkDevice device, const VkBufferCreateInfo *info,
                                              const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)info; (void)alloc;
    c.buffers++;
    ev(EV_CREATE_BUFFER);
    if (c.buffer_fail_n && c.buffers <= c.buffer_fail_n) {
        c.buffer_failures++;
        *buf = VK_NULL_HANDLE;
        return c.buffer_result;
    }
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

static VkResult VKAPI_CALL gate_create_image(VkDevice device, const VkImageCreateInfo *info,
                                             const VkAllocationCallbacks *alloc, VkImage *img)
{
    (void)device; (void)info; (void)alloc;
    c.images++;
    *img = (VkImage)hnew();
    c.last_image = *img;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_image_requirements(VkDevice device, VkImage img,
                                               VkMemoryRequirements *req)
{
    (void)device; (void)img;
    req->size = 4096u;
    req->alignment = 16u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_bind_image(VkDevice device, VkImage img, VkDeviceMemory mem,
                                           VkDeviceSize offset)
{
    (void)device; (void)img; (void)mem; (void)offset;
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_image_view(VkDevice device,
                                                  const VkImageViewCreateInfo *info,
                                                  const VkAllocationCallbacks *alloc,
                                                  VkImageView *view)
{
    (void)device; (void)info; (void)alloc;
    c.imageviews++;
    *view = (VkImageView)hnew();
    c.last_view = *view;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_destroy_image(VkDevice device, VkImage img,
                                          const VkAllocationCallbacks *alloc)
{
    (void)device; (void)img; (void)alloc;
    c.image_destroys++;
}

static void VKAPI_CALL gate_destroy_image_view(VkDevice device, VkImageView view,
                                               const VkAllocationCallbacks *alloc)
{
    (void)device; (void)view; (void)alloc;
    c.imageview_destroys++;
}

static void VKAPI_CALL gate_destroy_framebuffer(VkDevice device, VkFramebuffer fb,
                                                const VkAllocationCallbacks *alloc)
{
    (void)device; (void)fb; (void)alloc;
    c.fb_destroys++;
}

static void VKAPI_CALL gate_destroy_pipeline(VkDevice device, VkPipeline pipe,
                                             const VkAllocationCallbacks *alloc)
{
    (void)device; (void)pipe; (void)alloc;
    c.pipe_destroys++;
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
    vkCmdCopyBufferToImage = gate_copy_buffer_to_image;
    vkCmdCopyImage = gate_copy_image;
    vkCmdClearColorImage = gate_clear_color;
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
    vkCreateImage = gate_create_image;
    vkGetImageMemoryRequirements = gate_image_requirements;
    vkBindImageMemory = gate_bind_image;
    vkCreateImageView = gate_create_image_view;
    vkDestroyImage = gate_destroy_image;
    vkDestroyImageView = gate_destroy_image_view;
    vkDestroyFramebuffer = gate_destroy_framebuffer;
    vkDestroyPipeline = gate_destroy_pipeline;
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
    printf("  --> %-56s %s (%d checks, %d failed)\n", prop, bad ? "FAIL" : "PASS",
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

/* The boundary's own "a dirty target had to be left unpublished" count. Guarded, because the
 * counter is part of the repair's interface: a header without it is reported as a failed check
 * by property 0 rather than breaking the build of the pre-fix source this case is run against. */
static uint64_t stats_incomplete(void)
{
#if VB_RB_CALLER_HAS_INCOMPLETE
    Nv2aVkReadbackStats st;

    stats(&st);
    return st.incomplete;
#else
    return 0;
#endif
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

/* Properties 12 and 13 use their own marker family for the big rectangles: every byte of a
 * 4096x4096 (0x1x) and an 8192x2048 (0x2x) rectangle differs from the 0x5x staging slice the
 * publication copies in, so "the whole rectangle changed and nothing outside it did" stays an
 * exact count at that size too rather than a statistical one. */
static void guest_marker_rect(uint32_t va, uint32_t pitch, uint32_t rw, uint32_t rh,
                              uint32_t bpp, uint32_t base)
{
    uint32_t r, x;

    for (r = 0; r < rh; r++)
        for (x = 0; x < rw * bpp; x++)
            g_guest[va + (size_t)r * pitch + x] = (uint8_t)(base + (x & 0x0Fu));
}

static void guest_big_markers(void)
{
    guest_marker_rect(BIG_A_VA, BIG_A_PITCH, BIG_A_RW, BIG_A_RH, BIG_BPP, 0x10u);
    guest_marker_rect(BIG_B_VA, BIG_B_PITCH, BIG_B_RW, BIG_B_RH, BIG_BPP, 0x20u);
}

/* The whole back end's state as install/first use leaves it, with the ring idle and nothing
 * injected. */
static void gate_world(void)
{
    unsigned i;

    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    c.begin_result = VK_SUCCESS;
    c.wait_result = VK_SUCCESS;
    c.ds_result = VK_ERROR_OUT_OF_POOL_MEMORY;
    c.buffer_result = VK_ERROR_OUT_OF_DEVICE_MEMORY;

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

/* A target as the draw paths leave it: used, dirty over its own rectangle, and backed by an
 * image whose handles the properties can recognise after the call. */
static void wire_target(VbTarget *t, uint32_t va, uint32_t pitch, uint32_t bpp,
                        uint32_t dx0, uint32_t dy0, uint32_t dx1, uint32_t dy1, uint32_t h)
{
    memset(t, 0, sizeof *t);
    t->used = 1;
    t->va = va;
    t->pitch = pitch;
    t->bpp = bpp;
    t->w = pitch / bpp;
    t->h = h;
    t->dirty = 1;
    t->dx0 = dx0;
    t->dy0 = dy0;
    t->dx1 = dx1;
    t->dy1 = dy1;
    t->im.img = (VkImage)hnew();
    t->im.mem = (VkDeviceMemory)hnew();
    t->im.view = (VkImageView)hnew();
    t->last_use = 1;
}

/* The two-target world of the flip properties, in the order they sit in g.targets[]: the
 * 4-byte target first, so a failure at the 16-bit target's pack set is reached with a copy
 * already recorded. The slice offsets are the production's own: the first staged target's slice
 * is at 0, the second's after it. */
typedef struct {
    VbTarget *ts[2];
    VbTarget *t4;
    VbTarget *t16;
    VkDeviceSize off4, off16;
    unsigned n;
} FlipWorld;

static void flip_world(FlipWorld *w)
{
    gate_world();
    wire_target(&g.targets[0], T4_VA, T4_PITCH, T4_BPP, T4_DX0, T4_DY0, T4_DX1, T4_DY1, T4_H);
    wire_target(&g.targets[1], T16_VA, T16_PITCH, T16_BPP, T16_DX0, T16_DY0, T16_DX1, T16_DY1,
                T16_H);
    w->t4 = &g.targets[0];
    w->t16 = &g.targets[1];
    w->ts[0] = w->t4;
    w->ts[1] = w->t16;
    w->off4 = 0;
    w->off16 = T4_SLICE;
    w->n = 2;
}

/* The GPU's half of a batch, as the fixture supplies it: what the copy engine writes for a
 * 4-byte target and what cs_pack's dispatch writes for a 16-bit one. The bases are arguments so
 * a property that publishes twice can stage different bytes the second time. */
static void stage_target(const VbTarget *t, VkDeviceSize off, uint32_t base)
{
    uint32_t rw = t->dx1 - t->dx0;
    uint32_t stride = t->bpp == 2 ? ((rw * 2u + 3u) & ~3u) : rw * 4u;

    fill_stage_slice(g_stage + off, (size_t)stride * (t->dy1 - t->dy0), base);
}

static void stage_gpu(const FlipWorld *w)
{
    stage_target(w->t4, w->off4, 0x60u);
    stage_target(w->t16, w->off16, 0x50u);
}

/* ── The shared property helpers ────────────────────────────────────────────────────── */

/* What guest memory must hold after a successful publication: the pre-call bytes, with each
 * staged rectangle replaced by its slice's rows. */
static void expect_guest(VbTarget *const *ts, const VkDeviceSize *off, unsigned n)
{
    unsigned k, row;

    memcpy(g_expect, g_pre, GATE_GUEST);
    for (k = 0; k < n; k++) {
        const VbTarget *t = ts[k];
        uint32_t rw = t->dx1 - t->dx0;
        uint32_t stride = t->bpp == 2 ? ((rw * 2u + 3u) & ~3u) : rw * 4u;

        for (row = 0; row < t->dy1 - t->dy0; row++)
            memcpy(g_expect + t->va + (size_t)(t->dy0 + row) * t->pitch
                                    + (size_t)t->dx0 * t->bpp,
                   g_stage + off[k] + (VkDeviceSize)row * stride, (size_t)rw * t->bpp);
    }
}

/* What vb_target_load must have written into a slot stream for `t`: A8R8G8B8 copied byte for
 * byte, or 5:6:5 expanded by shifting, exactly as the production does it. */
static void expect_load(const VbTarget *t, const uint8_t *guest, uint8_t *out)
{
    uint32_t y, x;

    for (y = 0; y < t->h; y++) {
        const uint8_t *row = guest + (size_t)t->va + (size_t)y * t->pitch;
        uint8_t *o = out + (size_t)y * t->w * 4;

        if (t->bpp == 4) {
            memcpy(o, row, (size_t)t->w * 4);
        } else {
            const uint16_t *r16 = (const uint16_t *)row;
            for (x = 0; x < t->w; x++) {
                uint32_t v = r16[x];
                uint32_t w = 0xFF000000u | ((v & 0xF800u) << 8) | ((v & 0x07E0u) << 5)
                           | ((v & 0x001Fu) << 3);
                memcpy(o + (size_t)x * 4, &w, 4);
            }
        }
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

static size_t rect_bytes(const VbTarget *t)
{
    return (size_t)(t->dx1 - t->dx0) * t->bpp * (t->dy1 - t->dy0);
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

/* Guest memory: exactly the published rectangles (and the caller's own changes), or exactly
 * what it held before the call. */
static void check_guest_published(const char *tag, VbTarget *const *ts, const VkDeviceSize *off,
                                  unsigned n)
{
    size_t total, inside = 0, outside, wrong;
    size_t want_in = 0;
    unsigned k;
    char what[110];

    expect_guest(ts, off, n);
    for (k = 0; k < n; k++)
        want_in += rect_bytes(ts[k]);
    total = count_diff(g_pre, g_guest, GATE_GUEST);
    for (k = 0; k < n; k++)
        inside += diff_inside(g_pre, g_guest, ts[k]);
    outside = total - inside;
    wrong = count_diff(g_expect, g_guest, GATE_GUEST);

    snprintf(what, sizeof what, "%s guest memory holds exactly the published rectangles", tag);
    check(wrong == 0, what, "%zu of %u bytes differ from the expected image "
                            "(guest checksum 0x%016llX, expected 0x%016llX)", wrong,
          (unsigned)GATE_GUEST, (unsigned long long)hash_bytes(g_guest, GATE_GUEST),
          (unsigned long long)hash_bytes(g_expect, GATE_GUEST));
    if (wrong) {
        note_diffs("guest vs expected", g_expect, g_guest);
        note_diffs("guest vs pre-call", g_pre, g_guest);
    }
    snprintf(what, sizeof what, "%s every staged rectangle was overwritten from its slice", tag);
    check(inside == want_in, what, "%zu of %zu bytes inside the rectangles changed", inside,
          want_in);
    snprintf(what, sizeof what, "%s no byte outside the rectangles was written", tag);
    check(outside == 0, what, "%zu of %u bytes differ outside (%zu of %u in total)",
          outside, (unsigned)GATE_GUEST, total, (unsigned)GATE_GUEST);
}

static void check_guest_unchanged(const char *tag, const char *why)
{
    size_t total = count_diff(g_pre, g_guest, GATE_GUEST);
    char what[110];

    snprintf(what, sizeof what, "%s guest memory is byte-identical: %s", tag, why);
    check(total == 0, what, "checksum 0x%016llX -> 0x%016llX, %zu of %u bytes differ",
          (unsigned long long)hash_bytes(g_pre, GATE_GUEST),
          (unsigned long long)hash_bytes(g_guest, GATE_GUEST), total, (unsigned)GATE_GUEST);
    if (total) {
        note_diffs("guest vs pre-call", g_pre, g_guest);
        note_diffs("guest vs expected", g_expect, g_guest);
    }
}

/* What a call must have published, as deltas of the counters nv2a_vk_backend_readback_stats
 * reports. */
typedef struct {
    uint64_t readbacks, batches, packed, packed_pixels, copied, bytes;
} RbWant;

static void want_published_two(RbWant *w)
{
    memset(w, 0, sizeof *w);
    w->readbacks = 2;
    w->batches = 1;
    w->packed = 1;
    w->packed_pixels = (uint64_t)T16_RW * T16_RH;
    w->copied = 1;
    w->bytes = (uint64_t)GATE_PUBLISHED;
}

static void want_published_one_16(RbWant *w)
{
    memset(w, 0, sizeof *w);            /* one 16-bit target: no 4-byte copy at all */
    w->readbacks = 1;
    w->batches = 1;
    w->packed = 1;
    w->packed_pixels = (uint64_t)T16_RW * T16_RH;
    w->bytes = (uint64_t)T16_SLICE;
}

static void want_nothing(RbWant *w)
{
    memset(w, 0, sizeof *w);
}

/* Every publication counter, checked one by one so a failure names the one that moved. The two
 * time counters are clocks: their direction is all that is asserted, except where the property
 * is "nothing was published at all", where they must not move either. */
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
    check(a->packed_pixels == b->packed_pixels + w->packed_pixels, what,
          "%llu -> %llu (wanted +%llu)", (unsigned long long)b->packed_pixels,
          (unsigned long long)a->packed_pixels, (unsigned long long)w->packed_pixels);
    snprintf(what, sizeof what, "%s 4-byte targets copied out of their image", tag);
    check(a->copied == b->copied + w->copied, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->copied, (unsigned long long)a->copied,
          (unsigned long long)w->copied);
    snprintf(what, sizeof what, "%s bytes written into guest memory", tag);
    check(a->bytes == b->bytes + w->bytes, what, "%llu -> %llu (wanted +%llu)",
          (unsigned long long)b->bytes, (unsigned long long)a->bytes,
          (unsigned long long)w->bytes);
    if (exact_times) {
        snprintf(what, sizeof what, "%s the three readback clocks did not move either", tag);
        check(a->wait_ns == b->wait_ns && a->publish_ns == b->publish_ns, what,
              "wait_ns %llu -> %llu, publish_ns %llu -> %llu",
              (unsigned long long)b->wait_ns, (unsigned long long)a->wait_ns,
              (unsigned long long)b->publish_ns, (unsigned long long)a->publish_ns);
    } else {
        snprintf(what, sizeof what, "%s the readback clocks only advanced (clocks: direction)", tag);
        check(a->wait_ns >= b->wait_ns && a->publish_ns >= b->publish_ns, what,
              "wait_ns %llu -> %llu, publish_ns %llu -> %llu",
              (unsigned long long)b->wait_ns, (unsigned long long)a->wait_ns,
              (unsigned long long)b->publish_ns, (unsigned long long)a->publish_ns);
    }
}

/* The boundary's own "left unpublished" count, which is part of the repair's interface. */
static void check_incomplete(const char *tag, uint64_t before, uint64_t after, uint64_t want)
{
    char what[110];

    snprintf(what, sizeof what, "%s the left-unpublished count is +%llu", tag,
             (unsigned long long)want);
#if VB_RB_CALLER_HAS_INCOMPLETE
    check(after == before + want, what, "readback_incomplete %llu -> %llu",
          (unsigned long long)before, (unsigned long long)after);
#else
    (void)before; (void)after;
    check(0, what, "the readback stats header has no `incomplete` member to report it with");
#endif
}

/* The health of the back end: a stopped one is the same pair of values vb_stop latches. */
static void check_health(const char *tag, int want_ok)
{
    char what[110];

    snprintf(what, sizeof what, "%s the back end %s", tag,
             want_ok ? "is still publishing (g.ok 1, g.failed 0)"
                     : "has stopped publishing (g.ok 0, g.failed 1)");
    check(want_ok ? (g.ok == 1 && g.failed == 0) : (g.ok == 0 && g.failed == 1), what,
          "g.ok %d, g.failed %d, g.fails %u", g.ok, g.failed, g.fails);
}

static void check_dirty_stale(const char *tag, const char *label, const VbTarget *t, int dirty,
                              int stale)
{
    char what[110];

    snprintf(what, sizeof what, "%s the %s target is %s and %s", tag, label,
             dirty ? "still dirty" : "clean", stale ? "stale" : "NOT stale");
    check(t->dirty == dirty && t->stale == stale, what, "dirty %d (wanted %d), stale %d "
                                                        "(wanted %d)", t->dirty, dirty, t->stale,
          stale);
}

/* ── The two load-side properties, which are what a stale/not-stale flip decides ─────── */

/* vb_target_load on `t`, with the stream slice it must move. `want_load` 0 is the property
 * "the stopped back end reloads nothing": no command, no stream byte, no rt_load. */
static void load_probe(const char *tag, VbTarget *t, int want_load)
{
    unsigned slot = g.cur;
    VkDeviceSize off = (g.slots[slot].stream_off + (VB_STREAM_ALIGN - 1u))
                     & ~(VkDeviceSize)(VB_STREAM_ALIGN - 1u);
    size_t bytes = (size_t)t->w * t->h * 4;
    unsigned copy0 = c.copy2img;
    uint64_t loads0 = g.st.rt_loads;
    uint64_t stream0 = hash_bytes(g_stream[slot], (size_t)g.slots[slot].stream_off);
    int stale0 = t->stale;
    char what[110];

    if (bytes / 4u > GATE_LOAD_WORDS) {
        check(0, "the fixture's load scratch is big enough", "%zu words needed, %u available",
              bytes / 4u, (unsigned)GATE_LOAD_WORDS);
        return;
    }
    expect_load(t, g_guest, (uint8_t *)g_loaded);
    expect_load(t, g_pre, (uint8_t *)g_loaded_before);

    snprintf(what, sizeof what, "%s (a %ux%u %u-byte target, slice at %llu)",
             tag, t->w, t->h, t->bpp, (unsigned long long)off);

    if (!want_load) {
        /* Nothing may move: the target's newer image was never published, so reloading the
         * older guest bytes would replace it with something older than what the GPU holds. The
         * decide is the flag BEFORE the call: a flip that marked the target stale is exactly
         * what makes the next load read those older bytes. */
        check(stale0 == 0, "      the target was NOT stale when the load was called",
              "stale %d before the call (a flip that published it would have set it)", stale0);
        vb_target_load(t);
        check(c.copy2img == copy0, "      the stopped load recorded no vkCmdCopyBufferToImage",
              "vkCmdCopyBufferToImage %u -> %u", copy0, c.copy2img);
        check(g.st.rt_loads == loads0, "      the stopped load counted no rt_load",
              "rt_loads %llu -> %llu", (unsigned long long)loads0,
              (unsigned long long)g.st.rt_loads);
        check(g.cur == slot && g.slots[slot].stream_off == off
              && hash_bytes(g_stream[slot], (size_t)g.slots[slot].stream_off) == stream0,
              "      the stopped load moved no stream byte",
              "stream_off %llu (seen %llu), slot stream checksum 0x%016llX -> 0x%016llX",
              (unsigned long long)g.slots[slot].stream_off, (unsigned long long)off,
              (unsigned long long)stream0,
              (unsigned long long)hash_bytes(g_stream[slot], (size_t)g.slots[slot].stream_off));
        (void)what;
        return;
    }

    vb_target_load(t);
    check(g.cur == slot && g.slots[slot].stream_off == off + (VkDeviceSize)bytes,
          "      the load moved exactly its own slice of the current slot's stream",
          "stream_off %llu -> %llu (wanted %llu), slot %u -> %u",
          (unsigned long long)off, (unsigned long long)g.slots[slot].stream_off,
          (unsigned long long)(off + (VkDeviceSize)bytes), slot, g.cur);
    check(c.copy2img == copy0 + 1u && c.copy2img_img == t->im.img
          && c.copy2img_buf == g.slots[slot].stream
          && c.copy2img_region.bufferOffset == off,
          "      the load recorded one vkCmdCopyBufferToImage from that slice",
          "copies %u -> %u, image %p (target %p), buffer %p (stream %p), offset %llu (wanted %llu)",
          copy0, c.copy2img, (void *)c.copy2img_img, (void *)t->im.img,
          (void *)c.copy2img_buf, (void *)g.slots[slot].stream,
          (unsigned long long)c.copy2img_region.bufferOffset, (unsigned long long)off);
    check(g.st.rt_loads == loads0 + 1u, "      the load counted exactly one rt_load",
          "rt_loads %llu -> %llu", (unsigned long long)loads0,
          (unsigned long long)g.st.rt_loads);
    check(t->stale == 0, "      the target is no longer stale", "stale %d", t->stale);

    /* The bytes themselves: the expansion of what guest memory holds NOW. */
    {
        size_t wrong = count_diff((const uint8_t *)g_loaded, g_stream[slot] + off, bytes);
        size_t old = count_diff((const uint8_t *)g_loaded_before, g_stream[slot] + off, bytes);

        check(wrong == 0, "      the load faithfully copied the guest bytes as they now stand",
              "%zu of %zu bytes differ (loaded checksum 0x%016llX, from guest 0x%016llX)", wrong,
              bytes, (unsigned long long)hash_bytes(g_stream[slot] + off, bytes),
              (unsigned long long)hash_bytes((const uint8_t *)g_loaded, bytes));
        check(old > 0, "      and those guest bytes are NOT the pre-publication image",
              "%zu of %zu bytes differ from what the older guest bytes would give "
              "(old-image checksum 0x%016llX)", old, bytes,
              (unsigned long long)hash_bytes((const uint8_t *)g_loaded_before, bytes));
    }
}

/* ── 0. the source under test, and the seam this case binds to ─────────────────────── */

static void prop0(void)
{
    Nv2aVkReadbackStats st;
    int chk = g_checks, bad = g_check_failures;

    section("0. the source under test, probed from the same bytes at build time");
    printf("      production bodies compiled from: %s\n", VB_RB_CALLER_SOURCE);
    printf("      sha256 %s\n", VB_RB_CALLER_SOURCE_SHA256);
    printf("      %u bytes\n", (unsigned)VB_RB_CALLER_SOURCE_BYTES);
    printf("      interface header: %s\n", VB_RB_CALLER_BACKEND_HEADER);
    note("every caller driven below -- vb_flip, vb_target_load, vb_target_for, vb_texture -- is "
         "that file's own, compiled into this translation unit by #include. This case is built "
         "against the pre-fix source too, so it names none of the repair's new seam; the markers "
         "are printed, never asserted, and the properties are what decide.");

    marker("the flush reports its outcome (static int vb_targets_flush)", VB_RB_CALLER_FLUSH_OUTCOME);
    marker("the boundary entry point and its ring refresh (flush_recover/readback_recover)",
           VB_RB_CALLER_HAS_RECOVER);
    marker("the coherence stop (static void vb_stop)", VB_RB_CALLER_HAS_STOP);
    marker("the left-unpublished counter (g.st.readback_incomplete)",
           VB_RB_CALLER_COUNTS_INCOMPLETE);
    marker("the stop query (static int vb_stopped)", VB_RB_CALLER_HAS_STOPPED);
    marker("the back end's table carries the stop query", VB_RB_CALLER_REGISTERS_STOPPED);
    marker("vb_flip publishes (and stops) before it marks anything stale",
           VB_RB_CALLER_FLIP_PUBLISHES_FIRST);
    marker("... and stops before it marks anything stale", VB_RB_CALLER_FLIP_STOPS_BEFORE_STALE);
    marker("vb_target_for publishes before it releases",
           VB_RB_CALLER_TARGET_FOR_PUBLISHES_BEFORE_RELEASE);
    marker("... and stops when that publication fails", VB_RB_CALLER_TARGET_FOR_STOPS);
    marker("vb_texture's overlap loop publishes through the boundary entry point",
           VB_RB_CALLER_TEXTURE_OVERLAP_PUBLISHES);
    marker("... and stops when that publication fails", VB_RB_CALLER_TEXTURE_OVERLAP_STOPS);
    marker("the readback stats carry the left-unpublished count",
           VB_RB_CALLER_HAS_INCOMPLETE);

    check(VB_READBACK_MAX == (96u << 20) && VB_MAX_TARGETS >= 2u && VB_SLOTS == 3u
          && VB_MAX_DIM >= 4096u && VB_MAX_TEXTURES >= 1u,
          "the fixture's targets, ring and textures fit the source's own bounds",
          "VB_READBACK_MAX %llu, VB_MAX_TARGETS %u, VB_SLOTS %u, VB_MAX_DIM %u, "
          "VB_MAX_TEXTURES %u", (unsigned long long)VB_READBACK_MAX, (unsigned)VB_MAX_TARGETS,
          (unsigned)VB_SLOTS, (unsigned)VB_MAX_DIM, (unsigned)VB_MAX_TEXTURES);
    check((uint64_t)GATE_PUBLISHED <= VB_READBACK_MAX
          && (uint64_t)T16_SLICE <= VB_READBACK_MAX && (uint64_t)T4_SLICE <= VB_READBACK_MAX,
          "the fixture's slices are inside the staging cap, so one batch carries both",
          "16-bit %llu, 4-byte %llu, cap %llu", (unsigned long long)T16_SLICE,
          (unsigned long long)T4_SLICE, (unsigned long long)VB_READBACK_MAX);
    check(T16_RW * T16_BPP == T16_STRIDE && T4_RW * T4_BPP == T4_STRIDE
          && T16_RW * T16_BPP == (uint32_t)TX_SPAN,
          "the rectangles are byte-exact slices, and the overlapping texture is one row of one",
          "16-bit rw*bpp %u = stride %u, 4-byte %u = %u, texture span %u",
          (unsigned)T16_RW * T16_BPP, (unsigned)T16_STRIDE, (unsigned)T4_RW * T4_BPP,
          (unsigned)T4_STRIDE, (unsigned)TX_SPAN);
    check(TX_OFFSET >= T16_VA && TX_OFFSET + TX_SPAN <= T16_VA + T16_PITCH * T16_H,
          "the overlapping texture lies inside the 16-bit target, and not at its address",
          "texture 0x%05X..0x%05X, target 0x%05X..0x%05X", (unsigned)TX_OFFSET,
          (unsigned)(TX_OFFSET + TX_SPAN), (unsigned)T16_VA,
          (unsigned)(T16_VA + T16_PITCH * T16_H));

    memset(&st, 0, sizeof st);
    nv2a_vk_backend_readback_stats(&st);
    check(st.readbacks == 0 && st.batches == 0 && st.packed == 0 && st.packed_pixels == 0
          && st.copied == 0 && st.bytes == 0,
          "the published-work counters start at zero in a fresh world",
          "readbacks %llu, batches %llu, packed %llu, pixels %llu, copied %llu, bytes %llu",
          (unsigned long long)st.readbacks, (unsigned long long)st.batches,
          (unsigned long long)st.packed, (unsigned long long)st.packed_pixels,
          (unsigned long long)st.copied, (unsigned long long)st.bytes);
    check(VB_RB_CALLER_HAS_INCOMPLETE != 0,
          "the readback stats report the boundary's left-unpublished count",
          "VB_RB_CALLER_HAS_INCOMPLETE %d (a header without it cannot report a stop's cause)",
          VB_RB_CALLER_HAS_INCOMPLETE);
    verdict("0 the source and its seam", chk, bad);
}

/* ── 1. the success control ─────────────────────────────────────────────────────────── */

static void prop_success(void)
{
    FlipWorld w;
    VbTarget *ts[2];
    VkDeviceSize off[2];
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    uint32_t groups_x = (T16_RW + 15u) / 16u, groups_y = (T16_RH + 7u) / 8u;
    int chk = g_checks, bad = g_check_failures;

    section("1. the success control: one flip publishes both rectangles exactly");
    note("the injected state is the healthy one: the pack set allocates and the staging pair is "
         "made; the staging buffer is filled with what the copy engine and cs_pack would write");

    flip_world(&w);
    stage_gpu(&w);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();

    /* the recording */
    check(c.begins == 1u && c.resets == 1u && c.began == g.slots[0].cmd,
          "the frame's publication was recorded into one command buffer, begun once",
          "begins %u, resets %u, command buffer %p (slot 0 %p)", c.begins, c.resets,
          (void *)c.began, (void *)g.slots[0].cmd);
    check(c.ds_allocs == 1u && c.ds_failures == 0u,
          "the pack path was reached and its descriptor set was allocated",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);
    check(c.imgcopy == 1u && c.imgcopy_img == w.t4->im.img && c.imgcopy_buf == g.rb_buf
          && c.imgcopy_region.bufferOffset == w.off4,
          "the 4-byte target was copied straight out of its image, at its own slice",
          "vkCmdCopyImageToBuffer %u, image %p (target %p), buffer %p, offset %llu (wanted %llu)",
          c.imgcopy, (void *)c.imgcopy_img, (void *)w.t4->im.img, (void *)c.imgcopy_buf,
          (unsigned long long)c.imgcopy_region.bufferOffset, (unsigned long long)w.off4);
    check(c.dispatches == 1u && c.dispatch_group[0] == groups_x && c.dispatch_group[1] == groups_y
          && c.bufcopy == 1u && c.bufcopy_region.size == (VkDeviceSize)T16_STRIDE * T16_RH,
          "the 16-bit target was packed by one dispatch and one buffer copy",
          "dispatches %u (%ux%ux1, wanted %ux%u), buffer copies %u, copy size %llu (wanted %llu)",
          c.dispatches, c.dispatch_group[0], c.dispatch_group[1], groups_x, groups_y, c.bufcopy,
          (unsigned long long)c.bufcopy_region.size,
          (unsigned long long)((VkDeviceSize)T16_STRIDE * T16_RH));
    check(c.submits == 1u && c.submitted[0] == g.slots[0].cmd && c.ends == 1u
          && c.waits == 1u && c.invalidates == 1u && g.st.drain_waits == 1u
          && g.npending == 0u,
          "it was ended, submitted, drained and invalidated exactly once",
          "submits %u, ends %u, waits %u, invalidates %u, drain_waits %llu, npending %u",
          c.submits, c.ends, c.waits, c.invalidates, (unsigned long long)g.st.drain_waits,
          g.npending);

    /* the publication, the frame and the targets */
    ts[0] = w.t4;
    ts[1] = w.t16;
    off[0] = w.off4;
    off[1] = w.off16;
    check_guest_published("1 the success control", ts, off, 2);
    check_dirty_stale("1 the success control", "4-byte", w.t4, 0, 1);
    check_dirty_stale("1 the success control", "16-bit", w.t16, 0, 1);
    check(g.frame == 1u, "the flip advanced the frame exactly once", "g.frame %u", g.frame);
    check_health("1 the success control", 1);
    want_published_two(&want);
    check_counters("1 the success control", &b, &a, &want, 0);
    check_incomplete("1 the success control", inc0, inc1, 0);

    /* the next use of each target must load what the flip just published */
    load_probe("1b the 4-byte target's next load", w.t4, 1);
    load_probe("1c the 16-bit target's next load", w.t16, 1);

    verdict("1 the success control", chk, bad);
}

/* ── 2/3. a failed publication through the flip ─────────────────────────────────────── */

/* The state a stopped flip must leave behind: no frame, no stale mark, no guest byte, no
 * counter, nothing released, and the boundary's own count of what it had to leave behind. */
static void check_stopped_flip(const char *tag, const FlipWorld *w, const Nv2aVkReadbackStats *b,
                               const Nv2aVkReadbackStats *a, uint64_t inc0, uint64_t inc1,
                               uint64_t inc_want, unsigned fails_want)
{
    RbWant want;
    char what[110];

    check(g.frame == 0u, "the flip did NOT advance the frame", "g.frame %u (wanted 0)", g.frame);
    check_health(tag, 0);
    check_dirty_stale(tag, "4-byte", w->t4, 1, 0);
    check_dirty_stale(tag, "16-bit", w->t16, 1, 0);
    check_guest_unchanged(tag, "the failed publication published nothing");
    want_nothing(&want);
    check_counters(tag, b, a, &want, 1);
    check_incomplete(tag, inc0, inc1, inc_want);
    snprintf(what, sizeof what, "%s g.fails counts exactly the injected allocation failures", tag);
    check(g.fails == fails_want, what, "g.fails %u (wanted %u: the stop itself must not count)",
          g.fails, fails_want);
    snprintf(what, sizeof what, "%s nothing of either target was released or destroyed", tag);
    check(c.image_destroys == 0u && c.imageview_destroys == 0u && c.frees == 0u
          && c.fb_destroys == 0u && c.pipe_destroys == 0u && g.ngarbage == 0u,
          what, "image destroys %u, view destroys %u, memory frees %u, fb %u, pipe %u, "
                "held garbage %llu", c.image_destroys, c.imageview_destroys, c.frees,
          c.fb_destroys, c.pipe_destroys, (unsigned long long)g.ngarbage);
}

static void prop_descriptor(int transient)
{
    FlipWorld w;
    VbTarget *ts[2];
    VkDeviceSize off[2];
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    int chk = g_checks, bad = g_check_failures;

    section(transient
            ? "3. a transient descriptor failure through vb_flip: the retry publishes exactly once"
            : "2. a permanent descriptor failure through vb_flip: retried once, then it stops");
    note("the injected outcome is VK_ERROR_OUT_OF_POOL_MEMORY from this fixture's stub, NOT real "
         "pool exhaustion: the batch's pack descriptor set cannot be allocated (%s)",
         transient ? "on the first attempt only" : "on every attempt");

    flip_world(&w);
    c.ds_fail_n = transient ? 1u : 99u;
    stage_gpu(&w);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();

    check(c.ds_allocs == 2u && c.ds_failures == (transient ? 1u : 2u),
          "the allocation was attempted twice: the one bounded retry really happens",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);
    check(c.imgcopy == 2u && c.dispatches == (transient ? 1u : 0u)
          && c.bufcopy == (transient ? 1u : 0u),
          transient
            ? "the retry recorded the pack dispatch the first attempt could not"
            : "the retry recorded no dispatch either: the pack set never allocated",
          "4-byte copies %u (the first attempt's and the retry's), pack dispatches %u, "
          "pack buffer copies %u", c.imgcopy, c.dispatches, c.bufcopy);

    ts[0] = w.t4;
    ts[1] = w.t16;
    off[0] = w.off4;
    off[1] = w.off16;

    if (!transient) {
        check_stopped_flip("2 the permanent descriptor failure", &w, &b, &a, inc0, inc1, 2u, 2u);
        check(c.submits == 1u && c.begins == 2u && c.ends == 1u && c.waits == 1u,
              "the ring was refreshed for the retry: the abandoned recording was submitted, and "
              "the retry began its own",
              "submits %u, begins %u (the abandoned attempt's and the retry's), ends %u, waits %u",
              c.submits, c.begins, c.ends, c.waits);
        check(w.t4->im.img != VK_NULL_HANDLE && w.t4->im.view != VK_NULL_HANDLE
              && w.t4->dx1 == T4_DX1 && w.t16->dx1 == T16_DX1 && w.t16->im.view != VK_NULL_HANDLE,
              "the targets' image handles and dirty rectangles are intact",
              "4-byte img %p view %p rect x1 %u, 16-bit view %p rect x1 %u",
              (void *)w.t4->im.img, (void *)w.t4->im.view, w.t4->dx1, (void *)w.t16->im.view,
              w.t16->dx1);

        /* the after-the-stop half, kept with the world that produced it */
        {
            uint32_t frame0 = g.frame;
            unsigned copy0 = c.copy2img;
            unsigned fails0 = g.fails;
            unsigned submits0 = c.submits;
            uint64_t inc_before_second = inc1;

            vb_flip();
            check(g.frame == frame0, "a second flip publishes nothing and advances nothing",
                  "g.frame %u -> %u", frame0, g.frame);
            check(c.submits == submits0 && g.fails == fails0
                  && c.copy2img == copy0 && stats_incomplete() == inc_before_second,
                  "it recorded nothing, submitted nothing and stopped nothing again",
                  "submits %u -> %u, g.fails %u -> %u, copies %u -> %u, incomplete %llu -> %llu",
                  submits0, c.submits, fails0, g.fails, copy0, c.copy2img,
                  (unsigned long long)inc_before_second,
                  (unsigned long long)stats_incomplete());
            check_dirty_stale("11 after the stop", "4-byte", w.t4, 1, 0);
            check_dirty_stale("11 after the stop", "16-bit", w.t16, 1, 0);
            check_guest_unchanged("11 after the stop", "the stopped back end published nothing");
            load_probe("11 the 4-byte target's load after the stop", w.t4, 0);
            load_probe("11 the 16-bit target's load after the stop", w.t16, 0);
        }
        verdict("2 the permanent descriptor failure through the flip", chk, bad);
        return;
    }

    check_dirty_stale("3 the transient descriptor failure", "4-byte", w.t4, 0, 1);
    check_dirty_stale("3 the transient descriptor failure", "16-bit", w.t16, 0, 1);
    check(g.frame == 1u, "the flip advanced the frame exactly once", "g.frame %u", g.frame);
    check_health("3 the transient descriptor failure", 1);
    check_guest_published("3 the transient descriptor failure", ts, off, 2);
    want_published_two(&want);
    check_counters("3 the transient descriptor failure", &b, &a, &want, 0);
    check_incomplete("3 the transient descriptor failure", inc0, inc1, 0);
    load_probe("3 the 4-byte target's next load", w.t4, 1);

    verdict("3 the transient descriptor failure through the flip", chk, bad);
}

/* ── 4/5. the staging pair's failure through the flip ──────────────────────────────── */

static void prop_staging(int transient)
{
    FlipWorld w;
    VbTarget *ts[2];
    VkDeviceSize off[2];
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    int chk = g_checks, bad = g_check_failures;

    section(transient
            ? "5. a transient staging failure through vb_flip: the retry publishes exactly once"
            : "4. a permanent staging failure through vb_flip: retried once, then it stops");
    note("the injected outcome is VK_ERROR_OUT_OF_DEVICE_MEMORY from this fixture's stub: "
         "vb_readback_reserve's pair cannot be created, so no batch is staged at all (%s)",
         transient ? "on the first attempt only" : "on every attempt");

    flip_world(&w);
    c.buffer_fail_n = transient ? 1u : 99u;
    stage_gpu(&w);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();

    check(c.buffers == (transient ? 3u : 2u) && c.buffer_failures == (transient ? 1u : 2u),
          "the staging pair was attempted twice: the one bounded retry really happens",
          "vkCreateBuffer %u (%u failed), wanted %u attempts and %u failures", c.buffers,
          c.buffer_failures, transient ? 3u : 2u, transient ? 1u : 2u);
    check(c.begins == (transient ? 1u : 0u) && c.submits == (transient ? 1u : 0u)
          && c.ends == (transient ? 1u : 0u),
          transient
            ? "only the retry recorded: the failed attempt staged nothing to record"
            : "the failure happens before any recording, so nothing was begun or submitted",
          "begins %u, submits %u, ends %u (wanted %u of each)", c.begins, c.submits, c.ends,
          transient ? 1u : 0u);

    ts[0] = w.t4;
    ts[1] = w.t16;
    off[0] = w.off4;
    off[1] = w.off16;

    if (!transient) {
        check_stopped_flip("4 the permanent staging failure", &w, &b, &a, inc0, inc1, 2u, 2u);
        check(g.rb_buf == VK_NULL_HANDLE && g.rb_cap == 0,
              "no staging pair was left half-made", "rb_buf %p, capacity %llu",
              (void *)g.rb_buf, (unsigned long long)g.rb_cap);
        verdict("4 the permanent staging failure through the flip", chk, bad);
        return;
    }

    check_dirty_stale("5 the transient staging failure", "4-byte", w.t4, 0, 1);
    check_dirty_stale("5 the transient staging failure", "16-bit", w.t16, 0, 1);
    check(g.frame == 1u, "the flip advanced the frame exactly once", "g.frame %u", g.frame);
    check_health("5 the transient staging failure", 1);
    check_guest_published("5 the transient staging failure", ts, off, 2);
    want_published_two(&want);
    check_counters("5 the transient staging failure", &b, &a, &want, 0);
    check_incomplete("5 the transient staging failure", inc0, inc1, 0);
    check(g.rb_buf != VK_NULL_HANDLE && g.rb_ptr == g_stage && g.rb_cap == (VkDeviceSize)GATE_RB_CAP,
          "the retry made the pair, and it maps the fixture's own staging arena",
          "rb %p, mapped %p (arena %p), capacity %llu (wanted %u)", (void *)g.rb_buf,
          (void *)g.rb_ptr, (void *)g_stage, (unsigned long long)g.rb_cap, (unsigned)GATE_RB_CAP);
    load_probe("5 the 16-bit target's next load", w.t16, 1);

    verdict("5 the transient staging failure through the flip", chk, bad);
}

/* ── 6/7/8. vb_target_for: a replacement must publish before it releases ───────────── */

/* The surface vb_target_for is asked for: the same address as the target in `va`, a different
 * pitch, so it takes the "same address, new shape" branch. */
static void reshape_surface(Nv2aSurface *s, uint32_t va, uint32_t pitch)
{
    memset(s, 0, sizeof *s);
    s->color_va = va;
    s->pitch = pitch;
    s->bytes_per_pixel = T16_BPP;
    s->width = pitch / T16_BPP;
    s->height = 32u;
    s->color_format = 0x3u;
}

static void prop_target_for_reshape(int transient)
{
    VbTarget *ts[1];
    VkDeviceSize off[1];
    Nv2aSurface s;
    VbTarget before;
    VkImage img0;
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    VkDeviceSize slice;
    VbTarget *got;
    int chk = g_checks, bad = g_check_failures;

    section(transient
            ? "7. vb_target_for, same address and a new shape, transient: publish, then replace"
            : "6. vb_target_for, same address and a new shape, permanent: NULL, not released");
    note("the target is asked for with a different pitch, so the caller must publish it before "
         "it may release or reshape it; the allocation %s", transient
         ? "fails on the first attempt and succeeds on the retry"
         : "fails on every attempt");

    gate_world();
    wire_target(&g.targets[0], T16_VA, T16_PITCH, T16_BPP, T16_DX0, T16_DY0, T16_DX1, T16_DY1,
                T16_H);
    c.ds_fail_n = transient ? 1u : 99u;
    reshape_surface(&s, T16_VA, T16_PITCH * 2u);        /* 88 bytes: a new pitch, a new target */
    before = g.targets[0];
    img0 = before.im.img;
    slice = (VkDeviceSize)T16_STRIDE * T16_RH;
    stage_target(&before, 0, 0x50u);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    got = vb_target_for(&s);

    stats(&a);
    inc1 = stats_incomplete();
    /* The guest expectation and the rectangle that changed are the target AS THE CALL SAW IT: a
     * successful replacement rewrites g.targets[0], so it no longer describes what was
     * published. */
    ts[0] = &before;
    off[0] = 0;

    check(c.ds_allocs == 2u && c.ds_failures == (transient ? 1u : 2u),
          "the target's own publication was attempted twice before anything was released",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);

    if (!transient) {
        RbWant none;

        check(got == NULL, "vb_target_for returned NULL", "returned %p (wanted NULL)", (void *)got);
        check_health("6 the permanent reshape failure", 0);
        check(g.frame == 0u, "the frame did not advance", "g.frame %u", g.frame);
        want_nothing(&none);
        check_counters("6 the permanent reshape failure", &b, &a, &none, 1);
        check_incomplete("6 the permanent reshape failure", inc0, inc1, 1);
        check_guest_unchanged("6 the permanent reshape failure",
                              "the newer image was never published");
        check(g.targets[0].im.img == before.im.img && g.targets[0].im.view == before.im.view
              && g.targets[0].im.mem == before.im.mem && g.targets[0].used == 1
              && g.targets[0].va == before.va && g.targets[0].pitch == before.pitch
              && g.targets[0].bpp == before.bpp && g.targets[0].w == before.w
              && g.targets[0].h == before.h,
              "the target was NOT released: its image handles and shape are untouched",
              "img %p (was %p), view %p (was %p), mem %p (was %p), pitch %u (was %u), "
              "h %u (was %u)", (void *)g.targets[0].im.img, (void *)before.im.img,
              (void *)g.targets[0].im.view, (void *)before.im.view, (void *)g.targets[0].im.mem,
              (void *)before.im.mem, g.targets[0].pitch, before.pitch, g.targets[0].h, before.h);
        check(g.targets[0].dirty == 1 && g.targets[0].dx0 == T16_DX0 && g.targets[0].dy0 == T16_DY0
              && g.targets[0].dx1 == T16_DX1 && g.targets[0].dy1 == T16_DY1,
              "its dirty rectangle is intact too, so the newer pixels are still publishable",
              "dirty %d, rect %u,%u..%u,%u", g.targets[0].dirty, g.targets[0].dx0,
              g.targets[0].dy0, g.targets[0].dx1, g.targets[0].dy1);
        check(c.image_destroys == 0u && c.imageview_destroys == 0u && c.frees == 0u
              && c.images == 0u && c.imageviews == 0u,
              "nothing was destroyed and no new target image was made",
              "image destroys %u, view destroys %u, frees %u, images %u, views %u",
              c.image_destroys, c.imageview_destroys, c.frees, c.images, c.imageviews);
        verdict("6 vb_target_for, permanent reshape", chk, bad);
        return;
    }

    check(got == &g.targets[0], "vb_target_for returned the target", "returned %p (target %p)",
          (void *)got, (void *)&g.targets[0]);
    check_health("7 the transient reshape failure", 1);
    check_guest_published("7 the transient reshape failure", ts, off, 1);
    want_published_one_16(&want);
    check_counters("7 the transient reshape failure", &b, &a, &want, 0);
    check_incomplete("7 the transient reshape failure", inc0, inc1, 0);
    check((size_t)slice == rect_bytes(&before) && c.imgcopy == 0u && c.dispatches == 1u
          && c.bufcopy == 1u,
          "the published slice was the target's own rectangle, packed by one dispatch",
          "slice %llu (rect %zu bytes), image copies %u, dispatches %u, buffer copies %u",
          (unsigned long long)slice, rect_bytes(&before), c.imgcopy, c.dispatches, c.bufcopy);
    check(g.targets[0].im.img != img0 && g.targets[0].im.img != VK_NULL_HANDLE
          && g.targets[0].pitch == s.pitch && g.targets[0].bpp == T16_BPP
          && g.targets[0].w == s.pitch / T16_BPP && g.targets[0].h == s.height
          && g.targets[0].dirty == 0 && g.targets[0].stale == 1,
          "the target was replaced only after that publication: new image, the new shape",
          "img %p (was %p), pitch %u (wanted %u), w %u (wanted %u), h %u (wanted %u), dirty %d, "
          "stale %d", (void *)g.targets[0].im.img, (void *)img0, g.targets[0].pitch, s.pitch,
          g.targets[0].w, s.pitch / T16_BPP, g.targets[0].h, s.height, g.targets[0].dirty,
          g.targets[0].stale);
    check(c.image_destroys == 1u && c.imageview_destroys == 1u && c.images == 1u
          && c.imageviews == 1u,
          "the old image was released and exactly one new one made",
          "image destroys %u (wanted 1), view destroys %u, images made %u, views made %u",
          c.image_destroys, c.imageview_destroys, c.images, c.imageviews);

    verdict("7 vb_target_for, transient reshape", chk, bad);
}

/* The LRU branch: every target is used and none matches the address, so the least recently used
 * one is the victim. Its own publication is the one that must land before it is released. */
static void prop_target_for_evict(int transient)
{
    VbTarget *ts[1];
    VkDeviceSize off[1];
    Nv2aSurface s;
    VbTarget before;
    VkImage img0;
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    VbTarget *got;
    unsigned i;
    int chk = g_checks, bad = g_check_failures;

    section(transient
            ? "8b the LRU eviction, transient: the victim is published before it is released"
            : "8a the LRU eviction, permanent: NULL, and the victim is NOT released");
    note("all %u targets are in use and none has the address vb_target_for is asked for, so the "
         "least recently used one is the victim", (unsigned)VB_MAX_TARGETS);

    gate_world();
    wire_target(&g.targets[0], T16_VA, T16_PITCH, T16_BPP, T16_DX0, T16_DY0, T16_DX1, T16_DY1,
                T16_H);
    for (i = 1; i < VB_MAX_TARGETS; i++) {
        memset(&g.targets[i], 0, sizeof g.targets[i]);
        g.targets[i].used = 1;
        g.targets[i].va = T4_VA + i * 0x10000u;
        g.targets[i].pitch = T16_PITCH;
        g.targets[i].bpp = T16_BPP;
        g.targets[i].w = T16_PITCH / T16_BPP;
        g.targets[i].h = T16_H;
        g.targets[i].dirty = 0;
        g.targets[i].im.img = (VkImage)hnew();
        g.targets[i].im.mem = (VkDeviceMemory)hnew();
        g.targets[i].im.view = (VkImageView)hnew();
        g.targets[i].last_use = 2u + i;                 /* targets[0] is the least used */
    }
    c.ds_fail_n = transient ? 1u : 99u;
    reshape_surface(&s, EVICT_NEW_VA, T16_PITCH);
    s.clip_y = 0;
    before = g.targets[0];
    img0 = before.im.img;
    stage_target(&before, 0, 0x50u);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    got = vb_target_for(&s);

    stats(&a);
    inc1 = stats_incomplete();
    /* The guest expectation and the rectangle that changed are the target AS THE CALL SAW IT: a
     * successful replacement rewrites g.targets[0], so it no longer describes what was
     * published. */
    ts[0] = &before;
    off[0] = 0;

    check(c.ds_allocs == 2u && c.ds_failures == (transient ? 1u : 2u),
          "the victim's publication was attempted twice before anything was released",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);

    if (!transient) {
        RbWant none;

        check(got == NULL, "vb_target_for returned NULL", "returned %p (wanted NULL)", (void *)got);
        check_health("8a the permanent eviction failure", 0);
        want_nothing(&none);
        check_counters("8a the permanent eviction failure", &b, &a, &none, 1);
        check_incomplete("8a the permanent eviction failure", inc0, inc1, 1);
        check_guest_unchanged("8a the permanent eviction failure", "the victim was not published");
        check(g.targets[0].im.img == before.im.img && g.targets[0].im.view == before.im.view
              && g.targets[0].va == before.va && g.targets[0].pitch == before.pitch
              && g.targets[0].dirty == 1,
              "the victim was NOT released: handle, address, shape and dirty rectangle intact",
              "img %p (was %p), va %u (was %u), pitch %u (was %u), dirty %d",
              (void *)g.targets[0].im.img, (void *)before.im.img, g.targets[0].va, before.va,
              g.targets[0].pitch, before.pitch, g.targets[0].dirty);
        check(c.image_destroys == 0u && c.images == 0u,
              "nothing was destroyed and no new target image was made",
              "image destroys %u, images %u", c.image_destroys, c.images);
        verdict("8a vb_target_for, permanent eviction", chk, bad);
        return;
    }

    check(got != NULL && got->va == EVICT_NEW_VA && got->pitch == T16_PITCH
          && got->im.img != img0 && got->dirty == 0 && got->stale == 1,
          "the victim was replaced by the new target only after its publication",
          "returned %p, va %u (wanted %u), pitch %u, img %p (was %p), dirty %d, stale %d",
          (void *)got, got ? got->va : 0u, (unsigned)EVICT_NEW_VA, got ? got->pitch : 0u,
          got ? (void *)got->im.img : NULL, (void *)img0, got ? got->dirty : -1,
          got ? got->stale : -1);
    check_health("8b the transient eviction failure", 1);
    check_guest_published("8b the transient eviction failure", ts, off, 1);
    want_published_one_16(&want);
    check_counters("8b the transient eviction failure", &b, &a, &want, 0);
    check_incomplete("8b the transient eviction failure", inc0, inc1, 0);

    verdict("8b vb_target_for, transient eviction", chk, bad);
}

/* ── 9/10. vb_texture over a dirty target: no decode of superseded guest bytes ─────── */

/* The overlapping texture: one row of the 16-bit target's dirty rectangle, so a decode before
 * that target's publication reads bytes the publication supersedes, and a decode after it reads
 * the published ones. */
static void overlap_texture(Nv2aTexture *tx)
{
    memset(tx, 0, sizeof *tx);
    tx->offset = TX_OFFSET;
    tx->width = TX_W;
    tx->height = TX_H;
    tx->pitch = TX_PITCH;
    tx->color = 0x12u;                  /* LIN_A8R8G8B8: a format the fixture's decoder takes */
    tx->addr_u = 1u;
    tx->addr_v = 1u;
    tx->linear = 1u;
}

static void prop_texture(int transient)
{
    VbTarget *ts[1];
    VkDeviceSize off[1];
    VbTarget before;
    Nv2aTexture tx;
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    uint64_t want_hash, old_hash;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    float iw = 0.0f, ih = 0.0f;
    VkImageView view;
    int chk = g_checks, bad = g_check_failures;

    section(transient
            ? "10. vb_texture over a dirty target, transient: publish first, then a live view"
            : "9. vb_texture over a dirty target, permanent: NULL, and nothing is decoded");

    gate_world();
    wire_target(&g.targets[0], T16_VA, T16_PITCH, T16_BPP, T16_DX0, T16_DY0, T16_DX1, T16_DY1,
                T16_H);
    c.ds_fail_n = transient ? 1u : 99u;
    overlap_texture(&tx);
    before = g.targets[0];
    ts[0] = &g.targets[0];
    off[0] = 0;
    stage_target(&before, 0, 0x50u);
    memcpy(g_pre, g_guest, GATE_GUEST);
    expect_guest(ts, off, 1);                   /* what the target's publication must produce */
    want_hash = hash_bytes(g_expect + tx.offset, tx.pitch * tx.height);
    old_hash = hash_bytes(g_pre + tx.offset, tx.pitch * tx.height);
    stats(&b);
    inc0 = stats_incomplete();

    view = vb_texture(&tx, NULL, &layout, &iw, &ih);

    stats(&a);
    inc1 = stats_incomplete();

    check(want_hash != old_hash,
          "the texture really does overlap bytes the publication changes",
          "published 0x%016llX vs pre-publication 0x%016llX at 0x%05X (%u bytes)",
          (unsigned long long)want_hash, (unsigned long long)old_hash, (unsigned)tx.offset,
          (unsigned)tx.pitch * tx.height);
    check(c.spans == 1u, "the fixture's span query was reached, so the overlap loop ran",
          "nv2a_backend_texture_span calls %u (span %u)", c.spans, (unsigned)tx.pitch * tx.height);
    check(c.ds_allocs == 2u && c.ds_failures == (transient ? 1u : 2u),
          "the overlapping target's publication was attempted twice",
          "vkAllocateDescriptorSets %u, failures %u", c.ds_allocs, c.ds_failures);

    if (!transient) {
        RbWant none;

        check(view == VK_NULL_HANDLE, "vb_texture returned VK_NULL_HANDLE",
              "returned %p (wanted VK_NULL_HANDLE)", (void *)view);
        check(c.decodes == 0u,
              "NO guest-byte decode happened: the superseded bytes were never read",
              "nv2a_backend_decode_texture calls %u (wanted 0), last shown 0x%05X",
              c.decodes, (unsigned)c.decoded_offset);
        check(c.images == 0u && c.imageviews == 0u,
              "no texture image was made from stale bytes", "images %u, views %u", c.images,
              c.imageviews);
        check(g.st.tex_uploads == 0u, "no texture upload was counted", "tex_uploads %llu",
              (unsigned long long)g.st.tex_uploads);
        check_health("9 the permanent texture failure", 0);
        check_guest_unchanged("9 the permanent texture failure", "nothing was published");
        want_nothing(&none);
        check_counters("9 the permanent texture failure", &b, &a, &none, 1);
        check_incomplete("9 the permanent texture failure", inc0, inc1, 1);
        check(g.targets[0].im.img == before.im.img && g.targets[0].im.view == before.im.view
              && g.targets[0].dirty == 1,
              "the target was not released or cleared: it is still publishable",
              "img %p (was %p), view %p (was %p), dirty %d",
              (void *)g.targets[0].im.img, (void *)before.im.img,
              (void *)g.targets[0].im.view, (void *)before.im.view, g.targets[0].dirty);

        verdict("9 vb_texture over a dirty target, permanent", chk, bad);
        return;
    }

    check(view != VK_NULL_HANDLE, "vb_texture returned a live view", "returned %p", (void *)view);
    check(c.decodes == 1u && c.decoded_offset == tx.offset && c.decoded_span == tx.pitch * tx.height,
          "exactly one decode, of the texture's own span",
          "decodes %u (wanted 1), offset 0x%05X (wanted 0x%05X), span %u (wanted %u)",
          c.decodes, (unsigned)c.decoded_offset, (unsigned)tx.offset, (unsigned)c.decoded_span,
          (unsigned)tx.pitch * tx.height);
    check(c.decoded_hash == want_hash && c.decoded_hash != old_hash,
          "the decoder was shown the PUBLISHED bytes, never the pre-publication ones",
          "decoded 0x%016llX, published 0x%016llX, pre-publication 0x%016llX",
          (unsigned long long)c.decoded_hash, (unsigned long long)want_hash,
          (unsigned long long)old_hash);
    check(c.images == 1u && c.imageviews == 1u && g.st.tex_uploads == 1u && view == c.last_view,
          "one texture image was made and uploaded from them",
          "images %u, views %u, tex_uploads %llu, view %p (last made %p)", c.images, c.imageviews,
          (unsigned long long)g.st.tex_uploads, (void *)view, (void *)c.last_view);
    check_health("10 the transient texture failure", 1);
    check_guest_published("10 the transient texture failure", ts, off, 1);
    want_published_one_16(&want);
    check_counters("10 the transient texture failure", &b, &a, &want, 0);
    check_incomplete("10 the transient texture failure", inc0, inc1, 0);
    check(g.st.rt_loads == 0u && g.targets[0].stale == 0,
          "the target was published, not reloaded: no load ran",
          "rt_loads %llu, stale %d (the one recorded copy is the texture's own upload, image %p)",
          (unsigned long long)g.st.rt_loads, g.targets[0].stale,
          c.copy2img ? (void *)c.copy2img_img : NULL);

    verdict("10 vb_texture over a dirty target, transient", chk, bad);
}

/* ── 12. the capacity seam: a group over VB_READBACK_MAX splits into two batches ─────── */

/* Two dirty 4-byte targets whose slices together exceed VB_READBACK_MAX (two 64 MiB rectangles,
 * 128 MiB in all). The flush's group scan takes targets while `total + slice <= VB_READBACK_MAX`,
 * so the cap is what makes this ONE FLIP become TWO BATCHES rather than fail: the first group
 * holds target 0 alone, the second target 1, and each group has its own reservation, submission,
 * fence wait, invalidate and host memcpy. The staging pair's growth to 64 MiB is the bounded
 * behaviour -- `vb_readback_reserve` grows the pair by doubling to the batch's need and never past
 * the 96 MiB cap -- and the second batch's slice fits the pair the first one grew, so the pair
 * must NOT be released and re-made between them: that is the lifetime half of this property, and
 * the fixture has both `vkMapMemory` (one call means one pair) and the destination buffer of every
 * publication command to decide it on. */
static void prop_split(void)
{
    VbTarget *ts[2];
    VkDeviceSize off[2];
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    unsigned i;
    int same_dst = 1;
    int chk = g_checks, bad = g_check_failures;

    section("12. a dirty group over VB_READBACK_MAX splits into two batches, both published once");
    note("two 64 MiB dirty rectangles (4096x4096 and 8192x2048 4-byte) = 128 MiB over the %llu MiB "
         "cap, so one flip is two batches: the cap splits the group, it does not fail it",
         (unsigned long long)(VB_READBACK_MAX >> 20));
    note("the pair grows to the first batch's 64 MiB and the second batch's slice fits that same "
         "pair: one vkCreateBuffer pair, one vkMapMemory, and every publication command's "
         "destination is the same buffer");

    gate_world();
    wire_target(&g.targets[0], BIG_A_VA, BIG_A_PITCH, BIG_BPP, 0, 0, BIG_A_RW, BIG_A_RH, BIG_A_RH);
    wire_target(&g.targets[1], BIG_B_VA, BIG_B_PITCH, BIG_BPP, 0, 0, BIG_B_RW, BIG_B_RH, BIG_B_RH);
    guest_big_markers();                                /* what guest memory holds first */
    fill_stage_slice(g_stage, (size_t)BIG_SLICE, 0x50u);        /* the GPU's half of both slices */
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();
    ts[0] = &g.targets[0];
    ts[1] = &g.targets[1];
    off[0] = 0;
    off[1] = 0;                         /* each batch's slice starts at the pair's offset 0 */

    check(g.targets[0].dx1 - g.targets[0].dx0 == BIG_A_RW
          && g.targets[1].dx1 - g.targets[1].dx0 == BIG_B_RW,
          "the two targets really are two different shapes with the same 64 MiB slice",
          "A %ux%u stride %llu, B %ux%u stride %llu, slice %llu, both = %llu",
          g.targets[0].dx1 - g.targets[0].dx0, g.targets[0].dy1 - g.targets[0].dy0,
          (unsigned long long)((VkDeviceSize)(g.targets[0].dx1 - g.targets[0].dx0) * BIG_BPP),
          g.targets[1].dx1 - g.targets[1].dx0, g.targets[1].dy1 - g.targets[1].dy0,
          (unsigned long long)((VkDeviceSize)(g.targets[1].dx1 - g.targets[1].dx0) * BIG_BPP),
          (unsigned long long)BIG_SLICE, (unsigned long long)BIG_TOTAL);

    /* two batches: two recordings, two submissions, two waits, two invalidates */
    check(c.begins == 2u && c.resets == 2u && c.ends == 2u && c.submits == 2u && c.waits == 2u
          && c.invalidates == 2u && g.st.drain_waits == 2u,
          "the frame became two batches: two recordings, two submits, two waits, two invalidates",
          "begins %u, resets %u, ends %u, submits %u, waits %u, invalidates %u, drain_waits %llu",
          c.begins, c.resets, c.ends, c.submits, c.waits, c.invalidates,
          (unsigned long long)g.st.drain_waits);
    check(c.imgcopy == 2u && g.targets[0].im.img != g.targets[1].im.img,
          "each batch staged its own target's image, exactly once",
          "vkCmdCopyImageToBuffer %u (targets %p and %p)", c.imgcopy,
          (void *)g.targets[0].im.img, (void *)g.targets[1].im.img);

    /* the pair's lifetime: one pair, grown once, reused by the second batch */
    for (i = 0; i < c.ncopy_dst; i++)
        if (c.copy_dst[i] != g.rb_buf)
            same_dst = 0;
    check(c.ncopy_dst == 2u && same_dst && g.rb_buf != VK_NULL_HANDLE,
          "both batches published through the SAME staging buffer",
          "%u publication commands, destination %p vs %p (first %p, second %p)",
          c.ncopy_dst, (void *)c.copy_dst[0], (void *)g.rb_buf, (void *)c.copy_dst[0],
          (void *)(c.ncopy_dst > 1u ? c.copy_dst[1] : VK_NULL_HANDLE));
    check(c.buffers == 2u && c.maps == 1u && c.buffer_destroys == 0u && c.frees == 0u,
          "the pair was made once and never released or re-made: the second batch reused it",
          "vkCreateBuffer %u (a pair), vkMapMemory %u, buffer destroys %u, frees %u", c.buffers,
          c.maps, c.buffer_destroys, c.frees);
    check(g.rb_cap == BIG_SLICE && g.rb_ptr == g_stage && g.rb_buf != VK_NULL_HANDLE,
          "its capacity grew to exactly the first batch's 64 MiB, inside the 96 MiB cap",
          "capacity %llu (wanted %llu, cap %llu), mapped %p (arena %p)",
          (unsigned long long)g.rb_cap, (unsigned long long)BIG_SLICE,
          (unsigned long long)VB_READBACK_MAX, (void *)g.rb_ptr, (void *)g_stage);

    /* the ordering: the first batch's invalidate (which precedes its host memcpy) comes before
     * the second batch's recording begins -- so the first rectangle was in guest memory first */
    check(ev_nth(EV_SUBMIT, 1) >= 0 && ev_nth(EV_INVALIDATE, 1) >= 0 && ev_nth(EV_BEGIN, 2) >= 0
          && ev_nth(EV_WAIT, 1) > ev_nth(EV_SUBMIT, 1)
          && ev_nth(EV_INVALIDATE, 1) > ev_nth(EV_WAIT, 1)
          && ev_nth(EV_INVALIDATE, 1) < ev_nth(EV_BEGIN, 2),
          "the first batch's publication preceded the second batch's recording",
          "trace indices: submit#1 %d, wait#1 %d, invalidate#1 %d (its memcpy loop follows it), "
          "begin#2 %d", ev_nth(EV_SUBMIT, 1), ev_nth(EV_WAIT, 1), ev_nth(EV_INVALIDATE, 1),
          ev_nth(EV_BEGIN, 2));

    /* the bytes: both rectangles exact, row by row, and nothing outside them or in the padding */
    check_guest_published("12 the split flip", ts, off, 2);
    check_dirty_stale("12 the split flip", "4096x4096", &g.targets[0], 0, 1);
    check_dirty_stale("12 the split flip", "8192x2048", &g.targets[1], 0, 1);
    check(g.frame == 1u, "the flip advanced the frame exactly once", "g.frame %u", g.frame);
    check_health("12 the split flip", 1);
    memset(&want, 0, sizeof want);
    want.readbacks = 2;
    want.batches = 2;                   /* one published batch per group */
    want.copied = 2;                    /* both 4-byte: no pack dispatch at all */
    want.bytes = (uint64_t)BIG_TOTAL;   /* 128 MiB into guest memory */
    check_counters("12 the split flip", &b, &a, &want, 0);
    check_incomplete("12 the split flip", inc0, inc1, 0);

    verdict("12 the capacity seam: the group splits and both batches publish", chk, bad);
}

/* ── 12b. the capacity seam: a split frame whose second batch cannot publish stops ────── */

/* The failure half of property 12, and the half the caller-propagation mutants decide. Two dirty
 * 16-bit targets, 56 MiB of slice each, so the frame is two batches; the fixture then makes every
 * pack descriptor set from the SECOND allocation on fail, which is exactly "the first batch
 * publishes, the second cannot". A boundary that treats a partially published frame as published
 * would advance g.frame and mark both targets stale, and the next load would then read the guest
 * bytes the second target's publication never wrote. What must happen instead: the first batch's
 * rectangle is in guest memory exactly, the second target stays dirty, the boundary counts it as
 * left unpublished and stops -- no frame advance, nothing marked stale, nothing released. */
static void prop_split_failure(void)
{
    VbTarget *ts[1];
    VkDeviceSize off[1];
    VbTarget a_before;
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    uint32_t groups_x = (MID_RW + 15u) / 16u, groups_y = (MID_RH + 7u) / 8u;
    int chk = g_checks, bad = g_check_failures;

    section("12b. a split frame whose second batch cannot publish stops the boundary");
    note("two 56 MiB 16-bit targets = 112 MiB over the 96 MiB cap: two batches, and the pack set "
         "allocation fails from the second one on (VK_ERROR_OUT_OF_POOL_MEMORY from this fixture's "
         "stub), so the first batch publishes and the second must not be treated as published");

    gate_world();
    wire_target(&g.targets[0], MID_A_VA, MID_PITCH, MID_BPP, 0, 0, MID_RW, MID_RH, MID_H);
    wire_target(&g.targets[1], MID_B_VA, MID_PITCH, MID_BPP, 0, 0, MID_RW, MID_RH, MID_H);
    c.ds_fail_from = 2u;                /* #1 (the first batch) succeeds; #2 and the retry fail */
    guest_marker_rect(MID_A_VA, MID_PITCH, MID_RW, MID_RH, MID_BPP, 0x10u);
    guest_marker_rect(MID_B_VA, MID_PITCH, MID_RW, MID_RH, MID_BPP, 0x20u);
    fill_stage_slice(g_stage, (size_t)MID_SLICE, 0x50u);
    a_before = g.targets[0];
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();
    ts[0] = &a_before;
    off[0] = 0;

    check(c.ds_allocs == 3u && c.ds_failures == 2u,
          "the second batch's allocation was tried, failed, and the one retry tried again",
          "vkAllocateDescriptorSets %u, failures %u (wanted 3 attempts, 2 failures)", c.ds_allocs,
          c.ds_failures);
    check(c.dispatches == 1u && c.bufcopy == 1u && c.imgcopy == 0u
          && c.dispatch_group[0] == groups_x && c.dispatch_group[1] == groups_y,
          "exactly ONE publication's commands were recorded: the first batch's pack dispatch",
          "dispatches %u (%ux%ux1, wanted %ux%u), buffer copies %u, image copies %u", c.dispatches,
          c.dispatch_group[0], c.dispatch_group[1], groups_x, groups_y, c.bufcopy, c.imgcopy);
    check(c.buffers == 2u && c.maps == 1u && g.rb_cap == (VkDeviceSize)(VB_READBACK_MAX / 3u * 2u)
          && c.buffer_destroys == 0u && c.frees == 0u,
          "the second batch's slice fitted the same pair the first grew, and nothing was released",
          "vkCreateBuffer %u, vkMapMemory %u, capacity %llu, buffer destroys %u, frees %u",
          c.buffers, c.maps, (unsigned long long)g.rb_cap, c.buffer_destroys, c.frees);

    check_guest_published("12b the half-published split", ts, off, 1);
    check_dirty_stale("12b the half-published split", "the published", &g.targets[0], 0, 0);
    check_dirty_stale("12b the half-published split", "the unpublished", &g.targets[1], 1, 0);
    check(g.targets[1].im.img != VK_NULL_HANDLE && g.targets[1].im.view != VK_NULL_HANDLE,
          "the unpublished target still owns its image", "img %p, view %p",
          (void *)g.targets[1].im.img, (void *)g.targets[1].im.view);
    check(g.frame == 0u, "the flip did NOT advance the frame over a half-published frame",
          "g.frame %u (wanted 0)", g.frame);
    check_health("12b the half-published split", 0);
    check(g.fails == 2u, "g.fails counts the two injected allocation failures",
          "g.fails %u (wanted 2: the stop must not count)", g.fails);
    memset(&want, 0, sizeof want);
    want.readbacks = 1;                 /* only the first batch's target */
    want.batches = 1;
    want.packed = 1;
    want.packed_pixels = (uint64_t)MID_RW * MID_RH;
    want.bytes = (uint64_t)MID_SLICE;
    check_counters("12b the half-published split", &b, &a, &want, 0);
    check_incomplete("12b the half-published split", inc0, inc1, 1);
    check(c.image_destroys == 0u && c.imageview_destroys == 0u,
          "neither target was released while stopping", "image destroys %u, view destroys %u",
          c.image_destroys, c.imageview_destroys);

    verdict("12b the capacity seam: a half-published split stops the boundary", chk, bad);
}

/* ── 13. the capacity seam: a single over-cap dirty target is never staged ───────────── */

/* A dirty rectangle of 256 MiB -- over VB_READBACK_MAX on its own -- beside a normal dirty target.
 * The group scan steps over it (it can never be staged, so its group would publish nothing) and
 * stages the other target, so the flip publishes what it can and then STOPS: the over-cap branch is
 * a stop, never a silent skip of that target nor a partial copy of it. The synthetic target's
 * address lies outside the fixture's guest arena, which is the strongest available statement of
 * "never published": the flush reaches guest memory only for a target it staged, and this one is
 * unreachable by construction (vb_target_for's 4096 cap could never produce this shape either, so
 * the target is wired by hand as the capacity seam itself). */
static void prop_over_cap(void)
{
    VbTarget *ts[1];
    VkDeviceSize off[1];
    VbTarget over_before;
    VbTarget *small;
    Nv2aVkReadbackStats b, a;
    RbWant want;
    uint64_t inc0, inc1;
    int chk = g_checks, bad = g_check_failures;

    section("13. a single over-cap dirty target is never staged: the flip stops, it does not skip");
    note("the synthetic target's dirty rectangle is %llu MiB, over the %llu MiB staging cap by "
         "itself, and its address (%#x) is outside the %llu MiB guest arena: its group can publish "
         "nothing, so the boundary must stop rather than drop it or copy part of it",
         (unsigned long long)(OVERCAP_SLICE >> 20), (unsigned long long)(VB_READBACK_MAX >> 20),
         (unsigned)OVERCAP_VA, (unsigned long long)(GATE_GUEST >> 20));

    gate_world();
    wire_target(&g.targets[0], OVERCAP_VA, OVERCAP_PITCH, BIG_BPP, 0, 0, OVERCAP_DX1, OVERCAP_DY1,
                OVERCAP_H);
    wire_target(&g.targets[1], T4_VA, T4_PITCH, T4_BPP, T4_DX0, T4_DY0, T4_DX1, T4_DY1, T4_H);
    over_before = g.targets[0];
    small = &g.targets[1];
    stage_target(small, 0, 0x40u);
    memcpy(g_pre, g_guest, GATE_GUEST);
    stats(&b);
    inc0 = stats_incomplete();

    vb_flip();

    stats(&a);
    inc1 = stats_incomplete();
    ts[0] = small;
    off[0] = 0;

    check(small->dx1 - small->dx0 == T4_RW && small->dy1 - small->dy0 == T4_RH,
          "the normal target is a small dirty rectangle that does fit the cap", "rect %ux%u",
          small->dx1 - small->dx0, small->dy1 - small->dy0);
    check(c.imgcopy == 1u && c.imgcopy_img == small->im.img && c.imgcopy_buf == g.rb_buf,
          "only the target that fits was staged: no command ever named the over-cap target",
          "vkCmdCopyImageToBuffer %u, image %p (small %p, over-cap %p), buffer %p", c.imgcopy,
          (void *)c.imgcopy_img, (void *)small->im.img, (void *)over_before.im.img,
          (void *)c.imgcopy_buf);
    check(c.begins == 1u && c.submits == 1u && c.waits == 1u && c.invalidates == 1u,
          "one batch published, and the retry recorded nothing: nothing else was stageable",
          "begins %u, submits %u, waits %u, invalidates %u", c.begins, c.submits, c.waits,
          c.invalidates);
    check_guest_published("13 the over-cap flip", ts, off, 1);
    check_dirty_stale("13 the over-cap flip", "the normal", small, 0, 0);
    check_dirty_stale("13 the over-cap flip", "the over-cap", &g.targets[0], 1, 0);
    check(g.targets[0].im.img == over_before.im.img
          && g.targets[0].im.view == over_before.im.view
          && g.targets[0].dx1 == OVERCAP_DX1 && g.targets[0].dy1 == OVERCAP_DY1
          && g.targets[0].va == over_before.va && g.targets[0].pitch == over_before.pitch,
          "the over-cap target is untouched: same image, same address, same dirty rectangle",
          "img %p (was %p), va %#x (was %#x), pitch %u (was %u), rect %u,%u",
          (void *)g.targets[0].im.img, (void *)over_before.im.img, (unsigned)g.targets[0].va,
          (unsigned)over_before.va, g.targets[0].pitch, over_before.pitch, g.targets[0].dx1,
          g.targets[0].dy1);
    check(g.frame == 0u, "the flip did NOT advance the frame: it stopped", "g.frame %u", g.frame);
    check_health("13 the over-cap flip", 0);
    memset(&want, 0, sizeof want);
    want.readbacks = 1;                 /* the small target */
    want.batches = 1;
    want.copied = 1;
    want.bytes = (uint64_t)T4_SLICE;
    check_counters("13 the over-cap flip", &b, &a, &want, 0);
    check_incomplete("13 the over-cap flip", inc0, inc1, 1);
    check(c.image_destroys == 0u && c.frees == 0u && c.buffer_destroys == 0u,
          "nothing was released while stopping", "image destroys %u, frees %u, buffer destroys %u",
          c.image_destroys, c.frees, c.buffer_destroys);

    verdict("13 the capacity seam: an over-cap target stops the boundary", chk, bad);
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

/* The fixture's stand-in for the executor's texture decoder. It records WHAT IT WAS SHOWN --
 * the guest bytes at the texture's own address, exactly as the production would read them --
 * because "a texture was decoded from guest bytes a live target superseded" is a statement
 * about those bytes and about nothing else. */
int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t n = tex->width * tex->height, i;

    c.decodes++;
    c.decoded_offset = tex->offset;
    c.decoded_span = tex->pitch * tex->height;
    c.decoded_hash = hash_bytes(g_guest + tex->offset, c.decoded_span);
    for (i = 0; i < n; i++)
        argb_out[i] = 0xFF000000u | ((i * 0x00010101u) & 0x00FFFFFFu);
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    c.spans++;
    return tex->pitch * tex->height;    /* the linear level-0 image, as this decoder reads it */
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

    printf("vk_readback_callers: the readback publication's coherence boundaries, as their "
           "CALLERS see them,\nagainst the toolkit's own nv2a_vk_backend.c\n");
    printf("no GPU, no ICD and no skip path: every Vulkan entry point the driven paths reach is "
           "a controlled stub,\nthe staging buffer vkMapMemory hands back is the fixture's own "
           "bytes, and both injected\nfailures are stub return values\n");

    g_guest = (uint8_t *)malloc(GATE_GUEST);
    g_pre = (uint8_t *)malloc(GATE_GUEST);
    g_expect = (uint8_t *)malloc(GATE_GUEST);
    g_stage = (uint8_t *)malloc(GATE_STAGE);
    for (i = 0; i < VB_SLOTS; i++)
        g_stream[i] = (uint8_t *)malloc(VB_SLOT_STREAM);
    if (!g_guest || !g_pre || !g_expect || !g_stage
        || !g_stream[0] || !g_stream[1] || !g_stream[2]) {
        fprintf(stderr, "FAIL: vk_readback_callers: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();

    prop0();
    prop_success();
    prop_descriptor(0);
    prop_descriptor(1);
    prop_staging(0);
    prop_staging(1);
    prop_target_for_reshape(0);
    prop_target_for_reshape(1);
    prop_target_for_evict(0);
    prop_target_for_evict(1);
    prop_texture(0);
    prop_texture(1);
    prop_split();
    prop_split_failure();
    prop_over_cap();

    for (i = 0; i < VB_SLOTS; i++)
        free(g_stream[i]);
    free(g_stage);
    free(g_expect);
    free(g_pre);
    free(g_guest);

    printf("\n");
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_readback_callers: %d of %d properties failed (%d of %d "
                        "checks failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_readback_callers: %d properties, %d checks -- a publication that fails is "
           "retried once and then stops the back end: no frame advance, no stale mark, no release, "
           "no decode, and no load of the guest bytes it never published; an over-cap group is "
           "split into bounded batches and an over-cap target stops the boundary\n", g_props,
           g_checks);
    return 0;
}
