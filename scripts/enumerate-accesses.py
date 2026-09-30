"""Enumerate guest-VA accesses in the original XBE, normalised to `uint32`.

Plan T9.  This tool answers one question without spelling:

    which instructions in the original Xbox executable reference guest virtual
    address V?

Why it exists
-------------
`AGENTS.md` "Generated-source rules" records the failure this tool prevents.  The
lifter spells **one** guest address **two** ways in generated C:

  * an `A1`/`A3` moffs operand (always `eax`) is emitted **hex** -- `MEM32(0xFE820010u)`;
  * a ModRM `disp32` operand is emitted **signed decimal** -- `MEM32(-25034736)`.

`-25034736 & 0xFFFFFFFF == 0xFE820010`: the same guest VA.  Every address at or above
`0x80000000` is exposed to this, which covers all MMIO and the contiguous window.  A
`PIO_FREE` site list built by grepping one spelling covered **10 of 28** sites
(`docs/jsrf-technical-record.md` section 4).

So this tool never greps generated code for a *spelling*.  It enumerates from the
**original XBE instruction stream**, normalises every operand to `uint32`, and only then
reconciles against generated C **by value**.  Generated code is a cross-check here, never
the source of the population.

Method (and what it cannot see)
-------------------------------
Two independent enumerations are run and cross-checked:

1. **Recursive descent** from the XBE entry point plus every dispatch seed, following
   direct `call`/`jmp`/`jcc` targets and fall-through, over a visited set.  This is the
   control-flow-following method `docs/agent-workflow.md` section 6.1 requires; a linear
   sweep is inadmissible for completeness because it drifts at the first data island.
2. **Raw-byte fallback**: an alignment-independent scan for the little-endian dword.
   This is an **existence** check.  It is deliberately not filtered by reachability,
   because an operand that a misaligned decode renders as a plausible unrelated
   instruction is still a real site.

A site is **CONFIRMED** when a real instruction that references the value by operand has
its start in the recursive-descent visited set.  Sites the raw scan finds but the walk did
not reach are reported as **UNREACHED**, never silently dropped and never counted as
confirmed.

WHAT THIS METHOD CANNOT SEE -- printed on every run, not just here:

  * register-indirect accesses (`mov eax,[ebx]`) -- no literal to find;
  * computed addresses (`lea eax,[ebx+ecx*4]` then a load through `eax`);
  * table-driven accesses (a function pointer or data pointer read from a table and then
    dereferenced), including every vtable dispatch;
  * a literal address that is materialised by arithmetic rather than by one operand;
  * code in a section the walk did not enter, or reached only through an indirect branch;
  * any instruction whose start the walk never visited *and* whose operand the raw scan
    cannot separate from data (a coincidental dword match is reported as UNREACHED, not
    resolved).

Known-answer controls
---------------------
Every run prints its own controls and exits nonzero if any fails.  The values come from
the tool, never from a hand transcription.

Run:  python -X utf8 scripts/enumerate-accesses.py
      python -X utf8 scripts/enumerate-accesses.py --value 0xFE820010
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

UINT32 = 0xFFFFFFFF

# ---------------------------------------------------------------- known-answer constants
#
# `docs/jsrf-technical-record.md` section 4 (`PIO_FREE`): 28 direct reads, "reconciled by
# normalised value: 10 hex-spelled + 18 signed-decimal-spelled sites".
# `plan-jsrf-bare-minimum.md` T9: "reproduces `PIO_FREE` = 28 sites (10 hex + 18 decimal
# spellings) and the vtable base = 3 references (TR section 6)".

PIO_FREE_VA = 0xFE820010
VTABLE_BASE_VA = 0x001E0F00

EXPECT_PIO_SITES = 28
EXPECT_PIO_HEX = 10
EXPECT_PIO_DECIMAL = 18
EXPECT_VTABLE_REFS = 3

# Negative control: adjacent to PIO_FREE, and present nowhere as an operand.
NEGATIVE_CONTROL_VA = 0xFE820011

# Transposition control: the recorded historical error (plan T10) swapped these two.
# They must be distinguishable, and the tool must say *why*.
TRANSPOSITION_LEFT = 0x00193D62
TRANSPOSITION_RIGHT = 0x00193D96

# x86 encodings of a direct absolute 32-bit memory operand.
ENC_A1_MOFFS = "A1_moffs32"
ENC_A3_MOFFS = "A3_moffs32"
ENC_MODRM = "modrm"

# Where a value can sit inside an instruction.
FORM_MEM_ABSOLUTE = "mem-absolute"
FORM_MEM_DISPLACEMENT = "mem-displacement"
FORM_IMMEDIATE = "immediate"
FORM_CODE_TARGET = "code-target"

# Instruction mnemonics whose immediate operand is a branch target, not an access.
BRANCH_MNEMONICS = frozenset({
    "call", "jmp", "ja", "jae", "jb", "jbe", "jc", "jcxz", "je", "jecxz", "jg", "jge",
    "jl", "jle", "jna", "jnae", "jnb", "jnbe", "jnc", "jne", "jng", "jnge", "jnl", "jnle",
    "jno", "jnp", "jns", "jnz", "jo", "jp", "jpe", "jpo", "js", "jz", "loop", "loope",
    "loopne",
})

MAX_BACK = 8  # longest x86-32 instruction whose operand ends the encoding

# `MEM8/16/32( literal )` in generated C.  Both spellings are matched by construction.
GEN_LITERAL = re.compile(r"MEM(?:8|16|32)\(\s*(-?\d+|0[xX][0-9a-fA-F]+)\s*u?\s*\)")

# `{ 0x0014CF20u, sub_0014CF20 },` in `recomp_dispatch.c`.
DISPATCH_ENTRY = re.compile(r"\{\s*0x([0-9A-Fa-f]{8})\s*u?\s*,")

TERMINATORS = frozenset({
    "ret", "retf", "iret", "iretd", "hlt", "int3", "ud0", "ud1", "ud2",
})


class EnumerateError(Exception):
    """An input or operand form that cannot be enumerated.  Always fatal, never skipped."""


def u32(value: int) -> int:
    """Normalise any operand value to an unsigned 32-bit guest value.

    This is the whole point of the tool.  A capstone displacement is a SIGNED Python int,
    so `-25034736` and `0xFE820010` are the same guest VA and must compare equal.
    """
    return value & UINT32


def hex32(value: int) -> str:
    """Every reported address is `0x%08X` of the masked value."""
    return f"0x{u32(value):08X}"


# --------------------------------------------------------------------------------- image


class XbeImage:
    """The original XBE, addressed by guest virtual address.

    The section map comes from `game/mygame_analysis.json` (`virtual_addr`, `raw_addr`,
    `raw_size`), the same mapping `scripts/inspect-jsrf.py` uses.
    """

    def __init__(self, path: Path, sections: list[dict]):
        if not isinstance(sections, list) or not sections:
            raise EnumerateError("analysis has no sections")
        self.path = path
        try:
            self.raw = path.read_bytes()
        except OSError as exc:
            raise EnumerateError(f"cannot read {path}: {exc}") from exc
        self.sections: list[dict] = []
        for s in sections:
            for key in ("name", "virtual_addr", "raw_addr", "raw_size"):
                if key not in s:
                    raise EnumerateError(f"section missing {key!r}: {s!r}")
            self.sections.append({
                "name": s["name"],
                "va": int(s["virtual_addr"], 16),
                "raw": int(s["raw_addr"], 16),
                "size": int(s["raw_size"]),
                "executable": bool(s.get("executable", True)),
            })

    @classmethod
    def load(cls, xbe: Path, analysis: Path) -> "XbeImage":
        try:
            data = json.loads(analysis.read_text(encoding="utf-8"))
            sections = data["sections"]
        except (OSError, ValueError, KeyError) as exc:
            raise EnumerateError(f"cannot read analysis {analysis}: {exc}") from exc
        image = cls(xbe, sections)
        image.analysis = data
        return image

    @property
    def entry_point(self) -> int:
        raw = getattr(self, "analysis", {}).get("entry_point")
        if raw is None:
            raise EnumerateError("analysis has no entry_point")
        return u32(int(raw, 16))

    def section_of(self, va: int) -> dict | None:
        va = u32(va)
        for s in self.sections:
            if s["va"] <= va < s["va"] + s["size"]:
                return s
        return None

    def read(self, va: int, length: int) -> bytes:
        """Bytes at `va`, clamped to the containing section (never crossing into another)."""
        s = self.section_of(va)
        if s is None:
            return b""
        off = s["raw"] + (u32(va) - s["va"])
        available = s["va"] + s["size"] - u32(va)
        return self.raw[off:off + min(length, available)]

    def raw_occurrences(self, value: int) -> list[tuple[int, str]]:
        """Every VA of a little-endian dword equal to `value`, in every section.

        Alignment-independent: the needle is searched at every byte offset, so an unaligned
        immediate (as `mov dword ptr [esi], imm32` produces) is found.  This is the
        existence check, not the completeness claim.
        """
        needle = u32(value).to_bytes(4, "little")
        hits: list[tuple[int, str]] = []
        for s in self.sections:
            blob = self.raw[s["raw"]:s["raw"] + s["size"]]
            at = blob.find(needle)
            while at != -1:
                hits.append((s["va"] + at, s["name"]))
                at = blob.find(needle, at + 1)
        return sorted(hits)


# -------------------------------------------------------------------------------- decode


def new_disassembler() -> capstone.Cs:
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


def decode_one(image: XbeImage, va: int, md: capstone.Cs):
    """Decode exactly one instruction starting at `va`, or None if nothing decodes."""
    buf = image.read(va, 16)
    if not buf:
        return None
    for insn in md.disasm(buf, u32(va), count=1):
        return insn
    return None


# Legacy (non-REX) instruction prefixes in 32-bit mode.
LEGACY_PREFIXES = frozenset({0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65,
                             0x66, 0x67, 0xF0, 0xF2, 0xF3})

# Opcodes whose operand is an absolute `moffs32` with no ModRM byte.
MOFFS_OPCODES = frozenset({0xA0, 0xA1, 0xA2, 0xA3})


def modrm_offset(code: bytes) -> int | None:
    """The index of the ModRM byte in an instruction, or None if there is not one.

    Skips legacy prefixes and a two-byte `0F` opcode.  This is enough to locate the ModRM
    byte for every instruction form this tool reports a displacement for.
    """
    i = 0
    while i < len(code) and code[i] in LEGACY_PREFIXES:
        i += 1
    if i >= len(code):
        return None
    if code[i] == 0x0F:
        i += 2
    else:
        i += 1
    return i if i < len(code) else None


def displacement_width(code: bytes) -> int:
    """The encoded displacement width in BYTES: 0, 1 or 4.

    Derived from the ModRM `mod`/`r/m` fields, never guessed from the value.  Guessing from
    the value is wrong in exactly the case that matters: a `disp32` of `0x40` looks like a
    `disp8` if you only look at how small it is.
    """
    if not code:
        return 0
    if code[0] in MOFFS_OPCODES:
        return 4
    off = modrm_offset(code)
    if off is None:
        return 0
    modrm = code[off]
    mod, rm = modrm >> 6, modrm & 0b111
    if mod == 0b01:
        return 1
    if mod == 0b10:
        return 4
    if mod == 0b00:
        if rm == 0b101:
            return 4                      # disp32, no base
        if rm == 0b100 and off + 1 < len(code):
            if (code[off + 1] & 0b111) == 0b101:
                return 4                  # SIB with a disp32 base
        return 0                          # no displacement
    return 0                              # mod == 0b11: register operand, no memory


def operand_refs(insn) -> list[dict]:
    """Every operand of `insn` that carries a 32-bit value, normalised to `uint32`.

    Structured capstone operands only -- `op_str` is never parsed, because a spelling is
    exactly what this tool must not depend on.

    Three forms are distinguished:

      * `mem-absolute`     -- `[disp32]`, no base and no index.  A genuine address reference.
      * `mem-displacement` -- `[base + index*scale + disp]`.  The displacement is an
        OFFSET, not an address.  Reported so the operand form is covered, never counted as
        a site for the displacement value.
      * `immediate`        -- `push imm32`, `mov reg, imm32`.  An address reference.
      * `code-target`      -- the immediate of a branch.  CODE, not a data access: `call
        0x0014CF20` references a function, and counting it as a guest access would make
        every call target a site.
    """
    is_branch = insn.mnemonic in BRANCH_MNEMONICS
    refs: list[dict] = []
    for op in insn.operands:
        if op.type == capstone.x86.X86_OP_MEM:
            mem = op.mem
            value = u32(mem.disp)
            if mem.base == 0 and mem.index == 0:
                refs.append({"form": FORM_MEM_ABSOLUTE, "value": value,
                             "disp": mem.disp, "base": None, "index": None, "scale": 0})
            else:
                refs.append({"form": FORM_MEM_DISPLACEMENT, "value": value,
                             "disp": mem.disp,
                             "base": insn.reg_name(mem.base) if mem.base else None,
                             "index": insn.reg_name(mem.index) if mem.index else None,
                             "scale": mem.scale})
        elif op.type == capstone.x86.X86_OP_IMM:
            refs.append({"form": FORM_CODE_TARGET if is_branch else FORM_IMMEDIATE,
                         "value": u32(op.imm), "disp": op.imm,
                         "base": None, "index": None, "scale": 0})
    return refs


def encoding_of(insn) -> str:
    """The x86 encoding class of a direct absolute operand, for spelling correlation."""
    if not insn.bytes:
        return "unknown"
    first = insn.bytes[0]
    if first == 0xA1:
        return ENC_A1_MOFFS
    if first == 0xA3:
        return ENC_A3_MOFFS
    return ENC_MODRM


# ------------------------------------------------------------------------- recursive walk


class Walk:
    """The recursive-descent reachability result.

    `refs` indexes every operand reference of every visited instruction, at ANY width, by
    its normalised `uint32` value.  The raw-byte scan can only see a 4-byte operand, so a
    `disp8` such as `[ebx+0x10]` is invisible to it; the walk sees the instruction stream and
    therefore covers `disp8` and `disp32` alike.  The two are kept as separate evidence and
    never merged into the site count.
    """

    def __init__(self) -> None:
        self.starts: set[int] = set()
        self.refs: dict[int, list[tuple]] = {}
        self.decoded = 0
        self.undecodable = 0
        self.seeds = 0
        self.exhausted = False

    def __contains__(self, va: int) -> bool:
        return u32(va) in self.starts

    def references(self, value: int) -> list[dict]:
        """Every operand reference of a visited instruction whose normalised value matches."""
        out: list[dict] = []
        for va, form, raw_value, base, index, scale, mnemonic, width in \
                self.refs.get(u32(value), ()):
            out.append({
                "instruction_va": hex32(va),
                "form": form,
                "width_bytes": width,
                "base": base,
                "index": index,
                "scale": scale,
                "mnemonic": mnemonic,
            })
        out.sort(key=lambda r: int(r["instruction_va"], 16))
        return out


def _record_refs(walk: Walk, insn, va: int) -> None:
    """Index every operand reference of one decoded instruction, normalised to `uint32`.

    A branch's immediate is recorded as `FORM_CODE_TARGET`, not as an access: `call
    0x0014CF20` references code, not a data location, and conflating the two would report
    every call target as a guest access.
    """
    is_branch = insn.mnemonic in BRANCH_MNEMONICS
    code = bytes(insn.bytes)
    width = displacement_width(code)
    for op in insn.operands:
        if op.type == capstone.x86.X86_OP_MEM:
            mem = op.mem
            if mem.base == 0 and mem.index == 0:
                form = FORM_MEM_ABSOLUTE
            else:
                form = FORM_MEM_DISPLACEMENT
            base = insn.reg_name(mem.base) if mem.base else None
            index = insn.reg_name(mem.index) if mem.index else None
            walk.refs.setdefault(u32(mem.disp), []).append(
                (va, form, mem.disp, base, index, mem.scale, insn.mnemonic, width))
        elif op.type == capstone.x86.X86_OP_IMM:
            form = FORM_CODE_TARGET if is_branch else FORM_IMMEDIATE
            walk.refs.setdefault(u32(op.imm), []).append(
                (va, form, op.imm, None, None, 0, insn.mnemonic, 4))


def walk_program(image: XbeImage, seeds, md: capstone.Cs | None = None,
                 max_starts: int = 4_000_000) -> Walk:
    """Recursive descent from `seeds`, following direct branches and fall-through.

    Successors:
      * `call rel32`  -> target **and** fall-through.  The fall-through matters for an
        INDIRECT call too: the callee returns, so the instruction after `call [mem]` is
        reachable.  Omitting it silently truncates every function that makes a virtual
        call -- measured on this XBE as 44,713 missing instruction starts, including one of
        the three vtable-base references.
      * `jmp rel32`   -> target only.
      * `jcc rel32`   -> target and fall-through.
      * anything else -> fall-through.
      * a terminator  -> nothing.
      * an indirect `jmp` -> nothing: the target is not a literal.

    Anything the walk does not reach is reported as UNREACHED by the caller, never dropped.
    """
    md = md or new_disassembler()
    walk = Walk()
    walk.seeds = len(set(seeds))
    stack = [u32(s) for s in seeds]
    while stack:
        va = stack.pop()
        if va in walk.starts:
            continue
        if image.section_of(va) is None:
            continue
        if len(walk.starts) >= max_starts:
            walk.exhausted = True
            break
        walk.starts.add(va)
        insn = decode_one(image, va, md)
        if insn is None:
            walk.undecodable += 1
            continue
        walk.decoded += 1
        _record_refs(walk, insn, va)
        nxt = va + len(insn.bytes)
        mnemonic = insn.mnemonic
        operands = insn.operands
        target = None
        if len(operands) == 1 and operands[0].type == capstone.x86.X86_OP_IMM:
            target = u32(operands[0].imm)
        if mnemonic in TERMINATORS:
            successors: list[int] = []
        elif mnemonic == "jmp":
            successors = [target] if target is not None else []
        elif mnemonic == "call":
            successors = ([target] if target is not None else []) + [nxt]
        elif mnemonic.startswith("j") and target is not None:
            successors = [target, nxt]
        else:
            successors = [nxt]
        stack.extend(s for s in successors if s is not None)
    return walk


def dispatch_seeds(dispatch_c: Path) -> list[int]:
    """Every guest VA in the generated dispatch table, plus nothing else.

    These are the function entries the translation knows about, so seeding from them is
    what makes the walk cover the whole translated program rather than only the entry's
    call graph.
    """
    if not dispatch_c.is_file():
        raise EnumerateError(f"dispatch table not found: {dispatch_c}")
    try:
        text = dispatch_c.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise EnumerateError(f"cannot read {dispatch_c}: {exc}") from exc
    seeds = {u32(int(m.group(1), 16)) for m in DISPATCH_ENTRY.finditer(text)}
    if not seeds:
        raise EnumerateError(f"no dispatch entries parsed from {dispatch_c}")
    return sorted(seeds)


# --------------------------------------------------------------------------- enumeration


def decode_boundary(image: XbeImage, va: int, md: capstone.Cs):
    """The instruction that starts exactly at `va`, or None if `va` is not a boundary.

    This is the transposition probe, and it is deliberately NOT `locate_instructions`: an
    address can be a real instruction start without any operand dword ending on it (a
    two-byte `mov esi, ecx`, for instance).  Conflating "is an instruction boundary" with
    "is a site operand" makes the boundary probe answer "no" for every instruction that has
    no trailing dword -- which is most of them.
    """
    if image.section_of(va) is None:
        return None
    insn = decode_one(image, va, md)
    if insn is None or insn.address != u32(va):
        return None
    return insn


def containing_instruction(image: XbeImage, va: int, walk: Walk | None,
                           md: capstone.Cs) -> dict | None:
    """The walked instruction whose bytes CONTAIN `va`, if any.

    This is what makes an interior address legible: `0x00193D62` is not "some address that
    decodes to something", it is the second byte of the 6-byte instruction at `0x00193D61`.
    """
    for back in range(1, 16):
        start = va - back
        if image.section_of(start) is None:
            continue
        if walk is not None and start not in walk:
            continue
        insn = decode_boundary(image, start, md)
        if insn is None:
            continue
        if start + len(insn.bytes) > va:
            return {"instruction_va": hex32(start),
                    "text": f"{insn.mnemonic} {insn.op_str}",
                    "bytes": insn.bytes.hex(),
                    "length": len(insn.bytes)}
    return None


def boundary_evidence(image: XbeImage, va: int, walk: Walk | None,
                      md: capstone.Cs) -> dict:
    """Is `va` an instruction boundary, and is it a boundary the walk actually visited?

    A bare decode is not enough to separate a real instruction start from a coincidental
    one: x86 decodes from almost any byte, so "something decodes here" is true nearly
    everywhere and discriminates nothing.  The load-bearing predicate is whether the
    recursive descent -- which only ever steps from one verified boundary to the next --
    visited `va` as an instruction start.
    """
    insn = decode_boundary(image, va, md)
    reached = bool(walk is not None and va in walk)
    return {
        "address": hex32(va),
        "decodes": insn is not None,
        "text": None if insn is None else f"{insn.mnemonic} {insn.op_str}",
        "bytes": None if insn is None else insn.bytes.hex(),
        "reached_as_instruction_start": reached,
        "contained_in": None if reached else containing_instruction(image, va, walk, md),
    }


def locate_instructions(image: XbeImage, operand_va: int, md: capstone.Cs,
                        max_back: int = MAX_BACK) -> list[dict]:
    """Instructions whose trailing 4 bytes are the dword at `operand_va`.

    The start is resolved by DECODING backwards, never by assuming a fixed offset.  An
    earlier session decoded from `find_offset - 2` and capstone silently rendered a
    mid-instruction byte as plausible garbage; a candidate is therefore accepted only when
    the decode begins exactly at the candidate start, the instruction ends exactly where the
    operand ends, and one of its operands really is the value.
    """
    found: list[dict] = []
    for back in range(1, max_back + 1):
        start = operand_va - back
        if start < 0:
            continue
        if image.section_of(start) is None:
            continue
        insn = decode_one(image, start, md)
        if insn is None:
            continue
        if insn.address != start:
            continue
        if start + len(insn.bytes) != operand_va + 4:
            continue
        found.append({
            "start": start,
            "length": len(insn.bytes),
            "bytes": insn.bytes.hex(),
            "raw": bytes(insn.bytes),
            "text": f"{insn.mnemonic} {insn.op_str}",
            "mnemonic": insn.mnemonic,
            "encoding": encoding_of(insn),
            "refs": operand_refs(insn),
        })
    return found


def enumerate_value(image: XbeImage, walk: Walk | None, value: int,
                    md: capstone.Cs | None = None) -> dict:
    """Every site in the original XBE that references `value`, normalised to `uint32`.

    Two independent enumerations are produced and reconciled:

      * the **raw-byte scan** finds every little-endian dword equal to the value, at every
        alignment (existence, alignment-independent);
      * the **walk** indexes every operand reference of every instruction it visited, at
        every width (`disp8` included, which a 4-byte scan cannot see).

    Returns a dict with:
      * `sites`                 -- CONFIRMED.  A site is an instruction that the recursive
                                   descent visited AND that references the value as an
                                   address (a `mem-absolute` or an `immediate` operand).
      * `walk_only_references`  -- references the walk saw that the 4-byte raw scan could
                                   not: a `disp8` offset, or a 4-byte value that the scan
                                   found but whose referring instruction start it could not
                                   resolve.  Reported separately; never merged into `sites`.
      * `unreached`             -- a real instruction referencing the value that the walk did
                                   not reach.  Reported, never counted as confirmed.
      * `displacement_matches`  -- base/index-relative displacements equal to the value.
                                   These are OFFSETS, not addresses; reported, not counted.
      * `code_targets`          -- branch targets equal to the value.  Code, not an access.
      * `unlocated_operands`    -- an operand dword that is DATA: no decodable instruction
                                   ends on it, or the only decode does not reference the
                                   value by operand.
      * `ambiguous_operands`    -- an operand dword ending more than one decodable
                                   instruction.  x86 is not self-synchronising, so these are
                                   different *interpretations* of one dword, not different
                                   sites; reported so the ambiguity is visible.
      * `raw_occurrences`       -- the alignment-independent existence count.

    `sites` is the answer to "how many places touch this address", and it comes from the
    control-flow-following enumeration.  Every other bucket is loss accounting: a raw dword
    is in exactly one of `sites` / `unreached` / `unlocated_operands`, and nothing is
    silently dropped.
    """
    md = md or new_disassembler()
    want = u32(value)
    sites: list[dict] = []
    unreached: list[dict] = []
    displacements: list[dict] = []
    code_targets: list[dict] = []
    unlocated: list[dict] = []
    ambiguous: list[dict] = []

    walk_refs = walk.references(want) if walk is not None else []
    raw_operand_vas = [va for va, _ in image.raw_occurrences(want)]
    resolved_starts: set[int] = set()

    # ---- evidence 1: the raw-byte scan, resolved to instructions by decoding backwards.
    #
    # x86 is not self-synchronising, so ONE operand dword can end several decodable
    # instructions.  Those are different INTERPRETATIONS of one dword, not different sites.
    # The recursive descent arbitrates: it only ever steps between verified boundaries, so
    # the candidate whose start it visited is the real instruction.  Emitting a row per
    # interpretation is what makes a 28-site answer read as 48.
    for operand_va, section in image.raw_occurrences(want):
        candidates = locate_instructions(image, operand_va, md)
        if not candidates:
            unlocated.append({"operand_va": hex32(operand_va), "section": section,
                              "reason": "no-instruction-ends-here"})
            continue
        if walk is None:
            chosen = candidates[0]
            alternates = candidates[1:]
        else:
            walked = [c for c in candidates if c["start"] in walk]
            chosen = walked[0] if walked else candidates[0]
            alternates = [c for c in candidates if c is not chosen]
        if alternates:
            ambiguous.append({
                "operand_va": hex32(operand_va),
                "chosen": hex32(chosen["start"]),
                "alternates": [hex32(c["start"]) for c in alternates],
            })
        resolved_starts.add(chosen["start"])

        ref = next((r for r in chosen["refs"] if r["value"] == want), None)
        if ref is None:
            unlocated.append({
                "operand_va": hex32(operand_va), "section": section,
                "decodes_as": hex32(chosen["start"]),
                "reason": "decode-does-not-reference-the-value",
            })
            continue

        row = {
            "instruction_va": hex32(chosen["start"]),
            "operand_va": hex32(operand_va),
            "section": section,
            "encoding": chosen["encoding"],
            "bytes": chosen["bytes"],
            "text": chosen["text"],
            "form": ref["form"],
            "reached": bool(walk is not None and chosen["start"] in walk),
        }
        if ref["form"] == FORM_MEM_DISPLACEMENT:
            displacements.append(dict(row, base=ref["base"], index=ref["index"],
                                      scale=ref["scale"],
                                      width_bytes=displacement_width(chosen["raw"])))
        elif ref["form"] == FORM_CODE_TARGET:
            code_targets.append(row)
        elif row["reached"]:
            sites.append(row)
        else:
            unreached.append(row)

    # ---- evidence 2: the walk's own operand index.  Independent of the raw scan, so it
    #      sees a `disp8` (one byte, invisible to a 4-byte needle) and any reference whose
    #      instruction start the scan could not resolve.
    walk_only: list[dict] = []
    for ref in walk_refs:
        start = int(ref["instruction_va"], 16)
        if start in resolved_starts:
            continue
        row = dict(ref, reached=True, walk_only=True)
        if ref["form"] == FORM_MEM_DISPLACEMENT:
            displacements.append(row)
        elif ref["form"] == FORM_CODE_TARGET:
            code_targets.append(row)
        else:
            walk_only.append(row)

    sites.sort(key=lambda r: int(r["instruction_va"], 16))
    unreached.sort(key=lambda r: int(r["instruction_va"], 16))
    displacements.sort(key=lambda r: int(r["instruction_va"], 16))
    code_targets.sort(key=lambda r: int(r["instruction_va"], 16))
    unlocated.sort(key=lambda r: int(r["operand_va"], 16))
    ambiguous.sort(key=lambda r: int(r["operand_va"], 16))
    walk_only.sort(key=lambda r: int(r["instruction_va"], 16))
    by_encoding: dict[str, int] = {}
    for r in sites:
        by_encoding[r["encoding"]] = by_encoding.get(r["encoding"], 0) + 1
    return {
        "value": hex32(want),
        "raw_occurrences": len(raw_operand_vas),
        "sites": sites,
        "site_count": len(sites),
        "unreached": unreached,
        "unreached_count": len(unreached),
        "walk_only_references": walk_only,
        "walk_only_count": len(walk_only),
        "displacement_matches": displacements,
        "displacement_count": len(displacements),
        "code_targets": code_targets,
        "code_target_count": len(code_targets),
        "unlocated_operands": unlocated,
        "unlocated_count": len(unlocated),
        "ambiguous_operands": ambiguous,
        "ambiguous_count": len(ambiguous),
        "sites_by_encoding": dict(sorted(by_encoding.items())),
    }


# -------------------------------------------------------------------- generated reconcile


def scan_generated(gen_dir: Path, value: int) -> dict:
    """Generated-C occurrences of `value`, split by the spelling the lifter emitted.

    Every `MEM8/16/32(...)` literal is normalised to `uint32` before comparison, so the hex
    and signed-decimal spellings of one address are both found **by value**.  A scan that
    matched a spelling would report 10 where the answer is 28.
    """
    if not gen_dir.is_dir():
        raise EnumerateError(f"generated-source directory not found: {gen_dir}")
    want = u32(value)
    by_spelling: dict[str, int] = {}
    occurrences: list[dict] = []
    files = sorted(gen_dir.rglob("*.c"))
    if not files:
        raise EnumerateError(f"no .c files under {gen_dir}")
    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise EnumerateError(f"cannot read {path}: {exc}") from exc
        for m in GEN_LITERAL.finditer(text):
            token = m.group(1)
            literal = int(token, 16) if token.lower().startswith("0x") else int(token)
            if u32(literal) != want:
                continue
            spelling = "hex" if token.lower().startswith("0x") else "decimal"
            by_spelling[spelling] = by_spelling.get(spelling, 0) + 1
            occurrences.append({
                "file": str(path.relative_to(gen_dir.parent.parent)),
                "line": text.count("\n", 0, m.start()) + 1,
                "literal": token,
                "spelling": spelling,
            })
    occurrences.sort(key=lambda r: (r["file"], r["line"], r["literal"]))
    return {
        "value": hex32(want),
        "by_spelling": dict(sorted(by_spelling.items())),
        "total": len(occurrences),
        "occurrences": occurrences,
    }


# ------------------------------------------------------------------------------- controls


class Control:
    def __init__(self, name: str, passed: bool, detail: str):
        self.name = name
        self.passed = passed
        self.detail = detail

    def line(self) -> str:
        return f"  [{'PASS' if self.passed else 'FAIL'}] {self.name}: {self.detail}"


def run_controls(pio: dict, vtable: dict, negative: dict, gen: dict,
                 boundaries: dict) -> list[Control]:
    """The known-answer controls, evaluated from measured values only.

    Every expected value here is compared against a number this tool just computed from the
    original XBE or the generated tree.  Nothing is transcribed from a record.
    """
    controls: list[Control] = []

    controls.append(Control(
        "PIO_FREE site count == 28",
        pio["site_count"] == EXPECT_PIO_SITES,
        f"measured {pio['site_count']}, expected {EXPECT_PIO_SITES} "
        f"(raw operand dwords {pio['raw_occurrences']}, "
        f"unreached {pio['unreached_count']}, unlocated {pio['unlocated_count']})"))

    hex_spelled = gen["by_spelling"].get("hex", 0)
    dec_spelled = gen["by_spelling"].get("decimal", 0)
    controls.append(Control(
        "PIO_FREE generated spellings == 10 hex + 18 decimal",
        (hex_spelled, dec_spelled) == (EXPECT_PIO_HEX, EXPECT_PIO_DECIMAL)
        and gen["total"] == EXPECT_PIO_SITES,
        f"measured {gen['total']} = {hex_spelled} hex + {dec_spelled} decimal, "
        f"expected {EXPECT_PIO_SITES} = {EXPECT_PIO_HEX} hex + {EXPECT_PIO_DECIMAL} decimal"))

    # The mechanism behind the split: the lifter emits an A1 moffs operand as hex and a
    # ModRM disp32 operand as signed decimal, so the XBE's own encoding histogram must equal
    # the generated spelling histogram.  This is what makes the split a finding, not a
    # coincidence.
    a1 = pio["sites_by_encoding"].get(ENC_A1_MOFFS, 0)
    modrm = pio["sites_by_encoding"].get(ENC_MODRM, 0)
    controls.append(Control(
        "PIO_FREE A1 moffs count == generated hex spelling count",
        a1 == hex_spelled,
        f"measured A1-encoded XBE sites {a1} vs generated hex spellings {hex_spelled}"))
    controls.append(Control(
        "PIO_FREE ModRM disp32 count == generated decimal spelling count",
        modrm == dec_spelled,
        f"measured ModRM-encoded XBE sites {modrm} vs generated decimal spellings "
        f"{dec_spelled}"))

    controls.append(Control(
        "vtable base references == 3",
        vtable["site_count"] == EXPECT_VTABLE_REFS
        and vtable["unreached_count"] == 0,
        f"measured {vtable['site_count']} reached "
        f"(raw operand dwords {vtable['raw_occurrences']}, "
        f"unreached {vtable['unreached_count']})"))

    controls.append(Control(
        "negative control finds nothing",
        negative["site_count"] == 0 and negative["raw_occurrences"] == 0
        and negative["unreached_count"] == 0,
        f"{negative['value']} -> {negative['site_count']} sites, "
        f"{negative['raw_occurrences']} raw dword occurrences"))

    # Transposition control.  A bare decode discriminates nothing (x86 decodes from almost
    # any byte), so the load-bearing predicate is whether the recursive descent -- which only
    # steps between verified boundaries -- visited the address as an instruction start.
    left_reached = boundaries["left"]["reached_as_instruction_start"]
    right_reached = boundaries["right"]["reached_as_instruction_start"]
    left_in = boundaries["left"]["contained_in"]
    left_desc = (f"interior to {left_in['text']!r} at {left_in['instruction_va']}"
                 if left_in else "not an instruction start")
    controls.append(Control(
        "transposition control distinguishes the two addresses",
        left_reached != right_reached,
        f"{boundaries['left']['address']} walked instruction start: {left_reached} "
        f"({left_desc}); {boundaries['right']['address']} walked instruction start: "
        f"{right_reached} ({boundaries['right']['text']!r})"))

    return controls


# --------------------------------------------------------------------------------- report


CANNOT_SEE = (
    "WHAT THIS METHOD CANNOT SEE (register-indirect, computed and table-driven accesses):",
    "  * register-indirect accesses (`mov eax,[ebx]`) -- no literal operand exists to find;",
    "  * computed addresses (`lea eax,[ebx+ecx*4]`, then a load through `eax`);",
    "  * table-driven accesses -- a pointer read from a table and then dereferenced, which",
    "    includes EVERY vtable dispatch: only the table's own base literal is visible;",
    "  * a literal materialised by arithmetic rather than by a single operand;",
    "  * code reached only through an indirect branch, in a section the walk did not enter;",
    "  * an operand the walk did not reach is reported as UNREACHED, not resolved -- a",
    "    coincidental dword match and a real site the walk missed are indistinguishable here.",
)


def format_report(label: str, rep: dict) -> list[str]:
    lines = [f"  {label} {rep['value']}",
             f"    confirmed sites (recursive-descent reached): {rep['site_count']}"]
    if rep["sites_by_encoding"]:
        lines.append("    by encoding: " + ", ".join(
            f"{k}={v}" for k, v in rep["sites_by_encoding"].items()))
    lines.append(f"    raw operand dwords (alignment-independent):  {rep['raw_occurrences']}")
    lines.append(f"    UNREACHED sites:                             {rep['unreached_count']}")
    lines.append(f"    walk-only references (disp8 / unresolved):   {rep['walk_only_count']}")
    lines.append(f"    base/index displacement matches (not sites): {rep['displacement_count']}")
    lines.append(f"    branch targets equal to the value (code):    {rep['code_target_count']}")
    lines.append(f"    operand dwords that are data:                {rep['unlocated_count']}")
    lines.append(f"    operand dwords with several decodes:         {rep['ambiguous_count']}")
    for r in rep["sites"]:
        lines.append(f"      {r['instruction_va']}  {r['encoding']:<12} "
                     f"{r['bytes']:<16} {r['text']}  [{r['section']}]")
    for r in rep["unreached"]:
        lines.append(f"      {r['instruction_va']}  UNREACHED  {r['bytes']:<16} "
                     f"{r['text']}  [{r['section']}]")
    return lines


def analyse(xbe: Path, analysis: Path, gen_dir: Path, dispatch_c: Path,
            values: list[int], quiet: bool = False) -> tuple[dict, list[Control]]:
    """Run the whole enumeration.  Returns (report, controls)."""
    image = XbeImage.load(xbe, analysis)
    md = new_disassembler()

    seeds = set(dispatch_seeds(dispatch_c))
    seeds.add(image.entry_point)
    walk = walk_program(image, seeds, md)

    reports = {u32(v): enumerate_value(image, walk, v, md) for v in values}
    pio = reports[u32(PIO_FREE_VA)]
    vtable = reports[u32(VTABLE_BASE_VA)]
    negative = reports[u32(NEGATIVE_CONTROL_VA)]
    left = reports[u32(TRANSPOSITION_LEFT)]
    right = reports[u32(TRANSPOSITION_RIGHT)]

    gen = scan_generated(gen_dir, PIO_FREE_VA)

    boundaries = {
        "left": boundary_evidence(image, TRANSPOSITION_LEFT, walk, md),
        "right": boundary_evidence(image, TRANSPOSITION_RIGHT, walk, md),
    }

    controls = run_controls(pio, vtable, negative, gen, boundaries)

    report = {
        "tool": "enumerate-accesses",
        "schema_version": 1,
        "method": {
            "enumeration": "recursive-descent from the XBE entry point plus every "
                           "generated dispatch seed, following direct call/jmp/jcc targets "
                           "and fall-through (indirect call keeps its fall-through)",
            "existence_check": "alignment-independent raw-byte scan for the little-endian "
                               "dword, unfiltered by reachability",
            "normalisation": "every operand masked to uint32 and printed as 0x%08X",
            "operand_forms": [FORM_MEM_ABSOLUTE, FORM_MEM_DISPLACEMENT, FORM_IMMEDIATE],
            "cannot_see": [l.strip() for l in CANNOT_SEE if l.strip().startswith("*")],
        },
        "walk": {
            "seeds": walk.seeds,
            "entry_point": hex32(image.entry_point),
            "instruction_starts_reached": len(walk.starts),
            "instructions_decoded": walk.decoded,
            "undecodable_starts": walk.undecodable,
            "exhausted": walk.exhausted,
        },
        "generated_pio_free": gen,
        "queries": {hex32(v): reports[u32(v)] for v in values},
        "controls": [{"name": c.name, "passed": c.passed, "detail": c.detail}
                     for c in controls],
        "controls_passed": sum(1 for c in controls if c.passed),
        "controls_failed": sum(1 for c in controls if not c.passed),
        "negative_control_value": hex32(NEGATIVE_CONTROL_VA),
        "transposition": {
            "left": hex32(TRANSPOSITION_LEFT),
            "right": hex32(TRANSPOSITION_RIGHT),
            "left_evidence": boundaries["left"],
            "right_evidence": boundaries["right"],
            "left_sites": left["site_count"],
            "right_sites": right["site_count"],
            "distinguishable": (boundaries["left"]["reached_as_instruction_start"]
                                != boundaries["right"]["reached_as_instruction_start"]),
        },
    }
    return report, controls


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Enumerate guest-VA accesses in the original XBE, normalised to uint32.")
    ap.add_argument("--xbe", type=Path, default=ROOT / "game" / "default.xbe")
    ap.add_argument("--analysis", type=Path,
                    default=ROOT / "game" / "mygame_analysis.json")
    ap.add_argument("--gen-dir", type=Path, default=ROOT / "src" / "recomp" / "gen")
    ap.add_argument("--dispatch", type=Path,
                    default=ROOT / "src" / "recomp" / "gen" / "recomp_dispatch.c")
    ap.add_argument("--value", action="append", default=None,
                    help="extra guest VA to enumerate (repeatable)")
    ap.add_argument("--json", type=Path, default=None, help="write the full report here")
    args = ap.parse_args(argv)

    values = [PIO_FREE_VA, VTABLE_BASE_VA, NEGATIVE_CONTROL_VA,
              TRANSPOSITION_LEFT, TRANSPOSITION_RIGHT]
    if args.value:
        values.extend(int(v, 0) for v in args.value)

    try:
        report, controls = analyse(args.xbe, args.analysis, args.gen_dir, args.dispatch,
                                   values)
    except EnumerateError as exc:
        print(f"enumeration failed: {exc}")
        return 2

    print("=" * 78)
    print("enumerate-accesses -- guest accesses by VALUE, never by spelling")
    print("=" * 78)
    print(f"  XBE          {args.xbe}")
    print(f"  dispatch     {args.dispatch}")
    print(f"  generated    {args.gen_dir}")
    print()
    print("METHOD")
    print(f"  {report['method']['enumeration']}")
    print(f"  {report['method']['existence_check']}")
    print(f"  {report['method']['normalisation']}")
    print()
    print("WALK")
    for key, val in report["walk"].items():
        print(f"  {key}: {val}")
    print()
    print("ENUMERATION")
    for value in values:
        print("\n".join(format_report("", report["queries"][hex32(value)])))
    print()
    print("RECONCILIATION BY NORMALISED VALUE (generated C)")
    gen = report["generated_pio_free"]
    print(f"  {gen['value']}: total {gen['total']}, spellings {gen['by_spelling']}")
    print("  (a spelling-only grep of the hex form would report "
          f"{gen['by_spelling'].get('hex', 0)} of {gen['total']})")
    print()
    print("CONTROLS (each run prints its own known-answer controls)")
    for c in controls:
        print(c.line())
    print(f"  {report['controls_passed']} passed, {report['controls_failed']} failed")
    print()
    print("TRANSPOSITION CONTROL")
    t = report["transposition"]
    for side in ("left", "right"):
        ev = t[f"{side}_evidence"]
        where = (f"interior to {ev['contained_in']['text']!r} at "
                 f"{ev['contained_in']['instruction_va']}"
                 if ev["contained_in"] else f"decodes as {ev['text']!r}")
        print(f"  {t[side]}: walked instruction start = "
              f"{ev['reached_as_instruction_start']}, {where}, "
              f"sites = {t[f'{side}_sites']}")
    print(f"  distinguishable: {t['distinguishable']}")
    print()
    print("\n".join(CANNOT_SEE))
    print()

    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"wrote {args.json}")

    return 1 if report["controls_failed"] else 0


if __name__ == "__main__":
    sys.exit(main())
