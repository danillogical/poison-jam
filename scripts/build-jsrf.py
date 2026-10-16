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

The shader corpus is OPTIONAL, and this guard decides which side it is on: it asks the
toolkit for its Vulkan capability with a fresh probe (`scripts/cmake/vulkan-capability`)
and branches on that. With Vulkan and its shader tools the retail corpus is required and
its provenance is validated before anything is configured; without them the build compiles
the generated empty corpus and there is nothing to validate. `--check-corpus-only` is the
entry the game's CMakeLists.txt calls from inside its own configure, so a real corpus
cannot reach the compiler unvalidated -- see `scripts/cmake/vulkan-capability/CMakeLists.txt`
and `xboxrecomp/cmake/XbrVulkan.cmake` for the bound decision itself.
"""
from pathlib import Path
import argparse
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
logs = root / 'logs'
logs.mkdir(exist_ok=True)
env = dict(os.environ)
python = sys.executable

sys.path.insert(0, str(root / 'scripts'))
import jsrf_build  # noqa: E402

# The provenance schema is shared with the generator that records it; load it by
# path because `vsh_aot_provenance.py` is a module in name only (it is imported by
# two scripts, not installed).  ONE instance per process: when this file is not the
# first loader, reuse what is there, so `prov.ProvenanceError` is ONE class and a
# refusal raised by a `gen_shaders` call is caught by the `except` clauses below
# (a second copy would define a second class, and the refusal would escape as a
# traceback).
def _load_prov():
    importlib_path = root / 'scripts' / 'vsh_aot_provenance.py'
    loaded = sys.modules.get('vsh_aot_provenance')
    if (loaded is not None and getattr(loaded, '__file__', None)
            and os.path.normcase(os.path.realpath(str(loaded.__file__)))
            == os.path.normcase(os.path.realpath(str(importlib_path)))):
        return loaded
    spec = importlib.util.spec_from_file_location('vsh_aot_provenance', importlib_path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['vsh_aot_provenance'] = mod
    spec.loader.exec_module(mod)
    return mod


prov = _load_prov()

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
parser.add_argument('--check-corpus-only', action='store_true',
                    help='answer the corpus question and exit: ask the toolkit for its Vulkan '
                         'capability with a fresh probe, then run the corpus provenance guard '
                         'when the capability is available (or when --expect-capability-on '
                         'says the caller already decided it is) and print the summary. No '
                         'preflight, no configure, no build, no write to the corpus. This is '
                         'the entry the game CMakeLists.txt calls from inside its own '
                         'configure, so a real corpus cannot be compiled unvalidated.')
parser.add_argument('--expect-capability-on', action='store_true',
                    help='with --check-corpus-only: run the corpus guard UNCONDITIONALLY. The '
                         'caller has already decided the capability is on, so a disagreement '
                         'between this guard\'s probe and that decision can never SKIP the '
                         'validation.')
parser.add_argument('--toolkit', type=Path,
                    help='with --check-corpus-only: resolve the corpus against THIS toolkit '
                         'instead of the one this repository binds to (the build tree\'s cached '
                         'XBOXRECOMP_DIR, then $XBOXRECOMP_DIR, then the sibling checkout). It '
                         'must still be a real toolkit checkout, and it is the tree the '
                         'capability probe is asked about and the tree the guard names. The '
                         'game CMakeLists.txt passes its own ${XBOXRECOMP_DIR}, so a configure '
                         'of a SECOND build tree is validated against the toolkit THAT '
                         'configure links rather than the one another build tree cached.')
parser.add_argument('--probe-parent', type=Path,
                    help='with --check-corpus-only: create the capability probe\'s throwaway '
                         'build tree under this directory instead of '
                         '<game>/build/vulkan-capability. The game CMakeLists.txt passes '
                         '${CMAKE_BINARY_DIR}, so a configure of another build tree never '
                         'writes a probe tree into the build tree of the one beside it. The '
                         'directory is created when absent and this run\'s leaf under it is '
                         'removed afterwards, exactly as the default is.')
args = parser.parse_args()

if args.expect_capability_on and not args.check_corpus_only:
    raise SystemExit('--expect-capability-on is only meaningful with --check-corpus-only: '
                     'the build flow decides the capability itself, from its own probe.')

if (args.toolkit is not None or args.probe_parent is not None) and not args.check_corpus_only:
    raise SystemExit('--toolkit and --probe-parent are only meaningful with '
                     '--check-corpus-only: the build flow resolves the toolkit it is bound to '
                     '(the build tree\'s cache, then $XBOXRECOMP_DIR) and places its capability '
                     'probe under the build tree it configures.')

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


def resolve_bound_toolkit():
    """The ONE toolkit this build binds to, `(path, label)`; a conflict is a refusal.

    CMake links and includes the toolkit its build tree caches (`XBOXRECOMP_DIR` in
    `build/CMakeCache.txt`), while the generator and this guard read the sources
    they name.  When those are different trees, every check here is self-consistent
    and still describes SPIR-V nobody links, so the disagreement is a refusal rather
    than a preference -- and it happens BEFORE preflight, so the toolkit this run
    reports is the toolkit it will use.
    """
    try:
        return gen_shaders.resolve_toolkit()
    except prov.ProvenanceError as error:
        raise SystemExit(
            f'The toolkit this build binds to cannot be resolved: {error}\n'
            'The corpus guard, the shader generator and the CMake link must name ONE '
            'toolkit, so nothing is built or regenerated until they do.')


def named_toolkit(path):
    """The toolkit `--toolkit` names, as `(path, label)`; not a checkout is a refusal.

    The configure that calls `--check-corpus-only` already decided which toolkit it links
    (`-DXBOXRECOMP_DIR=...`, recorded in ITS cache), so resolving the toolkit through the MAIN
    build tree's cache instead -- the default -- can refuse a legitimate configure with a
    conflict that names a tree the caller never mentioned, and always answers about a tree
    the caller may not link.  This makes the caller's own toolkit authoritative.

    Authoritative is not unvalidated: the named tree must be a real toolkit checkout, by the
    SAME rule the guard and the generator apply to the toolkit they resolve themselves
    (`scripts/gen-shaders.py`'s `resolve_toolkit`): `CMakeLists.txt` and all three translator
    sources, because a toolkit that is merely named proves nothing about the code the corpus
    was built from.
    """
    root = Path(os.path.realpath(str(path)))
    missing = [p for p in [root / 'CMakeLists.txt', *prov.source_paths(root).values()]
               if not p.is_file()]
    if missing:
        raise SystemExit(
            f'--toolkit {root} is not a toolkit checkout: it is missing '
            + ', '.join(str(p) for p in missing)
            + f'. It must carry CMakeLists.txt and the three translator sources ('
            + ', '.join(prov.SOURCE_NAMES)
            + '), the same rule the guard and the generator apply to the toolkit they resolve '
              'for themselves; point --toolkit at the toolkit this configure links. Nothing is '
              'built or regenerated until it does.')
    return root, 'the --toolkit argument'


def check_corpus_provenance(toolkit=None, toolkit_from=None):
    """Refuse a corpus the current inputs have moved past; return a summary string.

    This runs BEFORE the CMake configure step, so a stale corpus never reaches the
    compiler -- and the corpus is retail-derived generated output, so the refusal
    names the path and every differing field and then names `just shaders`. There is
    deliberately no bypass flag and no auto-regeneration: a guard that could be
    waved through would not be a guard, and regenerating under a failing build is
    how the wrong half of a pipeline gets rewritten.

    The checks run in this order, and NONE of them mutates anything:

      1. the corpus files exist (they are what this build compiles);
      2. the toolkit is resolved -- one tree for the guard, the generator and the
         CMake link (`resolve_bound_toolkit`; the caller passes the same pair it
         gave preflight, so there is exactly one resolution per run);
      3. the OWNED tool tree (`build/vsh-tool`) is bound to that same toolkit.  Its
         own cache records the toolkit it was configured against
         (`CMAKE_HOME_DIRECTORY`), and its build system compiles that tree -- so a
         tree bootstrapped against another checkout is a refusal before anything is
         accepted, because the artifact it would rebuild is exactly the binary whose
         hash the corpus records;
      4. the generating tool is found.  It is the owned artifact
         (`build/vsh-tool`) or an explicit `JSRF_NV2A_VSH_HLSL` override, and
         `tool.sha256` is a DECIDING field, so "no tool" is a refusal here -- with
         no tool there is nothing to compare the corpus's tool identity against,
         and accepting the corpus would be accepting an unverified identity;
      5. the tool's freshness, through `gen_shaders.staleness_refusal` -- the ONE
         rule the generator's `--check` applies too: the corpus's recorded source
         CONTENT must match the sources on disk (a moved source means the tool that
         produced this corpus predates the content it names), and an ENVIRONMENT
         OVERRIDE must also be newer than every source -- it is the one binary whose
         freshness cannot be re-derived here, because this script must never rebuild
         or replace a path that came from the environment.  The owned artifact's
         mtime is NOT a refusal on its own: the generator refuses to record sources
         its tool is older than, and an owned artifact whose recorded source content
         still matches has only had its sources' mtimes moved since (a checkout, a
         copy, an `rsync --times`) -- the tolerated case is newer mtimes with
         IDENTICAL content, not a rebuilt executable, which would have a different
         hash and be refused by `tool.sha256` already;
      6. the expectation is derived from those inputs (`prov.expected`), which
         requires the same existing tool again;
      7. `prov.check_corpus` compares field by field and every difference is a
         refusal.
    """
    missing = [CORPUS_DIR / name for name in CORPUS_FILES if not (CORPUS_DIR / name).is_file()]
    if missing:
        raise SystemExit(
            'The NV2A vertex-program corpus is missing: '
            + ', '.join(str(p) for p in missing)
            + '. It is retail-derived generated output and is not tracked in git, so a '
              f'fresh checkout has none until it is generated; {prov.REMEDY}.')

    if toolkit is None:
        toolkit, toolkit_from = resolve_bound_toolkit()

    conflict = gen_shaders.toolkit_binding_conflict(toolkit, gen_shaders.TOOL_BUILD_DIR,
                                                    toolkit_from)
    if conflict:
        raise SystemExit(conflict + ' Nothing is built or regenerated until the owned tool '
                                    'tree and the toolkit this build resolves name the same '
                                    'checkout.')

    tool = gen_shaders.find_host_tool(gen_shaders.TOOL_BUILD_DIR)
    if tool is None:
        # No decision to make: a corpus records the executable that produced it, and
        # that record is decisive, so this guard cannot pass a corpus whose tool it
        # cannot hash.  (A schema-1 corpus in the same state used to pass here with an
        # advisory -- the defect this refusal replaces.)
        override = os.environ.get('JSRF_NV2A_VSH_HLSL')
        if override:
            state = (f'JSRF_NV2A_VSH_HLSL={override} was set, but that is not an existing '
                     f'file, and a path named by the environment is never ignored in favour '
                     f'of another tool')
        else:
            state = ('no JSRF_NV2A_VSH_HLSL override was set, and the tool it would fall '
                     'back to is not built')
        raise SystemExit(
            f'The executable that produced the NV2A vertex-program corpus cannot be found: '
            f'{state}. The tool this repository owns is the nv2a_vsh_hlsl.exe under '
            f'{gen_shaders.TOOL_BUILD_DIR} (build/vsh-tool, either CMake layout), and a '
            f'corpus records which tool generated it as a DECIDING field, so the build '
            f'refuses a corpus it cannot compare against a tool. Regenerate with '
            f'`just shaders`, which builds and records the owned tool'
            + ('; unset JSRF_NV2A_VSH_HLSL first, so the owned tool is the one that runs.'
               if override else '.'))

    # The freshness rule is `gen_shaders.staleness_refusal` -- the same one the
    # generator's `--check` applies -- so the two can never disagree about whether a
    # tool may stand for the sources the corpus records.
    try:
        refusal = gen_shaders.staleness_refusal(tool, toolkit)
    except prov.ProvenanceError as error:
        # `tool_staleness` reads the translator sources; one vanishing between the
        # resolution above and this check is an unreadable provenance input, not a
        # traceback.  Every refusal before configure names the same remedy.
        names = ', '.join(prov.SOURCE_NAMES)
        raise SystemExit(
            f'The freshness of the executable that produced the NV2A vertex-program corpus '
            f'({tool}) cannot be established: {error}. The corpus records the sources that '
            f'produced it ({names}), so they must be readable from the toolkit {toolkit} '
            f'({toolkit_from}) before the corpus can be accepted; restore the toolkit, then '
            f'{prov.REMEDY} and rebuild.')
    if refusal is not None and refusal[0] == 'content':
        changed = refusal[1]
        detail = '\n'.join(
            f'  - {name}: the corpus records {recorded!r}, {toolkit} has {now!r}'
            for name, (recorded, now) in sorted(changed.items()))
        override = os.environ.get('JSRF_NV2A_VSH_HLSL')
        # The content trigger compares the sources the EXISTING corpus RECORDED with
        # the sources on disk and never looks at the tool, so when an override is set
        # the honest remedy is to unset it: rebuilding it cannot clear this, and
        # `just shaders` can only rebuild the owned tool once the override is gone.
        remedy = (f' JSRF_NV2A_VSH_HLSL={override} is set: rebuilding that executable cannot '
                  f'clear this trigger, because what differs is the content the corpus records '
                  f'against the sources on disk. Unset JSRF_NV2A_VSH_HLSL, {prov.REMEDY} -- '
                  f'which rebuilds the tool this repository owns from the current sources and '
                  f'records them -- and rebuild; the override may be set again afterwards.'
                  if override else '')
        raise SystemExit(
            f'The NV2A vertex-program corpus records different SOURCE content than the '
            f'toolkit {toolkit} ({toolkit_from}) has now:\n{detail}\n'
            f'A source that moved since the last generation means the tool that produced '
            f'this corpus ({tool}) predates the content the corpus names. The corpus is '
            f'retail-derived generated output; do not edit it. Regenerate it -- '
            f'{prov.REMEDY} -- and rebuild.' + remedy)
    if refusal is not None:
        newer = refusal[1]
        raise SystemExit(
            f'JSRF_NV2A_VSH_HLSL={os.environ["JSRF_NV2A_VSH_HLSL"]} is older than '
            + ', '.join(str(p) for _, p in newer)
            + f', so it cannot be shown to have been built from the sources the corpus '
              f'records and it cannot be rebuilt here (a path named by the environment is '
              f'never overwritten or replaced by this build). Rebuild that executable '
              f'yourself, or unset JSRF_NV2A_VSH_HLSL so {prov.REMEDY} generates with the '
              f'tool this repository owns.')
    # An owned artifact with mtime-only staleness is deliberately NOT refused here
    # (see step 5 above): the tolerated state is translator sources whose mtimes are
    # newer with IDENTICAL content -- their bytes have not moved, so nothing was built
    # from different sources.  A rebuilt (or relinked) executable is not that case: it
    # has a different hash, and `tool.sha256` refuses it below.

    try:
        want = prov.expected(toolkit, tool, corpus_dxc(),
                             programs=gen_shaders.EXPECT_PROGRAMS,
                             distinct=gen_shaders.EXPECT_DISTINCT)
    except prov.ProvenanceError as error:
        raise SystemExit(f'the corpus provenance cannot be re-derived: {error}. '
                         f'Regenerate -- {prov.REMEDY} -- once the toolchain is back.')

    # ONE production sequence, in the schema module, so the fixtures exercise the
    # guard the build actually runs rather than a private copy of it.
    _, problems = prov.check_corpus(CORPUS_DIR, want)

    if problems:
        detail = '\n'.join(f'  - {problem}' for problem in problems)
        raise SystemExit(
            f'The NV2A vertex-program corpus at {CORPUS_DIR} is STALE: the inputs that '
            f'produced it are not the inputs this tree has now.\n'
            f'{detail}\n'
            f'The corpus is retail-derived generated output; do not edit it. Regenerate '
            f'it -- {prov.REMEDY} -- and rebuild.')

    return (f'schema {want["schema"]}, {want["programs"]} programs, '
            f'{want["distinct"]} distinct, tool {tool} sha256 {want["tool.sha256"]}')


# ── the Vulkan capability, asked freshly on every run ────────────────────────
# `scripts/cmake/vulkan-capability/CMakeLists.txt` is a trivial `project(... NONE)` tree
# that `include()`s the toolkit's `cmake/XbrVulkan.cmake` and reports the verdict that file
# decides.  Configuring it in a FRESH directory is what makes the answer THIS run's: the
# game build tree's own cached `XBR_VULKAN_FOUND` is a statement about an earlier
# configure, and a stale truth value must never decide which corpus a build compiles.
#
# A project rather than `cmake -P`: `find_package(Vulkan)` is not script-mode-safe
# (CMake's FindVulkan calls `add_library`), while a `project(... NONE)` tree needs no
# compiler and needs nothing built.
PROBE_PROJECT = root / 'scripts' / 'cmake' / 'vulkan-capability'
# The probe's throwaway build trees, under the game's OWN build output (never the source
# tree, never the corpus).  This parent is SHARED with any other run in this tree, so
# nothing here ever deletes or reuses it: every run gets a fresh leaf under it (`mkdtemp`),
# and removes only that leaf.  A leaf of its own is also what keeps the answer fresh -- a
# new leaf cannot inherit a cache an earlier configure left behind.
PROBE_PARENT = gen_shaders.GAME_BUILD_DIR / 'vulkan-capability'
PROBE_LOG = logs / 'vulkan-capability-probe.log'
# The controls the game's own configure reads out of ITS cache.  Forwarding them is what
# keeps this guard and that configure from answering different questions: a build whose
# cache disables `find_package(Vulkan)`, or names the two tools, is asked about exactly
# those.  `XBR_VULKAN_FOUND` is deliberately NOT forwarded -- recomputing it is the whole
# point -- and `XBR_DXC`/`XBR_SPIRV_VAL` are forwarded as values, not trusted as verdicts:
# the toolkit checks the paths EXIST, so a cached toolchain that has since been deleted
# decides "unavailable" here just as it does in the configure.
CAPABILITY_FORWARDED = ('XBR_DXC', 'XBR_SPIRV_VAL', 'XBR_VK_SW_ICD',
                        'CMAKE_DISABLE_FIND_PACKAGE_Vulkan',
                        # The rest of the loader discovery, for the same reason: a configure
                        # that will see a cached `Vulkan_LIBRARY`, `Vulkan_INCLUDE_DIR`,
                        # `Vulkan_DIR` or `CMAKE_PREFIX_PATH` must be asked about THOSE, or the
                        # probe's fresh verdict can disagree with the configure that runs right
                        # after it -- and the contract asks for ONE consistent decision.
                        # Forwarded as cache VALUES, never trusted as verdicts: `find_package`
                        # re-searches when a cached path is not a real file, so a loader that
                        # has since been deleted decides "unavailable" in the probe exactly as
                        # it does in the configure.
                        'Vulkan_LIBRARY', 'Vulkan_INCLUDE_DIR', 'Vulkan_DIR',
                        'CMAKE_PREFIX_PATH')
# One CMakeCache entry of ANY type (`:PATH=`, `:FILEPATH=`, `:BOOL=`, and the
# `:UNINITIALIZED` an early configure failure leaves).  The name is anchored and must be
# followed by the type-or-`=` separator, so a longer variable cannot match.
_CACHE_ENTRY = r'^\s*{name}\s*(?::\s*[A-Za-z_][A-Za-z0-9_]*\s*)?=\s*(.*?)\s*$'
_TRUTHY = ('1', 'ON', 'TRUE', 'Y', 'YES')


def cache_value(path, name):
    """One CMakeCache entry's value, or None.

    A missing cache, a missing entry and an empty value all mean "this build tree has not
    spoken" -- the caller decides what that implies.
    """
    path = Path(path)
    if not path.is_file():
        return None
    match = re.search(_CACHE_ENTRY.format(name=re.escape(name)),
                      path.read_text(encoding='utf-8', errors='replace'), re.MULTILINE)
    if not match:
        return None
    value = match.group(1).strip()
    return value or None


def probe_message(output):
    """The failure message the probe project printed, collapsed to one line, or ''.

    CMake wraps a `message(FATAL_ERROR ...)` across lines and pads it with indentation, so
    the message is taken from the first `CMake Error`/`CMake Warning` line to the blank line
    that ends it, with its whitespace collapsed.
    """
    lines = (output or '').splitlines()
    for index, line in enumerate(lines):
        if not (line.startswith('CMake Error') or line.startswith('CMake Warning')):
            continue
        collected = []
        for following in lines[index + 1:]:
            stripped = following.strip()
            if not stripped or stripped.startswith('--') or stripped.startswith('CMake '):
                break
            collected.append(stripped)
        if collected:
            return ' '.join(' '.join(collected).split())
    return ''


def probe_capability(toolkit, probe_parent=None):
    """The Vulkan capability, from a FRESH configure of the shipped probe project.

    Returns `{asked, available, reason, loader, dxc, spirv_val}`.  `available` is the
    decision the guard acts on, and it is True when `asked` is False -- a toolkit that
    cannot answer the question at all (no probe project, no `cmake/XbrVulkan.cmake` to
    include, a probe that failed to configure) leaves the retail corpus REQUIRED, exactly
    as it was before the capability was decidable.  That is the conservative direction: it
    can only ever demand the VALIDATED retail corpus, and it can never accept an empty or
    unvalidated one.  It is also unreachable in a tree that can configure, because the
    toolkit's own `CMakeLists.txt` includes that same cmake file unconditionally.

    `probe_parent` is the directory the throwaway probe tree is created under (default
    `PROBE_PARENT`); `--probe-parent` passes the build tree of the configure that called
    this check, so a scratch configure does not write into another build tree.  A parent that
    cannot be created is the same undetermined case as a probe that cannot be configured --
    `available` stays True and the corpus stays required -- because failing OPEN there would
    let an unanswered question compile an unvalidated corpus.
    """
    output = ''
    probe_dir = None
    created_parent = False
    parent = Path(probe_parent) if probe_parent is not None else PROBE_PARENT
    reason = ''
    game_cache = gen_shaders.GAME_BUILD_DIR / gen_shaders.CACHE_NAME
    try:
        if not (PROBE_PROJECT / 'CMakeLists.txt').is_file():
            reason = f'the capability probe project {PROBE_PROJECT} is missing'
        else:
            try:
                if not parent.exists():
                    # Created only when absent, and removed again at the end only when this run
                    # was the one that made it (below): a parent another run already had is
                    # never deleted, and no run ever deletes the parent as a whole.
                    parent.mkdir(parents=True)
                    created_parent = True
                # A FRESH leaf of this run's own: `mkdtemp` cannot collide with a concurrent
                # run's, so neither can remove or overwrite the other's probe tree while it is
                # being configured.
                probe_dir = Path(tempfile.mkdtemp(dir=parent, prefix='run-'))
            except OSError as error:
                # Fail CLOSED, and without a traceback: an unwritable parent (a file in the
                # way, a read-only tree, no such drive) is a question this host could not ask,
                # not a reason to compile an unvalidated corpus.
                reason = (f'the capability probe tree could not be created under {parent} '
                          f'({error.__class__.__name__}: {error}), so the toolkit was never '
                          f'asked')
            if probe_dir is not None:
                argv = ['cmake', '-S', str(PROBE_PROJECT), '-B', str(probe_dir),
                        f'-DXBOXRECOMP_DIR={toolkit}']
                for name in CAPABILITY_FORWARDED:
                    value = cache_value(game_cache, name)
                    if value is not None:
                        argv.append(f'-D{name}={value}')
                try:
                    answered = subprocess.run(argv, cwd=root, env=env, capture_output=True,
                                              text=True)
                except OSError as error:
                    answered = None
                    reason = (f'cmake could not be started ({error}), so the toolkit was never '
                              f'asked')
                if answered is None:
                    return {'asked': False, 'available': True, 'reason': reason, 'loader': '',
                            'dxc': '', 'spirv_val': ''}
                output = (answered.stdout or '') + (answered.stderr or '')
                cache = probe_dir / gen_shaders.CACHE_NAME
                verdict = cache_value(cache, 'XBR_VULKAN_FOUND')
                if answered.returncode != 0:
                    reason = (probe_message(output)
                              or f'the capability probe configure exited {answered.returncode}')
                elif verdict is None:
                    reason = f'the capability probe recorded no XBR_VULKAN_FOUND in {cache}'
                else:
                    return {'asked': True,
                            'available': verdict.strip().upper() in _TRUTHY,
                            'reason': cache_value(cache, 'XBR_VULKAN_UNAVAILABLE_REASON') or '',
                            'loader': cache_value(cache, 'Vulkan_LIBRARY') or '',
                            'dxc': cache_value(cache, 'XBR_DXC') or '',
                            'spirv_val': cache_value(cache, 'XBR_SPIRV_VAL') or ''}
    finally:
        # Only this run's own leaf.  `ignore_errors` because a probe tree another process
        # still holds open must not turn a diagnostic cleanup into a failure.
        if probe_dir is not None:
            shutil.rmtree(probe_dir, ignore_errors=True)
        if created_parent:
            # Remove the parent again only when it is empty (`rmdir` refuses otherwise), so
            # a concurrent run's leaf keeps it alive.  It is a no-op when the parent was
            # already there before this run, which is the promise: never delete it out from
            # under anyone.
            try:
                parent.rmdir()
            except OSError:
                pass
        # The probe's own output is kept: a probe that could not answer is the one thing
        # here that needs a second look, and its output names the toolkit's own status line.
        # LAST WRITER WINS: two concurrent runs may each write it, and it is diagnostic
        # evidence with `capability:` on stdout as the authoritative statement.
        PROBE_LOG.parent.mkdir(parents=True, exist_ok=True)
        PROBE_LOG.write_text(output, encoding='utf-8', errors='replace')
    return {'asked': False, 'available': True, 'reason': reason, 'loader': '', 'dxc': '',
            'spirv_val': ''}


def capability_line(capability):
    """The ONE `capability:` line, so every build log shows the detection state.

    A capability that could not be DETERMINED is named as such rather than folded into
    either verdict: the corpus stays required, and pretending that is "Vulkan available"
    would hide the reason.
    """
    if not capability['asked']:
        return (f'capability: undetermined -- {capability["reason"]} '
                f'(see {PROBE_LOG.relative_to(root)}); the retail corpus stays required and '
                f'is validated as before')
    if capability['available']:
        return (f'capability: Vulkan available (loader {capability["loader"]}; '
                f'dxc {capability["dxc"]}; spirv-val {capability["spirv_val"]})')
    return f'capability: Vulkan unavailable ({capability["reason"]})'


def corpus_off_line(capability):
    """The `corpus   :` line of a build that compiles the generated empty corpus."""
    return (f'Vulkan unavailable ({capability["reason"]}) -- this build compiles the '
            f'generated empty corpus (nv2a_vsh_program_count = 0); the retail corpus is '
            f'not compiled')


def report_corpus_line(capability, retail_required):
    """Print the ONE `corpus   :` line, and refuse a corpus this build may not compile.

    THE single place either entry reports and checks the corpus, so the build flow and
    `--check-corpus-only` cannot drift apart.  `retail_required` is the caller's decision:
    the fresh capability, or the caller's own `--expect-capability-on`.  When it is True the
    R1 guard runs here -- BEFORE the configure step, which is the first thing that mutates
    anything, so a stale corpus never reaches the compiler -- and refuses when the corpus is
    missing or the inputs that produced it have moved.  When it is False there is no retail
    corpus to validate in this build: the line names the generated empty corpus that is
    compiled instead, because an empty table is not a validated retail corpus and "no
    corpus" must never be read as validated provenance.
    """
    if not retail_required:
        print(f'corpus   : {corpus_off_line(capability)}')
        return
    print(f'corpus   : {check_corpus_provenance(BOUND_TOOLKIT, BOUND_TOOLKIT_FROM)}')


# ── preflight ────────────────────────────────────────────────────────────────
# The toolkit is resolved ONCE, before anything is printed or touched: it is what
# CMake links (via the build tree's cache), what the generator reads, and what the
# guard compares against, so the `toolkit  :` line below reports the bound tree
# rather than a third path that could silently disagree with both.  `--toolkit` names
# that tree explicitly, for the ONE caller that already knows which toolkit it links (the
# configure that called `--check-corpus-only`): resolving through the MAIN build tree's
# cache instead would answer about -- and refuse over -- a toolkit that configure never
# named, and could write a probe tree into a build/ it does not own.
if args.toolkit is not None:
    BOUND_TOOLKIT, BOUND_TOOLKIT_FROM = named_toolkit(args.toolkit)
else:
    BOUND_TOOLKIT, BOUND_TOOLKIT_FROM = resolve_bound_toolkit()

if args.check_corpus_only:
    # The entry the game's CMakeLists.txt calls from inside its capability-ON configure, and
    # the same question a caller can ask by hand.  It answers and exits: no preflight, no
    # configure of the game, no build, and no write to the corpus.  `--expect-capability-on`
    # runs the corpus guard UNCONDITIONALLY, because the caller has already decided the
    # capability is on -- so a disagreement between this probe and that decision can never
    # SKIP the validation.  `--toolkit` and `--probe-parent` bind the check to the calling
    # configure's OWN toolkit and build tree, so a second build tree neither inherits a
    # conflict from `build/`'s cache nor places its probe there.  The toolkit is REPORTED
    # here for the reason the full flow reports it: a check that validated against one tree
    # while the caller links another would be internally consistent and describe the wrong
    # SPIR-V, so which tree was used -- and how it was named -- is part of the answer.
    print(f'toolkit  : {BOUND_TOOLKIT} ({BOUND_TOOLKIT_FROM})')
    CHECK_CAPABILITY = probe_capability(BOUND_TOOLKIT, args.probe_parent)
    print(capability_line(CHECK_CAPABILITY))
    report_corpus_line(CHECK_CAPABILITY,
                       CHECK_CAPABILITY['available'] or args.expect_capability_on)
    raise SystemExit(0)

report = jsrf_build.preflight(args.parallel, toolkit=(BOUND_TOOLKIT, BOUND_TOOLKIT_FROM))
print(f'python   : {report["python"]} ({report["python_version"]})')
print(f'cmake    : {report["cmake"]}')
print(f'ctest    : {report["ctest"]}')
print(f'toolkit  : {report["toolkit"]} ({report["toolkit_source"]})')
print(f'parallel : {report["parallel"]}')
if report['duplicate_case_keys']:
    print(f'environment: collapsed duplicate-case keys {report["duplicate_case_keys"]}')
if not report['ok']:
    for problem in report['problems']:
        print(f'PREFLIGHT FAILED: {problem}')
    raise SystemExit('Preflight failed; nothing was built or regenerated.')

# The capability this run acts on, asked of the toolkit ITSELF (see `probe_capability`), so
# the decision is fresh: no cached verdict of this repository's or the build tree's decides
# anything here.  What it decides is whether the retail corpus is required at all -- and the
# `corpus   :` line below is the ONE place that answers it.
CAPABILITY = probe_capability(BOUND_TOOLKIT)
print(capability_line(CAPABILITY))
report_corpus_line(CAPABILITY, CAPABILITY['available'])

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
