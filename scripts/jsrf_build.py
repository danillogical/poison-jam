"""P0.5: the guarded build entry point's support library.

Three things this provides, each fixing a measured failure:

1. **Preflight** (`preflight`).  Reports the resolved Python, CMake and CTest,
   the toolchain dependencies, and the parallel setting -- *before* any mutation.
   Tests must be able to distinguish a missing tool, an invalid parallel count, a
   normalized environment, an ordinary compiler failure, and the measured silent
   confined signature.
2. **Target inventory derived from CMake** (`discover_required_targets`).  The
   hand-maintained list in `build-jsrf.py` names 11 targets, but CTest discovers
   **12** tests: `xbox_timestamp_publication` comes from the *toolkit*
   subdirectory (`add_subdirectory(${XBOXRECOMP_DIR} ...)`) and no hand-written
   list in this repository can know about it.  MSBuild deletes a target's output
   when its link fails, so a build that does not name that target leaves the test
   permanently "Not Run" with nothing in the log to explain it.  The inventory is
   therefore *generated*, and the build entry point fails when the built set does
   not cover the discovered set.
3. **The measured silent-failure signature** (`is_confined_silent_failure`).  A
   confined sandbox blocks MSBuild's named-pipe workers; the build then exits 1
   with **no error text**, stopping at `Checking File Globs`.  Only that exact
   signature permits a serial retry, and the first log is preserved.
"""
from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
INVENTORY_SCHEMA_VERSION = 1
INVENTORY_PATH = ROOT / 'build' / 'target-inventory.json'

# The measured silent confined signature.  Deliberately narrow: matching a broad
# "build failed with no output" would let a genuine compile error be retried
# serially and, worse, reported as an environment limitation.
CONFINED_MARKERS = (
    'Checking File Globs',
    'ResolveProjectReferences',
)
CONFINED_REQUIRED = 'Checking File Globs'


class BuildError(RuntimeError):
    """Preflight or inventory failed in a way the caller must not ignore."""


# ── preflight ────────────────────────────────────────────────────────────────

def _resolve_tool(name: str) -> str | None:
    return shutil.which(name)


def normalized_environment(environ: dict[str, str] | None = None) -> dict[str, str]:
    """Collapse duplicate-case Windows environment keys.

    This machine's block contains `Path`, `PATH` and `path`.  MSBuild's CL task
    builds a case-sensitive dictionary from it and dies with
    `MSB6001 ... Item has already been added. Key in dictionary: 'Path'`.  The
    duplicates arrive with the host process, so they cannot be removed from
    PowerShell; `os.environ` collapses them, which is why the build must be
    launched from Python.
    """
    source = os.environ if environ is None else environ
    collapsed: dict[str, str] = {}
    for key, value in source.items():
        collapsed[key.upper()] = value
    return collapsed


def duplicate_case_keys(environ: dict[str, str] | None = None) -> list[str]:
    """Keys that appear under more than one casing in the raw environment."""
    source = os.environ if environ is None else environ
    seen: dict[str, list[str]] = {}
    for key in source:
        seen.setdefault(key.upper(), []).append(key)
    return sorted(name for names in seen.values() if len(names) > 1 for name in names)


def preflight(parallel: int = 4, environ: dict[str, str] | None = None) -> dict[str, Any]:
    """Report the resolved toolchain and refuse an unusable configuration."""
    result: dict[str, Any] = {
        'python': sys.executable,
        'python_version': sys.version.split()[0],
        'cmake': _resolve_tool('cmake'),
        'ctest': _resolve_tool('ctest'),
        'parallel': parallel,
        'duplicate_case_keys': duplicate_case_keys(environ),
        'environment_normalized': True,
        'problems': [],
        'missing_tools': [],
    }
    for tool in ('cmake', 'ctest'):
        if not result[tool]:
            result['missing_tools'].append(tool)
            result['problems'].append(f'{tool} was not found on PATH')

    if not isinstance(parallel, int) or isinstance(parallel, bool) or parallel < 1:
        result['problems'].append(
            f'--parallel must be a positive integer, got {parallel!r}')

    # The toolkit carries targets CTest runs, so a missing sibling is fatal.
    toolkit = ROOT.parent / 'xboxrecomp'
    result['toolkit'] = str(toolkit)
    if not toolkit.is_dir():
        result['problems'].append(f'toolkit tree is missing: {toolkit}')
    else:
        cmake_lists = ROOT / 'CMakeLists.txt'
        if not cmake_lists.is_file():
            result['problems'].append('CMakeLists.txt is missing')
        else:
            text = cmake_lists.read_text(encoding='utf-8', errors='replace')
            if 'add_subdirectory' not in text:
                result['problems'].append(
                    'CMakeLists.txt no longer adds the toolkit subdirectory, so the '
                    'derived target inventory would be incomplete')

    build_dir = ROOT / 'build'
    result['build_dir'] = str(build_dir)
    result['build_dir_exists'] = build_dir.is_dir()
    result['ok'] = not result['problems']
    return result


# ── target inventory ─────────────────────────────────────────────────────────

def ctest_tests(build_dir: Path | None = None) -> list[dict[str, str]]:
    """Every test CTest discovers, with its executable or command.

    This is the *denominator*.  A suite total cannot stand in for it, and a test
    that is 'Not Run' because its target was never built must remain in the
    denominator rather than vanish.
    """
    build_dir = build_dir or (ROOT / 'build')
    if not build_dir.is_dir():
        raise BuildError(f'build directory is missing: {build_dir}')
    proc = subprocess.run(['ctest', '--test-dir', str(build_dir), '-C', 'Release', '-N'],
                          cwd=ROOT, capture_output=True, text=True)
    if proc.returncode != 0:
        raise BuildError(f'ctest -N failed: {proc.stderr.strip() or proc.stdout.strip()}')
    tests: list[dict[str, str]] = []
    for line in proc.stdout.split('\n'):
        match = re.match(r'\s*Test\s+#(\d+):\s+(\S+)', line)
        if match:
            tests.append({'index': match.group(1), 'name': match.group(2)})
    return tests


def target_for_test(build_dir: Path) -> dict[str, str]:
    """Map each CTest test to the CMake target that produces its command.

    `CTestTestfile.cmake` records one `add_test` per configuration, all quoted:

        add_test("jsrf_save_root" ".../build/Release/jsrf_save_root_test.exe")

    The executable's stem is the build target.  A python-driven test has no build
    target, which is recorded rather than guessed.  Every `CTestTestfile.cmake`
    under the build tree is read, including the toolkit's -- which is exactly how
    `xbox_timestamp_publication` is discovered at all.
    """
    mapping: dict[str, str] = {}
    pattern = re.compile(r'add_test\(\s*"([^"]+)"\s+"([^"]*)"')
    for path in sorted(build_dir.rglob('CTestTestfile.cmake')):
        text = path.read_text(encoding='utf-8', errors='replace')
        for match in pattern.finditer(text):
            name, command = match.group(1), match.group(2)
            first = command.strip().split()[0] if command.strip() else ''
            if not first:
                continue
            if first.endswith('.py') or Path(first).name.lower().startswith('python'):
                mapping.setdefault(name, '')
            else:
                mapping.setdefault(name, Path(first).stem)
    return mapping


def discover_required_targets(build_dir: Path | None = None) -> dict[str, Any]:
    """Derive the build target list from CMake/CTest, not a hand-written list."""
    build_dir = build_dir or (ROOT / 'build')
    tests = ctest_tests(build_dir)
    mapping = target_for_test(build_dir)
    targets: list[str] = []
    unmapped: list[str] = []
    for test in tests:
        target = mapping.get(test['name'])
        if target is None:
            unmapped.append(test['name'])
        elif target:
            if target not in targets:
                targets.append(target)
        else:
            # A python-driven test has no build target; the interpreter does.
            unmapped.append(test['name'])
    return {
        'schema_version': INVENTORY_SCHEMA_VERSION,
        'tests': tests,
        'test_count': len(tests),
        'targets': sorted(targets),
        'tests_without_build_target': sorted(unmapped),
    }


def write_inventory(build_dir: Path | None = None,
                    path: Path | None = None) -> dict[str, Any]:
    build_dir = build_dir or (ROOT / 'build')
    path = path or INVENTORY_PATH
    inventory = discover_required_targets(build_dir)
    inventory['source'] = 'derived from ctest -N and CTestTestfile.cmake'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(inventory, indent=2, sort_keys=True), encoding='utf-8')
    return inventory


def hand_maintained_targets(build_script: Path | None = None) -> list[str] | None:
    """Read a literal TARGETS list out of build-jsrf.py, if one still exists.

    Returns ``None`` when the entry point has no literal list -- which is the
    desired end state, not an error.  It used to raise, and that broke the recorded
    P0.5-AC2 evidence command once the list was removed: a checker that fails when
    the defect is *fixed* is measuring the wrong thing.
    """
    build_script = build_script or (ROOT / 'scripts' / 'build-jsrf.py')
    text = build_script.read_text(encoding='utf-8', errors='replace')
    match = re.search(r'^TARGETS\s*=\s*\[(.*?)\]', text, re.DOTALL | re.MULTILINE)
    if not match:
        return None
    return sorted(re.findall(r"'([^']+)'", match.group(1)))


def inventory_gaps(build_dir: Path | None = None) -> dict[str, Any]:
    """Which discovered targets a hand-maintained list would fail to build.

    When no literal list exists the comparison is reported as ``unavailable``
    rather than raising: the absence of a hand-maintained list is the *fixed*
    state, so a caller must be able to observe it as a result.
    """
    derived = discover_required_targets(build_dir)
    hand = hand_maintained_targets()
    if hand is None:
        return {
            'derived_targets': derived['targets'],
            'hand_maintained_targets': None,
            'discovered_but_not_hand_listed': [],
            'hand_listed_but_not_discovered': [],
            'complete': True,
            'comparison': 'unavailable: the entry point derives its targets, so there '
                          'is no hand-maintained list to compare against',
        }
    missing = [t for t in derived['targets'] if t not in hand]
    extra = [t for t in hand if t not in derived['targets']]
    return {
        'derived_targets': derived['targets'],
        'hand_maintained_targets': hand,
        'discovered_but_not_hand_listed': missing,
        'hand_listed_but_not_discovered': extra,
        'complete': not missing,
        'comparison': 'performed',
    }


# ── failure classification ───────────────────────────────────────────────────

def is_confined_silent_failure(returncode: int, log_text: str) -> bool:
    """The measured confined-sandbox signature, and nothing else.

    Requires a nonzero exit, the `Checking File Globs` marker, and the *absence*
    of real compiler diagnostics.  A build that fails with actual errors must not
    match, or a genuine defect would be retried serially and misreported as an
    environment limitation.
    """
    if returncode == 0:
        return False
    if CONFINED_REQUIRED not in log_text:
        return False
    if not any(marker in log_text for marker in CONFINED_MARKERS):
        return False
    # Real diagnostics that must disqualify the signature.
    for disqualifier in ('error C', 'error LNK', 'error MSB', ': error', 'fatal error'):
        if disqualifier in log_text:
            return False
    return True


def classify_build_failure(returncode: int, log_text: str) -> str:
    """Name the failure class, so tests can distinguish the cases."""
    if returncode == 0:
        return 'success'
    if is_confined_silent_failure(returncode, log_text):
        return 'confined_silent'
    for marker, name in (('error C', 'compile_error'),
                         ('error LNK', 'link_error'),
                         ('error MSB', 'msbuild_error'),
                         ('CMake Error', 'cmake_error')):
        if marker in log_text:
            return name
    return 'unknown_failure'
