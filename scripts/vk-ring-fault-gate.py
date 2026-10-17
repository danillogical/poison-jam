"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_ring_fault_gate.c binds to.

The fault-gate test compiles the toolkit's own src/video/nv2a_vk_backend.c into its
translation unit, so the functions it drives are the production ones -- there is no copy
of the algorithm to drift. This script is the other half of that: it reads the same file
AT BUILD TIME and refuses the build when what the test needs is not there.

  * Every REQUIRED anchor is a definition, a struct or a #define the test's C code names
    (or the numbers it uses) -- the ring size, the stream size and alignment, the slot
    struct and its fields, the eight functions it calls, and the Vulkan branch itself.
    One that is gone is a hard error (exit 2), which fails the custom command and the
    build. It never skips and never warns-and-continues: a moved or renamed seam must
    break the build, not silently turn the case into something else.

  * The REPAIR's seam is probed as a set of markers rather than required, and that
    distinction is deliberate: the pre-fix source must still BUILD, so the case can run
    against it and report the defect per property. Each marker is a #define in the
    generated header; the test asserts the behaviour the marker stands for, and where a
    marker is absent it reports a failed check naming it instead of compiling a
    reference to a field that does not exist.

Outputs (both declared to CMake as one custom command):
  --header    vk_ring_fault_gate_seam.h   the markers + the source's identity, #included
                                          by the test
  --manifest  vk_ring_fault_gate_seam.txt the same verdict in prose, kept next to the build
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

# Every literal below is normalised for whitespace before matching, so reflowing a
# declaration's parameters does not break the probe -- but its name, its parameter list
# and the values the test computes with must still be there.
REQUIRED = [
    ("the Vulkan branch of the file", "#if !defined(XBR_VK_BACKEND)"),
    ("the ring size", "#define VB_SLOTS 3"),
    ("the per-slot stream size", "#define VB_SLOT_STREAM (16u << 20)"),
    ("the stream alignment", "#define VB_STREAM_ALIGN 256u"),
    ("the per-slot descriptor-set budget", "#define VB_SLOT_SETS 2048"),
    ("the slot struct", "} VbSlot;"),
    ("the slot's command buffer field", "VkCommandBuffer cmd;"),
    ("the slot's fence field", "VkFence fence;"),
    ("the slot's pending bit", "int pending;"),
    ("the slot's recording bit", "int recording;"),
    ("the slot's descriptor pool field", "VkDescriptorPool dpool;"),
    ("the slot's used-set count field", "uint32_t sets_used;"),
    ("the slot's stream buffer field", "VkBuffer stream;"),
    ("the slot's mapped stream field", "uint8_t *stream_ptr;"),
    ("the slot's stream offset field", "VkDeviceSize stream_off;"),
    ("the back end's state object", "} g;"),
    ("vb_reserve", "static void vb_reserve(VkDeviceSize bytes, uint32_t sets)"),
    ("vb_stream", "static uint8_t *vb_stream(VkDeviceSize bytes, VkBuffer *buf, VkDeviceSize *off)"),
    ("vb_slot_wait", "static int vb_slot_wait(unsigned i)"),
    ("vb_slot_submit", "static void vb_slot_submit(void)"),
    ("vb_slot_advance", "static void vb_slot_advance(void)"),
    ("vb_cmd", "static VkCommandBuffer vb_cmd(void)"),
    ("vb_transfer_cmd", "static VkCommandBuffer vb_transfer_cmd(void)"),
    ("vb_barrier", "static void vb_barrier(void)"),
    ("vb_image_dep", "static void vb_image_dep(VkImage img, VkImageAspectFlags aspect, "
                     "VkImageLayout from, VkImageLayout to, VkPipelineStageFlags src_stage, "
                     "VkAccessFlags src_access, VkPipelineStageFlags dst_stage, "
                     "VkAccessFlags dst_access)"),
    ("vb_image_create", "static int vb_image_create(VbImage *im, uint32_t w, uint32_t h, "
                        "VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)"),
    ("vb_target_load", "static void vb_target_load(VbTarget *t)"),
    ("vb_tex_upload", "static void vb_tex_upload(VbTex *e, const Nv2aTexture *tx)"),
]


def flat(text):
    return re.sub(r"\s+", " ", text)


def probe(source_text):
    """The repair's seam, as markers the test can compile against."""
    f = flat(source_text)
    return {
        # The early return the plan N4.1 review asked for, in each upload path: it must sit
        # between the vb_reserve and the first write into what that reserve handed back.
        "VB_GATE_HAS_TARGET_LOAD_GUARD": int("vb_reserve((VkDeviceSize)t->w * t->h * 4, 0); "
                                             "if (!g.ok)" in f),
        "VB_GATE_HAS_TEX_UPLOAD_GUARD": int("vb_reserve(bytes, 0); if (!g.ok)" in f),
        # The write-once latch, armed by every fatal failure the ring can observe.
        "VB_GATE_HAS_FAILED_LATCH": int("int failed;" in f
                                        and "if (g.failed)" in f
                                        and source_text.count("g.failed = 1") >= 2),
        # The discard buffer, allocated and begun at install, handed back when no recording
        # is possible.
        "VB_GATE_HAS_DEAD_BUFFER": int("VkCommandBuffer dead;" in f
                                       and "&g.dead" in f
                                       and source_text.count("return g.dead") >= 2),
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
        print("vk-ring-fault-gate: no such source: %s" % path, file=sys.stderr)
        return 2
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    f = flat(text)

    missing = [name for name, literal in REQUIRED if flat(literal) not in f]
    if missing:
        print("vk-ring-fault-gate: %s no longer provides what the fault-gate test binds to:"
              % path, file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The test drives the production bodies of this file; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    seam = probe(text)
    digest = hashlib.sha256(raw).hexdigest()
    source = str(path.resolve())

    header = ["/* Generated by scripts/vk-ring-fault-gate.py from the toolkit's Vulkan back end.",
              " * Do not edit: the file is rebuilt from the source every time the source changes.",
              " *",
              " * The fault-gate test compiles that source into its own translation unit, so the",
              " * bodies it drives are the production ones.  These markers describe the repair's",
              " * seam in the file the test was built against; where one is 0, the test reports a",
              " * failed check naming it (it never skips). */",
              "#ifndef VK_RING_FAULT_GATE_SEAM_H",
              "#define VK_RING_FAULT_GATE_SEAM_H",
              "",
              "#define VB_GATE_SOURCE %s" % c_string(source),
              "#define VB_GATE_SOURCE_SHA256 %s" % c_string(digest),
              "#define VB_GATE_SOURCE_BYTES %d" % len(raw),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_RING_FAULT_GATE_SEAM_H */")

    manifest = ["vk-ring-fault-gate: the seam of %s" % source,
                "  sha256  %s" % digest,
                "  bytes   %d" % len(raw),
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                ""]
    for name in sorted(seam):
        manifest.append("  %-32s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("A guard marked ABSENT is the plan N4.1 defect: the upload path writes guest")
    manifest.append("pixels into a slot whose fence wait failed, before it checks g.ok.")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-ring-fault-gate: probed %s (sha256 %s)" % (path, digest[:16]))
    for name in sorted(seam):
        print("  %-32s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
