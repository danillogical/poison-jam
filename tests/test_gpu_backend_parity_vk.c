/* GPU back end parity, Vulkan: the same pushbuffer, executed by the software (CPU) executor and
 * by the Vulkan back end on a software Vulkan driver (lavapipe), must leave agreeing colour
 * surfaces in guest memory once the frame is flipped.
 *
 * This is the Vulkan sibling of test_gpu_backend_parity.c and follows its method: each case builds
 * a pushbuffer by hand (standalone NV2A, NV097 object on subchannel 0, the real executor on the
 * commit seam). Every CPU reference is taken first, because the back end installer is idempotent and
 * cannot be re-registered after nv2a_backend_register(NULL).
 *
 * It tests the contract of the Vulkan back end, not its implementation:
 *   - Parity cases (compared with the CPU executor, same tolerances as the D3D11 test: 8 per channel
 *     on 8888, 9 on R5G6B5, at least 98% of pixels, drawn-region mismatches within perimeter + 2%,
 *     coverage differing by at most the perimeter): clear + untextured triangle (16 and 32 bit),
 *     textured triangle (magnified, 1 texel per pixel, linear filter), alpha-tested triangles
 *     (untextured and textured), render-to-texture, and flush-before-sample (a texture that overlaps,
 *     but does not alias, a target drawn this frame).
 *   - Absolute cases, where the CPU executor is no reference (it has no stencil, and its clear
 *     ignores the colour mask): the clear contract, clear colour masks and scissors, face culling,
 *     depth (LESS with write on and off, test off with write on, the clear value), stencil (INCR,
 *     DECR with clamp, EQUAL, clear value, ignored without a depth buffer), a 16 bit round trip,
 *     alpha on 16 bit targets, reshape, eviction, non-finite vertices.
 *   - One register-combiner case as a documented known difference: this back end draws combiner
 *     batches as stage 0 x diffuse, so colours are expected to differ from the CPU reference
 *     (XFAIL); only coverage is gated.
 *
 * The test selects the CPU device with RECOMP_VK_DEVICE=llvmpipe, which CTest also sets together with
 * VK_DRIVER_FILES (cache variable XBR_VK_SW_ICD). It skips (exit 77) with a message when
 * XBR_VK_SW_ICD is unset, or when the back end cannot be installed -- notably in an elevated process,
 * where the Vulkan loader ignores VK_DRIVER_FILES. Run it non-elevated. */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "video/nv2a_vk_backend.h"
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

/* ── Guest memory map ────────────────────────────────────────────────────
 * Plain RAM addresses well clear of the (synthetic, empty) XBE image; no contiguous allocation is
 * made, so a DMA offset resolves to itself. Every case owns its addresses, so no case depends on
 * what an earlier one left in a target. */
#define SURF(n)   (0x00400000u + (uint32_t)(n) * 0x00010000u)   /* colour surfaces, up to 64x64x4 */
#define VBUF(n)   (0x00800000u + (uint32_t)(n) * 0x00010000u)   /* vertex buffers */
#define ZETA(n)   (0x00B00000u + (uint32_t)(n) * 0x00010000u)   /* zeta "surfaces": only the address matters */
#define TEX_MAG   0x00C00000u     /* 8x8 R5G6B5 linear, pitch 16     */
#define TEX_ONE   0x00C10000u     /* 16x16 R5G6B5 linear, pitch 32   */
#define TEX_ALPHA 0x00C20000u     /* 16x16 A8R8G8B8 linear, pitch 64 */
#define EVICT_BASE 0x00D00000u    /* 17 targets of 16x16x4, 0x1000 apart */
#define SURF_BYTES 0x00010000u    /* span pre-filled/cleared per surface */
#define SENTINEL  0x5Au
#define SENTINEL32 0x5A5A5A5Au

enum {                            /* SURF()/VBUF()/ZETA() slots */
    N_CLEAR565, N_TRI8888, N_TEX_MAG, N_TEX_ONE, N_TEX_LIN, N_ALPHA_TRI, N_TEX_ALPHA,
    N_RTT_A, N_RTT_B, N_FS_T, N_FS_U, N_NEG, N_ALPHA16, N_RESHAPE, N_ROUNDTRIP,
    N_DEPTH1, N_DEPTH2, N_STENCIL1, N_STENCIL2, N_STENCIL3, N_CMASK32, N_CMASK16,
    N_SCISSOR32, N_SCISSOR16, N_CULL0, N_CULL1, N_CULL2, N_CULL3, N_CULL4, N_CULL5, N_CULL6,
    N_NONFINITE, N_XFAIL, N_CLEARC16, N_CLEARC32, N_ALPHA16B, N_NSLOTS
};

#define FILTER_NEAREST 0x01010000u   /* SET_TEXTURE_FILTER: MIN=1 (bits 16-23), MAG=1 (24-27) */
#define FILTER_LINEAR  0x02020000u   /* MIN=2, MAG=2 */

/* The tolerances of test_gpu_backend_parity.c. */
#define TOL_8888 8
#define TOL_565  9
#define COVER_TOL 24

#define CLR   0xFF101010u
#define RED   0xFFFF0000u
#define GREEN 0xFF00FF00u
#define BLUE  0xFF0000FFu
#define YELLOW 0xFFFFFF00u

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
    if (g_pbn + 2u > 0x4000u / 4u) { fprintf(stderr, "FAIL: test pushbuffer overflow\n"); exit(1); }
    g_pb[g_pbn++] = (1u << 18) | method;
    g_pb[g_pbn++] = value;
}
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* Every case starts here, so none depends on the state an earlier one set: no alpha test, blend,
 * culling, depth, stencil, zeta, texture; colour mask all; front face CW. */
static void pb_reset_state(void)
{
    pb(0x0300, 0);                                 /* alpha test off */
    pb(0x0304, 0);                                 /* blend off */
    pb(0x0308, 0);                                 /* cull off */
    pb(0x030C, 0);                                 /* depth test off */
    pb(0x032C, 0);                                 /* stencil test off */
    pb(0x0354, 0x207u);                            /* depth func ALWAYS */
    pb(0x0358, 0x01010101u);                       /* colour mask: A R G B */
    pb(0x035C, 0);                                 /* depth write off */
    pb(0x0360, 0xFFu);                             /* stencil write mask */
    pb(0x0364, 0x207u);                            /* stencil func ALWAYS */
    pb(0x0368, 0);                                 /* stencil ref */
    pb(0x036C, 0xFFu);                             /* stencil read mask */
    pb(0x0370, 0x1E00u);                           /* stencil ops KEEP */
    pb(0x0374, 0x1E00u);
    pb(0x0378, 0x1E00u);
    pb(0x039C, 0x405u);                            /* cull face BACK */
    pb(0x03A0, 0x900u);                            /* front face CW */
    pb(0x0214, 0);                                 /* no zeta */
    pb(0x1B0C, 0);                                 /* stage 0 off */
    pb(0x1B4C, 0);                                 /* stage 1 off */
}
static void pb_surface(uint32_t va, uint32_t w, uint32_t h, uint32_t bpp)
{
    pb(0x0200, w << 16);                           /* CLIP_H: x0 w */
    pb(0x0204, h << 16);                           /* CLIP_V: y0 h */
    pb(0x0208, bpp == 2 ? 0x0103u : 0x0108u);      /* linear R5G6B5 / A8R8G8B8 */
    pb(0x020C, w * bpp);                           /* PITCH */
    pb(0x0210, va);                                /* COLOR_OFFSET */
}
static void pb_clear_flags(uint32_t argb, uint32_t flags)
{
    pb(0x1D90, argb);                              /* SET_COLOR_CLEAR_VALUE */
    pb(0x1D94, flags);                             /* CLEAR_SURFACE */
}
static void pb_clear(uint32_t argb) { pb_clear_flags(argb, 0xF0u); }
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

static void put_vertex_z(uint32_t vb, int i, float x, float y, float z, uint32_t diffuse,
                         float u, float v)
{
    float *f = (float *)G(vb + (uint32_t)i * 24u);
    f[0] = x; f[1] = y; f[2] = z;
    *(uint32_t *)(G(vb + (uint32_t)i * 24u) + 12) = diffuse;
    f[4] = u; f[5] = v;
}
static void put_vertex(uint32_t vb, int i, float x, float y, uint32_t diffuse, float u, float v)
{
    put_vertex_z(vb, i, x, y, 0.5f, diffuse, u, v);
}
/* A quad as two triangles (x0,y0)-(x1,y0)-(x0,y1) and (x1,y0)-(x1,y1)-(x0,y1), drawn at once. */
static void pb_quad(uint32_t vb, float x0, float y0, float x1, float y1, float z, uint32_t diffuse)
{
    put_vertex_z(vb, 0, x0, y0, z, diffuse, 0, 0);
    put_vertex_z(vb, 1, x1, y0, z, diffuse, 0, 0);
    put_vertex_z(vb, 2, x0, y1, z, diffuse, 0, 0);
    put_vertex_z(vb, 3, x1, y0, z, diffuse, 0, 0);
    put_vertex_z(vb, 4, x1, y1, z, diffuse, 0, 0);
    put_vertex_z(vb, 5, x0, y1, z, diffuse, 0, 0);
    pb_vertex_format(vb, 0);
    pb_draw_triangles(6);
}

static uint16_t pack565(uint32_t r, uint32_t g, uint32_t b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* ── Scenes compared with the CPU executor ───────────────────────────────── */
/* tol: per-channel tolerance; 0 means the default for the surface's bpp.
 * clear: A8R8G8B8 for a 32bpp surface, the raw 5:6:5 word for a 16bpp one. */
typedef struct { uint32_t va, w, h, bpp, clear; double perim, area; int tol; } Surf;
typedef struct { const char *name; int nsurf; Surf surf[2]; } Scene;

enum {
    PC_CLEAR565, PC_TRI8888, PC_TEX_MAG, PC_TEX_ONE, PC_TEX_LIN, PC_ALPHA_TRI, PC_TEX_ALPHA,
    PC_RTT, PC_FLUSH_SAMPLE, PC_NEG, NPAR
};
static Scene g_scene[NPAR];

static void build_clear_tri(int idx, uint32_t surf, uint32_t vb, uint32_t bpp, uint32_t clear,
                            uint32_t diffuse, int flip, const char *name)
{
    pb_begin();
    pb_reset_state();
    pb_surface(surf, 64, 64, bpp);
    pb_clear(clear);
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 8.0f, 8.0f, diffuse, 0, 0);
    put_vertex(vb, 1, 56.0f, 12.0f, diffuse, 0, 0);
    put_vertex(vb, 2, 20.0f, 58.0f, diffuse, 0, 0);
    pb_draw_triangles(3);
    if (flip) pb_flip();
    g_scene[idx].name = name;
    g_scene[idx].nsurf = 1;
    g_scene[idx].surf[0] = (Surf){ surf, 64, 64, bpp, clear,
        hypot(48, 4) + hypot(36, 46) + hypot(12, 50),
        fabs(0.5 * (48.0 * 50.0 - 4.0 * 12.0)), 0 };
}
/* 0x220C is R=4 G=16 B=12, about the 0x204060 the 8888 cases use. */
static void build_clear565(int flip)
{
    build_clear_tri(PC_CLEAR565, SURF(N_CLEAR565), VBUF(N_CLEAR565), 2, 0x0000220Cu, 0xFFE06020u,
                    flip, "clear + untextured triangle (R5G6B5)");
}
static void build_tri8888(int flip)
{
    build_clear_tri(PC_TRI8888, SURF(N_TRI8888), VBUF(N_TRI8888), 4, CLR, 0xFF3070D0u, flip,
                    "clear + untextured triangle (A8R8G8B8)");
}

/* An 8x8 R5G6B5 texture magnified about 6x onto a triangle. */
static void build_tex(int idx, uint32_t surf, uint32_t vb, uint32_t filter, const char *name)
{
    pb_begin();
    pb_reset_state();
    pb_surface(surf, 64, 64, 4);
    pb_clear(CLR);
    pb_texture_on(TEX_MAG, 0x11, 8, 8, 16, filter);
    pb_vertex_format(vb, 1);
    put_vertex(vb, 0, 8.0f, 8.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(vb, 1, 56.0f, 8.0f, 0xFFFFFFFFu, 8.0f, 0.0f);
    put_vertex(vb, 2, 8.0f, 56.0f, 0xFFFFFFFFu, 0.0f, 8.0f);
    pb_draw_triangles(3);
    pb_flip();
    g_scene[idx].name = name;
    g_scene[idx].nsurf = 1;
    g_scene[idx].surf[0] = (Surf){ surf, 64, 64, 4, CLR, 48 + 48 + 48 * 1.41421356, 0.5 * 48 * 48, 0 };
}
static void build_tex_mag(int flip)
{
    (void)flip;
    build_tex(PC_TEX_MAG, SURF(N_TEX_MAG), VBUF(N_TEX_MAG), FILTER_NEAREST,
              "textured triangle, magnified, filter NEAREST (R5G6B5 8x8, white diffuse)");
}
static void build_tex_lin(int flip)
{
    (void)flip;
    build_tex(PC_TEX_LIN, SURF(N_TEX_LIN), VBUF(N_TEX_LIN), FILTER_LINEAR,
              "same texture, filter LINEAR (coverage + smoothness only)");
}
/* One texel per pixel, so the comparison does not depend on how magnification is filtered. */
static void build_tex_one(int flip)
{
    const uint32_t vb = VBUF(N_TEX_ONE);
    (void)flip;
    pb_begin();
    pb_reset_state();
    pb_surface(SURF(N_TEX_ONE), 64, 64, 4);
    pb_clear(CLR);
    pb_texture_on(TEX_ONE, 0x11, 16, 16, 32, FILTER_NEAREST);
    pb_vertex_format(vb, 1);
    put_vertex(vb, 0, 8.0f, 8.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(vb, 1, 24.0f, 8.0f, 0xFFFFFFFFu, 16.0f, 0.0f);
    put_vertex(vb, 2, 8.0f, 24.0f, 0xFFFFFFFFu, 0.0f, 16.0f);
    pb_draw_triangles(3);
    pb_flip();
    g_scene[PC_TEX_ONE].name = "textured triangle at 1 texel per pixel (R5G6B5 16x16)";
    g_scene[PC_TEX_ONE].nsurf = 1;
    g_scene[PC_TEX_ONE].surf[0] = (Surf){ SURF(N_TEX_ONE), 64, 64, 4, CLR,
                                          16 + 16 + 16 * 1.41421356, 0.5 * 16 * 16, 0 };
}

/* Alpha test GREATER 0x80 on the diffuse alpha: the 0x40 triangle is discarded, the 0xC0 one drawn. */
static void build_alpha_tri(int flip)
{
    const uint32_t vb = VBUF(N_ALPHA_TRI);
    (void)flip;
    pb_begin();
    pb_reset_state();
    pb_surface(SURF(N_ALPHA_TRI), 64, 64, 4);
    pb_clear(CLR);
    pb(0x0300, 1);                                 /* ALPHA_TEST_ENABLE */
    pb(0x033C, 0x204u);                            /* ALPHA_FUNC GREATER */
    pb(0x0340, 0x80u);                             /* ALPHA_REF */
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 6.0f, 6.0f, 0x40FF0000u, 0, 0);
    put_vertex(vb, 1, 26.0f, 6.0f, 0x40FF0000u, 0, 0);
    put_vertex(vb, 2, 6.0f, 26.0f, 0x40FF0000u, 0, 0);
    put_vertex(vb, 3, 34.0f, 34.0f, 0xC000FF00u, 0, 0);
    put_vertex(vb, 4, 58.0f, 34.0f, 0xC000FF00u, 0, 0);
    put_vertex(vb, 5, 34.0f, 58.0f, 0xC000FF00u, 0, 0);
    pb_draw_triangles(6);
    pb_flip();
    g_scene[PC_ALPHA_TRI].name = "alpha test GREATER 0x80, untextured: alpha 0x40 triangle dropped, 0xC0 kept";
    g_scene[PC_ALPHA_TRI].nsurf = 1;
    g_scene[PC_ALPHA_TRI].surf[0] = (Surf){ SURF(N_ALPHA_TRI), 64, 64, 4, CLR,
                                            24 + 24 + 24 * 1.41421356, 0.5 * 24 * 24, 0 };
}

/* A 16x16 A8R8G8B8 texture in 4x4 blocks, alternately alpha 0xFF and 0x30, 1:1 on a quad. */
static void build_tex_alpha(int flip)
{
    const uint32_t vb = VBUF(N_TEX_ALPHA), white = 0xFFFFFFFFu;
    (void)flip;
    pb_begin();
    pb_reset_state();
    pb_surface(SURF(N_TEX_ALPHA), 64, 64, 4);
    pb_clear(CLR);
    pb(0x0300, 1);
    pb(0x033C, 0x204u);
    pb(0x0340, 0x80u);
    pb_texture_on(TEX_ALPHA, 0x12, 16, 16, 64, FILTER_NEAREST);
    pb_vertex_format(vb, 1);
    put_vertex(vb, 0, 8.0f, 8.0f, white, 0.0f, 0.0f);
    put_vertex(vb, 1, 24.0f, 8.0f, white, 16.0f, 0.0f);
    put_vertex(vb, 2, 8.0f, 24.0f, white, 0.0f, 16.0f);
    put_vertex(vb, 3, 24.0f, 8.0f, white, 16.0f, 0.0f);
    put_vertex(vb, 4, 24.0f, 24.0f, white, 16.0f, 16.0f);
    put_vertex(vb, 5, 8.0f, 24.0f, white, 0.0f, 16.0f);
    pb_draw_triangles(6);
    pb_flip();
    g_scene[PC_TEX_ALPHA].name = "textured quad, A8R8G8B8 texture, alpha test GREATER 0x80 (half the texels)";
    g_scene[PC_TEX_ALPHA].nsurf = 1;
    g_scene[PC_TEX_ALPHA].surf[0] = (Surf){ SURF(N_TEX_ALPHA), 64, 64, 4, CLR, 2.0 * (16 + 16), 128.0, 0 };
}

/* Render-to-texture, the texture aliasing the target exactly: A is drawn, then B samples A. */
static void build_rtt(int flip)
{
    const uint32_t a = SURF(N_RTT_A), b = SURF(N_RTT_B), va = VBUF(N_RTT_A), vbb = VBUF(N_RTT_B);
    const uint32_t clearA = 0xFF0000FFu, clearB = 0xFF303030u;
    pb_begin();
    pb_reset_state();
    pb_surface(a, 32, 32, 4);
    pb_clear(clearA);
    pb_vertex_format(va, 0);
    put_vertex(va, 0, 4.0f, 4.0f, 0xFFFF2020u, 0, 0);
    put_vertex(va, 1, 28.0f, 6.0f, 0xFFFF2020u, 0, 0);
    put_vertex(va, 2, 8.0f, 28.0f, 0xFFFF2020u, 0, 0);
    pb_draw_triangles(3);
    pb_surface(b, 64, 64, 4);
    pb_clear(clearB);
    pb_texture_on(a, 0x12, 32, 32, 32 * 4, FILTER_NEAREST);
    pb_vertex_format(vbb, 1);
    put_vertex(vbb, 0, 16.0f, 16.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(vbb, 1, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(vbb, 2, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    put_vertex(vbb, 3, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(vbb, 4, 48.0f, 48.0f, 0xFFFFFFFFu, 32.0f, 32.0f);
    put_vertex(vbb, 5, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    pb_draw_triangles(6);
    if (flip) pb_flip();
    g_scene[PC_RTT].name = "render-to-texture (A drawn, B samples A, texture aliases the target)";
    g_scene[PC_RTT].nsurf = 2;
    g_scene[PC_RTT].surf[0] = (Surf){ b, 64, 64, 4, clearB, 4 * 32.0, 32.0 * 32.0, 0 };
    g_scene[PC_RTT].surf[1] = (Surf){ a, 32, 32, 4, clearA, hypot(24, 2) + hypot(16, 22) + hypot(4, 24),
                                      fabs(0.5 * (24.0 * 24.0 - 2.0 * 4.0)), 0 };
}

/* Flush-before-sample: target T is drawn this frame (and so is dirty on the GPU), then U samples a
 * window of T as an A8R8G8B8 texture that starts 16 rows into T. That texture overlaps T but does
 * not alias it (another address), so the back end has to write T back to guest memory before it
 * reads the texture from there. T's content is far from U's clear colour everywhere, so the whole
 * quad is covered; without the flush U would show the guest memory's sentinel instead. */
static void build_flush_sample(int flip)
{
    const uint32_t t = SURF(N_FS_T), u = SURF(N_FS_U), vt = VBUF(N_FS_T), vu = VBUF(N_FS_U);
    const uint32_t clearT = 0xFF3060A0u, clearU = CLR;
    pb_begin();
    pb_reset_state();
    pb_surface(t, 64, 64, 4);
    pb_clear(clearT);
    pb_vertex_format(vt, 0);
    put_vertex(vt, 0, 2.0f, 2.0f, 0xFFFF2020u, 0, 0);
    put_vertex(vt, 1, 62.0f, 10.0f, 0xFFFF2020u, 0, 0);
    put_vertex(vt, 2, 10.0f, 62.0f, 0xFFFF2020u, 0, 0);
    pb_draw_triangles(3);
    pb_surface(u, 64, 64, 4);
    pb_clear(clearU);
    pb_texture_on(t + 16u * 256u, 0x12, 32, 32, 256, FILTER_NEAREST);
    pb_vertex_format(vu, 1);
    put_vertex(vu, 0, 16.0f, 16.0f, 0xFFFFFFFFu, 0.0f, 0.0f);
    put_vertex(vu, 1, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(vu, 2, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    put_vertex(vu, 3, 48.0f, 16.0f, 0xFFFFFFFFu, 32.0f, 0.0f);
    put_vertex(vu, 4, 48.0f, 48.0f, 0xFFFFFFFFu, 32.0f, 32.0f);
    put_vertex(vu, 5, 16.0f, 48.0f, 0xFFFFFFFFu, 0.0f, 32.0f);
    pb_draw_triangles(6);
    if (flip) pb_flip();
    g_scene[PC_FLUSH_SAMPLE].name = "flush-before-sample (T drawn, U samples a non-aliased window of T)";
    g_scene[PC_FLUSH_SAMPLE].nsurf = 2;
    g_scene[PC_FLUSH_SAMPLE].surf[0] = (Surf){ u, 64, 64, 4, clearU, 4 * 32.0, 32.0 * 32.0, 0 };
    g_scene[PC_FLUSH_SAMPLE].surf[1] = (Surf){ t, 64, 64, 4, clearT, hypot(60, 8) + hypot(52, 52) + hypot(8, 60),
                                               fabs(0.5 * (60.0 * 60.0 - 8.0 * 8.0)), 0 };
}

/* Negative control: the same scene as the 16 bit clear + triangle, at its own address. */
static void build_neg(int flip)
{
    build_clear_tri(PC_NEG, SURF(N_NEG), VBUF(N_NEG), 2, 0x0000220Cu, 0xFFE06020u, flip,
                    "negative control (R5G6B5)");
}

typedef void (*build_fn)(int flip);
static const build_fn k_build[NPAR] = {
    build_clear565, build_tri8888, build_tex_mag, build_tex_one, build_tex_lin, build_alpha_tri,
    build_tex_alpha, build_rtt, build_flush_sample, build_neg
};
/* Cases compared with the CPU reference after a flip (PC_NEG is the no-flip control, run apart). */
static const int k_parity[] = { PC_CLEAR565, PC_TRI8888, PC_TEX_MAG, PC_TEX_ONE, PC_TEX_LIN,
                                PC_RTT, PC_FLUSH_SAMPLE };
#define NPARITY ((int)(sizeof k_parity / sizeof k_parity[0]))

static void write_guest_data(void)
{
    uint32_t x, y;
    /* 8x8 R5G6B5: every texel distinct and far from the clear colour. */
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++)
            *(uint16_t *)(G(TEX_MAG) + y * 16 + x * 2) = pack565(60 + x * 24, 60 + y * 24, 220 - x * 8 - y * 8);
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++)
            *(uint16_t *)(G(TEX_ONE) + y * 32 + x * 2) = pack565(60 + x * 12, 60 + y * 12, 220 - x * 6 - y * 6);
    /* 16x16 A8R8G8B8: 4x4 blocks alternately opaque and alpha 0x30; colours far from the clear. */
    for (y = 0; y < 16; y++)
        for (x = 0; x < 16; x++) {
            uint32_t a = (((x >> 2) + (y >> 2)) & 1u) ? 0x30u : 0xFFu;
            *(uint32_t *)(G(TEX_ALPHA) + y * 64 + x * 4) =
                (a << 24) | ((60 + x * 12) << 16) | ((60 + y * 12) << 8) | (220 - x * 6 - y * 6);
        }
}

/* ── Running ─────────────────────────────────────────────────────────────── */
static NV2AState *g_gpu;
static int g_info;              /* nonzero: the checks that run are informational (the CPU executor) */

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
    if (!ok)
        fprintf(stderr, "FAIL: pushbuffer rejected (%s): %s, subchannel %u method 0x%04X param 0x%08X, get 0x%X\n",
                what, nv2a_submit_diagnostic(g_gpu->pfifo.submit_diag),
                g_gpu->pfifo.submit_diag_subchannel, g_gpu->pfifo.submit_diag_method,
                g_gpu->pfifo.submit_diag_param, g_gpu->pfifo.submit_diag_get);
    return ok;
}

static void prefill_va(uint32_t va, size_t bytes, uint8_t v) { memset(G(va), v, bytes); }
static void prefill(const Scene *sc)
{
    int i;
    for (i = 0; i < sc->nsurf; i++)
        prefill_va(sc->surf[i].va, SURF_BYTES, SENTINEL);
}

static uint8_t *snapshot_va(uint32_t va, size_t n)
{
    uint8_t *p = (uint8_t *)malloc(n);
    if (p) memcpy(p, G(va), n);
    return p;
}
static uint8_t *snapshot(const Surf *s) { return snapshot_va(s->va, (size_t)s->w * s->h * s->bpp); }

static uint32_t px32(const uint8_t *p, uint32_t w, int x, int y)
{
    return *(const uint32_t *)(p + ((size_t)y * w + (size_t)x) * 4u);
}
static uint32_t px16(const uint8_t *p, uint32_t w, int x, int y)
{
    return *(const uint16_t *)(p + ((size_t)y * w + (size_t)x) * 2u);
}

/* Pixels in [x0,x1) x [y0,y1) of a 32 bit surface that are not `want`; prints the first. */
static unsigned rect_bad32(const char *what, const uint8_t *p, uint32_t w, int x0, int y0, int x1,
                           int y1, uint32_t want)
{
    unsigned bad = 0;
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            uint32_t v = px32(p, w, x, y);
            if (v != want) {
                if (!bad && !g_info)
                    fprintf(stderr, "      %s: (%d,%d) is 0x%08X, want 0x%08X\n", what, x, y, v, want);
                bad++;
            }
        }
    return bad;
}
static int expect32(const char *what, const uint8_t *p, uint32_t w, int x0, int y0, int x1, int y1,
                    uint32_t want)
{
    unsigned bad = rect_bad32(what, p, w, x0, y0, x1, y1, want);
    printf("      %-44s %s (%u of %d pixels wrong)\n", what, bad ? (g_info ? "differs" : "FAIL") : "ok", bad,
           (x1 - x0) * (y1 - y0));
    return bad == 0;
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

static int surf_tol(const Surf *s) { return s->tol ? s->tol : s->bpp == 2 ? TOL_565 : TOL_8888; }

static Cmp compare(const Surf *s, const uint8_t *ref, const uint8_t *got)
{
    Cmp r;
    int tol = surf_tol(s);
    int clr[4], x, y;
    unsigned good = 0, bad_region = 0;
    uint8_t cpx[4];

    memset(&r, 0, sizeof r);
    if (s->bpp == 2) { uint16_t v = (uint16_t)s->clear; memcpy(cpx, &v, 2); }
    else memcpy(cpx, &s->clear, 4);
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
            fprintf(stderr, "FAIL [%s]: expected the surfaces to DIFFER, but %.2f%% match\n", label, c.all_pct);
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

/* The linear-filter case: the CPU screen-space path point-samples, so colours are not compared.
 * Coverage must still agree, and at every texel boundary along one row the GPU value must lie
 * between the two neighbouring texel colours and not be a hard step. */
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
    printf("  [linear/target] colour match %.2f%% (information only; CPU is nearest), coverage ref=%u got=%u (xor %u, allowed %.0f),\n"
           "      row %d: %d texel boundaries, %d blended, %d outside the neighbouring texel colours\n",
           c.all_pct, c.cov_ref, c.cov_got, c.cov_diff, s->perim, y, boundaries, smooth, outside);
    if (c.cov_ref < s->area * 0.9 || c.cov_ref > s->area * 1.1) {
        fprintf(stderr, "FAIL [linear]: CPU reference covers %u px, geometry predicts %.0f\n", c.cov_ref, s->area);
        ok = 0;
    }
    if (c.cov_diff > s->perim) {
        fprintf(stderr, "FAIL [linear]: coverage differs by %u px (allowed %.0f)\n", c.cov_diff, s->perim);
        ok = 0;
    }
    if (boundaries < 3 || outside || smooth < boundaries) {
        fprintf(stderr, "FAIL [linear]: GPU output is not smooth across texel boundaries\n");
        ok = 0;
    }
    return ok;
}

/* ── Clear contract ──────────────────────────────────────────────────────── */
/* The value is in the surface's own format, so a 16bpp surface holds exactly its low 16 bits (as
 * 5:6:5) and a 32bpp one the full word, in every pixel, after the flip. */
typedef struct { uint32_t bpp, value, expect; } ClearCase;
static int run_clear_contract(const char *who)
{
    static const ClearCase cases[] = {
        { 2, 0x0000FFFFu, 0xFFFFu }, { 2, 0x000020E4u, 0x20E4u }, { 2, 0x00000000u, 0x0000u },
        { 2, 0x0000F800u, 0xF800u }, { 4, 0xFF204060u, 0xFF204060u },
        { 2, 0xDEAD20E4u, 0x20E4u },
    };
    size_t n, k, nbad;
    int ok = 1;
    for (n = 0; n < sizeof cases / sizeof cases[0]; n++) {
        const ClearCase *c = &cases[n];
        uint32_t va = c->bpp == 2 ? SURF(N_CLEARC16) : SURF(N_CLEARC32);
        Surf s;
        uint8_t *px;
        memset(&s, 0, sizeof s);
        s.va = va; s.w = 64; s.h = 64; s.bpp = c->bpp;
        prefill_va(va, SURF_BYTES, SENTINEL);
        pb_begin(); pb_reset_state(); pb_surface(va, 64, 64, c->bpp); pb_clear(c->value); pb_flip();
        ok &= submit(who);
        px = snapshot(&s);
        if (!px) { ok = 0; continue; }
        nbad = 0;
        for (k = 0; k < 64u * 64u; k++) {
            uint32_t got = c->bpp == 2 ? ((uint16_t *)px)[k] : ((uint32_t *)px)[k];
            if (got != c->expect) nbad++;
        }
        printf("  [%s] clear contract %ubpp value 0x%08X: want 0x%0*X in all pixels, %u differ%s\n",
               who, c->bpp * 8, c->value, (int)c->bpp * 2, c->expect, (unsigned)nbad, nbad ? "  FAIL" : "");
        if (nbad) {
            fprintf(stderr, "FAIL: %s clear contract %ubpp 0x%08X: pixel 0 is 0x%X, want 0x%X\n", who,
                    c->bpp * 8, c->value,
                    c->bpp == 2 ? ((uint16_t *)px)[0] : ((uint32_t *)px)[0], c->expect);
            ok = 0;
        }
        free(px);
    }
    return ok;
}

/* Clear colour masks: flags 0x10 R, 0x20 G, 0x40 B each touch only their channel; 0x80 (alpha) only
 * on a 32 bit target. All five clears are in one frame, each with a value that differs in every
 * channel, so a clear that ignored the mask would leave a different word behind. */
static int run_clear_masks(void)
{
    int ok = 1;
    uint8_t *px;
    {
        uint32_t va = SURF(N_CMASK32);
        Surf s = { va, 64, 64, 4, 0, 0, 0, 0 };
        prefill_va(va, SURF_BYTES, SENTINEL);
        pb_begin(); pb_reset_state(); pb_surface(va, 64, 64, 4);
        pb_clear_flags(0xFF000000u, 0xF0u);
        pb_clear_flags(0x00112233u, 0x10u);          /* R -> 0x11 */
        pb_clear_flags(0x00445566u, 0x20u);          /* G -> 0x55 */
        pb_clear_flags(0x00778899u, 0x40u);          /* B -> 0x99 */
        pb_clear_flags(0x12345678u, 0x80u);          /* A -> 0x12 */
        pb_flip();
        ok &= submit("clear masks 32bpp");
        if ((px = snapshot(&s)) != NULL) {
            ok &= expect32("32bpp R,G,B,A cleared one flag at a time", px, 64, 0, 0, 64, 64, 0x12115599u);
            free(px);
        } else ok = 0;
    }
    {
        uint32_t va = SURF(N_CMASK16);
        Surf s = { va, 64, 64, 2, 0, 0, 0, 0 };
        unsigned bad = 0;
        int x, y;
        prefill_va(va, SURF_BYTES, SENTINEL);
        pb_begin(); pb_reset_state(); pb_surface(va, 64, 64, 2);
        pb_clear_flags(0x00000000u, 0xF0u);
        pb_clear_flags(0x0000F800u, 0x10u);          /* R only */
        pb_clear_flags(0x000007E0u, 0x20u);          /* G only: 0xFFE0 */
        pb_clear_flags(0x0000001Fu, 0x40u);          /* B only: 0xFFFF */
        pb_clear_flags(0x00000000u, 0x80u);          /* alpha flag alone: nothing on 16 bit */
        pb_flip();
        ok &= submit("clear masks 16bpp");
        if ((px = snapshot(&s)) != NULL) {
            for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) if (px16(px, 64, x, y) != 0xFFFFu) bad++;
            printf("      %-44s %s (%u of 4096 pixels wrong)\n", "16bpp R,G,B then alpha flag", bad ? "FAIL" : "ok", bad);
            if (bad) fprintf(stderr, "      (0,0) is 0x%04X, want 0xFFFF\n", px16(px, 64, 0, 0));
            ok &= bad == 0;
            free(px);
        } else ok = 0;
    }
    return ok;
}

/* A clear confined to a clip rectangle must leave everything outside it as it was. */
static int run_clear_scissor(void)
{
    int ok = 1, x, y;
    uint8_t *px;
    {
        uint32_t va = SURF(N_SCISSOR32);
        Surf s = { va, 64, 64, 4, 0, 0, 0, 0 };
        const uint32_t want = 0xFF2040A0u;
        unsigned bad = 0;
        prefill_va(va, SURF_BYTES, SENTINEL);
        pb_begin(); pb_reset_state(); pb_surface(va, 64, 64, 4);
        pb(0x0200, (32u << 16) | 16u); pb(0x0204, (32u << 16) | 16u);
        pb_clear(want); pb_flip();
        ok &= submit("scissored clear 32bpp");
        if ((px = snapshot(&s)) != NULL) {
            for (y = 0; y < 64; y++) for (x = 0; x < 64; x++)
                if (px32(px, 64, x, y) != (x >= 16 && x < 48 && y >= 16 && y < 48 ? want : SENTINEL32)) bad++;
            printf("      %-44s %s (%u of 4096 pixels wrong)\n", "32bpp clear inside the clip only", bad ? "FAIL" : "ok", bad);
            ok &= bad == 0;
            free(px);
        } else ok = 0;
    }
    {
        uint32_t va = SURF(N_SCISSOR16);
        Surf s = { va, 64, 64, 2, 0, 0, 0, 0 };
        unsigned bad = 0;
        prefill_va(va, SURF_BYTES, SENTINEL);
        pb_begin(); pb_reset_state(); pb_surface(va, 64, 64, 2);
        pb(0x0200, (32u << 16) | 16u); pb(0x0204, (32u << 16) | 16u);
        pb_clear(0x000020E4u); pb_flip();
        ok &= submit("scissored clear 16bpp");
        if ((px = snapshot(&s)) != NULL) {
            for (y = 0; y < 64; y++) for (x = 0; x < 64; x++)
                if (px16(px, 64, x, y) != (x >= 16 && x < 48 && y >= 16 && y < 48 ? 0x20E4u : 0x5A5Au)) bad++;
            printf("      %-44s %s (%u of 4096 pixels wrong)\n", "16bpp clear inside the clip only", bad ? "FAIL" : "ok", bad);
            ok &= bad == 0;
            free(px);
        } else ok = 0;
    }
    return ok;
}

/* ── Face culling ────────────────────────────────────────────────────────── */
/* The red triangle (4,4)-(28,4)-(4,28) runs clockwise on screen and the green one
 * (36,36)-(36,60)-(60,36) counter-clockwise. With front face CW (0x900) BACK culling drops green. */
static void cull_state(uint32_t enable, uint32_t face, uint32_t winding)
{
    pb(0x0308, enable);
    pb(0x039C, face);
    pb(0x03A0, winding);
}
static void cull_pair(uint32_t vb, float ox, float oy, float s)
{
    put_vertex(vb, 0, ox + 4 * s, oy + 4 * s, RED, 0, 0);
    put_vertex(vb, 1, ox + 28 * s, oy + 4 * s, RED, 0, 0);
    put_vertex(vb, 2, ox + 4 * s, oy + 28 * s, RED, 0, 0);
    put_vertex(vb, 3, ox + 36 * s, oy + 36 * s, GREEN, 0, 0);
    put_vertex(vb, 4, ox + 36 * s, oy + 60 * s, GREEN, 0, 0);
    put_vertex(vb, 5, ox + 60 * s, oy + 36 * s, GREEN, 0, 0);
    pb_vertex_format(vb, 0);
    pb_draw_triangles(6);
}
/* Pixels of the region that equal `want`, and those that differ from the clear colour. */
static void cull_region(const uint8_t *p, int x0, int y0, int x1, int y1, uint32_t want,
                        unsigned *match, unsigned *nonclear)
{
    int x, y;
    *match = *nonclear = 0;
    for (y = y0; y < y1; y++) for (x = x0; x < x1; x++) {
        uint32_t v = px32(p, 64, x, y);
        if (v == want) (*match)++;
        if (v != CLR) (*nonclear)++;
    }
}
static int run_cull(void)
{
    static const struct { uint32_t e, f, w; int l, r; const char *name; } cases[] = {
        { 0, 0x405, 0x900, 1, 1, "culling off" },
        { 1, 0x405, 0x900, 1, 0, "BACK, front CW: CW kept, CCW culled" },
        { 1, 0x404, 0x900, 0, 1, "FRONT, front CW: CW culled, CCW kept" },
        { 1, 0x405, 0x901, 0, 1, "BACK, front CCW: CW culled, CCW kept" },
        { 1, 0x404, 0x901, 1, 0, "FRONT, front CCW: CW kept, CCW culled" },
        { 1, 0x408, 0x900, 0, 0, "FRONT_AND_BACK: both culled" }
    };
    static const int slot[6] = { N_CULL0, N_CULL1, N_CULL2, N_CULL3, N_CULL4, N_CULL5 };
    int ok = 1, i;
    for (i = 0; i < 6; i++) {
        uint32_t surf = SURF(slot[i]), vb = VBUF(slot[i]);
        Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
        unsigned lm, ln, rm, rn;
        uint8_t *out;
        int good;
        pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_clear(CLR);
        cull_state(cases[i].e, cases[i].f, cases[i].w); cull_pair(vb, 0, 0, 1.0f); pb_flip();
        prefill_va(surf, SURF_BYTES, SENTINEL);
        if (!submit(cases[i].name) || !(out = snapshot(&s))) { ok = 0; continue; }
        cull_region(out, 0, 0, 32, 32, RED, &lm, &ln);
        cull_region(out, 32, 32, 64, 64, GREEN, &rm, &rn);
        good = !((cases[i].l && lm < 200) || (!cases[i].l && ln != 0)
              || (cases[i].r && rm < 200) || (!cases[i].r && rn != 0));
        printf("      cull %-40s CW triangle %u/%u, CCW triangle %u/%u  %s\n", cases[i].name, lm, ln, rm, rn,
               good ? "ok" : "FAIL");
        ok &= good;
        free(out);
    }
    /* State changes on one target between draws: each quadrant is a pair at half scale. */
    {
        uint32_t surf = SURF(N_CULL6), vb = VBUF(N_CULL6);
        Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
        uint8_t *out; unsigned m, n; int q, good = 1;
        pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_clear(CLR);
        cull_state(1, 0x405, 0x900); cull_pair(vb + 0x000, 0, 0, 0.5f);
        cull_state(1, 0x404, 0x900); cull_pair(vb + 0x400, 32, 0, 0.5f);
        cull_state(0, 0x405, 0x900); cull_pair(vb + 0x800, 0, 32, 0.5f);
        cull_state(1, 0x405, 0x901); cull_pair(vb + 0xC00, 32, 32, 0.5f); pb_flip();
        prefill_va(surf, SURF_BYTES, SENTINEL);
        if (!submit("cull state changes") || !(out = snapshot(&s))) ok = 0;
        else {
            for (q = 0; q < 4; q++) {
                int ox = (q & 1) ? 32 : 0, oy = (q & 2) ? 32 : 0;
                cull_region(out, ox, oy, ox + 16, oy + 16, RED, &m, &n);
                if (q == 0 && (m < 40 || n == 0)) good = 0;
                if (q == 1 && n != 0) good = 0;
                if (q == 2 && m < 40) good = 0;
                if (q == 3 && n != 0) good = 0;
                cull_region(out, ox + 16, oy + 16, ox + 32, oy + 32, GREEN, &m, &n);
                if (q == 0 && n != 0) good = 0;
                if (q == 1 && m < 40) good = 0;
                if (q == 2 && m < 40) good = 0;
                if (q == 3 && m < 40) good = 0;
            }
            printf("      cull %-40s %s\n", "state changed between draws", good ? "ok" : "FAIL");
            ok &= good;
            free(out);
        }
    }
    return ok;
}

/* ── Depth and stencil ───────────────────────────────────────────────────── */
/* SET_CLIP_MAX is the depth format's full scale, so vertex z is in 24 bit units. */
static void pb_zeta(uint32_t zeta)
{
    pb(0x0214, zeta);
    pb(0x0394, f2u(0.0f));
    pb(0x0398, f2u(16777215.0f));
}
#define ZNEAR 4000000.0f
#define ZMID  6000000.0f
#define ZFAR  8000000.0f
#define ZFARTHER 12000000.0f

/* Depth. `who` names the executor; with gate == 0 the result is information only (the CPU path).
 *   top left     test LESS, write OFF: blue at ZNEAR leaves the depth alone, so yellow at ZMID wins
 *   top right    test LESS, write ON:  blue at ZNEAR is kept against yellow at ZMID
 *   bottom left  test LESS, write ON:  red at ZFAR is kept against green at ZFARTHER
 *   bottom right test OFF, write ON (the back end turns it into ALWAYS/write): blue at ZNEAR still
 *                writes depth, so yellow at ZMID under test LESS is rejected */
static int run_depth_order(const char *who, int gate)
{
    uint32_t surf = SURF(N_DEPTH1), vb = VBUF(N_DEPTH1), zeta = ZETA(N_DEPTH1);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    uint8_t *px;
    int ok = 1;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_zeta(zeta);
    pb(0x1D8C, 0xFFFFFF00u);                        /* ZSTENCIL_CLEAR_VALUE: far, stencil 0 */
    pb_clear_flags(CLR, 0xF3u);
    pb(0x030C, 1); pb(0x0354, 0x201u);              /* depth test LESS */
    pb(0x035C, 0);
    pb_quad(vb + 0x000, 0, 0, 32, 32, ZNEAR, BLUE);
    pb(0x035C, 1);
    pb_quad(vb + 0x200, 0, 0, 32, 32, ZMID, YELLOW);
    pb_quad(vb + 0x400, 32, 0, 64, 32, ZNEAR, BLUE);
    pb_quad(vb + 0x600, 32, 0, 64, 32, ZMID, YELLOW);
    pb_quad(vb + 0x800, 0, 32, 32, 64, ZFAR, RED);
    pb_quad(vb + 0xA00, 0, 32, 32, 64, ZFARTHER, GREEN);
    pb(0x030C, 0);                                  /* test off, write on */
    pb_quad(vb + 0xC00, 32, 32, 64, 64, ZNEAR, BLUE);
    pb(0x030C, 1);
    pb_quad(vb + 0xE00, 32, 32, 64, 64, ZMID, YELLOW);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok &= submit(who);
    if (!(px = snapshot(&s))) return 0;
    printf("    depth ordering (%s)%s\n", who, gate ? "" : " -- information only");
    {
        int g = 1;
        g &= expect32("LESS, write off: later draw wins", px, 64, 0, 0, 32, 32, YELLOW);
        g &= expect32("LESS, write on: nearer draw kept", px, 64, 32, 0, 64, 32, BLUE);
        g &= expect32("LESS, write on: farther draw rejected", px, 64, 0, 32, 32, 64, RED);
        g &= expect32("test off + write on still writes depth", px, 64, 32, 32, 64, 64, BLUE);
        if (!gate && !g) printf("    (%s differs from the contract; not gated)\n", who);
        if (gate) ok &= g;
    }
    free(px);
    return gate ? ok : 1;
}

/* A depth clear uses an ALWAYS test regardless of the depth state, and writes the clear value:
 * with the clear at z = 0.25 (zstencil 0x40000000), LESS rejects ZMID and accepts 2e6. */
static int run_depth_clear(const char *who, int gate)
{
    uint32_t surf = SURF(N_DEPTH2), vb = VBUF(N_DEPTH2), zeta = ZETA(N_DEPTH2);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    uint8_t *px;
    int ok = 1, g = 1;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_zeta(zeta);
    pb(0x1D8C, 0x40000000u);                        /* depth 0x400000 = 0.25 of 24 bit, stencil 0 */
    pb(0x030C, 1); pb(0x0354, 0x200u); pb(0x035C, 0);   /* test NEVER, write off: the clear must ignore this */
    pb_clear_flags(CLR, 0xF3u);
    pb(0x0354, 0x201u);                             /* LESS */
    pb_quad(vb + 0x000, 0, 0, 32, 64, ZMID, RED);          /* 6e6 > 4.19e6: rejected */
    pb_quad(vb + 0x200, 32, 0, 64, 64, 2000000.0f, GREEN); /* 2e6 < 4.19e6: accepted */
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok &= submit(who);
    if (!(px = snapshot(&s))) return 0;
    printf("    depth clear value (%s)%s\n", who, gate ? "" : " -- information only");
    g &= expect32("quad behind the cleared depth rejected", px, 64, 0, 0, 32, 64, CLR);
    g &= expect32("quad in front of the cleared depth drawn", px, 64, 32, 0, 64, 64, GREEN);
    if (!gate && !g) printf("    (%s differs from the contract; not gated)\n", who);
    free(px);
    return gate ? (ok & g) : 1;
}

static void stencil_state(uint32_t func, uint32_t ref, uint32_t zpass)
{
    pb(0x032C, 1);
    pb(0x0364, func);
    pb(0x0368, ref);
    pb(0x036C, 0xFFu);
    pb(0x0360, 0xFFu);
    pb(0x0370, 0x1E00u);
    pb(0x0374, 0x1E00u);
    pb(0x0378, zpass);
}

/* Stencil counting without writing colour, then EQUAL passes that colour by count.
 *   x 0..31 INCR, x 16..47 INCR   -> counts 1 | 2 | 1 over x 0..15 | 16..31 | 32..47, 0 beyond
 *   x 0..15 DECR, x 48..63 DECR   -> 0 | 2 | 1 | 0 (the DECR at 0 must clamp, not wrap) */
static int run_stencil_counts(void)
{
    uint32_t surf = SURF(N_STENCIL1), vb = VBUF(N_STENCIL1), zeta = ZETA(N_STENCIL1);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    uint8_t *px;
    int ok, g = 1;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_zeta(zeta);
    pb(0x1D8C, 0xFFFFFF00u);
    pb_clear_flags(CLR, 0xF3u);
    pb(0x0358, 0);                                  /* colour mask off while counting */
    stencil_state(0x207u, 0, 0x1E02u);              /* ALWAYS, INCR on pass */
    pb_quad(vb + 0x000, 0, 0, 32, 64, 0.5f, RED);
    pb_quad(vb + 0x200, 16, 0, 48, 64, 0.5f, RED);
    pb(0x0378, 0x1E03u);                            /* DECR */
    pb_quad(vb + 0x400, 0, 0, 16, 64, 0.5f, RED);
    pb_quad(vb + 0x600, 48, 0, 64, 64, 0.5f, RED);
    pb(0x0358, 0x01010101u);
    stencil_state(0x202u, 1, 0x1E00u);              /* EQUAL 1 */
    pb_quad(vb + 0x800, 0, 0, 64, 64, 0.5f, GREEN);
    pb(0x0368, 2);
    pb_quad(vb + 0xA00, 0, 0, 64, 64, 0.5f, RED);
    pb(0x0368, 0);
    pb_quad(vb + 0xC00, 0, 0, 64, 64, 0.5f, BLUE);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok = submit("stencil counts");
    if (!(px = snapshot(&s))) return 0;
    printf("    stencil INCR / DECR / EQUAL\n");
    g &= expect32("x 0..15: 1 then DECR -> 0 (EQUAL 0)", px, 64, 0, 0, 16, 64, BLUE);
    g &= expect32("x 16..31: INCR twice -> 2 (EQUAL 2)", px, 64, 16, 0, 32, 64, RED);
    g &= expect32("x 32..47: INCR once -> 1 (EQUAL 1)", px, 64, 32, 0, 48, 64, GREEN);
    g &= expect32("x 48..63: DECR at 0 clamps (EQUAL 0)", px, 64, 48, 0, 64, 64, BLUE);
    free(px);
    return ok & g;
}

/* Stencil applies only with a depth buffer: NEVER draws nothing with a zeta, everything without. */
static int run_stencil_needs_depth(void)
{
    uint32_t surf = SURF(N_STENCIL2), vb = VBUF(N_STENCIL2), zeta = ZETA(N_STENCIL2);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    uint8_t *px;
    int ok, g = 1;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_zeta(zeta);
    pb(0x1D8C, 0xFFFFFF00u);
    pb_clear_flags(CLR, 0xF3u);
    stencil_state(0x200u, 0, 0x1E00u);              /* NEVER */
    pb_quad(vb + 0x000, 32, 0, 64, 64, 0.5f, RED);  /* with the zeta set: rejected */
    pb(0x0214, 0);                                  /* no depth buffer: the stencil state is ignored */
    pb_quad(vb + 0x200, 0, 0, 32, 64, 0.5f, GREEN);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok = submit("stencil without depth");
    if (!(px = snapshot(&s))) return 0;
    printf("    stencil needs a depth buffer\n");
    g &= expect32("no zeta: stencil NEVER ignored, drawn", px, 64, 0, 0, 32, 64, GREEN);
    g &= expect32("zeta set: stencil NEVER rejects", px, 64, 32, 0, 64, 64, CLR);
    free(px);
    return ok & g;
}

/* The clear writes the low 8 bits of the clear value to the stencil, whatever the stencil state. */
static int run_stencil_clear_value(void)
{
    uint32_t surf = SURF(N_STENCIL3), vb = VBUF(N_STENCIL3), zeta = ZETA(N_STENCIL3);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    uint8_t *px;
    int ok, g = 1;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_zeta(zeta);
    pb(0x1D8C, 0xFFFFFF05u);                        /* stencil clear value 5 */
    stencil_state(0x200u, 0, 0x1E00u);              /* NEVER: the clear must ignore it */
    pb_clear_flags(CLR, 0xF3u);
    stencil_state(0x202u, 5, 0x1E00u);              /* EQUAL 5 */
    pb_quad(vb + 0x000, 0, 0, 32, 64, 0.5f, GREEN);
    pb(0x0368, 4);                                  /* EQUAL 4 */
    pb_quad(vb + 0x200, 32, 0, 64, 64, 0.5f, RED);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok = submit("stencil clear value");
    if (!(px = snapshot(&s))) return 0;
    printf("    stencil clear value\n");
    g &= expect32("EQUAL 5 after clearing to 5 passes", px, 64, 0, 0, 32, 64, GREEN);
    g &= expect32("EQUAL 4 after clearing to 5 fails", px, 64, 32, 0, 64, 64, CLR);
    free(px);
    return ok & g;
}

/* ── 16 bit targets ──────────────────────────────────────────────────────── */
/* Load expands R5G6B5 and flush truncates it: guest memory holding arbitrary 565 words, a frame that
 * draws one small triangle and flips must give back every pixel it did not draw unchanged. */
static int run_roundtrip16(void)
{
    uint32_t surf = SURF(N_ROUNDTRIP), vb = VBUF(N_ROUNDTRIP);
    Surf s = { surf, 64, 64, 2, 0, 0, 0, 0 };
    uint32_t seed = 12345u, k;
    uint16_t *src = (uint16_t *)malloc(64 * 64 * 2);
    uint8_t *px;
    unsigned bad = 0;
    int x, y, ok;

    if (!src) return 0;
    for (k = 0; k < 64u * 64u; k++) {
        seed = seed * 1664525u + 1013904223u;
        src[k] = (uint16_t)(seed >> 16);
    }
    memcpy(G(surf), src, 64 * 64 * 2);
    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 2);
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 50.0f, 50.0f, RED, 0, 0);
    put_vertex(vb, 1, 60.0f, 50.0f, RED, 0, 0);
    put_vertex(vb, 2, 50.0f, 60.0f, RED, 0, 0);
    pb_draw_triangles(3);
    pb_flip();
    ok = submit("16bpp round trip");
    if (!(px = snapshot(&s))) { free(src); return 0; }
    for (y = 0; y < 64; y++) for (x = 0; x < 64; x++)
        if (!(x >= 48 && x < 62 && y >= 48 && y < 62) && px16(px, 64, x, y) != src[y * 64 + x]) {
            if (!bad) fprintf(stderr, "      (%d,%d) is 0x%04X, was 0x%04X\n", x, y, px16(px, 64, x, y), src[y * 64 + x]);
            bad++;
        }
    printf("      %-44s %s (%u pixels changed)\n", "untouched 565 words survive load + flush", bad ? "FAIL" : "ok", bad);
    ok &= bad == 0;
    if (px16(px, 64, 52, 52) != 0xF800u) {
        fprintf(stderr, "FAIL: drawn pixel is 0x%04X, want 0xF800\n", px16(px, 64, 52, 52));
        ok = 0;
    } else printf("      %-44s ok\n", "drawn pixel truncated to 5:6:5 (0xF800)");
    free(px); free(src);
    return ok;
}

/* The CPU executor applies the alpha test only on its transform path, not to the pre-transformed
 * batches these cases use (its reference draws every triangle and texel), so the alpha tests are
 * absolute: GREATER 0x80 on the diffuse alpha, then on the texel alpha times the diffuse. */
static int run_alpha_tests(void)
{
    int ok = 1, x, y;
    uint8_t *px;
    {
        Scene *sc = &g_scene[PC_ALPHA_TRI];
        unsigned dropped = 0, kept = 0, other = 0;
        build_alpha_tri(1);
        prefill(sc);
        ok &= submit("alpha test, untextured");
        if (!(px = snapshot(&sc->surf[0]))) return 0;
        for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
            uint32_t v = px32(px, 64, x, y);
            if (v == 0xC000FF00u) kept++;
            else if (v != CLR) other++;
            if (v == 0x40FF0000u) dropped++;
        }
        {
            int g = dropped == 0 && other == 0 && kept >= 259 && kept <= 317;
            printf("      %-44s %s (%u alpha-0x40 pixels, %u alpha-0xC0 pixels of ~288, %u others)\n",
                   "untextured: alpha 0x40 dropped, 0xC0 kept", g ? "ok" : "FAIL", dropped, kept, other);
            ok &= g;
        }
        free(px);
    }
    {
        Scene *sc = &g_scene[PC_TEX_ALPHA];
        unsigned bad = 0, vis = 0;
        build_tex_alpha(1);
        prefill(sc);
        ok &= submit("alpha test, textured");
        if (!(px = snapshot(&sc->surf[0]))) return 0;
        for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
            uint32_t v = px32(px, 64, x, y);
            int tx = x - 8, ty = y - 8;
            if (tx >= 0 && tx < 16 && ty >= 0 && ty < 16 && !((((tx >> 2) + (ty >> 2)) & 1) != 0)) {
                uint32_t want = (0xFFu << 24) | ((uint32_t)(60 + tx * 12) << 16) | ((uint32_t)(60 + ty * 12) << 8)
                              | (uint32_t)(220 - tx * 6 - ty * 6);
                int a[4], b[4];
                unpack((const uint8_t *)&v, 4, a); unpack((const uint8_t *)&want, 4, b);
                vis++;
                if (maxdiff(a, b, 1) > 2) bad++;
            } else if (v != CLR) bad++;
        }
        printf("      %-44s %s (%u opaque texels visible of 128, %u wrong pixels)\n",
               "textured: texels with alpha 0x30 discarded", (bad || vis != 128) ? "FAIL" : "ok", vis, bad);
        ok &= bad == 0 && vis == 128;
        free(px);
    }
    return ok;
}

/* A 16 bit target has no alpha, so a blend that reads destination alpha reads 1 -- on the CPU path --
 * and a draw with alpha 0 must not leave a 0 behind: red, then red * DST_ALPHA stays red.
 * `cleared`: the target's first use in the frame is a full colour clear (which skips the load), as a
 * title's frame starts; otherwise the target is loaded from guest memory. */
static int run_alpha16_variant(const char *who, int cleared, uint32_t surf, uint32_t vb)
{
    Surf s = { surf, 64, 64, 2, 0, 0, 0, 0 };
    uint8_t *px;
    unsigned bad = 0;
    int x, y;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 2);
    if (cleared) pb_clear(0x00000000u);
    pb_quad(vb + 0x000, 8, 8, 40, 40, 0.5f, 0x00FF0000u);    /* red, alpha 0, blend off */
    pb(0x0304, 1); pb(0x0344, 0x0000u); pb(0x0348, 0x0304u);   /* blend: src ZERO, dst DST_ALPHA */
    pb_quad(vb + 0x200, 8, 8, 40, 40, 0.5f, 0xFF00FF00u);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    if (!submit(who) || !(px = snapshot(&s))) return -1;
    for (y = 8; y < 40; y++) for (x = 8; x < 40; x++) if (px16(px, 64, x, y) != 0xF800u) bad++;
    if (bad && !g_info) fprintf(stderr, "      (%s, %s) pixel (20,20) is 0x%04X, want 0xF800\n", who,
                                cleared ? "after a clear" : "loaded", px16(px, 64, 20, 20));
    free(px);
    return (int)bad;
}
static int run_alpha16(const char *who, int gate)
{
    int ok = 1, bad_loaded, bad_cleared;

    bad_loaded = run_alpha16_variant(who, 0, SURF(N_ALPHA16B), VBUF(N_ALPHA16B));
    bad_cleared = run_alpha16_variant(who, 1, SURF(N_ALPHA16), VBUF(N_ALPHA16));
    printf("      %-44s %s (%d of 1024 pixels not 0xF800)%s\n", "alpha 0 draw, DST_ALPHA blend, loaded target",
           bad_loaded ? (g_info ? "differs" : "FAIL") : "ok", bad_loaded, gate ? "" : " -- information only");
    if (gate) ok &= bad_loaded == 0;
    /* After a full clear the image keeps its zero fill (the clear skips the load), so a destination-alpha
     * blend reads 0 where the CPU path reads 1: a known difference, reported, not gated. */
    printf("      %-44s %s (%d of 1024 pixels not 0xF800)%s\n", "same, target cleared first (CPU reads dst alpha 1)",
           bad_cleared ? (gate ? "XFAIL" : "differs") : (gate ? "XPASS" : "ok"), bad_cleared,
           gate ? " -- known difference, not gated" : " -- information only");
    return ok;
}

/* ── Target management ───────────────────────────────────────────────────── */
/* Same address, taller surface: the old target is written back and the new one reloaded from guest
 * memory, so the first frame's blue top half survives the second, taller target. */
static int run_reshape(void)
{
    uint32_t surf = SURF(N_RESHAPE), vb = VBUF(N_RESHAPE);
    Surf s = { surf, 64, 64, 4, 0, 0, 0, 0 };
    uint8_t *px;
    unsigned lowbad = 0, green = 0;
    int ok, g = 1, x, y;

    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 32, 4);
    pb_clear(BLUE);
    pb_surface(surf, 64, 64, 4);                    /* taller: same pitch, same address */
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 8.0f, 36.0f, GREEN, 0, 0);
    put_vertex(vb, 1, 56.0f, 40.0f, GREEN, 0, 0);
    put_vertex(vb, 2, 20.0f, 62.0f, GREEN, 0, 0);
    pb_draw_triangles(3);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok = submit("reshape");
    if (!(px = snapshot(&s))) return 0;
    g &= expect32("rows 0..31 keep the first target's clear", px, 64, 0, 0, 64, 32, BLUE);
    for (y = 32; y < 64; y++) for (x = 0; x < 64; x++) {
        uint32_t v = px32(px, 64, x, y);
        if (v == GREEN) green++;
        else if (v != SENTINEL32) lowbad++;
    }
    printf("      %-44s %s (%u pixels neither green nor guest memory, %u green; triangle is ~%.0f)\n",
           "lower half: triangle over reloaded guest memory", (lowbad || green < 400) ? "FAIL" : "ok",
           lowbad, green, fabs(0.5 * (48.0 * 26.0 - 4.0 * 12.0)));
    g &= lowbad == 0 && green >= 400;
    free(px);
    return ok & g;
}

/* Seventeen targets drawn in one frame: the least recently used is evicted (and must be written
 * back) before the flip, whatever the cache size. */
static int run_eviction(void)
{
    int ok = 1, i, bad = 0;
    uint8_t *px;
    pb_begin(); pb_reset_state();
    for (i = 0; i < 17; i++) {
        uint32_t va = EVICT_BASE + (uint32_t)i * 0x1000u;
        prefill_va(va, 0x1000, SENTINEL);
        pb_surface(va, 16, 16, 4);
        pb_clear(0xFF000000u | ((uint32_t)(i + 1) * 0x0D0B09u));
    }
    pb_flip();
    ok &= submit("eviction");
    for (i = 0; i < 17; i++) {
        uint32_t va = EVICT_BASE + (uint32_t)i * 0x1000u;
        char what[48];
        px = snapshot_va(va, 16 * 16 * 4);
        if (!px) { ok = 0; continue; }
        _snprintf(what, sizeof what, "target %d of 17", i);
        if (rect_bad32(what, px, 16, 0, 0, 16, 16, 0xFF000000u | ((uint32_t)(i + 1) * 0x0D0B09u))) bad++;
        free(px);
    }
    printf("      %-44s %s (%d of 17 targets wrong)\n", "all 17 targets written back", bad ? "FAIL" : "ok", bad);
    return ok && bad == 0;
}

/* Vertices with NaN or infinite coordinates are dropped; the good triangles around them still draw
 * and nothing is drawn from the bad ones. (The executor may drop some before the back end sees
 * them; either way no red may appear.) */
static int run_nonfinite(void)
{
    uint32_t surf = SURF(N_NONFINITE), vb = VBUF(N_NONFINITE);
    Surf s = { surf, 64, 64, 4, CLR, 0, 0, 0 };
    const uint32_t nanbits = 0x7FC00000u, infbits = 0x7F800000u;
    float nanf, inff;
    uint8_t *px;
    unsigned red = 0, green = 0;
    int ok, x, y;

    memcpy(&nanf, &nanbits, 4); memcpy(&inff, &infbits, 4);
    pb_begin(); pb_reset_state(); pb_surface(surf, 64, 64, 4); pb_clear(CLR);
    pb_vertex_format(vb, 0);
    put_vertex(vb, 0, 4.0f, 4.0f, GREEN, 0, 0);
    put_vertex(vb, 1, 28.0f, 4.0f, GREEN, 0, 0);
    put_vertex(vb, 2, 4.0f, 28.0f, GREEN, 0, 0);
    put_vertex(vb, 3, nanf, 10.0f, RED, 0, 0);
    put_vertex(vb, 4, 60.0f, 10.0f, RED, 0, 0);
    put_vertex(vb, 5, 40.0f, 40.0f, RED, 0, 0);
    put_vertex(vb, 6, 36.0f, inff, RED, 0, 0);
    put_vertex(vb, 7, 60.0f, 36.0f, RED, 0, 0);
    put_vertex(vb, 8, 36.0f, 60.0f, RED, 0, 0);
    put_vertex(vb, 9, 36.0f, 36.0f, GREEN, 0, 0);
    put_vertex(vb, 10, 60.0f, 36.0f, GREEN, 0, 0);
    put_vertex(vb, 11, 36.0f, 60.0f, GREEN, 0, 0);
    pb_draw_triangles(12);
    pb_flip();
    prefill_va(surf, SURF_BYTES, SENTINEL);
    ok = submit("non-finite vertices");
    if (!(px = snapshot(&s))) return 0;
    for (y = 0; y < 64; y++) for (x = 0; x < 64; x++) {
        uint32_t v = px32(px, 64, x, y);
        if (v == RED) red++;
        if (v == GREEN) green++;
    }
    printf("      %-44s %s (%u red, %u green; two good triangles are ~%.0f)\n", "bad triangles dropped, good ones drawn",
           (red || green < 500) ? "FAIL" : "ok", red, green, 0.5 * 24 * 24 + 0.5 * 24 * 24);
    free(px);
    return ok && !red && green >= 500;
}

/* ── Register-combiner batches: a documented known difference ────────────── */
/* The CPU executor evaluates combiners only on its vertex-program path, so this runs in program mode
 * with a pass-through vertex program. The Vulkan back end draws combiner batches as stage 0 x diffuse,
 * so a combiner that does anything else -- here a lerp through the final combiner -- differs from
 * the CPU reference in colour (XFAIL) while the triangle still lands in the same place (gated).
 * It runs last in each pass: programming the combiners sticks in the executor for good. */
#define RC_STRIDE 36u    /* pos@0 diffuse@12 specular@16 uv0@20 uv1@28 */
#define RC_ZERO 0x00u
#define RC_C0   0x01u
#define RC_V0   0x04u
#define RC_T0   0x08u
#define RC_R0   0x0Cu
#define RC_A(r) ((r) | 0x10u)

static void vsh_mov(uint32_t addr, uint32_t in, int final)
{
    uint32_t w[4] = { 0, 0, 0, 0 }, k;
    w[1] = (1u << 21) | (in << 9) | 0x1Bu;          /* mov, input index, swizzle xyzw */
    w[2] = (2u << 26);                               /* A source mux: input */
    w[3] = (0xFu << 12) | (1u << 11) | (addr << 3) | (final ? 1u : 0u);
    for (k = 0; k < 4; k++) pb(0x0B00u + 4u * k, w[k]);
}
static void build_combiner(void)
{
    const uint32_t vb = VBUF(N_XFAIL), col = 0u | (4u << 4) | (RC_STRIDE << 8), f2 = 2u | (2u << 4) | (RC_STRIDE << 8);
    const uint32_t diffuse = 0xFF2080E0u;
    int i;
    pb_begin(); pb_reset_state();
    pb_surface(SURF(N_XFAIL), 64, 64, 4);
    pb_clear(CLR);
    pb_texture_on(TEX_MAG, 0x11, 8, 8, 16, FILTER_NEAREST);
    pb(0x1720 + 0 * 4, vb);        pb(0x1760 + 0 * 4, 2u | (3u << 4) | (RC_STRIDE << 8));
    pb(0x1720 + 3 * 4, vb + 12);   pb(0x1760 + 3 * 4, col);
    pb(0x1720 + 4 * 4, vb + 16);   pb(0x1760 + 4 * 4, col);
    pb(0x1720 + 9 * 4, vb + 20);   pb(0x1760 + 9 * 4, f2);
    pb(0x1720 + 10 * 4, vb + 28);  pb(0x1760 + 10 * 4, f2);
    pb(0x1E94, 2u);                                  /* TRANSFORM_EXECUTION_MODE: program */
    pb(0x1E9C, 0u);                                  /* PROGRAM_LOAD slot 0 */
    pb(0x1EA0, 0u);                                  /* PROGRAM_START slot 0 */
    vsh_mov(0, 0, 0); vsh_mov(3, 3, 0); vsh_mov(4, 4, 0); vsh_mov(9, 9, 0); vsh_mov(10, 10, 1);
    pb(0x1E70, 1u);                                  /* SHADER_STAGE_PROGRAM: stage 0 = 2D */
    pb(0x1E60, 1u);                                  /* COMBINER_CONTROL: 1 stage */
    pb(0x0A60, 0x80808080u); pb(0x0A80, 0x80808080u);
    pb(0x1E20, 0x80808080u); pb(0x1E24, 0x80808080u);
    pb(0x0AC0, (RC_T0 << 24) | (RC_V0 << 16));       /* stage 0 colour: t0 * v0 -> r0 */
    pb(0x0260, (RC_A(RC_T0) << 24) | (RC_A(RC_V0) << 16));
    pb(0x1E40, RC_R0 << 4);
    pb(0x0AA0, RC_R0 << 4);
    /* final = A*B + (1-A)*C with A = c0 = 0.5, B = t0, C = v0: half of each, not their product */
    pb(0x0288, (RC_C0 << 24) | (RC_T0 << 16) | (RC_V0 << 8) | RC_ZERO);
    pb(0x028C, RC_A(RC_V0) << 8);
    for (i = 0; i < 3; i++) {
        static const float px[3][2] = { { 8, 8 }, { 56, 8 }, { 8, 56 } };
        static const float uv[3][2] = { { 0, 0 }, { 8, 0 }, { 0, 8 } };
        uint8_t *p = G(vb + (uint32_t)i * RC_STRIDE);
        float *f = (float *)p;
        f[0] = px[i][0]; f[1] = px[i][1]; f[2] = 0.5f;
        *(uint32_t *)(p + 12) = diffuse;
        *(uint32_t *)(p + 16) = 0;
        f[5] = uv[i][0]; f[6] = uv[i][1]; f[7] = 0; f[8] = 0;
    }
    pb_draw_triangles(3);
    pb_flip();
}
static const Surf k_xfail_surf = { SURF(N_XFAIL), 64, 64, 4, CLR, 48 + 48 + 48 * 1.41421356, 0.5 * 48 * 48, 0 };

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *ref[NPAR][2], *got[NPAR][2], *xref = NULL, *xgot = NULL;
    uint8_t *pbmem;
    int ok = 1, c, i, k;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: gpu_backend_parity_vk -- %s\n", argv[2]);
        return 77;
    }
    pbmem = (uint8_t *)VirtualAlloc(NULL, 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    memset(ref, 0, sizeof ref); memset(got, 0, sizeof got);
    if (!ram || !ramin || !instance || !pbmem) { fprintf(stderr, "FAIL: allocation\n"); return 1; }
    g_pb = (uint32_t *)pbmem;

    /* Guest RAM: a synthetic, section-less XBE is enough for the memory layout. */
    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) { fprintf(stderr, "FAIL: xbox_MemoryLayoutInit\n"); return 1; }

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
    if (!nv2a_pb_exec_consumer_registered()) { fprintf(stderr, "FAIL: executor not registered\n"); return 1; }
    nv2a_backend_register(NULL);
    write_guest_data();

    /* (a) CPU references: every parity case, then the cases whose CPU result is informational. */
    puts("-- software executor --");
    for (i = 0; i < NPARITY; i++) {
        c = k_parity[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("software");
        for (k = 0; k < g_scene[c].nsurf; k++) ref[c][k] = snapshot(&g_scene[c].surf[k]);
    }
    k_build[PC_NEG](1);                              /* the negative control's reference */
    prefill(&g_scene[PC_NEG]);
    ok &= submit("software");
    ref[PC_NEG][0] = snapshot(&g_scene[PC_NEG].surf[0]);
    puts("-- clear-value contract (software executor) --");
    ok &= run_clear_contract("software");
    puts("-- information only: the software executor on the absolute cases --");
    g_info = 1;
    run_depth_order("software", 0);
    run_depth_clear("software", 0);
    run_alpha16("software", 0);
    g_info = 0;

    /* (b) The Vulkan back end, on the software driver. */
    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        puts("SKIP: gpu_backend_parity_vk -- the Vulkan back end could not be installed on a software device "
             "(no Vulkan, no matching device, or an elevated process: the loader ignores VK_DRIVER_FILES there)");
        return 77;
    }
    puts("-- Vulkan back end --");
    for (i = 0; i < NPARITY; i++) {
        c = k_parity[i];
        k_build[c](1);
        prefill(&g_scene[c]);
        printf(" case %s\n", g_scene[c].name);
        ok &= submit("vulkan (flipped)");
        for (k = 0; k < g_scene[c].nsurf; k++) got[c][k] = snapshot(&g_scene[c].surf[k]);
    }
    puts("-- parity --");
    {
        static const char *const tag[NPAR] = { "clear565", "tri8888", "tex-magnified", "tex-1:1", "tex-linear",
                                               "alpha-tri", "tex-alpha", "render-to-texture", "flush-before-sample", "" };
        for (i = 0; i < NPARITY; i++) {
            c = k_parity[i];
            if (c == PC_TEX_LIN) {
                if (!ref[c][0] || !got[c][0]) { ok = 0; continue; }
                ok &= check_linear(&g_scene[c].surf[0], ref[c][0], got[c][0]);
                continue;
            }
            for (k = 0; k < g_scene[c].nsurf; k++) {
                char label[64];
                _snprintf(label, sizeof label, "%s/%s", tag[c], k == 0 ? "target" : "source");
                if (!ref[c][k] || !got[c][k]) { ok = 0; continue; }
                ok &= check_parity(label, &g_scene[c].surf[k], ref[c][k], got[c][k], 1);
            }
        }
    }

    /* Negative control: without the flip the guest surface is untouched; the flip alone writes it. */
    puts("-- negative control (no flip) --");
    build_neg(0);
    prefill(&g_scene[PC_NEG]);
    ok &= submit("vulkan (no flip)");
    got[PC_NEG][0] = snapshot(&g_scene[PC_NEG].surf[0]);
    if (ref[PC_NEG][0] && got[PC_NEG][0])
        ok &= check_parity("neg/before-flip", &g_scene[PC_NEG].surf[0], ref[PC_NEG][0], got[PC_NEG][0], 0);
    pb_begin(); pb_flip();
    ok &= submit("vulkan (flip only)");
    {
        uint8_t *after = snapshot(&g_scene[PC_NEG].surf[0]);
        if (ref[PC_NEG][0] && after)
            ok &= check_parity("neg/after-flip", &g_scene[PC_NEG].surf[0], ref[PC_NEG][0], after, 1);
        free(after);
    }

    puts("-- clear-value contract (Vulkan back end) --");
    ok &= run_clear_contract("vulkan");
    puts("-- clear colour masks --");
    ok &= run_clear_masks();
    puts("-- clear confined to the clip rectangle --");
    ok &= run_clear_scissor();
    puts("-- alpha test --");
    ok &= run_alpha_tests();
    puts("-- face culling --");
    ok &= run_cull();
    puts("-- depth --");
    ok &= run_depth_order("vulkan", 1);
    ok &= run_depth_clear("vulkan", 1);
    puts("-- stencil --");
    ok &= run_stencil_counts();
    ok &= run_stencil_needs_depth();
    ok &= run_stencil_clear_value();
    puts("-- 16 bit targets --");
    ok &= run_roundtrip16();
    ok &= run_alpha16("vulkan", 1);
    puts("-- target management --");
    ok &= run_reshape();
    ok &= run_eviction();
    puts("-- non-finite vertices --");
    ok &= run_nonfinite();

    /* The combiner case: Vulkan first, then the CPU reference with the back end unregistered. */
    puts("-- Vulkan back end: register-combiner batch (known difference) --");
    build_combiner();
    prefill_va(k_xfail_surf.va, SURF_BYTES, SENTINEL);
    ok &= submit("vulkan (combiner)");
    xgot = snapshot(&k_xfail_surf);
    puts("-- software executor: register-combiner batch --");
    nv2a_backend_register(NULL);
    build_combiner();
    prefill_va(k_xfail_surf.va, SURF_BYTES, SENTINEL);
    ok &= submit("software (combiner)");
    xref = snapshot(&k_xfail_surf);
    if (xref && xgot) {
        Cmp cm = compare(&k_xfail_surf, xref, xgot);
        printf("  [combiner lerp] colour match %.2f%% (worst channel diff %d), coverage ref=%u got=%u (xor %u, allowed %.0f)\n",
               cm.all_pct, cm.worst, cm.cov_ref, cm.cov_got, cm.cov_diff, k_xfail_surf.perim);
        if (cm.cov_diff > k_xfail_surf.perim) {
            fprintf(stderr, "FAIL [combiner lerp]: the batch is not drawn where the CPU draws it (coverage xor %u > %.0f)\n",
                    cm.cov_diff, k_xfail_surf.perim);
            ok = 0;
        }
        if (cm.all_pct >= 98.0)
            puts("  XPASS [combiner lerp]: colours now match the CPU reference; the back end runs combiners, "
                 "so promote this case to a gating parity case");
        else
            puts("  XFAIL [combiner lerp]: colours differ, as documented (combiner batches are drawn as stage 0 x diffuse)");
    } else ok = 0;

    fflush(stdout);
    nv2a_set_commit_consumer(NULL);
    for (c = 0; c < NPAR; c++)
        for (i = 0; i < 2; i++) { free(ref[c][i]); free(got[c][i]); }
    free(xref); free(xgot);
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fprintf(stderr, "FAIL: gpu_backend_parity_vk\n"); return 1; }
    puts("PASS: gpu_backend_parity_vk (software and Vulkan agree after the flip)");
    return 0;
}
