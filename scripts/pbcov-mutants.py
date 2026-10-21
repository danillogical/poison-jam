#!/usr/bin/env python3
"""Private mutation harness for the D4 coverage deciding gate.

WHAT THIS PROVES
    A deciding gate has to fail when the behaviour it decides is broken.  This
    harness takes the frozen production toolkit, copies it to a private build
    area, applies ONE named textual mutation at a time, configures a private
    build of the game repository against that copy, builds ONLY the two gate
    targets, and requires the gate to fail with a NAMED semantic assertion --
    not a compile error, not a skip, not a crash.  It then restores the pristine
    copy, rebuilds, and requires the same cases to PASS again.  The shared
    toolkit checkout is never written, and its source hashes are checked before
    and after every mutation.

    The mutants are not the historical probes: each one reintroduces one
    historical defect or one plausible new one into the CURRENT source, so the
    assertion that catches it is the gate's own.  Three groups are marked
    `historical` below because they are exactly the rejected behaviours from the
    D4 step-1 review: stage-1-only false negatives (the stage-0-only texgen and
    bitmask matrix-enable mutants), first-record mutability/live availability
    (the first_a/first_b overwrite and live-bump mutants), and the selected
    vertex-program word's availability (the component/wrong-slot/never-seen
    mutants).

INHERENTLY UNOBSERVABLE, REPORTED RATHER THAN PADDED
    A single-thread process cannot observe the ordering of a record's payload
    writes against its `valid` bit, nor a torn read during that write.  No
    mutant below claims to decide that race.  The observable equivalent the
    gate does decide is used instead: a claimed record's bytes are identical
    before and after later batches, method deliveries, reselections and
    repeated reports (the first-record-immutability and report-retention
    mutants).  Everything else in the table is observable through the real
    committed-stream walk or the real Vulkan draw site.

USAGE
    python -X utf8 scripts/pbcov-mutants.py
    python -X utf8 scripts/pbcov-mutants.py --list
    python -X utf8 scripts/pbcov-mutants.py --only first-b-live-availability
    python -X utf8 scripts/pbcov-mutants.py --keep --work D:\\scratch\\pbcov

    --toolkit     toolkit to copy (default $XBOXRECOMP_DIR or ../xboxrecomp)
    --expect-commit  required toolkit HEAD (default: the frozen D4 commit)
    --work        private build area (default: a fresh temp directory)
    --generator   CMake generator (default: "Visual Studio 18 2026")
    --no-vk       skip the Vulkan mutants and the Vulkan target
    --keep        keep the private work area (for inspection)
"""

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

FROZEN_COMMIT = "2bcdc3081362341b92a9efb8afad00bdfd005c85"
HERE = pathlib.Path(__file__).resolve().parent
GAME = HERE.parent
DEFAULT_ICD = pathlib.Path("C:/Users/logic/tools/vk-sw/x64/lvp_icd.x86_64.json")

# Files this harness mutates; their bytes are hashed in the shared checkout
# before and after every mutation, and in the private copy after each restore.
PROTECTED = (
    "src/kernel/nv2a_pb_exec.c",
    "src/video/nv2a_vk_backend.c",
    "src/video/nv2a_vk_rc_coverage.h",
)

# ── The mutant table ─────────────────────────────────────────────────────────
#
# `old` must occur EXACTLY once in the private source.  `expect` is the gate's
# own assertion text (the part after "FAIL pbcov_*[case]: ").  `env` is the
# per-case coverage gate; the Vulkan environment is added for vk targets.

MUTANTS = [
    # ---- historical: stage-1-only false negatives -------------------------
    dict(
        name="stage0-only-texgen",
        group="historical: stage-1-only",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="ff_stage1_only",
        expect="ff_texgen count (stage 1 only)",
        why="the pre-repair predicate read only stage 0's texgen words",
        old="""            if (texgen[s][c])
                texgen_on = 1;
""",
        new="""            if (s == 0 && texgen[s][c])
                texgen_on = 1;              /* MUTANT: stage 0 only */
""",
    ),
    dict(
        name="matrix-enable-stage0-bitmask",
        group="historical: stage-1-only",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="ff_stage1_only",
        expect="ff_texmat count (stage 1 only)",
        why="the pre-repair code called the low four bits of 0x420 a per-stage mask",
        old="""            if (mat_enable[s])
                texmat_on = 1;
""",
        new="""            if (mat_enable[0] & 0xFu)
                texmat_on = 1;              /* MUTANT: one word as a bitmask */
""",
    ),
    # ---- historical: first-record mutability / live availability ----------
    dict(
        name="first-a-overwrite",
        group="historical: first-record mutability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="first_records_immutable",
        expect="first_a immutable",
        why="a claimed first_a record must never be rewritten by a later batch",
        old="        if (!L->first_a.valid && (texgen_on || texmat_on)) {",
        new="        if (texgen_on || texmat_on) {  /* MUTANT: first record mutable */",
    ),
    dict(
        name="first-b-overwrite",
        group="historical: first-record mutability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="stage_first_retained",
        expect="first_b is immutable across later batches",
        why="a claimed first_b record must survive a later qualifying batch",
        old="    if (any_ge6 && !L->first_b.valid) {",
        new="    if (any_ge6) {                     /* MUTANT: first record mutable */",
    ),
    dict(
        name="first-c-overwrite",
        group="historical: first-record mutability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="lit_both_and_immutable",
        expect="first_c is immutable across later batches",
        why="a claimed first_c record must survive a later lit batch",
        old="    if ((spec || spot_on) && !L->first_c.valid) {",
        new="    if (spec || spot_on) {           /* MUTANT: first record mutable */",
    ),
    dict(
        name="first-b-live-availability",
        group="historical: live availability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="stage_first_retained",
        expect="first_b is immutable across later batches",
        why="the pre-repair first_b availability mask was OR-ed live at delivery",
        old="""        s_pb_seen[method / 4] = 1;
""",
        new="""        s_pb_seen[method / 4] = 1;
        if (g_nv2a_pb_coverage.first_b.valid)
            g_nv2a_pb_coverage.first_b.bump_seen |= 1u;   /* MUTANT: live */
""",
    ),
    # ---- historical: selected vp word availability ------------------------
    dict(
        name="vp-provenance-wrong-component",
        group="historical: selected-word availability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="vp_component_unavailable",
        expect="component 1 does not establish",
        why="only a component-0 write may make dword 0 available",
        old="        if (pb_coverage() && slot % 4 == 0)",
        new="        if (pb_coverage() && slot % 4 == 1)   /* MUTANT: wrong component */",
    ),
    dict(
        name="vp-provenance-wrong-slot",
        group="historical: selected-word availability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="vp_cursor_advance",
        expect="vp_first_word_seen of the advanced slot",
        why="provenance must belong to the cursor slot that was actually written",
        old="            s_pb_vp_word0[s_vp.prog_load] = 1;",
        new="            s_pb_vp_word0[0] = 1;          /* MUTANT: wrong slot */",
    ),
    dict(
        name="vp-availability-never-seen",
        group="historical: selected-word availability",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="vp_port_0b10",
        expect="vp_first_word_seen from 0x0B10",
        why="an accepted component-0 upload through any adjacent port must be seen",
        old="             && s_pb_vp_word0[L->first_b.vp_start]) ? 1u : 0u;",
        new="             && 0) ? 1u : 0u;         /* MUTANT: availability never set */",
    ),
    # ---- dropped batch hook ------------------------------------------------
    dict(
        name="drop-batch-hook",
        group="batch counting",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="ff_texgen_only",
        expect="ff_texgen count",
        why="every count below is driven by the single raster_batch hook",
        old="""    if (pb_coverage())
        pb_coverage_batch();
""",
        new="    /* MUTANT: batch hook dropped */\n",
    ),
    # ---- histogram / double count -----------------------------------------
    dict(
        name="drop-mode-histogram",
        group="stage-mode incidence",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="stage6_7_multi",
        expect="stage_mode_hist[6]",
        why="the 5-bit mode histogram counts (batch, stage) incidences",
        old="""            if (m < 32)
                L->stage_mode_hist[m]++;
""",
        new="            /* MUTANT: histogram dropped */\n",
    ),
    dict(
        name="mode7-not-qualifying",
        group="stage-mode incidence",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="stage7_isolated",
        expect="stage_ge6 batch count",
        why="both refused stage modes 6 and 7 must qualify",
        old="        if (m >= 6) {",
        new="        if (m >= 6 && m != 7) {        /* MUTANT: mode 7 dropped */",
    ),
    dict(
        name="double-count-multi-stage-batch",
        group="stage-mode incidence",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="stage6_7_multi",
        expect="stage_ge6 batch count once",
        why="a batch with two qualifying stages must still count once",
        old="""            if (!any_ge6)
                L->stage_ge6_batches++;
""",
        new="            L->stage_ge6_batches++;         /* MUTANT: per incidence */\n",
    ),
    # ---- report / gate -----------------------------------------------------
    dict(
        name="report-resets-ledger",
        group="retention and gate",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="report_retained",
        expect="report leaves the ledger byte-identical (first call)",
        why="the report is an observation, never a reset",
        old="""    const Nv2aPbCoverageLedger *L = &g_nv2a_pb_coverage;
    uint32_t i;
""",
        new="""    const Nv2aPbCoverageLedger *L = &g_nv2a_pb_coverage;
    uint32_t i;
    g_nv2a_pb_coverage.ff_texgen_batches = 0;      /* MUTANT: report reset */
""",
    ),
    dict(
        name="gate-tests-presence",
        group="retention and gate",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="gate_off",
        expect="gate off: enabled stays 0",
        why="only the exact value \"1\" may enable the gate",
        old="        c = v != NULL && v[0] == '1' && v[1] == '\\0';",
        new="        c = v != NULL;                 /* MUTANT: presence gate */",
        env={"RECOMP_PB_COVERAGE": "0"},
    ),
    # ---- subchannel pollution ---------------------------------------------
    dict(
        name="subchannel-polls-coverage-shadow",
        group="commit boundary",
        target="stream",
        file="src/kernel/nv2a_pb_exec.c",
        case="subchannel_guard",
        expect="ff_texgen not polluted by subchannel 1",
        why="a nonzero-subchannel method is not committed to the 3D class",
        old="""    if (subch != 0) {                      /* 3D class lives on subchannel 0 */
        note_unhandled(method, param);
        return;
    }
""",
        new="""    if (pb_coverage())
        pb_coverage_note_method(method);   /* MUTANT: note before the filter */
    if (subch != 0) {                      /* 3D class lives on subchannel 0 */
        note_unhandled(method, param);
        return;
    }
""",
    ),
    # ---- Vulkan real-device wiring ----------------------------------------
    dict(
        name="vk-record-hook-dropped",
        group="Vulkan refusal coverage",
        target="vk",
        file="src/video/nv2a_vk_backend.c",
        case="rc_unsupported_modes",
        expect="ledger total equals the production counter",
        why="the production fallback site must be the one that records",
        old="""            if (nv2a_vk_rc_coverage_enabled())
                nv2a_vk_rc_coverage_record(dr.rc_reason, dr.rc_stage, dr.rc_mode,
                                           b->combiner->stage_program,
                                           vb_cube_mask(b));
""",
        new="            /* MUTANT: record hook dropped */\n",
    ),
    dict(
        name="vk-cube-reason-count-dropped",
        group="Vulkan refusal coverage",
        target="vk",
        file="src/video/nv2a_vk_rc_coverage.h",
        case="rc_cube_mode3",
        expect="cube reason counted",
        why="each refusal increments exactly one reason counter",
        old="""    else if (reason == 2)
        c->cube_mode3++;
""",
        new="    /* MUTANT: cube reason count dropped */\n",
    ),
    dict(
        name="vk-cube-reason-misclassified",
        group="Vulkan refusal coverage",
        target="vk",
        file="src/video/nv2a_vk_rc_coverage.h",
        case="rc_cube_mode3",
        expect="mode>5 reason not counted",
        why="the cube refusal must not be booked as a mode>5 refusal",
        old="""    else if (reason == 2)
        c->cube_mode3++;
""",
        new="""    else if (reason == 2)
        c->mode_gt5++;                 /* MUTANT: wrong reason bucket */
""",
    ),
    dict(
        name="vk-first-refusal-overwrite",
        group="Vulkan refusal coverage",
        target="vk",
        file="src/video/nv2a_vk_rc_coverage.h",
        case="rc_first_report",
        expect="first stage",
        why="the first refusal record is claimed once and never rewritten",
        old="    if (!c->first_valid && (reason == 1 || reason == 2)) {",
        new="    if (reason == 1 || reason == 2) {  /* MUTANT: first record mutable */",
    ),
    dict(
        name="vk-cube-exception-dropped",
        group="Vulkan refusal coverage",
        target="vk",
        file="src/video/nv2a_vk_rc_coverage.h",
        case="rc_cube_mode3",
        expect="supported mode 3 does not add a fallback",
        why="mode 3 without a cube texture is supported and must not be refused",
        old="        if (m == 3 && ((cube_mask >> st) & 1u)) {",
        new="        if (m == 3) {                   /* MUTANT: cube exception dropped */",
    ),
]

# Cases re-run on the pristine baseline (each mutant's own case, plus one key
# case per property that has no mutant of its own).
EXTRA_BASELINE = {
    "stream": ["entry_forms", "subchannel_guard", "rejected_stream_rollback",
               "guards_stopped", "guards_sub_minimum", "vp_component_unavailable",
               "vp_zero_observed", "vp_start_out_of_range", "gate_on_identity"],
    "vk": ["rc_cube_mode3", "rc_supported_modes", "rc_priority", "rc_vtable",
           "rc_first_sample_on_wins"],
}


# ── Small process helpers ────────────────────────────────────────────────────

def run(cmd, cwd=None, env=None, timeout=1800):
    started = time.time()
    p = subprocess.run([str(c) for c in cmd], cwd=None if cwd is None else str(cwd),
                       env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       text=True, errors="replace", timeout=timeout)
    return p.returncode, p.stdout, time.time() - started


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def checkout_state(root: pathlib.Path):
    rc, out, _ = run(["git", "-C", root, "rev-parse", "HEAD"], timeout=60)
    head = out.strip() if rc == 0 else "?"
    # Tracked-file changes are what this harness must not cause.  --untracked-files=no
    # also keeps an unreadable ignored directory elsewhere from adding a warning.
    rc, out, _ = run(["git", "-C", root, "status", "--porcelain", "--untracked-files=no"],
                     timeout=60)
    status = out if rc == 0 else "?"
    return head, status


def copy_working_tree(src_root: pathlib.Path, dst_root: pathlib.Path):
    """Copy every tracked file byte-for-byte from the WORKING TREE.

    `git archive` is deliberately not used: core.autocrlf rewrites the blob, and
    the coverage corpus records hashes of the working-tree bytes.  A copy that
    differs by line endings would make the corpus guard (rightly) refuse the
    private build for a reason unrelated to this gate.
    """
    rc, out, _ = run(["git", "-C", src_root, "ls-files", "-z"], timeout=120)
    if rc != 0:
        raise RuntimeError(f"git ls-files failed in {src_root}")
    rels = [r for r in out.split("\0") if r]
    copied = 0
    for rel in rels:
        src = src_root / rel
        if not src.is_file():
            continue
        dst = dst_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
    return copied


def stream_env(extra=None):
    env = dict(os.environ)
    env.pop("RECOMP_PB_COVERAGE", None)
    env["RECOMP_PB_COVERAGE"] = "1"
    if extra:
        env.update(extra)
    return env


def vk_env(icd: pathlib.Path | None, extra=None):
    env = dict(os.environ)
    env.pop("RECOMP_PB_COVERAGE", None)
    env["RECOMP_PB_COVERAGE"] = "1"
    env["RECOMP_VK_DEVICE"] = "llvmpipe"
    env["RECOMP_VK_PIPELINE_CACHE"] = "off"
    if icd is not None:
        native = str(icd)
        env["VK_DRIVER_FILES"] = native
        env["VK_ICD_FILENAMES"] = native
    if extra:
        env.update(extra)
    return env


def find_case_exe(build_dir: pathlib.Path, target: str) -> pathlib.Path:
    names = ("jsrf_pbcov_stream_test.exe" if target == "stream"
             else "jsrf_pbcov_vk_test.exe")
    for cfg in ("Release", "Debug", "RelWithDebInfo", "MinSizeRel", ""):
        p = build_dir / cfg / names if cfg else build_dir / names
        if p.is_file():
            return p
    raise RuntimeError(f"{names} not found under {build_dir}")


def run_case(exe: pathlib.Path, case: str, target: str, icd, extra_env=None):
    env = (stream_env(extra_env) if target == "stream"
           else vk_env(icd, extra_env))
    return run([exe, "--case", case], env=env, timeout=300)


# ── Harness ──────────────────────────────────────────────────────────────────

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--toolkit", type=pathlib.Path,
                    default=pathlib.Path(os.environ.get("XBOXRECOMP_DIR", GAME.parent / "xboxrecomp")))
    ap.add_argument("--expect-commit", default=FROZEN_COMMIT)
    ap.add_argument("--work", type=pathlib.Path, default=None)
    ap.add_argument("--generator", default="Visual Studio 18 2026")
    ap.add_argument("--icd", type=pathlib.Path, default=None,
                    help="software Vulkan ICD manifest (default: $XBR_VK_SW_ICD or the local lavapipe)")
    ap.add_argument("--only", action="append", default=[],
                    help="run only the named mutant(s)")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--no-vk", action="store_true")
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args(argv)

    mutants = list(MUTANTS)
    if args.no_vk:
        mutants = [m for m in mutants if m["target"] != "vk"]
    if args.only:
        wanted = set(args.only)
        unknown = wanted - {m["name"] for m in MUTANTS}
        if unknown:
            print(f"unknown mutant(s): {', '.join(sorted(unknown))}", file=sys.stderr)
            return 2
        mutants = [m for m in mutants if m["name"] in wanted]
    if args.list:
        for m in mutants:
            print(f"{m['name']:<34} {m['group']:<42} {m['target']:<7} {m['case']:<28} {m['expect']}")
        return 0

    toolkit = args.toolkit.resolve()
    game = GAME.resolve()
    if not (toolkit / "CMakeLists.txt").is_file():
        print(f"toolkit not found: {toolkit}", file=sys.stderr)
        return 2
    head, status = checkout_state(toolkit)
    if args.expect_commit and head != args.expect_commit:
        print(f"toolkit HEAD is {head}, expected the frozen {args.expect_commit}; "
              f"pass --expect-commit '' to override", file=sys.stderr)
        return 2
    if status.strip():
        print("the shared toolkit checkout is not clean; this harness only copies it:\n"
              + status, file=sys.stderr)
        return 2
    shared_hashes = {rel: sha256(toolkit / rel) for rel in PROTECTED}

    work = args.work
    if work is None:
        work = pathlib.Path(tempfile.mkdtemp(prefix="pbcov-mutants-"))
    else:
        work = work.resolve()
        if work.exists():
            shutil.rmtree(work)
        work.mkdir(parents=True)
    work.mkdir(parents=True, exist_ok=True)
    print(f"work area: {work}")

    # ICD: explicit, environment, the existing build's cache, then the local default.
    icd = args.icd
    if icd is None:
        env_icd = os.environ.get("XBR_VK_SW_ICD")
        if env_icd:
            icd = pathlib.Path(env_icd)
        else:
            cache = game / "build" / "CMakeCache.txt"
            if cache.is_file():
                for line in cache.read_text(errors="replace").splitlines():
                    if line.startswith("XBR_VK_SW_ICD:"):
                        value = line.split("=", 1)[1].strip()
                        if value:
                            icd = pathlib.Path(value)
                        break
    if icd is None and DEFAULT_ICD.is_file():
        icd = DEFAULT_ICD
    if args.no_vk:
        icd = None

    # ── Private copies (byte-for-byte from the working tree) ────────────────
    pristine = work / "toolkit-pristine"
    toolkit_copy = work / "toolkit"
    private_game = work / "game"
    build_dir = work / "build"
    print("copying the toolkit working tree ...")
    n = copy_working_tree(toolkit, pristine)
    print(f"  {n} tracked files")
    shutil.copytree(pristine, toolkit_copy)
    for rel in PROTECTED:
        if sha256(toolkit_copy / rel) != shared_hashes[rel]:
            print(f"private copy differs from the shared checkout: {rel}", file=sys.stderr)
            return 2

    print("copying the game working tree (tracked + generated sources + vsh tool) ...")
    n = copy_working_tree(game, private_game)
    for extra in ("tests/test_pbcov_stream.c", "tests/test_pbcov_vk.c"):
        src = game / extra
        if src.is_file():
            dst = private_game / extra
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
    for rel in ("src/recomp/gen", "src/recomp/recovered"):
        src, dst = game / rel, private_game / rel
        if src.is_dir():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
    vsh_tool = game / "build" / "vsh-tool"
    if vsh_tool.is_dir():
        dst = private_game / "build" / "vsh-tool"
        shutil.copytree(vsh_tool, dst)
        # The owned tool tree's cache names the ORIGINAL toolkit; removing the
        # cache leaves its (hash-identical) executable in place and lets the
        # provenance guard validate the copy against the private toolkit.
        (dst / "CMakeCache.txt").unlink(missing_ok=True)
    print(f"  {n} tracked files")

    # ── Configure and build the frozen baseline ─────────────────────────────
    configure_env = dict(os.environ)
    configure_env.pop("RECOMP_PB_COVERAGE", None)
    if icd is not None:
        configure_env["XBR_VK_SW_ICD"] = str(icd)
    cmake = shutil.which("cmake") or "cmake"
    print("configuring the private build against the private toolkit ...")
    rc, out, elapsed = run([cmake, "-S", private_game, "-B", build_dir,
                            "-G", args.generator, "-A", "x64",
                            f"-DXBOXRECOMP_DIR={toolkit_copy}"],
                           env=configure_env, timeout=1800)
    if rc != 0:
        print(out[-4000:], file=sys.stderr)
        print("private configure failed", file=sys.stderr)
        return 2
    print(f"  configured in {elapsed:.1f}s")

    targets = ["jsrf_pbcov_stream_test"]
    if not args.no_vk:
        targets.append("jsrf_pbcov_vk_test")
    print("building the frozen baseline gate targets ...")
    rc, out, elapsed = run([cmake, "--build", build_dir, "--config", "Release",
                            "--target"] + targets, env=configure_env, timeout=3600)
    if rc != 0:
        print(out[-5000:], file=sys.stderr)
        print("baseline build failed", file=sys.stderr)
        return 2
    print(f"  built in {elapsed:.1f}s")

    exes = {"stream": find_case_exe(build_dir, "stream")}
    if not args.no_vk:
        exes["vk"] = find_case_exe(build_dir, "vk")

    def baseline_case(m):
        extra = m.get("env")
        return run_case(exes[m["target"]], m["case"], m["target"], icd, extra)

    baseline_ok = True
    print("\nbaseline run (pristine copy; every targeted case must PASS)")
    for m in mutants:
        rc, out, _ = baseline_case(m)
        ok = rc == 0 and f"PASS pbcov_" in out
        baseline_ok &= ok
        print(f"  {'PASS' if ok else 'FAIL'}  {m['target']:<6} {m['case']:<28} "
              f"({m['name']})")
        if not ok:
            print(out[-2000:], file=sys.stderr)
    for target, cases in EXTRA_BASELINE.items():
        if target not in exes:
            continue
        for case in cases:
            extra = {"RECOMP_PB_COVERAGE": "0"} if case == "gate_off" else None
            rc, out, _ = run_case(exes[target], case, target, icd, extra)
            ok = rc == 0 and "PASS pbcov_" in out
            baseline_ok &= ok
            print(f"  {'PASS' if ok else 'FAIL'}  {target:<6} {case:<28} (smoke)")
            if not ok:
                print(out[-2000:], file=sys.stderr)
    if not baseline_ok:
        print("the frozen baseline does not pass; refusing to judge mutants", file=sys.stderr)
        return 1

    # ── Mutants ─────────────────────────────────────────────────────────────
    rows = []
    all_ok = True
    print("\nmutants (each must FAIL with its named semantic assertion)")
    print(f"{'mutant':<34} {'target':<7} {'case':<28} {'result':<10} assertion")
    for m in mutants:
        src = toolkit_copy / m["file"]
        pristine_src = pristine / m["file"]
        # Byte-for-byte: text-mode write_text would translate newlines on
        # Windows and make the restore hash differ for a reason unrelated to
        # the mutation.
        original = pristine_src.read_bytes()
        # The checkout is CRLF on Windows (core.autocrlf); anchors are written
        # with LF.  Match the file's own convention so a mutation is a content
        # change, not a line-ending change.
        nl = b"\r\n" if b"\r\n" in original else b"\n"
        old = m["old"].encode("utf-8").replace(b"\n", nl)
        new = m["new"].encode("utf-8").replace(b"\n", nl)

        count = original.count(old)
        if count != 1:
            rows.append((m, "BAD-MUTANT", f"anchor occurs {count} times"))
            all_ok = False
            print(f"{m['name']:<34} {m['target']:<7} {m['case']:<28} {'BAD-MUTANT':<10} "
                  f"anchor occurs {count} times")
            continue
        mutated = original.replace(old, new, 1)
        src.write_bytes(mutated)

        rc, out, elapsed = run([cmake, "--build", build_dir, "--config", "Release",
                                "--target", "jsrf_pbcov_stream_test" if m["target"] == "stream"
                                else "jsrf_pbcov_vk_test"],
                               env=configure_env, timeout=1800)
        if rc != 0:
            # A mutant that does not compile is not a semantic mutant.
            rows.append((m, "COMPILE-FAIL", out[-500:].replace("\n", " ")))
            all_ok = False
            print(f"{m['name']:<34} {m['target']:<7} {m['case']:<28} {'COMPILE-FAIL':<10} "
                  f"a mutation must stay compilable")
            src.write_bytes(original)
            continue

        rc, out, elapsed = run_case(exes[m["target"]], m["case"], m["target"], icd,
                                    m.get("env"))
        named = f"FAIL pbcov_{'stream' if m['target'] == 'stream' else 'vk'}[{m['case']}]: {m['expect']}"
        if rc == 0:
            verdict = "STILL-PASS"
        elif rc == 77:
            verdict = "SKIP"
        elif rc not in (0, 1):
            verdict = f"CRASH({rc})"
        elif "fixture/harness error" in out:
            verdict = "HARNESS-ERROR"
        elif named in out:
            verdict = "PASS"
        elif "FAIL pbcov_" in out:
            verdict = "WRONG-ASSERT"
        else:
            verdict = "NO-ASSERT"
        ok = verdict == "PASS"
        all_ok &= ok
        rows.append((m, verdict, ""))
        print(f"{m['name']:<34} {m['target']:<7} {m['case']:<28} {verdict:<10} "
              f"{m['expect'] if ok else ''}")
        if not ok:
            tail = [line for line in out.splitlines() if "FAIL pbcov_" in line or "error" in line.lower()]
            print("    " + ("; ".join(tail[-3:]) if tail else out[-300:].replace("\n", " ")),
                  file=sys.stderr)

        # Restore and prove the private copy is pristine again.
        src.write_bytes(original)
        if sha256(toolkit_copy / m["file"]) != shared_hashes[m["file"]]:
            print(f"restore failed for {m['file']}", file=sys.stderr)
            return 2
        # The shared checkout must be untouched by the mutation.
        now_head, now_status = checkout_state(toolkit)
        now_hashes = {rel: sha256(toolkit / rel) for rel in PROTECTED}
        if now_head != head or now_status != status or now_hashes != shared_hashes:
            print("the shared toolkit checkout changed while the harness ran", file=sys.stderr)
            return 2

    # ── Restored baseline must pass again ───────────────────────────────────
    print("\nrebuilding the restored frozen baseline ...")
    rc, out, elapsed = run([cmake, "--build", build_dir, "--config", "Release",
                            "--target"] + targets, env=configure_env, timeout=3600)
    restored_ok = rc == 0
    if not restored_ok:
        print(out[-4000:], file=sys.stderr)
    for target, cases in EXTRA_BASELINE.items():
        if target not in exes:
            continue
        for case in cases:
            extra = {"RECOMP_PB_COVERAGE": "0"} if case == "gate_off" else None
            rc, out, _ = run_case(exes[target], case, target, icd, extra)
            restored_ok &= rc == 0 and "PASS pbcov_" in out
    for m in mutants:
        rc, out, _ = baseline_case(m)
        restored_ok &= rc == 0 and "PASS pbcov_" in out
    print(f"  restored baseline: {'PASS' if restored_ok else 'FAIL'}")

    now_head, now_status = checkout_state(toolkit)
    now_hashes = {rel: sha256(toolkit / rel) for rel in PROTECTED}
    shared_ok = (now_head == head and now_status == status
                 and now_hashes == shared_hashes)
    print(f"  shared toolkit unchanged: {'PASS' if shared_ok else 'FAIL'}")

    conclusive = [m for m, verdict, _ in rows if verdict == "PASS"]
    print(f"\n{len(conclusive)}/{len(mutants)} mutants decided by their named assertion")
    print("inherently unobservable and therefore not mutated: the single-thread ordering of "
          "a record's payload writes against its `valid` bit, and a concurrent torn read; "
          "the observable equivalent (claimed-record immutability and report retention) is "
          "decided by the first-*-overwrite and report-reset mutants above.")
    if args.keep or not (all_ok and restored_ok and shared_ok):
        print(f"work area kept: {work}")
    else:
        shutil.rmtree(work, ignore_errors=True)
    return 0 if (all_ok and restored_ok and shared_ok) else 1


if __name__ == "__main__":
    sys.exit(main())
