/* GPU back end parity: the same pushbuffer, executed by the software (CPU)
 * executor and by the Direct3D 11 back end, must leave agreeing colour surfaces
 * in guest memory once the frame is flipped.
 *
 * Each case builds a pushbuffer by hand (standalone NV2A, NV097 object on
 * subchannel 0, the real executor on the commit seam), runs it once with no back
 * end to get the CPU reference, and once with the D3D11 back end installed.
 * Software passes run first for every case because nv2a_d3d11_backend_install()
 * is idempotent and cannot be re-registered after nv2a_backend_register(NULL).
 *
 * What "agrees" means, and why these numbers:
 *   - Per-channel tolerance 8 (8888 surfaces) or 9 (R5G6B5). The CPU path
 *     truncates when it packs 565 and the GPU rounds; after bit-replicating back
 *     to 8 bits one 5-bit step is up to 9, so a single quantisation step must pass.
 *   - At least 98% of ALL pixels agree (the specified bar) AND the pixels inside
 *     the drawn region (union of both coverages) disagree by no more than
 *     perimeter + 2% of that region -- the first alone is nearly vacuous for a
 *     small triangle on a large cleared surface.
 *   - Coverage (pixel differs from the clear colour by more than 24) may differ
 *     by at most one pixel along the shape's perimeter: the two rasterisers use
 *     different edge rules.
 * The CPU path is flat shaded (first vertex colour) and the back end Gouraud
 * shades, so every case uses one diffuse colour for all vertices. */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "video/nv2a_d3d11_backend.h"
#include "recomp_types.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kernel/xbox_memory_layout.h's base address, spelled here to keep that header out of this
 * recomp_types.h translation unit. */
#define XBOX_BASE_ADDRESS 0x00010000u
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);
extern void xbox_MemoryLayoutShutdown(void);

/* The kernel bridge references the game dispatcher/diagnostics; this focused
 * fixture does not start guest threads, so keep those hooks inert. */
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site,
                        uint32_t value)
{ (void)kind; (void)target; (void)site; (void)value; }
/* The game's slot-latch / slot-watch hooks live in src/diagnostics.c, which this
 * toolkit-linked test does not build; inert stubs so the toolkit objects link. */
void jsrf_slot_latch_install(uint32_t raw_value, uint32_t installed_value)
{ (void)raw_value; (void)installed_value; }
void jsrf_slot_latch_sample(uint32_t tid, uint32_t call_index, uint32_t ordinal,
                            uint32_t before, uint32_t after)
{ (void)tid; (void)call_index; (void)ordinal; (void)before; (void)after; }
void jsrf_slot_watch_handshake(uint32_t slot_va) { (void)slot_va; }
void jsrf_slot_watch_alias_armed(uint32_t mapped_mask, uint32_t protect_mask, uint32_t alias_count)
{ (void)mapped_mask; (void)protect_mask; (void)alias_count; }
int jsrf_slot_watch_alias_touch(uint32_t alias_index, uint32_t fault_va, uint64_t rip,
                                uint32_t value, uint32_t published)
{ (void)alias_index; (void)fault_va; (void)rip; (void)value; (void)published; return 0; }
void jsrf_slot_watch_write(uint32_t provenance, uint32_t before, uint32_t after,
                           uint64_t rip, uint32_t ordinal)
{ (void)provenance; (void)before; (void)after; (void)rip; (void)ordinal; }

/* ── Guest memory map used by the cases ──────────────────────────────────
 * Plain RAM addresses well clear of the (synthetic, empty) XBE image; no
 * contiguous allocation is made, so a DMA offset resolves to itself. */
#define SURF_C1   0x00400000u     /* case 1: 64x64 R5G6B5            */
#define SURF_C2   0x00440000u     /* case 2: 64x64 A8R8G8B8          */
#define SURF_C3A  0x00480000u     /* case 3: A, 32x32 A8R8G8B8       */
#define SURF_C3B  0x004C0000u     /* case 3: B, 64x64 A8R8G8B8       */
#define SURF_C4   0x00500000u     /* case 4: 64x64 R5G6B5            */
#define VB_C1     0x00600000u
#define VB_C2     0x00610000u
#define VB_C3A    0x00620000u
#define VB_C3B    0x00630000u
#define VB_C4     0x00640000u
#define TEX_C2    0x00650000u     /* 8x8 R5G6B5 linear               */
#define SURF_C5   0x00460000u     /* case 2b: 64x64 A8R8G8B8         */
#define VB_C5     0x00660000u
#define TEX_C5    0x00670000u     /* 16x16 R5G6B5 linear             */
#define SURF_C6   0x00470000u     /* case 2c: 64x64 A8R8G8B8         */
#define VB_C6     0x00680000u
#define FILTER_NEAREST 0x01010000u   /* SET_TEXTURE_FILTER: MIN=1 (bits 16-23), MAG=1 (24-27) */
#define FILTER_LINEAR  0x02020000u   /* MIN=2, MAG=2 */
#define SURF_BYTES 0x00010000u    /* span pre-filled/cleared per surface */
#define SENTINEL  0x5Au

#define TOL_8888 8
#define TOL_565  9
#define COVER_TOL 24

/* ── Pushbuffer builder ──────────────────────────────────────────────────── */
static uint32_t *g_pb;
static uint32_t g_pbn;

static void pb_begin(void)
{
    memset(g_pb, 0, 0x4000);
    g_pbn = 0;
    g_pb[g_pbn++] = (1u << 18); g_pb[g_pbn++] = 0x6u;     /* SET_OBJECT NV097 */
}
static void pb(uint32_t method, uint32_t value)
{
    g_pb[g_pbn++] = (1u << 18) | method;
    g_pb[g_pbn++] = value;
}
static void pb_surface(uint32_t va, uint32_t w, uint32_t h, uint32_t bpp)
{
    pb(0x0200, w << 16);                           /* CLIP_H: x0 w */
    pb(0x0204, h << 16);                           /* CLIP_V: y0 h */
    pb(0x0208, bpp == 2 ? 0x0103u : 0x0108u);      /* linear R5G6B5 / A8R8G8B8 */
    pb(0x020C, w * bpp);                           /* PITCH */
    pb(0x0210, va);                                /* COLOR_OFFSET */
}
static void pb_clear(uint32_t argb)
{
    pb(0x1D90, argb);                              /* SET_COLOR_CLEAR_VALUE */
    pb(0x1D94, 0xF0u);                             /* CLEAR_SURFACE colour */
}
/* Vertex = float3 position @0, D3DCOLOR diffuse @12, float2 texel uv @16; 24 bytes. */
static void pb_vertex_format(uint32_t vb, int textured)
{
    pb(0x1720 + 0 * 4, vb);
    pb(0x1760 + 0 * 4, 2u | (3u << 4) | (24u << 8));
    pb(0x1720 + 3 * 4, vb + 12);
    pb(0x1760 + 3 * 4, 0u | (4u << 4) | (24u << 8));
    if (textured) {
        pb(0x1720 + 9 * 4, vb + 16);
        pb(0x1760 + 9 * 4, 2u | (2u << 4) | (24u << 8));
    } else {
        pb(0x1720 + 9 * 4, 0);
        pb(0x1760 + 9 * 4, 0);
    }
}
static void pb_texture_off(void) { pb(0x1B0C, 0); }
/* Stage 0, linear, clamp. fmt: 0x11 R5G6B5, 0x12 A8R8G8B8. */
static void pb_texture_on(uint32_t va, uint32_t fmt, uint32_t w, uint32_t h, uint32_t pitch,
                          uint32_t filter)
{
    pb(0x1B14, filter);                            /* SET_TEXTURE_FILTER */
    pb(0x1B00, va);
    pb(0x1B04, 0x1u | (2u << 4) | (fmt << 8) | (1u << 16));
    pb(0x1B08, 0x0303u);                           /* clamp U and V */
    pb(0x1B10, pitch << 16);                       /* CONTROL1: pitch */
    pb(0x1B1C, (w << 16) | h);                     /* IMAGE_RECT */
    pb(0x1B0C, 1u << 30);                          /* CONTROL0: ENABLE */
}
static void pb_draw_triangles(uint32_t nverts)
{
    pb(0x17FC, 5u);                                /* BEGIN TRIANGLES */
    pb(0x1810, ((nverts - 1u) << 24) | 0u);        /* DRAW_ARRAYS from 0 */
    pb(0x17FC, 0u);                                /* END */
}
static void pb_flip(void) { pb(0x0130, 0); }       /* FLIP_STALL */

/* ── Guest memory helpers ────────────────────────────────────────────────── */
static uint8_t *G(uint32_t va) { return (uint8_t *)xbox_GetMemoryOffset() + va; }

static void put_vertex(uint32_t vb, int i, float x, float y, uint32_t diffuse, float u, float v)
{
    float *f = (float *)G(vb + (uint32_t)i * 24u);
    f[0] = x; f[1] = y; f[2] = 0.5f;
    *(uint32_t *)(G(vb + (uint32_t)i * 24u) + 12) = diffuse;
    f[4] = u; f[5] = v;
}

static uint16_t pack565(uint32_t r, uint32_t g, uint32_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* ── The cases ───────────────────────────────────────────────────────────── */
typedef struct { uint32_t va, w, h, bpp, clear; double perim, area; } Surf;
typedef struct {
    const char *name;
    int nsurf;
    Surf surf[2];
} Scene;

static Scene g_scene[6];

/* Case 1 and 4 share one scene shape at different addresses. */
static void build_clear_tri(uint32_t surf, uint32_t vb, int flip, Scene *sc, const char *name)
{
    const uint32_t clear = 0xFF204060u, diffuse = 0xFFE06020u;
    pb_begin();
    pb_surface(surf, 64, 64, 2);
    pb_clear(clear);
    pb_texture_off();
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 8.0f, 8.0f, diffuse, 0, 0);
    put_vertex(vb, 1, 56.0f, 12.0f, diffuse, 0, 0);
    put_vertex(vb, 2, 20.0f, 58.0f, diffuse, 0, 0);
    pb_draw_triangles(3);
    if (flip) pb_flip();
    if (sc) {
        sc->name = name; sc->nsurf = 1;
        sc->surf[0] = (Surf){ surf, 64, 64, 2, clear,
            hypot(48, 4) + hypot(36, 46) + hypot(12, 50),
            fabs(0.5 * (48.0 * 50.0 - 4.0 * 12.0)) };
    }
}

static void build_case1(int flip) { build_clear_tri(SURF_C1, VB_C1, flip, &g_scene[0], "1 clear + untextured triangle (R5G6B5)"); }
static void build_case4(int flip) { build_clear_tri(SURF_C4, VB_C4, flip, &g_scene[3], "4 negative control (R5G6B5)"); }

/* Case 2 and 2c: an 8x8 R5G6B5 texture magnified about 6x onto a triangle. */
static void build_case2_with(int idx, uint32_t surf, uint32_t vb, uint32_t filter, const char *name)
{
    const uint32_t clear = 0xFF101010u;
    pb_begin();
    pb_surface(surf, 64, 64, 4);
    pb_clear(clear);
    pb_texture_on(TEX_C2, 0x11, 8, 8, 16, filter);
    pb_vertex_format(vb, 1);
    put_vertex(vb, 0, 8.0f, 8.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(vb, 1, 56.0f, 8.0f, 0xFFFFFFFFu, 8.0f, 0.0f);
    put_vertex(vb, 2, 8.0f, 56.0f, 0xFFFFFFFFu, 0.0f, 8.0f);
    pb_draw_triangles(3);
    pb_flip();
    g_scene[idx].name = name;
    g_scene[idx].nsurf = 1;
    g_scene[idx].surf[0] = (Surf){ surf, 64, 64, 4, clear, 48 + 48 + 48 * 1.41421356, 0.5 * 48 * 48 };
}
static void build_case2(int flip)
{
    (void)flip;
    build_case2_with(1, SURF_C2, VB_C2, FILTER_NEAREST,
                     "2 textured triangle, magnified, filter NEAREST (R5G6B5 8x8, white diffuse)");
}
static void build_case2c(int flip)
{
    (void)flip;
    build_case2_with(5, SURF_C6, VB_C6, FILTER_LINEAR,
                     "2c same texture, filter LINEAR (coverage + smoothness only)");
}

/* Case 2b: the same idea at one texel per pixel, so the comparison does not depend on
 * how a magnified texture is filtered (the CPU path is nearest, see case 2). */
static void build_case2b(int flip)
{
    const uint32_t clear = 0xFF101010u;
    pb_begin();
    pb_surface(SURF_C5, 64, 64, 4);
    pb_clear(clear);
    pb_texture_on(TEX_C5, 0x11, 16, 16, 32, FILTER_NEAREST);
    pb_vertex_format(VB_C5, 1);
    put_vertex(VB_C5, 0, 8.0f, 8.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(VB_C5, 1, 24.0f, 8.0f, 0xFFFFFFFFu, 16.0f, 0.0f);
    put_vertex(VB_C5, 2, 8.0f, 24.0f, 0xFFFFFFFFu, 0.0f, 16.0f);
    pb_draw_triangles(3);
    if (flip) pb_flip();
    g_scene[4].name = "2b textured triangle at 1 texel per pixel (R5G6B5 16x16)";
    g_scene[4].nsurf = 1;
    g_scene[4].surf[0] = (Surf){ SURF_C5, 64, 64, 4, clear, 16 + 16 + 16 * 1.41421356, 0.5 * 16 * 16 };
}

static void build_case3(int flip)
{
    const uint32_t clearA = 0xFF0000FFu, clearB = 0xFF303030u;
    pb_begin();
    /* Pass 1: draw a red triangle into A. */
    pb_surface(SURF_C3A, 32, 32, 4);
    pb_clear(clearA);
    pb_texture_off();
    pb_vertex_format(VB_C3A, 0);
    put_vertex(VB_C3A, 0, 4.0f, 4.0f, 0xFFFF2020u, 0, 0);
    put_vertex(VB_C3A, 1, 28.0f, 6.0f, 0xFFFF2020u, 0, 0);
    put_vertex(VB_C3A, 2, 8.0f, 28.0f, 0xFFFF2020u, 0, 0);
    pb_draw_triangles(3);
    /* Pass 2: retarget to B and draw a quad that samples A as a 32x32 texture. */
    pb_surface(SURF_C3B, 64, 64, 4);
    pb_clear(clearB);
    pb_texture_on(SURF_C3A, 0x12, 32, 32, 32 * 4, FILTER_NEAREST);
    pb_vertex_format(VB_C3B, 1);
    put_vertex(VB_C3B, 0, 16.0f, 16.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(VB_C3B, 1, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(VB_C3B, 2, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    put_vertex(VB_C3B, 3, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(VB_C3B, 4, 48.0f, 48.0f, 0xFFFFFFFFu, 32.0f, 32.0f);
    put_vertex(VB_C3B, 5, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    pb_draw_triangles(6);
    if (flip) pb_flip();
    g_scene[2].name = "3 render-to-texture (A drawn, B samples A)";
    g_scene[2].nsurf = 2;
    g_scene[2].surf[0] = (Surf){ SURF_C3B, 64, 64, 4, clearB, 4 * 32.0, 32.0 * 32.0 };
    g_scene[2].surf[1] = (Surf){ SURF_C3A, 32, 32, 4, clearA,
        hypot(24, 2) + hypot(16, 22) + hypot(4, 24),
        fabs(0.5 * (24.0 * 24.0 - 2.0 * 4.0)) };
}

static void write_guest_data(void)
{
    uint32_t x, y;
    /* 8x8 R5G6B5: every texel distinct and far from the clear colour. */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            *(uint16_t *)(G(TEX_C2) + y * 16 + x * 2) =
                pack565(60 + x * 24, 60 + y * 24, 220 - x * 8 - y * 8);
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            *(uint16_t *)(G(TEX_C5) + y * 32 + x * 2) =
                pack565(60 + x * 12, 60 + y * 12, 220 - x * 6 - y * 6);
}

/* ── Running ─────────────────────────────────────────────────────────────── */
static NV2AState *g_gpu;

static int submit(const char *what)
{
    Nv2aPbExecCounters b, a;
    int ok;
    memset(&b, 0, sizeof b); memset(&a, 0, sizeof a);
    nv2a_pb_exec_counters(&b);
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = g_pbn * 4u;
    ok = nv2a_submit_pending(g_gpu);
    nv2a_pb_exec_counters(&a);
    printf("  %s: accepted=%d draws+%u clears+%u tris+%u flips_stall+%u unhandled+%u\n",
           what, ok, a.draws - b.draws, a.clears - b.clears, a.tris_drawn - b.tris_drawn,
           a.flip_stalls - b.flip_stalls, a.unhandled - b.unhandled);
    if (!ok) fprintf(stderr, "FAIL: pushbuffer rejected (%s)\n", what);
    return ok;
}

static void prefill(const Scene *sc)
{
    int i;
    for (i = 0; i < sc->nsurf; i++)
        memset(G(sc->surf[i].va), SENTINEL, SURF_BYTES);
}

static uint8_t *snapshot(const Surf *s)
{
    size_t n = (size_t)s->w * s->h * s->bpp;
    uint8_t *p = (uint8_t *)malloc(n);
    if (p) memcpy(p, G(s->va), n);
    return p;
}

/* Unpack one pixel to A,R,G,B bytes. */
static void unpack(const uint8_t *px, uint32_t bpp, int c[4])
{
    if (bpp == 2) {
        uint16_t v = *(const uint16_t *)px;
        uint32_t r = v >> 11, g = (v >> 5) & 63, b = v & 31;
        c[0] = 255;
        c[1] = (int)((r << 3) | (r >> 2));
        c[2] = (int)((g << 2) | (g >> 4));
        c[3] = (int)((b << 3) | (b >> 2));
    } else {
        uint32_t v = *(const uint32_t *)px;
        c[0] = (int)(v >> 24); c[1] = (int)((v >> 16) & 255);
        c[2] = (int)((v >> 8) & 255); c[3] = (int)(v & 255);
    }
}

static int maxdiff(const int a[4], const int b[4], int with_alpha)
{
    int i, m = 0;
    for (i = with_alpha ? 0 : 1; i < 4; i++) {
        int d = abs(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

typedef struct {
    double all_pct, region_bad, region_allowed;
    unsigned cov_ref, cov_got, cov_diff, region;
    int worst;
} Cmp;

static Cmp compare(const Surf *s, const uint8_t *ref, const uint8_t *got)
{
    Cmp r;
    int tol = s->bpp == 2 ? TOL_565 : TOL_8888;
    int clr[4], x, y;
    unsigned good = 0, bad_region = 0;
    uint8_t cpx[4];

    memset(&r, 0, sizeof r);
    /* The clear colour as the surface stores it, expanded the same way. */
    if (s->bpp == 2) {
        uint16_t v = pack565((s->clear >> 16) & 255, (s->clear >> 8) & 255, s->clear & 255);
        memcpy(cpx, &v, 2);
    } else {
        memcpy(cpx, &s->clear, 4);
    }
    unpack(cpx, s->bpp, clr);

    for (y = 0; y < (int)s->h; y++)
        for (x = 0; x < (int)s->w; x++) {
            size_t o = ((size_t)y * s->w + (size_t)x) * s->bpp;
            int a[4], b[4], d, ca, cb;
            unpack(ref + o, s->bpp, a);
            unpack(got + o, s->bpp, b);
            d = maxdiff(a, b, s->bpp == 4);
            ca = maxdiff(a, clr, 0) > COVER_TOL;
            cb = maxdiff(b, clr, 0) > COVER_TOL;
            r.cov_ref += (unsigned)ca;
            r.cov_got += (unsigned)cb;
            r.cov_diff += (unsigned)(ca != cb);
            if (d > r.worst) r.worst = d;
            if (d <= tol) good++;
            if (ca || cb) {
                r.region++;
                if (d > tol) bad_region++;
            }
        }
    r.all_pct = 100.0 * good / ((double)s->w * s->h);
    r.region_bad = bad_region;
    r.region_allowed = s->perim + 0.02 * r.region;
    return r;
}

static int check_parity(const char *label, const Surf *s, const uint8_t *ref, const uint8_t *got,
                        int expect_match)
{
    Cmp c = compare(s, ref, got);
    int ok = 1;
    double area_lo = s->area * 0.9, area_hi = s->area * 1.1;

    printf("  [%s] surface 0x%08X %ux%u %ubpp: match %.2f%% of all pixels, worst channel diff %d,\n"
           "      coverage ref=%u got=%u (xor %u, allowed %.0f), drawn-region mismatches %.0f of %u (allowed %.0f)\n",
           label, s->va, s->w, s->h, s->bpp * 8, c.all_pct, c.worst, c.cov_ref, c.cov_got,
           c.cov_diff, s->perim, c.region_bad, c.region, c.region_allowed);

    if (expect_match && c.all_pct < 100.0) {
        /* First few disagreements, so a failure names pixels rather than percentages. */
        int x, y, shown = 0, tol = s->bpp == 2 ? TOL_565 : TOL_8888;
        for (y = 0; y < (int)s->h && shown < 8; y++)
            for (x = 0; x < (int)s->w && shown < 8; x++) {
                size_t o = ((size_t)y * s->w + (size_t)x) * s->bpp;
                int a[4], b[4];
                unpack(ref + o, s->bpp, a);
                unpack(got + o, s->bpp, b);
                if (maxdiff(a, b, s->bpp == 4) > tol) {
                    printf("      (%d,%d) ref ARGB %d,%d,%d,%d  got %d,%d,%d,%d\n", x, y,
                           a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
                    ++shown;
                }
            }
    }
    if (!expect_match) {
        if (c.all_pct >= 50.0) {
            fprintf(stderr, "FAIL [%s]: expected the surfaces to DIFFER, but %.2f%% match\n",
                    label, c.all_pct);
            ok = 0;
        }
        return ok;
    }
    /* The reference must itself show the drawing, or the test proves nothing. */
    if (c.cov_ref < area_lo || c.cov_ref > area_hi) {
        fprintf(stderr, "FAIL [%s]: CPU reference covers %u px, geometry predicts %.0f..%.0f\n",
                label, c.cov_ref, area_lo, area_hi);
        ok = 0;
    }
    if (c.all_pct < 98.0) {
        fprintf(stderr, "FAIL [%s]: only %.2f%% of pixels agree (need 98%%)\n", label, c.all_pct);
        ok = 0;
    }
    if (c.region_bad > c.region_allowed) {
        fprintf(stderr, "FAIL [%s]: %.0f drawn-region pixels disagree (allowed %.0f)\n",
                label, c.region_bad, c.region_allowed);
        ok = 0;
    }
    if (c.cov_diff > s->perim) {
        fprintf(stderr, "FAIL [%s]: coverage differs by %u px (edge band allows %.0f)\n",
                label, c.cov_diff, s->perim);
        ok = 0;
    }
    return ok;
}

/* Case 2c: the CPU screen-space path point-samples whatever the filter says, so colours are
 * not compared. Coverage must still agree, and at every texel boundary along one row the GPU
 * value must lie between the two neighbouring texel colours and not be a hard step. */
static int check_linear(const Surf *s, const uint8_t *ref, const uint8_t *got)
{
    Cmp c = compare(s, ref, got);
    int ok = 1, x, y = 12, boundaries = 0, smooth = 0, outside = 0;

    for (x = 10; x < 44; x++) {
        size_t o0 = ((size_t)y * s->w + (size_t)x) * s->bpp, o1 = o0 + s->bpp;
        int r0[4], r1[4], g0[4], g1[4], lo, hi;
        unpack(ref + o0, s->bpp, r0); unpack(ref + o1, s->bpp, r1);
        unpack(got + o0, s->bpp, g0); unpack(got + o1, s->bpp, g1);
        if (r0[1] == r1[1]) continue;
        ++boundaries;
        lo = r0[1] < r1[1] ? r0[1] : r1[1];
        hi = r0[1] < r1[1] ? r1[1] : r0[1];
        if (g0[1] < lo - 2 || g0[1] > hi + 2 || g1[1] < lo - 2 || g1[1] > hi + 2) ++outside;
        if (g0[1] != r0[1] || g1[1] != r1[1]) ++smooth;
    }
    printf("  [case2c/target] colour match %.2f%% (information only; CPU is nearest), coverage ref=%u got=%u (xor %u, allowed %.0f),\n"
           "      row %d: %d texel boundaries, %d blended, %d outside the neighbouring texel colours\n",
           c.all_pct, c.cov_ref, c.cov_got, c.cov_diff, s->perim, y, boundaries, smooth, outside);
    if (c.cov_ref < s->area * 0.9 || c.cov_ref > s->area * 1.1) {
        fprintf(stderr, "FAIL [case2c]: CPU reference covers %u px, geometry predicts %.0f\n", c.cov_ref, s->area);
        ok = 0;
    }
    if (c.cov_diff > s->perim) {
        fprintf(stderr, "FAIL [case2c]: coverage differs by %u px (allowed %.0f)\n", c.cov_diff, s->perim);
        ok = 0;
    }
    if (boundaries < 3 || outside || smooth < boundaries) {
        fprintf(stderr, "FAIL [case2c]: GPU output is not smooth across texel boundaries\n");
        ok = 0;
    }
    return ok;
}

typedef void (*build_fn)(int flip);
static const build_fn k_build[6] = { build_case1, build_case2, build_case3, build_case4, build_case2b, build_case2c };
/* Cases the back end runs with a flip and compares; case 3 (index 3) is the no-flip control. */
static const int k_parity[5] = { 0, 1, 4, 2, 5 };

int main(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *ref[6][2], *got[6][2];
    uint8_t *pbmem = (uint8_t *)VirtualAlloc(NULL, 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    int ok = 1, c, i;

    memset(ref, 0, sizeof ref); memset(got, 0, sizeof got);
    if (!ram || !ramin || !instance || !pbmem) { fprintf(stderr, "FAIL: allocation\n"); return 1; }
    g_pb = (uint32_t *)pbmem;

    /* Guest RAM: a synthetic, section-less XBE is enough for the memory layout. */
    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) {
        fprintf(stderr, "FAIL: xbox_MemoryLayoutInit\n");
        return 1;
    }

    nv2a_reset_standalone_for_test();
    g_gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!g_gpu) return 1;
    if (!nv2a_set_pushbuffer_window(g_gpu, pbmem, 0, 0x4000)) return 1;
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) return 1;
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u; pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u; object[1] = object[2] = object[3] = 0;
    }
    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) {
        fprintf(stderr, "FAIL: executor not registered\n");
        return 1;
    }
    nv2a_backend_register(NULL);
    write_guest_data();

    /* (a) CPU reference for every case. Case 4's reference is the same scene as
     * case 1 at its own address. */
    puts("-- software executor --");
    for (c = 0; c < 6; c++) {
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("software");
        for (i = 0; i < g_scene[c].nsurf; i++) ref[c][i] = snapshot(&g_scene[c].surf[i]);
    }

    /* (b) D3D11 back end. */
    _putenv_s("RECOMP_GPU_WARP", "1");     /* deterministic adapter; the back end logs "[GPUBE] device:" on stderr */
    puts("-- RECOMP_GPU_WARP=1 (adapter line: see '[GPUBE] device:' below) --");
    fflush(stdout);
    if (nv2a_d3d11_backend_install() != 0) {
        puts("SKIP: gpu_backend_parity -- no Direct3D 11 device (hardware or WARP) could be created");
        return 0;
    }
    puts("-- D3D11 back end --");
    for (i = 0; i < 5; i++) {
        c = k_parity[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("d3d11 (flipped)");
        {
            int j;
            for (j = 0; j < g_scene[c].nsurf; j++) got[c][j] = snapshot(&g_scene[c].surf[j]);
        }
    }

    puts("-- parity --");
    {
        static const char *tag[6] = { "case1", "case2", "case3", "case4", "case2b", "case2c" };
        static const char *tag3[5] = { "case1", "case2", "case3", "case4", "case2b" };
        int k;
        (void)tag3;
        for (k = 0; k < 5; k++) {
            c = k_parity[k];
            if (c == 5) { ok &= check_linear(&g_scene[5].surf[0], ref[5][0], got[5][0]); continue; }
            for (i = 0; i < g_scene[c].nsurf; i++) {
                char label[32];
                _snprintf(label, sizeof label, "%s/%s", tag[c], i == 0 ? "target" : "source");
                if (!ref[c][i] || !got[c][i]) { ok = 0; continue; }
                ok &= check_parity(label, &g_scene[c].surf[i], ref[c][i], got[c][i], 1);
            }
        }
    }

    /* Case 4, negative control: the back-end pass WITHOUT the flip must leave the
     * guest surface untouched (so it differs from the CPU result); the readback
     * happens at the flip, which is then issued alone and must close the gap. */
    puts("-- negative control (no flip) --");
    build_case4(0);
    prefill(&g_scene[3]);
    printf(" case %s\n", g_scene[3].name);
    ok &= submit("d3d11 (no flip)");
    got[3][0] = snapshot(&g_scene[3].surf[0]);
    if (ref[3][0] && got[3][0])
        ok &= check_parity("case4/before-flip", &g_scene[3].surf[0], ref[3][0], got[3][0], 0);
    pb_begin();
    pb_flip();
    ok &= submit("d3d11 (flip only)");
    {
        uint8_t *after = snapshot(&g_scene[3].surf[0]);
        if (ref[3][0] && after)
            ok &= check_parity("case4/after-flip", &g_scene[3].surf[0], ref[3][0], after, 1);
        free(after);
    }

    nv2a_backend_register(NULL);
    nv2a_set_commit_consumer(NULL);
    for (c = 0; c < 6; c++)
        for (i = 0; i < 2; i++) { free(ref[c][i]); free(got[c][i]); }
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fprintf(stderr, "FAIL: gpu_backend_parity\n"); return 1; }
    puts("PASS: gpu_backend_parity (software and D3D11 agree after the flip)");
    return 0;
}
