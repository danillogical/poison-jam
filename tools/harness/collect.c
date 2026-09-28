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
/* ONLY THE MEANINGFUL DR6 BITS MAY BE TESTED. MEASURED, NOT ASSUMED: on this host a real data
 * breakpoint delivers DR6 = 0xFFFF0FF1, because bits 4-11 and 12-15 are RESERVED on x86-64 and read
 * as 1. A test of the form `dr6 & ~1` is therefore ALWAYS TRUE, and the first version of this file
 * used exactly that -- so every genuine hit was classified "mixed" and never claimed, and the
 * instrument would have reported a silent absence of writes for a run that had one. This mask is
 * the set of bits the #DB contract actually gives meaning to: B0-B3, BD, BS, BT. */
#define A2H_DR6_MEANINGFUL 0x0000E00Fu
#define A2H_ARM_TID_CAPACITY 256
#define A2H_DR_HIT_CAPACITY 64
/* C2 per-birth ledger capacity. Same bounded pattern as dr_arm_tids above: a fixed array plus an
 * explicit overflow flag, so a truncated ledger is VISIBLE as truncated and can never be read as a
 * complete one. No run-length-sized history is kept anywhere in this file. */
#define A2H_BIRTH_CAPACITY 256

/* The three counters below answer three DIFFERENT questions and must never be substituted for one
 * another. Recorded here because all three were confused in this archive:
 *   dr_armed            -- successful arms this process performed. POSITIVE per-arm records back it.
 *   dr_arm_tid_count    -- tids armed so far, as a bounded list. UNDERCOUNTS arms when it overflows,
 *                          and before this change it was only printed at the handshake, so every
 *                          later arm was invisible.
 *   dr_disarmed         -- live threads whose DRs were zeroed cleanly at teardown. That walks EVERY
 *                          live thread unconditionally (dr_disarm_all), so it counts threads that
 *                          EXISTED at teardown whether or not they were ever armed: it OVERCOUNTS
 *                          arms and is reported below explicitly as not an arm count. */

/* why= on an arming observation. */
enum { A2H_WHY_HANDSHAKE = 0, A2H_WHY_CREATE_THREAD = 1, A2H_WHY_EXIT = 2 };
/* state= on a per-birth row: the outcome of this tid's arming, as observed at the event. */
enum { A2H_BIRTH_ARMED = 0, A2H_BIRTH_DEFERRED = 1, A2H_BIRTH_FAILED = 2, A2H_BIRTH_EXITED = 3 };
/* reason= on a per-birth row. DEFERRED and FAILED are kept apart on purpose: DEFERRED means the
 * attempt could not be made yet (mapping not published) and the handshake sweep is expected to
 * recover it; FAILED means an attempt was made and lost. A failed attempt that the sweep later
 * recovered is reported separately as recovered, and is NOT a terminal unarmed tid. */
enum { A2H_REASON_NONE = 0, A2H_REASON_NO_MAPPING_OFFSET = 1, A2H_REASON_COLLISION = 2,
       A2H_REASON_GET = 3, A2H_REASON_SET = 4, A2H_REASON_READBACK = 5, A2H_REASON_OPEN = 6,
       A2H_REASON_SNAPSHOT = 7 };

static int read_remote(DWORD64 address, void *buffer, SIZE_T size);
static DWORD64 symbol_address(const char *name);

static int dr_gate_read, dr_enabled;
static DWORD64 dr_canonical;
static int dr_geometry_pinned;
static unsigned dr_armed, dr_failed, dr_collision, dr_disarmed, dr_disarm_failed;
/* Declared here, after the A2H_REASON_* enum above, so its initializer is valid. */
static unsigned dr_last_fail_reason = A2H_REASON_NONE;
static DWORD dr_arm_tids[A2H_ARM_TID_CAPACITY];
static unsigned char dr_arm_tid_why[A2H_ARM_TID_CAPACITY];
static unsigned dr_arm_tid_count, dr_arm_tid_overflow;
static struct { DWORD tid; DWORD64 dr0, dr7; } dr_prev[A2H_ARM_TID_CAPACITY];
static unsigned dr_prev_count, dr_prev_overflow;
static struct { DWORD tid; DWORD64 dr6, rip; uint32_t value; DWORD64 ticks; } dr_hit[A2H_DR_HIT_CAPACITY];
static unsigned dr_hits, dr_hit_overflow;

/* C1/C2 event-time ledger. dr_seq is the monotonic sequence shared by every arming observation and
 * every per-birth row, so a reader can order arms against births and against the handshake without
 * relying on line numbers or on the collector's own print ordering. */
static unsigned dr_seq, dr_arm_ok_records, dr_armed_before_handshake, dr_armed_at_or_after_handshake;
static unsigned dr_create_thread_events, dr_exit_events;
static unsigned dr_distinct_armed;
static unsigned dr_handshake_seq, dr_terminal_seq;
static int dr_handshake_seen;
static unsigned dr_last_fail_reason;
/* One row per lifecycle event, never per attempt-over-time: a reused tid yields a second row, which
 * is how lifecycle identity is preserved rather than overwritten. */
static struct { unsigned seq, why, mapping_available, state, reason; DWORD tid; DWORD64 ticks; }
    dr_birth[A2H_BIRTH_CAPACITY];
static unsigned dr_birth_count, dr_birth_overflow;

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
    DWORD64 addr;
    /* Fixture-only pin. tests/test_collect_arming.c has no debuggee, so it cannot read the mapping
     * symbols out of one; it pins the geometry instead. Nothing in the collector ever sets this flag,
     * so production resolution is unchanged -- and pinning is the ONLY way a fixture could reach the
     * arm path at all, since the real path needs a live target's symbol table. */
    if (dr_geometry_pinned) return dr_canonical != 0;
    addr = symbol_address("g_xbox_mem_offset");
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

static int dr_arm_thread(HANDLE thread, DWORD tid, const char *why, unsigned *reason);
static int dr_was_armed(DWORD tid);

/* ── C1/C2 event-time ledger (observation only; every writer is behind the gate) ───────────────
 *
 * WHY: a successful arm after the handshake used to print nothing and the tid list was a
 * handshake-time snapshot, so "was this guest thread ever armed?" was not answerable from the
 * artifact -- which is how the absent-record error class in
 * docs/reviews/a2h-arming-coverage-advisor-reruling.md got committed twice. Every function here
 * writes a POSITIVE record at the moment the event happens, and nothing here changes arming
 * behaviour: no queue, no second sweep, no new mechanism. */

static const char *a2h_state_name(unsigned state)
{
    switch (state) {
    case A2H_BIRTH_ARMED:    return "armed";
    case A2H_BIRTH_DEFERRED: return "deferred";
    case A2H_BIRTH_FAILED:   return "failed";
    default:                 return "exited";
    }
}

static const char *a2h_reason_name(unsigned reason)
{
    switch (reason) {
    case A2H_REASON_NO_MAPPING_OFFSET: return "no_mapping_offset";
    case A2H_REASON_COLLISION:         return "collision";
    case A2H_REASON_GET:               return "get";
    case A2H_REASON_SET:               return "set";
    case A2H_REASON_READBACK:          return "readback";
    case A2H_REASON_OPEN:              return "open";
    case A2H_REASON_SNAPSHOT:          return "snapshot";
    default:                           return "none";
    }
}

/* One row per lifecycle event. Returns the seq it used, or 0 when the ledger is full -- the caller
 * must NOT report a state transition it failed to record, which is why the return value is checked
 * at the exit path. */
static unsigned a2h_birth_record(DWORD tid, unsigned why, int mapping_available,
                                 unsigned state, unsigned reason)
{
    LARGE_INTEGER now;
    unsigned seq = ++dr_seq;
    if (dr_birth_count >= A2H_BIRTH_CAPACITY) { dr_birth_overflow = 1; return 0; }
    QueryPerformanceCounter(&now);
    dr_birth[dr_birth_count].seq = seq;
    dr_birth[dr_birth_count].why = why;
    dr_birth[dr_birth_count].mapping_available = mapping_available ? 1u : 0u;
    dr_birth[dr_birth_count].state = state;
    dr_birth[dr_birth_count].reason = reason;
    dr_birth[dr_birth_count].tid = tid;
    dr_birth[dr_birth_count].ticks = (DWORD64)now.QuadPart;
    dr_birth_count++;
    return seq;
}

/* C1(1)/C1(2): the positive per-arm record. This is the record the Advisor ruling makes central --
 * without it no count in the archive can settle coverage. `dr7_readback` is the value actually read
 * back from the thread, not the value written, so a silently-refused arm cannot print as a success. */
static void a2h_arm_ok_record(DWORD tid, const char *why, DWORD64 dr7_readback, DWORD64 dr0_readback)
{
    LARGE_INTEGER now;
    unsigned seq = ++dr_seq;
    QueryPerformanceCounter(&now);
    dr_arm_ok_records++;
    if (dr_handshake_seen) dr_armed_at_or_after_handshake++;
    else dr_armed_before_handshake++;
    fprintf(report, "GUEST_DR_ARM_OK seq=%u tid=%lu why=%s dr0=%016llX dr7_readback=%016llX "
                    "canonical=%016llX ticks=%llu phase=%s\n",
            seq, tid, why, (unsigned long long)dr0_readback, (unsigned long long)dr7_readback,
            (unsigned long long)dr_canonical, (unsigned long long)now.QuadPart,
            /* `why` already says WHICH path armed this thread; `phase` says WHEN, relative to the
             * handshake event. The sweep runs during the handshake, so its arms are
             * phase=at_handshake and not post_handshake -- a naming distinction worth keeping, since
             * "post-handshake" is exactly the population that used to be invisible. */
            strcmp(why, "handshake") == 0 ? "at_handshake" :
            (dr_handshake_seen ? "post_handshake" : "pre_handshake"));
    fflush(report);
}

/* Every CREATE_THREAD debug event, whether or not an arm attempt follows. Emitted BEFORE the attempt
 * so that a crash or a refused arm inside the attempt cannot lose the birth itself. */
static void a2h_birth_event(DWORD tid)
{
    LARGE_INTEGER now;
    unsigned seq = ++dr_seq;
    QueryPerformanceCounter(&now);
    dr_create_thread_events++;
    fprintf(report, "GUEST_DR_BIRTH seq=%u tid=%lu event=create_thread mapping_available=%u "
                    "canonical=%016llX ticks=%llu handshake_seen=%u\n",
            seq, tid, dr_canonical ? 1u : 0u, (unsigned long long)dr_canonical,
            (unsigned long long)now.QuadPart, dr_handshake_seen ? 1u : 0u);
    fflush(report);
}

static int dr_arm_thread(HANDLE thread, DWORD tid, const char *why, unsigned *reason)
{
    CONTEXT context = {0}, back = {0};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (reason) *reason = A2H_REASON_NONE;
    if (!GetThreadContext(thread, &context)) {
        dr_failed++;
        if (reason) *reason = A2H_REASON_GET;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=get error=%lu\n", tid, why, GetLastError());
        return 0;
    }
    if (context.Dr1 || context.Dr2 || context.Dr3 || (context.Dr7 & A2H_DR7_OWNED) ||
        (context.Dr6 & A2H_DR6_MEANINGFUL)) {
        /* Another debug owner already holds breakpoint state on this thread. Do NOT clobber it:
         * an unreconciled DR owner makes every hit ambiguous, which is a coverage failure -- not a
         * reason to overwrite somebody else's watch. */
        dr_collision++;
        if (reason) *reason = A2H_REASON_COLLISION;
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
        if (reason) *reason = A2H_REASON_SET;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=set error=%lu\n", tid, why, GetLastError());
        return 0;
    }
    back.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(thread, &back) || back.Dr0 != dr_canonical ||
        (back.Dr7 & A2H_DR7_OWNED) != A2H_DR7) {
        dr_failed++;
        if (reason) *reason = A2H_REASON_READBACK;
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=readback dr0=%016llX dr7=%016llX error=%lu\n",
                tid, why, (unsigned long long)back.Dr0, (unsigned long long)back.Dr7, GetLastError());
        return 0;
    }
    dr_armed++;
    if (dr_arm_tid_count < A2H_ARM_TID_CAPACITY) {
        int seen = 0;
        for (unsigned i = 0; i < dr_arm_tid_count; i++) if (dr_arm_tids[i] == tid) { seen = 1; break; }
        if (!seen) dr_distinct_armed++;
        dr_arm_tid_why[dr_arm_tid_count] = (unsigned char)(strcmp(why, "handshake") == 0
                                                           ? A2H_WHY_HANDSHAKE : A2H_WHY_CREATE_THREAD);
        dr_arm_tids[dr_arm_tid_count++] = tid;
    } else dr_arm_tid_overflow = 1;
    /* C1(1): THE FIX. A success used to return silently, which is precisely how a post-handshake
     * arm became indistinguishable from no attempt at all. The record carries the readback DR7, so
     * the claim "armed" is backed by the value the thread actually reports. */
    a2h_arm_ok_record(tid, why, back.Dr7, back.Dr0);
    return 1;
}

/* Arm every live thread of the child. Called with the install thread STOPPED at the handshake, so
 * the toolkit's install store cannot execute before this returns. */
static void dr_arm_all(const char *why)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry = {sizeof(entry)};
    unsigned reason = A2H_REASON_NONE;
    if (snapshot == INVALID_HANDLE_VALUE) {
        dr_failed++;
        dr_last_fail_reason = A2H_REASON_SNAPSHOT;
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
            dr_last_fail_reason = A2H_REASON_OPEN;
            a2h_birth_record(entry.th32ThreadID, A2H_WHY_HANDSHAKE, 1, A2H_BIRTH_FAILED,
                             A2H_REASON_OPEN);
            fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=%s stage=open error=%lu\n",
                    entry.th32ThreadID, why, GetLastError());
            continue;
        }
        dr_arm_thread(handle, entry.th32ThreadID, why, &reason);
        /* Every attempt the sweep makes gets an event-time row too, so a tid the sweep touched is
         * never recorded only by the handshake summary -- and a tid the sweep touched but could NOT
         * arm is recorded as a failed attempt rather than left to be inferred from `failed=`. */
        a2h_birth_record(entry.th32ThreadID, A2H_WHY_HANDSHAKE, 1,
                         reason == A2H_REASON_NONE ? A2H_BIRTH_ARMED : A2H_BIRTH_FAILED, reason);
        if (reason != A2H_REASON_NONE) dr_last_fail_reason = reason;
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

/* ── C1(3)/C1(4) TERMINAL reconciliation ──────────────────────────────────────────────────────
 *
 * WHY a second, terminal line is required and why the handshake line above cannot serve as one:
 * `GUEST_DR_ARM why=handshake` is a snapshot taken at the handshake, so it cannot describe arms that
 * happened after it -- and `cleared=` in GUEST_DR_DISARM counts every live thread zeroed at teardown
 * REGARDLESS of whether it was ever armed, so it OVERCOUNTS arms and is deliberately NOT used as an
 * arm count anywhere below. The only arm count reported here is the count of positive per-arm
 * records (GUEST_DR_ARM_OK lines), which is why every success prints one.
 *
 * `recovered` is C1(4): a tid whose create_thread attempt FAILED or was DEFERRED (deferred means no
 * mapping offset was published yet, so no attempt was even possible) and which the handshake sweep
 * then armed successfully. Those are recovered, not unarmed -- conflating them is the accounting
 * error this record exists to prevent. `failed_never_recovered` is the set that is genuinely still
 * unarmed, and it is derived from per-event records, not from `failed=` (which counts ATTEMPTS, and
 * counts a tid twice when it both failed at birth and failed again in the sweep). */
static void dr_print_terminal_summary(void)
{
    unsigned recovered = 0, failed_never_recovered = 0, deferred_events = 0, failed_events = 0;
    unsigned exit_rows = 0, mapping_unavailable_births = 0, exited_unarmed = 0;
    unsigned pre_handshake_births = 0, pre_handshake_exits = 0, orphan_exits = 0;
    unsigned pre_mapping_exits = 0;
    dr_terminal_seq = ++dr_seq;
    for (unsigned i = 0; i < dr_birth_count; i++) {
        DWORD tid = dr_birth[i].tid;
        if (dr_birth[i].state == A2H_BIRTH_EXITED) {
            exit_rows++;
            if (dr_birth[i].seq < dr_handshake_seq) pre_handshake_exits++;
            /* A tid that was born with no mapping offset published and then exited BEFORE the
             * handshake is the one population neither the sweep nor any queue could retroactively
             * cover -- it is not live at the handshake, so the sweep never sees it. Counted from
             * records: mapping_available=0 on its own birth row, state=exited, seq before the
             * handshake. */
            if (!dr_birth[i].mapping_available && dr_birth[i].seq < dr_handshake_seq)
                pre_mapping_exits++;
            if (!dr_was_armed(tid)) orphan_exits++;
            continue;
        }
        if (dr_birth[i].why == A2H_WHY_CREATE_THREAD) {
            if (!dr_birth[i].mapping_available) mapping_unavailable_births++;
            if (dr_birth[i].seq < dr_handshake_seq) pre_handshake_births++;
            if (dr_birth[i].state == A2H_BIRTH_DEFERRED) deferred_events++;
            if (dr_birth[i].state == A2H_BIRTH_FAILED) failed_events++;
            if (dr_birth[i].state != A2H_BIRTH_ARMED) {
                if (dr_was_armed(tid)) recovered++;
                else if (dr_tid_exited(tid)) exited_unarmed++;
                else failed_never_recovered++;
            }
        } else if (dr_birth[i].state == A2H_BIRTH_FAILED && !dr_was_armed(tid)) {
            /* A sweep attempt that lost and was never subsequently recovered. */
            if (dr_tid_exited(tid)) exited_unarmed++;
            else failed_never_recovered++;
        }
    }
    fprintf(report, "GUEST_DR_ARM_TERMINAL arms_recorded=%u armed_before_handshake=%u "
                    "armed_at_or_after_handshake=%u arm_attempt_failures=%u collision=%u "
                    "create_thread_events=%u exit_events=%u deferred_births=%u failed_births=%u "
                    "recovered_by_sweep=%u failed_never_recovered=%u exited_unarmed=%u birth_rows=%u%s "
                    "handshake_seq=%u last_seq=%u handshake_seen=%u terminal=%u last_fail_reason=%s\n",
            dr_arm_ok_records, dr_armed_before_handshake, dr_armed_at_or_after_handshake, dr_failed,
            dr_collision, dr_create_thread_events, dr_exit_events, deferred_events, failed_events,
            recovered, failed_never_recovered, exited_unarmed, dr_birth_count,
            dr_birth_overflow ? " birth_overflow=1" : "",
            dr_handshake_seq, dr_seq, dr_handshake_seen ? 1u : 0u, dr_terminal_seq,
            a2h_reason_name(dr_last_fail_reason));
    /* The reconciliation the packet demands: what the artifact can and cannot prove. Every clause is
     * derived from records, never from a count that could be a snapshot or a superset. */
    fprintf(report, "GUEST_DR_ARM_RECONCILE arm_tid_list=%u arm_tid_overflow=%u "
                    "disarm_cleared=%u disarm_cleared_is_not_an_arm_count=1 "
                    "distinct_armed_tids=%u pre_handshake_births=%u pre_handshake_exits=%u "
                    "mapping_unavailable_births=%u orphan_exits=%u incomplete=%u\n",
            dr_arm_tid_count, dr_arm_tid_overflow, dr_disarmed, dr_distinct_armed,
            pre_handshake_births, pre_handshake_exits, mapping_unavailable_births, orphan_exits,
            (dr_birth_overflow || dr_arm_tid_overflow || dr_prev_overflow || dr_hit_overflow)
                ? 1u : 0u);
    fprintf(report, "GUEST_DR_ARM_RECONCILE_EXIT_RULE exits_recorded=%u exit_records_missing=%u "
                    "note=EXIT_THREAD_DEBUG_EVENT_is_not_delivered_to_a_DEBUG_ONLY_THIS_PROCESS_debugger\n",
            exit_rows, dr_exit_events);
    /* C2 PRE-MAPPING-EXIT BOUND, as a DERIVED decision rather than two counts a reader must combine
     * (and might combine wrongly). The packet's question is: could a tid born before mapping
     * availability have exited before the handshake having dispatched guest code -- uncovered by the
     * sweep, and uncoverable by any queue? Three outcomes, and the default is UNKNOWN:
     *
     *   EMPTY_NO_PRE_MAPPING_BIRTH  no birth row was ever recorded with mapping_available=0, and the
     *                               ledger did not overflow, and the handshake happened. Nothing could
     *                               have been in the window.
     *   NO_PRE_MAPPING_EXIT         pre-mapping births exist but every one of them is still live at
     *                               the handshake (no exited row before it), so the sweep covered them.
     *   PRE_MAPPING_EXIT_UNCOVERED  at least one tid was born without mapping availability and exited
     *                               before the handshake. Its guest dispatch status is NOT decidable
     *                               from these records alone, so this yields UNKNOWN and must be
     *                               joined against the ICALL/bridge-dispatch census before any
     *                               coverage claim -- never read as absence.
     *   UNKNOWN                     the ledger overflowed, no handshake was seen, or the exit path
     *                               could not be recorded (exit records are not delivered by this
     *                               debugger, see RECONCILE_EXIT_RULE), so the window cannot be closed
     *                               from records.
     */
    {
        const char *window;
        if (dr_birth_overflow || !dr_handshake_seen || dr_exit_events != exit_rows)
            window = "UNKNOWN";
        else if (pre_mapping_exits) window = "PRE_MAPPING_EXIT_UNCOVERED";
        else if (mapping_unavailable_births) window = "NO_PRE_MAPPING_EXIT";
        else window = "EMPTY_NO_PRE_MAPPING_BIRTH";
        fprintf(report, "GUEST_DR_ARM_PRE_MAPPING_BOUND births_before_mapping=%u "
                        "births_before_handshake=%u exits_before_handshake=%u pre_mapping_exits=%u "
                        "handshake_seq=%u decision=decidable_from_records window=%s\n",
                mapping_unavailable_births, pre_handshake_births, pre_handshake_exits,
                pre_mapping_exits, dr_handshake_seq, window);
    }
    /* Every observed arm, with its source, so the terminal record is self-contained and a reader
     * never has to reconstruct the set from the handshake snapshot plus guesswork. */
    for (unsigned i = 0; i < dr_birth_count; i++)
        fprintf(report, "GUEST_DR_BIRTH_ROW index=%u seq=%u tid=%lu why=%s state=%s reason=%s "
                        "mapping_available=%u ticks=%llu\n",
                i, dr_birth[i].seq, dr_birth[i].tid,
                dr_birth[i].why == A2H_WHY_HANDSHAKE ? "handshake" :
                (dr_birth[i].why == A2H_WHY_CREATE_THREAD ? "create_thread" : "exit"),
                a2h_state_name(dr_birth[i].state), a2h_reason_name(dr_birth[i].reason),
                dr_birth[i].mapping_available, (unsigned long long)dr_birth[i].ticks);
    for (unsigned i = 0; i < dr_arm_tid_count; i++)
        fprintf(report, "GUEST_DR_ARM_TID_TERMINAL index=%u tid=%lu source=%s\n", i,
                dr_arm_tids[i], dr_arm_tid_why[i] == A2H_WHY_HANDSHAKE ? "handshake" : "create_thread");
    fflush(report);
}

static void dr_handshake(DWORD tid)
{
    if (!dr_canonical) {
        /* Symbol resolution needs an initialized symbol handler; capture() has not run yet at the
         * handshake, so initialize it here. Same options capture() uses, so symbol lookups behave
         * identically whichever path initializes first. The process guard is not a behaviour change
         * for the collector (process is always live by the time a debug event arrives); it keeps the
         * fixture, which has no debuggee, from asking the symbol handler about a null handle. */
        if (process) {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS |
                          SYMOPT_FAIL_CRITICAL_ERRORS);
            SymInitialize(process, out_dir, TRUE);
        }
        resolve_geometry();
    }
    if (!dr_canonical) {
        dr_failed++;
        dr_last_fail_reason = A2H_REASON_NO_MAPPING_OFFSET;
        /* C2: the handshake still happened, and its seq is what every "before the handshake" bound is
         * measured against -- so it is stamped even on this failure path. A handshake with no mapping
         * offset leaves the whole run uncovered, which the terminal line reports as UNKNOWN rather
         * than as an empty pre-mapping window. */
        dr_handshake_seen = 1;
        dr_handshake_seq = ++dr_seq;
        fprintf(report, "GUEST_DR_ARM why=handshake ok=0 armed=0 failed=%u reason=no_mapping_offset "
                        "handshake_tid=%lu handshake_seq=%u\n", dr_failed, tid, dr_handshake_seq);
        fflush(report);
        return;
    }
    /* Stamped BEFORE the sweep, so every arm the sweep itself performs is correctly counted as
     * post-handshake, and so births can be ordered against the handshake. */
    dr_handshake_seen = 1;
    dr_handshake_seq = ++dr_seq;
    dr_arm_all("handshake");
    dr_print_arm_summary("handshake");
}

static int dr_was_armed(DWORD tid)
{
    for (unsigned i = 0; i < dr_arm_tid_count; i++)
        if (dr_arm_tids[i] == tid) return 1;
    return 0;
}

/* A tid with an exit row. Such a tid is reported in the EXIT population, never as a terminal unarmed
 * tid: "exited before it could be armed" and "still live and never armed" are different findings, and
 * collapsing them would be the same conflation C1(4) forbids for failed attempts. */
static int dr_tid_exited(DWORD tid)
{
    for (unsigned i = 0; i < dr_birth_count; i++)
        if (dr_birth[i].tid == tid && dr_birth[i].state == A2H_BIRTH_EXITED) return 1;
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
    if (dr6 & (A2H_DR6_MEANINGFUL & ~(DWORD64)0x1)) {
        /* MIXED STATUS: someone else's meaningful bit is set in the same DR6. Clearing B0 would be
         * ours to do, but the event cannot be attributed to this watch alone, so it is not claimed
         * and the report says which bits were shared. */
        fprintf(report, "GUEST_DR_HIT_MIXED tid=%lu dr6=%016llX shared=%04llX\n", tid,
                (unsigned long long)dr6,
                (unsigned long long)(dr6 & (A2H_DR6_MEANINGFUL & ~(DWORD64)0x1)));
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
    fprintf(report, "GUEST_DR_DISARM ok=%d cleared=%u failed=%u dr7_nonzero=%u hits=%u hit_overflow=%u "
                    "cleared_means=live_threads_zeroed_at_teardown_not_arms\n",
            dr_disarm_failed == 0 ? 1 : 0, dr_disarmed, dr_disarm_failed, dr_disarm_failed,
            dr_hits, dr_hit_overflow);
    fflush(report);
    /* C1(3): the TERMINAL full-list summary. Printed here, after teardown, so it describes the whole
     * process lifetime rather than the handshake instant -- and before fclose, so it cannot be lost. */
    dr_print_terminal_summary();
}

/* A2h install handshake. The toolkit raises this FIRST-CHANCE at its thunk-install callback and
 * does not perform the install store until this thread is continued, so arming here provably
 * precedes the install write. Observation only: no guest register, memory or device state is
 * touched, and the toolkit's own VEH still receives the exception afterwards
 * (DBG_EXCEPTION_NOT_HANDLED).
 *
 * THE #DB CONTRACT IS DECIDED BY DR6, NOT BY THE CHANCE. A single-step exception carries three
 * different meanings here and they are told apart by the status register, which is why the handler
 * reads it before claiming anything:
 *
 *   pure B0        -> this instrument's data breakpoint. Claimed, and ONLY the owned bit is cleared.
 *   B0 | BS or B1-3-> shared with another debug owner. Passed on untouched, reported as
 *                     coverage-uncertain; clearing a bit this collector does not own would corrupt
 *                     the other owner's watch.
 *   BS (TF) alone  -> somebody's single-step, AC'97's page trap among them. Passed on, because
 *                     swallowing it would stop AC'97 from ever re-protecting its page.
 *
 * Claiming on the first-chance event rather than waiting for second chance is deliberate: the
 * game's own VEH (src/main.c:212) logs every non-breakpoint exception it sees, so letting our
 * breakpoint fall through to it would both add ON-run log noise and hand a handler we do not own a
 * say in the outcome. */
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

/* The gated CREATE_THREAD branch, extracted verbatim from main()'s debug loop so that the fixture
 * can drive the SAME code the production collector runs rather than a test-local copy of it.
 * Behaviour is unchanged: the gate check is still the first thing this does. */
static void dr_on_create_thread(HANDLE thread, DWORD tid)
{
    unsigned reason = A2H_REASON_NONE;
    if (!dr_on()) return;
    /* C2: the birth is recorded FIRST, before any attempt, so the record exists even if the attempt
     * below cannot run or fails. */
    a2h_birth_event(tid);
    if (resolve_geometry()) {
        if (dr_arm_thread(thread, tid, "create_thread", &reason)) {
            /* C1(2): the success is now recorded as an event, not left to a summary printed at a
             * different time. This is the arm that used to be silent. */
            a2h_birth_record(tid, A2H_WHY_CREATE_THREAD, 1, A2H_BIRTH_ARMED, A2H_REASON_NONE);
        } else {
            a2h_birth_record(tid, A2H_WHY_CREATE_THREAD, 1, A2H_BIRTH_FAILED, reason);
            dr_last_fail_reason = reason;
            /* Kept for continuity with the existing artifact shape: a failed create_thread arm still
             * prints its summary. */
            dr_print_arm_summary("create_thread");
        }
    } else {
        dr_failed++;
        dr_last_fail_reason = A2H_REASON_NO_MAPPING_OFFSET;
        /* DEFERRED, not failed: no attempt was possible yet, and the handshake sweep is expected to
         * recover this tid. Recorded distinctly so a later recovery is reported as recovered rather
         * than as a contradiction. */
        a2h_birth_record(tid, A2H_WHY_CREATE_THREAD, 0, A2H_BIRTH_DEFERRED,
                         A2H_REASON_NO_MAPPING_OFFSET);
        fprintf(report, "GUEST_DR_ARM_FAIL tid=%lu why=create_thread reason=no_mapping_offset\n", tid);
    }
}

/* C2 lifecycle record, extracted from main()'s debug loop for the same reason as above. MEASURED
 * CAVEAT, recorded because the packet demands that an unprovable bound yield UNKNOWN rather than an
 * implied guarantee: with a DEBUG_ONLY_THIS_PROCESS debugger Windows does NOT deliver
 * EXIT_THREAD_DEBUG_EVENT for a thread that exits (only EXIT_PROCESS ends the loop), so this is
 * expected to be unreachable in practice. It is kept because it costs nothing and would capture a
 * delivery on a host that does send it; the terminal line reports exits_recorded and states the
 * caveat, so a zero exit count is never read as "no thread exited". */
static void dr_on_exit_thread(DWORD tid)
{
    if (!dr_on()) return;
    dr_exit_events++;
    if (!a2h_birth_record(tid, A2H_WHY_EXIT, dr_canonical ? 1 : 0, A2H_BIRTH_EXITED,
                          A2H_REASON_NONE)) {
        /* The ledger is full, so this exit could not be recorded. Losing a lifecycle row silently
         * would be exactly the absent-record error class; say so loudly. */
        fprintf(report, "GUEST_DR_BIRTH_DROPPED tid=%lu event=exit reason=ledger_full\n", tid);
        fflush(report);
    }
}

/* ── Fixture seams (tests/test_collect_arming.c) ────────────────────────────────────────────────
 *
 * These exist so the C1/C2 fixture drives the PRODUCTION branch bodies above rather than a
 * test-local copy of them -- the apu_watch_fixture_test precedent. Nothing in the collector calls
 * them, they add no behaviour, and the gate still decides everything: with JSRF_TRACE_A2H_DR unset
 * every one of them is inert (jsrf_a2h_test_create_thread/exit_thread return immediately, the
 * handshake refuses, and the terminal summary is not reached because dr_disarm_all's caller is
 * gated). */
void jsrf_a2h_test_begin(const char *report_path)
{
    FILE *file = fopen(report_path, "w");
    if (file) report = file;
    process_id = GetCurrentProcessId();
    dr_gate_read = 0;              /* re-read the gate, so the fixture's own environment decides */
    dr_enabled = 0;
}

void jsrf_a2h_test_geometry(DWORD64 canonical)
{
    if (!dr_on()) return;
    dr_geometry_pinned = 1;
    dr_canonical = canonical;
}

void jsrf_a2h_test_create_thread(HANDLE thread, DWORD tid)
{
    dr_on_create_thread(thread, tid);
}

void jsrf_a2h_test_exit_thread(DWORD tid)
{
    dr_on_exit_thread(tid);
}

int jsrf_a2h_test_handshake(DWORD tid)
{
    if (!dr_on()) return 0;
    dr_handshake(tid);
    return dr_handshake_seen;
}

void jsrf_a2h_test_terminal(void)
{
    if (!dr_on()) return;
    dr_print_terminal_summary();
}

DWORD jsrf_a2h_test_armed_count(void) { return dr_armed; }
DWORD jsrf_a2h_test_failed_count(void) { return dr_failed; }

/* The fixture links this file to drive the production branch bodies above, so its own main() is
 * compiled out there. Nothing in the production build defines JSRF_COLLECT_NO_MAIN, so the collector
 * entry point is unchanged. */
#ifndef JSRF_COLLECT_NO_MAIN
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
            dr_on_create_thread(event.u.CreateThread.hThread, event.dwThreadId);
            if (event.u.CreateThread.hThread) CloseHandle(event.u.CreateThread.hThread);
        } else if (event.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT) {
            dr_on_exit_thread(event.dwThreadId);
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
            } else if (code == EXCEPTION_SINGLE_STEP && dr_on()) {
                /* The handler decides ownership from DR6, not from the chance: pure B0 is ours,
                 * BS alone or a mixed status is somebody else's and is passed on. */
                continuation = dr_handle_single_step(event.dwThreadId);
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
#endif /* JSRF_COLLECT_NO_MAIN */
