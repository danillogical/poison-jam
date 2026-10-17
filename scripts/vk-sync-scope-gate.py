"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_sync_scope_gate.c binds to.

The sync-scope gate compiles the toolkit's own src/video/nv2a_vk_backend.c into its translation
unit, so the bodies it drives -- vb_create_defaults, vb_target_create, vb_target_load,
vb_begin_pass/vb_end_pass, vb_depth_for, vb_tex_upload, vb_texture, vb_targets_flush, vb_flip,
vb_slot_submit/vb_drain and the barrier helpers they call -- are the production ones, verbatim.
This script is the other half of that: it reads the same file AT BUILD TIME and refuses the build
when what the test needs is not there.

  * Every REQUIRED anchor is a definition, a struct field, a global, a #define or a claim the
    test's C code binds to. One that is gone is a hard error (exit 2), which fails the custom
    command and the build. It never skips and never warns-and-continues: a moved or renamed seam
    must break the build, not silently turn the case into something else.

  * The SIX N4.3 SCOPE MACROS are anchored as VALUES rather than as bare names: their bodies are
    read out of this source and compared, term by term, with the table the packet's specification
    fixes (VB_STAGE_IMAGE_DST = COLOR_ATTACHMENT_OUTPUT | EARLY_FRAGMENT_TESTS |
    LATE_FRAGMENT_TESTS | FRAGMENT_SHADER | COMPUTE_SHADER | TRANSFER and so on). A macro whose
    value differs from that table is a build error, which is what makes "the macros are read from
    the production source, not re-typed blindly" true of the test as well as of this probe: the
    test's own C compares them numerically a second time, against the same table written out as
    Vulkan bit names.

  * A SITE's scopes are MARKERS, not anchors. The gate is built to be pointed at mutant sources
    that move, narrow or replace a site's barrier -- including the form that replaces a scoped
    call with the retained broad vb_barrier() -- and every one of those must still COMPILE, so
    that the decision it loses is reached from the RECORDED COMMAND STREAM rather than from a
    failed grep. Each marker below says whether the source TEXT at one site still passes the
    scopes of the table; the test prints it, fails a check naming it when it is 0, and decides
    the site from what the stubs recorded.

  * The same distinction applies to the two retained broad sites: the marker only counts that
    there are exactly two `vb_barrier();` calls left, which is the narrowing claim this packet
    makes. WHICH of them is the abandonment barrier and what it must order is decided by the
    separate readback ordering gate (jsrf_vk_readback_ordering_gate), never here.

Outputs (both declared to CMake as one custom command):
  --header    vk_sync_scope_gate_seam.h    the markers + the source's identity, #included by the
                                           test
  --manifest  vk_sync_scope_gate_seam.txt  the same verdict in prose, kept next to the build
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

# Every literal below is normalised for whitespace before matching, so reflowing a declaration's
# parameters does not break the probe -- but its name, its parameter list, its field names and the
# values the test computes with must still be there.
REQUIRED = [
    ("the Vulkan branch of the file", "#if !defined(XBR_VK_BACKEND)"),
    ("the staging cap", "#define VB_READBACK_MAX (96u << 20)"),
    ("the target bound", "#define VB_MAX_TARGETS 16"),
    ("the depth bound", "#define VB_MAX_DEPTHS 8"),
    ("the texture bound", "#define VB_MAX_TEXTURES 512"),
    ("the identity bound", "#define VB_MAX_DIM 4096"),
    ("the ring size", "#define VB_SLOTS 3"),
    ("one slot's stream size", "#define VB_SLOT_STREAM (16u << 20)"),
    ("one slot's descriptor-set budget", "#define VB_SLOT_SETS 2048"),
    ("the stream slice alignment", "#define VB_STREAM_ALIGN 256u"),
    ("the image handle struct", "} VbImage;"),
    ("the image's VkImage", "VkImage img;"),
    ("the image's VkImageView", "VkImageView view;"),
    ("the target struct", "} VbTarget;"),
    ("the target's in-use flag", "int used;"),
    ("the target's guest surface fields", "uint32_t va, pitch, bpp, w, h;"),
    ("the target's image and view", "VbImage im, copy;"),
    ("the target's stale flag", "int stale;"),
    ("the target's dirty flag", "int dirty;"),
    ("the target's dirty rectangle", "uint32_t dx0, dy0, dx1, dy1;"),
    ("the depth struct", "} VbDepth;"),
    ("the depth's guest address", "uint32_t va, w, h;"),
    ("the depth's image", "VbImage im;"),
    ("the texture entry struct", "} VbTex;"),
    ("the texture entry's key fields", "uint32_t offset, color, w, h, pitch, span;"),
    ("the slot struct", "} VbSlot;"),
    ("the slot's command buffer field", "VkCommandBuffer cmd;"),
    ("the slot's fence field", "VkFence fence;"),
    ("the slot's pending bit", "int pending;"),
    ("the slot's recording bit", "int recording;"),
    ("the slot's descriptor pool field", "VkDescriptorPool dpool;"),
    ("the slot's used-set count", "uint32_t sets_used;"),
    ("the slot's stream buffer field", "VkBuffer stream;"),
    ("the slot's mapped stream pointer", "uint8_t *stream_ptr;"),
    ("the slot's stream offset field", "VkDeviceSize stream_off;"),
    ("the held-object struct", "} VbGarbage;"),
    ("the back end's state object", "} g;"),
    ("the device handle", "VkDevice dev;"),
    ("the queue handle", "VkQueue queue;"),
    ("the memory properties", "VkPhysicalDeviceMemoryProperties mp;"),
    ("the depth format", "VkFormat depth_format;"),
    ("the dead command buffer", "VkCommandBuffer dead;"),
    ("the slot ring", "VbSlot slots[VB_SLOTS];"),
    ("the current slot", "unsigned cur;"),
    ("the pending-slot count", "unsigned npending;"),
    ("the render passes", "VkRenderPass rp[2];"),
    ("the pack set layout field", "VkDescriptorSetLayout dsl_pack;"),
    ("the pack push layout field", "VkPipelineLayout layout_pack;"),
    ("the pack pipeline field", "VkPipeline pipe_pack;"),
    ("the device-local pack buffer field", "VkBuffer pack_buf;"),
    ("the host-visible staging buffer field", "VkBuffer rb_buf;"),
    ("the staging buffer's memory field", "VbAlloc rb_mem;"),
    ("the staging pair's mapped pointer", "uint8_t *rb_ptr;"),
    ("the staging pair's capacity", "VkDeviceSize rb_cap;"),
    ("the 1x1 white texture", "VbImage white;"),
    ("the open-pass flag", "int pass_open;"),
    ("the open pass's command buffer", "VkCommandBuffer pass_cmd;"),
    ("the open pass's target", "VbTarget *pass_t;"),
    ("the open pass's depth", "VbDepth *pass_d;"),
    ("the target array", "VbTarget targets[VB_MAX_TARGETS];"),
    ("the depth array", "VbDepth depths[VB_MAX_DEPTHS];"),
    ("the texture array", "VbTex texs[VB_MAX_TEXTURES];"),
    ("the frame counter", "uint32_t frame;"),
    ("the failure counters", "unsigned fails, validation_said;"),
    ("the statistics object", "VbStats st;"),
    ("the load counter", "uint64_t rt_loads, readbacks, readback_ns, rc_draws, rc_fallback, "
                         "pipelines, submits;"),
    ("the texture counters", "uint64_t tex_uploads, tex_hits, tex_undecodable, rt_aliases, "
                             "rt_copies;"),
    ("the wait counters", "uint64_t reuse_waits, drain_waits, splits, overflows;"),
    ("the publication counters", "uint64_t readback_batches, readback_packs, readback_pixels, "
                                 "readback_copies;"),
    ("the publication byte and time counters", "uint64_t readback_bytes, readback_wait_ns, "
                                               "readback_publish_ns;"),
    ("the unpublished-target counter", "uint64_t readback_incomplete;"),
    ("the published-target counter", "g.st.readbacks++"),
    ("the upload counter", "g.st.tex_uploads++"),
    ("the alias counter", "g.st.rt_aliases++"),
    ("the snapshot counter", "g.st.rt_copies++"),
    ("the load counter's increment", "g.st.rt_loads++"),
    ("the pack push block", "typedef struct { uint32_t rect[4], dst[4]; } VbPackPush;"),
    ("vb_fail", "static void vb_fail(const char *what, VkResult r)"),
    ("vb_guest", "static uint8_t *vb_guest(uint32_t va)"),
    ("vb_scratch", "static uint32_t *vb_scratch(size_t px)"),
    ("vb_mem_take", "static int vb_mem_take(const VkMemoryRequirements *mr, "
                    "VkMemoryPropertyFlags want, VkMemoryPropertyFlags fallback, "
                    "int nonlinear, int requires_dedicated, int prefers_dedicated, "
                    "const VkImage *dedicated_image, const VkBuffer *dedicated_buffer, "
                    "VbAlloc *out)"),
    ("vb_mem_give", "static void vb_mem_give(VbAlloc *a)"),
    ("vb_pool_gc", "static void vb_pool_gc(void)"),
    ("vb_host_buffer", "static int vb_host_buffer(VkDeviceSize size, VkBufferUsageFlags usage, "
                       "int cached, VkBuffer *buf, VbAlloc *mem, uint8_t **ptr)"),
    ("vb_device_buffer", "static int vb_device_buffer(VkDeviceSize size, "
                         "VkBufferUsageFlags usage, VkBuffer *buf, VbAlloc *mem)"),
    ("vb_discard", "static void vb_discard(VbGarbage e)"),
    ("vb_image_discard", "static void vb_image_discard(VbImage *im)"),
    ("vb_slot_wait", "static int vb_slot_wait(unsigned i)"),
    ("vb_slot_submit", "static void vb_slot_submit(void)"),
    ("vb_drain", "static void vb_drain(void)"),
    ("vb_slot_advance", "static void vb_slot_advance(void)"),
    ("vb_maybe_split", "static void vb_maybe_split(void)"),
    ("vb_cmd", "static VkCommandBuffer vb_cmd(void)"),
    ("vb_barrier", "static void vb_barrier(void)"),
    ("vb_dep", "static void vb_dep(VkPipelineStageFlags src_stage, VkAccessFlags src_access, "
               "VkPipelineStageFlags dst_stage, VkAccessFlags dst_access)"),
    ("vb_image_dep", "static void vb_image_dep(VkImage img, VkImageAspectFlags aspect, "
                     "VkImageLayout from, VkImageLayout to, VkPipelineStageFlags src_stage, "
                     "VkAccessFlags src_access, VkPipelineStageFlags dst_stage, "
                     "VkAccessFlags dst_access)"),
    ("vb_end_pass", "static void vb_end_pass(void)"),
    ("vb_transfer_cmd", "static VkCommandBuffer vb_transfer_cmd(void)"),
    ("vb_round_up", "static VkDeviceSize vb_round_up(VkDeviceSize bytes)"),
    ("vb_reserve", "static void vb_reserve(VkDeviceSize bytes, uint32_t sets)"),
    ("vb_stream", "static uint8_t *vb_stream(VkDeviceSize bytes, VkBuffer *buf, "
                  "VkDeviceSize *off)"),
    ("vb_image_create", "static int vb_image_create(VbImage *im, uint32_t w, uint32_t h, "
                        "VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)"),
    ("vb_framebuffer", "static VkFramebuffer vb_framebuffer(const VbTarget *t, "
                       "const VbDepth *d)"),
    ("vb_begin_pass", "static int vb_begin_pass(VbTarget *t, VbDepth *d)"),
    ("vb_target_release", "static void vb_target_release(VbTarget *t)"),
    ("vb_readback_slice", "static VkDeviceSize vb_readback_slice(const VbTarget *t, "
                          "uint32_t *row_stride)"),
    ("vb_readback_reserve", "static int vb_readback_reserve(VkDeviceSize bytes)"),
    ("vb_targets_flush", "static int vb_targets_flush(VbTarget **ts, unsigned n)"),
    ("vb_stop", "static void vb_stop(const char *why)"),
    ("vb_readback_recover", "static int vb_readback_recover(void)"),
    ("vb_targets_flush_recover", "static int vb_targets_flush_recover(VbTarget **ts, unsigned n)"),
    ("vb_target_load", "static void vb_target_load(VbTarget *t)"),
    ("vb_target_create", "static int vb_target_create(VbTarget *t, uint32_t va, uint32_t pitch, "
                         "uint32_t bpp, uint32_t w, uint32_t h)"),
    ("vb_target_for", "static VbTarget *vb_target_for(const Nv2aSurface *s)"),
    ("vb_target_mark", "static void vb_target_mark(VbTarget *t, const VkRect2D *r)"),
    ("vb_depth_for", "static VbDepth *vb_depth_for(uint32_t va, uint32_t w, uint32_t h)"),
    ("vb_tex_upload", "static void vb_tex_upload(VbTex *e, const Nv2aTexture *tx)"),
    ("vb_tex_aliases", "static int vb_tex_aliases(const Nv2aTexture *tx, const VbTarget *t)"),
    ("vb_texture", "static VkImageView vb_texture(const Nv2aTexture *tx, VbTarget *out, "
                   "VkImageLayout *layout, float *inv_w, float *inv_h)"),
    ("vb_sampler", "static VkSampler vb_sampler(const Nv2aTexture *tx)"),
    ("vb_stats", "static void vb_stats(int force)"),
    ("vb_flip", "static void vb_flip(void)"),
    ("vb_create_defaults", "static int vb_create_defaults(void)"),
    ("the public readback stats accessor",
     "void nv2a_vk_backend_readback_stats(Nv2aVkReadbackStats *out)"),
    # The two claims about the source the fixture records accesses FROM, neither of which the
    # fixture drives: every image the back end creates starts UNDEFINED (which is what the
    # transitions out of UNDEFINED mean), and the render pass LOADS its colour attachment and its
    # depth-stencil attachment and keeps both in GENERAL for their whole life (which is why a
    # pass's start is an attachment READ at COLOR_ATTACHMENT_OUTPUT / EARLY|LATE_FRAGMENT_TESTS).
    ("the created image starts UNDEFINED",
     "ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;"),
    ("the render pass loads its colour attachment",
     "att[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;"),
    ("... and its depth-stencil attachment is the same description",
     "att[1] = att[0];"),
    ("... and both stay in GENERAL between passes",
     "att[0].initialLayout = att[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;"),
]

# The six N4.3 scope macros, as the specification's table fixes them: the multiset of Vulkan
# tokens each one must OR together. Order is not compared (a reflow or a reorder is not a value
# change), a token that is missing or different is.
MACROS = {
    "VB_STAGE_IMAGE_DST": {
        "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT",
        "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT",
        "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT",
        "VK_PIPELINE_STAGE_TRANSFER_BIT",
    },
    "VB_ACCESS_IMAGE_DST": {
        "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT",
        "VK_ACCESS_COLOR_ATTACHMENT_READ_BIT",
        "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT",
        "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT",
        "VK_ACCESS_SHADER_READ_BIT",
        "VK_ACCESS_TRANSFER_READ_BIT",
        "VK_ACCESS_TRANSFER_WRITE_BIT",
    },
    "VB_STAGE_ATTACH": {
        "VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT",
        "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT",
    },
    "VB_ACCESS_ATTACH_WRITE": {
        "VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT",
        "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT",
    },
    "VB_STAGE_DEPTH": {
        "VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT",
        "VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT",
    },
    "VB_ACCESS_DEPTH": {
        "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT",
        "VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT",
    },
}

# The sites whose scopes the markers describe. Each is the function whose body the marker's call
# text is looked for in, and the call text itself; the delimiters are the function's signature and
# the next definition after it, exactly as the ordering gate slices its own site out of the file.
SLOTS = "static void vb_slot_submit(void)"
SLOT_DRAIN = "static void vb_drain(void)"
PASS = "static void vb_end_pass(void)"
PASS_TRANSFER = "static VkCommandBuffer vb_transfer_cmd(void)"
LOAD = "static void vb_target_load(VbTarget *t)"
TCREATE = "static int vb_target_create(VbTarget *t, uint32_t va, uint32_t pitch, uint32_t bpp,"
TFOR = "static VbTarget *vb_target_for(const Nv2aSurface *s)"
DEPTH = "static VbDepth *vb_depth_for(uint32_t va, uint32_t w, uint32_t h)"
ZSCALE = "static float vb_z_scale(const Nv2aRenderState *rs)"
TEXUP = "static void vb_tex_upload(VbTex *e, const Nv2aTexture *tx)"
TEXALIAS = "static int vb_tex_aliases(const Nv2aTexture *tx, const VbTarget *t)"
TEX = "static VkImageView vb_texture(const Nv2aTexture *tx, VbTarget *out, "
TEXEND = "static uint32_t vb_sampler_mode(uint32_t addr)"
FLUSH = "static int vb_targets_flush(VbTarget **ts, unsigned n)"
FLUSH_END = "static void vb_stop(const char *why)"
DEFAULTS = "static int vb_create_defaults(void)"
DEFAULTS_END = "static int vb_stopped(void)"
BROAD = "static void vb_barrier(void)"
BROAD_END = "static void vb_dep(VkPipelineStageFlags src_stage, VkAccessFlags src_access,"

# The abandonment branch, whose own text the retained-broad marker is read out of.
ABANDON = "if (!batch_ok)"
CONTINUE = "continue;"

# The scoped call each site must make, normalised. `VB_*` macros are named, not expanded: their
# VALUES are checked by MACROS above, so a call is allowed to name them and must name them.
S1_CALL = ("vb_dep(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
           "VB_STAGE_IMAGE_DST, VB_ACCESS_IMAGE_DST)")
S2_CALL = ("vb_dep(VB_STAGE_ATTACH, VB_ACCESS_ATTACH_WRITE, VB_STAGE_IMAGE_DST, "
           "VB_ACCESS_IMAGE_DST)")
S3_CALL = ("vb_dep(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, "
           "VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT)")
S5_CALL = ("vb_dep(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
           "VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT "
           "| VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT | "
           "VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)")
S7_CALL = S1_CALL
S8_CALL = ("vb_dep(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VB_STAGE_DEPTH, "
           "VB_ACCESS_DEPTH)")
UNDEF_DST = ("vb_image_dep(%s, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, "
             "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, "
             "VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT)")
SAMPLE_READ = ("vb_image_dep(%s, VK_IMAGE_ASPECT_COLOR_BIT, "
               "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "
               "VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, "
               "VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, "
               "VK_ACCESS_SHADER_READ_BIT)")
UNDEF_GENERAL = ("vb_image_dep(%s, %s, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, "
                 "VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT, "
                 "VK_ACCESS_TRANSFER_WRITE_BIT)")


def flat(text):
    return re.sub(r"\s+", " ", text)


def norm(text):
    return re.sub(r"\s+", "", text)


def body(text, start_anchor, end_anchor):
    """The source between a definition's signature and the next definition after it."""
    start = text.find(start_anchor)
    end = text.find(end_anchor, start + len(start_anchor)) if start >= 0 else -1
    if start < 0 or end < 0:
        return None
    return text[start:end]


def macro_value(text, name):
    """The tokens a `#define` mnemonics ORs together, as a set, or None if it is not there."""
    start = text.find("#define " + name)
    if start < 0:
        return None
    end = start
    while True:
        nl = text.find("\n", end)
        if nl < 0:
            return None
        line = text[end:nl]
        end = nl + 1
        if not line.rstrip().endswith("\\"):
            break
    definition = text[start:end]
    body_text = definition[len("#define " + name):]
    body_text = body_text.replace("\\", " ")
    body_text = body_text.strip()
    if body_text.startswith("(") and body_text.endswith(")"):
        body_text = body_text[1:-1]
    tokens = [t.strip() for t in body_text.split("|")]
    tokens = [t for t in tokens if t]
    if not tokens:
        return set()
    return set(tokens)


def count_calls(text, call):
    return norm(text).count(norm(call))


def probe(text):
    """Every marker the sync-scope gate compiles against, read out of this source."""
    f = flat(text)
    bodies = {}
    for name, (start, end) in {
        "submit": (SLOTS, SLOT_DRAIN),
        "pass": (PASS, PASS_TRANSFER),
        "load": (LOAD, TCREATE),
        "tcreate": (TCREATE, TFOR),
        "depth": (DEPTH, ZSCALE),
        "texup": (TEXUP, TEXALIAS),
        "texture": (TEX, TEXEND),
        "flush": (FLUSH, FLUSH_END),
        "defaults": (DEFAULTS, DEFAULTS_END),
        "broad": (BROAD, BROAD_END),
    }.items():
        bodies[name] = body(text, start, end) or ""
    flush_f = flat(bodies["flush"])
    abandon_at = flush_f.find(ABANDON)
    abandon = ""
    if abandon_at >= 0:
        stop = flush_f.find(CONTINUE, abandon_at + len(ABANDON))
        if stop >= 0:
            abandon = flush_f[abandon_at:stop + len(CONTINUE)]

    def has(where, call):
        return norm(call) in norm(bodies[where])

    def undef_of(where, image, aspect="VK_IMAGE_ASPECT_COLOR_BIT"):
        return has(where, UNDEF_DST % image)

    submit_calls = norm(bodies["submit"]).count("vb_barrier();")
    return {
        # ── the sites' scopes, read out of the source text ───────────────────────────────
        "VB_SYNC_SCOPE_GATE_S1_LOAD_SCOPED": int(has("load", S1_CALL)),
        "VB_SYNC_SCOPE_GATE_S2_PASS_SCOPED": int(has("pass", S2_CALL)),
        "VB_SYNC_SCOPE_GATE_S3_PACK_SCOPED": int(has("flush", S3_CALL)),
        # The publish barrier: a memory barrier into HOST whose source scope names BOTH device
        # writers (the pack kernel's shader writes and the copy engine's writes), recorded with
        # the flush's own two masks.
        "VB_SYNC_SCOPE_GATE_S4_PUBLISH_SCOPED": int(
            "mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;"
            in flush_f
            and "mb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;" in flush_f
            and "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, "
                "VK_PIPELINE_STAGE_HOST_BIT," in flush_f),
        "VB_SYNC_SCOPE_GATE_S5_SNAPSHOT_SCOPED": int(has("texture", S5_CALL)),
        "VB_SYNC_SCOPE_GATE_S6A_TEXTURE_UNDEF_SCOPED": int(undef_of("texup", "e->im.img")),
        "VB_SYNC_SCOPE_GATE_S6B_TEXTURE_SAMPLE_SCOPED": int(
            has("texup", SAMPLE_READ % "e->im.img")),
        "VB_SYNC_SCOPE_GATE_S6A_WHITE_UNDEF_SCOPED": int(undef_of("defaults", "g.white.img")),
        "VB_SYNC_SCOPE_GATE_S6B_WHITE_SAMPLE_SCOPED": int(
            has("defaults", SAMPLE_READ % "g.white.img")),
        "VB_SYNC_SCOPE_GATE_S7_CLEAR_SCOPED": int(has("tcreate", S7_CALL)),
        "VB_SYNC_SCOPE_GATE_S7B_TARGET_UNDEF_SCOPED": int(
            has("tcreate", UNDEF_GENERAL % ("t->im.img", "VK_IMAGE_ASPECT_COLOR_BIT"))),
        "VB_SYNC_SCOPE_GATE_S8_DEPTH_CLEAR_SCOPED": int(has("depth", S8_CALL)),
        "VB_SYNC_SCOPE_GATE_S8B_DEPTH_UNDEF_SCOPED": int(
            has("depth", UNDEF_GENERAL % ("d->im.img",
                                          "VK_IMAGE_ASPECT_DEPTH_BIT "
                                          "| VK_IMAGE_ASPECT_STENCIL_BIT"))),
        "VB_SYNC_SCOPE_GATE_S9_SNAPSHOT_UNDEF_SCOPED": int(
            has("texture", UNDEF_GENERAL % ("t->copy.img", "VK_IMAGE_ASPECT_COLOR_BIT"))),
        # ── the retained broad form, and what is left of it ───────────────────────────────
        # Exactly two `vb_barrier();` calls remain in the whole file: one in the submission path,
        # one in the flush's abandonment branch. That is the narrowing this packet claims, and it
        # is what the gate's P3 counts against the recorded stream.
        "VB_SYNC_SCOPE_GATE_RETAINED_TWO_BROAD": int(
            f.count("vb_barrier();") == 2 and submit_calls == 1 and norm("vb_barrier();")
            in norm(abandon)),
        # The abandonment branch itself: reported, never decided here. The ordering gate
        # (jsrf_vk_readback_ordering_gate) owns what it must order.
        "VB_SYNC_SCOPE_GATE_ABANDON_BROAD": int(norm("vb_barrier();") in norm(abandon)),
        # The publish barrier is NOT the broad form: it is the scoped HOST one recorded by the
        # flush, so the count of broad barriers stays one per submission.
        "VB_SYNC_SCOPE_GATE_PUBLISH_NOT_BROAD": int(
            "VK_PIPELINE_STAGE_ALL_COMMANDS_BIT" not in flush_f),
    }


def c_string(text):
    out = text.replace("\\", "\\\\").replace('"', '\\"')
    return '"%s"' % out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help="the toolkit's src/video/nv2a_vk_backend.c")
    ap.add_argument("--header", required=True, help="the seam header to write")
    ap.add_argument("--manifest", required=True, help="the seam manifest to write")
    args = ap.parse_args()

    path = Path(args.source)
    if not path.is_file():
        print("vk-sync-scope-gate: no such source: %s" % path, file=sys.stderr)
        return 2
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    f = flat(text)

    missing = [name for name, literal in REQUIRED if flat(literal) not in f]
    if missing:
        print("vk-sync-scope-gate: %s no longer provides what the sync-scope gate binds to:"
              % path, file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The test drives the production bodies of this file; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    wrong = []
    for name, want in MACROS.items():
        got = macro_value(text, name)
        if got is None:
            wrong.append("%s: not defined" % name)
        elif got != want:
            wrong.append("%s: %s" % (name, " | ".join(sorted(got)) if got else "(empty)"))
    if wrong:
        print("vk-sync-scope-gate: %s defines a scope macro with a value other than the one "
              "this gate was built against:" % path, file=sys.stderr)
        for line in wrong:
            print("  %s" % line, file=sys.stderr)
        print("The gate binds its recorded scopes to these macros; a macro whose value moved "
              "would decide a different synchronization than the one it reports, so this is a "
              "build error and not a warning.", file=sys.stderr)
        return 2

    seam = probe(text)
    digest = hashlib.sha256(raw).hexdigest()
    source = str(path.resolve())

    header = ["/* Generated by scripts/vk-sync-scope-gate.py from the toolkit's Vulkan back end.",
              " * Do not edit: the file is rebuilt from the source every time the source "
              "changes.",
              " *",
              " * The sync-scope gate compiles that source into its own translation unit, so the",
              " * bodies it drives are the production ones. These markers describe the scopes the",
              " * source's own text passes at each narrowed site; where one is 0, the test reports",
              " * a failed check naming it -- but the DECISION is always the recorded command",
              " * stream, so a site that moved still builds and is still decided. */",
              "#ifndef VK_SYNC_SCOPE_GATE_SEAM_H",
              "#define VK_SYNC_SCOPE_GATE_SEAM_H",
              "",
              "#define VB_SYNC_SCOPE_GATE_SOURCE %s" % c_string(source),
              "#define VB_SYNC_SCOPE_GATE_SOURCE_SHA256 %s" % c_string(digest),
              "#define VB_SYNC_SCOPE_GATE_SOURCE_BYTES %d" % len(raw),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_SYNC_SCOPE_GATE_SEAM_H */")

    manifest = ["vk-sync-scope-gate: the seam of %s" % source,
                "  sha256  %s" % digest,
                "  bytes   %d" % len(raw),
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                "  scope macros matching the packet's table: %d of %d"
                % (len(MACROS), len(MACROS)),
                ""]
    for name in sorted(seam):
        manifest.append("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("A marker marked ABSENT is a site whose source TEXT no longer passes the")
    manifest.append("scope the packet's table fixes for it. The gate reports it as a failed")
    manifest.append("check naming the site, and decides the site from the recorded command")
    manifest.append("stream as well: this gate is built to be pointed at mutant sources -- a")
    manifest.append("site narrowed, a transition removed, a site replaced by the retained broad")
    manifest.append("vb_barrier() -- where the text must still build so that the decision comes")
    manifest.append("out of the recording and not out of a grep.")
    manifest.append("")
    manifest.append("The six VB_* scope macros are NOT markers: they are required, and their")
    manifest.append("values are compared term by term with the packet's table. The test binds the")
    manifest.append("scopes of every recorded barrier to them, so a macro whose value moved must")
    manifest.append("fail the build rather than decide a different synchronization.")
    manifest.append("")
    manifest.append("VB_SYNC_SCOPE_GATE_RETAINED_TWO_BROAD counts the two broad vb_barrier()")
    manifest.append("sites the packet deliberately keeps. WHICH of them is the abandonment")
    manifest.append("barrier, and what it must order, is decided by")
    manifest.append("jsrf_vk_readback_ordering_gate and is only reported here.")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-sync-scope-gate: probed %s (sha256 %s)" % (path, digest[:16]))
    for name in sorted(seam):
        print("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
