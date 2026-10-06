"""`scripts/logq.py`: the present-ceiling tables and saved query.

A synthetic run log stands in for a real one. Parsing is asserted through
`parse_all` (no query engine involved); the saved query and the line accounting
need DuckDB and are skipped without it.
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))

try:
    import duckdb  # noqa: F401
    import logq
    HAVE_DUCKDB = True
except (ImportError, SystemExit):
    logq = None
    HAVE_DUCKDB = False

LOG = '\n'.join([
    '[BOOT] starting',
    '  [FBPRESENT] t=10s presents=100 hash=aaaaaaaaaaaaaaaa CHANGED',
    '  [FBPRESENT] t=20s presents=500 hash=bbbbbbbbbbbbbbbb CHANGED',
    '  [FBPRESENT] t=30s presents=1000 hash=5bdaea576b8509f5 unchanged',
    '  [FBPRESENT] t=40s presents=1000 hash=5bdaea576b8509f5 unchanged',
    '  [FBPRESENT] t=50s presents=1000 hash=5bdaea576b8509f5 unchanged',
    '  [PFIFO] user_write    NV_PFIFO_CACHE1_DMA_PUT = 00001000',
    '  [PFIFO] submit #3 diag=ok get=00001000 put=00002000 method=0100 subch=2 param=0000ABCD at=00001FFC',
    '  [PFIFO] reject diag=unsupported_method method=0F40 subch=1 param=DEADBEEF at=00001800 get=00001000 put=00002000 successes=7 rejections=1',
    '  [PFIFO] still rejecting n=16 diag=unsupported_method method=0F40 subch=1 param=DEADBEEF at=00001800 get=00001000 put=00002000',
    '  [PFIFO] recovered after 300 rejections get=00002000 put=00002000 successes=8',
    '  [PFIFO] admit-unknown class=97 method=1A40 param=00000001 at=00003000',
    '  [PFIFO] admit-unknown class=97 method=1A40 param=00000002 at=00003010',
    '  [PFIFO] admit-unknown class=62 method=0300 param=00000003 at=00003020',
    '  [PFIFO] budget_exhausted get=00001000 put=00009000 begin=00001000 end=00009000 words=4096 packets=12 pc=00001234',
    '          last 32 visit addresses:',
    '[GPU] flips 975 (increment-write), flip stalls 974; consumer: submission walk, 0 non-NV097 method(s) skipped',
    '[KERNEL] #1: ordinal 5 (slot 2) esp=0x1000 ret=0x2000 tid=1',
    '',
])
LINE_COUNT = len(LOG.splitlines())


def lineno_of(fragment: str, occurrence: int = 1) -> int:
    seen = 0
    for number, line in enumerate(LOG.splitlines(), start=1):
        if fragment in line:
            seen += 1
            if seen == occurrence:
                return number
    raise AssertionError(fragment)


@unittest.skipUnless(HAVE_DUCKDB, 'duckdb not importable')
class ParseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        rows = []
        for n, line in enumerate(LOG.splitlines(), start=1):
            m = logq.TAG.match(line)
            rows.append(('r', n, m.group(1) if m else '', line.rstrip()))
        cls.tables = logq.parse_all(rows, 'r')

    def test_presents(self) -> None:
        self.assertEqual(self.tables['presents'], [
            ('r', lineno_of('t=10s'), 10, 100, 'aaaaaaaaaaaaaaaa', True),
            ('r', lineno_of('t=20s'), 20, 500, 'bbbbbbbbbbbbbbbb', True),
            ('r', lineno_of('t=30s'), 30, 1000, '5bdaea576b8509f5', False),
            ('r', lineno_of('t=40s'), 40, 1000, '5bdaea576b8509f5', False),
            ('r', lineno_of('t=50s'), 50, 1000, '5bdaea576b8509f5', False),
        ])

    def pfifo(self, kind: str) -> list[tuple]:
        return [r for r in self.tables['pfifo'] if r[2] == kind]

    def test_pfifo_one_row_per_pfifo_decision_line(self) -> None:
        kinds = [r[2] for r in self.tables['pfifo']]
        self.assertEqual(kinds, ['submit', 'reject', 'still_rejecting', 'recovered',
                                 'admit_unknown', 'admit_unknown', 'admit_unknown',
                                 'budget_exhausted'])

    def test_pfifo_columns_are_run_lineno_kind_diag_n_method_subch_param_at_get_put_class(self) -> None:
        # (run, lineno, kind, diag, n, method, subch, param, at, get, put, class_id)
        self.assertEqual(self.pfifo('submit'), [
            ('r', lineno_of('submit #3'), 'submit', 'ok', 3, 0x100, 2, 0xABCD,
             0x1FFC, 0x1000, 0x2000, None)])

    def test_reject(self) -> None:
        self.assertEqual(self.pfifo('reject'), [
            ('r', lineno_of('[PFIFO] reject'), 'reject', 'unsupported_method', None,
             0xF40, 1, 0xDEADBEEF, 0x1800, 0x1000, 0x2000, None)])

    def test_still_rejecting_carries_n(self) -> None:
        self.assertEqual(self.pfifo('still_rejecting'), [
            ('r', lineno_of('still rejecting'), 'still_rejecting', 'unsupported_method',
             16, 0xF40, 1, 0xDEADBEEF, 0x1800, 0x1000, 0x2000, None)])

    def test_recovered_puts_rejection_count_in_n(self) -> None:
        self.assertEqual(self.pfifo('recovered'), [
            ('r', lineno_of('recovered after'), 'recovered', None, 300, None, None,
             None, None, 0x2000, 0x2000, None)])

    def test_admit_unknown_carries_class(self) -> None:
        rows = self.pfifo('admit_unknown')
        self.assertEqual(rows[0], ('r', lineno_of('class=97', 1), 'admit_unknown', None,
                                   None, 0x1A40, None, 1, 0x3000, None, None, 0x97))
        self.assertEqual(rows[2][11], 0x62)
        self.assertEqual(rows[2][5], 0x300)

    def test_budget_exhausted(self) -> None:
        self.assertEqual(self.pfifo('budget_exhausted'), [
            ('r', lineno_of('budget_exhausted'), 'budget_exhausted', None, None, None,
             None, None, None, 0x1000, 0x9000, None)])

    def test_gpu_flips(self) -> None:
        self.assertEqual(self.tables['gpu_flips'],
                         [('r', lineno_of('[GPU] flips'), 975, 974)])


@unittest.skipUnless(HAVE_DUCKDB, 'duckdb not importable')
class RunTests(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.run_dir = Path(self._tmp.name) / 'run-x'
        self.run_dir.mkdir()
        (self.run_dir / 'jsrf_run.log').write_text(LOG, encoding='utf-8')

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def test_coverage_counts_every_line_and_new_tables(self) -> None:
        connection = duckdb.connect(':memory:')
        counts = logq.build(connection, 'run-x', self.run_dir / 'jsrf_run.log')
        self.assertEqual(counts['lines'], LINE_COUNT)
        self.assertEqual(counts['_total_lines'], LINE_COUNT)
        self.assertEqual(connection.execute('SELECT count(*) FROM lines').fetchone()[0],
                         LINE_COUNT)
        self.assertEqual((counts['presents'], counts['pfifo'], counts['gpu_flips']),
                         (5, 8, 1))
        self.assertEqual(connection.execute(
            "SELECT count(*) FROM lines WHERE tag IN ('FBPRESENT','PFIFO','GPU')"
        ).fetchone()[0], 5 + 9 + 1)

    def test_present_ceiling_saved_query(self) -> None:
        saved = ROOT / 'tools' / 'queries' / 'present-ceiling.sql'
        self.assertTrue(saved.is_file(), 'tools/queries/present-ceiling.sql missing')
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'logq.py'),
             '--saved', 'present-ceiling', '--json', str(self.run_dir)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data['columns'], ['max_presents', 'first_t_at_max',
                                           'last_presents_t', 'last_reject_diag',
                                           'admitted_unknown'])
        self.assertEqual(data['rows'], [['1000', '30', '50', 'unsupported_method', '2']])


if __name__ == '__main__':
    unittest.main(verbosity=2)
