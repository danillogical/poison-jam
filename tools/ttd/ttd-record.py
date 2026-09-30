"""`just ttd-record <label>`: record one strict run under WinDbg TTD.

**Why this exists.** C1 needs to know which code writes the value the terminal
read sees at `[0x1C4064]`. The A2h line spent ~70,000 words making watch
instruments trustworthy against ~13,000 for the answer, and the reason is
structural: a native DR0 watch matches a *linear address*, so it watches the
canonical VA and is blind to the 28 mirror views that alias the same pages
(`tools/ttd/aliases.py`). A TTD trace records every access with its address, so
the same question becomes a filter over recorded events and the alias set stops
being a blind spot.

**Why it does not just run `ttd.exe jsrf_recomp.exe`.** That was tried and the
target exited with code 2. It does **not** establish a TTD failure, and it is not
evidence about anything: the executable requires `--save-root=<absolute writable
directory>` and the real runner supplies environment, profile and archive setup
that the guest depends on. This script therefore reproduces the runner's launch
contract rather than approximating it:

  * working directory is the game root (the executable resolves `game/default.xbe`
    relative to it);
  * `--save-root` points at a **fresh, empty, disposable** directory under the
    trace's own output directory, so no existing save is read or written;
  * the strict environment is assembled by the same module the runner uses
    (`scripts/jsrf_run_profile.py`), and `validate_launch_profile` refuses a strict
    launch without `RECOMP_GPU_ACK=0` -- this script does not insert it silently;
  * the run is bounded, and the trace is stopped and closed before the tool
    returns.

**What is deliberately NOT done.** This does not launch `jsrf_collect.exe`. The
collector exists to freeze threads and dump state at a deadline; under TTD the
trace itself is the record, and running both would put a debugger and a recorder
on the same process for no gain. A TTD trace is therefore **not** an archived run
and cannot satisfy a strict acceptance criterion by itself -- `docs/jsrf-run-profiles.md`
owns that distinction, and W11 is the ruling that decides whether TTD query output
is admissible as a decision input at all.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))

from jsrf_run_profile import (  # noqa: E402
    ProfileError,
    environment_settings,
    make_profile_record,
    validate_launch_profile,
)

TRACE_ROOT = ROOT / 'logs' / 'ttd'
EXE = ROOT / 'build' / 'Release' / 'jsrf_recomp.exe'
GAME_ROOT = ROOT
CREATE_NO_WINDOW = 0x08000000


def resolve_ttd() -> str | None:
    """ttd.exe, preferring PATH.

    The App Execution Alias on this host resolves to the TTD package's own
    executable, so `shutil.which` is enough; the explicit package path is a
    fallback for a host where the alias is absent.
    """
    found = shutil.which('ttd') or shutil.which('ttd.exe')
    if found:
        return found
    fallback = Path(
        r'C:\Program Files\WindowsApps'
        r'\Microsoft.TimeTravelDebugging_1.11.611.0_x64__8wekyb3d8bbwe\TTD.exe')
    return str(fallback) if fallback.is_file() else None


def is_elevated() -> bool:
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except Exception:
        return False


def build_environment(label: str, save_root: Path, trace_dir: Path) -> dict:
    """The launch environment handed to the recorded process.

    **`RECOMP_GPU_ACK=0` is NOT inserted here.**  The runner's contract, enforced
    by `jsrf_run_profile.validate_launch_profile`, is that the *caller* supplies
    it and the tool never invents it -- because a tool that inserted it would turn
    "this run is strict" from a decision into an accident.  The `just ttd-record`
    recipe sets it, exactly as `just strict-run` does.

    Everything else here is what the collector would otherwise have supplied: a
    log path inside the trace directory, and no watchdog (the recorder bounds the
    run).
    """
    env = dict(os.environ)
    env['JSRF_LOG_PATH'] = str(trace_dir / 'jsrf_run.log')
    env.pop('RECOMP_WATCHDOG_SECS', None)
    env['JSRF_TTD_LABEL'] = label
    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', default='ttd', help='trace label')
    parser.add_argument('--seconds', type=int, default=20,
                        help='wall-clock bound for the recorded run')
    parser.add_argument('--max-file-mb', type=int, default=8192,
                        help='TTD trace size cap in MB')
    parser.add_argument('--replay-cpu', default='MostConservative',
                        choices=('Default', 'MostConservative', 'MostAggressive',
                                 'IntelAvxRequired', 'IntelAvx2Required'),
                        help='replay CPU support to assume (affects trace size)')
    parser.add_argument('--dry-run', action='store_true',
                        help='print the launch contract and exit without recording')
    args = parser.parse_args()

    if not EXE.is_file():
        print(f'no executable at {EXE}; run `just build` first', file=sys.stderr)
        return 2

    ttd = resolve_ttd()
    if not ttd:
        print('ttd.exe not found on PATH or in the TTD package', file=sys.stderr)
        return 2

    if not is_elevated():
        # Recording needs the TTD kernel driver.  Failing here is much cheaper
        # than a trace that silently records nothing.
        print('TTD recording requires an elevated process; this one is not.',
              file=sys.stderr)
        return 2

    started = datetime.now()
    stamp = started.strftime('%Y%m%d-%H%M%S-') + f'{started.microsecond // 1000:03d}'
    trace_dir = TRACE_ROOT / f'{stamp}-{args.label}'
    save_root = trace_dir / 'save-root'

    if args.dry_run:
        print(f'executable : {EXE}')
        print(f'cwd        : {GAME_ROOT}')
        print(f'trace dir  : {trace_dir}')
        print(f'save root  : {save_root}')
        print(f'ttd        : {ttd}')
        print(f'bound      : {args.seconds}s, trace cap {args.max_file_mb} MB')
        print('env        : RECOMP_GPU_ACK=0 (strict), JSRF_LOG_PATH=<trace dir>')
        print(f'argv       : {EXE} --save-root={save_root}')
        return 0

    trace_dir.mkdir(parents=True, exist_ok=False)
    save_root.mkdir(exist_ok=False)

    env = build_environment(args.label, save_root, trace_dir)
    inherited = environment_settings(dict(os.environ))
    effective = environment_settings(env)
    try:
        # The profile contract is on the CALLER, so this validates the environment
        # this process inherited -- the one the recipe or the user set up.  If
        # RECOMP_GPU_ACK=0 is missing, this refuses rather than inserting it.
        validate_launch_profile('strict', '', inherited)
    except ProfileError as error:
        shutil.rmtree(trace_dir, ignore_errors=True)
        print(f'refusing strict recording: {error}', file=sys.stderr)
        print('  run it as: just ttd-record <label>   (sets RECOMP_GPU_ACK=0)',
              file=sys.stderr)
        return 2

    profile_record = make_profile_record('strict', inherited, effective, '')
    resolved_save_root = str(save_root.resolve(strict=True))

    command = [
        ttd, '-acceptEula', '-out', str(trace_dir),
        '-maxFile', str(args.max_file_mb),
        '-replayCpuSupport', args.replay_cpu,
        '-launch', str(EXE), f'--save-root={resolved_save_root}',
    ]

    contract = {
        'recorded_utc': datetime.now(timezone.utc).isoformat(),
        'label': args.label,
        'ttd': ttd,
        'executable': str(EXE),
        'executable_sha256': _sha256(EXE),
        'cwd': str(GAME_ROOT),
        'save_root': resolved_save_root,
        'disposable_save_root': True,
        'seconds_bound': args.seconds,
        'max_file_mb': args.max_file_mb,
        'replay_cpu_support': args.replay_cpu,
        'command': command,
        'run_profile': profile_record,
        'strict_environment': {
            key: env.get(key) for key in
            ('RECOMP_GPU_ACK', 'JSRF_LOG_PATH', 'JSRF_TTD_LABEL')
        },
        'note': ('A TTD trace is not an archived run: no collector, no frozen '
                 'capture. Profile classification and evidence admissibility are '
                 'owned by docs/jsrf-run-profiles.md; see W11.'),
    }
    (trace_dir / 'record-contract.json').write_text(
        json.dumps(contract, indent=2), encoding='utf-8')

    print(f'recording {args.label} (bound {args.seconds}s) -> {trace_dir}')
    print(f'  {EXE.name} --save-root={resolved_save_root}')
    process = subprocess.Popen(
        command, cwd=str(GAME_ROOT), env=env,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        creationflags=CREATE_NO_WINDOW if os.name == 'nt' else 0)
    try:
        # **The timeout must outlast the PROCESS, not the requested bound.** Measured:
        # `--seconds 20` gave `20 + 90 = 110s`, and the recorder was killed at 109.578s
        # with "Recording stopped" while its trace was only 20252 MB against a 20480 MB
        # cap -- so the wrapper, not the cap, ended it, and C-a could never be satisfied
        # through this path. The guest's own horizon is what decides when the run ends,
        # and it is not a function of `--seconds`.
        #
        # The allowance is therefore proportional to the bound with a floor, so a
        # caller asking for a long run gets a proportionally long grace.
        grace = max(300, args.seconds * 10)
        output, _ = process.communicate(timeout=args.seconds + grace)
        timed_out = False
    except subprocess.TimeoutExpired:
        # `-stop` is TTD's own control path: it ends the recording and lets the
        # recorder flush the trace.  Killing the recorder instead can leave a
        # truncated .run that opens but has no events.
        timed_out = True
        subprocess.run([ttd, '-stop', 'jsrf_recomp.exe'], capture_output=True)
        try:
            output, _ = process.communicate(timeout=120)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
    (trace_dir / 'ttd-output.txt').write_bytes(output or b'')

    runs = sorted(trace_dir.glob('*.run'))
    total_mb = sum(p.stat().st_size for p in runs) / 1024 / 1024
    contract['timed_out'] = timed_out
    contract['wrapper_grace_seconds'] = grace if 'grace' in dir() else None
    contract['ttd_exit_code'] = process.returncode

    # **How the recording ended decides whether the trace has a tail at all.**
    # Measured: a trace can be cut off by its own `-maxFile` cap, and then the
    # recorder writes "Recording stopped after Nms" where a completed one writes
    # "Process exited with exit code ...". The process keeps running afterwards, so
    # its log can contain events the TRACE does not -- which is exactly how a
    # log-only terminal came to be read as an in-trace one (Advisor ruling
    # 2026-09-30, condition C-a).
    ttd_text = (trace_dir / 'ttd-output.txt').read_text(encoding='utf-8',
                                                        errors='replace')
    contract['ended_by_process_exit'] = 'Process exited with exit code' in ttd_text
    contract['ended_by_recording_stop'] = 'Recording stopped' in ttd_text
    contract['at_size_cap'] = bool(
        contract.get('max_file_mb')
        and total_mb >= contract['max_file_mb'])
    # A trace with no tail cannot support an absence claim, so the record says so
    # rather than leaving a reader to infer it from a size.
    contract['has_tail'] = bool(
        contract['ended_by_process_exit'] and not contract['at_size_cap'])
    contract['trace_files'] = [
        {'name': p.name, 'bytes': p.stat().st_size,
         'sha256': _sha256(p)} for p in runs
    ]
    contract['trace_total_mb'] = round(total_mb, 2)
    (trace_dir / 'record-contract.json').write_text(
        json.dumps(contract, indent=2), encoding='utf-8')

    if not runs:
        print(f'no .run file was produced; see {trace_dir / "ttd-output.txt"}',
              file=sys.stderr)
        return 2
    print(f'trace: {runs[0]}  ({total_mb:.1f} MB)')
    print(f'contract: {trace_dir / "record-contract.json"}')
    print(f'query it with: just ttd-writes "{runs[0]}" 0x001C4064')
    return 0


def contract_path(trace_dir: Path) -> str:
    return str(trace_dir / 'record-contract.json')


def _sha256(path: Path) -> str:
    import hashlib
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


if __name__ == '__main__':
    sys.exit(main())
