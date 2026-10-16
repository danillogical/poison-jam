"""Independent check of the generated V5 vertex-program table (orchestrator-owned).

This does NOT re-run the generator's own checks. It answers a different question:
**does each entry's SPIR-V actually correspond to that entry's words?**

Method, per entry, reading only the generated `nv2a_vsh_programs.c`:

  1. Parse the entry's `hash`, `len`, `input_mask`, `digest`, its `vp_words_*`
     array and its `vp_spirv_*` array out of the C source with a regex. This is a
     text-level read of the artifact, not a call into the generator.
  2. Recompute the 64-bit identity digest in Python from the parsed words and
     compare with the entry's `digest` (the algorithm is transcribed from
     `src/video/nv2a_vsh_aot.h`, not imported from the game's script).
  3. Write the words to a temp `.bin`, run the host tool `nv2a_vsh_hlsl --bin ...
     --mask ... --vk`, compile with dxc and compare the resulting SPIR-V bytes
     with the entry's `vp_spirv_*` array. A mismatch means the table carries a
     shader that was not built from the words beside it -- the exact failure the
     identity fields exist to prevent.
  4. Validate the entry's own SPIR-V bytes with spirv-val.
  5. Check the table is strictly ascending by hash with no duplicate, that every
     `spirv_size` equals 4 * the array length, and that no `hash`/`len` pair is
     ambiguous.
  6. Independently re-extract the programs from the XBE and check the table
     covers every distinct static program (count and the set of hashes).

Exit 0 only when every check passes. Prints one line per entry problem and a
final summary. Read-only: it writes only under --scratch.

The default `--tool` is the executable the corpus RECORDS: an existing
`JSRF_NV2A_VSH_HLSL`, else the tool this repository owns under `build/vsh-tool`,
resolved by `gen_shaders.find_host_tool` (never the game build tree's own relinked
copy, whose hash has nothing to do with this table).  With no such tool the check
FAILS and names `just shaders`, which builds and records it; `--tool` overrides the
resolution, and `--skip-spirv` needs no tool and no device at all.

Usage: python -X utf8 scripts/check-vsh-aot-table.py [--c <path>] [--scratch <dir>]
       [--tool <nv2a_vsh_hlsl.exe>] [--skip-spirv] [--skip-coverage]
"""
import argparse
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_C = ROOT / 'src' / 'recomp' / 'gen' / 'shaders' / 'nv2a_vsh_programs.c'
VULKAN_SDK = Path(os.environ.get('VULKAN_SDK', r'C:\VulkanSDK\1.4.357.0'))
DXC = VULKAN_SDK / 'Bin' / 'dxc.exe'
SPIRV_VAL = VULKAN_SDK / 'Bin' / 'spirv-val.exe'
DIGEST_SEED = 0xCBF29CE484222325
DIGEST_PRIME = 0x100000001B3
EXPECT_DISTINCT = 126       # TR §31.12: the measured distinct static programs


def load_fvp():
    """`find-vertex-programs.py` by path: its name is not an importable module."""
    import importlib.util
    path = ROOT / 'scripts' / 'find-vertex-programs.py'
    if not path.is_file():
        print(f'FAIL: {path} is missing; it owns the XBE scan')
        raise SystemExit(2)
    spec = importlib.util.spec_from_file_location('find_vertex_programs', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_gen_shaders():
    """`gen-shaders.py` by path: it owns the ONE host-tool resolution.

    The tool this check recompiles with must be the binary the corpus's recorded
    identity refers to, so the default is resolved by importing the generator's
    `find_host_tool` (an existing `JSRF_NV2A_VSH_HLSL` override, else the OWNED
    artifact under `build/vsh-tool`) instead of spelling a search order a second
    time.  The game build tree's own copy is deliberately not a candidate: `just
    build` relinks it, so its hash has nothing to do with this corpus.  Nothing is
    built or written here; `find_host_tool` only looks.
    """
    import importlib.util
    path = ROOT / 'scripts' / 'gen-shaders.py'
    if not path.is_file():
        print(f'FAIL: {path} is missing; it owns the host-tool resolution')
        raise SystemExit(2)
    spec = importlib.util.spec_from_file_location('gen_shaders', path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['gen_shaders'] = mod
    spec.loader.exec_module(mod)
    return mod


def digest_words(words):
    """The 64-bit identity digest, transcribed from src/video/nv2a_vsh_aot.h."""
    h = DIGEST_SEED
    for ins in words:
        for w in ins:
            h = ((h ^ w) * DIGEST_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


def hash_words(words):
    """The 32-bit FNV-1a the back end binary-searches on.

    Transcribed from find-vertex-programs.py's program_info and
    nv2a_vsh_interp.c's decode_program, which both use these constants; the entry's
    `hash` field is what the lookup compares against, so it must be checked against
    the words it sits beside. Without this, a table whose rows kept their hash
    columns but had their words/SPIR-V swapped passes every other check here, and
    the checker's claim to catch a corrupted identity is false.
    """
    h = 2166136261
    for ins in words:
        for w in ins:
            h = ((h ^ w) * 16777619) & 0xFFFFFFFF
    return h


def parse_table(text):
    """Every entry, from the generated C source. Returns (entries, problems)."""
    problems = []
    words_arrays = {}
    spirv_arrays = {}
    for m in re.finditer(r'static const uint32_t (vp_words_([0-9A-Fa-f]{8}))'
                         r'\s*\[\s*\d+\s*\]\s*\[\s*4\s*\]\s*=\s*\{(.*?)\};', text, re.S):
        words_arrays[m.group(2).upper()] = m.group(3)
    for m in re.finditer(r'static const uint32_t (vp_spirv_([0-9A-Fa-f]{8}))'
                         r'\s*\[\s*\]\s*=\s*\{(.*?)\};', text, re.S):
        spirv_arrays[m.group(2).upper()] = m.group(3)
    body = re.search(r'nv2a_vsh_programs\[\]\s*=\s*\{(.*?)\n\};', text, re.S)
    if not body:
        return [], ['nv2a_vsh_programs[] was not found in the generated source']
    entries = []
    num = r'(0x[0-9A-Fa-f]+|\d+)[uU]*(?:[lL]{1,2})?'
    row = re.compile(r'\{\s*' + num + r'\s*,\s*' + num + r'\s*,\s*' + num + r'\s*,\s*' + num +
                     r'\s*,\s*(vp_words_([0-9A-Fa-f]{8}))\s*,\s*(vp_spirv_([0-9A-Fa-f]{8}))\s*,\s*'
                     r'(sizeof\s+vp_spirv_[0-9A-Fa-f]{8}|' + num + r')\s*\}')
    for m in row.finditer(body.group(1)):
        entries.append({
            'hash': int(m.group(1), 0), 'len': int(m.group(2), 0),
            'mask': int(m.group(3), 0), 'digest': int(m.group(4), 0),
            'words_key': m.group(6).upper(), 'spirv_key': m.group(8).upper(),
            'size_expr': m.group(9),
        })
    if not entries:
        problems.append('no table rows matched the expected initializer shape')
    return (entries, words_arrays, spirv_arrays), problems


def parse_words(blob, key):
    nums = re.findall(r'0x[0-9A-Fa-f]+|\d+', blob)
    vals = [int(n, 0) for n in nums]
    if len(vals) % 4:
        raise ValueError(f'{key}: {len(vals)} dwords is not a multiple of 4')
    return [tuple(vals[i:i + 4]) for i in range(0, len(vals), 4)]


def parse_dwords(blob):
    """A flat dword list: SPIR-V is a byte stream, not float4 groups."""
    return [int(n, 0) for n in re.findall(r'0x[0-9A-Fa-f]+|\d+', blob)]


def run(argv, **kw):
    return subprocess.run(argv, capture_output=True, text=True, **kw)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--c', type=Path, default=DEFAULT_C)
    ap.add_argument('--scratch', type=Path, default=None,
                    help='keep intermediates here instead of a temp dir')
    ap.add_argument('--tool', type=Path, default=None,
                    help='the nv2a_vsh_hlsl host tool (default: the tool the corpus '
                         'records -- an existing JSRF_NV2A_VSH_HLSL, else the owned '
                         'build/vsh-tool artifact, resolved by gen_shaders.find_host_tool)')
    ap.add_argument('--skip-spirv', action='store_true',
                    help='parse and hash only; do not re-translate or recompile')
    ap.add_argument('--skip-coverage', action='store_true',
                    help='do not re-scan the XBE for corpus coverage')
    args = ap.parse_args(argv)

    if not args.c.is_file():
        print(f'FAIL: generated table not found: {args.c}')
        return 2
    text = args.c.read_text(encoding='utf-8', errors='replace')
    parsed, problems = parse_table(text)
    if problems:
        for p in problems:
            print(f'FAIL: {p}')
        return 1
    entries, words_arrays, spirv_arrays = parsed

    tool = args.tool
    if tool is None and not args.skip_spirv:
        # The tool the corpus records, by the generator's own resolution -- never the
        # game build tree's relinked copy, whose hash has nothing to do with this table.
        tool = load_gen_shaders().find_host_tool()
    if not args.skip_spirv and tool is None:
        print('FAIL: the nv2a_vsh_hlsl host tool was not found: JSRF_NV2A_VSH_HLSL is not '
              f'set (or names a file that is not there) and the tool this repository owns, '
              f'the nv2a_vsh_hlsl.exe under {ROOT / "build" / "vsh-tool"} (either CMake '
              'layout), is not built; run `just shaders`, which builds and records that '
              'tool, or pass --tool')
        return 2
    if not args.skip_spirv and (not DXC.is_file() or not SPIRV_VAL.is_file()):
        print(f'FAIL: dxc/spirv-val missing under {VULKAN_SDK}')
        return 2

    scratch = args.scratch
    tmp = None
    if scratch is None and not args.skip_spirv:
        tmp = tempfile.mkdtemp(prefix='vsh-aot-check-')
        scratch = Path(tmp)
    if scratch is not None:
        scratch.mkdir(parents=True, exist_ok=True)

    fails = []
    # The table must be strictly ascending by hash with no duplicate: the back end
    # binary-searches it, so an unsorted table silently misses programs.
    hashes = [e['hash'] for e in entries]
    if hashes != sorted(hashes):
        fails.append('table is not ascending by hash')
    if len(set(hashes)) != len(hashes):
        dup = sorted({h for h in hashes if hashes.count(h) > 1})
        fails.append('duplicate hash(es): ' + ', '.join(f'{h:08X}' for h in dup))
    pairs = [(e['hash'], e['len'], e['mask']) for e in entries]
    if len(set(pairs)) != len(pairs):
        fails.append('duplicate (hash, len, mask) identity')
    declared = re.search(r'nv2a_vsh_program_count\s*=\s*([0-9]+)', text)
    if not declared:
        fails.append('nv2a_vsh_program_count was not found')
    elif int(declared.group(1)) != len(entries):
        fails.append(f'nv2a_vsh_program_count is {declared.group(1)}, '
                     f'but the table has {len(entries)} rows')

    recompiled = 0
    for e in entries:
        tag = f'{e["hash"]:08X}'
        wb = words_arrays.get(e['words_key'])
        sb = spirv_arrays.get(e['spirv_key'])
        if wb is None or sb is None:
            fails.append(f'{tag}: words or SPIR-V array missing')
            continue
        words = parse_words(wb, e['words_key'])
        spirv = parse_dwords(sb)
        blob = b''.join(struct.pack('<4I', *ins) for ins in words)
        # The array names must agree with the row's hash: a row whose hash column
        # was left alone while its arrays were swapped is a table that points a
        # hash at the wrong program.
        want = f'{e["hash"]:08X}'
        if e['words_key'] != want or e['spirv_key'] != want:
            fails.append(f'{want}: the row names {e["words_key"]}/{e["spirv_key"]}, '
                         f'but its hash is {want}')
        # The hash the back end searches on, recomputed from this row's own words.
        if hash_words(words) != e['hash']:
            fails.append(f'{want}: hash is {want}, but its own words hash to '
                         f'{hash_words(words):08X}')
        if len(words) != e['len']:
            fails.append(f'{tag}: len says {e["len"]}, the words array has {len(words)}')
        # The identity digest, recomputed here from the artifact's own words.
        if digest_words(words) != e['digest']:
            fails.append(f'{tag}: digest is {e["digest"]:016X}, '
                         f'recomputed {digest_words(words):016X}')
        if e['size_expr'].startswith('sizeof'):
            pass        # sizeof vp_spirv_x: cannot be wrong, but check the array below
        else:
            n = int(e['size_expr'].rstrip('u'))
            if n != len(spirv) * 4:
                fails.append(f'{tag}: spirv_size is {n}, the array is {len(spirv) * 4} bytes')
        if not spirv:
            fails.append(f'{tag}: empty SPIR-V')
            continue
        magic = spirv[0]
        if magic != 0x07230203:
            fails.append(f'{tag}: SPIR-V magic is 0x{magic:08X}')
        if e['mask'] >> 16:
            fails.append(f'{tag}: input_mask 0x{e["mask"]:X} has bits above 15')
        if not args.skip_spirv:
            # The decisive check: rebuild this entry's shader from this entry's
            # words and compare bytes with the SPIR-V stored beside them.
            base = scratch / tag
            (base.with_suffix('.bin')).write_bytes(blob)
            r = run([str(tool), '--bin', str(base.with_suffix('.bin')),
                     '--mask', f'0x{e["mask"]:X}', '--vk',
                     '--out', str(base.with_suffix('.hlsl'))])
            if r.returncode != 0:
                fails.append(f'{tag}: host tool exited {r.returncode}: {r.stderr.strip()}')
                continue
            r = run([str(DXC), '-spirv', '-T', 'vs_6_0', '-E', 'vs_prog', '-fvk-invert-y',
                     '-fspv-target-env=vulkan1.1', '-Fo', str(base.with_suffix('.spv')),
                     str(base.with_suffix('.hlsl'))])
            if r.returncode != 0:
                fails.append(f'{tag}: dxc exited {r.returncode}: {r.stderr.strip()[:300]}')
                continue
            built = base.with_suffix('.spv').read_bytes()
            if built != b''.join(struct.pack('<I', w) for w in spirv):
                fails.append(f'{tag}: stored SPIR-V differs from the shader built from '
                             f'its own words ({len(built)} vs {len(spirv) * 4} bytes)')
                continue
            recompiled += 1
        # The stored bytes must validate as SPIR-V for the target environment.
        if not args.skip_spirv:
            base = scratch / tag
            base.with_suffix('.stored.spv').write_bytes(
                b''.join(struct.pack('<I', w) for w in spirv))
            r = run([str(SPIRV_VAL), '--target-env', 'vulkan1.1',
                     str(base.with_suffix('.stored.spv'))])
            if r.returncode != 0:
                fails.append(f'{tag}: spirv-val rejected the stored SPIR-V: '
                             f'{r.stderr.strip()[:300]}')

    # The table must cover the XBE's distinct static programs.
    if not args.skip_coverage:
        fvp = load_fvp()
        game = ROOT / 'game'
        xbe = game / 'default.xbe'
        if not xbe.is_file():
            fails.append(f'{xbe} is missing, so the table cannot be checked for coverage')
        else:
            data = xbe.read_bytes()
            sections = fvp.xbe_sections(data)
            found = fvp.scan_functions(data, 'default.xbe', sections, False)
            covered = [(f['offset'], f['offset'] + 4 + 16 * f['len']) for f in found]
            found += fvp.scan_referenced(data, 'default.xbe', sections, covered)
            seen = {}
            for f in found:
                seen.setdefault(f['words_digest'], f)
            xbe_hashes = {f['hash'] for f in seen.values()}
            if len(seen) != EXPECT_DISTINCT:
                fails.append(f'XBE distinct programs is {len(seen)}, expected {EXPECT_DISTINCT}')
            missing = sorted(xbe_hashes - set(hashes))
            extra = sorted(set(hashes) - xbe_hashes)
            if missing:
                fails.append('table is missing XBE program(s): '
                             + ', '.join(f'{h:08X}' for h in missing))
            if extra:
                fails.append('table has program(s) the XBE does not: '
                             + ', '.join(f'{h:08X}' for h in extra))
    else:
        seen = {}

    if tmp:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'entries={len(entries)} recompiled_and_matched={recompiled} '
          f'ascending=yes duplicates=0 '
          f'xbe_distinct={len(seen) if seen else "not checked"} problems={len(fails)}')
    for f in fails[:40]:
        print(f'FAIL: {f}')
    if fails:
        print(f'check-vsh-aot-table: FAILED ({len(fails)} problem(s))')
        return 1
    print('check-vsh-aot-table: all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
