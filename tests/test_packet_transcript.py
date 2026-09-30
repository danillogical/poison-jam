"""Controls for W3's and W10's packet prerequisites, and W13's budget field.

**W3**: "Freezing requires a DeepSeek **dry-run transcript**: every command executed
on the target host and tree, output hashed in the packet", answering "frozen commands
that never ran (A4s-r4 anchors, A4s-r5 PowerShell 5.1 grep; 7 of 12 A4s Advisor
rulings)".

**W10**: "Each packet lists its load-bearing **premises with byte-level commands**;
the reviewer re-runs them first", answering "false ACCEPTs on false premises".

**W13**: "Per-task **senior-call budget** (§3), recorded in the packet; exceeding it
is an Advisor continue/stop decision", answering "change packets consumed 15–27
senior calls; discovery 2–4".

The controls weight the part of the check that is genuinely mechanical and that the
A4s failure was: **a packet can have a transcript section that covers three of its
twelve commands.** That gap is what `command_not_dry_run` catches, and a control
exercises it directly rather than only testing the section's presence.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_packet_transcript', ROOT / 'scripts' / 'check-packet-transcript.py')
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)

HASH = 'a' * 64

COMPLETE = f"""## C9 — a bounded outcome

**Contract revision:** r1   **Status:** draft
**Senior-call budget:** 1 Planner, 1 Advisor, 1 acceptance (W13)

### Load-bearing premises (W10)
- the slot holds an ordinal — `python -X utf8 scripts/inspect-jsrf.py disasm 0x1C4064 0x1C4068` — `8b512c85`

### Dry-run transcript (W3)
- `python -X utf8 scripts/inspect-jsrf.py disasm 0x1C4064 0x1C4068` — exit 0 — output sha256 `{HASH}`

### Execution
- Steps: run `python -X utf8 scripts/inspect-jsrf.py disasm 0x1C4064 0x1C4068`.
"""


class SectionParsingTests(unittest.TestCase):
    def test_a_section_is_read_to_the_next_heading(self) -> None:
        text = '### A\nbody a\n### B\nbody b\n'
        self.assertEqual(checker.section(text, checker.EXECUTION_HEADING), None)
        self.assertIn('body a', checker.section(
            text, __import__('re').compile(r'(?im)^#{2,4}\s*A\b.*$')))


class PacketAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'docs' / 'packets').mkdir(parents=True)
        self._saved = checker.ROOT
        checker.ROOT = self.root

    def tearDown(self) -> None:
        checker.ROOT = self._saved
        self._tmp.cleanup()

    def audit(self, body: str, name: str = 'c9.md') -> list[dict]:
        path = self.root / 'docs' / 'packets' / name
        path.write_text(body, encoding='utf-8')
        return checker.audit(path)

    def reasons(self, findings: list[dict]) -> set[str]:
        return {f['reason'] for f in findings}

    def test_a_complete_packet_passes(self) -> None:
        self.assertEqual(self.audit(COMPLETE), [])

    def test_a_missing_transcript_is_flagged(self) -> None:
        body = COMPLETE.replace('### Dry-run transcript (W3)\n', '### Not it\n')
        self.assertIn('no_dry_run_transcript', self.reasons(self.audit(body)))

    def test_an_unhashed_transcript_is_flagged(self) -> None:
        """A transcript without a hash cannot be told from a different run."""
        body = COMPLETE.replace(f'output sha256 `{HASH}`', 'output looked fine')
        self.assertIn('transcript_not_hashed', self.reasons(self.audit(body)))

    def test_missing_premises_are_flagged(self) -> None:
        body = COMPLETE.replace('### Load-bearing premises (W10)\n', '### Other\n')
        self.assertIn('no_load_bearing_premises', self.reasons(self.audit(body)))

    def test_premises_without_a_command_are_flagged(self) -> None:
        """W10 requires a byte-level command; prose cannot be re-run."""
        body = COMPLETE.replace(
            '- the slot holds an ordinal — `python -X utf8 scripts/inspect-jsrf.py '
            'disasm 0x1C4064 0x1C4068` — `8b512c85`',
            '- the slot holds an ordinal, as established earlier')
        self.assertIn('premises_without_commands', self.reasons(self.audit(body)))

    def test_an_execution_command_missing_from_the_transcript_is_flagged(self) -> None:
        """The A4s failure: a transcript covering three of twelve commands."""
        body = COMPLETE.replace(
            '- Steps: run `python -X utf8 scripts/inspect-jsrf.py disasm 0x1C4064 '
            '0x1C4068`.',
            '- Steps: run `python -X utf8 scripts/run-jsrf.py --seconds 5`.')
        self.assertIn('command_not_dry_run', self.reasons(self.audit(body)))

    def test_a_fully_covered_packet_is_not_flagged(self) -> None:
        """Every execution command appears in the transcript, so nothing is flagged."""
        self.assertNotIn('command_not_dry_run', self.reasons(self.audit(COMPLETE)))

    def test_a_draft_without_the_sections_is_informational(self) -> None:
        """A draft is expected to be incomplete; the sections bind at freezing."""
        findings = self.audit('## C9\n\n**Status:** draft\n\nNo sections yet.\n')
        self.assertIn('draft_missing_prerequisites', self.reasons(findings))


class ExemptionTests(unittest.TestCase):
    def test_every_exempt_packet_exists(self) -> None:
        """A stale exemption is a rule that silently stopped applying."""
        for name in checker.PRE_W3_W10_PACKETS:
            self.assertTrue((ROOT / 'docs' / 'packets' / name).is_file(),
                            f'{name} is exempt but does not exist; remove it from '
                            f'PRE_W3_W10_PACKETS')

    def test_the_exemption_is_not_a_blanket_waiver(self) -> None:
        """A packet NOT on the list is still governed."""
        import tempfile as tf
        with tf.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'docs' / 'packets').mkdir(parents=True)
            saved = checker.ROOT
            checker.ROOT = root
            try:
                path = root / 'docs' / 'packets' / 'brand-new.md'
                path.write_text('**Status:** ADEQUATE\n\nNo sections.\n',
                                encoding='utf-8')
                findings = checker.audit(path)
            finally:
                checker.ROOT = saved
        reasons = {f['reason'] for f in findings}
        self.assertIn('no_dry_run_transcript', reasons)
        self.assertNotIn('pre_w3_w10_packet', reasons)


class WorkflowTests(unittest.TestCase):
    def test_the_packet_template_carries_the_w13_budget(self) -> None:
        """W13 is 'packet template field', so the template must have the field."""
        text = (ROOT / 'docs' / 'agent-workflow.md').read_text(encoding='utf-8')
        self.assertIn('**Senior-call budget:**', text)
        self.assertIn('### Dry-run transcript (W3)', text)
        self.assertIn('### Load-bearing premises (W10)', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
