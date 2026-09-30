"""Controls for the TTD alias arithmetic and query plumbing (plan T1).

The TTD query itself needs a trace and a debugger, so it is exercised by running
`tools/ttd/ttd-query.py` against a recorded trace (recorded in the session record).
What is testable without either is the part that has already been wrong twice in
one session:

  * the **host base**.  The runtime does not map guest 0 at host 0; it maps it at
    an offset it prints.  A query that omits the offset looks 64 KB below every
    guest address.  The first working query returned 8 writes for guest
    `0x001C4064` that were really the XBE decompressor writing guest `0x001B4064`
    -- a plausible wrong answer, which is the worst kind.
  * the **alias arithmetic**.  29 linear addresses, one canonical plus 28 mirrors
    at 64 MB intervals.
  * **fail-closed derivation**.  Both numbers come from the run's own log lines.
    A log without them must produce `UNKNOWN`, never a default.

These are pure functions over a log string, so the controls are cheap and the
failures they catch are the ones that produced a wrong answer rather than an
error.
"""
from __future__ import annotations

import re
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'ttd'))

from aliases import (  # noqa: E402
    XBOX_NUM_MIRRORS,
    aliases,
    format_aliases,
    host_base_from_log,
    ram_size_from_log,
)

# Verbatim from a real strict run's log (the two lines the query depends on).
REAL_LOG = (
    'xbox_MemoryLayoutInit: mapped 65536 KB at 0x0000000000010000 '
    '(offset +65536 from Xbox base)\n'
    '  RAM mirror: 28/28 views mapped (covers 1856 MB)\n'
)


class HostBaseTests(unittest.TestCase):
    def test_reads_the_real_line(self) -> None:
        self.assertEqual(host_base_from_log(REAL_LOG), 0x10000)

    def test_absent_line_is_none_not_zero(self) -> None:
        """A missing line must not default to 0; that is the wrong-64-KB bug."""
        self.assertIsNone(host_base_from_log('RAM mirror: 28/28 views mapped\n'))

    def test_zero_base_is_distinguishable_from_absent(self) -> None:
        text = 'xbox_MemoryLayoutInit: mapped 65536 KB at 0x0000000000000000\n'
        self.assertEqual(host_base_from_log(text), 0)


class RamSizeTests(unittest.TestCase):
    def test_reads_the_real_line(self) -> None:
        coverage = ram_size_from_log(REAL_LOG)
        self.assertIsNotNone(coverage)
        mapped, expected, ram_bytes = coverage
        self.assertEqual((mapped, expected), (28, 28))
        # 1856 MB covers the base view plus 28 mirrors: 29 * 64 MB.
        self.assertEqual(ram_bytes, 64 * 1024 * 1024)

    def test_absent_line_is_none(self) -> None:
        self.assertIsNone(ram_size_from_log('nothing here\n'))

    def test_partial_coverage_is_reported_as_such(self) -> None:
        coverage = ram_size_from_log('  RAM mirror: 20/28 views mapped (covers 1344 MB)\n')
        self.assertIsNotNone(coverage)
        mapped, expected, _ = coverage
        self.assertEqual((mapped, expected), (20, 28))
        self.assertNotEqual(mapped, expected)  # the caller fails closed on this


class AliasTests(unittest.TestCase):
    RAM = 64 * 1024 * 1024

    def test_count_is_one_canonical_plus_28_mirrors(self) -> None:
        self.assertEqual(len(aliases(0x1C4064, self.RAM)), XBOX_NUM_MIRRORS + 1)
        self.assertEqual(len(aliases(0x1C4064, self.RAM)), 29)

    def test_first_is_the_canonical_host_address(self) -> None:
        self.assertEqual(aliases(0x1C4064, self.RAM, host_base=0x10000)[0], 0x1D4064)

    def test_mirrors_step_by_the_ram_size(self) -> None:
        result = aliases(0x1C4064, self.RAM, host_base=0x10000)
        for index in range(1, len(result)):
            self.assertEqual(result[index] - result[index - 1], self.RAM)

    def test_mirror_n_aliases_the_same_guest_offset(self) -> None:
        """Each alias must land on the same offset within its own view."""
        va = 0x1C4064
        for index, address in enumerate(aliases(va, self.RAM, host_base=0x10000)):
            self.assertEqual((address - 0x10000) % self.RAM, va)
            self.assertEqual((address - 0x10000) // self.RAM, index)

    def test_out_of_range_is_refused(self) -> None:
        """A non-RAM address has no mirrors; guessing one would be wrong."""
        for bad in (0x80000000, self.RAM, self.RAM + 1, -1):
            with self.assertRaises(ValueError):
                aliases(bad, self.RAM)

    def test_host_base_shifts_every_alias(self) -> None:
        without = aliases(0x1C4064, self.RAM)
        with_base = aliases(0x1C4064, self.RAM, host_base=0x10000)
        self.assertEqual([a - 0x10000 for a in with_base], without)

    def test_format_is_16_hex_digits(self) -> None:
        formatted = format_aliases(0x1C4064, self.RAM, host_base=0x10000)
        self.assertEqual(len(formatted), 29)
        for text in formatted:
            self.assertRegex(text, r'^0x[0-9A-F]{16}$')


class QueryScriptTests(unittest.TestCase):
    """The generated cdb script must carry the corrected addresses."""

    def test_script_passes_host_addresses(self) -> None:
        source = (ROOT / 'tools' / 'ttd' / 'ttd-query.py').read_text(encoding='utf-8')
        # The negative control must be translated, not passed as a raw guest VA.
        self.assertIn('host_base + negative_guest', source)
        self.assertIn('alias_list[index]', source)

    def test_writes_js_is_not_python(self) -> None:
        """A Python docstring at the top is a JavaScript syntax error.

        Measured: `writes.js` first shipped with a `\"\"\"` header, and the debugger
        reported `SyntaxError: Invalid or unexpected token` at load, then
        `Unable to bind name` for every later call -- which reads like a missing
        function, not a parse failure.
        """
        text = (ROOT / 'tools' / 'ttd' / 'writes.js').read_text(encoding='utf-8')
        self.assertFalse(text.lstrip().startswith('"""'))
        self.assertNotIn('"""', text)

    def test_writes_js_avoids_unsupported_syntax_in_code(self) -> None:
        """Only syntax MEASURED to fail is excluded.

        The engine accepts more than expected, and an earlier version of this
        control asserted a subset it had not measured -- flagging `for...of`,
        which the working query uses.  A control that forbids working code is a
        defect, so only the construct that actually failed is excluded here.

        Comments and string literals are excluded from the scan, because a
        backtick in prose about the engine is not a template literal.
        """
        text = (ROOT / 'tools' / 'ttd' / 'writes.js').read_text(encoding='utf-8')
        code = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
        code = re.sub(r'//[^\n]*', '', code)
        self.assertNotIn('=>', code, 'arrow functions are not used here')

    def test_writes_js_uses_the_verified_memory_api(self) -> None:
        """The query must go through `TTD.Memory`, the API this project verified.

        `host.memory.readMemoryValues` is NOT available in this debugger build --
        a readability probe built on it returned 0 for every address, including
        ones that were demonstrably readable.  A probe that silently reports
        "unreadable" everywhere is worse than no probe: it would certify any
        address as a valid negative control.
        """
        text = (ROOT / 'tools' / 'ttd' / 'writes.js').read_text(encoding='utf-8')
        self.assertIn('TTD.Memory(', text)
        self.assertNotIn('host.memory.readMemoryValues', text)


class AdmissionVerdictTests(unittest.TestCase):
    """W11's S1-S8 verdict, on the code path a real query uses.

    The ruling requires the conditions to be "evaluated mechanically" and to
    select UNKNOWN when any fails. A verdict that could not fail would satisfy
    every positive case, so each control here pairs a case that must be ADMITTED
    with one that must not.
    """

    def setUp(self) -> None:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'ttd_query', ROOT / 'tools' / 'ttd' / 'ttd-query.py')
        self.query = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.query)

    def _record(self, **overrides) -> dict:
        record = {
            'trace': str(ROOT / 'logs' / 'ttd' / 'nonexistent' / 'x.run'),
            'cdb': self.query.resolve_cdb() or 'cdb',
            'guest_va': '0x001C4064',
            'host_base': '0x0000000000010000',
            'ram_bytes': 64 * 1024 * 1024,
            'aliases_missing': [],
            'aliases_answered': 29,
            'alias_count': 29,
            'alias_coverage': 'COMPLETE',
            'mirrors_mapped': 28,
            'mirrors_expected': 28,
            'toolkit_mirrors': 28,
            'terminal': {'present': True, 'sequence': 100, 'kind': 'test',
                         'value_at_p': '0xFE000104'},
        }
        record.update(overrides)
        return record

    def _parsed(self, **overrides) -> dict:
        parsed = {
            'cdb_exit_code': 0,
            'summaries': [{'alias_index': i, 'truncated': False, 'writes': 1}
                          for i in range(29)],
            'lasts': {0: {'alias_index': 0, 'address': '0x0', 'ip': '0x7ff',
                          'size': 4, 'thread_id': 1, 'time_sequence': 50,
                          'time_steps': 1, 'value': '0x00000000FE000104'}},
            'events': [],
            'controls': {
                'trace_live_writes': ['16|truncated|1'],
                'mirror_writes': ['3|scanned|28'],
                'negative_writes': ['0'],
            },
        }
        parsed.update(overrides)
        return parsed

    def _args(self):
        """A stub args carrying a healthy install-positive runner.

        The verdict logic must be testable without a trace and a debugger; a
        verdict exercised only when a real trace exists is tested exactly when its
        bugs are most expensive.
        """
        class _Args:
            terminal_sequence = 100
            max_hits = 16
            install_positive_runner = staticmethod(
                lambda: {'found': True, 'evaluated': True,
                         'detail': 'stub install positive'})
        return _Args()

    def test_a_fully_satisfied_pass_is_admitted(self) -> None:
        """Known-good: every condition satisfied must yield ADMITTED.

        The record-contract is pointed at the real trace from this session so S1
        can be satisfied; without it S1 is UNKNOWN by design (see the next test).
        """
        contract = (ROOT / 'logs' / 'ttd' / '20260930-030513-184-c1-probe'
                    / 'record-contract.json')
        if not contract.is_file():
            self.skipTest('the recorded trace contract is not present')
        record = self._record(trace=str(contract.parent / 'jsrf_recomp01.run'))
        parsed = self._parsed()
        result = self.query.evaluate_conditions(record, parsed, self._args())
        self.assertEqual(result['verdict'], 'ADMITTED', result['conditions'])
        self.assertEqual(result['row'], 'ATTRIBUTED')

    def test_guest_width_and_full_width_values_compare_equal(self) -> None:
        """Formatting must not change the selected row.

        Measured: the last-write row prints a full 64-bit value while a caller
        naturally writes the guest-width form, so a string comparison selected
        UNATTRIBUTED WRITER for a slot that was in fact attributed.
        """
        contract = (ROOT / 'logs' / 'ttd' / '20260930-030513-184-c1-probe'
                    / 'record-contract.json')
        if not contract.is_file():
            self.skipTest('the recorded trace contract is not present')
        record = self._record(trace=str(contract.parent / 'jsrf_recomp01.run'))
        record['terminal']['value_at_p'] = '0xFE000104'   # guest width
        result = self.query.evaluate_conditions(record, self._parsed(), self._args())
        self.assertEqual(result['row'], 'ATTRIBUTED')
        self.assertEqual(result['verdict'], 'ADMITTED')

    def test_a_trace_without_the_terminal_event_is_not_admitted(self) -> None:
        """W-a: the delivered artifact's actual failure (the Advisor's F1)."""
        record = self._record(terminal={'present': False, 'sequence': None,
                                        'kind': None, 'value_at_p': None})
        result = self.query.evaluate_conditions(record, self._parsed(), self._args())
        self.assertEqual(result['verdict'], 'NOT ADMITTED')
        self.assertIn('S2_terminal_in_trace', result['failed'])

    def test_a_missing_mirror_positive_is_not_admitted(self) -> None:
        """W-c: a zero at a mirror is not evidence without it (the Advisor's F5)."""
        parsed = self._parsed()
        parsed['controls']['mirror_writes'] = ['0|scanned|28']
        result = self.query.evaluate_conditions(self._record(), parsed, self._args())
        self.assertEqual(result['verdict'], 'NOT ADMITTED')
        self.assertIn('S5_controls', result['failed'])

    def test_a_missing_negative_control_is_not_admitted(self) -> None:
        parsed = self._parsed()
        parsed['controls'].pop('negative_writes')
        result = self.query.evaluate_conditions(self._record(), parsed, self._args())
        self.assertEqual(result['verdict'], 'NOT ADMITTED')
        self.assertIn('S5_controls', result['failed'])

    def test_a_toolkit_mirror_change_is_not_admitted(self) -> None:
        """S3: the mirror count must be checked against the toolkit, not assumed."""
        record = self._record(toolkit_mirrors=32)
        result = self.query.evaluate_conditions(record, self._parsed(), self._args())
        self.assertEqual(result['verdict'], 'NOT ADMITTED')
        self.assertIn('S3_coverage', result['failed'])

    def test_a_missing_alias_summary_is_not_admitted(self) -> None:
        record = self._record(aliases_missing=[7], aliases_answered=28)
        result = self.query.evaluate_conditions(record, self._parsed(), self._args())
        self.assertEqual(result['verdict'], 'NOT ADMITTED')
        self.assertIn('S3_coverage', result['failed'])

    def test_an_omitted_display_line_is_not_a_coverage_failure(self) -> None:
        """The distinction the S3 fix turns on.

        `events_omitted` bounds the per-event DISPLAY list; the count is uncapped
        and the last-write row is the real final write, so the projection is
        complete. Treating the flag as a coverage failure failed a complete pass.
        """
        parsed = self._parsed()
        parsed['summaries'][0]['truncated'] = True
        result = self.query.evaluate_conditions(self._record(), parsed, self._args())
        self.assertIn('S3_coverage', result['conditions'])
        self.assertEqual(result['conditions']['S3_coverage']['verdict'], 'PASS')
        # S1 is UNKNOWN here because the fixture trace has no record-contract;
        # what this control asserts is that the flag did NOT fail S3.
        self.assertNotIn('S3_coverage', result['failed'])

    def test_value_mismatch_selects_unattributed_not_read_path(self) -> None:
        """W-b: a changed value with a recorded write is an UNRECORDED WRITER."""
        record = self._record()
        record['terminal']['value_at_p'] = '0xDEADBEEF'
        result = self.query.evaluate_conditions(record, self._parsed(), self._args())
        self.assertEqual(result['row'], 'UNATTRIBUTED WRITER')
        self.assertIn('S6_value_consistency', result['failed'])

    def test_read_path_needs_both_no_write_and_a_zero_value(self) -> None:
        """O-UNKNOWN is selectable ONLY under W-b equality with nothing written."""
        record = self._record()
        record['terminal']['value_at_p'] = '0x0000000000000000'
        parsed = self._parsed(lasts={0: None})
        result = self.query.evaluate_conditions(record, parsed, self._args())
        self.assertEqual(result['row'], 'READ PATH')
        self.assertEqual(result['conditions']['S6_value_consistency']['verdict'],
                         'PASS')

    def test_a_nonzero_value_with_no_write_is_not_the_read_path(self) -> None:
        """The trap the ruling names: a value nobody recorded a write for."""
        record = self._record()
        record['terminal']['value_at_p'] = '0x0000000000000001'
        parsed = self._parsed(lasts={0: None})
        result = self.query.evaluate_conditions(record, parsed, self._args())
        self.assertEqual(result['row'], 'UNATTRIBUTED WRITER')
        self.assertNotEqual(result['verdict'], 'ADMITTED')

    def test_missing_contract_is_unknown_not_a_pass(self) -> None:
        """S1 must not default to PASS when the recording metadata is absent."""
        result = self.query.evaluate_conditions(self._record(), self._parsed(), self._args())
        self.assertEqual(result['conditions']['S1_full_mode']['verdict'], 'UNKNOWN')
        self.assertEqual(result['verdict'], 'UNKNOWN')
        self.assertIn('S1_full_mode', result['unknown'])

    def test_read_path_does_not_require_a_contract(self) -> None:
        """The row and the verdict are separate: an UNKNOWN condition is not a row.

        A pass with a complete projection but missing recording metadata still
        SELECTS its row by W-b -- it simply may not be used as a decision input.
        Conflating the two would make the selected row depend on unrelated
        bookkeeping.
        """
        record = self._record()
        record['terminal']['value_at_p'] = '0x0000000000000000'
        parsed = self._parsed(lasts={0: None})
        # The install positive is carried by a SEPARATE pass at the install slot,
        # so this fixture supplies it explicitly; without it S5 fails for a reason
        # unrelated to the row this test is about.
        record['install_positive'] = {'found': True, 'detail': 'fixture'}
        result = self.query.evaluate_conditions(record, parsed, self._args())
        self.assertEqual(result['row'], 'READ PATH')
        self.assertEqual(result['verdict'], 'UNKNOWN')


class TerminalToolTests(unittest.TestCase):
    """The terminal-evidence tool, on the code path a real trace uses.

    Measured: `ttd-terminal.py` first read the guest VA `0x001C4060` as a HOST
    address and reported `????????`, which reads as "TTD recorded no memory there"
    when in fact it read the wrong 64 KB. That is the same defect `ttd-query.py`
    had, and the control below is the one that would have caught it.
    """

    def setUp(self) -> None:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'ttd_terminal', ROOT / 'tools' / 'ttd' / 'ttd-terminal.py')
        self.terminal = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.terminal)

    def test_the_terminal_evidence_comes_from_the_run_log(self) -> None:
        """The log carries the exception and the last kernel call."""
        run = ROOT / 'logs' / 'runs' / '20260930-053722-314-v3-repro-check'
        if not run.is_dir():
            self.skipTest('the horizon-reproducing run is not present')
        evidence = self.terminal.log_evidence(run / 'jsrf_run.log')
        self.assertEqual(len(evidence['invalid_icalls']), 1)
        self.assertEqual(evidence['invalid_icalls'][0]['return_address'], '0014982E')
        self.assertEqual(evidence['exceptions'][-1]['code'], '0xE0424943')
        last = evidence['last_kernel_call']
        self.assertIsNotNone(last)
        self.assertEqual(last['slot_va'],
                         f"0x{0x001C3F60 + last['slot'] * 4:08X}")

    def test_the_slot_va_is_derived_from_the_slot_index(self) -> None:
        """`slot N` is at `0x1C3F60 + N*4` (TR §5), not a second constant."""
        run = ROOT / 'logs' / 'runs' / '20260930-053722-314-v3-repro-check'
        if not run.is_dir():
            self.skipTest('the horizon-reproducing run is not present')
        last = self.terminal.log_evidence(run / 'jsrf_run.log')['last_kernel_call']
        self.assertEqual(int(last['slot_va'], 16),
                         0x001C3F60 + last['slot'] * 4)

    def test_the_guest_to_host_translation_is_required(self) -> None:
        """A log without the mapping line must be refused, not misread.

        The tool cannot translate without the runtime's own line, and reading a
        guest VA as a host address silently returns the wrong memory.
        """
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary)
            (run / 'jsrf_run.log').write_text(
                '[ICALL] invalid target 0x0 tid=1 esp=0 return=0014982E\n',
                encoding='utf-8')
            trace = run / 'x.run'
            trace.write_bytes(b'')
            import subprocess
            import sys
            result = subprocess.run(
                [sys.executable, '-X', 'utf8',
                 str(ROOT / 'tools' / 'ttd' / 'ttd-terminal.py'), str(trace)],
                capture_output=True, text=True)
        self.assertEqual(result.returncode, 3, result.stdout + result.stderr)
        self.assertIn('cannot be translated', result.stderr)

    def test_the_tool_uses_the_shared_cdb_resolver(self) -> None:
        """One resolver, so a WinDbg version bump is fixed in one place."""
        source = (ROOT / 'tools' / 'ttd' / 'ttd-terminal.py').read_text(
            encoding='utf-8')
        self.assertIn('from ttd_query_helpers import resolve_cdb', source)
        query = (ROOT / 'tools' / 'ttd' / 'ttd-query.py').read_text(encoding='utf-8')
        self.assertIn('from ttd_query_helpers import resolve_cdb', query)


if __name__ == '__main__':
    unittest.main(verbosity=2)
