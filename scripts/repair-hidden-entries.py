"""Repair manifest spans that consume a separate evidenced entry.

`scripts/check-hidden-entries.py` proves the defect: an entry's declared `end`
runs past the end of its own reachable body, and the bytes in between hold a
separate function that has independent entry evidence (an aligned
`.data`/`.rdata` pointer, or another manifest entry's start).

This script applies the mechanical repair for exactly that finding:

  * the **container's** `end` moves back to the byte after its own reachable
    terminator, which is a fact about the bytes it already contains;
  * each **consumed** address gets its own manifest entry, so the runtime can
    resolve it instead of trapping with `[ICALL] Failed to resolve VA`.

Every derived value carries how it was obtained:

  `PROVED`    a fully enumerated walk reaches a `ret N` at depth 0, so
              `stack_args = N` by the identity `scripts/check-stack-depth.py`
              documents.  This is the same class its gate uses.
  `INFERRED`  the walk is enumerated and every reachable `ret` agrees on `N`,
              but no path reaches one at depth 0.  Recorded as inferred, and
              the manifest's `evidence` says so.
  `KEPT`      the body is a tail-jump body with no `ret` of its own, so its
              cleanup belongs to the tail target.  The existing declared value
              is preserved rather than replaced by a guess.

The script refuses to write anything unless the result is self-consistent.  On
`--write`/`--out` it re-loads the repaired manifest and asserts, before touching
the file, that

  * `check-hidden-entries.py` reports no gating verdict on it,
  * no two entries share a start address,
  * every added entry is inside `.text` with `end > start`, and
  * every added entry lifts standalone through `recover-functions.py`'s
    translator (the check that would have caught the `LNK2005` collision without
    a full build).

`--dry-run` (the default) reports what it would do and writes nothing.  Turn
Review 1 (2026-10-05) found that an earlier version of this docstring *claimed*
those checks while `main()` performed none of them -- the same silent-success
shape as the `apply()` bug it describes -- so they are implemented rather than
merely described.
"""
from __future__ import annotations

import argparse
import bisect
import importlib.util
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))

_spec = importlib.util.spec_from_file_location(
    'check_hidden_entries', ROOT / 'scripts' / 'check-hidden-entries.py')
assert _spec and _spec.loader
hidden = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(hidden)

MANIFEST = ROOT / 'config' / 'recovered-functions.json'
EVIDENCE = (
    'Recovered because {container} declared [0x{cs:08X}, 0x{ce:08X}) but its own '
    'reachable body ends at 0x{body_end:08X}, and this address lies in the '
    'over-run with independent entry evidence: {why}. '
    'scripts/check-hidden-entries.py proves the containment; the span is '
    'tightened to the container\'s own end and this address gets its own body so '
    'the runtime resolves it instead of trapping. '
    'stack_args {args} was {how}.'
)


class Repairer:
    def __init__(self, root):
        self.root = root
        self.image = hidden.Image(root)
        self.database = hidden.Database(root)
        self.records = json.loads((root / 'config' / 'recovered-functions.json').read_text())
        self.entries = [hidden.Entry(record) for record in self.records]
        self.pointer_sites = hidden.load_pointer_sites(root)
        self.finder = hidden.HiddenEntryFinder(
            self.image, self.database, self.entries, self.pointer_sites)
        self.manifest_starts = sorted(entry.start for entry in self.entries)
        self.genuine_db = sorted(
            start for start, (_, method) in self.database._by_start.items()
            if method not in hidden._stack_depth.FOLDED_DETECTION)
        self.boundaries = sorted(set(self.manifest_starts) | set(self.genuine_db))
        self.boundary_set = set(self.boundaries)
        self.existing = {entry.start: entry.end for entry in self.entries}
        # Consumed addresses that already own a manifest entry whose declared end
        # disagrees with the derived one.  Reported, never silently accepted.
        self.end_conflicts = []
        # Consumed addresses that already have their own generated body, so they
        # must not get a manifest entry.
        self.shadowed = []

    def next_boundary(self, va):
        """The next address after `va` that some other body claims."""
        candidates = []
        index = bisect.bisect_right(self.manifest_starts, va)
        if index < len(self.manifest_starts):
            candidates.append(self.manifest_starts[index])
        index = bisect.bisect_right(self.genuine_db, va)
        if index < len(self.genuine_db):
            candidates.append(self.genuine_db[index])
        return min(candidates) if candidates else None

    def bounds(self, limit, steps=40):
        """Candidate walk bounds: `limit`, then progressively wider boundaries.

        A body the swallowing span truncated falls off `limit`; the next real
        boundary is then the honest place to stop it -- the `0x1FFF0` case,
        whose own body is longer than the span that covered it.  The widening
        steps through one sorted list of every address some other body claims,
        so it cannot drift a few bytes at a time into an unrelated span.
        """
        out = [limit]
        for candidate in self.boundaries:
            if candidate <= limit:
                continue
            out.append(candidate)
            if len(out) > steps:
                break
        return out

    def resolve(self, start, limit, keep_args=None):
        """Derive `(end, stack_args, how, enumerated)` for a body.

        `limit` is the preferred bound; wider bounds are tried only when the
        walk does not resolve inside it, so a normal entry is never widened.

        Two conditions reject a bound, and the second was found by a real test
        failure rather than by reasoning ahead of it:

        * a **fall-off** means the body is not contained by this bound;
        * a **tail jump to an address that is not an entry** means the body
          continues past the bound.  `0xB3C30` is why: bounded at `0xB3D67` its
          walk looks complete, but two of its exits are tail jumps to
          `0xB3DCF`/`0xB3DD0`, which are internal to the real body ending at
          `0xB3DD4`.  Accepting the first bound made a generated body that calls
          an unresolved stub for its own continuation, which
          `tests/test_recovery_span_ownership.py` caught.
        """
        best = None
        for bound in self.bounds(limit):
            exits, falloffs, truncated, depths = self.finder.analyzer.walk(start, bound)
            # A fall-off means the body is not contained by this bound, so the
            # walk says nothing yet about where it ends.  Widening continues.
            if truncated or falloffs or not exits or not depths:
                continue
            # A tail to a non-entry is an internal continuation, not an exit.
            escapes = [target for _, kind, _, target in exits
                       if kind == 'tail' and target is not None
                       and target not in self.boundary_set]
            if escapes:
                continue
            last = max(depths)
            insn = self.finder.analyzer.insn_at(last)
            if insn is None or insn.mnemonic not in hidden.TERMINATORS:
                continue
            enumerated = not any(kind in hidden.OPAQUE_EXITS
                                 for _, kind, _, _ in exits)
            depth0 = sorted({imm for _, kind, depth, imm in exits
                             if kind == 'ret' and depth == 0 and imm is not None})
            every = sorted({imm for _, kind, _, imm in exits
                            if kind == 'ret' and imm is not None})
            end = last + insn.size
            if enumerated and len(depth0) == 1:
                best = (end, depth0[0], 'PROVED', True)
                break
            if best is None:
                if len(every) == 1:
                    best = (end, every[0], 'INFERRED', enumerated)
                else:
                    best = (end, None, 'NO-RET-IMMEDIATE', enumerated)
        if best is None:
            return None, None, 'UNRESOLVED', False
        end, args, how, enumerated = best
        if args is None and keep_args is not None:
            return end, keep_args, 'KEPT', enumerated
        return end, args, how, enumerated

    def plan(self):
        """Return `(container_updates, new_entries)` without modifying anything."""
        updates, additions = [], []
        for record, entry in zip(self.records, self.entries):
            verdict, consumed, detail = self.finder.evaluate(entry)
            if verdict not in hidden.GATING:
                continue
            cend, cargs, chow, _ = self.resolve(
                entry.start, entry.end, keep_args=entry.stack_args)
            if cend is None:
                raise SystemExit('container 0x%08X did not resolve' % entry.start)
            if cend > entry.end:
                raise SystemExit('container 0x%08X resolved past its declared end'
                                 % entry.start)
            updates.append({'start': entry.start, 'old_end': entry.end,
                            'end': cend, 'old_args': entry.stack_args,
                            'args': cargs, 'how': chow,
                            'verdict': verdict, 'consumed': consumed,
                            'detail': detail})
            for index, va in enumerate(consumed):
                limit = consumed[index + 1] if index + 1 < len(consumed) \
                    else entry.end
                end, args, how, enumerated = self.resolve(va, limit)
                if end is None:
                    raise SystemExit('consumed 0x%08X did not resolve' % va)
                if va in self.existing:
                    # `OVERLAP`: the address already owns a manifest entry, so
                    # nothing is added.  Only the container is tightened, and the
                    # existing entry's own declared end is checked against the
                    # derived one so a second defect there is not silently kept.
                    prior = self.existing[va]
                    if prior != end:
                        self.end_conflicts.append((va, prior, end, how))
                    continue
                if va in self.finder.generated:
                    # `SHADOWED`: the generated chunk already defines
                    # `sub_<va>`, so adding a manifest entry would be
                    # `LNK2005 sub_<va> already defined`.  Only the container is
                    # tightened.  This is measured, not assumed: two of the
                    # fifty additions hit exactly this and the build failed.
                    self.shadowed.append(va)
                    continue
                why = self.finder.evidence(va)
                additions.append({
                    'start': va, 'end': end, 'stack_args': args if args is not None else 0,
                    'how': how, 'enumerated': enumerated,
                    'evidence': EVIDENCE.format(
                        container=('manifest entry 0x%08X' % entry.start),
                        cs=entry.start, ce=entry.end, body_end=self.finder.body(
                            entry.start, entry.end)['true_end'],
                        why='; '.join(why),
                        args=args if args is not None else 0,
                        how=how),
                })
        return updates, additions

    def apply(self, updates, additions):
        """Return the repaired manifest as a list of records.

        Additions are keyed by their own start address, which is *not* the
        container's, so they are appended after the container pass and the whole
        list is re-sorted.  Keying them to the container's start (the first
        version of this method) silently dropped all 50 of them while reporting
        success, because no container start coincides with a consumed address.
        """
        by_start = {update['start']: update for update in updates}
        out = []
        for record in self.records:
            start = int(record['start'], 16)
            update = by_start.get(start)
            if update is not None:
                record = dict(record)
                record['end'] = '0x%08X' % update['end']
                if update['args'] is not None and update['args'] != record.get('stack_args'):
                    record['stack_args'] = update['args']
            out.append(record)
        present = {int(record['start'], 16) for record in out}
        for item in additions:
            if item['start'] in present:
                raise SystemExit('addition 0x%08X already exists in the manifest'
                                 % item['start'])
            out.append({
                'start': '0x%08X' % item['start'],
                'end': '0x%08X' % item['end'],
                'section': '.text',
                'kind': 'routine',
                'stack_args': item['stack_args'],
                'evidence': item['evidence'],
            })
        out.sort(key=lambda record: int(record['start'], 16))
        starts = [int(record['start'], 16) for record in out]
        if len(set(starts)) != len(starts):
            raise SystemExit('the repaired manifest would contain duplicate starts')
        return out


def verify(repairer, repaired, additions):
    """Refuse to write a manifest that is not self-consistent.

    These are the checks the docstring promises, and every one of them is here
    because an earlier version of this script wrote a repaired manifest that had
    passed *none* of them and reported success anyway.  Turn Review 1
    (2026-10-05) found the docstring claiming checks `main()` did not perform --
    the same silent-success shape as the `apply()` bug.  They run before the
    file is touched, so a failure leaves the committed manifest intact.
    """
    problems = []

    starts = [int(record['start'], 16) for record in repaired]
    duplicates = sorted({s for s in starts if starts.count(s) > 1})
    if duplicates:
        problems.append('duplicate start(s): %s'
                        % ', '.join('0x%08X' % s for s in duplicates[:8]))

    for item in additions:
        if item['end'] <= item['start']:
            problems.append('addition 0x%08X has end 0x%08X <= start'
                            % (item['start'], item['end']))
        if not repairer.image.is_code(item['start']):
            problems.append('addition 0x%08X is not inside .text' % item['start'])

    # The repaired manifest must satisfy the detector that motivated the repair.
    entries = [hidden.Entry(record) for record in repaired]
    finder = hidden.HiddenEntryFinder(
        repairer.image, repairer.database, entries, repairer.pointer_sites,
        repairer.finder.generated, repairer.finder.dispatched)
    gating = []
    for entry in entries:
        verdict, consumed, _ = finder.evaluate(entry)
        if verdict in hidden.GATING:
            gating.append((entry.start, verdict, consumed))
    if gating:
        problems.append('%d span(s) still gate, e.g. 0x%08X %s'
                        % (len(gating), gating[0][0], gating[0][1]))

    # Every added entry must lift standalone.  This is the check that would have
    # caught the LNK2005 collision without spending a full build on it.
    if additions:
        try:
            import json as _json
            sys.path.insert(0, str(ROOT.parent / 'xboxrecomp'))
            from tools.recomp import config as _config
            from tools.recomp.translator import FunctionTranslator

            _config.configure_from_xbe(str(ROOT / 'game' / 'default.xbe'))
            data = (ROOT / 'game' / 'default.xbe').read_bytes()
            database = {}
            for item in _json.loads(
                    (ROOT / 'tools' / 'disasm' / 'output' / 'functions.json').read_text()):
                a = int(item['start'], 16)
                item['_addr'] = a
                item['end'] = int(item['end'], 16)
                database[a] = item
            for fix in _json.loads(
                    (ROOT / 'config' / 'boundary-fixes.json').read_text(encoding='utf-8')):
                a = int(fix['start'], 16)
                database[a]['end'] = int(fix['end'], 16)
            labels = {int(v['address'], 16): v['name'] for v in _json.loads(
                (ROOT / 'tools' / 'disasm' / 'output' / 'labels.json').read_text())}
            for item in additions:
                probe = dict(database)
                probe[item['start']] = {
                    'start': '0x%08X' % item['start'], 'end': item['end'],
                    '_addr': item['start'], 'size': item['end'] - item['start'],
                    'name': 'sub_%08X' % item['start'], 'confidence': 1.0,
                    'detection_method': 'reviewed_runtime_target',
                    'has_prologue': False, 'calls_to': [], 'called_by': []}
                translator = FunctionTranslator(data, probe, labels)
                code = translator.translate_function(item['start'], probe[item['start']])
                if not code or '/* TODO:' in code:
                    problems.append('addition 0x%08X does not lift standalone'
                                    % item['start'])
        except ImportError as exc:
            problems.append('cannot verify liftability (translator unavailable): %s' % exc)

    if problems:
        print('\nREFUSING TO WRITE: %d consistency problem(s):' % len(problems))
        for problem in problems:
            print('   %s' % problem)
        raise SystemExit(1)
    print('verified: %d entries, no gating span, no duplicate start, '
          '%d addition(s) lift standalone' % (len(repaired), len(additions)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--write', action='store_true',
                        help='write the repaired manifest (default is a dry run)')
    parser.add_argument('--out', default=None, metavar='PATH',
                        help='write the repaired manifest to this path instead')
    args = parser.parse_args()

    repairer = Repairer(ROOT)
    updates, additions = repairer.plan()

    print('containers to tighten: %d' % len(updates))
    print('entries to add       : %d' % len(additions))
    changes = [u for u in updates if u['args'] != u['old_args']]
    print('stack_args changes   : %d' % len(changes))
    for update in changes:
        print('   0x%08X  args %s -> %s (%s)'
              % (update['start'], update['old_args'], update['args'], update['how']))
    unresolved = [a for a in additions if a['how'] not in ('PROVED', 'INFERRED', 'KEPT')]
    print('additions with no clean derivation: %d' % len(unresolved))
    for item in unresolved:
        print('   0x%08X end 0x%08X %s' % (item['start'], item['end'], item['how']))
    inferred = [a for a in additions if a['how'] != 'PROVED']
    print('additions not PROVED: %d' % len(inferred))
    for item in inferred:
        print('   0x%08X..0x%08X args=%s (%s)'
              % (item['start'], item['end'], item['stack_args'], item['how']))
    print('consumed addresses already owning an entry (no addition): %d'
          % len([u for u in updates if u['verdict'] == hidden.OVERLAP]))
    print('consumed addresses already having a generated body (no addition): %d'
          % len(repairer.shadowed))
    for va in sorted(repairer.shadowed):
        print('   0x%08X (dispatch already defines sub_%08X)' % (va, va))
    conflicts = repairer.end_conflicts
    print('existing entries whose declared end disagrees with the derived one: %d'
          % len(conflicts))
    for va, prior, derived, how in conflicts:
        print('   0x%08X declared end 0x%08X, derived 0x%08X (%s)'
              % (va, prior, derived, how))

    if not args.write and not args.out:
        print('\ndry run: nothing written.  Use --write to apply.')
        return 0

    repaired = repairer.apply(updates, additions)
    verify(repairer, repaired, additions)

    target = Path(args.out) if args.out else MANIFEST
    target.write_text(json.dumps(repaired, indent=1) + '\n', encoding='utf-8')
    print('\nwrote %d entries -> %s' % (len(repaired), target))
    return 0


if __name__ == '__main__':
    sys.exit(main())
