/* The committed-stream fixture shared by the NV2A fixed-function SPECULAR gate's
 * two halves (tests/test_spec_stream.c, CPU; tests/test_spec_stream_vk.c,
 * Vulkan). It owns no expectation: every literal lives in tests/spec_stream_cases.h
 * with its derivation, and each half checks its own pixels against those literals.
 *
 * The fixture follows tests/light_stream_fixture.c exactly (guest RAM laid out by
 * a synthetic section-less XBE, `nv2a_init_standalone`, the pushbuffer window, an
 * instance/RAMHT binding for class NV097, `RECOMP_PB_EXEC=1`, the commit consumer,
 * real pushbuffer words through `nv2a_submit_pending`), and adds what the Q5a
 * fixture lacks: the register combiners are programmed so that the final combiner
 * emits either oD1 or oD0, which is the only way to OBSERVE the specular output
 * at all. One submit draws one untextured quad whose six vertices share one
 * normal, so the colour is flat.
 *
 * Nothing here calls an executor or back-end helper, and no expected value is
 * derived from one.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_SPEC_STREAM_FIXTURE_H
#define JSRF_SPEC_STREAM_FIXTURE_H

#include <stdint.h>

/* Low RAM, below the contiguous window and clear of the synthetic image at
 * 0x00010000, so dma_resolve passes each offset through. The target is 32x32
 * linear A8R8G8B8, pitch 128; the quad covers all of it. */
#define SF_W            32u
#define SF_H            32u
#define SF_PITCH        (SF_W * 4u)
#define SF_SURF_VA      0x00440000u
#define SF_POS_VA       0x00420000u     /* attribute 0: six float4 positions */
#define SF_NRM_VA       0x00420100u     /* attribute 2: six float3 normals */
#define SF_DIF_VA       0x00420200u     /* attribute 3: six D3DCOLOR diffuse */
#define SF_SPC_VA       0x00420300u     /* attribute 4: six D3DCOLOR specular */

/* The vertex colour stream: a diffuse the lighting rule must ignore
 * (SET_COLOR_MATERIAL = 0), and a specular the lit path must never read. Neither
 * value may appear in a correct lit pixel, so a defect that passes the vertex
 * attribute through is visible as these bytes. */
#define SF_VERTEX_DIF_ARGB  0xFF00FF00u
#define SF_VERTEX_SPC_ARGB  0xFFFF00FFu

/* The final-combiner observer. CW0 packs A<<24 | B<<16 | C<<8 | D (xemu
 * psh.c:1543-1551 parse_combiner_inputs), and each field's low nibble names a
 * register: 4 = V0 = oD0, 5 = V1 = oD1 (xemu psh_regs.h:74-75, psh.c:327-330,
 * psh.c:1085-1086). With A = B = 0 the combiner emits D + C, and with one of C/D
 * zero it emits exactly that register's rgb. The alpha field is CW1 bits 8-15,
 * G, and 0x14 selects V0's alpha (xemu psh.c:284). */
#define SF_OBS_D1       0x00000005u     /* D = V1 -> rgb = oD1 */
#define SF_OBS_D0       0x00000400u     /* C = V0 -> rgb = oD0 */
#define SF_OBS_ALPHA_V0 0x00001400u     /* G = V0 alpha (0x10 | 4) */

typedef struct {
    const float *mv;        /* 16 SET_MODEL_VIEW_MATRIX words (rows, delivery order); NULL = identity */
    const float *imv;       /* 12 SET_INVERSE_MODEL_VIEW words; NULL = identity */
    uint32_t normalize;     /* SET_NORMALIZATION_ENABLE */
    uint32_t lighting;      /* SET_LIGHTING_ENABLE */
    uint32_t spec_enable;   /* SET_SPECULAR_ENABLE */
    uint32_t light_control; /* SET_LIGHT_CONTROL */
    uint32_t observer;      /* final-combiner CW0: SF_OBS_D1 or SF_OBS_D0 */
    float normal[3];        /* attribute 2 at every vertex */
    float dir[3];           /* light 0 INFINITE_DIRECTION words, as delivered */
    float half[3];          /* light 0 INFINITE_HALF_VECTOR words, as delivered */
    float scene[3];         /* SCENE_AMBIENT_COLOR */
    float amb[3];           /* light 0 ambient */
    float dif[3];           /* light 0 diffuse */
    float spc[3];           /* light 0 specular */
    float alpha;            /* SET_MATERIAL_ALPHA */
} SDraw;

/* Guest memory, NV2A standalone, RAMHT and the commit consumer. Returns 1 on
 * success; prints the failing step otherwise. */
int sf_init(const char *tag);

/* Emit the whole stream for `d` and submit it. Returns 1 when the walk accepted it. */
int sf_submit(const SDraw *d);

/* The colour target, in pixels. */
uint32_t sf_pixel(uint32_t x, uint32_t y);
void sf_fill(uint32_t argb);

#endif
