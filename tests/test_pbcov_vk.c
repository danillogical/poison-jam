/* Independent deciding gate for the D4 Vulkan rc-refusal coverage
 * (RECOMP_PB_COVERAGE=1), real-device half.
 *
 * Authored by a worker distinct from the production change. This translation
 * unit #includes the toolkit's src/video/nv2a_vk_backend.c with XBR_VK_BACKEND
 * defined, so `nv2a_vk_backend_install`, `vb_draw`, the file-static `g` and the
 * production vtable are the REAL bodies. It installs on the configured device
 * (the CTest entry provides the software ICD and RECOMP_VK_DEVICE=llvmpipe) and
 * drives the production draw/fallback site; the counters it reads are the
 * production ledger `nv2a_vk_rc_coverage()` and `g.st.rc_fallback`.
 *
 * A refusal must be decided by the production wiring, not by a helper-only
 * copy: the cases below call `vb_draw` and assert that its own `rc_fallback`
 * counter equals the ledger's total, that the two reason counters sum to it
 * exclusively, and that the first refusal is frozen.
 *
 * If the CTest environment names an ICD, a failed install is a FAILURE, not a
 * skip: the software device was promised to this case. Only a build with no
 * ICD at all may skip (77), and that is the registration's choice, not this
 * case's.
 *
 * Inherently unobservable in one process, and therefore not claimed by a
 * "racy first sample" mutant: the interleaving of `first_valid` with a
 * concurrent reader. The observable equivalent asserted here is that the first
 * record's bytes never change once claimed, including across later refusals
 * and repeated reports.
 */
#define XBR_VK_BACKEND 1
#include "nv2a_vk_backend.c"                    /* the production bodies */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <process.h>

#define ARENA_BYTES 0x00080000u
#define SURF_VA     0x00010000u
#define SURF_W      64u
#define SURF_H      64u
#define SURF_PITCH  (SURF_W * 4u)
#define TEX_VA      0x00020000u

static uint8_t *g_arena;
static int g_failures;
static const char *g_case = "?";

/* The back end calls this to register its vtable; the stub records it so the
 * case can prove the production six-field table is the one installed. */
static const Nv2aBackend *g_registered;

static void chk_impl(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    if (cond)
        return;
    g_failures++;
    fprintf(stderr, "FAIL pbcov_vk[%s]: %s", g_case, what);
    if (fmt && *fmt) {
        fprintf(stderr, " -- ");
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
}

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

/* Capture one report call's stderr in a private temp file and restore fd 2. */
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

/* ── Fixture ────────────────────────────────────────────────────────────────── */

static Nv2aVertex g_v[6];
static Nv2aVertexExtra g_x[6];
static Nv2aTexture g_cube[4];
static Nv2aCombiner g_rc;
static Nv2aRenderState g_rs;
static Nv2aSurface g_surf;
static Nv2aBatch g_b;

static void fixture_init(void)
{
    static const float cp[6][2] = {
        { 8.0f, 8.0f }, { 56.0f, 8.0f }, { 8.0f, 56.0f },
        { 8.0f, 56.0f }, { 56.0f, 8.0f }, { 56.0f, 56.0f }
    };
    int i;

    g_arena = (uint8_t *)calloc(1, ARENA_BYTES);

    for (i = 0; i < 6; i++) {
        memset(&g_v[i], 0, sizeof g_v[i]);
        g_v[i].x = cp[i][0];
        g_v[i].y = cp[i][1];
        g_v[i].z = 0.5f;
        g_v[i].rhw = 1.0f;
        g_v[i].diffuse = 0xFF204060u;
        g_v[i].u = cp[i][0];
        g_v[i].v = cp[i][1];
        memset(&g_x[i], 0, sizeof g_x[i]);
        g_x[i].specular = 0xFF808080u;
        g_x[i].fog = 1.0f;
    }

    memset(&g_rs, 0, sizeof g_rs);
    g_rs.color_mask = 0x01010101u;
    g_rs.blend_eq = 0x8006u;
    g_rs.blend_src = 1u;
    g_rs.blend_dst = 0u;
    g_rs.alpha_func = 0x207u;
    g_rs.depth_func = 0x207u;
    g_rs.stencil_func = 0x207u;
    g_rs.stencil_write_mask = g_rs.stencil_read_mask = 0xFFu;
    g_rs.cull_face = 0x405u;
    g_rs.front_face = 0x900u;
    g_rs.depth_min = 0.0f;
    g_rs.depth_max = 16777215.0f;

    memset(&g_surf, 0, sizeof g_surf);
    g_surf.color_va = SURF_VA;
    g_surf.width = SURF_W;
    g_surf.height = SURF_H;
    g_surf.pitch = SURF_PITCH;
    g_surf.bytes_per_pixel = 4u;
    g_surf.color_format = 8u;
    g_surf.aa_sx = g_surf.aa_sy = 1u;

    memset(&g_rc, 0, sizeof g_rc);
    g_rc.control = 1u;                           /* one combiner stage */

    memset(g_cube, 0, sizeof g_cube);
    for (i = 0; i < 4; i++) {
        g_cube[i].offset = TEX_VA;
        g_cube[i].width = 8u;
        g_cube[i].height = 8u;
        g_cube[i].pitch = 32u;
        g_cube[i].color = 0x12u;
        g_cube[i].addr_u = g_cube[i].addr_v = 3u;
        g_cube[i].linear = 1u;
        g_cube[i].cube = 1u;
    }
}

/* Draw one rc batch through the production `vb_draw`. `program` is the raw
 * SET_SHADER_STAGE_PROGRAM word; `cube_bits` marks the stages whose texture is
 * a cube texture. Untextured otherwise. */
static void draw_rc(uint32_t program, uint32_t cube_bits)
{
    int st;

    memset(&g_b, 0, sizeof g_b);
    g_b.vertices = g_v;
    g_b.count = 6u;
    g_b.texture = NULL;
    g_b.state = &g_rs;
    g_b.extra = g_x;
    for (st = 0; st < 4; st++)
        g_b.textures[st] = (cube_bits & (1u << st)) ? &g_cube[st] : NULL;
    g_rc.stage_program = program;
    g_b.combiner = &g_rc;
    g_b.fog_color = 0u;
    g_b.clip_plane_mode = 0u;
    vb_draw(&g_surf, &g_b);
}

static int refused(uint32_t program, uint32_t cube_bits)
{
    uint64_t before = nv2a_vk_rc_coverage()->total;
    draw_rc(program, cube_bits);
    return nv2a_vk_rc_coverage()->total > before;
}

/* ── Cases ─────────────────────────────────────────────────────────────────── */

static int case_rc_unsupported_modes(void)
{
    const Nv2aVkRcCoverage *c;
    Nv2aVkRcCoverage first;

    draw_rc(6u, 0u);                             /* stage 0 mode 6 */
    c = nv2a_vk_rc_coverage();
    chk_u64("one fallback in the production counter", g.st.rc_fallback, 1u);
    chk_u64("ledger total equals the production counter", c->total,
            (uint64_t)g.st.rc_fallback);
    chk_u64("mode>5 reason counted", c->mode_gt5, 1u);
    chk_u64("cube reason not counted", c->cube_mode3, 0u);
    chk_u32("first reason", c->first_reason, 1u);
    chk_u32("first stage", c->first_stage, 0u);
    chk_u32("first mode", c->first_mode, 6u);
    chk_u32("first raw stage_program", c->first_stage_program, 6u);
    chk_u32("first cube mask", c->first_cube_mask, 0u);
    first = *c;

    draw_rc(7u << 15, 0u);                       /* stage 3 mode 7 */
    c = nv2a_vk_rc_coverage();
    chk_u64("both fallbacks in the production counter", g.st.rc_fallback, 2u);
    chk_u64("ledger total follows", c->total, 2u);
    chk_u64("both refusals are mode>5", c->mode_gt5, 2u);
    chk_u64("reason counters are exclusive", c->total,
            c->mode_gt5 + c->cube_mode3);
    chk_u32("first record is immutable", c->first_stage, first.first_stage);
    chk_u32("first record word is immutable", c->first_stage_program,
            first.first_stage_program);
    chk_u32("first record validity is immutable", c->first_valid, 1u);
    return 0;
}

static int case_rc_cube_mode3(void)
{
    const Nv2aVkRcCoverage *c;

    /* Mode 3 with a true cube texture is the second refusal reason. */
    draw_rc(3u << 5, 1u << 1);                   /* stage 1 mode 3 + cube */
    c = nv2a_vk_rc_coverage();
    chk_u64("fallback counted", g.st.rc_fallback, 1u);
    chk_u64("ledger total", c->total, 1u);
    chk_u64("cube reason counted", c->cube_mode3, 1u);
    chk_u64("mode>5 reason not counted", c->mode_gt5, 0u);
    chk_u32("first reason is the cube refusal", c->first_reason, 2u);
    chk_u32("first stage", c->first_stage, 1u);
    chk_u32("first mode", c->first_mode, 3u);
    chk_u32("first cube mask", c->first_cube_mask, 1u << 1);

    /* Mode 3 WITHOUT a cube texture is supported: no refusal, no count. */
    draw_rc(3u << 5, 0u);
    c = nv2a_vk_rc_coverage();
    chk_u64("supported mode 3 does not add a fallback", g.st.rc_fallback, 1u);
    chk_u64("supported mode 3 does not add to the ledger", c->total, 1u);
    chk_u32("first refusal still the cube one", c->first_reason, 2u);
    chk(g.st.rc_draws >= 1u, "the supported batch took the rc pipeline",
        "rc_draws %llu", (unsigned long long)g.st.rc_draws);
    return 0;
}

static int case_rc_supported_modes(void)
{
    const Nv2aVkRcCoverage *c;
    uint64_t draws_at_start = g.st.rc_draws;
    uint32_t stage, mode;
    char what[96];

    /* Every 5-bit mode 0..5 is supported at every stage 0..3, with no cube
     * texture. Each draw carries exactly one mode, in exactly one stage (the
     * other three stages are 0), so a boundary that refuses mode 5 anywhere
     * cannot hide behind another stage's supported modes. */
    for (stage = 0; stage < 4; stage++) {
        for (mode = 0; mode <= 5; mode++) {
            uint32_t program = mode << (stage * 5);
            uint64_t before = g.st.rc_draws;

            draw_rc(program, 0u);
            _snprintf(what, sizeof what,
                      "supported mode %u stage %u draws without fallback",
                      (unsigned)mode, (unsigned)stage);
            what[sizeof what - 1] = '\0';
            chk(g.st.rc_draws == before + 1u, what, "rc_draws %llu -> %llu",
                (unsigned long long)before,
                (unsigned long long)g.st.rc_draws);
            c = nv2a_vk_rc_coverage();
            chk_u64("supported sweep: no fallback", g.st.rc_fallback, 0u);
            chk_u64("supported sweep: ledger total 0", c->total, 0u);
            chk_u64("supported sweep: reason counters 0",
                    c->mode_gt5 + c->cube_mode3, 0u);
            chk_u32("supported sweep: no first refusal", c->first_valid, 0u);
        }
    }
    chk(g.st.rc_draws == draws_at_start + 24u,
        "the sweep took the rc pipeline exactly 24 times",
        "rc_draws %llu -> %llu", (unsigned long long)draws_at_start,
        (unsigned long long)g.st.rc_draws);
    return 0;
}

static int case_rc_priority(void)
{
    const Nv2aVkRcCoverage *c;

    /* Batch A: stage 0 mode 3 with a cube refuses as the cube reason before the
     * scan can reach stage 1's mode 7 (stages are scanned 0..3). */
    draw_rc(3u | (7u << 5), 1u);                 /* stage0 cube, stage1 mode7 */
    c = nv2a_vk_rc_coverage();
    chk_u64("first batch refused", c->total, 1u);
    chk_u32("stage order: earlier cube beats a later mode>5",
            c->first_reason, 2u);
    chk_u32("refusing stage is stage 0", c->first_stage, 0u);
    chk_u64("cube reason", c->cube_mode3, 1u);
    chk_u64("mode reason still zero", c->mode_gt5, 0u);

    /* Batch B: at one stage, mode > 5 is tested before the cube bit. */
    draw_rc(6u | (3u << 5), 1u);                 /* stage0 mode6 + cube */
    c = nv2a_vk_rc_coverage();
    chk_u64("second batch refused", c->total, 2u);
    chk_u64("per-stage order: mode>5 is checked before cube", c->mode_gt5, 1u);
    chk_u64("reasons stay exclusive", c->total, c->mode_gt5 + c->cube_mode3);
    chk_u32("first refusal remains batch A", c->first_reason, 2u);
    chk_u32("first refusing stage remains 0", c->first_stage, 0u);
    return 0;
}

static int case_rc_first_report(void)
{
    const Nv2aVkRcCoverage *c;
    Nv2aVkRcCoverage before, after_first, after_second;

    draw_rc(6u, 0u);                             /* mode > 5 */
    draw_rc(3u << 10, 1u << 2);                  /* cube-mode 3 */
    draw_rc(7u << 15, 0u);                       /* another mode > 5 */
    c = nv2a_vk_rc_coverage();
    chk_u64("three refusals", c->total, 3u);
    chk_u64("production counter follows", (uint64_t)g.st.rc_fallback, c->total);
    chk_u64("reason counters are exclusive and complete", c->total,
            c->mode_gt5 + c->cube_mode3);
    chk_u64("mode count", c->mode_gt5, 2u);
    chk_u64("cube count", c->cube_mode3, 1u);
    chk_u32("first refusal is the first batch", c->first_reason, 1u);
    chk_u32("first stage", c->first_stage, 0u);
    chk_u32("first mode", c->first_mode, 6u);
    before = *c;

    {
        char *log = NULL;
        chk(capture_begin("vk-on"), "stderr capture opened");
        if (g_cap_saved >= 0)
            nv2a_vk_rc_coverage_report();
        log = capture_end();
        chk(log != NULL && strstr(log, "[PBCOV]") != NULL,
            "gate on: report prints a [PBCOV] line", "captured %u bytes",
            (unsigned)(log ? strlen(log) : 0));
        free(log);
    }
    after_first = *nv2a_vk_rc_coverage();
    nv2a_vk_rc_coverage_report();
    after_second = *nv2a_vk_rc_coverage();
    chk(memcmp(&after_first, &before, sizeof before) == 0,
        "the report does not reset or modify the ledger (first call)");
    chk(memcmp(&after_second, &before, sizeof before) == 0,
        "the report does not reset or modify the ledger (second call)");
    chk_u32("reports do not clear the first record", after_second.first_valid, 1u);
    return 0;
}

static int case_rc_gate_off(void)
{
    const Nv2aVkRcCoverage *c;
    const char *v = getenv("RECOMP_PB_COVERAGE");

    chk(!(v && v[0] == '1' && v[1] == '\0'),
        "CTest env is not the exact-1 gate", "got '%s'", v ? v : "(unset)");
    draw_rc(6u, 0u);
    c = nv2a_vk_rc_coverage();
    /* The production refusal still happens and is still counted by the back
     * end's own aggregate; only the diagnostic ledger stays untouched. */
    chk_u64("the fallback still happens with coverage off", g.st.rc_fallback, 1u);
    chk_u64("gate off: ledger total stays 0", c->total, 0u);
    chk_u64("gate off: reasons stay 0", c->mode_gt5 + c->cube_mode3, 0u);
    chk_u32("gate off: no first record", c->first_valid, 0u);
    chk_u32("gate off: identity magic", c->magic, NV2A_VK_RC_COVERAGE_MAGIC);
    chk_u32("gate off: identity version", c->version, NV2A_VK_RC_COVERAGE_VERSION);
    chk_u32("gate off: identity size", c->size, (uint32_t)sizeof(Nv2aVkRcCoverage));
    {
        char *log = NULL;
        /* The only production caller of this callback is nv2a_pb_exec_report(),
         * and that call is inside its pb_coverage() gate. This captures the
         * production call condition exactly: off means the callback is NOT
         * invoked, so the diagnostic path emits nothing. The callback itself
         * is unconditional when called (decided below, on the on-path case);
         * the stream gate also proves the executor skips a registered
         * coverage_report hook while off. */
        chk(capture_begin("vk-off"), "stderr capture opened");
        if (g_cap_saved >= 0 && nv2a_vk_rc_coverage_enabled())
            nv2a_vk_rc_coverage_report();
        log = capture_end();
        chk(log != NULL, "gate off: report output was captured");
        chk(log != NULL && strstr(log, "[PBCOV]") == NULL,
            "gate off: the gated report path prints no [PBCOV] line",
            "captured %u bytes", (unsigned)(log ? strlen(log) : 0));
        free(log);
    }
    chk_u64("gate off: report does not enable counting", c->total, 0);
    return 0;
}

static int case_rc_first_sample_on_wins(void)
{
    const Nv2aVkRcCoverage *c;

    _putenv_s("RECOMP_PB_COVERAGE", "1");
    chk(nv2a_vk_rc_coverage_enabled() == 1, "the first sample is on");
    _putenv_s("RECOMP_PB_COVERAGE", "0");         /* too late */
    draw_rc(6u, 0u);
    c = nv2a_vk_rc_coverage();
    chk_u64("the first sample cannot be flipped off", c->total, 1u);
    chk_u32("and it still claims its first record", c->first_valid, 1u);
    return 0;
}

static int case_rc_first_sample_off_wins(void)
{
    const Nv2aVkRcCoverage *c;

    _putenv_s("RECOMP_PB_COVERAGE", "0");
    chk(nv2a_vk_rc_coverage_enabled() == 0, "the first sample is off");
    _putenv_s("RECOMP_PB_COVERAGE", "1");         /* too late */
    draw_rc(6u, 0u);
    c = nv2a_vk_rc_coverage();
    chk_u64("the first sample cannot be flipped on", c->total, 0u);
    chk_u32("and no record is claimed", c->first_valid, 0u);
    chk_u64("the renderer still refused and fell back", g.st.rc_fallback, 1u);
    return 0;
}

static int case_rc_vtable(void)
{
    /* The production vtable's layout and wiring, not a fixture's copy. */
    chk(g_registered == &s_vk_backend,
        "install registered the production back end");
    chk(s_vk_backend.clear == vb_clear, "clear slot is the production body");
    chk(s_vk_backend.draw == vb_draw, "draw slot is the production body");
    chk(s_vk_backend.flip == vb_flip, "flip slot is the production body");
    chk(s_vk_backend.draw_program == vb_draw_program,
        "draw_program slot is the production body");
    chk(s_vk_backend.stopped == vb_stopped, "stopped slot is the production body");
    chk(s_vk_backend.coverage_report == nv2a_vk_rc_coverage_report,
        "coverage_report is the production refusal report");
    chk(offsetof(Nv2aBackend, coverage_report) > offsetof(Nv2aBackend, stopped),
        "coverage_report remains the last vtable field");
    return 0;
}

/* ── Case table and the run ─────────────────────────────────────────────────── */

typedef struct { const char *name; int (*fn)(void); } Case;

static const Case g_cases[] = {
    { "rc_unsupported_modes", case_rc_unsupported_modes },
    { "rc_cube_mode3", case_rc_cube_mode3 },
    { "rc_supported_modes", case_rc_supported_modes },
    { "rc_priority", case_rc_priority },
    { "rc_first_report", case_rc_first_report },
    { "rc_gate_off", case_rc_gate_off },
    { "rc_first_sample_on_wins", case_rc_first_sample_on_wins },
    { "rc_first_sample_off_wins", case_rc_first_sample_off_wins },
    { "rc_vtable", case_rc_vtable },
};

int main(int argc, char **argv)
{
    const char *icd_files, *icd_names;
    size_t i;
    int rc;

    if (argc > 2 && strcmp(argv[1], "--skip") == 0) {
        printf("SKIP: pbcov_vk -- %s\n", argv[2]);
        return 77;
    }
    if (argc != 3 || strcmp(argv[1], "--case") != 0) {
        fprintf(stderr, "usage: %s --case <name>\n", argv[0]);
        return 2;
    }
    g_case = argv[2];

    printf("pbcov_vk: D4 rc-refusal coverage on the real Vulkan back end\n");
    fixture_init();
    if (!g_arena) {
        fprintf(stderr, "FAIL pbcov_vk[%s]: fixture allocation\n", g_case);
        return 1;
    }
    /* The pipeline cache is irrelevant to this case; a CTest entry also turns
     * it off. Naming it here keeps a hand-run from writing next to the tree. */
    _putenv_s("RECOMP_VK_PIPELINE_CACHE", "off");
    rc = nv2a_vk_backend_install();
    if (rc != 0) {
        icd_files = getenv("VK_DRIVER_FILES");
        icd_names = getenv("VK_ICD_FILENAMES");
        if ((icd_files && *icd_files) || (icd_names && *icd_names)) {
            /* The environment promised a device (the software ICD); a skip
             * would hide that promise being broken. */
            fprintf(stderr, "FAIL pbcov_vk[%s]: the software ICD named by "
                    "VK_DRIVER_FILES/VK_ICD_FILENAMES could not be installed\n", g_case);
            return 1;
        }
        printf("SKIP: pbcov_vk -- no Vulkan device and no ICD was configured\n");
        return 77;
    }

    for (i = 0; i < sizeof g_cases / sizeof g_cases[0]; i++) {
        if (strcmp(g_cases[i].name, g_case) != 0)
            continue;
        rc = g_cases[i].fn();
        if (rc != 0) {
            fprintf(stderr, "FAIL pbcov_vk[%s]: fixture/harness error\n", g_case);
            return rc;
        }
        if (g_failures) {
            fprintf(stderr, "FAIL pbcov_vk[%s]: %d assertion%s failed\n", g_case,
                    g_failures, g_failures == 1 ? "" : "s");
            return 1;
        }
        printf("PASS pbcov_vk[%s]\n", g_case);
        return 0;
    }
    fprintf(stderr, "FAIL pbcov_vk: unknown case '%s'\n", argv[2]);
    return 2;
}

/* ── The five toolkit symbols this translation unit does not link ───────────── */

ptrdiff_t xbox_GetMemoryOffset(void)
{
    return (ptrdiff_t)(uintptr_t)g_arena;
}

void nv2a_backend_register(const Nv2aBackend *backend)
{
    g_registered = backend;
}

int nv2a_backend_decode_texture(const Nv2aTexture *tex, uint32_t *argb_out)
{
    uint32_t x, y;
    if (!tex || !argb_out || !tex->width || !tex->height)
        return 0;
    for (y = 0; y < tex->height; y++)
        for (x = 0; x < tex->width; x++)
            argb_out[(size_t)y * tex->width + x] = 0xFF000000u
                | ((x * 31u) & 0xFFu) << 16
                | ((y * 47u) & 0xFFu) << 8
                | ((x + y) & 0xFFu);
    return 1;
}

uint32_t nv2a_backend_texture_span(const Nv2aTexture *tex)
{
    if (!tex)
        return 0;
    return tex->pitch * tex->height;
}

void nv2a_rc_unpack(uint32_t argb, float out[4])
{
    out[0] = (float)((argb >> 16) & 0xFFu) / 255.0f;
    out[1] = (float)((argb >> 8) & 0xFFu) / 255.0f;
    out[2] = (float)(argb & 0xFFu) / 255.0f;
    out[3] = (float)((argb >> 24) & 0xFFu) / 255.0f;
}
