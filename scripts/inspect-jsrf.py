"""Inspect original instructions, exact guest memory, or frozen GPU reports."""
import argparse
import json
from pathlib import Path
import struct
from jsrf_dump import CaptureError, DumpMemory

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
commands=parser.add_subparsers(dest='command',required=True)
integer=lambda s:int(s,0)
disasm=commands.add_parser('disasm')
disasm.add_argument('start',type=integer); disasm.add_argument('end',type=integer)
memory=commands.add_parser('memory')
memory.add_argument('run',type=Path); memory.add_argument('start',type=integer); memory.add_argument('length',type=integer)
memory.add_argument('--out',type=Path,help='export exactly these captured bytes as a binary file')
data=commands.add_parser('data')
data.add_argument('start',type=integer); data.add_argument('length',type=integer)
find=commands.add_parser('find')
find.add_argument('value',type=integer,help='dword value to locate in the original XBE image')
find.add_argument('--section',default=None,help='restrict to one section name, e.g. .data')
find.add_argument('--aligned',action='store_true',help='only 4-byte aligned offsets (pointer tables)')
gpu=commands.add_parser('gpu'); gpu.add_argument('run',type=Path)
symbols=commands.add_parser('symbols',help='name guest VAs from config/xdk-symbols.json (plan T2)')
symbols.add_argument('address',nargs='*',type=integer,help='VAs to name; omit to summarise the table')
symbols.add_argument('--near',action='store_true',help='also name the nearest symbol at or below each VA')
args=parser.parse_args()

def xdk_symbols():
    """The generated XDK symbol table, or None with a reason.

    Read from `config/xdk-symbols.json`, which `scripts/gen-xdk-symbols.py`
    writes from the XbSymbolDatabase CLI.  Values are never hand-entered: the
    table is a tool output with its own provenance (tool build identity, XBE
    hash, raw-output hash), and this reader only looks names up in it.
    """
    path=root/'config/xdk-symbols.json'
    if not path.is_file():
        return None,f'{path.relative_to(root)} is missing; run `just symbols`'
    try:
        record=json.loads(path.read_text(encoding='utf-8'))
    except (OSError,ValueError) as error:
        return None,f'{path.relative_to(root)} could not be read: {error}'
    table=record.get('symbols')
    if not isinstance(table,dict) or not table:
        return None,f'{path.relative_to(root)} carries no symbols'
    return record,None

def xbe_bytes(start,end):
    """Original bytes of the retail XBE for a virtual address range."""
    if not 0<=start<end<=0x100000000 or end-start>1024*1024:
        raise CaptureError('range must be positive and at most 1 MiB')
    sections=json.loads((root/'game/mygame_analysis.json').read_text())['sections']
    section=next((s for s in sections if int(s['virtual_addr'],16)<=start and end<=int(s['virtual_addr'],16)+s['raw_size']),None)
    if section is None: raise CaptureError('range is not contained in one file-backed XBE section')
    offset=int(section['raw_addr'],16)+start-int(section['virtual_addr'],16)
    with (root/'game/default.xbe').open('rb') as file:
        file.seek(offset); return file.read(end-start)

try:
    if args.command=='disasm':
        import capstone
        raw=xbe_bytes(args.start,args.end)
        decoder=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
        # Skip undecodable bytes instead of stopping.  Capstone stops at the
        # first invalid instruction and yields nothing at all, so an arbitrary
        # range -- or a start that is not an instruction boundary -- printed an
        # empty listing that looked like "no code here" rather than a decode
        # failure.  Emitting `.byte` keeps the listing aligned and honest.
        decoder.skipdata=True
        for insn in decoder.disasm(raw,args.start):
            print(f'{insn.address:08X} {insn.mnemonic:8} {insn.op_str}')
    elif args.command=='memory':
        with DumpMemory(args.run) as dump: data=dump.read(args.start,args.length)
        if args.out:
            args.out.write_bytes(data)
            print(f'Exported {len(data)} bytes from guest 0x{args.start:08X} to {args.out}')
        else:
            for i in range(0,len(data),16):
                chunk=data[i:i+16]
                if len(chunk)%4==0:
                    rendered=' '.join(f'{v:08X}' for v in struct.unpack('<'+'I'*(len(chunk)//4),chunk))
                else: rendered=chunk.hex(' ')
                print(f'{args.start+i:08X}: {rendered}')
    elif args.command=='data':
        raw=xbe_bytes(args.start,args.start+args.length)
        for i in range(0,len(raw),16):
            chunk=raw[i:i+16]
            if len(chunk)%4==0:
                rendered=' '.join(f'{v:08X}' for v in struct.unpack('<'+'I'*(len(chunk)//4),chunk))
            else: rendered=chunk.hex(' ')
            print(f'{args.start+i:08X}: {rendered}')
    elif args.command=='find':
        sections=json.loads((root/'game/mygame_analysis.json').read_text())['sections']
        needle=struct.pack('<I',args.value&0xFFFFFFFF)
        hits=0
        with (root/'game/default.xbe').open('rb') as file:
            for section in sections:
                if args.section and section['name']!=args.section: continue
                file.seek(int(section['raw_addr'],16)); raw=file.read(section['raw_size'])
                base=int(section['virtual_addr'],16)
                for at in range(0,len(raw)-3,4 if args.aligned else 1):
                    if raw[at:at+4]==needle:
                        print(f'{base+at:08X}  {section["name"]}')
                        hits+=1
        print(f'{hits} occurrence(s) of 0x{args.value:08X}')
    elif args.command=='symbols':
        record,problem=xdk_symbols()
        if problem:
            parser.exit(2,f'{problem}\n')
        table=record['symbols']
        # The generator writes VAs as JSON integers (they are ints in the tool's
        # output).  Accept a hex string too, so a hand-edited or reformatted table
        # reads rather than raising a TypeError that looks like a corrupt file.
        def as_va(value):
            if isinstance(value,int):
                return value
            return int(str(value),16) if str(value).startswith(('0x','0X')) else int(value,16)
        by_va=sorted(((as_va(va),name) for name,va in table.items()))
        if not args.address:
            print(f"table: {record.get('symbol_count')} symbols "
                  f"from {record.get('tool')}")
            tool=record.get('tool_identity',{})
            print(f"  tool revision: {tool.get('describe','UNKNOWN')} "
                  f"({tool.get('revision','UNKNOWN')})")
            print(f"  input XBE:     {record.get('input',{}).get('sha256','UNKNOWN')}")
            for library,count in (record.get('libraries') or {}).items():
                print(f'  {library:<12} {count}')
            if record.get('unknown_libraries'):
                print(f"  UNEXPECTED LIBRARIES: {record['unknown_libraries']}")
        else:
            for address in args.address:
                exact=[name for va,name in by_va if va==address]
                if exact:
                    print(f'{address:08X}  {", ".join(exact)}')
                    continue
                # A miss is reported as a miss.  XbSymbolDatabase covers XDK
                # libraries only, so an unnamed address is the normal case for
                # game code and for CRT helpers -- it is not evidence of a
                # defect, and printing a blank would read as one.
                if args.near:
                    below=[(va,name) for va,name in by_va if va<address]
                    if below:
                        va,name=below[-1]
                        print(f'{address:08X}  (no exact symbol) '
                              f'nearest below: {name} at {va:08X} '
                              f'(+0x{address-va:X})')
                    else:
                        print(f'{address:08X}  (no exact symbol, none below)')
                else:
                    print(f'{address:08X}  (no XDK symbol; pass --near to see the '
                          f'nearest below)')
    else:
        from jsrf_gpu import analyze, markdown
        print(markdown(analyze(args.run)))
except (OSError,ValueError,KeyError) as error:
    parser.exit(2,f'Inspection failed: {error}\n')
