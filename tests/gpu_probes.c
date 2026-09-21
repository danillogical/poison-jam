/* Synthetic queue observations only: this fixture does not execute NV2A. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <xbox/xboxrecomp.h>
#include "nv2a_mmio_hook.h"
#include "nv2a_regs.h"
#include "diagnostics.h"
extern ptrdiff_t g_xbox_mem_offset;
extern RECOMP_TLS uint32_t g_esp, g_eax, g_fs_base;
extern volatile LONG g_nv2a_ack_enabled;
uint32_t g_jsrf_gpu_fixture;
extern uint8_t g_nv2a_mmio_snapshot[];
extern volatile LONG g_nv2a_mmio_snapshot_generation;
#define GPU_WORD(va) (*(volatile uint32_t *)(g_xbox_mem_offset+(uint32_t)(va)))
static volatile LONG64 g_ptimer_fixture_ns;
static volatile LONG g_ptimer_notifier_stop;

static uint64_t gpu_ptimer_clock(void *opaque)
{
    (void)opaque;
    return (uint64_t)InterlockedCompareExchange64(&g_ptimer_fixture_ns, 0, 0);
}

static int gpu_ptimer_snapshot(uint32_t *pending, uint32_t *pmc, LONG *generation)
{
    for (unsigned i=0; i<1000; ++i) {
        LONG before=InterlockedCompareExchange(
            &g_nv2a_mmio_snapshot_generation,0,0);
        if (before & 1) { Sleep(0); continue; }
        MemoryBarrier();
        memcpy(pending,g_nv2a_mmio_snapshot+0x009100,sizeof(*pending));
        memcpy(pmc,g_nv2a_mmio_snapshot+0x000100,sizeof(*pmc));
        MemoryBarrier();
        LONG after=InterlockedCompareExchange(
            &g_nv2a_mmio_snapshot_generation,0,0);
        if (before==after && !(after & 1)) {
            if (generation) *generation=after;
            return 1;
        }
    }
    return 0;
}

static DWORD WINAPI gpu_ptimer_notifier(void *opaque)
{
    (void)opaque;
    while (!InterlockedCompareExchange(&g_ptimer_notifier_stop,0,0)) {
        nv2a_hook_notify_ptimer_clock_changed();
        Sleep(0);
    }
    return 0;
}

static void gpu_checkpoint(unsigned tag)
{
    ULONG_PTR data = tag;
    __try { RaiseException(0xE0424750,0,1,&data); }
    __except(GetExceptionCode()==0xE0424750 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
}

typedef struct { uint32_t top, tib; int unreadable; } GpuWorker;
typedef struct {
    volatile uint32_t *reg;
    uint32_t seed;
    volatile LONG *failure;
} MmioOwnerWorker;

static DWORD WINAPI gpu_mmio_owner_worker(void *arg)
{
    MmioOwnerWorker *worker = arg;
    for (uint32_t i=0; i<1000; ++i) {
        uint32_t value=worker->seed+i;
        *worker->reg=value;
        if (*worker->reg!=value) InterlockedExchange(worker->failure,1);
    }
    return 0;
}

static DWORD WINAPI gpu_probe_wait(void *arg)
{
    GpuWorker *worker = arg;
    g_esp=worker->top; g_fs_base=worker->tib; g_eax=0x47505557;
    recomp_diag_thread_start(0x47505557, worker->top-XBOX_WORKER_STACK_SIZE+16,worker->top+16);
    recomp_diag_record(JSRF_WAIT_BEGIN,0xFD800044,0x47505557,0);
    for (;;) {
        /* A protected aperture probe must not dereference it in the target. */
        if (!worker->unreadable && GPU_WORD(0xFD800044)==GPU_WORD(0xFD800040)) {
            fprintf(stderr,"GPU PROBE FAIL: stalled queue was acknowledged\n");
            RaiseException(0xE0424751,EXCEPTION_NONCONTINUABLE,0,NULL);
        }
        Sleep(1);
    }
}

int jsrf_probe_gpu(const char *mode)
{
    uint32_t begin;
    DWORD old_protect;
    GpuWorker worker;
    if (g_nv2a_ack_enabled) {
        fprintf(stderr,"GPU probes require RECOMP_GPU_ACK=0 before initialization\n"); return 2;
    }
    if (!strcmp(mode, "gpu-submit-supported") ||
        !strcmp(mode, "gpu-submit-bound") ||
        !strcmp(mode, "gpu-submit-blocked")) {
        NV2AHookSubmissionSnapshot before, after;
        MEMORY_BASIC_INFORMATION mbi;
        uint32_t begin_va = xbox_ContiguousAlloc(256, 256);
        uint32_t begin_phys;
        int blocked = !strcmp(mode, "gpu-submit-blocked");
        int bound = !strcmp(mode, "gpu-submit-bound");
        if (!begin_va) return 2;
        begin_phys = begin_va - 0x80000000u;
        g_jsrf_gpu_fixture = 1;
        if (bound && (!nv2a_set_fixture_execution(nv2a_get_state(), true) ||
                      !nv2a_set_fixture_binding(nv2a_get_state(), 0, 0x1234, 0x97))) return 2;
        GPU_WORD(0x19B224) = begin_va;
        GPU_WORD(0x19B228) = begin_va + 256;
        GPU_WORD(0x19B200) = begin_va + 16;
        if (VirtualQuery((void *)(g_xbox_mem_offset + 0xFD800040u),
                         &mbi, sizeof(mbi)) != sizeof(mbi) ||
            !(mbi.Protect & PAGE_NOACCESS)) return 1;
        memset((void *)(g_xbox_mem_offset + begin_va), 0, 256);
        if (blocked) {
            GPU_WORD(begin_va) = (1u << 18) | 0x100u;
            GPU_WORD(begin_va + 4) = 0x11112222u;
            GPU_WORD(begin_va + 8) = (1u << 18) | (5u << 13) | 0x180u;
            GPU_WORD(begin_va + 12) = 0xabcdef01u;
        } else if (bound) {
            GPU_WORD(begin_va) = (1u << 18) | 0x0000u;
            GPU_WORD(begin_va + 4) = 0x1234u;
            GPU_WORD(begin_va + 8) = (2u << 18) | 0x0200u;
            GPU_WORD(begin_va + 12) = 0x00100020u;
            GPU_WORD(begin_va + 16) = 0x00200040u;
        } else {
            GPU_WORD(begin_va) = (1u << 18) | 0x100u;
            GPU_WORD(begin_va + 4) = 0x11112222u;
        }
        if (!nv2a_hook_get_submission_snapshot(&before)) return 2;
            GPU_WORD(0xFD800044) = begin_phys;
            GPU_WORD(0xFD800040) = begin_phys +
                (blocked ? 16u : (bound ? 20u : 8u));
        if (!nv2a_hook_get_submission_snapshot(&after)) return 2;
        if (blocked) {
            if (after.get != begin_phys || after.put != begin_phys + 16u ||
                after.sink_count != before.sink_count ||
                after.successes != before.successes ||
                strcmp(after.diagnostic, "unsupported_method") ||
                after.diagnostic_subchannel != 5u ||
                after.diagnostic_method != 0x180u ||
                after.diagnostic_param != 0xabcdef01u) return 1;
        } else {
            uint32_t expected_bytes = bound ? 20u : 8u;
            if (after.get != begin_phys + expected_bytes || after.put != after.get ||
                after.sink_count != before.sink_count + (bound ? 3u : 1u) ||
                after.successes != before.successes + 1u ||
                after.last_method != (bound ? 0x204u : 0x100u) ||
                after.last_param != (bound ? 0x00200040u : 0x11112222u) ||
                strcmp(after.diagnostic, "ok")) return 1;
            if (bound) {
                NV2AState *state = nv2a_get_state();
                if (after.sink_count != before.sink_count + 3u ||
                    state->pgraph.regs[0x0200 / 4] != 0x00100020u ||
                    state->pgraph.regs[0x0204 / 4] != 0x00200040u) return 1;
            }
        }
        fprintf(stderr,
                "GPU_SUBMISSION mode=%s aperture=PAGE_NOACCESS get=%08X put=%08X diag=%s sink=%u successes=%u atomic=%s\n",
                mode, after.get, after.put, after.diagnostic, after.sink_count,
                after.successes, blocked ? "blocked-unchanged" : "accepted");
        if (blocked) fprintf(stderr,
                "GPU_UNSUPPORTED subchannel=%u method=%08X param=%08X\n",
                after.diagnostic_subchannel, after.diagnostic_method,
                after.diagnostic_param);
        gpu_checkpoint(1);
        gpu_checkpoint(2);
        gpu_checkpoint(3);
        fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_gpu\n",
                (unsigned long long)GetTickCount64(),GetCurrentThreadId());
        __try { RaiseException(0xE0424243,0,0,NULL); }
        __except(GetExceptionCode()==0xE0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
        return 0;
    }
    if (!strcmp(mode, "gpu-ptimer-runtime")) {
        uint32_t pending = 0, pmc = 0;
        LONG generation;
        HANDLE notifier;
        volatile uint32_t *restored =
            (volatile uint32_t *)(g_xbox_mem_offset+0xFD8001c0u);
        g_jsrf_gpu_fixture=1;
        InterlockedExchange64(&g_ptimer_fixture_ns, 0);
        if (!nv2a_hook_set_ptimer_clock(gpu_ptimer_clock, NULL)) return 2;
        GPU_WORD(0xFD009140)=0;
        GPU_WORD(0xFD009100)=NV_PTIMER_INTR_0_ALARM;
        GPU_WORD(0xFD009200)=1;
        GPU_WORD(0xFD009210)=1;
        GPU_WORD(0xFD009400)=0;
        GPU_WORD(0xFD009410)=0;
        GPU_WORD(0xFD009420)=0x1000;
        GPU_WORD(0xFD000140)=NV_PMC_INTR_0_PTIMER;
        GPU_WORD(0xFD009140)=NV_PTIMER_INTR_EN_0_ALARM;
        InterlockedExchange64(&g_ptimer_fixture_ns, 1000000);
        nv2a_hook_notify_ptimer_clock_changed();
        /* Observe only the owner snapshot after advancing the clock. A PTIMER
         * MMIO read here would service the alarm and invalidate the test. */
        for (unsigned i=0; i<1000; ++i) {
            if (!gpu_ptimer_snapshot(&pending,&pmc,NULL)) return 1;
            if (pending & NV_PTIMER_INTR_0_ALARM) break;
            Sleep(1);
        }
        if (!(pending & NV_PTIMER_INTR_0_ALARM) || !(pmc & NV_PMC_INTR_0_PTIMER))
            return 1;
        GPU_WORD(0xFD009100)=NV_PTIMER_INTR_0_ALARM;
        Sleep(10);
        if (!gpu_ptimer_snapshot(&pending,&pmc,&generation)) return 1;
        if ((pending & NV_PTIMER_INTR_0_ALARM) || (pmc & NV_PMC_INTR_0_PTIMER))
            return 1;
        InterlockedExchange(&g_ptimer_notifier_stop,0);
        notifier=CreateThread(NULL,0,gpu_ptimer_notifier,NULL,0,NULL);
        if (!notifier) return 2;
        Sleep(1);
        nv2a_hook_shutdown();
        InterlockedExchange(&g_ptimer_notifier_stop,1);
        if (WaitForSingleObject(notifier,5000)!=WAIT_OBJECT_0) return 1;
        CloseHandle(notifier);
        InterlockedExchange64(&g_ptimer_fixture_ns, 2000000);
        nv2a_hook_notify_ptimer_clock_changed();
        Sleep(10);
        *restored=0x11b30001;
        if (*restored!=0x11b30001 ||
            InterlockedCompareExchange(&g_nv2a_mmio_snapshot_generation,0,0)!=generation)
            return 1;
        fprintf(stderr,"GPU_PTIMER_RUNTIME no-mmio-expiry=PASS w1c-pmc=PASS shutdown=PASS\n");
        fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_gpu\n",
                (unsigned long long)GetTickCount64(),GetCurrentThreadId());
        return 0;
    }
    if (!strcmp(mode, "gpu-mmio-lifecycle")) {
        volatile uint32_t *reg=(volatile uint32_t *)(g_xbox_mem_offset+0xFD800180u);
        LONG generation;
        MEMORY_BASIC_INFORMATION mbi;
        g_jsrf_gpu_fixture=1;
        *reg=0xA5A55A5A;
        if (*reg!=0xA5A55A5A) return 1;
        gpu_checkpoint(1);
        generation=InterlockedCompareExchange(&g_nv2a_mmio_snapshot_generation,0,0);
        nv2a_hook_shutdown();
        if (VirtualQuery((void *)reg,&mbi,sizeof(mbi))!=sizeof(mbi) ||
            mbi.State!=MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS|PAGE_GUARD))) return 1;
        *reg=0x5A5AA5A5;
        if (*reg!=0x5A5AA5A5 ||
            InterlockedCompareExchange(&g_nv2a_mmio_snapshot_generation,0,0)!=generation)
            return 1;
        gpu_checkpoint(2);
        fprintf(stderr,"GPU_MMIO_LIFECYCLE teardown=PASS restored=PAGE_READWRITE inactive=PASS\n");
        fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_gpu\n",
                (unsigned long long)GetTickCount64(),GetCurrentThreadId());
        __try { RaiseException(0xE0424243,0,0,NULL); }
        __except(GetExceptionCode()==0xE0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
        return 0;
    }
    if (!strcmp(mode, "gpu-mmio-owner")) {
        volatile uint8_t *reg8 = (volatile uint8_t *)(g_xbox_mem_offset + 0xFD800100u);
        volatile uint16_t *reg16 = (volatile uint16_t *)(g_xbox_mem_offset + 0xFD800104u);
        volatile uint32_t *reg32 = (volatile uint32_t *)(g_xbox_mem_offset + 0xFD800108u);
        volatile uint32_t *pci_command = (volatile uint32_t *)(g_xbox_mem_offset + 0xFD001804u);
        volatile LONG failure=0;
        LONG generation_before;
        MmioOwnerWorker workers[2];
        HANDLE threads[2];
        g_jsrf_gpu_fixture=1;
        if (!nv2a_hook_run_decoder_tests()) return 2;
        /* These are real dereferences of the protected aperture. Repeated
         * accesses to one page prove that every access traverses VEH. */
        *reg8=0x5A; *reg16=0xA55A; *reg32=0x12345678;
        if (*reg8!=0x5A || *reg16!=0xA55A || *reg32!=0x12345678) return 1;
        *reg32 |= 4;
        if (*reg32 != 0x1234567C) return 1;
        *pci_command=0x12345678; *pci_command |= 4;
        if (*pci_command != 0x1234567C) return 1;
        generation_before=InterlockedCompareExchange(&g_nv2a_mmio_snapshot_generation,0,0);
        workers[0].reg=reg32+4; workers[0].seed=0x11000000; workers[0].failure=&failure;
        workers[1].reg=reg32+5; workers[1].seed=0x22000000; workers[1].failure=&failure;
        threads[0]=CreateThread(NULL,0,gpu_mmio_owner_worker,&workers[0],0,NULL);
        threads[1]=CreateThread(NULL,0,gpu_mmio_owner_worker,&workers[1],0,NULL);
        if (!threads[0] || !threads[1] ||
            WaitForMultipleObjects(2,threads,TRUE,5000)!=WAIT_OBJECT_0 || failure)
            return 1;
        CloseHandle(threads[0]); CloseHandle(threads[1]);
        /* Successful writes publish one odd/even generation pair. Reads use
         * the serialized owner without republishing unchanged state. */
        if (InterlockedCompareExchange(&g_nv2a_mmio_snapshot_generation,0,0)-generation_before < 4000)
            return 1;
        uint32_t expected = 0x1234567Cu, observed = 0;
        memcpy(&observed, g_nv2a_mmio_snapshot + 0x1804, sizeof(observed));
        if (observed != expected) {
            fprintf(stderr, "GPU MMIO OWNER FAIL: shadow=%08X expected=%08X\n",
                    observed, expected);
            return 1;
        }
        fprintf(stderr, "GPU_MMIO_OWNER actual-veh PCI_COMMAND=%08X widths=PASS flags=PASS bounds=PASS concurrent=PASS\n", observed);
        gpu_checkpoint(1);
        gpu_checkpoint(2);
        gpu_checkpoint(3);
        fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_gpu\n",
                (unsigned long long)GetTickCount64(),GetCurrentThreadId());
        __try { RaiseException(0xE0424243,0,0,NULL); }
        __except(GetExceptionCode()==0xE0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
        return 0;
    }
    begin=xbox_ContiguousAlloc(256,256);
    if (!begin) return 2;
    uint32_t begin_phys = begin - 0x80000000u;
    g_jsrf_gpu_fixture=1;
    GPU_WORD(0x19B224)=begin; GPU_WORD(0x19B228)=begin+256;
    GPU_WORD(0x19B200)=begin+16;
    /* Two legal packets, one incrementing and one non-incrementing. USER
     * DMA pointers are physical byte offsets; GPU_WORD reaches the actual
     * contiguous window through the mapped guest VA. */
    GPU_WORD(0x80000000u + begin_phys)=0x00040200; GPU_WORD(0x80000000u + begin_phys+4)=0xCAFE0123;
    GPU_WORD(0x80000000u + begin_phys+8)=0x400417FC; GPU_WORD(0x80000000u + begin_phys+12)=0;
    /* PUT now performs real bounded parsing. Reset GET afterward because
     * these legacy probes intentionally synthesize pointer history. */
    GPU_WORD(0xFD800040)=begin_phys+16;
    GPU_WORD(0xFD800044)=begin_phys;
    if (!strcmp(mode,"gpu-corrupt")) GPU_WORD(0x80000000u + begin_phys)=0x001C0200; /* needs 7 data words, only 3 submitted */
    fprintf(stderr,"GPU_FIXTURE mode=%s begin=%08X end=%08X\n",mode,begin,begin+256);
    if (!strcmp(mode,"gpu-unreadable")) nv2a_hook_disable_aperture();
    gpu_checkpoint(1);
    Sleep(30);
    if (!strcmp(mode,"gpu-progress")) {
        GPU_WORD(0xFD800044)=begin_phys+8;
        gpu_checkpoint(2);
        GPU_WORD(0xFD800044)=begin_phys+16;
        gpu_checkpoint(3);
    } else if (!strcmp(mode,"gpu-unreadable")) {
        if (!VirtualProtect((void *)(g_xbox_mem_offset+0xFD000000u),16u*1024u*1024u,
                            PAGE_NOACCESS,&old_protect)) return 2;
        gpu_checkpoint(2);
    } else gpu_checkpoint(2);
    fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu probe_gpu\n",
            (unsigned long long)GetTickCount64(),GetCurrentThreadId());
    if (!strcmp(mode,"gpu-progress") || !strcmp(mode,"gpu-corrupt")) {
        __try { RaiseException(0xE0424243,0,0,NULL); }
        __except(GetExceptionCode()==0xE0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
        return 0;
    }
    worker.top=xbox_AllocThreadStack(); worker.tib=xbox_AllocThreadTib();
    worker.unreadable=!strcmp(mode,"gpu-unreadable");
    if (!worker.top || !worker.tib) return 2;
    HANDLE thread=CreateThread(NULL,0,gpu_probe_wait,&worker,0,NULL);
    if (!thread) return 2;
    WaitForSingleObject(thread,INFINITE); /* collector deadline preserves the waiter */
    return 1;
}
