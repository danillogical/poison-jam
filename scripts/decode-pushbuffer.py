"""Decode JSRF's submitted pushbuffer from a run's guest memory.

The model rejects the packet at subchannel 1 with method 0x180, so the walk never
executes it and the title spins waiting for a value the GPU was supposed to
publish. Before implementing anything, read what the title actually submitted:
the packet sequence from GET to PUT, with subchannel, method and parameters.

Pushbuffer header (NV2A/NV097):
  bits 31:30  type     (0 = method packet, 1 = jump/call, 2 = return, 3 = NOP)
  bits 28:18  count    (for type 0)
  bits 15:13  subchannel
  bits 12:2   method   (dword index, so the byte offset is method*4)
  bit  0      (for type 0) 0 = increasing method, 1 = non-increasing
"""
import json
import struct
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "scripts"))
from jsrf_dump import DumpMemory

run = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "logs/runs/20260922-001023-466-bind-class"
get, put = 0x80001000, 0x80001B24

d = DumpMemory(run)
buf = d.read(get, put - get)
d.close()

pc = get
words = 0
out = []
while pc < put and words < 1024:
    off = pc - get
    h = struct.unpack_from("<I", buf, off)[0]
    typ = h >> 30
    if typ == 0:
        count = (h >> 18) & 0x7FF
        sub = (h >> 13) & 7
        method = h & 0x1FFC
        params = [struct.unpack_from("<I", buf, off + 4 * (i + 1))[0] for i in range(count)]
        out.append((pc, "method", sub, method, params))
        pc += 4 * (count + 1)
        words += count + 1
    elif typ == 1:
        target = h & 0x1FFFFFFF
        out.append((pc, "jump", None, target, []))
        pc = target
        words += 1
    elif typ == 2:
        out.append((pc, "return", None, None, []))
        pc += 4
        words += 1
    else:
        out.append((pc, "nop", None, None, []))
        pc += 4
        words += 1

print("packets decoded:", len(out), " words:", words, " range: %08X..%08X" % (get, put))
print()
print("%-9s %-7s %-4s %-7s %s" % ("addr", "kind", "sub", "method", "params"))
for addr, kind, sub, method, params in out:
    if kind == "method":
        print("%08X  %-7s %-4s 0x%04X  %s" % (addr, kind, sub, method,
              " ".join("%08X" % p for p in params[:6]) + (" ..." if len(params) > 6 else "")))
    else:
        print("%08X  %-7s %-4s %-7s %s" % (addr, kind, "-", "-",
              "0x%08X" % method if method is not None else ""))
