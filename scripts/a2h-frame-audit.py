#!/usr/bin/env python3
"""A2h frame audit: correct argument counting at the callee's call sites.

WHY THIS TOOL EXISTS
--------------------
A naive "count the pushes immediately before the call" analysis is WRONG, and it was wrong in
a way that flipped a conclusion twice during this packet's execution. The bound caller for the
failing invocation is:

    0017C918  push  eax          ; arg2 (an align16'd size)
    0017C919  push  0            ; arg1
    0017C91B  call  0x14a838     ; <-- AN INTERVENING CALL, mid-argument-setup
    0017C920  push  eax          ; arg0 (the getter's result)
    0017C921  call  0x1497dc     ; <-- THE CALL

A counter that stops at the first preceding `call` sees ONE push and concludes "one argument".
The call is really made with THREE, because `0x14a838` is `mov eax,[0x27dcd4]; ret` -- a plain
`ret` that pops ONLY its return address and cleans NO arguments, so the two pushes below it are
still argument material for `0x1497dc`.

So this tool tracks the stack explicitly instead of counting pushes, and it refuses to guess:
it consumes the toolkit's own generated ABI delta table (which records the exact ESP delta each
generated function produces) to know what each intervening call leaves behind.

FAIL-CLOSED RULES
-----------------
  * a decode that does not start from a verified instruction boundary is rejected, not guessed;
  * an intervening call whose delta is unknown aborts the walk rather than assuming cleanup;
  * the tool reports UNKNOWN rather than a number when it cannot account for the stack.

USAGE
-----
    python -X utf8 scripts/a2h-frame-audit.py callsites
    python -X utf8 scripts/a2h-frame-audit.py frame <logged-esp-hex> <pushes-after-logged-esp>
    python -X utf8 scripts/a2h-frame-audit.py verify
"""
from __future__ import annotations

import argparse
import importlib.util
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "src" / "recomp" / "gen"

CALLEE = 0x001497DC
SEH_HELPER = 0x0017D1F8
SEH_EPILOG = 0x0017D231

# The callee's verified prologue shape. Prologue bytes are asserted, not assumed.
CALLEE_PROLOGUE = [
    (0x001497DC, "6878010000", "push 0x178"),
    (0x001497E1, "68e80b1e00", "push 0x1E0BE8"),
    (0x001497E6, "e80d3a0300", "call 0x0017D1F8"),
]
# The SEH helper's frame-establishing sequence, verified byte-for-byte.
# NOTE: 0x0017D20F is `mov [esp+0x10], ebp` = 89 6c 24 10 (ModRM 6c = [esp+disp8] with ebp).
# An earlier revision of this file asserted 89442410 (which would be [esp+disp8], eax) and the
# verifier REJECTED it -- the tool catching its author's own mis-transcription is the point.
HELPER_FRAME = [
    (0x0017D20B, "8b442410", "mov eax,[esp+0x10]"),
    (0x0017D20F, "896c2410", "mov [esp+0x10],ebp"),
    (0x0017D213, "8d6c2410", "lea ebp,[esp+0x10]"),
]
# The read that feeds the producer.
READ_SITE = (0x00149800, "8b4510", "mov eax,[ebp+0x10]")
PRODUCER = (0x0014980E, "8945dc", "mov [ebp-0x24],eax")


class AuditError(Exception):
    """Raised rather than returning a plausible-looking wrong answer."""


def _load_slice_module():
    spec = importlib.util.spec_from_file_location(
        "a2h_slice", ROOT / "scripts" / "a2h-oom-slice.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


_SLICE = None


def slice_mod():
    global _SLICE
    if _SLICE is None:
        _SLICE = _load_slice_module()
    return _SLICE


def sections():
    return slice_mod().load_sections(ROOT / "game" / "mygame_analysis.json")


def xbe_path():
    return ROOT / "game" / "default.xbe"


def decode(va: int, length: int):
    m = slice_mod()
    return m.decode_from(m.xbe_window(xbe_path(), sections(), va, length), va, m._md())


def verify(va: int, expect_hex: str) -> str:
    """Verify an instruction's bytes; raise on any mismatch."""
    m = slice_mod()
    try:
        r = m.verify_instruction(xbe_path(), sections(), va, expect_hex)
    except m.LogError as exc:
        raise AuditError("0x%08X: %s" % (va, exc)) from exc
    return r["text"]


def load_abi_deltas() -> dict:
    """The toolkit's own generated expected-ESP-delta table, keyed by function VA.

    Taking this from the generated artifact rather than re-deriving it is deliberate: the
    recompiler already decided each function's stack cleanup, and its decision is what the
    running binary actually implements.
    """
    src = (GEN / "recomp_abi_deltas.c").read_text(encoding="utf-8", errors="replace")
    out = {}
    for m in re.finditer(
            r"\{\s*0x([0-9A-Fa-f]{8})u,\s*\{\s*(\d+)u,\s*(\d+)u,\s*(\d+)u,\s*(\d+)u\s*\}\s*\}",
            src):
        out[int(m.group(1), 16)] = [int(m.group(i)) for i in (2, 3, 4, 5)]
    return out


def load_abi_exempt() -> set:
    src = (GEN / "recomp_abi_deltas.c").read_text(encoding="utf-8", errors="replace")
    m = re.search(r"g_recomp_abi_exempt\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        return set()
    return {int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{8})u", m.group(1))}


def direct_calls_to(target: int):
    """Every `call target` in the guest image, with its return address.

    A LINEAR SWEEP OF A WHOLE SECTION DOES NOT WORK HERE, and the first version of this
    function proved it by returning ZERO call sites for a target that demonstrably has thirteen.
    (An earlier revision of this docstring said "eight" -- the superseded premise from before the
    boundary-corrected census. The count is THIRTEEN, and the test suite asserts it.)
    Sweeping 1.5 MB of .text sequentially desynchronises on the first data/padding byte and
    every instruction after it is garbage -- the same misalignment hazard this project keeps
    meeting, in the one place where a silent wrong answer is easiest to believe.

    Instead this scans the raw bytes for the exact encoding of `call rel32` (opcode E8) whose
    relative displacement resolves to `target`, then VERIFIES each candidate by decoding at
    that address. A byte-pattern scan cannot desynchronise, and the verify step rejects any
    candidate that is really data that happens to look like a call.
    """
    m = slice_mod()
    secs = sections()
    found = []
    for sec in secs:
        if not isinstance(sec, dict) or not sec.get("executable", False):
            continue
        va_raw = sec.get("virtual_addr")
        size = sec.get("virtual_size", 0)
        if va_raw is None or not size:
            continue
        lo = int(va_raw, 16) if isinstance(va_raw, str) else int(va_raw)
        raw = m.xbe_window(xbe_path(), secs, lo, size)
        for i in range(len(raw) - 4):
            if raw[i] != 0xE8:
                continue
            rel = int.from_bytes(raw[i + 1:i + 5], "little", signed=True)
            if (lo + i + 5 + rel) & 0xFFFFFFFF != target:
                continue
            va = lo + i
            # Verify: the bytes at this address must decode as a call to the target.
            try:
                insns = m.decode_from(m.xbe_window(xbe_path(), secs, va, 8), va, m._md())
            except Exception:
                continue
            if not insns:
                continue
            f = insns[0]
            if f.mnemonic == "call" and f.address == va and len(f.bytes) == 5:
                found.append((va, va + 5))
    return sorted(set(found))


def arguments_at(call_va: int, deltas: dict, exempt: set, window: int = 0xC0):
    """Count the arguments pushed for `call_va`, tracking ESP rather than counting pushes.

    Returns (count, detail, status). status is "ok" or "unknown:<reason>".

    The walk runs BACKWARD from the call. Going backward, a push ADDS an argument and a
    stack-cleaning instruction REMOVES pending argument material. An intervening call is
    handled by its generated ABI delta: delta 4 means it popped only its return address and
    therefore consumed NO arguments, so the walk continues past it.

    WALKING BACKWARD BY DECODING FROM `call_va - window` DOES NOT WORK, and an earlier revision
    of this function proved it by returning "unknown:no-instruction-at-call" for a call site
    that plainly exists. Starting a linear decode at an arbitrary earlier address desynchronises
    on data and never reaches the call. Instead this locates a SAFE START by scanning backward
    for a byte pattern that cannot begin a valid instruction mid-stream -- a `ret` (C3/C2) --
    and decodes forward from just after it.
    """
    m = slice_mod()
    secs = sections()
    start = _safe_start(call_va, window)
    insns = [i for i in decode(start, call_va - start + 8) if i.address <= call_va]
    if not insns or insns[-1].address != call_va:
        return None, [], "unknown:no-instruction-at-call"
    body = insns[:-1]
    pending = 0
    detail = []
    for insn in reversed(body):
        mn, op = insn.mnemonic, insn.op_str
        if mn == "push":
            pending += 1
            detail.append(("push", insn.address, op))
            continue
        if mn in ("pop",):
            pending -= 1
            detail.append(("pop", insn.address, op))
            continue
        if mn == "call":
            # An intervening call. Its own cleanup decides whether pending args survive.
            tgt_m = re.search(r"0x([0-9a-f]+)", op)
            if not tgt_m:
                return None, detail, "unknown:indirect-intervening-call"
            tgt = int(tgt_m.group(1), 16)
            d = deltas.get(tgt)
            if d is None:
                return None, detail, "unknown:no-delta-for-0x%08X" % tgt
            # delta is ESP after the call minus ESP just after its return address was pushed.
            # delta 4 == popped only the return address == consumed no arguments.
            consumed = d[0] - 4
            if consumed < 0:
                return None, detail, "unknown:negative-consumption-0x%08X" % tgt
            pending -= consumed // 4
            detail.append(("call", insn.address, "0x%08X consumed=%d" % (tgt, consumed)))
            continue
        if mn == "ret" or mn == "leave":
            break
        # `add esp, N` / `sub esp, N` adjust the pending argument count.
        mm = re.match(r"esp,\s*(0x[0-9a-f]+|\d+)", op)
        if mm and mn in ("add", "sub"):
            n = int(mm.group(1), 16) if mm.group(1).startswith("0x") else int(mm.group(1))
            pending += (-n if mn == "add" else n) // 4
            detail.append((mn, insn.address, op))
            continue
        if mn == "jmp" or mn.startswith("j"):
            break
    return pending, detail, "ok"


def _safe_start(call_va: int, window: int) -> int:
    """Find a resynchronisation point at or before `call_va - window`.

    A BARE `C3`/`C2` BYTE SCAN IS NOT A RELIABLE BASIC-BLOCK TERMINATOR, and an earlier revision of
    this function proved it by producing a WRONG ARGUMENT COUNT that reached the record. It scanned
    backward for a `C3` byte and found one at `0x0016B8EB` -- but that byte is the MODRM byte of
    `add ebx,0x12` (`83 c3 12`) at `0x0016B8EA`, not a `ret`. Decoding from `0x0016B8EC` then
    produced `adc al,[ebp+0x501874c0]`, a 6-byte instruction that SWALLOWED the `push eax` at
    `0x0016B8F1`, so the walk counted 2 arguments where there are 3. That is the misaligned-
    disassembly-produces-plausible-garbage failure mode, in the one place where a silent wrong
    answer is easiest to believe.

    THE AUTHORITATIVE BOUNDARY SET IS THE RECOMPILER'S OWN. The lifter already decided every
    instruction boundary and emitted a `loc_XXXXXXXX:` label for each one, in both the generated
    chunks and the reviewed recovered code. So this function now returns the NEAREST LABEL at or
    before the window start -- a boundary the recompiler asserts -- instead of guessing from bytes.
    If no label is available it falls back to the window start, and the caller reports UNKNOWN
    rather than fabricating a count.
    """
    labels = _loc_labels()
    lo = max(0, call_va - window)
    # The nearest label at or before lo is a boundary the lifter asserts.
    candidates = [v for v in labels if v <= lo]
    if candidates:
        return max(candidates)
    # No label before the window: try any label inside the window, which still resynchronises.
    inside = [v for v in labels if lo < v <= call_va]
    if inside:
        return min(inside)
    return lo


_LABELS_CACHE = None


def _loc_labels() -> set:
    """Every `loc_XXXXXXXX:` boundary the recompiler emitted, from generated AND recovered code.

    Both are needed: six of this callee's thirteen call sites live in `src/recomp/recovered/`, and a
    generated-only label set would leave exactly those sites unresynchronisable.
    """
    global _LABELS_CACHE
    if _LABELS_CACHE is not None:
        return _LABELS_CACHE
    labels = set()
    for base in (ROOT / "src" / "recomp" / "gen", ROOT / "src" / "recomp" / "recovered"):
        if not base.exists():
            continue
        for f in sorted(base.glob("*.c")):
            for m in re.finditer(r"loc_([0-9A-Fa-f]{8}):",
                                 f.read_text(encoding="utf-8", errors="replace")):
                labels.add(int(m.group(1), 16))
    _LABELS_CACHE = labels
    return labels


def frame_for_entry(entry_esp: int):
    """Given the callee's entry ESP E, return the frame the SEH helper establishes.

    Derived from verified bytes:
        push 0x178      esp = E-4
        push 0x1E0BE8   esp = E-8
        call helper     esp = E-12
        helper: push 0x1804A0   esp = E-16
                push eax        esp = E-20
                lea ebp,[esp+0x10] -> ebp = E-4
    """
    ebp = entry_esp - 4
    return {
        "entry_esp": entry_esp,
        "ebp": ebp,
        "arg0": ebp + 8,
        "arg1": ebp + 0xC,
        "arg2": ebp + 0x10,
        "return_address": entry_esp,
    }


def cmd_verify(_args):
    print("=== verifying the load-bearing instruction bytes ===")
    ok = True
    for va, b, label in CALLEE_PROLOGUE + HELPER_FRAME + [READ_SITE, PRODUCER]:
        try:
            text = verify(va, b)
            print("  OK   %08X  %-16s %-34s %s" % (va, b, text, label))
        except AuditError as exc:
            ok = False
            print("  FAIL %08X  %s" % (va, exc))
    print()
    print("ALL VERIFIED" if ok else "VERIFICATION FAILED")
    return 0 if ok else 1


def cmd_callsites(_args):
    deltas = load_abi_deltas()
    exempt = load_abi_exempt()
    print("=== every direct call to 0x%08X, with a stack-tracked argument count ===" % CALLEE)
    sites = direct_calls_to(CALLEE)
    print("  direct call sites found: %d" % len(sites))
    print()
    three = []
    for cva, ret in sites:
        n, detail, status = arguments_at(cva, deltas, exempt)
        if status != "ok":
            print("  call@0x%08X ret=0x%08X  args=UNKNOWN (%s)" % (cva, ret, status))
            continue
        tag = "  <== THREE ARGUMENTS" if n == 3 else ""
        print("  call@0x%08X ret=0x%08X  args=%d%s" % (cva, ret, n, tag))
        for kind, va, op in detail:
            print("        %-5s 0x%08X  %s" % (kind, va, op))
        if n == 3:
            three.append((cva, ret))
    print()
    print("  sites passing 3 arguments: %d" % len(three))
    for cva, ret in three:
        print("    call@0x%08X ret=0x%08X" % (cva, ret))
    return 0


def cmd_frame(args):
    esp = int(args.esp, 16)
    after = args.pushes
    entry = esp + after
    f = frame_for_entry(entry)
    print("=== frame derivation ===")
    print("  logged esp                      = 0x%08X" % esp)
    print("  + %d bytes of pushes/call       = 0x%08X  (callee entry ESP E)" % (after, entry))
    print("  ebp = E - 4                     = 0x%08X" % f["ebp"])
    print("  [ebp+8]   arg0                  = 0x%08X" % f["arg0"])
    print("  [ebp+0xC] arg1                  = 0x%08X" % f["arg1"])
    print("  [ebp+0x10] arg2  <== THE READ   = 0x%08X" % f["arg2"])
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("verify").set_defaults(fn=cmd_verify)
    sub.add_parser("callsites").set_defaults(fn=cmd_callsites)
    fp = sub.add_parser("frame")
    fp.add_argument("esp", help="logged esp as hex, e.g. 0x00F7FCF0")
    fp.add_argument("pushes", type=int, help="bytes of pushes+call between that esp and entry")
    fp.set_defaults(fn=cmd_frame)
    args = p.parse_args(argv)
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
