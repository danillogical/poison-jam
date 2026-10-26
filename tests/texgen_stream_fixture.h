/* The committed-stream fixture shared by the D4 step-6 texgen / texture-matrix
 * gate's two halves (tests/test_texgen_stream.c, CPU; tests/test_texgen_stream_vk.c,
 * Vulkan). It owns no expectation: every literal lives in
 * tests/texgen_stream_cases.h with its derivation, and each half checks its
 * own pixels against those literals.
 *
 * The fixture drives the path the title uses, as tests/bumpenv_stream_fixture.c
 * does: guest RAM laid out by a synthetic section-less XBE,
 * `nv2a_init_standalone`, the pushbuffer window, an instance/RAMHT binding for
 * class NV097, `RECOMP_PB_EXEC=1`, the executor's commit-consumer registration,
 * and real pushbuffer words through `nv2a_submit_pending`. Fixed-function
 * draws set the composite matrix (so the executor's batch_is_ffp holds),
 * TRANSFORM_EXECUTION_MODE 0, a model-view and inverse model-view, a normal
 * array at attribute 2, texgen modes and planes, texture-matrix enables and
 * words, and textures. Nothing here calls an executor or back-end helper.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_TEXGEN_STREAM_FIXTURE_H
#define JSRF_TEXGEN_STREAM_FIXTURE_H

#include <stdint.h>

/* The guest layout. Low RAM, below the contiguous window and clear of the
 * synthetic image at 0x00010000, so dma_resolve passes each offset through. */
#define TGF_SURF_VA     0x00400000u     /* 64x64 A8R8G8B8, pitch 256 */
#define TGF_SURF_W      64u
#define TGF_SURF_H      64u
#define TGF_SURF_PITCH  (TGF_SURF_W * 4u)
#define TGF_POS_VA      0x00420000u     /* attribute 0: six float4 positions */
#define TGF_NRM_VA      0x00420100u     /* attribute 2: six float3 normals */
#define TGF_DIF_VA      0x00420200u     /* attribute 3: six D3DCOLOR diffuse */
#define TGF_TC0_VA      0x00420300u     /* attribute 9: six float4 (or float2) */
#define TGF_TC1_VA      0x00420400u     /* attribute 10: six float4 (or float2) */
#define TGF_TEX_A_VA    0x00440000u     /* 8x8 swizzled A8R8G8B8, blue 0x80 */
#define TGF_TEX_B_VA    0x00441000u     /* 8x8 swizzled A8R8G8B8, blue 0x40 */
#define TGF_TEX_C_VA    0x00442000u     /* 8x8 swizzled A8R8G8B8, blue 0xC0 */
#define TGF_TEX_AL_VA   0x00443000u     /* 8x8 LINEAR copy of A, pitch 32 */

/* Every vertex's diffuse colour. The CPU fixed-function rasteriser writes it
 * for a pixel whose stage 0 is untextured. */
#define TGF_DIFFUSE     0xFF336699u

/* What a stage binds. */
enum {
    TGF_TEX_NONE = 0,   /* OFFSET 0: the stage's texture is invalid */
    TGF_TEX_A,          /* 8x8 SZ_A8R8G8B8 (0x06) at TGF_TEX_A_VA */
    TGF_TEX_B,          /* 8x8 SZ_A8R8G8B8 at TGF_TEX_B_VA */
    TGF_TEX_C,          /* 8x8 SZ_A8R8G8B8 at TGF_TEX_C_VA */
    TGF_TEX_A_LINEAR    /* LIN_A8R8G8B8 (0x12) + CONTROL1 pitch + IMAGE_RECT; stage 0 only */
};

typedef struct {
    const char *what;       /* the assertion's name, printed on failure */
    int ffp;                /* 1: fixed function; 0: the vertex program (oPos = v0, oT0 = c0) */
    uint32_t texgen[3][3];  /* stages 0..2, S T R mode words; stage 3 is sent DISABLE */
    const float *plane[3];  /* stages 0..2: 16 words S, T, R, Q planes; NULL = zeros */
    uint32_t mat_enable[4]; /* SET_TEXTURE_MATRIX_ENABLE, stages 0..3 */
    const float *mat[2];    /* stages 0..1: 16 SET_TEXTURE_MATRIX words; NULL = identity */
    const float *mv;        /* 16 SET_MODEL_VIEW_MATRIX words; NULL = identity */
    const float *imv;       /* 12 SET_INVERSE_MODEL_VIEW_MATRIX words; NULL = identity */
    uint32_t normalize;     /* SET_NORMALIZATION_ENABLE */
    float normal[3];        /* attribute 2 at every vertex */
    int tc_size[2];         /* attributes 9, 10: 0 = no array, else 2 or 4 floats */
    float tc[2][4];         /* their value at every vertex */
    const float *eye;       /* 4 SET_EYE_POSITION words, or NULL (not sent) */
    float vp_t0[4];         /* program draws: c[0], which the program writes to oT0 */
    int tex[3];             /* TGF_TEX_* for stages 0..2 */
    uint32_t prog;          /* SET_SHADER_STAGE_PROGRAM, 5 bits per stage */
    uint32_t out_reg;       /* the final combiner emits this register: 8..10 = t0..t2 */
    uint32_t want;          /* the literal ARGB pixel the hardware rule gives */
    float want_tc[3];       /* the observed stage's s, t, q as the batch carries them */
    int cpu_differs;        /* 1: the CPU fixed-function rasteriser draws want_cpu */
    uint32_t want_cpu;      /* its literal (a labelled CPU limitation) */
    const char *cpu_note;   /* the CPU assertion's name */
} TgDraw;

/* Guest memory, NV2A standalone, RAMHT, commit consumer, the surface, the
 * composite matrix, the position and diffuse arrays, the vertex program and
 * the combiner control. Returns 1 on success; prints the failing step otherwise. */
int tgf_init(const char *tag);

/* Emit the draw's whole state and one six-vertex TRIANGLES batch, then submit.
 * Returns 1 when the walk accepted the stream. */
int tgf_submit_draw(const TgDraw *d);

/* The same stream as tgf_submit_draw with one extra (method, param) pair
 * placed before the batch. Returns 1 when the walk accepted the stream. */
int tgf_submit_draw_with(const TgDraw *d, uint32_t method, uint32_t param);

/* The guest surface. */
uint32_t tgf_pixel(uint32_t x, uint32_t y);
void tgf_fill_surface(uint32_t argb);

#endif
