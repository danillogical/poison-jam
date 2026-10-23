/* The committed-stream fixture shared by the D4 step-5 BUMPENVMAP gate's two
 * halves (tests/test_bumpenv_stream.c, CPU; tests/test_bumpenv_stream_vk.c,
 * Vulkan). It owns no expectation: every literal lives in
 * tests/bumpenv_stream_cases.h with its derivation, and each half checks its
 * own pixels against those literals.
 *
 * The fixture drives the path the title uses: guest RAM laid out by a
 * synthetic section-less XBE, `nv2a_init_standalone`, the pushbuffer window,
 * an instance/RAMHT binding for class NV097, `RECOMP_PB_EXEC=1`, the
 * executor's commit-consumer registration, and real pushbuffer words through
 * `nv2a_submit_pending` -- the same owner path as tests/test_pbcov_stream.c.
 * Nothing here calls an executor or back-end helper directly.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never has to include the NV2A core headers
 * next to it.
 */
#ifndef JSRF_BUMPENV_STREAM_FIXTURE_H
#define JSRF_BUMPENV_STREAM_FIXTURE_H

#include <stdint.h>

/* The guest layout. Low RAM, below the contiguous window and clear of the
 * synthetic image at 0x00010000, so dma_resolve passes each offset through. */
#define BSF_SURF_VA     0x00400000u     /* 64x64 A8R8G8B8, pitch 256 */
#define BSF_SURF_W      64u
#define BSF_SURF_H      64u
#define BSF_SURF_PITCH  (BSF_SURF_W * 4u)
#define BSF_VB_VA       0x00420000u     /* six float4 positions */
#define BSF_TEX_D_VA    0x00440000u     /* 8x8 swizzled A8R8G8B8: the bump destination */
#define BSF_TEX_S_VA    0x00442000u     /* 4x4 swizzled A8R8G8B8: the delta sources */
#define BSF_TEX_DLIN_VA 0x00443000u     /* 8x8 LINEAR A8R8G8B8 copy of D, pitch 32 */
#define BSF_TEX_G8B8_VA 0x00444000u     /* 4x4 swizzled G8B8 (color 0x28): V8U8's code */

/* What a stage binds. Stage 3 can bind nothing: the walk admits no stage-3
 * SET_TEXTURE_OFFSET (0x1BC0) or SET_TEXTURE_FORMAT (0x1BC4); see
 * limitation_stage3_texture in bumpenv_stream_cases.h. */
enum {
    BSF_TEX_NONE = 0,   /* OFFSET 0: the stage's texture is invalid */
    BSF_TEX_D,          /* 8x8 SZ_A8R8G8B8 (0x06) at BSF_TEX_D_VA */
    BSF_TEX_S,          /* 4x4 SZ_A8R8G8B8 (0x06) at BSF_TEX_S_VA */
    BSF_TEX_D_CUBE,     /* D with FORMAT's cube bit (bit 2) set */
    BSF_TEX_D_VOLUME,   /* D with FORMAT dimensionality 3 and BASE_SIZE_P 1 (depth 2) */
    BSF_TEX_D_LINEAR,   /* LIN_A8R8G8B8 (0x12) + CONTROL1 pitch + IMAGE_RECT; stage 0 only */
    BSF_TEX_G8B8        /* 4x4 swizzled color 0x28 (G8B8, the code D3D uses for V8U8) */
};

/* What the Vulkan half must observe for a draw. */
enum {
    BSF_VK_RC = 0,          /* drawn by the combiner pipeline: rc_draws +1, rc_fallback +0 */
    BSF_VK_FALLBACK_MODE,   /* refused, reason 1 (a stage mode 8..31): rc_fallback +1 */
    BSF_VK_FALLBACK_CUBE    /* refused, reason 2 (mode 3 with a true cube texture) */
};

typedef struct {
    const char *what;       /* the assertion's name, printed on failure */
    uint32_t prog;          /* SET_SHADER_STAGE_PROGRAM, 5 bits per stage */
    uint32_t osi;           /* SET_SHADER_OTHER_STAGE_INPUT */
    int tex[3];             /* stages 0..2 */
    float tc[4][4];         /* each stage's texture coordinate s, t, r, q */
    float mat[4][4];        /* CANONICAL m00, m01, m10, m11 per stage (0 unused) */
    float scale[4];         /* BUMPENVLSCALE per stage (0 unused) */
    float offset[4];        /* BUMPENVLOFFSET per stage (0 unused) */
    uint32_t out_reg;       /* final combiner emits this register: 8..11 = t0..t3 */
    uint32_t final_c0;      /* 0: out = t; else out.rgb = t * c0 (c0 = this D3DCOLOR) */
    uint32_t want;          /* the literal ARGB pixel */
    int vk;                 /* BSF_VK_* */
    uint32_t vk_stage;      /* the refusing stage, for BSF_VK_FALLBACK_* */
    uint32_t vk_mode;       /* the refusing mode, for BSF_VK_FALLBACK_* */
} BsfDraw;

/* Guest memory, NV2A standalone, RAMHT, commit consumer, the vertex program
 * and the surface. Returns 1 on success; prints the failing step otherwise. */
int bsf_init(const char *tag);

/* Emit the draw's whole state and one six-vertex TRIANGLES batch, then submit.
 * Returns 1 when the walk accepted the stream. */
int bsf_submit_draw(const BsfDraw *d);

/* The same stream as bsf_submit_draw with one extra (method, param) pair
 * placed before the batch. Returns 1 when the walk accepted the stream. */
int bsf_submit_draw_with(const BsfDraw *d, uint32_t method, uint32_t param);

/* The guest surface. */
uint32_t bsf_pixel(uint32_t x, uint32_t y);
void bsf_fill_surface(uint32_t argb);

#endif
