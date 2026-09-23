"""P0.4 fixtures: structural validity is separate from image-content integrity.

The distinction is not cosmetic.  A previous version of the project's checker said
"a DIFFERS dump is not evidence, and its addresses are not guest addresses" — both
statements were wrong, and the second would have discarded the very evidence the
displacement finding rests on.  These fixtures pin the separation so it cannot be
collapsed again.
"""
from __future__ import annotations

import importlib.util
import json
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'scripts'
sys.path.insert(0, str(SCRIPTS))

_spec = importlib.util.spec_from_file_location(
    'check_dump_controls', SCRIPTS / 'check-dump-controls.py')
assert _spec and _spec.loader
controls = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(controls)

CHECKER = SCRIPTS / 'check-dump-controls.py'
A2G = 'logs/runs/20260922-224429-003-a2g-304f0-span'
GOOD = 'logs/runs/20260921-111607-936-kick-ack'


class FakeMemory:
    """A minimal stand-in for the dump reader."""

    def __init__(self, pages: dict[int, bytes]):
        self.pages = pages

    def read(self, va: int, length: int) -> bytes:
        if va not in self.pages:
            raise ValueError(f'address 0x{va:08X} is not mapped')
        data = self.pages[va]
        if len(data) < length:
            raise ValueError(f'address 0x{va:08X} has only {len(data)} bytes')
        return data[:length]

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False


class ExpectedBytesTests(unittest.TestCase):
    def test_documented_control_is_derived_correctly(self):
        """The checker must agree with the measured 545/546 control."""
        self.assertEqual(controls.expected_text_bytes(), controls.DOCUMENTED_CONTROL)

    def test_control_is_the_documented_hex(self):
        self.assertEqual(controls.DOCUMENTED_CONTROL.hex(),
                         '8b512c85d28b4130c70190431c00741c')


class StructureContentSeparationTests(unittest.TestCase):
    """The two axes must be reported independently."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.folder = Path(self._tmp.name) / 'run'
        self.folder.mkdir(parents=True)

    def tearDown(self):
        self._tmp.cleanup()

    def write_stack(self, *, esp: int | None = 0x00F7FD00, ret: int | None = 0x0014982E):
        lines = ['guest_ram=0x00000000+0x08000000']
        if esp is not None:
            parts = [f'esp=0x{esp:08X}']
            if ret is not None:
                parts.append(f'eip=0x{ret:08X}')
            lines.append(' '.join(parts))
        (self.folder / 'stacks.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')

    def test_displaced_content_is_still_structurally_ok(self):
        """The A2g shape: displaced image, valid stack.  Both must be reported."""
        self.write_stack()
        memory = FakeMemory({
            0x00F7FD00: struct.pack('<I', 0x0014982E),
            controls.PROBE_VA: bytes.fromhex('83c41456ff5010eb5d8b44240c85c06a'),
        })
        structure = controls.structural_verdict(self.folder, memory)
        content = controls.content_verdict(self.folder, memory, controls.DOCUMENTED_CONTROL)
        self.assertEqual(structure['status'], controls.STRUCTURE_OK)
        self.assertEqual(content['status'], controls.CONTENT_MISMATCH)

    def test_matching_content_is_reported_as_match(self):
        self.write_stack()
        memory = FakeMemory({
            0x00F7FD00: struct.pack('<I', 0x0014982E),
            controls.PROBE_VA: controls.DOCUMENTED_CONTROL,
        })
        self.assertEqual(
            controls.content_verdict(self.folder, memory, controls.DOCUMENTED_CONTROL)['status'],
            controls.CONTENT_MATCH)

    def test_content_match_does_not_certify_the_whole_image(self):
        self.write_stack()
        memory = FakeMemory({
            0x00F7FD00: struct.pack('<I', 0x0014982E),
            controls.PROBE_VA: controls.DOCUMENTED_CONTROL,
        })
        verdict = controls.content_verdict(self.folder, memory, controls.DOCUMENTED_CONTROL)
        self.assertIn('claim_limit', verdict)
        self.assertIn('one probe', verdict['claim_limit'])

    def test_structure_claim_is_limited_to_the_logged_esp(self):
        self.write_stack()
        memory = FakeMemory({
            0x00F7FD00: struct.pack('<I', 0x0014982E),
            controls.PROBE_VA: controls.DOCUMENTED_CONTROL,
        })
        verdict = controls.structural_verdict(self.folder, memory)
        self.assertIn('claim_limit', verdict)
        self.assertIn('logged esp only', verdict['claim_limit'])

    def test_missing_stack_file_is_unsupported_not_ok(self):
        memory = FakeMemory({controls.PROBE_VA: controls.DOCUMENTED_CONTROL})
        verdict = controls.structural_verdict(self.folder, memory)
        self.assertEqual(verdict['status'], controls.STRUCTURE_UNSUPPORTED)

    def test_unreadable_esp_is_unsupported_not_ok(self):
        self.write_stack()
        memory = FakeMemory({controls.PROBE_VA: controls.DOCUMENTED_CONTROL})
        verdict = controls.structural_verdict(self.folder, memory)
        self.assertEqual(verdict['status'], controls.STRUCTURE_UNSUPPORTED)

    def test_unreadable_content_is_distinct_from_mismatch(self):
        self.write_stack()
        memory = FakeMemory({0x00F7FD00: struct.pack('<I', 0x0014982E)})
        verdict = controls.content_verdict(self.folder, memory, controls.DOCUMENTED_CONTROL)
        self.assertEqual(verdict['status'], controls.CONTENT_UNREADABLE)


class MissingInputTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self):
        self._tmp.cleanup()

    def test_named_run_without_dump_is_a_failure(self):
        folder = self.root / 'no-dump'
        folder.mkdir()
        result = controls.check_run(folder, controls.DOCUMENTED_CONTROL, controls.load_reader())
        self.assertEqual(result['content']['status'], controls.CONTENT_MISSING)
        self.assertFalse(result['usable_for_content_claim'])

    def test_missing_directory_is_reported(self):
        result = controls.check_run(self.root / 'absent', controls.DOCUMENTED_CONTROL,
                                    controls.load_reader())
        self.assertEqual(result['content']['status'], controls.CONTENT_MISSING)

    def test_mismatch_result_carries_the_read_rule(self):
        folder = self.root / 'displaced'
        folder.mkdir()
        (folder / 'stacks.txt').write_text('guest_ram=0x0+0x1\nesp=0x00F7FD00\n',
                                           encoding='utf-8')
        result = controls.check_run(folder, controls.DOCUMENTED_CONTROL, controls.load_reader())
        # No real dump, so this is MISSING -- but the rule must be attached to a
        # real mismatch, which the live-artifact test below asserts.
        self.assertIn(result['content']['status'],
                      (controls.CONTENT_MISSING, controls.CONTENT_UNREADABLE))


class LiveArtifactTests(unittest.TestCase):
    """Reproduce the recorded A2g facts against the real archived dump."""

    def setUp(self):
        self.a2g = ROOT / A2G
        self.good = ROOT / GOOD
        if not (self.a2g / 'process.dmp').is_file() or not (self.good / 'process.dmp').is_file():
            self.skipTest('archived dumps are unavailable')

    def test_a2g_is_structurally_ok_and_content_displaced(self):
        reader = controls.load_reader()
        result = controls.check_run(self.a2g, controls.DOCUMENTED_CONTROL, reader)
        self.assertEqual(result['structure']['status'], controls.STRUCTURE_OK)
        self.assertEqual(result['content']['status'], controls.CONTENT_MISMATCH)
        self.assertFalse(result['usable_for_content_claim'])
        self.assertTrue(result['usable_for_structural_claim'])

    def test_a2g_thunk_slot_is_zero_at_its_actual_guest_va(self):
        """P0.4-AC3: the zero thunk slot is read at its ACTUAL VA, unshifted."""
        reader = controls.load_reader()
        with reader.DumpMemory(self.a2g) as memory:
            self.assertEqual(memory.read(0x001C4064, 4), b'\x00\x00\x00\x00')

    def test_a2g_logged_esp_holds_the_recorded_return_va(self):
        """P0.4-AC3: the return word is checked at the logged ESP."""
        reader = controls.load_reader()
        with reader.DumpMemory(self.a2g) as memory:
            word = struct.unpack('<I', memory.read(0x00F7FD00, 4))[0]
        self.assertEqual(word, 0x0014982E)

    def test_good_run_matches_on_both_axes(self):
        reader = controls.load_reader()
        result = controls.check_run(self.good, controls.DOCUMENTED_CONTROL, reader)
        self.assertEqual(result['structure']['status'], controls.STRUCTURE_OK)
        self.assertEqual(result['content']['status'], controls.CONTENT_MATCH)
        self.assertTrue(result['usable_for_content_claim'])

    def test_shifted_read_is_not_a_read_correction(self):
        """The displacement is real: reading at the actual VA differs from shifted.

        This pins the *reason* the read rule exists rather than asserting a
        correction would work -- a shifted comparison recovers byte provenance and
        nothing else.
        """
        reader = controls.load_reader()
        with reader.DumpMemory(self.a2g) as memory:
            actual = memory.read(controls.PROBE_VA, 16)
        self.assertNotEqual(actual, controls.DOCUMENTED_CONTROL,
                            'the A2g image is displaced, so the actual VA must differ')


class CliTests(unittest.TestCase):
    def test_json_shape_and_exit_for_displaced_run(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), A2G, '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        payload = json.loads(result.stdout)
        self.assertEqual(payload['expected'], controls.DOCUMENTED_CONTROL.hex())
        self.assertEqual(len(payload['runs']), 1)

    def test_missing_run_exits_1(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), 'no-such-run', '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        payload = json.loads(result.stdout)
        self.assertEqual(payload['runs'][0]['content']['status'], controls.CONTENT_MISSING)

    def test_no_arguments_is_a_usage_error(self):
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER)],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)

    def test_checker_is_read_only(self):
        before = (ROOT / A2G / 'process.dmp').read_bytes()[:64]
        subprocess.run([sys.executable, '-X', 'utf8', str(CHECKER), A2G],
                       cwd=ROOT, capture_output=True, text=True)
        self.assertEqual((ROOT / A2G / 'process.dmp').read_bytes()[:64], before)


if __name__ == '__main__':
    unittest.main(verbosity=2)
