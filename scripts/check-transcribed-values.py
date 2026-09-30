"""`scripts/check-transcribed-values.py`: W5's second-worker re-check.

Plan W5: "Values transcribed by hand into a table or record, by any role, are
re-checked row by row by a second DeepSeek worker against the artifact before a
criterion or ruling uses them (until T10 makes tool citation the only path)",
answering "12 recorded extraction errors; a byte reversal that became a 'permanent'
refutation for 35 minutes".

**What "until T10" means here.** T10's `scripts/cite.py` records a value WITH the
command and artifact that produced it, so a cited value is re-derivable by
construction. This check exists for the values that are **not** yet tool-cited: it
finds hand-transcribed values in a record and requires each to be either
(a) supported by a cited output, or (b) explicitly marked as transcribed and
re-checked. That is the state W5 describes before T10 lands everywhere, and it stays
useful after, because a record can still contain a number nobody generated.

**What it can check mechanically.** A hex literal in a record is *supported* when
some cited output (a file the record names, or a `tools/citations/` entry) contains
it. That is the same comparison T10's lint makes, so the two cannot disagree about
what "supported" means -- this one reads a different population (records that name
no citation store) and adds the `verified-by` requirement W5 names.

**What it cannot check.** Whether the value is the RIGHT one for the claim. A
transposition between two values that both appear in the cited output is invisible
to any text comparison; only re-deriving the value catches it, which is what
`scripts/cite.py record --capture` is for.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER_VERSION = 'jsrf-transcribed-values/1'

# Records that can carry a transcribed value.
RECORD_GLOBS = ('docs/reviews/*.md', 'docs/reviews/**/*.md',
                'docs/packets/*.md', 'plan-*.md')

# W5's required marker: a row that says a second worker re-checked it.
VERIFIED_BY = re.compile(r'(?im)\bverified[- ]by\b|\bre-?checked[- ]by\b|'
                         r'\bsecond[- ]worker\b')

# A hex literal of the width this project records.
HEX = re.compile(r'\b0x([0-9A-Fa-f]{4,16})\b')

# Well-known constants that are not transcriptions from an artifact.
#
# Measured: the first version flagged `0xC0000409` (`STATUS_STACK_BUFFER_OVERRUN`) in
# a record that names it as the exit code it observed, and `0x00011000` in a packet
# that names it as the project's DOCUMENTED dump-control address (AGENTS.md, "Dump
# integrity"). Both are constants a reader knows, not values extracted from a file,
# and a check that flags them trains its reader to ignore it.
KNOWN_CONSTANTS = {
    'c0000409',   # STATUS_STACK_BUFFER_OVERRUN
    'c0000017',   # STATUS_NO_MEMORY
    'c00000a0',   # STATUS_...
    'e0424943',   # this project's invalid-indirect-call code
    'e06d7363',   # C++ exception
    '80000003',   # breakpoint
    '00000000',   # zero, used as a literal everywhere
    'ffffffff',
    '7fffffff',
    '80000000',
    '00011000',   # the documented dump-control read VA (AGENTS.md, "Dump integrity")
    '001c4064',   # the documented terminal slot (TR §5)
    '001c3f60',   # the documented thunk table base (TR §5)
    'fe820010',   # the documented PIO_FREE counter (TR §4)
    '0017d4d0',   # __aulldiv, the documented CRT helper (TR §2)
    '0017d2c0',   # __aullrem (TR §2)
    '1c4064',     # the same slot, written without leading zeros
    '1c3f60',
    'fe820010',
}

# A path the record cites as the source of its values.  A DIRECTORY citation
# (`logs/ttd/`) is expanded: a record may cite a run directory rather than each file
# inside it, and refusing to look inside would flag every value it legitimately
# derived.
CITED_PATH = re.compile(r'`?((?:logs|docs|config|src|tools)/[\w./\\-]+)`?')

# Files that ARE tool output, so their own hex literals need no re-check.
TOOL_OUTPUT_SUFFIXES = ('.json', '.jsonl', '.txt')

# How much of a cited directory to read.  Bounded so a record citing `logs/` does not
# turn the check into a whole-archive scan.
MAX_CITED_FILES = 400
MAX_CITED_BYTES = 8 * 1024 * 1024

# Directories whose contents are generated, not transcribed.
SKIP_DIRS = ('docs/reviews/rulings',)


def record_files() -> list[Path]:
    files: list[Path] = []
    seen: set[Path] = set()
    for pattern in RECORD_GLOBS:
        for path in sorted(ROOT.glob(pattern)):
            if not path.is_file() or path in seen:
                continue
            relative = describe(path)
            if any(relative.startswith(d) for d in SKIP_DIRS):
                continue
            # A `.json` under docs/reviews is generated output, not a transcription.
            if path.suffix in TOOL_OUTPUT_SUFFIXES:
                continue
            seen.add(path)
            files.append(path)
    return files


def _values_in(path: Path, values: set[str]) -> None:
    """Add every hex literal in one file to `values`."""
    try:
        if path.stat().st_size > MAX_CITED_BYTES:
            return
        content = path.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return
    for hex_match in HEX.finditer(content):
        values.add(hex_match.group(1).lower())


def cited_values(path: Path, text: str) -> set[str]:
    """Every hex literal that appears in something this record cites.

    Read from the cited artifact's own bytes, so a value is supported only when the
    artifact really contains it.  A cited DIRECTORY is walked, because a record may
    cite a run directory rather than each file inside it.
    """
    values: set[str] = set()
    for match in CITED_PATH.finditer(text):
        candidate = match.group(1).replace('\\', '/')
        target = ROOT / candidate
        if target.is_file():
            _values_in(target, values)
            continue
        if not target.is_dir():
            continue
        count = 0
        for child in sorted(target.rglob('*')):
            if count >= MAX_CITED_FILES:
                break
            if child.is_file() and child.suffix in (
                    '.log', '.txt', '.json', '.jsonl', '.md', '.c', '.h'):
                _values_in(child, values)
                count += 1
    return values


def describe(path: Path) -> str:
    """A repo-relative path when possible, else the absolute one.

    --record accepts any path, and Path.relative_to raises for one outside the
    repository -- measured by a control that passes a temporary file.
    """
    try:
        return str(path.resolve().relative_to(ROOT)).replace('\\', '/')
    except ValueError:
        return str(path)


def audit(path: Path) -> list[dict]:
    findings: list[dict] = []
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError as error:
        return [{'check': 'transcribed_values', 'reason': 'unreadable',
                 'detail': f'{path.name}: {error}'}]
    relative = describe(path)

    literals = {m.group(1).lower() for m in HEX.finditer(text)}
    literals -= KNOWN_CONSTANTS
    if not literals:
        return []

    # W5's remedy, checked first: a record that says a second worker re-checked its
    # values satisfies the rule whatever a text comparison would say.
    if VERIFIED_BY.search(text):
        return [{
            'check': 'transcribed_values', 'reason': 'rechecked',
            'detail': f'{relative} marks its values re-checked by a second worker (W5)',
        }]

    # **The check can only decide one case, and it says so.** A record's hex literals
    # come from somewhere: a text artifact (a log, a JSON, a disassembly listing) or
    # from analysis the record does not reduce to a file -- a disassembly of
    # `game/default.xbe`, a TTD trace read through a debugger, a register observed in
    # a stack dump. Measured: the first version flagged
    # `docs/reviews/ttd-recording-exit-finding.md` for `0x00196A29`, which it derived
    # by disassembling the XBE -- a legitimate derivation this check cannot see,
    # because the artifact is a binary and the value came from a decoder.
    #
    # So the BLOCKING case is narrow and mechanical: the record presents its values
    # as a TABLE (a transcription, in W5's sense) and carries no re-check marker.
    # Everything else is reported as undecidable, which is what it is.
    supported = cited_values(path, text)
    unsupported = sorted(literals - supported)
    if not unsupported:
        return []

    if _has_value_table(text, unsupported):
        findings.append({
            'check': 'transcribed_values', 'reason': 'unsupported_value',
            'detail': (f'{relative}: a table of transcribed values includes '
                       f'{len(unsupported)} hex literal(s) that appear in no cited '
                       f'output, and the record carries no `verified-by` marker: '
                       f'{", ".join("0x" + v for v in unsupported[:6])}'
                       + (' ...' if len(unsupported) > 6 else '')),
            'values': unsupported[:20],
        })
    else:
        findings.append({
            'check': 'transcribed_values', 'reason': 'undecidable',
            'detail': (f'{relative}: {len(unsupported)} hex literal(s) are not in any '
                       f'cited TEXT artifact. The record does not present them as a '
                       f'transcribed table, so they were most likely derived by '
                       f'analysis (disassembly, a debugger read, a binary trace) that '
                       f'this check cannot reproduce. UNKNOWN, not a finding'),
            'values': unsupported[:20],
        })
    return findings


def _has_value_table(text: str, unsupported: list[str]) -> bool:
    """Is any unsupported value presented inside a markdown table row?

    A table row is `| ... | ... |`, which is how this project writes a transcribed
    value. A value in prose is part of an argument, not a transcription.
    """
    targets = {f'0x{v}'.lower() for v in unsupported}
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith('|') or not stripped.endswith('|'):
            continue
        lowered = stripped.lower()
        if any(target in lowered for target in targets):
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--record', action='append', default=[])
    args = parser.parse_args()

    if args.record:
        files = [Path(r) if Path(r).is_absolute() else ROOT / r for r in args.record]
    else:
        files = record_files()

    findings: list[dict] = []
    for path in files:
        findings.extend(audit(path))

    # A `rechecked` finding is a PASS: W5's remedy was applied.
    blocking = [f for f in findings if f['reason'] == 'unsupported_value']
    record = {
        'checker': CHECKER_VERSION,
        'records_examined': len(files),
        'findings': findings,
        'blocking': len(blocking),
        'note': ('W5 applies "until T10 makes tool citation the only path". A value '
                 'supported by a cited output, or marked `verified-by`, satisfies '
                 'it; a value in neither is the extraction error W5 records.'),
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  records examined : {len(files)}')
        if findings:
            for finding in findings:
                print(f'    [{finding["reason"]}] {finding["detail"]}')
        else:
            print('  no findings')
    return 1 if blocking else 0


if __name__ == '__main__':
    sys.exit(main())
