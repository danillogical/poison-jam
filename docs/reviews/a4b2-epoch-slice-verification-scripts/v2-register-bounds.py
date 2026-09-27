"""V2 (data) residual: bound every base register of the 296 computed X writes.

Slice-read words are 6..10, 0x18..0x1C, 0x1E, 0x24, 0x25 -- ALL <= 0x25 (37).
So a computed write can only reach a slice word if its base register is small.

For each base register used by a second-image computed X write, this finds every
write to that register in the second image and reports the minimum reachable value.
If the minimum (plus the smallest displacement) exceeds 37, the register's writes
cannot reach a slice word -- a clean resolution.
"""
import re
import sys
from collections import defaultdict

RUN = sys.argv[1]
log = open(RUN + "/jsrf_run.log", encoding="utf-8", errors="replace").read().split("\n")
b = [i for i, l in enumerate(log) if "second-decode-at-exchange begin" in l][0]
e = [i for i, l in enumerate(log) if "second-decode-at-exchange end" in l][0]
second = {}
for l in log[b:e]:
    m = re.match(r".*\[GPDECODE\] P ([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", l)
    if m:
        second[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())
order = sorted(second)
above = [pc for pc in order if pc >= 0x173]

MAX_SLICE = max(0x25, 0x1C, 10)   # 37

print("slice-read words are all <= 0x%02X (%d)" % (MAX_SLICE, MAX_SLICE))
print()

# collect the (register, min displacement) used by computed X writes
need = defaultdict(lambda: 10**9)
for pc in above:
    t = second[pc][1]
    for m in re.finditer(r",\s*x:\(r(\d)(?:\s*\+\s*(\d+))?\)", t.lower()):
        r = int(m.group(1))
        d = int(m.group(2)) if m.group(2) else 0
        need[r] = min(need[r], d)

print("=== base registers used, with their minimum displacement ===")
for r in sorted(need):
    print("  r%d : min displacement %d" % (r, need[r]))
print()

print("=== every write to each needed register, in the second image ===")
for r in sorted(need):
    pat_imm = re.compile(r"#\$([0-9a-f]+)\s*,\s*r%d\b" % r)
    pat_any = re.compile(r",\s*r%d\s*$" % r)
    imms, others = [], []
    for pc in above:
        t = second[pc][1]
        tl = t.lower()
        m = pat_imm.search(tl)
        if m:
            imms.append((pc, int(m.group(1), 16), t))
        elif pat_any.search(tl) or re.search(r"move\s+r%d\s*," % r, tl):
            others.append((pc, t))
    print("  --- r%d ---" % r)
    if imms:
        lo = min(v for _, v, _ in imms)
        hi = max(v for _, v, _ in imms)
        print("    immediate writes: %d, values 0x%X .. 0x%X" % (len(imms), lo, hi))
        reach = lo + need[r]
        verdict = ("CANNOT reach a slice word (min 0x%X > 0x%X)" % (reach, MAX_SLICE)
                   if reach > MAX_SLICE else
                   "*** CAN reach a slice word (min 0x%X) ***" % reach)
        print("    min reachable destination: 0x%X  -> %s" % (reach, verdict))
    if others:
        print("    NON-immediate writes: %d" % len(others))
        for pc, t in others[:8]:
            print("      P %04X  %s" % (pc, t))
        if len(others) > 8:
            print("      ... and %d more" % (len(others) - 8))
    if not imms and not others:
        print("    NO writes found in the second image -> base comes from image I")
    print()
