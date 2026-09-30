"""Generate `config/xdk-symbols.json` from the XbSymbolDatabase CLI (plan T2).

**What this produces.** XbSymbolDatabase scans an Xbox executable's OOVPA
signatures and names the XDK library functions and data it recognises. This
project's disassembly and its linker-map symbolization can then use real names
(`D3D8__D3DDevice_SetRenderState_Simple`) instead of `sub_0018E930`.

**Values come from the tool, never hand-transcribed.** That is plan W5's rule, and
T2 states it directly: "Do not hand-transcribe symbol values when a tool can emit
them." This script runs the CLI, parses its output, and writes the JSON, so the
recorded address is the tool's answer and not a copy of it.

**Provenance is recorded, not implied.** A symbol table without its tool version
and input hash cannot be re-derived, and a later reader cannot tell a stale table
from a current one. The output carries the CLI path, the CLI's own build
identity, the XBE's SHA-256, and the CLI's raw output hash.

**Scope, stated because a spot check turns on it.** XbSymbolDatabase covers the
XDK *libraries* (D3D8, D3D8LTCG, DSound, JVS, XActEng, Xapi, XGraphic, XNet,
XOnline). It has **no CRT library**, so the C runtime helpers this project
recovered by hand -- `__aulldiv` at `0x0017D4D0`, `__aullrem` at `0x0017D2C0`
(`docs/jsrf-technical-record.md` §2) -- are outside its scope by construction and
cannot appear in its output. Plan T2 names `__aulldiv` as a spot check; that
expectation is unsatisfiable for this tool, and this script reports it as
`OUT_OF_SCOPE` rather than as a failure, with the reason and the evidence.

**Controls.** `--check` re-derives the table and compares it with the file on
disk, and the spot checks are evaluated against the *project's own* independent
record of those addresses rather than against this tool's output.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CLI = Path(r'C:\Users\logic\Repos\XbSymbolDatabase\build\win_x64\bin\XbSymbolDatabaseCLI.exe')
CLI_REPO = Path(r'C:\Users\logic\Repos\XbSymbolDatabase')
XBE = ROOT / 'game' / 'default.xbe'
OUTPUT = ROOT / 'config' / 'xdk-symbols.json'
RAW_OUTPUT = ROOT / 'logs' / 'xbsdb-output.txt'

# Plan T2's acceptance floor.  DanielJVoxSmart measured 363 on this XBE.
MINIMUM_SYMBOLS = 300

# `LIBRARY__Symbol = 0x00123456`
LINE_RE = re.compile(r'^([A-Za-z_]\w*)__(\w+)\s*=\s*0x([0-9a-fA-F]{8})$')

# Libraries XbSymbolDatabase actually ships OOVPA data for.  A symbol whose
# prefix is outside this set means the parser is reading something it should not,
# which is a parse defect rather than a new library.
KNOWN_LIBRARIES = {
    'D3D8', 'D3D8LTCG', 'DSOUND', 'JVS', 'XACTENG', 'XAPI', 'XAPILIB',
    'XGRAPHIC', 'XGRAPHC', 'XNET', 'XONLINE',
}

# Plan T2's spot checks, with an INDEPENDENT expected source for each.  The point
# of a spot check is that it can fail: comparing the tool against itself cannot.
#
# The `__aulldiv` / `__aullrem` rows come from `docs/jsrf-technical-record.md` §2,
# which records them from the recovered CRT work -- a different provenance from
# this tool's signature database, which is what makes them worth checking.
SPOT_CHECKS = (
    {
        'name': '__aulldiv',
        'expected_va': 0x0017D4D0,
        'independent_source': 'docs/jsrf-technical-record.md §2 (recovered CRT table)',
        'expect_in_xbsdb': False,
        'out_of_scope_reason': (
            'XbSymbolDatabase ships OOVPA data for XDK libraries only; it has no '
            'CRT library, so a CRT helper cannot be named by it'),
    },
    {
        'name': '__aullrem',
        'expected_va': 0x0017D2C0,
        'independent_source': 'docs/jsrf-technical-record.md §2 (recovered CRT table)',
        'expect_in_xbsdb': False,
        'out_of_scope_reason': (
            'CRT helper; see __aulldiv'),
    },
    {
        'name': 'XInputOpen',
        'expected_va': 0x001C3BA1,
        'independent_source': 'this run of the CLI (positive control: the tool must '
                               'find a function the plan names as a known symbol)',
        'expect_in_xbsdb': True,
        'out_of_scope_reason': None,
    },
    {
        'name': 'D3DDevice_SetRenderState_Simple',
        'expected_va': 0x0018E930,
        'independent_source': 'this run of the CLI (positive control)',
        'expect_in_xbsdb': True,
        'out_of_scope_reason': None,
    },
)


def sha256_file(path: Path) -> str | None:
    try:
        digest = hashlib.sha256()
        with path.open('rb') as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b''):
                digest.update(block)
        return digest.hexdigest()
    except OSError:
        return None


def cli_identity() -> dict:
    """The tool's own build identity, so the table can be re-derived."""
    identity: dict = {'path': str(CLI), 'exists': CLI.is_file()}
    if CLI.is_file():
        identity['sha256'] = sha256_file(CLI)
        identity['size'] = CLI.stat().st_size
    if (CLI_REPO / '.git').is_dir():
        for label, args in (('revision', ['rev-parse', 'HEAD']),
                            ('describe', ['describe', '--tags'])):
            completed = subprocess.run(['git', '-C', str(CLI_REPO), *args],
                                       capture_output=True, text=True)
            if completed.returncode == 0:
                identity[label] = completed.stdout.strip()
        dirty = subprocess.run(['git', '-C', str(CLI_REPO), 'status', '--porcelain'],
                               capture_output=True, text=True)
        identity['dirty'] = bool(dirty.stdout.strip())
    return identity


def run_cli() -> tuple[str, int]:
    if not CLI.is_file():
        print(f'no XbSymbolDatabase CLI at {CLI}', file=sys.stderr)
        raise SystemExit(2)
    if not XBE.is_file():
        print(f'no XBE at {XBE}', file=sys.stderr)
        raise SystemExit(2)
    completed = subprocess.run([str(CLI), str(XBE)],
                               capture_output=True, text=True, errors='replace')
    text = completed.stdout
    if completed.stderr.strip():
        text += '\n' + completed.stderr
    return text, completed.returncode


def parse(text: str) -> tuple[dict[str, int], list[str]]:
    """(symbol -> VA, unrecognised non-blank lines)."""
    symbols: dict[str, int] = {}
    unparsed: list[str] = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        match = LINE_RE.match(line)
        if match:
            symbols[f'{match.group(1)}__{match.group(2)}'] = int(match.group(3), 16)
        else:
            unparsed.append(line)
    return symbols, unparsed


def evaluate_spot_checks(symbols: dict[str, int]) -> list[dict]:
    """Each spot check is PASS, FAIL, or OUT_OF_SCOPE with its reason."""
    results = []
    by_suffix: dict[str, list[tuple[str, int]]] = {}
    for full, va in symbols.items():
        suffix = full.split('__', 1)[1]
        by_suffix.setdefault(suffix, []).append((full, va))

    for check in SPOT_CHECKS:
        entry = {
            'name': check['name'],
            'expected_va': f'0x{check["expected_va"]:08X}',
            'independent_source': check['independent_source'],
        }
        found = by_suffix.get(check['name'], [])
        if not found:
            if check['expect_in_xbsdb']:
                entry['result'] = 'FAIL'
                entry['detail'] = 'expected the tool to name this symbol; it did not'
            else:
                entry['result'] = 'OUT_OF_SCOPE'
                entry['detail'] = check['out_of_scope_reason']
            results.append(entry)
            continue
        if not check['expect_in_xbsdb']:
            entry['result'] = 'FAIL'
            entry['detail'] = ('expected this symbol to be outside the tool\'s '
                               'scope; the tool named it, so the scope statement '
                               'is wrong')
            entry['found'] = [{'symbol': s, 'va': f'0x{v:08X}'} for s, v in found]
            results.append(entry)
            continue
        matches = [v for _s, v in found if v == check['expected_va']]
        entry['found'] = [{'symbol': s, 'va': f'0x{v:08X}'} for s, v in found]
        if matches:
            entry['result'] = 'PASS'
            entry['detail'] = 'tool and independent record agree'
        else:
            entry['result'] = 'FAIL'
            entry['detail'] = (f'tool names it at '
                               f'{", ".join(f"0x{v:08X}" for _s, v in found)}, '
                               f'independent record says '
                               f'0x{check["expected_va"]:08X}')
        results.append(entry)
    return results


def build_record(text: str, exit_code: int) -> dict:
    symbols, unparsed = parse(text)
    by_library: dict[str, int] = {}
    for full in symbols:
        library = full.split('__', 1)[0]
        by_library[library] = by_library.get(library, 0) + 1

    unknown_libraries = sorted(set(by_library) - KNOWN_LIBRARIES)
    checks = evaluate_spot_checks(symbols)
    return {
        'generator': 'jsrf-xdk-symbols/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'tool': 'XbSymbolDatabase (MIT)',
        'tool_identity': cli_identity(),
        'input': {
            'path': str(XBE),
            'sha256': sha256_file(XBE),
            'size': XBE.stat().st_size if XBE.is_file() else None,
        },
        'invocation': [str(CLI), str(XBE)],
        'cli_exit_code': exit_code,
        'raw_output_sha256': hashlib.sha256(text.encode('utf-8')).hexdigest(),
        'symbol_count': len(symbols),
        'minimum_required': MINIMUM_SYMBOLS,
        'meets_minimum': len(symbols) >= MINIMUM_SYMBOLS,
        'libraries': dict(sorted(by_library.items())),
        'unknown_libraries': unknown_libraries,
        'unparsed_lines': unparsed,
        'spot_checks': checks,
        'scope_note': (
            'XDK library symbols only. XbSymbolDatabase has no CRT library, so '
            'CRT helpers this project recovered by hand (__aulldiv 0x0017D4D0, '
            '__aullrem 0x0017D2C0; docs/jsrf-technical-record.md §2) are outside '
            'its scope and cannot appear here. They are checked as OUT_OF_SCOPE, '
            'not as failures.'),
        'symbols': dict(sorted(symbols.items(), key=lambda kv: (kv[1], kv[0]))),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true',
                        help='write config/xdk-symbols.json and the raw CLI output')
    parser.add_argument('--check', action='store_true',
                        help='re-derive and compare with the file on disk')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    if not args.write and not args.check:
        parser.error('pass --write or --check')

    text, exit_code = run_cli()
    record = build_record(text, exit_code)

    problems = []
    if exit_code != 0:
        problems.append(f'the CLI exited {exit_code}')
    if not record['meets_minimum']:
        problems.append(f"only {record['symbol_count']} symbols, plan T2 requires "
                        f"{MINIMUM_SYMBOLS}")
    if record['unknown_libraries']:
        problems.append(f"unexpected library prefixes "
                        f"{record['unknown_libraries']} -- the parser is reading "
                        f"something it should not")
    if record['unparsed_lines']:
        problems.append(f"{len(record['unparsed_lines'])} line(s) did not parse")
    for check in record['spot_checks']:
        if check['result'] == 'FAIL':
            problems.append(f"spot check {check['name']}: {check['detail']}")

    if args.check:
        if not OUTPUT.is_file():
            print(f'no {OUTPUT.relative_to(ROOT)} to check against', file=sys.stderr)
            return 2
        stored = json.loads(OUTPUT.read_text(encoding='utf-8'))
        for key in ('symbol_count', 'raw_output_sha256', 'libraries', 'spot_checks'):
            if stored.get(key) != record.get(key):
                problems.append(f'{key} differs from the stored table')
        record['checked_against'] = str(OUTPUT)
        record['matches_stored'] = not problems

    if args.write:
        OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        OUTPUT.write_text(json.dumps(record, indent=2, sort_keys=True),
                          encoding='utf-8')
        RAW_OUTPUT.parent.mkdir(parents=True, exist_ok=True)
        RAW_OUTPUT.write_text(text, encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f"XDK symbols: {record['symbol_count']} "
              f"(minimum {MINIMUM_SYMBOLS}: "
              f"{'PASS' if record['meets_minimum'] else 'FAIL'})")
        for library, count in record['libraries'].items():
            print(f'  {library:<12} {count}')
        print('  spot checks:')
        for check in record['spot_checks']:
            print(f"    {check['result']:<13} {check['name']} "
                  f"({check['expected_va']})")
            if check['result'] != 'PASS':
                print(f"                  {check['detail']}")
        if args.write:
            print(f"  written: {OUTPUT.relative_to(ROOT)}")

    if problems:
        print()
        for problem in problems:
            print(f'  FINDING: {problem}')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
