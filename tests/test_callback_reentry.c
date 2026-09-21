/* Bounded original-XBE contract for 0x00193D90 / 0x00194210 / 0x00197AAC.
 *
 * Instruction-faithful C model of the three bodies.  It records KeSetEvent
 * arguments against the in-place Type-0 dispatcher at context+0x1C8 and does
 * not treat a raw Win32 event as that object.  Production helpers stay
 * unresolved.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define PMC_PCRTC           0x01000000u
#define PGRAPH_CTX_SWITCH   0x00001000u
#define NOTIFICATION_EVENT  0u

typedef struct DispatcherHeader {
    uint8_t type;
    uint8_t absolute;
    uint8_t size;
    uint8_t inserted;
    volatile LONG signal_state;
    uint32_t flink;
    uint32_t blink;
} DispatcherHeader;

typedef struct GuestRegs {
    uint32_t eax, ecx, edx, ebx, esi, edi, ebp, esp;
    uint32_t seh_ebp;
    uint32_t irql;
} GuestRegs;

typedef struct DeviceRegs {
    volatile LONG pmc_intr;
    volatile LONG pcrtc_intr;
    volatile LONG pcrtc_byte;
    volatile LONG pgraph_intr;
    volatile LONG pgraph_nsource;
    volatile LONG pgraph_status;
    volatile LONG pgraph_trapped_addr;
    volatile LONG pgraph_trapped_data;
    volatile LONG pgraph_fifo;
    volatile LONG pgraph_ctx_control;
    volatile LONG pgraph_ctx_user;
    volatile LONG pgraph_rdi_index;
    volatile LONG pgraph_rdi_data;
    volatile LONG pgraph_debug;
    volatile LONG pgraph_channel_ctx_pointer;
    volatile LONG pgraph_channel_ctx_trigger;
} DeviceRegs;

typedef struct CallbackInfo {
    uint32_t counter_1f4;
    uint32_t index_1f0;
    uint32_t reason;
} CallbackInfo;

typedef struct Context {
    DeviceRegs *device;
    uint32_t callback;
    DispatcherHeader event_1c8;
    uint32_t slot_occupied[2];
    uint32_t slot_payload[2];
    uint32_t channel_130;
    uint32_t channel_table[8];
    uint32_t index_1f0;
    uint32_t counter_1f4;
    uint32_t snapshot_1f8;
    uint32_t threshold_1fc;
    uint32_t delta_208;
    uint32_t last_ts_20c;
    uint32_t instance_134[8];
} Context;

typedef struct Fixture {
    Context context;
    DeviceRegs device;
    GuestRegs regs;
    GuestRegs regs_before;
    volatile LONG timestamp;
    volatile LONG pcrtc_writes;
    volatile LONG keset_calls;
    volatile LONG keset_event_matches;
    volatile LONG keset_increment;
    volatile LONG keset_wait;
    volatile LONG callback_calls;
    volatile LONG callback_reason;
    volatile LONG callback_counter;
    volatile LONG callback_index;
    volatile LONG callback_esp_delta;
    volatile LONG queue_calls;
    volatile LONG queue_payload;
    volatile LONG queue_timestamp;
    volatile LONG calls_194210;
    volatile LONG calls_97aac;
    volatile LONG calls_96c0b;
    volatile LONG calls_96c4a;
    volatile LONG nested_194210;
    volatile LONG stalled;
    volatile LONG completed;
    volatile LONG reentry_depth;
    volatile LONG unsafe_reentry;
    HANDLE entered;
    HANDLE waiter;
    uint32_t spin_deadline_ms;
    uint32_t callback_mutates_pcrtc;
} Fixture;

static unsigned checks;

static int check(int condition, const char *name)
{
    ++checks;
    if (!condition) fprintf(stderr, "FAIL: %s\n", name);
    return condition;
}

static LONG load_long(volatile LONG *value)
{
    return InterlockedCompareExchange(value, 0, 0);
}

static void init_notification_event(DispatcherHeader *event)
{
    event->type = NOTIFICATION_EVENT;
    event->absolute = 0;
    event->size = 4;
    event->inserted = 0;
    event->signal_state = 1;
    event->flink = 0;
    event->blink = 0;
}

static void snapshot_regs(GuestRegs *regs)
{
    regs->eax = 0xA0A0A0A0u;
    regs->ecx = 0xC0C0C0C0u;
    regs->edx = 0xD0D0D0D0u;
    regs->ebx = 0xB0B0B0B0u;
    regs->esi = 0x51515151u;
    regs->edi = 0xD1D1D1D1u;
    regs->ebp = 0xE0E0E0E0u;
    regs->esp = 0x00F7FC24u;
    regs->seh_ebp = 0x5E5E5E5Eu;
    regs->irql = 0x11u;
}

static void initialize_fixture(Fixture *fixture)
{
    ZeroMemory(fixture, sizeof(*fixture));
    fixture->context.device = &fixture->device;
    init_notification_event(&fixture->context.event_1c8);
    fixture->context.callback = 1;
    fixture->context.threshold_1fc = 1;
    fixture->device.pgraph_fifo = 1;
    fixture->device.pcrtc_byte = 0xA5;
    snapshot_regs(&fixture->regs);
    fixture->regs_before = fixture->regs;
    fixture->timestamp = 0x1000;
}

static LONG ke_set_event(Fixture *fixture)
{
    LONG previous;

    InterlockedIncrement(&fixture->keset_calls);
    /* The original Event argument is the in-place header at context+0x1C8. */
    if (fixture->context.event_1c8.type == NOTIFICATION_EVENT &&
        fixture->context.event_1c8.size == 4)
        InterlockedIncrement(&fixture->keset_event_matches);
    fixture->keset_increment = 1;
    fixture->keset_wait = 0;
    previous = InterlockedExchange(&fixture->context.event_1c8.signal_state, 1);
    if (fixture->waiter)
        SetEvent(fixture->waiter);
    return previous;
}

static void ke_reset_event(Fixture *fixture)
{
    InterlockedExchange(&fixture->context.event_1c8.signal_state, 0);
    if (fixture->waiter)
        ResetEvent(fixture->waiter);
}

static DWORD wait_notification(Fixture *fixture, DWORD milliseconds)
{
    if (load_long(&fixture->context.event_1c8.signal_state) > 0)
        return WAIT_OBJECT_0;
    if (!fixture->waiter)
        return WAIT_TIMEOUT;
    return WaitForSingleObject(fixture->waiter, milliseconds);
}

static void guest_callback(Fixture *fixture, CallbackInfo *info, uint32_t *esp)
{
    uint32_t before = *esp;

    InterlockedIncrement(&fixture->callback_calls);
    fixture->callback_reason = info->reason;
    fixture->callback_counter = info->counter_1f4;
    fixture->callback_index = info->index_1f0;
    if (fixture->callback_mutates_pcrtc)
        fixture->device.pcrtc_byte = 0x5A;
    *esp = before + 4; /* cdecl: caller will add esp, 4 */
    fixture->callback_esp_delta = *esp - before;
}

static int spin_until_clear(Fixture *fixture, volatile LONG *word, LONG mask,
                            volatile LONG *write_counter, volatile LONG *store,
                            LONG store_value)
{
    DWORD start = GetTickCount();

    for (;;) {
        if (store)
            InterlockedExchange(store, store_value);
        if (write_counter)
            InterlockedIncrement(write_counter);
        if (fixture->entered)
            SetEvent(fixture->entered);
        if ((load_long(word) & mask) == 0)
            return 1;
        if (fixture->spin_deadline_ms &&
            GetTickCount() - start >= fixture->spin_deadline_ms) {
            InterlockedExchange(&fixture->stalled, 1);
            return 0;
        }
        Sleep(0);
    }
}

static void model_00193D10(Fixture *fixture, uint32_t payload, uint32_t timestamp)
{
    InterlockedIncrement(&fixture->queue_calls);
    fixture->queue_payload = payload;
    fixture->queue_timestamp = timestamp;
}

static void model_00196C0B(Fixture *fixture);
static void model_00194210(Fixture *fixture);
static void model_00197AAC(Fixture *fixture, uint32_t channel);

static void model_00193D90(Fixture *fixture)
{
    Context *ctx = &fixture->context;
    DeviceRegs *device = ctx->device;
    uint32_t timestamp = (uint32_t)InterlockedIncrement(&fixture->timestamp) + 0x1000u;
    uint32_t previous = ctx->last_ts_20c;
    uint32_t new_1f4;
    uint32_t delta;
    uint32_t above_threshold;
    uint32_t slot;
    uint32_t queued = 0;
    uint8_t saved_pcrtc = (uint8_t)load_long(&device->pcrtc_byte);
    uint32_t reason = 0;
    CallbackInfo info;
    uint32_t esp;

    fixture->regs.ecx = 0xC011C011u;
    if (previous != 0)
        ctx->delta_208 = timestamp - previous;
    new_1f4 = ctx->counter_1f4 + 1u;
    ctx->last_ts_20c = timestamp;
    delta = new_1f4 - ctx->snapshot_1f8;
    above_threshold = delta >= ctx->threshold_1fc;
    ctx->counter_1f4 = new_1f4;
    slot = ctx->index_1f0 & 1u;
    if (above_threshold && ctx->slot_occupied[slot] != 0) {
        model_00193D10(fixture, ctx->slot_payload[slot], timestamp);
        ctx->slot_occupied[slot] = 0;
        queued = 1;
    }
    if (!spin_until_clear(fixture, &device->pmc_intr, PMC_PCRTC,
                          &fixture->pcrtc_writes, &device->pcrtc_intr, 1))
        return;
    ke_set_event(fixture);
    fixture->regs.eax = 0;
    if (ctx->callback == 0) {
        InterlockedExchange(&device->pcrtc_byte, saved_pcrtc);
        fixture->regs.eax = 0;
        fixture->regs.esp = fixture->regs_before.esp + 4u;
        fixture->regs.ebx = fixture->regs_before.ebx;
        fixture->regs.esi = fixture->regs_before.esi;
        fixture->regs.edi = fixture->regs_before.edi;
        fixture->regs.ebp = fixture->regs_before.ebp;
        InterlockedExchange(&fixture->completed, 1);
        return;
    }
    if (queued)
        reason = 1;
    else if (above_threshold && ctx->threshold_1fc != 0)
        reason = 2;
    info.counter_1f4 = ctx->counter_1f4;
    info.index_1f0 = ctx->index_1f0;
    info.reason = reason;
    esp = fixture->regs.esp;
    guest_callback(fixture, &info, &esp);
    fixture->regs.esp = esp;
    InterlockedExchange(&device->pcrtc_byte, saved_pcrtc);
    fixture->regs.eax = 0;
    fixture->regs.esp = fixture->regs_before.esp + 4u;
    fixture->regs.ebx = fixture->regs_before.ebx;
    fixture->regs.esi = fixture->regs_before.esi;
    fixture->regs.edi = fixture->regs_before.edi;
    fixture->regs.ebp = fixture->regs_before.ebp;
    InterlockedExchange(&fixture->completed, 1);
}

static void model_00196C0B(Fixture *fixture)
{
    InterlockedIncrement(&fixture->calls_96c0b);
    if (load_long(&fixture->device.pgraph_status) == 0)
        return;
    /* Nested service dispatch from 0x00197AAC is unsafe while STATUS stays
     * set: 0x00194210 would spin or recurse.  Classify and clear nothing. */
    InterlockedIncrement(&fixture->unsafe_reentry);
}

static void model_00196C4A(Fixture *fixture, uint32_t channel)
{
    (void)channel;
    InterlockedIncrement(&fixture->calls_96c4a);
}

static void model_00197AAC(Fixture *fixture, uint32_t channel)
{
    Context *ctx = &fixture->context;
    DeviceRegs *device = ctx->device;
    LONG saved_fifo;
    uint32_t old_channel;

    InterlockedIncrement(&fixture->calls_97aac);
    if (load_long(&device->pgraph_intr) != 0) {
        InterlockedIncrement(&fixture->nested_194210);
        model_00194210(fixture);
    }
    saved_fifo = load_long(&device->pgraph_fifo);
    InterlockedExchange(&device->pgraph_fifo, 0);
    model_00196C0B(fixture);
    old_channel = ctx->channel_130;
    if (old_channel != channel)
        model_00196C4A(fixture, old_channel);
    ctx->channel_130 = channel;
    if (channel == 2u) {
        InterlockedExchange(&device->pgraph_ctx_control, 0x10000100);
        InterlockedExchange(&device->pgraph_debug, 0x08000000);
        InterlockedExchange(&device->pgraph_fifo, saved_fifo | 1);
        fixture->regs.ebp = fixture->regs_before.ebp;
        return;
    }
    InterlockedExchange(&device->pgraph_rdi_index, 0x00070000);
    InterlockedExchange(&device->pgraph_rdi_index, 0);
    InterlockedExchange(&device->pgraph_debug, 0x003D0000);
    InterlockedExchange(&device->pgraph_rdi_data, 0);
    InterlockedExchange(&device->pgraph_ctx_user, (channel & 0x1Fu) << 24);
    InterlockedExchange(&device->pgraph_channel_ctx_pointer,
                        ctx->instance_134[channel & 7u] & 0xFFFFu);
    InterlockedExchange(&device->pgraph_channel_ctx_trigger, 1);
    model_00196C0B(fixture);
    InterlockedExchange(&device->pgraph_ctx_control, 0x10010100);
    fixture->regs.ebp = fixture->regs_before.ebp;
}

static void model_00194210(Fixture *fixture)
{
    DeviceRegs *device = fixture->context.device;
    uint32_t intr;
    uint32_t trapped;
    uint32_t nsource;
    uint32_t channel;
    LONG depth;

    depth = InterlockedIncrement(&fixture->reentry_depth);
    InterlockedIncrement(&fixture->calls_194210);
    if (depth > 2) {
        InterlockedIncrement(&fixture->unsafe_reentry);
        InterlockedDecrement(&fixture->reentry_depth);
        return;
    }
    InterlockedExchange(&device->pgraph_fifo, 0);
    intr = (uint32_t)load_long(&device->pgraph_intr);
    trapped = (uint32_t)load_long(&device->pgraph_trapped_addr) & 0x1FFCu;
    nsource = (uint32_t)load_long(&device->pgraph_nsource);
    if (intr & PGRAPH_CTX_SWITCH) {
        InterlockedExchange(&device->pgraph_intr,
                            (LONG)(intr & ~PGRAPH_CTX_SWITCH));
        channel = ((uint32_t)load_long(&device->pgraph_trapped_addr) >> 20) & 0x1Fu;
        if (!spin_until_clear(fixture, &device->pgraph_status, ~0u, NULL, NULL, 0)) {
            InterlockedDecrement(&fixture->reentry_depth);
            return;
        }
        model_00197AAC(fixture, channel);
        intr = (uint32_t)load_long(&device->pgraph_intr);
        if (intr == 0) {
            InterlockedExchange(&device->pgraph_fifo, 1);
            fixture->regs.eax = 0;
            fixture->regs.esp = fixture->regs_before.esp + 4u;
            fixture->regs.ebx = fixture->regs_before.ebx;
            fixture->regs.esi = fixture->regs_before.esi;
            fixture->regs.edi = fixture->regs_before.edi;
            fixture->regs.ebp = fixture->regs_before.ebp;
            InterlockedExchange(&fixture->completed, 1);
            InterlockedDecrement(&fixture->reentry_depth);
            return;
        }
    }
    InterlockedExchange(&device->pgraph_intr, (LONG)intr);
    if (nsource == 0 || (intr & 0x100001u) == 0 || (nsource & 0x40u) != 0 ||
        trapped == 0x100u) {
        InterlockedExchange(&device->pgraph_fifo, 1);
        fixture->regs.eax = (uint32_t)load_long(&device->pgraph_intr);
        fixture->regs.esp = fixture->regs_before.esp + 4u;
        fixture->regs.ebx = fixture->regs_before.ebx;
        fixture->regs.esi = fixture->regs_before.esi;
        fixture->regs.edi = fixture->regs_before.edi;
        fixture->regs.ebp = fixture->regs_before.ebp;
        InterlockedExchange(&fixture->completed, 1);
        InterlockedDecrement(&fixture->reentry_depth);
        return;
    }
    InterlockedIncrement(&fixture->stalled);
    InterlockedDecrement(&fixture->reentry_depth);
}

static DWORD WINAPI worker_193d90(void *argument)
{
    model_00193D90(argument);
    return 0;
}

static DWORD WINAPI worker_194210(void *argument)
{
    model_00194210(argument);
    return 0;
}

static int join_worker(Fixture *fixture, HANDLE thread, const char *name)
{
    DWORD result;
    int ok = 1;

    result = WaitForSingleObject(thread, 1000);
    ok &= check(result == WAIT_OBJECT_0, name);
    if (result != WAIT_OBJECT_0) {
        fprintf(stderr, "FAIL worker did not exit after cooperative deadline\n");
        fflush(stderr);
        ExitProcess(3);
    }
    CloseHandle(thread);
    return ok;
}

static int regs_isolated(const Fixture *fixture)
{
    int ok = 1;
    ok &= check(fixture->regs.ebx == fixture->regs_before.ebx, "EBX restored");
    ok &= check(fixture->regs.esi == fixture->regs_before.esi, "ESI restored");
    ok &= check(fixture->regs.edi == fixture->regs_before.edi, "EDI restored");
    ok &= check(fixture->regs.ebp == fixture->regs_before.ebp, "EBP restored");
    ok &= check(fixture->regs.seh_ebp == fixture->regs_before.seh_ebp,
                "SEH EBP untouched");
    ok &= check(fixture->regs.irql == fixture->regs_before.irql, "IRQL untouched");
    ok &= check(fixture->regs.esp == fixture->regs_before.esp + 4u,
                "RET pops return address only");
    ok &= check(fixture->regs.eax == 0, "EAX returns 0");
    return ok;
}

static int run_193d90_wait_and_callback_cases(void)
{
    int ok = 1;
    Fixture fixture;

    initialize_fixture(&fixture);
    fixture.context.callback = 0;
    model_00193D90(&fixture);
    ok &= check(fixture.pcrtc_writes == 1, "already-clear PMC still writes PCRTC 1");
    ok &= check(fixture.keset_calls == 1, "already-clear still signals");
    ok &= check(fixture.keset_event_matches == 1,
                "KeSetEvent target is Type-0 size-4 context+0x1C8");
    ok &= check(fixture.keset_increment == 1 && fixture.keset_wait == 0,
                "KeSetEvent increment 1 wait FALSE");
    ok &= check(fixture.callback_calls == 0, "null callback pointer skips call");
    ok &= check(fixture.device.pcrtc_byte == 0xA5, "PCRTC byte restored without callback");
    ok &= regs_isolated(&fixture);

    initialize_fixture(&fixture);
    fixture.context.slot_occupied[0] = 1;
    fixture.context.slot_payload[0] = 0xAABBCCDDu;
    fixture.context.index_1f0 = 4;
    fixture.context.counter_1f4 = 10;
    fixture.context.snapshot_1f8 = 10;
    fixture.context.threshold_1fc = 1;
    fixture.callback_mutates_pcrtc = 1;
    model_00193D90(&fixture);
    ok &= check(fixture.queue_calls == 1, "occupied slot queues 0x00193D10");
    ok &= check(fixture.queue_payload == 0xAABBCCDDu, "queue payload from slot+4");
    ok &= check(fixture.context.slot_occupied[0] == 0, "slot cleared after queue");
    ok &= check(fixture.callback_reason == 1, "queued path reason 1");
    ok &= check(fixture.callback_counter == 11, "callback sees incremented 1F4");
    ok &= check(fixture.callback_index == 4, "callback sees 1F0");
    ok &= check(fixture.callback_esp_delta == 4, "cdecl callback cleanup is 4");
    ok &= check(fixture.device.pcrtc_byte == 0xA5, "callback PCRTC mutation restored");
    ok &= regs_isolated(&fixture);

    initialize_fixture(&fixture);
    fixture.context.threshold_1fc = 8;
    fixture.context.counter_1f4 = 1;
    fixture.context.snapshot_1f8 = 1;
    fixture.context.slot_occupied[0] = 1;
    model_00193D90(&fixture);
    ok &= check(fixture.queue_calls == 0, "below-threshold skips queue");
    ok &= check(fixture.callback_reason == 0, "below-threshold reason 0");
    ok &= check(fixture.context.slot_occupied[0] == 1, "skipped queue leaves slot");

    initialize_fixture(&fixture);
    fixture.context.threshold_1fc = 1;
    fixture.context.counter_1f4 = 10;
    fixture.context.snapshot_1f8 = 10;
    fixture.context.slot_occupied[0] = 0;
    model_00193D90(&fixture);
    ok &= check(fixture.queue_calls == 0, "empty slot skips queue");
    ok &= check(fixture.callback_reason == 2, "threshold without queue reason 2");
    return ok;
}

static int run_193d90_signal_and_stall_cases(void)
{
    int ok = 1;
    Fixture fixture;
    HANDLE thread;

    initialize_fixture(&fixture);
    fixture.waiter = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.waiter != NULL, "waiter created");
    if (!fixture.waiter) return 0;
    ok &= check(wait_notification(&fixture, 0) == WAIT_OBJECT_0,
                "init SignalState 1 is already signaled");
    ke_reset_event(&fixture);
    ok &= check(wait_notification(&fixture, 25) == WAIT_TIMEOUT,
                "reset-before-wait blocks on Type-0 object");
    model_00193D90(&fixture);
    ok &= check(wait_notification(&fixture, 25) == WAIT_OBJECT_0,
                "KeSetEvent delivers to reset waiter");
    ok &= check(fixture.context.event_1c8.signal_state == 1,
                "NotificationEvent stays signaled");
    ok &= check(wait_notification(&fixture, 0) == WAIT_OBJECT_0,
                "later wait still sees signaled notification");
    ke_reset_event(&fixture);
    ok &= check(wait_notification(&fixture, 25) == WAIT_TIMEOUT,
                "reset after signal misses that KeSetEvent");
    CloseHandle(fixture.waiter);
    fixture.waiter = NULL;

    initialize_fixture(&fixture);
    fixture.device.pmc_intr = PMC_PCRTC;
    fixture.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.entered != NULL, "clear-wait event created");
    if (!fixture.entered) return 0;
    thread = CreateThread(NULL, 0, worker_193d90, &fixture, 0, NULL);
    ok &= check(thread != NULL, "clear-wait worker created");
    if (!thread) {
        CloseHandle(fixture.entered);
        return 0;
    }
    ok &= check(WaitForSingleObject(fixture.entered, 1000) == WAIT_OBJECT_0,
                "spin observed set PMC PCRTC bit");
    ok &= check(WaitForSingleObject(thread, 0) == WAIT_TIMEOUT,
                "set PMC bit has no false completion");
    ok &= check(fixture.keset_calls == 0 && fixture.callback_calls == 0,
                "signal and callback wait for PMC clear");
    InterlockedExchange(&fixture.device.pmc_intr, 0);
    ok &= join_worker(&fixture, thread, "PMC clear releases 0x00193D90");
    ok &= check(fixture.keset_calls == 1 && fixture.completed == 1,
                "PMC clear then KeSetEvent");
    CloseHandle(fixture.entered);

    initialize_fixture(&fixture);
    fixture.device.pmc_intr = PMC_PCRTC;
    fixture.spin_deadline_ms = 25;
    fixture.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.entered != NULL, "stall event created");
    if (!fixture.entered) return 0;
    thread = CreateThread(NULL, 0, worker_193d90, &fixture, 0, NULL);
    ok &= check(thread != NULL, "stall worker created");
    if (!thread) {
        CloseHandle(fixture.entered);
        return 0;
    }
    ok &= check(WaitForSingleObject(fixture.entered, 1000) == WAIT_OBJECT_0,
                "never-clear worker entered PCRTC spin");
    ok &= join_worker(&fixture, thread, "never-clear spin hits test deadline");
    ok &= check(fixture.stalled == 1 && fixture.completed == 0,
                "never-clear PMC classified as stall");
    ok &= check(fixture.keset_calls == 0 && fixture.callback_calls == 0,
                "never-clear PMC skips KeSetEvent and callback");
    CloseHandle(fixture.entered);
    return ok;
}

static int run_reentry_cases(void)
{
    int ok = 1;
    Fixture fixture;
    HANDLE thread;

    initialize_fixture(&fixture);
    fixture.device.pgraph_intr = PGRAPH_CTX_SWITCH;
    fixture.device.pgraph_trapped_addr = 2u << 20;
    fixture.device.pgraph_status = 0;
    fixture.regs_before.esp = 0x00F7FC20u;
    fixture.regs.esp = fixture.regs_before.esp;
    model_00194210(&fixture);
    ok &= check(fixture.calls_194210 == 1, "one outer 0x00194210");
    ok &= check(fixture.calls_97aac == 1, "context-switch calls 0x00197AAC");
    ok &= check(fixture.nested_194210 == 0,
                "W1C of only 0x1000 does not nested-reenter 0x00194210");
    ok &= check(fixture.calls_96c0b == 1, "channel 2 polling once");
    ok &= check(fixture.unsafe_reentry == 0, "idle STATUS is not recursive service");
    ok &= check(fixture.calls_96c4a == 1, "channel change records 0x00196C4A");
    ok &= check(fixture.context.channel_130 == 2, "requested channel stored");
    ok &= check(fixture.device.pgraph_fifo == 1, "FIFO re-enabled on completion");
    ok &= check(fixture.completed == 1, "context-switch completes once");
    ok &= check(fixture.regs.ebp == fixture.regs_before.ebp, "reentry restores EBP");
    ok &= check(fixture.regs.irql == fixture.regs_before.irql, "reentry IRQL isolated");

    initialize_fixture(&fixture);
    fixture.device.pgraph_intr = PGRAPH_CTX_SWITCH;
    fixture.device.pgraph_status = 1;
    fixture.spin_deadline_ms = 25;
    fixture.entered = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok &= check(fixture.entered != NULL, "status-stall event created");
    if (!fixture.entered) return 0;
    thread = CreateThread(NULL, 0, worker_194210, &fixture, 0, NULL);
    ok &= check(thread != NULL, "status-stall worker created");
    if (!thread) {
        CloseHandle(fixture.entered);
        return 0;
    }
    ok &= check(WaitForSingleObject(fixture.entered, 1000) == WAIT_OBJECT_0,
                "STATUS spin entered");
    ok &= join_worker(&fixture, thread, "never-clear STATUS hits deadline");
    ok &= check(fixture.stalled == 1 && fixture.calls_97aac == 0,
                "never-clear STATUS does not call 0x00197AAC");
    ok &= check(fixture.device.pgraph_fifo == 0, "stalled context-switch leaves FIFO off");
    ok &= check(fixture.completed == 0, "STATUS stall is not completion");
    CloseHandle(fixture.entered);

    initialize_fixture(&fixture);
    fixture.device.pgraph_intr = PGRAPH_CTX_SWITCH;
    fixture.device.pgraph_trapped_addr = 1u << 20;
    fixture.device.pgraph_status = 0;
    fixture.context.channel_130 = 1;
    model_00194210(&fixture);
    ok &= check(fixture.calls_96c0b == 2, "non-2 channel polls twice");
    ok &= check(fixture.calls_96c4a == 0, "same channel skips 0x00196C4A");
    ok &= check(fixture.nested_194210 == 0, "no recursive false context-switch");
    ok &= check(fixture.completed == 1, "non-2 channel completes");

    initialize_fixture(&fixture);
    fixture.device.pgraph_status = 1;
    fixture.calls_96c0b = 0;
    model_00196C0B(&fixture);
    ok &= check(fixture.unsafe_reentry == 1,
                "0x00196C0B with STATUS set during 0x00197AAC classified unsafe");
    return ok;
}

static DWORD WINAPI isolation_worker(void *argument)
{
    Fixture *fixture = argument;
    fixture->regs.eax ^= 0xFFFFFFFFu;
    fixture->regs.edx ^= (uint32_t)GetCurrentThreadId();
    model_00193D90(fixture);
    return 0;
}

static int run_isolation_case(void)
{
    int ok = 1;
    Fixture a, b;
    HANDLE threads[2];

    initialize_fixture(&a);
    initialize_fixture(&b);
    a.regs_before.irql = 0x11u;
    b.regs_before.irql = 0x22u;
    a.regs.irql = 0x11u;
    b.regs.irql = 0x22u;
    a.regs_before.seh_ebp = 0x11111111u;
    b.regs_before.seh_ebp = 0x22222222u;
    a.regs.seh_ebp = 0x11111111u;
    b.regs.seh_ebp = 0x22222222u;
    threads[0] = CreateThread(NULL, 0, isolation_worker, &a, 0, NULL);
    threads[1] = CreateThread(NULL, 0, isolation_worker, &b, 0, NULL);
    ok &= check(threads[0] && threads[1], "isolation workers created");
    if (!threads[0] || !threads[1]) {
        if (threads[0]) CloseHandle(threads[0]);
        if (threads[1]) CloseHandle(threads[1]);
        return 0;
    }
    ok &= join_worker(&a, threads[0], "isolation A joined");
    ok &= join_worker(&b, threads[1], "isolation B joined");
    ok &= check(a.regs.irql == 0x11u && b.regs.irql == 0x22u,
                "per-thread IRQL isolated");
    ok &= check(a.regs.seh_ebp == 0x11111111u && b.regs.seh_ebp == 0x22222222u,
                "per-thread SEH isolated");
    ok &= check(a.regs.ebx == a.regs_before.ebx && b.regs.ebx == b.regs_before.ebx,
                "per-thread EBX restored independently");
    ok &= check(a.completed == 1 && b.completed == 1, "both isolation workers completed");
    return ok;
}

int main(void)
{
    int ok = 1;
    unsigned repetition;

    for (repetition = 0; repetition < 64u; ++repetition) {
        ok &= run_193d90_wait_and_callback_cases();
        ok &= run_193d90_signal_and_stall_cases();
        ok &= run_reentry_cases();
        ok &= run_isolation_case();
        if (!ok) break;
    }
    if (ok)
        printf("PASS: %u bounded 0x00193D90/0x00194210/0x00197AAC checks\n", checks);
    return ok ? 0 : 1;
}
