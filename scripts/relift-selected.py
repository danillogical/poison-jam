"""Relift existing functions affected by a reviewed instruction fix.

Only replaces function bodies in their existing chunks. No detection pass,
boundary changes, or full regeneration. Original XBE remains authoritative.
"""
from pathlib import Path
import json
import re
import sys

root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root.parent/'xboxrecomp'))
from tools.recomp import config
from tools.recomp.translator import FunctionTranslator
from tools.recomp.disasm import Disassembler

data=(root/'game/default.xbe').read_bytes()
config.configure_from_xbe(str(root/'game/default.xbe'))
db={}
for f in json.loads((root/'tools/disasm/output/functions.json').read_text()):
    a=int(f['start'],16);f['_addr']=a;f['end']=int(f['end'],16);db[a]=f
boundaries=json.loads((root/'config/boundary-fixes.json').read_text(encoding='utf-8'))
for fix in boundaries:
    a=int(fix['start'],16);db[a]['end']=int(fix['end'],16)
    db[a]['size']=db[a]['end']-a
labels={int(v['address'],16):v['name'] for v in json.loads((root/'tools/disasm/output/labels.json').read_text())}
# Preserve the compiled symbol inventory; the raw database predates CRT names.
for existing in (root/'src/recomp/gen').glob('recomp_*.c'):
    for m in re.finditer(r'Original: 0x([0-9A-F]+)(?:(?!\*/).)*\*/\nvoid (\w+)\(void\)',existing.read_text(),re.S):
        a=int(m[1],16)
        if a in db:db[a]['name']=m[2]
for a,name in json.loads((root/'config/manual-functions.json').read_text()).items():
    if int(a,16) in db:db[int(a,16)]['name']=name
t=FunctionTranslator(data,db,labels)
mode=sys.argv[1]
assert mode in ('atomics','wide-string-compare','boundaries','trace')
# A targeted guest-call trace.
#
# --trace-functions on a full regeneration rewrites every chunk, so a
# diagnostic that needs the entry/exit hooks for four functions costs a
# 100k-line diff and a tree nobody can review. Relifting only the named
# functions keeps the diff to those bodies, and the hooks are the same ones
# the full pass would emit. Addresses come from config/trace-functions.json
# so a run is reproducible from the repository alone.
trace_addresses=set()
if mode=='trace':
    for entry in json.loads((root/'config/trace-functions.json').read_text()):
        trace_addresses.add(int(entry,16) if isinstance(entry,str) else int(entry))
    if not trace_addresses:
        # Empty list = revert: regenerate the previously traced bodies with the
        # hooks off, so the tree goes back to what it was without a full pass.
        trace_addresses={int(e,16) for e in json.loads((root/'config/relift-trace.json').read_text())}
        if not trace_addresses: raise RuntimeError('nothing to revert')
    else:
        # Some traced targets are not entries in the raw split (the split missed
        # them), but they do have generated bodies. Take the span and the symbol
        # from the header already in the chunk: relifting must not move a
        # boundary, only replace the body.
        for path in (root/'src/recomp/gen').glob('recomp_*.c'):
            for m in re.finditer(r'/\*\*\n \* (\w+)\n \* Original: 0x([0-9A-F]+) - 0x([0-9A-F]+) \((\d+) bytes',path.read_text(encoding='utf-8')):
                name,start,end,size=m.group(1),int(m.group(2),16),int(m.group(3),16),int(m.group(4))
                if start in trace_addresses and start not in db:
                    db[start]={'_addr':start,'start':f'0x{start:08X}','end':end,'size':size,
                        'name':name,'confidence':1.0,'detection_method':'generated_body',
                        'has_prologue':False,'calls_to':[],'called_by':[]}
        t=FunctionTranslator(data,db,labels,trace_functions=trace_addresses)
    unknown=[f'0x{a:08X}' for a in sorted(trace_addresses) if a not in db]
    if unknown: raise RuntimeError('not in the function database: '+', '.join(unknown))
selected=[]
for path in (root/'src/recomp/gen').glob('recomp_*.c'):
    source=path.read_text();edits=[]
    for match in re.finditer(r'/\*\*(?:(?!\*/).)*?Original: 0x([0-9A-F]+)(?:(?!\*/).)*\*/\nvoid (\w+)\(void\)\n\{',source,re.S):
        # Anchor each header separately; the pattern must not span another header.
        header=match.group(0)
        if header.count('/**')!=1:raise RuntimeError('Header matcher crossed a function')
        a=int(match[1],16)
        end=source.find('\n/**',match.end());end=len(source) if end<0 else end
        old=source[match.start():end]
        wanted=('/* lock xadd */' in old or '/* lock cmpxchg */' in old or
                '/* xadd */' in old or '/* cmpxchg */' in old) if mode=='atomics' else bool(re.search(r'/\* rep\w* (?:cmps|scas)[wd] \*/',old))
        if mode=='boundaries':wanted=any(int(f['start'],16)==a for f in boundaries)
        if mode=='trace':wanted=a in trace_addresses
        if not wanted:continue
        generated=t.translate_function(a,db[a])
        if not generated or '/* TODO:' in generated:raise RuntimeError(f'Incomplete relift {a:08X}')
        assert re.search(r'void '+re.escape(match[2])+r'\(void\)',generated),f'Symbol changed at {a:08X}'
        edits.append((match.start(),end,generated+'\n'))
        selected.append(f'0x{a:08X}')
    for start,end,new in reversed(edits):source=source[:start]+new+source[end:]
    if edits:path.write_text(source)
(root/'config'/f'relift-{mode}.json').write_text(json.dumps(selected,indent=2)+'\n')
print(f'Relifted {len(selected)} {mode} functions')
