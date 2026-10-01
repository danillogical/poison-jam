"""`scripts/generated-parity.py`: plan T15, generated code compared by function.

Two subcommands, both keyed by guest address rather than by file, because chunk
boundaries move between translator revisions and a file diff is mostly noise:

  * `audit BASELINE CANDIDATE` reports which functions were added, removed,
    changed (normalised body hash), or *subsumed* -- a baseline start that now lies
    inside a candidate function's `Original:` range because boundary repair merged
    it into its owner, which is a boundary change and not missing code.
  * `overlay BASELINE CANDIDATE OUTPUT [--minimum A] [--maximum B]` copies the
    candidate tree to OUTPUT and puts back the baseline body of every changed
    function in [A, B]. Building and running OUTPUT answers "did a function in this
    range move the stop?"; halving the range finds the one that did. Functions whose
    `Original:` range changed, or that now subsume a baseline start, are skipped and
    listed: swapping a body across a boundary change would not compile into the
    same program. OUTPUT gets an `overlay-manifest.json`. The canonical tree is
    never written.

A tree is a directory holding `recomp_NNNN.c` chunks, for example
`src/recomp/gen` and a copy of it from another revision (`git worktree` or
`git archive`).

Ported from Mercenaries-Recompiled's `tools/recomp/audit_generated_function_parity.py`
and `tools/recomp/make_generated_overlay.py` (revision c978ee7), which carry this
notice:

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Changes here: one script with two subcommands, the opening brace may share the
signature's line, and the canonical-reference scan is dropped.

Exit 0 success, 2 a tree could not be read.
"""
from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

FUNCTION_RE = re.compile(r'(?m)^void\s+sub_([0-9A-Fa-f]{8})\s*\(void\)\s*\{')
ORIGINAL_RE = re.compile(r'\* Original:\s*0x([0-9A-Fa-f]{8})\s*-\s*0x([0-9A-Fa-f]{8})')
SPACE_RE = re.compile(r'[ \t]+')


@dataclass(frozen=True)
class FunctionBody:
    address: int
    source: str          # chunk path relative to the tree
    start_line: int
    body: str
    original_end: int

    @property
    def digest(self) -> str:
        """Hash of the body with blank lines and runs of spaces normalised away."""
        normalized = '\n'.join(SPACE_RE.sub(' ', line.rstrip()).strip()
                               for line in self.body.replace('\r\n', '\n').splitlines()
                               if line.strip())
        return hashlib.sha256(normalized.encode()).hexdigest()


def closing_brace(text: str, opening: int) -> int:
    """Index just past the brace matching text[opening], skipping comments and literals."""
    depth, state, index = 0, 'code', opening
    while index < len(text):
        char = text[index]
        following = text[index + 1] if index + 1 < len(text) else ''
        if state == 'code':
            if char == '/' and following == '/':
                state, index = 'line', index + 1
            elif char == '/' and following == '*':
                state, index = 'block', index + 1
            elif char == '"':
                state = 'string'
            elif char == "'":
                state = 'char'
            elif char == '{':
                depth += 1
            elif char == '}':
                depth -= 1
                if depth == 0:
                    return index + 1
        elif state == 'line' and char == '\n':
            state = 'code'
        elif state == 'block' and char == '*' and following == '/':
            state, index = 'code', index + 1
        elif state in ('string', 'char'):
            if char == '\\':
                index += 1
            elif (state == 'string' and char == '"') or (state == 'char' and char == "'"):
                state = 'code'
        index += 1
    raise ValueError('unterminated generated function body')


def load_functions(root: Path) -> dict[int, FunctionBody]:
    result: dict[int, FunctionBody] = {}
    files = sorted(root.rglob('recomp_[0-9][0-9][0-9][0-9].c'))
    if not files:
        raise ValueError(f'no recomp_NNNN.c files under {root}')
    for path in files:
        text = path.read_text(encoding='utf-8')
        for match in FUNCTION_RE.finditer(text):
            address = int(match.group(1), 16)
            banners = list(ORIGINAL_RE.finditer(text, max(0, match.start() - 512), match.start()))
            if not banners or int(banners[-1].group(1), 16) != address:
                raise ValueError(f'missing or mismatched Original range for sub_{address:08X} in {path}')
            end = closing_brace(text, text.index('{', match.start(), match.end()))
            if address in result:
                raise ValueError(f'duplicate sub_{address:08X}: {result[address].source} and {path}')
            result[address] = FunctionBody(address, str(path.relative_to(root)),
                                           text.count('\n', 0, match.start()) + 1,
                                           text[match.start():end], int(banners[-1].group(2), 16))
    return result


def subsumed_by(baseline: dict[int, FunctionBody],
                candidate: dict[int, FunctionBody]) -> dict[int, int]:
    """Removed baseline starts that lie inside a candidate function, mapped to that owner."""
    starts = sorted(candidate)
    result: dict[int, int] = {}
    for address in set(baseline) - set(candidate):
        index = bisect.bisect_right(starts, address) - 1
        if index >= 0:
            owner = starts[index]
            if owner < address < candidate[owner].original_end:
                result[address] = owner
    return result


def entry(body: FunctionBody) -> dict[str, object]:
    return {'address': f'0x{body.address:08X}', 'source': body.source,
            'line': body.start_line, 'sha256': body.digest,
            'original_end': f'0x{body.original_end:08X}'}


def audit(baseline_root: Path, candidate_root: Path) -> dict[str, object]:
    baseline, candidate = load_functions(baseline_root), load_functions(candidate_root)
    subsumed = subsumed_by(baseline, candidate)
    removed = sorted(set(baseline) - set(candidate) - set(subsumed))
    added = sorted(set(candidate) - set(baseline))
    common = set(baseline) & set(candidate)
    changed = sorted(a for a in common if baseline[a].digest != candidate[a].digest)
    return {
        'baseline_root': str(baseline_root), 'candidate_root': str(candidate_root),
        'baseline_function_count': len(baseline),
        'candidate_function_count': len(candidate),
        'unchanged_function_count': len(common) - len(changed),
        'removed': [entry(baseline[a]) for a in removed],
        'subsumed': [{'address': f'0x{a:08X}', 'baseline': entry(baseline[a]),
                      'candidate_owner': entry(candidate[o])}
                     for a, o in sorted(subsumed.items())],
        'added': [entry(candidate[a]) for a in added],
        'changed': [{'address': f'0x{a:08X}', 'baseline': entry(baseline[a]),
                     'candidate': entry(candidate[a])} for a in changed],
    }


def replace_body(text: str, address: int, replacement: str) -> str:
    for match in FUNCTION_RE.finditer(text):
        if int(match.group(1), 16) == address:
            end = closing_brace(text, text.index('{', match.start(), match.end()))
            return text[:match.start()] + replacement + text[end:]
    raise ValueError(f'sub_{address:08X} not found in the copied candidate file')


def overlay(baseline_root: Path, candidate_root: Path, output_root: Path,
            minimum: int, maximum: int) -> dict[str, object]:
    if output_root.exists():
        raise ValueError(f'output already exists: {output_root}')
    baseline, candidate = load_functions(baseline_root), load_functions(candidate_root)
    owners = set(subsumed_by(baseline, candidate).values())
    selected: list[int] = []
    skipped: list[dict[str, str]] = []
    for address in sorted(set(baseline) & set(candidate)):
        old, new = baseline[address], candidate[address]
        if not (minimum <= address <= maximum) or old.digest == new.digest:
            continue
        reason = ('range-changed' if old.original_end != new.original_end else
                  'subsumes-baseline-entry' if address in owners else None)
        if reason:
            skipped.append({'address': f'0x{address:08X}', 'reason': reason})
        else:
            selected.append(address)
    shutil.copytree(candidate_root, output_root)
    by_file: dict[str, list[int]] = {}
    for address in selected:
        by_file.setdefault(candidate[address].source, []).append(address)
    for relative, addresses in by_file.items():
        path = output_root / relative
        text = path.read_text(encoding='utf-8')
        for address in addresses:
            text = replace_body(text, address, baseline[address].body)
        path.write_text(text, encoding='utf-8', newline='\n')
    manifest = {'baseline': str(baseline_root), 'candidate': str(candidate_root),
                'minimum': f'0x{minimum:08X}', 'maximum': f'0x{maximum:08X}',
                'overlaid': [f'0x{a:08X}' for a in selected], 'skipped': skipped}
    (output_root / 'overlay-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n',
                                                       encoding='utf-8')
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest='command', required=True)
    a = sub.add_parser('audit', help='report function-level differences')
    a.add_argument('baseline', type=Path)
    a.add_argument('candidate', type=Path)
    a.add_argument('--output', type=Path, help='write the JSON report here')
    o = sub.add_parser('overlay', help='candidate tree with baseline bodies in a range')
    o.add_argument('baseline', type=Path)
    o.add_argument('candidate', type=Path)
    o.add_argument('output', type=Path)
    o.add_argument('--minimum', type=lambda v: int(v, 0), default=0)
    o.add_argument('--maximum', type=lambda v: int(v, 0), default=0xFFFFFFFF)
    args = parser.parse_args()

    try:
        if args.command == 'audit':
            report = audit(args.baseline.resolve(), args.candidate.resolve())
            rendered = json.dumps(report, indent=2) + '\n'
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(rendered, encoding='utf-8')
            else:
                print(rendered, end='')
            print(f'generated parity: baseline={report["baseline_function_count"]} '
                  f'candidate={report["candidate_function_count"]} '
                  f'removed={len(report["removed"])} subsumed={len(report["subsumed"])} '
                  f'added={len(report["added"])} changed={len(report["changed"])}',
                  file=sys.stderr)
        else:
            manifest = overlay(args.baseline.resolve(), args.candidate.resolve(),
                               args.output.resolve(), args.minimum, args.maximum)
            print(f'overlaid={len(manifest["overlaid"])} skipped={len(manifest["skipped"])} '
                  f'output={args.output}')
    except (OSError, ValueError) as exc:
        print(f'generated-parity: {exc}', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
