#include <stdio.h>
#include <string.h>
#include "jsrf_fatal_observe.h"

/* The four 0x6F730 entries are distinguished by the return address the lift
 * leaves at [esp]. These strings are what a run log is read against, so a
 * renamed or swapped site has to fail here rather than in a 20-minute run. */
int main(void)
{
    const char *direct_255 = jsrf_fatal_ctor_ret_name(0x000255B2u);
    const char *direct_664 = jsrf_fatal_ctor_ret_name(0x000664C8u);
    const char *direct_116 = jsrf_fatal_ctor_ret_name(0x000116EADu);
    const char *tail = jsrf_fatal_ctor_ret_name(0x00123456u);
    int failed = 0;

    if (!direct_255 || !strstr(direct_255, "0x255AD") || strstr(direct_255, "0x2537E"))
        failed = 1;
    if (!direct_664 || !strstr(direct_664, "0x664C3") || strstr(direct_664, "0x2537E"))
        failed = 1;
    if (!direct_116 || !strstr(direct_116, "0x116EA8") || strstr(direct_116, "0x2537E"))
        failed = 1;
    if (!tail || !strstr(tail, "0x2537E") || !strstr(tail, "0x25310"))
        failed = 1;
    /* The file-write return is not a constructor entry. Naming it as one
     * would make a file-open stack look like the tail path. */
    if (strcmp(jsrf_fatal_ctor_ret_name(0x0006EE73u), tail) != 0)
        failed = 1;
    if (failed) {
        fprintf(stderr, "fatal ret name mismatch\n");
        return 1;
    }
    return 0;
}
