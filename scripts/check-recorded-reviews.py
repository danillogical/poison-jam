"""Validate one durable review record against its required-criterion manifest.

Usage::

    python -X utf8 scripts/check-recorded-reviews.py \
        --contract docs/reviews/contracts/P0.2.json \
        --review docs/reviews/P0.2/<review-id>.json

Exit status: 0 acceptance-eligible, 1 failed criterion or stale evidence,
2 invalid or unverifiable input.  The command is read-only and never changes plan
status.

This replaces the earlier session-specific heuristic, which was tied to one
hardcoded session id, examined only a child's first turn, and matched generic
verdict words anywhere in the report.  It is a *transaction*: the record's
declared identity, ancestry, route, evidence and per-criterion verdicts must all
bind to real bytes, and the verdicts must come from an authoritative footer in the
reviewer's own completed turn.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from jsrf_review_records import (  # noqa: E402
    ELIGIBLE,
    EXIT_ELIGIBLE,
    EXIT_FAILED,
    EXIT_INVALID,
    INVALID,
    ReviewError,
    load_json_unique,
    load_required_manifest,
    validate_record,
)

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--contract', required=True,
                        help='required-criterion manifest, e.g. docs/reviews/contracts/P0.2.json')
    parser.add_argument('--review', required=True,
                        help='durable review record, e.g. docs/reviews/P0.2/<review-id>.json')
    parser.add_argument('--json', action='store_true', help='emit the result as JSON')
    args = parser.parse_args()

    contract_path = Path(args.contract)
    if not contract_path.is_absolute():
        contract_path = ROOT / contract_path
    review_path = Path(args.review)
    if not review_path.is_absolute():
        review_path = ROOT / review_path

    try:
        manifest = load_required_manifest(contract_path)
    except ReviewError as error:
        print(f'INVALID: {error}', file=sys.stderr)
        return EXIT_INVALID

    if not review_path.is_file():
        print(f'INVALID: review record is missing: {review_path}', file=sys.stderr)
        return EXIT_INVALID

    try:
        record = load_json_unique(review_path)
    except Exception as error:
        print(f'INVALID: review record is unreadable: {error}', file=sys.stderr)
        return EXIT_INVALID

    try:
        result = validate_record(record, manifest, ROOT)
    except ReviewError as error:
        print(f'INVALID: {error}', file=sys.stderr)
        return EXIT_INVALID

    if args.json:
        print(json.dumps({'review': str(review_path), 'contract': str(contract_path),
                          **result}, indent=2, sort_keys=True))
    else:
        print(f'review   : {review_path}')
        print(f'contract : {contract_path} (packet {manifest.get("packet_id")})')
        print(f'status   : {result["status"]}')
        for cid, value in sorted(result.get('criteria', {}).items()):
            print(f'  {cid:<12} {value["disposition"]:<14} {value["reason"]}')
        for reason in result.get('reasons', []):
            print(f'  reason: {reason}')

    if result['status'] == ELIGIBLE:
        return EXIT_ELIGIBLE
    # The interface distinguishes *invalid input* from *failed criteria*: exit 2
    # means the record could not be validated (missing fields, unknown schema, a
    # contract conflict, an unverifiable source), exit 1 means it was validated and
    # does not establish acceptance.  Measured before this fix: every non-eligible
    # status returned 1, so an INVALID record was indistinguishable from a FAILED
    # one -- the distinction the contract requires.
    if result['status'] == INVALID:
        return EXIT_INVALID
    return EXIT_FAILED


if __name__ == '__main__':
    sys.exit(main())
