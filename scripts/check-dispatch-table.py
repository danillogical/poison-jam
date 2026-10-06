"""Gate: a generated dispatch table's declared size must match its actual contents.

**Why this exists (measured failure).** `config/generated-patches.json`'s
`remove-b5f3a-dispatch` removed one tuple from `g_recomp_table[]` in
`src/recomp/gen/recomp_dispatch.c` but left the hand-written literal

    static const size_t g_recomp_table_size = 8928;   /* the array held 8927 */

`recomp_dispatch_init()` loops `for (i = 0; i < g_recomp_table_size; i++)` and
writes `g_flat_table[g_recomp_table[i].xbox_va - g_flat_base]`, so it read one
entry past the end of the array and stored from the garbage it read: a host
access violation (`0xC0000005`, write) at `recomp_dispatch.c:9299`, **before
`guest_entry`**, in runs g08 and g08b. `just check`, the full CTest suite and
`check-merge-structure.py` all passed on that tree. A green suite is not a
statement about a class no checker covers, and this was that class.

**The primary fix is structural, and this checker is the independent gate for
it.** `tools/recomp/translator.py` now emits

    static const size_t g_recomp_table_size =
        sizeof(g_recomp_table) / sizeof(g_recomp_table[0]);

which cannot disagree with the array it measures, and the game repo's
`fix-dispatch-table-size` patch re-applies the same derived text after a
regeneration. This script exists because that only covers trees produced by
those two routes. It re-derives the facts from the generated C itself, so it
also catches a tree assembled by hand, by an older toolkit, or by a future patch
that reintroduces a literal.

**What is checked, all from the file's own bytes** (no second source of truth):

  1. the declared count (a literal, or the derived `sizeof` form) equals the
     number of rows actually in `g_recomp_table[]`;
  2. every row's VA is unique, and the rows are strictly ascending -- both are
     required by `recomp_lookup`'s binary search, which silently returns the
     wrong function or `NULL` otherwise;
  3. `g_flat_span` covers the last row, so the flat table cannot be indexed out
     of bounds for a VA the table itself declares.

The `sizeof` form is accepted as a *pass* on (1) only because the compiler
enforces it: there is no second number to disagree. It is still parsed so a
malformed derivation (a wrong array name, a missing `sizeof`) is caught here
rather than at compile time on someone else's machine.

Usage:
    python -X utf8 scripts/check-dispatch-table.py [path ...]

Defaults to `src/recomp/gen/recomp_dispatch.c`. Exit 0 when every file is
consistent, 1 otherwise.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# `static const recomp_entry_t g_recomp_table[] = {` ... `};`
TABLE = re.compile(r'g_recomp_table\s*\[\s*\]\s*=\s*\{(.*?)\n\};', re.S)
ROW = re.compile(r'\{\s*0x([0-9A-Fa-f]{1,8})u?\s*,')
# `static const size_t g_recomp_table_size = <expr>;`
SIZE = re.compile(r'g_recomp_table_size\s*=\s*([^;]+);', re.S)
# `static const uint32_t g_flat_span = 0x...u;`
SPAN = re.compile(r'g_flat_span\s*=\s*0x([0-9A-Fa-f]+)u?\s*;')
BASE = re.compile(r'g_flat_base\s*=\s*0x([0-9A-Fa-f]+)u?\s*;')
DERIVED = re.compile(
    r'^\s*sizeof\s*\(\s*g_recomp_table\s*\)\s*/\s*sizeof\s*\(\s*g_recomp_table\s*\[\s*0\s*\]\s*\)\s*$')
LITERAL = re.compile(r'^\s*(\d+)\s*$')

# A row that has been COMMENTED OUT is not a row.
#
# **Measured defect (Advisor finding, reproduced).** `remove-b5f3a-dispatch`
# replaces its tuple with prose, so counting the raw text happens to work today.
# But a patch that instead comments the tuple out --
#
#     /* { 0x000B5F3Au, (recomp_func_t)sub_000B5F3A }, removed */
#
# -- leaves the array holding 8927 rows against a declared 8928, and the checker
# PASSED (exit 0), because it was counting the *spelling* of a row rather than a
# row. That is exactly the class this gate exists to catch, so the table body is
# comment-stripped before counting.
_BLOCK_COMMENT = re.compile(r'/\*.*?\*/', re.S)
_LINE_COMMENT = re.compile(r'//[^\n]*')


def strip_comments(text: str) -> str:
    """Remove C comments, preserving newlines so line numbers still line up.

    String literals are not a concern here: this is a table of `{ 0x...u, ... }`
    rows with no string members, and the checker never needs to distinguish a
    comment marker inside a string.
    """
    text = _BLOCK_COMMENT.sub(lambda m: '\n' * m.group(0).count('\n'), text)
    return _LINE_COMMENT.sub('', text)


def check_file(path: Path) -> list[str]:
    """Every consistency failure in one generated dispatch unit."""
    if not path.is_file():
        return [f'{path}: missing']
    raw_text = path.read_text(encoding='utf-8', errors='replace')
    # Count rows in the COMMENT-STRIPPED text: a commented-out tuple is not a row.
    text = strip_comments(raw_text)

    table = TABLE.search(text)
    if not table:
        return [f'{path}: no `g_recomp_table[] = {{ ... }}` definition found']
    rows = ROW.findall(table.group(1))
    if not rows:
        return [f'{path}: `g_recomp_table[]` has no rows']
    vas = [int(r, 16) for r in rows]
    n = len(vas)

    size = SIZE.search(text)
    if not size:
        return [f'{path}: no `g_recomp_table_size` definition found']
    expr = size.group(1)

    problems: list[str] = []

    if DERIVED.match(expr):
        pass  # the compiler enforces agreement; nothing to compare
    elif LITERAL.match(expr):
        declared = int(expr)
        if declared != n:
            problems.append(
                f'{path}: declared g_recomp_table_size = {declared} but '
                f'g_recomp_table[] holds {n} row(s) (off by {declared - n:+d}). '
                f'recomp_dispatch_init() reads {abs(declared - n)} entr'
                f'{"y" if abs(declared - n) == 1 else "ies"} past the end of the '
                f'array and stores from the garbage it reads. Derive the count '
                f'(`sizeof(g_recomp_table) / sizeof(g_recomp_table[0])`) or '
                f'correct the literal in the same change that edits the table.')
    else:
        problems.append(
            f'{path}: g_recomp_table_size is neither an integer literal nor the '
            f'derived `sizeof(g_recomp_table) / sizeof(g_recomp_table[0])` form: '
            f'{expr.strip()!r}. A count this checker cannot evaluate must not pass '
            f'as though it had been checked.')

    dupes = sorted({v for v in vas if vas.count(v) > 1})
    if dupes:
        shown = ', '.join(f'0x{v:08X}' for v in dupes[:8])
        problems.append(
            f'{path}: {len(dupes)} duplicate VA(s) in g_recomp_table[]: {shown}. '
            f'recomp_lookup()\'s binary search cannot resolve a duplicated key, so '
            f'one of the two bodies is unreachable by indirect call.')

    if vas != sorted(vas):
        for i in range(1, n):
            if vas[i] < vas[i - 1]:
                problems.append(
                    f'{path}: g_recomp_table[] is not sorted ascending: '
                    f'0x{vas[i - 1]:08X} precedes 0x{vas[i]:08X} at row {i}. '
                    f'recomp_lookup() binary-searches this array.')
                break

    span = SPAN.search(text)
    base = BASE.search(text)
    if span and base:
        flat_base, flat_span = int(base.group(1), 16), int(span.group(1), 16)
        last = max(vas)
        need = last - flat_base + 1
        if flat_span < need:
            problems.append(
                f'{path}: g_flat_span = 0x{flat_span:X} does not cover the last '
                f'table entry 0x{last:08X} from g_flat_base = 0x{flat_base:X} '
                f'(needs at least 0x{need:X}). recomp_dispatch_init() indexes '
                f'g_flat_table with that offset.')
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('paths', nargs='*', type=Path,
                        help='generated dispatch units; defaults to the live tree')
    args = parser.parse_args()

    paths = args.paths or [ROOT / 'src' / 'recomp' / 'gen' / 'recomp_dispatch.c']
    problems: list[str] = []
    for path in paths:
        problems.extend(check_file(path))

    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f'check-dispatch-table: FAIL ({len(problems)} problem(s))', file=sys.stderr)
        return 1
    print(f'check-dispatch-table: PASS ({len(paths)} file(s); declared size, '
          f'uniqueness, ordering and flat span all consistent)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
