/* Deliberate failures run only through --probe=..., before guest entry. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <xbox/xboxrecomp.h>
#include "diagnostics.h"
extern RECOMP_TLS uint32_t g_eax, g_esp;
extern RECOMP_TLS uint32_t g_edx;
extern ptrdiff_t g_xbox_mem_offset;
typedef void (*ProbeFunction)(void);
extern ProbeFunction recomp_lookup_kernel(uint32_t va);

typedef struct {
    unsigned index;
    uint32_t top, tib;
    const char *mode;
} Worker;
static HANDLE gate, ready[2];
static RTL_CRITICAL_SECTION *locks[2];
static LONG total, errors, spinning;

static uint32_t bridge_call(unsigned slot, const uint32_t *args, unsigned count)
{
    uint32_t before=g_esp;
    for (unsigned i=count;i>0;i--) { g_esp-=4; *(uint32_t *)(g_xbox_mem_offset+g_esp)=args[i-1]; }
    g_esp-=4; *(uint32_t *)(g_xbox_mem_offset+g_esp)=0x00F00D00;
    recomp_lookup_kernel(0xFE000000u+slot*4)();
    if (g_esp!=before) InterlockedIncrement(&errors);
    return g_eax;
}

static void probe_checkpoint(const char *name)
{
    fprintf(stderr,"[CHECKPOINT] ms=%llu tid=%lu %s\n",
        (unsigned long long)GetTickCount64(),GetCurrentThreadId(),name);
}

__declspec(noinline) static void probe_worker_fault(volatile uint32_t *pointer)
{
    *pointer = 0xBAD;
}

static DWORD WINAPI probe_worker(LPVOID argument)
{
    Worker *worker = argument;
    uint32_t marker = 0xA0000000u + worker->index;
    g_esp = worker->top; g_fs_base = worker->tib; g_eax = marker;
    *(uint32_t *)(g_xbox_mem_offset + g_esp) = marker;
    recomp_diag_thread_start(0xF1000000u + worker->index,
                            worker->top + 16 - 512*1024, worker->top + 16);
    WaitForSingleObject(gate, INFINITE);
    if (!strcmp(worker->mode,"dispatch-race")) {
        ProbeFunction fn=recomp_lookup_kernel(0xFE000000u+86*4); /* KeQueryInterruptTime */
        SetEvent(ready[0]);
        WaitForSingleObject(ready[1],INFINITE);
        g_esp-=4; *(uint32_t *)(g_xbox_mem_offset+g_esp)=0x00F00D04;
        fn();
        recomp_diag_thread_end();
        return 0;
    }
    if (!strcmp(worker->mode,"worker-crash"))
        probe_worker_fault((volatile uint32_t *)(uintptr_t)1);
    if (!strcmp(worker->mode,"spin"))
        for (;;) InterlockedIncrement(&spinning);
    if (!strcmp(worker->mode,"deadlock")) {
        unsigned first = worker->index & 1;
        xbox_RtlEnterCriticalSection(locks[first]);
        SetEvent(ready[first]);
        WaitForMultipleObjects(2, ready, TRUE, INFINITE);
        xbox_RtlEnterCriticalSection(locks[1-first]);
    }
    for (unsigned n=0;n<2000;n++) {
        xbox_RtlEnterCriticalSection(locks[0]);
        total++;
        xbox_RtlLeaveCriticalSection(locks[0]);
        if (g_eax != marker || g_fs_base != worker->tib || g_esp != worker->top ||
            *(uint32_t *)(g_xbox_mem_offset+g_esp) != marker) InterlockedIncrement(&errors);
        recomp_diag_record(JSRF_CALL,marker,n,worker->index);
    }
    /* End with a known ring history so external tests can check attribution. */
    for (unsigned n=0;n<200;n++) recomp_diag_record(JSRF_CALL,marker,n,worker->index);
    recomp_diag_thread_end();
    return 0;
}

extern int jsrf_probe_video(void);
extern int jsrf_probe_gpu(const char *mode);

int jsrf_run_probe(const char *mode)
{
    if (!strcmp(mode,"video")) return jsrf_probe_video();
    if (!strncmp(mode,"gpu-",4)) return jsrf_probe_gpu(mode);
    Worker workers[4];
    HANDLE threads[4];
    unsigned count = !strcmp(mode,"healthy") ? 4 : !strcmp(mode,"deadlock") ? 2 : 1;
    if (!strcmp(mode,"handled")) {
        __try { RaiseException(0xE0424242,0,0,NULL); }
        __except(GetExceptionCode()==0xE0424242 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
            probe_checkpoint("probe_handled"); return 0;
        }
    }
    if (strcmp(mode,"healthy") && strcmp(mode,"dispatch-race") && strcmp(mode,"deadlock") &&
        strcmp(mode,"worker-crash") && strcmp(mode,"spin")) return 2;
    uint32_t gpu_memory = xbox_ContiguousAlloc(16,16);
    if (!gpu_memory) return 2;
    *(uint32_t *)(g_xbox_mem_offset+gpu_memory) = 0x47505531;
    fprintf(stderr,"PROBE_GPU_MEMORY va=%08X value=47505531\n",gpu_memory);
    uint32_t lock_va = xbox_HeapAlloc(sizeof(RTL_CRITICAL_SECTION)*2,16);
    if (!lock_va) return 2;
    locks[0]=(RTL_CRITICAL_SECTION *)(g_xbox_mem_offset+lock_va);
    locks[1]=locks[0]+1;
    xbox_RtlInitializeCriticalSection(locks[0]); xbox_RtlInitializeCriticalSection(locks[1]);
    gate=CreateEventA(NULL,TRUE,FALSE,NULL);
    ready[0]=CreateEventA(NULL,TRUE,FALSE,NULL); ready[1]=CreateEventA(NULL,TRUE,FALSE,NULL);
    if (!gate || !ready[0] || !ready[1]) return 2;
    g_eax=0xCAFE1234;
    for (unsigned i=0;i<count;i++) {
        workers[i].index=i; workers[i].mode=mode;
        workers[i].top=xbox_AllocThreadStack(); workers[i].tib=xbox_AllocThreadTib();
        if (!workers[i].top || !workers[i].tib) return 2;
        threads[i]=CreateThread(NULL,0,probe_worker,&workers[i],0,NULL);
        if (!threads[i]) return 2;
    }
    probe_checkpoint("probe_started");
    if (!strcmp(mode,"dispatch-race")) {
        ProbeFunction fn=recomp_lookup_kernel(0xFE000000u+42*4); /* KeRaiseIrqlToDpcLevel */
        SetEvent(gate);
        WaitForSingleObject(ready[0],INFINITE); /* worker deliberately overwrites its lookup slot */
        g_esp-=4; *(uint32_t *)(g_xbox_mem_offset+g_esp)=0x00F00D08;
        fn();
        if (g_eax!=0 || g_edx!=0) InterlockedIncrement(&errors);
        SetEvent(ready[1]);
    } else SetEvent(gate);
    WaitForMultipleObjects(count,threads,TRUE,INFINITE);
    for (unsigned i=0;i<count;i++) { CloseHandle(threads[i]); xbox_FreeThreadStack(workers[i].top); }
    CloseHandle(gate); CloseHandle(ready[0]); CloseHandle(ready[1]);
    if (!strcmp(mode,"dispatch-race")) {
        if (errors) { probe_checkpoint("probe_dispatch_failed"); return 1; }
        probe_checkpoint("probe_dispatch"); return 0;
    }
    if (g_eax!=0xCAFE1234 || errors || total!=8000) {
        fprintf(stderr,"PROBE FAIL total=%ld errors=%ld eax=%08X\n",total,errors,g_eax); return 1;
    }
    probe_checkpoint("probe_healthy");
    /* Exercise the game's imported event bridges, including return/ESP ABI. */
    uint32_t storage=xbox_HeapAlloc(16,16);
    uint32_t create_args[]={storage,0,0,0}; /* notification event, initially clear */
    if (bridge_call(14,create_args,4)!=0) return 1;
    uint32_t token=*(uint32_t *)(g_xbox_mem_offset+storage);
    *(int64_t *)(g_xbox_mem_offset+storage+8)=0; /* immediate timeout */
    uint32_t wait_args[]={token,0,0,storage+8}, signal_args[]={token,0};
    if (bridge_call(18,wait_args,4)!=0x102) return 1;
    if (bridge_call(16,signal_args,2)!=0 || bridge_call(18,wait_args,4)!=0) return 1;
    if (bridge_call(17,&token,1)!=0 || bridge_call(18,wait_args,4)!=0x102) return 1;
    if (bridge_call(0,&token,1)!=0 || errors) return 1;
    probe_checkpoint("probe_events");
    __try { RaiseException(0xE0424243,0,0,NULL); }
    __except(GetExceptionCode()==0xE0424243 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { }
    return 0;
}
