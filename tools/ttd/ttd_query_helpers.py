"""Helpers shared by the TTD tools.

`resolve_cdb` lives here rather than in one of the tools because three of them need
it (`ttd-query.py`, `ttd-terminal.py`), and a second copy is a second thing to get
wrong: the WinDbg package path carries a version, and a tool with a stale copy would
fail to find a debugger that is installed.
"""
from __future__ import annotations

import shutil
from pathlib import Path


def resolve_cdb() -> str | None:
    """cdb.exe, preferring PATH and falling back to the WinDbg package."""
    found = shutil.which('cdb') or shutil.which('cdb.exe')
    if found:
        return found
    roots = [Path(r'C:\Program Files\WindowsApps'),
             Path(r'C:\Program Files (x86)\Windows Kits\10\Debuggers')]
    for root in roots:
        if not root.is_dir():
            continue
        for candidate in root.glob('**/cdb.exe'):
            return str(candidate)
    return None
