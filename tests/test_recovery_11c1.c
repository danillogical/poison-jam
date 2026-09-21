#include "recomp_types.h"
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

extern void sub_0019460A(void);
extern void sub_00194635(void);
extern void sub_00196800(void);
extern void sub_00196967(void);
extern void sub_00196A65(void);
extern void sub_00194533(void);
extern void sub_00196B34(void);
extern void sub_00196B6A(void);
extern void sub_00196B9A(void);
extern void sub_00194520(void);
extern void sub_00194676(void);
extern void sub_00196E80(void);
extern void sub_00197BCF(void);
extern void sub_00193C40(void);
extern void sub_00196C0B(void);
extern void sub_00193D90(void);
extern void sub_00194210(void);
extern void sub_00197AAC(void);
extern void sub_00196C4A(void);
extern void sub_00194780(void);
extern void sub_00194ADD(void);
extern void sub_00197C50(void);
extern void sub_001948B9(void);
extern void sub_00194913(void);
extern void sub_00196E92(void);
extern void sub_00194573(void);
extern void sub_0019701F(void);
extern void sub_001945D6(void);
extern void sub_00197058(void);
extern void sub_001970CD(void);
extern void sub_001949E2(void);
extern void sub_001912A0(void);

RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
RECOMP_TLS uint32_t g_fs_base, g_seh_ebp;
RECOMP_TLS int g_df;
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo, g_xbox_code_hi;

RECOMP_TLS volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];
RECOMP_TLS volatile uint32_t g_icall_trace_idx;
RECOMP_TLS volatile uint64_t g_icall_count;

/* The focused executable does not link the toolkit kernel HAL.  This fixed
 * seam tests the recovered RDTSC register split exactly; the toolkit's
 * xbox_timestamp_publication target separately tests the production adapter. */
static uint64_t timestamp_fixture_value;

uint64_t xbox_ReadTimeStampCounter(void)
{
    return timestamp_fixture_value;
}

static uint32_t claim_base;
static uint32_t claim_padding;
static uint32_t claim_bytes;
static uint32_t claim_padding_va;

static void fake_claim_gpu_instance_memory(void)
{
    claim_padding_va = MEM32(g_esp + 8);
    claim_bytes = MEM32(g_esp + 4);
    MEM32(claim_padding_va) = claim_padding;
    g_eax = claim_base;
    g_esp += 12; /* RET plus two stdcall arguments. */
}

recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }
recomp_func_t recomp_lookup(uint32_t va) { (void)va; return NULL; }
static uint32_t keset_calls, keset_event, keset_increment, keset_wait, keset_previous;
static uint32_t callback_calls, callback_1f4, callback_1f0, callback_reason;
static uint32_t calls_97aac, channel_97aac;
static uint32_t calls_96c4a, channel_96c4a;
static uint32_t calls_194210_reentry;

void jsrf_test_KeSetEvent(void)
{
    uint32_t event = MEM32(g_esp + 4u);
    ++keset_calls;
    keset_event = event;
    keset_increment = MEM32(g_esp + 8u);
    keset_wait = MEM32(g_esp + 12u);
    keset_previous = event ? MEM32(event + 4u) : 0u;
    if (event)
        MEM32(event + 4u) = 1u;
    g_eax = keset_previous;
    g_esp += 16u; /* stdcall 3 arguments plus return address */
}

void jsrf_test_guest_callback(void)
{
    uint32_t info = MEM32(g_esp + 4u);
    ++callback_calls;
    callback_1f4 = MEM32(info);
    callback_1f0 = MEM32(info + 4u);
    callback_reason = MEM32(info + 8u);
    g_esp += 4u; /* cdecl: caller add esp, 4 */
}

void jsrf_test_97AAC(void)
{
    uint32_t device = MEM32(g_ecx);
    ++calls_97aac;
    channel_97aac = MEM32(g_esp + 4u);
    MEM32(device + 0x400100u) = 0; /* model PGRAPH_INTR W1C readback */
    g_esp += 8u; /* RET 4 */
}

void jsrf_test_96C4A(void)
{
    ++calls_96c4a;
    channel_96c4a = MEM32(g_esp + 4u);
    g_esp += 8u; /* RET 4 */
}

void jsrf_test_194210_reentry(void)
{
    ++calls_194210_reentry;
    g_esp += 4u;
}

static uint32_t calls_97c50;
static uint16_t last_out_port;
static uint8_t last_out_value;

void jsrf_test_97C50(void)
{
    ++calls_97c50;
    g_esp += 4u;
}

void xbox_outb(uint16_t port, uint8_t value) { last_out_port = port; last_out_value = value; }
void xbox_outw(uint16_t port, uint16_t value) { (void)port; (void)value; }
void xbox_outl(uint16_t port, uint32_t value) { (void)port; (void)value; }
uint8_t xbox_inb(uint16_t port) { (void)port; return 0; }
uint16_t xbox_inw(uint16_t port) { (void)port; return 0; }
uint32_t xbox_inl(uint16_t port) { (void)port; return 0; }

void jsrf_test_KeQuerySystemTime(void)
{
    uint32_t dest = MEM32(g_esp + 4u);
    MEM32(dest) = 0x80A3E000u;
    MEM32(dest + 4u) = 0x01C1A1A1u;
    g_esp += 8u;
}

void jsrf_test_RtlTimeToTimeFields(void)
{
    uint32_t fields = MEM32(g_esp + 8u);
    MEM16(fields) = 2002;
    MEM16(fields + 2u) = 1;
    MEM16(fields + 4u) = 1;
    MEM16(fields + 6u) = 0;
    MEM16(fields + 8u) = 0;
    MEM16(fields + 10u) = 0;
    MEM16(fields + 12u) = 0;
    MEM16(fields + 14u) = 2;
    g_esp += 12u;
}

void sub_00193F70(void) { recomp_icall_fail_log(0x00193F70u); abort(); }
void sub_0014B794(void) { recomp_icall_fail_log(0x0014B794u); abort(); }
void sub_0018E120(void) { recomp_icall_fail_log(0x0018E120u); abort(); }

recomp_func_t recomp_lookup_kernel(uint32_t va)
{
    if (va == 0xFE000000u) return fake_claim_gpu_instance_memory;
    if (va == 0xFE000091u) return jsrf_test_KeSetEvent;
    if (va == 0xFE0000CBu) return jsrf_test_guest_callback;
    if (va == 0xFE000128u) return jsrf_test_KeQuerySystemTime;
    if (va == 0xFE000305u) return jsrf_test_RtlTimeToTimeFields;
    return NULL;
}
void recomp_icall_fail_log(uint32_t va) { (void)va; abort(); }
void recomp_icall_not_code_log(uint32_t va) { (void)va; abort(); }

/* Focused-only seams for 0x00196C0B.  The generated production body calls
 * the real unresolved symbols; recovered_11c1_test.c rewrites only this unit
 * to these hooks.  Each hook consumes the generated call return address. */
static uint32_t service_calls_194210;
static uint32_t service_calls_193D90;
static uint32_t service_order[4];
static uint32_t service_order_count;
static uint32_t service_clear_after_first;

static uint32_t submit_18e500_calls, submit_18e500_payload;
static uint32_t submit_18e584_calls, submit_18e584_context, submit_18e584_buffer;
static uint32_t submit_sequence[4], submit_sequence_count;
static uint32_t submit_expected_context, submit_expected_status_offset;
static uint32_t submit_status_seen;

void jsrf_test_submit_18E500(void)
{
    ++submit_18e500_calls;
    if (submit_sequence_count < 4u) submit_sequence[submit_sequence_count++] = 0x18E500u;
    submit_18e500_payload = MEM32(g_esp + 4u);
    g_esp += 8; /* RET 4 plus the one cdecl argument. */
}

void jsrf_test_submit_18E584(void)
{
    ++submit_18e584_calls;
    if (submit_sequence_count < 4u) submit_sequence[submit_sequence_count++] = 0x18E584u;
    submit_18e584_context = g_ecx;
    submit_18e584_buffer = MEM32(g_esp + 4u);
    if (submit_expected_context != 0u)
        submit_status_seen = MEM32(submit_expected_context + submit_expected_status_offset);
    g_esp += 8; /* RET 4 plus the one cdecl argument. */
}

void jsrf_test_service_194210(void)
{
    ++service_calls_194210;
    if (service_order_count < 4) service_order[service_order_count++] = 0x194210u;
    if (service_clear_after_first) MEM32(MEM32(g_ecx) + 0x400700u) = 0;
    g_esp += 4;
}

void jsrf_test_service_193D90(void)
{
    ++service_calls_193D90;
    if (service_order_count < 4) service_order[service_order_count++] = 0x193D90u;
    g_esp += 4;
}

static unsigned checks;
static int check_u32(uint32_t actual, uint32_t expected, const char *name)
{
    ++checks;
    if (actual == expected) return 1;
    fprintf(stderr, "FAIL %s: actual=%08X expected=%08X\n", name, actual, expected);
    return 0;
}

static int run_service_loop_recovery_tests(uint32_t context, uint32_t device)
{
    int ok = 1;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u; g_seh_ebp = 0x55555555u;
    g_ecx = context; g_esp = 0x00F00000u;
    MEM32(context) = device;
    MEM32(device + 0x400700u) = 0;
    MEM32(device + 0x100u) = 0x01001000u;
    service_calls_194210 = service_calls_193D90 = service_order_count = 0;
    service_clear_after_first = 0;
    sub_00196C0B();
    ok &= check_u32(service_calls_194210, 0, "196C0B zero status skips 194210");
    ok &= check_u32(service_calls_193D90, 0, "196C0B zero status skips 193D90");
    ok &= check_u32(g_esp, 0x00F00004u, "196C0B zero status RET cleanup");

    g_esp = 0x00F10000u;
    g_ebx = 0x66666666u; g_esi = 0x77777777u; g_edi = 0x88888888u;
    g_ebp = 0x99999999u; g_seh_ebp = 0xAAAAAAAAu;
    MEM32(device + 0x400700u) = 1;
    MEM32(device + 0x100u) = 0x01001000u;
    service_calls_194210 = service_calls_193D90 = service_order_count = 0;
    service_clear_after_first = 1;
    sub_00196C0B();
    ok &= check_u32(service_calls_194210, 1, "196C0B snapshot calls 194210");
    ok &= check_u32(service_calls_193D90, 1, "196C0B snapshot calls 193D90");
    ok &= check_u32(service_order[0], 0x194210u, "196C0B callback order first");
    ok &= check_u32(service_order[1], 0x193D90u, "196C0B callback order second");
    ok &= check_u32(MEM32(device + 0x400700u), 0, "196C0B reread observes clear");
    ok &= check_u32(g_esp, 0x00F10004u, "196C0B callback stack cleanup");
    ok &= check_u32(g_ebx, 0x66666666u, "196C0B EBX preserved");
    ok &= check_u32(g_esi, 0x77777777u, "196C0B ESI preserved");
    ok &= check_u32(g_edi, 0x88888888u, "196C0B EDI preserved");
    ok &= check_u32(g_ebp, 0x99999999u, "196C0B EBP preserved");
    ok &= check_u32(g_seh_ebp, 0xAAAAAAAAu, "196C0B SEH metadata preserved");
    return ok;
}

static int run_queue_helper_recovery_tests(uint32_t context, uint32_t device)
{
    int ok = 1;
    const uint32_t payload = 0x13579BDFu;
    const uint32_t stored = 0x2468ACE0u;
    MEM32(context) = device;
    MEM32(context + 0x1F4u) = 0xCAFEBABEu;
    MEM32(context + 0x210u) = 0xDEADBEEFu;
    MEM32(device + 0x40071Cu) = 0xA0u;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u; g_seh_ebp = 0x55555555u;

    /* Empty slot: the hardware submit helper still runs, but no callback is
     * permitted and all caller-visible queue state is updated exactly once. */
    MEM32(context + 0x1F0u) = 0u;
    MEM32(context + 0x814u) = 0u;
    submit_18e500_calls = submit_18e584_calls = 0u;
    submit_sequence_count = 0u; submit_expected_context = 0u;
    g_ecx = context; g_esp = 0x01080000u;
    MEM32(g_esp + 4u) = payload; MEM32(g_esp + 8u) = stored;
    sub_00193D10();
    ok &= check_u32(submit_18e500_calls, 1u, "193D10 empty submit count");
    ok &= check_u32(submit_18e500_payload, payload, "193D10 payload argument");
    ok &= check_u32(submit_18e584_calls, 0u, "193D10 empty skips callback");
    ok &= check_u32(MEM32(context + 0x1F0u), 1u, "193D10 empty index increment");
    ok &= check_u32(MEM32(context + 0x1F8u), 0xCAFEBABEu, "193D10 empty copy");
    ok &= check_u32(MEM32(context + 0x210u), stored, "193D10 stored argument");
    ok &= check_u32(MEM32(device + 0x40071Cu), 0xA2u, "193D10 device status bit");
    ok &= check_u32(g_eax, 0xCAFEBABEu, "193D10 EAX copied +1F4");
    ok &= check_u32(g_esp, 0x0108000Cu, "193D10 RET 8 cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "193D10 EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "193D10 ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "193D10 EDI preserved");

    /* Active slot zero records the exact context and +0x214 callback buffer. */
    MEM32(context + 0x1F0u) = 0u;
    MEM32(context + 0x814u) = 1u;
    MEM32(device + 0x40071Cu) = 0x40u;
    submit_18e500_calls = submit_18e584_calls = 0u;
    submit_sequence_count = 0u; submit_expected_context = context;
    submit_expected_status_offset = 0x814u;
    submit_status_seen = 0u;
    g_ecx = context; g_esp = 0x01090000u;
    MEM32(g_esp + 4u) = payload; MEM32(g_esp + 8u) = stored;
    sub_00193D10();
    ok &= check_u32(submit_18e584_calls, 1u, "193D10 slot0 callback count");
    ok &= check_u32(submit_18e584_context, context, "193D10 slot0 callback context");
    ok &= check_u32(submit_18e584_buffer, context + 0x214u,
                    "193D10 slot0 callback buffer");
    ok &= check_u32(submit_sequence[0], 0x18E500u, "193D10 slot0 submit first");
    ok &= check_u32(submit_sequence[1], 0x18E584u, "193D10 slot0 callback second");
    ok &= check_u32(submit_status_seen, 1u, "193D10 slot0 callback sees active status");
    ok &= check_u32(MEM32(context + 0x814u), 0u, "193D10 slot0 status clear");
    ok &= check_u32(MEM32(device + 0x40071Cu), 0x42u, "193D10 slot0 status bit");

    /* Slot one uses +0x300, and index increment wraps as an x86 dword. */
    MEM32(context + 0x1F0u) = 0xFFFFFFFFu;
    MEM32(context + 0x818u) = 1u;
    MEM32(device + 0x40071Cu) = 0x100u;
    submit_18e500_calls = submit_18e584_calls = 0u;
    submit_sequence_count = 0u; submit_expected_context = context;
    submit_expected_status_offset = 0x818u;
    submit_status_seen = 0u;
    g_ecx = context; g_esp = 0x010A0000u;
    MEM32(g_esp + 4u) = payload; MEM32(g_esp + 8u) = stored;
    sub_00193D10();
    ok &= check_u32(submit_18e584_calls, 1u, "193D10 slot1 callback count");
    ok &= check_u32(submit_18e584_context, context, "193D10 slot1 callback context");
    ok &= check_u32(submit_18e584_buffer, context + 0x514u,
                    "193D10 slot1 callback +514 buffer");
    ok &= check_u32(submit_sequence[0], 0x18E500u, "193D10 slot1 submit first");
    ok &= check_u32(submit_sequence[1], 0x18E584u, "193D10 slot1 callback second");
    ok &= check_u32(submit_status_seen, 1u, "193D10 slot1 callback sees active status");
    ok &= check_u32(MEM32(context + 0x818u), 0u, "193D10 slot1 status clear");
    ok &= check_u32(MEM32(context + 0x1F0u), 0u, "193D10 index wraps");
    ok &= check_u32(MEM32(device + 0x40071Cu), 0x102u, "193D10 slot1 device status bit");
    ok &= check_u32(g_eax, 0xCAFEBABEu, "193D10 slot1 EAX copied +1F4");
    ok &= check_u32(g_ebp, 0x44444444u, "193D10 EBP preserved");
    ok &= check_u32(g_seh_ebp, 0x55555555u, "193D10 SEH EBP preserved");
    return ok;
}

static int run_callback_reentry_recovery_tests(uint32_t context, uint32_t device)
{
    int ok = 1;
    MEM32(0x001C4014u) = 0xFE000091u;
    MEM32(context) = device;
    MEM8(device + 0x6013D4u) = 0xA5u;
    MEM32(device + 0x100u) = 0;
    MEM32(device + 0x600100u) = 0;
    MEM32(context + 0x1C4u) = 0;
    MEM32(context + 0x1C8u) = 0x00040000u; /* Type 0, Size 4 */
    MEM32(context + 0x1CCu) = 1u;
    MEM32(context + 0x1F0u) = 4u;
    MEM32(context + 0x1F4u) = 10u;
    MEM32(context + 0x1F8u) = 10u;
    MEM32(context + 0x1FCu) = 1u;
    MEM32(context + 0x208u) = 0;
    MEM32(context + 0x20Cu) = 0;
    MEM32(context + 0x1B0u) = 0;
    keset_calls = callback_calls = 0;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u; g_seh_ebp = 0x55555555u;
    g_ecx = context; g_esp = 0x01100000u;
    sub_00193D90();
    ok &= check_u32(keset_calls, 1u, "193D90 already-clear still KeSetEvent");
    ok &= check_u32(keset_event, context + 0x1C8u, "193D90 Event is context+0x1C8");
    ok &= check_u32(keset_increment, 1u, "193D90 Increment 1");
    ok &= check_u32(keset_wait, 0u, "193D90 Wait FALSE");
    ok &= check_u32(callback_calls, 0u, "193D90 null callback skips call");
    ok &= check_u32(MEM8(device + 0x6013D4u), 0xA5u, "193D90 PCRTC byte restored");
    ok &= check_u32(MEM32(device + 0x600100u), 1u, "193D90 writes PCRTC INTR 1");
    ok &= check_u32(g_eax, 0u, "193D90 EAX 0");
    ok &= check_u32(g_esp, 0x01100004u, "193D90 RET cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "193D90 EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "193D90 ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "193D90 EDI preserved");
    ok &= check_u32(g_ebp, 0x44444444u, "193D90 EBP preserved");
    ok &= check_u32(g_seh_ebp, 0x55555555u, "193D90 SEH preserved");

    MEM32(context + 0x1C4u) = 0xFE0000CBu;
    MEM32(context + 0x1B0u) = 1u;
    MEM32(context + 0x1B4u) = 0xAABBCCDDu;
    MEM32(context + 0x1F0u) = 0u;
    MEM32(context + 0x1F4u) = 10u;
    MEM32(context + 0x1F8u) = 10u;
    MEM32(context + 0x1FCu) = 1u;
    MEM32(context + 0x20Cu) = 0x1000u;
    MEM32(context + 0x814u) = 0u;
    submit_18e500_calls = submit_18e584_calls = 0u;
    keset_calls = callback_calls = 0;
    g_ecx = context; g_esp = 0x01110000u;
    sub_00193D90();
    ok &= check_u32(submit_18e500_calls, 1u, "193D90 queues through 0x00193D10");
    ok &= check_u32(MEM32(context + 0x1B0u), 0u, "193D90 clears occupied slot");
    ok &= check_u32(callback_calls, 1u, "193D90 cdecl callback once");
    ok &= check_u32(callback_reason, 1u, "193D90 queued reason 1");
    ok &= check_u32(callback_1f4, 11u, "193D90 callback 1F4");
    ok &= check_u32(callback_1f0, 1u, "193D90 callback 1F0 after queue increment");
    ok &= check_u32(MEM32(context + 0x208u), 0x01234567u - 0x1000u,
                    "193D90 timestamp delta");

    MEM32(context + 0x1B0u) = 0u;
    MEM32(context + 0x1F4u) = 10u;
    MEM32(context + 0x1F8u) = 10u;
    MEM32(context + 0x1FCu) = 1u;
    callback_calls = keset_calls = 0;
    g_ecx = context; g_esp = 0x01120000u;
    sub_00193D90();
    ok &= check_u32(callback_reason, 2u, "193D90 threshold without slot reason 2");

    MEM32(context + 0x1F4u) = 1u;
    MEM32(context + 0x1F8u) = 1u;
    MEM32(context + 0x1FCu) = 8u;
    MEM32(context + 0x1B0u) = 1u;
    callback_calls = 0;
    g_ecx = context; g_esp = 0x01130000u;
    sub_00193D90();
    ok &= check_u32(callback_reason, 0u, "193D90 below-threshold reason 0");
    ok &= check_u32(MEM32(context + 0x1B0u), 1u, "193D90 below-threshold keeps slot");

    MEM32(device + 0x400720u) = 1u;
    MEM32(device + 0x400100u) = 0x1000u;
    MEM32(device + 0x400700u) = 0u;
    MEM32(device + 0x400704u) = 2u << 20;
    MEM32(device + 0x400108u) = 0u;
    calls_97aac = 0;
    g_ebx = 0xA1A1A1A1u; g_esi = 0xB2B2B2B2u; g_edi = 0xC3C3C3C3u;
    g_ebp = 0xD4D4D4D4u; g_seh_ebp = 0xE5E5E5E5u;
    g_ecx = context; g_esp = 0x01140000u;
    sub_00194210();
    ok &= check_u32(calls_97aac, 1u, "194210 context-switch calls 97AAC");
    ok &= check_u32(channel_97aac, 0u, "194210 channel is masked trapped method>>20");
    ok &= check_u32(MEM32(device + 0x400720u), 1u, "194210 FIFO re-enabled");
    ok &= check_u32(g_esp, 0x01140004u, "194210 RET cleanup");
    ok &= check_u32(g_ebx, 0xA1A1A1A1u, "194210 EBX preserved");
    ok &= check_u32(g_ebp, 0xD4D4D4D4u, "194210 EBP preserved");
    ok &= check_u32(g_seh_ebp, 0xE5E5E5E5u, "194210 SEH preserved");

    MEM32(device + 0x400100u) = 0u;
    MEM32(device + 0x400700u) = 0u;
    MEM32(device + 0x400720u) = 1u;
    MEM32(context + 0x130u) = 2u;
    calls_96c4a = calls_194210_reentry = 0;
    g_ecx = context; g_esp = 0x01150000u;
    MEM32(g_esp + 4u) = 2u;
    g_ebp = 0xF1F1F1F1u; g_seh_ebp = 0xF2F2F2F2u;
    sub_00197AAC();
    ok &= check_u32(calls_194210_reentry, 0u, "97AAC zero INTR skips reentry");
    ok &= check_u32(calls_96c4a, 0u, "97AAC same channel skips 96C4A");
    ok &= check_u32(MEM32(context + 0x130u), 2u, "97AAC stores channel 2");
    ok &= check_u32(MEM32(device + 0x400720u) & 1u, 1u, "97AAC channel 2 FIFO|1");
    ok &= check_u32(g_esp, 0x01150008u, "97AAC RET 4 cleanup");
    ok &= check_u32(g_ebp, 0xF1F1F1F1u, "97AAC restores caller EBP");
    ok &= check_u32(g_seh_ebp, 0xF2F2F2F2u, "97AAC restores caller SEH");

    MEM32(context + 0x130u) = 0u;
    MEM32(device + 0x400100u) = 0x20u;
    calls_96c4a = calls_194210_reentry = 0;
    g_ecx = context; g_esp = 0x01160000u;
    MEM32(g_esp + 4u) = 2u;
    sub_00197AAC();
    ok &= check_u32(calls_194210_reentry, 1u, "97AAC nonzero INTR reenters 194210");
    ok &= check_u32(calls_96c4a, 1u, "97AAC channel change calls 96C4A");
    ok &= check_u32(channel_96c4a, 0u, "97AAC 96C4A old channel");

    MEM32(context + 0x138u) = 0x0000ABCDu;
    MEM32(device + 0x400784u) = 0u;
    MEM32(device + 0x400788u) = 0u;
    MEM32(device + 0x400144u) = 0u;
    MEM32(device + 0x400700u) = 0u;
    g_ebx = 0x12121212u; g_esi = 0x34343434u; g_edi = 0x56565656u;
    g_ebp = 0x78787878u; g_seh_ebp = 0x9A9A9A9Au;
    g_ecx = context; g_esp = 0x01170000u;
    MEM32(g_esp + 4u) = 2u;
    sub_00196C4A();
    ok &= check_u32(MEM32(device + 0x400784u), 0u, "96C4A channel 2 skips unload");
    ok &= check_u32(g_esp, 0x01170008u, "96C4A RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "96C4A EBX preserved");
    ok &= check_u32(g_esi, 0x34343434u, "96C4A ESI preserved");
    g_esp = 0x01180000u;
    MEM32(g_esp + 4u) = 1u;
    sub_00196C4A();
    ok &= check_u32(MEM32(device + 0x400784u), 0xABCDu, "96C4A writes masked instance");
    ok &= check_u32(MEM32(device + 0x400788u), 2u, "96C4A trigger 2");
    ok &= check_u32(MEM32(device + 0x400144u), 0x10000000u, "96C4A CTX_CONTROL");
    ok &= check_u32(g_esp, 0x01180008u, "96C4A channel 1 RET 4");
    ok &= check_u32(g_ebp, 0x78787878u, "96C4A EBP preserved");

    MEM32(0x001C3FB4u) = 0xFE000128u;
    MEM32(0x001C4054u) = 0xFE000305u;
    MEM32(context + 0xB0u) = 1u;
    MEM32(context + 0x118u) = 0u;
    MEM32(device + 0x184Cu) = 0x12340300u;
    MEM32(device + 0x200u) = 0u;
    MEM32(device + 0x140u) = 0u;
    MEM32(device + 0x400720u) = 1u;
    MEM32(device + 0x400100u) = 0u;
    MEM32(device + 0x400140u) = 0u;
    MEM32(device + 0x2100u) = 0u;
    MEM32(device + 0x2140u) = 0xFFFFFFFFu;
    calls_97c50 = 0;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u; g_seh_ebp = 0x55555555u;
    g_ecx = context; g_esp = 0x01190000u;
    sub_00194780();
    ok &= check_u32(g_eax, 1u, "94780 EAX 1");
    ok &= check_u32(calls_97c50, 1u, "94780 calls 0x00197C50");
    ok &= check_u32(MEM32(device + 0x200u), 0xFFFFFFFFu, "94780 PMC_ENABLE");
    ok &= check_u32(MEM32(device + 0x140u), 1u, "94780 PMC_INTR_EN");
    ok &= check_u32(MEM32(device + 0x400720u), 0u, "94780 PGRAPH FIFO off before 97C50");
    ok &= check_u32(MEM32(device + 0x400100u), 0xFFFFFFFFu, "94780 PGRAPH_INTR W1C after 97C50");
    ok &= check_u32(MEM32(device + 0x2140u), 0u, "94780 PFIFO_INTR_EN from context+118");
    ok &= check_u32(g_esp, 0x01190004u, "94780 RET cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "94780 EBX preserved");
    ok &= check_u32(g_ebp, 0x44444444u, "94780 EBP restored");

    MEM32(device + 0x200u) = 0xFFFFFFFFu;
    MEM32(device + 0x400700u) = 0u;
    MEM32(device + 0x1218u) = 0u;
    MEM32(context + 0x130u) = 2u;
    MEM32(context + 0x13Cu) = 0xABCDu;
    g_ebx = 0xA1A1A1A1u; g_esi = 0xB2B2B2B2u; g_edi = 0xC3C3C3C3u;
    g_ebp = 0xD4D4D4D4u; g_seh_ebp = 0xE5E5E5E5u;
    g_ecx = context; g_esp = 0x011A0000u;
    sub_00197C50();
    ok &= check_u32(MEM32(device + 0x200u) & 0x1000u, 0x1000u, "97C50 sets PMC enable 0x1000");
    ok &= check_u32(MEM32(device + 0x400780u), 0xABCDu, "97C50 writes masked 0x13C");
    ok &= check_u32(MEM32(device + 0x400144u), 0x10000100u,
                    "97C50 then 97AAC channel-2 CTX_CONTROL");
    ok &= check_u32(g_esp, 0x011A0004u, "97C50 RET cleanup");
    ok &= check_u32(g_ebx, 0xA1A1A1A1u, "97C50 EBX preserved");
    ok &= check_u32(g_ebp, 0xD4D4D4D4u, "97C50 EBP restored");
    return ok;
}

static uint32_t ramin_slot(uint32_t device, uint32_t index)
{
    return device + 0x700000u + (index << 4);
}

static int run_ramin_object_recovery_tests(uint32_t context, uint32_t device)
{
    int ok = 1;
    const uint32_t dest = context + 0x400u;
    const uint32_t dest2 = context + 0x420u;
    const uint32_t dest3 = context + 0x440u;
    const uint32_t dest4 = context + 0x460u;
    uint32_t out_addr = context + 0x480u;
    uint32_t out_type = context + 0x484u;
    uint32_t slot;

    MEM32(context) = device;
    g_df = 0;
    MEM32(out_addr) = 0x11111111u;
    MEM32(out_type) = 0x22222222u;
    g_ebx = 0xA0A0A0A0u; g_esi = 0xB1B1B1B1u; g_edi = 0xC2C2C2C2u;
    g_ebp = 0xD3D3D3D3u; g_seh_ebp = 0xE4E4E4E4u;
    g_esp = 0x011B0000u;
    MEM32(g_esp + 4u) = 0u;
    MEM32(g_esp + 8u) = out_addr;
    MEM32(g_esp + 12u) = out_type;
    MEM32(g_esp + 16u) = 0u;
    sub_001948B9();
    ok &= check_u32(MEM32(out_addr), 0u, "948B9 class-2 raw zero address");
    ok &= check_u32(MEM32(out_type), 2u, "948B9 class-2 type");
    ok &= check_u32(g_eax, out_type, "948B9 class-2 EAX out-class pointer");
    ok &= check_u32(g_esp, 0x011B0014u, "948B9 RET 0x10 cleanup");
    ok &= check_u32(g_ebx, 0xA0A0A0A0u, "948B9 EBX preserved");
    ok &= check_u32(g_esi, 0xB1B1B1B1u, "948B9 ESI preserved");
    ok &= check_u32(g_edi, 0xC2C2C2C2u, "948B9 EDI preserved");
    ok &= check_u32(g_ebp, 0xD3D3D3D3u, "948B9 EBP restored");
    ok &= check_u32(g_seh_ebp, 0xE4E4E4E4u, "948B9 SEH restored");

    g_esp = 0x011C0000u;
    MEM32(g_esp + 4u) = 0x81234567u;
    MEM32(g_esp + 8u) = out_addr;
    MEM32(g_esp + 12u) = out_type;
    MEM32(g_esp + 16u) = 0u;
    sub_001948B9();
    ok &= check_u32(MEM32(out_addr), 0x01234567u, "948B9 class-1 masked contiguous");
    ok &= check_u32(MEM32(out_type), 1u, "948B9 class-1 type");
    ok &= check_u32(g_eax, out_type, "948B9 class-1 EAX out-class pointer");
    ok &= check_u32(g_esp, 0x011C0014u, "948B9 class-1 RET 0x10");

    g_esp = 0x011D0000u;
    MEM32(g_esp + 4u) = 0x0A5A5A5Au;
    MEM32(g_esp + 8u) = out_addr;
    MEM32(g_esp + 12u) = out_type;
    MEM32(g_esp + 16u) = 1u;
    sub_001948B9();
    ok &= check_u32(MEM32(out_addr), 0x4A5A5A5Au, "948B9 class-3 AGP address");
    ok &= check_u32(MEM32(out_type), 3u, "948B9 class-3 type");
    ok &= check_u32(g_eax, out_type, "948B9 class-3 EAX out-class pointer");
    ok &= check_u32(g_esp, 0x011D0014u, "948B9 class-3 RET 0x10");

    /* First 0x00192090 call: handle 3, selector 0x3D, address 0, limit 0x7FFAFFF. */
    MEM32(context + 0x19Cu) = 0u;
    MEM32(dest) = 0xFFFFFFFFu;
    MEM32(dest + 4u) = 0xFFFFFFFFu;
    MEM32(dest + 8u) = 0xFFFFFFFFu;
    MEM32(dest + 12u) = 0xFFFFFFFFu;
    slot = ramin_slot(device, 0u);
    MEM32(slot) = 0xDEADBEEFu;
    MEM32(slot + 4u) = 0xDEADBEEFu;
    MEM32(slot + 8u) = 0xDEADBEEFu;
    MEM32(slot + 12u) = 0xDEADBEEFu;
    g_ebx = 0x10101010u; g_esi = 0x20202020u; g_edi = 0x30303030u;
    g_ebp = 0x40404040u; g_seh_ebp = 0x50505050u;
    g_ecx = context; g_esp = 0x011E0000u;
    MEM32(g_esp + 4u) = 3u;
    MEM32(g_esp + 8u) = 0x3Du;
    MEM32(g_esp + 12u) = 0u;
    MEM32(g_esp + 16u) = 0x7FFAFFFu;
    MEM32(g_esp + 20u) = dest;
    sub_00194913();
    ok &= check_u32(g_eax, 1u, "94913 EAX 1");
    ok &= check_u32(MEM32(context + 0x19Cu), 1u, "94913 increments instance tag");
    ok &= check_u32(MEM32(slot), 0xB03Du, "94913 class-2 RAMIN word0");
    ok &= check_u32(MEM32(slot + 4u), 0x7FFAFFFu, "94913 RAMIN limit");
    ok &= check_u32(MEM32(slot + 8u), 3u, "94913 RAMIN addr|3");
    ok &= check_u32(MEM32(slot + 12u), 3u, "94913 RAMIN addr|3 copy");
    ok &= check_u32(MEM32(dest), 3u, "94913 dest handle");
    ok &= check_u32(MEM32(dest + 4u), 0u, "94913 dest words 4-7 zero");
    ok &= check_u32(MEM16(dest + 6u), 0u, "94913 dest flags word");
    ok &= check_u32(MEM32(dest + 8u), 0x3Du, "94913 dest selector");
    ok &= check_u32(MEM32(dest + 12u), 0u, "94913 dest prior tag");
    ok &= check_u32(g_esp, 0x011E0018u, "94913 RET 0x14 cleanup");
    ok &= check_u32(g_ebx, 0x10101010u, "94913 EBX preserved");
    ok &= check_u32(g_esi, 0x20202020u, "94913 ESI preserved");
    ok &= check_u32(g_edi, 0x30303030u, "94913 EDI preserved");
    ok &= check_u32(g_ebp, 0x40404040u, "94913 EBP restored");
    ok &= check_u32(g_seh_ebp, 0x50505050u, "94913 SEH restored");

    /* Selector 2 uses class dword 2 at the next index. */
    slot = ramin_slot(device, 1u);
    g_ecx = context; g_esp = 0x011F0000u;
    MEM32(g_esp + 4u) = 5u;
    MEM32(g_esp + 8u) = 2u;
    MEM32(g_esp + 12u) = 0u;
    MEM32(g_esp + 16u) = 0x7FFAFFFu;
    MEM32(g_esp + 20u) = dest2;
    sub_00194913();
    ok &= check_u32(MEM32(context + 0x19Cu), 2u, "94913 second tag");
    ok &= check_u32(MEM32(slot), 0xB002u, "94913 selector-2 RAMIN word0");
    ok &= check_u32(MEM32(dest2), 5u, "94913 selector-2 dest handle");
    ok &= check_u32(MEM32(dest2 + 8u), 2u, "94913 selector-2 dest class");
    ok &= check_u32(MEM32(dest2 + 12u), 1u, "94913 selector-2 prior tag");
    ok &= check_u32(g_esp, 0x011F0018u, "94913 selector-2 RET 0x14");

    /* Selector 3 uses class dword 3. */
    slot = ramin_slot(device, 2u);
    g_ecx = context; g_esp = 0x01200000u;
    MEM32(g_esp + 4u) = 4u;
    MEM32(g_esp + 8u) = 3u;
    MEM32(g_esp + 12u) = 0u;
    MEM32(g_esp + 16u) = 0x7FFAFFFu;
    MEM32(g_esp + 20u) = dest3;
    sub_00194913();
    ok &= check_u32(MEM32(slot), 0xB003u, "94913 selector-3 RAMIN word0");
    ok &= check_u32(MEM32(dest3 + 8u), 3u, "94913 selector-3 dest class");
    ok &= check_u32(g_esp, 0x01200018u, "94913 selector-3 RET 0x14");

    /* Contiguous 0x80000000 path: class 1 ORs 0x20000 into word0. */
    slot = ramin_slot(device, 3u);
    g_ecx = context; g_esp = 0x01210000u;
    MEM32(g_esp + 4u) = 0xCu;
    MEM32(g_esp + 8u) = 0x3Du;
    MEM32(g_esp + 12u) = 0x80000000u;
    MEM32(g_esp + 16u) = 0x10000000u;
    MEM32(g_esp + 20u) = dest4;
    sub_00194913();
    ok &= check_u32(MEM32(context + 0x19Cu), 4u, "94913 contiguous tag");
    ok &= check_u32(MEM32(slot), 0x2B03Du, "94913 contiguous RAMIN word0");
    ok &= check_u32(MEM32(slot + 4u), 0x10000000u, "94913 contiguous limit");
    ok &= check_u32(MEM32(slot + 8u), 3u, "94913 contiguous addr|3");
    ok &= check_u32(MEM32(dest4), 0xCu, "94913 contiguous dest handle");
    ok &= check_u32(MEM32(dest4 + 12u), 3u, "94913 contiguous prior tag");
    ok &= check_u32(g_esp, 0x01210018u, "94913 contiguous RET 0x14");
    return ok;
}

static int run_surface_setup_recovery_tests(uint32_t context, uint32_t device)
{
    int ok = 1;
    const uint32_t tag = 0u;
    const uint32_t clear = device + 0x700000u + (tag << 4);
    const uint32_t dest = context + 0x500u;
    const uint32_t object = context + 0x520u;
    const uint32_t table = device + 0x1000u;
    uint32_t i;
    uint32_t slot;

    MEM32(context) = device;
    MEM32(context + 0xFCu) = 0u;
    MEM32(context + 0x100u) = 0u;
    MEM32(context + 0x104u) = 0u;
    MEM32(context + 0x108u) = tag;
    MEM32(context + 0x13Cu) = 1u;
    MEM32(context + 0x124u) = 0x1000u;
    MEM32(object + 0xCu) = 0xABCDu;
    MEM32(device + 0x2500u) = 0x11111111u;
    MEM32(device + 0x2504u) = 0x22222222u;
    MEM32(clear - 4u) = 0x13579BDFu;
    MEM32(clear + 0x37F0u) = 0x2468ACE0u;
    for (i = 0; i < 0xDFCu; i++) MEM32(clear + i * 4u) = 0xA5A5A5A5u;
    for (i = 0; i < 16u; i++) MEM32(table + i * 4u) = 0x5A5A5A5Au;
    g_df = 0;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u; g_seh_ebp = 0x55555555u;
    g_ecx = context; g_esp = 0x01220000u;
    MEM32(g_esp + 4u) = 0x80u;
    MEM32(g_esp + 8u) = 0x80u;
    MEM32(g_esp + 12u) = 8u;
    MEM32(g_esp + 16u) = object;
    sub_00196E92();
    ok &= check_u32(MEM32(clear - 4u), 0x13579BDFu, "96E92 clear lower sentinel");
    ok &= check_u32(MEM32(clear), 0u, "96E92 clear first");
    ok &= check_u32(MEM32(clear + 0x37ECu), 0u, "96E92 clear last");
    ok &= check_u32(MEM32(clear + 0x37F0u), 0x2468ACE0u, "96E92 clear upper sentinel");
    ok &= check_u32(MEM32(device + 0x700010u), tag, "96E92 tag at PRAMIN[13C]");
    ok &= check_u32(MEM32(context + 0x134u), tag, "96E92 context+134 tag");
    ok &= check_u32(MEM32(table), 0u, "96E92 instance table first");
    ok &= check_u32(MEM32(table + 0xCu), 0xABCDu, "96E92 instance table object+C");
    ok &= check_u32(MEM32(table + 0x14u), 0x86078u, "96E92 packed pitch dword");
    ok &= check_u32(MEM32(device + 0x3224u), 0x86078u, "96E92 PGRAPH 3224");
    ok &= check_u32(MEM32(device + 0x2500u), 1u, "96E92 2500 pulse");
    ok &= check_u32(MEM32(device + 0x2504u), 1u, "96E92 2504 bitmask");
    ok &= check_u32(MEM32(device + 0x3200u), 1u, "96E92 3200 enable");
    ok &= check_u32(MEM32(device + 0x3250u), 1u, "96E92 3250 enable");
    ok &= check_u32(MEM32(device + 0x3220u), 1u, "96E92 3220 when bit set");
    ok &= check_u32(MEM32(context + 0x100u), 1u, "96E92 context bit 0");
    ok &= check_u32(g_esp, 0x01220014u, "96E92 RET 0x10 cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "96E92 EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "96E92 ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "96E92 EDI preserved");
    ok &= check_u32(g_ebp, 0x44444444u, "96E92 EBP restored");
    ok &= check_u32(g_seh_ebp, 0x55555555u, "96E92 SEH restored");

    MEM32(context + 0x19Cu) = 2u;
    MEM32(context + 0xFCu) = 0xFFFFFFFFu;
    MEM32(context + 0x100u) = 0xFFFFFFFFu;
    MEM32(context + 0x10Cu) = 0xFFFFFFFFu;
    MEM32(context + 0x114u) = 0xFFFFFFFFu;
    MEM32(dest) = 0u;
    g_ebx = 0xA1A1A1A1u; g_esi = 0xB2B2B2B2u; g_edi = 0xC3C3C3C3u;
    g_ebp = 0xD4D4D4D4u; g_seh_ebp = 0xE5E5E5E5u;
    g_ecx = context; g_esp = 0x01230000u;
    MEM32(g_esp + 4u) = 0x206Eu;
    MEM32(g_esp + 8u) = 0u;
    MEM32(g_esp + 12u) = object;
    MEM32(g_esp + 16u) = 0u;
    MEM32(g_esp + 20u) = dest;
    sub_00194573();
    ok &= check_u32(g_eax, 1u, "94573 EAX 1");
    ok &= check_u32(MEM32(context + 0x108u), 2u, "94573 copies prior tag");
    ok &= check_u32(MEM32(context + 0x19Cu), 2u + 0x37Fu, "94573 tag plus 0x37F");
    ok &= check_u32(MEM32(context + 0xFCu), 0u, "94573 zeros FC before 96E92");
    ok &= check_u32(MEM32(dest), device + 0x00800000u, "94573 dest encoded address");
    ok &= check_u32(g_esp, 0x01230018u, "94573 RET 0x14 cleanup");
    ok &= check_u32(g_ebx, 0xA1A1A1A1u, "94573 EBX preserved");
    ok &= check_u32(g_esi, 0xB2B2B2B2u, "94573 ESI preserved");
    ok &= check_u32(g_edi, 0xC3C3C3C3u, "94573 EDI preserved");
    ok &= check_u32(g_ebp, 0xD4D4D4D4u, "94573 EBP preserved");

    MEM32(context + 0x12Cu) = 0x2000u;
    MEM32(context + 0xFCu) = 0u;
    g_ebx = 0x12121212u; g_esi = 0x23232323u; g_edi = 0x34343434u;
    g_ebp = 0x45454545u; g_seh_ebp = 0x56565656u;
    g_ecx = context; g_esp = 0x01240000u;
    MEM32(g_esp + 4u) = 0u;
    MEM32(g_esp + 8u) = 0x11111111u;
    MEM32(g_esp + 12u) = 0u;
    MEM32(g_esp + 16u) = 0xABu;
    MEM32(g_esp + 20u) = 0u;
    sub_0019701F();
    ok &= check_u32(MEM32(device + 0x2000u), 0x11111111u, "9701F instance word0");
    ok &= check_u32(MEM32(device + 0x2004u), 0x800000ABu, "9701F packed word1");
    ok &= check_u32(g_esp, 0x01240018u, "9701F RET 0x14 cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "9701F EBX preserved");
    ok &= check_u32(g_esi, 0x23232323u, "9701F ESI preserved");
    ok &= check_u32(g_edi, 0x34343434u, "9701F EDI preserved");
    ok &= check_u32(g_ebp, 0x45454545u, "9701F EBP restored");
    ok &= check_u32(g_seh_ebp, 0x56565656u, "9701F SEH restored");

    MEM32(object) = 0u;
    MEM32(object + 4u) = 0u;
    MEM16(object + 6u) = 0u;
    MEM32(object + 0xCu) = 0xABu;
    g_ebx = 0x61616161u; g_esi = 0x72727272u; g_edi = 0x83838383u;
    g_ebp = 0x94949494u;
    g_ecx = context; g_esp = 0x01250000u;
    MEM32(g_esp + 4u) = object;
    sub_001945D6();
    ok &= check_u32(g_eax, 1u, "945D6 EAX 1");
    ok &= check_u32(MEM32(device + 0x2000u), 0u, "945D6 hashed slot word0");
    ok &= check_u32(MEM32(device + 0x2004u), 0x800000ABu, "945D6 hashed slot word1");
    ok &= check_u32(g_esp, 0x01250008u, "945D6 RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x61616161u, "945D6 EBX preserved");
    ok &= check_u32(g_esi, 0x72727272u, "945D6 ESI preserved");
    ok &= check_u32(g_edi, 0x83838383u, "945D6 EDI preserved");
    ok &= check_u32(g_ebp, 0x94949494u, "945D6 EBP preserved");

    slot = ramin_slot(device, 5u);
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u;
    g_ecx = context; g_esp = 0x01260000u;
    MEM32(g_esp + 4u) = 5u;
    MEM32(g_esp + 8u) = 0x39u;
    sub_00197058();
    ok &= check_u32(MEM32(slot), 0x01000039u, "97058 class 0x39 word0");
    ok &= check_u32(MEM32(slot + 4u), 0u, "97058 class 0x39 word1");
    ok &= check_u32(MEM32(slot + 8u), 0u, "97058 word2 zero");
    ok &= check_u32(MEM32(slot + 12u), 0u, "97058 word3 zero");
    ok &= check_u32(g_esp, 0x0126000Cu, "97058 RET 8 cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "97058 EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "97058 ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "97058 EDI preserved");

    slot = ramin_slot(device, 6u);
    g_ecx = context; g_esp = 0x01270000u;
    MEM32(g_esp + 4u) = 6u;
    MEM32(g_esp + 8u) = 0x97u;
    sub_00197058();
    ok &= check_u32(MEM32(slot), 0x97u, "97058 class 0x97 word0");
    ok &= check_u32(MEM32(slot + 4u), 0xA00u, "97058 class 0x97 word1");
    ok &= check_u32(g_esp, 0x0127000Cu, "97058 class 0x97 RET 8");

    MEM32(context + 0xFCu) = 0u;
    MEM32(context + 0x134u) = 0u;
    MEM32(device + 0x700000u) = 0u;
    g_ebx = 0xA0A0A0A0u; g_esi = 0xB1B1B1B1u; g_edi = 0xC2C2C2C2u;
    g_ebp = 0xD3D3D3D3u; g_seh_ebp = 0xE4E4E4E4u;
    g_ecx = context; g_esp = 0x01280000u;
    sub_001970CD();
    ok &= check_u32(MEM32(device + 0x700000u) & 1u, 1u, "970CD ORs RAMIN bit 0");
    ok &= check_u32(MEM32(device + 0x70033Cu), 0xFFFF0000u, "970CD default 0x33C");
    ok &= check_u32(MEM32(device + 0x70047Cu), 0x101u, "970CD default 0x47C");
    ok &= check_u32(MEM32(device + 0x700490u), 0x111u, "970CD default 0x490");
    ok &= check_u32(MEM32(device + 0x7004A8u), 0x44400000u, "970CD default 0x4A8");
    ok &= check_u32(g_esp, 0x01280004u, "970CD RET cleanup");
    ok &= check_u32(g_ebx, 0xA0A0A0A0u, "970CD EBX preserved");
    ok &= check_u32(g_esi, 0xB1B1B1B1u, "970CD ESI preserved");
    ok &= check_u32(g_edi, 0xC2C2C2C2u, "970CD EDI preserved");
    ok &= check_u32(g_ebp, 0xD3D3D3D3u, "970CD EBP restored");
    ok &= check_u32(g_seh_ebp, 0xE4E4E4E4u, "970CD SEH restored");

    {
    const uint32_t named = context + 0x540u;
    MEM32(context + 0x19Cu) = 8u;
    MEM32(named) = 0u;
    g_df = 0;
    g_ebx = 0x10101010u; g_esi = 0x20202020u; g_edi = 0x30303030u;
    g_ebp = 0x40404040u; g_seh_ebp = 0x50505050u;
    g_ecx = context; g_esp = 0x01290000u;
    MEM32(g_esp + 4u) = 0xEu;
    MEM32(g_esp + 8u) = 0x39u;
    MEM32(g_esp + 12u) = named;
    sub_001949E2();
    ok &= check_u32(g_eax, 1u, "949E2 EAX 1");
    ok &= check_u32(MEM32(context + 0x19Cu), 9u, "949E2 class 0x39 tag plus 1");
    ok &= check_u32(MEM32(named), 0xEu, "949E2 dest handle");
    ok &= check_u32(MEM32(named + 8u), 0x39u, "949E2 dest class");
    ok &= check_u32(MEM32(named + 12u), 8u, "949E2 dest prior tag");
    ok &= check_u32(MEM16(named + 6u), 1u, "949E2 dest flags");
    ok &= check_u32(MEM32(ramin_slot(device, 8u)), 0x01000039u, "949E2 writes class 0x39 slot");
    }
    ok &= check_u32(g_esp, 0x01290010u, "949E2 RET 0xC cleanup");
    ok &= check_u32(g_ebx, 0x10101010u, "949E2 EBX preserved");
    ok &= check_u32(g_esi, 0x20202020u, "949E2 ESI preserved");
    ok &= check_u32(g_edi, 0x30303030u, "949E2 EDI preserved");
    ok &= check_u32(g_ebp, 0x40404040u, "949E2 EBP restored");

    {
    const uint32_t d3d = 0x00120000u;
    const uint32_t put = 0x00130000u;
    MEM32(0x0019DCE0u) = d3d;
    MEM32(0x0019DED0u) = 0u;
    MEM32(d3d) = 0x80001000u;
    MEM32(d3d + 8u) = 0x2000u;
    MEM32(d3d + 0x30u) = 5u;
    MEM32(d3d + 0x34u) = put;
    MEM32(d3d + 0x40u) = 0x111u;
    MEM32(d3d + 0x2ABCu) = 0x222u;
    MEM32(d3d + 0x2458u) = 0u;
    MEM32(d3d + 0x2AB8u) = 0u;
    MEM32(put) = 0xFFFFFFFFu;
    MEM32(0x0019DEC8u) = 0u;
    MEM32(0x0019DECCu) = 0u;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ebp = 0x44444444u;
    g_ecx = d3d; g_esp = 0x012A0000u;
    sub_001912A0();
    ok &= check_u32(MEM32(d3d + 0x2264u), 0x0019DE88u, "912A0 skip path 2264");
    ok &= check_u32(MEM32(0x0019DEC8u), 0x00001000u, "912A0 skip path published +40");
    ok &= check_u32(MEM32(0x0019DECCu), 0x00001000u, "912A0 skip path published +44");
    ok &= check_u32(MEM32(put), 3u, "912A0 skip path put-2");
    ok &= check_u32(MEM32(d3d + 0x2458u), 0x222u, "912A0 copies 2ABC");
    ok &= check_u32(MEM32(d3d + 0x2AB8u), 0x111u, "912A0 copies +40");
    ok &= check_u32(g_esp, 0x012A0004u, "912A0 RET cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "912A0 EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "912A0 ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "912A0 EDI preserved");
    ok &= check_u32(g_ebp, 0x44444444u, "912A0 EBP preserved");
    }
    return ok;
}

int main(void)
{
    const SIZE_T window_size = (SIZE_T)0x100000000ULL;
    const uint32_t context = 0x00100000u;
    uint8_t *window = (uint8_t *)VirtualAlloc(NULL, window_size,
                                               MEM_RESERVE, PAGE_NOACCESS);
    int ok = window != NULL;
    if (!ok) {
        fprintf(stderr, "FAIL: could not reserve guest address window (%lu)\n",
                GetLastError());
        return 2;
    }

    /* 0x00193C40 is exactly RDTSC; RET.  Use nonzero, distinct halves so the
     * guest EDX:EAX split cannot pass through duplicated host-timing behavior. */
    timestamp_fixture_value = 0x89ABCDEF01234567ULL;
    g_eax = 0xA1A2A3A4u; g_edx = 0xB1B2B3B4u;
    g_esp = 0x00180000u; g_ebx = 0x11112222u; g_esi = 0x33334444u;
    g_edi = 0x55556666u; g_ebp = 0x77778888u;
    sub_00193C40();
    ok &= check_u32(g_eax, 0x01234567u, "193C40 timestamp low half");
    ok &= check_u32(g_edx, 0x89ABCDEFu, "193C40 timestamp high half");
    ok &= check_u32(g_esp, 0x00180004u, "193C40 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x11112222u, "193C40 EBX preserved");
    ok &= check_u32(g_esi, 0x33334444u, "193C40 ESI preserved");
    ok &= check_u32(g_edi, 0x55556666u, "193C40 EDI preserved");
    ok &= check_u32(g_ebp, 0x77778888u, "193C40 EBP preserved");
    ok &= VirtualAlloc(window, 0x02000000, MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc(window + 0xFD000000u, 0x02000000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc(window + 0xFD600000u, 0x00100000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc(window + 0xFD200000u, 0x00300000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc(window + 0xFE000000u, 0x00100000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc(window + 0xFE700000u, 0x00210000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    if (!ok) {
        fprintf(stderr, "FAIL: could not commit guest/MMIO pages (%lu)\n",
                GetLastError());
        VirtualFree(window, 0, MEM_RELEASE);
        return 2;
    }
    g_xbox_mem_offset = (ptrdiff_t)(uintptr_t)window;
    ok &= run_service_loop_recovery_tests(context, 0xFD000000u);
    ok &= run_queue_helper_recovery_tests(context, 0xFD000000u);
    ok &= run_callback_reentry_recovery_tests(context, 0xFD000000u);
    ok &= run_ramin_object_recovery_tests(context, 0xFD000000u);
    ok &= run_surface_setup_recovery_tests(context, 0xFD000000u);
    ok &= VirtualAlloc((void *)XBOX_PTR((uint32_t)-44039872), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc((void *)XBOX_PTR((uint32_t)-50294464), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    ok &= VirtualAlloc((void *)XBOX_PTR(0xFD10020Cu), 0x1000,
                       MEM_COMMIT, PAGE_READWRITE) != NULL;
    MEM32(context) = 0xFD000000u;
    MEM32(0xFD001804u) = 0xA5A50010u;
    MEM32(0xFD600140u) = 0xFFFFFFFFu;
    MEM32(0xFD009140u) = 0xFFFFFFFFu;
    g_eax = 0x11111111u; g_esp = 0x00200000u;
    g_ebx = 0x22222222u; g_esi = 0x33333333u; g_edi = 0x44444444u;
    g_ecx = context;
    sub_0019460A();
    ok &= check_u32(MEM32(context), 0xFD000000u, "19460A context device");
    ok &= check_u32(MEM32(0xFD001804u), 0xA5A50014u, "19460A PCI command OR 4");
    ok &= check_u32(MEM32(0xFD600140u), 0, "19460A CRTC interrupt disable");
    ok &= check_u32(MEM32(0xFD009140u), 0, "19460A PTIMER interrupt disable");
    ok &= check_u32(g_eax, 1, "19460A return EAX");
    ok &= check_u32(g_esp, 0x00200004u, "19460A RET stack cleanup");
    ok &= check_u32(g_ebx, 0x22222222u, "19460A EBX preserved");
    ok &= check_u32(g_esi, 0x33333333u, "19460A ESI preserved");
    ok &= check_u32(g_edi, 0x44444444u, "19460A EDI preserved");

    MEM32(0xFD001800u) = 0x12345678u;
    MEM32(0xFD001808u) = 0x000000A1u;
    MEM32(0xFD10020Cu) = 0x03ABCDEFu;
    MEM32(context + 0xA4) = 0; MEM32(context + 0xA8) = 0;
    MEM32(context + 0xB8) = 0; MEM32(context + 0xBC) = 0;
    g_eax = 0x55555555u; g_esp = 0x00300000u;
    g_ebx = 0x66666666u; g_esi = 0x77777777u; g_edi = 0x88888888u;
    g_ecx = context;
    sub_00194635();
    ok &= check_u32(MEM32(context + 0xA4), (0x12345678u >> 16) & 0xFFFCu,
                    "194635 PBUS device field");
    ok &= check_u32(MEM32(context + 0xBC), 0xA1u, "194635 PBUS revision");
    ok &= check_u32(MEM32(context + 0xA8), 0x03ABCDEFu, "194635 PFB RAM size");
    ok &= check_u32(g_edx, 0x03ABCDEFu, "194635 PFB read result");
    ok &= check_u32(MEM32(context + 0xB8), 0x00FE502Au, "194635 constant");
    ok &= check_u32(g_eax, 1, "194635 return EAX");
    ok &= check_u32(g_esp, 0x00300004u, "194635 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x66666666u, "194635 EBX preserved");
    ok &= check_u32(g_esi, 0x77777777u, "194635 ESI preserved");
    ok &= check_u32(g_edi, 0x88888888u, "194635 EDI preserved");

    /* 0x00196800 decodes three PLL words with sequential truncating divides. */
    MEM32(context + 0xB8) = 1000u;
    MEM32(context + 0x150) = 0xDEADBEEFu;
    MEM32(context + 0x154) = 0xCAFEBABEu;
    MEM32(0xFD000200u) = 0u;
    MEM32(0xFD000140u) = 0x13572468u;
    MEM32(0xFD680504u) = 3u | (5u << 8) | (2u << 16); /* n=3,m=5,p=2 */
    MEM32(0xFD680508u) = 0u | (9u << 8) | (1u << 16); /* n=0 => frequency 0 */
    MEM32(0xFD680500u) = 4u | (7u << 8) | (3u << 16); /* n=4,m=7,p=3 */
    g_eax = 0x99999999u; g_esp = 0x00400000u;
    g_ebx = 0xAAAAAAA1u; g_esi = 0xBBBBBBB2u; g_edi = 0xCCCCCCC3u;
    g_ecx = context;
    sub_00196800();
    ok &= check_u32(MEM32(context + 0x150), 0u, "196800 prologue zero device state");
    ok &= check_u32(MEM32(context + 0x154), 0x13572468u, "196800 prologue MMIO-derived state");
    ok &= check_u32(MEM32(0xFD000200u), 0xFFFFFFFFu, "196800 prologue device sentinel");
    ok &= check_u32(MEM32(context + 0xCC), 3u, "196800 first n");
    ok &= check_u32(MEM32(context + 0xD0), 5u, "196800 first m");
    ok &= check_u32(MEM32(context + 0xD4), 1u, "196800 first valid");
    ok &= check_u32(MEM32(context + 0xD8), 2u, "196800 first p");
    ok &= check_u32(MEM32(context + 0xC0), 416u, "196800 first frequency");
    ok &= check_u32(MEM32(context + 0xDC), 0u, "196800 second n");
    ok &= check_u32(MEM32(context + 0xE0), 9u, "196800 second m");
    ok &= check_u32(MEM32(context + 0xE4), 1u, "196800 second valid");
    ok &= check_u32(MEM32(context + 0xE8), 1u, "196800 second p");
    ok &= check_u32(MEM32(context + 0xC4), 0u, "196800 zero n frequency");
    ok &= check_u32(MEM32(context + 0xEC), 4u, "196800 third n");
    ok &= check_u32(MEM32(context + 0xF0), 7u, "196800 third m");
    ok &= check_u32(MEM32(context + 0xF4), 1u, "196800 third valid");
    ok &= check_u32(MEM32(context + 0xF8), 3u, "196800 third p");
    ok &= check_u32(MEM32(context + 0xC8), 218u, "196800 third frequency");
    ok &= check_u32(g_eax, 218u, "196800 return EAX context+C8");
    ok &= check_u32(g_esp, 0x00400004u, "196800 RET stack cleanup");
    ok &= check_u32(g_ebx, 0xAAAAAAA1u, "196800 EBX preserved");
    ok &= check_u32(g_esi, 0xBBBBBBB2u, "196800 ESI preserved");
    ok &= check_u32(g_edi, 0xCCCCCCC3u, "196800 EDI preserved");

    /* High ctx+B8 and byte-sized m exercise defined low-32-bit IMUL wrap. */
    MEM32(context + 0xB8) = 0xF0000000u;
    MEM32(0xFD680504u) = 3u | (255u << 8); /* n=3,m=255,p=0 */
    ok &= check_u32(MEM32(context + 0xB8), 0xF0000000u, "196800 overflow ctx+B8 setup");
    ok &= check_u32(MEM32(0xFD680504u), 0x0000FF03u, "196800 overflow coefficient setup");
    g_eax = 0x77777777u; g_esp = 0x00500000u;
    g_ebx = 0x11111111u; g_esi = 0x22222222u; g_edi = 0x33333333u;
    g_ecx = context;
    sub_00196800();
    ok &= check_u32(MEM32(context + 0xC0), 0x05555555u, "196800 wrapped IMUL frequency");
    ok &= check_u32(g_eax, 0x04800000u, "196800 final frequency return EAX");
    ok &= check_u32(g_esp, 0x00500004u, "196800 wrapped RET stack cleanup");
    ok &= check_u32(g_ebx, 0x11111111u, "196800 wrapped EBX preserved");
    ok &= check_u32(g_esi, 0x22222222u, "196800 wrapped ESI preserved");
    ok &= check_u32(g_edi, 0x33333333u, "196800 wrapped EDI preserved");

    /* 0x00194520 returns the prior context tag while incrementing it. */
    MEM32(context + 0x19C) = 0x12345678u;
    g_eax = 0xAAAAAAAAu; g_esp = 0x00580000u; MEM32(g_esp + 4) = 0x20u;
    g_ecx = context; g_ebx = 0x10101010u; g_esi = 0x20202020u; g_edi = 0x30303030u;
    sub_00194520();
    ok &= check_u32(g_eax, 0x12345678u, "194520 prior EAX");
    ok &= check_u32(MEM32(context + 0x19C), 0x12345698u, "194520 incremented context");
    ok &= check_u32(g_esp, 0x00580008u, "194520 RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x10101010u, "194520 EBX preserved");
    ok &= check_u32(g_esi, 0x20202020u, "194520 ESI preserved");
    ok &= check_u32(g_edi, 0x30303030u, "194520 EDI preserved");
    MEM32(context + 0x19C) = 0xFFFFFFFCu;
    g_esp = 0x00590000u; MEM32(g_esp + 4) = 8u; g_ecx = context;
    sub_00194520();
    ok &= check_u32(g_eax, 0xFFFFFFFCu, "194520 overflow prior EAX");
    ok &= check_u32(MEM32(context + 0x19C), 4u, "194520 overflow increment");
    ok &= check_u32(g_esp, 0x00590008u, "194520 overflow RET cleanup");

    /* 0x00196967 claims 0x5000 bytes, derives the instance addresses, clears
     * exactly that range, and calls the reviewed 0x00194520 helper. */
    const uint32_t device = 0xFD000000u;
    const uint32_t base1 = 0x01000000u;
    const uint32_t clear1 = device + base1 + 0x700000u;
    MEM32(context) = device;
    MEM32(device + 0x100200u) = 0x11223344u;
    MEM32(device + 0x100204u) = 0x55667788u;
    MEM32(device + 0x1218u) = 0u; /* branch produces ctx+AC = 3 */
    MEM32(device + 0x100214u) = 0xCAFEBABDu;
    MEM32(device + 0x2210u) = 0xDEADBEEFu;
    MEM32(device + 0x2214u) = 0xDEADBEEFu;
    MEM32(context + 0x13Cu) = 0u;
    for (uint32_t off = 0; off < 0x5000u; off += 4) MEM32(clear1 + off) = 0xA5A5A5A5u;
    MEM32(clear1 - 4) = 0x13579BDFu;
    MEM32(clear1 + 0x5000u) = 0x2468ACE0u;
    claim_base = base1; claim_padding = base1; claim_bytes = 0; claim_padding_va = 0;
    g_eax = 0x11111111u; g_esp = 0x00600000u;
    g_ebx = 0x22222222u; g_esi = 0x33333333u; g_edi = 0x44444444u;
    g_ecx = context;
    MEM32(0x1C411Cu) = 0xFE000000u;
    sub_00196967();
    ok &= check_u32(claim_bytes, 0x5000u, "196967 claim byte count");
    ok &= check_u32(claim_padding_va, 0x00600000u - 8u, "196967 padding pointer");
    ok &= check_u32(MEM32(claim_padding_va), base1, "196967 padding result");
    ok &= check_u32(MEM32(context + 0x148), 0x11223344u, "196967 device field 148");
    ok &= check_u32(MEM32(context + 0x14C), 0x55667788u, "196967 device field 14C");
    ok &= check_u32(MEM32(context + 0x0AC), 3u, "196967 bit clear branch");
    ok &= check_u32(MEM32(context + 0x10), base1 - 0x5000u, "196967 base minus 5000");
    ok &= check_u32(MEM32(context + 0x12C), base1 + 0x700000u, "196967 instance base");
    ok &= check_u32(MEM32(context + 0x124), base1 + 0x701000u, "196967 instance +1000");
    ok &= check_u32(MEM32(context + 0x128), base1 + 0x701080u, "196967 instance +1080");
    ok &= check_u32(MEM32(context + 0x19C), 0x00100112u, "196967 instance tag plus helper");
    ok &= check_u32(MEM32(device + 0x2210u), 0x03000000u, "196967 device 2210");
    ok &= check_u32(MEM32(device + 0x2214u), 0x00090010u, "196967 device 2214");
    ok &= check_u32(MEM32(device + 0x100214u), 0xCAFEBABCu, "196967 device bit clear");
    ok &= check_u32(MEM32(clear1 - 4), 0x13579BDFu, "196967 clear lower sentinel");
    ok &= check_u32(MEM32(clear1 + 0x5000u), 0x2468ACE0u, "196967 clear upper sentinel");
    ok &= check_u32(MEM32(clear1), 0u, "196967 first cleared dword");
    ok &= check_u32(MEM32(clear1 + 0x4FFCu), 0u, "196967 last cleared dword");
    ok &= check_u32(MEM32(context + 0x13Cu), 0x0010010Au, "196967 helper prior result");
    ok &= check_u32(g_eax, 0x0010010Au, "196967 return EAX");
    ok &= check_u32(g_esp, 0x00600004u, "196967 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x22222222u, "196967 EBX preserved");
    ok &= check_u32(g_esi, 0x33333333u, "196967 ESI preserved");
    ok &= check_u32(g_edi, 0x44444444u, "196967 EDI preserved");

    /* The device bit set selects the alternate branch (ctx+AC = 2). */
    const uint32_t base2 = 0x01200000u;
    const uint32_t clear2 = device + base2 + 0x700000u;
    MEM32(device + 0x1218u) = 0x100u;
    for (uint32_t off = 0; off < 0x5000u; off += 4) MEM32(clear2 + off) = 0x5A5A5A5Au;
    claim_base = base2; claim_padding = base2;
    g_esp = 0x00700000u; g_ecx = context;
    sub_00196967();
    ok &= check_u32(MEM32(context + 0x0AC), 2u, "196967 bit set branch");
    ok &= check_u32(MEM32(context + 0x12C), base2 + 0x700000u, "196967 second instance base");
    ok &= check_u32(MEM32(clear2), 0u, "196967 second clear start");
    ok &= check_u32(MEM32(clear2 + 0x4FFCu), 0u, "196967 second clear end");

    /* 0x00196A65 preserves the original BL for its final branch, applies the
     * exact staged register masks, and leaves the adjacent post-RET word
     * untouched.  Exercise both original-byte paths with nonzero sentinels. */
    const uint32_t port = device + 0x6013D4u;
    const uint32_t reg8088 = device + 0x8088u;
    const uint32_t reg808c = device + 0x808Cu;
    const uint32_t reg0804 = device + 0x600804u;
    MEM8(port) = 0xA5u;
    MEM8(port + 1u) = 0x80u;
    MEM32(reg8088) = 0xA5A5A5A5u;
    MEM32(reg808c) = 0x5A5A5A5Au;
    MEM32(reg0804) = 0xFFFFFFFFu;
    MEM32(device + 0x6013D6u) = 0xC0DEC0DEu;
    g_eax = 0x11111111u; g_esp = 0x00800000u;
    g_ebx = 0x22222222u; g_esi = 0x33333333u; g_edi = 0x44444444u;
    g_ecx = context;
    sub_00196A65();
    ok &= check_u32(MEM8(port), 0x1Bu, "196A65 nonzero final selector");
    ok &= check_u32(MEM8(port + 1u), 0x05u, "196A65 nonzero final value");
    ok &= check_u32(MEM32(reg8088),
                    ((0xA5A5A5A5u & 0xFFFFF43Fu) | 0x400u) & 0xF43FFFFFu | 0x04000000u,
                    "196A65 8088 staged masks");
    ok &= check_u32(MEM32(reg808c),
                    ((0x5A5A5A5Au & 0xFFFFF40Fu) | 0x400u) & 0xF40FFFFFu | 0x04000000u,
                    "196A65 808C staged masks");
    ok &= check_u32(MEM32(reg0804), 0xFFFFFFFAu,
                    "196A65 600804 mask/set");
    ok &= check_u32(MEM32(device + 0x6013D6u), 0xC0DEC0DEu,
                    "196A65 adjacent byte isolation");
    ok &= check_u32(g_esp, 0x00800004u, "196A65 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x22222222u, "196A65 EBX preserved");
    ok &= check_u32(g_esi, 0x33333333u, "196A65 ESI preserved");
    ok &= check_u32(g_edi, 0x44444444u, "196A65 EDI preserved");

    MEM8(port) = 0xA5u;
    MEM8(port + 1u) = 0x00u;
    MEM32(reg8088) = 0xFFFFFFFFu;
    MEM32(reg808c) = 0x00000000u;
    MEM32(reg0804) = 0x00000005u;
    MEM32(device + 0x6013D6u) = 0xFACEB00Cu;
    g_eax = 0x55555555u; g_esp = 0x00900000u;
    g_ebx = 0x66666666u; g_esi = 0x77777777u; g_edi = 0x88888888u;
    g_ecx = context;
    sub_00196A65();
    ok &= check_u32(MEM8(port), 0x1Fu, "196A65 zero final selector");
    ok &= check_u32(MEM8(port + 1u), 0x99u, "196A65 zero final value");
    ok &= check_u32(MEM32(reg8088),
                    ((0xFFFFFFFFu & 0xFFFFF43Fu) | 0x400u) & 0xF43FFFFFu | 0x04000000u,
                    "196A65 8088 zero-path masks");
    ok &= check_u32(MEM32(reg808c),
                    ((0x00000000u & 0xFFFFF40Fu) | 0x400u) & 0xF40FFFFFu | 0x04000000u,
                    "196A65 808C zero-path masks");
    ok &= check_u32(MEM32(reg0804), 0x2u, "196A65 600804 zero-path mask/set");
    ok &= check_u32(MEM32(device + 0x6013D6u), 0xFACEB00Cu,
                    "196A65 zero-path adjacent isolation");
    ok &= check_u32(g_esp, 0x00900004u, "196A65 zero-path RET cleanup");
    ok &= check_u32(g_ebx, 0x66666666u, "196A65 zero-path EBX preserved");
    ok &= check_u32(g_esi, 0x77777777u, "196A65 zero-path ESI preserved");
    ok &= check_u32(g_edi, 0x88888888u, "196A65 zero-path EDI preserved");

    /* 0x00194533 writes three byte identity ramps selected by arg*0x300.
     * The source body is generated from the reviewed XBE span; exercise both
     * the zero group and a nonzero group, including exact boundary bytes. */
    const uint32_t ramp_base = context + 0x214u;
    const uint32_t ramp_group0_end = ramp_base + 0x300u;
    const uint32_t ramp_group2 = ramp_base + 0x600u;
    MEM8(ramp_base - 1u) = 0xA5u;
    MEM8(ramp_group0_end) = 0x5Au;
    for (uint32_t off = 0; off < 0x300u; ++off) MEM8(ramp_base + off) = 0xCCu;
    g_eax = 0x12345678u; g_esp = 0x00A00000u;
    g_ebx = 0x11112222u; g_esi = 0x33334444u; g_edi = 0x55556666u;
    g_ecx = context;
    MEM32(g_esp + 4u) = 0u;
    sub_00194533();
    ok &= check_u32(MEM8(ramp_base - 1u), 0xA5u, "194533 arg0 lower sentinel");
    ok &= check_u32(MEM8(ramp_group0_end), 0x5Au, "194533 arg0 upper sentinel");
    for (uint32_t channel = 0; channel < 3u; ++channel) {
        for (uint32_t byte = 0; byte < 0x100u; ++byte) {
            ok &= check_u32(MEM8(ramp_base + channel * 0x100u + byte), byte,
                            "194533 arg0 ramp byte");
        }
    }
    ok &= check_u32(g_eax, ramp_base + 0x1FFu, "194533 arg0 return EAX");
    ok &= check_u32(g_esp, 0x00A00008u, "194533 arg0 RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x11112222u, "194533 arg0 EBX preserved");
    ok &= check_u32(g_esi, 0x33334444u, "194533 arg0 ESI preserved");
    ok &= check_u32(g_edi, 0x55556666u, "194533 arg0 EDI preserved");

    MEM8(ramp_group2 - 1u) = 0x6Bu;
    MEM8(ramp_group2 + 0x300u) = 0x7Cu;
    for (uint32_t off = 0; off < 0x300u; ++off) MEM8(ramp_group2 + off) = 0xDDu;
    g_eax = 0x87654321u; g_esp = 0x00B00000u;
    g_ebx = 0x9999AAAAu; g_esi = 0xBBBBCCCCu; g_edi = 0xDDDDEEEEu;
    g_ecx = context;
    MEM32(g_esp + 4u) = 2u;
    sub_00194533();
    ok &= check_u32(MEM8(ramp_group2 - 1u), 0x6Bu, "194533 arg2 lower sentinel");
    ok &= check_u32(MEM8(ramp_group2 + 0x300u), 0x7Cu, "194533 arg2 upper sentinel");
    for (uint32_t channel = 0; channel < 3u; ++channel) {
        for (uint32_t byte = 0; byte < 0x100u; ++byte) {
            ok &= check_u32(MEM8(ramp_group2 + channel * 0x100u + byte), byte,
                            "194533 arg2 ramp byte");
        }
    }
    for (uint32_t channel = 0; channel < 3u; ++channel) {
        ok &= check_u32(MEM8(ramp_base + channel * 0x100u + 0x00u), 0u,
                        "194533 unrelated group start preserved");
        ok &= check_u32(MEM8(ramp_base + channel * 0x100u + 0xFFu), 0xFFu,
                        "194533 unrelated group end preserved");
    }
    ok &= check_u32(g_eax, ramp_group2 + 0x1FFu, "194533 arg2 return EAX");
    ok &= check_u32(g_esp, 0x00B00008u, "194533 arg2 RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x9999AAAAu, "194533 arg2 EBX preserved");
    ok &= check_u32(g_esi, 0xBBBBCCCCu, "194533 arg2 ESI preserved");
    ok &= check_u32(g_edi, 0xDDDDEEEEu, "194533 arg2 EDI preserved");

    /* 0x00196B34 writes two four-byte-spaced entries through one advancing
     * base.  The source fields are interleaved in the loop, so verify the
     * final map at every written address and preserve adjacent sentinels. */
    const uint32_t table = device + 0x8918u;
    const uint32_t table_before = table - 0x0Cu;
    const uint32_t table_after = table + 0x30u;
    MEM32(context) = device;
    MEM32(table_before) = 0xA1A2A3A4u;
    MEM32(table_after) = 0xB1B2B3B4u;
    for (uint32_t off = 0; off < 0x30u; off += 4u) MEM32(table + off) = 0xCCCC0000u + off;
    g_eax = 0x12345678u; g_esp = 0x00C00000u;
    g_ebx = 0x11112222u; g_esi = 0x33334444u; g_edi = 0x55556666u;
    g_ecx = context;
    sub_00196B34();
    ok &= check_u32(MEM32(table_before), 0xA1A2A3A4u, "196B34 lower adjacent sentinel");
    ok &= check_u32(MEM32(table_after), 0xB1B2B3B4u, "196B34 upper adjacent sentinel");
    ok &= check_u32(MEM32(device + 0x8910u), 0x00001000u, "196B34 first minus eight");
    ok &= check_u32(MEM32(device + 0x8914u), 0x00001000u, "196B34 second minus four");
    ok &= check_u32(MEM32(device + 0x8918u), 0x00001000u, "196B34 first zero");
    ok &= check_u32(MEM32(device + 0x891Cu), 0x00001000u, "196B34 second zero plus four");
    ok &= check_u32(MEM32(device + 0x8928u), 0xFFFFFFFFu, "196B34 first plus ten");
    ok &= check_u32(MEM32(device + 0x892Cu), 0xFFFFFFFFu, "196B34 second plus ten plus four");
    ok &= check_u32(MEM32(device + 0x8930u), 0u, "196B34 first plus eighteen");
    ok &= check_u32(MEM32(device + 0x8934u), 0u, "196B34 second plus eighteen plus four");
    ok &= check_u32(MEM32(device + 0x8938u), 0x00100000u, "196B34 first plus thirty two");
    ok &= check_u32(MEM32(device + 0x893Cu), 0x00100000u, "196B34 second plus thirty two plus four");
    ok &= check_u32(MEM32(device + 0x8940u), 0x00100000u, "196B34 first plus forty");
    ok &= check_u32(MEM32(device + 0x8944u), 0x00100000u, "196B34 second plus forty plus four");
    ok &= check_u32(g_esp, 0x00C00004u, "196B34 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x11112222u, "196B34 EBX preserved");
    ok &= check_u32(g_esi, 0x33334444u, "196B34 ESI preserved");
    ok &= check_u32(g_edi, 0x55556666u, "196B34 EDI preserved");

    /* 0x00196B6A stores the tag low halfword and clears exactly two
     * instance-derived dwords.  The normal tag exercises the direct path. */
    const uint32_t clear_tag_device = device;
    const uint32_t clear_tag = 0x00100112u;
    const uint32_t clear_addr = clear_tag_device + ((clear_tag + 0x70000u) << 4);
    MEM32(context) = clear_tag_device;
    MEM32(context + 0x13Cu) = clear_tag;
    MEM32(clear_tag_device + 0x400780u) = 0xA5A5A5A5u;
    MEM32(clear_addr - 4u) = 0x13579BDFu;
    MEM32(clear_addr) = 0xCCCCCCCCu;
    MEM32(clear_addr + 4u) = 0xDDDDDDDDu;
    MEM32(clear_addr + 8u) = 0x2468ACE0u;
    MEM32(clear_tag_device + 0x400784u) = 0xFACEB00Cu;
    g_eax = 0x11111111u; g_esp = 0x00D00000u;
    g_ebx = 0x22222222u; g_esi = 0x33333333u; g_edi = 0x44444444u;
    g_ecx = context;
    sub_00196B6A();
    ok &= check_u32(MEM32(clear_tag_device + 0x400780u), clear_tag & 0xFFFFu,
                    "196B6A normal low16 dword");
    ok &= check_u32(MEM32(clear_addr - 4u), 0x13579BDFu,
                    "196B6A normal lower sentinel");
    ok &= check_u32(MEM32(clear_addr), 0u, "196B6A normal first clear");
    ok &= check_u32(MEM32(clear_addr + 4u), 0u, "196B6A normal second clear");
    ok &= check_u32(MEM32(clear_addr + 8u), 0x2468ACE0u,
                    "196B6A normal upper sentinel");
    ok &= check_u32(MEM32(clear_tag_device + 0x400784u), 0xFACEB00Cu,
                    "196B6A normal unrelated device state");
    ok &= check_u32(g_esp, 0x00D00004u, "196B6A RET stack cleanup");
    ok &= check_u32(g_ebx, 0x22222222u, "196B6A EBX preserved");
    ok &= check_u32(g_esi, 0x33333333u, "196B6A ESI preserved");
    ok &= check_u32(g_edi, 0x44444444u, "196B6A EDI preserved");

    /* A high tag forces both the tag addition and shifted address through
     * the original 32-bit x86 wraparound path. */
    const uint32_t wrap_tag = 0xFFFFFFF0u;
    const uint32_t wrap_addr = clear_tag_device + ((wrap_tag + 0x70000u) << 4);
    MEM32(context + 0x13Cu) = wrap_tag;
    MEM32(clear_tag_device + 0x400780u) = 0x5A5A5A5Au;
    MEM32(wrap_addr - 4u) = 0x10203040u;
    MEM32(wrap_addr) = 0xAAAAAAAAu;
    MEM32(wrap_addr + 4u) = 0xBBBBBBBBu;
    MEM32(wrap_addr + 8u) = 0x50607080u;
    g_eax = 0x55555555u; g_esp = 0x00E00000u;
    g_ebx = 0x66666666u; g_esi = 0x77777777u; g_edi = 0x88888888u;
    g_ecx = context;
    sub_00196B6A();
    ok &= check_u32(MEM32(clear_tag_device + 0x400780u), 0xFFF0u,
                    "196B6A wrapped low16 dword");
    ok &= check_u32(MEM32(wrap_addr - 4u), 0x10203040u,
                    "196B6A wrapped lower sentinel");
    ok &= check_u32(MEM32(wrap_addr), 0u, "196B6A wrapped first clear");
    ok &= check_u32(MEM32(wrap_addr + 4u), 0u, "196B6A wrapped second clear");
    ok &= check_u32(MEM32(wrap_addr + 8u), 0x50607080u,
                    "196B6A wrapped upper sentinel");
    ok &= check_u32(g_esp, 0x00E00004u, "196B6A wrapped RET stack cleanup");
    ok &= check_u32(g_ebx, 0x66666666u, "196B6A wrapped EBX preserved");
    ok &= check_u32(g_esi, 0x77777777u, "196B6A wrapped ESI preserved");
    ok &= check_u32(g_edi, 0x88888888u, "196B6A wrapped EDI preserved");

    /* 0x00196B9A writes the default context values and clears the exact
     * device register list in original instruction order.  Use distinct
     * sentinels so omitted or misplaced writes remain visible. */
    const uint32_t defaults_device = 0xFD000000u;
    const uint32_t defaults_source = 0xCAFEBABEu;
    static const uint32_t default_offsets[] = {
        0x3210u, 0x3270u, 0x3240u, 0x3244u, 0x3058u, 0x3258u,
        0x2508u, 0x250Cu, 0x3228u, 0x2410u, 0x2420u
    };
    MEM32(context) = defaults_device;
    MEM32(context + 0x100u) = defaults_source;
    MEM32(context + 0x118u) = 0xA1A1A1A1u;
    MEM32(context + 0x11Cu) = 0xA2A2A2A2u;
    MEM32(context + 0x120u) = 0xA3A3A3A3u;
    MEM32(defaults_device + 0x2504u) = 0xB4B4B4B4u;
    for (uint32_t i = 0; i < sizeof(default_offsets) / sizeof(default_offsets[0]); ++i)
        MEM32(defaults_device + default_offsets[i]) = 0xD0000000u + i;
    MEM32(defaults_device + 0x2500u) = 0xE5E5E5E5u;
    MEM32(defaults_device + 0x2510u) = 0xE6E6E6E6u;
    g_eax = 0x11111111u; g_esp = 0x00F00000u;
    g_ebx = 0x12121212u; g_esi = 0x13131313u; g_edi = 0x14141414u;
    g_ebp = 0x15151515u;
    g_ecx = context;
    sub_00196B9A();
    ok &= check_u32(MEM32(context + 0x11Cu), 0xFFu, "196B9A context +11C default");
    ok &= check_u32(MEM32(context + 0x120u), 0x00800000u, "196B9A context +120 default");
    ok &= check_u32(MEM32(context + 0x118u), 0x01111111u, "196B9A context +118 default");
    ok &= check_u32(MEM32(context + 0x100u), defaults_source, "196B9A source preserved");
    ok &= check_u32(MEM32(defaults_device + 0x2504u), defaults_source, "196B9A copied source");
    for (uint32_t i = 0; i < sizeof(default_offsets) / sizeof(default_offsets[0]); ++i)
        ok &= check_u32(MEM32(defaults_device + default_offsets[i]), 0u,
                        "196B9A cleared register");
    ok &= check_u32(MEM32(defaults_device + 0x2500u), 0xE5E5E5E5u,
                    "196B9A lower unrelated sentinel");
    ok &= check_u32(MEM32(defaults_device + 0x2510u), 0xE6E6E6E6u,
                    "196B9A upper unrelated sentinel");
    ok &= check_u32(g_eax, defaults_device, "196B9A EAX device return");
    ok &= check_u32(g_esp, 0x00F00004u, "196B9A RET stack cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "196B9A EBX preserved");
    ok &= check_u32(g_esi, 0x13131313u, "196B9A ESI preserved");
    ok &= check_u32(g_edi, 0x14141414u, "196B9A EDI preserved");
    ok &= check_u32(g_ebp, 0x15151515u, "196B9A EBP preserved");

    /* 0x00196E80 writes the two device setup registers and leaves the loaded
     * device pointer observable in EAX.  Distinct surrounding sentinels make
     * omissions or an off-by-four target visible. */
    const uint32_t setup_device = 0xFD000000u;
    const uint32_t setup_first = setup_device + 0x600100u;
    const uint32_t setup_second = setup_device + 0x600140u;
    MEM32(context) = setup_device;
    MEM32(setup_first - 4u) = 0xA1A2A3A4u;
    MEM32(setup_first) = 0xB1B2B3B4u;
    MEM32(setup_first + 4u) = 0xC1C2C3C4u;
    MEM32(setup_second - 4u) = 0xD1D2D3D4u;
    MEM32(setup_second) = 0xE1E2E3E4u;
    MEM32(setup_second + 4u) = 0xF1F2F3F4u;
    g_eax = 0x11111111u; g_esp = 0x00F80000u;
    g_ebx = 0x12121212u; g_esi = 0x13131313u; g_edi = 0x14141414u;
    g_ebp = 0x15151515u;
    g_ecx = context;
    sub_00196E80();
    ok &= check_u32(MEM32(setup_first - 4u), 0xA1A2A3A4u,
                    "196E80 first lower sentinel");
    ok &= check_u32(MEM32(setup_first), 1u, "196E80 first register");
    ok &= check_u32(MEM32(setup_first + 4u), 0xC1C2C3C4u,
                    "196E80 first upper sentinel");
    ok &= check_u32(MEM32(setup_second - 4u), 0xD1D2D3D4u,
                    "196E80 second lower sentinel");
    ok &= check_u32(MEM32(setup_second), 1u, "196E80 second register");
    ok &= check_u32(MEM32(setup_second + 4u), 0xF1F2F3F4u,
                    "196E80 second upper sentinel");
    ok &= check_u32(g_eax, setup_device, "196E80 device return EAX");
    ok &= check_u32(g_esp, 0x00F80004u, "196E80 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "196E80 EBX preserved");
    ok &= check_u32(g_esi, 0x13131313u, "196E80 ESI preserved");
    ok &= check_u32(g_edi, 0x14141414u, "196E80 EDI preserved");
    ok &= check_u32(g_ebp, 0x15151515u, "196E80 EBP preserved");

    /* 0x00196C83 snapshots thirteen device fields into the old slot, loads
     * the raw requested slot, and restores the saved state words. */
    static const uint32_t channel_fields[] = {
        0x3240u, 0x3244u, 0x3248u, 0x322Cu, 0x3228u, 0x3224u,
        0x3280u, 0x3254u, 0x3268u, 0x3264u, 0x3260u, 0x326Cu, 0x324Cu
    };
    const uint32_t channel_table = 0x01800000u;
    const uint32_t channel_table_offset = channel_table - device;
    MEM32(context + 0x124u) = channel_table_offset;
    MEM32(context + 0x100u) = 0u;
    MEM32(device + 0x2500u) = 0x2500A001u;
    MEM32(device + 0x3200u) = 0x3200A002u;
    MEM32(device + 0x3250u) = 0x3250A003u;
    MEM32(device + 0x3204u) = 0u;
    MEM32(device + 0x2508u) = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i) {
        MEM32(device + channel_fields[i]) = 0xA0000000u + i;
        MEM32(channel_table + 0x40u + i * 4u) = 0xB0000000u + i;
    }
    MEM32(channel_table + 0x3Cu) = 0xA1A1A1A1u;
    MEM32(channel_table + 0x80u - 4u) = 0xB2B2B2B2u;
    MEM32(device + 0x204Cu) = 0xCC1CC1CCu;
    g_eax = 0xEEEEEEEEu; g_esp = 0x00F90000u;
    g_ebx = 0x12121212u; g_esi = 0x13131313u; g_edi = 0x14141414u;
    g_ebp = 0xA5A5A5A5u; g_seh_ebp = 0x5A5A5A5Au;
    g_ecx = context; MEM32(g_esp + 4u) = 1u;
    sub_00196C83();
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i) {
        ok &= check_u32(MEM32(channel_table + i * 4u), 0xA0000000u + i,
                        "196C83 arg1 saved field");
        ok &= check_u32(MEM32(device + channel_fields[i]), 0xB0000000u + i,
                        "196C83 arg1 loaded field");
    }
    ok &= check_u32(MEM32(channel_table + 0x3Cu), 0xA1A1A1A1u,
                    "196C83 slot lower sentinel");
    ok &= check_u32(MEM32(channel_table + 0x80u - 4u), 0xB2B2B2B2u,
                    "196C83 slot upper sentinel");
    ok &= check_u32(MEM32(device + 0x2500u), 0x2500A001u, "196C83 restore 2500");
    ok &= check_u32(MEM32(device + 0x3200u), 0x3200A002u, "196C83 restore 3200");
    ok &= check_u32(MEM32(device + 0x3250u), 0x3250A003u, "196C83 restore 3250");
    ok &= check_u32(MEM32(device + 0x204Cu), 0x001FFFFFu, "196C83 204C mask");
    ok &= check_u32(g_eax, device, "196C83 device return EAX");
    ok &= check_u32(g_esp, 0x00F90008u, "196C83 RET 4 cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "196C83 EBX preserved");
    ok &= check_u32(g_esi, 0x13131313u, "196C83 ESI preserved");
    ok &= check_u32(g_edi, 0x14141414u, "196C83 EDI preserved");
    ok &= check_u32(g_ebp, 0xA5A5A5A5u, "196C83 arg1 EBP metadata restored");
    ok &= check_u32(g_seh_ebp, 0x5A5A5A5Au, "196C83 arg1 SEH metadata restored");

    /* Explicit equal old-0x100 branch: equal first two fields leaves bit zero
     * clear when the requested channel is already selected. */
    MEM32(device + 0x3204u) = 0x100u; MEM32(device + 0x2508u) = 0u;
    MEM32(device + 0x3244u) = MEM32(device + 0x3240u);
    g_esp = 0x00F98000u; MEM32(g_esp + 4u) = 1u; g_ecx = context;
    sub_00196C83();
    ok &= check_u32(MEM32(device + 0x2508u), 0u,
                    "196C83 old 100 equal branch");

    /* Old index 0x100 exercises unequal/equal branches. */
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i) {
        MEM32(device + channel_fields[i]) = 0xC0000000u + i;
        MEM32(channel_table + 0x80u + i * 4u) = 0xD0000000u + i;
    }
    MEM32(device + 0x3204u) = 0x100u; MEM32(device + 0x2508u) = 0u;
    MEM32(context + 0x100u) = 0u; g_esp = 0x00FA0000u;
    MEM32(g_esp + 4u) = 2u; g_ecx = context; sub_00196C83();
    ok &= check_u32(MEM32(device + 0x2508u), 1u, "196C83 old 100 unequal");
    ok &= check_u32(MEM32(device + 0x3204u), 2u, "196C83 channel 2 index");
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
        ok &= check_u32(MEM32(device + channel_fields[i]), 0xD0000000u + i,
                        "196C83 channel 2 field");
    /* Channels 0, 1, 2 and 31 are bounded by the table; requested bit 2
     * promotes channel 2, and raw 32 still addresses table+0x800. */
    MEM32(context + 0x100u) = 1u << 2;
    for (uint32_t channel = 0; channel <= 2u; ++channel) {
        for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
            MEM32(channel_table + channel * 0x40u + i * 4u) =
                0x50000000u + channel * 0x100u + i;
        g_esp = 0x00FC0000u; MEM32(g_esp + 4u) = channel; g_ecx = context;
        sub_00196C83();
        if (channel == 2u)
            ok &= check_u32(MEM32(device + 0x3204u), 0x102u,
                            "196C83 requested bit 2 promotion");
        for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
            ok &= check_u32(MEM32(device + channel_fields[i]),
                            0x50000000u + channel * 0x100u + i,
                            "196C83 bounded channel field");
    }
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
        MEM32(channel_table + 31u * 0x40u + i * 4u) = 0x6F000000u + i;
    g_esp = 0x00FD0000u; MEM32(g_esp + 4u) = 31u; g_ecx = context;
    sub_00196C83();
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
        ok &= check_u32(MEM32(device + channel_fields[i]), 0x6F000000u + i,
                        "196C83 channel 31 field");
    MEM32(context + 0x100u) = 1u;
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
        MEM32(channel_table + 0x800u + i * 4u) = 0x72000000u + i;
    g_esp = 0x00FE0000u; MEM32(g_esp + 4u) = 32u; g_ecx = context;
    g_ebp = 0xB6B6B6B6u; g_seh_ebp = 0x6B6B6B6Bu;
    sub_00196C83();
    ok &= check_u32(MEM32(device + 0x3204u), 0x100u,
                    "196C83 raw32 active plus bit0");
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i)
        ok &= check_u32(MEM32(device + channel_fields[i]), 0x72000000u + i,
                        "196C83 raw32 slot field");
    ok &= check_u32(g_esp, 0x00FE0008u, "196C83 raw32 RET cleanup");
    ok &= check_u32(g_ebp, 0xB6B6B6B6u, "196C83 raw32 EBP metadata restored");
    ok &= check_u32(g_seh_ebp, 0x6B6B6B6Bu, "196C83 raw32 SEH metadata restored");

    /* 0x00197BCF initializes the device's channel-1 setup state, delegates
     * the channel snapshot/switch to the recovered 0x00196C83 helper, then
     * applies its final active-channel flags.  Check the representative old
     * slot as well as every final field touched by the straight-line body. */
    MEM32(context + 0x11Cu) = 0x12345ABCu;
    MEM32(context + 0x100u) = 0u;
    MEM32(context + 0x124u) = channel_table_offset;
    MEM32(device + 0x3204u) = 0u;
    MEM32(device + 0x2508u) = 0u;
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i) {
        MEM32(device + channel_fields[i]) = 0xC1000000u + i;
        MEM32(channel_table + 0x40u + i * 4u) = 0xD1000000u + i;
        MEM32(channel_table + i * 4u) = 0xE1000000u + i;
    }
    /* +0x3224 is one of the channel snapshot fields, so the requested
     * channel's value is the ordered post-call value of the setup write. */
    MEM32(channel_table + 0x40u + 5u * 4u) = 0x000F0078u;
    MEM32(channel_table - 4u) = 0xE1E1E1E1u;
    MEM32(channel_table + 0x40u + sizeof(channel_fields)) = 0xD1D1D1D1u;
    MEM32(device + 0x3224u) = 0xAAAAAAAAu;
    MEM32(device + 0x2044u) = 0xBBBBBBBBu;
    MEM32(device + 0x2040u) = 0xCCCCCCCCu;
    MEM32(device + 0x2500u) = 0xDDDDDDDDu;
    MEM32(device + 0x3000u) = 0xEEEEEEEEu;
    MEM32(device + 0x3050u) = 0xFFFFFFFFu;
    MEM32(device + 0x3200u) = 0x11111111u;
    MEM32(device + 0x3250u) = 0x22222222u;
    MEM32(device + 0x3220u) = 0x33333333u;
    MEM32(device + 0x3210u) = 0x44444444u;
    MEM32(device + 0x3270u) = 0x55555555u;
    g_eax = 0xEEEEEEEEu; g_esp = 0x00FF0000u;
    g_ebx = 0x12121212u; g_esi = 0x13131313u; g_edi = 0x14141414u;
    g_ebp = 0xA7A7A7A7u; g_seh_ebp = 0x7A7A7A7Au; g_ecx = context;
    sub_00197BCF();
    ok &= check_u32(MEM32(device + 0x3224u), 0x000F0078u, "197BCF 3224 setup");
    ok &= check_u32(MEM32(device + 0x2044u), 0x0101FFFFu, "197BCF 2044 setup");
    ok &= check_u32(MEM32(device + 0x2040u), 0x000002BCu, "197BCF masked context setup");
    ok &= check_u32(MEM32(device + 0x3000u), 0u, "197BCF 3000 clear");
    ok &= check_u32(MEM32(device + 0x3050u), 0u, "197BCF 3050 clear");
    ok &= check_u32(MEM32(device + 0x3210u), 0u, "197BCF 3210 clear");
    ok &= check_u32(MEM32(device + 0x3270u), 0u, "197BCF 3270 clear");
    ok &= check_u32(MEM32(device + 0x3220u), 0u, "197BCF 3220 clear");
    ok &= check_u32(MEM32(device + 0x3250u), 1u, "197BCF final 3250");
    ok &= check_u32(MEM32(device + 0x3200u), 1u, "197BCF final 3200");
    ok &= check_u32(MEM32(device + 0x2500u), 0u, "197BCF final 2500 clear");
    for (uint32_t i = 0; i < sizeof(channel_fields) / sizeof(channel_fields[0]); ++i) {
        ok &= check_u32(MEM32(channel_table + i * 4u),
                        i == 5u ? 0x000F0078u : 0xC1000000u + i,
                        "197BCF channel-0 saved field");
        ok &= check_u32(MEM32(device + channel_fields[i]),
                        i == 5u ? 0x000F0078u : 0xD1000000u + i,
                        "197BCF channel-1 restored field");
    }
    ok &= check_u32(MEM32(channel_table - 4u), 0xE1E1E1E1u,
                    "197BCF saved-slot lower sentinel");
    ok &= check_u32(MEM32(channel_table + 0x40u + sizeof(channel_fields)), 0xD1D1D1D1u,
                    "197BCF requested-slot upper sentinel");
    ok &= check_u32(g_eax, device, "197BCF device return EAX");
    ok &= check_u32(g_esp, 0x00FF0004u, "197BCF RET stack cleanup");
    ok &= check_u32(g_ebx, 0x12121212u, "197BCF EBX preserved");
    ok &= check_u32(g_esi, 0x13131313u, "197BCF ESI preserved");
    ok &= check_u32(g_edi, 0x14141414u, "197BCF EDI preserved");
    ok &= check_u32(g_ebp, 0xA7A7A7A7u, "197BCF EBP preserved");
    ok &= check_u32(g_seh_ebp, 0x7A7A7A7Au, "197BCF SEH metadata preserved");

    /* Integrated 0x00194676 setup path: exercise the recovered callees in
     * their original order, including PLL reduction and the allocation seam. */
    MEM32(context) = device;
    MEM32(context + 0xB0u) = 0xA1B2C3D4u;
    MEM32(context + 0xB8u) = 1000u;
    MEM32(context + 0xC8u) = 0x5A5A5A5Au;
    MEM32(context + 0x130u) = 0u;
    MEM32(context + 0x19Cu) = 0u;
    MEM32(0xFD680504u) = 3u | (5u << 8) | (2u << 16);
    MEM32(0xFD680508u) = 0u | (9u << 8) | (1u << 16);
    MEM32(0xFD680500u) = 4u | (7u << 8) | (3u << 16);
    MEM32(device + 0x100200u) = 0x11223344u;
    MEM32(device + 0x100204u) = 0x55667788u;
    MEM32(device + 0x1218u) = 0u;
    MEM32(device + 0x100214u) = 0xCAFEBABDu;
    MEM32(device + 0x2210u) = 0xDEADBEEFu;
    MEM32(device + 0x2214u) = 0xDEADBEEFu;
    MEM8(device + 0x6013D4u) = 0xA5u;
    MEM8(device + 0x6013D5u) = 0x80u;
    MEM32(device + 0x8088u) = 0xA5A5A5A5u;
    MEM32(device + 0x808Cu) = 0x5A5A5A5Au;
    MEM32(device + 0x600804u) = 0xFFFFFFFFu;
    MEM32(device + 0x400780u) = 0u;
    MEM32(device + 0x9420u) = 0x13579BDFu;
    const uint32_t integrated_base = 0x01300000u;
    const uint32_t integrated_clear = device + integrated_base + 0x700000u;
    for (uint32_t off = 0; off < 0x5000u; off += 4u)
        MEM32(integrated_clear + off) = 0xA5A5A5A5u;
    claim_base = integrated_base;
    claim_padding = integrated_base;
    claim_bytes = 0u;
    claim_padding_va = 0u;
    g_eax = 0xDEADBEEFu; g_esp = 0x01000000u;
    g_ebx = 0x21212121u; g_esi = 0x32323232u; g_edi = 0x43434343u;
    g_ebp = 0x54545454u; g_ecx = context;
    MEM32(0x1C411Cu) = 0xFE000000u;
    sub_00194676();
    ok &= check_u32(MEM32(context + 0xC8u), 218u,
                    "194676 PLL final frequency input");
    ok &= check_u32(MEM32(context + 0xB0u), 1u,
                    "194676 context clock-ready flag");
    ok &= check_u32(MEM32(device + 0x9200u), 0u,
                    "194676 reduced PTIMER numerator");
    ok &= check_u32(MEM32(device + 0x9210u), 61035u,
                    "194676 reduced PTIMER denominator");
    ok &= check_u32(MEM32(device + 0x9420u), 0xFFFFFFFFu,
                    "194676 PTIMER alarm prewrite");
    ok &= check_u32(MEM32(device + 0x1830u), 0u, "194676 setup state zero");
    ok &= check_u32(MEM32(device + 0x180Cu), 0xF800u, "194676 setup state F800");
    ok &= check_u32(claim_bytes, 0x5000u, "194676 allocation size");
    ok &= check_u32(MEM32(context + 0x12Cu), integrated_base + 0x700000u,
                    "194676 instance base");
    ok &= check_u32(MEM32(context + 0x19Cu), 0x00130112u,
                    "194676 context tag increment");
    ok &= check_u32(MEM32(device + 0x2210u), 0x03000000u,
                    "194676 instance register 2210");
    ok &= check_u32(MEM32(device + 0x2214u), 0x00090010u,
                    "194676 instance register 2214");
    ok &= check_u32(MEM32(integrated_clear), 0u,
                    "194676 instance clear first");
    ok &= check_u32(MEM32(integrated_clear + 0x4FFCu), 0u,
                    "194676 instance clear last");
    ok &= check_u32(MEM8(device + 0x6013D4u), 0x1Bu,
                    "194676 port selector");
    ok &= check_u32(MEM8(device + 0x6013D5u), 0x05u,
                    "194676 port value");
    ok &= check_u32(MEM8(context + 0x214u), 0u, "194676 identity ramp first");
    ok &= check_u32(MEM8(context + 0x313u), 0xFFu, "194676 identity ramp last");
    ok &= check_u32(MEM32(device + 0x8910u), 0x1000u,
                    "194676 register table");
    ok &= check_u32(MEM32(context + 0x130u), 2u, "194676 context mode");
    ok &= check_u32(MEM32(context + 0x11Cu), 0xFFu,
                    "194676 default context 11C");
    ok &= check_u32(MEM32(context + 0x120u), 0x00800000u,
                    "194676 default context 120");
    ok &= check_u32(MEM32(device + 0x3210u), 0u,
                    "194676 final defaults");
    ok &= check_u32(g_eax, 1u, "194676 final EAX");
    ok &= check_u32(g_esp, 0x01000004u, "194676 RET stack cleanup");
    ok &= check_u32(g_ebx, 0x21212121u, "194676 EBX preserved");
    ok &= check_u32(g_esi, 0x32323232u, "194676 ESI preserved");
    ok &= check_u32(g_edi, 0x43434343u, "194676 EDI preserved");

    VirtualFree(window, 0, MEM_RELEASE);
    if (ok) printf("PASS: %u 11c1 recovered leaf ABI/behavior checks\n", checks);
    return ok ? 0 : 1;
}
