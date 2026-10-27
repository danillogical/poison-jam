/* The NV2A window-clip deciding gate: the cases and their literal expectations,
 * shared by the committed-stream CPU half (tests/test_wclip_stream.c) and the
 * Vulkan half (tests/test_wclip_stream_vk.c).
 *
 * Each half #includes this file and supplies:
 *   run_case(c)    submit c->draw, then check every probe against its own renderer's
 *                  column (`cpu` or `vk`) and report failures by the probe's name
 * so the SAME hand-derived literal is checked by each renderer independently;
 * agreement between them is never the test.
 *
 * No expectation is read from a production helper or a probe: each is derived
 * below by hand from the hardware rule stated next (facts read from xemu's
 * pgraph method handlers and window-clip shader text; no code copied).
 *
 * ── The hardware rule used ───────────────────────────────────────────────────
 * W1 Registers, class NV097, subchannel 0.
 *      SET_WINDOW_CLIP_TYPE          0x02B4          bit 0: 0 inclusive, 1 exclusive
 *      SET_WINDOW_CLIP_HORIZONTAL(i) 0x02C0 + 4*i    xmin bits 0-11, xmax bits 16-27
 *      SET_WINDOW_CLIP_VERTICAL(i)   0x02E0 + 4*i    ymin bits 0-11, ymax bits 16-27
 *    i = 0..7. Writing slot i also writes slots i..7 (the same word lands in
 *    every later slot), so D3D's single scissor, a write to slot 0, sets all eight.
 * W2 Rect i covers logical pixels x in [xmin, xmax] and y in [ymin, ymax], BOTH
 *    ends inclusive. With anti-aliasing (SET_SURFACE_FORMAT bits 12-15: 1 = 2x1,
 *    2 = 2x2) it covers real pixels [xmin*sx, (xmax+1)*sx), likewise y.
 * W3 Inclusive: a pixel is written only if inside at least one of the 8 rects.
 *    Exclusive: only if inside none. The effective region is also intersected
 *    with the surface clip. Coordinates are surface pixels in the vertices' space.
 * W4 (a model choice, not hardware: reset registers are 0) Until any window-clip
 *    method is written there is no window clip; after one,
 *    all 8 registers count (unwritten ones are 0, i.e. the 1-pixel rect (0,0)).
 * W5 Clears are out of scope: the clear only sets the background here.
 *
 * ── The planned implementation's limitations (labelled LIMITATION below) ─────
 * CPU executor: exact. Vulkan back end: one scissor. Exact when the 8 rects are
 * identical. Inclusive with distinct rects: the scissor is the bounding box of
 * the union, intersected with the surface clip (not gated: see the next note).
 * Exclusive: no window clip.
 *
 * Slots 1-7 (0x02C4..0x02DC, 0x02E4..0x02FC) are not admitted by the walk until
 * a run witnesses them, so union semantics have no stream gate.
 *
 * ── The scene (tests/wclip_stream_fixture.c) ────────────────────────────────
 * A 64x64 real-pixel A8R8G8B8 surface cleared to WCF_BACKGROUND, then a quad of
 * WCF_QUAD covering the whole logical surface. A probed pixel therefore reads
 * WCF_QUAD where the draw wrote it and WCF_BACKGROUND where it did not.
 *
 * ── What today's toolkit does, so a failing assertion is expected to be named ─
 * nv2a_pb_exec.c accepts 0x02B4/0x02C0/0x02E0 but ignores them, and
 * nv2a_method_table.c admits ONLY slot 0 of each axis (0x02C0, 0x02E0), not
 * 0x02C4..0x02DC or 0x02E4..0x02FC. So today: every probe that expects
 * background inside the surface clip fails (the quad is drawn everywhere).
 */
#ifndef JSRF_WCLIP_STREAM_CASES_H
#define JSRF_WCLIP_STREAM_CASES_H

#include "wclip_stream_fixture.h"

#include <stddef.h>
#include <string.h>

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

/* One named pixel. `cpu` / `vk`: 1 = the quad is expected there, 0 = the background.
 * Where `vk` differs from `cpu` the Vulkan half reports `vk_what` (a LIMITATION label). */
typedef struct {
    const char *what;
    uint32_t x, y;          /* real surface pixels */
    int cpu, vk;
    const char *vk_what;    /* NULL: same as `what` */
} WcProbe;

typedef struct {
    const char *name;
    WcDraw draw;
    const WcProbe *probes;
    size_t nprobes;
} WcCase;

/* ── 1. single: the D3D shape ─────────────────────────────────────────────────
 * TYPE 0, one write to slot 0 of each axis (so all eight slots hold it), rect
 * x 10..41, y 7..30 on the 64x64 surface (no AA, surface clip = whole surface).
 * Per W2 the covered pixels are x in [10, 41], y in [7, 30]. CPU and Vulkan
 * identical (the eight rects are identical, so one scissor is exact). */
static const WcProbe k_single_p[] = {
    { "inside xmin/ymin corner (10,7) is drawn",      10,  7, 1, 1, NULL },
    { "inside xmax/ymax corner (41,30) is drawn",     41, 30, 1, 1, NULL },
    { "inside centre (25,20) is drawn",               25, 20, 1, 1, NULL },
    { "outside xmin-1 (9,20) is background",           9, 20, 0, 0, NULL },
    { "outside xmax+1 (42,20) is background",         42, 20, 0, 0, NULL },
    { "outside ymin-1 (25,6) is background",          25,  6, 0, 0, NULL },
    { "outside ymax+1 (25,31) is background",         25, 31, 0, 0, NULL },
    { "far corner (0,0) is background",                0,  0, 0, 0, NULL },
};

/* ── 2. full: JSRF's title value scaled down ──────────────────────────────────
 * TYPE 0, slot 0 = x 0..63, y 0..63 (xmax = W-1, ymax = H-1). By W2 the rect is
 * the whole 64x64 surface, so every pixel is drawn. Passes on today's code too:
 * it is the no-regression case. */
static const WcProbe k_full_p[] = {
    { "top-left (0,0) is drawn",        0,  0, 1, 1, NULL },
    { "top-right (63,0) is drawn",     63,  0, 1, 1, NULL },
    { "bottom-left (0,63) is drawn",    0, 63, 1, 1, NULL },
    { "bottom-right (63,63) is drawn", 63, 63, 1, 1, NULL },
    { "centre (32,32) is drawn",       32, 32, 1, 1, NULL },
};

/* ── 3. exclusive ─────────────────────────────────────────────────────────────
 * TYPE 1, slot 0 = x 20..39, y 20..39 (all eight slots). CPU (W3): drawn iff
 * inside NONE of the rects, so the square x,y in [20, 39] is background and
 * everything else is drawn. Vulkan LIMITATION: no window clip at all, so the
 * square is drawn too. */
static const WcProbe k_excl_p[] = {
    { "inside xmin/ymin corner (20,20) is not drawn",     20, 20, 0, 1,
      "LIMITATION Vulkan ignores exclusive clips: (20,20) is drawn" },
    { "inside xmax/ymax corner (39,39) is not drawn",     39, 39, 0, 1,
      "LIMITATION Vulkan ignores exclusive clips: (39,39) is drawn" },
    { "inside centre (30,30) is not drawn",               30, 30, 0, 1,
      "LIMITATION Vulkan ignores exclusive clips: (30,30) is drawn" },
    { "outside xmin-1 (19,30) is drawn",                  19, 30, 1, 1, NULL },
    { "outside xmax+1 (40,30) is drawn",                  40, 30, 1, 1, NULL },
    { "outside ymin-1 (30,19) is drawn",                  30, 19, 1, 1, NULL },
    { "outside ymax+1 (30,40) is drawn",                  30, 40, 1, 1, NULL },
    { "corner (0,0) is drawn",                             0,  0, 1, 1, NULL },
    { "corner (63,63) is drawn",                          63, 63, 1, 1, NULL },
};

/* ── 4. beyond_surface_clip ───────────────────────────────────────────────────
 * The surface clip is 48x48 at origin 0 on the 64x64 surface, and the window
 * clip is x 30..63, y 30..63 (TYPE 0, slot 0). W3: the region is the rect
 * intersected with the surface clip, x,y in [30, 47]. The clear ran before the
 * clip shrank, so pixels outside the surface clip hold the background. Both
 * renderers agree (one rect, so one scissor). */
static const WcProbe k_beyond_p[] = {
    { "inside xmin/ymin corner (30,30) is drawn",             30, 30, 1, 1, NULL },
    { "inside surface-clip edge (47,47) is drawn",            47, 47, 1, 1, NULL },
    { "inside (47,30) is drawn",                              47, 30, 1, 1, NULL },
    { "inside (40,40) is drawn",                              40, 40, 1, 1, NULL },
    { "outside xmin-1 (29,40) is background",                 29, 40, 0, 0, NULL },
    { "outside ymin-1 (40,29) is background",                 40, 29, 0, 0, NULL },
    { "in the rect but past the surface clip (50,40)",        50, 40, 0, 0, NULL },
    { "in the rect but below the surface clip (40,50)",       40, 50, 0, 0, NULL },
    { "in the rect but past both clip edges (63,63)",         63, 63, 0, 0, NULL },
    { "far corner (0,0) is background",                        0,  0, 0, 0, NULL },
};

/* ── 5. aa2x2 ─────────────────────────────────────────────────────────────────
 * SET_SURFACE_FORMAT AA = 2 (2x2): the 64x64 real surface is a 32x32 logical
 * one (surface clip 32x32, vertices 0..32). TYPE 0, slot 0 = logical x 5..9,
 * y 5..9. W2: real pixels [5*2, (9+1)*2) = [10, 20) in each axis, so 10 and 19
 * are the first and last drawn and 9 and 20 are not. Identical rects, so
 * Vulkan is exact. */
static const WcProbe k_aa_p[] = {
    { "inside first real pixel (10,10) is drawn",          10, 10, 1, 1, NULL },
    { "inside last real pixel (19,19) is drawn",           19, 19, 1, 1, NULL },
    { "inside (10,19) is drawn",                           10, 19, 1, 1, NULL },
    { "inside (19,10) is drawn",                           19, 10, 1, 1, NULL },
    { "outside xmin*2-1 (9,15) is background",              9, 15, 0, 0, NULL },
    { "outside (xmax+1)*2 (20,15) is background",          20, 15, 0, 0, NULL },
    { "outside ymin*2-1 (15,9) is background",             15,  9, 0, 0, NULL },
    { "outside (ymax+1)*2 (15,20) is background",          15, 20, 0, 0, NULL },
    { "far corner (0,0) is background",                     0,  0, 0, 0, NULL },
    { "far corner (63,63) is background",                  63, 63, 0, 0, NULL },
};

static const WcCase g_wc_cases[] = {
    { "single",
      { 0, 0, 0, 1, 0, 1, { { 0, 10, 41, 7, 30 } } },
      k_single_p, N_OF(k_single_p) },
    { "full",
      { 0, 0, 0, 1, 0, 1, { { 0, 0, 63, 0, 63 } } },
      k_full_p, N_OF(k_full_p) },
    { "exclusive",
      { 0, 0, 0, 1, 1, 1, { { 0, 20, 39, 20, 39 } } },
      k_excl_p, N_OF(k_excl_p) },
    { "beyond_surface_clip",
      { 0, 48, 48, 1, 0, 1, { { 0, 30, 63, 30, 63 } } },
      k_beyond_p, N_OF(k_beyond_p) },
    { "aa2x2",
      { 2, 0, 0, 1, 0, 1, { { 0, 5, 9, 5, 9 } } },
      k_aa_p, N_OF(k_aa_p) },
};

static const WcCase *wc_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(g_wc_cases); i++)
        if (strcmp(g_wc_cases[i].name, name) == 0)
            return &g_wc_cases[i];
    return NULL;
}

#endif
