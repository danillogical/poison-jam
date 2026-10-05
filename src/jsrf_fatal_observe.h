#ifndef JSRF_FATAL_OBSERVE_H
#define JSRF_FATAL_OBSERVE_H
#include <stdint.h>

/* Name the return address at the entry of 0x6F730.
 *
 * An XBE scan of every E8/E9 in the file-backed sections found exactly four
 * entries: calls at 0x255AD, 0x664C3 and 0x116EA8, and a tail jmp at 0x2537E.
 * No dword in the image holds 0x6F730, so there is no pointer-table entry.
 * The tail jmp restores the guest stack first, so the return address seen at
 * 0x6F730 is the caller of 0x25310, not 0x2537E. */
const char *jsrf_fatal_ctor_ret_name(uint32_t ret_va);

#endif
