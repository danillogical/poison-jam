"""Inventory a generated tree: functions, dispatch, stubs, recovered bodies, duplicates, markers.

    python -X utf8 scripts/recomp-inventory.py src/recomp <out.json>

Writes the full inventory to <out.json> and prints a summary; compare two inventories to classify a
regeneration (docs/upstream-merge-playbook.md step 8).
"""
import json, re, sys
from pathlib import Path
root = Path(sys.argv[1])          # a src/recomp directory
gen, rec = root / 'gen', root / 'recovered'
inv = {}
funcs = {}                        # name -> {va, end, chunk}
dupe_gen = []
for chunk in sorted(gen.glob('recomp_0*.c')):
    text = chunk.read_text(encoding='utf-8', errors='replace')
    for m in re.finditer(r'Original: 0x([0-9A-F]+) - 0x([0-9A-F]+)(?:(?!\*/).)*\*/\r?\nvoid (\w+)\(void\)', text, re.S):
        name = m[3]
        if name in funcs: dupe_gen.append(name)
        funcs[name] = {'va': int(m[1], 16), 'end': int(m[2], 16), 'chunk': chunk.name}
inv['generated_functions'] = {k: [f"0x{v['va']:08X}", f"0x{v['end']:08X}", v['chunk']] for k, v in sorted(funcs.items())}
inv['generated_count'] = len(funcs)
inv['generated_internal_duplicates'] = dupe_gen
hdr = (gen / 'recomp_funcs.h').read_text(encoding='utf-8', errors='replace')
inv['funcs_h_declared'] = sorted(set(re.findall(r'^void (\w+)\(void\);', hdr, re.M)))
m = re.search(r'\*\s*(\d+) functions', hdr); inv['funcs_h_banner'] = int(m[1]) if m else None
disp = (gen / 'recomp_dispatch.c').read_text(encoding='utf-8', errors='replace')
table = re.findall(r'\{\s*0x([0-9A-Fa-f]{8})u,\s*\(recomp_func_t\)(\w+)\s*\}', disp)
inv['dispatch'] = {f'0x{va.upper()}': fn for va, fn in table}
inv['dispatch_count'] = len(table)
m = re.search(r'Maps (\d+) Xbox VAs', disp); inv['dispatch_banner'] = int(m[1]) if m else None
inv['alias_wrappers'] = re.findall(r'static void recomp_alias_([0-9A-F]{8})\(void\) \{ ([^}]*)\}', disp)
stubs = {}
for name in ('recomp_stubs_unresolved.c', 'recomp_stubs_recovery.c'):
    p = gen / name
    if p.is_file():
        t = p.read_text(encoding='utf-8', errors='replace')
        stubs[name] = sorted(set(re.findall(r'^void (sub_[0-9A-F]{8})\(void\)', t, re.M)))
    else:
        stubs[name] = None
inv['stubs'] = stubs
recd = []
for p in sorted(rec.glob('*.c')):
    if p.name.endswith('_test.c'): continue
    recd += re.findall(r'^void (\w+)\(void\)\s*\{', p.read_text(encoding='utf-8', errors='replace'), re.M)
inv['recovered_definitions'] = sorted(recd)
inv['recovered_count'] = len(recd)
inv['recovered_internal_duplicates'] = sorted({n for n in recd if recd.count(n) > 1})
inv['recovered_vs_generated_duplicates'] = sorted(set(recd) & set(funcs))
stub_names = set().union(*[set(v) for v in stubs.values() if v])
inv['stub_vs_definition_duplicates'] = sorted(stub_names & (set(recd) | set(funcs)))
markers = {}
for pat in ('RECOMP_BACKEDGE()', 'RECOMP_BACK_EDGE(', 'RECOMP_STUB_HIT(', 'recomp_icall_fail_log(', 'RECOMP_UNIMPL', 'RECOMP_ABI_CALL', 'JSRF_ABI_CONTINUE', 'g_seh_ebp'):
    markers[pat] = sum(p.read_text(encoding='utf-8', errors='replace').count(pat) for p in list(gen.glob('*.c')) + list(rec.glob('*.c')))
inv['markers'] = markers
ad = gen / 'recomp_abi_deltas.c'
inv['abi_delta_entries'] = len(re.findall(r'^\s*\{ 0x[0-9A-F]{8}u,', ad.read_text(), re.M)) if ad.is_file() else None
Path(sys.argv[2]).write_text(json.dumps(inv, indent=1))
summary = {k: v for k, v in inv.items() if not isinstance(v, (dict, list)) or k in ('markers', 'generated_internal_duplicates', 'recovered_internal_duplicates', 'recovered_vs_generated_duplicates', 'stub_vs_definition_duplicates')}
summary['stub_counts'] = {k: (len(v) if v is not None else None) for k, v in stubs.items()}
summary['funcs_h_declared_count'] = len(inv['funcs_h_declared'])
summary['alias_wrapper_count'] = len(inv['alias_wrappers'])
print(json.dumps(summary, indent=1))
