"""Compute a retention plan for `logs/runs`: what is protected, what could go.

**This script never deletes anything.**  Plan T14 makes removal an owner decision;
the job here is to make that decision cheap by presenting exact numbers and exact
candidate directories, so the owner chooses between measured options rather than
approving a guess.

Protection is by **citation**, not by age.  An archived run is the evidence a
durable record points at, and `docs/jsrf-run-profiles.md` makes the run directory
the unit a reader re-derives a value from.  The rule is:

  * **Protected** -- any run directory named in a durable record (the plan, the
    technical record, a packet, a review record, the horizon ledger), plus the most
    recent N runs regardless of citation.
  * **Candidate** -- everything else, split by kind:
      - `test-*` harness probes: regenerable by re-running the harness.
      - named evidence runs: kept unless uncited, because a record may cite one
        under a name this scan cannot see (an abbreviated run ID, a hash).

The scan is deliberately conservative in the direction that matters: an
unreadable record is reported, not silently treated as carrying no citations,
because reading zero citations would offer a cited run for deletion.

Historical note: the previous version of this file bucketed runs by `save-root/`
versus evidence and reported LOGICAL size, which overstated the archive by ~16x
(2,735 GB logical against 172 GB actually allocated).  It now reports real
allocation, sparse-aware, using the same primitive as `scripts/disk-usage.py`.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import re
import sys
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Every durable record that can cite a run.  A run named anywhere in one of these
# is evidence for a claim and must not be offered for removal.
RECORD_GLOBS = (
    'plan-*.md',
    'report-*.md',
    'AGENTS.md',
    'docs/*.md',
    'docs/reviews/*.md',
    'docs/reviews/*.json',
    'docs/packets/*.md',
    'docs/reviews/**/*.json',
)
KEEP_RECENT_DEFAULT = 20

RUN_NAME_RE = re.compile(r'\d{8}-\d{6}-\d{3}-[A-Za-z0-9_.-]+')

_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_GetCompressedFileSizeW = _kernel32.GetCompressedFileSizeW
_GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
_GetCompressedFileSizeW.restype = wintypes.DWORD
_INVALID = 0xFFFFFFFF


def allocated(path: Path) -> int:
    """Real on-disk allocation (sparse-aware), falling back to logical size."""
    if sys.platform != 'win32':
        try:
            return path.stat().st_size
        except OSError:
            return 0
    hi = wintypes.DWORD(0)
    lo = _GetCompressedFileSizeW(str(path), ctypes.byref(hi))
    if lo == _INVALID and ctypes.get_last_error() != 0:
        try:
            return path.stat().st_size
        except OSError:
            return 0
    return (hi.value << 32) | lo


def dir_allocation(run: Path) -> tuple[int, int]:
    files = 0
    total = 0
    for path in run.rglob('*'):
        try:
            if not path.is_file():
                continue
        except OSError:
            continue
        files += 1
        total += allocated(path)
    return files, total


def cited_run_names() -> tuple[set[str], list[str]]:
    """Names cited by durable records, plus the records that could not be read."""
    cited: set[str] = set()
    unreadable: list[str] = []
    seen: set[Path] = set()
    for pattern in RECORD_GLOBS:
        for path in sorted(ROOT.glob(pattern)):
            if not path.is_file() or path in seen:
                continue
            seen.add(path)
            try:
                text = path.read_text(encoding='utf-8', errors='replace')
            except OSError as error:
                unreadable.append(f'{path.relative_to(ROOT)}: {error}')
                continue
            cited.update(RUN_NAME_RE.findall(text))
    return cited, unreadable


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runs-root', default=str(ROOT / 'logs' / 'runs'))
    parser.add_argument('--keep-recent', type=int, default=KEEP_RECENT_DEFAULT,
                        help='always protect this many newest runs (default %(default)s)')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--list-candidates', action='store_true',
                        help='print every candidate directory, not just the summary')
    args = parser.parse_args()

    runs_root = Path(args.runs_root)
    if not runs_root.is_dir():
        print(f'no run archive at {runs_root}', file=sys.stderr)
        return 2

    cited, unreadable = cited_run_names()
    runs = sorted((p for p in runs_root.iterdir() if p.is_dir()), key=lambda p: p.name)
    recent = {p.name for p in runs[-args.keep_recent:]} if args.keep_recent else set()

    protected: list[dict] = []
    candidates: list[dict] = []
    for run in runs:
        files, size = dir_allocation(run)
        reasons = []
        if run.name in cited:
            reasons.append('cited')
        if run.name in recent:
            reasons.append('recent')
        entry = {'name': run.name, 'files': files, 'bytes': size,
                 'gb': round(size / 1024 ** 3, 3), 'reasons': reasons}
        (protected if reasons else candidates).append(entry)

    by_kind: dict[str, dict] = {}
    for entry in candidates:
        kind = 'test' if entry['name'].split('-', 3)[-1].startswith('test-') else 'named'
        bucket = by_kind.setdefault(kind, {'count': 0, 'bytes': 0})
        bucket['count'] += 1
        bucket['bytes'] += entry['bytes']

    protected_bytes = sum(e['bytes'] for e in protected)
    candidate_bytes = sum(e['bytes'] for e in candidates)
    record = {
        'plan': 'jsrf-retention-plan/1',
        'runs_root': str(runs_root),
        'runs_total': len(runs),
        'keep_recent': args.keep_recent,
        'cited_names_found': len(cited),
        'unreadable_records': unreadable,
        'protected_runs': len(protected),
        'protected_bytes': protected_bytes,
        'protected_gb': round(protected_bytes / 1024 ** 3, 2),
        'candidate_runs': len(candidates),
        'candidate_bytes': candidate_bytes,
        'candidate_gb': round(candidate_bytes / 1024 ** 3, 2),
        'candidates_by_kind': {k: {'count': v['count'],
                                   'gb': round(v['bytes'] / 1024 ** 3, 2)}
                               for k, v in sorted(by_kind.items())},
    }

    if args.json:
        print(json.dumps({'summary': record,
                          'protected': protected,
                          'candidates': candidates}, indent=2, sort_keys=True))
    else:
        print(f'  runs root:        {runs_root}')
        print(f'  runs total:       {len(runs)}')
        print(f'  cited run names:  {len(cited)} found in durable records')
        print(f'  protected:        {len(protected)} runs, {record["protected_gb"]} GB '
              f'(cited or newest {args.keep_recent})')
        print(f'  CANDIDATES:       {len(candidates)} runs, {record["candidate_gb"]} GB')
        for kind, bucket in sorted(by_kind.items()):
            print(f'      {kind:<8} {bucket["count"]:>5} runs  '
                  f'{round(bucket["bytes"] / 1024 ** 3, 2):>8} GB')
        if unreadable:
            print(f'  WARNING: {len(unreadable)} record(s) could not be read; '
                  f'their citations are missing from this plan:')
            for item in unreadable:
                print(f'      {item}')
        print()
        print('  Nothing was deleted. Removing any candidate is an owner decision')
        print('  (plan T14); this script only measures.')
        if args.list_candidates:
            print()
            for entry in candidates:
                print(f'      {entry["name"]}  {entry["gb"]:.3f} GB')
    return 0


if __name__ == '__main__':
    sys.exit(main())
