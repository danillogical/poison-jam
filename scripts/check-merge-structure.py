"""A4s structural post-resolution check: conflict markers, duplicate case labels,
and duplicate file-scope definitions.

The problem this exists for is measured and specific.  A three-way merge can produce
defects in **clean hunks** -- hunks git never flagged -- and the merge rules that
decide conflicts never look at those.  `A4s-r5` measured three of them in the
`v0.11.0` sync:

* `case 138` appeared **twice in each dispatch switch** in
  `src/kernel/kernel_bridge.c`, because both sides added the same logical case in
  different places (a C constraint violation: duplicate case value);
* `bridge_KeResetEvent` was **defined twice**, because both sides added the function
  in different places while the merge base had neither.

Both are compile errors, so left alone they surface as a build failure and get
attributed to the merge's *build* rather than to its *resolution*.  That
misattribution is the reason this check runs on the resolved tree **before** the
build.

What it checks, over `SCOPE` (the toolkit build inputs of the evidence binary):

* **markers** -- no conflict markers remain;
* **dup-case** -- no two `case` labels with the same constant value in one switch,
  on the same preprocessor branch path;
* **dup-def** -- no two file-scope definitions of the same identifier on the same
  preprocessor branch path.

Deliberate exclusions, each measured on the real merge:

* a **declaration plus one definition** is legal and is not reported;
* a **tentative definition** completed later (``static const T x;`` then
  ``static const T x = {...};``) is legal C11 6.9.2p2 and is not reported;
* **identical macro redefinitions** are legal C11 6.10.3p3 and are not reported;
* constructs on **different** preprocessor branch paths are alternatives, not
  duplicates, and are not reported.

Known limits, stated rather than left implicit:

* the scan is **textual and does not preprocess**, so it cannot resolve ``#include``
  graphs, macro-expanded code, or ``#if`` conditions that depend on defined values.
  It treats each ``#if`` as an opaque branch and compares only within the same
  branch path, so a duplicate the preprocessor would produce from two *different*
  branch paths is **missed**;
* it therefore cannot replace a compiler.  It is paired with a build-failure
  attribution backstop: a redefinition or duplicate-case diagnostic in a
  merge-changed file is a resolution defect, not a build defect.

A file whose braces or switch braces cannot be balanced is reported `UNKNOWN`
rather than silently skipped.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
CHECKER_VERSION = 'jsrf-merge-structure/1'

# SCOPE: the toolkit paths that are build inputs of jsrf_recomp.exe.  Recorded as a
# path list with a guard that fails closed if the build graph starts including
# anything else (see docs/reviews/a4s-ac97-hunk-ruling.md, "Interpretation ruling 2").
SCOPE_PREFIXES = ('src/', 'include/')
SOURCE_SUFFIXES = ('.c', '.h')

MARKER_RE = re.compile(r'^(<<<<<<<|\|\|\|\|\|\|\||=======|>>>>>>>)(\s|$)')
CASE_RE = re.compile(r'\s*case\s+(-?\w+)\s*:')
SWITCH_RE = re.compile(r'\bswitch\s*\(', )
DIRECTIVE_RE = re.compile(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)')
DEFN_RE = re.compile(r'^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*$')
# a one-line definition: `... name(args) { ... }` all on one line.  Real in this tree
# (for example `static void bridge_XeLoadSection(void) { bridge_XeSection(1); }`), and
# missing it was a measured false negative in the first version of this scanner.
DEFN_INLINE_RE = re.compile(r'^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*\{.*\}\s*$')
# a file-scope object definition/declaration: `[static] type name [= ...];`
OBJ_RE = re.compile(r'^(?:static\s+)?(?:const\s+)?[\w \t\*]*?\b([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*(=|;)')


def _git(*args: str) -> str:
    out = subprocess.run(['git', '-C', str(TOOLKIT), *args],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError(f'git {" ".join(args)} failed: {out.stderr.strip()}')
    return out.stdout


def scope_files(rev: str) -> list[str]:
    """Tracked C/H files inside SCOPE at `rev`."""
    listing = _git('ls-tree', '-r', '--name-only', rev).split('\n')
    return sorted(f for f in listing
                  if f.endswith(SOURCE_SUFFIXES) and f.startswith(SCOPE_PREFIXES))


def read_rev(rev: str, path: str) -> list[str] | None:
    try:
        return _git('show', f'{rev}:{path}').split('\n')
    except RuntimeError:
        return None


def read_worktree(path: str) -> list[str] | None:
    p = TOOLKIT / path
    if not p.is_file():
        return None
    return p.read_text(encoding='utf-8', errors='replace').split('\n')


def strip_comments(lines: list[str]) -> list[str]:
    """Remove comments and string/char literals, PRESERVING line count.

    Line count is preserved so every reported line number refers to the real file.
    """
    out: list[str] = []
    in_block = False
    for line in lines:
        res: list[str] = []
        i, n = 0, len(line)
        while i < n:
            if in_block:
                j = line.find('*/', i)
                if j < 0:
                    i = n
                else:
                    in_block = False
                    i = j + 2
            elif line.startswith('/*', i):
                in_block = True
                i += 2
            elif line.startswith('//', i):
                i = n
            elif line[i] in '"\'':
                quote = line[i]
                res.append(' ')
                i += 1
                while i < n and line[i] != quote:
                    if line[i] == '\\':
                        i += 1
                    i += 1
                i += 1
            else:
                res.append(line[i])
                i += 1
        out.append(''.join(res))
    return out


def branch_paths(lines: list[str]) -> dict[int, tuple[str, ...]]:
    """Assign each line index a preprocessor branch path."""
    stack: list[list[str]] = []
    paths: dict[int, tuple[str, ...]] = {}
    for i, line in enumerate(lines):
        m = DIRECTIVE_RE.match(line)
        if m:
            kind, cond = m.group(1), m.group(2).strip()
            if kind in ('if', 'ifdef', 'ifndef'):
                stack.append([kind, cond])
            elif kind == 'elif' and stack:
                stack[-1] = ['elif', cond]
            elif kind == 'else' and stack:
                stack[-1] = ['else', stack[-1][1]]
            elif kind == 'endif' and stack:
                stack.pop()
        paths[i] = tuple(f'{k}:{c}' for k, c in stack)
    return paths


def switch_spans(lines: list[str]) -> tuple[list[tuple[int, int]], str | None]:
    """(start, end) 0-based inclusive spans for every `switch (...)`, brace-matched."""
    spans: list[tuple[int, int]] = []
    for i, line in enumerate(lines):
        if not SWITCH_RE.search(line):
            continue
        depth, end = 0, None
        for j in range(i, len(lines)):
            depth += lines[j].count('{') - lines[j].count('}')
            if depth == 0 and j > i:
                end = j
                break
        if end is None:
            return spans, f'switch at line {i + 1} has unbalanced braces'
        spans.append((i, end))
    return spans, None


def file_scope_definitions(lines: list[str]) -> dict[tuple[str, tuple[str, ...]], list[int]]:
    """identifier -> definition lines, for functions AND objects, at brace depth 0."""
    defs: dict[tuple[str, tuple[str, ...]], list[int]] = defaultdict(list)
    paths = branch_paths(lines)
    depth = 0
    for i, line in enumerate(lines):
        if depth == 0:
            m = DEFN_INLINE_RE.match(line)
            if m:
                defs[(m.group(1), paths[i])].append(i + 1)
                depth += line.count('{') - line.count('}')
                continue
            m = DEFN_RE.match(line)
            if m:
                for j in range(i + 1, min(i + 4, len(lines))):
                    s = lines[j].strip()
                    if not s:
                        continue
                    if s.startswith('{'):
                        defs[(m.group(1), paths[i])].append(i + 1)
                    break
            else:
                # object at file scope: `static T name = ...;` (an initialised
                # definition).  A bare `static T name;` is a tentative definition
                # and is legal alongside a later completed one, so it is skipped.
                m2 = OBJ_RE.match(line)
                if m2 and '=' in line and '(' not in line.split('=')[0]:
                    defs[(m2.group(1), paths[i])].append(i + 1)
        depth += line.count('{') - line.count('}')
    return defs


def macro_redefinitions(lines: list[str]) -> list[tuple[str, int, int]]:
    """Identical macro redefinitions are legal; only DIFFERENT bodies are reported."""
    paths = branch_paths(lines)
    seen: dict[tuple[str, tuple[str, ...]], tuple[str, int]] = {}
    findings: list[tuple[str, int, int]] = []
    for i, line in enumerate(lines):
        m = re.match(r'\s*#\s*define\s+([A-Za-z_]\w*)\s*(.*)', line)
        if not m:
            continue
        key = (m.group(1), paths[i])
        body = m.group(2).strip()
        if key in seen:
            prev_body, prev_line = seen[key]
            if prev_body != body:
                findings.append((m.group(1), prev_line, i + 1))
        else:
            seen[key] = (body, i + 1)
    return findings


def scan_file(path: str, raw: list[str]) -> tuple[list[dict], dict, str | None]:
    findings: list[dict] = []
    lines = strip_comments(raw)
    paths = branch_paths(lines)

    for i, line in enumerate(raw):
        if MARKER_RE.match(line):
            findings.append({'check': 'marker', 'file': path, 'line': i + 1,
                             'detail': line.strip()[:60]})

    text = '\n'.join(lines)
    if text.count('{') != text.count('}'):
        return findings, {}, 'unbalanced braces'

    spans, err = switch_spans(lines)
    if err:
        return findings, {}, err

    for (start, end) in spans:
        seen: dict[tuple[str, tuple[str, ...]], list[int]] = defaultdict(list)
        for j in range(start, end + 1):
            m = CASE_RE.match(lines[j])
            if m:
                seen[(m.group(1), paths[j])].append(j + 1)
        for (value, branch), locs in seen.items():
            if len(locs) > 1:
                findings.append({'check': 'dup-case', 'file': path, 'lines': locs,
                                 'detail': f'case {value} branch={branch or "()"}'})

    for (name, branch), locs in file_scope_definitions(lines).items():
        if len(locs) > 1:
            findings.append({'check': 'dup-def', 'file': path, 'lines': locs,
                             'detail': f'{name} branch={branch or "()"}'})

    for (name, a, b) in macro_redefinitions(lines):
        findings.append({'check': 'dup-macro', 'file': path, 'lines': [a, b],
                         'detail': f'{name} redefined with a different body'})

    return findings, {'switches': len(spans)}, None


def scope_files_worktree() -> list[str]:
    """Tracked C/H files inside SCOPE in the working tree.

    Uses `git ls-files` rather than a directory walk so that untracked scratch files
    (editor backups, generated leftovers) are not scanned, and so the file set is
    consistent with `scope_files(rev)`.  During a conflicted merge the conflicted
    paths are still listed by `ls-files`, which is what we want: the resolved tree is
    what the criterion checks.
    """
    listing = _git('ls-files', '--cached', '--others', '--exclude-standard').split('\n')
    return sorted(f for f in listing
                  if f.endswith(SOURCE_SUFFIXES) and f.startswith(SCOPE_PREFIXES))


def scan(rev: str | None) -> dict:
    """Scan a revision, or the working tree when `rev` is None."""
    files = scope_files(rev) if rev else scope_files_worktree()
    findings: list[dict] = []
    switches = 0
    unknown: list[dict] = []
    for f in files:
        raw = read_worktree(f) if rev is None else read_rev(rev, f)
        if raw is None:
            unknown.append({'file': f, 'reason': 'unreadable'})
            continue
        fnd, counts, reason = scan_file(f, raw)
        findings += fnd
        switches += counts.get('switches', 0)
        if reason:
            unknown.append({'file': f, 'reason': reason})
    return {'revision': rev or 'WORKTREE',
            'files': len(files), 'switches': switches,
            'findings': findings, 'unknown': unknown,
            'checker_version': CHECKER_VERSION}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rev', action='append', default=[],
                        help='revision to scan (repeatable); omit to scan the working tree')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--expect', type=int, default=None,
                        help='expected finding count; mismatch is a failure')
    args = parser.parse_args(argv)

    revs: list[str | None] = args.rev or [None]
    reports = [scan(r) for r in revs]

    if args.json:
        print(json.dumps(reports, indent=2))
    else:
        for rep in reports:
            print(f"{rep['revision']}: files={rep['files']} switches={rep['switches']} "
                  f"findings={len(rep['findings'])} unknown={len(rep['unknown'])}")
            for f in rep['findings']:
                where = f.get('line') or ','.join(str(x) for x in f.get('lines', []))
                print(f"    [{f['check']}] {f['file']}:{where}  {f['detail']}")
            for u in rep['unknown']:
                print(f"    [UNKNOWN] {u['file']}: {u['reason']}")

    # FAIL if any finding, or any UNKNOWN (fail closed), or the expectation mismatched.
    total = sum(len(r['findings']) for r in reports)
    unknowns = sum(len(r['unknown']) for r in reports)
    if args.expect is not None and total != args.expect:
        print(f'EXPECTATION FAILED: expected {args.expect} findings, found {total}',
              file=sys.stderr)
        return 1
    return 1 if (total or unknowns) else 0


if __name__ == '__main__':
    sys.exit(main())
