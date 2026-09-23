"""Read-only DSH review-source adapter for P0.2.

Decodes an original DSH child session log (``session.v3.jsonl.zstd``) and exposes
the identity, ancestry, route and selected completed turn needed by review
validation.  Originals are **never** written; decoding is streamed to memory.

The projection cache is explicitly *not* sufficient: measured cache records carry
no parent id in ``subagent.identity`` and contain truncated responses.  Missing
decoder or source is UNKNOWN, never an empty success.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1
ADAPTER_VERSION = 'jsrf-dsh-source/1'
SOURCE_KIND = 'dsh_session_turn'


class DshSourceError(ValueError):
    """The DSH source cannot be decoded or does not carry the needed identity."""


def _decompressor():
    try:
        import zstandard  # noqa: PLC0415
    except Exception as error:  # pragma: no cover - environment dependent
        raise DshSourceError(
            f'zstandard decoder is unavailable, so the original DSH log cannot be '
            f'read: {error}') from error
    return zstandard.ZstdDecompressor()


def read_records(path: Path) -> list[dict[str, Any]]:
    """Decode every JSONL record from a zstd-compressed session log."""
    if not path.is_file():
        raise DshSourceError(f'DSH session log is missing: {path}')
    decompressor = _decompressor()
    try:
        with path.open('rb') as handle:
            with decompressor.stream_reader(handle) as reader:
                raw = reader.read()
    except Exception as error:
        raise DshSourceError(f'DSH session log could not be decoded: {error}') from error

    records: list[dict[str, Any]] = []
    for line in raw.split(b'\n'):
        if not line.strip():
            continue
        try:
            parsed = json.loads(line)
        except Exception:
            continue
        if isinstance(parsed, dict):
            records.append(parsed)
    if not records:
        raise DshSourceError('DSH session log decoded to no records')
    return records


def source_identity(path: Path) -> dict[str, Any]:
    """Header identity and ancestry of a DSH child log."""
    records = read_records(path)
    header = next((r for r in records if r.get('type') == 'session'), None)
    descriptor = next((r for r in records
                       if r.get('type') in ('subagent/descriptor', 'delegation/descriptor')), None)
    if header is None:
        raise DshSourceError('DSH log has no session header record')

    child_id = header.get('id')
    if not isinstance(child_id, str) or not child_id:
        raise DshSourceError('DSH session header has no id')
    parent_id = header.get('parentSession')
    if not isinstance(parent_id, str) or not parent_id:
        raise DshSourceError(
            'DSH session header records no parentSession; ancestry is unverifiable '
            'from this source alone')

    route: dict[str, Any] = {}
    if isinstance(descriptor, dict):
        data = descriptor.get('data') or {}
        route = {
            'provider': data.get('agentProvider'),
            'model': data.get('agentModel'),
            'effort': data.get('agentReasoningEffort'),
            'mode': data.get('mode'),
            'label': data.get('label'),
        }
    return {
        'child_id': child_id,
        'parent_id': parent_id,
        'created_at': header.get('createdAt'),
        'schema_version': header.get('version'),
        'route': route,
    }


def completed_turns(path: Path) -> list[dict[str, Any]]:
    """Every turn, with its verdict text separated from its narration.

    A turn is completed only when a ``turn/end`` record carries
    ``reason.kind == 'completed'``.  Turns that errored are reported with their
    error so a route failure cannot be mistaken for a verdict.

    **Verdict selection is footer-first, and that ordering is load-bearing.**  A
    reviewer commonly emits its full review in one assistant message and then a
    short "delivered, result was X" summary in the next.  Taking the *last*
    message therefore picks the summary and loses the footer entirely -- measured
    on a real review, where the footer sat in a 4,510-character message and the
    final message was a 1,537-character summary.  So:

    * ``text`` is the **last assistant message that carries a top-level
      ``REVIEW_CRITERIA_JSON:`` footer**, falling back to the last non-empty
      message when no message carries one (which then fails footer validation
      honestly rather than silently picking the wrong text);
    * ``narration`` is every earlier message, kept separate and never used as the
      verdict: it contains intermediate reasoning that can mention dispositions,
      so folding it in would manufacture false prose-versus-footer contradictions.
    """
    records = read_records(path)
    turns: dict[Any, dict[str, Any]] = {}
    for record in records:
        kind = record.get('type')
        data = record.get('data') or {}
        turn = data.get('turn')
        if turn is None:
            continue
        entry = turns.setdefault(turn, {'turn': turn, 'messages': [], 'completed': False,
                                        'error': None})
        if kind == 'assistant/message':
            message = data.get('message') or {}
            text = '\n'.join(block.get('text') or ''
                             for block in (message.get('content') or [])
                             if isinstance(block, dict) and block.get('type') == 'text')
            if text.strip():
                entry['messages'].append({'text': text,
                                          'message_id': message.get('id')})
        elif kind == 'turn/end':
            reason = data.get('reason') or {}
            entry['completed'] = reason.get('kind') == 'completed'
            if reason.get('kind') == 'error':
                entry['error'] = reason.get('error')

    result: list[dict[str, Any]] = []
    for turn in sorted(turns, key=lambda value: (value is None, value)):
        entry = turns[turn]
        messages = entry['messages']
        with_footer = [m for m in messages
                       if any(line.startswith('REVIEW_CRITERIA_JSON:')
                              for line in m['text'].split('\n'))]
        chosen = with_footer[-1] if with_footer else (messages[-1] if messages else None)
        narration = [m['text'] for m in messages if m is not chosen]
        result.append({
            'turn': turn,
            'completed': entry['completed'],
            'error': entry['error'],
            # Authority scope: the single message whose footer is authoritative.
            'text': chosen['text'] if chosen else '',
            # Contradiction scope: the WHOLE turn, so a disagreement stated in an
            # earlier message cannot be disowned by quoting a footer shape later.
            'turn_text': '\n'.join(m['text'] for m in messages),
            'narration': '\n'.join(narration),
            'final_message_id': chosen['message_id'] if chosen else None,
            'footer_message_count': len(with_footer),
            'message_ids': [m['message_id'] for m in messages],
        })
    return result


def export_turn(path: Path, turn: Any) -> dict[str, Any]:
    """Export one completed DSH turn as a durable source record."""
    identity = source_identity(path)
    candidates = [entry for entry in completed_turns(path) if entry['turn'] == turn]
    if not candidates:
        raise DshSourceError(f'turn {turn!r} does not exist in this DSH log')
    entry = candidates[0]
    if entry['error'] is not None:
        raise DshSourceError(
            f'turn {turn!r} ended with a provider error, not a verdict: {entry["error"]}')
    if not entry['completed']:
        raise DshSourceError(f'turn {turn!r} is not a completed turn')
    if not entry['text'].strip():
        raise DshSourceError(f'turn {turn!r} has no assistant text')

    return {
        'schema_version': SCHEMA_VERSION,
        'adapter_version': ADAPTER_VERSION,
        'source_kind': SOURCE_KIND,
        'harness': 'dsh',
        'child_id': identity['child_id'],
        'parent_id': identity['parent_id'],
        'turn_id': str(turn),
        'provider': identity['route'].get('provider'),
        'model': identity['route'].get('model'),
        'effort': identity['route'].get('effort'),
        'verdict_text': entry['text'],
        'turn_text': entry['turn_text'],
        'provenance': {
            'session_path': str(path),
            'turn': turn,
            'final_message_id': entry['final_message_id'],
            'message_ids': entry['message_ids'],
        },
    }


def digest_source(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()
