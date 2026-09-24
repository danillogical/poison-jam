"""AC2 provenance helper for packet A3a.

    python -X utf8 scripts\\ac2-provenance.py capture
    python -X utf8 scripts\\ac2-provenance.py check <run-dir>
    python -X utf8 scripts\\ac2-provenance.py pins

WHY THIS FILE EXISTS, AND WHY IT NO LONGER READS PATCHES.  Every revision of
packet A3a from r2 to r9 was blocked, and every blocking defect was in AC2 --
each introduced by the previous repair.  Two families:

  * checks that FAILed a correct run (r2 revision-keyed, r5 absolute path list,
    r6 set difference, r7 unrunnable capture and an unsatisfiable whole-file-hunk
    clause, r9's refuse-to-overwrite capture blocking step 0);
  * checks that PASSed an incorrect run (r8 dropped every bound on what the edit
    touches; r9's hand-parsed `diff --git` headers accepted a rename pair or a
    bare `---`/`+++` pair naming another file).

Every one of those lived in the PATCH FORMAT.  So this helper does not read
`toolkit.patch` at all.  It compares STATE:

    A. the model file's hash in build-source.json DIFFERS from step 0
       -> the model was actually compiled into the measured binary
    B. EVERY OTHER entry in build-source.json equals its step-0 hash, and no
       entry is new or missing
       -> no other compiled input changed, in EITHER repository
    C. no untracked source file under either repository's src/
    D. the classifier differs from step 0 only at its declared line, EXACTLY
    E. the policy section is byte-identical to step 0
    F. the helper and its controls match the SHA-256 values pinned in the packet

There is no header syntax, no rename form, no quoting, no line-ending variant
and no context-line subtlety to get wrong, so the r8/r9 bypasses are
structurally impossible rather than merely detected.  A new or deleted compiled
file changes the KEY SET and is caught by B.

All I/O is raw bytes.  No PowerShell, no diff parsing, no console decoding.

Exit 0 = PASS, 1 = FAIL, 2 = BLOCKED.
"""
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Every environment override is gated on A3A_TEST_MODE, so a real run cannot be
# redirected to a decoy toolkit, classifier, policy or capture.  An ungated
# XBOXRECOMP_ROOT previously made a correct run FAIL clause A.
_TEST = os.environ.get('A3A_TEST_MODE') == '1'


def _path(env_name, default):
    return os.environ.get(env_name, default) if _TEST else default


TOOLKIT_ROOT = _path('XBOXRECOMP_ROOT',
                     os.path.join(os.path.dirname(ROOT), 'xboxrecomp'))
CAP_DIR = os.path.join(ROOT, 'logs', '_ac2_pre')

MODEL_REL = 'src/kernel/xbox_memory_layout.c'
MODEL = os.path.join(TOOLKIT_ROOT, *MODEL_REL.split('/'))
CLASSIFIER_REL = 'scripts/jsrf_run_profile.py'
CLASSIFIER = os.path.join(ROOT, *CLASSIFIER_REL.split('/'))
POLICY = os.path.join(ROOT, 'docs', 'jsrf-run-profiles.md')
HEADING = b'## Unconditional modeled hardware causes'

CLASSIFIER_LINE = 391
# The exact replacement for that line, in the file's real tuple form.  A
# substring test would accept a semantic change (`X if False else '_'`), so the
# clause requires EQUALITY against this byte string.
CLASSIFIER_NEW = (b"        (AC97_READY, 'was: forced the AC97 codec-ready "
                  b"bit; the runtime no longer reads it'),")

# SHA-256 pins for this helper and its controls, as printed in the packet.
#
# THE PINS LIVE IN THE PACKET, NOT HERE.
#
# An earlier revision embedded the expected hashes in this file and compared the
# file against its own constants.  That is self-referential and was bypassable:
# the "canonical" hash masked any line *starting with* `PIN_HELPER = `, so code
# appended to that same line was invisible to the hash, and re-embedding a new
# constant made `pins` report PASS.  Both were demonstrated.
#
# The pin is therefore supplied BY THE CALLER, as the literal printed in AC2:
#
#     python -X utf8 scripts\ac2-provenance.py pins <helper_sha256> <test_sha256>
#
# `pins` hashes the RAW bytes of both files -- nothing is masked, so nothing can
# hide -- prints both hashes, and compares them against the arguments.  A
# reviewer reads the two literals out of the packet and passes them in, which is
# the only form that can catch an edit to this file.

# Paths that honour the test-mode overrides defined above.
STATE = _path('A3A_STATE', os.path.join(CAP_DIR, 'pre_state.json'))
_CLASSIFIER = _path('A3A_CLASSIFIER', CLASSIFIER)
_POLICY = _path('A3A_POLICY', POLICY)


def raw_sha(path):
    """SHA-256 of the file's raw bytes.  No masking: nothing can hide."""
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def sha_file(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def sha_policy():
    with open(_POLICY, 'rb') as f:
        data = f.read()
    i = data.find(HEADING)
    if i == -1:
        raise SystemExit('BLOCKED: policy heading not found in %s' % _POLICY)
    return hashlib.sha256(data[data.rfind(b'\n', 0, i) + 1:]).hexdigest()


def baseline_source_keys():
    """The compiled-input file set, taken from an existing build-source.json.

    Falls back to walking the two repositories if no baseline archive is
    present, so `capture` works on a clean checkout.
    """
    runs = os.path.join(ROOT, 'logs', 'runs')
    if os.path.isdir(runs):
        for name in sorted(os.listdir(runs), reverse=True):
            bs = os.path.join(runs, name, 'build-source.json')
            if os.path.exists(bs):
                try:
                    keys = list(json.load(open(bs, encoding='utf-8'))['sources'])
                    if keys:
                        return keys
                except (ValueError, KeyError):
                    pass
    keys = []
    for base_root in (ROOT, TOOLKIT_ROOT):
        for base, dirs, files in os.walk(base_root):
            dirs[:] = [d for d in dirs if d not in ('.git', 'build', 'logs')]
            for fn in files:
                if fn.endswith(('.c', '.h', '.txt', '.json', '.cmake')):
                    keys.append(os.path.join(base, fn))
    return keys


def untracked_sources(root):
    """Untracked OR index-added sources under `src/`.

    Returns (entries, ok).  `ok` is False when git itself failed -- an earlier
    version ignored the return code, so a non-repo directory (rc 128, empty
    output) made clause C PASS.

    The test is on the INDEX status character (column 1), not a prefix match:
    an earlier version checked `startswith('??') or startswith('A ')`, which
    missed `AM` (added to the index, then modified again) and every other
    two-character combination beginning with `A`.
    """
    r = subprocess.run(['git', 'status', '--porcelain=v1', '--', 'src'],
                       capture_output=True, cwd=root)
    if r.returncode != 0:
        return ([], False)
    entries = []
    for line in r.stdout.decode('utf-8', 'replace').splitlines():
        if len(line) < 4:
            continue
        x, y = line[0], line[1]
        # `??` = untracked; index status A = newly added to the index.
        if (x, y) == ('?', '?') or x == 'A':
            entries.append(line[3:])
    return (entries, True)


def capture():
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    if not os.path.exists(MODEL):
        raise SystemExit('BLOCKED: model file not found at %s' % MODEL)

    keys = baseline_source_keys()
    sources = {}
    missing = []
    for k in keys:
        if os.path.exists(k):
            sources[k] = sha_file(k)
        else:
            missing.append(k)
    with open(_CLASSIFIER, 'rb') as f:
        clf = f.read().split(b'\n')

    new = {
        'sources': sources,
        'missing_at_capture': missing,
        'policy_sha': sha_policy(),
        'classifier_lines': len(clf),
        'classifier_line_sha': [hashlib.sha256(l).hexdigest() for l in clf],
    }

    # Idempotence: re-capturing is safe ONLY while the tree still matches the
    # existing capture.  This is what lets step 0 be re-run without either
    # blocking a correct run or silently recording a post-edit tree.
    if os.path.exists(STATE):
        old = json.load(open(STATE, encoding='utf-8'))
        same = (old.get('sources') == sources
                and old.get('policy_sha') == new['policy_sha']
                and old.get('classifier_line_sha') == new['classifier_line_sha'])
        if same:
            print('capture is current: the tree still matches %s' % STATE)
            print('  %d compiled inputs unchanged' % len(sources))
            return 0
        print('BLOCKED: %s exists and the tree NO LONGER matches it.' % STATE)
        print('An edit has happened since the capture.  Re-capturing now would')
        print('record POST-edit state and make a correct run FAIL clause A.')
        print('Start a new revision instead of deleting the capture.')
        return 2

    with open(STATE, 'w', encoding='utf-8') as f:
        json.dump(new, f, indent=2)
    print('captured pre-edit state')
    print('  compiled inputs %d (%d absent)' % (len(sources), len(missing)))
    print('  model           %s  %s' % (sources.get(MODEL, 'ABSENT')[:16], MODEL_REL))
    print('  classifier      %d lines' % len(clf))
    print('  policy          %s  (heading through EOF)' % new['policy_sha'][:16])
    print('wrote %s' % STATE)
    return 0


def check_pins(expected_helper=None, expected_test=None):
    """Clause F: the tooling must match the hashes printed in the packet.

    The expected values are ARGUMENTS, supplied by the caller from the packet
    text -- not constants in this file.  A checker that compares itself to
    itself cannot detect an edit to itself, which was demonstrated.

    With no arguments this reports the hashes and fails, so `pins` can never
    silently pass by default.
    """
    here = os.path.abspath(__file__)
    test = os.path.join(ROOT, 'tests', 'test_ac2_provenance.py')
    fails = []
    got = raw_sha(here)
    print('helper  sha256: %s' % got)
    if not os.path.exists(test):
        print('controls sha256: MISSING (%s)' % test)
        fails.append('F: controls missing at tests/test_ac2_provenance.py')
        got_t = None
    else:
        got_t = raw_sha(test)
        print('controls sha256: %s' % got_t)

    if expected_helper is None or expected_test is None:
        fails.append('F: no expected hashes supplied. Pass the two literals '
                     'printed in AC2: pins <helper_sha256> <test_sha256>')
        return fails
    if got != expected_helper.lower():
        fails.append('F: helper is %s, packet pins %s' % (got, expected_helper))
    if got_t is not None and got_t != expected_test.lower():
        fails.append('F: controls are %s, packet pins %s' % (got_t, expected_test))
    return fails


def packet_pins():
    """Read the two pinned SHA-256 literals OUT OF THE PACKET TEXT.

    This is what makes clause F non-self-referential: the expected values live
    in the reviewed document, not in the checker.  Editing the checker cannot
    change what the packet says it should be.
    """
    pkt = os.path.join(ROOT, 'docs', 'packets', 'a3a-ac97-codec-model.md')
    if not os.path.exists(pkt):
        return (None, None)
    text = open(pkt, encoding='utf-8', errors='replace').read()
    pat = re.compile(r'`([0-9a-fA-F]{64})`')
    idx = text.find('**The two literals, raw SHA-256, nothing masked:**')
    if idx == -1:
        return (None, None)
    found = [x.lower() for x in pat.findall(text[idx:idx + 2000])]
    if len(found) >= 2:
        return (found[0], found[1])
    return (None, None)


def check(run_dir):
    if not os.path.exists(STATE):
        print('BLOCKED: no pre-edit capture at %s' % STATE)
        return 2
    # A corrupt, wrong-shape or INCOMPLETE capture is also an instrument
    # problem, not evidence about the model, so it is BLOCKED rather than a
    # traceback.  Validating only the top level and "sources" was not enough:
    # a capture of {"sources": {}} with no `policy_sha` raised KeyError, and a
    # wrong-typed `classifier_lines` raised TypeError -- both exited 1, which
    # the packet's own wording says should be UNKNOWN.
    try:
        with open(STATE, encoding='utf-8') as f:
            state = json.load(f)
        if not isinstance(state, dict):
            raise ValueError('capture is %s, expected an object'
                             % type(state).__name__)
        required = {'sources': dict,
                    'policy_sha': str,
                    'classifier_lines': int,
                    'classifier_line_sha': list}
        for field, kind in required.items():
            if field not in state:
                raise ValueError('capture has no %r field' % field)
            val = state[field]
            # `bool` is a subclass of `int`, so `isinstance(True, int)` passes;
            # reject it explicitly, because `classifier_lines: true` is not a
            # line count.
            if kind is int and isinstance(val, bool):
                raise ValueError('capture field %r is a bool, expected int' % field)
            if not isinstance(val, kind):
                raise ValueError('capture field %r is %s, expected %s'
                                 % (field, type(val).__name__, kind.__name__))
        if len(state['classifier_line_sha']) != state['classifier_lines']:
            raise ValueError('capture classifier_line_sha has %d entries but '
                             'classifier_lines is %d'
                             % (len(state['classifier_line_sha']),
                                state['classifier_lines']))
        # Element types matter too: a null or int entry in `sources` or in
        # `classifier_line_sha` is malformed evidence, not a hash mismatch.
        for key, val in state['sources'].items():
            if not isinstance(key, str) or not isinstance(val, str):
                raise ValueError('capture sources has a non-string entry '
                                 '(%r: %s)' % (key, type(val).__name__))
        for i, val in enumerate(state['classifier_line_sha']):
            if not isinstance(val, str):
                raise ValueError('capture classifier_line_sha[%d] is %s, '
                                 'expected str' % (i, type(val).__name__))
    except (ValueError, OSError, TypeError, AttributeError,
            RecursionError, KeyError, IndexError) as exc:
        print('BLOCKED: the pre-edit capture at %s is unreadable, malformed or '
              'incomplete (%s: %s)' % (STATE, type(exc).__name__, exc))
        return 2

    def need(name):
        p = os.path.join(run_dir, name)
        if not os.path.exists(p):
            print('BLOCKED: %s missing from %s' % (name, run_dir))
            sys.exit(2)
        return p

    # NOTE: `toolkit.patch` is deliberately NOT required.  This helper compares
    # STATE via build-source.json, so a missing or unparseable patch cannot
    # block a correct run -- which is what made r7 and r9 block at step 0.
    #
    # A missing, unparseable OR WRONG-SHAPE build-source.json is BLOCKED
    # (exit 2), not FAIL.  The evidence cannot be read, which is an instrument
    # problem rather than evidence about the model.  Two earlier revisions got
    # this wrong: one let a corrupt file raise out of json.load and exit 1, and
    # the next caught only syntax/OS errors, so a well-formed file with the
    # wrong SHAPE (`{"sources": null}`, `[]`, `{"sources": "abc"}`) still
    # raised AttributeError and exited 1 -- labelled FAIL, contradicting the
    # packet's own "wrong shape is UNKNOWN" wording.  Both are now caught.
    bs_path = need('build-source.json')
    try:
        with open(bs_path, encoding='utf-8') as f:
            doc = json.load(f)
        if not isinstance(doc, dict):
            raise ValueError('top level is %s, expected an object'
                             % type(doc).__name__)
        # A MISSING "sources" key is malformed.  `doc.get('sources', {})` used
        # to treat it as an empty object, so `{}` fell through to clauses A/B/D
        # and reported FAIL with no BLOCKED: line -- contradicting the packet's
        # "wrong shape is BLOCKED" wording.  An EMPTY but present `{}` is still
        # a legitimate FAIL: the document is well formed, it simply records no
        # model, so clause A correctly rejects it.
        if 'sources' not in doc:
            raise ValueError('no "sources" key')
        src = doc['sources']
        if not isinstance(src, dict):
            raise ValueError('"sources" is %s, expected an object'
                             % type(src).__name__)
        # Element types: a non-string path or hash is malformed evidence.
        for k, v in src.items():
            if not isinstance(k, str) or not isinstance(v, str):
                raise ValueError('"sources" has a non-string entry '
                                 '(%r: %s)' % (k, type(v).__name__))
    except (ValueError, OSError, TypeError, AttributeError,
            RecursionError, KeyError, IndexError) as exc:
        print('BLOCKED: %s is unreadable or malformed (%s: %s)'
              % (bs_path, type(exc).__name__, exc))
        return 2

    def key(p):
        return os.path.normcase(os.path.normpath(p))

    src_n = {key(k): v for k, v in src.items()}
    cap_n = {key(k): v for k, v in state['sources'].items()}
    model_key = key(MODEL)
    fails = []

    # ---- A. the model was compiled in, with content differing from step 0 ---
    got = src_n.get(model_key)
    if got is None:
        fails.append('A: %s absent from build-source.json -- the model was not '
                     'compiled into the measured binary' % MODEL_REL)
    elif got == cap_n.get(model_key):
        fails.append('A: %s carries its PRE-EDIT hash -- the model was not '
                     'compiled in' % MODEL_REL)

    # ---- B. no OTHER compiled input changed, and none was added or removed --
    # This single clause replaces all patch parsing and covers BOTH repos.
    changed, added = [], []
    for k, want in cap_n.items():
        if k == model_key:
            continue
        have = src_n.get(k)
        if have is None:
            changed.append('%s (now absent)' % os.path.basename(k))
        elif have != want:
            changed.append(os.path.basename(k))
    for k in src_n:
        if k != model_key and k not in cap_n:
            added.append(os.path.basename(k))
    if changed:
        fails.append('B: %d other compiled input(s) changed: %s'
                     % (len(changed), sorted(changed)[:8]))
    if added:
        fails.append('B: %d new compiled input(s) not in the capture: %s'
                     % (len(added), sorted(added)[:8]))

    # ---- C. no untracked or staged-new source under either src/ ------------
    for label, root in (('game', ROOT), ('toolkit', TOOLKIT_ROOT)):
        entries, ok = untracked_sources(root)
        if not ok:
            # Fail CLOSED: an unreadable git state is not evidence of cleanliness.
            fails.append('C: git status failed for the %s repository, so its '
                         'untracked state is undetermined' % label)
            continue
        for u in entries:
            fails.append('C: untracked or staged-new source in %s: %s'
                         % (label, u))

    # ---- D. the classifier differs ONLY at its declared line, exactly -------
    if not os.path.exists(_CLASSIFIER):
        fails.append('D: classifier missing at %s' % CLASSIFIER_REL)
    else:
        with open(_CLASSIFIER, 'rb') as f:
            now = f.read().split(b'\n')
        old = state.get('classifier_line_sha')
        if old is None:
            fails.append('D: capture lacks per-line classifier hashes')
        elif len(now) != state['classifier_lines']:
            fails.append('D: classifier line count changed (%d -> %d)'
                         % (state['classifier_lines'], len(now)))
        else:
            changed_l = [i + 1 for i in range(len(now))
                         if hashlib.sha256(now[i]).hexdigest() != old[i]]
            if not changed_l:
                fails.append('D: the classifier was NOT edited; step 3 requires '
                             'line %d to be corrected' % CLASSIFIER_LINE)
            elif changed_l != [CLASSIFIER_LINE]:
                fails.append('D: classifier changed at lines %s, expected only '
                             'line %d' % (changed_l, CLASSIFIER_LINE))
            elif now[CLASSIFIER_LINE - 1] != CLASSIFIER_NEW:
                fails.append('D: classifier line %d is not EXACTLY the declared '
                             'replacement text' % CLASSIFIER_LINE)

    # ---- E. the policy section is byte-identical ---------------------------
    now_pol = sha_policy()
    if now_pol != state['policy_sha']:
        fails.append('E: the policy section was amended (sha %s, expected %s)'
                     % (now_pol[:16], state['policy_sha'][:16]))

    # ---- F. the pinned tooling matches the PACKET -------------------------
    fails.extend(check_pins(*packet_pins()))

    if fails:
        print('FAIL')
        for f in fails:
            print('  ' + f)
        return 1
    print('PASS: AC2 satisfied -- model compiled in; no other compiled input '
          'changed in either repository; no untracked source; classifier changed '
          'only at line %d; policy unamended; tooling pins match the packet'
          % CLASSIFIER_LINE)
    return 0


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == 'capture':
        sys.exit(capture())
    elif len(sys.argv) >= 3 and sys.argv[1] == 'check':
        sys.exit(check(sys.argv[2]))
    elif len(sys.argv) >= 2 and sys.argv[1] == 'pins':
        # Expected values come from the PACKET, or from explicit arguments.
        if len(sys.argv) >= 4:
            exp = (sys.argv[2], sys.argv[3])
        else:
            exp = packet_pins()
        f = check_pins(*exp)
        print('PASS: pins match the packet' if not f
              else 'FAIL\n  ' + '\n  '.join(f))
        sys.exit(1 if f else 0)
    else:
        print(__doc__)
        sys.exit(2)
