"""Measure REAL allocated (on-disk) size of a directory tree, sparse-aware.

PowerShell's Length property reports LOGICAL size, which overstates sparse and
compressed files enormously -- this project archives 2.36 GB sparse Partition images
per run, and their logical sum (1.08 TB) is meaningless against a 932 GB drive.

GetCompressedFileSizeW returns the actual allocation, so sparse files report their real
cost. Files that fail the call are counted as logical size and flagged, so a failure
cannot silently understate the total.
"""
import ctypes
import sys
from ctypes import wintypes
from pathlib import Path

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
GetCompressedFileSizeW = kernel32.GetCompressedFileSizeW
GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
GetCompressedFileSizeW.restype = wintypes.DWORD

INVALID = 0xFFFFFFFF


def allocated(p: Path):
    hi = wintypes.DWORD(0)
    lo = GetCompressedFileSizeW(str(p), ctypes.byref(hi))
    if lo == INVALID and ctypes.get_last_error() != 0:
        return None
    return (hi.value << 32) | lo


def walk(root: Path):
    logical = 0
    real = 0
    n = 0
    failed = 0
    sparse_saved = 0
    for p in root.rglob("*"):
        try:
            if not p.is_file():
                continue
        except OSError:
            continue
        try:
            lg = p.stat().st_size
        except OSError:
            continue
        alloc = allocated(p)
        if alloc is None:
            failed += 1
            alloc = lg
        logical += lg
        real += alloc
        if lg > alloc:
            sparse_saved += lg - alloc
        n += 1
    return n, logical, real, sparse_saved, failed


for arg in sys.argv[1:]:
    root = Path(arg)
    if not root.exists():
        print("  %-52s MISSING" % arg)
        continue
    n, logical, real, saved, failed = walk(root)
    print("  %s" % arg)
    print("    files:            %d" % n)
    print("    logical:          %.1f GB" % (logical / 1024**3))
    print("    REAL ON DISK:     %.1f GB" % (real / 1024**3))
    print("    reclaimed by sparse/compress: %.1f GB" % (saved / 1024**3))
    if failed:
        print("    ⚠ allocation query failed for %d files (counted logical)" % failed)
    print()
