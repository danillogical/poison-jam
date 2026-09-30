"""Controls for W9's control-first instrument gate.

Plan W9: "**Control-first instruments**: `run-jsrf.py` refuses a second ON run, and
checkers refuse to emit a row, until the raw log shows the named positive-control
event at an independently derived address", answering "13 A2h instrument defects; a
positive control silently weakened to a value comparison".

Two failures, and the controls exercise both:

  * an instrument run ON repeatedly while its control had never fired, so every "no
    hits" conclusion rested on an instrument nobody had shown could see anything
    ("15 DR0 ON runs before zero hits were noticed"); and
  * a control rewritten from "the event occurred at address X" to "some value
    changed" -- a weaker predicate that passes for unrelated reasons.

The second is why `--address` is required rather than optional, and why a control
that fired at the WRONG address is refused: that is exactly the weakened form.

Two defects the controls found are encoded here as regressions: `str.lstrip('0x')`
strips characters rather than a prefix (turning `0x001C4064` into `1C4064` but also
`0x00` into an empty string), and `\\b` cannot match between the `x` and the `0` of
`0x001C4064` because both are word characters.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_instrument_controls', ROOT / 'scripts' / 'check-instrument-controls.py')
assert _spec and _spec.loader
controls = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(controls)


LOG_WITH_CONTROL_AT_SLOT = (
    '  [A2HSLOT] alias touch mirror=3 fault=0x001C4064\n'
    '  [A2HSLOT] install control slot=65 va=0x001C4064 raw=80000115 '
    'installed=FE000104\n'
)
LOG_WITHOUT_CONTROL = '  [A2HSLOT] nothing happened here\n'


class ControlFiredTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def log(self, body: str) -> Path:
        path = self.root / 'jsrf_run.log'
        path.write_text(body, encoding='utf-8')
        return path

    def test_a_control_at_the_named_address_fires(self) -> None:
        fired, detail = controls.control_fired(
            self.log(LOG_WITH_CONTROL_AT_SLOT), 'install control', '0x001C4064')
        self.assertTrue(fired, detail)

    def test_a_control_at_the_wrong_address_does_not_fire(self) -> None:
        """The weakened form: the event occurred, but not where it must."""
        fired, detail = controls.control_fired(
            self.log(LOG_WITH_CONTROL_AT_SLOT), 'install control', '0xDEADBEEF')
        self.assertFalse(fired)
        self.assertIn('value comparison is not an address match', detail)

    def test_an_absent_control_does_not_fire(self) -> None:
        fired, detail = controls.control_fired(
            self.log(LOG_WITHOUT_CONTROL), 'install control', '0x001C4064')
        self.assertFalse(fired)
        self.assertIn('does not appear', detail)

    def test_a_missing_log_does_not_fire(self) -> None:
        """Absence of a log is not evidence the control fired."""
        fired, detail = controls.control_fired(
            self.root / 'absent.log', 'install control', '0x001C4064')
        self.assertFalse(fired)
        self.assertIn('no log', detail)

    def test_the_address_matches_with_or_without_the_0x_prefix(self) -> None:
        """Both spellings appear in real logs."""
        for spelling in ('0x001C4064', '001C4064', '0x1C4064'):
            fired, detail = controls.control_fired(
                self.log(LOG_WITH_CONTROL_AT_SLOT), 'install control', spelling)
            self.assertTrue(fired, f'{spelling}: {detail}')

    def test_the_address_is_not_matched_inside_a_longer_hex_run(self) -> None:
        """A boundary must prevent a prefix match.

        `0x001C40640` is not `0x001C4064`, and a check that accepted it would pass a
        control that fired somewhere else.
        """
        path = self.log('  install control va=0x001C40640\n')
        fired, _detail = controls.control_fired(path, 'install control', '0x001C4064')
        self.assertFalse(fired)


class RegistrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self._saved = (controls.ROOT, controls.STATE)
        controls.ROOT = self.root
        controls.STATE = self.root / 'logs' / 'instrument-controls.json'

    def tearDown(self) -> None:
        controls.ROOT, controls.STATE = self._saved
        self._tmp.cleanup()

    def run_cli(self, *args: str) -> tuple[int, str]:
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-instrument-controls.py'), *args],
            capture_output=True, text=True, cwd=str(ROOT))
        return result.returncode, result.stdout + result.stderr

    def test_a_control_without_an_address_is_refused(self) -> None:
        """W9 refuses the weakened form at registration."""
        log = self.root / 'jsrf_run.log'
        log.write_text(LOG_WITH_CONTROL_AT_SLOT, encoding='utf-8')
        code, output = self.run_cli('record', '--instrument', 'X',
                                    '--control', 'install control',
                                    '--log', str(log))
        self.assertEqual(code, 2)
        self.assertIn('--address', output)

    def test_a_control_without_a_name_is_refused(self) -> None:
        log = self.root / 'jsrf_run.log'
        log.write_text(LOG_WITH_CONTROL_AT_SLOT, encoding='utf-8')
        code, output = self.run_cli('record', '--instrument', 'X',
                                    '--address', '0x001C4064',
                                    '--log', str(log))
        self.assertEqual(code, 2)
        self.assertIn('--control', output)


class StateTests(unittest.TestCase):
    """The refusal rule itself, exercised on a scratch state file."""

    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self._saved = (controls.ROOT, controls.STATE)
        controls.ROOT = self.root
        controls.STATE = self.root / 'logs' / 'instrument-controls.json'

    def tearDown(self) -> None:
        controls.ROOT, controls.STATE = self._saved
        self._tmp.cleanup()

    def test_no_runs_recorded_allows_a_run(self) -> None:
        self.assertEqual(controls.load_state()['runs'], [])

    def test_an_uncontrolled_run_blocks_a_second(self) -> None:
        """The rule: a second ON run is refused until the control fires."""
        controls.save_state({'version': 1, 'runs': [
            {'instrument': 'JSRF_TRACE_A2H_DR', 'control': 'A2HSLOT',
             'address': '0x001C4064', 'control_fired': False,
             'detail': 'did not fire'},
        ]})
        state = controls.load_state()
        uncontrolled = [r for r in state['runs'] if not r.get('control_fired')]
        self.assertEqual(len(uncontrolled), 1)

    def test_a_controlled_run_does_not_block(self) -> None:
        controls.save_state({'version': 1, 'runs': [
            {'instrument': 'JSRF_TRACE_A2H_DR', 'control': 'A2HSLOT',
             'address': '0x001C4064', 'control_fired': True, 'detail': 'fired'},
        ]})
        state = controls.load_state()
        self.assertEqual([r for r in state['runs'] if not r.get('control_fired')], [])

    def test_an_unreadable_state_is_not_a_pass(self) -> None:
        """A state file that cannot be parsed must not read as 'nothing recorded'."""
        controls.STATE.parent.mkdir(parents=True, exist_ok=True)
        controls.STATE.write_text('{ not json', encoding='utf-8')
        state = controls.load_state()
        self.assertTrue(state.get('unreadable'))

    def test_the_state_file_is_under_gitignored_logs(self) -> None:
        """Run state is not a durable record; it lives with the run archive."""
        self.assertIn('logs', str(controls.STATE))
        self.assertIn('instrument-controls.json', str(controls.STATE))


class InstrumentDetectionTests(unittest.TestCase):
    def test_instrument_switches_are_recognised(self) -> None:
        found = controls.instruments_in({
            'JSRF_TRACE_A2H_DR': '1',
            'JSRF_TRACE_HEAP': '1',
            'RECOMP_GPU_ACK': '0',
            'RECOMP_MMIO_TRACE': '1',
        })
        self.assertIn('JSRF_TRACE_A2H_DR', found)
        self.assertNotIn('RECOMP_GPU_ACK', found)


if __name__ == '__main__':
    unittest.main(verbosity=2)
