/* The committed-stream fixture for the NV2A X8R8G8B8-alias gate. See
 * x8alias_stream_fixture.h for what it is and is not.
 *
 * Every number below is defined HERE, by hand, from public encodings: the
 * register numbers are the toolkit's src/nv2a/nv2a_regs.h and
 * src/nv2a/nv2a_method_table.c (what the walk admits); the guest-memory,
 * RAMHT and submit scaffolding is tests/wclip_stream_fixture.c's, the texture
 * binding and vertex-program/combiner words are tests/texgen_stream_fixture.c's.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "recomp_types.h"
#include "x8alias_stream_fixture.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#error "this fixture follows the repository's Windows-only committed-stream harness"
#endif

/* ── Toolkit symbols the game normally supplies (as tests/wclip_stream_fixture.c) ── */

recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site, uint32_t value)
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

/* The executor's window/present seam, inert here (no window is opened). */
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

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);

/* ── Register numbers ───────────────────────────────────────────────────────── */

#define M_SET_OBJECT            0x0000u
#define M_SURFACE_CLIP_H        0x0200u
#define M_SURFACE_CLIP_V        0x0204u
#define M_SURFACE_FORMAT        0x0208u
#define M_SURFACE_PITCH         0x020Cu
#define M_SURFACE_COLOR_OFFSET  0x0210u
#define M_FINAL_CW0             0x0288u     /* SET_COMBINER_SPECULAR_FOG_CW0 */
#define M_FINAL_CW1             0x028Cu
#define M_COMPOSITE             0x0680u     /* 16 words */
#define M_TRANSFORM_PROGRAM     0x0B00u
#define M_TRANSFORM_CONSTANT    0x0B80u
#define M_VERTEX_OFFSET0        0x1720u     /* + 4 * attribute */
#define M_VERTEX_FORMAT0        0x1760u     /* + 4 * attribute */
#define M_BEGIN_END             0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_TEX_OFFSET            0x1B00u     /* stage 0 */
#define M_TEX_FORMAT            0x1B04u
#define M_TEX_ADDRESS           0x1B08u
#define M_TEX_CONTROL0          0x1B0Cu     /* bit 30 is the enable */
#define M_TEX_CONTROL1          0x1B10u     /* pitch in bits 16-31, linear formats */
#define M_TEX_FILTER            0x1B14u
#define M_TEX_IMAGE_RECT        0x1B1Cu     /* width << 16 | height, linear formats */
#define M_COLOR_CLEAR_VALUE     0x1D90u
#define M_CLEAR_SURFACE         0x1D94u
#define M_FINAL_C0              0x1E20u
#define M_COMBINER_CONTROL      0x1E60u     /* any write turns the register combiners on */
#define M_SHADER_STAGE_PROGRAM  0x1E70u
#define M_OTHER_STAGE_INPUT     0x1E78u
#define M_TRANSFORM_EXEC_MODE   0x1E94u     /* 0 fixed function, 2 program */
#define M_TRANSFORM_CXT_WRITE   0x1E98u
#define M_TRANSFORM_PROG_LOAD   0x1E9Cu
#define M_TRANSFORM_PROG_START  0x1EA0u
#define M_TRANSFORM_CONST_LOAD  0x1EA4u

#define CLEAR_COLOR_ALL         0xF0u       /* CLEAR_SURFACE: R, G, B, A */
#define PRIM_TRIANGLES          5u          /* SET_BEGIN_END operand */
#define TEX_CONTROL0_ENABLE     0x40000000u
#define TEX_ADDRESS_CLAMP       (3u | (3u << 8) | (3u << 16))   /* clamp-to-edge u, v, p */
#define TEX_FILTER_NEAREST      ((1u << 24) | (1u << 16))       /* MAG 1, MIN 1: box, no mips */
#define SHADER_STAGE_2D         1u          /* stage 0 program PROJECT2D, 5 bits per stage */
/* SET_VERTEX_DATA_ARRAY_FORMAT: type bits 0-3 (0 D3DCOLOR, 2 float), size bits
 * 4-7, stride bits 8-15. */
#define VFMT(type, size, stride) ((uint32_t)(type) | ((uint32_t)(size) << 4) \
                                  | ((uint32_t)(stride) << 8))

/* The texture coordinate every vertex carries, in texels (a linear texture's
 * coordinates are texels). Both lie inside A, whose texels are all X8F_CLEAR
 * until the quad lands; for the self case it is a texel of the half the quad
 * does not cover, so the value read is X8F_CLEAR whatever order pixels land in. */
#define TC_QUAD_S       16.5f
#define TC_QUAD_T       16.5f
#define TC_SELF_S       24.5f
#define TC_SELF_T       8.5f

/* ── Fixture memory ─────────────────────────────────────────────────────────── */

#define RAM_SIZE        (64u * 1024u * 1024u)
#define RAMIN_SIZE      (1024u * 1024u)
#define INSTANCE_SIZE   0x20000u
#define PB_BYTES        0x10000u
#define XBOX_BASE_ADDRESS 0x00010000u
#define OBJ_HANDLE      6u
#define OBJ_ADDR        0x300u

static struct {
    uint8_t *ram, *ramin, *instance;
    uint32_t *pb;
    NV2AState *gpu;
    uint32_t pbw;
} F;

static uint8_t *guest(uint32_t va)
{
    return (uint8_t *)xbox_GetMemoryOffset() + va;
}

static void put32(uint32_t va, uint32_t v) { memcpy(guest(va), &v, 4); }

static uint32_t fbits(float f)
{
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}

static void emit(uint32_t method, uint32_t param)
{
    F.pb[F.pbw++] = (1u << 18) | method;          /* one dword, subchannel 0 */
    F.pb[F.pbw++] = param;
}

static int submit(void)
{
    F.gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    F.gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = F.pbw * 4u;
    return nv2a_submit_pending(F.gpu) ? 1 : 0;
}

int x8f_init(const char *tag)
{
    static uint8_t xbe[0x2000];
    uint32_t *pair, *object;

    memset(&F, 0, sizeof F);
    F.ram = (uint8_t *)calloc(1, RAM_SIZE);
    F.ramin = (uint8_t *)calloc(1, RAMIN_SIZE);
    F.instance = (uint8_t *)calloc(1, INSTANCE_SIZE);
    F.pb = (uint32_t *)VirtualAlloc(NULL, PB_BYTES, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!F.ram || !F.ramin || !F.instance || !F.pb) {
        fprintf(stderr, "FAIL %s: fixture allocation\n", tag);
        return 0;
    }
    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) {
        fprintf(stderr, "FAIL %s: xbox_MemoryLayoutInit\n", tag);
        return 0;
    }
    nv2a_reset_standalone_for_test();
    F.gpu = nv2a_init_standalone(F.ram, RAM_SIZE, F.ramin, RAMIN_SIZE);
    if (!F.gpu
     || !nv2a_set_pushbuffer_window(F.gpu, (uint8_t *)F.pb, 0, PB_BYTES)
     || !nv2a_bind_instance_memory(0x83fe0000u, F.instance, INSTANCE_SIZE)) {
        fprintf(stderr, "FAIL %s: nv2a standalone setup\n", tag);
        return 0;
    }
    /* RAMHT handle 6 -> object 0x300, class NV097. */
    pair = (uint32_t *)(F.instance + OBJ_HANDLE * 8u);
    object = (uint32_t *)(F.instance + (OBJ_ADDR << 4));
    pair[0] = OBJ_HANDLE;
    pair[1] = NV_RAMHT_STATUS | OBJ_ADDR;
    object[0] = 0x97u;
    object[1] = object[2] = object[3] = 0;

    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) {
        fprintf(stderr, "FAIL %s: executor did not register\n", tag);
        return 0;
    }
    return 1;
}

/* Six positions, two triangles covering x in [0, qw], y in [0, 32], z 0.5 and
 * w 1. The composite matrix is the identity, so the fixed-function unit puts
 * each at its own (x, y) pixel, and the vertex program writes oPos = v0, the
 * same place. Every vertex has diffuse white and the same texel coordinate. */
static void write_vertices(float qw, float s, float t)
{
    const float fh = (float)X8F_H;
    const float p[6][2] = {
        { 0.0f, 0.0f }, { qw, 0.0f }, { 0.0f, fh },
        { 0.0f, fh }, { qw, 0.0f }, { qw, fh }
    };
    int i;
    for (i = 0; i < 6; i++) {
        float v[4] = { p[i][0], p[i][1], 0.5f, 1.0f };
        float tc[2] = { s, t };
        memcpy(guest(X8F_POS_VA + 16u * (uint32_t)i), v, sizeof v);
        put32(X8F_DIF_VA + 4u * (uint32_t)i, 0xFFFFFFFFu);
        memcpy(guest(X8F_TC_VA + 8u * (uint32_t)i), tc, sizeof tc);
    }
}

/* MOV o[out], src.xyzw as one MAC instruction (nv2a_vsh_interp.c's table), as
 * tests/texgen_stream_fixture.c: o[0] is oPos and o[9] is oT0. */
static void vp_mov(uint32_t w[4], uint32_t out, int from_const, uint32_t idx, int final)
{
    w[0] = 0;
    w[1] = (1u << 21) | 0x1Bu | (from_const ? (idx << 13) : (idx << 9));
    w[2] = (from_const ? 3u : 2u) << 26;
    w[3] = (0xFu << 12) | (1u << 11) | (out << 3) | (final ? 1u : 0u);
}

/* oPos = v0; oT0 = c[0]. */
static void emit_program(void)
{
    uint32_t ins[2][4], i, j;

    vp_mov(ins[0], 0, 0, 0, 0);
    vp_mov(ins[1], 9, 1, 0, 1);
    emit(M_TRANSFORM_EXEC_MODE, 2u);
    emit(M_TRANSFORM_CXT_WRITE, 0u);
    emit(M_TRANSFORM_PROG_LOAD, 0u);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 4; j++)
            emit(M_TRANSFORM_PROGRAM + 4u * (4u * i + j), ins[i][j]);
    emit(M_TRANSFORM_PROG_START, 0u);
}

/* Stage 0 bound to surface A's address and pitch as a linear texture of `fmt`. */
static void emit_texture(uint32_t fmt)
{
    emit(M_TEX_OFFSET, X8F_A_VA);
    /* FORMAT: DMA A (bit 0), dimensionality 2 (4-7), colour (8-15), one level (16-19). */
    emit(M_TEX_FORMAT, 1u | (2u << 4) | (fmt << 8) | (1u << 16));
    emit(M_TEX_ADDRESS, TEX_ADDRESS_CLAMP);
    emit(M_TEX_CONTROL0, TEX_CONTROL0_ENABLE);
    emit(M_TEX_FILTER, TEX_FILTER_NEAREST);
    emit(M_TEX_CONTROL1, X8F_PITCH << 16);
    emit(M_TEX_IMAGE_RECT, (X8F_W << 16) | X8F_H);
}

int x8f_submit(const X8Draw *d)
{
    float s = d->self ? TC_SELF_S : TC_QUAD_S, t = d->self ? TC_SELF_T : TC_QUAD_T;
    uint32_t target = d->self ? X8F_A_VA : X8F_B_VA, k;

    write_vertices(d->self ? 16.0f : (float)X8F_W, s, t);
    F.pbw = 0;
    emit(M_SET_OBJECT, OBJ_HANDLE);

    /* Surface A, cleared to X8F_CLEAR (alpha 0x40). The clear is what makes A a
     * live render target in the Vulkan back end. */
    emit(M_SURFACE_CLIP_H, X8F_W << 16);
    emit(M_SURFACE_CLIP_V, X8F_H << 16);
    emit(M_SURFACE_FORMAT, 0x0108u);              /* A8R8G8B8 linear */
    emit(M_SURFACE_PITCH, X8F_PITCH);
    emit(M_SURFACE_COLOR_OFFSET, X8F_A_VA);
    emit(M_COLOR_CLEAR_VALUE, X8F_CLEAR);
    emit(M_CLEAR_SURFACE, CLEAR_COLOR_ALL);

    /* The draw target: B, or A again for the self case. */
    emit(M_SURFACE_COLOR_OFFSET, target);

    /* Identity composite in delivery order (it is what makes a batch
     * screen-space in the executor), one position array and one diffuse array. */
    for (k = 0; k < 16; k++)
        emit(M_COMPOSITE + 4u * k, fbits(k % 5u == 0 ? 1.0f : 0.0f));
    emit(M_VERTEX_OFFSET0 + 4u * 0u, X8F_POS_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 0u, VFMT(2, 4, 16));
    emit(M_VERTEX_OFFSET0 + 4u * 3u, X8F_DIF_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 3u, VFMT(0, 4, 4));
    emit_texture(d->fmt);

    if (!d->rc) {
        /* Fixed function, no COMBINER_CONTROL write at all: stage 0 modulated by
         * the diffuse. The texel coordinate is attribute 9. */
        emit(M_TRANSFORM_EXEC_MODE, 0u);
        emit(M_VERTEX_OFFSET0 + 4u * 9u, X8F_TC_VA);
        emit(M_VERTEX_FORMAT0 + 4u * 9u, VFMT(2, 2, 8));
    } else {
        /* Vertex program (oPos = v0, oT0 = c[0]) and the register combiners with
         * zero general stages. Final combiner out = D + C*(1-A) + B*A with every
         * input UNSIGNED_IDENTITY: A = B = D = 0, C = t0 (register 8), so rgb = t0
         * and alpha (CW1 bits 8-15, G) = t0's alpha (0x10 | 8): the texel's own
         * alpha is what reaches the pixel. */
        emit_program();
        emit(M_COMBINER_CONTROL, 0u);
        emit(M_SHADER_STAGE_PROGRAM, SHADER_STAGE_2D);
        emit(M_OTHER_STAGE_INPUT, 0u);
        emit(M_FINAL_CW0, 8u << 8);
        emit(M_FINAL_CW1, (0x10u | 8u) << 8);
        emit(M_FINAL_C0, 0u);
        emit(M_TRANSFORM_CONST_LOAD, 0u);
        emit(M_TRANSFORM_CONSTANT + 4u * 0u, fbits(s));
        emit(M_TRANSFORM_CONSTANT + 4u * 1u, fbits(t));
        emit(M_TRANSFORM_CONSTANT + 4u * 2u, fbits(0.0f));
        emit(M_TRANSFORM_CONSTANT + 4u * 3u, fbits(1.0f));
    }

    emit(M_BEGIN_END, PRIM_TRIANGLES);
    emit(M_DRAW_ARRAYS, (5u << 24) | 0u);         /* count field + 1 = six vertices */
    emit(M_BEGIN_END, 0u);
    return submit();
}

uint32_t x8f_pixel(uint32_t surf, uint32_t x, uint32_t y)
{
    uint32_t v;
    memcpy(&v, guest((surf == X8F_SURF_A ? X8F_A_VA : X8F_B_VA) + y * X8F_PITCH + x * 4u), 4);
    return v;
}

void x8f_fill(uint32_t argb)
{
    uint32_t i;
    for (i = 0; i < X8F_W * X8F_H; i++) {
        put32(X8F_A_VA + 4u * i, argb);
        put32(X8F_B_VA + 4u * i, argb);
    }
}
