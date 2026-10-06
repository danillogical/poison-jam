"""A recovered body must own every branch target inside its own span.

0x0007B8D0 was recovered with a span ending at 0x7B93C, a label inside the
method. The translator turned each jump to 0x7B93C/0x7B956/0x7B978/0x7BBF3 into
a call to a fatal stub (config/recovery-unresolved.json), and the run died at
0x7BBF3. The mechanical invariant: no body's generated code calls an
unresolved stub whose address lies inside that body's own span.
"""
import json, re, sys, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def self_span_stub_calls(text, entries, unresolved):
    bodies = [(m.start(), int(m[1], 16))
              for m in re.finditer(r'\n(?:static )?void body_([0-9A-F]{8})\(void\)', text)]
    bodies.append((len(text), 0))
    bad = []
    for (s, a), (n, _) in zip(bodies, bodies[1:]):
        if a not in entries:
            continue
        for m in re.finditer(r'g_seh_ebp = ebp; sub_([0-9A-F]{8})\(\); return;', text[s:n]):
            x = int(m[1], 16)
            if x in unresolved and a < x < entries[a]:
                bad.append((a, x))
    return bad


import bisect

# Bodies that still call an unresolved stub inside or just past their span
# (up to the next function start, or 0x400 past the recorded end). Frozen on
# 2026-10-05: open recovery-boundary defects, NOT audited. The test fails if a
# body NOT in this set starts doing so; the set should only shrink. Fix one by
# correcting its span in config/recovered-functions.json.
#
# SHRUNK 2026-10-05 by 14 entries, all fixed by widening the parent span to own
# its own release arm / shared epilogue (a gap_prologue false entry that reads
# esi before writing it and has no rel32 reference from anywhere in the XBE):
# 7AB40, 864E0, AE9D0, F7540, FC7B0, FCB40, FCE20, 101DC0, 104500, 104990,
# 10F960, 1101E0, plus B0210 and BB7B0 fixed earlier the same session. The batch
# of 12 resolved 65 trap call sites and dropped scripts/check-span-exits.py
# findings 428 -> 363. The fixed addresses are removed here so they cannot
# silently regress: 80 -> 66, which then equalled the measured set exactly.
#
# SHRUNK AGAIN 2026-10-06 by 6 entries, all repaired as under-wide spans:
# 0xAE560 (end -> 0xAE659, its shared epilogue), 0xB06E0 (end -> 0xB09E0),
# 0x48DB0 (end -> 0x496B7), 0x39410, 0xAC0B0 and 0xB3970. 66 -> 60, which again
# equals the measured set exactly -- and `test_no_stale_known_open_members`
# below now FAILS if that stops being true. The subtraction alone could not see
# a stale member: `bad - KNOWN_OPEN` is empty whether the list is exact or
# bloated, so six repaired addresses were sitting in it, and a regression on any
# of them would have been silently accepted.
#
# SHRUNK AGAIN 2026-10-06 by 29 entries, all repaired by the F7c certified
# under-wide batch (152 spans widened to the body's real end, each certified by a
# fully enumerated walk whose every exit is one `ret N` at depth 0). 60 -> 31,
# again exactly the measured set. The 8 candidates that batch SKIPPED are still
# here on purpose: seven have a `stack_args` that disagrees with their certified
# N (a separate finding) and one would have swallowed 0x44000.
#
# SHRUNK AGAIN 2026-10-06 by 6 entries, all repaired by F7d: the seven the F7c
# batch skipped for a `stack_args`/N disagreement, of which 0x202A0, 0x20420,
# 0x204D0, 0x34070, 0x38460, 0x705E0 and 0x171B50 each had BOTH an extent error
# and a hidden ABI error (the 0x7DA30/0x152BC0 shape: a truncated body hiding a
# `ret` immediate), plus 0x96560 recovered as stop 26. 31 -> 25, again exactly
# the measured set.
KNOWN_OPEN = {int(x, 16) for x in """11105 1F000 20760 2A000 4037C 44000 45DB0 52050 73C20 91830 A76E0 AEE80 C7D40 D02D0 E0710 E0CF0 EC0B0 F02F0 10A0E0 110B20 11FE90 127810 142400 1783D0 180038""".split()}


def boundary_stub_calls(text, entries, unresolved):
    """Bodies with a stub call to an unresolved address between the body start
    and min(next function start, recorded end + 0x400): the label is inside the
    span, or just beyond a span that ended too early."""
    starts = sorted(entries)
    bodies = [(m.start(), int(m[1], 16))
              for m in re.finditer(r'\n(?:static )?void body_([0-9A-F]{8})\(void\)', text)]
    bodies.append((len(text), 0))
    bad = set()
    for (s, a), (n, _) in zip(bodies, bodies[1:]):
        if a not in entries:
            continue
        i = bisect.bisect_right(starts, a)
        nxt = starts[i] if i < len(starts) else 1 << 32
        for m in re.finditer(r'g_seh_ebp = ebp; sub_([0-9A-F]{8})\(\); return;', text[s:n]):
            x = int(m[1], 16)
            if x in unresolved and a < x < min(nxt, entries[a] + 0x400):
                bad.add(a)
    return bad


class SpanOwnershipTest(unittest.TestCase):
    def setUp(self):
        rec = ROOT / 'src/recomp/recovered/recovered.c'
        if not rec.is_file():
            self.skipTest('recovered.c not generated')
        self.text = rec.read_text(encoding='utf-8')
        self.entries = {int(e['start'], 16): int(e['end'], 16) for e in
                        json.loads((ROOT / 'config/recovered-functions.json').read_text(encoding='utf-8'))}
        self.unresolved = {int(k, 16) for k in json.loads(
            (ROOT / 'config/recovery-unresolved.json').read_text())}

    def test_no_body_calls_a_stub_inside_its_own_span(self):
        self.assertEqual(self_span_stub_calls(self.text, self.entries, self.unresolved), [])

    def test_no_new_boundary_defects(self):
        bad = boundary_stub_calls(self.text, self.entries, self.unresolved)
        self.assertEqual(sorted(hex(a) for a in bad - KNOWN_OPEN), [])
        self.assertNotIn(0x7B8D0, bad)

    def test_no_stale_known_open_members(self):
        """`KNOWN_OPEN` must equal the measured set, not merely contain it.

        The subtraction `bad - KNOWN_OPEN` is empty whether the list is exact or
        bloated, so a repaired address left in it is invisible -- and a
        *regression* on that address would then be silently accepted. Six were
        sitting in it on 2026-10-06 (`0x39410`, `0x488B0`, `0xAC0B0`, `0xAE560`,
        `0xB06E0`, `0xB3970`). This assertion is what makes the set exact rather
        than a growing allowance.
        """
        bad = boundary_stub_calls(self.text, self.entries, self.unresolved)
        stale = sorted(hex(a) for a in KNOWN_OPEN - bad)
        self.assertEqual(
            stale, [],
            f'{len(stale)} KNOWN_OPEN member(s) are no longer flagged and must be '
            f'removed, or a regression on them cannot be seen: {stale}')

    def test_the_7b8d0_labels_are_owned(self):
        self.assertEqual(self.entries[0x7B8D0], 0x7BC04)
        for label in ('0007B93C', '0007B956', '0007B978', '0007BBF3'):
            self.assertIn(f'loc_{label}: ;', self.text)
        for x in (0x7B956, 0x7B978, 0x7BBF3):
            self.assertNotIn(x, self.unresolved)

    def test_4c400_is_a_recovered_entry_not_a_stub(self):
        self.assertEqual(self.entries[0x4C400], 0x4C8A1)
        self.assertNotIn(0x4C400, self.unresolved)

    def test_handler_table_methods_own_their_spans(self):
        # Handler table 0x1F9888 methods: each starts its own entry and none of
        # the folded neighbours still covers another's start.
        for start, end in ((0x4BF40,0x4BF67),(0x4BF70,0x4C070),(0x52150,0x521A5),(0x521B0,0x52343),
                           (0x52350,0x5245A),(0x52780,0x52948),(0x52FC0,0x5309D),(0x530A0,0x53262)):
            self.assertEqual(self.entries[start], end, hex(start))
        for s in (0x4BF70, 0x521B0, 0x52350, 0x530A0):
            covering = [hex(a) for a, b in self.entries.items() if a < s < b]
            self.assertEqual(covering, [], hex(s))

    def test_425d0_vtable_method_is_recovered(self):
        self.assertEqual(self.entries[0x425D0], 0x42745)
        self.assertNotIn(0x425D0, self.unresolved)

    def test_thunk_contract_and_receivers(self):
        # tail-jump thunks pop what their slot-1/3 receivers pop (ret 0xC)
        for a in (0x47470, 0x47540):
            e = [x for x in json.loads((ROOT / 'config/recovered-functions.json').read_text(encoding='utf-8')) if int(x['start'], 16) == a][0]
            self.assertEqual(e['stack_args'], 12, hex(a))
        for a, b in ((0x6EC80,0x6EE8A),(0x3E210,0x3E22E),(0x3E230,0x3E3B6),(0x41400,0x41554),(0x69F90,0x6A0FD)):
            self.assertEqual(self.entries[a], b, hex(a))

    def test_1403b0_owns_the_tail_through_the_next_prologue(self):
        # f8 (20261004-183625-666) aborted here: the reviewed end 0x140485 was
        # `mov [esi+0x20], ecx`, mid-function, so the body fell off with esi,
        # ebp and edi still pushed. 0x14048E is the internal `call 0x1437a0`.
        self.assertEqual(self.entries[0x1403B0], 0x140540)
        self.assertNotIn(0x14048E, self.unresolved)
        self.assertIn('loc_0014048E: ;', self.text)
        self.assertNotIn('sub_0014048E', self.text)
        self.assertIn('Original: 0x001403B0 - 0x00140540', self.text)

    def test_negative_control_old_short_span_is_caught(self):
        # The real old state: the span ended at 0x7B93C (labels beyond it) and
        # the jumps were calls to fatal stubs. The old entry must be flagged.
        entries = dict(self.entries); entries[0x7B8D0] = 0x7B93C
        un = set(self.unresolved) | {0x7B956, 0x7B978, 0x7BBF3}
        old = self.text.replace('goto loc_0007BBF3;', 'g_seh_ebp = ebp; sub_0007BBF3(); return;')
        self.assertIn(0x7B8D0, boundary_stub_calls(old, entries, un))
        self.assertNotIn(0x7B8D0, boundary_stub_calls(self.text, self.entries, self.unresolved))


if __name__ == '__main__':
    unittest.main()
