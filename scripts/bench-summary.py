"""Summarise key-script benchmark runs: read each run's bench.json and group by backend.

Usage:  python -X utf8 scripts/bench-summary.py [--json] [--name NAME] <run-dir>...

Prints one line per run, then per backend the median of presents/s and of the p50/p90/p99 frame
time with the min-max spread across runs.  Exit 0 on success, 1 if any run lacks bench.json,
2 on bad input (no run dirs, unreadable or malformed bench.json).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

METRICS = ("presents_per_s", "frame_ms_p50", "frame_ms_p90", "frame_ms_p99")


class BadInput(Exception):
    pass


def median(values):
    """Median of a non-empty list of numbers (mean of the middle two when even)."""
    v = sorted(values)
    if not v:
        raise ValueError("median of nothing")
    mid = len(v) // 2
    return v[mid] if len(v) % 2 else (v[mid - 1] + v[mid]) / 2.0


def group_by_backend(records):
    """records: dicts with 'backend' and 'name' -> {(backend, name): [record, ...]} in input order."""
    groups = {}
    for r in records:
        groups.setdefault((r.get("backend") or "unknown", r.get("name")), []).append(r)
    return groups


def summarize_group(records):
    """Per metric: {median, min, max, n} over the records that have a numeric value."""
    out = {"runs": len(records)}
    for m in METRICS:
        vals = [r[m] for r in records if isinstance(r.get(m), (int, float))]
        out[m] = {"median": median(vals), "min": min(vals), "max": max(vals), "n": len(vals)} \
            if vals else None
    return out


def load_run(run_dir, name=None):
    """The bench records of one run, each tagged with its run dir; None if bench.json is absent."""
    path = Path(run_dir) / "bench.json"
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        raise BadInput("%s: %s" % (path, e))
    if not isinstance(data, dict) or not all(isinstance(v, dict) for v in data.values()):
        raise BadInput("%s: expected an object of bench records" % path)
    recs = []
    for key, rec in data.items():
        if name is None or key == name:
            recs.append(dict(rec, name=rec.get("name", key), run=str(run_dir)))
    return recs


def _f(v, fmt="%.2f"):
    return "-" if v is None else fmt % v


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("runs", nargs="*", help="run directories containing bench.json")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--name", help="only this bench name")
    args = ap.parse_args(argv)
    if not args.runs:
        print("no run directories given", file=sys.stderr)
        return 2
    records, missing = [], []
    for run in args.runs:
        if not Path(run).is_dir():
            print("not a directory: %s" % run, file=sys.stderr)
            return 2
        try:
            recs = load_run(run, args.name)
        except BadInput as e:
            print(str(e), file=sys.stderr)
            return 2
        if not recs:
            missing.append(run)
        else:
            records.extend(recs)
    groups = group_by_backend(records)
    summary = [dict(backend=b, name=n, **summarize_group(rs)) for (b, n), rs in groups.items()]
    if args.json:
        print(json.dumps({"runs": records, "backends": summary, "missing": missing}, indent=2))
    else:
        for r in records:
            print("%s %s backend=%s presents/s=%s p50=%s p90=%s p99=%s max=%s aliased=%s/%s stalls=%s"
                  % (r["run"], r["name"], r.get("backend"), _f(r.get("presents_per_s")),
                     _f(r.get("frame_ms_p50"), "%.1f"), _f(r.get("frame_ms_p90"), "%.1f"),
                     _f(r.get("frame_ms_p99"), "%.1f"), _f(r.get("frame_ms_max"), "%.1f"),
                     r.get("aliased_polls"), r.get("increment_polls"), r.get("stalls_over_100ms")))
        for s in summary:
            print("== %s / %s: %d run(s)" % (s["backend"], s["name"], s["runs"]))
            for m in METRICS:
                v = s[m]
                print("   %-15s median %s  range %s - %s" % (
                    m, _f(v and v["median"]), _f(v and v["min"]), _f(v and v["max"])))
        for run in missing:
            print("MISSING bench.json: %s" % run, file=sys.stderr)
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
