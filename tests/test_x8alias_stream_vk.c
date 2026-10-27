/* Independent deciding gate for the X8R8G8B8 texture alpha rule (a LIN_X8R8G8B8
 * texture reads alpha 0xFF; LIN_A8R8G8B8 reads the stored alpha): the VULKAN half.
 *
 * Tests only; no production file is touched.
 *
 * SEAM. As tests/test_wclip_stream_vk.c: this translation unit #includes the
 * toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so `vb_clear`,
 * `vb_draw`, `vb_flip`, `vb_texture`, `vb_tex_aliases` and the file-static `g` are
 * the REAL production bodies. The clear and the batch the production executor built
 * from the committed pushbuffer stream (x8alias_stream_fixture.c) are captured at
 * the executor's own back-end seam by a capture vtable that records and then calls
 * the production `vb_clear` / `vb_draw` with the SAME pointers. No Nv2aBatch is
 * built by hand.
 *
 * What is asserted: the walk accepted the stream; the back end stayed healthy
 * (g.ok); the executor handed exactly one clear and one batch; the draw really took
 * the render-target alias path (g.st.rt_aliases +1, rt_copies +1 only for the self
 * case) and the intended shader (rc_draws +1 for the combiner cases, +0 otherwise),
 * so a pass cannot be vacuous; and each named pixel of x8alias_stream_cases.h, read
 * back from guest memory after the production `vb_flip`, equals its literal. A
 * failure prints `FAIL x8alias_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD is a
 * FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "x8alias_stream_fixture.h"
#include "x8alias_stream_cases.h"

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
    fprintf(stderr, "FAIL x8alias_stream_vk[%s]: %s", g_case, what);
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
    uint32_t clears, draws;
    uint32_t count;
} g_cap;

static void cap_clear(const Nv2aSurface *s, const Nv2aRenderState *rs, uint32_t flags,
                      uint32_t argb, uint32_t zstencil)
{
    g_cap.clears++;
    vb_clear(s, rs, flags, argb, zstencil);
}

static void cap_draw(const Nv2aSurface *s, const Nv2aBatch *b)
{
    g_cap.draws++;
    g_cap.count = b->count;
    vb_draw(s, b);                              /* the executor's own batch, unchanged */
}

static void cap_flip(void) { vb_flip(); }
static int cap_stopped(void) { return vb_stopped(); }

static const Nv2aBackend g_capture = {
    cap_clear, cap_draw, cap_flip, NULL /* draw_program: the executor transforms */,
    cap_stopped, NULL
};

/* ── Observing one case ─────────────────────────────────────────────────────── */

static void run_case(const X8Case *c)
{
    uint64_t al0 = g.st.rt_aliases, cp0 = g.st.rt_copies;
    uint64_t rc0 = g.st.rc_draws, fb0 = g.st.rc_fallback;
    size_t i;
    int accepted;

    x8f_fill(SENTINEL);
    accepted = x8f_submit(&c->draw);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    chk(g.ok, "the Vulkan back end stayed healthy", "g.ok = 0");
    chk(g_cap.clears == 1u, "the executor handed vb_clear exactly one clear",
        "got %u clears", (unsigned)g_cap.clears);
    chk(g_cap.draws == 1u, "the executor handed vb_draw exactly one batch",
        "got %u batches", (unsigned)g_cap.draws);
    chk(g_cap.count == 6u, "the batch carries the quad's six vertices", "vertex count %u",
        (unsigned)g_cap.count);
    chk(g.st.rt_aliases == al0 + 1u, "stage 0 bound surface A's live render target",
        "rt_aliases +%llu, want +1", (unsigned long long)(g.st.rt_aliases - al0));
    chk(g.st.rt_copies == cp0 + (c->draw.self ? 1u : 0u),
        "the snapshot copy is taken exactly when the draw samples its own target",
        "rt_copies +%llu, want +%u", (unsigned long long)(g.st.rt_copies - cp0),
        c->draw.self ? 1u : 0u);
    chk(g.st.rc_draws == rc0 + (c->draw.rc ? 1u : 0u) && g.st.rc_fallback == fb0,
        "the intended shader drew it",
        "rc_draws +%llu (want +%u), rc_fallback +%llu (want +0)",
        (unsigned long long)(g.st.rc_draws - rc0), c->draw.rc ? 1u : 0u,
        (unsigned long long)(g.st.rc_fallback - fb0));
    for (i = 0; i < c->nprobes; i++) {
        const X8Probe *p = &c->probes[i];
        uint32_t got = x8f_pixel(p->surf, p->x, p->y);

        printf("  %-52s vk 0x%08X (want 0x%08X)\n", p->what, (unsigned)got, (unsigned)p->want);
        chk(got == p->want, p->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)p->x, (unsigned)p->y, (unsigned)got, (unsigned)p->want);
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
    const X8Case *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: x8alias_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = x8_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL x8alias_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("x8alias_stream_vk: LIN_X8R8G8B8 alpha, the executor's committed-stream "
           "clear and batch into the production vb_clear / vb_draw\n");
    if (!x8f_init("x8alias_stream_vk")) {
        fprintf(stderr, "FAIL x8alias_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL x8alias_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: x8alias_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL x8alias_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL x8alias_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL x8alias_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS x8alias_stream_vk[%s]\n", g_case);
    return 0;
}
