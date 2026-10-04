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

    def test_negative_control_old_short_span_is_caught(self):
        # Re-create the old state: span ends at 0x7B93C and the labels are stubs.
        entries = dict(self.entries); entries[0x7B8D0] = 0x7B93C
        un = set(self.unresolved) | {0x7B956, 0x7B978, 0x7BBF3}
        old = self.text.replace('goto loc_0007BBF3;', 'g_seh_ebp = ebp; sub_0007BBF3(); return;')
        # With the corrected span the injected stub call lies inside it.
        bad = self_span_stub_calls(old, self.entries, un)
        self.assertIn((0x7B8D0, 0x7BBF3), bad)


if __name__ == '__main__':
    unittest.main()
