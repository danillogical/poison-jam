"""Record a completed reviewer turn as a durable P0.2 review record.

Usage::

    python -X utf8 scripts/record-review.py \
        --child <dsh-child-id> --turn <turn> --packet P0.3 \
        --review-id p0-3-acceptance --reviewed <path> [<path> ...] \
        [--criteria-json '<footer json>']

This is the *producer* half of the P0.2 transaction: it reads the reviewer's own
completed turn from the original DSH session log, writes the verdict text as a
tracked source, and emits a record that `check-recorded-reviews.py` can then
validate. It never invents a disposition -- every criterion row is filled from the
footer the reviewer actually emitted, and a criterion the footer does not map is
recorded as `CANNOT VERIFY`.

Exit 0 on write, 2 if the turn cannot be used (errored, incomplete, no footer).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))

from jsrf_dsh_source import DshSourceError, export_turn, source_identity  # noqa: E402
from jsrf_review_records import (  # noqa: E402
    AGREED,
    CANNOT_VERIFY,
    ReviewError,
    extract_footer,
    parse_footer_map,
)

SESSION_ROOT = Path.home() / '.dsh' / 'sessions' / '--C-Users-logic-Repos-my_xbox_game--'
CONTRACT = 'docs/packets/p0-acceptance-contract.md'


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--child', required=True, help='DSH child session id')
    parser.add_argument('--turn', required=True, help='completed turn number')
    parser.add_argument('--packet', required=True,
                        help='packet id, e.g. P0.3; may be a comma-separated list '
                             'when one review covers several packets (P0.3,P0.4,...)')
    parser.add_argument('--review-id', required=True)
    parser.add_argument('--reviewed', nargs='+', required=True,
                        help='repo-relative paths the review covers')
    parser.add_argument('--requested-model', default='workbuddy-ai/hy4-preview-f')
    parser.add_argument('--requested-effort', default='high')
    parser.add_argument('--session-root', default=str(SESSION_ROOT))
    parser.add_argument('--note', default='')
    args = parser.parse_args()

    # A single review may legitimately cover several packets -- measured: one
    # acceptance review returned all 18 criteria for P0.3 through P0.7 at once.  The
    # attribution guard below must still reject a footer naming a packet that was
    # NOT requested, so the requested set is what it checks against.
    packets = [p.strip() for p in args.packet.split(',') if p.strip()]
    if not packets:
        print('INVALID: --packet names no packet', file=sys.stderr)
        return 2

    session = Path(args.session_root) / args.child / 'session.v3.jsonl.zstd'
    if not session.is_file():
        print(f'INVALID: DSH session log is missing: {session}', file=sys.stderr)
        return 2

    try:
        identity = source_identity(session)
        exported = export_turn(session, int(args.turn) if args.turn.isdigit() else args.turn)
    except DshSourceError as error:
        print(f'INVALID: {error}', file=sys.stderr)
        return 2

    verdict = exported['verdict_text']
    if not verdict.strip():
        print('INVALID: the selected turn has no verdict text', file=sys.stderr)
        return 2

    # The footer is the authority for every disposition; refuse to guess.
    footer = extract_footer(verdict)
    if footer['status'] != 'authoritative':
        print(f'INVALID: the verdict has no authoritative footer ({footer["status"]}): '
              f'{footer["reason"]}', file=sys.stderr)
        return 2
    try:
        footer_map = parse_footer_map(footer['line'])
    except ReviewError as error:
        print(f'INVALID: the footer is malformed: {error}', file=sys.stderr)
        return 2

    # The footer must name the requested packets' criteria and nothing else.
    #
    # Without this the tool would happily record whatever a reviewer's turn
    # happened to say, including a review of a different artefact.  Measured: the
    # P0.2 *amendment* reviewer's session contains a `P0.2-AC1` line, and the
    # session cited it as an implementation acceptance review -- attributing a
    # verdict about a document to work that review never saw.  A footer naming
    # criteria outside the requested set is now refused rather than recorded.
    def packet_of(criterion_id: str) -> str | None:
        match = re.match(r'^(P0\.\d+)-AC\d+$', criterion_id)
        return match.group(1) if match else None

    foreign = sorted(cid for cid in footer_map if packet_of(cid) not in packets)
    if foreign:
        print(f'INVALID: the footer names criteria outside {", ".join(packets)}: '
              f'{", ".join(foreign)}. A verdict is about the revision it saw; use the '
              f'matching --packet or select the right turn.', file=sys.stderr)
        return 2
    if not footer_map:
        print(f'INVALID: the footer maps no criterion for {", ".join(packets)}',
              file=sys.stderr)
        return 2

    # And it must agree with each packet's independent required-criterion manifest,
    # so a partial footer cannot silently under-report a packet's population.
    for packet in packets:
        manifest_path = ROOT / 'docs' / 'reviews' / 'contracts' / f'{packet}.json'
        if not manifest_path.is_file():
            print(f'INVALID: no required-criterion manifest for {packet}',
                  file=sys.stderr)
            return 2
        try:
            manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
        except Exception as error:
            print(f'INVALID: the {packet} manifest is unreadable: {error}',
                  file=sys.stderr)
            return 2
        required = set(manifest.get('required_ids') or [])
        present = {cid for cid in footer_map if packet_of(cid) == packet}
        missing = sorted(required - present)
        extra = sorted(present - required)
        if missing or extra:
            print(f'INVALID: the footer population does not match the {packet} '
                  f'manifest. missing={missing} extra={extra}', file=sys.stderr)
            return 2

    contract_path = ROOT / CONTRACT
    contract_hash = sha256_file(contract_path)

    # Write the verdict text as a tracked source, then bind it by hash.
    # A multi-packet review is filed under its first packet, with the full set
    # recorded in `packet_id` so the record says what it actually covered.
    out_dir = ROOT / 'docs' / 'reviews' / packets[0]
    out_dir.mkdir(parents=True, exist_ok=True)
    source_path = out_dir / f'{args.review_id}-source.txt'
    source_path.write_text(verdict, encoding='utf-8')
    source_rel = str(source_path.relative_to(ROOT)).replace('\\', '/')
    source_hash = sha256_file(source_path)

    reviewed = []
    for name in args.reviewed:
        path = ROOT / name
        if not path.is_file():
            print(f'INVALID: reviewed path does not exist: {name}', file=sys.stderr)
            return 2
        reviewed.append({'path': name, 'sha256': sha256_file(path)})

    criteria = []
    for cid, disposition in sorted(footer_map.items()):
        criteria.append({
            'id': cid,
            'disposition': disposition,
            'evidence': [{'path': source_rel, 'sha256': source_hash}],
            'procedure': (f"read the reviewer's own completed turn "
                          f"{exported['turn_id']} of child {identity['child_id']}"),
            'observed_result': f'the reviewer footer states {disposition}',
        })

    record = {
        'schema_version': 1,
        'review_id': args.review_id,
        'packet_id': packets[0],
        'covers_packets': packets,
        'contract_sha256': contract_hash,
        'reviewed_files': reviewed,
        'harness': 'dsh',
        'parent_id': identity['parent_id'],
        'child_id': identity['child_id'],
        'requested_model': args.requested_model,
        'requested_effort': args.requested_effort,
        'identity_evidence': {'kind': 'turn_text', 'path': source_rel,
                              'sha256': source_hash,
                              # The original log, so the validator can re-open it and
                              # check ancestry and route instead of trusting the
                              # record.  A record that omits this is INVALID.
                              'source_session': str(session)},
        'turn_id': str(exported['turn_id']),
        'verdict_text': verdict,
        'contradiction_evidence': exported['turn_text'],
        'criteria': criteria,
    }
    if args.note:
        record['note'] = args.note

    record_path = out_dir / f'{args.review_id}.json'

    # Preserve an existing `delta` block.  A re-run of the producer must not erase
    # the recorded post-review delta: measured, rewriting the P0.1 record dropped
    # the block that documents why its `AGENTS.md` binding changed, which is the
    # very bookkeeping the post-review rule depends on.
    if record_path.is_file():
        try:
            previous = json.loads(record_path.read_text(encoding='utf-8'))
        except Exception:
            previous = {}
        if isinstance(previous, dict) and 'delta' in previous:
            record['delta'] = previous['delta']
            print(f'  preserved the existing delta block '
                  f'(affected criteria: {previous["delta"].get("affected_criteria") or "none"})')

    record_path.write_text(json.dumps(record, indent=2, sort_keys=True), encoding='utf-8')

    print(f'wrote {record_path.relative_to(ROOT)}')
    print(f'  source  : {source_rel} ({len(verdict)} chars)')
    print(f'  parent  : {identity["parent_id"]}')
    print(f'  child   : {identity["child_id"]}')
    print(f'  turn    : {exported["turn_id"]}')
    print(f'  criteria: {len(criteria)}')
    for cid, disposition in sorted(footer_map.items()):
        print(f'    {cid:<12} {disposition}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
