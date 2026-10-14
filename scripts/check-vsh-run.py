"""Check a JSRF run's vertex-program counters (plan step V5's e2e gate).

V5's gate is "0 vertex-program misses WITH positive retained cumulative GPU
vertex-program hit/draw activity". Zero misses with zero activity proves nothing:
a corpus that is never consulted, or a draw path that never runs, produces the
same zeros as a perfect run. This reads two INDEPENDENT counter sources so a
single missing line cannot fake a pass:

  1. the executor's own cumulative summary, which predates V5 and does not
     depend on the back end's report line:
         [GPU] vertex programs: N batches, V vertices; B triangles dropped behind the eye
         [GPU]   of those, G batches (T triangles) drawn by the back end from the
                 program, F declined to the CPU
  2. the Vulkan back end's rate-limited [GPUVK] line, which carries the
     identity-level detail the executor cannot know:
         vsh_hits= vsh_misses= vsh_identity_fail= vsh_declines= vsh_tris= vsh_const_uploads=

Source 1 is the primary evidence (G > 0 and T > 0 is a draw that really went
through a corpus shader); source 2 must agree with it and add `vsh_misses=0`.
Both are cumulative, and source 2 is printed on a fixed cadence, so the LAST line
of each in a run that was killed at its deadline is the run's totals.

Exit 0 only when the gate holds. Prints every number it read, with its line
number, so the claim can be re-checked by hand.

Usage: python -X utf8 scripts/check-vsh-run.py <run-dir> [--expect-misses 0]
"""
import argparse
import re
import sys
from pathlib import Path

# The executor's cumulative summary. One line per report; the last is the total.
RE_PROGRAM = re.compile(
    r'\[GPU\] vertex programs: (\d+) batches, (\d+) vertices; (\d+) triangles dropped')
RE_PROGRAM_GPU = re.compile(
    r'\[GPU\]\s+of those, (\d+) batches \((\d+) triangles\) drawn by the back end from the'
    r' program, (\d+) declined to the CPU')
# The back end's rate-limited line, keyed by name so field order does not matter.
RE_STAT = re.compile(r'(\w+)=(-?\d+(?:\.\d+)?)')
VSH_KEYS = ('vsh_hits', 'vsh_misses', 'vsh_identity_fail', 'vsh_declines', 'vsh_tris',
            'vsh_const_uploads')
# A run that hit an unresolved or invalid guest call is not evidence of anything.
RE_FATAL = re.compile(r'0xE0424943|\[FATAL\]|unresolved (?:indirect )?call', re.I)


def last_match(pattern, lines):
    hit = None
    for i, line in enumerate(lines):
        m = pattern.search(line)
        if m:
            hit = (i + 1, m)
    return hit


def gpuvk_lines(lines):
    """Every [GPUVK] stats line, as (line number, {name: value})."""
    out = []
    for i, line in enumerate(lines):
        if '[GPUVK]' in line and 'vsh_hits=' in line:
            out.append((i + 1, dict(RE_STAT.findall(line))))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('run', type=Path, help='logs/runs/<label> directory')
    ap.add_argument('--expect-misses', type=int, default=0)
    ap.add_argument('--log', default='jsrf_run.log')
    args = ap.parse_args(argv)

    log = args.run / args.log
    if not log.is_file():
        print(f'FAIL: {log} is not a run log')
        return 2
    lines = log.read_text(encoding='utf-8', errors='replace').splitlines()

    problems = []
    fatal = [n for n, l in enumerate(lines, 1) if RE_FATAL.search(l)]
    if fatal:
        problems.append(f'{len(fatal)} fatal/unresolved-call line(s), first at line {fatal[0]}')

    prog = last_match(RE_PROGRAM, lines)
    if not prog:
        problems.append('the executor never reported its vertex-program summary '
                        '([GPU] vertex programs: ...)')
        batches = verts = behind = None
    else:
        ln, m = prog
        batches, verts, behind = (int(x) for x in m.groups())
        print(f'line {ln}: [GPU] vertex programs: {batches} batches, {verts} vertices; '
              f'{behind} dropped behind the eye')

    gpu = last_match(RE_PROGRAM_GPU, lines)
    if not gpu:
        problems.append('the executor never reported how many program batches the back end drew')
        g_batches = g_tris = g_declined = None
    else:
        ln, m = gpu
        g_batches, g_tris, g_declined = (int(x) for x in m.groups())
        print(f'line {ln}: [GPU] of those: {g_batches} batches ({g_tris} triangles) drawn by the '
              f'back end from the program, {g_declined} declined to the CPU')

    vks = gpuvk_lines(lines)
    if not vks:
        problems.append('no [GPUVK] line carried the vsh_ counters: the back end either was not '
                        'installed, or this build predates V5')
        stats, stats_line = {}, None
    else:
        stats_line, stats = vks[-1]
        missing = [k for k in VSH_KEYS if k not in stats]
        if missing:
            problems.append(f'the [GPUVK] line at line {stats_line} is missing '
                            f'{", ".join(missing)}')
        print(f'line {stats_line}: [GPUVK] '
              + ' '.join(f'{k}={stats.get(k, "?")}' for k in VSH_KEYS))

        # The two sources are independent cumulative counters printed on different
        # cadences, so their last lines are not the same instant and requiring equality
        # would fail a correct run. What must hold: they are both monotonic, and the back
        # end's total is within one executor reporting step of the executor's total. A
        # stale, spoofed or unrelated line cannot satisfy that.
        seen = [int(m.group(1)) for m in
                (RE_PROGRAM_GPU.search(l) for l in lines) if m]
        if len(seen) < 2:
            # One executor report is not enough to know its cadence, so the two cumulative
            # totals cannot be compared. Source 1's positivity above is still the primary
            # evidence; say so rather than pretend to corroborate it.
            print('note: only one executor report, so its cadence is unknown and the two totals '
                  'are not cross-checked')
        else:
            # The back end's line is printed after the executor's last report, so its count
            # must be at least that one and at most one further reporting step of growth.
            step = seen[-1] - seen[-2]
            hits_v = int(stats.get('vsh_hits', 0))
            if hits_v < g_batches:
                problems.append(f'the two sources disagree: the executor counted {g_batches} '
                                f'batches drawn by the back end, but the back end reports only '
                                f'vsh_hits={hits_v} (one reporting step is {step})')
            elif hits_v > g_batches + step:
                problems.append(f'the two sources disagree: the back end reports vsh_hits='
                                f'{hits_v}, more than one reporting step ({step}) beyond the '
                                f'executor\'s last count of {g_batches}')
            else:
                print(f'agreement: executor {g_batches} vs back end vsh_hits={hits_v}, within one '
                      f'executor reporting step ({step})')

    # Source 1 is the primary evidence: a positive draw through a corpus shader.
    if g_batches is not None:
        if not batches:
            problems.append('the run submitted no vertex-program batch at all: the gate needs '
                            'activity, so zero misses here proves nothing')
        if g_batches <= 0:
            problems.append(f'the back end drew 0 of {batches} program batches: the corpus path '
                            f'never ran, so a zero miss count is vacuous')
        if g_tris is not None and g_tris <= 0:
            problems.append(f'the back end drew {g_batches} batches but 0 triangles')

    # Source 2 must agree with source 1 and add the identity-level detail.
    if stats:
        hits = int(stats.get('vsh_hits', 0))
        misses = int(stats.get('vsh_misses', 0))
        ident = int(stats.get('vsh_identity_fail', 0))
        declines = int(stats.get('vsh_declines', 0))
        tris = int(stats.get('vsh_tris', 0))
        if misses != args.expect_misses:
            problems.append(f'vsh_misses={misses}, expected {args.expect_misses}')
        if ident:
            problems.append(f'vsh_identity_fail={ident}: a hash matched but its identity did not, '
                            f'so the corpus disagrees with the guest')
        if hits <= 0:
            problems.append('vsh_hits=0: no batch was drawn by a corpus shader')
        if tris <= 0:
            problems.append('vsh_tris=0: the corpus path drew no triangle')
        if declines:
            print(f'note: vsh_declines={declines} (a found program this device/state could not '
                  f'draw; the executor counted those in the "declined to the CPU" field)')

    print()
    if problems:
        for p in problems:
            print(f'FAIL: {p}')
        print(f'check-vsh-run: FAILED ({len(problems)} problem(s))')
        return 1
    print('check-vsh-run: gate holds (misses 0, positive corpus-shader draw activity)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
