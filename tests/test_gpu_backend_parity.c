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
 * shades, so every case uses one diffuse colour for all vertices.
 *   - The DXT cases (D1..D4) set a per-surface tolerance of 3 instead (TOL_DXT): the only
 *     difference left there is how a BC unit and the CPU decoder round the 565 endpoint
 *     expansion and the 1/3, 1/2, n/7 and n/5 interpolations, a couple of LSBs. */
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
#define SURF_C7   0x00410000u     /* case 5: 64x64 A8R8G8B8          */
#define SURF_C8   0x00420000u     /* case 6                          */
#define SURF_C9   0x00430000u     /* case 7                          */
#define SURF_C10  0x00450000u     /* case 8                          */
#define VB_C7     0x00690000u
#define VB_C8     0x006A0000u
#define VB_C9     0x006B0000u
#define VB_C10    0x006C0000u
#define TEX_C9    0x006D0000u     /* case 7: stage 1, 8x8 R5G6B5 linear */
#define SURF_D1   0x00510000u     /* DXT case 1: 64x64 A8R8G8B8      */
#define SURF_D2   0x00520000u     /* DXT case 2                      */
#define SURF_D3   0x00530000u     /* DXT case 3                      */
#define SURF_D4   0x00540000u     /* DXT case 4                      */
#define VB_D1     0x00710000u
#define VB_D2     0x00720000u
#define VB_D3     0x00730000u
#define VB_D4     0x00740000u
#define TEX_D1    0x00750000u     /* 16x16 DXT1, opaque              */
#define TEX_D2    0x00760000u     /* 16x16 DXT1, 3-colour blocks     */
#define TEX_D3    0x00770000u     /* 16x16 DXT5                      */
#define TEX_D4    0x00780000u     /* 8x6 DXT1                        */
#define FILTER_NEAREST 0x01010000u   /* SET_TEXTURE_FILTER: MIN=1 (bits 16-23), MAG=1 (24-27) */
#define FILTER_LINEAR  0x02020000u   /* MIN=2, MAG=2 */
#define SURF_BYTES 0x00010000u    /* span pre-filled/cleared per surface */
#define SENTINEL  0x5Au

#define TOL_8888 8
#define TOL_565  9
#define TOL_DXT  3      /* DXT cases: BC decode vs CPU decode rounding, per 8-bit channel */
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
static void pb_texture_stage(uint32_t st, uint32_t va, uint32_t fmt, uint32_t w, uint32_t h,
                             uint32_t pitch, uint32_t filter)
{
    uint32_t b = 0x1B00u + st * 0x40u;
    pb(b + 0x14, filter);                          /* SET_TEXTURE_FILTER */
    pb(b + 0x00, va);
    pb(b + 0x04, 0x1u | (2u << 4) | (fmt << 8) | (1u << 16));
    pb(b + 0x08, 0x0303u);                         /* clamp U and V */
    pb(b + 0x10, pitch << 16);                     /* CONTROL1: pitch */
    pb(b + 0x1C, (w << 16) | h);                   /* IMAGE_RECT */
    pb(b + 0x0C, 1u << 30);                        /* CONTROL0: ENABLE */
}
/* Stages 1-3 accept only the methods in the toolkit's NV097 whitelist, which omits their CONTROL1
 * (pitch) and IMAGE_RECT; so a second texture has to be swizzled, sized by log2 in the format word. */
static void pb_texture_swz(uint32_t st, uint32_t va, uint32_t fmt, uint32_t lw, uint32_t lh,
                           uint32_t filter)
{
    uint32_t b = 0x1B00u + st * 0x40u;
    pb(b + 0x14, filter);
    pb(b + 0x00, va);
    pb(b + 0x04, 0x1u | (2u << 4) | (fmt << 8) | (1u << 16) | (lw << 20) | (lh << 24));
    pb(b + 0x08, 0x0303u);
    pb(b + 0x0C, 1u << 30);
}
static void pb_texture_on(uint32_t va, uint32_t fmt, uint32_t w, uint32_t h, uint32_t pitch,
                          uint32_t filter)
{
    pb_texture_stage(0, va, fmt, w, h, pitch, filter);
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
/* tol: per-channel tolerance; 0 means the default for the surface's bpp. */
typedef struct { uint32_t va, w, h, bpp, clear; double perim, area; int tol; } Surf;
typedef struct {
    const char *name;
    int nsurf;
    Surf surf[2];
} Scene;

#define NCASES 14       /* 0..9 as before, 10..13 the DXT cases */
static Scene g_scene[NCASES];

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


/* ── Register-combiner cases (5..8) ──────────────────────────────────────────
 * The CPU executor evaluates combiners only on its vertex-program path
 * (raster_xf_triangle's use_rc); its fixed-function path never does. So these
 * cases run in program mode with a five-instruction pass-through vertex program
 * (positions are already screen space, as the executor expects), which makes
 * both sides evaluate nv2a_rc_eval's arithmetic. Linear textures are addressed
 * in texels, as in case 2. */
#define RC_STRIDE 36u    /* pos@0 diffuse@12 specular@16 uv0@20 uv1@28 */

/* mov o[addr], v[in] (MAC op 1, all four components); `final` ends the program. */
static void vsh_mov(int slot, uint32_t addr, uint32_t in, int final)
{
    uint32_t w[4] = { 0, 0, 0, 0 }, k;
    w[1] = (1u << 21) | (in << 9) | 0x1Bu;          /* mov, input index, swizzle xyzw */
    w[2] = (2u << 26);                               /* A source mux: input */
    w[3] = (0xFu << 12) | (1u << 11) | (addr << 3) | (final ? 1u : 0u);
    (void)slot;
    for (k = 0; k < 4; k++) pb(0x0B00u + 4u * k, w[k]);
}

static void pb_vertex_format_rc(uint32_t vb)
{
    const uint32_t col = 0u | (4u << 4) | (RC_STRIDE << 8);
    const uint32_t f2 = 2u | (2u << 4) | (RC_STRIDE << 8);
    pb(0x1720 + 0 * 4, vb);        pb(0x1760 + 0 * 4, 2u | (3u << 4) | (RC_STRIDE << 8));
    pb(0x1720 + 3 * 4, vb + 12);   pb(0x1760 + 3 * 4, col);
    pb(0x1720 + 4 * 4, vb + 16);   pb(0x1760 + 4 * 4, col);
    pb(0x1720 + 9 * 4, vb + 20);   pb(0x1760 + 9 * 4, f2);
    pb(0x1720 + 10 * 4, vb + 28);  pb(0x1760 + 10 * 4, f2);
}

/* oPos<-v0, oD0<-v3, oD1<-v4, oT0<-v9, oT1<-v10. */
static void pb_vertex_program_rc(void)
{
    pb(0x1E94, 2u);                                  /* TRANSFORM_EXECUTION_MODE: program */
    pb(0x1E9C, 0u);                                  /* PROGRAM_LOAD slot 0 */
    pb(0x1EA0, 0u);                                  /* PROGRAM_START slot 0 */
    vsh_mov(0, 0, 0, 0);
    vsh_mov(1, 3, 3, 0);
    vsh_mov(2, 4, 4, 0);
    vsh_mov(3, 9, 9, 0);
    vsh_mov(4, 10, 10, 1);
}

static void put_vertex_rc(uint32_t vb, int i, float x, float y, uint32_t diffuse, uint32_t spec,
                          float u0, float v0, float u1, float v1)
{
    uint8_t *p = G(vb + (uint32_t)i * RC_STRIDE);
    float *f = (float *)p;
    f[0] = x; f[1] = y; f[2] = 0.5f;
    *(uint32_t *)(p + 12) = diffuse;
    *(uint32_t *)(p + 16) = spec;
    f[5] = u0; f[6] = v0; f[7] = u1; f[8] = v1;
}

/* Combiner register bytes: reg | alpha<<4. */
#define RC_ZERO 0x00u
#define RC_C0   0x01u
#define RC_V0   0x04u
#define RC_V1   0x05u
#define RC_T0   0x08u
#define RC_T1   0x09u
#define RC_R0   0x0Cu
#define RC_SUM  0x0Eu
#define RC_A(r) ((r) | 0x10u)

/* One general stage: r0 = a * b (RGB) and a.alpha * b.alpha (alpha). */
static void pb_rc_stage_product(uint32_t a, uint32_t b)
{
    pb(0x0AC0, (a << 24) | (b << 16));               /* COLOR_ICW(0) */
    pb(0x0260, (RC_A(a) << 24) | (RC_A(b) << 16));   /* ALPHA_ICW(0) */
    pb(0x1E40, RC_R0 << 4);                          /* COLOR_OCW(0): AB -> r0 */
    pb(0x0AA0, RC_R0 << 4);                          /* ALPHA_OCW(0) */
}

/* Common scene: the case-2 triangle (stage 0 in texels); stage 1 is swizzled so its uv is
 * normalised, and transposed to differ from stage 0. */
static void build_rc_scene(int idx, uint32_t surf, uint32_t vb, uint32_t diffuse, uint32_t spec,
                           const char *name)
{
    const uint32_t clear = 0xFF101010u;
    put_vertex_rc(vb, 0, 8.0f, 8.0f, diffuse, spec, 0.0f, 0.0f, 0.0f, 0.0f);
    put_vertex_rc(vb, 1, 56.0f, 8.0f, diffuse, spec, 8.0f, 0.0f, 0.0f, 1.0f);
    put_vertex_rc(vb, 2, 8.0f, 56.0f, diffuse, spec, 0.0f, 8.0f, 1.0f, 0.0f);
    pb_draw_triangles(3);
    pb_flip();
    g_scene[idx].name = name;
    g_scene[idx].nsurf = 1;
    g_scene[idx].surf[0] = (Surf){ surf, 64, 64, 4, clear, 48 + 48 + 48 * 1.41421356, 0.5 * 48 * 48 };
}

static void rc_begin(uint32_t surf, uint32_t vb)
{
    pb_begin();
    pb_surface(surf, 64, 64, 4);
    pb_clear(0xFF101010u);
    pb_texture_on(TEX_C2, 0x11, 8, 8, 16, FILTER_NEAREST);
    pb(0x1B4C, 0);                                   /* stage 1 off unless a case enables it */
    pb_vertex_format_rc(vb);
    pb_vertex_program_rc();
}

/* final = D + mix(C, B, A); with A=B=C=0 it passes D through. Alpha from G. */
static void pb_rc_final(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t g)
{
    pb(0x0288, (a << 24) | (b << 16) | (c << 8) | d);   /* SPECULAR_FOG_CW0 */
    pb(0x028C, g << 8);                                 /* SPECULAR_FOG_CW1 */
}

/* 5: one stage, r0 = tex0 * diffuse; the final combiner passes r0 through. */
static void build_case5(int flip)
{
    (void)flip;
    rc_begin(SURF_C7, VB_C7);
    pb(0x1E70, 1u);                                  /* SHADER_STAGE_PROGRAM: stage 0 = 2D */
    pb(0x1E60, 1u);                                  /* COMBINER_CONTROL: 1 stage */
    pb_rc_stage_product(RC_T0, RC_V0);
    pb_rc_final(RC_ZERO, RC_ZERO, RC_ZERO, RC_R0, RC_A(RC_R0));
    build_rc_scene(6, SURF_C7, VB_C7, 0xFFFFFFFFu, 0, "5 combiner: tex0 * diffuse (white), final passthrough");
}

/* 6: lerp through the final combiner, c0 = 0x80808080: c0*tex0 + (1-c0)*diffuse. */
static void build_case6(int flip)
{
    (void)flip;
    rc_begin(SURF_C8, VB_C8);
    pb(0x1E70, 1u);
    pb(0x1E60, 1u);
    pb(0x0A60, 0x80808080u);                         /* FACTOR0(0) */
    pb(0x0A80, 0x80808080u);                         /* FACTOR1(0) */
    pb(0x1E20, 0x80808080u);                         /* SPECULAR_FOG_FACTOR c0: the final combiner's c0 */
    pb(0x1E24, 0x80808080u);
    pb_rc_final(RC_C0, RC_T0, RC_V0, RC_ZERO, RC_A(RC_V0));
    build_rc_scene(7, SURF_C8, VB_C8, 0xFF2080E0u, 0, "6 combiner: lerp c0=0x80808080 of tex0 and diffuse (final A*B+(1-A)*C)");
}

/* 7: two textures, r0 = tex0 * tex1 (tex1 sampled with transposed coordinates). */
static void build_case7(int flip)
{
    (void)flip;
    rc_begin(SURF_C9, VB_C9);
    pb_texture_swz(1, TEX_C9, 0x05, 3, 3, FILTER_NEAREST);   /* SZ_R5G6B5 8x8 */
    pb(0x1E70, 1u | (1u << 5));                      /* stages 0 and 1 = 2D */
    pb(0x1E60, 1u);
    pb_rc_stage_product(RC_T0, RC_T1);
    pb_rc_final(RC_ZERO, RC_ZERO, RC_ZERO, RC_R0, RC_A(RC_R0));
    build_rc_scene(8, SURF_C9, VB_C9, 0xFFFFFFFFu, 0, "7 combiner: two textures, tex0 * tex1");
}

/* 8: final-combiner specular add: out = (tex0 * diffuse) + v1 through the V1+R0 sum. */
static void build_case8(int flip)
{
    (void)flip;
    rc_begin(SURF_C10, VB_C10);
    pb(0x1E70, 1u);
    pb(0x1E60, 1u);
    pb_rc_stage_product(RC_T0, RC_V0);
    pb_rc_final(RC_ZERO, RC_ZERO, RC_ZERO, RC_SUM, RC_A(RC_V0));
    build_rc_scene(9, SURF_C10, VB_C10, 0xFFC8C8C8u, 0xFF402010u, "8 combiner: specular add, (tex0 * diffuse) + v1 via final sum");
}


/* ── DXT cases (10..13) ──────────────────────────────────────────────────────
 * DXT1/DXT3/DXT5 (formats 0x0C/0x0E/0x0F) are linear 4x4 blocks in row-major block order. A
 * block-compressed texture takes its size from log2 in the format word and is sampled with
 * normalised coordinates (like a swizzled one), and has no pitch. Each case draws the whole
 * texture 1:1 as a quad (two triangles) at (8,8) on a 64x64 A8R8G8B8 surface with point
 * filtering and white diffuse, so every pixel is one texel. The quad is two triangles, so the
 * diagonal runs through pixel centres; the 64x64 surface keeps the 98% bar clear of it.
 *
 * Every texel of every texture is far from the clear colour (checked when the blocks were
 * written: each opaque palette entry differs from 0x101010 by more than COVER_TOL in some
 * channel), so coverage is exactly the opaque texels. Blocks were checked against both the CPU
 * decoder and an independent BC1/BC3 reference (rounded interpolation): they agree within 2
 * per channel, which TOL_DXT covers with a LSB to spare for the GPU.
 *
 * Texel (x,y) of a block is at bit 2*(4y+x) of the 32-bit colour index word (DXT1/DXT5 colour
 * half), and bit 3*(4y+x) of the 48-bit DXT5 alpha index word; both little-endian after the
 * two endpoints. c0/c1 are R5G6B5 little-endian. */
/* 16x16 DXT1, 4x4 blocks row-major, every block opaque (c0 > c1). */
static const uint8_t k_dxt1_opaque_16x16[] = {
    /* block (0,0): c0=#F7C321 (0xF604) > c1=#DE7DA5 (0xDBF4), 4-colour; texel(x,y) index=(x+2y+0)&3, all four entries used */
    0x04, 0xF6, 0xF4, 0xDB, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,0): c0=#D634C6 (0xD1B8) > c1=#217DBD (0x23F7), 4-colour; texel(x,y) index=(x+2y+1)&3, all four entries used */
    0xB8, 0xD1, 0xF7, 0x23, 0x39, 0x93, 0x39, 0x93,
    /* block (2,0): c0=#290873 (0x284E) > c1=#08D3F7 (0x0E9E), 4-colour; texel(x,y) index=(x+2y+2)&3, all four entries used */
    0x4E, 0x28, 0x9E, 0x0E, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,0): c0=#F7C3A5 (0xF614) > c1=#EF457B (0xEA2F), 4-colour; texel(x,y) index=(x+2y+3)&3, all four entries used */
    0x14, 0xF6, 0x2F, 0xEA, 0x93, 0x39, 0x93, 0x39,
    /* block (0,1): c0=#B5456B (0xB22D) > c1=#42BEB5 (0x45F6), 4-colour; texel(x,y) index=(x+2y+4)&3, all four entries used */
    0x2D, 0xB2, 0xF6, 0x45, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,1): c0=#F75D5A (0xF2EB) > c1=#3141C6 (0x3218), 4-colour; texel(x,y) index=(x+2y+5)&3, all four entries used */
    0xEB, 0xF2, 0x18, 0x32, 0x39, 0x93, 0x39, 0x93,
    /* block (2,1): c0=#AD7973 (0xABCE) > c1=#3169D6 (0x335A), 4-colour; texel(x,y) index=(x+2y+6)&3, all four entries used */
    0xCE, 0xAB, 0x5A, 0x33, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,1): c0=#AD04E7 (0xA83C) > c1=#297931 (0x2BC6), 4-colour; texel(x,y) index=(x+2y+7)&3, all four entries used */
    0x3C, 0xA8, 0xC6, 0x2B, 0x93, 0x39, 0x93, 0x39,
    /* block (0,2): c0=#E78ADE (0xE45B) > c1=#29306B (0x298D), 4-colour; texel(x,y) index=(x+2y+8)&3, all four entries used */
    0x5B, 0xE4, 0x8D, 0x29, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,2): c0=#EFAE7B (0xED6F) > c1=#4AC784 (0x4E30), 4-colour; texel(x,y) index=(x+2y+9)&3, all four entries used */
    0x6F, 0xED, 0x30, 0x4E, 0x39, 0x93, 0x39, 0x93,
    /* block (2,2): c0=#BD71DE (0xBB9B) > c1=#AD2C42 (0xA968), 4-colour; texel(x,y) index=(x+2y+10)&3, all four entries used */
    0x9B, 0xBB, 0x68, 0xA9, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,2): c0=#F76542 (0xF328) > c1=#397DC6 (0x3BF8), 4-colour; texel(x,y) index=(x+2y+11)&3, all four entries used */
    0x28, 0xF3, 0xF8, 0x3B, 0x93, 0x39, 0x93, 0x39,
    /* block (0,3): c0=#9C3CDE (0x99FB) > c1=#08AA63 (0x0D4C), 4-colour; texel(x,y) index=(x+2y+12)&3, all four entries used */
    0xFB, 0x99, 0x4C, 0x0D, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,3): c0=#DEA27B (0xDD0F) > c1=#84CBE7 (0x865C), 4-colour; texel(x,y) index=(x+2y+13)&3, all four entries used */
    0x0F, 0xDD, 0x5C, 0x86, 0x39, 0x93, 0x39, 0x93,
    /* block (2,3): c0=#DEAE21 (0xDD64) > c1=#429AF7 (0x44DE), 4-colour; texel(x,y) index=(x+2y+14)&3, all four entries used */
    0x64, 0xDD, 0xDE, 0x44, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,3): c0=#E7107B (0xE08F) > c1=#4AC7EF (0x4E3D), 4-colour; texel(x,y) index=(x+2y+15)&3, all four entries used */
    0x8F, 0xE0, 0x3D, 0x4E, 0x93, 0x39, 0x93, 0x39,
};

/* 16x16 DXT1; blocks (1,0), (2,1), (3,2) use the 3-colour+transparent mode. */
static const uint8_t k_dxt1_alpha_16x16[] = {
    /* block (0,0): opaque, c0=#AD4121 (0xAA04) > c1=#AD3C84 (0xA9F0), 4-colour; index=(x+2y+0)&3 */
    0x04, 0xAA, 0xF0, 0xA9, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,0): c0=#9C96E7 (0x9CBC) < c1=#C6CF4A (0xC669), 3-colour mode, entry 2 = midpoint, index 3 transparent at (2,1) */
    0xBC, 0x9C, 0x69, 0xC6, 0x24, 0x39, 0x92, 0x24,
    /* block (2,0): opaque, c0=#AD517B (0xAA8F) > c1=#94B608 (0x95A1), 4-colour; index=(x+2y+2)&3 */
    0x8F, 0xAA, 0xA1, 0x95, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,0): opaque, c0=#E769AD (0xE355) > c1=#52967B (0x54AF), 4-colour; index=(x+2y+3)&3 */
    0x55, 0xE3, 0xAF, 0x54, 0x93, 0x39, 0x93, 0x39,
    /* block (0,1): opaque, c0=#7B34EF (0x79BD) > c1=#008208 (0x0401), 4-colour; index=(x+2y+4)&3 */
    0xBD, 0x79, 0x01, 0x04, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,1): opaque, c0=#FF8E6B (0xFC6D) > c1=#94AECE (0x9579), 4-colour; index=(x+2y+5)&3 */
    0x6D, 0xFC, 0x79, 0x95, 0x39, 0x93, 0x39, 0x93,
    /* block (2,1): c0=c1=#18F773 (0x1FAE), 3-colour mode (c0 <= c1); entries 0,1,2 are all that colour, index 3 transparent at texels (0,0),(3,3) */
    0xAE, 0x1F, 0xAE, 0x1F, 0x93, 0x24, 0x49, 0xD2,
    /* block (3,1): opaque, c0=#9CCBCE (0x9E59) > c1=#8C2063 (0x890C), 4-colour; index=(x+2y+7)&3 */
    0x59, 0x9E, 0x0C, 0x89, 0x93, 0x39, 0x93, 0x39,
    /* block (0,2): opaque, c0=#6B9ECE (0x6CF9) > c1=#5A5508 (0x5AA1), 4-colour; index=(x+2y+8)&3 */
    0xF9, 0x6C, 0xA1, 0x5A, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,2): opaque, c0=#FF75EF (0xFBBD) > c1=#948A63 (0x944C), 4-colour; index=(x+2y+9)&3 */
    0xBD, 0xFB, 0x4C, 0x94, 0x39, 0x93, 0x39, 0x93,
    /* block (2,2): opaque, c0=#ADE35A (0xAF0B) > c1=#7B5973 (0x7ACE), 4-colour; index=(x+2y+10)&3 */
    0x0B, 0xAF, 0xCE, 0x7A, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,2): c0=#5A4529 (0x5A25) < c1=#7BD321 (0x7E84), 3-colour mode, entry 2 = midpoint, index 3 transparent at (2,1),(3,1),(2,3),(3,3) */
    0x25, 0x5A, 0x84, 0x7E, 0x50, 0xFA, 0x50, 0xFA,
    /* block (0,3): opaque, c0=#F74DFF (0xF27F) > c1=#184DFF (0x1A7F), 4-colour; index=(x+2y+12)&3 */
    0x7F, 0xF2, 0x7F, 0x1A, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,3): opaque, c0=#F7C773 (0xF62E) > c1=#4A2C84 (0x4970), 4-colour; index=(x+2y+13)&3 */
    0x2E, 0xF6, 0x70, 0x49, 0x39, 0x93, 0x39, 0x93,
    /* block (2,3): opaque, c0=#42DB18 (0x46C3) > c1=#1828EF (0x195D), 4-colour; index=(x+2y+14)&3 */
    0xC3, 0x46, 0x5D, 0x19, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,3): opaque, c0=#D610A5 (0xD094) > c1=#941808 (0x90C1), 4-colour; index=(x+2y+15)&3 */
    0x94, 0xD0, 0xC1, 0x90, 0x93, 0x39, 0x93, 0x39,
};

/* 16x16 DXT5: 16-byte blocks, alpha half first. */
static const uint8_t k_dxt5_16x16[] = {
    /* block (0,0): a0=255 a1=20 8-alpha mode (a0 > a1), alpha index=(4y+x+0)&7; colour c0=#FF0410 (0xF822) c0 > c1 c1=#420C9C (0x4073), index=(x+2y+0)&3 */
    0xFF, 0x14, 0x88, 0xC6, 0xFA, 0x88, 0xC6, 0xFA, 0x22, 0xF8, 0x73, 0x40, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,0): a0=17 a1=203 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+1)&7; colour c0=#C641C6 (0xC218) c0 > c1 c1=#94714A (0x9389), index=(x+2y+1)&3 */
    0x11, 0xCB, 0xD1, 0x58, 0x1F, 0xD1, 0x58, 0x1F, 0x18, 0xC2, 0x89, 0x93, 0x39, 0x93, 0x39, 0x93,
    /* block (2,0): a0=239 a1=38 8-alpha mode (a0 > a1), alpha index=(4y+x+2)&7; colour c0=#DE284A (0xD949) c0 > c1 c1=#C69A39 (0xC4C7), index=(x+2y+2)&3 */
    0xEF, 0x26, 0x1A, 0xEB, 0x23, 0x1A, 0xEB, 0x23, 0x49, 0xD9, 0xC7, 0xC4, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,0): a0=31 a1=209 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+3)&7; colour c0=#BDB273 (0xBD8E) c0 > c1 c1=#31516B (0x328D), index=(x+2y+3)&3 */
    0x1F, 0xD1, 0x63, 0x7D, 0x44, 0x63, 0x7D, 0x44, 0x8E, 0xBD, 0x8D, 0x32, 0x93, 0x39, 0x93, 0x39,
    /* block (0,1): a0=223 a1=56 8-alpha mode (a0 > a1), alpha index=(4y+x+4)&7; colour c0=#52B2D6 (0x559A) c0 > c1 c1=#427518 (0x43A3), index=(x+2y+4)&3 */
    0xDF, 0x38, 0xAC, 0x8F, 0x68, 0xAC, 0x8F, 0x68, 0x9A, 0x55, 0xA3, 0x43, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,1): a0=45 a1=215 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+5)&7; colour c0=#2941E7 (0x2A1C) c0 < c1 (still 4-colour in DXT5) c1=#AD6594 (0xAB32), index=(x+2y+5)&3 */
    0x2D, 0xD7, 0xF5, 0x11, 0x8D, 0xF5, 0x11, 0x8D, 0x1C, 0x2A, 0x32, 0xAB, 0x39, 0x93, 0x39, 0x93,
    /* block (2,1): a0=207 a1=74 8-alpha mode (a0 > a1), alpha index=(4y+x+6)&7; colour c0=#D618E7 (0xD0DC) c0 > c1 c1=#528231 (0x5406), index=(x+2y+6)&3 */
    0xCF, 0x4A, 0x3E, 0xA2, 0xB1, 0x3E, 0xA2, 0xB1, 0xDC, 0xD0, 0x06, 0x54, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,1): a0=59 a1=221 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+7)&7; colour c0=#A5D3B5 (0xA696) c0 > c1 c1=#4230AD (0x4195), index=(x+2y+7)&3 */
    0x3B, 0xDD, 0x47, 0x34, 0xD6, 0x47, 0x34, 0xD6, 0x96, 0xA6, 0x95, 0x41, 0x93, 0x39, 0x93, 0x39,
    /* block (0,2): a0=191 a1=92 8-alpha mode (a0 > a1), alpha index=(4y+x+8)&7; colour c0=#B518F7 (0xB0DE) c0 > c1 c1=#42C36B (0x460D), index=(x+2y+8)&3 */
    0xBF, 0x5C, 0x88, 0xC6, 0xFA, 0x88, 0xC6, 0xFA, 0xDE, 0xB0, 0x0D, 0x46, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,2): a0=73 a1=227 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+9)&7; colour c0=#B5FB6B (0xB7CD) c0 > c1 c1=#9449EF (0x925D), index=(x+2y+9)&3 */
    0x49, 0xE3, 0xD1, 0x58, 0x1F, 0xD1, 0x58, 0x1F, 0xCD, 0xB7, 0x5D, 0x92, 0x39, 0x93, 0x39, 0x93,
    /* block (2,2): a0=175 a1=110 8-alpha mode (a0 > a1), alpha index=(4y+x+10)&7; colour c0=#CE51EF (0xCA9D) c0 > c1 c1=#2196A5 (0x24B4), index=(x+2y+10)&3 */
    0xAF, 0x6E, 0x1A, 0xEB, 0x23, 0x1A, 0xEB, 0x23, 0x9D, 0xCA, 0xB4, 0x24, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,2): a0=87 a1=233 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+11)&7; colour c0=#D6CF7B (0xD66F) c0 > c1 c1=#8C08B5 (0x8856), index=(x+2y+11)&3 */
    0x57, 0xE9, 0x63, 0x7D, 0x44, 0x63, 0x7D, 0x44, 0x6F, 0xD6, 0x56, 0x88, 0x93, 0x39, 0x93, 0x39,
    /* block (0,3): a0=159 a1=128 8-alpha mode (a0 > a1), alpha index=(4y+x+12)&7; colour c0=#A54D6B (0xA26D) c0 > c1 c1=#189E7B (0x1CEF), index=(x+2y+12)&3 */
    0x9F, 0x80, 0xAC, 0x8F, 0x68, 0xAC, 0x8F, 0x68, 0x6D, 0xA2, 0xEF, 0x1C, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,3): a0=101 a1=239 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+13)&7; colour c0=#BD41B5 (0xBA16) c0 > c1 c1=#1800FF (0x181F), index=(x+2y+13)&3 */
    0x65, 0xEF, 0xF5, 0x11, 0x8D, 0xF5, 0x11, 0x8D, 0x16, 0xBA, 0x1F, 0x18, 0x39, 0x93, 0x39, 0x93,
    /* block (2,3): a0=143 a1=146 6-alpha mode (a0 <= a1; entries 6,7 = 0,255), alpha index=(4y+x+14)&7; colour c0=#29C3CE (0x2E19) c0 > c1 c1=#10925A (0x148B), index=(x+2y+14)&3 */
    0x8F, 0x92, 0x3E, 0xA2, 0xB1, 0x3E, 0xA2, 0xB1, 0x19, 0x2E, 0x8B, 0x14, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (3,3): a0=128 a1=128 6-alpha mode with a0 == a1, alpha index=(4y+x+15)&7; colour c0=#394D63 (0x3A6C) c0 > c1 c1=#00B2A5 (0x0594), index=(x+2y+15)&3 */
    0x80, 0x80, 0x47, 0x34, 0xD6, 0x47, 0x34, 0xD6, 0x6C, 0x3A, 0x94, 0x05, 0x93, 0x39, 0x93, 0x39,
};

/* 8x6 DXT1: 2x2 blocks, the bottom row only half used. */
static const uint8_t k_dxt1_8x6[] = {
    /* block (0,0): c0=#EF9A9C (0xECD3) > c1=#63FBDE (0x67DB), 4-colour; index=(x+2y+0)&3 */
    0xD3, 0xEC, 0xDB, 0x67, 0xE4, 0x4E, 0xE4, 0x4E,
    /* block (1,0): c0=#B549F7 (0xB25E) > c1=#31F3DE (0x379B), 4-colour; index=(x+2y+1)&3 */
    0x5E, 0xB2, 0x9B, 0x37, 0x39, 0x93, 0x39, 0x93,
    /* block (0,1) (rows 2,3 lie below the 6-row image): c0=#F7D3CE (0xF699) > c1=#9C49B5 (0x9A56), 4-colour; index=(x+2y+2)&3 */
    0x99, 0xF6, 0x56, 0x9A, 0x4E, 0xE4, 0x4E, 0xE4,
    /* block (1,1) (rows 2,3 lie below the 6-row image): c0=#CEDF42 (0xCEE8) > c1=#5A9E18 (0x5CE3), 4-colour; index=(x+2y+3)&3 */
    0xE8, 0xCE, 0xE3, 0x5C, 0x93, 0x39, 0x93, 0x39,
};

/* Stage 0, a block-compressed texture. The size is log2 in the format word; IMAGE_RECT is sent
 * only when the size is not a power of two (the 8x6 case), and then it overrides the log2 size. */
static void pb_texture_dxt(uint32_t va, uint32_t fmt, uint32_t w, uint32_t h, uint32_t filter)
{
    uint32_t lw = 0, lh = 0;
    while ((1u << lw) < w) lw++;
    while ((1u << lh) < h) lh++;
    pb(0x1B00u + 0x14, filter);
    pb(0x1B00u + 0x00, va);
    pb(0x1B00u + 0x04, 0x1u | (2u << 4) | (fmt << 8) | (1u << 16) | (lw << 20) | (lh << 24));
    pb(0x1B00u + 0x08, 0x0303u);
    if ((1u << lw) != w || (1u << lh) != h)
        pb(0x1B00u + 0x1C, (w << 16) | h);
    pb(0x1B00u + 0x0C, 1u << 30);
}

/* One case: clear, then the texture as a w x h quad at (8,8). Blend is off, so the surface
 * keeps the texel alpha (compared, as 8888 surfaces always are); with alpha_test the texels
 * under alpha 0x80 are discarded. `texels` is the number of texels that should be visible. */
static void build_dxt_case(int idx, uint32_t surf, uint32_t vb, uint32_t tex, uint32_t fmt,
                           uint32_t w, uint32_t h, int alpha_test, unsigned texels, const char *name)
{
    const uint32_t clear = 0xFF101010u, white = 0xFFFFFFFFu;
    const float x0 = 8.0f, y0 = 8.0f, x1 = 8.0f + (float)w, y1 = 8.0f + (float)h;
    pb_begin();
    pb_surface(surf, 64, 64, 4);
    pb_clear(clear);
    pb(0x0304, 0);                                 /* SET_BLEND_ENABLE off */
    pb(0x0300, (uint32_t)alpha_test);              /* SET_ALPHA_TEST_ENABLE */
    pb(0x033C, 0x204u);                            /* ALPHA_FUNC GREATER */
    pb(0x0340, 0x80u);                             /* ALPHA_REF */
    pb_texture_dxt(tex, fmt, w, h, FILTER_NEAREST);
    pb_vertex_format(vb, 1);
    put_vertex(vb, 0, x0, y0, white, 0.0f, 0.0f);
    put_vertex(vb, 1, x1, y0, white, 1.0f, 0.0f);
    put_vertex(vb, 2, x0, y1, white, 0.0f, 1.0f);
    put_vertex(vb, 3, x1, y0, white, 1.0f, 0.0f);
    put_vertex(vb, 4, x1, y1, white, 1.0f, 1.0f);
    put_vertex(vb, 5, x0, y1, white, 0.0f, 1.0f);
    pb_draw_triangles(6);
    pb_flip();
    pb(0x0300, 0);                                 /* after the flip: later cases expect it off */
    g_scene[idx].name = name;
    g_scene[idx].nsurf = 1;
    g_scene[idx].surf[0] = (Surf){ surf, 64, 64, 4, clear, 2.0 * (w + h), (double)texels, TOL_DXT };
}

static void build_case9a(int flip)
{
    (void)flip;
    build_dxt_case(10, SURF_D1, VB_D1, TEX_D1, 0x0C, 16, 16, 0, 256,
                   "D1 DXT1 16x16, opaque 4-colour blocks (c0 > c1), point filter");
}
static void build_case9b(int flip)
{
    (void)flip;
    /* 7 texels use index 3 of a 3-colour block, so they are transparent and alpha-tested away. */
    build_dxt_case(11, SURF_D2, VB_D2, TEX_D2, 0x0C, 16, 16, 1, 256 - 7,
                   "D2 DXT1 16x16, 3-colour+transparent blocks, alpha test GREATER 0x80");
}
static void build_case9c(int flip)
{
    (void)flip;
    build_dxt_case(12, SURF_D3, VB_D3, TEX_D3, 0x0F, 16, 16, 0, 256,
                   "D3 DXT5 16x16, 8- and 6-alpha blocks, alpha compared in the surface");
}
static void build_case9d(int flip)
{
    (void)flip;
    build_dxt_case(13, SURF_D4, VB_D4, TEX_D4, 0x0C, 8, 6, 0, 48,
                   "D4 DXT1 8x6, height not a multiple of 4 (decode fallback)");
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
    /* Case 7's second texture: a different pattern, bright enough that the product stays visible. */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
        {
            /* Morton order: x bits in the even positions, y bits in the odd ones. */
            uint32_t m = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2)
                       | ((x & 4) << 2) | ((y & 4) << 3);
            *(uint16_t *)(G(TEX_C9) + m * 2) = pack565(255 - x * 16, 140 + y * 12, 200 + x * 6);
        }
    memcpy(G(TEX_D1), k_dxt1_opaque_16x16, sizeof k_dxt1_opaque_16x16);
    memcpy(G(TEX_D2), k_dxt1_alpha_16x16, sizeof k_dxt1_alpha_16x16);
    memcpy(G(TEX_D3), k_dxt5_16x16, sizeof k_dxt5_16x16);
    memcpy(G(TEX_D4), k_dxt1_8x6, sizeof k_dxt1_8x6);
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

static int surf_tol(const Surf *s)
{
    return s->tol ? s->tol : s->bpp == 2 ? TOL_565 : TOL_8888;
}

static Cmp compare(const Surf *s, const uint8_t *ref, const uint8_t *got)
{
    Cmp r;
    int tol = surf_tol(s);
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
        int x, y, shown = 0, tol = surf_tol(s);
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
static const build_fn k_build[NCASES] = { build_case1, build_case2, build_case3, build_case4, build_case2b, build_case2c,
                                      build_case5, build_case6, build_case7, build_case8,
                                      build_case9a, build_case9b, build_case9c, build_case9d };
/* Cases the back end runs with a flip and compares; case 3 (index 3) is the no-flip control. */
static const int k_parity[5] = { 0, 1, 4, 2, 5 };
/* Combiner cases: they switch the executor to program mode and set rc_seen for good, so both
 * passes run them after everything else, including the no-flip control. */
static const int k_rc[4] = { 6, 7, 8, 9 };
/* DXT cases: fixed-function, so they run with cases 1-4 and before the combiners switch the executor to
 * program mode. They leave blend and alpha test off. */
static const int k_dxt[4] = { 10, 11, 12, 13 };

int main(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *ref[NCASES][2], *got[NCASES][2];
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
    for (i = 0; i < 4; i++) {
        c = k_dxt[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("software");
        ref[c][0] = snapshot(&g_scene[c].surf[0]);
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

    for (i = 0; i < 4; i++) {
        c = k_dxt[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("d3d11 (flipped)");
        got[c][0] = snapshot(&g_scene[c].surf[0]);
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

    puts("-- parity (DXT) --");
    {
        static const char *dtag[4] = { "dxt1-opaque", "dxt1-3colour-alphatest", "dxt5-alpha", "dxt1-8x6" };
        for (i = 0; i < 4; i++) {
            c = k_dxt[i];
            if (!ref[c][0] || !got[c][0]) { ok = 0; continue; }
            ok &= check_parity(dtag[i], &g_scene[c].surf[0], ref[c][0], got[c][0], 1);
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

    puts("-- D3D11 back end: register combiners (program mode) --");
    for (i = 0; i < 4; i++) {
        c = k_rc[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("d3d11 (flipped)");
        got[c][0] = snapshot(&g_scene[c].surf[0]);
    }
    /* The combiner cases' CPU references run last, with the back end unregistered: once a title
     * programs the combiners the executor's rc_seen never clears, and the back end then applies the
     * leftover combiner to fixed-function batches (the CPU path ignores it), which would corrupt
     * cases 1-4. */
    puts("-- software executor: register combiners (program mode) --");
    nv2a_backend_register(NULL);
    for (i = 0; i < 4; i++) {
        c = k_rc[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("software");
        ref[c][0] = snapshot(&g_scene[c].surf[0]);
    }
    puts("-- parity (combiners) --");
    {
        static const char *rtag[4] = { "case5/modulate", "case6/lerp", "case7/two-tex", "case8/spec-add" };
        for (i = 0; i < 4; i++) {
            c = k_rc[i];
            if (!ref[c][0] || !got[c][0]) { ok = 0; continue; }
            ok &= check_parity(rtag[i], &g_scene[c].surf[0], ref[c][0], got[c][0], 1);
        }
        /* Cross-check: a one-stage tex0 * white-diffuse combiner is case 2's fixed-function
         * modulate, on the same triangle and texture, so the two surfaces must agree. */
        if (ref[1][0] && ref[6][0] && got[1][0] && got[6][0]) {
            ok &= check_parity("case5-vs-case2/cpu", &g_scene[6].surf[0], ref[1][0], ref[6][0], 1);
            ok &= check_parity("case5-vs-case2/gpu", &g_scene[6].surf[0], got[1][0], got[6][0], 1);
        } else {
            ok = 0;
        }
    }

    nv2a_backend_register(NULL);
    nv2a_set_commit_consumer(NULL);
    for (c = 0; c < NCASES; c++)
        for (i = 0; i < 2; i++) { free(ref[c][i]); free(got[c][i]); }
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fprintf(stderr, "FAIL: gpu_backend_parity\n"); return 1; }
    puts("PASS: gpu_backend_parity (software and D3D11 agree after the flip)");
    return 0;
}
