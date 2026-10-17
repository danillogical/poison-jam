"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_readback_callers.c binds to.

The readback caller gate compiles the toolkit's own src/video/nv2a_vk_backend.c into its
translation unit, so the four coherence boundaries it drives -- vb_flip, vb_target_load,
vb_target_for and vb_texture -- are the production ones, verbatim. This script is the other
half of that: it reads the same file AT BUILD TIME and refuses the build when what the test
needs is not there.

  * Every REQUIRED anchor is a definition, a struct field, a global or a #define the test's
    C code names. One that is gone is a hard error (exit 2), which fails the custom command
    and the build. It never skips and never warns-and-continues: a moved or renamed seam must
    break the build, not silently turn the case into something else.

  * The repair's own seam is probed as a set of MARKERS rather than required, and that
    distinction is deliberate: the pre-fix source (git 891bca2) must still BUILD, because the
    case has to run against it and report the defect per property. Each marker becomes a
    #define in the generated header; the test PRINTS the markers and decides nothing with
    them -- the properties below the seam are what decide.

The markers are read out of the three callers' own bodies (each sliced out of the file by its
definition and the definition that follows it) and out of the flush's declaration, because
"the flip stops before it marks anything stale" is a statement about vb_flip's body and
nothing else.

Outputs (both declared to CMake as one custom command):
  --header    vk_readback_caller_gate_seam.h   the markers + the source's identity, #included
                                              by the test
  --manifest  vk_readback_caller_gate_seam.txt the same verdict in prose, kept next to the build
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

# Every literal below is normalised for whitespace before matching, so reflowing a
# declaration's parameters does not break the probe -- but its name, its parameter list,
# its field names and the values the test computes with must still be there. Every anchor
# here is present in BOTH the pre-fix source and the repaired one: this probe must not be
# the thing that decides which of them the case is built against.
REQUIRED = [
    ("the Vulkan branch of the file", "#if !defined(XBR_VK_BACKEND)"),
    ("the guest-memory accessor", "extern ptrdiff_t xbox_GetMemoryOffset(void);"),
    ("the publication staging cap", "#define VB_READBACK_MAX (96u << 20)"),
    ("the target bound", "#define VB_MAX_TARGETS 16"),
    ("the texture bound", "#define VB_MAX_TEXTURES 512"),
    ("the dimension bound", "#define VB_MAX_DIM 4096"),
    ("the ring size", "#define VB_SLOTS 3"),
    ("the stream slice alignment", "#define VB_STREAM_ALIGN 256u"),
    ("the target struct", "} VbTarget;"),
    ("the target's guest surface fields", "uint32_t va, pitch, bpp, w, h;"),
    ("the target's image and view", "VbImage im, copy;"),
    ("the target's stale flag", "int stale;"),
    ("the target's dirty flag", "int dirty;"),
    ("the target's dirty rectangle", "uint32_t dx0, dy0, dx1, dy1;"),
    ("the texture struct", "} VbTex;"),
    ("the slot struct", "} VbSlot;"),
    ("the slot's command buffer field", "VkCommandBuffer cmd;"),
    ("the slot's fence field", "VkFence fence;"),
    ("the slot's pending bit", "int pending;"),
    ("the slot's recording bit", "int recording;"),
    ("the slot's descriptor pool field", "VkDescriptorPool dpool;"),
    ("the slot's stream fields", "VkBuffer stream;"),
    ("the slot's mapped stream pointer", "uint8_t *stream_ptr;"),
    ("the slot's stream offset field", "VkDeviceSize stream_off;"),
    ("the back end's state object", "} g;"),
    ("the health flag", "int ok;"),
    ("the failure latch", "int failed;"),
    ("the device handle", "VkDevice dev;"),
    ("the queue handle", "VkQueue queue;"),
    ("the memory properties", "VkPhysicalDeviceMemoryProperties mp;"),
    ("the current slot", "unsigned cur;"),
    ("the pending-slot count", "unsigned npending;"),
    ("the frame counter", "uint32_t frame;"),
    ("the failure counter", "unsigned fails, validation_said;"),
    ("the stats object", "VbStats st;"),
    ("the target array", "VbTarget targets[VB_MAX_TARGETS];"),
    ("the texture array", "VbTex texs[VB_MAX_TEXTURES];"),
    ("the back end's fps/readback stat fields",
     "uint64_t readback_batches, readback_packs, readback_pixels, readback_copies;"),
    ("the readback load counter", "uint64_t rt_loads, readbacks, readback_ns,"),
    ("the pack push block", "typedef struct { uint32_t rect[4], dst[4]; } VbPackPush;"),
    ("the staging pair's fields", "VkBuffer pack_buf;"),
    ("the staging pair's host-visible buffer field", "VkBuffer rb_buf;"),
    ("the staging pair's mapped pointer", "uint8_t *rb_ptr;"),
    ("the staging pair's capacity", "VkDeviceSize rb_cap;"),
    ("the pack set layout field", "VkDescriptorSetLayout dsl_pack;"),
    ("the pack push layout field", "VkPipelineLayout layout_pack;"),
    ("the pack pipeline field", "VkPipeline pipe_pack;"),
    ("the published-target counter", "g.st.readbacks++"),
    ("the readback slice", "static VkDeviceSize vb_readback_slice(const VbTarget *t, "
                           "uint32_t *row_stride)"),
    ("the staging reservation", "static int vb_readback_reserve(VkDeviceSize bytes)"),
    # No anchor on a one-target flush wrapper: the repair removed it (both former call sites go
    # through the boundary entry point), and this case does not drive it. The flush itself is
    # anchored below without a return type, because the repair adds one and this case is built
    # against the pre-fix source too.
    ("the target flush", "vb_targets_flush(VbTarget **ts, unsigned n)"),
    ("the target release", "static void vb_target_release(VbTarget *t)"),
    ("the target creation", "static int vb_target_create(VbTarget *t, uint32_t va, "
                            "uint32_t pitch, uint32_t bpp, uint32_t w, uint32_t h)"),
    ("the image creation", "static int vb_image_create(VbImage *im, uint32_t w, uint32_t h, "
                           "VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)"),
    ("the garbage list", "static void vb_discard(VbGarbage e)"),
    ("the framebuffer forgetter", "static void vb_fb_forget(VkImageView view)"),
    ("the error reporter", "static void vb_fail(const char *what, VkResult r)"),
    ("the transfer command buffer", "static VkCommandBuffer vb_transfer_cmd(void)"),
    ("the recording command buffer", "static VkCommandBuffer vb_cmd(void)"),
    ("the barrier recorder", "static void vb_barrier(void)"),
    ("the slot wait", "static int vb_slot_wait(unsigned i)"),
    ("the slot submit", "static void vb_slot_submit(void)"),
    ("the drain", "static void vb_drain(void)"),
    ("the stream reservation", "static void vb_reserve(VkDeviceSize bytes, uint32_t sets)"),
    ("the stream slice", "static uint8_t *vb_stream(VkDeviceSize bytes, VkBuffer *buf, "
                         "VkDeviceSize *off)"),
    ("the host-visible buffer maker", "static int vb_host_buffer(VkDeviceSize size, "
                                      "VkBufferUsageFlags usage, int cached, VkBuffer *buf, "
                                      "VkDeviceMemory *mem, uint8_t **ptr)"),
    ("the device-local buffer maker", "static int vb_device_buffer(VkDeviceSize size, "
                                      "VkBufferUsageFlags usage, VkBuffer *buf, "
                                      "VkDeviceMemory *mem)"),
    ("the public readback stats accessor",
     "void nv2a_vk_backend_readback_stats(Nv2aVkReadbackStats *out)"),
    # The four boundaries the case drives. Their signatures are the case's whole interface to
    # the back end and are unchanged by the repair, so they are required, not probed.
    ("the flip", "static void vb_flip(void)"),
    ("the target load", "static void vb_target_load(VbTarget *t)"),
    ("the target lookup", "static VbTarget *vb_target_for(const Nv2aSurface *s)"),
    ("the texture view", "static VkImageView vb_texture(const Nv2aTexture *tx, VbTarget *out, "
                         "VkImageLayout *layout, float *inv_w, float *inv_h)"),
]

FLIP_START = "static void vb_flip(void)"
FLIP_END = "static VKAPI_ATTR VkBool32 VKAPI_CALL vb_debug("
TARGET_FOR_START = "static VbTarget *vb_target_for(const Nv2aSurface *s)"
TARGET_FOR_END = "static void vb_target_mark(VbTarget *t, const VkRect2D *r)"
TEXTURE_START = ("static VkImageView vb_texture(const Nv2aTexture *tx, VbTarget *out, "
                 "VkImageLayout *layout, float *inv_w, float *inv_h)")
TEXTURE_END = "static uint32_t vb_sampler_mode(uint32_t addr)"


def flat(text):
    return re.sub(r"\s+", " ", text)


def body(f, start, end):
    """`start`'s definition up to the definition that follows it, in the flattened text, or
    None if either is gone. Flattened so a declaration reflowed over several lines still
    delimits the body."""
    i = f.find(flat(start))
    if i < 0:
        return None
    j = f.find(flat(end), i + len(flat(start)))
    if j < 0:
        return None
    return f[i:j]


def probe(text):
    """The repair's seam, as markers the test can compile against and print."""
    f = flat(text)
    flip = body(f, FLIP_START, FLIP_END)
    target_for = body(f, TARGET_FOR_START, TARGET_FOR_END)
    texture = body(f, TEXTURE_START, TEXTURE_END)

    def before(a, b, text_):
        """Both present, and `a` before `b`."""
        if text_ is None:
            return 0
        i, j = text_.find(a), text_.find(b)
        return int(i >= 0 and j >= 0 and i < j)

    return {
        # The flush reports its outcome instead of leaving the caller to guess.
        "VB_RB_CALLER_FLUSH_OUTCOME":
            int("static int vb_targets_flush(VbTarget **ts, unsigned n)" in f),
        # The one bounded retry, and the ring refresh it is built on.
        "VB_RB_CALLER_HAS_RECOVER":
            int("static int vb_targets_flush_recover(VbTarget **ts, unsigned n)" in f
                and "static int vb_readback_recover(void)" in f),
        # The coherence stop, and the "left unpublished" counter behind it.
        "VB_RB_CALLER_HAS_STOP": int("static void vb_stop(const char *why)" in f),
        "VB_RB_CALLER_COUNTS_INCOMPLETE": int("g.st.readback_incomplete++" in f),
        # The read-only query the executor's CPU consumers are gated on.
        "VB_RB_CALLER_HAS_STOPPED": int("static int vb_stopped(void)" in f),
        # The flip publishes (and, failing that, stops) BEFORE it marks anything stale or
        # advances the frame -- the ordering the whole defect is about.
        "VB_RB_CALLER_FLIP_PUBLISHES_FIRST":
            before("vb_targets_flush_recover", "ts[i]->stale = 1", flip),
        "VB_RB_CALLER_FLIP_STOPS_BEFORE_STALE":
            before("vb_stop(", "ts[i]->stale = 1", flip),
        # A replacement or an eviction publishes the outgoing target through the boundary
        # entry point before it releases it.
        "VB_RB_CALLER_TARGET_FOR_PUBLISHES_BEFORE_RELEASE":
            before("vb_targets_flush_recover", "vb_target_release(t)", target_for),
        "VB_RB_CALLER_TARGET_FOR_STOPS":
            int(target_for is not None and "vb_stop(" in target_for),
        # The overlap loop publishes the target through the boundary entry point (and stops on
        # failure) before anything is decoded from guest memory.
        "VB_RB_CALLER_TEXTURE_OVERLAP_PUBLISHES":
            int(texture is not None and "vb_targets_flush_recover(&t, 1)" in texture),
        "VB_RB_CALLER_TEXTURE_OVERLAP_STOPS":
            int(texture is not None and "vb_stop(" in texture),
        # The stop is published to the executor: the back end's own table carries the query.
        "VB_RB_CALLER_REGISTERS_STOPPED":
            int("{ vb_clear, vb_draw, vb_flip, vb_draw_program, vb_stopped }" in f),
    }


def header_markers(header_text):
    """What the interface header says, as markers. Absent is reported, never fatal: the
    backend source is what the case is built against, and an old header is a finding, not a
    reason to skip."""
    if header_text is None:
        return {"VB_RB_CALLER_HAS_INCOMPLETE": 0}
    f = flat(header_text)
    return {
        # The reports' "left unpublished" counter: the boundary's own accounting of the
        # targets a stop left behind.
        "VB_RB_CALLER_HAS_INCOMPLETE": int("uint64_t incomplete;" in f),
    }


def c_string(text):
    out = text.replace("\\", "\\\\").replace('"', '\\"')
    return '"%s"' % out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help="the toolkit's src/video/nv2a_vk_backend.c")
    ap.add_argument("--header", required=True, help="the seam header to write")
    ap.add_argument("--manifest", required=True, help="the seam manifest to write")
    ap.add_argument("--backend-header", default=None,
                    help="the toolkit's src/video/nv2a_vk_backend.h, probed for its markers")
    args = ap.parse_args()

    path = Path(args.source)
    if not path.is_file():
        print("vk-readback-caller-gate: no such source: %s" % path, file=sys.stderr)
        return 2
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    f = flat(text)

    missing = [name for name, literal in REQUIRED if flat(literal) not in f]
    if missing:
        print("vk-readback-caller-gate: %s no longer provides what the readback caller gate "
              "binds to:" % path, file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The test drives the production bodies of this file; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    backend_header = None
    if args.backend_header:
        hp = Path(args.backend_header)
        if hp.is_file():
            backend_header = hp.read_bytes().decode("utf-8", errors="replace")

    seam = probe(text)
    seam.update(header_markers(backend_header))
    digest = hashlib.sha256(raw).hexdigest()
    source = str(path.resolve())
    hdr = str(Path(args.backend_header).resolve()) if args.backend_header else "(none given)"

    header = ["/* Generated by scripts/vk-readback-caller-gate.py from the toolkit's Vulkan back",
              " * end. Do not edit: the file is rebuilt from the source every time the source",
              " * changes.",
              " *",
              " * The readback caller gate compiles that source into its own translation unit, so",
              " * the boundaries it drives are the production ones. These markers describe the",
              " * repair's seam in the file the test was built against. The test PRINTS them:",
              " * the properties below the seam are what decides, and this case is built against",
              " * the pre-fix source too. */",
              "#ifndef VK_READBACK_CALLER_GATE_SEAM_H",
              "#define VK_READBACK_CALLER_GATE_SEAM_H",
              "",
              "#define VB_RB_CALLER_SOURCE %s" % c_string(source),
              "#define VB_RB_CALLER_SOURCE_SHA256 %s" % c_string(digest),
              "#define VB_RB_CALLER_SOURCE_BYTES %d" % len(raw),
              "#define VB_RB_CALLER_BACKEND_HEADER %s" % c_string(hdr),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_READBACK_CALLER_GATE_SEAM_H */")

    manifest = ["vk-readback-caller-gate: the seam of %s" % source,
                "  sha256  %s" % digest,
                "  bytes   %d" % len(raw),
                "  interface header: %s" % hdr,
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                ""]
    for name in sorted(seam):
        manifest.append("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("A marker marked ABSENT is what the pre-fix source reads as. The case is built")
    manifest.append("against the pre-fix source on purpose, so the markers are printed, never")
    manifest.append("asserted: the properties decide, and the pre-fix source is expected to fail")
    manifest.append("the coherence ones (a failed publication still advancing the frame, marking")
    manifest.append("every target stale, and letting the next load or texture read the guest bytes")
    manifest.append("that publication never updated).")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-readback-caller-gate: probed %s (sha256 %s)" % (path, digest[:16]))
    for name in sorted(seam):
        print("  %-46s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
