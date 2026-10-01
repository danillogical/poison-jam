"""Controls for the pre-commit gates (plan T7).

T7's acceptance is "a staged `game/` path and a planted fake token are both
refused".  Installing pre-commit does not establish that; the hooks have to be
**exercised**, and the cheapest honest way is to run each hook against a scratch
repository that really has the offending path staged.

Why a scratch repository rather than the real index: staging a `game/` path in the
game repository is the exact action the hook exists to prevent, and a test that
does it -- even transiently -- can lose a race with any other writer and leave
retail assets in the index.  A scratch repo has the same `git diff --cached`
semantics with none of that exposure.

Each case asserts on the hook's **exit code and its finding text**, not on a
message the hook might print for another reason.  A hook that refuses everything
would pass a bare exit-code check, so every refusal case is paired with a
known-good case that must exit 0.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STAGED_PATHS = ROOT / 'scripts' / 'precommit-staged-paths.py'
REPO_CHECKS = ROOT / 'scripts' / 'precommit-repo-checks.py'
MERGE_GATE = ROOT / 'scripts' / 'precommit-merge-gate.py'


def git(repo: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(['git', *args], cwd=str(repo),
                          capture_output=True, text=True)


def run_hook(script: Path, repo: Path, *extra: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, '-X', 'utf8', str(script), '--root', str(repo), *extra],
        capture_output=True, text=True, cwd=str(repo))


class ScratchRepo:
    """A throwaway git repository with one initial commit."""

    def __enter__(self) -> Path:
        self.path = Path(tempfile.mkdtemp(prefix='jsrf-precommit-'))
        git(self.path, 'init', '-q')
        git(self.path, 'config', 'user.email', 'test@example.invalid')
        git(self.path, 'config', 'user.name', 'Pre-commit Control')
        # A global commit.gpgsign would otherwise sign every scratch commit,
        # and a signing program that waits for its owner hangs the test.
        git(self.path, 'config', 'commit.gpgsign', 'false')
        git(self.path, 'config', 'tag.gpgsign', 'false')
        (self.path / 'README.md').write_text('scratch\n', encoding='utf-8')
        git(self.path, 'add', 'README.md')
        git(self.path, 'commit', '-q', '-m', 'initial')
        return self.path

    def __exit__(self, *_exc) -> None:
        shutil.rmtree(self.path, ignore_errors=True)


class StagedPathGateTests(unittest.TestCase):
    def test_clean_stage_is_allowed(self) -> None:
        """Known good: an ordinary file must not be refused."""
        with ScratchRepo() as repo:
            (repo / 'notes.md').write_text('ordinary content\n', encoding='utf-8')
            git(repo, 'add', 'notes.md')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('no game/ path', result.stdout)

    def test_staged_game_path_is_refused(self) -> None:
        """T7 control 1: a staged `game/` path must be refused by name."""
        with ScratchRepo() as repo:
            (repo / 'game').mkdir()
            (repo / 'game' / 'default.xbe').write_bytes(b'not really an xbe\n')
            # `-f` is how a gitignored path reaches the index in practice.
            git(repo, 'add', '-f', 'game/default.xbe')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('game/default.xbe', result.stdout)
            self.assertIn('nondistributable retail assets', result.stdout)

    def test_planted_fake_token_is_refused(self) -> None:
        """T7 control 2: a planted fake token must be refused, and redacted."""
        token = 'ghp_' + 'A1b2C3d4E5f6G7h8I9j0K1l2M3n4O5p6Q7r8'
        with ScratchRepo() as repo:
            (repo / 'leak.py').write_text(
                f'TOKEN = "{token}"\n', encoding='utf-8')
            git(repo, 'add', 'leak.py')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('github_token', result.stdout)
            self.assertIn('leak.py', result.stdout)
            # The report must locate the secret, not become a second copy of it.
            self.assertNotIn(token, result.stdout)
            self.assertIn('REDACTED', result.stdout)

    def test_planted_private_key_header_is_refused(self) -> None:
        # Assembled from fragments so this fixture does not itself contain the
        # contiguous marker it plants.  Measured: the first version of this test
        # was refused by the hook it tests, because the literal string in the
        # fixture matched -- a true positive on the wrong file, and a false
        # positive for a reader.
        marker = '-----BEGIN ' + 'RSA ' + 'PRIVATE KEY-----'
        with ScratchRepo() as repo:
            (repo / 'id_rsa').write_text(f'{marker}\nMIIabc\n', encoding='utf-8')
            git(repo, 'add', 'id_rsa')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('private_key', result.stdout)

    def test_the_scanner_does_not_flag_its_own_definition(self) -> None:
        """A gate that refuses its own source is a gate people learn to bypass.

        Measured: the first commit carrying these hooks was refused by
        `staged-paths`, which found the literal key markers inside
        `scripts/secret_patterns.py` and inside this test file.
        """
        for relative in ('scripts/secret_patterns.py', 'tests/test_precommit_hooks.py'):
            text = (ROOT / relative).read_text(encoding='utf-8')
            sys.path.insert(0, str(ROOT / 'scripts'))
            from secret_patterns import COMPILED as patterns
            for name, pattern in patterns:
                self.assertIsNone(
                    pattern.search(text),
                    f'{relative} matches its own {name} pattern; assemble the '
                    f'literal from fragments instead')

    def test_unstaged_secret_is_not_refused(self) -> None:
        """A secret in the working tree but not in the index is not a commit."""
        token = 'ghp_' + 'Z9y8X7w6V5u4T3s2R1q0P9o8N7m6L5k4J3i2'
        with ScratchRepo() as repo:
            (repo / 'scratch.txt').write_text(f'{token}\n', encoding='utf-8')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


class MergeGateTests(unittest.TestCase):
    def test_no_merge_in_progress_is_allowed(self) -> None:
        with ScratchRepo() as repo:
            result = run_hook(MERGE_GATE, repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('no merge in progress', result.stdout)

    def test_unresolved_merge_path_is_refused(self) -> None:
        """A real conflicted merge must be refused, not committed over."""
        with ScratchRepo() as repo:
            (repo / 'shared.txt').write_text('base\n', encoding='utf-8')
            git(repo, 'add', 'shared.txt')
            git(repo, 'commit', '-q', '-m', 'add shared')
            # The initial branch name is host-configurable, so read it rather
            # than assuming `master`.
            trunk = git(repo, 'rev-parse', '--abbrev-ref', 'HEAD').stdout.strip()
            git(repo, 'checkout', '-q', '-b', 'side')
            (repo / 'shared.txt').write_text('side\n', encoding='utf-8')
            git(repo, 'commit', '-q', '-am', 'side change')
            git(repo, 'checkout', '-q', trunk)
            (repo / 'shared.txt').write_text('main\n', encoding='utf-8')
            git(repo, 'commit', '-q', '-am', 'main change')
            merge = git(repo, 'merge', 'side')
            self.assertNotEqual(merge.returncode, 0,
                                'the control requires a real conflict')

            result = run_hook(MERGE_GATE, repo)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('UNRESOLVED', result.stdout)
            self.assertIn('shared.txt', result.stdout)


class RepoCheckGateTests(unittest.TestCase):
    def test_real_repository_passes_its_own_checkers(self) -> None:
        """The gate must be green on the repository it guards."""
        result = subprocess.run(
            [sys.executable, '-X', 'utf8', str(REPO_CHECKS)],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('repository checks:', result.stdout)


class DraftPacketWarningTests(unittest.TestCase):
    """W16: a staged draft packet not named in the commit message is warned about.

    The measured failure: `git add -A` swept a Planner's in-progress draft into a
    commit (`141cb7e`). It is a warning rather than a refusal because the content is
    what the writer wrote -- the accident is hygiene, and making it visible at the
    moment it happens is when it is cheap to undo.
    """

    def setUp(self) -> None:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            'precommit_repo_checks', ROOT / 'scripts' / 'precommit-repo-checks.py')
        self.checks = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.checks)

    def test_a_draft_packet_is_detected(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'docs' / 'packets').mkdir(parents=True)
            packet = root / 'docs' / 'packets' / 'c1-slot-write.md'
            packet.write_text('## C1\n\n**Status:** draft\n\nbody\n', encoding='utf-8')
            saved = self.checks.ROOT
            self.checks.ROOT = root
            try:
                found = self.checks.staged_draft_packets(['docs/packets/c1-slot-write.md'])
            finally:
                self.checks.ROOT = saved
        self.assertEqual(found, ['docs/packets/c1-slot-write.md'])

    def test_an_adequate_packet_is_not_a_draft(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'docs' / 'packets').mkdir(parents=True)
            (root / 'docs' / 'packets' / 'frozen.md').write_text(
                '**Status:** ADEQUATE\n', encoding='utf-8')
            saved = self.checks.ROOT
            self.checks.ROOT = root
            try:
                found = self.checks.staged_draft_packets(['docs/packets/frozen.md'])
            finally:
                self.checks.ROOT = saved
        self.assertEqual(found, [])

    def test_an_inadequate_packet_is_a_draft(self) -> None:
        """`INADEQUATE` is also a not-yet-frozen state (§5.2)."""
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'docs' / 'packets').mkdir(parents=True)
            (root / 'docs' / 'packets' / 'wip.md').write_text(
                '**Status:** INADEQUATE\n', encoding='utf-8')
            saved = self.checks.ROOT
            self.checks.ROOT = root
            try:
                found = self.checks.staged_draft_packets(['docs/packets/wip.md'])
            finally:
                self.checks.ROOT = saved
        self.assertEqual(found, ['docs/packets/wip.md'])

    def test_a_non_packet_path_is_not_checked(self) -> None:
        self.assertEqual(self.checks.staged_draft_packets(['docs/reviews/x.md']), [])
        self.assertEqual(self.checks.staged_draft_packets(['src/main.c']), [])

    def test_the_real_tree_has_no_staged_drafts(self) -> None:
        """The delivered tree must not be committing a draft right now.

        This is the property the hook actually enforces: not "no draft exists in the
        repository" -- `docs/packets/p0-acceptance-contract.md` legitimately says
        "Draft for plan review", and a draft may sit in the tree indefinitely -- but
        "no draft is being staged in THIS commit without being named". Staging is
        what the hook sees, so staging is what this control exercises.
        """
        staged = subprocess.run(
            ['git', 'diff', '--cached', '--name-only'], cwd=str(ROOT),
            capture_output=True, text=True)
        if staged.returncode != 0:
            self.skipTest('not a git working tree')
        paths = [line.replace('\\', '/') for line in staged.stdout.splitlines()
                 if line.strip()]
        drafts = self.checks.staged_draft_packets(paths)
        message = self.checks.commit_message()
        unnamed = [d for d in drafts if Path(d).stem not in message]
        self.assertEqual(unnamed, [],
                         f'draft packet(s) staged and not named in the commit '
                         f'message: {unnamed}')

    def test_a_draft_named_in_the_message_is_not_warned_about(self) -> None:
        """An intentional draft commit must not be nagged.

        Without this, a check that warned about every draft would pass the
        unnamed-draft control while making the warning useless.
        """
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'docs' / 'packets').mkdir(parents=True)
            (root / '.git').mkdir()
            (root / 'docs' / 'packets' / 'c1.md').write_text(
                '**Status:** draft\n', encoding='utf-8')
            (root / '.git' / 'COMMIT_EDITMSG').write_text(
                'c1: promote the slot-write discovery packet\n', encoding='utf-8')
            saved = self.checks.ROOT
            self.checks.ROOT = root
            try:
                drafts = self.checks.staged_draft_packets(['docs/packets/c1.md'])
                message = self.checks.commit_message()
            finally:
                self.checks.ROOT = saved
        self.assertEqual(drafts, ['docs/packets/c1.md'])
        self.assertIn(Path(drafts[0]).stem, message)


class ConfigTests(unittest.TestCase):
    def test_config_wires_every_hook(self) -> None:
        """A hook the config does not name is a hook that never runs."""
        config = (ROOT / '.pre-commit-config.yaml').read_text(encoding='utf-8')
        for hook in ('staged-paths', 'repo-checks', 'merge-gate'):
            self.assertIn(f'id: {hook}', config)
        for script in ('precommit-staged-paths.py', 'precommit-repo-checks.py',
                       'precommit-merge-gate.py'):
            self.assertTrue((ROOT / 'scripts' / script).is_file(),
                            f'{script} is named by the config but does not exist')
            self.assertIn(script, config)


if __name__ == '__main__':
    unittest.main(verbosity=2)
