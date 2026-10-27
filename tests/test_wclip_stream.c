/* Independent deciding gate for the NV2A window clip (SET_WINDOW_CLIP_TYPE /
 * _HORIZONTAL / _VERTICAL): the COMMITTED-STREAM CPU half.
 *
 * Tests only; no production file is touched.
 *
 * What runs: real pushbuffer words through `nv2a_submit_pending` with
 * RECOMP_PB_EXEC=1 (wclip_stream_fixture.c), so the walk's admission table and
 * the executor's method decoding and CPU fixed-function rasteriser all come
 * from the linked toolkit library. No decoder is stubbed and no back end is
 * registered, so the executor's own CPU path clears and draws every pixel into
 * guest memory, and the result is read back from the guest surface.
 *
 * What is asserted: the walk accepted the stream, then each named pixel of
 * wclip_stream_cases.h reads the quad colour or the background colour as the
 * `cpu` column says. A failure prints `FAIL wclip_stream[<case>]: <what>`.
 *
 * One case per process (`--case <name>`): the executor and the NV2A core are
 * process-wide singletons.
 */
#include "wclip_stream_fixture.h"
#include "wclip_stream_cases.h"

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
    fprintf(stderr, "FAIL wclip_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void run_case(const WcCase *c)
{
    size_t i;
    int accepted;

    wcf_fill_surface(SENTINEL);
    accepted = wcf_submit_draw(&c->draw);
    chk(accepted, "the walk accepted the stream", "the walk refused a window-clip word");
    for (i = 0; i < c->nprobes; i++) {
        const WcProbe *p = &c->probes[i];
        uint32_t want = p->cpu ? WCF_QUAD : WCF_BACKGROUND;
        uint32_t got = wcf_pixel(p->x, p->y);

        printf("  %-62s cpu 0x%08X (want 0x%08X)\n", p->what, (unsigned)got, (unsigned)want);
        chk(got == want, p->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)p->x, (unsigned)p->y, (unsigned)got, (unsigned)want);
    }
}

int main(int argc, char **argv)
{
    const WcCase *c;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = wc_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL wclip_stream: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("wclip_stream: the NV2A window clip through the committed stream, "
           "production CPU executor and rasteriser\n");
    if (!wcf_init("wclip_stream")) {
        fprintf(stderr, "FAIL wclip_stream[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL wclip_stream[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS wclip_stream[%s]\n", g_case);
    return 0;
}
