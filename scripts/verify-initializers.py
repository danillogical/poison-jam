"""Compare captured tables against a small interpreter of ORIGINAL XBE instructions.

Only reviewed straight-line MOV/XOR/RET initializers are supported. Reject any
other instruction, register-dependent address, or missing ABI checkpoint.
"""
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import capstone

root=Path(__file__).resolve().parents[1]
folder=Path(sys.argv[1])
spec=importlib.util.spec_from_file_location('harness_tests',root/'scripts/test-harness.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
dump=(folder/'process.dmp').read_bytes();_,ranges=module.dump_ranges(dump)
stacks=(folder/'stacks.txt').read_text(errors='replace')
ram=int(re.search(r'guest_ram=([0-9A-F]+)',stacks)[1],16)-0x10000
log=(folder/'jsrf_run.log').read_text(errors='replace')
xbe=(root/'game/default.xbe').read_bytes()
sections=json.loads((root/'game/mygame_analysis.json').read_text())['sections']
def read_guest(va):
    native=va+ram
    base,size,rva=next(r for r in ranges if r[0]<=native and native+4<=r[0]+r[1])
    return struct.unpack_from('<I',dump,rva+native-base)[0]

cs=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);cs.detail=True
total=0
for entry in json.loads((root/'config/recovered-functions.json').read_text()):
    if entry.get('kind','initializer')!='initializer':continue
    start,end=int(entry['start'],16),int(entry['end'],16)
    section=next(s for s in sections if int(s['virtual_addr'],16)<=start<int(s['virtual_addr'],16)+s['raw_size'])
    offset=int(section['raw_addr'],16)+start-int(section['virtual_addr'],16)
    registers={};writes={}
    instructions=list(cs.disasm(xbe[offset:offset+end-start],start))
    assert instructions[-1].mnemonic=='ret'
    for instruction in instructions:
        ops=instruction.operands
        if instruction.mnemonic=='ret':break
        if instruction.mnemonic=='xor' and ops[0].reg==ops[1].reg:
            registers[ops[0].reg]=0
        elif instruction.mnemonic=='mov':
            if ops[1].type==capstone.x86.X86_OP_IMM:value=ops[1].imm
            elif ops[1].type==capstone.x86.X86_OP_REG:value=registers[ops[1].reg]
            else:
                assert ops[1].mem.base==ops[1].mem.index==0 and ops[1].size==4
                value=read_guest(ops[1].mem.disp)
            if ops[0].type==capstone.x86.X86_OP_REG:registers[ops[0].reg]=value
            else:
                assert ops[0].mem.base==ops[0].mem.index==0 and ops[0].size==4
                writes[ops[0].mem.disp]=value
        else:raise AssertionError(f'Unsupported reference instruction {instruction.mnemonic}')
    for va,value in writes.items():assert read_guest(va)==value,(entry['start'],hex(va),hex(value),hex(read_guest(va)))
    assert f"[RECOVERED] 0x{start:08X} returned; ABI verified" in log
    print(f"PASS {entry['start']}: {len(writes)} original-XBE writes and guest ABI")
    total+=len(writes)
print(f'PASS: {total} initialized words match captured game memory')
