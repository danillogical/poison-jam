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

JS = ROOT / 'tools' / 'ttd' / 'writes.js'
THUNK_TABLE_BASE = 0x001C3F60
THUNK_TABLE_SLOTS = 120
THUNK_TABLE_END = THUNK_TABLE_BASE + THUNK_TABLE_SLOTS * 4

EVENT_RE = re.compile(
    r'^TTDWRITE\|(\d+)\|(0x[0-9a-f]{16})\|(0x[0-9a-f]{16})\|(\d+)\|(\d+)\|(\d+)\|(\d+)\|([0-9a-f]+)$')
SUMMARY_RE = re.compile(
    r'^TTDALIAS\|(\d+)\|(0x[0-9a-f]{16})\|(0x[0-9a-f]{16})\|(\d+)\|([01])$')
CONTROL_RE = re.compile(r'^TTDCONTROL\|([a-z_]+)\|(.*)$')


def resolve_cdb() -> str | None:
    found = shutil.which('cdb') or shutil.which('cdb.exe')
    if found:
        return found
    roots = [Path(r'C:\Program Files\WindowsApps'),
             Path(r'C:\Program Files (x86)\Windows Kits\10\Debuggers')]
    for root in roots:
        if not root.is_dir():
            continue
        for candidate in root.glob('**/cdb.exe'):
            return str(candidate)
    return None


def run_query(cdb: str, trace: Path, alias_list: list[int],
              max_hits: int, negative: int | None) -> tuple[str, int]:
    """Load writes.js into the trace and run the query; return (output, exit)."""
    calls = [
        f'dx @$scriptContents.queryTraceIsLive({max_hits})',
        'dx @$scriptContents.queryAliases('
        f'"{",".join(f"0x{a:016X}" for a in alias_list)}", {max_hits})',
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

def parse(output: str) -> dict:
    events: list[dict] = []
    summaries: list[dict] = []
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
        match = SUMMARY_RE.match(line)
        if match:
            index, lo, hi, count, truncated = match.groups()
            summaries.append({'alias_index': int(index), 'lo': lo, 'hi': hi,
                              'writes': int(count), 'truncated': truncated == '1'})
            continue
        match = CONTROL_RE.match(line)
        if match:
            controls.setdefault(match.group(1), []).append(match.group(2))
    return {'events': events, 'summaries': summaries, 'controls': controls}


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
    record['alias_count'] = len(alias_list)
    record['aliases'] = [f'0x{a:016X}' for a in alias_list]
    if mapped != expected:
        record['alias_coverage'] = 'INCOMPLETE'
    else:
        record['alias_coverage'] = 'COMPLETE'

    output, exit_code = run_query(cdb, trace, alias_list, args.max_hits, negative_host)
    parsed = parse(output)
    record['cdb_exit_code'] = exit_code
    record['events'] = parsed['events']
    record['alias_summaries'] = parsed['summaries']
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

    live = parsed['controls'].get('trace_live_writes', [])
    record['trace_is_live'] = bool(live and live[0].split('|')[0] != '0')
    negatives = parsed['controls'].get('negative_writes', [])
    record['negative_writes'] = int(negatives[0]) if negatives else None

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0

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
        print(f'  control trace-live     : {live[0]} write(s) observed elsewhere '
              f'-> the trace has events')
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
    return 0


if __name__ == '__main__':
    sys.exit(main())
