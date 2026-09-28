"""Exercise real collector launches; inspect outcomes, stacks AND dump contents."""
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
from jsrf_dump import dump_ranges

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT/'build'

def check_dump(folder, mode):
    data = (folder/'process.dmp').read_bytes()
    streams, ranges = dump_ranges(data)
    assert 3 in streams, 'missing native thread list'
    assert any(length >= 60*1024*1024 for _,length,_ in ranges), 'missing canonical guest RAM'
    stacks = (folder/'stacks.txt').read_text(errors='replace')
    for name in ('contiguous', 'nv2a_registers'):
        if name=='nv2a_registers' and mode!='gpu-mmio-lifecycle':
            assert 'DUMP_REGION_SKIPPED name=nv2a_registers' in stacks
            continue
        region = re.search(r'DUMP_REGION name='+name+r' va=([0-9A-F]+) address=([0-9A-F]+) size=(\d+)', stacks)
        assert region, f'missing {name} capture registration'
        address, size = int(region[2],16), int(region[3])
        assert any(base <= address and address+size <= base+length for base,length,_ in ranges), f'missing {name} dump memory'
    marker = re.search(r'PROBE_GPU_MEMORY va=([0-9A-F]+) value=([0-9A-F]+)',
                       (folder/'jsrf_run.log').read_text(errors='replace'))
    if not mode.startswith('gpu-'):
        assert marker, 'missing allocated GPU-memory probe'
        offset = int(re.search(r'guest_ram=([0-9A-F]+)', stacks)[1],16)-0x10000
        address = offset+int(marker[1],16)
        base, length, rva = next(r for r in ranges if r[0] <= address and address+4 <= r[0]+r[1])
        assert struct.unpack_from('<I',data,rva+address-base)[0] == int(marker[2],16), 'GPU memory contents not preserved'
    # The registry version is NOT pinned. It is bumped whenever the struct gains fields, and it has
    # already moved 1 -> 2 (the A2h NULL-slot latch) -> 3 (the slot write watch). Hard-coding it here
    # meant every bump silently broke this probe: the assertion failed with "missing registry" even
    # though the registry was present and healthy, which reads as a data problem rather than a stale
    # test. Accept any version the collector can print, and let the STRUCTURAL assertions below --
    # the registry memory being present in the dump and its first word reading 1 -- carry the check.
    match = re.search(r'GUEST_REGISTRY address=([0-9A-F]+) version=(\d+) claimed=(\d+) overflow=0', stacks)
    assert match, 'missing registry or registry overflow'
    assert int(match[2]) >= 1, 'registry version must be a positive integer'
    # Group indices shift with the version capture added above: 1 = address, 2 = version, 3 = claimed.
    address, count = int(match[1],16), int(match[3])
    assert count >= (1 if mode.startswith('gpu-') else 3 if mode=='deadlock' else 5 if mode=='healthy' else 2)
    # The registry struct BEGINS with `uint32 version`, so the first word in the dump IS the version
    # the collector printed. Asserting it equals the literal 1 was correct only while the version was
    # 1; it has since moved to 2 and then 3, so the check is now written against the version the
    # collector itself reported. That still proves the registry text and the registry MEMORY are the
    # same object -- which is the property this assertion exists to establish -- without pinning a
    # number that every struct change is entitled to move.
    expected_version = int(match[2])
    for base,length,rva in ranges:
        if base <= address and address+16 <= base+length:
            assert struct.unpack_from('<I',data,rva+address-base)[0] == expected_version
            break
    else: raise AssertionError('registry text exists but registry memory missing from dump')
    assert 'harness_probes.c:' in stacks or 'gpu_probes.c:' in stacks or mode == 'healthy', 'missing fixture source lines'
    if mode.startswith('gpu-'):
        gpu=json.loads((folder/'gpu-report.json').read_text())
        assert gpu['fixture'] and gpu['ack_enabled'] is False, 'fixture GPU ownership lost'
        assert gpu['snapshot_count'] >= 3
        snapshots=[json.loads(line) for line in (folder/'gpu-snapshots.jsonl').read_text().splitlines()]
        if mode=='gpu-unreadable':
            assert all(not s['model_snapshot_available'] for s in snapshots)
        elif mode=='gpu-mmio-lifecycle':
            assert snapshots[0]['model_snapshot_available']
            assert all(not s['model_snapshot_available'] for s in snapshots[1:])
            assert 'teardown=PASS' in (folder/'jsrf_run.log').read_text(errors='replace')
            return
        else:
            assert all(s['model_snapshot_available'] and not (s['model_snapshot_generation'] & 1) for s in snapshots)
        if mode=='gpu-mmio-owner':
            assert gpu['registers']['PCI_COMMAND']==0x1234567C
            assert 'actual-veh' in (folder/'jsrf_run.log').read_text(errors='replace')
            return
        expected_queue={'gpu-progress':'empty','gpu-stall':'decoded','gpu-corrupt':'truncated_packet','gpu-unreadable':'unavailable',
                        'gpu-submit-supported':'empty','gpu-submit-bound':'empty','gpu-submit-blocked':'decoded'}
        assert gpu['queue']['status']==expected_queue[mode], (folder,gpu['queue'])
        if mode=='gpu-progress':
            assert gpu['progress']=='pointer_changes_observed'
            assert [o['get'] for o in gpu['observations'][:3]]==[gpu['observations'][0]['get']+n for n in (0,8,16)]
        if mode in ('gpu-stall','gpu-corrupt'): assert gpu['progress']=='pending_unchanged'
        if mode in ('gpu-stall','gpu-unreadable'):
            assert 'gpu_probe_wait' in stacks and 'eax=47505557' in stacks
        if mode=='gpu-unreadable': assert all(v is None for v in gpu['registers'].values())
        if mode in ('gpu-submit-supported','gpu-submit-bound','gpu-submit-blocked'):
            log=(folder/'jsrf_run.log').read_text(errors='replace')
            assert f'GPU_SUBMISSION mode={mode} aperture=PAGE_NOACCESS' in log
            if mode=='gpu-submit-supported':
                assert 'diag=ok sink=1 successes=1 atomic=accepted' in log
                assert gpu['registers']['USER_DMA_GET']==gpu['registers']['USER_DMA_PUT']
            elif mode=='gpu-submit-bound':
                assert 'diag=ok sink=3 successes=1 atomic=accepted' in log
                assert gpu['registers']['USER_DMA_GET']==gpu['registers']['USER_DMA_PUT']
            else:
                assert 'diag=unsupported_method sink=0 successes=0 atomic=blocked-unchanged' in log
                assert 'GPU_UNSUPPORTED subchannel=5 method=00000180 param=ABCDEF01' in log
                assert gpu['registers']['USER_DMA_GET']!=gpu['registers']['USER_DMA_PUT']
    if mode=='worker-crash':
        assert 6 in streams, 'missing exception stream'
        assert 'probe_worker_fault' in stacks and ' FAULT' in stacks
        assert 'eax=A0000000' in stacks and 'GS ' in stacks
    if mode=='deadlock':
        assert stacks.count('probe_worker+') >= 2
        assert stacks.count('kind=5 ') >= 4 and stacks.count('kind=6 ') >= 2
    if mode=='spin': assert 'probe_worker+' in stacks and 'eax=A0000000' in stacks
    if mode=='healthy':
        assert 'kind=10 ' in stacks and 'kind=11 ' in stacks, 'missing event signal/reset history'
        assert re.search(r'kind=9 .*value=00000102',stacks), 'missing timeout result'
        assert stacks.count('state=2 ') == 4, 'expected four exited guest workers'
        for block in stacks.split('GUEST_THREAD ')[1:]:
            if 'state=2 ' not in block.splitlines()[0]: continue
            identity = int(re.search(r'identity=(\d+)',block)[1])
            expected = 0xA0000000 + identity - 2
            events = re.findall(r'kind=1 target=([0-9A-F]+)',block)
            assert len(events) == 127 and all(int(x,16)==expected for x in events), 'mixed thread histories'

def delivery_fixture(case, expect_decision, expect_cause, **fields):
    """Drive the collector's REAL delivery decision for one injected case.

    The decision semantics live in the collector (a2h_delivery_terminal), so this launches the actual
    jsrf_collect.exe rather than reimplementing the decision in Python -- a reimplementation would
    test itself, not the instrument. Each case gets its own process because the install-hit latch is
    write-once and the counters accumulate.

    The point of the set is the packet's losslessness clause: a reader must be able to tell NON-FIRING
    from NOT-RECORDED. `absent` and `filtered_only` are the two shapes a live run can have when no
    #DB arrives; `publication_failure` and `overflow` are the shapes that must NEVER be read as a
    negative. No assertion below reads a counter that no case exercises.
    """
    folder = Path(os.environ.get('TEMP') or '.')/f'a2h-delivery-{case}'
    folder.mkdir(parents=True, exist_ok=True)
    for stale in ('stacks.txt',):
        (folder/stale).unlink(missing_ok=True)
    env = dict(os.environ, JSRF_TRACE_A2H_DR='1')
    command = [str(BUILD/'Release/jsrf_collect.exe'), '1', str(folder),
               str(BUILD/'Release/jsrf_recomp.exe'), f'--a2h-delivery-fixture={case}']
    completed = subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=60,env=env)
    assert completed.returncode == 0, (case,completed.returncode,completed.stderr)
    text = (folder/'stacks.txt').read_text(errors='replace')
    terminal = next((l for l in text.splitlines() if l.startswith('GUEST_DR_DELIVERY_TERMINAL ')), None)
    cause = next((l for l in text.splitlines() if l.startswith('GUEST_DR_CAUSE ')), None)
    assert terminal, f'{case}: no terminal delivery record was published'
    assert cause, f'{case}: no cause record was published'
    assert f'decision={expect_decision} ' in terminal+' ', (case,terminal)
    assert f'cause={expect_cause} ' in cause+' ', (case,cause)
    for name,value in fields.items():
        assert f'{name}={value} ' in terminal+' ' or terminal.endswith(f'{name}={value}'), \
            (case,name,value,terminal)
    # A cause must never be named from an incomplete ledger: that is the difference between a
    # discrimination and a guess.
    if 'complete=0' in terminal:
        assert expect_cause == 'UNKNOWN_NOT_RECORDED' or expect_decision == 'HIT', (case,terminal)
    print(f'PASS delivery-{case}: {expect_decision}/{expect_cause}',flush=True)
    return terminal

def delivery_fixtures():
    """The five required cases plus the two loss modes that must never read as a negative."""
    # 1. an install event WITH a hit -> the write-once latch, and a claimed single-step
    delivery_fixture('hit','HIT','HIT_OBSERVED',install_hit='1',raw_single_step='1',
                     ss_first_chance='1',ss_routed_handler='1',post_continue_ok='1')
    # 2. an install event with an INTENTIONALLY ABSENT hit -> the NON-FIRING world
    delivery_fixture('absent','NON_FIRING','NO_RAW_EVENT',install_hit='0',raw_single_step='0',
                     complete='1',post_continue_ok='1',post_continue_reverted='0')
    # 2b. the live run's actual shape: the stream WAS read (non-single-step exceptions counted) while
    #     no single-step ever arrived. This must still be NON_FIRING -- if unrelated exceptions were
    #     folded into raw_single_step it would read DELIVERED_UNCLAIMED and the Phase-1 answer would
    #     be destroyed, so the separation is asserted, not assumed.
    delivery_fixture('filtered_only','NON_FIRING','NO_RAW_EVENT',raw_single_step='0',
                     other_code_exceptions='2',complete='1')
    # 3. first vs second chance
    delivery_fixture('second_chance','DELIVERED_UNCLAIMED','CHANCE_SEMANTICS',ss_first_chance='0',
                     ss_second_chance='1',ss_routed_terminal_second='1')
    # 4. a filtered / swallowed event: delivered, then consumed by a path that is not this one
    delivery_fixture('swallowed','DELIVERED_UNCLAIMED','DEBUGGER_SWALLOW',ss_routed_handler='0',
                     ss_routed_generic_first='1',complete='1')
    # 5. publication failure -- nothing counted at all, so a zero carries NO information
    delivery_fixture('publication_failure','UNKNOWN_NOT_RECORDED','UNKNOWN_NOT_RECORDED',
                     raw_events='0',complete='0')
    # 5b. publication failure by truncation -- the ledger overflowed and says so
    delivery_fixture('overflow','UNKNOWN_NOT_RECORDED','UNKNOWN_NOT_RECORDED',ss_overflow='1',
                     complete='0',ss_records='32')

def run(mode, repetition=0):
    expected_checkpoint = 'probe_gpu' if mode.startswith('gpu-') else 'probe_video' if mode=='video' else 'probe_events' if mode=='healthy' else 'probe_handled' if mode=='handled' else 'probe_dispatch' if mode=='dispatch-race' else 'probe_started'
    command = [sys.executable, '-X', 'utf8', str(ROOT/'scripts/run-jsrf.py'),
               '--seconds', '2', '--label', f'test-{mode}-{repetition}',
               '--probe', mode, '--expect-checkpoint', expected_checkpoint]
    completed = subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=70)
    match = re.search(r'Artifacts: (.+)',completed.stdout)
    if not match: raise AssertionError(completed.stdout+completed.stderr)
    folder = Path(match[1].strip())
    result = json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
    expected = 'normal_exit' if mode in ('healthy','handled','dispatch-race','video','gpu-progress','gpu-corrupt','gpu-mmio-owner','gpu-mmio-lifecycle','gpu-ptimer-runtime','gpu-submit-supported','gpu-submit-bound','gpu-submit-blocked') else 'unhandled_exception' if mode=='worker-crash' else 'diagnostic_deadline'
    assert result['outcome']==expected and result['checkpoints_passed'], (folder,result)
    assert (completed.returncode==0) == (expected=='normal_exit'), 'runner exit misclassification'
    assert result.get('gpu_snapshots_dropped',0)==0, 'lost GPU snapshots'
    if result.get('gpu_snapshots',0): assert result['gpu_report_ok'], 'GPU report failed'
    if mode not in ('handled','dispatch-race','video','gpu-ptimer-runtime'):
        assert result['dump_ok'] and result['named_frames']>0, (folder,result)
        check_dump(folder,mode)
    if expected=='normal_exit': assert result['exit_code']==0
    print(f'PASS {mode}: {folder}',flush=True)
    return str(folder)

if __name__=='__main__':
    folders=[]
    # The A2h delivery fixtures run FIRST: they are cheap, they need no game launch, and a failure
    # here means the delivery decision itself is unsound -- which would make every later observation
    # uninterpretable.
    delivery_fixtures()
    for i in range(3): folders.append(run('healthy',i))
    for mode in ('dispatch-race','handled','worker-crash','deadlock','spin','video','gpu-mmio-owner','gpu-mmio-lifecycle','gpu-ptimer-runtime','gpu-submit-supported','gpu-submit-bound','gpu-submit-blocked','gpu-progress','gpu-stall','gpu-corrupt','gpu-unreadable'): folders.append(run(mode))
    (ROOT/'logs/harness-test-results.json').write_text(json.dumps({'passed':True,'runs':folders},indent=2))
    print('PASS: 19 harness probes; trapped USER submission, MMIO ownership/lifecycle, PTIMER runtime, thread capture, GPU history/decoding, missing memory and stalled queues verified')
    print('PASS: 7 A2h delivery fixtures; NON-FIRING distinguished from NOT-RECORDED, first/second chance, swallow and publication failure verified')
