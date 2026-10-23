#!/usr/bin/env python3
"""Focused regression for the BUMPENVMAP controls harness's --workdir ownership.

Drives the real ``scripts/bumpenv-q-controls.py`` entry (importlib + ``main``)
with only the expensive boundary stubbed: ``copy_toolkit`` raises
``StopBeforeCopy`` (or, in the one "success" case, copies just the three
production files instead of the whole toolkit), and cmake/ctest are never
invoked.  No compile runs, no build is configured and no control verdict is
claimed by this file -- it decides only WHERE the harness may write and remove.

The scratch trees these cases offer the helper are created by the test itself
in a fresh sibling of the game checkout, NOT under %TEMP%: the harness refuses
%TEMP% for MSB8029 reasons before it ever looks at a child path, and that
earlier refusal would hide the defect under test.  Each one is removed by this
file's own ``finally``.  The "repository root" cases use a disposable
git-shaped directory, never the game or the toolkit: no test may put a real
tree in front of a helper that could delete it.  The real game root and one of
its ancestors are checked too, but only for a helper that exposes the
ownership API.

Set ``BUMPENV_CONTROLS_HELPER`` to another copy of the helper to run the same
checks against it; the historical red control runs this file against the
pre-repair helper, whose ``run_control`` deletes a pre-existing
``toolkit-<control>/`` inside the caller's ``--workdir``.
"""

from __future__ import annotations

import contextlib
import importlib.util
import inspect
import os
import pathlib
import secrets
import shutil
import sys
import tempfile
import types
import unittest

HERE = pathlib.Path(__file__).resolve().parent
GAME = HERE.parent
HELPER = pathlib.Path(
    os.environ.get("BUMPENV_CONTROLS_HELPER",
                   GAME / "scripts" / "bumpenv-q-controls.py")
).resolve()

MARKER_NAME = ".bumpenv-q-controls-owned"
TOOLKIT = (GAME / ".." / "xboxrecomp").resolve()


class StopBeforeCopy(Exception):
    """The bounded boundary: raising here proves no copy/configure/build ran."""


@contextlib.contextmanager
def scratch_root():
    """A disposable directory outside both repos and outside %TEMP%."""
    root = GAME.parent / ("tmp-bumpenv-controls-selftest-" + secrets.token_hex(6))
    root.mkdir(parents=True, exist_ok=False)
    try:
        yield root
    finally:
        shutil.rmtree(root, ignore_errors=True)


def load_helper(path):
    spec = importlib.util.spec_from_file_location(
        "bumpenv_controls_workdir_under_test", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # The helper derives GAME/TOOLKIT from its own location; when this file is
    # pointed at a copy elsewhere, keep the real repository identities so the
    # same checks apply to the same trees.
    module.GAME = GAME
    module.TOOLKIT = TOOLKIT
    return module


def invoke(module, argv):
    """Call the helper's main; the pre-repair helper takes no argv."""
    if "argv" in inspect.signature(module.main).parameters:
        return module.main(list(argv))
    saved = sys.argv
    sys.argv = [str(HELPER), *argv]
    try:
        return module.main()
    finally:
        sys.argv = saved


class WorkdirOwnershipTest(unittest.TestCase):
    def bounded_helper(self, calls=None):
        """Load the helper with only the toolkit-copy boundary stubbed."""
        module = load_helper(HELPER)
        calls = [] if calls is None else calls

        def copy_stub(dest):
            dest = pathlib.Path(dest)
            calls.append((dest, (dest.parent / MARKER_NAME).is_file()))
            raise StopBeforeCopy("stubbed at the toolkit copy boundary")

        module.copy_toolkit = copy_stub
        return module, calls

    @staticmethod
    def argv(work):
        return ["--control", "bind-guard", "--workdir", str(work)]

    @staticmethod
    def marker(module, root):
        return root / getattr(module, "OWNERSHIP_MARKER", MARKER_NAME)

    def test_preexisting_directory_is_refused_before_copy(self):
        with scratch_root() as root:
            module, calls = self.bounded_helper()
            existing = root / "preexisting"
            existing.mkdir()
            sentinel = existing / "sentinel.txt"
            sentinel.write_text("caller-owned", encoding="utf-8")

            rc = invoke(module, self.argv(existing))

            self.assertEqual(rc, 2, "an existing --workdir must be refused")
            self.assertTrue(existing.is_dir(), "the caller's directory must survive")
            self.assertTrue(sentinel.is_file(), "the sentinel must survive")
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "caller-owned")
            self.assertEqual(calls, [], "the copy boundary must not be reached")
            self.assertFalse(self.marker(module, existing).exists(),
                             "the helper must not claim a caller's directory")

    def test_preexisting_control_child_survives_refusal(self):
        """The R4 defect: a pre-existing toolkit-<control>/ was rmtree'd."""
        with scratch_root() as root:
            module, calls = self.bounded_helper()
            existing = root / "preexisting"
            existing.mkdir()
            child = existing / "toolkit-bind-guard"
            child.mkdir()
            sentinel = child / "preexisting-unrelated-sentinel.txt"
            sentinel.write_text("not mine\n", encoding="utf-8")

            try:
                rc = invoke(module, self.argv(existing))
            except StopBeforeCopy:
                rc = "reached the copy boundary after deleting the caller's child"

            self.assertEqual(
                rc, 2,
                f"an existing --workdir must be refused before the copy boundary "
                f"(the caller's child still present: {child.is_dir()})")
            self.assertTrue(child.is_dir(),
                            "a pre-existing toolkit-<control>/ must not be removed")
            self.assertTrue(sentinel.is_file(),
                            "the unrelated sentinel must survive")
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "not mine\n")
            self.assertEqual(calls, [], "the copy boundary must not be reached")
            self.assertFalse(self.marker(module, existing).exists(),
                             "the helper must not claim a caller's directory")

    def test_preexisting_file_is_refused_and_preserved(self):
        with scratch_root() as root:
            module, calls = self.bounded_helper()
            existing = root / "preexisting.txt"
            existing.write_text("caller-owned", encoding="utf-8")

            rc = invoke(module, self.argv(existing))

            self.assertEqual(rc, 2, "an existing --workdir file must be refused")
            self.assertTrue(existing.is_file())
            self.assertEqual(existing.read_text(encoding="utf-8"), "caller-owned")
            self.assertEqual(calls, [], "the copy boundary must not be reached")

    def test_repository_root_is_refused_and_preserved(self):
        with scratch_root() as root:
            module, calls = self.bounded_helper()
            # A disposable repository root: the pre-repair helper would delete
            # whatever it is handed, so a real tree is never offered.
            repo = root / "disposable-repo"
            (repo / ".git").mkdir(parents=True)
            (repo / "CMakeLists.txt").write_text(
                "project(disposable)\n", encoding="utf-8")
            (repo / "game.c").write_text(
                "int main(void) { return 0; }\n", encoding="utf-8")

            rc = invoke(module, self.argv(repo))

            self.assertEqual(rc, 2, "a repository root must be refused")
            self.assertTrue((repo / ".git").is_dir())
            self.assertTrue((repo / "CMakeLists.txt").is_file())
            self.assertTrue((repo / "game.c").is_file())
            self.assertEqual(calls, [], "nothing may reach the copy boundary")

    def test_real_game_root_and_ancestor_are_refused(self):
        if not hasattr(load_helper(HELPER), "OwnedWorkArea"):
            self.skipTest("pre-repair helper would delete --workdir; not exercised")
        with scratch_root():
            module, calls = self.bounded_helper()

            for guarded, label in ((GAME, "the real game root"),
                                   (GAME.parent, "an ancestor of the game root")):
                with self.subTest(guarded=str(guarded)):
                    rc = invoke(module, self.argv(guarded))
                    self.assertEqual(rc, 2, f"{label} must be refused")
                    self.assertTrue((GAME / "CMakeLists.txt").is_file(),
                                    "the real game root must be unmodified")
                    self.assertTrue((GAME / "scripts" /
                                     "bumpenv-q-controls.py").is_file())
                    self.assertEqual(calls, [],
                                     "nothing may reach the copy boundary")

    def test_new_path_inside_the_repo_is_refused_before_mkdir(self):
        module, calls = self.bounded_helper()
        inside = GAME / ("tmp-bumpenv-controls-selftest-"
                         + secrets.token_hex(6))

        rc = invoke(module, self.argv(inside))

        self.assertEqual(rc, 2, "a path inside the repo must be refused")
        self.assertFalse(inside.exists(), "nothing may be created inside the repo")
        self.assertEqual(calls, [], "nothing may reach the copy boundary")

    def test_workdir_under_temp_is_refused(self):
        module, calls = self.bounded_helper()
        inside = (pathlib.Path(tempfile.gettempdir())
                  / ("tmp-bumpenv-controls-selftest-" + secrets.token_hex(6)))

        rc = invoke(module, self.argv(inside))

        self.assertEqual(rc, 2, "a --workdir under %TEMP% must be refused")
        self.assertFalse(inside.exists(), "nothing may be created under %TEMP%")
        self.assertEqual(calls, [], "nothing may reach the copy boundary")

    def test_changed_ownership_prevents_cleanup(self):
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        with scratch_root() as root:
            args = types.SimpleNamespace(workdir=str(root / "owned"), keep=False)

            owned = module.acquire_work_area(args, GAME, TOOLKIT)
            self.assertTrue(owned.path.is_dir())
            marker = self.marker(module, owned.path)
            self.assertTrue(marker.is_file())

            marker.write_text("foreign owner\ntoken deadbeef\n", encoding="utf-8")
            self.assertFalse(owned.release(False),
                             "release must refuse a marker it did not write")
            self.assertTrue(owned.path.is_dir(),
                            "the root must survive a failed ownership check")

            marker.unlink()
            self.assertFalse(owned.release(False),
                             "release must refuse a root whose marker is gone")
            self.assertTrue(owned.path.is_dir())

    def test_success_removes_only_the_owned_root(self):
        """Mocked runner: one control "succeeds" with no process at all."""
        with scratch_root() as root:
            module = load_helper(HELPER)

            owned_paths = []
            phase = {"n": 0}

            def copy_stub(dest):
                dest = pathlib.Path(dest)
                owned_paths.append(dest)
                dest.mkdir(parents=True)
                for rel in module.PRODUCTION:
                    target = dest / rel
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(TOOLKIT / rel, target)

            module.copy_toolkit = copy_stub
            module.configure_private_build = lambda build, copy: (0, "")
            module.build_targets = lambda build, targets: (0, "")

            def hashes(build, targets=module.TARGETS):
                phase["n"] += 1
                return {t: f"hash-{phase['n']}-{t}" for t in targets}

            module.exe_hashes = hashes
            module.ctest_case = lambda build, name, env: (0, "")
            module.expect_red = lambda build, case, markers, env, prefix: True
            module.expect_green = lambda build, case, env, prefix: True

            sibling = root / "sibling"
            sibling.mkdir()
            sibling_sentinel = sibling / "sentinel.txt"
            sibling_sentinel.write_text("not mine", encoding="utf-8")
            owned_root = root / "owned"

            rc = invoke(module, ["--control", "bind-guard",
                                 "--workdir", str(owned_root)])

            self.assertEqual(rc, 0, "the stubbed control path must report PASS")
            self.assertTrue(owned_paths, "the copy boundary was reached")
            self.assertEqual(owned_paths[0], owned_root / "toolkit-bind-guard")
            self.assertFalse(owned_root.exists(),
                             "the owned root must be removed after a clean run")
            self.assertTrue(sibling_sentinel.is_file(),
                            "a sibling outside the owned root must survive")
            self.assertEqual(sibling_sentinel.read_text(encoding="utf-8"),
                             "not mine")

    def test_keep_leaves_the_owned_root_and_marker(self):
        with scratch_root() as root:
            module, _calls = self.bounded_helper()
            owned_root = root / "kept"

            with self.assertRaises(StopBeforeCopy):
                invoke(module, self.argv(owned_root) + ["--keep"])

            self.assertTrue(owned_root.is_dir(), "--keep must leave the owned root")
            marker = self.marker(module, owned_root)
            self.assertTrue(marker.is_file(), "--keep must leave the marker")
            self.assertIn("token", marker.read_text(encoding="utf-8"))
            # scratch_root() removes this tree when the case ends.

    def test_default_root_is_a_fresh_sibling_outside_the_repos(self):
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        args = types.SimpleNamespace(workdir=None, keep=False)
        owned = module.acquire_work_area(args, GAME, TOOLKIT)
        try:
            self.assertEqual(owned.path.parent, GAME.parent,
                             "the default root is a sibling of the game checkout")
            self.assertNotEqual(owned.path, GAME)
            self.assertNotEqual(owned.path, TOOLKIT)
            self.assertFalse(owned.path.is_relative_to(GAME))
            self.assertFalse(owned.path.is_relative_to(TOOLKIT))
            self.assertFalse(owned.path.is_relative_to(
                pathlib.Path(tempfile.gettempdir()).resolve()),
                "the default root must stay out of %TEMP% (MSB8029)")
            self.assertTrue(owned.release(False), "a fresh root is still owned")
            self.assertFalse(owned.path.exists(), "release must remove it")
        finally:
            if owned.path.exists():
                shutil.rmtree(owned.path, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
