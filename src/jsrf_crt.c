#include "recomp/gen/recomp_types.h"
#include <stdio.h>

/* 0x0017CEC0: CRT memmove, confirmed by the XBE's overlap check and
 * forward/backward copy paths. The generated body truncates its backward
 * jump table at 0x0017D15A and dispatches internal labels as functions.
 * cdecl: [esp+4]=dst, [esp+8]=src, [esp+12]=size; return dst in eax.
 * Only RET's four bytes belong to the callee. Preserve nonvolatile registers.
 */
void sub_0017CEC0(void)
{
    uint32_t destination = MEM32(g_esp + 4);
    uint32_t source = MEM32(g_esp + 8);
    uint32_t size = MEM32(g_esp + 12);
    static RECOMP_TLS int reported;

    if (size != 0)
        memmove((void *)XBOX_PTR(destination), (const void *)XBOX_PTR(source), size);
    g_eax = destination;
    g_esp += 4;
    if (!reported) {
        reported = 1;
        fprintf(stderr, "[JSRF] memmove 0x0017CEC0: copied %u bytes "
                "0x%08X -> 0x%08X; returned with esp=0x%08X\n",
                size, source, destination, g_esp);
    }
}
