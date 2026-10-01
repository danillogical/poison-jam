"""`scripts/patch-generated.py`: plan T18, exact-once patches to generated code.

A regeneration rewrites `src/recomp/gen/` from the XBE, so a hand edit made there
is lost on the next pass unless something re-applies it. This script is that
something: `config/generated-patches.json` lists each edit as exact text, and the
script applies it after every regeneration (`just regen` runs it).

The design follows Mercenaries-Recompiled's `Patch-Generated.py` (MIT), rewritten
as data rather than code:

  * **Exact once.** A patch's `before` text must occur exactly once in its scope
    and its `after` text not at all; then it is applied. If `after` occurs once
    and `before` not at all, it is already applied and left alone, so a second run
    changes nothing. Anything else -- the site missing, ambiguous, or both texts
    present -- fails the whole run. There is no `--allow-missing`: a patch whose
    site moved is a patch that silently stopped applying.
  * **Scoped.** `function` names a generated function (`0x00238C00` scopes the
    patch to the body of `sub_00238C00` in whichever chunk defines it); `file`
    names a file under the generated directory. One of the two, never neither, so
    a short `before` cannot match somewhere unintended.
  * **Recorded.** Each patch carries a `ledger` ID that must exist in
    `docs/jsrf-compatibility-ledger.md`, because a patched path is a departure from
    the original code and the ledger is where departures are listed.
  * **All or nothing.** Every patch is checked before any file is written.

`--check` applies nothing and exits 1 unless every patch is already applied.

Exit 0 success, 1 a patch failed or (with `--check`) is not applied, 2 the
manifest could not be read.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'config' / 'generated-patches.json'
GEN_DIR = ROOT / 'src' / 'recomp' / 'gen'
LEDGER = ROOT / 'docs' / 'jsrf-compatibility-ledger.md'

PATCH_FIELDS = {'id', 'ledger', 'reason', 'before', 'after', 'function', 'file'}
LEDGER_ROW = re.compile(r'^\|\s*(L\d+)\s*\|', re.MULTILINE)


class PatchError(ValueError):
    """The manifest or a patch is malformed."""


def ledger_ids(path: Path) -> set[str]:
    return set(LEDGER_ROW.findall(path.read_text(encoding='utf-8')))


def load_manifest(path: Path, known_ledger: set[str]) -> list[dict]:
    data = json.loads(path.read_text(encoding='utf-8'))
    if data.get('schema') != 1 or not isinstance(data.get('patches'), list):
        raise PatchError(f'{path}: expected {{"schema": 1, "patches": [...]}}')
    seen: set[str] = set()
    for patch in data['patches']:
        pid = patch.get('id')
        extra = set(patch) - PATCH_FIELDS
        if not pid or pid in seen:
            raise PatchError(f'patch id {pid!r} is missing or repeated')
        seen.add(pid)
        if extra:
            raise PatchError(f'{pid}: unknown field(s) {sorted(extra)}')
        if patch.get('ledger') not in known_ledger:
            raise PatchError(f'{pid}: ledger ID {patch.get("ledger")!r} is not an entry in '
                             f'{LEDGER.name}; add the entry first')
        if not patch.get('reason'):
            raise PatchError(f'{pid}: a patch needs a reason')
        if ('function' in patch) == ('file' in patch):
            raise PatchError(f'{pid}: give exactly one of "function" or "file"')
        # A deletion is written as a replacement (a comment will do): an empty
        # "after" cannot tell "already applied" from "site missing".
        if not patch.get('before') or not patch.get('after') or patch['before'] == patch['after']:
            raise PatchError(f'{pid}: "before" and "after" must be non-empty and differ')
    return data['patches']


def function_span(text: str, address: int) -> tuple[int, int] | None:
    """[start, end) of `void sub_XXXXXXXX(void)` through its closing brace."""
    head = re.search(rf'^void sub_{address:08X}\(void\)\s*\{{', text, re.MULTILINE)
    if not head:
        return None
    close = re.compile(r'^\}', re.MULTILINE).search(text, head.end())
    return (head.start(), close.end()) if close else None


def locate(patch: dict, gen_dir: Path, files: dict[Path, str]) -> tuple[Path, int, int]:
    """The file and span a patch applies to."""
    if 'file' in patch:
        path = (gen_dir / patch['file']).resolve()
        if gen_dir.resolve() not in path.parents:
            raise PatchError(f'{patch["id"]}: {patch["file"]!r} is outside {gen_dir}')
        if path not in files:
            if not path.is_file():
                raise PatchError(f'{patch["id"]}: {patch["file"]!r} does not exist')
            files[path] = path.read_text(encoding='utf-8')
        return path, 0, len(files[path])
    address = int(str(patch['function']), 16)
    hits = []
    for path in sorted(gen_dir.glob('*.c')):
        if path not in files:
            files[path] = path.read_text(encoding='utf-8')
        span = function_span(files[path], address)
        if span:
            hits.append((path, *span))
    if len(hits) != 1:
        raise PatchError(f'{patch["id"]}: sub_{address:08X} is defined {len(hits)} times '
                         f'under {gen_dir}, expected once')
    return hits[0]


def run(manifest: Path, gen_dir: Path, ledger: Path, check: bool) -> int:
    try:
        patches = load_manifest(manifest, ledger_ids(ledger))
    except (OSError, ValueError) as exc:
        print(f'patch-generated: {exc}', file=sys.stderr)
        return 2

    files: dict[Path, str] = {}
    failures: list[str] = []
    applied = already = 0
    for patch in patches:
        try:
            path, start, end = locate(patch, gen_dir, files)
        except PatchError as exc:
            failures.append(str(exc))
            continue
        text = files[path]
        scope = text[start:end]
        n_before, n_after = scope.count(patch['before']), scope.count(patch['after'])
        if n_before == 0 and n_after == 1:
            already += 1
            continue
        if n_before == 1 and n_after == 0 and not check:
            files[path] = text[:start] + scope.replace(patch['before'], patch['after']) + text[end:]
            applied += 1
            continue
        state = 'not applied' if n_before == 1 and n_after == 0 else \
            f'"before" found {n_before} time(s), "after" {n_after} time(s)'
        failures.append(f'{patch["id"]} ({patch["ledger"]}): {state} in {path.name}')

    if failures:
        for failure in failures:
            print(f'  FAILED {failure}')
        print(f'patch-generated: {len(failures)} of {len(patches)} patch(es) failed; '
              f'{"nothing checked further" if check else "no file was written"}')
        return 1
    if not check:
        for path, text in files.items():
            if path.read_text(encoding='utf-8') != text:
                path.write_text(text, encoding='utf-8', newline='\n')
    print(f'patch-generated: {len(patches)} patch(es); {applied} applied, {already} already applied')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--check', action='store_true',
                        help='apply nothing; fail unless every patch is already applied')
    parser.add_argument('--manifest', type=Path, default=MANIFEST)
    parser.add_argument('--gen-dir', type=Path, default=GEN_DIR)
    parser.add_argument('--ledger', type=Path, default=LEDGER)
    args = parser.parse_args()
    return run(args.manifest, args.gen_dir, args.ledger, args.check)


if __name__ == '__main__':
    raise SystemExit(main())
