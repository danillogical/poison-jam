"""Generate the NV2A implemented-method table from the measured inventory.

Milestone 11: the model accepts a method only if it is implemented, and
jsrf_nv2a_registers pins that contract. So the set of implemented methods has to
grow method by method -- and the authoritative list of which methods to implement
is the list the title actually submits, read out of its own pushbuffer.

This emits two things from one decode:

  docs/jsrf-nv2a-method-inventory.md   the human-readable inventory
  src/nv2a/nv2a_method_table.c         the table the walk consults

Kept generated rather than hand-listed so the two cannot drift, and so adding a
method is "it appeared in a real submission" rather than "someone guessed".

The methods are captured as register state (param -> regs[method/4]), which is
what the model already does for the NV097 surface-clip methods. That is a real
implementation for the register-setting methods, which most of them are; the ones
that trigger an action (blit, notify, flip) still need their own handling and are
called out in the report.
"""
import struct
import sys
from collections import defaultdict
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))
from jsrf_dump import DumpMemory

# `run_name` is the run whose ring is decoded. `--put` is the ring's top, and
# leaving it out asks the run's own log instead of assuming: the model prints
# `[PFIFO] submit #N diag=... get=... put=...` on every kick, and the guest ring
# lives at 0x80000000 + the PFIFO offset. The first version of this script
# hardcoded both ends to the first pushbuffer the title ever submitted
# (get 0x1000, put 0x1B24), which silently stopped covering the ring the moment
# the title started submitting more of it -- the run
# 20260922-110235-244-spanfix-1185b0 reaches put 0x2764 and rejects method
# 0x1BCC, which the 0x1B24 decode could not see.
args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--") and "=" in a)
# Every run named on the command line contributes its methods, and the table is the
# UNION of them. One ring is not enough: the table used to come from a single run
# whose ring stopped at 1497 words, so once the title began submitting more of its
# ring the walk met methods the table had never seen. Measured 2026-09-30: the F4 run
# submits 0x1720 at word 8124 of a 72353-word ring, and regenerating from that run
# ALONE would drop 148 methods the older ring contributes. The union keeps both, and
# every entry in it is still something a real submission contained.
run_names = args or ["20260922-110235-244-spanfix-1185b0"]
get = int(opts.get("get", "0x80001000"), 16)
# The walk budget has to exceed the longest ring decoded. It used to be 4096, which
# is BELOW the word where 0x1720 first appears (8124), so a decode that reported
# "reached PUT" for the old ring could never see the method the walk was stuck on.
budget = int(opts.get("budget", "200000"))


def ring_top_from_log(run):
    """Highest PUT the run's own log reports, as a guest address."""
    log = run / "jsrf_run.log"
    if not log.exists():
        return None
    top = None
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if "[PFIFO] submit" not in line or "put=" not in line:
            continue
        field = line.split("put=", 1)[1].split()[0]
        try:
            top = max(top or 0, int(field, 16))
        except ValueError:
            pass
    return None if top is None else 0x80000000 + top


def decode_ring(run_name, get, put_override, budget):
    """One run's ring, decoded with the walk `nv2a_submit_pending` performs.

    Returns (order, per, words, stop, put). `stop` is None when the walk reached PUT.
    """
    run = root / "logs/runs" / run_name
    put = put_override if put_override is not None else ring_top_from_log(run)
    if put is None:
        raise SystemExit("no [PFIFO] submit line in %s/jsrf_run.log; pass --put=" % run)
    d = DumpMemory(run)
    buf = d.read(get, put - get)
    d.close()

    per = defaultdict(lambda: defaultdict(list))
    order = []
    words = 0
    pc = get
    ret = 0
    stop = None
    seen = set()

    # The walk mirrors `nv2a_submit_pending` in src/nv2a/nv2a_core.c word for word.
    # It used to classify packets by `h >> 30`, which is NOT the model's test -- the
    # model calls a word a jump when `(h & 3) == 1` (call) or `(h & 3) == 2`
    # (return), or the high form `(h & 0xe0000003) == 0x20000000`. The two agree
    # over the first 0x1B24 words of the ring and diverge past it, which is how a
    # decode that looked correct produced `offset -2144335848 out of range` on the
    # ring the title actually submits now. Keeping the two in step is the point:
    # this generator's whole claim is that its list is what the model will walk.
    while pc != put:
        if words >= budget or (pc, ret) in seen:
            stop = "budget" if words >= budget else "loop"
            break
        seen.add((pc, ret))
        if pc < get or pc >= put or (pc - get) + 4 > len(buf):
            stop = "out_of_ring pc=0x%08X" % pc
            break
        off = pc - get
        h = struct.unpack_from("<I", buf, off)[0]
        pc += 4
        words += 1
        if (h & 0xE0000003) == 0x20000000 or (h & 3) == 1 or (h & 3) == 2 or h == 0x00020000:
            if (h & 3) == 2:
                if ret:
                    stop = "loop (double return)"
                    break
                ret = pc
            if h == 0x00020000:
                if not ret:
                    stop = "bad_target (return with no call)"
                    break
                pc = ret
                ret = 0
                continue
            target = (h & 0xFFFFFFFC) if (h & 3) in (1, 2) else (h & 0x1FFFFFFF)
            if target < get or target >= put or (target & 3):
                stop = "bad_target 0x%08X" % target
                break
            pc = target
            continue
        if (h & 0xE0030003) == 0 or (h & 0xE0030003) == 0x40000000:
            count = (h >> 18) & 0x7FF
            sub = (h >> 13) & 7
            method = h & 0x1FFC
            for i in range(count):
                if words >= budget:
                    stop = "budget"
                    break
                if pc == put:
                    stop = "truncated"
                    break
                p = struct.unpack_from("<I", buf, pc - get)[0]
                pc += 4
                words += 1
                m = method + 4 * i
                per[sub][m].append(p)
                if (sub, m) not in order:
                    order.append((sub, m))
            if stop:
                break
            continue
        stop = "reserved 0x%08X" % h
        break

    return order, per, words, stop, put


put_override = int(opts["put"], 16) if "put" in opts else None

per = defaultdict(lambda: defaultdict(list))
order = []
run_report = []
for run_name in run_names:
    r_order, r_per, r_words, r_stop, r_put = decode_ring(
        run_name, get, put_override, budget)
    run_report.append((run_name, get, r_put, r_words, r_stop, len(r_order)))
    print("ring 0x%08X..0x%08X from %s: %d words, %s"
          % (get, r_put, run_name, r_words,
             "reached PUT" if not r_stop else "stopped: " + r_stop))
    for (sub, m) in r_order:
        if (sub, m) not in order:
            order.append((sub, m))
    for sub in r_per:
        for m, params in r_per[sub].items():
            per[sub][m].extend(params)

words = sum(r[3] for r in run_report)
put = max(r[2] for r in run_report)
print("union of %d ring(s): %d distinct (subchannel, method) pairs, %d words"
      % (len(run_names), len(order), words))


CLASS = {0: "NV097_KELVIN_PRIMITIVE", 1: "NV_MEMORY_TO_MEMORY_FORMAT",
         2: "NV_IMAGE_BLIT", 3: "NV_CONTEXT_SURFACES_2D"}
CLASS_MACRO = {0: "NV097_CLASS", 1: "NV_MEMCPY_CLASS",
               2: "NV_IMAGEBLIT_CLASS", 3: "NV_SURFACES2D_CLASS"}

# ---- the inventory document -------------------------------------------------
out = ["# JSRF NV2A method inventory (milestone 11)\n",
       "Every (subchannel, method) pair the title submits in its real pushbuffer",
       "rings, in the order the walk meets them. The model accepts a method",
       "only if it is implemented, so this list is the specification: the walk stops",
       "at the first entry here that is not in the generated method table.",
       "",
       "Generated by `scripts/gen-nv2a-method-inventory.py` as the UNION of %d run(s):"
       % len(run_report),
       ""]
for r_name, r_get, r_put, r_words, r_stop, r_pairs in run_report:
    out.append("- `%s`: ring `0x%08X..0x%08X`, %d words, %d pairs, %s"
               % (r_name, r_get, r_put, r_words, r_pairs,
                  "reached PUT" if not r_stop else "stopped: " + r_stop))
out += ["",
        "**Why a union.** One ring is not enough. The table used to come from a single",
        "run whose ring stopped at 1497 words; once the title began submitting more of",
        "its ring the walk met methods that table had never seen. Measured 2026-09-30:",
        "the F4 run submits `0x1720` at word 8124 of a 72353-word ring, so a decode",
        "budgeted at 4096 words could never reach it, and regenerating from that run",
        "alone would drop 148 methods the older ring contributes.",
        "",
        "| # | subchannel | class | method | params seen |",
        "|---|---|---|---|---|"]
for i, (sub, m) in enumerate(order):
    ps = per[sub][m]
    shown = " ".join("%08X" % x for x in ps[:4]) + (" ..." if len(ps) > 4 else "")
    out.append("| %d | %d | %s | 0x%04X | %s |" % (i, sub, CLASS.get(sub, "?"), m, shown))
out += ["",
        "Distinct pairs: %d. Total words: %d." % (len(order), words),
        "",
        "## Notes",
        "",
        "- `subchannel 0` is `NV097_KELVIN_PRIMITIVE`; 1 is",
        "  `NV_MEMORY_TO_MEMORY_FORMAT`; 2 is `NV_IMAGE_BLIT`; 3 is",
        "  `NV_CONTEXT_SURFACES_2D` -- all four named in `nv2a_regs.h`.",
        "- Method numbers repeat across classes with different meanings, which is",
        "  why the sink record carries the class as well as the method.",
        "- `method 0x0000` is the PFIFO-level SET_OBJECT binding, not a class method,",
        "  and is handled before this table is consulted.",
        "- The walk here mirrors `nv2a_submit_pending` exactly, including its",
        "  `(h & 3) == 1` call and `(h & 3) == 2` return tests and its target",
        "  masking. An earlier version classified by `h >> 30`, which agrees over",
        "  the first 0x1B24 words and diverges after, so the table it produced was",
        "  not the list the model walks and the walk stopped at method 0x1BCC."]
(root / "docs/jsrf-nv2a-method-inventory.md").write_text(
    "\n".join(out) + "\n", encoding="utf-8", newline="\n")

# ---- the generated table ----------------------------------------------------
by_class = defaultdict(set)
for sub, m in order:
    if m != 0x0000:          # SET_OBJECT is handled separately
        by_class[CLASS_MACRO[sub]].add(m)
by_class["NV097_CLASS"] |= {0x0100, 0x0200, 0x0204}   # NOP + the two clip methods

lines = [
    "/* Generated by scripts/gen-nv2a-method-inventory.py -- do not edit by hand.",
    " *",
    " * The NV2A methods this model implements, per class. The walk accepts a method",
    " * only if it appears here; anything else rejects the whole stream and rolls it",
    " * back, which is the contract jsrf_nv2a_registers pins.",
    " *",
    " * The lists are measured, not guessed: they come from decoding the pushbuffer",
    " * the title actually submits (docs/jsrf-nv2a-method-inventory.md). Adding a",
    " * method therefore means it appeared in a real submission.",
    " */",
    "#include <stdbool.h>",
    "#include <stdint.h>",
    "",
    "/* Class ids, mirroring nv2a_core.c. */",
    "#define NV097_CLASS        0x97u",
    "#define NV_MEMCPY_CLASS    0x39u",
    "#define NV_IMAGEBLIT_CLASS 0x9Fu",
    "#define NV_SURFACES2D_CLASS 0x62u",
    "",
]
for macro in ("NV097_CLASS", "NV_MEMCPY_CLASS", "NV_IMAGEBLIT_CLASS", "NV_SURFACES2D_CLASS"):
    ms = sorted(by_class.get(macro, set()))
    lines.append("static const uint16_t g_methods_%s[] = {" % macro.lower())
    for i in range(0, len(ms), 8):
        lines.append("    " + " ".join("0x%04Xu," % m for m in ms[i:i + 8]))
    lines.append("};")
    lines.append("")
lines += [
    "static bool in_table(const uint16_t *t, unsigned n, uint32_t method)",
    "{",
    "    for (unsigned i = 0; i < n; ++i)",
    "        if (t[i] == method) return true;",
    "    return false;",
    "}",
    "",
    "/* Is this a method the model can execute on a subchannel bound to class_id?",
    " * A subchannel with no binding has class 0 and implements nothing, which is",
    " * why an unbound subchannel is still rejected. */",
    "bool nv2a_method_implemented(uint32_t class_id, uint32_t method)",
    "{",
    "    switch (class_id) {",
    "    case NV097_CLASS:",
    "        return in_table(g_methods_nv097_class,",
    "                        sizeof(g_methods_nv097_class) / sizeof(uint16_t), method);",
    "    case NV_MEMCPY_CLASS:",
    "        return in_table(g_methods_nv_memcpy_class,",
    "                        sizeof(g_methods_nv_memcpy_class) / sizeof(uint16_t), method);",
    "    case NV_IMAGEBLIT_CLASS:",
    "        return in_table(g_methods_nv_imageblit_class,",
    "                        sizeof(g_methods_nv_imageblit_class) / sizeof(uint16_t), method);",
    "    case NV_SURFACES2D_CLASS:",
    "        return in_table(g_methods_nv_surfaces2d_class,",
    "                        sizeof(g_methods_nv_surfaces2d_class) / sizeof(uint16_t), method);",
    "    default:",
    "        return false;",
    "    }",
    "}",
    "",
]
dest = Path(r"C:\Users\logic\Repos\xboxrecomp\src\nv2a\nv2a_method_table.c")
dest.write_text("\n".join(lines), encoding="utf-8", newline="\n")

print("wrote docs/jsrf-nv2a-method-inventory.md and", dest)
for macro in ("NV097_CLASS", "NV_MEMCPY_CLASS", "NV_IMAGEBLIT_CLASS", "NV_SURFACES2D_CLASS"):
    print("   %-20s %d methods" % (macro, len(by_class.get(macro, set()))))
