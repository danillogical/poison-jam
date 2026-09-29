#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <windows.h>
#include "recomp/gen/recomp_types.h"
#include "diagnostics.h"
#include "xbox_memory_layout.h"

extern void sub_0017CEC0(void);
extern recomp_func_t jsrf_lookup_recovered(uint32_t va);

/* 0x00162B9D: a shared COM error tail, `mov eax, 0x800401F0; ret 0xc`
 * (CO_E_NOTINITIALIZED). Its two callers, 0x00162A20 and 0x00162AB0, are
 * recovered bodies that end where it starts and tail-jump to it. The v0.12
 * translator folds such an abutting tail into its parents, and those parents
 * are not generated, so nothing emitted it any more (regeneration of
 * 2026-09-28). stdcall: the caller's return address plus 12 bytes. */
void sub_00162B9D(void)
{
    g_eax = 0x800401F0u;
    g_esp += 4 + 12;
}

recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    if (xbox_va == 0x0017CEC0u)
        return sub_0017CEC0;
    if (xbox_va == 0x00162B9Du)
        return sub_00162B9D;
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
     * exactly why the two can be compared.
     *
     * GATED ON EITHER DIAGNOSTIC. The terminal witness belongs to the version-3 WRITE watch, so it
     * must be produced whenever that watch is on -- not only when the older boundary-sampling gate
     * happens to be set too. Tying it to the old gate alone would let a run that armed the write
     * watch silently omit the one record the packet requires at the fatal moment. */
    if (getenv("JSRF_TRACE_A2H_SLOT") || getenv("JSRF_TRACE_A2H_DR")) {
        jsrf_slot_watch_terminal(va);
        fprintf(stderr, "  [A2HSLOT] terminal tid=%lu target=%08X slot=%08X live=%08X call=#%u observed=%u\n",
            GetCurrentThreadId(), va, 0x001C4064u, MEM32(0x001C4064u),
            jsrf_slot_latch_self_call_index(), jsrf_slot_latch_self_observed());
    }

    /* ── THE FOURTH READ, AND THE TERMINAL MARKER ──────────────────────────────────────────────
     *
     * THIS IS THE READ SITE'S OWN SEAM. `0x00193E62 mov eax,[esi+0x1C4]` loads the poll-callback
     * slot; `0x00193E68 test eax,eax` skips the call when it is zero; `0x00193EB5 call eax` calls it
     * otherwise. When that value is NOT code -- the observed failure, target 0x001D5078 -- the
     * generated sequence routes through the hook that reaches this function, so `va` IS the value
     * that read produced rather than a reconstruction of it:
     *
     *     loc_00193E62: eax = MEM32(esi + 0x1C4); ... RECOMP_ICALL_SAFE(eax, ...)
     *
     * THE READ INDEX IS NOT GUESSED. The value the installer deposits at the slot is 0x0015F9D0,
     * whose body is `inc dword ptr [0x265174]; ret` (verified from the original XBE), so the dword
     * at guest 0x00265174 counts exactly the NON-FATAL consumptions of this same call site. A raise
     * is therefore read number `counter + 1`: the guest's own count, read live, rather than a
     * sampled log's. Nothing is written to that counter and nothing about the call is altered.
     *
     * GATED SEPARATELY from the two older gates. This record belongs to the live slot-write watch, so
     * it must appear whenever that watch is on and must be absent when it is off; the environment is
     * read through the toolkit so the gate is consulted exactly once. */
    if (xbox_A2hSlotWatchEnabled()) {
        uint32_t handler_calls = MEM32(0x00265174u);
        xbox_A2hSlotWatchNoteFourthRead(va, handler_calls + 1u);
        xbox_A2hSlotWatchTerminal(va);
    }
    fflush(stderr);
    /* Preserve the first bad call before SAFE fallback can rewind arguments
     * or later execution can obscure its cause. Explicit opt-out is diagnostic. */
    if (!getenv("JSRF_ALLOW_UNRESOLVED"))
        RaiseException(0xE0424943u, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

/* Untranslated instructions (upstream v0.12 lifter, ee4eb97).
 *
 * The lifter emits RECOMP_UNIMPL(text, va) at every instruction it has no
 * translation for, where it used to leave a bare comment. The instruction is
 * still a no-op -- exactly what the older tree did silently -- so behaviour is
 * unchanged; this only makes the omission visible when it is reached.
 * RECOMP_UNIMPL_TRAP=1 stops at the first one, at the guest address of the
 * cause rather than wherever its damage surfaces. Contract copied from the
 * toolkit's templates/new-game/src/recomp_manual.c. */
void recomp_unimpl(const char *text, uint32_t va)
{
    static int printed;
    const char *trap = getenv("RECOMP_UNIMPL_TRAP");
    int stop = trap && *trap && *trap != '0';

    if (printed < 50 || stop) {
        printed++;
        fprintf(stderr,
                "[UNIMPL] untranslated instruction REACHED: `%s` at 0x%08X"
                " (a no-op; set RECOMP_UNIMPL_TRAP=1 to stop here)\n",
                text, va);
        fflush(stderr);
    }
    if (stop)
        abort();
}
