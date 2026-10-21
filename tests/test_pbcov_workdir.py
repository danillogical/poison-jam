#!/usr/bin/env python3
"""Focused regression for the mutation harness's --work ownership policy.

Drives the real ``scripts/pbcov-mutants.py`` entry (importlib + ``main``) with
the git/toolkit-copy boundary stubbed, so no git walk, copy, configure or build
runs.  Set ``PBCOV_MUTANTS_HELPER`` to another copy of the helper to run the
same checks against it; the historical red control runs this file against
576fa7a's helper.

The "repository root" case uses a disposable git-shaped directory, never the
game or the toolkit: the pre-repair helper deletes whatever ``--work`` path it
is given, and no test may put a real tree in front of that.  The real game root
is additionally checked only for a helper that exposes the ownership API (the
pre-repair helper would delete it).
"""

from __future__ import annotations

import importlib.util
import os
import pathlib
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
GAME = HERE.parent
HELPER = pathlib.Path(
    os.environ.get("PBCOV_MUTANTS_HELPER", GAME / "scripts" / "pbcov-mutants.py")
).resolve()

MARKER_NAME = ".pbcov-mutants-owned"


class StopBeforeCopy(Exception):
    """The bounded boundary: raising here proves no copy/configure ran."""


def load_helper(path):
    spec = importlib.util.spec_from_file_location(
        "pbcov_mutants_workdir_under_test", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class WorkdirOwnershipTest(unittest.TestCase):
    def bounded_helper(self, root):
        """Load the helper with only the expensive/VCS boundary stubbed."""
        module = load_helper(HELPER)
        calls = []

        module.checkout_state = lambda _toolkit: ("test-head", "")

        def copy_stub(src, dst):
            dst = pathlib.Path(dst)
            calls.append((pathlib.Path(src), dst,
                          (dst.parent / MARKER_NAME).is_file()))
            raise StopBeforeCopy("stubbed at the toolkit copy boundary")

        module.copy_working_tree = copy_stub

        # A toolkit shaped just enough for the pre-copy validation: the
        # CMakeLists.txt check and the PROTECTED hashes.
        toolkit = root / "fake-toolkit"
        toolkit.mkdir()
        (toolkit / "CMakeLists.txt").write_text("project(fake)\n", encoding="utf-8")
        for rel in module.PROTECTED:
            target = toolkit / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(f"/* {rel} */\n", encoding="utf-8")
        return module, toolkit, calls

    @staticmethod
    def argv(toolkit, work):
        return ["--toolkit", str(toolkit), "--expect-commit", "",
                "--no-vk", "--work", str(work)]

    @staticmethod
    def marker(module, root):
        return root / getattr(module, "OWNERSHIP_MARKER", MARKER_NAME)

    def test_preexisting_directory_is_refused_before_copy(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            module, toolkit, calls = self.bounded_helper(root)
            existing = root / "preexisting"
            existing.mkdir()
            sentinel = existing / "sentinel.txt"
            sentinel.write_text("caller-owned", encoding="utf-8")

            rc = module.main(self.argv(toolkit, existing))

            self.assertEqual(rc, 2, "an existing --work path must be refused")
            self.assertTrue(sentinel.is_file(), "the sentinel must survive")
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "caller-owned")
            self.assertTrue(existing.is_dir(), "the caller's directory must survive")
            self.assertEqual(calls, [], "the copy boundary must not be reached")
            self.assertFalse(self.marker(module, existing).exists(),
                             "the helper must not claim a caller's directory")

    def test_preexisting_file_is_refused_and_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            module, toolkit, calls = self.bounded_helper(root)
            existing = root / "preexisting.txt"
            existing.write_text("caller-owned", encoding="utf-8")

            rc = module.main(self.argv(toolkit, existing))

            self.assertEqual(rc, 2, "an existing --work file must be refused")
            self.assertTrue(existing.is_file())
            self.assertEqual(existing.read_text(encoding="utf-8"), "caller-owned")
            self.assertEqual(calls, [], "the copy boundary must not be reached")

    def test_fresh_path_failure_removes_only_the_owned_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            module, toolkit, calls = self.bounded_helper(root)
            sibling = root / "sibling"
            sibling.mkdir()
            sibling_sentinel = sibling / "sentinel.txt"
            sibling_sentinel.write_text("not mine", encoding="utf-8")
            fresh = root / "fresh-work"

            with self.assertRaises(StopBeforeCopy):
                module.main(self.argv(toolkit, fresh))

            self.assertEqual(len(calls), 1, "the stubbed copy boundary was reached")
            self.assertEqual(calls[0][1], fresh / "toolkit-pristine")
            self.assertTrue(calls[0][2],
                            "the owned root must be created and marked before the copy")
            self.assertFalse(fresh.exists(),
                             "the owned root must be removed after the failure")
            self.assertTrue(sibling_sentinel.is_file(),
                            "a sibling outside the owned root must survive")
            self.assertEqual(sibling_sentinel.read_text(encoding="utf-8"),
                             "not mine")

    def test_keep_leaves_the_owned_root_and_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            module, toolkit, _calls = self.bounded_helper(root)
            fresh = root / "kept-work"

            with self.assertRaises(StopBeforeCopy):
                module.main(self.argv(toolkit, fresh) + ["--keep"])

            self.assertTrue(fresh.is_dir(), "--keep must leave the owned root")
            maker = self.marker(module, fresh)
            self.assertTrue(maker.is_file(), "--keep must leave the marker")
            self.assertIn("token", maker.read_text(encoding="utf-8"))
            # The test's own TemporaryDirectory removes this tree.

    def test_repository_root_is_refused_and_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            module, toolkit, calls = self.bounded_helper(root)
            # A disposable repository root: the pre-repair helper would delete
            # whatever it is handed, so a real tree is never offered.
            repo = root / "disposable-repo"
            (repo / ".git").mkdir(parents=True)
            (repo / "CMakeLists.txt").write_text(
                "project(disposable)\n", encoding="utf-8")
            (repo / "game.c").write_text(
                "int main(void) { return 0; }\n", encoding="utf-8")

            rc = module.main(self.argv(toolkit, repo))

            self.assertEqual(rc, 2, "a repository root must be refused")
            self.assertTrue((repo / ".git").is_dir())
            self.assertTrue((repo / "CMakeLists.txt").is_file())
            self.assertTrue((repo / "game.c").is_file())
            self.assertEqual(calls, [], "nothing may reach the copy boundary")

    def test_actual_game_root_is_refused_when_ownership_exists(self):
        if not hasattr(load_helper(HELPER), "OwnedWorkArea"):
            self.skipTest("pre-repair helper would delete --work; not exercised")
        with tempfile.TemporaryDirectory() as tmp:
            module, toolkit, calls = self.bounded_helper(pathlib.Path(tmp))

            rc = module.main(self.argv(toolkit, GAME))

            self.assertEqual(rc, 2, "the real game root must be refused")
            self.assertTrue((GAME / "CMakeLists.txt").is_file())
            self.assertTrue((GAME / "scripts" / "pbcov-mutants.py").is_file())
            self.assertEqual(calls, [], "nothing may reach the copy boundary")


if __name__ == "__main__":
    unittest.main(verbosity=2)
