/* Independent deciding gate for the NV2A window clip: the VULKAN half.
 *
 * Tests only; no production file is touched.
 *
 * SEAM. As tests/test_texgen_stream_vk.c: this translation unit #includes the
 * toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so `vb_clear`,
 * `vb_draw`, `vb_flip`, `vb_scissor` and the file-static `g` are the REAL
 * production bodies. The batch every draw hands `vb_draw` is the one the
 * production executor built from the committed pushbuffer stream
 * (wclip_stream_fixture.c), captured at the executor's own back-end seam by a
 * capture vtable that records and then calls the production `vb_clear` / `vb_draw`
 * with the SAME pointers. No Nv2aBatch is built by hand.
 *
 * What is asserted: the walk accepted the stream, the executor handed exactly one
 * six-vertex batch to `vb_draw`, the back end stayed healthy (g.ok), and each
 * named pixel of wclip_stream_cases.h, read back from guest memory after the
 * production `vb_flip`, reads the quad or the background as the `vk` column says.
 * Where `vk` differs from `cpu` the assertion carries a LIMITATION label (one
 * scissor: no window clip for exclusive). A failure prints `FAIL wclip_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD is a
 * FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "wclip_stream_fixture.h"
#include "wclip_stream_cases.h"

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
    fprintf(stderr, "FAIL wclip_stream_vk[%s]: %s", g_case, what);
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
    uint32_t count;
} g_cap;

static void cap_clear(const Nv2aSurface *s, const Nv2aRenderState *rs, uint32_t flags,
                      uint32_t argb, uint32_t zstencil)
{
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
    cap_clear, cap_draw, cap_flip, NULL /* draw_program: not used by this fixture */,
    cap_stopped, NULL
};

/* ── Observing one case ─────────────────────────────────────────────────────── */

static void run_case(const WcCase *c)
{
    uint32_t cap0 = g_cap.draws;
    size_t i;
    int accepted;

    wcf_fill_surface(SENTINEL);
    accepted = wcf_submit_draw(&c->draw);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, "the walk accepted the stream", "the walk refused a window-clip word");
    chk(g.ok, "the Vulkan back end stayed healthy", "g.ok = 0");
    chk(g_cap.draws == cap0 + 1u, "the executor handed vb_draw exactly one batch",
        "got %u batches", (unsigned)(g_cap.draws - cap0));
    chk(g_cap.count == 6u, "the batch carries the quad's six vertices", "vertex count %u",
        (unsigned)g_cap.count);
    for (i = 0; i < c->nprobes; i++) {
        const WcProbe *p = &c->probes[i];
        const char *what = p->vk_what ? p->vk_what : p->what;
        uint32_t want = p->vk ? WCF_QUAD : WCF_BACKGROUND;
        uint32_t got = wcf_pixel(p->x, p->y);

        printf("  %-62s vk 0x%08X (want 0x%08X)\n", what, (unsigned)got, (unsigned)want);
        chk(got == want, what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)p->x, (unsigned)p->y, (unsigned)got, (unsigned)want);
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
    const WcCase *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: wclip_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = wc_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL wclip_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("wclip_stream_vk: the NV2A window clip, the executor's committed-stream "
           "batch into the production vb_draw\n");
    if (!wcf_init("wclip_stream_vk")) {
        fprintf(stderr, "FAIL wclip_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL wclip_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: wclip_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL wclip_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL wclip_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL wclip_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS wclip_stream_vk[%s]\n", g_case);
    return 0;
}
