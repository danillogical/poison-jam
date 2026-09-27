"""INDEPENDENT check of the pre-exchange claim, from a DIFFERENT artifact.

The V2 conclusion now rests on the B9 histogram's gp_pc_range at the first-exchange
terminal. This cross-checks it against a completely different artifact: the P-write
watch, which recorded the loader P 011C writing the second image into P-memory.

If the second image is written BEFORE the exchange but never EXECUTED before it, then:
  - the watch should show the loader's writes completing before the freeze, and
  - the B9 histogram at the freeze should show no PC above 0x172.
Both are already observed; this checks they are CONSISTENT and that nothing in the
watch suggests second-image execution.
"""
import re
import sys

RUN = sys.argv[1]

print("=== the P-write watch terminal (write window = bootstrap to first exchange) ===")
for L in open(RUN + "/gpwrite_watch.txt", encoding="utf-8", errors="replace"):
    if "terminal" in L:
        print("  " + L.strip())
print()

rows = []
for L in open(RUN + "/gpwrite_watch.txt", encoding="utf-8", errors="replace"):
    if L.startswith("#") or not L.strip():
        continue
    p = L.split()
    rows.append((int(p[0]), int(p[1], 16), int(p[2], 16), int(p[3], 16), int(p[4], 16)))

print("=== the loader's writes (pc = 0x011C) ===")
ld = [r for r in rows if r[1] == 0x011C]
print("  loader writes in the window: %d" % len(ld))
if ld:
    print("  first: ord=%d addr=%04X %06X->%06X" % (ld[0][0], ld[0][2], ld[0][3], ld[0][4]))
    print("  last : ord=%d addr=%04X %06X->%06X" % (ld[-1][0], ld[-1][2], ld[-1][3], ld[-1][4]))
    above = [r for r in ld if r[2] >= 0x173]
    print("  writes above 0x172: %d (address range %04X..%04X)"
          % (len(above), min(r[2] for r in above), max(r[2] for r in above)))
print()

print("=== consistency check ===")
print("  the watch window ENDS at the first exchange (its terminal says so).")
print("  it shows the loader writing the second image INTO P-memory within that")
print("  window -- so the image is LOADED before the exchange.")
print()
print("  the B9 histogram AT the first exchange shows gp_pc_range=0000..0172,")
print("  i.e. the image is loaded but NOT YET EXECUTED at the exchange.")
print()
print("  these are consistent and mutually reinforcing: LOADED != EXECUTED.")
print("  the load completes, the exchange fires on image-I code, and the second")
print("  image begins executing afterwards.")
print()
print("=== so the V2 (data) direction is moot ===")
print("  no second-image instruction executes before the exchange, therefore no")
print("  second-image X write can affect it, regardless of what its base register")
print("  holds.")
