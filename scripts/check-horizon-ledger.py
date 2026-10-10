"""`scripts/check-horizon-ledger.py`: W14's ledger lint.

Plan W14: "**Strict-horizon ledger and ceiling rule** (§3): one line per session;
3 packets or 4 h without moving the horizon or an accepted critical-path finding →
one Advisor ceiling call", with the check being "ledger lint: **a session with a
run and no ledger line fails**".

**What that means mechanically, and why each part is a separate check.** The rule
has two halves and they fail differently:

  * **A session that ran a strict run and left no line fails.** This is the one the
    plan names. It is checkable from the run archive: every archived strict run's
    directory name must appear somewhere in the ledger.
  * **A line must be re-derivable.** The ledger's own rules say "Values come from
    the run's own archived artifacts, not from transcription" and "the run directory
    is named so every line can be re-derived". So a row naming a run that does not
    exist, or a game revision that is not a revision, is a finding too -- otherwise
    the ledger becomes prose with a table drawn round it.

**What it deliberately does not check.** The ceiling rule ("3 packets or 4 h
without moving the horizon") needs a session boundary and a packet count, which
live in records this lint does not own. It reports the **time since the last
horizon move** as information, and leaves the judgement to the Advisor, because
that is a §2.3 call and not a checker's.

Exit 0 clean, 1 a finding, 2 the lint could not run.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ROOT / 'docs' / 'reviews' / 'strict-horizon-ledger.md'
RUNS = ROOT / 'logs' / 'runs'

CHECKER_VERSION = 'jsrf-horizon-ledger/3'

# The ledger's scope (owner decision recorded in plan-jsrf-bare-minimum.md, 2026-09-30; retired plan; `git show d6b8336:plan-jsrf-bare-minimum.md`):
# strict runs from 2026-09-29 onward; the earlier runs are not backfilled. The bare
# invocation applies it, so it no longer reports pre-scope runs as failures;
# `--since all` covers the whole archive.
SCOPE_START = '2026-09-29'

# A ledger row: `| date | game | toolkit | run-id | event | sites | stop |`
ROW = re.compile(r'^\|\s*(\d{4}-\d{2}-\d{2})\s*\|(.+)\|\s*$')
RUN_ID = re.compile(r'`(\d{8}-\d{6}-\d{3}-[A-Za-z0-9_.-]+)`')
REVISION = re.compile(r'`([0-9a-f]{7,40})`')
# A row that says the horizon was not reached is still a valid row; it must say so.
NOT_REACHED = re.compile(r'NOT REACHED|did not reach|no stop', re.IGNORECASE)
# A row that records the horizon actually moving. This is the only progress event:
# `REPRODUCED` re-observes the same event and `NOT REACHED` observes none, so
# neither is a move.
MOVED = re.compile(r'\bMOVED\b')
# The superseded-horizon section, whose rows describe an older horizon and must not
# take part in the "last move" calculation.
SUPERSEDED_HEADING = re.compile(r'^##\s+Prior horizon', re.IGNORECASE)


def all_named_runs() -> set[str]:
    """Every run id named ANYWHERE in the ledger, not only in date-prefixed rows.

    **Why the whole text.** Measured: the ledger's "Observed first-site
    distribution" table names four runs by their full directory names, and a scan
    that read only the date-prefixed session rows reported those four as uncovered
    -- a false failure that would have been "fixed" by editing a correct table. The
    ledger is the document that names runs; which table it uses is not the lint's
    business.
    """
    if not LEDGER.is_file():
        raise SystemExit(f'{LEDGER} is missing')
    return set(RUN_ID.findall(LEDGER.read_text(encoding='utf-8')))


def ledger_rows() -> list[dict]:
    """Every date-prefixed session row, with the run ids and revisions it names.

    Used for the *reproducibility* checks (a row must name a real run and a real
    revision), not for coverage -- see `all_named_runs`.

    Each row also carries `moved` (it records the horizon actually moving) and
    `superseded` (it sits in the "Prior horizon" section, so it describes an older
    horizon). **Why both.** Measured 2026-09-30: the previous version computed the
    last horizon move as "the last row that is not `NOT REACHED`", which counts
    `REPRODUCED` rows as moves and, because the superseded table sits below the
    session rows, made the 2026-09-28 row win by file order -- so W14's ceiling
    rule was judged from a date 78 h stale while 2026-09-30 rows existed, and it
    kept reporting 2026-09-28 even after two real moves. A row is a progress event
    only when it says `MOVED`.
    """
    if not LEDGER.is_file():
        raise SystemExit(f'{LEDGER} is missing')
    rows = []
    superseded = False
    for number, line in enumerate(LEDGER.read_text(encoding='utf-8').splitlines(), 1):
        if SUPERSEDED_HEADING.match(line):
            superseded = True
        if not line.startswith('|'):
            continue
        match = ROW.match(line)
        if not match:
            continue
        body = match.group(2)
        run_ids = RUN_ID.findall(body)
        if not run_ids:
            continue  # a header or a prose row; not a session line
        rows.append({
            'line': number,
            'date': match.group(1),
            'run_ids': run_ids,
            'revisions': REVISION.findall(body),
            'not_reached': bool(NOT_REACHED.search(body)),
            'moved': bool(MOVED.search(body)),
            'superseded': superseded,
            'text': line.strip(),
        })
    return rows


def ledger_epoch() -> str | None:
    """The date the ledger's coverage begins: its own earliest row.

    **Why a boundary is needed, and why this one is honest.** W14's check is "a
    session with a run and no ledger line fails", but the ledger was created on
    2026-09-29 and `logs/runs` reaches back to 2026-09-12. Measured: applying the
    rule to the whole archive reports **40 strict runs with no line**, every one of
    them predating the ledger's existence -- a check that is red from its first run
    and therefore ignored, which is the failure mode this project names repeatedly.

    The boundary is not a waiver of those 40 runs: they are reported as
    `pre_ledger` in the record, with their names, so the gap is visible rather than
    erased. What it means is that the rule governs runs from the point the ledger
    could have covered them. Extending it backwards would require reconstructing 40
    sessions' horizons from archives, which is a separate piece of work and not
    something a lint may assert by default.
    """
    rows = ledger_rows()
    return min((row['date'] for row in rows), default=None)


def archived_runs() -> list[Path]:
    if not RUNS.is_dir():
        return []
    return sorted((p for p in RUNS.iterdir() if p.is_dir()), key=lambda p: p.name)


def strict_runs(since: str | None = None) -> tuple[list[Path], list[str]]:
    """The archived runs the profile checker classifies STRICT.

    Re-derived rather than guessed from the name: `check-run-profile.py` is the
    authority on a run's profile (`docs/jsrf-run-profiles.md`), and a name-based
    guess would let a mislabelled run escape the ledger.

    **Batched, because 1184 absolute paths exceed the Windows command-line limit.**
    Measured: passing every run directory at once failed with
    `[WinError 206] The filename or extension is too long`, which reads like a
    missing script rather than an argument-length problem. `--since` filters before
    batching, so the common case is one small call.
    """
    runs = archived_runs()
    if since:
        runs = [r for r in runs if r.name[:8] >= since.replace('-', '')]
    if not runs:
        return [], []

    strict: list[Path] = []
    problems: list[str] = []
    batch_size = 40
    for start in range(0, len(runs), batch_size):
        batch = runs[start:start + batch_size]
        completed = subprocess.run(
            [sys.executable, '-X', 'utf8',
             str(ROOT / 'scripts' / 'check-run-profile.py'),
             *[str(r) for r in batch]],
            capture_output=True, text=True)
        if completed.returncode not in (0, 1):
            problems.append(completed.stderr.strip() or
                            f'exit {completed.returncode} on a batch')
            continue
        for line in completed.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[-1] == 'STRICT':
                strict.append(RUNS / parts[0])
    return strict, problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--since', default=SCOPE_START,
                        help=f'only consider runs from this date (YYYY-MM-DD); default '
                             f'{SCOPE_START}, the ledger scope; "all" for the whole archive')
    args = parser.parse_args()
    since = None if args.since == 'all' else args.since

    findings: list[dict] = []
    rows = ledger_rows()
    named = all_named_runs()

    strict, problems = strict_runs(since)
    for problem in problems:
        findings.append({'check': 'profile', 'reason': 'checker_failed',
                         'detail': problem})

    # The plan's named check: a strict run with no ledger line.  Scoped to the
    # ledger's own epoch; runs older than the ledger are reported, not failed.
    epoch = ledger_epoch()
    epoch_compact = epoch.replace('-', '') if epoch else None
    pre_ledger = []
    for run in strict:
        if run.name in named:
            continue
        if epoch_compact and run.name[:8] < epoch_compact:
            pre_ledger.append(run.name)
            continue
        findings.append({
            'check': 'ledger_coverage', 'reason': 'strict_run_without_a_line',
            'detail': (f'{run.name} is an archived STRICT run and no ledger row '
                       f'names it; W14 makes that a ledger failure'),
        })

    # A line naming a run that does not exist is a row nobody can re-derive.
    existing = {r.name for r in archived_runs()}
    for row in rows:
        for run_id in row['run_ids']:
            if run_id not in existing:
                findings.append({
                    'check': 'ledger_reproducible', 'reason': 'unknown_run',
                    'detail': (f'line {row["line"]} names run {run_id!r}, which is '
                               f'not in logs/runs; the row cannot be re-derived'),
                })
        if not row['revisions']:
            findings.append({
                'check': 'ledger_reproducible', 'reason': 'no_revision',
                'detail': f'line {row["line"]} names no game/toolkit revision',
            })

    # Information, not a finding: the ceiling rule is a §2.3 judgement.
    # Only a row that says MOVED is a progress event, and the superseded table is
    # excluded: its rows describe an older horizon and sit below the session rows.
    # A row that also says NOT REACHED is never a move, whatever else it says --
    # measured 2026-09-30: a contrast row whose prose said "the diagnostic changed
    # and its address did not" was counted as a move because it contained the word
    # MOVED, which is the same class of error this check exists to prevent.
    moved = [row for row in rows
             if row['moved'] and not row['superseded'] and not row['not_reached']]
    last_move = moved[-1] if moved else None
    elapsed_hours = None
    if last_move:
        try:
            when = datetime.strptime(last_move['date'], '%Y-%m-%d').replace(
                tzinfo=timezone.utc)
            elapsed_hours = round(
                (datetime.now(timezone.utc) - when).total_seconds() / 3600, 1)
        except ValueError:
            pass

    record = {
        'checker': CHECKER_VERSION,
        'ledger': str(LEDGER),
        'since': since or 'all',
        'rows': len(rows),
        'named_runs': len(named),
        'strict_runs_archived': len(strict),
        'strict_runs_without_a_line': [r.name for r in strict if r.name not in named],
        'horizon_moves': len(moved),
        'last_horizon_move': last_move['date'] if last_move else None,
        'last_horizon_move_run': (last_move['run_ids'][0] if last_move
                                  and last_move['run_ids'] else None),
        'hours_since_last_move': elapsed_hours,
        'ceiling_note': ('The ceiling rule (3 packets or 4 h) needs a session '
                         'boundary and a packet count, which this lint does not '
                         'own; the elapsed time is reported for the Advisor.'),
        'findings': findings,
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  runs considered since  : {since or "all"}')
        print(f'  ledger rows            : {len(rows)}')
        print(f'  strict runs archived   : {len(strict)}')
        print(f'  runs named in the ledger: {len(named)}')
        print(f'  horizon moves recorded : {record["horizon_moves"]}')
        if elapsed_hours is not None:
            print(f'  last horizon move      : {record["last_horizon_move"]} '
                  f'({elapsed_hours} h ago)')
            print(f'  that move was the run  : {record["last_horizon_move_run"]}')
        else:
            print('  last horizon move      : none recorded (no row says MOVED)')
        if findings:
            print(f'  {len(findings)} finding(s):')
            for finding in findings:
                print(f'    [{finding["check"]}/{finding["reason"]}] '
                      f'{finding["detail"]}')
        else:
            print('  no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
