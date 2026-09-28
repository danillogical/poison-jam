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
#define JSRF_REGISTRY_VERSION 2
#define JSRF_LATCH_CAPACITY 128

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
#endif
