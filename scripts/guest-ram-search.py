"""Search guest RAM snapshots written by `key-script-run.py` `ram_snap` steps for a value.

The keyscript driver produces no process dump, so a run's guest state is captured as
`ram/<name>.bin` plus a `<name>.json` sidecar. This tool finds the guest addresses whose
value satisfies every predicate across every supplied run, which is how a game-owned
witness is isolated without knowing its address in advance.

Usage:
  python -X utf8 scripts/guest-ram-search.py --run DIR [--run DIR ...] [--width 1|2|4 ...]
      [--eq NAME=INT ...] [--same N1,N2[,...] ...] [--lt A:B ...] [--ne A:B ...]
      [--control DIR:A:B ...] [--limit 200]

A VA survives only if, in every `--run`, each `--eq` holds, all names in each `--same` are
equal, each `--lt A:B` holds (value(A) < value(B)) and each `--ne A:B` holds. A VA also
survives only if, in every `--control DIR:A:B`, value(A) == value(B). A VA that touches an
unreadable page in any loaded snapshot is dropped: an unreadable page is never data.

Exit codes: 0 when at least one match; 1 when none; 2 for a missing snapshot or name, or a
length mismatch.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

DTYPES = {1: np.dtype("<u1"), 2: np.dtype("<u2"), 4: np.dtype("<u4")}
GUEST_PAGE_SIZE = 0x1000


class SearchError(Exception):
    pass


def load_snapshot(run_dir, name):
    """(raw bytes, base VA, unreadable page VAs) for one snapshot, read once.

    The caller derives each width's view from these bytes. Reading per width instead would
    re-read the same 64 MiB for every width, and the acceptance search loads several
    snapshots across several widths."""
    run_dir = Path(run_dir)
    sidecar_path = run_dir / "ram" / (name + ".json")
    bin_path = run_dir / "ram" / (name + ".bin")
    if not sidecar_path.is_file():
        raise SearchError("%s: no sidecar for snapshot %r" % (run_dir, name))
    if not bin_path.is_file():
        raise SearchError("%s: no data file for snapshot %r" % (run_dir, name))
    sidecar = json.loads(sidecar_path.read_text(encoding="utf-8"))
    raw = bin_path.read_bytes()
    va = int(sidecar["va"], 0)
    length = int(sidecar["length"])
    if length != len(raw):
        raise SearchError("%s: %s sidecar says %d bytes, file has %d"
                          % (run_dir, name, length, len(raw)))
    unreadable = {int(v, 0) for v in sidecar.get("unreadable_pages", [])}
    return raw, va, unreadable


def view_at_width(raw, va, width, run_dir, name):
    """A little-endian `width`-byte view of `raw`, or a SearchError naming the misalignment."""
    if va % width:
        raise SearchError("%s: %s base 0x%08X is not %d-byte aligned" % (run_dir, name, va, width))
    if len(raw) % width:
        raise SearchError("%s: %s length %d is not a multiple of %d"
                          % (run_dir, name, len(raw), width))
    return np.frombuffer(raw, dtype=np.uint8).view(DTYPES[width])


class RunSet:
    """Every snapshot named by any predicate, for one run directory."""

    def __init__(self, run_dir, names, widths):
        self.dir = Path(run_dir)
        self.names = names
        self.widths = widths
        self.by_width = {w: {} for w in widths}
        self.unreadable = set()
        for name in names:
            raw, va, bad = load_snapshot(self.dir, name)
            for w in widths:
                self.by_width[w][name] = (view_at_width(raw, va, w, self.dir, name), va)
            self.unreadable |= bad

    def arrays(self, width):
        return self.by_width[width]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--run", action="append", default=[], metavar="DIR", required=True)
    ap.add_argument("--width", action="append", default=[], type=int, choices=(1, 2, 4))
    ap.add_argument("--eq", action="append", default=[], metavar="NAME=INT")
    ap.add_argument("--range", action="append", default=[], metavar="NAME=LO:HI")
    ap.add_argument("--same", action="append", default=[], metavar="N1,N2[,...]")
    ap.add_argument("--lt", action="append", default=[], metavar="A:B")
    ap.add_argument("--ne", action="append", default=[], metavar="A:B")
    ap.add_argument("--control", action="append", default=[], metavar="DIR:A:B")
    ap.add_argument("--find-file", metavar="PATH")
    ap.add_argument("--find-ascii", metavar="TEXT")
    ap.add_argument("--snapshot", metavar="NAME")
    ap.add_argument("--limit", type=int, default=200)
    args = ap.parse_args(argv)

    widths = args.width or [1, 2, 4]
    eq = []
    for spec in args.eq:
        name, _, value = spec.partition("=")
        if not name or not value:
            ap.error("--eq must be NAME=INT: %r" % spec)
        eq.append((name, int(value, 0)))
    ranges = []
    for spec in args.range:
        name, _, span = spec.partition("=")
        lo, _, hi = span.partition(":")
        if not name or not lo or not hi:
            ap.error("--range must be NAME=LO:HI: %r" % spec)
        ranges.append((name, int(lo, 0), int(hi, 0)))
    same = [tuple(s.split(",")) for s in args.same]
    lt = [tuple(s.split(":")) for s in args.lt]
    ne = [tuple(s.split(":")) for s in args.ne]
    for pair in lt + ne:
        if len(pair) != 2:
            ap.error("--lt/--ne must be A:B")
    controls = []
    for spec in args.control:
        # A Windows path contains a colon, so split only the two trailing fields.
        parts = spec.rsplit(":", 2)
        if len(parts) != 3 or not parts[0] or not parts[1] or not parts[2]:
            ap.error("--control must be DIR:A:B: %r" % spec)
        controls.append((parts[0], parts[1], parts[2]))

    names = [n for n, _ in eq]
    names += [n for n, _, _ in ranges]
    for group in same:
        names += list(group)
    for a, b in lt + ne:
        names += [a, b]
    seen, ordered = set(), []
    for n in names:
        if n not in seen:
            seen.add(n)
            ordered.append(n)

    if args.find_file or args.find_ascii:
        # Locate a byte string inside one snapshot and report its guest VAs. This is how a
        # loaded asset (a talk script) is tied to the live address the game reads it from.
        if args.find_file and args.find_ascii:
            ap.error("--find-file and --find-ascii are mutually exclusive")
        if len(args.run) != 1:
            ap.error("find mode needs exactly one --run")
        if not args.snapshot:
            ap.error("find mode needs --snapshot NAME")
        if eq or ranges or same or lt or ne or controls:
            ap.error("find mode cannot be combined with predicates")
        needle = (Path(args.find_file).read_bytes() if args.find_file
                  else args.find_ascii.encode("utf-8"))
        if not needle:
            ap.error("find mode needs a non-empty needle")
        try:
            raw, va, unreadable = load_snapshot(args.run[0], args.snapshot)
        except (SearchError, OSError, KeyError, ValueError) as e:
            print("error: %s" % e, file=sys.stderr)
            return 2
        found = 0
        start = 0
        while True:
            i = raw.find(needle, start)
            if i < 0:
                break
            start = i + 1
            # Never report an address whose bytes touch an unreadable page: a hole would
            # zero-fill and could match by accident.
            pages = {((va + i + k) & ~(GUEST_PAGE_SIZE - 1))
                     for k in range(0, len(needle), GUEST_PAGE_SIZE)}
            pages |= {((va + i + len(needle) - 1) & ~(GUEST_PAGE_SIZE - 1))}
            if pages & unreadable:
                continue
            print("0x%08X" % (va + i))
            found += 1
        return 0 if found else 1

    try:
        runs = [RunSet(d, ordered, widths) for d in args.run]
        ctrl_runs = []
        ctrl_names = set()
        for d, a, b in controls:
            ctrl_names |= {a, b}
        for d, _, _ in controls:
            ctrl_runs.append(RunSet(d, sorted(ctrl_names), widths))
    except (SearchError, OSError, KeyError, ValueError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2

    total = 0
    for width in widths:
        # Names must resolve at this width in every run before a mask can be built.
        try:
            base_len = None
            masks = []
            for run in runs:
                arrs = run.arrays(width)
                n = len(arrs[ordered[0]][0])
                if base_len is None:
                    base_len = n
                elif n != base_len:
                    raise SearchError("snapshot lengths differ between runs")
                m = np.ones(n, dtype=bool)

                def value_of(name):
                    arr, base = arrs[name]
                    return arr, base

                for name, want in eq:
                    arr, _ = value_of(name)
                    m &= (arr == want)
                for name, lo, hi in ranges:
                    arr, _ = value_of(name)
                    m &= (arr >= lo) & (arr < hi)
                for group in same:
                    arr0, _ = value_of(group[0])
                    for other in group[1:]:
                        arr1, _ = value_of(other)
                        m &= (arr0 == arr1)
                for a, b in lt:
                    arra, _ = value_of(a)
                    arrb, _ = value_of(b)
                    m &= (arra < arrb)
                for a, b in ne:
                    arra, _ = value_of(a)
                    arrb, _ = value_of(b)
                    m &= (arra != arrb)
                masks.append(m)

            for ctrl, (_, a, b) in zip(ctrl_runs, controls):
                arrs = ctrl.arrays(width)
                arra, _ = arrs[a]
                arrb, _ = arrs[b]
                for m in masks:
                    if len(arra) != len(m):
                        raise SearchError("control snapshot length differs")
                    m &= (arra == arrb)

            combined = masks[0]
            for m in masks[1:]:
                combined &= m

            # Drop every VA on a page that was unreadable in any loaded snapshot.
            for run in runs + ctrl_runs:
                if not run.unreadable:
                    continue
                arrs = run.arrays(width)
                any_name = next(iter(arrs))
                arr0, base0 = arrs[any_name]
                if len(arr0) != len(combined):
                    raise SearchError("control snapshot length differs")
                idx = np.arange(len(combined))
                vas = base0 + idx * width
                for page in run.unreadable:
                    combined &= ~((vas >= page) & (vas < page + 0x1000))

            hits = np.nonzero(combined)[0]
            total += len(hits)
            if not len(hits):
                print("width=%d count=0" % width)
                continue
            print("width=%d count=%d" % (width, len(hits)))
            base0 = runs[0].arrays(width)[ordered[0]][1]
            for i in hits[:args.limit]:
                va = base0 + int(i) * width
                vals = []
                for run in runs:
                    arrs = run.arrays(width)
                    vals.append(str(int(arrs[ordered[0]][0][i])))
                print("0x%08X w=%d %s" % (va, width, " ".join(vals)))
        except (SearchError, KeyError) as e:
            print("error: %s" % e, file=sys.stderr)
            return 2

    return 0 if total else 1


if __name__ == "__main__":
    raise SystemExit(main())
