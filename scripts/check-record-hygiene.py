"""`scripts/check-record-hygiene.py`: W4's and W12's record lint.

Two rows, one lint, because both are about what a durable record may contain.

**W4** asks that "record hygiene is a lint that must pass before review", answering
"churn on prose (A3a r12-r24; OOM slice 5 rounds on sweeps)". The mechanism: a
review record carries BLOCKING findings and a disposition, and nothing else that a
later round can argue with. Prose that is neither a criterion disposition nor a
recorded decision is what the rounds were spent on.

**W12** asks that "Session 'verification' prose is replaced by script JSON; review
records keep rulings and packets, which are the only records later work reused",
answering "31 of 36 session verification records never cited (38,972 words)".

**What the lint checks**, each with a reason it is mechanical rather than a taste
judgement:

  1. **A disposition is one of the two allowed values.** `ACCEPT` or
     `NOT ACCEPTED`; anything else (`PARTIAL`, `MOSTLY`, `PASS WITH NOTES`) is
     unclassifiable by a reader and by the plan's own decision rows.
  2. **A finding is classified.** Every finding line says BLOCKING or advisory, so
     a reader does not have to infer which it is -- and so an advisory cannot be
     argued into blocking later.
  3. **No session-verification prose block.** A section titled like a session's own
     "verification" is the W12 pattern: narrative that no later work cites.
  4. **A cited artifact where the record makes a claim.** A record that asserts a
     measured result names a path or a command.

**What it deliberately does not check**: whether the findings are *correct*, whether
the review is *good*, or how long the prose is. A word-count limit would be a taste
rule wearing a number, and the W12 measurement is about citation, not length.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER_VERSION = 'jsrf-record-hygiene/1'

# Records this lint governs: review records and rulings, not the plan or the TR.
RECORD_GLOBS = ('docs/reviews/*.md', 'docs/reviews/**/*.md',
                'docs/reviews/*.json', 'docs/reviews/**/*.json')

# Directories whose contents are data, not records.
SKIP_NAMES = {'startup-current.md', 'strict-horizon-ledger.md',
              'review-record.schema.json', 't5-analyzer-baseline.json'}

# Records written before W12 was adopted.  W12 landed on 2026-09-30 and governs
# records written from then on; two older records legitimately carry a
# "## Verification" section, and rewriting a historical record to satisfy a rule
# adopted afterwards would destroy the record rather than improve it.
#
# The scope is a **file list, not a date rule**, because a record's own date is not
# in its filename and a date rule would need a source of truth the lint does not
# have.  Naming the files makes the exemption visible and reviewable; a new record
# is governed by default.
PRE_W12_RECORDS = {
    'docs/reviews/p0-1-vblank-adjudication.md',
    'docs/reviews/p0-2-implementation-review-round1.md',
}

ALLOWED_DISPOSITIONS = {'ACCEPT', 'NOT ACCEPTED'}
DISPOSITION = re.compile(
    r'(?im)^\s*(?:\*\*)?(?:overall\s+)?(?:disposition|verdict)(?:\*\*)?\s*:?\s*'
    r'[*`]*([A-Za-z ]+?)[*`]*\s*$')

# A finding line, in the forms this project's records use.
FINDING = re.compile(r'(?im)^\s*(?:[-*]\s*)?(?:\[[^\]]+\]\s*)?'
                     r'(?:BLOCKING|ADVISORY|Advisory|Blocking)\b')
CLASSIFIED = re.compile(r'(?i)\b(BLOCKING|advisory|deferred|non-?blocking)\b')

# A section heading that introduces session-verification prose (W12's pattern).
VERIFICATION_HEADING = re.compile(
    r'(?im)^#{1,4}\s*(?:session\s+)?verification\b.*$')

# Evidence that the record points at something a later reader can open.
#
# The path prefixes cover every tree a record can cite: `docs/`, `scripts/`,
# `tools/`, `logs/`, and -- measured by a control -- `src/` and `config/`, which a
# marker-inventory record legitimately cites. A regex that covered only the first
# four reported `p0-abi-marker-inventory.json` as uncited while it names
# `src/recomp/gen/recomp_types.h` on every entry.
CITATION = re.compile(
    r'(?i)(docs/[\w./-]+\.(?:md|json)|scripts/[\w./-]+\.py|tools/[\w./-]+'
    r'|logs/[\w./-]+|src/[\w./-]+|config/[\w./-]+|tests/[\w./-]+'
    r'|[0-9a-f]{40}|`[^`]*--[a-z-]+[^`]*`)')

# W12's pattern: narrative that reports what a session did, rather than a ruling,
# a criterion disposition, or a decision.  Matched on the opening phrase so a
# finding that happens to contain the word is not caught.
NARRATIVE_OPENER = re.compile(
    r'(?im)^\s*(?:I|We)\s+(?:verified|checked|confirmed|ran|inspected|reviewed)\b')


def record_files() -> list[Path]:
    files: list[Path] = []
    seen: set[Path] = set()
    for pattern in RECORD_GLOBS:
        for path in sorted(ROOT.glob(pattern)):
            if not path.is_file() or path in seen or path.name in SKIP_NAMES:
                continue
            seen.add(path)
            files.append(path)
    return files


def audit(path: Path) -> list[dict]:
    findings: list[dict] = []
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError as error:
        return [{'check': 'record_hygiene', 'reason': 'unreadable',
                 'detail': f'{path.name}: {error}'}]
    relative = str(path.relative_to(ROOT)).replace('\\', '/')
    lines = text.splitlines()

    # 1. A disposition, when the record states one, must be one of the two values.
    for match in DISPOSITION.finditer(text):
        value = match.group(1).strip()
        if not value:
            continue
        if value.upper() not in ALLOWED_DISPOSITIONS:
            line = text[:match.start()].count('\n') + 1
            findings.append({
                'check': 'record_hygiene', 'reason': 'unclassifiable_disposition',
                'detail': (f'{relative}:{line} states disposition {value!r}; only '
                           f'ACCEPT and NOT ACCEPTED are classifiable'),
            })

    # 2. A finding line must say whether it is blocking or advisory.
    for number, line in enumerate(lines, 1):
        if not FINDING.match(line):
            continue
        if not CLASSIFIED.search(line):
            findings.append({
                'check': 'record_hygiene', 'reason': 'unclassified_finding',
                'detail': (f'{relative}:{number} is a finding line that does not say '
                           f'BLOCKING or advisory'),
                'line': line.strip()[:160],
            })

    # 3. W12's pattern: session-verification prose.  Not applied to records written
    # before W12 was adopted; see PRE_W12_RECORDS.
    if relative not in PRE_W12_RECORDS:
        for match in VERIFICATION_HEADING.finditer(text):
            line = text[:match.start()].count('\n') + 1
            findings.append({
                'check': 'record_hygiene', 'reason': 'session_verification_prose',
                'detail': (f'{relative}:{line} opens a session-verification section. '
                           f'W12 replaces session "verification" prose with script '
                           f'JSON; a record keeps rulings, criteria and decisions'),
            })

        # 4. First-person narrative that reports what a session did.
        for number, line in enumerate(lines, 1):
            if NARRATIVE_OPENER.match(line):
                findings.append({
                    'check': 'record_hygiene', 'reason': 'session_narrative',
                    'detail': (f'{relative}:{number} is first-person session '
                               f'narrative; W12 keeps script JSON instead'),
                    'line': line.strip()[:160],
                })

    # 5. A record that claims a measurement must cite something openable.
    claims = re.search(r'(?i)\b(?:measured|observed|verified|reproduced)\b', text)
    if claims and not CITATION.search(text):
        findings.append({
            'check': 'record_hygiene', 'reason': 'uncited_claim',
            'detail': (f'{relative} claims a measurement but cites no artifact, '
                       f'command, hash or path a reader could open'),
        })
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--record', action='append', default=[],
                        help='check only this record (repeatable)')
    args = parser.parse_args()

    if args.record:
        files = [Path(r) if Path(r).is_absolute() else ROOT / r for r in args.record]
    else:
        files = record_files()

    findings: list[dict] = []
    for path in files:
        findings.extend(audit(path))

    record = {
        'checker': CHECKER_VERSION,
        'records_examined': len(files),
        'rules': ['classifiable_disposition', 'classified_finding',
                  'no_session_verification_prose', 'no_session_narrative',
                  'cited_claim'],
        'findings': findings,
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  records examined : {len(files)}')
        if findings:
            print(f'  {len(findings)} finding(s):')
            for finding in findings:
                print(f'    [{finding["reason"]}] {finding["detail"]}')
        else:
            print('  no findings')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
