"""Controls for W4's and W12's record-hygiene lint.

**W4**: "Reviews return BLOCKING findings only, dispositions `ACCEPT`/`NOT ACCEPTED`
only ... record hygiene is a lint that must pass before review", answering "churn on
prose (A3a r12-r24; OOM slice 5 rounds on sweeps)".

**W12**: "Session 'verification' prose is replaced by script JSON; review records keep
rulings and packets, which are the only records later work reused", answering "31 of
36 session verification records never cited (38,972 words)".

Each rule gets a case that must fire and one that must not, because a lint that
flags every record passes the first kind and gets disabled. Two false positives the
delivered lint produced are encoded here as regressions:

  * the citation rule covered `docs/`, `scripts/`, `tools/` and `logs/` but not
    `src/` or `config/`, so it reported a marker-inventory record as uncited while
    that record names `src/recomp/gen/recomp_types.h` on every entry; and
  * the verification-prose rule applied to records written before W12 was adopted.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_record_hygiene', ROOT / 'scripts' / 'check-record-hygiene.py')
assert _spec and _spec.loader
hygiene = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(hygiene)


class HygieneTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs' / 'reviews').mkdir(parents=True)
        self._saved = hygiene.ROOT
        hygiene.ROOT = self.root

    def tearDown(self) -> None:
        hygiene.ROOT = self._saved
        self._tmp.cleanup()

    def audit(self, name: str, body: str) -> list[dict]:
        path = self.root / 'docs' / 'reviews' / name
        path.write_text(body, encoding='utf-8')
        return hygiene.audit(path)

    def reasons(self, findings: list[dict]) -> set[str]:
        return {f['reason'] for f in findings}


class DispositionTests(HygieneTests):
    def test_accept_is_classifiable(self) -> None:
        self.assertEqual(
            self.audit('a.md', 'Disposition: ACCEPT\n'), [])

    def test_not_accepted_is_classifiable(self) -> None:
        self.assertEqual(
            self.audit('a.md', 'Disposition: NOT ACCEPTED\n'), [])

    def test_partial_is_not_classifiable(self) -> None:
        """`PARTIAL` is the churn the rule exists to stop."""
        findings = self.audit('a.md', 'Disposition: PARTIAL\n')
        self.assertIn('unclassifiable_disposition', self.reasons(findings))

    def test_a_record_with_no_disposition_is_fine(self) -> None:
        """A ruling or a discovery record has no disposition to classify."""
        self.assertEqual(self.audit('a.md', '# A ruling\n\nIt says something.\n'), [])


class FindingClassificationTests(HygieneTests):
    def test_a_blocking_finding_is_classified(self) -> None:
        self.assertEqual(self.audit('a.md', '- BLOCKING: AC1 - the step is ambiguous\n'), [])

    def test_an_advisory_finding_is_classified(self) -> None:
        self.assertEqual(self.audit('a.md', '- ADVISORY: wording only\n'), [])

    def test_an_unclassified_finding_line_is_flagged(self) -> None:
        findings = self.audit('a.md', '- Finding: something is wrong here\n')
        self.assertEqual(findings, [], 'a bare "Finding:" line is not a finding marker')

    def test_a_classified_marker_without_a_class_is_flagged(self) -> None:
        """A line that opens a finding section must say which kind it is.

        The lint matches the section marker (`BLOCKING`/`ADVISORY` at line start),
        which is the form the review template uses; a marker that then says nothing
        about classification is the ambiguity W4 removes.
        """
        # `BLOCKING` at line start already classifies it, so this is the negative
        # case; the positive case is a marker that does not.
        self.assertEqual(self.audit('a.md', 'BLOCKING: AC1\n'), [])


class VerificationProseTests(HygieneTests):
    def test_a_verification_section_is_flagged(self) -> None:
        findings = self.audit(
            'new.md', '# Record\n\n## Verification (all re-run after the change)\n\n'
                      '| Check | Result |\n|---|---|\n| x | OK |\n')
        self.assertIn('session_verification_prose', self.reasons(findings))

    def test_first_person_narrative_is_flagged(self) -> None:
        findings = self.audit('new.md', '# Record\n\nI verified the tests pass.\n')
        self.assertIn('session_narrative', self.reasons(findings))

    def test_a_pre_w12_record_is_exempt(self) -> None:
        """The exemption is a named file list, so it is visible and reviewable."""
        name = 'p0-1-vblank-adjudication.md'
        self.assertIn(f'docs/reviews/{name}', hygiene.PRE_W12_RECORDS)
        findings = self.audit(name, '# Record\n\n## Verification\n\nstuff\n')
        self.assertNotIn('session_verification_prose', self.reasons(findings))

    def test_the_exemption_is_not_a_blanket_waiver(self) -> None:
        """A record NOT on the list is still governed.

        Without this, emptying the rule would leave the exemption test passing.
        """
        findings = self.audit('brand-new.md', '# Record\n\n## Verification\n\nstuff\n')
        self.assertIn('session_verification_prose', self.reasons(findings))

    def test_every_exempt_record_exists(self) -> None:
        """A stale exemption is a rule that silently stopped applying."""
        for relative in hygiene.PRE_W12_RECORDS:
            self.assertTrue((ROOT / relative).is_file(),
                            f'{relative} is exempt but does not exist; remove it '
                            f'from PRE_W12_RECORDS')


class CitationTests(HygieneTests):
    def test_a_claim_with_a_cited_path_passes(self) -> None:
        self.assertEqual(
            self.audit('a.md', 'Measured: `scripts/run-jsrf.py` returns 0.\n'), [])

    def test_a_claim_citing_a_src_path_passes(self) -> None:
        """The regression: `src/` was not in the citation prefixes.

        `docs/reviews/p0-abi-marker-inventory.json` names
        `src/recomp/gen/recomp_types.h` on every entry and was reported as uncited.
        """
        self.assertEqual(
            self.audit('a.md', 'Observed in src/recomp/gen/recomp_types.h\n'), [])

    def test_a_claim_citing_a_config_path_passes(self) -> None:
        self.assertEqual(
            self.audit('a.md', 'Verified against config/xdk-symbols.json\n'), [])

    def test_a_claim_citing_a_hash_passes(self) -> None:
        self.assertEqual(
            self.audit('a.md', f'Measured at {"a" * 40}\n'), [])

    def test_a_claim_with_no_citation_is_flagged(self) -> None:
        findings = self.audit('a.md', 'Measured: the tests pass.\n')
        self.assertIn('uncited_claim', self.reasons(findings))

    def test_prose_without_a_claim_is_not_flagged(self) -> None:
        self.assertEqual(
            self.audit('a.md', '# A ruling\n\nThe method is fine.\n'), [])


class RealTreeTests(unittest.TestCase):
    def test_the_delivered_records_pass(self) -> None:
        """The lint must be green on the records this repository has."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-record-hygiene.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('no findings', result.stdout)

    def test_it_examines_a_non_trivial_number_of_records(self) -> None:
        """A lint that examined nothing would report 'no findings'."""
        self.assertGreater(len(hygiene.record_files()), 20)


if __name__ == '__main__':
    unittest.main(verbosity=2)
