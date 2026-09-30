#!/usr/bin/env python
"""Pre-commit gate 1: refuse a staged path under `game/`, and refuse secrets.

Plan T7 names both refusals.  This is a **staged-blob** check, not a working-tree
check: the thing being prevented is a nondistributable retail asset entering a
commit, and a file that is merely present in the working tree cannot do that.

The game repository is public.  `game/` is gitignored, so a normal `git add`
cannot reach it -- but `git add -f`, a `.gitignore` edit, or a rename can, and
the failure is unrecoverable without a history rewrite the owner has to authorize.
Refusing it before the commit exists is the cheap end of that trade.

Secrets are scanned with the same pattern set `scripts/secret-audit.py` uses, so
the pre-commit gate and the pre-push audit cannot disagree about what a secret is.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))

# The single secret-pattern table, shared with scripts/secret-audit.py.  Two
# hand-copied lists would drift, and the weaker one would decide whether a secret
# reached this public repository.
from secret_patterns import COMPILED, redact  # noqa: E402

# Any staged path that would publish original retail assets.
FORBIDDEN_PREFIXES = ('game/', 'game\\')

# The scan is the point; a size guard keeps a huge blob from making the hook slow
# enough to be disabled.  Anything over this is refused outright, which is also
# the repository's own blob limit.
MAX_BLOB_BYTES = 100 * 1024 * 1024


def staged_entries() -> list[tuple[str, str]]:
    """(sha, path) for every staged entry, including additions.

    Parsed NUL-separated.  The tab-separated `--name-status` form carries a
    different field count for an addition (`A<TAB>path`) than for a rename
    (`R100<TAB>old<TAB>new`), and a control caught this gate reporting
    "0 path(s)" for a freshly added file because of it -- a gate that silently
    scans nothing passes every control that only checks its exit code.
    """
    completed = subprocess.run(
        ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'],
        capture_output=True, text=True, cwd=str(ROOT))
    if completed.returncode != 0:
        print(f'could not read the index: {completed.stderr.strip()}', file=sys.stderr)
        raise SystemExit(2)
    paths = [p for p in completed.stdout.split('\0') if p.strip()]
    entries: list[tuple[str, str]] = []
    for path in paths:
        blob = subprocess.run(['git', 'rev-parse', f':{path}'],
                              capture_output=True, text=True, cwd=str(ROOT))
        if blob.returncode != 0:
            continue
        entries.append((blob.stdout.strip(), path))
    return entries


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', help='repository root override (fixtures)')
    args = parser.parse_args()

    global ROOT
    if args.root:
        ROOT = Path(args.root).resolve()

    failures: list[str] = []
    entries = staged_entries()

    for _sha, path in entries:
        normalized = path.replace('\\', '/')
        if normalized.startswith(FORBIDDEN_PREFIXES) or normalized.startswith('game/'):
            failures.append(
                f'REFUSED: staged path {path!r} is under game/, which holds '
                f'nondistributable retail assets. This repository is public. '
                f'Unstage it with: git restore --staged "{path}"')

    scanned = 0
    for _sha, path in entries:
        if path.replace('\\', '/').startswith('game/'):
            continue  # already refused above; do not read the asset
        size = subprocess.run(['git', 'cat-file', '-s', _sha],
                              capture_output=True, text=True, cwd=str(ROOT))
        if size.returncode != 0:
            continue
        if int(size.stdout.strip() or 0) > MAX_BLOB_BYTES:
            failures.append(
                f'REFUSED: staged blob for {path!r} is '
                f'{int(size.stdout.strip()) / 1024 ** 2:.1f} MB, over the 100 MB '
                f'repository limit')
            continue
        data = subprocess.run(['git', 'cat-file', 'blob', _sha],
                              capture_output=True, cwd=str(ROOT)).stdout
        scanned += 1
        text = data.decode('utf-8', errors='replace')
        for name, pattern in COMPILED:
            match = pattern.search(text)
            if match:
                excerpt = redact(text[max(0, match.start() - 20):match.end() + 20])
                failures.append(
                    f'REFUSED: {path!r} matches the {name} secret pattern: '
                    f'{excerpt[:120]!r}')

    if failures:
        print()
        for failure in failures:
            print(f'  {failure}')
        print()
        print(f'  {len(failures)} staged-path finding(s); commit refused.')
        return 1

    print(f'  staged-path gate: {len(entries)} path(s), {scanned} blob(s) scanned; '
          f'no game/ path, no secret, no oversized blob')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
