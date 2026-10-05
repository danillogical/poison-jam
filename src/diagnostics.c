#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "recomp/gen/recomp_types.h"
#include "diagnostics.h"
#include "jsrf_fatal_observe.h"

/* Slots are never recycled. Thus an exited TLS address cannot be mistaken for
 * a new thread. Only the owning thread writes a slot. External readers MUST
 * freeze the process at a debug event; no in-process live snapshot is allowed.
 * Odd sequence numbers mark events interrupted midway through publication. */
JsrfRegistry g_jsrf_debug = {JSRF_REGISTRY_VERSION};

/* The archived format is read by COMPUTED OFFSET (scripts/a2h-read-registry.py), so a silent drift
 * between this struct's real size and the size that reader assumes would misattribute every field
 * after the drift -- it would not fail, it would lie. This turns that drift into a build error.
 * It is a typedef rather than _Static_assert so it compiles under every standard this target uses. */
typedef char jsrf_registry_size_check[
    (sizeof(JsrfRegistry) == JSRF_REGISTRY_SIZE_EXPECTED) ? 1 : -1];

static RECOMP_TLS JsrfThread *current;

/* ── A2h NULL-slot latch ───────────────────────────────────────────────────────
 *
 * Deciding what zeroed the kernel thunk slot at 0x001C4064 requires a decision input that
 * cannot drop the deciding event (docs/agent-workflow.md 6.1.6). A log line CAN be dropped --
 * by the kernel-log budget, by concurrent stderr, or by a terminal RaiseException that runs
 * before a flush -- so the authoritative record is this write-once latch, and the log lines are
 * corroboration only.
 *
 * PER-THREAD, NOT GLOBAL. The toolkit's dispatch counter is `RECOMP_TLS` (kernel_bridge.c:316),
 * so each thread has its own call sequence and its own first transition. A single global record
 * would be claimed by whichever thread arrived first, which under races need not be the thread
 * whose transition matters. So one slot per thread, keyed by the live-read tid, claimed once.
 *
 * NEVER RECYCLED. A slot is claimed by the first thread that observes a transition and is never
 * reused, even after that thread exits -- recycling is how an exited thread's record gets
 * mistaken for a live one. When the fixed capacity is exhausted the overflow flag is set and the
 * event is reported UNKNOWN rather than given a recycled or dynamically allocated slot.
 *
 * ONE SLOT PER THREAD: `self_slot` is TLS, so a thread finds its own slot without searching and
 * without a lock. Publication is a release-store of `valid` after the payload is written. */
static RECOMP_TLS JsrfSlotTransition *self_slot;
static RECOMP_TLS uint32_t self_call_index;

void jsrf_slot_latch_install(uint32_t raw_value, uint32_t installed_value)
{
    /* The install sample is a POSITIVE CONTROL, not an observation: the image holds the ordinal
     * marker 0x80000115 at slot 65 and the toolkit's patch loop rewrites it to the synthetic
     * dispatch VA (KERNEL_VA_BASE + index*4). If the pair does not match that prediction, the
     * control FAILED and every downstream attribution is unsafe -- so `install_ok` records the
     * comparison rather than assuming it. */
    uint32_t predicted = 0xFE000000u + 65u * 4u;   /* slot 65 == (0x001C4064-0x001C3F60)/4 */
    if (g_jsrf_debug.latch.install_seen) return;
    g_jsrf_debug.latch.install_raw = raw_value;
    g_jsrf_debug.latch.install_value = installed_value;
    g_jsrf_debug.latch.install_ok =
        (raw_value == 0x80000115u && installed_value == predicted) ? 1u : 0u;
    InterlockedExchange((volatile LONG *)&g_jsrf_debug.latch.install_seen, 1);
}

void jsrf_slot_latch_sample(uint32_t tid, uint32_t call_index, uint32_t ordinal,
                            uint32_t before, uint32_t after)
{
    LONG slot;
    LARGE_INTEGER now;
    JsrfSlotTransition *s;

    self_call_index = call_index;   /* remembered so the terminal path can report it */

    /* Only a genuine nonzero->zero transition is a first-zero record. Anything else is the
     * caller's business to filter, but this guard makes the latch's meaning unambiguous. */
    if (before == 0 || after != 0) return;
    if (self_slot) return;          /* write-once per thread; a second transition is not the first */

    slot = InterlockedIncrement((volatile LONG *)&g_jsrf_debug.latch.claimed) - 1;
    if (slot >= JSRF_LATCH_CAPACITY) {
        /* FAIL CLOSED: no free slot means UNKNOWN, never a recycled slot. */
        InterlockedIncrement((volatile LONG *)&g_jsrf_debug.latch.overflow);
        return;
    }
    s = &g_jsrf_debug.latch.slots[slot];
    InterlockedExchange((volatile LONG *)&g_jsrf_debug.latch.sequence, 1);  /* odd = publishing */
    QueryPerformanceCounter(&now);
    s->tid = tid;
    s->call_index = call_index;
    s->ordinal = ordinal;
    s->before = before;
    s->after = after;
    s->intra = ordinal ? 1u : 0u;
    s->reserved = 0;
    s->ticks = (uint64_t)now.QuadPart;
    /* Release the payload before the reader can see it as valid. */
    InterlockedExchange((volatile LONG *)&s->valid, 1);
    self_slot = s;
    InterlockedExchange((volatile LONG *)&g_jsrf_debug.latch.sequence, 2);  /* even = settled */
}

uint32_t jsrf_slot_latch_self_call_index(void)
{
    return self_call_index;
}

uint32_t jsrf_slot_latch_self_observed(void)
{
    /* Distinguishes "this thread recorded a transition" from "this thread never sampled".
     * A bare 0 from the call index would otherwise be ambiguous. */
    return self_slot ? 1u : 0u;
}

/* ── A2h slot-WRITE watch (version 3) ────────────────────────────────────────────────────────
 *
 * Same discipline as the latch above, applied to the writes rather than the boundary samples:
 * fixed, class-keyed, write-once witnesses plus fixed-size censuses. Nothing here is sized by run
 * length, nothing is first-N, and nothing is recycled.
 *
 * WHY CLASS-KEYED RATHER THAN A HISTORY: the deciding question is not "what was every write" but
 * "was there a write at all, and what kind". Six classes cover that question exactly, and a class
 * that has already been witnessed is immutable -- so a later write cannot overwrite the first
 * witness for its class. The uncapped per-class counter preserves multiplicity without a log.
 *
 * THE TERMINAL WITNESS IS DELIBERATELY INDEPENDENT. It re-reads the LIVE slot and
 * g_xbox_mem_offset at the fatal moment rather than trusting the last sampled value, because the
 * whole point of the instrument is that the last sample and the terminal read DISAGREED.
 */
static volatile LONG g_a2h_watch_handshake = 0;

void jsrf_slot_watch_handshake(uint32_t slot_va)
{
    LARGE_INTEGER now;
    JsrfSlotWriteWatch *w = &g_jsrf_debug.watch;
    (void)slot_va;   /* recorded by the toolkit's own print; the record needs only the fact */
    if (InterlockedExchange(&g_a2h_watch_handshake, 1)) return;   /* write-once */
    QueryPerformanceCounter(&now);
    w->handshake_ticks = (uint64_t)now.QuadPart;
    InterlockedExchange((volatile LONG *)&w->handshake_seen, 1);
}

void jsrf_slot_watch_alias_armed(uint32_t mapped_mask, uint32_t protect_mask, uint32_t alias_count)
{
    JsrfSlotWriteWatch *w = &g_jsrf_debug.watch;
    extern ptrdiff_t g_xbox_mem_offset;
    uint64_t offset = (uint64_t)(uintptr_t)g_xbox_mem_offset;
    w->arm_offset_lo = (uint32_t)(offset & 0xFFFFFFFFu);
    w->arm_offset_hi = (uint32_t)(offset >> 32);
    w->mapped_mask = mapped_mask;
    w->protect_mask = protect_mask;
    w->alias_count = alias_count;
    /* FAIL CLOSED ON A PRESENT BUT UNARMED VIEW. A mapped mirror that could not be protected is a
     * hole in the census: an unwatched page can absorb a write that the census would then report
     * as absent, which is the one error this instrument must not make. So `armed` is set only when
     * every mapped view was protected, and the missing bits stay visible in protect_mask. */
    InterlockedExchange((volatile LONG *)&w->armed,
                        (mapped_mask != 0 && mapped_mask == protect_mask) ? 1 : 0);
}

int jsrf_slot_watch_alias_touch(uint32_t alias_index, uint32_t fault_va, uint64_t rip,
                                uint32_t value, uint32_t published)
{
    JsrfSlotWriteWatch *w = &g_jsrf_debug.watch;
    JsrfAliasTouch *t;
    LARGE_INTEGER now;
    /* THE RETURN VALUE IS THE PUBLICATION RESULT and the toolkit gates the page-open on it:
     * RECORD-BEFORE-OPEN means a page may be opened ONLY after this returns 1. A page opened
     * without a published record could absorb a concurrent write that is then neither recorded nor
     * the first touch -- so "could not publish" must be an explicit, recorded failure rather than
     * a silent absence that reads the same as "no touch happened". */
    if (alias_index >= JSRF_ALIAS_CAPACITY || !published) {
        InterlockedExchange((volatile LONG *)&w->publish_failed, 1);
        return 0;
    }
    t = &w->aliases[alias_index];
    if (!InterlockedExchange((volatile LONG *)&t->valid, 1)) {
        QueryPerformanceCounter(&now);
        t->alias_index = alias_index;
        t->mapped = (w->mapped_mask >> alias_index) & 1u;
        t->fault_va = fault_va;
        t->value = value;
        t->published = 1;
        t->rip = rip;
        t->ticks = (uint64_t)now.QuadPart;
        InterlockedIncrement((volatile LONG *)&w->touched_count);
    }
    /* An already-valid slot is a record that already exists, which is a successful publication for
     * the caller's purposes: the touch is recorded, so the page may be opened. */
    return 1;
}

void jsrf_slot_watch_write(uint32_t provenance, uint32_t before, uint32_t after,
                           uint64_t rip, uint32_t ordinal)
{
    JsrfSlotWriteWatch *w = &g_jsrf_debug.watch;
    uint32_t cls, trans;
    JsrfWriteClass *c;
    LARGE_INTEGER now;
    if (provenance > JSRF_PROV_UNKNOWN) provenance = JSRF_PROV_UNKNOWN;
    /* The transition class is derived from the values themselves, so a caller cannot mislabel a
     * restore as a zeroing. A before==after write is not a transition and claims nothing. */
    if (before == after) return;
    trans = (after == 0) ? JSRF_TRANS_TO_ZERO : JSRF_TRANS_RESTORE;
    cls = JSRF_WRITE_CLASS(provenance, trans);
    /* The counter is uncapped and updated at the event, so multiplicity survives without a log. */
    InterlockedIncrement64((volatile LONG64 *)&w->class_counts[cls]);
    c = &w->classes[cls];
    if (InterlockedExchange((volatile LONG *)&c->valid, 1)) return;   /* first witness is immutable */
    QueryPerformanceCounter(&now);
    c->class_id = cls;
    c->tid = GetCurrentThreadId();
    c->call_index = jsrf_slot_latch_self_call_index();
    c->ordinal = ordinal;
    c->before = before;
    c->after = after;
    c->rip = rip;
    c->ticks = (uint64_t)now.QuadPart;
    InterlockedIncrement((volatile LONG *)&w->class_claimed);
}

void jsrf_slot_watch_terminal(uint32_t target)
{
    JsrfSlotWriteWatch *w = &g_jsrf_debug.watch;
    extern ptrdiff_t g_xbox_mem_offset;
    uint64_t offset = (uint64_t)(uintptr_t)g_xbox_mem_offset;
    uint32_t flags = 0;
    LARGE_INTEGER now;
    /* INDEPENDENT RE-READS. `target` is what the generated code passed in; the slot below is read
     * again here, and the mapping offset is read again here. Neither is copied from an earlier
     * sample: the disagreement between the last boundary sample and this read is the phenomenon
     * under investigation, so reusing a sampled value would assume away the question. */
    w->terminal_target = target;
    w->terminal_slot = MEM32(0x001C4064u);
    w->terminal_offset_lo = (uint32_t)(offset & 0xFFFFFFFFu);
    w->terminal_offset_hi = (uint32_t)(offset >> 32);
    if (w->terminal_offset_lo == w->arm_offset_lo && w->terminal_offset_hi == w->arm_offset_hi)
        flags |= 1u;                       /* mapping offset stable arm -> terminal */
    if (w->armed) flags |= 2u;             /* alias census armed, every mapped view protected */
    if (!w->publish_failed) flags |= 4u;   /* no publication was lost */
    w->terminal_flags = flags;
    QueryPerformanceCounter(&now);
    w->terminal_ticks = (uint64_t)now.QuadPart;
    InterlockedExchange((volatile LONG *)&w->terminal_seen, 1);
}

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

/* Declared in recomp_types.h only under RECOMP_ABI_CHECK, which this file is
 * not always built with; defined in recomp_abi_deltas.c. */
int recomp_delta_allowed(uint32_t va, uint32_t out[4]);

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

/* Disc-error observation. Reads guest memory and prints. Writes nothing,
 * and does not change guest registers. Capped so a retry loop cannot fill
 * the log. The words are the job/dialog fields named in the current plan:
 * state +0x44/+0x48, the pending-I/O triple +0x50/+0x54/+0x64, dialog flags
 * +0x98, and the time snapshot +0x190/+0x194. +0x1604 is the field a live
 * 0x1C4F68 object was read at. */
static int fatal_ptr(uint32_t p)
{
    return p >= 0x00010000u && p < 0x04000000u - 0x1608u && (p & 3u) == 0;
}

static void fatal_dump_object(const char *tag, uint32_t p)
{
    if (!fatal_ptr(p)) {
        fprintf(stderr, "  [%s] %08X not an object\n", tag, p);
        return;
    }
    fprintf(stderr,
            "  [%s] %08X vt=%08X +44=%08X +48=%08X +50=%08X +54=%08X"
            " +64=%08X +98=%08X +190=%08X +194=%08X +1604=%08X\n",
            tag, p, MEM32(p), MEM32(p + 0x44), MEM32(p + 0x48),
            MEM32(p + 0x50), MEM32(p + 0x54), MEM32(p + 0x64),
            MEM32(p + 0x98), MEM32(p + 0x190), MEM32(p + 0x194),
            MEM32(p + 0x1604));
}

/* The 0x116EAD caller has just stored movsx(word [child+0x60]) at parent+0x60.
 * The four children are the ADX slots at parent+0x68. Log them before the
 * watchdog's own clear runs. Reads only. */
static void fatal_dump_adx_slots(uint32_t parent)
{
    uint32_t slot;
    if (!fatal_ptr(parent))
        return;
    fprintf(stderr, "  [FATAL-ADX] parent+60=%08X\n", MEM32(parent + 0x60));
    for (slot = 0; slot < 4; slot++) {
        uint32_t obj = MEM32(parent + 0x68u + slot * 4u);
        uint32_t link;
        if (!fatal_ptr(obj)) {
            fprintf(stderr, "  [FATAL-ADX] slot=%u obj=%08X not an object\n",
                    slot, obj);
            continue;
        }
        link = MEM32(obj + 4u);
        fprintf(stderr,
                "  [FATAL-ADX] slot=%u obj=%08X st=%u b6d=%u b72=%u"
                " w40=%04X w60=%04X ctr68=%04X ctr6a=%04X d38=%08X link=%08X",
                slot, obj, MEM8(obj + 1u), MEM8(obj + 0x6du), MEM8(obj + 0x72u),
                (unsigned)MEM16(obj + 0x40u), (unsigned)MEM16(obj + 0x60u),
                (unsigned)MEM16(obj + 0x68u), (unsigned)MEM16(obj + 0x6au),
                MEM32(obj + 0x38u), link);
        if (fatal_ptr(link))
            fprintf(stderr, " lst=%u d2c=%08X d30=%08X d38=%08X\n",
                    MEM8(link + 1u), MEM32(link + 0x2cu), MEM32(link + 0x30u),
                    MEM32(link + 0x38u));
        else
            fprintf(stderr, " link-unreadable\n");
    }
}

static void fatal_dump_stack(uint32_t esp)
{
    uint32_t i, shown = 0;
    fprintf(stderr, "  [FATAL-CTOR] stack");
    for (i = 0; i < 128 && shown < 48; i++) {
        uint32_t a = esp + i * 4u;
        uint32_t w;
        if (a < 0x00010000u || a >= 0x04000000u - 4u)
            break;
        w = MEM32(a);
        if (i < 8 || (w >= 0x00011000u && w < 0x001C3F60u)) {
            fprintf(stderr, " +%03X=%08X", i * 4u, w);
            shown++;
        }
    }
    fprintf(stderr, "\n");
}

void jsrf_fatal_ctor_enter(void)
{
    static volatile LONG n;
    LONG k = InterlockedIncrement(&n);
    uint32_t esp, ret;
    if (k > 16)
        return;
    esp = g_esp;
    ret = (esp >= 0x00010000u && esp < 0x04000000u - 4u) ? MEM32(esp) : 0;
    _lock_file(stderr);
    fprintf(stderr, "[FATAL-CTOR] #%ld ret=%08X %s ecx=%08X esi=%08X esp=%08X\n",
            k, ret, jsrf_fatal_ctor_ret_name(ret), g_ecx, g_esi, esp);
    fatal_dump_stack(esp);
    fatal_dump_object("ecx", g_ecx);
    fatal_dump_object("esi", g_esi);
    if (ret == 0x00116EADu)
        fatal_dump_adx_slots(g_esi);
    fflush(stderr);
    _unlock_file(stderr);
}

void jsrf_fatal_tail(uint32_t job, uint32_t caller_ret)
{
    static volatile LONG n;
    LONG k = InterlockedIncrement(&n);
    if (k > 16)
        return;
    _lock_file(stderr);
    fprintf(stderr, "[FATAL-TAIL] #%ld job=%08X caller_of_25310=%08X"
            " (tail jmp 0x2537E taken; esi still the job)\n",
            k, job, caller_ret);
    fatal_dump_object("job", job);
    fflush(stderr);
    _unlock_file(stderr);
}
