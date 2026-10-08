"""Inspect NV2A queue encodings without executing commands or assuming completion.

Encoding reference: xemu pfifo.c, revision 75650bd8cd91945f7b79774e2cee0b200ca373ff.
This decoder is an offline diagnostic, not an implementation of PFIFO semantics.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path
from jsrf_dump import CaptureError, DumpMemory, MAP_LINE


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
SUBMIT_STATE_BASE_FIELDS = ('generation', 'diag', 'method', 'subchannel', 'param', 'at', 'get',
                            'put', 'successes', 'rejections', 'consecutive_rejections',
                            'admitted_unknown')
# The latched FIRST budget stop. Added because the stop that mattered most --
# inside a packet's parameters -- printed nothing at all: only the header path
# dumped, and the submit log stops at 64 walks. `budget_local_pc` is where the
# walk was consuming; `at` is the rollback origin, which is not the same thing.
SUBMIT_STATE_BUDGET_FIELDS = ('budget_stops', 'budget_local_pc', 'budget_words',
                              'budget_packets', 'budget_count', 'budget_method',
                              'budget_ret', 'budget_in_param', 'budget_at_packet_limit')
# Unit accounting, appended after the budget transcript so an older reader stops
# before it rather than misreading it as a budget field.
SUBMIT_STATE_UNIT_FIELDS = ('units', 'units_total', 'loop_bound')
# The continuation audit, appended after the unit fields for the same reason.
# `budget_resume_matched` / `budget_resume_mismatched` are the load-bearing
# pair: benign chunking resumes at the committed boundary every time, so
# mismatched stays zero. `walk_packet_hist` is the log2 histogram of packets
# per walk -- the measurement that says whether the 1024 cap is near-binding
# at all, as opposed to merely present.
SUBMIT_STATE_CONTINUATION_FIELDS = (
    'budget_events_total', 'budget_resume_matched', 'budget_resume_mismatched',
    'budget_rewalked_words', 'budget_rewalked_packets', 'walk_serial',
    'walk_retry_count', 'walk_packet_max', 'walk_words_max',
    'walk_packet_hist0', 'walk_packet_hist1', 'walk_packet_hist2',
    'walk_packet_hist3', 'walk_packet_hist4', 'walk_packet_hist5',
    'walk_packet_hist6', 'walk_packet_hist7', 'walk_packet_hist8',
    'walk_packet_hist9', 'walk_packet_hist10', 'walk_packet_hist11',
    'budget_last_seq', 'budget_last_start_get', 'budget_last_committed_get',
    'budget_last_put', 'budget_last_local_pc', 'budget_last_tail_words',
    'budget_last_units', 'budget_last_resume_start_get',
    'budget_last_resume_end_get', 'budget_last_resume_ok', 'budget_last_resumed')
# The vblank delivery audit, appended after the continuation fields.
SUBMIT_STATE_VBLANK_FIELDS = (
    'vblank_pulses', 'vblank_already_pending', 'vblank_guest_acks',
    'vblank_enable_writes', 'vblank_enable_last', 'vblank_enable_cleared',
    'vblank_irq_asserted', 'vblank_irq_deasserted', 'vblank_last_ack_value',
    'vblank_pending_last')
# The resume-quality audit, appended after the vblank fields. These are the
# counters that make Case A machine-readable rather than log-derived:
# `resume_drained` counts resumptions whose walk reached the stop's PUT (the
# whole submission consumed), and `resume_stalled` counts resumptions that began
# at the right boundary but made NO progress -- the zero-commit livelock, which
# a boundary-only comparison cannot name.
SUBMIT_STATE_RESUME_QUALITY_FIELDS = ('budget_resume_stalled',
                                      'budget_resume_drained')
SUBMIT_STATE_FIELDS = (SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS
                       + SUBMIT_STATE_UNIT_FIELDS + SUBMIT_STATE_CONTINUATION_FIELDS
                       + SUBMIT_STATE_VBLANK_FIELDS
                       + SUBMIT_STATE_RESUME_QUALITY_FIELDS)
SUBMIT_STATE_BASE_SIZE = 4 * len(SUBMIT_STATE_BASE_FIELDS)
SUBMIT_STATE_BUDGET_SIZE = 4 * len(SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS)
SUBMIT_STATE_UNIT_SIZE = 4 * len(SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS
                                 + SUBMIT_STATE_UNIT_FIELDS)
SUBMIT_STATE_CONTINUATION_SIZE = 4 * len(
    SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS + SUBMIT_STATE_UNIT_FIELDS
    + SUBMIT_STATE_CONTINUATION_FIELDS)
SUBMIT_STATE_VBLANK_SIZE = 4 * len(
    SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS + SUBMIT_STATE_UNIT_FIELDS
    + SUBMIT_STATE_CONTINUATION_FIELDS + SUBMIT_STATE_VBLANK_FIELDS)
SUBMIT_STATE_SIZE = 4 * len(SUBMIT_STATE_FIELDS)


def decode_submit_state(raw):
    """Decode g_nv2a_submit_state (NV2ASubmitState, little-endian u32 fields).

    The field lists are the authority, not hardcoded counts. The struct GREW when
    the budget transcript was added, so an archive built before that has only the
    base fields; those are decoded and the new ones are simply absent (rather than
    reading adjacent memory as if it were the transcript). A literal count here
    would silently misread every field after an addition.
    """
    if len(raw) < SUBMIT_STATE_BASE_SIZE:
        raise ValueError(f'submit state needs {SUBMIT_STATE_BASE_SIZE} bytes, got {len(raw)}')
    have = len(raw) // 4
    fields = SUBMIT_STATE_FIELDS[:have]
    state = dict(zip(fields, struct.unpack_from('<%dI' % len(fields), raw)))
    state['has_budget_transcript'] = have >= len(SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS)
    state['has_continuation_audit'] = have >= len(
        SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS + SUBMIT_STATE_UNIT_FIELDS
        + SUBMIT_STATE_CONTINUATION_FIELDS)
    state['has_vblank_audit'] = have >= len(
        SUBMIT_STATE_BASE_FIELDS + SUBMIT_STATE_BUDGET_FIELDS + SUBMIT_STATE_UNIT_FIELDS
        + SUBMIT_STATE_CONTINUATION_FIELDS + SUBMIT_STATE_VBLANK_FIELDS)
    state['has_resume_quality'] = have >= len(SUBMIT_STATE_FIELDS)
    # The histogram arrives as flat fields (the struct holds an array, but the
    # decoder's field-list shape is flat); reassemble it so a reader does not
    # have to know how many bins there are.
    bins = [state.pop(f'walk_packet_hist{i}') for i in range(12)
            if f'walk_packet_hist{i}' in state]
    if bins:
        state['walk_packet_hist'] = bins
    state['diag_name'] = SUBMIT_DIAGNOSTIC_NAMES.get(state['diag'], 'unknown')
    state['torn'] = bool(state['generation'] & 1)   # odd while a walk is writing it
    return state


def _submit_state_declared_size(map_path):
    """Bytes the ARCHIVE's own linker map gives `g_nv2a_submit_state`.

    This is the archive describing itself, which is what makes it reliable: the
    map lays out globals in address order, so the distance to the NEXT symbol is
    the struct's size in that build. Measured: 64 bytes before the budget
    transcript existed, 96 after.

    A source-hash lookup was tried first and is NOT usable here -- `build-source`
    records the SHA-256 of the file content, and a run built from a dirty tree
    matches no commit, so the lookup silently found nothing. The map is present
    in every archive and needs no history.
    """
    text = Path(map_path).read_text(encoding='utf-8', errors='replace')
    # MAP_LINE groups: 1 = symbol name, 2 = address at the preferred base.
    rows = [(m.group(1), int(m.group(2), 16))
            for m in map(MAP_LINE.match, text.splitlines()) if m]
    for i, (name, address) in enumerate(rows):
        if name != 'g_nv2a_submit_state':
            continue
        # The next symbol's address bounds this one.
        if i + 1 < len(rows) and rows[i + 1][1] > address:
            return rows[i + 1][1] - address
    return None


def submit_state_size_for_archive(declared, warnings):
    """How many bytes of `g_nv2a_submit_state` to read from an archive.

    Split out from `read_submit_state` so the DECISION is testable without a dump.
    The regression this guards is silent: asking for the current size from an
    archive built before the struct grew does not fail, it reads whatever globals
    follow and reports them as transcript fields (measured: budget_count =
    44040355 from unrelated memory). A test that only exercises the decoder cannot
    catch that, because the decoder is handed a length and dutifully decodes it.

    `declared` is the size the ARCHIVE's linker map states, or None if it does not
    state one. Returning None means "do not decode".
    """
    if declared is None:
        warnings.append('Submit state size not stated by the archive map; '
                        'decoding the base fields only.')
        return SUBMIT_STATE_BASE_SIZE
    if declared >= SUBMIT_STATE_SIZE:
        return SUBMIT_STATE_SIZE
    if declared >= SUBMIT_STATE_VBLANK_SIZE:
        return SUBMIT_STATE_VBLANK_SIZE
    if declared >= SUBMIT_STATE_CONTINUATION_SIZE:
        return SUBMIT_STATE_CONTINUATION_SIZE
    if declared >= SUBMIT_STATE_UNIT_SIZE:
        return SUBMIT_STATE_UNIT_SIZE
    if declared >= SUBMIT_STATE_BUDGET_SIZE:
        return SUBMIT_STATE_BUDGET_SIZE
    if declared >= SUBMIT_STATE_BASE_SIZE:
        return SUBMIT_STATE_BASE_SIZE
    warnings.append(f'Submit state is {declared} bytes in this archive, smaller than '
                    f'the {SUBMIT_STATE_BASE_SIZE}-byte base layout; not decoded.')
    return None


def read_submit_state(folder, warnings, toolkit=None):
    """Submit state from the dump through the archived linker map, or None with a warning."""
    folder = Path(folder)
    map_path = folder/'jsrf_recomp.map'
    if not (folder/'process.dmp').exists() or not map_path.exists():
        warnings.append('Submit state unavailable: needs process.dmp and jsrf_recomp.map in the archive.')
        return None
    # Ask for the size the ARCHIVE declares, not the size this checkout has.
    size = submit_state_size_for_archive(_submit_state_declared_size(map_path), warnings)
    if size is None:
        return None
    try:
        with DumpMemory(folder) as memory:
            raw = memory.host_symbol(map_path, 'g_nv2a_submit_state', size)
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
    submit_state = read_submit_state(folder, warnings, toolkit)
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
        # The latched FIRST budget stop. Printed whenever one happened, because
        # this is the record the log could not carry: the stop inside a packet's
        # parameters printed nothing at all, and the submit log stops at 64 walks.
        if state.get('budget_stops'):
            where = ('INSIDE a packet\'s parameters' if state.get('budget_in_param')
                     else 'at a packet header')
            limit = ('the 1024-packet cap' if state.get('budget_at_packet_limit')
                     else 'the 4096-word cap')
            lines += [f"First budget stop (of {state['budget_stops']}): {where}, hit {limit}; "
                      f"local_pc {hx(state.get('budget_local_pc', 0))} "
                      f"(NOT the rollback origin {hx(state['at'])}); "
                      f"words {state.get('budget_words')}, packets {state.get('budget_packets')}, "
                      f"straddling packet method {hx(state.get('budget_method', 0))} "
                      f"count {state.get('budget_count')}, ret {hx(state.get('budget_ret', 0))}."]
        # The continuation audit. This is the part the first-stop latch cannot
        # provide: whether the SAME submission resumed at the boundary the last
        # committed unit published. `mismatched` staying zero is what makes
        # "benign chunking" a measurement rather than an assumption.
        if state.get('has_continuation_audit'):
            hist = state.get('walk_packet_hist') or []
            top = [i for i, n in enumerate(hist) if n]
            lines += [f"Budget continuation: {state.get('budget_events_total', 0)} stop(s); "
                      f"resumes matched {state.get('budget_resume_matched', 0)}, "
                      f"**mismatched {state.get('budget_resume_mismatched', 0)}**; "
                      f"re-walked tail {state.get('budget_rewalked_words', 0)} words; "
                      f"walks {state.get('walk_serial', 0)} "
                      f"({state.get('walk_retry_count', 0)} stalled retries)."]
            # The resume-quality counters are what make the classification
            # machine-readable. `drained` is the drain evidence (the walk
            # reached the stop's PUT); `stalled` is the zero-commit livelock,
            # which a boundary-only comparison cannot name.
            if state.get('has_resume_quality'):
                lines += [f"Resume quality: **drained {state.get('budget_resume_drained', 0)}** "
                          f"(walk reached the stop's PUT), "
                          f"**stalled {state.get('budget_resume_stalled', 0)}** "
                          f"(right boundary, no progress). A run whose drained count equals "
                          f"its stop count resumed every stop to completion."]
            lines += [f"Packets per walk: max {state.get('walk_packet_max', 0)}, "
                      f"words max {state.get('walk_words_max', 0)}; "
                      f"occupied log2 bins {top if top else 'none'} "
                      f"(bin i holds walks with 2^i..2^(i+1)-1 packets)."]
            if state.get('budget_last_seq'):
                lines += [f"Last stop #{state['budget_last_seq']}: start_get "
                          f"{hx(state.get('budget_last_start_get', 0))}, committed_get "
                          f"{hx(state.get('budget_last_committed_get', 0))}, put "
                          f"{hx(state.get('budget_last_put', 0))}, local_pc "
                          f"{hx(state.get('budget_last_local_pc', 0))}, rolled-back tail "
                          f"{state.get('budget_last_tail_words', 0)} words, "
                          f"{state.get('budget_last_units', 0)} unit(s) committed; "
                          f"resume start {hx(state.get('budget_last_resume_start_get', 0))} "
                          f"-> end {hx(state.get('budget_last_resume_end_get', 0))}, "
                          f"ok {state.get('budget_last_resume_ok')}, "
                          f"resumed {state.get('budget_last_resumed')}."]
        # The vblank delivery audit. This is the measurement the IRQ delivery
        # log cannot provide (it prints only the first three deliveries), and
        # it separates "the display stopped pulsing" from "the guest stopped
        # acknowledging" from "the line was left asserted".
        if state.get('has_vblank_audit'):
            pulses = state.get('vblank_pulses', 0)
            acks = state.get('vblank_guest_acks', 0)
            stale = state.get('vblank_already_pending', 0)
            lines += [f"Vblank delivery: {pulses} pulse(s), {acks} guest "
                      f"acknowledgement(s) (W1C), {stale} pulse(s) that found the bit "
                      f"still set; enable writes {state.get('vblank_enable_writes', 0)} "
                      f"(last {hx(state.get('vblank_enable_last', 0))}, "
                      f"{state.get('vblank_enable_cleared', 0)} cleared VBLANK); "
                      f"IRQ line transitions {state.get('vblank_irq_asserted', 0)} "
                      f"up / {state.get('vblank_irq_deasserted', 0)} down; "
                      f"last ack {hx(state.get('vblank_last_ack_value', 0))}, "
                      f"pending left {hx(state.get('vblank_pending_last', 0))}."]
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
