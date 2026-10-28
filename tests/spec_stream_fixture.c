/* The committed-stream fixture for the NV2A fixed-function SPECULAR gate. See
 * spec_stream_fixture.h for what it is and is not.
 *
 * Every number below is defined HERE, by hand, from public encodings: the
 * register numbers are the toolkit's src/nv2a/nv2a_regs.h and
 * src/nv2a/nv2a_method_table.c (what the walk admits); the guest-memory,
 * RAMHT and submit scaffolding is tests/light_stream_fixture.c's; the
 * final-combiner words are tests/x8alias_stream_fixture.c's (0x0288/0x028C/
 * 0x1E20/0x1E60) and xemu's psh.c field packing.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "recomp_types.h"
#include "spec_stream_fixture.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#error "this fixture follows the repository's Windows-only committed-stream harness"
#endif

/* ── Toolkit symbols the game normally supplies (as tests/light_stream_fixture.c) ── */

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
#define M_FINAL_CW1             0x028Cu     /* SET_COMBINER_SPECULAR_FOG_CW1 */
#define M_LIGHT_CONTROL         0x0294u     /* SET_LIGHT_CONTROL */
#define M_COLOR_MATERIAL        0x0298u     /* SET_COLOR_MATERIAL */
#define M_LIGHTING_ENABLE       0x0314u
#define M_NORMALIZATION_ENABLE  0x03A4u
#define M_MATERIAL_ALPHA        0x03B4u     /* float */
#define M_SPECULAR_ENABLE       0x03B8u
#define M_LIGHT_ENABLE_MASK     0x03BCu     /* 2 bits per light: 0 off, 1 infinite */
#define M_MODEL_VIEW            0x0480u     /* 16 words */
#define M_INVERSE_MODEL_VIEW    0x0580u     /* 12 words admitted */
#define M_COMPOSITE             0x0680u     /* 16 words */
#define M_SCENE_AMBIENT         0x0A10u     /* r, g, b floats */
#define M_LIGHT0                0x1000u     /* light 0 block; the walk admits only 0x1000..0x103C */
#define L0_AMBIENT              0x00u       /* + 4 * channel */
#define L0_DIFFUSE              0x0Cu
#define L0_SPECULAR             0x18u
#define L0_INF_HALF_VECTOR      0x28u
#define L0_INF_DIRECTION        0x34u
#define M_VERTEX_OFFSET0        0x1720u     /* + 4 * attribute */
#define M_VERTEX_FORMAT0        0x1760u     /* + 4 * attribute */
#define M_BEGIN_END             0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_TEX_CONTROL0          0x1B0Cu     /* stage 0; 0 leaves it disabled */
#define M_FINAL_C0              0x1E20u     /* SET_SPECULAR_FOG_FACTOR0 */
#define M_COMBINER_CONTROL      0x1E60u     /* any write turns the register combiners on */
#define M_SHADER_STAGE_PROGRAM  0x1E70u
#define M_OTHER_STAGE_INPUT     0x1E78u
#define M_TRANSFORM_EXEC_MODE   0x1E94u     /* 0 fixed function */

#define PRIM_TRIANGLES          5u          /* SET_BEGIN_END operand */
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

int sf_init(const char *tag)
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

/* Six positions, two triangles covering x, y in [0, 32], at z 0.5 and w 1; every
 * vertex the same normal `nrm`, the same diffuse stream colour and the same
 * specular stream colour. The composite matrix is the identity, so the
 * fixed-function unit puts each at its own (x, y) pixel whatever the model-view
 * is (the model-view only feeds the eye position). */
static void write_quad(const float nrm[3])
{
    const float fh = (float)SF_H, fw = (float)SF_W;
    const float p[6][2] = {
        { 0.0f, 0.0f }, { fw, 0.0f }, { 0.0f, fh },
        { 0.0f, fh }, { fw, 0.0f }, { fw, fh }
    };
    int i;
    for (i = 0; i < 6; i++) {
        float v[4] = { p[i][0], p[i][1], 0.5f, 1.0f };
        memcpy(guest(SF_POS_VA + 16u * (uint32_t)i), v, sizeof v);
        memcpy(guest(SF_NRM_VA + 12u * (uint32_t)i), nrm, 12);
        put32(SF_DIF_VA + 4u * (uint32_t)i, SF_VERTEX_DIF_ARGB);
        put32(SF_SPC_VA + 4u * (uint32_t)i, SF_VERTEX_SPC_ARGB);
    }
}

/* Words in delivery order; NULL is the identity (k % 5 == 0 on the diagonal). */
static void emit_matrix(uint32_t base, const float *w, uint32_t n)
{
    uint32_t k;
    for (k = 0; k < n; k++)
        emit(base + 4u * k, fbits(w ? w[k] : (k % 5u == 0 ? 1.0f : 0.0f)));
}

static void emit_vec3(uint32_t method, const float v[3])
{
    uint32_t k;
    for (k = 0; k < 3; k++)
        emit(method + 4u * k, fbits(v[k]));
}

int sf_submit(const SDraw *d)
{
    uint32_t k;

    write_quad(d->normal);
    F.pbw = 0;
    emit(M_SET_OBJECT, OBJ_HANDLE);

    emit(M_SURFACE_CLIP_H, SF_W << 16);
    emit(M_SURFACE_CLIP_V, SF_H << 16);
    emit(M_SURFACE_FORMAT, 0x0108u);              /* A8R8G8B8, pitch (linear) */
    emit(M_SURFACE_PITCH, SF_PITCH);
    emit(M_SURFACE_COLOR_OFFSET, SF_SURF_VA);

    /* Identity composite in delivery order (it is what makes a batch
     * fixed-function in the executor), fixed function, no texture. */
    for (k = 0; k < 16; k++)
        emit(M_COMPOSITE + 4u * k, fbits(k % 5u == 0 ? 1.0f : 0.0f));
    emit(M_TRANSFORM_EXEC_MODE, 0u);
    emit(M_TEX_CONTROL0, 0u);

    /* Register combiners on (a write to COMBINER_CONTROL is what sets the
     * executor's rc_seen), zero general stages, and a final combiner that emits
     * the observer register. Stage program 0: no stage samples anything, so no
     * texture state is needed. OTHER_STAGE_INPUT 0: no bump stage reads an
     * earlier one. FINAL_C0 0: the final constants are unused here. */
    emit(M_COMBINER_CONTROL, 0u);
    emit(M_SHADER_STAGE_PROGRAM, 0u);
    emit(M_OTHER_STAGE_INPUT, 0u);
    emit(M_FINAL_CW0, d->observer);
    emit(M_FINAL_CW1, SF_OBS_ALPHA_V0);
    emit(M_FINAL_C0, 0u);

    emit_matrix(M_MODEL_VIEW, d->mv, 16);
    emit_matrix(M_INVERSE_MODEL_VIEW, d->imv, 12);
    emit(M_NORMALIZATION_ENABLE, d->normalize);

    /* Lighting per the case, light 0 infinite, the diffuse not routed from the
     * vertex. LIGHT_CONTROL is delivered so the separate-specular bit is the
     * case's own, not a reset default. NO SET_SPECULAR_PARAMS word appears: the
     * walk does not admit 0x09E0..0x09F4 (see spec_stream_cases.h PENDING). */
    emit(M_LIGHTING_ENABLE, d->lighting);
    emit(M_COLOR_MATERIAL, 0u);
    emit(M_LIGHT_CONTROL, d->light_control);
    emit(M_SPECULAR_ENABLE, d->spec_enable);
    emit(M_LIGHT_ENABLE_MASK, 1u);
    emit(M_MATERIAL_ALPHA, fbits(d->alpha));
    emit_vec3(M_SCENE_AMBIENT, d->scene);
    emit_vec3(M_LIGHT0 + L0_AMBIENT, d->amb);
    emit_vec3(M_LIGHT0 + L0_DIFFUSE, d->dif);
    emit_vec3(M_LIGHT0 + L0_SPECULAR, d->spc);
    emit_vec3(M_LIGHT0 + L0_INF_HALF_VECTOR, d->half);
    emit_vec3(M_LIGHT0 + L0_INF_DIRECTION, d->dir);

    emit(M_VERTEX_OFFSET0 + 4u * 0u, SF_POS_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 0u, VFMT(2, 4, 16));
    emit(M_VERTEX_OFFSET0 + 4u * 2u, SF_NRM_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 2u, VFMT(2, 3, 12));
    emit(M_VERTEX_OFFSET0 + 4u * 3u, SF_DIF_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 3u, VFMT(0, 4, 4));
    emit(M_VERTEX_OFFSET0 + 4u * 4u, SF_SPC_VA);
    emit(M_VERTEX_FORMAT0 + 4u * 4u, VFMT(0, 4, 4));

    emit(M_BEGIN_END, PRIM_TRIANGLES);
    emit(M_DRAW_ARRAYS, (5u << 24) | 0u);         /* count field + 1 = six vertices */
    emit(M_BEGIN_END, 0u);
    return submit();
}

uint32_t sf_pixel(uint32_t x, uint32_t y)
{
    uint32_t v;
    memcpy(&v, guest(SF_SURF_VA + y * SF_PITCH + x * 4u), 4);
    return v;
}

void sf_fill(uint32_t argb)
{
    uint32_t i;
    for (i = 0; i < SF_W * SF_H; i++)
        put32(SF_SURF_VA + 4u * i, argb);
}
