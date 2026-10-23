/* Independent deciding gate for NV2A texture modes 6 (BUMPENVMAP) and 7
 * (BUMPENVMAP_LUM), D4 step 5: the COMMITTED-STREAM CPU half.
 *
 * Authored by a worker distinct from the production author and from the
 * focused step-4 gate's author. Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (bumpenv_stream_fixture.c), so the walk's admission table,
 * the executor's method decoding (texture stages, SET_TEXTURE_SET_BUMP_ENV_*,
 * SET_SHADER_OTHER_STAGE_INPUT, the combiners), the CPU vertex-program
 * interpreter, the production CPU rasteriser and its texture sampler -- the
 * real `sample_tex` that `nv2a_backend_decode_texture` also uses -- all come
 * from the linked toolkit library. No decoder is stubbed and no back end is
 * registered, so the executor's own CPU path draws every pixel into guest
 * memory, and the result is read back from the guest surface.
 *
 * What is asserted: each draw's pixel against the literal ARGB constant
 * derived by hand in bumpenv_stream_cases.h, at three surface positions; and
 * for the limitation cases, that the walk refuses the words a bump stage
 * would need. A failure prints `FAIL bumpenv_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "bumpenv_stream_fixture.h"
#include "bumpenv_stream_cases.h"

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
    fprintf(stderr, "FAIL bumpenv_stream[%s]: %s", g_case, what);
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

static void check_draw(const BsfDraw *d)
{
    uint32_t got[3];
    int accepted, i;

    bsf_fill_surface(SENTINEL);
    accepted = bsf_submit_draw(d);
    chk(accepted, d->what, "the walk refused the draw's stream");
    for (i = 0; i < 3; i++)
        got[i] = bsf_pixel(k_probe[i][0], k_probe[i][1]);
    printf("  %-78s cpu 0x%08X (want 0x%08X)\n", d->what, (unsigned)got[1],
           (unsigned)d->want);
    for (i = 0; i < 3; i++)
        chk(got[i] == d->want, d->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)k_probe[i][0], (unsigned)k_probe[i][1], (unsigned)got[i],
            (unsigned)d->want);
}

static void check_rejected(const BsfDraw *d, uint32_t method, uint32_t param,
                           const char *what)
{
    int accepted;

    bsf_fill_surface(SENTINEL);
    accepted = bsf_submit_draw_with(d, method, param);
    printf("  %-78s accepted=%d\n", what, accepted);
    chk(!accepted, what, "the stream carrying method 0x%04X was accepted", (unsigned)method);
    chk(bsf_pixel(32, 32) == SENTINEL, what, "a refused stream drew 0x%08X",
        (unsigned)bsf_pixel(32, 32));
}

int main(int argc, char **argv)
{
    const BsfCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = bsf_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL bumpenv_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("bumpenv_stream: modes 6/7 through the committed stream, production CPU "
           "executor and rasteriser\n");
    if (!bsf_init("bumpenv_stream")) {
        fprintf(stderr, "FAIL bumpenv_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    c->fn();
    if (g_failures) {
        fprintf(stderr, "FAIL bumpenv_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS bumpenv_stream[%s]\n", g_case);
    return 0;
}
