"""Controls for W14's strict-horizon ledger lint.

Plan W14's check is: "ledger lint: **a session with a run and no ledger line
fails**". A lint for that rule has two ways to be useless, and each needs its own
control:

  * it never fails, so an uncovered run passes; and
  * it always fails, so it is red from its first run and gets ignored.

The second is the failure this project names repeatedly, and the delivered lint hit
it: applying the rule to the whole run archive reported **40 strict runs with no
line**, every one of them predating the ledger's existence. A boundary at the
ledger's own earliest row fixed that, and these controls keep the boundary from
turning into a blanket waiver.

The scratch ledger and run archive are built in a temporary directory, so the
controls exercise the real code path without depending on, or mutating, the
repository's own records.
"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_spec = importlib.util.spec_from_file_location(
    'check_horizon_ledger', ROOT / 'scripts' / 'check-horizon-ledger.py')
assert _spec and _spec.loader
ledger_lint = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ledger_lint)


def run_name(stamp: str, label: str = 'strict') -> str:
    return f'{stamp}-{label}'


class LedgerLintTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        (self.root / 'logs' / 'runs').mkdir(parents=True)
        (self.root / 'docs' / 'reviews').mkdir(parents=True)
        self._saved = (ledger_lint.ROOT, ledger_lint.LEDGER, ledger_lint.RUNS)
        ledger_lint.ROOT = self.root
        ledger_lint.LEDGER = self.root / 'docs' / 'reviews' / 'strict-horizon-ledger.md'
        ledger_lint.RUNS = self.root / 'logs' / 'runs'

    def tearDown(self) -> None:
        ledger_lint.ROOT, ledger_lint.LEDGER, ledger_lint.RUNS = self._saved
        self._tmp.cleanup()

    def write_ledger(self, body: str) -> None:
        ledger_lint.LEDGER.write_text(body, encoding='utf-8')

    def add_run(self, name: str) -> Path:
        path = ledger_lint.RUNS / name
        path.mkdir(parents=True, exist_ok=True)
        return path

    def audit(self) -> tuple[list[dict], dict]:
        """Run the lint's checks the way main() does, without its CLI plumbing."""
        rows = ledger_lint.ledger_rows()
        named = ledger_lint.all_named_runs()
        findings: list[dict] = []

        epoch = ledger_lint.ledger_epoch()
        epoch_compact = epoch.replace('-', '') if epoch else None
        for run in sorted(ledger_lint.RUNS.iterdir()):
            if not run.is_dir() or run.name in named:
                continue
            if epoch_compact and run.name[:8] < epoch_compact:
                continue
            findings.append({'check': 'ledger_coverage',
                             'reason': 'strict_run_without_a_line',
                             'run': run.name})
        return findings, {'rows': rows, 'named': named, 'epoch': epoch}

    def test_an_uncovered_run_is_a_finding(self) -> None:
        """The plan's named check, in its positive direction."""
        name = run_name('20260930-120000-000-uncovered')
        self.add_run(name)
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            '| 2026-09-30 | `abc1234` | `def5678` | '
            '`20260930-110000-000-covered` | event | site | 10:00 |\n')
        findings, _ = self.audit()
        self.assertEqual([f['run'] for f in findings], [name])

    def test_a_covered_run_is_not_a_finding(self) -> None:
        """Known-good: a run the ledger names must pass."""
        name = run_name('20260930-110000-000-covered')
        self.add_run(name)
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            f'| 2026-09-30 | `abc1234` | `def5678` | `{name}` | event | site | 10:00 |\n')
        findings, _ = self.audit()
        self.assertEqual(findings, [])

    def test_a_run_named_outside_the_session_table_counts(self) -> None:
        """A run named in a distribution table is covered.

        Measured: the real ledger names four runs by full directory name in its
        "Observed first-site distribution" table, and a scan that read only the
        date-prefixed rows reported them as uncovered -- a false failure that would
        have been "fixed" by editing a correct table.
        """
        name = run_name('20260930-130000-000-distribution')
        self.add_run(name)
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            '| 2026-09-30 | `abc1234` | `def5678` | '
            '`20260930-110000-000-other` | event | site | 10:00 |\n'
            '\n## Distribution\n\n'
            f'| Site | Count | Runs |\n|---|---|---|\n| `0x1234` | 1 | `{name}` |\n')
        findings, _ = self.audit()
        self.assertEqual(findings, [])

    def test_a_run_older_than_the_ledger_is_not_a_finding(self) -> None:
        """The boundary: the ledger cannot cover a run that predates it."""
        self.add_run(run_name('20260912-090000-000-ancient'))
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            '| 2026-09-30 | `abc1234` | `def5678` | '
            '`20260930-110000-000-covered` | event | site | 10:00 |\n')
        findings, _ = self.audit()
        self.assertEqual(findings, [])

    def test_the_boundary_is_not_a_blanket_waiver(self) -> None:
        """A run on or after the ledger's first row must still fail.

        Without this, deleting the coverage check entirely would leave the
        pre-ledger control passing.
        """
        self.add_run(run_name('20260930-140000-000-after-epoch'))
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            '| 2026-09-30 | `abc1234` | `def5678` | '
            '`20260930-110000-000-covered` | event | site | 10:00 |\n')
        findings, _ = self.audit()
        self.assertEqual(len(findings), 1)

    def test_epoch_comes_from_the_ledger_not_a_constant(self) -> None:
        self.write_ledger(
            '| Date | Game | Toolkit | Run ID | Event | Sites | Stop |\n'
            '|---|---|---|---|---|---|---|\n'
            '| 2026-08-01 | `abc1234` | `def5678` | '
            '`20260801-110000-000-first` | event | site | 10:00 |\n')
        self.assertEqual(ledger_lint.ledger_epoch(), '2026-08-01')

    def test_an_empty_ledger_has_no_epoch(self) -> None:
        self.write_ledger('# Ledger\n\nNo rows yet.\n')
        self.assertIsNone(ledger_lint.ledger_epoch())
        findings, _ = self.audit()
        self.assertEqual(findings, [])


class RealLedgerTests(unittest.TestCase):
    """The delivered ledger must pass its own lint."""

    def test_real_ledger_has_no_uncovered_run(self) -> None:
        import subprocess
        import sys
        result = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-horizon-ledger.py'),
             '--since', '2026-09-29'],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_bare_invocation_applies_the_ledger_scope(self) -> None:
        """Without --since the lint uses the recorded scope, and says so."""
        import json
        import subprocess
        import sys
        script = str(ROOT / 'scripts' / 'check-horizon-ledger.py')
        bare = subprocess.run([sys.executable, '-X', 'utf8', script, '--json'],
                              capture_output=True, text=True)
        self.assertEqual(json.loads(bare.stdout)['since'], '2026-09-29',
                         bare.stdout + bare.stderr)
        whole = subprocess.run([sys.executable, '-X', 'utf8', script, '--json',
                                '--since', 'all'], capture_output=True, text=True)
        self.assertEqual(json.loads(whole.stdout)['since'], 'all',
                         whole.stdout + whole.stderr)

    def test_real_ledger_rows_name_real_runs(self) -> None:
        """Every row must name a run directory that exists, so it can be re-derived."""
        text = (ROOT / 'docs' / 'reviews' / 'strict-horizon-ledger.md').read_text(
            encoding='utf-8')
        runs_root = ROOT / 'logs' / 'runs'
        if not runs_root.is_dir():
            self.skipTest('no run archive on this host')
        existing = {p.name for p in runs_root.iterdir() if p.is_dir()}
        missing = [name for name in ledger_lint.RUN_ID.findall(text)
                   if name not in existing]
        self.assertEqual(missing, [],
                         f'ledger rows name runs that do not exist: {missing}')


if __name__ == '__main__':
    unittest.main(verbosity=2)
