"""Controls for W1's repeated-criterion-ID check.

Plan W1: "the same criterion ID blocking two consecutive INADEQUATE verdicts forces
redesign or a discovery packet; only the Advisor exempts".

**Why the controls are fixtures rather than the repository's own records.** Measured:
this repository has **no** `docs/reviews/<packet>-r<N>-*.md` files and no
`*-revision-history.md` files on disk -- the round-by-round records the rule is about
were retired with their packets and survive only in git history. A check exercised
only against the current tree would examine zero packets and pass, which is
indistinguishable from a check that cannot fail. So the controls build the records
the rule is written for.

Each control pairs a case that must fire with one that must not, because a checker
that flags every repeated word passes the first kind and is useless.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_review_recurrence', ROOT / 'scripts' / 'check-review-recurrence.py')
assert _spec and _spec.loader
recurrence = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(recurrence)


class CriterionParsingTests(unittest.TestCase):
    def test_the_forms_this_project_uses_are_recognised(self) -> None:
        for text in ('AC-1', 'AC1', 'AC-BOOT', 'P0.2-AC1', 'A4b2-AC-BOOT'):
            self.assertTrue(recurrence.CRITERION.search(text),
                            f'{text!r} should parse as a criterion id')

    def test_an_advisory_line_is_not_blocking(self) -> None:
        self.assertTrue(recurrence.NON_BLOCKING.search(
            'AC-1: advisory only, deferred to the next revision'))
        self.assertTrue(recurrence.NON_BLOCKING.search(
            'AC-2 is non-blocking'))

    def test_a_blocking_line_is_blocking(self) -> None:
        self.assertFalse(recurrence.NON_BLOCKING.search(
            'AC-1: BLOCKING - the criterion cannot be evaluated'))


class RecurrenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs' / 'reviews').mkdir(parents=True)
        self._saved = (recurrence.ROOT, recurrence.REVIEW_DIRS)
        recurrence.ROOT = self.root

    def tearDown(self) -> None:
        recurrence.ROOT, recurrence.REVIEW_DIRS = self._saved
        self._tmp.cleanup()

    def write(self, name: str, body: str) -> Path:
        path = self.root / 'docs' / 'reviews' / name
        path.write_text(body, encoding='utf-8')
        return path

    def findings(self) -> list[dict]:
        packets = recurrence.revision_records()
        results = []
        for packet, records in sorted(packets.items()):
            if len(records) < 2:
                continue
            previous = None
            previous_blocking: set[str] = set()
            exempted: set[str] = set()
            for record in records:
                blocking, exemptions = recurrence.blocking_criteria(record)
                for line in exemptions:
                    exempted.update(recurrence.CRITERION.findall(line))
                if (previous is not None
                        and previous['revision'] == record['revision'] - 1):
                    for criterion in sorted((previous_blocking & blocking) - exempted):
                        results.append({'packet': packet, 'criterion': criterion})
                previous = record
                previous_blocking = blocking
        return results

    def test_the_same_criterion_blocking_twice_is_a_finding(self) -> None:
        """The plan's named case: AC2 blocking r6 and r7."""
        self.write('a3a-r6-review.md', 'BLOCKING: AC2 - the body is unverified\n')
        self.write('a3a-r7-review.md', 'BLOCKING: AC2 - still unverified\n')
        self.assertEqual([f['criterion'] for f in self.findings()], ['AC2'])

    def test_different_criteria_are_not_a_finding(self) -> None:
        """A revision that fixes AC2 and fails AC3 is progress, not a loop."""
        self.write('a3a-r6-review.md', 'BLOCKING: AC2 - unverified\n')
        self.write('a3a-r7-review.md', 'BLOCKING: AC3 - different problem\n')
        self.assertEqual(self.findings(), [])

    def test_a_non_adjacent_recurrence_is_not_reported(self) -> None:
        """The rule is CONSECUTIVE verdicts; r6 and r9 is not the pattern."""
        self.write('a3a-r6-review.md', 'BLOCKING: AC2 - unverified\n')
        self.write('a3a-r7-review.md', 'BLOCKING: AC3 - different\n')
        self.write('a3a-r9-review.md', 'BLOCKING: AC2 - back again\n')
        self.assertEqual(self.findings(), [])

    def test_an_advisor_exemption_suppresses_the_finding(self) -> None:
        """`only the Advisor exempts` -- and the exemption must be traceable."""
        self.write('a3a-r6-review.md', 'BLOCKING: AC2 - unverified\n')
        self.write('a3a-r7-review.md',
                   'BLOCKING: AC2 - unverified\n'
                   'AC2 exempt: Advisor ruling 2026-09-30 recorded in '
                   'docs/reviews/rulings/x.md\n')
        self.assertEqual(self.findings(), [])

    def test_a_non_advisor_exemption_does_not_suppress(self) -> None:
        """A self-granted exemption must not count.

        Without this control, deleting the Advisor requirement would leave the
        exemption test passing.
        """
        self.write('a3a-r6-review.md', 'BLOCKING: AC2 - unverified\n')
        self.write('a3a-r7-review.md',
                   'BLOCKING: AC2 - unverified\n'
                   'AC2 exempt: the Session decided it was fine\n')
        self.assertEqual([f['criterion'] for f in self.findings()], ['AC2'])

    def test_an_advisory_repeat_is_not_a_finding(self) -> None:
        """An advisory that recurs is deferred, not a redesign signal."""
        self.write('a3a-r6-review.md', 'AC2: advisory, wording only\n')
        self.write('a3a-r7-review.md', 'AC2: advisory, wording only\n')
        self.assertEqual(self.findings(), [])

    def test_a_history_file_yields_one_entry_per_revision(self) -> None:
        """The other naming convention this repository uses."""
        self.write('a3a-revision-history.md',
                   '# Revision history\n\n'
                   '## r6\nBLOCKING: AC2 - unverified\n\n'
                   '## r7\nBLOCKING: AC2 - still unverified\n')
        packets = recurrence.revision_records()
        self.assertIn('a3a', packets)
        self.assertEqual([r['revision'] for r in packets['a3a']], [6, 7])
        self.assertEqual([f['criterion'] for f in self.findings()], ['AC2'])

    def test_a_history_file_with_no_revision_headings_is_still_examined(self) -> None:
        """A file with no `## rN` heading must not be silently dropped."""
        self.write('a3a-revision-history.md', 'BLOCKING: AC2 - unverified\n')
        packets = recurrence.revision_records()
        self.assertIn('a3a', packets)
        self.assertEqual(len(packets['a3a']), 1)


class RealTreeTests(unittest.TestCase):
    def test_the_check_runs_on_the_real_tree(self) -> None:
        """It must run clean; the tree legitimately has no revision records."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-review-recurrence.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('checker', result.stdout)

    def test_the_real_tree_has_no_revision_records_to_examine(self) -> None:
        """Recorded, because it is why the controls are fixtures.

        If this ever starts failing, real records have appeared and the controls
        should be re-pointed at them.
        """
        packets = recurrence.revision_records()
        self.assertEqual(packets, {},
                         'revision records now exist on disk; re-point the '
                         'controls at them instead of fixtures')


if __name__ == '__main__':
    unittest.main(verbosity=2)
