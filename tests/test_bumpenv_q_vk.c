/* Focused deciding gate: BUMPENVMAP / BUMPENVMAP_LUM must not apply
 * PROJECT2D's texture-q division. Actual AOT Vulkan half, plus the production
 * texture-binding witness.
 *
 * SEAM: this translation unit #includes the toolkit's src/video/nv2a_vk_backend.c
 * with XBR_VK_BACKEND defined (exactly as tests/test_pbcov_vk.c and
 * tests/test_vk_readback_fault_gate.c do), so `nv2a_vk_backend_install`,
 * `vb_draw`, `vb_flip`, `vb_rc_prepare`, the file-static `g` and the production
 * vtable are the REAL bodies. The pipeline is the ahead-of-time SPIR-V the
 * build compiled from src/video/shaders/nv2a_vk_rc.hlsl with dxc + spirv-val;
 * nothing is compiled at run time here or anywhere.
 *
 * ONE STAND-IN, LABELLED: `nv2a_backend_decode_texture` is stubbed (the toolkit
 * library is not linked, as in the sibling Vulkan tests). For a linear
 * A8R8G8B8 texture the stub reads exactly what production reads --
 * `((const uint32_t *)(mem + offset + y*pitch))[x]` in nv2a_pb_exec.c's
 * sample_tex -- because the decoder is a fixture input, not the rule under
 * test. The sampling, the combiner and the readback are all production.
 *
 * ── The fixture, and where its expected bytes come from ──────────────────────
 * An 8x8 linear A8R8G8B8 (0x12) texture at TEX_VA, pitch 32. The gradient is
 * DEFINED here by hand (no candidate helper or probe is an oracle):
 *
 *   argb(x, y) = 0xFF000000 | ((0x20+0x10x)<<16) | ((0x18+0x08y)<<8)
 *                          |  (0x30+0x10x+0x08y)
 *
 * R identifies x and G identifies y, so texels are distinct and no channel
 * wraps. The asserted texels:
 *
 *   (6,4) = 0x80,0x38,0xB0 -> 0xFF8038B0   stage 1's unperturbed texel
 *   (3,2) = 0x50,0x28,0x70 -> 0xFF502870   PROJECT2D at (6,4)/q=2, and what a
 *                                          restored /q on the bump path gives
 *   (2,3) = 0x40,0x30,0x68 -> 0xFF403068   stage 0, the bump delta source
 *
 * Deltas from those bytes (xemu psh.c sign3 on the decoded UNORM:
 *   x *= 255; x >= 128 ? (x-256)/127 : x/127 -- so the BYTE decides the sign
 *   and only a byte of 128 or more subtracts 256):
 *   du = sign3(0x68/255): 0x68 = 104 < 128, so 104/127 = 0.8188976  (blue, POSITIVE)
 *   dv = sign3(0x30/255): 0x30 =  48 < 128, so  48/127 = 0.3779528  (green)
 *   lum = 0x40/255 = 0.2509804 (red, unsigned)
 *
 * A zero matrix: dsdt = (0,0), so stage 1 samples (6,4) for BOTH q -> 0xFF8038B0.
 * With the /q divide restored, q=2 samples (3,2) -> 0xFF502870, so the q=2 row
 * fails with the named assertion "vk bump q=2 wrote the same texel as q=1".
 *
 * Matrix B [-0.25, 0.125, 0.125, 0.5] as [m00, m01, m10, m11], with those
 * positive deltas:
 *   dsdt.x = m00*du + m10*dv = (-0.25)(0.8188976) + 0.125(0.3779528)
 *          = -0.2047244 + 0.0472441 = -0.1574803
 *   dsdt.y = m01*du + m11*dv = 0.125(0.8188976) + 0.5(0.3779528)
 *          =  0.1023622 + 0.1889764 =  0.2913386
 *   (6,4) + dsdt = (5.8425197, 4.2913386) -> floor (5,4) = 0xFF7038A0
 * for BOTH q, because the bump path applies no divide. With the divide
 * restored, q=2 gives (3 - 0.1574803, 2 + 0.2913386) = (2.8425197, 2.2913386)
 * -> floor (2,2) = 0xFF402860, so the displacement row fails as well.
 *
 * Mode 7 luminance: k = scale*lum + offset = 2*(64/255)+0.25 = 0.7519608,
 * applied to every channel of (128,56,176,255)/255:
 *   R 0.3774510 -> 96 (0x60)   G 0.1651362 -> 42 (0x2A)
 *   B 0.5190133 -> 132 (0x84)  A 0.7519608 -> 192 (0xC0)
 * so the target pixel must be 0xC0602A84 (ARGB dword) for both q.
 *
 * ── The PROJECT2D control ────────────────────────────────────────────────────
 * Mode 1 at the same coordinate (6,4,0,2) MUST land on (3,2) = 0xFF502870.
 * An implementation that deleted the divide globally fails
 * "vk project2d control q=2 wrote the projected texel".
 *
 * ── R2, the production binding witness ───────────────────────────────────────
 * A texture bound at a stage whose mode is 4 (PASSTHRU) or 5 (CLIPPLANE) must
 * not be uploaded or bound, while modes 1, 2, 3, 6 and 7 must. The witness is
 * PRODUCTION state, not a re-evaluation of the range predicate: each mode gets
 * its OWN unused texture address, so a texture-cache entry at that address can
 * only exist because the back end bound it, and `g.st.tex_uploads` /
 * `g.st.tex_hits` separate "uploaded" from "cache hit". A range predicate
 * copied into this test cannot fabricate either.
 *
 * One process per case. A failed install when the environment names an ICD is
 * a FAILURE, not a skip.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>

#define ARENA_BYTES 0x00100000u
#define SURF_VA     0x00010000u
#define SURF_W      64u
#define SURF_H      64u
#define SURF_PITCH  (SURF_W * 4u)
#define TEX_VA      0x00020000u
#define TEX_W       8u
#define TEX_H       8u
#define TEX_PITCH   (TEX_W * 4u)

static uint8_t *g_arena;
static int g_failures;
static const char *g_case = "?";

static const Nv2aBackend *g_registered;

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL bumpenv_q_vk[%s]: %s", g_case, what);
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

/* ── Fixture ────────────────────────────────────────────────────────────────── */

static Nv2aVertex g_v[6];
static Nv2aVertexExtra g_x[6];
static Nv2aTexture g_tex;      /* stage 1's, the one under test */
static Nv2aTexture g_tex0;     /* stage 0's, always TEX_VA */
static Nv2aCombiner g_rc;
static Nv2aRenderState g_rs;
static Nv2aSurface g_surf;
static Nv2aBatch g_b;

static uint32_t fixture_argb(uint32_t x, uint32_t y)
{
    return 0xFF000000u
         | (((0x20u + 0x10u * x) & 0xFFu) << 16)
         | (((0x18u + 0x08u * y) & 0xFFu) <<  8)
         |  ((0x30u + 0x10u * x + 0x08u * y) & 0xFFu);
}

static void fixture_init(void)
{
    static const float cp[6][2] = {
        { 0.0f, 0.0f }, { 64.0f, 0.0f }, { 0.0f, 64.0f },
        { 0.0f, 64.0f }, { 64.0f, 0.0f }, { 64.0f, 64.0f }
    };
    uint32_t x, y;
    int i;

    g_arena = (uint8_t *)calloc(1, ARENA_BYTES);
    if (!g_arena)
        return;
    for (y = 0; y < TEX_H; y++)
        for (x = 0; x < TEX_W; x++)
            *(uint32_t *)(g_arena + TEX_VA + y * TEX_PITCH + x * 4) = fixture_argb(x, y);

    for (i = 0; i < 6; i++) {
        memset(&g_v[i], 0, sizeof g_v[i]);
        g_v[i].x = cp[i][0];
        g_v[i].y = cp[i][1];
        g_v[i].z = 0.5f;
        g_v[i].rhw = 1.0f;
        g_v[i].diffuse = 0xFFFFFFFFu;
        memset(&g_x[i], 0, sizeof g_x[i]);
        g_x[i].specular = 0xFFFFFFFFu;
        g_x[i].fog = 1.0f;
        /* Stage 0 samples (2,3); stage 1 is the bump stage under test. */
        g_x[i].tex[0][0] = 2.0f; g_x[i].tex[0][1] = 3.0f;
        g_x[i].tex[0][2] = 0.0f; g_x[i].tex[0][3] = 1.0f;
        g_x[i].tex[1][0] = 6.0f; g_x[i].tex[1][1] = 4.0f;
        g_x[i].tex[1][2] = 0.0f; g_x[i].tex[1][3] = 1.0f;   /* q set per case */
        g_x[i].tex[2][3] = 1.0f;
        g_x[i].tex[3][3] = 1.0f;
    }

    memset(&g_rs, 0, sizeof g_rs);
    g_rs.color_mask = 0x01010101u;      /* write every channel */
    g_rs.alpha_func = 0x207u;           /* ALWAYS */
    g_rs.depth_func = 0x207u;
    g_rs.stencil_func = 0x207u;
    g_rs.stencil_write_mask = g_rs.stencil_read_mask = 0xFFu;
    g_rs.cull_face = 0x405u;            /* none */
    g_rs.front_face = 0x900u;
    g_rs.depth_min = 0.0f;
    g_rs.depth_max = 16777215.0f;

    memset(&g_surf, 0, sizeof g_surf);
    g_surf.color_va = SURF_VA;
    g_surf.width = SURF_W;
    g_surf.height = SURF_H;
    g_surf.pitch = SURF_PITCH;
    g_surf.bytes_per_pixel = 4u;
    g_surf.color_format = 8u;
    g_surf.aa_sx = g_surf.aa_sy = 1u;

    memset(&g_tex, 0, sizeof g_tex);
    g_tex.offset = TEX_VA;
    g_tex.width = TEX_W;
    g_tex.height = TEX_H;
    g_tex.pitch = TEX_PITCH;
    g_tex.color = 0x12u;
    g_tex.addr_u = g_tex.addr_v = 3u;
    g_tex.linear = 1u;
    /* MIN = 1, MAG = 1: vb_filter_linear() gives NEAREST for mode 1 on both
     * halves, so a texel-coordinate fetch is the texel, not a bilinear blend. */
    g_tex.filter = (1u << 24) | (1u << 16);

    g_tex0 = g_tex;                 /* same image, a separate binding */

    memset(&g_rc, 0, sizeof g_rc);
    /* The final combiner alone, set to emit R[9] (stage 1's texel):
     * f0 = A<<24 | B<<16 | C<<8 | D, o.rgb = D + C*(1-A) + B*A.
     *   A = 0x00 (register 0, identity), B = 0x01 (c0 = white),
     *   C = 0x09 (R[9] = t1, identity map), D = 0x00.
     * f1's alpha byte selects t1's alpha: 0x19 = register 9 with the alpha bit. */
    g_rc.final0 = 0x00010900u;
    g_rc.final1 = 0x00001901u;
    g_rc.final_c0 = 0xFFFFFFFFu;
    g_rc.final_c1 = 0xFFFFFFFFu;
}

/* The centre pixel of the target, read back out of GUEST MEMORY after vb_flip. */
static uint32_t guest_pixel(uint32_t x, uint32_t y)
{
    return *(uint32_t *)(g_arena + SURF_VA + y * SURF_PITCH + x * 4);
}

/* One draw through the production vb_draw, then the production vb_flip, so the
 * target's pixels reach guest memory. `prog` is the raw SET_SHADER_STAGE_PROGRAM
 * word; `q` is stage 1's texture q. */
static uint32_t draw_and_read(uint32_t prog, float q, uint32_t tex_va, int bind_tex1)
{
    int i;

    for (i = 0; i < 6; i++)
        g_x[i].tex[1][3] = q;
    memset(&g_b, 0, sizeof g_b);
    g_b.vertices = g_v;
    g_b.count = 6u;
    g_b.texture = NULL;
    g_b.state = &g_rs;
    g_b.extra = g_x;
    g_b.textures[0] = bind_tex1 ? &g_tex0 : NULL;
    g_b.textures[1] = bind_tex1 ? &g_tex : NULL;
    g_b.textures[2] = NULL;
    g_b.textures[3] = NULL;
    g_tex.offset = tex_va;
    g_tex0.offset = TEX_VA;
    g_rc.stage_program = prog;
    g_b.combiner = &g_rc;
    g_b.fog_color = 0xFFFFFFFFu;
    g_b.clip_plane_mode = 0xFFFFu;      /* CLIPPLANE: keep everything */
    vb_draw(&g_surf, &g_b);
    vb_flip();
    return guest_pixel(SURF_W / 2u, SURF_H / 2u);
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

/* ── R1: bump q independence, through the AOT shader ────────────────────────── */

static int case_vk_bump_q(void)
{
    uint32_t got_q1, got_q2, got_lum_q1, got_lum_q2;
    int i;

    /* Stage 0 mode 1 (PROJECT2D, q=1 -> (2,3)); stage 1 mode 6, zero matrix. */
    g_rc.bump_mat[1][0] = g_rc.bump_mat[1][1] = 0.0f;
    g_rc.bump_mat[1][2] = g_rc.bump_mat[1][3] = 0.0f;
    g_rc.bump_scale[1] = 0.0f;
    g_rc.bump_offset[1] = 0.0f;
    for (i = 0; i < 6; i++)
        g_x[i].tex[0][3] = 1.0f;

    got_q1 = draw_and_read(1u | (6u << 5), 1.0f, TEX_VA, 1);
    got_q2 = draw_and_read(1u | (6u << 5), 2.0f, TEX_VA, 1);
    printf("  mode6 zero matrix: q=1 -> 0x%08X, q=2 -> 0x%08X (hand-derived 0xFF8038B0)\n",
           (unsigned)got_q1, (unsigned)got_q2);
    chk_u32("vk bump q=1 wrote the hand-derived texel", got_q1, 0xFF8038B0u);
    chk_u32("vk bump q=2 wrote the same texel as q=1", got_q2, 0xFF8038B0u);

    /* Matrix B, the same derivation the CPU file's bump_displacement asserts:
     * dsdt = (-0.1574803, 0.2913386), so (6,4) + dsdt -> texel (5,4), the
     * SAME texel for both q. This is the row that fails if the delta is
     * dropped as well as the row that fails if it is divided. */
    g_rc.bump_mat[1][0] = -0.25f;      /* m00 */
    g_rc.bump_mat[1][1] =  0.125f;     /* m01 */
    g_rc.bump_mat[1][2] =  0.125f;     /* m10 */
    g_rc.bump_mat[1][3] =  0.5f;       /* m11 */
    got_q1 = draw_and_read(1u | (6u << 5), 1.0f, TEX_VA, 1);
    got_q2 = draw_and_read(1u | (6u << 5), 2.0f, TEX_VA, 1);
    printf("  mode6 displacement: q=1 -> 0x%08X, q=2 -> 0x%08X (hand-derived 0xFF7038A0)\n",
           (unsigned)got_q1, (unsigned)got_q2);
    chk_u32("vk bump displacement q=1 wrote the perturbed hand-derived texel",
            got_q1, 0xFF7038A0u);
    chk_u32("vk bump displacement q=2 wrote the same perturbed texel as q=1",
            got_q2, 0xFF7038A0u);
    /* The perturbed texel was actually reached AND it is not the unperturbed
     * one. `got_q1 == got_q2` above only says the two q agree; this says the
     * delta did something. */
    chk(got_q1 == 0xFF7038A0u && got_q1 != 0xFF8038B0u,
        "vk bump displacement moved the sample off (6,4) to the perturbed texel",
        "got 0x%08X", (unsigned)got_q1);
    g_rc.bump_mat[1][0] = g_rc.bump_mat[1][1] = 0.0f;
    g_rc.bump_mat[1][2] = g_rc.bump_mat[1][3] = 0.0f;

    /* Mode 7 with a nonzero scale/offset and a nonzero stage-0 red. */
    g_rc.bump_scale[1] = 2.0f;
    g_rc.bump_offset[1] = 0.25f;
    got_lum_q1 = draw_and_read(1u | (7u << 5), 1.0f, TEX_VA, 1);
    got_lum_q2 = draw_and_read(1u | (7u << 5), 2.0f, TEX_VA, 1);
    printf("  mode7 lum: q=1 -> 0x%08X, q=2 -> 0x%08X (hand-derived 0xC0602A84)\n",
           (unsigned)got_lum_q1, (unsigned)got_lum_q2);
    chk_u32("vk bump lum q=1 wrote the hand-derived luminance texel",
            got_lum_q1, 0xC0602A84u);
    chk_u32("vk bump lum q=2 wrote the same texel as q=1",
            got_lum_q2, 0xC0602A84u);
    return 0;
}

static int case_vk_project2d_control(void)
{
    uint32_t got_q1, got_q2;

    g_rc.bump_mat[1][0] = g_rc.bump_mat[1][1] = 0.0f;
    g_rc.bump_mat[1][2] = g_rc.bump_mat[1][3] = 0.0f;
    got_q1 = draw_and_read(1u | (1u << 5), 1.0f, TEX_VA, 1);
    got_q2 = draw_and_read(1u | (1u << 5), 2.0f, TEX_VA, 1);
    printf("  mode1 PROJECT2D control: q=1 -> 0x%08X, q=2 -> 0x%08X "
           "(hand-derived 0xFF8038B0 / 0xFF502870)\n",
           (unsigned)got_q1, (unsigned)got_q2);
    chk_u32("vk project2d control q=1 wrote the coordinate itself", got_q1, 0xFF8038B0u);
    chk_u32("vk project2d control q=2 wrote the projected texel", got_q2, 0xFF502870u);
    chk(got_q1 != got_q2, "vk project2d control: q really changes the projected texel");
    return 0;
}

/* ── R2: the production binding witness, per stage ──────────────────────────── */

/* One unused texture address per probe, so a cache entry at that address can
 * only exist because the back end bound it. */
#define PROBE_VA(base) (0x00040000u + 0x1000u * (uint32_t)(base))

static int case_vk_bind_witness(void)
{
    static const uint32_t bound_modes[] = { 1u, 2u, 3u, 6u, 7u };
    static const uint32_t unbound_modes[] = { 4u, 5u };
    uint64_t up0;
    size_t k;
    int j;

    printf("  production state: g.st.tex_uploads / g.st.tex_hits and the "
           "texture-cache entries g.texs[]\n");

    /* Warm stage 0 (TEX_VA) once, with stage 1 off. From here stage 0 is a
     * cache hit on every draw, so any UPLOAD in a probe below can only be the
     * probe's own stage-1 texture -- the counters stop being ambiguous. */
    draw_and_read(1u, 1.0f, TEX_VA, 1);
    chk(g.st.tex_uploads == 1u, "the warm-up uploaded stage 0's texture once",
        "tex_uploads %llu", (unsigned long long)g.st.tex_uploads);

    /* Modes 4 and 5 must not upload OR bind the texture at their stage. */
    for (k = 0; k < sizeof unbound_modes / sizeof unbound_modes[0]; k++) {
        uint32_t mode = unbound_modes[k];
        uint32_t va = PROBE_VA(mode);
        int cached = 0;
        char what[128];

        up0 = g.st.tex_uploads;
        draw_and_read(1u | (mode << 5), 1.0f, va, 1);
        for (j = 0; j < VB_MAX_TEXTURES; j++)
            if (g.texs[j].used && g.texs[j].offset == va)
                cached = 1;
        _snprintf(what, sizeof what,
                  "mode %u must not upload its stage texture", (unsigned)mode);
        what[sizeof what - 1] = '\0';
        chk(g.st.tex_uploads == up0, what, "tex_uploads %llu -> %llu",
            (unsigned long long)up0, (unsigned long long)g.st.tex_uploads);
        _snprintf(what, sizeof what,
                  "mode %u leaves no texture-cache entry at its stage texture",
                  (unsigned)mode);
        what[sizeof what - 1] = '\0';
        chk(!cached, what);
        printf("  mode %u: uploads %llu, cached %d\n", (unsigned)mode,
               (unsigned long long)(g.st.tex_uploads - up0), cached);
    }

    /* Modes 1, 2, 3, 6 and 7 must upload and bind it. */
    for (k = 0; k < sizeof bound_modes / sizeof bound_modes[0]; k++) {
        uint32_t mode = bound_modes[k];
        uint32_t va = PROBE_VA(0x40u + mode);
        int cached = 0;
        char what[128];

        up0 = g.st.tex_uploads;
        draw_and_read(1u | (mode << 5), 1.0f, va, 1);
        for (j = 0; j < VB_MAX_TEXTURES; j++)
            if (g.texs[j].used && g.texs[j].offset == va)
                cached = 1;
        _snprintf(what, sizeof what,
                  "mode %u must upload its stage texture", (unsigned)mode);
        what[sizeof what - 1] = '\0';
        chk(g.st.tex_uploads == up0 + 1u, what, "tex_uploads %llu -> %llu",
            (unsigned long long)up0, (unsigned long long)g.st.tex_uploads);
        _snprintf(what, sizeof what,
                  "mode %u must bind its stage texture", (unsigned)mode);
        what[sizeof what - 1] = '\0';
        chk(cached, what);
        printf("  mode %u: uploads %llu, cached %d\n", (unsigned)mode,
               (unsigned long long)(g.st.tex_uploads - up0), cached);
    }

    /* The distinction the witness exists for: a bound texture is either
     * uploaded once or served from the cache, never "neither". */
    {
        uint32_t va = PROBE_VA(0x80u);
        uint64_t up_a, up_b, hit_a, hit_b;

        up_a = g.st.tex_uploads; hit_a = g.st.tex_hits;
        draw_and_read(1u | (6u << 5), 1.0f, va, 1);        /* first use: upload */
        up_b = g.st.tex_uploads; hit_b = g.st.tex_hits;
        chk(up_b == up_a + 1u, "first use of a bound texture uploads it once",
            "tex_uploads %llu -> %llu", (unsigned long long)up_a,
            (unsigned long long)up_b);
        draw_and_read(1u | (6u << 5), 1.0f, va, 1);        /* second: cache hit */
        chk(g.st.tex_uploads == up_b,
            "second use of the same bound texture does not upload again",
            "tex_uploads %llu -> %llu", (unsigned long long)up_b,
            (unsigned long long)g.st.tex_uploads);
        chk(g.st.tex_hits > hit_b,
            "second use is served as a cache hit, not silently skipped",
            "tex_hits %llu -> %llu", (unsigned long long)hit_b,
            (unsigned long long)g.st.tex_hits);
        (void)hit_a;
    }
    return 0;
}

/* ── Case table ─────────────────────────────────────────────────────────────── */

typedef struct { const char *name; int (*fn)(void); } Case;

static const Case g_cases[] = {
    { "vk_bump_q",             case_vk_bump_q },
    { "vk_project2d_control",  case_vk_project2d_control },
    { "vk_bind_witness",       case_vk_bind_witness },
};

int main(int argc, char **argv)
{
    const char *icd_files, *icd_names;
    size_t i;
    int rc;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: bumpenv_q_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];

    printf("bumpenv_q_vk: BUMPENVMAP q-independence on the real Vulkan back end "
           "(AOT SPIR-V, lavapipe)\n");
    fixture_init();
    if (!g_arena) {
        fprintf(stderr, "FAIL bumpenv_q_vk[%s]: fixture allocation\n", g_case);
        return 1;
    }
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    rc = nv2a_vk_backend_install();
    if (rc != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            fprintf(stderr, "FAIL bumpenv_q_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: bumpenv_q_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }
    report_device();

    for (i = 0; i < sizeof g_cases / sizeof g_cases[0]; i++) {
        if (strcmp(g_cases[i].name, g_case) != 0)
            continue;
        rc = g_cases[i].fn();
        if (rc != 0) {
            fprintf(stderr, "FAIL bumpenv_q_vk[%s]: fixture/harness error\n", g_case);
            return rc;
        }
        if (g_failures) {
            fprintf(stderr, "FAIL bumpenv_q_vk[%s]: %d assertion%s failed\n", g_case,
                    g_failures, g_failures == 1 ? "" : "s");
            return 1;
        }
        printf("PASS bumpenv_q_vk[%s]\n", g_case);
        return 0;
    }
    fprintf(stderr, "FAIL bumpenv_q_vk: unknown case '%s'\n", argv[2]);
    return 2;
}

/* ── The toolkit symbols this translation unit does not link ────────────────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)g_arena;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    g_registered = backend;
}

/* LABELLED STAND-IN for a format decoder that is not under test. For a linear
 * A8R8G8B8 texture this is exactly what nv2a_pb_exec.c's sample_tex does:
 *     *argb = ((const uint32_t *)(mem + base + v * t->pitch))[u];
 * The sampling that decides WHICH texel is fetched happens on the device. */
int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t x, y;
    if (!tex || !argb_out || !tex->width || !tex->height)
        return 0;
    if (tex->color != 0x12u || !tex->pitch)
        return 0;
    for (y = 0; y < tex->height; y++)
        for (x = 0; x < tex->width; x++)
            argb_out[(size_t)y * tex->width + x] =
                ((const uint32_t *)(g_arena + tex->offset + (size_t)y * tex->pitch))[x];
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    if (!tex || !tex->width || !tex->height || !tex->pitch)
        return 0;
    return tex->pitch * (tex->height - 1u) + tex->width * 4u;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}

/* The mono-clock anchor the executor's QPC conversion reads (nv2a_core.c). */
volatile LONGLONG g_nv2a_mono_anchor_count;
