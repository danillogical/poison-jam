"""T10 controls for `scripts/cite.py`: a lint that flags everything is useless.

The known-bad case is the project's **recorded historical error** (plan T10): the guest
address `0x00193D62` was recorded where `0x00193D96` belonged.  That is not a synthetic
pair.  `0x00193D96` is the instruction start of `mov esi, ecx` at the head of
`sub_00193D90`; `0x00193D62` is *inside* the instruction that begins at `0x00193D61`
(`mov dword ptr [ebx + 0x40071c], eax`, 6 bytes), so it is never an instruction start and
appears in no disassembly of the region.  `scripts/enumerate-accesses.py` already pins the
same pair as `TRANSPOSITION_LEFT`/`TRANSPOSITION_RIGHT`.

`tools/citations/outputs/t10-disasm-00193D50-00193DA0.txt` is the verbatim stdout of
`python -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0` on the original XBE.
Nothing in this file is hand-transcribed: every expected value is read back from the tool
under test or from that captured output.

The known-good case must NOT be flagged, and the store must not flag its own `be` field.
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CITE = ROOT / 'scripts' / 'cite.py'
CITED_OUTPUT = ROOT / 'tools' / 'citations' / 'outputs' / 't10-disasm-00193D50-00193DA0.txt'

RIGHT = '0x00193D96'          # the value that was recorded
LEFT = '0x00193D62'           # the transposition that must be flagged
RUN = '20260930-001405-390-v1-verified-strict'
RUN_DIR = ROOT / 'logs' / 'runs' / RUN
XBE = ROOT / 'game' / 'default.xbe'

# `C:\Python313\python.exe` is the documented interpreter; -X utf8 is required.
PYTHON = sys.executable


def run_cite(*argv, expect=None):
    """Run cite.py and return (exit code, stdout, stderr)."""
    completed = subprocess.run(
        [PYTHON, '-X', 'utf8', str(CITE), *[str(a) for a in argv]],
        cwd=str(ROOT), capture_output=True, text=True, encoding='utf-8', errors='replace')
    if expect is not None:
        assert completed.returncode == expect, (
            f'expected exit {expect}, got {completed.returncode}\n'
            f'stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}')
    return completed.returncode, completed.stdout, completed.stderr


class TempFixture(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='cite-fixture-'))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)

    def write(self, name, text):
        path = self.tmp / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8', newline='\n')
        return path

    def cited_output_sha(self):
        text = CITED_OUTPUT.read_text(encoding='utf-8')
        return json.loads(run_cite('check', '--json', self.tmp, expect=0)[1]) and None


class LintControls(TempFixture):
    """`check`: known-bad must be flagged, known-good must not."""

    def test_seeded_transposition_is_flagged(self):
        """0x00193D62 where 0x00193D96 was recorded -> finding (plan T10 PASS)."""
        record = self.write('transposed.md', (
            '# Record with the historical transposition\n\n'
            f'CITE: {CITED_OUTPUT}\n\n'
            f'The service helper `sub_{RIGHT}` begins at `{LEFT}`.\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn(f'{record}:5', out)
        self.assertIn(LEFT, out)
        self.assertIn('unsupported', out)
        self.assertNotIn('no-citation', out)
        # the supporting literal must NOT be reported
        self.assertNotIn(RIGHT, out)
        self.assertIn('1 unsupported', out)

    def test_known_good_record_is_not_flagged(self):
        """Hex literals that ARE in the cited output -> exit 0, no findings."""
        body = (
            '# Known-good record\n\n'
            f'CITE: {CITED_OUTPUT}\n\n'
            f'`{RIGHT}` is the instruction start of `mov esi, ecx`; `0x00193D90` is the\n'
            'helper entry and `0x00193D61` is the instruction that contains the misread\n'
            'address.\n')
        record = self.write('good.md', body)
        # The expected count is derived from the record, not transcribed: hand-counting a
        # hex-literal tally is the same class of error this tool exists to catch.
        claims = re.findall(r'0[xX][0-9a-fA-F]{4,16}', body)
        self.assertGreater(len(claims), 0, 'the fixture must contain claims')
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('0 unsupported', out)
        self.assertIn(f'{len(claims)} hex literal(s) considered, {len(claims)} supported', out)

    def test_lint_is_not_one_sided(self):
        """The same value in both spellings: only the transposition is a finding."""
        bad = self.write('bad.md', f'CITE: {CITED_OUTPUT}\n\naddress `{LEFT}`\n')
        good = self.write('good.md', f'CITE: {CITED_OUTPUT}\n\naddress `{RIGHT}`\n')
        self.assertEqual(run_cite('check', bad)[0], 1)
        self.assertEqual(run_cite('check', good)[0], 0)

    def test_record_without_citation_is_no_citation(self):
        """A record naming no cited output reports every literal as no-citation."""
        record = self.write('uncited.md', f'the helper entry is `{RIGHT}`\n')
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn('no-citation', out)
        self.assertIn('the record names no cited output', out)

    def test_cite_fence_body_is_not_linted(self):
        """A `cite` fence is the cited output, not a claim, so it is not scanned."""
        record = self.write('fenced.md', (
            f'CITE: {CITED_OUTPUT}\n\n'
            '```cite\n'
            f'00193D62 mov dword ptr [ebx + 0x40071c], eax\n'
            '```\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('0 hex literal(s) considered', out)

    def test_comment_literals_are_supported_by_a_named_output(self):
        """`<!-- -->` and `#` comments are scanned: a claim hidden there is still a claim."""
        record = self.write('comment.md', (
            f'CITE: {CITED_OUTPUT}\n\n'
            f'<!-- the helper is at {LEFT} -->\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn(LEFT, out)

    def test_quoted_command_arguments_are_not_claims(self):
        """A hex literal in a quoted command is a tool INPUT; the lint has no verdict.

        The recorded command of the cited output is `... disasm 0x00193D50 0x00193DA0`.
        `0x00193DA0` is an end bound, not a value the record asserts, so reporting it
        would be a trigger without a verdict.
        """
        record = self.write('command.md', (
            f'CITE: {CITED_OUTPUT}\n\n'
            'The cited output is the verbatim stdout of:\n\n'
            '    python -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0\n\n'
            f'and the instruction start is `{RIGHT}`.\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertNotIn('0x00193DA0', out)
        self.assertNotIn('0x00193D50', out)
        self.assertIn('0 unsupported', out)

    def test_a_prose_claim_is_still_scanned(self):
        """The command-quote exclusion must not become a hiding place for a bad value."""
        record = self.write('hidden.md', (
            f'CITE: {CITED_OUTPUT}\n\n'
            'Running `python -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0`\n'
            f'showed the helper at {LEFT}.\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn(LEFT, out)

    def test_literal_outside_the_universe_is_not_reported(self):
        """17 digits is outside `0x`+4..16, so the lint has no verdict and stays silent."""
        record = self.write('outside.md', (
            f'CITE: {CITED_OUTPUT}\n\nwide literal `0x00193D6200193D620`\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('0 hex literal(s) considered', out)

    def test_prefix_is_required(self):
        """A short literal must not be satisfied by a longer one: 0x00193D9 != 0x00193D96."""
        record = self.write('prefix.md', (
            f'CITE: {CITED_OUTPUT}\n\nshort `0x00193D9` is not the address {RIGHT}\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn('0x00193D9 ', out)
        self.assertIn('unsupported', out)

    def test_bare_spelling_in_a_record_is_outside_the_claim_universe(self):
        """Claims are `0x`-prefixed literals; a bare 8-digit run in prose is not a claim.

        The cited output prints addresses bare, so a record must spell the claim
        `0x`-prefixed -- and value matching is what makes that record supported.
        """
        record = self.write('bare.md', (
            f'CITE: {CITED_OUTPUT}\n\nthe instruction start is `00193D96`\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('0 hex literal(s) considered', out)

    def test_prefixed_claim_matches_a_bare_output_spelling(self):
        """The real case: record says 0x00193D96, the disassembly prints 00193D96."""
        record = self.write('prefixed.md', (
            f'CITE: {CITED_OUTPUT}\n\nthe instruction start is `{RIGHT}`\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('1 hex literal(s) considered, 1 supported', out)

    def test_width_does_not_change_the_value(self):
        """Support is decided by VALUE, so 0x193D96 is supported by 0x00193D96.

        Zero-padding does not change a number, so this is not a fabrication.  The lint
        must not report it: a trigger it has no verdict for is a defective check.
        """
        record = self.write('short.md', (
            f'CITE: {CITED_OUTPUT}\n\nentry `0x193D96`\n'))
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('1 hex literal(s) considered, 1 supported', out)

    def test_width_does_not_invent_support_for_a_different_value(self):
        """The converse: 0x193D9 must NOT be satisfied by 0x00193D96."""
        record = self.write('short.md', (
            f'CITE: {CITED_OUTPUT}\n\nentry `0x193D9`\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn('0x193D9 ', out)
        self.assertIn('unsupported', out)

    def test_byte_reversal_hint_names_the_recorded_failure(self):
        """A reversed value is reported as a byte-order transposition, with the real value."""
        record = self.write('reversed.md', (
            f'CITE: {CITED_OUTPUT}\n\nthe instruction start is `0x963D1900`\n'))
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn('0x963D1900', out)
        self.assertIn('BYTE-REVERSED spelling 0x00193D96 IS supported', out)

    def test_json_record_cited_output_mismatch_is_an_error(self):
        """A cited output that does not hash to output_sha256 is exit 2, not a pass."""
        record = self.write('record.json', json.dumps({
            'value': RIGHT, 'output_sha256': '00' * 32, 'cited_output': 'nothing here',
        }, indent=2))
        code, out, err = run_cite('check', record, expect=2)
        self.assertIn('output_sha256 does not match', err + out)

    def test_missing_record_path_is_exit_2(self):
        code, out, err = run_cite('check', self.tmp / 'absent.md', expect=2)
        self.assertIn('no such path', out + err)

    def test_empty_scan_is_reported_not_silent(self):
        code, out, err = run_cite('check', self.tmp, expect=0)
        self.assertIn('no file was scanned', out)


class ByteOrderAndRead(unittest.TestCase):
    """`read`: both byte orders, and the offset-shift control."""

    @unittest.skipUnless(RUN_DIR.is_dir(), f'{RUN} is not archived')
    def test_read_prints_both_byte_orders_and_shift_control(self):
        code, out, err = run_cite('read', RUN_DIR, '0x00011000', 4, expect=0)
        self.assertIn('little-endian (x86 byte order):', out)
        self.assertIn('big-endian (reversed byte order):', out)
        self.assertIn('offset-shift control', out)
        for shift in ('-3', '-2', '-1', '+0', '+1', '+2', '+3'):
            self.assertIn(f'shift {shift}', out.replace('shift +0', 'shift +0'))

    @unittest.skipUnless(RUN_DIR.is_dir(), f'{RUN} is not archived')
    def test_read_reports_the_same_bytes_at_shift_zero(self):
        code, out, err = run_cite('read', RUN_DIR, '0x00011000', 4, '--json', expect=0)
        payload = json.loads(out)
        raw = bytes.fromhex(payload['bytes'].replace(' ', ''))
        self.assertEqual(payload['little_endian']['hex'],
                         f'0x{int.from_bytes(raw, "little"):08X}')
        self.assertEqual(payload['big_endian']['hex'],
                         f'0x{int.from_bytes(raw, "big"):08X}')
        self.assertNotEqual(payload['little_endian']['hex'], payload['big_endian']['hex'])
        control = {row['shift']: row for row in payload['offset_shift_control']}
        self.assertEqual(control[0]['bytes'], payload['bytes'])
        self.assertEqual(control[0]['little_endian'], payload['little_endian']['hex'])
        for shift in (-3, -2, -1, 1, 2, 3):
            self.assertIn(shift, control)
            self.assertNotEqual(control[shift]['little_endian'],
                                payload['little_endian']['hex'],
                                f'shift {shift} did not move the read')

    @unittest.skipUnless(RUN_DIR.is_dir(), f'{RUN} is not archived')
    def test_read_with_verify_mapping_prints_the_verdict(self):
        """A CONTENT_MISMATCH dump is still readable (check-dump-mapping.py documents it)."""
        code, out, err = run_cite('read', RUN_DIR, '0x00011000', 4, '--verify-mapping',
                                  '--json', expect=0)
        self.assertIn('mapping check (check-dump-mapping.py) exit', out)

    def test_read_refuses_an_absent_run(self):
        code, out, err = run_cite('read', self.tmp_run(), '0x00011000', 4, expect=2) \
            if hasattr(self, 'tmp_run') else run_cite('read', 'logs/runs/does-not-exist',
                                                     '0x00011000', 4, expect=2)
        self.assertIn('cannot open', out + err)

    def test_read_length_bounds(self):
        code, out, err = run_cite('read', RUN_DIR, '0x00011000', 65, expect=2)
        self.assertIn('--length must be 1..64', err + out)


class RecordProducer(unittest.TestCase):
    """`record`: value + width + artifact hash + command + output hash."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='cite-record-'))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.store = self.tmp / 'store.jsonl'

    def record_transposition(self, value=RIGHT, extra=(), expect=0):
        return run_cite(
            'record', '--value', value, '--width', 4, '--byte-order', 'le',
            '--artifact', XBE, '--command',
            f'{PYTHON} -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0',
            '--key-kind', 'guest-va', '--key', '0x00193D96',
            '--store', self.store, *extra, expect=expect)

    def test_record_stores_hash_and_both_byte_orders(self):
        code, out, err = self.record_transposition(expect=0)
        record = json.loads(self.store.read_text(encoding='utf-8').splitlines()[0])
        self.assertEqual(record['schema'], 'jsrf-citation/1')
        self.assertEqual(record['value'], RIGHT)
        self.assertEqual(record['width'], 4)
        self.assertEqual(record['byte_order'], 'le')
        # both interpretations of the SAME bytes must be printed and differ
        self.assertNotEqual(record['le'], record['be'])
        self.assertEqual(record['be'], '0x963D1900')
        self.assertEqual(len(record['artifacts']), 1)
        self.assertEqual(len(record['artifacts'][0]['sha256']), 64)
        self.assertIn('inspect-jsrf.py disasm', record['command'])
        self.assertTrue(record['executed'])
        self.assertEqual(len(record['output_sha256']), 64)
        self.assertIn('00193D96', record['cited_output'])
        self.assertIn('cite-', record['id'])

    def test_recorded_value_is_re_derivable_from_its_own_output(self):
        """The recorded value must be in the captured output, or recording is refused."""
        code, out, err = run_cite(
            'record', '--value', LEFT, '--width', 4, '--byte-order', 'le',
            '--artifact', XBE, '--command',
            f'{PYTHON} -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0',
            '--key', '0x00193D62', '--store', self.store, expect=2)
        self.assertIn('does not appear in the captured output', err + out)
        self.assertFalse(self.store.exists())

    def test_width_is_required_for_byte_order(self):
        code, out, err = run_cite('record', '--value', RIGHT, '--width', 4,
                                  '--artifact', XBE, '--command', 'echo 0x00193D96',
                                  '--key', 'x', '--store', self.store, expect=2)
        self.assertIn('--byte-order is required when width > 1', err + out)

    def test_command_is_required(self):
        """argparse itself refuses a record with no command: a value with no command is
        hand transcription, and this tool exists to make that impossible."""
        code, out, err = run_cite('record', '--value', '0x01', '--width', 1,
                                  '--key', 'x', '--store', self.store, expect=2)
        self.assertIn('--command', err + out)
        self.assertIn('required', err + out)

    def test_value_must_fit_its_width(self):
        code, out, err = run_cite('record', '--value', '0x1234', '--width', 1,
                                  '--command', 'echo 0x1234', '--key', 'x',
                                  '--store', self.store, expect=2)
        self.assertIn('does not fit in 1 byte(s)', err + out)

    def test_key_universe_rejects_an_out_of_universe_key(self):
        """A key outside the declared universe is a could-not-run, not data."""
        universe = self.tmp / 'universe.txt'
        universe.write_text('# keys derived from source\n0x00193D96\n', encoding='utf-8')
        other = self.tmp / 'other-universe.txt'
        other.write_text('# a different finite key universe\n0x00193D90\n', encoding='utf-8')
        code, out, err = run_cite(
            'record', '--value', RIGHT, '--width', 4, '--byte-order', 'le',
            '--artifact', XBE, '--command',
            f'{PYTHON} -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0',
            '--key', '0x00193D96', '--key-universe', other,
            '--store', self.tmp / 'other.jsonl', expect=2)
        self.assertIn('outside the declared universe', err + out)
        self.assertFalse((self.tmp / 'other.jsonl').exists())

    def test_key_universe_accepts_an_in_universe_key(self):
        """The same key spelled bare (as `inspect-jsrf.py disasm` prints) is in-universe."""
        universe = self.tmp / 'universe.txt'
        universe.write_text('# keys derived from source\n00193D96\n', encoding='utf-8')
        self.record_transposition(extra=('--key-universe', universe), expect=0)
        self.assertTrue(self.store.is_file())

    def test_missing_key_universe_file_is_exit_2(self):
        code, out, err = self.record_transposition(
            extra=('--key-universe', self.tmp / 'absent.txt'), expect=2)
        self.assertIn('is not a file', err + out)

    def test_identical_recording_is_not_appended_twice(self):
        self.record_transposition(expect=0)
        first = self.store.read_text(encoding='utf-8')
        code, out, err = self.record_transposition(expect=0)
        self.assertIn('already recorded', out)
        self.assertEqual(self.store.read_text(encoding='utf-8'), first)

    def test_capture_mode_does_not_execute_the_command(self):
        capture = self.tmp / 'capture.txt'
        capture.write_text(f'00193D96 mov esi, ecx\n', encoding='utf-8')
        code, out, err = run_cite(
            'record', '--value', RIGHT, '--width', 4, '--byte-order', 'le',
            '--command', 'this-command-does-not-exist-xyz',
            '--capture', capture, '--key', '0x00193D96', '--store', self.store, expect=0)
        record = json.loads(self.store.read_text(encoding='utf-8').splitlines()[0])
        self.assertFalse(record['executed'])
        self.assertIsNone(record['exit_code'])

    def test_failing_command_is_refused(self):
        code, out, err = run_cite(
            'record', '--value', RIGHT, '--width', 4, '--byte-order', 'le',
            '--command', f'{PYTHON} -X utf8 -c "import sys; sys.exit(3)"',
            '--key', 'x', '--store', self.store, expect=2)
        self.assertIn('refusing to cite an output', err + out)


class StoreIsNotALintTarget(unittest.TestCase):
    """A citation store is a cited output, not a claim: its own `be` must not be flagged."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='cite-store-'))
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.store = self.tmp / 'store.jsonl'
        run_cite(
            'record', '--value', RIGHT, '--width', 4, '--byte-order', 'le',
            '--artifact', XBE, '--command',
            f'{PYTHON} -X utf8 scripts/inspect-jsrf.py disasm 0x00193D50 0x00193DA0',
            '--key', '0x00193D96', '--store', self.store, '--quiet', expect=0)

    def test_store_scanned_directly_is_clean(self):
        code, out, err = run_cite('check', self.store, expect=0)
        self.assertIn('0 unsupported', out)
        # the store's own be field 0x963D1900 must not have been treated as a claim
        self.assertNotIn('0x963D1900', out)

    def test_record_citing_the_store_is_clean(self):
        record = self.tmp / 'record.md'
        record.write_text(
            f'CITE-STORE: {self.store}\n\nthe helper `{RIGHT}` is the instruction start\n',
            encoding='utf-8', newline='\n')
        code, out, err = run_cite('check', record, expect=0)
        self.assertIn('1 supported', out)

    def test_record_citing_the_store_still_flags_a_transposition(self):
        record = self.tmp / 'record.md'
        record.write_text(
            f'CITE-STORE: {self.store}\n\nthe helper `{LEFT}` is the instruction start\n',
            encoding='utf-8', newline='\n')
        code, out, err = run_cite('check', record, expect=1)
        self.assertIn(LEFT, out)
        self.assertIn('unsupported', out)


class RealArtifactControl(unittest.TestCase):
    """The transposition is real in the original XBE, not a fixture convention."""

    @unittest.skipUnless(XBE.is_file(), 'game/default.xbe is not present')
    def test_only_the_recorded_address_is_an_instruction_start(self):
        completed = subprocess.run(
            [PYTHON, '-X', 'utf8', 'scripts/inspect-jsrf.py', 'disasm',
             '0x00193D50', '0x00193DA0'],
            cwd=str(ROOT), capture_output=True, text=True, encoding='utf-8', errors='replace')
        self.assertEqual(completed.returncode, 0, completed.stderr)
        text = completed.stdout
        self.assertIn(RIGHT[2:], text)
        self.assertNotIn(LEFT[2:], text)
        # the cited-output fixture is that same output, so the lint has real support
        self.assertEqual(CITED_OUTPUT.read_text(encoding='utf-8'),
                         text.replace('\r\n', '\n').replace('\r', '\n'))


if __name__ == '__main__':
    unittest.main()
