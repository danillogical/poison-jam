/* The ahead-of-time vertex-program corpus (plan step V5) and the Vulkan back end's
 * vertex-program path, gated end to end.
 *
 * The corpus is the game's generated table (src/recomp/gen/shaders/nv2a_vsh_programs.c,
 * `just shaders`) handed to the back end with nv2a_vk_backend_set_programs. This test is the
 * contract of that seam, not its implementation:
 *
 *   1. Corpus identity, checked against the live interpreter. For every entry: load its
 *      words through nv2a_vsh_set_instruction/nv2a_vsh_set_start_slot, read back hash, len
 *      and input_mask with nv2a_vsh_program_info, and recompute nv2a_vsh_aot_digest. All four
 *      must equal the table's fields, or the table names a program the guest cannot upload.
 *      The table must also be strictly ascending by hash with no duplicate, every len inside
 *      1..136, every mask non-empty, every spirv_size a positive multiple of 4. The measured
 *      distinct count for this title is 126.
 *   2. A real table hit through the GPU, with positive counters. A hand-built pushbuffer
 *      (standalone NV2A, NV097 object on subchannel 0, the real executor on the commit seam)
 *      uploads one corpus program's exact words, draws a triangle, and flips. The Vulkan
 *      program stats must show the hit, the triangles and the constant upload, the latched
 *      first-hit identity must be that program's, and the surface must show the drawing.
 *      The same batch is drawn by the CPU executor first and compared, so "it drew" is not
 *      the only evidence.
 *   3. A miss falls back to the CPU, and is not silent. A hand-written program that is NOT
 *      in the corpus (checked by hash, len and digest against every entry) is drawn: the miss
 *      counter and the first-miss latch must move, the CPU interpreter must still draw the
 *      batch into guest memory, no hit may be counted, and the once-per-hash stderr line must
 *      say the program is not in the corpus.
 *   4. An identity mismatch cannot select a wrong shader. The table is copied in memory and
 *      one entry's len (then its digest) is corrupted while its hash still matches. The draw
 *      must be declined with identity_fail, with no hit, and the CPU interpreter must draw
 *      the batch; the log must say the hash matched but not the identity.
 *   5. Two DIFFERENT corpus programs that share one input_mask must produce their own pixels.
 *      The pipeline cache is keyed by (prog, program hash, input_mask); if it were keyed by
 *      the mask alone the second program would be rasterised with the first one's shader.
 *      Both programs are run through the CPU interpreter first and the test asserts their
 *      outputs differ, so the case is known to be discriminating before the GPU is asked.
 *   6. A negative control (`--negative-control 1|2`) proves the checks above are load-bearing:
 *      it feeds check 1 a deliberately unsorted table with a wrong digest, and check 2 a stats
 *      struct whose hits are forced to 0, and reports the CHECK's own verdict as its exit
 *      status. Exit 1: the check correctly FAILED the simulated defect, which is the expected
 *      result and the evidence that the check is load-bearing. Exit 2: the check PASSED the
 *      defect, so the check does not test what it claims. Nothing is written to disk in that
 *      mode, and the real table is never modified.
 *   7. A fully culled batch records its corpus lookup (plan step R3). vb_draw_program looks
 *      the batch's program up BEFORE the cull FRONT_AND_BACK early return, so a batch nothing
 *      can see is still counted against the corpus: one miss when the corpus lacks its
 *      program (and still no CPU transform -- the culled return wins), or one cull_all and no
 *      fabricated hits/tris/const_uploads when the corpus carries it. The four cases R3.a..
 *      R3.d below drive that real path and assert EXACT per-batch deltas (misses +1, cull_all
 *      +1, declines/identity_fail unchanged, hits/tris/const_uploads unchanged), plus a
 *      byte-exact sentinel surface for the culled batches, because only an exact delta
 *      separates the new ordering from the old one.
 *
 * Every counter assertion reads nv2a_vk_backend_program_stats or the surface bytes. The only
 * log-line assertions are requirement 3's "the miss is not silent" and requirement 4's
 * identity-failure line, and both are substring matches against this process's own captured
 * stderr.
 *
 * The test selects the CPU device with RECOMP_VK_DEVICE=llvmpipe (CTest also sets it, with
 * VK_DRIVER_FILES from the cache variable XBR_VK_SW_ICD). It exits 77 -- a skip, never a pass
 * -- only when the Vulkan back end cannot be installed, notably in an elevated process where the
 * loader ignores VK_DRIVER_FILES, or through the explicit `--skip <reason>` no-ICD fallback,
 * which runs no check at all. A skip may mean only "the device is unavailable": if check 1 or
 * the CPU reference pass has already failed, the run FAILS (exit 1) instead of skipping, because
 * CTest counts 77 as Not Run and a skip would leave a broken corpus green. Run it non-elevated.
 *
 * RECOMP_GPU_VSH is deliberately NOT set: =0 makes the executor decline the program path
 * before it ever calls draw_program, which would turn every expected miss into a decline.
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
 * The miss/identity log assertions need this process's own stderr, so stderr is redirected to
 * a file for the whole run. `fail` writes to the original descriptor too, so a failure is
 * still visible where the test is being watched. */
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
/* Attributes 0..3 as above. `mask` decides which of them the batch needs. */
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

/* The constant file the corpus programs are driven with. Measured against the real interpreter:
 * c[58]/c[59] are the viewport scale/offset the D3D epilogue reads, c[96]/c[97] are the two
 * constants the title's own programs load per draw, and c[98..110] are the four 4x4 matrices
 * (position, texture, and the two lighting matrices) as scaled identities, so every corpus
 * program that reads them gets a bounded, finite result. */
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
    /* c[118] and c[150]: read by a relative-indexed corpus program and by the miss program. */
    pb_constant(118, 0.5f, 0.5f, 0.5f, 1.0f);
    pb_constant(150, 1.0f, 0.5f, 0.25f, 1.0f);
}

/* The register combiners, programmed once: the executor and the back end both need rc_seen set
 * before the vertex-program path is offered to a back end at all (backend_rc_active). Stage 0
 * outputs the interpolated diffuse (v0), the final stage is off, so a pixel is the vertex
 * colour -- which keeps check 5 a pure geometry test. */
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

/* The batch's three vertices, as the test's probe measured them through the interpreter. */
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

/* 1 when every byte of the surface is still the prefill sentinel, i.e. the batch wrote
 * nothing at all: no clear, no GPU draw and no CPU interpreter draw. A fully culled batch
 * must leave the whole 64 KiB that way -- and this is not vacuous, because the positive
 * control next to it draws the very same program into a cleared surface. */
static int surface_is_sentinel(uint32_t va, const char *who)
{
    const uint8_t *p = G(va);
    uint32_t i;
    for (i = 0; i < SURF_BYTES; i++) {
        if (p[i] != SENTINEL) {
            fail("FAIL: [%s] the surface was written: byte %u is 0x%02X, the prefill sentinel "
                 "is 0x%02X\n", who, (unsigned)i, p[i], SENTINEL);
            return 0;
        }
    }
    printf("  [%s] the surface is untouched: all %u bytes are the sentinel 0x%02X\n",
           who, (unsigned)SURF_BYTES, SENTINEL);
    return 1;
}

/* ── Surface comparison ──────────────────────────────────────────────────── */
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
    printf("  %-28s draws+%u clears+%u tris+%u flips_stall+%u unhandled+%u\n",
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

/* One batch: state, combiner, surface, optional clear, program and constants, one triangle,
 * optional flip. `cull_front_back` writes the pair the executor turns into cull mode 3
 * (SET_CULL_FACE_ENABLE 1 + SET_CULL_FACE FRONT_AND_BACK 0x408) after the reset, and `clear`
 * decides whether the colour surface is cleared: the R3 cases need a batch that draws nothing
 * at all, so that a prefilled surface is untouched only when nothing was written. */
static void build_batch_flags(uint32_t surf, uint32_t vb, const uint32_t (*words)[4], uint32_t len,
                              int flip, int cull_front_back, int clear)
{
    pb_begin();
    pb_reset_state();
    if (cull_front_back) {
        pb(0x0308, 1u);                            /* SET_CULL_FACE_ENABLE */
        pb(0x039C, 0x408u);                        /* SET_CULL_FACE FRONT_AND_BACK */
    }
    pb_combiner();
    pb_surface(surf, 64, 64);
    if (clear) pb_clear(CLR);
    pb_constants();
    pb_program(words, len);
    pb_vertex_format(vb);
    write_vertices(vb);
    pb_draw_triangles(3);
    if (flip) pb_flip();
}

/* The original shape every earlier case uses: no culling, and the surface cleared. */
static void build_batch(uint32_t surf, uint32_t vb, const uint32_t (*words)[4], uint32_t len,
                        int flip)
{
    build_batch_flags(surf, vb, words, len, flip, 0, 1);
}

/* ── 1. Corpus identity, against the live interpreter ────────────────────── */
static unsigned popcount32(uint32_t v) { unsigned n = 0; while (v) { n += v & 1u; v >>= 1; } return n; }

/* Returns 1 when the table is self-consistent. `tab`/`n` are the corpus under test, so the
 * negative control can hand it a deliberately corrupt copy. */
static int check_corpus_identity(const Nv2aVshAotEntry *tab, size_t n, int verbose)
{
    size_t i;
    int ok = 1;
    uint32_t bad_asc = 0, bad_dup = 0, bad_len = 0, bad_mask = 0, bad_spv = 0;
    uint32_t bad_hash = 0, bad_mask_field = 0, bad_digest = 0, bad_info = 0;

    printf("  corpus count = %zu\n", n);
    if (n != 126) {
        fail("FAIL: corpus count: table has %zu entries, the measured distinct corpus of this "
             "title is 126\n", n);
        ok = 0;
    }
    if (n < 126) {
        fail("FAIL: corpus count: at least 126 entries are required, the table has %zu\n", n);
        ok = 0;
    }
    for (i = 0; i < n; i++) {
        const Nv2aVshAotEntry *e = &tab[i];
        const uint32_t (*words)[4] = NULL;
        uint32_t len = 0, hash = 0, mask = 0;
        int wc = 0;
        uint32_t k;

        if (i && tab[i - 1].hash >= e->hash) {
            if (i && tab[i - 1].hash == e->hash) {
                bad_dup++;
                fail("FAIL: corpus identity: duplicate hash %08X at entries %zu and %zu\n",
                     e->hash, i - 1, i);
            } else {
                bad_asc++;
                fail("FAIL: corpus identity: entry %zu has hash %08X, below its predecessor's "
                     "%08X (the back end binary-searches this table)\n", i, e->hash, tab[i - 1].hash);
            }
            ok = 0;
        }
        if (e->len < 1 || e->len > NV2A_VSH_SLOTS) {
            bad_len++;
            fail("FAIL: corpus identity: %08X has len %u, outside 1..%u\n", e->hash, e->len,
                 (unsigned)NV2A_VSH_SLOTS);
            ok = 0;
        }
        if (popcount32(e->input_mask) < 1) {
            bad_mask++;
            fail("FAIL: corpus identity: %08X has input_mask 0x%04X, which reads no attribute\n",
                 e->hash, e->input_mask);
            ok = 0;
        }
        if (e->spirv_size == 0 || e->spirv_size % 4u != 0) {
            bad_spv++;
            fail("FAIL: corpus identity: %08X has spirv_size %zu, which is not a positive "
                 "multiple of 4\n", e->hash, e->spirv_size);
            ok = 0;
        }
        if (!e->words || !e->spirv) {
            fail("FAIL: corpus identity: %08X has a null words or spirv pointer\n", e->hash);
            ok = 0;
            continue;
        }
        /* Recompute the identity through the executor's own decoder: this is the check that the
         * table names a program the guest can actually upload. */
        for (k = 0; k < NV2A_VSH_SLOTS; k++) {
            const uint32_t zero[4] = { 0, 0, 0, 0 };
            nv2a_vsh_set_instruction(k, zero);
        }
        for (k = 0; k < e->len && k < NV2A_VSH_SLOTS; k++)
            nv2a_vsh_set_instruction(k, e->words[k]);
        nv2a_vsh_set_start_slot(0);
        if (!nv2a_vsh_program_info(&words, &len, &hash, &mask, &wc)) {
            bad_info++;
            fail("FAIL: corpus identity: %08X does not decode to a FINAL through "
                 "nv2a_vsh_program_info\n", e->hash);
            ok = 0;
            continue;
        }
        if (hash != e->hash) {
            bad_hash++;
            fail("FAIL: corpus identity: %08X decodes to hash %08X\n", e->hash, hash);
            ok = 0;
        }
        if (len != e->len) {
            bad_len++;
            fail("FAIL: corpus identity: %08X decodes to len %u, the table says %u\n",
                 e->hash, len, e->len);
            ok = 0;
        }
        if (mask != e->input_mask) {
            bad_mask_field++;
            fail("FAIL: corpus identity: %08X decodes to input_mask 0x%04X, the table says "
                 "0x%04X\n", e->hash, mask, e->input_mask);
            ok = 0;
        }
        if (nv2a_vsh_aot_digest(e->words, e->len) != e->digest) {
            bad_digest++;
            fail("FAIL: corpus identity: %08X digest is %016llX, recomputed %016llX\n",
                 e->hash, (unsigned long long)e->digest,
                 (unsigned long long)nv2a_vsh_aot_digest(e->words, e->len));
            ok = 0;
        }
        if (verbose && (i % 25u) == 0u)
            printf("    [%3zu] %08X len %3u mask %04X digest %016llX spirv %zu bytes\n",
                   i, e->hash, e->len, e->input_mask, (unsigned long long)e->digest, e->spirv_size);
    }
    printf("  corpus identity: %zu entries, %u unsorted, %u duplicate, %u bad len, %u bad mask, "
           "%u bad spirv, %u bad hash, %u bad mask field, %u bad digest, %u undecodable\n",
           n, bad_asc, bad_dup, bad_len, bad_mask, bad_spv, bad_hash, bad_mask_field, bad_digest,
           bad_info);
    return ok;
}

/* The GPU image must agree with the CPU reference for the same batch. The rule is the parity
 * test's (test_gpu_backend_parity_vk.c, check_parity), because the two rasterisers sample a
 * shared triangle edge differently by a pixel or so and that is a boundary effect, not a
 * disagreement about what the program computes:
 *   - at least 98% of all pixels within 8 per channel;
 *   - a drawn-region mismatch only within the perimeter band, i.e. 2% of the region plus the
 *     triangle's perimeter (a pixel that one side drew and the other did not IS a mismatch
 *     unless it lies on the boundary);
 *   - coverage differing by at most the perimeter;
 *   - at least 500 pixels that are not the clear colour, which matters on its own: a surface
 *     nothing drew would otherwise "agree" with a CPU reference that also drew nothing.
 * Shared by checks 2, 3, 4 and BOTH halves of 5, so no program gets a weaker comparison than
 * its neighbour. */
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
        fail("FAIL: [%s] only %u pixels differ from the clear colour, the draw did not land\n",
             who, drawn);
        ok = 0;
    }
    return ok;
}

/* ── 2. The hit predicate (separately callable, so the negative control can feed it) ── */
static int check_hit_stats(const Nv2aVkProgramStats *st, uint32_t hash, const char *who)
{
    int ok = 1;
    if (st->hits < 1) {
        fail("FAIL: [%s] hits: %llu, expected >= 1 (the corpus carries %08X)\n",
             who, (unsigned long long)st->hits, hash);
        ok = 0;
    }
    if (st->tris < 1) {
        fail("FAIL: [%s] tris: %llu, expected >= 1\n", who, (unsigned long long)st->tris);
        ok = 0;
    }
    if (st->const_uploads < 1) {
        fail("FAIL: [%s] const_uploads: %llu, expected >= 1 (the 192-float4 block per draw)\n",
             who, (unsigned long long)st->const_uploads);
        ok = 0;
    }
    if (st->first_hit_hash != hash) {
        fail("FAIL: [%s] first_hit_hash: %08X, expected %08X\n", who, st->first_hit_hash, hash);
        ok = 0;
    }
    if (st->first_hit_tris < 1) {
        fail("FAIL: [%s] first_hit_tris: %u, expected >= 1\n", who, st->first_hit_tris);
        ok = 0;
    }
    printf("  [%s] hits=%llu misses=%llu identity_fail=%llu declines=%llu tris=%llu "
           "const_uploads=%llu first_hit=%08X len %u mask %04X tris %u\n",
           who, (unsigned long long)st->hits, (unsigned long long)st->misses,
           (unsigned long long)st->identity_fail, (unsigned long long)st->declines,
           (unsigned long long)st->tris, (unsigned long long)st->const_uploads,
           st->first_hit_hash, st->first_hit_len, st->first_hit_mask, st->first_hit_tris);
    return ok;
}

/* ── The corpus entries this test drives ─────────────────────────────────── */
static const Nv2aVshAotEntry *table_find(uint32_t hash)
{
    size_t i;
    for (i = 0; i < nv2a_vsh_program_count; i++)
        if (nv2a_vsh_programs[i].hash == hash) return &nv2a_vsh_programs[i];
    return NULL;
}

/* Two mask-0007 programs that both draw a real triangle under the constant file above, and
 * that the interpreter puts in DIFFERENT places: the position matrices live in different
 * constant slots. This pair is what check 5 needs. */
#define HASH_A 0xEC6C788Bu
#define HASH_B 0xF2833A7Du

/* Run one entry through the interpreter directly, for the pre-flight that says the pair is
 * discriminating. Returns 1 when the program ran to FINAL. */
static int interp_entry(const Nv2aVshAotEntry *e, Nv2aVshOutput out[3], double *area)
{
    uint32_t k, i;
    int v, b;
    for (k = 0; k < NV2A_VSH_SLOTS; k++) {
        const uint32_t zero[4] = { 0, 0, 0, 0 };
        nv2a_vsh_set_instruction(k, zero);
    }
    for (k = 0; k < e->len; k++) nv2a_vsh_set_instruction(k, e->words[k]);
    nv2a_vsh_set_start_slot(0);
    for (v = 0; v < 3; v++) {
        float in[NV2A_VSH_INPUTS][4];
        memset(in, 0, sizeof in);
        for (i = 0; i < NV2A_VSH_INPUTS; i++) in[i][3] = 1.0f;
        for (b = 0; b < 4; b++)
            if ((e->input_mask >> b) & 1u) memcpy(in[b], k_vert[v][b], 16);
        if (!nv2a_vsh_run((const float (*)[4])in, &out[v])) return 0;
    }
    if (area) {
        double ax = out[1].pos[0] - out[0].pos[0], ay = out[1].pos[1] - out[0].pos[1];
        double bx = out[2].pos[0] - out[0].pos[0], by = out[2].pos[1] - out[0].pos[1];
        double d = ax * by - bx * ay;
        *area = (d < 0 ? -d : d) * 0.5;
    }
    return 1;
}

/* The triangle's perimeter in pixels, from the interpreter's own positions for the test batch:
 * the edge band the GPU-vs-CPU comparison allows, as the parity test does. */
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

/* The miss program: mov oPos, v0  ;  mov oD0, c[150]  FINAL.  mask 0x0001, two instructions. */
static const uint32_t k_miss_words[2][4] = {
    { 0x00000000u, 0x0020001bu, 0x08000000u, 0x0000f800u },
    { 0x00000000u, 0x0032c01bu, 0x0c000000u, 0x0000f819u }
};
/* The interpreter's identity for it, measured: hash 80BC1DAA, len 2, mask 0001,
 * digest 82C7201E7E0D396A. Recomputed here rather than trusted. */
#define MISS_HASH 0x80BC1DAAu

/* ── stderr capture ──────────────────────────────────────────────────────── */
static char g_log_path[512];

static int start_stderr_capture(void)
{
    const char *tmp = getenv("TEMP");
    if (!tmp || !tmp[0]) tmp = ".";
    _snprintf(g_log_path, sizeof g_log_path, "%s\\pv04_vsh_aot_stderr.log", tmp);
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
static int log_has(const char *text, const char *needle)
{
    return text && strstr(text, needle) != NULL;
}

/* The back end exposes `cull_all` (and its other whole-back-end counters) only on its
 * "[GPUVK] ..." counters line: nv2a_vk_backend_program_stats has no field for it. vb_stats
 * prints that line on a flip for the process's FIRST flip and then at most once every 10 s,
 * so the R3 cases read it twice: once on the first flip (forced) and once after waiting the
 * window out (see run_r3_cull_cases). Returns 1 and stores the value of `field` when a
 * counters line beyond the `*seen` already read is present; `*seen` then counts them.
 * Reads THIS process's own captured stderr, so a loader line without the field cannot be
 * mistaken for one. */
static int gpu_stats_field(const char *field, unsigned *seen, uint64_t *out)
{
    static const char k_prefix[] = "[GPUVK]";
    char needle[64], *log, *p;
    uint64_t vals[64];
    unsigned n = 0;
    int ok = 0;

    _snprintf(needle, sizeof needle, "%s=", field);
    log = read_log();
    if (!log) return 0;
    for (p = strstr(log, k_prefix); p && n < 64u; p = strstr(p + 1, k_prefix)) {
        char *nl = strchr(p, '\n');
        char *f = strstr(p, needle);
        size_t span = nl ? (size_t)(nl - p) : strlen(p);
        if (f && (size_t)(f - p) < span)
            vals[n++] = _strtoui64(f + strlen(needle), NULL, 10);
    }
    free(log);
    if (n > *seen) { *seen = n; *out = vals[n - 1u]; ok = 1; }
    return ok;
}

/* ── The run ─────────────────────────────────────────────────────────────── */
/* One surface (and one vertex buffer) per batch the test draws, so no case can read
 * another's pixels. The last three are the R3 cases': a fully culled batch must leave
 * its surface byte-for-byte as it was prefilled. */
#define NSURF 7
static uint8_t *g_cpu[NSURF];
static const uint32_t k_surf[NSURF] = { SURF(0), SURF(1), SURF(2), SURF(3), SURF(4), SURF(5), SURF(6) };
static const uint32_t k_vb[NSURF] = { VBUF(0), VBUF(1), VBUF(2), VBUF(3), VBUF(4), VBUF(5), VBUF(6) };
enum { C_HIT_A, C_HIT_B, C_MISS, C_IDENT, C_R3CULL, C_R3CULLHIT, C_R3VIS };

/* ── 7. A fully culled batch still records its corpus lookup (R3) ───────────
 * vb_draw_program looks a batch's program up BEFORE the cull FRONT_AND_BACK early
 * return, so a batch nothing can see is still counted against the corpus: a miss when
 * the corpus lacks its program (the culled return still wins -- neither the GPU nor
 * the CPU draws it) and cull_all with no fabricated hits/tris/const_uploads when the
 * corpus carries it. These cases drive that real path -- no second lookup, no cull
 * decision of their own -- and assert EXACT per-batch counter deltas, because only an
 * exact delta separates the two orderings: with the lookup after the cull return the
 * first case's `misses` delta is 0, so a "<= 1" or "not more than" check would pass on
 * both. `declines` and `identity_fail` are asserted UNCHANGED as well, because a batch
 * the back end never offers shows a 0 delta there too, and a hash collision would show
 * up as identity_fail rather than as a miss.
 *
 * The three cases:
 *   R3.a  the corpus program R3.d culls, drawn visibly: the control that the corpus is
 *         installed and that program really is a hit (hits +1, first_hit unchanged in
 *         identity).
 *   R3.b  cull FRONT_AND_BACK + a program the corpus does not carry: misses +1 exactly,
 *         cull_all +1 exactly, nothing drawn, surface untouched.
 *   R3.c  the SAME absent program, culling not-all: misses +1 exactly and the CPU
 *         interpreter draws it (pixels match the CPU reference), i.e. a visible miss
 *         still returns 0.
 *   R3.d  a program the corpus CARRIES, fully culled: cull_all +1 exactly, no hit, no
 *         triangle, no constant upload, surface untouched.
 *
 * cull_all is read from the "[GPUVK]" counters line (see gpu_stats_field). R3.b's flip
 * is the process's first flip, so its line is forced; R3.d's flip is preceded by a wait
 * of the 10 s stats window at the end of the sequence. The line is cumulative, so the
 * two readings bracket R3.c as well: cull_all 0 -> 1 over R3.b, and 1 -> 2 over R3.d
 * with R3.c in between proving the visible miss adds nothing.
 */
static int run_r3_cull_cases(void)
{
    const Nv2aVshAotEntry *ea = table_find(HASH_A);
    Nv2aVkProgramStats b, a;
    unsigned lines = 0;
    uint64_t cull_all = 0;
    double pm;
    int ok = 1;

    if (!ea) { fail("FAIL: [r3] %08X is not in the table\n", HASH_A); return 0; }
    pm = perim_words(k_miss_words, 2, 0x0001u);

    /* ── R3.a: the corpus program R3.d culls, drawn visibly first. ── */
    puts("-- R3.a: the corpus program the culled batch carries, drawn visibly (control) --");
    nv2a_vk_backend_program_stats(&b);
    build_batch_flags(k_surf[C_HIT_A], k_vb[C_HIT_A], ea->words, ea->len, 0, 0, 1);
    ok &= submit("r3 visible corpus hit");
    nv2a_vk_backend_program_stats(&a);
    printf("  [r3 visible hit] hits %llu -> %llu, tris %llu -> %llu, const_uploads %llu -> %llu, "
           "first_hit_hash %08X\n",
           (unsigned long long)b.hits, (unsigned long long)a.hits,
           (unsigned long long)b.tris, (unsigned long long)a.tris,
           (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads,
           a.first_hit_hash);
    if (a.hits != b.hits + 1) {
        fail("FAIL: [r3 visible hit] hits advanced by %llu, expected exactly 1: the corpus does "
             "not carry %08X, so the fully culled case below would prove nothing\n",
             (unsigned long long)(a.hits - b.hits), ea->hash);
        ok = 0;
    }
    if (a.tris != b.tris + 1 || a.const_uploads != b.const_uploads + 1) {
        fail("FAIL: [r3 visible hit] tris %llu -> %llu and const_uploads %llu -> %llu, expected "
             "exactly one triangle and one constant block\n",
             (unsigned long long)b.tris, (unsigned long long)a.tris,
             (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads);
        ok = 0;
    }
    if (a.first_hit_hash != ea->hash) {
        fail("FAIL: [r3 visible hit] first_hit_hash %08X, expected %08X\n",
             a.first_hit_hash, ea->hash);
        ok = 0;
    }

    /* ── R3.b: cull FRONT_AND_BACK + a program the corpus does not carry. ── */
    puts("-- R3.b: cull FRONT_AND_BACK + a program the corpus does not carry --");
    nv2a_vk_backend_program_stats(&b);
    build_batch_flags(k_surf[C_R3CULL], k_vb[C_R3CULL], k_miss_words, 2, 1, 1, 0);
    ok &= submit("r3 cull-all + absent program");
    nv2a_vk_backend_program_stats(&a);
    if (!gpu_stats_field("cull_all", &lines, &cull_all)) cull_all = 0;
    printf("  [r3 cull-all miss] misses %llu -> %llu, cull_all (counters line) %llu, "
           "hits %llu -> %llu, tris %llu -> %llu, const_uploads %llu -> %llu, "
           "declines %llu -> %llu, identity_fail %llu -> %llu, first_miss %08X len %u mask %04X\n",
           (unsigned long long)b.misses, (unsigned long long)a.misses, (unsigned long long)cull_all,
           (unsigned long long)b.hits, (unsigned long long)a.hits,
           (unsigned long long)b.tris, (unsigned long long)a.tris,
           (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads,
           (unsigned long long)b.declines, (unsigned long long)a.declines,
           (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail,
           a.first_miss_hash, a.first_miss_len, a.first_miss_mask);
    if (a.misses != b.misses + 1) {
        fail("FAIL: [r3 cull-all miss] misses advanced by %llu, expected exactly 1: a fully "
             "culled batch must still record that the corpus does not carry its program\n",
             (unsigned long long)(a.misses - b.misses));
        ok = 0;
    }
    if (a.hits != b.hits || a.tris != b.tris || a.const_uploads != b.const_uploads) {
        fail("FAIL: [r3 cull-all miss] a culled batch drew something: hits %llu -> %llu, tris "
             "%llu -> %llu, const_uploads %llu -> %llu\n",
             (unsigned long long)b.hits, (unsigned long long)a.hits,
             (unsigned long long)b.tris, (unsigned long long)a.tris,
             (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads);
        ok = 0;
    }
    if (a.declines != b.declines) {
        fail("FAIL: [r3 cull-all miss] declines advanced by %llu for a program the corpus does "
             "not carry, which must be a miss and nothing else\n",
             (unsigned long long)(a.declines - b.declines));
        ok = 0;
    }
    if (a.identity_fail != b.identity_fail) {
        fail("FAIL: [r3 cull-all miss] identity_fail advanced by %llu\n",
             (unsigned long long)(a.identity_fail - b.identity_fail));
        ok = 0;
    }
    if (a.first_miss_hash != MISS_HASH) {
        fail("FAIL: [r3 cull-all miss] first_miss_hash %08X, expected %08X\n",
             a.first_miss_hash, MISS_HASH);
        ok = 0;
    }
    ok &= surface_is_sentinel(k_surf[C_R3CULL], "r3 cull-all miss");
    if (!lines) {
        fail("FAIL: [r3 cull-all miss] the back end printed no [GPUVK] counters line after the "
             "batch, so cull_all cannot be read\n");
        ok = 0;
    } else if (cull_all != 1) {
        fail("FAIL: [r3 cull-all miss] cull_all %llu, expected exactly 1 after one cull-all "
             "batch\n", (unsigned long long)cull_all);
        ok = 0;
    } else {
        printf("  [r3 cull-all miss] cull_all is exactly 1 on the counters line\n");
    }

    /* ── R3.c: the same absent program, culling not-all (the positive control). ──
     * Built exactly like check 3's miss batch: cleared, and flipped, because the back end
     * still owns the clear and only a flip writes it back to guest memory -- so the read-back
     * surface is the clear plus the CPU interpreter's triangle, which is what the CPU
     * reference holds too. */
    puts("-- R3.c: the same absent program, culling not-all (positive control) --");
    nv2a_vk_backend_program_stats(&b);
    build_batch_flags(k_surf[C_R3VIS], k_vb[C_R3VIS], k_miss_words, 2, 1, 0, 1);
    ok &= submit("r3 visible miss");
    nv2a_vk_backend_program_stats(&a);
    printf("  [r3 visible miss] misses %llu -> %llu, hits %llu -> %llu, declines %llu -> %llu, "
           "identity_fail %llu -> %llu\n",
           (unsigned long long)b.misses, (unsigned long long)a.misses,
           (unsigned long long)b.hits, (unsigned long long)a.hits,
           (unsigned long long)b.declines, (unsigned long long)a.declines,
           (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail);
    if (a.misses != b.misses + 1) {
        fail("FAIL: [r3 visible miss] misses advanced by %llu, expected exactly 1\n",
             (unsigned long long)(a.misses - b.misses));
        ok = 0;
    }
    if (a.hits != b.hits || a.tris != b.tris || a.const_uploads != b.const_uploads) {
        fail("FAIL: [r3 visible miss] hits/tris/const_uploads advanced by %llu/%llu/%llu for a "
             "program the corpus does not carry\n",
             (unsigned long long)(a.hits - b.hits), (unsigned long long)(a.tris - b.tris),
             (unsigned long long)(a.const_uploads - b.const_uploads));
        ok = 0;
    }
    if (a.declines != b.declines || a.identity_fail != b.identity_fail) {
        fail("FAIL: [r3 visible miss] declines %llu -> %llu, identity_fail %llu -> %llu: a "
             "visible miss is a miss and nothing else\n",
             (unsigned long long)b.declines, (unsigned long long)a.declines,
             (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail);
        ok = 0;
    }
    {
        uint8_t *im = snapshot(k_surf[C_R3VIS], 64u * 64u * 4u);
        ok &= check_vs_cpu("r3 visible miss", g_cpu[C_R3VIS], im, pm);
        free(im);
    }

    /* ── R3.d: a program the corpus carries, fully culled. ── */
    puts("-- R3.d: a program the corpus carries, fully culled --");
    nv2a_vk_backend_program_stats(&b);
    build_batch_flags(k_surf[C_R3CULLHIT], k_vb[C_R3CULLHIT], ea->words, ea->len, 0, 1, 0);
    ok &= submit("r3 cull-all + corpus program");
    /* vb_stats prints at most once every 10 s: wait that window out and flip once more, so
     * the counters line carries this batch's cull_all too. */
    Sleep(10300);
    pb_begin(); pb_flip();
    ok &= submit("r3 cull-all + corpus program (counter flush)");
    nv2a_vk_backend_program_stats(&a);
    if (!gpu_stats_field("cull_all", &lines, &cull_all)) cull_all = 0;
    printf("  [r3 cull-all carried] cull_all (counters line) %llu, misses %llu -> %llu, "
           "hits %llu -> %llu, tris %llu -> %llu, const_uploads %llu -> %llu, "
           "declines %llu -> %llu, identity_fail %llu -> %llu\n",
           (unsigned long long)cull_all,
           (unsigned long long)b.misses, (unsigned long long)a.misses,
           (unsigned long long)b.hits, (unsigned long long)a.hits,
           (unsigned long long)b.tris, (unsigned long long)a.tris,
           (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads,
           (unsigned long long)b.declines, (unsigned long long)a.declines,
           (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail);
    if (a.hits != b.hits || a.tris != b.tris || a.const_uploads != b.const_uploads) {
        fail("FAIL: [r3 cull-all carried] a lookup match without a draw fabricated GPU work: "
             "hits %llu -> %llu, tris %llu -> %llu, const_uploads %llu -> %llu\n",
             (unsigned long long)b.hits, (unsigned long long)a.hits,
             (unsigned long long)b.tris, (unsigned long long)a.tris,
             (unsigned long long)b.const_uploads, (unsigned long long)a.const_uploads);
        ok = 0;
    }
    if (a.misses != b.misses || a.declines != b.declines || a.identity_fail != b.identity_fail) {
        fail("FAIL: [r3 cull-all carried] misses %llu -> %llu, declines %llu -> %llu, "
             "identity_fail %llu -> %llu for a program the corpus carries\n",
             (unsigned long long)b.misses, (unsigned long long)a.misses,
             (unsigned long long)b.declines, (unsigned long long)a.declines,
             (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail);
        ok = 0;
    }
    ok &= surface_is_sentinel(k_surf[C_R3CULLHIT], "r3 cull-all carried");
    if (cull_all != 2) {
        fail("FAIL: [r3 cull-all carried] cull_all %llu, expected exactly 2 (R3.b's cull-all "
             "batch, R3.c's visible miss adding nothing, and this one)\n",
             (unsigned long long)cull_all);
        ok = 0;
    } else {
        printf("  [r3 cull-all carried] cull_all is exactly 2: the cull-all batches each added 1 "
               "and the visible miss added none\n");
    }
    return ok;
}

static int run_gpu_pass(void)
{
    int ok = 1, z;
    uint8_t *img[NSURF];
    Nv2aVkProgramStats before, after;
    const Nv2aVshAotEntry *ea = table_find(HASH_A), *eb = table_find(HASH_B);
    static Nv2aVshAotEntry copy[256];
    size_t ncopy = nv2a_vsh_program_count;

    double perim[3];
    for (z = 0; z < NSURF; z++) img[z] = NULL;
    perim[0] = ea ? perim_words(ea->words, ea->len, ea->input_mask) : 0.0;
    perim[1] = eb ? perim_words(eb->words, eb->len, eb->input_mask) : 0.0;
    perim[2] = perim_words(k_miss_words, 2, 0x0001u);
    printf("  triangle perimeters from the interpreter: A %.0f px, B %.0f px, miss %.0f px\n",
           perim[0], perim[1], perim[2]);

    /* ── 7. The R3 cases come first on purpose: R3.b's cull-all batch owns the process's
     * first flip, so the back end's counters line that carries cull_all is forced at that
     * moment instead of being 10 s late. Nothing before them flips (the CPU pass has no
     * back end registered at all). ── */
    ok &= run_r3_cull_cases();

    /* ── 2. A real table hit. ── */
    puts("-- check 2: a corpus program drawn through the Vulkan program path --");
    memset(&before, 0, sizeof before);
    nv2a_vk_backend_program_stats(&before);
    build_batch(k_surf[C_HIT_A], k_vb[C_HIT_A], ea->words, ea->len, 1);
    ok &= submit("vulkan hit A");
    nv2a_vk_backend_program_stats(&after);
    ok &= check_hit_stats(&after, ea->hash, "hit A");
    img[C_HIT_A] = snapshot(k_surf[C_HIT_A], 64u * 64u * 4u);
    ok &= check_vs_cpu("hit A", g_cpu[C_HIT_A], img[C_HIT_A], perim[0]);
    if (after.hits <= before.hits) {
        fail("FAIL: [hit A] hits did not advance: %llu -> %llu\n",
             (unsigned long long)before.hits, (unsigned long long)after.hits);
        ok = 0;
    }

    /* ── 5. Two programs sharing one mask must keep their own shaders. ── */
    puts("-- check 5: two corpus programs with the same input_mask --");
    {
        Nv2aVshOutput oa[3], ob[3];
        double aa = 0, ab = 0;
        int d = 0, v, k;
        if (!ea || !eb) {
            fail("FAIL: [pair] the corpus does not carry %08X and %08X\n", HASH_A, HASH_B);
            return 0;
        }
        if (ea->input_mask != eb->input_mask) {
            fail("FAIL: [pair] %08X has mask %04X but %08X has %04X: this case needs two "
                 "programs that SHARE a mask\n", ea->hash, ea->input_mask, eb->hash,
                 eb->input_mask);
            return 0;
        }
        if (!interp_entry(ea, oa, &aa) || !interp_entry(eb, ob, &ab)) {
            fail("FAIL: [pair] a program of the pair did not run to FINAL in the interpreter\n");
            return 0;
        }
        for (v = 0; v < 3; v++) {
            for (k = 0; k < 4; k++) {
                if (oa[v].pos[k] != ob[v].pos[k]) d = 1;
                if (oa[v].d0[k] != ob[v].d0[k]) d = 1;
            }
            printf("    cpu %08X v%d pos %8.3f %8.3f %6.3f %6.3f d0 %6.3f %6.3f %6.3f %6.3f\n",
                   ea->hash, v, oa[v].pos[0], oa[v].pos[1], oa[v].pos[2], oa[v].pos[3],
                   oa[v].d0[0], oa[v].d0[1], oa[v].d0[2], oa[v].d0[3]);
        }
        for (v = 0; v < 3; v++)
            printf("    cpu %08X v%d pos %8.3f %8.3f %6.3f %6.3f d0 %6.3f %6.3f %6.3f %6.3f\n",
                   eb->hash, v, ob[v].pos[0], ob[v].pos[1], ob[v].pos[2], ob[v].pos[3],
                   ob[v].d0[0], ob[v].d0[1], ob[v].d0[2], ob[v].d0[3]);
        printf("    cpu areas %.1f and %.1f (mask %04X on both)\n", aa, ab, ea->input_mask);
        if (!d) {
            fail("FAIL: [pair] %08X and %08X produce identical interpreter output for this "
                 "batch, so a differing GPU result would prove nothing: pick another pair\n",
                 ea->hash, eb->hash);
            ok = 0;
        } else {
            Cmp cp = compare(g_cpu[C_HIT_A], g_cpu[C_HIT_B], 64, 64);
            printf("    cpu references differ in %u of 4096 pixels (coverage %u vs %u)\n",
                   4096u - cp.match, cp.cov_ref, cp.cov_got);
            if (cp.match == 4096u) {
                fail("FAIL: [pair] the CPU references are identical, so the case is not "
                     "discriminating\n");
                ok = 0;
            }
        }
        /* Now the GPU: B is drawn in the same process, with the same render state and the same
         * combiner as A. Each image is compared with its OWN CPU reference first -- that is what
         * catches a table whose entry for B carries another program's shader, which would still
         * differ from A and so pass the difference assertion below on its own. */
        build_batch(k_surf[C_HIT_B], k_vb[C_HIT_B], eb->words, eb->len, 1);
        ok &= submit("vulkan hit B");
        img[C_HIT_B] = snapshot(k_surf[C_HIT_B], 64u * 64u * 4u);
        ok &= check_vs_cpu("hit B", g_cpu[C_HIT_B], img[C_HIT_B], perim[1]);
        /* And their surfaces must differ from each other: the shared-pipeline defect. */
        {
            Cmp cp = compare(img[C_HIT_A], img[C_HIT_B], 64, 64);
            printf("    gpu surfaces differ in %u of 4096 pixels (coverage %u vs %u)\n",
                   4096u - cp.match, cp.cov_ref, cp.cov_got);
            if (cp.match == 4096u) {
                fail("FAIL: [pair] %08X and %08X produced the SAME surface on the GPU though "
                     "the interpreter separates them: the pipeline cache is serving one "
                     "program's shader for the other\n", ea->hash, eb->hash);
                ok = 0;
            }
        }
        nv2a_vk_backend_program_stats(&after);
        if (after.hits < 2) {
            fail("FAIL: [pair] hits %llu, expected >= 2 (one per program of the pair)\n",
                 (unsigned long long)after.hits);
            ok = 0;
        }
        if (after.first_hit_hash != ea->hash && after.first_hit_hash != eb->hash) {
            fail("FAIL: [pair] first_hit_hash %08X is neither %08X nor %08X\n",
                 after.first_hit_hash, ea->hash, eb->hash);
            ok = 0;
        }
    }

    /* ── 3. A miss falls back to the CPU, and is not silent. ── */
    puts("-- check 3: a program the corpus does not carry --");
    {
        uint32_t mhash, mlen = 2, mmask = 0;
        uint64_t mdigest = nv2a_vsh_aot_digest(k_miss_words, 2);
        uint32_t i;
        int clash = 0;
        /* Its identity, from the interpreter itself, and a search for it in the table. */
        for (i = 0; i < NV2A_VSH_SLOTS; i++) {
            const uint32_t zero[4] = { 0, 0, 0, 0 };
            nv2a_vsh_set_instruction(i, zero);
        }
        nv2a_vsh_set_instruction(0, k_miss_words[0]);
        nv2a_vsh_set_instruction(1, k_miss_words[1]);
        nv2a_vsh_set_start_slot(0);
        {
            const uint32_t (*w)[4] = NULL;
            uint32_t len = 0, hash = 0, mask = 0;
            int wc = 0;
            if (!nv2a_vsh_program_info(&w, &len, &hash, &mask, &wc)) {
                fail("FAIL: [miss] the miss program does not decode to a FINAL\n");
                ok = 0;
                hash = MISS_HASH;
            }
            mhash = hash; mlen = len; mmask = mask;
            printf("    miss program: hash %08X len %u mask %04X writes_c %d digest %016llX\n",
                   mhash, mlen, mmask, wc, (unsigned long long)mdigest);
        }
        for (i = 0; i < nv2a_vsh_program_count; i++) {
            const Nv2aVshAotEntry *e = &nv2a_vsh_programs[i];
            if (e->hash == mhash || e->digest == mdigest
                || nv2a_vsh_aot_digest(e->words, e->len) == mdigest) {
                clash = 1;
                fail("FAIL: [miss] the corpus carries this program: entry %zu hash %08X len %u "
                     "mask %04X digest %016llX\n", i, e->hash, e->len, e->input_mask,
                     (unsigned long long)e->digest);
                ok = 0;
            }
        }
        if (!clash) printf("    not in the corpus by hash, len or digest\n");
        nv2a_vk_backend_program_stats(&before);
        build_batch(k_surf[C_MISS], k_vb[C_MISS], k_miss_words, 2, 1);
        ok &= submit("vulkan miss");
        nv2a_vk_backend_program_stats(&after);
        img[C_MISS] = snapshot(k_surf[C_MISS], 64u * 64u * 4u);
        printf("  [miss] hits %llu -> %llu, misses %llu -> %llu, identity_fail %llu -> %llu, "
               "declines %llu -> %llu\n",
               (unsigned long long)before.hits, (unsigned long long)after.hits,
               (unsigned long long)before.misses, (unsigned long long)after.misses,
               (unsigned long long)before.identity_fail, (unsigned long long)after.identity_fail,
               (unsigned long long)before.declines, (unsigned long long)after.declines);
        printf("  [miss] first_miss_hash %08X len %u mask %04X\n", after.first_miss_hash,
               after.first_miss_len, after.first_miss_mask);
        if (after.misses <= before.misses) {
            fail("FAIL: [miss] misses did not advance: %llu -> %llu\n",
                 (unsigned long long)before.misses, (unsigned long long)after.misses);
            ok = 0;
        }
        if (after.first_miss_hash != mhash) {
            fail("FAIL: [miss] first_miss_hash %08X, expected %08X\n", after.first_miss_hash, mhash);
            ok = 0;
        }
        if (after.first_miss_len != mlen || after.first_miss_mask != mmask) {
            fail("FAIL: [miss] first_miss_len/mask %u/0x%04X, expected %u/0x%04X\n",
                 after.first_miss_len, after.first_miss_mask, mlen, mmask);
            ok = 0;
        }
        if (after.hits != before.hits) {
            fail("FAIL: [miss] hits advanced by %llu for a program the corpus does not carry\n",
                 (unsigned long long)(after.hits - before.hits));
            ok = 0;
        }
        /* The CPU interpreter must still have drawn it: the surface is not the clear colour and
         * it agrees with the CPU reference for the same batch. */
        {
            ok &= check_vs_cpu("miss", g_cpu[C_MISS], img[C_MISS], perim[2]);
        }
    }

    /* ── 4. A corrupted identity cannot select a wrong shader. ── */
    puts("-- check 4: a table whose identity fields are corrupt --");
    {
        size_t idx = 0;
        int found = 0, round;
        static const char *const what[2] = { "len", "digest" };
        for (idx = 0; idx < nv2a_vsh_program_count; idx++)
            if (nv2a_vsh_programs[idx].hash == HASH_A) { found = 1; break; }
        if (!found) {
            fail("FAIL: [identity] %08X is not in the table\n", HASH_A);
            return 0;
        }
        if (ncopy > 256) {
            fail("FAIL: [identity] the corpus is %zu entries, larger than the test's copy\n", ncopy);
            return 0;
        }
        memcpy(copy, nv2a_vsh_programs, ncopy * sizeof copy[0]);
        for (round = 0; round < 2; round++) {
            Nv2aVkProgramStats b4, af;
            uint32_t want_hash = copy[idx].hash;
            memcpy(copy, nv2a_vsh_programs, ncopy * sizeof copy[0]);
            if (round == 0)
                copy[idx].len = copy[idx].len - 1u;             /* hash still matches */
            else
                copy[idx].digest ^= 1ull;                       /* hash still matches */
            nv2a_vk_backend_set_programs(copy, ncopy);
            nv2a_vk_backend_program_stats(&b4);
            build_batch(k_surf[C_IDENT], k_vb[C_IDENT], nv2a_vsh_programs[idx].words,
                        nv2a_vsh_programs[idx].len, 1);
            ok &= submit("vulkan corrupt identity");
            nv2a_vk_backend_program_stats(&af);
            free(img[C_IDENT]);
            img[C_IDENT] = snapshot(k_surf[C_IDENT], 64u * 64u * 4u);
            printf("  [identity/%s] hash %08X still matches the table; hits %llu -> %llu, "
                   "identity_fail %llu -> %llu, misses %llu -> %llu\n", what[round], want_hash,
                   (unsigned long long)b4.hits, (unsigned long long)af.hits,
                   (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail,
                   (unsigned long long)b4.misses, (unsigned long long)af.misses);
            if (af.identity_fail <= b4.identity_fail) {
                fail("FAIL: [identity/%s] identity_fail did not advance: %llu -> %llu for a table "
                     "entry whose %s was corrupted\n", what[round],
                     (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail,
                     what[round]);
                ok = 0;
            }
            if (af.hits != b4.hits) {
                fail("FAIL: [identity/%s] hits advanced by %llu: a corrupt identity selected a "
                     "shader\n", what[round], (unsigned long long)(af.hits - b4.hits));
                ok = 0;
            }
            {
                char who[64];
                _snprintf(who, sizeof who, "identity/%s", what[round]);
                ok &= check_vs_cpu(who, g_cpu[C_IDENT], img[C_IDENT], perim[0]);
            }
        }
        memcpy(copy, nv2a_vsh_programs, ncopy * sizeof copy[0]);
        nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
        printf("  [identity] the table is restored (%zu entries)\n", ncopy);
    }
    for (z = 0; z < NSURF; z++) free(img[z]);
    return ok;
}

/* ── The CPU reference pass ──────────────────────────────────────────────── */
static int run_cpu_pass(void)
{
    const Nv2aVshAotEntry *ea = table_find(HASH_A), *eb = table_find(HASH_B);
    int ok = 1;
    if (!ea || !eb) return 0;
    build_batch(k_surf[C_HIT_A], k_vb[C_HIT_A], ea->words, ea->len, 1);
    ok &= submit("cpu hit A");
    build_batch(k_surf[C_HIT_B], k_vb[C_HIT_B], eb->words, eb->len, 1);
    ok &= submit("cpu hit B");
    build_batch(k_surf[C_MISS], k_vb[C_MISS], k_miss_words, 2, 1);
    ok &= submit("cpu miss");
    build_batch(k_surf[C_R3VIS], k_vb[C_R3VIS], k_miss_words, 2, 1);
    ok &= submit("cpu visible miss");
    build_batch(k_surf[C_IDENT], k_vb[C_IDENT], ea->words, ea->len, 1);
    ok &= submit("cpu identity");
    return ok;
}

/* ── Negative controls ───────────────────────────────────────────────────── */
static int negative_control(int which)
{
    static Nv2aVshAotEntry copy[256];
    size_t n = nv2a_vsh_program_count;
    int detected;

    if (n < 2 || n > 256) {
        fail("FAIL: negative control: the corpus has %zu entries\n", n);
        return 2;
    }
    if (which == 1) {
        /* (a) An unsorted table and a wrong digest must fail check 1. */
        memcpy(copy, nv2a_vsh_programs, n * sizeof copy[0]);
        { Nv2aVshAotEntry t = copy[0]; copy[0] = copy[1]; copy[1] = t; }   /* unsorted */
        copy[n / 2u].digest ^= 0xDEADBEEFull;                             /* wrong digest */
        printf("negative control 1: an unsorted table with one wrong digest\n");
        detected = !check_corpus_identity(copy, n, 0);
        if (!detected) {
            fail("FAIL: negative control 1: check 1 PASSED a table that is unsorted and carries "
                 "a wrong digest, so check 1 is not load-bearing\n");
            printf("negative control 1: check 1 MISSED the defect\n");
            return 2;
        }
        printf("negative control 1: check 1 FAILED the corrupt table, as it must\n");
        /* And the pristine table must still pass, or the control proves nothing either way. */
        if (!check_corpus_identity(nv2a_vsh_programs, n, 0)) {
            fail("FAIL: negative control 1: check 1 rejects the real corpus\n");
            return 2;
        }
        printf("negative control 1: check 1 accepts the real corpus and rejects the corrupt "
               "copy\n");
        return 1;
    }
    if (which == 2) {
        /* (b) A stats struct whose hits are forced to 0 must fail check 2. */
        Nv2aVkProgramStats st;
        const Nv2aVshAotEntry *ea = table_find(HASH_A);
        if (!ea) { fail("FAIL: negative control 2: %08X is not in the table\n", HASH_A); return 2; }
        printf("negative control 2: a stats struct whose hits are forced to 0\n");
        memset(&st, 0, sizeof st);
        nv2a_vk_backend_program_stats(&st);
        if (st.hits < 1) {
            fail("FAIL: negative control 2: the back end reports no hits at all (%llu), so the "
                 "control cannot be run: draw the hit batch first\n", (unsigned long long)st.hits);
            return 2;
        }
        if (!check_hit_stats(&st, ea->hash, "negative control 2 (undamaged)")) {
            fail("FAIL: negative control 2: check 2 rejects the real stats\n");
            return 2;
        }
        st.hits = 0;                                  /* in memory only, never on disk */
        detected = !check_hit_stats(&st, ea->hash, "negative control 2 (hits forced to 0)");
        if (!detected) {
            fail("FAIL: negative control 2: check 2 PASSED a stats struct with hits forced to 0, "
                 "so check 2 is not load-bearing\n");
            printf("negative control 2: check 2 MISSED the defect\n");
            return 2;
        }
        printf("negative control 2: check 2 FAILED the stats struct, as it must\n");
        return 1;
    }
    fail("FAIL: unknown negative control %d (expected 1 or 2)\n", which);
    return 2;
}

int main(int argc, char **argv)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *pbmem;
    char *log;
    int ok = 1, i, neg = 0;

    if (argc > 1 && strcmp(argv[1], "--skip") == 0) {
        /* The no-ICD fallback CTest uses. It runs NO check at all, so it cannot mask one: there
         * is nothing before it to fail. Reject any extra argument, so it can never be combined
         * with a mode that would have run checks. */
        if (argc != 3) {
            fprintf(stderr, "FAIL: --skip takes exactly one reason and runs no checks\n");
            return 1;
        }
        printf("SKIP: vsh_aot_programs -- %s (no check was run)\n", argv[2]);
        return 77;
    }
    if (argc > 1 && strcmp(argv[1], "--negative-control") == 0) {
        if (argc < 3) { fail("FAIL: --negative-control needs an argument\n"); return 1; }
        neg = atoi(argv[2]);
    }
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

    /* ── 1. Corpus identity, against the live interpreter. ── */
    puts("-- check 1: corpus identity against the live interpreter --");
    ok &= check_corpus_identity(nv2a_vsh_programs, nv2a_vsh_program_count, 1);

    if (neg == 1) {
        int rc = negative_control(1);
        fflush(stdout);
        printf("negative control 1: exit %d (%s)\n", rc,
               rc == 1 ? "check 1 correctly FAILED the simulated defect"
                       : "check 1 MISSED the simulated defect: the check is not load-bearing");
        return rc;
    }
    if (neg != 0 && neg != 2) {
        fail("FAIL: unknown negative control %d (expected 1 or 2)\n", neg);
        return 1;
    }

    /* ── The CPU reference pass. ── */
    puts("-- software executor (references) --");
    for (i = 0; i < NSURF; i++) prefill(k_surf[i]);
    ok &= run_cpu_pass();
    for (i = 0; i < NSURF; i++) g_cpu[i] = snapshot(k_surf[i], 64u * 64u * 4u);

    /* ── The Vulkan back end, on the software driver. ── */
    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        /* A skip may only mean "the device is unavailable". Everything that ran before this
         * point -- check 1 and the CPU reference pass -- has already reached a verdict, and a
         * failing check is a failure, not a skip: CTest counts 77 as Not Run, so skipping here
         * would let a broken corpus stay green. */
        if (!ok) {
            fail("FAIL: vsh_aot_programs: a check failed before the Vulkan back end could be "
                 "installed, so this is a FAILURE, not a skip\n");
            fflush(stdout);
            return 1;
        }
        puts("SKIP: vsh_aot_programs -- the Vulkan back end could not be installed on a software "
             "device (no Vulkan, no matching device, or an elevated process: the loader ignores "
             "VK_DRIVER_FILES there). Everything that ran before this point passed. This is a "
             "SKIP, not a pass.");
        fflush(stdout);
        return 77;
    }
    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    puts("-- Vulkan back end --");
    for (i = 0; i < NSURF; i++) prefill(k_surf[i]);
    ok &= run_gpu_pass();

    if (neg == 2) {
        /* The control needs real hits in the stats, so it runs after the hit draw above. */
        int rc = negative_control(2);
        fflush(stdout);
        printf("negative control 2: exit %d (%s)\n", rc,
               rc == 1 ? "check 2 correctly FAILED the simulated defect"
                       : "check 2 MISSED the simulated defect: the check is not load-bearing");
        return rc;
    }

    /* ── The log assertions: the miss and the identity failure must not be silent. ── */
    fflush(stderr);
    log = read_log();
    if (!log) {
        fail("FAIL: the test's stderr log could not be read back from %s\n", g_log_path);
        ok = 0;
    } else {
        static const char *const k_miss_line =
            "is not in the ahead-of-time corpus; the CPU interpreter draws it";
        static const char *const k_ident_line = "matched hash but not identity";
        if (!log_has(log, k_miss_line)) {
            fail("FAIL: [miss] the log has no line containing \"%s\": the miss was silent\n",
                 k_miss_line);
            ok = 0;
        } else {
            printf("  [miss] the log says the program is not in the corpus\n");
        }
        if (!log_has(log, k_ident_line)) {
            fail("FAIL: [identity] the log has no line containing \"%s\": the decline was silent\n",
                 k_ident_line);
            ok = 0;
        } else {
            printf("  [identity] the log says the hash matched but not the identity\n");
        }
        free(log);
    }

    fflush(stdout);
    nv2a_set_commit_consumer(NULL);
    for (i = 0; i < NSURF; i++) free(g_cpu[i]);
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();

    if (!ok) { fail("FAIL: vsh_aot_programs\n"); return 1; }
    puts("PASS: vsh_aot_programs (the corpus is self-consistent, a hit draws on the GPU, a miss "
         "and a corrupt identity fall back to the CPU, two same-mask programs keep their own "
         "shaders, and a fully culled batch still records its corpus lookup)");
    return 0;
}
