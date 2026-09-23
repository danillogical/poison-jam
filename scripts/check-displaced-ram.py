"""The discriminator between "the guest's RAM really moved" and "the capture mis-mapped".

Both hypotheses predict `dump[VA] == original[VA + SHIFT]` for image pages, so the
image comparison cannot separate them.  The live log can, because it is an
independent witness of what the GUEST read:

  H1 guest-side displacement (real bug)
      guest[0x1C4064] == original[0x1C4064 + SHIFT] == 0
      -> the call reads 0.  MATCHES the log's `invalid target 0x00000000`.

  H2 capture-side mis-mapping (guest RAM intact)
      guest[0x1C4064] == original[0x1C4064] == 0xFE000104
      -> the call reads 0xFE000104.  CONTRADICTS the log.

`original` is witnessed by the A2f dump, whose mapping passes the `.text` control.
"""
import importlib.util
import struct
from pathlib import Path

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('jsrf_dump', root / 'scripts' / 'jsrf_dump.py')
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

SHIFT = 0x37608
SLOT = 0x001C4064
LOG_TARGET = 0x00000000          # what the ICALL macro logged

with mod.DumpMemory(root / 'logs' / 'runs' / '20260922-190336-778-a2f-7e255-span') as g:
    original_slot = struct.unpack('<I', g.read(SLOT, 4))[0]
    displaced_slot = struct.unpack('<I', g.read(SLOT + SHIFT, 4))[0]

print('independent witness (A2f dump, mapping verified against the XBE):')
print('  original[0x%08X]            = 0x%08X' % (SLOT, original_slot))
print('  original[0x%08X + 0x%X] = 0x%08X' % (SLOT, SHIFT, displaced_slot))
print()
print('the live log recorded the ICALL target as 0x%08X' % LOG_TARGET)
print()
h1 = displaced_slot == LOG_TARGET
h2 = original_slot == LOG_TARGET
print('  H1 guest-side displacement predicts 0x%08X  -> %s'
      % (displaced_slot, 'MATCHES THE LOG' if h1 else 'contradicts'))
print('  H2 capture mis-mapping     predicts 0x%08X  -> %s'
      % (original_slot, 'MATCHES THE LOG' if h2 else 'contradicts'))
print()
if h1 and not h2:
    print('VERDICT: H1. The capture is faithful and the GUEST\'s RAM was displaced.')
    print('         Something moved guest image content down by 0x%X bytes, so the' % SHIFT)
    print('         thunk slot the call read had become the (zero) content of')
    print('         0x%08X.  That is a real guest-visible defect, not a capture artifact.' % (SLOT + SHIFT))
elif h2 and not h1:
    print('VERDICT: H2. The capture mis-mapped; the guest read a valid thunk.')
else:
    print('VERDICT: inconclusive -- both or neither match.')
