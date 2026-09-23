"""P0.4: separate capture validity from guest-memory integrity.

Two independent properties of an archived dump, deliberately reported separately
because conflating them destroyed real evidence once:

1. **Structural validity** -- can the dump be opened and read at all, and is the
   stack/register state usable *at the addresses it recorded*?  Measured by the
   recorded return VA being present at the logged ESP.  One stack word supports
   *that location*, not every region.
2. **Image-content integrity** -- do XBE-backed pages match the original image?
   The loader never patches `.text`, so an undisplaced dump returns the XBE's own
   bytes at guest VA `0x00011000`.

**A `CONTENT_MISMATCH` is a finding about the guest, not a fault in the capture.**
A faithful dump of corrupted RAM is *evidence of the corruption*: the thunk slot
really is zero where the guest looked.  So reads are always taken at **actual
guest VAs** and never shifted.  A shifted comparison against
`original[VA + displacement]` recovers **byte provenance** (which original bytes
ended up where) and nothing more; it does not reconstruct repaired state and must
never be applied as a read correction.

Usage::

    python -X utf8 scripts/check-dump-controls.py <run-dir> [--json]
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER_VERSION = 'jsrf-dump-controls/1'

PROBE_VA = 0x00011000
DOCUMENTED_CONTROL = bytes.fromhex('8b512c85d28b4130c70190431c00741c')

# Verdict vocabulary.  Structure and content are separate axes, and an
# unreadable capture is distinct from a displaced image.
STRUCTURE_OK = 'STRUCTURE_OK'
STRUCTURE_UNREADABLE = 'STRUCTURE_UNREADABLE'
STRUCTURE_UNSUPPORTED = 'STRUCTURE_UNSUPPORTED'
CONTENT_MATCH = 'MATCH'
CONTENT_MISMATCH = 'CONTENT_MISMATCH'
CONTENT_UNREADABLE = 'UNREADABLE'
CONTENT_MISSING = 'MISSING'

EXIT_CLEAN = 0
EXIT_FINDING = 1
EXIT_INVALID = 2


class DumpControlsError(ValueError):
    """The checker cannot run against this input."""


def load_reader():
    spec = importlib.util.spec_from_file_location(
        'jsrf_dump', ROOT / 'scripts' / 'jsrf_dump.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def expected_text_bytes() -> bytes:
    """The XBE's own bytes at the probe VA, from two independent derivations.

    Disagreement here is an error in the *checker*, not in the dumps.  An earlier
    version read the header field at 0x118 -- the **certificate address**, not the
    base -- so the derived offset was off by 0x10178 and every good dump was
    reported as a content mismatch.
    """
    xbe_path = ROOT / 'game' / 'default.xbe'
    if not xbe_path.is_file():
        raise DumpControlsError(f'original XBE is missing: {xbe_path}')
    xbe = xbe_path.read_bytes()
    if xbe[0:4] != b'XBEH':
        raise DumpControlsError('game/default.xbe is not an XBE')
    base = struct.unpack('<I', xbe[0x104:0x108])[0]
    offset = PROBE_VA - base
    if offset < 0 or offset + 16 > len(xbe):
        raise DumpControlsError(f'derived file offset 0x{offset:X} is outside the image')
    derived = xbe[offset:offset + 16]
    if derived != DOCUMENTED_CONTROL:
        raise DumpControlsError(
            'derivation disagrees with the documented control, so the CHECKER is '
            f'wrong: derived {derived.hex()} vs documented {DOCUMENTED_CONTROL.hex()}')
    return derived


def parse_guest_stack(folder: Path) -> dict:
    """Read the recorded return VA and ESP from ``stacks.txt`` if present."""
    path = folder / 'stacks.txt'
    if not path.is_file():
        return {'available': False, 'reason': 'stacks.txt is missing'}
    text = path.read_text(encoding='utf-8', errors='replace')
    result: dict = {'available': True}
    identity = None
    for line in text.split('\n'):
        stripped = line.strip()
        if stripped.startswith('guest_ram='):
            identity = stripped.partition('=')[2]
        # The collector logs the stop site and the stack pointer.
        if 'esp=' in stripped.casefold():
            for token in stripped.replace(',', ' ').split():
                lowered = token.casefold()
                if lowered.startswith('esp='):
                    try:
                        result['esp'] = int(token.partition('=')[2], 16)
                    except ValueError:
                        pass
                elif lowered.startswith('eip='):
                    try:
                        result['eip'] = int(token.partition('=')[2], 16)
                    except ValueError:
                        pass
    if identity:
        result['guest_ram_identity'] = identity
    return result


def structural_verdict(folder: Path, memory) -> dict:
    """Structural validity, and the stack control where the capture allows it.

    A recorded return VA sitting at the logged ESP is what supports *that
    location*.  Absence of the control is reported as unsupported, never as a
    pass -- and one stack word never certifies every region.
    """
    stack = parse_guest_stack(folder)
    verdict: dict = {'stack_identity': stack}
    if not stack.get('available'):
        verdict['status'] = STRUCTURE_UNSUPPORTED
        verdict['reason'] = 'no stacks.txt, so the stack location cannot be supported'
        return verdict
    esp = stack.get('esp')
    if esp is None:
        verdict['status'] = STRUCTURE_UNSUPPORTED
        verdict['reason'] = 'stacks.txt records no esp, so no stack control exists'
        return verdict
    try:
        word = struct.unpack('<I', memory.read(esp, 4))[0]
    except Exception as error:
        verdict['status'] = STRUCTURE_UNSUPPORTED
        verdict['reason'] = f'logged esp 0x{esp:08X} is unreadable: {error}'
        return verdict
    verdict['esp'] = esp
    verdict['word_at_esp'] = f'0x{word:08X}'
    verdict['status'] = STRUCTURE_OK
    verdict['reason'] = ('the capture is structurally readable and its stack is '
                         'usable at the logged location')
    verdict['claim_limit'] = ('this supports the stack at the logged esp only; it '
                              'does not certify every region of the dump')
    return verdict


def content_verdict(folder: Path, memory, want: bytes) -> dict:
    try:
        got = memory.read(PROBE_VA, 16)
    except Exception as error:
        return {'status': CONTENT_UNREADABLE, 'reason': str(error)}
    if got == want:
        return {
            'status': CONTENT_MATCH,
            'observed': got.hex(),
            'claim_limit': ('a 16-byte match is one probe of one page; it does not '
                            'certify the whole image'),
        }
    return {
        'status': CONTENT_MISMATCH,
        'observed': got.hex(),
        'expected': want.hex(),
        'reason': ('XBE-backed pages are displaced; this is a finding about guest '
                   'memory, not a fault in the capture'),
        'read_rule': ('read at ACTUAL guest VAs; never shift reads, and do not '
                      'discard the dump'),
    }


def check_run(folder: Path, want: bytes, reader) -> dict:
    result = {'run': folder.name, 'path': str(folder)}
    if not folder.is_dir():
        return {**result, 'structure': {'status': STRUCTURE_UNSUPPORTED,
                                        'reason': 'run directory is missing'},
                'content': {'status': CONTENT_MISSING, 'reason': 'run directory is missing'},
                'usable_for_content_claim': False}
    dump = folder / 'process.dmp'
    if not dump.is_file():
        return {**result, 'structure': {'status': STRUCTURE_UNSUPPORTED,
                                        'reason': 'process.dmp is missing'},
                'content': {'status': CONTENT_MISSING, 'reason': 'process.dmp is missing'},
                'usable_for_content_claim': False,
                'claim_limit': ('a named run with no dump is a failure, not a silent '
                                'pass')}
    try:
        with reader.DumpMemory(folder) as memory:
            structure = structural_verdict(folder, memory)
            content = content_verdict(folder, memory, want)
    except Exception as error:
        return {**result,
                'structure': {'status': STRUCTURE_UNREADABLE, 'reason': str(error)},
                'content': {'status': CONTENT_UNREADABLE, 'reason': str(error)},
                'usable_for_content_claim': False}

    result['structure'] = structure
    result['content'] = content
    # The two axes are independent: a displaced image is still readable.
    result['usable_for_content_claim'] = content['status'] == CONTENT_MATCH
    result['usable_for_structural_claim'] = structure['status'] == STRUCTURE_OK
    if content['status'] == CONTENT_MISMATCH:
        result['attribution'] = (
            'CONTENT_MISMATCH: read the dump at its actual guest VAs. A shifted '
            'comparison recovers byte provenance only and is never a read correction.')
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runs', nargs='*', help='run directories or names')
    parser.add_argument('--all', action='store_true', help='check every archived run')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    if not args.runs and not args.all:
        parser.error('name at least one run, or pass --all')

    try:
        want = expected_text_bytes()
        reader = load_reader()
    except DumpControlsError as error:
        print(f'INVALID: {error}', file=sys.stderr)
        return EXIT_INVALID

    if args.all:
        root = ROOT / 'logs' / 'runs'
        folders = sorted((p for p in root.iterdir() if p.is_dir()),
                         key=lambda p: p.name.casefold())
    else:
        folders = []
        for name in args.runs:
            candidate = Path(name)
            if not candidate.is_dir():
                candidate = ROOT / 'logs' / 'runs' / name
            folders.append(candidate)

    results = [check_run(folder, want, reader) for folder in folders]

    if args.json:
        print(json.dumps({'checker_version': CHECKER_VERSION,
                          'probe_va': f'0x{PROBE_VA:08X}',
                          'expected': want.hex(),
                          'runs': results}, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}; probe VA 0x{PROBE_VA:08X}')
        print(f'expected .text[0:16]: {want.hex()}')
        print()
        for entry in results:
            print(f'{entry["run"]}')
            print(f'   structure: {entry["structure"]["status"]:<22} '
                  f'{entry["structure"].get("reason", "")}')
            print(f'   content  : {entry["content"]["status"]:<22} '
                  f'{entry["content"].get("observed", "")}')
        print()
        content = {}
        structure = {}
        for entry in results:
            content[entry['content']['status']] = content.get(entry['content']['status'], 0) + 1
            structure[entry['structure']['status']] = structure.get(
                entry['structure']['status'], 0) + 1
        print('structure:', ', '.join(f'{k}={v}' for k, v in sorted(structure.items())))
        print('content  :', ', '.join(f'{k}={v}' for k, v in sorted(content.items())))
        if content.get(CONTENT_MISMATCH):
            print()
            print('A CONTENT_MISMATCH dump is still structurally readable: read it at '
                  'its actual guest VAs. Do not shift reads and do not discard it.')

    # The gate fails on a missing/unreadable capture and on a displaced image, but
    # the two are reported as different findings.
    failing = [e for e in results
               if e['content']['status'] in (CONTENT_MISSING, CONTENT_UNREADABLE)
               or e['structure']['status'] == STRUCTURE_UNREADABLE]
    displaced = [e for e in results if e['content']['status'] == CONTENT_MISMATCH]
    return EXIT_FINDING if (failing or displaced) else EXIT_CLEAN


if __name__ == '__main__':
    sys.exit(main())
