"""Reject stale executables and source edits during a build."""
import hashlib
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
toolkit=root.parent/'xboxrecomp'
output=root/'build/Release'
artifact_names=('jsrf_recomp.exe','jsrf_recomp.pdb','jsrf_recomp.map',
                'jsrf_collect.exe','jsrf_collect.pdb','jsrf_gpu_smoke.exe','jsrf_gpu_smoke.pdb')
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def sources():
    files=[root/'CMakeLists.txt',toolkit/'CMakeLists.txt']
    for folder in [root/'src',root/'config',root/'tests',root/'tools/harness',toolkit/'src',toolkit/'include']:
        files += [p for p in folder.rglob('*') if p.is_file() and p.suffix in ('.c','.h','.json','.txt','.cmake')]
    return {str(p.resolve()):digest(p) for p in sorted(set(files))}
mode=sys.argv[1]
pending=root/'build/source-pending.json';stamp=output/'build-source.json'
if mode=='before':pending.write_text(json.dumps(sources(),indent=2))
elif mode=='after':
    before=json.loads(pending.read_text());current=sources()
    if before!=current:raise SystemExit('Source changed during build; rebuild before running.')
    stamp.write_text(json.dumps({'sources':current,'exe_sha256':digest(output/'jsrf_recomp.exe'),
                                'artifacts':{name:digest(output/name) for name in artifact_names}},indent=2))
elif mode=='verify':
    if not stamp.exists():raise SystemExit('Missing build identity. Run scripts/build-jsrf.ps1 first.')
    built=json.loads(stamp.read_text());current=sources()
    changed=[p for p in set(built['sources'])|set(current) if built['sources'].get(p)!=current.get(p)]
    artifacts=built.get('artifacts',{})
    mismatched=[name for name in artifact_names if not (output/name).is_file() or artifacts.get(name)!=digest(output/name)]
    if changed or mismatched:
        raise SystemExit('Build/source/symbol mismatch. Run scripts/build-jsrf.ps1. Changed: '+', '.join((changed+mismatched)[:4]))
else:raise SystemExit('expected before/after/verify')
