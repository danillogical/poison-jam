#!/usr/bin/env python3
"""Generate the ahead-of-time NV2A vertex-program corpus (plan step V5).

The Vulkan back end never translates or compiles a guest vertex program at run
time (toolkit `src/video/nv2a_vk_backend.h`).  This script builds that corpus
BEFORE the game is linked: it extracts the title's static programs from the
retail XBE exactly as `scripts/find-vertex-programs.py` reports them, translates
each one to HLSL with the toolkit's host tool `nv2a_vsh_hlsl --vk`, compiles the
HLSL to SPIR-V with the Vulkan SDK's `dxc`, validates every module with
`spirv-val`, and emits the table the game links:

    src/recomp/gen/shaders/nv2a_vsh_programs.c   the corpus
    src/recomp/gen/shaders/nv2a_vsh_programs.h   its two externs

Both files are retail-derived generated output and stay untracked, like
`src/recomp/gen` itself; CMake refuses to configure without them.

Everything is offline and deterministic: no timestamp, no absolute path, no
unordered iteration reaches the emitted bytes, and `--check` regenerates into a
temporary directory and compares byte for byte.

The extraction is IMPORTED from `scripts/find-vertex-programs.py`, never copied,
so the two cannot drift.  The scan properties below are THIS title's measured
values (128 programs / 126 distinct word sequences / 126 distinct hashes /
0 collisions / 0 write c[]); a change in any of them means the corpus moved and
must not pass silently, so each one aborts the generation.

Bootstrap: the host tool is built from the toolkit, which configures and builds
standalone.  When no game build tree exists yet -- which is exactly the fresh
checkout, because the game's CMake refuses to configure without this corpus --
the script configures the toolkit into `build/vsh-tool/` and builds just the
`nv2a_vsh_hlsl` target.  So `just shaders` works before the first `just build`.
`JSRF_NV2A_VSH_HLSL` overrides the tool's path when it lives elsewhere.

Usage:  python -X utf8 scripts/gen-shaders.py [--game game] [--check]
"""
import argparse
import hashlib
import importlib.util
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = Path(__file__).resolve().parent
TOOLKIT = Path(os.environ.get('XBOXRECOMP_DIR', str(ROOT.parent / 'xboxrecomp')))
AOT_HEADER = TOOLKIT / 'src/video/nv2a_vsh_aot.h'
OUT_DIR = ROOT / 'src/recomp/gen/shaders'
C_NAME = 'nv2a_vsh_programs.c'
H_NAME = 'nv2a_vsh_programs.h'

# The host tool: the same binary either CMake layout can leave behind, then an
# explicit override.  Built by the toolkit's src/video/CMakeLists.txt.
HOST_TOOL_CANDIDATES = (
    ROOT / 'build/xboxrecomp/src/video/Release/nv2a_vsh_hlsl.exe',
    ROOT / 'build/xboxrecomp/src/video/nv2a_vsh_hlsl.exe',
)

# Where the tool is built when no game build tree exists yet.  This is the
# bootstrap: the game's CMake refuses to configure without the generated corpus,
# so on a fresh checkout this toolkit-only build is the only way to get the tool
# that produces that corpus.  Under build/ (gitignored).
TOOL_BUILD_DIR = ROOT / 'build' / 'vsh-tool'

# Measured on this title (see the module docstring).  These are guards, not
# tunables: the corpus is the whole set of programs the guest can upload, so a
# different count is a different title or a changed scanner.
EXPECT_PROGRAMS = 128
EXPECT_DISTINCT = 126
EXPECT_HASHES = 126
MAX_INSTRUCTIONS = 136                     # find_vertex_programs.SLOTS (NV2A_VSH_SLOTS)
MAX_INPUTS = 16                            # the shader's v0..v15

# nv2a_vsh_aot.h defines the identity digest.  It MUST be this algorithm; the
# constants are re-read from the header and asserted equal below, so a change
# there fails here instead of silently producing a table the back end rejects.
DIGEST_SEED = 0xCBF29CE484222325
DIGEST_PRIME = 0x100000001B3
MASK64 = (1 << 64) - 1

sys.path.insert(0, str(SCRIPTS))


def die(message):
    print(f'gen-shaders: {message}', file=sys.stderr)
    sys.exit(1)


# The extraction lives in scripts/find-vertex-programs.py, whose name is not an
# importable identifier; load it by path so the scan logic is shared, never copied.
_FVP_PATH = SCRIPTS / 'find-vertex-programs.py'
if not _FVP_PATH.is_file():
    die(f'{_FVP_PATH} is missing: it owns the XBE scan this generator reuses.')
_spec = importlib.util.spec_from_file_location('find_vertex_programs', _FVP_PATH)
fvp = importlib.util.module_from_spec(_spec)
sys.modules['find_vertex_programs'] = fvp
_spec.loader.exec_module(fvp)


def aot_digest(words):
    """nv2a_vsh_aot_digest: FNV-1a 64 over every dword of every instruction."""
    h = DIGEST_SEED
    for ins in words:
        for w in ins:
            h = ((h ^ w) * DIGEST_PRIME) & MASK64
    return h


def header_digest_constants():
    """The digest constants as nv2a_vsh_aot.h actually defines them."""
    if not AOT_HEADER.is_file():
        die(f'{AOT_HEADER} is missing: the toolkit must be checked out beside this '
            f'repo (set XBOXRECOMP_DIR otherwise); the generated table includes its header.')
    text = AOT_HEADER.read_text(encoding='utf-8')
    found = {}
    for name in ('NV2A_VSH_AOT_DIGEST_SEED', 'NV2A_VSH_AOT_DIGEST_PRIME'):
        m = re.search(r'#define\s+' + name + r'\s+(0[xX][0-9a-fA-F]+)', text)
        if not m:
            die(f'{AOT_HEADER} no longer defines {name}: the identity digest changed, '
                f'so this generator must be updated with it.')
        found[name] = int(m.group(1), 16)
    seed, prime = found['NV2A_VSH_AOT_DIGEST_SEED'], found['NV2A_VSH_AOT_DIGEST_PRIME']
    if (seed, prime) != (DIGEST_SEED, DIGEST_PRIME):
        die(f'{AOT_HEADER} digest constants are seed=0x{seed:X} prime=0x{prime:X}, but this '
            f'script computes seed=0x{DIGEST_SEED:X} prime=0x{DIGEST_PRIME:X}: the two must '
            f'agree, update scripts/gen-shaders.py.')
    return seed, prime


def extract(game):
    """Every static program, exactly as scripts/find-vertex-programs.py main() finds them."""
    xbe = game / 'default.xbe'
    if not xbe.is_file():
        die(f'{xbe} is missing: this corpus is extracted from the retail XBE. Put the game '
            f'files in {game} (AGENTS.md, "Rebuilding the lifted code").')
    files = [xbe] + sorted(p for p in (game / 'Media').rglob('*') if p.is_file())
    found = []
    for path in files:
        rel = path.relative_to(game).as_posix()
        data = path.read_bytes()
        sections = fvp.xbe_sections(data) if path.suffix.lower() == '.xbe' else []
        functions = fvp.scan_functions(data, rel, sections, False)
        found += functions
        if sections:
            covered = [(f['offset'], f['offset'] + 4 + 16 * f['len']) for f in functions]
            found += fvp.scan_referenced(data, rel, sections, covered)
    return found, len(files)


def attach_words(found, game):
    """find_vertex_programs.record() keeps offsets, not the words; read them back.

    The words are what this corpus ships, so the dedupe and the emitted table use
    the bytes the scanner actually matched, not a second derivation of them.
    """
    cache = {}
    for f in found:
        if 'words' in f:
            continue
        data = cache.get(f['file'])
        if data is None:
            data = cache[f['file']] = (game / f['file']).read_bytes()
        off = f['offset'] + (0 if f['type'] == 'raw' else 4)   # past the type/count header
        f['words'] = [struct.unpack_from('<4I', data, off + i * 16) for i in range(f['len'])]
        # The words we ship must be the ones the scanner hashed: re-derive the
        # scanner's own info from them and require it to agree, so a mis-sized
        # read can never emit a table whose hash and shader disagree.
        info = fvp.program_info(f['words'])
        if info is None or (info['hash'], info['len'], info['input_mask']) != (
                f['hash'], f['len'], f['input_mask']):
            die(f'{f["file"]}+{f["offset"]:X}: re-read words do not reproduce the scanner\'s '
                f'info (got {info}, record says hash {f["hash"]:08X} len {f["len"]} '
                f'mask 0x{f["input_mask"]:04X})')
    return found


def dedupe(found):
    """One record per distinct instruction sequence, keeping the first found."""
    by_words = {}
    for f in found:
        by_words.setdefault(tuple(tuple(ins) for ins in f['words']), f)
    return [by_words[k] for k in by_words]


def guard(found, distinct, scanned):
    """The measured properties of THIS title.  Any change is a moved corpus."""
    if len(found) != EXPECT_PROGRAMS:
        die(f'corpus moved: {len(found)} programs extracted, expected {EXPECT_PROGRAMS} '
            f'(scanned {scanned} files). A different count means the scanner or the game '
            f'data changed; re-measure with scripts/find-vertex-programs.py before trusting this.')
    if len(distinct) != EXPECT_DISTINCT:
        die(f'corpus moved: {len(distinct)} distinct word sequences, expected {EXPECT_DISTINCT}. '
            f'Re-measure with scripts/find-vertex-programs.py.')
    hashes = {}
    for f in distinct:
        hashes.setdefault(f['hash'], set()).add(tuple(tuple(ins) for ins in f['words']))
    collisions = sorted(h for h, s in hashes.items() if len(s) > 1)
    if collisions:
        die('hash collision: ' + ', '.join(f'{h:08X}' for h in collisions) + ' name more than one '
            'word sequence; the back end selects a shader by hash, so this cannot ship.')
    if len(hashes) != EXPECT_HASHES:
        die(f'corpus moved: {len(hashes)} distinct hashes, expected {EXPECT_HASHES}. '
            f'Re-measure with scripts/find-vertex-programs.py.')
    bad_c = [f for f in distinct if f['writes_c']]
    if bad_c:
        f = bad_c[0]
        die(f'{len(bad_c)} program(s) write c[] (first: hash {f["hash"]:08X}, {f["file"]}+{f["offset"]:X}); '
            f'the constant registers are uploaded per draw, so such a program is CPU-interpreter-only '
            f'and must not be in this corpus.')
    for f in distinct:
        if f['len'] == 0 or f['len'] > MAX_INSTRUCTIONS:
            die(f'program hash {f["hash"]:08X} ({f["file"]}+{f["offset"]:X}) has len {f["len"]}, '
                f'outside 1..{MAX_INSTRUCTIONS}.')
        n = bin(f['input_mask']).count('1')
        if n == 0 or n > MAX_INPUTS:
            die(f'program hash {f["hash"]:08X} ({f["file"]}+{f["offset"]:X}) reads {n} inputs '
                f'(mask 0x{f["input_mask"]:04X}), outside 1..{MAX_INPUTS}.')
    # The measured properties, for the run's summary line.  Every one of these was
    # just checked against its expected value above, so they are the facts, not a
    # re-derivation.
    return {'programs': len(found), 'distinct': len(distinct), 'hashes': len(hashes),
            'collisions': len(collisions), 'writes_c': sum(f['writes_c'] for f in distinct)}


def locate_host_tool(tool_build_dir):
    """The nv2a_vsh_hlsl host tool: an existing build, else build it from the toolkit.

    The game's CMake refuses to configure without the generated corpus, so on a
    fresh checkout the game's build tree does not exist yet and cannot produce this
    tool. The toolkit configures and builds standalone, so that is the bootstrap:
    configure the toolkit alone and build just this target. Nothing about the
    generated corpus is needed to build it.
    """
    override = os.environ.get('JSRF_NV2A_VSH_HLSL')
    if override:
        p = Path(override)
        if not p.is_file():
            die(f'JSRF_NV2A_VSH_HLSL={override} does not exist.')
        return p, 'JSRF_NV2A_VSH_HLSL'
    for p in HOST_TOOL_CANDIDATES:
        if p.is_file():
            return p, str(p.relative_to(ROOT).as_posix())
    return build_host_tool(tool_build_dir)


def build_host_tool(build_dir):
    """Configure and build the toolkit's nv2a_vsh_hlsl target; return its exe."""
    if not (TOOLKIT / 'CMakeLists.txt').is_file():
        die(f'{TOOLKIT} is not a toolkit checkout: the host tool is built from there '
            f'(set XBOXRECOMP_DIR).')

    def find_exe():
        for p in (build_dir / 'src' / 'video' / 'Release' / 'nv2a_vsh_hlsl.exe',
                  build_dir / 'src' / 'video' / 'nv2a_vsh_hlsl.exe'):
            if p.is_file():
                return p
        return None

    exe = find_exe()
    if exe is None:
        print(f'gen-shaders: nv2a_vsh_hlsl.exe is not built yet; configuring the toolkit '
              f'in {build_dir} and building it', file=sys.stderr)
        build_dir.mkdir(parents=True, exist_ok=True)
        for argv in (['cmake', '-S', str(TOOLKIT), '-B', str(build_dir)],
                     ['cmake', '--build', str(build_dir), '--config', 'Release',
                      '--target', 'nv2a_vsh_hlsl']):
            cp = subprocess.run(argv, capture_output=True, text=True)
            if cp.returncode != 0:
                die(f'{" ".join(argv[:3])} failed (exit {cp.returncode}):\n'
                    f'{(cp.stdout + cp.stderr).strip()[-1500:]}')
        # Re-resolve: the single-config layout has no Release subdirectory, so the
        # path that did not exist before the build is the one that does now.
        exe = find_exe()
        if exe is None:
            die(f'the toolkit build reported success but no nv2a_vsh_hlsl.exe exists under '
                f'{build_dir}')
    try:
        return exe, str(exe.relative_to(ROOT).as_posix())
    except ValueError:
        return exe, str(exe)


def locate_vulkan_tools():
    sdk = os.environ.get('VULKAN_SDK')
    if not sdk:
        die('VULKAN_SDK is not set: this corpus is compiled offline with the Vulkan SDK\'s '
            'dxc and spirv-val, so install the SDK and set VULKAN_SDK to its root.')
    bin_dir = Path(sdk) / 'Bin'
    for exe in ('dxc.exe', 'spirv-val.exe'):
        if not (bin_dir / exe).is_file():
            die(f'{bin_dir / exe} is not there: VULKAN_SDK={sdk} does not carry the shader '
                f'tools this generator needs (dxc.exe, spirv-val.exe).')
    return bin_dir / 'dxc.exe', bin_dir / 'spirv-val.exe'


def compile_program(rec, scratch, host, dxc, spirv_val):
    """Translate and compile one program.  Returns (spirv bytes, hlsl path)."""
    tag = f'{rec["hash"]:08x}'
    bin_path = scratch / f'{tag}.bin'
    hlsl_path = scratch / f'{tag}.bin.hlsl'
    spv_path = scratch / f'{tag}.spv'
    bin_path.write_bytes(b''.join(struct.pack('<4I', *ins) for ins in rec['words']))

    def fail(step, cp, detail):
        die(f'program hash {rec["hash"]:08X} len {rec["len"]} mask 0x{rec["input_mask"]:04X} '
            f'({rec["file"]}+{rec["offset"]:X}): {step} failed (exit {cp.returncode}): '
            f'{detail.strip() or "<no output>"}')

    # The mask spelling the tool parses (nv2a_vsh_hlsl_tool.c: 0x... or decimal).
    cp = subprocess.run([str(host), '--bin', str(bin_path), '--mask', f'0x{rec["input_mask"]:x}',
                         '--vk', '--out', str(hlsl_path)],
                        capture_output=True, text=True)
    if cp.returncode != 0:
        fail('nv2a_vsh_hlsl', cp, cp.stderr)
    if not hlsl_path.is_file():
        fail('nv2a_vsh_hlsl', cp, f'no HLSL written to {hlsl_path.name}')

    # The entry name nv2a_vsh_hlsl emits (NV2A_VSH_HLSL_ENTRY in the toolkit header).
    cp = subprocess.run([str(dxc), '-spirv', '-T', 'vs_6_0', '-E', 'vs_prog',
                         '-fvk-invert-y', '-fspv-target-env=vulkan1.1',
                         '-Fo', str(spv_path), str(hlsl_path)],
                        capture_output=True, text=True)
    if cp.returncode != 0 or not spv_path.is_file():
        fail('dxc', cp, cp.stdout + cp.stderr)

    cp = subprocess.run([str(spirv_val), '--target-env', 'vulkan1.1', str(spv_path)],
                        capture_output=True, text=True)
    if cp.returncode != 0:
        fail('spirv-val', cp, cp.stdout + cp.stderr)

    spirv = spv_path.read_bytes()
    if len(spirv) % 4 != 0:
        fail('spirv size', cp, f'SPIR-V is {len(spirv)} bytes, not a multiple of 4')
    return spirv, hlsl_path


def render_c(entries):
    out = []
    out.append('/*\n'
               ' * The NV2A ahead-of-time vertex-program corpus, generated by\n'
               ' * scripts/gen-shaders.py (plan step V5) from game/default.xbe.  Do not edit:\n'
               ' * run `just shaders` and commit nothing -- this file is retail-derived,\n'
               ' * untracked generated output.\n'
               ' *\n'
               ' * Each program was translated to HLSL by the toolkit\'s nv2a_vsh_hlsl and\n'
               ' * compiled to SPIR-V (vs_6_0, -fvk-invert-y) with the Vulkan SDK\'s dxc, then\n'
               ' * validated with spirv-val, before this table existed.  The runtime never\n'
               ' * translates or compiles: a program not in the table is declined and the\n'
               ' * executor\'s CPU interpreter draws it.\n'
               ' */\n'
               '#include "nv2a_vsh_aot.h"\n\n')
    for e in entries:
        tag = f'{e["hash"]:08x}'
        out.append(f'/* hash {e["hash"]:08X}, {e["len"]} instructions, inputs 0x{e["input_mask"]:04X} */\n')
        out.append(f'static const uint32_t vp_words_{tag}[{e["len"]}][4] = {{\n')
        for ins in e['words']:
            out.append('    { ' + ', '.join(f'0x{w:08x}u' for w in ins) + ' },\n')
        out.append('};\n\n')
        words = e['spirv_words']
        out.append(f'static const uint32_t vp_spirv_{tag}[] = {{\n')
        for i in range(0, len(words), 8):
            out.append('    ' + ' '.join(f'0x{w:08x}u,' for w in words[i:i + 8]) + '\n')
        out.append('};\n\n')
    out.append('/* Ascending by hash: nv2a_vk_backend.c binary-searches this table. */\n')
    out.append('const Nv2aVshAotEntry nv2a_vsh_programs[] = {\n')
    for e in entries:
        tag = f'{e["hash"]:08x}'
        out.append(f'    {{ 0x{e["hash"]:08x}u, {e["len"]}u, 0x{e["input_mask"]:08x}u, '
                   f'0x{e["digest"]:016x}ull, vp_words_{tag}, vp_spirv_{tag}, '
                   f'sizeof vp_spirv_{tag} }},\n')
    out.append('};\n\n')
    out.append(f'const size_t nv2a_vsh_program_count = {len(entries)};\n')
    return ''.join(out)


def render_h(entries):
    return (
        '/*\n'
        ' * The NV2A ahead-of-time vertex-program corpus (V5), generated by\n'
        ' * scripts/gen-shaders.py from game/default.xbe.  Do not edit.\n'
        ' *\n'
        ' * Handed to the Vulkan back end before the guest submits work:\n'
        ' * nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count).\n'
        ' */\n'
        '#ifndef JSRF_NV2A_VSH_PROGRAMS_H\n'
        '#define JSRF_NV2A_VSH_PROGRAMS_H\n\n'
        '#include <stddef.h>\n\n'
        '#include "nv2a_vsh_aot.h"\n\n'
        '/* Ascending by hash; the back end binary-searches it. */\n'
        'extern const Nv2aVshAotEntry nv2a_vsh_programs[];\n'
        f'extern const size_t nv2a_vsh_program_count;\n\n'
        f'/* {len(entries)} programs, all of them compiled and validated offline. */\n'
        '#endif /* JSRF_NV2A_VSH_PROGRAMS_H */\n')


def build(game, scratch, host, dxc, spirv_val):
    """Generate the corpus; returns ({filename: bytes}, SPIR-V byte total, measured stats)."""
    found, scanned = extract(game)
    attach_words(found, game)
    distinct = dedupe(found)
    stats = guard(found, distinct, scanned)
    entries = []
    total = 0
    for rec in sorted(distinct, key=lambda r: r['hash']):
        spirv, _ = compile_program(rec, scratch, host, dxc, spirv_val)
        total += len(spirv)
        entries.append({'hash': rec['hash'], 'len': rec['len'], 'input_mask': rec['input_mask'],
                        'digest': aot_digest(rec['words']), 'words': rec['words'],
                        'spirv_words': list(struct.unpack(f'<{len(spirv) // 4}I', spirv))})
    files = {C_NAME: render_c(entries).encode('utf-8'),
             H_NAME: render_h(entries).encode('utf-8')}
    return files, total, stats


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', type=Path, default=ROOT / 'game',
                    help='directory holding default.xbe and Media/ (default: game)')
    ap.add_argument('--check', action='store_true',
                    help='regenerate into a temporary directory and compare with the committed '
                         'output byte for byte; change nothing')
    args = ap.parse_args(argv)

    # The digest is defined in the toolkit's header; fail here rather than emit a
    # table whose identity fields the back end would reject.
    header_digest_constants()

    host, host_from = locate_host_tool(TOOL_BUILD_DIR)
    dxc, spirv_val = locate_vulkan_tools()

    if args.check:
        with tempfile.TemporaryDirectory(prefix='gen-shaders-check-') as tmp:
            tmp_path = Path(tmp)
            files, total, stats = build(args.game, tmp_path, host, dxc, spirv_val)
            bad = 0
            for name, data in files.items():
                target = OUT_DIR / name
                if not target.is_file():
                    print(f'check: {name} is missing from {OUT_DIR}', file=sys.stderr)
                    bad += 1
                    continue
                have = target.read_bytes()
                if have != data:
                    print(f'check: {name} differs (committed {len(have)} bytes sha256 '
                          f'{hashlib.sha256(have).hexdigest()}, regenerated {len(data)} bytes sha256 '
                          f'{hashlib.sha256(data).hexdigest()})', file=sys.stderr)
                    bad += 1
                else:
                    print(f'check: {name} identical ({len(data)} bytes sha256 '
                          f'{hashlib.sha256(data).hexdigest()})')
            if bad:
                print(f'check: FAILED ({bad} file(s) differ); regenerate with `just shaders`',
                      file=sys.stderr)
                return 1
            print(f'check: OK, {stats["programs"]} programs, {stats["distinct"]} distinct, '
                  f'{stats["hashes"]} hashes, {stats["collisions"]} collisions, '
                  f'{total} SPIR-V bytes, host tool {host_from}')
            return 0

    scratch = OUT_DIR / '_scratch'
    if scratch.exists():
        shutil.rmtree(scratch)
    scratch.mkdir(parents=True)
    try:
        files, total, stats = build(args.game, scratch, host, dxc, spirv_val)
        OUT_DIR.mkdir(parents=True, exist_ok=True)
        for name, data in files.items():
            (OUT_DIR / name).write_bytes(data)
    except BaseException:
        # A failing program is identified by hash/len/mask and the tool's stderr in
        # the message itself, so the scratch is only kept when explicitly asked for.
        if os.environ.get('JSRF_KEEP_SHADER_SCRATCH'):
            print(f'gen-shaders: scratch kept at {scratch}', file=sys.stderr)
        else:
            shutil.rmtree(scratch, ignore_errors=True)
        raise
    shutil.rmtree(scratch)

    c_sha = hashlib.sha256(files[C_NAME]).hexdigest()
    print(f'shaders: {stats["programs"]} programs, {stats["distinct"]} distinct word sequences, '
          f'{stats["hashes"]} distinct hashes, {stats["collisions"]} hash collisions, '
          f'{stats["writes_c"]} write c[]; {len(files[C_NAME])} bytes of table, '
          f'{total} SPIR-V bytes, host tool {host_from}')
    print(f'shaders: {C_NAME} {len(files[C_NAME])} bytes sha256 {c_sha}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
