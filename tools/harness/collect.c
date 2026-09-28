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

/* ── A2h slot-write watch (observation only, OFF by default) ─────────────────────────────────
 *
 * WHY: the terminal event of the trapped run is a raw-zero indirect call through the kernel thunk
 * slot at guest VA 0x001C4064 -- a slot whose image content is a valid ordinal-277 thunk and whose
 * installed value is 0xFE000104. A prior run read 0xFE000104 at every one of 15498 sampled bridge
 * boundaries and then read 0 at the terminal read, so the change happened in a gap with no
 * bracketing sample. This watch exists to decide whether the slot was written in that gap, and
 * whether a guest or a host thread did it.
 *
 * NO ADDRESS PAYLOAD CROSSES THE PROCESS BOUNDARY. The child maps the same backing at 29 linear
 * addresses (canonical plus 28 mirrors) at a fixed stride, so every watched address is computable
 * HERE from two symbols this collector already resolves:
 *
 *     canonical host = g_xbox_mem_offset + 0x001C4064
 *     alias m host   = g_xbox_mem_offset + (m+1) * ram_size + 0x001C4064     (m = 0..27)
 *
 * DR0 matches a LINEAR native address, so the canonical watch does NOT cover the mirrors; the
 * mirrors are covered by the toolkit's page census, not here. The handshake's job is therefore
 * only signal -> arm -> acknowledge.
 *
 * OFF BY DEFAULT: JSRF_TRACE_A2H_DR is read once and cached. Absent it this file behaves exactly
 * as before -- no DR is programmed, no single-step event is claimed, no handle mask changes. */
#define JSRF_A2H_DR_GATE "JSRF_TRACE_A2H_DR"
#define JSRF_A2H_DR_HANDSHAKE 0xE0424452u   /* distinct from 0xE0424750/0xE0424243/0xE0424943/0xE0424845 */
#define A2H_SLOT_VA 0x001C4064u
#define A2H_ALIASES 28u
/* DR7 = L0 (enable DR0) | RW0 = 01 (write only) | LEN0 = 11 (4 bytes). LE/GE stay CLEAR so the
 * processor reports the data breakpoint AFTER the storing instruction, which is the ordering the
 * writer attribution assumes. */
#define A2H_DR7 0x000D0001u
/* Bits this instrument owns: L0-G3 (0-7), LE/GE (8-9), RW0/LEN0 (16-19). The readback check
 * masks to these, because the CPU forces reserved DR7 bit 10 to 1 and an exact compare would
 * report a correctly armed watch as a failure. */
#define A2H_DR7_OWNED 0x000F03FFu
#define A2H_ARM_TID_CAPACITY 256
#define A2H_DR_HIT_CAPACITY 64

static int read_remote(DWORD64 address, void *buffer, SIZE_T size);
static DWORD64 symbol_address(const char *name);

static int dr_gate_read, dr_enabled;
static DWORD64 dr_canonical;
static unsigned dr_armed, dr_failed, dr_collision, dr_disarmed, dr_disarm_failed;
static DWORD dr_arm_tids[A2H_ARM_TID_CAPACITY];
static unsigned dr_arm_tid_count, dr_arm_tid_overflow;
static struct { DWORD tid; DWORD64 dr0, dr7; } dr_prev[A2H_ARM_TID_CAPACITY];
static unsigned dr_prev_count, dr_prev_overflow;
static struct { DWORD tid; DWORD64 dr6, rip; uint32_t value; DWORD64 ticks; } dr_hit[A2H_DR_HIT_CAPACITY];
static unsigned dr_hits, dr_hit_overflow;

static int dr_on(void)
{
    if (!dr_gate_read) { dr_enabled = getenv(JSRF_A2H_DR_GATE) != NULL; dr_gate_read = 1; }
    return dr_enabled;
}

/* Resolve the mapping geometry from symbols the collector already reads. Returns 1 when the
 * canonical host address is known. Called by capture() and, when gated, at the handshake. */
static int resolve_geometry(void)
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
    dr_canonical = guest_offset ? guest_offset + A2H_SLOT_VA : 0;
    return dr_canonical != 0;
}

static int dr_arm_thread(HANDLE thread, DWORD tid, const char *why)
{
    CONTEXT context = {0}, back = {0};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &context)) {
        dr_failed++;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=get error=%lu\n", tid, why, GetLastError());
        return 0;
    }
    if (context.Dr1 || context.Dr2 || context.Dr3 || (context.Dr7 & A2H_DR7_OWNED) || context.Dr6) {
        /* Another debug owner already holds breakpoint state on this thread. Do NOT clobber it:
         * an unreconciled DR owner makes every hit ambiguous, which is a coverage failure -- not a
         * reason to overwrite somebody else's watch. */
        dr_collision++;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=collision dr6=%016llX dr7=%016llX\n",
                tid, why, (unsigned long long)context.Dr6, (unsigned long long)context.Dr7);
        return 0;
    }
    if (dr_prev_count < A2H_ARM_TID_CAPACITY) {
        dr_prev[dr_prev_count].tid = tid;
        dr_prev[dr_prev_count].dr0 = context.Dr0;
        dr_prev[dr_prev_count].dr7 = context.Dr7;
        dr_prev_count++;
    } else dr_prev_overflow = 1;
    context.Dr0 = dr_canonical;
    context.Dr1 = context.Dr2 = context.Dr3 = 0;
    context.Dr7 = A2H_DR7;
    if (!SetThreadContext(thread, &context)) {
        dr_failed++;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=set error=%lu\n", tid, why, GetLastError());
        return 0;
    }
    back.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &back) || back.Dr0 != dr_canonical ||
        (back.Dr7 & A2H_DR7_OWNED) != A2H_DR7) {
        dr_failed++;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=readback dr0=%016llX dr7=%016llX error=%lu\n",
                tid, why, (unsigned long long)back.Dr0, (unsigned long long)back.Dr7, GetLastError());
        return 0;
    }
    dr_armed++;
    if (dr_arm_tid_count < A2H_ARM_TID_CAPACITY) dr_arm_tids[dr_arm_tid_count++] = tid;
    else dr_arm_tid_overflow = 1;
    return 1;
}

/* Arm every live thread of the child. Called with the install thread STOPPED at the handshake, so
 * the toolkit's install store cannot execute before this returns. */
static void dr_arm_all(const char *why)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry = {sizeof(entry)};
    if (snapshot == INVALID_HANDLE_VALUE) {
        dr_failed++;
        fprintf(report, "GUEST_DR_ARM_FAIL why=%s stage=snapshot error=%lu\n", why, GetLastError());
        return;
    }
    if (Thread32First(snapshot, &entry)) do {
        HANDLE handle;
        if (entry.th32OwnerProcessID != process_id) continue;
        handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                            FALSE, entry.th32ThreadID);
        if (!handle) {
            dr_failed++;
            fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=open error=%lu\n",
                    entry.th32ThreadID, why, GetLastError());
            continue;
        }
        dr_arm_thread(handle, entry.th32ThreadID, why);
        CloseHandle(handle);
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
}

/* Greppable, self-describing coverage line: a reader must be able to see from stacks.txt alone
 * whether arming SUCCEEDED, without running a tool. `ok=0` with failed>0 is a coverage failure. */
static void dr_print_arm_summary(const char *why)
{
    fprintf(report, "GUEST_DR_ARM why=%s ok=%d armed=%u failed=%u collision=%u canonical=%016llX "
                    "aliases=%u dr7=%08llX tids=%u%s%s\n",
            why, (dr_failed == 0 && dr_armed > 0) ? 1 : 0, dr_armed, dr_failed, dr_collision,
            (unsigned long long)dr_canonical, A2H_ALIASES, (unsigned long long)A2H_DR7,
            dr_arm_tid_count, dr_arm_tid_overflow ? " tid_overflow=1" : "",
            dr_prev_overflow ? " prev_overflow=1" : "");
    for (unsigned i = 0; i < dr_arm_tid_count; i++)
        fprintf(report, "GUEST_DR_ARM_TID index=%u tid=%lu\n", i, dr_arm_tids[i]);
    fflush(report);
}

static void dr_handshake(DWORD tid)
{
    if (!dr_canonical) {
        /* Symbol resolution needs an initialized symbol handler; capture() has not run yet at the
         * handshake, so initialize it here. Same options capture() uses, so symbol lookups behave
         * identically whichever path initializes first. */
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS |
                      SYMOPT_FAIL_CRITICAL_ERRORS);
        SymInitialize(process, out_dir, TRUE);
        resolve_geometry();
    }
    if (!dr_canonical) {
        dr_failed++;
        fprintf(report, "GUEST_DR_ARM why=handshake ok=0 armed=0 failed=%u reason=no_mapping_offset "
                        "handshake_tid=%lu\n", dr_failed, tid);
        fflush(report);
        return;
    }
    dr_arm_all("handshake");
    dr_print_arm_summary("handshake");
}

static int dr_was_armed(DWORD tid)
{
    for (unsigned i = 0; i < dr_arm_tid_count; i++)
        if (dr_arm_tids[i] == tid) return 1;
    return 0;
}

/* A #DB in a watched thread. Services ONLY this instrument's DR6.B0: BS (0x4000), B1-B3
 * (0x2,0x4,0x8) and TF belong to whoever else set them, so a mixed status is passed on and the run
 * is coverage-uncertain rather than credited with a canonical write. */
static DWORD dr_handle_single_step(DWORD tid)
{
    HANDLE handle;
    CONTEXT context = {0};
    DWORD64 dr6;
    uint32_t value = 0;
    LARGE_INTEGER now;
    /* ONLY THREADS THIS INSTRUMENT ARMED. A B0 on a thread we refused to arm means somebody else
     * owns DR0 there; servicing it would clear their status bit and corrupt their watch. The
     * collision check in dr_arm_thread already refused those threads, and this is the other half
     * of that promise. */
    if (!dr_was_armed(tid)) {
        fprintf(report, "GUEST_DR_HIT_UNSERVICED tid=%lu reason=not_armed_by_this_collector\n", tid);
        fflush(report);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                        FALSE, tid);
    if (!handle) {
        fprintf(report, "GUEST_DR_HIT_UNSERVICED tid=%lu stage=open error=%lu\n", tid, GetLastError());
        fflush(report);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(handle, &context)) {
        fprintf(report, "GUEST_DR_HIT_UNSERVICED tid=%lu stage=get error=%lu\n", tid, GetLastError());
        CloseHandle(handle);
        fflush(report);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    dr6 = context.Dr6;
    if (!(dr6 & 0x1)) {          /* not our B0: pass it on untouched */
        CloseHandle(handle);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    /* Post-hit read. The packet is explicit that this value can ALREADY reflect a competing write,
     * so it is recorded as the raw observation it is; attribution happens offline. */
    read_remote(dr_canonical, &value, sizeof(value));
    QueryPerformanceCounter(&now);
    if (dr_hits < A2H_DR_HIT_CAPACITY) {
        dr_hit[dr_hits].tid = tid;
        dr_hit[dr_hits].dr6 = dr6;
        dr_hit[dr_hits].rip = context.Rip;
        dr_hit[dr_hits].value = value;
        dr_hit[dr_hits].ticks = (DWORD64)now.QuadPart;
        dr_hits++;
    } else dr_hit_overflow++;
    fprintf(report, "GUEST_DR_HIT tid=%lu dr6=%016llX rip=%016llX canonical=%016llX value=%08X "
                    "ticks=%llu\n", tid, (unsigned long long)dr6, (unsigned long long)context.Rip,
            (unsigned long long)dr_canonical, value, (unsigned long long)now.QuadPart);
    fflush(report);
    if (dr6 & ~(DWORD64)0x1) {
        /* MIXED STATUS: someone else's bit is set in the same DR6. Clearing B0 would be ours to
         * do, but the event cannot be attributed to this watch alone, so it is not claimed. */
        fprintf(report, "GUEST_DR_HIT_MIXED tid=%lu dr6=%016llX\n", tid, (unsigned long long)dr6);
        fflush(report);
        CloseHandle(handle);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    context.Dr6 = dr6 & ~(DWORD64)0x1;   /* clear ONLY the owned status */
    if (!SetThreadContext(handle, &context))
        fprintf(report, "GUEST_DR_HIT_UNSERVICED tid=%lu stage=clear_dr6 error=%lu\n", tid, GetLastError());
    CloseHandle(handle);
    fflush(report);
    return DBG_CONTINUE;
}

/* Teardown: the ruling requires STRUCTURAL proof the instrument is disarmed, so every thread's DR7
 * is written back and read back, and the result is printed. */
static void dr_disarm_all(void)
{
    HANDLE snapshot;
    THREADENTRY32 entry = {sizeof(entry)};
    if (!dr_on() || !dr_canonical) return;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        fprintf(report, "GUEST_DR_DISARM ok=0 reason=snapshot error=%lu\n", GetLastError());
        fflush(report);
        return;
    }
    if (Thread32First(snapshot, &entry)) do {
        HANDLE handle;
        CONTEXT context = {0}, back = {0};
        if (entry.th32OwnerProcessID != process_id) continue;
        handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                            FALSE, entry.th32ThreadID);
        if (!handle) { dr_disarm_failed++; continue; }
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(handle, &context)) {
            dr_disarm_failed++; CloseHandle(handle); continue;
        }
        context.Dr0 = context.Dr1 = context.Dr2 = context.Dr3 = 0;
        context.Dr7 = 0;
        if (!SetThreadContext(handle, &context)) {
            dr_disarm_failed++; CloseHandle(handle); continue;
        }
        back.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(handle, &back) || (back.Dr7 & A2H_DR7_OWNED) || back.Dr0) {
            dr_disarm_failed++;
            fprintf(report, "GUEST_DR_DISARM_FAIL tid=%lu dr0=%016llX dr7=%016llX\n",
                    entry.th32ThreadID, (unsigned long long)back.Dr0, (unsigned long long)back.Dr7);
        } else dr_disarmed++;
        CloseHandle(handle);
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    fprintf(report, "GUEST_DR_DISARM ok=%d cleared=%u failed=%u dr7_nonzero=%u hits=%u hit_overflow=%u\n",
            dr_disarm_failed == 0 ? 1 : 0, dr_disarmed, dr_disarm_failed, dr_disarm_failed,
            dr_hits, dr_hit_overflow);
    fflush(report);
}

/* A2h install handshake. The toolkit raises this FIRST-CHANCE at its thunk-install callback and
 * does not perform the install store until this thread is continued, so arming here provably
 * precedes the install write. Observation only: no guest register, memory or device state is
 * touched, and the toolkit's own VEH still receives the exception afterwards
 * (DBG_EXCEPTION_NOT_HANDLED).
 *
 * A FIRST-CHANCE #DB IS DELIBERATELY NOT CLAIMED, even with the gate on. The collector's own
 * DebugBreakProcess arrives as a first-chance breakpoint, and the AC'97 page trap relies on its own
 * VEH seeing its single-step -- swallowing either would break a mechanism that predates this
 * instrument. So the watch is serviced on the second-chance path, which is where an unclaimed data
 * breakpoint surfaces, and nothing that another handler owns is intercepted. */
static DWORD dr_single_step(DWORD tid, int first_chance)
{
    if (first_chance) {
        fprintf(report, "GUEST_DR_HIT_FIRSTCHANCE tid=%lu (left to the target's own handlers)\n", tid);
        fflush(report);
        return DBG_EXCEPTION_NOT_HANDLED;
    }
    return dr_handle_single_step(tid);
}

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
    resolve_geometry();
    capture_gpu_snapshot(fault ? "unhandled_exception" : "capture", fault_tid, 0);
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot != INVALID_HANDLE_VALUE && Thread32First(snapshot, &thread)) do {
        HANDLE handle;
        CONTEXT context = {0};
        STACKFRAME64 frame = {0};
        DWORD64 previous = 0;
        if (thread.th32OwnerProcessID != process_id) continue;
        /* THREAD_SET_CONTEXT is REQUIRED for the gated A2h DR watch: without it SetThreadContext
         * fails on every thread and the instrument would silently arm nothing. It is requested
         * unconditionally because the flag is inert when the watch is off -- the collector's own
         * behaviour with the gate absent is unchanged. */
        handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                            FALSE, thread.th32ThreadID);
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
    /* Version 3: the slot-WRITE watch. Printed for the same reason the latch is printed -- these
     * are DECISION INPUTS, and a reader must be able to see the coverage facts (was the alias
     * census armed, was anything published, did the terminal witness run) in the archive's own
     * text rather than having to extract them from the minidump first. The frozen bytes remain
     * authoritative; this line is a cross-check that must reconcile with the extractor. */
    if (registry->version >= 3) {
        const JsrfSlotWriteWatch *w = &registry->watch;
        fprintf(report, "GUEST_SLOT_WATCH armed=%u alias_count=%u mapped_mask=%08X protect_mask=%08X "
                        "touched=%u publish_failed=%u handshake_seen=%u class_overflow=%u "
                        "class_claimed=%u terminal_seen=%u terminal_target=%08X terminal_slot=%08X "
                        "terminal_flags=%u arm_offset=%08X%08X terminal_offset=%08X%08X\n",
                w->armed, w->alias_count, w->mapped_mask, w->protect_mask, w->touched_count,
                w->publish_failed, w->handshake_seen, w->class_overflow, w->class_claimed,
                w->terminal_seen, w->terminal_target, w->terminal_slot, w->terminal_flags,
                w->arm_offset_hi, w->arm_offset_lo, w->terminal_offset_hi, w->terminal_offset_lo);
        for (unsigned c = 0; c < JSRF_WRITE_CLASS_CAPACITY; c++) {
            const JsrfWriteClass *k = &w->classes[c];
            fprintf(report, "GUEST_SLOT_WRITE class=%u count=%llu valid=%u tid=%u call=%u ordinal=%u "
                            "before=%08X after=%08X rip=%016llX ticks=%llu\n",
                    c, (unsigned long long)w->class_counts[c], k->valid, k->tid, k->call_index,
                    k->ordinal, k->before, k->after, (unsigned long long)k->rip,
                    (unsigned long long)k->ticks);
        }
        for (unsigned a = 0; a < JSRF_ALIAS_CAPACITY; a++) {
            const JsrfAliasTouch *t = &w->aliases[a];
            if (!t->valid) continue;
            fprintf(report, "GUEST_SLOT_ALIAS mirror=%u valid=%u mapped=%u published=%u fault_va=%08X "
                            "value=%08X rip=%016llX ticks=%llu\n",
                    a + 1, t->valid, t->mapped, t->published, t->fault_va, t->value,
                    (unsigned long long)t->rip, (unsigned long long)t->ticks);
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
        } else if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
            /* A NEW THREAD MUST BE ARMED WHILE IT IS STOPPED. This event is delivered before the
             * thread executes its first instruction, so arming here is the only point at which the
             * gated watch can be complete for a thread's whole life. Arming after ContinueDebugEvent
             * would leave a window in which the new thread could perform a watched write unarmed --
             * which is exactly the UNKNOWN/coverage failure the packet refuses to accept. */
            if (dr_on()) {
                if (resolve_geometry()) {
                    if (!dr_arm_thread(event.u.CreateThread.hThread, event.dwThreadId, "create_thread"))
                        dr_print_arm_summary("create_thread");
                } else {
                    dr_failed++;
                    fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=create_thread reason=no_mapping_offset\n",
                            event.dwThreadId);
                }
            }
            if (event.u.CreateThread.hThread) CloseHandle(event.u.CreateThread.hThread);
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
            } else if (code == EXCEPTION_SINGLE_STEP && dr_on() && !exception->dwFirstChance) {
                /* Only a SECOND-chance #DB is ours to service: an unclaimed data breakpoint. A
                 * first-chance #DB belongs to the target's own handlers (the AC'97 page trap among
                 * them) and is deliberately left alone. */
                continuation = dr_single_step(event.dwThreadId, exception->dwFirstChance);
            } else if (code == JSRF_A2H_DR_HANDSHAKE && exception->dwFirstChance && dr_on()) {
                /* A2h install handshake. The toolkit raises this FIRST-CHANCE at its thunk-install
                 * callback and does not perform the install store until this thread is continued,
                 * so arming here provably precedes the install write. Observation only: no guest
                 * register, memory or device state is touched, and the toolkit's own VEH still
                 * receives the exception afterwards (DBG_EXCEPTION_NOT_HANDLED).
                 *
                 * With the gate OFF this branch does not exist, so the exception takes the generic
                 * first-chance path below and the child behaves exactly as it did before. */
                dr_handshake(event.dwThreadId);
                continuation = DBG_EXCEPTION_NOT_HANDLED;
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
    if (dr_on()) dr_disarm_all();
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
