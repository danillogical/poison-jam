/* The committed-stream fixture shared by the NV2A X8R8G8B8-alias gate's two halves
 * (tests/test_x8alias_stream.c, CPU; tests/test_x8alias_stream_vk.c, Vulkan). It owns
 * no expectation: every literal lives in tests/x8alias_stream_cases.h with its
 * derivation, and each half checks its own pixels against those literals.
 *
 * The fixture follows tests/wclip_stream_fixture.c (guest RAM laid out by a
 * synthetic section-less XBE, `nv2a_init_standalone`, the pushbuffer window, an
 * instance/RAMHT binding for class NV097, `RECOMP_PB_EXEC=1`, the executor's
 * commit-consumer registration, real pushbuffer words through
 * `nv2a_submit_pending`) and tests/texgen_stream_fixture.c (how a LIN_ texture
 * and the vertex-program/combiner path are bound). One submit clears surface A to
 * a colour whose alpha is not 0xFF, then draws one quad textured by A's own
 * address and pitch, into surface B (or, for the self case, into A). Nothing here
 * calls an executor or back-end helper.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_X8ALIAS_STREAM_FIXTURE_H
#define JSRF_X8ALIAS_STREAM_FIXTURE_H

#include <stdint.h>

/* Low RAM, below the contiguous window and clear of the synthetic image at
 * 0x00010000, so dma_resolve passes each offset through. Both surfaces are 32x32
 * linear A8R8G8B8, pitch 128, and do not overlap. */
#define X8F_W           32u
#define X8F_H           32u
#define X8F_PITCH       (X8F_W * 4u)
#define X8F_A_VA        0x00440000u     /* the texture source: cleared, a render target */
#define X8F_B_VA        0x00450000u     /* where the quad is drawn (unless self) */
#define X8F_POS_VA      0x00420000u     /* attribute 0: six float4 positions */
#define X8F_DIF_VA      0x00420200u     /* attribute 3: six D3DCOLOR diffuse */
#define X8F_TC_VA       0x00420300u     /* attribute 9: six float2 (fixed function only) */

/* What the clear leaves in A: alpha 0x40, deliberately not 0xFF. */
#define X8F_CLEAR       0x40336699u

enum { X8F_SURF_A = 0, X8F_SURF_B = 1 };

typedef struct {
    uint32_t fmt;   /* NV097 texture colour format of stage 0: 0x12 LIN_A8R8G8B8, 0x1E LIN_X8R8G8B8 */
    int rc;         /* 0: fixed-function stage 0; 1: vertex program + final combiner emitting t0 */
    int self;       /* 0: the quad covers all of B; 1: it covers x in [0,16) of A, the texture itself */
} X8Draw;

/* Guest memory, NV2A standalone, RAMHT and the commit consumer. Returns 1 on
 * success; prints the failing step otherwise. */
int x8f_init(const char *tag);

/* Emit the whole stream for `d` and submit it. Returns 1 when the walk accepted it. */
int x8f_submit(const X8Draw *d);

/* The guest surfaces, in pixels. */
uint32_t x8f_pixel(uint32_t surf, uint32_t x, uint32_t y);
void x8f_fill(uint32_t argb);       /* both surfaces */

#endif
