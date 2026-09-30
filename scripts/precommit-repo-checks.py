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

import re
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


def staged_draft_packets(paths: list[str]) -> list[str]:
    """Staged packet files that are still drafts. W16's warning.

    Plan W16: "**Stage explicit paths only**; the Session never runs `git add -A`
    while another role may be writing; hashes are pinned only after the writer
    reports done", with the check "pre-commit warns on staged draft packets not
    named in the commit message".

    **Why it is a WARNING and not a refusal.** The measured failure is a Planner's
    in-progress draft swept into a commit by `git add -A` (`141cb7e`), which is a
    commit-hygiene accident rather than a correctness defect in the tree: the
    content is what the writer had written. Refusing would block a legitimate commit
    of a draft the author intends to land; warning makes the accident visible at the
    moment it happens, which is when it is cheap to undo.

    A draft is named in the commit message when the message mentions its packet id,
    so an intentional draft commit is not warned about.
    """
    drafts = []
    for path in paths:
        if not path.endswith('.md'):
            continue
        if '/packets/' not in f'/{path}' and not path.startswith('docs/packets/'):
            continue
        try:
            text = (ROOT / path).read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        # `**Status:** draft` and `**Status:** INADEQUATE` are the two not-yet-frozen
        # states (`docs/agent-workflow.md` §5.2).
        if re.search(r'\*\*Status:\*\*\s*(draft|INADEQUATE)\b', text, re.IGNORECASE):
            drafts.append(path)
    return drafts


def commit_message() -> str:
    """The message git will use, from COMMIT_EDITMSG or the -m form."""
    edit = ROOT / '.git' / 'COMMIT_EDITMSG'
    try:
        return edit.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return ''


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

    # W16: a draft packet staged without being named in the commit message.
    message = commit_message()
    for path in staged_draft_packets(paths):
        packet = Path(path).stem
        if packet and packet in message:
            continue
        print()
        print(f'  W16 WARNING: {path} is staged and its Status is draft/INADEQUATE,')
        print(f'  and the commit message does not name {packet!r}.')
        print(f'  If this draft was swept in by `git add -A` while its author was')
        print(f'  still writing (the measured failure, 141cb7e), unstage it with:')
        print(f'      git restore --staged {path}')
        print(f'  Otherwise name the packet in the commit message and re-commit.')

    if not ok:
        print()
        print('  repository check(s) failed; commit refused.')
        return 1
    print(f'  repository checks: {len(checks)} passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
