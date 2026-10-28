"""The ONE place that decides which xboxrecomp toolkit checkout this repository binds to.

Three sources can name the toolkit, in this order of precedence:

1. the game build tree's cached `XBOXRECOMP_DIR` (`build/CMakeCache.txt`): what CMake
   actually links and includes, so it is the truth about the executable;
2. the `XBOXRECOMP_DIR` environment variable;
3. the sibling checkout `../xboxrecomp`.

When (1) and (2) both exist and name different directories (after `realpath` and
`normcase`), the answer is a REFUSAL, not a preference: the generator and the build would
each be internally consistent while describing two different trees, which no later check
can see. `scripts/gen-shaders.py` applied this rule first; every other script now shares it.

    from toolkit_path import toolkit_path
    TOOLKIT = toolkit_path()

A script that must accept an explicit toolkit (a `--toolkit` option, a private mutant copy)
keeps that path explicit and does not call this.
"""
import os
import re
from pathlib import Path

GAME_ROOT = Path(__file__).resolve().parents[1]
GAME_BUILD_DIR = GAME_ROOT / 'build'
CACHE_NAME = 'CMakeCache.txt'

# One CMakeCache entry, tolerant of the `//`-comment lines CMake writes above each entry,
# of surrounding whitespace and of the entry's TYPE (`:PATH`, `:INTERNAL`, `:UNINITIALIZED`).
# The name is anchored and must be followed by the type-or-`=` separator, so a different
# variable that merely starts the same way (`XBOXRECOMP_DIRECTORY`) cannot match.
_CACHE_ENTRY = r'^\s*{name}\s*(?::\s*[A-Za-z_][A-Za-z0-9_]*\s*)?=\s*(.*?)\s*$'
CACHE_RE = re.compile(_CACHE_ENTRY.format(name='XBOXRECOMP_DIR'), re.MULTILINE)
# The owned host-tool tree's own configure root (see `scripts/gen-shaders.py`).
HOME_DIR_RE = re.compile(_CACHE_ENTRY.format(name='CMAKE_HOME_DIRECTORY'), re.MULTILINE)
VK_SW_ICD_RE = re.compile(_CACHE_ENTRY.format(name='XBR_VK_SW_ICD'), re.MULTILINE)


class ToolkitPathError(RuntimeError):
    """The toolkit cannot be bound unambiguously; the message names both paths."""


def cache_entry(path, entry_re):
    """One CMakeCache entry's value, or None when the cache or the entry is absent or empty."""
    path = Path(path)
    if not path.is_file():
        return None
    match = entry_re.search(path.read_text(encoding='utf-8', errors='replace'))
    if not match or not match.group(1).strip():
        return None
    return match.group(1).strip()


def software_icd(build_dir=None):
    """The software Vulkan ICD manifest: `$XBR_VK_SW_ICD`, else the game build cache's entry, else None."""
    env = os.environ.get('XBR_VK_SW_ICD')
    if env:
        return env
    return cache_entry(Path(build_dir or GAME_BUILD_DIR) / CACHE_NAME, VK_SW_ICD_RE)


def _same(a, b):
    return os.path.normcase(os.path.realpath(str(a))) == os.path.normcase(os.path.realpath(str(b)))


def toolkit_path(build_dir=None, game_root=None, with_label=False):
    """The toolkit root this repository binds to; `(path, label)` when `with_label`.

    `build_dir` and `game_root` default to this repository and exist so a test can bind a
    fixture tree. The returned path is `realpath`-normalised but NOT checked for being a
    complete checkout: callers that need that (the shader generator) check it themselves.
    """
    game_root = Path(game_root) if game_root else GAME_ROOT
    build_dir = Path(build_dir) if build_dir else game_root / 'build'
    cached_raw = cache_entry(build_dir / CACHE_NAME, CACHE_RE)
    cached = None
    if cached_raw is not None:
        # CMake stores an absolute path in a PATH entry; a relative one can only come from a
        # hand-edited cache, and configure ran in the game root.
        cached = Path(cached_raw)
        if not cached.is_absolute():
            cached = game_root / cached
    env_raw = os.environ.get('XBOXRECOMP_DIR')
    if cached is not None and env_raw and not _same(cached, env_raw):
        raise ToolkitPathError(
            f'the build tree caches XBOXRECOMP_DIR={cached_raw} but the environment sets '
            f'XBOXRECOMP_DIR={env_raw}: CMake would link one toolkit while the shader '
            f'generator and the corpus guard read the other, so the corpus could not be '
            f'proven to describe the SPIR-V that gets linked. Configure with '
            f'-DXBOXRECOMP_DIR=<the toolkit you want>, or unset XBOXRECOMP_DIR in the '
            f'environment, so both name the same tree; if the corpus came from the other '
            f'tree, re-run `just shaders` afterwards.')
    if cached is not None:
        root, label = cached, "the build tree's cached XBOXRECOMP_DIR"
    elif env_raw:
        root, label = Path(env_raw), 'XBOXRECOMP_DIR'
    else:
        root, label = game_root.parent / 'xboxrecomp', 'the sibling checkout'
    root = Path(os.path.realpath(str(root)))
    return (root, label) if with_label else root


if __name__ == '__main__':
    # `python scripts/toolkit_path.py` prints the binding; the justfile reads it.
    import sys
    try:
        print(toolkit_path())
    except ToolkitPathError as error:
        sys.exit(f'toolkit_path: {error}')
