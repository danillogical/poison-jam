"""`tools/ttd/ttd-census.py`: what writes a guest address range, and from where.

**Why this exists, and why it is not `ttd-query.py`.** The C1 investigation needed a
question `ttd-query.py` could not answer: *"what writes this whole range, and which
module does each writer live in"*. Asking it slot by slot returns one slot's history
and hides that the table is written **wholesale** by a bulk fill — which is exactly
what happened: 480 byte-at-a-time `memset` writes plus 120 installs, and the earlier
per-slot query could not see the shape.

It answers three things a per-slot query cannot:

  * **the census of a range** — every write, classified by the module its `IP` falls
    in, with counts. This is what produced `memset=299923|memcpy=77|exe=0` and the
    table's `fills=480|exe=120`;
  * **the W-d control** — whether writes from outside the process's own modules
    (kernel-mode writes) are reported **at all**, which W11 lists as exclusion (d)
    and which had never been tested;
  * **the alias sweep** — the same range through all 28 mirror views, which is W11's
    W-c control in the construction that actually exercises it.

**Read-only.** Every query is `TTD.Memory(..., "w")` or `"r"`. No step, no execute,
no write.

**A census is a bounded record.** The key is the **module** and the **alias index** —
a finite set derived from the loaded modules and `XBOX_NUM_MIRRORS` — not the identity
of individual events, so the table's size does not grow with run length. Per-event
samples are bounded and labelled as samples.
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

from aliases import aliases, host_base_from_log, ram_size_from_log  # noqa: E402
from ttd_query_helpers import resolve_cdb  # noqa: E402

CENSUS_JS = ROOT / 'tools' / 'ttd' / 'census.js'

MODULE_LINE = re.compile(
    r'ModLoad:\s*([0-9a-f`]+)\s+([0-9a-f`]+)\s+(\S+)$')
CENSUS = re.compile(
    r'^CENSUS\|([^|]+)\|(\d+)\|(\d+)\|(\d+)\|(\d+)\|(\d+)\|(\d+)$')
SAMPLE = re.compile(
    r'^SAMPLE\|(\d+)\|(0x[0-9a-f]+)\|(\d+)\|([0-9a-f]+)\|(0x[0-9a-f]+)\|(0x[0-9a-f]+)$')
ALIAS = re.compile(r'^ALIAS\|(\d+)\|(\d+)\|(\d+)$')
CONTROL = re.compile(r'^WDCONTROL\|([A-Z]+)\|(.*)$')


def loaded_modules(cdb: str, trace: Path) -> dict[str, tuple[int, int]]:
    """Every loaded module's `[base, end)` from the trace's own load events."""
    script = trace.parent / f'{trace.stem}-modules.txt'
    script.write_text('lm\nq\n', encoding='ascii')
    completed = subprocess.run([cdb, '-z', str(trace), '-cf', str(script)],
                               capture_output=True, text=True, errors='replace')
    modules: dict[str, tuple[int, int]] = {}
    for line in (completed.stdout + completed.stderr).splitlines():
        match = MODULE_LINE.search(line)
        if not match:
            continue
        start = int(match.group(1).replace('`', ''), 16)
        end = int(match.group(2).replace('`', ''), 16)
        name = Path(match.group(3)).name
        modules[name] = (start, end)
    return modules


def run(cdb: str, trace: Path, commands: list[str], tag: str) -> str:
    script = trace.parent / f'{trace.stem}-{tag}.txt'
    script.write_text('\n'.join(commands) + '\n', encoding='ascii')
    completed = subprocess.run([cdb, '-z', str(trace), '-cf', str(script)],
                               capture_output=True, text=True, errors='replace')
    return completed.stdout + completed.stderr


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--run-log', type=Path)
    parser.add_argument('--guest-va', type=lambda s: int(s, 0), required=True,
                        help='guest VA of the range start')
    parser.add_argument('--length', type=int, default=0x1E0,
                        help='range length (default: the 120-slot thunk table)')
    parser.add_argument('--max-events', type=int, default=300000)
    parser.add_argument('--samples', type=int, default=6)
    parser.add_argument('--alias-sweep', action='store_true',
                        help="query all 28 mirror views too (W11's W-c control)")
    parser.add_argument('--out', type=Path)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    run_log = args.run_log or (args.trace.parent / 'jsrf_run.log')
    if not run_log.is_file():
        print(f'no run log at {run_log}; pass --run-log', file=sys.stderr)
        return 2
    log_text = run_log.read_text(encoding='utf-8', errors='replace')
    coverage = ram_size_from_log(log_text)
    host_base = host_base_from_log(log_text)
    if coverage is None or host_base is None:
        print('the run log lacks the mirror/geometry lines, so the alias set cannot '
              'be derived from the run itself', file=sys.stderr)
        return 3
    mapped, expected, ram_bytes = coverage

    cdb = resolve_cdb()
    if not cdb:
        print('cdb.exe not found', file=sys.stderr)
        return 2

    modules = loaded_modules(cdb, args.trace)
    lo = host_base + args.guest_va
    hi = lo + args.length

    # **Hex, explicitly.** `f'{base}'` renders a Python int in DECIMAL, and
    # `census.js`'s `_parseHex` parses what it is given as hex -- so the first version
    # sent `140713372024832` where the module began at `0x7FFA628E0000`, every range was
    # wrong, and every write classified UNKNOWN. Measured, and it is the same class of
    # defect T1 hit twice: a number whose base is assumed rather than stated.
    module_list = ','.join(f'{name}=0x{base:X}:0x{end:X}'
                           for name, (base, end) in sorted(modules.items()))
    commands = [f'.scriptload {CENSUS_JS}',
                f'dx @$scriptContents.census("{module_list}", 0x{lo:X}, 0x{hi:X}, '
                f'{args.max_events}, {args.samples})']
    if args.alias_sweep:
        alias_list = aliases(args.guest_va, ram_bytes, host_base)
        # The mirrors only; index 0 is the canonical range already censused.
        commands.append(
            'dx @$scriptContents.aliasSweep("'
            + ','.join(f'0x{a:X}' for a in alias_list[1:])
            + f'", {args.length})')
        # The control CLASSIFIES by module, so it needs the module list and the set
        # the Python side considers "inside the process".
        process_modules = ','.join(
            name for name in modules
            if name.lower().startswith(('jsrf_recomp', 'vcruntime', 'msvcp', 'ucrtbase')))
        commands.append(
            f'dx @$scriptContents.kernelWriteControl("{module_list}", 0x{lo:X}, '
            f'0x{hi:X}, {args.max_events}, "{process_modules}")')
    commands.append('q')
    output = run(cdb, args.trace, commands, 'census')

    record: dict = {
        'tool': 'jsrf-ttd-census/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'trace': str(args.trace),
        'guest_va': f'0x{args.guest_va:08X}',
        'host_range': [f'0x{lo:016X}', f'0x{hi:016X}'],
        'host_base': f'0x{host_base:016X}',
        'ram_bytes': ram_bytes,
        'mirrors_mapped': mapped,
        'mirrors_expected': expected,
        'modules': {name: [f'0x{b:016X}', f'0x{e:016X}']
                    for name, (b, e) in sorted(modules.items())},
        'census': [],
        'samples': [],
        'aliases': [],
        'note': ('Read-only. The census key is the MODULE, a finite set derived from '
                 'the loaded modules, so the record does not grow with run length; '
                 'per-event rows are bounded samples.'),
    }
    for line in output.splitlines():
        line = line.strip()
        match = CENSUS.match(line)
        if match:
            module, total, size1, size4, other, first, last = match.groups()
            record['census'].append({
                'module': module, 'writes': int(total),
                'size1': int(size1), 'size4': int(size4), 'other_size': int(other),
                'first_sequence': int(first), 'last_sequence': int(last),
            })
            continue
        match = SAMPLE.match(line)
        if match:
            seq, addr, size, value, over, ip = match.groups()
            record['samples'].append({
                'sequence': int(seq), 'address': addr, 'size': int(size),
                'value': value, 'overwritten': over, 'ip': ip,
            })
            continue
        match = ALIAS.match(line)
        if match:
            index, writes, after = match.groups()
            record['aliases'].append({'index': int(index), 'writes': int(writes),
                                      'after_first_sequence': int(after)})
            continue
        match = CONTROL.match(line)
        if match:
            # The regex captures the TAG and the remainder, so the remainder is split
            # here. Measured: the first version unpacked four values from two groups
            # (`ValueError: not enough values to unpack`), so the whole `--alias-sweep`
            # path crashed before it printed anything -- and it was committed without
            # ever being run, which is exactly why the sweep is exercised now.
            tag, rest = match.groups()
            fields = rest.split('|')
            if tag == 'RESULT':
                if len(fields) < 3:
                    continue
                scanned, outside, verdict = fields[0], fields[1], fields[2]
                record['kernel_write_control'] = {
                    'scanned': int(scanned),
                    'outside_process_modules': int(outside),
                    'verdict': ('TTD DOES NOT REPORT KERNEL-MODE WRITES'
                                if verdict == 'NOT_REPORTED'
                                else 'kernel-mode writes ARE reported'),
                    'process_modules': process_modules,
                }
            elif tag == 'OUTSIDE' and len(fields) >= 3:
                record.setdefault('kernel_write_outside_samples', []).append({
                    'ip': fields[0], 'module': fields[1],
                    'sequence': int(fields[2]),
                })

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'write census  {record["guest_va"]} + {args.length} '
              f'(host {record["host_range"][0]}..{record["host_range"][1]})')
        print(f'  modules loaded : {len(modules)}')
        print(f'  census by module:')
        for entry in record['census']:
            print(f"    {entry['module']:<28} {entry['writes']:>8} writes "
                  f"(size1={entry['size1']} size4={entry['size4']} "
                  f"other={entry['other_size']}) "
                  f"seq {entry['first_sequence']}..{entry['last_sequence']}")
        if record['aliases']:
            written = [a for a in record['aliases'] if a['writes']]
            print(f"  alias sweep    : {len(record['aliases'])} mirror(s) queried, "
                  f"{len(written)} written")
            for entry in written[:8]:
                print(f"    mirror {entry['index']}: {entry['writes']} write(s)")
        control = record.get('kernel_write_control')
        if control:
            print(f"  W-d control    : {control['scanned']} scanned, "
                  f"{control['outside_process_modules']} from outside the process's "
                  f"modules -> {control['verdict']}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
