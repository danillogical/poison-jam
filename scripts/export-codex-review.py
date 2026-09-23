"""Export one completed Codex reviewer turn as a durable, verifiable source.

Usage::

    python -X utf8 scripts/export-codex-review.py \
        --rollout <child-rollout.jsonl> \
        --turn-id <completed-turn-id> \
        --output <source.json>

The export carries only what review validation needs: the child's identity and
ancestry, the route that answered, the selected completed verdict, and the
provenance needed to re-open and re-check it.  Prompts, reasoning, environment and
unrelated turns are deliberately omitted.

Provenance is the **immutable prefix through the selected completion**: later
appended turns do not invalidate an earlier completed review, but a change to
anything up to and including that completion does.  The exporter records the
selected record ordinals and the hash/length of that prefix, and the validator
re-opens the source and re-checks the prefix identity.

A hand-written JSON claiming to be an export is not sufficient: the validator
compares the exported fields with the events in the source itself.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

SCHEMA_VERSION = 1
EXPORTER_VERSION = 'jsrf-codex-export/1'
SOURCE_KIND = 'codex_rollout_prefix'

TURN_ID_RE = re.compile(r'^[0-9a-zA-Z-]{8,}$')


class ExportError(ValueError):
    """The rollout or selection cannot produce a trustworthy export."""


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_records(path: Path) -> list[tuple[int, dict]]:
    """Read a JSONL rollout, keeping the ordinal of every parseable record."""
    records: list[tuple[int, dict]] = []
    with path.open('r', encoding='utf-8', errors='replace') as handle:
        for ordinal, line in enumerate(handle):
            if not line.strip():
                continue
            try:
                parsed = json.loads(line)
            except Exception:
                continue
            if isinstance(parsed, dict):
                records.append((ordinal, parsed))
    return records


def prefix_identity(path: Path, ordinal: int) -> tuple[int, str]:
    """Hash and byte-length of the raw prefix through a given record ordinal.

    The prefix is measured in **bytes of the file**, not of the decoded records,
    so it is stable under any re-encoding choice.
    """
    total = 0
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for index, line in enumerate(handle):
            digest.update(line)
            total += len(line)
            if index == ordinal:
                return total, digest.hexdigest()
    raise ExportError(f'record ordinal {ordinal} is beyond the end of the rollout')


def find_session_meta(records: list[tuple[int, dict]]) -> tuple[int, dict]:
    for ordinal, record in records:
        if record.get('type') == 'session_meta':
            payload = record.get('payload')
            if isinstance(payload, dict):
                return ordinal, payload
    raise ExportError('rollout has no session_meta record; ancestry is unknown')


def export(rollout: Path, turn_id: str) -> dict:
    if not rollout.is_file():
        raise ExportError(f'rollout file is missing: {rollout}')
    if not TURN_ID_RE.match(turn_id or ''):
        raise ExportError(f'turn id {turn_id!r} is not a well-formed identifier')

    records = read_records(rollout)
    if not records:
        raise ExportError('rollout has no parseable records')

    meta_ordinal, meta = find_session_meta(records)

    child_id = meta.get('id')
    parent_id = meta.get('parent_thread_id') or (
        ((meta.get('source') or {}).get('subagent') or {})
        .get('thread_spawn', {}).get('parent_thread_id'))
    if not isinstance(child_id, str) or not child_id:
        raise ExportError('session_meta has no child id')
    if not isinstance(parent_id, str) or not parent_id:
        raise ExportError('session_meta has no parent thread id; ancestry is unverifiable')

    contexts = [(ordinal, record['payload']) for ordinal, record in records
                if record.get('type') == 'turn_context'
                and isinstance(record.get('payload'), dict)
                and record['payload'].get('turn_id') == turn_id]
    completions = [(ordinal, record['payload']) for ordinal, record in records
                   if record.get('type') == 'event_msg'
                   and isinstance(record.get('payload'), dict)
                   and record['payload'].get('type') == 'task_complete'
                   and record['payload'].get('turn_id') == turn_id]

    if not contexts:
        raise ExportError(f'no turn_context record for turn {turn_id!r}')
    if len(contexts) > 1:
        raise ExportError(f'ambiguous turn: {len(contexts)} turn_context records '
                          f'for {turn_id!r}')
    if not completions:
        raise ExportError(f'turn {turn_id!r} has no completed task_complete record')
    if len(completions) > 1:
        raise ExportError(f'ambiguous completion: {len(completions)} task_complete '
                          f'records for {turn_id!r}')

    context_ordinal, context = contexts[0]
    complete_ordinal, completion = completions[0]
    if complete_ordinal < context_ordinal:
        raise ExportError('completion precedes its own turn context')

    verdict = completion.get('last_agent_message')
    if not isinstance(verdict, str) or not verdict.strip():
        raise ExportError('the selected turn has an empty completed verdict')

    collaboration = context.get('collaboration_mode')
    settings = collaboration.get('settings') if isinstance(collaboration, dict) else None
    model = context.get('model') or (settings or {}).get('model')
    effort = context.get('effort') or (settings or {}).get('reasoning_effort')
    if not isinstance(model, str) or not model:
        raise ExportError('the selected turn records no model')

    length, digest = prefix_identity(rollout, complete_ordinal)
    return {
        'schema_version': SCHEMA_VERSION,
        'exporter_version': EXPORTER_VERSION,
        'source_kind': SOURCE_KIND,
        'harness': 'codex',
        'child_id': child_id,
        'parent_id': parent_id,
        'turn_id': turn_id,
        'model': model,
        'effort': effort,
        'verdict_text': verdict,
        'provenance': {
            'rollout_path': str(rollout),
            'session_meta_ordinal': meta_ordinal,
            'turn_context_ordinal': context_ordinal,
            'completion_ordinal': complete_ordinal,
            'prefix_bytes': length,
            'prefix_sha256': digest,
        },
    }


def verify_export(exported: dict, rollout: Path) -> list[str]:
    """Re-open the source and re-check the export against it.

    Returns a list of problems; empty means the export matches its source.
    """
    problems: list[str] = []
    if exported.get('schema_version') != SCHEMA_VERSION:
        return ['unknown export schema version']
    if exported.get('source_kind') != SOURCE_KIND:
        return ['export is not a codex rollout-prefix source']
    provenance = exported.get('provenance')
    if not isinstance(provenance, dict):
        return ['export has no provenance block']
    if str(provenance.get('rollout_path')) != str(rollout):
        problems.append('export names a different rollout path')
    if not rollout.is_file():
        return problems + [f'rollout source is missing: {rollout}']

    records = read_records(rollout)
    meta_ordinal, meta = find_session_meta(records)
    if meta.get('id') != exported.get('child_id'):
        problems.append('child id does not match the source session_meta')
    parent_id = meta.get('parent_thread_id') or (
        ((meta.get('source') or {}).get('subagent') or {})
        .get('thread_spawn', {}).get('parent_thread_id'))
    if parent_id != exported.get('parent_id'):
        problems.append('parent id does not match the source session_meta')

    turn_id = exported.get('turn_id')
    contexts = [p for _, r in records if r.get('type') == 'turn_context'
                for p in [r.get('payload')] if isinstance(p, dict)
                and p.get('turn_id') == turn_id]
    completions = [p for _, r in records if r.get('type') == 'event_msg'
                   for p in [r.get('payload')] if isinstance(p, dict)
                   and p.get('type') == 'task_complete' and p.get('turn_id') == turn_id]
    if len(contexts) != 1:
        problems.append(f'expected exactly one turn_context for the exported turn, found {len(contexts)}')
    if len(completions) != 1:
        problems.append(f'expected exactly one completion for the exported turn, found {len(completions)}')
    if len(completions) == 1 and completions[0].get('last_agent_message') != exported.get('verdict_text'):
        problems.append('exported verdict text does not match the source completion')
    if len(contexts) == 1:
        context = contexts[0]
        collaboration = context.get('collaboration_mode')
        settings = collaboration.get('settings') if isinstance(collaboration, dict) else None
        model = context.get('model') or (settings or {}).get('model')
        effort = context.get('effort') or (settings or {}).get('reasoning_effort')
        if model != exported.get('model'):
            problems.append('exported model does not match the source turn context')
        if effort != exported.get('effort'):
            problems.append('exported effort does not match the source turn context')

    completion_ordinal = provenance.get('completion_ordinal')
    if not isinstance(completion_ordinal, int):
        problems.append('provenance has no completion ordinal')
    else:
        try:
            length, digest = prefix_identity(rollout, completion_ordinal)
        except ExportError as error:
            problems.append(str(error))
        else:
            if length != provenance.get('prefix_bytes'):
                problems.append('source prefix length changed since export')
            if digest != provenance.get('prefix_sha256'):
                problems.append('source prefix hash changed since export')
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rollout', required=True, help='child rollout .jsonl')
    parser.add_argument('--turn-id', required=True, help='selected completed turn id')
    parser.add_argument('--output', required=True, help='destination source json')
    parser.add_argument('--verify', action='store_true',
                        help='re-check an existing export against its source instead')
    args = parser.parse_args()

    rollout = Path(args.rollout)
    output = Path(args.output)
    try:
        if args.verify:
            exported = json.loads(output.read_text(encoding='utf-8'))
            problems = verify_export(exported, rollout)
            for problem in problems:
                print(f'  problem: {problem}')
            print('verify: OK' if not problems else 'verify: FAILED')
            return 0 if not problems else 1
        exported = export(rollout, args.turn_id)
    except ExportError as error:
        print(f'export failed: {error}', file=sys.stderr)
        return 2

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(exported, indent=2, sort_keys=True), encoding='utf-8')
    print(f'wrote {output}')
    print(f'  child    : {exported["child_id"]}')
    print(f'  parent   : {exported["parent_id"]}')
    print(f'  turn     : {exported["turn_id"]}')
    print(f'  route    : {exported["model"]} @ {exported["effort"]}')
    print(f'  prefix   : {exported["provenance"]["prefix_bytes"]} bytes '
          f'{exported["provenance"]["prefix_sha256"][:16]}…')
    return 0


if __name__ == '__main__':
    sys.exit(main())
