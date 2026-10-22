#!/usr/bin/env python3
"""G1: the shipped `JSRF_FIXTURES_ONLY=ON` configure, exercised through real CMake.

WHAT THIS PINS
==============
`option(JSRF_FIXTURES_ONLY ... OFF)` (game commit 401d8c5) must let a source tree
that contains ONLY tracked files -- no `src/recomp/gen`, no
`src/recomp/recovered`, no `game/`, no retail corpus, no archives -- configure
successfully with `-DJSRF_FIXTURES_ONLY=ON -DBUILD_TESTING=ON`, even where the
Vulkan capability is ON.  The configure must:

  * succeed, and its own STATUS line and cache must show that the generated
    empty vertex-program corpus under `<build>/empty-corpus` was selected
    (`nv2a_vsh_program_count = 0`), not the retail corpus;
  * write no build description that names `src/recomp/gen`,
    `src/recomp/recovered` or `game/`, and generate the empty table itself;
  * configure neither `jsrf_recomp` nor `jsrf_collect`;
  * leave the portable tests configured (`BUILD_TESTING=ON`);
  * wire the generated empty table into the REAL configured fixture target:
    `jsrf_vsh_conformance_test.vcxproj` must list exactly one `ClCompile`
    entry for it (no missing, duplicate or retail table), and the project's
    own include list must bind the generated table's directory when it
    declares one;
  * print the advertised fixtures-only exclusion groups
    (`jsrf_fence_release_runtime`, `jsrf_apu_watch_fixture`,
    `jsrf_collect_arming_gate_on/off`, `jsrf_a2h_slotw_fixture`,
    `jsrf_ac97_reset_gate`, `jsrf_native_gpu_warp`, and its own
    `jsrf_fixtures_only` reason) in the real configure output.

The evidence is never a re-implementation of the branch: it is a real
configure's exit code, its forwarded output, the cache it wrote, the files it
generated, the project it configured and a real compile+link+run of a tiny
consumer of the generated table with the host compiler.  The table path is
DERIVED from the fixture project's own `ClCompile` entry, not hard-coded, so a
configure that compiles some other file (or none) cannot pass.  The consumer
includes the generated header, checks `nv2a_vsh_program_count == 0` and
performs lookups that must all return NULL, so it proves the selected TABLE,
not file presence.

MUTATION CONTROLS (private copies only)
=======================================
The shared repository is never edited; every control is a throwaway copy of
the tracked scratch tree:

  * M-G1 (`CMakeLists.txt:111`, the capability branch): remove
    `AND NOT JSRF_FIXTURES_ONLY`.  The same gate must FAIL: the mutant drives a
    fixtures-only configure into the retail-corpus requirement.
  * negative control: turn the fixtures-only include/glob branch at the top of
    the file OFF (`if(JSRF_FIXTURES_ONLY)` -> `if(FALSE)`), so the configure
    starts reading the source tree's generated directory.  The gate must FAIL
    on a generated-tree reference in the build description.
  * M-CORPUS: clear the empty-corpus source
    (`set(JSRF_CORPUS_SOURCES "")`) while leaving the table generation and
    include dir intact.  A configure alone still succeeds and still prints the
    empty-corpus STATUS line, so only the fixture project's binding proof can
    reject it: the gate must FAIL with the named "missing corpus binding"
    assertion.
  * restore: with the pristine bytes written back, the gate must PASS again.

The shared `CMakeLists.txt` and `tests/test_optional_vulkan.py` hashes are
recorded before and after and must be identical.

EXIT STATUS
===========
0  the gate passed (baseline and restored pass; all three mutants fail)
1  a real gate failure (or a shared-source change)
77 only when this host genuinely lacks CMake, git, the toolkit or the Vulkan
   SDK (the reason is printed)

RUNNING
=======
`python -X utf8 tests/test_fixtures_only.py` (how CTest runs it).  Scratch trees
live under the system temp directory and are removed afterwards; on failure a
short text diagnostic is left there.  Nothing writes the repository, its
ignored generated trees, the retail corpus or the toolkit.
"""
from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = Path(os.environ.get('XBOXRECOMP_DIR') or (ROOT.parent / 'xboxrecomp'))
VULKAN_SDK = Path(os.environ.get('VULKAN_SDK') or r'C:\VulkanSDK\1.4.357.0')

SHARED_INPUTS = ('CMakeLists.txt', 'tests/test_optional_vulkan.py')
EXCLUDED_TRACKED = ('src/recomp/gen/', 'src/recomp/recovered/', 'game/')
FORBIDDEN_TREE_DIRS = ('src/recomp/gen', 'src/recomp/recovered', 'game')

GEN_NEEDLES = ('src/recomp/gen', 'src\\recomp\\gen',
               'src/recomp/recovered', 'src\\recomp\\recovered')
GAME_NEEDLES = ('game/default.xbe', 'game\\default.xbe')

# Line 111 is the ONLY `if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)` whose
# body begins with the corpus comment (line 676 is the Vulkan test registration
# block).  Anchoring on the following line therefore mutates line 111 alone.
LINE111_ANCHOR = ('if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)\n'
                  '    # The corpus, generated by')
LINE111_MUTANT = ('if(XBR_VULKAN_FOUND)\n'
                  '    # The corpus, generated by')

# The top-of-file fixtures-only branch that picks the toolkit template include
# dir and skips the generated glob.  `if(FALSE)` makes fixtures-only read the
# source tree's generated directory instead.
FIXTURES_ANCHOR = ('if(JSRF_FIXTURES_ONLY)\n'
                   '    set(RECOMP_GEN_SOURCES "")')
FIXTURES_MUTANT = ('if(FALSE)\n'
                   '    set(RECOMP_GEN_SOURCES "")')

# The empty-corpus branch's source variable.  Clearing it leaves the configure
# succeeding and the STATUS line untouched, so only the fixture project's own
# `ClCompile` binding can catch it.
CORPUS_ANCHOR = ('    set(JSRF_CORPUS_SOURCES "${_jsrf_empty_dir}/nv2a_vsh_programs.c")\n'
                 '    set(JSRF_CORPUS_INCLUDE_DIR "${_jsrf_empty_dir}")')
CORPUS_MUTANT = ('    set(JSRF_CORPUS_SOURCES "")\n'
                 '    set(JSRF_CORPUS_INCLUDE_DIR "${_jsrf_empty_dir}")')

# The real fixture target whose project must bind the empty corpus.
FIXTURE_PROJECT = 'jsrf_vsh_conformance_test.vcxproj'
CORPUS_BASENAME = 'nv2a_vsh_programs.c'

# The exclusion groups the fixtures-only configure must ADVERTISE in its own
# output (not merely in CMakeLists.txt), plus its reason for not registering
# this gate itself.
C3_GROUPS = ('jsrf_fence_release_runtime', 'jsrf_apu_watch_fixture',
             'jsrf_collect_arming_gate_on/off', 'jsrf_a2h_slotw_fixture',
             'jsrf_ac97_reset_gate', 'jsrf_native_gpu_warp')
C3_SELF = ('jsrf_fixtures_only itself is also not registered in this mode',
           'needs the Windows tool layout', 'would recurse')

CONSUMER_C = r'''
#include <stdint.h>
#include <stdio.h>

#include "nv2a_vsh_programs.h"

/* The corpus contract the header declares: `nv2a_vsh_program_count` entries,
 * scanned by identity.  An empty table can never match. */
static const Nv2aVshAotEntry *lookup(uint32_t hash)
{
    size_t i;
    for (i = 0; i < nv2a_vsh_program_count; i++)
        if (nv2a_vsh_programs[i].hash == hash)
            return &nv2a_vsh_programs[i];
    return NULL;
}

int main(void)
{
    const uint32_t probes[] = { 0u, 0x811c9dc5u, 0xffffffffu };
    size_t i;
    int found = 0;

    for (i = 0; i < sizeof probes / sizeof probes[0]; i++)
        if (lookup(probes[i]) != NULL)
            found++;

    printf("count=%zu found=%d\n", nv2a_vsh_program_count, found);
    return (nv2a_vsh_program_count == 0 && found == 0) ? 0 : 1;
}
'''

CONSUMER_CMAKE = '''\
cmake_minimum_required(VERSION 3.20)
project(jsrf_g1_consumer C)
add_executable(g1_consumer "${JSRF_TABLE_C}" "${JSRF_CONSUMER_C}")
target_include_directories(g1_consumer PRIVATE
    "${JSRF_EMPTY_DIR}" "${JSRF_TOOLKIT}/src/video")
'''


class GateFailure(RuntimeError):
    """A real failure of the gate (exit 1), never a skip."""


def fail(message):
    raise GateFailure(message)


# ── truthful observables ─────────────────────────────────────────────────────
def read_exact(path):
    with open(path, 'r', encoding='utf-8', newline='') as handle:
        return handle.read()


def write_exact(path, text):
    with open(path, 'w', encoding='utf-8', newline='') as handle:
        handle.write(text)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def flattened(text):
    return ' '.join((text or '').split())


def norm_path(text):
    return text.replace('\\', '/').casefold()


def tail(text, limit=4000):
    text = text or ''
    return text[-limit:]


def run(command, cwd=None, timeout=900):
    """Run a real command; a missing executable is a gate failure, not a skip."""
    try:
        return subprocess.run(
            command, cwd=str(cwd) if cwd else None, env=configure_env(),
            capture_output=True, text=True, encoding='utf-8', errors='replace',
            timeout=timeout)
    except FileNotFoundError as exc:
        fail(f'cannot execute {command[0]!r}: {exc}')
    except subprocess.TimeoutExpired as exc:
        fail(f'{command[0]!r} timed out after {timeout}s: {exc}')


def configure_env():
    """The environment a configure and the consumer build run in.

    The toolkit and SDK are named explicitly, and the variables that could
    silently decide the capability or redirect the files under test are
    removed, so each case's inputs are its own.
    """
    scrub = ('XBOXRECOMP_DIR', 'VULKAN_SDK', 'JSRF_NV2A_VSH_HLSL',
             'XBR_VK_SW_ICD', 'XBR_DXC', 'XBR_SPIRV_VAL',
             'CMAKE_DISABLE_FIND_PACKAGE_Vulkan', 'CMAKE_PREFIX_PATH',
             'Vulkan_ROOT', 'JSRF_R2_GAME_CMAKE', 'JSRF_R2_BUILD_SCRIPT',
             'JSRF_FIXTURES_ONLY', 'CMAKE_GENERATOR', 'CMAKE_GENERATOR_PLATFORM')
    scrub_upper = {name.upper() for name in scrub}
    env = {key: value for key, value in os.environ.items()
           if key.upper() not in scrub_upper}
    env['XBOXRECOMP_DIR'] = str(TOOLKIT)
    env['VULKAN_SDK'] = str(VULKAN_SDK)
    return env


def files_mentioning(folder, needles, skip=()):
    """Every non-binary file under `folder` whose BYTES contain a needle."""
    skip = tuple(Path(path) for path in skip)
    binary = ('.obj', '.pdb', '.exe', '.dll', '.lib', '.ilk', '.res', '.tlog',
              '.iobj', '.ipdb', '.map', '.spv')
    patterns = [needle.encode('utf-8') for needle in needles]
    patterns += [needle.encode('utf-16-le') for needle in needles]
    hits = []
    for path in sorted(folder.rglob('*')):
        if not path.is_file():
            continue
        if any(path == item or item in path.parents for item in skip):
            continue
        if path.suffix.lower() in binary:
            continue
        try:
            raw = path.read_bytes()
        except OSError:
            continue
        if any(pattern in raw for pattern in patterns):
            hits.append(path)
    return hits


def missing_prerequisites():
    """What this host genuinely lacks, so the configure cannot be exercised.

    Only CMake itself and the Vulkan SDK the capability-ON fixtures-only path is
    written for are skips; a missing git, toolkit or source file is a real
    failure of the tree under test, not a reason to skip.
    """
    problems = []
    if shutil.which('cmake') is None:
        problems.append('cmake is not on PATH')
    if not (VULKAN_SDK / 'Bin' / 'dxc.exe').is_file():
        problems.append(f'{VULKAN_SDK}/Bin/dxc.exe')
    if not (VULKAN_SDK / 'Bin' / 'spirv-val.exe').is_file():
        problems.append(f'{VULKAN_SDK}/Bin/spirv-val.exe')
    return problems


def missing_gate_inputs():
    """A missing input the gate must FAIL on, because the host is the target."""
    problems = []
    if shutil.which('git') is None:
        problems.append('git is not on PATH')
    if not (TOOLKIT / 'cmake' / 'XbrVulkan.cmake').is_file():
        problems.append(f'{TOOLKIT}/cmake/XbrVulkan.cmake')
    if not (TOOLKIT / 'src' / 'video' / 'nv2a_vsh_aot.h').is_file():
        problems.append(f'{TOOLKIT}/src/video/nv2a_vsh_aot.h')
    return problems


# ── the scratch tree: TRACKED files only ─────────────────────────────────────
def tracked_paths():
    completed = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '-z'],
                               capture_output=True)
    if completed.returncode != 0:
        fail(f'git ls-files failed: '
             f'{completed.stderr.decode("utf-8", "replace")[-2000:]}')
    names = [os.fsdecode(name) for name in completed.stdout.split(b'\0') if name]
    if not names:
        fail(f'git ls-files listed no tracked file under {ROOT}')
    for name in names:
        posix = name.replace('\\', '/')
        if posix.startswith(EXCLUDED_TRACKED):
            fail(f'a tracked path is excluded from the fixtures-only tree: {name}')
        if posix.lower().endswith('.xbe'):
            fail(f'the retail executable is tracked and must not be copied: {name}')
    return names


def make_tree(destination, tracked):
    destination.mkdir(parents=True)
    for relative in tracked:
        source = ROOT / relative
        target = destination / relative
        if not source.is_file():
            fail(f'tracked file is missing from the working tree: {relative}')
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    for relative in FORBIDDEN_TREE_DIRS:
        if (destination / relative).exists():
            fail(f'the scratch tree contains {relative}, which fixtures-only '
                 'must configure without')
    if list(destination.rglob('*.xbe')):
        fail('the scratch tree contains an XBE')
    return destination


def mutate(tree, anchor, replacement, what):
    target = tree / 'CMakeLists.txt'
    text = read_exact(target)
    # The anchor is written with '\n'; CMakeLists.txt may be checked out with
    # CRLF, so match (and replace) using the file's own line ending.
    newline = '\r\n' if '\r\n' in text else '\n'
    old = anchor.replace('\n', newline)
    new = replacement.replace('\n', newline)
    count = text.count(old)
    if count != 1:
        fail(f'the {what} anchor occurs {count} times in {target}; the mutant '
             'cannot be applied unambiguously')
    write_exact(target, text.replace(old, new))
    if read_exact(target) == read_exact(ROOT / 'CMakeLists.txt'):
        fail(f'the {what} mutant is byte-identical to the shipped file')


# ── the configured fixture project: the corpus BINDING ──────────────────────
def resolve_project_path(project, value):
    """Resolve a vcxproj `Include=`/include-list entry against the project."""
    raw = (value or '').strip().strip('"')
    if not raw or raw.startswith('%(', 0):
        return None
    candidate = Path(raw)
    if not candidate.is_absolute():
        candidate = project.parent / candidate
    return Path(os.path.normpath(str(candidate)))


def fixture_corpus_binding(project, table_c, label):
    """Prove the configured fixture project compiles the generated empty table.

    The `ClCompile` list is the project's own binding: exactly one entry must
    resolve to `table_c`, no other `nv2a_vsh_programs.c` may appear (a retail
    table would be the wrong binding), and the project's include list must bind
    `table_c`'s directory where it declares one.  Returns the resolved path, so
    the consumer is compiled against what the fixture project actually builds.
    """
    text = read_exact(project)
    entries = re.findall(r'<ClCompile\s+Include="([^"]+)"', text)
    table_key = norm_path(str(table_c))
    corpus = []
    wrong = []
    for entry in entries:
        resolved = resolve_project_path(project, entry)
        if resolved is None:
            continue
        if norm_path(str(resolved)) == table_key:
            corpus.append(resolved)
        elif resolved.name.casefold() == CORPUS_BASENAME:
            wrong.append(str(resolved))
    if wrong:
        fail(f'{label}: wrong corpus binding: {project.name} compiles '
             f'{", ".join(sorted(set(wrong)))} instead of the generated empty '
             f'table {table_c}')
    if not corpus:
        names = ', '.join(sorted({Path(entry).name for entry in entries})) or 'none'
        fail(f'{label}: missing corpus binding: {project.name} has no ClCompile '
             f'entry for the generated empty corpus {table_c} '
             f'(ClCompile entries: {names})')
    if len(corpus) > 1:
        fail(f'{label}: duplicate corpus binding: {project.name} lists the '
             f'generated empty corpus {len(corpus)} times '
             f'({", ".join(str(path) for path in corpus)})')
    corpus_path = corpus[0]
    if not corpus_path.is_file():
        fail(f'{label}: the fixture project binds {corpus_path}, which does not '
             'exist')
    try:
        identical = corpus_path.samefile(table_c)
    except OSError as exc:
        fail(f'{label}: cannot compare {corpus_path} with {table_c}: {exc}')
    if not identical:
        fail(f'{label}: wrong corpus binding: {corpus_path} is not the '
             f'generated empty table {table_c}')

    include_lists = re.findall(
        r'<AdditionalIncludeDirectories>([^<]*)</AdditionalIncludeDirectories>',
        text)
    if include_lists:
        table_dir = norm_path(str(table_c.parent))
        unbound = []
        for index, value in enumerate(include_lists):
            if not value.strip():
                continue
            resolved = [resolve_project_path(project, part)
                        for part in value.split(';')]
            if not any(resolved_path is not None
                       and norm_path(str(resolved_path)) == table_dir
                       for resolved_path in resolved):
                unbound.append(index)
        if unbound:
            fail(f'{label}: the fixture project does not bind the generated '
                 f'empty-corpus include directory {table_c.parent} in its '
                 f'AdditionalIncludeDirectories ({len(unbound)} of '
                 f'{len(include_lists)} list(s): {unbound}); the bound table '
                 'header may not be includable')
    else:
        print(f'{label}: note: {project.name} declares no '
              'AdditionalIncludeDirectories; the empty-corpus include comes '
              'from the target')
    return corpus_path


# ── the gate ────────────────────────────────────────────────────────────────
def run_gate(tree, build_name, label):
    build = tree / build_name
    completed = run(['cmake', '-S', str(tree), '-B', str(build),
                     '-DJSRF_FIXTURES_ONLY=ON', '-DBUILD_TESTING=ON',
                     f'-DXBOXRECOMP_DIR={TOOLKIT}'], cwd=tree)
    text = flattened((completed.stdout or '') + (completed.stderr or ''))
    if completed.returncode != 0:
        fail(f'{label}: the fixtures-only configure failed (rc='
             f'{completed.returncode}); a fixtures-only tree must configure '
             f'without generated code or game data. Configure tail:\n'
             f'{tail(text)}')

    cache_path = build / 'CMakeCache.txt'
    if not cache_path.is_file():
        fail(f'{label}: the configure wrote no CMakeCache.txt')
    cache = read_exact(cache_path)
    for entry in ('JSRF_FIXTURES_ONLY:BOOL=ON', 'XBR_VULKAN_FOUND:BOOL=TRUE'):
        if entry not in cache:
            fail(f'{label}: the cache does not hold {entry}; this gate must run '
                 'the fixtures-only axis with the capability ON')
    if not re.search(r'^\s*BUILD_TESTING\s*:\s*BOOL\s*=\s*ON\s*$', cache,
                     re.MULTILINE):
        fail(f'{label}: BUILD_TESTING is not ON in the configure cache')

    # The configure's own status: the empty corpus at <build>/empty-corpus, count 0.
    for needle in ('JSRF_FIXTURES_ONLY: Vulkan is available',
                   'generated empty vertex-program corpus at',
                   '(nv2a_vsh_program_count = 0)'):
        if needle not in text:
            fail(f'{label}: the configure did not state {needle!r}; output:\n'
                 f'{tail(text)}')
    if norm_path(str(build / 'empty-corpus')) not in norm_path(text):
        fail(f'{label}: the configure did not name {build / "empty-corpus"} '
             f'as the selected empty corpus; output:\n{tail(text)}')
    if 'retail NV2A vertex-program corpus is required' in text:
        fail(f'{label}: the configure took the capability-ON retail-corpus '
             f'branch; output:\n{tail(text)}')

    # C3: the configure's own output must ADVERTISE every exclusion group it
    # implements, and its reason for not registering this gate in this mode.
    missing_groups = [group for group in C3_GROUPS if group not in text]
    if missing_groups:
        fail(f'{label}: the configure output does not advertise the '
             f'fixtures-only exclusion group(s) {missing_groups}; output:\n'
             f'{tail(text)}')
    missing_self = [needle for needle in C3_SELF if needle not in text]
    if missing_self:
        fail(f'{label}: the configure output does not state the jsrf_fixtures_only '
             f'self-exclusion reason ({missing_self}); output:\n{tail(text)}')

    # The generated table itself, in BUILD output, declaring zero programs.
    table_c = build / 'empty-corpus' / 'nv2a_vsh_programs.c'
    table_h = build / 'empty-corpus' / 'nv2a_vsh_programs.h'
    for path in (table_c, table_h):
        if not path.is_file():
            fail(f'{label}: {path} was not generated')
    if 'const size_t nv2a_vsh_program_count = 0;' not in read_exact(table_c):
        fail(f'{label}: the generated table does not declare program count 0')
    header = read_exact(table_h)
    for declaration in ('extern const Nv2aVshAotEntry nv2a_vsh_programs[];',
                        'extern const size_t nv2a_vsh_program_count;'):
        if declaration not in header:
            fail(f'{label}: the generated header lacks {declaration!r}')

    # Nothing the configure WROTE may name the ignored generated/recovered trees
    # or the game data.  The empty table is skipped: its comments name the retail
    # corpus to explain why it is absent, and it is compiler input.
    build_skip = (build / 'empty-corpus',)
    generated_hits = files_mentioning(build, GEN_NEEDLES, skip=build_skip)
    if generated_hits:
        names = ', '.join(str(path.relative_to(build)) for path in generated_hits[:8])
        fail(f'{label}: the build description references the ignored generated/'
             f'recovered tree(s) ({names}); a fixtures-only configure must never '
             'read them')
    game_hits = files_mentioning(build, GAME_NEEDLES, skip=build_skip)
    if game_hits:
        names = ', '.join(str(path.relative_to(build)) for path in game_hits[:8])
        fail(f'{label}: the build description references game data ({names})')

    # Neither the game nor the collector is configured.
    projects = sorted(path.name for path in build.rglob('*.vcxproj'))
    if len(projects) < 5:
        fail(f'{label}: the configure generated only {len(projects)} project(s) '
             '-- BUILD_TESTING did not take effect')
    for forbidden in ('jsrf_recomp.vcxproj', 'jsrf_collect.vcxproj'):
        if forbidden in projects:
            fail(f'{label}: the fixtures-only configure generated {forbidden}')
    for solution in build.glob('*.sln*'):
        if re.search(r'Path="[^"]*(?:jsrf_recomp|jsrf_collect)\.vcxproj"',
                     read_exact(solution), re.IGNORECASE):
            fail(f'{label}: {solution.name} lists jsrf_recomp/jsrf_collect')
    ctest = build / 'CTestTestfile.cmake'
    if not ctest.is_file() or 'add_test(' not in read_exact(ctest):
        fail(f'{label}: BUILD_TESTING produced no CTest registration')

    # The BINDING: the real fixture target CMake configured must compile the
    # generated empty table, and nothing else, and its include list must bind
    # that table's directory.  The consumer below uses THIS resolved path.
    fixture_projects = sorted(build.rglob(FIXTURE_PROJECT))
    if not fixture_projects:
        fail(f'{label}: the fixtures-only configure did not generate the fixture '
             f'target {FIXTURE_PROJECT}; the empty corpus is unbound')
    if len(fixture_projects) > 1:
        fail(f'{label}: the configure generated {len(fixture_projects)} '
             f'{FIXTURE_PROJECT} projects: '
             f'{", ".join(str(path) for path in fixture_projects)}')
    fixture_project = fixture_projects[0]
    corpus_path = fixture_corpus_binding(fixture_project, table_c, label)

    # Real compile+link+run of a consumer of the table the FIXTURE PROJECT binds.
    consumer_print = run_consumer(tree, build, corpus_path, label)
    print(f'{label}: PASS (configure rc=0, {fixture_project.name} binds '
          f'{corpus_path}, consumer: {consumer_print})')
    return {'build': build, 'consumer': consumer_print, 'status': text,
            'fixture': fixture_project.name, 'corpus': corpus_path}


def run_consumer(tree, build, corpus_path, label):
    probe = tree.parent / 'consumer-probe'
    probe.mkdir(parents=True, exist_ok=True)
    (probe / 'CMakeLists.txt').write_text(CONSUMER_CMAKE, encoding='utf-8',
                                          newline='\n')
    (probe / 'consumer.c').write_text(CONSUMER_C, encoding='utf-8', newline='\n')
    built = probe / 'build'
    # `corpus_path` came from the fixture project's own ClCompile entry; the
    # include dir is derived beside it, never hard-coded under <build>.
    completed = run(['cmake', '-S', str(probe), '-B', str(built),
                     f'-DJSRF_TABLE_C={corpus_path}',
                     f'-DJSRF_CONSUMER_C={probe / "consumer.c"}',
                     f'-DJSRF_EMPTY_DIR={corpus_path.parent}',
                     f'-DJSRF_TOOLKIT={TOOLKIT}'], timeout=600)
    if completed.returncode != 0:
        fail(f'{label}: the consumer project did not configure:\n'
             f'{tail(flattened(completed.stdout + completed.stderr))}')
    completed = run(['cmake', '--build', str(built), '--config', 'Release'],
                    timeout=600)
    if completed.returncode != 0:
        fail(f'{label}: the generated table did not compile and link:\n'
             f'{tail(flattened(completed.stdout + completed.stderr))}')
    candidates = (built / 'Release' / 'g1_consumer.exe', built / 'g1_consumer.exe',
                  built / 'Release' / 'g1_consumer', built / 'g1_consumer')
    executable = next((path for path in candidates if path.is_file()), None)
    if executable is None:
        matches = [path for path in built.rglob('g1_consumer*')
                   if path.is_file() and path.suffix.lower() in ('', '.exe')]
        executable = matches[0] if matches else None
    if executable is None:
        fail(f'{label}: the consumer executable was not produced under {built}')
    ran = run([str(executable)], timeout=120)
    printed = (ran.stdout or '').strip()
    if ran.returncode != 0 or printed != 'count=0 found=0':
        fail(f'{label}: the linked consumer of the generated table must print '
             f'count=0 found=0 and exit 0; it printed {printed!r} and exited '
             f'{ran.returncode}')
    return printed


def expect_gate_failure(action, what, required):
    try:
        action()
    except GateFailure as exc:
        reason = str(exc)
        if required not in reason:
            fail(f'{what}: the gate failed for an unexpected reason (missing '
                 f'{required!r}):\n{tail(reason)}')
        print(f'{what}: detected (gate FAILED as required): '
              f'{str(exc).splitlines()[0][:240]}')
        return reason
    fail(f'{what}: the gate PASSED a mutant it must reject')


# ── driver ──────────────────────────────────────────────────────────────────
def main():
    reason = missing_prerequisites()
    if reason:
        print('G1 SKIP: this host lacks ' + '; '.join(reason)
              + ' -- the fixtures-only configure cannot be exercised here.',
              file=sys.stderr)
        return 77
    inputs = missing_gate_inputs()
    if inputs:
        print('G1 FAIL: the host has CMake and Vulkan but the gate inputs are '
              'missing: ' + '; '.join(inputs), file=sys.stderr)
        return 1

    shared_before = {relative: sha256_file(ROOT / relative)
                     for relative in SHARED_INPUTS}
    pristine_cmake = read_exact(ROOT / 'CMakeLists.txt')
    print(f'g1: shared CMakeLists.txt sha256={shared_before["CMakeLists.txt"]}; '
          f'tests/test_optional_vulkan.py sha256='
          f'{shared_before["tests/test_optional_vulkan.py"]}')

    scratch = Path(tempfile.mkdtemp(prefix='jsrf-g1-fixtures-only-'))
    stage = 'setup'
    try:
        tracked = tracked_paths()
        print(f'g1: scratch tree from {len(tracked)} tracked file(s) under {ROOT}')

        # 1. Baseline: the shipped fixtures-only configure.
        stage = 'baseline'
        baseline_tree = make_tree(scratch / 'baseline' / 'tree', tracked)
        if sha256_file(baseline_tree / 'CMakeLists.txt') != \
                shared_before['CMakeLists.txt']:
            fail('the scratch tree did not receive the shipped CMakeLists.txt')
        run_gate(baseline_tree, 'build-baseline', 'baseline')

        # 2. M-G1: remove `AND NOT JSRF_FIXTURES_ONLY` from line 111.
        stage = 'mutant-111'
        mutant_tree = make_tree(scratch / 'mutant' / 'tree', tracked)
        mutate(mutant_tree, LINE111_ANCHOR, LINE111_MUTANT, 'line-111')
        expect_gate_failure(
            lambda: run_gate(mutant_tree, 'build-mutant', 'mutant-111'),
            'M-G1 (capability branch without the fixtures-only exclusion)',
            'configure failed')

        # 3. Restore the pristine bytes and require PASS again.
        stage = 'restored'
        write_exact(mutant_tree / 'CMakeLists.txt', pristine_cmake)
        if sha256_file(mutant_tree / 'CMakeLists.txt') != \
                shared_before['CMakeLists.txt']:
            fail('the restored CMakeLists.txt is not byte-identical to the '
                 'shipped file')
        run_gate(mutant_tree, 'build-restored', 'restored')

        # 4. Negative control: fixtures-only reads the source generated tree.
        stage = 'negative'
        negative_tree = make_tree(scratch / 'negative' / 'tree', tracked)
        mutate(negative_tree, FIXTURES_ANCHOR, FIXTURES_MUTANT, 'fixtures-branch')
        expect_gate_failure(
            lambda: run_gate(negative_tree, 'build-negative', 'negative'),
            'negative control (fixtures-only reads src/recomp/gen)',
            'references the ignored generated/recovered tree')

        # 5. M-CORPUS: clear the empty-corpus source while leaving the table
        # generation and include dir intact.  The configure still succeeds and
        # still prints the empty-corpus STATUS, so only the fixture project's
        # own ClCompile binding can reject it.
        stage = 'mutant-corpus'
        corpus_tree = make_tree(scratch / 'corpus' / 'tree', tracked)
        mutate(corpus_tree, CORPUS_ANCHOR, CORPUS_MUTANT, 'empty-corpus source')
        expect_gate_failure(
            lambda: run_gate(corpus_tree, 'build-corpus-mutant',
                             'mutant-corpus'),
            'M-CORPUS (empty-corpus source cleared)',
            'missing corpus binding')

        # 6. Restore the pristine bytes and require PASS again.
        stage = 'corpus-restored'
        write_exact(corpus_tree / 'CMakeLists.txt', pristine_cmake)
        if sha256_file(corpus_tree / 'CMakeLists.txt') != \
                shared_before['CMakeLists.txt']:
            fail('the restored corpus-tree CMakeLists.txt is not byte-identical '
                 'to the shipped file')
        run_gate(corpus_tree, 'build-corpus-restored', 'corpus-restored')

        # 7. The shared sources were inputs only.
        stage = 'shared-hashes'
        shared_after = {relative: sha256_file(ROOT / relative)
                        for relative in SHARED_INPUTS}
        if shared_after != shared_before:
            fail('a shared source changed during the run: before='
                 f'{shared_before}, after={shared_after}')
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    print('G1 PASS: fixtures-only configure selected the generated empty corpus, '
          'configured neither jsrf_recomp nor jsrf_collect, advertised every '
          'C3 exclusion group, bound the empty table in the real fixture '
          'project, linked and ran its consumer (count=0, no lookup hit), '
          'rejected the line-111, generated-tree and cleared-corpus mutants, '
          'and left the shared sources byte-identical.')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except GateFailure as failure:
        diagnostic = (f'G1 FAILED at {__file__}\n\n{failure}\n')
        where = Path(tempfile.gettempdir()) / \
            f'jsrf-g1-fixtures-only-failure-{os.getpid()}.txt'
        try:
            where.write_text(diagnostic, encoding='utf-8')
            hint = f'diagnostic: {where}'
        except OSError:
            hint = 'diagnostic could not be written'
        print(f'G1 FAIL: {failure}\n{hint}', file=sys.stderr)
        sys.exit(1)
    except Exception:  # noqa: BLE001 - a crash is a gate failure, never a skip
        traceback.print_exc()
        print('G1 FAIL: unexpected exception', file=sys.stderr)
        sys.exit(1)
