#!/usr/bin/env python3
"""Fail when a lifted loop back edge lacks the guest-CPU yield hook.

With `RECOMP_GUEST_LOCK` the guest CPU changes hands only at kernel calls and at
`RECOMP_BACK_EDGE(va)` hooks.  The translator emits a hooked back edge as

    { RECOMP_BACK_EDGE(0x0013B1A2u); goto loc_0013B190; }

when `FunctionTranslator` is built with `backedge_yield=True` (`--backedge-yield`).
A path that builds it without the flag (recovery, relift, hidden-entry repair)
leaves bare `goto loc_X;` back edges, and a thread spinning on one never yields.

A back edge is a `goto loc_X` whose label is defined EARLIER in the same function
body than the goto.  It is hooked when the text right before the `goto` is
`{ RECOMP_BACK_EDGE(0x<8 hex>u);`.  Forward gotos, gotos inside comments, and gotos
to a label of another function are ignored.

Scans `src/recomp/recovered/recovered.c` and the `src/recomp/gen/recomp_NNNN.c` chunks.

Usage::

    python -X utf8 scripts/check-back-edges.py --check
    python -X utf8 scripts/check-back-edges.py --check --limit 50

Exit 0 clean or skipped (no generated tree), 1 a bare back edge.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECOMP = ROOT / 'src' / 'recomp'

CHUNK_RE = re.compile(r'^recomp_\d+\.c$')
# A function definition at column 0: `void sub_X(void)`, `static void body_X(void)`.
FUNC_RE = re.compile(r'^(?:static\s+)?void\s+(\w+)\s*\(', re.MULTILINE)
LABEL_RE = re.compile(r'^[ \t]*loc_([0-9A-Fa-f]{8}):', re.MULTILINE)
GOTO_RE = re.compile(r'\bgoto\s+loc_([0-9A-Fa-f]{8})\s*;')
COMMENT_RE = re.compile(r'/\*.*?\*/|//[^\n]*', re.DOTALL)
# The text the translator puts before a hooked goto (translator._mark_back_edges).
HOOK_BEFORE_RE = re.compile(r'\{\s*RECOMP_BACK_EDGE\(0x([0-9A-Fa-f]{8})u\)\s*;\s*$')


def _blank_comments(text: str) -> str:
    """Replace comments with spaces, keeping newlines so offsets and lines hold."""
    return COMMENT_RE.sub(lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text)


def scan_text(text: str):
    """Return (back_edges, bare): back_edges is the count of backward gotos, bare
    the list of (line, owner, target_va) among them with no RECOMP_BACK_EDGE."""
    code = _blank_comments(text)
    starts = [(m.start(), m.group(1)) for m in FUNC_RE.finditer(code)]
    bounds = [s for s, _ in starts] + [len(code)]
    total = 0
    bare = []
    for i, (start, owner) in enumerate(starts):
        body = code[start:bounds[i + 1]]
        labels = {}
        for m in LABEL_RE.finditer(body):
            labels.setdefault(int(m.group(1), 16), m.start())
        for m in GOTO_RE.finditer(body):
            target = int(m.group(1), 16)
            if target not in labels or labels[target] >= m.start():
                continue
            total += 1
            # Only the few characters before the goto matter; bound the search.
            before = body[max(0, m.start() - 80):m.start()]
            if HOOK_BEFORE_RE.search(before):
                continue
            line = code.count('\n', 0, start + m.start()) + 1
            bare.append((line, owner, target))
    return total, bare


def target_files(recomp_dir: Path):
    files = [p for p in [recomp_dir / 'recovered' / 'recovered.c'] if p.exists()]
    files += sorted(p for p in (recomp_dir / 'gen').glob('recomp_*.c')
                    if CHUNK_RE.match(p.name))
    return files


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--check', action='store_true',
                    help='accepted for consistency; the check is always a check')
    ap.add_argument('--recomp-dir', type=Path, default=DEFAULT_RECOMP,
                    help='directory holding gen/ and recovered/')
    ap.add_argument('--limit', type=int, default=20,
                    help='offenders to print (default 20; 0 prints all)')
    args = ap.parse_args(argv)

    files = target_files(args.recomp_dir)
    if not files:
        print(f'check-back-edges: SKIP, no generated tree under {args.recomp_dir} '
              f'(run just regen)')
        return 0

    offenders = []
    total_edges = 0
    print(f'check-back-edges: {len(files)} file(s) under {args.recomp_dir}')
    for path in files:
        text = path.read_text(encoding='utf-8', errors='replace')
        edges, bare = scan_text(text)
        total_edges += edges
        print(f'  {path.parent.name}/{path.name}: {edges} back edge(s), {len(bare)} bare')
        offenders += [(path, *b) for b in bare]

    if offenders:
        shown = offenders if args.limit <= 0 else offenders[:args.limit]
        for path, line, owner, target in shown:
            print(f'  {path.name}:{line}: {owner} -> 0x{target:08X} bare back edge')
        if len(shown) < len(offenders):
            print(f'  ... and {len(offenders) - len(shown)} more (--limit 0 lists all)')
        print(f'check-back-edges: FAIL, {len(offenders)} of {total_edges} back edge(s) '
              f'lack RECOMP_BACK_EDGE', file=sys.stderr)
        return 1
    print(f'check-back-edges: ok ({total_edges} back edge(s), all hooked)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
