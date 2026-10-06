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
    """`g_nv2a_submit_state`: 12 little-endian u32 fields (NV2ASubmitState)."""
    FIELDS=('generation','diag','method','subchannel','param','at','get','put',
            'successes','rejections','consecutive_rejections','admitted_unknown')
    def pack(self,generation=2,diag=11):
        return struct.pack('<12I',generation,diag,0xF40,1,0xDEADBEEF,0x1800,0x1000,
                           0x2000,7,300,299,4)
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


if __name__=='__main__': unittest.main()
