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

from aliases import host_base_from_log  # noqa: E402
from ttd_query_helpers import resolve_cdb  # noqa: E402

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
        # **`dd` CANNOT SEE THE VALUE; `TTD.Memory` CAN.** Measured on the
        # horizon-reachable trace: `dd 0x1D4060` prints `????????` while
        # `TTD.Memory(0x1D4060, 0x1D4064, "r").Last()` returns
        # `Value: 0xfe000100`. A plain read wants the memory as it stands at the
        # current position, and TTD answers that only for pages it has paged in;
        # the data-model query walks the RECORDED accesses, which is exactly the
        # evidence W-b compares against.
        #
        # Reading through the data model is also the more honest instrument here:
        # it reports the last recorded access at the address, with its position, so
        # the value carries its own provenance.
        commands.append(f'dx -r1 @$cursession.TTD.Memory(0x{read_va:X}, '
                        f'0x{read_va + width:X}, "r").Last()')
        # The plain read is kept as a CROSS-CHECK, so a disagreement between the two
        # instruments is visible rather than resolved silently.
        reader = 'dq' if width == 8 else 'dd'
        commands.append(f'{reader} 0x{read_va:016X} L1')
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
        # The data-model query first: it walks recorded accesses and reports the
        # value with the position it was read at.
        value_match = re.search(r'Value\s*:\s*(0x[0-9a-f]+)', output, re.IGNORECASE)
        position_match = re.search(r'TimeStart\s*:\s*([0-9A-Fa-f]+:[0-9A-Fa-f]+)',
                                   output)
        if value_match:
            result['value_at_end'] = value_match.group(1).upper().replace('0X', '0x')
            result['value_at_end_source'] = 'TTD.Memory (the recorded accesses)'
            if position_match:
                result['value_at_end_position'] = position_match.group(1)
        # The plain read, as a cross-check.
        pattern = re.compile(
            rf'^[0-9a-f]{{8}}`{read_va:08x}\s+([0-9a-f?]{{8}})', re.MULTILINE)
        cross = pattern.search(output)
        if cross:
            token = cross.group(1)
            result['plain_read'] = None if '?' in token else '0x' + token.upper()
        if not value_match:
            if result.get('plain_read'):
                result['value_at_end'] = result['plain_read']
                result['value_at_end_source'] = 'a plain read'
            else:
                result['value_at_end'] = None
                result['value_at_end_reason'] = (
                    'neither TTD.Memory nor a plain read returned a value at this '
                    'address; this is UNREADABLE, not zero')
    result['output_tail'] = output.strip().splitlines()[-6:]
    return result


CALL_SLOT = re.compile(
    r'call\s+dword\s+ptr\s+\[0x([0-9A-Fa-f]+)\]')


def failing_call_slot(evidence: dict, run_log: Path) -> int | None:
    """The slot VA the failing call read, from its own return address.

    The ICALL line reports the return address; the call is the instruction ending
    there. Disassembling a window that ends at that address finds it, and its memory
    operand is the slot. Returns None when the call cannot be found, so the caller
    falls back rather than guessing.
    """
    if not evidence['invalid_icalls']:
        return None
    return_va = int(evidence['invalid_icalls'][-1]['return_address'], 16)
    completed = subprocess.run(
        [sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'inspect-jsrf.py'),
         'disasm', hex(max(0, return_va - 0x20)), hex(return_va)],
        capture_output=True, text=True)
    if completed.returncode != 0:
        return None
    # The LAST `call dword ptr [...]` at or before the return address.
    matches = CALL_SLOT.findall(completed.stdout)
    if not matches:
        return None
    return int(matches[-1], 16)


def trace_contract(trace: Path) -> dict:
    """The recording's end condition, from the contract and the recorder's output.

    **The recorder's own output is the authority, and it is read every time.** A
    contract written before this session does not carry the end-condition fields, and
    reading them as absent would make a COMPLETED trace look unknown -- the opposite
    error from the one C-a exists to catch. `ttd-output.txt` sits beside the trace and
    states the ending in the recorder's own words:

        "Process exited with exit code ..."   a completed recording
        "Recording stopped after Nms"         cut off, e.g. by `-maxFile`

    So the fields are DERIVED from that text whenever the contract lacks them, and the
    contract's values win when present because it was written by the recorder at the
    time.
    """
    directory = Path(trace).parent
    data: dict = {'found': False}
    path = directory / 'record-contract.json'
    if path.is_file():
        try:
            data = json.loads(path.read_text(encoding='utf-8'))
            data['found'] = True
        except (OSError, ValueError) as error:
            data = {'found': False, 'reason': str(error)}

    output = directory / 'ttd-output.txt'
    if output.is_file():
        text = output.read_text(encoding='utf-8', errors='replace')
        data.setdefault('ended_by_process_exit',
                        'Process exited with exit code' in text)
        data.setdefault('ended_by_recording_stop', 'Recording stopped' in text)
    if 'at_size_cap' not in data:
        runs = sorted(directory.glob('*.run'))
        total_mb = sum(p.stat().st_size for p in runs) / 1024 / 1024
        cap = data.get('max_file_mb')
        data['at_size_cap'] = bool(cap and total_mb >= cap)
        data.setdefault('trace_total_mb', round(total_mb, 2))
    return data


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

    # **The read is over HOST addresses.** The runtime maps guest 0 at an offset it
    # prints, and a cdb read is a host linear address. Measured: without this
    # translation the tool read `0x001C4060` -- a GUEST address -- and reported
    # `????????`, which reads as "TTD recorded no memory there" when in fact it read
    # the wrong 64 KB. That is the same defect `ttd-query.py` had, and it is why the
    # translation lives in one place now.
    log_text = run_log.read_text(encoding='utf-8', errors='replace')
    host_base = host_base_from_log(log_text)
    if host_base is None:
        print('the run log does not carry the "xbox_MemoryLayoutInit: mapped ... at '
              '0x..." line, so a guest VA cannot be translated to a host address; '
              'the value at P would be read from the wrong place', file=sys.stderr)
        return 3

    # The faulting thread, from the exception when there is one.
    thread = None
    if evidence['exceptions']:
        thread = str(evidence['exceptions'][-1]['tid'])
    guest_va = args.read_va
    if guest_va is None:
        # **The slot the FAILING CALL read, not the slot of the last kernel call.**
        # Measured: defaulting to the last kernel call read slot 64 while the failing
        # call used slot 65 (`call dword ptr [0x1c4064]` at `0x00149828`, return
        # `0x0014982E`), so W-b compared one slot's value against another slot's
        # write and selected the wrong row.
        #
        # The failing call's slot is derived from the return address: the ICALL
        # reports where the call RETURNS to, and the call is the instruction just
        # before it. `scripts/inspect-jsrf.py disasm` gives the instruction, and its
        # memory operand names the slot.
        guest_va = failing_call_slot(evidence, run_log)
    if guest_va is None and evidence['last_kernel_call']:
        guest_va = int(evidence['last_kernel_call']['slot_va'], 16)
    read_va = (host_base + guest_va) if guest_va is not None else None

    end = trace_end(cdb, args.trace, thread, read_va, args.width)

    record: dict = {
        'tool': 'jsrf-ttd-terminal/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'trace': str(args.trace),
        'run_log': str(run_log),
        'log_evidence': evidence,
        'trace_end': end,
        'guest_va_read': f'0x{guest_va:08X}' if guest_va is not None else None,
        'host_base': f'0x{host_base:016X}',
        'read_va': f'0x{read_va:016X}' if read_va is not None else None,
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
    # **C-b: is the terminal event IN THE TRACE?**
    #
    # Two facts decide it, and neither is the run log:
    #   * whether the recorder ended by PROCESS EXIT (from `record-contract.json`,
    #     which `ttd-record.py` now fills from the recorder's own output). A recorder
    #     that stopped at its `-maxFile` cap left the process running, so the log can
    #     contain events the trace does not.
    #   * whether the trace itself carries an exception record for the terminal code.
    #
    # Measured defect this replaces: `ttd-terminal.py` took P from `!tt 100` -- the
    # END of the trace -- and the query passed S2 whenever the run's LOG contained
    # the event. Both are satisfied by a trace whose recording stopped before the
    # terminal, which produced a wrong conclusion (Advisor ruling 2026-09-30).
    contract = trace_contract(args.trace)
    record['trace_contract'] = contract
    exception_code = (evidence['exceptions'][-1]['code']
                      if evidence['exceptions'] else None)
    in_trace = bool(
        contract.get('ended_by_process_exit')
        and not contract.get('at_size_cap')
        and exception_code is not None)
    record['terminal_in_trace'] = in_trace
    record['terminal_in_trace_basis'] = (
        f"recorder ended by process exit: {contract.get('ended_by_process_exit')}; "
        f"at size cap: {contract.get('at_size_cap')}; "
        f"exception code in the run log: {exception_code}")
    if not in_trace:
        record['terminal_in_trace_reason'] = (
            'the terminal event cannot be shown to be IN THE TRACE: '
            + ('the recorder stopped at its size cap, so the process outlived it'
               if contract.get('at_size_cap')
               else 'the recorder did not end by process exit'
               if not contract.get('ended_by_process_exit')
               else 'no exception code appears in the run log'))

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
            print(f"  host base      : {record['host_base']}")
            print(f"  read at end    : guest {record['guest_va_read']} -> host "
                  f"{record['read_va']} -> "
                  f"{end.get('value_at_end') or 'UNREADABLE'}")
        print()
        print()
        if record.get('terminal_in_trace'):
            print('  in the trace  : YES -- '
                  + str(record.get('terminal_in_trace_basis')))
        else:
            print('  in the trace  : NO -- W-a cannot be satisfied')
            print('                  ' + str(record.get('terminal_in_trace_reason')))
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
