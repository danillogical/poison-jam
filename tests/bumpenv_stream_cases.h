/* The D4 step-5 BUMPENVMAP / BUMPENVMAP_LUM deciding gate: the cases and their
 * literal expectations, shared by the committed-stream CPU half
 * (tests/test_bumpenv_stream.c) and the Vulkan half (tests/test_bumpenv_stream_vk.c).
 *
 * Each half #includes this file and supplies three functions:
 *   check_draw(d)                  submit d's stream, observe, compare to d->want
 *   check_rejected(d, m, p, what)  d's stream plus (m, p) must be refused by the walk
 *   case_note(fmt, ...)            a line of context on stdout
 * so the SAME literal is checked by each renderer independently; agreement
 * between them is never the test.
 *
 * Authored by a worker distinct from the production author (toolkit 2a3c1e9,
 * 5e38bb4, d9153fd) and from the focused step-4 gate's author. No expectation
 * is read from a production helper or a probe: each is derived below by hand
 * from the fixture's own images (bumpenv_stream_fixture.c) and the hardware
 * rule stated next.
 *
 * ── The hardware rule used (xemu 478b4f49 read for facts only; production line
 *    references are toolkit d9153fd) ─────────────────────────────────────────
 * R1 Delta source. Stage 1 reads stage 0. Stage 2 reads the stage named by
 *    SET_SHADER_OTHER_STAGE_INPUT bits 16-19, stage 3 the one named by bits
 *    20-23 (xemu psh.c `input_tex`; xemu pgraph.c keeps bits 12-27 of the
 *    word). Hardware (D3D8 Xbox PS_INPUTTEXTURE) lets a stage read only an
 *    EARLIER stage: stage 2 may name 0 or 1, stage 3 may name 0, 1 or 2; every
 *    source used here is one of those, and the source-selection draws use one
 *    that is NOT the immediately previous stage. Production: nv2a_pb_exec.c
 *    rc_bump_source (~3131), which also maps an illegal nibble to "no source".
 * R2 Deltas. du = sign3(source.B), dv = sign3(source.G), where
 *    sign3(x) = x*255 >= 128 ? (x*255 - 256)/127 : (x*255)/127 on the decoded
 *    UNORM byte (xemu psh.c `sign3`). So by byte:
 *        0x00 ->  0          0x7F -> +1          0x80 -> -128/127 = -1.0078740
 *        0x81 -> -1          0xFF -> -1/127 = -0.0078740
 * R3 Matrix. dsdt.x = m00*du + m10*dv, dsdt.y = m01*du + m11*dv (xemu psh.c
 *    `bumpMat[i] * dsdt` with GLSL's column-major mat2 built from
 *    BUMPMAT00, 01, 10, 11). The METHOD words arrive in the order
 *    m00, m01, m11, m10 (xemu pgraph.c SET_TEXTURE_SET_BUMP_ENV_MAT's swizzle
 *    table), which is how the fixture emits them; each draw below lists the
 *    canonical [m00, m01, m10, m11].
 * R4 Coordinate. The bump stage samples its own texture at (s, t) + dsdt,
 *    with NO texture-q division (xemu psh.c BUMPENVMAP: `pT.xy + dsdt`); only
 *    PROJECT2D (mode 1) divides by q (xemu: textureProj(pT.xyw)).
 * R5 Luminance (mode 7). k = BUMPENVLSCALE * source.R + BUMPENVLOFFSET, and the
 *    sampled texel is multiplied by k in EVERY channel, alpha included (xemu
 *    psh.c `t = t * (bumpScale * dsdtl.p + bumpOffset)`; production
 *    nv2a_pb_exec.c ~3227 and nv2a_vk_rc.hlsl `s *= ...`).
 * R6 Saturation -- the derived clamp rule. The mode-7 product is NOT clamped
 *    in the stage (neither xemu nor production clamps there). It is clamped
 *    only where every texture register is: a combiner input in
 *    UNSIGNED_IDENTITY mapping reads max(x, 0) with no upper bound (xemu psh.c
 *    get_input_var; production nv2a_combiner.c map_in), and the final
 *    combiner's output saturates to [0, 1] (xemu writes a UNORM target;
 *    production nv2a_combiner.c `clampf(out, 0, 1)`, HLSL `saturate(o)`). So a
 *    k > 1 product that the final combiner scales back DOWN keeps its excess,
 *    and a negative k reads 0. This is the rule both references agree on; the
 *    gate does not claim real-hardware fixed-point behaviour.
 * R7 Sampling. Nearest (FILTER MIN 1 / MAG 1), clamp-to-edge, level 0, so a
 *    normalised coordinate u selects texel floor(u * size) clamped to
 *    [0, size-1]. Every coordinate below lands at least 0.125 texel from a
 *    texel edge, so neither renderer's rounding can move it.
 * R8 Packing. A final value f in [0, 1] is the byte round(f * 255). No literal
 *    below is within 0.12 of a rounding tie.
 *
 * ── The images (bumpenv_stream_fixture.c) ─────────────────────────────────────
 * D, the bump destination, 8x8 swizzled A8R8G8B8:
 *    D(x, y) = A 0xFF, R 0x10 + 0x20*x, G 0x10 + 0x20*y, B 0x80
 *    so the R byte names x and the G byte names y:
 *        x or y:  0    1    2    3    4    5    6    7
 *        byte:   0x10 0x30 0x50 0x70 0x90 0xB0 0xD0 0xF0
 *    and D(x, y) as a literal is 0xFF000000 | R<<16 | G<<8 | 0x80, e.g.
 *    D(3,3) = 0xFF707080, D(5,3) = 0xFFB07080.
 *    Texel (x, y)'s centre is ((x + 0.5)/8, (y + 0.5)/8); 3.5/8 = 0.4375.
 * S, the delta sources, 4x4 swizzled A8R8G8B8, (R lum, G dv, B du), A 0xFF:
 *    S(0,0) 00 00 00   S(1,0) 40 00 7F   S(2,0) 80 7F 00   S(3,0) 20 00 81
 *    S(0,1) 00 00 80   S(1,1) 00 00 FF   S(2,1) 00 81 00   S(3,1) 00 80 00
 *    S(0,2) 00 FF 00   S(1,2) 00 7F 80   S(2,2) 00 80 7F   S(3,2) FF 00 00
 *    S(0,3) C0 00 00   S(1,3) 60 00 00
 *    A source stage samples S with PROJECT2D at q = 1 at ((x + 0.5)/4, (y + 0.5)/4).
 *
 * Every draw covers the whole 64x64 surface with constant texture coordinates,
 * so every covered pixel carries the same value; the halves read three pixels.
 * The final combiner emits one texture register (plain), or that register's
 * RGB times a constant c0 (scaled), with the register's own alpha.
 */
#ifndef JSRF_BUMPENV_STREAM_CASES_H
#define JSRF_BUMPENV_STREAM_CASES_H

#include "bumpenv_stream_fixture.h"

#include <stddef.h>
#include <string.h>

/* SET_SHADER_STAGE_PROGRAM: five bits per stage. */
#define PROG(m0, m1, m2, m3) ((uint32_t)(m0) | ((uint32_t)(m1) << 5) \
                              | ((uint32_t)(m2) << 10) | ((uint32_t)(m3) << 15))
/* A source texel of S, and a D coordinate given in texels, both at q = 1. */
#define S_TC(x, y)      { ((x) + 0.5f) / 4.0f, ((y) + 0.5f) / 4.0f, 0.0f, 1.0f }
#define D_TC(u, v, q)   { (u) / 8.0f, (v) / 8.0f, 0.0f, (q) }
#define NO_TC           { 0.0f, 0.0f, 0.0f, 1.0f }
/* Canonical [m00, m01, m10, m11]. 0.25 moves two texels of D per unit delta;
 * 7.9375 = 127/16 moves 63.5 texels, so a 1/127 delta moves half a texel. */
#define MAT(a, b, c, d) { (a), (b), (c), (d) }
#define DIAG(g)         MAT((g), 0.0f, 0.0f, (g))
#define GAIN_HALF_TEXEL 7.9375f

static void check_draw(const BsfDraw *d);
static void check_rejected(const BsfDraw *d, uint32_t method, uint32_t param,
                           const char *what);
static void case_note(const char *fmt, ...);

/* ── Modes 6: signed-byte boundaries on du (stage 1 reads stage 0) ──────────────
 * prog = PROJECT2D at stage 0 on S, BUMPENVMAP at stage 1 on D; out = t1. */
static const BsfDraw k_signed_du[] = {
    /* S(0,0): du = dv = 0 -> dsdt = 0, (3.5, 3.5) -> D(3,3) = 0xFF707080. */
    { .what = "du byte 0x00 does not move the sample: D(3,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFF707080u },
    /* S(1,0): du = sign3(0x7F) = +1, dv = 0, DIAG(0.25):
     * dsdt.x = 0.25*1 = 0.25 = +2 texels -> x 3.5 + 2 = 5.5 -> D(5,3) = 0xFFB07080.
     * (Unsigned reading 127/255*0.25*8 = +0.996 texel would give D(4,3).) */
    { .what = "du byte 0x7F is +1: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
    /* S(3,0): du = sign3(0x81) = (129-256)/127 = -1:
     * dsdt.x = -0.25 = -2 texels -> x 1.5 -> D(1,3) = 0xFF307080.
     * (Unsigned: +1.012 texel -> D(4,3).) */
    { .what = "du byte 0x81 is -1: D(1,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(3, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFF307080u },
    /* S(1,1): du = sign3(0xFF) = -1/127; m00 = 127/16, base x = 3.25 texels:
     * dsdt.x = (127/16)(-1/127) = -1/16 = -0.5 texel -> x 2.75 -> D(2,3) = 0xFF507080.
     * (0xFF read as 0 -> D(3,3); as +255/127 or unsigned 1.0 -> clamped D(7,3).) */
    { .what = "du byte 0xFF is -1/127: half a texel left, D(2,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 1), D_TC(3.25f, 3.5f, 1.0f) },
      .mat = { {0}, MAT(GAIN_HALF_TEXEL, 0.0f, 0.0f, 0.0f) },
      .out_reg = 9, .want = 0xFF507080u },
    /* S(1,2): du = sign3(0x80) = -128/127, dv = sign3(0x7F) = +1;
     * m00 = m10 = 127/16, m01 = m11 = 0, base x = 3.25:
     * dsdt.x = (127/16)(-128/127) + (127/16)(1) = -8 + 7.9375 = -0.0625
     *        = -0.5 texel -> x 2.75 -> D(2,3) = 0xFF507080; dsdt.y = 0.
     * (0x80 read like 0x81, i.e. -1 -> dsdt.x 0 -> D(3,3); read positive
     *  +128/127 or unsigned -> x far right, clamped D(7,3).) */
    { .what = "du byte 0x80 is -128/127, not -1: D(2,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 2), D_TC(3.25f, 3.5f, 1.0f) },
      .mat = { {0}, MAT(GAIN_HALF_TEXEL, 0.0f, GAIN_HALF_TEXEL, 0.0f) },
      .out_reg = 9, .want = 0xFF507080u },
};

/* ── Mode 6: signed-byte boundaries on dv ──────────────────────────────────── */
static const BsfDraw k_signed_dv[] = {
    /* S(0,0) at the half-texel gain on both axes, base (3.25, 3.25): du = dv = 0
     * -> no move -> D(3,3) = 0xFF707080. (Any non-zero reading of 0x00 at this
     * gain moves at least half a texel.) */
    { .what = "du and dv byte 0x00 at high gain stay put: D(3,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.25f, 3.25f, 1.0f) }, .mat = { {0}, DIAG(GAIN_HALF_TEXEL) },
      .out_reg = 9, .want = 0xFF707080u },
    /* S(2,0): du = 0, dv = sign3(0x7F) = +1, DIAG(0.25):
     * dsdt.y = m11*dv = 0.25 = +2 texels -> y 5.5 -> D(3,5) = 0xFF70B080. */
    { .what = "dv byte 0x7F is +1: D(3,5)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(2, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFF70B080u },
    /* S(2,1): dv = sign3(0x81) = -1 -> y 1.5 -> D(3,1) = 0xFF703080. */
    { .what = "dv byte 0x81 is -1: D(3,1)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(2, 1), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFF703080u },
    /* S(0,2): dv = sign3(0xFF) = -1/127; m11 = 127/16, base y = 3.25:
     * dsdt.y = -1/16 = -0.5 texel -> y 2.75 -> D(3,2) = 0xFF705080. */
    { .what = "dv byte 0xFF is -1/127: half a texel up, D(3,2)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 2), D_TC(3.5f, 3.25f, 1.0f) },
      .mat = { {0}, MAT(0.0f, 0.0f, 0.0f, GAIN_HALF_TEXEL) },
      .out_reg = 9, .want = 0xFF705080u },
    /* S(2,2): du = sign3(0x7F) = +1, dv = sign3(0x80) = -128/127;
     * m01 = m11 = 127/16, m00 = m10 = 0, base y = 3.25:
     * dsdt.y = m01*du + m11*dv = 7.9375 - 8 = -0.0625 = -0.5 texel -> y 2.75;
     * dsdt.x = m00*du + m10*dv = 0 -> D(3,2) = 0xFF705080. */
    { .what = "dv byte 0x80 is -128/127, not -1: D(3,2)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(2, 2), D_TC(3.5f, 3.25f, 1.0f) },
      .mat = { {0}, MAT(0.0f, GAIN_HALF_TEXEL, 0.0f, GAIN_HALF_TEXEL) },
      .out_reg = 9, .want = 0xFF705080u },
};

/* ── Mode 6: an asymmetric matrix, at stage 1 and at stage 2 ─────────────────
 * [m00, m01, m10, m11] = [0.125, 0.25, -0.375, 0]; the stream carries
 * 0.125, 0.25, 0 (m11), -0.375 (m10).
 *  du = +1, dv = 0 (S(1,0)): dsdt = (m00, m01) = (0.125, 0.25) = (+1, +2) texels
 *      -> (4.5, 5.5) -> D(4,5) = 0xFF90B080.
 *      m01 <-> m10 swapped gives dsdt.y = -0.375 -> D(4,0) = 0xFF901080.
 *  du = 0, dv = +1 (S(2,0)): dsdt = (m10, m11) = (-0.375, 0) = (-3, 0) texels
 *      -> (0.5, 3.5) -> D(0,3) = 0xFF107080.
 *      Swapped gives (m01, m11) = (+2, 0) -> D(5,3); the method words taken in
 *      stream order without the m11/m10 swizzle give (0, -3) -> D(3,0).
 * At stage 2 the source is stage 0 by SET_SHADER_OTHER_STAGE_INPUT = 0 and
 * stage 1 is NONE; the same matrix sits in stage 2's registers. */
#define ASYM MAT(0.125f, 0.25f, -0.375f, 0.0f)
static const BsfDraw k_asym[] = {
    { .what = "stage 1 asymmetric matrix, du only: dsdt = (m00, m01) -> D(4,5)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, ASYM },
      .out_reg = 9, .want = 0xFF90B080u },
    { .what = "stage 1 asymmetric matrix, dv only: dsdt = (m10, m11) -> D(0,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(2, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, ASYM },
      .out_reg = 9, .want = 0xFF107080u },
    { .what = "stage 2 asymmetric matrix, du only: dsdt = (m00, m01) -> D(4,5)",
      .prog = PROG(1, 0, 6, 0), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_NONE, BSF_TEX_D },
      .tc = { S_TC(1, 0), NO_TC, D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, {0}, ASYM },
      .out_reg = 10, .want = 0xFF90B080u },
    { .what = "stage 2 asymmetric matrix, dv only: dsdt = (m10, m11) -> D(0,3)",
      .prog = PROG(1, 0, 6, 0), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_NONE, BSF_TEX_D },
      .tc = { S_TC(2, 0), NO_TC, D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, {0}, ASYM },
      .out_reg = 10, .want = 0xFF107080u },
};

/* ── Mode 6 at stage 2: the source chosen by SET_SHADER_OTHER_STAGE_INPUT ──────
 * Stage 0 samples S(1,0) (du = +1), stage 1 samples S(3,0) (du = -1), stage 2
 * is BUMPENVMAP on D with DIAG(0.25).
 *  bits 16-19 = 0 (stage 0, NOT the immediately previous stage): du = +1
 *      -> x 5.5 -> D(5,3) = 0xFFB07080. Reading the previous stage instead
 *      gives du = -1 -> D(1,3).
 *  bits 16-19 = 1 (stage 1): du = -1 -> x 1.5 -> D(1,3) = 0xFF307080. Ignoring
 *      the word (taking stage 0) gives D(5,3). */
static const BsfDraw k_source2[] = {
    { .what = "stage 2 reads stage 0 when OTHER_STAGE_INPUT bits 16-19 = 0: D(5,3)",
      .prog = PROG(1, 1, 6, 0), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_D },
      .tc = { S_TC(1, 0), S_TC(3, 0), D_TC(3.5f, 3.5f, 1.0f) },
      .mat = { {0}, {0}, DIAG(0.25f) }, .out_reg = 10, .want = 0xFFB07080u },
    { .what = "stage 2 reads stage 1 when OTHER_STAGE_INPUT bits 16-19 = 1: D(1,3)",
      .prog = PROG(1, 1, 6, 0), .osi = 1u << 16,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_D },
      .tc = { S_TC(1, 0), S_TC(3, 0), D_TC(3.5f, 3.5f, 1.0f) },
      .mat = { {0}, {0}, DIAG(0.25f) }, .out_reg = 10, .want = 0xFF307080u },
};

/* ── Mode 7: luminance at stage 1 and stage 2 ──────────────────────────────────
 * L1: stage 1 BUMPENVMAP_LUM, source S(0,3): du = dv = 0, lum = 0xC0/255 =
 *     0.7529412; scale 0.5, offset 0.25 -> k = 0.3764706 + 0.25 = 0.6264706.
 *     D(3,3) * k: A 1*k*255 = 159.75 -> 0xA0; R, G 112/255*k*255 = 70.16 -> 0x46;
 *     B 128/255*k*255 = 80.19 -> 0x50.                             -> 0xA0464650
 * L2: source S(1,0): du = +1, lum = 0x40/255 = 0.2509804; DIAG(0.25) -> D(5,3)
 *     = (A FF, R B0, G 70, B 80); scale 2, offset 0.125 -> k = 0.5019608 +
 *     0.125 = 0.6269608: A 159.88 -> 0xA0; R 176*k = 110.34 -> 0x6E;
 *     G 112*k = 70.22 -> 0x46; B 128*k = 80.25 -> 0x50.           -> 0xA06E4650
 * Stage 2, source by bits 16-19; stage 0 samples S(0,3) (lum 0xC0), stage 1
 * samples S(1,3) (lum 0x60); scale 0.5, offset 0.25, zero matrix, D(3,3):
 *     source 0: lum 0xC0 -> k = 0.6264706 -> 0xA0464650 (as L1)
 *     source 1: lum 0x60/255 = 0.3764706 -> k = 0.1882353 + 0.25 = 0.4382353:
 *     A 111.75 -> 0x70; R, G 112*k = 49.08 -> 0x31; B 128*k = 56.09 -> 0x38
 *                                                                  -> 0x70313138 */
static const BsfDraw k_lum[] = {
    { .what = "stage 1 mode 7 multiplies every channel by scale*lum + offset: 0xA0464650",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0.5f }, .offset = { 0, 0.25f }, .out_reg = 9, .want = 0xA0464650u },
    { .what = "stage 1 mode 7 applies the delta AND the luminance: 0xA06E4650",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .scale = { 0, 2.0f }, .offset = { 0, 0.125f }, .out_reg = 9, .want = 0xA06E4650u },
    { .what = "stage 2 mode 7 takes lum from stage 0 (bits 16-19 = 0): 0xA0464650",
      .prog = PROG(1, 1, 7, 0), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_D },
      .tc = { S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0.5f }, .offset = { 0, 0, 0.25f }, .out_reg = 10, .want = 0xA0464650u },
    { .what = "stage 2 mode 7 takes lum from stage 1 (bits 16-19 = 1): 0x70313138",
      .prog = PROG(1, 1, 7, 0), .osi = 1u << 16,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_D },
      .tc = { S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0.5f }, .offset = { 0, 0, 0.25f }, .out_reg = 10, .want = 0x70313138u },
};

/* ── Mode 7: the saturation rule R6 ────────────────────────────────────────────
 * Source S(3,2): du = dv = 0, lum = 0xFF/255 = 1.
 *  SAT1 D(3,3), scale 1, offset 0.5 -> k = 1.5: t = (A 1.5, R G 168/255,
 *       B 192/255). Plain out saturates only A: 0xFF, R G 168 = 0xA8, B 0xC0.
 *                                                               -> 0xFFA8A8C0
 *  SAT2 D(6,6) = (FF, D0, D0, 80), scale 1, offset 1.25 -> k = 2.25:
 *       t = (A 2.25, R G 208*2.25/255 = 1.8352941, B 128*2.25/255 = 1.1294118).
 *       Plain out saturates every channel.                      -> 0xFFFFFFFF
 *  SAT3 the same t, out.rgb = t * c0 with c0 = 0x66/255 = 0.4, alpha = t.a:
 *       R G 1.8352941*0.4 = 0.7341176 -> 187.2 -> 0xBB; B 1.1294118*0.4 =
 *       0.4517647 -> 115.2 -> 0x73; A 2.25 -> saturates 0xFF.   -> 0xFFBBBB73
 *       A stage that clamped t to 1 would give 1*0.4 = 0x66 -> 0xFF666666.
 *  SAT4 scale 0.5, offset -1 -> k = -0.5: every channel of t is negative and
 *       UNSIGNED_IDENTITY reads max(x, 0) = 0, alpha too.       -> 0x00000000 */
static const BsfDraw k_sat[] = {
    { .what = "mode 7 k = 1.5: the product's alpha saturates at the output: 0xFFA8A8C0",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(3, 2), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 1.0f }, .offset = { 0, 0.5f }, .out_reg = 9, .want = 0xFFA8A8C0u },
    { .what = "mode 7 k = 2.25: every channel saturates at the output: 0xFFFFFFFF",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(3, 2), D_TC(6.5f, 6.5f, 1.0f) },
      .scale = { 0, 1.0f }, .offset = { 0, 1.25f }, .out_reg = 9, .want = 0xFFFFFFFFu },
    { .what = "mode 7 k = 2.25 scaled by c0 0.4 keeps the unclamped product: 0xFFBBBB73",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(3, 2), D_TC(6.5f, 6.5f, 1.0f) },
      .scale = { 0, 1.0f }, .offset = { 0, 1.25f }, .out_reg = 9,
      .final_c0 = 0x66666666u, .want = 0xFFBBBB73u },
    { .what = "mode 7 k = -0.5 reads zero through UNSIGNED_IDENTITY: 0x00000000",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(3, 2), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0.5f }, .offset = { 0, -1.0f }, .out_reg = 9, .want = 0x00000000u },
};

/* ── Mode 7 at stage 3: the source chosen by bits 20-23 ────────────────────────
 * Stage 3 cannot bind a texture through the committed stream (the walk admits
 * no stage-3 OFFSET/FORMAT), so its sample is the unusable-texture white
 * (1,1,1,1) and the output is (k, k, k, k): only the luminance is observable.
 * Stages 0, 1, 2 sample S(3,2) (lum 0xFF), S(0,3) (lum 0xC0), S(1,3) (lum
 * 0x60); stage 3 scale 0.5, offset 0.125; out = t3.
 *  bits 20-23 = 1 (stage 1, NOT the previous stage):
 *      k = 0.5*0.7529412 + 0.125 = 0.5014706 -> 127.875 -> 0x80   -> 0x80808080
 *  bits 20-23 = 0: k = 0.5*1 + 0.125 = 0.625 -> 159.375 -> 0x9F  -> 0x9F9F9F9F
 *  bits 20-23 = 2 (the previous stage): k = 0.5*0.3764706 + 0.125 =
 *      0.3132353 -> 79.875 -> 0x50                                -> 0x50505050
 *  bits 16-19 = 1, bits 20-23 = 0: stage 3 still reads stage 0     -> 0x9F9F9F9F */
static const BsfDraw k_source3[] = {
    { .what = "stage 3 mode 7 reads stage 1 when bits 20-23 = 1: 0x80808080",
      .prog = PROG(1, 1, 1, 7), .osi = 1u << 20,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_S },
      .tc = { S_TC(3, 2), S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0, 0.5f }, .offset = { 0, 0, 0, 0.125f },
      .out_reg = 11, .want = 0x80808080u },
    { .what = "stage 3 mode 7 reads stage 0 when bits 20-23 = 0: 0x9F9F9F9F",
      .prog = PROG(1, 1, 1, 7), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_S },
      .tc = { S_TC(3, 2), S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0, 0.5f }, .offset = { 0, 0, 0, 0.125f },
      .out_reg = 11, .want = 0x9F9F9F9Fu },
    { .what = "stage 3 mode 7 reads stage 2 when bits 20-23 = 2: 0x50505050",
      .prog = PROG(1, 1, 1, 7), .osi = 2u << 20,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_S },
      .tc = { S_TC(3, 2), S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0, 0.5f }, .offset = { 0, 0, 0, 0.125f },
      .out_reg = 11, .want = 0x50505050u },
    { .what = "stage 3 ignores stage 2's nibble (bits 16-19 = 1, 20-23 = 0): 0x9F9F9F9F",
      .prog = PROG(1, 1, 1, 7), .osi = 1u << 16,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_S },
      .tc = { S_TC(3, 2), S_TC(0, 3), S_TC(1, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0, 0, 0.5f }, .offset = { 0, 0, 0, 0.125f },
      .out_reg = 11, .want = 0x9F9F9F9Fu },
};

/* ── Bump modes ignore texture q (R4) ──────────────────────────────────────────
 * Stage 1 BUMPENVMAP, source S(1,0) (du = +1), DIAG(0.25), base (3.5, 3.5)
 * texels -> D(5,3) = 0xFFB07080 whatever q is. With PROJECT2D's divide put
 * back, (s/q + dsdt.x, t/q):
 *   q = 2:   (1.75 + 2, 1.75)  -> D(3,1)
 *   q = 4:   (0.875 + 2, 0.875) -> D(2,0)
 *   q = 0.5: (7 + 2, 7)         -> clamped D(7,7)
 * Mode 7 (L1's state) at q = 2 is 0xA0464650 (divided: D(1,1)*k). Stage 2
 * BUMPENVMAP (k_source2's first draw) at q = 2 is D(5,3). */
static const BsfDraw k_bump_q[] = {
    { .what = "stage 1 mode 6 at q = 2 samples (s, t) + dsdt: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 2.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
    { .what = "stage 1 mode 6 at q = 4 samples (s, t) + dsdt: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 4.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
    { .what = "stage 1 mode 6 at q = 0.5 samples (s, t) + dsdt: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 0.5f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
    { .what = "stage 1 mode 7 at q = 2 keeps texel and luminance: 0xA0464650",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 3), D_TC(3.5f, 3.5f, 2.0f) },
      .scale = { 0, 0.5f }, .offset = { 0, 0.25f }, .out_reg = 9, .want = 0xA0464650u },
    { .what = "stage 2 mode 6 at q = 2 samples (s, t) + dsdt: D(5,3)",
      .prog = PROG(1, 1, 6, 0), .osi = 0u,
      .tex = { BSF_TEX_S, BSF_TEX_S, BSF_TEX_D },
      .tc = { S_TC(1, 0), S_TC(3, 0), D_TC(3.5f, 3.5f, 2.0f) },
      .mat = { {0}, {0}, DIAG(0.25f) }, .out_reg = 10, .want = 0xFFB07080u },
};

/* ── PROJECT2D (mode 1) MUST divide by q: the positive controls ────────────────
 *  stage 1 at (6.5, 5.5) texels, q = 1 -> D(6,5) = 0xFFD0B080
 *  the same (s, t) at q = 2 -> (3.25, 2.75) -> D(3,2) = 0xFF705080
 *  stage 2 at (7.5, 7.5) texels, q = 4 -> (1.875, 1.875) -> D(1,1) = 0xFF303080
 *  (undivided, the last two would be D(6,5) and D(7,7) = 0xFFF0F080) */
static const BsfDraw k_project2d[] = {
    { .what = "stage 1 PROJECT2D at q = 1 samples (s, t): D(6,5)",
      .prog = PROG(0, 1, 0, 0), .tex = { BSF_TEX_NONE, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { NO_TC, D_TC(6.5f, 5.5f, 1.0f) }, .out_reg = 9, .want = 0xFFD0B080u },
    { .what = "stage 1 PROJECT2D at q = 2 samples (s/q, t/q): D(3,2)",
      .prog = PROG(0, 1, 0, 0), .tex = { BSF_TEX_NONE, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { NO_TC, D_TC(6.5f, 5.5f, 2.0f) }, .out_reg = 9, .want = 0xFF705080u },
    { .what = "stage 2 PROJECT2D at q = 4 samples (s/q, t/q): D(1,1)",
      .prog = PROG(0, 0, 1, 0), .tex = { BSF_TEX_NONE, BSF_TEX_NONE, BSF_TEX_D },
      .tc = { NO_TC, NO_TC, D_TC(7.5f, 7.5f, 4.0f) }, .out_reg = 10, .want = 0xFF303080u },
};

/* ── Preserved refusals: modes 8..31 ───────────────────────────────────────────
 * The CPU executor has no fallback path: a dot-product mode reads the zero
 * texel (0,0,0,0) (nv2a_pb_exec.c rc_stage_fetch `default`), so a final
 * combiner emitting that stage writes 0x00000000. The Vulkan back end refuses
 * the batch (nv2a_vk_rc_coverage.h: mode > 7, reason 1) and draws it the
 * stage-0 way; its pixel is not a literal here, its refusal is. Mode 7 is the
 * control: the last mode the combiner pipeline admits. */
static const BsfDraw k_refuse_modes[] = {
    { .what = "mode 8 at stage 1 is refused: the CPU stage reads zero, 0x00000000",
      .prog = PROG(1, 8, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.5f, 3.5f, 1.0f) }, .out_reg = 9, .want = 0x00000000u,
      .vk = BSF_VK_FALLBACK_MODE, .vk_stage = 1, .vk_mode = 8 },
    { .what = "mode 17 at stage 2 is refused: the CPU stage reads zero, 0x00000000",
      .prog = PROG(1, 0, 17, 0), .tex = { BSF_TEX_S, BSF_TEX_NONE, BSF_TEX_D },
      .tc = { S_TC(0, 0), NO_TC, D_TC(3.5f, 3.5f, 1.0f) }, .out_reg = 10,
      .want = 0x00000000u, .vk = BSF_VK_FALLBACK_MODE, .vk_stage = 2, .vk_mode = 17 },
    { .what = "mode 31 at stage 1 is refused: the CPU stage reads zero, 0x00000000",
      .prog = PROG(1, 31, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.5f, 3.5f, 1.0f) }, .out_reg = 9, .want = 0x00000000u,
      .vk = BSF_VK_FALLBACK_MODE, .vk_stage = 1, .vk_mode = 31 },
    { .what = "mode 7 at stage 1 is admitted (the threshold control): 0xA0464650",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 3), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0.5f }, .offset = { 0, 0.25f }, .out_reg = 9, .want = 0xA0464650u },
};

/* ── Preserved refusal: CUBEMAP (mode 3) on a true cube texture ────────────────
 * Stage 1 binds D with FORMAT's cube bit and samples direction (1, -0.1, 0.1).
 * The CPU executor does not refuse it: major axis +X, so face 0 (D's own
 * image), sc = -z = -0.1, tc = -y = 0.1, ma = 1 (the GL cube table), giving
 * ((sc/ma + 1)/2, (tc/ma + 1)/2) = (0.45, 0.55) -> (3.6, 4.4) texels -> D(3,4)
 * = 0xFF709080. The Vulkan back end refuses it (reason 2). The control:
 * CUBEMAP on the 2D D samples (s, t) directly, (3.5, 4.5) -> D(3,4), and is
 * admitted. (A stage-3 cube cannot be named at all: see
 * limitation_stage3_texture.) */
static const BsfDraw k_refuse_cube[] = {
    { .what = "mode 3 on a cube texture: CPU samples face +X, D(3,4)",
      .prog = PROG(1, 3, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D_CUBE, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), { 1.0f, -0.1f, 0.1f, 1.0f } }, .out_reg = 9,
      .want = 0xFF709080u, .vk = BSF_VK_FALLBACK_CUBE, .vk_stage = 1, .vk_mode = 3 },
    { .what = "mode 3 on a 2D texture is admitted and samples (s, t): D(3,4)",
      .prog = PROG(1, 3, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.5f, 4.5f, 1.0f) }, .out_reg = 9, .want = 0xFF709080u },
};

/* ── LIMITATION: stage 3 binds no texture ──────────────────────────────────────
 * The walk admits stage 3's ADDRESS, CONTROL0, FILTER and BORDER words but not
 * its OFFSET (0x1BC0) or FORMAT (0x1BC4) (src/nv2a/nv2a_method_table.c, an
 * inventory measured from the title). So a stage-3 bump stage samples the
 * unusable-texture white (1,1,1,1) whatever its delta: mode 6 at stage 3 with
 * a +2-texel delta from stage 0 still writes 0xFFFFFFFF. This is CURRENT
 * behaviour, asserted as a limitation, not correctness; it is also why a
 * "stage-3 cube" refusal is unreachable through the committed stream. */
static const BsfDraw k_stage3_white = {
    .what = "LIMITATION stage 3 mode 6 samples the unbound white texel: 0xFFFFFFFF",
    .prog = PROG(1, 0, 0, 6), .osi = 0u, .tex = { BSF_TEX_S, BSF_TEX_NONE, BSF_TEX_NONE },
    .tc = { S_TC(1, 0), NO_TC, NO_TC, D_TC(3.5f, 3.5f, 1.0f) },
    .mat = { {0}, {0}, {0}, DIAG(0.25f) }, .out_reg = 11, .want = 0xFFFFFFFFu };

/* ── LIMITATION: a rect (linear) texture cannot be a bump destination ──────────
 * A linear texture needs SET_TEXTURE_CONTROL1 (pitch) and IMAGE_RECT; the walk
 * admits both for stage 0 only (0x1B10, 0x1B1C), and stage 0 is never a bump
 * stage (R1). So the rect coordinate remap on a bump stage is unreachable
 * through the committed stream. The control: stage 0 LIN_A8R8G8B8, which is
 * addressed in texels, (3.5, 3.5) -> D(3,3) = 0xFF707080. */
static const BsfDraw k_rect_stage0 = {
    .what = "stage 0 linear (rect) texture is admitted and addressed in texels: D(3,3)",
    .prog = PROG(1, 0, 0, 0), .tex = { BSF_TEX_D_LINEAR, BSF_TEX_NONE, BSF_TEX_NONE },
    .tc = { { 3.5f, 3.5f, 0.0f, 1.0f } }, .out_reg = 8, .want = 0xFF707080u };

/* ── LIMITATION: a signed (V8U8) source is not decoded ─────────────────────────
 * Xbox D3D's V8U8 is NV2A colour 0x28 (G8B8); neither the CPU sampler
 * (nv2a_pb_exec.c sample_tex has no 0x28 case) nor the Vulkan upload (the
 * production decoder rejects it) decodes it, so the source stage reads the
 * unusable-texture white: du = dv = sign3(1.0) = -1/127 and lum = 1. CURRENT
 * behaviour, asserted as a limitation:
 *  mode 6, DIAG(127/16), base (3.25, 3.25): dsdt = -0.5 texel each
 *      -> (2.75, 2.75) -> D(2,2) = 0xFF505080
 *  mode 7, zero matrix, scale 0.25, offset 0.125 -> k = 0.375 on D(3,3):
 *      A 95.63 -> 0x60; R G 112*0.375 = 42 -> 0x2A; B 48 -> 0x30 -> 0x602A2A30 */
static const BsfDraw k_signed_src[] = {
    { .what = "LIMITATION a G8B8 (V8U8) source reads white: delta -1/127 each, D(2,2)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_G8B8, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.25f, 3.25f, 1.0f) }, .mat = { {0}, DIAG(GAIN_HALF_TEXEL) },
      .out_reg = 9, .want = 0xFF505080u },
    { .what = "LIMITATION a G8B8 (V8U8) source reads white: lum 1, 0x602A2A30",
      .prog = PROG(1, 7, 0, 0), .tex = { BSF_TEX_G8B8, BSF_TEX_D, BSF_TEX_NONE },
      .tc = { S_TC(0, 0), D_TC(3.5f, 3.5f, 1.0f) },
      .scale = { 0, 0.25f }, .offset = { 0, 0.125f }, .out_reg = 9, .want = 0x602A2A30u },
};

/* ── LIMITATION: cube and volume bump destinations ─────────────────────────────
 * A bump stage whose own texture is a cube or a volume samples it as the 2D
 * image at its base (face 0 / the first w x h texels): the executor stores no
 * depth, and both renderers treat mode 6/7 on a cube as face 0 (nv2a_pb_exec.c
 * ~3222, nv2a_vk_rc.hlsl). xemu itself cannot draw either (a vec2 into a cube
 * sampler; a vec3 with an unspecified r). CURRENT behaviour: source S(1,0)
 * (du = +1), DIAG(0.25) -> D(5,3) = 0xFFB07080 for both, and the Vulkan back
 * end admits both (no refusal). */
static const BsfDraw k_cube_volume[] = {
    { .what = "LIMITATION mode 6 on a cube destination samples face 0 as 2D: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D_CUBE, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
    { .what = "LIMITATION mode 6 on a volume destination samples its base as 2D: D(5,3)",
      .prog = PROG(1, 6, 0, 0), .tex = { BSF_TEX_S, BSF_TEX_D_VOLUME, BSF_TEX_NONE },
      .tc = { S_TC(1, 0), D_TC(3.5f, 3.5f, 1.0f) }, .mat = { {0}, DIAG(0.25f) },
      .out_reg = 9, .want = 0xFFB07080u },
};

/* ── The case table ─────────────────────────────────────────────────────────── */

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

static void run_draws(const BsfDraw *d, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        check_draw(&d[i]);
}

static void case_signed_du(void)      { run_draws(k_signed_du, N_OF(k_signed_du)); }
static void case_signed_dv(void)      { run_draws(k_signed_dv, N_OF(k_signed_dv)); }
static void case_asym(void)           { run_draws(k_asym, N_OF(k_asym)); }
static void case_source2(void)        { run_draws(k_source2, N_OF(k_source2)); }
static void case_lum(void)            { run_draws(k_lum, N_OF(k_lum)); }
static void case_sat(void)            { run_draws(k_sat, N_OF(k_sat)); }
static void case_source3(void)        { run_draws(k_source3, N_OF(k_source3)); }
static void case_bump_q(void)         { run_draws(k_bump_q, N_OF(k_bump_q)); }
static void case_project2d(void)      { run_draws(k_project2d, N_OF(k_project2d)); }
static void case_refuse_modes(void)   { run_draws(k_refuse_modes, N_OF(k_refuse_modes)); }
static void case_refuse_cube(void)    { run_draws(k_refuse_cube, N_OF(k_refuse_cube)); }
static void case_signed_src(void)     { run_draws(k_signed_src, N_OF(k_signed_src)); }
static void case_cube_volume(void)    { run_draws(k_cube_volume, N_OF(k_cube_volume)); }

static void case_stage3_texture(void)
{
    /* The positive control first: the same stream without the extra word. */
    check_draw(&k_stage3_white);
    check_rejected(&k_stage3_white, 0x1BC0u, BSF_TEX_D_VA,
                   "LIMITATION stage 3 SET_TEXTURE_OFFSET (0x1BC0) is refused by the walk");
    check_rejected(&k_stage3_white, 0x1BC4u, 0x03310625u,
                   "LIMITATION stage 3 SET_TEXTURE_FORMAT (0x1BC4, cube bit set) is refused");
    case_note("stage 3 can bind no texture through the committed stream, so a stage-3 "
              "cube (and a stage-3 bump destination) is unreachable there");
}

static void case_rect(void)
{
    check_draw(&k_rect_stage0);
    check_rejected(&k_lum[0], 0x1B50u, 32u << 16,
                   "LIMITATION stage 1 SET_TEXTURE_CONTROL1 (0x1B50, pitch) is refused");
    check_rejected(&k_lum[0], 0x1B5Cu, (8u << 16) | 8u,
                   "LIMITATION stage 1 SET_TEXTURE_IMAGE_RECT (0x1B5C) is refused");
    check_rejected(&k_source2[0], 0x1B90u, 32u << 16,
                   "LIMITATION stage 2 SET_TEXTURE_CONTROL1 (0x1B90, pitch) is refused");
    check_rejected(&k_source2[0], 0x1B9Cu, (8u << 16) | 8u,
                   "LIMITATION stage 2 SET_TEXTURE_IMAGE_RECT (0x1B9C) is refused");
    case_note("a linear texture is configurable at stage 0 only, which is never a bump "
              "stage: the rect remap on a bump stage is unreachable");
}

typedef struct { const char *name; void (*fn)(void); } BsfCase;

static const BsfCase g_bsf_cases[] = {
    { "mode6_signed_du",            case_signed_du },
    { "mode6_signed_dv",            case_signed_dv },
    { "mode6_asymmetric_matrix",    case_asym },
    { "mode6_stage2_source",        case_source2 },
    { "mode7_luminance",            case_lum },
    { "mode7_saturation",           case_sat },
    { "mode7_stage3_source",        case_source3 },
    { "bump_q_ignored",             case_bump_q },
    { "project2d_q_divides",        case_project2d },
    { "refusal_dot_modes",          case_refuse_modes },
    { "refusal_cube_mode3",         case_refuse_cube },
    { "limitation_stage3_texture",  case_stage3_texture },
    { "limitation_rect_bump",       case_rect },
    { "limitation_signed_source",   case_signed_src },
    { "limitation_cube_volume_dest", case_cube_volume },
};

static const BsfCase *bsf_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(g_bsf_cases); i++)
        if (strcmp(g_bsf_cases[i].name, name) == 0)
            return &g_bsf_cases[i];
    return NULL;
}

#endif
