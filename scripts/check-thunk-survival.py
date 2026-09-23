"""How much of the relocated thunk table survived, and what overwrote the rest?

The advisor predicted the patched table now sits at guest VA 0x0018C958 and that
later writes may have damaged part of it.  The 8-dword probe hits exactly there
and nowhere else, so the relocation is confirmed; this measures how far the run of
`0xFE000000 + i*4` extends and what replaced the remainder -- which says whether
the damage is one later write or many.
"""
import importlib.util
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('jsrf_dump', root / 'scripts' / 'jsrf_dump.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

TABLE = 0x001C3F60
SHIFT = 0x37608
RELOC = TABLE - SHIFT              # 0x0018C958

with mod.DumpMemory(root / 'logs' / 'runs' / '20260922-224429-003-a2g-304f0-span') as mem:
    raw = mem.read(RELOC, 120 * 4)
    vals = struct.unpack('<120I', raw)

    intact = 0
    for i, v in enumerate(vals):
        if v == 0xFE000000 + i * 4:
            intact += 1
        else:
            break
    print('relocated table at 0x%08X' % RELOC)
    print('  consecutive slots still holding 0xFE000000+i*4 from index 0: %d of 120' % intact)
    print()

    # Where does it break, and what is there instead?
    print('=== slot-by-slot around the break ===')
    lo = max(0, intact - 4)
    hi = min(120, intact + 12)
    for i in range(lo, hi):
        want = 0xFE000000 + i * 4
        mark = 'ok ' if vals[i] == want else 'BAD'
        print('  [%3d] VA 0x%08X  got 0x%08X  want 0x%08X  %s'
              % (i, RELOC + i * 4, vals[i], want, mark))

    print()
    total_ok = sum(1 for i, v in enumerate(vals) if v == 0xFE000000 + i * 4)
    print('  total slots matching anywhere: %d of 120' % total_ok)

    # Is the original location now holding the *predecessor's* content?
    print()
    print('=== cross-check: what the transfer model says the ORIGINAL slot holds ===')
    print('  original 0x%08X now = 0x%08X (should be whatever lived at 0x%08X)'
          % (TABLE, struct.unpack('<I', mem.read(TABLE, 4))[0], TABLE + SHIFT))
