#ifndef JSRF_DIAGNOSTICS_H
#define JSRF_DIAGNOSTICS_H
#include <stdint.h>
#define JSRF_THREAD_CAPACITY 128
#define JSRF_EVENT_CAPACITY 128
#define JSRF_REGISTER_COUNT 10
enum { JSRF_CALL=1, JSRF_TAIL, JSRF_THREAD_START, JSRF_THREAD_EXIT,
       JSRF_LOCK_WAIT, JSRF_LOCK_ACQUIRED, JSRF_LOCK_RELEASE,
       JSRF_WAIT_BEGIN, JSRF_WAIT_END, JSRF_SIGNAL, JSRF_RESET };
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
} JsrfRegistry;
extern JsrfRegistry g_jsrf_debug;
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high);
void recomp_diag_thread_end(void);
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site, uint32_t value);
int jsrf_run_probe(const char *mode);
#endif
