#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include "recomp/gen/recomp_types.h"
#include "diagnostics.h"

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

    /* A2h terminal witness: the raw target the generated code actually passed in, the LIVE value of
     * the kernel thunk slot it called through, and g_xbox_mem_offset -- all re-read at this moment.
     *
     * The record is written FIRST and unconditionally, because the raise below is
     * EXCEPTION_NONCONTINUABLE and a log line can be lost to the kernel-log budget, to concurrent
     * stderr, or to the raise terminating before a flush. The record is a fixed field block inside
     * g_jsrf_debug, which the collector archives by symbol, so it survives the raise. The print is
     * corroboration only.
     *
     * `va` is the target the generated code passed to this hook; the hook does NOT re-read it and
     * does not substitute its own read for the generated one. The slot read inside
     * jsrf_slot_watch_terminal is a SEPARATE, independent read of the same live slot, which is
     * exactly why the two can be compared. Off unless JSRF_TRACE_A2H_SLOT is set. */
    if (getenv("JSRF_TRACE_A2H_SLOT")) {
        jsrf_slot_watch_terminal(va);
        fprintf(stderr, "  [A2HSLOT] terminal tid=%lu target=%08X slot=%08X live=%08X call=#%u observed=%u\n",
            GetCurrentThreadId(), va, 0x001C4064u, MEM32(0x001C4064u),
            jsrf_slot_latch_self_call_index(), jsrf_slot_latch_self_observed());
    }
    fflush(stderr);
    /* Preserve the first bad call before SAFE fallback can rewind arguments
     * or later execution can obscure its cause. Explicit opt-out is diagnostic. */
    if (!getenv("JSRF_ALLOW_UNRESOLVED"))
        RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);
}
