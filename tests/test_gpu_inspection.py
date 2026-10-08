"""Offline decoder/capture regressions with independent malformed input cases."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from jsrf_dump import CaptureError, DumpMemory, dump_ranges
import jsrf_gpu
from jsrf_gpu import decode_queue
# Resolved at call time so a missing name fails only the tests that use it.
def load_method_table(*a,**k): return jsrf_gpu.load_method_table(*a,**k)
def missing_methods(*a,**k): return jsrf_gpu.missing_methods(*a,**k)
def predicted_walk_diagnostic(*a,**k): return jsrf_gpu.predicted_walk_diagnostic(*a,**k)
def analyze(*a,**k): return jsrf_gpu.analyze(*a,**k)

REAL_TABLE=Path.home()/'src'/'xboxrecomp'/'src'/'nv2a'/'nv2a_method_table.c'
SYNTHETIC_TABLE='''
#define FOO_CLASS 0x11u
#define BAR_CLASS 0x22u
static const uint16_t g_methods_foo[] = {
    0x0100u, 0x0200u,
    0x0204u,
};
static const uint16_t g_methods_bar[] = {
    0x0300u,
};
bool nv2a_method_implemented(uint32_t class_id, uint32_t method)
{
    switch (class_id) {
    case FOO_CLASS:
        return in_table(g_methods_foo,
                        sizeof(g_methods_foo) / sizeof(uint16_t), method);
    case BAR_CLASS:
        return in_table(g_methods_bar,
                        sizeof(g_methods_bar) / sizeof(uint16_t), method);
    default:
        return false;
    }
}
'''


class QueueTests(unittest.TestCase):
    base=0x80001000
    def decode(self, words, get=0, put=None, **limits):
        def read(va):
            i=(va-self.base)//4
            if i>=len(words) or words[i] is None: raise CaptureError('test memory unavailable')
            return words[i]
        return decode_queue(read,self.base,self.base+len(words)*4,self.base+get,
                            self.base+(len(words)*4 if put is None else put),**limits)
    def test_methods(self):
        r=self.decode([0x00080200,11,22,0x400837fc,33,44,0],put=24)
        self.assertEqual(r['status'],'decoded')
        self.assertEqual([(p['method'],p['subchannel'],p['data']) for p in r['packets']],[(0x200,0,[11,22]),(0x17fc,1,[33,44])])
    def test_wrap(self):
        r=self.decode([11,22,0,0x00080200],get=12,put=8)
        self.assertEqual(r['status'],'decoded'); self.assertEqual(r['packets'][0]['data'],[11,22])
    def test_truncated_at_put(self):
        r=self.decode([0x000C0200,11,22,33],put=8)
        self.assertEqual(r['status'],'truncated_packet'); self.assertEqual(r['packets'][0]['data'],[11])
    def test_missing_memory(self): self.assertEqual(self.decode([0x00040200,None,0],put=8)['status'],'missing_memory')
    def test_reserved(self): self.assertEqual(self.decode([0xE0000000,0],put=4)['status'],'reserved_opcode')
    def test_return_without_call(self): self.assertEqual(self.decode([0x00020000,0],put=4)['status'],'unexpected_return')
    def test_jump_cycle(self): self.assertEqual(self.decode([0x1001,0],put=4)['status'],'control_flow_loop')
    def test_old_jump(self):
        r=self.decode([0x20001008,0xE0000000,0x00040200,42,0],put=16)
        self.assertEqual(r['status'],'decoded'); self.assertEqual(r['packets'][-1]['data'],[42])
    def test_call_return(self):
        r=self.decode([0x1012,0x00040200,7,0,0x00040204,8,0x00020000,0],put=12)
        self.assertEqual(r['status'],'decoded'); self.assertEqual([p['kind'] for p in r['packets']],['call','incrementing','return','incrementing'])
    def test_nested_call(self): self.assertEqual(self.decode([0x100a,0,0x1002,0],put=4)['status'],'nested_call')
    def test_bad_jump(self): self.assertEqual(self.decode([0x80000001,0],put=4)['status'],'invalid_target')
    def test_budgets(self):
        self.assertEqual(self.decode([0,0,0],put=8,max_words=1)['status'],'budget_exhausted')
        self.assertEqual(self.decode([0,0,0],put=8,max_packets=1)['status'],'budget_exhausted')
    def test_bad_bounds(self): self.assertEqual(decode_queue(None,1,16,4,8)['status'],'invalid_bounds')
    def test_unreadable(self): self.assertEqual(decode_queue(None,None,16,4,8)['status'],'unavailable')
    def test_unsubmitted(self): self.assertEqual(decode_queue(None,self.base,self.base+32,0x80000000,0x80000000)['status'],'submission_unestablished')
    def test_outside_pointer(self): self.assertEqual(decode_queue(None,self.base,self.base+32,self.base+64,self.base+4)['status'],'invalid_pointer')


class WalkDefaultTests(unittest.TestCase):
    base=0x80001000
    def run_stream(self, repeats):
        words=[0x00500200]+list(range(20))
        words=words*repeats; put=self.base+len(words)*4
        words.append(0)   # spare slot so PUT differs from the ring end (end == start would read as empty)
        def read(va): return words[(va-self.base)//4]
        return decode_queue(read,self.base,put+4,self.base,put)
    def test_long_stream_under_word_budget_decodes(self):
        r=self.run_stream(100)   # 2100 words, 100 packets
        self.assertEqual(r['status'],'decoded'); self.assertEqual(r['words_read'],2100)
    def test_stream_over_word_budget_exhausts(self):
        self.assertEqual(self.run_stream(200)['status'],'budget_exhausted')   # 4200 words


class MethodTableTests(unittest.TestCase):
    def test_synthetic_table(self):
        with tempfile.TemporaryDirectory() as name:
            path=Path(name)/'table.c'; path.write_text(SYNTHETIC_TABLE)
            table=load_method_table(path)
        self.assertEqual(table,{0x11:{0x100,0x200,0x204},0x22:{0x300}})
    def test_missing_path(self):
        with tempfile.TemporaryDirectory() as name:
            with self.assertRaises(FileNotFoundError): load_method_table(Path(name)/'absent.c')
    @unittest.skipUnless(REAL_TABLE.exists(),'toolkit method table not present')
    def test_real_table(self):
        table=load_method_table(REAL_TABLE)
        self.assertIn(0x1720,table[0x97]); self.assertIn(0x0180,table[0x39])
        self.assertNotIn(0x030C,table[0x39])
    def test_default_bindings(self):
        self.assertEqual(jsrf_gpu.DEFAULT_SUBCHANNEL_CLASS,{0:0x97,1:0x39,2:0x9F,3:0x62})


TABLE={0x97:{0x1720,0x1724,0x1730},0x39:{0x0180},0x9F:{0x0300},0x62:set()}

def pkt(va,sub,method,count=1,kind='incrementing'):
    return dict(va=va,header=0,kind=kind,subchannel=sub,method=method,count=count,data=[0]*count)


class MissingMethodTests(unittest.TestCase):
    def test_one_listed_one_unlisted(self):
        r=missing_methods([pkt(0x80001000,0,0x1720),pkt(0x80001008,1,0x030C)],TABLE)
        self.assertEqual(len(r),1)
        self.assertEqual((r[0]['va'],r[0]['subchannel'],r[0]['class_id'],r[0]['method'],r[0]['occurrences'],r[0]['reason']),
                         (0x80001008,1,0x39,0x030C,1,'not_in_table'))
    def test_incrementing_middle_method(self):
        r=missing_methods([pkt(0x80001000,0,0x1720,count=3)],TABLE)   # 1720 ok, 1724 ok, 1728 missing
        self.assertEqual([e['method'] for e in r],[0x1728])
        TABLE2={0x97:{0x1720,0x1728}}
        r=missing_methods([pkt(0x80001000,0,0x1720,count=3)],TABLE2)
        self.assertEqual([e['method'] for e in r],[0x1724])
    def test_non_incrementing_only_first_method(self):
        r=missing_methods([pkt(0x80001000,0,0x1720,count=3,kind='non_incrementing')],{0x97:{0x1720}})
        self.assertEqual(r,[])
    def test_exempt_methods(self):
        r=missing_methods([pkt(0x80001000,1,0x0100),pkt(0x80001008,1,0x0000)],{0x39:set()})
        self.assertEqual(r,[])
    def test_set_object_makes_class_unknown(self):
        r=missing_methods([pkt(0x80001000,2,0x0000),pkt(0x80001008,2,0x0300)],TABLE)
        self.assertEqual(len(r),1)
        self.assertIsNone(r[0]['class_id']); self.assertEqual(r[0]['reason'],'binding_unknown')
        self.assertEqual((r[0]['subchannel'],r[0]['method']),(2,0x0300))
    def test_set_object_only_affects_its_subchannel(self):
        r=missing_methods([pkt(0x80001000,2,0x0000),pkt(0x80001008,0,0x1720)],TABLE)
        self.assertEqual(r,[])
    def test_dedupe_counts_occurrences_in_first_seen_order(self):
        packets=[pkt(0x80001000,1,0x030C),pkt(0x80001008,1,0x0310),pkt(0x80001010,1,0x030C),pkt(0x80001018,1,0x030C)]
        r=missing_methods(packets,TABLE)
        self.assertEqual([(e['method'],e['occurrences'],e['va']) for e in r],[(0x030C,3,0x80001000),(0x0310,1,0x80001008)])


class PredictedDiagnosticTests(unittest.TestCase):
    def q(self,status): return dict(status=status,packets=[])
    def test_missing_wins(self):
        self.assertEqual(predicted_walk_diagnostic(self.q('decoded'),[dict(method=1)]),'unsupported_method')
        self.assertEqual(predicted_walk_diagnostic(self.q('reserved_opcode'),[dict(method=1)]),'unsupported_method')
    def test_clean(self):
        for status in ('decoded','empty'): self.assertEqual(predicted_walk_diagnostic(self.q(status),[]),'ok')
    def test_no_table_is_never_ok(self):
        self.assertEqual(predicted_walk_diagnostic(self.q('decoded'),None),'unknown')
    def test_status_mapping(self):
        for status in ('budget_exhausted','control_flow_loop','reserved_opcode','invalid_target',
                       'truncated_packet','method_range_overflow'):
            self.assertEqual(predicted_walk_diagnostic(self.q(status),[]),status)
        self.assertEqual(predicted_walk_diagnostic(self.q('nested_call'),[]),'control_flow_loop')
        self.assertEqual(predicted_walk_diagnostic(self.q('unexpected_return'),[]),'invalid_target')
        self.assertEqual(predicted_walk_diagnostic(self.q('missing_memory'),[]),'unreadable_pushbuffer')
        self.assertEqual(predicted_walk_diagnostic(self.q('something_else'),[]),'unknown')
    def test_real_decode_statuses(self):
        d=QueueTests().decode
        self.assertEqual(predicted_walk_diagnostic(d([0xE0000000,0],put=4),[]),'reserved_opcode')
        self.assertEqual(predicted_walk_diagnostic(d([0x100a,0,0x1002,0],put=4),[]),'control_flow_loop')


class AnalyzeTests(unittest.TestCase):
    begin,end=0x80001000,0x80002000
    def folder(self, directory, get, put):
        snaps=[dict(version=1,index=i,tag='t',tick_ms=i,reason='r',tid=1,
                    registers=dict(USER_DMA_GET=get,USER_DMA_PUT=put),
                    device=dict(push_begin=self.begin,push_end=self.end)) for i in range(2)]
        (directory/'gpu-snapshots.jsonl').write_text('\n'.join(json.dumps(s) for s in snaps)+'\n')
    def test_default_toolkit_is_sibling(self):
        import inspect
        default=inspect.signature(jsrf_gpu.analyze).parameters['toolkit'].default
        self.assertTrue(default is None or Path(default)==Path(jsrf_gpu.__file__).resolve().parents[2]/'xboxrecomp')
    def test_ring_end_normalised_to_start(self):
        with tempfile.TemporaryDirectory() as name:
            folder=Path(name); self.folder(folder,self.end-0x80000000,self.begin-0x80000000)
            r=analyze(folder,toolkit=folder/'absent')
        self.assertNotEqual(r['progress'],'pending_unchanged'); self.assertEqual(r['progress'],'equal_unchanged')
    def test_real_pending_still_pending(self):
        with tempfile.TemporaryDirectory() as name:
            folder=Path(name); self.folder(folder,0x1000,0x1100)
            r=analyze(folder,toolkit=folder/'absent')
        self.assertEqual(r['progress'],'pending_unchanged')
    def test_absent_table_is_not_nothing_missing(self):
        with tempfile.TemporaryDirectory() as name:
            folder=Path(name); self.folder(folder,0x1000,0x1100)
            r=analyze(folder,toolkit=folder/'absent')
        self.assertIsNone(r['missing_methods'])
        self.assertTrue(any('method table' in w for w in r['warnings']))
        self.assertIn('method_table',r); self.assertNotEqual(r['predicted_diagnostic'],'ok')
    @unittest.skipUnless(REAL_TABLE.exists(),'toolkit method table not present')
    def test_table_present_reports_list_and_path(self):
        with tempfile.TemporaryDirectory() as name:
            folder=Path(name); self.folder(folder,0x1000,0x1100)
            r=analyze(folder,toolkit=REAL_TABLE.parents[2])
        self.assertIsInstance(r['missing_methods'],list)
        self.assertIn('path',r['method_table']); self.assertIn('nv2a_method_table.c',str(r['method_table']['path']))


class DumpTests(unittest.TestCase):
    def fixture(self, directory):
        # One MemoryList with adjacent fragments and a distinct high allocation.
        records=[(0x11000,b'ab'),(0x11002,b'cd'),(0x80011000,b'GPU!')]
        data=bytearray(32+12+4+len(records)*16)
        data[:4]=b'MDMP'; struct.pack_into('<II',data,8,1,32)
        struct.pack_into('<III',data,32,5,4+len(records)*16,44)
        struct.pack_into('<I',data,44,len(records))
        for i,(base,content) in enumerate(records):
            struct.pack_into('<QII',data,48+16*i,base,len(content),len(data))
            data+=content
        (directory/'process.dmp').write_bytes(data)
        (directory/'stacks.txt').write_text('DUMP guest_ram=0000000000020000+67043328\n')
        return data
    def test_split_and_distinct_allocations(self):
        with tempfile.TemporaryDirectory() as name:
            folder=Path(name); self.fixture(folder)
            with DumpMemory(folder) as memory:
                self.assertEqual(memory.read(0x1000,4),b'abcd')
                self.assertEqual(memory.read(0x80001000,4),b'GPU!')
                self.assertEqual(memory.read(0x1001,3),b'bcd')
                with self.assertRaisesRegex(CaptureError,'absent'): memory.read(0x2000,4)
                with self.assertRaises(CaptureError): memory.read(0xFFFFFFFF,4)
    def test_truncated(self):
        with tempfile.TemporaryDirectory() as name:
            data=self.fixture(Path(name))
            with self.assertRaises(CaptureError): dump_ranges(data[:-1])
    def test_bad_header(self):
        with self.assertRaises(CaptureError): dump_ranges(b'not a dump')
    def test_bad_stream_count(self):
        data=bytearray(32); data[:4]=b'MDMP'; struct.pack_into('<II',data,8,0xffffffff,32)
        with self.assertRaises(CaptureError): dump_ranges(data)



class SubmitStateDecodeTests(unittest.TestCase):
    """`g_nv2a_submit_state`: the base 12 u32 fields, plus the budget transcript.

    The struct GREW when the budget transcript was added, so the decoder must not
    assume one layout. Reading the full size from an archive built BEFORE the
    change does not fail -- it reads whatever globals follow and reports them as a
    transcript. That happened for real: a pre-change archive reported
    `budget_count = 44040355`, unrelated memory presented as a packet parameter
    count. These tests pin both layouts and the size selection that separates them.
    """
    FIELDS=('generation','diag','method','subchannel','param','at','get','put',
            'successes','rejections','consecutive_rejections','admitted_unknown')
    BUDGET_FIELDS=('budget_stops','budget_local_pc','budget_words','budget_packets',
                   'budget_count','budget_method','budget_ret','budget_in_param',
                   'budget_at_packet_limit')
    def pack(self,generation=2,diag=11):
        """The OLD (pre-transcript) 12-field layout."""
        return struct.pack('<12I',generation,diag,0xF40,1,0xDEADBEEF,0x1800,0x1000,
                           0x2000,7,300,299,4)
    def pack_full(self,generation=2,diag=5):
        """The NEW layout: base fields followed by the budget transcript."""
        return self.pack(generation,diag)+struct.pack(
            '<9I',1,0x4D4F4,4096,233,4,0x1760,0,1,0)
    def test_fields_and_diag_name(self):
        state=jsrf_gpu.decode_submit_state(self.pack())
        self.assertEqual([state[k] for k in self.FIELDS],
                         [2,11,0xF40,1,0xDEADBEEF,0x1800,0x1000,0x2000,7,300,299,4])
        self.assertEqual(state['diag_name'],'unsupported_method')
        self.assertIs(state['torn'],False)
    def test_odd_generation_is_torn(self):
        self.assertIs(jsrf_gpu.decode_submit_state(self.pack(generation=3))['torn'],True)
    def test_unknown_code(self):
        self.assertEqual(jsrf_gpu.decode_submit_state(self.pack(diag=999))['diag_name'],'unknown')
    def test_ok_code(self):
        self.assertEqual(jsrf_gpu.decode_submit_state(self.pack(diag=0))['diag_name'],'ok')
    def test_short_input(self):
        with self.assertRaises(ValueError): jsrf_gpu.decode_submit_state(self.pack()[:47])
    def test_extra_trailing_bytes_are_ignored(self):
        self.assertEqual(jsrf_gpu.decode_submit_state(self.pack()+b'\0'*8)['put'],0x2000)

    def test_old_layout_reports_no_transcript(self):
        """A pre-change archive must NOT be credited with transcript fields."""
        state=jsrf_gpu.decode_submit_state(self.pack())
        self.assertIs(state['has_budget_transcript'],False)
        for field in self.BUDGET_FIELDS:
            self.assertNotIn(field,state,
                             'a 12-field buffer must not yield %s' % field)
    def test_new_layout_decodes_the_transcript(self):
        state=jsrf_gpu.decode_submit_state(self.pack_full())
        self.assertIs(state['has_budget_transcript'],True)
        self.assertEqual([state[k] for k in self.BUDGET_FIELDS],
                         [1,0x4D4F4,4096,233,4,0x1760,0,1,0])
    def test_field_count_matches_the_size_constants(self):
        """The sizes are derived from the field tuples, so they cannot drift.

        The struct has grown in stages (base -> budget transcript -> unit
        accounting -> continuation audit -> vblank audit) and each stage is a
        separately declared tier, so the check is that the tiers are a
        partition of the full field list: appending a field to the full tuple
        without adding it to a tier, or vice versa, is what this catches.
        """
        self.assertEqual(jsrf_gpu.SUBMIT_STATE_BASE_SIZE,
                         4*len(jsrf_gpu.SUBMIT_STATE_BASE_FIELDS))
        self.assertEqual(jsrf_gpu.SUBMIT_STATE_SIZE,
                         4*len(jsrf_gpu.SUBMIT_STATE_FIELDS))
        tiers = (jsrf_gpu.SUBMIT_STATE_BASE_FIELDS
                 + jsrf_gpu.SUBMIT_STATE_BUDGET_FIELDS
                 + jsrf_gpu.SUBMIT_STATE_UNIT_FIELDS
                 + jsrf_gpu.SUBMIT_STATE_CONTINUATION_FIELDS
                 + jsrf_gpu.SUBMIT_STATE_VBLANK_FIELDS)
        self.assertEqual(list(tiers), list(jsrf_gpu.SUBMIT_STATE_FIELDS),
                         'the tiers must concatenate to the full field list, '
                         'in struct order')
        self.assertEqual(len(tiers), len(set(tiers)),
                         'a field appears in two tiers')


class SubmitStateSizeTests(unittest.TestCase):
    """The archive's own linker map states how big its struct is.

    This is the regression that mattered: the reader asked for the CURRENT size
    unconditionally, so a pre-change archive had adjacent globals read as
    transcript fields. The map is the right source because it is present in every
    archive and describes the build that produced it -- unlike a source-hash
    lookup, which finds nothing for a run built from a dirty tree.
    """
    def _map(self, gap):
        # Two symbols `gap` bytes apart, in the map's own column format.
        return (' Preferred load address is 140000000\n'
                ' 0001:00000000       g_nv2a_submit_state      0000000142687500     a.obj\n'
                ' 0001:00000000       g_after_state            %016X     a.obj\n'
                % (0x142687500+gap))
    def _size(self, gap):
        import tempfile, os
        fd,path=tempfile.mkstemp(suffix='.map'); os.close(fd)
        try:
            with open(path,'w',encoding='utf-8') as f: f.write(self._map(gap))
            return jsrf_gpu._submit_state_declared_size(path)
        finally: os.unlink(path)
    def test_old_archive_declares_the_base_size(self):
        self.assertEqual(self._size(64),64)
    def test_new_archive_declares_the_full_size(self):
        self.assertEqual(self._size(96),96)
    def test_absent_symbol_is_none(self):
        import tempfile, os
        fd,path=tempfile.mkstemp(suffix='.map'); os.close(fd)
        try:
            with open(path,'w',encoding='utf-8') as f:
                f.write(' Preferred load address is 140000000\n')
            self.assertIsNone(jsrf_gpu._submit_state_declared_size(path))
        finally: os.unlink(path)

    def test_size_decision_uses_the_declared_size(self):
        """The DECISION is what regressed, so test it directly.

        A decoder test cannot catch this: the decoder is handed a length and
        decodes it. The bug was in CHOOSING the length, so an archive declaring 64
        bytes must yield the base size even though this checkout's struct is
        larger.
        """
        w=[]
        self.assertEqual(jsrf_gpu.submit_state_size_for_archive(64,w),
                         jsrf_gpu.SUBMIT_STATE_BASE_SIZE)
        self.assertEqual(w,[])
        # A 96-byte archive is the UNIT-accounting layout (base 48 + budget
        # transcript 36 + units 12). It must yield that tier, not the current
        # full size: reading the newer continuation/vblank fields out of a
        # struct that predates them would decode whatever globals follow as
        # audit data -- the exact regression this decision exists to prevent.
        w=[]
        self.assertEqual(jsrf_gpu.submit_state_size_for_archive(96,w),
                         jsrf_gpu.SUBMIT_STATE_UNIT_SIZE)
        self.assertEqual(jsrf_gpu.SUBMIT_STATE_UNIT_SIZE, 96)
        self.assertEqual(w,[])
        # And the current layout yields the full size.
        w=[]
        self.assertEqual(
            jsrf_gpu.submit_state_size_for_archive(jsrf_gpu.SUBMIT_STATE_SIZE, w),
            jsrf_gpu.SUBMIT_STATE_SIZE)
        self.assertEqual(w,[])
    def test_unstated_size_falls_back_to_the_base_layout(self):
        w=[]
        self.assertEqual(jsrf_gpu.submit_state_size_for_archive(None,w),
                         jsrf_gpu.SUBMIT_STATE_BASE_SIZE)
        self.assertTrue(any('not stated' in m for m in w))
    def test_too_small_is_refused_not_truncated(self):
        w=[]
        self.assertIsNone(jsrf_gpu.submit_state_size_for_archive(16,w))
        self.assertTrue(any('smaller than' in m for m in w))
    def test_end_to_end_old_archive_yields_no_transcript(self):
        """The regression, end to end: a 64-byte declaration must not yield fields."""
        w=[]
        size=jsrf_gpu.submit_state_size_for_archive(64,w)
        state=jsrf_gpu.decode_submit_state(struct.pack('<%dI'%len(jsrf_gpu.SUBMIT_STATE_BASE_FIELDS),
                                                       2,5,0,0,0,0x494F4,0x494F4,0x4E680,3519,1,1,0)[:size])
        self.assertIs(state['has_budget_transcript'],False)
        self.assertNotIn('budget_count',state)


class ReadSubmitStateIntegrationTests(unittest.TestCase):
    """`read_submit_state` on REAL archived dumps, which is where the bug lived.

    The decoder tests cannot catch the original defect: the decoder is handed a
    length and decodes it faithfully. The bug was that `read_submit_state` asked
    for the CURRENT struct size from an archive built BEFORE the struct grew, so
    the extra bytes came from whatever globals follow `g_nv2a_submit_state`. On the
    real pre-change archive that produced `budget_count = 44040355`.

    These tests go through `read_submit_state` itself, so a regression in the size
    CHOICE fails them. They skip when the archives are not present (they are
    gitignored evidence, not part of a fresh checkout).
    """
    RUNS=Path(__file__).resolve().parents[1]/'logs'/'runs'
    OLD='20261006-213505-255-title005-admit3'      # built before the transcript
    NEW='20261007-014848-201-20261007-title006-budgetcatch'  # built after

    def _read(self, run):
        folder=self.RUNS/run
        if not (folder/'process.dmp').exists() or not (folder/'jsrf_recomp.map').exists():
            self.skipTest('archive %s is not present' % run)
        w=[]
        return jsrf_gpu.read_submit_state(folder,w),w

    def test_pre_change_archive_reports_no_transcript(self):
        state,w=self._read(self.OLD)
        self.assertIsNotNone(state,'the pre-change archive must still decode its base fields')
        self.assertIs(state['has_budget_transcript'],False,
                      'a pre-change archive was credited with transcript fields')
        self.assertNotIn('budget_count',state,
                         'budget fields came from memory after the struct')
        self.assertEqual(state['diag_name'],'budget_exhausted')
        self.assertEqual(state['successes'],3519)

    def test_post_change_archive_reports_the_transcript(self):
        state,w=self._read(self.NEW)
        self.assertIsNotNone(state)
        self.assertIs(state['has_budget_transcript'],True)
        self.assertIn('budget_stops',state)


if __name__=='__main__': unittest.main()
