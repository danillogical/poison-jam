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
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

root = Path(__file__).resolve().parents[1]

# `run_name` is the run whose ring is decoded. `--put` is the ring's top, and
# leaving it out asks the run's own log instead of assuming: the model prints
# `[PFIFO] submit #N diag=... get=... put=...` on every kick, and the guest ring
# lives at 0x80000000 + the PFIFO offset. The first version of this script
# hardcoded both ends to the first pushbuffer the title ever submitted
# (get 0x1000, put 0x1B24), which silently stopped covering the ring the moment
# the title started submitting more of it -- the run
# 20260922-110235-244-spanfix-1185b0 reaches put 0x2764 and rejects method
# 0x1BCC, which the 0x1B24 decode could not see.
# Argv is parsed by hand, and the two rules below exist because the hand parsing
# silently OVERWRITES BOTH GENERATED FILES when it is handed something it does not
# understand. Measured 2026-10-06: `--help` fell through both filters, so `run_names`
# took its default of ONE old run and the table was rewritten with ~86 methods
# dropped -- a destructive no-op that looks like a successful run. Two guards:
#
#   * `-h`/`--help` prints usage and exits WITHOUT writing anything;
#   * any other unrecognised `--option` is a hard error, not a silent default, so a
#     typo (e.g. `--puts=`) cannot quietly regenerate from the wrong ring set.
#
# A generator that writes two tracked files must fail closed on bad input.
KNOWN_OPTS = {"get", "put", "budget", "table-out", "doc-out", "witness",
              "allow-removals"}

if any(a in ("-h", "--help") for a in sys.argv[1:]):
    print(__doc__)
    print("usage: gen-nv2a-method-inventory.py [run-name ...] "
          "[--get=0xADDR] [--put=0xADDR] [--budget=N]")
    print()
    print("  run-name        one or more archived run directory names under logs/runs/.")
    print("                  The generated table is the UNION of their decoded rings;")
    print("                  each run is decoded only to its OWN log-derived ring top.")
    print("  --get=0xADDR    first ring word to decode (default 0x80001000)")
    print("  --put=0xADDR    force the ring top instead of reading the run's log.")
    print("                  Pass it explicitly when a run's log top is stale.")
    print("  --budget=N      decode budget in words (default 200000)")
    print("  --table-out=P   where to write the toolkit's nv2a_method_table.c")
    print("                  (default: the toolkit checkout path; the env var")
    print("                  JSRF_NV2A_TABLE_OUT does the same)")
    print("  --doc-out=P     where to write this repo's method inventory document")
    print("                  (default: docs/jsrf-nv2a-method-inventory.md; env var")
    print("                  JSRF_NV2A_DOC_OUT does the same)")
    print("  --witness=P     a runtime-witnessed method manifest (JSON) to UNION into")
    print("                  the table. Use this for methods the MODEL'S OWN WALK")
    print("                  reported via [PFIFO] admit-unknown: that record is emitted")
    print("                  only after a successful commit, so it is stronger evidence")
    print("                  than any decode. Default:")
    print("                  config/nv2a-runtime-witnessed-methods.json (env var")
    print("                  JSRF_NV2A_WITNESS does the same; --witness=none disables)")
    print("  --allow-removals  permit a table that removes existing methods")
    print("                  (refused by default: a removal re-breaks the walk)")
    print()
    print("Writes docs/jsrf-nv2a-method-inventory.md and the toolkit's")
    print("src/nv2a/nv2a_method_table.c. WARNING: with no run names it falls back to")
    print("one default run and will DROP methods; name every run whose ring matters.")
    sys.exit(0)

args = [a for a in sys.argv[1:] if not a.startswith("--")]
opts = dict(a[2:].split("=", 1) for a in sys.argv[1:] if a.startswith("--") and "=" in a)
# Options that take NO value, spelled bare. Without this set, `--allow-removals`
# would be reported as "missing a value" and the escape hatch would be unusable.
FLAG_OPTS = {"allow-removals"}
flags = {a[2:] for a in sys.argv[1:] if a.startswith("--") and "=" not in a
         and a[2:] in FLAG_OPTS}
unknown = sorted(set(opts) - KNOWN_OPTS)
# A `--known-option` with no `=` is NOT the same error as an unknown option, and
# reporting it as "unrecognised" is actively misleading: it names a real option in
# both the offending list and the known list. Split the two cases so the message
# says which one happened.
valueless = [a for a in sys.argv[1:]
             if a.startswith("--") and "=" not in a
             and a[2:] in KNOWN_OPTS and a[2:] not in FLAG_OPTS]
malformed = [a for a in sys.argv[1:]
             if a.startswith("--") and "=" not in a and a not in ("-h", "--help")
             and a[2:] not in KNOWN_OPTS]
if unknown or valueless or malformed:
    problems = []
    if unknown:
        problems.append("unrecognised option(s) %s"
                        % ", ".join("--%s" % u for u in unknown))
    if valueless:
        problems.append("option(s) missing a value: %s (use --name=value)"
                        % ", ".join(valueless))
    if malformed:
        problems.append("malformed option(s) %s" % " ".join(malformed))
    raise SystemExit(
        "gen-nv2a-method-inventory: %s; known options are %s. "
        "Refusing to regenerate from a default ring set (see --help)."
        % ("; ".join(problems),
           ", ".join("--%s" % k for k in sorted(KNOWN_OPTS))))

# A budget of zero (or negative) stops the walk on its first word and would emit a
# table containing only the hardcoded entries -- the `--budget=0` destructive case
# measured 2026-10-06. It is a value error, not a mode.
def _positive_int(name, text, base):
    try:
        value = int(text, base)
    except ValueError:
        raise SystemExit("gen-nv2a-method-inventory: --%s=%s is not a number"
                         % (name, text))
    if value <= 0:
        raise SystemExit("gen-nv2a-method-inventory: --%s=%s must be positive "
                         "(a non-positive value would emit an almost empty table)"
                         % (name, text))
    return value

# Imported only after the argument guards, so `--help` and a rejected option both
# work without the decoder's dependencies being importable. That ordering is
# deliberate: a usage request must not fail on an unrelated import error, because
# a failed `--help` is what tempted a caller into running the tool bare.
sys.path.insert(0, str(root / "scripts"))
from jsrf_dump import DumpMemory

# Every run named on the command line contributes its methods, and the table is the
# UNION of them. One ring is not enough: the table used to come from a single run
# whose ring stopped at 1497 words, so once the title began submitting more of its
# ring the walk met methods the table had never seen. Measured 2026-09-30: the F4 run
# submits 0x1720 at word 8124 of a 72353-word ring, and regenerating from that run
# ALONE would drop 148 methods the older ring contributes. The union keeps both, and
# every entry in it is still something a real submission contained.
run_names = args or ["20260922-110235-244-spanfix-1185b0"]
get = _positive_int("get", opts.get("get", "0x80001000"), 16)
# The walk budget has to exceed the longest ring decoded. It used to be 4096, which
# is BELOW the word where 0x1720 first appears (8124), so a decode that reported
# "reached PUT" for the old ring could never see the method the walk was stuck on.
budget = _positive_int("budget", opts.get("budget", "200000"), 10)


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


put_override = _positive_int("put", opts["put"], 16) if "put" in opts else None

per = defaultdict(lambda: defaultdict(list))
order = []
run_report = []
incomplete = []
for run_name in run_names:
    r_order, r_per, r_words, r_stop, r_put = decode_ring(
        run_name, get, put_override, budget)
    run_report.append((run_name, get, r_put, r_words, r_stop, len(r_order)))
    print("ring 0x%08X..0x%08X from %s: %d words, %s"
          % (get, r_put, run_name, r_words,
             "reached PUT" if not r_stop else "stopped: " + r_stop))
    if r_stop:
        incomplete.append((run_name, r_stop))
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

# A decode that stopped early has NOT seen the whole ring, so its method list is a
# PREFIX of what the title submitted -- writing it would drop every method after
# the stop. The generator's whole claim is that the table is what the title really
# submits, so an incomplete decode is a hard error, not a warning.
#
# This is also the mechanism behind the `--budget=0` incident: the walk stopped on
# its first word, the union was empty, and the write sites still ran.
if incomplete:
    raise SystemExit(
        "gen-nv2a-method-inventory: refusing to write from an INCOMPLETE decode -- "
        + "; ".join("%s stopped: %s" % (n, s) for n, s in incomplete)
        + ".\nA decode that did not reach PUT has not seen the whole ring, so the table\n"
          "it would write is a prefix of the real submission and DROPS methods. Fix the\n"
          "ring top (--put=), the run set, or the budget, then rerun.")


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
# The document text is built here but NOT written: both artifacts are written
# together at the end, after the removal and completeness guards, so a refused
# regeneration leaves the tree untouched. The runtime-witness section is appended
# BELOW, after the manifest is parsed, because it needs the parsed entries.

# ---- runtime-witnessed admissions -------------------------------------------
# A decode is not the model's walk (TR section 22): it increments every parameter's
# method, while `nv2a_submit_pending` honours the non-incrementing bit and rejects
# an incrementing span past 0x1FFC. A decode that "reached PUT" can therefore
# INVENT a method, so a decoded entry is not by itself sound provenance.
#
# The `[PFIFO] admit-unknown` record is different in kind: the walk queues it only
# inside the successful-commit block (after the consumer has received every staged
# method), so it witnesses a method the walk really staged AND committed. This
# manifest unions those witnesses into the table, which makes the admission
# durable and auditable: the table can be regenerated without re-decoding a
# wrapped ring, and without hand-editing a generated file that the next
# regeneration would silently drop.
WITNESS_ENTRIES = []
witness_path = None
if "witness" in opts:
    witness_path = None if opts["witness"].strip().lower() in ("", "none") else opts["witness"]
elif os.environ.get("JSRF_NV2A_WITNESS", "").strip().lower() in ("", "none"):
    witness_path = None
else:
    witness_path = os.environ.get("JSRF_NV2A_WITNESS") or str(
        root / "config" / "nv2a-runtime-witnessed-methods.json")
if witness_path is None and "witness" not in opts:
    witness_path = str(root / "config" / "nv2a-runtime-witnessed-methods.json")

CLASS_FOR_NAME = {"NV097_KELVIN_PRIMITIVE": "NV097_CLASS",
                  "NV_MEMORY_TO_MEMORY_FORMAT": "NV_MEMCPY_CLASS",
                  "NV_IMAGE_BLIT": "NV_IMAGEBLIT_CLASS",
                  "NV_CONTEXT_SURFACES_2D": "NV_SURFACES2D_CLASS"}
CLASS_BY_ID = {0x97: "NV097_CLASS", 0x39: "NV_MEMCPY_CLASS",
               0x9F: "NV_IMAGEBLIT_CLASS", 0x62: "NV_SURFACES2D_CLASS"}

if witness_path is not None:
    import json
    wp = Path(witness_path)
    if not wp.is_file():
        raise SystemExit("gen-nv2a-method-inventory: --witness=%s is not a readable "
                         "file (pass --witness=none to generate without witnesses)"
                         % witness_path)
    try:
        manifest = json.loads(wp.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise SystemExit("gen-nv2a-method-inventory: cannot parse the witness "
                         "manifest %s: %s" % (wp, exc))
    entries = manifest.get("entries")
    if not isinstance(entries, list) or not entries:
        raise SystemExit("gen-nv2a-method-inventory: witness manifest %s has no "
                         "`entries` list" % wp)
    for i, e in enumerate(entries):
        where = "entry %d of %s" % (i, wp)
        if not isinstance(e, dict):
            raise SystemExit("gen-nv2a-method-inventory: %s is not an object" % where)
        cls = str(e.get("class", "")).strip().lower()
        meth = str(e.get("method", "")).strip().lower()
        # Validate hard: a manifest is hand-maintained, so a typo must be an error
        # rather than a silently-wrong admission.
        try:
            cls_id = int(cls, 16)
        except ValueError:
            raise SystemExit("gen-nv2a-method-inventory: %s has a non-hex class %r"
                             % (where, e.get("class")))
        try:
            meth_id = int(meth, 16)
        except ValueError:
            raise SystemExit("gen-nv2a-method-inventory: %s has a non-hex method %r"
                             % (where, e.get("method")))
        if cls_id not in CLASS_BY_ID:
            raise SystemExit("gen-nv2a-method-inventory: %s names class 0x%02X, which "
                             "has no generated table (known: %s)"
                             % (where, cls_id,
                                " ".join("0x%02X" % c for c in sorted(CLASS_BY_ID))))
        if not 0 <= meth_id <= 0x1FFC or meth_id % 4:
            raise SystemExit("gen-nv2a-method-inventory: %s names method 0x%X, which "
                             "is not an aligned NV097 method" % (where, meth_id))
        # The witness text, the run and the log hash are the provenance. Require
        # them: an entry without its witness cannot be audited later.
        for field in ("run", "log_sha256", "witness"):
            if not str(e.get(field, "")).strip():
                raise SystemExit("gen-nv2a-method-inventory: %s is missing `%s`, so it "
                                 "has no auditable provenance" % (where, field))
        # A manifest is hand-maintained, so the DECLARED class/method must agree
        # with the witness TEXT. Without this, an entry can name one method and
        # quote a record for another -- the table would then admit a method no
        # witness ever justified, which is exactly the failure this file exists to
        # prevent.
        text = str(e["witness"])
        m = re.search(r"admit-unknown\s+class=([0-9A-Fa-f]+)\s+method=([0-9A-Fa-f]+)", text)
        if not m:
            raise SystemExit("gen-nv2a-method-inventory: %s has a `witness` that is not "
                             "a [PFIFO] admit-unknown record: %r" % (where, text))
        w_cls, w_meth = int(m.group(1), 16), int(m.group(2), 16)
        if (w_cls, w_meth) != (cls_id, meth_id):
            raise SystemExit("gen-nv2a-method-inventory: %s declares class=0x%02X "
                             "method=0x%04X but its witness quotes class=0x%02X "
                             "method=0x%04X -- the manifest contradicts itself"
                             % (where, cls_id, meth_id, w_cls, w_meth))
        # A hash that is not a hash cannot be checked later, so it is a typo, not
        # provenance. 64 hex characters is the sha256 the manifest documents.
        sha = str(e["log_sha256"]).strip().lower()
        if not re.fullmatch(r"[0-9a-f]{64}", sha):
            raise SystemExit("gen-nv2a-method-inventory: %s has a log_sha256 that is "
                             "not 64 hex characters: %r" % (where, e["log_sha256"]))
        # If the witness's archive is present, verify the hash AND that the quoted
        # record really is in that log. Absent archives are allowed (the logs are
        # gitignored), but a PRESENT log that disagrees is a hard error: that is a
        # stale or edited witness, the failure mode this check exists for.
        run_log = root / "logs" / "runs" / str(e["run"]) / "jsrf_run.log"
        if run_log.is_file():
            import hashlib
            actual = hashlib.sha256(run_log.read_bytes()).hexdigest()
            if actual != sha:
                raise SystemExit("gen-nv2a-method-inventory: %s quotes log_sha256 %s "
                                 "but %s hashes to %s -- the witness is stale or edited"
                                 % (where, sha, run_log, actual))
            body = run_log.read_text(encoding="utf-8", errors="replace")
            if text.strip() not in body:
                raise SystemExit("gen-nv2a-method-inventory: %s quotes a witness that "
                                 "does not appear in %s" % (where, run_log))
        WITNESS_ENTRIES.append((CLASS_BY_ID[cls_id], meth_id, e))

# ---- the generated table ----------------------------------------------------
by_class = defaultdict(set)
for sub, m in order:
    if m != 0x0000:          # SET_OBJECT is handled separately
        by_class[CLASS_MACRO[sub]].add(m)
by_class["NV097_CLASS"] |= {0x0100, 0x0200, 0x0204}   # NOP + the two clip methods

# Union the runtime witnesses. Deduplicate so the same witness twice is idempotent.
witness_added = defaultdict(set)
for macro, meth_id, _e in WITNESS_ENTRIES:
    if meth_id not in by_class[macro]:
        witness_added[macro].add(meth_id)
    by_class[macro].add(meth_id)

# Now that the witnesses are parsed, finish the document.
if WITNESS_ENTRIES:
    wout = ["",
            "## Runtime-witnessed admissions (not decoded)",
            "",
            "These methods were NOT taken from a decode. Each was reported by the model's",
            "own walk as `[PFIFO] admit-unknown`, a record emitted only inside the",
            "successful-commit block -- after the commit consumer received every staged",
            "method -- so it witnesses a method the walk really staged and committed.",
            "That is strictly stronger evidence than a decode, because the decoder is not",
            "the model's walk (TR section 22).",
            "",
            "Source manifest: `%s`" % os.path.relpath(witness_path, str(root)).replace("\\", "/"),
            "",
            "| class | method | run | log line | witness |",
            "|---|---|---|---|---|"]
    for macro, meth_id, e in sorted(WITNESS_ENTRIES, key=lambda t: (t[0], t[1])):
        wout.append("| %s | `0x%04X` | `%s` | %s | `%s` |"
                    % (macro, meth_id, e.get("run"), e.get("log_line"),
                       e.get("witness")))
    wout += ["",
             "Admitted by this generation: %s."
             % (", ".join("%s +%d" % (m, len(v)) for m, v in sorted(witness_added.items())
                          if v) or "none (all already present)"),
             "",
             "**Known limitation.** The admitting run is exploratory, and its witness",
             "queue is populated only for methods that COMMIT. A walk that is later",
             "rejected (for example on the word budget) never commits, so the methods it",
             "admitted are never logged. This list is therefore a lower bound on what a",
             "given region needs, not a complete inventory."]
    out += wout
inventory_doc = "\n".join(out) + "\n"

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
# The toolkit table lives in the OTHER repository, so its path cannot be derived
# from this script's own location. It is overridable because the hardcoded
# absolute path is itself a hazard: a copy of this script run from anywhere --
# a test scratch tree, say -- would still overwrite the real toolkit file. The
# override makes the write target explicit and testable; the default keeps the
# documented invocation working unchanged.
# `--table-out=` with an EMPTY value must not silently fall back to the env var or
# the default: `or` would treat "" as absent, so a caller who meant to redirect the
# write would instead overwrite the real toolkit table. An explicit empty value is
# an error.
if "table-out" in opts and not opts["table-out"].strip():
    raise SystemExit("gen-nv2a-method-inventory: --table-out= needs a path "
                     "(an empty value would silently write the default target)")
dest = Path(opts.get("table-out")
            or os.environ.get("JSRF_NV2A_TABLE_OUT")
            or r"C:\Users\logic\Repos\xboxrecomp\src\nv2a\nv2a_method_table.c")
# The inventory document normally sits beside the script's own repository. It is
# overridable for the same reason the table path is: a caller (or a test) that
# wants to exercise the generator without touching either repository must be able
# to redirect BOTH writes, and a guard that only redirects one of them still
# writes into the tree.
#
# Both artifacts are written only AFTER the completeness and removal guards pass,
# so a refused regeneration leaves both untouched. Writing the document first is
# how an invalid table target used to leave the document already overwritten.
doc_out = Path(opts.get("doc-out")
               or os.environ.get("JSRF_NV2A_DOC_OUT")
               or (root / "docs/jsrf-nv2a-method-inventory.md"))
if "doc-out" in opts and not opts["doc-out"].strip():
    raise SystemExit("gen-nv2a-method-inventory: --doc-out= needs a path")


# ---- fail-closed removal guard ----------------------------------------------
# The table is an ADMISSION set: the walk rejects the whole stream on a method it
# lacks (ledger L39). So REMOVING an entry is never a routine consequence of a
# regeneration -- it silently re-breaks whatever the removed method was needed
# for. Measured 2026-10-06: `--budget=0` and a bare `--help` both reached the
# write sites and produced a 3-method table over the real toolkit file, a
# destructive no-op that printed `wrote ...` as though it had succeeded.
#
# So: compare the new set against what is already in the destination and refuse
# to write a strict subset. A deliberate removal is still possible, but it must
# be asked for by name (`--allow-removals`).
def _methods_in_table_text(text):
    """The method set a generated table declares, keyed by CLASS MACRO.

    The generated arrays are named `g_methods_nv097_class`, i.e. the macro in
    lower case, while `by_class` is keyed by the upper-case macro. Normalising
    here is load-bearing: without `.upper()` every existing method looks removed
    and the guard refuses every regeneration.
    """
    found = defaultdict(set)
    current = None
    for line in text.splitlines():
        head = line.strip()
        if head.startswith("static const uint16_t g_methods_"):
            current = head.split("g_methods_", 1)[1].split("[", 1)[0].upper()
            continue
        if current is None:
            continue
        if head.startswith("};"):
            current = None
            continue
        for m in re.findall(r"0x([0-9A-Fa-f]{4})u,", head):
            found[current].add(int(m, 16))
    return found


new_table = "\n".join(lines)
# `--allow-removals` is a bare FLAG, so it lands in `flags`, not `opts`; reading
# only `opts` made the documented escape hatch impossible to invoke.
allow_removals = "allow-removals" in flags or "allow-removals" in opts
if not allow_removals and dest.is_file():
    try:
        old_text = dest.read_text(encoding="utf-8")
    except OSError as exc:
        # Fail CLOSED: an unreadable baseline means the removal check cannot run,
        # and silently treating it as "no removals" would defeat the guard.
        raise SystemExit("gen-nv2a-method-inventory: cannot read the existing table "
                         "at %s to check for removals (%s); refusing to write"
                         % (dest, exc))
    old_sets = _methods_in_table_text(old_text)
    removed = {}
    for cls, old in old_sets.items():
        gone = sorted(old - by_class.get(cls, set()))
        if gone:
            removed[cls] = gone
    if removed:
        detail = "; ".join(
            "%s: %d removed (%s)"
            % (cls, len(v), " ".join("0x%04X" % m for m in v[:12])
               + (" ..." if len(v) > 12 else ""))
            for cls, v in sorted(removed.items()))
        raise SystemExit(
            "gen-nv2a-method-inventory: refusing to write a table that REMOVES methods -- %s.\n"
            "The table is an admission set (ledger L39), so a removal re-breaks whatever\n"
            "needed the method. This usually means the run set is wrong (a stale ring top,\n"
            "a wrapped ring, a budget too small, or the no-argument default). Name every\n"
            "run whose ring matters, or pass --allow-removals if the removal is deliberate."
            % detail)

dest.write_text(new_table, encoding="utf-8", newline="\n")
doc_out.write_text(inventory_doc, encoding="utf-8", newline="\n")

print("wrote", doc_out, "and", dest)
for macro in ("NV097_CLASS", "NV_MEMCPY_CLASS", "NV_IMAGEBLIT_CLASS", "NV_SURFACES2D_CLASS"):
    print("   %-20s %d methods" % (macro, len(by_class.get(macro, set()))))
