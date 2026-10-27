/* Independent deciding gate for fixed-function vertex lighting (see
 * tests/light_stream_cases.h for the rule): the VULKAN half.
 *
 * Tests only; no production file is touched.
 *
 * SEAM. As tests/test_light_stream_vk.c: this translation unit #includes the toolkit's
 * src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so `vb_draw`, `vb_flip` and
 * the file-static `g` are the REAL production bodies. The batch the production executor
 * built from the committed pushbuffer stream (light_stream_fixture.c), vertex colours
 * included, is captured at the executor's own back-end seam by a capture vtable that
 * records and then calls the production `vb_draw` with the SAME pointer. No Nv2aBatch
 * is built by hand.
 *
 * What is asserted: the walk accepted the stream; the back end stayed healthy (g.ok);
 * the executor handed exactly one batch of six vertices; and each probe, read back from
 * guest memory after the production `vb_flip`, equals the case's literal. A failure
 * prints `FAIL light_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD is a
 * FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "light_stream_fixture.h"
#include "light_stream_cases.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SENTINEL 0x5A5A5A5Au

static int g_failures;
static const char *g_case = "?";

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL light_stream_vk[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

/* ── The capture vtable at the executor's back-end seam ─────────────────────── */

static struct {
    uint32_t draws;
    uint32_t bad_count;             /* batches whose vertex count is not six */
} g_cap;

static void cap_clear(const Nv2aSurface *s, const Nv2aRenderState *rs, uint32_t flags,
                      uint32_t argb, uint32_t zstencil)
{
    vb_clear(s, rs, flags, argb, zstencil);
}

static void cap_draw(const Nv2aSurface *s, const Nv2aBatch *b)
{
    g_cap.draws++;
    if (b->count != 6u)
        g_cap.bad_count++;
    vb_draw(s, b);                              /* the executor's own batch, unchanged */
}

static void cap_flip(void) { vb_flip(); }
static int cap_stopped(void) { return vb_stopped(); }

static const Nv2aBackend g_capture = {
    cap_clear, cap_draw, cap_flip, NULL /* draw_program: the executor transforms */,
    cap_stopped, NULL
};

/* ── Observing one case ─────────────────────────────────────────────────────── */

static void run_case(const LCase *c)
{
    size_t i;
    int accepted;

    lf_fill(SENTINEL);
    accepted = lf_submit(&c->draw);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    chk(g.ok, "the Vulkan back end stayed healthy", "g.ok = 0");
    chk(g_cap.draws == 1u, "the executor handed vb_draw exactly one batch",
        "got %u batches", (unsigned)g_cap.draws);
    chk(g_cap.bad_count == 0u, "the batch carries the quad's six vertices",
        "%u batches had another vertex count", (unsigned)g_cap.bad_count);
    for (i = 0; i < N_OF(k_probes); i++) {
        uint32_t x = k_probes[i].x, y = k_probes[i].y;
        uint32_t got = lf_pixel(x, y);

        printf("  probe (%2u,%2u) vk 0x%08X (want 0x%08X)\n", (unsigned)x, (unsigned)y,
               (unsigned)got, (unsigned)c->want);
        chk(got == c->want, "lit pixel equals the hand-derived literal",
            "pixel (%u,%u) got 0x%08X, want 0x%08X", (unsigned)x, (unsigned)y,
            (unsigned)got, (unsigned)c->want);
    }
}

static void report_device(void)
{
    VkPhysicalDeviceProperties p;
    if (g.pd && vkGetPhysicalDeviceProperties) {
        memset(&p, 0, sizeof p);
        vkGetPhysicalDeviceProperties(g.pd, &p);
        printf("  device: %s (type %d)\n", p.deviceName, (int)p.deviceType);
    }
}

int main(int argc, char **argv)
{
    const char *icd_files, *icd_names;
    const LCase *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: light_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = l_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL light_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("light_stream_vk: fixed-function vertex lighting, the executor's committed-stream "
           "batch into the production vb_draw\n");
    if (!lf_init("light_stream_vk")) {
        fprintf(stderr, "FAIL light_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL light_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: light_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL light_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL light_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL light_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS light_stream_vk[%s]\n", g_case);
    return 0;
}
