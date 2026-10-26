"""Toolkit tests on Windows: its own CTest plus the standalone `tests/<name>` CMake projects.

    python -X utf8 scripts/tk-tests.py [--toolkit DIR] [--skip-main] [--only-main] [NAME ...]

Steps:
1. the toolkit's CTest (configure with `-DBUILD_TESTING=ON`, build Release, `ctest`);
2. each standalone project -- a `tests/<name>/` with its own `project()` -- configured into
   `<toolkit>/build/sub-<name>`, built and run with `ctest`.

`tests/FORK-STATUS.md` in the toolkit records the standalone projects that do not pass in this
fork, in sections: (b) upstream tests of a design the fork does not carry, (c) POSIX-only
projects Windows refuses by design, (d) projects failing on `main` before the merge. A failing
project listed there is reported EXPECTED, not as a failure; a listed project that passes is
reported UNEXPECTED PASS (the record is stale; not a failure); any other failure is a FAILURE.
Exit 1 on any FAILURE (or a failing main CTest), else 0. Names on the command line limit the
standalone step. The toolkit defaults to the one `scripts/toolkit_path.py` resolves.
"""
import argparse
import os
import re
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolkit_path import ToolkitPathError, toolkit_path  # noqa: E402

STEP_TIMEOUT = 3600
# FORK-STATUS section heading -> the class letter the record uses for it.
SECTION_CLASSES = (('upstream', 'b'), ('windows refuses', 'c'), ('failing on', 'd'))
TAIL_LINES = 12


def parse_fork_status(path, projects):
    """`{project: class letter}` for every standalone project FORK-STATUS.md lists.

    A table row names its project in the first column; in prose, a sentence that opens with
    a backticked list followed by a colon names its projects. Only names that are real `tests/<name>/` projects count, so a
    header or an incidental mention (`apu_vp`, which passes) is never an expectation.
    """
    if not path.is_file():
        return {}
    expected = {}
    letter = None
    for paragraph in re.split(r'\n\s*\n', path.read_text(encoding='utf-8')):
        for line in paragraph.splitlines():
            if line.startswith('## '):
                heading = line[3:].lower()
                letter = next((c for key, c in SECTION_CLASSES if key in heading), None)
        if letter is None:
            continue
        rows = [l for l in paragraph.splitlines() if l.startswith('|')]
        if rows:
            names = [re.match(r'\|\s*`([^`]+)`', r) for r in rows]
            names = [m.group(1) for m in names if m]
        else:
            names = []
            for lead in re.finditer(r'(?:^|(?<=\.\n))((?:`[^`]+`(?:,\s*)?)+):', paragraph):
                names += re.findall(r'`([^`]+)`', lead.group(1))
        for name in names:
            if name in projects:
                expected[name] = letter
    return expected


def standalone_projects(tests_dir):
    found = []
    for entry in sorted(tests_dir.iterdir()):
        lists = entry / 'CMakeLists.txt'
        if lists.is_file() and re.search(r'^\s*project\s*\(', lists.read_text(
                encoding='utf-8', errors='replace'), re.IGNORECASE | re.MULTILINE):
            found.append(entry.name)
    return found


def run(cmd, capture):
    """Run one step; returns `(exit code, last lines of its output)`."""
    started = time.time()
    try:
        if capture:
            r = subprocess.run(cmd, capture_output=True, text=True, errors='replace',
                               timeout=STEP_TIMEOUT)
            tail = '\n'.join((r.stdout + r.stderr).strip().splitlines()[-TAIL_LINES:])
            return r.returncode, tail
        return subprocess.run(cmd, timeout=STEP_TIMEOUT).returncode, ''
    except subprocess.TimeoutExpired:
        return 124, f'timed out after {STEP_TIMEOUT}s'
    finally:
        print(f'   ({time.time() - started:.0f}s)', flush=True)


def run_main(toolkit):
    build = str(toolkit / 'build')
    steps = [['cmake', '-S', str(toolkit), '-B', build, '-DBUILD_TESTING=ON'],
             ['cmake', '--build', build, '--config', 'Release', '--parallel', '4'],
             ['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure']]
    for cmd in steps:
        print('>>', ' '.join(cmd), flush=True)
        code, _ = run(cmd, capture=False)
        print('<< exit', code, flush=True)
        if code:
            return code
    return 0


def run_standalone(toolkit, name):
    src, out = toolkit / 'tests' / name, toolkit / 'build' / f'sub-{name}'
    steps = [['cmake', '-S', str(src), '-B', str(out)] + (['-A', 'x64'] if os.name == 'nt' else []),
             ['cmake', '--build', str(out), '--config', 'Release'],
             ['ctest', '--test-dir', str(out), '-C', 'Release', '--output-on-failure']]
    for cmd in steps:
        print('>>', name, ' '.join(cmd[:2]), flush=True)
        code, tail = run(cmd, capture=True)
        if code:
            return code, f'{cmd[0]} {cmd[1]} exited {code}\n{tail}'
    return 0, ''


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--toolkit', type=Path, help='toolkit checkout (default: the resolver)')
    parser.add_argument('--skip-main', action='store_true', help='skip the toolkit CTest')
    parser.add_argument('--only-main', action='store_true', help='skip the standalone projects')
    parser.add_argument('names', nargs='*', help='standalone projects to run (default: all)')
    args = parser.parse_args(argv)
    try:
        toolkit = (args.toolkit or toolkit_path()).resolve()
    except ToolkitPathError as error:
        sys.exit(f'tk-tests: {error}')
    for key in ('http_proxy', 'https_proxy'):
        os.environ.pop(key, None)   # os.environ also collapses the Path/PATH duplicates MSBuild rejects
    tests_dir = toolkit / 'tests'
    projects = standalone_projects(tests_dir)
    expected = parse_fork_status(tests_dir / 'FORK-STATUS.md', projects)
    print(f'tk-tests: {toolkit}; {len(projects)} standalone project(s), '
          f'{len(expected)} listed in FORK-STATUS.md')
    unknown = [n for n in args.names if n not in projects]
    if unknown:
        sys.exit(f'tk-tests: not a standalone project: {", ".join(unknown)}')

    failures = []
    if not args.skip_main:
        code = run_main(toolkit)
        if code:
            failures.append(f'toolkit CTest exited {code}')
    results = []
    if not args.only_main:
        for name in args.names or projects:
            code, detail = run_standalone(toolkit, name)
            klass = expected.get(name)
            if code == 0 and klass:
                verdict = f'UNEXPECTED PASS (listed ({klass}) in FORK-STATUS.md; the record is stale)'
            elif code == 0:
                verdict = 'pass'
            elif klass:
                verdict = f'EXPECTED FAIL ({klass})'
            else:
                verdict = 'FAILURE'
                failures.append(f'{name}: {detail.splitlines()[0]}')
            print(f'<< {name}: {verdict}', flush=True)
            if verdict == 'FAILURE':
                print(detail, flush=True)
            results.append((name, verdict))
        print('\nstandalone summary:')
        for name, verdict in results:
            print(f'  {name:28s} {verdict}')
    print()
    if failures:
        print('tk-tests: FAILED\n  ' + '\n  '.join(failures))
        return 1
    print('tk-tests: ok (expected failures are not failures)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
