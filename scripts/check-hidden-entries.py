"""Find manifest entries whose declared span consumes a separate function.

## The failure this detects

`config/recovered-functions.json` gives every reviewed entry an exclusive `end`.
The documented convention for that end is "the next function entry", and
`scripts/check-entry-extents.py` already fails when the end lands mid-instruction.
Both can be satisfied while the entry is still **too wide**:

    0x0007DAE0   declared [0x7DAE0, 0x7DE20)
                 its own reachable body ends at 0x7DBCB `ret`
                 + 4 NOPs
                 and a complete 190-instruction function begins at 0x7DBD0,
                 which the declared span covers entirely.

`0x7DE20` is simultaneously this entry's declared end **and** the end of the
function at `0x7DBD0`, so an end-versus-next-start check calls the entry correct.
It is not: `0x7DBD0` is a real function -- its only reference in the whole image
is the aligned `.rdata` dword at `0x0020D3C8` -- and no span owns it, so an
indirect call through that dword traps with `[ICALL] Failed to resolve VA`.

The same shape is recorded for `0x1FF90` (consumes `0x1FFF0`) and `0x91C00`
(consumes `0x91C30` and `0x91D70`), and this detector finds **48** containers and
**59** consumed entries on the current manifest.

## The invariant

> A manifest entry's declared span must not extend past the end of its own
> reachable body into an address that has independent evidence of being a
> separate executable entry.

Both halves are load-bearing, and each answers a failure this project has
already paid for.

### Half 1 -- the body provably ends early

`Analyzer.walk` from the entry's own `start` is **reused**, not reimplemented
(`scripts/check-stack-depth.py`).  A second CFG walker would be a second thing to
keep correct, and that analyzer's verdicts are already gated and controlled.

The finding requires the walk to be *fully enumerated*:

* no instruction crosses `end` (`truncated` empty),
* no fall-off at all (`falloffs` empty) -- so no path ran into `end`,
* no `indirect` or `terminal` exit -- every `jmp` either resolved to an in-span
  target, was an immediate tail to a known address, or the table could not be
  read and was recorded opaque,
* the last reachable instruction is a terminator (`ret`/`jmp`/`int3`/...).

**That combination is what makes "not reached" a proof.**  With no opaque exit
and no fall-off, every path from the entry has been followed to a terminator, so
the walk is a complete enumeration of the entry's reachable instruction set.  An
address inside the declared span that the walk never visited is therefore not
reachable from this entry -- a statement about the bytes that were actually
walked.

This is the `0x80BD0` lesson used in the safe direction.  `0x80BD0` has **zero**
rel32 callers and is a real standalone SEH function reached only through a
`.rdata` dword, so "nothing calls it" proves nothing; a previous session widened
`0x80340` to `0x81853` on exactly that reasoning and was wrong.  The test here is
not caller absence.  It is that *this entry's own control flow* was fully
enumerated and did not arrive at the address.

### Half 2 -- the address has independent entry evidence

Two sources, both measured:

* **An aligned `.data`/`.rdata` dword whose value lands in `.text`.**  This is how
  these functions are dispatched: `0x7DBD0` occurs in exactly one aligned dword in
  the image, `0x0020D3C8`.
* **An address another manifest entry already starts at.**  Then the two spans
  overlap and at least one of them is wrong by construction.

A raw pointer sweep is noisy, so the candidate must also be *plausible as an
instruction start*: 4-byte aligned, not itself a padding byte, decodable, and
preceded by an alignment-padding boundary.  The last two tests are the ones
`docs/jsrf-technical-record.md` §12 measured as taking the sibling census from
117 to 93 -- ten of the thirteen rejected values come from a single 16-bit word
array whose high bytes happen to be `0x90`.

## Verdicts

| verdict | meaning | gate |
|---|---|---|
| `HIDDEN_ENTRY` | body provably ends early, and the over-run holds an evidenced entry that **does not resolve at runtime** | **FAILS** |
| `OVERLAP` | body provably ends early, and the over-run holds an address **another manifest entry starts at** | **FAILS** |
| `MISDISPATCH` | body provably ends early, and the over-run holds an evidenced entry that **resolves to a different symbol** | reported |
| `OVER_RUN` | body provably ends early and the over-run is not padding, but no evidenced entry was found | reported |
| `UNQUALIFIED` | a candidate exists, but the container's walk has an opaque exit or a fall-off | reported |
| `CLEAN` | no over-run, or the over-run is only alignment padding | pass |

`HIDDEN_ENTRY` and `OVERLAP` are the gating classes and need **no baseline**, for
the same reason `scripts/check-stack-depth.py` gates only its `STACK_ARGS` class:
each is a property of this entry's own bytes plus one other address's independent
reference, with no whole-program reasoning required.  Both are unambiguous --
a trap the runtime cannot resolve, or two spans that overlap.

`MISDISPATCH` is the separate population `docs/jsrf-technical-record.md` §12
identified: an alias shim **does** resolve, so `check-table-targets.py` cannot see
it, but the symbol answering it is its parent's, so entering it runs the wrong
body.  It is reported rather than gated because §12 leaves the actionable subset
open ("the rest are genuine mid-body labels for which running the parent's body
from the start is correct").

`UNQUALIFIED` is never silently treated as clean.  An unresolved indirect edge
could in principle be what reaches the over-run, so the honest answer is "not
decided", and the class is counted and named instead of guessed either way.

## Coverage limits, stated rather than hidden

* The evidence half sees `.data`/`.rdata` dwords and manifest starts.  A function
  reached **only** by a `rel32` call or a misaligned immediate is not found by it;
  the over-run half still reports `OVER_RUN` for the container.
* The walk is `check-stack-depth.py`'s model, with the same limits: an `esp`
  written from a register, an unresolvable indirect jump or a walk-bound trip
  makes the entry `UNQUALIFIED` rather than clean.
* Only `[start, end)` of the declared entry is examined.  An entry that is too
  *narrow* is `check-entry-extents.py`'s class, not this one.
* A jump table whose read stopped early could in principle hide an arm.  The read
  stops at the first value that is not executable code, and any `jmp` the walk
  could not fully resolve is recorded opaque, so an under-read table degrades the
  entry to `UNQUALIFIED` rather than to a false finding.

## Controls

`--selfcheck` replays four real defects at their pre-fix spans and requires
`HIDDEN_ENTRY` naming the specific consumed address, then requires the corrected
span to be clean.  A fifth control requires the `0x96F60` case -- whose body ends
in `jmp dword ptr [eax+8]` -- to stay `UNQUALIFIED`, because firing there would
mean the detector claims a separation it cannot prove.  A control that fired on
both the bad and the good span would prove nothing.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Reuse the reviewed reachable-CFG analyzer instead of writing a second walker.
_spec = importlib.util.spec_from_file_location(
    'check_stack_depth', ROOT / 'scripts' / 'check-stack-depth.py')
assert _spec and _spec.loader
_stack_depth = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_stack_depth)

Image = _stack_depth.Image
Database = _stack_depth.Database
Analyzer = _stack_depth.Analyzer
Entry = _stack_depth.Entry
UNK = _stack_depth.UNK

HIDDEN_ENTRY = 'HIDDEN_ENTRY'
OVERLAP = 'OVERLAP'
SHADOWED = 'SHADOWED'
MISDISPATCH = 'MISDISPATCH'
OVER_RUN = 'OVER_RUN'
UNQUALIFIED = 'UNQUALIFIED'
CLEAN = 'CLEAN'

# The container is over-wide in all three of these, so all three gate.  They
# differ only in what the *consumed* address needs:
#   HIDDEN_ENTRY  it resolves nowhere -> it needs its own manifest entry
#   OVERLAP       it is another manifest entry -> only the container is wrong
#   SHADOWED      it already has its own generated body -> only the container is
#                 wrong, and adding an entry would be `LNK2005 already defined`
GATING = (HIDDEN_ENTRY, OVERLAP, SHADOWED)

# A function boundary: alignment padding terminated by the previous body's exit.
PAD_BYTES = frozenset({0x90, 0xCC})
SCAN_BACK = 32
BOUNDARY_TAIL = frozenset({0xC3, 0xC2, 0xE9, 0xEB, 0xCC})
# Exits that end a body without a known successor.  An `indirect` exit means the
# walk could not enumerate where control goes, so a "not reached" conclusion for
# bytes past the body is not decidable and the entry must not be gated.
OPAQUE_EXITS = frozenset({'indirect', 'terminal'})
TERMINATORS = frozenset({'ret', 'retf', 'jmp', 'int3', 'hlt', 'ud2'})


def load_pointer_sites(root):
    """Every aligned `.data`/`.rdata` dword whose value lands in `.text`."""
    analysis = json.loads((root / 'game' / 'mygame_analysis.json').read_text())
    xbe = (root / 'game' / 'default.xbe').read_bytes()
    text = next(s for s in analysis['sections'] if s['name'] == '.text')
    low = int(text['virtual_addr'], 16)
    high = low + text['virtual_size']
    sites = {}
    for section in analysis['sections']:
        if section['name'] not in ('.data', '.rdata'):
            continue
        base = int(section['virtual_addr'], 16)
        offset = int(section['raw_addr'], 16)
        for i in range(0, section['raw_size'] - 3, 4):
            value = struct.unpack_from('<I', xbe, offset + i)[0]
            if low <= value < high:
                sites.setdefault(value, base + i)
    return sites


def load_generated_bodies(root):
    """What the generated dispatch says about each address it names.

    Returns `(own, all_entries)`.  `own` holds addresses whose dispatch tuple
    names their **own** symbol `sub_<va>`, so the generated chunk already
    defines that body -- adding a manifest entry for one would produce
    `LNK2005 sub_<va> already defined`.  Measured on the repaired manifest: two
    of fifty consumed addresses are in this state (`0x556D0`, `0xC42E0`), and
    both were caught by a real link failure before this set was consulted.

    `all_entries` is every address the dispatch names.  An address in it that is
    *not* in `own` resolves to a different symbol: that is the alias-shim
    misdispatch class `docs/jsrf-technical-record.md` §12 describes, where the
    address runs its parent's body from the start.

    Only the generated dispatch is read, never `recovered.c`, so this set is
    stable while a recovery regeneration is in flight.
    """
    import re
    dispatch = root / 'src' / 'recomp' / 'gen' / 'recomp_dispatch.c'
    if not dispatch.exists():
        return set(), set()
    own, every = set(), set()
    for match in re.finditer(
            r'\{\s*0x([0-9A-Fa-f]+)u,\s*\(recomp_func_t\)(\w+)\s*\}',
            dispatch.read_text(encoding='utf-8', errors='replace')):
        va = int(match.group(1), 16)
        every.add(va)
        if match.group(2).lower() == 'sub_%08x' % va:
            own.add(va)
    return own, every


class HiddenEntryFinder:
    """Detect declared spans that over-run into a separate evidenced entry."""

    def __init__(self, image, database, entries, pointer_sites,
                 generated=None, dispatched=None):
        self.image = image
        self.database = database
        self.analyzer = Analyzer(image, database, entries)
        self.pointer_sites = pointer_sites
        self.manifest_starts = {entry.start for entry in entries}
        # Addresses that already have their own generated body, and every
        # address the generated dispatch names, both read from
        # `recomp_dispatch.c`.  Empty when the generated tree is absent, which
        # only makes the checker report HIDDEN_ENTRY where the tree would say
        # SHADOWED -- never the reverse, so it cannot hide a defect.
        if generated is None or dispatched is None:
            own, every = load_generated_bodies(ROOT)
            generated = own if generated is None else generated
            dispatched = every if dispatched is None else dispatched
        self.generated = generated
        self.dispatched = dispatched

    # -- the walk ---------------------------------------------------------
    def body(self, start, end):
        """Describe where this entry's own reachable body stops, if it stops.

        Returns None when the walk does not establish an early end at all: a
        truncated decode, no modelled exit, or a fall-off that reaches `end`
        (positive evidence the body is *not* bounded by its span, so there is no
        over-run to report).

        Otherwise returns the last reachable address, the byte after its
        terminator, and `enumerated`: True only when every path was followed to a
        known successor, with no `indirect`/`terminal` exit.  **That flag is what
        makes "the walk never visited this address" a proof**, so a
        non-enumerated body may still be reported but must never be gated.
        """
        exits, falloffs, truncated, depths = self.analyzer.walk(start, end)
        if truncated or not exits or not depths or falloffs:
            return None
        last = max(depths)
        insn = self.analyzer.insn_at(last)
        if insn is None or insn.mnemonic not in TERMINATORS:
            return None
        true_end = last + insn.size
        if true_end > end:
            return None
        return {
            'true_end': true_end,
            'last': last,
            'depths': depths,
            'exits': exits,
            'enumerated': not any(kind in OPAQUE_EXITS
                                  for _, kind, _, _ in exits),
        }

    # -- candidate evidence ----------------------------------------------
    def plausible(self, va):
        """Could `va` begin an instruction, judged without any evidence source?

        The measured noise filter from `docs/jsrf-technical-record.md` §12: an
        aligned value in a data section is only a candidate function pointer if
        the address it names is aligned, is not itself a padding byte, decodes,
        and is preceded by an alignment-padding boundary.
        """
        if va % 4 != 0:
            return False
        first = self.image.bytes_at(va, 1)
        if not first or first[0] in PAD_BYTES:
            return False
        if self.analyzer.insn_at(va) is None:
            return False
        window = self.image.bytes_at(va - SCAN_BACK, SCAN_BACK)
        if window is None:
            return False
        pad = 0
        for byte in reversed(window):
            if byte in PAD_BYTES:
                pad += 1
            else:
                break
        if pad == 0:
            return False
        tail = self.image.bytes_at(va - pad - 1, 1)
        if tail is None:
            return False
        return tail[0] in BOUNDARY_TAIL or pad >= 4

    def evidence(self, va):
        """Independent reasons `va` is an executable entry in its own right."""
        reasons = []
        if va in self.manifest_starts:
            reasons.append('another manifest entry starts here')
        if va in self.pointer_sites:
            reasons.append('aligned pointer at 0x%08X' % self.pointer_sites[va])
        return reasons

    # -- the verdict ------------------------------------------------------
    def evaluate(self, entry):
        """Return `(verdict, consumed_addresses, detail)` for one entry."""
        container = self.body(entry.start, entry.end)
        if container is None:
            return CLEAN, [], 'the body does not provably end inside its own span'

        true_end = container['true_end']
        if true_end >= entry.end:
            return CLEAN, [], 'the body uses its whole declared span'

        leftover = self.image.bytes_at(true_end, entry.end - true_end) or b''
        if not leftover.strip(b'\x90\xcc'):
            return CLEAN, [], ('only %d padding byte(s) past the body at 0x%08X'
                               % (len(leftover), true_end))

        consumed = []
        for va in sorted(set(self.pointer_sites) | self.manifest_starts):
            if not (true_end <= va < entry.end) or va == entry.start:
                continue
            if va in container['depths']:
                continue                    # an internal label this body reaches
            if va not in self.manifest_starts and not self.plausible(va):
                continue
            reasons = self.evidence(va)
            if reasons:
                consumed.append((va, reasons))

        if not consumed:
            return OVER_RUN, [], (
                'body ends at 0x%08X but the span runs to 0x%08X and no evidenced '
                'entry was found in the over-run' % (true_end, entry.end))

        listed = ', '.join('0x%08X [%s]' % (va, '; '.join(reasons))
                           for va, reasons in consumed)

        # The gate rests on the walk being a complete enumeration.  When it is
        # not -- an unresolved indirect exit, or a terminal that is not a return
        # -- an unvisited address is not proven unreachable, because the
        # unmodelled edge could be exactly what reaches it.  The finding is then
        # reported as UNQUALIFIED and never gated, in either direction.
        if not container['enumerated']:
            return UNQUALIFIED, [va for va, _ in consumed], (
                'body ends at 0x%08X (declared end 0x%08X) and the span over-runs '
                'into %s, but the walk has an unresolved indirect or terminal '
                'exit, so the over-run is not proven unreachable'
                % (true_end, entry.end, listed))

        overlap = [va for va, _ in consumed if va in self.manifest_starts]
        if overlap:
            return OVERLAP, [va for va, _ in consumed], (
                'body ends at 0x%08X (declared end 0x%08X): the span covers the '
                'separate manifest entr(y/ies) %s'
                % (true_end, entry.end,
                   ', '.join('0x%08X' % va for va in overlap)))

        shadowed = [va for va, _ in consumed if va in self.generated]
        if shadowed:
            return SHADOWED, [va for va, _ in consumed], (
                'body ends at 0x%08X (declared end 0x%08X): the span covers %s, '
                'which already has its own generated body (adding a manifest '
                'entry would be LNK2005)'
                % (true_end, entry.end,
                   ', '.join('0x%08X' % va for va in shadowed)))

        misdispatched = [va for va, _ in consumed if va in self.dispatched]
        if misdispatched:
            return MISDISPATCH, [va for va, _ in consumed], (
                'body ends at 0x%08X (declared end 0x%08X): the span covers %s, '
                'which the dispatch answers with a different symbol'
                % (true_end, entry.end,
                   ', '.join('0x%08X' % va for va in misdispatched)))

        return HIDDEN_ENTRY, [va for va, _ in consumed], (
            'body ends at 0x%08X (declared end 0x%08X): the span consumes %s, '
            'which the runtime cannot resolve at all'
            % (true_end, entry.end, listed))


CONTROLS = (
    {
        'name': '0x0007DAE0 declared [0x7DAE0, 0x7DE20) consumes 0x7DBD0',
        'bad': {'start': '0x0007DAE0', 'end': '0x0007DE20', 'stack_args': 0},
        'good': {'start': '0x0007DAE0', 'end': '0x0007DBCC', 'stack_args': 0},
        'without': [0x7DBD0],
        'must_name': [0x7DBD0],
        'expect': HIDDEN_ENTRY,
        'why': 'the 190-instruction function at 0x7DBD0 is referenced only by the '
               'aligned .rdata dword at 0x0020D3C8, and the declared end 0x7DE20 '
               'is simultaneously that function\'s own end',
    },
    {
        'name': '0x0001FF90 declared [0x1FF90, 0x200A5) consumes 0x1FFF0',
        'bad': {'start': '0x0001FF90', 'end': '0x000200A5', 'stack_args': 4},
        'good': {'start': '0x0001FF90', 'end': '0x0001FFEA', 'stack_args': 4},
        'without': [0x1FFF0],
        'must_name': [0x1FFF0],
        'expect': HIDDEN_ENTRY,
        'why': 'the body ends `ret 4` at 0x1FFE7 and the span runs on through six '
               'NOPs into a function with its own prologue',
    },
    {
        'name': '0x00091C00 declared [0x91C00, 0x91EB0) consumes 0x91C30 and 0x91D70',
        'bad': {'start': '0x00091C00', 'end': '0x00091EB0', 'stack_args': 0},
        'good': {'start': '0x00091C00', 'end': '0x00091C24', 'stack_args': 0},
        'without': [0x91C30, 0x91D70],
        'must_name': [0x91C30, 0x91D70],
        'expect': HIDDEN_ENTRY,
        'why': 'the body ends with a plain `ret` at 0x91C23 and the span covers '
               'two further functions, both reachable only through .rdata dwords',
    },
    {
        'name': '0x00055670 declared [0x55670, 0x55800) consumes 0x556D0',
        'bad': {'start': '0x00055670', 'end': '0x00055800', 'stack_args': 0},
        'good': {'start': '0x00055670', 'end': '0x000556CD', 'stack_args': 0},
        'without': [],
        'must_name': [0x556D0],
        'expect': SHADOWED,
        'why': 'a three-byte `ret` then three NOPs, and the consumed address '
               'already has its own generated body, so only the container is '
               'wrong and adding an entry would be LNK2005',
    },
    {
        'name': '0x00096F60 declared [0x96F60, 0x97190) must stay UNQUALIFIED',
        'bad': {'start': '0x00096F60', 'end': '0x00097190', 'stack_args': 0},
        'good': None,
        'without': [],
        'must_name': [0x96F80],
        'expect': UNQUALIFIED,
        'why': 'the body ends in `jmp dword ptr [eax+8]`, an unresolved indirect '
               'exit, so the bytes past it are not provably unreached and the '
               'detector must not claim a separation it cannot prove',
    },
)


def selfcheck(build_finder) -> int:
    """Positive and negative controls over the real XBE bytes.

    Each case is a defect that actually happened, replayed at its pre-fix span.
    The pre-fix *manifest* state is reproduced by building the finder without the
    consumed addresses, because the committed manifest now carries their repairs
    -- otherwise the same span would legitimately report `OVERLAP` (the consumed
    address is a manifest entry now) and the control would drift every time a
    repair lands.  Reproducing the pre-fix state keeps the control asserting the
    verdict the defect was diagnosed under.

    The corrected form must come back clean, because a control that fires on both
    the bad and the good span would prove nothing.
    """
    failures = 0
    print('self-check: the detector against the real hidden-entry defects')
    for case in CONTROLS:
        # Rebuild the manifest as it was before this case's repair.
        finder = build_finder(set(case['without']))
        entry = Entry(case['bad'])
        verdict, consumed, detail = finder.evaluate(entry)
        want = case['expect']
        ok = verdict == want and all(va in consumed for va in case['must_name'])
        failures += 0 if ok else 1
        print('  %-5s %s' % ('PASS' if ok else 'FAIL', case['name']))
        print('        -> %s, consumed=%s: %s'
              % (verdict, ['0x%08X' % va for va in consumed], detail))
        if not ok:
            print('        expected %s naming %s.  %s'
                  % (want, ['0x%08X' % va for va in case['must_name']], case['why']))

        if case['good'] is not None:
            good = Entry(case['good'])
            gverdict, _, gdetail = finder.evaluate(good)
            gok = gverdict not in GATING
            failures += 0 if gok else 1
            print('  %-5s negative half: the corrected span does not gate'
                  % ('PASS' if gok else 'FAIL'))
            print('        -> %s: %s' % (gverdict, gdetail))

    if failures:
        print('self-check FAILED: %d control assertion(s) failed; the detector has '
              'lost the sensitivity this gate rests on.' % failures)
        return 1
    print('self-check passed: every hidden-entry defect is named at its pre-fix '
          'span, the indirect-exit case stays UNQUALIFIED, and every corrected '
          'span is clean.')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--selfcheck', action='store_true',
                        help='positive/negative controls over the real defects')
    parser.add_argument('--json', action='store_true',
                        help='emit machine-readable results')
    parser.add_argument('--show', default='HIDDEN_ENTRY,OVERLAP',
                        help='comma-separated verdicts to list')
    parser.add_argument('--show-all', action='store_true',
                        help='list every non-CLEAN verdict')
    parser.add_argument('--manifest', default=None, metavar='PATH',
                        help='validate this manifest instead of the committed one')
    args = parser.parse_args()

    image = Image(ROOT)
    database = Database(ROOT)
    manifest_path = Path(args.manifest) if args.manifest \
        else ROOT / 'config' / 'recovered-functions.json'
    records = json.loads(manifest_path.read_text())
    entries = [Entry(record) for record in records]

    pointer_sites = load_pointer_sites(ROOT)
    own_bodies, dispatched = load_generated_bodies(ROOT)

    def build_finder(without=frozenset()):
        """A finder over this manifest, optionally pretending some repairs are absent.

        `without` holds addresses to treat as *not yet recovered*, which is how
        `--selfcheck` replays a defect at its pre-fix span even though the
        committed manifest now carries the repair.
        """
        if not without:
            return HiddenEntryFinder(image, database, entries, pointer_sites,
                                     own_bodies, dispatched)
        kept = [entry for entry in entries if entry.start not in without]
        return HiddenEntryFinder(image, database, kept, pointer_sites,
                                 own_bodies, dispatched)

    if args.selfcheck:
        return selfcheck(build_finder)

    finder = build_finder()

    results = []
    counts: dict = {}
    for entry in entries:
        verdict, consumed, detail = finder.evaluate(entry)
        counts[verdict] = counts.get(verdict, 0) + 1
        results.append({'start': entry.start, 'end': entry.end,
                        'stack_args': entry.stack_args, 'verdict': verdict,
                        'consumed': ['0x%08X' % va for va in consumed],
                        'detail': detail})

    if args.json:
        print(json.dumps({'counts': counts, 'results': results}, indent=1))
        return 0

    show = {name.strip().upper() for name in args.show.split(',') if name.strip()}
    if args.show_all:
        show = {HIDDEN_ENTRY, OVERLAP, SHADOWED, MISDISPATCH, OVER_RUN, UNQUALIFIED}

    print('entries checked: %d' % len(entries))
    print('verdicts: ' + ', '.join('%d %s' % (v, k) for k, v in
                                   sorted(counts.items(), key=lambda kv: -kv[1])))
    for record in results:
        if record['verdict'] not in show:
            continue
        print('  %-12s entry 0x%08X-0x%08X: %s'
              % (record['verdict'], record['start'], record['end'], record['detail']))

    gated = sum(counts.get(name, 0) for name in GATING)
    if gated:
        print('\nFAIL: %d manifest span(s) consume a separate evidenced entry.'
              % gated)
        return 1
    print('\nPASS: no manifest span consumes a separate evidenced entry '
          '(%d over-run(s) reported, %d misdispatch(es), %d undecided, none '
          'suppressed)'
          % (counts.get(OVER_RUN, 0), counts.get(MISDISPATCH, 0),
             counts.get(UNQUALIFIED, 0)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
