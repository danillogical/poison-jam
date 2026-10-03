"""`scripts/read-host-symbol.py`: read a recompiled executable's global from a run's dump.

    python -X utf8 scripts/read-host-symbol.py <run-dir> <symbol> [--length N] [--as u32|u64|hex]
    python -X utf8 scripts/read-host-symbol.py <run-dir> --alias-hits

The symbol is located through the run's archived `jsrf_recomp.map` and the dump's
module list (scripts/jsrf_dump.py). `--alias-hits` lists every folded alias entry the
guest called, with its owner and count, from g_recomp_alias_icall_map and
g_recomp_alias_icall_hits -- the counters behind the `[ALIAS-ICALL]` log line, which
prints only the first eight distinct aliases.

Needs a dump written with MiniDumpWithDataSegs (collector after game 2026-10-02);
older dumps lack the executable's data pages, and the read says so rather than
guessing.

Exit 0 on success, 1 when the symbol or its memory is not in the dump.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from jsrf_dump import CaptureError, DumpMemory  # noqa: E402


def alias_hits(dump: DumpMemory, map_path: Path) -> list[tuple[int, int, int]]:
    """(alias VA, owner VA, calls) for every alias entry called at least once."""
    count, = struct.unpack('<I', dump.host_symbol(map_path, 'g_recomp_alias_icall_entries', 4))
    pairs = dump.host_symbol(map_path, 'g_recomp_alias_icall_map', count * 8)
    hits = dump.host_symbol(map_path, 'g_recomp_alias_icall_hits', count * 8)
    out = []
    for i in range(count):
        alias, owner = struct.unpack_from('<II', pairs, i * 8)
        calls, = struct.unpack_from('<Q', hits, i * 8)
        if calls:
            out.append((alias, owner, calls))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('symbol', nargs='?')
    parser.add_argument('--length', type=int, default=4)
    parser.add_argument('--as', dest='fmt', choices=('u32', 'u64', 'hex'), default='hex')
    parser.add_argument('--map', type=Path, help='linker map (default: <run-dir>/jsrf_recomp.map)')
    parser.add_argument('--alias-hits', action='store_true')
    args = parser.parse_args()
    if not args.alias_hits and not args.symbol:
        parser.error('give a symbol or --alias-hits')
    map_path = args.map or args.run_dir / 'jsrf_recomp.map'

    try:
        with DumpMemory(args.run_dir) as dump:
            if args.alias_hits:
                rows = alias_hits(dump, map_path)
                print(f'{len(rows)} alias entr{"y" if len(rows) == 1 else "ies"} called')
                for alias, owner, calls in rows:
                    print(f'  alias 0x{alias:08X} -> owner 0x{owner:08X}  calls {calls}')
                return 0
            data = dump.host_symbol(map_path, args.symbol, args.length)
    except (CaptureError, OSError, struct.error) as exc:
        print(f'read-host-symbol: {exc}', file=sys.stderr)
        return 1
    if args.fmt == 'u32':
        print(' '.join(str(v) for v in struct.unpack(f'<{len(data) // 4}I', data[:len(data) // 4 * 4])))
    elif args.fmt == 'u64':
        print(' '.join(str(v) for v in struct.unpack(f'<{len(data) // 8}Q', data[:len(data) // 8 * 8])))
    else:
        print(data.hex())
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
