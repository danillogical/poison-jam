/* Independent deciding gate for the per-zeta depth contract (ledger L62: two passes
 * with different SET_SURFACE_ZETA_OFFSET addresses do not share depth contents, two
 * passes with the same address do): the COMMITTED-STREAM CPU half.
 *
 * Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (zeta_stream_fixture.c). No decoder is stubbed and no back
 * end is registered, so the executor's own CPU path clears, depth-tests and draws
 * every pixel into guest memory, and the result is read back from the colour target.
 *
 * What is asserted: the walk accepted the stream, then each named pixel of
 * zeta_stream_cases.h reads its hand-derived ARGB literal. The CPU executor keeps
 * up to four z buffers keyed by zeta offset, but only its vertex-program raster path
 * tests against them: fixed-function `distinct` carries a labelled LIMITATION literal
 * (zeta_stream_cases.h), the other three cases the hardware rule. A failure prints `FAIL zeta_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "zeta_stream_fixture.h"
#include "zeta_stream_cases.h"

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
    fprintf(stderr, "FAIL zeta_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void run_case(const ZCase *c)
{
    const ZProbe *probes = c->cpu_probes ? c->cpu_probes : c->probes;
    size_t n = c->cpu_probes ? c->ncpu_probes : c->nprobes;
    size_t i;
    int accepted;

    zf_fill(SENTINEL);
    accepted = zf_submit(&c->draw);
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    for (i = 0; i < n; i++) {
        const ZProbe *p = &probes[i];
        uint32_t got = zf_pixel(p->x, p->y);

        printf("  %-52s cpu 0x%08X (want 0x%08X)\n", p->what, (unsigned)got, (unsigned)p->want);
        chk(got == p->want, p->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)p->x, (unsigned)p->y, (unsigned)got, (unsigned)p->want);
    }
}

int main(int argc, char **argv)
{
    const ZCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = z_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL zeta_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("zeta_stream: depth per zeta address through the committed stream, "
           "production CPU executor and rasteriser\n");
    if (!zf_init("zeta_stream")) {
        fprintf(stderr, "FAIL zeta_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL zeta_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS zeta_stream[%s]\n", g_case);
    return 0;
}
