/* N4.3 gate B2: the narrowed synchronisation of the toolkit's Vulkan back end, decided on a REAL
 * device -- the configured software ICD (lavapipe), through the CTest ENVIRONMENT -- from the
 * driver call stream the production back end itself records, and from the guest bytes its flip
 * publishes.
 *
 * What this case decides
 * ----------------------
 * N4.3 replaced the back end's two blanket barriers (`vb_barrier()` everywhere, and
 * `vb_image_barrier()` for every layout transition) with per-site scopes: a memory dependency at
 * each site names the stage and access of the producer and of the consumer it orders, and a
 * layout transition is recorded as `vb_image_dep()` with the transition's own scopes. Exactly two
 * call sites keep the deliberately broad `vb_barrier()` -- the per-submission barrier in
 * `vb_slot_submit` (the only thing that makes cross-submission ordering defined) and the
 * `if (!batch_ok)` abandonment branch of `vb_targets_flush` (ordered against a whole next batch).
 *
 * D1 reach -- the optimized paths were really driven: the private counters of this same
 *   translation unit (g.st, the public ring/readback accessors) show render-target loads and
 *   copies and aliases, texture uploads, packed and straight-copied readbacks, ring overlap and a
 *   completed frame per flip, with nothing incomplete and no failure. Each counter is a named
 *   check and a ZERO-valued counter FAILS: that is the "the optimized path was really reached"
 *   control, and the case also re-runs the whole predicate against a copy with each field zeroed
 *   in turn, so the control is shown to be load-bearing rather than asserted.
 * D2 bytes -- the guest bytes the flip publishes are exactly the fixture's own expected bytes:
 *   the 16-bit target's 5:6:5 pack (and the marker outside its partial-scissor rectangle), the
 *   depth-ordered pass (the nearer quad's colour), a 4x-magnified point-sampled texture (each
 *   texel's exact replicated block), and the self-sampling snapshot (the value derived from the
 *   pre-draw target content, which differs from both the clear colour and the untextured
 *   fallback). Every comparison is exact -- no tolerance, no percentage -- and the second frame's
 *   load-then-redraw is compared byte-for-byte with the first frame's publication.
 * D3 masks -- the recorded barrier stream contains, at least once, each changed site's exact
 *   (srcStage, srcAccess, dstStage, dstAccess) pair (S1..S9 below), with the resource the site
 *   names; the broad ALL_COMMANDS barrier appears exactly once per SUBMITTED command buffer, is
 *   the last barrier before that recording's vkEndCommandBuffer, is the only barrier whose source
 *   stage is ALL_COMMANDS, and no barrier of any kind is recorded while a render pass is open.
 *   The observed count per site is reported, and each site's count is tied to an independent
 *   quantity the production keeps itself (rt_loads, rt_copies, readback_packs, readback_batches,
 *   the number of vkCmdEndRenderPass calls, the number of live targets and depths), so a site
 *   whose barrier moved, changed scope or disappeared cannot pass by being merely present
 *   somewhere in the window.
 * D4 validation -- the counters the debug messenger keeps are reported (install-time and for the
 *   fixture window) and any validation ERROR fails the case. This case makes no claim about
 *   synchronization validation: the CTest ENVIRONMENT does not request a validation layer, so the
 *   counters are 0 by construction unless an external run (the packet owner's receipt) enables
 *   VK_INSTANCE_LAYERS, and the case says which of the two it is instead of implying the first.
 * D5 termination -- the whole case is bounded, prints its accounting and exits 0; the only skip
 *   (77) is a back end that could not be installed, with the reason printed.
 *
 * What the fixture drives, and what it does not
 * --------------------------------------------
 * The production bodies are this translation unit's: it #includes the toolkit's
 * src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so `nv2a_vk_backend_install()`,
 * `vb_clear`, `vb_draw`, `vb_flip`, `vb_target_create`, `vb_target_load`, `vb_depth_for`,
 * `vb_tex_upload`, `vb_texture`, `vb_targets_flush`, `vb_targets_flush_recover`,
 * `vb_create_defaults`, `vb_begin_pass`/`vb_end_pass`, `vb_cmd`/`vb_transfer_cmd` -- every body
 * under test is the
 * production one, verbatim, and the seam it binds to is checked at BUILD time by
 * scripts/vk-sync-paths-gate.py (a missing anchor fails the build; a missing marker is a printed
 * failed check, never a skip).
 *
 * Driven, through the back end's own entry points (the fixture supplies the decoded batches):
 *   - `nv2a_vk_backend_install()` -- for real: the production loader, instance, device, pipelines,
 *     samplers and 1x1 white texture, on the device RECOMP_VK_DEVICE names;
 *   - `vb_create_defaults()` a second time, from the fixture. The install's own commands are
 *     recorded by NOTHING: the recording forwarders below are installed after install() returns,
 *     so the 1x1 white texture's two layout transitions (S6) would otherwise be outside the
 *     window. The fixture re-drives the production function that records exactly them, and says so;
 *   - `vb_clear` -- the 16-bit target's partial-scissor clear (flags 0xF0, a known 5:6:5 word) and
 *     the 4-byte target's full clear that the self-sampling draw reads back;
 *   - `vb_draw` -- 100 degenerate (zero-area) batches per frame, whose descriptor sets are what
 *     makes the production's own vb_maybe_split() submit a slot (VB_SPLIT_SETS sets), plus the
 *     deciding draws: two overlapping depth-tested quads at different z on a 4-byte target, one
 *     pixel-aligned quad sampling a 4x4 texture magnified 4x with point filtering, and one quad
 *     drawn into the SAME target it samples (vb_texture's snapshot path);
 *   - `vb_depth_for` through the depth-tested draws (the depth-stencil image's creation);
 *   - `vb_targets_flush` / `vb_targets_flush_recover` through `vb_flip`, which publishes both
 *     dirty targets of every frame in one flush: the 16-bit one through the pack kernel and the
 *     4-byte one as a straight image copy.
 * NOT driven: `vb_draw_program` (no vertex-program batch), the combiner/rc path (every batch here
 * has combiner == NULL, so the stage-0 pipeline draws it), `vb_tex_aliases`' non-out branch (the
 * aliasing texture is always the target being drawn into), the release/eviction paths, the
 * coherence stop (`vb_stop`, `vb_readback_recover`, the removal path of the target caches) -- no
 * allocation or submission is made to fail here -- the dead command buffer of `vb_cmd` (nothing
 * is recorded into it, which D3's attribution check decides), and the abandonment branch of
 * `vb_targets_flush`: the retained broad R2 barrier is therefore NOT in this window, a fact D3
 * reports (the ALL_COMMANDS count must equal the number of submissions exactly) rather than
 * implies.
 *
 * The fixture is the decoder, and only the decoder: `nv2a_backend_decode_texture` fills the
 * deterministic 4x4 ARGB pattern below and `nv2a_backend_texture_span` returns the fixture's span,
 * exactly as the spec requires, so the texture path is real on the device while the GUEST TEXTURE
 * BYTES are the fixture's. Every other byte the case checks (the clears, the depth order, the
 * pack, the snapshot) is produced by the device itself and read back out of the guest arena the
 * fixture hands to `xbox_GetMemoryOffset()` -- which is the point of this gate: a wrong scope is a
 * wrong byte or a contract violation on a real driver, not a diff of two stub arguments.
 *
 * The recorders FORWARD, they do not swallow
 * ------------------------------------------
 * The five toolkit symbols this file's translation unit cannot link against are stubbed at the
 * bottom (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack). Every Vulkan entry point this case observes is a
 * static function pointer (VK_NO_PROTOTYPES) that `nv2a_vk_backend_install()` has already loaded
 * from the real loader and driver; after install each one is SAVED and a recording forwarder is
 * put in its place, which records the call and then calls the saved real pointer. Nothing is
 * swallowed and no Vulkan entry point is stubbed: the commands are the driver's own, and the
 * device executes them.
 *
 * The site table (read out of the production source; the macros are the file's own)
 * --------------------------------------------------------------------------------
 *   S1 vb_target_load, after vkCmdCopyBufferToImage
 *      TRANSFER / TRANSFER_WRITE           -> VB_STAGE_IMAGE_DST / VB_ACCESS_IMAGE_DST
 *   S2 vb_end_pass, after vkCmdEndRenderPass
 *      VB_STAGE_ATTACH / VB_ACCESS_ATTACH_WRITE -> VB_STAGE_IMAGE_DST / VB_ACCESS_IMAGE_DST
 *   S3 vb_targets_flush, after the pack vkCmdDispatch
 *      COMPUTE_SHADER / SHADER_WRITE        -> TRANSFER / TRANSFER_READ
 *   S4 vb_targets_flush, the publish barrier
 *      COMPUTE_SHADER|TRANSFER / SHADER_WRITE|TRANSFER_WRITE -> HOST / HOST_READ
 *   S5 vb_texture, after the self-snapshot vkCmdCopyImage
 *      TRANSFER / TRANSFER_WRITE            -> FRAGMENT_SHADER|COMPUTE_SHADER|TRANSFER /
 *                                              SHADER_READ|TRANSFER_READ|TRANSFER_WRITE
 *   S6 vb_tex_upload and vb_create_defaults, the two texture transitions
 *      TOP_OF_PIPE / 0, UNDEFINED            -> TRANSFER_DST_OPTIMAL: TRANSFER / TRANSFER_WRITE
 *      TRANSFER / TRANSFER_WRITE, TRANSFER_DST_OPTIMAL
 *                                            -> SHADER_READ_ONLY_OPTIMAL: FRAGMENT_SHADER|
 *                                               COMPUTE_SHADER / SHADER_READ
 *   S7 vb_target_create, after vkCmdClearColorImage
 *      TRANSFER / TRANSFER_WRITE            -> VB_STAGE_IMAGE_DST / VB_ACCESS_IMAGE_DST
 *   S8 vb_depth_for, after vkCmdClearDepthStencilImage
 *      TRANSFER / TRANSFER_WRITE            -> VB_STAGE_DEPTH / VB_ACCESS_DEPTH
 *   S9 vb_texture, the snapshot image's creation transition
 *      TOP_OF_PIPE / 0, UNDEFINED -> GENERAL: TRANSFER / TRANSFER_WRITE
 *   The target and depth creations' own transitions (UNDEFINED -> GENERAL, TOP_OF_PIPE / 0 ->
 *   TRANSFER / TRANSFER_WRITE) are the same scope pair as S9 on a different image, and are
 *   counted by role so the census names which image each transition was recorded on.
 *   R1 vb_slot_submit: the retained broad barrier, ALL_COMMANDS / MEMORY_WRITE ->
 *      ALL_COMMANDS / MEMORY_READ|MEMORY_WRITE, once per submitted recording and last in it.
 *   R2 vb_targets_flush's !batch_ok branch: the same function and the same masks -- NOT in this
 *      window (nothing is made to fail here), which is why the ALL_COMMANDS count must equal the
 *      number of submissions exactly: an extra one would mean that branch ran.
 * S1 and S7 share their scope pair and are separated structurally, by the command each one
 * follows (a load's copy versus a create's clear); every other pair is unique in the window, and
 * the window's ENTIRE census of memory-barrier scope pairs must be a subset of these, so a site
 * that started emitting some other pair fails rather than passing unnoticed.
 *
 * Which byte cases are byte-decided, and which are not
 * ---------------------------------------------------
 * Byte-decided (exact, every published byte of the rectangle compared against the fixture's own
 * plan, no tolerance):
 *   B1 the 16-bit target's packed publication (the clear word round-trips through the expansion
 *      and the pack kernel's truncation, so the expected halfword is the clear word itself);
 *   B2 the same target's bytes OUTSIDE its partial-scissor rectangle: they must still be the
 *      marker the fixture preloaded, because the publication writes the dirty rectangle only;
 *   B3 the 4-byte depth-ordered target: the nearer quad's colour inside its rectangle, the
 *      farther quad's colour outside it, both exactly the colours the fixture drew;
 *   B4 the 4x-magnified point-sampled texture: each 4x4 block is the exact replicated texel of the
 *      fixture's own decoded pattern (nearest filtering and pixel-centred sampling make this
 *      exact; the fixture also checks that the 16 blocks are distinct, so the expectation is
 *      load-bearing);
 *   B5 the self-sampling snapshot's output: the sampled pre-draw content masked by the draw's
 *      diffuse through factors of exactly 0 and 1, so the expected word is exact and differs from
 *      both the pre-draw content and the untextured-white fallback;
 *   B6 the second frame's load-then-redraw of the 16-bit target, compared byte-for-byte with the
 *      bytes the first frame published (and with B1's pattern).
 * Counter-only, and reported as such:
 *   C1 the ring overlap (max_inflight, splits, submissions) and the readback bookkeeping -- they
 *      are the production's own counters, not pixels;
 *   C2 the site census of D3 -- it decides masks and resources in the recorded stream, and says
 *      nothing about pixels;
 *   C3 the validation counters of D4 -- they count what a layer said, and are 0 when no layer ran;
 *   C4 the degenerate batches' contribution to the ring (they are what makes a frame split; that
 *      they write no fragment is byte-decided by B1/B2 for the target they are recorded into).
 * A case the fixture CANNOT compute exactly is not byte-checked: nothing in this file compares a
 * pixel against a formula it did not derive, and no case is reported as decided that is not.
 *
 * Bounded: three flips, 303 degenerate batches and 307 draw calls in total; the run is 0.2 s of
 * lavapipe and the case asserts its own elapsed time against a limit well inside the CTest
 * timeout of 600 s (the whole process, device creation included, is about 3 s).
 *
 * Expected verdicts, and what a changed scope does to them
 * -------------------------------------------------------
 * The source as it stands passes all six properties (119 checks). The gate was rebuilt against
 * private mutants of a COPY of the back end (never the shared tree) with the probe re-run on the
 * mutant, and each mutant went red where the table says it must:
 *   S2's destination dropping the sampler's stage/read (M1) -> D3: S2's class is empty and its
 *     count no longer matches the 13 vkCmdEndRenderPass calls;
 *   S1's source access becoming TRANSFER_READ (M2), S3's destination access becoming
 *     TRANSFER_WRITE only (M3), S4's source stage dropping TRANSFER (M4), S5's destination
 *     dropping the transfer read/write scopes (M5)  -> D3: that site's class is empty, its count
 *     no longer matches the production's own counter (rt_loads / readback_packs / readback_batches
 *     / rt_copies), and the whole-window census no longer contains only the table's pairs;
 *   S6's second transition (M6) or S7's layout transition (M7) removed -> the seam marker goes to
 *     0 AND D3's image-barrier census loses exactly that transition;
 *   S1/S7/S8 going back to vb_barrier() (M9) -> the seam markers AND D3 (the three classes empty,
 *     the ALL_COMMANDS count above the number of submissions);
 *   the publication's row memcpy losing the rectangle's column offset (M10) -> D2, byte for byte,
 *     with the barrier census still green: that is the case D3 cannot see;
 *   the draw entry never splitting the recording (M11) -> D1 (max_in_flight 1, splits 0).
 * A comment-only change (M8) stays green.
 */

#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */
#include "vk_sync_paths_gate_seam.h"            /* derived from the same source at build time */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Reporting ──────────────────────────────────────────────────────────────────────── */

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
    printf("      %-70s %s", what, cond ? "ok" : "FAIL");
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
    printf("      %-70s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

/* ── The fixture's geometry and its expected bytes ──────────────────────────────────── */

#define GT_FLIPS   3u

/* TB: the 16-bit (R5G6B5) target, cleared through a partial scissor. */
#define TB_VA      0x00010000u
#define TB_PITCH   32u
#define TB_W       16u
#define TB_H       16u
#define TB_BYTES   ((size_t)TB_PITCH * TB_H)
#define TB_CX      4u
#define TB_CY      4u
#define TB_CW      8u
#define TB_CH      8u
#define TB_CLEAR   0x000020E4u      /* R5G6B5 in the surface's own format */
#define TB_EXPECT  0x20E4u          /* what the pack kernel must publish for it */

/* TA: the 4-byte target of the depth-ordered pass. */
#define TA_VA      0x00020000u
#define TA_PITCH   64u
#define TA_W       16u
#define TA_H       16u
#define TA_BYTES   ((size_t)TA_PITCH * TA_H)
#define TA_FAR     0xFF0000FFu      /* the quad drawn first, at the greater z */
#define TA_NEAR    0xFF00FF00u      /* the qaud drawn second, at the smaller z, left half */
#define TA_ZFAR    8000000.0f
#define TA_ZNEAR   4000000.0f
#define TA_NEAR_X  8u               /* pixels [0,8) are covered by the near quad */

/* TT: the 4-byte target the magnified texture is drawn into. */
#define TT_VA      0x00030000u
#define TT_PITCH   64u
#define TT_W       16u
#define TT_H       16u
#define TT_BYTES   ((size_t)TT_PITCH * TT_H)

/* TS: the 4-byte target that samples itself through vb_texture's snapshot. */
#define TS_VA      0x00040000u
#define TS_PITCH   64u
#define TS_W       16u
#define TS_H       16u
#define TS_BYTES   ((size_t)TS_PITCH * TS_H)
#define TS_CLEAR   0xFFC08040u      /* the pre-draw content the snapshot copies */
#define TS_DIFFUSE 0xFF00FF00u      /* mask: only G and A pass, through factors 1 and 0 */
#define TS_EXPECT  0xFF008000u      /* = TS_CLEAR masked by TS_DIFFUSE */

/* The texture source: 4x4 A8R8G8B8 linear, magnified 4x, sampled with point filtering. */
#define TEX_VA     0x00050000u
#define TEX_W      4u
#define TEX_H      4u
#define TEX_PITCH  16u
#define TEX_MAG    4u               /* TT_W / TEX_W: each texel covers a 4x4 block */
#define TEX_BYTES  ((size_t)TEX_PITCH * TEX_H)

#define ZETA_VA    0x00060000u
#define GT_ARENA   0x00080000u      /* the flat guest arena the fixture hands over */

#define MARKER     0x5Au            /* preloaded outside the published rectangle of TB */
#define MARKER32   0x5A5A5A5Au

/* The ring-overlap draws. VB_SPLIT_SETS is the production's own threshold for a draw entry to
   submit the recording slot, so a frame needs more batches than that for vb_maybe_split to fire;
   100 of them makes at least two splits a frame on a three-slot ring. */
#define GT_PAD_DRAWS   100u
#define GT_PAD_VERTS   6u           /* two zero-area triangles: a batch, and no fragment */

/* the target index the checks use */
#define GT_TB 0
#define GT_TA 1
#define GT_TT 2
#define GT_TS 3
#define GT_NT 4

static const char *const gt_tname[GT_NT] = {
    "TB 16-bit", "TA depth", "TT texture", "TS self-sample"
};
static const uint32_t gt_tva[GT_NT] = { TB_VA, TA_VA, TT_VA, TS_VA };
static const uint32_t gt_tpitch[GT_NT] = { TB_PITCH, TA_PITCH, TT_PITCH, TS_PITCH };
static const uint32_t gt_tbpp[GT_NT] = { 2u, 4u, 4u, 4u };
static const uint32_t gt_tw[GT_NT] = { TB_W, TA_W, TT_W, TS_W };
static const uint32_t gt_th[GT_NT] = { TB_H, TA_H, TT_H, TS_H };
static const size_t gt_tbytes[GT_NT] = { TB_BYTES, TA_BYTES, TT_BYTES, TS_BYTES };

/* The fixture's decoded texel, exactly what nv2a_backend_decode_texture below hands the back end
   (an 0xAARRGGBB word, opaque, distinct in every channel and every texel). */
static uint32_t gt_tex_argb(uint32_t x, uint32_t y)
{
    return 0xFF000000u | ((0x10u * x + 0x37u) << 16) | ((0x20u * y + 0x1Bu) << 8)
         | (0x20u * x + 0x11u * y + 0x05u);
}

/* The guest word a published rectangle must hold at (x, y): the fixture's own plan, in the
   guest's format. Nothing here is derived from what the device produced. */
static uint32_t gt_expect_word(int which, uint32_t x, uint32_t y)
{
    switch (which) {
    case GT_TB: return TB_EXPECT;
    case GT_TA: return x < TA_NEAR_X ? TA_NEAR : TA_FAR;
    case GT_TT: return gt_tex_argb(x / TEX_MAG, y / TEX_MAG);
    case GT_TS: return TS_EXPECT;
    }
    return 0u;
}

/* ── The recorded driver call stream ────────────────────────────────────────────────── */

enum {
    GOP_PIPELINE_BARRIER = 1, GOP_IMAGE_BARRIER, GOP_BEGIN_PASS, GOP_END_PASS, GOP_BEGIN_CMD,
    GOP_END_CMD, GOP_RESET_CMD, GOP_SUBMIT, GOP_WAIT_FENCE, GOP_COPY_B2I, GOP_COPY_I2B,
    GOP_COPY_BUFFER, GOP_COPY_IMAGE, GOP_CLEAR_COLOR, GOP_CLEAR_DEPTH, GOP_DISPATCH, GOP_DRAW,
    GOP_DRAW_INDEXED
};

#define GT_MAX_OPS  8192
#define GT_MAX_RECS 128

typedef struct {
    unsigned seq;                   /* 1-based; seq - 1 is the array index */
    int op;
    VkCommandBuffer cb;
    unsigned rec;                   /* the recording (vkBeginCommandBuffer..vkEndCommandBuffer) */
    int pass_open;                  /* render passes open when this call was recorded */
    VkPipelineStageFlags src_stage, dst_stage;
    VkAccessFlags src_access, dst_access;
    VkDependencyFlags dep_flags;
    uint32_t mem_bars, buf_bars, img_bars;
    VkImage image;
    VkImageAspectFlags aspect;
    VkImageLayout old_layout, new_layout;
    uint32_t level_count, layer_count;
    int32_t queue_family;           /* -1: VK_QUEUE_FAMILY_IGNORED; -2: the two differ */
} GateOp;

static GateOp gt_ops[GT_MAX_OPS];
static unsigned gt_nops;
static int gt_ops_full;
static unsigned gt_cur_rec;                     /* 1-based recording index; 0: none begun */
static VkCommandBuffer gt_rec_cb[GT_MAX_RECS + 1];
static int gt_rec_ended[GT_MAX_RECS + 1], gt_rec_submitted[GT_MAX_RECS + 1];
static unsigned gt_pass_open, gt_pass_begins, gt_pass_ends_edge;
static unsigned gt_submits, gt_waits, gt_begins, gt_ends, gt_resets;

static GateOp *gt_add(int op, VkCommandBuffer cb)
{
    GateOp *o;

    if (gt_nops >= GT_MAX_OPS) {
        gt_ops_full = 1;
        return NULL;
    }
    o = &gt_ops[gt_nops];
    memset(o, 0, sizeof *o);
    o->seq = ++gt_nops;
    o->op = op;
    o->cb = cb;
    o->rec = gt_cur_rec;
    o->pass_open = (int)gt_pass_open;
    o->queue_family = -3;
    return o;
}

/* The last recorded op before `o` that is not a barrier, within the same recording: what the
   barrier follows is how the two sites that share a scope pair (S1's load and S7's create) are
   told apart, and it is also what makes "the broad barrier is last in its recording" a statement
   about the stream rather than about a count. */
static int gt_prev_op(const GateOp *o)
{
    size_t i = (size_t)(o - gt_ops);

    while (i > 0) {
        const GateOp *p = &gt_ops[--i];
        if (p->rec != o->rec)
            continue;
        if (p->op == GOP_PIPELINE_BARRIER || p->op == GOP_IMAGE_BARRIER)
            continue;
        return p->op;
    }
    return 0;
}

static unsigned gt_count_mem(VkPipelineStageFlags ss, VkAccessFlags sa,
                             VkPipelineStageFlags ds, VkAccessFlags da, int prev_op)
{
    unsigned i, n = 0;

    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        if (o->op != GOP_PIPELINE_BARRIER || o->mem_bars != 1u || o->img_bars != 0u)
            continue;
        if (o->src_stage != ss || o->dst_stage != ds)
            continue;
        if (o->src_access != sa || o->dst_access != da)
            continue;
        if (prev_op && gt_prev_op(o) != prev_op)
            continue;
        n++;
    }
    return n;
}

static const char *gt_role(VkImage img, char *buf, size_t n);

/* The image-barrier count of one class, restricted to the image a role names. The match is on the
   role's PREFIX, so "target va=" cannot also match the snapshot image's role string ("snapshot of
   target va=") and every transition is attributed to exactly one class. */
static unsigned gt_count_img_role(VkImageLayout from, VkImageLayout to, const char *role_prefix)
{
    unsigned i, n = 0;
    char role[96];

    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        if (o->op != GOP_IMAGE_BARRIER)
            continue;
        if (o->old_layout != from || o->new_layout != to)
            continue;
        gt_role(o->image, role, sizeof role);
        if (strncmp(role, role_prefix, strlen(role_prefix)) != 0)
            continue;
        n++;
    }
    return n;
}

static unsigned gt_count_broad(void)
{
    unsigned i, n = 0;

    for (i = 0; i < gt_nops; i++)
        if ((gt_ops[i].op == GOP_PIPELINE_BARRIER || gt_ops[i].op == GOP_IMAGE_BARRIER)
            && (gt_ops[i].src_stage & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT))
            n++;
    return n;
}

/* Which of the back end's images is this? Read out of the production's own handle tables, so the
   census names the resource a transition was recorded on instead of counting tuples blindly. */
static const char *gt_role(VkImage img, char *buf, size_t n)
{
    unsigned i;

    if (img == VK_NULL_HANDLE) {
        snprintf(buf, n, "VK_NULL_HANDLE");
        return buf;
    }
    if (g.white.img == img) {
        snprintf(buf, n, "the 1x1 white texture");
        return buf;
    }
    for (i = 0; i < VB_MAX_TARGETS; i++) {
        const VbTarget *t = &g.targets[i];
        if (!t->used)
            continue;
        if (t->im.img == img) {
            snprintf(buf, n, "target va=%u", (unsigned)t->va);
            return buf;
        }
        if (t->copy.img == img) {
            snprintf(buf, n, "snapshot of target va=%u", (unsigned)t->va);
            return buf;
        }
    }
    for (i = 0; i < VB_MAX_DEPTHS; i++)
        if (g.depths[i].used && g.depths[i].im.img == img) {
            snprintf(buf, n, "depth va=%u", (unsigned)g.depths[i].va);
            return buf;
        }
    for (i = 0; i < VB_MAX_TEXTURES; i++)
        if (g.texs[i].used && g.texs[i].im.img == img) {
            snprintf(buf, n, "texture va=%u", (unsigned)g.texs[i].offset);
            return buf;
        }
    snprintf(buf, n, "an image the fixture does not know");
    return buf;
}

static VbTarget *gt_target_of(uint32_t va)
{
    unsigned i;

    for (i = 0; i < VB_MAX_TARGETS; i++)
        if (g.targets[i].used && g.targets[i].va == va)
            return &g.targets[i];
    return NULL;
}

/* ── The recording forwarders: record, then call the saved real entry point ─────────── */

static PFN_vkCmdPipelineBarrier real_vkCmdPipelineBarrier;
static PFN_vkCmdDraw real_vkCmdDraw;
static PFN_vkCmdDrawIndexed real_vkCmdDrawIndexed;
static PFN_vkCmdDispatch real_vkCmdDispatch;
static PFN_vkCmdCopyImageToBuffer real_vkCmdCopyImageToBuffer;
static PFN_vkCmdCopyBuffer real_vkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage real_vkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage real_vkCmdCopyImage;
static PFN_vkCmdClearColorImage real_vkCmdClearColorImage;
static PFN_vkCmdClearDepthStencilImage real_vkCmdClearDepthStencilImage;
static PFN_vkQueueSubmit real_vkQueueSubmit;
static PFN_vkWaitForFences real_vkWaitForFences;
static PFN_vkCmdBeginRenderPass real_vkCmdBeginRenderPass;
static PFN_vkCmdEndRenderPass real_vkCmdEndRenderPass;
static PFN_vkBeginCommandBuffer real_vkBeginCommandBuffer;
static PFN_vkEndCommandBuffer real_vkEndCommandBuffer;
static PFN_vkResetCommandBuffer real_vkResetCommandBuffer;

static VKAPI_ATTR void VKAPI_CALL gt_pipeline_barrier(
    VkCommandBuffer cb, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage,
    VkDependencyFlags flags, uint32_t mem_n, const VkMemoryBarrier *mem, uint32_t buf_n,
    const VkBufferMemoryBarrier *buf, uint32_t img_n, const VkImageMemoryBarrier *img)
{
    GateOp *o = gt_add(GOP_PIPELINE_BARRIER, cb);
    uint32_t i;

    if (o) {
        o->src_stage = src_stage;
        o->dst_stage = dst_stage;
        o->dep_flags = flags;
        o->mem_bars = mem_n;
        o->buf_bars = buf_n;
        o->img_bars = img_n;
        if (mem_n == 1u && mem) {
            o->src_access = mem[0].srcAccessMask;
            o->dst_access = mem[0].dstAccessMask;
        }
        if (buf_n == 1u && buf) {           /* recorded so an unexpected shape cannot hide */
            o->src_access = buf[0].srcAccessMask;
            o->dst_access = buf[0].dstAccessMask;
        }
    }
    for (i = 0; i < img_n && img; i++) {
        GateOp *io = gt_add(GOP_IMAGE_BARRIER, cb);
        if (!io)
            break;
        io->src_stage = src_stage;
        io->dst_stage = dst_stage;
        io->dep_flags = flags;
        io->mem_bars = mem_n;
        io->buf_bars = buf_n;
        io->img_bars = img_n;
        io->src_access = img[i].srcAccessMask;
        io->dst_access = img[i].dstAccessMask;
        io->image = img[i].image;
        io->aspect = img[i].subresourceRange.aspectMask;
        io->old_layout = img[i].oldLayout;
        io->new_layout = img[i].newLayout;
        io->level_count = img[i].subresourceRange.levelCount;
        io->layer_count = img[i].subresourceRange.layerCount;
        io->queue_family = img[i].srcQueueFamilyIndex == img[i].dstQueueFamilyIndex
                         ? (int32_t)img[i].srcQueueFamilyIndex : -2;
    }
    real_vkCmdPipelineBarrier(cb, src_stage, dst_stage, flags, mem_n, mem, buf_n, buf, img_n, img);
}

static VKAPI_ATTR void VKAPI_CALL gt_begin_pass(VkCommandBuffer cb,
                                                const VkRenderPassBeginInfo *bi,
                                                VkSubpassContents contents)
{
    gt_add(GOP_BEGIN_PASS, cb);
    gt_pass_begins++;
    real_vkCmdBeginRenderPass(cb, bi, contents);
    gt_pass_open++;
}

static VKAPI_ATTR void VKAPI_CALL gt_end_pass(VkCommandBuffer cb)
{
    gt_add(GOP_END_PASS, cb);
    real_vkCmdEndRenderPass(cb);
    if (gt_pass_open)
        gt_pass_open--;
    gt_pass_ends_edge++;
}

static VKAPI_ATTR VkResult VKAPI_CALL gt_begin_cmd(VkCommandBuffer cb,
                                                   const VkCommandBufferBeginInfo *bi)
{
    if (gt_cur_rec < GT_MAX_RECS) {
        gt_cur_rec++;
        gt_rec_cb[gt_cur_rec] = cb;
        gt_rec_ended[gt_cur_rec] = 0;
        gt_rec_submitted[gt_cur_rec] = 0;
    } else {
        gt_cur_rec = 0;                 /* past the table: every op is then visibly unattributed */
    }
    gt_begins++;
    gt_add(GOP_BEGIN_CMD, cb);
    return real_vkBeginCommandBuffer(cb, bi);
}

static VKAPI_ATTR VkResult VKAPI_CALL gt_end_cmd(VkCommandBuffer cb)
{
    gt_add(GOP_END_CMD, cb);
    if (gt_cur_rec)
        gt_rec_ended[gt_cur_rec] = 1;
    gt_ends++;
    return real_vkEndCommandBuffer(cb);
}

static VKAPI_ATTR VkResult VKAPI_CALL gt_reset_cmd(VkCommandBuffer cb,
                                                   VkCommandBufferResetFlags flags)
{
    gt_resets++;
    gt_add(GOP_RESET_CMD, cb);
    return real_vkResetCommandBuffer(cb, flags);
}

static VKAPI_ATTR VkResult VKAPI_CALL gt_submit(VkQueue queue, uint32_t count,
                                                const VkSubmitInfo *submits, VkFence fence)
{
    unsigned r = 0;

    gt_submits++;
    if (count && submits && submits[0].commandBufferCount && submits[0].pCommandBuffers) {
        VkCommandBuffer cb = submits[0].pCommandBuffers[0];
        GateOp *o = gt_add(GOP_SUBMIT, cb);
        unsigned i;

        for (i = gt_cur_rec; i > 0; i--)
            if (gt_rec_cb[i] == cb) {
                r = i;
                break;
            }
        if (o)
            o->rec = r;
        if (r)
            gt_rec_submitted[r] = 1;
    } else {
        gt_add(GOP_SUBMIT, VK_NULL_HANDLE);
    }
    return real_vkQueueSubmit(queue, count, submits, fence);
}

static VKAPI_ATTR VkResult VKAPI_CALL gt_wait_fence(VkDevice dev, uint32_t count,
                                                    const VkFence *fences, VkBool32 all,
                                                    uint64_t timeout)
{
    gt_waits++;
    gt_add(GOP_WAIT_FENCE, VK_NULL_HANDLE);
    return real_vkWaitForFences(dev, count, fences, all, timeout);
}

static VKAPI_ATTR void VKAPI_CALL gt_draw(VkCommandBuffer cb, uint32_t count, uint32_t instances,
                                          uint32_t first, uint32_t first_instance)
{
    gt_add(GOP_DRAW, cb);
    real_vkCmdDraw(cb, count, instances, first, first_instance);
}

static VKAPI_ATTR void VKAPI_CALL gt_draw_indexed(VkCommandBuffer cb, uint32_t count,
                                                  uint32_t instances, uint32_t first,
                                                  int32_t vertex_offset, uint32_t first_instance)
{
    gt_add(GOP_DRAW_INDEXED, cb);
    real_vkCmdDrawIndexed(cb, count, instances, first, vertex_offset, first_instance);
}

static VKAPI_ATTR void VKAPI_CALL gt_dispatch(VkCommandBuffer cb, uint32_t x, uint32_t y,
                                              uint32_t z)
{
    gt_add(GOP_DISPATCH, cb);
    real_vkCmdDispatch(cb, x, y, z);
}

static VKAPI_ATTR void VKAPI_CALL gt_copy_image_to_buffer(VkCommandBuffer cb, VkImage src,
                                                          VkImageLayout layout, VkBuffer dst,
                                                          uint32_t n,
                                                          const VkBufferImageCopy *regions)
{
    gt_add(GOP_COPY_I2B, cb);
    real_vkCmdCopyImageToBuffer(cb, src, layout, dst, n, regions);
}

static VKAPI_ATTR void VKAPI_CALL gt_copy_buffer(VkCommandBuffer cb, VkBuffer src, VkBuffer dst,
                                                 uint32_t n, const VkBufferCopy *regions)
{
    gt_add(GOP_COPY_BUFFER, cb);
    real_vkCmdCopyBuffer(cb, src, dst, n, regions);
}

static VKAPI_ATTR void VKAPI_CALL gt_copy_buffer_to_image(VkCommandBuffer cb, VkBuffer src,
                                                          VkImage dst, VkImageLayout layout,
                                                          uint32_t n,
                                                          const VkBufferImageCopy *regions)
{
    gt_add(GOP_COPY_B2I, cb);
    real_vkCmdCopyBufferToImage(cb, src, dst, layout, n, regions);
}

static VKAPI_ATTR void VKAPI_CALL gt_copy_image(VkCommandBuffer cb, VkImage src,
                                                VkImageLayout src_layout, VkImage dst,
                                                VkImageLayout dst_layout, uint32_t n,
                                                const VkImageCopy *regions)
{
    gt_add(GOP_COPY_IMAGE, cb);
    real_vkCmdCopyImage(cb, src, src_layout, dst, dst_layout, n, regions);
}

static VKAPI_ATTR void VKAPI_CALL gt_clear_color(VkCommandBuffer cb, VkImage img,
                                                 VkImageLayout layout,
                                                 const VkClearColorValue *color, uint32_t n,
                                                 const VkImageSubresourceRange *ranges)
{
    gt_add(GOP_CLEAR_COLOR, cb);
    real_vkCmdClearColorImage(cb, img, layout, color, n, ranges);
}

static VKAPI_ATTR void VKAPI_CALL gt_clear_depth(VkCommandBuffer cb, VkImage img,
                                                 VkImageLayout layout,
                                                 const VkClearDepthStencilValue *value, uint32_t n,
                                                 const VkImageSubresourceRange *ranges)
{
    gt_add(GOP_CLEAR_DEPTH, cb);
    real_vkCmdClearDepthStencilImage(cb, img, layout, value, n, ranges);
}

/* Save each real entry point the install loaded and put the forwarder in its place. Nothing is
   installed before the install itself: the commands of the install (#everything up to
   vb_create_defaults' own submit) are therefore NOT in the recorded window, which is why the
   fixture re-drives vb_create_defaults below. */
static int gt_wire(void)
{
    int ok = 1;

#define GT_WRAP(fld, rec) \
    do { \
        if (!(fld)) { \
            fprintf(stderr, "FAIL: vk_sync_paths_gate: the installed back end loaded no %s\n", \
                    #fld); \
            ok = 0; \
        } else { \
            real_##fld = (fld); \
            (fld) = (rec); \
        } \
    } while (0)

    GT_WRAP(vkCmdPipelineBarrier, gt_pipeline_barrier);
    GT_WRAP(vkCmdBeginRenderPass, gt_begin_pass);
    GT_WRAP(vkCmdEndRenderPass, gt_end_pass);
    GT_WRAP(vkBeginCommandBuffer, gt_begin_cmd);
    GT_WRAP(vkEndCommandBuffer, gt_end_cmd);
    GT_WRAP(vkResetCommandBuffer, gt_reset_cmd);
    GT_WRAP(vkQueueSubmit, gt_submit);
    GT_WRAP(vkWaitForFences, gt_wait_fence);
    GT_WRAP(vkCmdDraw, gt_draw);
    GT_WRAP(vkCmdDrawIndexed, gt_draw_indexed);
    GT_WRAP(vkCmdDispatch, gt_dispatch);
    GT_WRAP(vkCmdCopyImageToBuffer, gt_copy_image_to_buffer);
    GT_WRAP(vkCmdCopyBuffer, gt_copy_buffer);
    GT_WRAP(vkCmdCopyBufferToImage, gt_copy_buffer_to_image);
    GT_WRAP(vkCmdCopyImage, gt_copy_image);
    GT_WRAP(vkCmdClearColorImage, gt_clear_color);
    GT_WRAP(vkCmdClearDepthStencilImage, gt_clear_depth);
#undef GT_WRAP
    if (!real_vkCmdPipelineBarrier)
        ok = 0;
    return ok;
}

/* ── The fixture ────────────────────────────────────────────────────────────────────── */

static uint8_t *gt_arena;
static Nv2aVertex gt_v[8];
static Nv2aBatch gt_b;
static Nv2aRenderState gt_rs_plain, gt_rs_depth;
static Nv2aSurface gt_s[GT_NT];
static Nv2aTexture gt_tx_src, gt_tx_alias;
static uint8_t *gt_cap_tb1, *gt_cap_ta1;
static int gt_defaults_rc;
static unsigned gt_draw_calls, gt_clear_calls, gt_pad_draws_seen;
static uint64_t gt_t0, gt_elapsed_ns;

typedef struct { uint32_t x0, y0, x1, y1; int live; } GateRect;
static GateRect gt_rect[GT_FLIPS][GT_NT];

static void gt_states(void)
{
    memset(&gt_rs_plain, 0, sizeof gt_rs_plain);
    gt_rs_plain.color_mask = 0x01010101u;
    gt_rs_plain.blend_eq = 0x8006u;
    gt_rs_plain.blend_src = 1u;
    gt_rs_plain.blend_dst = 0u;
    gt_rs_plain.alpha_func = 0x207u;
    gt_rs_plain.depth_func = 0x207u;
    gt_rs_plain.stencil_func = 0x207u;
    gt_rs_plain.stencil_write_mask = gt_rs_plain.stencil_read_mask = 0xFFu;
    gt_rs_plain.cull_face = 0x405u;
    gt_rs_plain.front_face = 0x900u;
    gt_rs_plain.depth_min = 0.0f;
    gt_rs_plain.depth_max = 16777215.0f;

    gt_rs_depth = gt_rs_plain;
    gt_rs_depth.zeta_va = ZETA_VA;
    gt_rs_depth.depth_test_enable = 1u;
    gt_rs_depth.depth_func = 0x201u;        /* GL_LESS */
    gt_rs_depth.depth_write = 1u;
}

static Nv2aSurface gt_surface(uint32_t va, uint32_t w, uint32_t h, uint32_t pitch, uint32_t bpp,
                              uint32_t fmt, uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch)
{
    Nv2aSurface s;

    memset(&s, 0, sizeof s);
    s.color_va = va;
    s.width = cw;
    s.height = ch;
    s.pitch = pitch;
    s.bytes_per_pixel = bpp;
    s.color_format = fmt;
    s.aa_sx = s.aa_sy = 1u;
    s.clip_x = cx;
    s.clip_y = cy;
    return s;
}

static void gt_fixture(void)
{
    unsigned i;

    gt_states();
    gt_s[GT_TB] = gt_surface(TB_VA, TB_W, TB_H, TB_PITCH, 2u, 3u, TB_CX, TB_CY, TB_CW, TB_CH);
    gt_s[GT_TA] = gt_surface(TA_VA, TA_W, TA_H, TA_PITCH, 4u, 8u, 0u, 0u, TA_W, TA_H);
    gt_s[GT_TT] = gt_surface(TT_VA, TT_W, TT_H, TT_PITCH, 4u, 8u, 0u, 0u, TT_W, TT_H);
    gt_s[GT_TS] = gt_surface(TS_VA, TS_W, TS_H, TS_PITCH, 4u, 8u, 0u, 0u, TS_W, TS_H);

    /* the 4x4 A8R8G8B8 source the decoder stub fills, and the same texture read back as a live
       render target (TS samples itself through vb_texture's snapshot) */
    memset(&gt_tx_src, 0, sizeof gt_tx_src);
    gt_tx_src.offset = TEX_VA;
    gt_tx_src.width = TEX_W;
    gt_tx_src.height = TEX_H;
    gt_tx_src.pitch = TEX_PITCH;
    gt_tx_src.color = 0x12u;                /* LIN_A8R8G8B8 */
    gt_tx_src.addr_u = gt_tx_src.addr_v = 3u;
    gt_tx_src.filter = 0x01010000u;         /* box/box: point sampling */
    gt_tx_src.linear = 1u;

    gt_tx_alias = gt_tx_src;
    gt_tx_alias.offset = TS_VA;
    gt_tx_alias.width = TS_W;
    gt_tx_alias.height = TS_H;
    gt_tx_alias.pitch = TS_PITCH;

    gt_arena = (uint8_t *)malloc(GT_ARENA);
    gt_cap_tb1 = (uint8_t *)malloc(TB_BYTES);
    gt_cap_ta1 = (uint8_t *)malloc(TA_BYTES);
    if (!gt_arena || !gt_cap_tb1 || !gt_cap_ta1)
        return;

    memset(gt_arena, MARKER, GT_ARENA);
    /* the texture source's guest bytes: the decoder stub does not read them, but the back end
       hashes them on the first use after a flip, so they must be a stable pattern */
    for (i = 0; i < TEX_W * TEX_H; i++) {
        uint32_t v = gt_tex_argb(i % TEX_W, i / TEX_W);
        memcpy(gt_arena + TEX_VA + (size_t)i * 4u, &v, 4u);
    }
}

static uint32_t gt_quad(Nv2aVertex *v, float x0, float y0, float x1, float y1, float z,
                        uint32_t diffuse, float u0, float v0, float u1, float v1)
{
    static const float cp[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
    unsigned i;

    for (i = 0; i < 6; i++) {
        memset(&v[i], 0, sizeof v[i]);
        v[i].x = cp[i][0] ? x1 : x0;
        v[i].y = cp[i][1] ? y1 : y0;
        v[i].z = z;
        v[i].rhw = 1.0f;
        v[i].diffuse = diffuse;
        v[i].u = cp[i][0] ? u1 : u0;
        v[i].v = cp[i][1] ? v1 : v0;
    }
    return 6u;
}

static void gt_draw_quad(const Nv2aSurface *s, const Nv2aRenderState *rs, const Nv2aTexture *tx,
                         float x0, float y0, float x1, float y1, float z, uint32_t diffuse,
                         float u0, float v0, float u1, float v1)
{
    memset(&gt_b, 0, sizeof gt_b);
    gt_b.vertices = gt_v;
    gt_b.count = gt_quad(gt_v, x0, y0, x1, y1, z, diffuse, u0, v0, u1, v1);
    gt_b.texture = tx;
    gt_b.state = rs;
    vb_draw(s, &gt_b);
    gt_draw_calls++;
}

/* Zero-area batches: real batches through the real draw entry, whose descriptor set is what makes
   the production's own vb_maybe_split submit the slot. They draw no fragment, which B1/B2 of D2
   decide for the target they are recorded into. */
static void gt_pad_draws(const Nv2aSurface *s, const Nv2aRenderState *rs, unsigned n)
{
    unsigned i;

    for (i = 0; i < n; i++) {
        gt_draw_quad(s, rs, NULL, 8.0f, 8.0f, 8.0f, 8.0f, 0.5f, 0u, 0.0f, 0.0f, 0.0f, 0.0f);
        gt_pad_draws_seen++;
    }
}

static void gt_capture(uint32_t va, size_t bytes, uint8_t *dst)
{
    memcpy(dst, gt_arena + va, bytes);
}

static void gt_capture_rects(unsigned frame)
{
    unsigned i;

    for (i = 0; i < GT_NT; i++) {
        VbTarget *t = gt_target_of(gt_tva[i]);
        GateRect *r = &gt_rect[frame][i];

        if (!t) {
            r->live = 0;
            continue;
        }
        r->live = 1;
        r->x0 = t->dx0;
        r->y0 = t->dy0;
        r->x1 = t->dx1;
        r->y1 = t->dy1;
    }
}

/* The whole fixture run: three flips, each publishing the 16-bit target (a pack) and one 4-byte
   target (a straight copy) in the same flush. */
static void gt_run(void)
{
    unsigned f;

    /* The white texture's two transitions (S6) are recorded by the install, before the forwarders
       exist. Re-drive the production function that records exactly them, so the site is in the
       window; it sets g.ok to 0 on its way out (that is the install hand-off), so restore it. */
    gt_defaults_rc = vb_create_defaults();
    g.ok = 1;

    for (f = 0; f < GT_FLIPS; f++) {
        gt_pad_draws(&gt_s[GT_TB], &gt_rs_plain, GT_PAD_DRAWS);
        vb_clear(&gt_s[GT_TB], &gt_rs_plain, 0xF0u, TB_CLEAR, 0u);
        gt_clear_calls++;
        gt_pad_draws(&gt_s[GT_TB], &gt_rs_plain, 1u);

        if (f == 0u) {
            /* the depth-ordered pass: the far quad over the whole target, then the near quad
               over its left half, both under LESS with depth writes on */
            gt_draw_quad(&gt_s[GT_TA], &gt_rs_depth, NULL, 0.0f, 0.0f, 16.0f, 16.0f, TA_ZFAR,
                         TA_FAR, 0.0f, 0.0f, 0.0f, 0.0f);
            gt_draw_quad(&gt_s[GT_TA], &gt_rs_depth, NULL, 0.0f, 0.0f, 8.0f, 16.0f, TA_ZNEAR,
                         TA_NEAR, 0.0f, 0.0f, 0.0f, 0.0f);
        } else if (f == 1u) {
            /* the magnified point-sampled texture: 4 texels across 16 pixels, uv in texels */
            gt_draw_quad(&gt_s[GT_TT], &gt_rs_plain, &gt_tx_src, 0.0f, 0.0f, 16.0f, 16.0f, 0.5f,
                         0xFFFFFFFFu, 0.0f, 0.0f, (float)TEX_W, (float)TEX_H);
        } else {
            /* the self-sampling target: a full clear, then a draw that samples the snapshot of
               exactly that cleared content through vb_texture's alias path */
            vb_clear(&gt_s[GT_TS], &gt_rs_plain, 0xF0u, TS_CLEAR, 0u);
            gt_clear_calls++;
            gt_draw_quad(&gt_s[GT_TS], &gt_rs_plain, &gt_tx_alias, 0.0f, 0.0f, 16.0f, 16.0f,
                         0.5f, TS_DIFFUSE, 0.0f, 0.0f, (float)TS_W, (float)TS_H);
        }

        gt_capture_rects(f);
        vb_flip();
        if (f == 0u) {
            gt_capture(TB_VA, TB_BYTES, gt_cap_tb1);
            gt_capture(TA_VA, TA_BYTES, gt_cap_ta1);
        }
    }
}

/* ── The D1 reach predicate, and its zero control ───────────────────────────────────── */

typedef struct {
    uint64_t rt_loads, tex_uploads, rt_copies, rt_aliases;
    uint64_t readback_packs, readback_copies, readback_batches, readback_bytes;
    uint64_t readback_incomplete;
    uint64_t max_inflight, submissions, splits, frames;
    uint64_t ok, failed;
} GateD1;

/* The reach predicate's fields, with the value that must make it FAIL: 0 for a counter the
   optimized path has to raise (a zero-valued counter is the reachability control the spec asks
   for), and a non-zero one for the counter that has to STAY zero. A reachability claim that
   cannot go red is not a claim, so the case re-runs the predicate with each field spoiled. */
static const struct { const char *name; size_t off; uint64_t spoiled; } gt_d1_field[] = {
    { "g.st.rt_loads", offsetof(GateD1, rt_loads), 0u },
    { "g.st.tex_uploads", offsetof(GateD1, tex_uploads), 0u },
    { "g.st.rt_copies", offsetof(GateD1, rt_copies), 0u },
    { "g.st.rt_aliases", offsetof(GateD1, rt_aliases), 0u },
    { "g.st.readback_packs", offsetof(GateD1, readback_packs), 0u },
    { "g.st.readback_copies", offsetof(GateD1, readback_copies), 0u },
    { "g.st.readback_batches", offsetof(GateD1, readback_batches), 0u },
    { "g.st.readback_bytes", offsetof(GateD1, readback_bytes), 0u },
    { "g.st.readback_incomplete (must stay zero)", offsetof(GateD1, readback_incomplete), 1u },
    { "n_ring.max_in_flight", offsetof(GateD1, max_inflight), 0u },
    { "n_ring.submissions", offsetof(GateD1, submissions), 0u },
    { "n_ring.splits", offsetof(GateD1, splits), 0u },
    { "n_ring.frames", offsetof(GateD1, frames), 0u }
};

#define GT_D1_NFIELDS ((unsigned)(sizeof gt_d1_field / sizeof gt_d1_field[0]))

static GateD1 gt_d1_now(void)
{
    Nv2aVkRingStats ring;
    Nv2aVkReadbackStats rb;
    GateD1 d;

    memset(&d, 0, sizeof d);
    nv2a_vk_backend_ring_stats(&ring);
    nv2a_vk_backend_readback_stats(&rb);
    d.rt_loads = g.st.rt_loads;
    d.tex_uploads = g.st.tex_uploads;
    d.rt_copies = g.st.rt_copies;
    d.rt_aliases = g.st.rt_aliases;
    d.readback_packs = g.st.readback_packs;
    d.readback_copies = g.st.readback_copies;
    d.readback_batches = g.st.readback_batches;
    d.readback_bytes = g.st.readback_bytes;
    d.readback_incomplete = g.st.readback_incomplete;
    d.max_inflight = ring.max_in_flight;
    d.submissions = ring.submissions;
    d.splits = ring.splits;
    d.frames = ring.frames;
    d.ok = (uint64_t)(g.ok != 0);
    d.failed = (uint64_t)(g.failed != 0);
    return d;
}

/* "The optimized path was really reached": every counter is positive, the ring overlapped, one
   frame completed per flip, nothing was left incomplete and nothing failed. */
static int gt_d1_ok(const GateD1 *d)
{
    return d->rt_loads > 0u && d->tex_uploads > 0u && d->rt_copies > 0u && d->rt_aliases > 0u
        && d->readback_packs > 0u && d->readback_copies > 0u && d->readback_batches > 0u
        && d->readback_bytes > 0u && d->readback_incomplete == 0u && d->max_inflight >= 2u
        && d->submissions >= 3u && d->splits > 0u && d->frames == GT_FLIPS && d->ok == 1u
        && d->failed == 0u;
}

/* ── Property 0: identity, anchors and the fixture's constants ──────────────────────── */

static void prop0(void)
{
    int chk = g_checks, bad = g_check_failures;
    unsigned i;

    section("0. identity, build-time anchors and the fixture's constants");
    note("the file compiled here is the one the seam header was generated from, and the macros "
         "this case decides are the production file's own, compared against their spelling in "
         "this file's table");

    check(VB_SYNC_GATE_SOURCE[0] != '\0', "0 the seam names the source it was probed from",
          "%s", VB_SYNC_GATE_SOURCE);
    check(VB_SYNC_GATE_SOURCE_BYTES > 1000, "0 the source has a plausible size",
          "%s bytes", VB_SYNC_GATE_SOURCE_SHA256);
    printf("      sha256 %s\n", VB_SYNC_GATE_SOURCE_SHA256);

    marker("VB_SYNC_GATE_HAS_VB_DEP", VB_SYNC_GATE_HAS_VB_DEP);
    marker("VB_SYNC_GATE_HAS_VB_IMAGE_DEP", VB_SYNC_GATE_HAS_VB_IMAGE_DEP);
    marker("VB_SYNC_GATE_NO_VB_IMAGE_BARRIER", VB_SYNC_GATE_NO_VB_IMAGE_BARRIER);
    marker("VB_SYNC_GATE_BROAD_SITES_TWO", VB_SYNC_GATE_BROAD_SITES_TWO);
    marker("VB_SYNC_GATE_IMAGE_DEP_SEVEN", VB_SYNC_GATE_IMAGE_DEP_SEVEN);
    marker("VB_SYNC_GATE_BROAD_AT_SUBMIT_AND_ABANDON", VB_SYNC_GATE_BROAD_AT_SUBMIT_AND_ABANDON);
    marker("VB_SYNC_GATE_SITES_ALL_NARROW", VB_SYNC_GATE_SITES_ALL_NARROW);
    check(VB_SYNC_GATE_HAS_VB_DEP != 0, "0 vb_dep() is the file's per-site memory dependency",
          "required");
    check(VB_SYNC_GATE_HAS_VB_IMAGE_DEP != 0, "0 vb_image_dep() is the scoped layout transition",
          "required");
    check(VB_SYNC_GATE_NO_VB_IMAGE_BARRIER != 0, "0 vb_image_barrier() is gone",
          "required: the blanket transition helper is what N4.3 removed");
    check(VB_SYNC_GATE_BROAD_SITES_TWO != 0, "0 vb_barrier() has exactly two call sites",
          "the per-submission barrier and the abandonment branch, nowhere else");
    check(VB_SYNC_GATE_BROAD_AT_SUBMIT_AND_ABANDON != 0,
          "0 the two broad call sites are the ones the table names",
          "vb_slot_submit and vb_targets_flush's !batch_ok branch");
    check(VB_SYNC_GATE_IMAGE_DEP_SEVEN != 0, "0 the seven layout transitions all use vb_image_dep",
          "texture upload x2, the 1x1 white texture x2, target, depth and snapshot creates");
    check(VB_SYNC_GATE_SITES_ALL_NARROW != 0, "0 no changed site records the broad form",
          "its call sites are exactly the two retained ones, and the only hand-recorded barrier "
          "is the publish barrier in vb_targets_flush");

    /* The scope macros, against this file's own spelling of the table. A macro whose value
       changed fails HERE, so the census below cannot be self-consistently wrong. */
    check(VB_STAGE_IMAGE_DST == (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                                 | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                                 | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
                                 | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                                 | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                 | VK_PIPELINE_STAGE_TRANSFER_BIT),
          "0 VB_STAGE_IMAGE_DST is the table's value", "");
    check(VB_ACCESS_IMAGE_DST == (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                                  | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                  | VK_ACCESS_SHADER_READ_BIT
                                  | VK_ACCESS_TRANSFER_READ_BIT
                                  | VK_ACCESS_TRANSFER_WRITE_BIT),
          "0 VB_ACCESS_IMAGE_DST is the table's value",
          "COLOR_ATTACHMENT_READ is in it because the pass is created with loadOp LOAD");
    check(VB_STAGE_ATTACH == (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
          "0 VB_STAGE_ATTACH is the table's value", "");
    check(VB_ACCESS_ATTACH_WRITE == (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                                     | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
          "0 VB_ACCESS_ATTACH_WRITE is the table's value", "");
    check(VB_STAGE_DEPTH == (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                             | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT),
          "0 VB_STAGE_DEPTH is the table's value", "");
    check(VB_ACCESS_DEPTH == (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT),
          "0 VB_ACCESS_DEPTH is the table's value", "");

    /* The fixture is sized by the production's own constants, not by numbers copied from it. */
    check(VB_SLOTS == 3u, "0 the ring has the three slots the fixture's overlap expects",
          "VB_SLOTS %d", (int)VB_SLOTS);
    check(VB_SPLIT_SETS == VB_SLOT_SETS / 64u,
          "0 VB_SPLIT_SETS is the production's own descriptor-set threshold",
          "VB_SPLIT_SETS %d of VB_SLOT_SETS %d", (int)VB_SPLIT_SETS, (int)VB_SLOT_SETS);
    check(GT_PAD_DRAWS > (unsigned)VB_SPLIT_SETS,
          "0 the frame records more batches than the split threshold",
          "%u degenerate batches vs %d sets", GT_PAD_DRAWS, (int)VB_SPLIT_SETS);
    check(TEX_MAG * TEX_W == TT_W && TEX_MAG * TEX_H == TT_H,
          "0 the texture is magnified by a whole number of pixels per texel",
          "%ux%u texels over %ux%u pixels", TEX_W, TEX_H, TT_W, TT_H);
    check(VB_MAX_TARGETS >= GT_NT && VB_MAX_DEPTHS >= 1,
          "0 every fixture target and depth fits the production's caches",
          "VB_MAX_TARGETS %d, VB_MAX_DEPTHS %d", (int)VB_MAX_TARGETS, (int)VB_MAX_DEPTHS);
    check(TB_EXPECT == TB_CLEAR && nv2a_clear_to_argb(TB_CLEAR, 3u) == 0xFF211C21u,
          "0 the 16-bit clear word survives the expansion and the pack's truncation exactly",
          "pack(expand(0x%04X)) = 0x%04X (the guest format round-trips)",
          (unsigned)TB_CLEAR, (unsigned)TB_EXPECT);
    check(((nv2a_clear_to_argb(TS_CLEAR, 8u) & 0x00FF0000u) >> 16) == 0xC0u
          && ((nv2a_clear_to_argb(TS_CLEAR, 8u) & 0x0000FF00u) >> 8) == 0x80u
          && ((nv2a_clear_to_argb(TS_CLEAR, 8u) & 0x000000FFu)) == 0x40u,
          "0 the self-sampling colour's channels are the ones the expected word masks", "");
    check(TS_EXPECT != TS_CLEAR && TS_EXPECT != TS_DIFFUSE && TS_EXPECT != TB_EXPECT,
          "0 the expected snapshot word differs from the pre-draw content and the fallback",
          "expected 0x%08X, pre-draw 0x%08X, untextured fallback (white*diffuse) 0x%08X",
          (unsigned)TS_EXPECT, (unsigned)TS_CLEAR, (unsigned)TS_DIFFUSE);
    check(MARKER32 != TB_EXPECT && ((MARKER32 ^ TB_EXPECT) & 0xFFFFu) != 0u,
          "0 the marker outside the rectangle cannot be mistaken for the clear's pattern",
          "marker 0x%04X vs published 0x%04X", MARKER32 & 0xFFFFu, (unsigned)TB_EXPECT);

    /* The fixture's own texel pattern must be able to fail: no two texels may be equal. */
    {
        unsigned x, y, bad_pairs = 0;

        for (y = 0; y < TEX_H; y++)
            for (x = 0; x < TEX_W; x++) {
                uint32_t a = gt_tex_argb(x, y);
                unsigned x2, y2;
                for (y2 = 0; y2 < TEX_H; y2++)
                    for (x2 = 0; x2 < TEX_W; x2++)
                        if ((x2 != x || y2 != y) && gt_tex_argb(x2, y2) == a)
                            bad_pairs++;
            }
        check(bad_pairs == 0u, "0 the fixture's 16 texels are pairwise distinct",
              "%u equal pairs would make a wrong magnification pass", bad_pairs);
    }

    /* The forwarders stand in the driver's path. */
    check(vkCmdPipelineBarrier == gt_pipeline_barrier && vkCmdDraw == gt_draw
          && vkCmdDispatch == gt_dispatch && vkQueueSubmit == gt_submit
          && vkCmdCopyImageToBuffer == gt_copy_image_to_buffer && vkCmdClearColorImage == gt_clear_color
          && vkCmdBeginRenderPass == gt_begin_pass && vkEndCommandBuffer == gt_end_cmd,
          "0 every observed entry point was replaced by a forwarding recorder", "");
    check(real_vkCmdPipelineBarrier != NULL && real_vkQueueSubmit != NULL
          && real_vkCmdDraw != NULL && real_vkCmdDispatch != NULL,
          "0 the saved real entry points are the ones the install loaded", "");
    check(gt_nops == 0u && g.failed == 0,
          "0 nothing was recorded before the forwarders were installed",
          "ops %u (the install's own commands are outside the window by construction)",
          gt_nops);

    verdict("0 identity, anchors and constants", chk, bad);
}

/* ── Property 1: D1 reach ───────────────────────────────────────────────────────────── */

static GateD1 gt_d1;

static void prop1(void)
{
    int chk = g_checks, bad = g_check_failures;
    unsigned i;

    section("1. D1 reach: the optimized paths were really driven");
    note("three frames, each clearing and packing a 16-bit target, depth-testing a 4-byte one, "
         "magnifying a texture and sampling a snapshot, with %u degenerate batches a frame; the "
         "counters are the production's own", GT_PAD_DRAWS);

    gt_run();
    gt_d1 = gt_d1_now();

    check(gt_defaults_rc == 0, "1 the re-driven vb_create_defaults() published and submitted",
          "returned %d, g.ok %d, submits %llu", gt_defaults_rc, g.ok,
          (unsigned long long)g.st.submits);
    check(g.ok == 1 && g.failed == 0, "1 the back end is neither stopped nor failed",
          "g.ok %d, g.failed %d, fails %u", g.ok, g.failed, g.fails);

    check(gt_d1.rt_loads > 0u, "1 g.st.rt_loads > 0 (targets were loaded from guest memory)",
          "rt_loads %llu", (unsigned long long)gt_d1.rt_loads);
    check(gt_d1.tex_uploads > 0u, "1 g.st.tex_uploads > 0 (the texture was decoded and uploaded)",
          "tex_uploads %llu", (unsigned long long)gt_d1.tex_uploads);
    check(gt_d1.rt_copies > 0u, "1 g.st.rt_copies > 0 (the self-sampling snapshot was copied)",
          "rt_copies %llu", (unsigned long long)gt_d1.rt_copies);
    check(gt_d1.rt_aliases > 0u, "1 g.st.rt_aliases > 0 (a target was sampled as a texture)",
          "rt_aliases %llu", (unsigned long long)gt_d1.rt_aliases);
    check(gt_d1.readback_packs > 0u, "1 g.st.readback_packs > 0 (the pack kernel published)",
          "readback_packs %llu", (unsigned long long)gt_d1.readback_packs);
    check(gt_d1.readback_copies > 0u, "1 g.st.readback_copies > 0 (a target copied straight out)",
          "readback_copies %llu", (unsigned long long)gt_d1.readback_copies);
    check(gt_d1.readback_batches > 0u, "1 g.st.readback_batches > 0 (batches reached a publish)",
          "readback_batches %llu", (unsigned long long)gt_d1.readback_batches);
    check(gt_d1.readback_bytes > 0u, "1 g.st.readback_bytes > 0 (guest memory was really written)",
          "readback_bytes %llu", (unsigned long long)gt_d1.readback_bytes);
    check(gt_d1.readback_incomplete == 0u, "1 g.st.readback_incomplete == 0 (nothing was dropped)",
          "readback_incomplete %llu", (unsigned long long)gt_d1.readback_incomplete);
    check(gt_d1.max_inflight >= 2u, "1 n_ring.max_in_flight >= 2 (two slots were outstanding)",
          "max_in_flight %llu", (unsigned long long)gt_d1.max_inflight);
    check(gt_d1.submissions >= 3u, "1 n_ring.submissions >= 3 (the ring really submitted)",
          "submissions %llu", (unsigned long long)gt_d1.submissions);
    check(gt_d1.splits > 0u, "1 n_ring.splits > 0 (a draw entry split the recording)",
          "splits %llu of %llu submissions", (unsigned long long)gt_d1.splits,
          (unsigned long long)gt_d1.submissions);
    check(gt_d1.frames == GT_FLIPS, "1 n_ring.frames == the fixture's flip count",
          "frames %llu, flips %u", (unsigned long long)gt_d1.frames, GT_FLIPS);
    check(gt_draw_calls > GT_PAD_DRAWS && gt_clear_calls == 4u,
          "1 the fixture drove the draws and clears it says it did",
          "draws %u (pad %u x %u + 6 deciding), clears %u", gt_draw_calls, GT_PAD_DRAWS, GT_FLIPS,
          gt_clear_calls);

    /* The reach predicate itself, and the control that it FAILS when a counter is zero: a
       reachability claim that cannot go red is not a claim. */
    check(gt_d1_ok(&gt_d1), "1 the reach predicate holds for the observed counters", "");
    for (i = 0; i < GT_D1_NFIELDS; i++) {
        GateD1 z = gt_d1;
        uint64_t *p = (uint64_t *)(void *)((uint8_t *)(void *)&z + gt_d1_field[i].off);

        *p = gt_d1_field[i].spoiled;
        check(!gt_d1_ok(&z), "1 the same predicate FAILS with one counter spoiled",
              "%s = %llu", gt_d1_field[i].name, (unsigned long long)gt_d1_field[i].spoiled);
    }

    verdict("1 D1 reach", chk, bad);
}

/* ── Property 2: D2 bytes ───────────────────────────────────────────────────────────── */

/* Every PIXEL -- not a sample of pixels -- of [x0,x1)x[y0,y1) in `base` (the arena, or a captured
   copy with va 0) must hold the fixture's expected word. No tolerance. */
static unsigned gt_rect_bad(const uint8_t *base, uint32_t va, uint32_t pitch, uint32_t bpp,
                            int which, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1,
                            int report)
{
    const uint8_t *p = base + va;
    uint32_t x, y;
    unsigned bad = 0;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            uint32_t got, want;
            if (bpp == 4u) {
                memcpy(&got, p + (size_t)y * pitch + (size_t)x * 4u, 4u);
            } else {
                uint16_t v;
                memcpy(&v, p + (size_t)y * pitch + (size_t)x * 2u, 2u);
                got = v;
            }
            want = gt_expect_word(which, x, y);
            if (got != want) {
                if (!bad && report)
                    printf("          first mismatch at (%u,%u): 0x%08X, want 0x%08X\n",
                           (unsigned)x, (unsigned)y, (unsigned)got, (unsigned)want);
                bad++;
            }
        }
    }
    return bad;
}

/* Every pixel of the target's surface OUTSIDE the published rectangle must still hold the marker
   the fixture preloaded: the publication writes the dirty rectangle and nothing else. */
static unsigned gt_outside_bad(const uint8_t *base, uint32_t va, uint32_t pitch, uint32_t bpp,
                               uint32_t w, uint32_t h, uint32_t x0, uint32_t y0, uint32_t x1,
                               uint32_t y1)
{
    const uint8_t *p = base + va;
    uint32_t x, y;
    unsigned bad = 0;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t got, want = bpp == 4u ? MARKER32 : (uint32_t)(MARKER | (MARKER << 8));
            if (x >= x0 && x < x1 && y >= y0 && y < y1)
                continue;
            if (bpp == 4u) {
                memcpy(&got, p + (size_t)y * pitch + (size_t)x * 4u, 4u);
            } else {
                uint16_t v;
                memcpy(&v, p + (size_t)y * pitch + (size_t)x * 2u, 2u);
                got = v;
            }
            if (got != want)
                bad++;
        }
    }
    return bad;
}

static void prop2(void)
{
    int chk = g_checks, bad = g_check_failures;
    char what[180];
    unsigned i, nbad;
    int same;

    section("2. D2 bytes: the published rectangles are exactly the fixture's plan");
    note("every byte of every published rectangle is compared, with no tolerance; the rectangles "
         "are the production's own dirty rectangles, read from the targets after each frame");

    for (i = 0; i < GT_NT; i++) {
        GateRect *r = NULL;
        unsigned f;

        for (f = 0; f < GT_FLIPS; f++)
            if (gt_rect[f][i].live)
                r = &gt_rect[f][i];
        snprintf(what, sizeof what, "2 %s was created and drawn into", gt_tname[i]);
        check(r != NULL, what, "va 0x%X, rectangle (%u,%u)-(%u,%u)", (unsigned)gt_tva[i],
              r ? (unsigned)r->x0 : 0u, r ? (unsigned)r->y0 : 0u, r ? (unsigned)r->x1 : 0u,
              r ? (unsigned)r->y1 : 0u);
        if (!r)
            continue;
    }

    /* B1/B2: the 16-bit target, its partial-scissor rectangle and the marker outside it. */
    {
        GateRect *r = &gt_rect[2][GT_TB];

        check(r->live && r->x0 == TB_CX && r->y0 == TB_CY && r->x1 == TB_CX + TB_CW
              && r->y1 == TB_CY + TB_CH,
              "2 TB's dirty rectangle is the partial scissor, not the whole target",
              "the production says (%u,%u)-(%u,%u), the fixture's clip is (%u,%u)-(%u,%u)",
              (unsigned)r->x0, (unsigned)r->y0, (unsigned)r->x1, (unsigned)r->y1, TB_CX, TB_CY,
              TB_CX + TB_CW, TB_CY + TB_CH);
        nbad = gt_rect_bad(gt_arena, TB_VA, TB_PITCH, 2u, GT_TB, r->x0, r->y0, r->x1, r->y1, 1);
        check(nbad == 0u, "2 B1 every published halfword of TB is the 5:6:5 clear word",
              "%u of %u pixels differ", nbad, (r->x1 - r->x0) * (r->y1 - r->y0));
        nbad = gt_outside_bad(gt_arena, TB_VA, TB_PITCH, 2u, TB_W, TB_H, r->x0, r->y0, r->x1,
                              r->y1);
        check(nbad == 0u, "2 B2 every halfword outside TB's rectangle is still the marker",
              "%u of %u pixels were touched", nbad,
              TB_W * TB_H - (r->x1 - r->x0) * (r->y1 - r->y0));
        same = memcmp(gt_arena + TB_VA, gt_cap_tb1, TB_BYTES) == 0;
        nbad = gt_rect_bad(gt_cap_tb1, 0u, TB_PITCH, 2u, GT_TB, r->x0, r->y0, r->x1, r->y1, 0);
        check(nbad == 0u, "2 B6 the first flip published the same pattern",
              "%u pixels of the frame-1 capture differ", nbad);
        check(same, "2 B6 the second and third frames' load-then-redraw republished it byte for "
                    "byte",
              "the guest bytes after flip 3 %s the bytes flip 1 published",
              same ? "equal" : "differ from");
    }

    /* B3: the depth-ordered pass. */
    {
        GateRect *r = &gt_rect[0][GT_TA];
        unsigned near_px = 0, far_px = 0;

        check(r->live && r->x0 == 0u && r->y0 == 0u && r->x1 == TA_W && r->y1 == TA_H,
              "2 TA's dirty rectangle is the whole target the two quads cover", "");
        nbad = gt_rect_bad(gt_arena, TA_VA, TA_PITCH, 4u, GT_TA, r->x0, r->y0, r->x1, r->y1, 1);
        for (i = 0; i < TA_W * TA_H; i++)
            if (gt_expect_word(GT_TA, i % TA_W, i / TA_W) == TA_NEAR)
                near_px++;
            else
                far_px++;
        check(nbad == 0u, "2 B3 the depth-ordered pass published the nearer quad's colour",
              "%u of %u pixels differ; the plan is %u near (0x%08X) and %u far (0x%08X)",
              nbad, TA_W * TA_H, near_px, (unsigned)TA_NEAR, far_px, (unsigned)TA_FAR);
        check(near_px > 0u && far_px > 0u, "2 B3 the expectation could have failed either way",
              "the overlap is %u pixels and the far region %u", near_px, far_px);
        check(memcmp(gt_arena + TA_VA, gt_cap_ta1, TA_BYTES) == 0,
              "2 B3 a target no later frame drew into was not republished",
              "TA's bytes still equal what flip 1 published");
    }

    /* B4: the magnified point-sampled texture. */
    {
        GateRect *r = &gt_rect[1][GT_TT];
        uint32_t blocks[TEX_W * TEX_H];
        unsigned distinct = 0, bx, by, b2;

        check(r->live && r->x0 == 0u && r->y0 == 0u && r->x1 == TT_W && r->y1 == TT_H,
              "2 TT's dirty rectangle is the whole target the quad covers", "");
        nbad = gt_rect_bad(gt_arena, TT_VA, TT_PITCH, 4u, GT_TT, r->x0, r->y0, r->x1, r->y1, 1);
        check(nbad == 0u, "2 B4 the magnified texture published each texel's exact 4x4 block",
              "%u of %u pixels differ from the fixture's own decoded pattern (point filtering, "
              "pixel-centred sampling)", nbad, TT_W * TT_H);

        /* and the check is load-bearing: the 16 blocks are pairwise distinct */
        for (by = 0; by < TEX_H; by++)
            for (bx = 0; bx < TEX_W; bx++) {
                uint32_t v;
                memcpy(&v, gt_arena + TT_VA + (size_t)(by * TEX_MAG) * TT_PITCH
                       + (size_t)(bx * TEX_MAG) * 4u, 4u);
                blocks[by * TEX_W + bx] = v;
            }
        for (i = 0; i < TEX_W * TEX_H; i++) {
            for (b2 = i + 1u; b2 < TEX_W * TEX_H; b2++)
                if (blocks[i] == blocks[b2])
                    break;
            if (b2 == TEX_W * TEX_H)
                distinct++;
        }
        check(distinct == TEX_W * TEX_H,
              "2 B4 all 16 published blocks are pairwise distinct",
              "%u of %u blocks are unique, so a wrong texel ordering cannot pass",
              distinct, TEX_W * TEX_H);
    }

    /* B5: the self-sampling snapshot. */
    {
        GateRect *r = &gt_rect[2][GT_TS];

        check(r->live && r->x0 == 0u && r->y0 == 0u && r->x1 == TS_W && r->y1 == TS_H,
              "2 TS's dirty rectangle is the whole target the self-sampling draw covers", "");
        nbad = gt_rect_bad(gt_arena, TS_VA, TS_PITCH, 4u, GT_TS, r->x0, r->y0, r->x1, r->y1, 1);
        check(nbad == 0u, "2 B5 the snapshot's sampled content published the masked word",
              "%u of %u pixels differ from 0x%08X (the pre-draw content 0x%08X masked by the "
              "diffuse 0x%08X through factors 0 and 1)",
              nbad, TS_W * TS_H, (unsigned)TS_EXPECT, (unsigned)TS_CLEAR,
              (unsigned)TS_DIFFUSE);
        check(TS_EXPECT != TS_DIFFUSE && TS_EXPECT != TS_CLEAR,
              "2 B5 the expected word is neither the untextured fallback nor the pre-draw content",
              "expected 0x%08X, fallback 0x%08X, pre-draw 0x%08X", (unsigned)TS_EXPECT,
              (unsigned)TS_DIFFUSE, (unsigned)TS_CLEAR);
    }

    verdict("2 D2 bytes", chk, bad);
}

/* ── Property 3: D3 masks ───────────────────────────────────────────────────────────── */

typedef struct {
    unsigned s1, s2, s3, s4, s5, s6a_tex, s6b_tex, s6a_white, s6b_white, s7, s8;
    unsigned tgt_create, depth_create, snapshot_create;
    unsigned r1, broad_any, img_total, mem_total, unclassified_mem, unexpected_img;
    unsigned in_pass, submitted;
} GateCensus;

static GateCensus gt_census(void)
{
    GateCensus c;
    unsigned i;

    memset(&c, 0, sizeof c);
    c.s1 = gt_count_mem(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VB_STAGE_IMAGE_DST, VB_ACCESS_IMAGE_DST, GOP_COPY_B2I);
    c.s2 = gt_count_mem(VB_STAGE_ATTACH, VB_ACCESS_ATTACH_WRITE, VB_STAGE_IMAGE_DST,
                        VB_ACCESS_IMAGE_DST, GOP_END_PASS);
    c.s3 = gt_count_mem(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0);
    c.s4 = gt_count_mem(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT, 0);
    c.s5 = gt_count_mem(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                        | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT
                        | VK_ACCESS_TRANSFER_WRITE_BIT, GOP_COPY_IMAGE);
    /* S7: the same scope pair as S1, told apart by the command it follows. */
    c.s7 = 0;
    c.s7 = gt_count_mem(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VB_STAGE_IMAGE_DST, VB_ACCESS_IMAGE_DST, GOP_CLEAR_COLOR);
    c.s8 = gt_count_mem(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                        VB_STAGE_DEPTH, VB_ACCESS_DEPTH, GOP_CLEAR_DEPTH);
    c.s6a_tex = gt_count_img_role(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  "texture va=");
    c.s6b_tex = gt_count_img_role(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "texture va=");
    c.s6a_white = gt_count_img_role(VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    "the 1x1 white texture");
    c.s6b_white = gt_count_img_role(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                    "the 1x1 white texture");
    c.tgt_create = gt_count_img_role(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                     "target va=");
    c.depth_create = gt_count_img_role(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                       "depth va=");
    c.snapshot_create = gt_count_img_role(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                          "snapshot of target va=");
    c.r1 = gt_count_mem(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0);
    c.broad_any = gt_count_broad();
    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        if (o->op == GOP_IMAGE_BARRIER)
            c.img_total++;
        if (o->op == GOP_PIPELINE_BARRIER && o->mem_bars == 1u && o->img_bars == 0u)
            c.mem_total++;
        if (o->op == GOP_BEGIN_CMD || o->op == GOP_END_CMD || o->op == GOP_RESET_CMD)
            continue;
        if ((o->op == GOP_PIPELINE_BARRIER || o->op == GOP_IMAGE_BARRIER) && o->pass_open)
            c.in_pass++;
    }
    for (i = 1; i <= gt_cur_rec && i <= GT_MAX_RECS; i++)
        if (gt_rec_submitted[i])
            c.submitted++;
    c.unclassified_mem = c.mem_total
        - (c.s1 + c.s2 + c.s3 + c.s4 + c.s5 + c.s7 + c.s8 + c.r1);
    c.unexpected_img = c.img_total
        - (c.s6a_tex + c.s6b_tex + c.s6a_white + c.s6b_white + c.tgt_create + c.depth_create
           + c.snapshot_create);
    return c;
}

static unsigned gt_used_targets(void)
{
    unsigned i, n = 0;

    for (i = 0; i < VB_MAX_TARGETS; i++)
        if (g.targets[i].used)
            n++;
    return n;
}

static unsigned gt_used_depths(void)
{
    unsigned i, n = 0;

    for (i = 0; i < VB_MAX_DEPTHS; i++)
        if (g.depths[i].used)
            n++;
    return n;
}

static void gt_print_census(const GateCensus *c)
{
    unsigned i;
    char role[96];

    printf("      observed, per site (the window is every command the fixture drove, from the "
           "forwarders' install on):\n");
    printf("        S1 vb_target_load's load barrier      %u  (g.st.rt_loads %llu)\n", c->s1,
           (unsigned long long)g.st.rt_loads);
    printf("        S2 vb_end_pass's attach barrier       %u  (vkCmdEndRenderPass calls %u)\n",
           c->s2, gt_pass_ends_edge);
    printf("        S3 the pack dispatch -> copy pair     %u  (g.st.readback_packs %llu)\n", c->s3,
           (unsigned long long)g.st.readback_packs);
    printf("        S4 the publish barrier                %u  (g.st.readback_batches %llu)\n",
           c->s4, (unsigned long long)g.st.readback_batches);
    printf("        S5 the snapshot copy's barrier        %u  (g.st.rt_copies %llu)\n", c->s5,
           (unsigned long long)g.st.rt_copies);
    printf("        S6 texture transitions                %u  (white: %u) \n", c->s6a_tex + c->s6b_tex,
           c->s6a_white + c->s6b_white);
    printf("        S7 vb_target_create's clear barrier   %u  (live targets %u)\n", c->s7,
           gt_used_targets());
    printf("        S8 vb_depth_for's clear barrier       %u  (live depths %u)\n", c->s8,
           gt_used_depths());
    printf("        S9/creates: target %u, depth %u, snapshot %u layout transitions\n",
           c->tgt_create, c->depth_create, c->snapshot_create);
    printf("        R1 the retained broad barrier         %u  (submitted recordings %u)\n", c->r1,
           c->submitted);
    printf("        image barriers total %u, memory barriers total %u, "
           "unclassified memory %u, unexpected image %u\n",
           c->img_total, c->mem_total, c->unclassified_mem, c->unexpected_img);
    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        if (o->op != GOP_IMAGE_BARRIER)
            continue;
        gt_role(o->image, role, sizeof role);
        printf("        layout %-32s %d -> %d, aspect 0x%X, %u levels, %u layers\n", role,
               (int)o->old_layout, (int)o->new_layout, (unsigned)o->aspect,
               (unsigned)o->level_count, (unsigned)o->layer_count);
    }
}

static void prop3(void)
{
    int chk = g_checks, bad = g_check_failures;
    GateCensus c = gt_census();
    unsigned i, recs_bad = 0, last_bad = 0, unattributed = 0, wrong_cb = 0, dead_ops = 0;
    unsigned img_bad = 0, attributed = 0;
    char what[200];

    section("3. D3 masks: each site's exact scopes, and the broad barrier's census");
    note("the recorded stream is the real driver's: every entry point below forwards to the "
         "pointer the install loaded, so these are the calls the device received");

    gt_print_census(&c);

    printf("      memory-barrier scope pairs the table names:\n");
    printf("        S1 TRANSFER/TW -> IMAGE_DST                %u\n", c.s1);
    printf("        S2 ATTACH/AW  -> IMAGE_DST                 %u\n", c.s2);
    printf("        S3 COMPUTE/SW -> TRANSFER/TR               %u\n", c.s3);
    printf("        S4 COMPUTE|TRANSFER/... -> HOST/HR         %u\n", c.s4);
    printf("        S5 TRANSFER/TW -> FRAG|COMPUTE|TRANSFER   %u\n", c.s5);
    printf("        S7 TRANSFER/TW -> IMAGE_DST (after a clear) %u\n", c.s7);
    printf("        S8 TRANSFER/TW -> DEPTH/DEPTH_ATTACH       %u\n", c.s8);
    printf("        R1 ALL_COMMANDS/MEMORY_WRITE -> ALL_COMMANDS/MEMORY_READ|WRITE  %u\n", c.r1);

    check(c.s1 >= 1u, "3 S1's exact scope pair is recorded at the load's site", "%u occurrences",
          c.s1);
    check(c.s2 >= 1u, "3 S2's exact scope pair is recorded at the pass's end", "%u occurrences",
          c.s2);
    check(c.s3 >= 1u, "3 S3's exact scope pair is recorded after a pack dispatch", "%u occurrences",
          c.s3);
    check(c.s4 >= 1u, "3 S4's exact scope pair is recorded as the publish barrier",
          "%u occurrences", c.s4);
    check(c.s5 >= 1u, "3 S5's exact scope pair is recorded after the snapshot copy",
          "%u occurrences", c.s5);
    check(c.s7 >= 1u, "3 S7's exact scope pair is recorded after a target create's clear",
          "%u occurrences", c.s7);
    check(c.s8 >= 1u, "3 S8's exact scope pair is recorded after the depth clear", "%u occurrences",
          c.s8);
    check(c.s6a_tex >= 1u && c.s6b_tex >= 1u, "3 S6's two transitions are recorded for the upload",
          "UNDEFINED->TRANSFER_DST %u, ->SHADER_READ_ONLY %u", c.s6a_tex, c.s6b_tex);
    check(c.s6a_white >= 1u && c.s6b_white >= 1u,
          "3 S6's two transitions are recorded for the 1x1 white texture too",
          "UNDEFINED->TRANSFER_DST %u, ->SHADER_READ_ONLY %u", c.s6a_white, c.s6b_white);
    check(c.snapshot_create >= 1u, "3 S9's snapshot transition is recorded", "%u occurrences",
          c.snapshot_create);

    /* Each site's count is tied to a quantity the production keeps itself: a barrier that moved,
       changed scope or vanished cannot keep the two equal. */
    check(c.s1 == (unsigned)g.st.rt_loads, "3 S1's count is the production's own rt_loads",
          "%u barriers, rt_loads %llu", c.s1, (unsigned long long)g.st.rt_loads);
    check(c.s2 == gt_pass_ends_edge, "3 S2's count is one per closed render pass",
          "%u barriers, %u vkCmdEndRenderPass calls", c.s2, gt_pass_ends_edge);
    check(c.s3 == (unsigned)g.st.readback_packs, "3 S3's count is one per packed publication",
          "%u barriers, readback_packs %llu", c.s3, (unsigned long long)g.st.readback_packs);
    check(c.s4 == (unsigned)g.st.readback_batches, "3 S4's count is one per published batch",
          "%u barriers, readback_batches %llu", c.s4,
          (unsigned long long)g.st.readback_batches);
    check(c.s5 == (unsigned)g.st.rt_copies, "3 S5's count is one per snapshot copy",
          "%u barriers, rt_copies %llu", c.s5, (unsigned long long)g.st.rt_copies);
    check(c.s7 == gt_used_targets(), "3 S7's count is one per target creation",
          "%u barriers, %u live targets", c.s7, gt_used_targets());
    check(c.s8 == gt_used_depths(), "3 S8's count is one per depth creation", "%u barriers, %u "
          "live depths", c.s8, gt_used_depths());
    check(c.tgt_create == gt_used_targets() && c.depth_create == gt_used_depths()
          && c.snapshot_create == 1u,
          "3 every target, depth and snapshot image got exactly its creation transition",
          "targets %u/%u, depths %u/%u, snapshot %u", c.tgt_create, gt_used_targets(),
          c.depth_create, gt_used_depths(), c.snapshot_create);

    /* Nowhere else: the window's whole census must be the table. */
    check(c.unclassified_mem == 0u,
          "3 every memory barrier in the window is one of the table's sites",
          "%u of %u memory barriers are none of S1..S5, S7, S8, R1", c.unclassified_mem,
          c.mem_total);
    check(c.unexpected_img == 0u,
          "3 every image barrier in the window is one of the transitions the table names",
          "%u of %u image barriers are none of S6, S9 and the three creation transitions",
          c.unexpected_img, c.img_total);
    check(c.mem_total == c.s1 + c.s2 + c.s3 + c.s4 + c.s5 + c.s7 + c.s8 + c.r1,
          "3 the site counts account for every memory barrier recorded", "%u barriers",
          c.mem_total);

    /* The retained broad site R1, and R2's absence. */
    check(c.r1 == c.submitted && c.submitted >= 3u,
          "3 the broad barrier appears exactly once per SUBMITTED command buffer",
          "%u broad barriers, %u submitted recordings, %u recordings begun", c.r1, c.submitted,
          gt_cur_rec);
    check(c.broad_any == c.r1, "3 no other barrier has ALL_COMMANDS as its source stage",
          "%u barriers carry ALL_COMMANDS, of which %u are the retained broad form", c.broad_any,
          c.r1);
    check(c.r1 == c.submitted, "3 R2 (the abandonment branch) was not taken in this window",
          "its masks are R1's, so any extra mutation-free broad barrier would show here; "
          "g.st.readback_incomplete %llu",
          (unsigned long long)g.st.readback_incomplete);

    /* The broad barrier is the last barrier of its recording, right before the end. */
    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        unsigned nrec, nb;

        if (o->op != GOP_BEGIN_CMD)
            continue;
        nrec = o->rec;
        if (!nrec)
            continue;
        nb = 0;
        {
            unsigned j, bars = 0, broads = 0, last_is_broad = 0;

            for (j = 0; j < gt_nops; j++) {
                const GateOp *p = &gt_ops[j];
                if (p->rec != nrec)
                    continue;
                if (p->op == GOP_PIPELINE_BARRIER || p->op == GOP_IMAGE_BARRIER) {
                    bars++;
                    if (p->src_stage & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
                        broads++;
                        last_is_broad = 1;
                    } else {
                        last_is_broad = 0;
                    }
                } else if (p->op == GOP_END_CMD && bars && !last_is_broad) {
                    last_bad++;
                }
            }
            nb = broads;
        }
        if (gt_rec_submitted[nrec] && nb != 1u)
            recs_bad++;
    }
    check(recs_bad == 0u, "3 every submitted recording carries exactly one broad barrier",
          "%u recordings disagree", recs_bad);
    check(last_bad == 0u, "3 that broad barrier is the LAST barrier before vkEndCommandBuffer",
          "%u recordings have a later barrier", last_bad);

    /* Structure of the stream itself. */
    check(c.in_pass == 0u, "3 no barrier of any kind is recorded while a render pass is open",
          "%u barriers were; %u passes were begun and %u ended", c.in_pass, gt_pass_begins,
          gt_pass_ends_edge);
    check(gt_pass_begins == gt_pass_ends_edge && gt_pass_open == 0u,
          "3 every render pass the fixture opened was closed inside its recording",
          "begun %u, ended %u, still open %u", gt_pass_begins, gt_pass_ends_edge, gt_pass_open);
    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        if (o->op == GOP_IMAGE_BARRIER) {
            if (o->level_count != 1u || o->layer_count != 1u)
                img_bad++;
            if (o->queue_family != (int32_t)VK_QUEUE_FAMILY_IGNORED && o->queue_family != -2)
                img_bad++;
            if (o->mem_bars != 0u || o->buf_bars != 0u)
                img_bad++;
        } else if (o->op == GOP_PIPELINE_BARRIER && o->mem_bars == 1u) {
            if (o->img_bars != 0u || o->buf_bars != 0u || o->dep_flags != 0u)
                img_bad++;
        }
    }
    check(img_bad == 0u,
          "3 every transition is whole-resource (1 level, 1 layer) and every memory barrier "
          "carries one memory barrier and no other",
          "%u calls disagree", img_bad);
    check(gt_ops_full == 0, "3 the recorded stream fits the fixture's own bound",
          "%u ops recorded of %d", gt_nops, (int)GT_MAX_OPS);
    for (i = 0; i < gt_nops; i++) {
        const GateOp *o = &gt_ops[i];
        /* A reset is recorded before the recording it precedes begins, and a fence wait is not
           recorded into a command buffer at all: neither names a recording, so neither is
           attributed here. Every OTHER command must belong to the recording of its own command
           buffer -- that is what keeps the per-site census honest, and it is also how a command
           recorded into the dead buffer (begun before the window) would show up. */
        if (o->op == GOP_RESET_CMD || o->op == GOP_WAIT_FENCE)
            continue;
        attributed++;
        if (!o->rec)
            unattributed++;
        else if (gt_rec_cb[o->rec] != o->cb)
            wrong_cb++;
        if (o->cb == g.dead)
            dead_ops++;
    }
    check(unattributed == 0 && wrong_cb == 0 && dead_ops == 0,
          "3 every recorded command belongs to a recording of its own command buffer",
          "%u of %u attributed commands do not (%u unattributed, %u in the wrong buffer, %u in "
          "the dead buffer)", attributed, gt_nops, unattributed, wrong_cb, dead_ops);
    check(attributed > 400u, "3 the attribution covers the whole stream",
          "%u of %u ops attributed (%u resets and fence waits are not recorded into a buffer)",
          attributed, gt_nops, gt_nops - attributed);

    snprintf(what, sizeof what, "3 the window contains the pack path, both copy directions and "
                                "the snapshot copy");
    check(c.s3 >= 1u && c.s4 >= 1u && c.s5 >= 1u && c.img_total >= 10u,
          what, "S3 %u, S4 %u, S5 %u, image barriers %u", c.s3, c.s4, c.s5, c.img_total);

    verdict("3 D3 masks", chk, bad);
}

/* ── Property 4: D4 validation ──────────────────────────────────────────────────────── */

static uint64_t gt_install_ve, gt_install_vw;

static void prop4(void)
{
    int chk = g_checks, bad = g_check_failures;
    const char *layers = getenv("VK_INSTANCE_LAYERS");
    const char *loader_layers = getenv("VK_LOADER_LAYERS_ENABLE");
    uint64_t ve = g.st.validation_errors, vw = g.st.validation_warnings;

    section("4. D4 validation: what the messenger counted, and what this case may claim");
    note("the back end creates a debug messenger when VK_EXT_debug_utils is present, and its "
         "callback counts VALIDATION/PERFORMANCE messages as errors or warnings; with no layer "
         "requested the counters are 0 by construction");

    printf("      install-time: errors %llu, warnings %llu (before the fixture drove anything)\n",
           (unsigned long long)gt_install_ve, (unsigned long long)gt_install_vw);
    printf("      fixture window: errors %llu, warnings %llu (delta from install)\n",
           (unsigned long long)(ve - gt_install_ve), (unsigned long long)(vw - gt_install_vw));
    printf("      messages printed (bounded by the back end) %u of VB_VALIDATION_SAY %d\n",
           g.validation_said, (int)VB_VALIDATION_SAY);
    if (layers && *layers)
        printf("      VK_INSTANCE_LAYERS=%s: an external run has enabled layers\n", layers);
    else if (loader_layers && *loader_layers)
        printf("      VK_LOADER_LAYERS_ENABLE=%s: an external run has enabled layers\n",
               loader_layers);
    else
        note("no validation layer is requested by the environment, so the counts below are 0 by "
             "construction: this case makes NO synchronization-validation claim (that receipt "
             "belongs to an external run with VK_INSTANCE_LAYERS set). The known warnings this "
             "back end emits are the 'attribute not consumed' ones the unused vertex inputs of "
             "the combiner/program pipelines raise; they are counted above, not parsed here.");

    check(ve == 0u, "4 no validation error was reported for the fixture window",
          "errors %llu", (unsigned long long)ve);
    check(gt_install_ve == 0u, "4 no validation error was reported while the back end installed",
          "errors %llu", (unsigned long long)gt_install_ve);
    check(ve >= gt_install_ve && vw >= gt_install_vw,
          "4 the counters are monotone (they are counters, not reads)",
          "install %llu/%llu, window %llu/%llu", (unsigned long long)gt_install_ve,
          (unsigned long long)gt_install_vw, (unsigned long long)ve, (unsigned long long)vw);

    verdict("4 D4 validation", chk, bad);
}

/* ── Property 5: D5 termination ─────────────────────────────────────────────────────── */

static void prop5(void)
{
    int chk = g_checks, bad = g_check_failures;
    Nv2aVkRingStats ring;
    double secs = (double)gt_elapsed_ns / 1e9;

    section("5. D5 termination: bounded, accounted, and nothing left in flight");

    nv2a_vk_backend_ring_stats(&ring);
    check(secs > 0.0 && secs < 600.0, "5 the whole fixture runs well inside the CTest timeout",
          "%.3f s of a 600 s TIMEOUT (the case's own limit is 600)", secs);
    check(g.ok == 1 && g.failed == 0, "5 the back end is neither stopped nor failed at the end",
          "g.ok %d, g.failed %d", g.ok, g.failed);
    check(g.frame == GT_FLIPS, "5 one frame completed per flip", "g.frame %u of %u", g.frame,
          GT_FLIPS);
    check(ring.in_flight == 0u && ring.drain_waits > 0u,
          "5 the last flip drained the ring", "in_flight %u, drain_waits %llu", ring.in_flight,
          (unsigned long long)ring.drain_waits);
    check(gt_waits > 0u && gt_submits >= 3u,
          "5 the fixture's submissions really were submitted and waited for",
          "vkQueueSubmit %u, vkWaitForFences %u", gt_submits, gt_waits);
    check(gt_pad_draws_seen == GT_PAD_DRAWS * GT_FLIPS + GT_FLIPS,
          "5 the degenerate batches the run recorded are the ones it was sized for",
          "%u of %u", gt_pad_draws_seen, GT_PAD_DRAWS * GT_FLIPS + GT_FLIPS);

    verdict("5 D5 termination", chk, bad);
}

/* ── The five symbols the toolkit file needs, and nothing else of the toolkit ───────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)gt_arena;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    (void)backend;
}

/* The fixture IS the texture: a deterministic 0xAARRGGBB level-0 pattern of the fixture's own
   dimensions. Everything else about the texture path -- the image, its transitions, the upload
   copy, the sampler and the draws that sample it -- is the production's and the device's. */
int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t x, y;

    if (!tex || !argb_out || !tex->width || !tex->height)
        return 0;
    for (y = 0; y < tex->height; y++)
        for (x = 0; x < tex->width; x++)
            argb_out[(size_t)y * tex->width + x] = gt_tex_argb(x, y);
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    if (!tex)
        return 0;
    return tex->width * tex->height * 4u;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* ── The run ────────────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    int rc;

    /* The sibling shape: a build without the software ICD registers this case with the reason
       printed, exactly as jsrf_gpu_backend_parity_vk's cases do. */
    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: vk_sync_paths_gate -- %s\n", argv[2]);
        return 77;
    }

    printf("vk_sync_paths_gate: N4.3's narrowed synchronization, decided on a real Vulkan device\n"
           "against the toolkit's own nv2a_vk_backend.c, from the driver call stream its own\n"
           "forwarders record and from the guest bytes its flip publishes\n");

    rc = nv2a_vk_backend_install();
    if (rc != 0) {
        printf("SKIP: vk_sync_paths_gate -- the Vulkan back end could not be installed on the "
               "configured software device\n"
               "      (no Vulkan loader, no ICD, or an elevated loader that ignores "
               "VK_DRIVER_FILES / VK_ICD_FILENAMES; RECOMP_VK_DEVICE=llvmpipe names the software "
               "device)\n");
        return 77;
    }
    gt_install_ve = g.st.validation_errors;
    gt_install_vw = g.st.validation_warnings;
    gt_elapsed_ns = 0;
    if (!gt_wire()) {
        fprintf(stderr, "FAIL: vk_sync_paths_gate: the installed back end did not carry every "
                        "entry point this case observes\n");
        return 1;
    }
    gt_fixture();
    if (!gt_arena || !gt_cap_tb1 || !gt_cap_ta1) {
        fprintf(stderr, "FAIL: vk_sync_paths_gate: the fixture could not be allocated\n");
        return 1;
    }
    gt_t0 = vb_now_ns();

    prop0();
    prop1();
    prop2();
    gt_elapsed_ns = vb_now_ns() - gt_t0;
    prop3();
    prop4();
    prop5();

    free(gt_cap_ta1);
    free(gt_cap_tb1);
    free(gt_arena);

    printf("\n");
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_sync_paths_gate: %d of %d properties failed (%d of %d checks "
                        "failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: %d properties, %d checks\n", g_props, g_checks);
    printf("PASS: vk_sync_paths_gate: the optimized paths were reached, every changed site's "
           "barrier carries the table's exact scopes with the broad barrier only once per "
           "submission, and the flipped bytes are the fixture's own plan byte for byte\n");
    return 0;
}
