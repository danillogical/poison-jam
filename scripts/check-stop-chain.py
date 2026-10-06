"""The stop-chain record: one row per runtime stop, each bound to real evidence.

**Why this exists.** The project's progress claims are a chain of runtime stops,
and the distinctions that matter are fine ones: *discovered* / *repaired* /
*statically validated* / *exercised* / *returned* / *ABI verified* / *not
exercised* / *runtime confirmed*. Every one of them has been confused at least
once, and each confusion was caught by a human reading a log rather than by a
machine:

* `0x00032610` was recorded as run-confirmed by run **f25**, which never
  dispatched the address at all -- f25's log has zero `[RECOVERED] 0x00032610
  returned; ABI verified` lines, and all 14 textual `32610` matches were the
  run's own directory name in `[SAVE]`/`[FBWIN]` path strings. The error survived
  three commits (Turn Reviewer finding B3).
* `0xB5EB0` was repaired from the bytes and two consecutive runs then failed to
  execute it. Recording that as a pass would have been a fabricated progression.
* g06 ran for 905 s, returned 457 ABI-verified results, and exercised **none** of
  the addresses it might have been credited with.

`scripts/check-run-exercised.py` already answers "did this run exercise this
address" correctly, from the ABI-verified return line rather than from text. This
gate is the other half: it holds the *record* to that answer, so a row cannot
claim more than its cited archives establish.

## The record

`config/stop-chain.json`. One row per stop:

```json
{
  "id": 21,
  "address": "0x000B5EB0",
  "state": "NOT_EXERCISED",
  "repair_commit": "64945a3",
  "evidence": [
    {"run": "20261005-211627-927-g07-thunk", "role": "discovered"},
    {"run": "20261005-223404-818-g08-b5eb0-fixed", "role": "not_exercised"}
  ],
  "note": "repaired from the bytes; two runs have not executed it"
}
```

## The invariant, per row

For every `{"run": R, "role": ...}` in a row for address A:

1. **R exists as an archive** under `logs/runs/`, with a `metadata.json`, a
   `jsrf_run.log`, and a `result.json`.
2. **R's log hash matches its own metadata** (`run_log_sha256`). A row cites a
   run, and the log it cites must be the log that was archived -- otherwise the
   citation can be satisfied by editing the log afterwards.
3. **The role is true of R**, checked against R's actual log:
   * `discovered` / `found`: R's log names A in an unresolved-call or ABI-failure
     line, or as a resolve failure. R need not descend from any repair: a run
     that *found* the defect necessarily predates the fix.
   * `exercised` / `confirmed` / `returned`: R's log has A's ABI-verified return
     line, AND R's recorded project revision descends from `repair_commit`. Both
     halves are required: the return proves the path ran, and the ancestry proves
     it ran *with the repair*.
   * `not_exercised`: R's log has **no** ABI-verified return for A. This is a
     negative role, so it is checked as one -- and it is not a pass.
4. **`state` agrees with the evidence.** A row may not say `RUNTIME_CONFIRMED`
   unless some evidence row has a confirming role; a row whose only evidence is
   `not_exercised` must be `NOT_EXERCISED`, never `RUNTIME_CONFIRMED`.

Rule 4 is the one that makes a false continuation claim fail, and it is
zero-baseline: it compares the row against its own cited evidence rather than
against a frozen list of known-good rows.

## What this gate does NOT do

It does not prove a run reached the *title screen*, and it is not a fidelity
claim. It is a record-consistency gate. The structural "certified lost
continuation" gate that proves a jump table's guard from the bytes is a different
tool with a different proof, and it is not this one.

Usage:
    python -X utf8 scripts/check-stop-chain.py [--record PATH] [--runs DIR]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / 'config' / 'stop-chain.json'
RUNS = ROOT / 'logs' / 'runs'
CURRENT_MANIFEST = ROOT / 'config' / 'recovered-functions.json'
CURRENT_MANIFEST_ENTRIES: dict[str, tuple] | None = None

# `[RECOVERED] 0x0013B750 returned; ABI verified (ESP/EBX/ESI/EDI)`
RETURNED = re.compile(r'\[RECOVERED\]\s+0x([0-9A-Fa-f]{8})\s+returned;\s*ABI verified')
# `[RECOVERED] ABI FAILURE 0x00026780 esp ... expected +4`
ABI_FAILURE = re.compile(r'\[RECOVERED\]\s+ABI FAILURE\s+0x([0-9A-Fa-f]{8})')
# Lines that name a defect the run hit.
DEFECT = (
    re.compile(r'\[ICALL\]\s+Failed to resolve VA\s+0x([0-9A-Fa-f]{8})'),
    ABI_FAILURE,
    re.compile(r'\[ALIAS-ICALL\]\s+target=0x([0-9A-Fa-f]{8})'),
)
# The toolkit's ICALL history: `  [15] 0x000B5EB0` immediately after a failed
# resolve. This is the *weaker* discovery signal and it is a real one -- it is
# how `0xB5EB0` was actually found: the guest trapped on the out-of-span table
# arm `0xB5F82`, and the ICALL history named `0xB5EB0` as frame 15, i.e. as the
# body whose own switch table had been cut. The address itself appears in no
# unresolved-call line, because the trap names the ARM, not the entry.
#
# Per `AGENTS.md` the ICALL history is partial and is not a full guest call
# stack, so this is accepted only for the *discovery* role, which is a negative
# claim about progress and never a confirmation.
ICALL_FRAME = re.compile(r'(?m)^\s*\[\s*\d+\]\s+0x([0-9A-Fa-f]{8})\s*$')

CONFIRMING_ROLES = {'exercised', 'confirmed', 'returned', 'runtime_confirmed'}
NEGATIVE_ROLES = {'not_exercised', 'not_reached'}
DISCOVERY_ROLES = {'discovered', 'found'}
KNOWN_ROLES = CONFIRMING_ROLES | NEGATIVE_ROLES | DISCOVERY_ROLES

CONFIRMED_STATES = {'RUNTIME_CONFIRMED'}
UNCONFIRMED_STATES = {'NOT_EXERCISED', 'REPAIRED', 'DISCOVERED', 'STATIC_ONLY'}


def normalise(va: str) -> str:
    return '0x%08X' % int(va, 16)


def log_path(run: str) -> Path:
    return RUNS / run / 'jsrf_run.log'


def read_log(run: str) -> str | None:
    path = log_path(run)
    if not path.is_file():
        return None
    return path.read_text(encoding='utf-8', errors='replace')


def exercised_addresses(log: str) -> set[str]:
    """Addresses with an ABI-verified return, EXCLUDING any that also failed.

    **Measured defect (Advisor finding, verified).** The return line is written
    once, on the first successful return, and it is **not exclusive with a later
    failure for the same address**. In the strict run
    `20260930-225440-580-f3-alias-fix-strict`, `0x00026780` logs

        line 75094  [RECOVERED] 0x00026780 returned; ABI verified (ESP/EBX/ESI/EDI)
        line 77608  [RECOVERED] ABI FAILURE 0x00026780 esp 00F7FEE0->00F7FCBC expected +4

    so the address demonstrably has a broken stack contract in that run, and
    `check-run-exercised.py` still reported PASS. A return line alone is therefore
    **not** sufficient for a confirming role: an address that also failed must
    never be counted as exercised.

    `JSRF_ABI_CONTINUE` is the other half. It turns the ABI check into a report, so
    a run under it can log many failures and keep going -- its returns are not
    evidence that the contracts held. That is read from the run's own recorded
    settings by `run_under_abi_continue`, not guessed.
    """
    returned = {normalise('0x' + m) for m in RETURNED.findall(log)}
    failed = {normalise('0x' + m) for m in ABI_FAILURE.findall(log)}
    return returned - failed


def run_under_abi_continue(metadata: dict | None) -> bool:
    """Did this run set `JSRF_ABI_CONTINUE`?

    Presence is what matters, including empty or `0` (the classifier's rule), so
    this is a membership test and never a truthiness test.
    """
    if not isinstance(metadata, dict):
        return False
    settings = metadata.get('settings')
    entries = settings if isinstance(settings, list) else []
    for entry in entries:
        if isinstance(entry, dict) and str(entry.get('name', '')).upper() == 'JSRF_ABI_CONTINUE':
            return True
    return False


def archived_manifest(archive: Path) -> dict | None:
    """The reviewed manifest as it was when the run was archived.

    The runner copies the source tree into `source.zip`, so the archive carries
    the exact `config/recovered-functions.json` the run was built from. That is
    the only way to answer "which body did this run actually execute", and it is
    why descent from the repair commit is **necessary but not sufficient**: a
    later commit can change the span again, and then the run exercised a
    different body than the record now describes.

    Returns `{start_va: (end, stack_args)}`, or None when unavailable.
    """
    import zipfile
    path = archive / 'source.zip'
    if not path.is_file():
        return None
    try:
        with zipfile.ZipFile(path) as zf:
            names = [n for n in zf.namelist() if n.endswith('config/recovered-functions.json')]
            if len(names) != 1:
                return None
            entries = json.loads(zf.read(names[0]))
    except (OSError, ValueError, KeyError):
        return None
    if not isinstance(entries, list):
        return None
    out: dict[str, tuple] = {}
    for entry in entries:
        if isinstance(entry, dict) and isinstance(entry.get('start'), str):
            out[normalise(entry['start'])] = (entry.get('end'), entry.get('stack_args'))
    return out


def manifest_agreement(label: str, run: str, address: str, row: dict) -> list[str]:
    """Does the run's archived manifest describe the same body as today's?

    Compares the `(end, stack_args)` tuple the run was built with against the
    current manifest. A run whose tuple differs exercised a *different* body, so
    its return does not confirm the body the record now describes.
    """
    archive = RUNS / run
    recorded = archived_manifest(archive)
    if recorded is None:
        return [f'{label}: cited run {run} carries no readable archived manifest '
                f'(source.zip), so which body it executed cannot be established; '
                f'refusing to assume it matches']
    if address not in recorded:
        return [f'{label}: cited run {run} was built from a manifest with no entry for '
                f'{address}, so it cannot have exercised that body as repaired']
    if not CURRENT_MANIFEST.is_file():
        return [f'{label}: the current manifest is unavailable, so agreement for '
                f'{run} cannot be checked']
    current = CURRENT_MANIFEST_ENTRIES
    if current is None:
        return [f'{label}: the current manifest is unreadable, so agreement for '
                f'{run} cannot be checked']
    if address not in current:
        return [f'{label}: {address} is not in the current manifest']
    if recorded[address] != current[address]:
        return [f'{label}: cited run {run} executed {address} as '
                f'(end, stack_args)={recorded[address]}, but the current manifest says '
                f'{current[address]}. Descent from the repair commit is necessary but '
                f'not sufficient: a later commit changed the span again, so this run '
                f'exercised a different body.']
    return []


def defect_addresses(log: str) -> set[str]:
    out: set[str] = set()
    for pattern in DEFECT:
        out |= {normalise('0x' + m) for m in pattern.findall(log)}
    return out


def icall_history_addresses(log: str) -> set[str]:
    """Addresses named as ICALL-history frames after a failed resolve.

    A *partial* record, not a call stack (see `AGENTS.md`), so this is only ever
    used for the discovery role.
    """
    out: set[str] = set()
    for block in re.split(r'(?m)^\[ICALL\]\s+Failed to resolve', log)[1:]:
        # The history follows immediately; stop at the next bracketed section.
        body = re.split(r'(?m)^\[[A-Z]', block, maxsplit=1)[0]
        out |= {normalise('0x' + m) for m in ICALL_FRAME.findall(body)}
    return out


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True)


def is_ancestor(ancestor: str, revision: str) -> bool | None:
    """True/False from git ancestry; None when the answer is not available.

    `None` is distinct from `False`: an unknown revision cannot be reported as
    "does not descend", because that would silently downgrade a claim.

    A revision that is not a **commit object in this repository** is `None`, not
    `False`.  That matters for the controls: they build scratch archives whose
    `metadata.json` cites a synthetic revision, and those runs are not in this
    repository's history at all.  Reporting that as "does not descend" would make
    every control fail for a reason unrelated to what it tests, and -- worse --
    would mean a real archive citing an unreadable revision was silently treated
    as a *refuted* claim rather than an unprovable one.
    """
    if not ancestor or not revision:
        return None
    known = git('cat-file', '-e', f'{revision}^{{commit}}')
    if known.returncode != 0:
        return None
    result = git('merge-base', '--is-ancestor', ancestor, revision)
    if result.returncode == 0:
        return True
    if result.returncode == 1:
        return False
    return None


def run_metadata(run: str) -> dict | None:
    path = RUNS / run / 'metadata.json'
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except ValueError:
        return None


def run_revision(metadata: dict | None) -> str | None:
    if not isinstance(metadata, dict):
        return None
    identities = metadata.get('repository_identities')
    if isinstance(identities, dict):
        project = identities.get('project')
        if isinstance(project, dict):
            revision = project.get('revision')
            if isinstance(revision, str) and revision.strip():
                return revision.strip()
    return None


def check_row(row: dict, cache: dict) -> list[str]:
    """Every consistency failure in one stop-chain row."""
    problems: list[str] = []
    ident = row.get('id')
    label = f'stop {ident}' if ident is not None else '<no id>'
    raw_address = row.get('address')
    if not isinstance(raw_address, str):
        return [f'{label}: no address']
    try:
        address = normalise(raw_address)
    except ValueError:
        return [f'{label}: address {raw_address!r} is not a hex VA']
    label = f'{label} {address}'

    state = row.get('state')
    if state not in CONFIRMED_STATES | UNCONFIRMED_STATES:
        problems.append(f'{label}: unknown state {state!r}')

    repair_commit = row.get('repair_commit')
    evidence = row.get('evidence')
    if not isinstance(evidence, list):
        problems.append(f'{label}: no evidence list')
        return problems
    if not evidence:
        # An empty evidence list is legal and is checked as a negative: the row
        # may describe a static repair but may not claim any runtime state.
        if state not in {'STATIC_ONLY', 'DISCOVERED'}:
            problems.append(
                f'{label}: state is {state} with NO evidence rows; a row with no '
                f'cited run can only be STATIC_ONLY or DISCOVERED')
        if not isinstance(repair_commit, str) or not repair_commit:
            problems.append(
                f'{label}: has no evidence rows and no repair_commit, so it records '
                f'nothing checkable')
        return problems

    roles_seen: set[str] = set()
    for item in evidence:
        if not isinstance(item, dict):
            problems.append(f'{label}: evidence item is not an object')
            continue
        run = item.get('run')
        role = item.get('role')
        if not isinstance(run, str) or not run:
            problems.append(f'{label}: evidence item has no run name')
            continue
        if role not in KNOWN_ROLES:
            problems.append(f'{label}: evidence for {run} has unknown role {role!r}')
            continue
        roles_seen.add(role)

        archive = RUNS / run
        if not archive.is_dir():
            problems.append(f'{label}: cited run {run} is not an archived run')
            continue
        for name in ('metadata.json', 'jsrf_run.log', 'result.json'):
            if not (archive / name).is_file():
                problems.append(f'{label}: cited run {run} is missing {name}')

        if run not in cache:
            cache[run] = (read_log(run), run_metadata(run))
        log, metadata = cache[run]

        # The cited log must be the log that was archived.
        if isinstance(metadata, dict):
            recorded = metadata.get('run_log_sha256')
            if isinstance(recorded, str) and recorded and log is not None:
                actual = hashlib.sha256(
                    (archive / 'jsrf_run.log').read_bytes()).hexdigest()
                if actual != recorded.lower():
                    problems.append(
                        f'{label}: cited run {run} has a log whose hash does not match '
                        f'its own metadata, so the citation is not bound to the archived bytes')

        if log is None:
            problems.append(f'{label}: cited run {run} has no readable jsrf_run.log')
            continue

        if role in CONFIRMING_ROLES:
            if address not in exercised_addresses(log):
                problems.append(
                    f'{label}: cited as {role!r} by {run}, but that log has no clean '
                    f'ABI-verified return for {address}. A run that does not reach an '
                    f'address establishes NOT EXERCISED, not a pass; and an address that '
                    f'returned AND later logged an ABI FAILURE is not clean either.')
                continue
            if run_under_abi_continue(metadata):
                problems.append(
                    f'{label}: cited as {role!r} by {run}, but that run set '
                    f'JSRF_ABI_CONTINUE, which turns the ABI check into a report. Its '
                    f'returns do not establish that the contracts held.')
                continue
            if not isinstance(repair_commit, str) or not repair_commit:
                problems.append(
                    f'{label}: cited as {role!r} but the row records no repair_commit, '
                    f'so the run cannot be shown to include the repair')
                continue
            revision = run_revision(metadata)
            if revision is None:
                problems.append(
                    f'{label}: cited run {run} records no project revision, so its '
                    f'descent from {repair_commit} cannot be established')
                continue
            ancestor = is_ancestor(repair_commit, revision)
            if ancestor is None:
                problems.append(
                    f'{label}: cannot resolve whether {repair_commit} descends into '
                    f'{run}\'s revision {revision[:9]}; refusing to assume it does')
            elif ancestor is False:
                problems.append(
                    f'{label}: cited as {role!r} by {run}, but that run\'s revision '
                    f'{revision[:9]} does not descend from the repair {repair_commit}. '
                    f'The return proves the path ran, not that it ran with the repair.')
            # Descent is necessary but NOT sufficient: a later commit can change the
            # span again, and then the run exercised a different body than the one
            # the record describes.
            problems.extend(manifest_agreement(label, run, address, row))
        elif role in NEGATIVE_ROLES:
            if address in exercised_addresses(log):
                problems.append(
                    f'{label}: cited as {role!r} by {run}, but that log DOES contain an '
                    f'ABI-verified return for {address}')
        elif role in DISCOVERY_ROLES:
            if (address not in defect_addresses(log)
                    and address not in exercised_addresses(log)
                    and address not in icall_history_addresses(log)):
                problems.append(
                    f'{label}: cited as {role!r} by {run}, but that log names {address} '
                    f'in no unresolved-call, ABI-failure or alias-misdispatch line, and '
                    f'not as an ICALL-history frame')

    # The state must be supported by the roles actually cited.
    confirming = roles_seen & CONFIRMING_ROLES
    if state in CONFIRMED_STATES and not confirming:
        problems.append(
            f'{label}: state is {state} but no evidence row has a confirming role '
            f'({sorted(CONFIRMING_ROLES)}); roles present: {sorted(roles_seen)}. '
            f'This is the false-continuation claim this gate exists to fail.')
    if state in UNCONFIRMED_STATES and confirming:
        problems.append(
            f'{label}: state is {state} but evidence already confirms it via '
            f'{sorted(confirming)}; the record understates a proved result')
    return problems


def load_record(path: Path) -> list[dict]:
    data = json.loads(path.read_text(encoding='utf-8'))
    if isinstance(data, dict):
        rows = data.get('stops')
    else:
        rows = data
    if not isinstance(rows, list):
        raise ValueError(f'{path}: expected a list of rows or {{"stops": [...]}}')
    return rows


def main() -> int:
    global RUNS, CURRENT_MANIFEST, CURRENT_MANIFEST_ENTRIES
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--record', type=Path, default=RECORD)
    parser.add_argument('--runs', type=Path, default=None)
    parser.add_argument('--manifest', type=Path, default=None)
    parser.add_argument('--quiet', action='store_true')
    args = parser.parse_args()

    if args.runs is not None:
        RUNS = args.runs
    if args.manifest is not None:
        CURRENT_MANIFEST = args.manifest
    if CURRENT_MANIFEST.is_file():
        try:
            entries = json.loads(CURRENT_MANIFEST.read_text(encoding='utf-8'))
            CURRENT_MANIFEST_ENTRIES = {
                normalise(e['start']): (e.get('end'), e.get('stack_args'))
                for e in entries if isinstance(e, dict) and isinstance(e.get('start'), str)}
        except (OSError, ValueError, KeyError):
            CURRENT_MANIFEST_ENTRIES = None

    if not args.record.is_file():
        print(f'check-stop-chain: {args.record} is missing; the stop chain has no record',
              file=sys.stderr)
        return 2
    try:
        rows = load_record(args.record)
    except (OSError, ValueError) as exc:
        print(f'check-stop-chain: {exc}', file=sys.stderr)
        return 2

    cache: dict = {}
    problems: list[str] = []
    for row in rows:
        problems.extend(check_row(row, cache))

    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f'check-stop-chain: FAIL ({len(problems)} problem(s) in {len(rows)} row(s))',
              file=sys.stderr)
        return 1
    if not args.quiet:
        states: dict[str, int] = {}
        for row in rows:
            states[row.get('state', '?')] = states.get(row.get('state', '?'), 0) + 1
        summary = ', '.join(f'{v} {k}' for k, v in sorted(states.items()))
        print(f'check-stop-chain: PASS ({len(rows)} row(s): {summary})')
    return 0


if __name__ == '__main__':
    sys.exit(main())
