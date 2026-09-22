"""List dispatch redirects that are complete functions, not fragments.

The alias fold in the toolkit merges an entry into the function that starts
where the entry ends. That is right when the entry is a fragment and wrong when
it is a whole function that merely abuts its neighbour: the body is deleted and
the dispatch tuple is pointed at the wrong function, so a virtual call through
a vtable slot executes an unrelated body. The failure is quiet until something
calls the slot.

This finds the candidates from the generated dispatch table, with three filters
that must all hold, because each one alone saturates (the report records the
three heuristics that were discarded for exactly that reason):

  * the redirect is entered from data -- the address appears as an aligned
    dword in .rdata or .data, which is what a vtable slot or a stored function
    pointer looks like;
  * the fold target is the very next dispatch entry, which is the shape the
    abutting rule produces;
  * the body decodes to a terminator (ret/jmp) before that target, so the
    original never fell through into it.

Output is a candidate list, not a verdict. Each still has to be read as
original code before it is recovered, because a mid-body label entered from a
data table has the first and third properties too.
"""
from pathlib import Path
import json
import re
import struct
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root.parent / 'xboxrecomp'))
import capstone

dispatch = (root / 'src/recomp/gen/recomp_dispatch.c').read_text(encoding='utf-8')
pairs = re.findall(r'\{\s*0x([0-9A-Fa-f]{8})u,\s*\(recomp_func_t\)(\w+)\s*\}', dispatch)


def symbol_address(name):
    m = re.match(r'sub_([0-9A-Fa-f]{8})', name)
    return int(m.group(1), 16) if m else None


redirect = {}
for va, symbol in pairs:
    target = symbol_address(symbol)
    if target is not None and target != int(va, 16):
        redirect[int(va, 16)] = target

all_va = sorted(int(va, 16) for va, _ in pairs)
sections = json.loads((root / 'game/mygame_analysis.json').read_text())['sections']
image = (root / 'game/default.xbe').read_bytes()

pointers = set()
for section in sections:
    if section['name'] not in ('.rdata', '.data'):
        continue
    blob = image[int(section['raw_addr'], 16):int(section['raw_addr'], 16) + section['raw_size']]
    for offset in range(0, len(blob) - 3, 4):
        value = struct.unpack_from('<I', blob, offset)[0]
        if 0x00010000 <= value < 0x00400000:
            pointers.add(value)

decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def last_instruction(va, end):
    """The last instruction of the body, stopping at the first terminator."""
    section = next((s for s in sections
                    if int(s['virtual_addr'], 16) <= va
                    and end <= int(s['virtual_addr'], 16) + s['raw_size']), None)
    if section is None:
        return None
    offset = int(section['raw_addr'], 16) + va - int(section['virtual_addr'], 16)
    last = None
    for insn in decoder.disasm(image[offset:offset + (end - va)], va):
        last = insn
        if insn.mnemonic in ('ret', 'jmp'):
            break
    return last


print(f'{len(redirect)} dispatch redirects')
candidates = []
for va in sorted(redirect):
    if va not in pointers:
        continue
    index = all_va.index(va)
    if index + 1 >= len(all_va) or redirect[va] != all_va[index + 1]:
        continue
    last = last_instruction(va, redirect[va])
    if last is None or last.mnemonic not in ('ret', 'jmp'):
        continue
    candidates.append((va, redirect[va], last))

print(f'{len(candidates)} entered from data, folded into the next entry, '
      f'and self-terminating:')
for va, target, last in candidates:
    print(f'  0x{va:08X} -> 0x{target:08X}   last={last.mnemonic} {last.op_str}')
