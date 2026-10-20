#!/usr/bin/env python3
"""Fail when a generated jump was deleted although its target is a callable function.

The label validator in the lifter turns a `goto loc_X` whose label is not in the
function into `(void)0; /* goto loc_X - dead code, label not in function */`. If
that jump is ever taken, control falls through instead of jumping. For a jump to
an address that has a symbol of its own that is a lifter defect: it should have
been a tail call. The adjustor thunk `sub_00169000` (`sub dword ptr [esp+4], 4;
jmp 0x168230`) was lifted that way, returned without the callee's `ret 8`, and
left the guest stack 8 bytes low (TR 31.53); 20 jumps across the tree had the
same shape.

This scans `gen/` and `recovered/` under the recomp tree for the deleted-jump
comment and classifies each target:

  callable (fails the check)
    recovered   a manifest entry, or defined in recovered/*.c
    generated   `void sub_<target>(void)` defined in a gen/ chunk
    stub        defined in a gen/ file that is not a recomp_NNNN chunk
    dispatch    dispatched under its own symbol `sub_<target>`
  no symbol (reported only)
    folded      dispatched under another symbol: an alias folded into its owner
    interior    not an entry anywhere: a label inside some body

A target is classified once, by the first matching category above. The toolkit's
`--strict-jumps` is the stricter generation-time gate; this is the check over
the tree that was actually built.

Usage:
    python scripts/check-dead-gotos.py [--recomp-dir DIR] [--manifest FILE] [-v]
"""
from __future__ import annotations

import argparse
import bisect
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_RECOMP = ROOT / "src" / "recomp"
DEFAULT_MANIFEST = ROOT / "config" / "recovered-functions.json"

# The comment the validator leaves behind; the target is the label's address.
DEAD_RE = re.compile(
    r"/\* goto loc_([0-9A-Fa-f]{8}) - dead code, label not in function \*/")
# `void sub_00169000(void)` at the start of a line: a definition, not a call.
DEF_RE = re.compile(r"^void sub_([0-9A-Fa-f]{8})\(void\)", re.MULTILINE)
# Enclosing function for the report: generated bodies and recovered bodies.
OWNER_RE = re.compile(
    r"^(?:static )?void ((?:sub|body)_[0-9A-Fa-f]{8})\(void\)", re.MULTILINE)
# `{ 0x00169000u, (recomp_func_t)sub_00169000 },`
DISPATCH_RE = re.compile(
    r"\{\s*0x([0-9A-Fa-f]{8})u\s*,\s*\(recomp_func_t\)\s*(sub_[0-9A-Fa-f]{8})\s*\}")
# Recovered lookup: `case 0x00011105u: return sub_00011105;`
CASE_RE = re.compile(
    r"case 0x([0-9A-Fa-f]{8})u:\s*return\s+(sub_[0-9A-Fa-f]{8})\s*;")
CHUNK_RE = re.compile(r"^recomp_\d+\.c$")

CALLABLE = ("recovered", "generated", "stub", "dispatch")
NO_SYMBOL = ("folded", "interior")


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def scan(recomp_dir: Path):
    """Return (sites, files) where each site is (path, line, owner, target)."""
    sites = []
    files = []
    for sub in ("gen", "recovered"):
        for path in sorted((recomp_dir / sub).glob("*.c")):
            files.append(path)
            text = _read(path)
            owners = [(m.start(), m.group(1)) for m in OWNER_RE.finditer(text)]
            starts = [o[0] for o in owners]
            for m in DEAD_RE.finditer(text):
                i = bisect.bisect_right(starts, m.start()) - 1
                owner = owners[i][1] if i >= 0 else "?"
                line = text.count("\n", 0, m.start()) + 1
                sites.append((path, line, owner, int(m.group(1), 16)))
    return sites, files


def symbols(recomp_dir: Path, manifest: Path | None):
    """Address sets for each way a target can have a symbol of its own."""
    recovered, generated, stub, own_dispatch, any_dispatch = (
        set(), set(), set(), set(), set())
    if manifest is not None and manifest.exists():
        for entry in json.loads(_read(manifest)):
            recovered.add(int(entry["start"], 16))
    for path in sorted((recomp_dir / "recovered").glob("*.c")):
        text = _read(path)
        recovered.update(int(m.group(1), 16) for m in DEF_RE.finditer(text))
        for m in CASE_RE.finditer(text):
            addr = int(m.group(1), 16)
            any_dispatch.add(addr)
            if int(m.group(2)[4:], 16) == addr:
                own_dispatch.add(addr)
    for path in sorted((recomp_dir / "gen").glob("*.c")):
        text = _read(path)
        defs = {int(m.group(1), 16) for m in DEF_RE.finditer(text)}
        (generated if CHUNK_RE.match(path.name) else stub).update(defs)
        for m in DISPATCH_RE.finditer(text):
            addr = int(m.group(1), 16)
            any_dispatch.add(addr)
            if int(m.group(2)[4:], 16) == addr:
                own_dispatch.add(addr)
    return recovered, generated, stub, own_dispatch, any_dispatch


def classify(target, recovered, generated, stub, own_dispatch, any_dispatch):
    if target in recovered:
        return "recovered"
    if target in generated:
        return "generated"
    if target in stub:
        return "stub"
    if target in own_dispatch:
        return "dispatch"
    if target in any_dispatch:
        return "folded"
    return "interior"


def check(recomp_dir: Path, manifest: Path | None):
    """Return (counts by category, failing sites, total, file count)."""
    sites, files = scan(recomp_dir)
    syms = symbols(recomp_dir, manifest)
    counts = Counter()
    failing = []
    for path, line, owner, target in sites:
        cat = classify(target, *syms)
        counts[cat] += 1
        if cat in CALLABLE:
            failing.append((path, line, owner, target, cat))
    return counts, failing, len(sites), len(files)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--recomp-dir", type=Path, default=DEFAULT_RECOMP,
                    help="directory holding gen/ and recovered/")
    ap.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST,
                    help="recovered-functions.json (entries count as callable)")
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="list every deleted jump with a callable target")
    args = ap.parse_args(argv)

    for sub in ("gen", "recovered"):
        if not (args.recomp_dir / sub).is_dir():
            print(f"check-dead-gotos: {args.recomp_dir / sub} is not a directory; "
                  f"generate the tree first", file=sys.stderr)
            return 2

    counts, failing, total, nfiles = check(args.recomp_dir, args.manifest)
    print(f"check-dead-gotos: {total} deleted jump(s) in {nfiles} file(s) "
          f"under {args.recomp_dir}")
    callable_total = sum(counts[c] for c in CALLABLE)
    print(f"  callable target: {callable_total}")
    for cat in CALLABLE:
        print(f"    {cat:<10} {counts[cat]}")
    print(f"  no symbol at target: {sum(counts[c] for c in NO_SYMBOL)}")
    for cat in NO_SYMBOL:
        print(f"    {cat:<10} {counts[cat]}")
    if failing:
        shown = failing if args.verbose else failing[:20]
        for path, line, owner, target, cat in shown:
            print(f"  {path.name}:{line}: {owner} -> 0x{target:08X} [{cat}]")
        if len(shown) < len(failing):
            print(f"  ... and {len(failing) - len(shown)} more (-v lists all)")
        print(f"check-dead-gotos: FAIL, {len(failing)} jump(s) to a function "
              f"with its own symbol were deleted instead of lifted as tail calls",
              file=sys.stderr)
        return 1
    print("check-dead-gotos: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
