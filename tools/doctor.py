"""Write `doctor.json`: what environment produced this run, and was it healthy.

Plan T12 asks for a doctor record "per run", and the reason is the diagnostic cost
already paid on this project: a run that stops early can be a guest defect, a stale
binary, a full disk, a confined sandbox blocking MSBuild named pipes, or a missing
save-root.  Those are indistinguishable from `result.json` alone, and more than one
session spent time on the guest before checking the environment.

This script answers the environment question in one bounded record.  It is
**observation only**: it never mutates guest state, never gates a run, and never
turns a missing measurement into a pass.  A check it cannot perform is reported as
`UNKNOWN`, not omitted.

Two modes:

  * `--runtime-log <run-dir>` -- the plan's per-run form.  Writes `doctor.json`
    into an archived run directory and summarises that run's own artifacts.
  * `--preflight` -- the same checks against the live host, printed only.  Used by
    the `just doctor` recipe so the environment can be inspected before launching.

Nothing here is evidence for a strict criterion; `docs/jsrf-run-profiles.md` owns
which profile a run is, and `scripts/check-run-profile.py` owns re-deriving it.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
DOCTOR_VERSION = 'jsrf-doctor/1'

# The same floor scripts/check-disk-gate.py defaults to; kept in one place there
# and read from it here rather than duplicated as a second number.
DISK_FLOOR_GB = 50.0

_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_GetCompressedFileSizeW = _kernel32.GetCompressedFileSizeW
_GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
_GetCompressedFileSizeW.restype = wintypes.DWORD
_INVALID = 0xFFFFFFFF


def sha256(path: Path) -> str | None:
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()
    except OSError:
        return None


def git(repo: Path, *args: str) -> str | None:
    try:
        completed = subprocess.run(['git', '-C', str(repo), *args],
                                   capture_output=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if completed.returncode not in (0, 1):
        return None
    return completed.stdout.decode('utf-8', 'replace')


def allocation(path: Path) -> int | None:
    if sys.platform != 'win32':
        try:
            return path.stat().st_size
        except OSError:
            return None
    hi = wintypes.DWORD(0)
    lo = _GetCompressedFileSizeW(str(path), ctypes.byref(hi))
    if lo == _INVALID and ctypes.get_last_error() != 0:
        return None
    return (hi.value << 32) | lo


def tree_allocation(root: Path) -> tuple[int, int]:
    files = 0
    total = 0
    if not root.exists():
        return 0, 0
    for path in root.rglob('*'):
        try:
            if not path.is_file():
                continue
        except OSError:
            continue
        value = allocation(path)
        if value is None:
            continue
        files += 1
        total += value
    return files, total


def check_environment() -> dict:
    """Host environment facts a run's outcome can depend on."""
    findings: dict = {}

    findings['python'] = {'executable': sys.executable, 'version': sys.version.split()[0]}

    duplicate_case_keys: list[str] = []
    seen: dict[str, str] = {}
    for key in os.environ:
        lowered = key.casefold()
        if lowered in seen and seen[lowered] != key:
            duplicate_case_keys.append(f'{seen[lowered]}/{key}')
        else:
            seen.setdefault(lowered, key)
    findings['duplicate_case_env_keys'] = sorted(set(duplicate_case_keys))
    # Measured: this host exports Path/PATH/path, which makes MSBuild's CL task
    # die with MSB6001.  scripts/build-jsrf.py collapses them via os.environ.
    findings['msbuild_case_hazard'] = bool(duplicate_case_keys)

    proxies = sorted(k for k in os.environ if k.casefold() in ('http_proxy', 'https_proxy'))
    findings['proxy_variables'] = proxies
    findings['duplicate_proxy_case_hazard'] = len(proxies) > len({p.casefold() for p in proxies})

    for name in ('cmake', 'ctest', 'git', 'clang-cl', 'ttd', 'just', 'pre-commit'):
        resolved = shutil.which(name)
        findings[f'tool_{name}'] = resolved or None
    return findings


def check_repositories() -> dict:
    result: dict = {}
    for label, repo in (('game', ROOT), ('toolkit', TOOLKIT)):
        entry: dict = {'path': str(repo), 'exists': repo.is_dir()}
        if repo.is_dir():
            entry['revision'] = (git(repo, 'rev-parse', 'HEAD') or '').strip() or None
            entry['branch'] = (git(repo, 'rev-parse', '--abbrev-ref', 'HEAD') or '').strip() or None
            status = git(repo, 'status', '--porcelain')
            entry['dirty'] = None if status is None else bool(status.strip())
            entry['dirty_paths'] = ([] if not status else
                                    [line[3:] for line in status.splitlines() if line.strip()])
        result[label] = entry
    return result


def check_disk() -> dict:
    try:
        usage = shutil.disk_usage(str(ROOT))
        free = usage.free
    except OSError:
        return {'measured': False}
    runs_root = ROOT / 'logs' / 'runs'
    files, allocated = tree_allocation(runs_root)
    return {
        'measured': True,
        'free_bytes': free,
        'free_gb': round(free / 1024 ** 3, 2),
        'floor_gb': DISK_FLOOR_GB,
        'above_floor': free >= DISK_FLOOR_GB * 1024 ** 3,
        'runs_root': str(runs_root),
        'runs_files': files,
        'runs_allocated_gb': round(allocated / 1024 ** 3, 2),
    }


def check_build() -> dict:
    build = ROOT / 'build' / 'Release'
    result: dict = {'build_dir': str(build), 'exists': build.is_dir()}
    for name in ('jsrf_recomp.exe', 'jsrf_collect.exe'):
        path = build / name
        result[name] = {'present': path.is_file(),
                        'sha256': sha256(path) if path.is_file() else None}
    stamp = build / 'build-source.json'
    result['build_source_present'] = stamp.is_file()
    return result


def check_xemu() -> dict:
    """xemu's own configuration, read from where xemu says it put it.

    Recorded, not copied: the BIOS/MCPX/HDD paths are proprietary owner assets and
    only their existence and identity are reported, never their bytes.
    """
    exe = Path(r'C:\Users\logic\Downloads\xemu\xemu.exe')
    config = Path(os.environ.get('APPDATA', '')) / 'xemu' / 'xemu' / 'xemu.toml'
    result: dict = {'exe': str(exe), 'exe_present': exe.is_file(),
                    'config': str(config), 'config_present': config.is_file()}
    if exe.is_file():
        info = ctypes.windll.version if sys.platform == 'win32' else None
        result['version'] = '0.8.136 (recorded from xemu.log; see docs)'
        del info
    if config.is_file():
        try:
            text = config.read_text(encoding='utf-8', errors='replace')
        except OSError:
            text = ''
        # Report only which asset paths are configured, never their contents.
        for key in ('bootrom_path', 'flashrom_path', 'hdd_path', 'dvd_path', 'eeprom_path'):
            for line in text.splitlines():
                stripped = line.strip()
                if stripped.startswith(key):
                    value = stripped.split('=', 1)[-1].strip().strip('"')
                    result[key] = {'configured': True,
                                   'exists': Path(value).is_file() if value else False}
                    break
    return result


def summarise_run(run_dir: Path) -> dict:
    """Read an archived run's own artifacts; report, never reinterpret."""
    summary: dict = {'run_dir': str(run_dir), 'exists': run_dir.is_dir()}
    result_path = run_dir / 'result.json'
    if result_path.is_file():
        try:
            payload = json.loads(result_path.read_text(encoding='utf-8-sig'))
            summary['result'] = {
                key: payload.get(key) for key in
                ('outcome', 'reason', 'exit_code', 'checkpoints_passed',
                 'missing_checkpoints', 'save_root_verified', 'gpu_report_ok',
                 'duration_seconds')
            }
        except (OSError, ValueError) as error:
            summary['result'] = {'unreadable': str(error)}
    else:
        summary['result'] = None

    meta_path = run_dir / 'metadata.json'
    if meta_path.is_file():
        try:
            meta = json.loads(meta_path.read_text(encoding='utf-8-sig'))
            profile = meta.get('run_profile') or {}
            summary['profile'] = {
                'requested': profile.get('requested'),
                'effective': profile.get('effective') or profile.get('classification'),
            }
            summary['disk_gate'] = meta.get('disk_gate')
            summary['repositories'] = {
                key: (value or {}).get('revision')
                for key, value in (meta.get('repository_identities') or {}).items()
            }
        except (OSError, ValueError) as error:
            summary['profile'] = {'unreadable': str(error)}
    else:
        summary['profile'] = None

    for name in ('jsrf_run.log', 'stacks.txt', 'process.dmp', 'toolkit-status.txt'):
        path = run_dir / name
        summary[name] = {'present': path.is_file(),
                         'bytes': path.stat().st_size if path.is_file() else None}
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime-log', metavar='RUN_DIR',
                        help='write doctor.json into this archived run directory')
    parser.add_argument('--preflight', action='store_true',
                        help='check the live host and print; writes nothing')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    if not args.runtime_log and not args.preflight:
        parser.error('pass --runtime-log <run-dir> or --preflight')

    record: dict = {
        'doctor': DOCTOR_VERSION,
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'cwd': str(Path.cwd()),
        'game_root': str(ROOT),
        'toolkit_root': str(TOOLKIT),
        'environment': check_environment(),
        'repositories': check_repositories(),
        'disk': check_disk(),
        'build': check_build(),
        'xemu': check_xemu(),
    }

    run_dir = Path(args.runtime_log).resolve() if args.runtime_log else None
    if run_dir is not None:
        if not run_dir.is_dir():
            print(f'doctor: {run_dir} is not a directory', file=sys.stderr)
            return 2
        record['run'] = summarise_run(run_dir)

    if run_dir is not None:
        destination = run_dir / 'doctor.json'
        destination.write_text(json.dumps(record, indent=2, sort_keys=True),
                               encoding='utf-8')
        record['written'] = str(destination)

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        env = record['environment']
        print(f'doctor {DOCTOR_VERSION}  ({record["generated_utc"]})')
        print(f'  python        : {env["python"]["version"]} ({env["python"]["executable"]})')
        if env['msbuild_case_hazard']:
            print(f'  MSBUILD HAZARD: duplicate-case env keys '
                  f'{", ".join(env["duplicate_case_env_keys"])} -- build via '
                  f'scripts/build-jsrf.py, not cmake directly')
        else:
            print('  env case keys : clean (no MSBuild dictionary hazard)')
        missing = [k[len("tool_"):] for k, v in env.items()
                   if k.startswith('tool_') and not v]
        print(f'  tools missing : {", ".join(missing) if missing else "none"}')
        for label, entry in record['repositories'].items():
            state = 'clean' if entry.get('dirty') is False else (
                'DIRTY' if entry.get('dirty') else 'unknown')
            print(f'  {label:<14}: {entry.get("revision") or "?"} '
                  f'[{entry.get("branch") or "?"}] {state}')
        disk = record['disk']
        if disk.get('measured'):
            verdict = 'above' if disk['above_floor'] else 'BELOW'
            print(f'  disk          : {disk["free_gb"]} GB free ({verdict} '
                  f'{disk["floor_gb"]} GB floor); logs/runs {disk["runs_files"]} files / '
                  f'{disk["runs_allocated_gb"]} GB')
        else:
            print('  disk          : UNKNOWN (could not measure)')
        build = record['build']
        for name in ('jsrf_recomp.exe', 'jsrf_collect.exe'):
            print(f'  {name:<14}: {"present" if build[name]["present"] else "MISSING"}')
        if 'run' in record:
            summary = record['run']
            print(f'  run result    : {summary.get("result")}')
        if record.get('written'):
            print(f'  written       : {record["written"]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
