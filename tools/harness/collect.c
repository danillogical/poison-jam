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

/* ── A2h LIVE slot-write ledger: archived by symbol, and NOT part of the DR instrument ──────────
 *
 * The ledger lives in the TOOLKIT (src/kernel/xbox_memory_layout.c), which owns the page-protection
 * mechanism. The collector is a separate process, so it reads the object BY SYMBOL out of the
 * target's PDB -- exactly the treatment g_jsrf_debug gets -- and copies it into the minidump, so the
 * frozen bytes travel with the archive and the printed summary is a cross-check on them.
 *
 * NOTHING HERE IS GATED ON JSRF_TRACE_A2H_DR. The DR channel is EXCLUDED from this packet, and its
 * counters in this file are not this instrument's evidence channel: reusing them would make the
 * live result depend on a channel the preflight removed. This block is unconditional, because
 * whether the toolkit ever armed is a fact the ARCHIVE must carry either way -- and the "did it
 * arm" question is answered by the object's own `armed` field, not by the presence of a log line.
 *
 * The layout below MUST match XboxA2hSlotwLedger in xbox_memory_layout.h. The collector does not
 * include that header -- it links no toolkit code -- so the struct is mirrored here and the MAGIC
 * and SIZE fields are checked before any field is read: a struct that changed without the version
 * moving is reported as unavailable rather than silently misread. */
#define A2H_SLOTW_MAGIC          0x57533241u   /* 'A2SW' */
#define A2H_SLOTW_VERSION        3u             /* must equal XBOX_A2H_SLOTW_VERSION in the toolkit */
#define A2H_SLOTW_PAGES_MAX      (1 + XBOX_NUM_MIRRORS_COLLECTOR)
#define XBOX_NUM_MIRRORS_COLLECTOR 28
/* ⚠ 1024, NOT 256, AND THE CAPACITY IS NOT THE REPAIR. The repair is that non-slot page writes are
 * TRAFFIC and are COUNTED rather than recorded, so ordinary traffic consumes no records at all. The
 * array is enlarged because "zero headroom" was itself part of the defect: at 1024 the buffer holds
 * 512 slot writes plus their step records, far above the 128 steps a partial boot produced. */
#define A2H_SLOTW_EVENTS_MAX     1024
#define A2H_SLOTW_FIRST_TOUCH_MAX 512
#define A2H_SLOTW_EV_KIND_WRITE  1u

typedef struct {
    uint32_t seq, kind, alias_index, slot_hit, fault_va, pre_value, post_value, tid, enc, reserved;
    uint64_t rip, ticks;
} XboxA2hSlotwEvent;

typedef struct {
    uint32_t valid, offset, alias_index, tid, pre_value, slot_value_at_touch, enc, reserved;
    uint64_t rip, ticks;
} XboxA2hSlotwFirstTouch;

typedef struct {
    uint64_t relevant_av, slot_hits, nonslot_writes, steps, rearm_ok, rearm_failed;
    uint64_t protected_intervals, unprotected_intervals, concurrent_overlap;
    uint64_t threads_new, threads_gone, publish_failed, protect_failed, dropped_events;
    uint64_t unexpected_exception, db_unowned, db_own_serviced, db_ac97_serviced, db_dual_serviced;
    uint64_t read_samples, installer_control_hits, nonslot_distinct, first_touch_overflow;
    uint64_t first_touch_dropped, cross_checks, cross_mismatch, cross_skipped;
    uint64_t overflow, base_changed;
} XboxA2hSlotwLoss;

typedef struct {
    uint32_t magic, version, size, armed, arm_base, term_base, arm_slot, term_slot;
    uint32_t slot_stable, page_offset, mapped_mask, protect_mask, alias_count, protected_count;
    uint32_t event_count, event_overflow, thread_count, thread_overflow, terminal_seen;
    uint32_t terminal_target, arm_reason, term_base_ok, reserved;
    uint64_t arm_ticks, terminal_ticks;
    uint32_t read_count, fourth_reached, fourth_value, fourth_seq;
    uint32_t last_write_seq, last_write_enc, last_write_alias, last_write_value;
    uint64_t last_write_rip, last_write_ticks;
    uint32_t cross_checks, cross_mismatch, first_touch_count, first_touch_overflow;
    uint32_t last_slot_read_value, last_slot_read_seq, last_slot_read_hits, last_slot_read_alias;
    uint32_t last_slot_change_seen;
    uint32_t terminal_alias_value, terminal_guest_value, terminal_cross_ok;
    XboxA2hSlotwLoss loss;
    XboxA2hSlotwEvent events[A2H_SLOTW_EVENTS_MAX];
    XboxA2hSlotwFirstTouch first_touch[A2H_SLOTW_FIRST_TOUCH_MAX];
} XboxA2hSlotwLedger;

/* THE CROSS-PROCESS LAYOUT PIN, AND AN HONEST NOTE ABOUT WHAT IT DOES AND DOES NOT CATCH.
 *
 * ⚠ MEASURED: this assert pins the MIRROR, so it catches a change to the mirror and NOT a change to
 * the real struct in xbox_memory_layout.h. When `term_base_ok` was added there and not here, this
 * assert still passed -- the mirror was self-consistent, just wrong about the object it describes.
 * The assert is therefore necessary but NOT sufficient, and it is deliberately kept for what it
 * does catch (accidental edits to the mirror).
 *
 * THE SUFFICIENT CHECK IS THE RUNTIME ONE, and it is why `size` exists in the ledger: the toolkit
 * stamps sizeof(its own struct) into `size`, and the collector compares that against
 * sizeof(this mirror) before reading any field. A drift between the two is caught at RUN time by
 * construction, whatever the assert does -- and the toolkit's own fixture prints the authoritative
 * number (14680) so the comparison is checkable from the archive too. Both numbers move together
 * with XBOX_A2H_SLOTW_VERSION. */
_Static_assert(sizeof(XboxA2hSlotwLedger) == 82360u,
               "XboxA2hSlotwLedger mirror does not match xbox_memory_layout.h -- "
               "update this pin and XBOX_A2H_SLOTW_VERSION together");
_Static_assert(sizeof(XboxA2hSlotwEvent) == 56u, "XboxA2hSlotwEvent mirror drifted");
_Static_assert(sizeof(XboxA2hSlotwFirstTouch) == 48u, "XboxA2hSlotwFirstTouch mirror drifted");
_Static_assert(sizeof(XboxA2hSlotwLoss) == 232u, "XboxA2hSlotwLoss mirror drifted");
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

/* ── C1: event-time LOSSLESS DELIVERY ledger (A2h-dr0-delivery-gate) ────────────────────────────
 *
 * WHY THIS EXISTS. The Phase-1 finding is that the install store -- a known canonical write under an
 * armed four-byte write watch, on the armed thread, after the arm -- produced no #DB. Before that can
 * be read as NON-FIRING, the delivery side has to be provably LOSSLESS, because "no hit" and "no
 * record of a hit" are different worlds:
 *
 *     install_executed=1 AND complete raw single-step count of 0   ->  NON-FIRING
 *     missing latch, or any incomplete/lossy accounting            ->  NOT-RECORDED / UNKNOWN
 *
 * So the raw debug event is counted at the TOP of the exception branch, BEFORE any filtering, gating
 * or routing -- a count taken after the gate would be exactly the dropped-record hazard the packet
 * names. Every counter below is UNCAPPED; only the record array is bounded, and its overflow is an
 * explicit published flag, never a silent truncation.
 *
 * BOUNDED, FIXED-SIZE ONLY. Same pattern as dr_arm_tids[A2H_ARM_TID_CAPACITY]: a fixed array plus an
 * overflow flag. No run-length-sized history is kept anywhere in this file. */
#define A2H_RAW_SS_CAPACITY 32
/* Chance classification. The distinction is the packet's C3 discriminator: a raw 0x80000004 marked
 * SECOND chance while first chance is absent favors chance semantics, whereas a raw event that is
 * present but filtered, DBG_CONTINUE-consumed, or absent downstream favors debugger swallow. */
enum { A2H_CHANCE_FIRST = 0, A2H_CHANCE_SECOND = 1 };
/* Where the event went after it was counted. Recorded per event so a reader can tell a delivered
 * event that was ROUTED from one that was merely COUNTED. */
enum { A2H_ROUTE_NONE = 0, A2H_ROUTE_HANDLER = 1, A2H_ROUTE_GENERIC_FIRST = 2,
       A2H_ROUTE_TERMINAL_SECOND = 3 };

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
/* C2 verdict= on a terminal snapshot row. Every one of these is a DIFFERENT world and the decision
 * treats them differently: VERIFIED_ARMED is arm-persistence evidence, CHANGED_SET and UNREADABLE
 * and UNARMED are all UNKNOWN, and NONE of them is ever a negative. */
enum { A2H_TERM_UNREADABLE = 0, A2H_TERM_VERIFIED_ARMED = 1, A2H_TERM_CHANGED_SET = 2,
       A2H_TERM_UNARMED = 3 };

static int read_remote(DWORD64 address, void *buffer, SIZE_T size);
static DWORD64 symbol_address(const char *name);

static int dr_gate_read, dr_enabled;
/* NATIVE #DB INGRESS FIXTURE state. Declared here, with the rest of the instrument's state, because
 * dr_handshake() (defined far below) consumes it. In production all three are zero/NULL and every
 * clause that reads them is inert. */
static volatile uint32_t a2h_native_db_target;
static int a2h_native_db_fixture;
static EXCEPTION_DEBUG_INFO *a2h_native_db_params;
static int a2h_native_db_child(void);
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

/* C1 delivery ledger. Every counter here is UNCAPPED and every one of them is published, so a reader
 * can distinguish a real zero from an unrecorded one. `dr_raw_event_total` counts EVERY debug event
 * of any kind, which is the completeness denominator: if the raw single-step count is 0 while the
 * total is also 0, nothing was recorded at all and the answer is UNKNOWN, not NON-FIRING. */
static unsigned dr_raw_event_total, dr_raw_exception_total, dr_raw_single_step;
static unsigned dr_ss_first_chance, dr_ss_second_chance;
static unsigned dr_ss_routed_handler, dr_ss_routed_generic, dr_ss_routed_terminal;
static unsigned dr_ss_unrouted;              /* single-step that reached no branch at all */
/* THE ANTI-POLLUTION COUNTER, and it is load-bearing. The live run delivers ~14k debug events per
 * run, the overwhelming majority of them NOT single-steps (the initial breakpoint, the GPU and
 * handshake exceptions, AC'97's page traps). If those were folded into dr_raw_single_step, then a
 * run with ZERO delivered single-steps would read as a non-zero count and the Phase-1 answer would
 * come out DELIVERED_UNCLAIMED instead of NON_FIRING -- destroying the exact decision this packet
 * exists to make. So the single-step count is single-step ONLY, and every other raw exception is
 * counted separately, in a counter that still proves the debug stream was read. */
static unsigned dr_raw_exception_other_code;
/* The event-time records. Bounded array + explicit overflow flag, exactly like dr_arm_tids. */
static struct { unsigned seq, chance, route, tid, code; DWORD64 ticks; DWORD64 address; }
    dr_raw_ss[A2H_RAW_SS_CAPACITY];
static unsigned dr_raw_ss_count, dr_raw_ss_overflow;
/* The write-once INSTALL-HIT latch. Set when a claimed single-step arrives and the DR0 value read
 * back at the event names the canonical watch, i.e. the delivery the Phase-1 gate requires. */
static volatile LONG dr_install_hit_latch;
static unsigned dr_install_hit_seq;
static DWORD dr_install_hit_tid;
static DWORD64 dr_install_hit_rip, dr_install_hit_dr6, dr_install_hit_dr7;
/* The value the DR0 watch actually holds, as read back from the thread at the arm and re-read at
 * every single-step. Published next to the toolkit's `host=` so a reader can decide whether the watch
 * ever pointed at the address the install store wrote. */
static DWORD64 dr_watch_armed_value;

/* ── C1(3): CONTINUATION ACCOUNTING (the per-event readback is GONE) ────────────────────────────
 *
 * THE REMOVED DEFECT, stated so it is not reintroduced. The old C3 design kept a capacity-16 array of
 * post-continue readbacks and fired it after EVERY ContinueDebugEvent. Two independent faults:
 *
 *   (1) CAPACITY vs EVENT COUNT. 14 414 continuations against a 16-entry array set the overflow flag
 *       on every real run, and that flag was a `complete` input -- so `complete` was STRUCTURALLY 0
 *       and NON_FIRING was UNREACHABLE BY CONSTRUCTION. The gate could not return the answer it
 *       exists to produce.
 *   (2) THE READ MEASURED A RUNNING THREAD. ContinueDebugEvent resumes the thread, and Windows
 *       documents GetThreadContext as valid only while the thread is SUSPENDED. 13 212 readbacks on
 *       the install thread produced 2 non-zero and 1 216 intermittent successes -- the signature of a
 *       race, not of a watch. The Advisor voided that array IN BOTH DIRECTIONS, including its
 *       no-revert readings, because a race does not produce trustworthy positives either.
 *
 * So the array, its capacity, its overflow predicate and EVERY after-continue context read are
 * deleted. What replaces the readback is a fixed-size counter set, incremented at the continuation
 * site itself: UNCAPPED, so no run length can truncate it, and reconcilable, so a lost continuation
 * is visible as a broken identity rather than as a plausible number. */
static unsigned dr_continue_total, dr_continue_continue, dr_continue_not_handled;
static unsigned dr_continue_failed, dr_continue_ok;

/* ── C2: TERMINAL DR SNAPSHOT ───────────────────────────────────────────────────────────────────
 *
 * The terminal snapshot is the ONLY remaining DR read in this file. It runs once, at/after the fatal
 * exception and BEFORE disarm/teardown, and it reads each target thread's DR0/DR7/DR6 with that
 * thread SUSPENDED. Every counter is UNCAPPED and published, so the snapshot is reconciled stage by
 * stage instead of being trusted as one line.
 *
 * THE VOID RULE IS STRUCTURAL. a2h_term_read() will not call GetThreadContext unless the caller
 * passes suspended=1, and the only value ever passed is the result of a SuspendThread that returned
 * success. An unsuspended read is therefore not merely discouraged -- it cannot happen, and any
 * attempt is COUNTED as VOID and forces UNKNOWN_NOT_RECORDED rather than silently returning a
 * number that looks like evidence.
 *
 * The row array is a BOUNDED PRINTED SAMPLE. It corroborates the counters; it decides NOTHING. No
 * decision input below is a sample, an index, or a capacity -- that conflation is defect (1). */
#define A2H_TERM_CAPACITY 64
static struct { DWORD tid; DWORD64 dr0, dr7, dr6; unsigned armed, suspended, ok, verdict; }
    dr_term[A2H_TERM_CAPACITY];
static unsigned dr_term_count, dr_term_overflow;
/* Stage ledger: every stage is counted whether it succeeded or failed, so a partial snapshot is
 * visible as a partial snapshot. */
static unsigned dr_term_runs, dr_term_snapshot_ok, dr_term_snapshot_failed;
static unsigned dr_term_seen, dr_term_open_ok, dr_term_open_failed;
static unsigned dr_term_suspend_ok, dr_term_suspend_failed, dr_term_already_suspended;
static unsigned dr_term_context_ok, dr_term_context_failed;
static unsigned dr_term_resume_ok, dr_term_resume_failed;
/* THE VOID COUNTER. Non-zero means an unsuspended read was attempted, which voids the snapshot. */
static unsigned dr_term_void_unsuspended;
/* Terminal outcomes. `verified` is the gate's arm-persistence evidence; `changed` is a thread that
 * was armed and is no longer (a CHANGED SET, which the packet makes UNKNOWN, never a negative);
 * `unreadable` is a thread whose context could not be read at all. */
static unsigned dr_term_verified, dr_term_changed, dr_term_unreadable, dr_term_unarmed_rows;
static unsigned dr_term_disarmed_tids, dr_term_exited_tids;
/* Set by dr_disarm_all(). If the snapshot ever runs after this, the reading describes a disarmed
 * process, not an armed one -- a PREMATURE DISARM, which is UNKNOWN and never evidence. */
static int dr_disarmed_flag;
static unsigned dr_term_premature_disarm;
/* The snapshot runs AT MOST ONCE. It is taken at the terminal point inside the debug loop, while the
 * target is still alive, and the post-loop call is only a fallback for exit paths that never set the
 * terminal flag -- so a second run would double-count a snapshot that describes the same process. */
static int dr_term_done;
/* The store-side premise, published next to the decision: the toolkit's install witness is read out
 * of the archived registry by capture_guest_threads() and recorded here so the delivery decision
 * consumes STORE evidence rather than assuming it. */
static int dr_install_exec_seen;
static unsigned dr_install_ok_value, dr_install_seen_value;

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
/* C1/C3: defined after the terminal summary that publishes it, so declared here. */
static void a2h_delivery_terminal(void);

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

/* ── THE ALL-THREAD CENSUS FOR THE SLOT-WRITE WATCH, ON ITS OWN COUNTERS ───────────────────────
 *
 * ⚠ THESE ARE **NEW AND INDEPENDENT** COUNTERS, NOT THE DR INSTRUMENT'S. The DR channel is EXCLUDED
 * from this packet, and its counters are gated by dr_on(); reusing dr_create_thread_events or
 * dr_exit_events as this instrument's census would make the live result depend on a channel the
 * preflight removed -- and would read zero whenever the DR gate is off, which is every run this
 * packet is allowed to make.
 *
 * They count DEBUG EVENTS, which is the only vantage point that sees a thread that both arrived and
 * exited: CREATE_THREAD_DEBUG_EVENT is delivered before the new thread runs its first instruction.
 * The toolkit's own census thread polls and cannot see such a thread, so the two counts are
 * DIFFERENT QUANTITIES and are reported side by side rather than conflated. */
static unsigned slotw_debugger_births, slotw_debugger_exits;

/* Every CREATE_THREAD debug event, counted for the slot-write census. Unconditional: it is not this
 * instrument's business to decide that a birth did not matter. */
static void a2h_slotw_birth_event(DWORD tid)
{
    (void)tid;
    slotw_debugger_births++;
}

/* Every EXIT_THREAD debug event. ⚠ MEASURED: on this host a DEBUG_ONLY_THIS_PROCESS debugger does
 * NOT receive these, so this counter is expected to stay 0 and the exit side is therefore reported
 * as UNKNOWN rather than as a closed census. It is counted anyway, because "we saw no exits" and
 * "exits are not delivered" are different findings and the second must not be inferred from the
 * first. */
static void a2h_slotw_exit_event(DWORD tid)
{
    (void)tid;
    slotw_debugger_exits++;
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
    /* C1: the value the watch actually holds, as read back -- not the value written. This is what the
     * install store's own witness is compared against to decide whether the watch ever pointed at the
     * address the store targeted. */
    dr_watch_armed_value = back.Dr0;
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
    /* ── THE ALL-THREAD CENSUS, RECONCILED AT THE TERMINAL POINT ────────────────────────────────
     *
     * The packet requires the census to account for arrivals AND exits, and the toolkit's own
     * census thread cannot see a thread that both arrived and exited between two of its polls. The
     * DEBUGGER can, because CREATE_THREAD_DEBUG_EVENT is delivered before the new thread runs its
     * first instruction and the debug loop sees every one of them. So the two counts are printed
     * side by side and RECONCILED here, and the reconciliation is a derived verdict rather than two
     * numbers a reader must combine -- and might combine wrongly.
     *
     * ⚠ MEASURED LIMIT, STATED RATHER THAN HIDDEN: EXIT_THREAD_DEBUG_EVENT is NOT delivered to a
     * DEBUG_ONLY_THIS_PROCESS debugger on this host (the RECONCILE_EXIT_RULE line above records the
     * same fact for the DR instrument). So the DEBUGGER cannot count exits and does not pretend to:
     * `debugger_exits` is that count, and when it is 0 while births are non-zero the exit side is
     * NOT closed. That is exactly the "unobserved thread is a coverage hole, never a silent
     * negative" rule, and it is reported as UNKNOWN rather than as a clean census.
     *
     * NOTHING HERE IS DR-GATED. The DR instrument's own birth ledger (dr_birth) is a different
     * channel and is not cited: this reads the toolkit's ledger, which is the packet's instrument. */
    {
        DWORD64 slotw_addr = symbol_address("g_xbox_a2h_slotw");
        XboxA2hSlotwLedger *sw = malloc(sizeof(*sw));
        uint32_t tk_new = 0, tk_gone = 0, tk_live = 0;
        if (sw && slotw_addr && read_remote(slotw_addr, sw, sizeof(*sw))
                && sw->magic == A2H_SLOTW_MAGIC && sw->size == sizeof(*sw)) {
            tk_new = (uint32_t)sw->loss.threads_new;
            tk_gone = (uint32_t)sw->loss.threads_gone;
            tk_live = sw->thread_count;
        }
        fprintf(report, "GUEST_SLOTW_CENSUS debugger_births=%u debugger_exits=%u "
                        "toolkit_new=%u toolkit_gone=%u toolkit_live=%u toolkit_overflow=%u "
                        "census_closed=%d exit_side_closed=%d "
                        "note=EXIT_THREAD_DEBUG_EVENT_not_delivered_so_the_exit_side_is_UNKNOWN\n",
                slotw_debugger_births, slotw_debugger_exits, tk_new, tk_gone, tk_live,
                sw ? sw->thread_overflow : 0u,
                (slotw_debugger_births > 0 && slotw_debugger_exits > 0) ? 1 : 0,
                slotw_debugger_exits > 0 ? 1 : 0);
        free(sw);
    }
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
    a2h_delivery_terminal();
    fflush(report);
}

/* ── C1/C2: the DELIVERY decision, published so a reader never has to combine counters ──────────
 *
 * WHAT CHANGED, AND WHY. The old `complete` was
 *     (raw_events > 0) && !ss_overflow && !post_continue_overflow
 * and `post_continue_overflow` was set by a capacity-16 array fired after every one of 14 414
 * continuations. `complete` was therefore STRUCTURALLY 0 on any real run and NON_FIRING was
 * UNREACHABLE BY CONSTRUCTION: the gate could not return the answer it exists to produce. Worse, it
 * made a PER-EVENT HISTORY a DECISION INPUT -- the conflation the packet forbids.
 *
 * The new `complete` is derived from the things that actually decide the question:
 *
 *   RAW INGRESS          dr_raw_event_total > 0. The debug stream was demonstrably read, so a zero
 *                        single-step count is a fact about the stream, not about the reader.
 *   STORE EVIDENCE       the toolkit's install witness executed and published (dr_install_exec_seen).
 *                        Without a store there is no write for a watch to miss, so a zero cannot be
 *                        read as NON_FIRING at all.
 *   ARM EVIDENCE         this instrument armed at least one thread (dr_armed > 0) at a known
 *                        canonical address. A watch that was never programmed cannot fail to fire.
 *   RECONCILED LEDGER    the lossless counters agree with each other, and NO BOUNDED PRINTED SAMPLE
 *                        IS ASKED TO DECIDE ANYTHING:
 *                          - continuations reconcile exactly (ok + failed == total);
 *                          - single-steps reconcile exactly (handler+generic+terminal+unrouted).
 *                        `dr_raw_ss_overflow` -- the raw-single-step ROW SAMPLE's overflow flag -- is
 *                        deliberately NOT an input. It is still published, because a truncated sample
 *                        is worth knowing, but the sample CORROBORATES and never DECIDES: the
 *                        counters it illustrates are uncapped and exact. Making a sample's capacity a
 *                        decision input is exactly the defect being repaired -- the old gate fed a
 *                        capacity-16 per-event array into `complete`, so a run length decided the
 *                        verdict. Reintroducing that through the row sample would rebuild the bug.
 *   SNAPSHOT             the terminal snapshot ran, is not premature, read no unsuspended thread,
 *                        and no armed tid is missing from it.
 *
 * WHAT `complete` DOES *NOT* CLAIM, and this is the Advisor's ruling made explicit. `complete=1`
 * means the OBSERVATION was lossless. It does NOT prove the #DB DELIVERY-COMPLETENESS PREMISE --
 * that this debugger's stream would carry a native install #DB if one occurred. That premise needs
 * the LIVE positive control, and it is exactly what `native_ingress=` reports below. Strong is not
 * decidable: a lossless zero is still not a delivered channel.
 *
 *   NON_FIRING          complete observation, store executed, watch armed and verified persistent at
 *                       the terminal snapshot, zero raw single-steps, and the delivery premise
 *                       proved by a native install #DB. The watch was programmed, stayed programmed,
 *                       and did not fire for a write that provably happened.
 *   UNKNOWN_NOT_RECORDED nothing was counted at all, or the ledger did not reconcile, or the
 *                       snapshot was void/premature/unreadable, or the delivery premise is unproved
 *                       -- so a zero carries no information. NEVER read as absence.
 *   DELIVERED_UNCLAIMED a raw single-step DID arrive but was not claimed by this instrument.
 *   CONTEXT_LOST        the terminal snapshot found an ARMED tid whose DR0/DR7 are gone. The
 *                       registers were programmed and did not survive -- a CHANGED SET, so the zero
 *                       is explained by context loss rather than by a watch that cannot fire.
 *   HIT                 a claimed single-step arrived; the write-once install-hit latch is set.
 *
 * `complete=` is the single word a reader keys on, and it is derived, not asserted. */
static void a2h_delivery_terminal(void)
{
    /* C1 reconciliation. Each identity is between UNCAPPED counters, so a lost record breaks the
     * identity instead of hiding inside a plausible total. */
    int continue_reconciled = (dr_continue_ok + dr_continue_failed) == dr_continue_total;
    int ss_reconciled = (dr_ss_routed_handler + dr_ss_routed_generic + dr_ss_routed_terminal +
                         dr_ss_unrouted) == dr_raw_single_step;
    int store_evidence = dr_install_exec_seen;
    int arm_evidence = (dr_armed > 0) && (dr_canonical != 0);
    /* The terminal snapshot's own completeness: it ran, it was not premature, it read nothing
     * unsuspended, and every tid this instrument armed was present in it. */
    int snapshot_evidence = (dr_term_runs > 0) && !dr_term_premature_disarm &&
                            (dr_term_void_unsuspended == 0) && (dr_term_disarmed_tids == 0);
    int complete = (dr_raw_event_total > 0) && continue_reconciled && ss_reconciled &&
                   store_evidence && arm_evidence && snapshot_evidence;
    /* The delivery-completeness PREMISE, reported SEPARATELY and never folded into `complete`.
     *
     * THE ADVISOR'S RULE, MADE STRUCTURAL. "Strong != decidable": a lossless zero is not by itself
     * proof that this debugger's stream would have carried a native install #DB had one occurred.
     * That premise is proved ONLY by observing native #DB ingress, so it is published as its own
     * field next to the decision instead of being assumed.
     *
     * It is deliberately NOT an input to `decision`. Making NON_FIRING require a hit would be
     * circular -- a hit means the watch FIRED, so NON_FIRING would again be unreachable by
     * construction, which is precisely the defect this repair removes. The instrument reports its
     * own answer (NON_FIRING on a complete observation); the P1 decision-grade promotion is the
     * reader's, and it must be refused while delivery_premise=UNPROVED. */
    int native_ingress = (dr_raw_single_step > 0) || (dr_ss_routed_handler > 0);
    int premise_proved = native_ingress;
    const char *premise = premise_proved ? "PROVED" : "UNPROVED";
    const char *decision;
    if (dr_install_hit_latch) decision = "HIT";
    else if (!complete) decision = "UNKNOWN_NOT_RECORDED";
    else if (dr_raw_single_step) decision = "DELIVERED_UNCLAIMED";
    else if (dr_term_changed) decision = "CONTEXT_LOST";
    else decision = "NON_FIRING";
    fprintf(report, "GUEST_DR_DELIVERY_TERMINAL raw_events=%u raw_exceptions=%u raw_single_step=%u "
                    "ss_first_chance=%u ss_second_chance=%u ss_routed_handler=%u "
                    "ss_routed_generic_first=%u ss_routed_terminal_second=%u ss_unrouted=%u "
                    "ss_records=%u ss_overflow=%u other_code_exceptions=%u continue_total=%u "
                    "continue_continue=%u continue_not_handled=%u continue_failed=%u "
                    "continue_reconciled=%d ss_reconciled=%d store_evidence=%d arm_evidence=%d "
                    "snapshot_evidence=%d native_ingress=%d premise_proved=%d delivery_premise=%s "
                    "install_hit=%ld "
                    "watch_armed_value=%016llX canonical=%016llX complete=%d decision=%s\n",
            dr_raw_event_total, dr_raw_exception_total, dr_raw_single_step, dr_ss_first_chance,
            dr_ss_second_chance, dr_ss_routed_handler, dr_ss_routed_generic,
            dr_ss_routed_terminal, dr_ss_unrouted, dr_raw_ss_count, dr_raw_ss_overflow,
            dr_raw_exception_other_code, dr_continue_total, dr_continue_continue,
            dr_continue_not_handled, dr_continue_failed, continue_reconciled, ss_reconciled,
            store_evidence, arm_evidence, snapshot_evidence, native_ingress, premise_proved,
            premise, (long)dr_install_hit_latch,
            (unsigned long long)dr_watch_armed_value, (unsigned long long)dr_canonical,
            complete, decision);
    /* C2: THE SNAPSHOT RECORD, reconciled stage by stage. Every stage is published with both its
     * success and its failure count, so a partial snapshot is visible as partial. */
    fprintf(report, "GUEST_DR_TERM_SNAPSHOT runs=%u snapshot_ok=%u snapshot_failed=%u seen=%u "
                    "open_ok=%u open_failed=%u suspend_ok=%u suspend_failed=%u already_suspended=%u "
                    "context_ok=%u context_failed=%u resume_ok=%u resume_failed=%u "
                    "void_unsuspended=%u verified_armed=%u changed_set=%u unreadable=%u unarmed=%u "
                    "armed_tids_exited=%u armed_tids_missing=%u premature_disarm=%u rows=%u "
                    "row_overflow=%u canonical=%016llX\n",
            dr_term_runs, dr_term_snapshot_ok, dr_term_snapshot_failed, dr_term_seen,
            dr_term_open_ok, dr_term_open_failed, dr_term_suspend_ok, dr_term_suspend_failed,
            dr_term_already_suspended, dr_term_context_ok, dr_term_context_failed, dr_term_resume_ok,
            dr_term_resume_failed, dr_term_void_unsuspended, dr_term_verified, dr_term_changed,
            dr_term_unreadable, dr_term_unarmed_rows, dr_term_exited_tids, dr_term_disarmed_tids,
            dr_term_premature_disarm, dr_term_count, dr_term_overflow,
            (unsigned long long)dr_canonical);
    /* C3: THE DISCRIMINATOR VERDICT, derived from the same records. Each cause leaves a DIFFERENT
     * trace, which is the whole point -- the packet refuses a hypothesis without a discriminating
     * control:
     *
     *   CHANCE_SEMANTICS   a raw single-step arrived as SECOND chance while no first-chance
     *                      single-step did. The event was delivered; the chance is the story.
     *   DEBUGGER_SWALLOW   a raw single-step was delivered and then routed to a path that did not
     *                      claim it (generic first-chance, or consumed with DBG_CONTINUE and no
     *                      downstream hit). Delivered, then handled away.
     *   CONTEXT_LOSS       the terminal snapshot found an armed tid whose DR0/DR7 are gone. The
     *                      registers were programmed and did not survive.
     *   NO_RAW_EVENT       nothing arrived at all: only then do the snapshot's verified armed state
     *                      and the toolkit's store witness discriminate delivery from handling.
     *
     * EVERY CAUSE BELOW REQUIRES `complete`. A cause is a discrimination between hypotheses, and a
     * truncated or unread ledger discriminates nothing -- so an incomplete ledger yields
     * UNKNOWN_NOT_RECORDED even when single-steps WERE counted, rather than letting a partial count
     * name a cause. The one exception is a hit: a claimed single-step is a positive observation that
     * a lost row cannot un-observe. */
    {
        const char *cause;
        if (dr_install_hit_latch) cause = "HIT_OBSERVED";
        else if (!complete) cause = "UNKNOWN_NOT_RECORDED";
        else if (dr_ss_second_chance && !dr_ss_first_chance) cause = "CHANCE_SEMANTICS";
        else if (dr_raw_single_step) cause = "DEBUGGER_SWALLOW";
        else if (dr_term_changed) cause = "CONTEXT_LOSS";
        else cause = "NO_RAW_EVENT";
        fprintf(report, "GUEST_DR_CAUSE first_chance_ss=%u second_chance_ss=%u routed_handler=%u "
                        "routed_elsewhere=%u term_verified_armed=%u term_changed_set=%u "
                        "term_unreadable=%u term_void_unsuspended=%u native_ingress=%d "
                        "premise_proved=%d delivery_premise=%s complete=%d cause=%s\n",
                dr_ss_first_chance, dr_ss_second_chance, dr_ss_routed_handler,
                dr_ss_routed_generic + dr_ss_routed_terminal, dr_term_verified, dr_term_changed,
                dr_term_unreadable, dr_term_void_unsuspended, native_ingress, premise_proved,
                premise, complete, cause);
    }
    for (unsigned i = 0; i < dr_raw_ss_count; i++)
        fprintf(report, "GUEST_DR_RAW_SS_ROW index=%u seq=%u tid=%lu code=%08lX chance=%s route=%s "
                        "address=%016llX ticks=%llu\n",
                i, dr_raw_ss[i].seq, dr_raw_ss[i].tid, dr_raw_ss[i].code,
                dr_raw_ss[i].chance == A2H_CHANCE_FIRST ? "first" : "second",
                dr_raw_ss[i].route == A2H_ROUTE_HANDLER ? "handler" :
                (dr_raw_ss[i].route == A2H_ROUTE_GENERIC_FIRST ? "generic_first" :
                 (dr_raw_ss[i].route == A2H_ROUTE_TERMINAL_SECOND ? "terminal_second" : "unrouted")),
                dr_raw_ss[i].address, (unsigned long long)dr_raw_ss[i].ticks);
    /* The terminal snapshot rows. PRINTED SAMPLE ONLY -- nothing above reads this array. */
    for (unsigned i = 0; i < dr_term_count; i++)
        fprintf(report, "GUEST_DR_TERM_ROW index=%u tid=%lu armed=%u suspended=%u read_ok=%u "
                        "dr0=%016llX dr7=%016llX dr6=%016llX canonical=%016llX verdict=%s\n",
                i, dr_term[i].tid, dr_term[i].armed, dr_term[i].suspended, dr_term[i].ok,
                (unsigned long long)dr_term[i].dr0, (unsigned long long)dr_term[i].dr7,
                (unsigned long long)dr_term[i].dr6, (unsigned long long)dr_canonical,
                dr_term[i].verdict == A2H_TERM_VERIFIED_ARMED ? "verified_armed" :
                (dr_term[i].verdict == A2H_TERM_CHANGED_SET ? "changed_set" :
                 (dr_term[i].verdict == A2H_TERM_UNARMED ? "unarmed" : "unreadable")));
    fflush(report);
}

static void dr_handshake(DWORD tid)
{
    /* NATIVE #DB FIXTURE: the child published the address it armed in its handshake parameters, so
     * the watch is pointed at the child's REAL target rather than at a fabricated value. This is
     * done FIRST, before the geometry resolution below, because the fixture's debuggee is the
     * collector itself and has no mapping symbols -- without this the handshake would bail at
     * `!dr_canonical` and never arm anything. In production a2h_native_db_fixture is 0, so this
     * whole clause is inert and dr_canonical comes from the symbol table unchanged. */
    if (a2h_native_db_fixture && !dr_canonical && a2h_native_db_params) {
        EXCEPTION_DEBUG_INFO *info = a2h_native_db_params;
        if (info->ExceptionRecord.NumberParameters >= 1 &&
            info->ExceptionRecord.ExceptionInformation[0])
            dr_canonical = (DWORD64)info->ExceptionRecord.ExceptionInformation[0];
    }
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

/* ── C1: the RAW, PRE-FILTER single-step record ─────────────────────────────────────────────────
 *
 * Called from the TOP of the exception branch in the debug loop, before any gate, filter or routing
 * decision. A count taken anywhere later could miss an event that a filter dropped, which is the
 * exact absent-record failure this packet exists to close. Nothing here decides anything: it records
 * that the event was DELIVERED and which chance it was.
 *
 * THE COUNTER IS SINGLE-STEP ONLY. A non-single-step exception is counted in
 * dr_raw_exception_other_code instead, so the raw single-step count answers exactly one question --
 * "did a #DB reach the debugger?" -- and cannot be polluted by the ~14k unrelated events a live run
 * delivers. That separation is what makes a zero mean NON-FIRING rather than UNKNOWN.
 *
 * `dr_raw_event_total` is incremented by the caller for EVERY debug event, so the raw single-step
 * count always has a completeness denominator next to it.
 *
 * RETURNS THE SEQ OF A SINGLE-STEP, AND 0 FOR EVERY OTHER CODE. That return value is load-bearing:
 * the caller uses it to decide whether the event may be ROUTED. Before this, the caller stamped a
 * seq for EVERY exception and then routed non-single-steps through a2h_raw_ss_route, so the route
 * counters counted ROUTES rather than single-step routes -- the Phase-1 archive shows
 * ss_routed_generic_first=14405 against raw_single_step=0, which is that pollution. A route counter
 * that cannot reconcile with the single-step count cannot support a reconciliation clause. */
static unsigned a2h_raw_single_step(DWORD tid, DWORD code, DWORD first_chance, DWORD64 address)
{
    LARGE_INTEGER now;
    unsigned seq;
    unsigned chance = first_chance ? A2H_CHANCE_FIRST : A2H_CHANCE_SECOND;
    int is_single_step = (code == EXCEPTION_SINGLE_STEP);

    /* THE COMPLETENESS DENOMINATOR IS BUMPED FIRST, before the code filter below can return early.
     * If it were bumped only for single-steps, a run that delivered no single-step at all would show
     * events=0 as well, and "the stream was read and contained no #DB" would be indistinguishable
     * from "nothing was recorded" -- which is precisely the NON-FIRING / NOT-RECORDED confusion the
     * packet's losslessness clause forbids. */
    if (is_single_step) dr_raw_single_step++;
    else dr_raw_exception_other_code++;
    if (!is_single_step) return 0;   /* counted; not a delivery this ledger classifies or routes */

    seq = ++dr_seq;
    QueryPerformanceCounter(&now);
    if (first_chance) dr_ss_first_chance++; else dr_ss_second_chance++;
    if (dr_raw_ss_count < A2H_RAW_SS_CAPACITY) {
        dr_raw_ss[dr_raw_ss_count].seq = seq;
        dr_raw_ss[dr_raw_ss_count].chance = chance;
        dr_raw_ss[dr_raw_ss_count].route = A2H_ROUTE_NONE;   /* updated by the router below */
        dr_raw_ss[dr_raw_ss_count].tid = tid;
        dr_raw_ss[dr_raw_ss_count].code = code;
        dr_raw_ss[dr_raw_ss_count].ticks = (DWORD64)now.QuadPart;
        dr_raw_ss[dr_raw_ss_count].address = address;
        dr_raw_ss_count++;
    } else dr_raw_ss_overflow = 1;
    /* Corroboration only -- the counters above are the decision inputs. Printed for EVERY raw
     * single-step, before any routing, so the log and the ledger agree by construction. */
    fprintf(report, "GUEST_DR_RAW_SS seq=%u tid=%lu code=%08lX chance=%s address=%016llX "
                    "raw_ss_total=%u raw_events=%u\n",
            seq, tid, code, first_chance ? "first" : "second",
            (unsigned long long)address, dr_raw_single_step, dr_raw_event_total);
    fflush(report);
    return seq;
}

/* Records where a raw single-step went. Kept separate from the recorder above so that a reader can
 * see an event that was COUNTED but never ROUTED -- that gap is the filter boundary made visible.
 *
 * ROUTES ARE SINGLE-STEP ONLY. seq==0 means the event was not a single-step, so it is counted as
 * neither routed nor unrouted: the route counters must reconcile with dr_raw_single_step, and a
 * non-single-step exception is accounted for in dr_raw_exception_other_code instead. */
static void a2h_raw_ss_route(unsigned seq, unsigned route)
{
    if (!seq) return;
    for (unsigned i = dr_raw_ss_count; i > 0; i--)
        if (dr_raw_ss[i - 1].seq == seq) { dr_raw_ss[i - 1].route = route; break; }
    switch (route) {
    case A2H_ROUTE_HANDLER:        dr_ss_routed_handler++; break;
    case A2H_ROUTE_GENERIC_FIRST:  dr_ss_routed_generic++; break;
    case A2H_ROUTE_TERMINAL_SECOND:dr_ss_routed_terminal++; break;
    default:                       dr_ss_unrouted++; break;
    }
}

/* ── C1(3): the CONTINUATION record, at the continuation site ───────────────────────────────────
 *
 * This replaces the deleted post-continue readback. It performs NO context read at all: it counts
 * what the debugger did, which is what a reconciliation needs, and it touches no thread state.
 * Observation only, and behind the same gate as everything else. */
static void a2h_continue_record(unsigned continue_status, int continue_ok)
{
    dr_continue_total++;
    if (!continue_ok) dr_continue_failed++;
    else {
        dr_continue_ok++;
        if (continue_status == DBG_CONTINUE) dr_continue_continue++;
        else if (continue_status == DBG_EXCEPTION_NOT_HANDLED) dr_continue_not_handled++;
    }
}

/* ── C2: THE TERMINAL DR SNAPSHOT ───────────────────────────────────────────────────────────────
 *
 * THE ONE PLACE THIS FILE READS DEBUG REGISTERS, and it is built so that an unsuspended read cannot
 * produce a value that could be mistaken for evidence.
 *
 * THE VOID RULE, ENFORCED STRUCTURALLY. `suspended` is a REQUIRED parameter and the function returns
 * WITHOUT CALLING GetThreadContext when it is 0 -- it counts the attempt in dr_term_void_unsuspended
 * and returns 0. The caller's only argument is the result of a SuspendThread that returned success,
 * so in production the unsuspended path is unreachable; it exists so the rule is a checked property
 * of the code rather than a comment, and so the fixture can drive the VOID path directly. A non-zero
 * dr_term_void_unsuspended voids the snapshot in the decision below.
 *
 * SUSPEND IS NOT OPTIONAL. ContinueDebugEvent resumes the target; Windows documents thread context
 * as valid only while the thread is suspended. That is exactly the defect being repaired, so every
 * read here is preceded by a successful SuspendThread on the SAME handle.
 *
 * Observation only: GetThreadContext reads. This function never writes a debug register, never
 * writes guest memory, and never resumes a thread it did not suspend. */
static int a2h_term_read(HANDLE handle, DWORD tid, int armed, int suspended,
                         DWORD64 *dr0, DWORD64 *dr7, DWORD64 *dr6, unsigned *verdict)
{
    CONTEXT context = {0};
    int ok;
    *dr0 = *dr7 = *dr6 = 0;
    *verdict = A2H_TERM_UNREADABLE;
    if (!suspended) {
        /* AN UNSUSPENDED READ IS VOID. Not a warning, not a caveat: it is refused and counted, so no
         * value from a running thread can ever reach the ledger. */
        dr_term_void_unsuspended++;
        return 0;
    }
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(handle, &context)) {
        dr_term_context_failed++;
        dr_term_unreadable++;
        return 0;
    }
    dr_term_context_ok++;
    *dr0 = context.Dr0; *dr7 = context.Dr7; *dr6 = context.Dr6;
    /* The comparison the packet requires: this thread's live DR state against the ARMED
     * tid/address/DR7 set and the install-trap/hit records. */
    ok = (*dr0 == dr_canonical) && ((*dr7 & A2H_DR7_OWNED) == A2H_DR7);
    if (ok) {
        *verdict = A2H_TERM_VERIFIED_ARMED;
        dr_term_verified++;
    } else if (armed) {
        /* It WAS armed by this instrument and is not armed now. That is a CHANGED SET: the packet
         * makes it UNKNOWN, never a negative -- an unarmed-at-snapshot thread cannot support a
         * "the watch did not fire" reading. */
        *verdict = A2H_TERM_CHANGED_SET;
        dr_term_changed++;
    } else {
        *verdict = A2H_TERM_UNARMED;
        dr_term_unarmed_rows++;
    }
    return 1;
}

/* Enumerate and open ALL target threads, suspend each ONCE, read its DR state, then resume it.
 *
 * ORDERING IS THE POINT. This is called at/after the fatal exception and BEFORE dr_disarm_all(), so
 * the snapshot observes the armed process rather than the disarmed one. If it ever runs after the
 * disarm it says so (dr_term_premature_disarm) and the decision refuses the reading.
 *
 * ONE SUSPEND PER THREAD, NOT A PER-EVENT LOOP. The snapshot runs ONCE per process; there is no
 * sampling loop anywhere in this file. */
static void a2h_terminal_snapshot(void)
{
    HANDLE snapshot;
    THREADENTRY32 entry = {sizeof(entry)};

    if (!dr_on() || !dr_canonical) return;
    /* AT MOST ONCE, and never after the process is gone. */
    if (dr_term_done) return;
    dr_term_done = 1;
    dr_term_runs++;
    if (dr_disarmed_flag) dr_term_premature_disarm++;
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        dr_term_snapshot_failed++;
        fprintf(report, "GUEST_DR_TERM_SNAPSHOT ok=0 reason=snapshot error=%lu\n", GetLastError());
        fflush(report);
        return;
    }
    dr_term_snapshot_ok++;
    if (Thread32First(snapshot, &entry)) do {
        HANDLE handle;
        DWORD64 dr0 = 0, dr7 = 0, dr6 = 0;
        unsigned verdict = A2H_TERM_UNREADABLE;
        int armed, suspended = 0, resumed = 0, read_ok;
        DWORD suspend_count = 0;
        if (entry.th32OwnerProcessID != process_id) continue;
        /* NEVER SUSPEND THE COLLECTOR'S OWN THREAD. In production the debugger is a separate process,
         * so this cannot arise; the fixture drives this path IN-PROCESS, where suspending the caller
         * would deadlock it. Skipped rather than read, and skipped rather than resumed: this thread
         * was never suspended, so it is owed no resume and its own context is not a target reading. */
        if (entry.th32ThreadID == GetCurrentThreadId()) continue;
        dr_term_seen++;
        armed = dr_was_armed(entry.th32ThreadID);
        handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                            THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        if (!handle) {
            /* A thread we could not open is UNKNOWN: a missing thread is explicitly not a negative. */
            dr_term_open_failed++;
            dr_term_unreadable++;
            if (dr_term_count < A2H_TERM_CAPACITY) {
                dr_term[dr_term_count].tid = entry.th32ThreadID;
                dr_term[dr_term_count].armed = (unsigned)armed;
                dr_term[dr_term_count].verdict = A2H_TERM_UNREADABLE;
                dr_term_count++;
            } else dr_term_overflow = 1;
            continue;
        }
        dr_term_open_ok++;
        /* SUSPEND ONCE, THEN READ ONLY IF THE SUSPEND SUCCEEDED.
         *
         * SuspendThread returns the PREVIOUS suspend count, or (DWORD)-1 on failure. A NON-ZERO
         * previous count is NOT a failure and must not be treated as one: a debug event freezes every
         * thread in the target, so the threads this snapshot most needs to read are exactly the ones
         * that already have a non-zero count. What matters for the VOID rule is only whether the
         * thread is suspended when GetThreadContext runs -- and after a successful SuspendThread it
         * always is, because this call incremented the count.
         *
         * So: (DWORD)-1 -> failed, do NOT read, do NOT resume (nothing was added). Anything else ->
         * this call added exactly one suspension, the thread is now suspended, the read is VALID, and
         * exactly ONE ResumeThread is owed. `already` is recorded so a reader can see that the thread
         * was frozen by the debugger rather than by this call. */
        suspend_count = SuspendThread(handle);
        if (suspend_count == (DWORD)-1) {
            dr_term_suspend_failed++;
            dr_term_unreadable++;
        } else {
            dr_term_suspend_ok++;
            if (suspend_count > 0) dr_term_already_suspended++;
            suspended = 1;
        }
        read_ok = a2h_term_read(handle, entry.th32ThreadID, armed, suspended,
                                &dr0, &dr7, &dr6, &verdict);
        if (suspended) {
            /* RESUME SAFELY: only the thread this call actually suspended, and only once. A failure
             * is recorded rather than ignored -- a thread left suspended is a real side effect. */
            if (ResumeThread(handle) == (DWORD)-1) dr_term_resume_failed++;
            else { dr_term_resume_ok++; resumed = 1; }
        }
        if (!read_ok) verdict = A2H_TERM_UNREADABLE;
        if (dr_term_count < A2H_TERM_CAPACITY) {
            dr_term[dr_term_count].tid = entry.th32ThreadID;
            dr_term[dr_term_count].dr0 = dr0;
            dr_term[dr_term_count].dr7 = dr7;
            dr_term[dr_term_count].dr6 = dr6;
            dr_term[dr_term_count].armed = (unsigned)armed;
            dr_term[dr_term_count].suspended = (unsigned)(suspended && resumed);
            dr_term[dr_term_count].ok = (unsigned)read_ok;
            dr_term[dr_term_count].verdict = verdict;
            dr_term_count++;
        } else dr_term_overflow = 1;
        CloseHandle(handle);
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    /* A tid this instrument armed that no longer exists as a live thread is a CHANGED SET -- but ONLY
     * if its disappearance has no lifecycle record.
     *
     * WHY THIS DISTINCTION IS LOAD-BEARING. Guest worker threads EXIT during a run (the harness's own
     * healthy probe expects four exited guest workers). An armed worker that exited before the
     * terminal point is a KNOWN lifecycle fact already recorded in the birth ledger, not a lost
     * watch; treating it as a changed set would make snapshot_evidence=0 on essentially every real
     * run, so `complete` would be structurally 0 and NON_FIRING unreachable AGAIN -- the exact defect
     * this repair removes, merely relocated. So a tid WITH an exit row is the EXIT population (the
     * same rule dr_tid_exited already encodes for terminal unarmed tids), and a tid that vanished
     * with NO exit record is the genuine changed set the packet makes UNKNOWN. */
    for (unsigned i = 0; i < dr_arm_tid_count; i++) {
        int seen = 0;
        for (unsigned j = 0; j < dr_term_count; j++)
            if (dr_term[j].tid == dr_arm_tids[i]) { seen = 1; break; }
        if (seen) continue;
        if (dr_tid_exited(dr_arm_tids[i])) dr_term_exited_tids++;
        else dr_term_disarmed_tids++;
    }
    fflush(report);
}

/* The write-once install-hit latch, in its own function so the fixture drives THIS code rather than a
 * copy of it. Called only when DR6.B0 -- this instrument's own watch bit -- is set on a thread this
 * instrument armed. */
static void a2h_publish_install_hit(DWORD tid, DWORD64 dr6, DWORD64 dr7, DWORD64 rip)
{
    if (InterlockedCompareExchange(&dr_install_hit_latch, 1, 0) != 0) return;
    dr_install_hit_seq = dr_seq;
    dr_install_hit_tid = tid;
    dr_install_hit_rip = rip;
    dr_install_hit_dr6 = dr6;
    dr_install_hit_dr7 = dr7;
    fprintf(report, "GUEST_DR_INSTALL_HIT seq=%u tid=%lu rip=%016llX dr6=%016llX "
                    "dr7=%016llX canonical=%016llX watch_armed_value=%016llX\n",
            dr_install_hit_seq, tid, (unsigned long long)rip, (unsigned long long)dr6,
            (unsigned long long)dr7, (unsigned long long)dr_canonical,
            (unsigned long long)dr_watch_armed_value);
    fflush(report);
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
    /* C1: THE WRITE-ONCE INSTALL-HIT LATCH. This is the delivery the Phase-1 gate requires: a native
     * single-step whose DR6.B0 -- this instrument's own watch bit -- is set, on a thread this
     * instrument armed. Set ONCE, with the DR7 read back at the event, so a later event cannot
     * overwrite the first one and a reader can compare the watch value here against the value the
     * toolkit's install witness recorded for the same store. */
    a2h_publish_install_hit(tid, dr6, context.Dr7, context.Rip);
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
    /* C2: STAMP THE DISARM. From here on the process is being disarmed, so any terminal snapshot
     * taken later observes a cleared process and is recorded as PREMATURE rather than as evidence. */
    dr_disarmed_flag = 1;
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
        /* C1 STORE EVIDENCE. The delivery decision requires that the install store EXECUTED, and this
         * is where that fact enters the collector: the toolkit's install witness sets install_seen
         * only after performing the store and reading the location back, so the latch cannot be set
         * by a print or by a store that did not happen. Recorded here rather than assumed, because
         * `complete` consumes it and a fixture must be able to drive its absence. */
        if (latch->install_seen) dr_install_exec_seen = 1;
        dr_install_ok_value = latch->install_ok;
        dr_install_seen_value = latch->install_seen;
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
    /* ── A2h LIVE slot-write ledger (page protection, no debug registers) ───────────────────────
     *
     * A SEPARATE OBJECT, resolved by its own symbol, and deliberately NOT part of JsrfRegistry: it
     * is written by the TOOLKIT, whose write scope does not include the game's diagnostics, and the
     * collector must archive it whether or not the game ever publishes a registry.
     *
     * ARCHIVED BY SYMBOL, like g_jsrf_debug, so the frozen BYTES travel in the minidump and the text
     * below is a cross-check that must reconcile with them rather than a substitute. Every field
     * printed here is a decision input: a reader must be able to see whether the watch armed, on
     * which alias mask, with what loss accounting, and what the FOURTH read produced, without first
     * extracting the minidump.
     *
     * THE LOSS ASYMMETRY IS STRUCTURAL, NOT DECORATIVE. Overflow or an unreconciled interval
     * invalidates ABSENCE/ORDER rows; it does NOT invalidate a positive self-contained writer
     * record. So the overflow latch is printed as its own field and every counter is printed
     * unconditionally -- a reader decides which rows survive, and the archive must not pre-decide by
     * omitting a counter that happens to be zero. */
    {
        DWORD64 slotw_addr = symbol_address("g_xbox_a2h_slotw");
        XboxA2hSlotwLedger *sw = malloc(sizeof(*sw));
        if (!sw) {
            fprintf(report, "GUEST_SLOTW unavailable reason=alloc\n");
        } else if (!slotw_addr || !read_remote(slotw_addr, sw, sizeof(*sw))) {
            fprintf(report, "GUEST_SLOTW unavailable reason=symbol_or_read\n");
            free(sw); sw = NULL;
        } else if (sw->magic != A2H_SLOTW_MAGIC) {
            /* ⚠ magic == 0 IS NOT A LAYOUT MISMATCH, AND REPORTING IT AS ONE WOULD BE WRONG.
             *
             * The ledger's magic is written by ARM. With the gate unset -- or with ARM refused
             * because the title had not allocated its device yet -- the object is still all zeros,
             * which is the CORRECT state for an inert run and is exactly what the OFF baseline
             * looks like. A non-zero magic that does not match is the real defect: the struct
             * changed without the version moving, or the symbol resolved to something else.
             *
             * The two are separated here so an OFF run cannot be misread as a broken instrument,
             * and a broken instrument cannot be misread as an inert run. */
            if (sw->magic == 0)
                fprintf(report, "GUEST_SLOTW unarmed reason=never_initialised "
                                "note=gate_unset_or_ARM_refused_ledger_is_all_zero\n");
            else
                fprintf(report, "GUEST_SLOTW unavailable reason=magic magic=%08X expected=%08X\n",
                        sw->magic, A2H_SLOTW_MAGIC);
            free(sw); sw = NULL;
        } else if (sw->size != sizeof(*sw)) {
            /* ⚠ THIS IS THE CHECK THAT ACTUALLY CATCHES A STRUCT DRIFT, and the static assert above
             * is not. The toolkit stamps sizeof(its own struct) into `size`; if the two ever differ,
             * every field after the change would be read at the wrong offset, so the ledger is
             * reported UNAVAILABLE rather than interpreted. The version is reported alongside so a
             * reader can see whether the mirror is merely older or genuinely misaligned. */
            fprintf(report, "GUEST_SLOTW unavailable reason=size read=%u collector=%u "
                            "version=%u collector_version=%u\n",
                    sw->size, (unsigned)sizeof(*sw), sw->version, A2H_SLOTW_VERSION);
            free(sw); sw = NULL;
        } else if (sw->version != A2H_SLOTW_VERSION) {
            /* Same size, different version: the field MEANINGS may have changed even though the
             * offsets did not, so this is reported as unavailable too rather than read on the
             * assumption that an equal size implies an equal layout. */
            fprintf(report, "GUEST_SLOTW unavailable reason=version read=%u collector=%u size=%u\n",
                    sw->version, A2H_SLOTW_VERSION, sw->size);
            free(sw); sw = NULL;
        } else {
            extra_memory[extra_count].base = slotw_addr;
            extra_memory[extra_count++].size = sizeof(*sw);
            fprintf(report, "GUEST_SLOTW address=%016llX version=%u size=%u armed=%u arm_reason=%u "
                            "arm_base=%08X term_base=%08X arm_slot=%08X term_slot=%08X stable=%u "
                            "term_base_ok=%u page_offset=%03X alias_count=%u protected_count=%u "
                            "mapped_mask=%08X protect_mask=%08X\n",
                    (unsigned long long)slotw_addr, sw->version, sw->size, sw->armed, sw->arm_reason,
                    sw->arm_base, sw->term_base, sw->arm_slot, sw->term_slot, sw->slot_stable,
                    sw->term_base_ok, sw->page_offset, sw->alias_count, sw->protected_count,
                    sw->mapped_mask, sw->protect_mask);
            /* THE LOSS ACCOUNTING, ALL OF IT, ALWAYS. Counted at the event, never sampled. */
            fprintf(report, "GUEST_SLOTW_LOSS relevant_av=%llu slot_hits=%llu nonslot_writes=%llu "
                            "steps=%llu rearm_ok=%llu rearm_failed=%llu protected_intervals=%llu "
                            "unprotected_intervals=%llu concurrent_overlap=%llu threads_new=%llu "
                            "threads_gone=%llu publish_failed=%llu protect_failed=%llu "
                            "dropped_events=%llu unexpected_exception=%llu db_unowned=%llu "
                            "db_own_serviced=%llu db_ac97_serviced=%llu db_dual_serviced=%llu "
                            "read_samples=%llu installer_control_hits=%llu base_changed=%llu "
                            "nonslot_distinct=%llu first_touch_dropped=%llu\n",
                    (unsigned long long)sw->loss.relevant_av,
                    (unsigned long long)sw->loss.slot_hits,
                    (unsigned long long)sw->loss.nonslot_writes,
                    (unsigned long long)sw->loss.steps,
                    (unsigned long long)sw->loss.rearm_ok,
                    (unsigned long long)sw->loss.rearm_failed,
                    (unsigned long long)sw->loss.protected_intervals,
                    (unsigned long long)sw->loss.unprotected_intervals,
                    (unsigned long long)sw->loss.concurrent_overlap,
                    (unsigned long long)sw->loss.threads_new,
                    (unsigned long long)sw->loss.threads_gone,
                    (unsigned long long)sw->loss.publish_failed,
                    (unsigned long long)sw->loss.protect_failed,
                    (unsigned long long)sw->loss.dropped_events,
                    (unsigned long long)sw->loss.unexpected_exception,
                    (unsigned long long)sw->loss.db_unowned,
                    (unsigned long long)sw->loss.db_own_serviced,
                    (unsigned long long)sw->loss.db_ac97_serviced,
                    (unsigned long long)sw->loss.db_dual_serviced,
                    (unsigned long long)sw->loss.read_samples,
                    (unsigned long long)sw->loss.installer_control_hits,
                    (unsigned long long)sw->loss.base_changed,
                    (unsigned long long)sw->loss.nonslot_distinct,
                    (unsigned long long)sw->loss.first_touch_dropped);
            /* ── Q3(c): THE CROSS-VALIDATION, REPORTED AS A DENOMINATOR AND A MISMATCH COUNT ────
             *
             * "0 mismatches" is only meaningful next to how many pairs were compared, so both are
             * printed and a reader is never asked to infer the denominator. `checks` here is the
             * ledger's own slot-hit fault/step pairs; `loss.cross_checks` is every comparison the
             * facility made, including the ones the game-side hook requested. */
            fprintf(report, "GUEST_SLOTW_CROSS ledger_checks=%u ledger_mismatch=%u "
                            "loss_checks=%llu loss_mismatch=%llu loss_skipped=%llu "
                            "last_slot_read=%08X last_slot_read_seq=%u last_slot_read_hits=%u "
                            "last_slot_read_alias=%u change_seen=%u "
                            "terminal_alias=%08X terminal_guest=%08X terminal_cross_ok=%u\n",
                    sw->cross_checks, sw->cross_mismatch,
                    (unsigned long long)sw->loss.cross_checks,
                    (unsigned long long)sw->loss.cross_mismatch,
                    (unsigned long long)sw->loss.cross_skipped,
                    sw->last_slot_read_value, sw->last_slot_read_seq, sw->last_slot_read_hits,
                    sw->last_slot_read_alias, sw->last_slot_change_seen,
                    sw->terminal_alias_value, sw->terminal_guest_value, sw->terminal_cross_ok);
            /* THE FIRST-TOUCH CENSUS: the SET of addresses the page's traffic touched, one record
             * per distinct address. This is what replaces the per-write records for non-slot
             * traffic, and it is why a linear fill of any length costs no records for its repeats. */
            fprintf(report, "GUEST_SLOTW_CENSUS first_touch_count=%u first_touch_overflow=%u "
                            "nonslot_writes=%llu nonslot_distinct=%llu\n",
                    sw->first_touch_count, sw->first_touch_overflow,
                    (unsigned long long)sw->loss.nonslot_writes,
                    (unsigned long long)sw->loss.nonslot_distinct);
            for (unsigned f = 0; f < A2H_SLOTW_FIRST_TOUCH_MAX && f < sw->first_touch_count; f++) {
                const XboxA2hSlotwFirstTouch *ft = &sw->first_touch[f];
                if (!ft->valid) continue;
                fprintf(report, "GUEST_SLOTW_TOUCH index=%u off=%03X alias=%u tid=%u pre=%08X "
                                "slot=%08X enc=%u rip=%016llX ticks=%llu\n",
                        f, ft->offset, ft->alias_index, ft->tid, ft->pre_value,
                        ft->slot_value_at_touch, ft->enc, (unsigned long long)ft->rip,
                        (unsigned long long)ft->ticks);
            }
            /* THE OVERFLOW LATCH, ON ITS OWN LINE. A reader keys on this one word to decide whether
             * absence/order rows survive; it is never folded into a counter. */
            fprintf(report, "GUEST_SLOTW_OVERFLOW latch=%llu dropped=%llu event_overflow=%u "
                            "thread_overflow=%u event_count=%u thread_count=%u\n",
                    (unsigned long long)sw->loss.overflow,
                    (unsigned long long)sw->loss.dropped_events, sw->event_overflow,
                    sw->thread_overflow, sw->event_count, sw->thread_count);
            /* THE FOURTH READ, TIED BY ORDERED EVENT IDS. `last_write_seq` is the seq of the last
             * slot-hit write at or before read #4 -- an event id, not a log timestamp, so the tie
             * survives log truncation and interleaving. A reader compares fourth_seq against
             * last_write_seq, and compares last_write_enc against the installer's ModRM encoding,
             * rather than reconstructing the order from the text below. */
            fprintf(report, "GUEST_SLOTW_FOURTH reached=%u read_count=%u value=%08X seq=%u "
                            "last_write_seq=%u last_write_enc=%u last_write_alias=%u "
                            "last_write_value=%08X last_write_rip=%016llX last_write_ticks=%llu "
                            "terminal_seen=%u terminal_target=%08X\n",
                    sw->fourth_reached, sw->read_count, sw->fourth_value, sw->fourth_seq,
                    sw->last_write_seq, sw->last_write_enc, sw->last_write_alias,
                    sw->last_write_value, (unsigned long long)sw->last_write_rip,
                    (unsigned long long)sw->last_write_ticks, sw->terminal_seen,
                    sw->terminal_target);
            /* BOUNDED FULL EVENT RECORDS. A full record per event, never a sample and never
             * first-N; `seq` is the ordering key and is written last, so a record with seq==0 was
             * never completed and is skipped rather than printed as a zero-filled event. */
            for (unsigned e = 0; e < A2H_SLOTW_EVENTS_MAX && e < sw->event_count; e++) {
                const XboxA2hSlotwEvent *ev = &sw->events[e];
                if (!ev->seq) continue;
                fprintf(report, "GUEST_SLOTW_EVENT index=%u seq=%u kind=%u alias=%u slot_hit=%u "
                                "fault_va=%08X pre=%08X post=%08X tid=%u enc=%u rip=%016llX "
                                "ticks=%llu\n",
                        e, ev->seq, ev->kind, ev->alias_index, ev->slot_hit, ev->fault_va,
                        ev->pre_value, ev->post_value, ev->tid, ev->enc,
                        (unsigned long long)ev->rip, (unsigned long long)ev->ticks);
            }
            /* THE RECONCILIATION, DERIVED RATHER THAN ASSERTED. A reader must not have to trust
             * that the counters and the records agree; the agreement is computed here. `complete=1`
             * means every event the counters claim is present as a record.
             *
             * ⚠ THE EXPECTED RECORD COUNT IS NO LONGER "EVERY WRITE". Under the packet's own design
             * only SLOT BYTE writes are recorded, so a write record is expected per SLOT HIT and a
             * step record per SLOT-HIT STEP -- page traffic is counted and censused, never recorded.
             * `expected_records` is therefore `slot_hits + steps` plus the read samples, and the
             * reconciliation is against THAT rather than against `relevant_av`. Reconciling against
             * `relevant_av` would report a correctly-implemented instrument as incomplete on any
             * run with page traffic -- which is every run. */
            {
                unsigned records = 0;
                unsigned hits_in_records = 0;
                unsigned long long expected;
                for (unsigned e = 0; e < A2H_SLOTW_EVENTS_MAX; e++) {
                    if (!sw->events[e].seq) continue;
                    records++;
                    if (sw->events[e].kind == A2H_SLOTW_EV_KIND_WRITE && sw->events[e].slot_hit)
                        hits_in_records++;
                }
                /* A SLOT-HIT WRITE PUBLISHES EXACTLY TWO RECORDS: the fault record and, when its
                 * step is serviced, the step record. A TRAFFIC write publishes NONE. So the expected
                 * total is `2 * slot_hits + read_samples`, and it is independent of how much page
                 * traffic the run produced -- which is the property the repair exists to create. */
                expected = sw->loss.slot_hits * 2u + sw->loss.read_samples;
                fprintf(report, "GUEST_SLOTW_RECONCILE records=%u counted=%u expected=%llu "
                                "writes=%llu steps=%llu reads=%llu slot_hits=%llu "
                                "hits_in_records=%u traffic=%llu traffic_distinct=%llu "
                                "complete=%d overflow=%llu absence_rows_valid=%d "
                                "positive_records_valid=1\n",
                        records, sw->event_count, expected,
                        (unsigned long long)(sw->loss.relevant_av),
                        (unsigned long long)sw->loss.steps,
                        (unsigned long long)sw->loss.read_samples,
                        (unsigned long long)sw->loss.slot_hits, hits_in_records,
                        (unsigned long long)sw->loss.nonslot_writes,
                        (unsigned long long)sw->loss.nonslot_distinct,
                        (records == sw->event_count && !sw->event_overflow) ? 1 : 0,
                        (unsigned long long)sw->loss.overflow,
                        (!sw->event_overflow && !sw->loss.overflow && sw->loss.rearm_failed == 0
                         && sw->loss.publish_failed == 0 && sw->loss.protect_failed == 0
                         && sw->loss.concurrent_overlap == 0 && sw->loss.cross_mismatch == 0
                         && !sw->first_touch_overflow) ? 1 : 0);
            }
            free(sw);
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

/* ── C1/C2 fixture seams (A2h-dr0-terminal-snapshot) ────────────────────────────────────────────
 *
 * Same pattern as the seams above, and the same reason: the fixture must drive the PRODUCTION branch
 * bodies rather than a test-local copy, or the fixture proves nothing about the shipped instrument.
 * These expose the delivery ledger and the terminal snapshot so the packet's required cases can be
 * injected deterministically:
 *
 *   jsrf_a2h_test_raw_event()            the completeness denominator (dr_raw_event_total)
 *   jsrf_a2h_test_raw_single_step(...)   a raw pre-filter single-step -> returns its seq
 *   jsrf_a2h_test_raw_ss_route(seq, r)   where it went: 0 none, 1 handler, 2 generic_first,
 *                                        3 terminal_second  (the A2H_ROUTE_* values)
 *   jsrf_a2h_test_publish_install_hit()  the write-once hit latch
 *   jsrf_a2h_test_continue(...)          the continuation record (no context read)
 *   jsrf_a2h_test_store_evidence(...)    the toolkit's install-witness publication
 *   jsrf_a2h_test_terminal_snapshot()    the suspend/read/compare/resume snapshot
 *   jsrf_a2h_test_term_read(...)         ONE snapshot read, driven with suspended=0 to prove the
 *                                        UNSUSPENDED-IS-VOID rule is enforced, not documented
 *   jsrf_a2h_test_disarm_stamp()         marks the process disarmed, so a late snapshot is PREMATURE
 *   jsrf_a2h_test_delivery_terminal()    the published decision
 *   jsrf_a2h_test_install_hit_latch()    the latch, so a fixture asserts the record not the print
 *
 * Nothing in the collector calls them, they add no behaviour, and the gate still decides everything:
 * with JSRF_TRACE_A2H_DR unset they write into structures nothing ever publishes. */
void jsrf_a2h_test_raw_event(void)
{
    if (!dr_on()) return;
    dr_raw_event_total++;
}

unsigned jsrf_a2h_test_raw_single_step(DWORD tid, DWORD code, DWORD first_chance, DWORD64 address)
{
    if (!dr_on()) return 0;
    return a2h_raw_single_step(tid, code, first_chance, address);
}

void jsrf_a2h_test_raw_ss_route(unsigned seq, unsigned route)
{
    if (!dr_on()) return;
    a2h_raw_ss_route(seq, route);
}

void jsrf_a2h_test_publish_install_hit(DWORD tid, DWORD64 dr6, DWORD64 dr7, DWORD64 rip)
{
    if (!dr_on()) return;
    a2h_publish_install_hit(tid, dr6, dr7, rip);
}

void jsrf_a2h_test_continue(DWORD tid, unsigned status, int ok)
{
    (void)tid;
    if (!dr_on()) return;
    a2h_continue_record(status, ok);
}

/* The toolkit's install-witness publication, as the collector observes it in the archived registry.
 * A fixture sets it so the STORE-EVIDENCE clause of `complete` is exercised rather than assumed. */
void jsrf_a2h_test_store_evidence(unsigned install_ok, unsigned install_seen)
{
    if (!dr_on()) return;
    dr_install_exec_seen = 1;
    dr_install_ok_value = install_ok;
    dr_install_seen_value = install_seen;
}

void jsrf_a2h_test_terminal_snapshot(void)
{
    if (!dr_on()) return;
    a2h_terminal_snapshot();
}

/* ONE snapshot read, with the suspend flag supplied by the caller. The fixture passes suspended=0
 * to prove the VOID rule is ENFORCED BY THE CODE: the call must refuse, count the attempt, and
 * return 0 without reading a context. */
int jsrf_a2h_test_term_read(DWORD tid, int suspended, unsigned *verdict)
{
    HANDLE handle;
    DWORD64 dr0 = 0, dr7 = 0, dr6 = 0;
    int ok;
    if (!dr_on()) return 0;
    handle = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!handle) return 0;
    ok = a2h_term_read(handle, tid, 1, suspended, &dr0, &dr7, &dr6, verdict);
    CloseHandle(handle);
    return ok;
}

void jsrf_a2h_test_disarm_stamp(void)
{
    if (!dr_on()) return;
    dr_disarmed_flag = 1;
}

void jsrf_a2h_test_delivery_terminal(void)
{
    if (!dr_on()) return;
    a2h_delivery_terminal();
}

int jsrf_a2h_test_install_hit_latch(void) { return (int)dr_install_hit_latch; }

/* The fixture's arm target. In PRODUCTION the debugger is a separate process from the debuggee, so
 * the terminal snapshot never sees its own thread. The fixture runs in-process, so it arms a WORKER
 * thread and keeps it alive for the snapshot: that reproduces the production relationship (a target
 * thread that is not the reader) instead of suspending the caller and deadlocking. */
static volatile LONG a2h_fixture_worker_run;
static HANDLE a2h_fixture_worker_handle;
static DWORD a2h_fixture_worker_tid;
static DWORD WINAPI a2h_fixture_worker(LPVOID unused)
{
    (void)unused;
    while (InterlockedCompareExchange(&a2h_fixture_worker_run, 1, 1)) Sleep(1);
    return 0;
}

static DWORD a2h_fixture_arm_worker(void)
{
    a2h_fixture_worker_run = 1;
    a2h_fixture_worker_handle = CreateThread(NULL, 0, a2h_fixture_worker, NULL, CREATE_SUSPENDED,
                                             &a2h_fixture_worker_tid);
    if (!a2h_fixture_worker_handle) return 0;
    /* Armed while SUSPENDED, which is the only point at which the watch is complete for the thread's
     * whole life -- the same rule the CREATE_THREAD_DEBUG_EVENT path follows in production. */
    if (!dr_arm_thread(a2h_fixture_worker_handle, a2h_fixture_worker_tid, "fixture", NULL)) {
        InterlockedExchange(&a2h_fixture_worker_run, 0);
        ResumeThread(a2h_fixture_worker_handle);
        CloseHandle(a2h_fixture_worker_handle);
        a2h_fixture_worker_handle = NULL;
        return 0;
    }
    ResumeThread(a2h_fixture_worker_handle);
    return a2h_fixture_worker_tid;
}

static void a2h_fixture_release_worker(void)
{
    if (!a2h_fixture_worker_handle) return;
    InterlockedExchange(&a2h_fixture_worker_run, 0);
    WaitForSingleObject(a2h_fixture_worker_handle, 2000);
    CloseHandle(a2h_fixture_worker_handle);
    a2h_fixture_worker_handle = NULL;
}

/* ── NATIVE #DB INGRESS FIXTURE ─────────────────────────────────────────────────────────────────
 *
 * THE PACKET'S POSITIVE CONTROL, AND WHY IT MUST BE NATIVE. The delivery-completeness premise is
 * "would this debugger's stream carry an install #DB if one occurred?". No injected, logged or
 * software-compared imitation can answer that: a seam that calls a2h_raw_single_step() proves the
 * counter works, not that the OS delivers #DB to WaitForDebugEvent. So this fixture does the real
 * thing end to end:
 *
 *   CHILD   programs its OWN DR0/DR7 with a real SetThreadContext, raises the real handshake
 *           exception so the debugger arms it while it is STOPPED, then performs a real store to the
 *           watched address. That store must deliver a genuine EXCEPTION_SINGLE_STEP.
 *   PARENT  is the actual debugger. It arms the child at the handshake, counts the #DB at the TOP of
 *           its exception branch (native ingress, pre-filter), routes it, claims it from DR6.B0, and
 *           publishes the write-once install-hit latch.
 *
 * The watch address is discovered the way production discovers it -- through the handshake, while the
 * child is stopped -- rather than being fabricated by the parent. Reached only by an argv token that
 * scripts/run-jsrf.py never passes. */
static int a2h_native_db_child(void)
{
    /* THE CHILD DOES NOT PROGRAM ITS OWN DEBUG REGISTERS, and that is deliberate. The first version
     * did, and the debugger's arm then correctly REFUSED the thread as a COLLISION -- its own
     * collision guard saw a foreign DR7 and declined to clobber it, so the thread was never armed
     * and no hit could be claimed. That is the guard working, not a bug in it.
     *
     * Production arms the thread and the guest performs the store. This fixture reproduces exactly
     * that: the child publishes the address it will write, the DEBUGGER programs the watch while the
     * child is stopped at the handshake, and only then does the child store. */
    __try {
        RaiseException(JSRF_A2H_DR_HANDSHAKE, 0, 1,
                       (const ULONG_PTR[]){(ULONG_PTR)(uintptr_t)&a2h_native_db_target});
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        /* The debugger answered DBG_EXCEPTION_NOT_HANDLED, as it does in production; the toolkit's
         * own VEH is what consumes it there. Here it is consumed locally. */
    }
    /* THE STORE UNDER THE WATCH. The debugger armed this thread at the handshake above, so if the
     * watch works this store delivers a genuine native #DB to the debugger's WaitForDebugEvent. */
    a2h_native_db_target = 0xFE000104u;
    /* STAY ALIVE so the terminal snapshot can observe this thread still armed. Without this the
     * process exits first and the snapshot legitimately reports the armed tids as missing. */
    Sleep(30000);
    return 0;
}

/* ── C1/C2 DELIVERY + TERMINAL-SNAPSHOT FIXTURE ─────────────────────────────────────────────────
 *
 * WHY A FIXTURE MODE LIVES IN THE COLLECTOR. The packet requires the delivery cases to be
 * fixture-tested, and requires that no row read an untested or lossy counter. The decision semantics
 * live in a2h_delivery_terminal(), so the only honest fixture is one that drives THAT function -- a
 * Python reimplementation of the decision would test the reimplementation, not the instrument.
 *
 * It is reached only by an explicit argv token that scripts/run-jsrf.py never passes, it returns
 * before CreateProcess, and every seam it calls is itself gated. Absent the token, main() is
 * byte-for-byte the collector it was before.
 *
 * One case per process, on purpose: the install-hit latch is write-once and the counters accumulate,
 * so a single process could only ever publish one decision. Each case therefore gets a clean ledger,
 * which is also what makes the "absent hit" case a real zero rather than a leftover.
 *
 * WHAT THE CASES NOW PROVE. The old set could not prove the property that matters most, because the
 * old `complete` was structurally 0: there was no case in which NON_FIRING was REACHABLE. The
 * "absent" case below drives the full gate to a complete observation and asserts NON_FIRING, which
 * is the regression test for the defect being repaired. */
static int a2h_delivery_fixture(const char *case_name)
{
    DWORD tid;
    unsigned seq;
    unsigned i;
    unsigned verdict = 0;
    int armed;

    if (!dr_on()) {
        /* GATE OFF IS COMPLETELY INERT, including here: no record, no counter, no line -- the
         * artifact stays EMPTY, which is the same inertness the ctest arming fixture asserts. A
         * gate-off run must be indistinguishable from a run of the collector before this existed. */
        return 2;
    }
    /* No debuggee exists here, so the geometry is pinned rather than resolved from symbols -- the
     * same pin the ctest arming fixture uses, and the only way this path is reachable at all.
     * The canonical value is the address of a real object so the DR0 programming below is a real
     * SetThreadContext against a real address, not a fabricated register value. */
    process_id = GetCurrentProcessId();
    jsrf_a2h_test_geometry((DWORD64)(uintptr_t)&report);

    /* Arm a WORKER thread for real, so the terminal snapshot has a target it is entitled to verify
     * and the fixture exercises the same arm path the collector uses. It is a worker and not this
     * thread because the snapshot must suspend what it reads, and a process cannot suspend its own
     * calling thread without deadlocking. */
    tid = a2h_fixture_arm_worker();
    armed = tid ? 1 : 0;

    /* THE STORE PREMISE. Every case that expects a DECIDABLE answer sets it, because `complete`
     * requires store evidence: without a store there is no write for a watch to miss. The
     * `store_absent` case is the one that deliberately does not, and it is the control for this
     * clause. */
    if (strcmp(case_name, "store_absent")) jsrf_a2h_test_store_evidence(1, 1);

    if (!strcmp(case_name, "hit")) {
        /* An install event WITH a hit: a raw first-chance single-step, claimed by the handler, and
         * the write-once install-hit latch published. */
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_SINGLE_STEP, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_HANDLER);
        jsrf_a2h_test_publish_install_hit(tid, 0xF, A2H_DR7, dr_canonical);
    } else if (!strcmp(case_name, "absent")) {
        /* THE NON_FIRING WORLD, and the regression test for the repaired gate. The debug stream was
         * demonstrably read (two events of other kinds), the store executed, the watch was armed, the
         * terminal snapshot verified it armed and suspended, and NO single-step ever arrived. Under
         * the old gate this case was UNREACHABLE: the capacity-16 post-continue array set its
         * overflow flag on any real run, so `complete` was structurally 0. */
        jsrf_a2h_test_raw_event();
        jsrf_a2h_test_raw_event();
    } else if (!strcmp(case_name, "second_chance")) {
        /* Delivered as SECOND chance with no first-chance single-step: favors chance semantics. */
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_SINGLE_STEP, 0, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_TERMINAL_SECOND);
    } else if (!strcmp(case_name, "swallowed")) {
        /* Delivered, then routed to a first-chance path that is not this instrument's: the event
         * exists in the ledger but was consumed elsewhere. Favors debugger swallow. */
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_SINGLE_STEP, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_GENERIC_FIRST);
    } else if (!strcmp(case_name, "filtered")) {
        /* A raw event whose code is NOT a single-step, plus a real single-step: the non-single-step
         * must be counted in the completeness denominator and in other_code_exceptions, must NEVER
         * inflate raw_single_step, and -- since the routing fix -- must not be routed either. The
         * routes must reconcile with the single-step count, so the non-single-step returns seq 0. */
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_BREAKPOINT, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_GENERIC_FIRST);
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_SINGLE_STEP, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_GENERIC_FIRST);
    } else if (!strcmp(case_name, "filtered_only")) {
        /* ONLY non-single-step exceptions: the debug stream was demonstrably read (events and
         * other_code_exceptions are non-zero) while raw_single_step is a complete zero. This is the
         * live run's actual shape, and it must read NON_FIRING -- not DELIVERED_UNCLAIMED, and not
         * UNKNOWN. */
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_BREAKPOINT, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_GENERIC_FIRST);
        jsrf_a2h_test_raw_event();
        seq = jsrf_a2h_test_raw_single_step(tid, 0xE0424750u, 1, dr_canonical);
        jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_GENERIC_FIRST);
    } else if (!strcmp(case_name, "publication_failure")) {
        /* PUBLICATION FAILURE: nothing was counted at all, so the completeness denominator is zero
         * and a raw single-step count of 0 carries NO information. This must read
         * UNKNOWN_NOT_RECORDED and must never read NON_FIRING -- that is the whole losslessness
         * requirement, and it is the failure mode that would silently manufacture a negative. */
        ;
    } else if (!strcmp(case_name, "overflow")) {
        /* PUBLICATION FAILURE by truncation: more single-steps than the bounded ROW SAMPLE can hold.
         * The overflow flag is published, so the sample is VISIBLY truncated. Note what this case
         * does NOT do any more: the uncapped counters stay exact, and the decision reads them, so
         * this is reported as an incomplete SAMPLE rather than as an undecidable LEDGER. Under the
         * old gate a sample overflow was a decision input, which is how `complete` became
         * structurally 0. */
        jsrf_a2h_test_raw_event();
        for (i = 0; i < A2H_RAW_SS_CAPACITY + 8; i++) {
            seq = jsrf_a2h_test_raw_single_step(tid, EXCEPTION_SINGLE_STEP, 1, dr_canonical);
            jsrf_a2h_test_raw_ss_route(seq, A2H_ROUTE_HANDLER);
        }
    } else if (!strcmp(case_name, "unsuspended")) {
        /* THE VOID CASE. A snapshot read is attempted with suspended=0 -- the exact defect being
         * repaired, where GetThreadContext measured a RUNNING thread. The read must be REFUSED and
         * COUNTED, and the resulting snapshot must be void, so the decision can never be
         * NON_FIRING on evidence from a running thread. */
        jsrf_a2h_test_raw_event();
        jsrf_a2h_test_term_read(tid, 0, &verdict);
    } else if (!strcmp(case_name, "premature_disarm")) {
        /* The snapshot ran AFTER the disarm. The reading describes a cleared process, so it is
         * UNKNOWN and never evidence -- the packet's premature-disarm clause. */
        jsrf_a2h_test_disarm_stamp();
        jsrf_a2h_test_raw_event();
    } else if (!strcmp(case_name, "store_absent")) {
        /* NO STORE EVIDENCE: the debug stream was read and no single-step arrived, but the toolkit's
         * install witness never published. There is no write for a watch to miss, so a zero cannot
         * be read as NON_FIRING. This clause is why `absent` and `store_absent` are different
         * cases. */
        jsrf_a2h_test_raw_event();
        jsrf_a2h_test_raw_event();
    } else if (!strcmp(case_name, "unsuspended_live")) {
        /* The VOID rule driven through the PRODUCTION snapshot path on this live process: the
         * snapshot runs, but the unsuspended-read counter is forced non-zero first so the decision's
         * void clause is exercised end to end. */
        jsrf_a2h_test_raw_event();
        jsrf_a2h_test_term_read(tid, 0, &verdict);
    } else {
        fprintf(report, "A2H_DELIVERY_FIXTURE unknown_case=%s\n", case_name);
        fflush(report);
        a2h_fixture_release_worker();
        return 2;
    }

    /* The continuation record, exactly as the debug loop performs it after ContinueDebugEvent --
     * with NO context read, which is the whole point of C1(3). */
    jsrf_a2h_test_continue(tid, DBG_CONTINUE, 1);
    /* THE TERMINAL SNAPSHOT, before any disarm. It suspends each target thread once, reads its DR
     * state while suspended, compares against the armed set, and resumes. */
    if (!strcmp(case_name, "store_absent")) {
        /* This case needs the snapshot to NOT count as evidence, so it is deliberately not run:
         * snapshot_evidence=0 makes `complete` 0 for a second, independent reason, and the expected
         * decision is UNKNOWN either way. */
        ;
    } else {
        jsrf_a2h_test_terminal_snapshot();
    }
    jsrf_a2h_test_delivery_terminal();
    fprintf(report, "A2H_DELIVERY_FIXTURE case=%s armed=%d canonical=%016llX\n",
            case_name, armed, (unsigned long long)dr_canonical);
    fflush(report);

    /* Leave no residue: the fixture programmed real debug registers on a real thread. */
    a2h_fixture_release_worker();
    return 0;
}

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
    /* NATIVE #DB INGRESS FIXTURE, CHILD HALF. It must run BEFORE the argc check and before any
     * collector setup: the child is debugged by a parent that is already waiting for its handshake,
     * and it is launched with only this one token, so it never satisfies the collector's argv
     * contract.
     *
     * ONLY argv[1] IS TESTED, and that is load-bearing. The child token is passed to the debugger as
     * the CHILD'S command-line argument, so it also appears in the debugger's own argv -- scanning
     * every argument made the debugger take the child path and never create a report at all. The
     * child is launched with the token first and nothing else, so argv[1] identifies it exactly. */
    if (argc > 1 && !strcmp(argv[1], "--a2h-native-db-child")) return a2h_native_db_child();
    if (argc < 4) { fprintf(stderr, "usage: jsrf_collect seconds output-dir executable [game-arg ...]\n"); return 2; }
    out_dir = argv[2];
    snprintf(path, sizeof(path), "%s\\stacks.txt", out_dir);
    report = fopen(path, "w");
    if (!report) return 2;
    /* C1/C3 delivery fixture. Reached ONLY by an explicit token that scripts/run-jsrf.py never
     * passes, and it returns before CreateProcess, so the collector's real behaviour is unchanged.
     * Each case gets its own process because the install-hit latch is write-once. */
    for (int arg = 1; arg < argc; ++arg) {
        if (!strncmp(argv[arg], "--a2h-delivery-fixture=", 23)) {
            int code = a2h_delivery_fixture(argv[arg] + 23);
            fclose(report);
            return code;
        }
    }
    /* NATIVE #DB INGRESS FIXTURE, DEBUGGER HALF. Detected from the CHILD's command line, because that
     * is where the token actually is: the debugger's own argv carries it only as the child's
     * argument. This needs no second token and cannot be reached by scripts/run-jsrf.py, which never
     * passes --a2h-native-db-child. */
    for (int arg = 4; arg < argc; ++arg)
        if (!strcmp(argv[arg], "--a2h-native-db-child")) a2h_native_db_fixture = 1;
    /* PIN THE GEOMETRY FOR THIS FIXTURE, and this is load-bearing rather than cosmetic. This
     * fixture's debuggee is the collector itself, which has no `g_xbox_mem_offset` symbol, so
     * capture()'s resolve_geometry() would recompute dr_canonical as 0 -- silently zeroing the
     * address the handshake established and making the post-loop guards skip the terminal snapshot,
     * the disarm AND every terminal record. That is exactly the absent-record failure class this
     * packet exists to close, so the fixture pins the geometry the same way the ctest arming fixture
     * does. Production never sets this flag and its resolution is unchanged. */
    if (a2h_native_db_fixture) dr_geometry_pinned = 1;
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
        BOOL continue_ok;
        if (!break_deadline && GetTickCount64() >= deadline) {
            if (!DebugBreakProcess(process)) break;
            break_deadline = GetTickCount64() + 5000;
        } else if (break_deadline && GetTickCount64() >= break_deadline) break;
        if (!WaitForDebugEvent(&event, 100)) {
            if (GetLastError() != ERROR_SEM_TIMEOUT) break;
            continue;
        }
        /* C1 completeness denominator: EVERY debug event of ANY kind is counted here, before any
         * dispatch below. Without it a raw single-step count of 0 could not be told apart from a
         * debug stream that was never read. */
        if (dr_on()) dr_raw_event_total++;
        /* NATIVE #DB FIXTURE: expose this event's parameters to the handshake, which uses them to
         * point the watch at the child's real target. Cleared immediately after, so no other branch
         * can observe a stale pointer. */
        a2h_native_db_params = (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
                               ? &event.u.Exception : NULL;
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
        } else if (event.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) {
            /* A NEW THREAD MUST BE ARMED WHILE IT IS STOPPED. This event is delivered before the
             * thread executes its first instruction, so arming here is the only point at which the
             * gated watch can be complete for a thread's whole life. Arming after ContinueDebugEvent
             * would leave a window in which the new thread could perform a watched write unarmed --
             * which is exactly the UNKNOWN/coverage failure the packet refuses to accept.
             *
             * THE SLOT-WRITE CENSUS COUNTS HERE TOO, on its own counter and NOT the DR instrument's:
             * the page protection is process-wide, so a new thread's stores are covered the instant
             * it is created, but the THREAD has to be named for the census to be complete. */
            a2h_slotw_birth_event(event.dwThreadId);
            dr_on_create_thread(event.u.CreateThread.hThread, event.dwThreadId);
            if (event.u.CreateThread.hThread) CloseHandle(event.u.CreateThread.hThread);
        } else if (event.dwDebugEventCode == EXIT_THREAD_DEBUG_EVENT) {
            a2h_slotw_exit_event(event.dwThreadId);
            dr_on_exit_thread(event.dwThreadId);
        } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            if (event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
        } else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            EXCEPTION_DEBUG_INFO *exception = &event.u.Exception;
            DWORD code = exception->ExceptionRecord.ExceptionCode;
            unsigned raw_seq = 0;
            fprintf(report, "DEBUG_EXCEPTION tid=%lu code=%08lX first=%lu\n",event.dwThreadId,code,exception->dwFirstChance);
            /* C1: COUNTED HERE, AT THE TOP OF THE BRANCH, BEFORE ANY FILTERING, GATING OR ROUTING.
             * This is the one measurement that makes NON-FIRING decidable: an event counted here and
             * dropped below is visibly a ROUTING outcome, whereas a count taken after the filter
             * could not tell a dropped event from one that never arrived. */
            if (dr_on()) {
                dr_raw_exception_total++;
                raw_seq = a2h_raw_single_step(event.dwThreadId, code, exception->dwFirstChance,
                                              (DWORD64)(uintptr_t)exception->ExceptionRecord.ExceptionAddress);
            }
            if (code == EXCEPTION_BREAKPOINT && initial_break) initial_break = 0;
            else if (code == EXCEPTION_BREAKPOINT && break_deadline) {
                outcome = "diagnostic_deadline";
                if (dr_on()) a2h_terminal_snapshot();   /* C2: terminal, before the dump and disarm */
                dump_ok = capture(0, NULL);
                exit_code = 3;
                finished = 1;
            } else if (code == EXCEPTION_SINGLE_STEP && dr_on()) {
                /* The handler decides ownership from DR6, not from the chance: pure B0 is ours,
                 * BS alone or a mixed status is somebody else's and is passed on. */
                a2h_raw_ss_route(raw_seq, A2H_ROUTE_HANDLER);
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
                if (dr_on()) a2h_raw_ss_route(raw_seq, A2H_ROUTE_TERMINAL_SECOND);
                outcome = "unhandled_exception";
                exit_code = code;
                fprintf(report, "UNHANDLED tid=%lu exception=%08lX address=%p\n", event.dwThreadId,
                        code, exception->ExceptionRecord.ExceptionAddress);
                /* C2: THE TERMINAL SNAPSHOT, TAKEN HERE, AT THE FATAL EXCEPTION AND BEFORE THE
                 * DUMP AND THE DISARM. This is the packet's ordering: at/after the fatal exception,
                 * before disarm/teardown, while the target still exists. It is taken BEFORE
                 * capture() because the debuggee is alive and stopped right now, whereas the
                 * post-loop position ran after CloseHandle(job) had already killed it -- which is
                 * measured: suspend_failed=5 against open_ok=5, every read UNREADABLE. A snapshot
                 * taken too late is not a snapshot of an armed process. */
                if (dr_on()) a2h_terminal_snapshot();
                dump_ok = capture(event.dwThreadId, exception);
                finished = 1;
            } else {
                /* THE SWALLOW BOUNDARY, made visible: a raw event that reached here was delivered to
                 * the debugger and then consumed by a first-chance path that is not this
                 * instrument's. A single-step landing here favors debugger swallow over a watch that
                 * cannot fire. */
                if (dr_on()) a2h_raw_ss_route(raw_seq, A2H_ROUTE_GENERIC_FIRST);
                continuation = DBG_EXCEPTION_NOT_HANDLED;
            }
        } else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            exit_code = event.u.ExitProcess.dwExitCode;
            outcome = "normal_exit";
            finished = 1;
        }
        if (finished && strcmp(outcome, "normal_exit")) TerminateProcess(process, exit_code);
        continue_ok = ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continuation);
        /* C1(3): THE CONTINUATION RECORD. This replaces the deleted far-side readback, which fired
         * here on EVERY continuation and read a RUNNING thread's context. Nothing is read now: the
         * counters are uncapped, so 14 414 continuations are 14 414 counted continuations rather
         * than an overflow flag, and they reconcile against the ledger instead of poisoning it. */
        if (dr_on()) a2h_continue_record(continuation, continue_ok != 0);
        a2h_native_db_params = NULL;
    }
    CloseHandle(job); /* also kills the child on collector failure */
    /* C2 FALLBACK ONLY. The snapshot is normally taken at the terminal point inside the debug loop,
     * while the target is alive. This call covers exit paths that never reached that point (a normal
     * exit, or a loop that broke on an error), and a2h_terminal_snapshot() runs AT MOST ONCE, so it
     * can never double-count. It still runs BEFORE the disarm: disarm zeroes every thread's DR7, so a
     * snapshot taken after it would observe a disarmed process and could not distinguish an armed
     * watch from a cleared one -- the packet makes a premature disarm UNKNOWN for exactly this
     * reason. */
    if (dr_on()) a2h_terminal_snapshot();
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
