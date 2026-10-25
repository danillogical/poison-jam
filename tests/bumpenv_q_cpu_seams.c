/* Link seams for tests/test_bumpenv_q_cpu.c, which #includes the production executor.
 *
 * The v0.16.0 executor reads three runtime counters whose real definitions live in
 * the kernel bridge, the guest memory layout and the pushbuffer scanner; linking
 * those would pull the whole runtime into a fixture that only tests the texture
 * fetch rule. The toolkit's own executor fixture (tests/nv2a_raster_batch) answers
 * them the same way. None of them is on the fetch path this fixture asserts.
 */
#include <stdint.h>

/* No vblank has been raised: the fixture never presents. */
uint32_t xbox_VblankCount(void) { return 0; }

/* The runtime owns no frame counters here: there is no guest memory layout. */
uint32_t xbox_Nv2aFrameCountersOwned(void) { return 0; }

/* The pushbuffer scanner is never on the stack in this fixture. */
int nv2a_pb_scan_in_call(void) { return 0; }
