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

/* Sequenced temporary probe: prints a running count so multiplicity and ordering
 * are visible. A single-shot probe cannot tell "the constructor returned 0x38"
 * from "the store ran twice and the second one wrote 0x38". */
void jsrf_trace_seq(uint32_t site, uint32_t value)
{
    static RECOMP_TLS unsigned count;
    if (count++ >= 2000) return;
    _lock_file(stderr);
    fprintf(stderr, "[SEQ] n=%u site=%08X value=%08X esp=%08X esi=%08X edi=%08X eax=%08X\n",
            count, site, value, g_esp, g_esi, g_edi, g_eax);
    _unlock_file(stderr);
}

/* Reports a callee whose ESP delta the table does not allow. Only reachable with
 * -DRECOMP_ABI_CHECK; see scripts/gen-abi-deltas.py for why the check exists. */
void jsrf_trace_delta_mismatch(uint32_t site, uint32_t va, uint32_t actual)
{
    /* Per-callee, not global. The first version capped the whole trace at 300
     * lines, and the two Windows SEH helpers -- which legitimately adjust the
     * stack themselves -- produced 151 + 149 = exactly 300. The cap was therefore
     * exhausted by known-benign noise and a real mismatch (sub_001680D0, delta 24
     * where its body allows 16) was dropped unreported. Every distinct callee now
     * gets named.
     *
     * Prints the observed delta AND the delta the table allows, because "the delta
     * is wrong" does not say by how much or in which direction, and the size of
     * the error is what identifies the instruction that caused it. */
    enum { SLOTS = 64, PER_CALLEE = 2 };
    static RECOMP_TLS uint32_t seen[SLOTS];
    static RECOMP_TLS unsigned hits[SLOTS];
    static RECOMP_TLS int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va) break;
    if (i == count) {
        if (count == SLOTS) return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    if (hits[i]++ >= PER_CALLEE) return;
    _lock_file(stderr);
    {
        uint32_t allowed[4];
        int n = recomp_delta_allowed(va, allowed), k;
        fprintf(stderr, "[DELTA] site=%08X callee=%08X actual=%u expected=",
                site, va, actual);
        if (!n) fprintf(stderr, "?");
        for (k = 0; k < n; k++) fprintf(stderr, "%s%u", k ? "," : "", allowed[k]);
        fprintf(stderr, " esp=%08X hit=%u\n", g_esp, hits[i]);
    }
    _unlock_file(stderr);
}

void recomp_diag_thread_end(void)
{
    if (!current) return;
    recomp_diag_record(JSRF_THREAD_EXIT, 0, 0, 0);
    InterlockedExchange((volatile LONG *)&current->state, 2);
    current = NULL;
}

/* A4b observation — re-apply after regeneration.
 *
 * The A4b2 watch-ledger seam. This is a THIN FORWARDER and nothing else: no
 * filtering, no cap, no output, no decision logic, and no value changes. It sets
 * the toolkit's anchor site once and then forwards every call to the toolkit's
 * apu_watch_cpu_store(), which itself ignores any call whose target_va is not
 * MEM32(0x001BA858)+0x810. It is called BEFORE the guest store it observes, so
 * the recorded value is the value about to land (record-before-store, Device
 * semantics 6).
 *
 * The anchor site is the guest's control store of the pending word; the toolkit
 * hard-codes no site, so A4b2 passes it here. Re-setting it is idempotent, so the
 * one-shot flag needs no lock: a lost race only writes the same constant twice. */
extern void apu_watch_set_anchor_site(uint32_t site_va);
extern void apu_watch_cpu_store(uint32_t site_va, uint32_t target_va, uint32_t value);

void jsrf_watch_store(uint32_t site_va, uint32_t target_va, uint32_t value)
{
    static int anchor_set;
    if (!anchor_set) {
        anchor_set = 1;
        apu_watch_set_anchor_site(0x001A18CEu);
    }
    apu_watch_cpu_store(site_va, target_va, value);
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
