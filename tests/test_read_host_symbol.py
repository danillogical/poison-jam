"""Controls for reading the recompiled executable's globals from a dump.

The session that needed the folded-alias counters could not read them: the dump
held only guest regions, and host addresses had to be derived by hand from the
linker map and the module base. These cases build a small synthetic minidump --
a module list, two captured host ranges and a linker map -- and check that a
symbol is found at the loaded base, that `--alias-hits` lists exactly the called
aliases, and that a symbol whose page was not captured is reported as absent
rather than read as zero.
"""
from __future__ import annotations

import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from jsrf_dump import CaptureError, DumpMemory, map_symbol  # noqa: E402

SCRIPT = ROOT / 'scripts' / 'read-host-symbol.py'
PREFERRED = 0x140000000
BASE = 0x7FF700000000
ENTRIES_AT = PREFERRED + 0x100000
MAP_AT = PREFERRED + 0x100010
HITS_AT = PREFERRED + 0x200000
PAIRS = [(0x0014FEF0, 0x00150231), (0x00037550, 0x00038530), (0x00024700, 0x000246E0)]
HITS = [5, 0, 2]

MAP_TEXT = f''' jsrf_recomp

 Preferred load address is {PREFERRED:016x}

  Address         Publics by Value              Rva+Base               Lib:Object

 0003:00100000       g_recomp_alias_icall_entries {ENTRIES_AT:016x}     recomp_dispatch.obj
 0003:00100010       g_recomp_alias_icall_map   {MAP_AT:016x}     recomp_dispatch.obj

 Static symbols

 0003:00200000       g_recomp_alias_icall_hits  {HITS_AT:016x}     recomp_dispatch.obj
'''


def build_dump(path: Path, with_hits: bool = True) -> None:
    """MDMP with a ModuleListStream (4) and a Memory64ListStream (9)."""
    name = 'C:\\build\\Release\\jsrf_recomp.exe'.encode('utf-16-le')
    first = struct.pack('<I', len(PAIRS)).ljust(0x10, b'\0') + b''.join(
        struct.pack('<II', *pair) for pair in PAIRS)
    second = b''.join(struct.pack('<Q', h) for h in HITS)
    ranges = [(BASE + (ENTRIES_AT - PREFERRED), first)]
    if with_hits:
        ranges.append((BASE + (HITS_AT - PREFERRED), second))

    directory_at = 32
    modules_at = directory_at + 2 * 12
    module_name_at = modules_at + 4 + 108
    memory_at = module_name_at + 4 + len(name)
    memory_size = 16 + 16 * len(ranges)
    blob_at = memory_at + memory_size

    header = b'MDMP' + struct.pack('<IIIIIQ', 0xA793, 2, directory_at, 0, 0, 0)
    directory = (struct.pack('<III', 4, 4 + 108, modules_at)
                 + struct.pack('<III', 9, memory_size, memory_at))
    module = struct.pack('<QIIII', BASE, 0x3000000, 0, 0, module_name_at).ljust(108, b'\0')
    modules = struct.pack('<I', 1) + module
    module_name = struct.pack('<I', len(name)) + name
    memory = struct.pack('<QQ', len(ranges), blob_at) + b''.join(
        struct.pack('<QQ', start, len(data)) for start, data in ranges)
    blob = b''.join(data for _, data in ranges)
    path.write_bytes(header + directory + modules + module_name + memory + blob)


class Scratch:
    def __init__(self, with_hits: bool = True):
        self.with_hits = with_hits

    def __enter__(self) -> Path:
        self.tmp = tempfile.TemporaryDirectory()
        run = Path(self.tmp.name)
        build_dump(run / 'process.dmp', self.with_hits)
        (run / 'stacks.txt').write_text('DUMP ok=1 guest_ram=0000000200000000+4096\n')
        (run / 'jsrf_recomp.map').write_text(MAP_TEXT)
        return run

    def __exit__(self, *_exc) -> None:
        self.tmp.cleanup()


class HostSymbolTests(unittest.TestCase):
    def test_map_symbol_reads_public_and_static_sections(self) -> None:
        with Scratch() as run:
            self.assertEqual(map_symbol(run / 'jsrf_recomp.map', 'g_recomp_alias_icall_map'),
                             (MAP_AT, PREFERRED))
            self.assertEqual(map_symbol(run / 'jsrf_recomp.map', 'g_recomp_alias_icall_hits')[0],
                             HITS_AT)
            with self.assertRaises(CaptureError):
                map_symbol(run / 'jsrf_recomp.map', 'g_absent')

    def test_a_symbol_is_read_at_the_loaded_base(self) -> None:
        with Scratch() as run, DumpMemory(run) as dump:
            data = dump.host_symbol(run / 'jsrf_recomp.map', 'g_recomp_alias_icall_entries', 4)
            self.assertEqual(struct.unpack('<I', data)[0], len(PAIRS))

    def test_alias_hits_lists_exactly_the_called_aliases(self) -> None:
        with Scratch() as run:
            result = subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT), str(run),
                                     '--alias-hits'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('2 alias entries called', result.stdout)
            self.assertIn('alias 0x0014FEF0 -> owner 0x00150231  calls 5', result.stdout)
            self.assertIn('alias 0x00024700 -> owner 0x000246E0  calls 2', result.stdout)
            self.assertNotIn('0x00037550', result.stdout)

    def test_an_uncaptured_counter_is_absent_not_zero(self) -> None:
        with Scratch(with_hits=False) as run:
            result = subprocess.run([sys.executable, '-X', 'utf8', str(SCRIPT), str(run),
                                     '--alias-hits'], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('absent from this dump', result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
