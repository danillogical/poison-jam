/* Independent deciding gate for NV2A fixed-function SPECULAR lighting (see
 * tests/spec_stream_cases.h for the pinned-source rule and every literal):
 * the COMMITTED-STREAM CPU half.
 *
 * Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (spec_stream_fixture.c). No decoder is stubbed and no back end
 * is registered, so the executor's own CPU path lights and rasterises the quad
 * into guest memory, and the result is read back from the colour target.
 *
 * What is asserted: the walk accepted the stream, then each probe reads the
 * case's labelled CPU literal. LIMITATION: the CPU fixed-function rasteriser does
 * not run the register combiners (`raster_triangle` writes the lit vertex colour
 * directly; only the vertex-PROGRAM path reaches `nv2a_rc_eval`), so this half
 * cannot observe oD1 and is NOT the deciding assertion. It is a plumbing and
 * positive-control check; tests/test_spec_stream_vk.c decides. See
 * spec_stream_cases.h, "CPU LIMITATION".
 *
 * A failure prints `FAIL spec_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "spec_stream_fixture.h"
#include "spec_stream_cases.h"

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
    fprintf(stderr, "FAIL spec_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void run_case(const SCase *c)
{
    size_t i;
    int accepted;

    sf_fill(SENTINEL);
    accepted = sf_submit(&c->draw);
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    if (c->cpu_differs)
        printf("  LIMITATION cpu: this half cannot observe oD1; it checks the lit "
               "vertex colour the CPU rasteriser writes\n");
    for (i = 0; i < N_OF(k_probes); i++) {
        uint32_t x = k_probes[i].x, y = k_probes[i].y;
        uint32_t got = sf_pixel(x, y);

        printf("  probe (%2u,%2u) cpu 0x%08X (want 0x%08X)%s\n", (unsigned)x, (unsigned)y,
               (unsigned)got, (unsigned)c->want_cpu,
               c->cpu_differs ? "  [CPU LIMITATION literal]" : "");
        chk(got == c->want_cpu, "pixel equals the labelled CPU literal",
            "pixel (%u,%u) got 0x%08X, want 0x%08X", (unsigned)x, (unsigned)y,
            (unsigned)got, (unsigned)c->want_cpu);
    }
}

int main(int argc, char **argv)
{
    const SCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = s_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL spec_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("spec_stream: fixed-function specular through the committed stream, "
           "production CPU executor and rasteriser\n");
    if (!sf_init("spec_stream")) {
        fprintf(stderr, "FAIL spec_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL spec_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS spec_stream[%s]\n", g_case);
    return 0;
}
