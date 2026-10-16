"""The provenance schema for the generated V5 vertex-program corpus (plan step R1).

The runtime identity check proves the guest *words* a batch uploads, not which
translator or compiler produced their SPIR-V.  So a change to the translator
(`nv2a_vsh_hlsl.c`), its header, or its CLI, or to dxc's version or flags, can
leave the game linking SPIR-V that no current source could produce -- wrong
pixels, with no signal at run time.  This module is the single definition of
what the generator records and what the build guard re-derives, so the two can
never disagree about the schema.

WHAT IS RECORDED, AND WHY EVERY FIELD DECIDES
---------------------------------------------
Every field in the block is an input that determines the emitted SPIR-V, and every
one of them is DECIDING: a corpus that omits a field, or records a value that
differs from the inputs in this tree, is refused.  There are no advisory fields.

`tool.sha256` is the SHA-256 of the executable that actually performed the
translation, and that executable is the ONE this repository owns: the
`nv2a_vsh_hlsl.exe` under `gen_shaders.TOOL_BUILD_DIR` (`<repo>/build/vsh-tool`), in
either CMake layout.  The game build tree's copy is deliberately not a candidate --
`just build` relinks it (MSVC embeds a PDB signature, so its hash moves without an
input moving) and this guard runs before that tree is even configured.  The owned
artifact's hash moves on EVERY rebuild of it: a rebuild from moved sources, and also
a forced rebuild from identical ones -- the link is not reproducible on this
toolchain (see REPRODUCIBILITY below), so a forced rebuild that made the compiler run
again moves the hash even when nothing in the sources changed.  A hash comparison
therefore cannot be read as "the same bytes, so the same sources"; it compares the
actual generating binary with the actual binary in use, and any move means the corpus
must be regenerated.  When no tool has been built at all the field cannot be
compared, and that is a refusal here rather than a
pass: a caller that cannot say which executable produced the corpus it is guarding
has not verified anything.

A moved binary -- a rebuilt tool, a replaced executable, a different build input --
therefore conservatively requires `just shaders`: regeneration is the offline step
that re-records the identity, and this guard never guesses that two different
executables are equivalent.

FRESHNESS: GENERATION ENFORCES IT, THE GUARD RE-CHECKS IT
---------------------------------------------------------
The hash comparison above is of *identity*, not of *newness*: two different hashes
could be a tool rebuilt from the same sources or a tool built from older ones.
Freshness -- that the tool was built from the sources the corpus records -- is
therefore enforced at GENERATION time, in `gen-shaders.py`, by two independent
triggers:

  * CONTENT.  The EXISTING corpus's recorded `source.*` hashes against the sources
    on disk.  A difference means the sources moved since the last generation, so no
    tool from that generation may be trusted, whatever its mtime.
  * MTIME.  A tool older than a source cannot have been built from it.

The build guard re-checks both: content for every corpus, plus mtime whenever the
tool is an environment override (`JSRF_NV2A_VSH_HLSL`) -- the one binary whose
freshness the guard cannot re-derive, because it must never rebuild or replace a
path that came from the environment.

REPRODUCIBILITY
---------------
The emitted block contains no absolute path, no username and no timestamp, so a
corpus regenerated from the same sources by the same tool is byte-identical.
The executable is NOT reproducible on this toolchain: measured on this host, three
clean rebuilds of byte-identical sources produced three different
`nv2a_vsh_hlsl.exe` hashes, because each PE carries its own link-time
`TimeDateStamp` and the game-tree build also embeds an absolute PDB path.  That is
precisely why generation pins the tool to the owned artifact rather than to whatever
binary happens to be around, and why `--check` compares bytes exactly: a different
tool identity IS a different corpus, and `just shaders` is what makes them agree.

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
# Schema 2 makes `tool.sha256` a DECIDING field (schema 1 recorded it as advisory
# data), so a corpus written by schema 1 is refused and must be regenerated with
# `just shaders` -- the recorded tool identity of a schema-1 block was never
# checked and therefore proves nothing about the binary that ran.
SCHEMA = 2

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

    `tool_path` is REQUIRED and must be an existing file: `tool.sha256` is a
    deciding field, so a caller that cannot name the executable it expects cannot
    produce a full expectation at all -- and must be refused here rather than
    silently compare one field less.  (The guard resolves the tool through
    `gen_shaders.find_host_tool` first and refuses when there is none; this is the
    second, independent door onto the same rule.)
    """
    if tool_path is None:
        raise ProvenanceError(
            'no host tool was named, so tool.sha256 -- a deciding field -- cannot be '
            'part of the expectation.  The executable that generated the corpus must be '
            f'known before it can be checked; {REMEDY}.')
    if not Path(tool_path).is_file():
        raise ProvenanceError(
            f'the host tool {tool_path} named for this expectation does not exist, so '
            f'tool.sha256 cannot be derived from it; {REMEDY}.')
    values = {
        'schema': str(SCHEMA),
        'dxc.version': dxc_version(dxc),
        'dxc.args': ' '.join(DXC_ARGS),
        'tool.args': ' '.join(TOOL_ARGS),
        'tool.sha256': sha256_file(tool_path),
    }
    for name, digest in source_hashes(toolkit).items():
        values[f'source.{name}'] = digest
    if programs is not None:
        values['programs'] = str(programs)
    if distinct is not None:
        values['distinct'] = str(distinct)
    return values


def compare(recorded, want):
    """`(ok, problems)`: recorded fields against what the inputs say now.

    EVERY key in `want` decides, `tool.sha256` included.  Both directions are
    checked: a field the block lacks is as much a refusal as a field that
    disagrees -- that is the "missing provenance must not become a pass" rule --
    and an unknown field means the block was written by a schema this guard does
    not implement (or by a generator that recorded something it no longer knows
    how to interpret), which must not be waved through either.
    """
    problems = []
    for key in want:
        if key not in recorded:
            problems.append(f'{key} is not recorded in the corpus provenance')
        elif recorded[key] != want[key]:
            problems.append(f'{key} differs: the corpus records {recorded[key]!r}, '
                            f'the current inputs give {want[key]!r}')
    for key in recorded:
        if key not in want:
            problems.append(f'the corpus records {key!r}, which this guard does not know')
    return (not problems), problems


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


def check_corpus(corpus_dir, want, files=CORPUS_FILES):
    """The production corpus-provenance check: `(ok, problems)`.

    This is the ONE sequence the build guard runs and the ONE the fixtures call, so
    a fixture cannot pass against a private copy of the logic while the shipped
    guard drifts.  It covers, in order:

      * each corpus file present and readable;
      * a well-formed block in each (`parse_block`'s own problems);
      * exactly one block per file (none is "the inputs are unknown", two is
        "which one is current?");
      * every deciding field present and equal (`compare`), INCLUDING
        `tool.sha256`: the executable that produced the table is one of the inputs
        that determine its bytes, so a corpus that does not record it (or records
        a different one) is refused;
      * the two files recording the SAME block, so one cannot be regenerated
        without the other;
      * the block's `distinct` agreeing with the table's declared
        `nv2a_vsh_program_count`.

    `want` is the caller's `expected(...)`; this function deliberately does not
    import the generator (the generator imports this module), so tool and dxc
    resolution stay the caller's business.
    """
    problems = []
    recorded = {}
    texts = {}

    # An empty or partial `want` would compare NOTHING and report success, which is
    # the "missing provenance became a pass" failure in its worst form: a guard that
    # vacuously accepts everything.  `check_corpus` therefore refuses a caller that
    # cannot say what it expects -- and `tool.sha256` is on that list, so a caller
    # that could not find a host tool cannot silently drop the tool expectation and
    # pass a corpus generated by an entirely different executable.
    required = [k for k in (['schema'] + [f'source.{n}' for n in SOURCE_NAMES]
                            + ['tool.sha256', 'tool.args', 'dxc.version', 'dxc.args',
                               'programs', 'distinct'])
                if k not in want]
    if required:
        return False, ['the caller supplied no expectation for '
                       + ', '.join(sorted(required))
                       + ', so this check could not compare them']
    if len(files) != 2:
        # The two-file agreement step below unpacks exactly two names; a caller
        # passing a different arity must get a refusal, not a ValueError.
        return False, [f'{len(files)} corpus file(s) were named, but this check needs '
                       f'exactly two (the header and the source that defines the count)']

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
        _, field_problems = compare(blocks[0], want)
        problems += [f'{path}: {p}' for p in field_problems]

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
    return (not problems), problems


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

