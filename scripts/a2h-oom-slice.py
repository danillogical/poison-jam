"""Parse archived JSRF run logs for the A2h allocation-failure chain.

Diagnostic tool for the `A2h-oom-causal-slice-r1` discovery. It binds, from a COMPLETE raw
`jsrf_run.log`, the ordinal-184 (`NtAllocateVirtualMemory`) invocation chain and the terminal
failure, so an attribution row rests on reproducible parsing rather than ad-hoc grepping.

Why this exists
---------------
This project has repeatedly been burned by ad-hoc log/disassembly parsing: a byte-width-only
scan that missed 182 writers, a disassembly started mid-instruction that capstone silently
rendered as plausible garbage, and a case-sensitive hash comparison that reported a false
mismatch. So this parser:

  * reads the COMPLETE log, never a capped display or a tail;
  * pairs each `ordinal 184` line with the allocation line that FOLLOWS it, so the return
    address and the size belong to the SAME invocation;
  * reports the invocation INDEX, so "was this call site reached before?" is answerable;
  * fails loudly on a malformed, truncated or contradictory log rather than returning a
    partial answer;
  * is deterministic: identical input gives byte-identical JSON.

It deliberately does NOT interpret a capped log's absence as a negative witness.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]

ORDINAL_184 = re.compile(
    r"ordinal 184 \(slot \d+\) esp=(0x[0-9A-Fa-f]+) ret=(0x[0-9A-Fa-f]+)")
ALLOC = re.compile(
    r"NtAllocateVirtualMemory: base=0x([0-9A-Fa-f]+) size=(\d+) type=(0x[0-9A-Fa-f]+)")
OOM = re.compile(r"out of memory \(requested (\d+), used (\d+)/(\d+)\)")
ICALL = re.compile(
    r"\[ICALL\] invalid target (0x[0-9A-Fa-f]+) tid=(\d+) esp=([0-9A-Fa-f]+) return=([0-9A-Fa-f]+)")

# The failing size this discovery is about.
FAIL_SIZE = 598869040


class LogError(Exception):
    """A log that cannot be bound. Always fatal, never a partial answer."""


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_log(path: Path) -> dict:
    """Bind the ordinal-184 chain, the OOM and the terminal ICALL from a complete log."""
    if not path.is_file():
        raise LogError(f"log not found: {path}")
    try:
        raw = path.read_text(encoding="utf-8", errors="strict")
    except (OSError, UnicodeDecodeError) as exc:
        raise LogError(f"cannot read {path} as UTF-8: {exc}") from exc
    if not raw.strip():
        raise LogError(f"log is empty: {path}")

    # Pair each ordinal-184 line with the allocation line that follows it, in order.
    # Walking positions guarantees the pairing is invocation-local rather than positional.
    invocations = []
    for m in ORDINAL_184.finditer(raw):
        tail = raw[m.end():]
        a = ALLOC.search(tail)
        if a is None:
            # An ordinal-184 line with no following allocation line is a contradiction:
            # either the log is truncated mid-invocation or the parser is wrong. Both are
            # fatal -- silently skipping would undercount the population.
            raise LogError(
                f"ordinal 184 at offset {m.start()} has no following allocation line "
                f"(truncated or malformed log)")
        invocations.append({
            "index": len(invocations),
            "offset": m.start(),
            "esp": m.group(1),
            "ret": m.group(2),
            "base": "0x" + a.group(1),
            "size": int(a.group(2)),
            "type": a.group(3),
        })

    ooms = [{"requested": int(x.group(1)), "used": int(x.group(2)),
             "arena": int(x.group(3))} for x in OOM.finditer(raw)]
    icalls = [{"target": x.group(1), "tid": int(x.group(2)),
               "esp": x.group(3), "return": x.group(4)} for x in ICALL.finditer(raw)]

    if not invocations:
        raise LogError(f"no ordinal-184 invocations found in {path}")

    failing = [i for i in invocations if i["size"] == FAIL_SIZE]

    return {
        "log": str(path),
        "log_sha256": sha256(path),
        "log_bytes": len(raw.encode("utf-8")),
        "invocations": invocations,
        "invocation_count": len(invocations),
        "failing_indices": [i["index"] for i in failing],
        "failing_size": FAIL_SIZE,
        "oom_events": ooms,
        "icall_events": icalls,
    }


def summarise(parsed: dict) -> dict:
    """Derive the comparisons the packet's rows depend on."""
    inv = parsed["invocations"]
    fails = parsed["failing_indices"]
    by_ret: dict[str, list[dict]] = {}
    for i in inv:
        by_ret.setdefault(i["ret"], []).append(i)

    out = {
        "invocation_count": parsed["invocation_count"],
        "failing_indices": fails,
        "oom_count": len(parsed["oom_events"]),
        "icall_count": len(parsed["icall_events"]),
        "distinct_return_vas": sorted(by_ret),
        # Always expose the per-site grouping, whether or not a failing size is present, so
        # a caller can inspect duplicate/conflicting invocations at one site without first
        # having to find the failure. An earlier version only set these keys when a failing
        # invocation existed, and the tests caught the resulting KeyError.
        "invocations_by_site": {
            ret: [{"index": s["index"], "size": s["size"], "esp": s["esp"], "type": s["type"]}
                  for s in invs]
            for ret, invs in sorted(by_ret.items())
        },
        "sites_reached_more_than_once": sorted(
            ret for ret, invs in by_ret.items() if len(invs) > 1),
    }
    if fails:
        fi = fails[0]
        fail_inv = inv[fi]
        same_site = by_ret.get(fail_inv["ret"], [])
        out["failing_call_site"] = fail_inv["ret"]
        out["failing_esp"] = fail_inv["esp"]
        out["same_site_invocations"] = [
            {"index": s["index"], "size": s["size"], "esp": s["esp"], "type": s["type"]}
            for s in same_site]
        out["same_site_count"] = len(same_site)
        if len(same_site) > 1:
            others = [s for s in same_site if s["index"] != fi]
            out["other_invocations_at_this_site"] = [
                {"index": s["index"], "size": s["size"], "esp": s["esp"]} for s in others]
            # do the ESPs differ? that means different frames
            esps = {s["esp"] for s in same_site}
            out["distinct_esps_at_site"] = sorted(esps)
            out["different_frames"] = len(esps) > 1
    return out


def compare(a: dict, b: dict) -> dict:
    """Field-by-field comparison of two bound archives."""
    sa, sb = summarise(a), summarise(b)
    keys = ["invocation_count", "failing_indices", "oom_count", "icall_count",
            "distinct_return_vas", "failing_call_site", "same_site_count",
            "other_invocations_at_this_site"]
    out = {}
    for k in keys:
        va, vb = sa.get(k), sb.get(k)
        out[k] = {"a": va, "b": vb, "same": va == vb}
    out["oom_tuple_same"] = a["oom_events"] == b["oom_events"]
    out["icall_tuple_same"] = (
        [(x["target"], x["esp"], x["return"]) for x in a["icall_events"]]
        == [(x["target"], x["esp"], x["return"]) for x in b["icall_events"]])
    # Compare the failing invocation SEMANTICALLY. A raw dict comparison also compares the
    # byte `offset` into each log file, which necessarily differs between two runs and would
    # report a spurious difference. The fields that matter are the call site, the frame, the
    # requested size and the allocation type.
    SEMANTIC = ("index", "esp", "ret", "base", "size", "type")
    fa = [{k: i[k] for k in SEMANTIC} for i in a["invocations"] if i["size"] == FAIL_SIZE]
    fb = [{k: i[k] for k in SEMANTIC} for i in b["invocations"] if i["size"] == FAIL_SIZE]
    out["failing_alloc_same"] = fa == fb
    out["failing_alloc_semantic"] = {"a": fa, "b": fb,
                                     "compared_fields": list(SEMANTIC)}
    return out


def load_sections(analysis: Path) -> list:
    """The XBE section table, from the analysis JSON.

    Local rather than imported so this module has no cross-tool dependency: the packet names
    THIS tool as the checked-in reproducer, and a reader must be able to run it alone.
    """
    try:
        data = json.loads(analysis.read_text(encoding="utf-8"))
        sections = data["sections"]
    except (OSError, ValueError, KeyError) as exc:
        raise LogError(f"cannot read analysis {analysis}: {exc}") from exc
    if not isinstance(sections, list) or not sections:
        raise LogError(f"analysis {analysis} has no sections")
    for s in sections:
        for key in ("name", "virtual_addr", "raw_addr", "raw_size"):
            if key not in s:
                raise LogError(f"analysis section missing {key!r}")
    return sections


def xbe_window(xbe: Path, sections: list, start: int, want: int) -> bytes:
    """Up to `want` bytes from `start`, clamped to the containing section's end."""
    for s in sections:
        va = int(s["virtual_addr"], 16)
        size = int(s["raw_size"])
        if va <= start < va + size:
            off = int(s["raw_addr"], 16) + start - va
            n = min(want, va + size - start)
            try:
                with xbe.open("rb") as fh:
                    fh.seek(off)
                    return fh.read(n)
            except OSError as exc:
                raise LogError(f"cannot read {xbe}: {exc}") from exc
    raise LogError(f"0x{start:08X} is not inside a file-backed section")


def verify_instruction(xbe: Path, sections: list, va: int, expect_bytes: str) -> dict:
    """Verify that `va` is a genuine instruction boundary decoding to `expect_bytes`.

    The packet requires a fixture proving that a MID-INSTRUCTION disassembly start is REJECTED
    rather than silently decoded as plausible garbage.  That hazard is real here: an earlier
    session began a decode one byte early and capstone returned plausible `.byte`/`jmp` output
    instead of an error, which then looked like evidence.

    So this is the disassembly surface the evidence record's instruction-byte citations rest
    on.  It rejects, rather than reports, when:
      * the requested bytes are not the bytes actually at `va`, or
      * decoding from `va` does not produce an instruction starting exactly at `va`, or
      * the decoded instruction's length differs from the expected bytes' length (which is what
        a misaligned start looks like: the same bytes decode to a different, longer instruction).
    """
    want = expect_bytes.replace(" ", "").lower()
    try:
        n = len(want) // 2
        if n == 0 or len(want) % 2:
            raise LogError(f"malformed expected-bytes {expect_bytes!r}")
        raw = xbe_window(xbe, sections, va, max(n, 16))
    except LogError:
        raise
    if len(raw) < n:
        raise LogError(f"0x{va:08X}: not enough bytes to verify")
    actual = raw[:n].hex()
    if actual != want:
        raise LogError(
            f"0x{va:08X}: bytes are {actual}, expected {want}")
    insns = decode_from(raw, va, _md())
    if not insns or insns[0].address != va:
        raise LogError(f"0x{va:08X}: decode does not begin at the requested VA")
    first = insns[0]
    if len(first.bytes) != n:
        raise LogError(
            f"0x{va:08X}: expected {n} bytes but the instruction decodes to "
            f"{len(first.bytes)} ({first.mnemonic} {first.op_str}) -- misaligned start?")
    return {
        "va": f"0x{va:08X}",
        "bytes": actual,
        "length": n,
        "text": f"{first.mnemonic} {first.op_str}",
    }


def decode_from(raw: bytes, va: int, md) -> list:
    """Decode instructions from `raw`, addressed starting at `va`."""
    return list(md.disasm(raw, va))


def _md():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    return md


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Bind the A2h ordinal-184 allocation-failure chain from archived logs, "
                    "and verify original-XBE instruction boundaries.")
    ap.add_argument("--log", type=Path, action="append", default=None,
                    help="a jsrf_run.log (repeat for comparison)")
    ap.add_argument("--xbe", type=Path, default=ROOT / "game" / "default.xbe")
    ap.add_argument("--analysis", type=Path, default=ROOT / "game" / "mygame_analysis.json")
    ap.add_argument("--verify", action="append", default=None, metavar="VA:BYTES",
                    help="verify an instruction boundary, e.g. 0x00149E24:8345dc20 "
                         "(repeatable)")
    ap.add_argument("--out", type=Path, default=None)
    args = ap.parse_args(argv)

    if not args.log and not args.verify:
        ap.error("give --log and/or --verify")

    result = {"tool": "a2h-oom-slice", "schema_version": 1}

    if args.verify:
        try:
            sections = load_sections(args.analysis)
        except LogError as exc:
            print(f"binding failed: {exc}")
            return 2
        verified = []
        for spec in args.verify:
            if ":" not in spec:
                print(f"binding failed: --verify needs VA:BYTES, got {spec!r}")
                return 2
            va_s, bytes_s = spec.split(":", 1)
            try:
                verified.append(verify_instruction(args.xbe, sections,
                                                   int(va_s, 16), bytes_s))
            except (LogError, ValueError) as exc:
                print(f"instruction verification failed: {exc}")
                return 2
        result["verified_instructions"] = verified

    if args.log:
        try:
            parsed = [parse_log(p) for p in args.log]
        except LogError as exc:
            print(f"binding failed: {exc}")
            return 2
        result["archives"] = [summarise(p) for p in parsed]
        if len(parsed) == 2:
            result["comparison"] = compare(parsed[0], parsed[1])

    text = json.dumps(result, indent=2) + "\n"
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8")
        print(f"wrote {args.out}")
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
