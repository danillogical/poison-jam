"""Propose a real `end` for recovered entries whose generated body has no return.

Forty of the 3068 entries in `config/recovered-functions.json` produce a body
with no `return` statement.  They are the COM vtable methods recovered to escape
the `ff4d442` abutting-alias fold, and their `end` was tightened to "the next
entry start inside the alias range" -- which lands in the middle of the real
function.  `0x00014870` is the measured shape:

```
00014870 push ecx / push ebx / push ebp / push esi
...
0001487E cmp  eax, ebp
00014880 push edi                       <-- covered by no entry
00014881 mov  [esp+0x10], ebp           <-- entry 0x00014881 starts here
00014885 jbe  0x14909
...
00014954 ret
00014955 nop
```

The head entry ends at `0x00014880`, so the body stops at the `cmp`: no branch,
no epilogue, no return.  Called, it aborts the way `0x00178F40` did.

The real function ends at `0x00014955`, just past its last `ret` and before the
inter-function padding.  That is the rule this script applies: decode forward
from the entry start and take the first `ret` that is followed by padding, or by
a byte that does not continue the function.  It never proposes a *shorter* span
and it never crosses a section end.

Usage:
  python scripts/fix-fragment-spans.py            # report proposals only
  python scripts/fix-fragment-spans.py --apply    # rewrite the entries
"""
from pathlib import Path
import argparse
import json
import re
import sys

import capstone

root = Path(__file__).resolve().parents[1]
xbe_path = root / 'game' / 'default.xbe'
sections = json.loads((root / 'game' / 'mygame_analysis.json').read_text())['sections']
config_path = root / 'config' / 'recovered-functions.json'
entries = json.loads(config_path.read_text())
xbe = xbe_path.open('rb')

GENERATED = root / 'src' / 'recomp' / 'recovered' / 'recovered.c'
SCAN_LIMIT = 0x2000
PAD = {0x90, 0xCC}


def section_for(va):
    for section in sections:
        base = int(section['virtual_addr'], 16)
        if base <= va < base + section['raw_size']:
            return section
    return None


def raw(va, length):
    section = section_for(va)
    if section is None:
        return None
    base = int(section['virtual_addr'], 16)
    offset = int(section['raw_addr'], 16) + (va - base)
    xbe.seek(offset)
    return xbe.read(length)


def returnless_bodies():
    """Entry addresses whose generated body contains no `return` statement."""
    source = GENERATED.read_text(encoding='utf-8', errors='replace')
    return {m.group(1) for m in re.finditer(
        r'static void body_([0-9A-F]{8})\(void\)\s*\{(.*?)\n\}\n', source, re.S)
        if 'return' not in m.group(2)}


def proposed_end(start, current_end, internal, entry_starts):
    """First `ret` past current_end that is followed by padding, plus its size.

    The scan stops at the first entry start past `current_end` that is not itself
    a return-less head.  An entry that *is* one is a candidate internal label of
    this function (`0x00014881` sits mid-function inside `0x00014870`), so the
    scan continues past it; anything else is the next function and the scan must
    not walk into it.
    """
    limit = end_of_section(start) - start
    data = raw(start, min(SCAN_LIMIT, limit))
    if data is None:
        return None, 'range is not inside one file-backed section'
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.skipdata = True
    pos = start
    for insn in decoder.disasm(data, start):
        if insn.address != pos:
            return None, f'decoder lost sync at 0x{pos:08X}'
        if insn.address > current_end:
            blocking = [a for a in entry_starts
                        if current_end < a <= insn.address and a not in internal]
            if blocking:
                return None, (f'the next function starts at 0x{min(blocking):08X} '
                              'before any `ret`')
        pos = insn.address + insn.size
        if insn.mnemonic == 'int3':
            # A noreturn body: the thread-startup trampolines end at an `int3`
            # after calling the noreturn thread exit, so they legitimately have
            # no `ret`. Scanning past them walks into the *next* function and
            # would glue four separate trampolines into each other.
            return None, (f'`int3` at 0x{insn.address:08X} ends a noreturn body; '
                          'nothing to extend')
        if insn.mnemonic in ('ret', 'retf') and pos > current_end:
            following = raw(pos, 1)
            if not following or following[0] in PAD:
                return pos, f'`{insn.mnemonic} {insn.op_str}` at 0x{insn.address:08X}'
            return None, (f'`{insn.mnemonic} {insn.op_str}` at 0x{insn.address:08X} is '
                          f'followed by 0x{following[0]:02X}, not padding')
    return None, 'no `ret` followed by padding within the scan limit'


def end_of_section(va):
    for section in sections:
        base = int(section['virtual_addr'], 16)
        if base <= va < base + section['raw_size']:
            return base + section['raw_size']
    return va + SCAN_LIMIT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true',
                        help='write the proposals into config/recovered-functions.json')
    args = parser.parse_args()
    wanted = returnless_bodies()
    by_start = {e['start'].upper()[2:]: e for e in entries}
    # Block the scan on any known function start, not only on recovered entries:
    # 0x0013B330 is a database function the manifest does not list, and scanning
    # past it walks into it.
    database = json.loads((root / 'tools' / 'disasm' / 'output'
                           / 'functions.recovered.json').read_text())
    entry_starts = ({int(e['start'], 16) for e in entries} |
                    {int(f['start'], 16) for f in database})
    # An entry that is itself a return-less head may be an internal label of the
    # function being scanned, so it does not stop the scan; the comparison has to
    # be on addresses, not on names.
    internal = {int(by_start[n]['start'], 16) for n in wanted if n in by_start}
    proposals = []
    unresolved = []
    for name in sorted(wanted):
        entry = by_start.get(name)
        if entry is None:
            unresolved.append((name, 'no config entry'))
            continue
        start, current = int(entry['start'], 16), int(entry['end'], 16)
        end, why = proposed_end(start, current, internal, entry_starts)
        if end is None:
            unresolved.append((name, why))
            continue
        if end <= current:
            unresolved.append((name, f'proposal 0x{end:08X} is not past 0x{current:08X}'))
            continue
        proposals.append((entry, current, end, why))
    for entry, current, end, why in proposals:
        print(f'{entry["start"]}-{entry["end"]} -> 0x{end:08X}  ({why})')
    for name, why in unresolved:
        print(f'UNRESOLVED 0x{name}: {why}')
    print(f'\n{len(wanted)} return-less bodies: {len(proposals)} proposals, '
          f'{len(unresolved)} unresolved')
    if not args.apply:
        print('dry run; pass --apply to rewrite the entries')
        return 0
    for entry, current, end, why in proposals:
        entry['end'] = f'0x{end:08X}'
        entry['evidence'] = (entry['evidence'].rstrip() +
                             f' END EXTENDED to 0x{end:08X}: the previous end 0x{current:08X} '
                             'stopped inside the function, so the generated body had no epilogue '
                             f'and no return statement at all. {why} is the last `ret` before the '
                             'inter-function padding, measured by scripts/fix-fragment-spans.py. '
                             'Evidence: report-deepseek.md, 2026-09-22.')
    config_path.write_text(json.dumps(entries, indent=2) + '\n', encoding='utf-8')
    print(f'applied {len(proposals)} span extensions to {config_path}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
