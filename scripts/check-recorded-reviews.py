"""Every subagent child of this session, its route, and whether its verdict is recorded.

`list_agents` shows only *current* children, so a review spawned earlier in this
session and already settled is invisible there.  The projection cache keeps every
session, including settled children, with its label, route and final response --
which is how a completed review can be found after the fact.

Cross-checking those verdicts against `report-deepseek.md` is the point: a review
that was run but never recorded is work that will be duplicated, and this session
has already done exactly that once (the A2f review had identified the 0x000304f0
defect that A2g later re-derived from scratch).
"""
import json
import re
from pathlib import Path

CACHE = Path.home() / '.dsh' / 'storages' / 'session_projcache' / 'sessions'
SESSION = '237565f1-5b30-45ff-b6df-058195974de8'
report = (Path(__file__).resolve().parents[1] / 'report-deepseek.md').read_text(
    encoding='utf-8', errors='replace')

rows = []
for path in sorted(CACHE.glob('*.json')):
    sid = path.stem.replace('session-', '')
    if sid == SESSION:
        continue
    try:
        rec = json.loads(path.read_text(encoding='utf-8', errors='replace'))
    except Exception:
        continue
    r = rec.get('record', {})
    rws = r.get('rows', {})
    ident = ((rws.get('subagent') or {}).get('val') or {}).get('identity')
    if not ident:
        continue                                  # not a child of any session
    label = ident.get('label') or '(no label)'
    mode = ident.get('mode')
    sel = ((rws.get('modelSelection') or {}).get('val') or {}).get('lastUsed') or {}
    turns = ((rws.get('turnOutline') or {}).get('val') or {}).get('turns') or []
    response = turns[0].get('response', '') if turns else ''
    created = r.get('identity', {}).get('createdAt')

    verdict = ''
    for pat in (r'Verdict:\s*\*{0,2}(ACCEPT|REJECT)', r'\b(ACCEPT|REJECT)\b'):
        m = re.search(pat, response)
        if m:
            verdict = m.group(1)
            break
    if not verdict:
        for pat in ('AGREED', 'DISAGREED', 'CANNOT VERIFY'):
            if pat in response:
                verdict = pat.lower()
                break

    recorded = label in report or (verdict and verdict in report)
    rows.append((created or 0, sid, label, '%s/%s' % (sel.get('provider'), sel.get('model')),
                 sel.get('reasoningEffort'), mode, verdict, recorded, len(response)))

rows.sort()
print('%-8s %-38s %-30s %-6s %-11s %-9s %-6s' %
      ('created', 'child', 'route', 'effort', 'mode', 'verdict', 'in rpt'))
print('-' * 130)
for created, sid, label, route, effort, mode, verdict, recorded, n in rows:
    print('%-8s %-38s %-30s %-6s %-11s %-9s %-6s' %
          (str(created)[-6:], sid[:36], route, effort or '?', mode or '?',
           verdict or ('(%d ch)' % n), 'yes' if recorded else 'NO'))
print()
print('total children: %d ; verdicts not found in the report: %d'
      % (len(rows), sum(1 for r in rows if not r[7])))
