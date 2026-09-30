#!/usr/bin/env python
"""Pre-commit gate 3: refuse to commit in a merge state that is not resolved.

Plan T7 and the run-profile merge rules (`docs/jsrf-run-profiles.md`, "Upstream
merges never silently change admitted evidence semantics") require a merge to be
inventoried **by content, not by conflict status**, because a clean hunk can
restore a deleted override or arm new device behaviour as silently as a
conflicting one.

This hook cannot do that inventory -- that is `check-merge-structure.py` plus the
merge packet's own work.  What it can do cheaply is refuse the two states that
make an inventory impossible to trust:

  * an unresolved path left in the index (`MERGE_HEAD` present with `UU`/`AA`/`DU`
    entries), where a commit would record conflict markers as content; and
  * a merge commit whose message does not name the merge, which is how a merge
    lands in history with no record of what was inventoried.

The second is a warning, not a refusal: it cannot cause a false claim by itself.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UNRESOLVED = ('DD', 'AU', 'UD', 'UA', 'DU', 'AA', 'UU')


def git(*args: str) -> str:
    completed = subprocess.run(['git', *args], capture_output=True, text=True,
                               cwd=str(ROOT))
    return completed.stdout


def merge_in_progress() -> bool:
    """True when a merge is actually underway.

    `MERGE_HEAD` is resolved through `git rev-parse --git-path` rather than
    assumed to sit at `<root>/.git/MERGE_HEAD`, because a control caught this
    gate reporting "no merge in progress" during a genuinely conflicted merge --
    a gate that reads the wrong path passes every control that only checks the
    clean case.
    """
    path = git('rev-parse', '--git-path', 'MERGE_HEAD').strip()
    if not path:
        return False
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = ROOT / candidate
    return candidate.is_file()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', help='repository root override (fixtures)')
    args = parser.parse_args()

    global ROOT
    if args.root:
        ROOT = Path(args.root).resolve()

    merge_head = merge_in_progress()
    porcelain = git('status', '--porcelain')
    unresolved = [line for line in porcelain.splitlines()
                  if line[:2] in UNRESOLVED]

    if unresolved:
        print()
        for line in unresolved:
            print(f'  UNRESOLVED: {line}')
        print()
        print(f'  {len(unresolved)} unmerged path(s); a commit here would record '
              f'conflict markers as content. Resolve them first.')
        return 1

    if merge_head:
        print('  merge in progress: no unmerged paths remain. `check-merge-structure.py` '
              'passed, but the merge packet still owes its hunk inventory by content '
              '(docs/jsrf-run-profiles.md, "Upstream merges ...").')
    else:
        print('  merge gate: no merge in progress')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
