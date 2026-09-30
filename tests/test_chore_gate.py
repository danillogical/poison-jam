"""Controls for W7's chore gate.

Plan W7: "**Chore class** in §5.8 for owner-directed mechanical work (§3 above),
with a scripted gate instead of Planner and acceptance review: build, ctest, and a
strict A/B run showing the same stop (**or the change in stop recorded as the
finding**)", answering "the v0.11 sync as packet A4s took 19.6 h and six revisions;
the owner's direct v0.12 sync (121 upstream commits) landed as one merge commit and
one record commit seven minutes apart".

The parenthetical is the load-bearing part and the controls weight it: **a changed
stop is a finding, not a failure.** A gate that failed on a moved stop would push a
session to hide the move, which is the opposite of what the plan asks for. So the
A/B has four verdicts and each is exercised here:

  * `SAME_STOP` -- both sides stopped at the same return address;
  * `STOP_REMOVED` -- the chore moved the horizon PAST the old site (usually
    progress, and calling it merely "changed" would bury the direction);
  * `STOP_APPEARED` -- the horizon moved BACK, which is a regression signal;
  * `NO_STOP_EITHER_SIDE` -- neither run reached an invalid call, so the A/B shows
    nothing about a stop site.

The stop is read from the run's own `jsrf_run.log`, not from a summary, and a chore
without both runs is `INCOMPLETE` rather than `PASS`: "no run" cannot show "the same
stop".
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'chore_gate', ROOT / 'scripts' / 'chore-gate.py')
assert _spec and _spec.loader
gate = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gate)

# The two archived runs this session produced, used as real fixtures.
STRICT_WITH_STOP = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
STRICT_NO_STOP = ROOT / 'logs' / 'runs' / '20260930-034938-427-ttd-exit-control'


def ab_verdict(before: Path, after: Path) -> str:
    """The A/B verdict, computed the way main() computes it."""
    b = gate.stop_of(before)
    a = gate.stop_of(after)
    before_stopped = bool(b.get('found') and b.get('stop'))
    after_stopped = bool(a.get('found') and a.get('stop'))
    if not (b.get('found') and a.get('found')):
        return 'UNKNOWN'
    if not before_stopped and not after_stopped:
        return 'NO_STOP_EITHER_SIDE'
    if before_stopped and not after_stopped:
        return 'STOP_REMOVED'
    if not before_stopped and after_stopped:
        return 'STOP_APPEARED'
    return 'SAME_STOP' if b.get('stop') == a.get('stop') else 'STOP_CHANGED'


class StopReadingTests(unittest.TestCase):
    def test_a_stop_is_read_from_the_log(self) -> None:
        """The recorded strict run ends at `0x0014982E`."""
        if not STRICT_WITH_STOP.is_dir():
            self.skipTest('the archived strict run is not present')
        result = gate.stop_of(STRICT_WITH_STOP)
        self.assertTrue(result['found'])
        self.assertEqual(result['stop']['return_address'], '0014982E')
        self.assertEqual(result['invalid_icalls'], 1)

    def test_a_run_without_a_stop_reports_none(self) -> None:
        """The control run reached its deadline with 0 invalid ICALLs."""
        if not STRICT_NO_STOP.is_dir():
            self.skipTest('the control run is not present')
        result = gate.stop_of(STRICT_NO_STOP)
        self.assertTrue(result['found'])
        self.assertNotIn('stop', result)
        self.assertEqual(result['invalid_icalls'], 0)

    def test_a_missing_log_is_not_a_zero(self) -> None:
        """Absence of a log must not read as absence of a stop."""
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            result = gate.stop_of(Path(temporary))
        self.assertFalse(result['found'])
        self.assertNotIn('stop', result)


class ABVerdictTests(unittest.TestCase):
    def setUp(self) -> None:
        for path in (STRICT_WITH_STOP, STRICT_NO_STOP):
            if not path.is_dir():
                self.skipTest('the archived runs are not present')

    def test_same_run_both_sides_is_same_stop(self) -> None:
        self.assertEqual(ab_verdict(STRICT_WITH_STOP, STRICT_WITH_STOP), 'SAME_STOP')

    def test_a_removed_stop_is_its_own_verdict(self) -> None:
        """The direction matters: the chore moved the horizon PAST the old site."""
        self.assertEqual(ab_verdict(STRICT_WITH_STOP, STRICT_NO_STOP), 'STOP_REMOVED')

    def test_an_appearing_stop_is_its_own_verdict(self) -> None:
        """The reverse direction is a regression signal, not "changed"."""
        self.assertEqual(ab_verdict(STRICT_NO_STOP, STRICT_WITH_STOP), 'STOP_APPEARED')

    def test_no_stop_either_side(self) -> None:
        self.assertEqual(ab_verdict(STRICT_NO_STOP, STRICT_NO_STOP),
                         'NO_STOP_EITHER_SIDE')

    def test_two_different_stops_are_stop_changed(self) -> None:
        """A genuine change: same shape, different return address."""
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, ret in (('a', '0014982E'), ('b', '00147D36')):
                run = root / name
                run.mkdir()
                (run / 'jsrf_run.log').write_text(
                    f'[ICALL] invalid target 0x00000000 tid=1 esp=00F7FD00 '
                    f'return={ret}\n', encoding='utf-8')
            self.assertEqual(ab_verdict(root / 'a', root / 'b'), 'STOP_CHANGED')

    def test_a_changed_stop_is_not_a_failure(self) -> None:
        """W7's parenthetical: the change is the FINDING.

        A gate that failed on a moved stop would push a session to hide the move.
        """
        import subprocess
        import sys
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name, ret in (('a', '0014982E'), ('b', '00147D36')):
                run = root / name
                run.mkdir()
                (run / 'jsrf_run.log').write_text(
                    f'[ICALL] invalid target 0x0 tid=1 esp=0 return={ret}\n',
                    encoding='utf-8')
            result = subprocess.run(
                [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'chore-gate.py'),
                 '--label', 't', '--skip-build', '--skip-ctest',
                 '--before', str(root / 'a'), '--after', str(root / 'b')],
                capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('STOP_CHANGED', result.stdout)
        self.assertIn('RECORD THIS AS THE FINDING', result.stdout)


class IncompletenessTests(unittest.TestCase):
    def test_no_ab_is_incomplete_not_pass(self) -> None:
        """`no run` cannot show `the same stop`."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'chore-gate.py'),
             '--label', 't', '--skip-build', '--skip-ctest'],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('INCOMPLETE', result.stdout)

    def test_a_missing_run_directory_is_incomplete(self) -> None:
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'chore-gate.py'),
             '--label', 't', '--skip-build', '--skip-ctest',
             '--before', 'logs/runs/does-not-exist',
             '--after', 'logs/runs/also-missing'],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('INCOMPLETE', result.stdout)


class WorkflowTests(unittest.TestCase):
    def test_the_workflow_states_the_chore_class(self) -> None:
        """W7 asks for 'Chore class in §5.8', so §5.8 must state it."""
        text = (ROOT / 'docs' / 'agent-workflow.md').read_text(encoding='utf-8')
        self.assertIn('### 5.8 Packet classes', text)
        self.assertIn('**Chore**', text)
        self.assertIn('A chore needs no packet', text)
        # The boundary that keeps the class from swallowing change packets.
        self.assertIn('may not change admitted evidence semantics', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
