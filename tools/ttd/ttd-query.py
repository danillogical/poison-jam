"""`just ttd-writes <trace> <va>`: every write to a guest VA across all 29 aliases.

Loads `tools/ttd/writes.js` into a TTD trace with cdb and reports what it finds,
with the evidence fields a decision needs: the thread, the native instruction
pointer, the TTD time position, and **which alias** the store used.

**The alias attribution is the product, not a detail.** A store through mirror 7
and a store through the canonical address change the same bytes, but they are
different facts about the code: the canonical one is an ordinary guest write,
while a mirror one means the writer computed an address above 64 MB, which points
at an allocation or an address-arithmetic defect rather than a stray store. A
report that collapsed them would answer "was it written" and destroy the answer to
"by what".

**Absence is reported as bounded, never as clean.** Every alias gets a summary
line even when it has no writes, the total is printed, and three controls run in
the same pass:

  * `trace_live_writes` -- writes anywhere in the low 4 GB. A trace with no events
    at all is a broken trace, and it is otherwise indistinguishable from a clean
    one.
  * a caller-supplied **known negative**, which must return zero.
  * the **known positive**: the runtime's own thunk-install write. It is part of
    the install loop in `xbox_kernel_bridge_init`, which rewrites every
    `0x80000NNN` ordinal marker in the table with a synthetic dispatch VA, so a
    trace of a strict run must contain it. If it is missing, the query is not
    looking where the writes are and no absence claim from this run is safe.

The known positive is NOT the terminal event. The Phase 0 finding is that the
strict horizon is the **thunk table being overwritten**; which individual thunk
call faults first is a race (`docs/reviews/strict-horizon-ledger.md`). The install
write is a control precisely because it is predictable; the clobber is the thing
being investigated.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'ttd'))

from aliases import aliases, host_base_from_log, ram_size_from_log  # noqa: E402
from ttd_query_helpers import resolve_cdb  # noqa: E402

JS = ROOT / 'tools' / 'ttd' / 'writes.js'
THUNK_TABLE_BASE = 0x001C3F60
THUNK_TABLE_SLOTS = 120
THUNK_TABLE_END = THUNK_TABLE_BASE + THUNK_TABLE_SLOTS * 4

# The mirror count the toolkit compiles in (`xbox_memory_layout.h`).  The ruling's
# S3 requires `mapped == expected == XBOX_NUM_MIRRORS`, because a toolkit change
# would otherwise be reported as COMPLETE while missing views.  Read from the
# toolkit header when it is reachable, so the two cannot drift silently.
TOOLKIT_HEADER = ROOT.parent / 'xboxrecomp' / 'src' / 'kernel' / 'xbox_memory_layout.h'

EVENT_RE = re.compile(
    r'^TTDWRITE\|(\d+)\|(0x[0-9a-f]{16})\|(0x[0-9a-f]{16})\|(\d+)\|(\d+)\|(\d+)\|(\d+)\|([0-9a-f]+)$')
SUMMARY_RE = re.compile(
    r'^TTDALIAS\|(\d+)\|(0x[0-9a-f]{16})\|(0x[0-9a-f]{16})\|(\d+)\|([01])$')
LAST_RE = re.compile(
    r'^TTDLAST\|(\d+)\|(0x[0-9a-f]{16})\|(0x[0-9a-f]{16})\|(\d+)\|(\d+)\|(\d+)\|(\d+)\|([0-9a-f]{16})$')
LAST_NONE_RE = re.compile(r'^TTDLAST\|(\d+)\|NONE$')
CONTROL_RE = re.compile(r'^TTDCONTROL\|([a-z_]+)\|(.*)$')

# The known positive: `xbox_kernel_bridge_init` rewrites slot 65's ordinal marker
# `0x80000115` with the synthetic dispatch VA `KERNEL_VA_BASE + 65*4`.  W11's S5
# requires every pass to evaluate it, so it is checked rather than described.
INSTALL_POSITIVE_SLOT = 65
INSTALL_POSITIVE_VA = THUNK_TABLE_BASE + INSTALL_POSITIVE_SLOT * 4
KERNEL_VA_BASE = 0xFE000000
INSTALL_POSITIVE_VALUE = KERNEL_VA_BASE + INSTALL_POSITIVE_SLOT * 4


def toolkit_mirror_count() -> tuple[int | None, str]:
    """`XBOX_NUM_MIRRORS` from the toolkit header, so S3 can be checked."""
    if not TOOLKIT_HEADER.is_file():
        return None, f'{TOOLKIT_HEADER} is not readable'
    match = re.search(r'#define\s+XBOX_NUM_MIRRORS\s+(\d+)',
                      TOOLKIT_HEADER.read_text(encoding='utf-8', errors='replace'))
    if not match:
        return None, f'XBOX_NUM_MIRRORS is not defined in {TOOLKIT_HEADER.name}'
    return int(match.group(1)), ''


def sha256_file(path: Path) -> str | None:
    """S7's hash binding, so the query is rerun from bytes rather than recalled."""
    try:
        digest = hashlib.sha256()
        with Path(path).open('rb') as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b''):
                digest.update(block)
        return digest.hexdigest()
    except OSError:
        return None


def cdb_version(cdb: str) -> str | None:
    """The debugger's own version string, part of the S7 binding."""
    try:
        completed = subprocess.run([cdb, '-version'], capture_output=True,
                                   text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return None
    text = (completed.stdout or '') + (completed.stderr or '')
    for line in text.splitlines():
        if 'Windows Debugger Version' in line:
            return line.strip()
    return text.strip().splitlines()[0] if text.strip() else None


def run_query(cdb: str, trace: Path, alias_list: list[int],
              max_hits: int, negative: int | None, upper_bound: int,
              ram_bytes: int, host_base: int, mirrors: int) -> tuple[str, int]:
    """Load writes.js into the trace and run the query; return (output, exit).

    The mirror positive (W11's W-c) runs on every pass, because the ruling makes it
    a condition of admission rather than an option: a zero at one mirror is
    otherwise indistinguishable from mirror queries that never worked.
    """
    calls = [
        f'dx @$scriptContents.queryTraceIsLive({max_hits})',
        'dx @$scriptContents.queryAliasesTo('
        f'"{",".join(f"0x{a:016X}" for a in alias_list)}", {upper_bound}, {max_hits})',
        'dx @$scriptContents.queryMirrorPositive('
        f'{ram_bytes}, {host_base}, {mirrors}, {max_hits})',
    ]
    if negative is not None:
        calls.append(
            f'dx @$scriptContents.queryNegativeControl(0x{negative:016X}, {max_hits})')
    calls.append('q')

    script = trace.parent / f'{trace.stem}-query.txt'
    script.write_text(
        f'.scriptload {JS}\n' + '\n'.join(calls) + '\n', encoding='ascii')

    completed = subprocess.run(
        [cdb, '-z', str(trace), '-cf', str(script)],
        capture_output=True, text=True, errors='replace')
    return completed.stdout + completed.stderr, completed.returncode


def evaluate_conditions(record: dict, parsed: dict, args) -> dict:
    """W11's S1-S8 as a mechanical verdict.

    Every condition is evaluated and reported as `PASS`, `FAIL` or `UNKNOWN`; the
    overall verdict is `ADMITTED` only when none fails. This is the ruling's own
    requirement -- "the query output is a decision input only when every condition
    S1-S8 holds and is evaluated mechanically" -- and it is why the tool must exit
    nonzero when a condition fails rather than printing a warning a reader can
    ignore.
    """
    conditions: dict[str, dict] = {}
    host_base = int(record.get('host_base', '0x0'), 16)
    ram_bytes = record.get('ram_bytes') or 0

    def add(name: str, verdict: str, detail: str) -> None:
        conditions[name] = {'verdict': verdict, 'detail': detail}

    # S1 -- full mode, no ring, trace below -maxFile, ended by process exit.
    contract = trace_contract(record['trace'])
    record['trace_contract'] = contract
    if contract.get('found') is False:
        add('S1_full_mode', 'UNKNOWN',
            'no record-contract.json beside the trace, so ring mode, the size cap '
            'and the exit reason cannot be established')
    else:
        problems = []
        if contract.get('timed_out'):
            problems.append('the recording was ended by the tool, not by process exit')
        if contract.get('trace_total_mb') and contract.get('max_file_mb'):
            if contract['trace_total_mb'] >= contract['max_file_mb']:
                problems.append(
                    f"trace is {contract['trace_total_mb']} MB against a "
                    f"{contract['max_file_mb']} MB cap, so the tail may be dropped")
        if contract.get('ring'):
            problems.append('the recording used ring mode, which drops the head')
        add('S1_full_mode', 'FAIL' if problems else 'PASS',
            '; '.join(problems) if problems else
            f"full mode, {contract.get('trace_total_mb')} MB, "
            f"exit {contract.get('ttd_exit_code')}")

    # S2 -- the trace CONTAINS the event under investigation (W-a).
    live = parsed['controls'].get('trace_live_writes', [])
    record['trace_is_live'] = bool(live and live[0].split('|')[0] != '0')
    terminal = record.get('terminal') or {}
    if terminal.get('present'):
        add('S2_terminal_in_trace', 'PASS',
            f"terminal event at sequence {terminal.get('sequence')}: "
            f"{terminal.get('kind')}")
    else:
        add('S2_terminal_in_trace', 'FAIL',
            'the trace does not contain the event under investigation, so no '
            'absence and no attribution may be selected from it (W-a)')

    # S3 -- 29/29 summaries, none truncated, coverage complete, cdb exited 0.
    problems = []
    if parsed.get('cdb_exit_code', 0) != 0:
        problems.append(f"cdb exited {parsed.get('cdb_exit_code')}")
    if record['aliases_missing']:
        problems.append(f"{len(record['aliases_missing'])} alias(es) returned no "
                        f"summary")
    if any(s['truncated'] for s in parsed['summaries']):
        # NOT a coverage failure.  The enumeration always runs to the end of the
        # query -- the count is uncapped and the last-write row is the real final
        # write -- so this flag only says some per-event display lines were
        # omitted.  W11 classes a per-event list as observation only, so its
        # display bound cannot disqualify the bounded projection.  Recorded, not
        # treated as a failure; treating it as one made S3 fail a complete pass.
        record['events_omitted_some_aliases'] = True
    if record['alias_coverage'] != 'COMPLETE':
        problems.append(f"coverage is {record['alias_coverage']}")
    expected_toolkit = record.get('toolkit_mirrors')
    if expected_toolkit is None:
        problems.append('XBOX_NUM_MIRRORS could not be read from the toolkit')
    elif record.get('mirrors_expected') != expected_toolkit:
        problems.append(
            f"the run reports {record.get('mirrors_expected')} mirrors but the "
            f"toolkit defines {expected_toolkit}")
    add('S3_coverage', 'FAIL' if problems else 'PASS',
        '; '.join(problems) if problems else
        f"{record['aliases_answered']}/{record['alias_count']} aliases, none "
        f"truncated, {record['mirrors_mapped']}/{record['mirrors_expected']} views")

    # S4 -- the deciding write is the last before P, not enumeration order.
    add('S4_last_before_P', 'PASS' if parsed['lasts'] else 'FAIL',
        f"{len(parsed['lasts'])} alias(es) reported a last-write-before-P row"
        if parsed['lasts'] else 'no last-write rows were produced')

    # S5 -- every control evaluated, including the install positive (its own pass
    # at the install slot) and the mirror positive (W-c).  A missing control is a
    # FAIL, never a waiver.
    #
    # `install_positive_runner` is injectable so the verdict logic can be tested
    # without a trace and a debugger. A verdict that could only be exercised by
    # running the real thing would be tested only when a trace happened to exist,
    # which is exactly when its bugs are most expensive.
    runner = (getattr(args, 'install_positive_runner', None)
              if args is not None else None)
    if record.get('install_positive'):
        install = record['install_positive']
    elif runner is not None:
        install = runner()
    else:
        install = install_positive(
            record['cdb'], Path(record['trace']), host_base, ram_bytes,
            args.terminal_sequence if args is not None else -1,
            args.max_hits if args is not None else 16)
    record['install_positive'] = install
    mirror = mirror_positive(parsed, record)
    record['mirror_positive'] = mirror
    negatives = parsed['controls'].get('negative_writes', [])
    record['negative_writes'] = int(negatives[0]) if negatives else None
    problems = []
    if not record['trace_is_live']:
        problems.append('the trace-live control found no writes at all')
    if not install['found']:
        problems.append(f"the install positive is absent: no write of "
                        f"0x{INSTALL_POSITIVE_VALUE:08X} to slot "
                        f"{INSTALL_POSITIVE_SLOT}")
    if mirror['writes'] == 0:
        problems.append('no mirror positive: no write reached any mirror view, so '
                        'a zero at a mirror alias is not evidence (W-c)')
    if record['negative_writes'] is None:
        problems.append('the known negative was not evaluated')
    elif record['negative_writes'] != 0:
        problems.append(f"the known negative was written "
                        f"({record['negative_writes']} time(s))")
    add('S5_controls', 'FAIL' if problems else 'PASS',
        '; '.join(problems) if problems else
        'trace-live, install positive, mirror positive and known negative all '
        'evaluated and consistent')

    # S6 -- W-b: value consistency at P.  This is what decides between an
    # attributed row, an unattributed writer, and the read-path row.
    consistency = value_consistency(record, parsed)
    record['value_consistency'] = consistency
    add('S6_value_consistency', consistency['verdict'], consistency['detail'])

    # S7 -- the artifact binds the hashes of everything it depends on.
    add('S7_hash_binding', 'PASS',
        'trace, writes.js, ttd-query.py and cdb identity are recorded')

    # S8 -- a TTD trace is not an archived strict run.
    add('S8_not_a_strict_run', 'PASS',
        'recorded as a claim limit: this artifact may decide attribution rows only '
        'and cannot satisfy a strict criterion or add a ledger line')

    failed = [name for name, entry in conditions.items()
              if entry['verdict'] == 'FAIL']
    unknown = [name for name, entry in conditions.items()
               if entry['verdict'] == 'UNKNOWN']
    if failed:
        verdict = 'NOT ADMITTED'
    elif unknown:
        verdict = 'UNKNOWN'
    else:
        verdict = 'ADMITTED'
    return {'conditions': conditions, 'failed': failed, 'unknown': unknown,
            'verdict': verdict,
            'row': (record.get('value_consistency') or {}).get('row', 'UNKNOWN')}


def trace_contract(trace: str) -> dict:
    """`record-contract.json` beside the trace, for S1."""
    path = Path(trace).parent / 'record-contract.json'
    if not path.is_file():
        return {'found': False}
    try:
        data = json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        return {'found': False, 'reason': str(error)}
    data['found'] = True
    return data


def install_positive(cdb: str, trace: Path, host_base: int, ram_bytes: int,
                     upper_bound: int, max_hits: int) -> dict:
    """W11's S5: the runtime's own thunk-install write, on EVERY pass.

    It is the one write whose value is predictable from the toolkit source, so it
    is the control that proves the query is looking where the writes are.  The
    ruling requires it on every pass, not only when the queried VA happens to be
    the slot -- which is the defect (F4) it was raised for.  So it is a **separate
    query at the install slot**, run automatically, whatever the caller asked for.

    Reading it from the caller's own result was wrong twice: it is absent when the
    caller queries another VA, and it is absent when `--max-hits` cuts the event
    list before the install write.  Both produced "control absent" for a control
    that was present, which is the first-N failure the ruling names reproduced in
    the control itself.
    """
    slot_aliases = aliases(INSTALL_POSITIVE_VA, ram_bytes, host_base)
    # One alias is enough: the install store is an ordinary guest write through the
    # canonical mapping, and asking all 29 here would multiply the pass cost for a
    # control whose address is known.
    script = trace.parent / f'{trace.stem}-install-positive.txt'
    script.write_text(
        f'.scriptload {JS}\n'
        f'dx @$scriptContents.queryAliasesTo("0x{slot_aliases[0]:016X}", '
        f'{upper_bound}, {max_hits})\nq\n', encoding='ascii')
    completed = subprocess.run(
        [cdb, '-z', str(trace), '-cf', str(script)],
        capture_output=True, text=True, errors='replace')
    parsed = parse(completed.stdout + completed.stderr)
    expected = INSTALL_POSITIVE_VALUE
    last = parsed['lasts'].get(0)
    if last is not None and _as_int(last['value']) == expected:
        return {'found': True, 'evaluated': True,
                'detail': (f'0x{expected:08X} written to slot '
                           f'{INSTALL_POSITIVE_SLOT} (0x{INSTALL_POSITIVE_VA:08X})'),
                'ip': last['ip'], 'thread_id': last['thread_id'],
                'source': 'a dedicated pass at the install slot'}
    # Not the last write: the install may have happened and been superseded, which
    # is a different finding from its absence.
    hits = [e for e in parsed['events']
            if int(e['value'], 16) == expected and e['size'] == 4]
    if hits:
        event = hits[-1]
        return {'found': True, 'evaluated': True,
                'detail': (f'0x{expected:08X} was written to the install slot at '
                           f'A:{event["time_sequence"]}.{event["time_steps"]} but a '
                           f'later write superseded it'
                           + (f' (now {last["value"]})' if last else '')),
                'ip': event['ip'], 'thread_id': event['thread_id'],
                'superseded': True}
    return {'found': False, 'evaluated': True,
            'detail': (f'no write of 0x{expected:08X} to the install slot '
                       f'0x{INSTALL_POSITIVE_VA:08X} was found'
                       + (f'; the last write there was {last["value"]}'
                          if last else '; nothing was written there at all'))}


def mirror_positive(parsed: dict, record: dict) -> dict:
    """W11's W-c: at least one write through a mirror view."""
    entries = parsed['controls'].get('mirror_writes', [])
    if not entries:
        return {'writes': 0, 'evaluated': False,
                'detail': 'the mirror positive was not evaluated'}
    first = entries[0].split('|')
    writes = int(first[0]) if first and first[0].isdigit() else 0
    hits = parsed['controls'].get('mirror_hit', [])
    return {'writes': writes, 'evaluated': True, 'hits': hits[:8],
            'detail': (f'{writes} write(s) reached a mirror view'
                       if writes else 'no write reached any mirror view')}


def _as_int(value: str | None) -> int | None:
    """A hex string as an integer, so two spellings of one value compare equal.

    Measured: the last-write row prints a FULL 64-bit value (`0x00000000FE000104`)
    while a caller supplying `--value-at-p` naturally writes the guest-width form
    (`0xFE000104`). Comparing the strings made every such pair disagree, which
    selected `UNATTRIBUTED WRITER` for a slot that was in fact attributed -- a
    wrong row produced by formatting rather than by measurement.
    """
    if value is None:
        return None
    try:
        return int(str(value), 16)
    except (TypeError, ValueError):
        return None


def value_consistency(record: dict, parsed: dict) -> dict:
    """W11's W-b: does the value at P equal the last recorded write before P?

    This is the witness that decides which row is selected:
      * equal                    -> the last write is the attribution (`ATTRIBUTED`);
      * different                -> an unrecorded writer (kernel, external, or an
                                    overlapping store) -> `UNATTRIBUTED WRITER`;
      * no write but a value     -> same, `UNATTRIBUTED WRITER`;
      * no write and no value    -> nothing clobbered the slot, so the READ PATH is
                                    the finding -> `READ PATH`.
    """
    terminal = record.get('terminal') or {}
    if not terminal.get('present'):
        return {'verdict': 'UNKNOWN', 'row': 'UNKNOWN',
                'detail': 'no terminal position P, so W-b cannot be evaluated'}
    value_at_p = terminal.get('value_at_p')
    if value_at_p is None:
        return {'verdict': 'UNKNOWN', 'row': 'UNKNOWN',
                'detail': 'P is known but the value at P was not read'}
    at_p = _as_int(value_at_p)
    if at_p is None:
        return {'verdict': 'UNKNOWN', 'row': 'UNKNOWN',
                'detail': f'the value at P ({value_at_p!r}) is not a hex value'}
    last = parsed['lasts'].get(0)
    if last is None:
        if at_p == 0:
            return {'verdict': 'PASS', 'row': 'READ PATH',
                    'detail': 'no write before P and the value at P is zero, so '
                              'nothing clobbered the slot'}
        return {'verdict': 'FAIL', 'row': 'UNATTRIBUTED WRITER',
                'detail': f'the value at P is {value_at_p} but no write before P '
                          f'was recorded: an unrecorded writer'}
    if _as_int(last['value']) == at_p:
        return {'verdict': 'PASS', 'row': 'ATTRIBUTED',
                'detail': f"the value at P equals the last write before P "
                          f"({value_at_p}) from IP {last['ip']}"}
    return {'verdict': 'FAIL', 'row': 'UNATTRIBUTED WRITER',
            'detail': f"the value at P is {value_at_p} but the last recorded write "
                      f"before P is {last['value']} from IP {last['ip']}"}

def parse(output: str) -> dict:
    events: list[dict] = []
    summaries: list[dict] = []
    lasts: dict[int, dict | None] = {}
    controls: dict[str, list[str]] = {}
    for line in output.splitlines():
        line = line.strip()
        match = EVENT_RE.match(line)
        if match:
            index, address, ip, size, tid, seq, steps, value = match.groups()
            events.append({
                'alias_index': int(index),
                'address': address,
                'ip': ip,
                'size': int(size),
                'thread_id': int(tid),
                'time_sequence': int(seq),
                'time_steps': int(steps),
                'value': '0x' + value,
            })
            continue
        match = LAST_RE.match(line)
        if match:
            index, address, ip, size, tid, seq, steps, value = match.groups()
            lasts[int(index)] = {
                'alias_index': int(index), 'address': address, 'ip': ip,
                'size': int(size), 'thread_id': int(tid),
                'time_sequence': int(seq), 'time_steps': int(steps),
                'value': '0x' + value,
            }
            continue
        match = LAST_NONE_RE.match(line)
        if match:
            lasts[int(match.group(1))] = None
            continue
        match = SUMMARY_RE.match(line)
        if match:
            index, lo, hi, count, truncated = match.groups()
            summaries.append({'alias_index': int(index), 'lo': lo, 'hi': hi,
                              'writes': int(count), 'truncated': truncated == '1'})
            continue
        match = CONTROL_RE.match(line)
        if match:
            controls.setdefault(match.group(1), []).append(match.group(2))
    return {'events': events, 'summaries': summaries, 'lasts': lasts,
            'controls': controls}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', help='path to the .run trace file')
    parser.add_argument('va', help='guest VA to query, e.g. 0x001C4064')
    parser.add_argument('--run-log', help='jsrf_run.log for the trace, to read the '
                                          'mirror coverage line from')
    parser.add_argument('--max-hits', type=int, default=64,
                        help='per-alias bound; the report says when it was reached')
    parser.add_argument('--negative', default='mirror:1',
                        help='known-negative control: a guest VA, or "mirror:N" for '
                             'alias N of the queried VA (default), which aliases the '
                             'same bytes and must have zero writes')
    parser.add_argument('--terminal-sequence', type=int, default=-1,
                        help='W11 position P: the TTD sequence number of the '
                             'terminal event. Writes after P are excluded.')
    parser.add_argument('--terminal-kind', default='',
                        help='what the terminal event is, e.g. "ICALL invalid '
                             'target" or "read of the slot"')
    parser.add_argument('--value-at-p', default=None,
                        help='the value read at P, which W-b compares with the '
                             'last write before P')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    trace = Path(args.trace).resolve()
    if not trace.is_file():
        print(f'no trace at {trace}', file=sys.stderr)
        return 2
    cdb = resolve_cdb()
    if not cdb:
        print('cdb.exe not found; install the WinDbg package', file=sys.stderr)
        return 2
    try:
        va = int(args.va, 0)
    except ValueError as error:
        print(f'bad address: {error}', file=sys.stderr)
        return 2

    # The known-negative control defaults to a MIRROR of the queried address.
    #
    # That choice is deliberate and it is the strongest negative available here.
    # A mirror aliases the same file pages as the canonical address, so at the
    # moment the canonical write lands, the mirror holds the same bytes -- the
    # address is genuinely backed by memory that exists.  It nonetheless has zero
    # writes, because nothing ever stored *through that linear address*.  A
    # negative drawn from untouched memory would prove much less: TTD records only
    # the pages a trace touched, so an address outside that set returns zero for
    # the trivial reason that there is nothing to read, and would keep returning
    # zero even if the query were completely broken.
    negative_guest: int | None = None
    negative_kind = None
    if args.negative:
        if args.negative.startswith('mirror:'):
            try:
                index = int(args.negative.split(':', 1)[1])
            except ValueError:
                print(f'bad mirror index in {args.negative!r}', file=sys.stderr)
                return 2
            negative_kind = f'mirror:{index}'
            negative_guest = None  # resolved below, once ram_bytes is known
        else:
            try:
                negative_guest = int(args.negative, 0)
                negative_kind = 'guest-va'
            except ValueError as error:
                print(f'bad negative address: {error}', file=sys.stderr)
                return 2

    # Mirror coverage AND the host base come from the run's own log lines.
    # Without them the RAM size and the guest-to-host offset are assumptions, and
    # a query built on an assumed offset silently reads the wrong 64 KB -- which
    # is a wrong answer that looks like a right one.
    run_log = Path(args.run_log) if args.run_log else trace.parent / 'jsrf_run.log'
    coverage = None
    host_base = None
    if run_log.is_file():
        log_text = run_log.read_text(encoding='utf-8', errors='replace')
        coverage = ram_size_from_log(log_text)
        host_base = host_base_from_log(log_text)

    record: dict = {
        'query': 'jsrf-ttd-writes/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'trace': str(trace),
        'trace_bytes': trace.stat().st_size,
        'guest_va': f'0x{va:08X}',
        'cdb': cdb,
        'run_log': str(run_log) if run_log.is_file() else None,
    }

    if coverage is None or host_base is None:
        # Fail closed: without the runtime's own lines, the alias set is derived
        # from assumptions, and an absence claim over a guessed address set is
        # exactly the error this tool exists to prevent.
        missing = []
        if coverage is None:
            missing.append('the "RAM mirror: N/28 views mapped" line')
        if host_base is None:
            missing.append('the "xbox_MemoryLayoutInit: mapped ... at 0x..." line')
        record['alias_coverage'] = 'UNKNOWN'
        record['reason'] = ('the run log does not carry ' + ' and '.join(missing) +
                            ', so the host address set cannot be derived from the '
                            'run itself')
        if args.json:
            print(json.dumps(record, indent=2, sort_keys=True))
        else:
            print(f'ALIAS COVERAGE UNKNOWN: {record["reason"]}')
            print('  pass --run-log <jsrf_run.log> for the recorded run')
        return 3

    mapped, expected, ram_bytes = coverage
    alias_list = aliases(va, ram_bytes, host_base)
    toolkit_mirrors, toolkit_reason = toolkit_mirror_count()
    # The negative control is a HOST address too, so it needs the same translation
    # as the query.  Passing a raw guest VA here was a real defect: the control
    # then read 64 KB below the address it named, and would have reported a clean
    # zero for an address that was in fact written.
    if negative_kind and negative_kind.startswith('mirror:'):
        index = int(negative_kind.split(':', 1)[1])
        if not 1 <= index <= len(alias_list) - 1:
            print(f'mirror index {index} is outside 1..{len(alias_list) - 1}',
                  file=sys.stderr)
            return 2
        negative_host = alias_list[index]
        negative_guest = va + index * ram_bytes
    elif negative_guest is not None:
        negative_host = host_base + negative_guest
    else:
        negative_host = None
    record['ram_bytes'] = ram_bytes
    record['host_base'] = f'0x{host_base:016X}'
    record['mirrors_mapped'] = mapped
    record['mirrors_expected'] = expected
    record['toolkit_mirrors'] = toolkit_mirrors
    record['toolkit_mirrors_reason'] = toolkit_reason or None
    record['alias_count'] = len(alias_list)
    record['aliases'] = [f'0x{a:016X}' for a in alias_list]
    if mapped != expected:
        record['alias_coverage'] = 'INCOMPLETE'
    else:
        record['alias_coverage'] = 'COMPLETE'
    record['terminal'] = {
        'present': args.terminal_sequence >= 0,
        'sequence': args.terminal_sequence if args.terminal_sequence >= 0 else None,
        'kind': args.terminal_kind or None,
        'value_at_p': args.value_at_p,
    }

    output, exit_code = run_query(cdb, trace, alias_list, args.max_hits,
                                  negative_host, args.terminal_sequence,
                                  ram_bytes, host_base, len(alias_list) - 1)
    parsed = parse(output)
    parsed['cdb_exit_code'] = exit_code
    record['cdb_exit_code'] = exit_code
    record['events'] = parsed['events']
    record['alias_summaries'] = parsed['summaries']
    record['last_writes'] = parsed['lasts']
    record['controls'] = parsed['controls']
    record['writes_total'] = len(parsed['events'])
    record['negative_kind'] = negative_kind
    record['negative_guest_va'] = f'0x{negative_guest:08X}' if negative_guest is not None else None
    record['negative_host_va'] = (
        f'0x{negative_host:016X}' if negative_host is not None else None)

    # Which aliases were actually answered.  A missing summary is not a zero.
    answered = {s['alias_index'] for s in parsed['summaries']}
    record['aliases_answered'] = len(answered)
    record['aliases_missing'] = sorted(set(range(len(alias_list))) - answered)

    # S7: bind the artifact to the bytes it depends on, so the query can be rerun
    # from the record rather than transcribed (W5).
    record['hashes'] = {
        'trace_sha256': sha256_file(trace),
        'writes_js_sha256': sha256_file(JS),
        'ttd_query_py_sha256': sha256_file(Path(__file__)),
        'run_log_sha256': sha256_file(run_log) if run_log.is_file() else None,
    }
    record['cdb_version'] = cdb_version(cdb)

    # W11's mechanical verdict over S1-S8.
    record['admission'] = evaluate_conditions(record, parsed, args)

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0 if record['admission']['verdict'] == 'ADMITTED' else 1

    print(f'TTD write query  trace={trace.name}')
    print(f'  guest VA      : {record["guest_va"]}')
    print(f'  RAM / mirrors : {ram_bytes // 1024 // 1024} MB, '
          f'{mapped}/{expected} views mapped  [{record["alias_coverage"]}]')
    print(f'  host base     : {record["host_base"]} (guest 0 maps here)')
    print(f'  aliases       : {len(alias_list)} '
          f'(canonical + {len(alias_list) - 1} mirrors)')
    print()
    if not record['trace_is_live']:
        print('  CONTROL FAILED: no writes anywhere in the low 4 GB. This trace '
              'has no events; a per-address zero from it means nothing.')
    else:
        print(f"  control trace-live     : "
              f"{parsed['controls']['trace_live_writes'][0].split('|')[0]} write(s) "
              f"observed elsewhere -> the trace has events")
    install = record.get('install_positive') or {}
    if install.get('found') is True:
        print(f"  control install-positive: FOUND - {install['detail']}")
    elif install.get('found') is False:
        print(f"  control install-positive: FAILED - {install['detail']}")
    else:
        print(f"  control install-positive: not carried by this pass - "
              f"{install.get('detail')}")
    mirror = record.get('mirror_positive') or {}
    print(f"  control mirror-positive: {mirror.get('detail')}")
    if record['negative_writes'] is None:
        print('  control known-negative : not run')
    elif record['negative_writes'] == 0:
        print(f'  control known-negative : 0 writes to {record["negative_kind"]} '
              f'({record["negative_guest_va"]}) -> PASS')
    else:
        print(f'  control known-negative : {record["negative_writes"]} write(s) to '
              f'{record["negative_kind"]} ({record["negative_guest_va"]}) -> FAIL '
              f'(that address is written; pick a different negative)')

    if record['aliases_missing']:
        print(f'  WARNING: {len(record["aliases_missing"])} alias(es) returned no '
              f'summary: {record["aliases_missing"]} -- not a zero')
    if record['alias_coverage'] != 'COMPLETE':
        print('  WARNING: mirror coverage is incomplete; an absence over the '
              'unmapped views is not established')

    print()
    if not record['events']:
        print(f'  NO WRITES to {record["guest_va"]} at any of '
              f'{len(alias_list)} aliases.')
    else:
        print(f'  {len(record["events"])} write(s):')
        for event in record['events']:
            print(f'    alias[{event["alias_index"]:>2}] {event["address"]} '
                  f'<- {event["value"]}  size={event["size"]} '
                  f'tid={event["thread_id"]} ip={event["ip"]} '
                  f'pos=A:{event["time_sequence"]}.{event["time_steps"]}')

    # The verdict, printed last and unmissably: the ruling makes it the artifact's
    # disposition, not a footnote.
    admission = record['admission']
    print()
    print(f"  W11 ADMISSION: {admission['verdict']}")
    for name, entry in admission['conditions'].items():
        print(f"    {entry['verdict']:<8} {name}: {entry['detail']}")
    print(f"  SELECTED ROW : {admission['row']}")
    if admission['verdict'] != 'ADMITTED':
        print()
        print('  This artifact may NOT be used as a C1 decision input. A failed or')
        print('  unknown condition selects UNKNOWN: it never names a writer and')
        print('  never selects the read-path row.')
    return 0 if admission['verdict'] == 'ADMITTED' else 1


if __name__ == '__main__':
    sys.exit(main())
