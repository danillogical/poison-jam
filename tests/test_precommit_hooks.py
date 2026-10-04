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

    # One staged blob per retail-content signature (plan T17). Built from bytes
    # here, never stored, so this file carries none of them.
    RETAIL_SAMPLES = {
        'default.bin': (b'XBEH' + b'\0' * 60, 'XBE header'),
        'partition.img': (b'FATX' + b'\0' * 60, 'FATX'),
        'process.bin': (b'MDMP' + b'\0' * 60, 'minidump'),
        'bundle.dat': (b'PK\x03\x04' + b'\0' * 60, 'zip archive'),
        'bundle.7': (b'7z\xbc\xaf\x27\x1c' + b'\0' * 60, '7-Zip archive'),
        'bundle.r': (b'Rar!\x1a\x07\x00' + b'\0' * 60, 'RAR archive'),
        'stream.g': (b'\x1f\x8b\x08' + b'\0' * 60, 'gzip stream'),
        'stream.x': (b'\xfd7zXZ\x00' + b'\0' * 60, 'xz stream'),
        'stream.z': (b'\x28\xb5\x2f\xfd' + b'\0' * 60, 'zstd stream'),
        'stream.b': (b'BZh9' + b'1AY&SY' + b'\0' * 60, 'bzip2 stream'),
        'bundle.t': (b'\0' * 257 + b'ustar' + b'\0' * 60, 'tar archive'),
        'disc.iso': (b'\0' * 64 + b'MICROSOFT*' + b'XBOX*MEDIA' + b'\0' * 64,
                     'Xbox disc image'),
        'ram.bin': (b'\0' * 64 + bytes.fromhex('8b512c85d28b4130c70190431c00741c'),
                    ".text bytes"),
        'blob.bin': (b'\0' * 64 + b'XBEH' + b'\0' * 64, 'embedded XBE header'),
    }

    def test_retail_content_is_refused_by_signature(self) -> None:
        """T17: each signature is refused whatever the file is called."""
        for name, (data, what) in self.RETAIL_SAMPLES.items():
            with self.subTest(name=name), ScratchRepo() as repo:
                (repo / name).write_bytes(data)
                git(repo, 'add', name)
                result = run_hook(STAGED_PATHS, repo)
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertIn(what, result.stdout)
                self.assertIn(name, result.stdout)

    def test_prose_about_retail_formats_is_allowed(self) -> None:
        """Known good: documents name these formats and quote the control hex."""
        with ScratchRepo() as repo:
            (repo / 'notes.md').write_text(
                'The XBEH magic opens an XBE. Control read: '
                '8b512c85d28b4130c70190431c00741c\n', encoding='utf-8')
            git(repo, 'add', 'notes.md')
            result = run_hook(STAGED_PATHS, repo)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('no retail content', result.stdout)

    def test_the_gate_carries_no_signature_it_refuses(self) -> None:
        """The gate's own sources must pass it, or the first commit is refused."""
        sys.path.insert(0, str(ROOT / 'scripts'))
        import importlib.util
        spec = importlib.util.spec_from_file_location('staged_paths', STAGED_PATHS)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for relative in ('scripts/precommit-staged-paths.py',
                         'tests/test_precommit_hooks.py'):
            data = (ROOT / relative).read_bytes()
            self.assertEqual(module.content_findings(relative, data), [], relative)

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
