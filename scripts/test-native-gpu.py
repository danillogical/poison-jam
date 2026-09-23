"""Run the native D3D11 GPU fixture and capture tool artifacts.

Python port of the deleted `scripts/test-native-gpu.ps1`. The PowerShell original
could not run on this host at all: the effective execution policy is `Restricted`
(every scope is `Undefined`, which falls back to the Windows-client default), so
any `.ps1` fails with "running scripts is disabled on this system". This file is
therefore the only working driver for the native fixture, not a convenience
alternative to one.

Steps, in the original order:

  1. `build-identity.py verify` -- refuse a stale or edited build.
  2. Archive the fixture exe/pdb, `build-source.json` and the fixture source
     into a timestamped folder under `logs/native-gpu/`.
  3. Record the RenderDoc and Nsight Systems paths and versions in `metadata.json`.
  4. Run the fixture (optionally `--warp`) and validate its JSON result:
     `outcome == "pass"`, `pixels_verified == 4096`, `debug_errors == 0`.
  5. Optionally capture with RenderDoc and/or Nsight Systems.

The fixture writes its result JSON to stdout and its diagnostics to stderr, so
the two are redirected to `baseline.json` and `baseline-errors.log`.

Usage:
    python -X utf8 scripts/test-native-gpu.py
    python -X utf8 scripts/test-native-gpu.py --warp
    python -X utf8 scripts/test-native-gpu.py --capture renderdoc
    python -X utf8 scripts/test-native-gpu.py --capture nsight
    python -X utf8 scripts/test-native-gpu.py --capture all
"""
from datetime import datetime
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
release = root / 'build' / 'Release'
expected_pixels = 4096


def fail(message):
    raise SystemExit(message)


def file_version(path):
    """Windows FileVersion string for `path`, or None when unavailable.

    The original read `.VersionInfo.FileVersion`; ctypes is the stdlib-only
    equivalent. Version text is provenance metadata, so any failure here
    degrades to None rather than stopping the run.
    """
    try:
        size = ctypes.windll.version.GetFileVersionInfoSizeW(ctypes.c_wchar_p(str(path)), None)
        if not size:
            return None
        buffer = ctypes.create_string_buffer(size)
        if not ctypes.windll.version.GetFileVersionInfoW(
                ctypes.c_wchar_p(str(path)), 0, size, buffer):
            return None
        pointer = ctypes.c_void_p()
        length = ctypes.c_uint()
        if not ctypes.windll.version.VerQueryValueW(
                buffer, ctypes.c_wchar_p('\\VarFileInfo\\Translation'),
                ctypes.byref(pointer), ctypes.byref(length)):
            return None
        words = ctypes.cast(pointer, ctypes.POINTER(ctypes.c_ushort))
        query = f'\\StringFileInfo\\{words[0]:04x}{words[1]:04x}\\FileVersion'
        value = ctypes.c_wchar_p()
        if not ctypes.windll.version.VerQueryValueW(
                buffer, ctypes.c_wchar_p(query), ctypes.byref(value), ctypes.byref(length)):
            return None
        return value.value or None
    except Exception:
        return None


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def find_nsight():
    """Newest `nsys.exe` under the Nsight Systems `target-windows-x64` folder.

    Nsight Systems is not Graphics; the installer nests the real binary under a
    versioned directory, so the search is recursive and picks the highest path.
    """
    program_files = os.environ.get('ProgramFiles')
    if not program_files:
        return None
    base = Path(program_files) / 'NVIDIA Corporation'
    if not base.is_dir():
        return None
    found = [Path(folder) / 'nsys.exe'
             for folder, _dirs, files in os.walk(base)
             if Path(folder).name == 'target-windows-x64' and 'nsys.exe' in files]
    if not found:
        return None
    return sorted(found, key=lambda p: str(p), reverse=True)[0]


parser = argparse.ArgumentParser(description='Run and capture the native D3D11 GPU fixture.')
parser.add_argument('--capture', choices=('none', 'renderdoc', 'nsight', 'all'), default='none',
                    help='optional external capture tool to drive after the baseline run')
parser.add_argument('--warp', action='store_true',
                    help='force the software (WARP) adapter instead of the NVIDIA adapter')
args = parser.parse_args()

# 1. Refuse a stale or edited build before archiving anything.
identity = subprocess.run([sys.executable, '-X', 'utf8', str(root / 'scripts' / 'build-identity.py'),
                           'verify'], cwd=root)
if identity.returncode != 0:
    fail('Build identity failed; run scripts/build-jsrf.py.')

# 2. Archive the fixture and the inputs it was built from.
folder = root / 'logs' / 'native-gpu' / datetime.now().strftime('%Y%m%d-%H%M%S-%f')[:-3]
folder.mkdir(parents=True, exist_ok=True)
for name in ('jsrf_gpu_smoke.exe', 'jsrf_gpu_smoke.pdb', 'build-source.json'):
    source = release / name
    if not source.is_file():
        fail(f'Missing build artifact {source}. Build the jsrf_gpu_smoke target first.')
    (folder / name).write_bytes(source.read_bytes())
(folder / 'native_gpu_smoke.c').write_bytes((root / 'tests' / 'native_gpu_smoke.c').read_bytes())
fixture = folder / 'jsrf_gpu_smoke.exe'
fixture_args = ['--warp'] if args.warp else []

# 3. Record which tools are present, so a capture can be interpreted later.
metadata = {
    'kind': 'native_d3d11_fixture',
    'game_rendering': False,
    'capture': args.capture,
    'warp': bool(args.warp),
    'exe_sha256': sha256(fixture),
    'tools': {},
}
renderdoc = None
program_files = os.environ.get('ProgramFiles')
if program_files:
    candidate = Path(program_files) / 'RenderDoc' / 'renderdoccmd.exe'
    if candidate.is_file():
        renderdoc = candidate
if renderdoc is not None:
    metadata['tools']['renderdoc'] = {'path': str(renderdoc), 'version': file_version(renderdoc)}
    header = renderdoc.parent / 'renderdoc_app.h'
    if header.is_file():
        (folder / 'renderdoc_app.h').write_bytes(header.read_bytes())
nsight = find_nsight()
if nsight is not None:
    metadata['tools']['nsight'] = {'path': str(nsight), 'version': file_version(nsight)}
(folder / 'metadata.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')

# 4. Baseline run. cwd is the game root so the run does not depend on where the
#    caller happened to be standing; the original inherited the shell's cwd.
with (folder / 'baseline.json').open('w', encoding='utf-8') as out, \
        (folder / 'baseline-errors.log').open('w', encoding='utf-8') as err:
    baseline = subprocess.run([str(fixture), *fixture_args], cwd=root, stdout=out, stderr=err)
if baseline.returncode != 0:
    fail(f'Native fixture failed; artifacts: {folder}')
try:
    result = json.loads((folder / 'baseline.json').read_text(encoding='utf-8'))
except json.JSONDecodeError as exc:
    fail(f'Native fixture produced no readable result JSON ({exc}); artifacts: {folder}')
if (result.get('outcome') != 'pass' or result.get('pixels_verified') != expected_pixels
        or result.get('debug_errors') != 0):
    fail(f'Native readback/validation failed: {folder}')

# 5. Optional captures, driven separately from the baseline run.
if args.capture in ('renderdoc', 'all'):
    if renderdoc is None:
        fail('Install RenderDoc to use --capture renderdoc.')
    with (folder / 'renderdoc.log').open('w', encoding='utf-8') as log:
        capture = subprocess.run(
            [str(renderdoc), 'capture', '-w', '-d', str(folder), '-c', str(folder / 'renderdoc'),
             '--opt-api-validation', '--opt-capture-callstacks', str(fixture), *fixture_args],
            cwd=root, stdout=log, stderr=subprocess.STDOUT)
    if capture.returncode != 0 or not list(folder.glob('*.rdc')):
        fail(f'RenderDoc capture failed: {folder}')
    if '"renderdoc_capture":true' not in (folder / 'renderdoc.log').read_text(encoding='utf-8'):
        fail('Fixture did not confirm programmatic capture; reconfigure with the RenderDoc '
             f'header available: {folder}')

if args.capture in ('nsight', 'all'):
    if nsight is None:
        fail('Install Nsight Systems to use --capture nsight.')
    with (folder / 'nsight.log').open('w', encoding='utf-8') as log:
        profile = subprocess.run(
            [str(nsight), 'profile', '--trace=dx11,dx11-annotations', '--sample=none',
             '--cpuctxsw=none', '--wait=primary', f'--output={folder / "nsight"}',
             str(fixture), *fixture_args],
            cwd=root, stdout=log, stderr=subprocess.STDOUT)
    if profile.returncode != 0 or not list(folder.glob('*.nsys-rep')):
        fail(f'Nsight capture failed: {folder}')

print(f'Native GPU artifacts: {folder}')
print(json.dumps(result, separators=(',', ':')))
