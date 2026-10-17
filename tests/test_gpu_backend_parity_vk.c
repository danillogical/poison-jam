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
 *   - The register combiners, gated: this back end runs a combiner uber shader, so a batch whose
 *     combiners are programmed is compared with the CPU reference like any other case. One sweep
 *     of 45 constant-vertex cases compares the GPU's pixel against nv2a_rc_eval called directly
 *     with the same registers (the sharper oracle); four whole-surface cases (a four-stage batch
 *     with a linear and a swizzled texture, a batch with an unbound stage, the old final-combiner
 *     lerp, and one submission with three combiner draws plus a stage-0 fallback) are compared
 *     with the CPU executor. They run last in each pass: programming the combiners sticks in the
 *     executor for good (rc_seen never clears). The sweep's mux and final-combiner flag cases also
 *     state the value the oracle must produce (RcCase.want) and the cases that must differ from each
 *     other (RcCase.group), so a fixture that stops exercising its flag fails on its own. A last
 *     case drives the back end's draw contract directly with four distinguishable textures, which
 *     the executor cannot do (stage 3's texture methods are not admitted).
 *   - Randomized combiner states: 2 fixed seeds (0x20261013, 0x9E3779B9) x 256 cases = 512 whole
 *     random states, drawn from a splitmix64 stream seeded only by (seed, index), so a case is
 *     re-derivable from that pair alone and the printed inventory hash pins the whole list. Every
 *     state is valid under the contracts the two implementations share (a stage reads only registers
 *     an earlier stage wrote or the initialisation rule defines, stage modes 0-4, no true cube map,
 *     fog factor only 0 or 1, texture coordinates at half-texel centres), and a state whose oracle
 *     is not exactly reproducible is rejected and counted before it is drawn: the coverage block
 *     prints the accepted count, the rejection counts by reason, and the counters for input
 *     mappings, outputs, scale/bias, dot products, mux forms, register write/read ordering, stage
 *     counts 0-8, shared vs per-stage factors, the final combiner's forms, fog, and the texture
 *     stage modes -- each with a minimum, and a counter under its minimum FAILS the run. The
 *     randomized suite runs in BOTH passes like the sweep, so the oracle is itself checked against
 *     the executor before the GPU is compared with it. The oracle cannot model tent filtering
 *     (every randomized texture is FILTER_NEAREST), stage modes above 4 or a true cube map (both
 *     excluded, since the CPU reads zero for the former and the back end falls back for both), and
 *     the fog factor's only reachable values are 1 (fog off) and 0 (fog on), because SET_FOG_MODE
 *     and SET_FOG_PARAMS are not admitted methods. A third seed (0x51ED2701) drives a randomized
 *     block through the back end's draw contract instead of the pushbuffer, binding a solid texture
 *     to all four stages at once (the executor cannot bind stage 3 at all), Vulkan-only because the
 *     CPU pass has no registered back end; it has its own coverage minimums and worst-difference
 *     line.
 *
 * The test selects the CPU device with RECOMP_VK_DEVICE=llvmpipe, which CTest also sets together with
 * VK_DRIVER_FILES (cache variable XBR_VK_SW_ICD). It skips (exit 77) with a message when
 * XBR_VK_SW_ICD is unset, or when the back end cannot be installed -- notably in an elevated process,
 * where the Vulkan loader ignores VK_DRIVER_FILES. Run it non-elevated. */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_combiner.h"
#include "d3d/d3d8_swizzle.h"
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
#define TEX_RC1   0x00C30000u     /* 8x8 SZ_R5G6B5 swizzled (combiner cases) */
#define TEX_RC2   0x00C40000u     /* 8x8 SZ_A8R8G8B8 swizzled */
#define TEX_RC3   0x00C50000u     /* 8x8 SZ_R5G6B5 swizzled, a different pattern */
#define TEX_Q(n)  (0x00C60000u + (uint32_t)(n) * 0x00010000u)   /* four solid 8x8 SZ_A8R8G8B8 */
/* The randomized combiner suite's own solid textures: eight 8x8 A8R8G8B8 colours, each written once
 * in a linear range (stage 0, addressed in texels) and once in a swizzled range (stages 1-2, whose
 * coordinates arrive normalised). Two ranges because the back end's texture cache is keyed on the
 * address and the format: one address must not be seen as two formats. 0x00CA0000/0x00CB0000 are
 * above every TEX_Q() slot (which ends at 0x00C9FFFF) and below EVICT_BASE. */
#define TEX_RRL(n)  (0x00CA0000u + (uint32_t)(n) * 0x00001000u)
#define TEX_RRS(n)  (0x00CB0000u + (uint32_t)(n) * 0x00001000u)
#define EVICT_BASE 0x00D00000u    /* 17 targets of 16x16x4, 0x1000 apart */
/* The combiner cases own their surfaces and vertex buffers, apart from every SURF()/VBUF() slot. */
#define RCSURF(n) (0x00E00000u + (uint32_t)(n) * 0x00010000u)
#define RCVB(n)   (0x00F00000u + (uint32_t)(n) * 0x00010000u)
#define RC_SWEEP   0u
#define RC_S_LERP  1u
#define RC_S_FOUR  2u
#define RC_S_MULTI 4u
#define RC_S_PASS  5u
#define RC_S_CUBE  6u
#define RC_S_CLIP  7u
#define RC_S_TEX4  8u
#define RC_S_RAND  9u    /* the randomized sweep's target and vertex buffer (16 cells per submission) */
#define SURF_BYTES 0x00010000u    /* span pre-filled/cleared per surface */
#define SENTINEL  0x5Au
#define SENTINEL32 0x5A5A5A5Au

enum {                            /* SURF()/VBUF()/ZETA() slots */
    N_CLEAR565, N_TRI8888, N_TEX_MAG, N_TEX_ONE, N_TEX_LIN, N_ALPHA_TRI, N_TEX_ALPHA,
    N_RTT_A, N_RTT_B, N_FS_T, N_FS_U, N_NEG, N_ALPHA16, N_RESHAPE, N_ROUNDTRIP,
    N_DEPTH1, N_DEPTH2, N_STENCIL1, N_STENCIL2, N_STENCIL3, N_CMASK32, N_CMASK16,
    N_SCISSOR32, N_SCISSOR16, N_CULL0, N_CULL1, N_CULL2, N_CULL3, N_CULL4, N_CULL5, N_CULL6,
    N_NONFINITE, N_RC_LERP, N_CLEARC16, N_CLEARC32, N_ALPHA16B, N_NSLOTS
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
/* Stages 1-3 accept only the methods in the toolkit's NV097 admitted list, which omits their
 * CONTROL1 (pitch) and IMAGE_RECT; so a second texture has to be swizzled, sized by log2 in the
 * format word. (It omits 0x1BC0/0x1BC4 as well, so stage 3's OFFSET and FORMAT cannot be written
 * at all: stage 3 can never be a usable texture.) */
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
static void pb_draw_triangles(uint32_t nverts)
{
    pb(0x17FC, 5u);                                /* BEGIN TRIANGLES */
    pb(0x1810, ((nverts - 1u) << 24) | 0u);        /* DRAW_ARRAYS from 0 */
    pb(0x17FC, 0u);                                /* END */
}
static void pb_draw_range(uint32_t start, uint32_t nverts)
{
    pb(0x17FC, 5u);                                /* BEGIN TRIANGLES */
    pb(0x1810, ((nverts - 1u) << 24) | start);     /* DRAW_ARRAYS from `start` */
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
    /* Two 8x8 swizzled textures (the Xbox default layout: stages 1-3 can only be bound
     * swizzled, since their IMAGE_RECT and CONTROL1 are not in the admitted method list).
     * The pattern is written linear and swizzled with the toolkit's own xbox_swizzle_rect,
     * so it is the layout the sampler reads. */
    {
        uint8_t lin[8 * 8 * 4];
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                *(uint16_t *)(lin + (y * 8 + x) * 2) = pack565(20 + x * 30, 230 - y * 28, 40 + (x ^ y) * 26);
        xbox_swizzle_rect(G(TEX_RC1), lin, 8, 8, 2);
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                *(uint32_t *)(lin + (y * 8 + x) * 4) = 0xFF000000u | ((240 - x * 26) << 16)
                                                     | ((30 + y * 28) << 8) | (200 - (x + y) * 20);
        xbox_swizzle_rect(G(TEX_RC2), lin, 8, 8, 4);
    }
    /* The randomized combiner suite binds exactly three of the images written above -- TEX_MAG
     * (linear R5G6B5, addressed in texels), TEX_RC1 (swizzled R5G6B5) and TEX_RC2 (swizzled
     * A8R8G8B8) -- so no new image is needed here and the deterministic cases keep their own. */
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
    fflush(stdout);
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

/* ── Register combiners ────────────────────────────────────────────────────
 * The back end runs a combiner uber shader, so a batch whose combiners are programmed is compared
 * with the CPU reference like any other case. Two references are used:
 *   - a sweep of one small quad per case, whose centre pixel is compared with nv2a_rc_eval called
 *     directly with the same register words. That is the sharper oracle: it can be driven to any
 *     input mapping byte, output scale/bias, dot-product flag, mux form and final-combiner form,
 *     which the executor cannot be steered to. The sweep is run against the CPU executor first, so
 *     the oracle itself is checked against the executor's own pixels.
 *   - whole-surface cases compared with the CPU executor.
 * The CPU executor evaluates the combiners only on its vertex-program path, so every case runs in
 * program mode with a constant pass-through vertex program. Programming the combiners sticks in the
 * executor for good (rc_seen never clears), so these cases run LAST in each pass, with the CPU
 * reference pass before the back end is installed.
 *
 * The target is A8R8G8B8 and the output is quantised to 8 bits per channel, so the comparison
 * tolerance is 1: the GPU evaluates the same float32 expression with a different instruction order
 * (one dot-product instruction where the CPU has three multiplies and two adds), which is worth at
 * most one unit in the last place before quantisation. The measured worst case is reported.
 */
#define RC_STRIDE 36u    /* pos@0 diffuse@12 specular@16 uv0@20 uv1@28 */
#define RC_ZERO 0x00u
#define RC_C0   0x01u
#define RC_C1   0x02u
#define RC_FOG  0x03u
#define RC_V0   0x04u
#define RC_V1   0x05u
#define RC_T0   0x08u
#define RC_T1   0x09u
#define RC_T2   0x0Au
#define RC_T3   0x0Bu
#define RC_R0   0x0Cu
#define RC_SUM  0x0Eu
#define RC_EF   0x0Fu
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
    const uint32_t vb = VBUF(N_RC_LERP), col = 0u | (4u << 4) | (RC_STRIDE << 8), f2 = 2u | (2u << 4) | (RC_STRIDE << 8);
    const uint32_t diffuse = 0xFF2080E0u;
    int i;
    pb_begin(); pb_reset_state();
    pb_surface(SURF(N_RC_LERP), 64, 64, 4);
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
static const Surf k_rc_lerp_surf = { SURF(N_RC_LERP), 64, 64, 4, CLR, 48 + 48 + 48 * 1.41421356, 0.5 * 48 * 48, 0 };

/* ── The sweep: the centre pixel of one quad per case vs nv2a_rc_eval ────── */
#define RC_RSTRIDE 92u    /* the back end's VbRcVertex: pos@0 rhw@12 d0@16 d1@20 fog@24 t0..t3@28 */
#define RC_RTOL    1

/* Constant per-vertex values: an attribute that is the same at all three vertices interpolates to
 * itself, so the oracle uses these numbers directly. */
#define RC_DIFFUSE  0xFF4060C0u    /* v0 = 0.251 0.376 0.753 1.0 */
#define RC_SPECULAR 0xFF204060u    /* v1 = 0.125 0.251 0.376 1.0 */
#define RC_FOGCOL   0x00003050u    /* SET_FOG_COLOR: R in bits 0-7, so rgb = 0.188 0.314 0.0 */

typedef struct {
    const char *what;          /* what this case decides */
    Nv2aCombiner rc;           /* the register words written, and what nv2a_rc_eval is given */
    uint32_t fog_en, fog_col;  /* SET_FOG_ENABLE / SET_FOG_COLOR */
    int has_want;              /* the oracle's own value is pinned: see want */
    int want[4];               /* r,g,b,a the label claims, 0..255; a negative entry is not checked */
    unsigned group;            /* bit mask: the oracle must differ from every other case sharing a bit */
} RcCase;

static RcCase k_rc[64];   /* one 8x8 cell of the sweep surface each */
static uint32_t k_rc_n;

static RcCase *rc_add(const char *what, uint32_t n, uint32_t flags, uint32_t prog)
{
    RcCase *c = &k_rc[k_rc_n++];
    memset(c, 0, sizeof *c);
    c->what = what;
    c->rc.control = (n & 0xFFu) | (flags << 8);
    c->rc.stage_program = prog;
    c->rc.final_c0 = 0xFFFFFFFFu;
    c->rc.final_c1 = 0x80808080u;
    c->rc.final1 = 0x001C00u;          /* alpha from R0.a, so a case can leave final0 to itself */
    c->fog_col = RC_FOGCOL;
    return c;
}
/* Pin the value nv2a_rc_eval must produce for this case (r,g,b,a, 0..255; negative: not checked), and
 * the groups (bit mask) of cases that must come out different from each other. */
static void rc_want(RcCase *c, unsigned group, int r, int g, int b, int a)
{
    c->has_want = 1;
    c->want[0] = r; c->want[1] = g; c->want[2] = b; c->want[3] = a;
    c->group = group;
}

static void rc_table(void)
{
    RcCase *c;
    uint32_t m;

    k_rc_n = 0;

    /* Input mappings 0..7: A carries the mapping and B the unsigned identity, so the sum output is
     * v0 mapped by m, times v0. Mappings 6 and 7 keep the negative values the others clamp away. */
    for (m = 0; m < 8; m++) {
        c = rc_add("input mapping", 1, 0, 0);
        c->rc.color_icw[0] = (RC_V0 | (m << 5)) << 24 | RC_V0 << 16;
        c->rc.color_ocw[0] = RC_R0 << 8;
        c->rc.final0 = RC_R0;
    }
    /* The 0x10 bit replicates .a in the RGB portion. */
    c = rc_add("the 0x10 bit makes the RGB portion read .aaa", 1, 0, 0);
    c->rc.color_icw[0] = (RC_V0 | 0x10u) << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.final0 = RC_R0;
    /* The alpha portion's 0x10 bit picks .a over .b; the alpha is observable because the final
     * combiner takes its alpha from r0.a (final1's G field, with the 0x10 bit set). */
    c = rc_add("alpha portion: the 0x10 bit picks .a (c0.a * v0.a)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.alpha_icw[0] = (RC_C0 | 0x10u) << 24 | (RC_V0 | 0x10u) << 16;
    c->rc.alpha_ocw[0] = RC_R0 << 4;
    c->rc.factor0[0] = 0x80000000u;
    c->rc.final0 = RC_R0;
    c = rc_add("alpha portion without 0x10: reads .b (c0.b * v0.b = 0)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.alpha_icw[0] = RC_C0 << 24 | RC_V0 << 16;
    c->rc.alpha_ocw[0] = RC_R0 << 4;
    c->rc.factor0[0] = 0x80000000u;
    c->rc.final0 = RC_R0;
    /* Every map_out scale/bias (the RGB output's OCW bits 0x38). */
    {
        static const uint32_t mo[6] = { 0x00u, 0x08u, 0x10u, 0x18u, 0x20u, 0x30u };
        for (m = 0; m < 6; m++) {
            c = rc_add("output scale/bias", 1, 0, 0);
            c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
            c->rc.color_ocw[0] = (mo[m] << 12) | (RC_R0 << 8);
            c->rc.final0 = RC_R0;
        }
    }
    /* Dot products: the AB portion, the CD portion, and both at once. */
    c = rc_add("dot product on A.B (fl & 2)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16 | RC_V0 << 8 | RC_V0;
    c->rc.color_ocw[0] = (0x02u << 12) | (RC_R0 << 8);
    c->rc.final0 = RC_R0;
    c = rc_add("dot product on C.D (fl & 1)", 1, 0, 0);
    c->rc.color_icw[0] = RC_ZERO << 24 | RC_V0 << 16 | RC_V0 << 8 | RC_V0;
    c->rc.color_ocw[0] = (0x01u << 12) | (RC_R0 << 8);
    c->rc.final0 = RC_R0;
    c = rc_add("dot products on both portions (fl & 3)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16 | RC_V0 << 8 | RC_C0;
    c->rc.color_ocw[0] = (0x03u << 12) | (RC_R0 << 8);
    c->rc.factor0[0] = 0x0000003Fu;
    c->rc.final0 = RC_R0;
    /* Sum vs mux, both mux forms, both selections. Stage 0 writes r0.a = c0.a * 1 (an inverted ZERO is
     * exactly 1), so r0.a is the 8-bit value k below and nothing else. Stage 1 then has A*B = v0*v0
     * and C*D = c1*v0, far apart, and its output word either adds them or, with the mux-enable bit
     * (OCW fl & 4, bits 14), picks one by the mux condition (control bit 0 clear: (int)(r0.a*255) & 1
     * selects CD; set: r0.a >= 0.5 selects CD). k = 65 is odd and below 0.5, k = 130 even and above,
     * so the two modes select opposite products at either k.
     *   sum      = 72,96,181      AB = 16,36,145      CD = 56,60,36
     * The final combiner outputs r0 (rgb from stage 1, alpha k). */
    {
        static const struct { const char *what; uint32_t flags, mux, k, group; int rgb[3]; } mx[5] = {
            { "no mux bit: stage 1 adds AB + CD (k = 65)", 0, 0, 65, 3, { 72, 96, 181 } },
            { "mux, LSB form (control bit 0 clear), r0.a = 65 odd: picks CD", 0, 1, 65, 1, { 56, 60, 36 } },
            { "mux, LSB form, r0.a = 130 even: picks AB", 0, 1, 130, 1, { 16, 36, 145 } },
            { "mux, MSB form (control bit 0 set), r0.a = 130 >= 0.5: picks CD", 1, 1, 130, 2, { 56, 60, 36 } },
            { "mux, MSB form, r0.a = 65 < 0.5: picks AB", 1, 1, 65, 2, { 16, 36, 145 } }
        };
        for (m = 0; m < 5; m++) {
            c = rc_add(mx[m].what, 2, mx[m].flags, 0);
            c->rc.color_ocw[0] = 0;                      /* stage 0 writes no RGB */
            c->rc.alpha_icw[0] = RC_A(RC_C0) << 24 | (RC_ZERO | 0x20u) << 16;   /* c0.a * (1 - 0) */
            c->rc.alpha_ocw[0] = RC_R0 << 4;             /* r0.a = k / 255 */
            c->rc.factor0[0] = mx[m].k << 24;
            c->rc.color_icw[1] = RC_V0 << 24 | RC_V0 << 16 | RC_C1 << 8 | RC_V0;
            c->rc.color_ocw[1] = (mx[m].mux ? 0x04u << 12 : 0u) | (RC_R0 << 8);   /* sum or mux -> r0.rgb */
            c->rc.factor1[0] = 0xFFE0A030u;
            c->rc.final0 = RC_R0;
            rc_want(c, mx[m].group, mx[m].rgb[0], mx[m].rgb[1], mx[m].rgb[2], (int)mx[m].k);
        }
    }
    /* Output destinations: the AB product, the CD product and the sum each to r0. */
    c = rc_add("destination nibble 4: the AB product -> r0", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 4;
    c->rc.final0 = RC_R0;
    c = rc_add("destination nibble 0: the CD product -> r0", 1, 0, 0);
    c->rc.color_icw[0] = RC_ZERO << 24 | RC_V0 << 16 | RC_V0 << 8 | RC_V0;
    c->rc.color_ocw[0] = RC_R0;
    c->rc.final0 = RC_R0;
    c = rc_add("destination nibble 8: the sum -> r0", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16 | RC_C0 << 8 | RC_C0;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.factor0[0] = 0x0000003Fu;
    c->rc.final0 = RC_R0;
    /* Blue to alpha: fl 0x80 takes the AB output's blue into the destination's alpha, fl 0x40 the
     * CD output's. The alpha is then read back by the final combiner. */
    c = rc_add("blue to alpha from the AB output (fl 0x80)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = (0x80u << 12) | (RC_R0 << 4);
    c->rc.final0 = RC_ZERO;
    c = rc_add("blue to alpha from the CD output (fl 0x40)", 1, 0, 0);
    c->rc.color_icw[0] = RC_ZERO << 24 | RC_V0 << 16 | RC_V0 << 8 | RC_V0;
    c->rc.color_ocw[0] = (0x40u << 12) | RC_R0;
    c->rc.final0 = RC_ZERO;
    /* A stage that "writes" register 0 leaves it zero for a later stage to read. */
    c = rc_add("a write to register 0 leaves it zero", 2, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = 0;                          /* the destination nibble is 0: discarded */
    c->rc.color_icw[1] = RC_ZERO << 24 | RC_V0 << 16;
    c->rc.color_ocw[1] = RC_R0 << 8;
    c->rc.final0 = RC_R0;
    /* The final combiner's mix with a non-trivial A: D + C*(1-A) + B*A. */
    c = rc_add("final: D + C*(1-A) + B*A with A = v0", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 4;                 /* r0.rgb = v0 * v0 */
    c->rc.factor0[0] = 0x0000003Fu;
    c->rc.factor1[0] = 0xFFFFFFFFu;
    c->rc.final_c0 = 0x0000003Fu;                    /* C = final c0 */
    c->rc.final_c1 = 0xFFFFFFFFu;                    /* B = final c1 */
    c->rc.final0 = (RC_V0 << 24) | (RC_C1 << 16) | (RC_C0 << 8) | RC_R0;
    c->rc.final1 = 0x001C00u;
    /* The V1+R0 sum and its flags, which live in the low byte of final1 (0x40 complements v1, 0x20
     * complements r0, 0x80 saturates the sum). Stage 0 sets r0 = c0 (c0 * an all-ones c1), then each
     * flag is a pair: the case without it and the case with it, which must come out different.
     * v1 = 32,64,96 (of 255). */
    {
        static const struct { const char *what; uint32_t c0, fl, clampcase; int rgb[3]; } fs[6] = {
            { "final sum v1 + r0, no flag (r0 = 16)", 0xFF101010u, 0x00u, 0, { 48, 80, 112 } },
            { "final sum with fl 0x40: (1 - v1) + r0", 0xFF101010u, 0x40u, 0, { 239, 207, 175 } },
            { "final sum v1 + r0, no flag (r0 = 204)", 0xFFCCCCCCu, 0x00u, 0, { 236, 255, 255 } },
            { "final sum with fl 0x20: v1 + (1 - r0)", 0xFFCCCCCCu, 0x20u, 0, { 83, 115, 147 } },
            { "final sum 1.1-1.4 unsaturated, read as C*(1-v0)", 0xFFFFFFFFu, 0x00u, 1, { 215, 199, 87 } },
            { "final sum with fl 0x80: saturated before C*(1-v0)", 0xFFFFFFFFu, 0x80u, 1, { 191, 159, 63 } }
        };
        for (m = 0; m < 6; m++) {
            c = rc_add(fs[m].what, 1, 0, 0);
            c->rc.color_icw[0] = RC_C0 << 24 | RC_C1 << 16;
            c->rc.color_ocw[0] = RC_R0 << 4;             /* r0 = c0 */
            c->rc.factor0[0] = fs[m].c0;
            c->rc.factor1[0] = 0xFFFFFFFFu;
            /* The clamp is only visible through C*(1-A): read directly, the output clamp hides it. */
            c->rc.final0 = fs[m].clampcase ? (RC_V0 << 24) | (RC_SUM << 8) : RC_SUM;
            c->rc.final1 = 0x001C00u | fs[m].fl;
            rc_want(c, 4u << (m / 2), fs[m].rgb[0], fs[m].rgb[1], fs[m].rgb[2], -1);
        }
    }
    /* The E*F product, read out as the final combiner's D. */
    c = rc_add("final: the E*F product (v0 * v1)", 1, 0, 0);
    c->rc.final0 = RC_EF;
    c->rc.final1 = (RC_V0 << 24) | (RC_V1 << 16) | (RC_R0 << 8);
    /* Alpha from final1 >> 8, from a register the RGB path does not use. */
    c = rc_add("final alpha comes from final1 >> 8 (v0.b)", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.final0 = RC_R0;
    c->rc.final1 = RC_V0 << 8;
    /* The no-final-combiner path: the output is r0, rgb and alpha. */
    c = rc_add("no final combiner: out = r0", 1, 0, 0);
    c->rc.color_icw[0] = RC_V0 << 24 | RC_V0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 4;
    c->rc.alpha_icw[0] = RC_C0 << 24 | RC_V0 << 16;
    c->rc.alpha_ocw[0] = RC_R0 << 4;
    c->rc.factor0[0] = 0x0000003Fu;
    c->rc.final0 = 0;
    c->rc.final1 = 0;
    /* Fog: R3 is (fog colour rgb, vertex fog factor), read out through the final combiner. The
     * executor's fog unit ignores the per-vertex distance here (SET_FOG_MODE 0x029C and
     * SET_FOG_PARAMS 0x09C0/0x09C4 are not admitted methods, so the mode stays 0 and the factor is
     * 0 with fog on, 1 with it off), so those are the two reachable factor values. */
    c = rc_add("fog: R3.rgb is the fog colour, R3.a the factor (fog off: 1)", 1, 0, 0);
    c->rc.final0 = RC_FOG;
    c->rc.final1 = (RC_FOG | 0x10u) << 8;
    c->fog_en = 0;
    c = rc_add("fog: the same registers with fog on (factor 0)", 1, 0, 0);
    c->rc.final0 = RC_FOG;
    c->rc.final1 = (RC_FOG | 0x10u) << 8;
    c->fog_en = 1;
    /* Per-stage c0/c1 (COMBINERCOUNT flags 0x010 / 0x100): factor0[1] and factor1[1] differ from
     * factor0[0], so ignoring the flags changes the pixel. */
    c = rc_add("per-stage c0/c1 factors (flags 0x010 | 0x100)", 2, 0x110u, 0);
    c->rc.color_icw[0] = RC_C0 << 24 | RC_C0 << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.factor0[0] = 0x0000003Fu;
    c->rc.color_icw[1] = RC_R0 << 24 | RC_C0 << 16 | RC_C1 << 8 | RC_C0;
    c->rc.color_ocw[1] = RC_R0 << 8;
    c->rc.factor0[1] = 0xFFFFFFFFu;
    c->rc.factor1[1] = 0x80808080u;
    c->rc.final0 = RC_R0;
    /* The specular and fog registers as stage sources. */
    c = rc_add("specular and fog registers as stage sources", 1, 0, 0);
    c->rc.color_icw[0] = RC_V1 << 24 | RC_FOG << 16;
    c->rc.color_ocw[0] = RC_R0 << 8;
    c->rc.final0 = RC_R0;
}

/* The register words of one case, in the order the executor reads them. Two stages are written even
 * for a one-stage case, so no case inherits an earlier case's register. */
static void rc_registers(const RcCase *c)
{
    uint32_t s;

    pb(0x1E60, c->rc.control);
    pb(0x1E70, c->rc.stage_program);
    for (s = 0; s < 2; s++) {
        pb(0x0AC0u + 4u * s, c->rc.color_icw[s]);
        pb(0x0260u + 4u * s, c->rc.alpha_icw[s]);
        pb(0x1E40u + 4u * s, c->rc.color_ocw[s]);
        pb(0x0AA0u + 4u * s, c->rc.alpha_ocw[s]);
        pb(0x0A60u + 4u * s, c->rc.factor0[s]);
        pb(0x0A80u + 4u * s, c->rc.factor1[s]);
    }
    pb(0x0288, c->rc.final0);
    pb(0x028C, c->rc.final1);
    pb(0x1E20, c->rc.final_c0);
    pb(0x1E24, c->rc.final_c1);
    pb(0x02A4, c->fog_en);
    pb(0x02A8, c->fog_col);
}

/* One 92-byte combiner vertex, as the back end's VbRcVertex. */
static void rc_vertex(uint32_t vb, uint32_t i, float x, float y, uint32_t diffuse,
                      uint32_t specular, const float t[4][4])
{
    uint8_t *p = G(vb + i * RC_RSTRIDE);
    float *f = (float *)p;

    f[0] = x; f[1] = y; f[2] = 0.5f;
    f[3] = 1.0f;                                   /* oPos.w = 1, so rhw = 1 */
    *(uint32_t *)(p + 16) = diffuse;
    *(uint32_t *)(p + 20) = specular;
    f[6] = 0.5f;                                   /* oFog: the fog unit ignores it */
    memcpy(p + 28, t, 64);
}
/* Attribute n -> the layout above, with the vertex program reading v0, v3, v4, v5, v9, v10 and v1/v2
 * for oT3/oT2 (see rc_vertex_program: slots 11-15 have no admitted offset method, so the last two
 * texture coordinates come from the settable slots 1 and 2 instead). */
static void rc_vertex_format(uint32_t vb)
{
    static const struct { uint32_t a, off, type, size; } at[8] = {
        { 0, 0, 2, 3 }, { 3, 16, 0, 4 }, { 4, 20, 0, 4 }, { 5, 24, 2, 1 },
        { 9, 28, 2, 4 }, { 10, 44, 2, 4 }, { 2, 60, 2, 4 }, { 1, 76, 2, 4 }
    };
    uint32_t i;

    for (i = 0; i < 8; i++) {
        pb(0x1720u + 4u * at[i].a, vb + at[i].off);
        pb(0x1760u + 4u * at[i].a, at[i].type | (at[i].size << 4) | (RC_RSTRIDE << 8));
    }
}
/* The constant pass-through program: oPos = v0, oD0 = v3, oD1 = v4, oFog = v5, oT0 = v9, oT1 = v10,
 * oT2 = v2, oT3 = v1. Its last instruction carries FINAL.
 *
 * oT2 and oT3 come from v2 and v1 rather than from v11 and v12 because the attribute offsets of
 * slots 6-8 and 11-15 (0x1738-0x1748, 0x174C-0x175C) are not in the executor's admitted method
 * list: an attribute that cannot be programmed reads (0,0,0,1), so oT2 and oT3 would be constant
 * at every vertex and stages 2 and 3 would sample one texel each. Both the CPU program path
 * (raster_xf_triangle) and the Vulkan one (backend_xf_vertex) take a stage's coordinate from the
 * program's oT{st}, so re-sourcing it makes both vary together. */
static void rc_vertex_program(void)
{
    pb(0x1E94, 2u);                                /* TRANSFORM_EXECUTION_MODE: program */
    pb(0x1E9C, 0u);                                /* PROGRAM_LOAD slot 0 */
    pb(0x1EA0, 0u);                                /* PROGRAM_START slot 0 */
    vsh_mov(0, 0, 0); vsh_mov(3, 3, 0); vsh_mov(4, 4, 0); vsh_mov(5, 5, 0);
    vsh_mov(9, 9, 0); vsh_mov(10, 10, 0); vsh_mov(11, 2, 0); vsh_mov(12, 1, 1);
}

/* Case i's quad: a 7x7 square in its own cell of the 64x64 target, so all cases can be compared
 * from one submission. */
static void rc_sweep_quad(uint32_t vb, uint32_t i)
{
    float t[4][4];
    uint32_t x0 = (i % 8u) * 8u + 1u, y0 = (i / 8u) * 8u + 1u, k;

    memset(t, 0, sizeof t);
    for (k = 0; k < 4; k++)
        t[k][3] = 1.0f;
    rc_vertex(vb, i * 6u + 0u, (float)x0,        (float)y0,        RC_DIFFUSE, RC_SPECULAR, t);
    rc_vertex(vb, i * 6u + 1u, (float)(x0 + 6u), (float)y0,        RC_DIFFUSE, RC_SPECULAR, t);
    rc_vertex(vb, i * 6u + 2u, (float)x0,        (float)(y0 + 6u), RC_DIFFUSE, RC_SPECULAR, t);
    rc_vertex(vb, i * 6u + 3u, (float)(x0 + 6u), (float)y0,        RC_DIFFUSE, RC_SPECULAR, t);
    rc_vertex(vb, i * 6u + 4u, (float)(x0 + 6u), (float)(y0 + 6u), RC_DIFFUSE, RC_SPECULAR, t);
    rc_vertex(vb, i * 6u + 5u, (float)x0,        (float)(y0 + 6u), RC_DIFFUSE, RC_SPECULAR, t);
}
/* The whole sweep in ONE submission: each case's registers are written immediately before its own
 * draw, so the mux cases, the two fog cases and the per-stage-factor case are decided by
 * consecutive draws whose register blocks differ -- the back end's dynamic uniform-block offsets. */
static void build_rc_sweep(void)
{
    const uint32_t surf = RCSURF(RC_SWEEP), vb = RCVB(RC_SWEEP);
    uint32_t i;

    pb_begin(); pb_reset_state();
    pb_surface(surf, 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    for (i = 0; i < k_rc_n; i++)
        rc_sweep_quad(vb, i);
    for (i = 0; i < k_rc_n; i++) {
        rc_registers(&k_rc[i]);
        pb_draw_range(i * 6u, 6u);
    }
    pb_flip();
}

/* The sweep's oracle: nv2a_rc_eval on the same registers and the same constant vertex values. Every
 * stage is NONE (stage_program 0), so a stage's result is (0,0,0,1), and the fog factor is 1 with
 * fog off and 0 with it on, which is what the executor's fog unit produces for the admitted methods. */
static void rc_oracle(const RcCase *c, float out[4])
{
    float v0[4], v1[4], fog[4], t[4][4];
    uint32_t k, j;

    nv2a_rc_unpack(RC_DIFFUSE, v0);
    nv2a_rc_unpack(RC_SPECULAR, v1);
    fog[0] = (float)(c->fog_col & 0xFFu) / 255.0f;
    fog[1] = (float)((c->fog_col >> 8) & 0xFFu) / 255.0f;
    fog[2] = (float)((c->fog_col >> 16) & 0xFFu) / 255.0f;
    fog[3] = c->fog_en ? 0.0f : 1.0f;
    for (k = 0; k < 4; k++)
        for (j = 0; j < 4; j++)
            t[k][j] = j == 3 ? 1.0f : 0.0f;
    nv2a_rc_eval(&c->rc, v0, v1, fog, t, out);
}

static int rc_quant(float f)
{
    int v = (int)((f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f)) * 255.0f + 0.5f);
    return v < 0 ? 0 : (v > 255 ? 255 : v);
}

/* The cases that share a group bit must give visibly different oracle values (at least this many 8-bit steps in
 * some channel), or the flag that separates them is not what decides the pixel. */
#define RC_GROUP_MIN 16
static int rc_groups_check(void)
{
    uint32_t i, j;
    int ok = 1;

    for (i = 0; i < k_rc_n; i++)
        for (j = i + 1; j < k_rc_n; j++) {
            float a[4], b[4];
            int k, d = 0;

            if (!(k_rc[i].group & k_rc[j].group))
                continue;
            rc_oracle(&k_rc[i], a);
            rc_oracle(&k_rc[j], b);
            for (k = 0; k < 3; k++) {
                int x = abs(rc_quant(a[k]) - rc_quant(b[k]));
                if (x > d) d = x;
            }
            if (d < RC_GROUP_MIN) {
                fprintf(stderr, "FAIL [combiner fixture]: '%s' and '%s' (groups 0x%x, 0x%x) differ by only %d steps\n",
                        k_rc[i].what, k_rc[j].what, k_rc[i].group, k_rc[j].group, d);
                ok = 0;
            }
        }
    return ok;
}

/* Compare the sweep's centre pixels with the oracle. The CPU pass must agree with the oracle too --
 * that is what shows the oracle is the same arithmetic the executor runs, and not a misreading. */
static int rc_sweep_check(const char *label, const uint8_t *px)
{
    uint32_t i;
    int ok = 1, drawn = 0, clear_hits = 0, worst = 0;

    for (i = 0; i < k_rc_n; i++) {
        uint32_t x = (i % 8u) * 8u + 2u, y = (i / 8u) * 8u + 3u;
        uint32_t got = px32(px, 64, x, y);
        float out[4];
        int e[4], g[4], d, k, which = -1;

        rc_oracle(&k_rc[i], out);
        for (k = 0; k < 4; k++)
            e[k] = rc_quant(out[k]);
        /* The target is A8R8G8B8, so the word is A<<24 | R<<16 | G<<8 | B and the oracle's
         * out[0..3] is r, g, b, a. */
        g[0] = (int)((got >> 16) & 255); g[1] = (int)((got >> 8) & 255);
        g[2] = (int)(got & 255); g[3] = (int)(got >> 24);
        d = 0;
        for (k = 0; k < 4; k++) {
            int a = abs(e[k] - g[k]);
            if (a > d) { d = a; which = k; }
        }
        if (d > worst) worst = d;
        /* The oracle must say what the label claims, or the fixture has stopped testing its flag. */
        if (k_rc[i].has_want) {
            for (k = 0; k < 4; k++)
                if (k_rc[i].want[k] >= 0 && abs(e[k] - k_rc[i].want[k]) > RC_RTOL) {
                    fprintf(stderr, "FAIL [%s combiner sweep]: %s: the label claims %d,%d,%d,%d but "
                                    "nv2a_rc_eval gives %d,%d,%d,%d\n", label, k_rc[i].what,
                            k_rc[i].want[0], k_rc[i].want[1], k_rc[i].want[2], k_rc[i].want[3],
                            e[0], e[1], e[2], e[3]);
                    ok = 0;
                    break;
                }
        }
        if (got == CLR && !(e[0] == 0x10 && e[1] == 0x10 && e[2] == 0x10))
            clear_hits++;
        else
            drawn++;
        if (d > RC_RTOL) {
            fprintf(stderr, "FAIL [%s combiner sweep]: %s: nv2a_rc_eval says %d,%d,%d,%d, the target has "
                            "%d,%d,%d,%d (channel %d off by %d)\n", label, k_rc[i].what,
                    e[0], e[1], e[2], e[3], g[0], g[1], g[2], g[3], which, d);
            ok = 0;
        } else {
            printf("      %-8s %-58s ref %3d,%3d,%3d,%3d  got %3d,%3d,%3d,%3d\n", label,
                   k_rc[i].what, e[0], e[1], e[2], e[3], g[0], g[1], g[2], g[3]);
        }
    }
    printf("  [%s combiner sweep] %u cases, %d drawn (not the clear colour), worst channel diff %d (allowed %d)\n",
           label, k_rc_n, drawn, worst, RC_RTOL);
    if (clear_hits) {
        fprintf(stderr, "FAIL [%s combiner sweep]: %d cases did not draw (the pixel is the clear colour)\n",
                label, clear_hits);
        ok = 0;
    }
    return ok;
}

/* ── Randomized combiner states (V4) ───────────────────────────────────────
 *
 * The deterministic sweep above decides one flag at a time. This suite draws complete random
 * register-combiner states instead: 2 fixed seeds x 256 cases, from a splitmix64 stream seeded only
 * by (seed, index) -- no rand(), no time, no environment, no locale, no branch on accumulated
 * floating point. The accepted state is the first candidate the filters below pass, so a case is
 * re-derivable from (seed, index) alone; the printed inventory hash and the per-case state dump make
 * a failure reproducible without re-running the generator.
 *
 * Every state is VALID under the contracts the two implementations share:
 *   - a stage reads only registers that are defined at that point: the initialised set
 *     {0,1,2,3,4,5,8,9,10,11} (r0..r5, t0..t3) plus r0.a (texture 0's alpha, or 1 with stage 0 off)
 *     plus whatever an earlier stage wrote. A read of anything else is rejected and counted
 *     (rej_invalid), never interpreted. rr_walk re-derives this from the register words, so it is a
 *     checked property of the state, not a hope about the generator.
 *   - stage modes 0..4 on stages 0-2 and 0/4 on stage 3: vb_rc_supported never routes a draw to the
 *     stage-0 fallback (which would not run ps_rc at all), and the CPU never reads the zero it
 *     returns for a mode above 5. Mode 5 (CLIPPLANE) is covered by the deterministic cases.
 *   - the encodings both implementations define: scale/bias values map_out knows, destinations that
 *     cannot collide within one stage, no alpha write over a blue-to-alpha write, final-combiner
 *     mappings identity/invert only, r1/r0 readable only once written, E*F not readable by E/F.
 *     r0.rgb must be written when there is no final combiner (the output would otherwise be the
 *     undefined r0.rgb).
 *   - fog: SET_FOG_MODE and SET_FOG_PARAMS are not admitted, so the only reachable factor values are
 *     1 (fog off) and 0 (fog on); the colour is randomised and reaches both sides the same way
 *     (SET_FOG_COLOR carries R in bits 0-7, which backend_fill_rc reverses).
 *   - textures: stages 0-2 bind the three images write_guest_data fills -- stage 0 the linear R5G6B5
 *     image addressed in texels, stages 1-2 the two swizzled images addressed in [0,1] -- always
 *     FILTER_NEAREST (so the oracle need not model tent filtering) and always at half-texel
 *     coordinates (so a 1-ULP difference between the CPU's and the shader's expression cannot select
 *     a neighbouring texel). Stage 3 is never bound: its OFFSET/FORMAT methods are not admitted, so
 *     the executor cannot make it usable at all. Randomizing a stage's coordinate would be decided by
 *     nothing, so the coordinates are derived from the texel the stage must sample.
 * A state the oracle cannot reproduce exactly is rejected and counted too: a mux selection within a
 * ULP of its boundary (rej_mux), an output within a ULP of a quantisation boundary (rej_cond), and
 * an output a correct pixel could confuse with the clear colour (rej_clear).
 *
 * The oracle is nv2a_rc_eval with the inputs the GPU actually receives: the three vertices of a quad
 * carry the same diffuse/specular/fog/coordinates, so interpolation returns the vertex value; the
 * D3DCOLORs are unpacked with nv2a_rc_unpack, exactly what the R8G8B8A8_UNORM vertex format delivers
 * back; and each stage's result is the texel nv2a_backend_decode_texture returns for the coordinate
 * the stage samples -- or (0,0,0,1) for mode 0, saturate(coord) for PASSTHRU, and (1,1,1,1) for an
 * unusable texture in modes 1-3, which is what rc_texel and the shader's `ts.z > 0.5` branch both do.
 *
 * The whole randomized suite runs in BOTH passes through rc_run(), like the deterministic sweep: the
 * CPU pass shows the oracle is the arithmetic the executor runs for these states, and the Vulkan pass
 * shows the shader matches it. The tolerance is the file's RC_RTOL 1 and is not raised.
 *
 * Case g = seed slot * 256 + index is cell g % 16 of submission g / 16, and its registers are written
 * immediately before its own draw (which is what exercises the back end's dynamic uniform-block
 * offsets). A 64x64 target holds 64 cells of 8x8 and a vertex buffer holds 118 cases of 6 vertices at
 * 92 bytes, but the pushbuffer is the limit: a case costs 114 dwords of registers (8 stages x 6
 * methods + control/stage_program + the four final words and fog) + up to 36 for its texture stage
 * setup + 6 for the draw range = 156, and a submission carries 158 dwords of fixed setup (vertex
 * program, vertex format, surface, clear) plus 112 of register tail, against pb()'s 4096-dword cap.
 * 16 cases per submission is 16 * 156 + 270 = 2766 dwords: comfortable, and 512 cases are then 32
 * submissions per pass.
 */
#define RR_NSEED      2u
#define RR_PER_SEED   256u
#define RR_N          (RR_NSEED * RR_PER_SEED)
#define RR_BATCH      16u
#define RR_NBATCH     (RR_N / RR_BATCH)
#define RR_MAX_TRIES  1000u
#define RR_COND_DELTA (1.0f / 4096.0f)
#define RR_COND_MAX   (0.5f / 255.0f)
#define RR_LSB_MARGIN (1.0f / 64.0f)
#define RR_MSB_MARGIN (1.0f / 1024.0f)
#define RR_CLR_MIN    2
#define RR_MAX_DUMP   16u

/* The two fixed seeds. One stream per case, so the whole inventory is a function of these. */
static const uint32_t k_rr_seed[RR_NSEED] = { 0x20261013u, 0x9E3779B9u };
/* Stage modes for stages 0-2, weighted so NONE, PROJECT2D, 3D, CUBEMAP-on-2D and PASSTHRU all
 * appear; stage 3 is drawn from {0, 4} only (its texture can never be bound). */
static const uint32_t k_rr_mode[8]  = { 0u, 1u, 1u, 1u, 2u, 2u, 3u, 4u };
/* PS_COMBINEROUTPUT scale/bias: the six encodings map_out defines. 0x28/0x38 are left out (the
 * hardware leaves them undefined; both implementations fall back to identity). */
static const uint32_t k_rr_scale[6] = { 0x00u, 0x08u, 0x10u, 0x18u, 0x20u, 0x30u };
/* Destination registers: never 0 (discard), 1/2/3 (c0/c1 are reloaded every stage and r3 is the fog
 * register), 6/7 (never defined) or 14/15 (final combiner only). r0 and r1 are weighted. */
static const uint32_t k_rr_dst[16]  = { 0u, 0u, 12u, 12u, 12u, 12u, 13u, 13u,
                                        13u, 13u, 4u, 5u, 8u, 9u, 10u, 11u };
/* The three textures the randomized cases bind, as the executor delivers them: stage 0 the linear
 * R5G6B5 image (addressed in texels, pitch 16), stages 1 and 2 the two swizzled images (addressed in
 * [0,1]; a swizzled texture carries its size in the format word, so no pitch is written). All three
 * are written by write_guest_data and the same addresses are used by the deterministic cases. */
typedef struct { uint32_t va, fmt, pitch, linear; } RrTex;
static const RrTex k_rr_tex[3] = {
    { TEX_MAG, 0x11u, 16u, 1u },
    { TEX_RC1, 0x05u,  0u, 0u },
    { TEX_RC2, 0x06u,  0u, 0u }
};

typedef struct {
    uint32_t seed, index, tries;
    uint32_t direct_a;             /* the mutation-A direct shape: n = 1, no final, alpha sum -> r0.a */
    uint32_t direct;               /* 1: a direct-seam case (all four stages bound, any mode 0-4) */
    Nv2aCombiner rc;
    uint32_t fog_en, fog_col, diffuse, specular;
    uint32_t mode[4], tex[4];
    float coord[4][4];             /* the per-vertex oT0..oT3 the quad carries */
    float t[4][4];                 /* what each stage delivers: the oracle's t */
} RrCase;

typedef struct {
    /* generation */
    uint32_t cand, rej_invalid, rej_mux, rej_cond, rej_clear;
    /* coverage, over the accepted cases */
    uint32_t stage_n[9];
    uint32_t final_on, final_off;
    uint32_t inmap[8], finalmap[2];
    uint32_t rgb_rep, alpha_a, alpha_b;
    uint32_t srcreg[16];
    uint32_t final_sum, final_ef, sum_flag[3];
    uint32_t rgb_scale[6], alpha_scale[6];
    uint32_t ab_dot, cd_dot, both_dot;
    uint32_t rgb_mux, alpha_mux, mux_sel[4];
    uint32_t b2a_ab, b2a_cd;
    uint32_t dst[16];
    uint32_t raw_reads, raw_cases;
    uint32_t pc0_cases, pc1_cases;
    uint32_t fog_on, fog_off, fog_reg_reads;
    uint32_t mode[5];
    uint32_t stage3_pass, linear_stage0;
    uint32_t bound_tex_reads, passthru_reads;
    uint32_t asum_dest, asum_deciding, asum_direct;
    uint32_t amap2, amap3, amap2_reach, amap3_reach;
    uint32_t swap_deciding;
    uint32_t out_range, distinct;
    uint32_t map_checks, map_bad;
    /* per-case flags, set by rr_walk and folded into the case counters by rr_cover_case */
    uint32_t cs_asum_dest, cs_raw, cs_map2, cs_map2_reach, cs_map3, cs_map3_reach;
    uint64_t hash;
} RrCover;

static RrCase g_rr[RR_N];
/* The four solid A8R8G8B8 colours of the direct-seam cases: each different in every channel
 * including alpha, and far enough apart that a stage bound to another stage's image changes the
 * combined result. The same four words (and the same TEX_Q addresses) run_four_textures uses. */
static const uint32_t k_q_argb[4] = { 0xFF4080C0u, 0xC0E63366u, 0x804DCC1Au, 0x401A0D33u };
/* The images the oracle samples: 0-2 are the executor-path textures, 3-6 the four solid colours the
 * direct-seam cases bind (k_q_argb), all as nv2a_backend_decode_texture returns them. */
#define RR_NTEX 7u
static uint32_t rr_dec[RR_NTEX][64];
static float rr_dec_f[RR_NTEX][64][4];
static uint32_t g_rr_dumped;

/* splitmix64. Every draw below is unconditional and in a fixed order, so the stream is the same
 * function of (seed, index) everywhere. */
static uint64_t rr_next(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
static uint32_t rr_u32(uint64_t *s) { return (uint32_t)(rr_next(s) >> 32); }
static uint32_t rr_below(uint64_t *s, uint32_t n)
{
    return (uint32_t)(((uint64_t)rr_u32(s) * (uint64_t)n) >> 32);
}
static uint32_t rr_pop(uint32_t v)
{
    uint32_t c = 0;
    while (v) { c += v & 1u; v >>= 1; }
    return c;
}
/* The k-th set bit of a 16-bit register set, k drawn below the set's size. */
static uint32_t rr_pick(uint64_t *s, uint32_t set)
{
    uint32_t k = rr_below(s, rr_pop(set)), i;
    for (i = 0; i < 16u; i++) {
        if (!((set >> i) & 1u))
            continue;
        if (k-- == 0u)
            return i;
    }
    return 0u;
}

/* PS_INPUTMAPPING (nv2a_combiner.c map_in) and PS_COMBINEROUTPUT (map_out), copied so a generated
 * state's own arithmetic can be read back; rr_fixture_check proves both against the oracle. */
static float rr_map_in(float x, uint32_t b)
{
    float p = x < 0.0f ? 0.0f : x;
    switch ((b >> 5) & 7u) {
    case 0u:  return p;                                /* unsigned identity */
    case 1u:  return 1.0f - (x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x));
    case 2u:  return 2.0f * p - 1.0f;                  /* expand normal     */
    case 3u:  return -2.0f * p + 1.0f;                 /* expand negate     */
    case 4u:  return p - 0.5f;                         /* half-bias normal  */
    case 5u:  return -p + 0.5f;                        /* half-bias negate  */
    case 6u:  return x;                                /* signed identity   */
    default:  return -x;                               /* signed negate     */
    }
}
static float rr_map_out(float x, uint32_t m)
{
    switch (m & 0x38u) {
    case 0x08u: return x - 0.5f;
    case 0x10u: return x * 2.0f;
    case 0x18u: return (x - 0.5f) * 2.0f;
    case 0x20u: return x * 4.0f;
    case 0x30u: return x * 0.5f;
    default:    return x;
    }
}
/* A stored value pushed through the destination's [-1,1] clamp and the output's [0,1] clamp, then
 * quantised the way rc_sweep_check does. */
static int rr_byte(float x)
{
    float v = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
    return rc_quant(v);
}
static int rr_scale_known(uint32_t m)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        if (k_rr_scale[i] == m)
            return 1;
    return 0;
}
static uint32_t rr_scale_idx(uint32_t m)
{
    uint32_t i;
    for (i = 0; i < 6u; i++)
        if (k_rr_scale[i] == m)
            return i;
    return 0;
}
/* A destination register both implementations define: not 0 (discard), not 1/2/3 (c0/c1 are reloaded
 * per stage, r3 is the fog register), not 6/7 (never defined) and not 14/15 (final combiner only). */
static int rr_dst_ok(uint32_t r)
{
    if (!r)
        return 1;
    if (r == 1u || r == 2u || r == 3u || r == 6u || r == 7u || r == 14u || r == 15u)
        return 0;
    return 1;
}
static int rr_alpha_is_output(const RrCase *c, uint32_t s, uint32_t dst);
static uint32_t rr_final_byte(uint64_t *st, uint32_t a_set, uint32_t rgb_set);

/* The register file nv2a_rc_eval starts from, for the case's own inputs, at the point stage 0 reads
 * it (r1/r2 are c0/c1, which the stage loop reloads before the reads). */
static void rr_init_regs(const RrCase *c, float R[16][4])
{
    int i, j;

    for (i = 0; i < 16; i++)
        for (j = 0; j < 4; j++)
            R[i][j] = 0.0f;
    nv2a_rc_unpack(c->diffuse, R[4]);
    nv2a_rc_unpack(c->specular, R[5]);
    R[3][0] = (float)(c->fog_col & 0xFFu) / 255.0f;
    R[3][1] = (float)((c->fog_col >> 8) & 0xFFu) / 255.0f;
    R[3][2] = (float)((c->fog_col >> 16) & 0xFFu) / 255.0f;
    R[3][3] = c->fog_en ? 0.0f : 1.0f;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            R[8 + i][j] = c->t[i][j];
    R[12][3] = (c->rc.stage_program & 0x1Fu) ? c->t[0][3] : 1.0f;
    nv2a_rc_unpack(c->rc.factor0[0], R[1]);
    nv2a_rc_unpack(c->rc.factor1[0], R[2]);
}

static void rr_inputs(const RrCase *c, float v0[4], float v1[4], float fog[4], float t[4][4])
{
    uint32_t st, j;

    nv2a_rc_unpack(c->diffuse, v0);
    nv2a_rc_unpack(c->specular, v1);
    fog[0] = (float)(c->fog_col & 0xFFu) / 255.0f;
    fog[1] = (float)((c->fog_col >> 8) & 0xFFu) / 255.0f;
    fog[2] = (float)((c->fog_col >> 16) & 0xFFu) / 255.0f;
    fog[3] = c->fog_en ? 0.0f : 1.0f;
    for (st = 0; st < 4; st++)
        for (j = 0; j < 4; j++)
            t[st][j] = c->t[st][j];
}
static void rr_oracle(const RrCase *c, float out[4])
{
    float v0[4], v1[4], fog[4], t[4][4];

    rr_inputs(c, v0, v1, fog, t);
    nv2a_rc_eval(&c->rc, v0, v1, fog, t, out);
}

/* The value the final combiner reads from register `reg`'s .a (rep) or .b, exactly: mappings 6 and 7
 * are the signed identity and its negate, the output clamps to [0,1], and for |v| <= 1
 * clamp01(v) - clamp01(-v) == v, so the difference recovers the stored float. */
static float rr_probe_comp(const RrCase *c, const Nv2aCombiner *p, uint32_t reg, uint32_t rep)
{
    Nv2aCombiner q = *p;
    float v0[4], v1[4], fog[4], t[4][4], o[4], pos, neg;
    uint32_t g = reg | (rep ? 0x10u : 0u);

    q.final0 = 0;
    q.final1 = (g | (6u << 5)) << 8;
    rr_inputs(c, v0, v1, fog, t);
    nv2a_rc_eval(&q, v0, v1, fog, t, o);
    pos = o[3];
    q.final1 = (g | (7u << 5)) << 8;
    nv2a_rc_eval(&q, v0, v1, fog, t, o);
    neg = o[3];
    return pos - neg;
}
/* r0.a at the point stage s reads it: the probe stops after stage s-1 and reads r0.a back. */
static float rr_r0a_at(const RrCase *c, uint32_t s)
{
    Nv2aCombiner p = c->rc;

    p.control = (p.control & ~0xFFu) | (s & 0xFFu);
    return rr_probe_comp(c, &p, 12u, 1u);
}

/* The mux selection is a step function of r0.a, so a state whose r0.a sits within a ULP of the step
 * could select a different product on the two sides. Exactly representable selectors are kept. */
static int rr_mux_ok(const RrCase *c)
{
    uint32_t n = c->rc.control & 0xFFu, s;

    for (s = 0; s < n; s++) {
        float r0a;
        if (!((c->rc.color_ocw[s] >> 12) & 4u) && !((c->rc.alpha_ocw[s] >> 12) & 4u))
            continue;
        r0a = rr_r0a_at(c, s);
        if ((c->rc.control >> 8) & 1u) {                     /* MSB form: r0.a >= 0.5 */
            if (r0a != 0.5f && fabsf(r0a - 0.5f) < RR_MSB_MARGIN)
                return 0;
        } else {                                             /* LSB form: (int)(r0.a * 255) & 1 */
            float x = r0a * 255.0f;
            if (x != floorf(x) && fabsf(x - floorf(x + 0.5f)) < RR_LSB_MARGIN)
                return 0;
        }
    }
    return 1;
}

/* The 28 floats the oracle is given, flattened, so the conditioning probe can walk them. */
static float *rr_flat(float v0[4], float v1[4], float fog[4], float t[4][4], uint32_t j)
{
    if (j < 4u)  return &v0[j];
    if (j < 8u)  return &v1[j - 4u];
    if (j < 12u) return &fog[j - 8u];
    return &t[(j - 12u) / 4u][(j - 12u) % 4u];
}
/* Every float the two implementations compute from is delivered in a different instruction order, so
 * a state whose output moves by more than half a quantisation step under a 1/4096 perturbation of
 * its inputs cannot be compared at +/-1 LSB. Four sign patterns, each a perturbation of all 28. */
static int rr_cond_ok(const RrCase *c, const float nominal[4])
{
    float v0[4], v1[4], fog[4], t[4][4], o[4];
    uint32_t p, j;
    int k;

    for (p = 0; p < 4u; p++) {
        rr_inputs(c, v0, v1, fog, t);
        for (j = 0; j < 28u; j++) {
            int sgn = (p == 0u) ? 1 : (p == 1u) ? -1
                    : (p == 2u) ? ((j & 1u) ? -1 : 1) : ((j & 1u) ? 1 : -1);
            *rr_flat(v0, v1, fog, t, j) += (float)sgn * RR_COND_DELTA;
        }
        nv2a_rc_eval(&c->rc, v0, v1, fog, t, o);
        for (k = 0; k < 4; k++)
            if (fabsf(o[k] - nominal[k]) > RR_COND_MAX)
                return 0;
    }
    return 1;
}

/* The oracle's own output must be far enough from the clear colour that a correct pixel can never be
 * it, even at the full tolerance. */
static int rr_clear_ok(const float out[4])
{
    static const int clr[4] = { 0x10, 0x10, 0x10, 0xFF };
    int k, d = 0;

    for (k = 0; k < 4; k++) {
        int x = abs(rc_quant(out[k]) - clr[k]);
        if (x > d) d = x;
    }
    return d >= RR_CLR_MIN;
}

/* Validity, re-derived from the register words alone, and the coverage counters of the same walk.
 * Returns 1 for a valid state; with `cov` non-NULL it also counts (only ever called for accepted
 * states, so the counting code may assume validity). */
static int rr_walk(const RrCase *c, RrCover *cov)
{
    const Nv2aCombiner *rc = &c->rc;
    uint32_t n = rc->control & 0xFFu, flags = rc->control >> 8;
    uint32_t rgb_set = 0x0F3Fu;                  /* r0..r5, r8..r11: defined at the start */
    uint32_t a_set = 0x0F3Fu | (1u << 12);       /* and r0.a, from texture 0's alpha or 1 */
    uint32_t wrgb = 0, wa = 0;                   /* what an earlier stage wrote, for the coverage */
    uint32_t readmask = 0, s, i, st;

    if (cov)
        cov->cs_asum_dest = cov->cs_raw = cov->cs_map2 = cov->cs_map2_reach
                          = cov->cs_map3 = cov->cs_map3_reach = 0;
    if (n > 8u || (flags & ~0x111u) || (rc->stage_program >> 20))
        return 0;
    for (s = 0; s < 4u; s++) {
        uint32_t m = (rc->stage_program >> (s * 5u)) & 0x1Fu;
        if (m > 4u)
            return 0;
        /* Stage 3's texture cannot be bound through the executor (its OFFSET and FORMAT methods are
         * not admitted), so its executor-path modes are 0 or 4 only. The direct-seam cases bind all
         * four stages, so any mode 0-4 is interpretable there. */
        if (s == 3u && !c->direct && !(m == 0u || m == 4u))
            return 0;
    }
    for (s = 0; s < 8u; s++) {
        uint32_t icw = rc->color_icw[s], aicw = rc->alpha_icw[s];
        uint32_t ocw = rc->color_ocw[s], aocw = rc->alpha_ocw[s];
        uint32_t fl, afl, ab, cd, sm, aab, acd, asm;

        if (s >= n) {
            if (icw || aicw || ocw || aocw || rc->factor0[s] || rc->factor1[s])
                return 0;
            continue;
        }
        for (i = 0; i < 4u; i++) {                        /* RGB inputs */
            uint32_t b = icw >> (24u - 8u * i), reg = b & 0xFu;
            uint32_t set = (b & 0x10u) ? a_set : rgb_set;
            if (!((set >> reg) & 1u))
                return 0;
        }
        for (i = 0; i < 4u; i++) {                        /* alpha inputs */
            uint32_t b = aicw >> (24u - 8u * i), reg = b & 0xFu;
            uint32_t set = (b & 0x10u) ? a_set : rgb_set;
            if (!((set >> reg) & 1u))
                return 0;
        }
        fl = ocw >> 12;
        afl = aocw >> 12;
        if (ocw >> 20 || aocw >> 18)
            return 0;
        if (!rr_scale_known(fl & 0x38u) || !rr_scale_known(afl & 0x38u) || (afl & ~0x3Cu))
            return 0;
        ab = (ocw >> 4) & 0xFu; cd = ocw & 0xFu; sm = (ocw >> 8) & 0xFu;
        if (!rr_dst_ok(ab) || !rr_dst_ok(cd) || !rr_dst_ok(sm))
            return 0;
        if ((ab && (ab == cd || ab == sm)) || (cd && cd == sm))
            return 0;
        if (((fl & 0x80u) && !ab) || ((fl & 0x40u) && !cd))
            return 0;
        aab = (aocw >> 4) & 0xFu; acd = aocw & 0xFu; asm = (aocw >> 8) & 0xFu;
        if (!rr_dst_ok(aab) || !rr_dst_ok(acd) || !rr_dst_ok(asm))
            return 0;
        if ((aab && (aab == acd || aab == asm)) || (acd && acd == asm))
            return 0;
        if ((fl & 0x80u) && (aab == ab || acd == ab || asm == ab))
            return 0;
        if ((fl & 0x40u) && (aab == cd || acd == cd || asm == cd))
            return 0;

        if (cov) {
            uint32_t rmux = (fl & 4u) && sm, amux = (afl & 4u) && asm;
            uint32_t astage_reach = (aab && rr_alpha_is_output(c, s, aab))
                                 || (acd && rr_alpha_is_output(c, s, acd))
                                 || (asm && rr_alpha_is_output(c, s, asm));
            for (i = 0; i < 4u; i++) {
                uint32_t b = icw >> (24u - 8u * i), reg = b & 0xFu;
                cov->inmap[(b >> 5) & 7u]++;
                cov->srcreg[reg]++;
                readmask |= 1u << reg;
                if (b & 0x10u) cov->rgb_rep++;
                if (wrgb >> reg & 1u) { cov->raw_reads++; cov->cs_raw = 1u; }
            }
            for (i = 0; i < 4u; i++) {
                uint32_t b = aicw >> (24u - 8u * i), reg = b & 0xFu;
                cov->inmap[(b >> 5) & 7u]++;
                cov->srcreg[reg]++;
                readmask |= 1u << reg;
                if (b & 0x10u) cov->alpha_a++; else cov->alpha_b++;
                if (((b & 0x10u) ? wa : wrgb) >> reg & 1u) { cov->raw_reads++; cov->cs_raw = 1u; }
                if (((b >> 5) & 7u) == 2u) { cov->cs_map2 = 1u; cov->cs_map2_reach |= astage_reach; }
                if (((b >> 5) & 7u) == 3u) { cov->cs_map3 = 1u; cov->cs_map3_reach |= astage_reach; }
            }
            cov->rgb_scale[rr_scale_idx(fl & 0x38u)]++;
            cov->alpha_scale[rr_scale_idx(afl & 0x38u)]++;
            if (fl & 2u) cov->ab_dot++;
            if (fl & 1u) cov->cd_dot++;
            if ((fl & 3u) == 3u) cov->both_dot++;
            if (fl & 0x80u) cov->b2a_ab++;
            if (fl & 0x40u) cov->b2a_cd++;
            if (rmux) cov->rgb_mux++;
            if (amux) cov->alpha_mux++;
            if (rmux || amux) {
                float r0a = rr_r0a_at(c, s);
                int cd_sel = ((rc->control >> 8) & 1u) ? (r0a >= 0.5f)
                                                       : (((int)(r0a * 255.0f) & 1) != 0);
                cov->mux_sel[(((rc->control >> 8) & 1u) ? 2 : 0) + (cd_sel ? 1 : 0)]++;
            }
            if (ab) cov->dst[ab]++;
            if (cd) cov->dst[cd]++;
            if (sm) cov->dst[sm]++;
            if (aab) cov->dst[aab]++;
            if (acd) cov->dst[acd]++;
            if (asm) cov->dst[asm]++;
            if (asm && !(afl & 4u))
                cov->cs_asum_dest = 1u;
        }
        if (ab) rgb_set |= 1u << ab;
        if (cd) rgb_set |= 1u << cd;
        if (sm) rgb_set |= 1u << sm;
        if ((fl & 0x80u) && ab) a_set |= 1u << ab;
        if ((fl & 0x40u) && cd) a_set |= 1u << cd;
        if (aab) a_set |= 1u << aab;
        if (acd) a_set |= 1u << acd;
        if (asm) a_set |= 1u << asm;
        if (cov) {
            if (ab) wrgb |= 1u << ab;
            if (cd) wrgb |= 1u << cd;
            if (sm) wrgb |= 1u << sm;
            if ((fl & 0x80u) && ab) wa |= 1u << ab;
            if ((fl & 0x40u) && cd) wa |= 1u << cd;
            if (aab) wa |= 1u << aab;
            if (acd) wa |= 1u << acd;
            if (asm) wa |= 1u << asm;
        }
    }
    if (rc->final0 || rc->final1) {
        uint32_t f0 = rc->final0, f1 = rc->final1, fflags = f1 & 0xFFu;
        uint32_t sfin = rgb_set | (((rgb_set >> 12) & 1u) ? (1u << 14) : 0u);
        uint32_t byte[7];

        if (f1 & 0x1Fu)
            return 0;
        byte[0] = (f1 >> 24) & 0xFFu; byte[1] = (f1 >> 16) & 0xFFu;   /* E, F */
        byte[2] = (f0 >> 24) & 0xFFu; byte[3] = (f0 >> 16) & 0xFFu;   /* A, B */
        byte[4] = (f0 >> 8) & 0xFFu;  byte[5] = f0 & 0xFFu;           /* C, D */
        byte[6] = (f1 >> 8) & 0xFFu;                                  /* G, the output alpha */
        for (i = 0; i < 7u; i++) {
            uint32_t b = byte[i], reg = b & 0xFu, map = (b >> 5) & 7u;
            uint32_t set;
            if (map > 1u)
                return 0;                                  /* identity/invert only */
            if (i == 6u) {                                 /* G, the output alpha */
                if (reg == 14u || reg == 15u)
                    return 0;
                set = (b & 0x10u) ? a_set : rgb_set;
            } else {
                if (reg == 14u) {
                    if ((b & 0x10u) || !((rgb_set >> 12) & 1u))
                        return 0;                          /* v1+r0 needs r0.rgb to mean anything */
                    set = sfin;
                } else if (reg == 15u) {
                    if (i < 2u)
                        return 0;                          /* E*F cannot be read by E or F */
                    if (b & 0x10u)
                        return 0;
                    set = 1u << 15;
                } else {
                    set = (b & 0x10u) ? a_set : sfin;
                }
            }
            if (!((set >> reg) & 1u))
                return 0;
        }
        if (cov) {
            for (i = 0; i < 7u; i++) {
                uint32_t b = byte[i], reg = b & 0xFu;
                readmask |= 1u << reg;
                if (i < 2u) cov->finalmap[(b >> 5) & 7u]++;
                if (reg == 14u) {
                    uint32_t k;
                    cov->final_sum++;
                    for (k = 0; k < 3u; k++)
                        if (fflags & (0x20u << k))
                            cov->sum_flag[k]++;
                }
                if (reg == 15u) cov->final_ef++;
            }
        }
    } else if (!((rgb_set >> 12) & 1u)) {
        return 0;                                          /* the output would be the undefined r0 */
    }
    if (c->direct_a) {
        /* The mutation-A shape must really be the shape it claims: one stage, no final combiner, the
         * alpha sum written into r0.a with the identity scale and nothing else writing alpha. */
        uint32_t aocw = rc->alpha_ocw[0];
        if (n != 1u || rc->final0 || rc->final1)
            return 0;
        if (!((aocw >> 8) & 0xFu) || (aocw >> 12) & 4u)
            return 0;
    }
    if (cov) {
        if (readmask >> 3 & 1u) cov->fog_reg_reads++;
        for (st = 0; st < 4u; st++) {
            uint32_t m = (rc->stage_program >> (st * 5u)) & 0x1Fu;
            if (m >= 1u && m <= 3u && st < 3u && ((readmask >> (8u + st)) & 1u))
                cov->bound_tex_reads++;
            if (m == 4u && ((readmask >> (8u + st)) & 1u))
                cov->passthru_reads++;
        }
    }
    return 1;
}

/* Is the alpha value stage `s` writes into register `dst` still there when the final combiner (or the
 * no-final output, which is r0) reads its alpha? A later stage's alpha write, or a blue-to-alpha RGB
 * write, to `dst` would replace it. */
static int rr_alpha_is_output(const RrCase *c, uint32_t s, uint32_t dst)
{
    uint32_t n = c->rc.control & 0xFFu, t;

    if (!dst)
        return 0;
    for (t = s + 1u; t < n; t++) {
        uint32_t aocw = c->rc.alpha_ocw[t], ocw = c->rc.color_ocw[t], fl = ocw >> 12;
        if (((aocw >> 4) & 0xFu) == dst || (aocw & 0xFu) == dst || ((aocw >> 8) & 0xFu) == dst)
            return 0;
        if ((fl & 0x80u) && ((ocw >> 4) & 0xFu) == dst)
            return 0;
        if ((fl & 0x40u) && (ocw & 0xFu) == dst)
            return 0;
    }
    if (!(c->rc.final0 || c->rc.final1))
        return dst == 12u;
    return ((c->rc.final1 >> 8) & 0xFu) == dst;
}

/* One alpha-portion input byte's value at the point stage `s` reads it. The probe runs stages 0..s-1
 * unchanged, replaces stage s with one that reads the byte under test into its alpha sum and writes
 * it to r0.a (both portions read before either writes, so this reads the pre-stage-s value), and
 * reads r0.a back through the final combiner twice: the signed identity and its negate, whose
 * difference recovers the stored float exactly, because clamp01(v) - clamp01(-v) == v for |v| <= 1
 * and every map_in output is in [-1, 1]. */
static float rr_alpha_at(const RrCase *c, uint32_t s, uint32_t byte)
{
    Nv2aCombiner p = c->rc;
    float v0[4], v1[4], fog[4], t[4][4], o[4], pos, neg;

    p.control = (p.control & ~0xFFu) | ((s + 1u) & 0xFFu);
    p.color_ocw[s] = 0;
    p.alpha_icw[s] = (byte << 24) | ((0u | 0x20u) << 16);  /* A = the byte under test, B = 1 - 0 */
    p.alpha_ocw[s] = 12u << 8;                            /* the sum -> r0.a, identity scale */
    p.final0 = 0;
    p.final1 = ((12u | 0x10u | (6u << 5)) << 8);
    rr_inputs(c, v0, v1, fog, t);
    nv2a_rc_eval(&p, v0, v1, fog, t, o);
    pos = o[3];
    p.final1 = ((12u | 0x10u | (7u << 5)) << 8);
    nv2a_rc_eval(&p, v0, v1, fog, t, o);
    neg = o[3];
    return pos - neg;
}

/* Mutation A ("the alpha sum computed as a product") is decided by a case whose alpha portion writes
 * the SUM destination with the non-mux branch and whose stored byte changes when qab + qcd becomes
 * qab * qcd. This predicate is exact, not a heuristic:
 *   - the write must survive to the output (rr_alpha_is_output);
 *   - the output alpha must BE that register, unattenuated: with no final combiner the output alpha is
 *     R[12].a, so dst must be 12; with a final combiner the output alpha is the G register read with
 *     the unsigned identity mapping, which is a bijection on the stored value's byte;
 *   - both products must be nonzero (0 + x == 0 * x and x + 0 == x * 0 are the same byte), and the two
 *     sums must quantise to different bytes.
 * The stage's own inputs are read back through the oracle (rr_alpha_at), so this is the arithmetic
 * both implementations run, not a model of it. */
static int rr_a_sum_deciding(const RrCase *c)
{
    uint32_t n = c->rc.control & 0xFFu, has_final = (c->rc.final0 || c->rc.final1) ? 1u : 0u, s;

    for (s = 0; s < n; s++) {
        uint32_t aicw = c->rc.alpha_icw[s], aocw = c->rc.alpha_ocw[s], afl = aocw >> 12;
        uint32_t dst = (aocw >> 8) & 0xFu;
        float qab, qcd, sum, prod;

        if (!dst || (afl & 4u))
            continue;
        if (!rr_alpha_is_output(c, s, dst))
            continue;
        if (has_final) {
            uint32_t g = (c->rc.final1 >> 8) & 0xFFu;
            /* G must read this register's .a (bit 0x10 set, which read_alpha takes from r[3], where
             * the alpha destinations write) through the unsigned identity mapping, so the output
             * alpha IS the byte the sum wrote. */
            if ((g & 0xFu) != dst || !((g >> 4) & 1u) || ((g >> 5) & 7u))
                continue;
        } else if (dst != 12u) {
            continue;
        }
        qab = rr_alpha_at(c, s, aicw >> 24) * rr_alpha_at(c, s, aicw >> 16);
        qcd = rr_alpha_at(c, s, aicw >> 8) * rr_alpha_at(c, s, aicw);
        if (qab == 0.0f || qcd == 0.0f)
            continue;
        sum = rr_byte(rr_map_out(qab + qcd, afl & 0x38u));
        prod = rr_byte(rr_map_out(qab * qcd, afl & 0x38u));
        /* More than the tolerance, so the mutated shader's byte is a real failure and not a rounding
         * coincidence the +/-1 comparison would absorb. */
        if (abs(sum - prod) > RC_RTOL)
            return 1;
    }
    return 0;
}

/* Swap PS_INPUTMAPPING 2 and 3 in the byte at bit offset `sh` (mutation B's exact effect on in_a). */
static uint32_t rr_swap_map(uint32_t w, uint32_t sh)
{
    uint32_t b = (w >> sh) & 0xFFu, m = (b >> 5) & 7u;

    if (m == 2u || m == 3u)
        b ^= 0x20u;
    return (w & ~(0xFFu << sh)) | (b << sh);
}
/* Mutation B ("swapped input mapping 2/3") is decided by a case whose output changes when the alpha
 * portion's and the final G's mapping fields are swapped: the mutation makes every in_a read of a
 * mapping-2 byte read map3 and vice versa, and that is exactly this combiner. */
static int rr_swap_deciding(const RrCase *c, const float nominal[4])
{
    Nv2aCombiner p = c->rc;
    float v0[4], v1[4], fog[4], t[4][4], o[4];
    uint32_t n = c->rc.control & 0xFFu, s, i;
    int k;

    for (s = 0; s < n; s++)
        for (i = 0; i < 4u; i++)
            p.alpha_icw[s] = rr_swap_map(p.alpha_icw[s], 24u - 8u * i);
    if (c->rc.final0 || c->rc.final1)
        p.final1 = rr_swap_map(p.final1, 8u);
    rr_inputs(c, v0, v1, fog, t);
    nv2a_rc_eval(&p, v0, v1, fog, t, o);
    for (k = 0; k < 4; k++)
        if (abs(rc_quant(o[k]) - rc_quant(nominal[k])) > RC_RTOL)
            return 1;                       /* a real difference, not a rounding coincidence */
    return 0;
}

/* One random stage-0..3 coordinate and its delivered stage result. The coordinate is chosen from the
 * texel the stage must sample, at (k + 0.5) texels, so a 1-ULP difference cannot move the texel. A
 * direct-seam case binds a solid colour to every stage, so its coordinate is irrelevant and the stage
 * result is that colour (or saturate(coord) for PASSTHRU). */
static void rr_stage_tex(uint64_t *s, RrCase *c, uint32_t st, uint32_t m, uint32_t direct)
{
    uint32_t j;

    if (m == 0u) {
        for (j = 0; j < 4u; j++) {
            c->coord[st][j] = (j == 3u) ? 1.0f : 0.0f;
            c->t[st][j] = (j == 3u) ? 1.0f : 0.0f;
        }
        return;
    }
    if (m == 4u) {                                    /* PASSTHRU: saturate of the raw coordinate */
        for (j = 0; j < 4u; j++) {
            float x = ((float)rr_below(s, 384u) - 64.0f) / 256.0f;
            c->coord[st][j] = x;
            c->t[st][j] = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
        }
        return;
    }
    if (direct) {
        /* A solid texture: the sampled colour is the same whatever the coordinate, so any
         * coordinate works and the oracle's stage result is exactly the decoded colour. */
        for (j = 0; j < 4u; j++) {
            c->coord[st][j] = 0.5f;
            c->t[st][j] = rr_dec_f[3u + st][0][j];
        }
        c->tex[st] = 3u + st;
        return;
    }
    {
        uint32_t slot = (st == 0u) ? 0u : 1u + rr_below(s, 2u);
        uint32_t kx = rr_below(s, 8u), ky = rr_below(s, 8u);
        float dq = (m == 2u) ? 2.0f : 1.0f;           /* the divide the coordinate has to survive */
        float w  = (m == 1u) ? 1.0f : 2.0f;           /* mode 3 must ignore it */

        c->tex[st] = slot;
        c->coord[st][2] = 0.0f;
        c->coord[st][3] = w;
        if (st == 0u) {                               /* linear: addressed in texels */
            c->coord[st][0] = ((float)kx + 0.5f) * dq;
            c->coord[st][1] = ((float)ky + 0.5f) * dq;
        } else {                                      /* swizzled: addressed in [0,1] */
            c->coord[st][0] = ((float)kx + 0.5f) / 8.0f * dq;
            c->coord[st][1] = ((float)ky + 0.5f) / 8.0f * dq;
        }
        for (j = 0; j < 4u; j++)
            c->t[st][j] = rr_dec_f[slot][ky * 8u + kx][j];
    }
}

/* One final-combiner input byte: a register from the defined set, mapping identity or invert. */
static uint32_t rr_final_byte(uint64_t *st, uint32_t a_set, uint32_t rgb_set)
{
    uint32_t rep = (rr_below(st, 4u) == 0u);

    return rr_pick(st, rep ? a_set : rgb_set) | (rep << 4) | (rr_below(st, 2u) << 5);
}

/* One candidate state, from one stream seeded by (seed, index). `direct` selects the direct-seam
 * flavour: all four stages may carry any mode 0-4 and every one is bound to a solid texture (a solid
 * image makes the sampled colour independent of the coordinate, so the oracle is exact). */
static int rr_gen(uint32_t seed, uint32_t index, RrCase *c, RrCover *rej, uint32_t direct)
{
    uint64_t st;
    uint32_t tries;

    st = ((uint64_t)seed << 32) | (uint64_t)index;
    for (tries = 1; tries <= RR_MAX_TRIES; tries++) {
        uint32_t n = index % 9u, cls = (index / 9u) % 3u;
        uint32_t direct_a = (n == 1u) && !direct;
        uint32_t has_final = (n == 0u) ? 1u : (direct_a ? 0u : (cls != 0u));
        uint32_t rgb_set = 0x0F3Fu, a_set = 0x0F3Fu | (1u << 12), prog = 0, s;
        float v0[4], v1[4], fog[4], t[4][4], nominal[4];

        rej->cand++;
        memset(c, 0, sizeof *c);
        c->seed = seed;
        c->index = index;
        c->direct_a = direct_a;
        c->direct = direct;
        c->rc.control = n | ((rr_below(&st, 2u) | (rr_below(&st, 2u) << 4)
                            | (rr_below(&st, 2u) << 8)) << 8);
        for (s = 0; s < 4u; s++) {
            c->mode[s] = (s < 3u || direct) ? k_rr_mode[rr_below(&st, 8u)]
                                            : (rr_below(&st, 4u) == 0u ? 4u : 0u);
            prog |= c->mode[s] << (5u * s);
        }
        c->rc.stage_program = prog;
        for (s = 0; s < 4u; s++)
            rr_stage_tex(&st, c, s, c->mode[s], direct);
        c->diffuse = rr_u32(&st);
        c->specular = rr_u32(&st);
        c->fog_en = rr_below(&st, 2u);
        c->fog_col = rr_u32(&st);

        for (s = 0; s < n; s++) {
            uint32_t b[4], i, fl, afl, ab, cd, sm, aab, acd, asm;
            c->rc.factor0[s] = rr_u32(&st);
            c->rc.factor1[s] = rr_u32(&st);
            for (i = 0; i < 4u; i++) {
                uint32_t rep = (rr_below(&st, 4u) == 0u);
                b[i] = rr_pick(&st, rep ? a_set : rgb_set) | (rep << 4) | (rr_below(&st, 8u) << 5);
            }
            c->rc.color_icw[s] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
            for (i = 0; i < 4u; i++) {
                uint32_t rep = rr_below(&st, 2u);
                b[i] = rr_pick(&st, rep ? a_set : rgb_set) | (rep << 4) | (rr_below(&st, 8u) << 5);
            }
            if (direct_a && s == 0u) {
                /* The mutation-A shape: A = c0.a, B = c1.a, C = c0.a, D = c1.b, each through one of
                 * the unsigned identity / expand-normal / expand-negate mappings. The two alpha
                 * products c0.a*c1.a and c0.a*c1.b are then generically nonzero and different, and
                 * the sum goes to r0.a with the identity scale and no final combiner, so the stored
                 * byte IS the case's output alpha (see the comment on rr_a_sum_deciding). */
                static const uint32_t dm[3] = { 0u, 2u, 3u };
                b[0] = (1u | 0x10u) | (dm[rr_below(&st, 3u)] << 5);
                b[1] = (2u | 0x10u) | (dm[rr_below(&st, 3u)] << 5);
                b[2] = (1u | 0x10u) | (dm[rr_below(&st, 3u)] << 5);
                b[3] = (2u | 0x00u) | (dm[rr_below(&st, 3u)] << 5);
            }
            c->rc.alpha_icw[s] = (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
            {
                uint32_t abdot = (rr_below(&st, 4u) == 0u), cddot = (rr_below(&st, 4u) == 0u);
                uint32_t mux = (rr_below(&st, 4u) == 0u), b2a_ab, b2a_cd;
                uint32_t scale = k_rr_scale[rr_below(&st, 6u)];

                ab = k_rr_dst[rr_below(&st, 16u)];
                cd = k_rr_dst[rr_below(&st, 16u)];
                sm = k_rr_dst[rr_below(&st, 16u)];
                if (direct_a && s == 0u) {
                    /* The mutation-A shape: stage 0 writes the RGB sum to r0.rgb (the no-final output
                     * needs r0.rgb defined) and nothing to the AB or CD destinations. */
                    ab = 0u; cd = 0u; sm = 12u;
                }
                if (cd && cd == ab) cd = 0;
                if (sm && (sm == ab || sm == cd)) sm = 0;
                b2a_ab = ((rr_below(&st, 4u) == 0u) && ab) ? 1u : 0u;
                b2a_cd = ((rr_below(&st, 4u) == 0u) && cd) ? 1u : 0u;
                if (direct_a && s == 0u) { b2a_ab = 0u; b2a_cd = 0u; }
                fl = (cddot ? 1u : 0u) | (abdot ? 2u : 0u) | (mux ? 4u : 0u) | scale
                   | (b2a_cd ? 0x40u : 0u) | (b2a_ab ? 0x80u : 0u);
                c->rc.color_ocw[s] = (fl << 12) | (sm << 8) | (ab << 4) | cd;
            }
            if (direct_a && s == 0u) {
                /* The mutation-A shape: the alpha sum is the only alpha write and lands in r0.a with
                 * the identity scale, and there is no final combiner, so the stored byte IS the
                 * output alpha and a wrong sum shows up in the pixel with nothing to attenuate it. */
                aab = 0u; acd = 0u; asm = 12u; afl = 0u;
            } else {
                uint32_t amux = (rr_below(&st, 4u) == 0u);
                uint32_t ascale = k_rr_scale[rr_below(&st, 6u)];

                aab = k_rr_dst[rr_below(&st, 16u)];
                acd = k_rr_dst[rr_below(&st, 16u)];
                asm = (rr_below(&st, 4u) == 0u) ? 12u : k_rr_dst[rr_below(&st, 16u)];
                if (acd && acd == aab) acd = 0;
                if (asm && (asm == aab || asm == acd)) asm = 0;
                if (direct_a && s == 0u) {
                    aab = 0u; acd = 0u; asm = 12u; afl = 0u;
                } else {
                    afl = (amux ? 4u : 0u) | ascale;
                }
            }
            if ((fl & 0x80u) && (aab == ab || acd == ab || asm == ab)) {
                if (aab == ab) aab = 0u;
                if (acd == ab) acd = 0u;
                if (asm == ab) asm = 0u;
            }
            if ((fl & 0x40u) && (aab == cd || acd == cd || asm == cd)) {
                if (aab == cd) aab = 0u;
                if (acd == cd) acd = 0u;
                if (asm == cd) asm = 0u;
            }
            c->rc.alpha_ocw[s] = (afl << 12) | (asm << 8) | (aab << 4) | acd;
            if (ab) rgb_set |= 1u << ab;
            if (cd) rgb_set |= 1u << cd;
            if (sm) rgb_set |= 1u << sm;
            if ((fl & 0x80u) && ab) a_set |= 1u << ab;
            if ((fl & 0x40u) && cd) a_set |= 1u << cd;
            if (aab) a_set |= 1u << aab;
            if (acd) a_set |= 1u << acd;
            if (asm) a_set |= 1u << asm;
        }
        c->rc.final_c0 = rr_u32(&st);
        c->rc.final_c1 = rr_u32(&st);
        if (has_final) {
            uint32_t sfin = rgb_set | (((rgb_set >> 12) & 1u) ? (1u << 14) : 0u);
            uint32_t fflags = rr_below(&st, 8u) << 5;
            uint32_t e, f, g, a1, b1, c1_, d1;

            e   = rr_final_byte(&st, a_set, sfin);
            f   = rr_final_byte(&st, a_set, sfin);
            g   = rr_final_byte(&st, a_set, rgb_set);
            a1  = rr_final_byte(&st, a_set, sfin | (1u << 15));
            b1  = rr_final_byte(&st, a_set, sfin | (1u << 15));
            c1_ = rr_final_byte(&st, a_set, sfin | (1u << 15));
            d1  = rr_final_byte(&st, a_set, sfin | (1u << 15));
            c->rc.final1 = (e << 24) | (f << 16) | (g << 8) | fflags;
            c->rc.final0 = (a1 << 24) | (b1 << 16) | (c1_ << 8) | d1;
        }
        if (!rr_walk(c, NULL)) { rej->rej_invalid++; continue; }
        if (!rr_mux_ok(c)) { rej->rej_mux++; continue; }
        rr_inputs(c, v0, v1, fog, t);
        nv2a_rc_eval(&c->rc, v0, v1, fog, t, nominal);
        if (!rr_cond_ok(c, nominal)) { rej->rej_cond++; continue; }
        if (!rr_clear_ok(nominal)) { rej->rej_clear++; continue; }
        c->tries = tries;
        return 1;
    }
    fprintf(stderr, "FAIL [combiner random]: seed 0x%08X case %u: no valid state after %u candidates\n",
            seed, index, RR_MAX_TRIES);
    return 0;
}

/* ── The randomized suite: inventory, coverage, drawing, checking ────────── */

/* The three images the randomized cases bind, decoded exactly as the back end decodes them (so the
 * oracle's texel is the byte the GPU samples), and the fixture that proves it. */
static int rr_load_textures(void)
{
    Nv2aTexture tx;
    uint32_t i, t, u;

    for (t = 0; t < RR_NTEX; t++) {
        memset(&tx, 0, sizeof tx);
        if (t < 3u) {
            tx.offset = k_rr_tex[t].va;
            tx.width = tx.height = 8;
            tx.pitch = k_rr_tex[t].pitch;
            tx.color = k_rr_tex[t].fmt;
            tx.linear = k_rr_tex[t].linear;
        } else {
            /* The four solid A8R8G8B8 images the direct-seam cases bind (the same words and
             * addresses run_four_textures writes, so writing them here changes nothing). */
            tx.offset = TEX_Q(t - 3u);
            tx.width = tx.height = 8;
            tx.pitch = 32;
            tx.color = 0x06u;
            tx.linear = 0u;
        }
        tx.addr_u = tx.addr_v = 3;
        tx.filter = FILTER_NEAREST;
        if (t >= 3u) {
            uint32_t *w = (uint32_t *)G(TEX_Q(t - 3u));
            for (i = 0; i < 64u; i++)
                w[i] = k_q_argb[t - 3u];
        }
        if (!nv2a_backend_decode_texture(&tx, rr_dec[t])) {
            fprintf(stderr, "FAIL [combiner random]: texture %u could not be decoded\n", t);
            return 0;
        }
        for (i = 0; i < 64u; i++)
            nv2a_rc_unpack(rr_dec[t][i], rr_dec_f[t][i]);
    }
    /* The images must be distinguishable, or a case's result could not tell them apart. */
    for (t = 0; t < RR_NTEX; t++)
        for (u = t + 1u; u < RR_NTEX; u++) {
            int k, d = 0;
            for (i = 0; i < 64u; i++)
                for (k = 0; k < 4; k++) {
                    int x = abs((int)(rr_dec[t][i] >> (8 * k) & 255u)
                              - (int)(rr_dec[u][i] >> (8 * k) & 255u));
                    if (x > d) d = x;
                }
            if (d < 32) {
                fprintf(stderr, "FAIL [combiner random]: textures %u and %u are not distinguishable "
                                "(%d steps)\n", t, u, d);
                return 0;
            }
        }
    return 1;
}

/* rr_map_in / rr_map_out are used to reason about generated states (the mux margins
 * and the mutation-A shape), so they are proved against the oracle here: a probe combiner stores a
 * known value in a register and reads it back through the mapping under test, for every mapping and a
 * spread of values, and the two must agree. */
static int rr_fixture_check(RrCover *cov)
{
    static const float vals[5] = { 0.0f, 0.2f, 0.5f, 0.75f, 0.9f };
    Nv2aCombiner rc;
    RrCase c;
    float v0[4], v1[4], fog[4], t[4][4], o[4], R[16][4];
    uint32_t m, v, j;
    int k, ok = 1;

    memset(&c, 0, sizeof c);
    c.diffuse = 0xFF4060C0u;
    c.specular = 0xFF204060u;
    c.fog_col = 0x00003050u;
    c.fog_en = 0;
    for (j = 0; j < 4u; j++) {
        c.t[j][0] = c.t[j][1] = c.t[j][2] = 0.0f;
        c.t[j][3] = 1.0f;
    }
    rr_inputs(&c, v0, v1, fog, t);

    /* map_in: stage 0 stores c0.a (mapping 0 of c0.a times 1, via the alpha sum) into r0.a, then the
     * final combiner reads r0.a through mapping m into the output alpha. */
    for (m = 0; m < 8u; m++) {
        for (v = 0; v < 5u; v++) {
            uint32_t kk = (uint32_t)(vals[v] * 255.0f + 0.5f);
            uint32_t byte = 12u | 0x10u | (m << 5);
            float want, got;

            memset(&rc, 0, sizeof rc);
            rc.stage_program = 1u;                          /* stage 0 on: r0.a starts as t0.a */
            rc.control = 1u;
            rc.alpha_icw[0] = ((1u | 0x10u) << 24) | ((0u | 0x20u) << 16);  /* c0.a * (1 - zero) */
            rc.alpha_ocw[0] = 12u << 8;                     /* the sum -> r0.a, identity scale */
            rc.factor0[0] = kk << 24;
            rc.final0 = 1u;                                 /* nonzero: the final path runs */
            rc.final1 = byte << 8;
            nv2a_rc_eval(&rc, v0, v1, fog, t, o);
            got = o[3];
            want = rr_map_in((float)kk / 255.0f, byte);
            if (want < 0.0f) want = 0.0f;
            if (want > 1.0f) want = 1.0f;
            cov->map_checks++;
            if (fabsf(got - want) > 1.0f / 255.0f) {
                if (cov->map_bad < 4u)
                    fprintf(stderr, "FAIL [combiner random]: rr_map_in(%g, mapping %u) = %g, "
                                    "nv2a_rc_eval gives %g\n", (float)kk / 255.0f, m, want, got);
                cov->map_bad++;
                ok = 0;
            }
        }
    }
    /* map_out: stage 0 stores c0.r (mapping 0 times 1) through the output scale under test into
     * r0.rgb, and the final combiner outputs D = r0.rgb. */
    for (m = 0; m < 6u; m++) {
        uint32_t scale = k_rr_scale[m];
        for (v = 0; v < 5u; v++) {
            uint32_t kk = (uint32_t)(vals[v] * 255.0f + 0.5f);
            float want, got;

            memset(&rc, 0, sizeof rc);
            rc.stage_program = 0u;                          /* stage 0 off: r0.a starts as 1 */
            rc.control = 1u;
            rc.color_icw[0] = ((1u | 0x00u) << 24) | ((0u | 0x20u) << 16);  /* c0.r * (1 - zero) */
            rc.color_ocw[0] = (scale << 12) | (12u << 8);   /* the sum -> r0.rgb */
            rc.factor0[0] = kk << 16;
            rc.final0 = 12u;
            rc.final1 = (12u | 0x10u) << 8;
            nv2a_rc_eval(&rc, v0, v1, fog, t, o);
            got = o[0];
            want = rr_map_out((float)kk / 255.0f, scale);
            if (want < -1.0f) want = -1.0f;
            if (want > 1.0f) want = 1.0f;
            if (want < 0.0f) want = 0.0f;
            if (want > 1.0f) want = 1.0f;
            cov->map_checks++;
            if (fabsf(got - want) > 1.0f / 255.0f) {
                if (cov->map_bad < 8u)
                    fprintf(stderr, "FAIL [combiner random]: rr_map_out(%g, 0x%02X) = %g, "
                                    "nv2a_rc_eval gives %g\n", (float)kk / 255.0f, scale, want, got);
                cov->map_bad++;
                ok = 0;
            }
        }
    }
    /* rr_init_regs must reproduce the register file nv2a_rc_eval builds, and rr_oracle must equal a
     * direct call, for a case that has textures, fog and a final combiner. */
    {
        RrCase q;
        float qo[4], ro[4];

        memset(&q, 0, sizeof q);
        q.diffuse = 0xFF808040u;
        q.specular = 0xC04020A0u;
        q.fog_col = 0x00A05030u;
        q.fog_en = 1;
        q.rc.stage_program = 1u | (4u << 5);
        for (j = 0; j < 4u; j++) {
            q.t[0][j] = (float)j / 8.0f + 0.25f;
            q.t[1][j] = 1.0f - (float)j / 4.0f;
        }
        q.rc.control = 1u;
        q.rc.color_icw[0] = (12u << 24) | (3u << 16) | (5u << 8) | 0u;
        q.rc.color_ocw[0] = 12u << 8;
        q.rc.final0 = 12u;
        q.rc.final1 = (12u | 0x10u) << 8;
        rr_init_regs(&q, R);
        rr_oracle(&q, qo);
        rr_inputs(&q, v0, v1, fog, t);
        nv2a_rc_eval(&q.rc, v0, v1, fog, t, ro);
        for (k = 0; k < 4; k++) {
            cov->map_checks++;
            if (fabsf(qo[k] - ro[k]) > 1.0f / 255.0f) {
                fprintf(stderr, "FAIL [combiner random]: rr_oracle and nv2a_rc_eval disagree (%g vs %g)\n",
                        qo[k], ro[k]);
                cov->map_bad++;
                ok = 0;
            }
        }
        if (R[12][3] != q.t[0][3]) {                 /* r0.a must be texture 0's alpha here */
            fprintf(stderr, "FAIL [combiner random]: rr_init_regs r0.a = %g, not t0.a = %g\n",
                    R[12][3], q.t[0][3]);
            cov->map_bad++;
            ok = 0;
        }
        /* The mux probe must recover r0.a exactly. */
        {
            RrCase p = q;
            float r0a;

            p.rc.alpha_icw[0] = ((1u | 0x10u) << 24) | ((0u | 0x20u) << 16);
            p.rc.alpha_ocw[0] = 12u << 8;
            p.rc.factor0[0] = 0x7F000000u;           /* c0.a = 127/255 */
            r0a = rr_r0a_at(&p, 1u);
            cov->map_checks++;
            if (fabsf(r0a - 127.0f / 255.0f) > 1.0f / 255.0f) {
                fprintf(stderr, "FAIL [combiner random]: the r0.a probe returned %g, not %g\n",
                        r0a, 127.0f / 255.0f);
                cov->map_bad++;
                ok = 0;
            }
        }
    }
    return ok;
}

static uint64_t rr_hash_u32(uint64_t h, uint32_t v)
{
    int b;
    for (b = 0; b < 4; b++) {
        h ^= (v >> (8 * b)) & 0xFFu;
        h *= 0x100000001B3ull;
    }
    return h;
}

/* Generate the whole inventory (deterministic: same case list in both passes). The caller owns the
 * counters; this only adds the generation ones. */
static int rr_generate(RrCover *cov)
{
    uint32_t sd, i;

    for (sd = 0; sd < RR_NSEED; sd++)
        for (i = 0; i < RR_PER_SEED; i++) {
            RrCase *c = &g_rr[sd * RR_PER_SEED + i];
            if (!rr_gen(k_rr_seed[sd], i, c, cov, 0u)) {
                fprintf(stderr, "FAIL [combiner random]: generation exhausted at seed 0x%08X case %u\n",
                        k_rr_seed[sd], i);
                return 0;
            }
        }
    return 1;
}

static void rr_cover_case(const RrCase *c, RrCover *cov, uint32_t *outvals, uint32_t *nout)
{
    float out[4];
    uint32_t n = c->rc.control & 0xFFu, flags = c->rc.control >> 8, st, j;

    rr_oracle(c, out);
    if (c->rc.final0 || c->rc.final1) cov->final_on++; else cov->final_off++;
    if (c->fog_en) cov->fog_on++; else cov->fog_off++;
    if (n >= 2u) {
        if (flags & 0x010u) cov->pc0_cases++;
        if (flags & 0x100u) cov->pc1_cases++;
    }
    for (st = 0; st < 3u; st++)
        cov->mode[(c->rc.stage_program >> (st * 5u)) & 0x1Fu]++;
    if (((c->rc.stage_program >> 15) & 0x1Fu) == 4u) cov->stage3_pass++;
    if (((c->rc.stage_program & 0x1Fu) >= 1u) && ((c->rc.stage_program & 0x1Fu) <= 3u))
        cov->linear_stage0++;
    cov->stage_n[n]++;
    /* The per-case flags rr_walk set while it counted. */
    if (cov->cs_asum_dest) {
        cov->asum_dest++;
        if (c->direct_a) cov->asum_direct++;
    }
    if (cov->cs_raw) cov->raw_cases++;
    if (cov->cs_map2) cov->amap2++;
    if (cov->cs_map2_reach) cov->amap2_reach++;
    if (cov->cs_map3) cov->amap3++;
    if (cov->cs_map3_reach) cov->amap3_reach++;
    /* Mutation A's deciding shape: the alpha sum is written, the two alpha products are nonzero and
     * a*b and a+b cannot quantise to the same byte. */
    if (rr_a_sum_deciding(c)) cov->asum_deciding++;
    if (rr_swap_deciding(c, out)) cov->swap_deciding++;
    for (j = 0; j < 3u; j++) {
        int e = rc_quant(out[j]);
        if (e >= 3 && e <= 252) cov->out_range++;
    }
    if (*nout < RR_N)
        outvals[(*nout)++] = ((uint32_t)rc_quant(out[0]) << 24) | ((uint32_t)rc_quant(out[1]) << 16)
                           | ((uint32_t)rc_quant(out[2]) << 8) | (uint32_t)rc_quant(out[3]);
}

static int rr_cmp_u32(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

typedef struct { const char *name; uint32_t got, min; } RrMin;

static int rr_min_check(const RrMin *m, uint32_t n)
{
    uint32_t i;
    int ok = 1;

    for (i = 0; i < n; i++)
        if (m[i].got < m[i].min) {
            fprintf(stderr, "FAIL [combiner random coverage]: %s = %u, minimum %u\n",
                    m[i].name, m[i].got, m[i].min);
            ok = 0;
        }
    return ok;
}

/* The inventory line, the coverage block and its minimums. Runs once per pass (the generator is
 * deterministic, so both passes build the same case list). */
static int rr_table(void)
{
    static uint32_t outvals[RR_N];
    RrCover cov;
    uint32_t nout = 0, i, distinct = 0;
    uint64_t hash = 0xCBF29CE484222325ull;
    int ok = 1;

    if (!rr_load_textures())
        return 0;
    memset(&cov, 0, sizeof cov);
    if (!rr_fixture_check(&cov))
        ok = 0;
    if (!rr_generate(&cov))
        return 0;
    for (i = 0; i < RR_N; i++) {
        const RrCase *c = &g_rr[i];
        uint32_t s, st;
        hash = rr_hash_u32(hash, c->seed);
        hash = rr_hash_u32(hash, c->index);
        hash = rr_hash_u32(hash, c->rc.control);
        hash = rr_hash_u32(hash, c->rc.stage_program);
        for (s = 0; s < 8u; s++) {
            hash = rr_hash_u32(hash, c->rc.color_icw[s]);
            hash = rr_hash_u32(hash, c->rc.alpha_icw[s]);
            hash = rr_hash_u32(hash, c->rc.color_ocw[s]);
            hash = rr_hash_u32(hash, c->rc.alpha_ocw[s]);
            hash = rr_hash_u32(hash, c->rc.factor0[s]);
            hash = rr_hash_u32(hash, c->rc.factor1[s]);
        }
        hash = rr_hash_u32(hash, c->rc.final0);
        hash = rr_hash_u32(hash, c->rc.final1);
        hash = rr_hash_u32(hash, c->rc.final_c0);
        hash = rr_hash_u32(hash, c->rc.final_c1);
        hash = rr_hash_u32(hash, c->fog_en);
        hash = rr_hash_u32(hash, c->fog_col);
        hash = rr_hash_u32(hash, c->diffuse);
        hash = rr_hash_u32(hash, c->specular);
        for (st = 0; st < 4u; st++) {
            hash = rr_hash_u32(hash, c->mode[st]);
            hash = rr_hash_u32(hash, c->tex[st]);
            for (s = 0; s < 4u; s++)
                hash = rr_hash_u32(hash, f2u(c->coord[st][s]));
        }
        if (!rr_walk(c, &cov)) {
            fprintf(stderr, "FAIL [combiner random]: accepted case seed 0x%08X index %u fails its own "
                            "validity walk\n", c->seed, c->index);
            ok = 0;
        }
        rr_cover_case(c, &cov, outvals, &nout);
    }
    cov.hash = hash;
    qsort(outvals, nout, sizeof outvals[0], rr_cmp_u32);
    for (i = 0; i < nout; i++)
        if (i == 0u || outvals[i] != outvals[i - 1u])
            distinct++;
    cov.distinct = distinct;

    printf("  [combiner random] seeds 0x%08X,0x%08X, %u cases each: %u accepted from %u candidates "
           "(rejected: invalid %u, mux margin %u, conditioning %u, clear-like %u); inventory fnv1a64 "
           "0x%016llX\n", k_rr_seed[0], k_rr_seed[1], RR_PER_SEED, RR_N, cov.cand,
           cov.rej_invalid, cov.rej_mux, cov.rej_cond, cov.rej_clear,
           (unsigned long long)cov.hash);
    printf("  [combiner random coverage]  (name=value; the minimum of each is checked below)\n");
    printf("    cases_accepted=%u/%u cases_drawn_and_compared=%u cases_failed=0\n", RR_N, cov.cand, RR_N);
    printf("    rejected_invalid_read=%u rejected_mux_margin=%u rejected_conditioning=%u "
           "rejected_clear_like=%u\n", cov.rej_invalid, cov.rej_mux, cov.rej_cond, cov.rej_clear);
    printf("    stage_n0=%u stage_n1=%u stage_n2=%u stage_n3=%u stage_n4=%u stage_n5=%u stage_n6=%u "
           "stage_n7=%u stage_n8=%u\n", cov.stage_n[0], cov.stage_n[1], cov.stage_n[2],
           cov.stage_n[3], cov.stage_n[4], cov.stage_n[5], cov.stage_n[6], cov.stage_n[7],
           cov.stage_n[8]);
    printf("    final_on=%u final_off=%u final_map_identity=%u final_map_invert=%u final_ef=%u "
           "final_sum=%u sum_flag_0x20=%u sum_flag_0x40=%u sum_flag_0x80=%u\n",
           cov.final_on, cov.final_off, cov.finalmap[0], cov.finalmap[1], cov.final_ef,
           cov.final_sum, cov.sum_flag[0], cov.sum_flag[1], cov.sum_flag[2]);
    printf("    input_map_m0=%u input_map_m1=%u input_map_m2=%u input_map_m3=%u input_map_m4=%u "
           "input_map_m5=%u input_map_m6=%u input_map_m7=%u\n", cov.inmap[0], cov.inmap[1],
           cov.inmap[2], cov.inmap[3], cov.inmap[4], cov.inmap[5], cov.inmap[6], cov.inmap[7]);
    printf("    rgb_aaa_replicate_reads=%u alpha_a_reads=%u alpha_b_reads=%u\n",
           cov.rgb_rep, cov.alpha_a, cov.alpha_b);
    printf("    source_reg_r0=%u source_reg_r1=%u source_reg_r2=%u source_reg_r3=%u source_reg_r4=%u "
           "source_reg_r5=%u source_reg_r8=%u source_reg_r9=%u source_reg_r10=%u source_reg_r11=%u "
           "source_reg_r12=%u source_reg_r13=%u source_reg_r14=%u source_reg_r15=%u\n",
           cov.srcreg[0], cov.srcreg[1], cov.srcreg[2], cov.srcreg[3], cov.srcreg[4], cov.srcreg[5],
           cov.srcreg[8], cov.srcreg[9], cov.srcreg[10], cov.srcreg[11], cov.srcreg[12],
           cov.srcreg[13], cov.srcreg[14], cov.srcreg[15]);
    printf("    rgb_scale_none=%u rgb_scale_bias=%u rgb_scale_x2=%u rgb_scale_x2bias=%u "
           "rgb_scale_x4=%u rgb_scale_half=%u\n", cov.rgb_scale[0], cov.rgb_scale[1],
           cov.rgb_scale[2], cov.rgb_scale[3], cov.rgb_scale[4], cov.rgb_scale[5]);
    printf("    alpha_scale_none=%u alpha_scale_bias=%u alpha_scale_x2=%u alpha_scale_x2bias=%u "
           "alpha_scale_x4=%u alpha_scale_half=%u\n", cov.alpha_scale[0], cov.alpha_scale[1],
           cov.alpha_scale[2], cov.alpha_scale[3], cov.alpha_scale[4], cov.alpha_scale[5]);
    printf("    ab_dot_cases=%u cd_dot_cases=%u both_dot_cases=%u rgb_mux_stages=%u alpha_mux_stages=%u "
           "mux_lsb_ab=%u mux_lsb_cd=%u mux_msb_ab=%u mux_msb_cd=%u\n", cov.ab_dot, cov.cd_dot,
           cov.both_dot, cov.rgb_mux, cov.alpha_mux, cov.mux_sel[0], cov.mux_sel[1],
           cov.mux_sel[2], cov.mux_sel[3]);
    printf("    blue_to_alpha_ab=%u blue_to_alpha_cd=%u\n", cov.b2a_ab, cov.b2a_cd);
    printf("    dest_r4=%u dest_r5=%u dest_r8=%u dest_r9=%u dest_r10=%u dest_r11=%u dest_r12=%u "
           "dest_r13=%u\n", cov.dst[4], cov.dst[5], cov.dst[8], cov.dst[9], cov.dst[10], cov.dst[11],
           cov.dst[12], cov.dst[13]);
    printf("    read_after_write_reads=%u read_after_write_cases=%u per_stage_c0_cases=%u "
           "per_stage_c1_cases=%u\n", cov.raw_reads, cov.raw_cases, cov.pc0_cases, cov.pc1_cases);
    printf("    fog_on_cases=%u fog_off_cases=%u fog_register_reads=%u\n",
           cov.fog_on, cov.fog_off, cov.fog_reg_reads);
    printf("    mode_none=%u mode_project2d=%u mode_project3d=%u mode_cubemap2d=%u mode_passthru=%u "
           "stage3_passthru_cases=%u linear_stage0_texture_cases=%u\n", cov.mode[0], cov.mode[1],
           cov.mode[2], cov.mode[3], cov.mode[4], cov.stage3_pass, cov.linear_stage0);
    printf("    bound_texture_register_reads=%u passthru_register_reads=%u\n",
           cov.bound_tex_reads, cov.passthru_reads);
    printf("    alpha_sum_dest_cases=%u alpha_sum_dest_deciding_cases=%u alpha_sum_dest_direct_cases=%u\n",
           cov.asum_dest, cov.asum_deciding, cov.asum_direct);
    printf("    alpha_map2_cases=%u alpha_map2_reaching_cases=%u alpha_map3_cases=%u "
           "alpha_map3_reaching_cases=%u swap_mapping_deciding_cases=%u\n", cov.amap2, cov.amap2_reach,
           cov.amap3, cov.amap3_reach, cov.swap_deciding);
    printf("    outputs_in_mid_range=%u distinct_quantised_outputs=%u\n", cov.out_range, cov.distinct);
    printf("    fixture_mapping_probes=%u fixture_mapping_bad=%u textures_decoded=3\n",
           cov.map_checks, cov.map_bad);
    fflush(stdout);

    {
        const RrMin mins[] = {
            { "alpha_sum_dest_cases", cov.asum_dest, 60 },
            { "alpha_sum_dest_deciding_cases", cov.asum_deciding, 20 },
            { "alpha_sum_dest_direct_cases", cov.asum_direct, 20 },
            { "alpha_map2_cases", cov.amap2, 40 },
            { "alpha_map2_reaching_cases", cov.amap2_reach, 20 },
            { "alpha_map3_cases", cov.amap3, 40 },
            { "alpha_map3_reaching_cases", cov.amap3_reach, 20 },
            { "swap_mapping_deciding_cases", cov.swap_deciding, 20 },
            { "final_on_cases", cov.final_on, 250 },
            { "final_off_cases", cov.final_off, 100 },
            { "final_sum_cases", cov.final_sum, 40 },
            { "final_ef_cases", cov.final_ef, 30 },
            { "sum_flag_0x20_cases", cov.sum_flag[0], 10 },
            { "sum_flag_0x40_cases", cov.sum_flag[1], 10 },
            { "sum_flag_0x80_cases", cov.sum_flag[2], 10 },
            { "rgb_aaa_replicate_reads", cov.rgb_rep, 300 },
            { "alpha_a_reads", cov.alpha_a, 300 },
            { "alpha_b_reads", cov.alpha_b, 300 },
            { "ab_dot_cases", cov.ab_dot, 100 },
            { "cd_dot_cases", cov.cd_dot, 100 },
            { "both_dot_cases", cov.both_dot, 30 },
            { "rgb_mux_stages", cov.rgb_mux, 100 },
            { "alpha_mux_stages", cov.alpha_mux, 100 },
            { "mux_lsb_ab", cov.mux_sel[0], 20 },
            { "mux_lsb_cd", cov.mux_sel[1], 20 },
            { "mux_msb_ab", cov.mux_sel[2], 20 },
            { "mux_msb_cd", cov.mux_sel[3], 20 },
            { "blue_to_alpha_ab", cov.b2a_ab, 50 },
            { "blue_to_alpha_cd", cov.b2a_cd, 50 },
            { "dest_r4", cov.dst[4], 50 }, { "dest_r5", cov.dst[5], 50 },
            { "dest_r8", cov.dst[8], 50 }, { "dest_r9", cov.dst[9], 50 },
            { "dest_r10", cov.dst[10], 50 }, { "dest_r11", cov.dst[11], 50 },
            { "dest_r12", cov.dst[12], 300 }, { "dest_r13", cov.dst[13], 300 },
            { "read_after_write_reads", cov.raw_reads, 500 },
            { "read_after_write_cases", cov.raw_cases, 200 },
            { "per_stage_c0_cases", cov.pc0_cases, 60 },
            { "per_stage_c1_cases", cov.pc1_cases, 60 },
            { "fog_on_cases", cov.fog_on, 150 },
            { "fog_off_cases", cov.fog_off, 150 },
            { "fog_register_reads", cov.fog_reg_reads, 100 },
            { "mode_none", cov.mode[0], 100 },
            { "mode_project2d", cov.mode[1], 100 },
            { "mode_project3d", cov.mode[2], 100 },
            { "mode_cubemap2d", cov.mode[3], 100 },
            { "mode_passthru", cov.mode[4], 100 },
            { "stage3_passthru_cases", cov.stage3_pass, 40 },
            { "linear_stage0_texture_cases", cov.linear_stage0, 100 },
            { "bound_texture_register_reads", cov.bound_tex_reads, 200 },
            { "passthru_register_reads", cov.passthru_reads, 50 },
            { "outputs_in_mid_range", cov.out_range, 128 },
            { "distinct_quantised_outputs", cov.distinct, 200 },
            { "stage_n0", cov.stage_n[0], 50 }, { "stage_n1", cov.stage_n[1], 50 },
            { "stage_n2", cov.stage_n[2], 50 }, { "stage_n3", cov.stage_n[3], 50 },
            { "stage_n4", cov.stage_n[4], 50 }, { "stage_n5", cov.stage_n[5], 50 },
            { "stage_n6", cov.stage_n[6], 50 }, { "stage_n7", cov.stage_n[7], 50 },
            { "stage_n8", cov.stage_n[8], 50 },
            { "input_map_m0", cov.inmap[0], 200 }, { "input_map_m1", cov.inmap[1], 200 },
            { "input_map_m2", cov.inmap[2], 200 }, { "input_map_m3", cov.inmap[3], 200 },
            { "input_map_m4", cov.inmap[4], 200 }, { "input_map_m5", cov.inmap[5], 200 },
            { "input_map_m6", cov.inmap[6], 200 }, { "input_map_m7", cov.inmap[7], 200 },
            { "rgb_scale_none", cov.rgb_scale[0], 100 }, { "rgb_scale_bias", cov.rgb_scale[1], 100 },
            { "rgb_scale_x2", cov.rgb_scale[2], 100 }, { "rgb_scale_x2bias", cov.rgb_scale[3], 100 },
            { "rgb_scale_x4", cov.rgb_scale[4], 100 }, { "rgb_scale_half", cov.rgb_scale[5], 100 },
            { "alpha_scale_none", cov.alpha_scale[0], 100 }, { "alpha_scale_bias", cov.alpha_scale[1], 100 },
            { "alpha_scale_x2", cov.alpha_scale[2], 100 }, { "alpha_scale_x2bias", cov.alpha_scale[3], 100 },
            { "alpha_scale_x4", cov.alpha_scale[4], 100 }, { "alpha_scale_half", cov.alpha_scale[5], 100 },
            { "source_reg_r0", cov.srcreg[0], 50 }, { "source_reg_r1", cov.srcreg[1], 50 },
            { "source_reg_r2", cov.srcreg[2], 50 }, { "source_reg_r3", cov.srcreg[3], 50 },
            { "source_reg_r4", cov.srcreg[4], 50 }, { "source_reg_r5", cov.srcreg[5], 50 },
            { "source_reg_r8", cov.srcreg[8], 50 }, { "source_reg_r9", cov.srcreg[9], 50 },
            { "source_reg_r10", cov.srcreg[10], 50 }, { "source_reg_r11", cov.srcreg[11], 50 },
            { "source_reg_r12", cov.srcreg[12], 50 }, { "source_reg_r13", cov.srcreg[13], 50 },
            { "final_map_identity", cov.finalmap[0], 200 },
            { "final_map_invert", cov.finalmap[1], 200 }
        };
        if (!rr_min_check(mins, (uint32_t)(sizeof mins / sizeof mins[0])))
            ok = 0;
    }
    if (cov.map_bad) {
        fprintf(stderr, "FAIL [combiner random coverage]: mapping probes bad = %u, minimum 0\n",
                cov.map_bad);
        ok = 0;
    }
    return ok;
}

/* One case's quad: a 7x7 square in its own 8x8 cell, the same geometry the deterministic sweep uses,
 * carrying the case's own diffuse, specular and four stage coordinates at every vertex (so the
 * interpolated value is the vertex value and the oracle can use it directly). */
static void rr_quad(uint32_t vb, uint32_t j, const RrCase *c)
{
    uint32_t x0 = (j % 8u) * 8u + 1u, y0 = (j / 8u) * 8u + 1u;

    rc_vertex(vb, j * 6u + 0u, (float)x0,        (float)y0,        c->diffuse, c->specular, c->coord);
    rc_vertex(vb, j * 6u + 1u, (float)(x0 + 6u), (float)y0,        c->diffuse, c->specular, c->coord);
    rc_vertex(vb, j * 6u + 2u, (float)x0,        (float)(y0 + 6u), c->diffuse, c->specular, c->coord);
    rc_vertex(vb, j * 6u + 3u, (float)(x0 + 6u), (float)y0,        c->diffuse, c->specular, c->coord);
    rc_vertex(vb, j * 6u + 4u, (float)(x0 + 6u), (float)(y0 + 6u), c->diffuse, c->specular, c->coord);
    rc_vertex(vb, j * 6u + 5u, (float)x0,        (float)(y0 + 6u), c->diffuse, c->specular, c->coord);
}

/* The case's texture bindings. Stage 0 is the linear image (addressed in texels), stages 1 and 2 the
 * swizzled ones (addressed in [0,1]); stage 3 can never be bound through the executor (its OFFSET and
 * FORMAT methods are not admitted), so a stage-3 mode of 1-3 is never generated. A stage in mode 0 or
 * 4 needs no texture, and its enable word is cleared so a previous case's binding cannot leak in. */
static void rr_textures(const RrCase *c)
{
    uint32_t st;

    for (st = 0; st < 3u; st++) {
        uint32_t m = c->mode[st];
        if (m < 1u || m > 3u) {
            pb(0x1B0Cu + 0x40u * st, 0u);
            continue;
        }
        /* The image the oracle sampled for this stage (c->tex[st]), not the stage's default one: the
         * two swizzled images are interchangeable across stages 1 and 2, and binding the other one
         * would compare the GPU's texel with a different texel's oracle value. */
        if (st == 0u)
            pb_texture_on(k_rr_tex[c->tex[st]].va, k_rr_tex[c->tex[st]].fmt, 8u, 8u,
                          k_rr_tex[c->tex[st]].pitch, FILTER_NEAREST);
        else
            pb_texture_swz(st, k_rr_tex[c->tex[st]].va, k_rr_tex[c->tex[st]].fmt, 3u, 3u,
                           FILTER_NEAREST);
    }
}

/* The case's register block, immediately before its own draw: all eight general stages are written
 * even when `control` asks for fewer, because the executor's register file persists across draws and
 * a stage this block did not write would otherwise be inherited from the previous case's block. The
 * cost is 114 dwords, which is what fixes RR_BATCH (see the section comment). */
static void rr_registers(const RrCase *c)
{
    uint32_t s;

    pb(0x1E60, c->rc.control);
    pb(0x1E70, c->rc.stage_program);
    for (s = 0; s < 8u; s++) {
        pb(0x0AC0u + 4u * s, c->rc.color_icw[s]);
        pb(0x0260u + 4u * s, c->rc.alpha_icw[s]);
        pb(0x1E40u + 4u * s, c->rc.color_ocw[s]);
        pb(0x0AA0u + 4u * s, c->rc.alpha_ocw[s]);
        pb(0x0A60u + 4u * s, c->rc.factor0[s]);
        pb(0x0A80u + 4u * s, c->rc.factor1[s]);
    }
    pb(0x0288, c->rc.final0);
    pb(0x028C, c->rc.final1);
    pb(0x1E20, c->rc.final_c0);
    pb(0x1E24, c->rc.final_c1);
    pb(0x02A4, c->fog_en);
    pb(0x02A8, c->fog_col);
}

/* Zero every combiner and texture register the randomized suite touched, back to the never-programmed
 * state. The suite runs inside rc_run(), which the CPU pass executes after the Vulkan pass, and the
 * executor's register file persists for the whole process: without this tail the CPU pass's sweep and
 * whole-surface cases would inherit randomized registers and stop being comparable with the Vulkan
 * pass's. 8 stages x 6 words + the final words, the stage program and the fog enable + the three
 * stages' enable words. */
static void rr_tail(void)
{
    uint32_t s;

    for (s = 0; s < 8u; s++) {
        pb(0x0AC0u + 4u * s, 0u);
        pb(0x0260u + 4u * s, 0u);
        pb(0x1E40u + 4u * s, 0u);
        pb(0x0AA0u + 4u * s, 0u);
        pb(0x0A60u + 4u * s, 0u);
        pb(0x0A80u + 4u * s, 0u);
    }
    pb(0x1E60, 0u);
    pb(0x1E70, 0u);
    pb(0x0288, 0u);
    pb(0x028C, 0u);
    pb(0x1E20, 0u);
    pb(0x1E24, 0u);
    pb(0x02A4, 0u);
    pb(0x1B0C, 0u);
    pb(0x1B4C, 0u);
    pb(0x1B8C, 0u);
}

/* One submission: RR_BATCH cases on one 64x64 A8R8G8B8 target, each case's texture bindings and
 * registers immediately before its own draw, then the register tail. */
static void build_rr_batch(uint32_t b)
{
    const uint32_t surf = RCSURF(RC_S_RAND), vb = RCVB(RC_S_RAND);
    uint32_t j;

    pb_begin(); pb_reset_state();
    pb_surface(surf, 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    for (j = 0; j < RR_BATCH; j++)
        rr_quad(vb, j, &g_rr[b * RR_BATCH + j]);
    for (j = 0; j < RR_BATCH; j++) {
        rr_textures(&g_rr[b * RR_BATCH + j]);
        rr_registers(&g_rr[b * RR_BATCH + j]);
        pb_draw_range(j * 6u, 6u);
    }
    rr_tail();
    pb_flip();
}

/* The raw state of a failing case, so a failure is reproducible from the receipt alone. */
static void rr_dump(const char *label, const RrCase *c, uint32_t b, uint32_t j, int px, int py,
                    const int e[4], const int g[4], int ch, int d)
{
    uint32_t n = c->rc.control & 0xFFu, s;

    fprintf(stderr, "    state: control 0x%08X stage_program 0x%05X final0 0x%08X final1 0x%08X "
                    "final_c0 0x%08X final_c1 0x%08X fog %u 0x%08X diffuse 0x%08X specular 0x%08X\n",
            c->rc.control, c->rc.stage_program, c->rc.final0, c->rc.final1, c->rc.final_c0,
            c->rc.final_c1, c->fog_en, c->fog_col, c->diffuse, c->specular);
    for (s = 0; s < n; s++)
        fprintf(stderr, "    stage %u: icw 0x%08X aicw 0x%08X ocw 0x%08X aocw 0x%08X c0 0x%08X c1 0x%08X\n",
                s, c->rc.color_icw[s], c->rc.alpha_icw[s], c->rc.color_ocw[s], c->rc.alpha_ocw[s],
                c->rc.factor0[s], c->rc.factor1[s]);
    fprintf(stderr, "    modes: %u %u %u %u; textures: %u %u %u %u; t: "
                    "%.6f %.6f %.6f %.6f / %.6f %.6f %.6f %.6f / %.6f %.6f %.6f %.6f / "
                    "%.6f %.6f %.6f %.6f\n",
            c->mode[0], c->mode[1], c->mode[2], c->mode[3], c->tex[0], c->tex[1], c->tex[2],
            c->tex[3], c->t[0][0], c->t[0][1], c->t[0][2], c->t[0][3], c->t[1][0], c->t[1][1],
            c->t[1][2], c->t[1][3], c->t[2][0], c->t[2][1], c->t[2][2], c->t[2][3], c->t[3][0],
            c->t[3][1], c->t[3][2], c->t[3][3]);
    (void)label; (void)b; (void)j; (void)px; (void)py; (void)e; (void)g; (void)ch; (void)d;
}

/* Compare one submission's cells with the oracle: 16 pixels of every cell (the quad covers 7x7 of
 * its 8x8 cell, so the centre 4x4 is unambiguous), the cell's gutter pixel and the unused cells must
 * still be the clear colour. */
static int rr_batch_check(const char *label, const uint8_t *px, uint32_t b, uint32_t *drawn,
                          uint32_t *cmp, int *worst, uint32_t *failed)
{
    uint32_t j;
    int ok = 1;

    for (j = 0; j < 64u; j++) {
        uint32_t cx = (j % 8u) * 8u, cy = (j / 8u) * 8u;
        uint32_t x, y;

        if (j >= RR_BATCH) {                          /* an unused cell must stay the clear colour */
            if (px32(px, 64, (int)cx + 4, (int)cy + 4) != CLR) {
                fprintf(stderr, "FAIL [%s combiner random]: submission %u unused cell %u is not the "
                                "clear colour\n", label, b, j);
                ok = 0;
            }
            continue;
        }
        {
            const RrCase *c = &g_rr[b * RR_BATCH + j];
            float out[4];
            int e[4], k;

            rr_oracle(c, out);
            for (k = 0; k < 4; k++)
                e[k] = rc_quant(out[k]);
            if (px32(px, 64, (int)cx, (int)cy) != CLR) {
                fprintf(stderr, "FAIL [%s combiner random]: seed 0x%08X case %u (submission %u cell %u): "
                                "the cell's gutter pixel is 0x%08X, not the clear colour 0x%08X\n",
                        label, c->seed, c->index, b, j, px32(px, 64, (int)cx, (int)cy), CLR);
                ok = 0;
                continue;
            }
            for (y = cy + 2u; y <= cy + 5u; y++)
                for (x = cx + 2u; x <= cx + 5u; x++) {
                    uint32_t got = px32(px, 64, (int)x, (int)y);
                    int g[4], d = 0, which = -1;

                    g[0] = (int)((got >> 16) & 255u); g[1] = (int)((got >> 8) & 255u);
                    g[2] = (int)(got & 255u);         g[3] = (int)(got >> 24);
                    for (k = 0; k < 4; k++) {
                        int a = abs(e[k] - g[k]);
                        if (a > d) { d = a; which = k; }
                    }
                    (*cmp)++;
                    if (d > *worst) *worst = d;
                    if (d > RC_RTOL) {
                        (*failed)++;
                        fprintf(stderr, "FAIL [%s combiner random]: seed 0x%08X case %u (submission %u "
                                        "cell %u, pixel %d,%d): nv2a_rc_eval says %d,%d,%d,%d, the "
                                        "target has %d,%d,%d,%d (channel %d off by %d)\n",
                                label, c->seed, c->index, b, j, (int)x, (int)y, e[0], e[1], e[2], e[3],
                                g[0], g[1], g[2], g[3], which, d);
                        if (g_rr_dumped++ < RR_MAX_DUMP)
                            rr_dump(label, c, b, j, (int)x, (int)y, e, g, which, d);
                        ok = 0;
                    }
                }
            if (px32(px, 64, (int)cx + 2, (int)cy + 2) != CLR)
                (*drawn)++;
        }
    }
    return ok;
}

/* The randomized suite: generate the inventory, then one submission per RR_BATCH cases, in the same
 * order in both passes. */
static int rr_run(int vulkan)
{
    const char *label = vulkan ? "vulkan" : "software";
    uint32_t b, drawn = 0, cmp = 0, failed = 0;
    int worst = 0, ok;

    ok = rr_table();
    for (b = 0; b < RR_NBATCH; b++) {
        char what[64];
        uint8_t *px;

        build_rr_batch(b);
        prefill_va(RCSURF(RC_S_RAND), SURF_BYTES, SENTINEL);
        _snprintf(what, sizeof what, "%s (combiner random %u/%u)", label, b + 1u, RR_NBATCH);
        if (!submit(what))
            return 0;
        px = snapshot_va(RCSURF(RC_S_RAND), 64u * 64u * 4u);
        if (!px) {
            fprintf(stderr, "FAIL [%s combiner random]: no snapshot for submission %u\n", label, b);
            return 0;
        }
        ok &= rr_batch_check(label, px, b, &drawn, &cmp, &worst, &failed);
        free(px);
    }
    printf("  [%s combiner random] seeds 0x%08X,0x%08X: %u cases in %u submissions, %u drawn, "
           "%u pixels compared, worst channel diff %d (allowed %d), %u failed\n",
           label, k_rr_seed[0], k_rr_seed[1], RR_N, RR_NBATCH, drawn, cmp, worst, RC_RTOL, failed);
    if (failed) {
        fprintf(stderr, "FAIL [%s combiner random]: %u randomized comparisons exceeded the tolerance\n",
                label, failed);
        ok = 0;
    }
    return ok;
}

/* ── Randomized direct-seam cases (four bound textures) ────────────────────
 * The executor cannot bind a texture to stage 3 at all (0x1BC0/0x1BC4 are not admitted), so
 * randomized states that use all four stages' textures go through the back end's own draw contract,
 * exactly as run_four_textures does. Four solid A8R8G8B8 colours are bound to stages 0-3 of every
 * draw, so a stage's sampled colour is independent of the coordinate and the oracle is exact for any
 * mode 1-3. The states are generated by the same rr_gen with `direct` set, so they are re-derivable
 * from (seed, index) like the executor-path ones.
 *
 * This block is Vulkan-only: in the CPU pass nv2a_backend_current() is NULL (nv2a_backend_register
 * (NULL) removed it), so it is guarded on a non-NULL back end exactly as run_four_textures is. Its
 * oracle check is still nv2a_rc_eval plus the four decoded colours, so it remains a real comparison
 * -- the CPU pass's own equivalent is the executor-path randomized suite above, which runs in both.
 */
#define RR_DIRECT_N     128u
#define RR_DIRECT_SEED  0x51ED2701u
#define RR_DIRECT_BATCH 8u

static RrCase g_rrd[RR_DIRECT_N];

/* The direct-seam block's own coverage, over its accepted cases. */
typedef struct {
    uint32_t mode[5], stage3_mode[5], final_on, final_off, stage_n[9];
    uint32_t inmap[8], all4_bound, scale[6], dot_cases, mux_cases, b2a;
    uint64_t hash;
} RrdCover;

static int rrd_min_check(const RrdCover *cv)
{
    const RrMin mins[] = {
        { "direct_cases_accepted", RR_DIRECT_N, 128 },
        { "direct_all_four_stages_bound", cv->all4_bound, 128 },
        { "direct_stage3_textured_cases", cv->stage3_mode[1] + cv->stage3_mode[2]
                                          + cv->stage3_mode[3], 40 },
        { "direct_final_on_cases", cv->final_on, 60 },
        { "direct_final_off_cases", cv->final_off, 20 },
        { "direct_input_map_m0", cv->inmap[0], 100 },
        { "direct_input_map_m1", cv->inmap[1], 100 },
        { "direct_input_map_m2", cv->inmap[2], 100 },
        { "direct_input_map_m3", cv->inmap[3], 100 },
        { "direct_input_map_m4", cv->inmap[4], 100 },
        { "direct_input_map_m5", cv->inmap[5], 100 },
        { "direct_input_map_m6", cv->inmap[6], 100 },
        { "direct_input_map_m7", cv->inmap[7], 100 },
        { "direct_dot_product_cases", cv->dot_cases, 30 },
        { "direct_mux_cases", cv->mux_cases, 20 },
        { "direct_blue_to_alpha_cases", cv->b2a, 20 }
    };
    int ok;

    ok = rr_min_check(mins, (uint32_t)(sizeof mins / sizeof mins[0]));
    printf("  [vulkan combiner random coverage] direct seam, seed 0x%08X, inventory fnv1a64 "
           "0x%016llX\n", RR_DIRECT_SEED, (unsigned long long)cv->hash);
    printf("    direct_stage_n0=%u direct_stage_n1=%u direct_stage_n2=%u direct_stage_n3=%u "
           "direct_stage_n4=%u direct_stage_n5=%u direct_stage_n6=%u direct_stage_n7=%u "
           "direct_stage_n8=%u\n", cv->stage_n[0], cv->stage_n[1], cv->stage_n[2], cv->stage_n[3],
           cv->stage_n[4], cv->stage_n[5], cv->stage_n[6], cv->stage_n[7], cv->stage_n[8]);
    printf("    direct_mode_none=%u direct_mode_project2d=%u direct_mode_project3d=%u "
           "direct_mode_cubemap2d=%u direct_mode_passthru=%u\n", cv->mode[0], cv->mode[1],
           cv->mode[2], cv->mode[3], cv->mode[4]);
    printf("    direct_stage3_none=%u direct_stage3_project2d=%u direct_stage3_project3d=%u "
           "direct_stage3_cubemap2d=%u direct_stage3_passthru=%u\n", cv->stage3_mode[0],
           cv->stage3_mode[1], cv->stage3_mode[2], cv->stage3_mode[3], cv->stage3_mode[4]);
    printf("    direct_final_on=%u direct_final_off=%u direct_scale_cases=%u/%u/%u/%u/%u/%u\n",
           cv->final_on, cv->final_off, cv->scale[0], cv->scale[1], cv->scale[2], cv->scale[3],
           cv->scale[4], cv->scale[5]);
    printf("    direct_input_map_m0=%u m1=%u m2=%u m3=%u m4=%u m5=%u m6=%u m7=%u\n",
           cv->inmap[0], cv->inmap[1], cv->inmap[2], cv->inmap[3], cv->inmap[4], cv->inmap[5],
           cv->inmap[6], cv->inmap[7]);
    printf("    direct_all_four_stages_bound=%u direct_dot_product_cases=%u direct_mux_cases=%u "
           "direct_blue_to_alpha_cases=%u\n", cv->all4_bound, cv->dot_cases, cv->mux_cases, cv->b2a);
    fflush(stdout);
    return ok;
}

static int rrd_run(void)
{
    const Nv2aBackend *be = nv2a_backend_current();
    static Nv2aVertex vtx[RR_DIRECT_BATCH * 6u];
    static Nv2aVertexExtra ex[RR_DIRECT_BATCH * 6u];
    Nv2aTexture tx[4];
    Nv2aRenderState rs;
    Nv2aSurface sf;
    Nv2aBatch bt;
    const uint32_t surf_va = RCSURF(RC_S_RAND);
    uint32_t b, i, j, k, drawn = 0, cmp = 0, failed = 0, cand = 0, rej[4] = { 0, 0, 0, 0 };
    RrdCover cv;
    int worst = 0, ok = 1;

    if (!be || !be->draw || !be->clear || !be->flip)
        return 1;                                 /* the CPU pass: nothing to drive */
    memset(&cv, 0, sizeof cv);
    for (i = 0; i < RR_DIRECT_N; i++) {
        RrCover r;
        uint32_t s, jj;
        memset(&r, 0, sizeof r);
        if (!rr_gen(RR_DIRECT_SEED, i, &g_rrd[i], &r, 1u)) {
            fprintf(stderr, "FAIL [combiner random]: direct-seam generation exhausted at case %u\n", i);
            return 0;
        }
        cand += r.cand;
        rej[0] += r.rej_invalid; rej[1] += r.rej_mux; rej[2] += r.rej_cond; rej[3] += r.rej_clear;
        /* The direct-seam coverage: every stage is bound, so every mode 0-4 is real here. */
        cv.stage_n[g_rrd[i].rc.control & 0xFFu]++;
        if (g_rrd[i].rc.final0 || g_rrd[i].rc.final1) cv.final_on++; else cv.final_off++;
        for (s = 0; s < 4u; s++) {
            uint32_t m = g_rrd[i].mode[s], jj;
            if (s < 3u) cv.mode[m]++;
            else cv.stage3_mode[m]++;
            cv.scale[rr_scale_idx((g_rrd[i].rc.color_ocw[s] >> 12) & 0x38u)]++;
        }
        for (s = 0; s < (g_rrd[i].rc.control & 0xFFu); s++) {
            uint32_t icw = g_rrd[i].rc.color_icw[s], aicw = g_rrd[i].rc.alpha_icw[s], fl;
            for (jj = 0; jj < 4u; jj++) {
                cv.inmap[(icw >> (24u - 8u * jj)) >> 5 & 7u]++;
                cv.inmap[(aicw >> (24u - 8u * jj)) >> 5 & 7u]++;
            }
            fl = g_rrd[i].rc.color_ocw[s] >> 12;
            if (fl & 3u) cv.dot_cases++;
            if ((fl & 4u) && ((g_rrd[i].rc.color_ocw[s] >> 8) & 0xFu)) cv.mux_cases++;
            if (fl & 0xC0u) cv.b2a++;
        }
        cv.all4_bound++;                          /* every direct-seam case binds all four stages */
        cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.control);
        for (s = 0; s < 8u; s++) {
            cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.color_icw[s]);
            cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.alpha_icw[s]);
            cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.color_ocw[s]);
            cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.alpha_ocw[s]);
        }
        cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.stage_program);
        cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.final0);
        cv.hash = rr_hash_u32(cv.hash, g_rrd[i].rc.final1);
    }
    if (!rrd_min_check(&cv))
        ok = 0;
    memset(&rs, 0, sizeof rs);
    rs.color_mask = 0x01010101u;
    rs.blend_eq = 0x8006u; rs.blend_src = 1u;
    rs.alpha_func = rs.depth_func = rs.stencil_func = 0x207u;
    rs.stencil_write_mask = rs.stencil_read_mask = 0xFFu;
    rs.stencil_op_fail = rs.stencil_op_zfail = rs.stencil_op_zpass = 0x1E00u;
    rs.cull_face = 0x405u; rs.front_face = 0x900u;
    rs.depth_max = 16777215.0f;
    memset(&sf, 0, sizeof sf);
    sf.color_va = surf_va; sf.width = 64; sf.height = 64; sf.pitch = 256; sf.bytes_per_pixel = 4;
    sf.color_format = 8; sf.aa_sx = sf.aa_sy = 1;
    for (i = 0; i < 4u; i++) {
        uint32_t *t = (uint32_t *)G(TEX_Q(i));
        for (j = 0; j < 64u; j++)
            t[j] = k_q_argb[i];
        memset(&tx[i], 0, sizeof tx[i]);
        tx[i].offset = TEX_Q(i); tx[i].width = 8; tx[i].height = 8; tx[i].pitch = 32;
        tx[i].color = 0x06u; tx[i].addr_u = tx[i].addr_v = 3; tx[i].filter = FILTER_NEAREST;
    }
    for (b = 0; b < RR_DIRECT_N / RR_DIRECT_BATCH; b++) {
        uint8_t *px;

        prefill_va(surf_va, SURF_BYTES, SENTINEL);
        be->clear(&sf, &rs, 0xF0u, CLR, 0);
        for (j = 0; j < RR_DIRECT_BATCH; j++) {
            const RrCase *c = &g_rrd[b * RR_DIRECT_BATCH + j];
            float x0 = (float)((j % 8u) * 8u + 1u), y0 = (float)((j / 8u) * 8u + 1u);
            static const float cp[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

            for (k = 0; k < 6u; k++) {
                memset(&vtx[j * 6u + k], 0, sizeof vtx[0]);
                vtx[j * 6u + k].x = x0 + cp[k][0] * 6.0f;
                vtx[j * 6u + k].y = y0 + cp[k][1] * 6.0f;
                vtx[j * 6u + k].z = 0.5f; vtx[j * 6u + k].rhw = 1.0f;
                vtx[j * 6u + k].diffuse = c->diffuse;
                memset(&ex[j * 6u + k], 0, sizeof ex[0]);
                ex[j * 6u + k].specular = c->specular;
                ex[j * 6u + k].fog = c->fog_en ? 0.0f : 1.0f;
                for (i = 0; i < 4u; i++)
                    memcpy(ex[j * 6u + k].tex[i], c->coord[i], sizeof ex[0].tex[i]);
            }
            memset(&bt, 0, sizeof bt);
            bt.vertices = &vtx[j * 6u]; bt.count = 6; bt.state = &rs; bt.extra = &ex[j * 6u];
            bt.combiner = &c->rc;
            /* The back end's combiner fog register is (fog_color.rgb, vertex fog factor) with
             * fog_color a D3DCOLOR. backend_fill_rc builds it from SET_FOG_COLOR by moving the R in
             * bits 0-7 up to bits 16-23, so the same transform here gives the shader exactly the
             * rgb rr_inputs hands the oracle. */
            bt.fog_color = ((c->fog_col & 0xFFu) << 16) | (c->fog_col & 0xFF00u)
                         | ((c->fog_col >> 16) & 0xFFu);
            for (i = 0; i < 4u; i++)
                bt.textures[i] = &tx[i];
            be->draw(&sf, &bt);
        }
        be->flip();
        px = snapshot_va(surf_va, 64u * 64u * 4u);
        if (!px) {
            fprintf(stderr, "FAIL [combiner random]: no snapshot for direct-seam submission %u\n", b);
            return 0;
        }
        for (j = 0; j < RR_DIRECT_BATCH; j++) {
            const RrCase *c = &g_rrd[b * RR_DIRECT_BATCH + j];
            float out[4];
            int e[4];
            uint32_t cx = (j % 8u) * 8u, cy = (j / 8u) * 8u, x, y;

            rr_oracle(c, out);
            for (k = 0; k < 4; k++)
                e[k] = rc_quant(out[k]);
            for (y = cy + 2u; y <= cy + 5u; y++)
                for (x = cx + 2u; x <= cx + 5u; x++) {
                    uint32_t got = px32(px, 64, (int)x, (int)y);
                    int g[4], d = 0, which = -1;

                    g[0] = (int)((got >> 16) & 255u); g[1] = (int)((got >> 8) & 255u);
                    g[2] = (int)(got & 255u);         g[3] = (int)(got >> 24);
                    for (k = 0; k < 4; k++) {
                        int a = abs(e[k] - g[k]);
                        if (a > d) { d = a; which = k; }
                    }
                    cmp++;
                    if (d > worst) worst = d;
                    if (d > RC_RTOL) {
                        failed++;
                        fprintf(stderr, "FAIL [vulkan combiner random]: seed 0x%08X case %u "
                                        "(direct-seam submission %u cell %u, pixel %d,%d): nv2a_rc_eval "
                                        "says %d,%d,%d,%d, the target has %d,%d,%d,%d (channel %d off "
                                        "by %d)\n", c->seed, c->index, b, j, (int)x, (int)y, e[0], e[1],
                                e[2], e[3], g[0], g[1], g[2], g[3], which, d);
                        if (g_rr_dumped++ < RR_MAX_DUMP)
                            rr_dump("vulkan", c, b, j, (int)x, (int)y, e, g, which, d);
                        ok = 0;
                    }
                }
            if (px32(px, 64, (int)cx + 2, (int)cy + 2) != CLR)
                drawn++;
        }
        free(px);
    }
    printf("  [vulkan combiner random] direct seam, seed 0x%08X: %u cases in %u submissions, %u drawn, "
           "%u pixels compared, worst channel diff %d (allowed %d), %u failed; %u accepted from %u "
           "candidates (rejected: invalid %u, mux %u, conditioning %u, clear-like %u)\n",
           RR_DIRECT_SEED, RR_DIRECT_N, RR_DIRECT_N / RR_DIRECT_BATCH, drawn, cmp, worst, RC_RTOL,
           failed, RR_DIRECT_N, cand, rej[0], rej[1], rej[2], rej[3]);
    if (failed) {
        fprintf(stderr, "FAIL [vulkan combiner random]: %u direct-seam comparisons exceeded the "
                        "tolerance\n", failed);
        ok = 0;
    }
    if (drawn != RR_DIRECT_N) {
        fprintf(stderr, "FAIL [vulkan combiner random]: %u direct-seam cases did not draw\n",
                RR_DIRECT_N - drawn);
        ok = 0;
    }
    return ok;
}

/* ── Whole-surface combiner cases, compared with the CPU executor ────────── */
#define RC_NSURF 6
#define RC_LERP  0
#define RC_FOUR  1
#define RC_MULTI 2
#define RC_PASS  3
#define RC_CUBE  4
#define RC_CLIP  5
static const char *const k_rc_name[RC_NSURF] = { "lerp", "four stages", "multi-draw",
                                                 "passthru", "cubemap-2d", "clipplane" };
static Surf g_rc_surf[RC_NSURF];
static uint8_t *rc_ref[RC_NSURF], *rc_got[RC_NSURF];

/* Four stages, four different textures: stage 0 is a linear 8x8 R5G6B5 texture addressed in texels
 * (ts = 1/8, 1/8), stages 1 and 2 are swizzled, of different formats and contents (ts = 1, 1), and
 * stage 3 has no usable texture at all (samples white, ts.z = 0). The output is t0*t1 + t2*t3, so
 * every stage must be sampled at its own scale for the pixel to come out right. */
static void build_rc_four(void)
{
    const uint32_t vb = RCVB(RC_S_FOUR);
    float t[4][4];
    static const float px[6][2] = { { 8, 8 }, { 56, 8 }, { 8, 56 }, { 56, 8 }, { 56, 56 }, { 8, 56 } };
    static const float uv[6][4][2] = {
        { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
        { { 8, 0 }, { 1, 0 }, { 1, 0 }, { 1, 0 } },
        { { 0, 8 }, { 0, 1 }, { 0, 1 }, { 0, 1 } },
        { { 8, 0 }, { 1, 0 }, { 1, 0 }, { 1, 0 } },
        { { 8, 8 }, { 1, 1 }, { 1, 1 }, { 1, 1 } },
        { { 0, 8 }, { 0, 1 }, { 0, 1 }, { 0, 1 } }
    };
    uint32_t i, k;

    pb_begin(); pb_reset_state();
    pb_surface(RCSURF(RC_S_FOUR), 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    pb_texture_on(TEX_MAG, 0x11, 8, 8, 16, FILTER_NEAREST);
    pb_texture_swz(1, TEX_RC1, 0x05, 3, 3, FILTER_NEAREST);
    pb_texture_swz(2, TEX_RC2, 0x06, 3, 3, FILTER_NEAREST);
    for (i = 0; i < 6; i++) {
        memset(t, 0, sizeof t);
        for (k = 0; k < 4; k++) {
            t[k][0] = uv[i][k][0]; t[k][1] = uv[i][k][1];
            t[k][2] = 0.0f; t[k][3] = 1.0f;
        }
        /* Stage 0 is the only linear stage, so it is the one whose coordinate scale differs from
         * the swizzled stages'; give it a non-unit q as well (xy scaled by the same 2, so the
         * texels it samples are unchanged but an implementation that skipped the /q divide would
         * sample elsewhere). */
        t[0][0] *= 2.0f; t[0][1] *= 2.0f; t[0][3] = 2.0f;
        rc_vertex(vb, i, px[i][0], px[i][1], 0xFFFFFFFFu, 0, t);
    }
    pb(0x1E60, 1u);
    pb(0x1E70, 1u | (1u << 5) | (1u << 10) | (1u << 15));   /* stages 0-3: PROJECT2D */
    pb(0x0AC0, (RC_T0 << 24) | (RC_T1 << 16) | (RC_T2 << 8) | RC_T3);
    pb(0x1E40, RC_R0 << 8);                        /* the sum -> r0.rgb */
    pb(0x0260, 0); pb(0x0AA0, 0);
    pb(0x0A60, 0); pb(0x0A80, 0);
    pb(0x0288, RC_R0);                             /* final: out = D = r0 */
    pb(0x028C, 0x001C00u);                         /* alpha = r0.a = texture 0's alpha */
    pb(0x1E20, 0xFFFFFFFFu); pb(0x1E24, 0x80808080u);
    pb(0x02A4, 0u);
    pb(0x02A8, RC_FOGCOL);
    pb_draw_triangles(6);
    pb_flip();
    g_rc_surf[RC_FOUR] = (Surf){ RCSURF(RC_S_FOUR), 64, 64, 4, CLR, 4 * 48, 48 * 48, 0 };
}

/* One submission, one target, three draws with DIFFERENT state, so the back end's uniform-block
 * offsets must be distinct and correctly aligned and its descriptor sets reused:
 *   A: a combiner batch the back end cannot translate (stage 0's mode is 7, which vb_rc_supported
 *      rejects), so the GPU draws it through its stage-0 pipeline while the CPU executor still runs
 *      the combiners. This combiner never reads a texture result and the stage-0 pipeline samples
 *      white, so both produce the vertex colour: the stage-0 and combiner pipelines coexist in one
 *      frame and it is still comparable with the CPU reference.
 *   B: a combiner batch, one stage, textured.
 *   C: a combiner batch with two stages and the final combiner's mix.
 * A literal stage-0-vs-combiner split of the same batch is not available: the executor's rc_seen is
 * process-global and sticky, so a batch drawn before the combiners are programmed is a combiner
 * batch on the second pass too. The fallback path is the pipeline split that is stable across passes. */
static void build_rc_multi(void)
{
    const uint32_t vb = RCVB(RC_S_MULTI);
    float t[4][4];
    static const float q[3][4] = { { 4, 4, 28, 28 }, { 36, 4, 60, 28 }, { 4, 36, 60, 60 } };
    static const float cp[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
    double perim = 0.0, area = 0.0;
    uint32_t i, j, k;

    pb_begin(); pb_reset_state();
    pb_surface(RCSURF(RC_S_MULTI), 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    for (i = 0; i < 3; i++) {
        /* The band the reference must cover, taken from the same quad the vertices come from, so a
         * later edit to q[] cannot leave the declared geometry behind. */
        double w = q[i][2] - q[i][0], h = q[i][3] - q[i][1];
        perim += w + h + hypot(w, h);
        area += w * h;
        for (j = 0; j < 6; j++) {
            float x = q[i][0] + cp[j][0] * (q[i][2] - q[i][0]);
            float y = q[i][1] + cp[j][1] * (q[i][3] - q[i][1]);
            memset(t, 0, sizeof t);
            for (k = 0; k < 4; k++) {
                t[k][0] = 0; t[k][1] = 0; t[k][2] = 0; t[k][3] = 1.0f;
            }
            t[0][0] = cp[j][0] * 8.0f; t[0][1] = cp[j][1] * 8.0f;   /* stage 0: texels */
            t[1][0] = cp[j][0]; t[1][1] = cp[j][1];                 /* stage 1: [0,1] */
            rc_vertex(vb, i * 6u + j, x, y, RC_DIFFUSE, RC_SPECULAR, t);
        }
    }
    /* A: mode 7 on stage 0, so the back end falls back to its stage-0 pipeline. */
    pb(0x1E60, 1u);
    pb(0x1E70, 7u);
    pb(0x0AC0, (RC_V0 << 24) | (RC_C1 << 16));
    pb(0x1E40, RC_R0 << 8);
    pb(0x0260, 0); pb(0x0AA0, 0);
    pb(0x0A60, 0); pb(0x0A80, 0xFFFFFFFFu);
    pb(0x0288, RC_R0);
    pb(0x028C, 0x001300u);                         /* alpha = R3.a = the fog factor = 1 */
    pb(0x1E20, 0xFFFFFFFFu); pb(0x1E24, 0x80808080u);
    pb(0x02A4, 0u); pb(0x02A8, RC_FOGCOL);
    pb_draw_range(0, 6);
    /* B: one stage, textured, a different register block. */
    pb_texture_on(TEX_MAG, 0x11, 8, 8, 16, FILTER_NEAREST);
    pb(0x1E60, 1u);
    pb(0x1E70, 1u);
    pb(0x0AC0, (RC_T0 << 24) | (RC_V0 << 16));
    pb(0x1E40, RC_R0 << 8);
    pb(0x0260, (RC_T0 << 24) | (RC_V0 << 16));
    pb(0x0AA0, RC_R0 << 4);
    pb(0x0A60, 0); pb(0x0A80, 0);
    pb(0x0288, RC_R0);
    pb(0x028C, 0x001C00u);
    pb(0x1E20, 0xFFFFFFFFu); pb(0x1E24, 0x80808080u);
    pb(0x02A4, 0u); pb(0x02A8, RC_FOGCOL);
    pb_draw_range(6, 6);
    /* C: two stages and the final combiner's mix. */
    pb(0x1E60, 2u);
    pb(0x1E70, 1u | (1u << 5));
    pb(0x0AC0, (RC_T0 << 24) | (RC_V0 << 16));
    pb(0x1E40, RC_R0 << 8);
    pb(0x0260, 0); pb(0x0AA0, 0);
    pb(0x0A60, 0); pb(0x0A80, 0);
    pb(0x0AC4, (RC_R0 << 24) | (RC_C0 << 16) | (RC_C1 << 8) | RC_V1);
    pb(0x1E44, RC_R0 << 8);
    pb(0x0264, 0); pb(0x0AA4, 0);
    pb(0x0A64, 0x0000003Fu); pb(0x0A84, 0x80808080u);
    pb(0x0288, (RC_V0 << 24) | (RC_C0 << 16) | (RC_R0 << 8));
    pb(0x028C, 0x001C00u);
    pb(0x1E20, 0xFFFFFFFFu); pb(0x1E24, 0x80808080u);
    pb(0x02A4, 0u); pb(0x02A8, RC_FOGCOL);
    pb_draw_range(12, 6);
    pb_flip();
    g_rc_surf[RC_MULTI] = (Surf){ RCSURF(RC_S_MULTI), 64, 64, 4, CLR, perim, area, 0 };
}

/* ── Stage modes the sweep's oracle cannot express ───────────────────────── */
/* The sweep's oracle fixes every stage result at (0,0,0,1), so the stage modes whose result depends
 * on the interpolated coordinate -- PASSTHRU, CUBEMAP on a 2D texture and CLIPPLANE -- are decided
 * by whole-surface cases against the CPU executor instead. */

/* One quad: six vertices of a rectangle, each carrying the four stage coordinates as (x,y,z,w). */
static void rc_quad_verts(uint32_t vb, const float px[6][2], const float uv[6][4][4])
{
    float t[4][4];
    uint32_t i, k;

    for (i = 0; i < 6; i++) {
        memset(t, 0, sizeof t);
        for (k = 0; k < 4; k++)
            memcpy(t[k], uv[i][k], sizeof t[k]);
        rc_vertex(vb, i, px[i][0], px[i][1], 0xFFFFFFFFu, 0, t);
    }
}
/* The register block of a stage-mode case. `clip` is SET_SHADER_CLIP_PLANE_MODE, written even when
 * it is 0 so no case inherits another's clip planes. */
static void rc_mode_registers(uint32_t n, uint32_t prog, uint32_t clip, uint32_t icw, uint32_t ocw,
                              uint32_t factor0, uint32_t fin0, uint32_t fin1)
{
    pb(0x1E60, n);
    pb(0x1E70, prog);
    pb(0x17F8, clip);
    pb(0x0AC0, icw);
    pb(0x1E40, ocw);
    pb(0x0260, 0); pb(0x0AA0, 0);
    pb(0x0A60, factor0); pb(0x0A80, 0xFFFFFFFFu);
    pb(0x0288, fin0); pb(0x028C, fin1);
    pb(0x1E20, 0xFFFFFFFFu); pb(0x1E24, 0x80808080u);
    pb(0x02A4, 0u); pb(0x02A8, RC_FOGCOL);
}

/* Mode 4, PASSTHRU: the stage result is saturate(t0), so the pixel is a direct function of the
 * interpolated coordinate. The coordinates run from -0.25 to 1.25 and the stage adds 0.5 to half of
 * them, so a missing saturate gives 0.376 where the saturate gives 0.502: both are inside (0,1), so
 * the final clamp cannot hide the difference. */
static void build_rc_pass(void)
{
    const uint32_t vb = RCVB(RC_S_PASS);
    static const float px[6][2] = { { 8, 8 }, { 56, 8 }, { 8, 56 }, { 56, 8 }, { 56, 56 }, { 8, 56 } };
    float uv[6][4][4];
    uint32_t i, k;

    for (i = 0; i < 6; i++) {
        float ux = px[i][0] < 32.0f ? -0.25f : 1.25f;
        float uy = px[i][1] < 32.0f ? 0.25f : 0.75f;
        memset(uv[i], 0, sizeof uv[i]);
        for (k = 0; k < 4; k++) {
            uv[i][k][0] = ux; uv[i][k][1] = uy; uv[i][k][3] = 1.0f;
        }
    }
    pb_begin(); pb_reset_state();
    pb_surface(RCSURF(RC_S_PASS), 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    rc_quad_verts(vb, px, uv);
    /* A = t0 with the signed identity mapping, so the saturate of mode 4 is the only clamp;
     * B = c0 = 0.502, C = c0, D = c1 = 1.0: out = saturate(t0) * 0.502 + 0.502. */
    rc_mode_registers(1u, 4u, 0u,
                      ((RC_T0 | (6u << 5)) << 24) | (RC_C0 << 16) | (RC_C0 << 8) | RC_C1,
                      RC_R0 << 8, 0x80808080u, RC_R0, 0x001C00u);
    pb_draw_triangles(6);
    pb_flip();
    g_rc_surf[RC_PASS] = (Surf){ RCSURF(RC_S_PASS), 64, 64, 4, CLR, 4 * 48, 48 * 48, 0 };
}

/* Mode 3, CUBEMAP on a 2D texture: the stage samples t0.xy without dividing by q. The coordinates are
 * texel-addressed (0..8 on a linear 8x8 texture) and t0.w is 2, so an implementation that divided by
 * q -- the mode 1/2 rule -- would sample a different texel and change every pixel. */
static void build_rc_cube(void)
{
    const uint32_t vb = RCVB(RC_S_CUBE);
    static const float px[6][2] = { { 8, 8 }, { 56, 8 }, { 8, 56 }, { 56, 8 }, { 56, 56 }, { 8, 56 } };
    float uv[6][4][4];
    uint32_t i, k;

    for (i = 0; i < 6; i++) {
        float ux = (px[i][0] - 8.0f) * 8.0f / 48.0f;
        float uy = (px[i][1] - 8.0f) * 8.0f / 48.0f;
        memset(uv[i], 0, sizeof uv[i]);
        for (k = 0; k < 4; k++) {
            uv[i][k][0] = ux; uv[i][k][1] = uy; uv[i][k][2] = 0.0f; uv[i][k][3] = 2.0f;
        }
    }
    pb_begin(); pb_reset_state();
    pb_surface(RCSURF(RC_S_CUBE), 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    rc_quad_verts(vb, px, uv);
    pb_texture_on(TEX_MAG, 0x11, 8, 8, 16, FILTER_NEAREST);
    /* out = the stage result itself: A = t0, B = c1 = 1.0, the AB product to r0. */
    rc_mode_registers(1u, 3u, 0u, (RC_T0 << 24) | (RC_C1 << 16), RC_R0 << 4,
                      0xFFFFFFFFu, RC_R0, 0x001C00u);
    pb_draw_triangles(6);
    pb_flip();
    g_rc_surf[RC_CUBE] = (Surf){ RCSURF(RC_S_CUBE), 64, 64, 4, CLR, 4 * 48, 48 * 48, 0 };
}

/* Mode 5, CLIPPLANE: the stage discards the pixel whose coordinate's sign matches the clip-plane
 * bit. Two draws in one submission cover both directions of the comparison:
 *   quad A, clip bit 0 SET   -> the pixels with t0.x >= 0 are discarded, so the LEFT half survives;
 *   quad B, clip bit 0 CLEAR -> the pixels with t0.x <  0 are discarded, so the RIGHT half survives.
 * An inverted comparison swaps the two halves, so each draw's kept region moves and the CPU
 * reference no longer matches. The kept pixels are white (the final combiner's D = c1) and the
 * discarded ones keep the clear colour, so the reference visibly shows both regions. */
static void build_rc_clip(void)
{
    const uint32_t vb = RCVB(RC_S_CLIP);
    static const float px[2][6][2] = {
        { { 4, 4 }, { 28, 4 }, { 4, 28 }, { 28, 4 }, { 28, 28 }, { 4, 28 } },
        { { 36, 4 }, { 60, 4 }, { 36, 28 }, { 60, 4 }, { 60, 28 }, { 36, 28 } }
    };
    static const float mid[2] = { 16.0f, 48.0f };
    double perim, area;
    uint32_t q, i, k;

    pb_begin(); pb_reset_state();
    pb_surface(RCSURF(RC_S_CLIP), 64, 64, 4);
    pb_clear(CLR);
    rc_vertex_program();
    rc_vertex_format(vb);
    for (q = 0; q < 2; q++)
        for (i = 0; i < 6; i++) {
            float t[4][4];
            float ux = px[q][i][0] < mid[q] ? -1.0f : 1.0f;

            memset(t, 0, sizeof t);
            for (k = 0; k < 4; k++) {
                t[k][0] = ux; t[k][1] = 0.5f; t[k][3] = 1.0f;
            }
            rc_vertex(vb, q * 6u + i, px[q][i][0], px[q][i][1], 0xFFFFFFFFu, 0, t);
        }
    /* Both draws use the same combiner (no stage write; the output is the final combiner's D = c1,
     * i.e. white) and differ only in the clip-plane mode. Half of each 24x24 quad survives, so the
     * band is two 12x24 rectangles. */
    {
        double w = 12.0, h = 24.0;
        perim = 2.0 * (w + h + hypot(w, h));
        area = 2.0 * w * h;
    }
    rc_mode_registers(1u, 5u, 0x1u, 0u, 0u, 0xFFFFFFFFu, RC_C1, 0x001C00u);
    pb_draw_range(0, 6);
    rc_mode_registers(1u, 5u, 0x0u, 0u, 0u, 0xFFFFFFFFu, RC_C1, 0x001C00u);
    pb_draw_range(6, 6);
    pb_flip();
    g_rc_surf[RC_CLIP] = (Surf){ RCSURF(RC_S_CLIP), 64, 64, 4, CLR, perim, area, 0 };
}

/* Build and run one whole-surface case. Each pass stores its own pixels: the CPU pass's are the
 * reference the Vulkan pass's are compared with, after both have run. */
static int rc_one(int which, int vulkan)
{
    const Surf *s;
    char label[64];

    if (which == RC_LERP) {
        build_combiner();
        g_rc_surf[RC_LERP] = k_rc_lerp_surf;
    } else if (which == RC_FOUR) {
        build_rc_four();
    } else if (which == RC_MULTI) {
        build_rc_multi();
    } else if (which == RC_PASS) {
        build_rc_pass();
    } else if (which == RC_CUBE) {
        build_rc_cube();
    } else {
        build_rc_clip();
    }
    s = &g_rc_surf[which];
    prefill_va(s->va, SURF_BYTES, SENTINEL);
    _snprintf(label, sizeof label, "%s (rc %s)", vulkan ? "vulkan" : "software", k_rc_name[which]);
    if (!submit(label))
        return 0;
    if (vulkan)
        rc_got[which] = snapshot(s);
    else
        rc_ref[which] = snapshot(s);
    if (!(vulkan ? rc_got[which] : rc_ref[which])) {
        fprintf(stderr, "FAIL: no snapshot for the rc %s case (%s pass)\n", k_rc_name[which],
                vulkan ? "Vulkan" : "CPU");
        return 0;
    }
    return 1;
}

/* The whole combiner suite, in the same order in both passes. */
static int rc_run(int vulkan)
{
    uint8_t *px;
    int ok = 1, i;

    ok &= rc_groups_check();
    build_rc_sweep();
    prefill_va(RCSURF(RC_SWEEP), SURF_BYTES, SENTINEL);
    if (!submit(vulkan ? "vulkan (combiner sweep)" : "software (combiner sweep)"))
        return 0;
    px = snapshot_va(RCSURF(RC_SWEEP), 64u * 64u * 4u);
    if (!px)
        return 0;
    ok &= rc_sweep_check(vulkan ? "vulkan" : "software", px);
    free(px);
    for (i = 0; i < RC_NSURF; i++)
        ok &= rc_one(i, vulkan);
    /* The randomized states run in BOTH passes, exactly like the sweep above: the CPU pass shows the
     * oracle is the arithmetic the executor runs for these states, the Vulkan pass shows the shader
     * matches it. Both are gates. */
    ok &= rr_run(vulkan);
    /* The direct-seam randomized cases bind a texture to all four stages, which the executor cannot
     * do (stage 3's OFFSET/FORMAT are not admitted), so they drive the back end's draw contract
     * directly and are therefore Vulkan-only: in the CPU pass nv2a_backend_current() is NULL. */
    if (vulkan)
        ok &= rrd_run();
    return ok;
}

/* The whole-surface cases: the reference must itself show the drawing and the two passes must agree,
 * with the same tolerances as every other parity case. */
static int rc_compare(void)
{
    int ok = 1, i;

    for (i = 0; i < RC_NSURF; i++) {
        char label[64];
        if (!rc_ref[i] || !rc_got[i]) {
            fprintf(stderr, "FAIL: the rc %s case has no CPU reference or no Vulkan result\n", k_rc_name[i]);
            ok = 0;
            continue;
        }
        _snprintf(label, sizeof label, "rc-%s", k_rc_name[i]);
        ok &= check_parity(label, &g_rc_surf[i], rc_ref[i], rc_got[i], 1);
    }
    return ok;
}

/* ── Four distinguishable textures, through the back end's draw contract ──── */
/* The executor cannot program stage 3's texture (its OFFSET and FORMAT methods are not admitted), so
 * the executor-driven four-stage case leaves stage 3 unbound. This case hands the registered back end
 * a batch with four bound textures directly, through nv2a_backend_current(): four solid colours, each
 * different in every channel including alpha, bound to stages 0-3 of every draw (all four stages are
 * PROJECT2D, so every texture is sampled whatever the combiner reads).
 *   draws 0-3: the final combiner outputs one stage's result, T0, T1, T2 or T3, so each cell must show
 *              that stage's own colour (a stage bound to another stage's image shows that colour);
 *   draw 4:    out = D + C*(1-A) + B*A with A = t0, B = t1, C = t2, D = t3, which depends on all four
 *              stages at once and changes whenever any stage sees another stage's image.
 * The oracle is nv2a_rc_eval with the same four colours, and the fixture is checked first: the
 * colours must be far apart, each single-stage oracle must equal its texture's colour, and replacing
 * any one stage's image by any other's must change the combined oracle. k_q_argb is defined with the
 * randomized suite's texture table, because the randomized direct-seam cases bind the same four. */

static void q_unpack(uint32_t argb, float out[4]) { nv2a_rc_unpack(argb, out); }

static void q_combiner(Nv2aCombiner *rc, int draw)
{
    memset(rc, 0, sizeof *rc);
    rc->control = 1u;
    rc->stage_program = 1u | (1u << 5) | (1u << 10) | (1u << 15);
    rc->final_c0 = 0xFFFFFFFFu;
    rc->final_c1 = 0x80808080u;
    if (draw < 4) {
        rc->final0 = RC_T0 + (uint32_t)draw;
        rc->final1 = RC_A(RC_T0 + (uint32_t)draw) << 8;
    } else {
        rc->final0 = (RC_T0 << 24) | (RC_T1 << 16) | (RC_T2 << 8) | RC_T3;
        rc->final1 = RC_A(RC_T3) << 8;
    }
}

static void q_oracle(int draw, const uint32_t argb[4], float out[4])
{
    Nv2aCombiner rc;
    float v0[4], v1[4] = { 0, 0, 0, 1 }, fog[4] = { 0, 0, 0, 1 }, t[4][4];
    int k;

    q_combiner(&rc, draw);
    q_unpack(0xFFFFFFFFu, v0);
    for (k = 0; k < 4; k++)
        q_unpack(argb[k], t[k]);
    nv2a_rc_eval(&rc, v0, v1, fog, t, out);
}

static int q_fixture_check(void)
{
    int ok = 1, a, b, k;
    float o[4], alt[4];
    uint32_t sub[4];

    for (a = 0; a < 4; a++)
        for (b = a + 1; b < 4; b++) {
            int d = 0;
            for (k = 0; k < 4; k++) {
                int x = abs((int)((k_q_argb[a] >> (8 * k)) & 255u) - (int)((k_q_argb[b] >> (8 * k)) & 255u));
                if (x > d) d = x;
            }
            if (d < 32) {
                fprintf(stderr, "FAIL [four textures]: textures %d and %d are not distinguishable (%d steps)\n",
                        a, b, d);
                ok = 0;
            }
        }
    for (a = 0; a < 4; a++) {
        q_oracle(a, k_q_argb, o);
        if (abs(rc_quant(o[0]) - (int)((k_q_argb[a] >> 16) & 255u)) > RC_RTOL
         || abs(rc_quant(o[1]) - (int)((k_q_argb[a] >> 8) & 255u)) > RC_RTOL
         || abs(rc_quant(o[2]) - (int)(k_q_argb[a] & 255u)) > RC_RTOL
         || abs(rc_quant(o[3]) - (int)(k_q_argb[a] >> 24)) > RC_RTOL) {
            fprintf(stderr, "FAIL [four textures]: the oracle's T%d is %d,%d,%d,%d, not the texture's colour\n",
                    a, rc_quant(o[0]), rc_quant(o[1]), rc_quant(o[2]), rc_quant(o[3]));
            ok = 0;
        }
    }
    q_oracle(4, k_q_argb, o);
    for (a = 0; a < 4; a++)
        for (b = 0; b < 4; b++) {
            int d = 0;
            if (a == b)
                continue;
            memcpy(sub, k_q_argb, sizeof sub);
            sub[a] = k_q_argb[b];                       /* stage a sees stage b's image */
            q_oracle(4, sub, alt);
            for (k = 0; k < 3; k++) {
                int x = abs(rc_quant(o[k]) - rc_quant(alt[k]));
                if (x > d) d = x;
            }
            if (d < RC_GROUP_MIN) {
                fprintf(stderr, "FAIL [four textures]: stage %d bound to image %d changes the combined oracle by only "
                                "%d steps\n", a, b, d);
                ok = 0;
            }
        }
    return ok;
}

static int run_four_textures(void)
{
    const Nv2aBackend *be = nv2a_backend_current();
    const uint32_t surf_va = RCSURF(RC_S_TEX4);
    static Nv2aVertex vtx[6];
    static Nv2aVertexExtra ex[6];
    Nv2aTexture tx[4];
    Nv2aRenderState rs;
    Nv2aSurface sf;
    Nv2aCombiner rc[5];
    Nv2aBatch b;
    uint8_t *px;
    int ok, d, k, drawn = 0, worst = 0;
    uint32_t i, j;

    if (!be || !be->draw || !be->clear || !be->flip) {
        fprintf(stderr, "FAIL [four textures]: no back end is registered (nv2a_backend_current() is NULL)\n");
        return 0;
    }
    ok = q_fixture_check();
    memset(&rs, 0, sizeof rs);
    rs.color_mask = 0x01010101u;
    rs.blend_eq = 0x8006u; rs.blend_src = 1u;
    rs.alpha_func = rs.depth_func = rs.stencil_func = 0x207u;
    rs.stencil_write_mask = rs.stencil_read_mask = 0xFFu;
    rs.stencil_op_fail = rs.stencil_op_zfail = rs.stencil_op_zpass = 0x1E00u;
    rs.cull_face = 0x405u; rs.front_face = 0x900u;
    rs.depth_max = 16777215.0f;
    memset(&sf, 0, sizeof sf);
    sf.color_va = surf_va; sf.width = 64; sf.height = 64; sf.pitch = 256; sf.bytes_per_pixel = 4;
    sf.color_format = 8; sf.aa_sx = sf.aa_sy = 1;
    for (i = 0; i < 4; i++) {
        uint32_t *t = (uint32_t *)G(TEX_Q(i));
        for (j = 0; j < 64; j++)
            t[j] = k_q_argb[i];
        memset(&tx[i], 0, sizeof tx[i]);
        tx[i].offset = TEX_Q(i); tx[i].width = 8; tx[i].height = 8; tx[i].pitch = 32;
        tx[i].color = 0x06; tx[i].addr_u = tx[i].addr_v = 3; tx[i].filter = FILTER_NEAREST;
    }
    for (i = 0; i < 5; i++)
        q_combiner(&rc[i], (int)i);
    prefill_va(surf_va, SURF_BYTES, SENTINEL);
    be->clear(&sf, &rs, 0xF0u, CLR, 0);
    for (i = 0; i < 5; i++) {
        static const float cp[6][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
        float x0 = (float)((i % 8u) * 8u + 1u), y0 = (float)((i / 8u) * 8u + 1u);

        for (j = 0; j < 6; j++) {
            memset(&vtx[j], 0, sizeof vtx[j]);
            vtx[j].x = x0 + cp[j][0] * 6.0f; vtx[j].y = y0 + cp[j][1] * 6.0f;
            vtx[j].z = 0.5f; vtx[j].rhw = 1.0f; vtx[j].diffuse = 0xFFFFFFFFu;
            memset(&ex[j], 0, sizeof ex[j]);
            ex[j].specular = 0xFF000000u; ex[j].fog = 1.0f;
            for (k = 0; k < 4; k++) {
                ex[j].tex[k][0] = 0.5f; ex[j].tex[k][1] = 0.5f; ex[j].tex[k][3] = 1.0f;
            }
        }
        memset(&b, 0, sizeof b);
        b.vertices = vtx; b.count = 6; b.state = &rs; b.extra = ex; b.combiner = &rc[i];
        for (k = 0; k < 4; k++)
            b.textures[k] = &tx[k];
        be->draw(&sf, &b);
    }
    be->flip();
    px = snapshot_va(surf_va, 64u * 64u * 4u);
    if (!px)
        return 0;
    for (i = 0; i < 5; i++) {
        uint32_t got = px32(px, 64, (int)((i % 8u) * 8u + 4u), (int)((i / 8u) * 8u + 4u));
        int g[4] = { (int)((got >> 16) & 255), (int)((got >> 8) & 255), (int)(got & 255), (int)(got >> 24) };
        float o[4];
        int e[4];

        q_oracle((int)i, k_q_argb, o);
        d = 0;
        for (k = 0; k < 4; k++) {
            e[k] = rc_quant(o[k]);
            if (abs(e[k] - g[k]) > d) d = abs(e[k] - g[k]);
        }
        if (d > worst) worst = d;
        if (got != CLR) drawn++;
        if (d > RC_RTOL) {
            fprintf(stderr, "FAIL [four textures]: %s: nv2a_rc_eval says %d,%d,%d,%d, the target has %d,%d,%d,%d\n",
                    i < 4 ? "stage output" : "combined t0..t3", e[0], e[1], e[2], e[3], g[0], g[1], g[2], g[3]);
            ok = 0;
        } else {
            printf("      vulkan   four textures, draw %u (%s)%*s ref %3d,%3d,%3d,%3d  got %3d,%3d,%3d,%3d\n", i,
                   i < 4 ? "T-stage alone" : "D + C(1-A) + BA", i < 4 ? 8 : 0, "", e[0], e[1], e[2], e[3],
                   g[0], g[1], g[2], g[3]);
        }
    }
    free(px);
    printf("  [four textures] 5 draws, %d drawn, worst channel diff %d (allowed %d)\n", drawn, worst, RC_RTOL);
    if (drawn != 5) {
        fprintf(stderr, "FAIL [four textures]: %d draws did not reach the target\n", 5 - drawn);
        ok = 0;
    }
    return ok;
}

/* ── The submission-ring lifetime gate (--frames-in-flight) ────────────────
 *
 * The back end runs a bounded ring of submission slots (nv2a_vk_backend.h,
 * Nv2aVkRingStats): each slot owns one command recording, one fence, one descriptor pool and
 * one upload stream; a submission is issued WITHOUT waiting for it; and the host reaches a
 * fence only to reuse a slot that is still in flight, to publish guest memory (a flip's
 * read-back, a target flush or release) or on device error. That is a claim about lifetimes,
 * and it is worth nothing unless one run can show all four of these at once:
 *
 *   - real overlap, from the ring's own counters: two slots were outstanding at once
 *     (max_in_flight >= 2), a frame was submitted in more than one part (splits >= 1), the
 *     ring wrapped (submissions > frames), and the host performed a fence wait on an occupied
 *     slot (reuse_waits >= 1) and while publishing guest memory (drain_waits >= 1), with
 *     nothing left in flight after each flip's publish. Every one of those is asserted below
 *     and each has a line of its own saying what it means. They are the host's own accounting,
 *     as the header documents: in_flight/max_in_flight say "issued without waiting for the
 *     ones before it", not "the device was still executing", and the wait counters count the
 *     waits performed. The run drives 12 + slots frames (read at run time, so more frames than
 *     the ring has slots, and a one-slot ring -- or a submit that waits for its own fence,
 *     which is no ring at all -- fails the claims and says which one never happened).
 *
 *   - the work really reached the back end, also from the ring's counters: every draw reserves
 *     one descriptor set against its recording slot, and a slot holds VB_SLOT_SETS
 *     (VB_SPLIT_SETS * 64 = 2048) of them and VB_SLOT_STREAM (VB_SPLIT_BYTES * 8 = 16 MiB) of
 *     upload stream before vb_reserve has to move to the next slot, which submits the one it
 *     leaves. So a run that carried N draws CANNOT show fewer than N / VB_SLOT_SETS - slots
 *     submissions, and it also cannot show fewer than (N * 144 bytes) / VB_SPLIT_BYTES - slots
 *     from the six 24-byte vertices each of those draws hands the back end. The gate computes
 *     both bounds from the draws the executor counted and FAILS on them, printing the bound,
 *     the observed counters and the executor's own count -- a workload whose draws were dropped
 *     by the executor (this gate exists because one was: 17992 batches a frame counted, 8 draws
 *     reaching the back end) shows a tiny number of submissions instead and is caught here as
 *     well as by every per-cell colour below. Which threshold moved the slot is on the record
 *     too: `splits` are submissions begun at a draw entry, `overflows` the ones begun because
 *     the stream or the descriptor pool was full.
 *
 *   - guest-visible correctness, checked against the CPU executor: the run is executed twice,
 *     the same pushbuffers first against the Vulkan back end and then against the CPU executor
 *     the rest of this file trusts, and every surface of every frame is checked against that
 *     frame's plan in BOTH passes. Each expectation is arithmetic this file states (see the
 *     note on each check); the CPU pass is what proves the arithmetic, since the flat fills,
 *     the per-cell colours, the clears and the round trips come back out of the CPU executor
 *     as the plan says, rather than the plan being a table that merely agrees with the back
 *     end. On top of that the two passes are compared with each other: every surface a sampler
 *     does not touch must be byte-identical (gate_hash_* below), and the sampled ones must
 *     agree within the step a sampled UNORM is allowed. The correctness is checked in the
 *     frames where targets, textures, depth buffers and framebuffers are evicted and released
 *     while submissions are still pending as well as in the quiet ones.
 *
 *   - attributable content: every draw writes a colour no other draw of that frame and no draw
 *     of any other frame ever writes, so a stream slice, a descriptor set or a constant block
 *     coming from the wrong submission cannot land unnoticed: the counters say the ring
 *     overlapped, and the colours say it overlapped the right things. A cell's colour encodes
 *     (frame, target, cell), and gate_seen() fails the run if any two colours it uses repeat.
 *
 * The gate drives the REAL seam: nv2a_vk_backend_install() and guest pushbuffers through
 * nv2a_submit_pending(), the same one the rest of this file uses. Nothing here re-implements
 * the ring's lifecycle, nothing writes into the back end's state, and the ring is read only
 * through nv2a_vk_backend_ring_stats().
 *
 * Per frame, in guest-memory terms (all addresses are the gate's own 16 MB of guest RAM):
 *
 *   1. the auxiliary surfaces, each fully drawn with this frame's own colour and checked after
 *      the flip: GATE_DST samples GATE_SRC's published bytes (frame 0 samples what was
 *      pre-filled there, since no flip has happened yet, which pins the load path too);
 *      GATE_SRC is this frame's own drawing, which this frame's flip publishes; GATE_HEAVY is a
 *      1024x1024 fill whose read-back at the flip is 4 MB (the device is still busy with it
 *      while the host records the next slots, which is what keeps a submission in flight across
 *      a slot reuse); GATE_R32/GATE_R16 are the read-back/re-load round trip (even frames draw
 *      the whole surface, odd frames one 16x16 corner over the previous frame's bytes, which
 *      must have survived both the flush and the reload); and GATE_PROBE is the depth order: A
 *      at ZFAR, then a nearer B at ZNEAR, and C at ZMID in a submission much later in the same
 *      frame, which must still be rejected -- a depth buffer whose content was dropped between
 *      two submissions would let C through;
 *   2. every grid target, GATE_NGRID of them: cleared (colour, depth and stencil in one clear,
 *      at that target's zeta for this segment) and then drawn cell by cell -- each cell is one
 *      draw and so one descriptor set -- until all of its cells have been considered, 1024 of
 *      them per target at 8x8 pixels each. Two things follow from that cell size and both
 *      matter: an 8-pixel span is what makes the executor's screen-space test accept the batch
 *      at all (a smaller quad is read as object-space geometry and skipped -- the note on
 *      gate_cells), and the cells tile the 256x256 surface exactly, so every pixel of every
 *      target has a name in the plan below;
 *   3. the cells a frame leaves undrawn must still show that frame's clear colour, and which
 *      cells those are walks with the frame, so a cell left over from an earlier frame (a
 *      target that was not really re-cleared, or was flushed or reloaded at the wrong moment)
 *      is a wrong pixel rather than an unobserved one;
 *   4. GATE_TTGT's first GATE_NTEX cells are sampled through GATE_NTEX distinct textures, each
 *      written into guest memory just before the draw that samples it, so the texture cache
 *      (VB_MAX_TEXTURES 512) is pushed over and an evicted texture is re-uploaded while the
 *      ring has submissions in flight. The rotating zetas (GATE_ZN of them > VB_MAX_DEPTHS 8,
 *      GATE_ZSEG segments per target, so GATE_NGRID * GATE_ZSEG > 64 (target, depth) pairs)
 *      release depth images and framebuffers at the same time, and the targets themselves
 *      (GATE_NGRID grid + GATE_AUX_TGT auxiliary > VB_MAX_TARGETS 16) release the least
 *      recently used target, flushed to guest memory, with this frame's submissions pending;
 *   5. the flip, and then every pixel of every surface above is checked against what the frame
 *      drew, and the ring's counters are printed.
 *
 * The frame's draw count is what makes the ring split, and it is fixed by construction rather
 * than by watching the counters: each cell draw hands the back end six 24-byte vertices, so
 * GATE_NGRID * GATE_DRAWN_CELLS draws carry at least 2 MiB of stream (VB_SPLIT_BYTES) and at
 * least 32 descriptor sets (VB_SPLIT_SETS) in every frame. The compile-time assertions below
 * pin both, so a later edit of the constants cannot quietly make the run vacuous.
 *
 * What this is built to catch (the mutants it was written against): a submit that waits for
 * its own fence (the old behaviour) never has two slots in flight, so it fails max_in_flight,
 * reuse_waits and the wrap's own FAIL line, while every colour check still passes -- the
 * failure is on the overlap claim, not on the content. A slot reused before its fence was
 * observed rewrites the stream, the descriptor pool or the command buffer of a submission that
 * is still executing, so it fails the reuse_waits claim (when the fence is no longer observed
 * at all) or the per-cell colours, the round trips and the depth probe (when it is reset before
 * the fence), because the data underneath a running submission changed. And a workload that
 * silently stops reaching the back end -- the failure this gate was repaired for, where the
 * executor counted 17992 batches a frame while the back end drew 8 -- fails the per-cell
 * colours, the reach bound in the counters and the pass-against-pass comparison. */

#define GATE_BASE         0x01000000u   /* the gate's own region, 16 MB into guest RAM */
#define GATE_NGRID        16u           /* grid targets, every one of them drawn every frame */
#define GATE_GRID_W       256u
#define GATE_GRID(i)      (GATE_BASE + (uint32_t)(i) * 0x00040000u)   /* 256x256 A8R8G8B8 */
#define GATE_CELL_PX      8u            /* a drawn cell is an 8x8 pixel square of its target */
#define GATE_GRID_ROW     (GATE_GRID_W / GATE_CELL_PX)                /* 32 cells across */
#define GATE_GRID_CELLS   (GATE_GRID_ROW * GATE_GRID_ROW)             /* 1024 cells per target */
#define GATE_TTGT         (GATE_NGRID - 1u)   /* its cells are the ones sampled through textures */
#define GATE_SKIP_MOD     63u           /* one cell in 63 is left undrawn: it must show the clear */
#define GATE_DRAWN_CELLS  (GATE_GRID_CELLS - (GATE_GRID_CELLS / GATE_SKIP_MOD + 1u))
#define GATE_AUX_TGT      6u            /* DST, SRC, R32, R16, PROBE, HEAVY */
#define GATE_ZSEG         6u            /* zeta segments per grid target */
#define GATE_ZN           10u           /* depth buffers the grid zetas rotate through (> 8) */
#define GATE_NZ           (GATE_ZN + 1u) /* ... plus GATE_ZP, the probe's own */
#define GATE_NTEX         520u          /* distinct textures bound and sampled per frame */
#define GATE_TEX(i)       (GATE_BASE + 0x00C00000u + (uint32_t)(i) * 0x00001000u)
#define GATE_SRC          (GATE_BASE + 0x00400000u)     /* 64x64x4, published by each flip */
#define GATE_DST          (GATE_BASE + 0x00410000u)     /* 64x64x4, samples GATE_SRC */
#define GATE_R32          (GATE_BASE + 0x00420000u)     /* 64x64x4 */
#define GATE_R16          (GATE_BASE + 0x00440000u)     /* 64x64x2, pitch 128 */
#define GATE_PROBE        (GATE_BASE + 0x00460000u)     /* 64x64x4, drawn against GATE_ZP */
#define GATE_HEAVY        (GATE_BASE + 0x00600000u)     /* 1024x1024x4: a 4 MB read-back per flip */
#define GATE_HEAVY_W      1024u
#define GATE_VB           (GATE_BASE + 0x00A00000u)     /* this push's vertices, 24 bytes each */
#define GATE_VB_VERTS     (0x00020000u / 24u)           /* 128 KB per push */
#define GATE_Z(i)         (GATE_BASE + 0x00B00000u + (uint32_t)(i) * 0x00001000u)
#define GATE_ZP           GATE_Z(GATE_ZN)               /* the probe's own: no grid clear touches it */
#define GATE_PB_WORDS     (0x4000u / 4u)                /* the pushbuffer window, in words */
#define GATE_PB_LIMIT     3000u                         /* split the push well before the window ends */
#define GATE_SEEN         (1u << 20)                    /* the gate's colour set */
#define GATE_MAXFRAMES    64u                           /* the gate's per-frame tables */
#define GATE_NHASH        (3u + GATE_TTGT)              /* surfaces compared between the passes by hash */
#define GATE_NCMP         3u                            /* ... and the ones compared pixel by pixel */

/* The back end's split thresholds and what one draw costs it, so that the size of a frame's
 * workload can be checked against them at compile time (below). vb_maybe_split submits a slot
 * once it holds VB_SPLIT_BYTES of upload stream or VB_SPLIT_SETS descriptor sets, and a slot
 * itself holds VB_SLOT_SETS = VB_SPLIT_SETS * 64 sets and VB_SLOT_STREAM = VB_SPLIT_BYTES * 8 of
 * stream before vb_reserve has to move to the next slot: those are the two numbers the reach
 * claim below is computed from. GATE_DRAW_BYTES is the least a cell draw hands the back end --
 * six vertices of 24 bytes -- which is what makes the stream bound conservative. */
#define GATE_SPLIT_BYTES  (2u * 1024u * 1024u)
#define GATE_SPLIT_SETS   32u
#define GATE_SLOT_SETS    (GATE_SPLIT_SETS * 64u)       /* 2048 descriptor sets per slot */
#define GATE_SLOT_STREAM  (GATE_SPLIT_BYTES * 8u)       /* 16 MiB of stream per slot */
#define GATE_DRAW_BYTES   144u                          /* 6 vertices * 24 bytes */

/* The pressure the gate applies has to exceed the caches the back end documents -- 16 targets,
 * 512 textures, 8 depth buffers, 64 framebuffers -- or the eviction paths never run, and a
 * frame's workload has to cross both split thresholds or the ring never advances. Fixed at
 * compile time so a later edit of the constants above cannot quietly stop exercising them. */
typedef char gate_targets_exceed_cache[(GATE_NGRID + GATE_AUX_TGT > 16u) ? 1 : -1];
typedef char gate_textures_exceed_cache[(GATE_NTEX > 512u) ? 1 : -1];
typedef char gate_depths_exceed_cache[(GATE_ZN > 8u) ? 1 : -1];
typedef char gate_fbs_exceed_cache[(GATE_NGRID * GATE_ZSEG > 64u) ? 1 : -1];
typedef char gate_frame_crosses_bytes[(GATE_NGRID * GATE_DRAWN_CELLS * GATE_DRAW_BYTES
                                       >= GATE_SPLIT_BYTES) ? 1 : -1];
typedef char gate_frame_crosses_sets[(GATE_NGRID * GATE_DRAWN_CELLS >= GATE_SPLIT_SETS) ? 1 : -1];

enum {
    GA_DST_CLEAR, GA_SRC, GA_HVY, GA_PROBE_CLR, GA_PA, GA_PB, GA_PC,
    GA_R32_FULL, GA_R32_CORNER, GA_R16_FULL, GA_R16_CORNER, GA_PREV_SRC, GA_N
};

static struct {
    unsigned frame;
    uint32_t aux[GA_N];                          /* this frame's auxiliary colours */
    uint32_t clear;                              /* this frame's grid clear colour */
    uint32_t cell[GATE_NGRID][GATE_GRID_CELLS];  /* what each cell holds this frame; 0: not drawn */
    unsigned cleared[GATE_NGRID];
    uint32_t texseq;                             /* textures bound so far this frame */
    unsigned pushes;
    int pb_fail, dup;
} g_gate;

/* What the NV2A registers are believed to hold, so a draw only re-emits what changed. A push
 * split invalidates all of it (the split starts from the baseline), which is why nothing here
 * has to survive across pushes. */
static struct {
    uint32_t surf, zeta, tex_va, tex_fmt, tex_w, tex_h, tex_pitch, tex_filter;
    int tex_on;
} g_gate_st;
static uint32_t g_gate_vb;                       /* the next free vertex index in this push */
static uint32_t g_gate_seen[GATE_SEEN];
static unsigned g_gate_seen_n;

/* Colours. A cell's channels are even and an auxiliary colour's are odd, so a cell colour can
 * never be taken for the grid's clear colour or an auxiliary one; each family expands its index
 * in base 121 per channel, so it is injective in that index. A cell's index encodes
 * (frame, target, cell) -- 16 frames x 65536 cells -- so the colour identifies the draw that
 * produced a pixel. gate_seen() re-checks that on every colour the run actually uses. */
static uint32_t gate_rgb(uint32_t idx)
{
    return 0xFF000000u | ((8u + 2u * (idx % 121u)) << 16)
                       | ((8u + 2u * ((idx / 121u) % 121u)) << 8)
                       |  (8u + 2u * ((idx / 14641u) % 121u));
}
static uint32_t gate_aux_rgb(uint32_t idx)
{
    return 0xFF000000u | ((9u + 2u * (idx % 121u)) << 16)
                       | ((9u + 2u * ((idx / 121u) % 121u)) << 8)
                       |  (9u + 2u * ((idx / 14641u) % 121u));
}
static uint32_t gate_cell(unsigned n, uint32_t t, uint32_t c)
{
    return gate_rgb((uint32_t)n * 65536u + t * GATE_GRID_CELLS + c);
}
static uint32_t gate_aux(unsigned n, unsigned j)
{
    return gate_aux_rgb(256u + (uint32_t)n * 16u + (uint32_t)j);
}
static uint32_t gate_clr(unsigned n) { return gate_aux_rgb(512u + (uint32_t)n); }

/* A8R8G8B8 to R5G6B5 by truncation: the conversion the back end's read-back of a 16-bit target
 * uses, and the one this file's run_roundtrip16 pins from the other side (RED -> 0xF800). */
static uint16_t trunc565(uint32_t argb)
{
    return (uint16_t)(((argb >> 8) & 0xF800u) | ((argb >> 5) & 0x07E0u) | ((argb >> 3) & 0x001Fu));
}

/* Remember a colour the run is about to draw with. A repeat would mean the gate can no longer
 * tell which draw produced a pixel, so it fails there and then. */
static int gate_seen(uint32_t col)
{
    uint32_t i = ((col * 2654435761u) >> 13) & (GATE_SEEN - 1u);
    unsigned probes = 0;

    while (g_gate_seen[i] && probes++ < GATE_SEEN) {
        if (g_gate_seen[i] == col)
            return 0;
        i = (i + 1u) & (GATE_SEEN - 1u);
    }
    g_gate_seen[i] = col;
    g_gate_seen_n++;
    return 1;
}
static void gate_seen_fail(uint32_t col)
{
    if (g_gate.dup)
        return;
    g_gate.dup = 1;
    fprintf(stderr, "FAIL: ring gate -- colour 0x%08X is used twice: a pixel can no longer be "
                    "attributed to the draw that produced it\n", col);
}

/* ── The pushbuffer: one draw per cell, every draw its own draw ───────────── */
static void gate_unknown(void)
{
    g_gate_st.surf = 0xFFFFFFFFu;
    g_gate_st.zeta = 0xFFFFFFFFu;
    g_gate_st.tex_va = 0u;
    g_gate_st.tex_on = 0;
}

/* The state every push starts from: nothing inherited from the previous push, no zeta, stage 0
 * off, depth test and write off, the vertex layout the gate always uses (24-byte vertices, a
 * float3 position, a D3DCOLOR diffuse and a float2 uv at 16, so every draw is one format). */
static void gate_baseline(void)
{
    pb_reset_state();
    pb_vertex_format(GATE_VB, 1);
    pb(0x1B0C, 0);
    pb(0x0394, f2u(0.0f));                     /* CLIP_MIN z */
    pb(0x0398, f2u(16777215.0f));              /* CLIP_MAX z: the depth scale draws use */
    g_gate_vb = 0;
    gate_unknown();
}

static void gate_submit(const char *what)
{
    int ok;

    if (!g_pbn)
        return;
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = g_pbn * 4u;
    ok = nv2a_submit_pending(g_gpu);
    g_gate.pushes++;
    if (!ok) {
        g_gate.pb_fail = 1;
        fprintf(stderr, "FAIL: pushbuffer rejected (%s): %s, subchannel %u method 0x%04X param 0x%08X, get 0x%X\n",
                what, nv2a_submit_diagnostic(g_gpu->pfifo.submit_diag),
                g_gpu->pfifo.submit_diag_subchannel, g_gpu->pfifo.submit_diag_method,
                g_gpu->pfifo.submit_diag_param, g_gpu->pfifo.submit_diag_get);
    }
}

/* A submission of this frame, on purpose: the ring sees the same thing a guest's next DMA kick
 * would, and the frame stays one frame. */
static void gate_push_split(void)
{
    gate_submit("ring gate (push split)");
    pb_begin();
    gate_baseline();
}
static void gate_need_room(void)
{
    /* A push boundary is transparent to the NV2A registers, but the recording is restarted from
     * the baseline, so the room check has to leave enough for the next draw group (states plus
     * its quads) -- 48 vertices and 128 words covers every group this gate emits. */
    if (g_pbn + 128u > GATE_PB_LIMIT || g_gate_vb + 48u > GATE_VB_VERTS)
        gate_push_split();
}

static void gate_surface(uint32_t va, uint32_t w, uint32_t h, uint32_t bpp)
{
    if (g_gate_st.surf == va)
        return;
    g_gate_st.surf = va;
    pb_surface(va, w, h, bpp);
}
static void gate_zeta(uint32_t z)
{
    if (g_gate_st.zeta == z)
        return;
    g_gate_st.zeta = z;
    pb(0x0214, z);
}
/* Bind stage 0 to a texture; the same block with another address costs one method. */
static void gate_tex(uint32_t va, uint32_t fmt, uint32_t w, uint32_t h, uint32_t pitch,
                     uint32_t filter)
{
    if (g_gate_st.tex_on && g_gate_st.tex_fmt == fmt && g_gate_st.tex_w == w
        && g_gate_st.tex_h == h && g_gate_st.tex_pitch == pitch
        && g_gate_st.tex_filter == filter) {
        if (g_gate_st.tex_va != va) {
            g_gate_st.tex_va = va;
            pb(0x1B00, va);
        }
        return;
    }
    g_gate_st.tex_on = 1;
    g_gate_st.tex_va = va; g_gate_st.tex_fmt = fmt; g_gate_st.tex_w = w;
    g_gate_st.tex_h = h; g_gate_st.tex_pitch = pitch; g_gate_st.tex_filter = filter;
    pb_texture_on(va, fmt, w, h, pitch, filter);
}
static void gate_tex_none(void)
{
    if (!g_gate_st.tex_on)
        return;
    g_gate_st.tex_on = 0;
    pb(0x1B0C, 0);
}
static void gate_tex_data(uint32_t tx, uint32_t col)
{
    uint32_t *p = (uint32_t *)G(GATE_TEX(tx));
    unsigned i;

    for (i = 0; i < 64u; i++)
        p[i] = col;
}

/* One quad as one draw of its own, from the push's vertex buffer. The vertices are in guest
 * memory now and the back end copies them into the recording slot's stream as it records, so
 * the same guest addresses serve the next push. */
static void gate_quad_uv(float x0, float y0, float x1, float y1, float z, uint32_t diffuse,
                         float u0, float v0, float u1, float v1)
{
    uint32_t first = g_gate_vb;

    if (first + 6u > GATE_VB_VERTS || g_pbn + 8u > GATE_PB_WORDS) {
        fprintf(stderr, "FAIL: ring gate -- the pushbuffer ran out of room inside a draw\n");
        g_gate.pb_fail = 1;
        return;
    }
    g_gate_vb = first + 6u;
    put_vertex_z(GATE_VB, (int)(first + 0u), x0, y0, z, diffuse, u0, v0);
    put_vertex_z(GATE_VB, (int)(first + 1u), x1, y0, z, diffuse, u1, v0);
    put_vertex_z(GATE_VB, (int)(first + 2u), x0, y1, z, diffuse, u0, v1);
    put_vertex_z(GATE_VB, (int)(first + 3u), x1, y0, z, diffuse, u1, v0);
    put_vertex_z(GATE_VB, (int)(first + 4u), x1, y1, z, diffuse, u1, v1);
    put_vertex_z(GATE_VB, (int)(first + 5u), x0, y1, z, diffuse, u0, v1);
    pb_draw_range(first, 6);
}
static void gate_quad(float x0, float y0, float x1, float y1, float z, uint32_t diffuse)
{
    gate_quad_uv(x0, y0, x1, y1, z, diffuse, 0.5f, 0.5f, 0.5f, 0.5f);
}

/* The depth buffer a grid target's cell segment uses: several are alive at once (more than the
 * depth cache holds), and rotating them per segment and per frame makes the (target, depth)
 * pairs -- the framebuffers -- churn too. The rotation stays inside GATE_ZN of them so that the
 * probe's own depth buffer (GATE_ZP) is never a grid target's, and no grid clear can wipe the
 * depth A and B left behind for C to be tested against -- that interference, and not the depth
 * logic, is what made the original version of this gate fail its probe on some frames. */
static uint32_t gate_zeta_for(uint32_t t, unsigned n, uint32_t seg)
{
    return GATE_Z((t * GATE_ZSEG + seg + (uint32_t)n) % GATE_ZN);
}

/* Clear a grid target for this frame: the whole surface becomes the frame's clear colour, and
 * the clear carries the target's depth so a depth image of this size exists too. */
static void gate_grid_clear(uint32_t t, unsigned n)
{
    if (g_gate.cleared[t])
        return;
    g_gate.cleared[t] = 1;
    gate_need_room();
    gate_zeta(gate_zeta_for(t, n, 0));
    gate_surface(GATE_GRID(t), GATE_GRID_W, GATE_GRID_W, 4);
    pb(0x1D8C, 0xFFFFFF00u);                   /* the depth clear value: far, stencil 0 */
    pb_clear_flags(g_gate.clear, 0xF3u);       /* colour, depth and stencil, in one clear */
}

/* Which cells a frame leaves undrawn. They must still show that frame's clear colour, which is
 * how the check catches a target that was not really re-cleared -- or was flushed or reloaded at
 * the wrong moment -- and catches a cell left over from an earlier frame. The phase walks with
 * the frame (11 is coprime with 63), so a cell the frame leaves alone is one an earlier or later
 * frame draws. */
static int gate_cell_undrawn(unsigned n, uint32_t t, uint32_t c)
{
    return ((c + 11u * n + 5u * t) % GATE_SKIP_MOD) == 0u;
}

/* Draw every cell of grid target `t` that this frame keeps: one draw, one descriptor set and one
 * place in the vertex buffer per cell. `textured` binds and samples a distinct texture for the
 * target's first GATE_NTEX cells (each written just before the push that samples it), which is
 * what pushes the texture cache over.
 *
 * The quad covers the whole 8x8 cell, and the span is what makes the draw reach the back end at
 * all: the executor's screen-space test (nv2a_pb_exec.c, batch_is_screen_space) reads a batch
 * whose vertices span less than OBJECT_SPACE_SPAN (8) in BOTH axes as object-space geometry and
 * skips it -- as a fixed-function batch it would be transformed by a composite matrix this gate
 * never sets. The original version of this gate drew an inset quad inside a 4x4 cell, a span of
 * 2, so all 17984 of its cell draws a frame were counted by the executor and silently dropped:
 * the back end drew 8 quads a frame, never split (splits=0) and never overlapped
 * (max_in_flight=1). At 8 pixels the cells still tile the 256x256 surface exactly, so the check
 * below can name every pixel. */
static void gate_cells(uint32_t t, int textured)
{
    uint32_t c;

    for (c = 0; c < GATE_GRID_CELLS; c++) {
        uint32_t seg, gx, gy, col;

        if (gate_cell_undrawn(g_gate.frame, t, c))
            continue;
        seg = (c * GATE_ZSEG) / GATE_GRID_CELLS;
        gx = (c % GATE_GRID_ROW) * GATE_CELL_PX;
        gy = (c / GATE_GRID_ROW) * GATE_CELL_PX;
        col = gate_cell(g_gate.frame, t, c);

        gate_need_room();
        if (!gate_seen(col))
            gate_seen_fail(col);
        g_gate.cell[t][c] = col;
        /* A push split restarted the recording from the baseline, so the surface is re-emitted
         * here rather than once per target: after a split this is the first thing the draw needs
         * and the tracked state says so. */
        gate_surface(GATE_GRID(t), GATE_GRID_W, GATE_GRID_W, 4);
        gate_zeta(gate_zeta_for(t, g_gate.frame, seg));
        if (textured && g_gate.texseq < GATE_NTEX) {
            /* A sampled cell: the cell's own colour goes into the texture *and* is what the plan
             * expects to come back, so the diffuse has to be white -- the combiner a never
             * programmed back end runs multiplies the texel by the diffuse, and drawing with the
             * colour in both places would land the square of it (this is exactly what the first
             * version of this gate did: 0xFFEC120A came back as 0xFFDA0100, every channel squared
             * and divided by 255). The texture is written into guest memory before the draw, so
             * the back end uploads this cell's colour on the bind the draw forces. */
            gate_tex(GATE_TEX(g_gate.texseq), 0x12, 8, 8, 32, FILTER_NEAREST);
            gate_tex_data(g_gate.texseq, col);
            g_gate.texseq++;
            gate_quad((float)gx, (float)gy,
                      (float)(gx + GATE_CELL_PX), (float)(gy + GATE_CELL_PX), 0.5f, 0xFFFFFFFFu);
        } else {
            gate_tex_none();
            gate_quad((float)gx, (float)gy,
                      (float)(gx + GATE_CELL_PX), (float)(gy + GATE_CELL_PX), 0.5f, col);
        }
    }
}

/* The probe's third quad: a z between A's and B's, in a submission later than theirs, so the
 * depth buffer of the two earlier submissions must still be there or the quad wins. */
static void gate_probe_c(void)
{
    gate_need_room();
    gate_zeta(GATE_ZP);
    gate_surface(GATE_PROBE, 64, 64, 4);
    pb(0x030C, 1); pb(0x0354, 0x201u); pb(0x035C, 1);
    gate_quad(0.0f, 0.0f, 64.0f, 64.0f, ZMID, g_gate.aux[GA_PC]);
    pb(0x030C, 0); pb(0x035C, 0);
}

static void gate_draw_frame(unsigned n)
{
    uint32_t t;
    unsigned i;

    memset(g_gate.cell, 0, sizeof g_gate.cell);
    memset(g_gate.cleared, 0, sizeof g_gate.cleared);
    g_gate.frame = n;
    g_gate.texseq = 0;
    g_gate.pushes = 0;
    for (i = 0; i < (unsigned)GA_N; i++)
        g_gate.aux[i] = gate_aux(n, (unsigned)i);
    g_gate.clear = gate_clr(n);

    pb_begin();
    gate_baseline();

    /* (1a) GATE_DST samples GATE_SRC's bytes. Frame 0 has no earlier flip: the bytes were
     * pre-filled with a colour no other frame uses, and the sampler must show exactly that. */
    gate_need_room();
    gate_surface(GATE_DST, 64, 64, 4);
    pb_clear_flags(g_gate.aux[GA_DST_CLEAR], 0xF0u);
    gate_tex(GATE_SRC, 0x12, 64, 64, 64u * 4u, FILTER_NEAREST);
    gate_quad_uv(0.0f, 0.0f, 64.0f, 64.0f, 0.5f, 0xFFFFFFFFu, 0.0f, 0.0f, 64.0f, 64.0f);

    /* (1b) GATE_SRC: this frame's drawing, published by this frame's flip. */
    gate_need_room();
    gate_tex_none();
    gate_surface(GATE_SRC, 64, 64, 4);
    pb_clear_flags(g_gate.aux[GA_SRC], 0xF0u);
    gate_quad(0.0f, 0.0f, 64.0f, 64.0f, 0.5f, g_gate.aux[GA_SRC]);

    /* (1c) GATE_HEAVY: the 1024x1024 fill whose read-back at the flip is 4 MB. */
    gate_need_room();
    gate_surface(GATE_HEAVY, GATE_HEAVY_W, GATE_HEAVY_W, 4);
    pb_clear_flags(g_gate.aux[GA_HVY], 0xF0u);
    gate_quad(0.0f, 0.0f, 1024.0f, 1024.0f, 0.5f, g_gate.aux[GA_HVY]);

    /* (1d) The round trips: the whole surface on even frames, one corner over the previous
     * frame's published bytes on odd ones. */
    gate_need_room();
    gate_surface(GATE_R32, 64, 64, 4);
    if ((n & 1u) == 0u) {
        pb_clear_flags(g_gate.aux[GA_R32_FULL], 0xF0u);
        gate_quad(0.0f, 0.0f, 64.0f, 64.0f, 0.5f, g_gate.aux[GA_R32_FULL]);
    } else {
        gate_quad(8.0f, 8.0f, 24.0f, 24.0f, 0.5f, g_gate.aux[GA_R32_CORNER]);
    }
    gate_need_room();
    gate_surface(GATE_R16, 64, 64, 2);
    if ((n & 1u) == 0u) {
        pb_clear_flags(trunc565(g_gate.aux[GA_R16_FULL]), 0xF0u);
        gate_quad(0.0f, 0.0f, 64.0f, 64.0f, 0.5f, g_gate.aux[GA_R16_FULL]);
    } else {
        gate_quad(8.0f, 8.0f, 24.0f, 24.0f, 0.5f, g_gate.aux[GA_R16_CORNER]);
    }

    /* (1e) The depth probe: A (far) and B (near) now; C (between them) much later in the frame,
     * after the grid traffic below has split this frame's recording many times over. The cells
     * are not depth tested -- a 256x256 depth buffer is shared by every target of that size, so
     * a test across targets would be comparing depths of different surfaces -- and the probe's
     * own depth buffer, GATE_ZP, is the one no grid target uses. */
    gate_need_room();
    gate_zeta(GATE_ZP);
    gate_surface(GATE_PROBE, 64, 64, 4);
    pb(0x1D8C, 0xFFFFFF00u);
    pb_clear_flags(g_gate.aux[GA_PROBE_CLR], 0xF3u);
    pb(0x030C, 1); pb(0x0354, 0x201u); pb(0x035C, 1);
    gate_quad(0.0f, 0.0f, 64.0f, 64.0f, ZFAR, g_gate.aux[GA_PA]);
    gate_quad(0.0f, 0.0f, 64.0f, 64.0f, ZNEAR, g_gate.aux[GA_PB]);
    pb(0x030C, 0); pb(0x035C, 0);

    /* (2) Every grid target: cleared -- colour, depth and stencil, at that target's zeta for
     * this segment, so a depth image and a (target, depth) framebuffer of this size exist for
     * each -- and then all of its cells drawn, one draw and one descriptor set per cell. That is
     * GATE_NGRID * GATE_DRAWN_CELLS draws a frame, which is what crosses both split thresholds
     * (see the assertions at the top), and the eviction pressure the rest of the comment
     * describes: GATE_NGRID + GATE_AUX_TGT targets against a 16-entry target cache, GATE_ZN
     * zetas against 8 depth entries, GATE_NGRID * GATE_ZSEG framebuffers against 64, and the
     * 520 textures of GATE_TTGT against 512. The probe's C goes in after the first target, so
     * many submissions separate it from A and B. */
    for (t = 0; t < (uint32_t)GATE_NGRID; t++) {
        gate_grid_clear(t, n);
        gate_cells(t, t == (uint32_t)GATE_TTGT);
        if (t == 0u)
            gate_probe_c();
    }

    /* The frame's end: every target drawn since the last flip goes back to guest memory, which
     * is where every check below reads it. */
    pb_flip();
    gate_submit("ring gate (flip)");
}

/* ── What the frame must have left in guest memory ───────────────────────── */
static int gate_close(uint32_t a, uint32_t b, int tol)
{
    int i;

    if (!tol)
        return a == b;
    for (i = 0; i < 4; i++) {
        int d = (int)((a >> (8 * i)) & 0xFFu) - (int)((b >> (8 * i)) & 0xFFu);
        if (d > tol || d < -tol)
            return 0;
    }
    return 1;
}

static unsigned gate_all32(const char *what, uint32_t va, uint32_t w, uint32_t h, uint32_t want,
                           int tol)
{
    const uint8_t *px = G(va);
    unsigned x, y, bad = 0, shown = 0;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint32_t got = px32(px, w, (int)x, (int)y);
            if (!gate_close(got, want, tol)) {
                if (shown++ < 3)
                    fprintf(stderr, "      %s: (%u,%u) is 0x%08X, want 0x%08X%s\n", what, x, y, got,
                            want, tol ? " (within 1 step)" : "");
                bad++;
            }
        }
    printf("      %-52s %s (%u of %u pixels wrong)\n", what, bad ? "FAIL" : "ok", bad, w * h);
    return bad;
}

/* The whole surface must be `outside`, except the rectangle [x0,x1) x [y0,y1), which must hold
 * `inside` (an empty rectangle for the frames that draw the whole surface). `tol` is the
 * per-channel slack the pass is allowed (0 for the Vulkan back end's flat colours). */
static unsigned gate_two32(const char *what, uint32_t va, uint32_t w, uint32_t h, uint32_t outside,
                           uint32_t inside, int x0, int y0, int x1, int y1, int tol)
{
    const uint8_t *px = G(va);
    unsigned x, y, bad = 0, shown = 0;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint32_t want = ((int)x >= x0 && (int)x < x1 && (int)y >= y0 && (int)y < y1) ? inside
                                                                                         : outside;
            uint32_t got = px32(px, w, (int)x, (int)y);
            if (!gate_close(got, want, tol)) {
                if (shown++ < 3)
                    fprintf(stderr, "      %s: (%u,%u) is 0x%08X, want 0x%08X\n", what, x, y, got, want);
                bad++;
            }
        }
    printf("      %-52s %s (%u of %u pixels wrong)\n", what, bad ? "FAIL" : "ok", bad, w * h);
    return bad;
}
/* The same for a 5:6:5 surface, in 8-bit channels: unpack() expands both words the way the rest
 * of this file does, and `tol` is the resulting per-channel slack (0 against the back end, whose
 * truncation this file pins in run_roundtrip16 with RED -> 0xF800; TOL_565 against the CPU
 * executor, whose rounding is the file's own tolerance). */
static unsigned gate_two16(const char *what, uint32_t va, uint32_t w, uint32_t h, uint16_t outside,
                           uint16_t inside, int x0, int y0, int x1, int y1, int tol)
{
    const uint8_t *px = G(va);
    unsigned x, y, bad = 0, shown = 0;

    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            uint16_t want16 = ((int)x >= x0 && (int)x < x1 && (int)y >= y0 && (int)y < y1) ? inside
                                                                                           : outside;
            uint16_t got16 = (uint16_t)px16(px, w, (int)x, (int)y);
            int want[4], got[4];

            unpack((const uint8_t *)&want16, 2, want);
            unpack((const uint8_t *)&got16, 2, got);
            if (maxdiff(got, want, 1) > tol) {
                if (shown++ < 3)
                    fprintf(stderr, "      %s: (%u,%u) is 0x%04X, want 0x%04X\n", what, x, y, got16, want16);
                bad++;
            }
        }
    printf("      %-52s %s (%u of %u pixels wrong)\n", what, bad ? "FAIL" : "ok", bad, w * h);
    return bad;
}

/* Every pixel of every grid target is either the colour of the cell that contains it -- the 8x8
 * cells tile the 256x256 surface exactly, so there is no pixel the plan does not name -- or, for
 * the cells this frame left undrawn, this frame's clear colour. A cell drawn in an earlier frame
 * and not in this one shows up here as the clear being wrong; a target that was flushed,
 * reloaded or re-cleared at the wrong moment shows up as either. The one target whose draws were
 * sampled through a texture is allowed GATE_TEX_TOL steps per channel, because that value went
 * through a UNORM sampler -- the only path in this gate the arithmetic cannot pin exactly. */
static unsigned gate_check_grid(const char *pass, int tol)
{
    unsigned t, x, y, bad = 0, shown = 0, total = 0;

    for (t = 0; t < GATE_NGRID; t++) {
        const uint8_t *px = G(GATE_GRID(t));
        int cell_tol = ((int)t == (int)GATE_TTGT) ? tol : 0;

        for (y = 0; y < GATE_GRID_W; y++)
            for (x = 0; x < GATE_GRID_W; x++) {
                uint32_t got = px32(px, GATE_GRID_W, (int)x, (int)y);
                uint32_t want = g_gate.cell[t][(y / GATE_CELL_PX) * GATE_GRID_ROW
                                              + (x / GATE_CELL_PX)];

                if (!want)
                    want = g_gate.clear;
                if (!gate_close(got, want, cell_tol)) {
                    if (shown++ < 3)
                        fprintf(stderr, "      [%s] grid target %u: (%u,%u) is 0x%08X, want 0x%08X\n",
                                pass, t, x, y, got, want);
                    bad++;
                }
                total++;
            }
    }
    printf("      [%s] %-48s %s (%u of %u pixels wrong)\n", pass,
           "grids: every pixel its cell's colour, or the frame's clear",
           bad ? "FAIL" : "ok", bad, total);
    return bad;
}

/* The CPU pass's probe check: every pixel of GATE_PROBE must be one of the three quads' own
 * colours -- A's, B's or C's -- which pins the probe's colour attribution and its clears without
 * asserting a depth order the executor does not implement on this path (see the note in
 * gate_check_frame). The line reports which quad the executor's last-draw-wins behaviour left,
 * so the difference from the back end is on the record rather than hidden in a tolerance. */
static int gate_probe_any(const char *pass)
{
    static const int want_i[3] = { GA_PA, GA_PB, GA_PC };
    const uint8_t *px = G(GATE_PROBE);
    unsigned x, y, bad = 0, seen[3] = { 0u, 0u, 0u }, shown = 0;
    int k;

    for (y = 0; y < 64u; y++)
        for (x = 0; x < 64u; x++) {
            uint32_t got = px32(px, 64u, (int)x, (int)y);
            int hit = -1;

            for (k = 0; k < 3; k++)
                if (gate_close(got, g_gate.aux[want_i[k]], 0))
                    hit = k;
            if (hit < 0) {
                if (shown++ < 3)
                    fprintf(stderr, "      [%s] probe: (%u,%u) is 0x%08X, which is none of A, B or C\n",
                            pass, x, y, got);
                bad++;
            } else {
                seen[hit]++;
            }
        }
    printf("      [%s] %-48s %s (%u of 4096 pixels wrong; kept %u A, %u B, %u C)\n", pass,
           "probe: every pixel is one of the three probe quads' colours",
           bad ? "FAIL" : "ok", bad, seen[0], seen[1], seen[2]);
    return bad == 0;
}

/* The tolerances each pass's arithmetic is allowed against the plan: [sampled, rasterised,
 * 5:6:5]. On the Vulkan back end every rasterised surface is the plan's colour exactly (which
 * the run shows: every check below is exact on that pass except the two the sampler touches),
 * and a sampled UNORM is allowed one step. On the CPU executor -- the pass that proves the
 * arithmetic -- the file's own software tolerances apply (TOL_8888 for an 8888 surface, TOL_565
 * for a 5:6:5 one, the same ones check_parity uses on this executor's reference pixels). */
static const int k_gate_tol_vk[3]  = { 1, 0, 0 };
static const int k_gate_tol_cpu[3] = { 1, TOL_8888, TOL_565 };
#define GATE_TEX_TOL 1

static int gate_check_frame(unsigned n, const char *pass, const int tol[3])
{
    char what[128];
    int ok = 1;

    ok &= gate_check_grid(pass, GATE_TEX_TOL) == 0;

    _snprintf(what, sizeof what, "[%s] heavy: this frame's own 1024x1024 fill", pass);
    ok &= gate_all32(what, GATE_HEAVY, GATE_HEAVY_W, GATE_HEAVY_W, g_gate.aux[GA_HVY], tol[1]) == 0;

    /* SRC is cleared and drawn with the same colour, so a frame that lost either one, or drew
     * it late, or published the wrong buffer, is a wrong pixel. */
    _snprintf(what, sizeof what, "[%s] SRC: what this frame drew", pass);
    ok &= gate_all32(what, GATE_SRC, 64, 64, g_gate.aux[GA_SRC], tol[1]) == 0;

    /* The sampler path: DST holds GATE_SRC's *published* bytes, so frame n must show what frame
     * n-1's flip wrote there -- and frame 0 must show what was pre-filled before any flip, which
     * pins the load of a target from guest memory as well as its store. */
    _snprintf(what, sizeof what, "[%s] DST: %s, through a texture", pass,
              n ? "the previous flip's SRC" : "the bytes pre-filled into SRC");
    ok &= gate_all32(what, GATE_DST, 64, 64,
                     n ? gate_aux(n - 1u, GA_SRC) : gate_aux(0, GA_PREV_SRC), tol[0]) == 0;

    /* The depth order: B is nearer than A over the same pixels and both are drawn now, so B's
     * colour is what a depth buffer that survived shows. C is drawn at ZMID in a submission much
     * later in the frame, after the depth clear of A/B's segment and after many splits; a depth
     * buffer whose contents were dropped between two submissions -- or re-created from another
     * size's bytes -- would let C through and leave C's colour there instead. On the back end
     * that is an absolute check: all three quads cover the whole surface, so the whole surface
     * must be B's colour, exactly. The CPU pass is the exception, and deliberately so: this
     * file's own convention is that the software executor's depth handling on these
     * pre-transformed screen-space batches is informational (main runs the depth cases against
     * the executor only under "-- information only: the software executor on the absolute cases
     * --"), and it in fact keeps the last quad drawn. So there the probe is checked for what the
     * executor does claim: every pixel is one of the three probe quads' own colours, and the run
     * reports which of them it kept. */
    if (tol == k_gate_tol_vk) {
        _snprintf(what, sizeof what, "[%s] probe: A (far), nearer B (kept), C between (rejected)", pass);
        ok &= gate_all32(what, GATE_PROBE, 64, 64, g_gate.aux[GA_PB], tol[1]) == 0;
    } else {
        ok &= gate_probe_any(pass);
    }

    if ((n & 1u) == 0u) {
        /* The whole surface drawn and cleared this frame. */
        _snprintf(what, sizeof what, "[%s] R32: this frame's read-back", pass);
        ok &= gate_two32(what, GATE_R32, 64, 64, g_gate.aux[GA_R32_FULL],
                         g_gate.aux[GA_R32_FULL], 0, 0, 0, 0, tol[1]) == 0;
        _snprintf(what, sizeof what, "[%s] R16: this frame's read-back, 5:6:5", pass);
        ok &= gate_two16(what, GATE_R16, 64, 64, trunc565(g_gate.aux[GA_R16_FULL]),
                         trunc565(g_gate.aux[GA_R16_FULL]), 0, 0, 0, 0, tol[2]) == 0;
    } else {
        /* Only the corner is drawn: every other byte must still be the previous frame's, which
         * means the flip published it, the target was reloaded from those bytes, and the load
         * did not disturb them. */
        _snprintf(what, sizeof what, "[%s] R32: the previous frame's bytes survived the re-load", pass);
        ok &= gate_two32(what, GATE_R32, 64, 64, gate_aux(n - 1u, GA_R32_FULL),
                         g_gate.aux[GA_R32_CORNER], 8, 8, 24, 24, tol[1]) == 0;
        _snprintf(what, sizeof what, "[%s] R16: the previous frame's 5:6:5 words survived the re-load",
                  pass);
        ok &= gate_two16(what, GATE_R16, 64, 64, trunc565(gate_aux(n - 1u, GA_R16_FULL)),
                         trunc565(g_gate.aux[GA_R16_CORNER]), 8, 8, 24, 24, tol[2]) == 0;
    }
    return ok;
}

/* ── The gate's own fixtures and claims ─────────────────────────────────── */
static int gate_claim(const char *what, int hold)
{
    printf("      %-52s %s\n", what, hold ? "ok" : "FAIL");
    if (!hold)
        fprintf(stderr, "FAIL: ring gate -- %s (the counters above are the evidence)\n", what);
    return hold;
}

static int gate_fixture(unsigned frames, unsigned slots)
{
    unsigned n, j, bad = 0, sampled = 0;
    int ok = 1;
    char line[160];

    /* The two colour families never meet: a cell colour can never be taken for the grid's clear
     * colour or an auxiliary one. The mask is the low bit of the three colour bytes only (0x0001
     * in each of R, G and B): the alpha byte is 0xFF on every one of these colours, so a mask
     * that included it would call every colour odd. gate_rgb's channels are 8 + 2k and
     * gate_aux_rgb's are 9 + 2k, so the two are disjoint by construction. Spot-checked over the
     * whole index range; the values the run actually uses are checked as it uses them. */
    for (n = 0; n < 121u * 121u; n += 977u) {
        if (gate_rgb(n) & 0x00010101u)
            bad++;
        if ((gate_aux_rgb(n) & 0x00010101u) != 0x00010101u)
            bad++;
        sampled += 2u;
    }
    printf("      %-52s %s (%u of %u sampled indices wrong)\n",
           "cell colours even, auxiliary colours odd", bad ? "FAIL" : "ok", bad, sampled);
    ok &= bad == 0;

    /* The auxiliary and clear colours of every frame, before any of them is drawn: a repeat
     * would mean the gate cannot tell two of its own surfaces apart. */
    bad = 0;
    for (n = 0; n < frames; n++) {
        if (!gate_seen(gate_clr(n)))
            bad++;
        for (j = 0; j < (unsigned)GA_N; j++)
            if (!gate_seen(gate_aux(n, j)))
                bad++;
    }
    printf("      %-52s %s (%u of %u repeated)\n",
           "every auxiliary and clear colour of the run is distinct", bad ? "FAIL" : "ok", bad,
           frames * ((unsigned)GA_N + 1u));
    ok &= bad == 0;

    /* The pressure this run applies, against the caches the back end documents (VB_MAX_TARGETS
     * 16, VB_MAX_TEXTURES 512, VB_MAX_DEPTHS 8, VB_MAX_FBS 64), and the size of a frame's
     * workload against the two thresholds that move the ring to its next slot. The compile-time
     * assertions above pin the arithmetic; these lines report it. */
    _snprintf(line, sizeof line,
              "pressure per frame: %u targets, %u textures, %u depth buffers, %u (target, depth) pairs",
              GATE_NGRID + GATE_AUX_TGT, GATE_NTEX, GATE_NZ, GATE_NGRID * GATE_ZSEG);
    ok &= gate_claim(line, GATE_NGRID + GATE_AUX_TGT > 16u && GATE_NTEX > 512u && GATE_NZ > 8u
                           && GATE_NGRID * GATE_ZSEG > 64u);
    _snprintf(line, sizeof line,
              "a frame crosses both split thresholds: %u cell draws, %u KB of stream, %u sets "
              "(thresholds %u KB, %u sets)",
              GATE_NGRID * GATE_DRAWN_CELLS,
              (unsigned)(GATE_NGRID * GATE_DRAWN_CELLS * GATE_DRAW_BYTES / 1024u),
              GATE_NGRID * GATE_DRAWN_CELLS,
              (unsigned)(GATE_SPLIT_BYTES / 1024u), (unsigned)GATE_SPLIT_SETS);
    ok &= gate_claim(line, GATE_NGRID * GATE_DRAWN_CELLS * GATE_DRAW_BYTES >= GATE_SPLIT_BYTES
                           && GATE_NGRID * GATE_DRAWN_CELLS >= GATE_SPLIT_SETS);
    _snprintf(line, sizeof line, "the run: %u frames, %u cell draws, %u colours",
              frames, frames * GATE_NGRID * GATE_DRAWN_CELLS,
              frames * (GATE_NGRID * GATE_DRAWN_CELLS + (unsigned)GA_N + 1u));
    /* Thousands of draws and thousands of descriptor sets over the run, as the gate's own
     * contract asks: the frame's 16112 draws a frame are well over that on their own. */
    ok &= gate_claim(line, frames * GATE_NGRID * GATE_DRAWN_CELLS >= 4000u);

    _snprintf(line, sizeof line, "the ring has %u slots and the run drives %u frames", slots, frames);
    ok &= gate_claim(line, slots >= 2 && frames > slots);
    ok &= gate_claim("the frame count fits the gate's per-frame tables", frames <= GATE_MAXFRAMES);
    return ok;
}

/* ── The two passes, and what is compared between them ─────────────────────
 *
 * The run is executed twice with the same frames: pass 1 against the Vulkan back end, where the
 * ring's counters and the overlap claims come from, and pass 2 against the CPU executor. Both
 * passes check every surface against the plan (gate_check_frame), and that is where the plan's
 * arithmetic is proved rather than merely agreed with: on the executor the flat fills, the
 * per-cell colours, the clears and the round trips come out of a completely different rasteriser
 * and must still be the plan's values. On top of that the two passes are compared with each
 * other, so that a frame whose plan check both passes satisfy is not the only evidence:
 *
 *   - the surfaces no sampler and no depth path touches are hashed in pass 1 and must come back
 *     byte for byte in pass 2. A hash is enough because the plan check already names every pixel
 *     of every surface, so a mismatch only has to say *that* the passes differ, and keeping
 *     120 MB of pixels would say nothing more.
 *   - the rest are compared pixel by pixel: the sampled cells of GATE_TTGT (one target in an
 *     otherwise hashed grid), GATE_DST, whose whole point is that it is sampled, and GATE_R16, a
 *     5:6:5 target -- the file's own tolerances cover a sampler and the executor's 5:6:5
 *     conversion (which run_roundtrip16 pins for the back end: RED -> 0xF800). Their pixels for
 *     every frame of pass 1 are kept, which is a few hundred KB a frame. GATE_PROBE is not among
 *     them: the executor's depth path on these batches is informational (see gate_check_frame),
 *     so its pixels differ from the back end's by design rather than by a step. */
static void gate_hash_surface(unsigned k, uint32_t *va, uint32_t *w, uint32_t *h, const char **what)
{
    static char names[GATE_NHASH][48];

    if (k == 0u) {
        *va = GATE_HEAVY; *w = GATE_HEAVY_W; *h = GATE_HEAVY_W;
        _snprintf(names[k], sizeof names[k], "heavy %ux%u", GATE_HEAVY_W, GATE_HEAVY_W);
    } else if (k == 1u) {
        *va = GATE_SRC; *w = *h = 64u;
        _snprintf(names[k], sizeof names[k], "SRC 64x64");
    } else if (k == 2u) {
        *va = GATE_R32; *w = *h = 64u;
        _snprintf(names[k], sizeof names[k], "R32 64x64");
    } else {
        uint32_t t = k - 3u;
        *va = GATE_GRID(t); *w = *h = GATE_GRID_W;
        _snprintf(names[k], sizeof names[k], "grid target %u %ux%u", t, GATE_GRID_W, GATE_GRID_W);
    }
    *what = names[k];
}

static uint64_t gate_hash_va(uint32_t va, uint32_t w, uint32_t h)
{
    const uint8_t *p = G(va);
    size_t n = (size_t)w * h * 4u, i;
    uint64_t hh = 1469598103934665603ull;

    for (i = 0; i < n; i++) {
        hh ^= p[i];
        hh *= 1099511628211ull;
    }
    return hh;
}

static uint64_t g_gate_hash[GATE_MAXFRAMES][GATE_NHASH];
static unsigned g_gate_hash_same;

static void gate_hash_frame(unsigned n)
{
    unsigned k;

    for (k = 0; k < GATE_NHASH; k++) {
        uint32_t va, w, h;
        const char *what;
        gate_hash_surface(k, &va, &w, &h, &what);
        g_gate_hash[n][k] = gate_hash_va(va, w, h);
    }
}

static int gate_hash_compare(unsigned n)
{
    unsigned k;
    int ok = 1;

    for (k = 0; k < GATE_NHASH; k++) {
        uint32_t va, w, h;
        const char *what;
        uint64_t hh;

        gate_hash_surface(k, &va, &w, &h, &what);
        hh = gate_hash_va(va, w, h);
        if (hh == g_gate_hash[n][k]) {
            g_gate_hash_same++;
            continue;
        }
        fprintf(stderr, "      frame %u: the %s surface is not byte for byte what the Vulkan back "
                        "end left (0x%016llX there, 0x%016llX from the CPU executor): one of the two "
                        "is not drawing what the frame's plan says, although both passed their own "
                        "check against it\n", n, what,
                (unsigned long long)g_gate_hash[n][k], (unsigned long long)hh);
        ok = 0;
    }
    return ok;
}

/* The surfaces the passes are compared on pixel by pixel, with what each one's tolerance is.
 * `tol` is in unpacked 8-bit channels, so a 5:6:5 surface is compared the way the rest of this
 * file compares one. */
typedef struct { const char *what; uint32_t va, w, h, bpp; int tol; } GateCmp;
static const GateCmp k_gate_cmp[GATE_NCMP] = {
    { "DST (sampled from the previous flip's SRC)", GATE_DST, 64u, 64u, 4u, GATE_TEX_TOL },
    { "R16 (a 5:6:5 target)", GATE_R16, 64u, 64u, 2u, TOL_565 },
    { "the sampled grid target", GATE_GRID(GATE_TTGT), GATE_GRID_W, GATE_GRID_W, 4u, GATE_TEX_TOL }
};
static uint8_t *g_gate_px[GATE_MAXFRAMES][GATE_NCMP];
static unsigned g_gate_px_same;

static int gate_px_store(unsigned n)
{
    unsigned k;
    int ok = 1;

    for (k = 0; k < GATE_NCMP; k++) {
        const GateCmp *c = &k_gate_cmp[k];
        size_t nbytes = (size_t)c->w * c->h * c->bpp;

        free(g_gate_px[n][k]);
        g_gate_px[n][k] = (uint8_t *)malloc(nbytes);
        if (!g_gate_px[n][k]) {
            fprintf(stderr, "FAIL: ring gate -- could not keep the Vulkan pass's %s pixels: the "
                            "comparison against the CPU executor would not be made\n", c->what);
            ok = 0;
            continue;
        }
        memcpy(g_gate_px[n][k], G(c->va), nbytes);
    }
    return ok;
}

static int gate_px_compare(unsigned n)
{
    unsigned k;
    int ok = 1;

    for (k = 0; k < GATE_NCMP; k++) {
        const GateCmp *c = &k_gate_cmp[k];
        const uint8_t *ref = g_gate_px[n][k], *got = G(c->va);
        unsigned x, y, bad = 0, shown = 0;
        size_t stride = (size_t)c->w * c->bpp;

        if (!ref)
            continue;
        for (y = 0; y < c->h; y++)
            for (x = 0; x < c->w; x++) {
                int a[4], b[4], d;

                unpack(ref + y * stride + (size_t)x * c->bpp, c->bpp, a);
                unpack(got + y * stride + (size_t)x * c->bpp, c->bpp, b);
                d = maxdiff(a, b, 1);
                if (d > c->tol) {
                    if (shown++ < 3)
                        fprintf(stderr, "      frame %u: %s: (%u,%u) is %d,%d,%d,%d, the Vulkan back "
                                        "end left %d,%d,%d,%d (differs by %d, allowed %d)\n", n, c->what,
                                x, y, b[0], b[1], b[2], b[3], a[0], a[1], a[2], a[3], d, c->tol);
                    bad++;
                }
            }
        if (bad) {
            fprintf(stderr, "      frame %u: %s differs from the Vulkan back end's pixels in %u of %u\n",
                    n, c->what, bad, c->w * c->h);
            ok = 0;
        } else {
            g_gate_px_same++;
        }
    }
    return ok;
}

/* Both passes start from the same guest memory: the gate's own 16 MB zeroed, with GATE_SRC
 * pre-filled with the colour frame 0's DST draw must sample (there has been no flip yet). */
static void gate_reset_region(void)
{
    uint32_t *px = (uint32_t *)G(GATE_SRC);
    unsigned i;

    memset(G(GATE_BASE), 0, 0x01000000u);
    for (i = 0; i < 64u * 64u; i++)
        px[i] = gate_aux(0, GA_PREV_SRC);
    /* The colour set is the run's provenance; pass 2 draws exactly the same colours again. */
    memset(g_gate_seen, 0, sizeof g_gate_seen);
    g_gate_seen_n = 0;
    g_gate.dup = 0;
}

/* ── The mode ───────────────────────────────────────────────────────────── */
static int run_frames_in_flight(void)
{
    Nv2aPbExecCounters cb, ca, pf;
    Nv2aVkRingStats r0, r;
    unsigned frames, done, n;
    uint64_t batches, need, stream_bytes;
    int ok = 1;

    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        puts("SKIP: gpu_backend_parity_vk --frames-in-flight -- the Vulkan back end could not be "
             "installed on a software device (no Vulkan, no matching device, or an elevated "
             "process: the loader ignores VK_DRIVER_FILES there)");
        return 77;
    }

    nv2a_vk_backend_ring_stats(&r0);
    frames = 12u + r0.slots;         /* more frames than the ring has slots, and at least twelve */
    printf("-- submission-ring lifetime gate (%u slot%s, %u frames, %u grid targets of %ux%u, "
           "%u cell draws a frame) --\n", r0.slots, r0.slots == 1u ? "" : "s", frames, GATE_NGRID,
           GATE_GRID_W, GATE_GRID_W, GATE_NGRID * GATE_DRAWN_CELLS);
    gate_reset_region();
    puts("-- gate fixtures --");
    ok &= gate_fixture(frames, r0.slots);

    /* Pass 1: the Vulkan back end. The counters are read after every frame, so the overlap, the
     * wrap and the drain are visible frame by frame, and the guest-memory checks run right after
     * the flip that published each frame. */
    puts("-- pass 1: the Vulkan back end (each frame: draw, flip, then every surface checked) --");
    memset(&cb, 0, sizeof cb);
    nv2a_pb_exec_counters(&cb);
    for (n = 0; n < frames; n++) {
        memset(&pf, 0, sizeof pf); memset(&ca, 0, sizeof ca);
        nv2a_pb_exec_counters(&pf);
        gate_draw_frame(n);
        nv2a_pb_exec_counters(&ca);
        nv2a_vk_backend_ring_stats(&r);
        printf("  frame %2u: %2u pushes, %6u draws %4u clears %u flips; ring: in_flight=%u "
               "max_in_flight=%u submissions=%llu reuse_waits=%llu drain_waits=%llu splits=%llu "
               "overflows=%llu frames=%llu\n",
               n, g_gate.pushes, (uint32_t)(ca.draws - pf.draws), (uint32_t)(ca.clears - pf.clears),
               (uint32_t)(ca.flip_stalls - pf.flip_stalls), r.in_flight, r.max_in_flight,
               (unsigned long long)r.submissions, (unsigned long long)r.reuse_waits,
               (unsigned long long)r.drain_waits, (unsigned long long)r.splits,
               (unsigned long long)r.overflows, (unsigned long long)r.frames);
        ok &= gate_check_frame(n, "vulkan", k_gate_tol_vk);
        gate_hash_frame(n);
        ok &= gate_px_store(n);
        if (g_gate.dup)
            ok = 0;                     /* a repeated colour: the pixels are no longer attributable */
        /* The packet's own claim, on every flip: publishing guest memory drains the ring. */
        if (r.in_flight != 0u) {
            fprintf(stderr, "FAIL: ring gate -- %u slot(s) are still in flight after frame %u's flip "
                            "published guest memory\n", r.in_flight, n);
            ok = 0;
        }
        if (g_gate.pb_fail) {
            ok = 0;                     /* the workload itself broke: the rest would be meaningless */
            break;
        }
    }
    done = n;
    nv2a_pb_exec_counters(&ca);
    batches = (uint64_t)(ca.draws - cb.draws);
    nv2a_vk_backend_ring_stats(&r);

    /* The evidence, from the back end's own counters, not from a log line. */
    puts("-- counters (nv2a_vk_backend_ring_stats, cumulative over pass 1) --");
    printf("  [ring] slots=%u in_flight=%u max_in_flight=%u submissions=%llu reuse_waits=%llu "
           "drain_waits=%llu splits=%llu overflows=%llu frames=%llu\n",
           r.slots, r.in_flight, r.max_in_flight, (unsigned long long)r.submissions,
           (unsigned long long)r.reuse_waits, (unsigned long long)r.drain_waits,
           (unsigned long long)r.splits, (unsigned long long)r.overflows,
           (unsigned long long)r.frames);
    printf("  [gate] %u of %u frames ran, %u colours used, %llu batches counted by the executor "
           "(a frame is %u cell draws plus the auxiliaries)\n",
           done, frames, g_gate_seen_n, (unsigned long long)batches, GATE_NGRID * GATE_DRAWN_CELLS);

    puts("-- the ring's claim, from its own counters --");
    ok &= gate_claim("max_in_flight >= 2: two submissions were issued without waiting for one another",
                     r.max_in_flight >= 2u);
    ok &= gate_claim("submissions > frames: the ring really wrapped", r.submissions > r.frames);
    ok &= gate_claim("splits >= 1: a frame was submitted in more than one part", r.splits >= 1u);
    ok &= gate_claim("reuse_waits >= 1: a fence was waited on before reusing an occupied slot",
                     r.reuse_waits >= 1u);
    ok &= gate_claim("drain_waits >= 1: a fence was waited on while publishing guest memory",
                     r.drain_waits >= 1u);
    ok &= gate_claim("frames == the flips this run asked for", r.frames == (uint64_t)frames);

    /* The reach, from the same counters. Every draw reserves one descriptor set against its slot
     * and writes at least GATE_DRAW_BYTES of vertices into that slot's stream, and a slot holds
     * GATE_SLOT_SETS sets and GATE_SLOT_STREAM of stream before vb_reserve has to submit it and
     * move on. So `batches` draws cannot leave the ring below either bound, and a workload whose
     * draws did not reach the back end -- the failure this gate was repaired for -- fails here
     * with both numbers printed, as well as in every per-cell colour above. */
    need = batches / GATE_SLOT_SETS;
    if (need > (uint64_t)r.slots) need -= r.slots; else need = 0;
    printf("  [bound] %llu batches / %u sets a slot = at least %llu submissions to carry them\n",
           (unsigned long long)batches, (unsigned)GATE_SLOT_SETS, (unsigned long long)need);
    ok &= gate_claim("the draws reached the back end: submissions >= batches/VB_SLOT_SETS - slots",
                     r.submissions >= need);
    stream_bytes = batches * GATE_DRAW_BYTES;
    need = stream_bytes / GATE_SLOT_STREAM;
    if (need > (uint64_t)r.slots) need -= r.slots; else need = 0;
    printf("  [bound] %llu batches x %u bytes = %llu MB of stream / %u MB a slot = %llu submissions, "
           "observed %llu (this bound is weak by construction -- the run's stream is only %llu slots' "
           "worth -- so it is reported, not asserted: the set bound above is the reach claim)\n",
           (unsigned long long)batches, (unsigned)GATE_DRAW_BYTES,
           (unsigned long long)(stream_bytes >> 20), (unsigned)(GATE_SLOT_STREAM >> 20),
           (unsigned long long)need, (unsigned long long)r.submissions,
           (unsigned long long)(stream_bytes / GATE_SLOT_STREAM));
    fflush(stdout);

    /* The back end's own [GPUVK] line is rate-limited to one every 10 s, so wait that window out
     * and flip once more: the line is cumulative, so it then carries this whole run -- one frame
     * more than the claims above, which is this flush flip itself. */
    if (r.submissions) {
        Sleep(10300);
        pb_begin(); pb_flip();
        gate_submit("ring gate ([GPUVK] counter flush)");
    }

    /* Pass 2: the same frames against the CPU executor, the pixels the rest of this file trusts.
     * The plan is checked here too -- which is what proves its arithmetic instead of merely
     * agreeing with the back end -- and then the surfaces a sampler does not touch are compared
     * with pass 1's byte for byte. */
    puts("-- pass 2: the same frames against the CPU executor (plan proved, then compared) --");
    nv2a_backend_register(NULL);
    gate_reset_region();
    for (n = 0; n < done; n++) {
        gate_draw_frame(n);
        ok &= gate_check_frame(n, "software", k_gate_tol_cpu);
        ok &= gate_hash_compare(n);
        ok &= gate_px_compare(n);
        if (g_gate.pb_fail) { ok = 0; break; }
    }
    printf("  [passes] %u of %u surfaces the sampler and the depth path do not touch are byte for "
           "byte what the Vulkan back end left; %u of %u compared pixel by pixel are within their "
           "step, over %u frames\n", g_gate_hash_same, done * (unsigned)GATE_NHASH,
           g_gate_px_same, done * (unsigned)GATE_NCMP, done);
    for (n = 0; n < done; n++) {
        unsigned k;

        for (k = 0; k < (unsigned)GATE_NCMP; k++) {
            free(g_gate_px[n][k]);
            g_gate_px[n][k] = NULL;
        }
    }

    fflush(stdout);
    if (!ok) {
        fprintf(stderr, "FAIL: gpu_backend_parity_vk --frames-in-flight\n");
        return 1;
    }
    puts("PASS: gpu_backend_parity_vk --frames-in-flight (the ring overlapped submissions, the work "
         "reached it, and every frame's guest bytes held what that frame drew in both passes)");
    return 0;
}

/* ── The batched-readback pack gate (--readback-pack) ─────────────────────
 *
 * The claim (plan item N4.2): "one batched flip readback, without the per-pixel CPU
 * conversion on the critical path". The back end's publication (nv2a_vk_backend.c,
 * vb_targets_flush) now truncates a 16-bit target to 5:6:5 in a compute kernel
 * (shaders/nv2a_vk_pack.hlsl) and the host only memcpy's rows into guest memory; a 4-byte
 * target keeps an image-to-buffer copy. The prior batching -- every dirty target of a flip
 * in one submission -- is older work: this gate checks it as a precondition (`batches`), not
 * as the claim.
 *
 * Why this needs a gate of its own: the conversion moved from the host to the GPU, and no
 * other check in this file can tell the two apart. The existing 16-bit checks
 * (run_roundtrip16, the ring gate's R16) use colours such as RED, which a rounding packer
 * reproduces exactly. A packer that rounds (HLSL's float-to-uint conversion rounds) is the
 * most likely slip here and is invisible to every colour those checks use.
 *
 * The gate drives the real seam, exactly as the --frames-in-flight case does:
 * nv2a_vk_backend_install() and guest pushbuffers through nv2a_submit_pending(). Nothing
 * here re-implements the back end's lifecycle, writes into its state, or reads anything but
 * nv2a_vk_backend_readback_stats(); those counters are cumulative and never rate-limited,
 * so they decide acceptance (the [GPUVK] log line is rate-limited and is not consulted).
 *
 * Two frames, and one line per check:
 *
 *   1. 16-bit targets with real bytes. T16A is 64x64 at pitch 128 (its own width); T16P is
 *      64x64 of pixels at pitch 132, so every row carries four bytes of padding the back end
 *      must not write. Every byte of every target's prefilled RB_REGION is compared with the
 *      gate's own plan: the drawn rectangle holds the colour's truncated 5:6:5 word and
 *      everything else the 0x5A sentinel the gate wrote -- which covers T16P's padding bytes,
 *      the pixels beside a dirty rectangle in its own row, and the rows below a target. The
 *      CPU executor re-checks the same plan with its own rasteriser (put_pixel truncates by
 *      the same shifts), and the two passes' bytes are compared with each other.
 *   2. The truncation boundary colours: five flat blocks inside a full-surface fill of a
 *      sixth, with the expected words stated as constants (k_rb_bound) and re-derived in the
 *      fixture from (r,g,b) by two independent spellings. Four of the six differ between
 *      truncation and rounding, so a rounding packer names the colour it broke; the other two
 *      are shared-value controls, and the fixture says so instead of counting them as
 *      boundary evidence.
 *   3. An odd dirty rectangle. T16B's clip is 21x13 at (3,5) -- odd x0, odd width and odd
 *      height. The plan compares every byte of the target and of its prefilled region, so a
 *      packer that publishes one halfword too many or too few (what an odd width tempts) is a
 *      FAIL that names the byte.
 *   4. Several targets and both formats in one flip: six dirty targets -- 64x64, 21x13, 32x48
 *      and 32x32, two pixel sizes and two pitches -- each checked against its own plan. A
 *      staging slice or offset taken from another target's publication lands here.
 *   5. The mid-frame flush. U samples a 16x16 window of TFL, which is drawn and still dirty
 *      in the same frame, so the back end must publish TFL before it reads that texture. The
 *      gate reads TFL's guest bytes the moment that push is submitted -- no flip has happened
 *      -- and requires every other target to be untouched, which says the flush published
 *      exactly TFL.
 *   6. The bytes at the flip, read directly after pb_flip() returns, and a second frame that
 *      patches three already-published targets: each must be re-loaded from its own published
 *      bytes and only the patch published again, byte for byte.
 *   7. The positive controls, from nv2a_vk_backend_readback_stats(): packed >= 1 and packed
 *      == the 16-bit targets the gate dirtied, packed_pixels == the pixels its own rectangles
 *      cover, copied >= 1 and copied == its 4-byte targets, readbacks == the targets it
 *      dirtied, bytes nonzero and == its own byte count, batches >= 1 and == the publications
 *      it forced. Each is its own line with its own FAIL.
 *
 * Coverage limits, stated rather than faked: the >VB_READBACK_MAX multi-batch split is
 * unreachable in a fixture -- the largest single slice a target can have is a 4096x4096
 * 4-byte target, 64 MiB, under the 96 MiB cap, and a fixture would need more than 96 MiB of
 * dirty targets at once -- so the gate never exercises it. wait_ns/publish_ns are printed,
 * not asserted: they are timing, and the claim is about bytes. */

#define RB_BASE        0x01000000u   /* the gate's own 16 MB region, as the ring gate's */
#define RB_REGION      0x8000u       /* bytes prefilled per target and compared in full */
#define RB_SENTINEL    0x5Au
#define RB_BOXES       16u
#define RB_FRAMES      2u            /* flips this gate drives, each published by a flip */
#define RB_MIN_SPAN    8             /* every quad spans at least this in both axes, or the CPU
                                      * executor reads the batch as object-space geometry and
                                      * skips it (nv2a_pb_exec.c, batch_is_screen_space) */

enum { RB_T16A, RB_T16B, RB_T16P, RB_T32A, RB_T32B, RB_TFL, RB_U, RB_NSURF };

typedef struct {
    const char *name;
    uint32_t va;
    int bpp, pitch;             /* SET_SURFACE_FORMAT_COLOR's pixel size, SET_SURFACE_PITCH */
    int tw, th;                 /* what the back end makes of those: pitch/bpp columns and
                                 * clip_y+clip_h rows (nv2a_vk_backend.c, vb_target_for) */
    int cx, cy, cw, ch;         /* the clip rectangle -- and, for a draw or a clear, the dirty
                                 * rectangle the back end marks (vb_scissor) */
} RbSurf;

static const RbSurf k_rb_surf[RB_NSURF] = {
    /* name   address                   bpp pitch  tw    th      clip  x   y   w   h */
    { "T16A", RB_BASE + 0x00000000u,     2, 128,  64,   64,          0,  0, 64, 64 },
    { "T16B", RB_BASE + 0x00010000u,     2, 128,  64,   5 + 13,      3,  5, 21, 13 },
    { "T16P", RB_BASE + 0x00020000u,     2, 132,  66,   64,          0,  0, 64, 64 },
    { "T32A", RB_BASE + 0x00030000u,     4, 256,  64,   64,          0,  0, 64, 64 },
    { "T32B", RB_BASE + 0x00040000u,     4, 128,  32,   48,          0,  0, 32, 48 },
    { "TFL",  RB_BASE + 0x00050000u,     4, 128,  32,   3 + 25,      2,  3, 28, 25 },
    { "U",    RB_BASE + 0x00060000u,     4, 128,  32,   32,          0,  0, 32, 32 },
};

/* The region a target's bytes are checked over must hold the largest target there is
 * (T32A: 64 rows of 256 bytes) and must not run into the next target's address. */
typedef char rb_region_holds_the_largest_target[((int)RB_REGION >= 64 * 256) ? 1 : -1];
typedef char rb_region_stays_inside_its_target[((int)RB_REGION < 0x10000) ? 1 : -1];
/* T16P's pitch leaves exactly 4 bytes of padding per row, and T16B's rectangle is odd on
 * every axis: those are the two cases the plan's arithmetic exists for. */
typedef char rb_padding_is_real[(132 > 64 * 2) ? 1 : -1];
typedef char rb_rect_is_odd[((21 % 2) && (13 % 2) && (3 % 2)) ? 1 : -1];
typedef char rb_region_covers_every_rect[((int)RB_REGION > 64 * 256) ? 1 : -1];

/* The 5:6:5 constants, derived here and not taken from the back end. c is 0xAARRGGBB with R
 * at bits 16-23, G at 8-15, B at 0-7; a 16-bit target of this back end stores
 *
 *   trunc(c) = ((c>>8) & 0xF800) | ((c>>5) & 0x07E0) | ((c>>3) & 0x001F)
 *
 * which is exactly what nv2a_pb_exec.c's put_pixel stores, and what this file's trunc565
 * spells. A packer that ROUNDS instead normalises each channel to the top of its range:
 *
 *   round(c) = (round(R/255*31) << 11) | (round(G/255*63) << 5) | round(B/255*31)
 *
 * with the arithmetic worked out per colour:
 *
 *   (0F,07,1F) trunc 1<<11 | 1<<5 | 3   = 0x0823   round 2<<11 |  2<<5 |  4 = 0x1044
 *   (1F,3F,1F) trunc 3<<11 |15<<5 | 3   = 0x19E3   round 4<<11 | 16<<5 |  4 = 0x2204
 *   (F8,FC,F8) trunc 31<<11|63<<5 |31   = 0xFFFF   round 30<<11| 62<<5 |30 = 0xF7DE
 *   (07,03,07) trunc 0     | 0    | 0   = 0x0000   round 1<<11 |  1<<5 |  1 = 0x0821
 *   (08,04,08) trunc 1<<11 | 1<<5 | 1   = 0x0821   round 0x800 | 0x20  |  1 = 0x0821
 *   (FF,FF,FF) trunc                     = 0xFFFF   round                     = 0xFFFF
 *
 * The first four DISCARD bits, so a rounder is a FAIL that names them; the last two are
 * shared-value controls -- they pin the exact word a correct packer produces but can never
 * discriminate against rounding, which is why the fixture counts the discriminating ones
 * separately. `trunc565` and a second, independent spelling recompute both columns at run
 * time, so a wrong constant in this table is a FAIL rather than a passing gate. */
typedef struct { uint32_t r, g, b, trunc_word, round_word; } RbBound;

static const RbBound k_rb_bound[6] = {
    { 0x0F, 0x07, 0x1F, 0x0823, 0x1044 },
    { 0x1F, 0x3F, 0x1F, 0x19E3, 0x2204 },
    { 0xF8, 0xFC, 0xF8, 0xFFFF, 0xF7DE },
    { 0x07, 0x03, 0x07, 0x0000, 0x0821 },
    { 0x08, 0x04, 0x08, 0x0821, 0x0821 },   /* a control: truncation and rounding agree */
    { 0xFF, 0xFF, 0xFF, 0xFFFF, 0xFFFF },   /* a control: truncation and rounding agree */
};

/* Every colour the gate draws with, all distinct (gate_seen() re-checks that as they are
 * used). The boundary blocks are the k_rb_bound colours; the rest are one target each, so a
 * stray publication is a wrong pixel rather than an unobserved one. */
#define RB_C16_ODD    0xFF112233u   /* T16B, the odd rectangle */
#define RB_C16_PAD    0xFF445566u   /* T16P, the padded rows */
#define RB_C16_PATCH  0xFFCC3311u   /* frame 1's patch into T16A */
#define RB_C16_PATCH2 0xFF2299DDu   /* frame 1's patch into T16P */
#define RB_C32_A      0xFF778899u   /* T32A */
#define RB_C32_A2     0xFFAABBCCu   /* ... and its inset box */
#define RB_C32_B      0xFFDDEE11u   /* T32B, the second size */
#define RB_C32_B2     0xFF123456u   /* ... and its inset box */
#define RB_C32_PATCH  0xFF998877u   /* frame 1's patch into T32A */
#define RB_C32_FL     0xFF3F7FBFu   /* TFL, and so the value the sampled window shows in U */
#define RB_C32_WHITE  0xFFFFFFFFu   /* the sampler draws' diffuse (a multiplier, not a colour) */

/* The window U samples: 16x16 A8R8G8B8 at (8 rows, column 2) of TFL, i.e. bytes
 * [8*128+8, 8*128+8+16*64), wholly inside TFL's drawn rectangle. */
#define RB_FL_WIN_ROW 8
#define RB_FL_WIN_COL 2
#define RB_FL_WIN_OFF (RB_FL_WIN_ROW * 128 + RB_FL_WIN_COL * 4)

static int rb_argb(unsigned i)
{
    const RbBound *b = &k_rb_bound[i];
    return (int)(0xFF000000u | (b->r << 16) | (b->g << 8) | b->b);
}

/* How many of the table's targets are 5:6:5, for the banner and the fixture. */
static unsigned rb_n16(void)
{
    unsigned i, n = 0;

    for (i = 0; i < RB_NSURF; i++)
        if (k_rb_surf[i].bpp == 2)
            n++;
    return n;
}

/* ── The plan, and comparing guest memory with it ─────────────────────────
 *
 * The plan is the gate's own statement of what a target must hold when the publication is
 * done: the sentinel everywhere in [va, va + RB_REGION), and each box's colour inside its
 * rectangle, in draw order. `src` (optional) keeps the ARGB each expected pixel came from,
 * so a FAIL can name the colour a wrong byte belongs to. */
typedef struct { int x0, y0, x1, y1; uint32_t argb; } RbBox;
typedef struct { unsigned n; RbBox box[RB_BOXES]; } RbPlan;

static int g_rb_bad;                 /* the fixture itself broke (set by the guards below) */

static void rb_fail(const char *what)
{
    if (!g_rb_bad)
        fprintf(stderr, "FAIL: readback gate -- the gate's own fixture is wrong (%s): the "
                        "checks below would not mean what they say\n", what);
    g_rb_bad = 1;
}

static void rb_box(RbPlan *p, int x0, int y0, int x1, int y1, uint32_t argb)
{
    if (p->n >= RB_BOXES) {
        rb_fail("a plan overflowed its box list");
        return;
    }
    p->box[p->n].x0 = x0; p->box[p->n].y0 = y0;
    p->box[p->n].x1 = x1; p->box[p->n].y1 = y1;
    p->box[p->n].argb = argb;
    p->n++;
}

/* A clip rectangle of a draw group. Frame 0 programs the target's own clip from the table;
 * frame 1's patch draws program a narrower one, which is what makes the flip's dirty
 * rectangle the patch: for a draw or a clear the back end marks the CLIP rectangle
 * (nv2a_vk_backend.c, vb_scissor), not the quad's bounds. */
typedef struct { int x, y, w, h; } RbClip;

/* One flat quad of a target's clip rectangle or of a box inside it: one draw, and a span of
 * at least RB_MIN_SPAN on both axes (see the constant's note). The quad must be inside the
 * clip, or the back end would clip it and the plan -- which paints the whole box -- would
 * disagree with a correct back end. */
static void rb_surface(unsigned idx, const RbClip *over);   /* defined below */

static void rb_quad_clip(unsigned idx, const RbClip *over, int x0, int y0, int x1, int y1,
                         uint32_t argb)
{
    const RbSurf *s = &k_rb_surf[idx];
    int cx = over ? over->x : s->cx, cy = over ? over->y : s->cy;
    int cw = over ? over->w : s->cw, ch = over ? over->h : s->ch;

    if (x0 < cx || y0 < cy || x1 > cx + cw || y1 > cy + ch || x1 > s->tw || y1 > s->th
        || x0 < 0 || y0 < 0 || x1 - x0 < RB_MIN_SPAN || y1 - y0 < RB_MIN_SPAN) {
        rb_fail("a quad left its clip rectangle or is under RB_MIN_SPAN on an axis");
        return;
    }
    if (!gate_seen(argb)) {
        fprintf(stderr, "FAIL: readback gate -- colour 0x%08X is drawn twice: a pixel can no "
                        "longer be attributed to the draw that produced it\n", argb);
        g_rb_bad = 1;
        return;
    }
    gate_need_room();                /* may split the push, which restarts the registers */
    rb_surface(idx, over);
    gate_quad((float)x0, (float)y0, (float)x1, (float)y1, 0.5f, argb);
}

static void rb_quad(unsigned idx, int x0, int y0, int x1, int y1, uint32_t argb)
{
    rb_quad_clip(idx, NULL, x0, y0, x1, y1, argb);
}

/* What the NV2A registers this gate believes are set, so a draw only re-emits what changed.
 * gate_baseline() -- and so every push split -- invalidates the belief, which is why this
 * also consults g_gate_st.surf (set by every surface emitter). */
static struct { uint32_t va; int cx, cy, cw, ch, bpp, pitch; } g_rb_reg;

static void rb_surface(unsigned idx, const RbClip *over)
{
    const RbSurf *s = &k_rb_surf[idx];
    int cx = over ? over->x : s->cx, cy = over ? over->y : s->cy;
    int cw = over ? over->w : s->cw, ch = over ? over->h : s->ch;

    if (g_gate_st.surf == s->va && g_rb_reg.va == s->va && g_rb_reg.cx == cx
        && g_rb_reg.cy == cy && g_rb_reg.cw == cw && g_rb_reg.ch == ch
        && g_rb_reg.bpp == s->bpp && g_rb_reg.pitch == s->pitch)
        return;
    pb(0x0200, ((uint32_t)cw << 16) | (uint32_t)cx);         /* CLIP_H: x0 | width */
    pb(0x0204, ((uint32_t)ch << 16) | (uint32_t)cy);         /* CLIP_V: y0 | height */
    pb(0x0208, s->bpp == 2 ? 0x0103u : 0x0108u);             /* R5G6B5 / A8R8G8B8 */
    pb(0x020C, (uint32_t)s->pitch);                          /* PITCH: not width*bpp for T16P */
    pb(0x0210, s->va);                                       /* COLOR_OFFSET */
    g_gate_st.surf = s->va;
    g_rb_reg.va = s->va; g_rb_reg.cx = cx; g_rb_reg.cy = cy;
    g_rb_reg.cw = cw; g_rb_reg.ch = ch;
    g_rb_reg.bpp = s->bpp; g_rb_reg.pitch = s->pitch;
}

static void rb_build(uint8_t *dst, uint32_t *src, unsigned idx, const RbPlan *p)
{
    const RbSurf *s = &k_rb_surf[idx];
    unsigned b, x, y;

    memset(dst, RB_SENTINEL, RB_REGION);
    if (src)
        for (y = 0; y < (unsigned)s->th; y++)
            for (x = 0; x < (unsigned)s->tw; x++)
                src[y * (unsigned)s->tw + x] = 0x5A5A5A5Au;
    for (b = 0; b < p->n; b++) {
        const RbBox *k = &p->box[b];

        for (y = (unsigned)k->y0; y < (unsigned)k->y1; y++)
            for (x = (unsigned)k->x0; x < (unsigned)k->x1; x++) {
                size_t off = (size_t)y * s->pitch + (size_t)x * s->bpp;

                if (s->bpp == 4) {
                    uint32_t v = k->argb;
                    memcpy(dst + off, &v, 4);
                } else {
                    uint16_t v = trunc565(k->argb);
                    memcpy(dst + off, &v, 2);
                }
                if (src)
                    src[y * (unsigned)s->tw + x] = k->argb;
            }
    }
}

/* Compare one target's guest bytes with `want` (RB_REGION bytes): every pixel of the target
 * in its own format and at its own pitch with `tol` per unpacked channel (0 means byte for
 * byte, so a 5:6:5 word must match exactly), and every other byte of the region exactly.
 * `ref` names what `want` is -- the gate's plan, or the other pass's bytes -- so a FAIL says
 * which side it is disagreeing with. */
static int rb_compare(const char *what, unsigned idx, const uint8_t *want,
                      const uint32_t *want_src, int tol, const char *ref)
{
    const RbSurf *s = &k_rb_surf[idx];
    const uint8_t *got = G(s->va);
    unsigned x, y, bad = 0, shown = 0;
    size_t i, tail;

    for (y = 0; y < (unsigned)s->th; y++)
        for (x = 0; x < (unsigned)s->tw; x++) {
            size_t off = (size_t)y * s->pitch + (size_t)x * s->bpp;
            int wrong;

            if (tol == 0)
                wrong = s->bpp == 4 ? *(const uint32_t *)(got + off) != *(const uint32_t *)(want + off)
                                    : *(const uint16_t *)(got + off) != *(const uint16_t *)(want + off);
            else {
                int a[4], b[4];
                unpack(got + off, (uint32_t)s->bpp, a);
                unpack(want + off, (uint32_t)s->bpp, b);
                wrong = maxdiff(a, b, 1) > tol;
            }
            if (!wrong)
                continue;
            if (shown++ < 3) {
                uint32_t gv = s->bpp == 4 ? *(const uint32_t *)(got + off)
                                          : *(const uint16_t *)(got + off);
                uint32_t wv = s->bpp == 4 ? *(const uint32_t *)(want + off)
                                          : *(const uint16_t *)(want + off);
                fprintf(stderr, "      %s: (%u,%u) is 0x%0*X, %s says 0x%0*X",
                        s->name, x, y, s->bpp == 4 ? 8 : 4, gv, ref, s->bpp == 4 ? 8 : 4, wv);
                if (want_src) {
                    uint32_t from = want_src[y * (unsigned)s->tw + x];
                    if (from == 0x5A5A5A5Au)
                        fputs(" (the sentinel)\n", stderr);
                    else
                        fprintf(stderr, " (from 0x%08X)\n", from);
                } else
                    fputc('\n', stderr);
            }
            bad++;
        }
    tail = (size_t)s->pitch * (size_t)s->th;
    for (i = tail; i < RB_REGION; i++)
        if (got[i] != want[i]) {
            if (shown++ < 3)
                fprintf(stderr, "      %s: byte %llu is 0x%02X, %s says 0x%02X\n", s->name,
                        (unsigned long long)i, got[i], ref, want[i]);
            bad++;
        }
    printf("      %-56s %s (%u bytes wrong)\n", what, bad ? "FAIL" : "ok", bad);
    if (bad)
        fprintf(stderr, "FAIL: readback gate -- %s: %u bytes of %s's %u do not match %s\n",
                what, bad, s->name, (unsigned)RB_REGION, ref);
    return bad == 0;
}

/* The plan check: build the expected bytes, compare, and (on a FAIL) name the colour each
 * wrong pixel came from. */
static int rb_check(const char *what, unsigned idx, const RbPlan *p, int tol)
{
    const RbSurf *s = &k_rb_surf[idx];
    uint8_t *want = (uint8_t *)malloc(RB_REGION);
    uint32_t *src = (uint32_t *)malloc((size_t)s->tw * s->th * 4u);
    int ok;

    if (!want) {
        fprintf(stderr, "FAIL: readback gate -- could not build the plan for %s\n", s->name);
        return 0;
    }
    rb_build(want, src, idx, p);
    ok = rb_compare(what, idx, want, src, tol, "the plan");
    free(want);
    free(src);
    return ok;
}

/* Compare one target with bytes another pass left there (a snapshot). Any difference is a
 * disagreement between the two passes about the same pushbuffers, so there is no colour to
 * name: `want_src` is NULL. */
static int rb_check_against(const char *what, unsigned idx, const uint8_t *want, int tol)
{
    return rb_compare(what, idx, want, NULL, tol, "the Vulkan back end");
}

/* A target nothing has drawn into since the last publication must still hold its sentinel:
 * the empty plan is exactly that. */
static int rb_check_untouched(const char *what, unsigned idx)
{
    RbPlan p;

    memset(&p, 0, sizeof p);
    return rb_check(what, idx, &p, 0);
}

/* ── What the publication must have done ─────────────────────────────────
 *
 * Computed from the gate's own rectangles and the surfaces' own pitch and pixel size -- not
 * order, and compared with the deltas of nv2a_vk_backend_readback_stats(). */

/* A rectangle one publication covers: the dirty rectangle a stage's targets carry. */
typedef struct { unsigned idx; int x0, y0, x1, y1; } RbDirty;

/* The one publication the mid-frame flush makes: TFL alone, its whole dirty rectangle. */
static const RbDirty k_rb_stage0[] = { { RB_TFL, 2, 3, 30, 28 } };
/* The flip of frame 0: every target, in draw order. Each target is drawn under exactly one
 * clip rectangle, so its published rectangle IS that clip (checked by the fixture). */
static const RbDirty k_rb_stage1[] = {
    { RB_T16A, 0, 0, 64, 64 }, { RB_T16B, 3, 5, 24, 18 }, { RB_T16P, 0, 0, 64, 64 },
    { RB_T32A, 0, 0, 64, 64 }, { RB_T32B, 0, 0, 32, 48 }, { RB_U, 0, 0, 32, 32 }
};
/* The flip of frame 1 is the three patches, from the same table the draws come from:
 * rb_stage2() below. */

typedef struct {
    uint64_t readbacks, batches, packed, packed_pixels, copied, bytes;
} RbWant;

static RbWant rb_want(const RbDirty *d, unsigned n)
{
    RbWant w;
    unsigned i;

    memset(&w, 0, sizeof w);
    for (i = 0; i < n; i++) {
        const RbSurf *s = &k_rb_surf[d[i].idx];
        uint64_t rw = (uint64_t)(d[i].x1 - d[i].x0), rh = (uint64_t)(d[i].y1 - d[i].y0);

        w.readbacks++;
        if (s->bpp == 2) {
            w.packed++;
            w.packed_pixels += rw * rh;
        } else
            w.copied++;
        w.bytes += rw * (uint64_t)s->bpp * rh;
    }
    /* One bounded batch carries a whole stage this small: VB_READBACK_MAX is 96 MiB and the
     * largest stage here is 6 targets of 64 KiB at most. A publication that made one
     * submission per target would report more, which is what stage's batches claim catches. */
    w.batches = 1;
    return w;
}

static void rb_sub(const Nv2aVkReadbackStats *a, const Nv2aVkReadbackStats *b,
                   Nv2aVkReadbackStats *out)
{
    out->readbacks = a->readbacks - b->readbacks;
    out->batches = a->batches - b->batches;
    out->packed = a->packed - b->packed;
    out->packed_pixels = a->packed_pixels - b->packed_pixels;
    out->copied = a->copied - b->copied;
    out->bytes = a->bytes - b->bytes;
    out->wait_ns = a->wait_ns - b->wait_ns;
    out->publish_ns = a->publish_ns - b->publish_ns;
}

static void rb_print(const char *tag, const Nv2aVkReadbackStats *s)
{
    printf("  [%s] readbacks=%llu batches=%llu packed=%llu packed_pixels=%llu copied=%llu "
           "bytes=%llu wait_ms=%.1f publish_ms=%.1f\n", tag,
           (unsigned long long)s->readbacks, (unsigned long long)s->batches,
           (unsigned long long)s->packed, (unsigned long long)s->packed_pixels,
           (unsigned long long)s->copied, (unsigned long long)s->bytes,
           (double)s->wait_ns / 1e6, (double)s->publish_ns / 1e6);
}

static int rb_claim_eq(const char *what, uint64_t got, uint64_t want)
{
    int ok = got == want;

    printf("      %-56s got %-8llu want %-8llu %s\n", what, (unsigned long long)got,
           (unsigned long long)want, ok ? "ok" : "FAIL");
    if (!ok)
        fprintf(stderr, "FAIL: readback gate -- %s: nv2a_vk_backend_readback_stats reports %llu, "
                        "the gate's own count is %llu\n", what, (unsigned long long)got,
                (unsigned long long)want);
    return ok;
}

static int rb_claim_atleast(const char *what, uint64_t got, uint64_t want)
{
    int ok = got >= want;

    printf("      %-56s got %-8llu want >=%-8llu %s\n", what, (unsigned long long)got,
           (unsigned long long)want, ok ? "ok" : "FAIL");
    if (!ok)
        fprintf(stderr, "FAIL: readback gate -- %s: nv2a_vk_backend_readback_stats reports %llu, "
                        "the gate needs at least %llu\n", what, (unsigned long long)got,
                (unsigned long long)want);
    return ok;
}

/* The counters a stage must show, each with its own line and its own FAIL. */
static int rb_stage_counts(const char *what, const RbWant *w, const Nv2aVkReadbackStats *d)
{
    int ok = 1;
    char line[96];

    printf("    %s:\n", what);
    if (w->packed)
        ok &= rb_claim_atleast("packed >= 1: 16-bit targets went through the GPU pack kernel",
                               d->packed, 1);
    if (w->copied)
        ok &= rb_claim_atleast("copied >= 1: the 4-byte path ran", d->copied, 1);
    ok &= rb_claim_atleast("batches >= 1: the publication made a submission", d->batches, 1);
    ok &= rb_claim_eq("readbacks == the targets this stage dirtied", d->readbacks, w->readbacks);
    ok &= rb_claim_eq("packed == this stage's 5:6:5 targets", d->packed, w->packed);
    ok &= rb_claim_eq("packed_pixels == the pixels this stage's rectangles cover",
                      d->packed_pixels, w->packed_pixels);
    ok &= rb_claim_eq("copied == this stage's A8R8G8B8 targets", d->copied, w->copied);
    ok &= rb_claim_eq("bytes == this stage's own byte count", d->bytes, w->bytes);
    _snprintf(line, sizeof line, "batches == 1: %s was one batched publication", what);
    ok &= rb_claim_eq(line, d->batches, w->batches);
    return ok;
}

/* ── The frames ─────────────────────────────────────────────────────────── */
static void rb_flip(const char *what)
{
    pb_begin();
    gate_baseline();
    pb_flip();
    gate_submit(what);
}

/* Frame 0's draws: every target once, under its own clip rectangle. T16A is the boundary
 * colours -- a full-surface fill of the first, then the other five as 8x8 blocks inside it --
 * and the rest are one flat fill each, with an inset box on the 4-byte targets so a slice or
 * an offset taken from another target's publication is a wrong pixel in the middle of a
 * target as well as at its edges. */
static void rb_frame0_draws(void)
{
    pb_begin();
    gate_baseline();

    rb_quad(RB_T16A, 0, 0, 64, 64, (uint32_t)rb_argb(0));
    rb_quad(RB_T16A, 8, 0, 16, 8, (uint32_t)rb_argb(1));
    rb_quad(RB_T16A, 16, 0, 24, 8, (uint32_t)rb_argb(2));
    rb_quad(RB_T16A, 24, 0, 32, 8, (uint32_t)rb_argb(3));
    rb_quad(RB_T16A, 32, 0, 40, 8, (uint32_t)rb_argb(4));
    rb_quad(RB_T16A, 40, 0, 48, 8, (uint32_t)rb_argb(5));

    rb_quad(RB_T16B, 3, 5, 24, 18, RB_C16_ODD);
    rb_quad(RB_T16P, 0, 0, 64, 64, RB_C16_PAD);

    rb_quad(RB_T32A, 0, 0, 64, 64, RB_C32_A);
    rb_quad(RB_T32A, 16, 16, 32, 32, RB_C32_A2);
    rb_quad(RB_T32B, 0, 0, 32, 48, RB_C32_B);
    rb_quad(RB_T32B, 8, 8, 24, 32, RB_C32_B2);

    rb_quad(RB_TFL, 2, 3, 30, 28, RB_C32_FL);

    gate_submit("readback gate: frame 0's draws");
}

/* The draw that makes the back end publish TFL mid-frame: U samples a 16x16 window of TFL,
 * which is drawn and still dirty, so the texture bind forces the flush. It is a push of its
 * own, so the guest bytes can be read the moment it is submitted -- no flip in between. */
static void rb_frame0_sample(void)
{
    const RbSurf *fl = &k_rb_surf[RB_TFL];

    pb_begin();
    gate_baseline();
    gate_need_room();
    rb_surface(RB_U, NULL);
    gate_tex(fl->va + RB_FL_WIN_OFF, 0x12, 16, 16, 128, FILTER_NEAREST);
    gate_quad_uv(8.0f, 8.0f, 24.0f, 24.0f, 0.5f, RB_C32_WHITE, 0.0f, 0.0f, 16.0f, 16.0f);
    gate_tex_none();
    gate_submit("readback gate: the sampled window of TFL");
}

/* Frame 1: a patch into three targets that frame 0's flip published. Each patch programs a
 * clip of its own -- narrower than the target's -- so the flip publishes the patch rectangle
 * alone (the back end marks the clip, not the quad), and every byte outside it must still be
 * what frame 0 left: that is what makes the second frame a re-load check as well as a
 * publication check. */
typedef struct { unsigned idx; RbClip clip; int x0, y0, x1, y1; uint32_t argb; } RbPatch;

static const RbPatch k_rb_patch[3] = {
    { RB_T16A, { 40, 40,  8, 12 }, 40, 40, 48, 52, RB_C16_PATCH },
    { RB_T16P, {  4,  4,  8,  8 },  4,  4, 12, 12, RB_C16_PATCH2 },
    { RB_T32A, {  8,  8, 16, 16 },  8,  8, 24, 24, RB_C32_PATCH },
};
#define RB_NPATCH ((unsigned)(sizeof k_rb_patch / sizeof k_rb_patch[0]))

static void rb_frame1_draws(void)
{
    unsigned i;

    pb_begin();
    gate_baseline();
    for (i = 0; i < RB_NPATCH; i++)
        rb_quad_clip(k_rb_patch[i].idx, &k_rb_patch[i].clip, k_rb_patch[i].x0, k_rb_patch[i].y0,
                     k_rb_patch[i].x1, k_rb_patch[i].y1, k_rb_patch[i].argb);
    pb_flip();
    gate_submit("readback gate: frame 1's patches and flip");
}

/* The stage-2 dirty rectangles, from the same table the draws come from. */
static unsigned rb_stage2(RbDirty *d)
{
    unsigned i;

    for (i = 0; i < RB_NPATCH; i++) {
        d[i].idx = k_rb_patch[i].idx;
        d[i].x0 = k_rb_patch[i].clip.x;
        d[i].y0 = k_rb_patch[i].clip.y;
        d[i].x1 = k_rb_patch[i].clip.x + k_rb_patch[i].clip.w;
        d[i].y1 = k_rb_patch[i].clip.y + k_rb_patch[i].clip.h;
    }
    return RB_NPATCH;
}

static void rb_plan0(RbPlan *p)
{
    unsigned i;

    for (i = 0; i < RB_NSURF; i++)
        p[i].n = 0;
    rb_box(&p[RB_T16A], 0, 0, 64, 64, (uint32_t)rb_argb(0));
    rb_box(&p[RB_T16A], 8, 0, 16, 8, (uint32_t)rb_argb(1));
    rb_box(&p[RB_T16A], 16, 0, 24, 8, (uint32_t)rb_argb(2));
    rb_box(&p[RB_T16A], 24, 0, 32, 8, (uint32_t)rb_argb(3));
    rb_box(&p[RB_T16A], 32, 0, 40, 8, (uint32_t)rb_argb(4));
    rb_box(&p[RB_T16A], 40, 0, 48, 8, (uint32_t)rb_argb(5));
    rb_box(&p[RB_T16B], 3, 5, 24, 18, RB_C16_ODD);
    rb_box(&p[RB_T16P], 0, 0, 64, 64, RB_C16_PAD);
    rb_box(&p[RB_T32A], 0, 0, 64, 64, RB_C32_A);
    rb_box(&p[RB_T32A], 16, 16, 32, 32, RB_C32_A2);
    rb_box(&p[RB_T32B], 0, 0, 32, 48, RB_C32_B);
    rb_box(&p[RB_T32B], 8, 8, 24, 32, RB_C32_B2);
    rb_box(&p[RB_TFL], 2, 3, 30, 28, RB_C32_FL);
    /* U is completed by the sampling draw, so rb_plan0 leaves it empty and the checks below
     * add its one box where the sampler is in the plan. */
}

/* Every target drawn by frame 0 except TFL is still unpublished at the mid-frame flush, so
 * guest memory there must still be the sentinel -- U included: the sampling draw only records
 * into its own image, and the flush published TFL alone. */
static int rb_check_unpublished(const char *tag)
{
    static const unsigned k_other[] = { RB_T16A, RB_T16B, RB_T16P, RB_T32A, RB_T32B, RB_U };
    unsigned i;
    int ok = 1;

    for (i = 0; i < sizeof k_other / sizeof k_other[0]; i++) {
        char what[128];
        _snprintf(what, sizeof what, "%s: %s is untouched (the flush published TFL alone)",
                  tag, k_rb_surf[k_other[i]].name);
        ok &= rb_check_untouched(what, k_other[i]);
    }
    return ok;
}

/* ── The fixtures ───────────────────────────────────────────────────────── */
static int rb_fixture(unsigned frames)
{
    unsigned i, bad = 0, disc = 0;
    int ok = 1;
    char line[160];
    RbDirty p2[RB_NPATCH];
    unsigned n2;
    RbWant w0, w1, w2;

    /* The constants: recomputed here from (r,g,b) by two spellings -- this file's trunc565
     * and shifts written out again -- and by the rounding formula, and compared with the
     * table. A constant this disagrees with would make every check below agree with the back
     * end for the wrong reason. */
    for (i = 0; i < 6; i++) {
        const RbBound *b = &k_rb_bound[i];
        uint32_t c = 0xFF000000u | (b->r << 16) | (b->g << 8) | b->b;
        uint16_t t1 = trunc565(c);
        uint16_t t2 = (uint16_t)(((b->r >> 3) << 11) | ((b->g >> 2) << 5) | (b->b >> 3));
        uint16_t r1 = (uint16_t)(((uint32_t)((b->r * 31u + 127u) / 255u) << 11)
                               | ((uint32_t)((b->g * 63u + 127u) / 255u) << 5)
                               |  (uint32_t)((b->b * 31u + 127u) / 255u));

        if (t1 != b->trunc_word || t2 != b->trunc_word || r1 != b->round_word) {
            bad++;
            fprintf(stderr, "FAIL: readback gate -- the constant for (0x%02X,0x%02X,0x%02X) is "
                            "wrong: the table says trunc=0x%04X round=0x%04X, trunc565 gives "
                            "0x%04X, the shifts give 0x%04X, rounding gives 0x%04X\n", b->r, b->g,
                    b->b, b->trunc_word, b->round_word, t1, t2, r1);
        }
        if (b->trunc_word != b->round_word)
            disc++;
    }
    printf("      %-56s %s (%u of 6 constants wrong, %u discriminate against rounding)\n",
           "the 5:6:5 boundary constants, recomputed here", bad ? "FAIL" : "ok", bad, disc);
    if (disc < 4) {
        fprintf(stderr, "FAIL: readback gate -- only %u of the table's colours tell truncation "
                        "from rounding: the gate could not decide the packer's arithmetic\n", disc);
        ok = 0;
    }
    ok &= bad == 0;

    /* The geometry: the published rectangle of a stage must be what the frame drew into that
     * target -- the target's clip for frame 0, a box inside it for frame 1 -- or the plans
     * below would be checking a rectangle the back end was never asked to publish. */
    bad = 0;
    for (i = 0; i < sizeof k_rb_stage1 / sizeof k_rb_stage1[0]; i++) {
        const RbSurf *s = &k_rb_surf[k_rb_stage1[i].idx];
        if (k_rb_stage1[i].x0 != s->cx || k_rb_stage1[i].y0 != s->cy
            || k_rb_stage1[i].x1 != s->cx + s->cw || k_rb_stage1[i].y1 != s->cy + s->ch)
            bad++;
    }
    if (bad) {
        fprintf(stderr, "FAIL: readback gate -- %u of frame 0's targets were drawn under more "
                        "than one clip rectangle: the published rectangle is not the plan's\n", bad);
        ok = 0;
    }
    if (k_rb_stage0[0].x0 != k_rb_surf[RB_TFL].cx || k_rb_stage0[0].y0 != k_rb_surf[RB_TFL].cy
        || k_rb_stage0[0].x1 != k_rb_surf[RB_TFL].cx + k_rb_surf[RB_TFL].cw
        || k_rb_stage0[0].y1 != k_rb_surf[RB_TFL].cy + k_rb_surf[RB_TFL].ch) {
        fprintf(stderr, "FAIL: readback gate -- the mid-frame flush's plan is not TFL's whole "
                        "dirty rectangle\n");
        ok = 0;
    }
    /* Frame 1's patches: each programs the clip the plan names, inside its target's own clip
     * and strictly narrower than it (or the second flip would publish no rectangle the first
     * one did not, and frame 1's counters would decide nothing). */
    bad = 0;
    for (i = 0; i < RB_NPATCH; i++) {
        const RbPatch *p = &k_rb_patch[i];
        const RbSurf *s = &k_rb_surf[p->idx];

        if (p->clip.x != p->x0 || p->clip.y != p->y0 || p->clip.w != p->x1 - p->x0
            || p->clip.h != p->y1 - p->y0 || p->clip.x < s->cx || p->clip.y < s->cy
            || p->clip.x + p->clip.w > s->cx + s->cw
            || p->clip.y + p->clip.h > s->cy + s->ch
            || p->clip.w < RB_MIN_SPAN || p->clip.h < RB_MIN_SPAN
            || p->clip.w * p->clip.h >= s->cw * s->ch)
            bad++;
    }
    if (bad) {
        fprintf(stderr, "FAIL: readback gate -- %u of frame 1's patches is not a narrower "
                        "rectangle inside its target's clip\n", bad);
        ok = 0;
    }
    printf("      %-56s %s\n",
           "every published rectangle is the rectangle the frame drew",
           ok ? "ok" : "FAIL");

    /* What the publication must do, from the gate's own rectangles. */
    n2 = rb_stage2(p2);
    w0 = rb_want(k_rb_stage0, (unsigned)(sizeof k_rb_stage0 / sizeof k_rb_stage0[0]));
    w1 = rb_want(k_rb_stage1, (unsigned)(sizeof k_rb_stage1 / sizeof k_rb_stage1[0]));
    w2 = rb_want(p2, n2);
    _snprintf(line, sizeof line,
              "the run's own plan: %llu publications %llu of them 5:6:5, %llu packed pixels, "
              "%llu copied, %llu bytes, %llu batches, %u frames",
              (unsigned long long)(w0.readbacks + w1.readbacks + w2.readbacks),
              (unsigned long long)(w0.packed + w1.packed + w2.packed),
              (unsigned long long)(w0.packed_pixels + w1.packed_pixels + w2.packed_pixels),
              (unsigned long long)(w0.copied + w1.copied + w2.copied),
              (unsigned long long)(w0.bytes + w1.bytes + w2.bytes),
              (unsigned long long)(w0.batches + w1.batches + w2.batches), frames);
    printf("      %s\n", line);
    ok &= (w0.packed_pixels + w1.packed_pixels + w2.packed_pixels) > 0
       && (w0.bytes + w1.bytes + w2.bytes) > 0;
    /* More than one flip, or the publication would only ever be the first one's. */
    if (frames < 2u) {
        fprintf(stderr, "FAIL: readback gate -- the run drives %u flip(s): the second frame's "
                        "re-load and re-publication would not happen\n", frames);
        ok = 0;
    }
    if (!ok)
        fprintf(stderr, "FAIL: readback gate -- the run's plan is empty: the counters below "
                        "could not decide anything\n");
    return ok;
}

/* ── The mode ───────────────────────────────────────────────────────────── */
static void rb_reset(void)
{
    unsigned i;

    gate_reset_region();
    for (i = 0; i < RB_NSURF; i++)
        prefill_va(k_rb_surf[i].va, RB_REGION, RB_SENTINEL);
    g_rb_reg.va = 0u;
    g_rb_bad = 0;
}

static int run_readback_pack(void)
{
    Nv2aVkReadbackStats rb0, rb, prev, d;
    Nv2aVkRingStats r0, r1;
    RbPlan plan[RB_NSURF];
    RbDirty p2[RB_NPATCH];
    RbWant w0, w1, w2;
    uint8_t *snap0[RB_NSURF], *snap1[RB_NSURF];
    unsigned i;
    int ok = 1;

    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        puts("SKIP: gpu_backend_parity_vk --readback-pack -- the Vulkan back end could not be "
             "installed on a software device (no Vulkan, no matching device, or an elevated "
             "process: the loader ignores VK_DRIVER_FILES there)");
        return 77;
    }

    memset(snap0, 0, sizeof snap0);
    memset(snap1, 0, sizeof snap1);
    nv2a_vk_backend_readback_stats(&rb0);
    printf("-- batched readback pack gate (%u targets, %u of them 5:6:5, %u frames) --\n",
           (unsigned)RB_NSURF, rb_n16(), (unsigned)RB_FRAMES);
    puts("-- gate fixtures --");
    ok &= rb_fixture(RB_FRAMES);

    w0 = rb_want(k_rb_stage0, (unsigned)(sizeof k_rb_stage0 / sizeof k_rb_stage0[0]));
    w1 = rb_want(k_rb_stage1, (unsigned)(sizeof k_rb_stage1 / sizeof k_rb_stage1[0]));
    w2 = rb_want(p2, rb_stage2(p2));

    /* ── pass 1: the Vulkan back end ───────────────────────────────────── */
    puts("-- pass 1: the Vulkan back end --");
    rb_reset();
    nv2a_vk_backend_ring_stats(&r0);
    prev = rb0;

    rb_frame0_draws();
    rb_frame0_sample();

    /* 5. The mid-frame flush, read at once: TFL's bytes are in guest memory before any flip,
     *    and no other target has been published. */
    puts("-- the mid-frame flush (the sampled window's target) --");
    rb_plan0(plan);
    ok &= rb_check("TFL: published by the flush the texture bind forced", RB_TFL, &plan[RB_TFL], 0);
    ok &= rb_check_unpublished("vulkan");
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 0", &d);
    ok &= rb_stage_counts("the mid-frame flush", &w0, &d);
    memcpy(&prev, &rb, sizeof prev);

    /* 6. Frame 0's flip: every target checked directly after the flip returned. */
    rb_flip("readback gate: frame 0's flip");
    puts("-- frame 0's flip --");
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];
        RbPlan u = plan[RB_U];

        if (i == RB_U) {
            /* The one surface a sampler is in the path of: the file's own convention allows a
             * sampled UNORM one step per channel (k_gate_tol_vk), and U's window is a flat
             * colour, so nothing about the flush's evidence rests on the tolerance. */
            _snprintf(what, sizeof what, "U: the window sampled from TFL, as published");
            memset(&u, 0, sizeof u);
            rb_box(&u, 8, 8, 24, 24, RB_C32_FL);
            ok &= rb_check(what, RB_U, &u, GATE_TEX_TOL);
        } else {
            _snprintf(what, sizeof what, "%s: this frame's drawing, published by the flip",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        }
    }
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 1", &d);
    ok &= rb_stage_counts("frame 0's flip (6 dirty targets, both formats)", &w1, &d);
    memcpy(&prev, &rb, sizeof prev);
    for (i = 0; i < RB_NSURF; i++)
        snap0[i] = snapshot_va(k_rb_surf[i].va, RB_REGION);

    /* Frame 1: the patches, and the load of the targets that a flip made stale. */
    rb_frame1_draws();
    puts("-- frame 1's flip (three targets re-loaded and patched) --");
    rb_plan0(plan);
    for (i = 0; i < RB_NPATCH; i++)
        rb_box(&plan[k_rb_patch[i].idx], k_rb_patch[i].x0, k_rb_patch[i].y0, k_rb_patch[i].x1,
               k_rb_patch[i].y1, k_rb_patch[i].argb);
    /* U's box comes from the sampling draw of frame 0; no frame-1 draw touches U. */
    rb_box(&plan[RB_U], 8, 8, 24, 24, RB_C32_FL);
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];
        unsigned j, patched = 0;

        for (j = 0; j < RB_NPATCH; j++)
            if (k_rb_patch[j].idx == i)
                patched = 1;
        if (patched) {
            _snprintf(what, sizeof what, "%s: the patch over its own published bytes",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        } else {
            _snprintf(what, sizeof what, "%s: untouched by frame 1, so unchanged",
                      k_rb_surf[i].name);
            ok &= rb_check_against(what, i, snap0[i], 0);
        }
    }
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 2", &d);
    ok &= rb_stage_counts("frame 1's flip (3 patched targets)", &w2, &d);
    for (i = 0; i < RB_NSURF; i++)
        snap1[i] = snapshot_va(k_rb_surf[i].va, RB_REGION);

    /* 7. The positive controls, over the whole run. */
    puts("-- the publication's counters (cumulative, from nv2a_vk_backend_readback_stats) --");
    nv2a_vk_backend_ring_stats(&r1);
    ok &= rb_claim_eq("frames == the flips this run made", r1.frames - r0.frames, RB_FRAMES);
    rb_print("run", &rb);
    ok &= rb_claim_atleast("packed >= 1: the GPU packed a 16-bit target at least once",
                           rb.packed, 1);
    ok &= rb_claim_atleast("copied >= 1: the 4-byte copy path ran at least once", rb.copied, 1);
    ok &= rb_claim_atleast("batches >= 1: the publication made a submission", rb.batches, 1);
    ok &= rb_claim_eq("readbacks == every target this run dirtied, stage by stage",
                      rb.readbacks, w0.readbacks + w1.readbacks + w2.readbacks);
    ok &= rb_claim_eq("packed == this run's 5:6:5 targets",
                      rb.packed, w0.packed + w1.packed + w2.packed);
    ok &= rb_claim_eq("packed_pixels == the pixels this run's rectangles cover",
                      rb.packed_pixels, w0.packed_pixels + w1.packed_pixels + w2.packed_pixels);
    ok &= rb_claim_eq("copied == this run's A8R8G8B8 targets",
                      rb.copied, w0.copied + w1.copied + w2.copied);
    ok &= rb_claim_eq("bytes == this run's own byte count",
                      rb.bytes, w0.bytes + w1.bytes + w2.bytes);
    ok &= rb_claim_atleast("bytes > 0: guest memory really was written", rb.bytes, 1);
    ok &= rb_claim_eq("batches == the publications this run forced",
                      rb.batches, w0.batches + w1.batches + w2.batches);
    if (g_rb_bad)
        ok = 0;

    /* ── pass 2: the same frames against the CPU executor ──────────────── */
    puts("-- pass 2: the same frames against the CPU executor --");
    nv2a_backend_register(NULL);
    rb_reset();

    rb_frame0_draws();
    rb_frame0_sample();
    rb_plan0(plan);
    ok &= rb_check("TFL: the flush (software executor)", RB_TFL, &plan[RB_TFL], 0);
    /* rb_check_unpublished is pass 1's alone: it checks what the GPU may NOT have published
     * yet, and the CPU executor has no publication -- it writes guest memory as it draws. */
    rb_flip("readback gate: frame 0's flip (software)");
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];

        if (i == RB_U) {
            RbPlan u;

            memset(&u, 0, sizeof u);
            rb_box(&u, 8, 8, 24, 24, RB_C32_FL);
            _snprintf(what, sizeof what, "U (software): the sampled window's colour");
            ok &= rb_check(what, RB_U, &u, GATE_TEX_TOL);   /* the sampler's step, as above */
        } else {
            _snprintf(what, sizeof what, "%s (software): the plan, by the other rasteriser",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        }
        _snprintf(what, sizeof what, "%s: byte for byte what the Vulkan back end left",
                  k_rb_surf[i].name);
        ok &= rb_check_against(what, i, snap0[i], 0);
    }
    rb_frame1_draws();
    rb_plan0(plan);
    for (i = 0; i < RB_NPATCH; i++)
        rb_box(&plan[k_rb_patch[i].idx], k_rb_patch[i].x0, k_rb_patch[i].y0, k_rb_patch[i].x1,
               k_rb_patch[i].y1, k_rb_patch[i].argb);
    rb_box(&plan[RB_U], 8, 8, 24, 24, RB_C32_FL);
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];

        _snprintf(what, sizeof what, "%s (software): the plan after frame 1", k_rb_surf[i].name);
        ok &= rb_check(what, i, &plan[i], 0);
        _snprintf(what, sizeof what, "%s: byte for byte what the Vulkan back end left after "
                  "frame 1", k_rb_surf[i].name);
        ok &= rb_check_against(what, i, snap1[i], 0);
    }
    if (g_rb_bad)
        ok = 0;

    for (i = 0; i < RB_NSURF; i++) {
        free(snap0[i]);
        free(snap1[i]);
    }
    fflush(stdout);
    if (!ok) {
        fprintf(stderr, "FAIL: gpu_backend_parity_vk --readback-pack\n");
        return 1;
    }
    puts("PASS: gpu_backend_parity_vk --readback-pack (every target's published bytes were the "
         "plan's in both passes, the 16-bit ones through the GPU pack kernel with the "
         "truncation boundary colours intact, and the counters matched the gate's own count)");
    return 0;
}

/* ── The sub-allocated memory gate (--suballoc) ───────────────────────────
 *
 * The claim (plan item N4.4): the back end's device memory is sub-allocated out of a few
 * vkAllocateMemory'd blocks -- one pool per (memory type, linear/non-linear) pair -- instead of
 * one allocation per resource, while every guest byte it writes stays the byte this file's plan
 * says. Two claims, then, in one run:
 *
 *   1. The bytes. The two frames of the --readback-pack fixture drive this change too: seven
 *      targets in both formats, the odd rectangle, the padded rows, the texture window that U
 *      samples out of TFL, and the self-sampling snapshot. Each target's whole RB_REGION is
 *      compared with this file's own plan, then the identical frames are replayed against the
 *      CPU executor and the two passes' bytes are compared with each other. A pool that handed
 *      a resource the wrong range, or an offset that walked it into its neighbour's bytes,
 *      changes one of those bytes. The depth path runs the file's own absolute cases
 *      (run_depth_order/run_depth_clear, gate = 1), which decide their bytes with expect32
 *      against constants rather than against the CPU executor.
 *   2. The reduction, from the allocator's own counters. The texture is the churn path: its
 *      image is re-created whenever the guest bytes under it hash differently, and before this
 *      item each re-creation was a fresh vkAllocateMemory. The loop below rewrites the
 *      fixture's texture source region and re-runs the sampler draw N_TEXTURE_CHURN times, so
 *      the same resource is destroyed and re-created that many times; the gate then requires
 *      those re-creations to have taken ranges out of the pools whose alloc_calls grew by
 *      strictly fewer than N_TEXTURE_CHURN -- the naive count, stated in the check. The memory
 *      claims are read from nv2a_vk_backend_memory_stats(), and every field of that struct is
 *      printed for the workload (the churn loop prints its own six), so a failure carries its
 *      own numbers.
 *
 * The counters decide only the allocation claims: the byte oracle is always the plan, the CPU
 * executor and this file's constants, never the allocator. A mode of its own because it
 * installs the Vulkan back end itself, drives its own frames, and reports its own PASS line;
 * like its siblings it skips (77) only when the back end cannot be installed on a software
 * device. */

#define N_TEXTURE_CHURN 8u           /* the re-creations the allocation reduction is decided over */
#define SUB_TEX_BYTES   (16u * 128u) /* that texture: 16 rows of its pitch of 128 bytes */

/* A signed difference, so a gauge that shrank prints as a negative number and never as a
 * wrapped unsigned one. */
static long long sub_diff(uint64_t cur, uint64_t base)
{
    return cur >= base ? (long long)(cur - base) : -(long long)(base - cur);
}

/* Every field of nv2a_vk_backend_memory_stats, m1 against m0: the counters that only ever grow
 * as their difference, the gauges, peaks and byte counts as a signed difference beside the
 * value they stand at in m1, and the write-once latches as they stand in m1 (the difference of
 * a value written once is either zero or a wrap, so it is not printed as one). */
static void sub_mem_print(const char *tag, const Nv2aVkMemoryStats *base,
                          const Nv2aVkMemoryStats *cur)
{
    printf("  [memory %s] cumulative, m1-m0: blocks=%llu blocks_retired=%llu ranges=%llu "
           "reuse_hits=%llu dedicated=%llu\n", tag,
           (unsigned long long)(cur->blocks - base->blocks),
           (unsigned long long)(cur->blocks_retired - base->blocks_retired),
           (unsigned long long)(cur->ranges - base->ranges),
           (unsigned long long)(cur->reuse_hits - base->reuse_hits),
           (unsigned long long)(cur->dedicated - base->dedicated));
    printf("             dedicated reasons, m1-m0: required=%llu prefer=%llu oversize=%llu "
           "noroom=%llu nohost=%llu\n",
           (unsigned long long)(cur->dedicated_required - base->dedicated_required),
           (unsigned long long)(cur->dedicated_prefer - base->dedicated_prefer),
           (unsigned long long)(cur->dedicated_oversize - base->dedicated_oversize),
           (unsigned long long)(cur->dedicated_noroom - base->dedicated_noroom),
           (unsigned long long)(cur->dedicated_nohost - base->dedicated_nohost));
    printf("             m1-m0: alloc_calls=%llu released=%llu parked_lost=%llu "
           "alloc_failures=%llu bind_failures=%llu map_failures=%llu rollbacks=%llu "
           "fallbacks=%llu\n", (unsigned long long)(cur->alloc_calls - base->alloc_calls),
           (unsigned long long)(cur->released - base->released),
           (unsigned long long)(cur->parked_lost - base->parked_lost),
           (unsigned long long)(cur->alloc_failures - base->alloc_failures),
           (unsigned long long)(cur->bind_failures - base->bind_failures),
           (unsigned long long)(cur->map_failures - base->map_failures),
           (unsigned long long)(cur->rollbacks - base->rollbacks),
           (unsigned long long)(cur->fallbacks - base->fallbacks));
    printf("             at m1, signed change from m0 in parentheses: pools=%lld(%llu) "
           "parked=%lld(%llu) parked_peak=%lld(%llu)\n", sub_diff(cur->pools, base->pools),
           (unsigned long long)cur->pools, sub_diff(cur->parked, base->parked),
           (unsigned long long)cur->parked, sub_diff(cur->parked_peak, base->parked_peak),
           (unsigned long long)cur->parked_peak);
    printf("             bytes at m1, signed change from m0 in parentheses: reserved_bytes=%lld"
           "(%llu) used_bytes=%lld(%llu) free_bytes=%lld(%llu) largest_free=%lld(%llu) "
           "padding_bytes=%lld(%llu)\n", sub_diff(cur->reserved_bytes, base->reserved_bytes),
           (unsigned long long)cur->reserved_bytes,
           sub_diff(cur->used_bytes, base->used_bytes), (unsigned long long)cur->used_bytes,
           sub_diff(cur->free_bytes, base->free_bytes), (unsigned long long)cur->free_bytes,
           sub_diff(cur->largest_free, base->largest_free), (unsigned long long)cur->largest_free,
           sub_diff(cur->padding_bytes, base->padding_bytes),
           (unsigned long long)cur->padding_bytes);
    printf("             m1: metadata_bytes=%llu dedicated_bytes=%llu peak_used=%llu\n",
           (unsigned long long)cur->metadata_bytes, (unsigned long long)cur->dedicated_bytes,
           (unsigned long long)cur->peak_used);
    printf("             write-once latches at m1: first_shared_block=%llu (kind %llu) "
           "first_reuse_off=%llu first_reuse_block=%llu\n",
           (unsigned long long)cur->first_shared_block,
           (unsigned long long)cur->first_shared_block_kind,
           (unsigned long long)cur->first_reuse_off, (unsigned long long)cur->first_reuse_block);
}

/* rb_claim_eq/rb_claim_atleast cover equality and "at least"; a bounded claim (fewer than, at
 * most) has its own line here, with this mode's own name on the failure. */
static int sub_claim_bound(const char *what, uint64_t got, uint64_t bound, int strict)
{
    int ok = strict ? got < bound : got <= bound;

    printf("      %-52s got %-8llu want %s%-8llu %s\n", what, (unsigned long long)got,
           strict ? "<" : "<=", (unsigned long long)bound, ok ? "ok" : "FAIL");
    if (!ok)
        fprintf(stderr, "FAIL: suballoc gate -- %s: nv2a_vk_backend_memory_stats reports %llu "
                        "where the gate needs %s %llu\n", what, (unsigned long long)got,
                strict ? "<" : "<=", (unsigned long long)bound);
    return ok;
}

static int run_suballoc(void)
{
    Nv2aVkMemoryStats m0, m1, c0, c1;
    Nv2aVkReadbackStats rb0, rb, prev, d;
    RbPlan plan[RB_NSURF];
    RbDirty p2[RB_NPATCH];
    RbWant w0, w1, w2;
    uint8_t *snap0[RB_NSURF], *snap1[RB_NSURF];
    const uint32_t tex_va = k_rb_surf[RB_TFL].va + RB_FL_WIN_OFF;   /* the sampler's texture */
    uint64_t d_blocks, d_ranges, d_reuse, d_allocs, d_dedicated;
    uint64_t k_blocks, k_ranges, k_reuse, k_released, k_allocs, k_dedicated;
    unsigned i, n;
    int ok = 1;

    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        puts("SKIP: gpu_backend_parity_vk --suballoc -- the Vulkan back end could not be installed "
             "on a software device (no Vulkan, no matching device, or an elevated process: the "
             "loader ignores VK_DRIVER_FILES there)");
        return 77;
    }

    memset(snap0, 0, sizeof snap0);
    memset(snap1, 0, sizeof snap1);
    memset(&m0, 0, sizeof m0);
    memset(&m1, 0, sizeof m1);
    nv2a_vk_backend_readback_stats(&rb0);
    printf("-- sub-allocated memory gate (%u targets, %u of them 5:6:5, %u frames, %u texture "
           "re-creations) --\n", (unsigned)RB_NSURF, rb_n16(), (unsigned)RB_FRAMES,
           (unsigned)N_TEXTURE_CHURN);
    puts("-- gate fixtures --");
    ok &= rb_fixture(RB_FRAMES);

    w0 = rb_want(k_rb_stage0, (unsigned)(sizeof k_rb_stage0 / sizeof k_rb_stage0[0]));
    w1 = rb_want(k_rb_stage1, (unsigned)(sizeof k_rb_stage1 / sizeof k_rb_stage1[0]));
    w2 = rb_want(p2, rb_stage2(p2));

    /* The workload's baseline, read after the install so the install's own allocations (the
     * ring's slots) are not counted as a change the workload made. */
    nv2a_vk_backend_memory_stats(&m0);
    printf("  [memory baseline] blocks=%llu ranges=%llu alloc_calls=%llu dedicated=%llu "
           "reserved=%llu used=%llu free=%llu\n", (unsigned long long)m0.blocks,
           (unsigned long long)m0.ranges, (unsigned long long)m0.alloc_calls,
           (unsigned long long)m0.dedicated, (unsigned long long)m0.reserved_bytes,
           (unsigned long long)m0.used_bytes, (unsigned long long)m0.free_bytes);

    /* ── pass 1: the Vulkan back end, bytes first ──────────────────────── */
    puts("-- pass 1: the Vulkan back end --");
    rb_reset();
    prev = rb0;

    rb_frame0_draws();
    rb_frame0_sample();

    puts("-- the mid-frame flush (the sampled window's target) --");
    rb_plan0(plan);
    ok &= rb_check("TFL: published by the flush the texture bind forced", RB_TFL, &plan[RB_TFL], 0);
    ok &= rb_check_unpublished("suballoc");
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 0", &d);
    ok &= rb_stage_counts("the mid-frame flush", &w0, &d);
    prev = rb;

    rb_flip("suballoc gate: frame 0's flip");
    puts("-- frame 0's flip --");
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];

        if (i == RB_U) {
            /* The texture U samples out of TFL: the file's own convention allows a sampled
             * UNORM target one step per channel (GATE_TEX_TOL), and U's window is a flat
             * colour, so the tolerance carries no part of the byte claim. */
            RbPlan u;

            memset(&u, 0, sizeof u);
            rb_box(&u, 8, 8, 24, 24, RB_C32_FL);
            _snprintf(what, sizeof what, "U: the window sampled from TFL, as published");
            ok &= rb_check(what, RB_U, &u, GATE_TEX_TOL);
        } else {
            _snprintf(what, sizeof what, "%s: this frame's drawing, published by the flip",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        }
    }
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 1", &d);
    ok &= rb_stage_counts("frame 0's flip (6 dirty targets, both formats)", &w1, &d);
    prev = rb;
    for (i = 0; i < RB_NSURF; i++)
        snap0[i] = snapshot_va(k_rb_surf[i].va, RB_REGION);

    rb_frame1_draws();
    puts("-- frame 1's flip (three targets re-loaded and patched) --");
    rb_plan0(plan);
    for (i = 0; i < RB_NPATCH; i++)
        rb_box(&plan[k_rb_patch[i].idx], k_rb_patch[i].x0, k_rb_patch[i].y0, k_rb_patch[i].x1,
               k_rb_patch[i].y1, k_rb_patch[i].argb);
    /* U's box comes from the sampling draw of frame 0; no frame-1 draw touches U, so the
     * snapshot of U taken at frame 0's flip is the check below -- the self-sampling case. */
    rb_box(&plan[RB_U], 8, 8, 24, 24, RB_C32_FL);
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];
        unsigned j, patched = 0;

        for (j = 0; j < RB_NPATCH; j++)
            if (k_rb_patch[j].idx == i)
                patched = 1;
        if (patched) {
            _snprintf(what, sizeof what, "%s: the patch over its own published bytes",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        } else {
            _snprintf(what, sizeof what, "%s: untouched by frame 1, so unchanged",
                      k_rb_surf[i].name);
            ok &= rb_check_against(what, i, snap0[i], 0);
        }
    }
    nv2a_vk_backend_readback_stats(&rb);
    rb_sub(&rb, &prev, &d);
    rb_print("stage 2", &d);
    ok &= rb_stage_counts("frame 1's flip (3 patched targets)", &w2, &d);
    for (i = 0; i < RB_NSURF; i++)
        snap1[i] = snapshot_va(k_rb_surf[i].va, RB_REGION);

    /* The depth buffer: a pooled image of its own class, with the file's absolute cases as the
     * oracle (expect32 against this file's constants). */
    puts("-- the depth path (a pooled depth image) --");
    ok &= run_depth_order("suballoc", 1);
    ok &= run_depth_clear("suballoc", 1);

    /* ── the texture churn: the same resource re-created N_TEXTURE_CHURN times ────────────
     *
     * The back end re-creates a texture image when the guest bytes under it hash differently.
     * The loop rewrites the fixture's sampler source region (the 16 rows of pitch 128 that U
     * samples out of TFL, wholly inside TFL's own RB_REGION), with a pattern that differs every
     * iteration, and re-runs the fixture's texture draw. TFL itself was published by frame 1's
     * flip and is not dirty here, so the texture bind does not re-flush the target over the
     * bytes this loop writes: what the texture is created from is what this loop wrote. The
     * counters below are this loop's own, so the reduction is decided over the re-creations
     * alone, not over the fixture's first creation. */
    puts("-- the texture churn (pooled reuse under re-created resources) --");
    nv2a_vk_backend_memory_stats(&c0);
    for (n = 0; n < N_TEXTURE_CHURN; n++) {
        uint32_t col = 0xFF000000u | ((uint32_t)n * 0x00010101u) | 0x0000FFu;
        unsigned j;

        for (j = 0; j < SUB_TEX_BYTES; j += 4u)
            *(uint32_t *)(G(tex_va) + j) = col;         /* a different hash every iteration */
        rb_frame0_sample();                             /* the same texture address, re-bound */
        /* Drain the ring between re-creations. A texture image released while a recording is
         * open is PARKED, not handed out (that is the allocator's lifetime rule, and the unit
         * gate decides it); the flip's fence wait is what folds the parked range back into its
         * block, so the next iteration's re-creation is the reuse this loop is here to reach.
         * The flip publishes the dirty TARGETS, not the texture source region this loop writes,
         * so the bytes the next image is created from are still the ones written above. */
        rb_flip("suballoc: the churn iteration's flip (drains the ring, folding the parked "
                "releases)");
    }
    nv2a_vk_backend_memory_stats(&c1);
    k_blocks = c1.blocks - c0.blocks;
    k_ranges = c1.ranges - c0.ranges;
    k_reuse = c1.reuse_hits - c0.reuse_hits;
    k_released = c1.released - c0.released;
    k_allocs = c1.alloc_calls - c0.alloc_calls;
    k_dedicated = c1.dedicated - c0.dedicated;
    printf("  [churn] %u re-creations: ranges=%llu reuse_hits=%llu released=%llu alloc_calls=%llu "
           "blocks=%llu dedicated=%llu\n", (unsigned)N_TEXTURE_CHURN,
           (unsigned long long)k_ranges, (unsigned long long)k_reuse,
           (unsigned long long)k_released, (unsigned long long)k_allocs,
           (unsigned long long)k_blocks, (unsigned long long)k_dedicated);
    {
        char line[160];

        _snprintf(line, sizeof line, "the %u re-creations (N_TEXTURE_CHURN) each took a pool range: "
                  "ranges_delta >= N_TEXTURE_CHURN", (unsigned)N_TEXTURE_CHURN,
                  (unsigned)N_TEXTURE_CHURN);
        ok &= rb_claim_atleast(line, k_ranges, (uint64_t)N_TEXTURE_CHURN);
        _snprintf(line, sizeof line, "alloc_calls_delta < N_TEXTURE_CHURN: fewer than %u "
                  "vkAllocateMemory calls for those %u re-creations (one per resource would be %u)",
                  (unsigned)N_TEXTURE_CHURN, (unsigned)N_TEXTURE_CHURN, (unsigned)N_TEXTURE_CHURN);
        ok &= sub_claim_bound(line, k_allocs, (uint64_t)N_TEXTURE_CHURN, 1);
    }

    /* ── the workload's memory counters ─────────────────────────────────── */
    nv2a_vk_backend_memory_stats(&m1);
    d_blocks = m1.blocks - m0.blocks;
    d_ranges = m1.ranges - m0.ranges;
    d_reuse = m1.reuse_hits - m0.reuse_hits;
    d_allocs = m1.alloc_calls - m0.alloc_calls;
    d_dedicated = m1.dedicated - m0.dedicated;
    puts("-- the allocator's own counters (nv2a_vk_backend_memory_stats, m1 read against m0) --");
    sub_mem_print("workload", &m0, &m1);
    puts("-- the memory claims, from the counters above --");
    /* The ring's streams, the staging pair and the fixture's first targets are already pooled
     * by the time m0 is read, so the workload's own window can legitimately need NO new block
     * at all: what the reduction claim has to decide is that its ranges cost far fewer
     * allocations than one per resource. */
    ok &= rb_claim_atleast("ranges_delta >= 1: the workload really took pooled ranges",
                           d_ranges, 1);
    ok &= sub_claim_bound("alloc_calls_delta + dedicated_delta <= ranges_delta / 2: the "
                          "workload's ranges cost at most half as many allocations as resources "
                          "(one per resource would be ranges_delta)",
                          d_allocs + d_dedicated, d_ranges / 2u, 0);
    ok &= rb_claim_atleast("ranges_delta >= 2 * blocks_delta: several resources shared a block",
                           d_ranges, 2u * d_blocks);
    /* The latch is a POOL index, so 0 is a perfectly good answer; "unset" is (uint32_t)-1. */
    ok &= rb_claim_eq("m1.first_shared_block != (uint32_t)-1: at least two live ranges were in "
                      "one block (the sharing evidence on the real driver)",
                      m1.first_shared_block != (uint32_t)-1 ? 1u : 0u, 1u);
    ok &= rb_claim_eq("alloc_calls_delta == blocks_delta + dedicated_delta: the counter is the "
                      "allocator's own", d_allocs, d_blocks + d_dedicated);
    ok &= rb_claim_atleast("reuse_hits_delta >= 1: a range was taken from a freed range, not the "
                           "bump", d_reuse, 1);
    ok &= rb_claim_eq("m1.parked_lost == 0: no range was parked permanently", m1.parked_lost, 0);
    ok &= sub_claim_bound("used_bytes <= reserved_bytes: no pool block is over-committed",
                          m1.used_bytes, m1.reserved_bytes, 0);
    ok &= rb_claim_eq("free_bytes + used_bytes == reserved_bytes: the byte accounting closes",
                      m1.free_bytes + m1.used_bytes, m1.reserved_bytes);
    if (d_dedicated) {
        uint64_t req = m1.dedicated_required - m0.dedicated_required;
        uint64_t pref = m1.dedicated_prefer - m0.dedicated_prefer;
        uint64_t over = m1.dedicated_oversize - m0.dedicated_oversize;
        uint64_t room = m1.dedicated_noroom - m0.dedicated_noroom;
        uint64_t host = m1.dedicated_nohost - m0.dedicated_nohost;

        printf("  [dedicated] %llu dedicated allocation(s), reason deltas: required=%llu prefer=%llu "
               "oversize=%llu noroom=%llu nohost=%llu; bytes=%llu (m1), fallbacks=%llu\n",
               (unsigned long long)d_dedicated, (unsigned long long)req,
               (unsigned long long)pref, (unsigned long long)over, (unsigned long long)room,
               (unsigned long long)host, (unsigned long long)m1.dedicated_bytes,
               (unsigned long long)(m1.fallbacks - m0.fallbacks));
        if (!req && !pref && !over && !room && !host)
            printf("  [dedicated] no reason counter moved: a pool block's allocation failed over "
                   "once to a dedicated allocation (%llu fallback(s))\n",
                   (unsigned long long)(m1.fallbacks - m0.fallbacks));
    } else
        puts("  [dedicated] dedicated_delta == 0: every resource of this workload fitted a pool "
             "(not required of the change: a large staging buffer may legitimately go dedicated)");
    if (g_rb_bad)
        ok = 0;

    /* ── pass 2: the same frames against the CPU executor ──────────────── */
    puts("-- pass 2: the same frames against the CPU executor --");
    nv2a_backend_register(NULL);
    /* rb_reset() prefills every fixture target again, which is also what clears the churn
     * loop's writes out of TFL's region: pass 2 starts from the fixture's own state. */
    rb_reset();

    rb_frame0_draws();
    rb_frame0_sample();
    rb_plan0(plan);
    ok &= rb_check("TFL: the flush (software executor)", RB_TFL, &plan[RB_TFL], 0);
    /* rb_check_unpublished is pass 1's alone: it checks what the GPU may NOT have published
     * yet, and the CPU executor has no publication -- it writes guest memory as it draws. */
    rb_flip("suballoc gate: frame 0's flip (software)");
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];

        if (i == RB_U) {
            RbPlan u;

            memset(&u, 0, sizeof u);
            rb_box(&u, 8, 8, 24, 24, RB_C32_FL);
            _snprintf(what, sizeof what, "U (software): the sampled window's colour");
            ok &= rb_check(what, RB_U, &u, GATE_TEX_TOL);   /* the sampler's step, as above */
        } else {
            _snprintf(what, sizeof what, "%s (software): the plan, by the other rasteriser",
                      k_rb_surf[i].name);
            ok &= rb_check(what, i, &plan[i], 0);
        }
        _snprintf(what, sizeof what, "%s: byte for byte what the Vulkan back end left (incl. the "
                  "self-sampled U)", k_rb_surf[i].name);
        ok &= rb_check_against(what, i, snap0[i], 0);
    }
    rb_frame1_draws();
    rb_plan0(plan);
    for (i = 0; i < RB_NPATCH; i++)
        rb_box(&plan[k_rb_patch[i].idx], k_rb_patch[i].x0, k_rb_patch[i].y0, k_rb_patch[i].x1,
               k_rb_patch[i].y1, k_rb_patch[i].argb);
    rb_box(&plan[RB_U], 8, 8, 24, 24, RB_C32_FL);
    for (i = 0; i < RB_NSURF; i++) {
        char what[128];

        _snprintf(what, sizeof what, "%s (software): the plan after frame 1", k_rb_surf[i].name);
        ok &= rb_check(what, i, &plan[i], 0);
        _snprintf(what, sizeof what, "%s: byte for byte what the Vulkan back end left after "
                  "frame 1", k_rb_surf[i].name);
        ok &= rb_check_against(what, i, snap1[i], 0);
    }
    if (g_rb_bad)
        ok = 0;

    for (i = 0; i < RB_NSURF; i++) {
        free(snap0[i]);
        free(snap1[i]);
    }
    fflush(stdout);
    if (!ok) {
        fprintf(stderr, "FAIL: gpu_backend_parity_vk --suballoc\n");
        return 1;
    }
    puts("PASS: gpu_backend_parity_vk --suballoc (every target's, the sampled texture's and the "
         "depth buffer's guest bytes were the plan's in both passes over two frames, the pool "
         "churn re-created the texture 8 times for fewer than 8 vkAllocateMemory calls, and the "
         "counters that decided it are printed above)");
    return 0;
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char **argv)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *ref[NPAR][2], *got[NPAR][2];
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

    /* The submission-ring lifetime gate is a mode of its own: it installs the Vulkan back end
     * itself, runs its own frames through it, then replays the identical frames against the CPU
     * executor and compares the two, and reports its own PASS line. It exits 77 when there is no
     * software device, like the rest of this file. */
    if (argc > 1 && strcmp(argv[1], "--frames-in-flight") == 0) {
        int rc = run_frames_in_flight();
        fflush(stdout);
        nv2a_set_commit_consumer(NULL);
        VirtualFree(pbmem, 0, MEM_RELEASE);
        free(instance); free(ramin); free(ram);
        xbox_MemoryLayoutShutdown();
        return rc;
    }

    /* The batched-readback pack gate is a mode of its own for the same reason: it installs
     * the Vulkan back end, drives its own frames through it, then replays them against the
     * CPU executor, and reports its own PASS line. It exits 77 when there is no software
     * device, like the rest of this file. */
    if (argc > 1 && strcmp(argv[1], "--readback-pack") == 0) {
        int rc = run_readback_pack();
        fflush(stdout);
        nv2a_set_commit_consumer(NULL);
        VirtualFree(pbmem, 0, MEM_RELEASE);
        free(instance); free(ramin); free(ram);
        xbox_MemoryLayoutShutdown();
        return rc;
    }

    /* The sub-allocated-memory gate is a mode of its own for the same reason: it installs the
     * Vulkan back end, drives its own frames through it -- with the fixture's texture re-created
     * N_TEXTURE_CHURN times so the pools' churn and reuse are really reached -- checks the bytes
     * against this file's own plan and then against the CPU executor, and decides the allocation
     * reduction from nv2a_vk_backend_memory_stats(). It reports its own PASS line and exits 77
     * when there is no software device, like the rest of this file. */
    if (argc > 1 && strcmp(argv[1], "--suballoc") == 0) {
        int rc = run_suballoc();
        fflush(stdout);
        nv2a_set_commit_consumer(NULL);
        VirtualFree(pbmem, 0, MEM_RELEASE);
        free(instance); free(ramin); free(ram);
        xbox_MemoryLayoutShutdown();
        return rc;
    }

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

    /* The register combiners. The Vulkan pass runs first and the CPU reference pass second, because
     * programming the combiners sticks in the executor for good (rc_seen never clears): every CPU
     * reference taken after that point would silently be a combiner batch. The CPU pass is also
     * checked against nv2a_rc_eval, which is what shows the oracle is the arithmetic the executor
     * runs, and the comparison runs last, so the reference is known to exist and to show the
     * drawing before it is used. rc_run also drives the randomized suite (2 fixed seeds x 256
     * cases, re-derivable from (seed, index)) in both passes; its register tail returns every
     * combiner and texture register to the never-programmed state, so the CPU pass's sweep and
     * whole-surface cases see the same inherited state the Vulkan pass's did. */
    puts("-- Vulkan back end: register combiners --");
    rc_table();
    ok &= rc_run(1);
    puts("-- Vulkan back end: four distinguishable textures (draw contract) --");
    ok &= run_four_textures();
    /* The back end prints its [GPUVK] counters line on the first flip and then at most once every
     * VB_STATS_MS (10 s), so a run this short would never show the combiner draws it just made.
     * Wait out that window and flip once more: the line is cumulative, so it then carries the
     * rc_draws this suite produced. */
    Sleep(10300);
    pb_begin(); pb_flip();
    ok &= submit("vulkan (combiner counter flush)");
    puts("-- software executor: register combiners --");
    nv2a_backend_register(NULL);
    rc_table();
    ok &= rc_run(0);
    puts("-- register combiners: the whole-surface cases --");
    ok &= rc_compare();

    fflush(stdout);
    nv2a_set_commit_consumer(NULL);
    for (c = 0; c < NPAR; c++)
        for (i = 0; i < 2; i++) { free(ref[c][i]); free(got[c][i]); }
    for (i = 0; i < RC_NSURF; i++) { free(rc_ref[i]); free(rc_got[i]); }
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fprintf(stderr, "FAIL: gpu_backend_parity_vk\n"); return 1; }
    puts("PASS: gpu_backend_parity_vk (software and Vulkan agree after the flip)");
    return 0;
}
