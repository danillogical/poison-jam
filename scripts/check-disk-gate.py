"""Refuse to start a run when the host has less free space than a run can consume.

The failure this exists to catch is recorded: `logs/runs` reached 1,172 run
directories / 167 GB logical by 2026-09-28 and the mandated harness verification
could not run.  A run that dies of a full disk produces a *truncated* archive --
which reads like a guest hang, not like an environment failure -- so the gate
must fire **before** any child starts, not after the archive is written.

Measured cost of not having it: a full disk was diagnosed as a guest defect more
than once, and the recovery work (classifying 1,184 run directories to find what
was safe to remove) was done under pressure instead of ahead of time.

Design notes:

  * **Sparse-aware.** This project archives multi-GB `Partition*.img` save roots
    per run; their *logical* size overstates real cost by ~1000x.  Free space is
    compared against real allocation, using `GetCompressedFileSizeW`, the same
    primitive `scripts/disk-usage.py` uses.
  * **Fail closed.** A missing, unreadable or non-finite free-space reading is a
    refusal, not a pass.  A gate that cannot measure cannot authorize a run.
  * **No deletion.** This script only measures and refuses.  Reclaim candidates
    are reported by `scripts/logs-reclaim-plan.py`; removing them is an owner
    decision (plan T14).
"""
from __future__ import annotations

import argparse
import ctypes
import json
import shutil
import sys
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The floor is a *pre-run* budget: enough headroom for one archived run plus the
# build tree it is launched from.  A run's own archive is dominated by the save
# root and the collector dump, which have measured at a few GB on this host.
DEFAULT_FLOOR_GB = 50.0

_kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_GetCompressedFileSizeW = _kernel32.GetCompressedFileSizeW
_GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
_GetCompressedFileSizeW.restype = wintypes.DWORD
_INVALID = 0xFFFFFFFF


def allocated(path: Path) -> int | None:
    """Real on-disk allocation, or None when the platform cannot answer."""
    if sys.platform != "win32":
        try:
            return path.stat().st_size
        except OSError:
            return None
    hi = wintypes.DWORD(0)
    lo = _GetCompressedFileSizeW(str(path), ctypes.byref(hi))
    if lo == _INVALID and ctypes.get_last_error() != 0:
        return None
    return (hi.value << 32) | lo


def tree_allocation(root: Path) -> tuple[int, int, int]:
    """(files, allocated_bytes, unreadable_files) for a tree, sparse-aware."""
    files = 0
    total = 0
    unreadable = 0
    if not root.exists():
        return 0, 0, 0
    for path in root.rglob("*"):
        try:
            if not path.is_file():
                continue
        except OSError:
            unreadable += 1
            continue
        value = allocated(path)
        if value is None:
            unreadable += 1
            continue
        files += 1
        total += value
    return files, total, unreadable


def free_bytes(anchor: Path) -> int | None:
    """Free bytes on the volume holding `anchor`, or None when unmeasurable."""
    probe = anchor
    while not probe.exists() and probe.parent != probe:
        probe = probe.parent
    try:
        return shutil.disk_usage(str(probe)).free
    except OSError:
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--floor-gb", type=float, default=DEFAULT_FLOOR_GB,
                        help="refuse below this many free GB (default %(default)s)")
    parser.add_argument("--anchor", default=str(ROOT),
                        help="volume to measure; defaults to the game repository root")
    parser.add_argument("--runs-root", default=str(ROOT / "logs" / "runs"),
                        help="run archive root to report usage for")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument("--quiet", action="store_true", help="only print on refusal")
    args = parser.parse_args()

    anchor = Path(args.anchor)
    free = free_bytes(anchor)
    floor = int(args.floor_gb * 1024 ** 3)

    if free is None:
        print("DISK GATE: REFUSED - free space could not be measured on "
              f"{anchor}; a gate that cannot measure cannot authorize a run.",
              file=sys.stderr)
        return 2
    if args.floor_gb < 0:
        print("DISK GATE: REFUSED - --floor-gb must not be negative.", file=sys.stderr)
        return 2

    runs_root = Path(args.runs_root)
    run_files, run_bytes, run_unreadable = tree_allocation(runs_root)
    allowed = free >= floor

    record = {
        "gate": "jsrf-disk-gate/1",
        "anchor": str(anchor),
        "free_bytes": free,
        "free_gb": round(free / 1024 ** 3, 2),
        "floor_bytes": floor,
        "floor_gb": args.floor_gb,
        "allowed": allowed,
        "runs_root": str(runs_root),
        "runs_files": run_files,
        "runs_allocated_bytes": run_bytes,
        "runs_allocated_gb": round(run_bytes / 1024 ** 3, 2),
        "runs_unreadable_files": run_unreadable,
    }

    if not allowed:
        print(
            f"DISK GATE: REFUSED - {record['free_gb']} GB free on {anchor}, "
            f"floor is {args.floor_gb} GB.\n"
            f"  logs/runs currently holds {run_files} files / "
            f"{record['runs_allocated_gb']} GB allocated.\n"
            "  Reclaim candidates: python -X utf8 scripts\\logs-reclaim-plan.py\n"
            "  Removing archived runs is an owner decision (plan T14); this gate\n"
            "  does not delete anything.",
            file=sys.stderr)
    elif not args.quiet:
        print(f"DISK GATE: PASS - {record['free_gb']} GB free on {anchor} "
              f"(floor {args.floor_gb} GB); logs/runs {run_files} files / "
              f"{record['runs_allocated_gb']} GB allocated")

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
    return 0 if allowed else 2


if __name__ == "__main__":
    sys.exit(main())
