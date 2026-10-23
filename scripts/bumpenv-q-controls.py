#!/usr/bin/env python3
"""Private RED/GREEN controls for the BUMPENVMAP q-independence gate.

This script never writes to the shared toolkit checkout or to the game's
production sources. It:

  1. copies the toolkit to a private directory OUTSIDE both repos,
  2. applies exactly ONE textual mutation there,
  3. configures and builds a private game build against that copy,
  4. runs the focused cases and requires RED with NAMED assertions,
  5. restores the mutated file and requires GREEN again,

and prints, for every step, the command, its exit code and the sha256 of the
mutated and restored files. Before and after, it prints the sha256 of the
shared toolkit's three production files, which proves the shared checkout was
never written.

It never runs the full test suite, the mutation harness, or any corpus/retail
regeneration. Like `scripts/pbcov-mutants.py`, it is a deliberate, documented
manual harness and is NOT registered with CTest: it rewrites production sources
in a private copy and takes minutes to run, so it is invoked by hand and cited
in the record rather than run by every `ctest` invocation.

    python -X utf8 scripts/bumpenv-q-controls.py --control q-divide
    python -X utf8 scripts/bumpenv-q-controls.py --control bind-guard
    python -X utf8 scripts/bumpenv-q-controls.py --control all

The scratch directory defaults to `tmp-bumpenv-controls`, a sibling of the game
checkout, and must live outside both repos AND outside %TEMP%: MSBuild's
incremental build is unreliable under the temporary directory (MSB8029), and a
restore that silently reused a mutated object would then be reported as a
GREEN/FAIL verdict about the product instead of a harness bug. For the same
reason the restore half deletes the focused targets' objects and executables
before rebuilding and asserts the restored executables really changed.

Exit status: 0 when every requested control was RED then GREEN as required,
1 when a control did not behave, 2 on a usage/setup error, 3 when the harness
itself is broken (e.g. a restored build produced an identical executable,
meaning the restored sources were never recompiled).
"""

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

GAME = Path(__file__).resolve().parent.parent
TOOLKIT = (GAME / ".." / "xboxrecomp").resolve()

# The three production files the primary's repair touched. Read-only here.
PRODUCTION = [
    "src/kernel/nv2a_pb_exec.c",
    "src/video/nv2a_vk_backend.c",
    "src/video/shaders/nv2a_vk_rc.hlsl",
]

# ── The controls ────────────────────────────────────────────────────────────
# Each mutation is ONE exact textual replacement. `red_cases` is the set of
# focused cases that must fail, and `red_markers` the assertion text that must
# appear in the failure (a named assertion, never a crash or a skip).

CONTROLS = {
    "q-divide": {
        "title": "restore PROJECT2D's /q divide on the bump paths",
        "edits": [
            # CPU executor: the repaired line, and the divide put back.
            ("src/kernel/nv2a_pb_exec.c",
             "rc_texel(t, 0, c[0] + dsdt[0], c[1] + dsdt[1], out);",
             "rc_texel(t, 0, c[0] / q + dsdt[0], c[1] / q + dsdt[1], out);"),
            # The AOT uber-shader: the same rule, in HLSL. The SPIR-V is
            # rebuilt by the build's own xbr_add_spirv rule; nothing is
            # compiled at run time.
            # One replacement, so no new declaration is needed: the divide is
            # spelled inline exactly as the pre-repair source spelled it.
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "tx.SampleLevel(sm, float2(t.x + dsdt.x, t.y + dsdt.y) * ts.xy, 0)",
             "tx.SampleLevel(sm, float2(t.x / (t.w != 0.0 ? t.w : 1.0) + dsdt.x, "
             "t.y / (t.w != 0.0 ? t.w : 1.0) + dsdt.y) * ts.xy, 0)"),
        ],
        "cpu_cases": ["bump_zero_matrix", "bump_displacement",
                      "bump_positive_delta", "bump_lum"],
        "cpu_markers": [
            "bump q=2 selects the SAME hand-derived texel (no PROJECT2D divide)",
            "mode7 q=2 R is the same texel and the same luminance",
        ],
        "vk_cases": ["vk_bump_q"],
        "vk_markers": [
            "vk bump q=2 wrote the same texel as q=1",
            "vk bump lum q=2 wrote the same texel as q=1",
        ],
        # The control that must keep passing: PROJECT2D itself is untouched.
        "cpu_still_green": ["project2d_control"],
    },
    "bind-guard": {
        "title": "restore the widened bind guard (m > 3 / m > 7 style)",
        "edits": [
            ("src/video/nv2a_vk_backend.c",
             "if (!tx || !(m == 1 || m == 2 || m == 3 || m == 6 || m == 7))",
             "if (!tx || m < 1 || m > 7)"),
        ],
        "cpu_cases": [],
        "cpu_markers": [],
        "vk_cases": ["vk_bind_witness"],
        "vk_markers": [
            "mode 4 must not upload its stage texture",
            "mode 5 must not upload its stage texture",
        ],
        "cpu_still_green": [],
    },
}


# ── Helpers ─────────────────────────────────────────────────────────────────

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class HarnessError(Exception):
    """The harness itself is broken -- never a verdict about the product."""


TARGETS = ["jsrf_bumpenv_q_cpu_test", "jsrf_bumpenv_q_vk_test"]


def force_rebuild(build_dir, targets=TARGETS):
    """Delete the focused targets' objects and executables before a build.

    Both test translation units #include production sources that CMake does
    NOT list among their sources: tests/test_bumpenv_q_cpu.c includes
    src/kernel/nv2a_pb_exec.c and tests/test_bumpenv_q_vk.c includes
    src/video/nv2a_vk_backend.c. MSBuild's compiler include-tracking is
    therefore the ONLY thing that would notice a change in those files, and
    that tracking is exactly what is unreliable here (it is what MSB8029
    disables under %TEMP%). Removing the objects makes the recompile
    unconditional, so a stale binary cannot masquerade as a RED or a GREEN.
    """
    removed = []
    for t in targets:
        d = build_dir / f"{t}.dir"
        if d.exists():
            shutil.rmtree(d)
            removed.append(d.name + "/")
        for exe in (build_dir / "Release").glob(f"{t}.exe"):
            exe.unlink()
            removed.append(exe.name)
    # The uber-shader's SPIR-V is produced by xbr_add_spirv as a custom
    # command keyed on the HLSL's timestamp. A restored HLSL copied back with
    # `copy2` keeps the SHARED file's older mtime, so dxc would not rerun and
    # the VK half would keep executing the mutated shader while the source on
    # disk looks pristine. Deleting the generated outputs makes the
    # regeneration unconditional, exactly like deleting the test objects.
    spirv = build_dir / "xboxrecomp" / "src" / "video" / "spirv"
    for gen in sorted(spirv.glob("xbr_nv2a_vk_rc_*")):
        gen.unlink()
        removed.append(gen.name)
    print("\nforced recompile: removed "
          + (", ".join(removed) if removed else "nothing (already absent)"))


def exe_hashes(build_dir, targets=TARGETS):
    """The sha256 of each focused test executable; missing is a harness bug."""
    out = {}
    for t in targets:
        p = build_dir / "Release" / f"{t}.exe"
        if not p.is_file():
            raise HarnessError(f"the build produced no {p}")
        out[t] = sha256(p)
    return out


def print_hashes(label, hashes):
    print(f"  {label}:")
    for t, h in hashes.items():
        print(f"    {h}  {t}.exe")


def run(cmd, cwd=None, env=None, label=""):
    """Run one command, echo it, return (exit code, combined output)."""
    print(f"\n$ {' '.join(str(c) for c in cmd)}")
    if label:
        print(f"  ({label})")
    p = subprocess.run([str(c) for c in cmd], cwd=str(cwd) if cwd else None,
                       env=env, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    out = (p.stdout or "") + (p.stderr or "")
    print(out, end="" if out.endswith("\n") else "\n")
    print(f"  exit code: {p.returncode}")
    return p.returncode, out


def production_hashes():
    return {rel: sha256(TOOLKIT / rel) for rel in PRODUCTION}


def copy_toolkit(dest):
    """A private copy of the toolkit's working tree, outside both repos."""
    def ignore(dirname, names):
        skip = {".git", "build"}
        return [n for n in names if n in skip]
    shutil.copytree(TOOLKIT, dest, ignore=ignore, symlinks=True)


def apply_edits(root, edits):
    """Apply each exact replacement; refuse if the text is not there once."""
    touched = []
    for rel, old, new in edits:
        path = root / rel
        text = path.read_text(encoding="utf-8")
        if old == new:
            continue
        n = text.count(old)
        if n != 1:
            raise SystemExit(
                f"REFUSED: {rel} contains the text to mutate {n} times, not once.\n"
                f"  looking for: {old[:100]}")
        path.write_text(text.replace(old, new, 1), encoding="utf-8")
        touched.append(rel)
    return sorted(set(touched))


def shared_cache_value(key):
    """One entry of the shared build tree's cache, or ''."""
    cache = GAME / "build" / "CMakeCache.txt"
    if not cache.is_file():
        return ""
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(key + ":"):
            return line.split("=", 1)[1].strip()
    return ""


def configure_private_build(build_dir, toolkit_copy):
    """Configure a private game build against the private toolkit copy.

    JSRF_FIXTURES_ONLY is the project's own supported mode for "no game data
    and no generated or recovered retail-derived code": the focused cases need
    neither, and the shared tree's `build/vsh-tool` (which the corpus
    provenance guard owns, and which records the SHARED toolkit) is then not
    consulted at all. That keeps the private build private.
    """
    cmd = [
        "cmake", "-S", str(GAME), "-B", str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DXBOXRECOMP_DIR={toolkit_copy}",
        "-DBUILD_TESTING=ON",
        "-DJSRF_FIXTURES_ONLY=ON",
    ]
    icd = shared_cache_value("XBR_VK_SW_ICD")
    if icd:
        cmd.append(f"-DXBR_VK_SW_ICD={icd}")
    return run(cmd, label=f"private configure against {toolkit_copy}")


def build_targets(build_dir, targets):
    return run(["cmake", "--build", str(build_dir), "--config", "Release",
                "--target", *targets], label="focused targets only")


def ctest_case(build_dir, name, env):
    return run(["ctest", "--test-dir", str(build_dir), "-C", "Release",
                "-R", f"^{name}$", "--output-on-failure"], env=env)


def vk_env():
    env = dict(os.environ)
    icd = env.get("XBR_VK_SW_ICD") or shared_cache_value("XBR_VK_SW_ICD")
    if icd:
        env["VK_DRIVER_FILES"] = icd
        env["VK_ICD_FILENAMES"] = icd
    env["RECOMP_VK_DEVICE"] = "llvmpipe"
    env["RECOMP_VK_PIPELINE_CACHE"] = "off"
    return env


def expect_red(build_dir, case, markers, env, prefix):
    """The case must FAIL and must name at least one of `markers`."""
    rc, out = ctest_case(build_dir, prefix + case, env)
    if rc == 0:
        print(f"  CONTROL FAILED: {case} was expected RED and passed")
        return False
    hit = [m for m in markers if m in out]
    if not hit:
        print(f"  CONTROL FAILED: {case} failed without a named assertion; "
              f"looked for {markers}")
        return False
    print(f"  RED as required, named assertion(s): {hit}")
    return True


def expect_green(build_dir, case, env, prefix):
    rc, _ = ctest_case(build_dir, prefix + case, env)
    if rc != 0:
        print(f"  CONTROL FAILED: {case} was expected GREEN and failed")
        return False
    print(f"  GREEN as required: {case}")
    return True


# ── One control ─────────────────────────────────────────────────────────────

def run_control(name, workdir, shared_before):
    spec = CONTROLS[name]
    print(f"\n{'=' * 78}\ncontrol: {name} -- {spec['title']}\n{'=' * 78}")

    copy = workdir / f"toolkit-{name}"
    build = workdir / f"build-{name}"
    if copy.exists():
        shutil.rmtree(copy)
    copy_toolkit(copy)

    rc, _ = configure_private_build(build, copy)
    if rc != 0:
        print("  CONTROL FAILED: the private configure failed")
        return False

    # Baseline: build the PRISTINE copy first, so the mutation build has an
    # executable hash to differ from. Without this baseline a "the mutation
    # never reached the compiler" check has nothing to compare against.
    rc, _ = build_targets(build, TARGETS)
    if rc != 0:
        print("  CONTROL FAILED: the pristine private build failed")
        return False
    pre_hashes = exe_hashes(build)
    print_hashes("executables after the PRISTINE build (baseline)", pre_hashes)

    mutated = apply_edits(copy, spec["edits"])
    print("\nmutated files in the PRIVATE copy:")
    before_mut = {}
    for rel in mutated:
        before_mut[rel] = sha256(copy / rel)
        print(f"  {rel}\n    sha256 {before_mut[rel]}  (mutated)")

    env = vk_env()
    ok = True
    force_rebuild(build)
    rc, _ = build_targets(build, TARGETS)
    if rc != 0:
        print("  CONTROL FAILED: the private build failed (a compile error is not RED)")
        return False
    mutated_hashes = exe_hashes(build)
    print_hashes("executables after the MUTATED build", mutated_hashes)
    if mutated_hashes == pre_hashes:
        raise HarnessError(
            f"{name}: the mutated build produced byte-identical executables; the "
            f"mutation never reached the compiler")

    for case in spec["cpu_cases"]:
        ok &= expect_red(build, case, spec["cpu_markers"], env, "jsrf_bumpenv_q_cpu_")
    for case in spec["vk_cases"]:
        ok &= expect_red(build, case, spec["vk_markers"], env, "jsrf_bumpenv_q_vk_")
    for case in spec["cpu_still_green"]:
        ok &= expect_green(build, case, env, "jsrf_bumpenv_q_cpu_")

    # Restore: re-copy the pristine files from the shared checkout.
    print("\nrestoring the private copy from the shared checkout (read-only there):")
    for rel in mutated:
        shutil.copy2(TOOLKIT / rel, copy / rel)
        # copy2 preserves the source mtime, which is OLDER than every object
        # and generated file the mutated source just produced. force_rebuild
        # deletes those anyway, so this is belt-and-braces rather than the
        # mechanism; leaving a stale timestamp would be a trap for anyone who
        # later builds without going through force_rebuild.
        os.utime(copy / rel, None)
        h = sha256(copy / rel)
        print(f"  {rel}\n    sha256 {h}  (restored)")
        if h != shared_before[rel]:
            print("  CONTROL FAILED: the restored file does not match the shared file")
            ok = False

    # The recompile is made unconditional, so the restored verdict cannot be
    # a stale mutated object being reused (see force_rebuild's note).
    force_rebuild(build)
    rc, _ = build_targets(build, TARGETS)
    if rc != 0:
        print("  CONTROL FAILED: the restored build failed")
        return False
    restored_hashes = exe_hashes(build)
    print_hashes("executables after the RESTORED build", restored_hashes)
    if restored_hashes == mutated_hashes:
        raise HarnessError(
            f"{name}: the restored build reused the mutated binary "
            f"({restored_hashes}); the GREEN half would be meaningless")
    for t in TARGETS:
        if restored_hashes[t] != pre_hashes[t]:
            print(f"  note: {t}.exe is not byte-identical to the pre-mutation "
                  f"binary (build-path/timestamp dependent); the hash CHANGED, "
                  f"which is what proves the recompile")

    for case in spec["cpu_cases"] + spec["cpu_still_green"]:
        ok &= expect_green(build, case, env, "jsrf_bumpenv_q_cpu_")
    for case in spec["vk_cases"]:
        ok &= expect_green(build, case, env, "jsrf_bumpenv_q_vk_")
    return bool(ok)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--control", default="all",
                    choices=sorted(CONTROLS) + ["all"])
    ap.add_argument("--workdir", default=None,
                    help="private scratch directory (default: a "
                         "tmp-bumpenv-controls sibling of the game checkout, "
                         "outside both repos and outside %TEMP%)")
    args = ap.parse_args()

    if not (TOOLKIT / "CMakeLists.txt").is_file():
        print(f"not a toolkit checkout: {TOOLKIT}", file=sys.stderr)
        return 2

    if args.workdir:
        workdir = Path(args.workdir).resolve()
        workdir.mkdir(parents=True, exist_ok=True)
    else:
        # NOT tempfile.mkdtemp(): MSBuild refuses to trust an incremental build
        # under %TEMP% (MSB8029), and a build it will not trust incrementally is
        # exactly how a stale mutated object survives a restore.
        workdir = (GAME.parent / "tmp-bumpenv-controls").resolve()
        workdir.mkdir(parents=True, exist_ok=True)
    for root in (GAME, TOOLKIT):
        if workdir == root or root in workdir.parents:
            print(f"refusing to work inside a repo: {workdir}", file=sys.stderr)
            return 2
    temp_root = Path(tempfile.gettempdir()).resolve()
    if workdir == temp_root or temp_root in workdir.parents:
        print(f"refusing to work under the temporary directory: {workdir}\n"
              f"  MSBuild will not trust an incremental build there (MSB8029), "
              f"which can leave a stale object behind a restore.",
              file=sys.stderr)
        return 2
    print(f"private scratch directory: {workdir}")

    shared_before = production_hashes()
    print("\nshared toolkit production files, BEFORE:")
    for rel, h in shared_before.items():
        print(f"  {h}  {rel}")

    names = sorted(CONTROLS) if args.control == "all" else [args.control]
    ok = True
    try:
        for name in names:
            ok &= run_control(name, workdir, shared_before)
    except HarnessError as exc:
        print(f"\nHARNESS ERROR: {exc}", file=sys.stderr)
        print("This is a bug in the harness, not a verdict about the product: "
              "no RED/GREEN claim from this run is sound.", file=sys.stderr)
        return 3

    shared_after = production_hashes()
    print("\nshared toolkit production files, AFTER:")
    for rel, h in shared_after.items():
        print(f"  {h}  {rel}")
    if shared_after != shared_before:
        print("CONTROL FAILED: the shared toolkit changed during the run")
        ok = False
    else:
        print("the shared toolkit is byte-identical: it was never written")

    print("\nRESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
