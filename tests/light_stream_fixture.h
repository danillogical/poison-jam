/* The committed-stream fixture shared by the NV2A fixed-function vertex-lighting
 * gate's two halves (tests/test_light_stream.c, CPU; tests/test_light_stream_vk.c,
 * Vulkan). It owns no expectation: every literal lives in tests/light_stream_cases.h
 * with its derivation, and each half checks its own pixels against those literals.
 *
 * The fixture follows tests/zeta_stream_fixture.c (guest RAM laid out by a synthetic
 * section-less XBE, `nv2a_init_standalone`, the pushbuffer window, an instance/RAMHT
 * binding for class NV097, `RECOMP_PB_EXEC=1`, the commit consumer, real pushbuffer
 * words through `nv2a_submit_pending`) and tests/texgen_stream_fixture.c (composite,
 * model-view, inverse model-view and a normal array on a fixed-function draw). One
 * submit draws one untextured, lit quad whose six vertices share one normal, so the
 * colour is flat. Nothing here calls an executor or back-end helper.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_LIGHT_STREAM_FIXTURE_H
#define JSRF_LIGHT_STREAM_FIXTURE_H

#include <stdint.h>

/* Low RAM, below the contiguous window and clear of the synthetic image at
 * 0x00010000, so dma_resolve passes each offset through. The target is 32x32 linear
 * A8R8G8B8, pitch 128; the quad covers all of it. */
#define LF_W            32u
#define LF_H            32u
#define LF_PITCH        (LF_W * 4u)
#define LF_SURF_VA      0x00440000u
#define LF_POS_VA       0x00420000u     /* attribute 0: six float4 positions */
#define LF_NRM_VA       0x00420100u     /* attribute 2: six float3 normals */
#define LF_DIF_VA       0x00420200u     /* attribute 3: six D3DCOLOR diffuse */

/* The vertex colour stream. Lighting must ignore it (SET_COLOR_MATERIAL is 0), so no
 * correct pixel ever reads this value. */
#define LF_VERTEX_ARGB  0xFF336699u

typedef struct {
    const float *mv;        /* 16 SET_MODEL_VIEW_MATRIX words (rows, delivery order); NULL = identity */
    const float *imv;       /* 12 SET_INVERSE_MODEL_VIEW_MATRIX words; NULL = identity */
    uint32_t normalize;     /* SET_NORMALIZATION_ENABLE */
    float normal[3];        /* attribute 2 at every vertex */
    float dir[3];           /* light 0 INFINITE_DIRECTION words, as delivered */
    float scene[3];         /* SCENE_AMBIENT_COLOR */
    float amb[3];           /* light 0 ambient */
    float dif[3];           /* light 0 diffuse */
    float alpha;            /* SET_MATERIAL_ALPHA */
} LDraw;

/* Guest memory, NV2A standalone, RAMHT and the commit consumer. Returns 1 on
 * success; prints the failing step otherwise. */
int lf_init(const char *tag);

/* Emit the whole stream for `d` and submit it. Returns 1 when the walk accepted it. */
int lf_submit(const LDraw *d);

/* The colour target, in pixels. */
uint32_t lf_pixel(uint32_t x, uint32_t y);
void lf_fill(uint32_t argb);

#endif
