#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp/gen/recomp_types.h"
#include "diagnostics.h"

/* Slots are never recycled. Thus an exited TLS address cannot be mistaken for
 * a new thread. Only the owning thread writes a slot. External readers MUST
 * freeze the process at a debug event; no in-process live snapshot is allowed.
 * Odd sequence numbers mark events interrupted midway through publication. */
JsrfRegistry g_jsrf_debug = {1};
static RECOMP_TLS JsrfThread *current;

void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{
    LONG slot;
    LARGE_INTEGER frequency;
    if (current) return;
    slot = InterlockedIncrement((volatile LONG *)&g_jsrf_debug.claimed) - 1;
    if (slot >= JSRF_THREAD_CAPACITY) {
        InterlockedIncrement((volatile LONG *)&g_jsrf_debug.overflow);
        return;
    }
    QueryPerformanceFrequency(&frequency);
    InterlockedCompareExchange64((volatile LONG64 *)&g_jsrf_debug.frequency, frequency.QuadPart, 0);
    current = &g_jsrf_debug.threads[slot];
    current->tid = GetCurrentThreadId(); current->identity = (uint32_t)slot + 1;
    current->start = start; current->stack_low = low; current->stack_high = high;
    uint32_t *regs[] = {&g_eax,&g_ecx,&g_edx,&g_esp,&g_ebx,&g_esi,&g_edi,&g_ebp,&g_seh_ebp,&g_fs_base};
    for (unsigned i=0;i<JSRF_REGISTER_COUNT;i++) current->registers[i]=(uint64_t)(uintptr_t)regs[i];
    InterlockedExchange((volatile LONG *)&current->state, 1);
    recomp_diag_record(JSRF_THREAD_START, start, 0, 0);
}

void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site, uint32_t value)
{
    LARGE_INTEGER now;
    JsrfEvent *event;
    uint32_t count;
    if (!current) return;
    count = current->count;
    event = &current->events[count % JSRF_EVENT_CAPACITY];
    InterlockedExchange((volatile LONG *)&event->sequence, (LONG)(count * 2 + 1));
    QueryPerformanceCounter(&now);
    event->ticks = (uint64_t)now.QuadPart; event->kind = kind;
    event->target = target; event->site = site; event->esp = g_esp; event->value = value;
    InterlockedExchange((volatile LONG *)&event->sequence, (LONG)(count * 2 + 2));
    InterlockedExchange((volatile LONG *)&current->count, (LONG)(count + 1));
}

void recomp_diag_thread_end(void)
{
    if (!current) return;
    recomp_diag_record(JSRF_THREAD_EXIT, 0, 0, 0);
    InterlockedExchange((volatile LONG *)&current->state, 2);
    current = NULL;
}

/* Temporary investigation hook, disabled unless JSRF_TRACE_HEAP is set.
 * Arguments and registers are observed, never changed. Capped per thread. */
void jsrf_trace_heap(uint32_t site, uint32_t frame)
{
    static RECOMP_TLS int enabled = -1;
    static RECOMP_TLS unsigned count;
    if (enabled < 0) enabled = getenv("JSRF_TRACE_HEAP") != NULL;
    if (!enabled || count++ >= 8192) return;
    uint32_t heap = MEM32(frame+8), head=heap+0x180;
    uint32_t first=MEM32(head), last=MEM32(head+4);
    _lock_file(stderr);
    fprintf(stderr,"[HEAP] tid=%lu n=%u site=%08X heap=%08X size=%X bp=%08X ax=%08X cx=%08X dx=%08X si=%08X di=%08X head=%08X,%08X",
        GetCurrentThreadId(),count,site,heap,MEM32(frame+16),frame,g_eax,g_ecx,g_edx,g_esi,g_edi,first,last);
    if (first>=0x10000 && first<0x4000000-8)
        fprintf(stderr," links=%08X,%08X chunk=%08X",MEM32(first),MEM32(first+4),MEM32(first-8));
    fprintf(stderr," caller=%08X\n",MEM32(frame+4));
    _unlock_file(stderr);
    if (site==0x149F6Au && MEM32(frame+16)>=0x10010 && !(MEM8(MEM32(frame+16)-11)&1))
        RaiseException(0xE0424845u,EXCEPTION_NONCONTINUABLE,0,NULL);
}
