"""Audit the commits about to be pushed: blob count, largest blob, and the secret scan.

Lists every object in origin/master..HEAD, reports the largest blob (GitHub refuses
files over 100 MB), and runs scripts/secret-audit.py over the outgoing blobs. Exits
non-zero if the secret audit does. Run it before every push of this public repository.

Usage:  python -X utf8 scripts/outgoing-audit.py [base-ref]   (default origin/master)
"""
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
base = sys.argv[1] if len(sys.argv) > 1 else 'origin/master'
objs = subprocess.run(['git', '-C', str(ROOT), 'rev-list', '--objects', base + '..HEAD'],
                      capture_output=True, text=True, check=True).stdout.split('\n')
shas = '\n'.join(l.split(' ')[0] for l in objs if l.strip())
info = subprocess.run(['git', '-C', str(ROOT), 'cat-file', '--batch-check'], input=shas + '\n',
                      capture_output=True, text=True, check=True).stdout.split('\n')
blobs = [l.split() for l in info if ' blob ' in l]
largest = max((int(b[2]) for b in blobs), default=0)
print('outgoing blobs:', len(blobs), 'largest bytes:', largest)
if largest > 100 * 1024 * 1024:
    sys.exit('a blob exceeds 100 MB; GitHub would refuse it')
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'blobs.txt'
    path.write_text('\n'.join(b[0] for b in blobs) + '\n')
    r = subprocess.run([sys.executable, '-X', 'utf8', str(ROOT / 'scripts' / 'secret-audit.py'), str(path)],
                       capture_output=True, text=True, cwd=ROOT)
print('\n'.join(l for l in r.stdout.splitlines() if 'TOTAL' in l or 'scanned' in l))
sys.exit(r.returncode)
