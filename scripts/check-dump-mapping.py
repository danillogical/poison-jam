"""Does each archived dump map guest VAs to the right bytes?

A dump can carry a well-formed `guest_ram=<base>+<size>` identity in `stacks.txt`
and still map guest VAs to the wrong bytes.  `scripts/jsrf_dump.py` validates that
an identity *exists*; it cannot tell whether it matches the payload.

The control is one read.  The loader never patches `.text`, so a correct dump must
return the XBE's own first bytes of `.text` at guest VA 0x00011000:

    8b512c85d28b4130c70190431c00741c

Measured 2026-09-22 (A2h): **one dump in 546 fails this control**, and it is the
A2g dump.  Its whole canonical window is displaced by 0x37608 bytes -- 460 of 545
XBE-backed pages read as `original[VA + 0x37608]` and zero read as `original[VA]`,
uniformly across every section -- while its identity line is byte-identical to a
good run's.  The displacement is a *guest* defect, not a capture artifact: the
live log recorded the ICALL target as 0x00000000, which is what the displaced
content holds, whereas a capture mis-mapping would have made the guest read
0xFE000104 and contradicted the log.

So: run this before any dump-based claim.  A `DIFFERS` dump is not evidence, and
its addresses are **not** guest addresses.

    python -X utf8 scripts/check-dump-mapping.py                # every archived run
    python -X utf8 scripts/check-dump-mapping.py <run-name> ... # named runs only

Exit status is 1 if any dump fails, so this is usable as a gate.
"""
from pathlib import Path
import importlib.util
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
PROBE_VA = 0x00011000


def load_reader():
    spec = importlib.util.spec_from_file_location('jsrf_dump', ROOT / 'scripts' / 'jsrf_dump.py')
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def xbe_text_prefix():
    sections = json.loads((ROOT / 'game' / 'mygame_analysis.json').read_text())['sections']
    text = [s for s in sections if s['name'] == '.text'][0]
    with (ROOT / 'game' / 'default.xbe').open('rb') as xbe:
        xbe.seek(int(text['raw_addr'], 16))
        return xbe.read(16)


def main():
    want = xbe_text_prefix()
    reader = load_reader()

    names = sys.argv[1:]
    if names:
        folders = [ROOT / 'logs' / 'runs' / n for n in names]
    else:
        folders = sorted((ROOT / 'logs' / 'runs').iterdir())

    print('expected .text[0:16] at guest VA 0x%08X: %s' % (PROBE_VA, want.hex()))
    print()

    matched = differed = missing = unreadable = 0
    failures = []
    for folder in folders:
        if not (folder / 'process.dmp').exists():
            missing += 1
            continue
        try:
            with reader.DumpMemory(folder) as memory:
                got = memory.read(PROBE_VA, 16)
        except Exception as exc:
            unreadable += 1
            print('%-46s UNREADABLE (%s)' % (folder.name, type(exc).__name__))
            failures.append(folder.name)
            continue
        if got == want:
            matched += 1
        else:
            differed += 1
            failures.append(folder.name)
            print('%-46s DIFFERS  %s' % (folder.name, got.hex()))

    print()
    print('matches: %d   differs: %d   no dump: %d   unreadable: %d'
          % (matched, differed, missing, unreadable))
    if failures:
        print()
        print('NOT evidence: %s' % ', '.join(failures))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
