/* N4.3's narrowed Vulkan synchronization, as a DECIDED property of the recorded command stream:
 * every barrier the packet narrowed is checked against the accesses it must order, and every
 * pair of accesses it claims to order is decided from the recording rather than from the call.
 *
 * What this case exists for
 * -------------------------
 * src/video/nv2a_vk_backend.c used to order everything against everything: one broad
 * vb_barrier() (ALL_COMMANDS / MEMORY_WRITE -> ALL_COMMANDS / MEMORY_READ|MEMORY_WRITE) sat at
 * every site that needed any ordering, and every layout transition was a picture-wide barrier
 * with the same masks. N4.3 narrowed each of those sites to the producer and the consumer the
 * resource actually has, through two helpers -- vb_dep(src_stage, src_access, dst_stage,
 * dst_access) and vb_image_dep(img, aspect, from, to, src_stage, src_access, dst_stage,
 * dst_access) -- and through six scope macros (VB_STAGE_IMAGE_DST, VB_ACCESS_IMAGE_DST,
 * VB_STAGE_ATTACH, VB_ACCESS_ATTACH_WRITE, VB_STAGE_DEPTH, VB_ACCESS_DEPTH) that name those
 * scopes once. Exactly two broad vb_barrier() sites are kept on purpose: the per-submission
 * barrier in vb_slot_submit (the only thing that makes cross-submission ordering defined) and
 * the abandonment barrier in vb_targets_flush. What THAT barrier must order is not decided here
 * -- jsrf_vk_readback_ordering_gate owns it -- this case only reports that exactly two broad
 * sites are left, which is the narrowing claim itself.
 *
 * Narrowing synchronization is not a change that cannot be wrong: a scoped barrier that misses
 * the stage or the access of a consumer it must order is a real hazard, and it is invisible in
 * the source -- the call still moves a barrier. So the decision here is made from the recorded
 * command stream.
 *
 * The world this case drives (one recording sequence, two targets)
 * ---------------------------------------------------------------
 *   vb_create_defaults()             the install path's own work: g.white's two layout
 *                                    transitions and the copy between them (S6), and the install
 *                                    submission (R1)
 *   vb_target_create(t4, ...)        S7: the UNDEFINED->GENERAL transition, vkCmdClearColorImage
 *                                    and the barrier that orders that clear's write against
 *                                    everything later on that image
 *   vb_target_create(t2, ...)        the same site on the second, 16-bit target
 *   vb_target_load(t4) / (t2)        S1: the load's vkCmdCopyBufferToImage and its barrier, with
 *                                    t4.stale set by vb_target_create as the production leaves it
 *   vb_depth_for(va, w, h)           S8: the depth image's transition, clear and barrier
 *   vb_begin_pass/vb_end_pass        S2: a pass's attachment writes and the barrier that orders
 *                                    them against the next pass's attachment read/write, a
 *                                    sample, the snapshot copy and the readback's image copy
 *   vb_tex_upload(&txl, &tx)         S6 on a decoded texture: UNDEFINED->TRANSFER_DST_OPTIMAL,
 *                                    the copy, TRANSFER_DST_OPTIMAL->SHADER_READ_ONLY_OPTIMAL
 *   vb_texture(&tx, t4, ...)         the same site through the back end's own texture cache
 *   vb_texture(&txa, t4, ...) x2     S9 and S5: the snapshot image's UNDEFINED->GENERAL
 *                                    transition, the self-sampling copy, and the barrier that
 *                                    orders that copy's write against the next copy's write and
 *                                    read and against the sampling of the snapshot
 *   vb_targets_flush(ts, 2)          S3: the pack dispatch and the buffer copy that reads its
 *                                    output; S4: the publish barrier and the whole batch
 *   vb_slot_submit()/vb_drain()      R1 in every submitted recording
 *   vb_flip()                        the frame boundary, through the real caller
 *
 * What it does NOT drive, and why the checks still hold
 * ----------------------------------------------------
 *   - nv2a_vk_backend_install, vb_load_loader, vb_create_instance, vb_pick_device,
 *     vb_create_device and vb_create_pipeline_objects: they need a loader, an ICD and a device.
 *     The fixture supplies the device, queue, render passes, descriptor-set layout, pipeline
 *     layout, pack pipeline and slot objects directly and sets g.ok = 1 after
 *     vb_create_defaults() returns 0, exactly where nv2a_vk_backend_install() does.
 *   - vb_draw / vb_submit / vb_draw_program: their setup (program lookup, vertex upload,
 *     pipeline creation) is not the packet's subject. The accesses they would record are
 *     recorded by the same stubs, called from the fixture: a draw inside a pass the fixture
 *     opened through vb_begin_pass records the pass's colour attachment read (the render pass is
 *     created with loadOp = LOAD) and the draw's attachment write, and a draw with a sampled
 *     image bound records a fragment shader read. The claims behind those accesses -- that the
 *     render pass loads its attachments and keeps them in GENERAL, and that a created image
 *     starts UNDEFINED -- are REQUIRED build-time anchors of scripts/vk-sync-scope-gate.py, so
 *     they cannot drift away from this fixture without breaking the build.
 *   - vb_target_for / vb_target_release / vb_target_mark: the fixture wires the two targets'
 *     guest geometry, their images and their dirty rectangles directly, as the ordering gate
 *     does, because the target cache's eviction and reshape logic is not what this packet
 *     changed.
 *   - The readback's failure paths (the abandonment branch, vb_readback_recover,
 *     vb_targets_flush_recover's second attempt, vb_stop): NOTHING fails here. That is property
 *     P4, and it is also why R2 (the retained abandonment barrier) is reported rather than
 *     decided: with no allocation failure there is no abandoned group for it to order.
 *
 * The decision: two accesses, and whether a barrier between them covers both
 * ----------------------------------------------------------------------
 * Every access the stubs see is recorded with its resource (a whole image, or a byte range of a
 * buffer), its stage flags and its access mask; every vkCmdPipelineBarrier with its source and
 * destination scopes; every submit and every fence wait with a sequence number. Then, for each
 * pair of overlapping accesses one of the classes below names, in recording order:
 *
 *   - SAME recording: ordered only by a barrier recorded STRICTLY BETWEEN them (one recorded
 *     after both cannot order them) whose source stage covers the earlier access's stage and
 *     whose source access covers the earlier access's access, and whose destination stage covers
 *     the LATER access's stage and whose destination access covers the LATER access's access.
 *     VK_ACCESS_MEMORY_WRITE_BIT is the specification's "every write" bit and covers any write
 *     access; VK_PIPELINE_STAGE_ALL_COMMANDS_BIT covers any stage. An image barrier names ONE
 *     image and is consulted only for accesses to that image; a memory barrier is global.
 *   - DIFFERENT recordings: ordered only by a submission of the earlier access's recording whose
 *     fence wait completed before the later command was recorded. That rule is deliberately
 *     STRICTER than Vulkan -- a queue-family ownership transfer, two submissions with their own
 *     signal/wait pairs, or a semaphore wait would be valid too -- so this case decides THIS
 *     implementation's stream and fails safe rather than accepting every legal alternative.
 *
 * The hazard classes, each positive-counted so that none can pass vacuously
 * -----------------------------------------------------------------------
 *   C1  an attachment write -> a later attachment read/write of the same image (S2)
 *   C2  an attachment write -> a later shader read of the same image: a fragment sample, or the
 *       pack kernel's own sampled read (S2)
 *   C3  an image transfer write (a load copy, a clear, a snapshot copy, a texture upload) -> a
 *       later attachment write, shader read, transfer read or transfer write of that image
 *       (S1, S5, S6, S7, S8)
 *   C4  the pack kernel's write of g.pack_buf -> the later buffer copy's TRANSFER_READ of the
 *       overlapping range (S3)
 *   C5  a device write of g.rb_buf (the copy engine's image copy, and the pack path's own buffer
 *       copy) -> the later host read of the overlapping range, behind the batch's fence wait (S4)
 *   C6  layout legality: every image access's layout argument equals the layout the fixture has
 *       tracked for that image; every image barrier's oldLayout equals that layout and updates it
 *       to newLayout; a SAMPLED_IMAGE descriptor needs SHADER_READ_ONLY_OPTIMAL or GENERAL, a copy
 *       destination TRANSFER_DST_OPTIMAL or GENERAL, a copy source TRANSFER_SRC_OPTIMAL or
 *       GENERAL, a colour attachment COLOR_ATTACHMENT_OPTIMAL or GENERAL, and a depth-stencil
 *       attachment DEPTH_STENCIL_ATTACHMENT_OPTIMAL or GENERAL.
 * Each class reports the pairs it decided and how many are left unordered; the check is pairs > 0
 * AND unordered == 0, so a class the fixture never reaches FAILS instead of passing vacuously.
 *
 * Mutation detectability: which property decides which row of the packet's table
 * ----------------------------------------------------------------------------
 *   M1  S2's destination drops FRAGMENT_SHADER/SHADER_READ      -> C2. The pair is "pass 1's
 *       colour attachment write -> the fragment read of that image in the very next draw", and
 *       the only barrier strictly between them is the S2 barrier recorded at that pass's end.
 *   M2  S1's srcAccessMask becomes TRANSFER_READ                -> C3. The load copy's write is
 *       no longer covered by the source access of the barrier that must order it.
 *   M3  S3's destination access becomes TRANSFER_WRITE only     -> C4. The buffer copy's
 *       TRANSFER_READ is no longer covered by the destination access.
 *   M4  S4's source stage drops TRANSFER (or TRANSFER_WRITE from its source access)  -> C5. The
 *       copy engine's write of g.rb_buf is no longer covered against the host read.
 *   M5  S5's destination drops TRANSFER_WRITE and TRANSFER_READ -> C3. The first snapshot copy's
 *       write is no longer ordered against the second copy's write of the snapshot, nor against
 *       its read of the target image.
 *   M6  S6's second transition removed (the texture stays TRANSFER_DST_OPTIMAL)   -> C6. The
 *       sampled descriptor's claimed layout no longer equals the tracked one, and the tracked one
 *       is not SHADER_READ_ONLY_OPTIMAL or GENERAL.
 *   M7  S7's layout transition removed                           -> C6. The clear passes GENERAL
 *       while the image is still tracked UNDEFINED.
 *   M8  comment-only control                                     -> nothing: it stays green, which
 *       is what separates "the defect" from "the fixture".
 *   M9  S1/S7/S8 replaced by the broad vb_barrier()              -> P3, on the count of barriers
 *       whose source stage is ALL_COMMANDS against the number of submissions. The hazard classes
 *       stay GREEN under M9 and this case says so rather than pretending otherwise: a broad
 *       ALL_COMMANDS / MEMORY_WRITE -> ALL_COMMANDS / MEMORY_READ|MEMORY_WRITE barrier really
 *       does order both sides of every pair the classes ask about (MEMORY_WRITE covers any write
 *       and ALL_COMMANDS covers any stage), so what the narrowing bought at those sites is the
 *       scope, not an ordering that was missing. P3 is the property that decides M9.
 *
 * Expected verdicts: measured against the table, not asserted
 * ----------------------------------------------------------
 * The production source passes all five properties (77 checks). Every row of the table above was
 * then built the way the packet owner builds them -- the source copied, that row's edit applied,
 * and this case rebuilt against the copy -- and every one of them COMPILES, which is the point:
 * the decision is the recording and not a grep, and each mutant is RED on the property its column
 * names. Measured: M1 C2 (1 pair unordered, no other class), M2 C3 (18), M3 C4 (1), M4 C5 (3),
 * M5 C3 (1), M6 C6 (2 accesses in the wrong layout, 2 illegal roles, 4 sampled descriptors),
 * M7 C6 (10, 30 and 4), M9 P3 (9 broad barriers for 4 submissions). The mutants' site markers all
 * go 0 as well, and the property or class the table does NOT name stays green in every case but
 * two -- and those two are reported rather than hidden: M6's removed transition was also the
 * barrier that ordered the texture upload's write before the sampling of that texture, so C3
 * loses 2 pairs as well; and M4 also fails P3's check that the publication's barrier names both
 * device writers. M8 (a comment, nothing else) is GREEN on all five properties with every marker
 * present, which is the control that separates "the defect" from "the fixture".
 *
 * The fixture's own claims, and how they are kept honest
 * -----------------------------------------------------
 * No GPU and no ICD are needed, and there is no skip path. Every Vulkan entry point the driven
 * paths reach is a static function pointer (VK_NO_PROTOTYPES) that this translation unit points
 * at a controlled stub, and the toolkit is not linked at all: its five undefined names
 * (xbox_GetMemoryOffset, nv2a_backend_register, nv2a_backend_decode_texture,
 * nv2a_backend_texture_span, nv2a_rc_unpack) are defined at the bottom of this file. The staging
 * buffer vkMapMemory hands back is the fixture's own arena, filled with the bytes the copy engine
 * or the pack kernel would have written when a batch's fence wait is behind it, so "guest memory
 * holds what the device wrote" is an exact byte comparison and not a plausible one. Nothing is
 * injected: the fixture's stubs never return a failure, which is what makes P4 (no failure path)
 * a property instead of an assumption.
 *
 * scripts/vk-sync-scope-gate.py reads the same source at BUILD time. Everything this file binds
 * to by name is a REQUIRED anchor there -- a definition, a struct field, a global, and the six
 * scope macros as VALUES, so a macro whose value differs from the packet's table fails the build
 * -- while each SITE's own scopes are MARKERS the probe reports and this file prints and checks,
 * so a source whose site moved, narrowed or was replaced by the broad barrier still COMPILES and
 * is decided from the recording rather than from a grep. The pre-N4.3 source does not build here,
 * and that is by design: this translation unit names the six scope macros in C, so the pre-change
 * source could not compile it whatever the probe did; the markers exist so that every mutant of
 * the post-change source can.
 *
 * There is no SKIP_RETURN_CODE and no skip mode in the build: the seam is inside this translation
 * unit, so the case always runs and exits 0 or 1.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies, verbatim */

#include "vk_sync_scope_gate_seam.h"            /* derived from the same source at build time */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── The fixture's geometry ─────────────────────────────────────────────────────────── */

/* Two render targets, both dirty over their whole surface, that fit one publication batch and
 * one group: a 4-byte 320x240 target (rows of 1280 bytes, slice 307200) and a 16-bit 320x240
 * target (rows of 640 bytes, packed by cs_pack, slice 153600). Their slices sum to 460800 bytes,
 * far under the production's VB_READBACK_MAX, so both publish in ONE batch; each is also far
 * under half the slot stream, so both uploads land in the slot's own stream and no private
 * buffer is made. */
#define GT_VA       0u
#define GT_PITCH    1280u
#define GT_W        320u
#define GT_H        240u
#define GT_SLICE    ((VkDeviceSize)GT_PITCH * GT_H)         /* 307200 */
#define GT_MARK     0x40u

#define GT2_VA      (GT_VA + (uint32_t)GT_SLICE)            /* 307200 */
#define GT2_PITCH   640u
#define GT2_W       320u
#define GT2_H       240u
#define GT2_SLICE   ((VkDeviceSize)GT2_PITCH * GT2_H)       /* 153600 */
#define GT2_MARK    0x50u

/* The depth image and the texture in guest ranges of their own. The texture's range must not
 * overlap a target's: vb_texture publishes a target that overlaps a guest texture before
 * decoding it, and that would add a flush this case did not ask for. */
#define GD_VA       0x00100000u
#define GTEX_VA     0x00200000u
#define GTEX_PITCH  256u
#define GTEX_W      64u
#define GTEX_H      64u
#define GTEX_SPAN   (GTEX_PITCH * GTEX_H)                   /* 16384 */
#define GTEX_MARK   0x60u

#define GATE_GUEST  ((size_t)0x00300000u)                   /* 3 MiB, past every range above */
#define GATE_CAP    ((VkDeviceSize)96u << 20)
#define GATE_READBACK ((VkDeviceSize)GT_SLICE + (VkDeviceSize)GT2_SLICE)
#define GATE_STAGE  ((size_t)(4u << 20))                    /* past the 1 MiB the pair rounds to */
#define GATE_SEED   0x5C0FFEEu
#define GATE_ROW    2048u

/* The scopes the packet's table fixes, written out as Vulkan bit names so the C side checks the
 * macros' VALUES a second time, independently of the probe's textual check. */
#define GATE_EXP_STAGE_IMAGE_DST (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT \
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT \
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT \
        | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT \
        | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT \
        | VK_PIPELINE_STAGE_TRANSFER_BIT)
#define GATE_EXP_ACCESS_IMAGE_DST (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT \
        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT \
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT \
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT \
        | VK_ACCESS_SHADER_READ_BIT \
        | VK_ACCESS_TRANSFER_READ_BIT \
        | VK_ACCESS_TRANSFER_WRITE_BIT)
#define GATE_EXP_STAGE_ATTACH (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT \
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT \
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
#define GATE_EXP_ACCESS_ATTACH_WRITE (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT \
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
#define GATE_EXP_STAGE_DEPTH (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT \
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
#define GATE_EXP_ACCESS_DEPTH (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT \
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

/* The rest of the table's scopes, named the way the production names them, so the site checks
 * read as the table does. */
#define GATE_STAGE_TOP       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
#define GATE_STAGE_TRANSFER  VK_PIPELINE_STAGE_TRANSFER_BIT
#define GATE_STAGE_COMPUTE   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
#define GATE_STAGE_HOST      VK_PIPELINE_STAGE_HOST_BIT
#define GATE_STAGE_FRAG_COMP (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT \
                              | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT)
#define GATE_STAGE_IMAGE_DST VB_STAGE_IMAGE_DST
#define GATE_ACCESS_IMAGE_DST VB_ACCESS_IMAGE_DST

#define GATE_MAX_ACC   192
#define GATE_MAX_BARS  128
#define GATE_MAX_SUBS  32
#define GATE_MAX_IMGS  64
#define GATE_MAX_VIEWS 64
#define GATE_MAX_FBS   16
#define GATE_MAX_BIND  64
#define GATE_NOTGT     0xFFFFFFFFu

/* ── The recording ─────────────────────────────────────────────────────────────────── */

/* What a recorded access is, in the terms the sites and the classes are written in. The kind
 * decides the role's layout rule and which class the access can take part in; everything else
 * about the access -- its stage flags, its access mask, its resource -- comes from the command
 * that recorded it. */
typedef enum {
    ACC_LOAD_COPY = 0,      /* vkCmdCopyBufferToImage into a target's image (S1) */
    ACC_UPLOAD_COPY,        /* vkCmdCopyBufferToImage into a texture's image (S6) */
    ACC_CLEAR_COLOR,        /* vkCmdClearColorImage (S7) */
    ACC_CLEAR_DEPTH,        /* vkCmdClearDepthStencilImage (S8) */
    ACC_ATTACH_READ,        /* a pass's colour attachment read (its LOAD) */
    ACC_ATTACH_RW,          /* a draw's colour attachment write */
    ACC_DEPTH_READ,         /* a pass's depth-stencil attachment read (its LOAD) */
    ACC_DEPTH_RW,           /* a draw's depth-stencil attachment read and write */
    ACC_SAMPLE_READ,        /* a draw's fragment-shader read of the bound sampled image */
    ACC_SNAPSHOT_SRC,       /* vkCmdCopyImage's source (the target image) */
    ACC_SNAPSHOT_DST,       /* vkCmdCopyImage's destination (the snapshot) (S5) */
    ACC_READBACK_SRC,       /* vkCmdCopyImageToBuffer's image source */
    ACC_READBACK_DST,       /* vkCmdCopyImageToBuffer's destination (g.rb_buf) */
    ACC_PACK_IMAGE,         /* the pack kernel's sampled read of the target's view */
    ACC_PACK_WRITE,         /* the pack kernel's write of g.pack_buf (S3) */
    ACC_PACK_READ,          /* vkCmdCopyBuffer's read of g.pack_buf (C4) */
    ACC_BUFFER_COPY_DST,    /* vkCmdCopyBuffer's write of g.rb_buf */
    ACC_HOST_READ,          /* the host reading g.rb_buf, behind the batch's fence wait (S4) */
    ACC_STREAM_READ         /* vkCmdCopyBufferToImage's read of the upload stream */
} GateKind;

typedef enum {
    ROLE_NONE = 0, ROLE_COLOR_ATTACH, ROLE_DEPTH_ATTACH, ROLE_COPY_DST, ROLE_COPY_SRC,
    ROLE_SAMPLED, ROLE_CLEAR_COLOR, ROLE_CLEAR_DEPTH
} GateRole;

typedef struct {
    unsigned seq;                   /* order, in one counter with the barriers */
    unsigned rec;                   /* which recording it was recorded into */
    unsigned slot;                  /* which ring slot's command buffer, or GATE_NOTGT */
    VkCommandBuffer cb;
    const char *how;                /* the command that recorded it */
    GateKind kind;
    int is_image;
    int is_write;
    VkPipelineStageFlags stage;
    VkAccessFlags access;
    /* an image access: which image, which aspect, the layout the command names, and the layout
     * the fixture had tracked when the command was recorded */
    VkImage img;
    GateRole role;
    VkImageAspectFlags aspect;
    VkImageLayout layout, tracked;
    int tracked_known;              /* 0: the image was not made inside this fixture */
    int layout_from_tracked;        /* no layout argument; only the role rule is checked */
    /* a buffer access: which buffer, which byte range */
    VkBuffer buf;
    VkDeviceSize off, size;
    unsigned tgt;                   /* which fixture target the range belongs to, or GATE_NOTGT */
    unsigned pass;                  /* the open pass it was recorded in, 0 if none */
} RecAcc;

/* One vkCmdPipelineBarrier: a memory barrier (global) or an image barrier (one image, carrying
 * the layout transition it performs). */
typedef struct {
    unsigned seq, rec, slot;
    VkCommandBuffer cb;
    int is_image;
    VkImage img;
    VkImageAspectFlags aspect;
    VkImageLayout old_layout, new_layout;
    VkPipelineStageFlags src, dst;
    VkAccessFlags srca, dsta;
    int pass_open;                  /* g.pass_open when it was recorded: a memory barrier inside a
                                     * render pass is what the gate checks must never happen */
} RecBar;

typedef struct {
    unsigned seq, rec, slot;
    VkCommandBuffer cb;
    unsigned waited_seq;            /* when a fence wait for it completed, 0 if never */
} RecSub;

typedef struct {
    VkImage img;
    VkImageLayout layout;           /* the fixture's tracked layout for that image */
    unsigned created_seq;           /* when vkCreateImage saw it */
    int used;
} GateImg;

typedef struct {
    VkImageView view;
    VkImage img;
    VkImageAspectFlags aspect;
    int used;
} GateView;

typedef struct {
    VkFramebuffer fb;
    VkImageView c, d;
    uint32_t n;
    int used;
} GateFb;

/* Every Vulkan call the driven paths reach, and what it saw. */
typedef struct {
    /* command buffers, submissions, waits */
    unsigned begins, ends, resets, submits, waits, fence_resets, pool_resets, invalidates;
    VkResult begin_result, wait_result;
    VkCommandBuffer began, ended, reset_cb;
    /* the drawing and descriptor machinery */
    unsigned draws, ds_allocs, ds_sets, updates, pipebinds, setbinds, pushes, dispatches;
    VkPipeline bound_pipe;
    VbPackPush push;                /* the pack block, copied as the production recorded it */
    VkImageView samp_view;          /* the sampled image the last descriptor write bound */
    VkImageLayout samp_layout;
    unsigned samp_binding;
    /* N4.4: every bind the driven world made, and whether it complied with the allocator's own
     * rules (a range's offset aligned to VB_POOL_ALIGN and to the requirement's alignment,
     * inside a live block, and never overlapping a live range of the same block). */
    unsigned binds_img, binds_buf, bind_violations;
    VkDeviceMemory bind_img_mem[GATE_MAX_BIND], bind_buf_mem[GATE_MAX_BIND];
    VkDeviceSize bind_img_off[GATE_MAX_BIND], bind_buf_off[GATE_MAX_BIND];
    char bind_violation[192];
    /* what the fixture supplied, as the device executing the last submitted recording */
    unsigned supply_calls, supply_writes;
    /* the stream. `seq` orders every access and barrier; `rec` counts the recordings (one per
     * vkBeginCommandBuffer), and slot_rec[] says which recording each slot's command buffer
     * currently holds. */
    unsigned seq, rec;
    unsigned slot_rec[VB_SLOTS];
    RecAcc accs[GATE_MAX_ACC];
    unsigned nacc;
    RecBar bars_rec[GATE_MAX_BARS];
    unsigned nbars;
    RecSub subs[GATE_MAX_SUBS];
    unsigned nsubs;
    /* the images, views and framebuffers the production made, and the layout tracker */
    GateImg imgs[GATE_MAX_IMGS];
    unsigned nimgs;
    GateView views[GATE_MAX_VIEWS];
    unsigned nviews;
    GateFb fbs[GATE_MAX_FBS];
    unsigned nfbs;
    /* the driver's own bookkeeping */
    unsigned pass_live;             /* the pass that is open, 0 if none (the fixture's own) */
    unsigned passes;                /* passes begun */
    unsigned dead_records;          /* commands recorded into g.dead (nothing may be) */
    unsigned other_calls;           /* a wired entry point the driven paths never call */
    unsigned mem_bars, img_bars, broad_bars, host_bars, mem_bars_in_pass;
    unsigned lay_checked, lay_bad, role_checked, role_bad, trans_checked, trans_bad;
    unsigned img_not_undef;         /* images the production asked to create in another layout:
                                     * every transition out of UNDEFINED assumes UNDEFINED */
    unsigned desc_checked, desc_bad;
    unsigned pass_failed;
    unsigned begin_failures;
    /* what the fixture's own stubs handed out */
    VkDescriptorPool dpool;
    VkDescriptorSet samp_set;
    /* the fixture's targets, so a staging write can be attributed and its bytes compared */
    VkImage tgt_img[2];
    uint32_t tgt_bpp[2];
    uint32_t tgt_mark[2];
    unsigned pack_target;
} GateCtl;

static GateCtl c;

/* The allocator's own rules, applied to every bind this fixture sees. A violation is counted
 * and named, not raised: property 5 asserts the count is zero. */
/* N4.4: what both requirement stubs report. The size is deliberately NOT a multiple of the
 * alignment, so a block that packs ranges without aligning them (the mutant the private
 * campaign builds) lands on offsets that are not multiples of the alignment, and the bind
 * property below can decide it. */
#define GATE_BIND_SIZE  4100u

static void bind_note(int is_image, VkDeviceMemory mem, VkDeviceSize off, VkDeviceSize align)
{
    unsigned p, b, i, n;
    int found = 0;

    if (is_image) {
        n = c.binds_img < GATE_MAX_BIND ? c.binds_img : GATE_MAX_BIND - 1u;
        c.bind_img_mem[n] = mem;
        c.bind_img_off[n] = off;
        c.binds_img++;
    } else {
        n = c.binds_buf < GATE_MAX_BIND ? c.binds_buf : GATE_MAX_BIND - 1u;
        c.bind_buf_mem[n] = mem;
        c.bind_buf_off[n] = off;
        c.binds_buf++;
    }
    if (off % VB_POOL_ALIGN || (align && off % align)) {
        c.bind_violations++;
        snprintf(c.bind_violation, sizeof c.bind_violation,
                 "%s bound at offset %llu: not a multiple of %llu (pool floor %u)",
                 is_image ? "an image" : "a buffer", (unsigned long long)off,
                 (unsigned long long)(align ? align : (VkDeviceSize)VB_POOL_ALIGN),
                 (unsigned)VB_POOL_ALIGN);
    }
    for (p = 0; p < VB_POOLS && !found; p++) {
        for (b = 0; b < g.pools[p].nblocks && !found; b++) {
            if (g.pools[p].blocks[b].mem != mem)
                continue;
            found = 1;
            if (off + GATE_BIND_SIZE > g.pools[p].blocks[b].size) {
                c.bind_violations++;
                snprintf(c.bind_violation, sizeof c.bind_violation,
                         "the range [%llu, %llu) runs past its block's %llu bytes",
                         (unsigned long long)off,
                         (unsigned long long)(off + GATE_BIND_SIZE),
                         (unsigned long long)g.pools[p].blocks[b].size);
            }
            if (is_image != (g.pools[p].nonlinear != 0)) {
                c.bind_violations++;
                snprintf(c.bind_violation, sizeof c.bind_violation,
                         "a%s resource is bound into a %s pool",
                         is_image ? "n image" : " buffer",
                         g.pools[p].nonlinear ? "non-linear (image)" : "linear (buffer)");
            }
        }
    }
    if (!found) {
        c.bind_violations++;
        snprintf(c.bind_violation, sizeof c.bind_violation,
                 "%s bound to memory %p, which is no live block of this back end",
                 is_image ? "an image" : " a buffer", (void *)mem);
    }
    /* No two live ranges of one block may overlap. */
    for (i = 0; i + 1u < (is_image ? c.binds_img : c.binds_buf) && i < GATE_MAX_BIND; i++) {
        const VkDeviceMemory *m = is_image ? c.bind_img_mem : c.bind_buf_mem;
        const VkDeviceSize *o = is_image ? c.bind_img_off : c.bind_buf_off;
        unsigned j;

        for (j = i + 1u; j < (is_image ? c.binds_img : c.binds_buf) && j < GATE_MAX_BIND; j++) {
            if (m[i] != m[j])
                continue;
            if (o[i] < o[j] + GATE_BIND_SIZE && o[j] < o[i] + GATE_BIND_SIZE) {
                c.bind_violations++;
                snprintf(c.bind_violation, sizeof c.bind_violation,
                         "two %ss bound to overlapping ranges [%llu] and [%llu] of one block",
                         is_image ? "image" : "buffer", (unsigned long long)o[i],
                         (unsigned long long)o[j]);
            }
        }
    }
}

static uint8_t *g_guest;                        /* xbox_GetMemoryOffset()'s answer */
static uint8_t *g_stage;                        /* what vkMapMemory hands back */
static uint8_t *g_row;                          /* one row of the fixture's own bytes */
static uint8_t *g_stream[VB_SLOTS];             /* one mapped slot stream each */

static uintptr_t g_handle = 0x8000u;

static void *hnew(void)
{
    g_handle += 16u;
    return (void *)g_handle;
}

static unsigned slot_of(VkCommandBuffer cb)
{
    unsigned i;

    for (i = 0; i < VB_SLOTS; i++)
        if (g.slots[i].cmd == cb)
            return i;
    return GATE_NOTGT;
}

static unsigned rec_of(VkCommandBuffer cb)
{
    unsigned i = slot_of(cb);

    return i == GATE_NOTGT ? 0u : c.slot_rec[i];
}

static GateImg *img_track(VkImage img)
{
    unsigned i;

    for (i = 0; i < c.nimgs; i++)
        if (c.imgs[i].used && c.imgs[i].img == img)
            return &c.imgs[i];
    return NULL;
}

/* Register an image the production created, with the layout vkCreateImage declared. */
static GateImg *img_new(VkImage img, VkImageLayout layout)
{
    GateImg *e = img_track(img);
    unsigned i;

    if (e)
        return e;
    for (i = 0; i < GATE_MAX_IMGS; i++)
        if (!c.imgs[i].used) {
            e = &c.imgs[i];
            break;
        }
    if (!e) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: the image tracker overflowed (%u)\n",
                (unsigned)GATE_MAX_IMGS);
        return NULL;
    }
    e->used = 1;
    e->img = img;
    e->layout = layout;
    e->created_seq = ++c.seq;
    if (c.nimgs <= i)
        c.nimgs = i + 1u;
    return e;
}

/* Which of the fixture's two targets an image (and so a staging range) belongs to. */
static unsigned target_of_image(VkImage img)
{
    unsigned i;

    for (i = 0; i < 2u; i++)
        if (c.tgt_img[i] == img)
            return i;
    return GATE_NOTGT;
}

static uint32_t bpp_of_image(VkImage img)
{
    unsigned i = target_of_image(img);

    return i == GATE_NOTGT ? 4u : c.tgt_bpp[i];
}

static GateView *view_of(VkImageView v)
{
    unsigned i;

    for (i = 0; i < c.nviews; i++)
        if (c.views[i].used && c.views[i].view == v)
            return &c.views[i];
    return NULL;
}

static GateFb *fb_of(VkFramebuffer fb)
{
    unsigned i;

    for (i = 0; i < c.nfbs; i++)
        if (c.fbs[i].used && c.fbs[i].fb == fb)
            return &c.fbs[i];
    return NULL;
}

/* The layout rule a role's accesses must satisfy. GENERAL is legal for every role: it is the
 * layout a target, a depth image and the self-sample snapshot live in for their whole life,
 * precisely because each of them is an attachment, a sampled image and a copy source or
 * destination in the same submission. */
static int layout_legal(GateRole role, VkImageLayout l)
{
    switch (role) {
    case ROLE_SAMPLED:
        return l == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_COPY_DST:
        return l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_COPY_SRC:
        return l == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_COLOR_ATTACH:
        return l == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_DEPTH_ATTACH:
        return l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_CLEAR_COLOR:
        return l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            || l == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            || l == VK_IMAGE_LAYOUT_GENERAL;
    case ROLE_CLEAR_DEPTH:
        return l == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            || l == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            || l == VK_IMAGE_LAYOUT_GENERAL;
    default:
        return 1;
    }
}

static GateRole role_of(GateKind k)
{
    switch (k) {
    case ACC_LOAD_COPY:
    case ACC_UPLOAD_COPY:
    case ACC_SNAPSHOT_DST:
        return ROLE_COPY_DST;
    case ACC_SNAPSHOT_SRC:
    case ACC_READBACK_SRC:
        return ROLE_COPY_SRC;
    case ACC_CLEAR_COLOR:
        return ROLE_CLEAR_COLOR;
    case ACC_CLEAR_DEPTH:
        return ROLE_CLEAR_DEPTH;
    case ACC_ATTACH_READ:
    case ACC_ATTACH_RW:
        return ROLE_COLOR_ATTACH;
    case ACC_DEPTH_READ:
    case ACC_DEPTH_RW:
        return ROLE_DEPTH_ATTACH;
    case ACC_SAMPLE_READ:
    case ACC_PACK_IMAGE:
        return ROLE_SAMPLED;
    default:
        return ROLE_NONE;
    }
}

static int kind_writes(GateKind k)
{
    switch (k) {
    case ACC_LOAD_COPY:
    case ACC_UPLOAD_COPY:
    case ACC_CLEAR_COLOR:
    case ACC_CLEAR_DEPTH:
    case ACC_ATTACH_RW:
    case ACC_DEPTH_RW:
    case ACC_SNAPSHOT_DST:
    case ACC_READBACK_DST:
    case ACC_PACK_WRITE:
    case ACC_BUFFER_COPY_DST:
        return 1;
    default:
        return 0;
    }
}

static const char *kind_name(GateKind k)
{
    switch (k) {
    case ACC_LOAD_COPY:       return "the load's copyBufferToImage";
    case ACC_UPLOAD_COPY:     return "the upload's copyBufferToImage";
    case ACC_CLEAR_COLOR:     return "clearColorImage";
    case ACC_CLEAR_DEPTH:     return "clearDepthStencilImage";
    case ACC_ATTACH_READ:     return "the pass's colour attachment read (LOAD)";
    case ACC_ATTACH_RW:       return "the draw's colour attachment write";
    case ACC_DEPTH_READ:      return "the pass's depth-stencil attachment read (LOAD)";
    case ACC_DEPTH_RW:        return "the draw's depth-stencil attachment read+write";
    case ACC_SAMPLE_READ:     return "the draw's fragment sample read";
    case ACC_SNAPSHOT_SRC:    return "the snapshot copy's source read";
    case ACC_SNAPSHOT_DST:    return "the snapshot copy's destination write";
    case ACC_READBACK_SRC:    return "the readback copy's image read";
    case ACC_READBACK_DST:    return "the readback copy's staging write";
    case ACC_PACK_IMAGE:      return "the pack kernel's sampled read";
    case ACC_PACK_WRITE:      return "the pack kernel's pack-buffer write";
    case ACC_PACK_READ:       return "the buffer copy's pack-buffer read";
    case ACC_BUFFER_COPY_DST: return "the buffer copy's staging write";
    case ACC_HOST_READ:       return "the host read of the staging buffer";
    case ACC_STREAM_READ:     return "the upload copy's stream read";
    default:                  return "?";
    }
}

/* ── Recording one access, one barrier, one submission ───────────────────────────────── */

static RecAcc *acc_new(const char *how, GateKind kind, VkPipelineStageFlags stage,
                       VkAccessFlags access, VkCommandBuffer cb)
{
    RecAcc *a;

    if (c.nacc >= GATE_MAX_ACC) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: the access log overflowed (%u)\n",
                (unsigned)GATE_MAX_ACC);
        return NULL;
    }
    a = &c.accs[c.nacc++];
    memset(a, 0, sizeof *a);
    a->seq = ++c.seq;
    a->rec = rec_of(cb);
    a->slot = slot_of(cb);
    a->cb = cb;
    a->how = how;
    a->kind = kind;
    a->is_write = kind_writes(kind);
    a->stage = stage;
    a->access = access;
    a->role = role_of(kind);
    a->tgt = GATE_NOTGT;
    a->pass = c.pass_live;
    if (g.failed || cb == g.dead)
        c.dead_records++;
    return a;
}

/* An image access: the image, its aspect, and the layout the command names. The tracked layout is
 * read NOW, so C6 can compare the two afterwards even though a later transition moved it on. */
static void acc_image(RecAcc *a, VkImage img, VkImageLayout layout, VkImageAspectFlags aspect)
{
    GateImg *t;

    if (!a)
        return;
    a->is_image = 1;
    a->img = img;
    a->aspect = aspect;
    a->layout = layout;
    t = img_track(img);
    if (t) {
        a->tracked = t->layout;
        a->tracked_known = 1;
        c.lay_checked++;
        if (!a->layout_from_tracked && a->layout != t->layout)
            c.lay_bad++;
        if (a->role != ROLE_NONE) {
            c.role_checked++;
            if (!layout_legal(a->role, t->layout))
                c.role_bad++;
        }
    }
}

static void acc_buffer(RecAcc *a, VkBuffer buf, VkDeviceSize off, VkDeviceSize size)
{
    if (!a)
        return;
    a->is_image = 0;
    a->buf = buf;
    a->off = off;
    a->size = size;
}

static void rec_mem_bar(VkCommandBuffer cb, VkPipelineStageFlags src, VkPipelineStageFlags dst,
                        VkAccessFlags srca, VkAccessFlags dsta)
{
    RecBar *b;

    if (c.nbars >= GATE_MAX_BARS) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: the barrier log overflowed (%u)\n",
                (unsigned)GATE_MAX_BARS);
        return;
    }
    b = &c.bars_rec[c.nbars++];
    memset(b, 0, sizeof *b);
    b->seq = ++c.seq;
    b->cb = cb;
    b->rec = rec_of(cb);
    b->slot = slot_of(cb);
    b->src = src;
    b->dst = dst;
    b->srca = srca;
    b->dsta = dsta;
    b->pass_open = g.pass_open;
    c.mem_bars++;
    if (g.pass_open)
        c.mem_bars_in_pass++;
    if (src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
        c.broad_bars++;
    if (dst & VK_PIPELINE_STAGE_HOST_BIT)
        c.host_bars++;
}

static void rec_img_bar(VkCommandBuffer cb, VkPipelineStageFlags src, VkPipelineStageFlags dst,
                        VkAccessFlags srca, VkAccessFlags dsta, VkImage img,
                        VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect)
{
    RecBar *b;
    GateImg *t = img_track(img);

    if (c.nbars >= GATE_MAX_BARS) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: the barrier log overflowed (%u)\n",
                (unsigned)GATE_MAX_BARS);
        return;
    }
    b = &c.bars_rec[c.nbars++];
    memset(b, 0, sizeof *b);
    b->seq = ++c.seq;
    b->cb = cb;
    b->rec = rec_of(cb);
    b->slot = slot_of(cb);
    b->src = src;
    b->dst = dst;
    b->srca = srca;
    b->dsta = dsta;
    b->is_image = 1;
    b->img = img;
    b->aspect = aspect;
    b->old_layout = from;
    b->new_layout = to;
    b->pass_open = g.pass_open;
    c.img_bars++;
    if (g.pass_open)
        c.mem_bars_in_pass++;
    if (src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
        c.broad_bars++;
    /* C6's transition half: oldLayout must be the layout the fixture had tracked, and the
     * transition moves it to newLayout. */
    if (t) {
        c.trans_checked++;
        if (t->layout != from)
            c.trans_bad++;
        t->layout = to;
    }
}

/* ── The stubs ──────────────────────────────────────────────────────────────────────── */

/* Defined with the rest of the fixture's state: the bytes the device would have written for a
 * submitted batch, supplied when that batch's fence wait is behind it. */
static void gate_supply(void);

static VkResult VKAPI_CALL gate_begin(VkCommandBuffer cmd, const VkCommandBufferBeginInfo *info)
{
    unsigned slot = slot_of(cmd);

    (void)info;
    c.begins++;
    c.began = cmd;
    c.slot_rec[slot == GATE_NOTGT ? 0u : slot] = ++c.rec;
    c.seq++;
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
    VkCommandBuffer cb = VK_NULL_HANDLE;

    (void)queue; (void)fence;
    c.submits++;
    if (count && submits[0].commandBufferCount)
        cb = submits[0].pCommandBuffers[0];
    if (c.nsubs < GATE_MAX_SUBS) {
        RecSub *s = &c.subs[c.nsubs++];

        s->seq = ++c.seq;
        s->cb = cb;
        s->rec = rec_of(cb);
        s->slot = slot_of(cb);
        s->waited_seq = 0;
    }
    return VK_SUCCESS;
}

/* A fence wait: the ring's fences are one per slot, so the submission it completes is that
 * slot's latest unwaited one. That is the boundary a later recording is ordered by. */
static VkResult VKAPI_CALL gate_wait(VkDevice device, uint32_t count, const VkFence *fences,
                                     VkBool32 all, uint64_t timeout)
{
    unsigned i;

    (void)device; (void)all; (void)timeout;
    c.waits++;
    if (count && fences) {
        for (i = 0; i < VB_SLOTS; i++)
            if (g.slots[i].fence == fences[0]) {
                unsigned k;

                for (k = c.nsubs; k > 0u; k--)
                    if (c.subs[k - 1u].slot == i && !c.subs[k - 1u].waited_seq) {
                        c.subs[k - 1u].waited_seq = c.seq + 1u;
                        break;
                    }
                break;
            }
    }
    c.seq++;
    return c.wait_result;
}

static VkResult VKAPI_CALL gate_fence_reset(VkDevice device, uint32_t count,
                                            const VkFence *fences)
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

/* ── Images, views, framebuffers ─────────────────────────────────────────────────────── */

static VkResult VKAPI_CALL gate_image_create(VkDevice device, const VkImageCreateInfo *info,
                                             const VkAllocationCallbacks *alloc, VkImage *img)
{
    (void)device; (void)alloc;
    /* The transitions the case checks all name UNDEFINED as the layout they transition FROM, so
     * an image created in another layout would make every one of them illegal: the fixture
     * records the layout the production asked for and fails the check if it is not UNDEFINED. */
    if (!info || info->initialLayout != VK_IMAGE_LAYOUT_UNDEFINED)
        c.img_not_undef++;
    *img = (VkImage)hnew();
    img_new(*img, info ? info->initialLayout : VK_IMAGE_LAYOUT_UNDEFINED);
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_image_requirements(VkDevice device, VkImage img,
                                               VkMemoryRequirements *req)
{
    (void)device; (void)img;
    req->size = GATE_BIND_SIZE;
    req->alignment = 4096u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_image_bind(VkDevice device, VkImage img, VkDeviceMemory mem,
                                           VkDeviceSize offset)
{
    (void)device; (void)img;
    bind_note(1, mem, offset, 4096u);       /* the requirement stub's own alignment */
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_image_destroy(VkDevice device, VkImage img,
                                          const VkAllocationCallbacks *alloc)
{
    (void)device; (void)img; (void)alloc;
    c.other_calls++;
}

static VkResult VKAPI_CALL gate_view_create(VkDevice device, const VkImageViewCreateInfo *info,
                                            const VkAllocationCallbacks *alloc,
                                            VkImageView *view)
{
    unsigned i;

    (void)device; (void)alloc;
    *view = (VkImageView)hnew();
    for (i = 0; i < GATE_MAX_VIEWS; i++)
        if (!c.views[i].used) {
            c.views[i].used = 1;
            c.views[i].view = *view;
            c.views[i].img = info ? info->image : VK_NULL_HANDLE;
            c.views[i].aspect = info ? info->subresourceRange.aspectMask
                                     : VK_IMAGE_ASPECT_COLOR_BIT;
            if (c.nviews <= i)
                c.nviews = i + 1u;
            break;
        }
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_view_destroy(VkDevice device, VkImageView view,
                                         const VkAllocationCallbacks *alloc)
{
    (void)device; (void)view; (void)alloc;
    c.other_calls++;
}

static VkResult VKAPI_CALL gate_sampler_create(VkDevice device, const VkSamplerCreateInfo *info,
                                               const VkAllocationCallbacks *alloc,
                                               VkSampler *samp)
{
    (void)device; (void)info; (void)alloc;
    *samp = (VkSampler)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_fb_create(VkDevice device, const VkFramebufferCreateInfo *info,
                                          const VkAllocationCallbacks *alloc, VkFramebuffer *fb)
{
    unsigned i;

    (void)device; (void)alloc;
    *fb = (VkFramebuffer)hnew();
    for (i = 0; i < GATE_MAX_FBS; i++)
        if (!c.fbs[i].used) {
            c.fbs[i].used = 1;
            c.fbs[i].fb = *fb;
            c.fbs[i].n = info ? info->attachmentCount : 0u;
            c.fbs[i].c = (info && info->attachmentCount > 0u) ? info->pAttachments[0]
                                                              : VK_NULL_HANDLE;
            c.fbs[i].d = (info && info->attachmentCount > 1u) ? info->pAttachments[1]
                                                              : VK_NULL_HANDLE;
            if (c.nfbs <= i)
                c.nfbs = i + 1u;
            break;
        }
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_fb_destroy(VkDevice device, VkFramebuffer fb,
                                       const VkAllocationCallbacks *alloc)
{
    (void)device; (void)fb; (void)alloc;
    c.other_calls++;
}

/* ── Render passes ───────────────────────────────────────────────────────────────────── */

/* The pass's own reads. The production creates both render passes with loadOp = LOAD and
 * initialLayout = finalLayout = GENERAL (that claim is a build-time anchor), so a pass's start
 * READS its colour attachment at COLOR_ATTACHMENT_OUTPUT and its depth-stencil attachment at the
 * two fragment test stages, both in the layout the image is in. Those reads are recorded through
 * the framebuffer the production built, since the pass's own globals (g.pass_t, g.pass_d) are set
 * AFTER vkCmdBeginRenderPass returns. */
static void VKAPI_CALL gate_begin_pass(VkCommandBuffer cmd, const VkRenderPassBeginInfo *bi,
                                       VkSubpassContents contents)
{
    GateFb *f = bi ? fb_of(bi->framebuffer) : NULL;
    unsigned n;

    (void)contents;
    c.passes++;
    c.pass_live = c.passes;
    if (!f)
        return;
    for (n = 0; n < f->n && n < 2u; n++) {
        GateView *v = view_of(n == 0u ? f->c : f->d);
        GateImg *t;
        RecAcc *a;

        if (!v)
            continue;
        t = img_track(v->img);
        if (n == 0u)
            a = acc_new("vkCmdBeginRenderPass (the colour attachment is LOADed)",
                        ACC_ATTACH_READ, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, cmd);
        else
            a = acc_new("vkCmdBeginRenderPass (the depth attachment is LOADed)",
                        ACC_DEPTH_READ, GATE_EXP_STAGE_DEPTH,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, cmd);
        if (a)
            a->layout_from_tracked = 1;
        acc_image(a, v->img, t ? t->layout : VK_IMAGE_LAYOUT_GENERAL, v->aspect);
    }
}

static void VKAPI_CALL gate_end_pass(VkCommandBuffer cmd)
{
    (void)cmd;
    c.pass_live = 0;
}

/* ── The commands that move bytes ────────────────────────────────────────────────────── */

/* The row bytes of a copy whose region may leave bufferRowLength 0 (a tight copy). */
static VkDeviceSize copy_bytes(const VkBufferImageCopy *r, uint32_t bpp)
{
    uint32_t rows = r->imageExtent.height ? r->imageExtent.height : 1u;
    uint32_t px = r->bufferRowLength ? r->bufferRowLength
                                     : (r->imageExtent.width ? r->imageExtent.width : 1u);

    return (VkDeviceSize)px * (VkDeviceSize)bpp * (VkDeviceSize)rows;
}

static void VKAPI_CALL gate_copy_to_image(VkCommandBuffer cmd, VkBuffer src, VkImage dst,
                                          VkImageLayout layout, uint32_t count,
                                          const VkBufferImageCopy *regions)
{
    RecAcc *r;
    RecAcc *w;
    VkDeviceSize off = 0, size = 0;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    GateKind kind;

    if (count && regions) {
        off = regions[0].bufferOffset;
        aspect = regions[0].imageSubresource.aspectMask;
        /* The stream is always B8R8G8A8 rows: the row length in pixels, four bytes each. */
        size = copy_bytes(&regions[0], 4u);
    }
    kind = target_of_image(dst) == GATE_NOTGT ? ACC_UPLOAD_COPY : ACC_LOAD_COPY;
    r = acc_new(kind == ACC_LOAD_COPY ? "the load's stream slice"
                                      : "the upload's stream slice",
                ACC_STREAM_READ, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, cmd);
    acc_buffer(r, src, off, size);
    w = acc_new(kind_name(kind), kind, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, cmd);
    acc_image(w, dst, layout, aspect);
    if (w)
        w->tgt = target_of_image(dst);
}

static void VKAPI_CALL gate_copy_to_buffer(VkCommandBuffer cmd, VkImage src,
                                           VkImageLayout layout, VkBuffer dst, uint32_t count,
                                           const VkBufferImageCopy *regions)
{
    unsigned target = target_of_image(src);
    VkDeviceSize off = 0, size = 0;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    RecAcc *w;

    if (count && regions) {
        off = regions[0].bufferOffset;
        aspect = regions[0].imageSubresource.aspectMask;
        size = copy_bytes(&regions[0], bpp_of_image(src));
    }
    acc_image(acc_new("vkCmdCopyImageToBuffer (the target image)", ACC_READBACK_SRC,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, cmd),
              src, layout, aspect);
    w = acc_new("vkCmdCopyImageToBuffer (the staging buffer)", ACC_READBACK_DST,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, cmd);
    acc_buffer(w, dst, off, size);
    if (w)
        w->tgt = target;
}

static void VKAPI_CALL gate_copy_buffer(VkCommandBuffer cmd, VkBuffer src, VkBuffer dst,
                                        uint32_t count, const VkBufferCopy *regions)
{
    VkDeviceSize src_off = 0, dst_off = 0, size = 0;
    RecAcc *w;

    if (count && regions) {
        src_off = regions[0].srcOffset;
        dst_off = regions[0].dstOffset;
        size = regions[0].size;
    }
    /* The read first: the copy reads the packed slice out of the pack buffer, and the pack path's
     * own barrier is recorded between that read and... no: between the kernel's WRITE and this
     * READ. Recording the read before the write of the same command keeps the two ends of the
     * pair in the order the command stream has them. */
    acc_buffer(acc_new("vkCmdCopyBuffer (the pack output read)", ACC_PACK_READ,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, cmd),
               src, src_off, size);
    w = acc_new("vkCmdCopyBuffer (the staging buffer)", ACC_BUFFER_COPY_DST,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, cmd);
    acc_buffer(w, dst, dst_off, size);
    if (w)
        w->tgt = c.pack_target;
}

static void VKAPI_CALL gate_copy_image(VkCommandBuffer cmd, VkImage src, VkImageLayout src_layout,
                                       VkImage dst, VkImageLayout dst_layout, uint32_t count,
                                       const VkImageCopy *regions)
{
    VkImageAspectFlags sa = VK_IMAGE_ASPECT_COLOR_BIT, da = VK_IMAGE_ASPECT_COLOR_BIT;

    if (count && regions) {
        sa = regions[0].srcSubresource.aspectMask;
        da = regions[0].dstSubresource.aspectMask;
    }
    acc_image(acc_new("vkCmdCopyImage (the target image)", ACC_SNAPSHOT_SRC,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT, cmd),
              src, src_layout, sa);
    acc_image(acc_new("vkCmdCopyImage (the snapshot image)", ACC_SNAPSHOT_DST,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, cmd),
              dst, dst_layout, da);
}

static void VKAPI_CALL gate_clear_color(VkCommandBuffer cmd, VkImage img, VkImageLayout layout,
                                        const VkClearColorValue *value, uint32_t count,
                                        const VkImageSubresourceRange *ranges)
{
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    (void)value;
    if (count && ranges)
        aspect = ranges[0].aspectMask;
    acc_image(acc_new("vkCmdClearColorImage", ACC_CLEAR_COLOR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                      VK_ACCESS_TRANSFER_WRITE_BIT, cmd), img, layout, aspect);
}

static void VKAPI_CALL gate_clear_depth(VkCommandBuffer cmd, VkImage img, VkImageLayout layout,
                                        const VkClearDepthStencilValue *value, uint32_t count,
                                        const VkImageSubresourceRange *ranges)
{
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

    (void)value;
    if (count && ranges)
        aspect = ranges[0].aspectMask;
    acc_image(acc_new("vkCmdClearDepthStencilImage", ACC_CLEAR_DEPTH,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, cmd),
              img, layout, aspect);
}

/* ── The pack kernel, a draw, and the descriptors behind them ─────────────────────────── */

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

static void VKAPI_CALL gate_bind_pipeline(VkCommandBuffer cmd, VkPipelineBindPoint point,
                                          VkPipeline pipe)
{
    (void)cmd; (void)point;
    c.pipebinds++;
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

/* The pack kernel's write: the dispatch stores the packed rectangle into the device-local pack
 * buffer, at the byte offset and row stride its push block carries. Its sampled read of the
 * target's own view is recorded FIRST, so the barrier the pack path records next is unambiguously
 * the one between that write and the buffer copy that reads it. */
static void VKAPI_CALL gate_dispatch(VkCommandBuffer cmd, uint32_t x, uint32_t y, uint32_t z)
{
    VkDeviceSize off = (VkDeviceSize)c.push.dst[0];
    VkDeviceSize size = (VkDeviceSize)c.push.dst[1] * (VkDeviceSize)c.push.rect[3];
    GateView *v;
    RecAcc *a;

    (void)x; (void)y; (void)z;
    c.dispatches++;
    if (c.samp_view) {
        v = view_of(c.samp_view);
        a = acc_new("the pack set's sampled image", ACC_PACK_IMAGE,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, cmd);
        if (a)
            a->tgt = v ? target_of_image(v->img) : GATE_NOTGT;
        if (v)
            acc_image(a, v->img, c.samp_layout, v->aspect);
        c.samp_view = VK_NULL_HANDLE;
    }
    a = acc_new("vkCmdDispatch (the pack kernel's write)", ACC_PACK_WRITE,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, cmd);
    acc_buffer(a, g.pack_buf, off, size);
    if (a)
        a->tgt = c.pack_target;
}

/* A draw: its attachments while a pass is open, and the fragment-shader read of whatever sampled
 * image is bound. The fixture calls this itself (it does not drive vb_draw's setup), which is
 * what records the attachment writes the S2 site must order. */
static void VKAPI_CALL gate_draw(VkCommandBuffer cmd, uint32_t count, uint32_t icount,
                                 uint32_t first, uint32_t fv)
{
    RecAcc *a;
    GateImg *t;

    (void)count; (void)icount; (void)first; (void)fv;
    c.draws++;
    if (g.pass_open && g.pass_t) {
        t = img_track(g.pass_t->im.img);
        a = acc_new("vkCmdDraw (the colour attachment write)", ACC_ATTACH_RW,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, cmd);
        if (a)
            a->layout_from_tracked = 1;
        acc_image(a, g.pass_t->im.img, t ? t->layout : VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_ASPECT_COLOR_BIT);
    }
    if (g.pass_open && g.pass_d) {
        t = img_track(g.pass_d->im.img);
        a = acc_new("vkCmdDraw (the depth-stencil attachment read+write)", ACC_DEPTH_RW,
                    GATE_EXP_STAGE_DEPTH,
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, cmd);
        if (a)
            a->layout_from_tracked = 1;
        acc_image(a, g.pass_d->im.img, t ? t->layout : VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    }
    if (c.samp_view) {
        GateView *v = view_of(c.samp_view);

        a = acc_new("vkCmdDraw (the bound sampled image is read)", ACC_SAMPLE_READ,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, cmd);
        if (v)
            acc_image(a, v->img, c.samp_layout, v->aspect);
        c.samp_view = VK_NULL_HANDLE;
    }
}

/* A descriptor write: what the shader will sample. Recording it here is what lets the fixture
 * check the SAMPLED_IMAGE rule (and, for the pack path, attribute the dispatch to its target);
 * the device ACCESS is recorded where the shader runs. */
static void VKAPI_CALL gate_ds_update(VkDevice device, uint32_t writes,
                                      const VkWriteDescriptorSet *w, uint32_t copies,
                                      const VkCopyDescriptorSet *cp)
{
    unsigned i;

    (void)device; (void)copies; (void)cp;
    c.updates++;
    for (i = 0; w && i < writes; i++) {
        GateView *v;
        GateImg *t;

        if (w[i].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || !w[i].pImageInfo)
            continue;
        c.samp_view = w[i].pImageInfo[0].imageView;
        c.samp_layout = w[i].pImageInfo[0].imageLayout;
        c.samp_binding = w[i].dstBinding;
        v = view_of(c.samp_view);
        if (!v)
            continue;
        c.pack_target = target_of_image(v->img);
        t = img_track(v->img);
        if (t) {
            c.desc_checked++;
            /* The descriptor's layout must be the layout the image is really in, and the tracked
             * layout must be one a SAMPLED_IMAGE descriptor is allowed to name. */
            if (c.samp_layout != t->layout)
                c.desc_bad++;
            if (!layout_legal(ROLE_SAMPLED, t->layout))
                c.desc_bad++;
        }
    }
}

static VkResult VKAPI_CALL gate_ds_alloc(VkDevice device, const VkDescriptorSetAllocateInfo *info,
                                         VkDescriptorSet *set)
{
    (void)device;
    c.ds_allocs++;
    c.dpool = info ? info->descriptorPool : VK_NULL_HANDLE;
    *set = (VkDescriptorSet)hnew();
    c.ds_sets++;
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_destroy_pipeline(VkDevice device, VkPipeline pipe,
                                             const VkAllocationCallbacks *alloc)
{
    (void)device; (void)pipe; (void)alloc;
    c.other_calls++;
}

/* ── The barriers ───────────────────────────────────────────────────────────────────── */

/* One vkCmdPipelineBarrier: a memory barrier and/or an image barrier. What it was recorded
 * through (the production's own vkCmdPipelineBarrier) is where the scope comes from; the fixture
 * only records it. */
static void VKAPI_CALL gate_barrier(VkCommandBuffer cmd, VkPipelineStageFlags src,
                                    VkPipelineStageFlags dst, VkDependencyFlags flags,
                                    uint32_t mbc, const VkMemoryBarrier *mb, uint32_t bbc,
                                    const VkBufferMemoryBarrier *bb, uint32_t ibc,
                                    const VkImageMemoryBarrier *ib)
{
    unsigned i;

    (void)flags; (void)bb;
    if (mbc && mb)
        rec_mem_bar(cmd, src, dst, mb[0].srcAccessMask, mb[0].dstAccessMask);
    for (i = 0; ibc && ib && i < ibc; i++)
        rec_img_bar(cmd, src, dst, ib[i].srcAccessMask, ib[i].dstAccessMask, ib[i].image,
                    ib[i].oldLayout, ib[i].newLayout, ib[i].subresourceRange.aspectMask);
}

/* ── Buffers, memory, mapping ───────────────────────────────────────────────────────── */

static VkResult VKAPI_CALL gate_buffer_create(VkDevice device, const VkBufferCreateInfo *info,
                                              const VkAllocationCallbacks *alloc, VkBuffer *buf)
{
    (void)device; (void)info; (void)alloc;
    *buf = (VkBuffer)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_buffer_requirements(VkDevice device, VkBuffer buf,
                                                VkMemoryRequirements *req)
{
    (void)device; (void)buf;
    req->size = GATE_BIND_SIZE;
    req->alignment = 4096u;
    req->memoryTypeBits = 1u;
}

static VkResult VKAPI_CALL gate_buffer_bind(VkDevice device, VkBuffer buf, VkDeviceMemory mem,
                                            VkDeviceSize offset)
{
    (void)device; (void)buf;
    bind_note(0, mem, offset, 4096u);
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_buffer_destroy(VkDevice device, VkBuffer buf,
                                           const VkAllocationCallbacks *alloc)
{
    (void)device; (void)buf; (void)alloc;
    c.other_calls++;
}

static VkResult VKAPI_CALL gate_allocate(VkDevice device, const VkMemoryAllocateInfo *info,
                                         const VkAllocationCallbacks *alloc, VkDeviceMemory *mem)
{
    (void)device; (void)info; (void)alloc;
    *mem = (VkDeviceMemory)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_free(VkDevice device, VkDeviceMemory mem,
                                 const VkAllocationCallbacks *alloc)
{
    (void)device; (void)mem; (void)alloc;
    c.other_calls++;
}

/* The staging buffer the host publishes from: the same stand-in arena every time, so the fixture
 * can fill it with the bytes the device would have written there. */
static VkResult VKAPI_CALL gate_map(VkDevice device, VkDeviceMemory mem, VkDeviceSize offset,
                                    VkDeviceSize size, VkMemoryMapFlags flags, void **ppData)
{
    (void)device; (void)mem; (void)offset; (void)size; (void)flags;
    *ppData = g_stage;
    return VK_SUCCESS;
}

/* The coherence boundary the publication performs immediately before it memcpys: the device has
 * executed the batch, so this is where the fixture supplies what the device wrote -- and where
 * the HOST's read of that staging buffer belongs in the stream. The host read is recorded as an
 * access of the batch's own recording (that is what it is: the publication reads the bytes the
 * batch's commands wrote), which is why the publish barrier's position decides it. */
static VkResult VKAPI_CALL gate_invalidate(VkDevice device, uint32_t count,
                                           const VkMappedMemoryRange *ranges)
{
    RecAcc *a;
    unsigned rec = c.nsubs ? c.subs[c.nsubs - 1u].rec : 0u;

    (void)device; (void)count; (void)ranges;
    gate_supply();
    c.invalidates++;
    if (c.nsubs) {
        unsigned slot = c.subs[c.nsubs - 1u].slot;

        a = acc_new("the publication's memcpy out of the staging buffer", ACC_HOST_READ,
                    VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_READ_BIT,
                    slot == GATE_NOTGT ? g.slots[0].cmd : g.slots[slot].cmd);
        acc_buffer(a, g.rb_buf, 0, g.rb_cap ? g.rb_cap : (VkDeviceSize)1);
        if (a)
            a->rec = rec;
    }
    return VK_SUCCESS;
}

/* ── The entry points the driven paths never reach ───────────────────────────────────── */

/* Wired so that no call can land in a NULL pointer, and counted so that a future production
 * change which starts calling one of them is REPORTED (P4 checks the counter is 0) instead of
 * silently recording nothing. */
static void VKAPI_CALL gate_get_queue(VkDevice device, uint32_t family, uint32_t index,
                                      VkQueue *queue)
{
    (void)device; (void)family; (void)index;
    c.other_calls++;
    *queue = g.queue;
}

static VkResult VKAPI_CALL gate_create_pool(VkDevice device, const VkCommandPoolCreateInfo *info,
                                            const VkAllocationCallbacks *alloc,
                                            VkCommandPool *pool)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *pool = (VkCommandPool)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_alloc_cmds(VkDevice device, const VkCommandBufferAllocateInfo *info,
                                           VkCommandBuffer *cmds)
{
    (void)device; (void)info;
    c.other_calls++;
    *cmds = (VkCommandBuffer)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_fence(VkDevice device, const VkFenceCreateInfo *info,
                                             const VkAllocationCallbacks *alloc, VkFence *fence)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *fence = (VkFence)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_render_pass(VkDevice device,
                                                   const VkRenderPassCreateInfo *info,
                                                   const VkAllocationCallbacks *alloc,
                                                   VkRenderPass *rp)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *rp = (VkRenderPass)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_shader(VkDevice device,
                                              const VkShaderModuleCreateInfo *info,
                                              const VkAllocationCallbacks *alloc,
                                              VkShaderModule *mod)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *mod = (VkShaderModule)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_destroy_shader(VkDevice device, VkShaderModule mod,
                                           const VkAllocationCallbacks *alloc)
{
    (void)device; (void)mod; (void)alloc;
    c.other_calls++;
}

static VkResult VKAPI_CALL gate_create_pcache(VkDevice device,
                                              const VkPipelineCacheCreateInfo *info,
                                              const VkAllocationCallbacks *alloc,
                                              VkPipelineCache *pc)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *pc = (VkPipelineCache)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_layout(VkDevice device,
                                              const VkPipelineLayoutCreateInfo *info,
                                              const VkAllocationCallbacks *alloc,
                                              VkPipelineLayout *layout)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *layout = (VkPipelineLayout)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_dsl(VkDevice device,
                                           const VkDescriptorSetLayoutCreateInfo *info,
                                           const VkAllocationCallbacks *alloc,
                                           VkDescriptorSetLayout *dsl)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *dsl = (VkDescriptorSetLayout)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_dpool(VkDevice device,
                                             const VkDescriptorPoolCreateInfo *info,
                                             const VkAllocationCallbacks *alloc,
                                             VkDescriptorPool *pool)
{
    (void)device; (void)info; (void)alloc;
    c.other_calls++;
    *pool = (VkDescriptorPool)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_graphics(VkDevice device, VkPipelineCache pc,
                                                uint32_t count,
                                                const VkGraphicsPipelineCreateInfo *info,
                                                const VkAllocationCallbacks *alloc,
                                                VkPipeline *pipes)
{
    (void)device; (void)pc; (void)count; (void)info; (void)alloc;
    c.other_calls++;
    pipes[0] = (VkPipeline)hnew();
    return VK_SUCCESS;
}

static VkResult VKAPI_CALL gate_create_compute(VkDevice device, VkPipelineCache pc,
                                               uint32_t count,
                                               const VkComputePipelineCreateInfo *info,
                                               const VkAllocationCallbacks *alloc,
                                               VkPipeline *pipes)
{
    (void)device; (void)pc; (void)count; (void)info; (void)alloc;
    c.other_calls++;
    pipes[0] = (VkPipeline)hnew();
    return VK_SUCCESS;
}

static void VKAPI_CALL gate_bind_vertex_buffers(VkCommandBuffer cmd, uint32_t first,
                                                uint32_t count, const VkBuffer *bufs,
                                                const VkDeviceSize *offsets)
{
    (void)cmd; (void)first; (void)count; (void)bufs; (void)offsets;
    c.other_calls++;
}

static void VKAPI_CALL gate_bind_index_buffer(VkCommandBuffer cmd, VkBuffer buf,
                                              VkDeviceSize offset, VkIndexType type)
{
    (void)cmd; (void)buf; (void)offset; (void)type;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_viewport(VkCommandBuffer cmd, uint32_t first, uint32_t count,
                                         const VkViewport *vp)
{
    (void)cmd; (void)first; (void)count; (void)vp;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_scissor(VkCommandBuffer cmd, uint32_t first, uint32_t count,
                                        const VkRect2D *r)
{
    (void)cmd; (void)first; (void)count; (void)r;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_stencil_reference(VkCommandBuffer cmd, VkStencilFaceFlags face,
                                                  uint32_t value)
{
    (void)cmd; (void)face; (void)value;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_stencil_compare(VkCommandBuffer cmd, VkStencilFaceFlags face,
                                                uint32_t mask)
{
    (void)cmd; (void)face; (void)mask;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_stencil_write(VkCommandBuffer cmd, VkStencilFaceFlags face,
                                              uint32_t mask)
{
    (void)cmd; (void)face; (void)mask;
    c.other_calls++;
}

static void VKAPI_CALL gate_set_blend_constants(VkCommandBuffer cmd, const float bc[4])
{
    (void)cmd; (void)bc;
    c.other_calls++;
}

static void VKAPI_CALL gate_draw_indexed(VkCommandBuffer cmd, uint32_t count, uint32_t instances,
                                         uint32_t first, int32_t vertex_offset,
                                         uint32_t first_instance)
{
    (void)cmd; (void)count; (void)instances; (void)first; (void)vertex_offset; (void)first_instance;
    c.other_calls++;
}

/* Point the back end's own entry points at the stubs above. The pointers are static in the
 * translation unit, which is why the file is compiled here. The instance- and device-creation
 * entry points (vkCreateInstance, vkEnumeratePhysicalDevices, vkCreateDevice, vkGetDeviceProcAddr
 * and the rest of the loader's) are deliberately NOT wired: the fixture does not drive
 * nv2a_vk_backend_install, so nothing can reach them. */
static void gate_wire(void)
{
    vkBeginCommandBuffer = gate_begin;
    vkEndCommandBuffer = gate_end;
    vkResetCommandBuffer = gate_cmd_reset;
    vkQueueSubmit = gate_submit;
    vkWaitForFences = gate_wait;
    vkResetFences = gate_fence_reset;
    vkResetDescriptorPool = gate_pool_reset;
    vkCreateImage = gate_image_create;
    vkGetImageMemoryRequirements = gate_image_requirements;
    vkBindImageMemory = gate_image_bind;
    vkDestroyImage = gate_image_destroy;
    vkCreateImageView = gate_view_create;
    vkDestroyImageView = gate_view_destroy;
    vkCreateSampler = gate_sampler_create;
    vkCreateFramebuffer = gate_fb_create;
    vkDestroyFramebuffer = gate_fb_destroy;
    vkCmdBeginRenderPass = gate_begin_pass;
    vkCmdEndRenderPass = gate_end_pass;
    vkCmdCopyBufferToImage = gate_copy_to_image;
    vkCmdCopyImageToBuffer = gate_copy_to_buffer;
    vkCmdCopyBuffer = gate_copy_buffer;
    vkCmdCopyImage = gate_copy_image;
    vkCmdClearColorImage = gate_clear_color;
    vkCmdClearDepthStencilImage = gate_clear_depth;
    vkCmdPipelineBarrier = gate_barrier;
    vkCmdPushConstants = gate_push_constants;
    vkCmdBindPipeline = gate_bind_pipeline;
    vkCmdBindDescriptorSets = gate_bind_sets;
    vkCmdDispatch = gate_dispatch;
    vkCmdDraw = gate_draw;
    vkUpdateDescriptorSets = gate_ds_update;
    vkAllocateDescriptorSets = gate_ds_alloc;
    vkDestroyPipeline = gate_destroy_pipeline;
    vkCreateBuffer = gate_buffer_create;
    vkGetBufferMemoryRequirements = gate_buffer_requirements;
    vkBindBufferMemory = gate_buffer_bind;
    vkDestroyBuffer = gate_buffer_destroy;
    vkAllocateMemory = gate_allocate;
    vkFreeMemory = gate_free;
    vkMapMemory = gate_map;
    vkInvalidateMappedMemoryRanges = gate_invalidate;
    vkGetDeviceQueue = gate_get_queue;
    vkCreateCommandPool = gate_create_pool;
    vkAllocateCommandBuffers = gate_alloc_cmds;
    vkCreateFence = gate_create_fence;
    vkCreateRenderPass = gate_create_render_pass;
    vkCreateShaderModule = gate_create_shader;
    vkDestroyShaderModule = gate_destroy_shader;
    vkCreatePipelineCache = gate_create_pcache;
    vkCreatePipelineLayout = gate_create_layout;
    vkCreateDescriptorSetLayout = gate_create_dsl;
    vkCreateDescriptorPool = gate_create_dpool;
    vkCreateGraphicsPipelines = gate_create_graphics;
    vkCreateComputePipelines = gate_create_compute;
    vkCmdBindVertexBuffers = gate_bind_vertex_buffers;
    vkCmdBindIndexBuffer = gate_bind_index_buffer;
    vkCmdSetViewport = gate_set_viewport;
    vkCmdSetScissor = gate_set_scissor;
    vkCmdSetStencilReference = gate_set_stencil_reference;
    vkCmdSetStencilCompareMask = gate_set_stencil_compare;
    vkCmdSetStencilWriteMask = gate_set_stencil_write;
    vkCmdSetBlendConstants = gate_set_blend_constants;
    vkCmdDrawIndexed = gate_draw_indexed;
}

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
    printf("      %s: %s (%d check%s, %d failed)\n", bad ? "RED " : "GREEN", prop,
           g_checks - checks_before, (g_checks - checks_before) == 1 ? "" : "s", bad);
    fflush(stdout);
}

static void check(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;

    g_checks++;
    if (!cond)
        g_check_failures++;
    printf("      [%s] %s", cond ? "ok  " : "FAIL", what);
    if (fmt && *fmt) {
        printf(" -- ");
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
    printf("\n");
    fflush(stdout);
}

static void note(const char *fmt, ...)
{
    va_list ap;

    printf("      note: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
}

static void marker(const char *what, int present)
{
    printf("      %-66s %s\n", what, present ? "present" : "ABSENT");
    fflush(stdout);
}

/* ── The fixture's own bytes ─────────────────────────────────────────────────────────── */

static void fill_bytes(uint8_t *p, size_t n, uint32_t seed)
{
    uint32_t w = 0x9E3779B9u ^ seed;
    size_t i;

    for (i = 0; i < n; i++) {
        w = w * 1103515245u + 12345u;
        p[i] = (uint8_t)(w >> 24);
    }
}

/* A staging pattern whose bytes never equal the guest marker's: an unwritten staging buffer holds
 * one base (0x8x) and what the fixture's device wrote holds another, so every comparison in this
 * case distinguishes "the device wrote this" from "it never did". */
static void fill_stage_slice(uint8_t *p, size_t n, uint32_t base)
{
    size_t i;

    for (i = 0; i < n; i++)
        p[i] = (uint8_t)(base + (i & 0x0Fu));
}

/* One target's guest rectangle, so "which rectangle did this publish touch" is a byte count. */
static void fill_guest_rows(uint32_t va, uint32_t pitch, uint32_t row_bytes, uint32_t h,
                            uint32_t base)
{
    uint32_t y;

    for (y = 0; y < h; y++) {
        fill_stage_slice(g_row, row_bytes, base);
        memcpy(g_guest + va + (size_t)y * pitch, g_row, row_bytes);
    }
}

/* The bytes the device would have written for a submitted batch, supplied when that batch's fence
 * wait is behind it: every recorded staging write of the newest submission's recording, filled
 * with its own target's pattern. Supplying per batch matters because two batches' slices start at
 * their own group's offset 0 and so share bytes. */
static void gate_supply(void)
{
    unsigned i;

    if (!c.nsubs)
        return;
    for (i = 0; i < c.nacc; i++) {
        RecAcc *a = &c.accs[i];

        if (a->rec != c.subs[c.nsubs - 1u].rec || a->is_image || !a->is_write
            || a->buf != g.rb_buf || a->tgt >= 2u)
            continue;
        /* N4.4: the staging buffer is a range of a pool block, so the device's writes land at
         * the block's mapping plus the range's offset plus the slice's offset in the range. */
        fill_stage_slice(g_stage + g.rb_mem.off + a->off, (size_t)a->size, c.tgt_mark[a->tgt]);
        c.supply_writes++;
    }
    c.supply_calls++;
}

/* ── The world, as install/first use leaves it ──────────────────────────────────────── */

static void gate_world(void)
{
    unsigned i;

    memset(&g, 0, sizeof g);
    memset(&c, 0, sizeof c);
    c.begin_result = VK_SUCCESS;
    c.wait_result = VK_SUCCESS;
    c.samp_set = (VkDescriptorSet)hnew();

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
    g.depth_format = VK_FORMAT_D24_UNORM_S8_UINT;
    g.rp[0] = (VkRenderPass)hnew();
    g.rp[1] = (VkRenderPass)hnew();
    g.dsl_pack = (VkDescriptorSetLayout)hnew();
    g.layout_pack = (VkPipelineLayout)hnew();
    g.pipe_pack = (VkPipeline)hnew();
    g.dead = (VkCommandBuffer)hnew();
    for (i = 0; i < VB_SLOTS; i++) {
        g.slots[i].cmd = (VkCommandBuffer)hnew();
        g.slots[i].fence = (VkFence)hnew();
        g.slots[i].dpool = (VkDescriptorPool)hnew();
        g.slots[i].stream = (VkBuffer)hnew();
        g.slots[i].stream_ptr = g_stream[i];
        fill_bytes(g_stream[i], VB_SLOT_STREAM, GATE_SEED + i);
    }
    c.dpool = g.slots[0].dpool;
    /* The fixture's two targets are known by image handle only once the production has made
     * them; until then nothing is attributed to them. */
    c.tgt_img[0] = VK_NULL_HANDLE;
    c.tgt_img[1] = VK_NULL_HANDLE;
    c.tgt_bpp[0] = 4u;
    c.tgt_bpp[1] = 2u;
    c.tgt_mark[0] = GT_MARK;
    c.tgt_mark[1] = GT2_MARK;
    c.pack_target = GATE_NOTGT;
    fill_bytes(g_guest, GATE_GUEST, GATE_SEED + 7u);
    fill_stage_slice(g_stage, GATE_STAGE, 0x80u);   /* nothing has written the staging yet */
    fill_guest_rows(GT_VA, GT_PITCH, GT_W * 4u, GT_H, GT_MARK);
    fill_guest_rows(GT2_VA, GT2_PITCH, GT2_W * 2u, GT2_H, GT2_MARK);
    fill_guest_rows(GTEX_VA, GTEX_PITCH, GTEX_W * 4u, GTEX_H, GTEX_MARK);
}

/* ── The drive ──────────────────────────────────────────────────────────────────────── */

/* What the drive did, so a property can be positive about the world rather than about the
 * source text: which production calls ran, what they returned, and which images they made. */
static struct {
    int defaults_rc;
    int t4_created, t2_created, depth_ok;
    int flush_rc;
    VbTarget *t4, *t2;
    VbDepth *d;
    Nv2aTexture tex_real, tex_alias;
    VbTex txl;
    VkImage white_img, depth_img, txl_img, real_img, alias_img, copy_img;
    VkImageLayout real_layout, alias_layout, txl_layout, copy_layout;
    VkImageView alias_view, real_view, copy_view;
} dr;

/* A draw inside a pass the fixture opened through the production's own vb_begin_pass: this is
 * what records a pass's attachment read (at the begin) and its draw's attachment write. */
static void draw_in_pass(VbTarget *t, VbDepth *d)
{
    if (!vb_begin_pass(t, d)) {
        c.begin_failures++;
        return;
    }
    vkCmdDraw(vb_cmd(), 3, 1, 0, 0);
    vb_end_pass();
}

/* A draw that SAMPLES: the production's own descriptor shape (a VkWriteDescriptorSet of a
 * SAMPLED_IMAGE), then a draw inside a pass on `out`, so the fragment read is recorded exactly
 * where the back end would perform it. `out` is never the image being sampled (the production's
 * own rule: a target that is sampled while it is drawn into is sampled through its snapshot).
 * The fixture chose t_out so that the draw's own attachment accesses are on a DIFFERENT image,
 * which keeps the sampled read's pairs unambiguous. */
static void sample_in_draw(VkImageView view, VkImageLayout layout, VbTarget *out)
{
    VkWriteDescriptorSet wr;
    VkDescriptorImageInfo ii;

    memset(&ii, 0, sizeof ii);
    ii.imageView = view;
    ii.imageLayout = layout;
    memset(&wr, 0, sizeof wr);
    wr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wr.dstSet = c.samp_set;
    wr.dstBinding = 0;
    wr.descriptorCount = 1;
    wr.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    wr.pImageInfo = &ii;
    vkUpdateDescriptorSets(g.dev, 1, &wr, 0, NULL);
    draw_in_pass(out, NULL);
}

static void drive_world(void)
{
    VbTarget *ts[2];
    VkImageView view;
    VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;
    float iw = 0.0f, ih = 0.0f;

    section("the world the fixture drives; every property below decides this recording");

    /* 1. the install path's own work: g.white's two transitions and the copy between them (S6),
     *    and the install submission (R1). nv2a_vk_backend_install() sets g.ok = 1 after this
     *    returns 0, and so does the fixture. */
    dr.defaults_rc = vb_create_defaults();
    dr.white_img = g.white.img;
    g.ok = 1;
    note("vb_create_defaults() = %d, then g.ok = 1 as nv2a_vk_backend_install() does; the white "
         "texture is image %p", dr.defaults_rc, (void *)dr.white_img);

    /* 2. two targets through the production's own target path: the UNDEFINED->GENERAL
     *    transition, the clear, and S7's barrier. */
    dr.t4 = &g.targets[0];
    dr.t2 = &g.targets[1];
    dr.t4_created = vb_target_create(dr.t4, GT_VA, GT_PITCH, 4u, GT_W, GT_H);
    c.tgt_img[0] = dr.t4->im.img;
    dr.t4->dirty = 1;
    dr.t4->dx0 = 0;
    dr.t4->dy0 = 0;
    dr.t4->dx1 = GT_W;
    dr.t4->dy1 = GT_H;
    dr.t2_created = vb_target_create(dr.t2, GT2_VA, GT2_PITCH, 2u, GT2_W, GT2_H);
    c.tgt_img[1] = dr.t2->im.img;
    dr.t2->dirty = 1;
    dr.t2->dx0 = 0;
    dr.t2->dy0 = 0;
    dr.t2->dx1 = GT2_W;
    dr.t2->dy1 = GT2_H;
    note("vb_target_create(t4) = %d (image %p), vb_target_create(t2) = %d (image %p); both left "
         "stale, as the production leaves a fresh target, and dirty over the whole surface",
         dr.t4_created, (void *)dr.t4->im.img, dr.t2_created, (void *)dr.t2->im.img);

    /* 3. the loads: S1, with the write-after-write pair (the clear's write against the load's)
     *    that S7's destination scope has to keep covering. */
    vb_target_load(dr.t4);
    vb_target_load(dr.t2);
    note("vb_target_load(t4) and (t2): stale was 1 from vb_target_create, so both recorded their "
         "copyBufferToImage and the barrier after it");

    /* 4. the depth image (S8), then two passes on t4 that share it: the second pass's attachment
     *    read and write are the pairs S2 must order, and the depth access makes S2's depth scopes
     *    decidable. */
    dr.d = vb_depth_for(GD_VA, GT_W, GT_H);
    dr.depth_ok = dr.d != NULL;
    dr.depth_img = dr.d ? dr.d->im.img : VK_NULL_HANDLE;
    note("vb_depth_for() = %p (image %p): the transition, the clear and S8's barrier",
         (void *)dr.d, (void *)dr.depth_img);
    draw_in_pass(dr.t4, dr.d);

    /* 5. the pair C2 exists for: pass 1's colour attachment write, then the very next draw's
     *    fragment read of that image. vb_texture returns the target's own view for an aliasing
     *    texture, and the draw's pass is on the OTHER target, so the only barrier strictly
     *    between the write and the read is the S2 barrier pass 1 recorded when it ended. */
    memset(&dr.tex_alias, 0, sizeof dr.tex_alias);
    dr.tex_alias.offset = GT_VA;
    dr.tex_alias.pitch = GT_PITCH;
    dr.tex_alias.color = 0x12u;                 /* LIN_A8R8G8B8: what vb_tex_aliases wants */
    dr.tex_alias.width = GT_W;
    dr.tex_alias.height = GT_H;
    dr.alias_view = vb_texture(&dr.tex_alias, dr.t2, &layout, &iw, &ih);
    dr.alias_layout = layout;
    dr.alias_img = VK_NULL_HANDLE;
    {
        GateView *v = view_of(dr.alias_view);

        if (v)
            dr.alias_img = v->img;
    }
    sample_in_draw(dr.alias_view, layout, dr.t2);
    note("vb_texture(alias of t4, out = t2) = view %p at layout %d (the target's own view), then "
         "the draw that samples it", (void *)dr.alias_view, (int)layout);

    /* 6. a second pass on t4 with the same depth: its attachment read (the LOAD) and its draw's
     *    write are the later attachment accesses C1 decides. */
    draw_in_pass(dr.t4, dr.d);
    /* 7. the 16-bit target gets a pass of its own, so the pack kernel's sampled read of its
     *    image has an attachment write to be ordered against (C2 again, this time a COMPUTE read
     *    the production itself performs). */
    draw_in_pass(dr.t2, NULL);

    /* 8. a decoded texture, straight through vb_tex_upload: UNDEFINED->TRANSFER_DST_OPTIMAL, the
     *    copy, then TRANSFER_DST_OPTIMAL->SHADER_READ_ONLY_OPTIMAL (S6), and a draw that samples
     *    it. The layout the fixture samples it at is the layout the production's own texture path
     *    reports for an uploaded image (*layout = SHADER_READ_ONLY_OPTIMAL), which is exactly the
     *    claim a removed transition would make false. */
    memset(&dr.txl, 0, sizeof dr.txl);
    memset(&dr.tex_real, 0, sizeof dr.tex_real);
    dr.tex_real.offset = GTEX_VA;
    dr.tex_real.width = GTEX_W;
    dr.tex_real.height = GTEX_H;
    dr.tex_real.pitch = GTEX_PITCH;
    dr.tex_real.color = 0x12u;                  /* LIN_A8R8G8B8: the decoder path */
    dr.tex_real.addr_u = dr.tex_real.addr_v = 3u;
    dr.tex_real.filter = 0x01010000u;
    vb_tex_upload(&dr.txl, &dr.tex_real);
    dr.txl_img = dr.txl.im.img;
    dr.txl_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sample_in_draw(dr.txl.im.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, dr.t4);
    note("vb_tex_upload(txl) made image %p; the draw that follows samples it at "
         "SHADER_READ_ONLY_OPTIMAL, the layout its second transition leaves it in",
         (void *)dr.txl_img);

    /* 9. the same texture through the back end's own cache (vb_texture), and the install's 1x1
     *    white texture -- whose upload write is in the INSTALL recording, so its pair with this
     *    read is decided across recordings, by the install's submission and fence wait. */
    dr.real_view = vb_texture(&dr.tex_real, dr.t4, &layout, &iw, &ih);
    dr.real_layout = layout;
    dr.real_img = VK_NULL_HANDLE;
    if (dr.real_view) {
        GateView *v = view_of(dr.real_view);

        if (v)
            dr.real_img = v->img;
    }
    sample_in_draw(dr.real_view, layout, dr.t2);
    sample_in_draw(g.white.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, dr.t4);
    note("vb_texture(the decoded texture) = view %p at layout %d (its own cache entry and "
         "upload); the white texture's view %p is sampled too", (void *)dr.real_view,
         (int)layout, (void *)g.white.view);

    /* 10. the self-sampling snapshot: S9's transition, the copy and S5's barrier, then a second
     *     copy of the same pair -- whose write against the first copy's write, and the sampling of
     *     the snapshot, are the pairs S5 decides. */
    dr.copy_view = vb_texture(&dr.tex_alias, dr.t4, &layout, &iw, &ih);
    dr.copy_layout = layout;
    dr.copy_img = dr.copy_view ? dr.t4->copy.img : VK_NULL_HANDLE;
    (void)vb_texture(&dr.tex_alias, dr.t4, &layout, &iw, &ih);
    sample_in_draw(dr.copy_view, dr.copy_layout, dr.t4);
    note("vb_texture(alias of t4, out = t4) x2: snapshot image %p copied twice and then sampled",
         (void *)dr.copy_img);

    /* 11. the readback: one 4-byte target straight out of its image, one 16-bit target packed by
     *     the kernel (S3), the publish barrier (S4), the batch's submit and fence wait, and the
     *     host's read of the staging buffer. */
    ts[0] = dr.t4;
    ts[1] = dr.t2;
    dr.flush_rc = vb_targets_flush(ts, 2u);
    note("vb_targets_flush(t4, t2) = %d: one batch, one submission, one fence wait, one "
         "invalidation and one host read", dr.flush_rc);

    /* 12. the frame boundary, through the real caller: a pass recorded into a new recording,
     *     t4 dirty again, so vb_flip publishes it and submits that recording. */
    draw_in_pass(dr.t4, NULL);
    dr.t4->dirty = 1;
    vb_flip();
    note("vb_flip(): the flip published t4 in the recording that also holds the pass, "
         "submitted it, waited, marked both targets stale and advanced the frame to %u",
         (unsigned)g.frame);

    /* 13. and one more submitted recording, through vb_slot_submit()/vb_drain() directly. */
    draw_in_pass(dr.t2, NULL);
    vb_slot_submit();
    vb_drain();
    note("a final pass submitted through vb_slot_submit()/vb_drain()");
}

/* ── The decision ───────────────────────────────────────────────────────────────────── */

#define GATE_WRITE_BITS (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT \
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT \
    | VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT)

static int access_has_write(VkAccessFlags mask)
{
    return (mask & GATE_WRITE_BITS) != 0;
}

/* Does `mask` cover `access`? The two broad bits mean what the specification says they mean:
 * VK_ACCESS_MEMORY_WRITE_BIT is "every write" and covers any write access, and
 * VK_ACCESS_MEMORY_READ_BIT is "every read" and covers any read access. Every other bit covers
 * itself (and the bits it is an OR of, which is what a scoped mask naming several accesses
 * does). An access that both reads and writes (a depth-stencil attachment's read+write, say)
 * needs the WRITE side covered: covering its read side alone does not order its write. */
static int access_covers(VkAccessFlags mask, VkAccessFlags access)
{
    if (!access)
        return 1;
    if (mask & access)
        return 1;
    if (access_has_write(access))
        return (mask & VK_ACCESS_MEMORY_WRITE_BIT) != 0;
    return (mask & VK_ACCESS_MEMORY_READ_BIT) != 0;
}

static int stage_covers(VkPipelineStageFlags mask, VkPipelineStageFlags stage)
{
    if (!stage)
        return 1;
    if (mask & stage)
        return 1;
    /* VK_PIPELINE_STAGE_ALL_COMMANDS_BIT specifies "all operations performed by all commands
     * supported on the queue", so it covers every command stage -- but VK_PIPELINE_STAGE_HOST_BIT
     * "is not invoked by any commands recorded in a command buffer", so a barrier whose
     * destination stage is ALL_COMMANDS does NOT cover the host's own read of device memory.
     * That is precisely what the publication's own barrier into VK_PIPELINE_STAGE_HOST_BIT is
     * for, and it is why this case does not let the retained per-submission barrier stand in for
     * it. */
    if ((mask & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) && !(stage & VK_PIPELINE_STAGE_HOST_BIT))
        return 1;
    return 0;
}

static int same_resource(const RecAcc *a, const RecAcc *b)
{
    if (a->is_image != b->is_image)
        return 0;
    if (a->is_image)
        return a->img == b->img;
    if (a->buf != b->buf)
        return 0;
    return a->off < b->off + b->size && b->off < a->off + a->size;
}

/* The barrier STRICTLY between the two accesses that orders the later one after the earlier one,
 * or NULL. A barrier recorded after both does not count, and an image barrier is consulted only
 * for accesses to the image it names (a memory barrier is global). */
static const RecBar *order_barrier(const RecAcc *a, const RecAcc *b)
{
    unsigned i;

    for (i = 0; i < c.nbars; i++) {
        const RecBar *r = &c.bars_rec[i];

        if (r->rec != a->rec || r->seq <= a->seq || r->seq >= b->seq)
            continue;
        if (r->is_image && !(a->is_image && r->img == a->img))
            continue;
        if (!stage_covers(r->src, a->stage))
            continue;
        if (!access_covers(r->srca, a->access))
            continue;
        if (!stage_covers(r->dst, b->stage))
            continue;
        if (!access_covers(r->dsta, b->access))
            continue;
        return r;
    }
    return NULL;
}

/* The submission-and-fence-wait boundary that orders the later recording after the earlier one,
 * or NULL. Deliberately stricter than Vulkan (see the file header). */
static const RecSub *order_submit(const RecAcc *a, const RecAcc *b)
{
    unsigned i;

    for (i = 0; i < c.nsubs; i++) {
        const RecSub *s = &c.subs[i];

        if (s->rec != a->rec || s->cb != a->cb)
            continue;
        if (!s->waited_seq || s->waited_seq >= b->seq)
            continue;
        return s;
    }
    return NULL;
}

/* Which kinds an attachment access is. An attachment WRITE is what C1 and C2 start from; an
 * attachment "read" is the pass's LOAD or the draw's own read/write of the depth image. */
static int acc_is_attach(const RecAcc *a)
{
    return a->kind == ACC_ATTACH_READ || a->kind == ACC_ATTACH_RW || a->kind == ACC_DEPTH_READ
        || a->kind == ACC_DEPTH_RW;
}
static int acc_is_attach_write(const RecAcc *a)
{
    return a->kind == ACC_ATTACH_RW || a->kind == ACC_DEPTH_RW;
}
static int acc_is_image_transfer_write(const RecAcc *a)
{
    return a->is_image && (a->kind == ACC_LOAD_COPY || a->kind == ACC_UPLOAD_COPY
                           || a->kind == ACC_CLEAR_COLOR || a->kind == ACC_CLEAR_DEPTH
                           || a->kind == ACC_SNAPSHOT_DST);
}
static int acc_is_later_image_access(const RecAcc *b)
{
    return b->is_image && (acc_is_attach(b) || b->kind == ACC_SAMPLE_READ
                           || b->kind == ACC_SNAPSHOT_SRC || b->kind == ACC_READBACK_SRC
                           || acc_is_image_transfer_write(b) || b->kind == ACC_PACK_IMAGE);
}
static int acc_is_device_write(const RecAcc *a)
{
    return !a->is_image && (a->kind == ACC_PACK_WRITE || a->kind == ACC_READBACK_DST
                            || a->kind == ACC_BUFFER_COPY_DST);
}

#define GATE_NCLS 5

static int class_pair(int cls, const RecAcc *a, const RecAcc *b)
{
    switch (cls) {
    case 0:     /* C1: an attachment write, then a later attachment read/write */
        return acc_is_attach_write(a) && acc_is_attach(b);
    case 1:     /* C2: an attachment write, then a later shader read of that image */
        return acc_is_attach_write(a) && b->kind == ACC_SAMPLE_READ;
    case 2:     /* C3: an image transfer write, then any later access of that image */
        return acc_is_image_transfer_write(a) && acc_is_later_image_access(b);
    case 3:     /* C4: the pack kernel's write, then the buffer copy's read of those bytes */
        return a->kind == ACC_PACK_WRITE && b->kind == ACC_PACK_READ;
    case 4:     /* C5: a device write of the staging buffer, then the host's read */
        return acc_is_device_write(a) && b->kind == ACC_HOST_READ;
    default:
        return 0;
    }
}

static const char *cls_tag[GATE_NCLS] = { "C1", "C2", "C3", "C4", "C5" };
static const char *cls_title[GATE_NCLS] = {
    "an attachment write, then a later attachment read/write of that image (S2)",
    "an attachment write, then a later shader read of that image (S2)",
    "an image transfer write, then a later access of that image (S1/S5/S6/S7/S8)",
    "the pack kernel's write, then the buffer copy's transfer read (S3)",
    "a device write of the staging buffer, then the host's read of those bytes (S4)"
};
static unsigned cls_pairs[GATE_NCLS], cls_unordered[GATE_NCLS];

static void describe_acc(const RecAcc *a, char *out, size_t n)
{
    if (!a) {
        snprintf(out, n, "(no such access)");
        return;
    }
    if (a->is_image)
        snprintf(out, n, "#%u %s on image %p (%s layout %d, recorded in %u, pass %u)", a->seq,
                 kind_name(a->kind), (void *)a->img, a->tracked_known ? "tracked" : "unknown",
                 (int)a->layout, a->rec, a->pass);
    else
        snprintf(out, n, "#%u %s on buffer %p [%llu, %llu) (recorded in %u)", a->seq,
                 kind_name(a->kind), (void *)a->buf, (unsigned long long)a->off,
                 (unsigned long long)(a->off + a->size), a->rec);
}

static void describe_bar(const RecBar *b, char *out, size_t n)
{
    if (!b) {
        snprintf(out, n, "(no such barrier)");
        return;
    }
    snprintf(out, n, "srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X dstAccess 0x%08X%s%s",
             (unsigned)b->src, (unsigned)b->srca, (unsigned)b->dst, (unsigned)b->dsta,
             b->is_image ? ", an image barrier on " : ", a memory barrier",
             b->is_image ? "" : "(global)");
}

/* Decide every pair of the class, print the evidence, and return the pair count. */
static unsigned decide_class(int cls)
{
    char da[220], db[220], dbar[220];
    unsigned i, j;

    printf("\n      -- %s: %s --\n", cls_tag[cls], cls_title[cls]);
    cls_pairs[cls] = 0u;
    cls_unordered[cls] = 0u;
    for (i = 0; i < c.nacc; i++) {
        for (j = 0; j < c.nacc; j++) {
            const RecAcc *a = &c.accs[i], *b = &c.accs[j];

            if (a->seq >= b->seq || !same_resource(a, b) || !class_pair(cls, a, b))
                continue;
            cls_pairs[cls]++;
            describe_acc(a, da, sizeof da);
            describe_acc(b, db, sizeof db);
            if (a->rec == b->rec) {
                const RecBar *r = order_barrier(a, b);

                if (r) {
                    describe_bar(r, dbar, sizeof dbar);
                    printf("      ordered   %s\n                before %s\n                by "
                           "the barrier at #%u (%s)\n", da, db, r->seq, dbar);
                } else {
                    cls_unordered[cls]++;
                    printf("      UNORDERED %s\n                before %s\n                in "
                           "recording %u: no barrier strictly between them covers both\n", da, db,
                           a->rec);
                }
            } else {
                const RecSub *s = order_submit(a, b);

                if (s)
                    printf("      ordered   %s\n                before %s\n                by "
                           "the submission at #%u of recording %u and its fence wait at #%u\n",
                           da, db, s->seq, s->rec, s->waited_seq);
                else {
                    cls_unordered[cls]++;
                    printf("      UNORDERED %s\n                before %s\n                in "
                           "recording %u/%u: the earlier recording was not submitted and waited "
                           "for before the later command was recorded\n", da, db, a->rec, b->rec);
                }
            }
        }
    }
    printf("      %s: %u pair(s) decided, %u unordered\n", cls_tag[cls], cls_pairs[cls],
           cls_unordered[cls]);
    fflush(stdout);
    return cls_pairs[cls];
}

/* ── The site helpers ───────────────────────────────────────────────────────────────── */

static const RecAcc *nth_kind(GateKind k, unsigned nth)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nacc; i++)
        if (c.accs[i].kind == k)
            if (n++ == nth)
                return &c.accs[i];
    return NULL;
}

static unsigned count_kind(GateKind k)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nacc; i++)
        if (c.accs[i].kind == k)
            n++;
    return n;
}

/* The last access recorded inside the pass whose number is `pass` (the fixture numbers the passes
 * in the order the production began them). For the S2 site that is the pass's last attachment
 * access, i.e. the draw's writes. */
static const RecAcc *last_acc_of_pass(unsigned pass)
{
    const RecAcc *last = NULL;
    unsigned i;

    for (i = 0; i < c.nacc; i++)
        if (c.accs[i].pass == pass)
            last = &c.accs[i];
    return last;
}

/* Did the pass record an access of this kind on this image? */
static int pass_has(unsigned pass, GateKind kind, VkImage img)
{
    unsigned i;

    for (i = 0; i < c.nacc; i++)
        if (c.accs[i].pass == pass && c.accs[i].kind == kind && c.accs[i].img == img)
            return 1;
    return 0;
}

/* The first barrier recorded after an access, and how many accesses lie strictly between the two.
 * A barrier that is the site's own has nothing between it and the command it orders. */
static const RecBar *bar_after(const RecAcc *w, unsigned *between)
{
    const RecBar *b = NULL;
    unsigned i, n = 0;

    if (!w)
        return NULL;
    for (i = 0; i < c.nbars; i++)
        if (c.bars_rec[i].seq > w->seq) {
            b = &c.bars_rec[i];
            break;
        }
    if (!b) {
        if (between)
            *between = 0u;
        return NULL;
    }
    for (i = 0; i < c.nacc; i++)
        if (c.accs[i].seq > w->seq && c.accs[i].seq < b->seq)
            n++;
    if (between)
        *between = n;
    return b;
}

/* An image barrier of the given shape and image. */
static const RecBar *img_bar_of(VkImage img, VkImageLayout from, VkImageLayout to, unsigned nth)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nbars; i++) {
        const RecBar *b = &c.bars_rec[i];

        if (!b->is_image || b->img != img || b->old_layout != from || b->new_layout != to)
            continue;
        if (n++ == nth)
            return b;
    }
    return NULL;
}

static unsigned count_img_bar_of(VkImage img, VkImageLayout from, VkImageLayout to)
{
    unsigned i, n = 0;

    for (i = 0; i < c.nbars; i++) {
        const RecBar *b = &c.bars_rec[i];

        if (b->is_image && b->img == img && b->old_layout == from && b->new_layout == to)
            n++;
    }
    return n;
}

/* Does the site's barrier have exactly the table's four scopes, and was it recorded outside any
 * render pass? */
static int bar_is(const RecBar *b, VkPipelineStageFlags src, VkAccessFlags srca,
                  VkPipelineStageFlags dst, VkAccessFlags dsta)
{
    return b && b->src == src && b->srca == srca && b->dst == dst && b->dsta == dsta
        && !b->pass_open;
}

static int img_bar_is(const RecBar *b, VkImage img, VkImageLayout from, VkImageLayout to,
                      VkPipelineStageFlags src, VkAccessFlags srca, VkPipelineStageFlags dst,
                      VkAccessFlags dsta)
{
    return b && b->is_image && b->img == img && b->old_layout == from && b->new_layout == to
        && b->src == src && b->srca == srca && b->dst == dst && b->dsta == dsta
        && !b->pass_open;
}

/* ── 0. the source, the seam, and the fixture's own constants ───────────────────────── */

static void prop0(void)
{
    VbTarget probe;
    uint32_t stride;
    VkDeviceSize slice;
    int chk = g_checks, bad = g_check_failures;

    section("0. the source under test, the seam it is compiled through, and the fixture's "
            "own constants");
    printf("      production bodies compiled from: %s\n", VB_SYNC_SCOPE_GATE_SOURCE);
    printf("      sha256 %s\n", VB_SYNC_SCOPE_GATE_SOURCE_SHA256);
    printf("      %u bytes\n", (unsigned)VB_SYNC_SCOPE_GATE_SOURCE_BYTES);
    note("every function driven below is that file's own, compiled into this translation unit by "
         "#include; scripts/vk-sync-scope-gate.py rejects the BUILD when a definition, a struct "
         "field, a global or a scope macro this fixture binds to is gone or has a different "
         "value, and the markers below say what that source's own text passes at each site");
    marker("S1 vb_target_load's load barrier is scoped", VB_SYNC_SCOPE_GATE_S1_LOAD_SCOPED);
    marker("S2 vb_end_pass's barrier is scoped", VB_SYNC_SCOPE_GATE_S2_PASS_SCOPED);
    marker("S3 vb_targets_flush's pack barrier is scoped", VB_SYNC_SCOPE_GATE_S3_PACK_SCOPED);
    marker("S4 vb_targets_flush's publish barrier is scoped", VB_SYNC_SCOPE_GATE_S4_PUBLISH_SCOPED);
    marker("S5 vb_texture's snapshot barrier is scoped", VB_SYNC_SCOPE_GATE_S5_SNAPSHOT_SCOPED);
    marker("S6a vb_tex_upload's first transition is scoped",
           VB_SYNC_SCOPE_GATE_S6A_TEXTURE_UNDEF_SCOPED);
    marker("S6b vb_tex_upload's second transition is scoped",
           VB_SYNC_SCOPE_GATE_S6B_TEXTURE_SAMPLE_SCOPED);
    marker("S6a vb_create_defaults' first transition is scoped",
           VB_SYNC_SCOPE_GATE_S6A_WHITE_UNDEF_SCOPED);
    marker("S6b vb_create_defaults' second transition is scoped",
           VB_SYNC_SCOPE_GATE_S6B_WHITE_SAMPLE_SCOPED);
    marker("S7 vb_target_create's clear barrier is scoped", VB_SYNC_SCOPE_GATE_S7_CLEAR_SCOPED);
    marker("S7b vb_target_create's transition is scoped",
           VB_SYNC_SCOPE_GATE_S7B_TARGET_UNDEF_SCOPED);
    marker("S8 vb_depth_for's clear barrier is scoped",
           VB_SYNC_SCOPE_GATE_S8_DEPTH_CLEAR_SCOPED);
    marker("S8b vb_depth_for's transition is scoped",
           VB_SYNC_SCOPE_GATE_S8B_DEPTH_UNDEF_SCOPED);
    marker("S9 vb_texture's snapshot transition is scoped",
           VB_SYNC_SCOPE_GATE_S9_SNAPSHOT_UNDEF_SCOPED);
    marker("exactly two broad vb_barrier() sites are left",
           VB_SYNC_SCOPE_GATE_RETAINED_TWO_BROAD);
    marker("the flush's own barrier is not the broad form",
           VB_SYNC_SCOPE_GATE_PUBLISH_NOT_BROAD);
    marker("the abandonment branch is still the broad form",
           VB_SYNC_SCOPE_GATE_ABANDON_BROAD);

    /* The six macros, against the packet's table written out as Vulkan bits. The probe checks the
     * same table against the source's text; this is the numeric check, and it is what every
     * recorded scope below is compared with. */
    check(VB_STAGE_IMAGE_DST == GATE_EXP_STAGE_IMAGE_DST,
          "VB_STAGE_IMAGE_DST is exactly the table's stage set",
          "0x%08X vs table 0x%08X", (unsigned)VB_STAGE_IMAGE_DST,
          (unsigned)GATE_EXP_STAGE_IMAGE_DST);
    check(VB_ACCESS_IMAGE_DST == GATE_EXP_ACCESS_IMAGE_DST,
          "VB_ACCESS_IMAGE_DST is exactly the table's access set",
          "0x%08X vs table 0x%08X", (unsigned)VB_ACCESS_IMAGE_DST,
          (unsigned)GATE_EXP_ACCESS_IMAGE_DST);
    check(VB_STAGE_ATTACH == GATE_EXP_STAGE_ATTACH,
          "VB_STAGE_ATTACH is exactly the table's stage set",
          "0x%08X vs table 0x%08X", (unsigned)VB_STAGE_ATTACH,
          (unsigned)GATE_EXP_STAGE_ATTACH);
    check(VB_ACCESS_ATTACH_WRITE == GATE_EXP_ACCESS_ATTACH_WRITE,
          "VB_ACCESS_ATTACH_WRITE is exactly the table's access set",
          "0x%08X vs table 0x%08X", (unsigned)VB_ACCESS_ATTACH_WRITE,
          (unsigned)GATE_EXP_ACCESS_ATTACH_WRITE);
    check(VB_STAGE_DEPTH == GATE_EXP_STAGE_DEPTH,
          "VB_STAGE_DEPTH is exactly the table's stage set",
          "0x%08X vs table 0x%08X", (unsigned)VB_STAGE_DEPTH, (unsigned)GATE_EXP_STAGE_DEPTH);
    check(VB_ACCESS_DEPTH == GATE_EXP_ACCESS_DEPTH,
          "VB_ACCESS_DEPTH is exactly the table's access set",
          "0x%08X vs table 0x%08X", (unsigned)VB_ACCESS_DEPTH, (unsigned)GATE_EXP_ACCESS_DEPTH);

    /* The narrowing's own shape: exactly two broad sites, one of them the abandonment branch --
     * and the flush's publish barrier is NOT one of them. What the abandonment barrier must order
     * is decided by jsrf_vk_readback_ordering_gate, not here. */
    check(VB_SYNC_SCOPE_GATE_RETAINED_TWO_BROAD && VB_SYNC_SCOPE_GATE_ABANDON_BROAD,
          "the source keeps exactly two broad vb_barrier() sites: the per-submission one in "
          "vb_slot_submit and the abandonment one in vb_targets_flush (reported here; the "
          "ordering gate decides what the second must order)",
          "two broad sites %d, abandonment branch still broad %d",
          VB_SYNC_SCOPE_GATE_RETAINED_TWO_BROAD, VB_SYNC_SCOPE_GATE_ABANDON_BROAD);
    check(VB_SYNC_SCOPE_GATE_PUBLISH_NOT_BROAD,
          "the flush's publish barrier is the scoped one, not ALL_COMMANDS",
          "marker %d", VB_SYNC_SCOPE_GATE_PUBLISH_NOT_BROAD);

    /* The fixture's geometry against the source's own bounds. */
    check(VB_READBACK_MAX == GATE_CAP && VB_MAX_TARGETS >= 2u && VB_MAX_DEPTHS >= 1u
          && VB_MAX_TEXTURES >= 1u && VB_SLOTS == 3u && VB_MAX_DIM >= GT_W
          && (VkDeviceSize)GT_W * GT_H * 4u <= VB_SLOT_STREAM / 2u,
          "the fixture's two targets, one depth image, one texture and one batch fit the "
          "source's own bounds",
          "cap %llu, targets %u, depths %u, textures %u, slots %u, dim %u, stream %u",
          (unsigned long long)VB_READBACK_MAX, (unsigned)VB_MAX_TARGETS, (unsigned)VB_MAX_DEPTHS,
          (unsigned)VB_MAX_TEXTURES, (unsigned)VB_SLOTS, (unsigned)VB_MAX_DIM,
          (unsigned)VB_SLOT_STREAM);
    check(GATE_READBACK <= GATE_CAP && GT_SLICE <= GATE_CAP && GT2_SLICE <= GATE_CAP,
          "both slices fit the publication cap, so the two targets are one batch and one group",
          "t4 %llu + t2 %llu = %llu, cap %llu", (unsigned long long)GT_SLICE,
          (unsigned long long)GT2_SLICE, (unsigned long long)GATE_READBACK,
          (unsigned long long)GATE_CAP);
    check((size_t)(GTEX_VA + GTEX_SPAN) <= GATE_GUEST
          && (size_t)(GT2_VA + (uint32_t)GT2_SLICE) <= GATE_GUEST,
          "the guest arena covers both targets' rectangles and the texture's range",
          "t2 ends 0x%X, texture ends 0x%X, arena %llu", (unsigned)(GT2_VA + (uint32_t)GT2_SLICE),
          (unsigned)(GTEX_VA + GTEX_SPAN), (unsigned long long)GATE_GUEST);
    check(GTEX_VA >= GT2_VA + (uint32_t)GT2_SLICE
          && !(GTEX_VA < GT_VA + (uint32_t)GT_SLICE && GT_VA < GTEX_VA + GTEX_SPAN),
          "the texture's guest range does not overlap a target's, so vb_texture's "
          "overlapping-guest-texture flush cannot join this stream",
          "t4 [%u, %u), t2 [%u, %u), texture [%u, %u)", (unsigned)GT_VA,
          (unsigned)(GT_VA + (uint32_t)GT_SLICE), (unsigned)GT2_VA,
          (unsigned)(GT2_VA + (uint32_t)GT2_SLICE), (unsigned)GTEX_VA,
          (unsigned)(GTEX_VA + GTEX_SPAN));

    /* The fixture's slices and rows are the production's own answers, and each target's rows are
     * exactly its slice, which is what makes the published-byte comparison below exact. */
    memset(&probe, 0, sizeof probe);
    probe.va = GT_VA;
    probe.pitch = GT_PITCH;
    probe.bpp = 4u;
    probe.w = GT_W;
    probe.h = GT_H;
    probe.dx0 = 0u;
    probe.dy0 = 0u;
    probe.dx1 = GT_W;
    probe.dy1 = GT_H;
    stride = 0u;
    slice = vb_readback_slice(&probe, &stride);
    check(slice == GT_SLICE && stride == GT_PITCH && (VkDeviceSize)GT_W * 4u == GT_PITCH,
          "the 4-byte target's slice and row stride are the production's own answers, and its "
          "rows are exactly its slice",
          "vb_readback_slice %llu/%u, fixture %llu/%u", (unsigned long long)slice, stride,
          (unsigned long long)GT_SLICE, GT_PITCH);
    memset(&probe, 0, sizeof probe);
    probe.va = GT2_VA;
    probe.pitch = GT2_PITCH;
    probe.bpp = 2u;
    probe.w = GT2_W;
    probe.h = GT2_H;
    probe.dx1 = GT2_W;
    probe.dy1 = GT2_H;
    stride = 0u;
    slice = vb_readback_slice(&probe, &stride);
    check(slice == GT2_SLICE && stride == GT2_PITCH && (VkDeviceSize)GT2_W * 2u == GT2_PITCH,
          "the 16-bit target's packed slice and row stride are the production's own answers, and "
          "its rows are exactly its slice",
          "vb_readback_slice %llu/%u, fixture %llu/%u", (unsigned long long)slice, stride,
          (unsigned long long)GT2_SLICE, GT2_PITCH);

    /* The fixture's bytes: the pattern the device is supplied with never equals the bytes the
     * guest held before, or every comparison below would pass for free. */
    check(GT_MARK != GT2_MARK && GT_MARK != 0x90u && GT2_MARK != 0x90u,
          "the two targets' markers and the unwritten staging pattern are three different bases",
          "t4 0x%02X, t2 0x%02X, unwritten staging 0x90", GT_MARK, GT2_MARK);

    verdict("0 the source, the seam and the fixture's constants", chk, bad);
}

/* ── 1. the sites, as the recording has them ─────────────────────────────────────────── */

static void prop1(void)
{
    char what[240];
    const RecAcc *w;
    const RecBar *b;
    unsigned between, i;
    int chk = g_checks, bad = g_check_failures;

    section("1. every site the packet narrowed, as the recorded command stream has it");

    /* The world first: a property about sites has to be positive that the fixture drove them. */
    check(dr.defaults_rc == 0,
          "vb_create_defaults() built the white texture, recorded its two transitions and "
          "submitted", "rc %d", dr.defaults_rc);
    check(dr.t4_created == 1 && dr.t2_created == 1 && dr.depth_ok,
          "the two targets and the depth image were made through the production's own paths",
          "t4 %d, t2 %d, depth %d", dr.t4_created, dr.t2_created, dr.depth_ok);
    check(count_kind(ACC_LOAD_COPY) == 2u && count_kind(ACC_CLEAR_COLOR) == 2u
          && count_kind(ACC_CLEAR_DEPTH) == 1u && count_kind(ACC_SNAPSHOT_DST) == 2u
          && count_kind(ACC_PACK_WRITE) == 1u && count_kind(ACC_READBACK_DST) == 2u
          && count_kind(ACC_BUFFER_COPY_DST) == 1u,
          "the sites were reached the number of times the world asks for",
          "loads %u, colour clears %u, depth clears %u, snapshot copies %u, pack dispatches %u, "
          "staging writes %u", count_kind(ACC_LOAD_COPY), count_kind(ACC_CLEAR_COLOR),
          count_kind(ACC_CLEAR_DEPTH), count_kind(ACC_SNAPSHOT_DST), count_kind(ACC_PACK_WRITE),
          count_kind(ACC_READBACK_DST));
    check(c.begin_failures == 0u && c.pass_failed == 0u,
          "every vb_begin_pass() the fixture drove opened its pass", "failures %u",
          c.begin_failures);

    /* ── S1: vb_target_load's barrier after the load's copyBufferToImage ── */
    w = nth_kind(ACC_LOAD_COPY, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S1: the buffer barrier recorded after the load's copy into the "
                                "first target's image has the table's scopes");
    check(VB_SYNC_SCOPE_GATE_S1_LOAD_SCOPED && bar_is(b, GATE_STAGE_TRANSFER,
          VK_ACCESS_TRANSFER_WRITE_BIT, GATE_EXP_STAGE_IMAGE_DST, GATE_EXP_ACCESS_IMAGE_DST),
          what, "marker %d; the barrier is srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X "
          "dstAccess 0x%08X, outside a pass %d", VB_SYNC_SCOPE_GATE_S1_LOAD_SCOPED,
          b ? (unsigned)b->src : 0u, b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u,
          b ? (unsigned)b->dsta : 0u, b ? !b->pass_open : 0);
    check(w && w->is_image && w->img == dr.t4->im.img && between == 0u,
          "S1: that barrier is the very next command recorded after the copy into the target's "
          "own image, so it is this site's",
          "%u access(es) between; producer image %p (t4's image %p)", between,
          w && w->is_image ? (void *)w->img : NULL, (void *)dr.t4->im.img);

    /* ── S2: vb_end_pass's barrier after the pass's attachment writes ── */
    w = last_acc_of_pass(1u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S2: the barrier recorded after the first pass's attachment "
                                "writes has the table's scopes");
    check(VB_SYNC_SCOPE_GATE_S2_PASS_SCOPED && bar_is(b, GATE_EXP_STAGE_ATTACH,
          GATE_EXP_ACCESS_ATTACH_WRITE, GATE_EXP_STAGE_IMAGE_DST, GATE_EXP_ACCESS_IMAGE_DST),
          what, "marker %d; the barrier is srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X "
          "dstAccess 0x%08X, outside a pass %d", VB_SYNC_SCOPE_GATE_S2_PASS_SCOPED,
          b ? (unsigned)b->src : 0u, b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u,
          b ? (unsigned)b->dsta : 0u, b ? !b->pass_open : 0);
    check(w && acc_is_attach_write(w) && between == 0u,
          "S2: it is this site's barrier -- recorded immediately after the pass's own last "
          "attachment access, with nothing in between",
          "the pass's last access is %s on image %p, access 0x%08X; %u access(es) between",
          w ? kind_name(w->kind) : "(none)", w ? (void *)w->img : NULL,
          w ? (unsigned)w->access : 0u, between);
    check(pass_has(1u, ACC_ATTACH_RW, dr.t4->im.img)
          && pass_has(1u, ACC_DEPTH_RW, dr.depth_img),
          "S2: that pass recorded the target's colour attachment write AND its depth-stencil "
          "attachment read+write, so the barrier's destination scope has to cover both sides",
          "colour write %d, depth read+write %d",
          pass_has(1u, ACC_ATTACH_RW, dr.t4->im.img), pass_has(1u, ACC_DEPTH_RW, dr.depth_img));
    note("the pass ran with a depth attachment, so the same barrier also has to cover the "
         "depth-stencil attachment accesses: %u of the pass's accesses are depth-stencil ones",
         (unsigned)(count_kind(ACC_DEPTH_RW) + count_kind(ACC_DEPTH_READ)));

    /* ── S3: the pack path's barrier between the dispatch and the buffer copy ── */
    w = nth_kind(ACC_PACK_WRITE, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S3: the barrier recorded between the pack dispatch and the "
                                "buffer copy that reads its output has the table's scopes");
    check(VB_SYNC_SCOPE_GATE_S3_PACK_SCOPED && bar_is(b, GATE_STAGE_COMPUTE,
          VK_ACCESS_SHADER_WRITE_BIT, GATE_STAGE_TRANSFER, VK_ACCESS_TRANSFER_READ_BIT), what,
          "marker %d; the barrier is srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X "
          "dstAccess 0x%08X", VB_SYNC_SCOPE_GATE_S3_PACK_SCOPED, b ? (unsigned)b->src : 0u,
          b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u, b ? (unsigned)b->dsta : 0u);
    check(w && !w->is_image && w->buf == g.pack_buf && between == 0u && w->size == GT2_SLICE,
          "S3: it is this site's barrier -- recorded immediately after the kernel's write of this "
          "target's slice of the device-local pack buffer, with nothing in between",
          "producer %s of [%llu, %llu) on pack buffer %p, %u between",
          w ? kind_name(w->kind) : "(none)", w ? (unsigned long long)w->off : 0ull,
          w ? (unsigned long long)(w->off + w->size) : 0ull, (void *)g.pack_buf, between);

    /* ── S4: the publish barrier ── */
    w = nth_kind(ACC_BUFFER_COPY_DST, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S4: the publication's barrier -- the batch's last staging write "
                                "against the host's read -- has the table's scopes");
    check(VB_SYNC_SCOPE_GATE_S4_PUBLISH_SCOPED
          && bar_is(b, GATE_STAGE_COMPUTE | GATE_STAGE_TRANSFER,
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, GATE_STAGE_HOST,
                    VK_ACCESS_HOST_READ_BIT), what,
          "marker %d; the barrier is srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X "
          "dstAccess 0x%08X", VB_SYNC_SCOPE_GATE_S4_PUBLISH_SCOPED, b ? (unsigned)b->src : 0u,
          b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u, b ? (unsigned)b->dsta : 0u);
    check(w && between == 0u && w->buf == g.rb_buf,
          "S4: it is this site's barrier -- recorded immediately after the batch's last write of "
          "the staging buffer, with nothing in between",
          "producer [%llu, %llu) on staging buffer %p, %u between",
          w ? (unsigned long long)w->off : 0ull, w ? (unsigned long long)(w->off + w->size) : 0ull,
          (void *)g.rb_buf, between);

    /* ── S5: the snapshot copy's barrier ── */
    w = nth_kind(ACC_SNAPSHOT_DST, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S5: the barrier recorded after the self-sampling snapshot copy "
                                "has the table's scopes");
    check(VB_SYNC_SCOPE_GATE_S5_SNAPSHOT_SCOPED && bar_is(b, GATE_STAGE_TRANSFER,
          VK_ACCESS_TRANSFER_WRITE_BIT, GATE_STAGE_FRAG_COMP | GATE_STAGE_TRANSFER,
          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT),
          what, "marker %d; the barrier is srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X "
          "dstAccess 0x%08X", VB_SYNC_SCOPE_GATE_S5_SNAPSHOT_SCOPED, b ? (unsigned)b->src : 0u,
          b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u, b ? (unsigned)b->dsta : 0u);
    check(w && w->is_image && w->img == dr.copy_img && between == 0u,
          "S5: it is this site's barrier -- recorded immediately after the copy's write of the "
          "snapshot image, with nothing in between",
          "producer image %p (the snapshot is %p), %u between",
          w && w->is_image ? (void *)w->img : NULL, (void *)dr.copy_img, between);

    /* ── S6: the two transitions, on both callers ── */
    {
        VkImage imgs[3];
        unsigned n = 0;

        imgs[n++] = dr.white_img;
        imgs[n++] = dr.txl_img;
        imgs[n++] = dr.real_img;
        for (i = 0; i < n; i++) {
            const RecBar *a = img_bar_of(imgs[i], VK_IMAGE_LAYOUT_UNDEFINED,
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0u);
            const RecBar *a2 = img_bar_of(imgs[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0u);
            snprintf(what, sizeof what, "S6: image %p got exactly the table's two transitions "
                     "(UNDEFINED->TRANSFER_DST_OPTIMAL, then ->SHADER_READ_ONLY_OPTIMAL)", (void *)imgs[i]);
            check(VB_SYNC_SCOPE_GATE_S6A_TEXTURE_UNDEF_SCOPED
                  && VB_SYNC_SCOPE_GATE_S6B_TEXTURE_SAMPLE_SCOPED
                  && (i == 0u ? (VB_SYNC_SCOPE_GATE_S6A_WHITE_UNDEF_SCOPED
                                 && VB_SYNC_SCOPE_GATE_S6B_WHITE_SAMPLE_SCOPED) : 1)
                  && img_bar_is(a, imgs[i], VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, GATE_STAGE_TOP, 0u,
                                GATE_STAGE_TRANSFER, VK_ACCESS_TRANSFER_WRITE_BIT)
                  && img_bar_is(a2, imgs[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, GATE_STAGE_TRANSFER,
                                VK_ACCESS_TRANSFER_WRITE_BIT, GATE_STAGE_FRAG_COMP,
                                VK_ACCESS_SHADER_READ_BIT), what,
                  "first %p (0x%08X -> 0x%08X), second %p (0x%08X -> 0x%08X)",
                  (void *)a, a ? (unsigned)a->src : 0u, a ? (unsigned)a->dsta : 0u, (void *)a2,
                  a2 ? (unsigned)a2->src : 0u, a2 ? (unsigned)a2->dsta : 0u);
        }
        check(count_img_bar_of(dr.txl_img, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) == 1u
              && count_img_bar_of(dr.txl_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) == 1u,
              "S6: vb_tex_upload records each of the two transitions exactly once per image",
              "%u first, %u second",
              count_img_bar_of(dr.txl_img, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL),
              count_img_bar_of(dr.txl_img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    /* ── S7 (the clear's barrier) and S7b (the create's transition) ── */
    w = nth_kind(ACC_CLEAR_COLOR, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S7: the barrier recorded after vb_target_create's clear has the "
                                "table's scopes");
    check(VB_SYNC_SCOPE_GATE_S7_CLEAR_SCOPED && bar_is(b, GATE_STAGE_TRANSFER,
          VK_ACCESS_TRANSFER_WRITE_BIT, GATE_EXP_STAGE_IMAGE_DST, GATE_EXP_ACCESS_IMAGE_DST),
          what, "marker %d; srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X dstAccess 0x%08X",
          VB_SYNC_SCOPE_GATE_S7_CLEAR_SCOPED, b ? (unsigned)b->src : 0u,
          b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u, b ? (unsigned)b->dsta : 0u);
    check(w && w->img == dr.t4->im.img && between == 0u,
          "S7: it is this site's barrier -- immediately after the clear of the target's own "
          "image, with nothing in between",
          "cleared image %p, %u access(es) between", w ? (void *)w->img : NULL, between);
    b = img_bar_of(dr.t4->im.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0u);
    check(VB_SYNC_SCOPE_GATE_S7B_TARGET_UNDEF_SCOPED
          && img_bar_is(b, dr.t4->im.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                        GATE_STAGE_TOP, 0u, GATE_STAGE_TRANSFER, VK_ACCESS_TRANSFER_WRITE_BIT)
          && img_bar_of(dr.t2->im.img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0u)
             != NULL,
          "S7b: each target's create records the table's UNDEFINED->GENERAL transition",
          "marker %d; transition %p", VB_SYNC_SCOPE_GATE_S7B_TARGET_UNDEF_SCOPED, (void *)b);

    /* ── S8 (the depth clear) and S8b (the depth image's transition) ── */
    w = nth_kind(ACC_CLEAR_DEPTH, 0u);
    b = bar_after(w, &between);
    snprintf(what, sizeof what, "S8: the barrier recorded after the depth image's clear has the "
                                "table's scopes");
    check(VB_SYNC_SCOPE_GATE_S8_DEPTH_CLEAR_SCOPED && bar_is(b, GATE_STAGE_TRANSFER,
          VK_ACCESS_TRANSFER_WRITE_BIT, GATE_EXP_STAGE_DEPTH, GATE_EXP_ACCESS_DEPTH), what,
          "marker %d; srcStage 0x%08X srcAccess 0x%08X -> dstStage 0x%08X dstAccess 0x%08X",
          VB_SYNC_SCOPE_GATE_S8_DEPTH_CLEAR_SCOPED, b ? (unsigned)b->src : 0u,
          b ? (unsigned)b->srca : 0u, b ? (unsigned)b->dst : 0u, b ? (unsigned)b->dsta : 0u);
    check(w && w->img == dr.depth_img && between == 0u,
          "S8: it is this site's barrier -- immediately after the clear of the depth image, with "
          "nothing in between",
          "cleared image %p (the depth image is %p), %u between", w ? (void *)w->img : NULL,
          (void *)dr.depth_img, between);
    b = img_bar_of(dr.depth_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0u);
    check(VB_SYNC_SCOPE_GATE_S8B_DEPTH_UNDEF_SCOPED
          && img_bar_is(b, dr.depth_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                        GATE_STAGE_TOP, 0u, GATE_STAGE_TRANSFER, VK_ACCESS_TRANSFER_WRITE_BIT),
          "S8b: the depth image's create records the table's UNDEFINED->GENERAL transition",
          "marker %d; transition %p", VB_SYNC_SCOPE_GATE_S8B_DEPTH_UNDEF_SCOPED, (void *)b);

    /* ── S9: the snapshot image's transition ── */
    b = img_bar_of(dr.copy_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0u);
    check(VB_SYNC_SCOPE_GATE_S9_SNAPSHOT_UNDEF_SCOPED
          && img_bar_is(b, dr.copy_img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                        GATE_STAGE_TOP, 0u, GATE_STAGE_TRANSFER, VK_ACCESS_TRANSFER_WRITE_BIT)
          && count_img_bar_of(dr.copy_img, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL) == 1u,
          "S9: the snapshot image is transitioned UNDEFINED->GENERAL exactly once, with the "
          "table's scopes",
          "marker %d; transition %p", VB_SYNC_SCOPE_GATE_S9_SNAPSHOT_UNDEF_SCOPED, (void *)b);

    /* ── R1 and R2 ── */
    {
        unsigned in_every = 1u;

        for (i = 0; i < c.nsubs; i++) {
            unsigned k, found = 0u;

            for (k = 0; k < c.nbars; k++)
                if (!c.bars_rec[k].is_image && c.bars_rec[k].rec == c.subs[i].rec
                    && c.bars_rec[k].seq < c.subs[i].seq
                    && c.bars_rec[k].src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                    && c.bars_rec[k].srca == VK_ACCESS_MEMORY_WRITE_BIT
                    && c.bars_rec[k].dsta == (VK_ACCESS_MEMORY_READ_BIT
                                              | VK_ACCESS_MEMORY_WRITE_BIT))
                    found = 1u;
            if (!found)
                in_every = 0u;
        }
        check(c.nsubs > 0u && in_every,
              "R1: every submitted recording carries the retained broad vb_barrier() before its "
              "submit, with the table's masks",
              "%u submission(s), every one with the broad barrier %u", c.nsubs, in_every);
        check(c.broad_bars == c.nsubs,
              "R2: no other broad barrier was recorded, so the abandonment branch was never "
              "reached (nothing failed here) -- its own semantics are the ordering gate's",
              "broad barriers %u, submissions %u", c.broad_bars, c.nsubs);
    }

    verdict("1 the sites, as the recording has them", chk, bad);
}

/* ── 2. the hazard classes ──────────────────────────────────────────────────────────── */

static void prop2(void)
{
    char what[240];
    int chk = g_checks, bad = g_check_failures;
    int cls;

    section("2. every pair of accesses the packet's scopes have to order, decided from the "
            "recording");
    for (cls = 0; cls < GATE_NCLS; cls++) {
        decide_class(cls);
        snprintf(what, sizeof what, "%s decided at least one pair, so the class is not vacuous",
                 cls_tag[cls]);
        check(cls_pairs[cls] > 0u, what, "%u pair(s)", cls_pairs[cls]);
        snprintf(what, sizeof what, "%s leaves no pair unordered", cls_tag[cls]);
        check(cls_unordered[cls] == 0u, what, "%u unordered of %u pair(s)", cls_unordered[cls],
              cls_pairs[cls]);
    }

    /* C6, the layout class: not a pair of accesses but a rule about each one. Its counts are
     * reported the same way -- decided (checked) and unordered (failed) -- so it cannot pass
     * vacuously either. */
    printf("\n      -- C6: layout legality, decided per access, transition and descriptor --\n");
    printf("      image accesses whose layout argument was compared with the tracked layout: %u\n",
           c.lay_checked);
    printf("      ... of which name a layout the image is not in: %u\n", c.lay_bad);
    printf("      accesses whose role rule was checked against the tracked layout: %u\n",
           c.role_checked);
    printf("      ... of which are illegal in that layout: %u\n", c.role_bad);
    printf("      image transitions whose oldLayout was compared with the tracked layout: %u\n",
           c.trans_checked);
    printf("      ... of which name a layout the image was not in: %u\n", c.trans_bad);
    printf("      SAMPLED_IMAGE descriptors whose layout was compared with the tracked one: %u\n",
           c.desc_checked);
    printf("      ... of which are wrong or name an illegal layout: %u\n", c.desc_bad);
    fflush(stdout);
    check(c.trans_checked > 0u && c.trans_bad == 0u,
          "C6: every image transition moved the image out of the layout it was really in",
          "%u decided, %u unordered", c.trans_checked, c.trans_bad);
    check(c.lay_checked > 0u && c.lay_bad == 0u,
          "C6: every image access named the layout that image was in",
          "%u decided, %u unordered", c.lay_checked, c.lay_bad);
    check(c.role_checked > 0u && c.role_bad == 0u,
          "C6: every access was legal in the layout its image was in (a copy destination, a "
          "colour attachment and a depth-stencil attachment alike)",
          "%u decided, %u unordered", c.role_checked, c.role_bad);
    check(c.desc_checked > 0u && c.desc_bad == 0u,
          "C6: every SAMPLED_IMAGE descriptor named the tracked layout, and a layout a sampled "
          "image may be in",
          "%u decided, %u unordered", c.desc_checked, c.desc_bad);
    check(c.img_not_undef == 0u,
          "C6: the production created every image UNDEFINED, which is the layout every one of "
          "those transitions leaves", "images created in another layout: %u", c.img_not_undef);

    verdict("2 the hazard classes", chk, bad);
}

/* ── 3. the narrowing, in effect ────────────────────────────────────────────────────── */

static void prop3(void)
{
    char what[240];
    unsigned i, host = 0u, host_bad = 0u, mem_wide = 0u, img_wide = 0u;
    int chk = g_checks, bad = g_check_failures;

    section("3. the narrowing itself: how many barriers are still broad, and where they are");

    for (i = 0; i < c.nbars; i++) {
        const RecBar *b = &c.bars_rec[i];

        if (b->is_image) {
            if (b->src == VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
                || b->srca == VK_ACCESS_MEMORY_WRITE_BIT
                || (b->dsta & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)))
                img_wide++;
            continue;
        }
        if (b->dst & VK_PIPELINE_STAGE_HOST_BIT) {
            host++;
            if (b->src != (GATE_STAGE_COMPUTE | GATE_STAGE_TRANSFER)
                || b->srca != (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)
                || b->dsta != VK_ACCESS_HOST_READ_BIT)
                host_bad++;
        }
        if ((b->srca & VK_ACCESS_MEMORY_WRITE_BIT) != 0
            && b->src != VK_PIPELINE_STAGE_ALL_COMMANDS_BIT)
            mem_wide++;
    }

    check(c.mem_bars_in_pass == 0u,
          "P3: every barrier was recorded outside any open render pass",
          "%u of %u were recorded inside one", c.mem_bars_in_pass, c.mem_bars + c.img_bars);
    check(c.broad_bars == c.nsubs && c.nsubs > 0u,
          "P3: exactly one ALL_COMMANDS-source barrier per submission -- the retained "
          "per-submission barrier, and nothing else broad",
          "%u broad barrier(s) for %u submission(s)", c.broad_bars, c.nsubs);
    check(mem_wide == 0u,
          "P3: no memory barrier names MEMORY_WRITE without being the retained broad form, so "
          "every narrowed site passes its own explicit access mask",
          "%u such barrier(s)", mem_wide);
    check(img_wide == 0u,
          "P3: no layout transition carries the broad scopes -- every one names its own source "
          "and destination",
          "%u broad image barrier(s) of %u", img_wide, c.img_bars);
    check(host == 2u && host_bad == 0u,
          "P3: each publication batch's barrier is the scoped one into VK_PIPELINE_STAGE_HOST_BIT, "
          "naming both device writers and the host's read",
          "%u barrier(s) into HOST, %u with the wrong scopes", host, host_bad);

    verdict("3 the narrowing in effect", chk, bad);
}

/* ── 4. no failure path ─────────────────────────────────────────────────────────────── */

/* The last staging write recorded for a target: the flip's batch re-supplies the first target, so
 * the comparison below is against the newest bytes the fixture supplied for it. */
static const RecAcc *last_stage_write(unsigned tgt)
{
    const RecAcc *w = NULL;
    unsigned i;

    for (i = 0; i < c.nacc; i++)
        if (!c.accs[i].is_image && c.accs[i].is_write && c.accs[i].buf == g.rb_buf
            && c.accs[i].tgt == tgt)
            w = &c.accs[i];
    return w;
}

/* How many bytes of that target's guest rectangle differ from the pattern the device was
 * supplied with. Zero is "the device's bytes reached guest memory exactly"; non-zero is the
 * publication having lost or ordered away bytes. */
static size_t guest_diff_base(const RecAcc *w, uint32_t base)
{
    unsigned tgt = w->tgt;
    VbTarget *t;
    size_t i, n, bad = 0;

    if (tgt >= 2u)
        return (size_t)-1;
    t = tgt == 0u ? dr.t4 : dr.t2;
    n = (size_t)w->size;
    for (i = 0; i < n; i++) {
        uint8_t want = (uint8_t)(base + (i & 0x0Fu));

        /* Both targets' rows are exactly their slice (P0 checks it), so the guest rectangle and
         * the staging slice are the same linear sequence of bytes. */
        if (g_guest[t->va + i] != want)
            bad++;
    }
    return bad;
}

static void prop4(void)
{
    char what[240];
    const RecAcc *wa, *wb;
    size_t da, db;
    int chk = g_checks, bad = g_check_failures;

    section("4. nothing failed: no injected outcome, no stop, and the published bytes are the "
            "device's own");

    check(g.failed == 0 && g.ok == 1 && g.fails == 0,
          "the back end never latched a failure and never reported one: g.ok 1, g.failed 0 and "
          "no vb_fail() call at all",
          "g.ok %d, g.failed %d, g.fails %u", g.ok, g.failed, g.fails);
    check(g.st.readback_incomplete == 0u,
          "no target was left unpublished by a boundary", "readback_incomplete %llu",
          (unsigned long long)g.st.readback_incomplete);
    check(dr.flush_rc == 1,
          "vb_targets_flush() returned 1: every dirty target was published, no batch was "
          "abandoned", "rc %d", dr.flush_rc);
    check(g.frame == 1u && dr.t4->stale && dr.t2->stale,
          "vb_flip() completed its boundary: the frame advanced and both targets were marked "
          "stale for the next load",
          "frame %u, t4 stale %d, t2 stale %d", (unsigned)g.frame, dr.t4->stale, dr.t2->stale);

    check(g.st.readbacks == 3u && g.st.readback_batches == 2u && g.st.readback_packs == 1u
          && g.st.readback_copies == 2u,
          "the publication's counters are the ones this world asks for: two targets in the "
          "batch, one 16-bit target packed, one 4-byte target copied in each batch",
          "readbacks %llu, batches %llu, packs %llu, copies %llu",
          (unsigned long long)g.st.readbacks, (unsigned long long)g.st.readback_batches,
          (unsigned long long)g.st.readback_packs, (unsigned long long)g.st.readback_copies);
    check(g.st.readback_bytes == GT_SLICE + GT2_SLICE + GT_SLICE,
          "the bytes published into guest memory are exactly the three rectangles",
          "%llu vs %llu", (unsigned long long)g.st.readback_bytes,
          (unsigned long long)(GT_SLICE + GT2_SLICE + GT_SLICE));
    check(c.submits == 4u && c.waits == 4u && c.invalidates == 2u
          && count_kind(ACC_HOST_READ) == 2u,
          "four submissions (install, the world's batch, the flip's batch, the explicit one), "
          "each waited for as it was made, and two host reads of the staging buffer",
          "submits %u, waits %u, invalidations %u, host reads %u", c.submits, c.waits,
          c.invalidates, count_kind(ACC_HOST_READ));
    check(c.supply_calls == 2u && c.supply_writes == 3u,
          "the fixture supplied the device's bytes once per batch: three staging writes over the "
          "two of them (t4 and t2, then t4 again)",
          "supply calls %u, writes %u", c.supply_calls, c.supply_writes);
    check(c.dead_records == 0u && c.other_calls == 0u,
          "nothing was recorded into the dead command buffer, and no entry point the driven "
          "paths do not reach was called",
          "dead records %u, other calls %u", c.dead_records, c.other_calls);
    check(g.rb_cap == (VkDeviceSize)(1u << 20) && g.rb_mem.mem != VK_NULL_HANDLE
          && g.rb_ptr == g_stage + g.rb_mem.off && g.rb_mem.off % VB_POOL_ALIGN == 0,
          "the staging pair was made once, by the production's own reserve, as an aligned range "
          "of a pool block mapped onto the fixture's arena",
          "capacity %llu, rb_ptr %p (arena %p + %llu)", (unsigned long long)g.rb_cap,
          (void *)g.rb_ptr, (void *)g_stage, (unsigned long long)g.rb_mem.off);

    /* The bytes: what guest memory holds is the pattern the device was supplied with, for every
     * target, and NOT the pattern of the other target or of the pre-publication guest bytes --
     * which is what makes the comparison discriminating rather than trivially true. */
    wa = last_stage_write(0u);
    wb = last_stage_write(1u);
    check(wa && wb, "both targets have a recorded staging write to compare guest memory with",
          "t4 write %p, t2 write %p", (void *)wa, (void *)wb);
    if (wa && wb) {
        da = guest_diff_base(wa, GT_MARK);
        db = guest_diff_base(wb, GT2_MARK);
        snprintf(what, sizeof what, "the first target's guest rectangle is byte for byte the "
                                    "bytes the device wrote for it");
        check(da == 0u, what, "%llu differing byte(s) of %llu",
              (unsigned long long)da, (unsigned long long)wa->size);
        snprintf(what, sizeof what, "the 16-bit target's guest rectangle is byte for byte the "
                                    "bytes the device wrote for it");
        check(db == 0u, what, "%llu differing byte(s) of %llu",
              (unsigned long long)db, (unsigned long long)wb->size);
        check(guest_diff_base(wa, GT2_MARK) > 0u && guest_diff_base(wb, GT_MARK) > 0u,
              "... and those comparisons discriminate: neither rectangle matches the OTHER "
              "target's pattern",
              "t4 vs t2's pattern: %llu differing, t2 vs t4's: %llu differing",
              (unsigned long long)guest_diff_base(wa, GT2_MARK),
              (unsigned long long)guest_diff_base(wb, GT_MARK));
    }

    verdict("4 no failure path", chk, bad);
}

/* ── 5. the binds N4.4's allocator handed out ────────────────────────────────────────────
 *
 * N4.4 moved every resource's memory into a pool, so the offsets a resource binds at are no
 * longer 0. What this property decides is the rule that replaced that expectation: every
 * offset is a multiple of the allocator's own VB_POOL_ALIGN and of the requirement's
 * alignment, the range fits inside a live block, the block's kind matches the resource's kind
 * (an image never sits in a linear pool and a buffer never in a non-linear one), and no two
 * live ranges of one block overlap. The old "offset 0" assertion would have to be deleted to
 * compile; this is what it decided, re-derived for the pooled allocator. */
static void prop5(void)
{
    unsigned i, shared = 0;
    int chk = g_checks, bad = g_check_failures;

    section("5. the memory every bind was given: aligned, inside its block, never overlapping");
    check(c.binds_img > 0u && c.binds_buf > 0u,
          "the driven world really bound images and buffers", "%u image(s), %u buffer(s)",
          c.binds_img, c.binds_buf);
    check(c.bind_violations == 0u,
          "every bind satisfies the allocator's own rules (alignment, kind, block, no overlap)",
          "%u violation(s): %s", c.bind_violations, c.bind_violation);
    for (i = 0; i < c.binds_img && i < GATE_MAX_BIND; i++) {
        unsigned j;

        for (j = 0; j < c.binds_buf && j < GATE_MAX_BIND; j++)
            if (c.bind_img_mem[i] == c.bind_buf_mem[j])
                shared = 1;
    }
    check(shared == 0u,
          "no image and no buffer ever share one block (the linear/non-linear separation)",
          "");
    note("the driven world bound %u image range(s) and %u buffer range(s); the first image "
         "offsets are %llu, %llu, %llu (VB_POOL_ALIGN %u)", c.binds_img, c.binds_buf,
         (unsigned long long)c.bind_img_off[0], (unsigned long long)c.bind_img_off[1],
         (unsigned long long)c.bind_img_off[2], (unsigned)VB_POOL_ALIGN);
    verdict("5 the binds' offsets and blocks", chk, bad);
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

/* The executor's decoder: this fixture has no guest texture data to decode, so it fills the
 * entry the back end allocated with a recognisable pattern and reports success -- the decoded
 * path is what vb_tex_upload takes when the format is not a DXT one. */
int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    unsigned i, n;

    if (!tex || !argb_out)
        return 0;
    n = tex->width * tex->height;
    for (i = 0; i < n; i++)
        argb_out[i] = 0xFF000000u | (uint32_t)(i & 0xFFFFu);
    return n ? 1 : 0;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    return tex ? tex->pitch * tex->height : 0u;
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

    printf("vk_sync_scope_gate: the scopes N4.3 narrowed the Vulkan back end's barriers to, "
           "decided from the\nrecorded command stream of the toolkit's own "
           "nv2a_vk_backend.c\n");
    printf("no GPU, no ICD and no skip path: every Vulkan entry point the driven paths reach is a "
           "controlled stub that\nrecords the command stream, and the staging buffer vkMapMemory "
           "hands back is the fixture's own bytes\n");

    g_guest = (uint8_t *)malloc(GATE_GUEST);
    g_stage = (uint8_t *)malloc(GATE_STAGE);
    g_row = (uint8_t *)malloc(GATE_ROW);
    for (i = 0; i < VB_SLOTS; i++)
        g_stream[i] = (uint8_t *)malloc(VB_SLOT_STREAM);
    if (!g_guest || !g_stage || !g_row || !g_stream[0] || !g_stream[1] || !g_stream[2]) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: the fixture could not be allocated\n");
        return 1;
    }

    gate_wire();
    gate_world();

    prop0();
    drive_world();
    prop1();
    prop2();
    prop3();
    prop4();
    prop5();

    for (i = 0; i < VB_SLOTS; i++)
        free(g_stream[i]);
    free(g_row);
    free(g_stage);
    free(g_guest);

    printf("\n");
    printf("vk_sync_scope_gate: %u pair(s) decided over the hazard classes, %u left unordered; "
           "%u image transition(s), %u layout-checked access(es) and %u sampled descriptor(s) "
           "checked; %u broad barrier(s) (one per submission) of %u memory and %u image "
           "barriers\n", cls_pairs[0] + cls_pairs[1] + cls_pairs[2] + cls_pairs[3] + cls_pairs[4],
           cls_unordered[0] + cls_unordered[1] + cls_unordered[2] + cls_unordered[3]
           + cls_unordered[4], c.trans_checked, c.lay_checked, c.desc_checked, c.broad_bars,
           c.mem_bars, c.img_bars);
    if (g_prop_failures) {
        fprintf(stderr, "FAIL: vk_sync_scope_gate: %d of %d properties failed (%d of %d checks "
                        "failed)\n", g_prop_failures, g_props, g_check_failures, g_checks);
        return 1;
    }
    printf("PASS: vk_sync_scope_gate: %d properties, %d checks -- every site the packet narrowed "
           "records a barrier whose scopes cover both the producer and the consumer of the "
           "resource it names, every pair of accesses those scopes must order is decided from the "
           "recording, and the only barriers left broad are the two the packet kept\n", g_props,
           g_checks);
    return 0;
}
