"""Bounded debugger launch and full artifact archive.

Same contract as the historical run-jsrf.ps1: verify identity, refuse a
concurrent project game, archive sources/symbols, run jsrf_collect.exe, then
classify checkpoints and GPU analysis. Stdlib only; works on Windows
PowerShell 5.1 hosts because this file never uses pwsh-only JSON APIs.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build' / 'Release'
TOOLKIT = ROOT.parent / 'xboxrecomp'
PROBES = {
    '', 'healthy', 'worker-crash', 'deadlock', 'spin', 'handled', 'dispatch-race',
    'video', 'gpu-progress', 'gpu-stall', 'gpu-corrupt', 'gpu-unreadable',
    'gpu-mmio-owner', 'gpu-mmio-lifecycle', 'gpu-ptimer-runtime',
    'gpu-submit-supported', 'gpu-submit-bound', 'gpu-submit-blocked',
}
ENV_KEYS = ('RECOMP_WATCHDOG_SECS', 'JSRF_LOG_PATH', 'JSRF_COLLECTED', 'RECOMP_GPU_ACK')
ARTIFACTS = (
    'jsrf_recomp.exe', 'jsrf_recomp.pdb', 'jsrf_recomp.map',
    'jsrf_collect.exe', 'jsrf_collect.pdb', 'build-source.json',
)
CREATE_NO_WINDOW = 0x08000000
PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
TH32CS_SNAPPROCESS = 0x00000002

class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = (
        ('dwSize', wintypes.DWORD),
        ('cntUsage', wintypes.DWORD),
        ('th32ProcessID', wintypes.DWORD),
        ('th32DefaultHeapID', ctypes.POINTER(ctypes.c_ulong)),
        ('th32ModuleID', wintypes.DWORD),
        ('cntThreads', wintypes.DWORD),
        ('th32ParentProcessID', wintypes.DWORD),
        ('pcPriClassBase', ctypes.c_long),
        ('dwFlags', wintypes.DWORD),
        ('szExeFile', wintypes.WCHAR * 260),
    )


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run_git(repo: Path, *args: str, binary: bool = False) -> bytes | str:
    completed = subprocess.run(['git', '-C', str(repo), *args], capture_output=True)
    # git diff returns 1 when the tree is dirty; that is still the patch.
    if completed.returncode not in (0, 1):
        detail = completed.stderr.decode('utf-8', 'replace').strip() or f'exit {completed.returncode}'
        raise RuntimeError(f'git {" ".join(args)} failed: {detail}')
    return completed.stdout if binary else completed.stdout.decode('utf-8', 'replace')


def archive_project_state(run_dir: Path) -> bool:
    """Archive the game repository's patch and status next to this run.

    Returns True when the repository answered.  The game repository's .git has
    no object store (no objects/, no loose refs, no packed-refs, no remote), so
    every git command against it exits 128 and run_git raises.  That used to
    abort the run before the guest ever started.

    Recording the exact source state beside a run is what makes the run
    evidence, so this is not dropped.  Instead the failure is recorded as a
    failure: a marker file naming the git error, plus the executable hash from
    the build-identity stamp, which is the strongest provenance available when
    there is no revision to name.  A reader can then tell "the repo could not
    answer on this date" apart from "someone forgot to archive it", and a
    healthy repository still writes both files exactly as before.
    """
    try:
        (run_dir / 'project.patch').write_bytes(
            run_git(ROOT, 'diff', '--binary', 'HEAD', binary=True))
        (run_dir / 'project-status.txt').write_text(
            run_git(ROOT, 'status', '--porcelain'), encoding='utf-8')
        return True
    except RuntimeError as error:
        exe_hash = ''
        stamp = BUILD / 'build-source.json'
        if stamp.is_file():
            try:
                exe_hash = json.loads(stamp.read_text())['exe_sha256']
            except (ValueError, KeyError):
                exe_hash = ''
        (run_dir / 'project-state-unavailable.txt').write_text(
            'The game repository could not be archived for this run.\n\n'
            f'git error: {error}\n\n'
            'Cause: .git has no object store, so no commit, diff or status can\n'
            'be produced.  The working tree is intact.  See AGENTS.md.\n\n'
            'Substitute provenance -- build/Release/build-source.json:\n'
            f'  exe_sha256: {exe_hash or "(unavailable)"}\n',
            encoding='utf-8')
        print(f'WARNING: project patch/status unavailable ({error}); '
              f'wrote project-state-unavailable.txt', file=sys.stderr)
        return False


def project_game_running(root: Path) -> bool:
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.Process32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.QueryFullProcessImageNameW.argtypes = [
        wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == wintypes.HANDLE(-1).value:
        raise RuntimeError('could not snapshot processes')
    prefix = str(root) + '\\'
    entry = PROCESSENTRY32W()
    entry.dwSize = ctypes.sizeof(PROCESSENTRY32W)
    try:
        more = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while more:
            if entry.szExeFile.lower() == 'jsrf_recomp.exe':
                handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, entry.th32ProcessID)
                if handle:
                    size = wintypes.DWORD(32768)
                    buf = ctypes.create_unicode_buffer(size.value)
                    ok = kernel32.QueryFullProcessImageNameW(handle, 0, buf, ctypes.byref(size))
                    kernel32.CloseHandle(handle)
                    if ok and buf.value.lower().startswith(prefix.lower()):
                        return True
            more = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
        return False
    finally:
        kernel32.CloseHandle(snapshot)


def source_entries() -> list[dict[str, str]]:
    files: list[Path] = []
    for folder in ('src', 'scripts', 'config', 'tests', 'tools/harness'):
        files.extend(p for p in (ROOT / folder).rglob('*') if p.is_file())
    files.append(ROOT / 'CMakeLists.txt')
    entries = []
    for path in files:
        relative = str(path.relative_to(ROOT)).replace('/', '\\')
        entries.append({'path': relative, 'sha256': digest(path)})
    entries.sort(key=lambda item: item['path'].lower())
    return entries


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=int, default=15)
    parser.add_argument('--label', default='run')
    parser.add_argument('--probe', default='')
    parser.add_argument('--expect-checkpoint', action='append', dest='expect_checkpoint')
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('--seconds must be between 1 and 300')
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.label):
        parser.error('--label must match [a-zA-Z0-9_-]+')
    if args.probe not in PROBES:
        parser.error(f'unknown --probe {args.probe!r}')
    if not args.expect_checkpoint:
        args.expect_checkpoint = ['memory_ready', 'guest_entry']
    return args


def classify_exit(result: dict) -> int:
    if result.get('outcome') == 'collector_failure':
        return 2
    if 'gpu_report_ok' in result and not result['gpu_report_ok']:
        return 2
    if not result.get('checkpoints_passed'):
        return 4
    if result.get('outcome') != 'normal_exit':
        return 3
    if result.get('exit_code') != 0:
        return 1
    return 0


def main() -> int:
    args = parse_args()
    previous = {key: os.environ.get(key) for key in ENV_KEYS}
    collector = None
    started = datetime.now()
    stamp = started.strftime('%Y%m%d-%H%M%S-') + f'{started.microsecond // 1000:03d}'
    run_dir = ROOT / 'logs' / 'runs' / f'{stamp}-{args.label}'
    try:
        verify = subprocess.run([sys.executable, str(ROOT / 'scripts' / 'build-identity.py'), 'verify'])
        if verify.returncode != 0:
            print('Refusing to run a stale or unverified build.', file=sys.stderr)
            return 2
        if project_game_running(ROOT):
            print('A project game instance is already running; inspect it before another run.', file=sys.stderr)
            return 2
        run_dir.mkdir(parents=True, exist_ok=True)
        for name in ARTIFACTS:
            shutil.copy2(BUILD / name, run_dir / name)
        (run_dir / 'toolkit.patch').write_bytes(run_git(TOOLKIT, 'diff', '--binary', 'HEAD', binary=True))
        (run_dir / 'toolkit-status.txt').write_text(run_git(TOOLKIT, 'status', '--porcelain'), encoding='utf-8')
        project_archived = archive_project_state(run_dir)
        os.environ['JSRF_COLLECTED'] = '1'
        if args.probe.startswith('gpu-'):
            os.environ['RECOMP_GPU_ACK'] = '0'
        os.environ['JSRF_LOG_PATH'] = str(run_dir / 'jsrf_run.log')
        os.environ.pop('RECOMP_WATCHDOG_SECS', None)
        metadata = {
            'started_utc': datetime.now(timezone.utc).isoformat(),
            'cwd': str(ROOT),
            'seconds': args.seconds,
            'probe': args.probe,
            'expected_checkpoints': args.expect_checkpoint,
            'toolkit_revision': run_git(TOOLKIT, 'rev-parse', 'HEAD').strip(),
            'project_archived': project_archived,
            'configuration': 'Release',
            'xbe_sha256': digest(ROOT / 'game' / 'default.xbe'),
            'exe_sha256': digest(run_dir / 'jsrf_recomp.exe'),
            'pdb_sha256': digest(run_dir / 'jsrf_recomp.pdb'),
            'settings': [{'name': name, 'value': value}
                         for name, value in sorted(os.environ.items())
                         if name.startswith(('RECOMP_', 'JSRF_'))],
            'sources': source_entries(),
        }
        (run_dir / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        archive = subprocess.run([sys.executable, str(ROOT / 'scripts' / 'archive-run-source.py'), str(run_dir)])
        if archive.returncode != 0:
            print('Source archiving failed.', file=sys.stderr)
            return 2
        command = [str(run_dir / 'jsrf_collect.exe'), str(args.seconds), str(run_dir),
                   str(run_dir / 'jsrf_recomp.exe')]
        if args.probe:
            command.append(f'--probe={args.probe}')
        collector = subprocess.Popen(
            command, cwd=str(ROOT),
            creationflags=CREATE_NO_WINDOW if os.name == 'nt' else 0)
        try:
            collector.wait(timeout=args.seconds + 45)
            timed_out = False
        except subprocess.TimeoutExpired:
            collector.kill()
            collector.wait(timeout=10)
            timed_out = True
        if timed_out:
            result = {'outcome': 'collector_failure', 'reason': 'collector host deadline exceeded'}
        elif (run_dir / 'result.json').is_file():
            result = json.loads((run_dir / 'result.json').read_text(encoding='utf-8-sig'))
        else:
            result = {'outcome': 'collector_failure', 'reason': 'missing collector result'}
        log_path = run_dir / 'jsrf_run.log'
        log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.is_file() else ''
        if log_path.is_file():
            shutil.copy2(log_path, ROOT / 'jsrf_run.log')
        missing = [name for name in args.expect_checkpoint
                   if not re.search(r'(?m)^\[CHECKPOINT\][^\r\n]* ' + re.escape(name) + r'\r?$', log)]
        result['missing_checkpoints'] = missing
        result['checkpoints_passed'] = not missing
        result['duration_seconds'] = (datetime.now() - started).total_seconds()
        if (run_dir / 'gpu-snapshots.jsonl').is_file():
            analysis = run_dir / 'gpu-analysis.log'
            with analysis.open('w', encoding='utf-8') as log_file:
                gpu = subprocess.run(
                    [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'jsrf_gpu.py'),
                     str(run_dir), '--write'],
                    stdout=log_file, stderr=subprocess.STDOUT)
            result['gpu_report_ok'] = gpu.returncode == 0
        (run_dir / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(f'Artifacts: {run_dir}')
        print(json.dumps(result, separators=(',', ':')))
        return classify_exit(result)
    finally:
        if collector is not None and collector.poll() is None:
            collector.kill()
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as error:
        print(f'Runner failed: {error}', file=sys.stderr)
        sys.exit(2)
