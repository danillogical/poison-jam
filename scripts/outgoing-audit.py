"""Audit the commits about to be pushed, in the game repository, the toolkit, or both.

For each repository this:

* fetches `origin`;
* requires `origin/<branch>` to be an ancestor of HEAD (the push is a fast-forward);
* lists the outgoing blobs (`origin/<branch>..HEAD`), the largest, and any over 100 MB;
* runs `scripts/secret-audit.py` over those blobs;
* requires a clean working tree.

The game repository additionally refuses any outgoing path AGENTS.md says is never tracked or
pushed: `game/`, the retail-derived `src/recomp/gen/` and `src/recomp/recovered/` (the AOT shader
corpus lives under the former), local analysis output, saves and logs, and Xbox BIOS/MCPX/HDD
assets or disk images. The toolkit additionally requires its `upstream` and `nworkers` remotes
(when present) to have the push URL `DISABLED`.

Exits non-zero on any failure; every failure is printed and listed at the end.

Usage:  python -X utf8 scripts/outgoing-audit.py [--repo game|toolkit|both] [--base REF]
(`--base` replaces `origin/<branch>` for ONE repository, e.g. to audit a throwaway branch.)
Run it before every push of either repository.
"""
import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolkit_path import ToolkitPathError, toolkit_path  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
MAX_BLOB = 100 * 1024 * 1024
FORBIDDEN_PREFIXES = (
    'game/', 'src/recomp/gen/', 'src/recomp/recovered/', 'tools/disasm/output/',
    'tools/func_id/output/', 'saves/', 'logs/', 'bios/', 'mcpx/', 'hdd/',
)
FORBIDDEN_SUFFIXES = ('.xbe', '.img', '.dmp', '.rom')
GUARDED_REMOTES = ('upstream', 'nworkers')


def git(repo, *args, stdin=None, check=True):
    result = subprocess.run(['git', '-C', str(repo), *args], input=stdin, capture_output=True,
                            text=True, encoding='utf-8', errors='replace')
    if check and result.returncode:
        raise RuntimeError(f'git {" ".join(args)} failed in {repo}: {result.stderr.strip()}')
    return result


def forbidden(path):
    """Why a path may never be pushed from the game repository, or None."""
    norm = path.replace('\\', '/').lstrip('./')
    for prefix in FORBIDDEN_PREFIXES:
        if norm.startswith(prefix):
            return f'under {prefix}'
    if norm.lower().endswith(FORBIDDEN_SUFFIXES):
        return 'a retail/disk-image/dump file type'
    return None


def audit_repo(label, repo, base_override, is_game):
    failures = []

    def fail(message):
        failures.append(message)
        print(f'[{label}] FAIL: {message}')

    print(f'[{label}] {repo}')
    branch = git(repo, 'rev-parse', '--abbrev-ref', 'HEAD').stdout.strip()
    fetched = git(repo, 'fetch', 'origin', check=False)
    if fetched.returncode:
        fail(f'git fetch origin failed: {fetched.stderr.strip()}')
        return failures
    base = base_override or f'origin/{branch}'
    if git(repo, 'rev-parse', '--verify', '--quiet', base, check=False).returncode:
        fail(f'{base} does not exist (branch {branch}); pass --base for a branch origin lacks')
        return failures
    if git(repo, 'merge-base', '--is-ancestor', base, 'HEAD', check=False).returncode:
        fail(f'{branch} is not a fast-forward of {base}: pushing would rewrite history')
    status = git(repo, 'status', '--porcelain').stdout.strip()
    if status:
        fail(f'working tree is not clean:\n{status}')

    objects = git(repo, 'rev-list', '--objects', f'{base}..HEAD', check=False).stdout.splitlines()
    shas = '\n'.join(line.split(' ')[0] for line in objects if line.strip())
    info = git(repo, 'cat-file', '--batch-check', stdin=shas + '\n').stdout.splitlines()
    blobs = [line.split() for line in info if ' blob ' in line]
    largest = max((int(b[2]) for b in blobs), default=0)
    print(f'[{label}] {branch}: {len(git(repo, "rev-list", f"{base}..HEAD", check=False).stdout.split())} '
          f'outgoing commit(s), {len(blobs)} blob(s), largest {largest} bytes')
    oversized = [b[0] for b in blobs if int(b[2]) > MAX_BLOB]
    if oversized:
        fail(f'{len(oversized)} blob(s) exceed 100 MB (GitHub refuses them): ' + ' '.join(oversized))

    if blobs:
        with tempfile.TemporaryDirectory() as tmp:
            listing = Path(tmp) / 'blobs.txt'
            listing.write_text('\n'.join(b[0] for b in blobs) + '\n')
            result = subprocess.run(
                [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'secret-audit.py'),
                 str(listing)], capture_output=True, text=True, encoding='utf-8',
                errors='replace', cwd=repo)
        print('\n'.join(f'[{label}] {line}' for line in result.stdout.splitlines()
                        if 'TOTAL' in line or 'scanned' in line))
        if result.returncode:
            fail('scripts/secret-audit.py reported findings:\n' + result.stdout[-800:])

    if is_game:
        names = git(repo, 'log', '--name-only', '--diff-filter=ACMR', '--format=',
                    f'{base}..HEAD', check=False).stdout.splitlines()
        for path in sorted({n for n in names if n.strip()}):
            why = forbidden(path)
            if why:
                fail(f'outgoing path {path} is {why}; AGENTS.md says never track or push it')
    else:
        remotes = git(repo, 'remote', check=False).stdout.split()
        for name in GUARDED_REMOTES:
            if name not in remotes:
                continue
            push = git(repo, 'remote', 'get-url', '--push', name).stdout.strip()
            print(f'[{label}] remote {name} push URL: {push}')
            if push != 'DISABLED':
                fail(f'remote {name} has push URL {push!r}, expected DISABLED')
    if not failures:
        print(f'[{label}] OK')
    return failures


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--repo', choices=('game', 'toolkit', 'both'), default='game')
    parser.add_argument('--base', help='audit REF..HEAD instead of origin/<branch>..HEAD')
    args = parser.parse_args(argv)
    if args.base and args.repo == 'both':
        parser.error('--base names one repository; pass --repo game or --repo toolkit')
    plan = []
    if args.repo in ('game', 'both'):
        plan.append(('game', ROOT, True))
    if args.repo in ('toolkit', 'both'):
        try:
            plan.append(('toolkit', toolkit_path(), False))
        except ToolkitPathError as error:
            sys.exit(f'outgoing-audit: {error}')
    failures = []
    for label, repo, is_game in plan:
        try:
            failures += audit_repo(label, repo, args.base, is_game)
        except RuntimeError as error:
            failures.append(str(error))
            print(f'[{label}] FAIL: {error}')
    if failures:
        print(f'\noutgoing-audit: {len(failures)} failure(s)')
        return 1
    print('\noutgoing-audit: all clean')
    return 0


if __name__ == '__main__':
    sys.exit(main())
