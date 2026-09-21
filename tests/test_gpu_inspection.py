"""Offline decoder/capture regressions with independent malformed input cases."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from jsrf_dump import CaptureError, DumpMemory, dump_ranges
from jsrf_gpu import decode_queue


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


if __name__=='__main__': unittest.main()
