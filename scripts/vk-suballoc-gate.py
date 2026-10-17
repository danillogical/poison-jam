"""Probe the toolkit's Vulkan back end for the seam tests/test_vk_suballoc_gate.c binds to.

The sub-allocation gate compiles the toolkit's own src/video/nv2a_vk_backend.c into its
translation unit, so the allocator it drives -- vb_mem_take, vb_mem_give, vb_pool_gc,
vb_mem_flush, vb_mem_invalidate, vb_mem_requirements, and the resource builders that use
them -- is the production one; there is no copy of the algorithm to drift. This script is
the other half of that: it reads the same source AT BUILD TIME and refuses the build when
what the test binds to is not there.

  * Every REQUIRED anchor is a type, a field, a #define, or a function signature the test's
    C code names (or the values it computes with) -- the six pool constants, the VbFreeRange
    / VbBlock / VbPool / VbAlloc fields, the pool array in `g`, the cached non-coherent atom
    size, the memory API's eight signatures, the public counter struct's fields (probed in
    the toolkit's own nv2a_vk_backend.h), and the optional Requirements2 / flush /
    invalidate entry points.  One that is gone is a hard error (exit 2), which fails the
    custom command and the build.  It never skips and never warns-and-continues: a moved or
    renamed seam must break the build, not silently turn the case into something else.

    The allocator's BEHAVIOUR is probed as a set of markers rather than required.  The
    markers in the generated header name the parts of the design the gate's checks decide
    (a free list, the parked-release list, the bounded park list's refusal that retains the
    range, the dedicated pNext, the optional Requirements2 lookups, the atom-size widening,
    the write-once reuse and sharing latches, block retirement, flush).  A marker that is 0 is
    reported by the property that needs it, as a FAILED CHECK naming it, not as a skip: the
    gate's verdict stays 0 or 1.

Outputs (both declared to CMake as one custom command):
  --header    vk_suballoc_gate_seam.h    the markers + both sources' identity, #included
                                         by the test
  --manifest  vk_suballoc_gate_seam.txt  the same verdict in prose, kept next to the build
"""
import argparse
import hashlib
import re
import sys
from pathlib import Path

# Every literal below is normalised for whitespace before matching, so reflowing a
# declaration's parameters does not break the probe -- but its name, its type and the values
# the test computes with must still be there.  "c" anchors are looked for in the toolkit's
# nv2a_vk_backend.c, "h" anchors in nv2a_vk_backend.h.
REQUIRED = [
    # The Vulkan branch, and the constants the fixture sizes its requests from.
    ("c", "the Vulkan branch of the file", "#if !defined(XBR_VK_BACKEND)"),
    ("c", "the pool count", "#define VB_POOLS 6"),
    ("c", "the per-pool block budget", "#define VB_POOL_BLOCKS 6"),
    ("c", "the per-block free-range budget", "#define VB_POOL_FREE_MAX 32"),
    ("c", "the minimum block size", "#define VB_POOL_MIN_BLOCK (1u << 20)"),
    ("c", "the largest pooled range", "#define VB_POOL_MAX_BLOCK (64u << 20)"),
    ("c", "the per-pool byte reserve", "#define VB_POOL_RESERVE (256u << 20)"),
    ("c", "the parked-release budget", "#define VB_POOL_PENDING 64"),
    ("c", "the pooling alignment floor", "#define VB_POOL_ALIGN 256u"),
    ("c", "the block-sharing budget", "#define VB_POOL_SHARE_MAX (8u << 20)"),
    ("c", "the empty-block keep policy", "#define VB_POOL_KEEP_EMPTY (4u << 20)"),
    # The allocator's types.
    ("c", "the free-range struct", "VkDeviceSize off, size; } VbFreeRange;"),
    ("c", "the block struct", "} VbBlock;"),
    ("c", "the block's allocation", "VkDeviceMemory mem;"),
    ("c", "the block's bump cursor", "VkDeviceSize bump;"),
    ("c", "the block's padding counter", "VkDeviceSize padding;"),
    ("c", "the block's single mapping", "uint8_t *map;"),
    ("c", "the block's free list", "VbFreeRange free_list[VB_POOL_FREE_MAX];"),
    ("c", "the free-range count", "unsigned nfree;"),
    ("c", "the pool struct", "} VbPool;"),
    ("c", "the pool's memory type", "uint32_t type_index;"),
    ("c", "the pool's kind", "int nonlinear;"),
    ("c", "the pool's blocks", "VbBlock blocks[VB_POOL_BLOCKS];"),
    ("c", "the pool's block count", "unsigned nblocks;"),
    ("c", "the pool's reserved-byte counter", "uint64_t reserved"),
    ("c", "the pool's range counters", "uint64_t ranges, reuse_hits;"),
    ("c", "the allocation struct", "} VbAlloc;"),
    ("c", "the allocation's offset", "VkDeviceSize off;"),
    ("c", "the allocation's pool index", "int pool;"),
    ("c", "the allocation's block index", "int block;"),
    ("c", "the allocation's coherence", "int coherent;"),
    ("c", "the allocation's host visibility", "int host_visible;"),
    ("c", "the pool array in the back end's state", "VbPool pools[VB_POOLS];"),
    ("c", "the cached non-coherent atom size", "VkDeviceSize atom;"),
    ("c", "the parked-release list", "static VbAlloc s_pool_pending[VB_POOL_PENDING];"),
    # The memory API the gate calls by name.
    ("c", "vb_mem_take", "static int vb_mem_take(const VkMemoryRequirements *mr, "
                         "VkMemoryPropertyFlags want, VkMemoryPropertyFlags fallback, "
                         "int nonlinear, int requires_dedicated, int prefers_dedicated, "
                         "const VkImage *dedicated_image, const VkBuffer *dedicated_buffer, "
                         "VbAlloc *out)"),
    ("c", "vb_mem_give", "static void vb_mem_give(VbAlloc *a)"),
    ("c", "vb_pool_gc", "static void vb_pool_gc(void)"),
    ("c", "vb_mem_flush", "static int vb_mem_flush(const VbAlloc *a, VkDeviceSize off, "
                          "VkDeviceSize size)"),
    ("c", "vb_mem_invalidate", "static int vb_mem_invalidate(const VbAlloc *a, VkDeviceSize off, "
                               "VkDeviceSize size)"),
    ("c", "vb_mem_requirements", "static int vb_mem_requirements(VkImage img, VkBuffer buf, "
                                 "VkMemoryRequirements *mr, int *requires_dedicated, "
                                 "int *prefers_dedicated)"),
    ("c", "vb_host_buffer", "static int vb_host_buffer(VkDeviceSize size, "
                            "VkBufferUsageFlags usage, int cached, VkBuffer *buf, "
                            "VbAlloc *mem, uint8_t **ptr)"),
    ("c", "vb_device_buffer", "static int vb_device_buffer(VkDeviceSize size, "
                              "VkBufferUsageFlags usage, VkBuffer *buf, VbAlloc *mem)"),
    ("c", "vb_image_create", "static int vb_image_create(VbImage *im, uint32_t w, uint32_t h, "
                             "VkFormat fmt, VkImageUsageFlags usage, VkImageAspectFlags aspect)"),
    ("c", "vb_discard", "static void vb_discard(VbGarbage e)"),
    # The optional entry points the allocator resolves through vkGetDeviceProcAddr.
    ("c", "vkGetImageMemoryRequirements2", "vkGetImageMemoryRequirements2"),
    ("c", "vkGetBufferMemoryRequirements2", "vkGetBufferMemoryRequirements2"),
    ("c", "vkFlushMappedMemoryRanges", "vkFlushMappedMemoryRanges"),
    ("c", "vkInvalidateMappedMemoryRanges", "vkInvalidateMappedMemoryRanges"),
    ("c", "vkUnmapMemory", "vkUnmapMemory"),
    ("c", "the dedicated pNext", "VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO"),
    ("c", "the limits' atom size", "nonCoherentAtomSize"),
    # The public counters the gate decides from, and the accessor that publishes them.
    ("c", "the memory stats accessor",
     "void nv2a_vk_backend_memory_stats(Nv2aVkMemoryStats *out)"),
    ("h", "the pool counter", "uint64_t pools;"),
    ("h", "the block counter", "uint64_t blocks;"),
    ("h", "the retired-block counter", "uint64_t blocks_retired;"),
    ("h", "the range counter", "uint64_t ranges;"),
    ("h", "the reuse counter", "uint64_t reuse_hits;"),
    ("h", "the dedicated counter", "uint64_t dedicated;"),
    ("h", "the dedicated reason counters",
     "uint64_t dedicated_required, dedicated_prefer, dedicated_oversize;"),
    ("h", "the other dedicated reason counters",
     "uint64_t dedicated_noroom, dedicated_nohost;"),
    ("h", "the allocation-call counter", "uint64_t alloc_calls;"),
    ("h", "the release counter", "uint64_t released;"),
    ("h", "the park counters", "uint64_t parked, parked_peak, parked_lost;"),
    ("h", "the byte counters",
     "uint64_t reserved_bytes, used_bytes, free_bytes, largest_free, padding_bytes;"),
    ("h", "the metadata counter", "uint64_t metadata_bytes;"),
    ("h", "the dedicated byte counters", "uint64_t dedicated_bytes, peak_used;"),
    ("h", "the failure counters",
     "uint64_t alloc_failures, bind_failures, map_failures, rollbacks, fallbacks;"),
    ("h", "the write-once sharing latch",
     "uint32_t first_shared_block, first_shared_block_kind;"),
    ("h", "the write-once reuse latch", "uint64_t first_reuse_off, first_reuse_block;"),
]


def flat(text):
    return re.sub(r"\s+", " ", text)


def probe(c_text, h_text):
    """The allocator's design, as markers the test can compile against."""
    f = flat(c_text)
    return {
        # A block keeps its free ranges, so a released range can be handed out again.
        "VB_GATE_HAS_FREE_LIST": int("free_list" in f and "nfree" in f),
        # A release while a recording is open or a submission is pending is parked, not
        # handed out (the bounded list of the interface's rule 6).
        "VB_GATE_HAS_PARK": int("s_pool_pending" in f and "parked_lost" in f
                                and "parked_peak" in c_text),
        # The bounded park list's admission failure is a REFUSAL, not a drop: the range stays
        # counted live in its block and reaches no free list (nothing freed, nothing handed
        # out), and a never-referenced rollback release is handed a path that releases the
        # range at once (vb_mem_give_now) instead of parking it. The refusal is reached
        # through a by-value vb_garbage_free entry the drain drops, so no VbAlloc survives
        # it. ABSENT means the source still clears the caller's handle and drops the range.
        "VB_GATE_HAS_REFUSE_PARK": int("vb_mem_give_now" in f and "s_pool_pending" in f),
        # A dedicated allocation carries its VkMemoryDedicatedAllocateInfo in pNext.
        "VB_GATE_HAS_DEDICATED_PNEXT":
            int(c_text.count("VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO") >= 1),
        # The two optional *Requirements2 lookups, whose absence means "pooled".
        "VB_GATE_HAS_REQS2": int("vkGetImageMemoryRequirements2" in f
                                 and "vkGetBufferMemoryRequirements2" in f),
        # The non-coherent flush/invalidate widening to whole atoms.
        "VB_GATE_HAS_ATOM_MATH": int("nonCoherentAtomSize" in f
                                     and c_text.count("g.atom") >= 1),
        # The write-once latches the gate reports as evidence.
        "VB_GATE_HAS_SHARED_LATCH": int("first_shared_block" in f),
        "VB_GATE_HAS_REUSE_LATCH": int("first_reuse_off" in f),
        # Block retirement, and the empty-block keep policy.
        "VB_GATE_HAS_RETIRE": int("blocks_retired" in f and "VB_POOL_KEEP_EMPTY" in f),
        # One mapping per host-visible block, handed out as map + off.
        "VB_GATE_HAS_BLOCK_MAP": int("->map" in f and "VK_WHOLE_SIZE" in f),
        # The host-visible pool's coherence policy: a request that cannot get coherence is
        # never pooled (the interface's dedicated_nohost reason).
        "VB_GATE_HAS_NOHOST_REASON": int("dedicated_nohost" in f),
        # The write-once failure latch the fixture clears between its injected-failure
        # scenarios (it is production's, and the fixture never sets it).
        "VB_GATE_HAS_FAILED_LATCH": int("int failed;" in f and "if (g.failed)" in f),
    }


def c_string(text):
    out = text.replace("\\", "\\\\").replace('"', '\\"')
    return '"%s"' % out


def load(path):
    raw = Path(path).read_bytes()
    return raw, raw.decode("utf-8", errors="replace")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--source", required=True, help="the toolkit's src/video/nv2a_vk_backend.c")
    ap.add_argument("--decls", required=True, help="the toolkit's src/video/nv2a_vk_backend.h")
    ap.add_argument("--header", required=True, help="the seam header to write")
    ap.add_argument("--manifest", required=True, help="the seam manifest to write")
    args = ap.parse_args()

    c_path, h_path = Path(args.source), Path(args.decls)
    if not c_path.is_file():
        print("vk-suballoc-gate: no such source: %s" % c_path, file=sys.stderr)
        return 2
    if not h_path.is_file():
        print("vk-suballoc-gate: no such header: %s" % h_path, file=sys.stderr)
        return 2
    c_raw, c_text = load(c_path)
    h_raw, h_text = load(h_path)
    flat_c, flat_h = flat(c_text), flat(h_text)

    missing = []
    for where, name, literal in REQUIRED:
        hay = flat_c if where == "c" else flat_h
        if flat(literal) not in hay:
            missing.append("%s (%s)" % (name, "nv2a_vk_backend.c" if where == "c"
                                        else "nv2a_vk_backend.h"))
    if missing:
        print("vk-suballoc-gate: the sub-allocation seam the gate binds to is gone:",
              file=sys.stderr)
        for name in missing:
            print("  missing: %s" % name, file=sys.stderr)
        print("The gate drives the production bodies of this file; a move or rename here is a "
              "change to the seam, not a reason to skip the case.", file=sys.stderr)
        return 2

    seam = probe(c_text, h_text)
    c_digest = hashlib.sha256(c_raw).hexdigest()
    h_digest = hashlib.sha256(h_raw).hexdigest()
    c_source, h_source = str(c_path.resolve()), str(h_path.resolve())

    header = ["/* Generated by scripts/vk-suballoc-gate.py from the toolkit's Vulkan back end.",
              " * Do not edit: the file is rebuilt from the sources every time either changes.",
              " *",
              " * The sub-allocation gate compiles nv2a_vk_backend.c into its own translation",
              " * unit, so the allocator it drives is the production one.  These markers",
              " * describe that allocator's design in the file the test was built against;",
              " * where one is 0, the test reports a failed check naming it (never a skip). */",
              "#ifndef VK_SUBALLOC_GATE_SEAM_H",
              "#define VK_SUBALLOC_GATE_SEAM_H",
              "",
              "#define VB_GATE_SOURCE %s" % c_string(c_source),
              "#define VB_GATE_SOURCE_SHA256 %s" % c_string(c_digest),
              "#define VB_GATE_SOURCE_BYTES %d" % len(c_raw),
              "#define VB_GATE_DECLS %s" % c_string(h_source),
              "#define VB_GATE_DECLS_SHA256 %s" % c_string(h_digest),
              "#define VB_GATE_REQUIRED_ANCHORS %d" % len(REQUIRED),
              ""]
    for name in sorted(seam):
        header.append("#define %s %d" % (name, seam[name]))
    header.append("")
    header.append("#endif /* VK_SUBALLOC_GATE_SEAM_H */")

    manifest = ["vk-suballoc-gate: the seam of %s" % c_source,
                "  sha256  %s" % c_digest,
                "  bytes   %d" % len(c_raw),
                "  decls   %s" % h_source,
                "  sha256  %s" % h_digest,
                "  required anchors present: %d of %d" % (len(REQUIRED), len(REQUIRED)),
                ""]
    for where, name, literal in REQUIRED:
        manifest.append("  [c] %s" % name if where == "c" else "  [h] %s" % name)
    manifest.append("")
    for name in sorted(seam):
        manifest.append("  %-32s %s" % (name, "present" if seam[name] else "ABSENT"))
    manifest.append("")
    manifest.append("The anchors above are what the gate binds to: their absence fails the")
    manifest.append("BUILD (exit 2).  The markers are the allocator's design, decided at RUN")
    manifest.append("time by the property that needs it.")

    Path(args.header).parent.mkdir(parents=True, exist_ok=True)
    Path(args.header).write_text("\n".join(header) + "\n", encoding="utf-8")
    Path(args.manifest).write_text("\n".join(manifest) + "\n", encoding="utf-8")
    print("vk-suballoc-gate: probed %s (sha256 %s)" % (c_path, c_digest[:16]))
    print("  required anchors: %d of %d present" % (len(REQUIRED), len(REQUIRED)))
    for name in sorted(seam):
        print("  %-32s %s" % (name, "present" if seam[name] else "ABSENT"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
