"""Controls for W5's transcribed-value re-check.

Plan W5: "Values transcribed by hand into a table or record, by any role, are
re-checked row by row by a second DeepSeek worker against the artifact before a
criterion or ruling uses them (until T10 makes tool citation the only path)",
answering "12 recorded extraction errors; a byte reversal that became a 'permanent'
refutation for 35 minutes".

**The check decides one case and says so.** It can compare a record's hex literals
against the text artifacts the record cites. It **cannot** reproduce a value derived
by analysis -- a disassembly of `game/default.xbe`, a debugger read of a TTD trace, a
register observed in a stack dump -- because the artifact is a binary and the value
came from a decoder. Measured: the first version flagged
`docs/reviews/ttd-recording-exit-finding.md` for `0x00196A29`, which that record
derived by disassembling the XBE. So an undecidable value is reported as `UNKNOWN`,
never as a finding, and a control asserts that.

Two false positives the controls found are encoded here as regressions:

  * `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`) named as an observed exit code;
  * `0x00011000`, the project's DOCUMENTED dump-control address (AGENTS.md).
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_transcribed_values', ROOT / 'scripts' / 'check-transcribed-values.py')
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)


class KnownConstantTests(unittest.TestCase):
    def test_documented_constants_are_recognised(self) -> None:
        """The two the first version flagged, plus the project's key addresses."""
        for value in ('c0000409', '00011000', '001c4064', '001c3f60', 'fe820010',
                      '0017d4d0'):
            self.assertIn(value, checker.KNOWN_CONSTANTS,
                          f'0x{value.upper()} should be a known constant')

    def test_a_status_code_is_not_a_transcription(self) -> None:
        """`0xC0000409` is an observed exit code, not a value read from a file."""
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved = checker.ROOT
            checker.ROOT = root
            try:
                path = root / 'r.md'
                path.write_text('The process exited 0xC0000409.\n', encoding='utf-8')
                findings = checker.audit(path)
            finally:
                checker.ROOT = saved
        self.assertEqual(findings, [])


class VerdictTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'logs').mkdir(parents=True)
        self._saved = checker.ROOT
        checker.ROOT = self.root

    def tearDown(self) -> None:
        checker.ROOT = self._saved
        self._tmp.cleanup()

    def audit(self, body: str) -> list[dict]:
        path = self.root / 'r.md'
        path.write_text(body, encoding='utf-8')
        return checker.audit(path)

    def reasons(self, findings: list[dict]) -> set[str]:
        return {f['reason'] for f in findings}

    def test_a_value_supported_by_a_cited_artifact_passes(self) -> None:
        (self.root / 'logs' / 'out.txt').write_text('value 0x00193D96 here\n',
                                                    encoding='utf-8')
        findings = self.audit('Measured `logs/out.txt` gives 0x00193D96.\n')
        self.assertEqual(findings, [])

    def test_a_value_in_a_table_with_no_citation_is_flagged(self) -> None:
        """W5's case: a transcribed table row nobody re-checked."""
        findings = self.audit(
            '| field | value |\n|---|---|\n| target | 0x00193D96 |\n')
        self.assertIn('unsupported_value', self.reasons(findings))

    def test_a_rechecked_record_passes(self) -> None:
        """W5's remedy: a second worker re-checked the values."""
        findings = self.audit(
            '| field | value |\n|---|---|\n| target | 0x00193D96 |\n\n'
            'Verified-by: worker-2, 2026-09-30\n')
        self.assertIn('rechecked', self.reasons(findings))
        self.assertNotIn('unsupported_value', self.reasons(findings))

    def test_a_value_in_prose_with_no_citation_is_undecidable(self) -> None:
        """Not a transcription: it was probably derived by analysis.

        Measured: `ttd-recording-exit-finding.md` derives `0x00196A29` by
        disassembling the XBE, which this check cannot reproduce. Reporting that as
        a finding would be a false positive on correct work.
        """
        findings = self.audit('The guest died at 0x00196A29.\n')
        self.assertIn('undecidable', self.reasons(findings))
        self.assertNotIn('unsupported_value', self.reasons(findings))

    def test_a_cited_directory_is_searched(self) -> None:
        """A record may cite a run directory rather than each file inside it."""
        run = self.root / 'logs' / 'runs' / 'x'
        run.mkdir(parents=True)
        (run / 'jsrf_run.log').write_text('ret=0x0014982E\n', encoding='utf-8')
        findings = self.audit('| site | `logs/runs/x` | 0x0014982E |\n')
        self.assertEqual(findings, [])

    def test_the_undecidable_verdict_is_not_blocking(self) -> None:
        """It reports UNKNOWN, and the exit code must reflect that."""
        import subprocess
        import sys
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'r.md'
            path.write_text('The guest died at 0x00196A29.\n', encoding='utf-8')
            result = subprocess.run(
                [sys.executable, '-X', 'utf8',
                 str(ROOT / 'scripts' / 'check-transcribed-values.py'),
                 '--record', str(path)],
                capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class RealTreeTests(unittest.TestCase):
    def test_the_delivered_records_pass(self) -> None:
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-transcribed-values.py')],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_it_examines_a_non_trivial_number_of_records(self) -> None:
        # The 2026-10-09 clean-up left 8 records; the floor only proves the scan is not empty.
        self.assertGreater(len(checker.record_files()), 5)


if __name__ == '__main__':
    unittest.main(verbosity=2)
