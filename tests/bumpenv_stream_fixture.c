/* The committed-stream fixture for the D4 step-5 BUMPENVMAP gate. See
 * bumpenv_stream_fixture.h for what it is and is not.
 *
 * Every number below is defined HERE, by hand, from public encodings:
 *   - register numbers: src/nv2a/nv2a_regs.h and src/nv2a/nv2a_method_table.c
 *     (what the walk admits);
 *   - the bump matrix's method order: xemu pgraph.c SET_TEXTURE_SET_BUMP_ENV_MAT
 *     writes method word i = 0,1,2,3 to BUMPMAT00, 01, 11, 10 -- so the stream
 *     carries m00, m01, m11, m10, in that order;
 *   - the vertex-program instruction layout: the field table at the top of the
 *     toolkit's src/kernel/nv2a_vsh_interp.c (also xemu vsh.c's field list);
 *   - the texture images: hand-defined below, written in the NV2A's swizzled
 *     (Morton) order by this file's own `morton`.
 * No executor, decoder or back-end helper is called to produce any of them.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_vsh_interp.h"
#include "recomp_types.h"
#include "bumpenv_stream_fixture.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#error "this fixture follows the repository's Windows-only committed-stream harness"
#endif

/* ── Toolkit symbols the game normally supplies (as tests/test_pbcov_stream.c) ── */

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

/* ── Register numbers (src/nv2a/nv2a_regs.h; every one is in the walk's table) ── */

#define M_SET_OBJECT            0x0000u
#define M_SURFACE_CLIP_H        0x0200u
#define M_SURFACE_CLIP_V        0x0204u
#define M_SURFACE_FORMAT        0x0208u
#define M_SURFACE_PITCH         0x020Cu
#define M_SURFACE_COLOR_OFFSET  0x0210u
#define M_FINAL_CW0             0x0288u     /* SET_COMBINER_SPECULAR_FOG_CW0 */
#define M_FINAL_CW1             0x028Cu
#define M_TRANSFORM_PROGRAM     0x0B00u     /* 32 dwords */
#define M_TRANSFORM_CONSTANT    0x0B80u     /* 16 dwords admitted */
#define M_VERTEX_OFFSET0        0x1720u
#define M_VERTEX_FORMAT0        0x1760u
#define M_BEGIN_END             0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_TEX_OFFSET            0x1B00u     /* + 0x40 * stage, as all below */
#define M_TEX_FORMAT            0x1B04u
#define M_TEX_ADDRESS           0x1B08u
#define M_TEX_CONTROL0          0x1B0Cu
#define M_TEX_CONTROL1          0x1B10u     /* admitted for stage 0 only */
#define M_TEX_FILTER            0x1B14u
#define M_TEX_IMAGE_RECT        0x1B1Cu     /* admitted for stage 0 only */
#define M_BUMP_MAT              0x1B28u     /* 4 dwords; stages 1..3 admitted */
#define M_BUMP_SCALE            0x1B38u
#define M_BUMP_OFFSET           0x1B3Cu
#define M_FINAL_C0              0x1E20u     /* SET_SPECULAR_FOG_FACTOR(0) */
#define M_COMBINER_CONTROL      0x1E60u
#define M_SHADER_STAGE_PROGRAM  0x1E70u
#define M_OTHER_STAGE_INPUT     0x1E78u
#define M_TRANSFORM_EXEC_MODE   0x1E94u
#define M_TRANSFORM_CXT_WRITE   0x1E98u
#define M_TRANSFORM_PROG_LOAD   0x1E9Cu
#define M_TRANSFORM_PROG_START  0x1EA0u
#define M_TRANSFORM_CONST_LOAD  0x1EA4u

#define PRIM_TRIANGLES          5u          /* SET_BEGIN_END operand */
#define TEX_CONTROL0_ENABLE     0x40000000u
#define TEX_ADDRESS_CLAMP       (3u | (3u << 8) | (3u << 16))   /* clamp-to-edge u, v, p */
#define TEX_FILTER_NEAREST      ((1u << 24) | (1u << 16))       /* MAG 1, MIN 1: box, no mips */

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

/* ── The images, hand-defined ───────────────────────────────────────────────── */

/* NV2A swizzle for a square power-of-two image: texel (x, y) lives at the
 * index whose bit 2i is bit i of x and whose bit 2i+1 is bit i of y. */
static uint32_t morton(uint32_t x, uint32_t y, uint32_t log2n)
{
    uint32_t i, o = 0;
    for (i = 0; i < log2n; i++)
        o |= (((x >> i) & 1u) << (2u * i)) | (((y >> i) & 1u) << (2u * i + 1u));
    return o;
}

/* D(x, y): R names x, G names y, B is a constant 0x80, A is opaque.
 *   R = 0x10 + 0x20*x, G = 0x10 + 0x20*y   (x, y = 0..7: 0x10..0xF0) */
static uint32_t tex_d(uint32_t x, uint32_t y)
{
    return 0xFF000000u | ((0x10u + 0x20u * x) << 16) | ((0x10u + 0x20u * y) << 8) | 0x80u;
}

/* S(x, y): the delta sources, (R, G, B) = (lum, dv, du) as bytes, A opaque.
 * The table is the contract bumpenv_stream_cases.h derives from. */
static const uint8_t k_src[4][4][3] = {     /* [y][x] = R, G, B */
    { {0x00,0x00,0x00}, {0x40,0x00,0x7F}, {0x80,0x7F,0x00}, {0x20,0x00,0x81} },
    { {0x00,0x00,0x80}, {0x00,0x00,0xFF}, {0x00,0x81,0x00}, {0x00,0x80,0x00} },
    { {0x00,0xFF,0x00}, {0x00,0x7F,0x80}, {0x00,0x80,0x7F}, {0xFF,0x00,0x00} },
    { {0xC0,0x00,0x00}, {0x60,0x00,0x00}, {0x00,0x00,0x00}, {0x00,0x00,0x00} },
};

static void write_images(void)
{
    uint32_t x, y;

    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            put32(BSF_TEX_D_VA + 4u * morton(x, y, 3), tex_d(x, y));
            put32(BSF_TEX_DLIN_VA + 32u * y + 4u * x, tex_d(x, y));
        }
    for (y = 0; y < 4; y++)
        for (x = 0; x < 4; x++) {
            const uint8_t *s = k_src[y][x];
            uint16_t g8b8 = 0x2020u;               /* any bytes: nothing decodes them */
            put32(BSF_TEX_S_VA + 4u * morton(x, y, 2),
                  0xFF000000u | ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2]);
            memcpy(guest(BSF_TEX_G8B8_VA + 2u * morton(x, y, 2)), &g8b8, 2);
        }
}

/* Six positions, two triangles covering the whole 64x64 surface. oPos is
 * written straight from v0, so these are surface pixels with w = 1. */
static void write_vertices(void)
{
    static const float p[6][2] = {
        { 0.0f, 0.0f }, { 64.0f, 0.0f }, { 0.0f, 64.0f },
        { 0.0f, 64.0f }, { 64.0f, 0.0f }, { 64.0f, 64.0f }
    };
    int i;
    for (i = 0; i < 6; i++) {
        float v[4] = { p[i][0], p[i][1], 0.5f, 1.0f };
        memcpy(guest(BSF_VB_VA + 16u * (uint32_t)i), v, sizeof v);
    }
}

/* ── The vertex program ─────────────────────────────────────────────────────── */

/* MOV o[out], src.xyzw as one MAC instruction (nv2a_vsh_interp.c's table):
 *   dw1: MAC op bits 21-24 = 1 (MOV); A swizzle bits 0-7 = 0x1B (x y z w, two
 *        bits each, x in the HIGHEST pair: 00 01 10 11); input index bits 9-12
 *        or constant index bits 13-20;
 *   dw2: A mux bits 26-27 = 2 (input v[]) or 3 (constant c[]);
 *   dw3: output mask bits 12-15 = 0xF, "to o[]" bit 11, output address bits
 *        3-10, FINAL bit 0.
 * o[0] is oPos and o[9..12] are oT0..oT3. */
static void vp_mov(uint32_t w[4], uint32_t out, int from_const, uint32_t idx, int final)
{
    w[0] = 0;
    w[1] = (1u << 21) | 0x1Bu | (from_const ? (idx << 13) : (idx << 9));
    w[2] = (from_const ? 3u : 2u) << 26;
    w[3] = (0xFu << 12) | (1u << 11) | (out << 3) | (final ? 1u : 0u);
}

/* oPos = v0; oT0..oT3 = c[0..3]. Each draw's texture coordinates are therefore
 * the four constants it uploads, identical at every vertex, so every covered
 * pixel samples the same texels. The walk admits vertex-array offsets only for
 * attributes 0-5, 9 and 10, which is one reason they come from constants. */
static void emit_program(void)
{
    uint32_t ins[5][4], i, j;

    vp_mov(ins[0], 0, 0, 0, 0);
    vp_mov(ins[1], 9, 1, 0, 0);
    vp_mov(ins[2], 10, 1, 1, 0);
    vp_mov(ins[3], 11, 1, 2, 0);
    vp_mov(ins[4], 12, 1, 3, 1);
    emit(M_TRANSFORM_EXEC_MODE, 2u);              /* PROGRAM */
    emit(M_TRANSFORM_CXT_WRITE, 0u);
    emit(M_TRANSFORM_PROG_LOAD, 0u);
    for (i = 0; i < 5; i++)
        for (j = 0; j < 4; j++)
            emit(M_TRANSFORM_PROGRAM + 4u * (4u * i + j), ins[i][j]);
    emit(M_TRANSFORM_PROG_START, 0u);
}

int bsf_init(const char *tag)
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

    write_images();
    write_vertices();

    /* The committed surface, the position array, the program, and the
     * combiner control that makes the executor run the combiners (rc_seen):
     * zero general stages, so only the final combiner below decides a pixel. */
    F.pbw = 0;
    emit(M_SET_OBJECT, OBJ_HANDLE);
    emit(M_SURFACE_CLIP_H, BSF_SURF_W << 16);
    emit(M_SURFACE_CLIP_V, BSF_SURF_H << 16);
    emit(M_SURFACE_FORMAT, 0x0108u);              /* A8R8G8B8, pitch (linear) */
    emit(M_SURFACE_PITCH, BSF_SURF_PITCH);
    emit(M_SURFACE_COLOR_OFFSET, BSF_SURF_VA);
    emit(M_VERTEX_OFFSET0, BSF_VB_VA);
    emit(M_VERTEX_FORMAT0, 2u | (4u << 4) | (16u << 8));   /* float, 4, stride 16 */
    emit_program();
    emit(M_COMBINER_CONTROL, 0u);
    if (!submit()) {
        fprintf(stderr, "FAIL %s: setup stream rejected\n", tag);
        return 0;
    }
    return 1;
}

/* ── One draw ───────────────────────────────────────────────────────────────── */

static void emit_texture(uint32_t st, int kind)
{
    uint32_t b = 0x40u * st, va = 0, color = 0x06u, lg = 3, cube = 0, dims = 2, lp = 0;

    switch (kind) {
    case BSF_TEX_D:        va = BSF_TEX_D_VA; break;
    case BSF_TEX_D_CUBE:   va = BSF_TEX_D_VA; cube = 1; break;
    case BSF_TEX_D_VOLUME: va = BSF_TEX_D_VA; dims = 3; lp = 1; break;
    case BSF_TEX_S:        va = BSF_TEX_S_VA; lg = 2; break;
    case BSF_TEX_G8B8:     va = BSF_TEX_G8B8_VA; lg = 2; color = 0x28u; break;
    case BSF_TEX_D_LINEAR: va = BSF_TEX_DLIN_VA; color = 0x12u; lg = 0; break;
    default:               break;
    }
    emit(M_TEX_OFFSET + b, va);
    if (!va)
        return;
    /* FORMAT: DMA A (bit 0), cube (bit 2), dimensionality (4-7), colour (8-15),
     * one level (16-19), log2 U (20-23), log2 V (24-27), log2 P (28-31). */
    emit(M_TEX_FORMAT + b, 1u | (cube << 2) | (dims << 4) | (color << 8) | (1u << 16)
                           | (lg << 20) | (lg << 24) | (lp << 28));
    emit(M_TEX_ADDRESS + b, TEX_ADDRESS_CLAMP);
    emit(M_TEX_CONTROL0 + b, TEX_CONTROL0_ENABLE);
    emit(M_TEX_FILTER + b, TEX_FILTER_NEAREST);
    if (kind == BSF_TEX_D_LINEAR) {               /* stage 0 only: walk-admitted there */
        emit(M_TEX_CONTROL1 + b, 32u << 16);      /* pitch 32 */
        emit(M_TEX_IMAGE_RECT + b, (8u << 16) | 8u);
    }
}

static void emit_draw_state(const BsfDraw *d)
{
    uint32_t st, i, r = d->out_reg & 0xFu;

    emit(M_SET_OBJECT, OBJ_HANDLE);
    emit(M_SHADER_STAGE_PROGRAM, d->prog);
    emit(M_OTHER_STAGE_INPUT, d->osi);
    /* Final combiner, out = D + C*(1-A) + B*A with every input UNSIGNED_IDENTITY:
     *   plain:  A = 0 (zero), B = 0, C = r, D = 0           -> out.rgb = r
     *   scaled: A = 1 (c0),   B = r, C = 0, D = 0           -> out.rgb = r * c0
     * and the alpha (CW1 bits 8-15, G) is r's alpha (0x10 | r). */
    emit(M_FINAL_CW0, d->final_c0 ? ((1u << 24) | (r << 16)) : (r << 8));
    emit(M_FINAL_CW1, (0x10u | r) << 8);
    emit(M_FINAL_C0, d->final_c0);
    for (st = 0; st < 3; st++)
        emit_texture(st, d->tex[st]);
    for (st = 1; st < 4; st++) {
        uint32_t b = 0x40u * st;
        /* The method order is m00, m01, m11, m10 (xemu pgraph.c). */
        emit(M_BUMP_MAT + b + 0x0u, fbits(d->mat[st][0]));     /* m00 */
        emit(M_BUMP_MAT + b + 0x4u, fbits(d->mat[st][1]));     /* m01 */
        emit(M_BUMP_MAT + b + 0x8u, fbits(d->mat[st][3]));     /* m11 */
        emit(M_BUMP_MAT + b + 0xCu, fbits(d->mat[st][2]));     /* m10 */
        emit(M_BUMP_SCALE + b, fbits(d->scale[st]));
        emit(M_BUMP_OFFSET + b, fbits(d->offset[st]));
    }
    emit(M_TRANSFORM_CONST_LOAD, 0u);
    for (st = 0; st < 4; st++)
        for (i = 0; i < 4; i++)
            emit(M_TRANSFORM_CONSTANT + 4u * (4u * st + i), fbits(d->tc[st][i]));
}

static void emit_batch(void)
{
    emit(M_BEGIN_END, PRIM_TRIANGLES);
    emit(M_DRAW_ARRAYS, (5u << 24) | 0u);         /* count field + 1 = six vertices */
    emit(M_BEGIN_END, 0u);
}

int bsf_submit_draw(const BsfDraw *d)
{
    F.pbw = 0;
    emit_draw_state(d);
    emit_batch();
    return submit();
}

int bsf_submit_draw_with(const BsfDraw *d, uint32_t method, uint32_t param)
{
    F.pbw = 0;
    emit_draw_state(d);
    emit(method, param);
    emit_batch();
    return submit();
}

uint32_t bsf_pixel(uint32_t x, uint32_t y)
{
    uint32_t v;
    memcpy(&v, guest(BSF_SURF_VA + y * BSF_SURF_PITCH + x * 4u), 4);
    return v;
}

void bsf_fill_surface(uint32_t argb)
{
    uint32_t i;
    for (i = 0; i < BSF_SURF_W * BSF_SURF_H; i++)
        put32(BSF_SURF_VA + 4u * i, argb);
}
