"""Validate manifest entries against their own reachable stack-depth CFG.

## Why this exists

Four defects in one session were found only by spending a game run each:

* `0x00080BD0` was declared `[0x80BD0, 0x80C83)` with `stack_args 4`.  The end is
  the **join point of four branches inside the function**, so the body ran its
  whole prologue and never reached its epilogue; the run measured
  `esp 00F7FE70->00F7FE30`, a delta of exactly `-0x40`.
* `0x000BB7B0` was declared `[0xBB7B0, 0xBBA04)`, cutting the function's own
  shared tail-merged epilogue at `0xBBA17` that three in-span branches target.
* `0x0007DA30` was declared `[0x7DA30, 0x7DA84)` with `stack_args 0`; the span was
  truncated and hid a `ret 4`.
* `0x00074C70` was declared `stack_args 4` for a body that has **no `ret` of its
  own** -- it ends `pop edi; pop esi; jmp 0x6A770`.

All four are stack-contract contradictions that are visible **statically**, from
the original bytes and the declared span, without running anything.  This script
is the detector.  It is deliberately not a heuristic lint: it walks the entry's
own reachable control flow and compares the declared `stack_args` with what the
reachable exits actually do.

## The model

`d` is `ESP - ESP_at_entry`, with the incoming return address already on the
stack, so `d` starts at **0**.  A correct function leaves `d == 0` at every
reachable `ret`, and the total delta to the caller is `4 + <ret immediate>`,
which is exactly what `scripts/recover-functions.py` asserts with
`stack_delta = 4 + entry.get('stack_args', 0)`.

Transfers are exact where the bytes make them exact and **UNKNOWN** where they do
not.  UNKNOWN is a real verdict, never a silent zero: an unknown call cleanup, an
unresolved indirect jump, or an `esp` write from a register all poison the depth,
and the entry is then reported UNKNOWN rather than PROVED.  That direction is
deliberate -- a validator that guesses would manufacture the false failures it is
supposed to prevent.

**The metadata under test never establishes its own correctness.**  A callee's
contribution is derived by walking *its* body, not by reading its declared
`stack_args`; if that walk does not resolve, the caller is UNKNOWN.  Otherwise a
wrong `stack_args` on one entry would launder itself into the next.

## Verdicts

| verdict | meaning | gate |
|---|---|---|
| `DEFECT` | a supported counterexample with a witness | **FAILS** |
| `SUSPICIOUS` | a boundary/entry finding needing adjudication | reported |
| `UNKNOWN` | an unsupported transfer, cleanup or target | reported, never PROVED |
| `PROVED` | every modelled reachable path checked, exit totals agree | pass |

`DEFECT` is deliberately **one class, so the gate needs no baseline and carries
no false-positive burden.**  The census on the committed manifest is the evidence
for that choice: of the four classes this model can report, only one is a
property of the original bytes that needs no global depth reasoning at all.

* `STACK_ARGS` -- **the gating class.** Every reachable exit is the *same*
  `ret N` reached at depth 0, there is no tail call and no fall-through, and the
  walk is fully resolved (no unknown depth, no unresolved callee cleanup, no
  unresolved indirect transfer).  Then `N` must equal `stack_args`, whatever any
  callee did: the body pushes and pops in balance and leaves through one
  immediate.  It is a byte fact about this entry alone.  This is the form that
  catches `0x7DA30`'s hidden `ret 4`, `0x74C70`'s copied `4`, and the twenty
  entries whose bodies are a balanced `push esi; ...; pop esi; ret 4` declared
  as `stack_args 0`.

Everything else is `SUSPICIOUS`, carries its precise code, and does **not** fail
the gate:

* `TRUNCATED` -- a reachable instruction crosses `end`.
* `FALL_OFF_END` -- control reaches `end` and no function begins there.
* `RET_DEPTH` -- a reachable `ret` sits at a nonzero depth.
* `CUT_EPILOGUE` -- a branch leaves the span at a nonzero depth to an address
  that is neither an entry nor inside any function.

**Why they are not gated, measured rather than asserted.** On the committed
3,109-entry manifest the census is 0 `STACK_ARGS`, 24 `RET_DEPTH`, 26
`FALL_OFF_END`, 71 `CUT_EPILOGUE` and 2 `TRUNCATED`.  The span classes overlap
`scripts/check-span-exits.py`'s existing 360-finding CUT-TARGET population (156
entries) -- a class the project already knows about and deliberately does not
gate -- and the rest need per-entry boundary adjudication that a validator must
not fake.  Folding them in would either block `just check` indefinitely or force
a broad baseline, and a broad baseline is exactly how `0x000307A0` stayed hidden
inside `config/entry-extent-baseline.json` while the gate stayed green.  They are
counted, named and printed instead, so the population stays visible and shrinks
by repair rather than by suppression.

`--show-suspicious` lists them, and `--selfcheck` proves the detector still
reports each historical defect under the code it was diagnosed with, so the
non-gating classes keep their regression coverage even though they do not fail
the build.

**What the gate does and does not depend on, stated precisely.**  The gating
class is a statement about *this entry's own bytes*: at a reachable `ret N`
reached at depth 0, the declared `stack_args` must be `N`.  Nothing about the
declared value comes from another function's metadata.  But the *depth* is not
always computable from this entry alone -- a `call` contributes its callee's own
`ret N` immediate, and those summaries are derived by walking the callee, using
manifest and analysis-database spans to find the callee's extent.  So the honest
statement is: **the conclusion is about this entry, but the depth it rests on may
depend on callee summaries.**  Treating every `call` as depth-neutral instead was
measured and discarded, because it finds **zero** defects on the pre-turn
manifest -- the twenty repaired entries all need their callee's cleanup to be
resolved to reach the `ret` at depth 0.

## Controls

`--selfcheck` replays each historical defect **at its pre-fix span and metadata**
and requires the detector to report the named verdict; each corrected entry is
then required to come back clean.  The pre-fix form is the only form in which the
miss is reproducible, because the manifest now carries the corrections.  A
control that passes on both the bad and the good span would prove nothing, so
each case asserts the *specific* verdict that changed.

**The gate is the bare run and it needs no baseline.**  It passes on the
committed manifest with zero `DEFECT`s, because the gating class is decidable and
the defects it finds are repaired rather than frozen.  `--baseline` and
`--write-baseline` exist only so a future session can measure a *regression*
against a named set; a baseline that is not empty is a defect population that was
suppressed, which is the failure `0x000307A0` documents, so
`tests/test_stack_depth.py` fails if `config/stack-depth-baseline.json` appears
without one.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

# --- verdict names, in the order a report prints them ----------------------
DEFECT = 'DEFECT'
SUSPICIOUS = 'SUSPICIOUS'
UNKNOWN = 'UNKNOWN'
PROVED = 'PROVED'

# Detection methods the analysis database uses for an address that is NOT a
# genuine entry in its own right.  `tail_jump_alias` is the `ff4d442`
# abutting-alias fold: the address resolves at run time, but the symbol that
# answers it is its parent's, so entering it runs the parent's body from the
# wrong byte.  `gap_prologue` is the pass that accepts a `push` after a `ret` as
# a function entry, which is how `0x000BBA04` and `0x000307F8` were invented.
# `resolution_starts.genuine_starts()` states the same distinction from the
# generated sources; this is the analysis-database half of it.
FOLDED_DETECTION = frozenset({'tail_jump_alias', 'gap_prologue'})

# A callee summary is a whole-body walk.  These bound the work so a pathological
# database span cannot make the gate slow; a bound that trips is reported as a
# limit, never as a pass.
MAX_SPAN_BYTES = 20000
MAX_INSN_BYTES = 15
MAX_JUMP_TABLE = 64
MAX_WALK_STEPS = 200000


class Unknown:
    """The abstract top: a depth that could not be derived."""

    __slots__ = ()

    def __repr__(self) -> str:
        return 'UNKNOWN'

    def __bool__(self) -> bool:
        return False


UNK = Unknown()


class Entry:
    """One manifest entry, with its span and its declared stack contract."""

    __slots__ = ('start', 'end', 'stack_args', 'has_stack_args', 'section')

    def __init__(self, record: dict) -> None:
        self.start = int(record['start'], 16)
        self.end = int(record['end'], 16)
        self.has_stack_args = 'stack_args' in record
        self.stack_args = record.get('stack_args', 0)
        self.section = record.get('section', '.text')

    @property
    def expected(self) -> int:
        return 4 + self.stack_args

    def __repr__(self) -> str:
        return f'<Entry 0x{self.start:08X}..0x{self.end:08X} args={self.stack_args}>'


class Image:
    """The original XBE's file-backed bytes, addressed by guest VA."""

    def __init__(self, root: Path) -> None:
        analysis = json.loads((root / 'game' / 'mygame_analysis.json').read_text())
        self._ranges = []
        for section in analysis['sections']:
            base = int(section['virtual_addr'], 16)
            size = section['raw_size']
            self._ranges.append((base, base + size,
                                 int(section['raw_addr'], 16), section.get('name', '')))
        self._ranges.sort()
        self._handle = (root / 'game' / 'default.xbe').open('rb')

    def bytes_at(self, va: int, size: int):
        for base, limit, offset, _ in self._ranges:
            if base <= va < limit:
                self._handle.seek(offset + (va - base))
                return self._handle.read(size)
        return None

    def contains(self, va: int) -> bool:
        return any(base <= va < limit for base, limit, _, _ in self._ranges)

    def is_code(self, va: int) -> bool:
        """True when va is inside an executable, file-backed section."""
        for base, limit, _, name in self._ranges:
            if base <= va < limit:
                return name in ('.text',)
        return False


class Database:
    """The analysis database: spans and detection methods for the whole XBE."""

    def __init__(self, root: Path) -> None:
        self._by_start = {}
        self._intervals = None
        path = root / 'tools' / 'disasm' / 'output' / 'functions.json'
        self.available = path.exists()
        if not self.available:
            return
        for record in json.loads(path.read_text()):
            self._by_start[int(record['start'], 16)] = (
                int(record['end'], 16), record.get('detection_method', ''))

    def span(self, va: int):
        return self._by_start.get(va)

    def is_genuine_entry(self, va: int, manifest_starts) -> bool:
        """Is va an entry in its own right, rather than a folded alias?

        A reviewed manifest entry is an entry by definition.  Otherwise the
        database decides, and a `tail_jump_alias`/`gap_prologue` record is a
        *false* entry -- it resolves at run time to its parent's body.  An
        address the database does not know at all is not claimed as genuine:
        the caller reports it, rather than assuming.
        """
        if va in manifest_starts:
            return True
        record = self._by_start.get(va)
        if record is None:
            return False
        return record[1] not in FOLDED_DETECTION

    # NOTE: a `Database.is_inside_a_genuine_body(va)` used to live here, meaning
    # "is va inside some real function's span at any offset".  It was dead code
    # (never called) AND broken (it called `self.enclosing_span`, which only
    # exists on `Analyzer`), so it would have raised AttributeError the first
    # time anything used it.  It is deleted rather than repaired because the
    # question it asks is the one `Analyzer.enclosing_span(va, exclude_start=...)`
    # already answers, and that is what `evaluate` calls -- including the
    # `exclude_start` that stops an entry from justifying a branch into its own
    # truncated span.  Keeping a second, subtly different copy of that test is
    # how the two would drift apart.


class Analyzer:
    """Reachable stack-depth CFG over one manifest."""

    def __init__(self, image: Image, database: Database, entries) -> None:
        self.image = image
        self.database = database
        self.entries = entries
        self.by_start = {entry.start: entry for entry in entries}
        self._md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        self._md.detail = True
        self._insn_cache: dict = {}
        self._walk_cache: dict = {}
        self._cleanup_cache: dict = {}
        self._cleanup_active: set = set()
        self._intervals = None
        self.limits_hit = 0

    def genuine_intervals(self, exclude_start=None):
        """Sorted spans of bodies that are real functions, for containment tests.

        **The entry under test is excluded.**  Its own declared span is the
        thing being judged, so letting it answer "is this address inside a real
        body?" would make every truncated span self-justifying: `0x00080BD0`
        with the wrong end `0x80C83` would see its own corrected span and call
        the fall-through legitimate.  Every span here belongs to *some other*
        function, which is the only thing that can make a continuation real.
        """
        if self._intervals is None:
            spans = {(entry.start, entry.end) for entry in self.entries}
            for start, (end, method) in self.database._by_start.items():
                if method not in FOLDED_DETECTION:
                    spans.add((start, end))
            self._intervals = sorted(spans)
        if exclude_start is None:
            return self._intervals
        return [span for span in self._intervals if span[0] != exclude_start]

    def enclosing_span(self, va: int, exclude_start=None):
        """The innermost genuine span containing va, or None.

        "Genuine" excludes the database's folded-alias and `gap_prologue`
        records, so a fragment invented by the `push`-after-`ret` pass is not a
        body to compose into.  Innermost wins, because the database's spans
        nest: `0x00032610`'s real `[0x32610, 0x3275F)` sits inside the alias
        record `[0x32610, 0x33800)` that overran through its jump table.
        """
        best = None
        for start, end in self.genuine_intervals(exclude_start):
            if start <= va < end:
                if best is None or (end - start) < (best[1] - best[0]):
                    best = (start, end)
        return best

    # -- decoding ---------------------------------------------------------
    def insn_at(self, va: int):
        if va in self._insn_cache:
            return self._insn_cache[va]
        raw = self.image.bytes_at(va, MAX_INSN_BYTES)
        found = None
        if raw:
            decoded = list(self._md.disasm(raw, va))
            if decoded and decoded[0].address == va:
                found = decoded[0]
        self._insn_cache[va] = found
        return found

    def jump_table(self, insn):
        """Targets of `jmp dword ptr [reg*4 + disp]`, or None if not that shape.

        The table is read from the original image and stops at the first entry
        that is not executable code, so a misread table cannot invent targets.
        `0x00032610` is why this matters: its own jump table at `0x32760`
        contains `C2 26 03 00`, which decodes as a spurious `ret 0x326`, and the
        naive linear decode that followed it is what made the database span the
        whole `0x32610..0x33800` range.
        """
        operands = insn.operands
        if not operands or operands[0].type != capstone.x86.X86_OP_MEM:
            return None
        disp = operands[0].mem.disp & 0xFFFFFFFF
        if disp == 0:
            return None
        data = self.image.bytes_at(disp, MAX_JUMP_TABLE * 4)
        if not data:
            return None
        targets = []
        for offset in range(0, len(data), 4):
            value = int.from_bytes(data[offset:offset + 4], 'little')
            if value == 0 or not self.image.is_code(value):
                break
            targets.append(value)
        return targets or None

    # -- span lookup ------------------------------------------------------
    def span_of(self, va: int):
        """(start, end) of the body owning va, preferring the reviewed manifest."""
        entry = self.by_start.get(va)
        if entry is not None:
            return entry.start, entry.end
        record = self.database.span(va)
        if record is None:
            return None
        return va, record[0]

    # -- the walk ---------------------------------------------------------
    def walk(self, start: int, end: int):
        """Reachable instructions from `start` to `end`.

        Returns `(exits, falloffs, truncated, depths)` where each exit is
        `(site, kind, depth, immediate)` with `kind` in `ret`/`tail`/`terminal`,
        each falloff is `(va, depth)`, and `depths` maps an address to the set of
        depths reaching it (for join conflicts).

        A `call` contributes its callee's own `ret N` immediate, derived by
        walking the callee; an unresolvable callee makes the depth UNKNOWN.  That
        is sound and deliberately pessimistic.  A tempting alternative -- treating
        a call as consuming its argument pushes -- was implemented and
        **discarded**: it cannot tell a prologue save from an argument push, so on
        `0x0007DA30` it produced depth -52 where the truth is 0.  A heuristic that
        cannot distinguish those two is worse than no model, because it reports
        confident nonsense.
        """
        key = (start, end)
        cached = self._walk_cache.get(key)
        if cached is not None:
            return cached
        work = [(start, 0)]
        depths: dict = {}
        exits, falloffs, truncated = [], [], []
        steps = 0
        while work:
            va, depth = work.pop()
            while True:
                steps += 1
                if steps > MAX_WALK_STEPS:
                    self.limits_hit += 1
                    falloffs.append((va, UNK))
                    break
                if va >= end or not self.image.contains(va):
                    falloffs.append((va, depth))
                    break
                seen = depths.setdefault(va, set())
                if depth in seen:
                    break
                seen.add(depth)
                insn = self.insn_at(va)
                if insn is None or va + insn.size > end:
                    truncated.append((va, depth))
                    break
                depth = self._transfer(insn, depth)
                mnemonic = insn.mnemonic
                if mnemonic in ('ret', 'retf'):
                    immediate = None
                    if mnemonic == 'ret':
                        immediate = 0
                        if insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
                            immediate = insn.operands[0].imm & 0xFFFFFFFF
                    exits.append((va, 'ret', depth, immediate))
                    break
                if mnemonic in ('int3', 'hlt', 'ud2', 'iretd', 'iret'):
                    # A terminal that is not a return and not a tail call.  It is
                    # recorded as an exit so the gating rule below can tell "every
                    # exit is a `ret N`" from "every exit I happened to model is a
                    # `ret N`".  Silently dropping it would let a body with a
                    # reachable `int3` on one path and a `ret` on another pass the
                    # gate.
                    exits.append((va, 'terminal', depth, None))
                    break
                if mnemonic == 'call':
                    depth = self._call_depth(insn, depth)
                    va += insn.size
                    continue
                if mnemonic == 'jmp':
                    if insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
                        target = insn.operands[0].imm & 0xFFFFFFFF
                        if start <= target < end:
                            va = target
                            continue
                        exits.append((va, 'tail', depth, target))
                        break
                    targets = self.jump_table(insn)
                    inside = [t for t in (targets or []) if start <= t < end]
                    if inside:
                        for target in inside:
                            work.append((target, depth))
                    else:
                        exits.append((va, 'indirect', depth, None))
                    break
                if mnemonic.startswith('j') and len(mnemonic) > 1:
                    if insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM:
                        target = insn.operands[0].imm & 0xFFFFFFFF
                        if start <= target < end:
                            work.append((target, depth))
                        else:
                            exits.append((va, 'tail', depth, target))
                    va += insn.size
                    continue
                va += insn.size
        result = (exits, falloffs, truncated, depths)
        self._walk_cache[key] = result
        return result

    def _transfer(self, insn, depth):
        """The stack effect of one non-control instruction."""
        if depth is UNK:
            return UNK
        mnemonic = insn.mnemonic
        if mnemonic in ('push', 'pushfd', 'pushf', 'pusha', 'pushal'):
            return depth - 4
        if mnemonic in ('pop', 'popfd', 'popf', 'popa', 'popal'):
            return depth + 4
        if mnemonic == 'enter':
            immediate = insn.operands[0].imm if insn.operands else 0
            return depth - 4 - immediate
        if mnemonic in ('sub', 'add') and len(insn.operands) > 1 \
                and insn.operands[0].type == capstone.x86.X86_OP_REG \
                and insn.reg_name(insn.operands[0].reg) == 'esp' \
                and insn.operands[1].type == capstone.x86.X86_OP_IMM:
            amount = insn.operands[1].imm & 0xFFFFFFFF
            if amount >= 0x80000000:
                amount -= 1 << 32
            return depth - amount if mnemonic == 'sub' else depth + amount
        if mnemonic in ('mov', 'lea', 'and', 'or', 'xchg') \
                and insn.operands and insn.operands[0].type == capstone.x86.X86_OP_REG \
                and insn.reg_name(insn.operands[0].reg) == 'esp':
            # A computed esp is not a value this model can follow, and `leave`
            # (mov esp,ebp; pop ebp) is the same case.  Say so rather than
            # guessing zero.
            return UNK
        if mnemonic == 'leave':
            return UNK
        return depth

    def _call_depth(self, insn, depth):
        """Depth at the instruction after a call.

        A `ret N` pops the return address **and** N more bytes, whatever the
        callee did internally, so the caller's successor depth changes by
        exactly the callee's cleanup -- derived by walking the callee, never
        read from the metadata under test.  Reading it from `stack_args` would
        let a wrong value on one entry launder itself into every caller, which
        is precisely the `0x00074C70` defect: its `stack_args 4` was copied from
        its abutting neighbour, while the body actually tail-jumps to `0x6A770`,
        whose plain `ret` cleans up nothing.
        """
        if depth is UNK:
            return UNK
        if not (insn.operands and insn.operands[0].type == capstone.x86.X86_OP_IMM):
            return UNK
        cleanup = self.cleanup(insn.operands[0].imm & 0xFFFFFFFF)
        return UNK if cleanup is UNK else depth + cleanup

    def cleanup(self, va: int):
        """Bytes a call to `va` removes from the caller beyond the return address.

        This is the primitive the whole model rests on, and it is deliberately
        **not** the callee's full delta: a callee's internal calls are resolved
        through this same function, so the recursion stops at `ret` immediates
        instead of cascading a single unresolved indirect call through every
        caller in the manifest.
        """
        cached = self._cleanup_cache.get(va)
        if cached is not None:
            return cached
        if va in self._cleanup_active:
            return UNK                       # recursive; not a value
        span = self.span_of(va)
        if span is None:
            return UNK
        start, end = span
        if end <= start or end - start > MAX_SPAN_BYTES:
            return UNK
        self._cleanup_active.add(va)
        try:
            result = self._cleanup_of_span(start, end)
        finally:
            self._cleanup_active.discard(va)
        self._cleanup_cache[va] = result
        return result

    def _cleanup_of_span(self, start: int, end: int):
        exits, falloffs, truncated, _ = self.walk(start, end)
        if truncated:
            return UNK
        candidates = set()
        for _, kind, depth, immediate in exits:
            if kind == 'ret':
                if immediate is None:
                    return UNK
                candidates.add(immediate)
            elif kind == 'tail':
                if depth is UNK:
                    return UNK
                tail = self.cleanup(immediate)
                if tail is UNK:
                    return UNK
                candidates.add(depth + tail)
            else:
                return UNK
        for va, depth in falloffs:
            if va == end and depth is not UNK:
                tail = self.cleanup(va)
                if tail is UNK:
                    return UNK
                candidates.add(depth + tail)
            else:
                return UNK
        if len(candidates) != 1:
            return UNK
        return next(iter(candidates))

    # -- the verdict ------------------------------------------------------
    def evaluate(self, entry: Entry):
        """Return `(verdict, code, detail)` for one manifest entry.

        The order matters: the more specific fault is reported first, so a
        finding names the actual defect instead of a downstream symptom.

        **Only `STACK_ARGS` is `DEFECT`.**  The module docstring carries the
        measured argument; in one line: it is the only class here that is a
        property of *this entry's own bytes* and does not depend on the depth
        arithmetic being right about any callee.  Everything else is reported
        with a precise code as `SUSPICIOUS`, so the population stays visible and
        is repaired rather than baselined away.
        """
        start, end = entry.start, entry.end
        if end <= start:
            return DEFECT, 'BAD_RANGE', f'end 0x{end:08X} is not past start 0x{start:08X}'
        exits, falloffs, truncated, depths = self.walk(start, end)

        if truncated:
            va = truncated[0][0]
            return SUSPICIOUS, 'TRUNCATED', (
                f'reachable instruction at 0x{va:08X} crosses end 0x{end:08X}')

        # Is the whole walk resolved?  A gating `STACK_ARGS` verdict requires it:
        # an unresolved callee cleanup could be why the depth is not 0.
        resolved = not any(UNK in seen for seen in depths.values())
        if resolved:
            for _, kind, depth, immediate in exits:
                if kind == 'ret' and (depth is UNK or immediate is None):
                    resolved = False
                    break
                if kind == 'tail' and (depth is UNK or self.cleanup(immediate) is UNK):
                    resolved = False
                    break
                if kind not in ('ret', 'tail'):
                    resolved = False
                    break
            if resolved:
                for _, depth in falloffs:
                    if depth is UNK:
                        resolved = False
                        break

        if resolved:
            for site, kind, depth, immediate in exits:
                if kind == 'ret' and depth != 0:
                    return SUSPICIOUS, 'RET_DEPTH', (
                        f'`ret` at 0x{site:08X} reached at depth {depth:+d}; '
                        f'a return needs depth 0')

        # A branch out of the span, at a nonzero depth, to an address that is
        # neither an entry nor inside any function: the span may have cut its own
        # shared epilogue and the target is a bare fragment.  This is the
        # `0x000BB7B0` class -- but it is also how `check-span-exits.py`'s
        # 360-finding CUT-TARGET population presents, so it is reported rather
        # than failed.
        for site, kind, depth, immediate in exits:
            if kind != 'tail' or depth is UNK or depth == 0:
                continue
            if self.database.is_genuine_entry(immediate, self.by_start):
                continue
            if self.enclosing_span(immediate, exclude_start=start) is not None:
                continue
            return SUSPICIOUS, 'CUT_EPILOGUE', (
                f'0x{site:08X} jumps to 0x{immediate:08X} at depth {depth:+d}, '
                f'and 0x{immediate:08X} is not an entry and is not inside any '
                f'function: the span may have cut its own epilogue')

        # A fall-through off the declared end with no function there: the body
        # has no exit of its own, so the wrapper sees whatever the partial
        # prologue left.  This is the measured `0x00080BD0 esp -0x40` and
        # `0x0007DA30` class.  It is reported rather than failed because the
        # correct end needs per-entry boundary adjudication.
        for va, depth in falloffs:
            if va == end:
                if self.database.is_genuine_entry(va, self.by_start):
                    continue
                if self.enclosing_span(va, exclude_start=start) is not None:
                    continue
                return SUSPICIOUS, 'FALL_OFF_END', (
                    f'control reaches end 0x{end:08X} with no return, and no '
                    f'function begins there')
            return SUSPICIOUS, 'FALL_OFF_END', (
                f'control leaves the span at 0x{va:08X} (end 0x{end:08X})')

        # --- the gating class -------------------------------------------------
        # **At a reachable `ret N` reached at depth 0, `stack_args` must be N.**
        #
        # `scripts/recover-functions.py` asserts
        # `g_esp == before_stack + 4 + stack_args` after the body, and a `ret N`
        # leaves `esp = entry_esp + 4 + N - d`, so in general
        # `stack_args = N - d`.  The gate uses only the **`d == 0`** case, and that
        # restriction is load-bearing rather than conservative-for-its-own-sake:
        #
        #   * `d == 0` is a claim about one concrete path through *this* entry's
        #     own bytes.  It says the body's own pushes and pops balance, which is
        #     exactly the situation in which `stack_args = N` with no reference to
        #     any callee.  It is checkable and it is what the twenty repaired
        #     defects and the four below all look like.
        #   * the general `N - d` form is **not** safe to gate on, and gating it
        #     was measured wrong: it fired on 18 entries, including
        #     `0x0001C000` ("must be -292") and `0x00022070` ("must be -44").
        #     Those are over-wide spans whose walk wanders into a neighbouring
        #     function and accumulates a large bogus depth, so a nonzero `d` is
        #     only as trustworthy as the span extent -- which is the very thing
        #     this validator is not allowed to assume.  A negative `stack_args` is
        #     also not representable, which is the tell that the depth is wrong
        #     rather than the manifest.
        #
        # **An earlier revision required *every* exit to be at depth 0, and that
        # was a real false-negative.**  One `ret` site is often reached by several
        # paths; a body whose `ret 16` is reached once at depth 0 and once at
        # UNKNOWN (through an indirect call) failed the all-depths test and was
        # reported merely `UNKNOWN/PARTIAL`, hiding a live defect behind an
        # unrelated unresolved path.  Measured: the all-depths rule found 20
        # defects, this rule finds those 20 plus `0x00021010`, `0x000F4FF0`,
        # `0x00102490` and `0x00152BC0`, whose generated bodies contradict their
        # own wrappers (`0x00021010` checks `+4` and emits `esp += 20`).  This was
        # found twice independently, by adversarial review and by re-deriving the
        # rule, and both times the same four addresses.
        #
        # `0x001BCB14` is the control that this rule must not touch: a single
        # `ret 4` declared as 12, correct because its depth is `-8`.  Its depth is
        # UNKNOWN, so it is never gated, and it is run-verified in 106 archived
        # runs.  Requiring `d == 0` is what keeps it safe.
        if not truncated:
            at_zero = [(s, i) for s, k, d, i in exits
                       if k == 'ret' and d == 0 and i is not None]
            implied = {i for _, i in at_zero}
            if len(implied) == 1:
                immediate = next(iter(implied))
                if immediate != entry.stack_args:
                    site = next(s for s, i in at_zero if i == immediate)
                    return DEFECT, 'STACK_ARGS', (
                        f'`ret {immediate:#x}` at 0x{site:08X} is reached at '
                        f'depth 0, so stack_args must be {immediate}, not '
                        f'{entry.stack_args}')
            elif len(implied) > 1:
                return SUSPICIOUS, 'EXIT_DISAGREE', (
                    f'depth-0 exits return different immediates '
                    f'{sorted(implied)}; the declared {entry.stack_args} is not '
                    f'contradicted by a single path')

        # --- reported, not gated: a tail target whose cleanup disagrees --------
        # A `jmp` out of the span is a tail call: control leaves through the
        # *target's* `ret N`, so the wrapper's `4 + stack_args` contract is the
        # target's cleanup, not this body's.  When the target's own cleanup is a
        # known value `T` and `stack_args != T`, the declared value is wrong --
        # but the argument runs through another function's body, so it is
        # reported rather than gated.  This is what catches `0x00074C70`, whose
        # `jmp 0x6A770` reaches a plain `ret` (T = 0) against a declared 4.
        for _, kind, depth, immediate in exits:
            if kind != 'tail' or depth is UNK:
                continue
            target_cleanup = self.cleanup(immediate)
            if target_cleanup is UNK:
                continue
            total = depth + 4 + target_cleanup
            if total != entry.expected:
                return SUSPICIOUS, 'TAIL_CLEANUP', (
                    f'the tail call to 0x{immediate:08X} cleans up '
                    f'{target_cleanup} and the path leaves at depth {depth:+d}, '
                    f'totalling {total}, but stack_args {entry.stack_args} '
                    f'implies {entry.expected}')

        # --- reported: the whole walk resolved and the unique total disagrees --
        # Rests on the callee summaries being right as well as this entry's own
        # bytes, so it is reported rather than gated.
        if resolved:
            totals = set()
            for _, kind, depth, immediate in exits:
                if kind == 'ret':
                    totals.add(depth + 4 + immediate)
                elif kind == 'tail':
                    totals.add(depth + 4 + self.cleanup(immediate))
            if len(totals) == 1:
                total = next(iter(totals))
                if total != entry.expected:
                    return SUSPICIOUS, 'TOTAL_MISMATCH', (
                        f'every reachable exit resolves and totals {total}, but '
                        f'stack_args {entry.stack_args} implies {entry.expected}')
            elif len(totals) > 1:
                return SUSPICIOUS, 'EXIT_DISAGREE', (
                    f'reachable exits total {sorted(totals)}; no single contract '
                    f'fits them all')

        conflicts = [va for va, seen in depths.items()
                     if len([d for d in seen if d is not UNK]) > 1]
        if conflicts:
            va = min(conflicts)
            known = sorted(d for d in depths[va] if d is not UNK)
            return SUSPICIOUS, 'JOIN_CONFLICT', (
                f'0x{va:08X} is reached at depths {known}; the stack cannot be '
                f'two things at one instruction')

        if not resolved:
            return UNKNOWN, 'PARTIAL', (
                f'{len(exits)} exit site(s); the declared contract holds on every '
                f'one this model could resolve')
        if not exits:
            return UNKNOWN, 'NO_RESOLVABLE_EXIT', 'no reachable exit'
        return PROVED, 'OK', ''


# --- controls -------------------------------------------------------------
# Each case is a **pre-fix** span and metadata, because the manifest now carries
# the corrections and the defect is no longer present to find.  The required
# verdict is the one that changed when the defect was fixed; a case whose
# corrected form is also DEFECT would not discriminate, so `good` is asserted
# clean as the negative half of the same control.
#
# `verdict` and `code` are both asserted, not just "is it bad".  A control that
# only checked `!= PROVED` would still pass if the detector reported the *wrong*
# defect, which is how a validator quietly loses the sensitivity it was built
# for.  The non-gating classes are controls too: they do not fail the build, so
# this self-check is the only thing that keeps their detection alive.
CONTROLS = (
    {
        'name': '0x00080BD0 end 0x80C83 (the four-way join point), args 4',
        'bad': {'start': '0x00080BD0', 'end': '0x00080C83', 'stack_args': 4},
        'good': {'start': '0x00080BD0', 'end': '0x00081853', 'stack_args': 0},
        'verdict': SUSPICIOUS, 'code': 'CUT_EPILOGUE',
        'why': 'the span stops at a join point, so the body never reaches its '
               'epilogue; the run measured esp -0x40.  The branch to 0x80C83 '
               'leaves the span at depth -0x40 to an address no function owns',
    },
    {
        'name': '0x0007DA30 end 0x7DA84, args 0 (truncated; hides `ret 4`)',
        'bad': {'start': '0x0007DA30', 'end': '0x0007DA84', 'stack_args': 0},
        'good': {'start': '0x0007DA30', 'end': '0x0007DAD6', 'stack_args': 4},
        'verdict': SUSPICIOUS, 'code': 'FALL_OFF_END',
        'why': 'the truncated body falls off the end with no epilogue',
    },
    {
        'name': '0x00152BC0 args 8 (the value copied from the swallowed neighbour)',
        'bad': {'start': '0x00152BC0', 'end': '0x00152E30', 'stack_args': 8},
        'good': {'start': '0x00152BC0', 'end': '0x00152DE0', 'stack_args': 24},
        'verdict': DEFECT, 'code': 'STACK_ARGS',
        'why': 'all six reachable exits are `ret 0x18` and 0x00152DCF is at '
               'depth 0, so the declared 8 is wrong; the 8 belongs to the '
               'swallowed function at 0x00152DE0, whose own `ret 8` is correct '
               'for it.  This is the 0x00074C70 pattern of a value inherited '
               'from a different body',
    },
    {
        'name': '0x00021010 args 0 (the key was ABSENT, so the wrapper used 0)',
        'bad': {'start': '0x00021010', 'end': '0x00021104'},
        'good': {'start': '0x00021010', 'end': '0x00021104', 'stack_args': 16},
        'verdict': DEFECT, 'code': 'STACK_ARGS',
        'why': 'the only reachable exit is `ret 0x10` at 0x00021101 at depth 0, '
               'so 16 is forced; the ABSENT key is exactly as wrong as an '
               'explicit 0 and the control omits it to prove that',
    },
    {
        'name': '0x000F4FF0 args 0 (two depth-0 paths and two unresolved ones)',
        'bad': {'start': '0x000F4FF0', 'end': '0x000F5310', 'stack_args': 0},
        'good': {'start': '0x000F4FF0', 'end': '0x000F5310', 'stack_args': 4},
        'verdict': DEFECT, 'code': 'STACK_ARGS',
        'why': 'the same `ret 4` site is reached at depth 0 by one path and at '
               'UNKNOWN by another; the earlier all-depths rule let the '
               'UNKNOWN path hide the defect, and this control is what pins '
               'the per-path form',
    },
    {
        'name': '0x0007DA30 full span 0x7DAD6 but args 0 (the hidden `ret 4`)',
        'bad': {'start': '0x0007DA30', 'end': '0x0007DAD6', 'stack_args': 0},
        'good': {'start': '0x0007DA30', 'end': '0x0007DAD6', 'stack_args': 4},
        # Not a `DEFECT`, and the reason is a stated limit rather than a
        # misreading: this body's only exit is `ret 4` reached at UNKNOWN depth,
        # because it makes three `call dword ptr [...]` indirect calls whose
        # callees cannot be resolved, and it has NO path reaching that `ret` at
        # depth 0.  So `stack_args = N - d` cannot be evaluated for it, which is
        # exactly the line between this entry and `0x000F4FF0` above: that one
        # has a depth-0 path and is gated; this one does not and is not.  The
        # wrong value is still *not* `PROVED`, which is what the assertion below
        # requires -- and the defect itself is caught by the truncated-span
        # control above, which is the form the run actually hit.
        'verdict': UNKNOWN, 'code': 'PARTIAL',
        'why': 'the corrected span rejects the old cleanup value, but only as '
               'UNKNOWN: its indirect calls leave the pre-ret depth unresolved '
               'on every path',
    },
    {
        'name': '0x00074C70 args 4 (the body has no `ret` of its own)',
        'bad': {'start': '0x00074C70', 'end': '0x00074CB0', 'stack_args': 4},
        'good': {'start': '0x00074C70', 'end': '0x00074CB0', 'stack_args': 0},
        'verdict': SUSPICIOUS, 'code': 'TAIL_CLEANUP',
        'why': 'the body tail-jumps to 0x6A770, whose plain `ret` cleans up '
               'nothing, so the total is 4 against a declared 8; the tail is why '
               'the depth-independent class does not apply',
    },
    {
        'name': '0x000BB7B0 end 0xBBA04 (cuts the shared tail-merged epilogue)',
        'bad': {'start': '0x000BB7B0', 'end': '0x000BBA04', 'stack_args': 0},
        'good': {'start': '0x000BB7B0', 'end': '0x000BBA19', 'stack_args': 0},
        'verdict': SUSPICIOUS, 'code': 'CUT_EPILOGUE',
        'why': 'three in-span branches and jump-table slot 5 target 0xBBA17, '
               'which is outside the old span; each leaves at depth -4 to an '
               'address no function owns, which is why the run died on a trap '
               'stub there',
    },
    {
        'name': '0x000307A0 end 0x307F8 (cuts `test esi,esi` in half)',
        'bad': {'start': '0x000307A0', 'end': '0x000307F8', 'stack_args': 4},
        'good': {'start': '0x000307A0', 'end': '0x00030800', 'stack_args': 4},
        'verdict': SUSPICIOUS, 'code': 'TRUNCATED',
        'why': 'the cut `test esi,esi` is reachable and the fall-through lands '
               'mid-instruction',
    },
    {
        'name': '0x000246E0 declared args 0 (its own body ends `pop esi; ret 4`)',
        'bad': {'start': '0x000246E0', 'end': '0x00024700', 'stack_args': 0},
        'good': {'start': '0x000246E0', 'end': '0x00024700', 'stack_args': 4},
        'verdict': DEFECT, 'code': 'STACK_ARGS',
        'why': 'found by the validator on the live manifest: the body is '
               '`push esi; ...; pop esi; ret 4`, so the wrapper expectation of '
               '+4 is one return address short of the real +8',
    },
)


def selfcheck(analyzer: Analyzer, manifest_starts) -> int:
    """Positive and negative controls for the detector's own decision procedure."""
    failures = 0
    print('self-check: the detector against the historical defects, at their '
          'pre-fix spans and metadata')
    for case in CONTROLS:
        bad = Entry(case['bad'])
        verdict, code, detail = analyzer.evaluate(bad)
        ok = verdict == case['verdict'] and code == case['code']
        failures += 0 if ok else 1
        print('  %-5s %s' % ('PASS' if ok else 'FAIL', case['name']))
        print('        -> %s %s: %s' % (verdict, code, detail))
        if not ok:
            print('        expected %s/%s.  %s'
                  % (case['verdict'], case['code'], case['why']))

        good = Entry(case['good'])
        gverdict, gcode, gdetail = analyzer.evaluate(good)
        gok = gverdict != DEFECT
        failures += 0 if gok else 1
        print('  %-5s negative half: the corrected entry is not DEFECT'
              % ('PASS' if gok else 'FAIL'))
        print('        -> %s %s: %s' % (gverdict, gcode, gdetail))

    if failures:
        print('self-check FAILED: %d of %d control assertions failed; the '
              'detector has lost the sensitivity this gate rests on.'
              % (failures, 2 * len(CONTROLS)))
        return 1
    print('self-check passed: all %d historical defects are reported with the '
          'expected verdict and code at their pre-fix spans, and every corrected '
          'entry is clean.' % len(CONTROLS))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--json', action='store_true',
                        help='emit machine-readable results')
    parser.add_argument('--selfcheck', action='store_true',
                        help='positive/negative controls: replay the historical '
                             'defects at their pre-fix spans')
    parser.add_argument('--baseline', default=None, metavar='PATH',
                        help='JSON list of entry starts already DEFECT when this '
                             'gate was introduced; only a DEFECT outside it fails')
    parser.add_argument('--write-baseline', default=None, metavar='PATH',
                        help='record the current DEFECT entry starts as the baseline')
    parser.add_argument('--limit', type=int, default=40,
                        help='how many findings of each kind to print')
    parser.add_argument('--show', default='DEFECT',
                        help='comma-separated verdicts to list in full; default '
                             'DEFECT.  Use --show-suspicious to see the reported '
                             'classes, which do not fail the gate.')
    parser.add_argument('--show-suspicious', action='store_true',
                        help='list every SUSPICIOUS finding (equivalent to '
                             '--show DEFECT,SUSPICIOUS)')
    parser.add_argument('--manifest', default=None, metavar='PATH',
                        help='validate this manifest instead of the committed one')
    args = parser.parse_args()

    image = Image(ROOT)
    database = Database(ROOT)
    manifest_path = Path(args.manifest) if args.manifest \
        else ROOT / 'config' / 'recovered-functions.json'
    records = json.loads(manifest_path.read_text())
    entries = [Entry(record) for record in records]
    analyzer = Analyzer(image, database, entries)

    if args.selfcheck:
        return selfcheck(analyzer, set(analyzer.by_start))

    if not database.available:
        print('FAIL: tools/disasm/output/functions.json is missing, so entry '
              'identity cannot be decided; refusing to pass', file=sys.stderr)
        return 2

    results = []
    counts: dict = {}
    for entry in entries:
        verdict, code, detail = analyzer.evaluate(entry)
        counts[verdict] = counts.get(verdict, 0) + 1
        results.append({'start': entry.start, 'end': entry.end,
                        'stack_args': entry.stack_args,
                        'verdict': verdict, 'code': code, 'detail': detail})

    defects = [r for r in results if r['verdict'] == DEFECT]

    if args.write_baseline:
        Path(args.write_baseline).write_text(
            json.dumps(sorted('0x%08X' % r['start'] for r in defects), indent=2) + '\n',
            encoding='utf-8')
        print('baseline written: %d DEFECT entry start(s) -> %s'
              % (len(defects), args.write_baseline))
        return 0

    if args.json:
        print(json.dumps({'counts': counts, 'results': results}, indent=1))
        return 0

    show = {name.strip().upper() for name in args.show.split(',') if name.strip()}
    if args.show_suspicious:
        show.add(SUSPICIOUS)
    print('entries checked: %d' % len(entries))
    print('verdicts: ' + ', '.join('%d %s' % (v, k)
                                   for k, v in sorted(counts.items(), key=lambda kv: -kv[1])))
    codes: dict = {}
    for record in results:
        if record['verdict'] in (DEFECT, SUSPICIOUS):
            key = (record['verdict'], record['code'])
            codes[key] = codes.get(key, 0) + 1
    if codes:
        print('findings: ' + ', '.join('%d %s/%s' % (v, k[0], k[1])
                                       for k, v in sorted(codes.items())))
    if SUSPICIOUS not in show and counts.get(SUSPICIOUS):
        print('note: %d SUSPICIOUS finding(s) not listed; they do not fail the '
              'gate.  Use --show-suspicious to see them.' % counts[SUSPICIOUS])
    for verdict in (DEFECT, SUSPICIOUS, UNKNOWN):
        if verdict not in show:
            continue
        rows = [r for r in results if r['verdict'] == verdict]
        if not rows:
            continue
        print('\n%s (%d):' % (verdict, len(rows)))
        for record in rows[:args.limit]:
            print('  0x%08X-0x%08X args=%d  %s: %s'
                  % (record['start'], record['end'], record['stack_args'],
                     record['code'], record['detail']))
        if len(rows) > args.limit:
            print('  ... %d more' % (len(rows) - args.limit))

    if analyzer.limits_hit:
        print('\nnote: %d walk(s) hit the %d-step bound and were reported UNKNOWN'
              % (analyzer.limits_hit, MAX_WALK_STEPS))

    if args.baseline:
        try:
            known = set(json.loads(Path(args.baseline).read_text(encoding='utf-8')))
        except FileNotFoundError:
            print('%s: no baseline file; treating every DEFECT as new'
                  % args.baseline, file=sys.stderr)
            known = set()
        except Exception as exc:                       # noqa: BLE001
            print('%s: unreadable baseline (%s); refusing to pass'
                  % (args.baseline, exc), file=sys.stderr)
            return 2
        current = {'0x%08X' % r['start'] for r in defects}
        new = sorted(current - known)
        stale = sorted(known - current)
        if stale:
            print('\nnote: %d baseline entr(y/ies) are no longer DEFECT and can be '
                  'removed: %s' % (len(stale), ', '.join(stale[:8])))
        if new:
            print('\nFAIL: %d NEW stack-depth DEFECT(s): %s'
                  % (len(new), ', '.join(new)))
            return 1
        print('\nPASS: no new stack-depth DEFECTs (%d known and reviewed)'
              % len(known))
        return 0

    if defects:
        print('\nFAIL: %d stack-depth DEFECT(s) and no --baseline given'
              % len(defects))
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
