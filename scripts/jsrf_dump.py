"""Bounded, read-only minidump memory access. Guest VAs retain their identity.

Host globals of the recompiled executable (counters such as
g_recomp_alias_icall_hits) are read by symbol: the archived linker map gives the
symbol's address at the preferred load base, and the dump's module list gives the
base the image actually loaded at. Writable globals are present only in dumps
written with MiniDumpWithDataSegs (tools/harness/collect.c); read-only data (const
tables in .rdata) is not in the dump at all and is read from the archived
executable instead, which holds exactly those bytes. Writable data is never read
from the file: there it holds initial values, not the frozen state.
"""
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


# A map line in either the public or the static section:
#   0003:00012340       g_recomp_alias_icall_hits   00000001425c3120     recomp_dispatch.obj
MAP_LINE = re.compile(r'^\s*[0-9A-Fa-f]{4}:[0-9A-Fa-f]{8}\s+(\S+)\s+([0-9A-Fa-f]{16})\b')
PREFERRED_LOAD = re.compile(r'Preferred load address is ([0-9A-Fa-f]+)')


def map_symbol(map_path, name):
    """(address at the preferred base, preferred base) of a symbol in a linker map."""
    text = Path(map_path).read_text(encoding='utf-8', errors='replace')
    preferred = PREFERRED_LOAD.search(text)
    if not preferred:
        raise CaptureError(f'{map_path} states no preferred load address')
    hits = {int(m[2], 16) for m in map(MAP_LINE.match, text.splitlines()) if m and m[1] == name}
    if not hits:
        raise CaptureError(f'{name} is not in {map_path}')
    if len(hits) > 1:
        raise CaptureError(f'{name} has {len(hits)} addresses in {map_path}')
    return hits.pop(), int(preferred[1], 16)


def image_bytes(exe_path, rva, length):
    """Bytes at an RVA of a PE image file, from a read-only, file-backed section only."""
    data = Path(exe_path).read_bytes()
    if data[:2] != b'MZ':
        raise CaptureError(f'{exe_path} is not a PE image')
    pe, = struct.unpack_from('<I', data, 0x3C)
    if data[pe:pe + 4] != b'PE\0\0':
        raise CaptureError(f'{exe_path} has no PE header')
    sections, optional_size = struct.unpack_from('<H', data, pe + 6)[0], \
        struct.unpack_from('<H', data, pe + 20)[0]
    table = pe + 24 + optional_size
    for i in range(sections):
        header = table + 40 * i
        vsize, va, raw_size, raw_ptr = struct.unpack_from('<IIII', data, header + 8)
        flags, = struct.unpack_from('<I', data, header + 36)
        if va <= rva and rva + length <= va + max(vsize, raw_size):
            if flags & 0x80000000:        # IMAGE_SCN_MEM_WRITE
                raise CaptureError(f'RVA 0x{rva:X} is in a writable section; its frozen value '
                                   f'is only in a dump, never in the image file')
            if rva + length > va + raw_size:
                raise CaptureError(f'RVA 0x{rva:X} is not file-backed in {exe_path}')
            start = raw_ptr + rva - va
            return data[start:start + length]
    raise CaptureError(f'RVA 0x{rva:X} is in no section of {exe_path}')


def module_bases(data, streams):
    """{lower-case module file name: (base, size)} from the ModuleListStream."""
    if 4 not in streams:
        raise CaptureError('dump has no module list')
    offset, size = streams[4]
    count, = struct.unpack_from('<I', data, offset)
    if 4 + count * 108 > size:
        raise CaptureError('truncated module list')
    modules = {}
    for i in range(count):
        entry = offset + 4 + i * 108
        base, image_size, _checksum, _stamp, name_rva = struct.unpack_from('<QIIII', data, entry)
        length, = struct.unpack_from('<I', data, name_rva)
        name = bytes(data[name_rva + 4:name_rva + 4 + length]).decode('utf-16-le', 'replace')
        modules[name.replace('/', '\\').rsplit('\\', 1)[-1].lower()] = (base, image_size)
    return modules


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
        return self._read(self.offset+va, length, lambda a: f'guest memory 0x{a-self.offset:08X}')

    def read_host(self, address, length):
        """Bytes at a host address, for the executable's own globals."""
        if address < 0 or length < 0 or length > 16*1024*1024:
            raise CaptureError('host read must be non-negative and within 16 MiB')
        return self._read(address, length, lambda a: f'host memory 0x{a:016X}')

    def host_symbol(self, map_path, name, length, module='jsrf_recomp.exe', image=None):
        """Bytes of a global of `module`, located through its linker map.

        A symbol absent from the dump is read from `image` (the archived
        executable) when it lies in a read-only section, and only then."""
        bases = module_bases(self.data, self.streams)
        if module.lower() not in bases:
            raise CaptureError(f'{module} is not in the dump module list')
        address, preferred = map_symbol(map_path, name)
        try:
            return self.read_host(bases[module.lower()][0] + address - preferred, length)
        except CaptureError:
            if image is None:
                raise
            return image_bytes(image, address - preferred, length)

    def _read(self, cursor, length, describe):
        result = bytearray()
        stop = cursor + length
        while cursor < stop:
            choices = [r for r in self.ranges if r[0] <= cursor < r[0]+r[1]]
            if not choices:
                raise CaptureError(f'{describe(cursor)} is absent from this dump')
            base, size, rva = max(choices, key=lambda r:r[0]+r[1])
            n = min(stop-cursor, base+size-cursor)
            result += self.data[rva+cursor-base:rva+cursor-base+n]
            cursor += n
        return bytes(result)

    def word(self, va): return struct.unpack('<I', self.read(va,4))[0]
