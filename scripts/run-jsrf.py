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
import ntpath
import os
import re
import shutil
import subprocess
import sys
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path

from jsrf_run_profile import (
    ProfileError,
    environment_settings,
    make_profile_record,
    parse_save_root_markers,
    resolve_requested_profile,
    validate_launch_profile,
)

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


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


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


def disk_gate(floor_gb: float | None) -> tuple[bool, dict]:
    """Run the pre-run free-space gate (plan T14) and return (allowed, record).

    The gate must fire before any child starts: a run that dies of a full disk
    writes a truncated archive that reads like a guest hang.  A gate that cannot
    measure refuses, so an unparseable result is never an authorization.
    """
    command = [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'check-disk-gate.py'),
               '--json', '--quiet']
    if floor_gb is not None:
        command.extend(['--floor-gb', str(floor_gb)])
    completed = subprocess.run(command, capture_output=True)
    record: dict = {'exit_code': completed.returncode}
    text = completed.stdout.decode('utf-8', 'replace').strip()
    if text:
        try:
            record.update(json.loads(text))
        except ValueError:
            record['parse_error'] = text[:400]
    if completed.returncode == 0:
        return True, record
    detail = completed.stderr.decode('utf-8', 'replace').strip()
    record['detail'] = detail
    return False, record


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
    parser.add_argument('--profile', choices=('strict', 'exploratory', 'fixture'),
                        help='evidence profile; defaults to strict for a guest run and fixture for --probe')
    parser.add_argument('--expect-checkpoint', action='append', dest='expect_checkpoint')
    parser.add_argument('--disk-floor-gb', type=float, default=None,
                        help='free-space floor in GB; defaults to the gate default (50)')
    parser.add_argument('--skip-disk-gate', action='store_true',
                        help='bypass the pre-run free-space gate; records the bypass in metadata')
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error('--seconds must be between 1 and 300')
    if not re.fullmatch(r'[a-zA-Z0-9_-]+', args.label):
        parser.error('--label must match [a-zA-Z0-9_-]+')
    if args.probe not in PROBES:
        parser.error(f'unknown --probe {args.probe!r}')
    try:
        args.profile, args.profile_source = resolve_requested_profile(args.profile, args.probe)
    except ProfileError as error:
        parser.error(str(error))
    if not args.expect_checkpoint:
        args.expect_checkpoint = ['memory_ready', 'guest_entry']
    return args


def repository_identity(repo: Path, patch_path: Path, status_path: Path) -> dict[str, str]:
    """Bind a run to each repository revision and archived working-tree state."""
    revision = run_git(repo, 'rev-parse', 'HEAD').strip()
    return {
        'revision': revision,
        'patch_sha256': digest(patch_path),
        'status_sha256': digest(status_path),
    }


def save_root_observation(log: str, expected_path: str) -> tuple[str | None, str | None, bool]:
    """Require both option resolution and a path-layer translation witness."""
    resolved_path, path_layer_path = parse_save_root_markers(log)
    if not resolved_path or not path_layer_path:
        return resolved_path, path_layer_path, False
    normalize = ntpath.normcase
    verified = (normalize(resolved_path) == normalize(expected_path)
                and normalize(path_layer_path) == normalize(expected_path)
                and normalize(path_layer_path) == normalize(resolved_path))
    return resolved_path, path_layer_path, verified


def classify_exit(result: dict) -> int:
    if result.get('outcome') == 'collector_failure':
        return 2
    if result.get('save_root_verified') is False:
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
    inherited_settings = environment_settings(dict(os.environ))
    try:
        validate_launch_profile(args.profile, args.probe, inherited_settings)
    except ProfileError as error:
        print(f'Refusing {args.profile} launch: {error}', file=sys.stderr)
        return 2
    previous = {key: os.environ.get(key) for key in ENV_KEYS}
    collector = None
    started = datetime.now()
    stamp = started.strftime('%Y%m%d-%H%M%S-') + f'{started.microsecond // 1000:03d}'
    run_dir = ROOT / 'logs' / 'runs' / f'{stamp}-{args.label}'
    gate_record: dict = {'skipped': True, 'reason': '--skip-disk-gate'}
    if not args.skip_disk_gate:
        allowed, gate_record = disk_gate(args.disk_floor_gb)
        if not allowed:
            print('Refusing to launch: pre-run disk gate did not pass.', file=sys.stderr)
            if gate_record.get('detail'):
                print(gate_record['detail'], file=sys.stderr)
            return 2
        gate_record['skipped'] = False
    try:
        verify = subprocess.run([sys.executable, str(ROOT / 'scripts' / 'build-identity.py'), 'verify'])
        if verify.returncode != 0:
            print('Refusing to run a stale or unverified build.', file=sys.stderr)
            return 2
        if project_game_running(ROOT):
            print('A project game instance is already running; inspect it before another run.', file=sys.stderr)
            return 2
        try:
            run_dir.mkdir(parents=True, exist_ok=False)
        except OSError as error:
            print(f'Could not create fresh run directory: {error}', file=sys.stderr)
            return 2
        save_root = run_dir / 'save-root'
        try:
            save_root.mkdir(exist_ok=False)
        except OSError as error:
            print(f'Could not create disposable save root: {error}', file=sys.stderr)
            return 2
        if next(save_root.iterdir(), None) is not None:
            print('Refusing to launch with a non-empty disposable save root.', file=sys.stderr)
            return 2
        resolved_save_root = str(save_root.resolve(strict=True))
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
        effective_settings = environment_settings(dict(os.environ))
        profile_record = make_profile_record(
            args.profile, inherited_settings, effective_settings, args.probe)
        command = [str(run_dir / 'jsrf_collect.exe'), str(args.seconds), str(run_dir),
                   str(run_dir / 'jsrf_recomp.exe'), f'--save-root={resolved_save_root}']
        if args.probe:
            command.append(f'--probe={args.probe}')
        project_identity = repository_identity(
            ROOT, run_dir / 'project.patch', run_dir / 'project-status.txt') if project_archived else None
        toolkit_identity = repository_identity(
            TOOLKIT, run_dir / 'toolkit.patch', run_dir / 'toolkit-status.txt')
        metadata = {
            'started_utc': datetime.now(timezone.utc).isoformat(),
            'cwd': str(ROOT),
            'seconds': args.seconds,
            'probe': args.probe,
            'profile_source': args.profile_source,
            'expected_checkpoints': args.expect_checkpoint,
            'project_archived': project_archived,
            'configuration': 'Release',
            'xbe_path': str(ROOT / 'game' / 'default.xbe'),
            'xbe_sha256': digest(ROOT / 'game' / 'default.xbe'),
            'exe_sha256': digest(run_dir / 'jsrf_recomp.exe'),
            'pdb_sha256': digest(run_dir / 'jsrf_recomp.pdb'),
            'map_sha256': digest(run_dir / 'jsrf_recomp.map'),
            'collector_sha256': digest(run_dir / 'jsrf_collect.exe'),
            'build_source_sha256': digest(run_dir / 'build-source.json'),
            'artifact_sha256': {name: digest(run_dir / name) for name in ARTIFACTS},
            'settings': effective_settings,
            'run_profile': profile_record,
            'disk_gate': gate_record,
            'command': command,
            'repository_identities': {
                'project': project_identity,
                'toolkit': toolkit_identity,
            },
            'save_root': {
                'archive_root': str(run_dir.resolve()),
                'expected_resolved_path': resolved_save_root,
                'observed_resolved_path': None,
                'observed_path_layer_root': None,
                'disposable': True,
                'verified': False,
            },
            'sources': source_entries(),
        }
        (run_dir / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        archive = subprocess.run([sys.executable, str(ROOT / 'scripts' / 'archive-run-source.py'), str(run_dir)])
        if archive.returncode != 0:
            print('Source archiving failed.', file=sys.stderr)
            return 2
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
        metadata['run_log_sha256'] = digest(log_path) if log_path.is_file() else None
        observed_root, observed_path_layer, save_root_verified = save_root_observation(
            log, resolved_save_root)
        metadata['save_root']['observed_resolved_path'] = observed_root
        metadata['save_root']['observed_path_layer_root'] = observed_path_layer
        metadata['save_root']['verified'] = save_root_verified
        if not save_root_verified:
            result['save_root_verified'] = False
            result['save_root_reason'] = (
                'runtime did not report exactly one matching resolved save root')
        else:
            result['save_root_verified'] = True
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
        (run_dir / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
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
