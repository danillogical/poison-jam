"""Extract EXACTLY the histogram block that follows the first-exchange terminal.

The dump format is: terminal line, then a header, then #H lines, then #G lines.
Each dump is terminated by the NEXT terminal line. This extracts precisely one block,
so the pre-exchange execution record is unambiguous.
"""
import sys

BASE = sys.argv[1]
lines = open(BASE + "/gpb9_trace.txt", encoding="utf-8", errors="replace").read().split("\n")

fx = [i for i, l in enumerate(lines) if "reason=first-exchange" in l][0]
print("=== the first-exchange terminal (line %d) ===" % fx)
print("  " + lines[fx].strip())
print()

# the dump follows the terminal; it ends at the next terminal line
end = None
for j in range(fx + 1, len(lines)):
    if "terminal reason=" in lines[j]:
        end = j
        break
if end is None:
    end = len(lines)

block = lines[fx + 1:end]
hdr = [l for l in block if l.startswith("# exec_total")]
gp = [int(l.split()[1], 16) for l in block if l.startswith("#G ")]
h = [int(l.split()[1], 16) for l in block if l.startswith("#H ")]

print("=== the block immediately after it (lines %d..%d) ===" % (fx + 1, end - 1))
for l in hdr:
    print("  " + l.strip())
print()
print("  #G entries (GP PCs with nonzero count): %d" % len(gp))
print("  #H entries (all-core PCs): %d" % len(h))
if gp:
    print("  GP PC range: %04X..%04X" % (min(gp), max(gp)))
    above = [p for p in gp if p >= 0x173]
    print("  GP PCs >= 0x173: %d" % len(above))
    if above:
        print("    %s" % " ".join("%04X" % p for p in sorted(above)[:24]))
    else:
        print("    NONE")
print()

print("=== the header's own range fields, which are unambiguous ===")
for l in hdr:
    import re
    m = re.search(r"gp_pc_range=([0-9A-F]{4})\.\.([0-9A-F]{4})", l)
    f = re.search(r"gp_first_high=([0-9A-F]{4})", l)
    e = re.search(r"exec_total=(\d+)", l)
    if m:
        print("  exec_total=%s  gp_pc_range=%s..%s  gp_first_high=%s"
              % (e.group(1), m.group(1), m.group(2), f.group(1) if f else "?"))
        lo, hi = int(m.group(1), 16), int(m.group(2), 16)
        if hi < 0x173:
            print("  -> the GP had executed ONLY image I at the exchange.")
        else:
            print("  -> the GP had executed ABOVE image I at the exchange.")
print()

print("=== and the NEXT dump, for contrast ===")
nxt = [i for i, l in enumerate(lines) if l.startswith("# exec_total") and i > end]
if nxt:
    print("  " + lines[nxt[0]].strip())
