"""Run ctest un-elevated (limited-rights scheduled task) from an elevated session.

The Vulkan loader ignores `VK_DRIVER_FILES` in an elevated process, so the software-ICD tests
only run un-elevated. This is `ctest --test-dir <build> -C Release <args>` through
`scripts/run-limited.py`; every argument it does not recognise goes to ctest.

    python scripts/ctest-limited.py [--build-dir DIR] [--task-timeout SECONDS] [ctest args...]

`--build-dir` defaults to the game's `build`. A `-R` regex containing `|` is refused: the
alternation has broken limited runs before (a pipe in a task's command line is read by cmd.exe
as a pipe), and a run that silently selects the wrong tests proves nothing. Select with several
invocations, or one anchored prefix such as `-R ^jsrf_texgen_stream_vk_`.
"""
import argparse
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGEX_FLAGS = ('-R', '--tests-regex')


def alternation_in_regex(ctest_args):
    """The offending `-R` value, or None. Handles `-R x`, `-Rx`, `--tests-regex x` and `=x`."""
    for index, arg in enumerate(ctest_args):
        value = None
        if arg in REGEX_FLAGS:
            value = ctest_args[index + 1] if index + 1 < len(ctest_args) else ''
        elif arg.startswith('--tests-regex='):
            value = arg.split('=', 1)[1]
        elif arg.startswith('-R') and not arg.startswith('-R-'):
            value = arg[2:]
        if value and '|' in value:
            return value
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--build-dir', default=str(ROOT / 'build'))
    parser.add_argument('--task-timeout', type=int, default=1800,
                        help='seconds to wait for ctest (default 1800)')
    args, ctest_args = parser.parse_known_args(argv)
    bad = alternation_in_regex(ctest_args)
    if bad is not None:
        print(f'ctest-limited: refusing -R {bad!r}: a regex containing "|" has broken limited '
              f'runs before. Run one -R per invocation, or use an anchored prefix.',
              file=sys.stderr)
        return 2
    spec = importlib.util.spec_from_file_location('run_limited', ROOT / 'scripts' / 'run-limited.py')
    run_limited = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(run_limited)
    return run_limited.main(['--timeout', str(args.task_timeout), '--cwd', str(ROOT), '--',
                             'ctest', '--test-dir', args.build_dir, '-C', 'Release',
                             *ctest_args])


if __name__ == '__main__':
    sys.exit(main())
