"""Fixtures for scripts/check-back-edges.py: every lifted loop back edge needs the hook.

The defect this pins: recovery, relift and hidden-entry repair build the translator
without `backedge_yield`, so their back edges are bare `goto loc_X;` and a guest
thread spinning on one never yields the guest-CPU lock.  Each fixture states the
wrong outcome it prevents:

* a bare back edge must FAIL (else the defect passes);
* a hooked one must pass, a forward goto, a goto in a comment and a goto to another
  function's label must be ignored (else a correct tree fails).
"""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'

_spec = importlib.util.spec_from_file_location(
    'check_back_edges', SCRIPTS / 'check-back-edges.py')
assert _spec and _spec.loader
cbe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cbe)

HOOKED = """
void sub_00001000(void)
{
loc_00001000: ;
    eax++;
    if (CMP_EQ(_fa, _fb)) { RECOMP_BACK_EDGE(0x00001008u); goto loc_00001000; } /* je */
}
"""
BARE = HOOKED.replace('{ RECOMP_BACK_EDGE(0x00001008u); goto loc_00001000; }',
                      'goto loc_00001000;')
FORWARD = """
void sub_00002000(void)
{
    if (x) goto loc_00002010;
    eax++;
loc_00002010: ;
}
"""
CROSS_FUNCTION = """
static void body_00003000(void)
{
loc_00003000: ;
    eax++;
}

static void body_00003100(void)
{
    goto loc_00003000;
}
"""
COMMENTED = """
void sub_00004000(void)
{
loc_00004000: ;
    /* goto loc_00004000; */
}
"""


class CheckBackEdges(unittest.TestCase):
    def test_hooked_back_edge_passes(self):
        self.assertEqual(cbe.scan_text(HOOKED), (1, []))

    def test_bare_back_edge_fails(self):
        total, bare = cbe.scan_text(BARE)
        self.assertEqual(total, 1)
        self.assertEqual(len(bare), 1)
        self.assertEqual(bare[0][1:], ('sub_00001000', 0x1000))

    def test_forward_goto_ignored(self):
        self.assertEqual(cbe.scan_text(FORWARD), (0, []))

    def test_goto_to_other_function_label_ignored(self):
        self.assertEqual(cbe.scan_text(CROSS_FUNCTION), (0, []))

    def test_goto_in_comment_ignored(self):
        self.assertEqual(cbe.scan_text(COMMENTED), (0, []))

    def test_main_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            gen = Path(tmp) / 'gen'
            gen.mkdir()
            (Path(tmp) / 'recovered').mkdir()
            self.assertEqual(cbe.main(['--recomp-dir', tmp]), 0)  # no files: SKIP
            (gen / 'recomp_0000.c').write_text(HOOKED, encoding='utf-8')
            self.assertEqual(cbe.main(['--recomp-dir', tmp]), 0)
            (gen / 'recomp_0001.c').write_text(BARE, encoding='utf-8')
            self.assertEqual(cbe.main(['--recomp-dir', tmp]), 1)

    def test_real_spin_loop_is_hooked(self):
        path = ROOT / 'src' / 'recomp' / 'recovered' / 'recovered.c'
        if not path.exists():
            self.skipTest('no generated tree')
        text = path.read_text(encoding='utf-8', errors='replace')
        _, bare = cbe.scan_text(text)
        self.assertIn('goto loc_0013B190', text)
        self.assertEqual([b for b in bare if b[2] == 0x0013B190], [],
                         'the 0x13B1A2 je spin loop has no RECOMP_BACK_EDGE')


if __name__ == '__main__':
    unittest.main()
