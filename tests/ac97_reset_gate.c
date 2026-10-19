/*
 * ac97_reset_gate.c -- the independent behavioural gate for the C3-RR channel-reset repair.
 *
 * WHAT THIS IS. The production code under test is the NABM owner in
 * toolkit src/kernel/xbox_memory_layout.c (ac97m_arm / ac97m_veh, decoding through
 * src/platform/mmio_decode.h against private s_ac97m_regs under one leaf
 * CRITICAL_SECTION). This gate drives the PRODUCTION path wherever possible: after
 * xbox_MemoryLayoutInit on the real XBE it performs REAL faulting stores and loads
 * through the guest mapping at guest 0xFEC00xxx and asserts the values read back.
 * The test seams (xbox_Ac97ModelFixture*) are used ONLY for state the public
 * register interface cannot reach: seeding a prior control value that already has
 * RR set (every real write clears RR, so no real sequence can produce that prior),
 * reading the serviced/refused counters, and driving the tick's coherent GC/GS
 * transaction deterministically. Private unit-policy coverage is labelled as such;
 * the refusal rows drive crafted contexts (legitimate unit coverage, said so).
 *
 * WHAT IT ASSERTS (from pinned xemu hw/audio/ac97.c @ 2799183e, TR 31.46/31.47):
 *  - a byte write to a defined channel CR with RR set -> CR = prior & 0x1C
 *    (RR and RPBM cleared, bits 2-4 from the PRIOR state), and BDBAR/CIV/LVI/
 *    PICB/PIV zeroed, SR = halted (DCH=1);
 *  - a CR write without RR -> CR = value & 0x1F, and if bit0 clear SR keeps DCH;
 *  - SR low byte (pinned L833-836, L847-848: RO=DCH|CELV kept, W1C=FIFOE|BCIS|
 *    LVBCI cleared, bits 5-7 stored): DCH/CELV can neither be set nor cleared,
 *    W1C bits clear on 1 but never set (byte rows, high byte kept zero);
 *  - LVI byte (pinned L817: val % 32, i.e. low 5 bits): 0x22 -> 0x02, 0xFF -> 0x1F;
 *  - BDBAR dword (pinned L891: val & ~3): 0xDEADBEEF -> 0xDEADBEEC (word/byte
 *    forms NOT asserted: the pinned byte path has no BDBAR case and ignores
 *    them, so there is no pinned expectation to check);
 *  - reserved CR-shaped bytes 0x3B/0x4B/0x5B/0x6B are NEVER masked (plain storage);
 *  - GLOB_CNT/GLOB_STA are the RESTORED contract (plain storage + L20), NOT the
 *    pinned GC_VALID_MASK / GS W1C+RO policy this packet removed: GLOB_CNT
 *    round-trips as written; every GLOB_STA bit round-trips except bit 8, which
 *    is always the level of GLOB_CNT bit 1 (write-path sync + tick). These rows
 *    kill a re-added W1C/RO policy;
 *  - every other byte of the 4 KB region is plain storage (today's behaviour).
 *
 * GEOMETRY (page offsets; NABM block = page+0x100, 128 bytes; channels at
 * NABM +0x00/+0x10/+0x20/+0x70; CR at block+0x0B, SR word at +0x06, BDBAR dword
 * at +0x00, CIV +0x04, LVI +0x05, PICB word +0x08, PIV +0x0A; GLOB_CNT page+0x12C,
 * GLOB_STA +0x130, CAS +0x134):
 *  - guest 0xFEC00000 + page_off. PO CR byte: guest 0xFEC0011B.
 *    SO CR byte: guest 0xFEC0017B.
 *
 * EXIT CODE: 0 when every assertion passes, 1 when any assertion fails, 2 for
 * harness errors (layout not initialised, page not protected, XBE unreadable).
 * Assertion FAILURES (FAIL [...]) are kept separate from harness errors
 * (HARNESS [...]) and from skips (SKIP [...]); skips never fail the run.
 */
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern void xbox_MemoryLayoutShutdown(void);

/* The test seams the production code exports (the ONLY bypass; see the header
 * comment for why each use is legitimate). Deliberately absent from every
 * public header. */
extern int      xbox_Ac97ModelFixtureArmed(void);
extern void     xbox_Ac97ModelFixtureSeed(uint32_t page_off, uint32_t val, int size);
extern uint32_t xbox_Ac97ModelFixturePeek(uint32_t page_off, int size);
extern uint32_t xbox_Ac97ModelFixtureServiced(void);
extern uint32_t xbox_Ac97ModelFixtureRefused(void);
extern void     xbox_Ac97ModelFixtureTick(void);

/* The kernel bridge references the game dispatcher/diagnostics; this focused
 * gate does not start guest threads, so keep those hooks inert (same stubs as
 * tests/test_gpu_backend_parity.c). */
typedef void (*recomp_func_t)(void);
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site, uint32_t value)
{ (void)kind; (void)target; (void)site; (void)value; }
void jsrf_slot_latch_install(uint32_t raw_value, uint32_t installed_value)
{ (void)raw_value; (void)installed_value; }
void jsrf_slot_latch_sample(uint32_t tid, uint32_t call_index, uint32_t ordinal,
                             uint32_t before, uint32_t after)
{ (void)tid; (void)call_index; (void)ordinal; (void)before; (void)after; }
void jsrf_slot_watch_handshake(uint32_t slot_va) { (void)slot_va; }
void jsrf_slot_watch_alias_armed(uint32_t mapped_mask, uint32_t protect_mask, uint32_t alias_count)
{ (void)mapped_mask; (void)protect_mask; (void)alias_count; }
int jsrf_slot_watch_alias_touch(uint32_t alias_index, uint32_t fault_va, uint64_t rip,
                                uint32_t value, uint32_t published)
{ (void)alias_index; (void)fault_va; (void)rip; (void)value; (void)published; return 0; }
void jsrf_slot_watch_write(uint32_t provenance, uint32_t before, uint32_t after,
                           uint64_t rip, uint32_t ordinal)
{ (void)provenance; (void)before; (void)after; (void)rip; (void)ordinal; }

/* ── geometry ──────────────────────────────────────────────────────────────── */
#define NAC97_PAGE_GUEST 0xFEC00000u
#define NAC97_NABM       0x100u
/* Channel block bases as PAGE offsets (NABM 0x00/0x10/0x20/0x70 at page+0x100). */
#define NAC97_PI         0x100u
#define NAC97_PO         0x110u
#define NAC97_MC         0x120u
#define NAC97_SO         0x170u
#define NAC97_GC         0x12Cu
#define NAC97_GS         0x130u
#define NAC97_CAS        0x134u
#define NAC97_NAM0       0x000u

static unsigned checks_run, checks_failed, harness_errors, skips;
static const char *current_case = "?";
static char *g_page; /* host address of guest 0xFEC00000 */

#define CHECK(cond, ...) do {                                                  \
        checks_run++;                                                          \
        if (!(cond)) {                                                         \
            checks_failed++;                                                   \
            fprintf(stderr, "FAIL [%s] %s:%d: ", current_case, __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                                      \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

#define HARNESS(...) do {                                                      \
        harness_errors++;                                                      \
        fprintf(stderr, "HARNESS [%s] %s:%d: ", current_case, __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define SKIP(...) do {                                                         \
        skips++;                                                               \
        fprintf(stderr, "SKIP [%s]: ", current_case);                           \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)


/* ── real faulting access through the guest mapping ──────────────────────────
 * The page is PAGE_NOACCESS once armed, so every access below faults into the
 * production VEH and is decoded from the faulting instruction. A plain byte of
 * host C would be optimised into a wider or reordered access; the volatile
 * qualifier plus a compiler barrier keeps each probe exactly one guest access
 * of the intended width. */
static void g8_store(uint32_t page_off, uint8_t v)
{
    volatile uint8_t *p = (volatile uint8_t *)(g_page + page_off);
    *p = v;
    MemoryBarrier();
}

static uint8_t g8_load(uint32_t page_off)
{
    volatile uint8_t *p = (volatile uint8_t *)(g_page + page_off);
    uint8_t v = *p;
    MemoryBarrier();
    return v;
}

static void g32_store(uint32_t page_off, uint32_t v)
{
    volatile uint32_t *p = (volatile uint32_t *)(g_page + page_off);
    *p = v;
    MemoryBarrier();
}

static uint32_t g32_load(uint32_t page_off)
{
    volatile uint32_t *p = (volatile uint32_t *)(g_page + page_off);
    uint32_t v = *p;
    MemoryBarrier();
    return v;
}

/* Snapshot one full channel block through REAL guest loads: BDBAR (dword at
 * block+0x00), CIV (+0x04), LVI (+0x05), SR (word +0x06), PICB (word +0x08),
 * PIV (+0x0A), CR (+0x0B).
 *
 * ADDRESS-FORM DISCIPLINE (load-bearing): every faulting access below uses ONE
 * dynamic register plus a constant displacement ([reg+disp], ModRM only). That
 * is a deliberate documented subset, not a claim about the decoder: the decoder
 * is FIXED (mmio_decode.h `int base = (s & 7) | (rex_b ? 8 : 0);`), and the
 * SIB+extended-base form is exercised by the crafted-context
 * sib-extended-base-decode row instead. Computing `blk` once with LEA (no
 * fault) and indexing it with constants keeps every fault on a decoder-covered
 * form and keeps the value rows independent of decoder-form coverage. */
typedef struct {
    uint32_t bdbar;
    uint8_t civ, lvi;
    uint16_t sr, picb;
    uint8_t piv, cr;
} chan_snap_t;

static chan_snap_t snap_channel(uint32_t base)
{
    volatile uint8_t *blk = (volatile uint8_t *)(g_page + base); /* LEA: no fault */
    chan_snap_t s;
    s.bdbar = (uint32_t)blk[0x00] | ((uint32_t)blk[0x01] << 8)
            | ((uint32_t)blk[0x02] << 16) | ((uint32_t)blk[0x03] << 24);
    s.civ = blk[0x04];
    s.lvi = blk[0x05];
    s.sr = (uint16_t)(blk[0x06] | ((uint16_t)blk[0x07] << 8));
    s.picb = (uint16_t)(blk[0x08] | ((uint16_t)blk[0x09] << 8));
    s.piv = blk[0x0A];
    s.cr = blk[0x0B];
    MemoryBarrier();
    return s;
}

/* Seed one full channel block through the seam (setup only, never the probe:
 * every assertion below reads through REAL guest loads). */
static void seed_channel(uint32_t base, uint32_t bdbar, uint8_t civ, uint8_t lvi,
                         uint16_t sr, uint16_t picb, uint8_t piv, uint8_t cr)
{
    xbox_Ac97ModelFixtureSeed(base + 0x00, bdbar, 4);
    xbox_Ac97ModelFixtureSeed(base + 0x04, civ, 1);
    xbox_Ac97ModelFixtureSeed(base + 0x05, lvi, 1);
    xbox_Ac97ModelFixtureSeed(base + 0x06, sr, 2);
    xbox_Ac97ModelFixtureSeed(base + 0x08, picb, 2);
    xbox_Ac97ModelFixtureSeed(base + 0x0A, piv, 1);
    xbox_Ac97ModelFixtureSeed(base + 0x0B, cr, 1);
}

/* ── the deciding rows ───────────────────────────────────────────────────────
 * Each MUST fail on the pre-repair source (plain RAM echoes the stored byte)
 * and pass on the final source. Every probe is a REAL faulting store/load. */

/* Deciding-1: a fresh PO CR (prior 0x00) stored with RR -> expect 0x00.
 * On the pre-repair source the read echoes the stored 0x02. */
static void case_deciding_po_reset_fresh(void)
{
    uint8_t got;
    current_case = "deciding-po-reset-fresh";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x02);
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x00, "PO CR reset: expected 0x00, got 0x%02X", got);
}

/* Deciding-2: legal seeding, prior 0x1C then reset -> prior & 0x1C = 0x1C. */
static void case_deciding_po_reset_seeded_1c(void)
{
    uint8_t got;
    current_case = "deciding-po-reset-seeded-1c";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x1C); /* public write, no RR: stores 0x1C */
    g8_store(NAC97_PO + 0x0B, 0x02); /* reset: 0x1C & 0x1C */
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x1C, "PO CR reset after 0x1C seed: expected 0x1C, got 0x%02X", got);
}

/* Deciding-3: legal seeding, prior 0x1D then reset -> 0x1D & 0x1C = 0x1C. */
static void case_deciding_po_reset_seeded_1d(void)
{
    uint8_t got;
    current_case = "deciding-po-reset-seeded-1d";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x1D);
    g8_store(NAC97_PO + 0x0B, 0x02);
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x1C, "PO CR reset after 0x1D seed: expected 0x1C, got 0x%02X", got);
}

/* Deciding-4 (the mutant-killer): prior 0x00, written byte 0x1E (RR set, since
 * 0x1E = 11110b has bit1 SET) -> expect 0x00. The correct model keeps
 * prior & 0x1C = 0x00; a mask-the-written-byte model (value & ~2) gives 0x1C
 * here, so this row distinguishes prior&0x1C from value&~2. */
static void case_deciding_po_reset_fresh_1e(void)
{
    uint8_t got;
    current_case = "deciding-po-reset-fresh-1e";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x1E); /* RR set (bit1), RPBM clear */
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x00, "PO CR reset 0x1E on fresh CR: expected 0x00, got 0x%02X", got);
}
/* Deciding-5: SO block CR at guest 0xFEC0017B: store 0x02, read ->
 * expect that channel's prior & 0x1C. */
static void case_deciding_so_reset(void)
{
    uint8_t got;
    current_case = "deciding-so-reset";
    seed_channel(NAC97_SO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_SO + 0x0B, 0x1D); /* prior = 0x1D through the public path */
    g8_store(NAC97_SO + 0x0B, 0x02);
    got = g8_load(NAC97_SO + 0x0B);
    CHECK(got == 0x1C, "SO CR reset after 0x1D seed: expected 0x1C, got 0x%02X", got);
}

/* Unaffected positive (must pass on BOTH sources): store 0x1D (RR clear) to a
 * fresh CR -> expect 0x1D. */
static void case_positive_nonrr_write(void)
{
    uint8_t got;
    current_case = "positive-nonrr-write";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x1D);
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x1D, "PO CR non-RR write 0x1D: expected 0x1D, got 0x%02X", got);
}

/* Private unit-policy coverage ONLY (labelled; seeded through the seam because
 * no public write can produce it): prior 0x1F then reset 0x02 -> 0x1C. */
static void case_unit_prior_1f_reset(void)
{
    uint8_t got;
    current_case = "unit-prior-1f-reset";
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x1F); /* seam: RR already set in the PRIOR */
    g8_store(NAC97_PO + 0x0B, 0x02); /* the probe is still a REAL faulting store */
    got = g8_load(NAC97_PO + 0x0B);
    CHECK(got == 0x1C, "unit-policy prior 0x1F reset: expected 0x1C, got 0x%02X", got);
}

/* ── SR / LVI / BDBAR deciding rows ───────────────────────────────────────────
 * Expectations below are computed from the PINNED source (xemu hw/audio/ac97.c
 * @ 2799183e), never from the model's answer and never from Intel alone:
 *  - SR byte (pinned L833-836, L34-41, L847-848): RO = DCH|CELV = 0x0003 kept,
 *    W1C = FIFOE|BCIS|LVBCI = 0x001C cleared on write-1, every other bit
 *    (5-7, i.e. 0x00E0) stored via `v & ~(RO|W1C)`. All rows keep the SR high
 *    byte zero unless stated.
 *  - LVI byte (pinned L817): `lvi = val % 32` (low 5 bits kept).
 *  - BDBAR dword (pinned L891): `bdbar = val & ~3`. The pinned byte path
 *    (nabm_writeb) has NO BDBAR case and ignores it, so there is no pinned
 *    expectation for word/byte BDBAR forms and none is asserted here.
 * Every probe is a REAL faulting guest store/load through page 0xFEC00xxx. */
static void case_sr_readonly_w1c(void)
{
    uint16_t got;
    current_case = "sr-readonly-w1c";
    /* Fresh block: SR = 0x0001. A store of 0x00 must leave DCH set (read-only:
     * neither the RO bit nor any W1C bit can be changed by writing 0). */
    seed_channel(NAC97_PI, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0x00);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0001u, "SR store 0x00 on 0x0001: expected 0x0001, got 0x%04X", got);
    /* Writing 0x01 cannot CLEAR DCH (read-only). */
    seed_channel(NAC97_PI, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0x01);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0001u, "SR store 0x01 on 0x0001: expected 0x0001, got 0x%04X", got);
    /* Writing 0x02 cannot SET CELV (read-only). */
    seed_channel(NAC97_PI, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0x02);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0001u, "SR store 0x02 on 0x0001: expected 0x0001, got 0x%04X", got);
    /* Seed SR = 0x001D (DCH + LVBCI + BCIS), store 0x04: LVBCI is
     * write-1-clear, so 0x001D & ~0x0004 = 0x0019. */
    seed_channel(NAC97_PI, 0, 0, 0, 0x001D, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0x04);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0019u, "SR store 0x04 on 0x001D: expected 0x0019, got 0x%04X", got);
    /* Then store 0x18 on the resulting 0x0019: FIFOE|BCIS clear, leaving DCH. */
    g8_store(NAC97_PI + 0x06, 0x18);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0001u, "SR store 0x18 on 0x0019: expected 0x0001, got 0x%04X", got);
    /* Fresh SR = 0x0001: W1C bits cannot be SET by writing 1s (0x1C has no
     * stored bit to OR in, and the W1C clear keeps them clear). */
    seed_channel(NAC97_PI, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0x1C);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x0001u, "SR store 0x1C on 0x0001: expected 0x0001, got 0x%04X", got);
    /* Bits 5-7 store: 0xE0 on 0x0001 -> 0x00E0 | 0x0001 = 0x00E1. */
    seed_channel(NAC97_PI, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PI + 0x06, 0xE0);
    got = (uint16_t)(g8_load(NAC97_PI + 0x06) | ((uint16_t)g8_load(NAC97_PI + 0x07) << 8));
    CHECK(got == 0x00E1u, "SR store 0xE0 on 0x0001: expected 0x00E1, got 0x%04X", got);
}

static void case_lvi_mask(void)
{
    uint8_t got;
    current_case = "lvi-mask";
    /* Pinned L817: lvi = val % 32. 0x22 = 34 % 32 = 2. */
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x05, 0x22);
    got = g8_load(NAC97_PO + 0x05);
    CHECK(got == 0x02, "LVI store 0x22: expected 0x02, got 0x%02X", got);
    /* 0xFF = 255 % 32 = 31 = 0x1F. */
    g8_store(NAC97_PO + 0x05, 0xFF);
    got = g8_load(NAC97_PO + 0x05);
    CHECK(got == 0x1F, "LVI store 0xFF: expected 0x1F, got 0x%02X", got);
}

static void case_bdbar_mask(void)
{
    uint32_t got;
    current_case = "bdbar-mask";
    /* Pinned L891: bdbar = val & ~3. 0xDEADBEEF & ~3 = 0xDEADBEEC.
     * Word/byte BDBAR forms are deliberately NOT asserted: the pinned byte
     * path does not implement them, so there is no pinned expectation. */
    seed_channel(NAC97_MC, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g32_store(NAC97_MC + 0x00, 0xDEADBEEFu);
    got = g32_load(NAC97_MC + 0x00);
    CHECK(got == 0xDEADBEECu, "BDBAR dword store 0xDEADBEEF: expected 0xDEADBEEC, got 0x%08X", got);
}

/* ── complete observable reset state ───────────────────────────────────────── */
static void case_full_reset_state(void)
{
    chan_snap_t s, next;
    current_case = "full-reset-state";
    /* Plant nonzero register state through the seam (setup), then reset
     * through a REAL guest store and snapshot through REAL guest loads. */
    seed_channel(NAC97_PO, 0xDEADBEEFu, 0x07, 0x1F, 0x0001, 0x00AB, 0x0D, 0x1D);
    g8_store(NAC97_PO + 0x0B, 0x02);
    s = snap_channel(NAC97_PO);
    CHECK(s.cr == 0x1C, "reset CR: expected 0x1C, got 0x%02X", s.cr);
    CHECK(s.bdbar == 0, "reset BDBAR: expected 0, got 0x%08X", s.bdbar);
    CHECK(s.civ == 0, "reset CIV: expected 0, got 0x%02X", s.civ);
    CHECK(s.lvi == 0, "reset LVI: expected 0, got 0x%02X", s.lvi);
    CHECK(s.picb == 0, "reset PICB: expected 0, got 0x%04X", s.picb);
    CHECK(s.piv == 0, "reset PIV: expected 0, got 0x%02X", s.piv);
    CHECK((s.sr & 0x0001u) != 0, "reset SR DCH: expected bit0 set, got 0x%04X", s.sr);
    CHECK(s.sr == 0x0001u, "reset SR full: expected 0x0001, got 0x%04X", s.sr);

    /* The NEXT channel (MC) is untouched by a PO reset. */
    seed_channel(NAC97_MC, 0x12345678u, 0x03, 0x05, 0x00AA, 0x00CD, 0x09, 0x0D);
    /* Re-seed PO's neighbour state AFTER the PO reset so the comparison reads
     * what the MC block holds while PO is in its reset state. */
    g8_store(NAC97_PO + 0x0B, 0x1D); /* leave PO non-quiescent; MC must not move */
    next = snap_channel(NAC97_MC);
    CHECK(next.bdbar == 0x12345678u, "neighbour BDBAR untouched: expected 0x12345678, got 0x%08X", next.bdbar);
    CHECK(next.cr == 0x0D, "neighbour CR untouched: expected 0x0D, got 0x%02X", next.cr);
    CHECK(next.sr == 0x00AAu, "neighbour SR untouched: expected 0x00AA, got 0x%04X", next.sr);
}

/* Reserved CR-shaped bytes 0x3B/0x4B/0x5B/0x6B stored with bit1 set read back
 * unchanged (plain storage, NEVER masked). Same address-form discipline as
 * snap_channel: the per-byte address is materialised once (LEA) and
 * dereferenced with a zero displacement, so no fault uses [reg+reg]. */
static void case_reserved_bytes_plain(void)
{
    static const uint32_t kOff[4] = { 0x13Bu, 0x14Bu, 0x15Bu, 0x16Bu };
    int i;
    current_case = "reserved-bytes-plain";
    for (i = 0; i < 4; i++) {
        volatile uint8_t *p = (volatile uint8_t *)(g_page + kOff[i]); /* LEA */
        uint8_t got;
        *p = 0x02;
        MemoryBarrier();
        got = *p;
        MemoryBarrier();
        CHECK(got == 0x02, "reserved byte %03X: expected 0x02, got 0x%02X", kOff[i], got);
        *p = 0x1E;
        MemoryBarrier();
        got = *p;
        MemoryBarrier();
        CHECK(got == 0x1E, "reserved byte %03X: expected 0x1E, got 0x%02X", kOff[i], got);
    }
}

/* GLOB_CNT plain storage, GLOB_STA storage + L20, CAS and a NAM byte.
 * THE RESTORED CONTRACT (this packet removed the old dword GLOB_STA W1C/RO
 * policy and its AC97M_GS_WCLEAR / AC97M_GS_RO / AC97M_GS_VALID constants, and
 * they must stay gone): GLOB_CNT stores as written (no VALID_MASK); every
 * GLOB_STA bit round-trips as written EXCEPT bit 8, which is always the level
 * of GLOB_CNT bit 1 (re-derived on any overlapping write and by the tick).
 * These rows kill a re-added W1C/RO policy: they assert the values the policy
 * would destroy (0x12345678 echoes, 0x0000FFFF reads 0x0000FEFF, 0xFFFFFFFF
 * reads 0xFFFFFFFF with GC bit1 set). Every probe is a REAL faulting guest
 * store/load at 0xFEC0012C/0xFEC00130. */
static void case_globals_and_nam_kept(void)
{
    uint8_t gc, nam;
    uint32_t gs, cas, dword48, dword08;
    current_case = "globals-and-nam-kept";
    /* GLOB_CNT plain storage: dword store 0x12345678 reads back unchanged. */
    g32_store(NAC97_GC, 0x12345678u);
    gs = g32_load(NAC97_GC);
    CHECK(gs == 0x12345678u, "GLOB_CNT plain storage: expected 0x12345678, got 0x%08X", gs);
    gc = g8_load(NAC97_GC);
    CHECK(gc == 0x78u, "GLOB_CNT byte0 kept: expected 0x78, got 0x%02X", gc);

    /* GLOB_STA storage + L20, GC bit1 CLEAR: dword store 0x0000FFFF must read
     * 0x0000FEFF -- every bit round-trips except bit 8 (held clear by GC bit1
     * clear). NOT 0x0000 (a re-added W1C policy would clear W1C+storable bits)
     * and NOT 0x0100 (bit 8 is derived, not stored). */
    g8_store(NAC97_GC, (uint8_t)(g8_load(NAC97_GC) & ~0x02u)); /* GC bit1 clear */
    g32_store(NAC97_GS, 0x0000FFFFu);
    gs = g32_load(NAC97_GS);
    CHECK(gs == 0x0000FEFFu, "GLOB_STA 0xFFFF with GC bit1 clear: expected 0x0000FEFF, got 0x%08X", gs);

    /* GLOB_STA storage + L20, GC bit1 SET: dword store 0xFFFFFFFF must read
     * 0xFFFFFFFF (bit 8 derived set, every other bit stored). */
    g8_store(NAC97_GC, (uint8_t)(g8_load(NAC97_GC) | 0x02u)); /* GC bit1 set */
    g32_store(NAC97_GS, 0xFFFFFFFFu);
    gs = g32_load(NAC97_GS);
    CHECK(gs == 0xFFFFFFFFu, "GLOB_STA 0xFFFFFFFF with GC bit1 set: expected 0xFFFFFFFF, got 0x%08X", gs);
    /* Restore GC bit1 clear for the rows below. */
    g8_store(NAC97_GC, (uint8_t)(g8_load(NAC97_GC) & ~0x02u));

    /* GC byte store 0x02 at 0x12C then a load of GLOB_STA -> bit 8 set
     * (write-path sync); GC byte store 0x00 -> bit 8 clear. */
    g32_store(NAC97_GS, 0x00000000u);
    g8_store(NAC97_GC, 0x02);
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) != 0, "GC 0x02 write-path sync: GS bit8 set, GS=0x%08X", gs);
    g8_store(NAC97_GC, 0x00);
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) == 0, "GC 0x00 write-path sync: GS bit8 clear, GS=0x%08X", gs);

    /* Partial / overlapping GS writes, GC bit1 SET: byte store 0x00 to 0x131
     * (GS byte 1, bit 8 itself) must NOT clear the derived bit. */
    g8_store(NAC97_GC, 0x02); /* GC bit1 set */
    g32_store(NAC97_GS, 0x00000000u); /* syncs to 0x00000100 */
    g8_store(NAC97_GS + 1u, 0x00); /* writes GS bits 8-15 as 0x00 */
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) != 0, "GS byte1 0x00 with GC bit1 set: bit8 still 1, GS=0x%08X", gs);

    /* Partial / overlapping GS writes, GC bit1 CLEAR: byte store 0xFF to 0x131
     * must read 0x0000FE00 (bits 9-15 stored as written, derived bit 8 wins
     * and stays clear). */
    g8_store(NAC97_GC, 0x00); /* GC bit1 clear */
    g32_store(NAC97_GS, 0x00000000u);
    g8_store(NAC97_GS + 1u, 0xFF); /* writes GS bits 8-15 as 0xFF */
    gs = g32_load(NAC97_GS);
    CHECK(gs == 0x0000FE00u, "GS byte1 0xFF with GC bit1 clear: expected 0x0000FE00, got 0x%08X", gs);

    /* Overlapping dword store at 0x12E (GC bytes 2-3 + GS bytes 0-1) with GC
     * bit1 CLEAR: every stored byte round-trips, derived bit 8 still wins. */
    xbox_Ac97ModelFixtureSeed(NAC97_GC, 0x00000000u, 4);
    xbox_Ac97ModelFixtureSeed(NAC97_GS, 0x00000000u, 4);
    xbox_Ac97ModelFixtureTick();
    g32_store(0x12Eu, 0xFFFFFFFFu);
    gs = g32_load(NAC97_GS);
    CHECK(gs == 0x0000FEFFu, "dword at 0x12E with GC bit1 clear: expected 0x0000FEFF, got 0x%08X", gs);

    /* CAS keeps its value. */
    g32_store(NAC97_CAS, 0xA5A5A5A5u);
    cas = g32_load(NAC97_CAS);
    CHECK(cas == 0xA5A5A5A5u, "CAS kept: expected 0xA5A5A5A5, got 0x%08X", cas);

    /* A NAM byte (page 0x000) keeps its value. */
    g8_store(NAC97_NAM0, 0x5A);
    nam = g8_load(NAC97_NAM0);
    CHECK(nam == 0x5A, "NAM byte kept: expected 0x5A, got 0x%02X", nam);

    /* Composed dword reads: CIV/LVI/SR (block+0x04) and PICB/PIV/CR (+0x08).
     * LVI is seeded raw (0x22): the 5-bit mask applies to model WRITES, while
     * this row checks that the dword read composes the stored bytes. */
    seed_channel(NAC97_PI, 0x11223344u, 0x21, 0x22, 0x3344, 0x5566, 0x77, 0x09);
    dword48 = g32_load(NAC97_PI + 0x04);
    CHECK(dword48 == 0x33442221u, "CIV/LVI/SR dword: expected 0x33442221, got 0x%08X", dword48);
    dword08 = g32_load(NAC97_PI + 0x08);
    CHECK(dword08 == 0x09775566u, "PICB/PIV/CR dword: expected 0x09775566, got 0x%08X", dword08);
}

/* ── L20 / tick coherence ────────────────────────────────────────────────────
 * Write GLOB_CNT bit1 through a real guest store, then a real guest load of
 * GLOB_STA must already have bit8 set (the write path), and the fixture tick
 * must keep it set; clearing GC bit1 must clear GS bit8 after the tick.
 * No constant codec-ready bit: with GC bit1 clear, GS bit8 reads clear. */
static void case_l20_tick_coherence(void)
{
    uint32_t gs;
    current_case = "l20-tick-coherence";
    xbox_Ac97ModelFixtureSeed(NAC97_GC, 0x00u, 1);
    xbox_Ac97ModelFixtureSeed(NAC97_GS, 0x00000000u, 4);
    xbox_Ac97ModelFixtureTick();
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) == 0, "no constant ready bit: GS=0x%08X with GC bit1 clear", gs);

    g8_store(NAC97_GC, 0x02); /* set GC bit1 through the REAL guest path */
    gs = g32_load(NAC97_GS);  /* the write path must already have set GS bit8 */
    CHECK((gs & 0x00000100u) != 0, "GS bit8 set on the GC write path: GS=0x%08X", gs);

    xbox_Ac97ModelFixtureTick(); /* the tick keeps it set */
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) != 0, "GS bit8 kept by the tick: GS=0x%08X", gs);

    g8_store(NAC97_GC, 0x00); /* clear GC bit1 through the REAL guest path */
    xbox_Ac97ModelFixtureTick();
    gs = g32_load(NAC97_GS);
    CHECK((gs & 0x00000100u) == 0, "GS bit8 cleared after GC clear + tick: GS=0x%08X", gs);
}

/* ── tick re-derives L20 from private state (labelled private-state rows) ────
 * This state is NOT constructible through the public write path (every guest
 * GC/GS write re-syncs bit 8 immediately), so the seam sets it up -- same
 * structure as case_unit_prior_1f_reset: seam setup, then a REAL guest load
 * decides. The expectations are the L20 contract (GS bit8 = level of GC
 * bit1), not model output. A no-op tick passes every write-path row but fails
 * these. */
static void case_tick_derives_set(void)
{
    uint32_t gs;
    current_case = "tick-derives-set";
    /* Labelled private-state row (same structure as unit-prior-1f-reset): the
     * split (GC bit1 SET, GS bit8 CLEAR) is NOT constructible through the
     * public write path -- every guest GC/GS store re-syncs bit 8 -- so the
     * seam sets it up and a REAL guest load decides. Guarded against the ack
     * worker's per-tick sync (which would heal the split if it fired between
     * the fixture tick and the load): the seed+tick+load runs back-to-back 8
     * times and only the last CHECKs, so a no-op tick fails deterministically
     * while a working tick passes. Count stays 89. */
    {
        int i;
        gs = 0;
        for (i = 0; i < 8; i++) {
            xbox_Ac97ModelFixtureSeed(NAC97_GC, 0x02u, 1);
            xbox_Ac97ModelFixtureSeed(NAC97_GS, 0x00000000u, 4);
            xbox_Ac97ModelFixtureTick();
            gs = g32_load(NAC97_GS);
        }
    }
    CHECK((gs & 0x00000100u) != 0, "tick re-derives set: expected bit8 set, got 0x%08X", gs);
}

static void case_tick_derives_clear(void)
{
    uint32_t gs;
    current_case = "tick-derives-clear";
    /* Inverse split (GC bit1 CLEAR, GS bit8 SET): same discipline. */
    {
        int i;
        gs = 0;
        for (i = 0; i < 8; i++) {
            xbox_Ac97ModelFixtureSeed(NAC97_GC, 0x00u, 1);
            xbox_Ac97ModelFixtureSeed(NAC97_GS, 0x00000100u, 4);
            xbox_Ac97ModelFixtureTick();
            gs = g32_load(NAC97_GS);
        }
    }
    CHECK((gs & 0x00000100u) == 0, "tick re-derives clear: expected bit8 clear, got 0x%08X", gs);
}

/* ── deterministic two-thread controls (barrier/gate-driven, NOT stress) ──── */
typedef struct {
    HANDLE go;          /* manual-reset: both threads start together */
    HANDLE done_a, done_b;
    volatile LONG order; /* 0 = unset, 1 = reset-first, 2 = other-first */
    uint8_t cr_seen;
    uint32_t gc_seen, gs_bit8;
} race_rec_t;

static race_rec_t g_race;

/* (a) a reset store and a concurrent CR store must leave the CR byte equal to
 * the result of SOME serial order. The two serial outcomes are enumerated
 * below; a split rd/wr lock produces a value no serial order can produce
 * (e.g. a torn CR that is neither), which this row makes observable. */
static DWORD WINAPI race_reset_thread(LPVOID p)
{
    race_rec_t *r = (race_rec_t *)p;
    WaitForSingleObject(r->go, INFINITE);
    if (InterlockedCompareExchange(&r->order, 1, 0) == 0) {
        /* First: our reset runs before the peer's plain store. */
    }
    g8_store(NAC97_MC + 0x0B, 0x02);
    SetEvent(r->done_a);
    return 0;
}

static DWORD WINAPI race_plain_thread(LPVOID p)
{
    race_rec_t *r = (race_rec_t *)p;
    WaitForSingleObject(r->go, INFINITE);
    if (InterlockedCompareExchange(&r->order, 2, 0) == 0) {
        /* First: our plain store runs before the peer's reset. */
    }
    /* Order control: mutant M5 publishes a rendezvous (named events) around
     * its split; joining it forces the stale interleaving deterministically.
     * Production has no such events, so this falls back to a 20 ms bias there
     * (reset-then-plain) -- deterministic, not stress, in both cases. */
    {
        HANDLE rd = OpenEventA(SYNCHRONIZE, FALSE, "AC97MUT_M5_RD");
        if (rd) {
            HANDLE wr = OpenEventA(EVENT_MODIFY_STATE, FALSE, "AC97MUT_M5_WR");
            if (WaitForSingleObject(rd, 5000) == WAIT_OBJECT_0)
                g8_store(NAC97_MC + 0x0B, 0x19);
            if (wr) { SetEvent(wr); CloseHandle(wr); }
            CloseHandle(rd);
        } else {
            Sleep(20);
            g8_store(NAC97_MC + 0x0B, 0x19);
        }
    }
    SetEvent(r->done_b);
    return 0;
}

static void case_two_thread_cr_serial(void)
{
    HANDLE th[2];
    uint8_t got;
    LONG order;
    current_case = "two-thread-cr-serial";
    /* Prior 0x1D through the public path: reset-first => 0x19 (the plain
     * store wins outright); plain-first => prior 0x19, reset => 0x18. Any
     * other value (e.g. 0x1C from a torn read of the pre-plain prior, or 0x02
     * echoing the stored byte) is reachable by NO serial order. */
    seed_channel(NAC97_MC, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_MC + 0x0B, 0x1D);
    memset(&g_race, 0, sizeof g_race);
    g_race.go = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_race.done_a = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_race.done_b = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_race.go || !g_race.done_a || !g_race.done_b) {
        HARNESS("two-thread-cr-serial: CreateEvent failed (%lu)", GetLastError());
        goto out;
    }
    th[0] = CreateThread(NULL, 0, race_reset_thread, &g_race, 0, NULL);
    th[1] = CreateThread(NULL, 0, race_plain_thread, &g_race, 0, NULL);
    if (!th[0] || !th[1]) {
        HARNESS("two-thread-cr-serial: CreateThread failed (%lu)", GetLastError());
        goto out;
    }
    SetEvent(g_race.go); /* release both threads at once */
    WaitForSingleObject(g_race.done_a, INFINITE);
    WaitForSingleObject(g_race.done_b, INFINITE);
    WaitForMultipleObjects(2, th, TRUE, INFINITE);
    CloseHandle(th[0]);
    CloseHandle(th[1]);
    order = g_race.order;
    got = g8_load(NAC97_MC + 0x0B); /* REAL guest load of the raced result */
    g_race.cr_seen = got;
    CHECK(got == 0x19 || got == 0x18,
          "raced CR is a serial outcome (order=%ld): got 0x%02X, want 0x19 or 0x18",
          order, got);
    fprintf(stderr, "INFO [two-thread-cr-serial]: observed transaction order=%ld cr=0x%02X\n",
            order, got);
out:
    if (g_race.go) CloseHandle(g_race.go);
    if (g_race.done_a) CloseHandle(g_race.done_a);
    if (g_race.done_b) CloseHandle(g_race.done_b);
}

static DWORD WINAPI race_gc_thread(LPVOID p)
{
    race_rec_t *r = (race_rec_t *)p;
    WaitForSingleObject(r->go, INFINITE);
    g8_store(NAC97_GC, 0x02);
    SetEvent(r->done_a);
    return 0;
}

static DWORD WINAPI race_tick_thread(LPVOID p)
{
    race_rec_t *r = (race_rec_t *)p;
    WaitForSingleObject(r->go, INFINITE);
    xbox_Ac97ModelFixtureTick();
    SetEvent(r->done_b);
    return 0;
}

/* (b) the guest GC write concurrent with the fixture tick must lose neither
 * the guest's GC value nor the GS relation. */
static void case_two_thread_gc_tick(void)
{
    HANDLE th[2];
    uint8_t gc;
    uint32_t gs;
    current_case = "two-thread-gc-tick";
    xbox_Ac97ModelFixtureSeed(NAC97_GC, 0x00u, 1);
    xbox_Ac97ModelFixtureSeed(NAC97_GS, 0x00000000u, 4);
    memset(&g_race, 0, sizeof g_race);
    g_race.go = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_race.done_a = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_race.done_b = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_race.go || !g_race.done_a || !g_race.done_b) {
        HARNESS("two-thread-gc-tick: CreateEvent failed (%lu)", GetLastError());
        goto out;
    }
    th[0] = CreateThread(NULL, 0, race_gc_thread, &g_race, 0, NULL);
    th[1] = CreateThread(NULL, 0, race_tick_thread, &g_race, 0, NULL);
    if (!th[0] || !th[1]) {
        HARNESS("two-thread-gc-tick: CreateThread failed (%lu)", GetLastError());
        goto out;
    }
    SetEvent(g_race.go);
    WaitForSingleObject(g_race.done_a, INFINITE);
    WaitForSingleObject(g_race.done_b, INFINITE);
    WaitForMultipleObjects(2, th, TRUE, INFINITE);
    CloseHandle(th[0]);
    CloseHandle(th[1]);
    xbox_Ac97ModelFixtureTick(); /* settle: one deterministic transaction */
    gc = g8_load(NAC97_GC);
    gs = g32_load(NAC97_GS);
    g_race.gc_seen = gc;
    g_race.gs_bit8 = (gs >> 8) & 1u;
    CHECK(gc == 0x02, "raced GC value kept: expected 0x02, got 0x%02X", gc);
    CHECK((gs & 0x00000100u) != 0, "raced GS relation kept: GS=0x%08X", gs);
    fprintf(stderr, "INFO [two-thread-gc-tick]: observed gc=0x%02X gs=0x%08X\n", gc, gs);
out:
    if (g_race.go) CloseHandle(g_race.go);
    if (g_race.done_a) CloseHandle(g_race.done_a);
    if (g_race.done_b) CloseHandle(g_race.done_b);
}

/* ── refusal coverage (unit coverage with crafted contexts, said so) ─────────
 * An undecodable instruction form and an operand that starts outside the page
 * must be refused with Rip unadvanced and no state change.
 *
 * COVERAGE NOTE (bring-up finding, production since fixed): the first version
 * of snap_channel emitted a SIB+[reg+reg] dword read with an extended base
 * register, which the then-production decoder mis-decoded (SIB base dropped
 * REX.B at the old mmio_decode.h:337). The decoder is FIXED (`int base =
 * (s & 7) | (rex_b ? 8 : 0);`), and every probe in this gate still uses
 * [reg+disp] forms only as a deliberate documented subset (see the
 * ADDRESS-FORM DISCIPLINE note above). The rows below therefore drive crafted
 * CONTEXTs for the refusal paths: that is legitimate unit coverage, labelled
 * as such, not a production-path proof.
 *
 * The production VEH has no exported entry for delivering a crafted context,
 * but its two refusal conditions are (a) mmio_decode fails and (b) the decoded
 * effective start is outside [page, page+0x1000). Both are exercised here by
 * decoding crafted bytes in an executable scratch buffer with a hand-built
 * CONTEXT whose Rip points at them and whose base register holds the live page
 * address: (b) is the exact range comparison the VEH performs, evaluated
 * against the live protected page. Rip must be unadvanced and model state
 * unchanged in both rows. */
#if defined(_WIN32)
#include "../platform/mmio_decode.h"
#else
#error "this gate exercises the Windows NABM owner only"
#endif
/* The gate binary links the toolkit from XBOXRECOMP_DIR; the harness build
 * aliases the page end for the safety-net window below. */
#ifndef AC97_PAGE_SPAN_END
#define AC97_PAGE_SPAN_END 0x1000u
#endif
static uint8_t gate_scratch[64];
static DWORD gate_scratch_old;
static void gate_scratch_exec(void)
{
    if (!VirtualProtect(gate_scratch, sizeof gate_scratch, PAGE_EXECUTE_READWRITE,
                        &gate_scratch_old))
        gate_scratch_old = 0;
}
static void gate_scratch_restore(void)
{
    DWORD tmp;
    if (gate_scratch_old)
        VirtualProtect(gate_scratch, sizeof gate_scratch, PAGE_EXECUTE_READWRITE, &tmp);
}
static void case_refusal_undecodable_form(void)
{
    CONTEXT ctx;
    mmio_insn_t d;
    uint32_t before;
    current_case = "refusal-undecodable-form";
    /* FF /7 op r/m64: DEC -- not in the decoder's covered set. */
    gate_scratch[0] = 0xFF; gate_scratch[1] = 0x38; /* DEC [rax] */
    gate_scratch_exec();
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    ctx.Rip = (DWORD64)(uintptr_t)gate_scratch;
    ctx.Rax = (DWORD64)(uintptr_t)g_page;
    before = xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1);
    CHECK(!mmio_decode(&ctx, &d),
          "undecodable form must not decode (DEC [rax] at scratch)");
    CHECK(ctx.Rip == (DWORD64)(uintptr_t)gate_scratch,
          "decode must not advance Rip");
    CHECK(xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1) == before,
          "decode must not mutate model state");
    gate_scratch_restore();
}
static void case_refusal_out_of_page_operand(void)
{
    CONTEXT ctx;
    mmio_insn_t d;
    uint64_t base, lo, hi;
    uint32_t before;
    current_case = "refusal-out-of-page-operand";
    /* C6 80 F8 0F 00 00 05: MOV byte ptr [rax+0xFF8],5 -- decodable, but the
     * operand starts 8 bytes before the page end with size 1... in range, so
     * instead aim one byte PAST the end: disp 0x1000. */
    gate_scratch[0] = 0xC6; gate_scratch[1] = 0x80;
    gate_scratch[2] = 0x00; gate_scratch[3] = 0x10;
    gate_scratch[4] = 0x00; gate_scratch[5] = 0x00;
    gate_scratch[6] = 0x05;
    gate_scratch_exec();
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    ctx.Rip = (DWORD64)(uintptr_t)gate_scratch;
    ctx.Rax = (DWORD64)(uintptr_t)g_page;
    CHECK(mmio_decode(&ctx, &d) != 0, "crafted in-range-shaped store must decode");
    base = (uint64_t)(uintptr_t)g_page;
    lo = d.eff;
    hi = d.eff + (uint64_t)d.size;
    before = xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1);
    CHECK(lo == base + 0x1000u, "crafted operand starts one past the page end: eff=0x%llX",
          (unsigned long long)lo);
    CHECK(!(lo >= base && hi <= base + 0x1000u),
          "VEH range check must refuse an operand starting outside the page");
    CHECK(ctx.Rip == (DWORD64)(uintptr_t)gate_scratch,
          "refused decode must not advance Rip");
    CHECK(xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1) == before,
          "refused operand must not mutate model state");
    gate_scratch_restore();
}
static void case_refusal_counters_pinned(void)
{
    uint32_t s0, r0, s1, r1;
    current_case = "refusal-counters-pinned";
    s0 = xbox_Ac97ModelFixtureServiced();
    r0 = xbox_Ac97ModelFixtureRefused();
    xbox_Ac97ModelFixtureTick(); /* no fault: neither counter may move */
    s1 = xbox_Ac97ModelFixtureServiced();
    r1 = xbox_Ac97ModelFixtureRefused();
    CHECK(s1 == s0, "tick advances no service count: %u -> %u", s0, s1);
    CHECK(r1 == r0, "tick advances no refusal count: %u -> %u", r0, r1);
}

/* Crafted-context unit coverage (labelled): the SIB+[extended base] shape that
 * was mis-decoded before the decoder fix. A base+index operand with an
 * extended base register (here R8) plus an index must decode to base+index.
 * No real compiled probe in this gate is forced to emit that shape, so this
 * row drives it with bytes: `41 8A 04 08` = MOV al, byte ptr [r8+rcx]
 * (REX.B only, ModRM mod=00 reg=000 rm=100, SIB scale=0 index=001 base=000
 * -> base R8, index RCX). eff must be R8+RCX. */
static void case_sib_extended_base_decode(void)
{
    CONTEXT ctx;
    mmio_insn_t d;
    current_case = "sib-extended-base-decode";
    gate_scratch[0] = 0x41; gate_scratch[1] = 0x8A; /* REX.B; MOV r/m8 */
    gate_scratch[2] = 0x04; gate_scratch[3] = 0x08; /* ModRM [SIB]; SIB base=R8 idx=RCX */
    gate_scratch_exec();
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    ctx.Rip = (DWORD64)(uintptr_t)gate_scratch;
    ctx.R8 = (DWORD64)(uintptr_t)g_page;
    ctx.Rcx = (DWORD64)0x11B;
    CHECK(mmio_decode(&ctx, &d) != 0, "SIB extended-base form must decode");
    CHECK(d.eff == (uint64_t)(uintptr_t)g_page + 0x11Bu,
          "SIB eff must be base+index: eff=0x%llX want 0x%llX",
          (unsigned long long)d.eff,
          (unsigned long long)((uint64_t)(uintptr_t)g_page + 0x11Bu));
    CHECK(d.size == 1, "SIB form width must be 1, got %d", d.size);
    CHECK(ctx.Rip == (DWORD64)(uintptr_t)gate_scratch,
          "decode must not advance Rip");
    gate_scratch_restore();
}

#define NAC97_SPAN 0xFFEu
static volatile LONG gate_swallowed = 0;

/* Safety net for the spanning probe ONLY (transparent otherwise). Registered
 * BEFORE xbox_MemoryLayoutInit, so the production VEH (registered during init)
 * runs FIRST: serviced faults never reach this handler. A fault the production
 * owner refuses (EXCEPTION_CONTINUE_SEARCH) falls through to here; if it is
 * the spanning probe's read, the instruction is decoded for its length, Rip is
 * advanced past it and execution resumes -- letting the gate OBSERVE the
 * refusal (refused counter +1, no state change) instead of dying on it. */
static LONG CALLBACK gate_safety_veh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
            && ep->ExceptionRecord->NumberParameters >= 2
            && ep->ExceptionRecord->ExceptionInformation[0] == 0
            && g_page) {
        uint64_t fault = (uint64_t)ep->ExceptionRecord->ExceptionInformation[1];
        uint64_t base = (uint64_t)(uintptr_t)g_page;
        if (fault >= base + NAC97_SPAN && fault < base + AC97_PAGE_SPAN_END) {
            mmio_insn_t d;
            if (mmio_decode(ep->ContextRecord, &d)) {
                ep->ContextRecord->Rip += (DWORD64)d.rip_delta;
                InterlockedIncrement(&gate_swallowed);
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* A REAL faulting access the production owner must REFUSE: a dword load at
 * page+0xFFE whose operand spans past the page end (lo in range, hi out).
 * Production refuses (refused +1, serviced +0, state unchanged); mutant M9
 * (range check removed) services it instead, which this row catches as
 * refused +0 / serviced +1. The safety VEH above advances Rip so the refusal
 * is observable rather than fatal. */
static void case_refusal_spanning_access(void)
{
    uint32_t s0, r0, w0, s1, r1, w1, before;
    current_case = "refusal-spanning-access";
    s0 = xbox_Ac97ModelFixtureServiced();
    r0 = xbox_Ac97ModelFixtureRefused();
    w0 = (uint32_t)gate_swallowed;
    before = xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1);
    {
        volatile uint32_t *p = (volatile uint32_t *)(g_page + NAC97_SPAN);
        (void)*p; /* faults: operand [page+0xFFE, page+0x1002) spans past the end */
        MemoryBarrier();
    }
    s1 = xbox_Ac97ModelFixtureServiced();
    r1 = xbox_Ac97ModelFixtureRefused();
    w1 = (uint32_t)gate_swallowed;
    CHECK(r1 == r0 + 1, "spanning access must be refused: refused %u -> %u", r0, r1);
    CHECK(s1 == s0, "spanning access must not be serviced: serviced %u -> %u", s0, s1);
    CHECK(w1 == w0 + 1, "safety net must have advanced the refused instruction");
    CHECK(xbox_Ac97ModelFixturePeek(NAC97_PO + 0x0B, 1) == before,
          "refused spanning access must not mutate model state");
}

/* A real faulting access IS serviced (the positive production-path proof that
 * refusal coverage complements): the serviced counter advances and the value
 * path works. */
static void case_serviced_counter_advances(void)
{
    uint32_t s0, s1;
    uint8_t got;
    current_case = "serviced-counter-advances";
    s0 = xbox_Ac97ModelFixtureServiced();
    seed_channel(NAC97_PO, 0, 0, 0, 0x0001, 0, 0, 0x00);
    g8_store(NAC97_PO + 0x0B, 0x19);
    got = g8_load(NAC97_PO + 0x0B);
    s1 = xbox_Ac97ModelFixtureServiced();
    CHECK(got == 0x19, "serviced-path value: expected 0x19, got 0x%02X", got);
    CHECK(s1 > s0, "serviced counter advanced: %u -> %u", s0, s1);
}

/* ── seam misuse / rejected-request coverage ──────────────────────────────────
 * The seam is fail-safe when the model is not armed (Seed/Tick no-op, Peek 0)
 * and validates armed requests (size in {1,2,4}, overflow-safe in-page range).
 * Rejected armed requests are no-ops and must not disturb in-range state. */
static void case_seam_misuse_unarmed(const char *when)
{
    uint32_t peek_before, peek_after;
    current_case = "seam-misuse-unarmed";
    /* While unarmed the model reports disarmed. */
    CHECK(xbox_Ac97ModelFixtureArmed() == 0,
          "seam %s: Armed() must be 0 while unarmed", when);
    /* Seed-before-arm is a no-op (must not crash); Peek is 0; Tick is a no-op. */
    xbox_Ac97ModelFixtureSeed(0x100u, 0xFFu, 4);
    peek_before = xbox_Ac97ModelFixturePeek(0x100u, 4);
    CHECK(peek_before == 0u,
          "seam %s: Peek(0x100,4) while unarmed must be 0, got 0x%08X", when, peek_before);
    xbox_Ac97ModelFixtureTick(); /* must not crash */
    peek_after = xbox_Ac97ModelFixturePeek(0x100u, 4);
    CHECK(peek_after == 0u,
          "seam %s: Peek(0x100,4) after unarmed Tick must be 0, got 0x%08X", when, peek_after);
}

static void case_seam_rejected_while_armed(void)
{
    uint32_t keep_before, keep_after, bad;
    current_case = "seam-rejected-while-armed";
    /* An in-range peek to compare before/after the rejected requests. */
    xbox_Ac97ModelFixtureSeed(0x100u, 0x11223344u, 4);
    keep_before = xbox_Ac97ModelFixturePeek(0x100u, 4);
    CHECK(keep_before == 0x11223344u,
          "seam armed setup: expected 0x11223344, got 0x%08X", keep_before);
    /* Rejected Seeds: out-of-range page offset, page-end offset, bad sizes. */
    xbox_Ac97ModelFixtureSeed(0xFFFFFFFFu, 0xA5A5A5A5u, 4);
    xbox_Ac97ModelFixtureSeed(0xFFEu, 0xA5A5A5A5u, 4);
    xbox_Ac97ModelFixtureSeed(0x100u, 0xA5A5A5A5u, 5);
    xbox_Ac97ModelFixtureSeed(0x100u, 0xA5A5A5A5u, 0);
    keep_after = xbox_Ac97ModelFixturePeek(0x100u, 4);
    CHECK(keep_after == keep_before,
          "seam rejected Seeds must not disturb state: 0x%08X -> 0x%08X",
          keep_before, keep_after);
    /* Rejected Peek: out-of-range offset returns 0 and disturbs nothing. */
    bad = xbox_Ac97ModelFixturePeek(0xFFFFFFFFu, 4);
    CHECK(bad == 0u, "seam Peek(0xFFFFFFFF,4) must be 0, got 0x%08X", bad);
    CHECK(xbox_Ac97ModelFixturePeek(0x100u, 4) == keep_before,
          "seam rejected Peek must not disturb state");
}

/* ── XBE loading ───────────────────────────────────────────────────────────── */
static void *load_xbe(const char *path, size_t *out_size)
{
    FILE *f = NULL;
    long n = 0;
    void *buf = NULL;
    size_t got = 0;
    if (fopen_s(&f, path, "rb") != 0 || !f) {
        HARNESS("cannot open XBE %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        HARNESS("XBE %s has no bytes", path);
        fclose(f);
        return NULL;
    }
    buf = malloc((size_t)n);
    if (!buf) {
        HARNESS("out of memory reading XBE");
        fclose(f);
        return NULL;
    }
    got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) {
        HARNESS("short read on XBE %s", path);
        free(buf);
        return NULL;
    }
    *out_size = got;
    return buf;
}

int main(int argc, char **argv)
{
    void *xbe = NULL;
    size_t xbe_size = 0;
    const char *xbe_path;
    MEMORY_BASIC_INFORMATION mbi;

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    if (argc < 2) {
        HARNESS("usage: %s <path-to-xbe>", argv[0]);
        return 2;
    }
    xbe_path = argv[1];
    xbe = load_xbe(xbe_path, &xbe_size);
    if (!xbe)
        return 2;
    fprintf(stderr, "  [ac97-gate] image %zu bytes; initialising the layout\n", xbe_size);
    /* Seed-before-arm misuse (fail-safe, decided before init): Armed() is 0,
     * Seed is a no-op, Peek is 0, Tick is a no-op. */
    case_seam_misuse_unarmed("pre-arm");
    /* Safety net BEFORE init: production VEH (registered during init) runs
     * first, so this handler only ever sees what production refused. */
    AddVectoredExceptionHandler(1, gate_safety_veh);
    if (!xbox_MemoryLayoutInit(xbe, xbe_size)) {
        HARNESS("memory layout init failed");
        free(xbe);
        return 2;
    }
    free(xbe);

    /* HOST address of the guest page: the MCPX aperture is placed at
     * XBOX_MCPX_BASE + g_memory_offset (VirtualAlloc with the exact address
     * hint), and the NABM page is AC97_NABM_OFFSET into it -- the same
     * arithmetic ac97m_arm uses. Verified below with VirtualQuery (NOACCESS)
     * rather than assumed. */
    g_page = (char *)((uintptr_t)NAC97_PAGE_GUEST + (uintptr_t)xbox_GetMemoryOffset());
    fprintf(stderr, "  [ac97-gate] host page=%p guest=0x%08X offset=%td\n",
            (void *)g_page, NAC97_PAGE_GUEST, xbox_GetMemoryOffset());
    if (!VirtualQuery(g_page, &mbi, sizeof mbi)) {
        HARNESS("VirtualQuery on the NABM page failed (%lu)", GetLastError());
        return 2;
    }
    /* CHECK, not HARNESS: an unprotected page is itself a named gate failure
     * (mutant M6 leaves the page writable), and the run continues so the
     * plain-RAM echoes are also observed as value failures. */
    current_case = "page-protected";
    CHECK((mbi.Protect & PAGE_NOACCESS) != 0,
          "NABM page not protected: Protect=0x%lX (expected PAGE_NOACCESS)",
          (unsigned long)mbi.Protect);
    if (!xbox_Ac97ModelFixtureArmed()) {
        HARNESS("model not armed after layout init");
        return 2;
    }
    fprintf(stderr, "  [ac97-gate] page protected at guest 0x%08X, model armed; running cases\n",
            NAC97_PAGE_GUEST);

    /* Rejected seam requests while armed: no-ops that disturb nothing. */
    case_seam_rejected_while_armed();
    /* Deciding rows first: each must FAIL on the pre-repair source for the
     * semantic reason (VALUE quoted), and PASS on the final source. */
    case_deciding_po_reset_fresh();
    case_deciding_po_reset_seeded_1c();
    case_deciding_po_reset_seeded_1d();
    case_deciding_po_reset_fresh_1e();
    case_deciding_so_reset();
    /* Unaffected positive: passes on BOTH sources. */
    case_positive_nonrr_write();
    /* Labelled private unit-policy coverage (seam-seeded prior). */
    case_unit_prior_1f_reset();
    /* SR / LVI / BDBAR deciding rows (pinned xemu expectations). */
    case_sr_readonly_w1c();
    case_lvi_mask();
    case_bdbar_mask();
    /* Complete observable state + neighbours + composed reads. */
    case_full_reset_state();
    case_reserved_bytes_plain();
    case_globals_and_nam_kept();
    /* L20 / tick coherence. */
    case_l20_tick_coherence();
    /* Tick re-derivation from private state (labelled; seam setup, real loads). */
    case_tick_derives_set();
    case_tick_derives_clear();
    /* Deterministic two-thread controls. */
    case_two_thread_cr_serial();
    case_two_thread_gc_tick();
    /* Refusal / serviced counter coverage (crafted-context rows are unit
     * coverage, labelled as such; the spanning row is a real faulting read). */
    case_refusal_spanning_access();
    case_refusal_undecodable_form();
    case_refusal_out_of_page_operand();
    case_refusal_counters_pinned();
    case_sib_extended_base_decode();
    case_serviced_counter_advances();

    xbox_MemoryLayoutShutdown();
    /* Post-shutdown misuse: the seam is fail-safe again (fresh checkpoint). */
    case_seam_misuse_unarmed("post-shutdown");

    printf("AC97-RESET-GATE checks=%u failed=%u harness_errors=%u skips=%u\n",
           checks_run, checks_failed, harness_errors, skips);
    if (harness_errors)
        return 2;
    return checks_failed ? 1 : 0;
}
