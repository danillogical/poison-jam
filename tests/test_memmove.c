#include <stdio.h>
#include "recomp/gen/recomp_types.h"

ptrdiff_t g_xbox_mem_offset;
RECOMP_TLS uint32_t g_eax, g_esp;
extern void sub_0017CEC0(void);

static unsigned char guest[8192];
static unsigned char expected[8192];

int main(void)
{
    unsigned cases = 0;
    g_xbox_mem_offset = (ptrdiff_t)guest;
    /* All alignments, tiny jump-table copies, large copies, both overlap
     * directions, equal pointers and disjoint regions. Compare all memory
     * to catch overwrites and verify the cdecl return/stack contract. */
    for (unsigned size = 0; size <= 1024; ++size) {
        for (unsigned alignment = 0; alignment < 4; ++alignment) {
            static const int distances[] = {-2048, -17, -3, -1, 0, 1, 3, 17, 2048};
            for (unsigned d = 0; d < sizeof(distances) / sizeof(distances[0]); ++d) {
                uint32_t source = 3072 + alignment;
                uint32_t destination = (uint32_t)((int)source + distances[d]);
                for (unsigned i = 0; i < sizeof(guest); ++i)
                    guest[i] = (unsigned char)((i * 37 + i / 7) & 255);
                g_esp = 128;
                g_eax = 0xDEADBEEF;
                MEM32(128) = 0x12345678;
                MEM32(132) = destination;
                MEM32(136) = source;
                MEM32(140) = size;
                memcpy(expected, guest, sizeof(guest));
                memmove(expected + destination, expected + source, size);
                sub_0017CEC0();
                if (g_esp != 132 || g_eax != destination ||
                    memcmp(guest, expected, sizeof(guest)) != 0) {
                    fprintf(stderr, "FAIL size=%u alignment=%u distance=%d\n",
                            size, alignment, distances[d]);
                    return 1;
                }
                ++cases;
            }
        }
    }
    printf("PASS: %u memmove cases (bytes, bounds, return value, cdecl stack)\n", cases);
    return 0;
}
