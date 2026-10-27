/* The committed-stream fixture shared by the NV2A window-clip gate's two halves
 * (tests/test_wclip_stream.c, CPU; tests/test_wclip_stream_vk.c, Vulkan). It owns
 * no expectation: every literal lives in tests/wclip_stream_cases.h with its
 * derivation, and each half checks its own pixels against those literals.
 *
 * The fixture follows tests/texgen_stream_fixture.c: guest RAM laid out by a
 * synthetic section-less XBE, `nv2a_init_standalone`, the pushbuffer window, an
 * instance/RAMHT binding for class NV097, `RECOMP_PB_EXEC=1`, the executor's
 * commit-consumer registration, and real pushbuffer words through
 * `nv2a_submit_pending`. One submit sets the surface, clears it to a background
 * colour, sets the (smaller) surface clip, writes the window-clip words, and
 * draws one untextured fixed-function quad in one solid colour that covers the
 * whole surface. Nothing here calls an executor or back-end helper.
 *
 * It lives in its own translation unit so the Vulkan half, which #includes the
 * production nv2a_vk_backend.c, never includes the NV2A core headers.
 */
#ifndef JSRF_WCLIP_STREAM_FIXTURE_H
#define JSRF_WCLIP_STREAM_FIXTURE_H

#include <stdint.h>

/* The guest layout. Low RAM, below the contiguous window and clear of the
 * synthetic image at 0x00010000, so dma_resolve passes each offset through. The
 * guest surface is always 64x64 REAL pixels, A8R8G8B8, pitch 256; with
 * anti-aliasing the LOGICAL surface (clip, vertices, window clip) is smaller. */
#define WCF_SURF_VA     0x00400000u
#define WCF_SURF_W      64u
#define WCF_SURF_H      64u
#define WCF_SURF_PITCH  (WCF_SURF_W * 4u)
#define WCF_POS_VA      0x00420000u     /* attribute 0: six float4 positions */
#define WCF_DIF_VA      0x00420200u     /* attribute 3: six D3DCOLOR diffuse */

/* What the clear leaves, and what the quad writes where the window clip admits it. */
#define WCF_BACKGROUND  0xFF102030u
#define WCF_QUAD        0xFFE0A040u

/* One window-clip slot write: both words of slot `slot`, which the hardware
 * also lands in every later slot. Only slot 0 is admitted by the walk. */
typedef struct {
    uint32_t slot;                      /* 0 */
    uint32_t xmin, xmax, ymin, ymax;    /* 12-bit fields, logical pixels */
} WcWrite;

typedef struct {
    uint32_t aa;                /* SET_SURFACE_FORMAT bits 12-15: 0 off, 1 = 2x1, 2 = 2x2 */
    uint32_t clip_w, clip_h;    /* the draw's surface clip, logical pixels; 0 = the whole logical surface */
    int has_type;               /* 1: SET_WINDOW_CLIP_TYPE is written */
    uint32_t type;              /* its parameter: 0 inclusive, 1 exclusive */
    uint32_t nwrites;           /* 0 = no window-clip method is written at all */
    WcWrite writes[1];          /* slot 0 only: the walk admits no other slot */
} WcDraw;

/* Guest memory, NV2A standalone, RAMHT and the commit consumer. Returns 1 on
 * success; prints the failing step otherwise. */
int wcf_init(const char *tag);

/* Emit the whole stream for `d` and submit it. Returns 1 when the walk accepted it. */
int wcf_submit_draw(const WcDraw *d);

/* The guest surface, in real pixels. */
uint32_t wcf_pixel(uint32_t x, uint32_t y);
void wcf_fill_surface(uint32_t argb);

#endif
