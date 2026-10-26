/* The committed-stream fixture for the D4 step-6 texgen / texture-matrix gate.
 * See texgen_stream_fixture.h for what it is and is not.
 *
 * Every number below is defined HERE, by hand, from public encodings:
 *   - register numbers: the toolkit's src/nv2a/nv2a_regs.h and
 *     src/nv2a/nv2a_method_table.c (what the walk admits), cross-read against
 *     xemu's nv2a_regs.h (NV097_SET_TEXGEN_S 0x3C0 ... SET_EYE_POSITION 0xA50);
 *   - the texgen / matrix method layout: xemu pgraph.c -- TEXGEN S/T/R/Q at
 *     0x3C0 + 0x10*stage + 4*component, MATRIX_ENABLE 0x420 + 4*stage, the
 *     model-view, inverse model-view, composite and texture matrices and the
 *     texgen planes each load word k into constant row k/4, component k%4;
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
#include "texgen_stream_fixture.h"

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

/* ── Register numbers (every one is in the walk's table) ─────────────────────── */

#define M_SET_OBJECT            0x0000u
#define M_SURFACE_CLIP_H        0x0200u
#define M_SURFACE_CLIP_V        0x0204u
#define M_SURFACE_FORMAT        0x0208u
#define M_SURFACE_PITCH         0x020Cu
#define M_SURFACE_COLOR_OFFSET  0x0210u
#define M_FINAL_CW0             0x0288u     /* SET_COMBINER_SPECULAR_FOG_CW0 */
#define M_FINAL_CW1             0x028Cu
#define M_NORMALIZATION_ENABLE  0x03A4u
#define M_TEXGEN_S              0x03C0u     /* + 0x10 * stage + 4 * component (S, T, R) */
#define M_TEXTURE_MATRIX_ENABLE 0x0420u     /* + 4 * stage */
#define M_MODEL_VIEW            0x0480u     /* 16 words */
#define M_INVERSE_MODEL_VIEW    0x0580u     /* 12 words admitted */
#define M_COMPOSITE             0x0680u     /* 16 words */
#define M_TEXTURE_MATRIX        0x06C0u     /* + 0x40 * stage, 16 words; stages 0-1 admitted */
#define M_TEXGEN_PLANE_S        0x0840u     /* + 0x40 * stage + 0x10 * component */
#define M_EYE_POSITION          0x0A50u     /* 4 words */
#define M_TRANSFORM_PROGRAM     0x0B00u
#define M_TRANSFORM_CONSTANT    0x0B80u
#define M_VERTEX_OFFSET0        0x1720u     /* + 4 * attribute */
#define M_VERTEX_FORMAT0        0x1760u     /* + 4 * attribute */
#define M_BEGIN_END             0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_TEX_OFFSET            0x1B00u     /* + 0x40 * stage, as all below */
#define M_TEX_FORMAT            0x1B04u
#define M_TEX_ADDRESS           0x1B08u
#define M_TEX_CONTROL0          0x1B0Cu
#define M_TEX_CONTROL1          0x1B10u     /* admitted for stage 0 only */
#define M_TEX_FILTER            0x1B14u
#define M_TEX_IMAGE_RECT        0x1B1Cu     /* admitted for stage 0 only */
#define M_FINAL_C0              0x1E20u
#define M_COMBINER_CONTROL      0x1E60u
#define M_SHADER_STAGE_PROGRAM  0x1E70u
#define M_OTHER_STAGE_INPUT     0x1E78u
#define M_TRANSFORM_EXEC_MODE   0x1E94u     /* 0 fixed function, 2 program */
#define M_TRANSFORM_CXT_WRITE   0x1E98u
#define M_TRANSFORM_PROG_LOAD   0x1E9Cu
#define M_TRANSFORM_PROG_START  0x1EA0u
#define M_TRANSFORM_CONST_LOAD  0x1EA4u

#define PRIM_TRIANGLES          5u          /* SET_BEGIN_END operand */
#define TEX_CONTROL0_ENABLE     0x40000000u
#define TEX_ADDRESS_CLAMP       (3u | (3u << 8) | (3u << 16))   /* clamp-to-edge u, v, p */
#define TEX_FILTER_NEAREST      ((1u << 24) | (1u << 16))       /* MAG 1, MIN 1: box, no mips */
/* SET_VERTEX_DATA_ARRAY_FORMAT: type bits 0-3 (0 D3DCOLOR, 2 float), size bits
 * 4-7, stride bits 8-15. Size 0 disables the attribute (what D3D sends). */
#define VFMT(type, size, stride) ((uint32_t)(type) | ((uint32_t)(size) << 4) \
                                  | ((uint32_t)(stride) << 8))
#define VFMT_OFF                VFMT(2, 0, 0)

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

/* T(x, y): R names x, G names y, B names the image, A is opaque.
 *   R = 0x10 + 0x20*x, G = 0x10 + 0x20*y   (x, y = 0..7: 0x10..0xF0) */
static uint32_t texel(uint32_t x, uint32_t y, uint32_t blue)
{
    return 0xFF000000u | ((0x10u + 0x20u * x) << 16) | ((0x10u + 0x20u * y) << 8) | blue;
}

static void write_images(void)
{
    uint32_t x, y;

    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            put32(TGF_TEX_A_VA + 4u * morton(x, y, 3), texel(x, y, 0x80u));
            put32(TGF_TEX_B_VA + 4u * morton(x, y, 3), texel(x, y, 0x40u));
            put32(TGF_TEX_C_VA + 4u * morton(x, y, 3), texel(x, y, 0xC0u));
            put32(TGF_TEX_AL_VA + 32u * y + 4u * x, texel(x, y, 0x80u));
        }
}

/* Six positions, two triangles covering the whole 64x64 surface, with z 0.5
 * and w 1. The composite matrix is the identity, so the fixed-function unit
 * puts each at its own (x, y) surface pixel; the program writes oPos = v0,
 * which is the same place. Every vertex has the same diffuse colour. */
static void write_vertices(void)
{
    static const float p[6][2] = {
        { 0.0f, 0.0f }, { 64.0f, 0.0f }, { 0.0f, 64.0f },
        { 0.0f, 64.0f }, { 64.0f, 0.0f }, { 64.0f, 64.0f }
    };
    int i;
    for (i = 0; i < 6; i++) {
        float v[4] = { p[i][0], p[i][1], 0.5f, 1.0f };
        memcpy(guest(TGF_POS_VA + 16u * (uint32_t)i), v, sizeof v);
        put32(TGF_DIF_VA + 4u * (uint32_t)i, TGF_DIFFUSE);
    }
}

/* ── The vertex program (program draws only) ────────────────────────────────── */

/* MOV o[out], src.xyzw as one MAC instruction (nv2a_vsh_interp.c's table):
 *   dw1: MAC op bits 21-24 = 1 (MOV); A swizzle bits 0-7 = 0x1B (x y z w);
 *        input index bits 9-12 or constant index bits 13-20;
 *   dw2: A mux bits 26-27 = 2 (input v[]) or 3 (constant c[]);
 *   dw3: output mask bits 12-15 = 0xF, "to o[]" bit 11, output address bits
 *        3-10, FINAL bit 0.
 * o[0] is oPos and o[9] is oT0. */
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

int tgf_init(const char *tag)
{
    static uint8_t xbe[0x2000];
    uint32_t *pair, *object, k;

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

    /* The committed surface; the composite matrix, identity in delivery order
     * (it is what makes a batch fixed-function in the executor); the position
     * and diffuse arrays; the program; and the combiner control that makes the
     * executor run the combiners (zero general stages, so only the final
     * combiner below decides a pixel on the paths that use them). */
    F.pbw = 0;
    emit(M_SET_OBJECT, OBJ_HANDLE);
    emit(M_SURFACE_CLIP_H, TGF_SURF_W << 16);
    emit(M_SURFACE_CLIP_V, TGF_SURF_H << 16);
    emit(M_SURFACE_FORMAT, 0x0108u);              /* A8R8G8B8, pitch (linear) */
    emit(M_SURFACE_PITCH, TGF_SURF_PITCH);
    emit(M_SURFACE_COLOR_OFFSET, TGF_SURF_VA);
    for (k = 0; k < 16; k++)
        emit(M_COMPOSITE + 4u * k, fbits(k % 5u == 0 ? 1.0f : 0.0f));
    emit(M_VERTEX_OFFSET0 + 4u * 0u, TGF_POS_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 0u, VFMT(2, 4, 16));
    emit(M_VERTEX_OFFSET0 + 4u * 3u, TGF_DIF_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 3u, VFMT(0, 4, 4));
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
    uint32_t b = 0x40u * st, va = 0, color = 0x06u, lg = 3;

    switch (kind) {
    case TGF_TEX_A:        va = TGF_TEX_A_VA; break;
    case TGF_TEX_B:        va = TGF_TEX_B_VA; break;
    case TGF_TEX_C:        va = TGF_TEX_C_VA; break;
    case TGF_TEX_A_LINEAR: va = TGF_TEX_AL_VA; color = 0x12u; lg = 0; break;
    default:               break;
    }
    emit(M_TEX_OFFSET + b, va);
    if (!va)
        return;
    /* FORMAT: DMA A (bit 0), dimensionality 2 (4-7), colour (8-15), one level
     * (16-19), log2 U (20-23), log2 V (24-27), log2 P 0 (28-31). */
    emit(M_TEX_FORMAT + b, 1u | (2u << 4) | (color << 8) | (1u << 16) | (lg << 20) | (lg << 24));
    emit(M_TEX_ADDRESS + b, TEX_ADDRESS_CLAMP);
    emit(M_TEX_CONTROL0 + b, TEX_CONTROL0_ENABLE);
    emit(M_TEX_FILTER + b, TEX_FILTER_NEAREST);
    if (kind == TGF_TEX_A_LINEAR) {               /* stage 0 only: walk-admitted there */
        emit(M_TEX_CONTROL1 + b, 32u << 16);      /* pitch 32 */
        emit(M_TEX_IMAGE_RECT + b, (8u << 16) | 8u);
    }
}

/* Words in delivery order; NULL is the identity (k % 5 == 0 on the diagonal). */
static void emit_matrix(uint32_t base, const float *w, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < n; k++)
        emit(base + 4u * k, fbits(w ? w[k] : (k % 5u == 0 ? 1.0f : 0.0f)));
}

/* One texture-coordinate attribute (9 or 10): unbound, or `size` floats of
 * `v` at every vertex. */
static void emit_tc_array(uint32_t attr, uint32_t va, int size, const float v[4])
{
    int i;
    if (!size) {
        emit(M_VERTEX_OFFSET0 + 4u * attr, 0u);
        emit(M_VERTEX_FORMAT0 + 4u * attr, VFMT_OFF);
        return;
    }
    for (i = 0; i < 6; i++)
        memcpy(guest(va + 16u * (uint32_t)i), v, 16);
    emit(M_VERTEX_OFFSET0 + 4u * attr, va);
    emit(M_VERTEX_FORMAT0 + 4u * attr, VFMT(2, (uint32_t)size, 16));
}

static void emit_draw_state(const TgDraw *d)
{
    uint32_t st, c, k, r = d->out_reg & 0xFu;
    int i;

    emit(M_SET_OBJECT, OBJ_HANDLE);
    emit(M_TRANSFORM_EXEC_MODE, d->ffp ? 0u : 2u);
    emit(M_SHADER_STAGE_PROGRAM, d->prog);
    emit(M_OTHER_STAGE_INPUT, 0u);
    /* Final combiner, out = D + C*(1-A) + B*A with every input UNSIGNED_IDENTITY:
     * A = 0 (zero), B = 0, C = r, D = 0 -> out.rgb = r; alpha (CW1 bits 8-15,
     * G) is r's alpha (0x10 | r). */
    emit(M_FINAL_CW0, r << 8);
    emit(M_FINAL_CW1, (0x10u | r) << 8);
    emit(M_FINAL_C0, 0u);
    for (st = 0; st < 3; st++)
        emit_texture(st, d->tex[st]);

    for (st = 0; st < 4; st++)
        for (c = 0; c < 3; c++)
            emit(M_TEXGEN_S + 0x10u * st + 4u * c, st < 3 ? d->texgen[st][c] : 0u);
    for (st = 0; st < 3; st++)
        for (k = 0; k < 16; k++)
            emit(M_TEXGEN_PLANE_S + 0x40u * st + 4u * k,
                 fbits(d->plane[st] ? d->plane[st][k] : 0.0f));
    for (st = 0; st < 4; st++)
        emit(M_TEXTURE_MATRIX_ENABLE + 4u * st, d->mat_enable[st]);
    for (st = 0; st < 2; st++)
        emit_matrix(M_TEXTURE_MATRIX + 0x40u * st, d->mat[st], 16);
    emit_matrix(M_MODEL_VIEW, d->mv, 16);
    emit_matrix(M_INVERSE_MODEL_VIEW, d->imv, 12);
    emit(M_NORMALIZATION_ENABLE, d->normalize);
    if (d->eye)
        for (k = 0; k < 4; k++)
            emit(M_EYE_POSITION + 4u * k, fbits(d->eye[k]));

    for (i = 0; i < 6; i++)
        memcpy(guest(TGF_NRM_VA + 12u * (uint32_t)i), d->normal, 12);
    emit(M_VERTEX_OFFSET0 + 4u * 2u, TGF_NRM_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 2u, VFMT(2, 3, 12));
    emit_tc_array(9, TGF_TC0_VA, d->tc_size[0], d->tc[0]);
    emit_tc_array(10, TGF_TC1_VA, d->tc_size[1], d->tc[1]);

    emit(M_TRANSFORM_CONST_LOAD, 0u);
    for (k = 0; k < 4; k++)
        emit(M_TRANSFORM_CONSTANT + 4u * k, fbits(d->vp_t0[k]));
}

static void emit_batch(void)
{
    emit(M_BEGIN_END, PRIM_TRIANGLES);
    emit(M_DRAW_ARRAYS, (5u << 24) | 0u);         /* count field + 1 = six vertices */
    emit(M_BEGIN_END, 0u);
}

int tgf_submit_draw(const TgDraw *d)
{
    F.pbw = 0;
    emit_draw_state(d);
    emit_batch();
    return submit();
}

int tgf_submit_draw_with(const TgDraw *d, uint32_t method, uint32_t param)
{
    F.pbw = 0;
    emit_draw_state(d);
    emit(method, param);
    emit_batch();
    return submit();
}

uint32_t tgf_pixel(uint32_t x, uint32_t y)
{
    uint32_t v;
    memcpy(&v, guest(TGF_SURF_VA + y * TGF_SURF_PITCH + x * 4u), 4);
    return v;
}

void tgf_fill_surface(uint32_t argb)
{
    uint32_t i;
    for (i = 0; i < TGF_SURF_W * TGF_SURF_H; i++)
        put32(TGF_SURF_VA + 4u * i, argb);
}
