"""Classify the title's direct PIO_FREE (0xFE820010) gates from original-XBE bytes.

Diagnostic tool for the `PIO_FREE-title-demand-bound-r1` discovery.  It reads the
ORIGINAL XBE, finds every direct read of 0xFE820010, decodes forward from a VERIFIED
instruction boundary, and reports each gate's form deterministically.

Why this exists
---------------
Earlier sessions classified these sites with ad-hoc window heuristics and produced four
distinct failures.  The worst was a disassembly started one byte before a genuine
instruction boundary, which capstone silently rendered as plausible `.byte`/garbage
instead of erroring.  This tool therefore:

  * locates each read by DECODING, never by assuming `find_offset - 1` or `- 2`;
  * verifies the encoding byte, the instruction length, AND that the decoded operand
    really is the dword `find` matched -- so a misaligned start is REJECTED, not decoded;
  * uses capstone's structured operands throughout, never string matching;
  * fails loudly on anything it cannot classify and never silently skips a site.

Address handling
----------------
`inspect-jsrf.py find` reports the VA of the 4-byte little-endian OPERAND.  For a direct
absolute 32-bit load that operand is the last 4 bytes of the instruction, so it sits at
`start + 1` (`A1 moffs32`, 5 bytes) or `start + 2` (`8B /r` with `mod=00, r/m=101`,
6 bytes).  Those are the only two encodings of a direct absolute 32-bit load, and this
tool recognises exactly those two.

Determinism
-----------
Sites are emitted sorted by instruction VA and every value is a plain integer or a
fixed-format hex string, so repeated runs on identical inputs are byte-identical.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

PIO_FREE_VA = 0xFE820010

# The only two encodings of a direct absolute 32-bit load.
ENC_A1_MOFFS = "A1_moffs32"           # mov eax, moffs32  -> 5 bytes, operand at start+1
ENC_MODRM_DISP32 = "8B_modrm_disp32"  # mov r32, moffs32  -> 6 bytes, operand at start+2
ENCODINGS = ((ENC_A1_MOFFS, 1, 0xA1), (ENC_MODRM_DISP32, 2, 0x8B))

# Bytes decoded forward from each instruction start.  Ample to reach the compare and the
# conditional branch for every observed form without running away.
DECODE_WINDOW = 64

# The stub under test returns this constant; the packet fixes these derived values.
STUB_VALUE = 0x80
VARIABLE_AVAILABLE = STUB_VALUE >> 2            # 32
CONSTANT_AVAILABLE = STUB_VALUE & 0xFFFFFFFC    # 128

# Mask used to normalise a signed decimal literal in generated source to its uint32 value.
UINT32 = 0xFFFFFFFF


class ClassifyError(Exception):
    """A site or input that cannot be classified.  Always fatal, never skipped."""


def _u32(value: int) -> int:
    return value & UINT32


# --------------------------------------------------------------------------- input


def load_sections(analysis: Path) -> list[dict]:
    try:
        data = json.loads(analysis.read_text(encoding="utf-8"))
        sections = data["sections"]
    except (OSError, ValueError, KeyError) as exc:
        raise ClassifyError(f"cannot read analysis {analysis}: {exc}") from exc
    if not isinstance(sections, list) or not sections:
        raise ClassifyError(f"analysis {analysis} has no sections")
    for s in sections:
        for key in ("name", "virtual_addr", "raw_addr", "raw_size"):
            if key not in s:
                raise ClassifyError(f"analysis section missing {key!r}: {s!r}")
    return sections


def section_containing(sections: list[dict], start: int) -> dict | None:
    """The file-backed section containing `start`, or None."""
    for s in sections:
        va = int(s["virtual_addr"], 16)
        size = int(s["raw_size"])
        if va <= start < va + size:
            return s
    return None


def xbe_slice(xbe: Path, sections: list[dict], start: int, length: int) -> bytes:
    """Original bytes of the retail XBE for a virtual address range.

    The range must lie inside ONE file-backed section.  Callers that want "as much as is
    available" should use `xbe_window`, which clamps to the section end instead of
    failing, so a short section does not masquerade as a classification failure.
    """
    if length <= 0:
        raise ClassifyError("length must be positive")
    end = start + length
    if not 0 <= start < end <= 0x100000000:
        raise ClassifyError(f"range 0x{start:08X}..0x{end:08X} is outside the 32-bit space")
    for s in sections:
        va = int(s["virtual_addr"], 16)
        size = int(s["raw_size"])
        if va <= start and end <= va + size:
            off = int(s["raw_addr"], 16) + start - va
            try:
                with xbe.open("rb") as fh:
                    fh.seek(off)
                    raw = fh.read(length)
            except OSError as exc:
                raise ClassifyError(f"cannot read {xbe}: {exc}") from exc
            if len(raw) != length:
                raise ClassifyError(
                    f"short read at 0x{start:08X}: wanted {length}, got {len(raw)}")
            return raw
    raise ClassifyError(
        f"range 0x{start:08X}..0x{end:08X} is not contained in one file-backed section")


def xbe_window(xbe: Path, sections: list[dict], start: int, want: int) -> bytes:
    """Up to `want` bytes from `start`, clamped to the containing section's end.

    Raises if `start` is not inside a file-backed section at all.  Clamping is what lets
    a site near the end of a section decode the bytes that actually exist, rather than
    failing the whole classification because a fixed window overran the section.
    """
    s = section_containing(sections, start)
    if s is None:
        raise ClassifyError(f"0x{start:08X} is not inside a file-backed section")
    va = int(s["virtual_addr"], 16)
    available = va + int(s["raw_size"]) - start
    return xbe_slice(xbe, sections, start, min(want, available))


def find_operand_offsets(xbe: Path, sections: list[dict], value: int) -> list[tuple[int, str]]:
    """Every VA of a little-endian dword equal to `value`, with its section name.

    Mirrors `inspect-jsrf.py find` exactly (unaligned, every section), so the population
    reconciles one-to-one with the accepted `A4p` set.
    """
    needle = _u32(value).to_bytes(4, "little")
    hits: list[tuple[int, str]] = []
    try:
        with xbe.open("rb") as fh:
            for s in sections:
                fh.seek(int(s["raw_addr"], 16))
                raw = fh.read(int(s["raw_size"]))
                base = int(s["virtual_addr"], 16)
                at = raw.find(needle)
                while at != -1:
                    hits.append((base + at, s["name"]))
                    at = raw.find(needle, at + 1)
    except OSError as exc:
        raise ClassifyError(f"cannot read {xbe}: {exc}") from exc
    return hits


# ------------------------------------------------------------------------ decoding


def decode_from(raw: bytes, va: int, md: capstone.Cs) -> list:
    return list(md.disasm(raw, va))


def absolute_memory_va(insn) -> int | None:
    """The absolute 32-bit address of the instruction's memory operand, or None.

    Scans EVERY operand, because for a load the memory reference is the SOURCE
    (`mov edx, [addr]` has the register in operand 0 and the memory in operand 1).  An
    earlier version only inspected operand 0 and therefore failed to recognise every
    ModRM load -- the tests caught it.
    """
    for op in insn.operands:
        if op.type != capstone.x86.X86_OP_MEM:
            continue
        mem = op.mem
        if mem.base != 0 or mem.index != 0:
            continue
        return _u32(mem.disp)
    return None


def locate_read(xbe: Path, sections: list[dict], operand_va: int, md: capstone.Cs) -> dict:
    """Find the instruction reading PIO_FREE whose operand sits at `operand_va`.

    For each legal encoding this checks, in order:
      1. the instruction's first byte is the expected opcode;
      2. decoding from that start reproduces an instruction beginning exactly there;
      3. the instruction's length is `operand_offset + 4`, so the operand really is the
         trailing dword and therefore starts at `start + operand_offset`;
      4. that operand resolves to the absolute address PIO_FREE_VA.

    A `start - 2` misalignment fails (1) or (2) and is REJECTED rather than decoded.
    """
    for enc, back, opcode in ENCODINGS:
        start = operand_va - back
        if start < 0:
            continue
        try:
            raw = xbe_window(xbe, sections, start, DECODE_WINDOW)
        except ClassifyError:
            continue
        if not raw or raw[0] != opcode:
            continue
        insns = decode_from(raw, start, md)
        if not insns:
            continue
        first = insns[0]
        if first.address != start:
            continue
        if len(first.bytes) != back + 4:
            continue
        if absolute_memory_va(first) != PIO_FREE_VA:
            continue
        if first.mnemonic != "mov":
            continue
        if not first.operands or first.operands[0].type != capstone.x86.X86_OP_REG:
            continue
        if enc == ENC_A1_MOFFS and first.reg_name(first.operands[0].reg) != "eax":
            continue
        return {
            "encoding": enc,
            "instruction_va": start,
            "operand_va": operand_va,
            "instruction_bytes": first.bytes.hex(),
            "instruction_text": f"{first.mnemonic} {first.op_str}",
            "loaded_register": first.reg_name(first.operands[0].reg),
            "insns": insns,
        }
    raise ClassifyError(
        f"no valid direct-read encoding with operand at 0x{operand_va:08X} "
        f"(tried start-1 A1 and start-2 8B)")


# ---------------------------------------------------------------------- classifying


def _apply_transform(reg: str, insn, transforms: list[dict]) -> str:
    """Record a transformation of `reg` and return the register now holding the value.

    Structured operands only: nothing here inspects `op_str`.
    """
    dst = insn.operands[0]
    if dst.type != capstone.x86.X86_OP_REG:
        return reg
    dst_name = insn.reg_name(dst.reg)
    if dst_name != reg:
        return reg

    if insn.mnemonic == "shr" and len(insn.operands) == 2 and \
            insn.operands[1].type == capstone.x86.X86_OP_IMM:
        transforms.append({"op": "shr", "va": f"0x{insn.address:08X}",
                           "bytes": insn.bytes.hex(), "amount": insn.operands[1].imm})
        return dst_name
    if insn.mnemonic == "and" and len(insn.operands) == 2 and \
            insn.operands[1].type == capstone.x86.X86_OP_IMM:
        mask = _u32(insn.operands[1].imm)
        transforms.append({"op": "and", "va": f"0x{insn.address:08X}",
                           "bytes": insn.bytes.hex(), "mask": f"0x{mask:08X}"})
        return dst_name
    if insn.mnemonic == "mov" and len(insn.operands) == 2 and \
            insn.operands[1].type == capstone.x86.X86_OP_REG:
        transforms.append({"op": "mov", "va": f"0x{insn.address:08X}",
                           "bytes": insn.bytes.hex(),
                           "from": insn.reg_name(insn.operands[1].reg)})
        return dst_name
    return reg


def stub_value_through(transforms: list[dict]) -> int | None:
    """The stub's value after the recorded transformations, or None if unknown.

    A `mov` from another register breaks the chain: the value is then not a function of
    the stub alone, so sufficiency cannot be decided from the stub's constant.
    """
    value = STUB_VALUE
    for t in transforms:
        if t["op"] == "shr":
            value >>= t["amount"]
        elif t["op"] == "and":
            value &= int(t["mask"], 16)
        else:
            return None
    return _u32(value)


def classify_site(site: dict) -> dict:
    """Classify one site from its decoded instruction stream.

    Raises ClassifyError for anything unrecognised -- never returns a partial row.
    """
    insns = site["insns"]
    out = {
        "instruction_va": f"0x{site['instruction_va']:08X}",
        "operand_va": f"0x{site['operand_va']:08X}",
        "encoding": site["encoding"],
        "instruction_bytes": site["instruction_bytes"],
        "instruction_text": site["instruction_text"],
        "loaded_register": site["loaded_register"],
    }

    cmp_i = None
    for i, insn in enumerate(insns[1:], start=1):
        if insn.mnemonic == "cmp":
            cmp_i = i
            break
        if insn.mnemonic in ("test", "or", "xor"):
            raise ClassifyError(
                f"{out['instruction_va']}: unsupported gate predicate "
                f"{insn.mnemonic!r} at 0x{insn.address:08X}")
    if cmp_i is None:
        raise ClassifyError(f"{out['instruction_va']}: no cmp within {DECODE_WINDOW} bytes")

    branch_i = None
    for j in range(cmp_i + 1, min(cmp_i + 4, len(insns))):
        if insns[j].mnemonic.startswith("j"):
            branch_i = j
            break
    if branch_i is None:
        raise ClassifyError(f"{out['instruction_va']}: no conditional branch after cmp")

    cmp_insn, br_insn = insns[cmp_i], insns[branch_i]
    if len(br_insn.operands) != 1 or br_insn.operands[0].type != capstone.x86.X86_OP_IMM:
        raise ClassifyError(f"{out['instruction_va']}: branch has no immediate target")

    out["cmp_va"] = f"0x{cmp_insn.address:08X}"
    out["cmp_bytes"] = cmp_insn.bytes.hex()
    out["cmp_text"] = f"{cmp_insn.mnemonic} {cmp_insn.op_str}"
    out["branch_va"] = f"0x{br_insn.address:08X}"
    out["branch_bytes"] = br_insn.bytes.hex()
    out["branch_text"] = f"{br_insn.mnemonic} {br_insn.op_str}"
    out["branch_predicate"] = br_insn.mnemonic
    target = _u32(br_insn.operands[0].imm)
    out["branch_target"] = f"0x{target:08X}"
    out["branch_is_backward"] = target <= br_insn.address
    # `jb`/`jbe` are the unsigned-below forms; the demand comparisons are unsigned.
    out["unsigned_below"] = br_insn.mnemonic in ("jb", "jbe", "jnae")

    # Transformations applied to the loaded register on the way to the compare.
    reg = site["loaded_register"]
    transforms: list[dict] = []
    for insn in insns[1:cmp_i]:
        reg = _apply_transform(reg, insn, transforms)
    out["transforms"] = transforms
    out["transformed_register"] = reg

    if len(cmp_insn.operands) != 2:
        raise ClassifyError(f"{out['instruction_va']}: cmp does not have two operands")
    left, right = cmp_insn.operands

    def is_polled(op) -> bool:
        return op.type == capstone.x86.X86_OP_REG and cmp_insn.reg_name(op.reg) == reg

    if is_polled(left):
        demand, out["compare_order"] = right, "poll_first"
    elif is_polled(right):
        demand, out["compare_order"] = left, "poll_second"
    else:
        raise ClassifyError(
            f"{out['instruction_va']}: neither cmp operand is the transformed "
            f"register {reg!r} (cmp {cmp_insn.op_str})")

    if demand.type == capstone.x86.X86_OP_IMM:
        literal = _u32(demand.imm)
        out["gate_form"] = "CONSTANT"
        out["demand_literal"] = f"0x{literal:08X}"
        out["demand_register"] = None
        available = stub_value_through(transforms)
        if available is None:
            raise ClassifyError(
                f"{out['instruction_va']}: constant gate but the stub's value is not "
                f"determined by the recorded transforms")
        out["stub_available"] = f"0x{available:08X}"
        out["stub_passes"] = available >= literal
        out["stub_margin"] = available - literal
    elif demand.type == capstone.x86.X86_OP_REG:
        out["gate_form"] = "VARIABLE"
        out["demand_literal"] = None
        out["demand_register"] = cmp_insn.reg_name(demand.reg)
        available = stub_value_through(transforms)
        out["stub_available"] = None if available is None else available
        # The demand is a register: sufficiency needs that register's upper bound, which
        # is the backward slice's job (experiment 3), not this tool's.
        out["stub_passes"] = None
        out["stub_margin"] = None
    else:
        raise ClassifyError(
            f"{out['instruction_va']}: cmp demand operand is neither immediate nor "
            f"register (cmp {cmp_insn.op_str})")
    return out


# ------------------------------------------------------------- generated-spelling scan

GEN_LITERAL = re.compile(r"MEM(?:8|16|32)\(\s*(-?\d+|0[xX][0-9a-fA-F]+)\s*u?\s*\)")


def scan_generated(gen_dir: Path, value: int) -> dict:
    """Count generated-source occurrences of a normalized 32-bit literal value.

    `AGENTS.md` warns that one guest address can be spelled two ways -- hex for an `A1`
    moffs load and signed decimal for a ModRM `disp32` operand -- so a scan that greps
    only one spelling undercounts.  This normalises EVERY MEM8/16/32 literal to uint32
    and compares, so both spellings are found by construction.
    """
    if not gen_dir.is_dir():
        raise ClassifyError(f"generated-source directory not found: {gen_dir}")
    want = _u32(value)
    by_spelling: dict[str, int] = {}
    matching: list[dict] = []
    files = sorted(p for p in gen_dir.rglob("*.c"))
    if not files:
        raise ClassifyError(f"no .c files under {gen_dir}")
    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise ClassifyError(f"cannot read {path}: {exc}") from exc
        for m in GEN_LITERAL.finditer(text):
            token = m.group(1)
            literal = int(token, 16) if token.lower().startswith("0x") else int(token)
            if _u32(literal) != want:
                continue
            spelling = "hex" if token.lower().startswith("0x") else "decimal"
            by_spelling[spelling] = by_spelling.get(spelling, 0) + 1
            line_no = text.count("\n", 0, m.start()) + 1
            matching.append({
                "file": str(path.relative_to(gen_dir.parent.parent)),
                "line": line_no,
                "literal": token,
                "spelling": spelling,
            })
    matching.sort(key=lambda r: (r["file"], r["line"], r["literal"]))
    return {
        "value": f"0x{want:08X}",
        "by_spelling": dict(sorted(by_spelling.items())),
        "total": len(matching),
        "occurrences": matching,
    }


# ---------------------------------------------------------------------------- driver


def classify_all(xbe: Path, analysis: Path, gen_dir: Path | None = None) -> dict:
    sections = load_sections(analysis)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True

    hits = find_operand_offsets(xbe, sections, PIO_FREE_VA)
    sites: list[dict] = []
    rejects: list[dict] = []
    for operand_va, section in hits:
        try:
            located = locate_read(xbe, sections, operand_va, md)
            row = classify_site(located)
            row["section"] = section
            sites.append(row)
        except ClassifyError as exc:
            rejects.append({"operand_va": f"0x{operand_va:08X}",
                            "section": section, "reason": str(exc)})

    sites.sort(key=lambda s: int(s["instruction_va"], 16))
    rejects.sort(key=lambda r: int(r["operand_va"], 16))

    constant = [s for s in sites if s["gate_form"] == "CONSTANT"]
    variable = [s for s in sites if s["gate_form"] == "VARIABLE"]
    literals: dict[str, int] = {}
    for s in constant:
        literals[s["demand_literal"]] = literals.get(s["demand_literal"], 0) + 1
    registers: dict[str, int] = {}
    for s in variable:
        registers[s["demand_register"]] = registers.get(s["demand_register"], 0) + 1

    result = {
        "tool": "pio-free-demand",
        "schema_version": 1,
        "pio_free_va": f"0x{PIO_FREE_VA:08X}",
        "stub_value": f"0x{STUB_VALUE:08X}",
        "operand_offsets_found": len(hits),
        "sites_classified": len(sites),
        "sites_rejected": len(rejects),
        "counts": {"constant": len(constant), "variable": len(variable)},
        "constant_literals": dict(sorted(literals.items())),
        "variable_demand_registers": dict(sorted(registers.items())),
        "constant_sites_failing_stub": sorted(
            s["instruction_va"] for s in constant if not s["stub_passes"]),
        "constant_sites_zero_margin": sorted(
            s["instruction_va"] for s in constant if s["stub_margin"] == 0),
        "sites": sites,
        "rejects": rejects,
    }
    if gen_dir is not None:
        result["generated_spellings"] = scan_generated(gen_dir, PIO_FREE_VA)
    return result


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Classify the title's direct PIO_FREE gates from original-XBE bytes.")
    ap.add_argument("--xbe", type=Path, default=ROOT / "game" / "default.xbe")
    ap.add_argument("--analysis", type=Path,
                    default=ROOT / "game" / "mygame_analysis.json")
    ap.add_argument("--gen-dir", type=Path, default=None,
                    help="also scan this generated-source tree for both literal spellings")
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args(argv)

    try:
        result = classify_all(args.xbe, args.analysis, args.gen_dir)
    except ClassifyError as exc:
        print(f"classification failed: {exc}")
        return 2

    text = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"wrote {args.out}: {result['sites_classified']} classified, "
              f"{result['sites_rejected']} rejected, "
              f"constant={result['counts']['constant']} "
              f"variable={result['counts']['variable']}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
