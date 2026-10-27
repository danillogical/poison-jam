/* The committed-stream fixture for the NV2A per-zeta depth gate. See
 * zeta_stream_fixture.h for what it is and is not.
 *
 * Every number below is defined HERE, by hand, from public encodings: the
 * register numbers are the toolkit's src/nv2a/nv2a_regs.h and
 * src/nv2a/nv2a_method_table.c (what the walk admits); the guest-memory,
 * RAMHT and submit scaffolding is tests/x8alias_stream_fixture.c's.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "recomp_types.h"
#include "zeta_stream_fixture.h"

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
#define M_SURFACE_ZETA_OFFSET   0x0214u
#define M_DEPTH_TEST_ENABLE     0x030Cu
#define M_DEPTH_FUNC            0x0354u
#define M_CLIP_MAX              0x0398u     /* SET_CLIP_MAX, float */
#define M_COLOR_MASK            0x0358u
#define M_DEPTH_MASK            0x035Cu
#define M_COMPOSITE             0x0680u     /* 16 words */
#define M_VERTEX_OFFSET0        0x1720u     /* + 4 * attribute */
#define M_VERTEX_FORMAT0        0x1760u     /* + 4 * attribute */
#define M_BEGIN_END             0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_TEX_CONTROL0          0x1B0Cu     /* stage 0; 0 leaves it disabled */
#define M_ZSTENCIL_CLEAR_VALUE  0x1D8Cu
#define M_COLOR_CLEAR_VALUE     0x1D90u
#define M_CLEAR_SURFACE         0x1D94u
#define M_TRANSFORM_EXEC_MODE   0x1E94u     /* 0 fixed function, 2 program */
#define M_TRANSFORM_CXT_WRITE   0x1E98u
#define M_TRANSFORM_PROG_LOAD   0x1E9Cu
#define M_TRANSFORM_PROG_START  0x1EA0u
#define M_TRANSFORM_PROGRAM     0x0B00u

#define CLEAR_Z                 0x01u       /* CLEAR_SURFACE: depth */
#define CLEAR_COLOR_ALL         0xF0u       /* CLEAR_SURFACE: R, G, B, A */
#define PRIM_TRIANGLES          5u          /* SET_BEGIN_END operand */
#define DEPTH_FUNC_LESS         0x201u      /* GL enum, as the NV2A */
#define COLOR_MASK_ALL          0x01010101u /* A, R, G, B each one byte, bit 0 = write */
/* SET_SURFACE_FORMAT: colour A8R8G8B8 (8), zeta Z24S8 (2 << 4), linear (1 << 8). */
#define SURFACE_FORMAT_Z24      0x0128u
/* SET_VERTEX_DATA_ARRAY_FORMAT: type bits 0-3 (0 D3DCOLOR, 2 float), size bits
 * 4-7, stride bits 8-15. */
#define VFMT(type, size, stride) ((uint32_t)(type) | ((uint32_t)(size) << 4) \
                                  | ((uint32_t)(stride) << 8))

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

int zf_init(const char *tag)
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

/* Six positions, two triangles covering x in [0, qw], y in [0, 32], at depth z and
 * w 1, all six vertices the colour `argb`. The composite matrix is the identity, so the
 * fixed-function unit puts each at its own (x, y) pixel with z unchanged. */
static void write_quad(uint32_t pos_va, uint32_t dif_va, float qw, float z, uint32_t argb)
{
    const float fh = (float)ZF_H;
    const float p[6][2] = {
        { 0.0f, 0.0f }, { qw, 0.0f }, { 0.0f, fh },
        { 0.0f, fh }, { qw, 0.0f }, { qw, fh }
    };
    int i;
    for (i = 0; i < 6; i++) {
        float v[4] = { p[i][0], p[i][1], z, 1.0f };
        memcpy(guest(pos_va + 16u * (uint32_t)i), v, sizeof v);
        put32(dif_va + 4u * (uint32_t)i, argb);
    }
}

/* MOV o[out], v[idx].xyzw as one MAC instruction (nv2a_vsh_interp.c's decode: word 1
 * bits 9-12 the input register, bits 21-24 the MAC op 1 = MOV; word 2 bits 26-27 the A
 * mux, 2 = input; word 3 bit 11 output-is-o, bits 3-10 the output index, bits 12-15 the
 * write mask, bit 0 final), as tests/x8alias_stream_fixture.c. o[0] is oPos, o[3] oD0. */
static void vp_mov(uint32_t w[4], uint32_t out, uint32_t idx, int final)
{
    w[0] = 0;
    w[1] = (1u << 21) | 0x1Bu | (idx << 9);
    w[2] = 2u << 26;
    w[3] = (0xFu << 12) | (1u << 11) | (out << 3) | (final ? 1u : 0u);
}

/* oPos = v0 (x, y, z and w=1 straight from the vertex); oD0 = v3 (the diffuse). */
static void emit_program(void)
{
    uint32_t ins[2][4], i, j;

    vp_mov(ins[0], 0, 0, 0);
    vp_mov(ins[1], 3, 3, 1);
    emit(M_TRANSFORM_EXEC_MODE, 2u);
    emit(M_TRANSFORM_CXT_WRITE, 0u);
    emit(M_TRANSFORM_PROG_LOAD, 0u);
    for (i = 0; i < 2; i++)
        for (j = 0; j < 4; j++)
            emit(M_TRANSFORM_PROGRAM + 4u * (4u * i + j), ins[i][j]);
    emit(M_TRANSFORM_PROG_START, 0u);
}

/* Bind one quad's position and diffuse arrays and draw it. */
static void emit_draw(uint32_t pos_va, uint32_t dif_va)
{
    emit(M_VERTEX_OFFSET0 + 4u * 0u, pos_va);
    emit(M_VERTEX_FORMAT0 + 4u * 0u, VFMT(2, 4, 16));
    emit(M_VERTEX_OFFSET0 + 4u * 3u, dif_va);
    emit(M_VERTEX_FORMAT0 + 4u * 3u, VFMT(0, 4, 4));
    emit(M_BEGIN_END, PRIM_TRIANGLES);
    emit(M_DRAW_ARRAYS, (5u << 24) | 0u);         /* count field + 1 = six vertices */
    emit(M_BEGIN_END, 0u);
}

int zf_submit(const ZDraw *d)
{
    uint32_t k;

    write_quad(ZF_POS_P_VA, ZF_DIF_P_VA, ZF_P_W, ZF_Z_NEAR, ZF_P_ARGB);
    write_quad(ZF_POS_Q_VA, ZF_DIF_Q_VA, ZF_Q_W, ZF_Z_MID, ZF_Q_ARGB);
    F.pbw = 0;
    emit(M_SET_OBJECT, OBJ_HANDLE);

    /* One colour target C throughout; the zeta address is what changes. */
    emit(M_SURFACE_CLIP_H, ZF_W << 16);
    emit(M_SURFACE_CLIP_V, ZF_H << 16);
    emit(M_SURFACE_FORMAT, SURFACE_FORMAT_Z24);
    emit(M_SURFACE_PITCH, ZF_PITCH);
    emit(M_SURFACE_COLOR_OFFSET, ZF_C_VA);
    emit(M_CLIP_MAX, fbits(ZF_Z_SCALE));

    /* Identity composite in delivery order (it is what makes a batch screen-space in
     * the executor), fixed function, no texture, depth test LESS with depth writes. */
    for (k = 0; k < 16; k++)
        emit(M_COMPOSITE + 4u * k, fbits(k % 5u == 0 ? 1.0f : 0.0f));
    if (d->program)
        emit_program();
    else
        emit(M_TRANSFORM_EXEC_MODE, 0u);
    emit(M_TEX_CONTROL0, 0u);
    emit(M_COLOR_MASK, COLOR_MASK_ALL);
    emit(M_DEPTH_TEST_ENABLE, 1u);
    emit(M_DEPTH_FUNC, DEPTH_FUNC_LESS);
    emit(M_DEPTH_MASK, 1u);
    emit(M_COLOR_CLEAR_VALUE, ZF_BG_ARGB);
    emit(M_ZSTENCIL_CLEAR_VALUE, ZF_ZCLEAR);

    /* 1. Zeta Z1: clear colour and depth, draw P at the near depth (Z1 = ZF_Z_NEAR). */
    emit(M_SURFACE_ZETA_OFFSET, ZF_Z1_VA);
    emit(M_CLEAR_SURFACE, CLEAR_COLOR_ALL | CLEAR_Z);
    emit_draw(ZF_POS_P_VA, ZF_DIF_P_VA);

    /* 2. The middle zeta (Z2, or Z1 again): clear depth only, draw nothing. */
    emit(M_SURFACE_ZETA_OFFSET, d->mid_zeta);
    emit(M_CLEAR_SURFACE, CLEAR_Z);

    /* 3. Back to Z1 with no clear: Q at the middle depth, wider than P. */
    emit(M_SURFACE_ZETA_OFFSET, ZF_Z1_VA);
    emit_draw(ZF_POS_Q_VA, ZF_DIF_Q_VA);
    return submit();
}

uint32_t zf_pixel(uint32_t x, uint32_t y)
{
    uint32_t v;
    memcpy(&v, guest(ZF_C_VA + y * ZF_PITCH + x * 4u), 4);
    return v;
}

void zf_fill(uint32_t argb)
{
    uint32_t i;
    for (i = 0; i < ZF_W * ZF_H; i++)
        put32(ZF_C_VA + 4u * i, argb);
}
