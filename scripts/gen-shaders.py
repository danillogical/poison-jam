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
temporary directory and compares with the committed corpus.  That comparison
blanks only the ADVISORY field (`tool.sha256`, whose binary is not reproducible on
this toolchain), so a different-but-equivalent host tool cannot make a current
corpus look stale; every other byte is compared exactly.

The corpus records its own provenance, in both emitted files: the SHA-256 of the
three toolkit translator sources, of the host tool, and the compiler's version
string and both argument vectors, plus this title's program counts.
`scripts/vsh_aot_provenance.py` is the single definition of that schema and
`scripts/build-jsrf.py` refuses to build a corpus whose recorded inputs no longer
describe how it was made.

Because generation is the only moment at which a stale tool can corrupt the
corpus, the tool is refused HERE, before it is used.  The property is that a tool
must never record a source hash for bytes it did not read, and it is established
by two independent triggers:

  * CONTENT.  The EXISTING corpus's recorded `source.*` hashes are compared with
    the sources on disk.  A difference means the sources moved since the last
    generation, so no tool from that generation may be trusted, and the tool is
    rebuilt REGARDLESS of mtime.  This is what closes the case mtime cannot see:
    an archive extraction, an `rsync --times`, or a restored backup that gives a
    stale tool a NEWER mtime than sources it never read.
  * MTIME.  A tool older than a source cannot have been built from it.  This is
    the second line of defence, for a first generation with no corpus to compare
    against.

When a rebuild is needed the generator forces one, because CMake's own rule is
"rebuild if an input is NEWER than the output" and a content change with an older
mtime looks up to date to it (measured: a plain `--target nv2a_vsh_hlsl` build left
both the object file and the executable byte-identical).  A tool this script did
not build -- an explicit `JSRF_NV2A_VSH_HLSL` override -- cannot be rebuilt or
verified by content, so its limit is printed as an explicit advisory rather than
accepted in silence.

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
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = Path(__file__).resolve().parent
TOOLKIT = Path(os.environ.get('XBOXRECOMP_DIR', str(ROOT.parent / 'xboxrecomp')))
AOT_HEADER = TOOLKIT / 'src/video/nv2a_vsh_aot.h'
OUT_DIR = ROOT / 'src/recomp/gen/shaders'
C_NAME = 'nv2a_vsh_programs.c'
H_NAME = 'nv2a_vsh_programs.h'

# The two executable layouts a CMake build tree can leave behind: multi-config
# (Visual Studio) and single-config.  One definition, used for the game build
# tree's candidates and for the bootstrap search.
_TOOL_SUFFIXES = (('src', 'video', 'Release', 'nv2a_vsh_hlsl.exe'),
                  ('src', 'video', 'nv2a_vsh_hlsl.exe'))

# The game's build tree; the toolkit is added under it as `xboxrecomp`.  The host
# tool: the same binary either CMake layout can leave behind.  Built by the
# toolkit's src/video/CMakeLists.txt.
GAME_BUILD_DIR = ROOT / 'build'
HOST_TOOL_CANDIDATES = tuple(GAME_BUILD_DIR / 'xboxrecomp' / Path(*parts)
                             for parts in _TOOL_SUFFIXES)

# Where the tool is built when no game build tree exists yet.  This is the
# bootstrap: the game's CMake refuses to configure without the generated corpus,
# so on a fresh checkout this toolkit-only build is the only way to get the tool
# that produces that corpus.  Under build/ (gitignored).
TOOL_BUILD_DIR = ROOT / 'build' / 'vsh-tool'

# How a stale tool is rebuilt, when this script owns the build tree it lives in.
# These two vectors are spelled here for the argv this script RUNS, and the
# provenance module spells the same vectors as the values it RECORDS; the module
# is loaded further down, so they cannot be imported at this point.  They are
# therefore kept equal by `check_arg_vectors`, which refuses to generate if the
# argv actually run differs from the recorded vector -- so a drift here is a hard
# failure at generation time, not a silently false provenance block.
REBUILD_HINT = 'cmake --build <dir> --config Release --target nv2a_vsh_hlsl'
TOOL_ARGS = ('--bin', '{bin}', '--mask', '{mask}', '--vk', '--out', '{out}')
DXC_ARGS = ('-spirv', '-T', 'vs_6_0', '-E', 'vs_prog', '-fvk-invert-y',
            '-fspv-target-env=vulkan1.1')

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

# The provenance schema is shared with the build guard, which re-derives the same
# values; load it by path for the same reason.
_PROV_PATH = SCRIPTS / 'vsh_aot_provenance.py'
if not _PROV_PATH.is_file():
    die(f'{_PROV_PATH} is missing: it owns the provenance schema this generator '
        f'records and scripts/build-jsrf.py re-derives.')
_prov_spec = importlib.util.spec_from_file_location('vsh_aot_provenance', _PROV_PATH)
prov = importlib.util.module_from_spec(_prov_spec)
sys.modules['vsh_aot_provenance'] = prov
_prov_spec.loader.exec_module(prov)


def check_arg_vectors():
    """The provenance module must describe the command lines this script runs.

    The block is data about a compile; if the two disagreed, the corpus would
    record a compile that did not happen, which is worse than recording nothing.
    So the ACTUAL argv this script builds is compared here, with the file operands
    substituted, against the module's vectors -- and the generator refuses to emit.
    """
    mine = host_argv('{host}', '{bin}', '{mask}', '{out}')
    if tuple(mine[1:]) != tuple(prov.TOOL_ARGS):
        die(f'the host tool argument vector recorded in {_PROV_PATH} is '
            f'{tuple(prov.TOOL_ARGS)}, but this generator runs {tuple(mine[1:])}: '
            f'update whichever moved before generating, or the corpus would record a '
            f'compile that did not happen.')
    mine = dxc_argv('{dxc}', '{out}', '{hlsl}')
    # The module records the flags that determine the SPIR-V; the output and input
    # operands are appended by `dxc_argv` and are not part of that.
    if tuple(mine[1:1 + len(DXC_ARGS)]) != tuple(prov.DXC_ARGS):
        die(f'the dxc argument vector recorded in {_PROV_PATH} is {tuple(prov.DXC_ARGS)}, '
            f'but this generator runs {tuple(mine[1:1 + len(DXC_ARGS)])}: update '
            f'whichever moved before generating, or the corpus would record a compile '
            f'that did not happen.')


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


def translator_sources():
    """The three translator sources, refusing a toolkit that is not checked out."""
    paths = prov.source_paths(TOOLKIT)
    for path in paths.values():
        if not path.is_file():
            die(f'the translator source {path} is missing: the toolkit must be checked '
                f'out beside this repo (set XBOXRECOMP_DIR otherwise), because the '
                f'corpus records the sources that produced it.')
    return paths


def newer_than_tool(host):
    """The translator sources strictly newer than `host`, newest first.

    mtime is the mechanism (see the module docstring): a tool older than any input
    that determines its output cannot have been built from that input, so using it
    would silently emit a corpus no current source can reproduce.
    """
    mtime = host.stat().st_mtime
    newer = [(p.stat().st_mtime, name, p) for name, p in translator_sources().items()
             if p.stat().st_mtime > mtime]
    return [(name, path) for _, name, path in sorted(newer, reverse=True)]


def stale_message(host, label, newer):
    return (f'the host tool {label} is STALE: it is older than '
            + ', '.join(str(p) for _, p in newer)
            + f'. A corpus generated with it would record inputs it was not built '
              f'from. Rebuild it with `{REBUILD_HINT}` (a fresh toolkit build tree '
              f'may need `cmake -S {TOOLKIT} -B <dir>` first).')


def tool_label(host):
    try:
        return str(host.relative_to(ROOT).as_posix())
    except ValueError:
        return str(host)


def _tool_in(build_dir):
    """The host tool a CMake build tree has built, in either layout.

    The game build tree adds the toolkit as the subdirectory `xboxrecomp`, so its
    tool sits one level deeper than the bootstrap's own tree's.
    """
    for root in (build_dir, build_dir / 'xboxrecomp'):
        for parts in _TOOL_SUFFIXES:
            p = root / Path(*parts)
            if p.is_file():
                return p
    return None


def rebuild_host_tool(build_dir, why):
    """Rebuild the `nv2a_vsh_hlsl` target of `build_dir`; return the new executable.

    Only ever called for a tree this script owns (the game build tree or the
    bootstrap): an explicit `JSRF_NV2A_VSH_HLSL` override is refused instead, never
    silently replaced by a different tool.
    """
    argv = ['cmake', '--build', str(build_dir), '--config', 'Release',
            '--target', 'nv2a_vsh_hlsl']
    print(f'gen-shaders: {why}; rebuilding it with `{" ".join(argv)}`', file=sys.stderr)
    cp = subprocess.run(argv, capture_output=True, text=True)
    if cp.returncode != 0:
        die(f'`{" ".join(argv)}` failed (exit {cp.returncode}):\n'
            f'{(cp.stdout + cp.stderr).strip()[-1500:]}')
    exe = _tool_in(build_dir)
    if exe is None:
        die(f'the build reported success but no nv2a_vsh_hlsl.exe exists under {build_dir}')
    return exe


def owning_build_dir(host, tool_build_dir):
    """The build tree `host` was built in, when it is one this script can rebuild."""
    for tree in (GAME_BUILD_DIR, tool_build_dir):
        if _tool_in(tree) == host:
            return tree
    return None


def recorded_sources():
    """`{name: sha256}` as the EXISTING corpus records them, or None if there is none.

    This is the content record of the sources that produced the last generation, and
    it is what makes the freshness proof more than mtime.  An archive extraction, an
    `rsync --times`, or a restored backup can give a stale tool a NEWER mtime than
    the sources it never read; mtime alone then calls it fresh.  Comparing the
    recorded hashes against the sources on disk catches that: if they differ, the
    sources have moved since the last generation, so no tool from that generation
    may be trusted on mtime grounds.

    Returns None when the corpus does not exist yet (first generation, no record) or
    when it carries no readable block -- in both cases there is no content record and
    the mtime rule is all there is.
    """
    for name in (C_NAME, H_NAME):
        path = OUT_DIR / name
        if not path.is_file():
            return None
        blocks, _problems = prov.parse_block(path.read_text(encoding='utf-8', errors='replace'))
        if len(blocks) == 1:
            return {k[len('source.'):]: v for k, v in blocks[0].items()
                    if k.startswith('source.')}
    return None


def sources_changed_since_last_generation():
    """`{name: (recorded, now)}` for sources the existing corpus recorded differently.

    Empty when there is no corpus yet, or when every recorded source hash equals the
    source on disk.  A non-empty result means the tool that produced that corpus
    predates this content and cannot be trusted on mtime alone.
    """
    recorded = recorded_sources()
    if not recorded:
        return {}
    now = prov.source_hashes(TOOLKIT)
    return {name: (recorded.get(name), now.get(name))
            for name in now if recorded.get(name) != now.get(name)}


def force_tool_rebuild(build_dir, why):
    """Rebuild the tool even though CMake may see its objects as up to date.

    Needed because CMake's rule is "rebuild if an input is NEWER than the output": a
    source whose content changed but whose mtime was set BELOW the tool's (archive
    extraction, `rsync --times`, a restored backup) looks up to date to it, and
    `cmake --build` would return the stale binary unchanged.  Measured on this host:
    with the source's content edited and its mtime set to the epoch, a plain
    `cmake --build --target nv2a_vsh_hlsl` reported success and left BOTH the object
    file and the executable byte-identical, so mtime is genuinely the only lever
    CMake has here.

    The forcing discards this target's own object files, which makes the compiler run
    whatever the timestamps say; it is scoped to this build tree and disturbs no other
    target.  Only when no object file is found (an unexpected layout) does it fall
    back to touching the sources, which is the one remaining lever.
    """
    build_dir = Path(build_dir)
    obj_dir = build_dir / 'xboxrecomp' / 'src' / 'video' / 'nv2a_vsh_hlsl.dir'
    if not obj_dir.is_dir():
        obj_dir = build_dir / 'src' / 'video' / 'nv2a_vsh_hlsl.dir'
    removed = sorted(obj_dir.rglob('*.obj')) if obj_dir.is_dir() else []
    for obj in removed:
        obj.unlink()
    if removed:
        detail = f'{len(removed)} object file(s) discarded'
    else:
        now = time.time()
        for path in translator_sources().values():
            os.utime(path, (now, now))
        detail = 'no object files were found, so the sources were touched instead'
    print(f'gen-shaders: {why}; forcing a recompile of nv2a_vsh_hlsl in {build_dir} '
          f'({detail})', file=sys.stderr)
    return rebuild_host_tool(build_dir, 'the tool must be rebuilt from the current sources')


def fresh_or_rebuild(host, tool_build_dir):
    """`host` if it can be PROVEN current, else rebuild it in its own tree.

    Two independent reasons to rebuild, in order of strength:

    1. CONTENT: the existing corpus records source hashes that differ from the
       sources on disk.  The sources moved since the last generation, so a tool from
       that generation may not be used to record the new content -- regardless of
       mtime, which is what the archive/`rsync --times`/restored-backup cases defeat.
    2. MTIME: the tool is older than a source, so it cannot have been built from it.

    If neither fires, the tool is used as-is.  If a rebuild is needed but the tool is
    not one this script can rebuild, it refuses with the rebuild hint.
    """
    changed = sources_changed_since_last_generation()
    newer = newer_than_tool(host)
    if not changed and not newer:
        return host
    label = tool_label(host)
    tree = owning_build_dir(host, tool_build_dir)
    if tree is None:
        if changed:
            names = ', '.join(sorted(changed))
            die(f'the host tool {label} cannot be trusted: the corpus records different '
                f'content for {names}, so the sources moved since it was built, and it is '
                f'not one this script can rebuild (it is not under {GAME_BUILD_DIR} or '
                f'{tool_build_dir}). Rebuild it with `{REBUILD_HINT}` and re-run.')
        die(stale_message(host, label, newer))
    if changed:
        why = ('the corpus records different source content for ' + ', '.join(sorted(changed))
               + ', so the host tool predates the current sources')
    else:
        why = stale_message(host, label, newer)
    rebuilt = force_tool_rebuild(tree, why)
    newer = newer_than_tool(rebuilt)
    if newer:
        die(f'{tool_label(rebuilt)} is still older than '
            + ', '.join(str(p) for _, p in newer)
            + ' after rebuilding it; check the clock and the build tree '
              f'(`{REBUILD_HINT}`).')
    return rebuilt


def find_host_tool(tool_build_dir):
    """The host tool already present on this host, or None.  Builds nothing.

    This is the ONE definition of "which tool would this host use", and both this
    generator and `scripts/build-jsrf.py` read it: the guard re-derives the
    corpus's provenance from the same binary the generator would have run, so the
    order -- an explicit `JSRF_NV2A_VSH_HLSL` override, the game build tree's
    candidate, then the bootstrap tree -- must not be spelled a second time.
    """
    override = os.environ.get('JSRF_NV2A_VSH_HLSL')
    if override:
        p = Path(override)
        return p if p.is_file() else None
    for p in HOST_TOOL_CANDIDATES:
        if p.is_file():
            return p
    return _tool_in(tool_build_dir)


def rebuild_reasons(host):
    """`(changed, newer)`: why `host` may not be used to record the current sources."""
    return sources_changed_since_last_generation(), newer_than_tool(host)


def override_advisory(label, changed, newer):
    """The explicit LIMIT of what an override can prove.  Printed, never a refusal.

    An override is a legitimate escape hatch, but it is a tool this script did not
    build, so nothing available here can establish that the binary was built from the
    sources on disk: the tool hash is not reproducible on this toolchain (MSVC embeds
    a PDB signature), so it cannot be checked by content either.  Accepting it in
    silence would present an unproven tool as a verified one -- exactly the failure
    this whole mechanism exists to prevent -- so the limit is stated instead, and the
    recorded `tool.sha256` is named for what it is: the only trace of which binary
    ran, not evidence that it is current.
    """
    reasons = []
    if changed:
        reasons.append('the corpus records different content for '
                       + ', '.join(sorted(changed)))
    if newer:
        reasons.append('it is older than ' + ', '.join(str(p) for _, p in newer))
    return (f'gen-shaders: advisory: the host tool {label} cannot be PROVEN fresh ('
            + '; '.join(reasons)
            + '), and a tool named explicitly is never rebuilt in place. The corpus will '
              'record its sha256 as the only trace of which binary produced it; that '
              'field is advisory, so the block cannot be shown to come from the sources '
              'on disk. Unset JSRF_NV2A_VSH_HLSL to let this script rebuild the tool it '
              f'owns, or rebuild this one with `{REBUILD_HINT}`.')


def locate_host_tool(tool_build_dir):
    """The nv2a_vsh_hlsl host tool: an existing build, else build it from the toolkit.

    Whichever way it is found -- an explicit override, a candidate under the game
    build tree, or the bootstrap -- it must be shown current before it is used: by
    the corpus's recorded source CONTENT, and by mtime.  The game's CMake refuses to
    configure without the generated corpus, so on a fresh checkout the game's build
    tree does not exist yet and cannot produce this tool; the toolkit configures and
    builds standalone, so that is the bootstrap.

    An explicit override is the one case that cannot be verified and cannot be
    rebuilt, so its limit is stated loudly rather than assumed away.
    """
    override = os.environ.get('JSRF_NV2A_VSH_HLSL')
    if override and not Path(override).is_file():
        die(f'JSRF_NV2A_VSH_HLSL={override} does not exist.')
    host = find_host_tool(tool_build_dir)
    if host is None:
        return build_host_tool(tool_build_dir)
    if override:
        label = f'JSRF_NV2A_VSH_HLSL={override}'
        changed, newer = rebuild_reasons(host)
        if changed or newer:
            print(override_advisory(label, changed, newer), file=sys.stderr)
        return host, label
    return fresh_or_rebuild(host, tool_build_dir), tool_label(host)


def build_host_tool(build_dir):
    """Configure and build the toolkit's nv2a_vsh_hlsl target; return its exe."""
    if not (TOOLKIT / 'CMakeLists.txt').is_file():
        die(f'{TOOLKIT} is not a toolkit checkout: the host tool is built from there '
            f'(set XBOXRECOMP_DIR).')

    exe = _tool_in(build_dir)
    if exe is None:
        print(f'gen-shaders: nv2a_vsh_hlsl.exe is not built yet; configuring the toolkit '
              f'in {build_dir} and building it', file=sys.stderr)
        build_dir.mkdir(parents=True, exist_ok=True)
        argv = ['cmake', '-S', str(TOOLKIT), '-B', str(build_dir)]
        cp = subprocess.run(argv, capture_output=True, text=True)
        if cp.returncode != 0:
            die(f'`{" ".join(argv)}` failed (exit {cp.returncode}):\n'
                f'{(cp.stdout + cp.stderr).strip()[-1500:]}')
        # Re-resolve: the single-config layout has no Release subdirectory, so the
        # path that did not exist before the build is the one that does now.
        exe = rebuild_host_tool(build_dir, 'the toolkit build tree has no host tool yet')
    else:
        exe = fresh_or_rebuild(exe, build_dir)
    return exe, tool_label(exe)


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


def host_argv(host, bin_path, mask, out_path):
    """The host tool's command line, built FROM `TOOL_ARGS` with its operands filled in.

    Building the command line out of the recorded vector (rather than passing a
    hand-written list and asserting it equals the vector) makes the two the same
    object: the block cannot describe a compile this script did not run, because
    there is only one place the arguments are spelled.
    """
    return [str(host)] + [part.format(bin=str(bin_path), mask=mask, out=str(out_path))
                          for part in TOOL_ARGS]


def dxc_argv(dxc, out_path, hlsl_path):
    """dxc's command line: the recorded vector, then the output and the input."""
    return [str(dxc), *DXC_ARGS, '-Fo', str(out_path), str(hlsl_path)]


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
    cp = subprocess.run(host_argv(host, bin_path, f'0x{rec["input_mask"]:x}', hlsl_path),
                        capture_output=True, text=True)
    if cp.returncode != 0:
        fail('nv2a_vsh_hlsl', cp, cp.stderr)
    if not hlsl_path.is_file():
        fail('nv2a_vsh_hlsl', cp, f'no HLSL written to {hlsl_path.name}')

    # The entry name nv2a_vsh_hlsl emits (NV2A_VSH_HLSL_ENTRY in the toolkit header).
    cp = subprocess.run(dxc_argv(dxc, spv_path, hlsl_path),
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


def provenance(host, dxc, stats):
    """The provenance block body for the corpus generated NOW.

    `check_arg_vectors` has already refused if this script's command lines differ
    from the recorded ones, so the block describes exactly what ran. The counts
    come from `guard`'s measurement of this title, so a table whose own row count
    disagrees with its block is a refusal on the build side.
    """
    return prov.render_block(prov.expected(TOOLKIT, host, dxc,
                                           programs=stats['programs'],
                                           distinct=stats['distinct']))


def render_c(entries, block):
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
               ' *\n'
               ' * Provenance: the inputs that determine this SPIR-V.  scripts/build-jsrf.py\n'
               ' * re-derives them and refuses to build when one has moved, so a stale corpus\n'
               ' * cannot be linked silently.  Regenerate with `just shaders`.\n'
               f'{block}\n'
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


def render_h(entries, block):
    return (
        '/*\n'
        ' * The NV2A ahead-of-time vertex-program corpus (V5), generated by\n'
        ' * scripts/gen-shaders.py from game/default.xbe.  Do not edit.\n'
        ' *\n'
        ' * Handed to the Vulkan back end before the guest submits work:\n'
        ' * nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count).\n'
        ' *\n'
        ' * Provenance: the inputs that determine this SPIR-V.  scripts/build-jsrf.py\n'
        ' * re-derives them and refuses to build when one has moved, so a stale corpus\n'
        ' * cannot be linked silently.  Regenerate with `just shaders`.\n'
        f'{block}\n'
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


def assert_tool_current(host, label, rebuildable):
    """Refuse to generate if a tool this script OWNS is older than a translator source.

    The property R1 must guarantee: a tool must never be used to record a source hash
    for bytes it did not read.  `fresh_or_rebuild` has already rebuilt or refused on
    both triggers, so this is the backstop -- it makes the invariant explicit at the
    point of use rather than leaving it implicit in the resolution path.

    `rebuildable` is False only for an explicit `JSRF_NV2A_VSH_HLSL` override, which
    is a deliberate escape hatch: it is a tool this script did not build and cannot
    verify by content (the tool hash is not reproducible), so refusing it would
    remove the escape hatch while proving nothing.  `locate_host_tool` has already
    printed the explicit advisory that its freshness is UNPROVEN, which is the honest
    treatment of that case; here it is exempted from the backstop for the same reason.
    """
    if not rebuildable:
        return
    newer = newer_than_tool(host)
    if newer:
        die(stale_message(host, label, newer)
            + ' Nothing will be generated with it: the corpus would record source '
              'hashes the tool never read.')


def build(game, scratch, host, dxc, spirv_val):
    """Generate the corpus; returns ({filename: bytes}, SPIR-V byte total, measured stats)."""
    found, scanned = extract(game)
    attach_words(found, game)
    distinct = dedupe(found)
    stats = guard(found, distinct, scanned)
    block = provenance(host, dxc, stats)
    entries = []
    total = 0
    for rec in sorted(distinct, key=lambda r: r['hash']):
        spirv, _ = compile_program(rec, scratch, host, dxc, spirv_val)
        total += len(spirv)
        entries.append({'hash': rec['hash'], 'len': rec['len'], 'input_mask': rec['input_mask'],
                        'digest': aot_digest(rec['words']), 'words': rec['words'],
                        'spirv_words': list(struct.unpack(f'<{len(spirv) // 4}I', spirv))})
    files = {C_NAME: render_c(entries, block).encode('utf-8'),
             H_NAME: render_h(entries, block).encode('utf-8')}
    return files, total, stats


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game', type=Path, default=ROOT / 'game',
                    help='directory holding default.xbe and Media/ (default: game)')
    ap.add_argument('--check', action='store_true',
                    help='regenerate into a temporary directory and compare with the committed '
                         'output (the advisory tool.sha256 line is blanked on both sides; '
                         'change nothing)')
    args = ap.parse_args(argv)

    # The digest is defined in the toolkit's header; fail here rather than emit a
    # table whose identity fields the back end would reject.
    header_digest_constants()
    # The block must describe the command lines this run uses, not other ones.
    check_arg_vectors()

    host, host_from = locate_host_tool(TOOL_BUILD_DIR)
    # A tool this script owns can be rebuilt and so must be provably current; an
    # override elsewhere is the one case it cannot prove or fix, and
    # `locate_host_tool` has already said so out loud.
    assert_tool_current(host, host_from,
                        rebuildable=owning_build_dir(host, TOOL_BUILD_DIR) is not None)
    dxc, spirv_val = locate_vulkan_tools()
    host_sha = prov.sha256_file(host)

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
                # Compare with the ADVISORY fields blanked on both sides: a different
                # but equivalent host tool binary must not make a current corpus look
                # stale.  Everything outside those lines is compared exactly.
                if prov.strip_advisory(have.decode('utf-8', 'replace')) != \
                   prov.strip_advisory(data.decode('utf-8', 'replace')):
                    print(f'check: {name} differs (committed {len(have)} bytes sha256 '
                          f'{hashlib.sha256(have).hexdigest()}, regenerated {len(data)} bytes sha256 '
                          f'{hashlib.sha256(data).hexdigest()})', file=sys.stderr)
                    bad += 1
                elif have != data:
                    # Only advisory lines differ: report it, do not fail.  The corpus is
                    # current; the recorded tool hash is a different binary.
                    print(f'check: {name} current; only the advisory provenance differs '
                          f'({len(data)} bytes, committed sha256 '
                          f'{hashlib.sha256(have).hexdigest()}, regenerated '
                          f'{hashlib.sha256(data).hexdigest()})')
                else:
                    print(f'check: {name} identical ({len(data)} bytes sha256 '
                          f'{hashlib.sha256(data).hexdigest()})')
            if bad:
                print(f'check: FAILED ({bad} file(s) differ); regenerate with `just shaders`',
                      file=sys.stderr)
                return 1
            print(f'check: OK, {stats["programs"]} programs, {stats["distinct"]} distinct, '
                  f'{stats["hashes"]} hashes, {stats["collisions"]} collisions, '
                  f'{total} SPIR-V bytes, host tool {host_from} sha256 {host_sha}')
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
          f'{total} SPIR-V bytes, host tool {host_from} sha256 {host_sha}')
    print(f'shaders: {C_NAME} {len(files[C_NAME])} bytes sha256 {c_sha}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
