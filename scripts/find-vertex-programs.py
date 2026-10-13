"""Find JSRF's NV2A vertex programs in the original XBE and assets, read-only.

Two forms reach the GPU, and both are copied into SET_TRANSFORM_PROGRAM methods
word for word by the XDK's 0x18FC80, so a program found here is exactly what the
executor later assembles:

- **XDK vertex shader functions** (`xvs`, `xvss`, `xvsw`): a header dword -- low
  word the type (0x2078, 0x7378, 0x7778), high word the instruction count --
  then that many 16-byte NV2A instructions, the last one FINAL.
  `D3DDevice_CreateVertexShader` (XBE 0x18FF90) reads that header.  The game
  keeps its own in the XBE's .data; every file under `Media/` is scanned too.
- **Bare microcode** (`raw`): instructions with no header, used by the D3D
  library itself (the screen-space pass-through programs at 0x19A258 /
  0x19A318 / 0x19A3C8, chosen at 0x1903A0).  Found by following every dword in
  the XBE that holds a section address to a run of instructions ending FINAL.

For each program this prints where it is (file, XBE virtual address when the
file is the XBE), its length, and the hash, input mask and writes-c flag that
toolkit `nv2a_vsh_program_info` computes (FNV-1a over all four dwords of every
instruction up to the first FINAL; the mask from the instructions alone).  With
`--log`, it also reads the `[GPUBE] vertex program HASH (N instructions, inputs
MASK)` lines a run prints under RECOMP_VSH_HLSL_DUMP=1 and reports which runtime
programs were matched.

Nothing is copied: the report carries offsets, lengths and hashes only.

Usage:  python -X utf8 scripts/find-vertex-programs.py [--game game] [--log jsrf_run.log ...]
                                                       [--json out.json] [--all]
"""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TYPES = {0x2078: 'xvs', 0x7378: 'xvss', 0x7778: 'xvsw'}
SLOTS = 136                                    # NV2A_VSH_SLOTS
LOG_LINE = re.compile(r'\[GPUBE\] vertex program ([0-9A-Fa-f]{8}) \((\d+) instructions, inputs ([0-9A-Fa-f]{4})\)')


def field(ins, dw, lo, width):
    return (ins[dw] >> lo) & ((1 << width) - 1)


def program_info(words):
    """Mirror of nv2a_vsh_interp.c decode_program for a program starting at slot 0."""
    h, inputs, writes_c, opos, n = 2166136261, 0, 0, 0, 0
    for ins in words:
        for w in ins:
            h = ((h ^ w) * 16777619) & 0xFFFFFFFF
        mac, ilu = field(ins, 1, 21, 4), field(ins, 1, 25, 3)
        mux = (field(ins, 2, 26, 2), field(ins, 2, 11, 2), field(ins, 3, 28, 2))
        uses = (mac != 0, 2 <= mac <= 12 and mac != 3, mac in (3, 4) or ilu != 0)
        for i in range(3):
            if uses[i] and mux[i] == 2:
                inputs |= 1 << field(ins, 1, 9, 4)
        if field(ins, 3, 12, 4) and not field(ins, 3, 11, 1):
            writes_c = 1
        if field(ins, 3, 12, 4) and field(ins, 3, 11, 1) and field(ins, 3, 3, 8) == 0:
            opos = 1
        n += 1
        if field(ins, 3, 0, 1):
            return {'hash': h, 'len': n, 'input_mask': inputs, 'writes_c': writes_c,
                    'writes_opos': opos}
    return None


def plausible(words, info):
    """NV2A dword 0 is unused and zero in real programs, and every one positions its vertex."""
    return info is not None and info['writes_opos'] and not any(ins[0] for ins in words)


def xbe_sections(data):
    if data[:4] != b'XBEH':
        return []
    base = struct.unpack_from('<I', data, 0x104)[0]
    count, table = struct.unpack_from('<II', data, 0x11C)
    out = []
    for i in range(count):
        _flags, va, _vsize, raw, rsize, name_va = struct.unpack_from('<6I', data, table - base + i * 56)
        end = data.index(b'\0', name_va - base)
        out.append((data[name_va - base:end].decode('ascii', 'replace'), va, raw, rsize))
    return out


def file_va(sections, off):
    for name, va, raw, rsize in sections:
        if raw <= off < raw + rsize:
            return name, va + off - raw
    return None, None


def va_file(sections, va):
    for _name, sva, raw, rsize in sections:
        if sva <= va < sva + rsize:
            return raw + va - sva
    return None


def record(rel, off, sections, kind, words, info):
    sec, va = file_va(sections, off) if sections else (None, None)
    digest = hashlib.sha256(b''.join(struct.pack('<4I', *ins) for ins in words)).hexdigest()[:16]
    return {'file': rel, 'offset': off, 'section': sec, 'va': va, 'type': kind,
            'words_digest': digest, **info}


def scan_functions(data, rel, sections, show_all):
    """XDK shader functions: a type/count header, then the instructions."""
    found = []
    for lo in (b'\x78\x20', b'\x78\x73', b'\x78\x77'):
        start = 0
        while True:
            off = data.find(lo, start)
            if off < 0:
                break
            start = off + 1
            if off + 4 > len(data):
                continue
            kind, count = struct.unpack_from('<HH', data, off)
            if not 1 <= count <= SLOTS or off + 4 + count * 16 > len(data):
                continue
            words = [struct.unpack_from('<4I', data, off + 4 + i * 16) for i in range(count)]
            info = program_info(words)
            # The assembler marks only the last instruction FINAL; anything else is a false hit.
            if info is None or info['len'] != count:
                continue
            if not show_all and not plausible(words, info):
                continue
            found.append(record(rel, off, sections, TYPES[kind], words, info))
    return found


def scan_referenced(data, rel, sections, covered):
    """Bare microcode in the XBE, reached through any dword holding its address."""
    lo = min(s[1] for s in sections)
    hi = max(s[1] + s[3] for s in sections)
    targets = set()
    for shift in range(4):
        view = data[shift:shift + (len(data) - shift) // 4 * 4]
        targets.update(v for (v,) in struct.iter_unpack('<I', view) if lo <= v < hi and not v & 3)
    found = []
    for va in sorted(targets):
        off = va_file(sections, va)
        if off is None or any(a <= off < b for a, b in covered):
            continue
        words = []
        for i in range(SLOTS):
            if off + (i + 1) * 16 > len(data):
                break
            ins = struct.unpack_from('<4I', data, off + i * 16)
            if ins[0]:
                break
            words.append(ins)
            if ins[3] & 1:
                break
        info = program_info(words)
        if info is None or info['len'] < 2 or not plausible(words, info):
            continue
        found.append(record(rel, off, sections, 'raw', words, info))
    return found


def runtime_programs(logs):
    seen = {}
    for log in logs:
        for line in Path(log).read_text(encoding='utf-8', errors='replace').splitlines():
            m = LOG_LINE.search(line)
            if m:
                key = (int(m.group(1), 16), int(m.group(2)), int(m.group(3), 16))
                seen[key] = seen.get(key, 0) + 1
    return seen


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', type=Path, default=ROOT / 'game')
    ap.add_argument('--log', action='append', default=[], help='run log with RECOMP_VSH_HLSL_DUMP=1 output')
    ap.add_argument('--json', type=Path, help='also write the report as JSON (hashes and offsets only)')
    ap.add_argument('--all', action='store_true',
                    help='keep header hits with a nonzero dword 0 or no oPos write (mostly false hits)')
    args = ap.parse_args(argv)

    files = [args.game / 'default.xbe'] + sorted(p for p in (args.game / 'Media').rglob('*') if p.is_file())
    found = []
    for path in files:
        rel = path.relative_to(args.game).as_posix()
        data = path.read_bytes()
        sections = xbe_sections(data) if path.suffix.lower() == '.xbe' else []
        functions = scan_functions(data, rel, sections, args.all)
        found += functions
        if sections:
            covered = [(f['offset'], f['offset'] + 4 + 16 * f['len']) for f in functions]
            found += scan_referenced(data, rel, sections, covered)

    runtime = runtime_programs(args.log)
    runtime_hashes = {h for h, _, _ in runtime}
    by_hash, by_digest = {}, {}
    for f in found:
        by_hash.setdefault(f['hash'], set()).add(f['words_digest'])
        f['duplicate_of'] = by_digest.setdefault(f['words_digest'], f'{f["file"]}+{f["offset"]:X}')
        if f['duplicate_of'] == f'{f["file"]}+{f["offset"]:X}':
            f['duplicate_of'] = None
        f['runtime_seen'] = (f['hash'] in runtime_hashes) if runtime else None

    print(f'{"file":30} {"offset":>8} {"section":7} {"va":>8} {"type":4} {"len":>3} {"hash":8}'
          f' {"inputs":6} c[] runtime duplicate')
    for f in found:
        va = f'{f["va"]:08X}' if f['va'] is not None else '-'
        rt = '-' if f['runtime_seen'] is None else ('yes' if f['runtime_seen'] else 'no')
        print(f'{f["file"]:30} {f["offset"]:8X} {f["section"] or "-":7} {va:>8} {f["type"]:4} {f["len"]:3}'
              f' {f["hash"]:08X} {f["input_mask"]:04X}   {f["writes_c"]}   {rt:7} {f["duplicate_of"] or ""}')
    collisions = [h for h, d in by_hash.items() if len(d) > 1]
    for kind in ('xvs', 'xvss', 'xvsw', 'raw'):
        some = [f for f in found if f['type'] == kind]
        if some:
            print(f'\nstatic {kind}: {len(some)} programs, {len({f["words_digest"] for f in some})} distinct, in '
                  + ', '.join(sorted({f["file"] + (":" + f["section"] if f["section"] else "") for f in some})),
                  end='')
    print(f'\nstatic total: {len(found)} programs, {len(by_digest)} distinct word sequences, '
          f'{len(by_hash)} distinct hashes, {len(collisions)} hash collisions; '
          f'{sum(f["writes_c"] for f in found)} write c[] (CPU-only); scanned {len(files)} files')
    unmatched = []
    if runtime:
        for (h, n, mask), uses in sorted(runtime.items()):
            hits = [f for f in found if f['hash'] == h and f['len'] == n and f['input_mask'] == mask]
            if not hits:
                unmatched.append({'hash': h, 'len': n, 'input_mask': mask})
            print(f'runtime {h:08X} len {n:3} inputs {mask:04X}: '
                  + ('matched ' + ', '.join(f'{f["file"]}@{f["va"]:08X}' if f['va'] is not None
                                            else f'{f["file"]}+{f["offset"]:X}' for f in hits[:3])
                     if hits else 'NOT FOUND statically'))
        print(f'runtime: {len(runtime)} distinct, matched {len(runtime) - len(unmatched)}, '
              f'unmatched {len(unmatched)}')
    if args.json:
        args.json.write_text(json.dumps({'static': found, 'runtime_unmatched': unmatched,
                                         'runtime': [{'hash': h, 'len': n, 'input_mask': m}
                                                     for h, n, m in runtime]}, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
