"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_sync_paths_gate.c binds to.

The N4.3 device gate (tests/test_vk_sync_paths_gate.c) compiles the toolkit's own
src/video/nv2a_vk_backend.c into its translation unit -- so the bodies it drives and the barrier
scopes it decides are the production ones, verbatim -- installs the REAL back end on the configured
software ICD, and records the driver call stream with forwarders that call the real entry points
the install loaded. This script is the build-time half: it reads the same file and refuses the
build when what the test binds to is not there.

  * Every REQUIRED anchor is a definition, a struct field, a global, a #define or a function
    pointer declaration the test's C code names. One that is gone is a hard error (exit 2), which
    fails the custom command and the build. It never skips and never warns-and-continues: a moved
    or renamed seam must break the build, not silently turn the case into something else.

  * The STRUCTURE of the change is probed as MARKERS (emitted as #define ... 0|1 into the seam
    header). They are not anchors, so a source in which one of them is absent still builds and the
    case reports a FAILED CHECK naming it -- which is what makes the gate able to decide a
    reverted, partially applied or otherwise mutated change instead of failing to compile.

The markers are read out of the source's own text, via the flattened (whitespace-normalised)
image or out of the body of one function:

  vb_dep / vb_image_dep exist, vb_image_barrier is gone, vb_barrier() has exactly two call sites
  and they are vb_slot_submit's and vb_targets_flush's `if (!batch_ok)` branch, exactly seven
  vb_image_dep call sites (the layout transitions), and the file's only direct
  vkCmdPipelineBarrier calls are the three helper bodies plus its own publish barrier (whose scopes
  the run-time census decides) -- so no changed site records a barrier by hand.

The test re-checks the macros' VALUES at run time (against its own spelling of the N4.3 table), so
a macro whose value drifted fails a check even though this script would still call it present.

Outputs (both declared to CMake as one custom command):
  --header    vk_sync_paths_gate_seam.h   the markers + the source's identity, #included by the test
  --manifest  vk_sync_paths_gate_seam.txt the same verdict in prose, kept next to the build
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
    # the fixture's sizes and the ring-overlap sizing
    ("the target bound", "#define VB_MAX_TARGETS   16"),
    ("the depth bound", "#define VB_MAX_DEPTHS    8"),
    ("the texture bound", "#define VB_MAX_TEXTURES  512"),
    ("the 4096 pixel dimension limit", "#define VB_MAX_DIM       4096"),
    ("the ring size", "#define VB_SLOTS         3"),
    ("one slot's stream size", "#define VB_SLOT_STREAM   (16u << 20)"),
    ("the stream alignment", "#define VB_STREAM_ALIGN 256u"),
    ("one slot's descriptor-set budget", "#define VB_SLOT_SETS     2048"),
    ("the publication staging cap", "#define VB_READBACK_MAX  (96u << 20)"),
    ("the draw-entry split byte threshold", "#define VB_SPLIT_BYTES   (VB_SLOT_STREAM / 8)"),
    ("the draw-entry split set threshold", "#define VB_SPLIT_SETS    (VB_SLOT_SETS / 64)"),
    # the structs the fixture reads (targets, images, textures, depths) and the back end's state
    ("the image struct", "} VbImage;"),
    ("the image's handle", "VkImage img;"),
    ("the image's view", "VkImageView view;"),
    ("the target struct", "} VbTarget;"),
    ("the target's guest surface fields", "uint32_t va, pitch, bpp, w, h;"),
    ("the target's image and snapshot", "VbImage im, copy;"),
    ("the target's stale flag", "int stale;"),
    ("the target's dirty flag", "int dirty;"),
    ("the target's dirty rectangle", "uint32_t dx0, dy0, dx1, dy1;"),
    ("the depth struct", "} VbDepth;"),
    ("the texture struct", "} VbTex;"),
    ("the back end's state object", "} g;"),
    ("the device handle", "VkDevice dev;"),
    ("the queue handle", "VkQueue queue;"),
    ("the stopped/failed state", "int ok;"),
    ("the failure latch", "int failed;"),
    ("the dead command buffer", "VkCommandBuffer dead;"),
    ("the slot ring", "VbSlot slots[VB_SLOTS];"),
    ("the current slot", "unsigned cur;"),
    ("the pending-slot count", "unsigned npending;"),
    ("the highest overlap the ring reached", "unsigned max_inflight;"),
    ("the open pass flag", "int pass_open;"),
    ("the target array", "VbTarget targets[VB_MAX_TARGETS];"),
    ("the depth array", "VbDepth depths[VB_MAX_DEPTHS];"),
    ("the texture array", "VbTex texs[VB_MAX_TEXTURES];"),
    ("the 1x1 white texture", "VbImage white;"),
    ("the frame counter", "uint32_t frame;"),
    ("the validation-message bound", "unsigned fails, validation_said;"),
    ("the statistics object", "VbStats st;"),
    ("the validation counters", "uint64_t validation_errors, validation_warnings;"),
    ("the render-target load counter", "uint64_t rt_loads, readbacks, readback_ns, rc_draws, "
                                       "rc_fallback, pipelines, submits;"),
    ("the texture and alias counters", "uint64_t tex_uploads, tex_hits, tex_undecodable, "
                                       "rt_aliases, rt_copies;"),
    ("the publication counters", "uint64_t readback_batches, readback_packs, readback_pixels, "
                                 "readback_copies;"),
    ("the publication byte counters", "uint64_t readback_bytes, readback_wait_ns, "
                                      "readback_publish_ns;"),
    ("the unpublished-target counter", "uint64_t readback_incomplete;"),
    ("the split and overflow counters", "uint64_t reuse_waits, drain_waits, splits, overflows;"),
    # the device function pointers the recording forwarders replace
    ("vkCmdPipelineBarrier's declaration", "X(vkCmdPipelineBarrier)"),
    ("vkCmdCopyBufferToImage's declaration", "X(vkCmdCopyBufferToImage)"),
    ("vkCmdCopyImageToBuffer's declaration", "X(vkCmdCopyImageToBuffer)"),
    ("vkCmdCopyBuffer's declaration", "X(vkCmdCopyBuffer)"),
    ("vkCmdCopyImage's declaration", "X(vkCmdCopyImage)"),
    ("vkCmdClearColorImage's declaration", "X(vkCmdClearColorImage)"),
    ("vkCmdClearDepthStencilImage's declaration", "X(vkCmdClearDepthStencilImage)"),
    ("vkCmdDispatch's declaration", "X(vkCmdDispatch)"),
    ("vkCmdDraw's declaration", "X(vkCmdDraw)"),
    ("vkCmdDrawIndexed's declaration", "X(vkCmdDrawIndexed)"),
    ("vkCmdBeginRenderPass's declaration", "X(vkCmdBeginRenderPass)"),
    ("vkCmdEndRenderPass's declaration", "X(vkCmdEndRenderPass)"),
    ("vkBeginCommandBuffer's declaration", "X(vkBeginCommandBuffer)"),
    ("vkEndCommandBuffer's declaration", "X(vkEndCommandBuffer)"),
    ("vkResetCommandBuffer's declaration", "X(vkResetCommandBuffer)"),
    ("vkQueueSubmit's declaration", "X(vkQueueSubmit)"),
    ("vkWaitForFences's declaration", "X(vkWaitForFences)"),
    # the scope macros the case compares against its own spelling of the table
    ("VB_STAGE_IMAGE_DST", "#define VB_STAGE_IMAGE_DST (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_"
                           "OUTPUT_BIT"),
    ("VB_ACCESS_IMAGE_DST", "#define VB_ACCESS_IMAGE_DST (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT"),
    ("VB_STAGE_ATTACH", "#define VB_STAGE_ATTACH (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT"),
    ("VB_ACCESS_ATTACH_WRITE", "#define VB_ACCESS_ATTACH_WRITE (VK_ACCESS_COLOR_ATTACHMENT_"
                               "WRITE_BIT"),
    ("VB_STAGE_DEPTH", "#define VB_STAGE_DEPTH (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT"),
    ("VB_ACCESS_DEPTH", "#define VB_ACCESS_DEPTH (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT"),
    # the production functions the fixture drives
    ("vb_barrier", "static void vb_barrier(void);"),
    ("vb_dep", "static void vb_dep(VkPipelineStageFlags src_stage, VkAccessFlags src_access, "
                "VkPipelineStageFlags dst_stage, VkAccessFlags dst_access)"),
    ("vb_image_dep", "static void vb_image_dep(VkImage img, VkImageAspectFlags aspect, "
                     "VkImageLayout from, VkImageLayout to, VkPipelineStageFlags src_stage, "
                     "VkAccessFlags src_access, VkPipelineStageFlags dst_stage, "
                     "VkAccessFlags dst_access)"),
    ("vb_end_pass", "static void vb_end_pass(void)"),
    ("vb_slot_submit", "static void vb_slot_submit(void)"),
    ("vb_drain", "static void vb_drain(void)"),
    ("vb_flip", "static void vb_flip(void)"),
    ("vb_target_load", "static void vb_target_load(VbTarget *t)"),
    ("vb_target_create", "static int vb_target_create(VbTarget *t, uint32_t va, uint32_t pitch, "
                         "uint32_t bpp, uint32_t w, uint32_t h)"),
    ("vb_targets_flush", "static int vb_targets_flush(VbTarget **ts, unsigned n)"),
    ("vb_targets_flush_recover", "static int vb_targets_flush_recover(VbTarget **ts, unsigned n)"),
    ("vb_depth_for", "static VbDepth *vb_depth_for(uint32_t va, uint32_t w, uint32_t h)"),
    ("vb_tex_upload", "static void vb_tex_upload(VbTex *e, const Nv2aTexture *tx)"),
    ("vb_texture", "static VkImageView vb_texture(const Nv2aTexture *tx, VbTarget *out, "
                   "VkImageLayout *layout, float *inv_w, float *inv_h)"),
    ("vb_begin_pass", "static int vb_begin_pass(VbTarget *t, VbDepth *d)"),
    ("vb_clear", "static void vb_clear(const Nv2aSurface *s, const Nv2aRenderState *rs, "
                 "uint32_t flags, uint32_t argb, uint32_t zstencil)"),
    ("vb_draw", "static void vb_draw(const Nv2aSurface *s, const Nv2aBatch *b)"),
    ("vb_create_defaults", "static int vb_create_defaults(void)"),
    ("vb_guest", "static uint8_t *vb_guest(uint32_t va)"),
    ("vb_now_ns", "static uint64_t vb_now_ns(void)"),
    # the public seams the case reports through
    ("the install entry point", "int nv2a_vk_backend_install(void)"),
    ("the ring stats accessor", "void nv2a_vk_backend_ring_stats(Nv2aVkRingStats *out)"),
    ("the readback stats accessor", "void nv2a_vk_backend_readback_stats(Nv2aVkReadbackStats *out)"),
]

# vb_slot_submit's body is where the per-submission barrier lives, and vb_targets_flush's
# `if (!batch_ok)` branch is where the abandonment barrier lives. The two delimiters give the
# probe those two bodies out of the file's own bytes.
SUBMIT_START = "static void vb_slot_submit(void)"
SUBMIT_END = "static void vb_drain(void)"
FLUSH_START = "static int vb_targets_flush(VbTarget **ts, unsigned n)"
FLUSH_END = "static void vb_stop(const char *why)"
ABANDON = "if (!batch_ok)"


def flat(text):
    return re.sub(r"\s+", " ", text)


def body(text, start_marker, end_marker):
    """The slice from one definition to the next, or None if either is gone."""
    start = text.find(start_marker)
    end = text.find(end_marker, start + len(start_marker)) if start >= 0 else -1
    if start < 0 or end < 0:
        return None
    return text[start:end]


def abandon_branch(flat_flush):
    """The `if (!batch_ok) { ... }` branch, up to the `continue;` that ends it."""
    start = flat_flush.find(ABANDON)
    if start < 0:
        return None
    end = flat_flush.find("continue;", start + len(ABANDON))
    if end < 0:
        return None
    return flat_flush[start:end + len("continue;")]


def probe(source_text):
    """The structure of the N4.3 change, as markers the test can compile against."""
    f = flat(source_text)
    submit = body(f, SUBMIT_START, SUBMIT_END)
    flush = body(f, FLUSH_START, FLUSH_END)
    branch = abandon_branch(flush) if flush is not None else None

    # vb_barrier(); -- the retained broad form: the declaration is not a call, so the call sites
    # are what the test's property turns on.
    broad_calls = f.count("vb_barrier();")
    # vb_image_dep( -- once in its own definition and once per layout transition.
    image_dep = f.count("vb_image_dep(")
    # vkCmdPipelineBarrier( -- only inside the three helper bodies; a changed site that recorded a
    # barrier by hand would add one here.
    raw_barriers = f.count("vkCmdPipelineBarrier(")

    return {
        "VB_SYNC_GATE_HAS_VB_DEP": int(
            "static void vb_dep(VkPipelineStageFlags src_stage" in f),
        "VB_SYNC_GATE_HAS_VB_IMAGE_DEP": int(
            "static void vb_image_dep(VkImage img, VkImageAspectFlags aspect" in f),
        # The blanket helper this change removed. A surviving vb_image_barrier is the change
        # reverted, and the case says so as a failed check rather than a build error.
        "VB_SYNC_GATE_NO_VB_IMAGE_BARRIER": int("vb_image_barrier" not in f),
        # Exactly two call sites, and they are the two the table names.
        "VB_SYNC_GATE_BROAD_SITES_TWO": int(broad_calls == 2),
        "VB_SYNC_GATE_BROAD_AT_SUBMIT_AND_ABANDON": int(
            submit is not None and flush is not None and branch is not None
            and "vb_barrier();" in submit and "vb_barrier();" in branch
            and broad_calls == 2),
        # Seven transitions: texture upload x2, the 1x1 white texture x2, target, depth and
        # snapshot creates. Counted as calls (the definition adds one).
        "VB_SYNC_GATE_IMAGE_DEP_SEVEN": int(image_dep == 8),
        # No changed site records a barrier by hand: the only direct vkCmdPipelineBarrier calls are
        # the three helper bodies plus vb_targets_flush's own publish barrier, and the abandonment
        # branch uses the file's broad helper rather than a hand-written barrier.
        "VB_SYNC_GATE_SITES_ALL_NARROW": int(
            raw_barriers == 4 and branch is not None
            and "vkCmdPipelineBarrier(" not in branch
            and "VK_PIPELINE_STAGE_HOST_BIT" in (flush or "")
            and "VK_ACCESS_HOST_READ_BIT" in (flush or "")),
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
        print("vk-sync-paths-gate: no such source: %s" % path, file=sys.stderr)
        return 2
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    f = flat(text)

    missing = [name for name, literal in REQUIRED if flat(literal) not in f]
    if missing:
        print("vk-sync-paths-gate: %s no longer provides what the synchronization-paths gate "
              "binds to:" % path, file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The test drives the production bodies of this file, replaces the driver entry "
              "points it observes and reports through its counters; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    seam = probe(text)
    digest = hashlib.sha256(raw).hexdigest()
    source = str(path.resolve())

    header = ["/* Generated by scripts/vk-sync-paths-gate.py from the toolkit's Vulkan back end.",
              " * Do not edit: the file is rebuilt from the source every time the source changes.",
              " *",
              " * The synchronization-paths gate compiles that source into its own translation unit,",
              " * installs the real back end on the configured device and records the driver call",
              " * stream with forwarders, so the bodies it drives and the scopes it decides are the",
              " * production ones. These markers describe the N4.3 change's structure in the file the",
              " * test was built against; where one is 0, the test reports a FAILED CHECK naming it",
              " * (it never skips, and it never silently narrows what it decides). */",
              "#ifndef VK_SYNC_PATHS_GATE_SEAM_H",
              "#define VK_SYNC_PATHS_GATE_SEAM_H",
              "",
              "#define VB_SYNC_GATE_SOURCE %s" % c_string(source),
              "#define VB_SYNC_GATE_SOURCE_SHA256 %s" % c_string(digest),
              "#define VB_SYNC_GATE_SOURCE_BYTES %d" % len(raw),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_SYNC_PATHS_GATE_SEAM_H */")

    manifest = ["vk-sync-paths-gate: the seam of %s" % source,
                "  sha256  %s" % digest,
                "  bytes   %d" % len(raw),
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                ""]
    for name in sorted(seam):
        manifest.append("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("A marker marked ABSENT is a structure this gate decides: with vb_image_barrier")
    manifest.append("back, or with the broad vb_barrier() at a site the table lists as narrowed, the")
    manifest.append("case reports the failed check that marker stands for -- from the recorded")
    manifest.append("driver stream and the published bytes, on the real device.")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-sync-paths-gate: probed %s (sha256 %s)" % (path, digest[:16]))
    for name in sorted(seam):
        print("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
