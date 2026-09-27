"""V2 — cross-boundary enumeration over at-exchange second-image bytes, BOTH directions.

Advisor-ordered. Static artifacts only; no new runs, no new packet.

Three directions, as specified:

  (out) image-I transfers targeting >= 0x173 -- do they fall on the doorbell path?
        do their returns clobber slice state?
  (in)  second-image transfers targeting slice nodes OR the dead-block PCs
        (a second-image -> dead-block edge plus dead-block writes would be a
        two-hop chain; the dead blocks' image-I unreachability does NOT cover it)
  (data) second-image X-write destinations vs slice-read words
        ([24..28] dec = [0x18..0x1C], guard words, shared-state words)

Any live edge/write, or anything indirect/unresolvable -> precise UNKNOWN -> re-refer.
"""
import json
import re
import sys

BASE = sys.argv[1]

# ---- image I decode
I = {}
for L in open(BASE + "/bootstrap_state_decode.txt", encoding="utf-8"):
    m = re.match(r"([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", L.rstrip("\n"))
    if m:
        I[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())

# ---- slice nodes: the exchange descriptor, its guard words, shared state
SLICE_PCS = {0x0000, 0x0002, 0x0003, 0x0004, 0x0006, 0x0007, 0x0009, 0x000A,
             0x000B, 0x000D, 0x000E, 0x00C1, 0x00C3, 0x00D2, 0x00D4, 0x00D6,
             0x00D8, 0x00D9, 0x00DA, 0x00EB, 0x00F0, 0x00F1, 0x00F3, 0x00F4,
             0x00F6, 0x00F8, 0x00FA}
DEAD_BLOCK_PCS = set(range(0x0133, 0x0144)) | set(range(0x0144, 0x0155))
# slice-read/written X words: the exchange descriptor region [0x18..0x1C] = [24..28]dec,
# the P 0007 descriptor [6..10], the guard/shared words
SLICE_X = set(range(0x18, 0x1D)) | set(range(6, 11)) | {0x24, 0x25, 0x1E}

print("=== the V2 universe ===")
print("  image I instructions      : %d" % len(I))
print("  slice PCs                 : %d" % len(SLICE_PCS))
print("  dead-block PCs            : %d" % len(DEAD_BLOCK_PCS))
print("  slice X words             : %s" % sorted("0x%02X" % a for a in SLICE_X))
print()

# ============================================================ (out) direction
print("=== (out) image-I transfers targeting >= 0x173 ===")
out = []
for pc in sorted(I):
    t = I[pc][1]
    for m in re.finditer(r"p:\$([0-9a-f]{1,6})", t.lower()):
        tg = int(m.group(1), 16)
        if tg >= 0x173:
            out.append((pc, t, tg))
print("  static p: targets >= 0x173 from image I: %d" % len(out))
for pc, t, tg in out:
    print("    P %04X -> 0x%04X   %s" % (pc, t, tg))
if not out:
    print("    NONE -- image I makes no static code transfer above 0x172.")
print()

# indirect/computed transfers from image I
print("  computed/indirect transfers in image I:")
ind = [(pc, I[pc][1]) for pc in sorted(I)
       if re.match(r"^(jmp|jsr)\b.*(\(|,\s*[rxy][0-7]\b)", I[pc][1].lower())]
print("    count: %d" % len(ind))
for pc, t in ind:
    print("      P %04X  %s" % (pc, t))
print()

# ============================================================= (in) direction
print("=== (in) second-image transfers targeting slice nodes or dead-block PCs ===")
# The second image is at-exchange code above 0x173. Its static targets must be
# enumerated from the at-exchange decode.
RUN = sys.argv[2]
log = open(RUN + "/jsrf_run.log", encoding="utf-8", errors="replace").read().split("\n")
b = [i for i, l in enumerate(log) if "second-decode-at-exchange begin" in l]
e = [i for i, l in enumerate(log) if "second-decode-at-exchange end" in l]
second = {}
if b and e:
    for l in log[b[0]:e[0]]:
        m = re.match(r".*\[GPDECODE\] P ([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", l)
        if m:
            second[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())
print("  at-exchange second-image instructions: %d" % len(second))
above = {pc: v for pc, v in second.items() if pc >= 0x173}
print("  of those, PCs >= 0x173: %d" % len(above))
print()

inbound = []
for pc, (w, t) in sorted(above.items()):
    for m in re.finditer(r"p:\$([0-9a-f]{1,6})", t.lower()):
        tg = int(m.group(1), 16)
        if tg in SLICE_PCS or tg in DEAD_BLOCK_PCS:
            inbound.append((pc, t, tg, "slice" if tg in SLICE_PCS else "dead-block"))
print("  second-image -> slice-node or dead-block transfers: %d" % len(inbound))
for pc, t, tg, kind in inbound:
    print("    P %04X -> 0x%04X  (%s)   %s" % (pc, tg, kind, t))
if not inbound:
    print("    NONE -- no second-image instruction statically targets a slice node")
    print("    or a dead-block PC.")
print()

# =========================================================== (data) direction
print("=== (data) second-image X-write destinations vs slice-read words ===")
data = []
for pc, (w, t) in sorted(above.items()):
    tl = t.lower()
    for m in re.finditer(r",\s*x:\$([0-9a-f]{4})", tl):
        a = int(m.group(1), 16)
        data.append((pc, t, a, a in SLICE_X))
    for m in re.finditer(r",\s*x:\(r(\d)(?:\s*\+\s*(\d+))?\)", tl):
        data.append((pc, t, None, None))
direct = [d for d in data if d[2] is not None]
computed = [d for d in data if d[2] is None]
print("  second-image X writes: %d total (%d direct, %d computed)"
      % (len(data), len(direct), len(computed)))
print("  direct writes landing in a slice-read word: %d"
      % len([d for d in direct if d[3]]))
for pc, t, a, hit in direct:
    mark = "   <<< SLICE WORD" if hit else ""
    print("    P %04X  x:$%04X  %s%s" % (pc, a, t, mark))
print()
print("  computed X writes (destination depends on a register): %d" % len(computed))
for pc, t, _, _ in computed[:20]:
    print("    P %04X  %s" % (pc, t))
if len(computed) > 20:
    print("    ... and %d more" % (len(computed) - 20))
print()

print("=== V2 VERDICT ===")
live = len(inbound) + len([d for d in direct if d[3]])
print("  (out) static image-I transfers above 0x172 : %d" % len(out))
print("  (out) computed transfers in image I        : %d" % len(ind))
print("  (in)  second-image -> slice/dead transfers : %d" % len(inbound))
print("  (data) second-image direct writes to slice : %d"
      % len([d for d in direct if d[3]]))
print("  (data) second-image COMPUTED X writes      : %d" % len(computed))
print()
if live == 0 and not computed:
    print("  V2 CLEAN on static evidence: no live cross-boundary edge or write.")
else:
    print("  V2 NOT CLEAN: %d live edge(s)/write(s), %d computed write(s) unresolved."
          % (live, len(computed)))
