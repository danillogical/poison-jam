/* Independent deciding gate for NV2A fixed-function texgen and texture
 * matrices, D4 step 6: the VULKAN half.
 *
 * Authored by a worker distinct from the production author. Tests only; no
 * production file is touched.
 *
 * SEAM. This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c
 * with XBR_VK_BACKEND defined (as tests/test_bumpenv_stream_vk.c does), so
 * `vb_draw`, `vb_flip`, the file-static `g` and the ahead-of-time SPIR-V built
 * from src/video/shaders/nv2a_vk_rc.hlsl are the REAL production bodies.
 * Nothing is compiled at run time.
 *
 * INPUT. The batch every draw hands `vb_draw` is the one the production
 * executor built from the committed pushbuffer stream (texgen_stream_fixture.c:
 * real words through `nv2a_submit_pending`, RECOMP_PB_EXEC=1, the linked
 * toolkit library). It is captured at the executor's own back-end seam: after
 * `nv2a_vk_backend_install` registers the production vtable, a capture vtable
 * is registered in its place whose `draw` records what it was given and
 * passes the SAME `const Nv2aBatch *` straight to the production `vb_draw`.
 * Its `draw_program` is NULL, so the executor transforms the program draw on
 * its own CPU interpreter and builds that Nv2aBatch itself. The fixed-function
 * draws' texture coordinates are the executor's own (texgen and texture
 * matrix applied on the CPU before the batch is built). No Nv2aBatch is built
 * by hand, and the production texture decoder is linked, not stubbed.
 *
 * What is asserted, per draw: the executor handed exactly one six-vertex
 * batch with combiner inputs to `vb_draw`; every vertex of it carries the
 * observed stage's s, t and q derived by hand in texgen_stream_cases.h (the
 * coordinate, before the pixel stage's q divide); the combiner pipeline drew
 * it (`g.st.rc_draws` +1, `rc_fallback` +0); and the pixel read back from
 * guest memory after the production `vb_flip` equals the hardware literal --
 * the same literal the CPU half checks on its own wherever its rasteriser can
 * observe it. A failure prints `FAIL texgen_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD
 * is a FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "texgen_stream_fixture.h"
#include "texgen_stream_cases.h"

#include <math.h>
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
    fprintf(stderr, "FAIL texgen_stream_vk[%s]: %s", g_case, what);
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

/* ── The capture vtable at the executor's back-end seam ─────────────────────── */

#define CAP_MAX 6

static struct {
    uint32_t draws;
    int has_rc, has_extra;
    uint32_t count;
    float tex[CAP_MAX][4][4];
} g_cap;

static void cap_clear(const Nv2aSurface *s, const Nv2aRenderState *rs, uint32_t flags,
                      uint32_t argb, uint32_t zstencil)
{
    vb_clear(s, rs, flags, argb, zstencil);
}

static void cap_draw(const Nv2aSurface *s, const Nv2aBatch *b)
{
    uint32_t i;

    g_cap.draws++;
    g_cap.count = b->count;
    g_cap.has_rc = b->combiner != NULL;
    g_cap.has_extra = b->extra != NULL;
    if (b->extra)
        for (i = 0; i < b->count && i < CAP_MAX; i++)
            memcpy(g_cap.tex[i], b->extra[i].tex, sizeof g_cap.tex[i]);
    vb_draw(s, b);                              /* the executor's own batch, unchanged */
}

static void cap_flip(void) { vb_flip(); }
static int cap_stopped(void) { return vb_stopped(); }

static const Nv2aBackend g_capture = {
    cap_clear, cap_draw, cap_flip, NULL /* draw_program: the executor transforms */,
    cap_stopped, NULL
};

/* ── Observing one draw ─────────────────────────────────────────────────────── */

static const uint32_t k_probe[3][2] = { { 2, 2 }, { 32, 32 }, { 61, 61 } };

static int tc_near(float got, float want)
{
    return fabsf(got - want) <= 1e-4f * (fabsf(want) > 1.0f ? fabsf(want) : 1.0f);
}

/* Every vertex of the executor's batch must carry the observed stage's
 * hand-derived s, t and q. */
static void check_batch_coords(const TgDraw *d)
{
    uint32_t st = (d->out_reg & 0xFu) - 8u, v;

    chk(g_cap.has_rc && g_cap.has_extra, d->what,
        "the executor's batch carried no combiner/extra (rc %d, extra %d)",
        g_cap.has_rc, g_cap.has_extra);
    chk(g_cap.count == 6u, d->what, "batch vertex count %u, want 6", (unsigned)g_cap.count);
    if (!g_cap.has_extra || st > 3u)
        return;
    for (v = 0; v < g_cap.count && v < CAP_MAX; v++) {
        const float *t = g_cap.tex[v][st];
        chk(tc_near(t[0], d->want_tc[0]) && tc_near(t[1], d->want_tc[1]) && tc_near(t[3], d->want_tc[2]),
            d->what, "batch vertex %u stage %u (s, t, q) = (%.6f, %.6f, %.6f), derived "
            "(%.6f, %.6f, %.6f)", (unsigned)v, (unsigned)st, (double)t[0], (double)t[1],
            (double)t[3], (double)d->want_tc[0], (double)d->want_tc[1], (double)d->want_tc[2]);
    }
}

static void check_draw(const TgDraw *d)
{
    uint64_t rc0 = g.st.rc_draws, fb0 = g.st.rc_fallback;
    uint32_t cap0 = g_cap.draws, got[3];
    int accepted, i;

    memset(g_cap.tex, 0, sizeof g_cap.tex);
    tgf_fill_surface(SENTINEL);
    accepted = tgf_submit_draw(d);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, d->what, "the walk refused the draw's stream");
    chk(g.ok, d->what, "the Vulkan back end failed (g.ok = 0)");
    chk(g_cap.draws == cap0 + 1u, d->what,
        "the executor handed vb_draw %u batches, want exactly 1",
        (unsigned)(g_cap.draws - cap0));
    check_batch_coords(d);
    for (i = 0; i < 3; i++)
        got[i] = tgf_pixel(k_probe[i][0], k_probe[i][1]);
    printf("  %-78s vk 0x%08X (want 0x%08X)\n", d->what, (unsigned)got[1], (unsigned)d->want);
    chk(g.st.rc_draws == rc0 + 1u && g.st.rc_fallback == fb0, d->what,
        "the combiner pipeline did not draw it (rc_draws +%llu, rc_fallback +%llu)",
        (unsigned long long)(g.st.rc_draws - rc0),
        (unsigned long long)(g.st.rc_fallback - fb0));
    for (i = 0; i < 3; i++)
        chk(got[i] == d->want, d->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
            (unsigned)k_probe[i][0], (unsigned)k_probe[i][1], (unsigned)got[i],
            (unsigned)d->want);
}

static void check_rejected(const TgDraw *d, uint32_t method, uint32_t param, const char *what)
{
    uint32_t cap0 = g_cap.draws;
    int accepted;

    tgf_fill_surface(SENTINEL);
    accepted = tgf_submit_draw_with(d, method, param);
    vb_flip();
    printf("  %-78s accepted=%d\n", what, accepted);
    chk(!accepted, what, "the stream carrying method 0x%04X was accepted", (unsigned)method);
    chk(g_cap.draws == cap0, what, "a refused stream reached vb_draw");
    chk(tgf_pixel(32, 32) == SENTINEL, what, "a refused stream drew 0x%08X",
        (unsigned)tgf_pixel(32, 32));
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
    const TgCase *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: texgen_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = tg_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL texgen_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("texgen_stream_vk: fixed-function texgen and texture matrices, the executor's "
           "committed-stream batch into the production vb_draw (AOT SPIR-V)\n");
    if (!tgf_init("texgen_stream_vk")) {
        fprintf(stderr, "FAIL texgen_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL texgen_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: texgen_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL texgen_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL texgen_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    c->fn();
    if (g_failures) {
        fprintf(stderr, "FAIL texgen_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS texgen_stream_vk[%s]\n", g_case);
    return 0;
}
