"""Controls for `scripts/generated-parity.py` (plan T15).

T15's acceptance: the audit "finds a seeded one-function change between two
trees", and an overlay restores it. Running the overlaid build is a Windows step;
here the overlay's output is checked to carry the baseline body.
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts' / 'generated-parity.py'

spec = importlib.util.spec_from_file_location('generated_parity', SCRIPT)
parity = importlib.util.module_from_spec(spec)
sys.modules['generated_parity'] = parity   # its dataclass looks its module up
spec.loader.exec_module(parity)


def write_tree(root: Path, bodies: list[tuple[int, str]], ends: dict[int, int] | None = None) -> None:
    root.mkdir(parents=True)
    content = '#define RECOMP_GENERATED_CODE\n#include "recomp_funcs.h"\n\n'
    for address, statement in bodies:
        end = (ends or {}).get(address, address + 0x10)
        content += (f'/**\n * sub_{address:08X}\n'
                    f' * Original: 0x{address:08X} - 0x{end:08X} (16 bytes, 1 insns)\n */\n'
                    f'void sub_{address:08X}(void)\n{{\n    {statement}\n}}\n\n')
    (root / 'recomp_0000.c').write_text(content, encoding='utf-8')


class ParityTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix='jsrf-parity-'))

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_audit_finds_a_seeded_one_function_change(self) -> None:
        base, cand = self.tmp / 'base', self.tmp / 'cand'
        write_tree(base, [(0x1000, 'eax = 1;'), (0x2000, 'eax = 2;')])
        # Whitespace-only differences are not changes; the seeded one is.
        write_tree(cand, [(0x1000, 'eax   = 1;'), (0x2000, 'eax = 3;'), (0x3000, 'return;')])
        report = parity.audit(base, cand)
        self.assertEqual(report['unchanged_function_count'], 1)
        self.assertEqual([c['address'] for c in report['changed']], ['0x00002000'])
        self.assertEqual([a['address'] for a in report['added']], ['0x00003000'])
        self.assertEqual(report['removed'], [])

    def test_a_merged_start_is_subsumed_not_removed(self) -> None:
        base, cand = self.tmp / 'base', self.tmp / 'cand'
        write_tree(base, [(0x1000, 'eax = 1;'), (0x1008, 'eax = 2;')])
        write_tree(cand, [(0x1000, 'eax = 1; eax = 2;')], ends={0x1000: 0x1020})
        report = parity.audit(base, cand)
        self.assertEqual(report['removed'], [])
        self.assertEqual([s['address'] for s in report['subsumed']], ['0x00001008'])

    def test_overlay_restores_the_baseline_body_in_range_only(self) -> None:
        base, cand, out = self.tmp / 'base', self.tmp / 'cand', self.tmp / 'out'
        write_tree(base, [(0x1000, 'eax = 1;'), (0x2000, 'eax = 2;')])
        write_tree(cand, [(0x1000, 'eax = 9;'), (0x2000, 'eax = 3;')])
        manifest = parity.overlay(base, cand, out, 0x1800, 0x2800)
        self.assertEqual(manifest['overlaid'], ['0x00002000'])
        text = (out / 'recomp_0000.c').read_text(encoding='utf-8')
        self.assertIn('eax = 2;', text)          # restored inside the range
        self.assertIn('eax = 9;', text)          # left alone outside it
        self.assertTrue((out / 'overlay-manifest.json').is_file())
        self.assertEqual(parity.audit(base, out)['changed'][0]['address'], '0x00001000')
        # The candidate tree itself is untouched.
        self.assertIn('eax = 3;', (cand / 'recomp_0000.c').read_text(encoding='utf-8'))

    def test_overlay_skips_a_boundary_change(self) -> None:
        base, cand, out = self.tmp / 'base', self.tmp / 'cand', self.tmp / 'out'
        write_tree(base, [(0x1000, 'eax = 1;')])
        write_tree(cand, [(0x1000, 'eax = 5;')], ends={0x1000: 0x1040})
        manifest = parity.overlay(base, cand, out, 0, 0xFFFFFFFF)
        self.assertEqual(manifest['overlaid'], [])
        self.assertEqual(manifest['skipped'], [{'address': '0x00001000', 'reason': 'range-changed'}])

    def test_cli_audit_runs(self) -> None:
        base, cand = self.tmp / 'base', self.tmp / 'cand'
        write_tree(base, [(0x1000, 'eax = 1;')])
        write_tree(cand, [(0x1000, 'eax = 2;')])
        result = subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT), 'audit',
                                 str(base), str(cand)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(json.loads(result.stdout)['changed']), 1)
        self.assertIn('changed=1', result.stderr)

    def test_the_real_tree_loads(self) -> None:
        """Every generated chunk in the repository parses, with one body per address."""
        functions = parity.load_functions(ROOT / 'src' / 'recomp' / 'gen')
        self.assertGreater(len(functions), 1000)


if __name__ == '__main__':
    unittest.main(verbosity=2)
