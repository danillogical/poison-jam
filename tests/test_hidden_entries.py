"""Controls for the hidden-entry detector (`scripts/check-hidden-entries.py`).

**The measured failure this pins.** A manifest entry's declared `end` can be
simultaneously its own declared end *and* the next analysis-database function
start, so `check-entry-extents.py` calls it correct, while the span still covers
a complete separate function:

    0x0007DAE0  declared [0x7DAE0, 0x7DE20)
                its own reachable body ends at 0x7DBCB `ret` + 4 NOPs
                and a 190-instruction function begins at 0x7DBD0, whose only
                reference in the whole image is the aligned `.rdata` dword at
                `0x0020D3C8`.

`0x7DE20` is *also* the end of the function at `0x7DBD0`, which is exactly why
an end-versus-next-start check cannot see the defect.  The same shape is
recorded for `0x1FF90` (consumes `0x1FFF0`) and `0x91C00` (consumes `0x91C30`
and `0x91D70`).

**What these controls check.**

1. That the detector still names each real defect, with the *specific consumed
   address*, at its pre-fix span -- the only form in which the miss reproduces,
   because the manifest now carries the corrections.
2. That every corrected span is clean.  A control that fires on both the bad and
   the good span would prove nothing.
3. That a body whose walk has an unresolved indirect exit stays `UNQUALIFIED`
   rather than being reported as a separation.  `0x96F60` ends in
   `jmp dword ptr [eax+8]`; firing there would mean the detector claims a
   separation it cannot prove, which is the `0x80BD0` mistake in a new costume.
4. That the gate has **no baseline**, and that the previously-recorded
   `no rel32 callers` test is not used anywhere as an identity test.
"""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / 'scripts' / 'check-hidden-entries.py'
BASELINE = ROOT / 'config' / 'hidden-entries-baseline.json'

_spec = importlib.util.spec_from_file_location('check_hidden_entries', CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)


def make_finder(manifest_path=None):
    image = checker.Image(ROOT)
    database = checker.Database(ROOT)
    path = manifest_path or ROOT / 'config' / 'recovered-functions.json'
    records = json.loads(Path(path).read_text())
    entries = [checker.Entry(record) for record in records]
    return checker.HiddenEntryFinder(image, database, entries,
                                     checker.load_pointer_sites(ROOT))


class SelfCheckTests(unittest.TestCase):
    """The detector's own positive/negative controls, run through the CLI."""

    def test_selfcheck_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--selfcheck'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         f'self-check failed:\n{result.stdout}\n{result.stderr}')
        self.assertIn('self-check passed', result.stdout)

    def test_every_control_is_exercised(self) -> None:
        """Each control in the checker has a case, and each asserts a verdict."""
        names = {case['name'] for case in checker.CONTROLS}
        self.assertGreaterEqual(len(names), 5)
        for case in checker.CONTROLS:
            self.assertIn('bad', case)
            self.assertIn('must_name', case)
            self.assertIn('expect', case)
            self.assertIn('why', case)


class RealDefectTests(unittest.TestCase):
    """The recorded defects, at their pre-fix spans, against real bytes.

    The committed manifest now carries the repairs, so a pre-fix span would
    legitimately report `OVERLAP` or `SHADOWED` (the consumed address is a
    manifest entry or has a generated body now).  Each test therefore builds a
    finder that omits the repair, which is the state the defect was diagnosed in.
    """

    @classmethod
    def setUpClass(cls) -> None:
        cls.image = checker.Image(ROOT)
        cls.database = checker.Database(ROOT)
        cls.records = json.loads(
            (ROOT / 'config' / 'recovered-functions.json').read_text())
        cls.pointer_sites = checker.load_pointer_sites(ROOT)
        cls.own, cls.dispatched = checker.load_generated_bodies(ROOT)

    def finder_without(self, without=()):
        entries = [checker.Entry(record) for record in self.records
                   if int(record['start'], 16) not in set(without)]
        return checker.HiddenEntryFinder(self.image, self.database, entries,
                                         self.pointer_sites, self.own,
                                         self.dispatched)

    def _evaluate(self, finder, start, end, args=0):
        return finder.evaluate(checker.Entry(
            {'start': '0x%08X' % start, 'end': '0x%08X' % end,
             'stack_args': args}))

    def test_motivating_case_0x7dae0_consumes_0x7dbd0(self) -> None:
        finder = self.finder_without([0x7DBD0])
        verdict, consumed, detail = self._evaluate(finder, 0x7DAE0, 0x7DE20)
        self.assertEqual(verdict, checker.HIDDEN_ENTRY, detail)
        self.assertIn(0x7DBD0, consumed)
        # The declared end is *also* the consumed function's own end, which is
        # the whole reason an end-versus-next-start check cannot see this.
        self.assertIn('0x0020D3C8', detail)

    def test_corrected_0x7dae0_is_clean(self) -> None:
        finder = self.finder_without()
        verdict, _, detail = self._evaluate(finder, 0x7DAE0, 0x7DBCC)
        self.assertNotIn(verdict, checker.GATING, detail)

    def test_0x1ff90_consumes_0x1fff0(self) -> None:
        finder = self.finder_without([0x1FFF0])
        verdict, consumed, detail = self._evaluate(finder, 0x1FF90, 0x200A5, args=4)
        self.assertEqual(verdict, checker.HIDDEN_ENTRY, detail)
        self.assertIn(0x1FFF0, consumed)

    def test_corrected_0x1ff90_is_clean(self) -> None:
        finder = self.finder_without()
        verdict, _, detail = self._evaluate(finder, 0x1FF90, 0x1FFEA, args=4)
        self.assertNotIn(verdict, checker.GATING, detail)

    def test_0x91c00_consumes_both_0x91c30_and_0x91d70(self) -> None:
        finder = self.finder_without([0x91C30, 0x91D70])
        verdict, consumed, detail = self._evaluate(finder, 0x91C00, 0x91EB0)
        self.assertEqual(verdict, checker.HIDDEN_ENTRY, detail)
        self.assertIn(0x91C30, consumed)
        self.assertIn(0x91D70, consumed)

    def test_corrected_0x91c00_is_clean(self) -> None:
        finder = self.finder_without()
        verdict, _, detail = self._evaluate(finder, 0x91C00, 0x91C24)
        self.assertNotIn(verdict, checker.GATING, detail)

    def test_shadowed_case_is_not_given_a_duplicate_entry(self) -> None:
        """`0x556D0` already has a generated body, so it is `SHADOWED`.

        This was a real build failure: adding a manifest entry for it produced
        `LNK2005 sub_000556D0 already defined in recovered.obj`.
        """
        finder = self.finder_without()
        verdict, consumed, detail = self._evaluate(finder, 0x55670, 0x55800)
        self.assertEqual(verdict, checker.SHADOWED, detail)
        self.assertIn(0x556D0, consumed)
        self.assertIn(0x556D0, self.own)


class SoundnessTests(unittest.TestCase):
    """Cases the detector must NOT report as a separation."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.finder = make_finder()

    def test_indirect_exit_is_not_gated(self) -> None:
        """`0x96F60` ends in `jmp [eax+8]`, so separation is not provable."""
        verdict, consumed, detail = self.finder.evaluate(checker.Entry(
            {'start': '0x00096F60', 'end': '0x00097190', 'stack_args': 0}))
        self.assertEqual(verdict, checker.UNQUALIFIED, detail)
        self.assertIn(0x96F80, consumed)

    def test_a_body_using_its_whole_span_is_clean(self) -> None:
        """A tight span has no over-run to report, whatever sits after it."""
        verdict, consumed, detail = self.finder.evaluate(checker.Entry(
            {'start': '0x0007DBD0', 'end': '0x0007DE20', 'stack_args': 0}))
        self.assertEqual(verdict, checker.CLEAN, detail)
        self.assertEqual(consumed, [])

    def test_the_real_seh_function_is_never_called_a_label(self) -> None:
        """`0x80BD0` has zero rel32 callers and is a real standalone SEH function.

        The detector must not treat caller absence as evidence of anything: it
        is a manifest entry in its own right, so no other span may consume it.
        """
        verdict, consumed, detail = self.finder.evaluate(checker.Entry(
            {'start': '0x00080BD0', 'end': '0x00081853', 'stack_args': 0}))
        self.assertNotIn(verdict, checker.GATING, detail)

    def test_plausible_filter_rejects_unaligned_values(self) -> None:
        """The §12 noise filter: an unaligned dword value is not an entry."""
        self.assertFalse(self.finder.plausible(0x000C88D5))
        self.assertFalse(self.finder.plausible(0x001588EA))

    def test_no_caller_based_identity_test_exists(self) -> None:
        """The rejected test must not have crept into the decision procedure.

        The checker's module docstring *discusses* the rejected reasoning, so the
        scan is limited to the code after the docstring.
        """
        source = CHECKER.read_text(encoding='utf-8')
        body = source.split('"""', 2)[-1]
        for banned in ('called_by', 'rel32', 'caller absence'):
            self.assertNotIn(banned, body,
                             f'{banned!r} appears in the decision procedure')


class GateTests(unittest.TestCase):
    """The gate is real, needs no baseline, and fails on a defective manifest."""

    def test_gate_passes_on_the_committed_manifest(self) -> None:
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER)],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         f'gate failed on the committed manifest:\n'
                         f'{result.stdout}\n{result.stderr}')
        self.assertIn('PASS', result.stdout)

    def test_gate_fails_on_a_defective_manifest(self) -> None:
        """The deciding control: a bad span must make the real gate exit nonzero.

        The defect is injected into a temporary copy, never into the committed
        manifest, and the copy is run through the same CLI path `just check`
        uses.  The injected state is the genuine pre-fix one: the container's
        over-wide end is restored *and* the recovered entry for the consumed
        address is removed, which is what the manifest looked like when the
        defect was live.
        """
        import tempfile
        records = json.loads((ROOT / 'config' / 'recovered-functions.json').read_text())
        injected = 0
        kept = []
        for record in records:
            start = int(record['start'], 16)
            if start == 0x7DAE0:
                record = dict(record)
                record['end'] = '0x0007DE20'
                injected += 1
            elif start == 0x7DBD0:
                injected += 1
                continue                      # drop the repair
            kept.append(record)
        self.assertEqual(injected, 2, 'the injection did not find its targets')
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'manifest.json'
            path.write_text(json.dumps(kept), encoding='utf-8')
            result = subprocess.run(
                [sys.executable, '-X', 'utf8', str(CHECKER), '--manifest', str(path)],
                cwd=ROOT, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0,
                            f'the gate passed a defective manifest:\n{result.stdout}')
        self.assertIn('0x0007DBD0', result.stdout)

    def test_no_baseline_file_exists(self) -> None:
        """A baseline here would be a suppressed defect population, not a gate."""
        self.assertFalse(
            BASELINE.exists(),
            f'{BASELINE.name} exists; the gating classes are decidable and must '
            'be repaired rather than frozen')


if __name__ == '__main__':
    unittest.main()
