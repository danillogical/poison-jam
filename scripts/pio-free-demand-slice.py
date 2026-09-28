"""Backward demand slice for the 15 VARIABLE PIO_FREE gates.

Experiment 3 of `PIO_FREE-title-demand-bound-r1`: for each variable gate, slice backward
from the demand operand *as it stands at the `cmp`*, through reaching definitions, to
prove an unsigned 32-bit upper bound <= 32 -- or exhibit a concrete feasible demand > 32 --
or record exactly which edge/input is missing (`OPEN`).

It is deliberately NOT a forward CFG enumeration.  It walks backward from the compare
along straight-line predecessors within the enclosing basic block, following the demand
register through the instructions that define it, and stops at a control-flow join, a
call, or an unrecognised definition.  Whatever it cannot prove is `OPEN`, never `BOUND`.

Demand forms it can currently prove
-----------------------------------
  * `movzx r32, byte ptr [base + disp]`  ->  0..255            (byte)
  * `movsx r32, byte ptr [base + disp]`  -> -128..127          (signed byte)
  * `lea r32, [src + src*k]`             ->  k * src
  * `lea r32, [src + src*k] ; shl r32, s`->  k * 2^s * src
  * `mov r32, src ; shl r32, s`          ->  2^s * src
  * `imul r32, src, k`                   ->  k * src
  * `xor r32, r32`                       ->  0 (constant)
Anything else is `OPEN` with the exact missing witness.

Output is deterministic and one row per variable site.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

# Reuse the classifier rather than re-implementing XBE access or decoding.
_spec = importlib.util.spec_from_file_location(
    "pio_free_demand", ROOT / "scripts" / "pio-free-demand.py")
pfd = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(pfd)

ClassifyError = pfd.ClassifyError
UINT32 = 0xFFFFFFFF
VARIABLE_AVAILABLE = pfd.VARIABLE_AVAILABLE  # 32
# How far back to look for the demand's definition before declaring it OPEN.
SLICE_WINDOW = 256


def _u32(v: int) -> int:
    return v & UINT32


def decode_block(xbe: Path, sections: list[dict], around: int, md) -> tuple[list, int]:
    """Decode a window that ENDS at `around`, aligned to a verified instruction start.

    The window start is found by scanning a small range of candidate starts and choosing
    the one whose forward decode reproduces an instruction beginning exactly at `around`.
    That is how a mid-instruction start is avoided without an external symbol table.
    """
    for back in range(SLICE_WINDOW, 0, -1):
        start = around - back
        if start < 0:
            continue
        try:
            raw = pfd.xbe_window(xbe, sections, start, back + 16)
        except ClassifyError:
            continue
        if len(raw) < back + 1:
            continue
        insns = pfd.decode_from(raw, start, md)
        # Keep only the instructions that lie inside the window.
        insns = [i for i in insns if start <= i.address <= around]
        if not insns:
            continue
        # The decode must reach `around` exactly -- that proves the alignment.
        if insns[-1].address == around:
            return insns, start
    raise ClassifyError(f"cannot find an instruction-aligned window ending at 0x{around:08X}")


def _is_control_flow(insn) -> bool:
    m = insn.mnemonic
    return (m.startswith("j") or m in ("call", "ret", "retn", "leave", "loop",
                                       "loope", "loopne", "int3", "hlt"))


def _reg_written(insn) -> str | None:
    """The register written by operand 0, if this instruction writes one."""
    if not insn.operands:
        return None
    op = insn.operands[0]
    if op.type != capstone.x86.X86_OP_REG:
        return None
    if insn.mnemonic in ("cmp", "test", "push", "jmp", "call"):
        return None
    return insn.reg_name(op.reg)


def _byte_load_range(insn, reg: str) -> dict | None:
    """`movzx`/`movsx r32, byte ptr [base+disp]` -> the byte's range."""
    if insn.mnemonic not in ("movzx", "movsx"):
        return None
    if len(insn.operands) != 2:
        return None
    dst, src = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if src.type != capstone.x86.X86_OP_MEM or src.size != 1:
        return None
    signed = insn.mnemonic == "movsx"
    return {
        "kind": "byte_load",
        "signed": signed,
        "lo": -128 if signed else 0,
        "hi": 127 if signed else 255,
        "source": f"{insn.mnemonic} {insn.op_str}",
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "base": insn.reg_name(src.mem.base) if src.mem.base else None,
        "index": insn.reg_name(src.mem.index) if src.mem.index else None,
        "disp": _u32(src.mem.disp),
    }


def _scale_of_lea(insn, reg: str) -> dict | None:
    """`lea r32, [src + src*k]` -> multiplier k, with src recorded."""
    if insn.mnemonic != "lea" or len(insn.operands) != 2:
        return None
    dst, src = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if src.type != capstone.x86.X86_OP_MEM:
        return None
    mem = src.mem
    if mem.base == 0 or mem.index == 0 or mem.base != mem.index or mem.disp != 0:
        return None
    return {
        "kind": "lea_scale",
        "k": 1 + mem.scale,
        "src": insn.reg_name(mem.index),
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


def _imul_scale(insn, reg: str) -> dict | None:
    """`imul r32, src, k` -> multiplier k."""
    if insn.mnemonic != "imul" or len(insn.operands) != 3:
        return None
    dst, src, imm = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if src.type != capstone.x86.X86_OP_REG or imm.type != capstone.x86.X86_OP_IMM:
        return None
    return {
        "kind": "imul_scale",
        "k": _u32(imm.imm),
        "src": insn.reg_name(src.reg),
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


def _shl_amount(insn, reg: str) -> dict | None:
    """`shl r32, imm` -> shift amount."""
    if insn.mnemonic != "shl" or len(insn.operands) != 2:
        return None
    dst, imm = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if imm.type != capstone.x86.X86_OP_IMM:
        return None
    return {
        "kind": "shl",
        "amount": imm.imm,
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


def _mov_from_reg(insn, reg: str) -> dict | None:
    """`mov r32, src` -> an alias of src."""
    if insn.mnemonic != "mov" or len(insn.operands) != 2:
        return None
    dst, src = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if src.type != capstone.x86.X86_OP_REG:
        return None
    return {
        "kind": "alias",
        "src": insn.reg_name(src.reg),
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


def _zeroing(insn, reg: str) -> dict | None:
    """`xor r32, r32` / `sub r32, r32` -> exactly 0."""
    if insn.mnemonic not in ("xor", "sub") or len(insn.operands) != 2:
        return None
    a, b = insn.operands
    if a.type != capstone.x86.X86_OP_REG or b.type != capstone.x86.X86_OP_REG:
        return None
    if insn.reg_name(a.reg) != reg or a.reg != b.reg:
        return None
    return {
        "kind": "constant",
        "lo": 0, "hi": 0,
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


def _mov_imm(insn, reg: str) -> dict | None:
    """`mov r32, imm32` -> exactly that constant."""
    if insn.mnemonic != "mov" or len(insn.operands) != 2:
        return None
    dst, src = insn.operands
    if dst.type != capstone.x86.X86_OP_REG or insn.reg_name(dst.reg) != reg:
        return None
    if src.type != capstone.x86.X86_OP_IMM:
        return None
    v = _u32(src.imm)
    return {
        "kind": "constant",
        "lo": v, "hi": v,
        "va": f"0x{insn.address:08X}",
        "bytes": insn.bytes.hex(),
        "source": f"{insn.mnemonic} {insn.op_str}",
    }


DEFINERS = (_byte_load_range, _scale_of_lea, _imul_scale, _shl_amount,
            _mov_from_reg, _zeroing, _mov_imm)


def define_of(insn, reg: str) -> dict | None:
    """How this instruction defines `reg`, or None if it does not / is unrecognised."""
    for fn in DEFINERS:
        d = fn(insn, reg)
        if d is not None:
            return d
    return None


def slice_demand(xbe: Path, sections: list[dict], site: dict, md) -> dict:
    """Prove a bound on the demand register, or record the exact missing witness."""
    demand_reg = site["demand_register"]
    cmp_va = int(site["cmp_va"], 16)

    try:
        insns, _ = decode_block(xbe, sections, cmp_va, md)
    except ClassifyError as exc:
        return {"verdict": "OPEN", "reason": str(exc), "chain": []}

    # Walk backward from the compare, following the demand register through its
    # definitions.  Stop at the first control-flow instruction (block entry), which is
    # where a join would be and where this tool refuses to guess.
    chain: list[dict] = []
    reg = demand_reg
    factors: list[int] = []
    shifts = 0

    for insn in reversed(insns[:-1] if insns and insns[-1].address == cmp_va else insns):
        if _is_control_flow(insn):
            return {
                "verdict": "OPEN",
                "reason": (f"reached control flow {insn.mnemonic!r} at "
                           f"0x{insn.address:08X} before the demand's definition; "
                           f"a join or call supplies the register"),
                "chain": chain,
                "open_at": f"0x{insn.address:08X}",
            }
        written = _reg_written(insn)
        if written != reg:
            continue
        d = define_of(insn, reg)
        if d is None:
            return {
                "verdict": "OPEN",
                "reason": (f"unrecognised definition of {reg} by "
                           f"'{insn.mnemonic} {insn.op_str}' at 0x{insn.address:08X}"),
                "chain": chain,
                "open_at": f"0x{insn.address:08X}",
            }
        chain.append(d)
        if d["kind"] == "byte_load":
            lo = d["lo"] * _prod(factors) * (2 ** shifts) if factors or shifts else d["lo"]
            hi = d["hi"] * _prod(factors) * (2 ** shifts) if factors or shifts else d["hi"]
            return _verdict(d, lo, hi, factors, shifts, chain)
        if d["kind"] == "constant":
            lo = d["lo"] * _prod(factors) * (2 ** shifts) if factors or shifts else d["lo"]
            hi = d["hi"] * _prod(factors) * (2 ** shifts) if factors or shifts else d["hi"]
            return _verdict(d, lo, hi, factors, shifts, chain)
        if d["kind"] in ("lea_scale", "imul_scale"):
            factors.append(d["k"])
            reg = d["src"]
            continue
        if d["kind"] == "shl":
            shifts += d["amount"]
            continue
        if d["kind"] == "alias":
            reg = d["src"]
            continue
        return {
            "verdict": "OPEN",
            "reason": f"unhandled definition kind {d['kind']!r}",
            "chain": chain,
        }

    return {
        "verdict": "OPEN",
        "reason": (f"no definition of {demand_reg} found between the block entry and "
                   f"the compare; the value is supplied earlier or by a call"),
        "chain": chain,
    }


def _prod(values: list[int]) -> int:
    out = 1
    for v in values:
        out *= v
    return out


def _verdict(defn: dict, lo: int, hi: int, factors: list[int], shifts: int,
             chain: list[dict]) -> dict:
    """Report the bound this slice actually established -- and no more.

    IMPORTANT: a byte load's *generic* range (0..255) bounds the demand's FINITENESS but
    is NOT a sufficiency proof.  The packet is explicit that "generic byte<=255 only
    establishes finiteness (255x10=2550), not sufficiency".  So a byte-sourced demand is
    reported as `OPEN` with the generic range recorded as an upper limit, and the missing
    witness named as the field's actual maximum.  Only a *constant* source (or a byte
    whose proven value range is already <= the available amount) can yield `BOUND`.

    Reporting `EXCEED` from the generic byte bound would be wrong in the other direction:
    it would assert a reachable over-demand without a feasibility witness, which the
    packet forbids ("On unbounded/unresolved input or uncertain feasibility, mark OPEN
    (not EXCEED, not BOUND)").
    """
    k = _prod(factors) * (2 ** shifts)
    out = {
        "source_form": defn["source"],
        "source_va": defn["va"],
        "source_kind": defn["kind"],
        "source_range": [lo, hi],
        "multiplier": k,
        "max_demand_if_source_at_max": hi,
        "available": VARIABLE_AVAILABLE,
        "margin_if_source_at_max": VARIABLE_AVAILABLE - hi,
        "chain": chain,
    }

    if defn["kind"] == "constant":
        # A proven constant is decidable outright.
        out["verdict"] = "BOUND" if hi <= VARIABLE_AVAILABLE else "EXCEED"
        out["max_demand"] = hi
        out["margin"] = VARIABLE_AVAILABLE - hi
        if out["verdict"] == "EXCEED":
            out["feasible_witness"] = (
                f"the demand is the constant {hi}, which exceeds the stub's "
                f"{VARIABLE_AVAILABLE}")
        return out

    # Byte-sourced: the generic range bounds finiteness only.
    out["byte_source"] = {
        "base": defn["base"], "index": defn["index"],
        "disp": f"0x{defn['disp']:08X}", "signed": defn["signed"],
    }
    out["bound_is_generic_byte"] = True
    out["verdict"] = "OPEN"
    out["reason"] = (
        f"demand = {k} x byte[{defn['base']}"
        f"{' + ' + defn['index'] if defn['index'] else ''} + 0x{defn['disp']:X}]; "
        f"the byte's GENERIC range 0..255 bounds the demand at {hi}, which is finiteness "
        f"only. Sufficiency needs the field's actual maximum: the stub supplies "
        f"{VARIABLE_AVAILABLE}, so the site is safe iff max_byte <= {VARIABLE_AVAILABLE // k}"
        f" (k={k})")
    out["missing_witness"] = (
        f"the actual maximum value of the byte field at +0x{defn['disp']:X} over all "
        f"feasible paths reaching 0x{chain[0]['va'] if chain else '?'}")
    out["sufficient_iff_max_byte_at_most"] = VARIABLE_AVAILABLE // k
    return out


def analyse(xbe: Path, analysis: Path, sites_json: Path) -> dict:
    sections = pfd.load_sections(analysis)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    data = json.loads(sites_json.read_text(encoding="utf-8"))

    rows = []
    for site in data["sites"]:
        if site["gate_form"] != "VARIABLE":
            continue
        res = slice_demand(xbe, sections, site, md)
        rows.append({
            "instruction_va": site["instruction_va"],
            "demand_register": site["demand_register"],
            "cmp_va": site["cmp_va"],
            "cmp_text": site["cmp_text"],
            "branch_va": site["branch_va"],
            "branch_predicate": site["branch_predicate"],
            "available": VARIABLE_AVAILABLE,
            **res,
        })
    rows.sort(key=lambda r: int(r["instruction_va"], 16))

    counts: dict[str, int] = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    return {
        "tool": "pio-free-demand-slice",
        "schema_version": 1,
        "available": VARIABLE_AVAILABLE,
        "variable_sites": len(rows),
        "verdict_counts": dict(sorted(counts.items())),
        "rows": rows,
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Backward demand slice for variable gates.")
    ap.add_argument("--xbe", type=Path, default=ROOT / "game" / "default.xbe")
    ap.add_argument("--analysis", type=Path,
                    default=ROOT / "game" / "mygame_analysis.json")
    ap.add_argument("--sites", type=Path,
                    default=ROOT / "docs" / "reviews" / "pio-free-title-demand-sites.json")
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args(argv)
    result = analyse(args.xbe, args.analysis, args.sites)
    text = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"wrote {args.out}: {result['verdict_counts']}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
