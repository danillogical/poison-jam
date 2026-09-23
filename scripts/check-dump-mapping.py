"""Is an archived dump structurally readable, and does its image content match the XBE?

**This checks two independent properties and reports them separately, because
conflating them destroyed real evidence once.**

  1. **Structural validity** -- can the dump be opened and read at all?  A dump
     that opens and returns bytes is structurally readable, and its stack and
     registers are usable *at the addresses it recorded*.
  2. **Image-content integrity** -- do XBE-backed pages match the original image?
     The loader never patches `.text`, so a dump whose content is undisplaced must
     return the XBE's own first bytes of `.text` at guest VA 0x00011000:

         8b512c85d28b4130c70190431c00741c

Measured 2026-09-22 (A2h): **one dump in 546 fails control 2**, and it is the A2g
dump.  Its whole canonical window is displaced by 0x37608 bytes -- 460 of 545
XBE-backed pages read as `original[VA + 0x37608]`, zero read as `original[VA]`,
uniformly across every section -- while its identity line is byte-identical to a
good run's.  The displacement is a *guest* defect, not a capture artifact: the
live log recorded the ICALL target as 0x00000000, which is what the displaced
content holds, whereas a capture mis-mapping would have made the guest read
0xFE000104 and contradicted the log.  Its stack had **not** moved.

**How to use a CONTENT_MISMATCH result, because the obvious reading is wrong.**
A faithful dump of corrupted RAM is *evidence of the corruption* -- the thunk slot
really is zero where the guest looked.  So:

  * **Read it at its actual guest VAs.**  Do not shift reads.
  * A shifted comparison against `original[VA + displacement]` recovers **byte
    provenance** (which original bytes ended up where) and nothing more.  It does
    not reconstruct repaired runtime state and must never be used as a read
    correction.
  * Do **not** conclude the dump is worthless.  Only the image-content claim is
    affected.  A previous version of this file said "a DIFFERS dump is not
    evidence, and its addresses are not guest addresses" -- both statements were
    wrong, and the second would have discarded the A2h evidence itself.
  * A 16-byte match does not certify the whole image either; it is one probe.

    python -X utf8 scripts/check-dump-mapping.py                # every archived run
    python -X utf8 scripts/check-dump-mapping.py <run-name> ... # named runs only

Exit status is 1 if any dump is MISSING, UNREADABLE or CONTENT_MISMATCH, so this is
usable as a gate.  A named run with no dump is a failure, not a silent pass.
"""
from pathlib import Path
import importlib.util
import sys

ROOT = Path(__file__).resolve().parents[1]
PROBE_VA = 0x00011000
DOCUMENTED_CONTROL = bytes.fromhex('8b512c85d28b4130c70190431c00741c')

MATCH = 'MATCH'
CONTENT_MISMATCH = 'CONTENT_MISMATCH'
UNREADABLE = 'UNREADABLE'
MISSING = 'MISSING'


def load_reader():
    spec = importlib.util.spec_from_file_location('jsrf_dump', ROOT / 'scripts' / 'jsrf_dump.py')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def want_bytes():
    """The XBE's own bytes for guest VA 0x11000.

    Two independent sources, and **disagreement is an error in this function, not
    in the dumps.**  Getting this wrong is how a checker ends up flagging every
    *good* dump: an earlier version read the header field at 0x118, which is the
    **certificate address**, not the base address -- so the derived offset was off
    by 0x10178 and all three test dumps were reported as content mismatches.

    Source 1 is the base address at 0x104 plus the identity `file = VA - base`
    (the header region is mapped at the base, so file offset equals RVA).
    Source 2 is the constant this control has used since A2h, measured against 545
    of 546 archived dumps.  If they disagree, refuse to report anything.
    """
    from struct import unpack
    xbe = (ROOT / 'game' / 'default.xbe').read_bytes()
    if xbe[0:4] != b'XBEH':
        raise SystemExit('game/default.xbe is not an XBE')
    base = unpack('<I', xbe[0x104:0x108])[0]
    offset = PROBE_VA - base
    if offset < 0 or offset + 16 > len(xbe):
        raise SystemExit('derived file offset 0x%X is outside the image' % offset)
    derived = xbe[offset:offset + 16]
    if derived != DOCUMENTED_CONTROL:
        raise SystemExit(
            'derivation disagrees with the documented control -- the CHECKER is '
            'wrong, do not trust any verdict below.\n'
            '  derived   (base 0x%08X -> file 0x%X): %s\n'
            '  documented (545/546 dumps):           %s'
            % (base, offset, derived.hex(), DOCUMENTED_CONTROL.hex()))
    return derived


def main():
    reader = load_reader()
    want = want_bytes()

    if len(sys.argv) > 1:
        folders = []
        for name in sys.argv[1:]:
            folder = ROOT / 'logs' / 'runs' / name
            if not folder.is_dir():
                folder = Path(name)
            folders.append(folder)
    else:
        folders = sorted((ROOT / 'logs' / 'runs').iterdir())

    print('expected .text[0:16] at guest VA 0x%08X: %s' % (PROBE_VA, want.hex()))
    print()

    tally = {MATCH: 0, CONTENT_MISMATCH: 0, UNREADABLE: 0, MISSING: 0}
    failures = []
    for folder in folders:
        if not (folder / 'process.dmp').exists():
            tally[MISSING] += 1
            failures.append(folder.name)
            print('%-46s %s' % (folder.name, MISSING))
            continue
        try:
            with reader.DumpMemory(folder) as memory:
                got = memory.read(PROBE_VA, 16)
        except Exception as exc:
            tally[UNREADABLE] += 1
            failures.append(folder.name)
            print('%-46s %s (%s)' % (folder.name, UNREADABLE, type(exc).__name__))
            continue
        if got == want:
            tally[MATCH] += 1
        else:
            tally[CONTENT_MISMATCH] += 1
            failures.append(folder.name)
            print('%-46s %s  %s' % (folder.name, CONTENT_MISMATCH, got.hex()))

    print()
    print('matches: %d   content-mismatch: %d   unreadable: %d   missing: %d'
          % (tally[MATCH], tally[CONTENT_MISMATCH], tally[UNREADABLE], tally[MISSING]))
    if failures:
        print()
        print('Not usable for an IMAGE-CONTENT claim: %s' % ', '.join(failures))
        if tally[CONTENT_MISMATCH]:
            print('A CONTENT_MISMATCH dump is still structurally readable: read it at '
                  'its actual guest VAs. Do not shift reads and do not discard it.')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
