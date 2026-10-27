/* Independent deciding gate for the X8R8G8B8 texture alpha rule (a LIN_X8R8G8B8
 * texture reads alpha 0xFF; LIN_A8R8G8B8 reads the stored alpha): the
 * COMMITTED-STREAM CPU half.
 *
 * Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (x8alias_stream_fixture.c). No decoder is stubbed and no back
 * end is registered, so the executor's own CPU path clears and draws every pixel
 * into guest memory, and the result is read back from the guest surfaces.
 *
 * What is asserted: the walk accepted the stream, then each named pixel of
 * x8alias_stream_cases.h reads its hand-derived ARGB literal. This half is the
 * agreement baseline: the CPU executor already forces alpha 0xFF for 0x1E. A
 * failure prints `FAIL x8alias_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "x8alias_stream_fixture.h"
#include "x8alias_stream_cases.h"

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
    fprintf(stderr, "FAIL x8alias_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void run_case(const X8Case *c)
{
    size_t i;
    int accepted;

    x8f_fill(SENTINEL);
    accepted = x8f_submit(&c->draw);
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    for (i = 0; i < c->nprobes; i++) {
        const X8Probe *p = &c->probes[i];
        uint32_t got = x8f_pixel(p->surf, p->x, p->y);

        printf("  %-52s cpu 0x%08X (want 0x%08X)\n", p->what, (unsigned)got, (unsigned)p->want);
        chk(got == p->want, p->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)p->x, (unsigned)p->y, (unsigned)got, (unsigned)p->want);
    }
}

int main(int argc, char **argv)
{
    const X8Case *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = x8_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL x8alias_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("x8alias_stream: LIN_X8R8G8B8 alpha through the committed stream, "
           "production CPU executor and rasteriser\n");
    if (!x8f_init("x8alias_stream")) {
        fprintf(stderr, "FAIL x8alias_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL x8alias_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS x8alias_stream[%s]\n", g_case);
    return 0;
}
