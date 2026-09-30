"""Extract the V3 rebaseline evidence from an archived run, one line per fact.

Reads only the run's own artifacts (`jsrf_run.log`, `result.json`) and prints a
compact record.  Written because the raw log is ~100 MB of `[KERNEL]` lines and
eyeballing it is how a stop site gets transcribed wrong.

Usage::

    python -X utf8 scripts/v3-evidence.py <run-dir> [--json]
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

KERNEL_RE = re.compile(r'\[KERNEL\] #(\d+): ordinal (\d+) \(slot (\d+)\).*?ret=0x([0-9A-Fa-f]+) tid=(\d+)')
ICALL_RE = re.compile(r'\[ICALL\] invalid target 0x([0-9A-Fa-f]+) tid=(\d+) esp=([0-9A-Fa-f]+) return=([0-9A-Fa-f]+)')
ALIAS_RE = re.compile(r'\[ALIAS-ICALL\] target=0x([0-9A-Fa-f]+) owner=0x([0-9A-Fa-f]+)')
UNIMPL_RE = re.compile(r'\[UNIMPL\]')
EXC_RE = re.compile(r'\[EXCEPTION\] tid=(\d+) code=0x([0-9A-Fa-f]+)')
KMEM_RE = re.compile(r'\[KMEM\] summary (.*)')
KMEM_REJECT_RE = re.compile(r'\[KMEM\] reject (.*)')
EXPORT_RE = re.compile(r'data export ordinal (\d+) \(slot (\d+)\) -> 0x([0-9A-Fa-f]+)')
GMETER_RE = re.compile(r'\[GMETER\](.*)')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)

    run = Path(args.run_dir)
    if not run.is_dir():
        run = ROOT / args.run_dir
    if not run.is_dir():
        print(f'no such run directory: {args.run_dir}', file=sys.stderr)
        return 2

    log = run / 'jsrf_run.log'
    if not log.is_file():
        print(f'no jsrf_run.log in {run}', file=sys.stderr)
        return 2

    text = log.read_text(encoding='utf-8', errors='replace')
    lines = text.split('\n')

    result = {}
    rj = run / 'result.json'
    if rj.is_file():
        result = json.loads(rj.read_text(encoding='utf-8'))

    # Kernel calls: the counter is per-thread, so the run's total is the sum of
    # each thread's maximum -- not the max over all lines.
    per_thread: dict[str, int] = {}
    last_calls: list[str] = []
    for line in lines:
        m = KERNEL_RE.search(line)
        if m:
            n, ordinal, slot, ret, tid = m.groups()
            per_thread[tid] = max(per_thread.get(tid, 0), int(n))
            last_calls.append(f'#{n} ordinal {ordinal} slot {slot} ret=0x{ret} tid={tid}')
    last_calls = last_calls[-20:]

    icalls = [{'target': m.group(1), 'tid': m.group(2),
               'esp': m.group(3), 'return': m.group(4)}
              for m in (ICALL_RE.search(l) for l in lines) if m]
    alias = [{'target': m.group(1), 'owner': m.group(2)}
             for m in (ALIAS_RE.search(l) for l in lines) if m]
    exceptions = [{'tid': m.group(1), 'code': m.group(2)}
                  for m in (EXC_RE.search(l) for l in lines) if m]
    kmem = [m.group(1).strip() for m in (KMEM_RE.search(l) for l in lines) if m]
    kmem_rejects = [m.group(1).strip() for m in (KMEM_REJECT_RE.search(l) for l in lines) if m]
    exports = [f'ordinal {m.group(1)} slot {m.group(2)} -> 0x{m.group(3)}'
               for m in (EXPORT_RE.search(l) for l in lines) if m]
    gmeter = [m.group(1).strip() for m in (GMETER_RE.search(l) for l in lines) if m]
    unimpl = len(UNIMPL_RE.findall(text))

    payload = {
        'run': run.name,
        'result': result,
        'kernel_calls_total': sum(per_thread.values()),
        'kernel_calls_per_thread': per_thread,
        'kernel_calls_last_20': last_calls,
        'icall_invalid': icalls,
        'alias_icall': alias,
        'exceptions': exceptions,
        'kmem_summary': kmem,
        'kmem_rejects': kmem_rejects,
        'data_export_ordinals': exports,
        'gmeter': gmeter,
        'unimpl_lines': unimpl,
        'log_lines': len(lines),
    }

    if args.json:
        print(json.dumps(payload, indent=2, sort_keys=True))
        return 0

    print(f'run            : {payload["run"]}')
    print(f'outcome        : {result.get("outcome")}  duration={result.get("duration_seconds")}s')
    print(f'kernel calls   : {payload["kernel_calls_total"]} total; per thread {per_thread}')
    print(f'[UNIMPL] lines : {unimpl}')
    print(f'data exports   : {len(exports)}')
    for e in exports:
        print(f'    {e}')
    print(f'[KMEM] summary : {kmem[0] if kmem else "(none)"}')
    for r in kmem_rejects:
        print(f'    reject: {r}')
    print(f'[GMETER]       : {gmeter if gmeter else "(none)"}')
    print(f'invalid ICALLs : {len(icalls)}')
    for i in icalls:
        print(f'    target=0x{i["target"]} tid={i["tid"]} esp={i["esp"]} return={i["return"]}')
    print(f'ALIAS-ICALLs   : {len(alias)}')
    for a in alias[:8]:
        print(f'    target=0x{a["target"]} owner=0x{a["owner"]}')
    print(f'exceptions     : {len(exceptions)}')
    for e in exceptions:
        print(f'    tid={e["tid"]} code=0x{e["code"]}')
    print('last 20 kernel calls:')
    for c in last_calls:
        print(f'    {c}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
