"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_readback_fault_gate.c binds to.

The readback fault gate compiles the toolkit's own src/video/nv2a_vk_backend.c into its
translation unit, so the functions it drives -- vb_targets_flush, vb_readback_slice,
vb_readback_reserve, the recording path behind them -- are the production ones, verbatim.
This script is the other half of that: it reads the same file AT BUILD TIME and refuses the
build when what the test needs is not there.

  * Every REQUIRED anchor is a definition, a struct field, a global or a #define the test's
    C code names. One that is gone is a hard error (exit 2), which fails the custom command
    and the build. It never skips and never warns-and-continues: a moved or renamed seam must
    break the build, not silently turn the case into something else.

  * The REPAIR's seam is probed as a set of markers rather than required, and that
    distinction is deliberate: the pre-fix source must still BUILD, because the case has to
    run against it and report the defect per property. Each marker is a #define in the
    generated header; the test asserts the behaviour the marker stands for, and where a
    marker is absent it reports a failed check naming it instead of compiling a reference to
    a variable that does not exist.

The markers are read out of vb_targets_flush's own body (sliced out of the file by its
definition and the definition that follows it), because "the batch's skip comes before the
batch's submit" is a statement about that body and nothing else.

Outputs (both declared to CMake as one custom command):
  --header    vk_readback_fault_gate_seam.h   the markers + the source's identity, #included
                                              by the test
  --manifest  vk_readback_fault_gate_seam.txt the same verdict in prose, kept next to the build
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

# Every literal below is normalised for whitespace before matching, so reflowing a
# declaration's parameters does not break the probe -- but its name, its parameter list,
# its field names and the values the test computes with must still be there.
REQUIRED = [
    ("the Vulkan branch of the file", "#if !defined(XBR_VK_BACKEND)"),
    ("the publication staging cap", "#define VB_READBACK_MAX (96u << 20)"),
    ("the target bound", "#define VB_MAX_TARGETS 16"),
    ("the ring size", "#define VB_SLOTS 3"),
    ("the target struct", "} VbTarget;"),
    ("the target's guest surface fields", "uint32_t va, pitch, bpp, w, h;"),
    ("the target's image and view", "VbImage im, copy;"),
    ("the target's dirty flag", "int dirty;"),
    ("the target's dirty rectangle", "uint32_t dx0, dy0, dx1, dy1;"),
    ("the slot struct", "} VbSlot;"),
    ("the slot's command buffer field", "VkCommandBuffer cmd;"),
    ("the slot's pending bit", "int pending;"),
    ("the slot's recording bit", "int recording;"),
    ("the slot's stream offset field", "VkDeviceSize stream_off;"),
    ("the back end's state object", "} g;"),
    ("the device handle", "VkDevice dev;"),
    ("the queue handle", "VkQueue queue;"),
    ("the memory properties", "VkPhysicalDeviceMemoryProperties mp;"),
    ("the current slot", "unsigned cur;"),
    ("the pending-slot count", "unsigned npending;"),
    ("the pack push block", "typedef struct { uint32_t rect[4], dst[4]; } VbPackPush;"),
    ("the host-visible staging buffer field", "VkBuffer rb_buf;"),
    ("the device-local pack buffer field", "VkBuffer pack_buf;"),
    ("the staging pair's memory fields", "VbAlloc pack_mem;"),
    ("the staging pair's mapped pointer", "uint8_t *rb_ptr;"),
    ("the staging pair's capacity", "VkDeviceSize rb_cap;"),
    ("the pack set layout field", "VkDescriptorSetLayout dsl_pack;"),
    ("the pack push layout field", "VkPipelineLayout layout_pack;"),
    ("the pack pipeline field", "VkPipeline pipe_pack;"),
    ("the publication counters", "uint64_t readback_batches, readback_packs, readback_pixels, "
                                 "readback_copies;"),
    ("the publication byte and time counters", "uint64_t readback_bytes, readback_wait_ns, "
                                               "readback_publish_ns;"),
    ("the published-target counter", "g.st.readbacks++"),
    ("vb_readback_slice", "static VkDeviceSize vb_readback_slice(const VbTarget *t, "
                          "uint32_t *row_stride)"),
    ("vb_readback_reserve", "static int vb_readback_reserve(VkDeviceSize bytes)"),
    ("vb_targets_flush", "static int vb_targets_flush(VbTarget **ts, unsigned n)"),
    ("vb_transfer_cmd", "static VkCommandBuffer vb_transfer_cmd(void)"),
    ("vb_cmd", "static VkCommandBuffer vb_cmd(void)"),
    ("vb_barrier", "static void vb_barrier(void)"),
    ("vb_slot_wait", "static int vb_slot_wait(unsigned i)"),
    ("vb_slot_submit", "static void vb_slot_submit(void)"),
    ("vb_drain", "static void vb_drain(void)"),
    ("vb_host_buffer", "static int vb_host_buffer(VkDeviceSize size, VkBufferUsageFlags usage, "
                       "int cached, VkBuffer *buf, VbAlloc *mem, uint8_t **ptr)"),
    ("vb_device_buffer", "static int vb_device_buffer(VkDeviceSize size, "
                         "VkBufferUsageFlags usage, VkBuffer *buf, VbAlloc *mem)"),
    ("the public readback stats accessor",
     "void nv2a_vk_backend_readback_stats(Nv2aVkReadbackStats *out)"),
]

# The repair adds a return value to the flush and removes the one-target wrapper that used to
# follow it, so the slice's delimiters are the flush's new signature and the next definition that
# still exists. The slice therefore also spans the three new seam functions between them
# (vb_stop, vb_readback_recover, vb_targets_flush_recover). That is deliberate and the markers
# below are unaffected: none of the six publication counters appears in those three (the seam
# counts readback_incomplete instead), and the first `vb_slot_submit();` in the slice is still
# the publication boundary inside the flush -- the recovery's own submit comes after it. With
# these two strings the probe still asserts exactly what it always did, and it does not break
# again if a definition is inserted between them.
FLUSH_START = "static int vb_targets_flush(VbTarget **ts, unsigned n)"
FLUSH_END = "static void vb_target_load(VbTarget *t)"


def flat(text):
    return re.sub(r"\s+", " ", text)


def flush_body(text):
    """vb_targets_flush's own body, from its definition to the definition that follows it."""
    start = text.find(FLUSH_START)
    end = text.find(FLUSH_END, start + len(FLUSH_START)) if start >= 0 else -1
    if start < 0 or end < 0:
        return None
    return text[start:end]


def probe(source_text):
    """The repair's seam, as markers the test can compile against."""
    f = flat(source_text)
    body = flush_body(source_text)
    if body is None:
        # The anchors above already fail the build in this case; keep the markers at 0 so a
        # header is still written and the failure names the anchors.
        return {"VB_RB_GATE_HAS_BATCH_FLAG": 0,
                "VB_RB_GATE_HAS_BATCH_SKIP": 0,
                "VB_RB_GATE_COUNTERS_AFTER_SUBMIT": 0}
    bf = flat(body)
    submit = bf.find("vb_slot_submit();")
    skip = bf.find("if (!batch_ok)")
    # The publish counters: every one of them is incremented inside the publish, which is
    # after the batch's submit -- that is what makes a batch that never submitted leave them
    # all unchanged.
    counters = ["g.st.readbacks++", "g.st.readback_batches++", "g.st.readback_packs++",
                "g.st.readback_pixels +=", "g.st.readback_copies++", "g.st.readback_bytes +="]
    return {
        # The batch-local success flag the fix introduced.
        "VB_RB_GATE_HAS_BATCH_FLAG": int("int batch_ok = 1;" in bf),
        # The whole-batch skip: the flag is cleared in the allocation-failure branch, and the
        # check on it sits between the record loop and the batch's submit.
        "VB_RB_GATE_HAS_BATCH_SKIP": int("int batch_ok = 1;" in bf
                                         and "batch_ok = 0;" in bf
                                         and skip >= 0 and submit > skip),
        # Counted in the publish only: every publication counter's increment is after the
        # submit, and each of them appears exactly once in the whole file.
        "VB_RB_GATE_COUNTERS_AFTER_SUBMIT": int(
            submit > 0
            and all(bf.find(name) > submit for name in counters)
            and all(source_text.count(name) == 1 for name in counters)),
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
        print("vk-readback-fault-gate: no such source: %s" % path, file=sys.stderr)
        return 2
    raw = path.read_bytes()
    text = raw.decode("utf-8", errors="replace")
    f = flat(text)

    missing = [name for name, literal in REQUIRED if flat(literal) not in f]
    if missing:
        print("vk-readback-fault-gate: %s no longer provides what the readback fault gate "
              "binds to:" % path, file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The test drives the production bodies of this file; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    seam = probe(text)
    digest = hashlib.sha256(raw).hexdigest()
    source = str(path.resolve())

    header = ["/* Generated by scripts/vk-readback-fault-gate.py from the toolkit's Vulkan back end.",
              " * Do not edit: the file is rebuilt from the source every time the source changes.",
              " *",
              " * The readback fault gate compiles that source into its own translation unit, so",
              " * the bodies it drives are the production ones. These markers describe the",
              " * repair's seam in the file the test was built against; where one is 0, the test",
              " * reports a failed check naming it (it never skips). */",
              "#ifndef VK_READBACK_FAULT_GATE_SEAM_H",
              "#define VK_READBACK_FAULT_GATE_SEAM_H",
              "",
              "#define VB_RB_GATE_SOURCE %s" % c_string(source),
              "#define VB_RB_GATE_SOURCE_SHA256 %s" % c_string(digest),
              "#define VB_RB_GATE_SOURCE_BYTES %d" % len(raw),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_READBACK_FAULT_GATE_SEAM_H */")

    manifest = ["vk-readback-fault-gate: the seam of %s" % source,
                "  sha256  %s" % digest,
                "  bytes   %d" % len(raw),
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                ""]
    for name in sorted(seam):
        manifest.append("  %-36s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("A marker marked ABSENT is the defect this case exists for: a batch whose pack")
    manifest.append("descriptor set could not be allocated still submits and still publishes, so its")
    manifest.append("staged targets are marked clean from bytes nothing wrote.")
    manifest.append("")
    manifest.append("The publish counters must be incremented after the batch's submit; that is")
    manifest.append("what makes a batch that never reached it leave them all unchanged.")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-readback-fault-gate: probed %s (sha256 %s)" % (path, digest[:16]))
    for name in sorted(seam):
        print("  %-36s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
