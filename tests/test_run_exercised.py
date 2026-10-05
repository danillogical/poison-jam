"""Controls for the run-coverage check (Reviewer finding B3).

**The measured failure this pins.** This session recorded `0x00032610` as
"run-confirmed" by run `f25`, which never dispatched the address: f25's log has
zero `[RECOVERED] 0x00032610 returned; ABI verified` lines, and all 14 textual
`32610` matches were its own directory name in `[SAVE]`/`[FBWIN]` path strings. A
clean run that never reaches the changed code proves nothing, and the error
survived three commits before the Turn Reviewer caught it.

**What these controls check.** That the checker distinguishes "the run returned
from this entry" from "the run merely mentions this address". The deciding control
is the pair of real archived runs: `f25` must FAIL for `0x00032610` and `f27` must
PASS, on the same checker, with no special-casing. If both passed, the check would
be matching text rather than a return event, which is exactly the defect.
"""
from __future__ import annotations

import importlib.util
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNS = ROOT / 'logs' / 'runs'
CHECKER = ROOT / 'scripts' / 'check-run-exercised.py'

_spec = importlib.util.spec_from_file_location('check_run_exercised', CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)

F25 = RUNS / '20261005-034216-016-f25-32610'
F27 = RUNS / '20261005-110711-776-f27-verify'


class RegexTests(unittest.TestCase):
    def test_a_returned_line_is_read(self) -> None:
        line = '[RECOVERED] 0x0013B750 returned; ABI verified (ESP/EBX/ESI/EDI)'
        self.assertEqual(checker.RETURNED.findall(line), ['0013B750'])

    def test_a_directory_name_is_not_a_return(self) -> None:
        """The exact B3 failure: a path containing the address is not an event."""
        line = '  [SAVE] \\Device\\Harddisk0\\partition1\\...\\f25-32610\\save-root'
        self.assertEqual(checker.RETURNED.findall(line), [])
        self.assertEqual(checker.RETURNED.findall('f25-32610'), [])

    def test_an_alias_icall_line_is_read(self) -> None:
        line = '[ALIAS-ICALL] target=0x00032610 owner=0x00033800'
        self.assertEqual(checker.ALIAS_ICALL.findall(line), [('00032610', '00033800')])

    def test_normalise_pads_to_eight_digits(self) -> None:
        self.assertEqual(checker.normalise('0x32610'), '0x00032610')
        self.assertEqual(checker.normalise('0x00032610'), '0x00032610')


@unittest.skipUnless(F25.is_dir() and F27.is_dir(),
                     'the f25/f27 run archives are not present')
class RealRunControlTests(unittest.TestCase):
    """The deciding control: the checker must separate the two real runs."""

    def run_check(self, run: Path, *addresses: str) -> subprocess.CompletedProcess:
        return subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), str(run), *addresses],
            capture_output=True, text=True, cwd=str(ROOT))

    def test_f25_does_not_exercise_0x32610(self) -> None:
        """f25 is the run that was wrongly credited. It must FAIL."""
        result = self.run_check(F25, '0x32610')
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn('was NOT exercised', result.stdout)

    def test_f27_does_exercise_0x32610(self) -> None:
        """f27 is the genuine confirmation. It must PASS."""
        result = self.run_check(F27, '0x32610')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('was exercised', result.stdout)

    def test_the_two_runs_disagree_so_the_check_is_not_a_text_match(self) -> None:
        """If both runs agreed, the check would not be reading a return event."""
        a = self.run_check(F25, '0x32610').returncode
        b = self.run_check(F27, '0x32610').returncode
        self.assertNotEqual(a, b)

    def test_f28_exercises_0x54750(self) -> None:
        run = RUNS / '20261005-111824-944-f28-batch'
        if not run.is_dir():
            self.skipTest('f28 archive not present')
        result = self.run_check(run, '0x54750')
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class ArgumentTests(unittest.TestCase):
    def test_a_missing_run_directory_fails_closed(self) -> None:
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), 'logs/runs/does-not-exist',
             '0x11000'],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(result.returncode, 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
