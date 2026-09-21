"""Archive the source present at launch (not a claim of a clean rebuild)."""
import json
from pathlib import Path
import subprocess
import sys
import zipfile

run=Path(sys.argv[1])
root=Path(__file__).resolve().parents[1]
meta=json.loads((run/'metadata.json').read_text(encoding='utf-8-sig'))
with zipfile.ZipFile(run/'source.zip','w',compression=zipfile.ZIP_DEFLATED,compresslevel=1) as archive:
    for entry in meta['sources']:
        archive.write(root/entry['path'],'project/'+entry['path'].replace('\\','/'))
    toolkit=(root/'../xboxrecomp').resolve()
    # Include changed tracked toolkit files AND new source headers absent from patches.
    tracked=subprocess.check_output(['git','-C',str(toolkit),'diff','--name-only','HEAD'],text=True).splitlines()
    untracked=subprocess.check_output(['git','-C',str(toolkit),'ls-files','--others','--exclude-standard'],text=True).splitlines()
    for name in sorted(set(tracked+untracked)):
        p=toolkit/name
        if p.is_file() and p.suffix in ('.c','.h','.py','.txt','.cmake'):
            archive.write(p,'toolkit/'+name)
