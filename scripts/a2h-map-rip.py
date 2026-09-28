"""Map the competitor's native RIP to a symbol, using the archived linker map.

The run's image base was 0x00007FF606630000 (read from the ledger), while the map's
preferred load address is 0x140000000. So the RVA is what matters:

    RVA = native_rip - image_base
    map address = 0x140000000 + RVA

Then find the map's public symbol whose address is the greatest one <= that, and report
the symbol and the offset within it. Both the symbol and the delta are printed so the
reader can judge whether the placement is exact (delta 0) or interior.
"""
import sys
from pathlib import Path

MAP = Path(sys.argv[1])
IMAGE_BASE = int(sys.argv[2], 16)
RIPS = [(int(a, 16), lab) for a, lab in
        (x.split("=") for x in sys.argv[3:])]

PREFERRED = 0x140000000

# Parse the "Publics by Value" section: lines like
#  0001:00000000       ??_C@_0...   0000000140001000     f   ...
# or the Rva+Base column. We take the FIRST 16-hex-digit token that looks like an address
# >= PREFERRED, plus the symbol token.
pub = []
in_pub = False
for line in MAP.read_text(encoding="utf-8", errors="replace").split("\n"):
    if "Publics by Value" in line:
        in_pub = True
        continue
    if not in_pub:
        continue
    if line.startswith(" ") is False and line.strip():
        # left the section
        if pub:
            break
        continue
    parts = line.split()
    if len(parts) < 3:
        continue
    # The map's column order is:  Address   Symbol   Rva+Base   Lib:Object
    # So the SYMBOL is parts[1] and the ADDRESS is parts[2]. Taking the first
    # 16-hex-digit token instead finds the "Lib:Object" or a mangled name and
    # silently reports the wrong symbol -- so index the columns explicitly.
    if len(parts) < 3:
        continue
    try:
        addr = int(parts[2], 16)
    except ValueError:
        continue
    sym = parts[1]
    if addr >= PREFERRED:
        pub.append((addr, sym))

print("  publics parsed: %d" % len(pub))
if not pub:
    raise SystemExit("no publics parsed -- map format differs")

pub.sort()
addrs = [a for a, _ in pub]

import bisect
for rip, label in RIPS:
    rva = rip - IMAGE_BASE
    want = PREFERRED + rva
    i = bisect.bisect_right(addrs, want) - 1
    if i < 0:
        print("  %-26s rip=%016X rva=0x%X -> BEFORE the first public" % (label, rip, rva))
        continue
    a, s = pub[i]
    delta = want - a
    nxt = pub[i + 1][0] if i + 1 < len(pub) else None
    span = (nxt - a) if nxt else None
    print("  %-26s rip=%016X" % (label, rip))
    print("      rva=0x%-10X map=%016X" % (rva, want))
    print("      symbol=%s" % s)
    print("      delta=+0x%X   next=+0x%X   span=%s" % (
        delta, (nxt - a) if nxt else -1, hex(span) if span else "?"))
    if delta == 0:
        print("      => EXACT: the RIP is the symbol's entry")
    elif span and delta < span:
        print("      => INTERIOR of that symbol (%d bytes in)" % delta)
    else:
        print("      => beyond the symbol's next neighbour -- placement suspect")
    print()
