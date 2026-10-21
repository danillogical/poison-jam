/* Independent deciding gate for the D4 executor coverage diagnostics
 * (RECOMP_PB_COVERAGE=1), committed-stream half.
 *
 * This file was authored by a worker that did not write the production
 * coverage code and must not change it. Every expectation below is derived
 * from `src/kernel/nv2a_backend.h` (the ledger contract), `src/nv2a/nv2a_regs.h`
 * (the owned register numbers), `src/nv2a/nv2a_method_table.c` (what the walk
 * admits) and the executor source under test -- not from any helper body and
 * not from the historical body probes.
 *
 * It drives the REAL path the title uses: guest RAM, a synthetic section-less
 * XBE for the memory layout, `nv2a_init_standalone`, the pushbuffer window,
 * the instance/RAMHT binding, `RECOMP_PB_EXEC=1`, the commit-consumer
 * registration, and real pushbuffer words through `nv2a_submit_pending`. The
 * counters it reads are `nv2a_pb_exec_coverage()` / the ledger's own fields.
 *
 * One case per process (`--case <name>`). The coverage gate is read once per
 * process, so CTest registers a separate process for each environment variant.
 * Unknown cases fail. Each failure names the assertion on stderr.
 *
 * Deliberately NOT asserted, because it is not observable in one process:
 * the ordering of `payload then valid` in a record under concurrent readers.
 * The observable equivalent is asserted instead: a claimed record's bytes are
 * identical before and after later batches, later method writes and repeated
 * reports.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_vsh_interp.h"
#include "recomp_types.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>

#if !defined(_WIN32)
#error "this fixture follows the repository's Windows-only test harness"
#endif

/* ── Toolkits symbols the focused fixture must satisfy ──────────────────────── */

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

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);
extern void xbox_MemoryLayoutShutdown(void);
extern void nv2a_pb_exec_report(void);

/* The executor's window/present seam, inert here (no window is opened). */
void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowPresent(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowStart(void) {}
void xbox_FramebufferWindowSetTitle(const uint16_t *name, int max_chars)
{ (void)name; (void)max_chars; }
void xbox_FramebufferWindowFrameStats(uint32_t draws) { (void)draws; }
uint32_t xbox_FramebufferPresentSerial(void) { return 0; }
unsigned long long xbox_FramebufferPresentHash(void) { return 0; }
int xbox_FramebufferDumpBmp(const char *path) { (void)path; return 0; }
void xbox_FramebufferWindowSetAA(uint32_t sy) { (void)sy; }

/* ── Assertions ─────────────────────────────────────────────────────────────── */

static int g_failures;
static const char *g_case = "?";

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL pbcov_stream[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

/* A two-argument call gets the empty format; a formatted call's literal is
 * concatenated with that same empty literal, so both shapes are one function. */
#define chk(cond, what, ...) chk_impl((cond), (what), "" __VA_ARGS__)

static void chk_u64(const char *what, uint64_t got, uint64_t want)
{
    chk(got == want, what, "got %llu, want %llu",
        (unsigned long long)got, (unsigned long long)want);
}

static void chk_u32(const char *what, uint32_t got, uint32_t want)
{
    chk(got == want, what, "got 0x%08X, want 0x%08X", (unsigned)got, (unsigned)want);
}

static void chk_bytes(const char *what, const void *got, const void *want, size_t n)
{
    chk(memcmp(got, want, n) == 0, what, "byte content differs (%u bytes)",
        (unsigned)n);
}

/* ── Fixture ────────────────────────────────────────────────────────────────── */

#define RAM_SIZE        (64u * 1024u * 1024u)
#define RAMIN_SIZE      (1024u * 1024u)
#define INSTANCE_SIZE   0x20000u
#define PB_BYTES        0x10000u

#define XBOX_BASE_ADDRESS 0x00010000u
#define SURF_VA         0x00400000u
#define SURF_W          64u
#define SURF_H          64u
#define SURF_PITCH      (SURF_W * 4u)
#define VB_VA           0x00420000u

#define OBJ_HANDLE      6u
#define OBJ_ADDR        0x300u

typedef struct {
    uint8_t *ram;
    uint8_t *ramin;
    uint8_t *instance;
    uint32_t *pb;
    NV2AState *gpu;
    uint32_t pbw;
} Fx;

/* Register numbers, from src/nv2a/nv2a_regs.h. */
#define M_SET_OBJECT            0x0000u
#define M_SET_LIGHTING_ENABLE   0x0314u
#define M_SET_SPECULAR_ENABLE   0x03B8u
#define M_SET_LIGHT_MASK        0x03BCu
#define M_SET_TEXGEN_S          0x03C0u
#define M_SET_TEXGEN_T          0x03C4u
#define M_SET_TEXGEN_R          0x03C8u
#define M_SET_TEXGEN_Q          0x03CCu
#define M_SET_TEXMAT_ENABLE     0x0420u
#define M_SET_TEXMAT            0x06C0u
#define M_SET_COMPOSITE         0x0680u
#define M_SET_VERTEX_FORMAT0    0x1760u
#define M_SET_VERTEX_OFFSET0    0x1720u
#define M_SET_VERTEX_OFFSET3    0x172Cu
#define M_SET_VERTEX_FORMAT3    0x176Cu
#define M_SET_BEGIN_END         0x17FCu
#define M_DRAW_ARRAYS           0x1810u
#define M_ARRAY_ELEMENT16       0x1800u
#define M_INLINE_ARRAY          0x1818u
#define M_SET_VERTEX4F          0x1518u
#define M_SHADER_STAGE_PROGRAM  0x1E70u
#define M_COMBINER_CONTROL      0x1E60u
#define M_TRANSFORM_EXEC_MODE   0x1E94u
#define M_TRANSFORM_PROGRAM     0x0B00u
#define M_TRANSFORM_PROGRAM_LOAD 0x1E9Cu
#define M_TRANSFORM_PROGRAM_START 0x1EA0u
#define M_BUMP_MAT1             0x1B68u   /* stage 1 MAT00 */
#define M_BUMP_SCALE1           0x1B78u
#define M_BUMP_OFFSET1          0x1B7Cu
#define M_SPOT_FALLOFF0         0x1040u   /* NOT admitted */

static uint32_t gpu32(Fx *f, uint32_t va)
{
    uint32_t v;
    memcpy(&v, f->ram + va, 4);
    return v;
}

static void emit_method(Fx *f, uint32_t subch, uint32_t method, uint32_t param)
{
    f->pb[f->pbw++] = (subch << 13) | (1u << 18) | method;
    f->pb[f->pbw++] = param;
}

static void emit(Fx *f, uint32_t method, uint32_t param)
{
    emit_method(f, 0, method, param);
}

static uint32_t emit_begin(Fx *f, uint32_t prim)
{
    uint32_t at = f->pbw;
    emit(f, M_SET_BEGIN_END, prim);
    return at;
}

static void emit_end(Fx *f)
{
    emit(f, M_SET_BEGIN_END, 0);
}

/* Three vertices, indices 0..2, the title's own array path. */
static void emit_batch_arrays(Fx *f)
{
    emit_begin(f, 5u);
    emit(f, M_DRAW_ARRAYS, (2u << 24) | 0u);      /* count field + 1 = 3 */
    emit_end(f);
}

/* Two vertices: below the batch minimum. */
static void emit_short_batch(Fx *f)
{
    emit_begin(f, 5u);
    emit(f, M_DRAW_ARRAYS, (1u << 24) | 0u);      /* two vertices */
    emit_end(f);
}

static int fx_submit(Fx *f)
{
    f->gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    f->gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = f->pbw * 4u;
    return nv2a_submit_pending(f->gpu) ? 1 : 0;
}

static void put_vertex(Fx *f, uint32_t i, float x, float y)
{
    uint8_t *v = f->ram + VB_VA + (size_t)i * 24u;
    float *p = (float *)v;
    uint32_t argb = 0xFF204060u;
    p[0] = x;
    p[1] = y;
    p[2] = 0.5f;
    memcpy(v + 12, &argb, 4u);
}

static int fx_init(Fx *f, const char *gate)
{
    static uint8_t xbe[0x2000];
    uint32_t *pair, *object;

    memset(f, 0, sizeof *f);
    f->ram = (uint8_t *)calloc(1, RAM_SIZE);
    f->ramin = (uint8_t *)calloc(1, RAMIN_SIZE);
    f->instance = (uint8_t *)calloc(1, INSTANCE_SIZE);
    f->pb = (uint32_t *)VirtualAlloc(NULL, PB_BYTES, MEM_RESERVE | MEM_COMMIT,
                                     PAGE_READWRITE);
    if (!f->ram || !f->ramin || !f->instance || !f->pb) {
        fprintf(stderr, "FAIL pbcov_stream[%s]: fixture allocation\n", g_case);
        return 0;
    }

    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) {
        fprintf(stderr, "FAIL pbcov_stream[%s]: xbox_MemoryLayoutInit\n", g_case);
        return 0;
    }

    nv2a_reset_standalone_for_test();
    f->gpu = nv2a_init_standalone(f->ram, RAM_SIZE, f->ramin, RAMIN_SIZE);
    if (!f->gpu
     || !nv2a_set_pushbuffer_window(f->gpu, (uint8_t *)f->pb, 0, PB_BYTES)
     || !nv2a_bind_instance_memory(0x83fe0000u, f->instance, INSTANCE_SIZE)) {
        fprintf(stderr, "FAIL pbcov_stream[%s]: nv2a standalone setup\n", g_case);
        return 0;
    }

    /* RAMHT handle 6 -> object 0x300, class NV097. */
    pair = (uint32_t *)(f->instance + OBJ_HANDLE * 8u);
    object = (uint32_t *)(f->instance + (OBJ_ADDR << 4));
    pair[0] = OBJ_HANDLE;
    pair[1] = NV_RAMHT_STATUS | OBJ_ADDR;
    object[0] = 0x97u;
    object[1] = object[2] = object[3] = 0;

    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) {
        fprintf(stderr, "FAIL pbcov_stream[%s]: executor did not register\n", g_case);
        return 0;
    }

    /* Guest vertices the CPU fallback can read. */
    put_vertex(f, 0, 8.0f, 8.0f);
    put_vertex(f, 1, 56.0f, 12.0f);
    put_vertex(f, 2, 20.0f, 58.0f);

    /* A real committed surface + vertex layout, on subchannel 0. */
    f->pbw = 0;
    emit(f, M_SET_OBJECT, OBJ_HANDLE);
    emit(f, 0x0200u, (uint32_t)SURF_W << 16);
    emit(f, 0x0204u, (uint32_t)SURF_H << 16);
    emit(f, 0x0208u, 0x0108u);                    /* A8R8G8B8, linear */
    emit(f, 0x020Cu, SURF_PITCH);
    emit(f, 0x0210u, SURF_VA);
    emit(f, M_SET_VERTEX_OFFSET0, VB_VA);
    emit(f, M_SET_VERTEX_FORMAT0, 2u | (3u << 4) | (24u << 8));
    emit(f, M_SET_VERTEX_OFFSET3, VB_VA + 12u);
    emit(f, M_SET_VERTEX_FORMAT3, 0u | (4u << 4) | (24u << 8));
    /* composite_set, so batch_is_ffp() is true for ordinary batches. */
    emit(f, M_SET_COMPOSITE, 0x3F800000u);
    (void)gate;
    if (!fx_submit(f)) {
        fprintf(stderr, "FAIL pbcov_stream[%s]: setup stream rejected\n", g_case);
        return 0;
    }
    return 1;
}

static const Nv2aPbCoverageLedger *ledger(void)
{
    return nv2a_pb_exec_coverage();
}

/* ── Capturing one report's stderr ───────────────────────────────────────────
 *
 * The [PBCOV] lines are the diagnostic's only output.  They must exist when
 * the gate is on and must not exist when it is off.  Only this one report is
 * redirected, to a private file in the process's temp directory that is
 * deleted before the case returns; fd 2 is restored from a saved duplicate.
 * The file is per-process (the harness is process-per-case). */
static char g_cap_path[600];
static int g_cap_saved = -1;

static int capture_begin(const char *tag)
{
    const char *tmp = getenv("TEMP");
    int fd;

    if (!tmp || !*tmp)
        tmp = ".";
    _snprintf(g_cap_path, sizeof g_cap_path, "%s\\pbcov-%s-%ld.log", tmp, tag,
              (long)_getpid());
    g_cap_path[sizeof g_cap_path - 1] = '\0';
    remove(g_cap_path);
    fflush(stderr);
    g_cap_saved = _dup(2);
    if (g_cap_saved < 0)
        return 0;
    fd = _open(g_cap_path, _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY,
               _S_IREAD | _S_IWRITE);
    if (fd < 0) {
        _close(g_cap_saved);
        g_cap_saved = -1;
        return 0;
    }
    if (_dup2(fd, 2) != 0) {
        _close(fd);
        _close(g_cap_saved);
        g_cap_saved = -1;
        return 0;
    }
    _close(fd);
    return 1;
}

/* Restore stderr, read the captured text, delete the file. Caller frees. */
static char *capture_end(void)
{
    FILE *f;
    long n;
    char *b = NULL;

    fflush(stderr);
    if (g_cap_saved >= 0) {
        _dup2(g_cap_saved, 2);
        _close(g_cap_saved);
        g_cap_saved = -1;
    }
    f = fopen(g_cap_path, "rb");
    if (f) {
        if (fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) >= 0
         && fseek(f, 0, SEEK_SET) == 0) {
            b = (char *)malloc((size_t)n + 1);
            if (b) {
                size_t got = fread(b, 1, (size_t)n, f);
                b[got] = '\0';
            }
        }
        fclose(f);
    }
    remove(g_cap_path);
    return b;
}

/* One ordinary fixed-function batch with an optional stage-program word. */
static int submit_one_batch(Fx *f, uint32_t stage_program)
{
    f->pbw = 0;
    emit(f, M_SET_OBJECT, OBJ_HANDLE);
    if (stage_program)
        emit(f, M_SHADER_STAGE_PROGRAM, stage_program);
    emit_batch_arrays(f);
    return fx_submit(f);
}

/* ── (a) fixed-function texgen / texture matrix ─────────────────────────────── */

static int case_ff_neither(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    if (!submit_one_batch(&f, 0)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count", L->ff_texgen_batches, 0);
    chk_u64("ff_texmat count", L->ff_texmat_batches, 0);
    chk_u64("ff_texgen_texmat count", L->ff_texgen_texmat_batches, 0);
    chk_u32("first_a unclaimed", L->first_a.valid, 0);
    return 0;
}

static int case_ff_texgen_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x2400u);            /* stage 0 S: EYE_LINEAR */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count", L->ff_texgen_batches, 1);
    chk_u64("ff_texmat count", L->ff_texmat_batches, 0);
    chk_u64("ff_texgen_texmat count", L->ff_texgen_texmat_batches, 0);
    chk_u32("first_a valid", L->first_a.valid, 1);
    chk_u32("first_a draw ordinal", L->first_a.draw, 1);
    chk_u32("first_a texgen raw stage0 S", L->first_a.texgen[0][0], 0x2400u);
    chk_u32("first_a texgen_seen has only delivered word",
            L->first_a.texgen_seen, 0x1u);
    chk_u32("first_a mat_enable_seen is zero", L->first_a.mat_enable_seen, 0);
    chk((int)L->first_a.matrix_seen == 0, "first_a matrix_seen is zero", "%llu",
        (unsigned long long)L->first_a.matrix_seen);
    return 0;
}

static int case_ff_texmat_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXMAT_ENABLE, 0x00000001u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count", L->ff_texgen_batches, 0);
    chk_u64("ff_texmat count", L->ff_texmat_batches, 1);
    chk_u64("ff_texgen_texmat count", L->ff_texgen_texmat_batches, 0);
    chk_u32("first_a valid", L->first_a.valid, 1);
    chk_u32("first_a mat_enable stage0 raw", L->first_a.mat_enable[0], 1);
    chk_u32("first_a mat_enable_seen stage0", L->first_a.mat_enable_seen, 0x1u);
    chk_u32("first_a texgen_seen is zero", L->first_a.texgen_seen, 0);
    return 0;
}

static int case_ff_both_stage_assoc(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    int s, j;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    /* Stage 0 and stage 1, distinct raw values so association is decidable. */
    emit(&f, M_SET_TEXGEN_S, 0x2401u);
    emit(&f, M_SET_TEXGEN_T, 0x2402u);
    emit(&f, M_SET_TEXMAT_ENABLE, 0x00000003u);
    emit(&f, 0x0424u, 0x00000005u);
    for (j = 0; j < 16; j++) {
        emit(&f, M_SET_TEXMAT + 4u * (uint32_t)j, 0x1000u + (uint32_t)j);
        emit(&f, 0x0700u + 4u * (uint32_t)j, 0x2000u + (uint32_t)j);
    }
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count", L->ff_texgen_batches, 1);
    chk_u64("ff_texmat count", L->ff_texmat_batches, 1);
    chk_u64("ff_texgen_texmat count", L->ff_texgen_texmat_batches, 1);
    chk_u32("first_a texgen_seen stage0 S/T", L->first_a.texgen_seen, 0x3u);
    chk_u32("first_a mat_enable_seen stages 0/1", L->first_a.mat_enable_seen, 0x3u);
    chk_u32("first_a stage0 matrix-enable raw", L->first_a.mat_enable[0], 0x3u);
    chk_u32("first_a stage1 matrix-enable raw", L->first_a.mat_enable[1], 0x5u);
    for (j = 0; j < 16; j++) {
        chk_u32("first_a texture_matrix stage0 association",
                L->first_a.texture_matrix[0][j], 0x1000u + (uint32_t)j);
        chk_u32("first_a texture_matrix stage1 association",
                L->first_a.texture_matrix[1][j], 0x2000u + (uint32_t)j);
    }
    chk(L->first_a.matrix_seen == ((1ull << 32) - 1ull),
        "first_a matrix_seen stages 0/1", "%016llX",
        (unsigned long long)L->first_a.matrix_seen);
    for (s = 2; s < 4; s++)
        chk_u32("first_a unadmitted matrix stage word stays zero",
                L->first_a.texture_matrix[s][0], 0);
    return 0;
}

static int case_ff_stage1_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, 0x03D0u, 0x2401u);                   /* stage 1 S */
    emit(&f, 0x0424u, 0x00000002u);               /* stage 1 enable, NOT a bitmask */
    emit(&f, 0x0700u, 0xA1B2C3D4u);               /* stage 1 matrix j=0 */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count (stage 1 only)", L->ff_texgen_batches, 1);
    chk_u64("ff_texmat count (stage 1 only)", L->ff_texmat_batches, 1);
    chk_u64("ff_texgen_texmat count (stage 1 only)",
            L->ff_texgen_texmat_batches, 1);
    chk_u32("first_a texgen_seen stage1 S", L->first_a.texgen_seen, 1u << 4);
    chk_u32("first_a mat_enable_seen stage1", L->first_a.mat_enable_seen, 1u << 1);
    chk_u32("first_a stage1 enable raw", L->first_a.mat_enable[1], 2u);
    chk_u32("first_a stage1 matrix raw", L->first_a.texture_matrix[1][0], 0xA1B2C3D4u);
    chk(L->first_a.matrix_seen == ((uint64_t)1 << 16),
        "first_a matrix_seen marks stage1 j0 only", "%016llX",
        (unsigned long long)L->first_a.matrix_seen);
    return 0;
}

static int case_ff_stage3_texgen_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, 0x03F0u, 0x2401u);                   /* stage 3 S, no matrix enable */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count (stage 3 only)", L->ff_texgen_batches, 1);
    chk_u64("ff_texmat count (stage 3 only)", L->ff_texmat_batches, 0);
    chk_u64("ff_texgen_texmat count (stage 3 only)",
            L->ff_texgen_texmat_batches, 0);
    chk_u32("first_a valid", L->first_a.valid, 1);
    chk_u32("first_a draw ordinal", L->first_a.draw, 1);
    chk_u32("first_a texgen raw stage3 S", L->first_a.texgen[3][0], 0x2401u);
    chk_u32("first_a texgen_seen is stage3 S only",
            L->first_a.texgen_seen, 1u << 12);
    chk_u32("first_a mat_enable_seen is zero", L->first_a.mat_enable_seen, 0);
    chk((int)L->first_a.matrix_seen == 0,
        "first_a matrix_seen is zero (no payload written)", "%llu",
        (unsigned long long)L->first_a.matrix_seen);
    chk_u32("stage0 texgen stays zero", L->first_a.texgen[0][0], 0);
    return 0;
}

static int case_ff_texgen_zero_seen(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x00000000u);        /* delivered zero */
    emit(&f, M_SET_TEXMAT_ENABLE, 0x00000001u);   /* makes the batch qualify */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("delivered zero does not mean texgen enabled", L->ff_texgen_batches, 0);
    chk_u64("matrix enable qualifies the batch", L->ff_texmat_batches, 1);
    chk_u64("both counters are separate", L->ff_texgen_texmat_batches, 0);
    chk_u32("first_a valid", L->first_a.valid, 1);
    chk_u32("first_a draw ordinal", L->first_a.draw, 1);
    chk_u32("delivered zero raw value", L->first_a.texgen[0][0], 0);
    chk_u32("delivered zero keeps its seen bit",
            L->first_a.texgen_seen & (1u << 0), 1u);
    chk_u32("undelivered Q cannot acquire a bit",
            L->first_a.texgen_seen & (1u << 3), 0);
    chk_u32("only the delivered stage0 S word is seen",
            L->first_a.texgen_seen, 0x1u);
    chk_u32("first_a mat_enable_seen stage0", L->first_a.mat_enable_seen, 0x1u);
    chk_u32("first_a stage0 enable raw", L->first_a.mat_enable[0], 1u);
    chk((int)L->first_a.matrix_seen == 0,
        "no texture-matrix payload was written", "%llu",
        (unsigned long long)L->first_a.matrix_seen);
    return 0;
}

static int case_ff_stage2_enable_no_matrix(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, 0x03E0u, 0x2401u);                   /* stage 2 S */
    emit(&f, 0x0428u, 0x00000001u);               /* stage 2 enable */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count (stage 2)", L->ff_texgen_batches, 1);
    chk_u64("ff_texmat count (stage 2)", L->ff_texmat_batches, 1);
    chk_u32("first_a texgen_seen stage2 S", L->first_a.texgen_seen, 1u << 8);
    chk_u32("first_a mat_enable_seen stage2", L->first_a.mat_enable_seen, 1u << 2);
    chk((int)L->first_a.matrix_seen == 0,
        "first_a matrix_seen stays zero: stage2 matrix is not admitted", "%llu",
        (unsigned long long)L->first_a.matrix_seen);
    chk_u32("first_a stage2 matrix raw is UNKNOWN zero",
            L->first_a.texture_matrix[2][0], 0);
    return 0;
}

static int case_ff_stage3_enable_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, 0x042Cu, 0x00000007u);               /* stage 3 enable raw */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen count", L->ff_texgen_batches, 0);
    chk_u64("ff_texmat count (stage 3)", L->ff_texmat_batches, 1);
    chk_u32("first_a mat_enable_seen stage3", L->first_a.mat_enable_seen, 1u << 3);
    chk_u32("first_a stage3 enable raw", L->first_a.mat_enable[3], 7u);
    chk((int)L->first_a.matrix_seen == 0,
        "first_a matrix_seen stays zero: stage3 matrix is not admitted", "%llu",
        (unsigned long long)L->first_a.matrix_seen);
    return 0;
}

/* ── (b) stage modes >= 6 ───────────────────────────────────────────────────── */

static int case_stage6_isolated(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    if (!submit_one_batch(&f, 6u)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("stage_ge6 batch count", L->stage_ge6_batches, 1);
    chk_u64("stage_ge6 by stage0", L->stage_ge6_by_stage[0], 1);
    chk_u64("stage_ge6 by stage1", L->stage_ge6_by_stage[1], 0);
    chk_u64("stage_mode_hist[6]", L->stage_mode_hist[6], 1);
    chk_u64("stage_mode_hist[7]", L->stage_mode_hist[7], 0);
    chk_u32("first_b valid", L->first_b.valid, 1);
    chk_u32("first_b draw ordinal", L->first_b.draw, 1);
    chk_u32("first_b stage_program raw", L->first_b.stage_program, 6u);
    chk_u32("first_b stage_program_seen", L->first_b.stage_program_seen, 1);
    chk_u32("first_b modes[0]", L->first_b.modes[0], 6u);
    chk_u32("first_b modes[1..3] zero", L->first_b.modes[1] | L->first_b.modes[2]
                                        | L->first_b.modes[3], 0);
    chk_u32("first_b bump_seen clears unadmitted stage 0",
            L->first_b.bump_seen & 0x3Fu, 0);
    chk_u32("first_b stage0 bump raw remains UNKNOWN zero",
            L->first_b.bump_mat[0][0] | L->first_b.bump_scale[0]
            | L->first_b.bump_offset[0], 0);
    chk_u32("first_b vp_start default", L->first_b.vp_start, 0);
    chk_u32("first_b vp_first_word unknown zero", L->first_b.vp_first_word, 0);
    chk_u32("first_b vp_first_word_seen clear", L->first_b.vp_first_word_seen, 0);
    return 0;
}

static int case_stage7_isolated(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    if (!submit_one_batch(&f, 7u << 15)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("stage_ge6 batch count", L->stage_ge6_batches, 1);
    chk_u64("stage_ge6 by stage3", L->stage_ge6_by_stage[3], 1);
    chk_u64("stage_ge6 by stage0", L->stage_ge6_by_stage[0], 0);
    chk_u64("stage_mode_hist[7]", L->stage_mode_hist[7], 1);
    chk_u64("stage_mode_hist[6]", L->stage_mode_hist[6], 0);
    chk_u32("first_b modes[3]", L->first_b.modes[3], 7u);
    chk_u32("first_b modes[0..2] zero", L->first_b.modes[0] | L->first_b.modes[1]
                                        | L->first_b.modes[2], 0);
    return 0;
}

static int case_stage6_7_multi(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    /* stage0 = 6, stage1 = 5 (supported), stage2 = 7, stage3 = 4. */
    if (!submit_one_batch(&f, 6u | (5u << 5) | (7u << 10) | (4u << 15))) {
        chk(0, "batch accepted");
        return 1;
    }
    L = ledger();
    chk_u64("stage_ge6 batch count once", L->stage_ge6_batches, 1);
    chk_u64("stage_ge6 by stage0", L->stage_ge6_by_stage[0], 1);
    chk_u64("stage_ge6 by stage2", L->stage_ge6_by_stage[2], 1);
    chk_u64("stage_ge6 by stage1", L->stage_ge6_by_stage[1], 0);
    chk_u64("stage_ge6 by stage3", L->stage_ge6_by_stage[3], 0);
    chk_u64("stage_mode_hist[6]", L->stage_mode_hist[6], 1);
    chk_u64("stage_mode_hist[7]", L->stage_mode_hist[7], 1);
    chk_u32("first_b modes[0]", L->first_b.modes[0], 6u);
    chk_u32("first_b modes[1]", L->first_b.modes[1], 5u);
    chk_u32("first_b modes[2]", L->first_b.modes[2], 7u);
    chk_u32("first_b modes[3]", L->first_b.modes[3], 4u);
    chk_u32("first_b stage_program raw", L->first_b.stage_program,
            6u | (5u << 5) | (7u << 10) | (4u << 15));
    return 0;
}

static int case_stage_batch_vs_incidence(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    /* Batch 1: mode 6 on stages 0 and 1 -> two incidences, one batch.
     * Batch 2: mode 7 on stage 2. */
    if (!submit_one_batch(&f, 6u | (6u << 5))) { chk(0, "batch 1 accepted"); return 1; }
    if (!submit_one_batch(&f, 7u << 10)) { chk(0, "batch 2 accepted"); return 1; }
    L = ledger();
    chk_u64("stage_ge6 batches", L->stage_ge6_batches, 2);
    chk_u64("stage_ge6 by stage0", L->stage_ge6_by_stage[0], 1);
    chk_u64("stage_ge6 by stage1", L->stage_ge6_by_stage[1], 1);
    chk_u64("stage_ge6 by stage2", L->stage_ge6_by_stage[2], 1);
    chk_u64("stage_mode_hist[6] counts incidences", L->stage_mode_hist[6], 2);
    chk_u64("stage_mode_hist[7] counts incidences", L->stage_mode_hist[7], 1);
    chk_u64("histogram total is the incidence total",
            L->stage_mode_hist[6] + L->stage_mode_hist[7], 3);
    chk_u32("first_b draw is the first qualifying batch", L->first_b.draw, 1);
    return 0;
}

/* The production back end entry, so the program batch really takes the GPU
 * path; after the back end is removed the identical stream falls back to the
 * CPU and must still count once. */
static uint32_t g_fake_draw_program_calls;
static uint32_t g_fake_draw_calls;
static int fake_draw_program(const Nv2aSurface *s, const Nv2aBatch *b, const Nv2aProgramBatch *p)
{
    (void)s; (void)b; (void)p;
    g_fake_draw_program_calls++;
    return 1;
}
static void fake_draw(const Nv2aSurface *s, const Nv2aBatch *b)
{ (void)s; (void)b; g_fake_draw_calls++; }
static void fake_clear(const Nv2aSurface *s, const Nv2aRenderState *rs,
                       uint32_t flags, uint32_t argb, uint32_t zstencil)
{ (void)s; (void)rs; (void)flags; (void)argb; (void)zstencil; }
static void fake_flip(void) {}
static const Nv2aBackend g_program_backend = {
    fake_clear, fake_draw, fake_flip, fake_draw_program, NULL
};

static void emit_program_mov_final(Fx *f)
{
    /* One FINAL instruction: MOV oPos, v0. Encoded from the field table at the
     * top of the toolkit's nv2a_vsh_interp.c (public program encoding). */
    emit(f, M_TRANSFORM_PROGRAM + 0x00u, 0x00000000u);
    emit(f, M_TRANSFORM_PROGRAM + 0x04u, 0x200000E4u);
    emit(f, M_TRANSFORM_PROGRAM + 0x08u, 0x01C80000u);
    emit(f, M_TRANSFORM_PROGRAM + 0x0Cu, 0x0000F801u);
}

static int case_stage_program_path_count(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;

    /* Program mode, combiners seen, valid FINAL program, GPU-capable back end. */
    nv2a_backend_register(&g_program_backend);
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(&f, M_TRANSFORM_EXEC_MODE, 2u);
    emit(&f, M_COMBINER_CONTROL, 1u);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit_program_mov_final(&f);
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "program batch accepted"); return 1; }
    chk_u32("the back end executed the program batch", g_fake_draw_program_calls, 1);
    chk_u32("the fixed-function draw entry was not used", g_fake_draw_calls, 0);
    L = ledger();
    chk_u64("program batch counted once", L->stage_ge6_batches, 1);

    /* Remove the back end: the same program batch is transformed by the CPU. */
    nv2a_backend_register(NULL);
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "fallback batch accepted"); return 1; }
    L = ledger();
    chk_u64("CPU-fallback program batch counted once too",
            L->stage_ge6_batches, 2);
    chk_u64("stage0 incidences", L->stage_ge6_by_stage[0], 2);
    return 0;
}

static int case_stage_first_retained(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbCoverageLedger first;
    Fx f;
    if (!fx_init(&f, "1")) return 1;

    /* First qualifying batch: stage 1 bump words ARE admitted (0x1B68...),
     * stage 0's are not; selected vp start/word are uploaded. One of the
     * admitted words is a deliberate zero, so an observed zero and an
     * undelivered zero stay distinguishable. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(&f, M_BUMP_MAT1, 0x11111111u);
    emit(&f, M_BUMP_MAT1 + 4u, 0x22222222u);
    emit(&f, M_BUMP_MAT1 + 8u, 0x33333333u);
    emit(&f, M_BUMP_MAT1 + 12u, 0x00000000u);     /* delivered zero */
    emit(&f, M_BUMP_SCALE1, 0x55555555u);
    emit(&f, M_BUMP_OFFSET1, 0x66666666u);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit(&f, M_TRANSFORM_PROGRAM, 0xDEADBEEFu);
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "first batch accepted"); return 1; }
    L = ledger();
    chk_u32("first_b valid", L->first_b.valid, 1);
    chk_u32("bump_seen marks stage1 MAT00..offset", L->first_b.bump_seen, 0xFC0u);
    chk_u32("stage1 bump MAT raw", L->first_b.bump_mat[1][0], 0x11111111u);
    chk_u32("stage1 bump MAT10 raw", L->first_b.bump_mat[1][2], 0x33333333u);
    chk_u32("delivered zero keeps its availability bit",
            L->first_b.bump_seen & (1u << 9), 1u << 9);
    chk_u32("delivered zero stays an observed zero", L->first_b.bump_mat[1][3], 0);
    chk_u32("stage1 bump scale raw", L->first_b.bump_scale[1], 0x55555555u);
    chk_u32("stage1 bump offset raw", L->first_b.bump_offset[1], 0x66666666u);
    chk_u32("vp_start", L->first_b.vp_start, 0u);
    chk_u32("vp_first_word", L->first_b.vp_first_word, 0xDEADBEEFu);
    chk_u32("vp_first_word_seen", L->first_b.vp_first_word_seen, 1u);
    first = *L;

    /* Second batch: different program state, different bump words, reselected
     * vp word. The first record must be byte-identical. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 7u << 10);
    emit(&f, M_BUMP_MAT1, 0xAAAAAAAAu);
    emit(&f, M_BUMP_SCALE1, 0xBBBBBBBBu);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 1u);
    emit(&f, M_TRANSFORM_PROGRAM, 0xFEEDFACEu);
    emit(&f, M_TRANSFORM_PROGRAM_START, 1u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "second batch accepted"); return 1; }
    L = ledger();
    chk_u64("both batches counted", L->stage_ge6_batches, 2);
    chk_u64("stage0 incidence", L->stage_ge6_by_stage[0], 1);
    chk_u64("stage2 incidence", L->stage_ge6_by_stage[2], 1);
    chk_bytes("first_b is immutable across later batches",
              &L->first_b, &first.first_b, sizeof first.first_b);
    return 0;
}

/* ── (c) lit fixed-function ─────────────────────────────────────────────────── */

static int case_lit_nonqualifying(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;

    /* Batch 1: lighting off, a spot light and specular are enabled anyway. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 0u);
    emit(&f, M_SET_SPECULAR_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 3u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "lighting-off batch accepted"); return 1; }

    /* Batch 2: lighting on, but no specular and no lights. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_SPECULAR_ENABLE, 0u);
    emit(&f, M_SET_LIGHT_MASK, 0u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "no-lights batch accepted"); return 1; }

    L = ledger();
    chk_u64("lit_specular count", L->lit_specular_batches, 0);
    chk_u64("lit_spot count", L->lit_spot_batches, 0);
    chk_u64("lit_both count", L->lit_both_batches, 0);
    chk_u32("first_c unclaimed", L->first_c.valid, 0);
    return 0;
}

static int case_lit_specular_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 2u);
    emit(&f, M_SET_SPECULAR_ENABLE, 3u);
    emit(&f, M_SET_LIGHT_MASK, 0x00000015u);      /* kinds: 1, 1, 0, 1 */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("lit_specular count", L->lit_specular_batches, 1);
    chk_u64("lit_spot count", L->lit_spot_batches, 0);
    chk_u64("lit_both count", L->lit_both_batches, 0);
    chk_u32("first_c valid", L->first_c.valid, 1);
    chk_u32("first_c draw ordinal", L->first_c.draw, 1);
    chk_u32("first_c lighting raw", L->first_c.lighting_enable, 2u);
    chk_u32("first_c specular raw", L->first_c.specular_enable, 3u);
    chk_u32("first_c light mask raw", L->first_c.light_mask, 0x15u);
    chk_u32("first_c all three enables delivered", L->first_c.enable_seen, 0x7u);
    chk_u32("first_c light kind0", L->first_c.light_kind[0], 1u);
    chk_u32("first_c light kind1", L->first_c.light_kind[1], 1u);
    chk_u32("first_c light kind2", L->first_c.light_kind[2], 1u);
    chk_u32("first_c light kind3", L->first_c.light_kind[3], 0u);
    chk((int)L->first_c.spot_light == -1, "first_c spot_light is -1", "%d",
        (int)L->first_c.spot_light);
    chk_u32("first_c spot_seen zero", L->first_c.spot_seen, 0);
    return 0;
}

static int case_lit_spot_only(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 0x00000003u);      /* light0 = SPOT */
    /* 0x3B8 is deliberately never committed: its seen bit must stay clear and
     * the raw value must stay UNKNOWN zero. */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u64("lit_specular count", L->lit_specular_batches, 0);
    chk_u64("lit_spot count", L->lit_spot_batches, 1);
    chk_u64("lit_both count", L->lit_both_batches, 0);
    chk_u32("first_c valid", L->first_c.valid, 1);
    chk_u32("first_c specular raw UNKNOWN zero", L->first_c.specular_enable, 0);
    chk_u32("first_c enable_seen excludes undelivered 0x3B8",
            L->first_c.enable_seen, 0x5u);
    chk((int)L->first_c.spot_light == 0, "first_c spot_light index", "%d",
        (int)L->first_c.spot_light);
    chk_u32("first_c light kind0", L->first_c.light_kind[0], 3u);
    chk_u32("first_c spot_seen is zero (payloads unadmitted)",
            L->first_c.spot_seen, 0);
    chk_u32("first_c spot falloff raw UNKNOWN zero",
            L->first_c.spot_falloff[0] | L->first_c.spot_falloff[1]
            | L->first_c.spot_falloff[2], 0);
    chk_u32("first_c spot direction raw UNKNOWN zero",
            L->first_c.spot_direction[0] | L->first_c.spot_direction[1]
            | L->first_c.spot_direction[2], 0);
    chk_u32("first_c local attenuation raw UNKNOWN zero",
            L->first_c.local_attenuation[0] | L->first_c.local_attenuation[1]
            | L->first_c.local_attenuation[2], 0);
    return 0;
}

static int case_lit_both_and_immutable(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbCoverageLedger first;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_SPECULAR_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 0x00000039u);      /* 1, local, SPOT */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "first batch accepted"); return 1; }
    L = ledger();
    chk_u64("lit_specular count", L->lit_specular_batches, 1);
    chk_u64("lit_spot count", L->lit_spot_batches, 1);
    chk_u64("lit_both count", L->lit_both_batches, 1);
    chk_u32("first_c spot_light is light 2", (uint32_t)L->first_c.spot_light, 2u);
    chk_u32("first_c kind1 local", L->first_c.light_kind[1], 2u);
    chk_u32("first_c kind2 spot", L->first_c.light_kind[2], 3u);
    first = *L;

    /* A later batch with a different mask and specular state must not rewrite
     * the first record, and both counters advance independently. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_SPECULAR_ENABLE, 2u);
    emit(&f, M_SET_LIGHT_MASK, 0x0000000Cu);      /* light1 and light2 SPOT */
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "second batch accepted"); return 1; }
    L = ledger();
    chk_u64("lit_specular count after second", L->lit_specular_batches, 2);
    chk_u64("lit_spot count after second", L->lit_spot_batches, 2);
    chk_u64("lit_both count after second", L->lit_both_batches, 2);
    chk_bytes("first_c is immutable across later batches",
              &L->first_c, &first.first_c, sizeof first.first_c);
    return 0;
}

/* ── cross-cutting: first records / reports ─────────────────────────────────── */

static int case_first_records_immutable(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbCoverageLedger first;
    Fx f;
    if (!fx_init(&f, "1")) return 1;

    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x2400u);
    emit(&f, M_SET_TEXMAT_ENABLE, 1u);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_SPECULAR_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 3u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "first batch accepted"); return 1; }
    L = ledger();
    chk_u32("all three first records claimed",
            L->first_a.valid + L->first_b.valid + L->first_c.valid, 3u);
    first = *L;

    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x2402u);
    emit(&f, M_SET_TEXGEN_T, 0x2401u);
    emit(&f, M_SET_TEXMAT_ENABLE, 0xBu);
    emit(&f, M_SET_TEXMAT, 0x99999999u);
    emit(&f, M_SHADER_STAGE_PROGRAM, 7u << 5);
    emit(&f, M_SET_LIGHT_MASK, 0x30u);
    emit(&f, M_SET_LIGHTING_ENABLE, 0u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "second batch accepted"); return 1; }
    L = ledger();
    chk_bytes("first_a immutable", &L->first_a, &first.first_a, sizeof first.first_a);
    chk_bytes("first_b immutable", &L->first_b, &first.first_b, sizeof first.first_b);
    chk_bytes("first_c immutable", &L->first_c, &first.first_c, sizeof first.first_c);
    chk_u64("ff_texgen still cumulative", L->ff_texgen_batches, 2);
    chk_u64("ff_texmat still cumulative", L->ff_texmat_batches, 2);
    chk_u64("stage_ge6 still cumulative", L->stage_ge6_batches, 2);
    /* The second batch has lighting off, so only the first one is lit. */
    chk_u64("lit_specular did not count the unlit batch",
            L->lit_specular_batches, 1);
    return 0;
}

static int case_report_retained(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbCoverageLedger before, after_first, after_second;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x2400u);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 3u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    before = *L;
    nv2a_pb_exec_report();
    after_first = *ledger();
    nv2a_pb_exec_report();
    after_second = *ledger();
    /* The report prints; it must not reset or mutate any retained state. */
    chk_bytes("report leaves the ledger byte-identical (first call)",
              &after_first, &before, sizeof before);
    chk_bytes("report leaves the ledger byte-identical (second call)",
              &after_second, &before, sizeof before);
    chk_u32("report did not claim or clear a first record",
            after_second.first_a.valid + after_second.first_b.valid
            + after_second.first_c.valid, 3u);
    return 0;
}

/* ── cross-cutting: selected vertex-program word ───────────────────────────── */

static int submit_vp_batch(Fx *f)
{
    f->pbw = 0;
    emit(f, M_SET_OBJECT, OBJ_HANDLE);
    emit(f, M_SHADER_STAGE_PROGRAM, 6u);
    emit_batch_arrays(f);
    return fx_submit(f);
}

static int case_vp_port_0b00(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0xCAFEBABEu);   /* port 0x0B00, comp 0 */
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("vp_start", L->first_b.vp_start, 0u);
    chk_u32("vp_first_word from 0x0B00", L->first_b.vp_first_word, 0xCAFEBABEu);
    chk_u32("vp_first_word_seen from 0x0B00", L->first_b.vp_first_word_seen, 1u);
    return 0;
}

static int case_vp_port_0b10(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 3u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x10u, 0x1234ABCDu);   /* port 0x0B10, comp 0 */
    emit(&f, M_TRANSFORM_PROGRAM_START, 3u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("vp_start", L->first_b.vp_start, 3u);
    chk_u32("vp_first_word from 0x0B10", L->first_b.vp_first_word, 0x1234ABCDu);
    chk_u32("vp_first_word_seen from 0x0B10", L->first_b.vp_first_word_seen, 1u);
    return 0;
}

static int case_vp_port_0b70(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 5u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x70u, 0x0BADF00Du);   /* port 0x0B70, comp 0 */
    emit(&f, M_TRANSFORM_PROGRAM_START, 5u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("vp_start", L->first_b.vp_start, 5u);
    chk_u32("vp_first_word from 0x0B70", L->first_b.vp_first_word, 0x0BADF00Du);
    chk_u32("vp_first_word_seen from 0x0B70", L->first_b.vp_first_word_seen, 1u);
    return 0;
}

static int case_vp_cursor_advance(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 1u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0x00000001u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x04u, 0x00000002u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x08u, 0x00000003u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x0Cu, 0x00000004u);   /* advances to slot 2 */
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0xABCD0000u);   /* slot 2 dword 0 */
    emit(&f, M_TRANSFORM_PROGRAM_START, 2u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("vp_start after cursor advance", L->first_b.vp_start, 2u);
    chk_u32("vp_first_word of the advanced slot",
            L->first_b.vp_first_word, 0xABCD0000u);
    chk_u32("vp_first_word_seen of the advanced slot",
            L->first_b.vp_first_word_seen, 1u);
    return 0;
}

static int case_vp_component_unavailable(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbCoverageLedger first;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    /* Only components 1, 2 and 3 of slot 0 are written: dword 0 availability
     * must stay clear, and the untouched slot value is UNKNOWN, not an
     * observed zero. The component-3 write advances the cursor, which must
     * not move the provenance either. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x04u, 0x77777777u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x08u, 0x88888888u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x0Cu, 0x99999999u);
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("component 1 does not establish dword0 availability",
            L->first_b.vp_first_word_seen, 0);
    chk_u32("untouched dword0 is raw zero", L->first_b.vp_first_word, 0);
    first = *L;

    /* A later accepted component-0 write must not retroactively claim the
     * already-claimed first record. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0x55AA55AAu);
    if (!fx_submit(&f)) { chk(0, "late upload accepted"); return 1; }
    L = ledger();
    chk_bytes("first_b keeps its frozen availability",
              &L->first_b, &first.first_b, sizeof first.first_b);
    return 0;
}

static int case_vp_zero_observed(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, 0u);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0x00000000u);   /* legitimate zero */
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("observed zero word", L->first_b.vp_first_word, 0);
    chk_u32("observed zero has its seen bit set",
            L->first_b.vp_first_word_seen, 1);
    return 0;
}

static int case_vp_upload_past_end(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM_LOAD, NV2A_VSH_SLOTS);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0xDEADBEEFu);   /* dropped: past end */
    emit(&f, M_TRANSFORM_PROGRAM_START, 0u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("out-of-range upload cannot claim availability",
            L->first_b.vp_first_word_seen, 0);
    chk_u32("slot 0 was not written by the dropped upload",
            L->first_b.vp_first_word, 0);
    return 0;
}

static int case_vp_start_out_of_range(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_TRANSFORM_PROGRAM + 0x00u, 0xABCD1234u);   /* slot 0 is written */
    emit(&f, M_TRANSFORM_PROGRAM_START, NV2A_VSH_SLOTS + 4u);
    if (!fx_submit(&f)) { chk(0, "upload stream accepted"); return 1; }
    if (!submit_vp_batch(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("out-of-range start is retained raw",
            L->first_b.vp_start, NV2A_VSH_SLOTS + 4u);
    chk_u32("out-of-range start yields UNKNOWN zero word",
            L->first_b.vp_first_word, 0);
    chk_u32("out-of-range start clears availability",
            L->first_b.vp_first_word_seen, 0);
    return 0;
}

/* ── cross-cutting: gate ───────────────────────────────────────────────────── */

/* A back end whose coverage hook is observable. The executor's report is the
 * only caller, and its call sits inside pb_coverage(); counting the calls is
 * how the off-path "no coverage output" claim is decided for a registered
 * back end (the Vulkan callback plays this role in production). */
static uint32_t g_cov_hook_calls;
static void coverage_hook_counter(void) { ++g_cov_hook_calls; }
static const Nv2aBackend g_cov_backend = {
    NULL, NULL, NULL, NULL, NULL, coverage_hook_counter
};

/* Submit a workload that would raise every class of counter with the gate on. */
static int submit_all_classes(Fx *f)
{
    f->pbw = 0;
    emit(f, M_SET_OBJECT, OBJ_HANDLE);
    emit(f, M_SET_TEXGEN_S, 0x2400u);
    emit(f, M_SET_TEXMAT_ENABLE, 1u);
    emit(f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(f, M_SET_LIGHTING_ENABLE, 1u);
    emit(f, M_SET_SPECULAR_ENABLE, 1u);
    emit(f, M_SET_LIGHT_MASK, 3u);
    emit_batch_arrays(f);
    return fx_submit(f);
}

static int case_gate_off(void)
{
    const Nv2aPbCoverageLedger *L;
    const char *v = getenv("RECOMP_PB_COVERAGE");
    Fx f;
    if (!fx_init(&f, "off")) return 1;
    g_cov_hook_calls = 0;
    nv2a_backend_register(&g_cov_backend);
    if (!submit_all_classes(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk(!(v && v[0] == '1' && v[1] == '\0'), "CTest env is not the exact-1 gate",
        "got '%s'", v ? v : "(unset)");
    chk_u32("gate off: enabled stays 0", L->enabled, 0);
    chk_u32("gate off: identity magic", L->magic, NV2A_PB_COVERAGE_MAGIC);
    chk_u32("gate off: identity version", L->version, 3u);
    chk_u32("gate off: identity size", L->size,
            (uint32_t)sizeof(Nv2aPbCoverageLedger));
    chk_u64("gate off: ff_texgen", L->ff_texgen_batches, 0);
    chk_u64("gate off: ff_texmat", L->ff_texmat_batches, 0);
    chk_u64("gate off: stage_ge6", L->stage_ge6_batches, 0);
    chk_u64("gate off: stage histogram", L->stage_mode_hist[6], 0);
    chk_u64("gate off: lit_specular", L->lit_specular_batches, 0);
    chk_u64("gate off: lit_spot", L->lit_spot_batches, 0);
    chk_u32("gate off: no first_a", L->first_a.valid, 0);
    chk_u32("gate off: no first_b", L->first_b.valid, 0);
    chk_u32("gate off: no first_c", L->first_c.valid, 0);
    {
        char *log = NULL;
        chk(capture_begin("gate-off"), "stderr capture opened");
        if (g_cap_saved >= 0)
            nv2a_pb_exec_report();
        log = capture_end();
        chk(log != NULL, "gate off: report output was captured");
        chk(log != NULL && strstr(log, "[PBCOV]") == NULL,
            "gate off: report prints no [PBCOV] line", "captured %u bytes",
            (unsigned)(log ? strlen(log) : 0));
        free(log);
    }
    chk_u32("gate off: the back end coverage hook is not called",
            g_cov_hook_calls, 0);
    chk_u64("gate off: report does not enable or count", L->stage_ge6_batches, 0);
    nv2a_backend_register(NULL);
    return 0;
}

static int case_gate_first_sample_off_wins(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    _putenv_s("RECOMP_PB_COVERAGE", "");          /* unset */
    if (!fx_init(&f, "off")) return 1;
    /* Sample the gate through the report before any batch. */
    nv2a_pb_exec_report();
    _putenv_s("RECOMP_PB_COVERAGE", "1");         /* too late */
    if (!submit_all_classes(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("later env change cannot turn the gate on", L->enabled, 0);
    chk_u64("later env change cannot start counting", L->stage_ge6_batches, 0);
    chk_u32("later env change cannot claim a record", L->first_a.valid, 0);
    return 0;
}

static int case_gate_first_sample_on_wins(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    _putenv_s("RECOMP_PB_COVERAGE", "1");
    if (!fx_init(&f, "on")) return 1;
    nv2a_pb_exec_report();                        /* samples exact 1 */
    _putenv_s("RECOMP_PB_COVERAGE", "0");         /* too late */
    if (!submit_all_classes(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("later env change cannot turn the gate off", L->enabled, 1);
    chk_u64("the gate keeps counting after the env changes",
            L->ff_texgen_batches, 1);
    chk_u64("and keeps its stage counts", L->stage_ge6_batches, 1);
    return 0;
}

static int case_gate_on_identity(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    g_cov_hook_calls = 0;
    nv2a_backend_register(&g_cov_backend);
    if (!submit_all_classes(&f)) { chk(0, "batch accepted"); return 1; }
    L = ledger();
    chk_u32("identity magic", L->magic, NV2A_PB_COVERAGE_MAGIC);
    chk_u32("identity version", L->version, 3u);
    chk_u32("identity size", L->size, (uint32_t)sizeof(Nv2aPbCoverageLedger));
    chk_u32("enabled", L->enabled, 1);
    chk_u64("one batch raises ff_texgen", L->ff_texgen_batches, 1);
    chk_u64("one batch raises ff_texmat", L->ff_texmat_batches, 1);
    chk_u64("one batch raises both", L->ff_texgen_texmat_batches, 1);
    chk_u64("one batch raises stage_ge6", L->stage_ge6_batches, 1);
    chk_u64("one batch raises specular", L->lit_specular_batches, 1);
    chk_u64("one batch raises spot", L->lit_spot_batches, 1);
    chk_u64("one batch raises both lit", L->lit_both_batches, 1);
    {
        Nv2aPbCoverageLedger before = *L, after;
        char *log = NULL;
        chk(capture_begin("gate-on"), "stderr capture opened");
        if (g_cap_saved >= 0)
            nv2a_pb_exec_report();
        log = capture_end();
        chk(log != NULL && strstr(log, "[PBCOV]") != NULL,
            "gate on: report prints a [PBCOV] line", "captured %u bytes",
            (unsigned)(log ? strlen(log) : 0));
        after = *ledger();
        chk_bytes("gate on: report leaves the ledger byte-identical",
                  &after, &before, sizeof before);
        free(log);
    }
    chk(g_cov_hook_calls >= 1,
        "gate on: the back end coverage hook is called by the report",
        "calls %u", g_cov_hook_calls);
    nv2a_backend_register(NULL);
    return 0;
}

/* ── cross-cutting: subchannel and rollback ────────────────────────────────── */

static int case_subchannel_guard(void)
{
    const Nv2aPbCoverageLedger *L;
    Nv2aPbExecCounters before, after;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    /* Bind subchannel 1 to the same NV097 object through the real RAMHT. */
    memset(&before, 0, sizeof before);
    memset(&after, 0, sizeof after);
    nv2a_pb_exec_counters(&before);
    f.pbw = 0;
    emit_method(&f, 1, M_SET_OBJECT, OBJ_HANDLE);
    emit_method(&f, 1, M_SET_TEXGEN_S, 0x2400u);
    emit_method(&f, 1, M_SET_TEXMAT_ENABLE, 1u);
    emit_method(&f, 1, M_SHADER_STAGE_PROGRAM, 6u);
    emit_method(&f, 1, M_SET_LIGHTING_ENABLE, 1u);
    emit_method(&f, 1, M_SET_SPECULAR_ENABLE, 1u);
    emit_method(&f, 1, M_SET_LIGHT_MASK, 3u);
    if (!fx_submit(&f)) { chk(0, "subchannel-1 stream accepted"); return 1; }
    nv2a_pb_exec_counters(&after);
    /* Positive control: the seven methods really were committed to the
     * executor and filtered there, rather than rejected by the walk. */
    chk_u64("subchannel-1 methods reached the executor and were filtered",
            (uint64_t)(after.unhandled - before.unhandled), 7u);
    /* A clean subchannel-0 batch: none of the subchannel-1 state may qualify. */
    if (!submit_one_batch(&f, 0)) { chk(0, "clean batch accepted"); return 1; }
    L = ledger();
    chk_u64("ff_texgen not polluted by subchannel 1", L->ff_texgen_batches, 0);
    chk_u64("ff_texmat not polluted by subchannel 1", L->ff_texmat_batches, 0);
    chk_u64("stage_ge6 not polluted by subchannel 1", L->stage_ge6_batches, 0);
    chk_u64("lit not polluted by subchannel 1", L->lit_specular_batches, 0);
    chk_u32("no first_a from a filtered subchannel", L->first_a.valid, 0);
    chk_u32("no first_b from a filtered subchannel", L->first_b.valid, 0);
    chk_u32("no first_c from a filtered subchannel", L->first_c.valid, 0);
    return 0;
}

static int case_rejected_stream_rollback(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;

    /* A single unit that is rejected at 0x3CC (TEXGEN_Q is not admitted) and
     * at 0x1040 (SPOT_FALLOFF is not admitted). The admitted 0x3C0/0x420 and
     * the batch before the rejection must not commit. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXGEN_S, 0x2400u);
    emit(&f, M_SET_TEXMAT_ENABLE, 1u);
    emit(&f, M_SET_TEXGEN_Q, 0x2400u);            /* rejected */
    emit_batch_arrays(&f);
    chk(!fx_submit(&f), "stream with unadmitted TEXGEN_Q is rejected");
    L = ledger();
    chk_u64("rejected unit counted nothing", L->ff_texgen_batches, 0);
    chk_u32("rejected unit claimed no record", L->first_a.valid, 0);

    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_LIGHTING_ENABLE, 1u);
    emit(&f, M_SET_LIGHT_MASK, 3u);
    emit(&f, M_SPOT_FALLOFF0, 0x3F800000u);       /* rejected */
    emit_batch_arrays(&f);
    chk(!fx_submit(&f), "stream with unadmitted SPOT_FALLOFF is rejected");
    L = ledger();
    chk_u32("rejected spot stream claimed no record", L->first_c.valid, 0);

    /* Clean control: the rejected admitted words must not have marked seen.
     * A texmat-only batch claims first_a; texgen_seen must still be 0. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SET_TEXMAT_ENABLE, 1u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "clean control accepted"); return 1; }
    L = ledger();
    chk_u64("clean control counts", L->ff_texmat_batches, 1);
    chk_u32("first_a texgen_seen did not inherit the rejected stream",
            L->first_a.texgen_seen, 0);
    chk_u32("first_a matrix_seen did not inherit the rejected stream",
            (uint32_t)L->first_a.matrix_seen, 0);
    return 0;
}

/* ── cross-cutting: batch guards and entry forms ───────────────────────────── */

static uint32_t g_stop_flag;
static int stop_backend_stopped(void) { return (int)g_stop_flag; }
static const Nv2aBackend g_stop_backend = {
    NULL, NULL, NULL, NULL, stop_backend_stopped
};

static int case_guards_stopped(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    nv2a_backend_register(&g_stop_backend);
    g_stop_flag = 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit(&f, M_SET_TEXGEN_S, 0x2400u);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "stopped batch accepted"); return 1; }
    L = ledger();
    chk_u64("stopped backend: no batch counted", L->stage_ge6_batches, 0);
    chk_u64("stopped backend: no ff counter", L->ff_texgen_batches, 0);
    chk_u32("stopped backend: no first record", L->first_b.valid, 0);

    /* Positive control: the same stream with the back end running counts one. */
    g_stop_flag = 0;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit_batch_arrays(&f);
    if (!fx_submit(&f)) { chk(0, "running batch accepted"); return 1; }
    L = ledger();
    chk_u64("running backend: the batch counts once", L->stage_ge6_batches, 1);
    nv2a_backend_register(NULL);
    return 0;
}

static int case_guards_sub_minimum(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    if (!fx_init(&f, "1")) return 1;
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);
    emit_short_batch(&f);
    if (!fx_submit(&f)) { chk(0, "sub-minimum stream accepted"); return 1; }
    L = ledger();
    chk_u64("a two-vertex batch counts nothing", L->stage_ge6_batches, 0);
    chk_u32("a two-vertex batch claims no first record", L->first_b.valid, 0);
    /* Positive control a few words later. */
    if (!submit_one_batch(&f, 6u)) { chk(0, "control batch accepted"); return 1; }
    L = ledger();
    chk_u64("the three-vertex control counts once", L->stage_ge6_batches, 1);
    return 0;
}

static int case_entry_forms(void)
{
    const Nv2aPbCoverageLedger *L;
    Fx f;
    uint32_t i;
    if (!fx_init(&f, "1")) return 1;

    /* One batch per entry form: DRAW_ARRAYS, ARRAY_ELEMENT16, INLINE_ARRAY,
     * immediate SET_VERTEX4F. All reach raster_batch exactly once. */
    f.pbw = 0;
    emit(&f, M_SET_OBJECT, OBJ_HANDLE);
    emit(&f, M_SHADER_STAGE_PROGRAM, 6u);

    emit_begin(&f, 5u);
    emit(&f, M_DRAW_ARRAYS, (2u << 24) | 0u);
    emit_end(&f);

    emit_begin(&f, 5u);
    emit(&f, M_ARRAY_ELEMENT16, 0x00010000u);
    emit(&f, M_ARRAY_ELEMENT16, 0x00000002u);
    emit_end(&f);

    /* INLINE_ARRAY: the payload packs the enabled attributes, so each vertex is
     * float3 position (12 bytes) + D3DCOLOR diffuse (4 bytes) = 16 bytes, and
     * three vertices are twelve dwords. */
    emit(&f, M_SET_VERTEX_FORMAT0, 2u | (3u << 4));
    emit_begin(&f, 5u);
    for (i = 0; i < 12u; i++)
        emit(&f, M_INLINE_ARRAY, 0x3F800000u);
    emit_end(&f);

    /* Immediate: three SET_VERTEX4F groups of four dwords. */
    emit_begin(&f, 5u);
    for (i = 0; i < 3u; i++) {
        emit(&f, M_SET_VERTEX4F + 0x00u, 0x3F800000u);
        emit(&f, M_SET_VERTEX4F + 0x04u, 0x3F800000u);
        emit(&f, M_SET_VERTEX4F + 0x08u, 0x3F800000u);
        emit(&f, M_SET_VERTEX4F + 0x0Cu, 0x3F800000u);
    }
    emit_end(&f);

    if (!fx_submit(&f)) { chk(0, "entry-form stream accepted"); return 1; }
    L = ledger();
    chk_u64("every entry form counted exactly once", L->stage_ge6_batches, 4);
    chk_u64("stage0 incidence is the four batches", L->stage_ge6_by_stage[0], 4);
    chk_u64("histogram has four incidences", L->stage_mode_hist[6], 4);
    chk_u32("the first batch was DRAW_ARRAYS (draw ordinal 1)",
            L->first_b.draw, 1);
    /* INLINE_ARRAY and immediate mode set inline_active, so batch_is_ffp() is
     * false: the fixed-function counters must stay put even though composite
     * state is set. */
    chk_u64("entry forms do not double count fixed-function batches",
            L->ff_texgen_batches, 0);
    chk_u64("entry forms do not double count the texture-matrix counter",
            L->ff_texmat_batches, 0);
    return 0;
}

/* ── cross-cutting: vtable layout for the other back ends ──────────────────── */

static void vt_clear(const Nv2aSurface *s, const Nv2aRenderState *rs,
                     uint32_t flags, uint32_t argb, uint32_t zstencil)
{ (void)s; (void)rs; (void)flags; (void)argb; (void)zstencil; }
static void vt_draw(const Nv2aSurface *s, const Nv2aBatch *b) { (void)s; (void)b; }
static void vt_flip(void) {}
static int vt_stopped(void) { return 0; }

static int case_vtable_legacy_init(void)
{
    /* A five-field initializer, exactly the shape the existing CPU/back-end
     * fixtures use. The new optional coverage_report must remain LAST and must
     * default to NULL; nothing may shift the older entries. */
    static const Nv2aBackend legacy = {
        vt_clear, vt_draw, vt_flip, NULL, vt_stopped
    };
    static const Nv2aBackend zeroed = { 0 };
    chk(legacy.clear == vt_clear, "legacy clear slot");
    chk(legacy.draw == vt_draw, "legacy draw slot");
    chk(legacy.flip == vt_flip, "legacy flip slot");
    chk(legacy.draw_program == NULL, "legacy draw_program slot");
    chk(legacy.stopped == vt_stopped, "legacy stopped slot");
    chk(legacy.coverage_report == NULL,
        "coverage_report is optional and defaults to NULL");
    chk(zeroed.clear == NULL && zeroed.coverage_report == NULL,
        "a zero initializer has no coverage hook");
    chk(offsetof(Nv2aBackend, coverage_report) > offsetof(Nv2aBackend, stopped),
        "coverage_report remains the last vtable field");
    return 0;
}

/* ── Case table ─────────────────────────────────────────────────────────────── */

typedef struct { const char *name; int (*fn)(void); } Case;

static const Case g_cases[] = {
    { "ff_neither", case_ff_neither },
    { "ff_texgen_only", case_ff_texgen_only },
    { "ff_texgen_zero_seen", case_ff_texgen_zero_seen },
    { "ff_texmat_only", case_ff_texmat_only },
    { "ff_both_stage_assoc", case_ff_both_stage_assoc },
    { "ff_stage1_only", case_ff_stage1_only },
    { "ff_stage2_enable_no_matrix", case_ff_stage2_enable_no_matrix },
    { "ff_stage3_enable_only", case_ff_stage3_enable_only },
    { "ff_stage3_texgen_only", case_ff_stage3_texgen_only },
    { "stage6_isolated", case_stage6_isolated },
    { "stage7_isolated", case_stage7_isolated },
    { "stage6_7_multi", case_stage6_7_multi },
    { "stage_batch_vs_incidence", case_stage_batch_vs_incidence },
    { "stage_program_path_count", case_stage_program_path_count },
    { "stage_first_retained", case_stage_first_retained },
    { "lit_nonqualifying", case_lit_nonqualifying },
    { "lit_specular_only", case_lit_specular_only },
    { "lit_spot_only", case_lit_spot_only },
    { "lit_both_and_immutable", case_lit_both_and_immutable },
    { "first_records_immutable", case_first_records_immutable },
    { "report_retained", case_report_retained },
    { "vp_port_0b00", case_vp_port_0b00 },
    { "vp_port_0b10", case_vp_port_0b10 },
    { "vp_port_0b70", case_vp_port_0b70 },
    { "vp_cursor_advance", case_vp_cursor_advance },
    { "vp_component_unavailable", case_vp_component_unavailable },
    { "vp_zero_observed", case_vp_zero_observed },
    { "vp_upload_past_end", case_vp_upload_past_end },
    { "vp_start_out_of_range", case_vp_start_out_of_range },
    { "gate_off", case_gate_off },
    { "gate_first_sample_off_wins", case_gate_first_sample_off_wins },
    { "gate_first_sample_on_wins", case_gate_first_sample_on_wins },
    { "gate_on_identity", case_gate_on_identity },
    { "subchannel_guard", case_subchannel_guard },
    { "rejected_stream_rollback", case_rejected_stream_rollback },
    { "guards_stopped", case_guards_stopped },
    { "guards_sub_minimum", case_guards_sub_minimum },
    { "entry_forms", case_entry_forms },
    { "vtable_legacy_init", case_vtable_legacy_init },
};

int main(int argc, char **argv)
{
    size_t i;
    int rc;

    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];
    for (i = 0; i < sizeof g_cases / sizeof g_cases[0]; i++) {
        if (strcmp(g_cases[i].name, argv[2]) != 0)
            continue;
        rc = g_cases[i].fn();
        if (rc != 0) {
            fprintf(stderr, "FAIL pbcov_stream[%s]: fixture/harness error\n", g_case);
            return rc;
        }
        if (g_failures) {
            fprintf(stderr, "FAIL pbcov_stream[%s]: %d assertion%s failed\n", g_case,
                    g_failures, g_failures == 1 ? "" : "s");
            return 1;
        }
        printf("PASS pbcov_stream[%s]\n", g_case);
        return 0;
    }
    fprintf(stderr, "FAIL pbcov_stream: unknown case '%s'\n", argv[2]);
    return 2;
}
