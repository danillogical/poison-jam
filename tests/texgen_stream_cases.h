/* The D4 step-6 fixed-function texgen / texture-matrix deciding gate: the cases
 * and their literal expectations, shared by the committed-stream CPU half
 * (tests/test_texgen_stream.c) and the Vulkan half (tests/test_texgen_stream_vk.c).
 *
 * Each half #includes this file and supplies three functions:
 *   check_draw(d)                  submit d's stream, observe, compare to the literal
 *   check_rejected(d, m, p, what)  d's stream plus (m, p) must be refused by the walk
 *   case_note(fmt, ...)            a line of context on stdout
 * so the SAME literal is checked by each renderer independently; agreement
 * between them is never the test.
 *
 * Authored by a worker distinct from the production author (toolkit cf185d0).
 * No expectation is read from a production helper or a probe: each is derived
 * below by hand from the fixture's own images (texgen_stream_fixture.c) and the
 * hardware rule stated next.
 *
 * ── The hardware rule used (xemu 478b4f49 read for facts only) ───────────────
 * R1 Matrix words. Every transform matrix -- model-view, inverse model-view,
 *    composite, texture -- loads its method word k into constant row k/4,
 *    component k%4 (pgraph.c:1748-1784, SET_MODEL_VIEW_MATRIX ...
 *    SET_TEXTURE_MATRIX; the texgen planes the same way, pgraph.c:1799-1807).
 *    GLSL builds mat4(c[r], c[r+1], c[r+2], c[r+3]) (glsl/common.h:94-96), so
 *    each constant row is a COLUMN of the GLSL matrix, and every product is a
 *    row vector times it (vsh-ff.c:171-176 `position * modelViewMat0`,
 *    `vec4(normal, 0.0) * invModelViewMat0`; vsh-ff.c:249 `oT = oT * texMat`).
 *    (v * M)[j] = dot(v, column j), so out[j] = sum_i W[4j+i] * in[i]: the
 *    words, in delivery order, are the ROWS of the matrix applied as W * in.
 *    MIT cross-check, reJSRF v0.0.28 (nworkers/reJSRF PR #65, design doc
 *    20261015-i064-texgen.md): JSRF's stage-0 transform seen through D3D8 is
 *    "scale (0.5, -0.5), fourth row (0.5, 0.5)" in D3D's row-vector convention,
 *    i.e. u = 0.5*nx + 0.5, v = -0.5*ny + 0.5 (COUNT2). The NV2A words observed
 *    for that state (jsrf_observed_state below) read as rows give exactly
 *    s = 0.5*nx + 0.5*q, t = -0.5*ny + 0.5*q with q = 1; read as columns they
 *    would give s = 0.5*nx, t = -0.5*ny and q = 0.5*nx + 0.5*ny + q.
 * R2 Eye position. e = MV * pos, all four components (vsh-ff.c:171,
 *    skinning off: model-view matrix 0).
 * R3 Normal. n = IMV * (nrm, 0), xyz: n[j] = sum_{i<3} W_imv[4j+i] * nrm[i]
 *    (vsh-ff.c:174-176). The INVERSE model-view words are used, never the
 *    model-view's (the runtime supplies them so this is the inverse transpose;
 *    reJSRF likewise transforms normals by the inverse transpose of World*View).
 *    Only rows 0-2 matter (w = 0), which is why 12 words suffice. n is
 *    normalised only when SET_NORMALIZATION_ENABLE (0x3A4) is set (pgraph.c:1628,
 *    vsh-ff.c:178-180), AFTER the transform, and NORMAL_MAP, SPHERE_MAP and
 *    REFLECTION_MAP all use that n.
 * R4 Linear modes. EYE_LINEAR = plane . e (vsh-ff.c:196-198); OBJECT_LINEAR =
 *    plane . pos (vsh-ff.c:200-202). Plane words are used as delivered, in x, y,
 *    z, w order: stage s's S plane at 0x840 + 0x40*s, T at +0x10, R at +0x20.
 * R5 Map modes. SPHERE_MAP (S and T only): u = normalize(e.xyz) -- the viewer at
 *    the eye-space origin, no w divide -- r = reflect(u, n) = u - 2(n.u)n,
 *    m = 2|r + (0,0,1)|, s = r.x/m + 0.5, t = r.y/m + 0.5 (vsh-ff.c:204-223).
 *    REFLECTION_MAP: s, t, r = r.x, r.y, r.z (vsh-ff.c:224-233). NORMAL_MAP:
 *    n.x, n.y, n.z (vsh-ff.c:234-237). DISABLE: the attribute's own component
 *    (vsh-ff.c:192-194). Each component is chosen independently.
 *    For u = (0,0,-1) and a unit n with n.z > 0 the sphere formula reduces to
 *    s = 0.5 + n.x/2, t = 0.5 + n.y/2 (r + (0,0,1) = 2 n.z n, so m = 4 n.z).
 * R6 Order and q. Texgen first, then an enabled texture matrix transforms all
 *    four components (vsh-ff.c:246-252 follows the texgen loop). TEXGEN_Q is not
 *    admitted, so q is always the attribute's w (1 when the array is absent or
 *    has fewer than four components) and the matrix transforms it. The pixel
 *    stage divides s, t by q only for PROJECT2D (xemu psh.c textureProj), which
 *    every draw here uses.
 * R7 Texel scaling. A swizzled texture is addressed in [0, 1] (texel =
 *    floor(8s)); a linear (rect) texture in texels (texel = floor(s)). A
 *    generated coordinate is a coordinate like any other.
 * R8 Sampling. Nearest, clamp-to-edge, level 0. Every coordinate below lands
 *    at least 0.14 texel (most at least 0.25) from a texel edge, so neither
 *    renderer's rounding can move it.
 *
 * ── The fixture (texgen_stream_fixture.c) ──────────────────────────────────────
 * Two triangles cover the 64x64 surface; every vertex is pos = (x, y, 0.5, 1)
 * with x, y in {0, 64}, and every vertex of a draw has the same normal and
 * texture-coordinate attribute. Planes and model-views below give x and y
 * zero weight wherever a generated value is used, so every covered pixel
 * carries the same coordinate; the halves read three pixels.
 * Images, 8x8: T(x, y) = A 0xFF, R 0x10 + 0x20*x, G 0x10 + 0x20*y, B by image:
 *    A (swizzled) B 0x80, B (swizzled) B 0x40, C (swizzled) B 0xC0, AL (linear
 *    copy of A, pitch 32). So:
 *        x or y:  0    1    2    3    4    5    6    7
 *        byte:   0x10 0x30 0x50 0x70 0x90 0xB0 0xD0 0xF0
 *    and A(x, y) = 0xFF000000 | R<<16 | G<<8 | 0x80, e.g. A(5,2) = 0xFFB05080.
 *    Texel x's centre is (x + 0.5)/8: 0.5/8 = 0.0625, 2.5/8 = 0.3125, 5.5/8 = 0.6875.
 * The final combiner emits one texture register (the stage the draw observes).
 * The CPU fixed-function rasteriser has no combiners: it writes stage 0's texel
 * straight to the pixel, or the diffuse colour 0xFF336699 when stage 0 is
 * untextured. Where that differs from the hardware rule the draw carries a
 * separate, labelled CPU literal (`cpu_differs`); the Vulkan half always checks
 * the hardware literal.
 */
#ifndef JSRF_TEXGEN_STREAM_CASES_H
#define JSRF_TEXGEN_STREAM_CASES_H

#include "texgen_stream_fixture.h"

#include <stddef.h>
#include <string.h>

/* SET_SHADER_STAGE_PROGRAM: five bits per stage; 1 is PROJECT2D. */
#define PROG(m0, m1, m2, m3) ((uint32_t)(m0) | ((uint32_t)(m1) << 5) \
                              | ((uint32_t)(m2) << 10) | ((uint32_t)(m3) << 15))
#define TG_EYE   0x2400u
#define TG_OBJ   0x2401u
#define TG_SPH   0x2402u
#define TG_NRM   0x8511u
#define TG_REF   0x8512u
#define TEX0(k)  { (k), TGF_TEX_NONE, TGF_TEX_NONE }
#define CPU_DIFFUSE_NOTE "LIMITATION cpu: the fixed-function rasteriser samples stage 0 only, " \
                         "so an untextured stage 0 writes the diffuse 0xFF336699"

static void check_draw(const TgDraw *d);
static void check_rejected(const TgDraw *d, uint32_t method, uint32_t param,
                           const char *what);
static void case_note(const char *fmt, ...);

/* ── The matrices and planes, in DELIVERY order (four words per row) ──────── */

/* R_TO_S: s' = r, t' = t, r' = s, q' = q -- makes the R component visible. */
static const float k_r_to_s[16] = { 0,0,1,0,  0,1,0,0,  1,0,0,0,  0,0,0,1 };
/* The JSRF-observed stage-0 words: 3F000000,0,0,3F000000, 0,BF000000,0,3F000000,
 * 0,0,0,0, 0,0,0,3F800000. Rows: s' = 0.5s + 0.5q, t' = -0.5t + 0.5q, r' = 0, q' = q. */
static const float k_jsrf[16] = { 0.5f,0,0,0.5f,  0,-0.5f,0,0.5f,  0,0,0,0,  0,0,0,1.0f };
/* An asymmetric matrix: s' = t + 0.25q, t' = 0.5s, r' = 0, q' = q. Its transpose
 * would give s'' = 0.5t, t'' = s, q'' = 0.25s + q. */
static const float k_asym[16] = { 0,1,0,0.25f,  0.5f,0,0,0,  0,0,0,0,  0,0,0,1 };
/* q' = 2q: a projective row. */
static const float k_q2[16] = { 1,0,0,0,  0,1,0,0,  0,0,0,0,  0,0,0,2 };
/* s' = s + 0.125q, t' = t + 0.25q, q' = 0.5q: shows which q went in. */
static const float k_qattr[16] = { 1,0,0,0.125f,  0,1,0,0.25f,  0,0,0,0,  0,0,0,0.5f };

/* Model-views (16 words) and inverse model-views (12 words, rows 0-2). */
static const float k_mv_t[16] = { 1,0,0,0,  0,1,0,0,  0,0,1,0.25f,  0,0,0,1 };        /* e.z = 0.75 */
static const float k_mv_ep[16] = { 1,0,0,0,  0,1,0,0,  0,0,0.5f,0.375f,  0,0,0,1 };   /* e.z = 0.625 */
static const float k_mv_e1[16] = { 0,0,0,0,  0,0,0,0,  0,0,0,-2.0f,  0,0,0,1 };       /* e = (0,0,-2) */
static const float k_mv_e2[16] = { 0,0,0,1.5f,  0,0,0,0.5f,  0,0,0,-1.5f,  0,0,0,1 }; /* e = (1.5,0.5,-1.5) */
static const float k_mv_e3[16] = { 0,0,0,0.5f,  0,0,0,0.75f,  0,0,0,-1.5f,  0,0,0,1 };/* e = (0.5,0.75,-1.5) */
/* Non-uniform scale diag(2, 0.5, 1): inverse diag(0.5, 2, 1), which differs
 * from the transpose (= itself). */
static const float k_mv_s[16] = { 2,0,0,0,  0,0.5f,0,0,  0,0,1,0,  0,0,0,1 };
static const float k_imv_s[12] = { 0.5f,0,0,0,  0,2,0,0,  0,0,1,0 };
/* e.x = 2 pos.y, e.y = 4 pos.x: A = [[0,2,0],[4,0,0],[0,0,1]],
 * A^-1 = [[0,0.25,0],[0.5,0,0],[0,0,1]], its transpose = the IMV rows below. */
static const float k_mv_r[16] = { 0,2,0,0,  4,0,0,0,  0,0,1,0,  0,0,0,1 };
static const float k_imv_r[12] = { 0,0.5f,0,0,  0.25f,0,0,0,  0,0,1,0 };
static const float k_imv_h[12] = { 0.5f,0,0,0,  0,1,0,0,  0,0,1,0 };                  /* diag(0.5,1,1) */

/* Texgen planes: S, T, R, Q (16 words). */
static const float k_p_eye[16] = { 0,0,1,-0.0625f,  0,0,0.5f,-0.0625f,  0,0,1,-0.5625f,  0,0,0,0 };
static const float k_p_obj[16] = { 0,0,0.25f,0.6875f,  0,0,0.5f,0.0625f,  0,0,1,0.4375f,  0,0,0,0 };
static const float k_p_ep[16]  = { 0,0,1,0.0625f,  0,0,-1,0.9375f,  0,0,0,0,  0,0,0,0 };
static const float k_p_s1[16]  = { 0,0,0,0.8125f,  0,0,1,-0.1875f,  0,0,0,0,  0,0,0,0 };
static const float k_p_c1[16]  = { 0,0,0,0.6875f,  0,0,0,0.3125f,  0,0,0,0,  0,0,0,0 };
static const float k_p_c2[16]  = { 0,0,0,5.5f,  0,0,0,2.5f,  0,0,0,0,  0,0,0,0 };
static const float k_p_q[16]   = { 0,0,0,0.3125f,  0,0,0,0.3125f,  0,0,0,0,  0,0,0,0 };

/* Unit normals: N_U = (0.3125, 0.6875, sqrt(1 - 0.0977 - 0.4727) = 0.6555055);
 * N1 = (0.375, -0.625, 0.6846532); N2 = (-0.625, 0.375, 0.6846532);
 * N3 = (-0.375, -0.125, sqrt(0.84375) = 0.9185587). */
#define N_U { 0.3125f, 0.6875f, 0.6555055f }
#define N_1 { 0.375f, -0.625f, 0.6846532f }
#define N_2 { -0.625f, 0.375f, 0.6846532f }
#define N_3 { -0.375f, -0.125f, 0.9185587f }

/* ── EYE_LINEAR (R2, R4) ───────────────────────────────────────────────────────
 * MV k_mv_t: e = (x, y, 0.5 + 0.25, 1) = (x, y, 0.75, 1). Planes k_p_eye:
 *   S (0,0,1,-0.0625):   0.75 - 0.0625 = 0.6875 -> 5.5 -> x 5
 *   T (0,0,0.5,-0.0625): 0.375 - 0.0625 = 0.3125 -> 2.5 -> y 2   -> A(5,2) = 0xFFB05080
 *   R (0,0,1,-0.5625):   0.75 - 0.5625 = 0.1875; with k_r_to_s s' = r = 0.1875
 *                        -> 1.5 -> x 1, t' = 0.3125 -> y 2       -> A(1,2) = 0xFF305080
 * (Planes against the object position, z 0.5: S 0.4375, T 0.1875 -> A(3,1).) */
static const TgDraw k_eye[] = {
    { .what = "EYE_LINEAR S/T: plane . (MV pos) -> A(5,2)", .ffp = 1,
      .texgen = { { TG_EYE, TG_EYE, 0 } }, .plane = { k_p_eye }, .mv = k_mv_t,
      .normal = { 0, 0, 1 }, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB05080u, .want_tc = { 0.6875f, 0.3125f, 1.0f } },
    { .what = "EYE_LINEAR R reaches s through the matrix -> A(1,2)", .ffp = 1,
      .texgen = { { TG_EYE, TG_EYE, TG_EYE } }, .plane = { k_p_eye }, .mv = k_mv_t,
      .mat_enable = { 1 }, .mat = { k_r_to_s }, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF305080u, .want_tc = { 0.1875f, 0.3125f, 1.0f } },
};

/* ── OBJECT_LINEAR (R4): the model-view does not enter ─────────────────────────
 * Same MV k_mv_t (e.z 0.75, ignored); pos = (x, y, 0.5, 1). Planes k_p_obj:
 *   S (0,0,0.25,0.6875): 0.125 + 0.6875 = 0.8125 -> 6.5 -> x 6
 *   T (0,0,0.5,0.0625):  0.25 + 0.0625 = 0.3125 -> 2.5 -> y 2    -> A(6,2) = 0xFFD05080
 *   R (0,0,1,0.4375):    0.5 + 0.4375 = 0.9375; k_r_to_s s' = 0.9375 -> 7.5
 *                        -> x 7, y 2                            -> A(7,2) = 0xFFF05080 */
static const TgDraw k_obj[] = {
    { .what = "OBJECT_LINEAR S/T: plane . pos -> A(6,2)", .ffp = 1,
      .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_obj }, .mv = k_mv_t,
      .normal = { 0, 0, 1 }, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFD05080u, .want_tc = { 0.8125f, 0.3125f, 1.0f } },
    { .what = "OBJECT_LINEAR R reaches s through the matrix -> A(7,2)", .ffp = 1,
      .texgen = { { TG_OBJ, TG_OBJ, TG_OBJ } }, .plane = { k_p_obj }, .mv = k_mv_t,
      .mat_enable = { 1 }, .mat = { k_r_to_s }, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFF05080u, .want_tc = { 0.9375f, 0.3125f, 1.0f } },
};

/* ── SPHERE_MAP (R5) ───────────────────────────────────────────────────────────
 * S1 MV k_mv_e1: e = (0,0,-2) at every vertex, u = (0,0,-1). n = (2,-3,6)/7:
 *    s = 0.5 + (2/7)/2 = 0.642857 -> 5.143 -> x 5; t = 0.5 - (3/7)/2 = 0.285714
 *    -> 2.286 -> y 2                                            -> A(5,2) = 0xFFB05080
 *    (no +0.5: s 1/7 -> A(1,0); m without the 2: s = n.x + 0.5 -> A(6,0))
 * S2 MV k_mv_e2: e = (1.5, 0.5, -1.5), |e| = sqrt(4.75), u = (3,1,-3)/sqrt(19).
 *    n = (0,0,1): n.u = -3/sqrt19, r = u + (6/sqrt19)(0,0,1) = (3,1,3)/sqrt19;
 *    |r + (0,0,1)|^2 = (9 + 1 + (3 + sqrt19)^2)/19 = 2 + 6/sqrt19 = 3.376494,
 *    m = 2*1.837524 = 3.675048;
 *    s = (3/sqrt19)/m + 0.5 = 0.688247/3.675048 + 0.5 = 0.687276 -> 5.498 -> x 5
 *    t = (1/sqrt19)/m + 0.5 = 0.229416/3.675048 + 0.5 = 0.562425 -> 4.499 -> y 4
 *                                                               -> A(5,4) = 0xFFB09080
 *    (u = (0,0,-1), an infinite viewer, gives s = t = 0.5; e not normalised
 *    gives s = 0.7535 -> x 6.) */
static const TgDraw k_sphere[] = {
    { .what = "SPHERE_MAP S/T, viewer straight ahead: 0.5 + n/2 -> A(5,2)", .ffp = 1,
      .texgen = { { TG_SPH, TG_SPH, 0 } }, .mv = k_mv_e1,
      .normal = { 2.0f / 7.0f, -3.0f / 7.0f, 6.0f / 7.0f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB05080u, .want_tc = { 0.642857f, 0.285714f, 1.0f } },
    { .what = "SPHERE_MAP S/T, off-axis viewer u = normalize(e) -> A(5,4)", .ffp = 1,
      .texgen = { { TG_SPH, TG_SPH, 0 } }, .mv = k_mv_e2, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB09080u, .want_tc = { 0.687276f, 0.562425f, 1.0f } },
};

/* ── NORMAL_MAP (R3, R5) ───────────────────────────────────────────────────────
 * IMV identity, normalisation off, n = N_U = (0.3125, 0.6875, 0.6555055):
 *   s 0.3125 -> 2.5 -> x 2, t 0.6875 -> 5.5 -> y 5               -> A(2,5) = 0xFF50B080
 *   R = n.z 0.6555055; k_r_to_s s' = 0.6555 -> 5.244 -> x 5, y 5 -> A(5,5) = 0xFFB0B080 */
static const TgDraw k_normal[] = {
    { .what = "NORMAL_MAP S/T: (n.x, n.y) -> A(2,5)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .normal = N_U,
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF50B080u, .want_tc = { 0.3125f, 0.6875f, 1.0f } },
    { .what = "NORMAL_MAP R = n.z reaches s through the matrix -> A(5,5)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, TG_NRM } }, .mat_enable = { 1 }, .mat = { k_r_to_s },
      .normal = N_U, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB0B080u, .want_tc = { 0.6555055f, 0.6875f, 1.0f } },
};

/* ── REFLECTION_MAP (R5) ───────────────────────────────────────────────────────
 * MV k_mv_e3: e = (0.5, 0.75, -1.5), |e| = 1.75, u = (2, 3, -6)/7. n = (4,1,8)/9.
 *   n.u = (8 + 3 - 48)/63 = -37/63; r = u + (74/63) n
 *     = (162 + 296, 243 + 74, -486 + 592)/567 = (458, 317, 106)/567
 *     = (0.807760, 0.559083, 0.186949)
 *   s 0.807760 -> 6.462 -> x 6, t 0.559083 -> 4.473 -> y 4      -> A(6,4) = 0xFFD09080
 *   R: k_r_to_s s' = 0.186949 -> 1.496 -> x 1, y 4               -> A(1,4) = 0xFF309080
 *   (an infinite viewer u = (0,0,-1) gives r = (64, 16, 47)/81 -> A(6,1)) */
static const TgDraw k_reflect[] = {
    { .what = "REFLECTION_MAP S/T: reflect(normalize(e), n).xy -> A(6,4)", .ffp = 1,
      .texgen = { { TG_REF, TG_REF, 0 } }, .mv = k_mv_e3,
      .normal = { 4.0f / 9.0f, 1.0f / 9.0f, 8.0f / 9.0f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFD09080u, .want_tc = { 0.807760f, 0.559083f, 1.0f } },
    { .what = "REFLECTION_MAP R = r.z reaches s through the matrix -> A(1,4)", .ffp = 1,
      .texgen = { { TG_REF, TG_REF, TG_REF } }, .mv = k_mv_e3,
      .mat_enable = { 1 }, .mat = { k_r_to_s },
      .normal = { 4.0f / 9.0f, 1.0f / 9.0f, 8.0f / 9.0f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF309080u, .want_tc = { 0.186949f, 0.559083f, 1.0f } },
};

/* ── DISABLE: attribute passthrough, per component (R5) ────────────────────────
 * D1 all DISABLE, attribute 9 (0.3125, 0.6875, 0, 1)            -> A(2,5) = 0xFF50B080
 * D2 S NORMAL_MAP, T DISABLE: n = (0.8125, 0.5625, 0.15), attribute (0.5, 0.1875):
 *    s = n.x 0.8125 -> 6.5 -> x 6, t = 0.1875 -> 1.5 -> y 1      -> A(6,1) = 0xFFD03080
 * D3 S DISABLE, T NORMAL_MAP: attribute (0.4375, 0.9), same n:
 *    s 0.4375 -> 3.5 -> x 3, t = n.y 0.5625 -> 4.5 -> y 4        -> A(3,4) = 0xFF709080 */
static const TgDraw k_disable[] = {
    { .what = "DISABLE passes the attribute through -> A(2,5)", .ffp = 1,
      .normal = { 0.8f, 0.1f, 0.59f }, .tc_size = { 4 }, .tc = { { 0.3125f, 0.6875f, 0, 1 } },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF50B080u, .want_tc = { 0.3125f, 0.6875f, 1.0f } },
    { .what = "S NORMAL_MAP with T DISABLE: s = n.x, t = attribute -> A(6,1)", .ffp = 1,
      .texgen = { { TG_NRM, 0, 0 } }, .normal = { 0.8125f, 0.5625f, 0.15f },
      .tc_size = { 4 }, .tc = { { 0.5f, 0.1875f, 0, 1 } },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFD03080u, .want_tc = { 0.8125f, 0.1875f, 1.0f } },
    { .what = "S DISABLE with T NORMAL_MAP: s = attribute, t = n.y -> A(3,4)", .ffp = 1,
      .texgen = { { 0, TG_NRM, 0 } }, .normal = { 0.8125f, 0.5625f, 0.15f },
      .tc_size = { 4 }, .tc = { { 0.4375f, 0.9f, 0, 1 } },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF709080u, .want_tc = { 0.4375f, 0.5625f, 1.0f } },
};

/* ── The JSRF-observed state, verbatim (§31.62 coverage record) ───────────────
 * Stage 0 S/T/R = NORMAL_MAP (0x8511), Q = 0 (not sendable), matrix enable 1,
 * matrix words k_jsrf. A float2 texcoord set at attribute 9 (D3D TEX1; texgen
 * overrides s and t, q is its default w 1). IMV identity, normalisation off.
 * s' = 0.5 n.x + 0.5, t' = -0.5 n.y + 0.5 (R1):
 *   N1 ( 0.375, -0.625): s 0.6875 -> 5.5, t 0.8125 -> 6.5      -> A(5,6) = 0xFFB0D080
 *   N2 (-0.625,  0.375): s 0.1875 -> 1.5, t 0.3125 -> 2.5      -> A(1,2) = 0xFF305080
 *   N3 (-0.375, -0.125): s 0.3125 -> 2.5, t 0.5625 -> 4.5      -> A(2,4) = 0xFF509080
 * Words taken as columns instead: N1 -> s 0.1875, t 0.3125 (CPU A(1,2)). */
#define JSRF_DRAW(name, n, want_, s_, t_) \
    { .what = name, .ffp = 1, .texgen = { { TG_NRM, TG_NRM, TG_NRM } }, \
      .mat_enable = { 1 }, .mat = { k_jsrf }, .normal = n, \
      .tc_size = { 2 }, .tc = { { 0.9f, 0.9f, 0, 1 } }, \
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0), \
      .out_reg = 8, .want = (want_), .want_tc = { (s_), (t_), 1.0f } }
static const TgDraw k_jsrf_state[] = {
    JSRF_DRAW("JSRF state, normal N1 (0.375,-0.625) -> A(5,6)", N_1, 0xFFB0D080u, 0.6875f, 0.8125f),
    JSRF_DRAW("JSRF state, normal N2 (-0.625,0.375) -> A(1,2)", N_2, 0xFF305080u, 0.1875f, 0.3125f),
    JSRF_DRAW("JSRF state, normal N3 (-0.375,-0.125) -> A(2,4)", N_3, 0xFF509080u, 0.3125f, 0.5625f),
};

/* ── An asymmetric texture matrix catches a transpose (R1, R6) ────────────────
 * k_asym rows: s' = t + 0.25, t' = 0.5 s (q = 1).
 * D1 DISABLE, attribute (0.8125, 0.3125): s' 0.5625 -> 4.5 -> x 4, t' 0.40625
 *    -> 3.25 -> y 3                                              -> A(4,3) = 0xFF907080
 *    (transposed: s'' 0.15625, t'' 0.8125 -> A(1,6) on CPU)
 * D2 NORMAL_MAP S/T, n = (0.6875, 0.1875, 0.5) then the matrix: s' 0.1875 + 0.25
 *    = 0.4375 -> 3.5 -> x 3, t' 0.34375 -> 2.75 -> y 2            -> A(3,2) = 0xFF705080 */
static const TgDraw k_asym_draws[] = {
    { .what = "asymmetric matrix on the attribute: words are rows -> A(4,3)", .ffp = 1,
      .mat_enable = { 1 }, .mat = { k_asym }, .normal = { 0, 0, 1 },
      .tc_size = { 4 }, .tc = { { 0.8125f, 0.3125f, 0, 1 } },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF907080u, .want_tc = { 0.5625f, 0.40625f, 1.0f } },
    { .what = "asymmetric matrix after NORMAL_MAP: texgen, then rows -> A(3,2)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .mat_enable = { 1 }, .mat = { k_asym },
      .normal = { 0.6875f, 0.1875f, 0.5f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF705080u, .want_tc = { 0.4375f, 0.34375f, 1.0f } },
};

/* ── Inverse model-view, not model-view, for normals (R3) ─────────────────────
 * I1 MV diag(2, 0.5, 1), IMV diag(0.5, 2, 1), nrm (0.625, 0.28125, 0.5), no
 *    normalisation: n = (0.3125, 0.5625) -> 2.5, 4.5              -> A(2,4) = 0xFF509080
 *    (the model-view instead: (1.25, 0.140625) -> s off the texture)
 * I2 an asymmetric pair: MV rows (0,2,0,0),(4,0,0,0),..., IMV rows (0,0.5,0),
 *    (0.25,0,0), (0,0,1) (its inverse transpose). nrm (1.75, 1.375, 0.5):
 *    n = (0.5*1.375, 0.25*1.75) = (0.6875, 0.4375) -> 5.5, 3.5    -> A(5,3) = 0xFFB07080
 *    (IMV words read as columns: (0.34375, 0.875) -> A(2,7); MV: (2.75, 7) off) */
static const TgDraw k_imv[] = {
    { .what = "non-uniform scale: the normal goes through the INVERSE words -> A(2,4)",
      .ffp = 1, .texgen = { { TG_NRM, TG_NRM, 0 } }, .mv = k_mv_s, .imv = k_imv_s,
      .normal = { 0.625f, 0.28125f, 0.5f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF509080u, .want_tc = { 0.3125f, 0.5625f, 1.0f } },
    { .what = "asymmetric inverse model-view: its words are rows too -> A(5,3)",
      .ffp = 1, .texgen = { { TG_NRM, TG_NRM, 0 } }, .mv = k_mv_r, .imv = k_imv_r,
      .normal = { 1.75f, 1.375f, 0.5f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB07080u, .want_tc = { 0.6875f, 0.4375f, 1.0f } },
};

/* ── NORMALIZATION_ENABLE (R3) ─────────────────────────────────────────────────
 * Z1/Z2 IMV identity, nrm (3, 5, 4)/16 = (0.1875, 0.3125, 0.25), |nrm| = sqrt50/16:
 *    off: (0.1875, 0.3125) -> 1.5, 2.5                           -> A(1,2) = 0xFF305080
 *    on:  (3, 5)/sqrt50 = (0.424264, 0.707107) -> 3.394, 5.657   -> A(3,5) = 0xFF70B080
 * Z3/Z4 IMV diag(0.5, 1, 1), nrm (0.625, 0.3125, 0.125) -> n = (5, 5, 2)/16:
 *    off: (0.3125, 0.3125) -> 2.5, 2.5                           -> A(2,2) = 0xFF505080
 *    on:  (5, 5)/sqrt54 = 0.680414 each -> 5.443                 -> A(5,5) = 0xFFB0B080
 *    (normalised BEFORE the transform: (0.44022, 0.44022) -> A(3,3)) */
static const TgDraw k_norm[] = {
    { .what = "NORMALIZATION off keeps a non-unit normal -> A(1,2)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .normal = { 0.1875f, 0.3125f, 0.25f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF305080u, .want_tc = { 0.1875f, 0.3125f, 1.0f } },
    { .what = "NORMALIZATION on makes it unit -> A(3,5)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .normalize = 1, .normal = { 0.1875f, 0.3125f, 0.25f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF70B080u, .want_tc = { 0.424264f, 0.707107f, 1.0f } },
    { .what = "NORMALIZATION off after a scaling IMV -> A(2,2)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .imv = k_imv_h, .normal = { 0.625f, 0.3125f, 0.125f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF505080u, .want_tc = { 0.3125f, 0.3125f, 1.0f } },
    { .what = "NORMALIZATION on normalises AFTER the IMV -> A(5,5)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .imv = k_imv_h, .normalize = 1,
      .normal = { 0.625f, 0.3125f, 0.125f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB0B080u, .want_tc = { 0.680414f, 0.680414f, 1.0f } },
};

/* ── EYE_LINEAR vs OBJECT_LINEAR through one non-identity model-view (R4) ─────
 * MV k_mv_ep: e.z = 0.5*0.5 + 0.375 = 0.625; pos.z = 0.5. Planes k_p_ep:
 *   S (0,0,1,0.0625), T (0,0,-1,0.9375).
 *   EYE:    s 0.6875 -> 5.5, t 0.3125 -> 2.5                     -> A(5,2) = 0xFFB05080
 *   OBJECT: s 0.5625 -> 4.5, t 0.4375 -> 3.5                     -> A(4,3) = 0xFF907080 */
static const TgDraw k_eye_plane[] = {
    { .what = "EYE_LINEAR plane meets the eye-space position -> A(5,2)", .ffp = 1,
      .texgen = { { TG_EYE, TG_EYE, 0 } }, .plane = { k_p_ep }, .mv = k_mv_ep,
      .normal = { 0, 0, 1 }, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB05080u, .want_tc = { 0.6875f, 0.3125f, 1.0f } },
    { .what = "OBJECT_LINEAR, same plane and model-view, meets the object position -> A(4,3)",
      .ffp = 1, .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_ep }, .mv = k_mv_ep,
      .normal = { 0, 0, 1 }, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF907080u, .want_tc = { 0.5625f, 0.4375f, 1.0f } },
};

/* ── Texture-matrix enable (R6) ────────────────────────────────────────────────
 * The JSRF words are delivered both times; only the enable differs.
 * NORMAL_MAP S/T, n = N_U (0.3125, 0.6875):
 *   enable 0: s 0.3125 -> 2.5, t 0.6875 -> 5.5                   -> A(2,5) = 0xFF50B080
 *   enable 1: s 0.5 + 0.15625 = 0.65625 -> 5.25, t 0.5 - 0.34375 = 0.15625
 *             -> 1.25                                            -> A(5,1) = 0xFFB03080 */
static const TgDraw k_mat_enable[] = {
    { .what = "matrix enable 0: delivered words are not applied -> A(2,5)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .mat_enable = { 0 }, .mat = { k_jsrf },
      .normal = N_U, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF50B080u, .want_tc = { 0.3125f, 0.6875f, 1.0f } },
    { .what = "matrix enable 1: the same words are applied -> A(5,1)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .mat_enable = { 1 }, .mat = { k_jsrf },
      .normal = N_U, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB03080u, .want_tc = { 0.65625f, 0.15625f, 1.0f } },
};

/* ── Stages 1 and 2 (the Vulkan half observes them; see CPU_DIFFUSE_NOTE) ──────
 * Hardware texgen generates the coordinate whether or not the stage has an
 * array (vsh-ff.c writes oT<i> from the generated value), so the stage samples.
 * G1 stage 1 NORMAL_MAP S/T over an attribute-10 array (0.9, 0.9) that it
 *    overrides; stage-1 matrix k_jsrf enabled at 0x700. Stage 0 holds k_asym,
 *    disabled. N1: s 0.6875, t 0.8125                            -> B(5,6) = 0xFFB0D040
 * G2 stage 1 OBJECT_LINEAR S/T, NO attribute-10 array, stage-1 planes k_p_s1:
 *    S (0,0,0,0.8125) -> 6.5, T (0,0,1,-0.1875) -> 0.3125 -> 2.5. Stage 0's
 *    planes are k_p_obj (unused)                                 -> B(6,2) = 0xFFD05040
 * G3 stage 2 NORMAL_MAP S/T, no array (attribute 11 is not even admitted);
 *    stage 0's matrix k_jsrf is enabled and must not apply. n = (0.5625,
 *    0.1875, 0.805): s 0.5625 -> 4.5, t 0.1875 -> 1.5             -> C(4,1) = 0xFF9030C0 */
static const TgDraw k_stage1[] = {
    { .what = "stage 1 NORMAL_MAP + its own matrix overrides its array -> B(5,6)", .ffp = 1,
      .texgen = { { 0, 0, 0 }, { TG_NRM, TG_NRM, 0 } }, .mat_enable = { 0, 1 },
      .mat = { k_asym, k_jsrf }, .normal = N_1,
      .tc_size = { 0, 4 }, .tc = { { 0 }, { 0.9f, 0.9f, 0, 1 } },
      .tex = { TGF_TEX_NONE, TGF_TEX_B, TGF_TEX_NONE }, .prog = PROG(0, 1, 0, 0),
      .out_reg = 9, .want = 0xFFB0D040u, .want_tc = { 0.6875f, 0.8125f, 1.0f },
      .cpu_differs = 1, .want_cpu = TGF_DIFFUSE, .cpu_note = CPU_DIFFUSE_NOTE },
    { .what = "stage 1 OBJECT_LINEAR with no texcoord array still samples -> B(6,2)", .ffp = 1,
      .texgen = { { 0, 0, 0 }, { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_obj, k_p_s1 },
      .normal = { 0, 0, 1 },
      .tex = { TGF_TEX_NONE, TGF_TEX_B, TGF_TEX_NONE }, .prog = PROG(0, 1, 0, 0),
      .out_reg = 9, .want = 0xFFD05040u, .want_tc = { 0.8125f, 0.3125f, 1.0f },
      .cpu_differs = 1, .want_cpu = TGF_DIFFUSE, .cpu_note = CPU_DIFFUSE_NOTE },
    { .what = "stage 2 NORMAL_MAP with no array samples; stage 0's matrix stays out -> C(4,1)",
      .ffp = 1, .texgen = { { 0, 0, 0 }, { 0, 0, 0 }, { TG_NRM, TG_NRM, 0 } },
      .mat_enable = { 1 }, .mat = { k_jsrf }, .normal = { 0.5625f, 0.1875f, 0.805f },
      .tex = { TGF_TEX_NONE, TGF_TEX_NONE, TGF_TEX_C }, .prog = PROG(0, 0, 1, 0),
      .out_reg = 10, .want = 0xFF9030C0u, .want_tc = { 0.5625f, 0.1875f, 1.0f },
      .cpu_differs = 1, .want_cpu = TGF_DIFFUSE, .cpu_note = CPU_DIFFUSE_NOTE },
};

/* ── A vertex-program batch is unaffected ──────────────────────────────────────
 * Texgen and the matrix are fixed-function vertex processing only (vsh-ff.c is
 * the fixed-function generator; a program writes oT itself).
 * P1 the program draw: oT0 = c[0] = (0.3125, 0.6875, 0, 1) with the JSRF texgen
 *    and matrix state live                                       -> A(2,5) = 0xFF50B080
 *    (texgen leaking in would give A(5,6))
 * P2 the positive control: the same state, fixed function, N1    -> A(5,6) = 0xFFB0D080 */
static const TgDraw k_vp[] = {
    { .what = "vertex-program batch ignores texgen and the texture matrix -> A(2,5)",
      .ffp = 0, .texgen = { { TG_NRM, TG_NRM, TG_NRM } }, .mat_enable = { 1 },
      .mat = { k_jsrf }, .normal = N_1, .vp_t0 = { 0.3125f, 0.6875f, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF50B080u, .want_tc = { 0.3125f, 0.6875f, 1.0f } },
    { .what = "the same state on a fixed-function batch generates -> A(5,6)",
      .ffp = 1, .texgen = { { TG_NRM, TG_NRM, TG_NRM } }, .mat_enable = { 1 },
      .mat = { k_jsrf }, .normal = N_1, .vp_t0 = { 0.3125f, 0.6875f, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB0D080u, .want_tc = { 0.6875f, 0.8125f, 1.0f } },
};

/* ── Swizzled vs linear texel scaling (R7) ─────────────────────────────────────
 * OBJECT_LINEAR S/T with constant planes (w column only, pos.w = 1).
 * C1 swizzled A, (0.6875, 0.3125) -> 5.5, 2.5                    -> A(5,2) = 0xFFB05080
 * C2 linear AL, (0.6875, 0.3125) texels -> texel (0,0)           -> 0xFF101080
 * C3 linear AL, (5.5, 2.5) texels -> texel (5,2)                 -> 0xFFB05080 */
static const TgDraw k_scale[] = {
    { .what = "swizzled texture: generated (0.6875,0.3125) is normalised -> A(5,2)", .ffp = 1,
      .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_c1 }, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB05080u, .want_tc = { 0.6875f, 0.3125f, 1.0f } },
    { .what = "linear texture: the same generated value is in texels -> texel (0,0)", .ffp = 1,
      .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_c1 }, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A_LINEAR), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF101080u, .want_tc = { 0.6875f, 0.3125f, 1.0f } },
    { .what = "linear texture: generated (5.5,2.5) texels -> texel (5,2)", .ffp = 1,
      .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_c2 }, .normal = { 0, 0, 1 },
      .tex = TEX0(TGF_TEX_A_LINEAR), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFFB05080u, .want_tc = { 5.5f, 2.5f, 1.0f } },
};

/* ── A projective matrix row: PROJECT2D divides by the transformed q (R6) ──────
 * k_q2: q' = 2.
 * Q1 DISABLE, attribute (0.6875, 0.3125, 0, 1): (s, t, q) = (0.6875, 0.3125, 2);
 *    PROJECT2D: (0.34375, 0.15625) -> 2.75, 1.25                 -> A(2,1) = 0xFF503080
 * Q2 NORMAL_MAP, n = (0.8125, 0.5625, 0.15): (0.8125, 0.5625, 2) -> (0.40625,
 *    0.28125) -> 3.25, 2.25                                      -> A(3,2) = 0xFF705080
 * LIMITATION cpu: the CPU fixed-function rasteriser never divides by q (it
 * ignores the stage program), so it samples (s, t): Q1 5.5, 2.5 -> A(5,2) =
 * 0xFFB05080; Q2 6.5, 4.5 -> A(6,4) = 0xFFD09080. */
#define CPU_NOQ_NOTE "LIMITATION cpu: the fixed-function rasteriser never divides by q"
static const TgDraw k_q_project[] = {
    { .what = "matrix q row 2: PROJECT2D samples (s/q, t/q) -> A(2,1)", .ffp = 1,
      .mat_enable = { 1 }, .mat = { k_q2 }, .normal = { 0, 0, 1 },
      .tc_size = { 4 }, .tc = { { 0.6875f, 0.3125f, 0, 1 } },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF503080u, .want_tc = { 0.6875f, 0.3125f, 2.0f },
      .cpu_differs = 1, .want_cpu = 0xFFB05080u,
      .cpu_note = CPU_NOQ_NOTE ": attribute through q row 2 samples (s, t) -> A(5,2)" },
    { .what = "NORMAL_MAP then matrix q row 2: PROJECT2D divides -> A(3,2)", .ffp = 1,
      .texgen = { { TG_NRM, TG_NRM, 0 } }, .mat_enable = { 1 }, .mat = { k_q2 },
      .normal = { 0.8125f, 0.5625f, 0.15f },
      .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
      .out_reg = 8, .want = 0xFF705080u, .want_tc = { 0.8125f, 0.5625f, 2.0f },
      .cpu_differs = 1, .want_cpu = 0xFFD09080u,
      .cpu_note = CPU_NOQ_NOTE ": NORMAL_MAP through q row 2 samples (s, t) -> A(6,4)" },
};

/* ── LIMITATION: TEXGEN_Q is not admitted, so q is the attribute's w ──────────
 * S/T OBJECT_LINEAR (0.3125, 0.3125); attribute 9 = (0.9, 0.9, 0, 2); k_qattr:
 *   s' = 0.3125 + 0.125*2 = 0.5625 -> 4.5, t' = 0.3125 + 0.25*2 = 0.8125 -> 6.5,
 *   q' = 0.5*2 = 1                                               -> A(4,6) = 0xFF90D080
 *   (q taken as 1 instead: s' 0.4375, t' 0.5625, q' 0.5) */
static const TgDraw k_texgen_q = {
    .what = "LIMITATION q is the attribute's w (2) under S/T texgen -> A(4,6)", .ffp = 1,
    .texgen = { { TG_OBJ, TG_OBJ, 0 } }, .plane = { k_p_q },
    .mat_enable = { 1 }, .mat = { k_qattr }, .normal = { 0, 0, 1 },
    .tc_size = { 4 }, .tc = { { 0.9f, 0.9f, 0, 2.0f } },
    .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
    .out_reg = 8, .want = 0xFF90D080u, .want_tc = { 0.5625f, 0.8125f, 1.0f } };

/* ── LIMITATION: stage 2/3 texture matrices are not admitted ───────────────────
 * The walk admits MATRIX_ENABLE for every stage (0x428, 0x42C) but matrix
 * words only for stages 0-1 (0x6C0-0x73C). So an enabled stage-2 matrix is
 * not applied: stage 2 NORMAL_MAP as G3                          -> C(4,1) = 0xFF9030C0 */
static const TgDraw k_stage2_mat = {
    .what = "LIMITATION an enabled stage-2 matrix is not applied -> C(4,1)", .ffp = 1,
    .texgen = { { 0, 0, 0 }, { 0, 0, 0 }, { TG_NRM, TG_NRM, 0 } },
    .mat_enable = { 0, 0, 1 }, .normal = { 0.5625f, 0.1875f, 0.805f },
    .tex = { TGF_TEX_NONE, TGF_TEX_NONE, TGF_TEX_C }, .prog = PROG(0, 0, 1, 0),
    .out_reg = 10, .want = 0xFF9030C0u, .want_tc = { 0.5625f, 0.1875f, 1.0f },
    .cpu_differs = 1, .want_cpu = TGF_DIFFUSE, .cpu_note = CPU_DIFFUSE_NOTE };

/* ── LIMITATION: the viewer model is the default (local viewer) ───────────────
 * TEXGEN_VIEW_MODEL (0x9CC) is not admitted; its reset value 0 is
 * LOCAL_VIEWER, and xemu's generator never reads it ("TODO: TexGen View Model
 * missing!", vsh-ff.c:188) nor EYE_POSITION for texgen (eyePosition only
 * enters local-eye lighting, vsh-ff.c:290-293). So the sphere map's u is
 * normalize(e.xyz) from the eye-space origin whatever EYE_POSITION holds:
 * S2's state plus EYE_POSITION (5, -3, 2, 1)                     -> A(5,4) = 0xFFB09080.
 * This pins current behaviour, matching xemu; real hardware's use of
 * EYE_POSITION for texgen is not established here. */
static const float k_eye_pos[4] = { 5.0f, -3.0f, 2.0f, 1.0f };
static const TgDraw k_viewer = {
    .what = "LIMITATION local viewer: EYE_POSITION does not move SPHERE_MAP -> A(5,4)",
    .ffp = 1, .texgen = { { TG_SPH, TG_SPH, 0 } }, .mv = k_mv_e2, .eye = k_eye_pos,
    .normal = { 0, 0, 1 }, .tex = TEX0(TGF_TEX_A), .prog = PROG(1, 0, 0, 0),
    .out_reg = 8, .want = 0xFFB09080u, .want_tc = { 0.687276f, 0.562425f, 1.0f } };

/* ── The case table ─────────────────────────────────────────────────────────── */

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

static void run_draws(const TgDraw *d, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        check_draw(&d[i]);
}

static void case_eye(void)        { run_draws(k_eye, N_OF(k_eye)); }
static void case_obj(void)        { run_draws(k_obj, N_OF(k_obj)); }
static void case_sphere(void)     { run_draws(k_sphere, N_OF(k_sphere)); }
static void case_normal(void)     { run_draws(k_normal, N_OF(k_normal)); }
static void case_reflect(void)    { run_draws(k_reflect, N_OF(k_reflect)); }
static void case_disable(void)    { run_draws(k_disable, N_OF(k_disable)); }
static void case_jsrf(void)       { run_draws(k_jsrf_state, N_OF(k_jsrf_state)); }
static void case_asym(void)       { run_draws(k_asym_draws, N_OF(k_asym_draws)); }
static void case_imv(void)        { run_draws(k_imv, N_OF(k_imv)); }
static void case_norm(void)       { run_draws(k_norm, N_OF(k_norm)); }
static void case_eye_plane(void)  { run_draws(k_eye_plane, N_OF(k_eye_plane)); }
static void case_mat_enable(void) { run_draws(k_mat_enable, N_OF(k_mat_enable)); }
static void case_stage1(void)     { run_draws(k_stage1, N_OF(k_stage1)); }
static void case_vp(void)         { run_draws(k_vp, N_OF(k_vp)); }
static void case_scale(void)      { run_draws(k_scale, N_OF(k_scale)); }
static void case_q_project(void)  { run_draws(k_q_project, N_OF(k_q_project)); }

static void case_texgen_q(void)
{
    /* The positive control first: the same stream without the extra word. */
    check_draw(&k_texgen_q);
    check_rejected(&k_texgen_q, 0x03CCu, TG_OBJ,
                   "LIMITATION stage 0 SET_TEXGEN_Q (0x3CC) is refused by the walk");
    check_rejected(&k_texgen_q, 0x03DCu, TG_OBJ,
                   "LIMITATION stage 1 SET_TEXGEN_Q (0x3DC) is refused by the walk");
    case_note("TEXGEN_Q cannot be delivered, so q is always the attribute's w");
}

static void case_stage23_matrix(void)
{
    check_draw(&k_stage2_mat);
    check_rejected(&k_stage2_mat, 0x0740u, 0x3F000000u,
                   "LIMITATION stage 2 SET_TEXTURE_MATRIX (0x740) is refused by the walk");
    check_rejected(&k_stage2_mat, 0x0780u, 0x3F000000u,
                   "LIMITATION stage 3 SET_TEXTURE_MATRIX (0x780) is refused by the walk");
    case_note("stage 2/3 matrix words cannot be delivered, so their enables are inert");
}

static void case_viewer(void)
{
    check_draw(&k_viewer);
    check_rejected(&k_viewer, 0x09CCu, 1u,
                   "LIMITATION SET_TEXGEN_VIEW_MODEL (0x9CC, infinite viewer) is refused");
    case_note("the viewer model stays LOCAL_VIEWER, u from the eye-space origin");
}

typedef struct { const char *name; void (*fn)(void); } TgCase;

static const TgCase g_tg_cases[] = {
    { "mode_eye_linear",            case_eye },
    { "mode_object_linear",         case_obj },
    { "mode_sphere_map",            case_sphere },
    { "mode_normal_map",            case_normal },
    { "mode_reflection_map",        case_reflect },
    { "mode_disable_passthrough",   case_disable },
    { "jsrf_observed_state",        case_jsrf },
    { "texmat_asymmetric",          case_asym },
    { "imv_nonuniform_scale",       case_imv },
    { "normalization_enable",       case_norm },
    { "eye_plane_modelview",        case_eye_plane },
    { "texmat_enable",              case_mat_enable },
    { "stage1_texgen",              case_stage1 },
    { "vertex_program_unaffected",  case_vp },
    { "texel_scaling",              case_scale },
    { "texmat_q_projective",        case_q_project },
    { "limitation_texgen_q",        case_texgen_q },
    { "limitation_stage23_matrix",  case_stage23_matrix },
    { "limitation_viewer_model",    case_viewer },
};

static const TgCase *tg_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(g_tg_cases); i++)
        if (strcmp(g_tg_cases[i].name, name) == 0)
            return &g_tg_cases[i];
    return NULL;
}

#endif
