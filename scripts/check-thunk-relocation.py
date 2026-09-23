"""Test the advisor's prediction: the patched thunk table survives, relocated.

The advisor's model is an overlapping bulk transfer with
`source = destination + 0x37608`.  Under it the loader's synthetic-VA table was
written at 0x001C3F60 and then moved DOWN by 0x37608, so the patched table should
still exist at guest VA `0x001C3F60 - 0x37608 = 0x0018C958`, with slot 65 at
`0x0018CA5C`.

That is a falsifiable prediction with a distinctive signature: 120 consecutive
dwords equal to `0xFE000000 + i*4`.  Search the whole guest window for that
sequence -- not just the predicted address -- and report every hit.  A model that
predicts where a 480-byte distinctive pattern lands is worth a great deal more
than one that only explains what is already known.
"""
import importlib.util
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('jsrf_dump', root / 'scripts' / 'jsrf_dump.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

SHIFT = 0x37608
TABLE = 0x001C3F60
PREDICTED = TABLE - SHIFT          # 0x0018C958

# The signature: the loader's own synthetic VAs, as 120 consecutive dwords.
SIG = struct.pack('<120I', *[0xFE000000 + i * 4 for i in range(120)])
# A shorter probe for partial survival.
PROBE = struct.pack('<8I', *[0xFE000000 + i * 4 for i in range(8)])

with mod.DumpMemory(root / 'logs' / 'runs' / '20260922-224429-003-a2g-304f0-span') as mem:
    print('predicted relocated table at guest VA 0x%08X (slot 65 -> 0x%08X)'
          % (PREDICTED, PREDICTED + 65 * 4))
    print()

    print('=== read the prediction directly ===')
    try:
        raw = mem.read(PREDICTED, 480)
        vals = struct.unpack('<120I', raw)
        full = raw == SIG
        print('  first 8 dwords: %s' % ' '.join('%08X' % v for v in vals[:8]))
        print('  all 120 match 0xFE000000+i*4 : %s' % full)
        print('  slot 65 at 0x%08X = 0x%08X  (expected 0xFE000104)'
              % (PREDICTED + 65 * 4, struct.unpack('<I', mem.read(PREDICTED + 65 * 4, 4))[0]))
    except Exception as exc:
        print('  UNREADABLE: %s' % exc)
    print()

    print('=== search the whole canonical window for the 120-dword signature ===')
    hits = []
    for va in range(0x00010000, 0x04000000, 0x10000):
        try:
            chunk = mem.read(va, 0x10000)
        except Exception:
            continue
        i = chunk.find(SIG)
        while i >= 0:
            hits.append(va + i)
            i = chunk.find(SIG, i + 1)
    print('  full-signature hits: %d' % len(hits))
    for va in hits:
        print('    0x%08X' % va)
    print()

    print('=== search for a PARTIAL survivor: any 8-dword run of the sequence ===')
    partial = []
    for va in range(0x00010000, 0x04000000, 0x10000):
        try:
            chunk = mem.read(va, 0x10000)
        except Exception:
            continue
        i = chunk.find(PROBE)
        while i >= 0:
            partial.append(va + i)
            i = chunk.find(PROBE, i + 1)
    print('  partial hits: %d' % len(partial))
    for va in partial[:12]:
        print('    0x%08X' % va)
