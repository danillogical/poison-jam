/* The committed-stream fixture shared by the NV2A per-zeta depth gate's two halves
 * (tests/test_zeta_stream.c, CPU; tests/test_zeta_stream_vk.c, Vulkan). It owns no
 * expectation: every literal lives in tests/zeta_stream_cases.h with its derivation,
 * and each half checks its own pixels against those literals.
 *
 * The fixture follows tests/x8alias_stream_fixture.c (guest RAM laid out by a
 * synthetic section-less XBE, `nv2a_init_standalone`, the pushbuffer window, an
 * instance/RAMHT binding for class NV097, `RECOMP_PB_EXEC=1`, the executor's
 * commit-consumer registration, real pushbuffer words through
 * `nv2a_submit_pending`). One submit uses a single colour target C with depth test
 * LESS and depth writes on, and switches SET_SURFACE_ZETA_OFFSET between three
 * steps (see zeta_stream_cases.h). Nothing here calls an executor or back-end helper.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_ZETA_STREAM_FIXTURE_H
#define JSRF_ZETA_STREAM_FIXTURE_H

#include <stdint.h>

/* Low RAM, below the contiguous window and clear of the synthetic image at
 * 0x00010000, so dma_resolve passes each offset through. C is 32x32 linear
 * A8R8G8B8, pitch 128. The zeta addresses are only ever names: nothing reads or
 * writes guest memory there. */
#define ZF_W            32u
#define ZF_H            32u
#define ZF_PITCH        (ZF_W * 4u)
#define ZF_C_VA         0x00440000u     /* the colour target, shared by every step */
#define ZF_Z1_VA        0x00460000u     /* the first zeta address */
#define ZF_Z2_VA        0x00470000u     /* the other zeta address (case distinct) */
#define ZF_POS_P_VA     0x00420000u     /* attribute 0: quad P, six float4 positions */
#define ZF_DIF_P_VA     0x00420200u     /* attribute 3: quad P, six D3DCOLOR diffuse */
#define ZF_POS_Q_VA     0x00421000u     /* quad Q, same layout */
#define ZF_DIF_Q_VA     0x00421200u

/* The scene. Depth units are the Z24 scale SET_CLIP_MAX announces (2^24 - 1). */
#define ZF_Z_SCALE      16777215.0f
#define ZF_Z_NEAR       4194303.75f     /* 0.25 of the scale, exact in a float */
#define ZF_Z_MID        8388607.5f      /* 0.5 of the scale, exact in a float */
#define ZF_ZCLEAR       0xFFFFFF00u     /* SET_ZSTENCIL_CLEAR_VALUE: Z24 = 0xFFFFFF (1.0), stencil 0 */
#define ZF_BG_ARGB      0xFF102030u     /* the colour clear */
#define ZF_P_ARGB       0xFF4488CCu     /* quad P, depth ZF_Z_NEAR */
#define ZF_Q_ARGB       0xFFEE7711u     /* quad Q, depth ZF_Z_MID */
#define ZF_P_W          16.0f           /* P covers x in [0, 16) */
#define ZF_Q_W          24.0f           /* Q covers x in [0, 24) */

typedef struct {
    uint32_t mid_zeta;  /* zeta address of the middle (clear) step: ZF_Z2_VA, or ZF_Z1_VA */
    int program;        /* 0: fixed function; 1: vertex program oPos = v0, oD0 = v3 */
} ZDraw;

/* Guest memory, NV2A standalone, RAMHT and the commit consumer. Returns 1 on
 * success; prints the failing step otherwise. */
int zf_init(const char *tag);

/* Emit the whole stream for `d` and submit it. Returns 1 when the walk accepted it. */
int zf_submit(const ZDraw *d);

/* The colour target, in pixels. */
uint32_t zf_pixel(uint32_t x, uint32_t y);
void zf_fill(uint32_t argb);

#endif
