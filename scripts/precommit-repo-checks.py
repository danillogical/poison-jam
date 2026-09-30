#!/usr/bin/env python
"""Pre-commit gate 2: the repository's own checkers, on every commit.

Plan T7 names these hooks.  A checker that only runs when someone remembers is a
checker that does not run; the measured failure this answers is a stale document
or a broken generated tree reaching `master` because the check lived in a
session's memory instead of in the commit path.

Three hooks, chosen because each one guards a failure this repository has
actually paid for:

  * `check-agent-docs.py --check` -- a fact duplicated across the agent-facing
    documents (five stale claims survived on 2026-09-22 that way), and now the
    plan-T6 recipe names.
  * `check-merge-structure.py` -- conflict markers, duplicate `case` labels and
    duplicate file-scope definitions, which a textual merge resolution can leave
    behind while every conflict is nominally "resolved".
  * `check-generation-provenance.py --check` -- generated output that no longer
    matches the source it claims to come from.

Only the last is skipped when the generated tree is untouched, because it hashes
a large tree; the skip is decided from the staged path list, not from a flag, so
it cannot be silently disabled by forgetting to pass something.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PYTHON = sys.executable

# check-generation-provenance.py hashes the whole generated tree; run it only
# when something under it is staged.
GENERATED_PREFIXES = ('src/recomp/gen/', 'config/')


def staged_paths() -> list[str]:
    completed = subprocess.run(
        ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMRD'],
        capture_output=True, text=True, cwd=str(ROOT))
    if completed.returncode != 0:
        print(f'could not read the index: {completed.stderr.strip()}', file=sys.stderr)
        raise SystemExit(2)
    return [line.replace('\\', '/') for line in completed.stdout.splitlines() if line.strip()]


def run_check(label: str, argv: list[str]) -> bool:
    print(f'  [{label}] {" ".join(argv[2:])}')
    completed = subprocess.run(argv, cwd=str(ROOT))
    if completed.returncode != 0:
        print(f'  [{label}] FAILED (exit {completed.returncode})')
        return False
    return True


def main() -> int:
    paths = staged_paths()
    checks: list[tuple[str, list[str]]] = [
        ('agent-docs', [PYTHON, '-X', 'utf8', 'scripts/check-agent-docs.py', '--check']),
        ('merge-structure', [PYTHON, '-X', 'utf8', 'scripts/check-merge-structure.py']),
    ]
    if any(p.startswith(GENERATED_PREFIXES) for p in paths):
        checks.append(('generation-provenance',
                       [PYTHON, '-X', 'utf8', 'scripts/check-generation-provenance.py',
                        '--check']))
    else:
        print('  [generation-provenance] skipped: no generated or config path staged')

    ok = True
    for label, argv in checks:
        if not run_check(label, argv):
            ok = False

    if not ok:
        print()
        print('  repository check(s) failed; commit refused.')
        return 1
    print(f'  repository checks: {len(checks)} passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
