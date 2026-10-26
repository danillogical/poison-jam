"""Regressions for the host tools: the toolkit resolver and the limited/audit/tk-tests helpers.

The resolver is checked against throwaway game roots, so the shared toolkit is never touched.
"""
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import toolkit_path as tp  # noqa: E402


def load(name):
    spec = importlib.util.spec_from_file_location(name.replace('-', '_'), SCRIPTS / f'{name}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ResolverTest(unittest.TestCase):
    def setUp(self):
        self._saved = os.environ.pop('XBOXRECOMP_DIR', None)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name).resolve()
        self.game = base / 'game'
        (self.game / 'build').mkdir(parents=True)
        self.sibling = base / 'xboxrecomp'
        self.other = base / 'other'
        for d in (self.sibling, self.other):
            d.mkdir()

    def tearDown(self):
        os.environ.pop('XBOXRECOMP_DIR', None)
        if self._saved is not None:
            os.environ['XBOXRECOMP_DIR'] = self._saved

    def cache(self, value):
        (self.game / 'build' / 'CMakeCache.txt').write_text(
            f'// Path to the toolkit\nXBOXRECOMP_DIR:PATH={value}\n')

    def resolve(self, **kw):
        return tp.toolkit_path(game_root=self.game, **kw)

    def test_sibling_when_nothing_else_speaks(self):
        self.assertEqual(self.resolve(), self.sibling)

    def test_environment_beats_sibling(self):
        os.environ['XBOXRECOMP_DIR'] = str(self.other)
        self.assertEqual(self.resolve(), self.other)

    def test_cache_beats_sibling_and_agreeing_environment_is_fine(self):
        self.cache(f'{self.game}/../other')
        self.assertEqual(self.resolve(), self.other)
        os.environ['XBOXRECOMP_DIR'] = str(self.other)
        self.assertEqual(self.resolve(with_label=True)[0], self.other)

    def test_cache_and_environment_that_disagree_are_refused(self):
        self.cache(self.sibling)
        os.environ['XBOXRECOMP_DIR'] = str(self.other)
        with self.assertRaises(tp.ToolkitPathError) as caught:
            self.resolve()
        self.assertIn(str(self.other), str(caught.exception))
        self.assertIn(str(self.sibling), str(caught.exception))

    def test_similarly_named_cache_variable_is_ignored(self):
        (self.game / 'build' / 'CMakeCache.txt').write_text(
            f'XBOXRECOMP_DIRECTORY:PATH={self.other}\n')
        self.assertEqual(self.resolve(), self.sibling)


class HelperTest(unittest.TestCase):
    def test_ctest_limited_refuses_alternation_in_any_regex_spelling(self):
        mod = load('ctest-limited')
        for args in (['-R', 'a|b'], ['-Ra|b'], ['--tests-regex', 'a|b'], ['--tests-regex=a|b']):
            self.assertEqual(mod.alternation_in_regex(args), 'a|b', args)
        self.assertIsNone(mod.alternation_in_regex(['-R', '^jsrf_texgen_stream_vk_']))
        self.assertIsNone(mod.alternation_in_regex(['-E', 'a|b']))
        self.assertEqual(mod.main(['-R', 'a|b']), 2)

    def test_audit_forbids_what_agents_never_tracks(self):
        mod = load('outgoing-audit')
        for path in ('game/default.xbe', 'src/recomp/gen/recomp_0001.c',
                     'src/recomp/recovered/x.c', 'src/recomp/gen/shaders/nv2a_vsh_programs.c',
                     'hdd/Partition1.img', 'tests/x/boot.XBE', 'saves/a.sav'):
            self.assertIsNotNone(mod.forbidden(path), path)
        for path in ('scripts/run-limited.py', 'src/recomp_manual.c', 'docs/game.md',
                     'tests/test_host_tools.py'):
            self.assertIsNone(mod.forbidden(path), path)

    def test_tk_tests_reads_fork_status_sections(self):
        mod = load('tk-tests')
        text = ('# T\n\n## Upstream tests of a design\n\n| Project | What |\n|---|---|\n'
                '| `vblank_ack` | link |\n\n## Windows refuses them by design\n\n'
                '`a_posix`, `b_posix`: POSIX-only.\n`c_imports`: needs objdump.\n\n'
                '## Failing on `main`\n\n`adpcm`, `mix`: error `gp_ep.h`. (`apu_vp`, upstream\'s)\n')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'FORK-STATUS.md'
            path.write_text(text)
            projects = ['vblank_ack', 'a_posix', 'b_posix', 'c_imports', 'adpcm', 'mix',
                        'apu_vp', 'passing']
            self.assertEqual(mod.parse_fork_status(path, projects),
                             {'vblank_ack': 'b', 'a_posix': 'c', 'b_posix': 'c',
                              'c_imports': 'c', 'adpcm': 'd', 'mix': 'd'})

    def test_run_limited_returns_the_commands_exit_code_and_output(self):
        r = subprocess.run(
            [sys.executable, str(SCRIPTS / 'run-limited.py'), '--timeout', '120', '--env',
             'RL_PROBE=ok%PATH%', '--', 'python', '-c',
             'import os,sys; print(os.environ["RL_PROBE"]); sys.exit(3)'],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 3, r.stdout + r.stderr)
        self.assertIn('ok%PATH%', r.stdout)


if __name__ == '__main__':
    unittest.main()
