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
    python -X utf8 scripts/bumpenv-q-controls.py --control texgen
    python -X utf8 scripts/bumpenv-q-controls.py --control q-divide --keep

The step-5 committed-stream gate (tests/test_bumpenv_stream.c, its Vulkan half
tests/test_bumpenv_stream_vk.c and the literals in tests/bumpenv_stream_cases.h)
adds six controls, each applied to the CPU executor AND the AOT HLSL where the
rule lives in both, and each run against the new focused stream cases:
matrix-swap (m01 <-> m10), unsigned-delta (no sign3), drop-lum (mode 7's
k = 1), stream-q-divide (the bump path divides by q again), wrong-source
(SET_SHADER_OTHER_STAGE_INPUT ignored, the previous stage used) and refusal-gt8
(the refusal threshold moved to > 8). The step-6 texgen gate
(tests/test_texgen_stream.c, tests/test_texgen_stream_vk.c and the literals in
tests/texgen_stream_cases.h) adds six more, each a mutation of the CPU
executor's fixed-function texgen block (which also builds the Vulkan batch's
coordinates): texmat-transpose, texgen-modelview-normal, texgen-ignored,
sphere-formula, eye-plane-object and texmat-enable-ignored. `--control texgen`
runs those six in one owned root. The window-clip gate (tests/test_wclip_stream.c,
tests/test_wclip_stream_vk.c, literals in tests/wclip_stream_cases.h) adds four:
wclip-xmax-exclusive, wclip-aa-ignored, wclip-exclusive-as-inclusive and
wclip-vk-ignored; `--control wclip` runs them. The LIN_X8R8G8B8 alias gate
(tests/test_x8alias_stream*.c, tests/x8alias_stream_cases.h) adds four more,
`--control x8`: x8-alias-identity-view, x8-self-identity-view,
x8-swizzle-dropped and x8-cpu-alpha-kept. Vulkan cases need the software
ICD, which the loader ignores in an elevated process, so from an elevated
session run this script through `just controls <name>` (scripts/run-limited.py).

The scratch directory defaults to a FRESH `tmp-bumpenv-controls-<token>` sibling
of the game checkout and must live outside both repos AND outside %TEMP%:
MSBuild's incremental build is unreliable under the temporary directory
(MSB8029), and a restore that silently reused a mutated object would then be
reported as a GREEN/FAIL verdict about the product instead of a harness bug. For
the same reason the restore half deletes the focused targets' objects and
executables before rebuilding and asserts the restored executables really
changed.

Ownership: `--workdir` names WHERE this invocation may create its scratch tree;
it does NOT grant ownership of a path that already exists. A `--workdir` that
exists at all (directory, file or symlink) is refused, untouched, with exit 2,
and a fresh default root is created with `mkdir(exist_ok=False)`. The root is
stamped with an ownership marker and its `(st_dev, st_ino)` identity, and it is
removed only when that marker, the resolved path and the identity all still
match. No pre-existing `toolkit-<control>/` or `build-<control>/` is ever
removed: one that already exists is a harness bug (exit 3), never a path to
delete. The forced-recompile cleanup does remove the focused targets
recursively, but only after the same ownership proof and an exact per-target
resolution check, and never by following a link. A failed stamp removes the
fresh root only while its recorded directory identity still matches, and leaves
a replaced one in place with a diagnostic. `--keep` leaves the owned root
in place and prints its path.

Exit status: 0 when every requested control was RED then GREEN as required,
1 when a control did not behave, 2 on a usage/setup error, 3 when the harness
itself is broken (e.g. a restored build produced an identical executable,
meaning the restored sources were never recompiled).
"""

import argparse
import hashlib
import os
import secrets
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolkit_path import toolkit_path

GAME = Path(__file__).resolve().parent.parent
TOOLKIT = toolkit_path()

# The production files the controls mutate (the step-4 repair's three, plus the
# shared refusal rule the refusal-gt8 control moves). Read-only here.
PRODUCTION = [
    "src/kernel/nv2a_pb_exec.c",
    "src/kernel/nv2a_backend.h",
    "src/video/nv2a_vk_backend.c",
    "src/video/shaders/nv2a_vk_rc.hlsl",
    "src/video/nv2a_vk_rc_coverage.h",
]

# ── The controls ────────────────────────────────────────────────────────────
# Each mutation is a list of exact textual replacements, each of which must
# match exactly once. `*_cases` is the set of focused cases that must fail, and
# `*_markers` the assertion text that must appear in each failure (a named
# assertion, never a crash or a skip). The step-4 q gate's cases are
# `cpu_cases`/`vk_cases`; the step-5 stream gate's are `stream_cases`
# (jsrf_bumpenv_stream_*) and `stream_vk_cases` (jsrf_bumpenv_stream_vk_*), with
# `stream_still_green`/`stream_vk_still_green` for the controls that must keep
# passing under the mutation.

# The step-4 repair's bump-path divide, reused by the stream gate's control.
Q_DIVIDE_EDITS = [
    # CPU executor: the repaired line, and the divide put back.
    ("src/kernel/nv2a_pb_exec.c",
     "rc_texel(t, 0, c[0] + dsdt[0], c[1] + dsdt[1], out);",
     "rc_texel(t, 0, c[0] / q + dsdt[0], c[1] / q + dsdt[1], out);"),
    # The AOT uber-shader: the same rule, in HLSL. The SPIR-V is rebuilt by
    # the build's own xbr_add_spirv rule; nothing is compiled at run time.
    # One replacement, so no new declaration is needed: the divide is spelled
    # inline exactly as the pre-repair source spelled it.
    ("src/video/shaders/nv2a_vk_rc.hlsl",
     "tx.SampleLevel(sm, float2(t.x + dsdt.x, t.y + dsdt.y) * ts.xy, 0)",
     "tx.SampleLevel(sm, float2(t.x / (t.w != 0.0 ? t.w : 1.0) + dsdt.x, "
     "t.y / (t.w != 0.0 ? t.w : 1.0) + dsdt.y) * ts.xy, 0)"),
]

CONTROLS = {
    "q-divide": {
        "title": "restore PROJECT2D's /q divide on the bump paths",
        "edits": Q_DIVIDE_EDITS,
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

    # ── The step-5 committed-stream gate's controls ─────────────────────────
    "matrix-swap": {
        "title": "transpose the bump matrix (m01 <-> m10) on both renderers",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "dsdt[0] = m[0] * du + m[2] * dv;",
             "dsdt[0] = m[0] * du + m[1] * dv;"),
            ("src/kernel/nv2a_pb_exec.c",
             "dsdt[1] = m[1] * du + m[3] * dv;",
             "dsdt[1] = m[2] * du + m[3] * dv;"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "dsdt.x = bm.x * du + bm.z * dv;",
             "dsdt.x = bm.x * du + bm.y * dv;"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "dsdt.y = bm.y * du + bm.w * dv;",
             "dsdt.y = bm.z * du + bm.w * dv;"),
        ],
        "stream_cases": ["mode6_asymmetric_matrix", "mode6_signed_du"],
        "stream_markers": [
            "stage 1 asymmetric matrix, du only",
            "stage 1 asymmetric matrix, dv only",
            "du byte 0x80 is -128/127, not -1",
        ],
        "stream_vk_cases": ["mode6_asymmetric_matrix", "mode6_signed_du"],
        "stream_vk_markers": [
            "stage 1 asymmetric matrix, du only",
            "stage 1 asymmetric matrix, dv only",
            "du byte 0x80 is -128/127, not -1",
        ],
    },
    "unsigned-delta": {
        "title": "read du/dv as unsigned UNORM (no sign3) on both renderers",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "du = rc_sign3(tprev[src][2]);", "du = tprev[src][2];"),
            ("src/kernel/nv2a_pb_exec.c",
             "dv = rc_sign3(tprev[src][1]);", "dv = tprev[src][1];"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "du = sign3(earlier[src].z);", "du = earlier[src].z;"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "dv = sign3(earlier[src].y);", "dv = earlier[src].y;"),
        ],
        "stream_cases": ["mode6_signed_du", "mode6_signed_dv"],
        "stream_markers": [
            "du byte 0x7F is +1", "du byte 0x81 is -1",
            "dv byte 0x7F is +1", "dv byte 0x81 is -1",
        ],
        "stream_vk_cases": ["mode6_signed_du", "mode6_signed_dv"],
        "stream_vk_markers": [
            "du byte 0x7F is +1", "du byte 0x81 is -1",
            "dv byte 0x7F is +1", "dv byte 0x81 is -1",
        ],
    },
    "drop-lum": {
        "title": "mode 7 ignores its luminance factor (k = 1) on both renderers",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "float k = s_gpu.rc.bump_scale[st] * lum + s_gpu.rc.bump_offset[st];",
             "float k = 1.0f;"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "s *= rc.bumpscl[st] * lum + rc.bumpofs[st];",
             "s *= 1.0;"),
        ],
        "stream_cases": ["mode7_luminance", "mode7_saturation", "mode7_stage3_source"],
        "stream_markers": [
            "stage 1 mode 7 multiplies every channel by scale*lum + offset",
            "mode 7 k = -0.5 reads zero through UNSIGNED_IDENTITY",
            "stage 3 mode 7 reads stage 1 when bits 20-23 = 1",
        ],
        "stream_vk_cases": ["mode7_luminance", "mode7_saturation",
                            "mode7_stage3_source"],
        "stream_vk_markers": [
            "stage 1 mode 7 multiplies every channel by scale*lum + offset",
            "mode 7 k = -0.5 reads zero through UNSIGNED_IDENTITY",
            "stage 3 mode 7 reads stage 1 when bits 20-23 = 1",
        ],
    },
    "stream-q-divide": {
        "title": "the bump path divides by q again (stream gate)",
        "edits": Q_DIVIDE_EDITS,
        "stream_cases": ["bump_q_ignored"],
        "stream_markers": [
            "stage 1 mode 6 at q = 2 samples (s, t) + dsdt",
            "stage 1 mode 7 at q = 2 keeps texel and luminance",
        ],
        "stream_vk_cases": ["bump_q_ignored"],
        "stream_vk_markers": [
            "stage 1 mode 6 at q = 2 samples (s, t) + dsdt",
            "stage 1 mode 7 at q = 2 keeps texel and luminance",
        ],
        # PROJECT2D itself is untouched and must keep dividing.
        "stream_still_green": ["project2d_q_divides"],
        "stream_vk_still_green": ["project2d_q_divides"],
    },
    "wrong-source": {
        "title": "ignore SET_SHADER_OTHER_STAGE_INPUT: read the previous stage",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "src = (s_gpu.rc.other_stage_input >> (st == 2 ? 16 : 20)) & 0xFu;",
             "src = (uint32_t)st - 1u;"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "src = (rc.misc.x >> (st == 2 ? 16 : 20)) & 0xF;",
             "src = st - 1;"),
        ],
        "stream_cases": ["mode6_stage2_source", "mode7_stage3_source"],
        "stream_markers": [
            "stage 2 reads stage 0 when OTHER_STAGE_INPUT bits 16-19 = 0",
            "stage 3 mode 7 reads stage 1 when bits 20-23 = 1",
        ],
        "stream_vk_cases": ["mode6_stage2_source", "mode7_stage3_source"],
        "stream_vk_markers": [
            "stage 2 reads stage 0 when OTHER_STAGE_INPUT bits 16-19 = 0",
            "stage 3 mode 7 reads stage 1 when bits 20-23 = 1",
        ],
    },
    "refusal-gt8": {
        "title": "move the refusal threshold to > 8 (mode 8 treated as bump)",
        "edits": [
            ("src/video/nv2a_vk_rc_coverage.h", "if (m > 7) {", "if (m > 8) {"),
            ("src/video/shaders/nv2a_vk_rc.hlsl",
             "if (m == 6 || m == 7) {", "if (m == 6 || m == 7 || m == 8) {"),
            ("src/kernel/nv2a_pb_exec.c",
             "case 6: case 7: {", "case 6: case 7: case 8: {"),
        ],
        "stream_cases": ["refusal_dot_modes"],
        "stream_markers": [
            "mode 8 at stage 1 is refused: the CPU stage reads zero",
        ],
        "stream_vk_cases": ["refusal_dot_modes"],
        "stream_vk_markers": [
            "vk refuses a stage mode 8..31 (rc_fallback, reason 1)",
        ],
    },
}

# ── The step-6 texgen gate's controls ────────────────────────────────────────
# Each mutates the one fixed-function texgen/matrix block in the CPU executor;
# the Vulkan half sees the same mutation because the executor builds the
# batch's coordinates there. `texgen_cases` are jsrf_texgen_stream_* and
# `texgen_vk_cases` jsrf_texgen_stream_vk_*, with *_still_green for cases the
# mutation must not touch.

TEXGEN_CONTROLS = {
    "texmat-transpose": {
        "title": "apply the texture matrix with its words as columns",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "out[i] = t[4 * i] * in[0] + t[4 * i + 1] * in[1]",
             "out[i] = t[i] * in[0] + t[4 + i] * in[1]"),
            ("src/kernel/nv2a_pb_exec.c",
             "+ t[4 * i + 2] * in[2] + t[4 * i + 3] * in[3];",
             "+ t[8 + i] * in[2] + t[12 + i] * in[3];"),
        ],
        "texgen_cases": ["texmat_asymmetric", "jsrf_observed_state"],
        "texgen_markers": [
            "asymmetric matrix on the attribute: words are rows",
            "asymmetric matrix after NORMAL_MAP: texgen, then rows",
            "JSRF state, normal N1",
        ],
        "texgen_still_green": ["mode_disable_passthrough", "mode_normal_map"],
    },
    "texgen-modelview-normal": {
        "title": "transform the normal by the model-view words, not the inverse",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "n[i] = imv[4 * i] * nrm[0] + imv[4 * i + 1] * nrm[1] + imv[4 * i + 2] * nrm[2];",
             "n[i] = mv[4 * i] * nrm[0] + mv[4 * i + 1] * nrm[1] + mv[4 * i + 2] * nrm[2];"),
        ],
        "texgen_cases": ["imv_nonuniform_scale"],
        "texgen_markers": [
            "non-uniform scale: the normal goes through the INVERSE words",
            "asymmetric inverse model-view: its words are rows too",
        ],
        "texgen_still_green": ["jsrf_observed_state", "mode_normal_map"],
    },
    "texgen-ignored": {
        "title": "texgen modes ignored: every component keeps the attribute",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "if (mode[0] || mode[1] || mode[2]) {", "if (0) {"),
        ],
        "texgen_cases": ["mode_normal_map", "jsrf_observed_state", "mode_sphere_map"],
        "texgen_markers": [
            "NORMAL_MAP S/T: (n.x, n.y)",
            "JSRF state, normal N1",
            "SPHERE_MAP S/T, viewer straight ahead",
        ],
    },
    "sphere-formula": {
        "title": "SPHERE_MAP drops its +0.5 bias",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "out[c] = m > 1e-8f ? r[c] / m + 0.5f : 0.5f;",
             "out[c] = m > 1e-8f ? r[c] / m : 0.5f;"),
        ],
        "texgen_cases": ["mode_sphere_map", "limitation_viewer_model"],
        "texgen_markers": [
            "SPHERE_MAP S/T, viewer straight ahead",
            "SPHERE_MAP S/T, off-axis viewer",
            "LIMITATION local viewer: EYE_POSITION does not move SPHERE_MAP",
        ],
        "texgen_still_green": ["mode_reflection_map", "mode_normal_map"],
    },
    "eye-plane-object": {
        "title": "EYE_LINEAR takes the plane against the object position",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "out[c] = pl[0] * e[0] + pl[1] * e[1] + pl[2] * e[2] + pl[3] * e[3];",
             "out[c] = pl[0] * pos[0] + pl[1] * pos[1] + pl[2] * pos[2] + pl[3] * pos[3];"),
        ],
        "texgen_cases": ["eye_plane_modelview", "mode_eye_linear"],
        "texgen_markers": [
            "EYE_LINEAR plane meets the eye-space position",
            "EYE_LINEAR S/T: plane . (MV pos)",
        ],
        "texgen_still_green": ["mode_object_linear"],
    },
    "texmat-enable-ignored": {
        "title": "the matrix-enable word is ignored: delivered words always apply",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "if (st < 2 && s_reg[(FFTG_MATRIX_ENABLE + 4u * (uint32_t)st) / 4]) {",
             "if (st < 2) {"),
        ],
        "texgen_cases": ["texmat_enable"],
        "texgen_markers": [
            "matrix enable 0: delivered words are not applied",
        ],
        "texgen_still_green": ["mode_normal_map", "jsrf_observed_state"],
    },
}
for _spec in TEXGEN_CONTROLS.values():
    # The same named assertions decide both halves.
    _spec["texgen_vk_cases"] = list(_spec["texgen_cases"])
    _spec["texgen_vk_markers"] = list(_spec["texgen_markers"])
    if "texgen_still_green" in _spec:
        _spec["texgen_vk_still_green"] = list(_spec["texgen_still_green"])
CONTROLS.update(TEXGEN_CONTROLS)

# The window-clip gate's controls. `wclip_cases` are jsrf_wclip_stream_* and
# `wclip_vk_cases` jsrf_wclip_stream_vk_*; each half names its own markers.
WCLIP_CONTROLS = {
    "wclip-xmax-exclusive": {
        "title": "treat xmax as exclusive (drop the rect's last column)",
        "edits": [
            ("src/kernel/nv2a_backend.h",
             "rx0 = xa * sx; rx1 = (xb + 1) * sx;",
             "rx0 = xa * sx; rx1 = xb * sx;"),
        ],
        "wclip_cases": ["single", "full"],
        "wclip_markers": ["inside xmax/ymax corner (41,30) is drawn",
                          "top-right (63,0) is drawn"],
        "wclip_vk_cases": ["single", "full"],
        "wclip_vk_markers": ["inside xmax/ymax corner (41,30) is drawn",
                             "top-right (63,0) is drawn"],
        "wclip_still_green": ["exclusive"],
        "wclip_vk_still_green": ["exclusive"],
    },
    "wclip-aa-ignored": {
        "title": "use logical window-clip pixels on an anti-aliased surface",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "uint32_t sx = s_gpu.aa_sx > 1.5f ? 2 : 1, sy = s_gpu.aa_sy > 1.5f ? 2 : 1;",
             "uint32_t sx = 1, sy = 1;"),
            ("src/video/nv2a_vk_backend.c",
             "nv2a_wclip_bounds(rs, s->aa_sx, s->aa_sy,",
             "nv2a_wclip_bounds(rs, 1, 1,"),
        ],
        "wclip_cases": ["aa2x2"],
        "wclip_markers": ["inside last real pixel (19,19) is drawn"],
        "wclip_vk_cases": ["aa2x2"],
        "wclip_vk_markers": ["inside last real pixel (19,19) is drawn"],
        "wclip_still_green": ["single"],
        "wclip_vk_still_green": ["single"],
    },
    "wclip-exclusive-as-inclusive": {
        "title": "ignore the window-clip type bit",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "r->wclip_exclusive = param & 1;",
             "r->wclip_exclusive = 0;"),
        ],
        "wclip_cases": ["exclusive"],
        "wclip_markers": ["inside centre (30,30) is not drawn"],
        "wclip_vk_cases": ["exclusive"],
        "wclip_vk_markers": ["outside xmin-1 (19,30) is drawn"],
        "wclip_still_green": ["single"],
        "wclip_vk_still_green": ["single"],
    },
    "wclip-vk-ignored": {
        "title": "the Vulkan back end never applies the window clip",
        "edits": [
            ("src/video/nv2a_vk_backend.c",
             "if (!vb_wclip(s, rs, &dr->scissor))",
             "if (0 && !vb_wclip(s, rs, &dr->scissor))"),
        ],
        "wclip_vk_cases": ["single", "aa2x2"],
        "wclip_vk_markers": ["outside xmax+1 (42,20) is background",
                             "outside (xmax+1)*2 (20,15) is background"],
        "wclip_still_green": ["single", "aa2x2"],
    },
}
CONTROLS.update(WCLIP_CONTROLS)

# The LIN_X8R8G8B8 alias gate's controls: `x8_cases` are jsrf_x8alias_stream_*,
# `x8_vk_cases` jsrf_x8alias_stream_vk_*.
X8_CONTROLS = {
    "x8-alias-identity-view": {
        "title": "an aliased LIN_X8R8G8B8 target samples its identity view",
        "edits": [
            ("src/video/nv2a_vk_backend.c",
             "return tx->color == 0x1E ? vb_image_view_one(&t->im) : t->im.view;",
             "return t->im.view;"),
        ],
        "x8_vk_cases": ["x8_ff", "x8_rc"],
        "x8_vk_markers": ["B upper-left triangle (4,4)"],
        "x8_vk_still_green": ["x8_self", "a8_ff"],
    },
    "x8-self-identity-view": {
        "title": "the self-sampling snapshot of a LIN_X8R8G8B8 target keeps its alpha",
        "edits": [
            ("src/video/nv2a_vk_backend.c",
             "return tx->color == 0x1E ? vb_image_view_one(&t->copy) : t->copy.view;",
             "return t->copy.view;"),
        ],
        "x8_vk_cases": ["x8_self"],
        "x8_vk_markers": ["A drawn half, upper-left triangle (4,4)"],
        "x8_vk_still_green": ["x8_ff", "x8_rc"],
    },
    "x8-swizzle-dropped": {
        "title": "the alpha-one view keeps the identity alpha swizzle",
        "edits": [
            ("src/video/nv2a_vk_backend.c",
             "vi.components.a = VK_COMPONENT_SWIZZLE_ONE;",
             "vi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;"),
        ],
        "x8_vk_cases": ["x8_ff", "x8_rc", "x8_self"],
        "x8_vk_markers": ["B upper-left triangle (4,4)",
                          "A drawn half, upper-left triangle (4,4)"],
        "x8_vk_still_green": ["a8_ff", "a8_rc"],
    },
    "x8-cpu-alpha-kept": {
        "title": "the CPU decoder keeps a LIN_X8R8G8B8 texel's stored alpha",
        "edits": [
            ("src/kernel/nv2a_pb_exec.c",
             "*argb = ((const uint32_t *)p)[u] | 0xFF000000u;",
             "*argb = ((const uint32_t *)p)[u];"),
        ],
        "x8_cases": ["x8_ff", "x8_rc"],
        "x8_markers": ["B upper-left triangle (4,4)"],
        "x8_still_green": ["a8_ff", "a8_rc"],
    },
}
CONTROLS.update(X8_CONTROLS)

# CTest prefixes for each case group, in the order the groups are run.
RED_GROUPS = [
    ("cpu_cases", "cpu_markers", "jsrf_bumpenv_q_cpu_"),
    ("vk_cases", "vk_markers", "jsrf_bumpenv_q_vk_"),
    ("stream_cases", "stream_markers", "jsrf_bumpenv_stream_"),
    ("stream_vk_cases", "stream_vk_markers", "jsrf_bumpenv_stream_vk_"),
    ("texgen_cases", "texgen_markers", "jsrf_texgen_stream_"),
    ("texgen_vk_cases", "texgen_vk_markers", "jsrf_texgen_stream_vk_"),
    ("wclip_cases", "wclip_markers", "jsrf_wclip_stream_"),
    ("wclip_vk_cases", "wclip_vk_markers", "jsrf_wclip_stream_vk_"),
    ("x8_cases", "x8_markers", "jsrf_x8alias_stream_"),
    ("x8_vk_cases", "x8_vk_markers", "jsrf_x8alias_stream_vk_"),
]
GREEN_GROUPS = [
    ("cpu_still_green", "jsrf_bumpenv_q_cpu_"),
    ("stream_still_green", "jsrf_bumpenv_stream_"),
    ("stream_vk_still_green", "jsrf_bumpenv_stream_vk_"),
    ("texgen_still_green", "jsrf_texgen_stream_"),
    ("texgen_vk_still_green", "jsrf_texgen_stream_vk_"),
    ("wclip_still_green", "jsrf_wclip_stream_"),
    ("wclip_vk_still_green", "jsrf_wclip_stream_vk_"),
    ("x8_still_green", "jsrf_x8alias_stream_"),
    ("x8_vk_still_green", "jsrf_x8alias_stream_vk_"),
]


# ── Helpers ─────────────────────────────────────────────────────────────────

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class HarnessError(Exception):
    """The harness itself is broken -- never a verdict about the product."""


# ── Owned work area ──────────────────────────────────────────────────────────
#
# --workdir names WHERE this invocation may put its private copies; it does not
# grant ownership of a path that already exists. Only a directory this
# invocation created (and stamped below) is ever removed, and only after its
# resolved path, directory identity and marker still match. A caller's
# pre-existing directory or file is refused before the first copy.

OWNERSHIP_MARKER = ".bumpenv-q-controls-owned"


class WorkAreaRefused(RuntimeError):
    """The --workdir path is not a path this invocation is allowed to own."""


class OwnedWorkArea:
    def __init__(self, path, game, toolkit):
        self.path = Path(path)
        self.resolved = self.path.resolve()
        self.game = Path(game).resolve()
        self.toolkit = Path(toolkit).resolve()
        self.token = secrets.token_hex(16)
        self.identity = None

    def stamp(self):
        """Record the directory identity, then write the ownership marker.

        The identity is recorded FIRST: if the marker write is what fails, the
        caller must still be able to tell whether the directory it is about to
        remove is the one it just created.
        """
        st = self.path.stat()
        self.identity = (st.st_dev, st.st_ino)
        (self.path / OWNERSHIP_MARKER).write_text(
            "bumpenv-q-controls owned work area\n"
            f"pid {os.getpid()}\n"
            f"token {self.token}\n",
            encoding="utf-8", newline="\n")

    def _same_directory(self):
        """True only while the path is still the very directory created here.

        Weaker than `_still_owned()` on purpose: it needs no ownership marker,
        because the marker write is exactly what can fail. Used before removing
        a root whose stamping failed.
        """
        try:
            if self.path.is_symlink():
                return False
            if self.path.resolve() != self.resolved:
                return False
            st = self.path.stat()
        except OSError:
            return False
        return (self.identity is not None
                and (st.st_dev, st.st_ino) == self.identity)

    def contains(self, path):
        """True only for a path inside this owned root (resolved)."""
        try:
            p = Path(path).resolve()
        except OSError:
            return False
        return p == self.resolved or self.resolved in p.parents

    def _still_owned(self):
        """True only for the exact, unchanged directory this run created."""
        try:
            if self.path.is_symlink():
                return False
            if self.path.resolve() != self.resolved:
                return False
            st = self.path.stat()
            if self.identity is None or (st.st_dev, st.st_ino) != self.identity:
                return False
            marker = self.path / OWNERSHIP_MARKER
            if marker.is_symlink() or not marker.is_file():
                return False
            if self.token not in marker.read_text(encoding="utf-8",
                                                  errors="replace"):
                return False
        except OSError:
            return False
        # Never a filesystem root ...
        if self.resolved.parent == self.resolved:
            return False
        if self.resolved.anchor and self.resolved == Path(self.resolved.anchor):
            return False
        # ... nor the game/toolkit root, nor an ancestor of either.
        for guarded in (self.game, self.toolkit):
            if self.resolved == guarded or guarded in self.resolved.parents:
                return False
        return True

    def release(self, keep):
        """Remove the owned root (or, with --keep, report where it is)."""
        if keep:
            print(f"work area kept: {self.path}")
            return True
        if not self._still_owned():
            print(f"work area NOT removed (ownership changed): {self.path}",
                  file=sys.stderr)
            return False
        try:
            shutil.rmtree(self.path)
        except OSError as exc:
            print(f"work area NOT removed ({exc}): {self.path}", file=sys.stderr)
            return False
        return True


def _validate_root(root, game, toolkit):
    """Refuse a resolved root that must never be owned; called BEFORE mkdir."""
    root = Path(root).resolve()
    for guarded in (game, toolkit):
        if root == guarded or root.is_relative_to(guarded):
            raise WorkAreaRefused(
                f"refusing to work inside a repo: {root}")
        if guarded.is_relative_to(root):
            raise WorkAreaRefused(
                f"refusing to own an ancestor of a repo: {root}")
    temp_root = Path(tempfile.gettempdir()).resolve()
    if root == temp_root or root.is_relative_to(temp_root):
        raise WorkAreaRefused(
            f"refusing to work under the temporary directory: {root}\n"
            f"  MSBuild will not trust an incremental build there (MSB8029), "
            f"which can leave a stale object behind a restore.")
    if root.parent == root or (root.anchor and root == Path(root.anchor)):
        raise WorkAreaRefused(f"refusing to own a filesystem root: {root}")


def acquire_work_area(args, game, toolkit):
    """Create and stamp a fresh work area; never remove a caller's path."""
    if args.workdir is not None:
        given = Path(args.workdir)
        if given.is_symlink() or given.exists():
            raise WorkAreaRefused(
                f"--workdir {given} already exists; refusing to modify or remove "
                f"a path this invocation did not create. Choose a new path.")
        root = given.resolve()
        _validate_root(root, game, toolkit)
        try:
            root.mkdir(parents=True, exist_ok=False)
        except OSError as exc:
            raise WorkAreaRefused(
                f"could not create --workdir {root}: {exc}") from exc
    else:
        # NOT tempfile.mkdtemp(): MSBuild refuses to trust an incremental build
        # under %TEMP% (MSB8029), and a build it will not trust incrementally is
        # exactly how a stale mutated object survives a restore. A fresh unique
        # sibling of the game checkout keeps the build out of %TEMP% and out of
        # both repos.
        root = (Path(game).parent
                / ("tmp-bumpenv-controls-" + secrets.token_hex(8))).resolve()
        _validate_root(root, game, toolkit)
        try:
            root.mkdir(parents=True, exist_ok=False)
        except OSError as exc:
            raise WorkAreaRefused(
                f"could not create the scratch root {root}: {exc}") from exc
    owned = OwnedWorkArea(root, game, toolkit)
    try:
        owned.stamp()
    except OSError as exc:
        if args.keep:
            print(f"work area kept: {root}")
        elif owned._same_directory():
            shutil.rmtree(root, ignore_errors=True)
        else:
            # The root is no longer (or cannot be proven to still be) the
            # directory created above, so it is not ours to remove.
            print(f"work area NOT removed (ownership changed): {root}",
                  file=sys.stderr)
        raise WorkAreaRefused(
            f"could not record ownership of {root}: {exc}") from exc
    return owned


TARGETS = ["jsrf_bumpenv_q_cpu_test", "jsrf_bumpenv_q_vk_test",
           "jsrf_bumpenv_stream_test", "jsrf_bumpenv_stream_vk_test",
           "jsrf_texgen_stream_test", "jsrf_texgen_stream_vk_test",
           "jsrf_wclip_stream_test", "jsrf_wclip_stream_vk_test",
           "jsrf_x8alias_stream_test", "jsrf_x8alias_stream_vk_test"]


def _is_link(path):
    """True for a symlink, or for a Windows junction (Python 3.12+)."""
    if path.is_symlink():
        return True
    is_junction = getattr(path, "is_junction", None)
    return bool(is_junction and is_junction())


def force_rebuild(build_dir, targets=TARGETS, owned=None):
    """Delete the focused targets' objects and executables before a build.

    Both test translation units #include production sources that CMake does
    NOT list among their sources: tests/test_bumpenv_q_cpu.c includes
    src/kernel/nv2a_pb_exec.c and tests/test_bumpenv_q_vk.c includes
    src/video/nv2a_vk_backend.c. MSBuild's compiler include-tracking is
    therefore the ONLY thing that would notice a change in those files, and
    that tracking is exactly what is unreliable here (it is what MSB8029
    disables under %TEMP%). Removing the objects makes the recompile
    unconditional, so a stale binary cannot masquerade as a RED or a GREEN.

    The cleanup is NOT a set of file-level unlinks: every focused `*.dir` is a
    private object tree and is removed RECURSIVELY, along with the focused
    executables and the uber-shader's generated SPIR-V. So, before the first
    removal, this function requires a supplied ownership proof whose current
    identity, resolved path and ownership marker still match -- ``owned=None``
    is a refusal, not a bypass -- keeps the build directory inside that root,
    and checks EVERY exact deletion target: a `*.dir` that is itself a
    symlink/junction, or any target that resolves outside the owned root, is a
    harness bug, never a path to clean. All targets are validated first and
    removed afterwards, so a refusal leaves every target intact.

    That is a sequential invariant, not race freedom: validation and removal
    are two steps, and a directory swapped in between them is not detected. A
    failed proof is never compensated for with unconditional removal.
    """
    if owned is None:
        raise HarnessError(
            f"refusing to clean {build_dir}: no owned work area was supplied, "
            f"so there is no ownership proof for the deletion targets; "
            f"nothing was removed")
    if not owned._still_owned():
        raise HarnessError(
            f"refusing to clean {build_dir}: ownership proof failed for the "
            f"work area {owned.path} -- it is no longer the directory this run "
            f"created (its identity, resolved path or ownership marker "
            f"changed); nothing was removed")
    if not owned.contains(build_dir):
        raise HarnessError(
            f"refusing to clean {build_dir}: it is outside the owned work area "
            f"{owned.path}")
    # Plan every deletion FIRST, so the ownership checks below and the removals
    # further down act on exactly the same paths. The order matches the
    # original per-target removal order, so the printout is unchanged.
    planned = []
    for t in targets:
        d = build_dir / f"{t}.dir"
        if d.exists() or _is_link(d):
            planned.append(("tree", d))
        for exe in (build_dir / "Release").glob(f"{t}.exe"):
            planned.append(("file", exe))
    # The uber-shader's SPIR-V is produced by xbr_add_spirv as a custom
    # command keyed on the HLSL's timestamp. A restored HLSL copied back with
    # `copy2` keeps the SHARED file's older mtime, so dxc would not rerun and
    # the VK half would keep executing the mutated shader while the source on
    # disk looks pristine. Deleting the generated outputs makes the
    # regeneration unconditional, exactly like deleting the test objects.
    spirv = build_dir / "xboxrecomp" / "src" / "video" / "spirv"
    for gen in sorted(spirv.glob("xbr_nv2a_vk_rc_*")):
        planned.append(("file", gen))

    for kind, target in planned:
        if kind == "tree" and _is_link(target):
            raise HarnessError(
                f"refusing to clean {target}: it is a symlink or junction, not "
                f"a real directory inside the owned work area {owned.path}; "
                f"nothing was removed")
        try:
            resolved = target.resolve()
        except OSError as exc:
            raise HarnessError(
                f"refusing to clean {target}: it could not be resolved ({exc}); "
                f"nothing was removed") from exc
        if not owned.contains(resolved):
            raise HarnessError(
                f"refusing to clean {target}: it resolves to {resolved}, which "
                f"is outside the owned work area {owned.path}; "
                f"nothing was removed")

    removed = []
    for kind, target in planned:
        try:
            if kind == "tree":
                shutil.rmtree(target)
                removed.append(target.name + "/")
            else:
                target.unlink()
                removed.append(target.name)
        except OSError as exc:
            raise HarnessError(
                f"could not clean {target}: {exc}\n"
                f"  the forced recompile did not complete, so no RED/GREEN "
                f"verdict from this run is sound") from exc
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

def run_control(name, workdir, shared_before, owned=None):
    spec = CONTROLS[name]
    print(f"\n{'=' * 78}\ncontrol: {name} -- {spec['title']}\n{'=' * 78}")

    copy = workdir / f"toolkit-{name}"
    build = workdir / f"build-{name}"
    # These live INSIDE the owned root and this invocation creates them, so a
    # pre-existing one means the harness (or a caller) put something there that
    # this run did not create. That is a harness bug, never a path to delete.
    for d in (copy, build):
        if owned is not None and not owned.contains(d):
            raise HarnessError(
                f"{name}: {d} is outside the owned work area {owned.path}")
        if d.exists() or d.is_symlink():
            raise HarnessError(
                f"{name}: {d} already exists inside the owned work area; refusing "
                f"to remove a path this run did not create")
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
    force_rebuild(build, owned=owned)
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

    for cases, markers, prefix in RED_GROUPS:
        for case in spec.get(cases, []):
            ok &= expect_red(build, case, spec.get(markers, []), env, prefix)
    for cases, prefix in GREEN_GROUPS:
        for case in spec.get(cases, []):
            ok &= expect_green(build, case, env, prefix)

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
    force_rebuild(build, owned=owned)
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

    # Restored: every case the mutation turned RED, and every control that had
    # to stay green, must pass again.
    for cases, _markers, prefix in RED_GROUPS:
        for case in spec.get(cases, []):
            ok &= expect_green(build, case, env, prefix)
    for cases, prefix in GREEN_GROUPS:
        for case in spec.get(cases, []):
            ok &= expect_green(build, case, env, prefix)
    return bool(ok)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--control", default="all",
                    choices=sorted(CONTROLS) + ["all", "texgen", "wclip", "x8"])
    ap.add_argument("--workdir", default=None,
                    help="private scratch directory to CREATE; it must not exist "
                         "yet (default: a fresh tmp-bumpenv-controls-<token> "
                         "sibling of the game checkout, outside both repos and "
                         "outside %%TEMP%%)")
    ap.add_argument("--keep", action="store_true",
                    help="keep the owned scratch directory and print its path")
    args = ap.parse_args(argv)

    if not (TOOLKIT / "CMakeLists.txt").is_file():
        print(f"not a toolkit checkout: {TOOLKIT}", file=sys.stderr)
        return 2

    try:
        owned = acquire_work_area(args, GAME, TOOLKIT)
    except WorkAreaRefused as exc:
        print(f"REFUSED: {exc}", file=sys.stderr)
        return 2
    print(f"private scratch directory: {owned.path}")

    keep = args.keep
    try:
        try:
            return _run_controls(args, owned)
        except HarnessError as exc:
            print(f"\nHARNESS ERROR: {exc}", file=sys.stderr)
            print("This is a bug in the harness, not a verdict about the product: "
                  "no RED/GREEN claim from this run is sound.", file=sys.stderr)
            # Keep the tree: the diagnosis needs the copies and build logs.
            keep = True
            return 3
    finally:
        owned.release(keep)


def _run_controls(args, owned):
    workdir = owned.path
    shared_before = production_hashes()
    print("\nshared toolkit production files, BEFORE:")
    for rel, h in shared_before.items():
        print(f"  {h}  {rel}")

    if args.control == "all":
        names = sorted(CONTROLS)
    elif args.control == "texgen":
        names = sorted(TEXGEN_CONTROLS)
    elif args.control == "wclip":
        names = sorted(WCLIP_CONTROLS)
    elif args.control == "x8":
        names = sorted(X8_CONTROLS)
    else:
        names = [args.control]
    ok = True
    for name in names:
        ok &= run_control(name, workdir, shared_before, owned)

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
