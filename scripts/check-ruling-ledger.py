"""`scripts/check-ruling-ledger.py`: W6's ruling-ledger lint.

Plan W6: "**Ruling ledger per question** (`docs/reviews/rulings/<question>.md`): a
new ruling **lists the hazards of earlier rulings it supersedes**", answering "A2h
page-guard rejected at 00:38, adopted at 09:32, then failed on the hazard cited at
00:38".

**The failure, restated.** A ruling was rejected for a stated hazard, adopted eight
hours later without that hazard being addressed, and then failed on exactly the
hazard that had been named. The information existed and was lost between two
decisions, because nothing required the second decision to read the first.

**What the lint checks**, each mechanical:

  1. **A ruling states what would reverse it.** `docs/agent-workflow.md` §3.3 makes
     this part of a technical-policy ruling: "the question and decision; observed /
     inferred / uncertain basis; **what would reverse it**; where it is recorded".
     Without it a ruling cannot be revisited on evidence, only on authority.
  2. **A ruling states its basis** in the observed/inferred/uncertain vocabulary
     (§2.4.1), so a reader can tell a measurement from a hypothesis.
  3. **A superseding ruling names the hazards of what it supersedes.** This is W6's
     specific requirement. A ruling that says it supersedes another must say which
     hazards of the earlier one it is carrying, accepting, or refuting -- otherwise
     the A2h sequence repeats.
  4. **A ruling names its owning document**, so the rule it states is findable.

**What it does not check**: whether a ruling is *correct*, whether it should have
been made, or whether a hazard is adequately handled. Those are §2.3 judgements. The
lint checks that the four facts are present, which is what makes a later reader able
to find the earlier decision at all.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RULINGS = ROOT / 'docs' / 'reviews' / 'rulings'
CHECKER_VERSION = 'jsrf-ruling-ledger/1'

# The four facts §3.3 and W6 require, as heading or label patterns.
REVERSED_BY = re.compile(r'(?im)^#{1,4}\s*reversed?\s*by\b|^\s*\*{0,2}REVERSED[_ ]BY\b')
BASIS = re.compile(r'(?im)^#{1,4}\s*basis\b|^\s*\*{0,2}BASIS\b|'
                   r'\bobserved\b.*\binferred\b|\bobserved\b.*\buncertain\b')
RECORD_IN = re.compile(r'(?im)^#{1,4}\s*record(ed)?\s*in\b|'
                       r'^\s*\*{0,2}RECORD[_ ]IN\b')
SUPERSEDES = re.compile(r'(?i)\bsupersed(?:e|es|ed|ing)\b')
# A hazard statement: the word near a ruling reference.
HAZARD = re.compile(r'(?i)\bhazards?\b')

# `docs/reviews/rulings/<question>.md` is the owned location; a ruling that lives
# elsewhere is not in the ledger.
def ruling_files() -> list[Path]:
    if not RULINGS.is_dir():
        return []
    return sorted(p for p in RULINGS.glob('*.md') if p.is_file())


def audit(path: Path) -> list[dict]:
    findings: list[dict] = []
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError as error:
        return [{'check': 'ruling_ledger', 'reason': 'unreadable',
                 'detail': f'{path.name}: {error}'}]
    relative = f'docs/reviews/rulings/{path.name}'

    if not REVERSED_BY.search(text):
        findings.append({
            'check': 'ruling_ledger', 'reason': 'no_reversal_condition',
            'detail': (f'{relative} does not state what would reverse it; '
                       f'`docs/agent-workflow.md` §3.3 makes that part of a '
                       f'technical-policy ruling, and without it the ruling can be '
                       f'revisited only on authority, never on evidence'),
        })
    if not BASIS.search(text):
        findings.append({
            'check': 'ruling_ledger', 'reason': 'no_basis',
            'detail': (f'{relative} does not state its basis in the '
                       f'observed/inferred/uncertain vocabulary (§2.4.1), so a '
                       f'reader cannot tell a measurement from a hypothesis'),
        })
    if not RECORD_IN.search(text):
        findings.append({
            'check': 'ruling_ledger', 'reason': 'no_owning_document',
            'detail': (f'{relative} does not name the document that owns the rule '
                       f'it states, so the rule is not findable from the topic'),
        })

    # W6's specific requirement.
    if SUPERSEDES.search(text) and not HAZARD.search(text):
        findings.append({
            'check': 'ruling_ledger', 'reason': 'supersedes_without_hazards',
            'detail': (f'{relative} supersedes an earlier ruling but names no hazard '
                       f'of it. W6 requires the hazards to be listed, because the '
                       f'A2h page-guard sequence was a hazard cited at 00:38, '
                       f'adopted at 09:32, and then failed on that same hazard'),
        })
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    files = ruling_files()
    findings: list[dict] = []
    for path in files:
        findings.extend(audit(path))

    record = {
        'checker': CHECKER_VERSION,
        'ledger': str(RULINGS),
        'rulings': [f.name for f in files],
        'required_facts': ['reversal condition', 'basis',
                           'owning document', 'superseded hazards'],
        'findings': findings,
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  rulings in the ledger : {len(files)}')
        for name in record['rulings']:
            print(f'    {name}')
        if findings:
            print(f'  {len(findings)} finding(s):')
            for finding in findings:
                print(f'    [{finding["reason"]}] {finding["detail"]}')
        else:
            print('  no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
