"""Fail when a lifted SSE-compare consumer ignores the unordered (NaN) case.

x86 `comiss`/`ucomiss`/`comisd`/`ucomisd` set ZF=PF=CF=1 when either operand is
NaN. So after a NaN compare `je`, `jb`, `jbe`, `jp` and `cmovb` are taken, and
`jne`, `ja`, `jae` and `jnp` are not. C's relational operators are all false on
NaN, so a lifted condition that tests the bare relation gets `==`, `<` and `<=`
wrong (the branch must be taken) and `!=` wrong (it must not be).

The lifter emits the compare as `_fca = ...; _fcb = ...; /* comiss */` and every
consumer as a condition over `_fca`/`_fcb`. Lifter output is correct since
toolkit commit 894ceef, but a generated body is only as new as its last
regeneration, so a stale chunk keeps the old spelling. This checker reads the
bodies in `src/recomp/gen/*.c` and fails on any stale condition:

    == < <=   must be `(REL || (_fca != _fca || _fcb != _fcb))`
    !=        must be `(REL && !(_fca != _fca || _fcb != _fcb))`
    > >=      need nothing: false on NaN is what ja/jae do

`jp`/`jnp` test only the unordered term and have no relation to check.

Usage:  check-sse-unordered.py [PATH ...]   (files or directories of *.c;
default src/recomp/gen). A built-in positive control runs first and the checker
fails if the detector does not flag a known-stale condition.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DIR = ROOT / 'src' / 'recomp' / 'gen'

COMPARE_RE = re.compile(r'/\*\s*u?comis[sd]\s*\*/')
FUNC_RE = re.compile(r'^(?:static\s+)?\w[\w\s\*]*?\b(\w+)\s*\([^;{]*\)\s*$')
REL_RE = re.compile(r'\b(_fc[ab])\s*(==|!=|<=|>=|<|>)\s*(_fc[ab])\b')
UNORDERED = r'\(\s*_fca\s*!=\s*_fca\s*\|\|\s*_fcb\s*!=\s*_fcb\s*\)'
FLIP = {'<': '>', '>': '<', '<=': '>=', '>=': '<=', '==': '==', '!=': '!='}


def stale_relations(line: str) -> list[tuple[str, str]]:
    """(relation, why) for each condition in `line` that lacks the unordered term."""
    out = []
    for m in REL_RE.finditer(line):
        lhs, op, rhs = m.groups()
        if lhs == rhs:
            continue
        if lhs == '_fcb':
            op = FLIP[op]
        rest = line[m.end():]
        if op in ('==', '<', '<='):
            ok = re.match(r'\s*\|\|\s*' + UNORDERED, rest)
            why = f'{op} must also be true when unordered'
        elif op == '!=':
            ok = re.match(r'\s*&&\s*!\s*' + UNORDERED, rest)
            why = '!= must be false when unordered'
        else:
            continue
        if not ok:
            out.append((f'{m.group(1)} {m.group(2)} {m.group(3)}', why))
    return out


def scan_text(text: str):
    """Yield (line_no, function, had_compare, relation, why) per stale condition."""
    func, seen_compare = '?', False
    for no, line in enumerate(text.split('\n'), 1):
        if line and not line[0].isspace():
            m = FUNC_RE.match(line)
            if m:
                func, seen_compare = m.group(1), False
        if COMPARE_RE.search(line):
            seen_compare = True
        for rel, why in stale_relations(line):
            yield no, func, seen_compare, rel, why


def selfcheck() -> str | None:
    bad = {
        'je': 'if ((_fca == _fcb)) goto loc_00000001;',
        'jb': 'if ((_fca < _fcb)) goto loc_00000001;',
        'jbe': 'if ((_fca <= _fcb)) eax = ecx;',
        'jne': 'if ((_fca != _fcb)) goto loc_00000001;',
        'jb reversed': 'if ((_fcb > _fca)) goto loc_00000001;',
    }
    good = {
        'je': 'if ((_fca == _fcb || (_fca != _fca || _fcb != _fcb))) goto L;',
        'jb': 'if ((_fca < _fcb || (_fca != _fca || _fcb != _fcb))) goto L;',
        'jbe': 'if ((_fca <= _fcb || (_fca != _fca || _fcb != _fcb))) eax = ecx;',
        'jne': 'if ((_fca != _fcb && !(_fca != _fca || _fcb != _fcb))) goto L;',
        'ja': 'if ((_fca > _fcb)) goto L;',
        'jae': 'if ((_fca >= _fcb)) goto L;',
        'jp': 'if ((_fca != _fca || _fcb != _fcb)) goto L;',
        'jnp': 'if (!(_fca != _fca || _fcb != _fcb)) goto L;',
        'snapshot': '_fca = xmm0.f[0]; _fcb = xmm7.f[0]; /* comiss */',
    }
    for name, line in bad.items():
        if not stale_relations(line):
            return f'positive control failed: stale {name} not flagged: {line}'
    for name, line in good.items():
        if stale_relations(line):
            return f'negative control failed: correct {name} flagged: {line}'
    return None


def gather(paths: list[str]) -> list[Path]:
    files: list[Path] = []
    for p in map(Path, paths or [str(DEFAULT_DIR)]):
        files.extend(sorted(p.glob('*.c')) if p.is_dir() else [p])
    return files


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('paths', nargs='*', help='files or directories (default src/recomp/gen)')
    args = ap.parse_args()

    problem = selfcheck()
    if problem:
        print(f'check-sse-unordered: FAIL {problem}')
        return 1
    files = gather(args.paths)
    if not files or not all(f.is_file() for f in files):
        print('check-sse-unordered: FAIL no generated sources to scan '
              f'({args.paths or DEFAULT_DIR})')
        return 1

    compares, findings = 0, []
    for f in files:
        text = f.read_text(encoding='utf-8', errors='replace')
        compares += len(COMPARE_RE.findall(text))
        findings += [(f, *r) for r in scan_text(text)]
    for f, no, func, had, rel, why in findings:
        note = '' if had else ' (no preceding compare in function)'
        print(f'{f.name}:{no}: {func}: stale `{rel}`: {why}{note}')
    if findings:
        print(f'check-sse-unordered: FAIL {len(findings)} stale condition(s) '
              f'in {len({x[0] for x in findings})} file(s); regenerate with '
              '`scripts/relift-selected.py comiss`')
        return 1
    print(f'check-sse-unordered: ok ({compares} SSE compares in {len(files)} files)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
