#!/usr/bin/env python3
"""A2h NULL-slot triage: Exp1 offline mining, with tool-computed counts and loss accounting.

WHY THIS TOOL EXISTS
--------------------
The Advisor made one discipline BINDING for this line: **no hand counts in decision inputs.**
Every count must be tool-computed, from a named artifact, with positive controls and loss
accounting. The reason is concrete -- two hand counts of the same quantity (ordinal-277
dispatches from one call site) produced 1177 and 1909, and **both were withdrawn as uncitable**.
Two disagreeing hand counts are not a measurement with an error bar; they are two unverified
numbers.

The ICALL/kernel log is also BUDGETED and therefore potentially LOSSY
(`RECOMP_KERNEL_LOG_BUDGET`), so any count taken from it is meaningless without stating what the
cap was and whether it was reached. This tool reports the cap, the actual line count, and whether
truncation occurred, and it refuses to present a count as complete when the evidence cannot
support completeness.

USAGE
-----
    python -X utf8 scripts/a2h-null-slot-triage.py <run-dir> [--json out.json]
    python -X utf8 scripts/a2h-null-slot-triage.py --self-test
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# The terminal ICALL site and the slot it reads. These are anchors, not guesses: the call site is
# where the log's own `return=` points, and the slot is the operand in the verified instruction
# `call dword ptr [0x1c4064]` at 0x00149828.
ICALL_SITE = 0x00149828
ICALL_RETURN = 0x0014982E
SLOT_VA = 0x001C4064
SLOT_IMAGE_VALUE = 0x80000115          # the original XBE's .rdata content at that slot
ORDINAL_184 = 184
ORDINAL_277 = 277

KERNEL_RE = re.compile(
    r"\[KERNEL\]\s+#(\d+):\s+ordinal\s+(\d+)\s+\(slot\s+(\d+)\)\s+esp=(0x[0-9A-Fa-f]+)\s+ret=(0x[0-9A-Fa-f]+)")
# TWO ICALL FORMS EXIST and a parser that knows one silently reports zero for the other:
#   R1 (raw-zero terminal):  [ICALL] invalid target 0x00000000 tid=65356 esp=00F7FD00 return=0014982E
#   nr-baseline (other):     [ICALL] Failed to resolve VA 0xFFFFFFFF (thread calls:953, tid=57592)
# The second form carries no esp/return, so a comparison that assumes the first would
# mischaracterise the baseline's terminal event. Both are parsed and TAGGED.
ICALL_RE = re.compile(
    r"\[ICALL\]\s+invalid target\s+(0x[0-9A-Fa-f]+)\s+tid=(\d+)\s+esp=([0-9A-Fa-f]+)\s+return=([0-9A-Fa-f]+)")
ICALL_UNRESOLVED_RE = re.compile(
    r"\[ICALL\]\s+Failed to resolve VA\s+(0x[0-9A-Fa-f]+)\s+\(thread calls:\s*(\d+),\s*tid=(\d+)")
EXC_RE = re.compile(r"\[EXCEPTION\]\s+tid=(\d+)\s+code=(0x[0-9A-Fa-f]+)")
REGS_RE = re.compile(r"Xbox regs:\s*(.*)")
OOM_RE = re.compile(r"out of memory \(requested (\d+), used (\d+)/(\d+)\)")


class TriageError(Exception):
    """Raised rather than emitting a count that the evidence cannot support."""


def parse_log(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.split("\n")

    kernel = []
    for i, L in enumerate(lines):
        m = KERNEL_RE.search(L)
        if m:
            kernel.append({
                "line": i + 1,
                "call_index": int(m.group(1)),
                "ordinal": int(m.group(2)),
                "slot": int(m.group(3)),
                "esp": int(m.group(4), 16),
                "ret": int(m.group(5), 16),
            })

    icalls = []
    for i, L in enumerate(lines):
        m = ICALL_RE.search(L)
        if m:
            icalls.append({
                "line": i + 1,
                "form": "invalid_target",
                "target": int(m.group(1), 16),
                "tid": int(m.group(2)),
                "esp": int(m.group(3), 16),
                "ret": int(m.group(4), 16),
                "thread_calls": None,
            })
            continue
        m = ICALL_UNRESOLVED_RE.search(L)
        if m:
            # The other terminal form. It carries NO esp/return, so any comparison that assumes
            # the first form would silently mischaracterise this run's terminal event.
            icalls.append({
                "line": i + 1,
                "form": "failed_to_resolve",
                "target": int(m.group(1), 16),
                "tid": int(m.group(3)),
                "esp": None,
                "ret": None,
                "thread_calls": int(m.group(2)),
            })

    excs = [{"line": i + 1, "tid": int(m.group(1)), "code": int(m.group(2), 16)}
            for i, L in enumerate(lines) for m in [EXC_RE.search(L)] if m]

    regs = []
    for i, L in enumerate(lines):
        m = REGS_RE.search(L)
        if m:
            regs.append({"line": i + 1, "raw": m.group(1).strip()})

    ooms = [{"line": i + 1, "requested": int(m.group(1)), "used": int(m.group(2)),
             "total": int(m.group(3))}
            for i, L in enumerate(lines) for m in [OOM_RE.search(L)] if m]

    return {"lines": len(lines), "kernel": kernel, "icalls": icalls,
            "exceptions": excs, "regs": regs, "ooms": ooms, "text": text}


def metadata_budget(run_dir: Path) -> dict:
    """Ground the log budget from RUN METADATA, not from log text.

    The Advisor corrected the Session on this: `RECOMP_KERNEL_LOG_BUDGET` is an ENVIRONMENT VARIABLE,
    so searching the log for it is methodologically unsound -- an env var is not log text, and its
    absence from the log says nothing about whether it was set. The budget must be read from the
    run's own recipe/metadata and stated.

    THE METADATA SHAPE IS NOT A SIMPLE KEY. In this project's `metadata.json` the env vars appear as
    records with a `"name"` field, e.g. `{"name": "RECOMP_KERNEL_LOG_BUDGET", "value": "100000"}`.
    An earlier version of this function looked only for a key CONTAINING `LOG_BUDGET` and therefore
    returned None for a budget that was present -- an absent-record-read-as-negative error, the same
    class this project keeps hitting. So the walk now also inspects `name`/`value` PAIRS.
    """
    out = {"source": None, "budget": None, "raw": None}

    def scan(obj, depth=0):
        """Return the first budget value found, checking keys AND name/value pairs."""
        if depth > 5:
            return None
        if isinstance(obj, dict):
            # A record whose name field IS the env var, with the value beside it.
            name = obj.get("name")
            if isinstance(name, str) and "LOG_BUDGET" in name.upper():
                for vk in ("value", "val", "env_value", "setting"):
                    if vk in obj and obj[vk] is not None:
                        return obj[vk]
            # A plain key containing the env var name.
            for k, v in obj.items():
                if isinstance(k, str) and "LOG_BUDGET" in k.upper():
                    return v
            for v in obj.values():
                r = scan(v, depth + 1)
                if r is not None:
                    return r
        elif isinstance(obj, list):
            for v in obj:
                r = scan(v, depth + 1)
                if r is not None:
                    return r
        elif isinstance(obj, str):
            m = re.search(r"LOG_BUDGET[=:\s]+(\d+)", obj)
            if m:
                return m.group(1)
        return None

    for name in ("metadata.json", "result.json", "run.json"):
        p = run_dir / name
        if not p.exists():
            continue
        try:
            data = json.loads(p.read_text(encoding="utf-8", errors="replace"))
        except Exception:
            continue
        val = scan(data)
        if val is not None:
            try:
                val = int(str(val).strip())
            except ValueError:
                pass
            out.update({"source": name, "budget": val})
            return out
    return out


def budget_accounting(parsed, meta: dict) -> dict:
    """The ICALL/kernel log is budgeted AND the call index is PER-THREAD. State both.

    A count from a truncated log is a LOWER BOUND, never a total, and the difference decides
    whether a row may be selected at all.

    THE INDEX IS PER-THREAD, established from the toolkit source rather than inferred:
    `kernel_bridge.c:316` declares `static RECOMP_TLS int g_kernel_call_count = 0;` -- RECOMP_TLS
    means one counter PER THREAD. So the `#N` in each log line restarts per thread, which is why
    this run's log shows 6471 parsed kernel lines under a maximum index of only 5555, with 585
    indices REUSED for different events and 357 decreases (e.g. line 820: 306 -> 1).

    CONSEQUENCE: a raw parsed-line total is a lower bound on LOGGED dispatches and is NOT a
    dispatch total. Any claim of "N dispatches before the transition" must name the thread, and a
    completeness check must key on the per-thread index rather than assuming a global sequence.
    """
    text = parsed["text"]
    truncated = bool(re.search(r"log budget|budget reached|truncat|suppress", text, re.I))
    indices = [k["call_index"] for k in parsed["kernel"]]
    contiguous = None
    decreases = 0
    reused = 0
    if indices:
        contiguous = indices == list(range(indices[0], indices[0] + len(indices)))
        decreases = sum(1 for i in range(1, len(indices)) if indices[i] < indices[i - 1])
        reused = len(indices) - len(set(indices))
    summaries = re.findall(r"summary:\s*(\d+) total calls", text)
    budget = meta.get("budget")
    # Headroom is only meaningful when the budget is known from metadata.
    headroom = None
    if isinstance(budget, int) and budget > 0:
        headroom = budget - len(parsed["kernel"])
    return {
        "budget_declared": budget,
        "budget_source": meta.get("source"),
        "budget_headroom": headroom,
        "budget_note": ("read from run metadata; the env var is NOT log text, so searching the log "
                        "for it is unsound (Advisor correction)"),
        "kernel_lines": len(parsed["kernel"]),
        "icall_lines": len(parsed["icalls"]),
        "truncation_notice_found": truncated,
        "call_index_contiguous": contiguous,
        "call_index_decreases": decreases,
        "call_index_reused": reused,
        "index_is_per_thread": True,
        "index_basis": ("kernel_bridge.c:316 `static RECOMP_TLS int g_kernel_call_count = 0;` "
                        "-- RECOMP_TLS is per-thread"),
        "printed_thread_totals": [int(s) for s in summaries],
        "first_call_index": indices[0] if indices else None,
        "last_call_index": indices[-1] if indices else None,
        # A GLOBAL contiguity test is the WRONG test for a per-thread counter, so `complete` is
        # False whenever the log interleaves threads -- which is the honest answer, because
        # per-thread windows cannot be reconstructed from an unattributed log.
        "complete": bool(not truncated and contiguous and meta.get("source")),
        "complete_note": ("a per-thread counter CANNOT be shown complete from an unattributed "
                          "log; the kernel lines carry no tid, so per-thread windows are not "
                          "reconstructable from this artifact"),
    }


def ordering(parsed) -> dict:
    """OOM versus NULL ordering, decided by line position -- and by reading past the call.

    The Advisor's terminal-event rule: attribute a terminal event by reading the instructions past
    the failing call, never by temporal co-occurrence. So this reports ORDER (an observation) and
    separately reports whether the guest's own check exists (a fact from the bytes, supplied by the
    caller as a constant here because it is static evidence, not log evidence).
    """
    oom_lines = [o["line"] for o in parsed["ooms"]]
    icall_lines = [c["line"] for c in parsed["icalls"]]
    exc_lines = [e["line"] for e in parsed["exceptions"]]
    out = {
        "oom_lines": oom_lines,
        "icall_lines": icall_lines,
        "exception_lines": exc_lines,
    }
    if oom_lines and icall_lines:
        out["oom_before_icall"] = min(oom_lines) < min(icall_lines)
        out["lines_between"] = min(icall_lines) - min(oom_lines)
    if icall_lines and exc_lines:
        out["icall_before_exception"] = min(icall_lines) < min(exc_lines)
    # The guest DOES check the allocation result -- verified bytes, cited not derived here.
    out["guest_checks_result"] = {
        "value": True,
        "basis": ("verified XBE bytes 00149E56 `test eax,eax` / 00149E58 `jl 0x149eec`; "
                  "0xC0000017 is negative so the branch is TAKEN"),
        "evidence_class": "static structural, not this log",
    }
    return out


def thread_check(parsed) -> dict:
    """`tid=` is already present on every ICALL line. Verify single-threaded or attribute.

    FAIL CLOSED: ZERO parsed ICALL lines is NOT evidence of single-threadedness -- it is absence of
    evidence. An earlier version of this function returned `single_threaded: true` for an empty
    list, which would have let a run with no parsed ICALL be described as single-threaded. Absence
    of a witness must never read as a negative measurement.
    """
    tids = Counter(c["tid"] for c in parsed["icalls"])
    forms = Counter(c["form"] for c in parsed["icalls"])
    if not parsed["icalls"]:
        return {
            "distinct_tids": 0,
            "tid_counts": {},
            "forms": {},
            "single_threaded": None,
            "status": "UNKNOWN:no-icall-lines-parsed",
            "note": ("no ICALL line was parsed, so nothing is known about threads. This is NOT "
                     "evidence of single-threadedness; check the parser against this run's log "
                     "format before drawing any conclusion."),
        }
    return {
        "distinct_tids": len(tids),
        "tid_counts": dict(tids),
        "forms": dict(forms),
        "single_threaded": len(tids) == 1,
        "status": "ok",
    }


def slot_transitions(parsed) -> dict:
    """What the log says about the slot's dispatch history at the terminal call site.

    COUNTS ARE COMPUTED, NOT COUNTED BY HAND, and they carry the completeness flag. This is the
    specific quantity whose hand counts (1177, 1909) were withdrawn.
    """
    from_site = [k for k in parsed["kernel"]
                 if k["ordinal"] == ORDINAL_277 and k["ret"] == ICALL_RETURN]
    all_277 = [k for k in parsed["kernel"] if k["ordinal"] == ORDINAL_277]
    # The last dispatch from the terminal site, and whether it precedes the ICALL.
    last_from_site = from_site[-1] if from_site else None
    first_icall = parsed["icalls"][0] if parsed["icalls"] else None
    return {
        "ordinal_277_from_terminal_site": len(from_site),
        "ordinal_277_anywhere": len(all_277),
        "last_277_from_site": last_from_site,
        "first_icall": first_icall,
        "last_277_precedes_icall": bool(
            last_from_site and first_icall and last_from_site["line"] < first_icall["line"]),
        "slot_va": "0x%08X" % SLOT_VA,
        "slot_image_value": "0x%08X" % SLOT_IMAGE_VALUE,
        "slot_image_ordinal": SLOT_IMAGE_VALUE & 0x7FFFFFFF,
    }


def icall_context(parsed, n: int = 12) -> list:
    """The last N kernel/ICALL lines before the exception, for the faulting-region context."""
    if not parsed["icalls"]:
        return []
    first = parsed["icalls"][0]["line"]
    lines = parsed["text"].split("\n")
    lo = max(0, first - n)
    return [{"line": i + 1, "text": lines[i].strip()[:150]}
            for i in range(lo, min(first + 2, len(lines))) if lines[i].strip()]


def run(run_dir: Path) -> dict:
    log = run_dir / "jsrf_run.log"
    if not log.exists():
        raise TriageError("no jsrf_run.log in %s" % run_dir)
    import hashlib
    parsed = parse_log(log)
    meta = metadata_budget(run_dir)
    return {
        "run_dir": str(run_dir),
        "log": str(log),
        # Artifact identity, so a count can be cited as an (artifact, query, value) triple.
        "log_sha256": hashlib.sha256(log.read_bytes()).hexdigest().upper(),
        "log_bytes": log.stat().st_size,
        "budget": budget_accounting(parsed, meta),
        "ordering": ordering(parsed),
        "threads": thread_check(parsed),
        "slot": slot_transitions(parsed),
        "icall_context": icall_context(parsed),
    }


def self_test() -> int:
    """Fixture-based self test: the tool must count correctly on a KNOWN synthetic log.

    A counting tool that is never tested on known input is exactly the failure mode this project
    keeps hitting, so the fixture below has a hand-computable expected answer.
    """
    import tempfile
    lines = []
    for i in range(1, 6):
        lines.append("[KERNEL] #%d: ordinal 277 (slot 65) esp=0x00F7FD00 ret=0x0014982E" % i)
    lines.append("[KERNEL] #6: ordinal 184 (slot 10) esp=0x00F7FCF0 ret=0x00149E50")
    lines.append("xbox_HeapAlloc: out of memory (requested 598869040, used 12715008/50855936)")
    lines.append("[ICALL] invalid target 0x00000000 tid=65356 esp=00F7FD00 return=0014982E")
    lines.append("[EXCEPTION] tid=65356 code=0xE0424943 RIP=0x7FFA6EB441CA")
    with tempfile.TemporaryDirectory() as td:
        p = Path(td)
        (p / "jsrf_run.log").write_text("\n".join(lines), encoding="utf-8")
        r = run(p)
    ok = True
    checks = [
        ("ordinal_277_from_terminal_site == 5", r["slot"]["ordinal_277_from_terminal_site"] == 5),
        ("ordinal_277_anywhere == 5", r["slot"]["ordinal_277_anywhere"] == 5),
        ("one ICALL parsed", r["budget"]["icall_lines"] == 1),
        ("OOM before ICALL", r["ordering"].get("oom_before_icall") is True),
        ("single threaded", r["threads"]["single_threaded"] is True),
        ("call indices contiguous", r["budget"]["call_index_contiguous"] is True),
        ("last 277 precedes the ICALL", r["slot"]["last_277_precedes_icall"] is True),
        ("guest_checks_result is recorded as static", 
         r["ordering"]["guest_checks_result"]["evidence_class"].startswith("static")),
        ("index flagged per-thread", r["budget"]["index_is_per_thread"] is True),
    ]
    # A NON-contiguous log must be reported incomplete rather than counted as if complete. This is
    # the loss-accounting guard: a lower bound must never be presented as a total.
    lines2 = list(lines)
    lines2.insert(3, "[KERNEL] #2: ordinal 277 (slot 65) esp=0x00F7FD00 ret=0x0014982E")
    with tempfile.TemporaryDirectory() as td:
        p2 = Path(td)
        (p2 / "jsrf_run.log").write_text("\n".join(lines2), encoding="utf-8")
        r2 = run(p2)
    checks.append(("non-contiguous log reported incomplete",
                   r2["budget"]["call_index_reused"] > 0 and r2["budget"]["complete"] is False))
    for label, passed in checks:
        print("  %-46s %s" % (label, "PASS" if passed else "FAIL"))
        ok = ok and passed
    print()
    print("SELF-TEST %s" % ("OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("run_dir", nargs="?", help="archived run directory")
    ap.add_argument("--json", help="write the full result here")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if not a.run_dir:
        ap.error("run_dir or --self-test is required")
    try:
        result = run(Path(a.run_dir))
    except TriageError as exc:
        print("ERROR: %s" % exc, file=sys.stderr)
        return 2
    print(json.dumps(result, indent=2, default=str))
    if a.json:
        Path(a.json).write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
