/* The NV2A X8R8G8B8-alias deciding gate: the cases and their literal expectations,
 * shared by the committed-stream CPU half (tests/test_x8alias_stream.c) and the
 * Vulkan half (tests/test_x8alias_stream_vk.c).
 *
 * Each half #includes this file and supplies run_case(c), which submits c->draw and
 * checks every probe against the literal below; the SAME hand-derived literal is
 * checked by each renderer independently, and agreement between them is never the
 * test. No expectation is read from a production helper or a probe.
 *
 * ── The rule (owner-decided) ─────────────────────────────────────────────────
 * X1 A texture whose NV097 colour format is LIN_X8R8G8B8 (0x1E) reads alpha = 1.0
 *    (0xFF) for every texel, whatever the top byte of the stored dword holds.
 * X2 LIN_A8R8G8B8 (0x12) reads the stored alpha.
 *
 * ── The scene (tests/x8alias_stream_fixture.c) ───────────────────────────────
 * Surface A (32x32, linear A8R8G8B8, pitch 128) is cleared to X8F_CLEAR =
 * 0x40336699: alpha 0x40, R 0x33, G 0x66, B 0x99 (the dword is B8G8R8A8 byte for
 * byte). The clear is what makes A a live render target in the Vulkan back end.
 * Then a quad is drawn into surface B (a different address, A8R8G8B8, so alpha is
 * stored), stage 0 bound to A's address and pitch as a linear texture of the
 * case's format, nearest filter, clamp, diffuse white, texel coordinate (16.5,
 * 16.5) at every vertex. Every texel of A is 0x40336699, so the coordinate only
 * has to lie inside A. The quad is two triangles (upper-left, lower-right of the
 * diagonal x + y = 32); each case probes a pixel in each.
 *
 * ── How the texel's alpha reaches the stored pixel, per path ─────────────────
 * Fixed function, CPU (nv2a_pb_exec.c raster_triangle): sample_texture's texel is
 *    written by put_pixel unchanged; the diffuse is not even multiplied in. With
 *    blending off (the reset state; nothing in the stream enables it) the stored
 *    pixel is the texel, alpha byte included.
 * Fixed function, Vulkan (shaders/nv2a_vk_stage0.hlsl ps_main): c = diffuse * t0
 *    sample. Diffuse is 0xFFFFFFFF = (1,1,1,1), so c = the sampled texel in all
 *    four channels, including a; no alpha test is armed (func index -1); blending
 *    is off, so the B8G8R8A8 target stores c.a. 0x40/255 and 0x33/255 etc. round
 *    trip through UNORM8 exactly.
 * Register combiners, CPU (raster_xf_triangle with use_rc, nv2a_combiner.c): the
 *    final combiner is out = D + C*(1-A) + B*A with A = B = D = 0 and C = register
 *    8 (t0), so rgb = t0.rgb; CW1 bits 8-15 name the alpha input 0x18 = alpha of
 *    register 8, so out.a = t0.a. Zero general stages, so nothing else touches it.
 * Register combiners, Vulkan (shaders/nv2a_vk_rc.hlsl): the same final combiner,
 *    o.rgb = D + C*(1-A) + B*A and o.a = in_a(R[8]) from G; R[8] is the sampled
 *    stage-0 texel (the stage program is PROJECT2D, q = 1).
 * The program draws put the coordinate in oT0 from c[0] = (16.5, 16.5, 0, 1).
 *
 * ── The literals ─────────────────────────────────────────────────────────────
 * x8_ff   0x1E, fixed function. X1: alpha reads 0xFF, rgb 0x336699 -> 0xFF336699.
 * a8_ff   0x12, fixed function. X2: stored alpha 0x40 -> 0x40336699 (the no-
 *         regression control: the alias path is exercised here too).
 * x8_rc   0x1E, vertex program + final combiner emitting t0 -> 0xFF336699.
 * a8_rc   0x12, same path -> 0x40336699 (control; an addition to the requested set).
 * x8_self 0x1E, fixed function, the quad draws into A itself, covering x in
 *         [0, 16) of every row, sampling A at texel (24, 8) in the half it does not
 *         cover (so the texel read is 0x40336699 whatever order pixels land in; on
 *         Vulkan this is the snapshot-copy path). Drawn half: X1 -> 0xFF336699.
 *         Undrawn half: the clear value survives -> 0x40336699.
 *         Probes (4,4) is in the upper-left triangle (x/16 + y/32 < 1), (12,28)
 *         in the lower-right one; (20,4) and (28,28) lie outside the quad.
 *
 * ── What today's code does, so a failing assertion is expected to be named ───
 * CPU executor: sample_tex case 0x1E ORs in 0xFF000000, so every CPU case passes
 *    (the agreement baseline, not a red case).
 * Vulkan: vb_tex_aliases accepts 0x1E, and vb_texture hands back the target's own
 *    B8G8R8A8 view (or its snapshot copy), so the stored 0x40 leaks: x8_ff, x8_rc
 *    and the drawn-half probes of x8_self read 0x40336699 and FAIL on the alpha
 *    byte; a8_ff, a8_rc and the undrawn-half probes of x8_self PASS.
 * The Vulkan half also asserts, as a positive control against a vacuous pass, that
 * the draw really took the render-target alias path (g.st.rt_aliases +1, and
 * rt_copies +1 for x8_self only) and the intended shader (rc_draws).
 */
#ifndef JSRF_X8ALIAS_STREAM_CASES_H
#define JSRF_X8ALIAS_STREAM_CASES_H

#include "x8alias_stream_fixture.h"

#include <stddef.h>
#include <string.h>

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

#define LIT_X8   0xFF336699u        /* X1: alpha forced to 0xFF */
#define LIT_A8   0x40336699u        /* X2: the stored alpha */

/* One named pixel of one surface and the ARGB literal it must read. */
typedef struct {
    const char *what;
    uint32_t surf;                  /* X8F_SURF_A or X8F_SURF_B */
    uint32_t x, y;
    uint32_t want;
} X8Probe;

typedef struct {
    const char *name;
    X8Draw draw;
    const X8Probe *probes;
    size_t nprobes;
} X8Case;

/* Quad into B, whole surface: two probes in each triangle (x + y < 32 upper-left, > 32 lower-right). */
#define B_PROBES(lit) \
    { "B upper-left triangle (4,4)",   X8F_SURF_B,  4,  4, (lit) }, \
    { "B upper-left triangle (27,2)",  X8F_SURF_B, 27,  2, (lit) }, \
    { "B lower-right triangle (6,30)", X8F_SURF_B,  6, 30, (lit) }, \
    { "B lower-right triangle (28,28)", X8F_SURF_B, 28, 28, (lit) }

static const X8Probe k_x8_probes[] = { B_PROBES(LIT_X8) };
static const X8Probe k_a8_probes[] = { B_PROBES(LIT_A8) };

static const X8Probe k_self_probes[] = {
    { "A drawn half, upper-left triangle (4,4)",   X8F_SURF_A,  4,  4, LIT_X8 },
    { "A drawn half, lower-right triangle (12,28)", X8F_SURF_A, 12, 28, LIT_X8 },
    { "A undrawn half keeps the clear (20,4)",     X8F_SURF_A, 20,  4, LIT_A8 },
    { "A undrawn half keeps the clear (28,28)",    X8F_SURF_A, 28, 28, LIT_A8 },
};

static const X8Case k_cases[] = {
    { "x8_ff",   { 0x1E, 0, 0 }, k_x8_probes,   N_OF(k_x8_probes) },
    { "a8_ff",   { 0x12, 0, 0 }, k_a8_probes,   N_OF(k_a8_probes) },
    { "x8_rc",   { 0x1E, 1, 0 }, k_x8_probes,   N_OF(k_x8_probes) },
    { "a8_rc",   { 0x12, 1, 0 }, k_a8_probes,   N_OF(k_a8_probes) },
    { "x8_self", { 0x1E, 0, 1 }, k_self_probes, N_OF(k_self_probes) },
};

static const X8Case *x8_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(k_cases); i++)
        if (strcmp(k_cases[i].name, name) == 0)
            return &k_cases[i];
    return NULL;
}

#endif
