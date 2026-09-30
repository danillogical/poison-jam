"""Re-apply the six A4b2 `jsrf_watch_store` observation hooks after a regeneration.

`tools.recomp` owns `src/recomp/gen/*.c` and rewrites them wholesale, so the
hooks the A4b2 packet hand-edited into the generated tree are erased by every
full pass.  The previous pass re-applied them by hand, which is exactly the
kind of step that silently does nothing: the hook is *observation only*, so a
forgotten one does not fail a build -- it makes the run's watch artifact empty
and reads as "the guest never wrote there".

Each hook is anchored on the **guest** label or store the translator emits,
never on a line number, because line numbers move.  An anchor that does not
match exactly once is an error: the script refuses to guess.  Running it twice
is a no-op.

Usage::

    python -X utf8 scripts/apply-a4b2-hooks.py --check   # report, change nothing
    python -X utf8 scripts/apply-a4b2-hooks.py --apply

Exit 0 when every hook is present (after --apply) or every anchor resolves
uniquely (--check); 1 on a missing/ambiguous anchor; 2 if it cannot run.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EXTERN = ('extern void jsrf_watch_store(uint32_t site_va, uint32_t target_va, '
          'uint32_t value);')

# The declaration must land AFTER the chunk's includes: it uses uint32_t, and a
# file-scope declaration above `#include "recomp_funcs.h"` is a syntax error
# (measured -- inserting at line 1 produced C2146/C2061/C2059 on every chunk).
EXTERN_BANNER = (
    '/* A4b observation \u2014 re-apply after regeneration.',
    ' * The A4b2 watch-ledger seam: a thin forwarder defined in src/diagnostics.c. */',
)

# The generated chunks are written as UTF-8 WITHOUT a byte-order mark.  A BOM
# prepended by a hand edit is a one-line diff on the first line of every chunk
# and makes the tree look modified when it is not; strip it on write.
MARKER_SUFFIX = ' /* A4b observation \u2014 re-apply after regeneration */'

# (chunk, guest site VA, the anchor line, the hook body, why)
#
# The anchor is the generated line the store compiles to; the hook is inserted
# immediately BEFORE it so the watch sees the pre-store value the original
# instruction is about to overwrite.  `context` is a second line that must
# appear within the following few lines, which is what disambiguates anchors
# that are not textually unique (`MEM32(ebx) = eax;` occurs 11 times).
HOOKS = (
    ('recomp_0000.c', 0x0006DACE,
     'MEM32(eax + 0x810) = ecx;',
     'jsrf_watch_store(0x0006DACEu, eax + 0x810, ecx);',
     'MEM32(eax + 0x814) = 1;',
     'A4b: the index-2064 slot store the A2h attribution watched'),
    ('recomp_0001.c', 0x0006DBBA,
     'MEM32(ecx + 0x810) = eax;',
     'jsrf_watch_store(0x0006DBBAu, ecx + 0x810, eax);',
     'eax = MEM32(ecx + 0x804);',
     'A4b: list-head store, caller 1'),
    ('recomp_0001.c', 0x0006DEF3,
     'MEM32(ecx + 0x810) = eax;',
     'jsrf_watch_store(0x0006DEF3u, ecx + 0x810, eax);',
     'MEM32(eax) = 0;',
     'A4b: list-head store, caller 2'),
    ('recomp_0005.c', 0x001A1751,
     'MEM32(edi + 0x810) = MEM32(edi + 0x810) & 0;',
     'jsrf_watch_store(0x001A1751u, edi + 0x810, 0);',
     'PUSH32(esp, 6);',
     'A4b: the zeroing store the GP DMA path performs'),
    ('recomp_0005.c', 0x001A18CE,
     'MEM32(ebx) = eax;',
     'jsrf_watch_store(0x001A18CEu, ebx, eax);',
     'loc_001A18D0: ;',
     'A4b: the post-`rep movsb` store into the descriptor'),
    ('recomp_0005.c', 0x001A1FA7,
     'MEM32(edi + 0x10) = ebp;',
     'jsrf_watch_store(0x001A1FA7u, edi + 0x10, ebp);',
     'MEM32(edi) = ebp;',
     'A4b: the descriptor field the watch correlates against'),
)

CONTEXT_WINDOW = 4
MARKER = 'A4b observation'
BOM = '\ufeff'


def read_chunk(path: Path) -> list[str]:
    """Read a generated chunk, dropping a BOM a hand edit may have left.

    Reading is newline-agnostic (`read_text` translates CRLF to LF); the line
    ending that matters is restored on write.
    """
    text = path.read_text(encoding='utf-8')
    if text.startswith(BOM):
        text = text[len(BOM):]
    return text.split('\n')


def write_chunk(path: Path, lines: list[str]) -> None:
    """Write a chunk back exactly as the generator leaves it: UTF-8, no BOM, CRLF.

    `core.autocrlf=true` and the generator both put CRLF on disk, and the
    provenance manifest and preservation baseline hash the **on-disk bytes**.
    Writing LF here silently changes the hash of every touched chunk (measured).
    """
    text = '\n'.join(lines)
    if text.startswith(BOM):
        text = text[len(BOM):]
    text = text.replace('\r\n', '\n').replace('\n', '\r\n')
    path.write_bytes(text.encode('utf-8'))


def chunk_path(name: str) -> Path:
    return ROOT / 'src' / 'recomp' / 'gen' / name


def load() -> dict[str, list[str]]:
    texts: dict[str, list[str]] = {}
    for name, *_ in HOOKS:
        path = chunk_path(name)
        if not path.is_file():
            raise FileNotFoundError(f'generated chunk is missing: {path}')
        texts[name] = read_chunk(path)
    return texts


def anchor_index(lines: list[str], anchor: str, context: str) -> tuple[int, str | None]:
    """Find the single line equal to `anchor` whose context follows.

    Returns (index, error).  Ambiguity is an error, never a first match.
    """
    candidates = [i for i, line in enumerate(lines) if line.strip() == anchor]
    if not candidates:
        return -1, f'anchor not found: {anchor!r}'
    if len(candidates) > 1:
        narrowed = []
        for i in candidates:
            window = lines[i + 1:i + 1 + CONTEXT_WINDOW]
            if any(line.strip() == context for line in window):
                narrowed.append(i)
        if len(narrowed) == 1:
            return narrowed[0], None
        if not narrowed:
            return -1, (f'anchor {anchor!r} occurs {len(candidates)} times and none '
                        f'is followed by {context!r}')
        return -1, (f'anchor {anchor!r} occurs {len(candidates)} times and '
                    f'{len(narrowed)} match the context {context!r}')
    return candidates[0], None


def already_hooked(lines: list[str], hook: str) -> bool:
    """True when the call is present, with or without its trailing comment."""
    return any(line.strip().startswith(hook) for line in lines)


def ensure_extern(lines: list[str]) -> bool:
    """Insert the forwarder declaration after the chunk's includes.

    Anchored on the last `#include` line, never on a fixed line number, and
    never above the first include (see EXTERN_BANNER).
    """
    if any(line.strip() == EXTERN for line in lines):
        return False
    includes = [i for i, line in enumerate(lines)
                if line.lstrip().startswith('#include')]
    if not includes:
        raise ValueError('chunk has no #include line to anchor the declaration after')
    at = includes[-1] + 1
    lines[at:at] = ['', *EXTERN_BANNER, EXTERN]
    return True


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--check', action='store_true',
                       help='report hook state; change nothing')
    group.add_argument('--apply', action='store_true',
                       help='insert any missing hook')
    args = parser.parse_args(argv)

    try:
        texts = load()
    except FileNotFoundError as error:
        print(f'cannot run: {error}', file=sys.stderr)
        return 2

    problems: list[str] = []
    applied = 0
    present = 0

    for name, site, anchor, hook, context, why in HOOKS:
        lines = texts[name]
        if already_hooked(lines, hook):
            present += 1
            print(f'  present  {name}  0x{site:08X}  {why}')
            continue
        index, error = anchor_index(lines, anchor, context)
        if error:
            problems.append(f'{name} 0x{site:08X}: {error}')
            print(f'  BROKEN   {name}  0x{site:08X}  {error}')
            continue
        if args.apply:
            lines.insert(index, f'    {hook}{MARKER_SUFFIX}')
            texts[name] = lines
            applied += 1
            print(f'  applied  {name}  0x{site:08X}  line {index + 1}  {why}')
        else:
            print(f'  missing  {name}  0x{site:08X}  anchor at line {index + 1}  {why}')

    if problems:
        print(f'{len(problems)} anchor problem(s); nothing written for those',
              file=sys.stderr)
        return 1

    if args.apply and applied:
        for name, lines in texts.items():
            ensure_extern(lines)
            write_chunk(chunk_path(name), lines)
    elif args.apply:
        # The hooks are present but the declaration may have been lost (it is
        # inserted with them, and a hand edit that removes it must not leave the
        # calls implicitly declared). Repair it in place.
        repaired = []
        for name, lines in texts.items():
            if ensure_extern(lines):
                write_chunk(chunk_path(name), lines)
                repaired.append(name)
        if repaired:
            print(f'declaration restored in: {", ".join(repaired)}')

    if args.apply:
        print(f'{present} already present, {applied} applied')
    else:
        total = len(HOOKS)
        print(f'{present}/{total} present'
              + ('' if present == total else f'; {total - present} missing'))
        if present != total:
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
