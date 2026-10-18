"""Re-record the generated-code preservation baseline after a reviewed regeneration.

Every protected file whose digest changed (by check-generation-provenance.py's own digest)
gets its new hash in docs/reviews/p0-full-generated-baseline.json, with one dated entry
naming the files and the reason. Run it after check-generation-provenance.py --write,
whenever a recovery, relift or patch changes generated sources.

Usage:  python -X utf8 scripts/update-generated-baseline.py "<reason>" <YYYY-MM-DD>
"""
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) != 3:
    sys.exit(__doc__.strip().splitlines()[-1])
spec = importlib.util.spec_from_file_location('gp', ROOT / 'scripts' / 'check-generation-provenance.py')
gp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gp)
path = gp.BASELINE_PATH
raw = path.read_bytes()
nl = '\r\n' if b'\r\n' in raw else '\n'
base = json.loads(raw.decode('utf-8'))
changed = []
for name, old in base['files'].items():
    new = gp._digest(gp.ROOT / name)
    if new != old:
        base['files'][name] = new
        changed.append(name)
if not changed:
    sys.exit('no protected file changed; nothing to record')
base['updates'].append({'date': sys.argv[2], 'files': changed, 'reason': sys.argv[1]})
path.write_bytes((json.dumps(base, indent=1) + '\n').replace('\n', nl).encode('utf-8'))
print('updated', len(changed), 'file(s):', changed)
