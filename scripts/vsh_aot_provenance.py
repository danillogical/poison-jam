"""The provenance schema for the generated V5 vertex-program corpus (plan step R1).

The runtime identity check proves the guest *words* a batch uploads, not which
translator or compiler produced their SPIR-V.  So a change to the translator
(`nv2a_vsh_hlsl.c`), its header, or its CLI, or to dxc's version or flags, can
leave the game linking SPIR-V that no current source could produce -- wrong
pixels, with no signal at run time.  This module is the single definition of
what the generator records and what the build guard re-derives, so the two can
never disagree about the schema.

THE THREAT AND WHY THE REFUSAL KEYS ON SOURCES
----------------------------------------------
The refusal must fire when an input that determines the SPIR-V has moved.  The
three translator sources and dxc's version/flags all satisfy that.  The host
tool's own executable hash does NOT: measured on this host, three clean
rebuilds of byte-identical sources produced three different
`nv2a_vsh_hlsl.exe` hashes, because MSVC embeds a non-reproducible PDB
signature.  The tool hash is therefore RECORDED for traceability and diagnosis
(it answers "which binary produced this?") but is deliberately NOT a refusal
condition -- a guard that demanded a stable tool hash would fail every clean
rebuild, and a guard that can only pass by accident is worse than none.

Freshness of the tool (that it was built from the sources recorded here) is
enforced at GENERATION time, in `gen-shaders.py`, by comparing the executable's
mtime against all three sources.  That split is deliberate: generation is the
only moment at which a stale tool can actually corrupt the corpus.

REPRODUCIBILITY
---------------
The emitted block contains no absolute path, no username and no timestamp, so the
DECIDING fields are identical wherever the same inputs are regenerated.  The
recorded `tool.sha256` is NOT reproducible, and that is the reason it is advisory:
measured on this host, three clean rebuilds of byte-identical sources produced three
different `nv2a_vsh_hlsl.exe` hashes.  Inspection of the PE headers shows why -- each
executable carries its own link-time `TimeDateStamp`, and the game-tree build also
embeds an absolute PDB path, so the hash depends on the checkout location as well as
the clock.  Two runs with equivalent tool binaries therefore produce byte-different
corpus files describing the SAME corpus; `strip_advisory` exists so a comparison can
ignore exactly that field.
Arguments are recorded with their file operands replaced by placeholders
(`{bin}`, `{mask}`, `{out}`); only the flags that affect the output are
recorded.  `dxc --version` is normalized to a single whitespace-collapsed line.
"""

from __future__ import annotations

import hashlib
import os
import re
import subprocess
import sys
from pathlib import Path

# Bumped when the block's field set changes in a way a reader must notice.  A
# mismatch is a refusal, not a warning: a guard that silently accepts a schema
# it does not understand is the "missing provenance became a pass" failure.
SCHEMA = 1

BEGIN_MARK = 'JSRF-VSH-AOT-PROVENANCE-BEGIN'
END_MARK = 'JSRF-VSH-AOT-PROVENANCE-END'

# The three toolkit sources that determine the translated HLSL.  `nv2a_vsh_hlsl.h`
# owns the entry point and the digest constants, `nv2a_vsh_hlsl_tool.c` owns the
# CLI and the `--vk` rewriting.
SOURCE_NAMES = ('nv2a_vsh_hlsl.c', 'nv2a_vsh_hlsl.h', 'nv2a_vsh_hlsl_tool.c')

# The dxc flags that determine the emitted SPIR-V.  Recorded as data, compared as
# data; the generator asserts its own command line equals this vector, so the
# block cannot describe a compile that did not happen.
DXC_ARGS = ('-spirv', '-T', 'vs_6_0', '-E', 'vs_prog', '-fvk-invert-y',
            '-fspv-target-env=vulkan1.1')

# The host tool's flags, with file operands as placeholders: the output depends on
# `--vk` (it emits the bindings and locations), not on which scratch path was used.
TOOL_ARGS = ('--bin', '{bin}', '--mask', '{mask}', '--vk', '--out', '{out}')

REMEDY = 'run `just shaders`'


def sha256_file(path):
    """The SHA-256 of a file's bytes."""
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def source_paths(toolkit):
    """The three translator sources, in SOURCE_NAMES order."""
    video = Path(toolkit) / 'src' / 'video'
    return {name: video / name for name in SOURCE_NAMES}


def source_hashes(toolkit):
    """`{name: sha256}` for the three translator sources.

    A missing source is an error, never an empty string: an absent input must not
    hash to something a recorded block could accidentally match.
    """
    out = {}
    for name, path in source_paths(toolkit).items():
        if not path.is_file():
            raise ProvenanceError(f'the translator source {path} is missing')
        out[name] = sha256_file(path)
    return out


class ProvenanceError(Exception):
    """A provenance input could not be read, or a block could not be parsed."""


def normalize_version(text):
    """`dxc --version` output as one whitespace-collapsed line.

    dxc writes a single line today, but collapsing is what keeps the recorded
    value stable if a future SDK adds a trailing newline or wraps the text.
    """
    return ' '.join((text or '').split())


def dxc_version(dxc):
    """The installed dxc's version string, normalized.  Raises if dxc cannot run."""
    cp = subprocess.run([str(dxc), '--version'], capture_output=True, text=True)
    if cp.returncode != 0:
        raise ProvenanceError(f'{dxc} --version exited {cp.returncode}: '
                              f'{(cp.stderr or cp.stdout).strip()[:200]}')
    version = normalize_version(cp.stdout or cp.stderr)
    if not version:
        raise ProvenanceError(f'{dxc} --version produced no output')
    return version


def expected(toolkit, tool_path, dxc, programs=None, distinct=None):
    """The provenance a corpus generated NOW must carry.

    `programs`/`distinct` are the corpus's own measured counts; they are included
    so a block whose counts disagree with the table it sits in is a refusal.

    `tool_path` may be None, which OMITS `tool.sha256`.  That case exists because
    the tool hash is advisory: a corpus can be perfectly current while no host tool
    is built (the corpus lives in `src/recomp/gen/shaders` and survives deleting
    `build/`), and a guard that refused then would reject a valid tree.  The
    generator always passes a real path, so the recorded block always carries the
    field.
    """
    values = {
        'schema': str(SCHEMA),
        'dxc.version': dxc_version(dxc),
        'dxc.args': ' '.join(DXC_ARGS),
        'tool.args': ' '.join(TOOL_ARGS),
    }
    if tool_path is not None:
        values['tool.sha256'] = sha256_file(tool_path)
    for name, digest in source_hashes(toolkit).items():
        values[f'source.{name}'] = digest
    if programs is not None:
        values['programs'] = str(programs)
    if distinct is not None:
        values['distinct'] = str(distinct)
    return values


# Fields that are RECORDED but never REFUSED on.  `tool.sha256` is here because
# the executable is not reproducible on this toolchain (see the module docstring):
# a guard that compared it would reject a corpus that a `just shaders` run had
# just produced, because the subsequent toolkit relink changes the hash without
# changing a single input that determines the SPIR-V.  The value stays in the
# block so a reader can ask "which binary produced this?", and `compare` reports a
# difference as advice -- it never fails on it.
ADVISORY_FIELDS = ('tool.sha256',)


def compare(recorded, want):
    """`(ok, problems, advisories)`: recorded fields against what the inputs say now.

    Both directions are checked over the DECIDING fields.  A field the block lacks
    is as much a refusal as a field that disagrees -- that is the "missing
    provenance must not become a pass" rule -- and an unknown field means the block
    was written by a schema this guard does not implement.

    ADVISORY_FIELDS are compared for the report only; a difference there is an
    `advisory`, never a problem, so the tool hash can move (a relink) without
    refusing a current corpus.
    """
    problems = []
    advisories = []
    for key in want:
        if key in ADVISORY_FIELDS:
            if key in recorded and recorded[key] != want[key]:
                advisories.append(f'{key} differs (recorded {recorded[key]!r}, '
                                  f'now {want[key]!r}) -- advisory only, not a refusal')
            continue
        if key not in recorded:
            problems.append(f'{key} is not recorded in the corpus provenance')
        elif recorded[key] != want[key]:
            problems.append(f'{key} differs: the corpus records {recorded[key]!r}, '
                            f'the current inputs give {want[key]!r}')
    for key in recorded:
        if key not in want and key not in ADVISORY_FIELDS:
            problems.append(f'the corpus records {key!r}, which this guard does not know')
    return (not problems), problems, advisories


# The corpus files a generated table is spread across, in the order the guard
# reads them.  Both must carry the SAME block, so the two-file agreement check
# below catches one file regenerated without the other.
CORPUS_FILES = ('nv2a_vsh_programs.h', 'nv2a_vsh_programs.c')

# `nv2a_vsh_program_count` is the table's own independent declaration of how many
# rows it holds.  The provenance block's `distinct` must agree with it, so a block
# copied from a different corpus cannot pass by agreeing with itself.  The DEFINITION
# (`= <n>`) lives in the `.c`; the `.h` only declares `extern`, so searching the
# header for a value would find nothing and the check would be vacuous.
COUNT_RE = re.compile(r'nv2a_vsh_program_count\s*=\s*(\d+)\s*;')
COUNT_FILE = 'nv2a_vsh_programs.c'


def strip_advisory(text):
    """`text` with every ADVISORY_FIELDS line removed from its provenance block(s).

    `gen-shaders.py --check` compares regenerated bytes against the committed corpus.
    The tool hash is advisory, so two runs using different-but-equivalent host tool
    binaries produce byte-different files that describe the SAME corpus -- and a
    byte comparison would call that a stale corpus.  Blanking only the advisory
    lines makes `--check` ask the question that matters ("is this corpus current?")
    without making the advisory field decisive, which is exactly the mistake the
    build guard avoids.
    """
    out = []
    for line in text.splitlines(keepends=True):
        bare = re.sub(r'^\s*\*?\s*', '', line).strip()
        key = bare.split(' = ', 1)[0].strip() if ' = ' in bare else None
        if key in ADVISORY_FIELDS:
            out.append(re.sub(r'(?<=\s)\S.*$', '<advisory>\n', line) if ' = ' in line else line)
        else:
            out.append(line)
    return ''.join(out)


def check_corpus(corpus_dir, want, files=CORPUS_FILES):
    """The production corpus-provenance check: `(ok, problems, advisories)`.

    This is the ONE sequence the build guard runs and the ONE the fixtures call, so
    a fixture cannot pass against a private copy of the logic while the shipped
    guard drifts.  It covers, in order:

      * each corpus file present and readable;
      * a well-formed block in each (`parse_block`'s own problems);
      * exactly one block per file (none is "the inputs are unknown", two is
        "which one is current?");
      * every deciding field present and equal (`compare`);
      * the two files recording the SAME block, so one cannot be regenerated
        without the other;
      * the block's `distinct` agreeing with the table's declared
        `nv2a_vsh_program_count`.

    `want` is the caller's `expected(...)`; this function deliberately does not
    import the generator (the generator imports this module), so tool and dxc
    resolution stay the caller's business.
    """
    problems = []
    advisories = []
    recorded = {}
    texts = {}

    # An empty or partial `want` would compare NOTHING and report success, which is
    # the "missing provenance became a pass" failure in its worst form: a guard that
    # vacuously accepts everything.  `check_corpus` therefore refuses a caller that
    # cannot say what it expects.  (Not reachable from `build-jsrf.py`, which always
    # passes a full `expected(...)`; this closes the trap for any future caller.)
    required = [k for k in (['schema'] + [f'source.{n}' for n in SOURCE_NAMES]
                            + ['tool.args', 'dxc.version', 'dxc.args', 'programs', 'distinct'])
                if k not in want]
    if required:
        return False, ['the caller supplied no expectation for '
                       + ', '.join(sorted(required))
                       + ', so this check could not compare them'], []
    if len(files) != 2:
        # The two-file agreement step below unpacks exactly two names; a caller
        # passing a different arity must get a refusal, not a ValueError.
        return False, [f'{len(files)} corpus file(s) were named, but this check needs '
                       f'exactly two (the header and the source that defines the count)'], []

    for name in files:
        path = Path(corpus_dir) / name
        if not path.is_file():
            problems.append(f'{path} is missing, so the inputs that produced this table '
                            f'are unknown')
            continue
        text = texts[name] = path.read_text(encoding='utf-8', errors='replace')
        blocks, block_problems = parse_block(text)
        for problem in block_problems:
            problems.append(f'{path}: malformed provenance block: {problem}')
        if not blocks:
            problems.append(f'{path} records no provenance block '
                            f'({BEGIN_MARK} .. {END_MARK}), so the inputs that produced '
                            f'this table are unknown')
            continue
        if len(blocks) > 1:
            problems.append(f'{path} records {len(blocks)} provenance blocks, expected one')
            continue
        recorded[name] = blocks[0]
        _, field_problems, field_advisories = compare(blocks[0], want)
        problems += [f'{path}: {p}' for p in field_problems]
        advisories += [f'{path}: {a}' for a in field_advisories]

    if len(recorded) == len(files):
        first, second = (recorded[name] for name in files)
        if first != second:
            differing = sorted(k for k in set(first) | set(second)
                               if first.get(k) != second.get(k))
            problems.append('the two corpus files record different provenance for '
                            + ', '.join(differing)
                            + f' ({files[0]} vs {files[1]}), so one of them is stale')
        declared = COUNT_RE.search(texts.get(COUNT_FILE, ''))
        if not declared:
            problems.append(f'{Path(corpus_dir) / COUNT_FILE} does not define '
                            f'nv2a_vsh_program_count')
        elif recorded[files[0]].get('distinct') != declared.group(1):
            problems.append(f'the block records distinct = '
                            f'{recorded[files[0]].get("distinct")!r} but the table it sits '
                            f'in declares nv2a_vsh_program_count = {declared.group(1)}')
    return (not problems), problems, advisories


def render_block(values, indent=' * '):
    """The provenance block as a C comment body, key order fixed for reproducibility."""
    order = (['schema'] + [f'source.{n}' for n in SOURCE_NAMES]
             + ['tool.sha256', 'tool.args', 'dxc.version', 'dxc.args',
                'programs', 'distinct'])
    missing = [k for k in order if k not in values]
    if missing:
        raise ProvenanceError('provenance is missing ' + ', '.join(missing))
    extra = [k for k in values if k not in order]
    if extra:
        raise ProvenanceError('provenance has unknown field(s): ' + ', '.join(sorted(extra)))
    lines = [f'{indent}{BEGIN_MARK}']
    for key in order:
        lines.append(f'{indent}{key} = {values[key]}')
    lines.append(f'{indent}{END_MARK}')
    return '\n'.join(lines)


def parse_block(text):
    """Every provenance block in `text` as `(values, problems)`.

    Tolerant about surrounding whitespace and comment markers, strict about
    structure: a begin without an end, a duplicate key or a line with no `=` is a
    problem rather than a silently dropped field.
    """
    problems = []
    blocks = []
    current = None
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        # Strip the C comment decoration this file is emitted inside.
        line = re.sub(r'^/\*\s?', '', line)
        line = re.sub(r'^\*\s?', '', line)
        line = re.sub(r'\s?\*/$', '', line).strip()
        if line == BEGIN_MARK:
            if current is not None:
                problems.append(f'line {lineno}: a second {BEGIN_MARK} before {END_MARK}')
            current = ({}, lineno)
            continue
        if line == END_MARK:
            if current is None:
                problems.append(f'line {lineno}: {END_MARK} without {BEGIN_MARK}')
            else:
                blocks.append(current[0])
                current = None
            continue
        if current is None or not line:
            continue
        if ' = ' not in line:
            problems.append(f'line {lineno}: {line!r} is not "key = value"')
            continue
        key, _, value = line.partition(' = ')
        key, value = key.strip(), value.strip()
        if key in current[0]:
            problems.append(f'line {lineno}: duplicate key {key!r}')
        current[0][key] = value
    if current is not None:
        problems.append(f'{BEGIN_MARK} at line {current[1]} has no {END_MARK}')
    return blocks, problems

