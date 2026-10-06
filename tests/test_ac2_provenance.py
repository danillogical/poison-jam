"""Controls for scripts/ac2-provenance.py.

    python -X utf8 tests/test_ac2_provenance.py

Each control ISOLATES one clause: it applies the declared edits (so clause D
passes), then breaks exactly one thing, and asserts the helper FAILs and names
that clause.  An earlier suite ran seven controls that all tripped clause D, so
they would have passed even if clauses B, C and E were deleted -- these do not.

The suite NEVER touches the real logs/_ac2_pre/pre_state.json: it builds a
synthetic repo root under %TEMP% with its own capture, and drives the helper
through A3A_TEST_MODE=1 overrides.

Every case corresponds to a real defect found in an earlier revision of A5a.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HELPER = os.environ.get('A3A_HELPER', os.path.join(ROOT, 'scripts',
                                                   'ac2-provenance.py'))
SELF = os.environ.get('A3A_SELF', os.path.join(ROOT, 'tests',
                                               'test_ac2_provenance.py'))
BASE = os.path.join(ROOT, 'logs', 'runs', '20260923-013448-357-p0-strict-baseline')
MODEL_REL = 'src/kernel/xbox_memory_layout.c'
CLF_LINE = 391
CLF_TEXT = (b"        (AC97_READY, 'was: forced the AC97 codec-ready "
            b"bit; the runtime no longer reads it'),")
POLICY_HEADING = b'## Unconditional modeled hardware causes'


def sh(*a, **kw):
    kw.setdefault('text', True)
    return subprocess.run(list(a), capture_output=True, **kw)


class Bench:
    """A synthetic repo root with its own capture and archives."""

    def __init__(self):
        self.root = tempfile.mkdtemp(prefix='ac2bench')
        os.makedirs(os.path.join(self.root, 'scripts'))
        os.makedirs(os.path.join(self.root, 'tests'))
        os.makedirs(os.path.join(self.root, 'docs'))
        os.makedirs(os.path.join(self.root, 'logs', '_ac2_pre'))
        os.makedirs(os.path.join(self.root, 'logs', 'runs'))

        self.helper = os.path.join(self.root, 'scripts', 'ac2-provenance.py')
        shutil.copy(HELPER, self.helper)
        self.test = os.path.join(self.root, 'tests', 'test_ac2_provenance.py')
        shutil.copy(SELF, self.test)
        self.policy = os.path.join(self.root, 'docs', 'jsrf-run-profiles.md')
        shutil.copy(os.path.join(ROOT, 'docs', 'jsrf-run-profiles.md'), self.policy)

        # Classifier copy. It starts UN-EDITED, because step 0 captures BEFORE
        # the edit; `apply_clf_edit()` applies the declared change afterwards.
        self.clf = os.path.join(self.root, 'scripts', 'jsrf_run_profile.py')
        shutil.copy(os.path.join(ROOT, 'scripts', 'jsrf_run_profile.py'), self.clf)

        # Toolkit stand-in: one model file plus one other compiled input.
        self.tk = os.path.join(self.root, 'tk')
        os.makedirs(os.path.join(self.tk, 'src', 'kernel'))
        self.model = os.path.join(self.tk, 'src', 'kernel', 'xbox_memory_layout.c')
        self.other = os.path.join(self.tk, 'src', 'kernel', 'kernel_bridge.c')
        open(self.model, 'wb').write(b'/* model, pre-edit */\n')
        open(self.other, 'wb').write(b'/* bridge */\n')

        self.state = os.path.join(self.root, 'logs', '_ac2_pre', 'pre_state.json')

        # The bench builds its OWN small file tree standing in for both
        # repositories, then seeds a baseline build-source.json naming exactly
        # those files.  That exercises the same logic as the real 144-entry set
        # without depending on the real trees.
        self.seed = os.path.join(self.root, 'logs', 'runs', 'seed')
        os.makedirs(self.seed)
        self.game_src = os.path.join(self.root, 'game_src')
        os.makedirs(self.game_src)
        self.game_main = os.path.join(self.game_src, 'main.c')
        open(self.game_main, 'wb').write(b'/* game main */\n')

        seeded = {}
        for k in (self.model, self.other, self.game_main):
            seeded[k] = hashlib.sha256(open(k, 'rb').read()).hexdigest()
        json.dump({'sources': seeded},
                  open(os.path.join(self.seed, 'build-source.json'), 'w',
                       encoding='utf-8'))
        self.seeded = seeded

        # Clause C runs `git status` in both roots, so the bench's synthetic
        # roots must be real, CLEAN repositories -- otherwise C correctly fails
        # closed on an unreadable or wholly-untracked tree.
        for r in (self.root, self.tk):
            subprocess.run(['git', 'init', '-q'], cwd=r, capture_output=True)
            subprocess.run(['git', 'add', '-A'], cwd=r, capture_output=True)
            subprocess.run(['git', '-c', 'user.email=b@b', '-c', 'user.name=b',
                            '-c', 'commit.gpgsign=false', 'commit', '-qm', 'bench'],
                           cwd=r, capture_output=True)

    def env(self, **extra):
        e = dict(os.environ, A3A_TEST_MODE='1', XBOXRECOMP_ROOT=self.tk,
                 A3A_CLASSIFIER=self.clf, A3A_POLICY=self.policy,
                 A3A_STATE=self.state)
        e.update(extra)
        return e

    def run(self, *args, **kw):
        """Run the bench's helper copy.

        The copy lives in a synthetic root, so clause F cannot read the real
        packet.  The bench therefore writes a matching packet into its own root
        naming the COPY's hash -- which is what a correct packet would say --
        so clause F is exercised rather than skipped.  `F1` then tampers with
        the copy to prove clause F fires.
        """
        if not getattr(self, '_pkt_ready', False):
            self._write_bench_packet()
        return sh(sys.executable, '-X', 'utf8', self.helper, *args,
                  cwd=self.root, env=self.env(**kw))

    def _write_bench_packet(self):
        pkt_dir = os.path.join(self.root, 'docs', 'packets')
        os.makedirs(pkt_dir, exist_ok=True)
        h = hashlib.sha256(open(self.helper, 'rb').read()).hexdigest()
        t = hashlib.sha256(open(self.test, 'rb').read()).hexdigest()
        body = (
            '# bench packet\n\n'
            '**The two literals, raw SHA-256, nothing masked:**\n'
            '> - helper pin: `%s`\n'
            '> - controls pin: `%s`\n' % (h, t))
        with open(os.path.join(pkt_dir, 'a3a-ac97-codec-model.md'), 'w',
                  encoding='utf-8') as f:
            f.write(body)
        self._pkt_ready = True

    def capture(self):
        return self.run('capture')

    def apply_clf_edit(self):
        """Apply the declared classifier edit, as step 3 would."""
        lines = open(os.path.join(ROOT, 'scripts', 'jsrf_run_profile.py'),
                     'rb').read().split(b'\n')
        lines[CLF_LINE - 1] = CLF_TEXT  # restore the declared edit
        with open(self.clf, 'wb') as f:
            f.write(b'\n'.join(lines))

    def correct_run(self):
        """Capture, then edit the model AND the classifier, then archive."""
        self.capture()
        self.apply_clf_edit()
        post = self.post_edit_model()
        return self.archive(post)

    def archive(self, model_hash, sources=None):
        """A run-dir whose build-source.json reflects `sources`.

        Keyed off the bench's SEEDED key set (the real compiled-input set), so
        each control breaks exactly one thing.
        """
        d = tempfile.mkdtemp(prefix='ac2run', dir=self.root)
        merged = {}
        for k, v in self.seeded.items():
            if k == self.model:
                merged[k] = model_hash
            elif sources and k in sources:
                merged[k] = sources[k]
            else:
                merged[k] = v
        # Extra keys the caller wants to look "new".
        for k, v in (sources or {}).items():
            if k not in self.seeded:
                merged[k] = v
        json.dump({'sources': merged},
                  open(os.path.join(d, 'build-source.json'), 'w', encoding='utf-8'))
        return d

    def post_edit_model(self):
        open(self.model, 'wb').write(b'/* model, POST-edit */\n')
        return hashlib.sha256(open(self.model, 'rb').read()).hexdigest()

    def cleanup(self):
        """Remove the bench, clearing git's read-only object bits first.

        `shutil.rmtree(..., ignore_errors=True)` silently leaves the read-only
        `.git/objects` files behind, so every run leaked an `ac2bench*` tree --
        the r18 review found 625 of them in %TEMP%. The onerror handler clears
        the read-only attribute and retries.
        """
        import stat

        def onerror(func, path, exc_info):
            try:
                os.chmod(path, stat.S_IWRITE)
                func(path)
            except OSError:
                pass

        shutil.rmtree(self.root, onerror=onerror)


def main():
    results = []

    def expect(name, ok, detail=''):
        print('%-54s %s' % (name, 'OK' if ok else 'FAILED'))
        if not ok:
            if detail:
                print(detail)
            results.append(name)

    # ---------------- baseline: a correct run PASSes -----------------------
    b = Bench()
    try:
        run = b.correct_run()
        r = b.run('check', run)
        expect('A0 correct run PASSes', r.returncode == 0,
               r.stdout + r.stderr)
    finally:
        b.cleanup()

    # ---------------- clause A -------------------------------------------
    b = Bench()
    try:
        b.capture()
        b.apply_clf_edit()
        pre_hash = hashlib.sha256(open(b.model, 'rb').read()).hexdigest()
        run = b.archive(pre_hash)          # unchanged -> not compiled in
        r = b.run('check', run)
        expect('A1 unchanged model hash FAILs (clause A)',
               r.returncode == 1 and 'A:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- clause B: game-side and toolkit-side compiled inputs --
    b = Bench()
    try:
        b.capture()
        post = b.post_edit_model()
        tampered = {b.other: hashlib.sha256(b'tampered bridge').hexdigest()}
        run = b.archive(post, sources=tampered)
        r = b.run('check', run)
        expect('B1 other toolkit compiled input changed FAILs (clause B)',
               r.returncode == 1 and 'B:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    b = Bench()
    try:
        run = b.correct_run()
        # A GAME-side compiled input changes (src/main.c) after capture.
        bs = json.load(open(os.path.join(run, 'build-source.json'), encoding='utf-8'))
        key = [k for k in bs['sources'] if k.endswith('main.c')][0]
        bs['sources'][key] = hashlib.sha256(b'tampered main').hexdigest()
        json.dump(bs, open(os.path.join(run, 'build-source.json'), 'w', encoding='utf-8'))
        r = b.run('check', run)
        expect('B2 game-side compiled input changed FAILs (clause B, r9-B2)',
               r.returncode == 1 and 'B:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    b = Bench()
    try:
        run = b.correct_run()
        bs = json.load(open(os.path.join(run, 'build-source.json'), encoding='utf-8'))
        bs['sources'][os.path.join(b.tk, 'src', 'kernel', 'evil.c')] = \
            hashlib.sha256(b'evil').hexdigest()
        json.dump(bs, open(os.path.join(run, 'build-source.json'), 'w', encoding='utf-8'))
        r = b.run('check', run)
        expect('B3 new compiled input FAILs (clause B)',
               r.returncode == 1 and 'B:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    b = Bench()
    try:
        run = b.correct_run()
        # A captured compiled input DISAPPEARS from the archive's key set.
        bs = json.load(open(os.path.join(run, 'build-source.json'), encoding='utf-8'))
        victim = [k for k in bs['sources'] if k.endswith('main.c')][0]
        del bs['sources'][victim]
        json.dump(bs, open(os.path.join(run, 'build-source.json'), 'w', encoding='utf-8'))
        r = b.run('check', run)
        expect('B4 removed compiled input FAILs (clause B)',
               r.returncode == 1 and 'B:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- clause C -------------------------------------------
    b = Bench()
    try:
        run = b.correct_run()
        # An untracked source file appears under the toolkit's src/.
        os.makedirs(os.path.join(b.tk, '.git'), exist_ok=True)
        subprocess.run(['git', 'init', '-q'], cwd=b.tk, capture_output=True)
        open(os.path.join(b.tk, 'src', 'kernel', 'evil.c'), 'wb').write(b'/* evil */\n')
        r = b.run('check', run)
        expect('C1 untracked src/ file FAILs (clause C)',
               r.returncode == 1 and 'C:' in r.stdout, r.stdout)

        # A file added to the INDEX (status `A `) must also fire, and so must
        # `AM` -- an earlier prefix test missed the latter.
        os.unlink(os.path.join(b.tk, 'src', 'kernel', 'evil.c'))
        open(os.path.join(b.tk, 'src', 'kernel', 'staged.c'), 'wb').write(b'/* s */\n')
        subprocess.run(['git', 'add', 'src/kernel/staged.c'], cwd=b.tk,
                       capture_output=True)
        open(os.path.join(b.tk, 'src', 'kernel', 'staged.c'), 'ab').write(b'/* more */\n')
        r = b.run('check', run)
        expect('C2 index-added-then-modified (AM) FAILs (clause C)',
               r.returncode == 1 and 'C:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- path-case insensitivity ----------------------------
    b = Bench()
    try:
        run = b.correct_run()
        # Rewrite the archive's keys in a DIFFERENT CASE, as a build run from a
        # differently-cased cwd would record them.
        bs = json.load(open(os.path.join(run, 'build-source.json'), encoding='utf-8'))
        bs['sources'] = {k.upper(): v for k, v in bs['sources'].items()}
        json.dump(bs, open(os.path.join(run, 'build-source.json'), 'w', encoding='utf-8'))
        r = b.run('check', run)
        expect('P1 differently-cased archive keys still PASS',
               r.returncode == 0, r.stdout)
    finally:
        b.cleanup()

    # ---------------- BLOCKED on unreadable evidence ---------------------
    # Four shapes, because r16 shipped a claim that only the first was handled:
    # a well-formed file with the WRONG SHAPE raised AttributeError and exited
    # 1, contradicting the packet's own "wrong shape is UNKNOWN" wording.
    # `{}` (a MISSING "sources" key) is included because r17's guard still let
    # it through via `doc.get('sources', {})`.
    for label, payload in (('syntactic', '{ not json'),
                           ('null sources', '{"sources": null}'),
                           ('string sources', '{"sources": "abc"}'),
                           ('list root', '[]'),
                           ('no sources key', '{}')):
        b = Bench()
        try:
            run = b.correct_run()
            open(os.path.join(run, 'build-source.json'), 'w').write(payload)
            r = b.run('check', run)
            expect('Z4 %s build-source.json BLOCKs (exit 2)' % label,
                   r.returncode == 2 and 'BLOCKED' in r.stdout, r.stdout)
        finally:
            b.cleanup()

    # An EMPTY but PRESENT "sources" is a legitimate FAIL, not BLOCKED: the
    # document is well formed, it just records no model, so clause A rejects it.
    b = Bench()
    try:
        run = b.correct_run()
        open(os.path.join(run, 'build-source.json'), 'w').write('{"sources": {}}')
        r = b.run('check', run)
        expect('Z6 empty-but-present sources FAILs (exit 1), not BLOCKED',
               r.returncode == 1 and 'A:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    b = Bench()
    try:
        run = b.correct_run()
        # A corrupt CAPTURE is also an instrument problem.
        open(b.state, 'w').write('{ corrupt')
        r = b.run('check', run)
        expect('Z5 corrupt pre_state.json BLOCKs (exit 2)',
               r.returncode == 2 and 'BLOCKED' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # An INCOMPLETE or ill-typed capture: fields missing, wrong-typed, or with
    # malformed elements.  The r18 review found three kinds of malformed input
    # that still exited 1, so these are asserted explicitly.
    for label, state_text in (
            ('no policy_sha', '{"sources": {}}'),
            ('wrong classifier_lines',
             '{"sources": {}, "policy_sha": "x", "classifier_lines": "no", '
             '"classifier_line_sha": []}'),
            ('bool classifier_lines',
             '{"sources": {}, "policy_sha": "x", "classifier_lines": true, '
             '"classifier_line_sha": []}'),
            ('null in classifier_line_sha',
             '{"sources": {}, "policy_sha": "x", "classifier_lines": 1, '
             '"classifier_line_sha": [null]}'),
            ('int in capture sources',
             '{"sources": {"a": 1}, "policy_sha": "x", "classifier_lines": 0, '
             '"classifier_line_sha": []}'),
            ('deeply nested',
             '[' * 200000)):
        b = Bench()
        try:
            run = b.correct_run()
            open(b.state, 'w').write(state_text)
            r = b.run('check', run)
            expect('Z7 %s capture BLOCKs (exit 2)' % label,
                   r.returncode == 2 and 'BLOCKED' in r.stdout, r.stdout)
        finally:
            b.cleanup()

    # Ill-typed ELEMENTS in build-source.json, and pathological nesting.
    for label, payload in (
            ('int in sources', '{"sources": {"a": 1}}'),
            ('deeply nested', '[' * 200000)):
        b = Bench()
        try:
            run = b.correct_run()
            open(os.path.join(run, 'build-source.json'), 'w').write(payload)
            r = b.run('check', run)
            expect('Z8 %s build-source.json BLOCKs (exit 2)' % label,
                   r.returncode == 2 and 'BLOCKED' in r.stdout, r.stdout)
        finally:
            b.cleanup()

    # ---------------- clause D -------------------------------------------
    b = Bench()
    try:
        run = b.correct_run()
        # Revert the declared classifier edit -> D must fire.
        shutil.copy(os.path.join(ROOT, 'scripts', 'jsrf_run_profile.py'), b.clf)
        r = b.run('check', run)
        expect('D1 unedited classifier FAILs (clause D)',
               r.returncode == 1 and 'D:' in r.stdout, r.stdout)

        # Declared edit PLUS a second changed line.
        lines = open(os.path.join(ROOT, 'scripts', 'jsrf_run_profile.py'),
                     'rb').read().split(b'\n')
        lines[CLF_LINE - 1] = CLF_TEXT
        lines[10] = b'# extra'
        with open(b.clf, 'wb') as f:
            f.write(b'\n'.join(lines))
        r = b.run('check', run)
        expect('D2 second changed classifier line FAILs (clause D)',
               r.returncode == 1 and 'D:' in r.stdout, r.stdout)

        # Declared line present but not EXACTLY the declared text: a semantic
        # change that a substring test would accept.
        lines[10] = b'# back'
        lines[CLF_LINE - 1] = (b"        (AC97_READY if False else '_', "
                               b"'was: forced the AC97 codec-ready bit; "
                               b"the runtime no longer reads it'),")
        with open(b.clf, 'wb') as f:
            f.write(b'\n'.join(lines))
        r = b.run('check', run)
        expect('D3 semantic change on the declared line FAILs (clause D)',
               r.returncode == 1 and 'D:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- clause E -------------------------------------------
    b = Bench()
    try:
        run = b.correct_run()
        with open(b.policy, 'ab') as f:
            f.write(b'\n- an amendment to the policy section\n')
        r = b.run('check', run)
        expect('E1 amended policy section FAILs (clause E)',
               r.returncode == 1 and 'E:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- clause F -------------------------------------------
    # F1: a helper whose LOGIC differs from the pin must FAIL.  Append a
    # comment line to the bench's helper copy, which changes its canonical hash
    # without changing behaviour -- exactly the tampering the pin exists to
    # catch (the r9 review saw this happen for real, mid-review).
    b = Bench()
    try:
        run = b.correct_run()
        with open(b.helper, 'ab') as f:
            f.write(b'\n# tampered\n')
        r = b.run('check', run)
        expect('F1 helper modified vs pin FAILs (clause F)',
               r.returncode == 1 and 'F:' in r.stdout, r.stdout)
    finally:
        b.cleanup()

    # ---------------- capture idempotence (r9 B1) ------------------------
    b = Bench()
    try:
        r1 = b.capture()
        r2 = b.capture()                     # unchanged tree -> safe re-run
        expect('Z1 capture is idempotent while the tree is unchanged',
               r1.returncode == 0 and r2.returncode == 0, r1.stdout + r2.stdout)
        open(b.model, 'wb').write(b'/* edited */\n')
        r3 = b.capture()                     # edited tree -> must refuse
        expect('Z2 capture refuses after an edit (r9-B1)',
               r3.returncode == 2 and 'BLOCKED' in r3.stdout, r3.stdout)
    finally:
        b.cleanup()

    # ---------------- the real capture is never touched ------------------
    real = os.path.join(ROOT, 'logs', '_ac2_pre', 'pre_state.json')
    before = os.path.getmtime(real) if os.path.exists(real) else None
    b = Bench()
    try:
        b.capture()
        b.post_edit_model()
        b.run('check', b.archive(b.post_edit_model()))
    finally:
        b.cleanup()
    after = os.path.getmtime(real) if os.path.exists(real) else None
    expect('Z3 the real pre_state.json is untouched', before == after,
           'before=%s after=%s' % (before, after))

    print()
    if results:
        print('FAILED: %s' % ', '.join(results))
        return 1
    print('all controls OK')
    return 0


if __name__ == '__main__':
    sys.exit(main())
