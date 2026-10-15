"""Build the game and every target CTest runs, from a sanitized environment.

This is the working entry point. The PowerShell wrapper it once mirrored no
longer exists (`scripts/build-jsrf.ps1` was never committed), so this script is
the only implementation -- see the note at the end.

It cannot be run from a host whose environment block carries the same variable
under several cases: this machine's block contains `Path`, `PATH` and `path`, and
MSBuild's CL task builds a case-sensitive dictionary out of that block and dies
with

    MSB6001: Invalid command line switch for "CL.exe".
    System.ArgumentException: Item has already been added.
    Key in dictionary: 'Path'  Key being added: 'PATH'

The duplicates arrive with the host process, so no PowerShell session can remove
them -- the block is already malformed by the time PowerShell starts, and
`Get-ChildItem env:` itself fails on it. Python's `os.environ` collapses the case
variants to one key, so launching the same command sequence from here hands the
toolchain a well-formed environment.

**`--parallel N` for N > 1 fails under a CONFINED file policy, and works under
full access -- this is policy-dependent, not a property of the tree.** MSBuild's
multi-node workers talk over named pipes, which a confined sandbox can block.
The failure is silent and looks like a code defect: exit 1 with **no error text at
all**, the log stopping at `Checking File Globs`, and `-- /verbosity:diagnostic`
reporting `Done building target "ResolveProjectReferences" ... -- FAILED` with
`0 Error(s)`. `MSBUILDDISABLENODEREUSE=1` does not help.

Measured both ways 2026-09-22: `--parallel 1` builds cleanly when confined, and
`--parallel 4` builds cleanly under full access. **So: if a build dies silently at
`Checking File Globs`, retry with `--parallel 1` before investigating the tree** --
and do not record the parallel limit as a hard fact, because it is not one. An
earlier version of this docstring stated the restriction unconditionally, which is
exactly the mistake to avoid.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
logs = root / 'logs'
logs.mkdir(exist_ok=True)
env = dict(os.environ)
python = sys.executable

sys.path.insert(0, str(root / 'scripts'))
import jsrf_build  # noqa: E402

# The provenance schema is shared with the generator that records it; load it by
# path because `vsh_aot_provenance.py` is a module in name only (it is imported by
# two scripts, not installed).
_prov_spec = importlib.util.spec_from_file_location(
    'vsh_aot_provenance', root / 'scripts' / 'vsh_aot_provenance.py')
prov = importlib.util.module_from_spec(_prov_spec)
sys.modules['vsh_aot_provenance'] = prov
_prov_spec.loader.exec_module(prov)

# The generator owns this title's measured program counts and the search order for
# the host tool; import it by path for the same reason, so the guard cannot drift
# from the generator about either.
_gen_spec = importlib.util.spec_from_file_location(
    'gen_shaders', root / 'scripts' / 'gen-shaders.py')
gen_shaders = importlib.util.module_from_spec(_gen_spec)
sys.modules['gen_shaders'] = gen_shaders
_gen_spec.loader.exec_module(gen_shaders)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--parallel', type=int, default=4,
                    help='MSBuild node count; use 1 where the sandbox blocks '
                         'MSBuild named pipes (default: 4)')
parser.add_argument('--allow-regeneration', action='store_true',
                    help='permit recovery regeneration (off by default: it can erase '
                         'current ABI instrumentation; see scripts/check-generation-'
                         'provenance.py, which records and checks that baseline)')
args = parser.parse_args()

# The build set is DERIVED from CMake/CTest, not hand-maintained.  Measured: the
# previous literal list named 11 targets while CTest discovers 12 tests --
# `xbox_timestamp_test` comes from the toolkit subdirectory and no list in this
# repository can know about it.  MSBuild deletes a target's output when its link
# fails, so omitting it leaves that test permanently "Not Run" with nothing in the
# log to explain it.
ARTIFACT_TARGETS = ['jsrf_recomp', 'jsrf_collect']


def build_targets():
    """Every target CTest needs, plus the artifacts the runner copies."""
    derived = jsrf_build.discover_required_targets()
    targets = list(derived['targets'])
    for name in ARTIFACT_TARGETS:
        if name not in targets:
            targets.append(name)
    return sorted(set(targets)), derived


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


def run_build(targets, parallel, log_name):
    """Build, and retry serially ONLY on the measured confined signature."""
    argv = ['cmake', '--build', 'build', '--config', 'Release', '--target', *targets,
            '--parallel', str(parallel)]
    with (logs / log_name).open('w', encoding='utf-8', errors='replace') as log:
        proc = subprocess.run(argv, cwd=root, env=env, stdout=log,
                              stderr=subprocess.STDOUT, text=True)
    text = (logs / log_name).read_text(encoding='utf-8', errors='replace')
    failure = jsrf_build.classify_build_failure(proc.returncode, text)
    if failure == 'success':
        return True, 'success'
    if failure == 'confined_silent' and parallel != 1:
        # Preserve the first log; never overwrite the evidence of the original run.
        print('Build died with the measured confined-sandbox signature '
              '(silent, stopped at "Checking File Globs"). Retrying serially; '
              f'the original log is preserved at logs/{log_name}.')
        serial_log = f'{Path(log_name).stem}-serial.log'
        argv_serial = ['cmake', '--build', 'build', '--config', 'Release',
                       '--target', *targets, '--parallel', '1']
        with (logs / serial_log).open('w', encoding='utf-8', errors='replace') as log:
            proc_serial = subprocess.run(argv_serial, cwd=root, env=env, stdout=log,
                                         stderr=subprocess.STDOUT, text=True)
        if proc_serial.returncode == 0:
            return True, 'success_after_serial_retry'
        print(f'Serial retry also failed; see logs/{serial_log}.')
        return False, 'confined_silent_serial_retry_failed'
    print(f'Build failed ({failure}). See logs/{log_name}.')
    return False, failure


# ── the generated shader corpus ──────────────────────────────────────────────
CORPUS_DIR = root / 'src' / 'recomp' / 'gen' / 'shaders'
CORPUS_FILES = ('nv2a_vsh_programs.c', 'nv2a_vsh_programs.h')


def corpus_dxc():
    """The dxc the corpus was compiled with, or a refusal naming what is missing."""
    sdk = os.environ.get('VULKAN_SDK')
    if not sdk:
        raise SystemExit(
            'VULKAN_SDK is not set, so the corpus provenance cannot be re-derived: the '
            'recorded dxc version and flags are part of what makes the SPIR-V current. '
            f'Install the Vulkan SDK and set VULKAN_SDK; then {prov.REMEDY}.')
    dxc = Path(sdk) / 'Bin' / 'dxc.exe'
    if not dxc.is_file():
        raise SystemExit(
            f'{dxc} is not there (VULKAN_SDK={sdk}), so the corpus provenance cannot be '
            f're-derived. Install the Vulkan SDK shader tools; then {prov.REMEDY}.')
    return dxc


def check_corpus_provenance():
    """Refuse a corpus the current inputs have moved past; return a summary string.

    This runs BEFORE the CMake configure step, so a stale corpus never reaches the
    compiler -- and the corpus is retail-derived generated output, so the refusal
    names the path and every differing field and then names `just shaders`. There is
    deliberately no bypass flag and no auto-regeneration: a guard that could be
    waved through would not be a guard, and regenerating under a failing build is
    how the wrong half of a pipeline gets rewritten.

    `tool.sha256` is compared but never decisive (the executable is not
    reproducible on this toolchain), so it is reported as advisory only.
    """
    missing = [CORPUS_DIR / name for name in CORPUS_FILES if not (CORPUS_DIR / name).is_file()]
    if missing:
        raise SystemExit(
            'The NV2A vertex-program corpus is missing: '
            + ', '.join(str(p) for p in missing)
            + '. It is retail-derived generated output and is not tracked in git, so a '
              f'fresh checkout has none until it is generated; {prov.REMEDY}.')

    tool = gen_shaders.find_host_tool(gen_shaders.TOOL_BUILD_DIR)
    if tool is None:
        # NOT a refusal.  The tool is needed only to record `tool.sha256`, which is
        # advisory and never decides: the corpus is current if its SOURCE hashes,
        # dxc version/args and counts match, and the corpus survives deleting
        # `build/`.  Refusing here would reject a valid tree on a clean rebuild.
        print('corpus   : advisory: the nv2a_vsh_hlsl host tool was not found (searched '
              + ', '.join(str(p) for p in gen_shaders.HOST_TOOL_CANDIDATES)
              + '), so its hash cannot be re-recorded. The tool hash is advisory, so this '
                'is not a refusal; build it with '
                f'`{gen_shaders.REBUILD_HINT}` if you want it recorded.')
    if tool is None and os.environ.get('JSRF_NV2A_VSH_HLSL'):
        # An override that does not exist is still worth saying out loud: the caller
        # named a tool, and the corpus cannot be compared against it.
        print(f'corpus   : advisory: JSRF_NV2A_VSH_HLSL='
              f'{os.environ["JSRF_NV2A_VSH_HLSL"]} does not exist, so that tool was not '
              f'used for the advisory hash.')

    try:
        want = prov.expected(gen_shaders.TOOLKIT, tool, corpus_dxc(),
                             programs=gen_shaders.EXPECT_PROGRAMS,
                             distinct=gen_shaders.EXPECT_DISTINCT)
    except prov.ProvenanceError as error:
        raise SystemExit(f'the corpus provenance cannot be re-derived: {error}. '
                         f'Regenerate -- {prov.REMEDY} -- once the toolchain is back.')

    # ONE production sequence, in the schema module, so the fixtures exercise the
    # guard the build actually runs rather than a private copy of it.
    _, problems, advisories = prov.check_corpus(CORPUS_DIR, want)

    for advisory in advisories:
        print(f'corpus   : advisory: {advisory}')
    if problems:
        detail = '\n'.join(f'  - {problem}' for problem in problems)
        raise SystemExit(
            f'The NV2A vertex-program corpus at {CORPUS_DIR} is STALE: the inputs that '
            f'produced it are not the inputs this tree has now.\n'
            f'{detail}\n'
            f'The corpus is retail-derived generated output; do not edit it. Regenerate '
            f'it -- {prov.REMEDY} -- and rebuild.')

    summary = (f'schema {want["schema"]}, {want["programs"]} programs, '
               f'{want["distinct"]} distinct')
    if tool is not None and 'tool.sha256' in want:
        summary += f', tool {tool} sha256 {want["tool.sha256"]}'
    else:
        summary += ', tool hash not recorded (no host tool was found; it is advisory)'
    return summary


# ── preflight ────────────────────────────────────────────────────────────────
report = jsrf_build.preflight(args.parallel)
print(f'python   : {report["python"]} ({report["python_version"]})')
print(f'cmake    : {report["cmake"]}')
print(f'ctest    : {report["ctest"]}')
print(f'toolkit  : {report["toolkit"]}')
print(f'parallel : {report["parallel"]}')
if report['duplicate_case_keys']:
    print(f'environment: collapsed duplicate-case keys {report["duplicate_case_keys"]}')
if not report['ok']:
    for problem in report['problems']:
        print(f'PREFLIGHT FAILED: {problem}')
    raise SystemExit('Preflight failed; nothing was built or regenerated.')

# BEFORE configure: the corpus is what the build compiles, and the configure step
# is the first thing that mutates anything.  A stale corpus must never get that far.
print(f'corpus   : {check_corpus_provenance()}')

if not args.allow_regeneration:
    # Recovery regeneration can erase current ABI instrumentation, so it is off
    # unless explicitly asked for.  This is NOT about the corpus provenance guard
    # above: that one re-derives the generated shader corpus and refuses a stale
    # one, while this switch controls `scripts/recover-functions.py` rewriting
    # src/recomp/gen.  The pinned generated baseline is what gets compiled, and
    # `scripts/check-generation-provenance.py --check` is what checks it.
    print('regeneration: disabled (pass --allow-regeneration to permit it; it can '
          'erase current ABI instrumentation)')

if not run(['cmake', '-S', '.', '-B', 'build'], 'configure-current.log'):
    raise SystemExit('CMake configure failed; see logs/configure-current.log.')

if args.allow_regeneration:
    for script, message in (
            ('scripts/recover-functions.py',
             'Recovery generation failed; refusing to compile older generated output.'),
            ('scripts/generate-lifter-tests.py',
             'Lifter regression generation failed.')):
        if not run([python, '-X', 'utf8', script], f'{Path(script).stem}-current.log'):
            raise SystemExit(message)

# Inventory AFTER configure, because configure is what writes CTestTestfile.cmake.
try:
    targets, derived = build_targets()
    inventory = jsrf_build.write_inventory()
except jsrf_build.BuildError as error:
    raise SystemExit(f'Target inventory could not be derived: {error}')
print(f'targets  : {len(targets)} derived from CTest ({derived["test_count"]} tests)')
print(f'inventory: {jsrf_build.INVENTORY_PATH.relative_to(root)}')

if not run([python, 'scripts/build-identity.py', 'before'], 'build-identity-before.log'):
    raise SystemExit('Source fingerprint failed.')

ok, outcome = run_build(targets, args.parallel, 'build-current.log')
if not ok:
    raise SystemExit('Build failed. Game was not launched.')

if not run([python, 'scripts/build-identity.py', 'after'], 'build-identity-after.log'):
    raise SystemExit('Build identity validation failed.')

print(f'Build succeeded ({outcome}); source/executable identity recorded. '
      f'See logs/build-current.log.')
