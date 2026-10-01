"""Controls for W2's premise-qualification gate.

Plan W2 requires five items before any Planner call, and names what each prevents:
"A2h-r6 six revisions on a fixed failure; the 571 MB premise ~5 days, refuted by the
`test eax`/`jl` four instructions after the call; 15 DR0 ON runs before zero hits
were noticed; the slot field named by the Halo XDK layout only after the owner's
audit".

Every one of those is a premise a mechanical check would have refused, so the gate's
value is entirely in its ability to say no. These controls therefore weight the
negative cases: each item gets a case that must FAIL or be UNKNOWN, paired with one
that must PASS, because a gate that cannot refuse is worse than no gate -- it carries
the appearance of qualification.

Three defects found while building the gate are encoded here as regressions:

  * the profile parser read the summary line's last token instead of the run's
    classification, so a STRICT run was reported as a failure;
  * the disassembly check decoded from `address - 16`, where capstone's `skipdata`
    re-synchronisation had drifted, and reported a real instruction boundary as
    absent;
  * the report crashed on an item with no `detail` key.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'qualify_premise', ROOT / 'scripts' / 'qualify-premise.py')
assert _spec and _spec.loader
gate = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gate)


class DisassemblyItemTests(unittest.TestCase):
    """Item 1: the failing instruction must be a real instruction boundary."""

    def test_a_real_boundary_passes(self) -> None:
        """`0x00149828` is `call dword ptr [0x1c4064]` -- the recorded stop site."""
        result = gate.check_disassembly({'failing_instruction': 0x00149828})
        self.assertEqual(result['verdict'], 'PASS', result.get('detail'))
        self.assertIn('call', result['instruction'])

    def test_a_missing_address_is_unknown_not_pass(self) -> None:
        result = gate.check_disassembly({'failing_instruction': None})
        self.assertEqual(result['verdict'], 'UNKNOWN')
        self.assertIn('cannot be checked', result['detail'])

    def test_the_decode_start_is_reported(self) -> None:
        """A decode whose alignment is not established is not evidence.

        Measured: decoding from `address - 16` produced a listing in which
        `0x00149828` did not appear, while a wider window found the correct
        instruction. Reporting the start lets a reader judge the alignment.
        """
        result = gate.check_disassembly({'failing_instruction': 0x00149828})
        self.assertIn('decode_start', result)
        self.assertTrue(result['decode_start'].startswith('0x'))

    def test_a_mid_instruction_address_does_not_falsely_pass(self) -> None:
        """An address interior to an instruction must not read as a boundary.

        `0x0014982A` is inside `call dword ptr [0x1c4064]` (which starts at
        `0x00149828` and is 6 bytes). Whether the decoder happens to emit a line
        there depends on its re-synchronisation, so the honest verdicts are FAIL (a
        `.byte` line) or UNKNOWN -- never a silent PASS with a plausible
        instruction.
        """
        result = gate.check_disassembly({'failing_instruction': 0x0014982A})
        self.assertIn(result['verdict'], ('FAIL', 'UNKNOWN'), result.get('detail'))


class ProfileParsingTests(unittest.TestCase):
    """Item 2: a strict run must classify STRICT, read from the run's own line."""

    def test_a_strict_run_is_not_reported_as_a_failure(self) -> None:
        """The regression: the parser read the summary's last token.

        `check-run-profile.py` prints the run's line, a blank line, then
        `checked 1; strict 1, exploratory 0, fixture 0, unknown 0, missing 0`.
        Taking the last token of the whole output yielded "missing".
        """
        run = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
        if not run.is_dir():
            self.skipTest('the archived strict run is not present')
        result = gate.check_run(run, {'fixed_by': []})
        self.assertEqual(result['profile'], 'STRICT', result.get('profile_output'))
        self.assertEqual(result['profile_verdict'], 'PASS')


class ProfilePurposeTests(unittest.TestCase):
    """Item 2 under pragmatism: bare-minimum lines may rest on exploratory runs."""

    def test_fidelity_refuses_an_exploratory_run(self) -> None:
        verdict, _ = gate.profile_verdict('r', 'EXPLORATORY', 'fidelity', ['L16'])
        self.assertEqual(verdict, 'FAIL')

    def test_bare_minimum_accepts_an_exploratory_run_with_ledger_ids(self) -> None:
        verdict, detail = gate.profile_verdict('r', 'EXPLORATORY', 'bare-minimum',
                                               ['L16', 'L18'])
        self.assertEqual(verdict, 'PASS', detail)

    def test_bare_minimum_without_ledger_ids_is_unknown(self) -> None:
        verdict, detail = gate.profile_verdict('r', 'EXPLORATORY', 'bare-minimum', [])
        self.assertEqual(verdict, 'UNKNOWN')
        self.assertIn('--ledger-id', detail)

    def test_an_id_the_ledger_lacks_fails(self) -> None:
        verdict, detail = gate.profile_verdict('r', 'EXPLORATORY', 'bare-minimum',
                                               ['L16', 'L999'])
        self.assertEqual(verdict, 'FAIL')
        self.assertIn('L999', detail)

    def test_bare_minimum_still_refuses_a_fixture_run(self) -> None:
        verdict, _ = gate.profile_verdict('r', 'FIXTURE', 'bare-minimum', ['L16'])
        self.assertEqual(verdict, 'FAIL')

    def test_strict_passes_either_purpose(self) -> None:
        for purpose in ('fidelity', 'bare-minimum'):
            self.assertEqual(gate.profile_verdict('r', 'STRICT', purpose, [])[0], 'PASS')


class FreshnessItemTests(unittest.TestCase):
    """Item 2: the premise must postdate the fixes the brief names."""

    def test_no_fix_named_is_unknown_not_pass(self) -> None:
        """The A2h-r6 failure: six revisions on a failure that was already fixed."""
        run = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
        if not run.is_dir():
            self.skipTest('the archived strict run is not present')
        result = gate.check_run(run, {'fixed_by': []})
        self.assertEqual(result['freshness_verdict'], 'UNKNOWN')
        self.assertIn('predates a fix', result['freshness_detail'])

    def test_ancestry_is_decided_not_assumed(self) -> None:
        """A bogus commit must not read as contained."""
        self.assertIsNone(gate.is_ancestor(gate.TOOLKIT, 'f' * 40, 'HEAD'))
        code, head = gate.git(gate.TOOLKIT, 'rev-parse', 'HEAD')
        if code == 0 and head:
            self.assertTrue(gate.is_ancestor(gate.TOOLKIT, head.strip(),
                                             head.strip()))


class PositiveControlItemTests(unittest.TestCase):
    """Item 3: an instrument that never fired produced no observation."""

    def test_an_absent_control_fails(self) -> None:
        run = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
        if not run.is_dir():
            self.skipTest('the archived strict run is not present')
        result = gate.check_positive_control(run, 'NO-SUCH-MARKER-ANYWHERE')
        self.assertEqual(result['verdict'], 'FAIL')
        self.assertIn('unsupported', result['detail'])

    def test_a_present_control_passes(self) -> None:
        run = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
        if not run.is_dir():
            self.skipTest('the archived strict run is not present')
        result = gate.check_positive_control(
            run, 'Kernel thunk bridge: resolving')
        self.assertEqual(result['verdict'], 'PASS')
        self.assertIn('fired', result['detail'])

    def test_no_control_named_is_unknown(self) -> None:
        run = ROOT / 'logs' / 'runs' / '20260930-001405-390-v1-verified-strict'
        result = gate.check_positive_control(run, None)
        self.assertEqual(result['verdict'], 'UNKNOWN')


class DryRunItemTests(unittest.TestCase):
    """Item 4: a frozen command that names a missing script cannot run."""

    def test_a_missing_script_fails(self) -> None:
        result = gate.check_dry_run(['python -X utf8 scripts/does-not-exist.py'])
        self.assertEqual(result['verdict'], 'FAIL')

    def test_a_real_script_passes(self) -> None:
        result = gate.check_dry_run(['python -X utf8 scripts/run-jsrf.py --seconds 5'])
        self.assertEqual(result['verdict'], 'PASS')

    def test_no_commands_is_unknown(self) -> None:
        self.assertEqual(gate.check_dry_run([])['verdict'], 'UNKNOWN')

    def test_a_command_without_a_script_is_unknown(self) -> None:
        result = gate.check_dry_run(['cmake --build build'])
        self.assertEqual(result['verdict'], 'UNKNOWN')


class SymptomSearchItemTests(unittest.TestCase):
    """Item 5: the symptom is searched for before a Planner call."""

    def test_no_symptom_is_unknown(self) -> None:
        self.assertEqual(gate.check_symptom_search(None)['verdict'], 'UNKNOWN')

    def _prior_art(self, root: Path, names) -> Path:
        for name in names:
            (root / name).mkdir(parents=True)
            (root / name / 'notes.md').write_text('the thunk table was overwritten\n',
                                                 encoding='utf-8')
        return root

    def test_a_search_reports_its_hits(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            prior = self._prior_art(Path(tmp), [n for n, _ in gate.PRIOR_ART])
            result = gate.check_symptom_search('thunk table', prior)
        self.assertEqual(result['verdict'], 'PASS')
        for name, _url in gate.PRIOR_ART:
            self.assertEqual(result['hits'][name], ['notes.md'])
        self.assertIn('hits', result)
        # The toolkit history names the thunk table, so a search must find something
        # -- a zero here would mean the search is not reaching the repositories.
        total = sum(len(v) for v in result['hits'].values())
        self.assertGreater(total, 0, 'the search found nothing anywhere')

    def test_a_missing_prior_art_checkout_is_unknown(self) -> None:
        """A search that skipped a prior-art title is not a search of the set."""
        with tempfile.TemporaryDirectory() as tmp:
            prior = self._prior_art(Path(tmp), [gate.PRIOR_ART[0][0]])
            result = gate.check_symptom_search('thunk table', prior)
        self.assertEqual(result['verdict'], 'UNKNOWN')
        self.assertIn(gate.PRIOR_ART[1][1], result['detail'])


class VerdictTests(unittest.TestCase):
    """The overall verdict must fail closed."""

    def test_unknown_blocks(self) -> None:
        """A gate that defaulted to PASS would carry false assurance."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'qualify-premise.py'),
             '--run', '20260930-001405-390-v1-verified-strict'],
            capture_output=True, text=True)
        # No --fixed-by, no --positive-control: two items are UNKNOWN, so the gate
        # must refuse even though three items pass.
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('INCOMPLETE', result.stdout)
        self.assertIn('Do NOT take this premise to the Planner', result.stdout)

    def test_the_report_does_not_crash_on_a_missing_detail(self) -> None:
        """The regression: the report read `entry['detail']` unconditionally."""
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'qualify-premise.py'),
             '--run', '20260930-001405-390-v1-verified-strict',
             '--failing-instruction', '0x00149828'],
            capture_output=True, text=True)
        self.assertNotIn('Traceback', result.stderr, result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
