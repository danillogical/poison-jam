"""Inspect NV2A queue encodings without executing commands or assuming completion.

Encoding reference: xemu pfifo.c, revision 75650bd8cd91945f7b79774e2cee0b200ca373ff.
This decoder is an offline diagnostic, not an implementation of PFIFO semantics.
"""
import argparse
import json
from pathlib import Path
from jsrf_dump import CaptureError, DumpMemory


def decode_queue(read, begin, end, get, put, max_words=1024, max_packets=256):
    packets, consumed, visited, ret = [], 0, set(), None
    def result(status, detail=''):
        return dict(status=status, detail=detail, words_read=consumed, packets=packets)
    if any(v is None for v in (begin,end,get,put)):
        return result('unavailable','queue fields could not be read')
    if not (0x80000000 <= begin < end <= 0x84000000) or any(v & 3 for v in (begin,end,get,put)):
        return result('invalid_bounds','unaligned or invalid contiguous queue range')
    if not (begin <= get <= end and begin <= put <= end):
        if get == put == 0x80000000:
            return result('submission_unestablished','zero hardware pointers lie outside the allocated buffer')
        return result('invalid_pointer','GET/PUT outside declared buffer')
    pc = begin if get == end else get
    put = begin if put == end else put
    def advance(address): return begin if address+4 == end else address+4
    try:
        while pc != put:
            if consumed >= max_words or len(packets) >= max_packets:
                return result('budget_exhausted','bounded inspection stopped')
            state = (pc,ret)
            if state in visited: return result('control_flow_loop',f'revisited 0x{pc:08X}')
            visited.add(state)
            address, word = pc, read(pc)
            consumed += 1
            pc = advance(pc)
            packet = dict(va=address, header=word)
            packets.append(packet)
            if (word & 0xe0000003) == 0x20000000:
                packet['kind'], target = 'old_jump', word & 0x1fffffff
            elif word & 3 == 1:
                packet['kind'], target = 'jump', word & 0xfffffffc
            elif word & 3 == 2:
                if ret is not None: return result('nested_call','hardware subroutine slot already occupied')
                ret = pc
                packet['kind'], target = 'call', word & 0xfffffffc
            elif word == 0x00020000:
                packet['kind'] = 'return'
                if ret is None: return result('unexpected_return','no saved subroutine address')
                target, ret = ret-0x80000000, None
            elif word & 0xe0030003 in (0,0x40000000):
                count, method = (word>>18)&0x7ff, word&0x1ffc
                packet.update(kind='non_incrementing' if word & 0x40000000 else 'incrementing',
                              subchannel=(word>>13)&7, method=method, count=count, data=[])
                if not (word & 0x40000000) and count and method+4*(count-1)>0x1ffc:
                    return result('method_range_overflow','method span exceeds decoded address field')
                for _ in range(count):
                    if pc == put: return result('truncated_packet','PUT reached before all parameter words')
                    if consumed >= max_words: return result('budget_exhausted','parameter inspection limit')
                    packet['data'].append(read(pc)); consumed += 1; pc=advance(pc)
                continue
            else:
                packet['kind'] = 'reserved'
                return result('reserved_opcode',f'0x{word:08X} at 0x{address:08X}')
            # The diagnostic uses the current toolkit's physical contiguous window.
            if target >= 0x04000000: return result('invalid_target','unsupported physical target')
            pc = 0x80000000+target
            packet['target'] = pc
            if pc & 3 or not begin <= pc < end: return result('invalid_target','branch leaves declared buffer')
        return result('empty' if not packets else 'decoded','GET == PUT is not proof of executed commands')
    except CaptureError as error:
        return result('missing_memory',str(error))


def load_snapshots(folder):
    path = Path(folder)/'gpu-snapshots.jsonl'
    if not path.exists(): return []
    if path.stat().st_size > 4*1024*1024: raise CaptureError('GPU snapshot file exceeds inspection limit')
    snapshots = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if len(snapshots)>64 or any(s.get('version') != 1 for s in snapshots):
        raise CaptureError('unsupported GPU snapshot version/count')
    return snapshots


def analyze(folder):
    folder = Path(folder)
    snapshots = load_snapshots(folder)
    if not snapshots: return dict(status='unavailable',warnings=['No frozen GPU snapshots in this archive.'])
    final = snapshots[-1]
    regs, device = final['registers'], final['device']
    warnings = ['Snapshot values describe storage; this report does not assert GPU execution or rendering.']
    if final.get('fixture'): warnings.append('SYNTHETIC FIXTURE: pointer changes are test actions, not GPU work.')
    if final.get('ack_enabled'): warnings.append('Legacy acknowledgement is active: GET can advance without command execution.')
    if final.get('ack_enabled') is None: warnings.append('Acknowledgement mode unavailable; ownership is not established.')
    unreadable = [name for name,value in regs.items() if value is None]
    if unreadable: warnings.append('Unreadable registers: '+', '.join(unreadable))
    observations=[]
    for snap in snapshots:
        r=snap['registers']
        observations.append(dict(index=snap['index'],tag=snap['tag'],tick_ms=snap['tick_ms'],
                                 reason=snap['reason'],tid=snap['tid'],get=r.get('USER_DMA_GET'),put=r.get('USER_DMA_PUT')))
    pairs=[(o['get'],o['put']) for o in observations if o['get'] is not None and o['put'] is not None]
    progress='insufficient_observations'
    if len(pairs)>=2:
        progress = 'pointer_changes_observed' if len(set(pairs))>1 else 'pending_unchanged' if pairs[-1][0]!=pairs[-1][1] else 'equal_unchanged'
    get,put=regs.get('USER_DMA_GET'),regs.get('USER_DMA_PUT')
    queue=dict(status='unavailable',detail='dump or register values unavailable',packets=[])
    if get is not None and put is not None:
        if get>=0x04000000 or put>=0x04000000:
            queue=dict(status='invalid_pointer',detail='unsupported physical GET/PUT format',packets=[])
        elif (folder/'process.dmp').exists():
            with DumpMemory(folder) as memory:
                queue=decode_queue(memory.word,device.get('push_begin'),device.get('push_end'),
                                   0x80000000+get,0x80000000+put)
    if progress == 'pending_unchanged':
        warnings.append('Pending queue unchanged across observed stops; inspect CPU waiter stacks. Unobserved intermediate changes are possible.')
    return dict(version=1,status='captured',snapshot_count=len(snapshots),fixture=final.get('fixture'),
                ack_enabled=final.get('ack_enabled'),registers=regs,device=device,queue=queue,
                progress=progress,observations=observations,warnings=warnings)


def markdown(report):
    lines=['# GPU capture report','',f"Status: {report['status']}",'']
    lines += ['- '+warning for warning in report.get('warnings',[])]
    if report['status']!='captured': return '\n'.join(lines)+'\n'
    lines += ['',f"Observed queue history: **{report['progress']}**.",
              f"Final pending queue decode: **{report['queue']['status']}**.",'',
              '| Snapshot | Reason | Thread | Tick (ms) | GET | PUT |','|---|---|---|---|---|---|']
    def hx(value): return 'unavailable' if value is None else f'0x{value:08X}'
    for row in report['observations']:
        lines.append(f"| {row['index']} | {row['reason']} | {row['tid']} | {row['tick_ms']} | {hx(row['get'])} | {hx(row['put'])} |")
    lines += ['', '## Final register storage', '', '| Register | Value |','|---|---|']
    lines += [f'| {key} | {hx(value)} |' for key,value in report['registers'].items()]
    lines += ['', '## Pending packets', '', 'Only the final dump is decoded. Earlier snapshots retain bounded previews;',
              'final memory must not be treated as the historical contents of a reused buffer.', '']
    for packet in report['queue'].get('packets',[]):
        lines.append(f"- {hx(packet['va'])}: {packet['kind'] if 'kind' in packet else 'incomplete'}; header {hx(packet['header'])}" +
                     (f"; method {hx(packet['method'])}, count {packet['count']}" if 'method' in packet else ''))
    lines += ['',report['queue'].get('detail','')]
    return '\n'.join(lines)+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--write',action='store_true',help='write gpu-report.json and gpu-report.md into the archive')
    parser.add_argument('--compare',type=Path,help='compare final register storage against another archive')
    args=parser.parse_args()
    try:
        report=analyze(args.run)
        if args.compare:
            other=analyze(args.compare)
            report['register_changes']={key:dict(before=other.get('registers',{}).get(key),after=value)
                                        for key,value in report.get('registers',{}).items()
                                        if other.get('registers',{}).get(key)!=value}
            print(json.dumps(report['register_changes'],indent=2))
        if args.write:
            (args.run/'gpu-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            (args.run/'gpu-report.md').write_text(markdown(report),encoding='utf-8')
        else: print(markdown(report))
    except (OSError,ValueError,KeyError) as error:
        parser.exit(2,f'GPU inspection failed: {error}\n')


if __name__=='__main__': main()
