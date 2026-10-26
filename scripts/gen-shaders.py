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
temporary directory and compares the result with the committed corpus BYTE FOR
BYTE.  There is no exempt field: the block now records a decisive tool identity,
so a regenerated file that differs anywhere -- including its `tool.sha256` line --
is reported as a stale corpus that `just shaders` must regenerate.  `--check`
resolves the host tool by FINDING it (`check_host_tool` -> `find_host_tool`): it
never configures, builds or writes anything, so a check cannot move the decisive
`tool.sha256` it is checking, and it refuses -- naming `just shaders` -- when the
tool the corpus records is absent, or when that tool may not stand for the sources
the corpus records (both triggers of the freshness rule below, applied exactly as
`scripts/build-jsrf.py` applies them).

THE ONE TOOLKIT (generator == CMake link == build guard)
--------------------------------------------------------
The three translator sources the corpus records are read from ONE resolved toolkit
root: `resolve_toolkit()` prefers the build tree's cached `XBOXRECOMP_DIR`, falls
back to `$XBOXRECOMP_DIR`, then to the sibling checkout -- and REFUSES when the
cache and the environment name different trees, because CMake links the cached one
while the generator and the guard would read the other.  `scripts/build-jsrf.py`
resolves it once through this same function and reports it in preflight, so no
third, hard-coded path can disagree with what is linked.

The host tool whose SHA-256 is recorded is the artifact this repository OWNS
(`build/vsh-tool`), and that tree is itself a CMake build tree of ONE toolkit: the
configure that created it recorded its toolkit in the tree's own cache as
`CMAKE_HOME_DIRECTORY`, and `cmake --build` in it compiles THAT tree from then on.
So the tree is read back and compared with the resolved toolkit before its artifact
is used or rebuilt, and a tree configured against a different checkout is REFUSED
(`toolkit_binding_conflict`, in both this generator and the build guard): a rebuild
there would produce a binary built from one toolkit while the corpus recorded the
other's source hashes -- the same "recorded identity describes a binary nobody
verified" defect one level down.

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
    generation, so no tool from that generation may be trusted, and an owned tool
    is rebuilt REGARDLESS of mtime.  This is what closes the case mtime cannot see:
    an archive extraction, an `rsync --times`, or a restored backup that gives a
    stale tool a NEWER mtime than sources it never read.
  * MTIME.  A tool older than a source cannot have been built from it.  This is
    the second line of defence, for a first generation with no corpus to compare
    against.

When a rebuild is needed the generator forces one, because CMake's own rule is
"rebuild if an input is NEWER than the output" and a content change with an older
mtime looks up to date to it (measured: a plain `--target nv2a_vsh_hlsl` build left
both the object file and the executable byte-identical).

An explicit `JSRF_NV2A_VSH_HLSL` override is the ONE tool whose freshness this
script cannot re-derive, because it must not rebuild or replace a path that came
from the environment.  It is therefore REFUSED when either trigger fires, naming
the override and both remedies (rebuild it yourself, or unset the variable and let
this script build the tool it owns).  An advisory is not a freshness check: a
stale override that is merely warned about is a stale tool recording hashes for
bytes it never read, which is the defect this whole mechanism exists to prevent.

The tool whose hash is RECORDED is the artifact this repository owns -- the
`nv2a_vsh_hlsl.exe` under `build/vsh-tool` -- and not the game build tree's copy
of it.  `just build` relinks that copy (MSVC embeds a PDB signature, so its hash
moves without an input moving), and this script runs before that tree is even
configured on a fresh checkout; pinning the record to the owned artifact makes the
recorded identity stable across a game build and identifiable afterwards.

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
`JSRF_NV2A_VSH_HLSL` overrides the tool's path when it lives elsewhere; a stale
override is refused, never rebuilt or replaced.

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
OUT_DIR = ROOT / 'src/recomp/gen/shaders'
C_NAME = 'nv2a_vsh_programs.c'
H_NAME = 'nv2a_vsh_programs.h'

# Executables carry `.exe` on Windows only.
EXE_SUFFIX = '.exe' if os.name == 'nt' else ''


def vulkan_sdk_bin(sdk):
    """The Vulkan SDK's tool directory: `Bin` on Windows, `bin` on the other hosts' SDKs.

    The spelling the host's own SDK uses is tried first and the other one second; when
    neither exists the host's spelling is returned, so the caller's missing-tool refusal
    names it.  On Windows this is `<sdk>/Bin`, as it always was.
    """
    names = ('Bin', 'bin') if os.name == 'nt' else ('bin', 'Bin')
    for name in names:
        if (Path(sdk) / name).is_dir():
            return Path(sdk) / name
    return Path(sdk) / names[0]


# The two executable layouts a CMake build tree can leave behind: multi-config
# (Visual Studio) and single-config.  Only the owned build tree below uses this.
_TOOL_SUFFIXES = (('src', 'video', 'Release', 'nv2a_vsh_hlsl' + EXE_SUFFIX),
                  ('src', 'video', 'nv2a_vsh_hlsl' + EXE_SUFFIX))

# The game's build tree.  It is named here for the CMakeCache lookup that binds
# the toolkit, NOT as a place to look for the host tool: `just build` relinks
# `build/xboxrecomp/.../nv2a_vsh_hlsl.exe` (MSVC embeds a PDB signature, so its
# hash moves without an input moving), and on a fresh checkout this guard runs
# before that tree is configured at all -- so that copy can never be the binary
# whose hash the corpus records.
GAME_BUILD_DIR = ROOT / 'build'

# Where the tool is built when no game build tree exists yet.  This is the
# bootstrap: the game's CMake refuses to configure without the generated corpus,
# so on a fresh checkout this toolkit-only build is the only way to get the tool
# that produces that corpus.  Under build/ (gitignored).  It is also the ONLY
# build tree this script owns, and therefore the only one whose nv2a_vsh_hlsl it
# will rebuild: its artifact is the one the corpus records (see the module
# docstring).
TOOL_BUILD_DIR = GAME_BUILD_DIR / 'vsh-tool'

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
# values; load it by path for the same reason.  ONE instance per process, though:
# `scripts/build-jsrf.py` loads this same file before it loads this script, and a
# second copy loaded here would define a second `ProvenanceError` class -- so a
# refusal raised here (an unreadable translator source, a cache/environment
# conflict) would NOT be caught by that script's `except prov.ProvenanceError` and
# would surface as a traceback instead of the refusal it is.
def _same_file(a, b):
    try:
        return (os.path.normcase(os.path.realpath(str(a)))
                == os.path.normcase(os.path.realpath(str(b))))
    except (OSError, ValueError):
        return False


_PROV_PATH = SCRIPTS / 'vsh_aot_provenance.py'
if not _PROV_PATH.is_file():
    die(f'{_PROV_PATH} is missing: it owns the provenance schema this generator '
        f'records and scripts/build-jsrf.py re-derives.')
_loaded_prov = sys.modules.get('vsh_aot_provenance')
if _loaded_prov is not None and getattr(_loaded_prov, '__file__', None) \
        and _same_file(_loaded_prov.__file__, _PROV_PATH):
    prov = _loaded_prov
else:
    _prov_spec = importlib.util.spec_from_file_location('vsh_aot_provenance', _PROV_PATH)
    prov = importlib.util.module_from_spec(_prov_spec)
    sys.modules['vsh_aot_provenance'] = prov
    _prov_spec.loader.exec_module(prov)


# The cache parsing and the three-way toolkit resolution live in scripts/toolkit_path.py,
# shared with every other script that needs the toolkit; one instance per process, like
# the provenance module above.
_TP_PATH = SCRIPTS / 'toolkit_path.py'
if not _TP_PATH.is_file():
    die(f'{_TP_PATH} is missing: it owns the toolkit resolution this generator applies.')
_loaded_tp = sys.modules.get('toolkit_path')
if _loaded_tp is not None and getattr(_loaded_tp, '__file__', None) \
        and _same_file(_loaded_tp.__file__, _TP_PATH):
    toolkit_path = _loaded_tp
else:
    _tp_spec = importlib.util.spec_from_file_location('toolkit_path', _TP_PATH)
    toolkit_path = importlib.util.module_from_spec(_tp_spec)
    sys.modules['toolkit_path'] = toolkit_path
    _tp_spec.loader.exec_module(toolkit_path)

# The game's own build tree is `GAME_BUILD_DIR`; `resolve_toolkit` reads its cache so the
# guard, the generator and the `add_subdirectory` cannot drift onto different checkouts
# (see the module docstring).
CACHE_NAME = toolkit_path.CACHE_NAME
CACHE_RE = toolkit_path.CACHE_RE
# The OWNED host-tool tree's own configure root (see `toolkit_binding_conflict`).
HOME_DIR_RE = toolkit_path.HOME_DIR_RE
cache_entry = toolkit_path.cache_entry


def cached_toolkit(build_dir=None):
    """`XBOXRECOMP_DIR` as the build tree's CMakeCache recorded it, or None.

    A missing cache, a missing entry and an empty value are all "the build tree
    has not spoken": the caller falls through to the environment and then to the
    sibling checkout.
    """
    raw = cache_entry(Path(build_dir or GAME_BUILD_DIR) / CACHE_NAME, CACHE_RE)
    if raw is None:
        return None
    value = Path(raw)
    # CMake stores an absolute path in a PATH cache entry.  A relative one could
    # only come from a hand-edited cache, and the directory configure ran in is
    # this repository, so that is what it is resolved against.
    return value if value.is_absolute() else (ROOT / value)


def owned_tool_toolkit(tool_build_dir=None):
    """The toolkit the OWNED host-tool tree was configured against, or None.

    `build/vsh-tool` is one CMake build tree of ONE toolkit: its cache records that
    toolkit as `CMAKE_HOME_DIRECTORY`, and `cmake --build` in it compiles THAT tree's
    sources from then on -- whichever toolkit this repository resolves now.  So the
    tree's identity has to be read back before its artifact is used or rebuilt; the
    `tool.sha256` the corpus records is otherwise an identity nobody verified (the
    defect R1R exists to close, one level down).

    None means the tree has not been configured (or the entry is missing), which is
    not a problem on its own: a caller may supply a bare tool file, and the bootstrap
    configures the tree itself -- the tool-hash comparison and the fail-closed
    `cmake --build` are the checks then.
    """
    raw = cache_entry(Path(tool_build_dir or TOOL_BUILD_DIR) / CACHE_NAME, HOME_DIR_RE)
    if raw is None:
        return None
    value = Path(raw)
    return value if value.is_absolute() else (ROOT / value)


def toolkit_binding_conflict(toolkit, tool_build_dir=None, label=None):
    """Why the owned tool tree may not be used for `toolkit`, or None.

    The ONE wording of this refusal, so the generator (`main`) and the build guard
    (`scripts/build-jsrf.py`, which imports this) name the same trees and the same
    remedy.  The condition is a cache that EXISTS and names another toolkit: the
    owned tree's build system is then rooted in a checkout that is not the one whose
    sources the corpus records, so a rebuild there would produce a binary from one
    toolkit while the corpus recorded the other's source hashes.

    `JSRF_NV2A_VSH_HLSL` being set does not exempt the tree: a run that used the
    override would still leave `build/vsh-tool` rooted in the wrong checkout, so the
    next run that falls back to the owned artifact would rebuild it from there.
    """
    tree = Path(tool_build_dir or TOOL_BUILD_DIR)
    configured = owned_tool_toolkit(tree)
    if configured is None:
        return None
    if os.path.normcase(os.path.realpath(str(configured))) == os.path.normcase(
            os.path.realpath(str(toolkit))):
        return None
    resolved = f'{toolkit}' + (f' ({label})' if label else '')
    return (
        f'the host-tool build tree {tree} was configured against a DIFFERENT toolkit: its '
        f'{CACHE_NAME} records CMAKE_HOME_DIRECTORY={configured}, but the toolkit this build '
        f'resolves and links is {resolved}. Rebuilding in {tree} would compile {configured}\'s '
        f'sources into the executable whose SHA-256 the corpus records as {toolkit}\'s, so the '
        f'recorded identity would describe a binary built from other code. Remedy: delete {tree} '
        f'and re-run `just shaders`, which reconfigures it against {toolkit}; or point the build '
        f'tree (its cached XBOXRECOMP_DIR, i.e. -DXBOXRECOMP_DIR=<path>) and XBOXRECOMP_DIR at '
        f'{configured}, the toolkit that tree already uses.')


def resolve_toolkit(build_dir=None):
    """The ONE toolkit root this repository binds to: `(path, label)`.

    The three-way resolution (cached `XBOXRECOMP_DIR`, `$XBOXRECOMP_DIR`, sibling) and
    the REFUSAL when the cache and the environment disagree live in
    `scripts/toolkit_path.py`, shared with every other script; see its docstring.

    On top of that, the winner must be a toolkit checkout -- `CMakeLists.txt` and all
    three translator sources -- because a toolkit that is merely *named* proves nothing
    about the code the corpus was built from.
    """
    try:
        root, label = toolkit_path.toolkit_path(
            build_dir or GAME_BUILD_DIR, game_root=ROOT, with_label=True)
    except toolkit_path.ToolkitPathError as error:
        raise prov.ProvenanceError(str(error)) from None
    missing = [p for p in [root / 'CMakeLists.txt', *prov.source_paths(root).values()]
               if not p.is_file()]
    if missing:
        raise prov.ProvenanceError(
            f'the toolkit {root} ({label}) is not a toolkit checkout: it is missing '
            + ', '.join(str(p) for p in missing)
            + '. It must carry CMakeLists.txt and the three translator sources ('
            + ', '.join(prov.SOURCE_NAMES) + '); point the build tree at the right tree '
            'with -DXBOXRECOMP_DIR=<path>, set XBOXRECOMP_DIR, or check the toolkit out '
            'beside this repo, then re-run `just shaders`.')
    return root, label


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


def header_digest_constants(toolkit):
    """The digest constants as nv2a_vsh_aot.h actually defines them."""
    header = Path(toolkit) / 'src/video/nv2a_vsh_aot.h'
    if not header.is_file():
        die(f'{header} is missing: the toolkit must be checked out beside this '
            f'repo (set XBOXRECOMP_DIR otherwise); the generated table includes its header.')
    text = header.read_text(encoding='utf-8')
    found = {}
    for name in ('NV2A_VSH_AOT_DIGEST_SEED', 'NV2A_VSH_AOT_DIGEST_PRIME'):
        m = re.search(r'#define\s+' + name + r'\s+(0[xX][0-9a-fA-F]+)', text)
        if not m:
            die(f'{header} no longer defines {name}: the identity digest changed, '
                f'so this generator must be updated with it.')
        found[name] = int(m.group(1), 16)
    seed, prime = found['NV2A_VSH_AOT_DIGEST_SEED'], found['NV2A_VSH_AOT_DIGEST_PRIME']
    if (seed, prime) != (DIGEST_SEED, DIGEST_PRIME):
        die(f'{header} digest constants are seed=0x{seed:X} prime=0x{prime:X}, but this '
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


def translator_sources(toolkit):
    """The three translator sources, refusing a toolkit that is not checked out."""
    paths = prov.source_paths(toolkit)
    for path in paths.values():
        if not path.is_file():
            die(f'the translator source {path} is missing: the toolkit must be checked '
                f'out beside this repo (set XBOXRECOMP_DIR otherwise), because the '
                f'corpus records the sources that produced it.')
    return paths


def newer_than_tool(host, toolkit):
    """The translator sources strictly newer than `host`, newest first.

    mtime is the mechanism (see the module docstring): a tool older than any input
    that determines its output cannot have been built from that input, so using it
    would silently emit a corpus no current source can reproduce.
    """
    mtime = host.stat().st_mtime
    newer = [(p.stat().st_mtime, name, p) for name, p in translator_sources(toolkit).items()
             if p.stat().st_mtime > mtime]
    return [(name, path) for _, name, path in sorted(newer, reverse=True)]


def stale_message(host, label, newer, toolkit):
    return (f'the host tool {label} is STALE: it is older than '
            + ', '.join(str(p) for _, p in newer)
            + f'. A corpus generated with it would record inputs it was not built '
              f'from. Rebuild it with `{REBUILD_HINT}` (a fresh toolkit build tree '
              f'may need `cmake -S {toolkit} -B <dir>` first).')


def tool_label(host):
    try:
        return str(host.relative_to(ROOT).as_posix())
    except ValueError:
        return str(host)


def _tool_in(build_dir):
    """The host tool a CMake build tree has built, in either layout.

    Only the owned bootstrap tree (`build/vsh-tool`) is searched through this, but
    it is written for both layouts a CMake tree can take (`<dir>/src/video[/Release]`)
    and for a tree whose own nested `xboxrecomp` subdirectory holds the toolkit, so
    the same lookup stays correct if the owned tree is ever configured differently.
    """
    for root in (build_dir, build_dir / 'xboxrecomp'):
        for parts in _TOOL_SUFFIXES:
            p = root / Path(*parts)
            if p.is_file():
                return p
    return None


def rebuild_host_tool(build_dir, why):
    """Rebuild the `nv2a_vsh_hlsl` target of `build_dir`; return the new executable.

    Only ever called for the tree this script owns (`build/vsh-tool`): an explicit
    `JSRF_NV2A_VSH_HLSL` override is refused instead, never silently replaced by a
    different tool, and the game build tree is not a candidate for the recorded
    identity at all.
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
    """The build tree `host` was built in, when it is one this script can rebuild.

    Only the owned tree counts.  The game build tree is NOT a place this script
    looks for a host tool at all (see `find_host_tool` and the module docstring),
    so a binary that lives there is never "owned" -- and a tool named by
    `JSRF_NV2A_VSH_HLSL` is explicitly not this script's to rebuild or replace.
    """
    return tool_build_dir if _tool_in(tool_build_dir) == host else None


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


def sources_changed_since_last_generation(toolkit):
    """`{name: (recorded, now)}` for sources the existing corpus recorded differently.

    Empty when there is no corpus yet, or when every recorded source hash equals the
    source on disk.  A non-empty result means the tool that produced that corpus
    predates this content and cannot be trusted on mtime alone.
    """
    recorded = recorded_sources()
    if not recorded:
        return {}
    now = prov.source_hashes(toolkit)
    return {name: (recorded.get(name), now.get(name))
            for name in now if recorded.get(name) != now.get(name)}


def tool_staleness(host, toolkit):
    """`(changed, newer)`: why `host` may not be used to record the current sources.

    `changed` is the CONTENT record: `{name: (recorded, now)}` for translator
    sources the existing corpus recorded differently (`{}` when there is no corpus
    to compare against).  `newer` is the MTIME record: the translator sources
    strictly newer than `host`, newest first.

    Public because the build guard calls it: the guard must reach the same verdict
    about a tool as the generator would, and this is the ONE definition of that
    verdict.  Both triggers mean the same thing -- the tool cannot be shown to have
    read the sources the corpus names -- so any non-empty result is a refusal at
    generation time.
    """
    return sources_changed_since_last_generation(toolkit), newer_than_tool(host, toolkit)


def force_tool_rebuild(build_dir, why, toolkit):
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
        for path in translator_sources(toolkit).values():
            os.utime(path, (now, now))
        detail = 'no object files were found, so the sources were touched instead'
    print(f'gen-shaders: {why}; forcing a recompile of nv2a_vsh_hlsl in {build_dir} '
          f'({detail})', file=sys.stderr)
    return rebuild_host_tool(build_dir, 'the tool must be rebuilt from the current sources')


def fresh_or_rebuild(host, tool_build_dir, toolkit):
    """`host` if it can be PROVEN current, else rebuild it in the tree this script owns.

    Two independent reasons to rebuild, in order of strength:

    1. CONTENT: the existing corpus records source hashes that differ from the
       sources on disk.  The sources moved since the last generation, so a tool from
       that generation may not be used to record the new content -- regardless of
       mtime, which is what the archive/`rsync --times`/restored-backup cases defeat.
    2. MTIME: the tool is older than a source, so it cannot have been built from it.

    If neither fires, the tool is used as-is.  If a rebuild is needed but the tool is
    not one this script can rebuild -- i.e. it is not the artifact of the owned tree,
    which for a caller of this function means it came from `JSRF_NV2A_VSH_HLSL` or
    from a tree this script does not own -- it refuses with the rebuild hint instead
    of touching a path that is not its to rewrite.
    """
    changed, newer = tool_staleness(host, toolkit)
    if not changed and not newer:
        return host
    label = tool_label(host)
    tree = owning_build_dir(host, tool_build_dir)
    if tree is None:
        if changed:
            names = ', '.join(sorted(changed))
            die(f'the host tool {label} cannot be trusted: the corpus records different '
                f'content for {names}, so the sources moved since it was built, and it is '
                f'not one this script can rebuild (it is not the artifact of '
                f'{tool_build_dir}). It is not overwritten here; rebuild it with '
                f'`{REBUILD_HINT}` and re-run.')
        die(stale_message(host, label, newer, toolkit))
    if changed:
        why = ('the corpus records different source content for ' + ', '.join(sorted(changed))
               + ', so the host tool predates the current sources')
    else:
        why = stale_message(host, label, newer, toolkit)
    rebuilt = force_tool_rebuild(tree, why, toolkit)
    newer = newer_than_tool(rebuilt, toolkit)
    if newer:
        die(f'{tool_label(rebuilt)} is still older than '
            + ', '.join(str(p) for _, p in newer)
            + ' after rebuilding it; check the clock and the build tree '
              f'(`{REBUILD_HINT}`).')
    return rebuilt


def find_host_tool(tool_build_dir=None):
    """The host tool already present on this host, or None.  Builds nothing.

    This is the ONE definition of "which tool would this host use", and both this
    generator and `scripts/build-jsrf.py` read it: the guard re-derives the
    corpus's provenance from the same binary the generator would have run, so the
    order -- an explicit `JSRF_NV2A_VSH_HLSL` override, then the OWNED artifact
    under `build/vsh-tool` -- must not be spelled a second time.

    The owned artifact is the only build-tree copy considered.  The game build
    tree's copy is deliberately excluded: `just build` relinks it, this script runs
    before that tree is configured on a fresh checkout, and `--check`/the guard
    would then compare the recorded hash against a binary that has nothing to do
    with the corpus (see the module docstring).  An override names an existing file
    or nothing: it is never silently ignored and never replaced.
    """
    override = os.environ.get('JSRF_NV2A_VSH_HLSL')
    if override:
        p = Path(override)
        return p if p.is_file() else None
    return _tool_in(tool_build_dir or TOOL_BUILD_DIR)


def staleness_refusal(host, toolkit):
    """Why `host` may not be trusted with the current sources: `(trigger, detail)` or None.

    THE shared freshness rule.  `--check` (below) and the build guard
    (`scripts/build-jsrf.py`, which imports this) must reach the same verdict about a
    tool, so the rule is defined once here and only its wording differs at the two
    call sites.  The two triggers are the ones `tool_staleness` measures:

      * `('content', {name: (recorded, now)})` -- the existing corpus records different
        source content than the sources on disk.  The sources moved since the last
        generation, so no tool from that generation may record them.  Refused for
        EVERY tool, the owned artifact included.
      * `('override', [(name, path)])` -- the tool is an environment override
        (`JSRF_NV2A_VSH_HLSL`) older than a source.  Only an override is refused on
        mtime alone: it is the one binary whose freshness nobody can re-derive, because
        it must never be rebuilt or replaced.  An owned artifact that is merely older
        on mtime is NOT refused -- its recorded source CONTENT still matches, so the
        sources behind it are the same bytes and only their mtimes moved (a checkout,
        a copy, an `rsync --times`); a tool built from different sources, or merely
        relinked, has a different hash and is already refused by `tool.sha256`.
      * `None` -- nothing fires; the tool may be used.

    `tool_staleness` reads the translator sources, so a source vanishing between
    resolution and this call raises `prov.ProvenanceError`; callers turn that into
    their own refusal rather than a traceback.
    """
    changed, newer = tool_staleness(host, toolkit)
    if changed:
        return 'content', changed
    if newer and os.environ.get('JSRF_NV2A_VSH_HLSL'):
        return 'override', newer
    return None


def check_host_tool(tool_build_dir, toolkit):
    """`--check`'s tool: FOUND, never built, and not stale.  Returns `(path, label)`.

    `--check` answers a read-only question -- does the committed corpus still match
    what these inputs produce? -- so it must not be able to change the answer.
    Resolving through `locate_host_tool` could force-rebuild the owned artifact, and a
    rebuild moves the decisive `tool.sha256` of the very corpus the check was asked
    about (and can leave the tree refused).  So this path uses `find_host_tool` -- no
    configure, no build, no write of any kind -- and, when a tool exists, applies the
    same freshness rule as the build guard (`staleness_refusal`) with the same
    remedy: `just shaders` is what rebuilds a tool and records the sources again.
    """
    host = find_host_tool(tool_build_dir)
    if host is None:
        override = os.environ.get('JSRF_NV2A_VSH_HLSL')
        if override:
            state = (f'JSRF_NV2A_VSH_HLSL={override} is set but is not an existing file '
                     f'(a path named by the environment is never ignored or replaced)')
        else:
            state = (f'JSRF_NV2A_VSH_HLSL is not set and the tool this repository owns is not '
                     f'built under {tool_build_dir} (either CMake layout)')
        die(f'--check cannot compare the corpus: {state}, and --check never builds or '
            f'rebuilds anything. Run `just shaders`, which builds the tool this repository '
            f'owns and records its identity; without a tool there is nothing to compare the '
            f'corpus\'s recorded tool.sha256 against.')
    label = tool_label(host)
    refusal = staleness_refusal(host, toolkit)
    if refusal is not None and refusal[0] == 'content':
        detail = '\n'.join(
            f'  - {name}: the corpus records {recorded!r}, {toolkit} has {now!r}'
            for name, (recorded, now) in sorted(refusal[1].items()))
        die(f'--check cannot compare the corpus against {label}: the sources moved since it was '
            f'generated, and this is a check, so nothing is rebuilt.\n{detail}\n'
            f'Regenerate with `just shaders`, which rebuilds the tool this repository owns from '
            f'the current sources and records them; --check then compares a corpus its own tool '
            f'produced.')
    if refusal is not None:
        die(f'--check cannot compare the corpus against {label}: it is older than '
            + ', '.join(str(p) for _, p in refusal[1])
            + f', so it cannot be shown to have been built from the sources the corpus '
              f'records, and --check never rebuilds or replaces a path named by the environment. '
              f'Rebuild that executable, or unset JSRF_NV2A_VSH_HLSL so `just shaders` builds and '
              f'records the tool this repository owns, and re-run --check.')
    return host, label


def override_stale_message(label, changed, newer):
    """Why an explicit override is refused, with the remedies and the no-touch rule.

    `label` is the `JSRF_NV2A_VSH_HLSL=<path>` spelling, so the refusal names the
    exact environment value the caller set.

    The remedies differ by trigger, and the difference is not cosmetic.  The MTIME
    trigger is the half a rebuild clears.  The CONTENT trigger is not: it compares the
    sources the EXISTING corpus recorded with the sources on disk and never looks at
    this executable, so rebuilding the override cannot clear it -- what clears it is
    UNSETTING the variable, so that `just shaders` rebuilds the tool this script owns
    from the current sources and records them; the override may be set again
    afterwards.
    """
    reasons = []
    if changed:
        reasons.append('the corpus records different content for '
                       + ', '.join(sorted(changed)))
    if newer:
        reasons.append('it is older than ' + ', '.join(str(p) for _, p in newer))
    if changed:
        options = ['unset JSRF_NV2A_VSH_HLSL, which is what clears the content trigger: it '
                   'compares the sources the existing corpus RECORDED with the sources on disk '
                   'and never looks at this executable, so rebuilding it cannot clear it -- '
                   f'`just shaders` then rebuilds the tool this script owns in {TOOL_BUILD_DIR} '
                   'from the current sources and records them, after which the override may be '
                   'set again']
        if newer:
            options.append(f'rebuild it from the current sources (`{REBUILD_HINT}`), which '
                           'clears the mtime trigger but not the content one')
    else:
        options = [f'rebuild it from the current sources (`{REBUILD_HINT}`)',
                   'unset JSRF_NV2A_VSH_HLSL so this script builds and uses the tool it owns in '
                   f'{TOOL_BUILD_DIR}']
    return (f'{label} is STALE and must not generate this corpus: '
            + '; '.join(reasons)
            + '. A tool named by the environment is never rebuilt or replaced by this script '
              '-- it will not overwrite that path, and it cannot prove the binary came from the '
              'sources on disk. Remedy: '
            + '; or '.join(options) + '.')


def locate_host_tool(tool_build_dir=None, toolkit=None):
    """The nv2a_vsh_hlsl host tool: an existing build, else build it from the toolkit.

    Whichever way it is found -- an explicit override or the artifact of the owned
    build tree -- it must be shown current before it is used: by the corpus's
    recorded source CONTENT, and by mtime.  The game's CMake refuses to configure
    without the generated corpus, so on a fresh checkout the game's build tree does
    not exist yet and cannot produce this tool; the toolkit configures and builds
    standalone, so that is the bootstrap.

    An explicit override is the one case that cannot be rebuilt here, so it is
    REFUSED when either trigger fires rather than used with a warning: the recorded
    `tool.sha256` makes the corpus's identity decisive, and a stale binary recording
    hashes for bytes it never read is exactly the defect this mechanism exists to
    prevent.  `toolkit` is the resolved root whose sources the corpus records; when
    omitted it is resolved here (a cache/environment conflict is a refusal).
    """
    tool_build_dir = Path(tool_build_dir) if tool_build_dir is not None else TOOL_BUILD_DIR
    if toolkit is None:
        toolkit = resolve_toolkit()[0]
    override = os.environ.get('JSRF_NV2A_VSH_HLSL')
    if override and not Path(override).is_file():
        die(f'JSRF_NV2A_VSH_HLSL={override} does not exist. It is not ignored and not '
            f'replaced: unset it to let this script build and use the tool it owns in '
            f'{TOOL_BUILD_DIR}, or point it at the executable you built.')
    host = find_host_tool(tool_build_dir)
    if override:
        label = f'JSRF_NV2A_VSH_HLSL={override}'
        changed, newer = tool_staleness(host, toolkit)
        if changed or newer:
            die(override_stale_message(label, changed, newer))
        return host, label
    if host is None:
        return build_host_tool(tool_build_dir, toolkit)
    return fresh_or_rebuild(host, tool_build_dir, toolkit), tool_label(host)


def build_host_tool(build_dir, toolkit):
    """Configure and build the toolkit's nv2a_vsh_hlsl target; return its exe.

    The bootstrap: only reached when the owned tree has no artifact yet, and only
    ever builds into `build_dir`, which is this script's own tree.
    """
    toolkit = Path(toolkit)
    if not (toolkit / 'CMakeLists.txt').is_file():
        die(f'{toolkit} is not a toolkit checkout: the host tool is built from there '
            f'(set XBOXRECOMP_DIR).')

    exe = _tool_in(build_dir)
    if exe is None:
        print(f'gen-shaders: nv2a_vsh_hlsl.exe is not built yet; configuring the toolkit '
              f'in {build_dir} and building it', file=sys.stderr)
        build_dir.mkdir(parents=True, exist_ok=True)
        argv = ['cmake', '-S', str(toolkit), '-B', str(build_dir)]
        cp = subprocess.run(argv, capture_output=True, text=True)
        if cp.returncode != 0:
            die(f'`{" ".join(argv)}` failed (exit {cp.returncode}):\n'
                f'{(cp.stdout + cp.stderr).strip()[-1500:]}')
        # Re-resolve: the single-config layout has no Release subdirectory, so the
        # path that did not exist before the build is the one that does now.
        exe = rebuild_host_tool(build_dir, 'the toolkit build tree has no host tool yet')
    else:
        exe = fresh_or_rebuild(exe, build_dir, toolkit)
    return exe, tool_label(exe)


def locate_vulkan_tools():
    sdk = os.environ.get('VULKAN_SDK')
    if not sdk:
        die('VULKAN_SDK is not set: this corpus is compiled offline with the Vulkan SDK\'s '
            'dxc and spirv-val, so install the SDK and set VULKAN_SDK to its root.')
    bin_dir = vulkan_sdk_bin(sdk)
    for exe in ('dxc' + EXE_SUFFIX, 'spirv-val' + EXE_SUFFIX):
        if not (bin_dir / exe).is_file():
            die(f'{bin_dir / exe} is not there: VULKAN_SDK={sdk} does not carry the shader '
                f'tools this generator needs (dxc{EXE_SUFFIX}, spirv-val{EXE_SUFFIX}).')
    return bin_dir / ('dxc' + EXE_SUFFIX), bin_dir / ('spirv-val' + EXE_SUFFIX)


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


def provenance(host, dxc, stats, toolkit):
    """The provenance block body for the corpus generated NOW.

    `check_arg_vectors` has already refused if this script's command lines differ
    from the recorded ones, so the block describes exactly what ran. The counts
    come from `guard`'s measurement of this title, so a table whose own row count
    disagrees with its block is a refusal on the build side.  `toolkit` is the
    resolved root this run read its sources from, so the block records the sources
    the corpus was actually built from.
    """
    return prov.render_block(prov.expected(toolkit, host, dxc,
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


def assert_tool_current(host, label, toolkit):
    """Refuse to generate if the tool is older than a translator source.

    The property R1 must guarantee: a tool must never be used to record a source hash
    for bytes it did not read.  `locate_host_tool` has already rebuilt an owned tool
    or refused both an owned tool and a stale override on that rule, so this is the
    backstop -- it makes the invariant explicit at the point of use rather than
    leaving it implicit in the resolution path.  There is no exemption: a tool the
    script owns has just been rebuilt if it needed it, and an override that needed it
    was refused rather than used, so reaching here with a stale tool means the
    resolution path is broken.
    """
    newer = newer_than_tool(host, toolkit)
    if newer:
        die(stale_message(host, label, newer, toolkit)
            + ' Nothing will be generated with it: the corpus would record source '
              'hashes the tool never read.')


def build(game, scratch, host, dxc, spirv_val, toolkit):
    """Generate the corpus; returns ({filename: bytes}, SPIR-V byte total, measured stats)."""
    found, scanned = extract(game)
    attach_words(found, game)
    distinct = dedupe(found)
    stats = guard(found, distinct, scanned)
    block = provenance(host, dxc, stats, toolkit)
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
                    help='regenerate into a temporary directory and compare the result with '
                         'the committed corpus BYTE FOR BYTE -- no field is exempt, so a '
                         'difference (including the recorded tool identity, which is '
                         'decisive) means the corpus is stale; change nothing: the host tool '
                         'is found and never built, rebuilt or touched, and no file in this '
                         'tree is written')
    args = ap.parse_args(argv)

    # ONE toolkit for the whole run: the tree whose sources the corpus records and
    # whose CMake the build links.  A cache/environment conflict is a refusal here
    # rather than a preference, because both choices would be self-consistent.
    try:
        toolkit, toolkit_from = resolve_toolkit()
    except prov.ProvenanceError as error:
        die(str(error))
    # The owned tool tree is tied by its OWN cache to whichever toolkit configured
    # it, and its build system compiles THAT tree's sources.  If that is not the
    # resolved toolkit, nothing here may use or rebuild the tree -- not even a run
    # that would use an override, because the tree would stay rooted in the wrong
    # checkout (see `toolkit_binding_conflict`).  Checked before the tool is found,
    # used or rebuilt.
    conflict = toolkit_binding_conflict(toolkit, TOOL_BUILD_DIR, toolkit_from)
    if conflict:
        die(conflict)
    # The digest is defined in that toolkit's header; fail here rather than emit a
    # table whose identity fields the back end would reject.
    header_digest_constants(toolkit)
    # The block must describe the command lines this run uses, not other ones.
    check_arg_vectors()

    if args.check:
        # A check must not be able to change what it is checking: the tool is FOUND
        # (never built, so `tool.sha256` cannot move under it) and refused when it may
        # not stand for the sources the corpus records.
        host, host_from = check_host_tool(TOOL_BUILD_DIR, toolkit)
    else:
        host, host_from = locate_host_tool(TOOL_BUILD_DIR, toolkit)
        # The backstop: the corpus must never record a source hash for bytes the tool
        # that ran never read.  `locate_host_tool` has already rebuilt an owned tool or
        # refused a stale override, so this can only fire if that path is broken.
        assert_tool_current(host, host_from, toolkit)
    dxc, spirv_val = locate_vulkan_tools()
    host_sha = prov.sha256_file(host)

    if args.check:
        with tempfile.TemporaryDirectory(prefix='gen-shaders-check-') as tmp:
            tmp_path = Path(tmp)
            files, total, stats = build(args.game, tmp_path, host, dxc, spirv_val, toolkit)
            bad = 0
            for name, data in files.items():
                target = OUT_DIR / name
                if not target.is_file():
                    print(f'check: {name} is missing from {OUT_DIR}', file=sys.stderr)
                    bad += 1
                    continue
                have = target.read_bytes()
                # BYTE-EXACT: the block now records a DECISIVE tool identity, so two
                # different-but-equivalent host tool binaries describe two different
                # corpora -- `just shaders` is what makes them agree, and any
                # difference here (tool identity included) is a stale corpus.  The
                # committed bytes are what gets compiled, so they are the claim.
                if have != data:
                    print(f'check: {name} differs (committed {len(have)} bytes sha256 '
                          f'{hashlib.sha256(have).hexdigest()}, regenerated {len(data)} bytes '
                          f'sha256 {hashlib.sha256(data).hexdigest()}); the comparison is '
                          f'byte-exact and the recorded tool identity is decisive',
                          file=sys.stderr)
                    bad += 1
                else:
                    print(f'check: {name} identical, byte for byte ({len(data)} bytes sha256 '
                          f'{hashlib.sha256(data).hexdigest()})')
            if bad:
                print(f'check: FAILED ({bad} file(s) differ); regenerate with `just shaders`',
                      file=sys.stderr)
                return 1
            print(f'check: OK, toolkit {toolkit} ({toolkit_from}), {stats["programs"]} programs, '
                  f'{stats["distinct"]} distinct, {stats["hashes"]} hashes, '
                  f'{stats["collisions"]} collisions, {total} SPIR-V bytes, host tool '
                  f'{host_from} sha256 {host_sha}')
            return 0

    scratch = OUT_DIR / '_scratch'
    if scratch.exists():
        shutil.rmtree(scratch)
    scratch.mkdir(parents=True)
    try:
        files, total, stats = build(args.game, scratch, host, dxc, spirv_val, toolkit)
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
          f'{total} SPIR-V bytes, toolkit {toolkit} ({toolkit_from}), host tool {host_from} '
          f'sha256 {host_sha}')
    print(f'shaders: {C_NAME} {len(files[C_NAME])} bytes sha256 {c_sha}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
