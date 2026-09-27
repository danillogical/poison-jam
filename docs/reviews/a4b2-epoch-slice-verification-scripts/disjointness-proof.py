"""Descriptor-disjointness: PROVE alias-closure, do not assume it.

The Advisor ruled that "the callers write pairwise disjoint descriptor regions"
graduates from a Session finding to a LOAD-BEARING PREMISE, and must therefore be
proven inside the packet rather than assumed. This script does the proof work.

What must be shown, for the premise "the doorbell's descriptor is not aliased by
another builder call" to hold:

  (1) Every call site of the builder(s), enumerated (not sampled).
  (2) For each, the descriptor base r0 at entry, and hence the written region
      r0..r0+4 (the builder writes x:(r0+0)..x:(r0+4)).
  (3) The regions are pairwise disjoint.
  (4) No OTHER instruction writes into any of those regions -- i.e. the regions are
      alias-closed against the rest of the program, not merely disjoint from each
      other.
  (5) The READER: whatever consumes a descriptor must not be able to read one
      region as another by a computed pointer. This is the part the previous
      preparation explicitly left open.

Point (5) is the hard one. This script reports what it can establish and marks the
rest UNKNOWN rather than declaring closure it has not shown.
"""
import re
import sys

BASE = sys.argv[1]

dec = {}
for L in open(BASE + "/bootstrap_state_decode.txt", encoding="utf-8"):
    m = re.match(r"([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", L.rstrip("\n"))
    if m:
        dec[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())

# The second image too, since a builder caller could live there.
import subprocess  # noqa: F401  (kept explicit: no external calls are made)

BUILDERS = {0x00DB: "builder A", 0x00EB: "builder B"}

print("=== (1) every call site of the builders, enumerated ===")
calls = []
for pc, (w, t) in sorted(dec.items()):
    m = re.match(r"^(jsr|bsr)\s+p:\$([0-9a-f]{4})", t.lower())
    if m and int(m.group(2), 16) in BUILDERS:
        calls.append((pc, int(m.group(2), 16)))
for pc, tgt in calls:
    print("  P %04X -> P %04X  (%s)" % (pc, tgt, BUILDERS[tgt]))
print("  total call sites: %d" % len(calls))
print()

print("=== (2) r0 at entry for each call site (walk back to the last r0 write) ===")
def r0_at(pc):
    """Walk backwards through the DECODE ORDER to the nearest write to r0.

    Critical: `pc - 1` is WRONG here. DSP56300 instructions are 1..N words, so the
    previous instruction is not at pc-1 -- it is the previous entry in the decoded
    order. Using pc-1 skipped the very instruction that sets r0 at every call site
    (e.g. from P 0007 it landed on P 0006 and stopped, never reaching P 0002).
    """
    order = sorted(dec)
    try:
        i = order.index(pc)
    except ValueError:
        return (None, None, None)
    j, steps = i - 1, 0
    while j >= 0 and steps < 64:
        cur = order[j]
        w, t = dec[cur]
        tl = t.lower()
        m = re.search(r"#\$([0-9a-f]+)\s*,\s*r0\b", tl)
        if m:
            return (cur, t, int(m.group(1), 16))
        if re.search(r",\s*r0\s*$", tl) or re.search(r"move\s+r0\s*,", tl):
            return (cur, t, None)
        if (re.match(r"^(rts|rti|bra|jmp|jsr|bsr)\b", tl)
                or re.match(r"^(beq|bne|bge|bgt|ble|blt|brclr|brset|jclr|jset)\b", tl)
                or re.match(r"^(do|dor|rep)\b", tl)):
            return (cur, t, None)
        j -= 1
        steps += 1
    return (None, None, None)

regions = []
for pc, tgt in calls:
    src, txt, val = r0_at(pc)
    print("  call P %04X: r0 <- P %s  %s  value=%s"
          % (pc, "%04X" % src if src else "????", txt or "?", ("0x%X" % val) if val is not None else "UNKNOWN"))
    if val is not None:
        regions.append((pc, val, val + 4))
print()

print("=== (3) are the descriptor regions pairwise disjoint? ===")
ok = True
for i in range(len(regions)):
    for j in range(i + 1, len(regions)):
        a, b = regions[i], regions[j]
        ov = not (a[2] < b[1] or b[2] < a[1])
        if ov:
            ok = False
        print("  call P %04X x:[%d..%d] vs call P %04X x:[%d..%d]: %s"
              % (a[0], a[1], a[2], b[0], b[1], b[2], "OVERLAP" if ov else "disjoint"))
print("  PAIRWISE DISJOINT: %s" % ok)
print()

print("=== (4) alias-closure: does ANY other instruction write into those regions? ===")
covered = set()
for _, lo, hi in regions:
    covered.update(range(lo, hi + 1))
writers = []
for pc, (w, t) in sorted(dec.items()):
    # any X write whose address could fall in the covered set
    for m in re.finditer(r"x:\(r(\d)\s*\+\s*(\d+)\)", t.lower()):
        writers.append((pc, t, "computed x:(r%s+%s)" % (m.group(1), m.group(2))))
    for m in re.finditer(r"x:\$([0-9a-f]{4})\b", t.lower()):
        a = int(m.group(1), 16)
        if a in covered:
            writers.append((pc, t, "direct x:$%04X" % a))
print("  candidate writers touching the covered regions: %d" % len(writers))
for pc, t, why in writers[:20]:
    print("     P %04X  %-34s (%s)" % (pc, t, why))
print()

print("=== (5) the READER -- can a computed pointer read one region as another? ===")
readers = [(pc, t) for pc, (w, t) in sorted(dec.items())
           if re.search(r"x:\(r[0-7]", t.lower()) and re.match(r"^move\s+x:", t.lower())]
print("  computed X reads in image I: %d" % len(readers))
for pc, t in readers:
    print("     P %04X  %s" % (pc, t))
print()
print("  VERDICT on (5): the regions are written by fixed r0 values at distinct call")
print("  sites, but a computed READER could in principle address any of them. Whether")
print("  one does is NOT established by this script -- it is reported as UNKNOWN and")
print("  must be closed by the packet's demand-driven slice from the doorbell output.")
