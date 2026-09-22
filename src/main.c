#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <io.h>

#include <xbox/xboxrecomp.h>
#include "diagnostics.h"
#include "nv2a_mmio_hook.h"

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern RECOMP_TLS uint32_t g_seh_ebp;
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top;
extern RECOMP_TLS uint16_t g_fp_control_word;
extern RECOMP_TLS int g_fp_cmp;
extern RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
extern RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
extern ptrdiff_t g_xbox_mem_offset;

#define YOUR_GAME_ENTRY_POINT 0x00148023
#define YOUR_GAME_XBE_PATH "game\\default.xbe"
#define YOUR_GAME_DIR "game"

extern void xbe_entry_point(void);

static void checkpoint(const char *name)
{
    fprintf(stderr, "[CHECKPOINT] ms=%llu tid=%lu %s\n",
            (unsigned long long)GetTickCount64(), GetCurrentThreadId(), name);
}

static BOOL load_xbe(const char *path, void **out_data, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    long size;
    void *data;

    if (!file)
        return FALSE;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        fclose(file);
        return FALSE;
    }
    data = malloc((size_t)size);
    if (!data) {
        fclose(file);
        return FALSE;
    }
    if (fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        fclose(file);
        return FALSE;
    }
    fclose(file);
    *out_data = data;
    *out_size = (size_t)size;
    return TRUE;
}

static LONG CALLBACK veh_handler(PEXCEPTION_POINTERS exception)
{
    DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code == STATUS_GUARD_PAGE_VIOLATION || code == EXCEPTION_ACCESS_VIOLATION) {
        uintptr_t fault = exception->ExceptionRecord->ExceptionInformation[1];
        uintptr_t guest_fault = fault - (uintptr_t)g_xbox_mem_offset;
        if (guest_fault >= 0xFD000000u && guest_fault < 0xFE000000u &&
            nv2a_hook_handle_mmio(exception->ContextRecord, fault,
                                  (uint32_t)guest_fault,
                                  (int)exception->ExceptionRecord->ExceptionInformation[0]))
            return EXCEPTION_CONTINUE_EXECUTION;
        _lock_file(stderr);
        fprintf(stderr, "[EXCEPTION first-chance] tid=%lu RIP=0x%llX fault=0x%llX (%s)\n",
            GetCurrentThreadId(),
            (unsigned long long)exception->ContextRecord->Rip,
            (unsigned long long)fault,
            exception->ExceptionRecord->ExceptionInformation[0] ? "write" : "read");
        fprintf(stderr, "  Xbox regs: eax=0x%08X ecx=0x%08X edx=0x%08X esp=0x%08X\n",
            g_eax, g_ecx, g_edx, g_esp);
        fprintf(stderr, "  Xbox regs: ebx=0x%08X esi=0x%08X edi=0x%08X\n",
            g_ebx, g_esi, g_edi);
        fflush(stderr);
        _unlock_file(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line, int show)
{
    void *xbe_data = NULL;
    size_t xbe_size = 0;
    extern void xbox_path_init(const char *game_dir, const char *save_dir);

    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show;

    /* Share one OS file description so stdout cannot overwrite stderr. */
    const char *log_path = getenv("JSRF_LOG_PATH");
    if (!freopen(log_path ? log_path : "jsrf_run.log", "w", stderr) ||
        !freopen("NUL", "w", stdout) ||
        _dup2(_fileno(stderr), _fileno(stdout)) != 0) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    printf("=== Jet Set Radio Future - Static Recompilation ===\n");
    checkpoint("host_entry");

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(GetCurrentProcess(), NULL, TRUE);
    AddVectoredExceptionHandler(1, veh_handler);

    if (!load_xbe(YOUR_GAME_XBE_PATH, &xbe_data, &xbe_size)) {
        MessageBoxA(NULL, "Failed to load game\\default.xbe.", "JSRF Recomp", MB_ICONERROR);
        return 1;
    }
    printf("XBE loaded: %zu bytes\n", xbe_size);

    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        MessageBoxA(NULL, "Failed to initialize Xbox memory layout.", "JSRF Recomp", MB_ICONERROR);
        free(xbe_data);
        return 1;
    }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    checkpoint("memory_ready");
    xbox_kernel_init();
    xbox_path_init(YOUR_GAME_DIR, NULL);
    xbox_kernel_bridge_init();
    nv2a_hook_init(g_xbox_mem_offset);
    if (!nv2a_hook_install_aperture((void *)(0xFD000000u + g_xbox_mem_offset),
                                    16u * 1024u * 1024u)) {
        fprintf(stderr, "[NV2A] failed to install MMIO state owner\n");
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* Publish the pushbuffer write position where the title waits for GPU
     * completion.
     *
     * JSRF's D3D device lives at guest 0x0019DCE0. It keeps the pushbuffer write
     * position at +0x00 and, at +0x34, a pointer to the notify word it polls.
     * Measured from a run: the ring is 0x80001000..0x80081000, the current chunk
     * ends at 0x80008DFC and the write position is 0x80001B24, so the free space
     * 0x001910E0 computes is 29400 -- under the 0x8000 the caller requires -- and
     * 0x00191440 therefore waits at 0x001914F0 for the GPU to publish completion
     * into the notify word at 0x80000000. Nothing in the model wrote guest
     * memory, so that word kept the title's own 0xDEADBEEF sentinel and the wait
     * never ended.
     *
     * xbox_Nv2aMirrorFence does exactly this and had never been registered by any
     * title; this is its first use. It follows the device pointer fresh on every
     * poll through fence_readable, which handles the contiguous window, so
     * registering before the device exists is fine.
     */
    if (xbox_Nv2aMirrorFence(0x0019DCE0u, 0x00u, 0x34u) != 0)
        fprintf(stderr, "[NV2A] fence mirror registration failed\n");

    g_esp = XBOX_STACK_TOP;
    recomp_diag_thread_start(YOUR_GAME_ENTRY_POINT, XBOX_STACK_BASE, XBOX_STACK_TOP + 16);

    const char *probe = strstr(command_line, "--probe=");
    if (probe) {
        int result = jsrf_run_probe(probe + 8);
        recomp_diag_thread_end();
        xbox_kernel_shutdown();
        nv2a_hook_shutdown();
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return result;
    }

    if (!recomp_dispatch_init())
        fprintf(stderr, "[BOOT] flat dispatch unavailable\n");
    if (!getenv("JSRF_COLLECTED")) xbox_WatchdogStart();

    printf("Starting entry point 0x%08X...\n", YOUR_GAME_ENTRY_POINT);
    checkpoint("guest_entry");
    xbe_entry_point();
    recomp_diag_thread_end();

    xbox_kernel_shutdown();
    nv2a_hook_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOW);
}
