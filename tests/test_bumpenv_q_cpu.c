/* Focused deciding gate: BUMPENVMAP / BUMPENVMAP_LUM must not apply
 * PROJECT2D's texture-q division. CPU half.
 *
 * SEAM: this is a DIRECT HELPER SEAM. The translation unit #includes the
 * toolkit's src/kernel/nv2a_pb_exec.c, so `rc_stage_fetch`, `s_gpu`, `Texture`,
 * `rc_texel` and `rc_sign3` below are the REAL production bodies (the same way
 * tests/test_pbcov_stream.c includes the real executor headers and links the
 * real library). It does NOT claim committed-stream coverage, and it does not
 * claim pixel coverage: no rasteriser runs. It asserts the fetch rule itself,
 * which is the thing the repair changed. The fixture is host-portable: it
 * allocates its arena with calloc, includes the real executor, and needs only
 * the toolkit's `platform` shim off Windows for the QPC/Interlocked seams --
 * no rasteriser and no Windows guest-memory/VEH setup.
 *
 * Authored by a worker distinct from the production change.
 *
 * ── The fixture, and where its expected bytes come from ──────────────────────
 * An 8x8 linear A8R8G8B8 (format 0x12) texture at TEX_VA, pitch 32 bytes.
 * `sample_tex` returns the raw guest dword for a linear 32-bit format
 * (`*argb = ((const uint32_t *)p)[u]`, p = base + v*pitch), and
 * `nv2a_rc_unpack` reads it as R=(c>>16)&0xFF, G=(c>>8)&0xFF, B=c&0xFF,
 * A=c>>24. So the dword is 0xAARRGGBB and the gradient is DEFINED here, by
 * hand, not read from any candidate helper or probe:
 *
 *   argb(x, y) = 0xFF000000
 *              | ((0x20 + 0x10*x)      << 16)     R, x = 0..7 -> 0x20..0x90
 *              | ((0x18 + 0x08*y)      <<  8)     G, y = 0..7 -> 0x18..0x50
 *              |  (0x30 + 0x10*x + 0x08*y)        B, max 0x30+0x70+0x38 = 0xD8
 *
 * R identifies x and G identifies y, so every texel's dword is distinct, and
 * no channel wraps. The four texels this file asserts:
 *
 *   (6,4) = R 0x20+0x60=0x80, G 0x18+0x20=0x38, B 0x30+0x60+0x20=0xB0
 *         = 0xFF8038B0      <- the stage-1 texel the bump modes must select
 *   (5,4) = R 0x20+0x50=0x70, G 0x38,          B 0x30+0x50+0x20=0xA0
 *         = 0xFF7038A0      <- B's perturbed texel (the delta moves the sample off (6,4))
 *   (3,2) = R 0x20+0x30=0x50, G 0x18+0x10=0x28, B 0x30+0x30+0x10=0x70
 *         = 0xFF502870      <- PROJECT2D at (6,4)/q=2, and the mutation's texel
 *   (2,3) = R 0x20+0x20=0x40, G 0x18+0x18=0x30, B 0x30+0x20+0x18=0x68
 *         = 0xFF403068      <- stage 0's texel, the bump delta source
 *
 * ── The deltas, derived by hand from those bytes ─────────────────────────────
 * xemu psh.c sign3(x) = x*255 >= 128 ? (x*255-256)/127 : (x*255)/127, applied
 * to the decoded UNORM float, so the *byte* decides the sign.
 *   du = sign3(0x68/255): 0x68 = 104, and 104 < 128, so it is 104/127
 *                          = 0.8188976                    (blue, POSITIVE)
 *   dv = sign3(0x30/255): 0x30 =  48 < 128, so  48/127
 *                          = 0.3779528                    (green)
 *   lum = 0x40/255 = 0.2509804                             (red, unsigned)
 * (sign3 only subtracts 256 when the BYTE is >= 128; both deltas here are
 * below that, so both keep their positive sign.)
 *
 *   matrix [m00, m01, m10, m11]  ->  dsdt.x = m00*du + m10*dv
 *                                    dsdt.y = m01*du + m11*dv
 *
 *   A zero matrix            dsdt = (0, 0)
 *   B [-0.25, 0.125, 0.125, 0.5]   (moves off (6,4) to (5,4) for both q)
 *       dsdt.x = (-0.25)(0.8188976) + 0.125(0.3779528)
 *              = -0.2047244 + 0.0472441 = -0.1574803
 *       dsdt.y = 0.125(0.8188976) + 0.5(0.3779528)
 *              =  0.1023622 + 0.1889764 =  0.2913386
 *       (6,4) + that = (5.8425197, 4.2913386) -> floor (5,4) = 0xFF7038A0
 *   C [0.5, 0.125, 0.5, 0.5]  (the "delta really applied" case: it stays on (6,4))
 *       dsdt.x = 0.5(0.8188976) + 0.5(0.3779528)
 *              = 0.4094488 + 0.1889764 = 0.5984252
 *       dsdt.y = 0.125(0.8188976) + 0.5(0.3779528) = 0.2913386
 *       (6,4) + that = (6.5984252, 4.2913386) -> floor (6,4) = 0xFF8038B0
 *   So B is the case where the delta moves the sample OFF (6,4) while staying
 *   in the same row, and C is the case where it stays on (6,4): between them
 *   a "delta ignored" implementation cannot pass both, and a q-divided one
 *   cannot pass either q=2 row.
 *
 * With PROJECT2D's divide restored on the bump path (c/q + dsdt), q=2 gives
 *   A: (3,2)                              = 0xFF502870
 *   B: (3 - 0.1574803, 2 + 0.2913386) = (2.8425197, 2.2913386) -> (2,2)
 *      = 0xFF402860
 *   C: (3 + 0.5984252, 2.2913386) = (3.5984252, 2.2913386) -> (3,2)
 *      = 0xFF502870
 * every one of which differs from the expected dword, so each q=2 row fails.
 *
 * ── Mode 7 luminance, derived by hand ────────────────────────────────────────
 * k = bump_scale[1]*lum + bump_offset[1] = 2.0*(64/255) + 0.25
 *   = 128/255 + 0.25 = 0.5019608 + 0.25 = 0.7519608
 * out = texel(6,4) * k, every channel (xemu multiplies all four):
 *   R = (128/255)*0.7519608 = 0.3774510
 *   G = ( 56/255)*0.7519608 = 0.1651362
 *   B = (176/255)*0.7519608 = 0.5190133
 *   A =          1*0.7519608 = 0.7519608
 * The CPU path returns unclamped floats, so they are asserted as floats.
 *
 * ── The control that must not pass by deleting projection ────────────────────
 * PROJECT2D (mode 1) at the same coordinate (6,4,0,2) MUST land on (3,2):
 * an implementation that globally removed the divide returns 0xFF8038B0 and
 * fails `project2d control: q=2 lands on the projected texel`.
 *
 * One case per process is not needed here (nothing is sampled once), but the
 * program still takes --case so a failure names one case.
 */
#include "kernel/kernel.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_vsh_interp.h"
#include "recomp_types.h"

#include "nv2a_pb_exec.c"       /* THE production executor: rc_stage_fetch etc. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#define ARENA_BYTES 0x00080000u
#define TEX_VA      0x00020000u
#define TEX_W       8u
#define TEX_H       8u
#define TEX_PITCH   (TEX_W * 4u)

/* ── Assertions ─────────────────────────────────────────────────────────────── */

static int g_failures;
static const char *g_case = "?";

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL bumpenv_q_cpu[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void chk_u32(const char *what, uint32_t got, uint32_t want)
{
    chk(got == want, what, "got 0x%08X, want 0x%08X", (unsigned)got, (unsigned)want);
}

static void chk_f(const char *what, float got, float want)
{
    chk(fabsf(got - want) <= 1e-5f, what, "got %.7f, want %.7f", (double)got, (double)want);
}

/* ── Fixture ────────────────────────────────────────────────────────────────── */

static uint8_t *g_arena;

/* The gradient this file defines, in the guest's own A8R8G8B8 dword. */
static uint32_t fixture_argb(uint32_t x, uint32_t y)
{
    return 0xFF000000u
         | (((0x20u + 0x10u * x) & 0xFFu) << 16)
         | (((0x18u + 0x08u * y) & 0xFFu) <<  8)
         |  ((0x30u + 0x10u * x + 0x08u * y) & 0xFFu);
}

static void fixture_texture(void)
{
    uint32_t x, y;
    for (y = 0; y < TEX_H; y++)
        for (x = 0; x < TEX_W; x++)
            *(uint32_t *)(g_arena + TEX_VA + y * TEX_PITCH + x * 4) = fixture_argb(x, y);
}

/* Program the executor's stage state exactly as SET_TEXTURE_* / SET_SHADER_*
 * would leave it, through the same fields rc_stage_fetch reads. */
static void program_stage(uint32_t st, uint32_t mode, const float mat[4],
                          float scale, float offset)
{
    Texture *t = &s_gpu.texs[st];
    int i;

    memset(t, 0, sizeof *t);
    t->offset  = TEX_VA;
    t->width   = TEX_W;
    t->height  = TEX_H;
    t->pitch   = TEX_PITCH;
    t->color   = 0x12u;                 /* LIN_A8R8G8B8 */
    t->addr_u  = 3u;                    /* clamp */
    t->addr_v  = 3u;
    t->filter  = 0u;                    /* nearest */
    t->levels  = 1u;
    t->valid   = 1;
    s_gpu.rc.stage_program &= ~(0x1Fu << (st * 5));
    s_gpu.rc.stage_program |= (mode & 0x1Fu) << (st * 5);
    for (i = 0; i < 4; i++)
        s_gpu.rc.bump_mat[st][i] = mat[i];
    s_gpu.rc.bump_scale[st]  = scale;
    s_gpu.rc.bump_offset[st] = offset;
}

/* The stage's result as the packed dword a pixel would carry: the four floats
 * back to UNORM bytes, which is what makes "the same texel" a byte compare. */
static uint32_t pack(const float v[4])
{
    uint32_t r[4];
    int i;
    for (i = 0; i < 4; i++) {
        float c = v[i] < 0.0f ? 0.0f : (v[i] > 1.0f ? 1.0f : v[i]);
        r[i] = (uint32_t)(c * 255.0f + 0.5f);
    }
    return (r[3] << 24) | (r[0] << 16) | (r[1] << 8) | r[2];
}

/* Fetch stage `st` at coordinate (u,v,r,q) with `tprev` already holding the
 * earlier stages' results. Returns the packed dword and prints the row. */
static uint32_t fetch_packed(const char *label, uint32_t st, float u, float v,
                             float q, const float tprev[4][4])
{
    float c[4], out[4] = { 0, 0, 0, 1 };
    int keep;

    c[0] = u; c[1] = v; c[2] = 0.0f; c[3] = q;
    keep = rc_stage_fetch((int)st, c, tprev, out);
    printf("  %-34s q=%g -> keep=%d rgba=(%.6f, %.6f, %.6f, %.6f) 0x%08X\n",
           label, (double)q, keep, (double)out[0], (double)out[1],
           (double)out[2], (double)out[3], (unsigned)pack(out));
    return pack(out);
}

/* ── Cases ──────────────────────────────────────────────────────────────────── */

/* Stage 0 at (2,3) is the delta source; every case re-derives it through the
 * real fetch so the deltas come from production sampling, not from a constant. */
static void stage0_source(float tprev[4][4])
{
    float c[4], out[4];
    int st;

    for (st = 0; st < 4; st++) {
        tprev[st][0] = tprev[st][1] = tprev[st][2] = 0.0f;
        tprev[st][3] = 1.0f;
    }
    c[0] = 2.0f; c[1] = 3.0f; c[2] = 0.0f; c[3] = 1.0f;
    program_stage(0, 1u, (const float[4]){0, 0, 0, 0}, 0.0f, 0.0f);
    rc_stage_fetch(0, c, tprev, out);
    for (st = 0; st < 4; st++)
        tprev[0][st] = out[st];
    printf("  stage0 source (2,3,0,1) = 0x%08X (hand-derived 0xFF403068)\n",
           (unsigned)pack(out));
    chk_u32("stage 0 delta source is the hand-derived texel (2,3)",
            pack(out), 0xFF403068u);
}

static void bump_pair(const char *label, uint32_t mode, const float mat[4],
                      float scale, float offset, uint32_t want)
{
    static const float zero[4] = { 0, 0, 0, 0 };
    float tprev[4][4];
    uint32_t got1, got2;

    program_stage(1, mode, mat, scale, offset);
    stage0_source(tprev);
    got1 = fetch_packed(label, 1, 6.0f, 4.0f, 1.0f, (const float (*)[4])tprev);
    got2 = fetch_packed(label, 1, 6.0f, 4.0f, 2.0f, (const float (*)[4])tprev);
    (void)zero;
    chk_u32("bump q=1 selects the hand-derived texel", got1, want);
    chk_u32("bump q=2 selects the SAME hand-derived texel (no PROJECT2D divide)",
            got2, want);
    chk(got1 == got2, "bump selection is independent of q");
}

static int case_bump_zero_matrix(void)
{
    static const float zero[4] = { 0, 0, 0, 0 };
    bump_pair("mode6 zero matrix", 6u, zero, 0.0f, 0.0f, 0xFF8038B0u);
    return 0;
}

static int case_bump_displacement(void)
{
    /* B: a small matrix whose perturbed coordinate moves off (6,4) into
     * texel (5,4) for BOTH q -- so a delta that was dropped fails here too.
     * Hand-derived above. */
    static const float m[4] = { -0.25f, 0.125f, 0.125f, 0.5f };
    bump_pair("mode6 displacement", 6u, m, 0.0f, 0.0f, 0xFF7038A0u);
    return 0;
}

static int case_bump_positive_delta(void)
{
    /* C: the same deltas with a positive m00 stay on texel (6,4), so the
     * pair B/C brackets the boundary from both sides: B moves the sample off
     * (6,4) and C leaves it there, so a delta that is dropped cannot satisfy
     * both. */
    static const float m[4] = { 0.5f, 0.125f, 0.5f, 0.5f };
    bump_pair("mode6 positive delta", 6u, m, 0.0f, 0.0f, 0xFF8038B0u);
    return 0;
}

static int case_bump_lum(void)
{
    static const float zero[4] = { 0, 0, 0, 0 };
    float tprev[4][4], out1[4], out2[4], c[4];
    const float k = 2.0f * (64.0f / 255.0f) + 0.25f;   /* hand derivation */

    program_stage(1, 7u, zero, 2.0f, 0.25f);
    stage0_source(tprev);
    c[0] = 6.0f; c[1] = 4.0f; c[2] = 0.0f; c[3] = 1.0f;
    rc_stage_fetch(1, c, tprev, out1);
    c[3] = 2.0f;
    rc_stage_fetch(1, c, tprev, out2);
    printf("  mode7 lum k = %.7f (hand-derived 0.7519608)\n", (double)k);
    printf("  mode7 q=1 rgba=(%.6f, %.6f, %.6f, %.6f)\n",
           (double)out1[0], (double)out1[1], (double)out1[2], (double)out1[3]);
    printf("  mode7 q=2 rgba=(%.6f, %.6f, %.6f, %.6f)\n",
           (double)out2[0], (double)out2[1], (double)out2[2], (double)out2[3]);
    /* Hand-derived: (128/255, 56/255, 176/255, 1) * k, every channel. */
    chk_f("mode7 q=1 R = (128/255)*k", out1[0], (128.0f / 255.0f) * k);
    chk_f("mode7 q=1 G = ( 56/255)*k", out1[1], ( 56.0f / 255.0f) * k);
    chk_f("mode7 q=1 B = (176/255)*k", out1[2], (176.0f / 255.0f) * k);
    chk_f("mode7 q=1 A = 1*k",         out1[3], k);
    chk_f("mode7 q=2 R is the same texel and the same luminance", out2[0], out1[0]);
    chk_f("mode7 q=2 G is the same texel and the same luminance", out2[1], out1[1]);
    chk_f("mode7 q=2 B is the same texel and the same luminance", out2[2], out1[2]);
    chk_f("mode7 q=2 A is the same texel and the same luminance", out2[3], out1[3]);
    return 0;
}

static int case_project2d_control(void)
{
    static const float zero[4] = { 0, 0, 0, 0 };
    float tprev[4][4];
    uint32_t got1, got2;

    /* The control: PROJECT2D MUST still divide. If the divide were removed
     * globally to make the bump rows pass, q=2 would return 0xFF8038B0. */
    program_stage(1, 1u, zero, 0.0f, 0.0f);
    stage0_source(tprev);
    got1 = fetch_packed("mode1 PROJECT2D control", 1, 6.0f, 4.0f, 1.0f,
                        (const float (*)[4])tprev);
    got2 = fetch_packed("mode1 PROJECT2D control", 1, 6.0f, 4.0f, 2.0f,
                        (const float (*)[4])tprev);
    chk_u32("project2d control: q=1 samples the coordinate itself", got1, 0xFF8038B0u);
    chk_u32("project2d control: q=2 lands on the projected texel (6/2, 4/2)",
            got2, 0xFF502870u);
    chk(got1 != got2, "project2d control: q really changes the projected texel");
    return 0;
}

/* ── Case table ─────────────────────────────────────────────────────────────── */

typedef struct { const char *name; int (*fn)(void); } Case;

static const Case g_cases[] = {
    { "bump_zero_matrix",  case_bump_zero_matrix },
    { "bump_displacement", case_bump_displacement },
    { "bump_positive_delta", case_bump_positive_delta },
    { "bump_lum",          case_bump_lum },
    { "project2d_control", case_project2d_control },
};

int main(int argc, char **argv)
{
    size_t i;
    int rc;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];

    printf("bumpenv_q_cpu: BUMPENVMAP q-independence, DIRECT HELPER SEAM "
           "(real rc_stage_fetch, no rasteriser)\n");
    g_arena = (uint8_t *)calloc(1, ARENA_BYTES);
    if (!g_arena) {
        fprintf(stderr, "FAIL bumpenv_q_cpu[%s]: fixture allocation\n", g_case);
        return 1;
    }
    fixture_texture();
    memset(&s_gpu, 0, sizeof s_gpu);

    for (i = 0; i < sizeof g_cases / sizeof g_cases[0]; i++) {
        if (strcmp(g_cases[i].name, g_case) != 0)
            continue;
        rc = g_cases[i].fn();
        if (rc != 0) {
            fprintf(stderr, "FAIL bumpenv_q_cpu[%s]: fixture/harness error\n", g_case);
            return rc;
        }
        if (g_failures) {
            fprintf(stderr, "FAIL bumpenv_q_cpu[%s]: %d assertion%s failed\n", g_case,
                    g_failures, g_failures == 1 ? "" : "s");
            return 1;
        }
        printf("PASS bumpenv_q_cpu[%s]\n", g_case);
        return 0;
    }
    fprintf(stderr, "FAIL bumpenv_q_cpu: unknown case '%s'\n", argv[2]);
    return 2;
}

/* ── The host seams the real executor calls but a test does not own ─────────── */

ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)(uintptr_t)g_arena; }

void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowPresent(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowStart(void) {}
void xbox_FramebufferWindowSetTitle(const uint16_t *name, int max_chars)
{ (void)name; (void)max_chars; }
void xbox_FramebufferWindowFrameStats(uint32_t draws) { (void)draws; }
uint32_t xbox_FramebufferPresentSerial(void) { return 0; }
unsigned long long xbox_FramebufferPresentHash(void) { return 0; }
int xbox_FramebufferDumpBmp(const char *path) { (void)path; return 0; }
void xbox_FramebufferWindowSetAA(uint32_t sy) { (void)sy; }
int xbox_Nv2aFenceRelease(uint32_t value) { (void)value; return 0; }
void xbox_Nv2aFrameCounterFlip(void) {}
uint32_t xbox_ContiguousAllocatedBytes(void) { return ARENA_BYTES; }
uint32_t g_xbox_image_lo = 0x00010000u, g_xbox_image_hi = 0x03FFFFFFu;
void nv2a_set_commit_consumer(nv2a_commit_consumer_fn fn) { (void)fn; }

/* The mono-clock anchor the executor's QPC conversion reads (nv2a_core.c). */
volatile LONGLONG g_nv2a_mono_anchor_count;
