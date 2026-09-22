"""Find reviewed recovery entries whose `end` is not an instruction boundary.

`config/recovered-functions.json` stores an exclusive `end`, and
`tools/recomp/translator.py` reads exactly `end - start` bytes before decoding.
An `end` that lands in the middle of an instruction therefore truncates the last
instruction, and the translator silently emits a body without it.  When the cut
instruction is the function's `ret N`, the body loses its epilogue entirely and
has no `return` statement at all, so the checked wrapper aborts the run with an
ABI failure whose `esp` delta is 0.

That happened to `0x00178F40`: the entry ended at `0x001791BA` while its
`ret 0x14` is the three bytes `C2 14 00` at `0x001791B8..0x001791BA`, so only two
of the three bytes were in range.  The run stopped with

    [RECOVERED] ABI FAILURE 0x00178F40 esp 00F7FDA0->00F7FDA0 expected +24

in `logs/runs/20260922-101516-467-p1-178f40`.  Correcting `end` to `0x001791BB`
fixed it.  This script is the detector for the rest of that class.

Method -- the point is to avoid calling alignment padding a defect.  Decode
linearly from `start`; accept every instruction that fits entirely inside `end`;
then look at the bytes left over:

  TRUNCATED  the leftover bytes are neither empty nor padding, so a real
             instruction is cut in half.  `end` must move to that instruction's
             end.
  PADDING    the leftover bytes are all 0x90/0xCC/0x00, i.e. alignment.  Fine,
             and only reported with --show-padding.
  BROKEN     the decoder lost sync inside the entry.  Not an `end` defect, but
             worth knowing about.
  NO-TERMINATOR  informational: the last in-range instruction is not
             ret/jmp/int3, so the body may fall through into whatever follows.
             Legitimate fragments exist, so this is not a defect by itself.
"""
from pathlib import Path
import argparse
import json
import sys

import capstone

root = Path(__file__).resolve().parents[1]
xbe_path = root / 'game' / 'default.xbe'
sections = json.loads((root / 'game' / 'mygame_analysis.json').read_text())['sections']
entries = json.loads((root / 'config' / 'recovered-functions.json').read_text())
xbe = xbe_path.open('rb')

TERMINATORS = {'ret', 'retf', 'jmp', 'int3', 'hlt', 'ud2'}
PADDING_BYTES = {0x90, 0xCC, 0x00}
WINDOW_PAST_END = 16
MAX_INSN = 15


def section_for(va):
    """The file-backed section containing va, or None."""
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


def check(entry):
    start, end = int(entry['start'], 16), int(entry['end'], 16)
    if end <= start:
        return [('BAD-RANGE', start, f'end 0x{end:08X} is not past start 0x{start:08X}')]
    data = raw(start, end - start + WINDOW_PAST_END)
    if data is None:
        return [('NO-SECTION', start, 'range is not inside one file-backed section')]
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.skipdata = True
    problems = []
    pos = start
    last = None
    for insn in decoder.disasm(data, start):
        if insn.address != pos:
            problems.append((
                'BROKEN', pos,
                f'decoder lost sync at 0x{pos:08X} (next instruction reported at '
                f'0x{insn.address:08X})'))
            return problems
        stop = insn.address + insn.size
        if stop > end:
            break
        pos = stop
        last = insn
    if pos < end:
        leftover = raw(pos, end - pos)
        rendered_bytes = ' '.join(f'{byte:02X}' for byte in leftover)
        # Whether the leftover is a cut instruction or alignment padding depends
        # on reachability, not on the byte values alone: after a ret/jmp/int3 the
        # bytes are unreachable, so they cannot be a cut instruction the body
        # needed.  A single 0x00 is ambiguous by value but not by position.
        unreachable = last is not None and last.mnemonic in TERMINATORS
        if unreachable:
            problems.append((
                'PADDING', pos,
                f'{len(leftover)} byte(s) after the terminator at 0x{last.address:08X}: '
                f'{rendered_bytes} at 0x{pos:08X}..0x{end - 1:08X}; unreachable, so '
                'end could be tightened but nothing is lost'))
        else:
            # Decode past `end` so the cut instruction can be named in full.
            wider = raw(pos, min(end - pos + WINDOW_PAST_END, MAX_INSN * 2))
            insn = next(iter(decoder.disasm(wider, pos)), None)
            if insn is None or insn.address != pos:
                detail = (f'leftover {rendered_bytes} at 0x{pos:08X}..0x{end - 1:08X} '
                          f'does not start a decodable instruction; end is 0x{end:08X}')
            else:
                needs = insn.address + insn.size
                detail = (f'`{insn.mnemonic} {insn.op_str}` starts at 0x{pos:08X}, needs '
                          f'bytes up to 0x{needs - 1:08X} but end is 0x{end:08X}; '
                          f'fix end to 0x{needs:08X}')
            problems.append(('TRUNCATED', pos, detail))
    if last is not None and last.mnemonic not in TERMINATORS:
        problems.append((
            'NO-TERMINATOR', last.address,
            f'last in-range instruction is `{last.mnemonic} {last.op_str}`; '
            'the body may fall through past the entry'))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--show-padding', action='store_true',
                        help='also list entries with only alignment padding left over')
    parser.add_argument('--quiet-terminators', action='store_true',
                        help='suppress the informational NO-TERMINATOR reports')
    args = parser.parse_args()
    counts = {}
    for entry in sorted(entries, key=lambda e: int(e['start'], 16)):
        for kind, address, detail in check(entry):
            if kind == 'PADDING' and not args.show_padding:
                continue
            if kind == 'NO-TERMINATOR' and args.quiet_terminators:
                continue
            counts[kind] = counts.get(kind, 0) + 1
            print(f'{kind:14} entry {entry["start"]}-{entry["end"]}: {detail}')
    summary = ', '.join(f'{v} {k}' for k, v in sorted(counts.items())) or 'nothing'
    print(f'\n{len(entries)} entries checked: {summary}')
    return 1 if counts.get('TRUNCATED') else 0


if __name__ == '__main__':
    sys.exit(main())
