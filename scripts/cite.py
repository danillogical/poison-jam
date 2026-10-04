#!/usr/bin/env python
"""cite.py -- tool-cited values for JSRF records (plan T10).

Two recorded failures motivate this tool (plan-jsrf-bare-minimum.md sections 3 and 6):

* twelve recorded extraction errors, one of which was a hand-transcribed byte
  reversal that stood as a "permanent" refutation for 35 minutes;
* values entering records by hand transcription instead of by tool.

So a citation is a value *plus the command and artifact that produced it*, so the
value can be re-derived rather than trusted, and a lint rejects a hex literal in a
record that no cited output contains.

Subcommands
-----------
``record``  run (or read the captured output of) the command that produced a value and
            append a citation record: value, width, byte order, both byte-order
            interpretations, artifact paths with SHA-256, the exact command, the
            command's output and its SHA-256, and a stable id.
``read``    read guest memory from an archived run and print the value in *both* byte
            orders, plus an offset-shift control (the same read at +/-1..3 bytes) so a
            reader can see whether the value they recorded is the value at the address
            they named.  Dump parsing is ``scripts/jsrf_dump.py:DumpMemory``; this tool
            does not reimplement it.
``check``   the lint: scan records for hex literals and report every literal that no
            cited output contains.

KEY UNIVERSE (docs/jsrf-run-profiles.md "Evidence rule")
------------------------------------------------------
A citation is keyed by **the property the decision classifies**, not by the read that
observed it.  ``--key-kind guest-va`` keys a citation by the guest virtual address
whose value is being cited; ``file-offset`` by a byte offset in a named file;
``symbol`` by a symbol name; ``config`` by a config key.  The key universe is
therefore the finite set of keys the owning packet enumerates **from source** -- for
guest memory, the address list derived from the original XBE (for example the 28
enumerated ``PIO_FREE`` sites), never the set of reads a run happened to perform.
A store's size is bounded by ``|keys| x |byte orders| x |producers|`` and does not
grow with run length or input volume.  Pass ``--key-universe FILE`` to make that bound
mechanical: ``record`` refuses a key outside the declared universe, because an
out-of-universe key is a bug detector, not data.

CHECK SCOPE (mechanical, stated so no trigger lacks a verdict)
--------------------------------------------------------------
* Files scanned: every path given to ``check``; a directory is walked recursively.
  Only suffixes in ``TEXT_SUFFIXES`` (.md .markdown .json .jsonl .txt .csv .tsv) are
  scanned; any other suffix is listed as skipped.  With no path given, the default
  scan root is ``tools/citations/records``.
* Literals considered: ``0x`` + 4..16 hex digits, optional ``u``/``l`` suffix,
  bounded by non-word characters.  Literals with more than 16 digits, or separated by
  ``_``, are outside the universe and are not reported (the lint has no verdict for
  them).
* Matching a literal against a cited output is **by value**, case-insensitively.  A cited
  output states a value in either spelling this project uses: ``0x``-prefixed
  (``0x00193D96``, any width up to 16 digits, optional ``u``/``l`` suffix) or bare 8
  digits (``00193D96``, as ``inspect-jsrf.py disasm`` prints).  Because the decision is
  the *value*, width and zero-padding never matter: ``0x193D96`` and ``0x00193D96`` are
  the same number.  Padding never invents support for a *different* value -- ``0x00193D9``
  is not supported by ``0x00193D96``, and a literal inside a longer hex token
  (``0x00193D96FF``) supports nothing.  Requiring the ``0x`` prefix, or exact token text,
  would flag every correct record that cites a disassembly, which is the "flags
  everything" failure a one-sided control hides.  Bare tokens are exactly 8 digits so
  prose cannot enter the value universe (the ``acce`` in "accessed" is not a value).
* What counts as a cited output, per scanned record:
  1. the body of a fenced block whose info string is ``cite`` / ``cited-output``;
  2. the contents of a file named by a ``CITE: <path>`` line (recognised only outside
     code fences);
  3. the citation records of a store named by a ``CITE-STORE: <path>`` line, by a JSON
     ``citations`` field, or by ``--store``;
  4. a JSON record's own ``cited_output`` / ``cited_outputs`` fields -- but only when
     ``sha256(cited_output)`` equals the record's ``output_sha256``; a mismatch is
     itself a finding and that output is not treated as support;
  5. ``artifacts[].path`` that is a readable text file within ``--max-output-bytes``;
     a directory artifact (for example a run directory) is provenance, not a cited
     output -- the cited output of a memory read is the command's output;
  6. files passed with ``--cite-artifact`` (applied to every scanned record).
* Excluded from the literal scan (each exclusion is structural, not heuristic):
  the body of a ``cite`` fence, because it *is* the cited output rather than a claim; the
  contents of a ``CITE:``-named output file, which may itself be a scanned document; the
  lines of a citation store, whose literals are the tool's own transcription of its own
  captured output; and a line that *quotes a command* -- a line whose only ``0x`` literals
  are command arguments (``... disasm 0x00193D50 0x00193DA0``).  A hex literal in a quoted
  command is an input, and this lint has no verdict for inputs, so it does not report one.
  A record that asserts a value in prose is still scanned: a ``CITE:`` line names a cited
  output and is not itself a claim.
* Verdict: SUPPORTED if some cited output contains the literal's *value*, else
  UNSUPPORTED.  A record that names no cited output at all reports every literal as
  ``no-citation``.  Support means "this value appears in a cited output"; it does not mean
  "this value is the value at the recorded offset".  Binding a literal to an offset is the
  job of ``read`` (both byte orders plus the offset-shift control) and of the
  ``value``/``le``/``be`` fields of a ``record``-produced citation.
* The lint detects absence of support in cited outputs.  It does not detect a
  fabricated cited output; re-running the recorded command (``record --capture``) is
  how a cited output is re-derived.

Exit codes: 0 clean, 1 findings, 2 could-not-run (unreadable path, unparsable JSON,
out-of-universe key, value absent from its own output).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

SCHEMA = 'jsrf-citation/1'
KEY_KINDS = ('guest-va', 'file-offset', 'symbol', 'config', 'literal')
BYTE_ORDERS = ('le', 'be')
CITE_FENCE_INFO = ('cite', 'cited-output', 'cited_output')
TEXT_SUFFIXES = ('.md', '.markdown', '.json', '.jsonl', '.txt', '.csv', '.tsv')
DEFAULT_SCAN_ROOT = Path('tools') / 'citations' / 'records'
DEFAULT_MAX_OUTPUT_BYTES = 8 * 1024 * 1024

# An invocation of a tool.  A hex literal in a quoted command is an *input* to the tool
# that produced the cited output; this lint decides whether a claimed value appears in a
# cited output and has no verdict for a tool input, so it must not report one
# (`docs/jsrf-run-profiles.md`: a check whose trigger it has no verdict for is defective).
COMMAND_QUOTE = re.compile(
    r'(?:python(?:\.exe)?|py|just|ctest|cmake|pwsh|powershell|\.\\|scripts[\\/])',
    re.IGNORECASE)


def is_command_line(line: str) -> bool:
    """Does this line quote a command rather than assert a value?

    Structural, not heuristic: the line must look like an invocation (a tool token, at
    least two words) and must carry no prose assertion (no backticks, not a markdown
    table row, not a sentence ending in a full stop).  A line that asserts a value in
    prose is still scanned.
    """
    stripped = line.strip().lstrip('-*> \t').strip()
    if not stripped or not COMMAND_QUOTE.search(stripped):
        return False
    if stripped.startswith('|') or '`' in stripped:
        return False
    words = stripped.split()
    return len(words) >= 2 and not stripped.endswith('.')


# 0x + 4..16 hex digits, optional integer suffix, bounded by non-word characters.
HEX_TOKEN = re.compile(r'(?<![\w])0[xX]([0-9a-fA-F]{4,16})(?:[uUlL]{0,3})(?![\w])')
CITE_LINE = re.compile(r'^\s*CITE(-STORE)?\s*:\s*(.+?)\s*$', re.IGNORECASE)
FENCE_OPEN = re.compile(r'^(`{3,}|~{3,})\s*(.*)$')
FENCE_CLOSE = re.compile(r'^(`{3,}|~{3,})\s*$')

EXIT_CLEAN, EXIT_FINDINGS, EXIT_ERROR = 0, 1, 2


class CiteError(Exception):
    """A could-not-run condition: the tool refuses to guess a verdict."""


# --------------------------------------------------------------------------- hashing


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_text(text: str) -> str:
    """Hash of the UTF-8 encoding of newline-normalised text.

    ``record`` captures command output with universal newlines, so ``\\r\\n`` and
    ``\\r`` are stored as ``\\n``; hashing the same normalised text on both sides keeps
    the ``output_sha256`` check meaningful.
    """
    return sha256_bytes(text.replace('\r\n', '\n').replace('\r', '\n').encode('utf-8'))


def sha256_path(path: Path) -> tuple[str, int, str]:
    """(sha256, size_in_bytes, kind) for a file or a directory tree.

    A directory is hashed over its sorted (relative path, size, sha256) manifest, so a
    run directory has a content identity without depending on walk order or mtimes.
    """
    if path.is_dir():
        entries, total = [], 0
        for child in sorted(p for p in path.rglob('*') if p.is_file()):
            digest = sha256_bytes(child.read_bytes())
            size = child.stat().st_size
            entries.append({'path': child.relative_to(path).as_posix(),
                            'size': size, 'sha256': digest})
            total += size
        canonical = json.dumps(entries, sort_keys=True, separators=(',', ':'))
        return sha256_text(canonical), total, 'directory'
    data = path.read_bytes()
    return sha256_bytes(data), len(data), 'file'


def canonical_json(obj) -> str:
    return json.dumps(obj, sort_keys=True, separators=(',', ':'), ensure_ascii=True)


# ------------------------------------------------------------------------- literals


def find_hex_literals(text: str):
    """Yield (start, end, digits) for every literal inside the lint's universe."""
    for match in HEX_TOKEN.finditer(text):
        digits = match.group(1)
        # A longer token than the universe admits (more than 16 digits) is not reported:
        # the lint has no verdict for it, and reporting one would be a trigger without one.
        if len(digits) > 16:
            continue
        yield match.start(), match.end(), digits


# A `0x`-prefixed hex token of any width, with an optional C integer suffix.  The trailing
# guard is `[0-9a-fA-F]` rather than `\w` so that `0x00193D96u` still matches while a longer
# hex token (`0x00193D96FF`) is a different value and matches nothing.
PREFIXED_TOKEN = re.compile(r'0[xX]([0-9a-fA-F]{1,16})(?:[uUlL]{0,3})(?![0-9a-fA-F])')

# A bare hex token of exactly 8 digits: how `inspect-jsrf.py disasm` prints a guest
# address (`00193D96`).  Exactly 8 digits keeps prose out of the universe -- a shorter run
# such as the `acce` in "accessed" would otherwise become a value -- and the lookbehind
# stops a bare run being read out of the middle of a prefixed token.
BARE_TOKEN = re.compile(r'(?<![0-9A-Za-z_])([0-9a-fA-F]{8})(?![0-9a-fA-F])')


def output_values(text: str) -> frozenset:
    """Every hex value a cited output states, as integers.

    This is the lint's whole notion of "contains": a cited output states a *value*, and a
    record that spells the same value at a different width or with a `0x` prefix is not a
    fabrication.  The failure this lint exists to catch -- `0x00193D62` where `0x00193D96`
    was recorded -- is a different *value* and is still caught.
    """
    values = {int(match.group(1), 16) for match in PREFIXED_TOKEN.finditer(text)}
    values.update(int(match.group(1), 16) for match in BARE_TOKEN.finditer(text))
    return frozenset(values)


def literal_value(digits: str) -> int:
    return int(digits, 16)


def reversed_value(digits: str) -> int | None:
    """The same bytes read in the other byte order, when the width is a whole byte count."""
    if len(digits) % 2:
        return None
    return int.from_bytes(bytes.fromhex(digits)[::-1], 'big')


def line_col(text: str, offset: int) -> tuple[int, int]:
    line = text.count('\n', 0, offset) + 1
    return line, offset - text.rfind('\n', 0, offset)


def parse_int(text: str) -> int:
    try:
        return int(text, 16) if text.lower().startswith('0x') else int(text, 10)
    except ValueError as exc:
        raise CiteError(f'not an integer: {text!r}') from exc


def hex_forms(value: int, width: int) -> str:
    """The canonical ``0x`` spelling of a value at its recorded width."""
    return f'0x{value:0{2 * width}X}'


# ------------------------------------------------------------------- path plumbing


def repo_root_default() -> Path:
    return Path(__file__).resolve().parents[1]


def resolve_relative(name: str, record_path: Path | None, repo_root: Path) -> Path:
    """Resolve a name written in a record: record directory first, then repo root."""
    candidate = Path(name)
    if candidate.is_absolute():
        return candidate
    if record_path is not None:
        near = record_path.parent / candidate
        if near.exists():
            return near
    return repo_root / candidate


def read_text_file(path: Path, max_bytes: int) -> tuple[str | None, str | None]:
    """(text, None) when readable text, else (None, reason)."""
    try:
        if not path.is_file():
            return None, 'not a regular file'
        size = path.stat().st_size
        if size > max_bytes:
            return None, f'{size} bytes exceeds --max-output-bytes'
        with path.open('rb') as handle:
            head = handle.read(8192)
        if b'\x00' in head:
            return None, 'binary (NUL byte in first 8 KiB)'
        return path.read_text(encoding='utf-8', errors='replace'), None
    except OSError as exc:
        return None, f'unreadable: {exc}'


# ------------------------------------------------------------- citation resolution


class CitedOutput:
    __slots__ = ('source', 'text', '_values')

    def __init__(self, source: str, text: str):
        self.source = source
        self.text = text
        self._values = None

    @property
    def values(self) -> frozenset:
        if self._values is None:
            self._values = output_values(self.text)
        return self._values


class RecordScan:
    """Cited outputs, not-searchable artifacts and excluded spans for one file."""

    def __init__(self, path: Path, text: str):
        self.path = path
        self.text = text
        self.outputs: list[CitedOutput] = []
        self.not_searchable: list[tuple[str, str]] = []
        self.excluded_lines: set[int] = set()   # 0-based
        self.errors: list[str] = []
        self.store_paths: list[str] = []
        self.is_store = False                   # whole file is a citation store
        self.store_lines: set[int] = set()      # 0-based JSONL citation lines

    def add_output(self, source: str, text: str):
        self.outputs.append(CitedOutput(source, text))

    def add_file_output(self, path: Path, max_bytes: int):
        text, reason = read_text_file(path, max_bytes)
        if reason is not None:
            self.not_searchable.append((str(path), reason))
        else:
            self.add_output(str(path), text)


def scan_markdown(scan: RecordScan, repo_root: Path, max_bytes: int):
    lines = scan.text.splitlines(keepends=True)
    in_fence, fence_info, fence_start = False, '', 0
    fences = []
    for index, line in enumerate(lines):
        stripped = line.strip()
        if not in_fence:
            opened = FENCE_OPEN.match(stripped)
            if opened:
                in_fence, fence_info, fence_start = True, opened.group(2).strip().lower(), index
            continue
        if FENCE_CLOSE.match(stripped):
            fences.append((fence_start, index, fence_info))
            in_fence, fence_info = False, ''
    if in_fence:
        fences.append((fence_start, len(lines), fence_info))

    fenced_lines = set()
    for start, stop, info in fences:
        for index in range(start, stop):
            fenced_lines.add(index)
        if info.split()[0] if info else '' in CITE_FENCE_INFO:
            body = ''.join(lines[start + 1:stop])
            scan.add_output(f'{scan.path}:{start + 2} fenced {info.split()[0]} block', body)
            scan.excluded_lines.update(range(start, stop + 1))

    for index, line in enumerate(lines):
        if index in fenced_lines:
            continue
        named = CITE_LINE.match(line)
        if not named:
            continue
        is_store, name = bool(named.group(1)), named.group(2)
        if is_store:
            scan.store_paths.append(name)
        else:
            scan.add_file_output(resolve_relative(name, scan.path, repo_root), max_bytes)


def walk_json(obj, scan: RecordScan, repo_root: Path, max_bytes: int, where: str = '$'):
    """Collect cited outputs / stores / artifacts from a parsed JSON document."""
    if isinstance(obj, dict):
        record_consistent = True
        if obj.get('schema') == SCHEMA:
            scan.is_store = True
        if isinstance(obj.get('cited_output'), str):
            record_consistent = (obj.get('output_sha256') in (None, sha256_text(obj['cited_output'])))
            if obj.get('output_sha256') and not record_consistent:
                scan.errors.append(
                    f'{scan.path}:{where}: output_sha256 does not match sha256(cited_output); '
                    'the cited output is not treated as support')
            if record_consistent:
                scan.add_output(f'{scan.path}:{where}.cited_output', obj['cited_output'])
        for field in ('cited_outputs', 'cited_output'):
            entries = obj.get(field)
            if not isinstance(entries, list):
                continue
            for position, entry in enumerate(entries):
                label = f'{scan.path}:{where}.{field}[{position}]'
                if isinstance(entry, str):
                    scan.add_output(label, entry)
                elif isinstance(entry, dict) and isinstance(entry.get('text'), str):
                    scan.add_output(label, entry['text'])
                elif isinstance(entry, dict) and isinstance(entry.get('path'), str):
                    scan.add_file_output(resolve_relative(entry['path'], scan.path, repo_root),
                                         max_bytes)
        citations = obj.get('citations')
        if isinstance(citations, list):
            for entry in citations:
                if isinstance(entry, str):
                    scan.store_paths.append(entry)
                elif isinstance(entry, dict) and isinstance(entry.get('store'), str):
                    scan.store_paths.append(entry['store'])
        artifacts = obj.get('artifacts')
        if isinstance(artifacts, list):
            for position, entry in enumerate(artifacts):
                if not isinstance(entry, dict) or not isinstance(entry.get('path'), str):
                    continue
                path = resolve_relative(entry['path'], scan.path, repo_root)
                if path.is_dir():
                    scan.not_searchable.append((str(path), 'directory artifact: provenance only'))
                else:
                    scan.add_file_output(path, max_bytes)
        for key, value in obj.items():
            if key in ('cited_output', 'cited_outputs'):
                continue
            walk_json(value, scan, repo_root, max_bytes, f'{where}.{key}')
    elif isinstance(obj, list):
        for position, entry in enumerate(obj):
            walk_json(entry, scan, repo_root, max_bytes, f'{where}[{position}]')


def load_store(path: Path, max_bytes: int) -> tuple[list[CitedOutput], list[str]]:
    """(cited outputs, errors) for a citation store (JSON document or JSONL)."""
    text, reason = read_text_file(path, max_bytes)
    if reason is not None:
        return [], [f'{path}: {reason}']
    documents, errors = [], []
    if path.suffix.lower() == '.jsonl':
        for number, line in enumerate(text.splitlines(), 1):
            if not line.strip():
                continue
            try:
                documents.append(json.loads(line))
            except json.JSONDecodeError as exc:
                errors.append(f'{path}:{number}: malformed JSONL record: {exc}')
    else:
        try:
            documents.append(json.loads(text))
        except json.JSONDecodeError as exc:
            errors.append(f'{path}: malformed JSON store: {exc}')
    outputs = []
    for document in documents:
        probe = RecordScan(path, '')
        walk_json(document, probe, path.parent, max_bytes)
        errors.extend(probe.errors)
        outputs.extend(probe.outputs)
    return outputs, errors


def scan_record_file(path: Path, repo_root: Path, max_bytes: int) -> RecordScan:
    text, reason = read_text_file(path, max_bytes)
    if reason is not None:
        raise CiteError(f'{path}: {reason}')
    scan = RecordScan(path, text)
    suffix = path.suffix.lower()
    if suffix == '.jsonl':
        for number, line in enumerate(text.splitlines(), 1):
            if not line.strip():
                continue
            try:
                walk_json(json.loads(line), scan, repo_root, max_bytes, f'$:{number}')
            except json.JSONDecodeError as exc:
                scan.errors.append(f'{path}:{number}: malformed JSONL record: {exc}')
            else:
                if scan.is_store:
                    scan.store_lines.add(number - 1)
                    scan.is_store = False
    elif suffix == '.json':
        try:
            walk_json(json.loads(text), scan, repo_root, max_bytes)
        except json.JSONDecodeError as exc:
            scan.errors.append(f'{path}: malformed JSON record: {exc}')
    else:
        scan_markdown(scan, repo_root, max_bytes)
    return scan


# ------------------------------------------------------------------------ record


def build_record(args, repo_root: Path) -> dict:
    value = parse_int(args.value)
    width = args.width
    if width < 1 or width > 16:
        raise CiteError(f'--width must be 1..16, got {width}')
    if value < 0 or value >= 1 << (8 * width):
        raise CiteError(f'value 0x{value:X} does not fit in {width} byte(s)')
    if width > 1 and not args.byte_order:
        raise CiteError('--byte-order is required when width > 1: a byte order ambiguity '
                        'is only resolvable if the order is recorded')
    order = args.byte_order or 'le'

    offset = None if args.offset is None else parse_int(args.offset)
    key = args.key
    if key is None:
        if args.key_kind == 'guest-va' and offset is not None:
            key = f'0x{offset:08X}'
        else:
            raise CiteError('--key is required (or --offset with --key-kind guest-va): a '
                            'citation is keyed by the property the decision classifies')
    if args.key_universe:
        universe_path = Path(args.key_universe)
        if not universe_path.is_file():
            raise CiteError(f'--key-universe {universe_path} is not a file')
        declared = []
        for line in universe_path.read_text(encoding='utf-8', errors='replace').splitlines():
            line = line.split('#', 1)[0].strip()
            if line:
                declared.append(line)
        if not any(normalise_key(k) == normalise_key(key) for k in declared):
            raise CiteError(f'key {key} is outside the declared universe of '
                            f'{universe_path} ({len(declared)} keys): out-of-universe keys are '
                            'bug detectors, not data')
        key = next(k for k in declared if normalise_key(k) == normalise_key(key)).strip()
    if not args.command:
        raise CiteError('--command is required: a value with no command is hand transcription')

    artifacts = []
    for name in args.artifact or []:
        path = Path(name)
        if not path.exists():
            raise CiteError(f'--artifact {name} does not exist')
        digest, size, kind = sha256_path(path)
        artifacts.append({'path': str(path), 'kind': kind, 'size': size, 'sha256': digest})

    if args.capture:
        capture_path = Path(args.capture)
        text, reason = read_text_file(capture_path, DEFAULT_MAX_OUTPUT_BYTES * 8)
        if reason is not None:
            raise CiteError(f'--capture {capture_path}: {reason}')
        executed, exit_code = False, None
    else:
        completed = subprocess.run(args.command, shell=True, capture_output=True, text=True,
                                   encoding='utf-8', errors='replace')
        text = (completed.stdout or '') + (completed.stderr or '')
        executed, exit_code = True, completed.returncode
        if exit_code != 0:
            raise CiteError(f'command exited {exit_code}; refusing to cite an output that is '
                            f'not a successful read: {args.command}')

    padded = hex_forms(value, width)
    if not args.no_require_output_match and value not in output_values(text):
        raise CiteError(
            f'value {padded} does not appear in the captured output of {args.command!r} '
            'in any accepted spelling: the record would not be re-derivable. Re-read the '
            'artifact with `cite.py read`, or pass --no-require-output-match to record an '
            'explicitly unverified value')

    # `--byte-order` is the order the artifact was read in, so it fixes the byte sequence;
    # the two interpretations of that same sequence are what a reader can compare.
    raw = value.to_bytes(width, byteorder=('little' if order == 'le' else 'big'))
    identity = {
        'key': key, 'key_kind': args.key_kind, 'value': padded, 'width': width,
        'byte_order': order, 'offset': None if offset is None else f'0x{offset:08X}',
        'artifacts': sorted(a['sha256'] for a in artifacts), 'command': args.command,
    }
    record = {
        'schema': SCHEMA,
        'id': 'cite-' + sha256_text(canonical_json(identity))[:16],
        'key': key,
        'key_kind': args.key_kind,
        'value': padded,
        'value_minimal': f'0x{value:X}',
        'value_int': value,
        'width': width,
        'byte_order': order,
        'le': f'0x{int.from_bytes(raw, "little"):0{2 * width}X}',
        'be': f'0x{int.from_bytes(raw, "big"):0{2 * width}X}',
        'offset': None if offset is None else f'0x{offset:08X}',
        'artifacts': artifacts,
        'command': args.command,
        'executed': executed,
        'exit_code': exit_code,
        'output_sha256': sha256_text(text),
        'cited_output': text.replace('\r\n', '\n').replace('\r', '\n'),
        'recorded_utc': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
        'record': args.record,
        'note': args.note,
    }
    if args.key_universe:
        record['key_universe'] = str(args.key_universe)
    return record


def normalise_key(key: str) -> str:
    """Normalise a key so ``0x00193D96``, ``00193D96`` and ``193D96`` compare equal.

    Bare hex is accepted only for the whole token: a decimal-looking key such as ``42``
    stays ``42``, so a decimal key universe is not silently reinterpreted as hex.
    """
    text = key.strip()
    try:
        if text.lower().startswith('0x'):
            return f'0x{int(text, 16):08X}'
        if re.fullmatch(r'[0-9A-Fa-f]{8}', text):
            return f'0x{int(text, 16):08X}'
        int(text, 10)
        return text
    except ValueError:
        return text.lower()


def cmd_record(args, repo_root: Path) -> int:
    record = build_record(args, repo_root)
    line = json.dumps(record, ensure_ascii=True)
    if args.store:
        store = Path(args.store)
        store.parent.mkdir(parents=True, exist_ok=True)
        existing = []
        if store.is_file():
            existing = [json.loads(l) for l in store.read_text(encoding='utf-8').splitlines()
                        if l.strip()]
        if any(item.get('id') == record['id'] for item in existing):
            print(f'{record["id"]}: already recorded in {store} (identical inputs); not appended')
        else:
            with store.open('a', encoding='utf-8', newline='\n') as handle:
                handle.write(line + '\n')
            print(f'{record["id"]}: appended to {store}')
    if args.out:
        Path(args.out).write_text(json.dumps(record, indent=2, ensure_ascii=True) + '\n',
                                  encoding='utf-8')
        print(f'{record["id"]}: wrote {args.out}')
    if not args.quiet:
        print(f'value {record["value"]} width {record["width"]} {record["byte_order"].upper()}'
              f'  le {record["le"]}  be {record["be"]}  key {record["key"]}')
        print(line)
    return EXIT_CLEAN


# -------------------------------------------------------------------------- read


def read_guest(run_dir: Path, va: int, length: int) -> bytes:
    """Read guest memory through the archived-run reader; never reimplement parsing."""
    scripts_dir = str(Path(__file__).resolve().parent)
    if scripts_dir not in sys.path:
        sys.path.insert(0, scripts_dir)
    from jsrf_dump import CaptureError, DumpMemory  # noqa: E402  (path set above)

    try:
        with DumpMemory(run_dir) as dump:
            return dump.read(va, length)
    except CaptureError as exc:
        raise CiteError(f'guest read 0x{va:08X}+{length} from {run_dir}: {exc}') from exc
    except OSError as exc:
        raise CiteError(f'cannot open {run_dir}: {exc}') from exc


def interpret(data: bytes) -> tuple[str, int, str, int]:
    return (f'0x{int.from_bytes(data, "little"):0{2 * len(data)}X}', int.from_bytes(data, 'little'),
            f'0x{int.from_bytes(data, "big"):0{2 * len(data)}X}', int.from_bytes(data, 'big'))


def cmd_read(args, repo_root: Path) -> int:
    run_dir, va, length = Path(args.run), parse_int(args.va), args.length
    if length < 1 or length > 64:
        raise CiteError('--length must be 1..64 bytes')
    if args.verify_mapping:
        checker = Path(__file__).resolve().parent / 'check-dump-mapping.py'
        if not checker.is_file():
            raise CiteError(f'--verify-mapping: {checker} is missing')
        completed = subprocess.run([sys.executable, '-X', 'utf8', str(checker), str(run_dir)],
                                   capture_output=True, text=True, encoding='utf-8', errors='replace')
        tail = (completed.stdout or '').strip().splitlines()[-3:]
        print('mapping check (check-dump-mapping.py) exit '
              f'{completed.returncode}: ' + ' | '.join(tail))
        # Only an unreadable dump stops a read.  check-dump-mapping.py documents that a
        # CONTENT_MISMATCH dump "is still structurally readable: read it at its actual guest
        # VAs. Do not shift reads and do not discard it" -- so its exit 1 is not fatal here,
        # and the verdict is printed above rather than inferred by this tool.
        if completed.returncode == 2 or 'UNREADABLE' in (completed.stdout or ''):
            raise CiteError(f'check-dump-mapping.py exited {completed.returncode} for {run_dir}: '
                            'the dump is unreadable, so no read can be interpreted')

    primary = read_guest(run_dir, va, length)
    if args.bytes_out:
        Path(args.bytes_out).write_bytes(primary)
    le_hex, le_int, be_hex, be_int = interpret(primary)

    shifts, unavailable = [], []
    for shift in (-3, -2, -1, 0, 1, 2, 3):
        address = va + shift
        if address < 0:
            unavailable.append((shift, address, 'negative guest VA'))
            continue
        try:
            data = primary if shift == 0 else read_guest(run_dir, address, length)
        except CiteError as exc:
            unavailable.append((shift, address, str(exc).split(': ', 1)[-1]))
            continue
        shifts.append((shift, address, data))

    payload = {
        'run': str(run_dir), 'guest_va': f'0x{va:08X}', 'length': length,
        'bytes': primary.hex(' '),
        'little_endian': {'hex': le_hex, 'decimal': le_int, 'meaning': 'x86 byte order'},
        'big_endian': {'hex': be_hex, 'decimal': be_int, 'meaning': 'reversed byte order'},
        'offset_shift_control': [
            {'shift': shift, 'guest_va': f'0x{address:08X}', 'bytes': data.hex(' '),
             'little_endian': interpret(data)[0], 'big_endian': interpret(data)[2]}
            for shift, address, data in shifts],
        'offset_shift_unavailable': [
            {'shift': shift, 'guest_va': f'0x{address & 0xFFFFFFFF:08X}', 'reason': reason}
            for shift, address, reason in unavailable],
    }
    if args.json:
        print(json.dumps(payload, indent=2))
        return EXIT_CLEAN

    print(f'run: {run_dir}')
    print(f'guest VA: 0x{va:08X}   length: {length}   bytes: {primary.hex(" ")}')
    print(f'little-endian (x86 byte order):        {le_hex}   ({le_int})')
    print(f'big-endian (reversed byte order):      {be_hex}   ({be_int})')
    print('offset-shift control (same length, shifted guest VA):')
    for shift, address, data in shifts:
        data_le, data_le_int, data_be, _ = interpret(data)
        marker = '  <- recorded read' if shift == 0 else ''
        print(f'  shift {shift:+d}  0x{address:08X}  bytes {data.hex(" ")}  '
              f'le {data_le}  be {data_be}{marker}')
    for shift, address, reason in unavailable:
        print(f'  shift {shift:+d}  0x{address & 0xFFFFFFFF:08X}  UNAVAILABLE: {reason}')
    if not any(shift == 0 for shift, _, _ in shifts):
        print('warning: the offset-shift control did not run at all')
    print('A value transcribed from the wrong row above is a transposition; record only the '
          'row whose guest VA you named.')
    return EXIT_CLEAN


# ------------------------------------------------------------------------- check


def collect_scan_files(paths, repo_root: Path) -> tuple[list[Path], list[Path], list[str]]:
    files, skipped, errors = [], [], []
    for name in paths:
        path = Path(name)
        if not path.is_absolute() and not path.exists():
            path = repo_root / path
        if not path.exists():
            errors.append(f'{name}: no such path')
            continue
        if path.is_dir():
            for child in sorted(path.rglob('*')):
                if not child.is_file():
                    continue
                (files if child.suffix.lower() in TEXT_SUFFIXES else skipped).append(child)
        elif path.suffix.lower() in TEXT_SUFFIXES:
            files.append(path)
        else:
            skipped.append(path)
    return files, skipped, errors


def cmd_check(args, repo_root: Path) -> int:
    paths = list(args.paths)
    if not paths:
        default = repo_root / DEFAULT_SCAN_ROOT
        if not default.exists():
            raise CiteError(f'no paths given and the default scan root {default} does not exist')
        paths = [str(default)]
    files, skipped, errors = collect_scan_files(paths, repo_root)

    global_outputs: list[CitedOutput] = []
    for name in args.cite_artifact or []:
        path = Path(name)
        text, reason = read_text_file(path, args.max_output_bytes)
        if reason is not None:
            errors.append(f'--cite-artifact {path}: {reason}')
        else:
            global_outputs.append(CitedOutput(f'--cite-artifact {path}', text))
    for name in args.store or []:
        outputs, store_errors = load_store(Path(name), args.max_output_bytes)
        global_outputs.extend(outputs)
        errors.extend(store_errors)

    findings, considered, supported_total = [], 0, 0
    for path in files:
        scan = scan_record_file(path, repo_root, args.max_output_bytes)
        errors.extend(scan.errors)
        outputs = list(global_outputs) + list(scan.outputs)
        if scan.is_store:
            continue                      # a citation store is a cited output, not a claim
        for name in scan.store_paths:
            store_path = resolve_relative(name, path, repo_root)
            store_outputs, store_errors = load_store(store_path, args.max_output_bytes)
            outputs.extend(store_outputs)
            errors.extend(store_errors)
        for start, _end, digits in find_hex_literals(scan.text):
            line, column = line_col(scan.text, start)
            if (line - 1) in scan.excluded_lines or (line - 1) in scan.store_lines:
                continue
            if is_command_line(scan.text.splitlines()[line - 1]):
                continue
            considered += 1
            literal = f'0x{digits}'
            value = literal_value(digits)
            if any(value in output.values for output in outputs):
                supported_total += 1
                continue
            hint = ''
            kind = 'no-citation' if not outputs else 'unsupported'
            if kind == 'unsupported':
                flipped = reversed_value(digits)
                if flipped is not None and any(flipped in o.values for o in outputs):
                    hint = (f'; the BYTE-REVERSED spelling 0x{flipped:0{len(digits)}X} IS '
                            'supported, so this is a byte-order transposition, the '
                            'recorded historical error')
            findings.append({'file': str(path), 'line': line, 'column': column, 'literal': literal,
                             'kind': kind,
                             'detail': ('the record names no cited output' if kind == 'no-citation'
                                        else f'no cited output contains {literal}{hint}'),
                             'cited_outputs': [o.source for o in outputs]})
    findings.sort(key=lambda f: (f['file'], f['line'], f['column']))

    payload = {
        'scanned': [str(p) for p in files], 'skipped': [str(p) for p in skipped],
        'considered': considered, 'supported': supported_total,
        'findings': findings, 'errors': errors,
        'cited_outputs': sorted({o.source for o in global_outputs}),
    }
    if args.json:
        print(json.dumps(payload, indent=2))
    else:
        for finding in findings:
            print(f'{finding["file"]}:{finding["line"]}:{finding["column"]}: {finding["literal"]} '
                  f'{finding["kind"]} ({finding["detail"]})')
        for error in errors:
            print(f'ERROR: {error}')
        print(f'scanned {len(files)} file(s), skipped {len(skipped)}; {considered} hex literal(s) '
              f'considered, {supported_total} supported, {len(findings)} unsupported')
        if not files:
            print('warning: no file was scanned; the lint produced no verdict for any literal')
    if errors:
        return EXIT_ERROR
    return EXIT_FINDINGS if findings else EXIT_CLEAN


# --------------------------------------------------------------------------- main


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description='Tool-cited values for JSRF records (plan T10).')
    parser.add_argument('--repo-root', default=None, help='record-relative paths resolve here')
    # `--command` is the producer's recorded command, so the subcommand dest must not be
    # named `command`: argparse would let `--command` overwrite the dispatch key.
    sub = parser.add_subparsers(dest='subcommand', required=True)

    record = sub.add_parser('record', help='record a value with the command and artifact that '
                                           'produced it')
    record.add_argument('--value', required=True, help='the value, 0x-prefixed hex or decimal')
    record.add_argument('--width', type=int, required=True, help='value width in bytes (1..16)')
    record.add_argument('--byte-order', choices=BYTE_ORDERS, default=None,
                        help='byte order of the read (required when width > 1)')
    record.add_argument('--artifact', action='append', default=[],
                        help='artifact the value came from; repeatable; SHA-256 is recorded')
    record.add_argument('--command', required=True, help='exact command that produced the value')
    record.add_argument('--capture', default=None,
                        help='read the command output from this file instead of running it')
    record.add_argument('--offset', default=None, help='offset/address the value was read at')
    record.add_argument('--key', default=None, help='the property the decision classifies')
    record.add_argument('--key-kind', choices=KEY_KINDS, default='guest-va')
    record.add_argument('--key-universe', default=None,
                        help='file listing the finite key universe derived from source')
    record.add_argument('--record', default=None, help='record file this value belongs to')
    record.add_argument('--note', default=None)
    record.add_argument('--store', default=None, help='append the citation to this JSONL store')
    record.add_argument('--out', default=None, help='also write the citation as JSON here')
    record.add_argument('--no-require-output-match', action='store_true',
                        help='permit a value that does not appear in its own captured output')
    record.add_argument('--quiet', action='store_true')

    read = sub.add_parser('read', help='read guest memory: both byte orders and a shift control')
    read.add_argument('run', help='archived run directory (contains process.dmp and stacks.txt)')
    read.add_argument('va', help='guest virtual address')
    read.add_argument('length', type=int, help='number of bytes to read')
    read.add_argument('--json', action='store_true')
    read.add_argument('--bytes-out', default=None, help='write exactly these bytes here')
    read.add_argument('--verify-mapping', action='store_true',
                      help='run check-dump-mapping.py first and refuse a displaced dump')

    check = sub.add_parser('check', help='lint: hex literals no cited output contains')
    check.add_argument('paths', nargs='*', help='record files or directories to scan')
    check.add_argument('--cite-artifact', action='append', default=[],
                       help='file whose contents are a cited output for every record')
    check.add_argument('--store', action='append', default=[],
                       help='citation store (JSON or JSONL) treated as a cited output')
    check.add_argument('--max-output-bytes', type=int, default=DEFAULT_MAX_OUTPUT_BYTES)
    check.add_argument('--json', action='store_true')
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    repo_root = Path(args.repo_root).resolve() if args.repo_root else repo_root_default()
    try:
        if args.subcommand == 'record':
            return cmd_record(args, repo_root)
        if args.subcommand == 'read':
            return cmd_read(args, repo_root)
        return cmd_check(args, repo_root)
    except CiteError as exc:
        print(f'cite.py {args.subcommand}: {exc}', file=sys.stderr)
        return EXIT_ERROR


if __name__ == '__main__':
    sys.exit(main())
