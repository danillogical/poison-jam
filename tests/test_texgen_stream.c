/* Independent deciding gate for NV2A fixed-function texgen and texture
 * matrices, D4 step 6: the COMMITTED-STREAM CPU half.
 *
 * Authored by a worker distinct from the production author. Tests only; no
 * production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (texgen_stream_fixture.c), so the walk's admission table,
 * the executor's method decoding (composite, model-view, inverse model-view,
 * normalisation, texgen modes and planes, texture-matrix enables and words,
 * vertex arrays, texture stages), its fixed-function batch classification,
 * the CPU fixed-function rasteriser and its sampler -- and, for the program
 * draw, the CPU vertex-program interpreter and combiners -- all come from the
 * linked toolkit library. No decoder is stubbed and no back end is
 * registered, so the executor's own CPU path draws every pixel into guest
 * memory, and the result is read back from the guest surface.
 *
 * What is asserted: each draw's pixel against the literal ARGB constant
 * derived by hand in texgen_stream_cases.h, at three surface positions (the
 * draw's labelled CPU literal where `cpu_differs`); and for the limitation
 * cases, that the walk refuses the words they would need. A failure prints
 * `FAIL texgen_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "texgen_stream_fixture.h"
#include "texgen_stream_cases.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* What a draw that never reached the surface leaves behind. */
#define SENTINEL 0x5A5A5A5Au

static int g_failures;
static const char *g_case = "?";

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL texgen_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void case_note(const char *fmt, ...)
{
    va_list ap;
    printf("  note: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

/* Three positions inside both triangles' union, one near each corner and one
 * at the centre: every covered pixel carries the same constant coordinates. */
static const uint32_t k_probe[3][2] = { { 2, 2 }, { 32, 32 }, { 61, 61 } };

static void check_draw(const TgDraw *d)
{
    const char *what = d->cpu_differs ? d->cpu_note : d->what;
    uint32_t want = d->cpu_differs ? d->want_cpu : d->want, got[3];
    int accepted, i;

    tgf_fill_surface(SENTINEL);
    accepted = tgf_submit_draw(d);
    chk(accepted, d->what, "the walk refused the draw's stream");
    for (i = 0; i < 3; i++)
        got[i] = tgf_pixel(k_probe[i][0], k_probe[i][1]);
    printf("  %-78s cpu 0x%08X (want 0x%08X)\n", d->what, (unsigned)got[1], (unsigned)want);
    if (d->cpu_differs)
        printf("    (%s; the hardware literal 0x%08X is the Vulkan half's)\n", d->cpu_note,
               (unsigned)d->want);
    for (i = 0; i < 3; i++)
        chk(got[i] == want, what, "%s: pixel (%u,%u) got 0x%08X, want 0x%08X", d->what,
            (unsigned)k_probe[i][0], (unsigned)k_probe[i][1], (unsigned)got[i],
            (unsigned)want);
}

static void check_rejected(const TgDraw *d, uint32_t method, uint32_t param, const char *what)
{
    int accepted;

    tgf_fill_surface(SENTINEL);
    accepted = tgf_submit_draw_with(d, method, param);
    printf("  %-78s accepted=%d\n", what, accepted);
    chk(!accepted, what, "the stream carrying method 0x%04X was accepted", (unsigned)method);
    chk(tgf_pixel(32, 32) == SENTINEL, what, "a refused stream drew 0x%08X",
        (unsigned)tgf_pixel(32, 32));
}

int main(int argc, char **argv)
{
    const TgCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = tg_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL texgen_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("texgen_stream: fixed-function texgen and texture matrices through the "
           "committed stream, production CPU executor and rasteriser\n");
    if (!tgf_init("texgen_stream")) {
        fprintf(stderr, "FAIL texgen_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    c->fn();
    if (g_failures) {
        fprintf(stderr, "FAIL texgen_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS texgen_stream[%s]\n", g_case);
    return 0;
}
