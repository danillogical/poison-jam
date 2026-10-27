/* Independent deciding gate for fixed-function vertex lighting (SET_LIGHTING_ENABLE
 * 0x0314: eye normal from the inverse model-view rows, normalised only when
 * SET_NORMALIZATION_ENABLE, infinite-light direction normalised, ambient + diffuse *
 * max(0, n.L), clamped, truncating pack): the COMMITTED-STREAM CPU half.
 *
 * Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (light_stream_fixture.c). No decoder is stubbed and no back end is
 * registered, so the executor's own CPU path lights and rasterises the quad into guest
 * memory, and the result is read back from the colour target.
 *
 * What is asserted: the walk accepted the stream, then each probe of
 * light_stream_cases.h reads the case's hand-derived ARGB literal. A failure prints
 * `FAIL light_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "light_stream_fixture.h"
#include "light_stream_cases.h"

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
    fprintf(stderr, "FAIL light_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void run_case(const LCase *c)
{
    size_t i;
    int accepted;

    lf_fill(SENTINEL);
    accepted = lf_submit(&c->draw);
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    for (i = 0; i < N_OF(k_probes); i++) {
        uint32_t x = k_probes[i].x, y = k_probes[i].y;
        uint32_t got = lf_pixel(x, y);

        printf("  probe (%2u,%2u) cpu 0x%08X (want 0x%08X)\n", (unsigned)x, (unsigned)y,
               (unsigned)got, (unsigned)c->want);
        chk(got == c->want, "lit pixel equals the hand-derived literal",
            "pixel (%u,%u) got 0x%08X, want 0x%08X", (unsigned)x, (unsigned)y,
            (unsigned)got, (unsigned)c->want);
    }
}

int main(int argc, char **argv)
{
    const LCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = l_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL light_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("light_stream: fixed-function vertex lighting through the committed stream, "
           "production CPU executor and rasteriser\n");
    if (!lf_init("light_stream")) {
        fprintf(stderr, "FAIL light_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL light_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS light_stream[%s]\n", g_case);
    return 0;
}
