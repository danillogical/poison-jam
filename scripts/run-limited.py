"""Run any command un-elevated, from an elevated session, through a limited-rights scheduled task.

Why: the Vulkan loader ignores `VK_DRIVER_FILES` in an elevated process, so the software-ICD
(llvmpipe) tests and controls only run un-elevated, and an ssh session on the Windows host is
elevated. A scheduled task created with `/rl limited` runs the command at medium integrity.

    python scripts/run-limited.py [--cwd DIR] [--env KEY=VALUE]... [--timeout SECONDS]
                                  [--software-icd] -- COMMAND [ARGS...]

The task is created, run, waited for (with a timeout), its combined output is printed and its
exit code is returned. The task and every temporary file are removed on every path. When this
process is NOT elevated (or not on Windows) the command is run directly instead: there is
nothing to drop.

`--software-icd` sets the software Vulkan ICD environment (`VK_DRIVER_FILES`,
`VK_ICD_FILENAMES`, `XBR_VK_SW_ICD`, `RECOMP_VK_DEVICE=llvmpipe`) from the game build cache's
`XBR_VK_SW_ICD` entry. A leading `python`/`python3` in COMMAND is replaced with the Python that
runs this script, because a scheduled task does not inherit the caller's PATH.

Exit codes: the command's, or 124 on timeout, or 2 on a task/usage failure.
"""
import argparse
import ctypes
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolkit_path import cache_entry  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
ICD_RE = re.compile(r'^\s*XBR_VK_SW_ICD\s*(?::\s*[A-Za-z_][A-Za-z0-9_]*\s*)?=\s*(.*?)\s*$',
                    re.MULTILINE)
POLL_SECONDS = 1.0


def is_elevated():
    """True only for an elevated Windows process; everything else has nothing to drop."""
    if os.name != 'nt':
        return False
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except (AttributeError, OSError):
        return False


def software_icd_env(build_dir=None):
    """The software-ICD environment, from the build cache's `XBR_VK_SW_ICD`."""
    icd = cache_entry(Path(build_dir or ROOT / 'build') / 'CMakeCache.txt', ICD_RE)
    if not icd:
        sys.exit('run-limited: --software-icd needs XBR_VK_SW_ICD in build/CMakeCache.txt '
                 '(configure the game with a software ICD first)')
    icd = str(Path(icd))
    return {'VK_DRIVER_FILES': icd, 'VK_ICD_FILENAMES': icd, 'XBR_VK_SW_ICD': icd,
            'RECOMP_VK_DEVICE': 'llvmpipe'}


def bat_quote(text):
    """A command-line token for a .bat file: `%` doubles so cmd does not expand it."""
    return subprocess.list2cmdline([text]).replace('%', '%%')


def run_direct(command, cwd, env, timeout):
    try:
        return subprocess.run(command, cwd=cwd, env=env, timeout=timeout).returncode
    except subprocess.TimeoutExpired:
        print(f'run-limited: timed out after {timeout}s', file=sys.stderr)
        return 124


def schtasks(*args):
    return subprocess.run(['schtasks', *args], capture_output=True, text=True)


def run_task(command, cwd, env_overrides, timeout):
    name = 'run_limited_' + uuid.uuid4().hex[:8]
    # Not `tempfile.mkdtemp`: on Windows it restricts the directory to the elevated token's
    # ACL, which the limited task (the same user, medium integrity) then cannot write to.
    work = Path(tempfile.gettempdir()) / name
    work.mkdir()
    bat, out, rc = work / (name + '.bat'), work / (name + '.out'), work / (name + '.rc')
    lines = ['@echo off']
    if cwd:
        lines.append(f'cd /d "{cwd}"')
    for key, value in env_overrides.items():
        lines.append(f'set "{key}={value}"'.replace('%', '%%'))
    lines.append(' '.join(bat_quote(token) for token in command) + f' > "{out}" 2>&1')
    # The code goes to a temporary name first, so its appearance means it is complete.
    lines.append(f'echo %errorlevel% > "{rc}.tmp"')
    lines.append(f'move /y "{rc}.tmp" "{rc}" > nul')
    bat.write_text('\r\n'.join(lines) + '\r\n')
    code = 2
    created = False
    try:
        result = schtasks('/create', '/f', '/tn', name, '/tr', str(bat), '/sc', 'once',
                          '/st', '23:59', '/rl', 'limited',
                          '/ru', os.environ.get('USERNAME', ''), '/it')
        if result.returncode:
            print('run-limited: schtasks create failed: ' + result.stdout + result.stderr,
                  file=sys.stderr)
            return 2
        created = True
        result = schtasks('/run', '/tn', name)
        if result.returncode:
            print('run-limited: schtasks run failed: ' + result.stdout + result.stderr,
                  file=sys.stderr)
            return 2
        deadline = time.time() + timeout
        while not rc.exists():
            if time.time() > deadline:
                schtasks('/end', '/tn', name)
                print(f'run-limited: timed out after {timeout}s (task {name})', file=sys.stderr)
                code = 124
                break
            time.sleep(POLL_SECONDS)
        else:
            code = int(rc.read_text().strip() or 1)
        if out.exists():
            sys.stdout.write(out.read_text(errors='replace'))
            sys.stdout.flush()
    finally:
        if created:
            schtasks('/delete', '/f', '/tn', name)
        shutil.rmtree(work, ignore_errors=True)
    return code


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--cwd', help='working directory for the command (default: current)')
    parser.add_argument('--env', action='append', default=[], metavar='KEY=VALUE',
                        help='environment variable for the command; repeatable')
    parser.add_argument('--timeout', type=int, default=1800,
                        help='seconds to wait for the command (default 1800)')
    parser.add_argument('--software-icd', action='store_true',
                        help='set the llvmpipe ICD environment from the build cache')
    parser.add_argument('command', nargs=argparse.REMAINDER, help='-- COMMAND [ARGS...]')
    args = parser.parse_args(argv)
    command = args.command[1:] if args.command[:1] == ['--'] else args.command
    if not command:
        parser.error('no command given (put it after --)')
    if command[0] in ('python', 'python3'):
        command[0] = sys.executable
    env_overrides = software_icd_env() if args.software_icd else {}
    for item in args.env:
        key, sep, value = item.partition('=')
        if not sep or not key:
            parser.error(f'--env wants KEY=VALUE, got {item!r}')
        env_overrides[key] = value
    cwd = str(Path(args.cwd).resolve()) if args.cwd else os.getcwd()
    if not is_elevated():
        env = dict(os.environ)
        env.update(env_overrides)
        return run_direct(command, cwd, env, args.timeout)
    return run_task(command, cwd, env_overrides, args.timeout)


if __name__ == '__main__':
    sys.exit(main())
