"""Save isolation: a run's saves must land under its own private root, provably.

Three layers, each pinned here without a game run:

1. `save_folder_observation()` in scripts/run-jsrf.py (also used by
   key-script-run.py) requires the log to name the title's UDATA and TDATA folders
   exactly once each, both inside the run's save root.
2. `run-jsrf.py main()` names the private root as RECOMP_SAVE_DIR by default,
   records whether the value came from the `runner` or the `caller`, and exits 2
   when the runtime's report does not verify. Driven through the real `main()`
   with the process launches replaced by a stand-in that writes a log.
3. The refusal in src/main.c: with RECOMP_SAVE_DIR set, a translated UDATA or
   TDATA outside it prints `[SAVE] udata_layer=FAIL` and returns 2; unset (owner
   play) it only reports. The block is compiled from main.c's own text, with the
   path translator stubbed, so the decision under test is the shipped one.
"""
from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import ntpath
import os
import re
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
MAIN_C = ROOT / "src" / "main.c"


def _load_runner():
    sys.path.insert(0, str(SCRIPTS))
    try:
        spec = importlib.util.spec_from_file_location("run_jsrf_save_iso",
                                                      SCRIPTS / "run-jsrf.py")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    finally:
        sys.path.remove(str(SCRIPTS))
    return mod


run_jsrf = _load_runner()
observe = run_jsrf.save_folder_observation

R = "C:\\runs\\20261020-run\\save-root"


def log(udata=None, tdata=None, eol="\n", extra=()):
    lines = list(extra)
    if udata is not None:
        lines.append(f"[SAVE] udata_root={udata}")
    if tdata is not None:
        lines.append(f"[SAVE] tdata_root={tdata}")
    return eol.join(lines) + eol


U = R + "\\UDATA\\5345000a"
T = R + "\\TDATA\\5345000a"


class FolderObservationTest(unittest.TestCase):

    def test_both_inside_passes(self):
        self.assertEqual(observe(log(U, T), R), (U, T, True))

    def test_crlf_lines_pass(self):
        self.assertEqual(observe(log(U, T, eol="\r\n"), R), (U, T, True))

    def test_a_missing_udata_line_fails(self):
        self.assertEqual(observe(log(None, T), R), (None, T, False))

    def test_a_missing_tdata_line_fails(self):
        self.assertEqual(observe(log(U, None), R), (U, None, False))

    def test_no_lines_at_all_fails(self):
        self.assertEqual(observe("[SAVE] resolved_root=" + R + "\n", R),
                         (None, None, False))

    def test_a_duplicate_line_fails_even_when_both_copies_are_inside(self):
        for dup in ("udata", "tdata"):
            with self.subTest(dup):
                text = log(U, T) + (f"[SAVE] {dup}_root=" + (U if dup == "udata" else T)
                                    + "\n")
                u, t, ok = observe(text, R)
                self.assertFalse(ok)
                self.assertIsNone(u if dup == "udata" else t)

    def test_a_duplicate_with_a_different_value_fails(self):
        text = log(U, T) + "[SAVE] udata_root=C:\\game\\UDATA\\5345000a\n"
        self.assertFalse(observe(text, R)[2])

    def test_a_path_outside_the_root_fails(self):
        self.assertFalse(observe(log("C:\\game\\UDATA\\5345000a", T), R)[2])
        self.assertFalse(observe(log(U, "C:\\game\\TDATA\\5345000a"), R)[2])

    def test_one_inside_one_outside_fails(self):
        out = "D:\\elsewhere\\TDATA\\5345000a"
        self.assertEqual(observe(log(U, out), R), (U, out, False))

    def test_a_sibling_directory_sharing_the_prefix_fails(self):
        self.assertFalse(observe(log(R + "2\\UDATA\\x", R + "2\\TDATA\\x"), R)[2])
        self.assertFalse(observe(log(R + "-old\\UDATA\\x", T), R)[2])

    def test_dotdot_escaping_the_root_fails(self):
        self.assertFalse(observe(log(R + "\\..\\other\\UDATA", T), R)[2])

    def test_dotdot_that_stays_inside_passes(self):
        self.assertTrue(observe(log(R + "\\a\\..\\UDATA", T), R)[2])

    def test_case_differences_pass(self):
        self.assertTrue(observe(log(U.upper(), T.lower()), R)[2])
        self.assertTrue(observe(log(U, T), R.upper())[2])

    def test_forward_slashes_pass(self):
        self.assertTrue(observe(log(U.replace("\\", "/"), T), R)[2])

    def test_a_trailing_separator_on_the_root_or_the_paths_passes(self):
        self.assertTrue(observe(log(U, T), R + "\\")[2])
        self.assertTrue(observe(log(U + "\\", T + "\\"), R)[2])

    def test_the_root_itself_is_not_inside_it(self):
        self.assertFalse(observe(log(R, T), R)[2])
        self.assertFalse(observe(log(R + "\\", T), R)[2])

    def test_a_marker_that_is_not_at_the_start_of_a_line_is_ignored(self):
        text = "noise [SAVE] udata_root=" + U + "\n[SAVE] tdata_root=" + T + "\n"
        self.assertEqual(observe(text, R)[0], None)


# --- the runner's default / override recording, through the real main() ---------

class _Done:
    def __init__(self, rc=0):
        self.returncode = rc
        self.stdout = b""
        self.stderr = b""


class RunnerRecordingTest(unittest.TestCase):

    def setUp(self):
        self._env = dict(os.environ)
        self.addCleanup(self._restore_env)
        for k in list(os.environ):
            if k.startswith(("RECOMP_", "JSRF_")):
                del os.environ[k]
        self._tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self._tmp.cleanup)
        self.root = Path(self._tmp.name)
        (self.root / "game").mkdir()
        (self.root / "game" / "default.xbe").write_bytes(b"xbe")
        build = self.root / "build" / "Release"
        build.mkdir(parents=True)
        for name in run_jsrf.ARTIFACTS:
            (build / name).write_bytes(name.encode())
        self.launches = []

    def _restore_env(self):
        os.environ.clear()
        os.environ.update(self._env)

    def _run(self, log_for, label="saveiso", env=None):
        """Run main() with launches faked; log_for(save_root) is the runtime's stdout."""
        os.environ.update(env or {})
        owner = self
        mod = run_jsrf
        saved = {k: getattr(mod, k) for k in (
            "ROOT", "BUILD", "TOOLKIT", "project_game_running",
            "archive_project_state", "repository_identity", "source_entries")}
        saved_run, saved_popen = mod.subprocess.run, mod.subprocess.Popen

        class FakePopen:
            def __init__(self, command, cwd=None, creationflags=0):
                run_dir = Path(command[2])
                save_root = [c for c in command if c.startswith("--save-root=")][0][12:]
                owner.launches.append({
                    "command": command,
                    "env_save_dir": os.environ.get("RECOMP_SAVE_DIR"),
                    "save_root": save_root, "run_dir": run_dir})
                (run_dir / "jsrf_run.log").write_text(log_for(save_root),
                                                      encoding="utf-8")
                (run_dir / "result.json").write_text(json.dumps(
                    {"outcome": "normal_exit", "exit_code": 0}), encoding="utf-8")

            def wait(self, timeout=None):
                return 0

            def poll(self):
                return 0

            def kill(self):
                pass

        try:
            mod.ROOT, mod.BUILD, mod.TOOLKIT = (
                self.root, self.root / "build" / "Release", self.root / "tk")
            mod.project_game_running = lambda root: False
            mod.archive_project_state = lambda run_dir: False
            mod.repository_identity = lambda *a, **k: {"dirty": False}
            mod.source_entries = lambda: []
            mod.subprocess.run = lambda *a, **k: _Done()
            mod.subprocess.Popen = FakePopen
            old_argv = sys.argv
            sys.argv = ["run-jsrf.py", "--seconds", "1", "--label", label,
                        "--profile", "exploratory", "--skip-disk-gate"]
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    rc = mod.main()
            finally:
                sys.argv = old_argv
        finally:
            for k, v in saved.items():
                setattr(mod, k, v)
            mod.subprocess.run, mod.subprocess.Popen = saved_run, saved_popen
        launch = self.launches[-1]
        meta = json.loads((launch["run_dir"] / "metadata.json").read_text("utf-8"))
        result = json.loads((launch["run_dir"] / "result.json").read_text("utf-8"))
        return rc, meta["save_root"], result, launch

    @staticmethod
    def _good(save_root):
        return ("[CHECKPOINT] ms=1 tid=1 memory_ready\n[CHECKPOINT] ms=2 tid=1 guest_entry\n"
                f"[SAVE] resolved_root={save_root}\n[SAVE] path_layer_root={save_root}\n"
                f"[SAVE] udata_root={save_root}\\UDATA\\5345000a\n"
                f"[SAVE] tdata_root={save_root}\\TDATA\\5345000a\n")

    def test_by_default_the_runner_names_the_private_root_as_the_save_dir(self):
        rc, rec, result, launch = self._run(self._good)
        self.assertEqual(rc, 0, result)
        self.assertEqual(launch["env_save_dir"], launch["save_root"])
        self.assertEqual(rec["recomp_save_dir"], launch["save_root"])
        self.assertEqual(rec["recomp_save_dir_source"], "runner")
        self.assertTrue(rec["verified"])
        self.assertEqual(rec["observed_udata_root"], launch["save_root"] + "\\UDATA\\5345000a")
        self.assertEqual(rec["observed_tdata_root"], launch["save_root"] + "\\TDATA\\5345000a")
        self.assertTrue(result["save_root_verified"])

    def test_the_private_root_lives_inside_this_runs_directory(self):
        _, rec, _, launch = self._run(self._good)
        self.assertTrue(ntpath.normcase(launch["save_root"]).startswith(
            ntpath.normcase(str(launch["run_dir"].resolve())) + "\\"))

    def test_a_caller_value_wins_and_is_recorded_as_the_callers(self):
        other = str(self.root / "elsewhere")
        rc, rec, result, launch = self._run(
            lambda root: self._good(root), env={"RECOMP_SAVE_DIR": other})
        self.assertEqual(launch["env_save_dir"], other)
        self.assertEqual(rec["recomp_save_dir"], other)
        self.assertEqual(rec["recomp_save_dir_source"], "caller")

    def test_a_caller_root_whose_folders_land_elsewhere_does_not_verify(self):
        other = str(self.root / "elsewhere")
        rc, rec, result, _ = self._run(
            lambda root: self._good(root).replace(
                root + "\\UDATA", other + "\\UDATA").replace(
                root + "\\TDATA", other + "\\TDATA"),
            env={"RECOMP_SAVE_DIR": other})
        self.assertEqual(rc, 2)
        self.assertFalse(rec["verified"])
        self.assertFalse(result["save_root_verified"])

    def test_a_log_without_the_folder_lines_fails_the_run(self):
        rc, rec, result, _ = self._run(
            lambda root: self._good(root).split("[SAVE] udata_root")[0])
        self.assertEqual(rc, 2)
        self.assertFalse(rec["verified"])
        self.assertIsNone(rec["observed_udata_root"])
        self.assertFalse(result["save_root_verified"])

    def test_folders_under_the_shipped_game_dir_fail_the_run(self):
        rc, rec, result, _ = self._run(
            lambda root: self._good(root).replace(
                root + "\\UDATA", "C:\\game\\UDATA").replace(
                root + "\\TDATA", "C:\\game\\TDATA"))
        self.assertEqual(rc, 2)
        self.assertFalse(rec["verified"])

    def test_a_duplicate_folder_line_fails_the_run(self):
        rc, rec, _, _ = self._run(lambda root: self._good(root) + self._good(root).splitlines()[2] + "\n")
        self.assertEqual(rc, 2)
        self.assertFalse(rec["verified"])


# --- the refusal in main.c ------------------------------------------------------

HARNESS = r'''
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jsrf_save_root.h"
static int shutdown_calls;
void xbox_kernel_shutdown(void) { ++shutdown_calls; }
void xbox_MemoryLayoutShutdown(void) {}
BOOL xbox_translate_path(const char *p, WCHAR *buf, DWORD n)
{
    const char *e = strstr(p, "UDATA") ? getenv("FAKE_UDATA") : getenv("FAKE_TDATA");
    if (!e || !strcmp(e, "FAIL")) return FALSE;
    return MultiByteToWideChar(CP_UTF8, 0, e, -1, buf, (int)n) > 0;
}
static int run(void)
{
    char *xbe_data = NULL;
%BLOCK%
    return 0;
}
int main(void)
{
    int rc = run();
    fprintf(stderr, "rc=%d shutdown=%d\n", rc, shutdown_calls);
    return rc;
}
'''


def _block():
    text = MAIN_C.read_text(encoding="utf-8")
    start = text.index("    {\n        /* The title's save folders translate")
    end = text.index("    xbox_kernel_bridge_init();", start)
    block = text[start:end]
    assert "udata_layer=FAIL" in block
    return block


def _compiler():
    for c in (shutil.which("clang"), shutil.which("gcc"),
              r"C:\Program Files\LLVM\bin\clang.exe"):
        if c and Path(c).exists():
            return c
    raise AssertionError("no C compiler found (clang, gcc or "
                         r"C:\Program Files\LLVM\bin\clang.exe); this test "
                         "cannot run, which is a failure, not a skip")


@unittest.skipUnless(os.name == "nt", "main.c is Windows-only")
class MainRefusalTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls._tmp.name)
        (tmp / "t.c").write_text(HARNESS.replace("%BLOCK%", _block()))
        cls.exe = tmp / "t.exe"
        r = subprocess.run(
            [_compiler(), str(tmp / "t.c"), str(ROOT / "src" / "jsrf_save_root.c"),
             "-I", str(ROOT / "src"), "-o", str(cls.exe), "-w",
             "-lshell32", "-lole32", "-luser32", "-ladvapi32"],
            capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("compile failed:\n" + r.stderr[-3000:])
        cls.base = str(tmp / "root")

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def run_case(self, save_dir, udata, tdata):
        env = {k: v for k, v in os.environ.items()
               if k not in ("RECOMP_SAVE_DIR", "FAKE_UDATA", "FAKE_TDATA")}
        if save_dir is not None:
            env["RECOMP_SAVE_DIR"] = save_dir
        env["FAKE_UDATA"], env["FAKE_TDATA"] = udata, tdata
        r = subprocess.run([str(self.exe)], capture_output=True, text=True, env=env)
        return r.returncode, r.stdout, r.stderr

    def inside(self):
        b = self.base
        return b + "\\UDATA\\5345000a", b + "\\TDATA\\5345000a"

    def test_folders_under_the_save_dir_proceed_and_are_reported_once(self):
        u, t = self.inside()
        rc, out, err = self.run_case(self.base, u, t)
        self.assertEqual(rc, 0, err)
        self.assertEqual(out.count("[SAVE] udata_root="), 1, out)
        self.assertEqual(out.count("[SAVE] tdata_root="), 1, out)
        self.assertNotIn("FAIL", err)
        # What the runner parses is what main.c printed.
        self.assertTrue(observe(out, self.base)[2], out)

    def test_a_udata_outside_the_save_dir_is_refused(self):
        u, t = self.inside()
        rc, out, err = self.run_case(self.base, "C:\\game\\UDATA\\5345000a", t)
        self.assertEqual(rc, 2, err)
        self.assertIn("[SAVE] udata_layer=FAIL: save folders are not under "
                      "RECOMP_SAVE_DIR=" + self.base, err)
        self.assertIn("shutdown=1", err)
        self.assertFalse(observe(out, self.base)[2])

    def test_a_tdata_outside_the_save_dir_is_refused(self):
        u, t = self.inside()
        rc, out, err = self.run_case(self.base, u, "C:\\game\\TDATA\\5345000a")
        self.assertEqual(rc, 2, err)
        self.assertIn("udata_layer=FAIL", err)

    def test_a_sibling_sharing_the_prefix_is_refused(self):
        rc, out, err = self.run_case(self.base, self.base + "2\\UDATA\\x",
                                     self.base + "2\\TDATA\\x")
        self.assertEqual(rc, 2, err)

    def test_the_save_dir_itself_is_refused(self):
        rc, out, err = self.run_case(self.base, self.base, self.base + "\\TDATA\\x")
        self.assertEqual(rc, 2, err)

    def test_dotdot_out_of_the_save_dir_is_refused(self):
        u, t = self.inside()
        rc, out, err = self.run_case(self.base, self.base + "\\..\\other\\UDATA", t)
        self.assertEqual(rc, 2, err)

    def test_case_slash_and_trailing_separator_variants_of_the_save_dir_proceed(self):
        u, t = self.inside()
        for variant in (self.base.upper(), self.base.replace("\\", "/"),
                        self.base + "\\", self.base.replace("\\", "/") + "/"):
            with self.subTest(variant=variant):
                rc, out, err = self.run_case(variant, u, t)
                self.assertEqual(rc, 0, err)

    def test_a_translation_that_fails_is_refused_when_enforcing(self):
        u, t = self.inside()
        rc, out, err = self.run_case(self.base, "FAIL", t)
        self.assertEqual(rc, 2, err)
        self.assertIn("udata_root unavailable", err)
        self.assertIn("udata_layer=FAIL", err)

    def test_a_save_dir_too_long_to_resolve_is_refused(self):
        u, t = self.inside()
        rc, out, err = self.run_case("C:\\" + "d" * 300, u, t)
        self.assertEqual(rc, 2, err)

    def test_owner_play_only_reports(self):
        rc, out, err = self.run_case(None, "C:\\game\\UDATA\\5345000a",
                                     "C:\\game\\TDATA\\5345000a")
        self.assertEqual(rc, 0, err)
        self.assertEqual(out.count("[SAVE] udata_root="), 1, out)
        self.assertNotIn("FAIL", err)
        self.assertFalse(observe(out, self.base)[2])

    def test_an_empty_save_dir_is_owner_play(self):
        rc, out, err = self.run_case("", "C:\\game\\UDATA\\5345000a",
                                     "C:\\game\\TDATA\\5345000a")
        self.assertEqual(rc, 0, err)

    def test_owner_play_with_an_unavailable_translation_still_proceeds(self):
        rc, out, err = self.run_case(None, "FAIL", "C:\\game\\TDATA\\x")
        self.assertEqual(rc, 0, err)
        self.assertIn("udata_root unavailable", err)


if __name__ == "__main__":
    unittest.main()
