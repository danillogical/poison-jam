"""`scripts/xemu-diff.py`: compare a checkpoint between xemu and a recomp run.

Plan T3's last piece. Two controls decide whether this tool is worth anything:

  * **the diff of a checkpoint against itself is empty**; and
  * **a seeded one-byte change is found**.

Both are in `--selftest`, which runs them against the same code path as a real
comparison. A comparator that reports "no differences" because it compared nothing
passes the first control and fails the second; one that always reports differences
passes the second and fails the first. Running both is what makes either meaningful.

**What is compared, and what that means.** The two sides are not the same kind of
artifact:

  * xemu is the **oracle** -- real emulated hardware, reached through
    `scripts/xemu-gdbstub.py`, which is read-only by construction.
  * a recomp run is a **frozen capture** -- an archived run's dump, read through
    `scripts/jsrf_dump.py`.

The comparison is therefore over **bytes at a guest VA**, which is the only thing
both sides can honestly supply. It does not compare control flow, timing, or
device state, and a match does not establish that the recompilation is correct --
only that these bytes agree at this address. Stating that is the difference
between an oracle and a rubber stamp.

**Guest VAs, never host addresses.** xemu's gdbstub addresses are guest physical
addresses; a recomp dump's addresses are guest VAs. Both sides are asked for the
same guest VA, and the tool refuses a comparison it cannot make rather than
shifting one side to fit.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GDBSTUB = ROOT / 'scripts' / 'xemu-gdbstub.py'
INSPECT = ROOT / 'scripts' / 'inspect-jsrf.py'


def hexdump_diff(left: bytes, right: bytes, left_name: str, right_name: str,
                 base: int) -> list[dict]:
    """Every differing byte, with both values and the byte offset.

    A diff that reported only "different" would not let a reader see whether the
    difference is one byte or a shifted block -- and a shifted block is a
    different finding from a changed one.
    """
    differences = []
    for offset in range(max(len(left), len(right))):
        a = left[offset] if offset < len(left) else None
        b = right[offset] if offset < len(right) else None
        if a != b:
            differences.append({
                'offset': offset,
                'address': f'0x{base + offset:08X}',
                left_name: None if a is None else f'{a:02x}',
                right_name: None if b is None else f'{b:02x}',
            })
    return differences


def read_xemu(port: int, address: int, length: int) -> tuple[bytes | None, str]:
    """The oracle side, through the read-only gdbstub client."""
    completed = subprocess.run(
        [sys.executable, '-X', 'utf8', str(GDBSTUB), '--port', str(port),
         '--read', f'{hex(address)}:{length}', '--json'],
        capture_output=True, text=True)
    if completed.returncode != 0:
        return None, completed.stderr.strip() or f'exit {completed.returncode}'
    try:
        record = json.loads(completed.stdout)
    except ValueError as error:
        return None, f'the gdbstub output was not JSON: {error}'
    for entry in record.get('reads', []):
        if entry.get('address', '').lower() == f'0x{address:08X}'.lower():
            if 'error' in entry:
                return None, entry['error']
            return bytes.fromhex(entry['hex']), ''
    return None, 'the gdbstub read did not appear in its own output'


def read_recomp(run_dir: Path, address: int, length: int) -> tuple[bytes | None, str]:
    """The recomp side, through the project's own dump reader."""
    completed = subprocess.run(
        [sys.executable, '-X', 'utf8', str(INSPECT), 'memory', str(run_dir),
         hex(address), str(length)],
        capture_output=True, text=True)
    if completed.returncode != 0:
        return None, completed.stderr.strip() or f'exit {completed.returncode}'
    data = bytearray()
    for line in completed.stdout.splitlines():
        line = line.strip()
        if not line or ':' not in line:
            continue
        _prefix, _, rest = line.partition(':')
        for word in rest.split():
            try:
                value = int(word, 16)
            except ValueError:
                continue
            data.extend(value.to_bytes(4, 'little'))
    if not data:
        return None, 'the dump reader produced no words'
    return bytes(data[:length]), ''


def selftest() -> int:
    """T3's two controls, on the same comparison path a real run uses."""
    print('xemu-diff selftest')
    base = 0x001C3F60
    sample = bytes.fromhex('960602804d6901802c34028028780180')

    same = hexdump_diff(sample, sample, 'xemu', 'recomp', base)
    print(f'  control 1 (self vs self): {len(same)} difference(s) '
          f'-> {"PASS" if not same else "FAIL"}')

    seeded = bytearray(sample)
    seeded[5] ^= 0x01
    found = hexdump_diff(sample, bytes(seeded), 'xemu', 'recomp', base)
    ok_seed = (len(found) == 1 and found[0]['offset'] == 5
               and found[0]['address'] == f'0x{base + 5:08X}')
    print(f'  control 2 (one-byte seed): {len(found)} difference(s) at '
          f'{found[0]["address"] if found else "none"} '
          f'-> {"PASS" if ok_seed else "FAIL"}')

    # A control that the comparator can see a LENGTH difference, which is how a
    # truncated read would otherwise compare equal on the shared prefix.
    short = hexdump_diff(sample, sample[:4], 'xemu', 'recomp', base)
    ok_short = len(short) == len(sample) - 4
    print(f'  control 3 (truncation): {len(short)} difference(s) '
          f'-> {"PASS" if ok_short else "FAIL"}')

    passed = not same and ok_seed and ok_short
    print(f'  selftest: {"PASS" if passed else "FAIL"}')
    return 0 if passed else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--xemu-port', type=int, default=1234)
    parser.add_argument('--recomp-run', type=Path,
                        help='archived recomp run directory')
    parser.add_argument('--address', type=lambda s: int(s, 0),
                        help='guest VA to compare')
    parser.add_argument('--length', type=int, default=64)
    parser.add_argument('--out', type=Path, help='write the comparison JSON here')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--selftest', action='store_true',
                        help="run T3's controls and exit")
    args = parser.parse_args()

    if args.selftest:
        return selftest()
    if args.recomp_run is None or args.address is None:
        parser.error('pass --recomp-run and --address, or --selftest')

    record: dict = {
        'tool': 'jsrf-xemu-diff/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'guest_va': f'0x{args.address:08X}',
        'length': args.length,
        'xemu_endpoint': f'127.0.0.1:{args.xemu_port}',
        'recomp_run': str(args.recomp_run),
        'claim_limits': (
            'Compares bytes at one guest VA. A match does not establish that the '
            'recompilation is correct, and a mismatch does not by itself name a '
            'cause; the diff locates the disagreement, nothing more.'),
    }

    xemu_bytes, xemu_error = read_xemu(args.xemu_port, args.address, args.length)
    recomp_bytes, recomp_error = read_recomp(args.recomp_run, args.address,
                                             args.length)

    if xemu_bytes is None or recomp_bytes is None:
        record['result'] = 'UNKNOWN'
        record['xemu_error'] = xemu_error or None
        record['recomp_error'] = recomp_error or None
        record['reason'] = ('one side could not be read; a comparison that could '
                            'not be made is not an empty diff')
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                                encoding='utf-8')
        if args.json:
            print(json.dumps(record, indent=2, sort_keys=True))
        else:
            print(f'UNKNOWN: {record["reason"]}', file=sys.stderr)
            if xemu_error:
                print(f'  xemu:   {xemu_error}', file=sys.stderr)
            if recomp_error:
                print(f'  recomp: {recomp_error}', file=sys.stderr)
        return 3

    record['xemu_sha256'] = hashlib.sha256(xemu_bytes).hexdigest()
    record['recomp_sha256'] = hashlib.sha256(recomp_bytes).hexdigest()
    record['xemu_hex'] = xemu_bytes.hex()
    record['recomp_hex'] = recomp_bytes.hex()
    differences = hexdump_diff(xemu_bytes, recomp_bytes, 'xemu', 'recomp',
                               args.address)
    record['differences'] = differences
    record['difference_count'] = len(differences)
    record['result'] = 'MATCH' if not differences else 'DIFFER'

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'xemu-diff at guest {record["guest_va"]} ({args.length} bytes)')
        print(f'  xemu   : {record["xemu_sha256"][:16]}')
        print(f'  recomp : {record["recomp_sha256"][:16]}')
        if not differences:
            print('  RESULT : MATCH (empty diff)')
        else:
            print(f'  RESULT : DIFFER, {len(differences)} byte(s)')
            for entry in differences[:16]:
                print(f'    {entry["address"]}  xemu={entry["xemu"]}  '
                      f'recomp={entry["recomp"]}')
            if len(differences) > 16:
                print(f'    ... {len(differences) - 16} more')
    return 0 if not differences else 1


if __name__ == '__main__':
    sys.exit(main())
