"""`tools/ttd/ttd-terminal.py`: find the terminal position P and the value at P.

**Why this is separate from `ttd-query.py`.** The query answers "what wrote this
address". W11's witnesses W-a and W-b need two facts about the *terminal event*
first: where in the trace it is (position P), and what the slot held when the read
that faulted happened. `ttd-query.py` accepts both as arguments
(`--terminal-sequence`, `--value-at-p`) precisely so the query stays a query; this
tool produces them from the trace.

**What it looks for, in order.**

  1. **The exception.** The run's own log carries the project's fatal code when the
     invalid indirect call is taken (`[EXCEPTION] … code=0xE0424943`), which gives
     the guest return address and the thread. That is the strongest anchor because it
     is written by the runtime at the event.
  2. **The last kernel call.** `[KERNEL] #N: ordinal O (slot S) … ret=R` gives the
     call the guest was making when it stopped, and its slot.
  3. **The trace's own end.** TTD's last recorded event on the faulting thread is the
     position P; the tool reports it as a sequence number for `--terminal-sequence`.

**The value at P is read from the trace, not from the log.** The log says what the
*guest* read; the trace says what memory *held*. W-b compares those two, so the tool
reads the slot's bytes at the end position with cdb and reports them. A value the log
does not mention is not invented.

**Read-only.** Every cdb command is a seek or a read (`!tt`, `dq`, `dd`, `r`); no
`e`, `p`, `t` or `g` is issued.
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'ttd'))

from ttd_query_helpers import resolve_cdb  # noqa: E402  (shared, see below)

# The runtime's fatal code for an invalid indirect call.
INVALID_ICALL_CODE = '0xE0424943'
ICALL = re.compile(
    r'\[ICALL\]\s+invalid target\s+0x([0-9A-Fa-f]+)\s+tid=(\d+)\s+'
    r'esp=([0-9A-Fa-f]+)\s+return=([0-9A-Fa-f]+)')
EXCEPTION = re.compile(
    r'\[EXCEPTION\]\s+tid=(\d+)\s+code=0x([0-9A-Fa-f]+)\s+RIP=0x([0-9A-Fa-f]+)')
KERNEL_CALL = re.compile(
    r'\[KERNEL\]\s+#(\d+):\s+ordinal\s+(\d+)\s+\(slot\s+(\d+)\)\s+'
    r'esp=0x([0-9A-Fa-f]+)\s+ret=0x([0-9A-Fa-f]+)\s+tid=(\d+)')
THUNK_TABLE_BASE = 0x001C3F60
THUNK_TABLE_SLOTS = 120


def log_evidence(run_log: Path) -> dict:
    """The terminal facts the run's own log carries."""
    text = run_log.read_text(encoding='utf-8', errors='replace')
    evidence: dict = {
        'log': str(run_log),
        'invalid_icalls': [],
        'exceptions': [],
        'last_kernel_call': None,
    }
    for match in ICALL.finditer(text):
        target, tid, esp, ret = match.groups()
        evidence['invalid_icalls'].append({
            'target': f'0x{target}', 'tid': int(tid), 'esp': esp,
            'return_address': ret,
        })
    for match in EXCEPTION.finditer(text):
        tid, code, rip = match.groups()
        evidence['exceptions'].append({
            'tid': int(tid), 'code': f'0x{code.upper()}', 'rip': f'0x{rip}',
        })
    kernels = KERNEL_CALL.findall(text)
    if kernels:
        number, ordinal, slot, esp, ret, tid = kernels[-1]
        evidence['last_kernel_call'] = {
            'n': int(number), 'ordinal': int(ordinal), 'slot': int(slot),
            'esp': esp, 'return_address': ret, 'tid': int(tid),
            'slot_va': f'0x{THUNK_TABLE_BASE + int(slot) * 4:08X}',
        }
    return evidence


def trace_end(cdb: str, trace: Path, thread: str | None,
              read_va: int | None, width: int) -> dict:
    """The trace's last position, and optionally the bytes there.

    A read at the end position, not a step: `!tt 100` seeks to the end, `~Ns`
    selects a thread, and `dq`/`dd` read. Nothing here executes guest code.
    """
    commands = ['!tt 100']
    if thread:
        commands.append(f'~{thread}s')
    if read_va is not None:
        reader = 'dq' if width == 8 else 'dd'
        commands.append(f'{reader} 0x{read_va:016X} L4')
    commands.append('q')
    script = Path(trace).parent / f'{trace.stem}-terminal.txt'
    script.write_text('\n'.join(commands) + '\n', encoding='ascii')

    completed = subprocess.run(
        [cdb, '-z', str(trace), '-cf', str(script)],
        capture_output=True, text=True, errors='replace')
    output = completed.stdout + completed.stderr
    result: dict = {'cdb_exit_code': completed.returncode}
    for line in output.splitlines():
        if 'Time Travel Position:' in line:
            raw = line.split('Time Travel Position:')[1].strip()
            result['position'] = raw
            # `245384B:0 [Unindexed] Index` -- the SEQUENCE is the hex before the
            # colon, and that is what `TTD.Memory` compares against `TimeStart`.
            # Reporting the whole decorated string made `--terminal-sequence`
            # unusable, because the query needs the number.
            match = re.match(r'([0-9A-Fa-f]+):([0-9A-Fa-f]+)', raw)
            if match:
                result['sequence'] = int(match.group(1), 16)
                result['steps'] = int(match.group(2), 16)
    if read_va is not None:
        # cdb prints `00000000`001d4060  ???????? ????????` when the page was never
        # recorded, and `00000000`001d4060  fe000104 fe000108` when it was. Only the
        # second is a value; the first is UNREADABLE and must not read as zero.
        pattern = re.compile(
            rf'^[0-9a-f]{{8}}`{read_va:08x}\s+([0-9a-f?]{{8}})', re.MULTILINE)
        match = pattern.search(output)
        if match:
            token = match.group(1)
            if '?' in token:
                result['value_at_end'] = None
                result['value_at_end_reason'] = (
                    'the slot reads as ????????, so TTD recorded no memory there; '
                    'this is UNREADABLE, not zero')
            else:
                result['value_at_end'] = '0x' + token.upper()
        else:
            result['value_at_end'] = None
            result['value_at_end_reason'] = 'the read did not appear in cdb output'
    result['output_tail'] = output.strip().splitlines()[-6:]
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--run-log', type=Path,
                        help='the run log beside the trace (default: sibling)')
    parser.add_argument('--read-va', type=lambda s: int(s, 0), default=None,
                        help='read this guest VA at the end position')
    parser.add_argument('--width', type=int, default=4, choices=(4, 8))
    parser.add_argument('--out', type=Path)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    run_log = args.run_log or (args.trace.parent / 'jsrf_run.log')
    if not run_log.is_file():
        print(f'no run log at {run_log}; pass --run-log', file=sys.stderr)
        return 2

    cdb = resolve_cdb()
    if not cdb:
        print('cdb.exe not found', file=sys.stderr)
        return 2

    evidence = log_evidence(run_log)
    # The faulting thread, from the exception when there is one.
    thread = None
    if evidence['exceptions']:
        thread = str(evidence['exceptions'][-1]['tid'])
    read_va = args.read_va
    if read_va is None and evidence['last_kernel_call']:
        read_va = int(evidence['last_kernel_call']['slot_va'], 16)

    end = trace_end(cdb, args.trace, thread, read_va, args.width)

    record: dict = {
        'tool': 'jsrf-ttd-terminal/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'trace': str(args.trace),
        'run_log': str(run_log),
        'log_evidence': evidence,
        'trace_end': end,
        'read_va': f'0x{read_va:08X}' if read_va is not None else None,
        'note': ('Read-only: `!tt` seeks, `~Ns` selects a thread, `dq`/`dd` read. '
                 'No step, no execute, no write.'),
    }

    # What the query needs, in the form it accepts.
    if evidence['exceptions']:
        record['terminal_kind'] = (
            f"the runtime's fatal invalid-indirect-call code "
            f"{evidence['exceptions'][-1]['code']}")
    elif evidence['invalid_icalls']:
        record['terminal_kind'] = 'an invalid indirect call'
    else:
        record['terminal_kind'] = None
    record['terminal_sequence_argument'] = end.get('sequence')
    record['value_at_p_argument'] = end.get('value_at_end')
    if end.get('value_at_end') is None and end.get('value_at_end_reason'):
        record['value_at_p_unavailable'] = end['value_at_end_reason']

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'terminal evidence  trace={args.trace.name}')
        print(f'  run log        : {run_log.name}')
        print(f"  invalid ICALLs : {len(evidence['invalid_icalls'])}")
        for entry in evidence['invalid_icalls'][:4]:
            print(f"    target={entry['target']} return={entry['return_address']} "
                  f"tid={entry['tid']}")
        for entry in evidence['exceptions'][:4]:
            print(f"  exception      : tid={entry['tid']} code={entry['code']}")
        last = evidence['last_kernel_call']
        if last:
            print(f"  last kernel    : #{last['n']} ordinal {last['ordinal']} "
                  f"(slot {last['slot']} = {last['slot_va']}) ret={last['return_address']}")
        print(f"  trace end      : {end.get('position', 'UNKNOWN')}")
        if record['read_va']:
            print(f"  read at end    : {record['read_va']} -> "
                  f"{end.get('value_at_end', 'UNREADABLE')}")
        print()
        print()
        if record.get('value_at_p_unavailable'):
            print(f"  value at P    : UNAVAILABLE -- "
                  f"{record['value_at_p_unavailable']}")
        print(f"  --terminal-sequence {record['terminal_sequence_argument']} "
              f"--terminal-kind \"{record['terminal_kind']}\""
              + (f" --value-at-p {record['value_at_p_argument']}"
                 if record['value_at_p_argument'] else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
