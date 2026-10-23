/* Independent deciding gate for NV2A texture modes 6 (BUMPENVMAP) and 7
 * (BUMPENVMAP_LUM), D4 step 5: the VULKAN half.
 *
 * Authored by a worker distinct from the production author and from the
 * focused step-4 gate's author. Tests only; no production file is touched.
 *
 * SEAM. This translation unit #includes the toolkit's src/video/nv2a_vk_backend.c
 * with XBR_VK_BACKEND defined (as tests/test_bumpenv_q_vk.c and
 * tests/test_pbcov_vk.c do), so `vb_draw`, `vb_flip`, `vb_rc_prepare`, the
 * refusal rule, the file-static `g` and the ahead-of-time SPIR-V built from
 * src/video/shaders/nv2a_vk_rc.hlsl are the REAL production bodies. Nothing
 * is compiled at run time.
 *
 * INPUT. The batch every draw hands `vb_draw` is the one the production
 * executor built from the committed pushbuffer stream (bumpenv_stream_fixture.c:
 * real words through `nv2a_submit_pending`, RECOMP_PB_EXEC=1, the linked
 * toolkit library). It is captured at the executor's own back-end seam: after
 * `nv2a_vk_backend_install` registers the production vtable, a capture
 * vtable is registered in its place whose `draw` records what it was given
 * and passes the SAME `const Nv2aBatch *` straight to the production
 * `vb_draw`; its clear/flip/stopped are the production ones. Its
 * `draw_program` is NULL, so the executor transforms the program batch on
 * its own CPU interpreter and builds the Nv2aBatch itself
 * (nv2a_pb_exec.c backend_program_batch), exactly as it does for any program
 * the ahead-of-time corpus does not carry. No Nv2aBatch is built by hand.
 *
 * DECODER. The production `nv2a_backend_decode_texture` (linked from the
 * toolkit library) decodes every texture; there is no stand-in.
 *
 * What is asserted, per draw: the executor handed exactly one batch to
 * `vb_draw`; that batch carries the stream's stage program, other-stage
 * input and CANONICAL bump matrix / scale / offset for stages 1..3; the
 * combiner pipeline drew it (`g.st.rc_draws`) or refused it
 * (`g.st.rc_fallback` and the production refusal ledger's reason, stage and
 * mode); and, for every drawn batch, the pixel read back from guest memory
 * after the production `vb_flip` equals the literal derived by hand in
 * bumpenv_stream_cases.h -- the same literal the CPU half checks on its own.
 * A failure prints `FAIL bumpenv_stream_vk[<case>]: <what>`.
 *
 * One case per process. A failed install when the environment names an ICD
 * is a FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include "bumpenv_stream_fixture.h"
#include "bumpenv_stream_cases.h"

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
    fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: %s", g_case, what);
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

static struct {
    uint32_t draws;
    int has_rc, has_extra;
    Nv2aCombiner rc;
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
    g_cap.has_rc = b->combiner != NULL;
    g_cap.has_extra = b->extra != NULL;
    if (b->combiner)
        g_cap.rc = *b->combiner;
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

static int uses_g8b8(const BsfDraw *d)
{
    int st;
    for (st = 0; st < 3; st++)
        if (d->tex[st] == BSF_TEX_G8B8)
            return 1;
    return 0;
}

/* The batch the executor built must carry the stream's state, canonically. */
static void check_batch_state(const BsfDraw *d)
{
    int st, i;

    chk(g_cap.has_rc && g_cap.has_extra, d->what,
        "the executor's batch carried no combiner/extra (rc %d, extra %d)",
        g_cap.has_rc, g_cap.has_extra);
    if (!g_cap.has_rc)
        return;
    chk(g_cap.count == 6u, d->what, "batch vertex count %u, want 6", (unsigned)g_cap.count);
    chk(g_cap.rc.stage_program == d->prog, d->what,
        "batch stage_program 0x%08X, stream 0x%08X",
        (unsigned)g_cap.rc.stage_program, (unsigned)d->prog);
    chk(g_cap.rc.other_stage_input == d->osi, d->what,
        "batch other_stage_input 0x%08X, stream 0x%08X",
        (unsigned)g_cap.rc.other_stage_input, (unsigned)d->osi);
    for (st = 1; st < 4; st++) {
        for (i = 0; i < 4; i++)
            chk(g_cap.rc.bump_mat[st][i] == d->mat[st][i], d->what,
                "batch bump_mat[%d][%d] = %g, canonical stream value %g", st, i,
                (double)g_cap.rc.bump_mat[st][i], (double)d->mat[st][i]);
        chk(g_cap.rc.bump_scale[st] == d->scale[st], d->what,
            "batch bump_scale[%d] = %g, stream %g", st,
            (double)g_cap.rc.bump_scale[st], (double)d->scale[st]);
        chk(g_cap.rc.bump_offset[st] == d->offset[st], d->what,
            "batch bump_offset[%d] = %g, stream %g", st,
            (double)g_cap.rc.bump_offset[st], (double)d->offset[st]);
    }
}

static void check_draw(const BsfDraw *d)
{
    const Nv2aVkRcCoverage *cov = nv2a_vk_rc_coverage();
    uint64_t rc0 = g.st.rc_draws, fb0 = g.st.rc_fallback;
    uint64_t gt0 = cov->mode_gt5, cube0 = cov->cube_mode3;
    uint32_t cap0 = g_cap.draws, got[3];
    int accepted, i;

    bsf_fill_surface(SENTINEL);
    accepted = bsf_submit_draw(d);
    vb_flip();                                  /* the production readback into guest memory */
    chk(accepted, d->what, "the walk refused the draw's stream");
    chk(g.ok, d->what, "the Vulkan back end failed (g.ok = 0)");
    chk(g_cap.draws == cap0 + 1u, d->what,
        "the executor handed vb_draw %u batches, want exactly 1",
        (unsigned)(g_cap.draws - cap0));
    check_batch_state(d);
    for (i = 0; i < 3; i++)
        got[i] = bsf_pixel(k_probe[i][0], k_probe[i][1]);

    if (d->vk == BSF_VK_RC) {
        printf("  %-78s vk 0x%08X (want 0x%08X)\n", d->what, (unsigned)got[1],
               (unsigned)d->want);
        chk(g.st.rc_draws == rc0 + 1u && g.st.rc_fallback == fb0, d->what,
            "the combiner pipeline did not draw it (rc_draws +%llu, rc_fallback +%llu)",
            (unsigned long long)(g.st.rc_draws - rc0),
            (unsigned long long)(g.st.rc_fallback - fb0));
        for (i = 0; i < 3; i++)
            chk(got[i] == d->want, d->what, "pixel (%u,%u) got 0x%08X, want 0x%08X",
                (unsigned)k_probe[i][0], (unsigned)k_probe[i][1], (unsigned)got[i],
                (unsigned)d->want);
    } else {
        const char *what = d->vk == BSF_VK_FALLBACK_MODE
            ? "vk refuses a stage mode 8..31 (rc_fallback, reason 1)"
            : "vk refuses CUBEMAP on a true cube texture (rc_fallback, reason 2)";
        printf("  %-78s vk rc_fallback +%llu, rc_draws +%llu\n", d->what,
               (unsigned long long)(g.st.rc_fallback - fb0),
               (unsigned long long)(g.st.rc_draws - rc0));
        chk(g.st.rc_fallback == fb0 + 1u && g.st.rc_draws == rc0, what,
            "%s: rc_fallback +%llu, rc_draws +%llu", d->what,
            (unsigned long long)(g.st.rc_fallback - fb0),
            (unsigned long long)(g.st.rc_draws - rc0));
        chk(nv2a_vk_rc_coverage_enabled(), what,
            "RECOMP_PB_COVERAGE=1 is required to read the refusal reason");
        if (d->vk == BSF_VK_FALLBACK_MODE)
            chk(cov->mode_gt5 == gt0 + 1u && cov->cube_mode3 == cube0, what,
                "%s: mode counter +%llu, cube counter +%llu", d->what,
                (unsigned long long)(cov->mode_gt5 - gt0),
                (unsigned long long)(cov->cube_mode3 - cube0));
        else
            chk(cov->cube_mode3 == cube0 + 1u && cov->mode_gt5 == gt0, what,
                "%s: cube counter +%llu, mode counter +%llu", d->what,
                (unsigned long long)(cov->cube_mode3 - cube0),
                (unsigned long long)(cov->mode_gt5 - gt0));
        /* The first refusal of the process is frozen in the ledger: each case's
         * first fallback draw is the process's first, so it is the record. */
        if (fb0 == 0) {
            chk(cov->first_valid == 1u, what, "no first refusal record");
            chk(cov->first_reason == (d->vk == BSF_VK_FALLBACK_MODE ? 1u : 2u), what,
                "first_reason %u", (unsigned)cov->first_reason);
            chk(cov->first_stage == d->vk_stage && cov->first_mode == d->vk_mode, what,
                "first refusal stage %u mode %u, want stage %u mode %u",
                (unsigned)cov->first_stage, (unsigned)cov->first_mode,
                (unsigned)d->vk_stage, (unsigned)d->vk_mode);
        }
        chk(got[1] != SENTINEL, what, "%s: the refused batch was not drawn at all", d->what);
    }
    if (uses_g8b8(d))
        chk(g.st.tex_undecodable > 0u, d->what,
            "the production decoder was expected to reject colour 0x28 (tex_undecodable 0)");
}

static void check_rejected(const BsfDraw *d, uint32_t method, uint32_t param,
                           const char *what)
{
    uint32_t cap0 = g_cap.draws;
    int accepted;

    bsf_fill_surface(SENTINEL);
    accepted = bsf_submit_draw_with(d, method, param);
    vb_flip();
    printf("  %-78s accepted=%d\n", what, accepted);
    chk(!accepted, what, "the stream carrying method 0x%04X was accepted", (unsigned)method);
    chk(g_cap.draws == cap0, what, "a refused stream reached vb_draw");
    chk(bsf_pixel(32, 32) == SENTINEL, what, "a refused stream drew 0x%08X",
        (unsigned)bsf_pixel(32, 32));
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
    const BsfCase *c;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: bumpenv_stream_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    c = bsf_find_case(g_case);
    if (!c) {
        fprintf(stderr, "FAIL bumpenv_stream_vk: unknown case '%s'\n", g_case);
        return 2;
    }
    printf("bumpenv_stream_vk: modes 6/7, the executor's committed-stream batch into the "
           "production vb_draw (AOT SPIR-V)\n");
    if (!bsf_init("bumpenv_stream_vk")) {
        fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: fixture/harness error\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    if (nv2a_vk_backend_install() != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: bumpenv_stream_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();
    if (nv2a_backend_current() != &s_vk_backend) {
        fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: install did not register the "
                "production vtable\n", g_case);
        return 1;
    }
    nv2a_backend_register(&g_capture);
    if (nv2a_backend_current() != &g_capture) {
        fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: the capture vtable is not the one "
                "the executor will call\n", g_case);
        return 1;
    }
    c->fn();
    if (g_failures) {
        fprintf(stderr, "FAIL bumpenv_stream_vk[%s]: %d assertion%s failed\n", g_case,
                g_failures, g_failures == 1 ? "" : "s");
        return 1;
    }
    printf("PASS bumpenv_stream_vk[%s]\n", g_case);
    return 0;
}
