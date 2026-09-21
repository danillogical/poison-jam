"""Bounded, read-only minidump memory access. Guest VAs retain their identity."""
import mmap
from pathlib import Path
import re
import struct


class CaptureError(ValueError):
    pass


def dump_ranges(data):
    def span(offset, length):
        if offset < 0 or length < 0 or offset+length > len(data):
            raise CaptureError('truncated minidump stream or memory range')
    span(0, 32)
    if data[:4] != b'MDMP':
        raise CaptureError('not a Windows minidump')
    count, directory = struct.unpack_from('<II', data, 8)
    if count > 4096:
        raise CaptureError('unreasonable minidump stream count')
    span(directory, count*12)
    streams, ranges = {}, []
    for i in range(count):
        kind, size, offset = struct.unpack_from('<III', data, directory+12*i)
        span(offset, size)
        streams[kind] = (offset, size)
        if kind == 5:
            if size < 4: raise CaptureError('truncated MemoryList')
            n, = struct.unpack_from('<I', data, offset)
            if 4+n*16 > size: raise CaptureError('truncated memory descriptors')
            for j in range(n):
                base, length, rva = struct.unpack_from('<QII', data, offset+4+16*j)
                span(rva, length)
                ranges.append((base, length, rva))
        elif kind == 9:
            if size < 16: raise CaptureError('truncated Memory64List')
            n, rva = struct.unpack_from('<QQ', data, offset)
            if 16+n*16 > size: raise CaptureError('truncated memory64 descriptors')
            for j in range(n):
                base, length = struct.unpack_from('<QQ', data, offset+16+16*j)
                span(rva, length)
                ranges.append((base, length, rva))
                rva += length
    return streams, sorted(ranges)


class DumpMemory:
    def __init__(self, folder):
        self.folder = Path(folder)
        self.file = (self.folder/'process.dmp').open('rb')
        try:
            self.data = mmap.mmap(self.file.fileno(), 0, access=mmap.ACCESS_READ)
            self.streams, self.ranges = dump_ranges(self.data)
            stacks = (self.folder/'stacks.txt').read_text(errors='replace')
            matches = re.findall(r'guest_ram=([0-9A-F]+)\+(\d+)', stacks)
            if not matches or int(matches[-1][1]) == 0:
                raise CaptureError('capture has no canonical guest mapping identity')
            self.offset = int(matches[-1][0],16)-0x10000
        except Exception:
            self.close()
            raise

    def close(self):
        if hasattr(self, 'data'): self.data.close()
        self.file.close()

    def __enter__(self): return self
    def __exit__(self, *args): self.close()

    def read(self, va, length):
        if va < 0 or length < 0 or va+length > 0x100000000 or length > 1024*1024:
            raise CaptureError('guest read must fit 32-bit VA space and the 1 MiB inspection limit')
        result = bytearray()
        cursor, stop = self.offset+va, self.offset+va+length
        while cursor < stop:
            choices = [r for r in self.ranges if r[0] <= cursor < r[0]+r[1]]
            if not choices:
                raise CaptureError(f'guest memory 0x{cursor-self.offset:08X} is absent from this dump')
            base, size, rva = max(choices, key=lambda r:r[0]+r[1])
            n = min(stop-cursor, base+size-cursor)
            result += self.data[rva+cursor-base:rva+cursor-base+n]
            cursor += n
        return bytes(result)

    def word(self, va): return struct.unpack('<I', self.read(va,4))[0]
