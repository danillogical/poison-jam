"""Read the recorded model route for each subagent of this session.

`deepseek-harness.md` is explicit that a route is verified by reading the session
log, never by asking the model.  The per-session projection cache holds
`modelSelection.lastUsed`, which records what DSH **requested**; the transcript's
`subagent/descriptor` records the same at spawn.  This prints the projection view
for every session file whose id matches a child of this session.
"""
import json
import sys
from pathlib import Path

cache = Path.home() / '.dsh' / 'storages' / 'session_projcache' / 'sessions'

WANT = {
    '9a029485-6202-4ac2-bac6-cf11caf2a44b': 'child 1 — read the A2f run log (context isolation)',
    '1415063e-fa7d-4c0a-8e89-f827d1a43fba': 'child 2 — independent A2f+A2g review (acceptance gate)',
}

print('%-38s %-34s %-6s %s' % ('session', 'provider/model', 'effort', 'role'))
print('-' * 120)
for path in sorted(cache.glob('*.json')):
    sid = path.stem.replace('session-', '')
    try:
        rec = json.loads(path.read_text(encoding='utf-8', errors='replace'))
    except Exception as exc:
        print('%-38s UNREADABLE %s' % (sid, exc))
        continue
    rows = rec.get('record', {}).get('rows', {})
    sel = (rows.get('modelSelection') or {}).get('val') or {}
    last = sel.get('lastUsed') or {}
    label = ((rows.get('subagent') or {}).get('val') or {}).get('identity', {}).get('label')
    title = ((rows.get('title') or {}).get('val'))
    mode = ((rows.get('subagent') or {}).get('val') or {}).get('identity', {}).get('mode')
    role = WANT.get(sid) or (label or title or '')
    if not (last or label or title):
        continue
    print('%-38s %-34s %-6s %s' % (
        sid,
        '%s/%s' % (last.get('provider', '?'), last.get('model', '?')),
        last.get('reasoningEffort', '?'),
        role if not mode else '%s [%s]' % (role, mode)))
