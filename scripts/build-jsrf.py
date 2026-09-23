"""Build the game and every target CTest runs, from a sanitized environment.

`scripts/build-jsrf.ps1` is the documented entry point and does exactly these
steps. It cannot be used from a host whose environment block carries the same
variable under several cases: this machine's block contains `Path`, `PATH` and
`path`, and MSBuild's CL task builds a case-sensitive dictionary out of that
block and dies with

    MSB6001: Invalid command line switch for "CL.exe".
    System.ArgumentException: Item has already been added.
    Key in dictionary: 'Path'  Key being added: 'PATH'

The duplicates arrive with the host process, so no PowerShell session can
remove them -- the block is already malformed by the time PowerShell starts,
and `Get-ChildItem env:` itself fails on it. Python's `os.environ` collapses
the case variants to one key, so launching the same command sequence from here
hands the toolchain a well-formed environment.

Same steps, same order and the same guards as the PowerShell wrapper; the two
must be kept in step.

**`--parallel 1` under the DSH harness.** MSBuild's multi-node workers talk over
named pipes, which that harness's sandbox blocks, so any `--parallel N` with
`N > 1` fails with exit 1 and **no error text at all** -- the log stops at
`Checking File Globs`, and `-- /verbosity:diagnostic` reports
`Done building target "ResolveProjectReferences" ... -- FAILED` with
`0 Error(s)`. `MSBUILDDISABLENODEREUSE=1` does not help. Single-node builds are
slower and correct; the default stays 4 for hosts where it works.
"""
from pathlib import Path
import argparse
import os
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
logs = root / 'logs'
logs.mkdir(exist_ok=True)
env = dict(os.environ)
python = sys.executable

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--parallel', type=int, default=4,
                    help='MSBuild node count; use 1 where the sandbox blocks '
                         'MSBuild named pipes (default: 4)')
args = parser.parse_args()

TARGETS = [
    'jsrf_recomp', 'jsrf_collect', 'jsrf_crt_test', 'jsrf_lifter_test',
    'jsrf_nv2a_test', 'jsrf_recovery_11c1_test', 'jsrf_service_chain_test',
    'jsrf_callback_reentry_test', 'jsrf_nv2a_hal_test', 'jsrf_inplace_event_test',
    'jsrf_gpu_smoke',
]


def run(argv, log_name):
    """Run one step, tee its output to logs/<log_name>, stop on failure."""
    with (logs / log_name).open('w', encoding='utf-8', errors='replace') as log:
        proc = subprocess.run(argv, cwd=root, env=env, stdout=log,
                              stderr=subprocess.STDOUT, text=True)
    if proc.returncode != 0:
        print(f'FAILED ({proc.returncode}): {" ".join(argv)}')
        tail = (logs / log_name).read_text(encoding='utf-8', errors='replace').splitlines()
        print('\n'.join(tail[-25:]))
        return False
    return True


if not run(['cmake', '-S', '.', '-B', 'build'], 'configure-current.log'):
    raise SystemExit('CMake configure failed; see logs/configure-current.log.')

for script, message in (('scripts/recover-functions.py', 'Recovery generation failed; refusing to compile older generated output.'),
                        ('scripts/generate-lifter-tests.py', 'Lifter regression generation failed.')):
    if not run([python, '-X', 'utf8', script], f'{Path(script).stem}-current.log'):
        raise SystemExit(message)

if not run([python, 'scripts/build-identity.py', 'before'], 'build-identity-before.log'):
    raise SystemExit('Source fingerprint failed.')

if not run(['cmake', '--build', 'build', '--config', 'Release', '--target', *TARGETS,
            '--parallel', str(args.parallel)], 'build-current.log'):
    raise SystemExit('Build failed. Game was not launched.')

if not run([python, 'scripts/build-identity.py', 'after'], 'build-identity-after.log'):
    raise SystemExit('Build identity validation failed.')

print('Build succeeded; source/executable identity recorded. See logs/build-current.log.')
