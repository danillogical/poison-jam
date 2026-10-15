/* Packet PV-04R: the rejected-identity diagnostic of the V5 vertex-program path must
 * not crash on a MALFORMED corpus entry.
 *
 * The seam under test is the real one, exactly as test_vsh_aot_programs.c drives it:
 * standalone NV2A, an NV097 object on subchannel 0, hand-built pushbuffers, the real
 * executor on the commit seam, nv2a_vk_backend_install() and
 * nv2a_vk_backend_set_programs(). Nothing here is copied helper code and nothing here
 * calls a production function directly: every case uploads a real program through the
 * pushbuffer and reads the verdict out of nv2a_vk_backend_program_stats and the
 * surface bytes the back end wrote back into guest memory at the flip.
 *
 * vb_program_lookup (src/video/nv2a_vk_backend.c) rejects an entry whose identity does
 * not match the batch, and then its diagnostic (vb_program_log_once) digests that SAME
 * rejected entry -- e->words with e->len. When the rejection is because `words` is NULL
 * or because `len` is nonsense, that digest reads through NULL or runs for 2^32
 * instructions: the process dies with an access violation (0xC0000005) instead of
 * reporting a declined draw. This test drives that path with a corpus copy that is
 * malformed in memory only, and requires a clean rejection with the CPU fallback.
 *
 * Cases, in order (each one draws the same real program through the executor):
 *   0. POSITIVE CONTROL, real unmodified table: hits >= 1, tris >= 1, identity_fail == 0,
 *      the surface agrees with the CPU reference. Without this, a later "rejected"
 *      verdict could be an artifact of the program path never running.
 *   1. entry `words` = NULL (hash and len unchanged).
 *   2. entry `len` = 0xFFFFFFFF.
 *   3. entry `len` = the real length + 1.
 *   4. entry `len` = the real length - 1.
 *   5. entry `spirv` = NULL.
 *   6. entry `spirv_size` = 0.
 *   7. POSITIVE CONTROL again after the real table is restored, so the malformed copies
 *      provably did not damage the real corpus.
 *
 * Every malformed case must show: the process survived; identity_fail advanced by
 * EXACTLY 1; hits, misses, declines, tris and const_uploads did not move (the hash was
 * found -- this was not a miss -- and no shader was selected); the executor's CPU
 * interpreter drew the batch, so the surface is not the clear colour and agrees with a
 * CPU reference pass captured the same way test_vsh_aot_programs.c captures its own;
 * and nv2a_vsh_programs still matches the snapshot taken at startup.
 *
 * There is no skip mode. The whole point of this file is the backend path, so a device
 * that cannot be installed is a FAILURE (exit 1), never 77: CTest counts 77 as Not Run,
 * and a regression that cannot report the crash is worse than none. A crash is likewise
 * a failure: the run only exits 0 when every case passed.
 *
 * RECOMP_GPU_VSH is deliberately NOT set: =0 makes the executor decline the program path
 * before it ever calls draw_program, which would turn every expected rejection into a
 * decline that never reached the diagnostic.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_vsh_interp.h"
#include "video/nv2a_vk_backend.h"
/* The generated corpus header, from whichever include path the build offers it on. */
#if defined(__has_include)
#  if __has_include("nv2a_vsh_programs.h")
#    include "nv2a_vsh_programs.h"
#  else
#    include "../src/recomp/gen/shaders/nv2a_vsh_programs.h"
#  endif
#else
#  include "nv2a_vsh_programs.h"
#endif
#include "recomp_types.h"
#include <io.h>
#include <math.h>
#include <stdarg.h>
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

/* The kernel bridge references the game dispatcher/diagnostics; this focused fixture does not
 * start guest threads, so keep those hooks inert (the same set the parity fixtures define). */
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

/* ── Failure reporting, mirrored to the console and into the captured stderr ──
 * The identity diagnostic writes to stderr, and its line is read back at the end as
 * supporting evidence that the diagnostic ran at all; `fail` writes to the original
 * descriptor too, so a failure is still visible where the test is being watched. */
static int g_stderr_saved = -1;

static void fail(const char *fmt, ...)
{
    char b[1024];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = _vsnprintf(b, sizeof b - 1, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)strlen(b);
    b[n] = '\0';
    if (g_stderr_saved >= 0) _write(g_stderr_saved, b, n);
    fputs(b, stderr);
    fflush(stderr);
}

/* ── Guest memory map ──────────────────────────────────────────────────────
 * Plain RAM addresses well clear of the (synthetic, empty) XBE image. */
#define SURF(n) (0x00400000u + (uint32_t)(n) * 0x00010000u)
#define VBUF(n) (0x00800000u + (uint32_t)(n) * 0x00010000u)
#define SURF_BYTES 0x00010000u
#define SENTINEL   0x5Au
#define CLR        0xFF101010u

/* Vertex = float3 position @0, float3 normal @12, float2 uv @24, D3DCOLOR diffuse @32. */
#define VSTRIDE 36u
#define ATTR_POS 0
#define ATTR_NRM 1
#define ATTR_UV  2
#define ATTR_COL 3

/* ── Pushbuffer builder (standalone NV2A, NV097 object on subchannel 0) ──── */
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
    if (g_pbn + 2u > 0x4000u / 4u) { fail("FAIL: test pushbuffer overflow\n"); exit(1); }
    g_pb[g_pbn++] = (1u << 18) | method;
    g_pb[g_pbn++] = value;
}
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* No alpha test, blend, culling, depth, stencil, zeta or texture; colour mask all; front CW. */
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
    pb(0x0360, 0xFFu);
    pb(0x0364, 0x207u);
    pb(0x0368, 0);
    pb(0x036C, 0xFFu);
    pb(0x0370, 0x1E00u);
    pb(0x0374, 0x1E00u);
    pb(0x0378, 0x1E00u);
    pb(0x039C, 0x405u);                            /* cull face BACK */
    pb(0x03A0, 0x900u);                            /* front face CW */
    pb(0x0214, 0);                                 /* no zeta */
    pb(0x1B0C, 0);                                 /* stage 0 off */
    pb(0x1B4C, 0);                                 /* stage 1 off */
}
static void pb_surface(uint32_t va, uint32_t w, uint32_t h)
{
    pb(0x0200, w << 16);                           /* CLIP_H: x0 w */
    pb(0x0204, h << 16);                           /* CLIP_V: y0 h */
    pb(0x0208, 0x0108u);                           /* linear A8R8G8B8 */
    pb(0x020C, w * 4u);                            /* PITCH */
    pb(0x0210, va);                                /* COLOR_OFFSET */
}
static void pb_clear(uint32_t argb)
{
    pb(0x1D90, argb);                              /* SET_COLOR_CLEAR_VALUE */
    pb(0x1D94, 0xF0u);                             /* CLEAR_SURFACE: colour */
}
static void pb_vertex_format(uint32_t vb)
{
    pb(0x1720 + ATTR_POS * 4, vb + 0);             pb(0x1760 + ATTR_POS * 4, 2u | (3u << 4) | (VSTRIDE << 8));
    pb(0x1720 + ATTR_NRM * 4, vb + 12);            pb(0x1760 + ATTR_NRM * 4, 2u | (3u << 4) | (VSTRIDE << 8));
    pb(0x1720 + ATTR_UV  * 4, vb + 24);            pb(0x1760 + ATTR_UV  * 4, 2u | (2u << 4) | (VSTRIDE << 8));
    pb(0x1720 + ATTR_COL * 4, vb + 32);            pb(0x1760 + ATTR_COL * 4, 0u | (4u << 4) | (VSTRIDE << 8));
}
static void pb_draw_triangles(uint32_t nverts)
{
    pb(0x17FC, 5u);                                /* BEGIN TRIANGLES */
    pb(0x1810, ((nverts - 1u) << 24) | 0u);        /* DRAW_ARRAYS from 0 */
    pb(0x17FC, 0u);                                /* END */
}
static void pb_flip(void) { pb(0x0130, 0); }       /* FLIP_STALL */

/* ── Program and constant upload ─────────────────────────────────────────── */
static void pb_program(const uint32_t (*words)[4], uint32_t len)
{
    uint32_t i, k;
    pb(0x1E94, 2u);                                /* TRANSFORM_EXECUTION_MODE: program */
    pb(0x1E9C, 0u);                                /* PROGRAM_LOAD slot 0 */
    pb(0x1EA0, 0u);                                /* PROGRAM_START slot 0 */
    for (i = 0; i < len; i++)
        for (k = 0; k < 4; k++)
            pb(0x0B00u + 4u * k, words[i][k]);
}
static void pb_constant(uint32_t index, float x, float y, float z, float w)
{
    pb(0x1EA4, index);                             /* CONSTANT_LOAD */
    pb(0x0B80, f2u(x)); pb(0x0B84, f2u(y)); pb(0x0B88, f2u(z)); pb(0x0B8C, f2u(w));
}

/* The constant file the corpus program is driven with, as test_vsh_aot_programs.c
 * measured it against the real interpreter: c[58]/c[59] are the viewport scale/offset the
 * D3D epilogue reads, c[96]/c[97] are the two constants the title's own programs load per
 * draw, and c[98..110] are four 4x4 matrices as scaled identities. */
static void pb_constants(void)
{
    uint32_t i;
    pb_constant(58, 1.0f, 1.0f, 1.0f, 1.0f);
    pb_constant(59, 0.0f, 0.0f, 0.0f, 0.0f);
    pb_constant(96, 1.0f, 1.0f, 1.0f, 8.0f);
    pb_constant(97, 0.25f, 0.25f, 0.25f, 1.0f);
    for (i = 0; i < 4; i++) {
        pb_constant(98u + 4u * i + 0u, 1.0f, 0.0f, 0.0f, 0.0f);
        pb_constant(98u + 4u * i + 1u, 0.0f, 1.0f, 0.0f, 0.0f);
        pb_constant(98u + 4u * i + 2u, 0.0f, 0.0f, 1.0f, 0.0f);
        pb_constant(98u + 4u * i + 3u, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    pb_constant(118, 0.5f, 0.5f, 0.5f, 1.0f);
    pb_constant(150, 1.0f, 0.5f, 0.25f, 1.0f);
}

/* The register combiners, programmed once: the executor and the back end both need rc_seen
 * set before the vertex-program path is offered to a back end at all (backend_rc_active).
 * Stage 0 outputs the interpolated diffuse (v0), the final stage is off, so a pixel is the
 * vertex colour. */
#define RC_ZERO 0x00u
#define RC_C1   0x02u
#define RC_V0   0x04u
#define RC_R0   0x0Cu
#define RC_A(r) ((r) | 0x10u)
static void pb_combiner(void)
{
    pb(0x1E70, 1u);                                /* SHADER_STAGE_PROGRAM: stage 0 = 2D */
    pb(0x1E60, 1u);                                /* COMBINER_CONTROL: 1 stage (sets rc_seen) */
    pb(0x0A60, 0x80808080u);                       /* FACTOR0 */
    pb(0x0A80, 0xFFFFFFFFu);                       /* FACTOR1 = 1: A*B is A */
    pb(0x1E20, 0x80808080u); pb(0x1E24, 0x80808080u);
    pb(0x0AC0, (RC_V0 << 24) | (RC_C1 << 16));     /* stage 0 colour: v0 * 1 -> r0 */
    pb(0x0260, (RC_A(RC_V0) << 24) | (RC_A(RC_C1) << 16));
    pb(0x1E40, RC_R0 << 4);                        /* colour out -> r0 */
    pb(0x0AA0, RC_R0 << 4);                        /* alpha out -> r0 */
    pb(0x0288, 0u); pb(0x028C, 0u);                /* no final stage: the pixel is r0 */
}

/* ── Guest memory helpers ────────────────────────────────────────────────── */
static uint8_t *G(uint32_t va) { return (uint8_t *)xbox_GetMemoryOffset() + va; }

/* The batch's three vertices, as test_vsh_aot_programs.c's probe measured them. */
static const float k_vert[3][4][4] = {
    { { 8.0f,  8.0f, 0.5f, 1.0f }, { 0.0f,  0.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.9f, 0.15f, 0.20f, 1.0f } },
    { { 56.0f, 12.0f, 0.5f, 1.0f }, { 0.5f,  0.0f, 0.5f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.2f, 0.85f, 0.30f, 1.0f } },
    { { 20.0f, 58.0f, 0.5f, 1.0f }, { 0.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.15f, 0.25f, 0.80f, 1.0f } }
};

static void write_vertices(uint32_t vb)
{
    int i;
    for (i = 0; i < 3; i++) {
        uint8_t *p = G(vb + (uint32_t)i * VSTRIDE);
        memcpy(p + 0, k_vert[i][0], 16);
        memcpy(p + 12, k_vert[i][1], 16);
        memcpy(p + 24, k_vert[i][2], 16);
        *(uint32_t *)(p + 32) = ((uint32_t)(k_vert[i][3][3] * 255.0f + 0.5f) << 24)
                              | ((uint32_t)(k_vert[i][3][0] * 255.0f + 0.5f) << 16)
                              | ((uint32_t)(k_vert[i][3][1] * 255.0f + 0.5f) << 8)
                              |  (uint32_t)(k_vert[i][3][2] * 255.0f + 0.5f);
    }
}

static void prefill(uint32_t va) { memset(G(va), SENTINEL, SURF_BYTES); }

static uint8_t *snapshot(uint32_t va, size_t n)
{
    uint8_t *p = (uint8_t *)malloc(n);
    if (p) memcpy(p, G(va), n);
    return p;
}

/* ── Surface comparison (the same rule test_vsh_aot_programs.c uses) ─────── */
typedef struct { unsigned match, worst, cov_ref, cov_got, cov_xor; } Cmp;

static uint32_t px32(const uint8_t *p, uint32_t w, int x, int y)
{
    return *(const uint32_t *)(p + ((size_t)y * w + (size_t)x) * 4u);
}
static int chan_diff(uint32_t a, uint32_t b)
{
    int i, m = 0;
    for (i = 0; i < 3; i++) {
        int d = (int)((a >> (16 - 8 * i)) & 0xFFu) - (int)((b >> (16 - 8 * i)) & 0xFFu);
        if (d < 0) d = -d;
        if (d > m) m = d;
    }
    return m;
}
static Cmp compare(const uint8_t *ref, const uint8_t *got, uint32_t w, uint32_t h)
{
    Cmp c;
    int x, y;
    memset(&c, 0, sizeof c);
    for (y = 0; y < (int)h; y++)
        for (x = 0; x < (int)w; x++) {
            uint32_t a = px32(ref, w, x, y), b = px32(got, w, x, y);
            int d = chan_diff(a, b);
            int ca = a != CLR, cb = b != CLR;
            if (d <= 8) c.match++;
            if ((unsigned)d > c.worst) c.worst = (unsigned)d;
            c.cov_ref += (unsigned)ca;
            c.cov_got += (unsigned)cb;
            c.cov_xor += (unsigned)(ca != cb);
        }
    return c;
}

/* The drawn surface must agree with the CPU reference for the same batch, under the
 * parity test's rule (a shared triangle edge is sampled a pixel differently by the two
 * rasterisers, which is a boundary effect). A surface nothing drew would otherwise
 * "agree" with a reference that also drew nothing, so >= 500 pixels must differ from the
 * clear colour on their own. */
static int check_vs_cpu(const char *who, const uint8_t *cpu, const uint8_t *gpu,
                        double perimeter)
{
    Cmp cp;
    unsigned drawn = 0, x, y, region = 0, region_bad = 0;
    double region_allowed;
    int ok = 1;

    if (!cpu || !gpu) {
        fail("FAIL: [%s] no CPU reference or no GPU image to compare\n", who);
        return 0;
    }
    cp = compare(cpu, gpu, 64, 64);
    for (y = 0; y < 64; y++)
        for (x = 0; x < 64; x++) {
            uint32_t a = px32(cpu, 64, (int)x, (int)y), b = px32(gpu, 64, (int)x, (int)y);
            int ca = a != CLR, cb = b != CLR;
            if (ca || cb) {
                region++;
                if (chan_diff(a, b) > 8) region_bad++;
            }
            if (b != CLR) drawn++;
        }
    region_allowed = perimeter + 0.02 * (double)region;
    printf("  [%s] vs the CPU executor: %.2f%% of pixels within 8, worst channel diff %u, "
           "coverage cpu=%u gpu=%u (xor %u), drawn-region mismatches %u of %u (allowed %.0f); "
           "%u of 4096 pixels are not the clear colour\n",
           who, 100.0 * cp.match / 4096.0, cp.worst, cp.cov_ref, cp.cov_got, cp.cov_xor,
           region_bad, region, region_allowed, drawn);
    if (cp.match * 100u < 4096u * 98u) {
        fail("FAIL: [%s] only %.2f%% of pixels agree with the CPU executor (need 98%%), worst "
             "channel diff %u\n", who, 100.0 * cp.match / 4096.0, cp.worst);
        ok = 0;
    }
    if ((double)region_bad > region_allowed) {
        fail("FAIL: [%s] %u drawn-region pixels disagree with the CPU executor (allowed %.0f: "
             "perimeter %.0f + 2%% of %u)\n", who, region_bad, region_allowed, perimeter, region);
        ok = 0;
    }
    if ((double)cp.cov_xor > perimeter) {
        fail("FAIL: [%s] coverage differs from the CPU executor by %u pixels (the edge band "
             "allows %.0f)\n", who, cp.cov_xor, perimeter);
        ok = 0;
    }
    if (drawn < 500) {
        fail("FAIL: [%s] only %u pixels differ from the clear colour: the batch did not land in "
             "guest memory, so the CPU interpreter did not draw it\n", who, drawn);
        ok = 0;
    }
    return ok;
}

/* ── The executor seam ───────────────────────────────────────────────────── */
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
    printf("  %-34s draws+%u clears+%u tris+%u flips_stall+%u unhandled+%u\n",
           what, a.draws - b.draws, a.clears - b.clears, a.tris_drawn - b.tris_drawn,
           a.flip_stalls - b.flip_stalls, a.unhandled - b.unhandled);
    fflush(stdout);
    if (!ok)
        fail("FAIL: pushbuffer rejected (%s): %s, subchannel %u method 0x%04X param 0x%08X, get 0x%X\n",
             what, nv2a_submit_diagnostic(g_gpu->pfifo.submit_diag),
             g_gpu->pfifo.submit_diag_subchannel, g_gpu->pfifo.submit_diag_method,
             g_gpu->pfifo.submit_diag_param, g_gpu->pfifo.submit_diag_get);
    return ok;
}

/* One batch: clear, upload the program and its constants, draw one triangle, flip. */
static void build_batch(uint32_t surf, uint32_t vb, const uint32_t (*words)[4], uint32_t len,
                        int flip)
{
    pb_begin();
    pb_reset_state();
    pb_combiner();
    pb_surface(surf, 64, 64);
    pb_clear(CLR);
    pb_constants();
    pb_program(words, len);
    pb_vertex_format(vb);
    write_vertices(vb);
    pb_draw_triangles(3);
    if (flip) pb_flip();
}

/* ── The corpus entries this test drives ─────────────────────────────────── */
/* The program test_vsh_aot_programs.c uses for its hit and identity cases: hash EC6C788B,
 * len 16, input_mask 0007. It draws a real triangle under the constant file above. */
#define HASH_A 0xEC6C788Bu

static const Nv2aVshAotEntry *table_find(uint32_t hash)
{
    size_t i;
    for (i = 0; i < nv2a_vsh_program_count; i++)
        if (nv2a_vsh_programs[i].hash == hash) return &nv2a_vsh_programs[i];
    return NULL;
}

/* The triangle's perimeter in pixels, from the interpreter's own positions for this batch:
 * the edge band the GPU-vs-CPU comparison allows. */
static double perim_words(const uint32_t (*words)[4], uint32_t len, uint32_t mask)
{
    Nv2aVshOutput o[3];
    double p = 0;
    int v, b;
    uint32_t k, i;
    for (k = 0; k < NV2A_VSH_SLOTS; k++) {
        const uint32_t zero[4] = { 0, 0, 0, 0 };
        nv2a_vsh_set_instruction(k, zero);
    }
    for (k = 0; k < len; k++) nv2a_vsh_set_instruction(k, words[k]);
    nv2a_vsh_set_start_slot(0);
    for (v = 0; v < 3; v++) {
        float in[NV2A_VSH_INPUTS][4];
        memset(in, 0, sizeof in);
        for (i = 0; i < NV2A_VSH_INPUTS; i++) in[i][3] = 1.0f;
        for (b = 0; b < 4; b++)
            if ((mask >> b) & 1u) memcpy(in[b], k_vert[v][b], 16);
        if (!nv2a_vsh_run((const float (*)[4])in, &o[v])) return 0;
    }
    for (v = 0; v < 3; v++) {
        const Nv2aVshOutput *a = &o[v], *n = &o[(v + 1) % 3];
        double dx = (double)n->pos[0] - a->pos[0], dy = (double)n->pos[1] - a->pos[1];
        p += sqrt(dx * dx + dy * dy);
    }
    return p;
}

/* ── stderr capture ──────────────────────────────────────────────────────── */
static char g_log_path[512];

static int start_stderr_capture(void)
{
    const char *tmp = getenv("TEMP");
    if (!tmp || !tmp[0]) tmp = ".";
    _snprintf(g_log_path, sizeof g_log_path, "%s\\pv04r_vsh_malformed_stderr.log", tmp);
    g_stderr_saved = _dup(2);
    if (g_stderr_saved < 0) return 0;
    if (!freopen(g_log_path, "w", stderr)) { g_stderr_saved = -1; return 0; }
    return 1;
}
static char *read_log(void)
{
    FILE *f;
    long n;
    char *b;
    fflush(stderr);
    f = fopen(g_log_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (16 << 20)) { fclose(f); return NULL; }
    b = (char *)malloc((size_t)n + 1);
    if (b) {
        size_t got = fread(b, 1, (size_t)n, f);
        b[got] = '\0';
    }
    fclose(f);
    return b;
}

/* ── The run ─────────────────────────────────────────────────────────────── */
static const uint32_t k_surf[10] = { SURF(0), SURF(1), SURF(2), SURF(3), SURF(4),
                                     SURF(5), SURF(6), SURF(7), SURF(8), SURF(9) };
static const uint32_t k_vb[10]   = { VBUF(0), VBUF(1), VBUF(2), VBUF(3), VBUF(4),
                                     VBUF(5), VBUF(6), VBUF(7), VBUF(8), VBUF(9) };
enum { S_REF = 0, S_CTRL1 = 1, S_CASE0 = 2, S_CTRL2 = 8 };

static uint8_t *g_cpu;                       /* the CPU reference for the A batch */
static const Nv2aVshAotEntry *g_entry;       /* the real entry for HASH_A */
static size_t g_idx;                         /* its index in the real table */
static double g_perim;
static Nv2aVshAotEntry g_pristine[256];      /* the real table as it was at startup */
static Nv2aVshAotEntry g_copy[256];          /* the copy the back end is handed */
static size_t g_ncopy;

/* The real generated table must never be modified: only g_copy is ever corrupted. */
static int table_intact(const char *who)
{
    if (memcmp(nv2a_vsh_programs, g_pristine, g_ncopy * sizeof g_pristine[0]) != 0) {
        size_t i;
        for (i = 0; i < g_ncopy; i++) {
            const Nv2aVshAotEntry *a = &nv2a_vsh_programs[i], *b = &g_pristine[i];
            if (a->hash != b->hash || a->len != b->len || a->input_mask != b->input_mask
                || a->digest != b->digest || a->words != b->words || a->spirv != b->spirv
                || a->spirv_size != b->spirv_size) {
                fail("FAIL: [%s] the REAL table was modified at entry %zu: hash %08X->%08X len "
                     "%u->%u mask %04X->%04X digest %016llX->%016llX words %p->%p spirv %p->%p "
                     "spirv_size %zu->%zu\n", who, i, b->hash, a->hash, b->len, a->len,
                     b->input_mask, a->input_mask, (unsigned long long)b->digest,
                     (unsigned long long)a->digest, (const void *)b->words, (const void *)a->words,
                     (const void *)b->spirv, (const void *)a->spirv, b->spirv_size, a->spirv_size);
                return 0;
            }
        }
        fail("FAIL: [%s] nv2a_vsh_programs differs from the startup snapshot but no entry "
             "differs field by field\n", who);
        return 0;
    }
    return 1;
}

/* Case 0 and case 7: the real, unmodified table must hit. `expect_identity_fail` is the
 * cumulative identity_fail the run must be at before the control: 0 for the first control,
 * and the number of malformed cases for the second. */
static int positive_control(const char *who, uint32_t surf, uint32_t vb, int want_hits_min,
                            uint64_t expect_identity_fail)
{
    Nv2aVkProgramStats b4, af;
    uint8_t *img;
    int ok = 1;

    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    memset(&b4, 0, sizeof b4);
    nv2a_vk_backend_program_stats(&b4);
    prefill(surf);
    build_batch(surf, vb, g_entry->words, g_entry->len, 1);
    ok &= submit(who);
    img = snapshot(surf, 64u * 64u * 4u);
    memset(&af, 0, sizeof af);
    nv2a_vk_backend_program_stats(&af);
    printf("  [%s] hits %llu -> %llu, tris %llu -> %llu, identity_fail %llu -> %llu, "
           "misses %llu -> %llu, declines %llu -> %llu, const_uploads %llu -> %llu\n", who,
           (unsigned long long)b4.hits, (unsigned long long)af.hits,
           (unsigned long long)b4.tris, (unsigned long long)af.tris,
           (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail,
           (unsigned long long)b4.misses, (unsigned long long)af.misses,
           (unsigned long long)b4.declines, (unsigned long long)af.declines,
           (unsigned long long)b4.const_uploads, (unsigned long long)af.const_uploads);
    if (af.hits < 1 || af.hits <= b4.hits) {
        fail("FAIL: [%s] hits: %llu -> %llu, expected the real table's %08X to be a hit "
             "(>= 1 and advancing)\n", who, (unsigned long long)b4.hits,
             (unsigned long long)af.hits, g_entry->hash);
        ok = 0;
    }
    if (af.hits < (uint64_t)want_hits_min) {
        fail("FAIL: [%s] hits: %llu, expected >= %d\n", who, (unsigned long long)af.hits,
             want_hits_min);
        ok = 0;
    }
    if (af.tris < 1) {
        fail("FAIL: [%s] tris: %llu, expected >= 1\n", who, (unsigned long long)af.tris);
        ok = 0;
    }
    if (af.const_uploads <= b4.const_uploads) {
        fail("FAIL: [%s] const_uploads did not advance: %llu -> %llu\n", who,
             (unsigned long long)b4.const_uploads, (unsigned long long)af.const_uploads);
        ok = 0;
    }
    if (b4.identity_fail != expect_identity_fail) {
        fail("FAIL: [%s] identity_fail is %llu before this control, expected %llu (only the "
             "malformed copies may have moved it)\n", who,
             (unsigned long long)b4.identity_fail, (unsigned long long)expect_identity_fail);
        ok = 0;
    }
    if (af.identity_fail != b4.identity_fail) {
        fail("FAIL: [%s] identity_fail advanced by %llu with the real table restored: %llu -> "
             "%llu\n", who, (unsigned long long)(af.identity_fail - b4.identity_fail),
             (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail);
        ok = 0;
    }
    if (af.first_hit_hash != g_entry->hash) {
        fail("FAIL: [%s] first_hit_hash: %08X, expected %08X\n", who, af.first_hit_hash,
             g_entry->hash);
        ok = 0;
    }
    ok &= check_vs_cpu(who, g_cpu, img, g_perim);
    ok &= table_intact(who);
    free(img);
    return ok;
}

/* ── The malformed copies ────────────────────────────────────────────────── */
static void mut_words_null(Nv2aVshAotEntry *e)    { e->words = NULL; }
static void mut_len_max(Nv2aVshAotEntry *e)       { e->len = 0xFFFFFFFFu; }
static void mut_len_plus_one(Nv2aVshAotEntry *e)  { e->len = e->len + 1u; }
static void mut_len_minus_one(Nv2aVshAotEntry *e) { e->len = e->len - 1u; }
static void mut_spirv_null(Nv2aVshAotEntry *e)    { e->spirv = NULL; }
static void mut_spirv_size_zero(Nv2aVshAotEntry *e) { e->spirv_size = 0; }

typedef struct { const char *name; void (*mutate)(Nv2aVshAotEntry *); } Malformed;

static const Malformed k_cases[] = {
    { "null words",     mut_words_null },
    { "len UINT32_MAX", mut_len_max },
    { "len real+1",     mut_len_plus_one },
    { "len real-1",     mut_len_minus_one },
    { "spirv NULL",     mut_spirv_null },
    { "spirv_size 0",   mut_spirv_size_zero }
};

/* One malformed case: copy the real table, corrupt ONLY the copy, hand the copy to the
 * back end, and draw the program the guest really uploaded. */
static int malformed_case(const Malformed *c, uint32_t surf, uint32_t vb)
{
    Nv2aVkProgramStats b4, af;
    Nv2aVshAotEntry *e;
    uint8_t *img;
    int ok = 1;

    memcpy(g_copy, g_pristine, g_ncopy * sizeof g_copy[0]);
    e = &g_copy[g_idx];
    c->mutate(e);
    ok &= table_intact(c->name);
    nv2a_vk_backend_set_programs(g_copy, g_ncopy);
    memset(&b4, 0, sizeof b4);
    nv2a_vk_backend_program_stats(&b4);
    prefill(surf);
    /* The guest uploads the REAL program: hash, len, mask and digest all match the real
     * table entry, and only the copy the back end holds is malformed. */
    build_batch(surf, vb, g_entry->words, g_entry->len, 1);
    printf("  [%s] the back end holds a copy whose entry %08X is: len %u, words %s, spirv %s, "
           "spirv_size %zu (hash and digest unchanged)\n", c->name, e->hash, e->len,
           e->words ? "set" : "NULL", e->spirv ? "set" : "NULL", e->spirv_size);
    fflush(stdout);
    /* If the production diagnostic digests the rejected entry, the process dies HERE. */
    ok &= submit(c->name);
    img = snapshot(surf, 64u * 64u * 4u);
    memset(&af, 0, sizeof af);
    nv2a_vk_backend_program_stats(&af);
    printf("  [%s] SURVIVED the rejection diagnostic: identity_fail %llu -> %llu, hits %llu -> "
           "%llu, misses %llu -> %llu, declines %llu -> %llu, tris %llu -> %llu, const_uploads "
           "%llu -> %llu\n", c->name,
           (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail,
           (unsigned long long)b4.hits, (unsigned long long)af.hits,
           (unsigned long long)b4.misses, (unsigned long long)af.misses,
           (unsigned long long)b4.declines, (unsigned long long)af.declines,
           (unsigned long long)b4.tris, (unsigned long long)af.tris,
           (unsigned long long)b4.const_uploads, (unsigned long long)af.const_uploads);
    if (af.identity_fail != b4.identity_fail + 1u) {
        fail("FAIL: [%s] identity_fail: %llu -> %llu, expected exactly +1\n", c->name,
             (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail);
        ok = 0;
    }
    if (af.hits != b4.hits) {
        fail("FAIL: [%s] hits advanced by %llu: a malformed entry selected a shader\n", c->name,
             (unsigned long long)(af.hits - b4.hits));
        ok = 0;
    }
    if (af.misses != b4.misses) {
        fail("FAIL: [%s] misses advanced by %llu: the hash IS in the table, so this must be an "
             "identity rejection, not a miss\n", c->name,
             (unsigned long long)(af.misses - b4.misses));
        ok = 0;
    }
    if (af.declines != b4.declines) {
        fail("FAIL: [%s] declines advanced by %llu: the draw was refused before the identity "
             "check, so this case proves nothing\n", c->name,
             (unsigned long long)(af.declines - b4.declines));
        ok = 0;
    }
    if (af.tris != b4.tris) {
        fail("FAIL: [%s] tris advanced by %llu on the program path\n", c->name,
             (unsigned long long)(af.tris - b4.tris));
        ok = 0;
    }
    if (af.const_uploads != b4.const_uploads) {
        fail("FAIL: [%s] const_uploads advanced by %llu on the program path\n", c->name,
             (unsigned long long)(af.const_uploads - b4.const_uploads));
        ok = 0;
    }
    /* The batch must still be in guest memory, drawn by the executor's CPU interpreter. */
    ok &= check_vs_cpu(c->name, g_cpu, img, g_perim);
    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    ok &= table_intact(c->name);
    free(img);
    return ok;
}

int main(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *pbmem;
    char *log;
    size_t i;
    int ok = 1;

    start_stderr_capture();

    pbmem = (uint8_t *)VirtualAlloc(NULL, 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ram || !ramin || !instance || !pbmem) { fail("FAIL: allocation\n"); return 1; }
    g_pb = (uint32_t *)pbmem;

    /* Guest RAM: a synthetic, section-less XBE is enough for the memory layout. */
    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) { fail("FAIL: xbox_MemoryLayoutInit\n"); return 1; }

    nv2a_reset_standalone_for_test();
    g_gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!g_gpu) { fail("FAIL: nv2a_init_standalone\n"); return 1; }
    if (!nv2a_set_pushbuffer_window(g_gpu, pbmem, 0, 0x4000)) { fail("FAIL: pushbuffer window\n"); return 1; }
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) { fail("FAIL: instance memory\n"); return 1; }
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u; pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u; object[1] = object[2] = object[3] = 0;
    }
    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) { fail("FAIL: executor not registered\n"); return 1; }
    nv2a_backend_register(NULL);                       /* CPU pass first: the installer is one-way */

    /* ── The real table, snapshotted before anything can touch it. ── */
    g_ncopy = nv2a_vsh_program_count;
    if (g_ncopy < 1 || g_ncopy > 256) {
        fail("FAIL: the corpus has %zu entries, which the test's copy cannot hold\n", g_ncopy);
        return 1;
    }
    memcpy(g_pristine, nv2a_vsh_programs, g_ncopy * sizeof g_pristine[0]);
    g_entry = table_find(HASH_A);
    if (!g_entry) { fail("FAIL: the corpus does not carry %08X\n", HASH_A); return 1; }
    for (g_idx = 0; g_idx < g_ncopy; g_idx++)
        if (nv2a_vsh_programs[g_idx].hash == HASH_A) break;
    printf("corpus: %zu entries, driving entry %zu hash %08X len %u mask %04X digest %016llX "
           "spirv %zu bytes\n", g_ncopy, g_idx, g_entry->hash, g_entry->len, g_entry->input_mask,
           (unsigned long long)g_entry->digest, g_entry->spirv_size);
    if (!g_entry->words || !g_entry->spirv || g_entry->len == 0 || g_entry->spirv_size == 0) {
        fail("FAIL: the corpus entry %08X is itself malformed, so the cases cannot be built\n",
             g_entry->hash);
        return 1;
    }
    ok &= table_intact("startup");

    /* ── The CPU reference for the A batch, exactly as test_vsh_aot_programs.c captures it. ── */
    puts("-- software executor (reference) --");
    prefill(k_surf[S_REF]);
    build_batch(k_surf[S_REF], k_vb[S_REF], g_entry->words, g_entry->len, 1);
    ok &= submit("cpu reference");
    g_cpu = snapshot(k_surf[S_REF], 64u * 64u * 4u);
    if (!g_cpu) { fail("FAIL: no CPU reference image\n"); return 1; }
    /* After the reference batch, so the interpreter holds the constant file the batch
     * uploads; test_vsh_aot_programs.c measures its perimeters at the same point for the
     * same reason (before it, every c[] register is still zero and the program's positions
     * collapse to a point). */
    g_perim = perim_words(g_entry->words, g_entry->len, g_entry->input_mask);
    printf("triangle perimeter from the interpreter: %.0f px\n", g_perim);
    if (!(g_perim > 1.0)) {
        fail("FAIL: the interpreter's perimeter for %08X is %.1f px, so the batch does not "
             "describe a triangle and no surface comparison would mean anything\n",
             g_entry->hash, g_perim);
        return 1;
    }

    /* ── The Vulkan back end, on the software driver. ── */
    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        /* No skip: this file's whole subject is the backend path, so an unavailable device
         * cannot be reported as a pass, and 77 (CTest: Not Run) is not a pass either. */
        fail("FAIL: vsh_aot_malformed: the Vulkan back end could not be installed on a software "
             "device (no Vulkan, no matching device, or an elevated process, where the loader "
             "ignores VK_DRIVER_FILES). No case could be run, so this is a FAILURE, not a "
             "skip.\n");
        fflush(stdout);
        return 1;
    }
    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    puts("-- Vulkan back end --");

    /* ── Positive control, first: the harness reaches the program path at all. ── */
    puts("-- control 1: the real, unmodified table must hit --");
    prefill(k_surf[S_CTRL1]);
    ok &= positive_control("control 1", k_surf[S_CTRL1], k_vb[S_CTRL1], 1, 0);

    /* ── The malformed copies. ── */
    puts("-- malformed corpus copies (in memory only) --");
    for (i = 0; i < sizeof k_cases / sizeof k_cases[0]; i++) {
        uint32_t surf = k_surf[S_CASE0 + i], vb = k_vb[S_CASE0 + i];
        printf("-- case %zu: %s --\n", i + 1, k_cases[i].name);
        prefill(surf);
        ok &= malformed_case(&k_cases[i], surf, vb);
    }

    /* ── The real table back in place: the malformed copies damaged nothing. ── */
    puts("-- control 2: the real table, restored, must still hit --");
    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    ok &= table_intact("control 2");
    prefill(k_surf[S_CTRL2]);
    ok &= positive_control("control 2", k_surf[S_CTRL2], k_vb[S_CTRL2], 2,
                           (uint64_t)(sizeof k_cases / sizeof k_cases[0]));

    /* ── Supporting evidence: the diagnostic really ran (it is the crash site). ── */
    fflush(stderr);
    log = read_log();
    if (!log) {
        fail("FAIL: the test's stderr log could not be read back from %s\n", g_log_path);
        ok = 0;
    } else {
        static const char *const k_ident_line = "matched hash but not identity";
        if (!strstr(log, k_ident_line)) {
            fail("FAIL: the log has no line containing \"%s\": the identity rejection was "
                 "silent, so the diagnostic did not run\n", k_ident_line);
            ok = 0;
        } else {
            printf("  the log says the hash matched but not the identity (the diagnostic ran and "
                   "the process survived it)\n");
        }
        free(log);
    }

    fflush(stdout);
    nv2a_set_commit_consumer(NULL);
    free(g_cpu);
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fail("FAIL: vsh_aot_malformed\n"); return 1; }
    puts("PASS: vsh_aot_malformed (every malformed corpus entry was cleanly rejected with "
         "identity_fail +1, no hit, and the CPU interpreter drew the batch; the real table was "
         "never touched)");
    return 0;
}
