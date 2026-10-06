"""Inspect NV2A queue encodings without executing commands or assuming completion.

Encoding reference: xemu pfifo.c, revision 75650bd8cd91945f7b79774e2cee0b200ca373ff.
This decoder is an offline diagnostic, not an implementation of PFIFO semantics.
"""
import argparse
import json
import re
import struct
import subprocess
from pathlib import Path
from jsrf_dump import CaptureError, DumpMemory


# Default budgets mirror the toolkit walk so budget_exhausted here means what it means there.
def decode_queue(read, begin, end, get, put, max_words=4096, max_packets=1024):
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


# Subchannel bindings when no SET_OBJECT is seen; see docs/jsrf-nv2a-method-inventory.md notes.
DEFAULT_SUBCHANNEL_CLASS = {0: 0x97, 1: 0x39, 2: 0x9F, 3: 0x62}
WALK_DIAGNOSTIC = dict(budget_exhausted='budget_exhausted', control_flow_loop='control_flow_loop',
                       nested_call='control_flow_loop', reserved_opcode='reserved_opcode',
                       invalid_target='invalid_target', unexpected_return='invalid_target',
                       truncated_packet='truncated_packet', method_range_overflow='method_range_overflow',
                       missing_memory='unreadable_pushbuffer', invalid_pointer='invalid_get_put')


def load_method_table(path):
    """Parse nv2a_method_table.c into {class_id: {method, ...}}."""
    text = Path(path).read_text(encoding='utf-8', errors='replace')
    classes = {m.group(1): int(m.group(2), 16) for m in re.finditer(r'#define\s+(\w+_CLASS)\s+(0x[0-9a-fA-F]+)u?', text)}
    arrays = {m.group(1): {int(v, 16) for v in re.findall(r'0x[0-9a-fA-F]+', m.group(2))}
              for m in re.finditer(r'g_methods_(\w+)\s*\[\s*\]\s*=\s*\{([^}]*)\}', text)}
    table = {}
    for m in re.finditer(r'case\s+(\w+_CLASS)\s*:\s*return\s+in_table\(\s*g_methods_(\w+)', text):
        if m.group(1) in classes and m.group(2) in arrays:
            table[classes[m.group(1)]] = arrays[m.group(2)]
    return table


def missing_methods(packets, table, bindings=DEFAULT_SUBCHANNEL_CLASS):
    """Methods the toolkit walk would reject: SET_OBJECT (0x0000) rebinds, 0x0100 is exempt."""
    bound, found = dict(bindings), {}
    for packet in packets:
        if 'method' not in packet: continue
        sub, incrementing = packet['subchannel'], packet['kind'] == 'incrementing'
        for i in range(packet['count']):
            method = packet['method'] + (4*i if incrementing else 0)
            if method == 0x0000: bound[sub] = None
            elif method != 0x0100:
                class_id = bound.get(sub)
                if class_id is None: reason = 'binding_unknown'
                elif method not in table.get(class_id, ()): reason = 'not_in_table'
                else: continue
                key = (sub, class_id, method)
                if key in found: found[key]['occurrences'] += 1
                else: found[key] = dict(va=packet['va'], subchannel=sub, class_id=class_id, method=method,
                                        occurrences=1, reason=reason)
            if not incrementing: break
    return list(found.values())


def predicted_walk_diagnostic(queue, missing):
    """Name the walk's nv2a_submit_diagnostic() string for this decode; 'unknown' when not established."""
    status = queue.get('status')
    if missing: return 'unsupported_method'
    diagnostic = WALK_DIAGNOSTIC.get(status)
    if diagnostic: return diagnostic
    return 'ok' if status in ('decoded', 'empty') and missing is not None else 'unknown'


# NV2A_SUBMIT_* codes, toolkit src/nv2a/nv2a_core.c (nv2a_submit_diagnostic).
SUBMIT_DIAGNOSTIC_NAMES = {
    0: 'ok', 1: 'unmapped_pushbuffer', 2: 'unreadable_pushbuffer', 3: 'reserved_opcode',
    4: 'truncated_packet', 5: 'budget_exhausted', 6: 'control_flow_loop', 7: 'invalid_target',
    8: 'method_range_overflow', 9: 'sink_capacity', 10: 'invalid_get_put',
    11: 'unsupported_method', 12: 'invalid_handle', 13: 'semaphore_fault',
    14: 'software_method_trap', 15: 'flip_stall', 16: 'held_software_method',
    17: 'held_flip_stall', 18: 'software_method_unchecked'}
SUBMIT_STATE_FIELDS = ('generation', 'diag', 'method', 'subchannel', 'param', 'at', 'get',
                       'put', 'successes', 'rejections', 'consecutive_rejections',
                       'admitted_unknown')
SUBMIT_STATE_SIZE = 4 * len(SUBMIT_STATE_FIELDS)


def decode_submit_state(raw):
    """Decode g_nv2a_submit_state (NV2ASubmitState: 12 little-endian u32); extra bytes are ignored."""
    if len(raw) < SUBMIT_STATE_SIZE:
        raise ValueError(f'submit state needs {SUBMIT_STATE_SIZE} bytes, got {len(raw)}')
    state = dict(zip(SUBMIT_STATE_FIELDS, struct.unpack_from('<12I', raw)))
    state['diag_name'] = SUBMIT_DIAGNOSTIC_NAMES.get(state['diag'], 'unknown')
    state['torn'] = bool(state['generation'] & 1)   # odd while a walk is writing it
    return state


def read_submit_state(folder, warnings):
    """Submit state from the dump through the archived linker map, or None with a warning."""
    folder = Path(folder)
    map_path = folder/'jsrf_recomp.map'
    if not (folder/'process.dmp').exists() or not map_path.exists():
        warnings.append('Submit state unavailable: needs process.dmp and jsrf_recomp.map in the archive.')
        return None
    try:
        with DumpMemory(folder) as memory:
            raw = memory.host_symbol(map_path, 'g_nv2a_submit_state', SUBMIT_STATE_SIZE)
        return decode_submit_state(raw)
    except (CaptureError, OSError, ValueError, struct.error) as error:
        warnings.append(f'Submit state unavailable (g_nv2a_submit_state; a build before 2026-10-06 lacks it): {error}')
        return None


def toolkit_revision(toolkit):
    try:
        done = subprocess.run(['git', '-C', str(toolkit), 'rev-parse', '--short', 'HEAD'],
                              capture_output=True, text=True, timeout=10)
        return done.stdout.strip() or None if done.returncode == 0 else None
    except (OSError, subprocess.SubprocessError): return None


def load_snapshots(folder):
    path = Path(folder)/'gpu-snapshots.jsonl'
    if not path.exists(): return []
    if path.stat().st_size > 4*1024*1024: raise CaptureError('GPU snapshot file exceeds inspection limit')
    snapshots = [json.loads(line) for line in path.read_text().splitlines() if line.strip()]
    if len(snapshots)>64 or any(s.get('version') != 1 for s in snapshots):
        raise CaptureError('unsupported GPU snapshot version/count')
    return snapshots


def analyze(folder, toolkit=None):
    folder = Path(folder)
    toolkit = Path(toolkit) if toolkit else Path(__file__).resolve().parents[2]/'xboxrecomp'
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
    def norm(value):
        # A pointer at the ring end is the ring start.
        if value is not None and device.get('push_end') is not None and device.get('push_begin') is not None \
                and 0x80000000+value==device['push_end']: return device['push_begin']-0x80000000
        return value
    for snap in snapshots:
        r=snap['registers']
        observations.append(dict(index=snap['index'],tag=snap['tag'],tick_ms=snap['tick_ms'],
                                 reason=snap['reason'],tid=snap['tid'],get=r.get('USER_DMA_GET'),put=r.get('USER_DMA_PUT')))
    pairs=[(norm(o['get']),norm(o['put'])) for o in observations if o['get'] is not None and o['put'] is not None]
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
    table_path = toolkit/'src'/'nv2a'/'nv2a_method_table.c'
    method_table = dict(path=str(table_path), toolkit_rev=toolkit_revision(toolkit))
    try: table = load_method_table(table_path)
    except OSError:
        table = None
        warnings.append(f'Toolkit method table not found at {table_path}; unsupported methods were not checked.')
    missing = None if table is None else missing_methods(queue.get('packets', []), table)
    submit_state = read_submit_state(folder, warnings)
    if progress == 'pending_unchanged':
        warnings.append('Pending queue unchanged across observed stops; inspect CPU waiter stacks. Unobserved intermediate changes are possible.')
    return dict(version=1,status='captured',snapshot_count=len(snapshots),fixture=final.get('fixture'),
                ack_enabled=final.get('ack_enabled'),registers=regs,device=device,queue=queue,
                progress=progress,observations=observations,warnings=warnings,
                missing_methods=missing,predicted_diagnostic=predicted_walk_diagnostic(queue,missing),method_table=method_table,submit_state=submit_state)


def markdown(report):
    lines=['# GPU capture report','',f"Status: {report['status']}",'']
    lines += ['- '+warning for warning in report.get('warnings',[])]
    if report['status']!='captured': return '\n'.join(lines)+'\n'
    lines += ['',f"Observed queue history: **{report['progress']}**.",
              f"Final pending queue decode: **{report['queue']['status']}**.",
              f"Predicted walk diagnostic: **{report['predicted_diagnostic']}**.",'',
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
    state = report.get('submit_state')
    if state is None: lines += ['', 'Submit state: unavailable.']
    else:
        lines += ['', f"Submit state: last walk **{state['diag_name']}**; {state['consecutive_rejections']} consecutive rejection(s); "
                      f"method {hx(state['method'])}, subchannel {state['subchannel']}, param {hx(state['param'])}; "
                      f"torn: {'yes' if state['torn'] else 'no'}."]
    lines += ['',report['queue'].get('detail',''),'','## Methods in the pending stream the table lacks','']
    missing = report['missing_methods']
    if missing is None: lines.append('Method table unavailable; unsupported methods were not checked.')
    elif not missing: lines.append('None.')
    else:
        lines += ['| Subchannel | Class | Method | First VA | Occurrences |','|---|---|---|---|---|']
        lines += [f"| {m['subchannel']} | {'unknown' if m['class_id'] is None else hx(m['class_id'])} | {hx(m['method'])} | {hx(m['va'])} | {m['occurrences']} |" for m in missing]
    return '\n'.join(lines)+'\n'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--write',action='store_true',help='write gpu-report.json and gpu-report.md into the archive')
    parser.add_argument('--toolkit',type=Path,help='toolkit checkout holding src/nv2a/nv2a_method_table.c (default: sibling xboxrecomp)')
    parser.add_argument('--compare',type=Path,help='compare final register storage against another archive')
    args=parser.parse_args()
    try:
        report=analyze(args.run,args.toolkit)
        if args.compare:
            other=analyze(args.compare,args.toolkit)
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
