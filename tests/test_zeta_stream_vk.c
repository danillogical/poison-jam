/* Independent deciding gate for the per-zeta depth contract (ledger L62: two passes
 * with different SET_SURFACE_ZETA_OFFSET addresses do not share depth contents, two
 * passes with the same address do): the VULKAN half.
 *
 * Tests only; no production file is touched.
 *
 * SEAM. As tests/test_x8alias_stream_vk.c: this translation unit #includes the
 * toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND defined, so `vb_clear`,
 * `vb_draw`, `vb_flip`, `vb_depth_for` and the file-static `g` are the REAL
 * production bodies. The clears and batches the production executor built from the
 * committed pushbuffer stream (zeta_stream_fixture.c) are captured at the executor's
 * own back-end seam by a capture vtable that records and then calls the production
 * `vb_clear` / `vb_draw` with the SAME pointers. No Nv2aBatch is built by hand.
 *
 * What is asserted: the walk accepted the stream; the back end stayed healthy
 * (g.ok); the executor handed exactly two clears (colour+depth on Z1, depth-only on
 * the middle zeta) and two batches of six vertices; the back end holds the expected
 * number of depth images afterwards (the `used` entries of g.depths: the back end
 * has no counter for them), each keyed by a zeta address the stream used; and each
 * named pixel of zeta_stream_cases.h, read back from guest memory after the
 * production `vb_flip`, equals its literal. A failure prints
 * `FAIL zeta_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD is a
 * FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "zeta_stream_fixture.h"
#include "zeta_stream_cases.h"

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
    fprintf(stderr, "FAIL zeta_stream_vk[%s]: %s", g_case, what);
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
    uint32_t bad_count;             /* batches whose vertex count is not six */
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

/* How many depth images the back end holds, and whether every one is keyed by a zeta
 * address of the stream. */
static unsigned depth_images(int *keys_ok)
{
    unsigned i, n = 0;

    *keys_ok = 1;
    for (i = 0; i < VB_MAX_DEPTHS; i++) {
        if (!g.depths[i].used)
            continue;
        n++;
        if (g.depths[i].va != ZF_Z1_VA && g.depths[i].va != ZF_Z2_VA)
            *keys_ok = 0;
    }
    return n;
}

static void run_case(const ZCase *c)
{
    size_t i;
    int accepted, keys_ok;
    unsigned depths;

    zf_fill(SENTINEL);
    accepted = zf_submit(&c->draw);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, "the walk accepted the stream", "the walk refused a method");
    chk(g.ok, "the Vulkan back end stayed healthy", "g.ok = 0");
    chk(g_cap.clears == 2u, "the executor handed vb_clear exactly two clears",
        "got %u clears", (unsigned)g_cap.clears);
    chk(g_cap.draws == 2u, "the executor handed vb_draw exactly two batches",
        "got %u batches", (unsigned)g_cap.draws);
    chk(g_cap.bad_count == 0u, "every batch carries a quad's six vertices",
        "%u batches had another vertex count", (unsigned)g_cap.bad_count);
    depths = depth_images(&keys_ok);
    chk(depths == c->want_depths, "the back end holds one depth image per zeta address",
        "%u depth images, want %u", depths, c->want_depths);
    chk(keys_ok, "every depth image is keyed by a zeta address of the stream");
    for (i = 0; i < c->nprobes; i++) {
        const ZProbe *p = &c->probes[i];
        uint32_t got = zf_pixel(p->x, p->y);

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
    const ZCase *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: zeta_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = z_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL zeta_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("zeta_stream_vk: depth per zeta address, the executor's committed-stream "
           "clear and batch into the production vb_clear / vb_draw\n");
    if (!zf_init("zeta_stream_vk")) {
        fprintf(stderr, "FAIL zeta_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL zeta_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: zeta_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL zeta_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL zeta_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    run_case(c);
    if (g_failures) {
        fprintf(stderr, "FAIL zeta_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS zeta_stream_vk[%s]\n", g_case);
    return 0;
}
