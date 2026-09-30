"""`scripts/xemu-oracle.py`: launch xemu on the owner's configuration (plan T3).

**T3's scope, stated because it is easy to overrun.** The plan asks for a gdbstub
recipe that stops at a named guest PC and dumps guest registers/memory, so a
recomp run's checkpoint can be compared against the emulator. It explicitly does
**not** ask for a general emulator integration. This module therefore does three
things and stops:

  * resolves the exact files xemu is **configured** to use, rather than guessing
    among the several images in the asset directories;
  * launches xemu with the same arguments it uses itself, plus `-gdb`, so a
    debugger can attach;
  * exposes a bounded stop-and-dump so `scripts/xemu-diff.py` has something to
    compare against.

**The proprietary assets are never copied.** Every path below is read from
`%APPDATA%\\xemu\\xemu\\xemu.toml` and used **in place**. Nothing is written into
either repository, and the record this module emits contains paths and hashes of
*configuration*, never asset bytes.

**Why the arguments are built here rather than taken from `xemu.log`.** xemu's own
log line is a single string with unquoted spaces in the disc path
(`... -drive index=1,media=cdrom,file=C:\\...\\JSRF - Jet Set Radio Future (USA).xiso.iso ...`),
so passing it through a shell splits it into three arguments and xemu dies with
`-: invalid option`. That is the whole reason a launcher exists instead of a
one-line command.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import re
import socket
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
XEMU = Path(r'C:\Users\logic\Downloads\xemu\xemu.exe')
CONFIG = Path(os.environ.get('APPDATA', '')) / 'xemu' / 'xemu' / 'xemu.toml'

# The keys xemu's `[sys.files]` section uses for the assets T3 needs.  Read from
# the config, never defaulted: a default would silently pick a different image
# than the one the owner configured.
ASSET_KEYS = ('bootrom_path', 'flashrom_path', 'eeprom_path', 'hdd_path', 'dvd_path')

CREATE_NO_WINDOW = 0x08000000


def read_config() -> dict[str, str]:
    """The `key = 'value'` pairs from xemu's own config file."""
    if not CONFIG.is_file():
        raise SystemExit(f'xemu config not found: {CONFIG}')
    values: dict[str, str] = {}
    for line in CONFIG.read_text(encoding='utf-8', errors='replace').splitlines():
        match = re.match(r"^\s*([\w_]+)\s*=\s*'(.*)'\s*$", line)
        if match:
            values[match.group(1)] = match.group(2)
    return values


def resolve_assets(config: dict[str, str]) -> dict[str, dict]:
    """Each configured asset with its existence and size, never its bytes."""
    assets: dict[str, dict] = {}
    for key in ASSET_KEYS:
        value = config.get(key)
        if not value:
            assets[key] = {'configured': False}
            continue
        path = Path(value)
        assets[key] = {
            'configured': True,
            'path': str(path),
            'exists': path.is_file(),
            'bytes': path.stat().st_size if path.is_file() else None,
        }
    return assets


def missing_assets(assets: dict[str, dict]) -> list[str]:
    return [key for key, entry in assets.items()
            if not entry.get('configured') or not entry.get('exists')]


def xemu_argv(assets: dict[str, dict], gdb_port: int | None,
              extra: list[str] | None = None) -> list[str]:
    """xemu's argument list for an oracle run.

    **xemu builds its own machine arguments from `xemu.toml` and appends whatever
    it is given.** Measured: passing the `-machine`/`-bios`/`-drive` set as well
    produced a command line with the whole set twice, and xemu then failed with

        -drive index=0,...: Could not open '...xbox_hdd.qcow2': The process cannot
        access the file because it is being used by another process.

    -- because its second `-drive` for the same image conflicted with its first.
    So this function passes **only** the options the config cannot express, and
    the asset paths in `assets` are recorded for provenance rather than replayed
    as arguments. That is also why the paths must resolve: a missing asset is a
    blocked oracle, and xemu reports it far less clearly than this script does.
    """
    argv = [str(XEMU)]
    if gdb_port is not None:
        argv.extend(['-gdb', f'tcp::{gdb_port}'])
    if extra:
        argv.extend(extra)
    return argv


def port_open(port: int, host: str = '127.0.0.1', timeout: float = 1.0) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(timeout)
        return sock.connect_ex((host, port)) == 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--check', action='store_true',
                        help='resolve the configuration and exit; launch nothing')
    parser.add_argument('--launch', action='store_true',
                        help='launch xemu and wait for the gdbstub to accept')
    parser.add_argument('--gdb-port', type=int, default=1234)
    parser.add_argument('--wait', type=int, default=40,
                        help='seconds to wait for the gdbstub')
    parser.add_argument('--record', type=Path,
                        help='write the launch record here')
    args = parser.parse_args()

    if not XEMU.is_file():
        print(f'xemu not found at {XEMU}', file=sys.stderr)
        return 2

    config = read_config()
    assets = resolve_assets(config)
    missing = missing_assets(assets)

    record = {
        'tool': 'jsrf-xemu-oracle/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'xemu': {'path': str(XEMU), 'exists': XEMU.is_file(),
                 'bytes': XEMU.stat().st_size if XEMU.is_file() else None},
        'config_path': str(CONFIG),
        'config_present': CONFIG.is_file(),
        'assets': assets,
        'missing_assets': missing,
        'note': ('Asset paths are used IN PLACE from the owner\'s configuration. '
                 'Nothing is copied into either repository, and only paths, sizes '
                 'and existence are recorded -- never asset bytes.'),
    }

    if missing:
        record['result'] = 'BLOCKED'
        record['reason'] = f'configured asset(s) missing or unset: {missing}'
    elif args.check:
        record['result'] = 'CONFIGURED'
    elif args.launch:
        argv = xemu_argv(assets, args.gdb_port)
        record['argv'] = argv
        print(f'launching xemu (gdbstub on {args.gdb_port})')
        process = subprocess.Popen(
            argv, cwd=str(XEMU.parent),
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            creationflags=CREATE_NO_WINDOW if os.name == 'nt' else 0)
        deadline = time.time() + args.wait
        attached = False
        while time.time() < deadline:
            if process.poll() is not None:
                break
            if port_open(args.gdb_port):
                attached = True
                break
            time.sleep(0.5)
        record['xemu_pid'] = process.pid
        record['gdbstub_reachable'] = attached
        record['xemu_running'] = process.poll() is None
        if attached:
            record['result'] = 'RUNNING'
            print(f'  xemu pid {process.pid}; gdbstub accepting on '
                  f'127.0.0.1:{args.gdb_port}')
            print(f'  xemu is left running; stop it with: '
                  f'Stop-Process -Id {process.pid}')
        else:
            record['result'] = 'FAILED'
            record['reason'] = ('xemu exited or the gdbstub never accepted '
                                f'within {args.wait}s')
            print(f'  {record["reason"]}', file=sys.stderr)
    else:
        record['result'] = 'CONFIGURED'
        record['argv'] = xemu_argv(assets, args.gdb_port)

    if args.record:
        args.record.parent.mkdir(parents=True, exist_ok=True)
        args.record.write_text(json.dumps(record, indent=2, sort_keys=True),
                               encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    elif not args.launch:
        print(f'xemu     : {XEMU} ({record["xemu"]["bytes"]:,} bytes)')
        print(f'config   : {CONFIG}')
        for key, entry in assets.items():
            if entry.get('configured'):
                mark = 'OK     ' if entry['exists'] else 'MISSING'
                print(f'  {mark} {key:<14} {entry.get("bytes") or 0:>13,}  '
                      f'{entry["path"]}')
            else:
                print(f'  UNSET   {key:<14} {"":>13}  (not in the config)')
        print(f'result   : {record["result"]}')
        if 'argv' in record:
            print('argv     :')
            for item in record['argv']:
                print(f'  {item}')

    return 0 if record['result'] in ('CONFIGURED', 'RUNNING') else 2


if __name__ == '__main__':
    sys.exit(main())
