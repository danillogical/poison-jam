"""`scripts/analyze-toolkit.py`: clang-cl and MSVC `/analyze` configurations (T5).

Plan T5 asks for both configurations of the **toolkit**, with baseline warning
counts recorded, and specifically that "the known `%lld`-with-`int` class and
implicit declarations are reported by at least one of them".

**Why two analysers.** They are not redundant: MSVC's `/analyze` is a
path-sensitive dataflow analyser that finds real bugs but is slow and
Windows-only, while clang's `-Weverything`-style diagnostics are fast, portable and
catch a different class (format-string mismatches, shadowing, sign conversion).
A finding that only one reports is still a finding; the plan's acceptance is
satisfied by *at least one*, which is the honest bar when the two see different
things.

**Why the baseline is recorded, not asserted.** The toolkit is a fork with a large
existing warning population. A check that demanded zero warnings would be red from
the first run and would be ignored. What is useful is the **set**, recorded once,
so a later change that adds a warning is visible as a difference -- and so the two
named classes can be pointed at directly.

**Read-only.** Both configurations build into a scratch directory outside the
repository, and neither touches the toolkit's own `build/`.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLKIT = ROOT.parent / 'xboxrecomp'
SCRATCH = ROOT / 'logs' / 'analyze'
CLANG_CL = shutil.which('clang-cl') or r'C:\Program Files\LLVM\bin\clang-cl.exe'

# `file(line[,col]): warning Cnnnn: text` and clang's
# `file(line,col): warning: text [-Wflag]`.
MSVC_DIAG = re.compile(
    r'^(?P<file>[^\s(][^(]*)\((?P<line>\d+)(?:,\d+)?\)\s*:\s*'
    r'(?P<level>warning|error)\s*(?P<code>[A-Z]+\d+)?\s*:\s*(?P<text>.*)$')
CLANG_DIAG = re.compile(
    r'^(?P<file>[^\s:][^:]*):(?P<line>\d+):(?P<col>\d+):\s*'
    r'(?P<level>warning|error)\s*:\s*(?P<text>.*?)(?:\s*\[(?P<flag>-[^\]]+)\])?$')

# The two classes plan T5 names.  Each is a pattern over the diagnostic text, so
# the report can say whether the class was actually seen rather than whether some
# count was non-zero.
NAMED_CLASSES = {
    'printf_int64': (
        re.compile(r'(?i)%lld|%I64d|long long|__int64'),
        'a 64-bit format specifier used with a 32-bit argument (`%lld`-with-`int`)'),
    'implicit_declaration': (
        re.compile(r'(?i)implicit(ly)? declar|C4013|implicit-function-declaration'),
        'a call to a function with no prototype in scope'),
}


def run(argv: list[str], cwd: Path, log: Path, env: dict | None = None) -> int:
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w', encoding='utf-8', errors='replace') as handle:
        completed = subprocess.run(argv, cwd=str(cwd), stdout=handle,
                                   stderr=subprocess.STDOUT, env=env)
    return completed.returncode


def parse_diagnostics(text: str) -> list[dict]:
    """Every diagnostic, in either the MSVC or the clang-native shape.

    **`clang-cl` emits MSVC-style diagnostics**, not clang's `file:line:col:`
    form: measured, `.../xbox_winnt.h(73,10): warning: non-portable path ...`.
    Parsing only the native clang shape reported **0 diagnostics from a run that
    emitted 76**, which reads exactly like a clean tree. Both patterns are tried
    so the count cannot silently collapse to zero on a compiler that changes
    which form it uses.
    """
    findings = []
    for line in text.splitlines():
        stripped = line.strip()
        for pattern in (MSVC_DIAG, CLANG_DIAG):
            match = pattern.match(stripped)
            if not match:
                continue
            groups = match.groupdict()
            findings.append({
                'file': groups.get('file'),
                'line': int(groups['line']) if groups.get('line') else None,
                'level': groups.get('level'),
                'code': groups.get('code') or groups.get('flag'),
                'text': (groups.get('text') or '').strip()[:200],
            })
            break
    return findings


def classify(findings: list[dict]) -> dict[str, int]:
    counts: dict[str, int] = {}
    for name, (pattern, _why) in NAMED_CLASSES.items():
        counts[name] = sum(1 for f in findings if pattern.search(f['text'] or ''))
    return counts


def summarise(label: str, findings: list[dict], exit_code: int,
              log: Path) -> dict:
    by_code: dict[str, int] = {}
    for finding in findings:
        key = finding['code'] or finding['level'] or 'unknown'
        by_code[key] = by_code.get(key, 0) + 1
    return {
        'configuration': label,
        'exit_code': exit_code,
        'log': str(log),
        'total_diagnostics': len(findings),
        'by_code': dict(sorted(by_code.items(), key=lambda kv: -kv[1])),
        'named_classes': classify(findings),
        'findings': findings[:200],
        'truncated': len(findings) > 200,
    }


def msvc_environment() -> dict | None:
    """The MSVC developer environment, captured from `vcvars64.bat`.

    clang-cl is an MSVC-compatible driver: it needs the SDK and CRT include
    directories that `vcvars64.bat` sets. Measured: without them the run dies at
    `fatal error: 'qemu/osdep.h' file not found`, which is an environment failure
    wearing the costume of a code failure.
    """
    candidates = [
        Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'),
        Path(r'C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvars64.bat'),
        Path(r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'),
    ]
    script = next((p for p in candidates if p.is_file()), None)
    if script is None:
        found = list(Path(r'C:\Program Files\Microsoft Visual Studio').glob(
            '**/VC/Auxiliary/Build/vcvars64.bat'))
        script = found[0] if found else None
    if script is None:
        return None
    completed = subprocess.run(
        ['cmd', '/c', f'call "{script}" >nul 2>&1 && set'],
        capture_output=True, text=True)
    env: dict[str, str] = {}
    for line in completed.stdout.splitlines():
        key, separator, value = line.partition('=')
        if separator:
            env[key] = value
    return env or None


def compile_commands(build: Path) -> list[dict] | None:
    """CMake's own per-file compile commands, the authority on include paths.

    Hand-assembling `/I` flags was measured to be wrong: the toolkit's `gp_ep.h`
    includes `qemu/osdep.h`, which the build supplies from a directory that no
    hand-written flag list names. Using the project's own command lines is the
    only way this configuration sees what the real build sees.
    """
    path = build / 'compile_commands.json'
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        return None


def entry_arguments(entry: dict) -> list[str]:
    """A compile_commands entry's arguments, from either form.

    CMake can emit `arguments` (a list) or `command` (one string). Measured: the
    Ninja generator on this host emits **`command`**, and code that only read
    `arguments` fell through to a bare source filename -- dropping every `/I` flag
    and producing `fatal error: 'platform/xbox_winnt.h' file not found` on 63 of
    75 translation units. That is an environment failure wearing the costume of 63
    missing headers.

    Splitting the string form is safe here because CMake quotes any argument that
    contains a space, so `shlex` with POSIX rules reconstructs the original list.
    """
    arguments = entry.get('arguments')
    if arguments:
        return [str(a) for a in arguments]
    command = entry.get('command')
    if not command:
        return []
    try:
        import shlex
        return shlex.split(command, posix=True)
    except ValueError:
        return command.split()


def clang_configuration(record: dict) -> dict:
    """clang-cl over the toolkit's C sources, using CMake's own command lines.

    **The Ninja generator is required, not a preference.** Measured: the Visual
    Studio generator accepts `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and silently
    produces no `compile_commands.json` -- MSBuild project files carry the flags
    instead. Ninja writes the file, and the file is the authority on the include
    paths: the toolkit's `gp_ep.h` includes `qemu/osdep.h` from a directory that no
    hand-assembled `/I` list names.
    """
    build = SCRATCH / 'clang'
    log = SCRATCH / 'clang-cl.log'
    env = msvc_environment() or dict(os.environ)
    configure = ['cmake', '-S', str(TOOLKIT), '-B', str(build), '-G', 'Ninja',
                 '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    code = run(configure, TOOLKIT, SCRATCH / 'clang-configure.log', env)
    if code != 0:
        return {'configuration': 'clang-cl', 'exit_code': code,
                'log': str(SCRATCH / 'clang-configure.log'),
                'total_diagnostics': 0, 'by_code': {},
                'named_classes': {k: 0 for k in NAMED_CLASSES},
                'findings': [], 'truncated': False,
                'error': ('CMake configure with the Ninja generator failed; see '
                          'the configure log')}

    commands = compile_commands(build)
    if not commands:
        return {'configuration': 'clang-cl', 'exit_code': 2, 'log': str(log),
                'total_diagnostics': 0, 'by_code': {},
                'named_classes': {k: 0 for k in NAMED_CLASSES},
                'findings': [], 'truncated': False,
                'error': 'CMake produced no compile_commands.json'}

    env = msvc_environment() or dict(os.environ)
    texts = []
    worst = 0
    for index, entry in enumerate(commands):
        # Re-drive the project's own command with clang-cl instead of cl.exe.
        # `/Zs` is syntax-only: this is an analysis pass, not a build, so it must
        # not write objects into a tree anyone links.
        #
        # The filter drops ONLY what would defeat a syntax pass: the compiler
        # binary, `/c`, and any output path. Include directories (`/I...`),
        # defines (`/D...`) and language flags all stay, because dropping them is
        # what produced `fatal error: 'platform/xbox_winnt.h' file not found` --
        # an environment failure that reads like a missing header.
        arguments = [str(CLANG_CL), '/nologo', '/Zs', '/W4']
        original = entry_arguments(entry)
        if not original:
            return {'configuration': 'clang-cl', 'exit_code': 2, 'log': str(log),
                    'total_diagnostics': 0, 'by_code': {},
                    'named_classes': {k: 0 for k in NAMED_CLASSES},
                    'findings': [], 'truncated': False,
                    'error': f'compile_commands entry {index} carries neither '
                             f'"arguments" nor "command"'}
        for item in original[1:]:
            lowered = item.lower()
            if lowered in ('/c', '-c', '/nologo'):
                continue
            if lowered.startswith(('/fo', '-o', '/fe', '/fd', '/fa')):
                continue
            if lowered.endswith('.c') or lowered.endswith('.obj'):
                continue
            arguments.append(item)
        arguments.append(entry['file'])
        per_file = SCRATCH / f'clang-{index:04d}.log'
        worst = max(worst, run(arguments, Path(entry['directory']), per_file, env))
        texts.append(per_file.read_text(encoding='utf-8', errors='replace'))
    log.write_text('\n'.join(texts), encoding='utf-8')

    result = summarise('clang-cl /Zs /W4 (CMake command lines)',
                       parse_diagnostics('\n'.join(texts)),
                       worst, log)
    result['compilation_units'] = len(commands)
    result['environment_limited'] = bool(
        re.search(r'fatal error:.*file not found', '\n'.join(texts)))
    result['environment_note'] = (
        'clang-cl still could not see a header the build supplies; the diagnostics '
        'below are real but the configuration is not a complete compile.'
        if result['environment_limited'] else None)
    return result


def msvc_configuration(record: dict) -> dict:
    """MSVC `/analyze` through CMake, into a scratch build tree.

    **`--clean-first` is required, not tidiness.** Measured: the first run reported
    355 diagnostics and the second reported **0**, because MSBuild skipped every
    up-to-date translation unit and `/analyze` only runs on files it actually
    compiles. A count that collapses to zero because nothing recompiled is
    indistinguishable from a clean tree -- the exact failure this tool exists to
    avoid -- so the analysis build always starts clean.
    """
    build = SCRATCH / 'msvc'
    log = SCRATCH / 'msvc-analyze.log'
    configure = ['cmake', '-S', str(TOOLKIT), '-B', str(build),
                 '-DCMAKE_C_FLAGS=/analyze']
    code = run(configure, TOOLKIT, SCRATCH / 'msvc-configure.log')
    if code != 0:
        return {'configuration': 'MSVC /analyze', 'exit_code': code,
                'log': str(SCRATCH / 'msvc-configure.log'),
                'total_diagnostics': 0, 'by_code': {},
                'named_classes': {k: 0 for k in NAMED_CLASSES},
                'findings': [], 'truncated': False,
                'error': 'CMake configure failed; see the configure log'}
    code = run(['cmake', '--build', str(build), '--config', 'Release',
                '--parallel', '4', '--clean-first'], TOOLKIT, log)
    text = log.read_text(encoding='utf-8', errors='replace')
    result = summarise('MSVC /analyze', parse_diagnostics(text), code, log)
    result['forced_rebuild'] = True
    return result


def describe(path: Path) -> str:
    """A repo-relative path when possible, else the absolute one.

    `--out` accepts any path, and `Path.relative_to` raises for one outside the
    repository -- so a report path in `logs/` given as a relative argument is
    resolved before printing rather than crashing the summary.
    """
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def seeded_control() -> dict:
    """Prove the named-class checks can actually FIRE.

    A class reported as "NOT SEEN" over the toolkit is only meaningful if the
    detector would have seen it. Without this, `implicit_declaration: 0` is
    indistinguishable from a regex that never matches anything -- and the toolkit
    genuinely cannot produce one, because its `CMakeLists.txt` sets `/we4013`
    (implicit declaration is an **error**, not a warning), so the absence is real
    and has a reason.

    This compiles a tiny translation unit carrying both named classes, with the
    same detector, and requires both to be found.
    """
    source = SCRATCH / 'seeded_control.c'
    source.write_text(
        '#include <stdio.h>\n'
        'int main(void) {\n'
        '    long long big = 1;\n'
        '    int small = 2;\n'
        '    printf("%lld\\n", small);   /* 64-bit specifier, 32-bit argument */\n'
        '    return (int)(undeclared_helper(big));  /* no prototype in scope */\n'
        '}\n', encoding='utf-8')
    log = SCRATCH / 'seeded-control.log'
    env = msvc_environment() or dict(os.environ)
    # Deliberately WITHOUT /we4013, so the implicit-declaration class is reported
    # as a warning rather than stopping the compile. The point is to prove the
    # DETECTOR works, not to reproduce the toolkit's flags.
    #
    # `-Wno-error` is the clang spelling; measured, clang-cl rejects `/Wno-error`
    # with `no such file or directory` and then emits nothing at all -- which the
    # control correctly reported as DETECTOR FAILED rather than as an absence.
    argv = [str(CLANG_CL), '/nologo', '/Zs', '/W4', '-Wno-error', str(source)]
    code = run(argv, SCRATCH, log, env)
    text = log.read_text(encoding='utf-8', errors='replace')
    findings = parse_diagnostics(text)
    counts = classify(findings)
    return {
        'exit_code': code,
        'log': str(log),
        'total_diagnostics': len(findings),
        'named_classes': counts,
        'fired': {name: counts[name] > 0 for name in NAMED_CLASSES},
        'note': ('A seeded translation unit carrying both named classes, compiled '
                 'without /we4013. A class the detector reports here but not over '
                 'the toolkit is an absence WITH a reason; a class it reports '
                 'nowhere is a broken detector.'),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', default='baseline',
                        choices=('baseline', 'clang', 'msvc', 'both'))
    parser.add_argument('--out', type=Path,
                        default=ROOT / 'docs' / 'reviews' / 't5-analyzer-baseline.json')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()

    if not TOOLKIT.is_dir():
        print(f'no toolkit at {TOOLKIT}', file=sys.stderr)
        return 2
    SCRATCH.mkdir(parents=True, exist_ok=True)

    record: dict = {
        'tool': 'jsrf-analyze-toolkit/1',
        'generated_utc': datetime.now(timezone.utc).isoformat(),
        'toolkit': str(TOOLKIT),
        'note': ('Read-only: both configurations build into logs/analyze/, outside '
                 'the repository. The baseline is recorded as a SET of '
                 'diagnostics so a later change is visible as a difference; the '
                 'toolkit has a large pre-existing warning population and a '
                 'zero-warning check would be ignored.'),
        'named_class_reasons': {name: why for name, (_p, why) in NAMED_CLASSES.items()},
    }

    configurations = []
    if args.target in ('baseline', 'clang', 'both'):
        configurations.append(clang_configuration(record))
    if args.target in ('baseline', 'msvc', 'both'):
        configurations.append(msvc_configuration(record))
    record['configurations'] = configurations
    record['seeded_control'] = seeded_control()

    # T5's acceptance: the two named classes are reported by AT LEAST ONE
    # configuration.  A class seen by neither is a finding about the check, not a
    # pass, so it is reported rather than glossed.
    for name in NAMED_CLASSES:
        seen = {c['configuration']: c['named_classes'].get(name, 0)
                for c in configurations}
        record.setdefault('named_class_totals', {})[name] = {
            'per_configuration': seen,
            'reported_by_any': any(count > 0 for count in seen.values()),
        }

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True),
                            encoding='utf-8')

    if args.json:
        print(json.dumps(record, indent=2, sort_keys=True))
        return 0

    print(f'analyzer baseline -> {describe(args.out)}')
    for configuration in configurations:
        print(f"  {configuration['configuration']}: "
              f"{configuration['total_diagnostics']} diagnostic(s), "
              f"exit {configuration['exit_code']}")
        if configuration.get('error'):
            print(f"    ERROR: {configuration['error']}")
        for code, count in list(configuration['by_code'].items())[:8]:
            print(f'      {code:<12} {count}')
    print('  plan T5 named classes (reported by at least one configuration):')
    for name, entry in record.get('named_class_totals', {}).items():
        mark = 'PASS' if entry['reported_by_any'] else 'NOT SEEN'
        print(f"    {mark:<9} {name}: {entry['per_configuration']}")
        print(f"              {record['named_class_reasons'][name]}")
        if not entry['reported_by_any']:
            control = record.get('seeded_control', {})
            fired = control.get('fired', {}).get(name)
            if fired:
                print(f"              detector WORKS (fires on the seeded control); "
                      f"the absence over the toolkit is real and has a reason")
            elif fired is False:
                print(f"              DETECTOR FAILED: it does not fire even on a "
                      f"seeded control, so this absence is not evidence")
    return 0


if __name__ == '__main__':
    sys.exit(main())
