#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"   /* Nv2aPbExecCounters, commit-consumer seam */
#include "recomp_types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The kernel bridge references the game dispatcher/diagnostics; this focused
 * ABI fixture does not start guest threads, so keep those hooks inert. */
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site,
                        uint32_t value)
{ (void)kind; (void)target; (void)site; (void)value; }
/* A2h NULL-slot latch stubs. The real implementation lives in the game's src/diagnostics.c,
 * which this standalone toolkit-linked test does not build; the bridge calls these
 * unconditionally when its gate is armed, so an inert stub is required for linking. They
 * observe nothing and change nothing. */
void jsrf_slot_latch_install(uint32_t raw_value, uint32_t installed_value)
{ (void)raw_value; (void)installed_value; }
void jsrf_slot_latch_sample(uint32_t tid, uint32_t call_index, uint32_t ordinal,
                            uint32_t before, uint32_t after)
{ (void)tid; (void)call_index; (void)ordinal; (void)before; (void)after; }
/* A2h slot-WRITE watch stubs (registry version 3), same treatment. Both the census and the install
 * handshake are behind JSRF_TRACE_A2H_DR, which this fixture never sets, so they are unreachable
 * here -- but the toolkit objects that call them are linked in regardless. */
void jsrf_slot_watch_handshake(uint32_t slot_va) { (void)slot_va; }
void jsrf_slot_watch_alias_armed(uint32_t mapped_mask, uint32_t protect_mask, uint32_t alias_count)
{ (void)mapped_mask; (void)protect_mask; (void)alias_count; }
int jsrf_slot_watch_alias_touch(uint32_t alias_index, uint32_t fault_va, uint64_t rip,
                                uint32_t value, uint32_t published)
{ (void)alias_index; (void)fault_va; (void)rip; (void)value; (void)published; return 0; }
void jsrf_slot_watch_write(uint32_t provenance, uint32_t before, uint32_t after,
                           uint64_t rip, uint32_t ordinal)
{ (void)provenance; (void)before; (void)after; (void)rip; (void)ordinal; }

static int run_hal_pci_contract(void)
{
    uint8_t *vram = (uint8_t *)calloc(1, 64u * 1024u * 1024u);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint32_t value = 0;
    uint8_t byte = 0;
    uint8_t invalid[8];
    int ok = vram && ramin;
    if (!ok || !nv2a_init_standalone(vram, 64u * 1024u * 1024u,
                                     ramin, 1024u * 1024u)) return 0;

    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, FALSE);
    if (value != 0x02A010DEu) fprintf(stderr, "identity read %08X\n", value), ok = 0;
    value = 0xFFFFFFFFu;
    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x00, &value, 4, FALSE);
    if (value != 0x02A010DEu) fprintf(stderr, "identity write changed %08X\n", value), ok = 0;
    value = 0xFFFFFFFFu;
    xbox_HalReadWritePCISpace(1, 0, 0x08, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x08, &value, 4, FALSE);
    if (value != 0x030000A1u) fprintf(stderr, "class write changed %08X\n", value), ok = 0;

    value = 0x11223344u;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, FALSE);
    if (value != 0x11223344u) fprintf(stderr, "vendor read %08X\n", value), ok = 0;
    byte = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4F, &byte, 1, FALSE);
    byte |= 0x1Fu;
    xbox_HalReadWritePCISpace(1, 0, 0x4F, &byte, 1, TRUE);
    value = 0;
    xbox_HalReadWritePCISpace(1, 0, 0x4C, &value, 4, FALSE);
    if (value != 0x1F223344u) fprintf(stderr, "vendor byte read %08X\n", value), ok = 0;

    value = 0xA5A5A5A5u;
    xbox_HalReadWritePCISpace(0, 0, 0x4C, &value, 4, FALSE);
    if (value != 0) fprintf(stderr, "other bus read %08X\n", value), ok = 0;
    value = 0xA5A5A5A5u;
    xbox_HalReadWritePCISpace(1, 0, 0x7F, &value, 2, FALSE);
    if (value != 0xA5A50000u) fprintf(stderr, "crossing read %08X\n", value), ok = 0;
    memset(invalid, 0xA5, sizeof(invalid));
    xbox_HalReadWritePCISpace(1, 0, 0x4C, invalid, 8, FALSE);
    for (size_t i = 0; i < sizeof(invalid); ++i)
        if (invalid[i] != 0x00) ok = 0;

    free(ramin);
    free(vram);
    if (!ok) {
        fprintf(stderr, "FAIL: HAL PCI bridge contract\n");
        return 0;
    }
    puts("PASS: HAL PCI bridge ABI/config contract");
    return 1;
}

/* ── Committed-method consumer (Architecture A) ─────────────────────────────
 *
 * The executor used to be driven by the legacy pushbuffer SCAN, which the ack
 * worker runs -- and which is retired once the MMIO state owner takes over. The
 * submission walk now hands committed methods to a registered consumer, so this
 * fixture drives the REAL owner path (register window + PUT through the core)
 * and reads the REAL executor's counters. Driving nv2a_pb_exec_method directly
 * would prove only that the executor works, not that the walk feeds it.
 */

/* The consumer is the real executor; these count what the seam delivered. */
static uint32_t g_seen_calls;
static uint32_t g_seen_nv097;
static uint32_t g_seen_other;
static uint32_t g_last_subch, g_last_class, g_last_method, g_last_param;

static void counting_consumer(uint32_t subchannel, uint32_t class_id,
                              uint32_t method, uint32_t param)
{
    ++g_seen_calls;
    if (class_id == 0x97u) ++g_seen_nv097; else ++g_seen_other;
    g_last_subch = subchannel; g_last_class = class_id;
    g_last_method = method; g_last_param = param;
}

static void consumer_reset(void)
{
    g_seen_calls = g_seen_nv097 = g_seen_other = 0;
    g_last_subch = g_last_class = g_last_method = g_last_param = 0;
}

static int exec_check(uint32_t actual, uint32_t expected, const char *name)
{
    if (actual == expected) return 1;
    fprintf(stderr, "FAIL %s: actual=%u expected=%u\n", name, actual, expected);
    return 0;
}

static int run_consumer_contract(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    uint32_t *pb = (uint32_t *)VirtualAlloc(NULL, 0x2000,
                                            MEM_RESERVE | MEM_COMMIT,
                                            PAGE_READWRITE);
    NV2AState *gpu;
    int ok = 1;
    if (!ram || !ramin || !instance || !pb) return 0;

    nv2a_reset_standalone_for_test();
    gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!gpu) return 0;
    /* The owner path binds the window and the RAMHT handle the same way the
     * register fixture does. */
    if (!nv2a_set_pushbuffer_window(gpu, (uint8_t *)pb, 0, 0x2000)) return 0;
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) return 0;
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u;
        pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u;
        object[1] = object[2] = object[3] = 0;
    }

    /* (1) No consumer registered: a committed stream delivers nothing. */
    nv2a_set_commit_consumer(NULL);
    consumer_reset();
    memset(pb, 0, 0x2000);
    pb[0] = (1u << 18); pb[1] = 0x6u;            /* SET_OBJECT, NV097 */
    pb[2] = (1u << 18) | 0x1D94u; pb[3] = 0xF0u; /* CLEAR_SURFACE */
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 16;
    ok &= exec_check(nv2a_submit_pending(gpu), 1, "consumer: unregistered stream accepted");
    ok &= exec_check(g_seen_calls, 0, "consumer: unregistered delivers nothing");

    /* (2) Registered: every committed method arrives, in order, with its class. */
    nv2a_set_commit_consumer(counting_consumer);
    consumer_reset();
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 16;
    ok &= exec_check(nv2a_submit_pending(gpu), 1, "consumer: committed stream accepted");
    ok &= exec_check(g_seen_calls, 2, "consumer: committed methods delivered");
    ok &= exec_check(g_seen_nv097, 2, "consumer: both were NV097");
    ok &= exec_check(g_last_method, 0x1D94u, "consumer: last method is the committed one");
    ok &= exec_check(g_last_class, 0x97u, "consumer: last class is NV097");

    /* (3) A REJECTED submission delivers nothing: the walk fails before commit. */
    consumer_reset();
    memset(pb, 0, 0x2000);
    pb[0] = (1u << 18); pb[1] = 0x6u;              /* binds NV097 */
    pb[2] = (1u << 18) | 0x1D94u; pb[3] = 0xF0u;   /* a clear that WOULD execute */
    pb[4] = (1u << 18) | 0x03FCu; pb[5] = 0x04000004u;  /* unmodelled -> rejects */
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 24;
    ok &= exec_check(nv2a_submit_pending(gpu), 0, "consumer: unsupported method rejects");
    ok &= exec_check(g_seen_calls, 0, "consumer: rejected stream delivers nothing");

    /* (4) Non-NV097 classes reach the consumer WITH their own class, so the
     * executor wrapper skips them: the executor's counters must not move, and
     * the wrapper's skip count must advance. Asserting "callback delivered 0"
     * would be wrong -- the core delivers every committed method and the
     * filtering is the wrapper's job. */
    {
        uint32_t skip_before = nv2a_pb_exec_skipped_non_nv097();
        Nv2aPbExecCounters c_before, c_after;
        memset(&c_before, 0, sizeof(c_before));
        nv2a_pb_exec_counters(&c_before);
        consumer_reset();
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x6u;
        pb[2] = (1u << 18) | 0x100u; pb[3] = 0x11u;    /* NV097 NOP */
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 16;
        ok &= exec_check(nv2a_submit_pending(gpu), 1, "consumer: mixed stream accepted");
        ok &= exec_check(g_seen_calls, 2, "consumer: core delivers every committed method");
        ok &= exec_check(g_seen_nv097, 2, "consumer: both entries carry NV097");
        memset(&c_after, 0, sizeof(c_after));
        nv2a_pb_exec_counters(&c_after);
        ok &= exec_check(c_after.draws - c_before.draws, 0,
                         "consumer: no draws from a NOP stream");
        ok &= exec_check(c_after.flips - c_before.flips, 0,
                         "consumer: no flips from a NOP stream");
        ok &= exec_check(nv2a_pb_exec_skipped_non_nv097() - skip_before, 0,
                         "consumer: NV097 entries are not counted as skipped");
    }

    /* (5) Mid-stream rebind: a second SET_OBJECT binds the SAME subchannel
     * again. The NV097 methods walked BEFORE it must still carry NV097 --
     * reading the class from binding_class[] afterwards would retro-label them
     * with the later binding and the executor would skip work that really was
     * NV097. */
    {
        uint32_t *pair2 = (uint32_t *)(instance + 0x7u * 8u);
        uint32_t *obj2 = (uint32_t *)(instance + (0x310u << 4));
        pair2[0] = 0x7u; pair2[1] = NV_RAMHT_STATUS | 0x310u;
        obj2[0] = 0x97u; obj2[1] = obj2[2] = obj2[3] = 0;

        consumer_reset();
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x6u;               /* bind NV097, subch 0 */
        pb[2] = (1u << 18) | 0x100u; pb[3] = 0x22u;     /* NV097 NOP, pre-rebind */
        pb[4] = (1u << 18); pb[5] = 0x7u;               /* rebind subch 0 */
        pb[6] = (1u << 18) | 0x100u; pb[7] = 0x33u;     /* NV097 NOP, post-rebind */
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 32;
        ok &= exec_check(nv2a_submit_pending(gpu), 1, "consumer: rebind stream accepted");
        ok &= exec_check(g_seen_calls, 4, "consumer: rebind stream delivered all four");
        ok &= exec_check(g_seen_nv097, 4,
                         "consumer: pre-rebind methods keep their own class");
        ok &= exec_check(g_seen_other, 0, "consumer: rebind produced no foreign class");
    }

    nv2a_set_commit_consumer(NULL);
    VirtualFree(pb, 0, MEM_RELEASE);
    free(instance);
    free(ramin);
    free(ram);
    return ok;
}

static int run_executor_counter_contract(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    uint32_t *pb = (uint32_t *)VirtualAlloc(NULL, 0x2000,
                                            MEM_RESERVE | MEM_COMMIT,
                                            PAGE_READWRITE);
    NV2AState *gpu;
    Nv2aPbExecCounters before, after;
    uint32_t flip_calls_before = 0, flip_calls_after = 0;
    int ok = 1;
    if (!ram || !ramin || !instance || !pb) return 0;

    nv2a_reset_standalone_for_test();
    gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!gpu) return 0;
    if (!nv2a_set_pushbuffer_window(gpu, (uint8_t *)pb, 0, 0x2000)) return 0;
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) return 0;
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u; pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u; object[1] = object[2] = object[3] = 0;
    }
    /* The real executor registered on the real seam.
     *
     * The gate is RECOMP_PB_EXEC's PRESENCE, so a plain `just test` run (which
     * does not set it) would silently skip the only checks that prove the walk
     * feeds the executor -- a green suite that tested nothing. So the guard is
     * exercised BOTH ways here, deterministically, from inside the fixture:
     * absent must refuse, present must register. `_putenv_s(name, "")` removes
     * the variable, which is what makes the absent case a real absence rather
     * than an empty string (getenv would still return non-NULL for that). */
    _putenv_s("RECOMP_PB_EXEC", "");
    nv2a_pb_exec_register_commit_consumer();
    ok &= exec_check(nv2a_pb_exec_consumer_registered(), 0,
                     "executor: RECOMP_PB_EXEC absent -> no registration");
    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) {
        fprintf(stderr, "FAIL executor: RECOMP_PB_EXEC present but not registered\n");
        VirtualFree(pb, 0, MEM_RELEASE); free(instance); free(ramin); free(ram);
        return 0;
    }
    ok &= exec_check(nv2a_pb_exec_consumer_registered(), 1,
                     "executor: RECOMP_PB_EXEC present -> registered");
    memset(&before, 0, sizeof(before));
    nv2a_pb_exec_counters(&before);
    flip_calls_before = xbox_Nv2aFrameCounterFlipCalls();

    /* A committed FLIP_INCREMENT_WRITE (0x12C) advances `flips`; a committed
     * FLIP_STALL (0x130) must advance `flip_stalls` ONLY -- it does not move the
     * write pointer, so counting it in `flips` would misreport the swap path. */
    memset(pb, 0, 0x2000);
    pb[0]  = (1u << 18); pb[1]  = 0x6u;                 /* SET_OBJECT NV097 */
    pb[2]  = (1u << 18) | 0x012Cu; pb[3]  = 1u;         /* FLIP_INCREMENT_WRITE */
    pb[4]  = (1u << 18) | 0x0130u; pb[5]  = 0u;         /* FLIP_STALL */
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 24;
    ok &= exec_check(nv2a_submit_pending(gpu), 1, "executor: flip stream accepted");

    memset(&after, 0, sizeof(after));
    nv2a_pb_exec_counters(&after);
    flip_calls_after = xbox_Nv2aFrameCounterFlipCalls();
    ok &= exec_check(after.flips - before.flips, 1,
                     "executor: 0x12C increments flips by one");
    ok &= exec_check(after.flip_stalls - before.flip_stalls, 1,
                     "executor: 0x130 increments flip_stalls by one");
    /* 0x130 is a completed swap, so it must report exactly one flip to the
     * frame-counter path. Asserted as the CALL COUNT, not a guest counter
     * delta: the counter walk inside xbox_Nv2aFrameCounterFlip is skipped when
     * g_memory_base is NULL (which it is here -- this fixture never runs the
     * memory-layout init), so a delta assertion would fail for a reason that has
     * nothing to do with the flip path. The call count is unconditional. */
    ok &= exec_check(flip_calls_after - flip_calls_before, 1,
                     "executor: 0x130 reports exactly one completed swap");
    /* SET_OBJECT (method 0) is itself a committed method on the bound NV097
     * subchannel, so it is delivered like any other; the executor has no
     * implementation for it and counts it unhandled. Exactly one, so a
     * mis-delivered stream would show up here as a different number. */
    ok &= exec_check(after.unhandled - before.unhandled, 1,
                     "executor: only SET_OBJECT is unhandled in this stream");

    /* (2) A committed method on a NON-NV097 class must not reach the executor:
     * its counters stay put and the wrapper's skip count advances. The class
     * here is NV_MEMCPY (0x39), which the core's method table really admits,
     * so the stream is accepted and the filtering under test is the wrapper's
     * -- not the walk's rejection. */
    {
        uint32_t *pair = (uint32_t *)(instance + 0x8u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x320u << 4));
        Nv2aPbExecCounters c_before, c_after;
        uint32_t skip_before;
        pair[0] = 0x8u; pair[1] = NV_RAMHT_STATUS | 0x320u;
        object[0] = 0x39u; object[1] = object[2] = object[3] = 0;

        memset(&c_before, 0, sizeof(c_before));
        nv2a_pb_exec_counters(&c_before);
        skip_before = nv2a_pb_exec_skipped_non_nv097();

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x8u;               /* bind NV_MEMCPY */
        pb[2] = (1u << 18) | 0x0180u; pb[3] = 0x1234u;  /* its real method */
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 16;
        ok &= exec_check(nv2a_submit_pending(gpu), 1,
                         "executor: non-NV097 stream accepted");
        memset(&c_after, 0, sizeof(c_after));
        nv2a_pb_exec_counters(&c_after);
        ok &= exec_check(c_after.draws - c_before.draws, 0,
                         "executor: non-NV097 stream draws nothing");
        ok &= exec_check(c_after.flips - c_before.flips, 0,
                         "executor: non-NV097 stream flips nothing");
        ok &= exec_check(c_after.unhandled - c_before.unhandled, 0,
                         "executor: non-NV097 stream reaches no handler");
        ok &= exec_check(nv2a_pb_exec_skipped_non_nv097() - skip_before, 2,
                         "executor: both non-NV097 entries counted as skipped");
    }

    nv2a_set_commit_consumer(NULL);
    VirtualFree(pb, 0, MEM_RELEASE);
    free(instance);
    free(ramin);
    free(ram);
    return ok;
}

/* A recorder back end. With one registered, clear_surface() hands the clear to
 * the back end and returns BEFORE touching guest memory -- which is the only
 * way to exercise a REAL committed clear from this fixture, whose memory
 * pointer is NULL because the memory layout was never initialised. */
static uint32_t g_backend_clears, g_backend_flips, g_backend_draws;
static void backend_clear(const Nv2aSurface *s, const Nv2aRenderState *rs,
                          uint32_t flags, uint32_t argb, uint32_t zstencil)
{
    (void)s; (void)rs; (void)flags; (void)argb; (void)zstencil;
    ++g_backend_clears;
}
static void backend_draw(const Nv2aSurface *s, const Nv2aBatch *b)
{ (void)s; (void)b; ++g_backend_draws; }
static void backend_flip(void) { ++g_backend_flips; }
static const Nv2aBackend g_test_backend = {
    backend_clear, backend_draw, backend_flip
};

static int run_executor_clear_contract(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    uint32_t *pb = (uint32_t *)VirtualAlloc(NULL, 0x2000,
                                            MEM_RESERVE | MEM_COMMIT,
                                            PAGE_READWRITE);
    NV2AState *gpu;
    int ok = 1;
    if (!ram || !ramin || !instance || !pb) return 0;

    nv2a_reset_standalone_for_test();
    gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!gpu) return 0;
    if (!nv2a_set_pushbuffer_window(gpu, (uint8_t *)pb, 0, 0x2000)) return 0;
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) return 0;
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u; pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u; object[1] = object[2] = object[3] = 0;
    }
    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) {
        fprintf(stderr, "FAIL executor: RECOMP_PB_EXEC present but not registered\n");
        VirtualFree(pb, 0, MEM_RELEASE); free(instance); free(ramin); free(ram);
        return 0;
    }
    nv2a_backend_register(&g_test_backend);
    g_backend_clears = 0;

    {
        Nv2aPbExecCounters before, after;
        memset(&before, 0, sizeof(before));
        nv2a_pb_exec_counters(&before);

        /* A committed surface + clear, all through the owner walk. Clip is 4x4
         * and pitch 16, so bpp = 4 and the surface is well-formed. */
        memset(pb, 0, 0x2000);
        pb[0]  = (1u << 18); pb[1]  = 0x6u;             /* SET_OBJECT NV097 */
        pb[2]  = (1u << 18) | 0x0200u; pb[3]  = (4u << 16);      /* CLIP_H: x0 w4 */
        pb[4]  = (1u << 18) | 0x0204u; pb[5]  = (4u << 16);      /* CLIP_V: y0 h4 */
        pb[6]  = (1u << 18) | 0x020Cu; pb[7]  = 16u;             /* PITCH 16 */
        pb[8]  = (1u << 18) | 0x0210u; pb[9]  = 0x1000u;         /* COLOR_OFFSET */
        pb[10] = (1u << 18) | 0x1D90u; pb[11] = 0x0000FF00u;     /* CLEAR VALUE */
        pb[12] = (1u << 18) | 0x1D94u; pb[13] = 0xF0u;           /* CLEAR_SURFACE */
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 56;
        ok &= exec_check(nv2a_submit_pending(gpu), 1,
                         "executor: committed clear stream accepted");

        memset(&after, 0, sizeof(after));
        nv2a_pb_exec_counters(&after);
        ok &= exec_check(after.clears - before.clears, 1,
                         "executor: committed CLEAR increments clears by one");
        ok &= exec_check(g_backend_clears, 1,
                         "executor: committed CLEAR reached the back end");
    }

    /* Atomicity against a LATE fault: the clear is well-formed and comes first,
     * then an unmodelled method rejects the whole submission. The real executor
     * counters and the back end must both be untouched -- the clear must not
     * have been executed. */
    {
        Nv2aPbExecCounters before, after;
        uint32_t backend_before = g_backend_clears;
        uint32_t flip_calls_before = xbox_Nv2aFrameCounterFlipCalls();
        memset(&before, 0, sizeof(before));
        nv2a_pb_exec_counters(&before);

        memset(pb, 0, 0x2000);
        pb[0]  = (1u << 18); pb[1]  = 0x6u;
        pb[2]  = (1u << 18) | 0x0200u; pb[3]  = (4u << 16);
        pb[4]  = (1u << 18) | 0x0204u; pb[5]  = (4u << 16);
        pb[6]  = (1u << 18) | 0x020Cu; pb[7]  = 16u;
        pb[8]  = (1u << 18) | 0x0210u; pb[9]  = 0x1000u;
        pb[10] = (1u << 18) | 0x1D94u; pb[11] = 0xF0u;           /* the clear */
        pb[12] = (1u << 18) | 0x03FCu; pb[13] = 0x04000004u;     /* unmodelled */
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
        gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = 56;
        ok &= exec_check(nv2a_submit_pending(gpu), 0,
                         "executor: late unmodelled method rejects the stream");

        memset(&after, 0, sizeof(after));
        nv2a_pb_exec_counters(&after);
        ok &= exec_check(after.clears - before.clears, 0,
                         "executor: rejected stream does not clear");
        ok &= exec_check(after.flips - before.flips, 0,
                         "executor: rejected stream does not flip");
        ok &= exec_check(g_backend_clears - backend_before, 0,
                         "executor: rejected stream reaches no back end");
        ok &= exec_check(xbox_Nv2aFrameCounterFlipCalls() - flip_calls_before, 0,
                         "executor: rejected stream reports no completed swap");
    }

    nv2a_backend_register(NULL);
    nv2a_set_commit_consumer(NULL);
    VirtualFree(pb, 0, MEM_RELEASE);
    free(instance);
    free(ramin);
    free(ram);
    return ok;
}

int main(void)
{
    int ok = 1;
    ok &= run_hal_pci_contract();
    ok &= run_consumer_contract();
    ok &= run_executor_counter_contract();
    ok &= run_executor_clear_contract();
    if (!ok) { fprintf(stderr, "FAIL: committed-method consumer contract\n"); return 1; }
    puts("PASS: committed-method consumer (owner path -> real executor)");
    return 0;
}