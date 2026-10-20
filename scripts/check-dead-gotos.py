#!/usr/bin/env python3
"""Fail when a generated jump was silently deleted or trapped despite a callable target.

C has no goto into another function, so when the label validator finds a `goto
loc_X` whose label is not in the emitting body it cannot keep the jump. Two
spellings have existed:

  silent     `(void)0; /* goto loc_X - dead code, label not in function */`
             A taken jump falls through into code the guest never ran. This
             spelling is retired: any occurrence fails the check.
  trap       `RECOMP_JUMP_TRAP(site, target)`
             A taken jump is fatal at run time and names both ends. Allowed, and
             counted, unless the target has a symbol of its own: such a jump
             should have been a tail call (the adjustor thunk `sub_00169000`,
             `sub dword ptr [esp+4], 4; jmp 0x168230`, returned without the
             callee's `ret 8` and left the guest stack 8 bytes low, TR 31.53).

Trap targets are classified by what the tree has at that address, first match
winning:

  callable (fails the check)
    recovered   a manifest entry, or defined in recovered/*.c
    generated   `void sub_<target>(void)` defined in a gen/ chunk
    stub        defined in a gen/ file that is not a recomp_NNNN chunk
    dispatch    dispatched under its own symbol `sub_<target>`
  no symbol (reported only)
    folded      dispatched through `recomp_alias_<target>` that calls its owner
    fatal       dispatched through `recomp_alias_<target>` that aborts, because
                its owner starts after it
    interior    not an entry anywhere: a label inside some body

The toolkit's `--strict-jumps` is the generation-time gate; this is the check
over the tree that was actually built.

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

# The retired silent spelling; the target is the label's address.
SILENT_RE = re.compile(
    r"/\* goto loc_([0-9A-Fa-f]{8}) - dead code, label not in function \*/")
# `RECOMP_JUMP_TRAP(0x0002E05Cu, 0x0002DC69u);` -- site, then target.
TRAP_RE = re.compile(
    r"^(?!\s*#).*?RECOMP_JUMP_TRAP\(\s*0x([0-9A-Fa-f]{8})u\s*,"
    r"\s*0x([0-9A-Fa-f]{8})u\s*\)", re.MULTILINE)
# `void sub_00169000(void)` at the start of a line: a definition, not a call.
DEF_RE = re.compile(r"^void sub_([0-9A-Fa-f]{8})\(void\)", re.MULTILINE)
# Enclosing function for the report: generated bodies and recovered bodies.
OWNER_RE = re.compile(
    r"^(?:static )?void ((?:sub|body)_[0-9A-Fa-f]{8})\(void\)", re.MULTILINE)
# `{ 0x00169000u, (recomp_func_t)sub_00169000 },` or `...)recomp_alias_0002DC69 },`
DISPATCH_RE = re.compile(
    r"\{\s*0x([0-9A-Fa-f]{8})u\s*,\s*\(recomp_func_t\)\s*"
    r"((?:sub|recomp_alias)_[0-9A-Fa-f]{8})\s*\}")
# Recovered lookup: `case 0x00011105u: return sub_00011105;`
CASE_RE = re.compile(
    r"case 0x([0-9A-Fa-f]{8})u:\s*return\s+(sub_[0-9A-Fa-f]{8})\s*;")
# `static void recomp_alias_0002DC69(void) { ... }`, one line each.
ALIAS_WRAPPER_RE = re.compile(
    r"^static void recomp_alias_([0-9A-Fa-f]{8})\(void\)(.*)$", re.MULTILINE)
CHUNK_RE = re.compile(r"^recomp_\d+\.c$")

CALLABLE = ("recovered", "generated", "stub", "dispatch")
NO_SYMBOL = ("folded", "fatal", "interior")


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def scan(recomp_dir: Path):
    """Return (sites, files); a site is (path, line, owner, kind, target)."""
    sites = []
    files = []
    for sub in ("gen", "recovered"):
        for path in sorted((recomp_dir / sub).glob("*.c")):
            files.append(path)
            text = _read(path)
            owners = [(m.start(), m.group(1)) for m in OWNER_RE.finditer(text)]
            starts = [o[0] for o in owners]

            def add(match, kind, target):
                i = bisect.bisect_right(starts, match.start()) - 1
                owner = owners[i][1] if i >= 0 else "?"
                line = text.count("\n", 0, match.start()) + 1
                sites.append((path, line, owner, kind, int(target, 16)))

            for m in SILENT_RE.finditer(text):
                add(m, "silent", m.group(1))
            for m in TRAP_RE.finditer(text):
                add(m, "trap", m.group(2))
    return sites, files


def symbols(recomp_dir: Path, manifest: Path | None):
    """Address sets for each way a target can have something of its own."""
    recovered, generated, stub = set(), set(), set()
    own_dispatch, folded, fatal = set(), set(), set()
    if manifest is not None and manifest.exists():
        for entry in json.loads(_read(manifest)):
            recovered.add(int(entry["start"], 16))
    for path in sorted((recomp_dir / "recovered").glob("*.c")):
        text = _read(path)
        recovered.update(int(m.group(1), 16) for m in DEF_RE.finditer(text))
        for m in CASE_RE.finditer(text):
            addr = int(m.group(1), 16)
            if int(m.group(2)[4:], 16) == addr:
                own_dispatch.add(addr)
    for path in sorted((recomp_dir / "gen").glob("*.c")):
        text = _read(path)
        defs = {int(m.group(1), 16) for m in DEF_RE.finditer(text)}
        (generated if CHUNK_RE.match(path.name) else stub).update(defs)
        for m in DISPATCH_RE.finditer(text):
            addr = int(m.group(1), 16)
            if m.group(2).startswith("sub_") and int(m.group(2)[4:], 16) == addr:
                own_dispatch.add(addr)
        for m in ALIAS_WRAPPER_RE.finditer(text):
            (fatal if "abort()" in m.group(2) else folded).add(int(m.group(1), 16))
    return recovered, generated, stub, own_dispatch, folded, fatal


def classify(target, recovered, generated, stub, own_dispatch, folded, fatal):
    if target in recovered:
        return "recovered"
    if target in generated:
        return "generated"
    if target in stub:
        return "stub"
    if target in own_dispatch:
        return "dispatch"
    if target in fatal:
        return "fatal"
    if target in folded:
        return "folded"
    return "interior"


def check(recomp_dir: Path, manifest: Path | None):
    """Return (counts, failing sites, total sites, file count).

    counts holds "silent" plus one entry per trap category.
    """
    sites, files = scan(recomp_dir)
    syms = symbols(recomp_dir, manifest)
    counts = Counter()
    failing = []
    for path, line, owner, kind, target in sites:
        if kind == "silent":
            counts["silent"] += 1
            failing.append((path, line, owner, target, "silent"))
            continue
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
                    help="list every failing jump")
    args = ap.parse_args(argv)

    for sub in ("gen", "recovered"):
        if not (args.recomp_dir / sub).is_dir():
            print(f"check-dead-gotos: {args.recomp_dir / sub} is not a directory; "
                  f"generate the tree first", file=sys.stderr)
            return 2

    counts, failing, total, nfiles = check(args.recomp_dir, args.manifest)
    traps = sum(counts[c] for c in CALLABLE + NO_SYMBOL)
    callable_total = sum(counts[c] for c in CALLABLE)
    print(f"check-dead-gotos: {total} untranslatable jump(s) in {nfiles} file(s) "
          f"under {args.recomp_dir}")
    print(f"  silent (void)0 deletions: {counts['silent']}")
    print(f"  fatal traps: {traps}")
    print(f"    callable target: {callable_total}")
    for cat in CALLABLE:
        print(f"      {cat:<10} {counts[cat]}")
    print(f"    no symbol at target: {sum(counts[c] for c in NO_SYMBOL)}")
    for cat in NO_SYMBOL:
        print(f"      {cat:<10} {counts[cat]}")
    if failing:
        shown = failing if args.verbose else failing[:20]
        for path, line, owner, target, cat in shown:
            print(f"  {path.name}:{line}: {owner} -> 0x{target:08X} [{cat}]")
        if len(shown) < len(failing):
            print(f"  ... and {len(failing) - len(shown)} more (-v lists all)")
        print(f"check-dead-gotos: FAIL, {counts['silent']} jump(s) silently "
              f"deleted as (void)0 and {callable_total} trapped despite a target "
              f"with its own symbol", file=sys.stderr)
        return 1
    print("check-dead-gotos: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
