#ifndef JSRF_DIAGNOSTICS_H
#define JSRF_DIAGNOSTICS_H
#include <stdint.h>
#define JSRF_THREAD_CAPACITY 128
#define JSRF_EVENT_CAPACITY 128
#define JSRF_REGISTER_COUNT 10

/* The A2h NULL-slot latch.
 *
 * WHY THIS EXISTS: the terminal event of the trapped run is a raw-zero indirect call through
 * the kernel thunk slot at guest VA 0x001C4064 -- a slot whose image content is a valid
 * ordinal-277 thunk. Deciding WHO zeroed it requires a decision input that cannot drop the
 * deciding event, which per docs/agent-workflow.md 6.1.6 means a write-once latch or an
 * uncapped counter updated AT the event, NOT a capped or sampled log. A log line can be
 * dropped by the kernel-log budget, by concurrent stderr, or by a RaiseException that
 * terminates before a flush -- and a dropped line would be exactly the deciding event.
 *
 * BOUNDED BY CONSTRUCTION: JSRF_LATCH_CAPACITY slots, keyed by live-read tid, never recycled
 * and never overwritten even after a thread exits. A thread arriving with no free slot sets
 * `overflow` and is reported UNKNOWN rather than being given a recycled or dynamic slot.
 *
 * THIS STRUCT IS ARCHIVED FOR FREE: tools/harness/collect.c resolves g_jsrf_debug BY SYMBOL and
 * reads sizeof(JsrfRegistry) into the dump, so extending JsrfRegistry extends what is archived.
 * That is why JSRF_REGISTRY_VERSION MUST be bumped whenever this layout changes: the collector
 * prints the version it read, and a reader that does not check it would silently misinterpret
 * a differently-laid-out struct.
 */
#define JSRF_REGISTRY_VERSION 3
#define JSRF_LATCH_CAPACITY 128

/* ── A2h slot-WRITE watch (version 3) ────────────────────────────────────────────────────────
 *
 * The version-2 latch samples the slot across BRIDGE BOUNDARIES. A prior run read 0xFE000104 at
 * every one of 15498 sampled boundaries across six threads and then read 0 at the terminal read,
 * so the change happened in a gap with no bracketing sample. These records exist to decide whether
 * the slot was written in that gap and whether a guest or a host thread did it.
 *
 * NOT A PER-WRITE HISTORY. Everything here is either a fixed, class-keyed, write-once witness or a
 * fixed-size census keyed by a compile-time constant (mirror index, writer class). Nothing is
 * sized by run length, and nothing is first-N: the packet's rule is that a capped log is not a
 * decision input.
 *
 * SIX WRITER CLASSES, ONE WITNESS EACH. Three provenance classes {guest, bridge/host, unknown} x
 * two transition classes {nonzero->zero, zero->nonzero}. A class's first witness is claimed by
 * CAS and is then IMMUTABLE. When the capacity is exhausted the overflow counter records it and
 * the class is reported UNKNOWN -- never a recycled slot.
 */
#define JSRF_WRITE_CLASS_CAPACITY 6
#define JSRF_ALIAS_CAPACITY 28      /* XBOX_NUM_MIRRORS; a source constant, not a run length */

/* Provenance of an observed write, as classified by the native #DB handler's RIP and by the
 * toolkit's own knowledge of which code it is. UNKNOWN is a first-class answer: the packet is
 * explicit that a native RIP alone does not identify a guest instruction. */
enum { JSRF_PROV_GUEST = 0, JSRF_PROV_HOST = 1, JSRF_PROV_UNKNOWN = 2 };
enum { JSRF_TRANS_TO_ZERO = 0, JSRF_TRANS_RESTORE = 1 };
#define JSRF_WRITE_CLASS(prov, trans) ((uint32_t)((prov) * 2 + (trans)))

typedef struct {
    uint32_t valid;       /* 1 once this class's first witness is complete and immutable */
    uint32_t class_id;    /* JSRF_WRITE_CLASS(provenance, transition) */
    uint32_t tid;         /* native tid that observed the write */
    uint32_t call_index;  /* that thread's dispatch # at the write */
    uint32_t ordinal;     /* bridge ordinal if the write was intra-bridge, else 0 */
    uint32_t before;      /* slot value before the write */
    uint32_t after;       /* slot value after the write */
    uint32_t reserved;
    uint64_t ticks;       /* QPC at the write, for cross-thread ordering */
    uint64_t rip;         /* native instruction pointer that performed the store */
} JsrfWriteClass;

/* One first-touch witness per mirror index. RECORD-BEFORE-OPEN: the toolkit publishes this record
 * BEFORE it calls VirtualProtect to open the page, so a concurrent writer escaping a temporary
 * read-write window cannot make a zero-touch conclusion false. `published` is the publication's
 * own outcome -- a failed publication is a coverage failure, never a silent absence. */
typedef struct {
    uint32_t valid;        /* 1 once the payload below is complete */
    uint32_t alias_index;  /* 0 = canonical-adjacent mirror 1; index m means mirror m+1 */
    uint32_t mapped;       /* 1 if that mirror view was mapped when the watch armed */
    uint32_t published;    /* 1 = publication succeeded before any VirtualProtect open */
    uint32_t fault_va;     /* the faulting host address inside the protected page */
    uint32_t value;        /* live slot value observed at the fault */
    uint64_t rip;          /* native instruction pointer that took the write fault */
    uint64_t ticks;        /* QPC at the fault */
} JsrfAliasTouch;

typedef struct {
    uint32_t armed;            /* 1 once the alias census armed successfully */
    uint32_t alias_count;      /* mapped mirror views found at arm time */
    uint32_t mapped_mask;      /* bit m = mirror m+1 present at arm time */
    uint32_t protect_mask;     /* bit m = mirror m+1 successfully RO-protected */
    uint32_t touched_count;    /* first-touch records published */
    uint32_t publish_failed;   /* 1 = a touch could not be published before opening (coverage) */
    uint32_t handshake_seen;   /* 1 once the collector handshake was raised */
    uint32_t class_overflow;   /* 1 = a class witness was lost to capacity (UNKNOWN) */
    uint32_t terminal_seen;    /* 1 once the terminal witness ran */
    uint32_t terminal_target;  /* the raw target the generated code passed to the fatal hook */
    uint32_t terminal_slot;    /* independent re-read of the LIVE slot at that moment */
    uint32_t terminal_flags;   /* bit0 offset stable, bit1 alias census armed, bit2 publish ok */
    uint32_t terminal_offset_lo;  /* low 32 bits of the re-read g_xbox_mem_offset */
    uint32_t terminal_offset_hi;  /* high 32 bits; a 64-bit offset must not be truncated */
    uint32_t arm_offset_lo;    /* g_xbox_mem_offset as read when the census armed */
    uint32_t arm_offset_hi;    /* so terminal stability is a comparison, not an assumption */
    uint32_t class_claimed;    /* class witnesses claimed so far */
    uint32_t reserved;
    uint64_t handshake_ticks;
    uint64_t terminal_ticks;
    uint64_t class_counts[JSRF_WRITE_CLASS_CAPACITY];  /* uncapped per-class write counter */
    JsrfWriteClass classes[JSRF_WRITE_CLASS_CAPACITY];
    JsrfAliasTouch aliases[JSRF_ALIAS_CAPACITY];
} JsrfSlotWriteWatch;

/* The registry's own size, as the Python extractor computes it from this header's field list.
 * A compile-time mismatch is a build error rather than a silently misread archive: the extractor
 * reads by computed offset, so a drift here would misattribute every field after it. */
#define JSRF_REGISTRY_SIZE_EXPECTED 545344u

/* Event kinds. PRESERVED VERBATIM: these are referenced by src/diagnostics.c and their NUMERIC
 * VALUES ARE PART OF THE ARCHIVED FORMAT -- the collector's own kind-name table in
 * tools/harness/collect.c:210 indexes by position, so reordering or renumbering this enum would
 * silently relabel every archived event. Do not touch it when extending the registry. */
enum { JSRF_CALL=1, JSRF_TAIL, JSRF_THREAD_START, JSRF_THREAD_EXIT,
       JSRF_LOCK_WAIT, JSRF_LOCK_ACQUIRED, JSRF_LOCK_RELEASE,
       JSRF_WAIT_BEGIN, JSRF_WAIT_END, JSRF_SIGNAL, JSRF_RESET };

typedef struct {
    uint32_t valid;       /* 1 once the payload below is complete and immutable */
    uint32_t tid;         /* live-read thread id that owns this slot */
    uint32_t call_index;  /* that thread's dispatch # at the transition */
    uint32_t ordinal;     /* bridge ordinal if the transition was intra-bridge, else 0 */
    uint32_t before;      /* slot value before the transition */
    uint32_t after;       /* slot value after the transition (0 for a first-zero record) */
    uint32_t intra;       /* 1 = transition observed across a bridge call, 0 = between calls */
    uint32_t reserved;
    uint64_t ticks;       /* QPC at the transition, for cross-thread ordering */
} JsrfSlotTransition;

typedef struct {
    uint32_t install_seen;   /* 1 once the install sample was taken */
    uint32_t install_raw;    /* image value before the toolkit patched the slot */
    uint32_t install_value;  /* value the toolkit installed */
    uint32_t install_ok;     /* 1 if the pair matched the predicted control */
    uint32_t claimed;        /* latch slots claimed so far */
    uint32_t overflow;       /* 1 = a thread arrived with no free slot (UNKNOWN, not recycled) */
    uint32_t partial;        /* 1 = a torn publication was observed */
    uint32_t sequence;       /* odd while a slot is being published */
    JsrfSlotTransition slots[JSRF_LATCH_CAPACITY];
} JsrfSlotLatch;

typedef struct {
    uint32_t sequence, kind;
    uint64_t ticks;
    uint32_t target, site, esp, value;
} JsrfEvent;
typedef struct {
    uint32_t state, tid, identity, start;
    uint32_t stack_low, stack_high, count, reserved;
    uint64_t registers[JSRF_REGISTER_COUNT];
    JsrfEvent events[JSRF_EVENT_CAPACITY];
} JsrfThread;
typedef struct {
    uint32_t version, claimed, overflow, reserved;
    uint64_t frequency;
    JsrfThread threads[JSRF_THREAD_CAPACITY];
    JsrfSlotLatch latch;   /* appended; bump JSRF_REGISTRY_VERSION when this layout changes */
    JsrfSlotWriteWatch watch;   /* v3: the slot-WRITE watch; append-only, never reordered */
} JsrfRegistry;
extern JsrfRegistry g_jsrf_debug;
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high);
void recomp_diag_thread_end(void);
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site, uint32_t value);
int jsrf_run_probe(const char *mode);

/* A2h NULL-slot latch API.
 *
 * The toolkit calls the first two; the game calls the last two from its own terminal path.
 * All are observation-only: no guest register, memory, allocation result, cleanup or device
 * state is read-modify-written, and the terminal RaiseException is untouched. */
void jsrf_slot_latch_install(uint32_t raw_value, uint32_t installed_value);
void jsrf_slot_latch_sample(uint32_t tid, uint32_t call_index, uint32_t ordinal,
                            uint32_t before, uint32_t after);
/* Latest per-thread dispatch index this thread's latch slot has recorded (0 if none). */
uint32_t jsrf_slot_latch_self_call_index(void);
/* 1 if this thread owns a latch slot, so a 0 from the above is a real 0 and not "unknown". */
uint32_t jsrf_slot_latch_self_observed(void);

/* A2h slot-WRITE watch API (version 3).
 *
 * The toolkit calls the first four; the game calls the terminal one from its own fatal path. All
 * are observation-only: no guest register, memory, allocation result, stack cleanup or device
 * state is read-modify-written, and RaiseException is untouched. */
void jsrf_slot_watch_handshake(uint32_t slot_va);
void jsrf_slot_watch_alias_armed(uint32_t mapped_mask, uint32_t protect_mask, uint32_t alias_count);
/* Returns 1 if the touch is now recorded. The toolkit opens the alias page ONLY on a 1:
 * RECORD-BEFORE-OPEN is mandatory, because a page opened without a published record could absorb a
 * concurrent write that is then neither recorded nor the first touch. */
int jsrf_slot_watch_alias_touch(uint32_t alias_index, uint32_t fault_va, uint64_t rip,
                                uint32_t value, uint32_t published);
void jsrf_slot_watch_write(uint32_t provenance, uint32_t before, uint32_t after,
                           uint64_t rip, uint32_t ordinal);
/* Terminal witness: `target` is the raw VA the generated code actually passed in. The live slot
 * and g_xbox_mem_offset are re-read INDEPENDENTLY here, at this moment, rather than inferred. */
void jsrf_slot_watch_terminal(uint32_t target);
#endif
