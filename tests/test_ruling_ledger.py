"""Controls for W6's ruling-ledger lint.

Plan W6: "Ruling ledger per question (`docs/reviews/rulings/<question>.md`): a new
ruling **lists the hazards of earlier rulings it supersedes**", answering "A2h
page-guard rejected at 00:38, adopted at 09:32, then failed on the hazard cited at
00:38".

The failure is not that a ruling was wrong. It is that the information existed in one
decision and was lost before the next, because nothing required the second decision
to read the first. So the lint checks that the four facts a later reader needs are
PRESENT -- not that the ruling is correct, which is a §2.3 judgement.

Every rule gets a case that must fire and one that must not: a lint that flagged
every ruling would be red from its first run, and one that flagged none would pass
the positive cases while catching nothing.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_ruling_ledger', ROOT / 'scripts' / 'check-ruling-ledger.py')
assert _spec and _spec.loader
ledger = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ledger)


COMPLETE = """# Ruling: a question

## Ruling text

RULING: the thing is admitted under these conditions.

## Basis

- **Observed:** the measurement.
- **Inferred:** the consequence.
- **Uncertain:** what is not known.

## Reversed by

- A control showing the measurement was wrong.

## Recorded in

- `docs/jsrf-run-profiles.md` §"The rule".
"""


class RulingLintTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs' / 'reviews' / 'rulings').mkdir(parents=True)
        self._saved = ledger.ROOT
        ledger.ROOT = self.root

    def tearDown(self) -> None:
        ledger.ROOT = self._saved
        self._tmp.cleanup()

    def audit(self, body: str, name: str = 'q.md') -> list[dict]:
        path = self.root / 'docs' / 'reviews' / 'rulings' / name
        path.write_text(body, encoding='utf-8')
        return ledger.audit(path)

    def reasons(self, findings: list[dict]) -> set[str]:
        return {f['reason'] for f in findings}


class RequiredFactsTests(RulingLintTests):
    def test_a_complete_ruling_passes(self) -> None:
        self.assertEqual(self.audit(COMPLETE), [])

    def test_a_missing_reversal_condition_is_flagged(self) -> None:
        body = COMPLETE.replace('## Reversed by\n\n- A control showing the '
                                'measurement was wrong.\n\n', '')
        self.assertIn('no_reversal_condition', self.reasons(self.audit(body)))

    def test_a_missing_basis_is_flagged(self) -> None:
        body = COMPLETE.replace('## Basis\n\n- **Observed:** the measurement.\n'
                                '- **Inferred:** the consequence.\n'
                                '- **Uncertain:** what is not known.\n\n', '')
        self.assertIn('no_basis', self.reasons(self.audit(body)))

    def test_a_missing_owning_document_is_flagged(self) -> None:
        body = COMPLETE.replace('## Recorded in\n\n- `docs/jsrf-run-profiles.md` '
                                '§"The rule".\n', '')
        self.assertIn('no_owning_document', self.reasons(self.audit(body)))

    def test_the_basis_accepts_the_inline_vocabulary(self) -> None:
        """§2.4.1's vocabulary counts whether or not it is a heading."""
        body = ('# Ruling\n\nObserved: the measurement. Inferred: the rest.\n\n'
                '## Reversed by\n\n- evidence\n\n## Recorded in\n\n- docs/x.md\n')
        self.assertNotIn('no_basis', self.reasons(self.audit(body)))


class SupersessionTests(RulingLintTests):
    def test_superseding_without_hazards_is_flagged(self) -> None:
        """W6's specific requirement, and the A2h failure it answers."""
        body = COMPLETE + '\nThis ruling supersedes the earlier page-guard ruling.\n'
        self.assertIn('supersedes_without_hazards', self.reasons(self.audit(body)))

    def test_superseding_with_hazards_passes(self) -> None:
        body = COMPLETE + (
            '\nThis ruling supersedes the earlier page-guard ruling. Its hazards, '
            'carried forward: the guard could fire on an unrelated page.\n')
        self.assertNotIn('supersedes_without_hazards', self.reasons(self.audit(body)))

    def test_a_ruling_that_supersedes_nothing_is_not_asked_for_hazards(self) -> None:
        """A first ruling has no predecessor, and demanding hazards would be noise."""
        self.assertEqual(self.audit(COMPLETE), [])

    def test_the_word_supersede_is_matched_in_its_forms(self) -> None:
        for form in ('supersedes', 'superseding', 'superseded'):
            self.assertTrue(ledger.SUPERSEDES.search(f'This {form} the earlier one'))


class RealLedgerTests(unittest.TestCase):
    def test_the_delivered_ledger_passes(self) -> None:
        """The W11 ruling is the ledger's first entry and must satisfy its own lint."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-ruling-ledger.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('no findings', result.stdout)

    def test_the_ledger_has_at_least_one_ruling(self) -> None:
        """A lint over an empty directory reports 'no findings' vacuously."""
        self.assertGreaterEqual(len(ledger.ruling_files()), 1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
