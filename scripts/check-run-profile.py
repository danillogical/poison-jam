"""Reclassify archived run profiles; only a checked strict archive is clean.

Usage::

    python -X utf8 scripts/check-run-profile.py <run-dir> [...]
    python -X utf8 scripts/check-run-profile.py --all

Exit status is 1 for exploratory, fixture, unknown, or missing evidence. Legacy
settings can reveal an exploratory override, but cannot establish strict status
without versioned profile and provenance fields.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from jsrf_run_profile import (
    EXPLORATORY,
    FIXTURE,
    MISSING,
    STRICT,
    UNKNOWN,
    classify_run_directory,
)


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', nargs='*', help='run directory names or paths')
    parser.add_argument('--all', action='store_true',
                        help='check every run under logs/runs/')
    args = parser.parse_args()

    if args.all:
        runs_root = ROOT / 'logs' / 'runs'
        targets = sorted((path for path in runs_root.glob('*') if path.is_dir()),
                         key=lambda path: path.name.casefold())
        if not targets:
            targets = [runs_root]
    elif args.runs:
        targets = []
        for name in args.runs:
            path = Path(name)
            if not path.is_dir():
                path = ROOT / 'logs' / 'runs' / name
            targets.append(path)
    else:
        parser.error('name at least one run, or pass --all')

    tally = {STRICT: 0, EXPLORATORY: 0, FIXTURE: 0, UNKNOWN: 0, MISSING: 0}
    failures = 0
    for run in targets:
        result = classify_run_directory(run)
        status = result.get('classification', UNKNOWN)
        if status not in tally:
            status = UNKNOWN
        tally[status] += 1
        print('%-52s %s' % (run.name or str(run), status.upper()))
        if status != STRICT:
            failures += 1
            for reason in result.get('reasons', []):
                print('      %s' % reason)

    print()
    print('checked %d; strict %d, exploratory %d, fixture %d, unknown %d, missing %d'
          % (len(targets), tally[STRICT], tally[EXPLORATORY], tally[FIXTURE],
             tally[UNKNOWN], tally[MISSING]))
    if failures:
        print()
        print('Only a reclassified strict run can support strict boot, audio, GPU, '
              'or liveness claims.')
        print('Flagging a run requires claim-by-claim review; the count alone does '
              'not establish that any particular claim was wrong.')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
