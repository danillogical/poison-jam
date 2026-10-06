"""P0.7: generation provenance, protected-marker preservation, and candidate mode.

The problem this exists for is measured and specific.  `scripts/recover-functions.py`
runs as part of the ordinary build and **rewrites generated output**, which can
erase the ABI instrumentation the current evidence depends on.  Nothing recorded
*which inputs* produced the committed generated tree, so a regeneration could not
be distinguished from a regression, and an earlier session measured a fix as
"zero effect" because it had regenerated the wrong half of the pipeline.

Three obligations, matching P0.7-AC1 to AC3:

* **AC1 — provenance manifest.** Identify both repository revisions and dirty
  state, the XBE and analysis-input hashes, the exact command and arguments,
  generator versions, generation mode, the generated outputs, and the protected
  ABI-instrumentation markers.  Missing or unknown inputs are `UNKNOWN`, never a
  silent pass.
* **AC2 — check-only mode.** A no-op generation writes only to an isolated
  candidate directory, and the production tree is byte-identical before and after.
* **AC3 — negative controls.** A changed input hash, a changed recipe, or a
  removed protected marker is rejected; a missing manifest cannot pass.
"""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
MANIFEST_SCHEMA_VERSION = 1
CHECKER_VERSION = 'jsrf-generation-provenance/1'

MANIFEST_PATH = ROOT / 'docs' / 'reviews' / 'p0-7-generation-provenance.json'
BASELINE_PATH = ROOT / 'docs' / 'reviews' / 'p0-full-generated-baseline.json'
CANDIDATE_DIR = ROOT / 'build' / 'generation-candidate'

# Manifest keys that record *why* the bytes moved, not what they are.  They are
# written by hand (and appended by scripts/recover-functions.py) and cannot be
# re-measured, so `write_manifest()` carries them over instead of rebuilding
# them.  See the docstring there for the measured loss this prevents.
#
# This is the *candidate* set.  `write_manifest()` preserves only the keys the
# existing file actually has, because the two records use different names: the
# provenance manifest carries `amendments`/`regenerations` while the preservation
# baseline carries `updates`.  A key a file does not have is not added, so the
# writer never invents an empty history section.
PRESERVED_KEYS = ('amendments', 'regenerations', 'updates')

# The analysis inputs the translation actually reads.  `AGENTS.md` records the
# trap: a "clean" regeneration must clear the disassembly JSON too, because it is
# an *input*, not a cache -- clearing only `.disasm_cache.json` leaves the
# translation reading a stale partial database.
ANALYSIS_INPUTS = (
    'tools/disasm/output/functions.json',
    'tools/disasm/output/functions.recovered.json',
    'config/manual-functions.json',
    'config/recovered-functions.json',
    'config/boundary-fixes.json',
    'config/recovery-unresolved.json',
)

# The full pass does not read the game's analysis output. Run from the game root
# with the toolkit on PYTHONPATH, `tools.recomp` resolves its default analysis
# directories inside the toolkit, and those gitignored files are what the chunks
# were generated from (measured 2026-09-28: every generated function is in the
# toolkit's 8768-entry functions.json; 713 are absent from the game's 8437-entry
# one, which relift-selected.py and recover-functions.py use). Recorded as
# `toolkit:`-prefixed inputs so a changed toolkit analysis file is caught.
TOOLKIT_ANALYSIS_INPUTS = (
    'tools/disasm/output/functions.json',
    'tools/disasm/output/labels.json',
    'tools/func_id/output/identified_functions.json',
    'tools/abi_analysis/output/abi_functions.json',
)
TOOLKIT_PREFIX = 'toolkit:'


def _input_path(name: str) -> Path:
    if name.startswith(TOOLKIT_PREFIX):
        return TOOLKIT / name[len(TOOLKIT_PREFIX):]
    return ROOT / name

# The single correct full-translation invocation.  Recorded because nothing in
# `scripts/` runs it, so it is easy to invoke by hand and get subtly wrong --
# which is exactly what happened when omitting the two flags left two functions
# double-defined.
FULL_TRANSLATION_COMMAND = (
    'python -m tools.recomp game/default.xbe --all --split 1000 '
    '--gen-dir src/recomp/gen --game-name "Jet Set Radio Future" '
    '--manual-functions config/manual-functions.json '
    '--exclude-manual src/recomp_manual.c '
    '--trace-functions config/trace-functions.json --backedge-yield'
)

# ABI-instrumentation markers that a regeneration must not silently remove.  Each
# is a *contract*, not a comment: the ABI checks abort when they fail.
PROTECTED_MARKERS = (
    'RECOMP_ABI_CALL',
    'JSRF_ABI_CONTINUE',
    'g_seh_ebp',
    'RECOMP_GENERATED_CODE',
)

# Generated outputs the translation owns.  `recover-functions.py` must never write
# these -- it owns only `recomp_stubs_recovery.c`.
TRANSLATION_OWNED = (
    'src/recomp/gen/recomp_funcs.h',
    'src/recomp/gen/recomp_dispatch.c',
    'src/recomp/gen/recomp_stubs_unresolved.c',
    'src/recomp/gen/recomp_types.h',
)
RECOVERY_OWNED = (
    'src/recomp/recovered/recovered.c',
    'src/recomp/gen/recomp_stubs_recovery.c',
)


class ProvenanceError(ValueError):
    """The manifest or an input cannot be established."""


def _digest(path: Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b''):
            digest.update(chunk)
    return digest.hexdigest()


def _git(repo: Path, *args: str) -> str | None:
    try:
        proc = subprocess.run(['git', '-C', str(repo), *args],
                              capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if proc.returncode != 0:
        return None
    return proc.stdout


def repository_state(repo: Path) -> dict[str, Any]:
    """Revision plus dirty-tree identity, or an explicit unknown."""
    revision = _git(repo, 'rev-parse', 'HEAD')
    status = _git(repo, 'status', '--porcelain')
    if revision is None:
        return {'revision': None, 'dirty': None, 'state': 'UNKNOWN',
                'reason': f'git could not read {repo}'}
    return {
        'revision': revision.strip(),
        'dirty': bool(status and status.strip()),
        'status_sha256': hashlib.sha256((status or '').encode()).hexdigest(),
        'state': 'MEASURED',
    }


def input_hashes() -> dict[str, Any]:
    """Hash every analysis input; a missing one is recorded, not skipped."""
    result: dict[str, Any] = {}
    for name in ANALYSIS_INPUTS + tuple(TOOLKIT_PREFIX + n for n in TOOLKIT_ANALYSIS_INPUTS):
        path = _input_path(name)
        digest = _digest(path)
        result[name] = {'sha256': digest,
                        'present': digest is not None,
                        'bytes': path.stat().st_size if path.is_file() else None}
    result['game/default.xbe'] = {
        'sha256': _digest(ROOT / 'game' / 'default.xbe'),
        'present': (ROOT / 'game' / 'default.xbe').is_file(),
        'bytes': (ROOT / 'game' / 'default.xbe').stat().st_size
        if (ROOT / 'game' / 'default.xbe').is_file() else None,
    }
    return result


def generator_versions() -> dict[str, Any]:
    """Record the generator identities a regeneration would use."""
    versions: dict[str, Any] = {}
    for name in ('tools/recomp/lifter.py', 'tools/recomp/translator.py',
                 'scripts/recover-functions.py', 'scripts/relift-selected.py'):
        digest = _digest(TOOLKIT / name)
        if digest is None:
            digest = _digest(ROOT / name)
        versions[name] = digest
    versions['toolkit'] = repository_state(TOOLKIT)
    versions['project'] = repository_state(ROOT)
    return versions


def protected_marker_counts(paths: tuple[str, ...] | None = None) -> dict[str, Any]:
    """Count each protected marker across the generated tree.

    A count of zero is a finding, not an absence of evidence: the marker is a
    contract, and its disappearance means the instrumentation was erased.
    """
    paths = paths or (TRANSLATION_OWNED + RECOVERY_OWNED)
    counts: dict[str, Any] = {}
    for name in paths:
        path = ROOT / name
        if not path.is_file():
            counts[name] = {'present': False, 'markers': {}}
            continue
        text = path.read_text(encoding='utf-8', errors='replace')
        counts[name] = {
            'present': True,
            'markers': {marker: len(re.findall(re.escape(marker), text))
                        for marker in PROTECTED_MARKERS},
        }
    return counts


def total_marker_counts(counts: dict[str, Any]) -> dict[str, int]:
    totals = {marker: 0 for marker in PROTECTED_MARKERS}
    for entry in counts.values():
        for marker, value in (entry.get('markers') or {}).items():
            totals[marker] = totals.get(marker, 0) + value
    return totals


def generated_tree_hashes() -> dict[str, str | None]:
    """Hash the generated tree exactly as the preservation baseline lists it."""
    if not BASELINE_PATH.is_file():
        raise ProvenanceError(f'generated baseline is missing: {BASELINE_PATH}')
    baseline = json.loads(BASELINE_PATH.read_text(encoding='utf-8'))
    files = baseline.get('files')
    if not isinstance(files, dict) or not files:
        raise ProvenanceError('generated baseline lists no files')
    return {name: _digest(ROOT / name) for name in files}


def build_manifest() -> dict[str, Any]:
    """Assemble the provenance manifest from measured inputs."""
    return {
        'schema_version': MANIFEST_SCHEMA_VERSION,
        'checker_version': CHECKER_VERSION,
        'purpose': ('Provenance for the generated tree. Establishes what produced '
                    'the current bytes; it is NOT a claim that the old generation '
                    'is reproducible.'),
        'generation_mode': ('full regeneration 2026-09-28 with the toolkit 2925f0b lifter, '
                            'then relift-selected.py boundaries; hand edits re-applied '
                            '(recomp_types.h project additions, A4b2 watch hooks)'),
        'full_translation_command': FULL_TRANSLATION_COMMAND,
        'inputs': input_hashes(),
        'generators': generator_versions(),
        'generated_outputs': generated_tree_hashes(),
        'protected_markers': {
            'names': list(PROTECTED_MARKERS),
            'per_file': protected_marker_counts(),
            'totals': total_marker_counts(protected_marker_counts()),
        },
        'ownership': {
            'translation_owns': list(TRANSLATION_OWNED),
            'recovery_owns': list(RECOVERY_OWNED),
            'rule': ('recover-functions.py owns only recomp_stubs_recovery.c and '
                     'recovered.c; the full pass owns recomp_funcs.h, the chunks, '
                     'the dispatch table and recomp_stubs_unresolved.c. Never let '
                     'one write the other\'s files.'),
        },
        'known_unknowns': [],
    }


def check_manifest(manifest: Any, *, baseline: dict[str, str | None] | None = None,
                   current: dict[str, str | None] | None = None,
                   markers: dict[str, int] | None = None) -> dict[str, Any]:
    """Validate a manifest.  Returns ``{ok, problems, unknown}``.

    Fails closed on every axis: a missing manifest, a missing input, a changed
    generated byte, or a vanished protected marker is a problem, never an empty
    success.
    """
    problems: list[str] = []
    unknown: list[str] = []

    if not isinstance(manifest, dict):
        return {'ok': False, 'problems': ['manifest root is not an object'],
                'unknown': []}
    if manifest.get('schema_version') != MANIFEST_SCHEMA_VERSION:
        return {'ok': False, 'problems': ['unknown manifest schema version'],
                'unknown': []}

    inputs = manifest.get('inputs')
    if not isinstance(inputs, dict) or not inputs:
        problems.append('manifest records no inputs')
    else:
        for name, entry in inputs.items():
            if not isinstance(entry, dict):
                problems.append(f'input {name} is malformed')
                continue
            if not entry.get('present'):
                unknown.append(f'input {name} was absent when the manifest was built')
                continue
            digest = entry.get('sha256')
            if not isinstance(digest, str) or not re.fullmatch(r'[0-9a-f]{64}', digest):
                problems.append(f'input {name} has no valid hash')
                continue
            actual = _digest(_input_path(name))
            if actual is None:
                unknown.append(f'input {name} is missing now')
            elif actual != digest:
                problems.append(f'input {name} changed since the manifest was built')

    generators = manifest.get('generators')
    if not isinstance(generators, dict):
        problems.append('manifest records no generator identities')
    else:
        for repo in ('project', 'toolkit'):
            state = generators.get(repo)
            if not isinstance(state, dict) or state.get('state') != 'MEASURED':
                unknown.append(f'{repo} repository state is UNKNOWN')
            elif not state.get('revision'):
                unknown.append(f'{repo} repository has no revision')

    outputs = manifest.get('generated_outputs')
    if not isinstance(outputs, dict) or not outputs:
        problems.append('manifest records no generated outputs')
    else:
        reference = baseline if baseline is not None else generated_tree_hashes()
        observed = current if current is not None else generated_tree_hashes()
        for name, expected in reference.items():
            recorded = outputs.get(name)
            if recorded is None:
                problems.append(f'generated output {name} is absent from the manifest')
                continue
            if expected is None:
                unknown.append(f'generated output {name} is missing')
                continue
            if recorded != expected:
                problems.append(f'generated output {name} changed since the manifest')
            if observed.get(name) != expected:
                problems.append(f'generated output {name} differs from the baseline')

    recorded_markers = manifest.get('protected_markers')
    if not isinstance(recorded_markers, dict):
        problems.append('manifest records no protected markers')
    else:
        names = recorded_markers.get('names')
        if not isinstance(names, list) or not names:
            problems.append('manifest lists no protected marker names')
        else:
            missing_names = [m for m in PROTECTED_MARKERS if m not in names]
            if missing_names:
                problems.append('manifest omits protected marker(s): '
                                + ', '.join(missing_names))
        totals = markers if markers is not None else total_marker_counts(
            protected_marker_counts())
        for marker in PROTECTED_MARKERS:
            if totals.get(marker, 0) == 0:
                problems.append(
                    f'protected marker {marker!r} occurs 0 times: the ABI '
                    f'instrumentation has been erased')

    return {'ok': not problems and not unknown,
            'problems': problems, 'unknown': unknown}


def check_only(baseline: dict[str, str | None] | None = None,
               candidate_dir: Path | None = None) -> dict[str, Any]:
    """P0.7-AC2: a no-op generation must not touch the production tree.

    Records the generated tree before and after writing an isolated candidate
    directory, and asserts the production bytes are unchanged.
    """
    candidate_dir = candidate_dir or CANDIDATE_DIR
    before = generated_tree_hashes()
    markers_before = total_marker_counts(protected_marker_counts())

    candidate_dir.mkdir(parents=True, exist_ok=True)
    candidate_manifest = build_manifest()
    (candidate_dir / 'candidate-manifest.json').write_text(
        json.dumps(candidate_manifest, indent=2, sort_keys=True), encoding='utf-8')

    after = generated_tree_hashes()
    markers_after = total_marker_counts(protected_marker_counts())

    changed = sorted(name for name in before if before.get(name) != after.get(name))
    reference = baseline if baseline is not None else before
    return {
        'production_unchanged': not changed,
        'changed_outputs': changed,
        'markers_before': markers_before,
        'markers_after': markers_after,
        'markers_unchanged': markers_before == markers_after,
        'candidate_dir': str(candidate_dir),
        'baseline_matches': all(before.get(n) == reference.get(n) for n in reference),
    }


def detect_indent(text: str) -> int | None:
    """The indent width a JSON file already uses, or None if undetectable.

    Reads the first indented line rather than assuming a style: a record file's
    formatting is part of its reviewability, and rewriting 3,200 lines to change
    two is how a real amendment becomes unreviewable.
    """
    for line in text.splitlines()[1:]:
        stripped = line.lstrip(' ')
        if stripped and stripped != line:
            return len(line) - len(stripped)
        if stripped:
            return None  # a top-level key at column 0: the file is not indented
    return None


def write_manifest(path: Path | None = None) -> dict[str, Any]:
    """Record the measured axes, **preserving the hand-maintained history**.

    ``amendments`` and ``regenerations`` are not measurable: they are the dated
    record of *why* the generated bytes moved, written by hand and by
    ``scripts/recover-functions.py``.  This function used to rebuild the manifest
    from ``build_manifest()`` alone and overwrite the file, which silently erased
    both lists -- measured four times in one session (39/1, then 40/2, then 41/3),
    each time re-attached from a pre-write copy.  A tool that destroys the record
    it is meant to preserve is worse than one that fails, because the loss is
    invisible until someone looks.

    The lists are therefore read back from the existing file and carried over
    verbatim.  A missing or unreadable manifest is not an error here: the first
    write of a fresh tree legitimately has nothing to preserve, and it produces
    empty lists rather than dropping anything.

    The file's own indentation is also preserved.  Both records in this
    repository are written with a one-space indent, so a hard-coded ``indent=2``
    rewrote every line of a 3,200-line file to record a two-line change -- which
    is how a real amendment becomes unreviewable.  A fresh file gets ``indent=1``,
    matching the records this project already has.
    """
    path = path or MANIFEST_PATH
    preserved: dict[str, Any] = {}
    indent = 1
    original_text = ''
    if path.is_file():
        try:
            original_text = path.read_text(encoding='utf-8')
            previous = json.loads(original_text)
        except (OSError, ValueError):
            previous = None
            original_text = ''
        indent = detect_indent(original_text) or indent
        if isinstance(previous, dict):
            for key in PRESERVED_KEYS:
                if key not in previous:
                    # This file does not carry that history section.  Do not
                    # invent an empty one: the two records use different names.
                    continue
                value = previous[key]
                if isinstance(value, list):
                    preserved[key] = value
                else:
                    # Present but not a list: do not silently replace a malformed
                    # record with an empty one.  Fail loudly instead.
                    raise ProvenanceError(
                        f'{path}: {key!r} exists but is not a list '
                        f'({type(value).__name__}); refusing to overwrite the '
                        f'hand-maintained history')
    manifest = build_manifest()
    for key, value in preserved.items():
        manifest[key] = value
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(render_manifest(manifest, indent, original_text), encoding='utf-8')
    return manifest


def extract_raw_value(text: str, key: str) -> str | None:
    """The raw source text of a top-level ``"key": <value>``, or None.

    Scanning rather than re-serializing is deliberate.  The preserved lists are
    history, and history includes *how it was written*: `json.dumps` of a loaded
    list normalizes the key order inside each entry, so a re-serialized list is
    not the file's list even when every value is identical.  Measured on this
    repository's baseline, that reordering moved 60 lines of a 417-line file to
    record a two-line change.  Splicing the original bytes is the only form that
    is exactly a no-op when nothing about the history changed.
    """
    needle = f'"{key}"'
    start = 0
    while True:
        at = text.find(needle, start)
        if at < 0:
            return None
        start = at + len(needle)
        # Top level only: the key must sit at the object's own indent and be
        # followed by a colon.
        line_start = text.rfind('\n', 0, at) + 1
        if text[line_start:at].strip() != '':
            continue
        after = text.find(':', at + len(needle))
        if after < 0:
            return None
        if text[at + len(needle):after].strip() != '':
            continue
        value_start = after + 1
        while value_start < len(text) and text[value_start] in ' \t':
            value_start += 1
        # Walk to the matching close bracket, respecting strings and escapes.
        depth = 0
        in_string = False
        escaped = False
        for i in range(value_start, len(text)):
            ch = text[i]
            if in_string:
                if escaped:
                    escaped = False
                elif ch == '\\':
                    escaped = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
            elif ch in '[{':
                depth += 1
            elif ch in ']}':
                depth -= 1
                if depth == 0:
                    return text[value_start:i + 1]
        return None


def render_manifest(manifest: dict[str, Any], indent: int = 1,
                    original_text: str | None = None) -> str:
    """Serialize a manifest, leaving the hand-maintained history verbatim.

    ``sort_keys=True`` applies to the *measured* axes, which is what makes a
    re-write a clean diff: they have no meaningful order.  The preserved lists do
    have one -- they are dated records, appended in the order things happened, and
    each entry's own keys are in the order a person wrote them.  Sorting them
    reordered 256 lines of history to record a two-line change, which is the same
    reviewability failure as reformatting the whole file.

    When ``original_text`` is given, each preserved list is spliced in as its
    original bytes; otherwise it is serialized in the loaded order.  Both paths
    keep the entries' own key order, so the only difference from the input is the
    measured part.
    """
    base = {k: v for k, v in manifest.items() if k not in PRESERVED_KEYS}
    # `json.dumps({}, indent=n)` is `"{}"`, not `"{\n}"`, so an all-history
    # manifest has no braces to strip and must be handled explicitly.
    if base:
        lines = json.dumps(base, indent=indent, sort_keys=True).splitlines()
        if lines and lines[-1] == '}':
            lines = lines[:-1]
    else:
        # `json.dumps({}, indent=n)` is `"{}"`, so there is no brace to keep and
        # the opening brace of the manifest must be supplied here.
        lines = ['{']
    pad = ' ' * indent
    # Render exactly the preserved keys this manifest actually has, in the order
    # they are listed here.  A file that carries only `updates` must not gain an
    # empty `amendments`/`regenerations` pair.
    for key in PRESERVED_KEYS:
        if key not in manifest:
            continue
        if lines and not lines[-1].rstrip().endswith('{'):
            lines[-1] += ','
        raw = extract_raw_value(original_text, key) if original_text else None
        if raw is not None:
            lines.append(f'{pad}"{key}": {raw}')
            continue
        body = json.dumps(manifest[key], indent=indent)
        body = '\n'.join(('' if i == 0 else pad) + line
                         for i, line in enumerate(body.splitlines()))
        lines.append(f'{pad}"{key}": {body.lstrip()}')
    lines.append('}')
    return '\n'.join(lines) + '\n'


def main(argv: list[str] | None = None) -> int:
    """CLI: ``--check`` validates, ``--write`` records, ``--check-only`` is AC2."""
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true',
                        help='validate the recorded manifest against the tree')
    parser.add_argument('--write', action='store_true',
                        help='record the manifest from measured inputs')
    parser.add_argument('--check-only', action='store_true',
                        help='run a no-op generation into an isolated candidate dir')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args(argv)

    if not (args.check or args.write or args.check_only):
        parser.error('pass --check, --write or --check-only')

    if args.write:
        manifest = write_manifest()
        print(f'wrote {MANIFEST_PATH.relative_to(ROOT)}')
        print(f'  inputs  : {len(manifest["inputs"])}')
        print(f'  outputs : {len(manifest["generated_outputs"])}')
        print(f'  markers : {manifest["protected_markers"]["totals"]}')

    if args.check_only:
        result = check_only()
        if args.json:
            print(json.dumps(result, indent=2, sort_keys=True))
        else:
            print(f'production_unchanged : {result["production_unchanged"]}')
            print(f'markers_unchanged    : {result["markers_unchanged"]}')
            print(f'baseline_matches     : {result["baseline_matches"]}')
            print(f'candidate_dir        : {result["candidate_dir"]}')
        if not (result['production_unchanged'] and result['markers_unchanged']):
            print('check-only touched the production tree', file=sys.stderr)
            return 1

    if args.check:
        if not MANIFEST_PATH.is_file():
            print(f'INVALID: manifest is missing: {MANIFEST_PATH}', file=sys.stderr)
            return 2
        try:
            manifest = json.loads(MANIFEST_PATH.read_text(encoding='utf-8'))
        except Exception as error:
            print(f'INVALID: manifest is unreadable: {error}', file=sys.stderr)
            return 2
        result = check_manifest(manifest)
        if args.json:
            print(json.dumps({'checker_version': CHECKER_VERSION, **result},
                             indent=2, sort_keys=True))
        else:
            print(f'checker {CHECKER_VERSION}')
            print(f'ok       : {result["ok"]}')
            for problem in result['problems']:
                print(f'  problem: {problem}')
            for item in result['unknown']:
                print(f'  unknown: {item}')
        return 0 if result['ok'] else 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
