#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include "recomp/gen/recomp_types.h"

extern void sub_0017CEC0(void);
extern recomp_func_t jsrf_lookup_recovered(uint32_t va);

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    if (xbox_va == 0x0017CEC0u)
        return sub_0017CEC0;
    return jsrf_lookup_recovered(xbox_va);
}

void recomp_icall_fail_log(uint32_t va)
{
    _lock_file(stderr);
    fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (thread calls: %llu, tid=%lu, ms=%llu)\n",
        va, (unsigned long long)g_icall_count, GetCurrentThreadId(),
        (unsigned long long)GetTickCount64());
    for (int i = 0; i < 16; i++) {
        int index = (int)((g_icall_trace_idx - 16 + i) & 15);
        if (g_icall_trace[index])
            fprintf(stderr, "  [%2d] 0x%08X\n", i, g_icall_trace[index]);
    }
    fflush(stderr);
    _unlock_file(stderr);
    if (!getenv("JSRF_ALLOW_UNRESOLVED"))
        RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

void recomp_icall_not_code_log(uint32_t va)
{
    fprintf(stderr, "[ICALL] invalid target 0x%08X tid=%lu esp=%08X return=%08X\n",
        va, GetCurrentThreadId(), g_esp, MEM32(g_esp));
    fflush(stderr);
    /* Preserve the first bad call before SAFE fallback can rewind arguments
     * or later execution can obscure its cause. Explicit opt-out is diagnostic. */
    if (!getenv("JSRF_ALLOW_UNRESOLVED"))
        RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);
}
