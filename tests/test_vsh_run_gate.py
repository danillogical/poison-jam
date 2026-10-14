"""Fixtures for `scripts/check-vsh-run.py`, the V5 e2e gate (orchestrator-owned).

The gate's whole point is that "0 misses" is worthless without positive draw
activity, so its failure modes are pinned on synthetic logs: a run that never
submitted a program batch, one where the back end drew none, one where the two
independent counter sources disagree, and one where a miss or an identity failure
is present. No game run is needed.
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / 'scripts' / 'check-vsh-run.py'

_spec = importlib.util.spec_from_file_location('check_vsh_run', CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)

PROG = '[GPU] vertex programs: {b} batches, {v} vertices; 0 triangles dropped behind the eye'
PROG_GPU = ('[GPU]   of those, {g} batches ({t} triangles) drawn by the back end from the'
            ' program, {d} declined to the CPU')
GPUVK = ('[GPUVK] flips=100 draws=1 tris=2 clears=3 validation_errors=0 validation_warnings=0'
         ' cull_all={ca} vsh_hits={h} vsh_misses={m} vsh_identity_fail={i} vsh_declines={dc}'
         ' vsh_tris={t} vsh_const_uploads={c}')


class VshRunGateTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory(prefix='vsh-run-')
        self.run = Path(self._tmp.name) / 'run'
        self.run.mkdir()

    def tearDown(self):
        self._tmp.cleanup()

    def check(self, lines, *extra):
        (self.run / 'jsrf_run.log').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        return subprocess.run([sys.executable, str(CHECKER), str(self.run), *extra],
                              capture_output=True, text=True)

    @staticmethod
    def good():
        return [PROG.format(b=667609, v=365091613), PROG_GPU.format(g=667609, t=365091613, d=0),
                GPUVK.format(ca=0, h=667609, m=0, i=0, dc=0, t=365091613, c=667609)]

    def test_healthy_run_passes(self):
        r = self.check(self.good())
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('gate holds', r.stdout)
        self.assertIn('vsh_hits=667609', r.stdout)

    def test_zero_activity_is_not_a_pass(self):
        """The failure the gate exists for: 0 misses because nothing ran."""
        r = self.check([PROG.format(b=0, v=0), PROG_GPU.format(g=0, t=0, d=0),
                        GPUVK.format(ca=0, h=0, m=0, i=0, dc=0, t=0, c=0)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('submitted no vertex-program batch', r.stdout)
        self.assertIn('vsh_hits=0', r.stdout)

    def test_all_batches_declined_is_not_a_pass(self):
        r = self.check([PROG.format(b=5000, v=100000), PROG_GPU.format(g=0, t=0, d=5000),
                        GPUVK.format(ca=0, h=0, m=0, i=0, dc=5000, t=0, c=0)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('drew 0 of 5000 program batches', r.stdout)

    def test_miss_fails_the_gate(self):
        r = self.check([PROG.format(b=100, v=1000), PROG_GPU.format(g=99, t=900, d=1),
                        GPUVK.format(ca=0, h=99, m=1, i=0, dc=1, t=900, c=99)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('vsh_misses=1, expected 0', r.stdout)

    def test_identity_failure_fails_the_gate(self):
        r = self.check([PROG.format(b=100, v=1000), PROG_GPU.format(g=99, t=900, d=1),
                        GPUVK.format(ca=0, h=99, m=0, i=1, dc=0, t=900, c=99)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('vsh_identity_fail=1', r.stdout)

    def test_disagreeing_sources_fail_the_gate(self):
        """A single spoofed or stale line must not be able to fake a pass."""
        r = self.check([PROG.format(b=10, v=100), PROG_GPU.format(g=10, t=90, d=0),
                        PROG.format(b=100, v=1000), PROG_GPU.format(g=100, t=900, d=0),
                        GPUVK.format(ca=0, h=7, m=0, i=0, dc=0, t=900, c=7)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('the two sources disagree', r.stdout)

    def test_gap_within_one_reporting_step_is_accepted(self):
        """The two sources print on different cadences, so their last lines differ slightly."""
        r = self.check([PROG.format(b=10, v=100), PROG_GPU.format(g=10, t=90, d=0),
                        PROG.format(b=100, v=1000), PROG_GPU.format(g=94, t=900, d=0),
                        GPUVK.format(ca=0, h=100, m=0, i=0, dc=0, t=900, c=100)])
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('within one executor reporting step', r.stdout)

    def test_gap_beyond_one_reporting_step_fails(self):
        """A stale or spoofed back-end line must not be able to fake agreement."""
        r = self.check([PROG.format(b=10, v=100), PROG_GPU.format(g=10, t=90, d=0),
                        PROG.format(b=100, v=1000), PROG_GPU.format(g=100, t=900, d=0),
                        GPUVK.format(ca=0, h=94, m=0, i=0, dc=0, t=900, c=94)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('the back end reports only vsh_hits=94', r.stdout)

    def test_gap_growing_faster_than_the_cadence_fails(self):
        """The other direction: a back-end count ahead of the executor's own reports."""
        r = self.check([PROG.format(b=10, v=100), PROG_GPU.format(g=10, t=90, d=0),
                        PROG.format(b=100, v=1000), PROG_GPU.format(g=100, t=900, d=0),
                        GPUVK.format(ca=0, h=5000, m=0, i=0, dc=0, t=900, c=5000)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('more than one reporting step', r.stdout)

    def test_missing_gpuvk_line_fails_the_gate(self):
        r = self.check([PROG.format(b=100, v=1000), PROG_GPU.format(g=100, t=900, d=0)])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('no [GPUVK] line carried the vsh_ counters', r.stdout)

    def test_fatal_call_fails_the_gate(self):
        lines = self.good() + ['[FATAL] unresolved indirect call 0xE0424943 from sub_00012345']
        r = self.check(lines)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('fatal/unresolved-call line', r.stdout)

    def test_last_lines_win_so_a_killed_run_still_counts(self):
        """A run killed at its deadline prints no exit summary; the last cadence line is the total."""
        lines = [PROG.format(b=10, v=100), PROG_GPU.format(g=10, t=100, d=0),
                 GPUVK.format(ca=0, h=10, m=0, i=0, dc=0, t=100, c=10),
                 PROG.format(b=900, v=9000), PROG_GPU.format(g=900, t=9000, d=0),
                 GPUVK.format(ca=0, h=900, m=0, i=0, dc=0, t=9000, c=900)]
        r = self.check(lines)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn('vsh_hits=900', r.stdout)

    def test_missing_log_is_not_a_pass(self):
        r = subprocess.run([sys.executable, str(CHECKER), str(self.run)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2)
        self.assertIn('is not a run log', r.stdout)


if __name__ == '__main__':
    unittest.main()
