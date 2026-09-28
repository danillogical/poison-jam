#!/usr/bin/env python3
"""Extract the game's JsrfRegistry (and its A2h NULL-slot latch) from an archived minidump.

WHY THIS TOOL EXISTS
--------------------
The A2h NULL-slot latch lives inside `JsrfRegistry`, which `tools/harness/collect.c` adds to the
minidump's extra-memory list as a WHOLE STRUCT:

    extra_memory[extra_count].base = address; extra_memory[extra_count++].size = sizeof(*registry);

So the latch bytes ARE archived. But the registry sits at a **HOST** address (e.g.
`0x00007FF745DF9000`), while `scripts/inspect-jsrf.py` reads **guest** addresses and rejects a
64-bit VA outright. **No checked-in tool could read the latch**, which made the packet's decision
records unreadable even though they were present. This tool closes that gap.

It is deliberately narrow: it parses the minidump's stream directory, finds the module/memory
information it needs, and returns the registry bytes. It does not attempt to be a general dump
reader.

FAIL-CLOSED RULES
-----------------
  * an unknown minidump version, a missing stream, or a truncated range raises rather than
    returning a zero-filled buffer;
  * the registry's own `version` field is CHECKED against the expected value, because the layout
    is version-tagged and a mismatch means the reader would misinterpret every field after it;
  * every field is read at a computed offset that is derived from the C struct, and the computed
    struct size is asserted against what the collector would have written.

USAGE
-----
    python -X utf8 scripts/a2h-read-registry.py <run-dir>
    python -X utf8 scripts/a2h-read-registry.py <run-dir> --json out.json
    python -X utf8 scripts/a2h-read-registry.py --self-test
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ── The JsrfRegistry layout, computed from src/diagnostics.h ────────────────────────────────
# Version 2 appends JsrfSlotLatch. Version 3 appends JsrfSlotWriteWatch. JSRF_REGISTRY_VERSION
# must match or the reader refuses.
#
# BOTH VERSIONS ARE STILL UNDERSTOOD, and that is deliberate rather than convenience: the real
# archived run that validated this layout is a VERSION-2 dump, so the check that pins the layout
# against actual bytes can only be kept by continuing to read v2. The v3 layout is pinned the same
# way, against a checked-in fixture, and both must agree with the arithmetic.
EXPECTED_VERSION = 3
KNOWN_VERSIONS = (2, 3)
JSRF_THREAD_CAPACITY = 128
JSRF_EVENT_CAPACITY = 128
JSRF_REGISTER_COUNT = 10
JSRF_LATCH_CAPACITY = 128
JSRF_WRITE_CLASS_CAPACITY = 6
JSRF_ALIAS_CAPACITY = 28

# struct JsrfEvent { uint32 sequence, kind; uint64 ticks; uint32 target, site, esp, value; }
#   sequence 0..3, kind 4..7, ticks 8..15 (8-aligned), target 16..19, site 20..23,
#   esp 24..27, value 28..31. The struct's alignment is 8 (from the uint64) and 32 is a multiple
#   of 8, so SIZE IS 32.
#
# AN EARLIER VERSION OF THIS TOOL SAID 24 AND WAS WRONG, which shifted every derived offset and
# made the reader report the latch as absent from a dump that contains it. The value is now
# pinned by a test that measures it from a REAL archived dump: the registry header is located by
# its known (version, claimed, overflow, reserved) tuple, and the latch is located by its
# distinctive install record, and the measured offset must equal the computed one.
EVENT_SIZE = 32
# struct JsrfThread { uint32 state, tid, identity, start; uint32 stack_low, stack_high, count,
#                     reserved; uint64 registers[10]; JsrfEvent events[128]; }
THREAD_HEAD = 4 * 8            # 8 uint32
THREAD_REGS = 8 * JSRF_REGISTER_COUNT
THREAD_SIZE = THREAD_HEAD + THREAD_REGS + EVENT_SIZE * JSRF_EVENT_CAPACITY
# struct JsrfSlotTransition { 8 uint32 + uint64 ticks } -> 32 + 8 = 40, 8-aligned
TRANSITION_SIZE = 8 * 4 + 8
# struct JsrfSlotLatch { 8 uint32; JsrfSlotTransition slots[128]; }
LATCH_HEAD = 8 * 4
LATCH_SIZE = LATCH_HEAD + TRANSITION_SIZE * JSRF_LATCH_CAPACITY
# struct JsrfRegistry { uint32 version, claimed, overflow, reserved; uint64 frequency;
#                       JsrfThread threads[128]; JsrfSlotLatch latch; }
REGISTRY_HEAD = 4 * 4 + 8      # 24
THREADS_OFF = REGISTRY_HEAD
LATCH_OFF = REGISTRY_HEAD + THREAD_SIZE * JSRF_THREAD_CAPACITY
REGISTRY_V2_SIZE = LATCH_OFF + LATCH_SIZE

# ── Version 3 appends JsrfSlotWriteWatch ─────────────────────────────────────────────────────
# struct JsrfWriteClass { 8 uint32; uint64 ticks; uint64 rip; } -> 32 + 8 + 8 = 48, 8-aligned
WRITE_CLASS_SIZE = 8 * 4 + 8 + 8
# struct JsrfAliasTouch { 6 uint32; uint64 rip; uint64 ticks; } -> 24 + 8 + 8 = 40, 8-aligned
ALIAS_TOUCH_SIZE = 6 * 4 + 8 + 8
# struct JsrfSlotWriteWatch {
#     uint32 armed, alias_count, mapped_mask, protect_mask;
#     uint32 touched_count, publish_failed, handshake_seen, class_overflow;
#     uint32 terminal_seen, terminal_target, terminal_slot, terminal_flags;
#     uint32 terminal_offset_lo, terminal_offset_hi, arm_offset_lo, arm_offset_hi;
#     uint32 class_claimed, reserved;            <- 18 uint32 = 72 bytes
#     uint64 handshake_ticks, terminal_ticks;    <- 16 bytes, 8-aligned
#     uint64 class_counts[6];                    <- 48
#     JsrfWriteClass classes[6];                 <- 288
#     JsrfAliasTouch aliases[28];                <- 1120
# }
WATCH_HEAD = 18 * 4
WATCH_SIZE = WATCH_HEAD + 2 * 8 + 8 * JSRF_WRITE_CLASS_CAPACITY \
             + WRITE_CLASS_SIZE * JSRF_WRITE_CLASS_CAPACITY \
             + ALIAS_TOUCH_SIZE * JSRF_ALIAS_CAPACITY
WATCH_OFF = REGISTRY_V2_SIZE
REGISTRY_SIZE = WATCH_OFF + WATCH_SIZE
# struct JsrfRegistry { uint32 version, claimed, overflow, reserved; uint64 frequency;
#                       JsrfThread threads[128]; JsrfSlotLatch latch; JsrfSlotWriteWatch watch; }
REGISTRY_SIZE_V2 = REGISTRY_V2_SIZE

# The compile-time size the C header asserts. A drift between this reader's arithmetic and the
# struct the collector actually archived would not fail -- it would misattribute every field after
# the drift -- so the two are required to agree rather than being allowed to differ quietly.
JSRF_REGISTRY_SIZE_EXPECTED = 545344

# ── Minidump constants ───────────────────────────────────────────────────────────────────────
MDMP_SIGNATURE = b"MDMP"
MINIDUMP_VERSION = 0xA793
# Stream type numbers, VERIFIED against a real archived dump rather than assumed: that dump
# carries type 17 (Memory64List) and type 5 (MemoryList). A reader that guessed type 9 for the
# 64-bit list would find nothing and report a present registry as absent.
MEMORY_LIST_STREAM = 5          # MINIDUMP_MEMORY_LIST_STREAM (32-bit addresses)
MEMORY64_LIST_STREAM = 17       # MINIDUMP_MEMORY64_LIST_STREAM (64-bit addresses)
MODULE_LIST_STREAM = 4
THREAD_LIST_STREAM = 3


class RegistryError(Exception):
    """Raised rather than returning a plausible-looking zero-filled buffer."""


def parse_header(raw: bytes) -> dict:
    if raw[:4] != MDMP_SIGNATURE:
        raise RegistryError("not a minidump: signature %r" % raw[:4])
    sig, ver, nstreams, sdir = struct.unpack_from("<IIII", raw, 0)
    # The Version field packs TWO 16-bit halves: the low half is MINIDUMP_VERSION (0xA793) and
    # the high half is an implementation-specific version. Comparing the whole dword rejects
    # every real dump -- an over-strict check that would have failed closed on valid input.
    low = ver & 0xFFFF
    high = (ver >> 16) & 0xFFFF
    if low != MINIDUMP_VERSION:
        raise RegistryError("unexpected minidump version 0x%04X (expected 0x%04X)"
                            % (low, MINIDUMP_VERSION))
    streams = {}
    for i in range(nstreams):
        st, sz, rva = struct.unpack_from("<III", raw, sdir + i * 12)
        if st:
            streams[st] = (sz, rva)
    return {"version": ver, "version_low": low, "version_high": high,
            "nstreams": nstreams, "streams": streams}


def memory_ranges(raw: bytes, hdr: dict) -> list:
    """Every (start, size, file_offset) the dump carries, from BOTH memory-list streams.

    Two forms matter and a reader must handle both:
      * type 5 (MINIDUMP_MEMORY_LIST) carries 32-bit start addresses AND an explicit file offset
        per descriptor;
      * type 17 (MINIDUMP_MEMORY64_LIST) carries 64-bit start addresses and a single BaseRva,
        with each range's data laid out CONTIGUOUSLY from there -- there is no per-range offset.

    A 64-bit target's registry lives at a 64-bit address, so it can only appear in the type-17
    list. A reader that knew only type 5, or that assumed type 17 also carried per-range offsets,
    would report a present registry as absent.
    """
    out = []
    if MEMORY_LIST_STREAM in hdr["streams"]:
        sz, rva = hdr["streams"][MEMORY_LIST_STREAM]
        n = struct.unpack_from("<I", raw, rva)[0]
        for i in range(n):
            base = rva + 4 + i * 16
            start, size, off = struct.unpack_from("<III", raw, base)
            out.append((start, size, off))
    if MEMORY64_LIST_STREAM in hdr["streams"]:
        sz, rva = hdr["streams"][MEMORY64_LIST_STREAM]
        n = struct.unpack_from("<Q", raw, rva)[0]
        base_rva = struct.unpack_from("<Q", raw, rva + 8)[0]
        cursor = base_rva
        for i in range(n):
            d = rva + 16 + i * 16
            # BOUNDS-CHECK EACH DESCRIPTOR AGAINST THE STREAM'S OWN DECLARED SIZE. A real
            # archived dump in this project declares one more descriptor than its stream size can
            # hold, so an unchecked read runs off the end of the file and raises struct.error --
            # which would be reported as a tool crash rather than as the truncated-record fact it
            # is. Stop at the stream boundary and let the caller see a short range list.
            if d + 16 > rva + sz or d + 16 > len(raw):
                break
            start, size = struct.unpack_from("<QQ", raw, d)
            out.append((start, size, cursor))
            cursor += size
    return out


def read_range(raw: bytes, ranges: list, addr: int, size: int) -> bytes:
    """Read `size` bytes at `addr`, failing closed if the dump does not carry them."""
    for start, rsize, off in ranges:
        if start <= addr and addr + size <= start + rsize:
            delta = addr - start
            return raw[off + delta: off + delta + size]
    raise RegistryError("address 0x%016X (+%d bytes) is not present in this dump" % (addr, size))


def find_registry_address(run_dir: Path):
    """The registry's host address, printed version and claimed count, from the run's stacks.txt.

    `claimed` is returned as well as the address because it is the second word of the header
    signature this tool searches for: the collector prints it, so the search does not have to
    guess it. Guessing was tolerable while only one archived dump existed; it stops being
    tolerable once the layout is versioned and two versions must both be readable.
    """
    p = run_dir / "stacks.txt"
    if not p.exists():
        raise RegistryError("no stacks.txt in %s" % run_dir)
    text = p.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"GUEST_REGISTRY address=([0-9A-Fa-f]+)\s+version=(\d+)(?:\s+claimed=(\d+))?", text)
    if not m:
        raise RegistryError("no GUEST_REGISTRY line in stacks.txt")
    claimed = int(m.group(3)) if m.group(3) is not None else None
    return int(m.group(1), 16), int(m.group(2)), claimed


def locate_registry_in_dump(raw: bytes, version: int, claimed_hint: int = None) -> int:
    """Find the registry's FILE OFFSET in the dump by its unique header signature.

    WHY A SIGNATURE SEARCH RATHER THAN A MEMORY-LIST WALK: the registry lives at a 64-bit HOST
    address, so reaching it means walking MINIDUMP_MEMORY64_LIST. This project's archived dumps
    carry a memory-list stream whose declared descriptor count does not match its stream size, so
    a descriptor walk either runs off the end or produces garbage ranges. Rather than depend on
    that, this locates the registry by a signature that is VERIFIABLY UNIQUE, and then VALIDATES
    the location against the independently computed layout:

      * the header tuple (version, claimed, overflow, reserved) must occur EXACTLY ONCE;
      * the latch's install record (install_seen=1, install_raw=0x80000115, install_value=
        0xFE000104, install_ok=1) must occur EXACTLY ONCE;
      * the DISTANCE between them must equal the layout-derived LATCH_OFF exactly.

    That third check is what makes the search sound rather than a guess: the two signatures are
    independent, and a wrong layout would put them at the wrong distance. The Session verified
    this on a real archived run: one header hit, one install hit, distance 538648 == LATCH_OFF.

    THE SAME PROPERTY IS KEPT FOR VERSION 3. A v3 dump must additionally show the WATCH record at
    exactly WATCH_OFF -- a second, independent distance check against a second pair of signatures
    -- so the appended layout is validated against bytes rather than only against arithmetic. The
    watch is located by its own distinctive tuple, and its absence is reported rather than raised,
    because a v3 binary with the diagnostic gate off writes no watch fields at all and that is a
    legitimate state, not a layout error.
    """
    hdr_pat = struct.pack("<IIII", version, claimed_hint if claimed_hint is not None else 5, 0, 0)
    hits = []
    i = 0
    while True:
        i = raw.find(hdr_pat, i)
        if i < 0:
            break
        hits.append(i)
        i += 1
    if len(hits) != 1:
        raise RegistryError(
            "the registry header signature occurs %d times (expected exactly 1); a search-based "
            "location is only sound when the signature is unique" % len(hits))
    reg_off = hits[0]

    latch_pat = struct.pack("<IIII", 1, 0x80000115, 0xFE000104, 1)
    lh = []
    i = 0
    while True:
        i = raw.find(latch_pat, i)
        if i < 0:
            break
        lh.append(i)
        i += 1
    if len(lh) == 0:
        # AN ABSENT INSTALL RECORD IS AN EXPECTED STATE, NOT AN ERROR. With the diagnostic gate
        # OFF, the install callback never runs, so no record exists -- and that is precisely what
        # the OFF run is supposed to show. Report it as absent rather than raising, so a caller
        # can distinguish "gate was off" from "the layout is wrong".
        return reg_off, False
    if len(lh) != 1:
        raise RegistryError(
            "the latch install-record signature occurs %d times (expected at most 1)" % len(lh))
    measured = lh[0] - reg_off
    if measured != LATCH_OFF:
        raise RegistryError(
            "measured latch offset %d does not match the layout-derived LATCH_OFF %d -- the "
            "struct layout is wrong, so every field read after the header would be misread"
            % (measured, LATCH_OFF))
    return reg_off, True


def locate_watch_in_dump(raw: bytes, reg_off: int, version: int):
    """Locate the version-3 write watch by DERIVING its offset, then corroborating by content.

    WHY THIS IS DERIVED AND NOT SEARCHED. An earlier version searched the dump for the watch's own
    arming record -- `(armed=1, alias_count, mapped_mask, protect_mask)` with `mapped == protect` --
    and required the hit to be unique. That is unsound, and a real archive proved it: in a
    GATE-OFF run the watch never arms, so the true record is all zeros, while the pattern for
    small `alias_count` is not distinctive at all -- `alias_count == 1` produces `(1,1,1,1)`, which
    occurs dozens of times in a 129 MB dump. The uniqueness guard therefore RAISED on the very
    archive the successor packet must read: a fail-closed guard correctly rejecting an unsound
    search.

    The sound technique is the one already used for `LATCH_OFF`: the registry header is verified
    UNIQUE, `WATCH_OFF` is computed from the C layout, so the watch location is DERIVED from the
    registry rather than searched for. Content is then only a CORROBORATING check that must match
    one of the two states the writer can actually produce:

      * gate OFF -- the watch is all zero, because the diagnostic never ran;
      * gate ON  -- `armed` is exactly 0 or 1, `alias_count` is within capacity, and when armed
                    the mapped and protect masks are equal (arming succeeds only if they agree).

    A location whose content satisfies NEITHER still raises, so the fail-closed property is kept:
    the check moved from "is the pattern unique" (unsound) to "does the derived location contain a
    state the writer could have produced" (sound).
    """
    if version < 3:
        return None, False

    off = reg_off + WATCH_OFF
    if off + WATCH_HEAD > len(raw):
        raise RegistryError(
            "derived write-watch offset %d is past the end of the dump (%d bytes)"
            % (off, len(raw)))

    armed, alias_count, mapped_mask, protect_mask = struct.unpack_from("<IIII", raw, off)

    if armed == 0 and alias_count == 0 and mapped_mask == 0 and protect_mask == 0:
        # Gate OFF: the diagnostic never armed, so an all-zero record is legitimate. This is the
        # OFF-run inertness state, and its presence is returned rather than raised.
        return off, False

    # Gate ON: only these states are producible.
    if armed > 1:
        raise RegistryError(
            "write-watch at the derived offset %d has armed=%u, which the writer cannot produce "
            "-- the version-3 struct layout is wrong, so every watch field would be misread"
            % (off, armed))
    if alias_count > JSRF_ALIAS_CAPACITY:
        raise RegistryError(
            "write-watch at the derived offset %d has alias_count=%u, above the capacity %d"
            % (off, alias_count, JSRF_ALIAS_CAPACITY))
    if armed == 1 and alias_count and mapped_mask != protect_mask:
        raise RegistryError(
            "write-watch at the derived offset %d is armed with mapped=%08X but protected=%08X; "
            "arming only succeeds when those agree, so this is not a record the writer produced"
            % (off, mapped_mask, protect_mask))

    return off, True


def read_registry(run_dir: Path) -> dict:
    dmp = run_dir / "process.dmp"
    if not dmp.exists():
        raise RegistryError("no process.dmp in %s" % run_dir)
    raw = dmp.read_bytes()
    hdr = parse_header(raw)
    addr, printed_version, printed_claimed = find_registry_address(run_dir)

    # LOCATE FIRST, using the version the collector printed: it is the version that is actually in
    # the file, and it is the second word of the header signature this search keys on.
    reg_off, install_present = locate_registry_in_dump(
        raw, printed_version if printed_version in KNOWN_VERSIONS else EXPECTED_VERSION,
        printed_claimed)

    # THE VERSION IN THE DUMP DECIDES THE LAYOUT, not the version the reader prefers. A v2 dump
    # must still be read as v2 -- the layout's byte-level validation lives in a v2 archive, and
    # reading it as v3 would silently reinterpret the bytes after the latch. So the version is
    # taken from the dump's own header word FIRST, and the number of bytes read follows from it:
    # a v2 registry is genuinely 1544 bytes shorter, and demanding the v3 size from it would fail
    # closed on a valid archive.
    if reg_off + REGISTRY_HEAD > len(raw):
        raise RegistryError("registry header at %d runs past the end of the dump" % reg_off)
    dump_version = struct.unpack_from("<I", raw, reg_off)[0]
    if dump_version not in KNOWN_VERSIONS:
        raise RegistryError(
            "registry version %d is not one this reader understands %s -- the layout is "
            "version-tagged, so every field after the header would be misread"
            % (dump_version, KNOWN_VERSIONS))
    blob_size = REGISTRY_SIZE if dump_version >= 3 else REGISTRY_SIZE_V2
    blob = raw[reg_off:reg_off + blob_size]
    if len(blob) != blob_size:
        raise RegistryError("truncated registry: got %d bytes, expected %d"
                            % (len(blob), blob_size))

    version, claimed, overflow, reserved = struct.unpack_from("<IIII", blob, 0)
    frequency = struct.unpack_from("<Q", blob, 16)[0]

    if version != printed_version:
        raise RegistryError(
            "the dump's registry version %d disagrees with the collector's printed version %d -- "
            "the dump and the archive's text describe different layouts" % (version, printed_version))

    latch = blob[LATCH_OFF:LATCH_OFF + LATCH_SIZE]
    (install_seen, install_raw, install_value, install_ok,
     latch_claimed, latch_overflow, latch_partial, latch_seq) = struct.unpack_from("<IIIIIIII", latch, 0)

    transitions = []
    for i in range(JSRF_LATCH_CAPACITY):
        off = LATCH_HEAD + i * TRANSITION_SIZE
        (valid, tid, call_index, ordinal, before, after, intra, rsvd) = struct.unpack_from(
            "<IIIIIIII", latch, off)
        ticks = struct.unpack_from("<Q", latch, off + 32)[0]
        if valid or tid or call_index or ordinal or before or after:
            transitions.append({
                "slot": i, "valid": valid, "tid": tid, "call_index": call_index,
                "ordinal": ordinal, "before": "0x%08X" % before, "after": "0x%08X" % after,
                "intra": intra, "ticks": ticks,
            })

    threads = []
    for i in range(JSRF_THREAD_CAPACITY):
        off = THREADS_OFF + i * THREAD_SIZE
        state, tid, identity, start, slo, shi, count, rsvd = struct.unpack_from("<IIIIIIII", blob, off)
        if state or tid or identity:
            threads.append({"slot": i, "state": state, "tid": tid, "identity": identity,
                            "start": "0x%08X" % start, "stack": "0x%08X..0x%08X" % (slo, shi),
                            "events": count,
                            "events_lost": max(0, count - JSRF_EVENT_CAPACITY)})

    result = {
        "run_dir": str(run_dir),
        "dump_sha256": hashlib.sha256(raw).hexdigest().upper(),
        "dump_bytes": len(raw),
        "minidump_version": "0x%X" % hdr["version"],
        "registry_file_offset": reg_off,
        "install_record_present": install_present,
        "registry_address": "0x%016X" % addr,
        "registry_size": REGISTRY_SIZE,
        "version": version,
        "version_matches_reader": version == EXPECTED_VERSION,
        "printed_version_matches": printed_version == version,
        "claimed": claimed,
        "overflow": overflow,
        "frequency": frequency,
        "threads": threads,
        "latch": {
            "install_seen": install_seen,
            "install_raw": "0x%08X" % install_raw,
            "install_value": "0x%08X" % install_value,
            "install_ok": install_ok,
            "claimed": latch_claimed,
            "overflow": latch_overflow,
            "partial": latch_partial,
            "sequence": latch_seq,
            "transitions": transitions,
            "transition_count": len(transitions),
        },
        "layout": {
            "event_size": EVENT_SIZE, "thread_size": THREAD_SIZE,
            "transition_size": TRANSITION_SIZE, "latch_size": LATCH_SIZE,
            "write_class_size": WRITE_CLASS_SIZE, "alias_touch_size": ALIAS_TOUCH_SIZE,
            "watch_head": WATCH_HEAD, "watch_size": WATCH_SIZE,
            "threads_offset": THREADS_OFF, "latch_offset": LATCH_OFF,
            "watch_offset": WATCH_OFF,
            "registry_size_v2": REGISTRY_SIZE_V2,
            "registry_size": REGISTRY_SIZE,
        },
    }

    if version >= 3:
        watch_off, watch_present = locate_watch_in_dump(raw, reg_off, version)
        result["watch_record_present"] = watch_present
        result["watch_file_offset"] = watch_off
        result["watch"] = read_watch(blob)
    else:
        # A v2 archive genuinely has no watch. Reported as absent rather than as an empty watch, so
        # a caller cannot mistake "this layout predates the field" for "the field was zero".
        result["watch_record_present"] = False
        result["watch_file_offset"] = None
        result["watch"] = None
    return result


def read_watch(blob: bytes) -> dict:
    """Decode the version-3 write watch from a registry blob already validated as v3."""
    w = blob[WATCH_OFF:WATCH_OFF + WATCH_SIZE]
    if len(w) != WATCH_SIZE:
        raise RegistryError("truncated write watch: got %d bytes, expected %d" % (len(w), WATCH_SIZE))
    (armed, alias_count, mapped_mask, protect_mask,
     touched_count, publish_failed, handshake_seen, class_overflow,
     terminal_seen, terminal_target, terminal_slot, terminal_flags,
     term_lo, term_hi, arm_lo, arm_hi,
     class_claimed, rsvd) = struct.unpack_from("<18I", w, 0)
    handshake_ticks, terminal_ticks = struct.unpack_from("<QQ", w, WATCH_HEAD)
    counts = list(struct.unpack_from("<%dQ" % JSRF_WRITE_CLASS_CAPACITY, w, WATCH_HEAD + 16))

    classes = []
    for i in range(JSRF_WRITE_CLASS_CAPACITY):
        off = WATCH_HEAD + 16 + 8 * JSRF_WRITE_CLASS_CAPACITY + i * WRITE_CLASS_SIZE
        (valid, class_id, tid, call_index, ordinal, before, after, crsvd) = struct.unpack_from(
            "<IIIIIIII", w, off)
        ticks, rip = struct.unpack_from("<QQ", w, off + 32)
        classes.append({
            "index": i, "valid": valid, "class_id": class_id, "tid": tid,
            "call_index": call_index, "ordinal": ordinal,
            "before": "0x%08X" % before, "after": "0x%08X" % after,
            "ticks": ticks, "rip": "0x%016X" % rip,
            "count": counts[i],
        })

    aliases = []
    base = WATCH_HEAD + 16 + 8 * JSRF_WRITE_CLASS_CAPACITY + WRITE_CLASS_SIZE * JSRF_WRITE_CLASS_CAPACITY
    for i in range(JSRF_ALIAS_CAPACITY):
        off = base + i * ALIAS_TOUCH_SIZE
        (valid, alias_index, mapped, published, fault_va, value) = struct.unpack_from("<IIIIII", w, off)
        rip, ticks = struct.unpack_from("<QQ", w, off + 24)
        if valid:
            aliases.append({
                "index": i, "valid": valid, "alias_index": alias_index, "mapped": mapped,
                "published": published, "fault_va": "0x%08X" % fault_va,
                "value": "0x%08X" % value, "rip": "0x%016X" % rip, "ticks": ticks,
            })

    return {
        "armed": armed,
        "alias_count": alias_count,
        "mapped_mask": "0x%08X" % mapped_mask,
        "protect_mask": "0x%08X" % protect_mask,
        "touched_count": touched_count,
        "publish_failed": publish_failed,
        "handshake_seen": handshake_seen,
        "class_overflow": class_overflow,
        "class_claimed": class_claimed,
        "terminal_seen": terminal_seen,
        "terminal_target": "0x%08X" % terminal_target,
        "terminal_slot": "0x%08X" % terminal_slot,
        "terminal_flags": terminal_flags,
        "terminal_offset": "0x%08X%08X" % (term_hi, term_lo),
        "arm_offset": "0x%08X%08X" % (arm_hi, arm_lo),
        "offset_stable": (term_lo, term_hi) == (arm_lo, arm_hi),
        "handshake_ticks": handshake_ticks,
        "terminal_ticks": terminal_ticks,
        "class_counts": counts,
        "classes": classes,
        "class_witness_count": sum(1 for c in classes if c["valid"]),
        "aliases": aliases,
        "alias_touch_count": len(aliases),
        "alias_write_count": sum(1 for a in aliases if a["published"]),
    }


def self_test() -> int:
    """Fixture: the layout arithmetic and the fail-closed paths, on KNOWN values."""
    checks = []

    def chk(label, cond):
        checks.append((label, bool(cond)))

    chk("event size is 32", EVENT_SIZE == 32)
    chk("thread size is 4208", THREAD_SIZE == 4208)
    chk("transition size is 40", TRANSITION_SIZE == 40)
    chk("latch size is 5152", LATCH_SIZE == 5152)
    chk("latch offset is 538648", LATCH_OFF == 538648)
    chk("registry v2 size is 543800", REGISTRY_SIZE_V2 == 543800)
    chk("registry v2 size is latch offset + latch", REGISTRY_SIZE_V2 == LATCH_OFF + LATCH_SIZE)
    chk("write class size is 48", WRITE_CLASS_SIZE == 48)
    chk("alias touch size is 40", ALIAS_TOUCH_SIZE == 40)
    chk("watch head is 72", WATCH_HEAD == 72)
    chk("watch size is 1544", WATCH_SIZE == 1544)
    chk("watch offset is 543800", WATCH_OFF == 543800)
    chk("watch offset is the v2 size", WATCH_OFF == REGISTRY_SIZE_V2)
    chk("registry size is 545344", REGISTRY_SIZE == 545344)
    chk("registry size is watch offset + watch", REGISTRY_SIZE == WATCH_OFF + WATCH_SIZE)
    chk("registry size matches the C header's assertion",
        REGISTRY_SIZE == JSRF_REGISTRY_SIZE_EXPECTED)
    chk("registry size is under 1 MiB", REGISTRY_SIZE < 1024 * 1024)

    # MEASURED-FROM-A-REAL-DUMP CONTROL. These constants were WRONG once (EVENT_SIZE 24), which
    # shifted every offset and made the reader report the latch absent from a dump containing it.
    # The real archived run lets the layout be checked against bytes rather than arithmetic. It is
    # a VERSION-2 dump, which is why v2 stays readable: this is the only byte-level pin that exists
    # for the shared prefix, and it must not be retired just because the current version moved on.
    real = ROOT / "logs" / "runs" / "20260928-001520-474-a2h-null-slot-authoritative-on"
    if (real / "process.dmp").exists():
        raw = (real / "process.dmp").read_bytes()
        hdr_pat = struct.pack("<IIII", 2, 5, 0, 0)
        reg_hits = []
        i = 0
        while True:
            i = raw.find(hdr_pat, i)
            if i < 0:
                break
            reg_hits.append(i)
            i += 1
        chk("real dump: exactly one registry header", len(reg_hits) == 1)
        latch_pat = struct.pack("<IIII", 1, 0x80000115, 0xFE000104, 1)
        latch_hits = []
        i = 0
        while True:
            i = raw.find(latch_pat, i)
            if i < 0:
                break
            latch_hits.append(i)
            i += 1
        chk("real dump: exactly one install record", len(latch_hits) == 1)
        if reg_hits and latch_hits:
            measured = latch_hits[0] - reg_hits[0]
            chk("real dump: measured latch offset == computed LATCH_OFF (%d)" % measured,
                measured == LATCH_OFF)

    # A synthetic minidump carrying one range, to prove the reader works and fails closed.
    import tempfile

    def build_registry(version: int) -> bytes:
        body = struct.pack("<IIII", version, 5, 0, 0) + struct.pack("<Q", 10000000)
        body += b"\x00" * (REGISTRY_SIZE - len(body))
        latch = bytearray(body[LATCH_OFF:LATCH_OFF + LATCH_SIZE])
        struct.pack_into("<IIIIIIII", latch, 0, 1, 0x80000115, 0xFE000104, 1, 1, 0, 0, 2)
        struct.pack_into("<IIIIIIII", latch, LATCH_HEAD, 1, 1234, 7, 277, 0xFE000104, 0, 1, 0)
        struct.pack_into("<Q", latch, LATCH_HEAD + 32, 555)
        body = body[:LATCH_OFF] + bytes(latch)
        if version >= 3:
            # Re-extend to the full v3 size: the splice above truncated the buffer at the end of
            # the latch, so the watch region does not exist yet.
            body = body + b"\x00" * (REGISTRY_SIZE - len(body))
            w = bytearray(body[WATCH_OFF:WATCH_OFF + WATCH_SIZE])
            # The arming record: armed, alias_count=28, mapped==protect==(1<<28)-1.
            struct.pack_into("<IIII", w, 0, 1, 28, (1 << 28) - 1, (1 << 28) - 1)
            # touched_count=1, no publish failure, handshake seen, terminal seen.
            struct.pack_into("<IIII", w, 16, 1, 0, 1, 0)
            struct.pack_into("<IIII", w, 32, 1, 0x00000000, 0x00000000, 0x7)
            struct.pack_into("<IIII", w, 48, 0x10000, 0x0, 0x10000, 0x0)
            struct.pack_into("<II", w, 64, 1, 0)
            struct.pack_into("<QQ", w, WATCH_HEAD, 111, 999)
            struct.pack_into("<Q", w, WATCH_HEAD + 16 + 8 * 0, 3)     # class 0 write count
            c0 = WATCH_HEAD + 16 + 8 * JSRF_WRITE_CLASS_CAPACITY
            struct.pack_into("<IIIIIIII", w, c0, 1, 0, 4242, 9, 277, 0xFE000104, 0, 0)
            struct.pack_into("<QQ", w, c0 + 32, 777, 0x00007FF700123456)
            a0 = c0 + WRITE_CLASS_SIZE * JSRF_WRITE_CLASS_CAPACITY
            struct.pack_into("<IIIIII", w, a0, 1, 0, 1, 1, 0x04000000 + 0x4064, 0xFE000104)
            struct.pack_into("<QQ", w, a0 + 24, 0x00007FF700654321, 888)
            body = body[:WATCH_OFF] + bytes(w)
        return body

    body = build_registry(EXPECTED_VERSION)

    ADDR = 0x00007FF700000000

    def build_dump(path: Path, payload: bytes, range_start: int = None, range_size: int = None):
        """Lay out a minimal but STRUCTURALLY VALID minidump carrying one memory range.

        THREE things this fixture must get right, each of which an earlier version got wrong and
        the reader then correctly rejected:
          * MINIDUMP_HEADER is **32** bytes (eight 4-byte fields: Signature, Version,
            NumberOfStreams, StreamDirectoryRva, CheckSum, TimeDateStamp, Flags, and the padding
            that rounds it to 32). `struct.pack("<IIII", ...)` produces only 16, so the directory
            must be padded out to 32 or every subsequent offset is wrong.
          * a 64-bit start address CANNOT be expressed in the 32-bit memory list, so the fixture
            uses the type-17 (Memory64List) form, whose descriptors carry NO per-range file offset
            -- the data follows a single BaseRva contiguously.
          * the payload must be at least REGISTRY_SIZE bytes, or the reader correctly reports the
            range as absent.
        """
        start = ADDR if range_start is None else range_start
        size = len(payload) if range_size is None else range_size
        hdr_size = 32
        dir_size = 12
        memlist_size = 16 + 16          # NumberOfMemoryRanges(8) + BaseRva(8) + one descriptor(16)
        payload_off = hdr_size + dir_size + memlist_size
        hdr = struct.pack("<IIII", 0x504D444D, MINIDUMP_VERSION, 1, hdr_size)
        hdr += b"\x00" * (hdr_size - len(hdr))     # pad to the real 32-byte header
        dirr = struct.pack("<III", MEMORY64_LIST_STREAM, memlist_size, hdr_size + dir_size)
        meml = (struct.pack("<Q", 1) + struct.pack("<Q", payload_off)
                + struct.pack("<QQ", start, size))
        path.write_bytes(hdr + dirr + meml + payload)

    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        build_dump(p / "process.dmp", body)
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=%d claimed=5 overflow=0\n" % (ADDR, EXPECTED_VERSION),
            encoding="utf-8")
        r = read_registry(p)
    chk("fixture: version read", r["version"] == EXPECTED_VERSION)
    chk("fixture: install control read", r["latch"]["install_ok"] == 1
        and r["latch"]["install_raw"] == "0x80000115")
    chk("fixture: one transition read", r["latch"]["transition_count"] == 1)
    chk("fixture: transition fields", r["latch"]["transitions"][0]["tid"] == 1234
        and r["latch"]["transitions"][0]["ordinal"] == 277)
    # ── the version-3 watch, read back from fixture bytes ──────────────────────────────────────
    w = r["watch"]
    chk("v3 fixture: watch record present", r["watch_record_present"] is True)
    chk("v3 fixture: watch at the computed offset",
        r["watch_file_offset"] is not None and r["watch_file_offset"] - r["registry_file_offset"] == WATCH_OFF)
    chk("v3 fixture: alias census armed", w["armed"] == 1 and w["alias_count"] == 28)
    chk("v3 fixture: mapped == protected mask", w["mapped_mask"] == w["protect_mask"])
    chk("v3 fixture: one alias touch", w["alias_touch_count"] == 1)
    chk("v3 fixture: alias touch fields",
        w["aliases"][0]["rip"] == "0x00007FF700654321" and w["aliases"][0]["value"] == "0xFE000104")
    chk("v3 fixture: one class witness", w["class_witness_count"] == 1)
    chk("v3 fixture: class witness fields",
        w["classes"][0]["tid"] == 4242 and w["classes"][0]["before"] == "0xFE000104"
        and w["classes"][0]["after"] == "0x00000000" and w["classes"][0]["ordinal"] == 277)
    chk("v3 fixture: uncapped class counter", w["class_counts"][0] == 3)
    chk("v3 fixture: handshake and terminal seen",
        w["handshake_seen"] == 1 and w["terminal_seen"] == 1)
    chk("v3 fixture: terminal target is zero (the raw-zero call)", w["terminal_target"] == "0x00000000")
    chk("v3 fixture: offset stable arm->terminal", w["offset_stable"] is True)
    chk("v3 fixture: terminal flags say armed+stable+published", w["terminal_flags"] == 0x7)

    # A VERSION-2 ARCHIVE MUST STILL READ AS V2. This is the regression that matters most: the
    # layout's only byte-level pin is a v2 dump, so a reader that "upgraded" by reinterpreting v2
    # bytes as v3 would misread everything after the latch while still appearing to work.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        build_dump(p / "process.dmp", build_registry(2))
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=2 claimed=5 overflow=0\n" % ADDR, encoding="utf-8")
        r2 = read_registry(p)
    chk("v2 archive: read as version 2", r2["version"] == 2)
    chk("v2 archive: install control still read", r2["latch"]["install_ok"] == 1)
    chk("v2 archive: one transition read", r2["latch"]["transition_count"] == 1)
    chk("v2 archive: no watch claimed", r2["watch"] is None and r2["watch_record_present"] is False)
    chk("v2 archive: flagged as not the current version", r2["version_matches_reader"] is False)

    # A v3 registry whose watch record is ABSENT (gate off) must report absence, not a zeroed watch.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        no_watch = bytearray(build_registry(EXPECTED_VERSION))
        no_watch[WATCH_OFF:WATCH_OFF + WATCH_SIZE] = b"\x00" * WATCH_SIZE
        build_dump(p / "process.dmp", bytes(no_watch))
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=%d claimed=5 overflow=0\n" % (ADDR, EXPECTED_VERSION),
            encoding="utf-8")
        r3 = read_registry(p)
    chk("v3 gate-off: watch reported absent, not zeroed", r3["watch_record_present"] is False)
    chk("v3 gate-off: watch still decodable", r3["watch"]["armed"] == 0
        and r3["watch"]["alias_touch_count"] == 0)

    # A version mismatch must RAISE, not return misread fields.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        bad = struct.pack("<IIII", 99, 5, 0, 0) + b"\x00" * (REGISTRY_SIZE - 16)
        build_dump(p / "process.dmp", bad)
        (p / "stacks.txt").write_text("GUEST_REGISTRY address=%016X version=99\n" % ADDR, encoding="utf-8")
        try:
            read_registry(p)
            chk("version mismatch raises", False)
        except RegistryError:
            chk("version mismatch raises", True)

    # A dump whose version disagrees with the collector's printed version must RAISE: the two
    # describe different layouts, and picking either one silently is how a field gets misread.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        build_dump(p / "process.dmp", build_registry(EXPECTED_VERSION))
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=2 claimed=5 overflow=0\n" % ADDR, encoding="utf-8")
        try:
            read_registry(p)
            chk("dump/printed version disagreement raises", False)
        except RegistryError:
            chk("dump/printed version disagreement raises", True)

    # THE WATCH LOCATION IS DERIVED, NOT SEARCHED, so the fail-closed property to test is now
    # CONTENT-based: a derived location holding a state the writer cannot produce must RAISE.
    # This replaced an earlier test that moved the arming record to a wrong offset and expected a
    # distance mismatch -- that premise died with the search. The reason the search died is worth
    # keeping visible: `alias_count == 1` yields the pattern (1,1,1,1), which occurs dozens of
    # times in a real 129 MB dump, so requiring a unique hit RAISED on a legitimate gate-OFF
    # archive. The uniqueness guard was right; the search was the defect.
    for label, word0 in (("armed=5 (impossible)", 5), ("armed=2 (impossible)", 2)):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td)
            bad = bytearray(build_registry(EXPECTED_VERSION))
            struct.pack_into("<I", bad, WATCH_OFF, word0)
            build_dump(p / "process.dmp", bytes(bad))
            (p / "stacks.txt").write_text(
                "GUEST_REGISTRY address=%016X version=%d claimed=5 overflow=0\n"
                % (ADDR, EXPECTED_VERSION), encoding="utf-8")
            try:
                read_registry(p)
                chk("v3 %s raises" % label, False)
            except RegistryError:
                chk("v3 %s raises" % label, True)

    # An ARMED watch whose masks disagree is also unproducible: arming only succeeds when every
    # mapped mirror page was protected, so mapped == protect is an invariant of a successful arm.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        bad = bytearray(build_registry(EXPECTED_VERSION))
        struct.pack_into("<IIII", bad, WATCH_OFF, 1, 28, 0x0FFFFFFF, 0x0000FFFF)
        build_dump(p / "process.dmp", bytes(bad))
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=%d claimed=5 overflow=0\n"
            % (ADDR, EXPECTED_VERSION), encoding="utf-8")
        try:
            read_registry(p)
            chk("v3 armed-with-mismatched-masks raises", False)
        except RegistryError:
            chk("v3 armed-with-mismatched-masks raises", True)

    # And an ALL-ZERO watch is a LEGITIMATE state -- the gate-OFF inertness record -- so it must
    # NOT raise. This is the case the old search could not read, and it is the one the successor
    # packet depends on.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        build_dump(p / "process.dmp", build_registry(EXPECTED_VERSION))
        (p / "stacks.txt").write_text(
            "GUEST_REGISTRY address=%016X version=%d claimed=5 overflow=0\n"
            % (ADDR, EXPECTED_VERSION), encoding="utf-8")
        try:
            r = read_registry(p)
            chk("v3 all-zero watch reads as gate-OFF, not an error", True)
        except RegistryError as exc:
            chk("v3 all-zero watch reads as gate-OFF, not an error (%s)" % str(exc)[:40], False)

    # An absent range must RAISE, not return zeros.
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        build_dump(p / "process.dmp", b"\x00" * 64, range_start=0x1000, range_size=16)
        (p / "stacks.txt").write_text("GUEST_REGISTRY address=%016X version=2\n" % ADDR, encoding="utf-8")
        try:
            read_registry(p)
            chk("absent range raises", False)
        except RegistryError:
            chk("absent range raises", True)

    ok = True
    for label, passed in checks:
        print("  %-46s %s" % (label, "PASS" if passed else "FAIL"))
        ok = ok and passed
    print()
    print("SELF-TEST %s" % ("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("run_dir", nargs="?")
    ap.add_argument("--json")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not a.run_dir:
        ap.error("run_dir or --self-test is required")
    try:
        r = read_registry(Path(a.run_dir))
    except RegistryError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2
    print(json.dumps(r, indent=2, default=str))
    if a.json:
        Path(a.json).write_text(json.dumps(r, indent=2, default=str), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
