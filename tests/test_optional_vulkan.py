"""R2: the vertex-program corpus seam of a build WITHOUT the Vulkan toolchain.

WHAT THIS PINS
==============
The toolkit publishes ONE bound capability decision, `XBR_VULKAN_FOUND` (a FORCE-refreshed
cache bool, with `XBR_VULKAN_UNAVAILABLE_REASON` beside it), and the game's
`CMakeLists.txt` branches on it exactly once:

  * capability ON  -- the retail corpus in `src/recomp/gen/shaders` is REQUIRED, its
    provenance is validated inside the SAME configure by `scripts/build-jsrf.py
    --check-corpus-only --expect-capability-on`, and it is compiled;
  * capability OFF -- a generated empty table (`<build>/empty-corpus/nv2a_vsh_programs.{c,h}`,
    `nv2a_vsh_program_count = 0`) is compiled instead, and the retail corpus is neither
    globbed, nor included, nor compiled -- EVEN WHEN IT IS PRESENT.

Every case here loads the REAL shipped files (`CMakeLists.txt`, `scripts/build-jsrf.py` and
the `scripts/cmake/vulkan-capability` probe project it configures) in a scratch copy of the
tree; nothing re-implements the decision.  What is asserted is always an observable of a
real configure: its exit code, its status line, the cache it wrote, the generated files, the
source list in the generated project, the CTest registration, the guard's exit code, and the
result of compiling and LINKING a tiny consumer of the generated table with the real compiler.

ABSENCE CONTROLS (no host change)
=================================
The toolchain is removed from the configure's view with CMake's own controls, never by
touching the SDK, PATH or the registry:

    -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON
    -DXBR_DXC=<nonexistent>  -DXBR_SPIRV_VAL=<nonexistent>

`XBR_VULKAN_FOUND` is recomputed from scratch on every configure, so these controls decide
the mode of the configure they are passed to and of nothing else.

OVERRIDES (how a mutant copy is what the cases load)
====================================================
Two environment variables redirect the files under test, so a reviewer can run this module
against a file-level mutant of a production file:

  * `JSRF_R2_GAME_CMAKE=<path>`   -- used as the scratch tree's `CMakeLists.txt`
                                     (the mutants below are copies of the shipped file with
                                     one localized substitution each);
  * `JSRF_R2_BUILD_SCRIPT=<path>` -- copied over the scratch tree's
                                     `scripts/build-jsrf.py`.

`MutantDetectionTests` uses exactly this override to run a focused case in a CHILD process
against each mutant and require it to turn red, with a comment-only edit as the control:
M-A (`if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)` -> `if(TRUE AND NOT JSRF_FIXTURES_ONLY)`:
the capability test is removed while the fixtures-only axis stays), M-B (the two FATAL_ERRORs of
the capability-ON refusal -> WARNING: an empty-table success),
M-C (the generated table declares one program) and M-C2 (the OFF branch globs the stale
retail corpus again).

PREREQUISITES
=============
The host state the suite is written for (and the same one the R1 fixtures need): the sibling
toolkit checkout with `cmake/XbrVulkan.cmake`, the Vulkan SDK at `$VULKAN_SDK` (dxc and
spirv-val), the ignored lifted tree (`src/recomp/gen` + `recovered/recovered.c`), the retail
corpus, and the owned `build/vsh-tool/.../nv2a_vsh_hlsl.exe`.  A missing prerequisite skips
the cases that need it and nothing else.

RUNNING
=======
`python -X utf8 tests/test_optional_vulkan.py` (how CTest runs it).  Every case is temp-dir
isolated; nothing writes the repository, the owner's build tree, the corpus or the SDK.
Measured on the host this was written on: ~117 s for the ten cases (seven configures of the
real project, one small compile+link, four mutant child runs and one control child run), so
the registered CTest TIMEOUT is 300 s.
"""
from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The files under test.  `JSRF_R2_GAME_CMAKE` / `JSRF_R2_BUILD_SCRIPT` are the documented
# mutant channels (see the module docstring): they are read ONCE, here, so a child process
# the mutant harness spawns loads the mutant and this process loads the shipped file.
GAME_CMAKE = Path(os.environ.get('JSRF_R2_GAME_CMAKE') or (ROOT / 'CMakeLists.txt'))
BUILD_SCRIPT = Path(os.environ.get('JSRF_R2_BUILD_SCRIPT')
                    or (ROOT / 'scripts' / 'build-jsrf.py'))
SHIPPED_CMAKE = ROOT / 'CMakeLists.txt'
SHIPPED_BUILD_SCRIPT = ROOT / 'scripts' / 'build-jsrf.py'
if GAME_CMAKE != SHIPPED_CMAKE or BUILD_SCRIPT != SHIPPED_BUILD_SCRIPT:
    # A silent mode switch is how a stray environment variable turns a green suite into
    # evidence about a file nobody meant to test, so the override announces itself in the
    # log.  (Everything else about the run is unchanged: the cases still assert on
    # observables of a real configure, of whichever file was named.)
    print(f'r2: OVERRIDE in effect: game CMakeLists.txt <- {GAME_CMAKE}; '
          f'scripts/build-jsrf.py <- {BUILD_SCRIPT}; the shipped files are '
          f'{SHIPPED_CMAKE} and {SHIPPED_BUILD_SCRIPT}')

sys.path.insert(0, str(ROOT / 'scripts'))
from toolkit_path import toolkit_path  # noqa: E402
TOOLKIT = toolkit_path()
# The SDK root is had from the environment and otherwise from the documented install
# location, like the R1 fixtures: a capability-ON configure must not depend on whose shell
# started it (the guard's own `corpus_dxc` reads `$VULKAN_SDK` too, so it is passed down).
VULKAN_SDK = Path(os.environ.get('VULKAN_SDK') or r'C:\VulkanSDK\1.4.357.0')
DXC = VULKAN_SDK / 'Bin' / 'dxc.exe'
SPIRV_VAL = VULKAN_SDK / 'Bin' / 'spirv-val.exe'

REAL_CORPUS = ROOT / 'src' / 'recomp' / 'gen' / 'shaders'
CORPUS_FILES = ('nv2a_vsh_programs.c', 'nv2a_vsh_programs.h')
# The owned artifact whose sha256 the ignored corpus records (`build/vsh-tool`, either CMake
# layout).  The game build tree's own copy is deliberately not a candidate: `just build`
# relinks it, so its hash moves without an input moving.
OWNED_TOOL_CANDIDATES = (
    ROOT / 'build' / 'vsh-tool' / 'src' / 'video' / 'Release' / 'nv2a_vsh_hlsl.exe',
    ROOT / 'build' / 'vsh-tool' / 'src' / 'video' / 'nv2a_vsh_hlsl.exe',
)
OWNED_TOOL = next((path for path in OWNED_TOOL_CANDIDATES if path.is_file()), None)

# The three controls that remove the toolchain without touching the host.
OFF_CONTROLS = (
    '-DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=ON',
)
# `-U` is CMake's own cache-removal escape hatch, and it is how a build tree that was
# configured OFF is asked the capability question AGAIN: without it the OFF controls would
# still sit in the cache of the second configure and decide its mode for it.
ON_RECONFIGURE = ('-U', 'CMAKE_DISABLE_FIND_PACKAGE_Vulkan', '-U', 'XBR_DXC',
                  '-U', 'XBR_SPIRV_VAL', '-U', 'XBR_VK_SW_ICD')

EMPTY_TABLE_C = 'nv2a_vsh_programs.c'
EMPTY_TABLE_H = 'nv2a_vsh_programs.h'
COUNT_ZERO = 'const size_t nv2a_vsh_program_count = 0;'
CORPUS_NEEDLE = ('gen\\shaders', 'gen/shaders')

_CACHE_ENTRY = r'^\s*{name}\s*(?::\s*[A-Za-z_][A-Za-z0-9_]*\s*)?=\s*(.*?)\s*$'


# ── small observables ────────────────────────────────────────────────────────────────────
def read_exact(path):
    """The file's text with NO newline translation (the trees are Windows trees)."""
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


def cache_value(cache, name):
    """One CMakeCache entry's value, of any type, or None (the guard's own rule)."""
    cache = Path(cache)
    if not cache.is_file():
        return None
    match = re.search(_CACHE_ENTRY.format(name=re.escape(name)), read_exact(cache),
                      re.MULTILINE)
    if not match:
        return None
    return match.group(1).strip() or None


def flattened(text):
    """CMake wraps long `message()`s across lines; every message assertion is on this."""
    return ' '.join(text.split())


def configure_text(completed):
    return flattened((completed.stdout or '') + (completed.stderr or ''))


def files_mentioning(folder, needles, skip=()):
    """Every file under `folder` whose BYTES contain one of `needles`, minus `skip`.

    Bytes, not text: a generated build file is not guaranteed to be UTF-8 (some CMake and
    MSBuild intermediates are UTF-16), and the needles are ASCII, so both encodings are
    searched and a file that cannot be decoded cannot hide a reference.
    """
    skip = tuple(Path(s) for s in skip)
    patterns = [n.encode('utf-8') for n in needles] + [n.encode('utf-16-le') for n in needles]
    hits = []
    for path in sorted(folder.rglob('*')):
        if not path.is_file():
            continue
        if any(path == s or s in path.parents for s in skip):
            continue
        if path.suffix.lower() in ('.obj', '.pdb', '.exe', '.dll', '.lib', '.ilk', '.res'):
            continue
        try:
            raw = path.read_bytes()
        except OSError:
            continue
        if any(pattern in raw for pattern in patterns):
            hits.append(path)
    return hits


def table_sources(project_text):
    """The `nv2a_vsh_programs.c` the generated project COMPILES, as native paths.

    Both element forms are matched: the VS generator writes `<ClCompile Include="x" />`
    when the source needs no extra properties and `<ClCompile Include="x">` with children
    (an `<ObjectFileName>`, say) when it does -- a project that compiles TWO sources named
    the same way is exactly the mutant this suite has to see.
    """
    found = re.findall(r'<ClCompile Include="([^"]*)"\s*/?>', project_text)
    return [Path(entry) for entry in found if Path(entry).name == 'nv2a_vsh_programs.c']


def registered_tests(build):
    ctest = build / 'CTestTestfile.cmake'
    if not ctest.is_file():
        return []
    return re.findall(r'add_test\("([^"]+)"', read_exact(ctest))


def base_env():
    """The environment every configure and every guard run in.

    The toolkit and the SDK are named explicitly (the guard's probe and `corpus_dxc` read
    them from the environment), and the variables that could otherwise decide a mode behind
    the case's back are removed -- in particular `JSRF_NV2A_VSH_HLSL`, which would make the
    guard use a tool this suite did not choose, and a `CMAKE_DISABLE_FIND_PACKAGE_Vulkan`
    or `XBR_VK_SW_ICD` from the caller's shell -- so the controls of each case are its own.
    """
    scrub = ('XBOXRECOMP_DIR', 'VULKAN_SDK', 'JSRF_NV2A_VSH_HLSL', 'XBR_VK_SW_ICD',
             'XBR_DXC', 'XBR_SPIRV_VAL', 'CMAKE_DISABLE_FIND_PACKAGE_Vulkan',
             'CMAKE_PREFIX_PATH', 'Vulkan_ROOT', 'JSRF_R2_GAME_CMAKE',
             'JSRF_R2_BUILD_SCRIPT')
    env = {key: value for key, value in os.environ.items() if key not in scrub}
    env['XBOXRECOMP_DIR'] = str(TOOLKIT)
    env['VULKAN_SDK'] = str(VULKAN_SDK)
    return env


def prerequisites():
    """What this host must have for the cases to assert through the REAL decision."""
    missing = []
    if not (TOOLKIT / 'cmake' / 'XbrVulkan.cmake').is_file():
        missing.append(f'{TOOLKIT}/cmake/XbrVulkan.cmake')
    if not DXC.is_file():
        missing.append(str(DXC))
    if not SPIRV_VAL.is_file():
        missing.append(str(SPIRV_VAL))
    if not (ROOT / 'src' / 'recomp' / 'recovered' / 'recovered.c').is_file():
        missing.append('src/recomp/recovered/recovered.c')
    if not (REAL_CORPUS / 'nv2a_vsh_programs.c').is_file():
        missing.append(str(REAL_CORPUS))
    if OWNED_TOOL is None:
        missing.append('build/vsh-tool/.../nv2a_vsh_hlsl.exe')
    return missing


class ScratchTree:
    """A throwaway copy of the game tree the SHIPPED `CMakeLists.txt` configures.

    `src/recomp` (the ignored lifted code, including `recovered/recovered.c`) and the
    reference sources are what the top of that file requires before it will configure; the
    test sources are needed because `BUILD_TESTING` is left at its default, so the corpus-
    dependent registrations this suite asserts on are the ones a real configure makes.  The
    retail corpus is COPIED (`corpus=True`) or OMITTED (`corpus=False`, the fresh-checkout
    state) -- the owner's corpus is never read from, written to or configured against.
    """

    def __init__(self, root, corpus):
        self.dir = Path(root)
        self.dir.mkdir(parents=True)
        self.corpus = self.dir / 'src' / 'recomp' / 'gen' / 'shaders'
        shutil.copytree(ROOT / 'src', self.dir / 'src',
                        ignore=None if corpus else shutil.ignore_patterns('shaders'))
        shutil.copytree(ROOT / 'tests', self.dir / 'tests',
                        ignore=shutil.ignore_patterns('__pycache__'))
        shutil.copytree(ROOT / 'scripts', self.dir / 'scripts',
                        ignore=shutil.ignore_patterns('__pycache__'))
        # Byte copies, so a mutant harness's file is exactly what this tree configures.
        shutil.copy(GAME_CMAKE, self.dir / 'CMakeLists.txt')
        if BUILD_SCRIPT != SHIPPED_BUILD_SCRIPT:
            shutil.copy(BUILD_SCRIPT, self.dir / 'scripts' / 'build-jsrf.py')
        harness = self.dir / 'tools' / 'harness'
        harness.mkdir(parents=True)
        shutil.copy(ROOT / 'tools' / 'harness' / 'collect.c', harness / 'collect.c')

    def install_owned_tool(self):
        """The artifact the corpus records, under the tree's own `build/vsh-tool`."""
        destination = (self.dir / 'build' / 'vsh-tool' / 'src' / 'video' / 'Release'
                       / 'nv2a_vsh_hlsl.exe')
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(OWNED_TOOL, destination)
        return destination

    def configure(self, name, extra=()):
        """`cmake -S <tree> -B <tree>/<name>` -- the game's real configure entry."""
        return subprocess.run(
            ['cmake', '-S', str(self.dir), '-B', str(self.dir / name),
             f'-DXBOXRECOMP_DIR={TOOLKIT}', *extra],
            cwd=str(self.dir), env=base_env(), capture_output=True, text=True, timeout=900)

    def guard(self, *args):
        """The shipped guard, exactly as CMake's capability-ON branch calls it."""
        return subprocess.run(
            [sys.executable, '-X', 'utf8', str(self.dir / 'scripts' / 'build-jsrf.py'),
             '--check-corpus-only', *args],
            cwd=str(self.dir), env=base_env(), capture_output=True, text=True, timeout=900)


class CorpusCase(unittest.TestCase):
    """The assertions every case shares, so the two branches cannot drift."""

    def assert_empty_table_selected(self, build, exe='jsrf_recomp'):
        """The generated empty table is on disk, is count 0, and IS what the exe compiles."""
        table_c = build / 'empty-corpus' / EMPTY_TABLE_C
        table_h = build / 'empty-corpus' / EMPTY_TABLE_H
        self.assertTrue(table_c.is_file(), f'{table_c} was not generated')
        self.assertTrue(table_h.is_file(), f'{table_h} was not generated')
        self.assertIn(COUNT_ZERO, read_exact(table_c),
                      'the generated table must declare count 0')
        self.assertIn('nv2a_vsh_programs[]', read_exact(table_h))
        project = read_exact(build / f'{exe}.vcxproj')
        sources = table_sources(project)
        self.assertEqual([str(source) for source in sources], [str(table_c)],
                         f'the exe must compile exactly the generated empty table: {sources}')
        self.assertNotIn(CORPUS_NEEDLE[0], project)
        self.assertIn(str(build / 'empty-corpus'), read_exact(build / f'{exe}.vcxproj'))

    def assert_real_corpus_selected(self, build, exe='jsrf_recomp'):
        """The real corpus (and only it) is what the exe compiles."""
        expected = self.tree.dir / 'src' / 'recomp' / 'gen' / 'shaders' / EMPTY_TABLE_C
        project = read_exact(build / f'{exe}.vcxproj')
        sources = table_sources(project)
        self.assertEqual([str(source) for source in sources], [str(expected)],
                         f'the exe must compile exactly the retail corpus table: {sources}')
        self.assertNotIn('empty-corpus', project)

    def assert_no_corpus_reference(self, build, where=''):
        """NOTHING the configure wrote names the retail corpus directory.

        The generated empty table is skipped on purpose: its own comments NAME that
        directory to explain why it exists, and it is compiler input rather than a build
        description.  Every other file -- project, filters, solution, CTest registration,
        cache, CMake's own intermediates -- must be silent about it.
        """
        hits = files_mentioning(build, CORPUS_NEEDLE, skip=(build / 'empty-corpus',))
        self.assertEqual([str(hit.relative_to(build)) for hit in hits], [],
                         f'the OFF branch must not reference the retail corpus {where}')


class OffWithoutCorpusTests(CorpusCase):
    """Case 1: capability OFF with no corpus -- the defect R2 exists to fix."""

    @classmethod
    def setUpClass(cls):
        missing = prerequisites()
        if missing:
            raise unittest.SkipTest('this host lacks: ' + ', '.join(missing))
        cls._tmp = tempfile.TemporaryDirectory(prefix='jsrf-r2-off-')
        cls.tree = ScratchTree(Path(cls._tmp.name) / 'tree', corpus=False)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_capability_off_without_a_corpus_compiles_the_generated_empty_table(self):
        build = self.tree.dir / 'build-off'
        completed = self.tree.configure('build-off', OFF_CONTROLS + (
            f'-DXBR_DXC={self.tree.dir / "absent" / "dxc.exe"}',
            f'-DXBR_SPIRV_VAL={self.tree.dir / "absent" / "spirv-val.exe"}'))
        text = configure_text(completed)
        self.assertEqual(completed.returncode, 0,
                         'the configure must succeed without the toolchain, but today\'s '
                         f'pre-R2 behaviour is exactly the corpus FATAL:\n{text[-4000:]}')

        # The configure really RAN, and it really decided OFF: the FORCE-refreshed cache
        # entry, not a message string.
        cache = build / 'CMakeCache.txt'
        self.assertTrue(cache.is_file(), 'the configure wrote no cache')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=FALSE', read_exact(cache))
        reason = cache_value(cache, 'XBR_VULKAN_UNAVAILABLE_REASON')
        self.assertTrue(reason, 'the OFF decision must record WHY it is false')

        # The status line names the reason discovery gave and the fallback that follows.
        self.assertIn('-- Vulkan/unavailable:', text)
        self.assertIn(flattened(reason), text,
                      'the status line must name the reason the cache holds')
        self.assertIn('generated empty vertex-program corpus', text)
        self.assertIn('nv2a_vsh_program_count = 0', text)
        self.assertIn('CPU interpreter', text)

        # The empty table is generated build output and it defines count 0.
        self.assert_empty_table_selected(build)
        self.assert_no_corpus_reference(build, 'in an OFF configure')

        # The corpus-dependent test targets are not registered (they link the corpus).
        self.assertIn('jsrf_vsh_aot, jsrf_vsh_aot_control1/2 and jsrf_vsh_aot_malformed are '
                      'not registered', text)
        registered = registered_tests(build)
        self.assertNotIn('jsrf_vsh_aot', registered)
        # ... while THIS test is registered UNCONDITIONALLY, which is the point of the
        # branch: gating the no-Vulkan gate on the capability would leave exactly the
        # SDK-less build it exists for with no gate at all.
        self.assertIn('jsrf_optional_vulkan', registered)

        # LINK EVIDENCE: the generated table and the toolkit's `nv2a_vsh_aot.h` compile and
        # link into a real executable, and the count it prints is 0.
        printed = self.link_the_generated_table(build)
        print(f'r2 case 1: OFF configure rc={completed.returncode}; reason: {reason}; '
              f'the linked consumer of the generated table printed {printed!r}')

    def link_the_generated_table(self, build):
        """Compile+link a consumer of the generated table with the real compiler, run it."""
        probe = self.tree.dir / 'count-probe'
        table_dir = build / 'empty-corpus'
        probe.mkdir(parents=True, exist_ok=True)
        (probe / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\n'
            'project(jsrf_corpus_count_probe C)\n'
            'add_executable(corpus_count_probe "${JSRF_TABLE_C}" "${JSRF_CONSUMER_C}")\n'
            'target_include_directories(corpus_count_probe PRIVATE\n'
            '    "${JSRF_TABLE_DIR}" "${JSRF_TOOLKIT}/src/video")\n', encoding='utf-8')
        (probe / 'count.c').write_text(
            '#include <stdio.h>\n'
            '#include "nv2a_vsh_programs.h"\n'
            'int main(void) {\n'
            '    const Nv2aVshAotEntry *table = nv2a_vsh_programs;\n'
            '    (void)table;\n'
            '    printf("%zu\\n", nv2a_vsh_program_count);\n'
            '    return nv2a_vsh_program_count == 0 ? 0 : 1;\n'
            '}\n', encoding='utf-8')
        built = probe / 'build'
        configure = subprocess.run(
            ['cmake', '-S', str(probe), '-B', str(built),
             f'-DJSRF_TABLE_C={table_dir / EMPTY_TABLE_C}',
             f'-DJSRF_CONSUMER_C={probe / "count.c"}',
             f'-DJSRF_TABLE_DIR={table_dir}', f'-DJSRF_TOOLKIT={TOOLKIT}'],
            env=base_env(), capture_output=True, text=True, timeout=900)
        self.assertEqual(configure.returncode, 0,
                         f'the consumer project did not configure:\n'
                         f'{configure.stdout[-2000:]}{configure.stderr[-2000:]}')
        compiled = subprocess.run(['cmake', '--build', str(built), '--config', 'Release'],
                                  env=base_env(), capture_output=True, text=True, timeout=900)
        self.assertEqual(compiled.returncode, 0,
                         f'the generated table did not compile and link:\n'
                         f'{compiled.stdout[-3000:]}{compiled.stderr[-3000:]}')
        executable = built / 'Release' / 'corpus_count_probe.exe'
        self.assertTrue(executable.is_file(), f'{executable} was not linked')
        ran = subprocess.run([str(executable)], capture_output=True, text=True, timeout=120)
        self.assertEqual(ran.stdout.strip(), '0',
                         f'the linked consumer of the generated table must print 0, not '
                         f'{ran.stdout!r}')
        self.assertEqual(ran.returncode, 0,
                         'the consumer exits non-zero when the count it links is not 0')
        return ran.stdout.strip()


class OnWithoutCorpusTests(CorpusCase):
    """Case 2: capability ON with no corpus -- the refusal that must survive."""

    @classmethod
    def setUpClass(cls):
        missing = prerequisites()
        if missing:
            raise unittest.SkipTest('this host lacks: ' + ', '.join(missing))
        cls._tmp = tempfile.TemporaryDirectory(prefix='jsrf-r2-on-')
        cls.tree = ScratchTree(Path(cls._tmp.name) / 'tree', corpus=False)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_capability_on_without_a_corpus_refuses_naming_just_shaders(self):
        build = self.tree.dir / 'build-on'
        completed = self.tree.configure('build-on')
        text = configure_text(completed)
        self.assertNotEqual(completed.returncode, 0,
                            'a capability-ON configure must refuse a missing corpus, '
                            f'because it would otherwise compile an empty table:\n{text[-3000:]}')
        self.assertIn('src/recomp/gen/shaders is not tracked', text)
        self.assertIn('just shaders', text)
        self.assertIn('XBR_VULKAN_FOUND=TRUE', text,
                      'the refusal must be the capability-ON branch, not a discovery failure')
        self.assertFalse((build / 'empty-corpus' / EMPTY_TABLE_C).exists(),
                         'the ON branch must not generate an empty table')

        # The same question asked by hand, through the entry CMake calls: it must refuse
        # exactly the same way and must not configure anything.
        guard_build = self.tree.dir / 'build'
        self.assertFalse((guard_build / 'CMakeCache.txt').exists(),
                         'precondition: the guard call must be given an unconfigured tree')
        for args in ((), ('--expect-capability-on',)):
            ran = self.tree.guard(*args)
            output = configure_text(ran)
            self.assertEqual(ran.returncode, 1,
                             f'`--check-corpus-only {" ".join(args)}` must exit 1 without the '
                             f'corpus:\n{output[-3000:]}')
            self.assertIn('capability: Vulkan available', output)
            self.assertIn('just shaders', output)
            self.assertIn('nv2a_vsh_programs.c', output)
        self.assertFalse((guard_build / 'CMakeCache.txt').exists(),
                         'the guard must not configure the game (it configures only its own '
                         'throwaway capability probe)')
        print(f'r2 case 2: ON configure rc={completed.returncode}; guard rc=1; refusal: '
              f'{[line.strip() for line in (completed.stdout + completed.stderr).splitlines() if "run `just shaders`" in line][:1]}')


class CorpusPresentTests(CorpusCase):
    """Cases 3, 4 and 5: a real (older) corpus in the tree, with and without the toolchain."""

    @classmethod
    def setUpClass(cls):
        missing = prerequisites()
        if missing:
            raise unittest.SkipTest('this host lacks: ' + ', '.join(missing))
        cls._tmp = tempfile.TemporaryDirectory(prefix='jsrf-r2-corpus-')
        cls.tree = ScratchTree(Path(cls._tmp.name) / 'tree', corpus=True)
        cls.tree.install_owned_tool()

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def corpus_state(self):
        return {name: (sha256_file(self.tree.corpus / name),
                       (self.tree.corpus / name).stat().st_mtime_ns,
                       (self.tree.corpus / name).stat().st_size)
                for name in CORPUS_FILES}

    def test_capability_off_ignores_a_real_corpus_and_leaves_it_untouched(self):
        """The old-corpus case: present in the tree, and still NOT compiled."""
        before = self.corpus_state()
        self.assertEqual(sorted(path.name for path in self.tree.corpus.iterdir()),
                         sorted(CORPUS_FILES), 'precondition: the copy is the retail corpus')
        build = self.tree.dir / 'build-off-with-corpus'
        completed = self.tree.configure('build-off-with-corpus', OFF_CONTROLS + (
            f'-DXBR_DXC={self.tree.dir / "absent" / "dxc.exe"}',
            f'-DXBR_SPIRV_VAL={self.tree.dir / "absent" / "spirv-val.exe"}'))
        text = configure_text(completed)
        self.assertEqual(completed.returncode, 0,
                         f'an OFF configure must not need the corpus:\n{text[-3000:]}')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=FALSE',
                      read_exact(build / 'CMakeCache.txt'))
        self.assertIn('-- Vulkan/unavailable:', text)
        self.assertIn('neither included nor compiled', text)

        # The empty table is what the project compiles -- with the corpus RIGHT THERE.
        self.assert_empty_table_selected(build)
        self.assert_no_corpus_reference(build, 'even with a real corpus present')

        after = self.corpus_state()
        self.assertEqual(after, before,
                         'the OFF configure must not touch the corpus copy (hash, mtime or '
                         'size), and must not add anything beside it')
        self.assertEqual(sorted(path.name for path in self.tree.corpus.iterdir()),
                         sorted(CORPUS_FILES),
                         'nothing new may appear under the tree\'s corpus directory')
        print(f'r2 case 3: OFF-with-corpus rc={completed.returncode}; corpus sha256/mtime '
              f'unchanged: {after["nv2a_vsh_programs.c"][0][:12]}...')

    def test_capability_on_refuses_a_corpus_recording_another_tool(self):
        """The in-configure provenance check is load-bearing, and it is quotable."""
        saved = {name: read_exact(self.tree.corpus / name) for name in CORPUS_FILES}
        mutated = 'deadbeef' * 8
        try:
            for name in CORPUS_FILES:
                text = re.sub(r'(tool\.sha256 = )[0-9a-f]{64}', rf'\g<1>{mutated}', saved[name])
                self.assertNotEqual(text, saved[name],
                                    f'{name} records no tool.sha256 to corrupt')
                write_exact(self.tree.corpus / name, text)
            completed = self.tree.configure('build-on-mutated-corpus')
            text = configure_text(completed)
            self.assertNotEqual(completed.returncode, 0,
                                'a corpus recording another tool must not be compiled:\n'
                                f'{text[-4000:]}')
            self.assertIn('tool.sha256 differs', text)
            self.assertIn(mutated, text)
            self.assertIn('--check-corpus-only --expect-capability-on exited 1', text,
                          'the refusal must be the GUARD\'s, run inside this configure')
            self.assertIn('just shaders', text)
            self.assertFalse((self.tree.dir / 'build-on-mutated-corpus' / 'empty-corpus'
                              / EMPTY_TABLE_C).exists(),
                             'a failed ON configure must not fall back to an empty table')
            print(f'r2 case 4b: mutated-corpus rc={completed.returncode}; guard problem: '
                  f'{[line.strip() for line in (completed.stdout + completed.stderr).splitlines() if "tool.sha256 differs" in line][:1]}')
        finally:
            for name in CORPUS_FILES:
                write_exact(self.tree.corpus / name, saved[name])
        for name in CORPUS_FILES:
            self.assertEqual(read_exact(self.tree.corpus / name), saved[name],
                             f'{name} was not restored byte for byte')

    def test_capability_on_validates_the_corpus_in_the_same_configure(self):
        """The positive control: with the toolchain and a current corpus, ON compiles it."""
        probe_log = self.tree.dir / 'logs' / 'vulkan-capability-probe.log'
        if probe_log.exists():
            probe_log.unlink()
        build = self.tree.dir / 'build-on-with-corpus'
        completed = self.tree.configure('build-on-with-corpus')
        text = configure_text(completed)
        self.assertEqual(completed.returncode, 0,
                         f'the ON configure must accept a current corpus:\n{text[-4000:]}')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=TRUE', read_exact(build / 'CMakeCache.txt'))
        self.assertIn('-- Vulkan/available:', text)
        self.assertIn('validated in this configure', text)
        self.assertIn('compiled (1 file(s) from src/recomp/gen/shaders)', text)
        self.assert_real_corpus_selected(build)
        self.assertFalse((build / 'empty-corpus' / EMPTY_TABLE_C).exists(),
                         'an ON configure must not generate the empty table')

        # The provenance check RAN inside this configure: its own capability probe wrote
        # the log the guard keeps, in this tree, with this configure's environment.
        self.assertTrue(probe_log.is_file(),
                        'the in-configure provenance check left no capability probe log')
        self.assertIn('XBR capability probe: XBR_VULKAN_FOUND=TRUE', read_exact(probe_log))

        # And the corpus-dependent tests are registered, so the ON branch is not silently
        # narrower than the tree it describes.
        registered = registered_tests(build)
        self.assertIn('jsrf_vsh_aot', registered)
        self.assertIn('jsrf_vsh_aot_malformed', registered)
        print(f'r2 case 4a: ON-with-corpus rc={completed.returncode}; status: '
              f'{[line.strip() for line in (completed.stdout + completed.stderr).splitlines() if "Vulkan/available:" in line][:1]}')

    def test_the_mode_transitions_in_one_build_tree(self):
        """Case 5: OFF -> ON -> OFF in ONE build tree, by reconfigure alone."""
        build_dir = self.tree.dir / 'build-transitions'
        name = 'build-transitions'
        off_controls = OFF_CONTROLS + (
            f'-DXBR_DXC={self.tree.dir / "absent" / "dxc.exe"}',
            f'-DXBR_SPIRV_VAL={self.tree.dir / "absent" / "spirv-val.exe"}')

        first = self.tree.configure(name, off_controls)
        first_text = configure_text(first)
        self.assertEqual(first.returncode, 0, f'OFF configure failed:\n{first_text[-3000:]}')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=FALSE',
                      read_exact(build_dir / 'CMakeCache.txt'))
        self.assertIn('-- Vulkan/unavailable:', first_text)
        self.assert_empty_table_selected(build_dir)
        self.assert_no_corpus_reference(build_dir, 'after the first OFF configure')

        second = self.tree.configure(name, ON_RECONFIGURE)
        second_text = configure_text(second)
        self.assertEqual(second.returncode, 0,
                         f'the same tree must reconfigure ON:\n{second_text[-4000:]}')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=TRUE',
                      read_exact(build_dir / 'CMakeCache.txt'))
        self.assertIn('-- Vulkan/available:', second_text,
                      'the cache entry must be refreshed by the second configure')
        self.assert_real_corpus_selected(build_dir)
        self.assertIn('jsrf_vsh_aot', registered_tests(build_dir))

        third = self.tree.configure(name, off_controls)
        third_text = configure_text(third)
        self.assertEqual(third.returncode, 0, f'OFF reconfigure failed:\n{third_text[-3000:]}')
        self.assertIn('XBR_VULKAN_FOUND:BOOL=FALSE',
                      read_exact(build_dir / 'CMakeCache.txt'))
        self.assertIn('-- Vulkan/unavailable:', third_text,
                      'the ON value must not survive a reconfigure')
        self.assert_empty_table_selected(build_dir)
        self.assertNotIn('jsrf_vsh_aot', registered_tests(build_dir),
                         'the final OFF configure must not register the corpus-dependent tests')

        # Zero references to the retail corpus in everything the FINAL configure wrote.  The
        # VS generator leaves the project files of targets that no longer exist behind, so
        # every file that still names it must be an ORPHAN: a project file whose target the
        # final configure did not register (CMake's own current-target list is the oracle).
        targets = read_exact(build_dir / 'CMakeFiles' / 'TargetDirectories.txt')
        leftovers = files_mentioning(build_dir, CORPUS_NEEDLE,
                                     skip=(build_dir / 'empty-corpus',))
        orphans = []
        for leftover in leftovers:
            self.assertIn(leftover.suffix.lower(), ('.vcxproj', '.filters'),
                          f'{leftover.relative_to(build_dir)} names the retail corpus and is '
                          f'not a project file of a removed target')
            stem = leftover.name.split('.')[0]
            self.assertNotIn(f'{stem}.dir', targets,
                             f'{stem} is a CURRENT target and still names the retail corpus')
            orphans.append(stem)
        self.assertNotIn('jsrf_recomp', orphans,
                         'the game exe\'s own project must not name the retail corpus')
        print(f'r2 case 5: OFF->ON->OFF rc={first.returncode}/{second.returncode}/'
              f'{third.returncode}; leftover orphan project files: {sorted(set(orphans))}')


class PartialToolchainTests(CorpusCase):
    """Case 6: the loader is there, dxc is not -- a partial toolchain decides OFF."""

    @classmethod
    def setUpClass(cls):
        missing = prerequisites()
        if missing:
            raise unittest.SkipTest('this host lacks: ' + ', '.join(missing))
        cls._tmp = tempfile.TemporaryDirectory(prefix='jsrf-r2-partial-')
        cls.tree = ScratchTree(Path(cls._tmp.name) / 'tree', corpus=False)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def test_a_partial_toolchain_decides_off_and_names_dxc(self):
        absent = self.tree.dir / 'absent' / 'dxc.exe'
        build = self.tree.dir / 'build-partial'
        completed = self.tree.configure('build-partial', (f'-DXBR_DXC={absent}',))
        text = configure_text(completed)
        self.assertEqual(completed.returncode, 0, f'the configure failed:\n{text[-3000:]}')
        cache = build / 'CMakeCache.txt'
        self.assertIn('XBR_VULKAN_FOUND:BOOL=FALSE', read_exact(cache))
        reason = cache_value(cache, 'XBR_VULKAN_UNAVAILABLE_REASON')
        self.assertIn('dxc', reason,
                      f'the reason must name the check that failed: {reason!r}')
        self.assertIn('does not exist', reason)
        self.assertTrue(cache_value(cache, 'Vulkan_LIBRARY'),
                        'the loader WAS found -- this is the partial case, not a missing SDK')
        self.assertNotIn('CMAKE_DISABLE_FIND_PACKAGE_Vulkan', read_exact(cache),
                         'the loader must be discovered normally in this case')
        self.assertIn(flattened(reason), text)
        self.assertIn('-- Vulkan/unavailable:', text)
        self.assert_empty_table_selected(build)
        self.assert_no_corpus_reference(build, 'with a partial toolchain')
        print(f'r2 case 6: partial-toolchain rc={completed.returncode}; reason: {reason}')


# ── the mutants ──────────────────────────────────────────────────────────────────────────
class MutantDetectionTests(unittest.TestCase):
    """The cases above detect the weakenings of the branch they exist for.

    Each mutant is a scratch copy of the SHIPPED `CMakeLists.txt` with one localized
    substitution (or the two FATAL_ERRORs of the capability-ON refusal, for M-B), and the
    focused case runs in a CHILD process with `JSRF_R2_GAME_CMAKE=<mutant>`, so the tree
    that child configures is built from the mutant file and nothing else.  A mutant is
    DETECTED when that child fails.  The control is a comment-only edit, which must stay
    green -- otherwise the harness would be reporting something other than the mutation.
    """

    CASE_OFF = ('tests.test_optional_vulkan.OffWithoutCorpusTests.'
                'test_capability_off_without_a_corpus_compiles_the_generated_empty_table')
    CASE_ON_NOCORPUS = ('tests.test_optional_vulkan.OnWithoutCorpusTests.'
                        'test_capability_on_without_a_corpus_refuses_naming_just_shaders')
    CASE_OFF_WITH_CORPUS = ('tests.test_optional_vulkan.CorpusPresentTests.'
                            'test_capability_off_ignores_a_real_corpus_and_leaves_it_untouched')
    CASE_PARTIAL = ('tests.test_optional_vulkan.PartialToolchainTests.'
                    'test_a_partial_toolchain_decides_off_and_names_dxc')

    MUTANTS = {
        'M-A the missing-corpus FATAL is unconditional again': {
            'subs': [('if(XBR_VULKAN_FOUND AND NOT JSRF_FIXTURES_ONLY)\n'
                      '    # The corpus, generated by scripts/gen-shaders.py',
                      'if(TRUE AND NOT JSRF_FIXTURES_ONLY)\n'
                      '    # The corpus, generated by scripts/gen-shaders.py')],
            'cases': (CASE_OFF, CASE_OFF_WITH_CORPUS, CASE_PARTIAL),
            'why': 'the capability branch is gone, so a build WITHOUT the toolchain demands '
                   'the retail corpus through the R1 FATAL again -- every OFF-mode case must '
                   'turn red',
        },
        'M-B the capability-ON refusals become warnings': {
            'subs': [('    if(NOT RECOMP_SHADER_SOURCES)\n        message(FATAL_ERROR\n',
                      '    if(NOT RECOMP_SHADER_SOURCES)\n        message(WARNING\n'),
                     ('    if(NOT _jsrf_corpus_result EQUAL 0)\n'
                      '        message(FATAL_ERROR\n',
                      '    if(NOT _jsrf_corpus_result EQUAL 0)\n        message(WARNING\n')],
            'cases': (CASE_ON_NOCORPUS,),
            'why': 'a capability-ON configure with no corpus becomes an empty-table success '
                   'instead of the refusal that keeps an empty table out of a real build',
        },
        'M-C the generated empty table declares one program': {
            'subs': [('"const size_t nv2a_vsh_program_count = 0;\\n")',
                      '"const size_t nv2a_vsh_program_count = 1;\\n")')],
            'cases': (CASE_OFF, CASE_OFF_WITH_CORPUS),
            'why': 'the empty mode is corrupted: the table it compiles is no longer empty, '
                   'so the fallback would claim a program it does not have',
        },
        'M-C2 the OFF branch compiles the stale retail corpus too': {
            'subs': [('    set(JSRF_CORPUS_SOURCES "${_jsrf_empty_dir}/nv2a_vsh_programs.c")\n',
                      '    file(GLOB _jsrf_stale_corpus CONFIGURE_DEPENDS '
                      '"src/recomp/gen/shaders/*.c")\n'
                      '    set(JSRF_CORPUS_SOURCES "${_jsrf_empty_dir}/nv2a_vsh_programs.c" '
                      '${_jsrf_stale_corpus})\n')],
            'cases': (CASE_OFF, CASE_OFF_WITH_CORPUS),
            'why': 'the OFF branch globs the stale retail corpus again: CMake\'s own glob '
                   'record and the corpus source list then name src/recomp/gen/shaders in a '
                   'build that must not reference it at all, and with a corpus present the '
                   'exe compiles it as well',
        },
    }

    CONTROL = {
        'subs': [('project(jsrf_recomp C)\n',
                  'project(jsrf_recomp C)  # a comment-only edit: this must stay GREEN\n')],
        'cases': (CASE_OFF,),
    }

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory(prefix='jsrf-r2-mutants-')

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def mutant_file(self, name, subs):
        """A byte copy of the shipped `CMakeLists.txt` with `subs` applied."""
        base = Path(self._tmp.name) / re.sub(r'\W+', '-', name)
        base.mkdir(parents=True, exist_ok=True)
        target = base / 'CMakeLists.txt'
        source = read_exact(SHIPPED_CMAKE)
        for old, new in subs:
            if '\r\n' in source:            # the shipped file is a Windows file
                old, new = old.replace('\n', '\r\n'), new.replace('\n', '\r\n')
            self.assertEqual(source.count(old), 1,
                             f'the mutation anchor for {name!r} is not unique in the shipped '
                             f'CMakeLists.txt ({source.count(old)} occurrences); the shipped '
                             f'code changed shape and this mutant must follow it')
            source = source.replace(old, new)
        write_exact(target, source)
        return target, source

    def run_focused(self, cmake, cases):
        """Run `cases` in a child process whose game CMakeLists.txt is `cmake`."""
        env = base_env()
        env['JSRF_R2_GAME_CMAKE'] = str(cmake)
        return subprocess.run(
            [sys.executable, '-X', 'utf8', '-m', 'unittest', '-v', *cases],
            cwd=str(ROOT), env=env, capture_output=True, text=True, timeout=1800)

    @staticmethod
    def failing_cases(output):
        return [line.strip() for line in output.splitlines()
                if line.startswith('FAIL:') or line.startswith('ERROR:')]

    def verdict(self, name, spec):
        cmake, _ = self.mutant_file(name, spec['subs'])
        completed = self.run_focused(cmake, spec['cases'])
        cases = self.failing_cases(completed.stderr)
        detected = completed.returncode != 0
        detail = ('; '.join(cases) if cases
                  else (completed.stderr.strip().splitlines() or ['-'])[-1])
        print(f'r2 mutant: {name}: detected={"yes" if detected else "NO"} '
              f'(exit {completed.returncode}, {len(cases)} failing case(s): {detail})')
        return detected, cases, completed

    def test_every_mutant_is_detected(self):
        for name, spec in self.MUTANTS.items():
            with self.subTest(mutant=name):
                detected, cases, completed = self.verdict(name, spec)
                self.assertTrue(
                    detected,
                    f'{name} was NOT detected: the focused cases still pass against a tree '
                    f'where {spec["why"]}.\n'
                    f'{completed.stdout[-2000:]}\n{completed.stderr[-2000:]}')
                self.assertTrue(cases, f'{name} failed without naming a case:\n'
                                       f'{completed.stderr[-2000:]}')

    def test_a_comment_only_edit_stays_green(self):
        cmake, source = self.mutant_file('control', self.CONTROL['subs'])
        completed = self.run_focused(cmake, self.CONTROL['cases'])
        print(f'r2 mutant: control (comment-only edit): '
              f'detected={"no" if completed.returncode == 0 else "yes (BROKEN HARNESS)"}')
        self.assertEqual(completed.returncode, 0,
                         f'a comment-only edit turned the cases red, so the harness is '
                         f'reporting something other than the mutation:'
                         f'\n{completed.stdout[-2000:]}\n{completed.stderr[-2000:]}')

    def test_the_mutants_are_copies_with_only_their_edits(self):
        """The substrate: everything but the substitutions is the shipped file, byte for byte."""
        shipped = read_exact(SHIPPED_CMAKE)
        for name, spec in self.MUTANTS.items():
            with self.subTest(mutant=name):
                cmake, source = self.mutant_file(name, spec['subs'])
                self.assertEqual(read_exact(cmake), source)
                self.assertNotEqual(source, shipped)
                reversed_ = source
                for old, new in spec['subs']:
                    if '\r\n' in source:
                        old, new = old.replace('\n', '\r\n'), new.replace('\n', '\r\n')
                    reversed_ = reversed_.replace(new, old)
                self.assertEqual(reversed_, shipped,
                                 f'{name} differs from the shipped file by more than its '
                                 f'{len(spec["subs"])} substitution(s)')


if __name__ == '__main__':
    unittest.main(verbosity=2)
