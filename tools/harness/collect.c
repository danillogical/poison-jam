/* External Windows debugger: freeze on second-chance exceptions/deadlines,
 * collect native stacks and a dump, then terminate only the launched child. */
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/diagnostics.h"
#include "../../src/jsrf_save_root.h"

static HANDLE process;
static DWORD process_id;
static FILE *report;
static const char *out_dir;
static unsigned thread_count, named_frames;
static DWORD64 ram_base;
static ULONG ram_size;
static DWORD64 guest_offset;
static int memory_added;
static struct { DWORD64 base; ULONG size; } extra_memory[3 + JSRF_THREAD_CAPACITY * JSRF_REGISTER_COUNT];
static unsigned extra_count, extra_next;

static void capture_guest_threads(void);

static int read_remote(DWORD64 address, void *buffer, SIZE_T size)
{
    SIZE_T got = 0;
    return ReadProcessMemory(process, (void *)(uintptr_t)address, buffer, size, &got) && got == size;
}

static DWORD64 symbol_address(const char *name)
{
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    memset(buffer, 0, sizeof(buffer));
    symbol->SizeOfStruct = sizeof(*symbol);
    symbol->MaxNameLen = MAX_SYM_NAME;
    return SymFromName(process, name, symbol) ? symbol->Address : 0;
}

/* These apertures are separate allocations in the current memory layout,
 * not aliases of the canonical RAM. Never fault in pages or read a future
 * trapped MMIO device: only include an entirely committed readable region. */
static void capture_backed_region(const char *name, uint32_t va, ULONG size)
{
    DWORD64 base = guest_offset + va, cursor = base, end = base + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION region;
        if (!VirtualQueryEx(process, (void *)(uintptr_t)cursor, &region, sizeof(region)) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(region.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                               PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
            fprintf(report, "DUMP_REGION_SKIPPED name=%s va=%08X (not fully readable)\n", name, va);
            return;
        }
        DWORD64 next = (DWORD64)(uintptr_t)region.BaseAddress + region.RegionSize;
        if (next <= cursor) return;
        cursor = next;
    }
    if (extra_count >= sizeof(extra_memory) / sizeof(extra_memory[0])) {
        fprintf(report, "DUMP_REGION_SKIPPED name=%s (capture capacity)\n", name);
        return;
    }
    extra_memory[extra_count].base = base;
    extra_memory[extra_count++].size = size;
    fprintf(report, "DUMP_REGION name=%s va=%08X address=%016llX size=%lu\n",
            name, va, (unsigned long long)base, size);
}

#include "gpu_capture.h"

static void frame_line(DWORD64 pc)
{
    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    DWORD64 displacement = 0;
    DWORD line_displacement = 0;
    IMAGEHLP_LINE64 line = {0};
    memset(buffer, 0, sizeof(buffer));
    symbol->SizeOfStruct = sizeof(*symbol);
    symbol->MaxNameLen = MAX_SYM_NAME;
    fprintf(report, "  %016llX", (unsigned long long)pc);
    if (SymFromAddr(process, pc, &displacement, symbol)) {
        fprintf(report, " %s+0x%llX", symbol->Name, (unsigned long long)displacement);
        named_frames++;
    }
    line.SizeOfStruct = sizeof(line);
    if (SymGetLineFromAddr64(process, pc, &line_displacement, &line))
        fprintf(report, " %s:%lu", line.FileName, line.LineNumber);
    fputc('\n', report);
}

static BOOL CALLBACK dump_callback(PVOID unused, const MINIDUMP_CALLBACK_INPUT *in,
                                   MINIDUMP_CALLBACK_OUTPUT *out)
{
    (void)unused;
    if (in->CallbackType == MemoryCallback) {
        if (!memory_added && ram_size) {
            memory_added = 1;
            out->MemoryBase = ram_base; out->MemorySize = ram_size;
        } else if (extra_next < extra_count) {
            out->MemoryBase = extra_memory[extra_next].base;
            out->MemorySize = extra_memory[extra_next++].size;
        } else return FALSE;
    }
    return TRUE;
}

static int capture(DWORD fault_tid, const EXCEPTION_DEBUG_INFO *fault)
{
    char path[MAX_PATH];
    HANDLE snapshot, file;
    THREADENTRY32 thread = {sizeof(thread)};
    MINIDUMP_CALLBACK_INFORMATION callback = {dump_callback, NULL};
    MINIDUMP_EXCEPTION_INFORMATION exception_info;
    EXCEPTION_POINTERS pointers;
    CONTEXT fault_context = {0};
    EXCEPTION_RECORD fault_record;
    int ok;
    /* A process can request several diagnostic captures. Do not accumulate
     * stale thread counts, region callbacks or RAM identity across them. */
    thread_count = named_frames = 0;
    extra_count = extra_next = 0;
    ram_base = ram_size = guest_offset = 0;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS |
                  SYMOPT_FAIL_CRITICAL_ERRORS);
    if (!SymInitialize(process, out_dir, TRUE))
        fprintf(report, "SymInitialize failed: %lu\n", GetLastError());
    /* Include the canonical guest RAM once, not all 32 mirror mappings. */
    {
        uint64_t offset = 0, size = 0;
        DWORD64 addr = symbol_address("g_xbox_mem_offset");
        if (addr && read_remote(addr, &offset, sizeof(offset)) && offset) {
            guest_offset = offset;
            addr = symbol_address("g_xbox_total_ram");
            if (addr && read_remote(addr, &size, sizeof(size)) && size <= 128 * 1024 * 1024) {
                ram_base = offset + 0x10000;
                ram_size = (ULONG)(size - 0x10000);
            }
        }
    }
    capture_gpu_snapshot(fault ? "unhandled_exception" : "capture", fault_tid, 0);
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE && Thread32First(snapshot, &thread)) do {
        HANDLE handle;
        CONTEXT context = {0};
        STACKFRAME64 frame = {0};
        DWORD64 previous = 0;
        if (thread.th32OwnerProcessID != process_id) continue;
        handle = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, thread.th32ThreadID);
        if (!handle) continue;
        context.ContextFlags = CONTEXT_ALL;
        fprintf(report, "THREAD %lu%s\n", thread.th32ThreadID,
                thread.th32ThreadID == fault_tid ? " FAULT" : "");
        if (GetThreadContext(handle, &context)) {
            thread_count++;
            if (thread.th32ThreadID == fault_tid) fault_context = context;
            frame.AddrPC.Offset = context.Rip;
            frame.AddrStack.Offset = context.Rsp;
            frame.AddrFrame.Offset = context.Rbp;
            frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
            frame_line(context.Rip);
            for (unsigned i = 0; i < 80; ++i) {
                if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, handle, &frame,
                        &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL)) break;
                if (!frame.AddrPC.Offset || frame.AddrPC.Offset == previous) break;
                previous = frame.AddrPC.Offset;
                frame_line(frame.AddrPC.Offset);
            }
        } else fprintf(report, "  GetThreadContext error %lu\n", GetLastError());
        CloseHandle(handle);
    } while (Thread32Next(snapshot, &thread));
    if (snapshot != INVALID_HANDLE_VALUE) CloseHandle(snapshot);
    capture_guest_threads();
    if (guest_offset) {
        /* Match XBOX_CONTIG_BASE/SIZE and XBOX_NV2A_BASE/SIZE in the toolkit.
         * Captures include the reserved GPU instance area at the window's top. */
        capture_backed_region("contiguous", 0x80000000u, 64u * 1024u * 1024u);
        capture_backed_region("nv2a_registers", 0xFD000000u, 16u * 1024u * 1024u);
    }
    if (fault) {
        fault_record = fault->ExceptionRecord;
        pointers.ExceptionRecord = &fault_record;
        pointers.ContextRecord = &fault_context;
        exception_info.ThreadId = fault_tid;
        exception_info.ExceptionPointers = &pointers;
        exception_info.ClientPointers = FALSE;
    }
    snprintf(path, sizeof(path), "%s\\process.dmp", out_dir);
    file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    memory_added = 0; extra_next = 0;
    ok = file != INVALID_HANDLE_VALUE && MiniDumpWriteDump(process, process_id, file,
        MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules | MiniDumpWithProcessThreadData,
        fault ? &exception_info : NULL, NULL, &callback);
    fprintf(report, "DUMP ok=%d error=%lu guest_ram=%016llX+%lu threads=%u named_frames=%u\n",
        ok, ok ? 0 : GetLastError(), (unsigned long long)ram_base, ram_size, thread_count, named_frames);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    SymCleanup(process);
    fflush(report);
    return ok;
}

static void capture_guest_threads(void)
{
    DWORD64 address = symbol_address("g_jsrf_debug");
    JsrfRegistry *registry = malloc(sizeof(*registry));
    const char *names[] = {"eax","ecx","edx","esp","ebx","esi","edi","ebp","seh","fs"};
    const char *kinds[] = {"unknown","call","tail","thread_start","thread_exit",
        "lock_wait","lock_acquired","lock_release","wait_begin","wait_end","signal","reset"};
    if (!registry) return;
    if (!address || !read_remote(address, registry, sizeof(*registry))) {
        fprintf(report, "Guest registry unavailable\n"); free(registry); return;
    }
    extra_count = 0;
    extra_memory[extra_count].base=address; extra_memory[extra_count++].size=sizeof(*registry);
    fprintf(report, "GUEST_REGISTRY address=%016llX version=%u claimed=%u overflow=%u qpc_frequency=%llu\n",
            (unsigned long long)address, registry->version, registry->claimed, registry->overflow,
            (unsigned long long)registry->frequency);
    /* The A2h NULL-slot latch is part of this struct and is therefore already archived by the
     * line above. Print its own fields too, because they are DECISION INPUTS: without this line a
     * reader sees only the THREAD registry's `claimed` and would have to extract the latch from
     * the minidump to learn whether the install control passed. The latch's verdict belongs in
     * the archive's own text, next to the data it describes.
     *
     * Guarded on the version, because the latch was appended and an older registry has no such
     * field -- reading it unconditionally would misreport whatever bytes follow the threads. */
    if (registry->version >= 2) {
        const JsrfSlotLatch *latch = &registry->latch;
        fprintf(report, "GUEST_SLOT_LATCH install_seen=%u install_raw=%08X install_value=%08X "
                        "install_ok=%u claimed=%u overflow=%u partial=%u sequence=%u\n",
                latch->install_seen, latch->install_raw, latch->install_value, latch->install_ok,
                latch->claimed, latch->overflow, latch->partial, latch->sequence);
        for (unsigned s = 0; s < JSRF_LATCH_CAPACITY; s++) {
            const JsrfSlotTransition *t = &latch->slots[s];
            if (!t->valid && !t->tid && !t->call_index && !t->before && !t->after) continue;
            fprintf(report, "GUEST_SLOT_TRANSITION slot=%u tid=%u call=%u ordinal=%u "
                            "before=%08X after=%08X intra=%u ticks=%llu\n",
                    s, t->tid, t->call_index, t->ordinal, t->before, t->after, t->intra,
                    (unsigned long long)t->ticks);
        }
    }
    for (unsigned i=0;i<registry->claimed && i<JSRF_THREAD_CAPACITY;i++) {
        JsrfThread *thread = &registry->threads[i];
        uint32_t registers[JSRF_REGISTER_COUNT] = {0};
        fprintf(report, "GUEST_THREAD identity=%u tid=%u state=%u start=%08X stack=%08X..%08X events=%u overwritten=%u\n",
            thread->identity, thread->tid, thread->state, thread->start,
            thread->stack_low, thread->stack_high, thread->count,
            thread->count > JSRF_EVENT_CAPACITY ? thread->count - JSRF_EVENT_CAPACITY : 0);
        /* Exited slots retain history but their TLS pointers must not be read. */
        if (thread->state == 1) {
            for (unsigned r=0;r<JSRF_REGISTER_COUNT;r++) {
                if (read_remote(thread->registers[r], &registers[r], 4))
                    fprintf(report, " %s=%08X", names[r], registers[r]);
                extra_memory[extra_count].base=thread->registers[r];
                extra_memory[extra_count++].size=4;
            }
            fputc('\n', report);
            uint64_t sp = registers[3];
            for (unsigned k=0;k<128 && sp>=thread->stack_low && sp+4<=thread->stack_high;k++,sp+=4) {
                uint32_t word;
                uint64_t offset = ram_base ? ram_base - 0x10000 : 0;
                if (!offset || !read_remote(offset+sp,&word,4)) break;
                fprintf(report," GS %08llX %08X\n",(unsigned long long)sp,word);
            }
        }
        uint32_t first = thread->count > JSRF_EVENT_CAPACITY ? thread->count-JSRF_EVENT_CAPACITY : 0;
        for (uint32_t n=first;n<thread->count;n++) {
            JsrfEvent *event = &thread->events[n%JSRF_EVENT_CAPACITY];
            if (event->sequence != n*2+2) {
                fprintf(report," EVENT %u incomplete\n",n); continue;
            }
            fprintf(report," EVENT %u ticks=%llu kind=%u target=%08X site=%08X esp=%08X value=%08X name=%s\n",
                n,(unsigned long long)event->ticks,event->kind,event->target,event->site,event->esp,event->value,
                event->kind<sizeof(kinds)/sizeof(kinds[0]) ? kinds[event->kind] : "unknown");
        }
    }
    free(registry);
}

int main(int argc, char **argv)
{
    STARTUPINFOA startup = {sizeof(startup)};
    PROCESS_INFORMATION child = {0};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    HANDLE job;
    char command[32767], path[MAX_PATH];
    size_t command_used = 0;
    ULONGLONG deadline, break_deadline = 0;
    DWORD exit_code = 0;
    const char *outcome = "collector_failure";
    int initial_break = 1, dump_ok = 0, finished = 0;
    if (argc < 4) { fprintf(stderr, "usage: jsrf_collect seconds output-dir executable [game-arg ...]\n"); return 2; }
    out_dir = argv[2];
    snprintf(path, sizeof(path), "%s\\stacks.txt", out_dir);
    report = fopen(path, "w");
    if (!report) return 2;
    command[0] = '\0';
    if (!jsrf_append_windows_arg(command, sizeof(command), &command_used, argv[3])) {
        fprintf(report, "Child command line exceeds the Windows command limit.\n");
        fclose(report);
        return 2;
    }
    for (int arg = 4; arg < argc; ++arg) {
        if (!jsrf_append_windows_arg(command, sizeof(command), &command_used, argv[arg])) {
            fprintf(report, "Child command line exceeds the Windows command limit.\n");
            fclose(report);
            return 2;
        }
    }
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    job = CreateJobObjectA(NULL, NULL);
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return 2;
    if (!CreateProcessA(NULL, command, NULL, NULL, FALSE,
            DEBUG_ONLY_THIS_PROCESS | CREATE_SUSPENDED, NULL, NULL, &startup, &child)) {
        fprintf(report, "CreateProcess error=%lu\n", GetLastError()); return 2;
    }
    process = child.hProcess;
    process_id = child.dwProcessId;
    if (!AssignProcessToJobObject(job, process)) { TerminateProcess(process, 2); return 2; }
    ResumeThread(child.hThread);
    CloseHandle(child.hThread);
    deadline = GetTickCount64() + strtoul(argv[1], NULL, 10) * 1000ull;
    while (!finished) {
        DEBUG_EVENT event;
        DWORD continuation = DBG_CONTINUE;
        if (!break_deadline && GetTickCount64() >= deadline) {
            if (!DebugBreakProcess(process)) break;
            break_deadline = GetTickCount64() + 5000;
        } else if (break_deadline && GetTickCount64() >= break_deadline) break;
        if (!WaitForDebugEvent(&event, 100)) {
            if (GetLastError() != ERROR_SEM_TIMEOUT) break;
            continue;
        }
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
        } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            if (event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
        } else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            EXCEPTION_DEBUG_INFO *exception = &event.u.Exception;
            DWORD code = exception->ExceptionRecord.ExceptionCode;
            fprintf(report, "DEBUG_EXCEPTION tid=%lu code=%08lX first=%lu\n",event.dwThreadId,code,exception->dwFirstChance);
            if (code == EXCEPTION_BREAKPOINT && initial_break) initial_break = 0;
            else if (code == EXCEPTION_BREAKPOINT && break_deadline) {
                outcome = "diagnostic_deadline";
                dump_ok = capture(0, NULL);
                exit_code = 3;
                finished = 1;
            } else if (code == 0xE0424750 && exception->dwFirstChance) {
                /* Cooperative GPU observation. The target's handler receives
                 * the exception afterward; this never indicates GPU success. */
                capture_gpu_event(event.dwThreadId, exception->ExceptionRecord.NumberParameters ?
                                  exception->ExceptionRecord.ExceptionInformation[0] : 0);
                continuation = DBG_EXCEPTION_NOT_HANDLED;
            } else if (code == 0xE0424243 && exception->dwFirstChance &&
                       jsrf_argv_has_probe(argc, argv)) {
                /* Explicit fixture snapshot request; the fixture handles it. */
                dump_ok = capture(0, NULL);
                continuation = DBG_EXCEPTION_NOT_HANDLED;
            } else if (!exception->dwFirstChance) {
                outcome = "unhandled_exception";
                exit_code = code;
                fprintf(report, "UNHANDLED tid=%lu exception=%08lX address=%p\n", event.dwThreadId,
                        code, exception->ExceptionRecord.ExceptionAddress);
                dump_ok = capture(event.dwThreadId, exception);
                finished = 1;
            } else continuation = DBG_EXCEPTION_NOT_HANDLED;
        } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            exit_code = event.u.ExitProcess.dwExitCode;
            outcome = "normal_exit";
            finished = 1;
        }
        if (finished && strcmp(outcome, "normal_exit")) TerminateProcess(process, exit_code);
        ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continuation);
    }
    CloseHandle(job); /* also kills the child on collector failure */
    CloseHandle(process);
    fclose(report);
    snprintf(path, sizeof(path), "%s\\result.json", out_dir);
    report = fopen(path, "w");
    if (!report) return 2;
    fprintf(report, "{\"outcome\":\"%s\",\"exit_code\":%lu,\"dump_ok\":%s,"
            "\"native_threads\":%u,\"named_frames\":%u,\"gpu_snapshots\":%u,\"gpu_snapshots_dropped\":%u}\n",
            outcome, exit_code, dump_ok ? "true" : "false", thread_count, named_frames,
            gpu_snapshot_count, gpu_snapshot_dropped);
    fclose(report);
    return !strcmp(outcome, "normal_exit") ? (exit_code ? 1 : 0) :
           !strcmp(outcome, "collector_failure") || !dump_ok ? 2 : 3;
}
