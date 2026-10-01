#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <io.h>

#include <xbox/xboxrecomp.h>
#include "diagnostics.h"
#include "jsrf_save_root.h"
#include "nv2a_mmio_hook.h"
#include "apu.h"
#include "apu_mmio_hook.h"

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

/* ── THE RECOMPILED MODULE'S CODE BOUND, MEASURED THROUGH THE GENERATED DISPATCH ─────────────────
 *
 * ⚠ WHY THIS LIVES HERE AND NOT IN THE TOOLKIT. The toolkit is a static library; the recompiled
 * module's functions are THIS game's generated translation units (`src/recomp/gen/recomp_*.c`), and
 * a library cannot enumerate symbols it does not contain. The embedder can, through the generated
 * dispatch, and the packet requires the range classifier to work from the REAL artifact rather than
 * from an arithmetic guess.
 *
 * ⚠ SO THE BOUND IS DERIVED FROM ADDRESSES `recomp_lookup` ACTUALLY RETURNS. Every probe below is a
 * real guest VA; `recomp_lookup` answers with the REAL native address of the REAL generated function
 * that implements it, or NULL when no function starts there. The extent of the non-NULL answers is
 * the recompiled module's code bound, measured rather than assumed.
 *
 * ⚠ AND A DEGENERATE OR EMPTY RESULT IS REPORTED AS INVALID, NOT WIDENED. If the probes found fewer
 * than two distinct addresses the bound is published as INVALID, which makes the toolkit's range
 * classifier REFUSE to classify and record `range_unavailable` -- the packet's INFRA FAILURE. That is
 * the correct outcome: a bound invented here would make "RIP is inside the recompiled module" true
 * for addresses that are not, and the installer control would become satisfiable by an unrelated
 * writer. There is no fallback bound and there must not be one.
 *
 * ⚠ THE PROBE STRIDE IS 4 BYTES, AND ITS COVERAGE IS MEASURED RATHER THAN ARGUED. Guest x86
 * functions are not 4-byte aligned in general, so the stride misses entries: MEASURED, it reaches
 * 7 458 of the dispatch's 8 768 entries and misses 1 310. That was checked rather than waved away,
 * because a missed entry could in principle sit outside the extent and classify TOOLKIT_HOST. The
 * measurement: of the 1 310 missed entries, ZERO have a native address outside the published extent,
 * and ZERO real recompiled bodies have a start outside it either -- the extent is exactly
 * `[0x140003430, 0x140c15920]`, identical to the extent of all 8 658 real bodies. So the missed
 * entries are aliases and interior labels of functions the probe already found (many guest VAs map
 * onto one generated body), not functions of their own.
 *
 * ⚠ AND THE MISS DIRECTION IS THE SAFE ONE EVEN IF THAT EVER CHANGED. Missing a START can only make
 * the classification STRICTER: a RIP in an unlisted function is attributed to the listed function
 * below it, which is still a recompiled body, so it is still GAME_MODULE -- and if it fell below the
 * lowest start it would be TOOLKIT_HOST, which is a CONSERVATIVE miss and never a false GAME_MODULE.
 * A false GAME_MODULE would require a HOST symbol between two published starts, and the probe cannot
 * create that by missing an entry. */
#define JSRF_RECOMP_PROBE_LO   0x00010000u   /* below the first generated function */
#define JSRF_RECOMP_PROBE_HI   0x00800000u   /* above the last one, well inside guest RAM */
#define JSRF_RECOMP_PROBE_STEP 4u

/* ⚠ DECLARED HERE RATHER THAN BY INCLUDING recomp_types.h, AND THE REASON IS A REAL COLLISION.
 * recomp_types.h defines `eax`, `ecx`, `MEM32` and friends as macros over the register globals;
 * main.c declares those globals itself and pulls in <windows.h>, and the two do not coexist. The
 * generated dispatch's public surface is these two symbols and nothing else is needed here, so they
 * are declared directly. They are exactly the declarations recomp_dispatch.c provides -- a drift
 * would be a link error, not a silent misread. */
typedef void (*jsrf_recomp_func_t)(void);
extern jsrf_recomp_func_t recomp_lookup(uint32_t xbox_va);
extern int recomp_dispatch_init(void);

/* ⚠ THE PUBLICATION IS THE SET OF REAL FUNCTION STARTS, AND WHAT IT DOES *NOT* BUY IS RECORDED HERE.
 *
 * The obvious publication is `[min, max]` of the observed addresses. MEASURED against the real linker
 * map and PE section table: the game's own probe objects (`harness_probes`, `video_probes`,
 * `gpu_probes`) are linked into the MIDDLE of the recompiled extent as one 0x1690-byte run of 14 host
 * functions -- `probe_worker_fault`, `gpu_probe_wait`, `jsrf_probe_gpu` among them, all of which run
 * during a probe run and touch memory.
 *
 * ⚠ AND THE SET DOES *NOT* FIX THAT, WHICH WAS MEASURED RATHER THAN ASSUMED. The classifier was
 * changed to test membership of this set, and the fixture's discriminating arm then showed the two
 * tests have EQUIVALENT COVERAGE: a host run lying strictly between two published starts falls inside
 * the interval attributed to the recompiled function BELOW it under EITHER test. Distinguishing them
 * needs function ENDS, and the generated dispatch answers only function ENTRIES -- so the residual is
 * a real limit of what this embedder can publish, not a defect in the test.
 *
 * The set is still published rather than a bare interval, for two honest reasons: it is the REAL
 * data the dispatch can answer (so the archive carries the actual function starts and a reader can
 * re-classify by hand), and it makes the extent's definition explicit -- first start to last start --
 * instead of a min/max over a probe whose stride could silently narrow it. The residual is ASSERTED
 * in the fixture and stated in the packet rather than claimed away. */
#define JSRF_RECOMP_PROBE_LO   0x00010000u   /* below the first generated function */
#define JSRF_RECOMP_PROBE_HI   0x00800000u   /* above the last one, well inside guest RAM */
#define JSRF_RECOMP_PROBE_STEP 4u
/* The collector's mirror and the toolkit's ledger both hold 512; the CLASSIFIER's set is held in the
 * toolkit and capped separately at XBOX_A2H_SLOTW_RECOMP_MAX. The real title has 8 658 starts. */
#define JSRF_RECOMP_STARTS_MAX 16384

static uint64_t g_jsrf_recomp_starts[JSRF_RECOMP_STARTS_MAX];

static void a2h_publish_recomp_bounds(void)
{
    uint32_t va;
    uint32_t probes = 0, hits = 0;
    uint32_t accepted;

    for (va = JSRF_RECOMP_PROBE_LO; va < JSRF_RECOMP_PROBE_HI; va += JSRF_RECOMP_PROBE_STEP) {
        jsrf_recomp_func_t fn = recomp_lookup(va);
        uint64_t addr;
        if (!fn)
            continue;
        addr = (uint64_t)(uintptr_t)fn;
        if (!addr)
            continue;
        probes++;
        /* ⚠ A DUPLICATE ADDRESS IS NOT A SECOND FUNCTION. The dispatch maps several guest VAs onto
         * one generated body (a shared tail, a thunk alias), and recording the same start twice
         * would not change the classification but would waste the cap. Skipping it keeps `hits` an
         * honest count of DISTINCT recompiled functions, which is what the archive reports. */
        if (hits > 0 && addr == g_jsrf_recomp_starts[hits - 1])
            continue;
        if (hits >= JSRF_RECOMP_STARTS_MAX) {
            fprintf(stderr, "[A2HSLOTW] recompiled start set OVERFLOWS %u -- REFUSING WHOLE;"
                            " the classifier will report range_unavailable (INFRA FAILURE)\n",
                    (unsigned)JSRF_RECOMP_STARTS_MAX);
            fflush(stderr);
            /* LATCHED BEFORE THE REFUSAL, so the archive says WHY the set is absent rather than
             * leaving a clean refusal with no reason attached. */
            xbox_A2hSlotWatchNoteRecompOverflow();
            xbox_A2hSlotWatchSetRecompStarts(NULL, 0u, probes);
            return;
        }
        g_jsrf_recomp_starts[hits++] = addr;
    }

    /* ⚠ THE PROBE WALKS GUEST VAs IN ASCENDING ORDER, BUT NATIVE ADDRESSES ARE NOT IN GUEST ORDER.
     * The generated translation units are grouped by chunk, and a guest VA in a later chunk can
     * have a lower native address. The toolkit SORTS the set internally before publishing it, so the
     * order here does not matter -- and the consecutive-duplicate skip above is therefore only a
     * cheap filter, not the deduplication the classifier depends on. */
    accepted = xbox_A2hSlotWatchSetRecompStarts(g_jsrf_recomp_starts, hits, probes);
    if (!accepted || hits < 2) {
        fprintf(stderr, "[A2HSLOTW] recompiled start set NOT publishable: hits=%u accepted=%u"
                        " -- the range classifier will refuse and report range_unavailable\n",
                hits, accepted);
        fflush(stderr);
        return;
    }
    fprintf(stderr, "[A2HSLOTW] recompiled start set published: %u distinct function starts over"
                    " %u probes (the toolkit sorts the set and reports its extent)\n", hits, probes);
    fflush(stderr);
}

/* ── Q3(c): THE GAME-SIDE HOOK READ, FOR THE CROSS-VALIDATION ────────────────────────────────────
 *
 * The Advisor requires cross-validation "where fault-record and hook reads overlap (same address +
 * time must agree -- the control that caught the byte-order trap)". This is the hook side.
 *
 * THE TWO READERS USE DIFFERENT ARITHMETIC FOR THE SAME PHYSICAL DWORD, AND THAT IS THE POINT:
 *
 *   * THE FAULT RECORD reads through `g_a2h_slotw_pages[alias-1] + (slot_va & 0xFFF)` -- a RAW HOST
 *     PAGE BASE captured once at ARM and cached by the toolkit. Nothing re-derives it per event.
 *   * THIS HOOK reads `MEM32(slot_va)` -- the guest's OWN translation, `g_xbox_mem_offset` plus the
 *     guest VA, re-derived on every access, exactly as the generated code reads it.
 *
 * A wrong base, a wrong offset, or a stale cached pointer would make one of them read the wrong
 * dword while the other stayed right, and nothing downstream could tell: the instrument would simply
 * publish a wrong number. That is the byte-order-trap failure class, and it is what this catches.
 *
 * ⚠ THE QUIET-WINDOW GUARD, WHICH IS WHAT MAKES THE COMPARISON SOUND RATHER THAN RACY. A concurrent
 * slot write would make the two readers legitimately disagree. So the toolkit publishes the
 * slot-hit count at the moment it made its read; this hook reads that triple, performs its own
 * read, then reads the count AGAIN. An unchanged count proves no slot write landed across the
 * window, so both readers cover the same value and must agree. A count that moved means the window
 * was not quiet and the pair is counted SKIPPED by the toolkit, never as agreement -- so "no
 * comparison" can never be read as "agreement".
 *
 * OBSERVATION ONLY. The read is a plain load from an RO page, which the mechanism explicitly lets
 * pass without faulting. Nothing is written, nothing is changed, and the whole thing is behind the
 * same gate as the watch. */
static HANDLE g_a2h_cross_thread = NULL;
static volatile LONG g_a2h_cross_stop = 0;

static DWORD WINAPI a2h_cross_validate_thread(LPVOID param)
{
    unsigned long samples = 0, compared = 0, mismatches = 0;
    (void)param;

    while (!InterlockedCompareExchange(&g_a2h_cross_stop, 0, 0)) {
        uint32_t slot_va = xbox_A2hSlotWatchSlotVa();
        uint32_t rec_value, rec_seq, rec_hits, hook_value, hits_after;

        if (slot_va && xbox_A2hSlotWatchLastSlotRead(&rec_value, &rec_seq, &rec_hits)) {
            /* THE HOOK'S OWN READ, through the guest's translation and not through the instrument's
             * cached alias base. This is the expansion of the guest's own `MEM32(slot_va)`
             * (`XBOX_PTR` = guest VA + g_xbox_mem_offset), written out because this translation unit
             * does not pull in recomp_types.h's macros -- and writing it out makes explicit that the
             * address is RE-DERIVED here rather than reusing the toolkit's cached page base, which
             * is the whole point of the cross-validation. */
            hook_value = *(volatile uint32_t *)((uintptr_t)slot_va + (uintptr_t)g_xbox_mem_offset);
            hits_after = xbox_A2hSlotWatchSlotHits();
            samples++;
            if (hits_after == rec_hits) {
                /* THE WINDOW WAS QUIET: no slot write landed between the instrument's read and this
                 * one, so the two cover the SAME ADDRESS AT THE SAME TIME and must agree. The
                 * toolkit applies the further alias guard (only canonical reads are comparable,
                 * because the mirror views are not coherent here) and counts every case it does not
                 * compare as SKIPPED rather than as agreement. */
                compared++;
                if (!xbox_A2hSlotWatchCrossCheck(slot_va, rec_value, hook_value)) {
                    mismatches++;
                    fprintf(stderr, "  [A2HSLOTW] CROSS-VALIDATION hook disagrees: slot=%08X"
                                    " fault_record=%08X hook=%08X seq=%u samples=%lu\n",
                            slot_va, rec_value, hook_value, rec_seq, samples);
                    fflush(stderr);
                }
            } else {
                /* The window was not quiet. Counted by the toolkit as skipped; never as agreement. */
                xbox_A2hSlotWatchCrossCheck(0xFFFFFFFFu, 0u, 0u);
            }
        }
        Sleep(1);
    }
    fprintf(stderr, "  [A2HSLOTW] cross-validation hook stopped samples=%lu compared=%lu"
                    " mismatches=%lu\n", samples, compared, mismatches);
    fflush(stderr);
    return 0;
}

static void a2h_cross_validate_start(void)
{
    if (!xbox_A2hSlotWatchEnabled() || g_a2h_cross_thread)
        return;
    g_a2h_cross_stop = 0;
    g_a2h_cross_thread = CreateThread(NULL, 0, a2h_cross_validate_thread, NULL, 0, NULL);
    if (!g_a2h_cross_thread) {
        fprintf(stderr, "  [A2HSLOTW] cross-validation hook NOT created (error %lu): Q3(c) has no"
                        " independent reader this run\n", GetLastError());
        fflush(stderr);
    }
}

static void a2h_cross_validate_stop(void)
{
    if (!g_a2h_cross_thread)
        return;
    InterlockedExchange(&g_a2h_cross_stop, 1);
    WaitForSingleObject(g_a2h_cross_thread, 2000);
    CloseHandle(g_a2h_cross_thread);
    g_a2h_cross_thread = NULL;
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
        /* The APU's own 512K, which only faults when RECOMP_APU_TRAP unmapped
         * it. Nothing else routes these: the aperture is otherwise plain
         * memory, so a register write is accepted and discarded, which is how
         * the DSP command block at 0x803C0800 gets a status word written to it
         * that nothing ever answers. */
        if (guest_fault >= 0xFE800000u && guest_fault < 0xFE880000u &&
            apu_hook_handle_mmio(exception->ContextRecord, fault,
                                 (uint32_t)guest_fault,
                                 (int)exception->ExceptionRecord->ExceptionInformation[0]))
            return EXCEPTION_CONTINUE_EXECUTION;
        _lock_file(stderr);
        fprintf(stderr, "[EXCEPTION first-chance] tid=%lu code=0x%08lX RIP=0x%llX fault=0x%llX (%s)\n",
            GetCurrentThreadId(), (unsigned long)code,
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
    /* Anything that is not a memory fault is reported too, and with the guest
     * registers, because the alternative is a stack trace and no state.
     *
     * A guest `div` with a zero divisor reaches the host as an ordinary
     * STATUS_INTEGER_DIVIDE_BY_ZERO -- the recompiled code runs the guest's own
     * div instruction -- and the only question worth answering about it is what
     * the divisor's container held. The fault address names the instruction,
     * not the object, so ECX is the field that matters. JSRF's first one is
     * 0x001A2BFC `div esi` with esi = byte [ecx+0x64]
     * (logs/runs/20260922-055208-364-p5-ac97-only). */
    else if (code != EXCEPTION_BREAKPOINT) {
        _lock_file(stderr);
        fprintf(stderr, "[EXCEPTION] tid=%lu code=0x%08lX RIP=0x%llX\n",
            GetCurrentThreadId(), (unsigned long)code,
            (unsigned long long)exception->ContextRecord->Rip);
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
    JsrfLaunchOptions launch;
    LPWSTR *wide_argv = NULL;
    int wide_argc = 0;
    WCHAR resolved_root[MAX_PATH];
    char save_root_utf8[JSRF_SAVE_ROOT_UTF8_CAP];
    char probe_utf8[JSRF_PROBE_NAME_CAP * 4];
    DWORD save_error = ERROR_SUCCESS;

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

    /* Parse the process command line as argv tokens. Substring scanning the
     * raw WinMain tail made a save path containing "--probe=" look like a
     * probe request and could enter fixture code instead of the title. */
    wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (!wide_argv ||
        !jsrf_parse_launch_args_w(wide_argc, wide_argv, &launch, &save_error)) {
        if (!save_error)
            save_error = GetLastError();
        fprintf(stderr, "[SAVE] root rejected: missing, duplicate, or unknown launch argument (winerror=%lu)\n",
                (unsigned long)save_error);
        if (wide_argv)
            LocalFree(wide_argv);
        return 2;
    }
    if (!jsrf_preflight_save_root(launch.save_root, resolved_root, &save_error)) {
        const char *reason = "directory preflight failed";
        if (save_error == ERROR_BAD_PATHNAME)
            reason = "root must be an absolute Windows path";
        else if (save_error == ERROR_FILENAME_EXCED_RANGE)
            reason = "root does not fit toolkit MAX_PATH buffers";
        else if (save_error == ERROR_PATH_NOT_FOUND || save_error == ERROR_FILE_NOT_FOUND)
            reason = "requested directory does not exist";
        else if (save_error == ERROR_DIRECTORY)
            reason = "requested root is not a directory";
        else if (save_error == ERROR_ACCESS_DENIED)
            reason = "requested directory is not writable";
        else if (save_error == ERROR_INVALID_NAME)
            reason = "volume root or reserved profile path is not allowed";
        else if (save_error == ERROR_CANT_RESOLVE_FILENAME)
            reason = "root resolves through a junction or symbolic link";
        else if (save_error == ERROR_NOT_READY)
            reason = "cannot resolve LocalAppData safety boundary";
        fprintf(stderr, "[SAVE] root rejected: %s (winerror=%lu)\n",
                reason, (unsigned long)save_error);
        LocalFree(wide_argv);
        return 2;
    }
    if (!jsrf_save_root_to_utf8(resolved_root, save_root_utf8,
                                sizeof(save_root_utf8), &save_error) ||
        (launch.has_probe &&
         !jsrf_save_root_to_utf8(launch.probe, probe_utf8,
                                 sizeof(probe_utf8), &save_error))) {
        fprintf(stderr, "[SAVE] launch rejected: UTF-8 conversion failed (winerror=%lu)\n",
                (unsigned long)save_error);
        LocalFree(wide_argv);
        return 2;
    }
    LocalFree(wide_argv);
    wide_argv = NULL;
    printf("[SAVE] resolved_root=%s\n", save_root_utf8);
    fflush(stdout);

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

    /* The emulated APU, which the toolkit has shipped all along and which no
     * caller has ever initialised -- so every APU register write in this title
     * has landed in the plain MCPX aperture and been thrown away.
     *
     * What that costs is now measured rather than assumed. DirectSound hands
     * the GP DSP a 24-byte command block in guest RAM (the 48K contiguous
     * allocation at 0x803C0000; its base is published at guest 0x1BA858), sets
     * the status dword at +0x810 to 3, and spins at 0x001A18D0 until the DSP
     * writes 0 back. The only code in the title that ever writes 0 there is the
     * DSP stop path (0x001A1F9B and 0x001A1747), so in the original the
     * hardware is the one that answers. Nothing here can answer yet, but the
     * register traffic that precedes the wait is only observable once the APU
     * is instantiated and its window is trapped, and that traffic is the next
     * measurement.
     *
     * Instantiated only under RECOMP_APU_TRAP, the same switch that unmaps the
     * window. Without the trap no APU access can fault, so the hook is
     * unreachable and the APU would be a background waveOut thread nothing
     * talks to; with it, the emulated APU owns the registers instead of the
     * plain-memory stand-in. One switch, one behaviour, and the default run
     * unchanged. */
    if (getenv("RECOMP_APU_TRAP")) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
        if (!g_apu_state)
            fprintf(stderr, "[APU] init failed; trapped APU registers will"
                            " fault with no handler\n");
    }

    xbox_kernel_init();
    xbox_path_init(YOUR_GAME_DIR, save_root_utf8);
    {
        WCHAR actual_partition0[MAX_PATH];
        WCHAR expected_partition0[MAX_PATH];
        extern BOOL xbox_translate_path(const char *xbox_path,
                                        WCHAR *host_path_buf, DWORD buf_size);
        int written = swprintf_s(expected_partition0, MAX_PATH,
                                 L"%s\\Partition0.img", resolved_root);
        DWORD path_error = ERROR_SUCCESS;
        if (written < 0) {
            path_error = ERROR_FILENAME_EXCED_RANGE;
        } else if (!xbox_translate_path("\\Device\\Harddisk0\\Partition0",
                                        actual_partition0, MAX_PATH)) {
            /* The toolkit BOOL API does not promise to set last-error. */
            path_error = ERROR_INVALID_DATA;
        } else if (_wcsicmp(actual_partition0, expected_partition0) != 0) {
            path_error = ERROR_INVALID_DATA;
        } else if (GetFileAttributesW(actual_partition0) == INVALID_FILE_ATTRIBUTES) {
            path_error = GetLastError();
        }
        if (path_error != ERROR_SUCCESS) {
            fprintf(stderr, "[SAVE] path_layer=FAIL winerror=%lu\n",
                    (unsigned long)path_error);
            xbox_kernel_shutdown();
            xbox_MemoryLayoutShutdown();
            free(xbe_data);
            return 2;
        }
        printf("[SAVE] path_layer_root=%s\n", save_root_utf8);
        fflush(stdout);
    }
    xbox_kernel_bridge_init();
    nv2a_hook_init(g_xbox_mem_offset);
    /* Connect the card's interrupt line to the guest's GPU vector.
     *
     * The model asserts it from its own display clock when a pending bit is
     * unmasked; the guest's ISR acknowledges by writing 1 back to the source.
     * Without this the model aggregates its masks and tells nobody. */
    if (xbox_Nv2aAttachIrqLine() != 0)
        fprintf(stderr, "[NV2A] interrupt line not attached; the guest will"
                        " not see GPU interrupts\n");
    if (!nv2a_hook_install_aperture((void *)(0xFD000000u + g_xbox_mem_offset),
                                    16u * 1024u * 1024u)) {
        fprintf(stderr, "[NV2A] failed to install MMIO state owner\n");
        xbox_MemoryLayoutShutdown();
        free(xbe_data);
        return 1;
    }
    /* Publish the GPU completion fence where the title waits for it.
     *
     * JSRF's D3D device global is at guest 0x0019DCE0 and holds a POINTER to the
     * device; the device itself is allocated at runtime (0x0019B200 in the run
     * below). At +0x34 the device keeps a pointer to the notify word it polls,
     * and at +0x30 the fence counter it compares that word against.
     *
     * Measured from a run (logs/runs/20260922-054222-202-p1-pfifo-trace):
     *   [0x0019DCE0] = 0x0019B200   the device
     *   [+0x00] = 0x80001000        pushbuffer write position
     *   [+0x04] = 0x80008DFC        current chunk end
     *   [+0x24]/[+0x28] = 0x80001000/0x80081000   ring bounds (512 KB)
     *   [+0x30] = 5                 the fence counter
     *   [+0x34] = 0x80000000        the notify word
     *
     * 0x00191440 waits at 0x001914F0 as
     *
     *     eax = [dev+0x30] - [ebx]      ; limit - fence
     *   L: ecx = [notify]; esi = [dev+0x30] - ecx; cmp eax, esi; jb L
     *
     * so it exits when `[dev+0x30] - notify <= [dev+0x30] - fence`, i.e. when
     * notify >= fence. That subtraction is only monotone while notify is at
     * most [dev+0x30]. Publishing the pushbuffer POSITION there (a VA such as
     * 0x80001000) makes it wrap, the comparison inverts, and the wait never
     * ends -- which is exactly what the previous registration did.
     *
     * The title's own code publishes this word itself at 0x0019133B as
     * `[dev+0x30] - 2`, so the counter at +0x30 is the quantity the wait wants
     * and the counter is what is mirrored here. Nothing executes the push
     * buffer synchronously, so the counter is the honest answer.
     *
     * This is a HOST-SIDE WRITER into guest memory and it is a plausible competitor for the
     * software-device slot, so it is checked rather than assumed: the device pointer global is at
     * 0x0019DCE0 and the slot is at device+0x242C, while the counter mirrored here is at
     * device+0x30 -- 0x23FC bytes apart, on different pages, so this writer cannot reach the
     * watched page at all. The watch does not exclude it and does not need to: a store from this
     * thread would be caught by the same page protection as any other.
     *
     * xbox_Nv2aMirrorFence follows the device pointer fresh on every poll
     * through fence_readable, which handles the contiguous window, so
     * registering before the device exists is fine.
     */
    if (xbox_Nv2aMirrorFence(0x0019DCE0u, 0x30u, 0x34u) != 0)
        fprintf(stderr, "[NV2A] fence mirror registration failed\n");

    /* ── THE RECOMPILED START SET, PUBLISHED BEFORE THE WATCH CAN ARM ────────────────────────────
     *
     * ⚠ THE ORDER HERE IS LOAD-BEARING AND WAS A DEFECT WHEN IT WAS WRONG. The watch's ARM is
     * DEFERRED: its census poll thread arms as soon as `MEM32(0x0019DCE0)` names a plausible device,
     * and it can classify a faulting RIP from that instant. So the set the range classifier needs
     * must be PUBLISHED BEFORE that thread exists. Publishing it after xbox_A2hSlotWatchStart()
     * would leave a window in which faults classify UNKNOWN -- and UNKNOWN is INFRA FAILURE, so a
     * race would destroy the run rather than a defect.
     *
     * ⚠ AND IT NEEDS NO FLAT DISPATCH. `recomp_lookup` falls back to its own binary search over the
     * generated table when `recomp_dispatch_init()` has not run -- by design, so a caller may use it
     * without initialising anything. That is why this can sit BEFORE the init call below without
     * changing when the flat table is built: the probe is correct either way, and the OFF path stays
     * exactly as it was. */
    /* Gated like everything else in this facility: with JSRF_TRACE_A2H_SLOTW unset the probe loop
     * does not run at all, so an OFF run does no work and prints nothing. */
    if (xbox_A2hSlotWatchEnabled())
        a2h_publish_recomp_bounds();

    /* ── THE A2h LIVE SLOT-WRITE WATCH ─────────────────────────────────────────────────────────
     *
     * ARMED HERE, AND ONLY HERE, because this is the first point at which the title's own device is
     * known to be live: `MEM32(0x0019DCE0)` is a POINTER to an object JSRF allocates at runtime, so
     * the slot `+0x242C` has no address before that allocation. Arming earlier would read a zero and
     * protect nothing; hardcoding 0x0019D62C would protect whatever happened to be there. The
     * toolkit re-reads the pointer, derives the slot by checked 32-bit addition, protects that page
     * in the canonical view and in every mapped mirror alias, and re-derives it again at TERMINAL.
     *
     * It is called AFTER AddVectoredExceptionHandler above and AFTER the toolkit's own handlers are
     * installed, so the VEH order a reader re-verifies is the order that was actually in force.
     * Gated by JSRF_TRACE_A2H_SLOTW: with it unset this call returns immediately, no page is
     * protected, no thread is created and nothing is printed -- the run is byte-for-byte the run it
     * was before this existed. */
    xbox_A2hSlotWatchStart();
    /* Q3(c): START THE INDEPENDENT READER. The Advisor requires cross-validation "where fault-record
     * and hook reads overlap"; this is the hook side, and it is started only when the watch's own
     * gate is set, so an OFF run creates no thread and reads nothing. */
    a2h_cross_validate_start();

    g_esp = XBOX_STACK_TOP;
    recomp_diag_thread_start(YOUR_GAME_ENTRY_POINT, XBOX_STACK_BASE, XBOX_STACK_TOP + 16);

    if (launch.has_probe) {
        int result = jsrf_run_probe(probe_utf8);
        recomp_diag_thread_end();
        xbox_kernel_shutdown();
        nv2a_hook_shutdown();
        a2h_cross_validate_stop();
        xbox_A2hSlotWatchStop();
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
    a2h_cross_validate_stop();
    xbox_A2hSlotWatchStop();
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
