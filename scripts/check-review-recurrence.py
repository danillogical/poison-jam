"""`scripts/check-review-recurrence.py`: W1's repeated-criterion-ID check.

Plan W1: "Blockers carry `criterion-ID` and a category; **the same criterion ID
blocking two consecutive INADEQUATE verdicts forces redesign or a discovery
packet**; only the Advisor exempts", with the check being "`check-recorded-reviews.py`
flags a repeated ID".

**The failure it answers**, from the plan's own row: "patch-the-patch loops (A3a AC2
r6–r10, P0.2 AC2, A4b watch→ledger→`[GPIN]`, A4b2 AC-BOOT)". Those are revisions
that fixed a criterion, failed it again, fixed it again, and failed it again -- each
round looking locally reasonable and each round costing a senior call.

**Why this is a separate tool rather than a change to `check-recorded-reviews.py`.**
That checker validates ONE record against its contract. Recurrence is a property of a
**sequence** of records for one packet, which needs the revision history and a
notion of "consecutive". Keeping the two apart also means the per-record checker
keeps its single, well-understood job.

**What counts as consecutive.** Records are ordered by the revision number parsed
from the filename (`<packet>-r<N>-*.md` / `.json`). A criterion is a recurrence when
it appears as a blocking finding in two records whose revisions are adjacent, or
more strictly when the same criterion blocks again after a revision that was
supposed to fix it. The conservative reading is used: **two consecutive records**
that both block the same criterion ID.

**What it does not do.** It does not decide whether the redesign is warranted, does
not classify findings, and does not exempt anything -- "only the Advisor exempts"
(§2.3), so an exemption is recorded as a line naming the criterion and the Advisor
ruling, and this tool reads it rather than inventing one.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHECKER_VERSION = 'jsrf-review-recurrence/1'

# `<packet>-r<N>-<anything>.md|json`, and the `-r<N>` form used by some records.
REVISION_NAME = re.compile(r'^(?P<packet>.+?)-r(?P<revision>\d+)(?:[-.].*)?$')
# A criterion id: `AC-1`, `AC1`, `P0.2-AC1`, `AC-BOOT`, `AC2` -- the forms this
# project's packets actually use.
CRITERION = re.compile(r'\b((?:[A-Z]+\d*(?:\.\d+)?-)?AC[-_]?[A-Za-z0-9]+)\b')
# A finding is blocking unless the text says otherwise.
NON_BLOCKING = re.compile(r'(?i)\b(advisory|deferred|non-?blocking|not blocking)\b')

REVIEW_DIRS = ('docs/reviews',)


def revision_records() -> dict[str, list[dict]]:
    """Packet -> its records, ordered by revision number.

    Two sources, because this repository uses both:

      * `docs/reviews/<packet>-r<N>-*.{md,json}` -- the per-revision review
        records the workflow's ownership table names; and
      * `docs/reviews/<packet>-revision-history.md` -- the non-authoritative
        revision log, which is where the older packets' round-by-round findings
        actually live.  Measured: this repository has **no** `<packet>-r<N>-*`
        files on disk, so a scan that read only those examined zero packets and
        reported "no recurrence" -- a check that cannot fail.

    A history file is parsed into one pseudo-record per revision heading, because
    that is the granularity the recurrence rule needs.
    """
    packets: dict[str, list[dict]] = {}
    for directory in REVIEW_DIRS:
        root = ROOT / directory
        if not root.is_dir():
            continue
        for path in sorted(root.rglob('*')):
            if not path.is_file() or path.suffix not in ('.md', '.json'):
                continue
            match = REVISION_NAME.match(path.stem)
            if match:
                packets.setdefault(match.group('packet'), []).append({
                    'revision': int(match.group('revision')),
                    'path': path,
                    'name': path.name,
                    'text': None,
                })
                continue
            if path.stem.endswith('-revision-history'):
                packet = path.stem[:-len('-revision-history')]
                for entry in history_entries(path):
                    packets.setdefault(packet, []).append(entry)
    for records in packets.values():
        records.sort(key=lambda r: r['revision'])
    return packets


def history_entries(path: Path) -> list[dict]:
    """One entry per revision heading in a `-revision-history.md` file.

    Headings are read in the forms this project uses: `## r3`, `### r3`, `## R3`,
    and `## Revision 3`.  A file with no such heading yields one entry at revision
    0, so its content is still examined rather than silently dropped.
    """
    try:
        text = path.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return []
    headings = list(re.finditer(
        r'(?im)^#{2,4}\s*(?:revision\s*)?r?(\d+)\b.*$', text))
    if not headings:
        return [{'revision': 0, 'path': path, 'name': path.name, 'text': text}]
    entries = []
    for index, heading in enumerate(headings):
        start = heading.end()
        end = headings[index + 1].start() if index + 1 < len(headings) else len(text)
        entries.append({
            'revision': int(heading.group(1)),
            'path': path,
            'name': f'{path.name}#r{heading.group(1)}',
            'text': text[start:end],
        })
    return entries


def blocking_criteria(record: dict) -> tuple[set[str], list[str]]:
    """The criterion ids this record reports as BLOCKING, and its exemption lines.

    An exemption is a line that names a criterion AND cites an Advisor ruling, so
    a reader can trace it. Anything else is not an exemption.
    """
    if record.get('text') is not None:
        text = record['text']
    else:
        try:
            text = record['path'].read_text(encoding='utf-8', errors='replace')
        except OSError:
            return set(), []
    blocking: set[str] = set()
    exemptions: list[str] = []
    for line in text.splitlines():
        ids = CRITERION.findall(line)
        if not ids:
            continue
        lowered = line.casefold()
        if 'advisor' in lowered and ('exempt' in lowered or 'waiv' in lowered
                                     or 'ruling' in lowered):
            exemptions.append(line.strip()[:200])
            continue
        if NON_BLOCKING.search(line):
            continue
        # A finding line names a criterion and marks it blocking, or sits under a
        # BLOCKING heading.  Both are read: the heading is carried in `context`.
        if re.search(r'(?i)\bblocking\b|\bDISAGREED\b|\bNOT ACCEPTED\b', line):
            blocking.update(ids)
    # Also collect ids from a BLOCKING: section, which is how the template writes
    # them (`BLOCKING: <each: location; failure scenario; required outcome>`).
    for match in re.finditer(r'(?is)^\s*BLOCKING\s*:\s*(.+?)(?=^\s*[A-Z_]+\s*:|\Z)',
                             text, re.MULTILINE):
        body = match.group(1)
        for line in body.splitlines():
            if NON_BLOCKING.search(line):
                continue
            blocking.update(CRITERION.findall(line))
    return blocking, exemptions


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--packet', help='check only this packet')
    args = parser.parse_args()

    packets = revision_records()
    if args.packet:
        packets = {k: v for k, v in packets.items() if k == args.packet}

    findings: list[dict] = []
    examined = 0
    for packet, records in sorted(packets.items()):
        if len(records) < 2:
            continue
        examined += 1
        previous: dict | None = None
        previous_blocking: set[str] = set()
        exempted: set[str] = set()
        for record in records:
            blocking, exemptions = blocking_criteria(record)
            for line in exemptions:
                exempted.update(CRITERION.findall(line))
            if previous is not None and previous['revision'] == record['revision'] - 1:
                repeated = (previous_blocking & blocking) - exempted
                for criterion in sorted(repeated):
                    findings.append({
                        'check': 'review_recurrence',
                        'reason': 'same_criterion_blocked_twice_consecutively',
                        'detail': (f'{packet}: {criterion} blocks in both '
                                   f"r{previous['revision']} "
                                   f"({previous['name']}) and r{record['revision']} "
                                   f"({record['name']}); W1 requires redesign or a "
                                   f'discovery packet, and only the Advisor exempts'),
                        'packet': packet,
                        'criterion': criterion,
                        'revisions': [previous['revision'], record['revision']],
                    })
            previous = record
            previous_blocking = blocking

    record = {
        'checker': CHECKER_VERSION,
        'packets_with_two_or_more_records': examined,
        'findings': findings,
        'note': ('A repeated blocking criterion is not automatically a defect in the '
                 'revision; it is the signal W1 exists for -- two rounds spent on the '
                 'same criterion. An Advisor exemption is read from a line that names '
                 'the criterion and cites the ruling.'),
    }

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    else:
        print(f'checker {CHECKER_VERSION}')
        print(f'  packets examined : {examined}')
        if findings:
            print(f'  {len(findings)} recurrence(s):')
            for finding in findings:
                print(f'    [{finding["packet"]}] {finding["detail"]}')
        else:
            print('  no recurrence')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
