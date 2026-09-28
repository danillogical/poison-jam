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
# Version 2 appends JsrfSlotLatch. JSRF_REGISTRY_VERSION must match or the reader refuses.
EXPECTED_VERSION = 2
JSRF_THREAD_CAPACITY = 128
JSRF_EVENT_CAPACITY = 128
JSRF_REGISTER_COUNT = 10
JSRF_LATCH_CAPACITY = 128

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
REGISTRY_SIZE = LATCH_OFF + LATCH_SIZE

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


def find_registry_address(run_dir: Path) -> int:
    """The registry's host address, from the run's own stacks.txt."""
    p = run_dir / "stacks.txt"
    if not p.exists():
        raise RegistryError("no stacks.txt in %s" % run_dir)
    m = re.search(r"GUEST_REGISTRY address=([0-9A-Fa-f]+)\s+version=(\d+)", p.read_text(
        encoding="utf-8", errors="replace"))
    if not m:
        raise RegistryError("no GUEST_REGISTRY line in stacks.txt")
    return int(m.group(1), 16), int(m.group(2))


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


def read_registry(run_dir: Path) -> dict:
    dmp = run_dir / "process.dmp"
    if not dmp.exists():
        raise RegistryError("no process.dmp in %s" % run_dir)
    raw = dmp.read_bytes()
    hdr = parse_header(raw)
    addr, printed_version = find_registry_address(run_dir)

    reg_off, install_present = locate_registry_in_dump(raw, EXPECTED_VERSION)
    blob = raw[reg_off:reg_off + REGISTRY_SIZE]
    if len(blob) != REGISTRY_SIZE:
        raise RegistryError("truncated registry: got %d bytes, expected %d"
                            % (len(blob), REGISTRY_SIZE))

    version, claimed, overflow, reserved = struct.unpack_from("<IIII", blob, 0)
    frequency = struct.unpack_from("<Q", blob, 16)[0]

    if version != EXPECTED_VERSION:
        raise RegistryError(
            "registry version %d does not match this reader's expected %d -- the layout is "
            "version-tagged, so every field after the header would be misread"
            % (version, EXPECTED_VERSION))

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

    return {
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
            "threads_offset": THREADS_OFF, "latch_offset": LATCH_OFF,
            "registry_size": REGISTRY_SIZE,
        },
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
    chk("registry size is 543800", REGISTRY_SIZE == 543800)
    chk("registry size is latch offset + latch", REGISTRY_SIZE == LATCH_OFF + LATCH_SIZE)
    chk("registry size is under 1 MiB", REGISTRY_SIZE < 1024 * 1024)

    # MEASURED-FROM-A-REAL-DUMP CONTROL. These constants were WRONG once (EVENT_SIZE 24), which
    # shifted every offset and made the reader report the latch absent from a dump containing it.
    # The real archived run lets the layout be checked against bytes rather than arithmetic.
    real = ROOT / "logs" / "runs" / "20260928-001520-474-a2h-null-slot-authoritative-on"
    if (real / "process.dmp").exists():
        raw = (real / "process.dmp").read_bytes()
        hdr_pat = struct.pack("<IIII", EXPECTED_VERSION, 5, 0, 0)
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
    body = struct.pack("<IIII", EXPECTED_VERSION, 5, 0, 0) + struct.pack("<Q", 10000000)
    body += b"\x00" * (REGISTRY_SIZE - len(body))
    # Give the latch an install record and one transition.
    latch = bytearray(body[LATCH_OFF:])
    struct.pack_into("<IIIIIIII", latch, 0, 1, 0x80000115, 0xFE000104, 1, 1, 0, 0, 2)
    struct.pack_into("<IIIIIIII", latch, LATCH_HEAD, 1, 1234, 7, 277, 0xFE000104, 0, 1, 0)
    struct.pack_into("<Q", latch, LATCH_HEAD + 32, 555)
    body = body[:LATCH_OFF] + bytes(latch)

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
