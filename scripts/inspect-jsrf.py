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
gpu=commands.add_parser('gpu'); gpu.add_argument('run',type=Path)
args=parser.parse_args()
try:
    if args.command=='disasm':
        import capstone
        if not 0 <= args.start < args.end <= 0x100000000 or args.end-args.start>1024*1024:
            raise CaptureError('instruction range must be positive and at most 1 MiB')
        sections=json.loads((root/'game/mygame_analysis.json').read_text())['sections']
        section=next((s for s in sections if int(s['virtual_addr'],16)<=args.start and args.end<=int(s['virtual_addr'],16)+s['raw_size']),None)
        if section is None: raise CaptureError('range is not contained in one file-backed XBE section')
        offset=int(section['raw_addr'],16)+args.start-int(section['virtual_addr'],16)
        with (root/'game/default.xbe').open('rb') as file:
            file.seek(offset); raw=file.read(args.end-args.start)
        for insn in capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32).disasm(raw,args.start):
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
    else:
        from jsrf_gpu import analyze, markdown
        print(markdown(analyze(args.run)))
except (OSError,ValueError,KeyError) as error:
    parser.exit(2,f'Inspection failed: {error}\n')
