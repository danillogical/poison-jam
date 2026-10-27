/* The NV2A fixed-function vertex-lighting deciding gate: the cases and their literal
 * expectations, shared by the committed-stream CPU half (tests/test_light_stream.c)
 * and the Vulkan half (tests/test_light_stream_vk.c).
 *
 * Each half #includes this file and supplies run_case(c), which submits c->draw and
 * checks every probe against the literal below; the SAME hand-derived literal is
 * checked by each renderer independently (the executor computes the vertex colour
 * for both back ends), and agreement between them is never the test. No expectation
 * is read from a production helper or a probe.
 *
 * ── The rule (decided; ledger L82 keeps the facing-away flip) ────────────────
 * For SET_LIGHTING_ENABLE (0x0314) = 1, the diffuse colour (oD0) of an untextured
 * fixed-function draw:
 *   e  = MODEL_VIEW (16 words at 0x0480, rows, delivery order) . (x, y, z, 1)
 *   n  = INVERSE_MODEL_VIEW rows . normal, n[i] = IMV[4i]nx + IMV[4i+1]ny + IMV[4i+2]nz
 *        (12 words at 0x0580), normalised only when SET_NORMALIZATION_ENABLE
 *        (0x03A4) bit 0 is set (the same n the texgen gate pins, texgen_stream_cases.h R3).
 *   if n.e > 0 (the normal faces away from the eye at the origin), n = -n.
 *   light 0, infinite (SET_LIGHT_ENABLE_MASK 0x03BC = 1): L = normalize(INFINITE_DIRECTION
 *        words 0x1034..0x103C); contribution = ambient + diffuse * max(0, n.L), per channel.
 *   rgb = SCENE_AMBIENT (0x0A10..0x0A18) + contribution, clamped to [0, 1];
 *   alpha = SET_MATERIAL_ALPHA (0x03B4) because SET_COLOR_MATERIAL (0x0298) = 0 does
 *   not route the diffuse from the vertex.
 * The vertex colour stream (LF_VERTEX_ARGB 0xFF336699) is present and must be ignored.
 * The pixel packs by truncation: byte = (uint32_t)(c * 255).
 *
 * ── The scene (tests/light_stream_fixture.c) ─────────────────────────────────
 * 32x32 linear A8R8G8B8, identity composite (so positions are pixels whatever the
 * model-view is), one quad covering the surface at z 0.5, w 1, six vertices sharing
 * one normal, so the colour is flat. The model-view only feeds e. Probes (4,4) (upper-
 * left triangle), (28,28) and (24,10) (lower-right triangle) are well inside the quad.
 * With the identity model-view e = (x, y, 0.5, 1) and a normal along z gives n.e = n.z/2,
 * so n.z < 0 never flips and n.z > 0 always does.
 *
 * ── Dyadic constants, so every float product below is exact ──────────────────
 *   SCENE_AMBIENT S = (0.125,  0.0625, 0.25 )
 *   light ambient  A = (0.0625, 0.125,  0.125)
 *   light diffuse  D = (0.5,    0.25,   0.125)
 *   material alpha   = 0.75
 *   S + A            = (0.1875, 0.1875, 0.375)            [ambient-only colour]
 *   S + A + D        = (0.6875, 0.4375, 0.5  )            [full diffuse, n.L = 1]
 * Truncating pack, byte = (uint32_t)(c * 255):
 *   0.1875 * 255 = 47.8125  -> 47  = 0x2F
 *   0.375  * 255 = 95.625   -> 95  = 0x5F
 *   0.6875 * 255 = 175.3125 -> 175 = 0xAF
 *   0.4375 * 255 = 111.5625 -> 111 = 0x6F
 *   0.5    * 255 = 127.5    -> 127 = 0x7F
 *   0.75   * 255 = 191.25   -> 191 = 0xBF   (alpha)
 *   LIT_FULL    = S+A+D            = (0.6875, 0.4375, 0.5)    -> 0xBF AF 6F 7F
 *   LIT_AMBIENT = S+A              = (0.1875, 0.1875, 0.375)  -> 0xBF 2F 2F 5F
 *
 * ── The cases ────────────────────────────────────────────────────────────────
 * Unless a case says otherwise: mv, imv identity, normalisation off, normal (0,0,-1)
 * (n.e = -0.5, no flip), L = (0,0,-1).
 *
 * sign_toward  n.L = (0,0,-1).(0,0,-1) = 1. rgb = S+A+D.                         LIT_FULL
 * sign_away    L = (0,0,+1): n.L = -1, max(0,-1) = 0. rgb = S+A.                 LIT_AMBIENT
 * imv_scale_off mv = diag(2,2,2,1), imv = diag(0.5,0.5,0.5), normalisation OFF. n = (0,0,-0.5)
 *              (length 0.5), e = (2x, 2y, 1), n.e = -0.5: no flip. n.L = 0.5.
 *              R = 0.1875 + 0.5*0.5   = 0.4375 -> 111 = 0x6F
 *              G = 0.1875 + 0.25*0.5  = 0.3125 -> 79.6875 -> 79 = 0x4F
 *              B = 0.375  + 0.125*0.5 = 0.4375 -> 0x6F                            LIT_HALF 0xBF6F4F6F
 * imv_scale_on the same with normalisation ON: n = (0,0,-1), n.L = 1.            LIT_FULL
 * imv_rows     normal (1,0,0), imv words rows (0,1,0,.) (0,0,1,.) (-1,0,0,.), normalisation
 *              ON (a permutation with a sign, not a shear: the exact unit results a shear
 *              cannot give in dyadic floats). Read as ROWS (the rule):
 *              n = (IMV[0], IMV[4], IMV[8]) = (0,0,-1); n.e = -0.5; n.L = 1.     LIT_FULL
 *              Read as COLUMNS (the transpose, a wrong reading): n' = (IMV[0],
 *              IMV[1], IMV[2]) = (0,1,0); n'.L = 0.                              LIT_AMBIENT
 *              The assertion is the rows result.
 * dir_unit     INFINITE_DIRECTION (0,0,-2): L = normalize = (0,0,-1), n.L = 1.   LIT_FULL
 *              (An unnormalised L would give n.L = 2: R = 0.1875+1.0 clamps to 1 -> 0xFF,
 *              G = 0.1875+0.5 = 0.6875 -> 0xAF, B = 0.375+0.25 = 0.625 -> 159.375 -> 0x9F,
 *              i.e. 0xBFFFAF9F, LIT_L2.)
 * flip         normal (0,0,+1): n.e = +0.5 > 0, faces away, n -> (0,0,-1). L = (0,0,-1)
 *              (along the original -n): n.L = 1 after the flip (-1 -> 0 without).  LIT_FULL
 *
 * ── Today's result per case (read from nv2a_pb_exec.c lit_color, NOT run) ────
 * lit_color transforms the normal by the MODEL-VIEW rows, always normalises it, flips
 * on n.e > 0 and does not normalise L.
 *   sign_toward   PASS  (identity mv).
 *   sign_away     PASS.
 *   imv_scale_off FAIL  all probes: n = normalize(mv.nrm) = (0,0,-1), n.L = 1, reads
 *                       LIT_FULL 0xBFAF6F7F, want 0xBF6F4F6F.
 *   imv_scale_on  PASS  (normalised either way; same direction).
 *   imv_rows      FAIL  all probes: the model-view is the identity, so n = (1,0,0); the
 *                       triangles' first vertices are (0,0) and (0,32), n.e = 0, no flip;
 *                       n.L = 0, reads LIT_AMBIENT 0xBF2F2F5F, want 0xBFAF6F7F.
 *   dir_unit      FAIL  all probes: L is used as stored, n.L = 2, reads LIT_L2
 *                       0xBFFFAF9F, want 0xBFAF6F7F.
 *   flip          PASS  (the same flip).
 */
#ifndef JSRF_LIGHT_STREAM_CASES_H
#define JSRF_LIGHT_STREAM_CASES_H

#include "light_stream_fixture.h"

#include <stddef.h>
#include <string.h>

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

#define LIT_FULL    0xBFAF6F7Fu     /* S+A+D, n.L = 1 */
#define LIT_AMBIENT 0xBF2F2F5Fu     /* S+A, n.L <= 0 */
#define LIT_HALF    0xBF6F4F6Fu     /* S+A+0.5 D, n.L = 0.5 */

#define LC_SCENE   { 0.125f, 0.0625f, 0.25f }
#define LC_AMB     { 0.0625f, 0.125f, 0.125f }
#define LC_DIF     { 0.5f, 0.25f, 0.125f }
#define LC_ALPHA   0.75f

/* Model-view (16 words) and inverse model-view (12 words), delivery order. */
static const float k_mv_x2[16] = { 2,0,0,0,  0,2,0,0,  0,0,2,0,  0,0,0,1 };
static const float k_imv_half[12] = { 0.5f,0,0,0,  0,0.5f,0,0,  0,0,0.5f,0 };
static const float k_imv_perm[12] = { 0,1,0,0,  0,0,1,0,  -1,0,0,0 };

/* The three probes, all inside the quad, two triangles covered. */
typedef struct { uint32_t x, y; } LProbe;
static const LProbe k_probes[] = { { 4, 4 }, { 28, 28 }, { 24, 10 } };

typedef struct {
    const char *name;
    LDraw draw;
    uint32_t want;                  /* the hardware-rule ARGB at every probe */
} LCase;

/* mv, imv, normalize, normal, dir, scene, amb, dif, alpha; then the literal. */
static const LCase k_cases[] = {
    { "sign_toward",
      { NULL, NULL, 0, { 0, 0, -1 }, { 0, 0, -1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_FULL },
    { "sign_away",
      { NULL, NULL, 0, { 0, 0, -1 }, { 0, 0, 1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_AMBIENT },
    { "imv_scale_off",
      { k_mv_x2, k_imv_half, 0, { 0, 0, -1 }, { 0, 0, -1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_HALF },
    { "imv_scale_on",
      { k_mv_x2, k_imv_half, 1, { 0, 0, -1 }, { 0, 0, -1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_FULL },
    { "imv_rows",
      { NULL, k_imv_perm, 1, { 1, 0, 0 }, { 0, 0, -1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_FULL },
    { "dir_unit",
      { NULL, NULL, 0, { 0, 0, -1 }, { 0, 0, -2 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_FULL },
    { "flip",
      { NULL, NULL, 0, { 0, 0, 1 }, { 0, 0, -1 }, LC_SCENE, LC_AMB, LC_DIF, LC_ALPHA },
      LIT_FULL },
};

static const LCase *l_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(k_cases); i++)
        if (strcmp(k_cases[i].name, name) == 0)
            return &k_cases[i];
    return NULL;
}

#endif
