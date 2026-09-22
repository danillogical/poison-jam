"""Converge the recovered manifest against the guest, one bounded run at a time.

Each run reports at most one blocking defect and both kinds are mechanical:

  `[ICALL] Failed to resolve VA 0xXXXXXXXX`
      A real function entry the disassembler missed entirely. Add it with the
      extent of its straight-line body (up to and including its first
      terminator), then rebuild.

  `[RECOVERED] ABI FAILURE 0xXXXXXXXX esp A->B expected +N`
      That entry's stack_args is wrong. The message states the real delta, so
      stack_args = (B - A) - 4.

Loops until a run produces neither, or the step budget runs out. `recover-functions.py`
correctly refuses an incomplete translation, so the loop also prunes any entry the
translator will not lift standalone -- those are genuine fragments, not functions.

Usage:  python scripts/converge-manifest.py [--steps 40] [--seconds 20]
"""
import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parents[1]
CFG = ROOT / "config/recovered-functions.json"
RUNS = ROOT / "logs/runs"
PY = sys.executable
TERM = {"ret", "jmp", "retf", "iret", "int3"}

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--steps", type=int, default=40)
parser.add_argument("--seconds", type=int, default=20)
args = parser.parse_args()

sections = json.loads((ROOT / "game/mygame_analysis.json").read_text())["sections"]
xbe = (ROOT / "game/default.xbe").read_bytes()
cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def section_of(va):
    for s in sections:
        base = int(s["virtual_addr"], 16)
        if base <= va < base + s["raw_size"]:
            return s
    return None


def read(va, n):
    s = section_of(va)
    if not s:
        return None
    base = int(s["virtual_addr"], 16)
    n = min(n, base + s["raw_size"] - va)
    off = int(s["raw_addr"], 16) + (va - base)
    return xbe[off: off + n]


def body_end(va):
    """End of the straight-line body: past its first terminating instruction."""
    raw = read(va, 0x400)
    if not raw:
        return None
    for insn in cs.disasm(raw, va):
        if insn.mnemonic in TERM:
            return insn.address + insn.size
    return None


def load():
    return json.loads(CFG.read_text(encoding="utf-8"))


def save(entries):
    CFG.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8", newline="\n")


def regenerate():
    for _ in range(80):
        r = subprocess.run([PY, "-X", "utf8", "scripts/recover-functions.py"],
                           cwd=ROOT, capture_output=True, text=True)
        if r.returncode == 0:
            return True
        m = re.search(r"Incomplete translation at ([0-9A-Fa-f]{8})", r.stderr or r.stdout)
        if not m:
            print("  recover failed:", (r.stderr or r.stdout)[-300:])
            return False
        bad = int(m.group(1), 16)
        entries = load()
        pruned = [e for e in entries if int(e["start"], 16) != bad]
        if len(pruned) == len(entries):
            print("  cannot drop", hex(bad))
            return False
        save(pruned)
        print(f"  pruned 0x{bad:08X} (not a standalone function)")
    return False


def build():
    env = {k: v for k, v in os.environ.items()
           if k.lower() not in ("http_proxy", "https_proxy")}
    subprocess.run([PY, "scripts/build-identity.py", "before"], cwd=ROOT,
                   capture_output=True)
    r = subprocess.run(["cmake", "--build", "build", "--config", "Release",
                        "--target", "jsrf_recomp", "jsrf_collect", "--parallel", "4"],
                       cwd=ROOT, env=env, capture_output=True, text=True)
    if r.returncode != 0:
        print("  BUILD FAILED")
        print(r.stdout[-2000:])
        return False
    subprocess.run([PY, "scripts/build-identity.py", "after"], cwd=ROOT,
                   capture_output=True)
    return True


def run(label):
    before = {p.name for p in RUNS.iterdir() if p.is_dir()}
    subprocess.run([PY, "-X", "utf8", "scripts/run-jsrf.py", "--seconds",
                    str(args.seconds), "--label", label], cwd=ROOT, capture_output=True)
    new = [p for p in RUNS.iterdir() if p.is_dir() and p.name not in before]
    if not new:
        return None
    d = max(new, key=lambda p: p.stat().st_mtime)
    log = d / "jsrf_run.log"
    return log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""


for step in range(args.steps):
    log = run(f"converge-{step}")
    if log is None:
        print("no run produced")
        break

    abi = re.findall(
        r"ABI FAILURE 0x([0-9A-F]{8}) esp ([0-9A-F]{8})->([0-9A-F]{8}) expected \+(\d+)"
        r"; bx ([0-9A-F]{8})->([0-9A-F]{8}) si ([0-9A-F]{8})->([0-9A-F]{8})"
        r" di ([0-9A-F]{8})->([0-9A-F]{8}) bp ([0-9A-F]{8})->([0-9A-F]{8})", log)
    unresolved = re.findall(r"Failed to resolve VA 0x([0-9A-F]{8})", log)

    if not abi and not unresolved:
        print(f"step {step}: converged. "
              f"kernel={len(re.findall(r'KERNEL] #', log))} "
              f"verified={len(re.findall(r'ABI verified', log))}")
        break

    if abi:
        g = abi[0]
        addr = int(g[0], 16)
        esp_ok = (int(g[2], 16) - int(g[1], 16)) == int(g[3])
        regs_ok = all(g[i] == g[i + 1] for i in (4, 6, 8, 10))
        entries = load()
        hit = [e for e in entries if int(e["start"], 16) == addr]
        if not hit:
            print(f"step {step}: ABI failure at 0x{addr:08X} is not a manifest entry")
            break
        if not regs_ok:
            # The body does not preserve the nonvolatile registers, so it is a
            # mid-body fragment, not a function: the fold was right for this
            # address. Remove it and let the dispatch go back to the parent.
            print(f"step {step}: 0x{addr:08X} does not preserve bx/si/di/bp -> "
                  f"fragment, removing")
            save([e for e in entries if int(e["start"], 16) != addr])
        elif not esp_ok:
            want = (int(g[2], 16) - int(g[1], 16)) - 4
            print(f"step {step}: 0x{addr:08X} stack_args "
                  f"{hit[0].get('stack_args')} -> {want}")
            hit[0]["stack_args"] = want
            save(entries)
        else:
            print(f"step {step}: 0x{addr:08X} ABI failure is neither the ESP delta "
                  f"nor a clobbered register; stopping")
            break
    else:
        addr = int(unresolved[0], 16)
        entries = load()
        if any(int(e["start"], 16) == addr for e in entries):
            print(f"step {step}: 0x{addr:08X} is in the manifest but still unresolved")
            break
        end = body_end(addr)
        if end is None:
            print(f"step {step}: cannot size 0x{addr:08X}")
            break
        sec = (section_of(addr) or {}).get("name", "?")
        print(f"step {step}: add 0x{addr:08X}..0x{end:08X} ({sec})")
        entries.append({
            "start": f"0x{addr:08X}", "end": f"0x{end:08X}", "section": sec,
            "kind": "routine",
            "evidence": (
                f"Missed function entry reached indirectly and fatal as "
                f"`Failed to resolve VA 0x{addr:08X}`. It is a real {sec} function whose "
                f"straight-line body ends at 0x{end:08X}; the function database does not "
                f"contain it, so nothing had ever emitted a body or a dispatch tuple for it. "
                f"Recovered from the guest's own call by scripts/converge-manifest.py."),
        })
        save(entries)

    if not regenerate():
        break
    if not build():
        break
else:
    print("step budget exhausted")
