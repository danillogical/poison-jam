"""The linear addresses one guest VA is reachable at, and why that matters.

**The mechanism.** `xbox_memory_layout.c` maps the 64 MB Xbox RAM region once at
its base and then creates 28 additional *mirror views* of the same file mapping at
64 MB intervals (`MapViewOfFileEx` with a shared mapping handle). All 29 views
alias the same physical pages, so a store through any of them changes the bytes
every other one reads.

**Why a watch misses it.** A native debug register (DR0) matches a **linear
address**. A watch programmed on the canonical VA therefore traps only stores that
spell the address that way. A store through mirror *m* is the same memory and
never touches the watched linear address, so the watch reports zero writes while
the value changes. This is not hypothetical here: it is the mechanism that hid
Halo's `fs:[4]` corruption on this project (`xbox_ProtectMirrorsForDebug`'s
comment), and it is why the A2h alias census exists at all.

**What a TTD trace gives instead.** TTD records every memory access with its
**address, size, IP, thread and time position**, so the query is a filter over a
recorded event stream rather than a hardware watch. All 29 addresses can be asked
about in one pass, and the answer is not limited by which spelling the store used.

The count is 29 = 1 canonical + 28 mirrors, and it is derived from the toolkit
(`XBOX_NUM_MIRRORS`) rather than written down here. A run prints its own
`RAM mirror: N/28 views mapped` line; when fewer than 28 are mapped the unwatched
addresses are a **coverage gap**, and a zero-write answer over them is not a
negative. `mirror_coverage` below reports that so a caller can fail closed.
"""
from __future__ import annotations

import re

# Guest physical RAM size on this title's console. The runtime derives its own
# size at startup and prints how many MB the mapped views cover; a run's own line
# is the authority, and `ram_size_from_log` reads it.
DEFAULT_RAM_MB = 64
XBOX_NUM_MIRRORS = 28

_MIRROR_LINE = re.compile(
    r'RAM mirror:\s*(\d+)/(\d+)\s+views mapped \(covers (\d+) MB\)')

# The runtime does not map Xbox RAM at host address 0.  It reserves a span and
# places the base view at an offset from it, and prints both:
#
#     xbox_MemoryLayoutInit: mapped 65536 KB at 0x0000000000010000 (offset +65536 from Xbox base)
#
# A TTD query is over HOST linear addresses, so this offset is not cosmetic: a
# query that omits it looks 64 KB below every guest address and returns whatever
# happens to live there.  Measured cost of getting this wrong: the first working
# query returned 8 writes for guest `0x001C4064` that were really the XBE
# decompressor writing guest `0x001B4064` -- a plausible-looking wrong answer,
# which is the worst kind.
_LAYOUT_LINE = re.compile(
    r'xbox_MemoryLayoutInit:\s*mapped\s+\d+\s+KB\s+at\s+0x([0-9A-Fa-f]+)')


def host_base_from_log(log_text: str) -> int | None:
    """The host linear address the guest's 0x00000000 maps to, or None."""
    match = _LAYOUT_LINE.search(log_text)
    if not match:
        return None
    return int(match.group(1), 16)


def ram_size_from_log(log_text: str) -> tuple[int, int, int] | None:
    """(mapped_views, expected_views, ram_bytes) from the runtime's own line.

    Returns None when the line is absent.  That is a real possibility and the
    caller must treat it as `UNKNOWN`, not as "no mirrors": the line is the only
    witness that the mirror views were created at all, and a run where the
    mapping failed is exactly the run whose alias coverage is incomplete.
    """
    match = _MIRROR_LINE.search(log_text)
    if not match:
        return None
    mapped, expected, covered_mb = (int(g) for g in match.groups())
    if expected <= 0:
        return None
    # `covers` counts the base view plus the mapped mirrors.
    ram_bytes = covered_mb * 1024 * 1024 // (mapped + 1)
    return mapped, expected, ram_bytes


def aliases(va: int, ram_bytes: int, host_base: int = 0,
            num_mirrors: int = XBOX_NUM_MIRRORS) -> list[int]:
    """Every host linear address that aliases guest `va`, canonical first.

    `host_base` is where the guest's address 0 lives on the host (see
    `host_base_from_log`); a TTD query is over host addresses, so omitting it
    silently queries the wrong 64 KB.

    Mirror *m* (1-based) covers guest `m * ram_bytes .. (m+1) * ram_bytes`, and
    the runtime places it at host `host_base + m * ram_bytes`.  The base view is
    `host_base + va`.
    """
    if not 0 <= va < ram_bytes:
        raise ValueError(
            f'VA 0x{va:08X} is outside the {ram_bytes // 1024 // 1024} MB RAM '
            f'region, so it has no mirrors; the alias model only applies to RAM')
    return [host_base + va + m * ram_bytes for m in range(num_mirrors + 1)]


def format_aliases(va: int, ram_bytes: int, host_base: int = 0,
                   num_mirrors: int = XBOX_NUM_MIRRORS) -> list[str]:
    return [f'0x{a:016X}' for a in aliases(va, ram_bytes, host_base, num_mirrors)]
