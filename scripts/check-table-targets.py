"""Find pointer-table targets that an indirect call would not resolve.

The failure this detects is a trap, not a cut:

    [ICALL] Failed to resolve VA 0x0007BE30

An address reaches an indirect call, the runtime has no body for it, and the
generated stub raises.  Two ways an address ends up unresolvable, and this
finds both:

  SWALLOWED  a real function that lies strictly inside another entry's span, so
             the alias fold never gave it its own entry.  `0x0007BE30` was
             swallowed by `0x0007BDD0`, whose span ran past its own `ret`,
             across two switch tables and into the next function.
  UNCOVERED  an address no span contains at all.  `0x001185B0` is this one: the
             disassembler's own database stops the preceding body at
             `0x001185AA`, six padding bytes short, and the only entry to it is
             a tail call.

The candidates come from the image itself: every aligned dword in `.data` and
`.rdata` whose value lands in `.text`.  That is how `0x0007BE30` was found --
it occurs at exactly one aligned dword in the whole image, `0x0020D2B8`, and
that is the table the enclosing function calls through.

Most such dwords are not function pointers.  A raw sweep of this title's data
sections yields ~260 in-span candidates and the great majority are byte runs
that happen to look like a `.text` address.  The filter that makes the list
usable is a *function boundary* test: a real entry is preceded by alignment
padding that ends a previous body -- a run of 0x90 or 0xCC, with a
`ret`/`jmp`/`int3` in front of it.  Every one of the addresses fixed with this
script passes it, and it is cheap to state and check.

Resolvability is decided by `scripts/resolution_starts.py`, which reads the
three tables the runtime consults -- the generated `jsrf_lookup_recovered`
switch, `manual-functions.json`, and the generated dispatch.  An earlier version
tested against `config/recovered-functions.json` alone, which lists the 3071
*reviewed* entries while the dispatch covers 8768 addresses, so addresses that
already work looked uncovered.  Six entries of the class dispatch table at
`0x0020D2B8` -- `0x0007C4B0`, `0x0007C800`, `0x00011C90`, `0x0007D000`,
`0x0007D3D0`, `0x0007D120`, `0x0007D290` -- were named as the next packet on
that basis and need no work at all; every one of the 64 entries of that table
resolves.

Output is a triage list.  The run's `[ICALL] Failed to resolve VA` line is the
authority for which candidate actually matters.
"""
from pathlib import Path
import argparse
import json
import struct
import sys

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
from resolution_starts import runtime_starts  # noqa: E402

root = Path(__file__).resolve().parents[1]
sections = json.loads((root / 'game' / 'mygame_analysis.json').read_text())['sections']
entries = json.loads((root / 'config' / 'recovered-functions.json').read_text())
xbe = (root / 'game' / 'default.xbe').read_bytes()

PAD_BYTES = {0x90, 0xCC}
SCAN_BACK = 32
TERMINATORS = {0xC3, 0xC2, 0xE9, 0xEB, 0xCC}


def section_for(va):
    for section in sections:
        base = int(section['virtual_addr'], 16)
        if base <= va < base + section['raw_size']:
            return section
    return None


def read(va, size):
    section = section_for(va)
    if section is None:
        return None
    offset = int(section['raw_addr'], 16) + (va - int(section['virtual_addr'], 16))
    if offset + size > len(xbe):
        return None
    return xbe[offset:offset + size]


def preceded_by_boundary(va):
    """A run of alignment padding, terminated by the previous body's exit."""
    window = read(va - SCAN_BACK, SCAN_BACK)
    if window is None:
        return False
    pad = 0
    for byte in reversed(window):
        if byte in PAD_BYTES:
            pad += 1
        else:
            break
    if pad == 0:
        return False
    before = va - pad - 1
    tail = read(before, 1)
    if tail is None:
        return False
    # The byte before the padding has to be a `ret` or a `jmp` opcode, or the
    # padding has to be long enough that it is clearly alignment rather than a
    # coincidence.
    return tail[0] in TERMINATORS or pad >= 4


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limit', type=int, default=40)
    parser.add_argument('--all', action='store_true',
                        help='skip the function-boundary filter')
    parser.add_argument('--json', action='store_true',
                        help='emit machine-readable output')
    args = parser.parse_args()

    text = next(s for s in sections if s['name'] == '.text')
    tlo = int(text['virtual_addr'], 16)
    thi = tlo + text['virtual_size']

    spans = sorted((int(e['start'], 16), int(e['end'], 16), e['start']) for e in entries)
    covered = runtime_starts(root)

    candidates = {}
    for section in sections:
        if section['name'] not in ('.data', '.rdata'):
            continue
        base = int(section['virtual_addr'], 16)
        offset = int(section['raw_addr'], 16)
        for i in range(0, section['raw_size'] - 3, 4):
            value = struct.unpack_from('<I', xbe, offset + i)[0]
            if not (tlo <= value < thi) or value in covered:
                continue
            container = None
            for lo, hi, name in spans:
                if lo < value < hi:
                    container = '%s..0x%08X' % (name, hi)
                    break
            candidates.setdefault(value, {
                'container': container,
                'occurrences': 0,
                'at': '0x%08X' % (base + i),
            })
            candidates[value]['occurrences'] += 1

    if not args.all:
        candidates = {v: w for v, w in candidates.items()
                      if preceded_by_boundary(v)}

    if args.json:
        print(json.dumps({('0x%08X' % v): w for v, w in sorted(candidates.items())},
                         indent=1))
        return 0

    swallowed = sorted(v for v, w in candidates.items() if w['container'])
    uncovered = sorted(v for v, w in candidates.items() if not w['container'])
    print('unresolvable pointer-table candidates: %d'
          ' (%d swallowed, %d uncovered)'
          % (len(candidates), len(swallowed), len(uncovered)))
    for value in sorted(candidates):
        info = candidates[value]
        where = ('inside %s' % info['container']) if info['container'] else 'no span'
        print('  0x%08X  %-34s first at %s, %d occurrence(s)'
              % (value, where, info['at'], info['occurrences']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
