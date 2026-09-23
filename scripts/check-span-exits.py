"""Find reviewed recovery entries whose span cuts a branch target.

`config/recovered-functions.json` stores an exclusive `end`, and the translator
emits exactly `end - start` bytes.  `scripts/check-entry-extents.py` catches the
case where `end` lands *inside* an instruction.  This catches the other half of
the same defect: `end` lands on a clean instruction boundary, but a branch
*inside* the span targets an address at or past `end`, so the span is missing
code that its own control flow reaches.

When that happens the generator sees a branch to an address no entry covers, so
it emits a trap stub for it, and at run time the trap fires:

    [ICALL] Failed to resolve VA 0x00141830

which is how this class was found.  `0x00141810` was entered with
`end 0x00141822`, and its `jne 0x141830` at `0x00141820` targets an internal
label 14 bytes past the end.  The evidence field even says why -- "end tightened
to the next function entry inside the alias range" -- and that tightening was
wrong: the sibling entries in the same function-pointer table all use
"end = the next function's start", which is the convention that covers trailing
padding and any internal label.

A branch target at or past `end` is NOT a defect when it is a known entry start:
that is a tail call, and the sibling spans end exactly at the next entry start
for that reason.  Only targets that are neither inside the span nor a known
entry start are reported.

**The entry test has to mean "its own entry", and that cost this script its
third miss (A2g, 2026-09-22).**  `entry_starts()` unions `runtime_starts()`,
which answers "can the runtime resolve this address" -- and a `tail_jump_alias`
folded into its parent by the `ff4d442` abutting-alias rule resolves *fine*,
because the dispatch tuple names the parent's symbol:

    { 0x0003060Eu, (recomp_func_t)sub_00030570 },

So `0x000304F0`'s own `ja 0x3060E` was skipped as "a tail call to a real entry"
when `0x3060E` is not an entry at all -- entering it runs `sub_00030570` from its
first byte.  All three known instances of this class were found by a *run* and
none by this script, and this is why.  The rule is now `genuine_starts()`: an
address counts only when the symbol answering it is its own.  Findings go
283 -> 446; the three known-true cases are asserted present below.

The same defect points the other way as well, and the first version of this
script only looked forward.  `0x00118610` was entered with `end 0x00118630`
and its last instruction is `jmp 0x1185b0` at `0x00118622` -- a tail call
*below* the span start, to a function the disassembler's database also missed
(the previous body it knows about ends `0x001185AA`, six padding bytes short of
`0x001185B0`).  Because the check was `target >= end`, it was skipped: 381
findings, and none of them this one.  The run found it instead:

    [ICALL] Failed to resolve VA 0x001185B0

so the rule is now "outside the span, in either direction", and the report says
which way.
"""
from pathlib import Path
import argparse
import json
import sys

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))
from resolution_starts import genuine_starts, runtime_starts  # noqa: E402

root = Path(__file__).resolve().parents[1]
xbe_path = root / 'game' / 'default.xbe'
sections = json.loads((root / 'game' / 'mygame_analysis.json').read_text())['sections']
entries = json.loads((root / 'config' / 'recovered-functions.json').read_text())
xbe = xbe_path.open('rb')

MAX_INSN = 15
# Deliberately no `call`: a call goes to a separate function, so its target being
# outside the span is the normal case, and including it buried the real findings
# under ten thousand of them on the first run of this script. `jmp` is included
# because an unconditional jump to a non-entry address is a tail call into a
# hole; the conditional jumps are the ones this class is about.
BRANCH_MNEMONICS = {'jmp', 'jmpq', 'ljmp'}
COND_PREFIX = 'j'


def section_for(va):
    for section in sections:
        base = int(section['virtual_addr'], 16)
        if base <= va < base + section['raw_size']:
            return section
    return None


def bytes_at(va, size):
    section = section_for(va)
    if section is None:
        return None
    offset = int(section['raw_addr'], 16) + (va - int(section['virtual_addr'], 16))
    xbe.seek(offset)
    return xbe.read(size)


def entry_starts():
    """Every address that is legitimately a function entry *in its own right*.

    The manifest alone is not enough. A first version of this script used only
    `recovered-functions.json`, and 4516 of its 746 remaining findings were
    forward jumps to real function starts the manifest does not list -- exactly
    the mistake `fix-fragment-spans.py` made in the previous session, which had
    to union the disassembler's own database for the same reason. A jump to a
    function start is a tail call, not a cut.

    The authoritative set is what the binary actually resolves, and that is
    `scripts/resolution_starts.py`: the generated `jsrf_lookup_recovered`
    switch, `manual-functions.json`, and the generated dispatch table. The
    disassembler's database and the boundary list are kept as well because they
    are the sources the reviewed spans were built from, and a start that only
    one of them knows is still not a hole.

    **`genuine_starts()` and not `runtime_starts()`, since A2g.** The latter
    answers "can the runtime resolve this", which is true of a folded alias
    too -- see the module docstring. Only an address whose *own* symbol answers
    it is an entry, because entering a folded alias runs its parent's body from
    a byte that is not that body's start.
    """
    starts = set(genuine_starts(root))
    for entry in entries:
        starts.add(int(entry['start'], 16))
    database = root / 'tools' / 'disasm' / 'output' / 'functions.recovered.json'
    if database.exists():
        for item in json.loads(database.read_text()):
            if isinstance(item, dict) and 'start' in item:
                starts.add(int(item['start'], 16))
    for name in ('manual-functions.json', 'boundary-fixes.json'):
        path = root / 'config' / name
        if not path.exists():
            continue
        data = json.loads(path.read_text())
        if isinstance(data, list):
            for item in data:
                if isinstance(item, dict) and 'start' in item:
                    starts.add(int(item['start'], 16))
        elif isinstance(data, dict):
            for key in data:
                try:
                    starts.add(int(key, 16))
                except ValueError:
                    pass
    return starts


# Instances of this class that are known-true, each confirmed by a bounded run
# and fixed by widening the parent's span.  All three were originally found by a
# *run* rather than by this script, so each one is a positive control: run with
# `--selfcheck`, the detector's own decision procedure is applied to the
# **pre-fix** span and must report the known cut target.  A rule change that
# silently loses this sensitivity is the failure mode this guards, and it has
# happened once already (the `runtime_starts` entry test, which cost A2g).
KNOWN_TRUE = [
    {
        'entry': '0x00025040', 'prefix_end': '0x00025233',
        'target': '0x000252B5',
        'what': 'A2e: the switch epilogue, cut by a gap_prologue false entry',
    },
    {
        'entry': '0x0007E180', 'prefix_end': '0x0007E242',
        'target': '0x0007E255',
        'what': 'A2f: the out-of-line tail merge, cut by a false entry',
    },
    {
        'entry': '0x000304F0', 'prefix_end': '0x00030508',
        'target': '0x0003060E',
        'what': "A2g: the switch's early-out epilogue, cut at the dispatch insn",
    },
]


def branch_targets(start, end, md):
    """Direct branch targets reached by instructions inside the span."""
    targets = []
    va = start
    while va < end:
        raw = bytes_at(va, min(MAX_INSN, end - va))
        if not raw:
            break
        insns = list(md.disasm(raw, va))
        if not insns:
            break
        insn = insns[0]
        size = insn.size
        if va + size > end:
            break                      # cut instruction; check-entry-extents.py's job
        mnemonic = insn.mnemonic
        is_branch = mnemonic in BRANCH_MNEMONICS or (
            mnemonic.startswith(COND_PREFIX) and len(mnemonic) > 1)
        if is_branch and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
            targets.append((va, mnemonic, insn.operands[0].imm & 0xFFFFFFFF))
        va += size
    return targets


def selfcheck(md):
    """Positive control: would this rule have caught the three known cases?

    Each case is replayed at its **pre-fix** span, because the real manifest now
    carries the corrected end and the defect is no longer present to find.  The
    question is whether the detector's decision procedure reports the known cut
    target given the span that was actually declared at the time -- which is the
    only form in which the miss can be reproduced.

    Returns 0 if every case is reported, 1 otherwise.
    """
    starts = entry_starts()
    failures = 0
    print('self-check: the detector against the three known-true cases, '
          'at their pre-fix spans')
    for case in KNOWN_TRUE:
        start = int(case['entry'], 16)
        end = int(case['prefix_end'], 16)
        target = int(case['target'], 16)
        reported = []
        for site, mnemonic, tgt in branch_targets(start, end, md):
            if start <= tgt < end or tgt in starts:
                continue
            reported.append(tgt)
        ok = target in reported
        failures += 0 if ok else 1
        print('  %-5s %s  span %s..%s  target %s  %s'
              % ('PASS' if ok else 'FAIL', case['entry'], case['entry'],
                 case['prefix_end'], case['target'], case['what']))
        if not ok:
            print('        reported instead: %s'
                  % (', '.join('0x%08X' % t for t in reported) or '(nothing)'))
    if failures:
        print('self-check FAILED: %d of %d known cases are not detectable; the '
              'entry test or the branch rule has lost sensitivity.'
              % (failures, len(KNOWN_TRUE)))
        return 1
    print('self-check passed: all %d known cases are detectable at their '
          'pre-fix spans.' % len(KNOWN_TRUE))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--json', action='store_true', help='emit machine-readable output')
    parser.add_argument('--limit', type=int, default=40)
    parser.add_argument('--selfcheck', action='store_true',
                        help='positive control: verify the rule still detects the '
                             'three known-true cases at their pre-fix spans')
    args = parser.parse_args()

    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    if args.selfcheck:
        return selfcheck(md)

    starts = entry_starts()

    findings = []
    for entry in entries:
        start = int(entry['start'], 16)
        end = int(entry['end'], 16)
        if end <= start:
            continue
        for site, mnemonic, target in branch_targets(start, end, md):
            if start <= target < end:
                continue               # inside the span, or a backward head jump
            if target in starts:
                continue               # a tail call to a real entry
            findings.append({
                'start': entry['start'],
                'end': entry['end'],
                'site': '0x%08X' % site,
                'mnemonic': mnemonic,
                'target': '0x%08X' % target,
                'gap': target - end if target >= end else start - target,
                'direction': 'forward' if target >= end else 'backward',
            })

    if args.json:
        print(json.dumps(findings, indent=1))
        return 0

    print('entries checked: %d' % len(entries))
    forward = [f for f in findings if f['direction'] == 'forward']
    backward = [f for f in findings if f['direction'] == 'backward']
    print('CUT-TARGET findings: %d (%d forward, %d backward)'
          % (len(findings), len(forward), len(backward)))
    for finding in findings[:args.limit]:
        print('  %s end %s  %s %s -> %s  (%s, %+d)'
              % (finding['start'], finding['end'], finding['site'],
                 finding['mnemonic'], finding['target'], finding['direction'],
                 finding['gap']))
    if len(findings) > args.limit:
        print('  ... %d more' % (len(findings) - args.limit))
    return 0


if __name__ == '__main__':
    sys.exit(main())
