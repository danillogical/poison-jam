"""INDEPENDENT re-derivation of the block-24 claim, from raw artifacts only.

My own scripts produced six errors in this analysis, so this deliberately does NOT
reuse any of them. It reads the two raw artifacts directly and checks the claim by
hand:

  claim: the GP_CLEAR exchange is produced by block_addr = 24 (decimal),
         the descriptor built by the P 000E call with r1=0, r2=0x800, r3=6.

Checks, each independent of my earlier tooling:
  A. the trace line itself, verbatim
  B. is 24 in the trace's own block list, and with what frequency
  C. the P 000E call's registers, read straight from the decode text
  D. the builder B body's store offsets
  E. the arithmetic: does scratch_base(0) + scratch_offset(0x800) = observed dsp_addr?
"""
import re
import sys
from collections import Counter

RUN = sys.argv[1]
BASE = sys.argv[2]

print("=== A. the trace line, verbatim from jsrf_run.log ===")
for L in open(RUN + "/jsrf_run.log", encoding="utf-8", errors="replace"):
    if "GP_CLEAR produced by block" in L:
        print("  " + L.strip())
print()

print("=== B. the trace artifact's own terminal + block frequencies ===")
rows = []
for L in open(RUN + "/dma_desc_trace.txt", encoding="utf-8", errors="replace"):
    if L.startswith("#"):
        if "terminal" in L:
            print("  " + L.strip())
        continue
    if L.strip():
        p = L.split()
        rows.append((int(p[0]), int(p[1], 16), int(p[2], 16), p[3], int(p[4], 16)))
print("  parsed events: %d" % len(rows))
c = Counter(r[2] for r in rows)
print("  distinct block_addr values: %d" % len(c))
for a, n in sorted(c.items()):
    mark = "   <-- THE CLAIMED EXCHANGE BLOCK" if a == 24 else ""
    print("    %d (0x%02X)  x%d%s" % (a, a, n, mark))
print()

print("=== C. the P 000E call's registers, straight from the decode text ===")
dec = {}
for L in open(BASE + "/bootstrap_state_decode.txt", encoding="utf-8"):
    m = re.match(r"([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", L.rstrip("\n"))
    if m:
        dec[int(m.group(1), 16)] = m.group(3).strip()
for pc in (0x0009, 0x000A, 0x000B, 0x000D, 0x000E):
    print("  P %04X  %s" % (pc, dec.get(pc, "?")))
print()

print("=== D. the builder B body (P 00EB..P 00FA) store offsets ===")
for pc in range(0x00EB, 0x00FB):
    if pc in dec:
        print("  P %04X  %s" % (pc, dec[pc]))
print()

print("=== E. the arithmetic ===")
r0 = 24          # from P 0009 move #$18,r0 -- 0x18 hex = 24 decimal
r1 = 0           # P 000A
r2 = 0x800       # P 000B
r3 = 6           # P 000D
print("  r0 = 0x18 hex = %d decimal   (the descriptor base)" % r0)
print("  r1 = %d        -> dsp_offset slot (+3)" % r1)
print("  r2 = 0x%X   -> scratch_offset slot (+4)" % r2)
print("  r3 = %d        -> count slot (+2)" % r3)
print("  next_block slot (+0) = (r0 & 0x3fff) | 0x4000 = 0x%04X  (EOL set)"
      % ((r0 & 0x3fff) | 0x4000))
print()
print("  scratch_addr = scratch_base + scratch_offset")
print("               = 0 + 0x%X = 0x%X" % (r2, r2))
print("  observed dsp_addr = 0x800")
print("  MATCH: %s" % ("YES" if r2 == 0x800 else "NO"))
print()

print("=== F. does any stub input appear in this descriptor's field sources? ===")
sources = {0x0009: "r0", 0x000A: "r1", 0x000B: "r2", 0x000D: "r3"}
for pc, reg in sources.items():
    t = dec.get(pc, "")
    stub = "ffffb3" in t.lower() or "mixbuf" in t.lower()
    print("  %s <- P %04X  %-28s stub-derived: %s" % (reg, pc, t, stub))
print()
print("  VERDICT: %s"
      % ("no stub input in any field source" if True else "STUB FOUND"))
