/* Register-only fixture. No window, guest execution, or renderer is simulated. */
#include "nv2a_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The core references the renderer, but these tests must never dispatch a draw. */
int pgraph_d3d11_method(int subchannel, uint32_t method, uint32_t param)
{
    (void)subchannel; (void)method; (void)param;
    fprintf(stderr, "FAIL: register fixture unexpectedly dispatched a draw\n");
    abort();
}

static unsigned checks;
static uint64_t fixture_clock_ns;
static uint64_t fixture_clock(void *opaque)
{
    (void)opaque;
    return fixture_clock_ns;
}
static int check(uint64_t actual, uint64_t expected, const char *name)
{
    ++checks;
    if (actual == expected) return 1;
    fprintf(stderr, "FAIL %s: actual=%llu expected=%llu\n", name,
            (unsigned long long)actual, (unsigned long long)expected);
    return 0;
}

/* Interrupt-line capture. The model reports a line LEVEL on every update that
 * changes it; the sink is the guest's host owner, so it must never see the
 * same level twice in a row. Counting transitions is what makes "reported
 * once" distinguishable from "reported every update". */
struct irq_capture {
    int transitions;
    int last_level;
};

static void irq_capture_sink(void *opaque, int asserted)
{
    struct irq_capture *capture = (struct irq_capture *)opaque;
    if (capture->last_level == asserted) {
        fprintf(stderr, "FAIL: interrupt sink saw level %d twice\n", asserted);
        abort();
    }
    capture->last_level = asserted;
    capture->transitions++;
}

static void submit_reset(NV2AState *gpu, uint32_t get, uint32_t put)
{
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = get;
    gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = put;
    gpu->pfifo.submit_diag = 0;
    gpu->pfifo.submit_diag_get = 0;
    gpu->pfifo.submit_diag_subchannel = 0;
    gpu->pfifo.submit_diag_method = 0;
    gpu->pfifo.submit_diag_param = 0;
    gpu->pfifo.submit_words = 0;
    gpu->pfifo.submit_packets = 0;
    gpu->pfifo.submit_last_method = 0;
    gpu->pfifo.submit_last_param = 0;
    gpu->pfifo.submit_successes = 0;
    gpu->pfifo.sink_count = 0;
}

static int submit_diag_is(NV2AState *gpu, const char *expected)
{
    return strcmp(nv2a_submit_diagnostic(gpu->pfifo.submit_diag), expected) == 0;
}

/* Install one original-XBE RAMHT pair plus the 16-byte RAMIN object it names.
 * Hash is the 0x001945D6 11-bit fold; for 4K tables that is the handle itself
 * when handle < 0x800. */
static void ramht_install(uint8_t *instance, uint32_t handle, uint32_t tag,
                          uint32_t engine, uint32_t class_id, uint32_t extra)
{
    uint32_t *pair = (uint32_t *)(instance + handle * 8u);
    uint32_t *object = (uint32_t *)(instance + (tag << 4));
    pair[0] = handle;
    pair[1] = NV_RAMHT_STATUS | engine | (tag & NV_RAMHT_INSTANCE);
    object[0] = class_id;
    object[1] = extra;
    object[2] = 0;
    object[3] = 0;
}

int main(void)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = calloc(1, ram_size), *ramin = calloc(1, 1024u * 1024u);
    uint8_t *instance = calloc(1, 0x20000u);
    NV2AState *gpu;
    uint32_t pll;
    uint64_t reset_hz;
    int ok = 1;
    if (!ram || !ramin || !instance) return 2;
    /* Exercise claim publication before the hook/core exists. */
    ok &= check(nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u), 1,
                "PRAMIN pre-init binding accepted");
    ok &= check(nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u), 1,
                "PRAMIN repeated claim transaction is idempotent");
    ok &= check(nv2a_bind_instance_memory(0x83fe0000u, instance, 0x10000u), 0,
                "PRAMIN contradictory claim transaction rejected");
    ok &= check(nv2a_bind_instance_memory(0x83fe0000u, NULL, 0x20000u), 0,
                "PRAMIN null binding rejected");
    ok &= check(nv2a_bind_instance_memory(0xfffffff0u, instance, 0x20u), 0,
                "PRAMIN overflow binding rejected");
    gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!gpu) return 2;
    ok &= check(gpu->ramin_ptr == instance, 1,
                "PRAMIN claim-before-init is consumed by initialization");
    ok &= check(gpu->ramin.size, 0x20000u,
                "PRAMIN claim-before-init size is published");

    /* Recreate the focused singleton to prove the production init-first
     * ordering without allowing the detached constructor RAMIN as backing. */
    nv2a_reset_standalone_for_test();
    memset(ramin, 0, 1024u * 1024u);
    ramin[0x10] = 0x6cu;
    gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!gpu) return 2;
    ok &= check(gpu->ramin_ptr == NULL, 1,
                "PRAMIN init-first starts unbound");
    ok &= check(gpu->ramin.size, 0, "PRAMIN unbound size is zero");
    ok &= check(nv2a_mmio_read(gpu, 0x700010u, 1), 0,
                "PRAMIN unbound read returns zero");
    nv2a_mmio_write(gpu, 0x700010u, 0x5au, 1);
    ok &= check(ramin[0x10], 0x6cu,
                "PRAMIN unbound write ignores detached RAMIN");
    {
        hwaddr unbound_len = 0xfeedu;
        DMAObject unbound_dma = nv_dma_load(gpu, 0);
        ok &= check(unbound_dma.dma_class, 0,
                    "PRAMIN unbound DMA load rejected");
        ok &= check(nv_dma_map(gpu, 0, &unbound_len) == NULL, 1,
                    "PRAMIN unbound DMA map rejected");
        ok &= check(unbound_len, 0, "PRAMIN unbound map clears length");
    }

    /* 11b4b3: the claim supplies the only PRAMIN backing.  The guest base is
     * retained by the binding for ordering/diagnostics; this fixture supplies
     * the exact host mapping a production claim would publish. */
    ok &= check(nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u), 1,
                "PRAMIN instance binding accepted");
    ok &= check(gpu->ramin_ptr == instance, 1,
                "PRAMIN init-before-claim publishes backing");
    nv2a_mmio_write(gpu, 0x700000u + 0x10u, 0x5au, 1);
    ok &= check(instance[0x10], 0x5au, "PRAMIN byte write reaches claim");
    nv2a_mmio_write(gpu, 0x700000u + 0x20u, 0xa55au, 2);
    ok &= check(*(uint16_t *)(instance + 0x20), 0xa55au,
                "PRAMIN word write reaches claim");
    nv2a_mmio_write(gpu, 0x700000u + 0x30u, 0xdeadbeefu, 4);
    ok &= check(*(uint32_t *)(instance + 0x30), 0xdeadbeefu,
                "PRAMIN dword write reaches claim");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x10u, 1), 0x5au,
                "PRAMIN byte round trip");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x20u, 2), 0xa55au,
                "PRAMIN word round trip");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x30u, 4), 0xdeadbeefu,
                "PRAMIN dword round trip");
    nv2a_mmio_write(gpu, 0x700000u + 0x100u, 0x00001003u, 4);
    nv2a_mmio_write(gpu, 0x700000u + 0x104u, 0x00000fffu, 4);
    nv2a_mmio_write(gpu, 0x700000u + 0x108u, 0x00200003u, 4);
    DMAObject dma = nv_dma_load(gpu, 0x100);
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x100u, 4), 0x00001003u,
                "RAMIN DMA word0 visible");
    ok &= check(dma.dma_class, 3, "RAMIN DMA class visible");
    ok &= check(dma.limit, 0xfffu, "RAMIN DMA limit visible");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x108u, 4), 0x00200003u,
                "RAMIN DMA address|3 visible");
    ok &= check(dma.address, 0x200000u, "RAMIN DMA address aligned");
    {
        hwaddr map_len = 0xfeedu;
        uint8_t *saved_ramin = gpu->ramin_ptr;
        uint64_t saved_ramin_size = gpu->ramin.size;
        DMAObject invalid_dma;
        ok &= check(nv_dma_map(gpu, 0x100u, NULL) == NULL, 1,
                    "RAMIN DMA map tolerates null length output");
        gpu->ramin_ptr = NULL;
        invalid_dma = nv_dma_load(gpu, 0x100u);
        ok &= check(invalid_dma.dma_class, 0,
                    "RAMIN DMA load rejects null backing");
        ok &= check(nv_dma_map(gpu, 0x100u, &map_len) == NULL, 1,
                    "RAMIN DMA map rejects null backing");
        ok &= check(map_len, 0, "RAMIN rejected map clears length");
        gpu->ramin_ptr = saved_ramin;
        gpu->ramin.size = 8;
        invalid_dma = nv_dma_load(gpu, 0);
        ok &= check(invalid_dma.limit, 0,
                    "RAMIN DMA load rejects undersized descriptor backing");
        map_len = 0xfeedu;
        ok &= check(nv_dma_map(gpu, 0, &map_len) == NULL, 1,
                    "RAMIN DMA map rejects undersized descriptor backing");
        ok &= check(map_len, 0, "RAMIN undersized map clears length");
        gpu->ramin.size = saved_ramin_size;
        invalid_dma = nv_dma_load(gpu, saved_ramin_size - 8u);
        ok &= check(invalid_dma.address, 0,
                    "RAMIN DMA load rejects crossing descriptor");
        map_len = 0xfeedu;
        ok &= check(nv_dma_map(gpu, saved_ramin_size - 8u, &map_len) == NULL, 1,
                    "RAMIN DMA map rejects crossing descriptor");
        ok &= check(map_len, 0, "RAMIN crossing map clears length");
    }
    instance[0x1ffffu] = 0x7cu;
    nv2a_mmio_write(gpu, 0x700000u + 0x1ffffu, 0x11u, 2);
    ok &= check(instance[0x1ffffu], 0x7cu, "PRAMIN crossing write rejected");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x1ffffu, 2), 0,
                "PRAMIN crossing read rejected");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x20000u, 4), 0,
                "PRAMIN out of range read rejected");
    ok &= check(nv2a_mmio_read(gpu, 0x700000u + 0x1ffffu, 8), 0,
                "PRAMIN invalid width rejected");

    /* These are the first reads/writes in original JSRF 0x19460A/0x194635. */
    ok &= check(nv2a_mmio_read(gpu, 0x1800, 4), 0x02A010DE, "GPU PCI identity");
    ok &= check(nv2a_mmio_read(gpu, 0x1808, 4) & 0xff, 0xA1, "GPU revision");
    ok &= check(nv2a_mmio_read(gpu, 0x10020C, 4), ram_size, "visible RAM size");
    nv2a_mmio_write(gpu, 0x100410, 0x10000, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x100410, 4), 0,
                "PFB WBC flush reads idle");
    nv2a_mmio_write(gpu, 0x1804, 4, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x1804, 4) & 4, 4, "PCI bus master enable");
    ok &= check(nv2a_mmio_read(gpu, 0x1800, 4), 0x02A010DE, "PCI identity preserved");
    /* 11b2: standard PCI fields and vendor bytes share PBUS backing. */
    nv2a_mmio_write(gpu, 0x180c, 0x0000f800, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x180c, 4), 0x0000f800,
                "PCI latency dword");
    nv2a_mmio_write(gpu, 0x1830, 0, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x1830, 4), 0, "PCI expansion ROM BAR");
    nv2a_mmio_write(gpu, 0x184c, 0x11223344, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x184c, 4), 0x11223344,
                "PBUS vendor dword round trip");
    {
        uint8_t byte = 0;
        ok &= check(nv2a_pci_config_read(gpu, 0x4f, &byte, 1), 1,
                    "PCI vendor byte read");
        ok &= check(byte, 0x11, "PCI vendor byte offset 0x4F");
        byte |= 0x1f;
        ok &= check(nv2a_pci_config_write(gpu, 0x4f, &byte, 1), 1,
                    "PCI vendor byte write");
        ok &= check(nv2a_mmio_read(gpu, 0x184c, 4), 0x1f223344,
                    "PBUS vendor byte update");
    }
    ok &= check(nv2a_pci_config_write(gpu, 0x0c, &(uint32_t){0x0000f800}, 4), 1,
                "PCI config dword write");
    ok &= check(nv2a_mmio_read(gpu, 0x180c, 4), 0x0000f800,
                "PCI config dword shared read");
    ok &= check(nv2a_pci_config_read(gpu, 0x0c, &(uint32_t){0}, 8), 0,
                "PCI invalid width rejected");
    gpu->parent_obj.config[0x7f] = 0x6d;
    gpu->parent_obj.config[0x80] = 0xa7;
    ok &= check(nv2a_mmio_read(gpu, 0x187f, 1), 0x6d,
                "PBUS final in-range byte readable");
    nv2a_mmio_write(gpu, 0x187f, 0x1122, 2);
    ok &= check(nv2a_mmio_read(gpu, 0x187f, 2), 0,
                "PBUS crossing nonzero read rejected");
    ok &= check(gpu->parent_obj.config[0x7f], 0x6d,
                "PBUS crossing write preserves final mirror byte");
    ok &= check(gpu->parent_obj.config[0x80], 0xa7,
                "PBUS crossing write preserves sentinel beyond mirror");
    nv2a_mmio_write(gpu, 0x600140, 0, 4);
    nv2a_mmio_write(gpu, 0x009140, 0, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x600140, 4), 0, "CRTC interrupts disabled");
    ok &= check(nv2a_mmio_read(gpu, 0x009140, 4), 0, "timer interrupts disabled");
    gpu->pgraph.regs[NV_PGRAPH_INTR] = 0x1001;
    gpu->pgraph.pending_interrupts = 0x1001;
    nv2a_mmio_write(gpu, 0x400100, 0x1000, 4);
    ok &= check(nv2a_mmio_read(gpu, 0x400100, 4), 1, "PGRAPH_INTR W1C keeps other bits");
    ok &= check(gpu->pgraph.pending_interrupts, 1, "PGRAPH pending follows W1C");

    /* 11b3: PTIMER uses a 56-bit tick value split across encoded registers. */
    {
        fixture_clock_ns = 100000;
        nv2a_ptimer_set_clock(gpu, fixture_clock, NULL);
        nv2a_mmio_write(gpu, 0x000140, NV_PMC_INTR_0_PTIMER, 4);
        nv2a_mmio_write(gpu, 0x009140, NV_PTIMER_INTR_EN_0_ALARM, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009100, 4) & NV_PTIMER_INTR_0_ALARM,
                    NV_PTIMER_INTR_0_ALARM, "PTIMER reset alarm fires on late enable");
        nv2a_mmio_write(gpu, 0x009100, NV_PTIMER_INTR_0_ALARM, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009100, 4), 0,
                    "PTIMER default alarm W1C remains clear");

        fixture_clock_ns = 0;
        nv2a_mmio_write(gpu, 0x009400, 0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0x12345, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009410, 4), 0x12345,
                    "PTIMER TIME_1 write preserves requested high half");
        nv2a_mmio_write(gpu, 0x009400, 0x89abcde0, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009400, 4), 0x89abcde0,
                    "PTIMER TIME_0 write preserves encoded low half");

        /* Writing a time below the nonzero absolute clock creates an unsigned
         * offset; the retained value is still the requested encoded time. */
        fixture_clock_ns = 1000000000ULL;
        nv2a_mmio_write(gpu, 0x009400, 0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009400, 4), 0,
                    "PTIMER TIME offset underflow low half");
        ok &= check(nv2a_mmio_read(gpu, 0x009410, 4), 0,
                    "PTIMER TIME offset underflow high half");

        fixture_clock_ns = 0;
        nv2a_mmio_write(gpu, 0x009400, 0xffffffe0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0x1fffffff, 4);
        fixture_clock_ns = 100;
        ok &= check(nv2a_mmio_read(gpu, 0x009410, 4), 0,
                    "PTIMER retained offset wraps high half");
        ok &= check(nv2a_mmio_read(gpu, 0x009400, 4) < 0x1000, 1,
                    "PTIMER retained offset wraps low half");

        fixture_clock_ns = 0;
        nv2a_mmio_write(gpu, 0x009400, 0x2468ace0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0x13579, 4);
        nv2a_mmio_write(gpu, 0x009200, 0, 4);
        fixture_clock_ns = 9000000000ULL;
        ok &= check(nv2a_mmio_read(gpu, 0x009400, 4), 0x2468ace0,
                    "PTIMER zero numerator retains TIME_0 offset");
        ok &= check(nv2a_mmio_read(gpu, 0x009410, 4), 0x13579,
                    "PTIMER zero numerator retains TIME_1 offset");
        ok &= check(nv2a_ptimer_next_alarm_ns(gpu), UINT64_MAX,
                    "PTIMER zero numerator suspends scheduling");
        nv2a_mmio_write(gpu, 0x009200, 1, 4);
        fixture_clock_ns = 0;
        nv2a_mmio_write(gpu, 0x009400, 0x13579bc0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0x2468a, 4);
        nv2a_mmio_write(gpu, 0x009210, 0, 4);
        fixture_clock_ns = 9000000000ULL;
        ok &= check(nv2a_mmio_read(gpu, 0x009400, 4), 0x13579bc0,
                    "PTIMER zero denominator retains TIME_0 offset");
        ok &= check(nv2a_mmio_read(gpu, 0x009410, 4), 0x2468a,
                    "PTIMER zero denominator retains TIME_1 offset");
        ok &= check(nv2a_ptimer_next_alarm_ns(gpu), UINT64_MAX,
                    "PTIMER zero denominator suspends scheduling");

        nv2a_mmio_write(gpu, 0x009210, 1, 4);
        fixture_clock_ns = 0;
        nv2a_mmio_write(gpu, 0x009400, 0, 4);
        nv2a_mmio_write(gpu, 0x009410, 0, 4);
        nv2a_mmio_write(gpu, 0x009420, 0x1000, 4);
        fixture_clock_ns = 4000000000ULL;
        nv2a_ptimer_service(gpu);
        uint32_t fired = (uint32_t)nv2a_mmio_read(gpu, 0x009100, 4) &
                          NV_PTIMER_INTR_0_ALARM;
        ok &= check(fired, NV_PTIMER_INTR_0_ALARM,
                    "PTIMER multi-period late alarm pending");
        ok &= check(gpu->ptimer.alarm_time & 0xffffffffULL, 0x1000,
                    "PTIMER multi-period next alarm keeps low word");
        ok &= check(nv2a_ptimer_next_alarm_ns(gpu) > 0, 1,
                    "PTIMER multi-period next alarm is future");
        ok &= check(nv2a_mmio_read(gpu, 0x000100, 4) & NV_PMC_INTR_0_PTIMER,
                    NV_PMC_INTR_0_PTIMER, "PTIMER PMC interrupt aggregation");
        nv2a_mmio_write(gpu, 0x009100, NV_PTIMER_INTR_0_ALARM, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x009100, 4), 0,
                    "PTIMER alarm W1C");
        ok &= check(nv2a_mmio_read(gpu, 0x000100, 4) & NV_PMC_INTR_0_PTIMER,
                    0, "PTIMER W1C clears PMC aggregation");
    }

    /* Reapplying an unchanged control register cannot change the clock. */
    pll = (uint32_t)nv2a_mmio_read(gpu, 0x680000 + NV_PRAMDAC_NVPLL_COEFF, 4);
    reset_hz = gpu->pramdac.core_clock_freq;
    nv2a_mmio_write(gpu, 0x680000 + NV_PRAMDAC_NVPLL_COEFF, pll, 4);
    ok &= check(gpu->pramdac.core_clock_freq, reset_hz, "unchanged PLL keeps clock");
    ok &= check(gpu->pramdac.core_clock_freq, (uint64_t)NV2A_CRYSTAL_FREQ * 14,
                "reset PLL gives approximately 233 MHz");
    nv2a_mmio_write(gpu, 0x680000 + NV_PRAMDAC_NVPLL_COEFF, pll & ~0xffu, 4);
    ok &= check(gpu->pramdac.core_clock_freq, 0, "disabled PLL divider");
    nv2a_mmio_write(gpu, 0x680000 + NV_PRAMDAC_NVPLL_COEFF, pll, 4);
    ok &= check(gpu->pramdac.core_clock_freq, reset_hz, "restored PLL keeps clock");

    /* 11b4: bounded packet/control decoding is one atomic transaction. */
    {
        uint32_t *pb = (uint32_t *)VirtualAlloc(NULL, 0x2000,
                                               MEM_RESERVE | MEM_COMMIT,
                                               PAGE_READWRITE);
        DWORD old_protect;
        ok &= check(pb != NULL, 1, "USER test mapping allocation");
        ok &= check(nv2a_set_pushbuffer_window(gpu, (uint8_t *)pb, 0, 0x2000), 1,
                    "USER contiguous window registration");

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x100u;
        pb[1] = 0x11111111;
        pb[2] = 0x40000000u | (3u << 18) | 0x100u;
        pb[3] = 0x22222222; pb[4] = 0x33333333; pb[5] = 0x44444444;
        submit_reset(gpu, 0, 24);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER increment/nonincrement accepted");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 24,
                    "USER supported stream advances GET exactly");
        ok &= check(gpu->pfifo.sink_count, 4, "USER records complete method stream");
        ok &= check(gpu->pfifo.sink[1].method, 0x100, "USER incrementing NOP method");
        ok &= check(gpu->pfifo.sink[3].method, 0x100, "USER nonincrementing NOP holds");
        nv2a_mmio_write(gpu, 0x800044, 0x55, 1);
        nv2a_mmio_write(gpu, 0x800040, 0x66, 2);
        ok &= check(nv2a_mmio_read(gpu, 0x800044, 4), 24,
                    "USER byte GET write rejected");
        ok &= check(nv2a_mmio_read(gpu, 0x800040, 4), 24,
                    "USER word PUT write rejected");

        /* 11b4b2 fixture seam: RAMIN/DMA object lookup is not yet available,
         * so bind a known NV097 object explicitly and exercise only the
         * reviewed state shadow methods. */
        /* Registration alone never activates the fixture path, including
         * SET_OBJECT itself. */
        memset(pb, 0, 0x2000);
        ok &= check(nv2a_set_fixture_binding(gpu, 0, 0x1234, 0x97), 1,
                    "USER fixture NV097 binding registration");
        pb[0] = (1u << 18) | 0x0000u; pb[1] = 0x1234;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER SET_OBJECT rejected while fixture execution disabled");
        ok &= check(gpu->pfifo.binding_object[0], 0,
                    "USER disabled SET_OBJECT binding unchanged");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER production SET_OBJECT without RAMHT is invalid_handle");
        ok &= check(gpu->pfifo.submit_diag_method, 0,
                    "USER disabled SET_OBJECT exact method diagnostic");
        ok &= check(gpu->pfifo.submit_diag_param, 0x1234,
                    "USER disabled SET_OBJECT exact parameter diagnostic");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 0,
                    "USER invalid-handle GET rollback");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x0200u; pb[1] = 0xdeadbeefu;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER state method rejected before active bind");
        ok &= check(submit_diag_is(gpu, "unsupported_method"), 1,
                    "USER prebind state diagnostic");
        ok &= check(nv2a_set_fixture_execution(gpu, true), 1,
                    "USER fixture execution gate");
        ok &= check(nv2a_set_fixture_binding(gpu, 0, 0x1234, 0x97), 1,
                    "USER fixture NV097 binding seam");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x0000u; pb[1] = 0x1234;
        pb[2] = (2u << 18) | 0x0200u;
        pb[3] = 0x00100020; pb[4] = 0x00200040;
        submit_reset(gpu, 0, 20);
        ok &= check(nv2a_submit_pending(gpu), 1,
                    "USER bound NV097 clip methods accepted");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], 0x00100020,
                    "NV097 surface clip horizontal stored");
        ok &= check(gpu->pgraph.regs[0x0204 / 4], 0x00200040,
                    "NV097 surface clip vertical stored");
        /* The pinned NV097 definitions assign all 32 bits of each clip word
         * to two 16-bit fields, so zero and all-ones are both defined. */
        memset(pb, 0, 0x2000);
        pb[0] = (2u << 18) | 0x0200u;
        pb[1] = 0x00000000u; pb[2] = 0xffffffffu;
        submit_reset(gpu, 0, 12);
        ok &= check(nv2a_submit_pending(gpu), 1,
                    "USER clip full parameter domain accepted");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], 0,
                    "NV097 zero clip fields stored");
        ok &= check(gpu->pgraph.regs[0x0204 / 4], 0xffffffffu,
                    "NV097 maximum clip fields stored");

        /* The vertex-program methods the title submits are implemented, so the
         * walk accepts them and stores them as NV097 register state. Decoding
         * JSRF's own ring at 0x80001000..0x80002764 shows it submits the
         * transform-execution-mode and transform-program-start pair, and the
         * model rejected the whole stream at 0x1BCC
         * (logs/runs/20260922-110235-244-spanfix-1185b0, `[PFIFO] submit #1
         * diag=unsupported_method ... method=1BCC`), which left GET at 0x1000
         * while PUT was 0x2764 and the guest blocked in
         * `KeWaitForSingleObject` waiting for the ring to drain.
         *
         * The earlier inventory missed them because its walk classified packets
         * by `h >> 30` rather than the model's own `(h & 3) == 1` call and
         * `(h & 3) == 2` return test, so past 0x1B24 words it was decoding a
         * different stream than the model walks. The generator now mirrors
         * `nv2a_submit_pending` word for word. */
        memset(pb, 0, 0x2000);
        pb[0] = (2u << 18) | 0x1BC8u;
        pb[1] = 0x00000002u; pb[2] = 0x00000003u;
        submit_reset(gpu, 0, 12);
        ok &= check(nv2a_submit_pending(gpu), 1,
                    "USER NV097 transform execution mode accepted");
        ok &= check(submit_diag_is(gpu, "ok"), 1,
                    "USER NV097 transform execution mode diagnostic");
        ok &= check(gpu->pgraph.regs[0x1BC8 / 4], 0x00000002u,
                    "NV097 transform execution mode stored");
        ok &= check(gpu->pgraph.regs[0x1BCC / 4], 0x00000003u,
                    "NV097 transform program start stored");

        /* Implementing a method must not weaken the rejection next door: the
         * neighbours of 0x1BC8 that the title never submits still reject. */
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x1BD0u;
        pb[1] = 0x00000004u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER unimplemented transform neighbour rejected");
        ok &= check(submit_diag_is(gpu, "unsupported_method"), 1,
                    "USER unimplemented transform neighbour diagnostic");
        ok &= check(gpu->pfifo.submit_diag_method, 0x1BD0u,
                    "USER unimplemented transform neighbour exact method");
        ok &= check(gpu->pgraph.regs[0x1BC8 / 4], 0x00000002u,
                    "NV097 transform execution mode survives neighbour reject");

        /* Handle rebinding is per subchannel and commits with the stream. */
        ok &= check(nv2a_set_fixture_binding(gpu, 0, 0x5678, 0x97), 1,
                    "USER fixture rebind registration");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x5678;
        pb[2] = (1u << 18) | 0x0200u; pb[3] = 0x05000005u;
        submit_reset(gpu, 0, 16);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER NV097 rebind accepted");
        ok &= check(gpu->pfifo.binding_object[0], 0x5678,
                    "USER active rebind object");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], 0x05000005u,
                    "USER rebound state stored");
        ok &= check(nv2a_set_fixture_binding(gpu, 1, 0x2222, 0x97), 1,
                    "USER second subchannel registration");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | (1u << 13); pb[1] = 0x2222;
        pb[2] = (1u << 18) | (1u << 13) | 0x0204u; pb[3] = 0x06000006u;
        submit_reset(gpu, 0, 16);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER subchannel one state accepted");
        ok &= check(gpu->pfifo.binding_object[1], 0x2222,
                    "USER subchannel one active object");

        /* A wrong class and a later unsupported method roll back all staged
         * bindings and PGRAPH slots atomically. */
        ok &= check(nv2a_set_fixture_binding(gpu, 2, 0x3333, 0x98), 1,
                    "USER invalid class registration");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | (2u << 13); pb[1] = 0x3333;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0, "USER invalid class rejected");
        ok &= check(gpu->pfifo.binding_object[2], 0,
                    "USER invalid class active binding unchanged");
        uint32_t old_object = gpu->pfifo.binding_object[0];
        uint32_t old_clip_h = gpu->pgraph.regs[0x0200 / 4];
        uint32_t old_clip_v = gpu->pgraph.regs[0x0204 / 4];
        ok &= check(nv2a_set_fixture_binding(gpu, 0, 0x9abc, 0x97), 1,
                    "USER rollback object registration");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x9abc;
        pb[2] = (3u << 18) | 0x03fcu;
        pb[3] = 0x07000007u; pb[4] = 0x08000008u;
        pb[5] = 0x04000004u;
        submit_reset(gpu, 0, 24);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER removed surface format rollback");
        ok &= check(gpu->pfifo.binding_object[0], old_object,
                    "USER late unsupported binding rollback");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], old_clip_h,
                    "USER late unsupported state rollback");
        ok &= check(gpu->pgraph.regs[0x0204 / 4], old_clip_v,
                    "USER late unsupported vertical state rollback");
        ok &= check(gpu->pfifo.submit_diag_method, 0x03fc,
                    "USER removed format exact method diagnostic");
        ok &= check(gpu->pfifo.submit_diag_param, 0x07000007,
                    "USER removed format exact parameter diagnostic");

        /* Binding on another subchannel is outside the fixture contract and
         * must leave GET and the state shadow untouched. */
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | (3u << 13) | 0x0200u; pb[1] = 0xdeadbeef;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER unbound subchannel state method rejected");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 0,
                    "USER unbound method GET rollback");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], old_clip_h,
                    "USER unbound method state rollback");
        ok &= check(submit_diag_is(gpu, "unsupported_method"), 1,
                    "USER unbound method diagnostic");

        /* 11b4b3: production SET_OBJECT walks the claimed PRAMIN RAMHT. */
        ok &= check(nv2a_set_fixture_execution(gpu, false), 1,
                    "USER fixture gate off for RAMHT lookup");
        gpu->pfifo.regs[NV_PFIFO_RAMHT] = 0x03000000u;
        memset(instance, 0, 0x20000u);
        ramht_install(instance, 0xDu, 0x49Cu, NV_RAMHT_ENGINE_GRAPHICS,
                      0x97u, 0xA00u);
        /* Capture object word0 is 0x0000B03D; class is the low byte. */
        ramht_install(instance, 0x3u, 0x112u, 0, 0xB03Du, 0x7FFAFFFu);
        ramht_install(instance, 0x2u, 0x118u, 0, 0x3Du, 0x7FFAFFFu);
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0xDu;
        pb[2] = (2u << 18) | 0x0200u;
        pb[3] = 0x00100020u; pb[4] = 0x00200040u;
        submit_reset(gpu, 0, 20);
        ok &= check(nv2a_submit_pending(gpu), 1,
                    "USER RAMHT handle 0xD binds NV097");
        ok &= check(gpu->pfifo.binding_object[0], 0xDu,
                    "USER RAMHT active object is handle 0xD");
        ok &= check(gpu->pfifo.binding_class[0], 0x97u,
                    "USER RAMHT class is RAMIN word0");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], 0x00100020u,
                    "USER RAMHT NV097 clip H stored");
        ok &= check(gpu->pgraph.regs[0x0204 / 4], 0x00200040u,
                    "USER RAMHT NV097 clip V stored");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 20,
                    "USER RAMHT GET advances on accepted bind");

        uint32_t ramht_clip_h = gpu->pgraph.regs[0x0200 / 4];
        uint32_t ramht_clip_v = gpu->pgraph.regs[0x0204 / 4];
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x3u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 1,
                    "USER RAMHT DMA class 0x3D binds");
        ok &= check(gpu->pfifo.binding_class[0], 0x3Du,
                    "USER RAMHT class is low byte of 0xB03D");
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x0200u; pb[1] = 0x11111111u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER RAMHT non-NV097 clip rejected");
        ok &= check(gpu->pfifo.binding_object[0], 0x3u,
                    "USER RAMHT failed clip leaves 0x3D bind");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], ramht_clip_h,
                    "USER RAMHT failed clip rolls back H");

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0xDu;
        pb[2] = (3u << 18) | 0x03fcu;
        pb[3] = 0x07000007u; pb[4] = 0x08000008u;
        pb[5] = 0x04000004u;
        submit_reset(gpu, 0, 24);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER RAMHT late unsupported rolls back");
        ok &= check(gpu->pfifo.binding_object[0], 0x3u,
                    "USER RAMHT late unsupported object rollback");
        ok &= check(gpu->pgraph.regs[0x0200 / 4], ramht_clip_h,
                    "USER RAMHT late unsupported H rollback");
        ok &= check(gpu->pgraph.regs[0x0204 / 4], ramht_clip_v,
                    "USER RAMHT late unsupported V rollback");
        ok &= check(gpu->pfifo.submit_diag_method, 0x03fc,
                    "USER RAMHT late format method diagnostic");

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x999u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER missing RAMHT handle rejected");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER missing handle diagnostic");
        ok &= check(gpu->pfifo.submit_diag_param, 0x999u,
                    "USER missing handle parameter diagnostic");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 0,
                    "USER missing handle GET rollback");

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER zero handle rejected");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER zero handle diagnostic");

        {
            uint32_t *pair = (uint32_t *)(instance + 0x2u * 8u);
            pair[1] &= ~NV_RAMHT_STATUS;
            memset(pb, 0, 0x2000);
            pb[0] = (1u << 18); pb[1] = 0x2u;
            submit_reset(gpu, 0, 8);
            ok &= check(nv2a_submit_pending(gpu), 0,
                        "USER RAMHT invalid-bit rejected");
            ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                        "USER invalid-bit diagnostic");
            pair[1] |= NV_RAMHT_STATUS;
        }

        {
            uint32_t *pair = (uint32_t *)(instance + 0x2u * 8u);
            uint32_t saved = pair[0];
            pair[0] = 0x20u;
            memset(pb, 0, 0x2000);
            pb[0] = (1u << 18); pb[1] = 0x2u;
            submit_reset(gpu, 0, 8);
            ok &= check(nv2a_submit_pending(gpu), 0,
                        "USER RAMHT handle mismatch rejected");
            ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                        "USER handle mismatch diagnostic");
            pair[0] = saved;
        }

        {
            uint32_t *pair = (uint32_t *)(instance + 0x4u * 8u);
            pair[0] = 0x4u;
            pair[1] = NV_RAMHT_STATUS | 0x2000u;
        }
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x4u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER RAMHT instance outside PRAMIN rejected");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER out-of-bounds instance diagnostic");

        ramht_install(instance, 0x5u, 0x120u, 0, 0, 0);
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x5u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER RAMHT class-zero object rejected");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER class-zero diagnostic");

        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18); pb[1] = 0x200u;
        submit_reset(gpu, 0, 8);
        ok &= check(nv2a_submit_pending(gpu), 0,
                    "USER 11-bit hash outside 4K RAMHT rejected");
        ok &= check(submit_diag_is(gpu, "invalid_handle"), 1,
                    "USER oversized hash diagnostic");

        memset(pb, 0, 0x2000);
        pb[0x7fe] = (1u << 18) | 0x100u; pb[0x7ff] = 0xabcdef01;
        submit_reset(gpu, 0x1ff8, 0);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER ring wrap accepted");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 0,
                    "USER ring wrap GET exact");

        memset(pb, 0, 0x2000);
        pb[0] = 0x20000008u; pb[2] = (1u << 18) | 0x100u; pb[3] = 7;
        submit_reset(gpu, 0, 16);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER old jump accepted");
        memset(pb, 0, 0x2000);
        pb[0] = 0x00000009u; pb[2] = (1u << 18) | 0x100u; pb[3] = 8;
        submit_reset(gpu, 0, 16);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER jump accepted");
        memset(pb, 0, 0x2000);
        /* The subroutine lies beyond PUT. RETURN restores PC=4, where the
         * method packet consumes through the distinct PUT at byte 12. */
        pb[0] = 0x00000012u; pb[1] = (1u << 18) | 0x100u; pb[2] = 9;
        pb[4] = 0x00020000u;
        submit_reset(gpu, 0, 12);
        ok &= check(nv2a_submit_pending(gpu), 1, "USER bounded call/return accepted");
        ok &= check(gpu->pfifo.submit_packets, 3,
                    "USER call/return executes call return and resumed packet");
        ok &= check(gpu->pfifo.sink_count, 1,
                    "USER return resumes saved method stream");
        ok &= check(gpu->pfifo.sink[0].method, 0x100,
                    "USER return restores saved method PC");
        ok &= check(gpu->pfifo.sink[0].param, 9,
                    "USER return restores saved parameter PC");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 12,
                    "USER return path reaches distinct PUT");

#define REJECT_CASE(name_, get_, put_, diag_) do { \
            uint32_t old_sink = gpu->pfifo.sink_count; \
            uint32_t old_success = gpu->pfifo.submit_successes; \
            uint32_t old_method = gpu->pfifo.submit_last_method; \
            uint32_t old_param = gpu->pfifo.submit_last_param; \
            uint32_t old_get = (get_); \
            gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = (get_); \
            gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = (put_); \
            ok &= check(nv2a_submit_pending(gpu), 0, name_ " rejected"); \
            ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], old_get, name_ " GET unchanged"); \
            ok &= check(gpu->pfifo.sink_count, old_sink, name_ " sink unchanged"); \
            ok &= check(gpu->pfifo.submit_successes, old_success, name_ " acceptance count unchanged"); \
            ok &= check(gpu->pfifo.submit_last_method, old_method, name_ " last method unchanged"); \
            ok &= check(gpu->pfifo.submit_last_param, old_param, name_ " last param unchanged"); \
            ok &= check(submit_diag_is(gpu, diag_), 1, name_ " diagnostic"); \
        } while (0)

        {
            /* Methods the title never submits, so they are absent from the
             * generated table and must still reject. 0x208/0x20C/0x210/0x214 used
             * to be here; decoding JSRF's own pushbuffer showed it does submit
             * them, so they are implemented now. The assertion is the same -- an
             * unimplemented method rejects the stream and rolls it back. */
            static const uint32_t removed_surface[][2] = {
                { 0x03fcu, 0x04000004u },
                { 0x04fcu, 0x00000800u },
                { 0x0ffcu, 0x00100000u },
                { 0x17fcu, 0x00200000u },
            };
            uint32_t clip_h = gpu->pgraph.regs[0x0200 / 4];
            uint32_t clip_v = gpu->pgraph.regs[0x0204 / 4];
            for (unsigned i = 0; i < sizeof(removed_surface) / sizeof(removed_surface[0]); ++i) {
                memset(pb, 0, 0x2000);
                pb[0] = (1u << 18) | removed_surface[i][0];
                pb[1] = removed_surface[i][1];
                submit_reset(gpu, 0, 8);
                REJECT_CASE("USER unmodeled surface method", 0, 8,
                            "unsupported_method");
                ok &= check(gpu->pfifo.submit_diag_method,
                            removed_surface[i][0],
                            "USER unmodeled surface exact method diagnostic");
                ok &= check(gpu->pfifo.submit_diag_param,
                            removed_surface[i][1],
                            "USER unmodeled surface exact parameter diagnostic");
                ok &= check(gpu->pgraph.regs[0x0200 / 4], clip_h,
                            "USER unmodeled surface horizontal clip unchanged");
                ok &= check(gpu->pgraph.regs[0x0204 / 4], clip_v,
                            "USER unmodeled surface vertical clip unchanged");
            }
        }

        submit_reset(gpu, 0, 8); pb[0] = 0x80000001u;
        REJECT_CASE("USER high-bit target", 0, 8, "invalid_target");
        submit_reset(gpu, 0x80000000u, 4);
        REJECT_CASE("USER high-bit GET", 0x80000000u, 4, "invalid_get_put");
        submit_reset(gpu, 0, 0x80000000u);
        REJECT_CASE("USER high-bit PUT", 0, 0x80000000u, "invalid_get_put");

        memset(pb, 0, 0x2000); submit_reset(gpu, 0, 8);
        pb[0] = (2u << 18) | 0x100u; pb[1] = 1;
        REJECT_CASE("USER truncated packet", 0, 8, "truncated_packet");

        memset(pb, 0, 0x2000); submit_reset(gpu, 0, 16);
        pb[0] = (1u << 18) | 0x100u; pb[1] = 1; pb[2] = 0x60000000u;
        REJECT_CASE("USER whole-stream atomicity", 0, 12, "reserved_opcode");

        memset(pb, 0, 0x2000); submit_reset(gpu, 0, 16);
        pb[0] = (1u << 18) | 0x100u; pb[1] = 1;
        pb[2] = (1u << 18) | (5u << 13) | 0x180u; pb[3] = 0xabcdef01u;
        REJECT_CASE("USER unsupported method atomicity", 0, 16, "unsupported_method");
        ok &= check(gpu->pfifo.submit_diag_subchannel, 5, "USER unsupported subchannel diagnostic");
        ok &= check(gpu->pfifo.submit_diag_method, 0x180, "USER unsupported method diagnostic");
        ok &= check(gpu->pfifo.submit_diag_param, 0xabcdef01, "USER unsupported parameter diagnostic");

        memset(pb, 0, 0x2000); pb[0] = 1;
        submit_reset(gpu, 0, 4);
        REJECT_CASE("USER control loop", 0, 4, "control_flow_loop");

        memset(pb, 0, 0x2000);
        /* Valid NOP packets (count 0, method 0x100), one word each, so the
         * stream actually reaches the packet budget. All-zero words would fail
         * earlier as a SET_OBJECT with handle 0 and never test the budget. */
        for (unsigned i = 0; i < 1025; ++i) pb[i] = 0x100u;
        submit_reset(gpu, 0, 1025 * 4);
        REJECT_CASE("USER packet/word budget", 0, 1025 * 4, "budget_exhausted");

        memset(pb, 0, 0x2000); pb[0] = (2u << 18) | 0x1ffcu;
        submit_reset(gpu, 0, 12);
        REJECT_CASE("USER method range", 0, 12, "method_range_overflow");

        memset(pb, 0, 0x2000); pb[0] = (1025u << 18) | 0x100u;
        submit_reset(gpu, 0, 0x1008);
        REJECT_CASE("USER sink capacity", 0, 0x408, "sink_capacity");

        memset(pb, 0, 0x2000); pb[0x3fe] = (2u << 18) | 0x100u; pb[0x3ff] = 1;
        ok &= check(VirtualProtect((uint8_t *)pb + 0x1000, 0x1000,
                                   PAGE_NOACCESS, &old_protect), 1,
                    "USER unreadable page installed");
        submit_reset(gpu, 0xff8, 0x1004);
        REJECT_CASE("USER unreadable span", 0xff8, 0x1004, "unreadable_pushbuffer");
        VirtualProtect((uint8_t *)pb + 0x1000, 0x1000, old_protect, &old_protect);

        /* PUT is a plain ring offset with no latch bits.  JSRF's
         * set-bit-16-and-spin at 0x00191270 targets 0x100410, NV_PFB_WBC,
         * whose flush reads idle (checked with the PCI identity above). */
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | 0x100u; pb[1] = 0xdeadbeefu;
        submit_reset(gpu, 0, 0);
        nv2a_mmio_write(gpu, 0x800040, 8, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x800040, 4), 8,
                    "USER PUT stores its offset verbatim");
        ok &= check(nv2a_mmio_read(gpu, 0x800044, 4), 8,
                    "USER PUT write runs the pending submission (GET advanced)");
        ok &= check(gpu->pfifo.sink_count, 1, "USER PUT stream reached the sink");

        /* Bit 16 is an ordinary offset bit once the ring passes 64 KB; masking
         * it walks the ring 64 KB short.  GET already equals PUT here, so
         * nothing is walked outside the fixture buffer. */
        submit_reset(gpu, 0x10008u, 0x10008u);
        nv2a_mmio_write(gpu, 0x800040, 0x10008u, 4);
        ok &= check(nv2a_mmio_read(gpu, 0x800040, 4), 0x10008u,
                    "USER PUT keeps bit 16");
        ok &= check(nv2a_mmio_read(gpu, 0x800044, 4), 0x10008u,
                    "USER PUT with bit 16 is not walked as a lower offset");

        /* A blocked stream leaves GET where it stopped and says why. */
        memset(pb, 0, 0x2000);
        pb[0] = (1u << 18) | (6u << 13) | 0x180u; pb[1] = 0x99;
        submit_reset(gpu, 0, 0);
        nv2a_mmio_write(gpu, 0x800040, 8, 4);
        ok &= check(submit_diag_is(gpu, "unsupported_method"), 1,
                    "USER blocked stream reports why it blocked");
        ok &= check(gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET], 0,
                    "USER blocked stream leaves GET untouched");
#undef REJECT_CASE
        VirtualFree(pb, 0, MEM_RELEASE);
    }
    /* ── Interrupt controller: source, masks, acknowledgment ──────────
     *
     * A2. The chain the title waits on is: display clock -> PCRTC pending bit
     * -> PMC summary bit 24 -> the card's line -> the guest's ISR 0x00193C50
     * -> KeInsertQueueDpc -> the DPC routine 0x00194480 -> 0x00193D90 ->
     * KeSetEvent on the title's frame event. Everything downstream of the
     * line is guest code; what this fixture pins is the model's half, and in
     * particular that the guest's own write-1-to-clear is the only thing that
     * clears a pending bit. A model that clears its own pending bit would
     * make the guest's acknowledgment untestable and the whole run synthetic.
     */
    {
        struct irq_capture capture = { 0, -1 };

        /* Normalise: no block has anything pending or enabled, and the PMC
         * aggregate reflects that. */
        gpu->pfifo.pending_interrupts = 0;  gpu->pfifo.enabled_interrupts = 0;
        gpu->pgraph.pending_interrupts = 0; gpu->pgraph.enabled_interrupts = 0;
        gpu->ptimer.pending_interrupts = 0; gpu->ptimer.enabled_interrupts = 0;
        gpu->pcrtc.pending_interrupts = 0;  gpu->pcrtc.enabled_interrupts = 0;
        nv2a_mmio_write(gpu, 0x000140, 0, 4);          /* PMC master enable off */
        nv2a_mmio_write(gpu, 0x600140, 0, 4);          /* PCRTC vblank enable off */
        nv2a_mmio_write(gpu, 0x600100, 0xFFFFFFFFu, 4);
        nv2a_mmio_write(gpu, 0x000100, 0xFFFFFFFFu, 4);
        nv2a_set_irq_sink(gpu, irq_capture_sink, &capture);
        ok &= check(nv2a_irq_line_asserted(gpu), 0, "line starts deasserted");
        ok &= check((unsigned)capture.transitions, 0, "no line transition before a source");

        /* The source is unconditional; the masks gate delivery, not the bit. */
        nv2a_vblank_pulse(gpu);
        ok &= check((gpu->pcrtc.pending_interrupts & NV_PCRTC_INTR_0_VBLANK) != 0, 1,
                    "vblank pulse sets the PCRTC pending bit with everything disabled");
        ok &= check((gpu->pmc.pending_interrupts & NV_PMC_INTR_0_PCRTC) != 0, 0,
                    "block-disabled vblank does not reach the PMC summary");
        ok &= check(nv2a_irq_line_asserted(gpu), 0, "block-disabled vblank asserts no line");
        ok &= check((unsigned)capture.transitions, 0, "block-disabled vblank reports no transition");

        /* Block enable, master still off: summary yes, line no. */
        nv2a_mmio_write(gpu, 0x600140, 1, 4);
        ok &= check((gpu->pmc.pending_interrupts & NV_PMC_INTR_0_PCRTC) != 0, 1,
                    "block-enabled vblank reaches the PMC summary");
        ok &= check(nv2a_irq_line_asserted(gpu), 0, "master-disabled summary asserts no line");
        ok &= check((unsigned)capture.transitions, 0, "master-disabled summary reports no transition");

        /* Master enable: now the line rises, once. */
        nv2a_mmio_write(gpu, 0x000140, 1, 4);
        ok &= check(nv2a_irq_line_asserted(gpu), 1, "master-enabled summary asserts the line");
        ok &= check((unsigned)capture.transitions, 1, "line rise reported exactly once");

        /* No spurious repeat: further frames while unacknowledged keep the
         * same level and must not re-report it. */
        nv2a_vblank_pulse(gpu);
        nv2a_vblank_pulse(gpu);
        ok &= check((gpu->pcrtc.pending_interrupts & NV_PCRTC_INTR_0_VBLANK) != 0, 1,
                    "unacknowledged vblanks leave the source asserted");
        ok &= check((unsigned)capture.transitions, 1, "unacknowledged vblanks do not re-report the line");

        /* A W1C that does not name the vblank bit must not clear it. */
        nv2a_mmio_write(gpu, 0x600100, 0xFFFFFFFEu, 4);
        ok &= check((gpu->pcrtc.pending_interrupts & NV_PCRTC_INTR_0_VBLANK) != 0, 1,
                    "W1C of another PCRTC bit leaves vblank pending");

        /* The guest's acknowledgment: its own write-1-to-clear. */
        nv2a_mmio_write(gpu, 0x600100, NV_PCRTC_INTR_0_VBLANK, 4);
        ok &= check((gpu->pcrtc.pending_interrupts & NV_PCRTC_INTR_0_VBLANK) != 0, 0,
                    "guest W1C clears the PCRTC pending bit");
        ok &= check((gpu->pmc.pending_interrupts & NV_PMC_INTR_0_PCRTC) != 0, 0,
                    "acknowledged vblank drops out of the PMC summary");
        ok &= check(nv2a_irq_line_asserted(gpu), 0, "acknowledged vblank deasserts the line");
        ok &= check((unsigned)capture.transitions, 2, "line fall reported exactly once");

        /* And the next frame is delivered again. */
        nv2a_vblank_pulse(gpu);
        ok &= check((unsigned)capture.transitions, 3, "next frame re-raises the line");

        /* Masking after the fact stops delivery without touching the source. */
        nv2a_mmio_write(gpu, 0x600140, 0, 4);
        ok &= check((gpu->pcrtc.pending_interrupts & NV_PCRTC_INTR_0_VBLANK) != 0, 1,
                    "masking the block leaves the source asserted");
        ok &= check(nv2a_irq_line_asserted(gpu), 0, "masking the block deasserts the line");
        nv2a_set_irq_sink(gpu, NULL, NULL);
    }
    {
        uint64_t frame_ns = nv2a_display_frame_ns(gpu);
        ok &= check(frame_ns >= 1000000000ull / 240ull &&
                    frame_ns <= 1000000000ull / 40ull, 1,
                    "display frame period is a plausible refresh");
        ok &= check(nv2a_display_frame_source() != NULL, 1,
                    "display frame period reports its source");
    }
    if (ok) printf("PASS: %u NV2A register/clock contracts (no renderer)\n", checks);
    /* State is process-owned; the standalone core has no teardown API yet. */
    return ok ? 0 : 1;
}
