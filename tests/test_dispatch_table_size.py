"""Controls for `scripts/check-dispatch-table.py`.

The gate exists because a real host crash shipped through a green suite: a patch
removed one row from `g_recomp_table[]` and left the declared count one too high,
so `recomp_dispatch_init()` read past the array and the process died with an
access violation **before `guest_entry`**.

A checker that cannot fail is not a checker. Every control here builds a scratch
dispatch unit in a temporary directory and runs the real script against it, so
nothing in the live tree is touched and the failure path is exercised end to end.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts' / 'check-dispatch-table.py'


def unit(rows: list[int], size_expr: str, base: int = 0x00011000,
         span: int | None = None) -> str:
    """A minimal dispatch unit in the generator's real shape."""
    if span is None:
        span = (max(rows) - base + 1) if rows else 0
    body = '\n'.join(f'    {{ 0x{va:08X}u, (recomp_func_t)sub_{va:08X} }},' for va in rows)
    return (
        'static const recomp_entry_t g_recomp_table[] = {\n'
        f'{body}\n'
        '};\n'
        '\n'
        f'static const size_t g_recomp_table_size = {size_expr};\n'
        '\n'
        f'static const uint32_t g_flat_base = 0x{base:08X}u;\n'
        f'static const uint32_t g_flat_span = 0x{span:08X}u;\n'
    )


DERIVED = 'sizeof(g_recomp_table) / sizeof(g_recomp_table[0])'
THREE = [0x00011000, 0x00011040, 0x00011080]


class DispatchTableControls(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = Path(tempfile.mkdtemp(prefix='jsrf-disptable-'))
        self.addCleanup(shutil.rmtree, self.tmp, True)

    def run_on(self, text: str) -> subprocess.CompletedProcess:
        path = self.tmp / 'recomp_dispatch.c'
        path.write_text(text, encoding='utf-8')
        return subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT), str(path)],
                              capture_output=True, text=True)

    # -- the gate must be able to fail ------------------------------------

    def test_declared_size_one_too_high_is_the_shipped_crash(self):
        """The exact g08/g08b defect: 8928 declared, 8927 rows."""
        rows = [0x00011000 + 4 * i for i in range(8)]
        rows = rows[:-1]                       # one row removed by a patch
        text = unit(rows, str(len(rows) + 1))  # ...and the count left alone
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('declared g_recomp_table_size = 8', r.stderr)
        self.assertIn('holds 7 row(s)', r.stderr)
        self.assertIn('past the end', r.stderr)

    def test_declared_size_one_too_low_fails_too(self):
        """The other direction: a row added without the count."""
        text = unit(THREE, str(len(THREE) - 1))
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('holds 3 row(s)', r.stderr)

    def test_duplicate_va_fails(self):
        text = unit([0x00011000, 0x00011040, 0x00011040], '3')
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('duplicate VA', r.stderr)

    def test_unsorted_table_fails(self):
        text = unit([0x00011040, 0x00011000, 0x00011080], '3')
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('not sorted ascending', r.stderr)

    def test_flat_span_too_small_fails(self):
        text = unit(THREE, '3', span=0x10)
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('g_flat_span', r.stderr)

    def test_unevaluable_count_is_not_a_pass(self):
        """A count this checker cannot read must not pass as though checked."""
        text = unit(THREE, 'compute_count()')
        r = self.run_on(text)
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('neither an integer literal nor the derived', r.stderr)

    def test_missing_table_fails(self):
        r = self.run_on('int unrelated;\n')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('no `g_recomp_table[]', r.stderr)

    # -- ...and must pass on the real, correct shapes ---------------------

    def test_derived_size_passes(self):
        r = self.run_on(unit(THREE, DERIVED))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('PASS', r.stdout)

    def test_correct_literal_passes(self):
        r = self.run_on(unit(THREE, '3'))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_derived_size_with_reformatted_whitespace_passes(self):
        """The generator wraps the expression across two lines."""
        text = unit(THREE, '\n    sizeof(g_recomp_table) / sizeof(g_recomp_table[0])')
        r = self.run_on(text)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_live_tree_passes(self):
        """The durable tree must satisfy the gate, not merely the fixtures."""
        path = ROOT / 'src' / 'recomp' / 'gen' / 'recomp_dispatch.c'
        if not path.is_file():
            self.skipTest('generated dispatch unit is not present')
        r = subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT)],
                           capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_live_tree_derives_its_count(self):
        """The live tree must use the derived form, not a literal.

        This is the durable property: a literal can drift from the array again,
        which is exactly how the crash shipped.
        """
        path = ROOT / 'src' / 'recomp' / 'gen' / 'recomp_dispatch.c'
        if not path.is_file():
            self.skipTest('generated dispatch unit is not present')
        text = path.read_text(encoding='utf-8', errors='replace')
        self.assertIn('sizeof(g_recomp_table) / sizeof(g_recomp_table[0])', text,
                      'the live dispatch table still declares its size as a literal')


if __name__ == '__main__':
    unittest.main(verbosity=2)
