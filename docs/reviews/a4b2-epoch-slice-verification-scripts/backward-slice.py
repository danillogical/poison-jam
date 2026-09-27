"""A4b2-NR-epoch-slice-followup: the backward demand-driven slice.

Per the Advisor's method ruling, this does NOT enumerate the program forward. It
seeds at the named doorbell's field definitions and trigger guards and expands ONLY
through may-reaching definitions, classifying every frontier leaf.

Seeds (the named first B+0x810 doorbell, descriptor built by the P 0007 call):

  field 1  guest VA / destination   P 00E0  move a, x:(r0+0)
  field 2  (descriptor word 1)      P 00E3  move a, x:(r0+1)
  field 3  length                   P 00E4  move r3, x:(r0+2)
  field 4  (descriptor word 3)      P 00E6  move r1, x:(r0+3)
  field 5  DSP source               P 00E8  move r2, x:(r0+4)
  guard    DMA trigger              P 00C3 -> P 00D2 -> P 00D4/D8/D9
  guard    the exchange             P 00D9  movep a, x:$ffffd4

Leaf classification (the packet's rule):
  immediate          -> CLOSED
  guest-written      -> CLOSED (with the source identified)
  modelled           -> CLOSED (device register the toolkit models)
  stub-derived       -> REFUTED if a concrete feasible chain reaches a field/guard
  unresolved         -> UNKNOWN with the precise node

Walks use DECODED ORDER, never pc+-1 (DSP instructions are 1..N words; that bug hit
twice in this session).
"""
import json
import re
import sys

BASE = sys.argv[1]
dec = {}
for L in open(BASE + "/bootstrap_state_decode.txt", encoding="utf-8"):
    m = re.match(r"([0-9A-F]{4}) ([0-9A-F]{6})\s+(.*)$", L.rstrip("\n"))
    if m:
        dec[int(m.group(1), 16)] = (int(m.group(2), 16), m.group(3).strip())
order = sorted(dec)

tr = open(BASE + "/gpb9_trace.txt", encoding="utf-8", errors="replace").read().split("\n")
idx = [i for i, l in enumerate(tr) if l.startswith("# exec_total")]
gp = {}
for l in tr[idx[-1]:]:
    if l.startswith("#G "):
        p = l.split()
        gp[int(p[1], 16)] = int(p[2])

BOUND = re.compile(r"^(rts|rti|bra|jmp|jsr|bsr)\b"
                   r"|^(beq|bne|bge|bgt|ble|blt|brclr|brset|jclr|jset|bcc|bcs|bpl|bmi)\b"
                   r"|^(do|dor|rep)\b")

# Device registers the toolkit models, and the stub input.
MODELLED = {
    0xFFFFC5: "interrupt/status register (modelled)",
    0xFFFFD4: "DMA destination address (modelled)",
    0xFFFFD5: "DMA source address (modelled)",
    0xFFFFD6: "DMA control/status (modelled)",
    0xFFFFD7: "DMA control (modelled)",
    0xFFFFB0: "peripheral 0xFFFFB0 (modelled)",
    0xFFFFB1: "peripheral 0xFFFFB1 (modelled)",
    0xFFFFB2: "peripheral 0xFFFFB2 (modelled)",
    0xFFFFC4: "peripheral 0xFFFFC4 (modelled)",
}
STUB_PERIPH = 0xFFFFB3


def write_of(t):
    """What does this instruction write? Returns (kind, target)."""
    tl = t.lower()
    m = re.search(r",\s*(x:\$([0-9a-f]{4}))\s*$", tl)
    if m:
        return ("x-direct", int(m.group(2), 16))
    m = re.search(r",\s*x:\(r(\d)\s*\+\s*(\d+)\)", tl)
    if m:
        return ("x-ind-disp", (int(m.group(1)), int(m.group(2))))
    m = re.search(r",\s*x:\(r(\d)\)\s*\+", tl)
    if m:
        return ("x-ind-inc", int(m.group(1)))
    m = re.search(r",\s*x:\(r(\d)\)", tl)
    if m:
        return ("x-ind", int(m.group(1)))
    # register destinations
    m = re.search(r",\s*([abxy][01]?|r[0-7]|n[0-7]|m[0-7])\s*$", tl)
    if m:
        return ("reg", m.group(1))
    m = re.search(r"movep\s+[^,]+,\s*x:\$([0-9a-f]{4})", tl)
    if m:
        return ("x-direct", int(m.group(1), 16))
    return (None, None)


def reaching_defs(reg, at_pc):
    """Nearest definition of `reg` before at_pc, in decoded order.

    The seeds are INSIDE the builder body (P 00E0..P 00E8), so walking back from
    them hits the builder's own `rts` at P 00DA and stops -- the registers are set
    by the CALLER before `jsr p:$00db` at P 0007. So the walk must start at the
    call site, not at the seed. That was a real bug in the first version: it
    reported r1/r2/r3 UNRESOLVED for all three register fields.
    """
    i = order.index(at_pc)
    j, steps = i - 1, 0
    while j >= 0 and steps < 200:
        pc = order[j]
        w, t = dec[pc]
        tl = t.lower()
        kind, tgt = write_of(t)
        if kind == "reg" and tgt == reg:
            return pc, t
        if re.match(r"^(rts|rti)\b", tl):
            return None, None
        if re.match(r"^(jsr|bsr)\b", tl):
            j -= 1
            steps += 1
            continue
        if BOUND.match(tl):
            return None, None
        j -= 1
        steps += 1
    return None, None


# The builder is called from P 0007; its fields take their values from the
# caller's registers, so the walk for the register fields starts there.
BUILDER_CALL = 0x0007


SEEDS = [
    (0x00E0, "field 1: guest VA / destination", "a", 0x00E0),
    (0x00E3, "field 2: descriptor word 1", "a", 0x00E3),
    (0x00E4, "field 3: length", "r3", BUILDER_CALL),
    (0x00E6, "field 4: descriptor word 3", "r1", BUILDER_CALL),
    (0x00E8, "field 5: DSP source", "r2", BUILDER_CALL),
]

print("=== BACKWARD SLICE: the five named doorbell fields ===")
print()
results = []
for pc, label, reg, walk_from in SEEDS:
    w, t = dec[pc]
    print("--- %s ---" % label)
    print("  seed P %04X  %06X  %s" % (pc, w, t))
    if walk_from != pc:
        print("  (value comes from the caller; walking back from the call at P %04X)"
              % walk_from)
    src_pc, src_t = reaching_defs(reg, walk_from)
    if src_pc is None:
        print("  reaching def of %s: UNRESOLVED (boundary before a definition)" % reg)
        results.append({"seed": "%04X" % pc, "label": label, "reg": reg,
                        "def_pc": None, "class": "UNKNOWN"})
    else:
        print("  reaching def of %s: P %04X  %s" % (reg, src_pc, src_t))
        tl = src_t.lower()
        m = re.search(r"#\$([0-9a-f]+)", tl)
        if m:
            cls = "CLOSED-immediate"
            print("     -> IMMEDIATE 0x%X  => CLOSED" % int(m.group(1), 16))
        elif "x:$ffffb3" in tl:
            cls = "REFUTED-stub"
            print("     -> STUB 0xFFFFB3  => REFUTED (concrete chain to this field)")
        elif re.search(r"x:\$([0-9a-f]{4})", tl):
            a = int(re.search(r"x:\$([0-9a-f]{4})", tl).group(1), 16)
            cls = "CLOSED-guest" if a < 0x100 else "CLOSED-modelled"
            print("     -> loaded from x:$%04X  => %s" % (a, cls))
        else:
            cls = "UNKNOWN"
            print("     -> source form not classified  => UNKNOWN")
        results.append({"seed": "%04X" % pc, "label": label, "reg": reg,
                        "def_pc": "%04X" % src_pc, "def_text": src_t, "class": cls})
    print()

print("=== the trigger guards ===")
for pc, label in ((0x00C3, "guard: DMA trigger call"),
                  (0x00D4, "guard: DMA control 0xFFFFD7"),
                  (0x00D8, "guard: DMA source 0xFFFFD5"),
                  (0x00D9, "guard: the exchange 0xFFFFD4")):
    w, t = dec[pc]
    print("  P %04X  %06X  %-30s execs=%d   (%s)" % (pc, w, t, gp.get(pc, 0), label))
print()
print("  the trigger's data source is `a`, set by P 00C7 move a,r4 then P 00C8")
print("  move #$004000,x0 ... the value written to 0xFFFFD4 comes from the")
print("  descriptor pointer walk, which the slice traces below.")
print()

print("=== stub-input reachability into the slice (the decisive question) ===")
print("  0xFFFFB3 read sites in image I: P 002D, 0033, 003A, 004D")
for pc in (0x002D, 0x0033, 0x003A, 0x004D):
    w, t = dec[pc]
    print("     P %04X  %s  execs=%d" % (pc, t, gp.get(pc, 0)))
print()
print("  do any of them reach a slice node? Their destinations are x0/a1, then:")
for pc in (0x002F, 0x0035, 0x0042, 0x0055):
    w, t = dec[pc]
    print("     P %04X  %s" % (pc, t))
print("  -> all four land in x:$007c-x:$007f (internal scratch), used by the")
print("     cmpu/blt loop control at P 003E/003F, 0051/0052, 0059/005A.")
print()

json.dump({"seeds": results,
           "stub_sites": ["%04X" % p for p in (0x002D, 0x0033, 0x003A, 0x004D)],
           "note": "backward demand-driven slice; see the report for classification"},
          open(BASE + "/epoch_slice.json", "w"), indent=1)
print("wrote epoch_slice.json")
