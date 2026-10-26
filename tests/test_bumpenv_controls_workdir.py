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
import io
import os
import pathlib
import secrets
import shutil
import subprocess
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
sys.path.insert(0, str(GAME / 'scripts'))
from toolkit_path import toolkit_path  # noqa: E402
TOOLKIT = toolkit_path()


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

    # ── force_rebuild: the ownership proof must hold before ANY removal ─────
    #
    # force_rebuild recursively removes each focused `*.dir`, the focused
    # executables and the generated SPIR-V, so it needs the same proof final
    # root release uses. These cases hand it disposable owned trees only.

    @staticmethod
    def force_rebuild_error(module, build, owned):
        """Call force_rebuild; return the exception it raised, or None."""
        try:
            module.force_rebuild(build, owned=owned)
        except Exception as exc:  # the exception TYPE is the assertion
            return exc
        return None

    @staticmethod
    def owned_area(module, root, name="owned"):
        """A fresh, stamped, disposable owned work area."""
        args = types.SimpleNamespace(workdir=str(root / name), keep=False)
        return module.acquire_work_area(args, GAME, TOOLKIT)

    @staticmethod
    def focused_tree(build, targets, label):
        """The real deletion targets: `*.dir` trees, exes and SPIR-V."""
        trees = {}
        for t in targets:
            tree = build / f"{t}.dir"
            (tree / "nested").mkdir(parents=True, exist_ok=True)
            (tree / "object.obj").write_text(f"{label} {t}\n", encoding="utf-8")
            (tree / "nested" / "object.obj").write_text(
                f"{label} {t}\n", encoding="utf-8")
            (tree / "sentinel.txt").write_text(
                f"{label} {t}\n", encoding="utf-8")
            (build / "Release").mkdir(parents=True, exist_ok=True)
            (build / "Release" / f"{t}.exe").write_text(
                f"{label} {t}\n", encoding="utf-8")
            trees[t] = tree
        spirv = build / "xboxrecomp" / "src" / "video" / "spirv"
        spirv.mkdir(parents=True, exist_ok=True)
        for name in ("xbr_nv2a_vk_rc_vs.spv", "xbr_nv2a_vk_rc_ps.spv"):
            (spirv / name).write_text(f"{label} {name}\n", encoding="utf-8")
        return trees

    def test_force_rebuild_refuses_a_replaced_owned_root(self):
        """The defect: contains() stayed true after the root was replaced."""
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        with scratch_root() as root:
            owned = self.owned_area(module, root)
            build = owned.path / "build-bind-guard"
            self.focused_tree(build, module.TARGETS, "original")
            original_dir = build / f"{module.TARGETS[0]}.dir"
            self.assertTrue((original_dir / "sentinel.txt").is_file())

            # Move the genuinely owned root aside and put a DIFFERENT directory
            # at the same pathname, holding a foreign marker and its own tree.
            moved = root / "moved-aside"
            owned.path.rename(moved)
            replacement = owned.path
            replacement.mkdir()
            (replacement / MARKER_NAME).write_text(
                "foreign owner\ntoken deadbeef\n", encoding="utf-8")
            replacement_build = replacement / "build-bind-guard"
            replacement_trees = self.focused_tree(
                replacement_build, module.TARGETS, "replacement")
            sentinel = replacement_trees[module.TARGETS[0]] / "sentinel.txt"
            exe = (replacement_build / "Release"
                   / f"{module.TARGETS[0]}.exe")
            self.assertFalse(owned._still_owned(),
                             "a replaced root is no longer owned")

            error = self.force_rebuild_error(module, replacement_build, owned)
            survived = (sentinel.is_file(), exe.is_file(),
                        replacement_trees[module.TARGETS[0]].is_dir())
            self.assertIsInstance(
                error, module.HarnessError,
                f"force_rebuild must refuse a replaced root with a named "
                f"HarnessError, not delete the replacement's files; raised "
                f"{error!r}; replacement sentinel/exe/dir present: {survived}")
            text = str(error)
            self.assertIn(str(owned.path), text,
                          "the refusal must name the work area root")
            self.assertTrue(
                "ownership" in text or "no longer" in text
                or "owned work area" in text,
                f"the refusal must name the ownership failure: {text}")
            self.assertTrue(sentinel.is_file(),
                            "the replacement's sentinel must survive")
            self.assertTrue(exe.is_file(),
                            "the replacement's executable must survive")
            self.assertTrue(replacement_trees[module.TARGETS[0]].is_dir(),
                            "the replacement's object tree must survive")
            self.assertTrue(moved.is_dir(),
                            "the moved-aside original must be untouched")

    def test_force_rebuild_without_an_ownership_proof_is_refused(self):
        """owned=None must not be a bypass: no proof, no deletion."""
        module = load_helper(HELPER)
        with scratch_root() as root:
            build = root / "unproven" / "build-bind-guard"
            trees = self.focused_tree(build, module.TARGETS, "unproven")
            sentinel = trees[module.TARGETS[0]] / "sentinel.txt"
            exe = build / "Release" / f"{module.TARGETS[0]}.exe"

            error = self.force_rebuild_error(module, build, None)
            survived = (sentinel.is_file(), exe.is_file(),
                        trees[module.TARGETS[0]].is_dir())
            self.assertIsInstance(
                error, module.HarnessError,
                f"force_rebuild must refuse to clean without an ownership "
                f"proof, not delete the targets; raised {error!r}; "
                f"sentinel/exe/dir present: {survived}")
            text = str(error)
            self.assertTrue(
                "no owned work area" in text or "ownership proof" in text,
                f"the refusal must name the missing ownership proof: {text}")
            self.assertIn("nothing was removed", text)
            self.assertTrue(sentinel.is_file(), "nothing may be removed")
            self.assertTrue(exe.is_file(), "nothing may be removed")
            self.assertTrue(trees[module.TARGETS[0]].is_dir(),
                            "nothing may be removed")

    def test_force_rebuild_refuses_a_redirected_deletion_target(self):
        """A `*.dir` junction/symlink to a foreign directory must be refused."""
        module = load_helper(HELPER)
        target = module.TARGETS[0]
        with scratch_root() as root:
            owned = self.owned_area(module, root)
            build = owned.path / "build-bind-guard"
            build.mkdir(parents=True)
            foreign = root / "foreign"
            foreign.mkdir()
            sentinel = foreign / "sentinel.txt"
            sentinel.write_text("foreign data\n", encoding="utf-8")
            link = build / f"{target}.dir"
            if os.name == "nt":
                created = subprocess.run(
                    ["cmd", "/c", "mklink", "/J", str(link), str(foreign)],
                    capture_output=True, text=True)
                if created.returncode != 0:
                    self.skipTest(
                        "this host cannot create a junction: "
                        + (created.stderr or created.stdout).strip())
            else:
                try:
                    os.symlink(foreign, link, target_is_directory=True)
                except (OSError, NotImplementedError) as exc:
                    self.skipTest(f"this host cannot create a directory "
                                  f"symlink: {exc}")
            # A real second target the call must not reach either.
            other = self.focused_tree(
                build, [module.TARGETS[1]], "redirect")
            other_tree = other[module.TARGETS[1]]

            error = self.force_rebuild_error(module, build, owned)
            followed = sorted(p.name for p in foreign.iterdir())
            self.assertIsInstance(
                error, module.HarnessError,
                f"force_rebuild must refuse a redirected `{target}.dir` with a "
                f"named HarnessError, not follow it; raised {error!r}; foreign "
                f"contents: {followed}; sentinel present: {sentinel.is_file()}")
            self.assertIn(str(link), str(error),
                          "the refusal must name the offending target")
            self.assertTrue(sentinel.is_file(),
                            "the foreign sentinel must survive")
            self.assertEqual(sentinel.read_text(encoding="utf-8"),
                             "foreign data\n",
                             "the foreign directory must not be modified")
            self.assertTrue(link.is_symlink()
                            or getattr(link, "is_junction", lambda: False)(),
                            "the link itself must be left in place")
            self.assertEqual(followed, ["sentinel.txt"],
                             "the foreign directory must be left exactly as it "
                             "was: no target may be followed into it")
            self.assertTrue(other_tree.is_dir(),
                            "a refusal must happen before the first removal")

    def test_force_rebuild_removes_only_the_owned_focused_targets(self):
        """The normal fresh-owned path must keep working, exactly as before."""
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        with scratch_root() as root:
            owned = self.owned_area(module, root)
            build = owned.path / "build-bind-guard"
            trees = self.focused_tree(build, module.TARGETS, "fresh")
            spirv = build / "xboxrecomp" / "src" / "video" / "spirv"
            sibling = root / "sibling"
            sibling.mkdir()
            sibling_sentinel = sibling / "sentinel.txt"
            sibling_sentinel.write_text("not mine\n", encoding="utf-8")

            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                result = module.force_rebuild(build, owned=owned)

            self.assertIsNone(result, "the fresh-owned call returns normally")
            for t in module.TARGETS:
                self.assertFalse(trees[t].exists(),
                                 f"{t}.dir must be removed recursively")
                self.assertFalse((build / "Release" / f"{t}.exe").exists(),
                                 f"{t}.exe must be removed")
            self.assertEqual(sorted(p.name for p in spirv.iterdir()), [],
                             "the generated SPIR-V must be removed")
            printed = out.getvalue()
            self.assertIn("forced recompile: removed", printed,
                          "the existing printout must be preserved")
            for t in module.TARGETS:
                self.assertIn(f"{t}.dir/", printed)
                self.assertIn(f"{t}.exe", printed)
            self.assertIn("xbr_nv2a_vk_rc_vs.spv", printed)
            self.assertTrue(sibling_sentinel.is_file(),
                            "a file outside the owned root must survive")
            self.assertEqual(sibling_sentinel.read_text(encoding="utf-8"),
                             "not mine\n")

    # ── acquire_work_area: a failed stamp must not remove a foreign root ────
    #
    # stamp() records the identity before it writes the marker, so a marker
    # write that fails still leaves a usable proof. The failure path must use
    # it instead of an unconditional rmtree.

    @staticmethod
    def failing_stamp(on_fail=None):
        """A stamp() that records the real identity, then fails."""
        def stamp(self):
            st = self.path.stat()
            self.identity = (st.st_dev, st.st_ino)
            if on_fail is not None:
                on_fail(self)
            raise OSError("stamp failed")

        return stamp

    def acquire_refused(self, module, work):
        """Call acquire_work_area; return (exception, stderr text)."""
        args = types.SimpleNamespace(workdir=str(work), keep=False)
        err = io.StringIO()
        raised = None
        with contextlib.redirect_stderr(err):
            try:
                module.acquire_work_area(args, GAME, TOOLKIT)
            except Exception as exc:  # the exception TYPE is the assertion
                raised = exc
        return raised, err.getvalue()

    def test_stamp_failure_with_intact_identity_removes_the_fresh_root(self):
        """A root this invocation created and still owns is not leaked."""
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        with scratch_root() as root:
            work = root / "owned"
            module.OwnedWorkArea.stamp = self.failing_stamp()

            raised, err = self.acquire_refused(module, work)

            self.assertIsInstance(
                raised, module.WorkAreaRefused,
                f"a failed stamp must be refused as a work-area error, not "
                f"leak a scratch root; raised {raised!r}")
            self.assertIn(str(work), str(raised),
                          "the refusal must name the root")
            self.assertIn("could not record ownership", str(raised))
            self.assertFalse(work.exists(),
                             "a still-owned fresh root must not be leaked")
            self.assertNotIn("NOT removed", err,
                             "an owned root is removed, not reported as kept")

    def test_stamp_failure_with_replaced_identity_preserves_the_replacement(
            self):
        """The same defect class as force_rebuild: a replacement is not ours."""
        module = load_helper(HELPER)
        self.assertTrue(hasattr(module, "OwnedWorkArea"),
                        "the helper must expose the ownership API")
        with scratch_root() as root:
            work = root / "owned"
            moved = root / "moved-aside"
            sentinel = root / "foreign-sentinel.txt"

            def replace_then_fail(owned):
                owned.path.rename(moved)
                owned.path.mkdir()
                sentinel.write_text("foreign data\n", encoding="utf-8")

            module.OwnedWorkArea.stamp = self.failing_stamp(
                replace_then_fail)

            raised, err = self.acquire_refused(module, work)

            survived = (work.is_dir(), sentinel.is_file(), moved.is_dir())
            self.assertIsInstance(
                raised, module.WorkAreaRefused,
                f"a failed stamp must still be refused; raised {raised!r}; "
                f"replacement/sentinel/moved present: {survived}")
            self.assertIn(str(work), str(raised),
                          "the refusal must name the root")
            self.assertTrue(work.is_dir(),
                            "a replacement directory at the same pathname must "
                            "not be removed")
            self.assertTrue(sentinel.is_file(),
                            "the foreign sentinel must survive")
            self.assertEqual(sentinel.read_text(encoding="utf-8"),
                             "foreign data\n")
            self.assertTrue(moved.is_dir(),
                            "the moved-aside original must be untouched")
            self.assertIn("NOT removed", err,
                          "the diagnostic must say the root was left in place")
            self.assertIn(str(work), err,
                          "the diagnostic must name the root it left in place")


if __name__ == "__main__":
    unittest.main(verbosity=2)
