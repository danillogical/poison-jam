/* The NV2A per-zeta depth deciding gate: the cases and their literal expectations,
 * shared by the committed-stream CPU half (tests/test_zeta_stream.c) and the Vulkan
 * half (tests/test_zeta_stream_vk.c).
 *
 * Each half #includes this file and supplies run_case(c), which submits c->draw and
 * checks every probe against the literal below; the SAME hand-derived literal is
 * checked by each renderer independently, and agreement between them is never the
 * test. No expectation is read from a production helper or a probe.
 *
 * ── The rule (ledger L62: "depth per zeta address") ──────────────────────────
 * Z1 Two passes that use different SET_SURFACE_ZETA_OFFSET (method 0x0214)
 *    addresses do not share depth contents: a depth clear of one leaves the other
 *    as it was.
 * Z2 Two passes that use the same zeta address share them: a depth clear of it
 *    resets what an earlier pass wrote.
 *
 * ── The scene (tests/zeta_stream_fixture.c) ──────────────────────────────────
 * One colour target C (32x32, linear A8R8G8B8), depth test LESS (0x201), depth
 * writes on, SET_CLIP_MAX = 2^24-1 (the Z24 scale), SET_ZSTENCIL_CLEAR_VALUE
 * 0xFFFFFF00 (depth 0xFFFFFF = 1.0, stencil 0), colour clear 0xFF102030.
 *   step 1  zeta Z1: CLEAR_SURFACE colour + depth -> C = 0xFF102030 everywhere,
 *           Z1 = 1.0 everywhere. Draw quad P, colour 0xFF4488CC, x in [0,16), full
 *           height, depth 0.25 (4194303.75 of 16777215). 0.25 < 1.0 passes:
 *           C = P over x < 16, and Z1 = 0.25 there.
 *   step 2  zeta = mid_zeta (Z2 in `distinct`, Z1 again in `same`): CLEAR_SURFACE
 *           depth only (colour untouched), no draw. Z2's depth becomes 1.0.
 *   step 3  zeta Z1, no clear: draw quad Q, colour 0xFFEE7711, x in [0,24), full
 *           height, depth 0.5 (8388607.5 of 16777215).
 * Both quads are two triangles (upper-left, lower-right of the diagonal
 * x/qw + y/32 = 1); the probes put one pixel of each triangle in every region.
 * Pixel centres are used (x+0.5, y+0.5), none within a pixel of an edge.
 *
 * ── How each step's depth behaviour follows, and the literals ────────────────
 * distinct (mid_zeta = Z2). Step 2 clears Z2 only (Z1). Z1 still holds 0.25 for
 * x < 16 and 1.0 for 16 <= x < 24 (never drawn since step 1).
 *   x < 16:        Q at 0.5 vs 0.25: LESS fails -> C keeps P:      0xFF4488CC
 *   16 <= x < 24:  Q at 0.5 vs 1.0:  LESS passes -> Q:             0xFFEE7711
 *   x >= 24:       no quad covers it, step 1's colour clear:        0xFF102030
 *   If the back end shared one depth buffer for both addresses, step 2's clear
 *   would reset it to 1.0, Q would pass over x < 16 and those probes would read
 *   0xFFEE7711 instead.
 * same (mid_zeta = Z1). Step 2 clears the very buffer step 1 wrote, so Z1 = 1.0
 *   everywhere again.
 *   x < 24:        Q at 0.5 vs 1.0:  LESS passes -> Q:             0xFFEE7711
 *   x >= 24:                                                       0xFF102030
 *   This is the sensitivity control: it proves the clear in step 2 reaches the
 *   buffer that step 3 tests against, so `distinct` surviving P is down to the
 *   separate address and nothing else (a draw that failed for another reason, or a
 *   clear that never took effect, would show here).
 * The 16 <= x < 24 pixels read Q in both cases, so Q's draw is proven to land.
 *
 * distinct_vp / same_vp: the same scene drawn in program mode (TRANSFORM_EXECUTION_MODE
 * 2, program oPos = v0, oD0 = v3), with the same literals as distinct / same on BOTH
 * halves. z reaches the depth compare unchanged: the CPU program path
 * (raster_xf_triangle) interpolates pos[2] after the divide by w = 1 and compares it
 * with the z buffer's raw clear value (zstencil >> 8 = 16777215); the Vulkan program
 * path hands pos[2] to vb_draw as Nv2aVertex.z, scaled by 1/16777215 from SET_CLIP_MAX.
 * Fixed function, same scale. `distinct` and `same` use the fixed-function path.
 *
 * Probes: (4,4) is in the upper-left triangle of both quads, (12,28) in the
 * lower-right one; (18,2) and (20,16) are Q-only, in Q's upper-left and
 * lower-right triangle; (28,4) and (28,16) are covered by neither.
 *
 * ── What the back ends do, so a failing assertion is expected to be named ────
 * CPU executor (nv2a_pb_exec.c zbuf_current): up to four float z buffers, each
 *    keyed by the raw zeta offset; a new one starts at 3.4e38 and CLEAR_SURFACE
 *    with the Z bit clears only the current one. Only the vertex-program raster path
 *    tests against them: same, distinct_vp and same_vp pass; fixed-function `distinct`
 *    expects the labelled LIMITATION literal below.
 * Vulkan (nv2a_vk_backend.c vb_depth_for): VB_MAX_DEPTHS (8) depth images keyed by
 *    (zeta address, w, h); a CLEAR_SURFACE with the Z bit clears the image
 *    vb_depth_for(rs->zeta_va) returns. Both cases are expected to pass. The
 *    Vulkan half also asserts the number of live depth images: 2 in `distinct`,
 *    1 in `same`.
 */
#ifndef JSRF_ZETA_STREAM_CASES_H
#define JSRF_ZETA_STREAM_CASES_H

#include "zeta_stream_fixture.h"

#include <stddef.h>
#include <string.h>

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

#define LIT_P   0xFF4488CCu         /* quad P survives */
#define LIT_Q   0xFFEE7711u         /* quad Q drawn */
#define LIT_BG  0xFF102030u         /* the colour clear */

/* One named pixel of C and the ARGB literal it must read. */
typedef struct {
    const char *what;
    uint32_t x, y;
    uint32_t want;
} ZProbe;

typedef struct {
    const char *name;
    ZDraw draw;
    unsigned want_depths;           /* live depth images afterwards (Vulkan half) */
    const ZProbe *probes;           /* the hardware rule, checked by the Vulkan half */
    size_t nprobes;
    const ZProbe *cpu_probes;       /* the CPU half's probes when they differ, else NULL */
    size_t ncpu_probes;
} ZCase;

/* The four rows that do not depend on the depth rule. */
#define TAIL_PROBES \
    { "Q-only region, upper-left triangle (18,2)", 18, 2, LIT_Q }, \
    { "Q-only region, lower-right triangle (20,16)", 20, 16, LIT_Q }, \
    { "outside both quads (28,4)",              28,  4, LIT_BG }, \
    { "outside both quads (28,16)",             28, 16, LIT_BG }

static const ZProbe k_distinct_probes[] = {
    { "P region, upper-left triangle (4,4)",    4,  4, LIT_P },
    { "P region, lower-right triangle (12,28)", 12, 28, LIT_P },
    TAIL_PROBES
};
static const ZProbe k_same_probes[] = {
    { "P region, upper-left triangle (4,4)",    4,  4, LIT_Q },
    { "P region, lower-right triangle (12,28)", 12, 28, LIT_Q },
    TAIL_PROBES
};

/* ── LIMITATION: the CPU fixed-function rasteriser has no depth test ───────────
 * raster_triangle (nv2a_pb_exec.c, the screen-space / fixed-function path) never
 * reads a z buffer; only the vertex-program path (raster_xf_triangle, T.zb =
 * zbuf_current(1)) does. So on the CPU, fixed-function `distinct` draws Q over P
 * whatever the zeta addresses are, and reads Q in P's area instead of the hardware
 * rule's P. `same` is unaffected (Q is drawn on the hardware too). The program-mode
 * cases (distinct_vp, same_vp) hold the hardware rule on the CPU. */
static const ZProbe k_distinct_cpu_probes[] = {
    { "LIMITATION CPU fixed-function rasteriser has no depth test: P area (4,4) reads Q",
      4, 4, LIT_Q },
    { "LIMITATION CPU fixed-function rasteriser has no depth test: P area (12,28) reads Q",
      12, 28, LIT_Q },
    TAIL_PROBES
};

/* mid_zeta, program; want_depths; probes (hardware rule); CPU-half probes or NULL. */
static const ZCase k_cases[] = {
    { "distinct",    { ZF_Z2_VA, 0 }, 2u, k_distinct_probes, N_OF(k_distinct_probes),
      k_distinct_cpu_probes, N_OF(k_distinct_cpu_probes) },
    { "same",        { ZF_Z1_VA, 0 }, 1u, k_same_probes,     N_OF(k_same_probes), NULL, 0 },
    { "distinct_vp", { ZF_Z2_VA, 1 }, 2u, k_distinct_probes, N_OF(k_distinct_probes), NULL, 0 },
    { "same_vp",     { ZF_Z1_VA, 1 }, 1u, k_same_probes,     N_OF(k_same_probes), NULL, 0 },
};

static const ZCase *z_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(k_cases); i++)
        if (strcmp(k_cases[i].name, name) == 0)
            return &k_cases[i];
    return NULL;
}

#endif
