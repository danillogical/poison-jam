"""V2 residual: resolve the 296 computed X writes in the second image.

The Advisor's V2 requires (data) second-image X-write destinations vs slice-read
words. Direct writes are clean (0 hits). The computed ones depend on a register, so
each must be resolved or reported as a precise UNKNOWN.

The slice-read words are the exchange descriptor region [0x18..0x1C] = [24..28]dec,
the P 0007 descriptor [6..10], and the guard/shared words {0x1E, 0x24, 0x25}.

Strategy: for each computed X-write site, find the nearest write to the base register
in decoded order, stepping over calls, and resolve to an immediate if possible. Group
by register and report the distribution of resolved values against the slice words.
"""
import re
import sys
from collections import Counter, defaultdict

BASE = sys.argv[1]
RUN = sys.argv[2]

log = open(RUN + "/jsrf_run.log", encoding="utf-8", errors="replace").read().split("\n")
b = [i for i, l in enumerate(log) if "second-decode-at-exchange begin" in l][0]
e = [i for i, l in enumerate(log) if "second-decode-at-exchange end" in l][0]
second = {}
for l in log[b:e]:
    m = re.match(r".*\[GPDECODE\] P ([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", l)
    if m:
        second[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())
order = sorted(second)
next_pc = {pc: (order[i + 1] if i + 1 < len(order) else None)
           for i, pc in enumerate(order)}

SLICE_X = set(range(0x18, 0x1D)) | set(range(6, 11)) | {0x1E, 0x24, 0x25}

BOUND = re.compile(r"^(rts|rti|bra|jmp|jsr|bsr)\b"
                   r"|^(beq|bne|bge|bgt|ble|blt|brclr|brset|jclr|jset|bcc|bcs|bpl|bmi)\b"
                   r"|^(do|dor|rep)\b")

# collect computed X writes above 0x173
sites = []
for pc in order:
    if pc < 0x173:
        continue
    w, t = second[pc]
    tl = t.lower()
    for m in re.finditer(r",\s*x:\(r(\d)(?:\s*\+\s*(\d+))?\)(\+)?", tl):
        sites.append((pc, t, int(m.group(1)),
                      int(m.group(2)) if m.group(2) else 0))
print("=== computed X-write sites in the second image: %d ===" % len(sites))
byreg = Counter(r for _, _, r, _ in sites)
print("  by base register: %s" % dict(byreg))
print()

# distinct (register, displacement) forms
forms = Counter((r, d) for _, _, r, d in sites)
print("=== distinct forms ===")
for (r, d), n in sorted(forms.items()):
    print("  x:(r%d+%d)  x%d" % (r, d, n))
print()


def nearest_imm(reg, at_pc, limit=200):
    """Nearest write to `reg` before at_pc, stepping over calls; immediate or not."""
    i = order.index(at_pc)
    j, steps = i - 1, 0
    while j >= 0 and steps < limit:
        pc = order[j]
        w, t = second[pc]
        tl = t.lower()
        m = re.search(r"#\$([0-9a-f]+)\s*,\s*r%d\b" % reg, tl)
        if m:
            return pc, t, int(m.group(1), 16)
        if re.search(r",\s*r%d\s*$" % reg, tl) or re.search(r"move\s+r%d\s*," % reg, tl):
            return pc, t, None
        if re.match(r"^(rts|rti)\b", tl):
            return None, None, None
        if re.match(r"^(jsr|bsr)\b", tl):
            j -= 1
            steps += 1
            continue
        if BOUND.match(tl):
            return None, None, None
        j -= 1
        steps += 1
    return None, None, None


print("=== resolving each site's base register ===")
resolved = Counter()
unresolved = []
hits = []
for pc, t, reg, disp in sites:
    src, st, val = nearest_imm(reg, pc)
    if val is None:
        unresolved.append((pc, t, reg, disp, src, st))
        resolved["UNRESOLVED"] += 1
    else:
        lo = val + disp
        hi = lo  # a single word; post-increment handled below
        if lo in SLICE_X:
            hits.append((pc, t, reg, val, disp, lo))
            resolved["HIT_SLICE"] += 1
        else:
            resolved["outside"] += 1

print("  %s" % dict(resolved))
print()
print("=== sites whose resolved destination lands in a SLICE word ===")
for pc, t, reg, val, disp, lo in hits:
    print("  P %04X  x:(r%d+%d) with r%d=0x%X -> x:[0x%X]  SLICE   %s"
          % (pc, reg, disp, reg, val, lo, t))
if not hits:
    print("  NONE")
print()

print("=== sites whose base register could not be resolved to an immediate ===")
print("  count: %d" % len(unresolved))
byr = Counter(r for _, _, r, _, _, _ in unresolved)
print("  by register: %s" % dict(byr))
for pc, t, reg, disp, src, st in unresolved[:20]:
    print("    P %04X  x:(r%d+%d)   base from P %s %s"
          % (pc, reg, disp,
             "%04X" % src if src else "????", st or ""))
if len(unresolved) > 20:
    print("    ... and %d more" % (len(unresolved) - 20))
print()

print("=== V2 (data) VERDICT ===")
print("  computed X-write sites        : %d" % len(sites))
print("  resolved to a non-slice word  : %d" % resolved["outside"])
print("  resolved INTO a slice word    : %d" % resolved["HIT_SLICE"])
print("  UNRESOLVED (base not imm)     : %d" % resolved["UNRESOLVED"])
print()
if resolved["HIT_SLICE"] == 0 and resolved["UNRESOLVED"] == 0:
    print("  CLEAN: every computed second-image X write resolves outside the slice words.")
elif resolved["HIT_SLICE"] > 0:
    print("  NOT CLEAN: a computed write resolves into a slice word.")
else:
    print("  RESIDUAL: %d site(s) have a non-immediate base -> precise UNKNOWN."
          % resolved["UNRESOLVED"])
