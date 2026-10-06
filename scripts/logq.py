"""`scripts/logq.py`: load an archived run's log into DuckDB and query it.

**Why this exists.** Plan T8. Answering a question about a run currently means
grepping a 34 KB to multi-MB log with a regular expression, and the answers are
counts, distributions and joins that a text tool answers badly: "how many kernel
calls per ordinal", "which recovered functions returned without an ABI
verification", "did any `[ICALL]` fire before the first `[CHECKPOINT]`". Each of
those has been re-derived by hand more than once, and a hand-derived count is a
transcription (plan W5) waiting to be wrong.

DuckDB is already installed as a Python package (1.5.6). It reads the log
directly, so this is a thin loader plus a query surface, not a new dependency.

**Line shapes are parsed per tag, and unparsed lines are kept.** A loader that
silently drops lines it does not recognise would make every count a lower bound
that reads as a total. Every line lands in `lines` with its tag and text; the
tag-specific tables are views over the ones that matched, and `logq.py --coverage`
reports how many lines each parser accounted for. A parser that stops matching
after a format change shows up as a coverage drop rather than as a smaller number.

**The tables are built for the questions the project actually asks:**

  lines           every line: run, lineno, tag, text
  kernel_calls    `[KERNEL] #N: ordinal O (slot S) esp=E ret=R tid=T`
  kmem_summary    `[KMEM] summary k=v ...` -- one row, one column per counter
  kmem_rejects    `[KMEM] reject kind=... base=... size=... type=... status=...`
  icall           `[ICALL] ...` including invalid targets
  alias_icalls    `[ALIAS-ICALL] target=... owner=...`
  checkpoints     `[CHECKPOINT] ms=... tid=... name`
  recovered       `[RECOVERED] 0x... returned; ABI verified (...)` and failures
  traces          `[TRACE] -> name (va) from=... esp=... eax=... ...`
  gmeter          `[GMETER] ...` when `RECOMP_GUEST_METER=1`
  presents        `[FBPRESENT] t=Ns presents=N hash=H CHANGED|unchanged`
  pfifo           one row per `[PFIFO]` decision line (submit, reject,
                  still_rejecting, recovered, admit_unknown, budget_exhausted);
                  `user_write` register traces are not decisions and are not rows
  gpu_flips       `[GPU] flips N (...), flip stalls M`

**Values are the log's, not the tool's.** Every table stores what the line says;
`--query` runs the caller's SQL against them, and the saved queries under
`tools/queries/` are ordinary `.sql` files so a reader can see the exact question
without reading this file.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

try:
    import duckdb
except ImportError as error:  # pragma: no cover - environment problem
    print(f'duckdb is not importable: {error}\n'
          f'  install it for the interpreter you are using: '
          f'python -m pip install duckdb', file=sys.stderr)
    raise SystemExit(2)

ROOT = Path(__file__).resolve().parents[1]
QUERY_DIR = ROOT / 'tools' / 'queries'

# Each parser is (table, regex).  A line may match several; the tag-specific
# tables are independent, and `lines` always holds everything.
KERNEL_CALL = re.compile(
    r'\[KERNEL\]\s+#(\d+):\s+ordinal\s+(\d+)\s+\(slot\s+(\d+)\)\s+'
    r'esp=0x([0-9A-Fa-f]+)\s+ret=0x([0-9A-Fa-f]+)\s+tid=(\d+)')
KMEM_SUMMARY = re.compile(r'\[KMEM\]\s+summary\s+(.*)$')
KMEM_REJECT = re.compile(
    r'\[KMEM\]\s+reject\s+kind=(\S+)\s+base=0x([0-9A-Fa-f]+)\s+'
    r'size=0x([0-9A-Fa-f]+)\s+type=0x([0-9A-Fa-f]+)\s+status=0x([0-9A-Fa-f]+)')
ICALL_INVALID = re.compile(
    r'\[ICALL\]\s+invalid target\s+0x([0-9A-Fa-f]+)\s+tid=(\d+)\s+'
    r'esp=([0-9A-Fa-f]+)\s+return=([0-9A-Fa-f]+)')
ALIAS_ICALL = re.compile(
    r'\[ALIAS-ICALL\]\s+target=0x([0-9A-Fa-f]+)\s+owner=0x([0-9A-Fa-f]+)')
CHECKPOINT = re.compile(r'\[CHECKPOINT\]\s+ms=(\d+)\s+tid=(\d+)\s+(\S+)')
RECOVERED = re.compile(
    r'\[RECOVERED\]\s+0x([0-9A-Fa-f]+)\s+returned;\s+ABI verified\s+\(([^)]*)\)')
RECOVERED_FAIL = re.compile(
    r'\[RECOVERED\]\s+0x([0-9A-Fa-f]+)\s+.*?(?:ABI|violation|failed)',
    re.IGNORECASE)
TRACE = re.compile(
    r'\[TRACE\]\s+(->|<-)\s+(\S+)\s+\(0x([0-9A-Fa-f]+)\)\s+(.*)$')
TRACE_FIELD = re.compile(r'(\w+)=([0-9A-Fa-f]+)')
GMETER = re.compile(r'\[GMETER\]\s*(.*)$')
FBPRESENT = re.compile(
    r'\[FBPRESENT\]\s+t=(\d+)s\s+presents=(-?\d+)\s+hash=([0-9A-Fa-f]+)\s+(CHANGED|unchanged)')
PFIFO = re.compile(
    r'\[PFIFO\]\s+(submit|reject|still rejecting|recovered|admit-unknown|budget_exhausted)\b(.*)$')
PFIFO_FIELD = re.compile(r'(\w+)=(\w+)')
GPU_FLIPS = re.compile(r'\[GPU\]\s+flips\s+(\d+)\b.*?flip stalls\s+(\d+)')
TAG = re.compile(r'^\s*\[([A-Z0-9_-]+)\]')


def log_path(target: Path) -> Path:
    """Accept a run directory or a log file."""
    if target.is_dir():
        candidate = target / 'jsrf_run.log'
        if not candidate.is_file():
            raise SystemExit(f'no jsrf_run.log in {target}')
        return candidate
    if not target.is_file():
        raise SystemExit(f'no such run or log: {target}')
    return target


def read_lines(path: Path, run_name: str) -> list[tuple[int, str, str, str]]:
    """(run, lineno, tag, text) for every line; tag is '' when there is none."""
    rows: list[tuple[int, str, str, str]] = []
    text = path.read_text(encoding='utf-8', errors='replace')
    for index, line in enumerate(text.splitlines(), start=1):
        match = TAG.match(line)
        rows.append((run_name, index, match.group(1) if match else '', line.rstrip()))
    return rows


def _hex(fields: dict[str, str], key: str) -> int | None:
    value = fields.get(key)
    return int(value, 16) if value is not None else None


def pfifo_row(run_name: str, lineno: int, kind: str, rest: str) -> tuple:
    """(run, lineno, kind, diag, n, method, subch, param, at, get, put, class_id)."""
    fields = dict(PFIFO_FIELD.findall(rest))
    n = None
    if kind == 'submit':
        found = re.match(r'\s*#(\d+)', rest)
        n = int(found.group(1)) if found else None
    elif kind == 'still rejecting' and 'n' in fields:
        n = int(fields['n'])
    elif kind == 'recovered':
        found = re.match(r'\s*after\s+(\d+)\s+rejections', rest)
        n = int(found.group(1)) if found else None
    subch = fields.get('subch')
    return (run_name, lineno, kind.replace('-', '_').replace(' ', '_'),
            fields.get('diag'), n, _hex(fields, 'method'),
            int(subch) if subch is not None else None, _hex(fields, 'param'),
            _hex(fields, 'at'), _hex(fields, 'get'), _hex(fields, 'put'),
            _hex(fields, 'class'))


def parse_all(rows: list[tuple[int, str, str, str]], run_name: str) -> dict[str, list[tuple]]:
    """Every tag-specific table, built in one pass."""
    kernel_calls, kmem_rejects, icalls, alias_icalls = [], [], [], []
    checkpoints, recovered, recovered_failed, traces, gmeter = [], [], [], [], []
    kmem_summary: list[tuple] = []
    presents, pfifo, gpu_flips = [], [], []

    for _run, lineno, _tag, text in rows:
        match = KERNEL_CALL.search(text)
        if match:
            number, ordinal, slot, esp, ret, tid = match.groups()
            kernel_calls.append((run_name, lineno, int(number), int(ordinal),
                                 int(slot), esp, ret, int(tid)))
        match = KMEM_SUMMARY.search(text)
        if match:
            for key, value in re.findall(r'(\w+)=(-?\d+)', match.group(1)):
                kmem_summary.append((run_name, lineno, key, int(value)))
        match = KMEM_REJECT.search(text)
        if match:
            kind, base, size, kind_type, status = match.groups()
            kmem_rejects.append((run_name, lineno, kind, base, size,
                                 kind_type, status))
        match = ICALL_INVALID.search(text)
        if match:
            target, tid, esp, ret = match.groups()
            icalls.append((run_name, lineno, target, int(tid), esp, ret, 'invalid'))
        match = ALIAS_ICALL.search(text)
        if match:
            target, owner = match.groups()
            alias_icalls.append((run_name, lineno, target, owner))
        match = CHECKPOINT.search(text)
        if match:
            ms, tid, name = match.groups()
            checkpoints.append((run_name, lineno, int(ms), int(tid), name))
        match = RECOVERED.search(text)
        if match:
            va, registers = match.groups()
            recovered.append((run_name, lineno, va,
                              [r.strip() for r in registers.split('/') if r.strip()]))
        elif RECOVERED_FAIL.search(text):
            recovered_failed.append((run_name, lineno, text))
        match = TRACE.search(text)
        if match:
            direction, name, va, rest = match.groups()
            fields = {k: v for k, v in TRACE_FIELD.findall(rest)}
            traces.append((run_name, lineno, direction, name, va,
                           fields.get('from', ''), fields.get('esp', ''),
                           fields.get('eax', '')))
        match = GMETER.search(text)
        if match:
            gmeter.append((run_name, lineno, match.group(1)))
        match = FBPRESENT.search(text)
        if match:
            t, count, digest, state = match.groups()
            presents.append((run_name, lineno, int(t), int(count), digest,
                             state == 'CHANGED'))
        match = PFIFO.search(text)
        if match:
            pfifo.append(pfifo_row(run_name, lineno, match.group(1), match.group(2)))
        match = GPU_FLIPS.search(text)
        if match:
            gpu_flips.append((run_name, lineno, int(match.group(1)),
                              int(match.group(2))))
    return {
        'kernel_calls': kernel_calls, 'kmem_summary': kmem_summary,
        'kmem_rejects': kmem_rejects, 'icalls': icalls,
        'alias_icalls': alias_icalls, 'checkpoints': checkpoints,
        'recovered': recovered, 'recovered_failed': recovered_failed,
        'traces': traces, 'gmeter': gmeter,
        'presents': presents, 'pfifo': pfifo, 'gpu_flips': gpu_flips,
    }


SCHEMA = (
    ('kernel_calls', '(run VARCHAR, lineno BIGINT, call_no BIGINT, ordinal BIGINT, '
                     'slot BIGINT, esp VARCHAR, ret VARCHAR, tid BIGINT)'),
    ('kmem_summary', '(run VARCHAR, lineno BIGINT, counter VARCHAR, value BIGINT)'),
    ('kmem_rejects', '(run VARCHAR, lineno BIGINT, kind VARCHAR, base VARCHAR, '
                     'size VARCHAR, alloc_type VARCHAR, status VARCHAR)'),
    ('icalls', '(run VARCHAR, lineno BIGINT, target VARCHAR, tid BIGINT, '
               'esp VARCHAR, ret VARCHAR, kind VARCHAR)'),
    ('alias_icalls', '(run VARCHAR, lineno BIGINT, target VARCHAR, owner VARCHAR)'),
    ('checkpoints', '(run VARCHAR, lineno BIGINT, ms BIGINT, tid BIGINT, name VARCHAR)'),
    ('recovered', '(run VARCHAR, lineno BIGINT, va VARCHAR, registers VARCHAR[])'),
    ('recovered_failed', '(run VARCHAR, lineno BIGINT, text VARCHAR)'),
    ('traces', '(run VARCHAR, lineno BIGINT, direction VARCHAR, name VARCHAR, '
               'va VARCHAR, caller VARCHAR, esp VARCHAR, eax VARCHAR)'),
    ('gmeter', '(run VARCHAR, lineno BIGINT, text VARCHAR)'),
    ('presents', '(run VARCHAR, lineno BIGINT, t BIGINT, presents BIGINT, '
                 'hash VARCHAR, changed BOOLEAN)'),
    ('pfifo', '(run VARCHAR, lineno BIGINT, kind VARCHAR, diag VARCHAR, n BIGINT, '
              'method BIGINT, subch BIGINT, param BIGINT, "at" BIGINT, get BIGINT, '
              'put BIGINT, class_id BIGINT)'),
    ('gpu_flips', '(run VARCHAR, lineno BIGINT, flips BIGINT, stalls BIGINT)'),
    ('lines', '(run VARCHAR, lineno BIGINT, tag VARCHAR, text VARCHAR)'),
)


def build(connection, run_name: str, path: Path) -> dict[str, int]:
    rows = read_lines(path, run_name)
    tables = parse_all(rows, run_name)
    counts: dict[str, int] = {'lines': len(rows)}
    connection.execute('CREATE TABLE lines (run VARCHAR, lineno BIGINT, '
                       'tag VARCHAR, text VARCHAR)')
    if rows:
        connection.executemany('INSERT INTO lines VALUES (?, ?, ?, ?)', rows)
    for table, schema in SCHEMA:
        if table == 'lines':
            continue
        connection.execute(f'CREATE TABLE {table} {schema}')
        data = tables[table]
        if data:
            connection.executemany(
                f'INSERT INTO {table} VALUES ({", ".join("?" * len(data[0]))})',
                data)
        counts[table] = len(data)
    counts['_total_lines'] = len(rows)
    return counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', nargs='+', help='run directory or jsrf_run.log')
    parser.add_argument('--query', help='SQL to run')
    parser.add_argument('--file', help='a .sql file to run')
    parser.add_argument('--saved', help='run a saved query by name from tools/queries/')
    parser.add_argument('--list-saved', action='store_true',
                        help='list the saved queries')
    parser.add_argument('--coverage', action='store_true',
                        help='report how many lines each parser accounted for')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    if args.list_saved:
        for path in sorted(QUERY_DIR.glob('*.sql')):
            first = path.read_text(encoding='utf-8').splitlines()
            comment = next((l.lstrip('- ').strip() for l in first
                            if l.strip().startswith('--')), '')
            print(f'  {path.stem:<28} {comment}')
        return 0

    sql = args.query
    if args.file:
        sql = Path(args.file).read_text(encoding='utf-8')
    if args.saved:
        candidate = QUERY_DIR / f'{args.saved}.sql'
        if not candidate.is_file():
            candidate = QUERY_DIR / args.saved
        if not candidate.is_file():
            print(f'no saved query {args.saved!r} in {QUERY_DIR}', file=sys.stderr)
            return 2
        sql = candidate.read_text(encoding='utf-8')

    connection = duckdb.connect(':memory:')
    counts: dict[str, dict[str, int]] = {}
    for target in args.runs:
        path = log_path(Path(target))
        run_name = path.parent.name if path.name == 'jsrf_run.log' else path.stem
        counts[run_name] = build(connection, run_name, path)

    if args.coverage or not sql:
        if args.json:
            print(json.dumps(counts, indent=2, sort_keys=True))
            return 0
        for run_name, table_counts in counts.items():
            total = table_counts.get('_total_lines', 0)
            print(f'  {run_name}: {total} lines')
            for table, count in sorted(table_counts.items()):
                if table.startswith('_'):
                    continue
                print(f'    {table:<18} {count}')
        if not sql:
            print()
            print('  pass --query, --file or --saved to run SQL; --list-saved lists')
            print('  the saved queries. Tables are created from the log itself, so')
            print('  a parser that stops matching shows up as a lower count here.')
        return 0

    try:
        result = connection.execute(sql)
        columns = [d[0] for d in result.description]
        rows = result.fetchall()
    except duckdb.Error as error:
        print(f'query failed: {error}', file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps({'columns': columns,
                          'rows': [[str(v) for v in row] for row in rows]},
                         indent=2))
        return 0
    print('  ' + ' | '.join(columns))
    for row in rows:
        print('  ' + ' | '.join('' if v is None else str(v) for v in row))
    print(f'  ({len(rows)} row(s))')
    return 0


if __name__ == '__main__':
    sys.exit(main())
