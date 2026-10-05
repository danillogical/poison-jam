#include "jsrf_fatal_observe.h"

const char *jsrf_fatal_ctor_ret_name(uint32_t ret_va)
{
    switch (ret_va) {
    case 0x000255B2u:
        return "direct call at 0x255AD in 0x25400 (time delta reached 0xE4E1C0)";
    case 0x000664C8u:
        return "direct call at 0x664C3 (0x257B0 returned >= 2)";
    case 0x000116EADu:
        return "direct call at 0x116EA8 (0x13AA50 returned nonzero)";
    default:
        return "not a direct 0x6F730 return; the only other entry is the tail "
               "jmp at 0x2537E, so this is the return address of the caller of 0x25310";
    }
}
