"""Controls for the stack-depth CFG validator (`scripts/check-stack-depth.py`).

**The measured failure this pins.** Four defects in one session were found only
by spending a game run each, and every one was a stack-contract contradiction that
is visible statically from the original bytes:

* `0x00080BD0` declared `[0x80BD0, 0x80C83)` with `stack_args 4`; the end is the
  **join point of four branches inside the function**, so the body ran its whole
  prologue and never reached its epilogue. The run measured `esp
  00F7FE70->00F7FE30`, a delta of exactly `-0x40`.
* `0x000BB7B0` declared `[0xBB7B0, 0xBBA04)`, cutting the shared tail-merged
  epilogue at `0xBBA17` that three in-span branches and jump-table slot 5 target.
* `0x0007DA30` declared `[0x7DA30, 0x7DA84)` with `stack_args 0`; the span was
  truncated and hid a `ret 4`.
* `0x00074C70` declared `stack_args 4` for a body with **no `ret` of its own** --
  it ends `pop edi; pop esi; jmp 0x6A770`.

Plus `0x000307A0`, whose extent ended in the middle of `test esi,esi` and allowed
a reachable fall-through into a symbol decoded from mid-instruction.

**What these controls check.** That the detector still reports each of those, with
the *verdict and code it was diagnosed under*, when replayed at its pre-fix span
-- the only form in which the miss is reproducible, because the manifest now
carries the corrections. Asserting only "is it bad" would still pass if the
detector reported the wrong defect, and the non-gating classes have no other
regression coverage because they do not fail the build.

**And that the corrected entries are clean.** A control that fires on both the bad
and the good span would prove nothing, so every case asserts the corrected entry is
not `DEFECT`.

**And that the gate has no baseline.** The whole point of narrowing `DEFECT` to one
mechanically-decidable class was to avoid freezing live defects, which is how
`0x000307A0` stayed hidden inside `config/entry-extent-baseline.json` while
`just check` stayed green. A `config/stack-depth-baseline.json` appearing without a
recorded reason is therefore itself a finding, not a convenience.
"""
from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER = ROOT / 'scripts' / 'check-stack-depth.py'
BASELINE = ROOT / 'config' / 'stack-depth-baseline.json'

_spec = importlib.util.spec_from_file_location('check_stack_depth', CHECKER)
assert _spec and _spec.loader
checker = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(checker)


class SelfCheckTests(unittest.TestCase):
    """The detector's own positive/negative controls, run through the CLI."""

    def test_selfcheck_passes(self) -> None:
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--selfcheck'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         f'self-check failed:\n{result.stdout}\n{result.stderr}')
        self.assertIn('self-check passed', result.stdout)

    def test_every_historical_case_is_a_control(self) -> None:
        """Each defect the docstring names has a case, and each asserts a code."""
        named = {'0x00080BD0', '0x0007DA30', '0x00074C70', '0x000BB7B0',
                 '0x000307A0', '0x000246E0', '0x00152BC0', '0x00021010',
                 '0x000F4FF0'}
        covered = {case['bad']['start'] for case in checker.CONTROLS}
        self.assertEqual(named - covered, set(),
                         'a named historical defect has no control')
        for case in checker.CONTROLS:
            self.assertIn('code', case, f"{case['name']} asserts no code")
            self.assertIn('good', case, f"{case['name']} has no negative half")

    def test_the_per_path_rule_is_pinned(self) -> None:
        """The false-negative that cost this class four defects must stay fixed.

        An earlier revision gated only when **every** reachable exit sat at depth
        0.  `0x000F4FF0`'s `ret 4` is reached at depth 0 by one path and at
        UNKNOWN by another, so that rule reported the entry merely `UNKNOWN` and
        hid a live defect.  `0x00021010` is the same shape via an ABSENT key.  Both
        must be `DEFECT` now, and `0x0007DA30` -- which has *no* depth-0 path --
        must still not be.
        """
        cases = {case['bad']['start']: case for case in checker.CONTROLS}
        for start in ('0x000F4FF0', '0x00021010', '0x00152BC0'):
            case = cases[start]
            self.assertEqual(case['verdict'], checker.DEFECT,
                             f'{start} is not gated; the all-depths false-negative '
                             f'has regressed')
            self.assertEqual(case['code'], 'STACK_ARGS')
        # The other half: an entry with no depth-0 path must NOT be gated, or the
        # rule would be gating on an unresolved depth.
        da30 = cases['0x0007DA30']
        self.assertEqual(da30['verdict'], checker.UNKNOWN,
                         '0x0007DA30 has no depth-0 path and must not be gated')

    def test_the_rule_is_not_the_unsound_general_form(self) -> None:
        """`N + d` at a *nonzero* depth must not be gated.

        The general form would fire on entries whose walk pops registers the body
        never pushed -- the signature of a mid-function entry or an over-wide span,
        which is an *extent* question.  Measured: all 29 such entries have `d > 0`.
        This asserts the specific behaviour rather than only "0 DEFECTs" (which
        `test_gate_passes_with_no_baseline` already covers): no gated finding may
        cite a nonzero depth, and `0x1BCB14` -- correct at `d = +8` -- must stay
        ungated.
        """
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--json'],
            cwd=ROOT, capture_output=True, text=True)
        payload = json.loads(result.stdout)
        defects = [r for r in payload['results'] if r['verdict'] == 'DEFECT']
        self.assertEqual(defects, [],
                         f'the committed manifest should have 0 DEFECTs, got: '
                         f'{[(r["start"], r["detail"]) for r in defects]}')
        # No gated finding anywhere may rest on a nonzero depth.
        for record in payload['results']:
            if record['verdict'] != 'DEFECT':
                continue
            self.assertNotIn('depth +', record['detail'],
                             f'{record["start"]} is gated on a nonzero depth')
        # 0x1BCB14 is correct at d = +8 and must never be gated.  `--json`
        # reports `start` as an integer, not the manifest's hex string.
        by_start = {r['start']: r for r in payload['results']}
        entry = by_start.get(0x001BCB14)
        self.assertIsNotNone(entry, '0x001BCB14 is not in the report')
        self.assertNotEqual(entry['verdict'], 'DEFECT',
                            '0x001BCB14 is correct at d = +8 and must stay ungated')

    def test_ret_depth_is_reported_per_path(self) -> None:
        """A resolved nonzero-depth `ret` must be reported even if other paths are UNKNOWN.

        The `resolved`-gated version hid 14 of the 29 such entries behind an
        unrelated unresolved path -- the same false-negative shape as the gating
        rule, one class down.
        """
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--json'],
            cwd=ROOT, capture_output=True, text=True)
        payload = json.loads(result.stdout)
        ret_depth = [r for r in payload['results'] if r['code'] == 'RET_DEPTH']
        self.assertGreaterEqual(
            len(ret_depth), 29,
            f'RET_DEPTH should report at least the 29 entries with a resolved '
            f'nonzero-depth ret, got {len(ret_depth)}')
        for record in ret_depth:
            self.assertIn('depth +', record['detail'],
                          f'{record["start"]} is RET_DEPTH but cites no positive '
                          f'depth: {record["detail"]}')


class GateTests(unittest.TestCase):
    """The gate's own behaviour on the committed manifest."""

    def test_gate_passes_with_no_baseline(self) -> None:
        """Zero DEFECTs on the committed manifest, and no frozen baseline."""
        self.assertFalse(
            BASELINE.exists(),
            'config/stack-depth-baseline.json exists. The gate is designed to pass '
            'with no baseline: DEFECT is narrowed to one class that is decidable '
            'from the entry\'s own bytes. A baseline means live defects were frozen '
            'instead of repaired, which is the failure 0x000307A0 documents.')
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER)],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0,
                         f'gate failed:\n{result.stdout[-2000:]}\n{result.stderr}')
        # The verdict summary lists only non-zero verdicts, so "0 DEFECT" is never
        # printed.  The deciding assertion is that the word DEFECT is absent from
        # the summary line and from every finding header.
        summary = next((line for line in result.stdout.splitlines()
                        if line.startswith('verdicts:')), '')
        self.assertNotIn('DEFECT', summary,
                         f'the gate reported a DEFECT on the committed manifest: '
                         f'{summary}')
        self.assertNotIn('\nDEFECT (', result.stdout,
                         'the gate listed a DEFECT section on the committed manifest')

    def test_a_broken_manifest_fails_the_gate(self) -> None:
        """The deciding control: the gate must be able to fail.

        A copy of the manifest with one proven entry's `stack_args` reverted to its
        pre-fix value must make the gate exit nonzero and name that entry. Without
        this the gate could be vacuously green.
        """
        records = json.loads((ROOT / 'config' / 'recovered-functions.json').read_text())
        target = '0x000246E0'
        for record in records:
            if record['start'] == target:
                record['stack_args'] = 0        # the pre-fix, wrong value
                break
        else:
            self.fail(f'{target} is not in the manifest')

        scratch = ROOT / 'build' / 'stack-depth-control-manifest.json'
        scratch.parent.mkdir(parents=True, exist_ok=True)
        scratch.write_text(json.dumps(records), encoding='utf-8')
        try:
            result = subprocess.run(
                [sys.executable, '-X', 'utf8', str(CHECKER),
                 '--manifest', str(scratch)],
                cwd=ROOT, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0,
                                'the gate passed a manifest with a reverted defect')
            self.assertIn(target.upper(), result.stdout.upper(),
                          'the gate did not name the reverted entry')
            self.assertIn('DEFECT', result.stdout,
                          'the gate did not report a DEFECT for the reverted entry')
        finally:
            scratch.unlink(missing_ok=True)

    def test_deterministic_output(self) -> None:
        """Two runs must agree byte for byte; the gate is not order-dependent."""
        runs = [subprocess.run([sys.executable, '-X', 'utf8', str(CHECKER), '--limit', '5'],
                               cwd=ROOT, capture_output=True, text=True).stdout
                for _ in range(2)]
        self.assertEqual(runs[0], runs[1], 'the validator is not deterministic')

    def test_json_output_covers_every_entry(self) -> None:
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(CHECKER), '--json'],
            cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0)
        payload = json.loads(result.stdout)
        manifest = json.loads((ROOT / 'config' / 'recovered-functions.json').read_text())
        self.assertEqual(len(payload['results']), len(manifest),
                         'the JSON report does not cover every manifest entry')
        self.assertEqual(sum(payload['counts'].values()), len(manifest))


class ModelTests(unittest.TestCase):
    """Unit controls on the depth model itself, independent of the manifest."""

    def test_unknown_is_never_zero(self) -> None:
        """An unresolved value must not silently behave as 0."""
        self.assertIsNot(checker.UNK, 0)
        self.assertFalse(bool(checker.UNK))

    def test_stack_args_is_the_ret_immediate(self) -> None:
        """The gating rule's arithmetic: `ret N` removes 4+N, so args == N at d=0."""
        entry = checker.Entry({'start': '0x000246E0', 'end': '0x00024700',
                               'stack_args': 4})
        self.assertEqual(entry.expected, 8)

    def test_folded_detection_methods_are_not_genuine_entries(self) -> None:
        """The `ff4d442` alias fold and the `push`-after-`ret` pass are both false."""
        self.assertIn('tail_jump_alias', checker.FOLDED_DETECTION)
        self.assertIn('gap_prologue', checker.FOLDED_DETECTION)


if __name__ == '__main__':
    unittest.main()
