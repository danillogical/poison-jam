/* Bounded concurrency contract for the unrecovered 0x00196C0B service loop.
 *
 * The loop below deliberately mirrors only the original instructions whose
 * behavior is established: read PGRAPH status 0x400700, snapshot service bits
 * at 0x100, call 0x00194210 for 0x1000 and 0x00193D90 for 0x01000000, then
 * reread 0x400700.  The callbacks are counted test seams.  They do not make
 * either dependency, 0x00197AAC, or the production loop recoverable.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define SERVICE_194210 0x00001000u
#define SERVICE_193D90 0x01000000u

typedef struct ServiceFixture ServiceFixture;
typedef void (*ServiceCallback)(ServiceFixture *fixture);

struct ServiceFixture {
    volatile LONG pending_400700;
    volatile LONG service_100;
    volatile LONG calls_194210;
    volatile LONG calls_193D90;
    volatile LONG iterations;
    volatile LONG completed;
    volatile LONG clear_in_callback;
    volatile LONG reentry_mode;
    volatile LONG reentry_safe;
    volatile LONG reentry_blocked;
    HANDLE entered;
    ServiceCallback callback_194210;
    ServiceCallback callback_193D90;
};

static unsigned checks;

static int check(int condition, const char *name)
{
    ++checks;
    if (!condition) fprintf(stderr, "FAIL: %s\n", name);
    return condition;
}

static LONG load_register(volatile LONG *value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

static void service_loop_00196C0B(ServiceFixture *fixture);

static void callback_00194210(ServiceFixture *fixture)
{
    InterlockedIncrement(&fixture->calls_194210);

    /* 0x00194210 can reach 0x00197AAC, which calls 0x00196C0B again.  A
     * same-context nested entry is safe only after the pending word is clear.
     * Keep both sides of that precondition executable without recursing into
     * an intentionally infinite production-shaped loop. */
    if (load_register(&fixture->reentry_mode) == 1) {
        InterlockedExchange(&fixture->pending_400700, 0);
        service_loop_00196C0B(fixture);
        InterlockedIncrement(&fixture->reentry_safe);
    } else if (load_register(&fixture->reentry_mode) == 2) {
        if (load_register(&fixture->pending_400700) != 0)
            InterlockedIncrement(&fixture->reentry_blocked);
        InterlockedExchange(&fixture->pending_400700, 0);
    } else if (load_register(&fixture->clear_in_callback)) {
        InterlockedExchange(&fixture->pending_400700, 0);
    }
}

static void callback_00193D90(ServiceFixture *fixture)
{
    InterlockedIncrement(&fixture->calls_193D90);
    if (load_register(&fixture->clear_in_callback))
        InterlockedExchange(&fixture->pending_400700, 0);
}

static void service_loop_00196C0B(ServiceFixture *fixture)
{
    if (load_register(&fixture->pending_400700) != 0) {
        do {
            uint32_t services;
            InterlockedIncrement(&fixture->iterations);
            if (fixture->entered) SetEvent(fixture->entered);
            services = (uint32_t)load_register(&fixture->service_100);
            if (services & SERVICE_194210)
                fixture->callback_194210(fixture);
            if (services & SERVICE_193D90)
                fixture->callback_193D90(fixture);
        } while (load_register(&fixture->pending_400700) != 0);
    }
}

static DWORD WINAPI service_worker(void *argument)
{
    ServiceFixture *fixture = argument;
    service_loop_00196C0B(fixture);
    InterlockedExchange(&fixture->completed, 1);
    return 0;
}

static void initialize_fixture(ServiceFixture *fixture, LONG pending, LONG services)
{
    ZeroMemory(fixture, sizeof(*fixture));
    fixture->pending_400700 = pending;
    fixture->service_100 = services;
    fixture->callback_194210 = callback_00194210;
    fixture->callback_193D90 = callback_00193D90;
}

static int join_and_close_worker(ServiceFixture *fixture, HANDLE thread,
                                 const char *bounded_name)
{
    DWORD result;
    int ok = 1;

    /* pending_400700 is the exact loop's cooperative stop/clear state.  Clear
     * it on every cleanup path, then keep the stack fixture alive until the
     * native worker has definitely exited. */
    InterlockedExchange(&fixture->pending_400700, 0);
    result = WaitForSingleObject(thread, 1000);
    ok &= check(result == WAIT_OBJECT_0, bounded_name);
    if (result != WAIT_OBJECT_0) {
        /* Returning would release the stack-backed fixture under a live
         * worker, while an infinite join would make direct runs unbounded.
         * Fail the test process after the cooperative deadline; Windows then
         * tears down the test-only worker and its handles together. */
        fprintf(stderr, "FAIL worker did not exit after cooperative deadline\n");
        fflush(stderr);
        ExitProcess(3);
    }
    CloseHandle(thread);
    return ok;
}

static int run_zero_and_service_bit_cases(void)
{
    int ok = 1;
    ServiceFixture fixture;

    initialize_fixture(&fixture, 0, SERVICE_194210 | SERVICE_193D90);
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.iterations == 0, "initial zero completes without polling");
    ok &= check(fixture.calls_194210 == 0, "initial zero skips 194210");
    ok &= check(fixture.calls_193D90 == 0, "initial zero skips 193D90");

    initialize_fixture(&fixture, 1, SERVICE_194210);
    fixture.clear_in_callback = 1;
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.calls_194210 == 1, "0x1000 calls only 194210 once");
    ok &= check(fixture.calls_193D90 == 0, "0x1000 skips 193D90");
    ok &= check(fixture.pending_400700 == 0, "0x1000 completes only after clear");

    initialize_fixture(&fixture, 1, SERVICE_193D90);
    fixture.clear_in_callback = 1;
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.calls_194210 == 0, "0x01000000 skips 194210");
    ok &= check(fixture.calls_193D90 == 1, "0x01000000 calls only 193D90 once");
    ok &= check(fixture.pending_400700 == 0, "0x01000000 completes only after clear");

    initialize_fixture(&fixture, 1, SERVICE_194210 | SERVICE_193D90);
    fixture.clear_in_callback = 1;
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.calls_194210 == 1, "combined bits call 194210 once");
    ok &= check(fixture.calls_193D90 == 1, "combined snapshot still calls 193D90 once");
    ok &= check(fixture.iterations == 1, "combined bits use one status snapshot");
    return ok;
}

static int run_native_clear_and_stall_cases(void)
{
    int ok = 1;
    ServiceFixture fixture;
    HANDLE thread;

    initialize_fixture(&fixture, 1, 0);
    fixture.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.entered != NULL, "native-clear event created");
    if (!fixture.entered) return 0;
    thread = CreateThread(NULL, 0, service_worker, &fixture, 0, NULL);
    ok &= check(thread != NULL, "native-clear worker created");
    if (!thread) {
        CloseHandle(fixture.entered);
        fixture.entered = NULL;
        return 0;
    }
    ok &= check(WaitForSingleObject(fixture.entered, 1000) == WAIT_OBJECT_0,
                "native-clear worker observed nonzero status");
    ok &= check(WaitForSingleObject(thread, 0) == WAIT_TIMEOUT,
                "nonzero status has no false completion");
    ok &= join_and_close_worker(&fixture, thread,
                                "native clear releases polling loop");
    ok &= check(fixture.completed == 1, "native-clear completion published");
    CloseHandle(fixture.entered);
    fixture.entered = NULL;

    initialize_fixture(&fixture, 1, 0);
    fixture.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.entered != NULL, "stall event created");
    if (!fixture.entered) return 0;
    thread = CreateThread(NULL, 0, service_worker, &fixture, 0, NULL);
    ok &= check(thread != NULL, "stall worker created");
    if (!thread) {
        CloseHandle(fixture.entered);
        fixture.entered = NULL;
        return 0;
    }
    ok &= check(WaitForSingleObject(fixture.entered, 1000) == WAIT_OBJECT_0,
                "stall worker entered polling loop");
    ok &= check(WaitForSingleObject(thread, 25) == WAIT_TIMEOUT,
                "never-cleared status classified as stall");
    ok &= check(fixture.completed == 0 && fixture.pending_400700 != 0,
                "stall cannot report completion");
    ok &= join_and_close_worker(&fixture, thread,
                                "stall fixture cleanup releases exact loop");
    CloseHandle(fixture.entered);
    fixture.entered = NULL;
    return ok;
}

static int run_reentry_and_generic_event_cases(void)
{
    int ok = 1;
    ServiceFixture fixture;
    HANDLE signal;

    initialize_fixture(&fixture, 1, SERVICE_194210);
    fixture.reentry_mode = 1;
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.reentry_safe == 1 && fixture.pending_400700 == 0,
                "same-context reentry is bounded after pending clear");

    initialize_fixture(&fixture, 1, SERVICE_194210);
    fixture.reentry_mode = 2;
    service_loop_00196C0B(&fixture);
    ok &= check(fixture.reentry_blocked == 1,
                "same-context reentry before clear classified unsafe");
    ok &= check(fixture.calls_194210 == 1,
                "unsafe reentry seam does not recurse or claim service");

    /* Generic future event-contract seam only.  Win32 auto-reset behavior does
     * not establish the original-XBE event type, signal source, or 0x00193D90
     * missed-signal behavior, and is outside acceptance for that function. */
    signal = CreateEventA(NULL, FALSE, FALSE, NULL);
    ok &= check(signal != NULL, "generic event seam created");
    if (!signal) return 0;
    SetEvent(signal);
    ok &= check(WaitForSingleObject(signal, 25) == WAIT_OBJECT_0,
                "generic pre-wait auto-reset signal is observable");
    ok &= check(WaitForSingleObject(signal, 25) == WAIT_TIMEOUT,
                "generic consumed signal does not complete twice");
    SetEvent(signal);
    ResetEvent(signal);
    ok &= check(WaitForSingleObject(signal, 25) == WAIT_TIMEOUT,
                "generic reset-before-wait reaches deadline");
    CloseHandle(signal);
    return ok;
}

int main(void)
{
    int ok = 1;
    for (unsigned repetition = 0; repetition < 64; ++repetition) {
        ok &= run_zero_and_service_bit_cases();
        ok &= run_native_clear_and_stall_cases();
        ok &= run_reentry_and_generic_event_cases();
        if (!ok) break;
    }
    if (ok)
        printf("PASS: %u bounded 0x00196C0B service-chain checks\n", checks);
    return ok ? 0 : 1;
}
