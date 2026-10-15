/* V6: the vertex-program conformance gate (plan-jsrf-gameplay.md, step V6).
 *
 * This is a SEPARATE gate from V5's (tests/test_vsh_aot_programs.c). V5 asks whether the
 * generated retail corpus and the back end's lookup are self-consistent; V6 asks whether the
 * offline translation pipeline and the corpus seam reproduce the *interpreter* for the
 * synthetic V1..V7 taxonomy, on the bytes and on the pixels. It shares no fixture code with V5:
 * the V1..V7 programs come from the parity test's own encoder.
 *
 * FIXTURES. The V1..V7 taxonomy is not re-invented here. tests/test_gpu_backend_parity.c
 * carries a self-contained vertex-program encoder and the six case programs between its
 * `VSH-ENCODER-BEGIN` and `VSH-ENCODER-END` markers. This file does NOT copy that block:
 * CMakeLists.txt extracts the region verbatim at configure time (re-extracting whenever the
 * parity test changes, via CMAKE_CONFIGURE_DEPENDS) into
 * <build>/vsh-conformance/vsh_encoder_region.inc, and this file `#include`s it. The encoder
 * therefore cannot drift from the one the parity test uses -- the copy is the same bytes by
 * construction, and a change on either side re-extracts.
 *
 * THE FIVE CHECKS.
 *   1. BYTE EQUALITY. For every case V1..V6, for V7 (V1's words under RECOMP_GPU_VSH=0), and for
 *      three extra fixtures whose implied masks are sparse and two of which reach above bit 7,
 *      the host tool `nv2a_vsh_hlsl.exe --bin <words> --mask <m> --out <f>` must write bytes
 *      identical to calling `nv2a_vsh_hlsl(words, len, m, ...)` directly. A single differing
 *      byte fails with its offset. The same runs assert the tool's contract, so "byte-identical"
 *      cannot be satisfied by both outputs being empty or both being annotated: the plain form
 *      carries no `[[vk::` at all; the `--vk` form carries exactly two `[[vk::binding(`
 *      decorations (9 and 10), one `[[vk::location(n)]]` per mask bit n at n's own bit number,
 *      and `SV_ClipDistance0`; and a mask the instructions do not imply is refused with exit 3
 *      (which is what makes the accepted `--mask` evidence about the program's real input mask).
 *      The seven output varyings are asserted BY NAME AND LOCATION, not merely counted: the count
 *      cannot see a varying moved to another location, and the fixtures' combiner reads d0 alone,
 *      so a moved t1 would leave every pixel identical and pass everything else here (the
 *      verifier's m9). The locations are the combiner's contract, src/video/shaders/nv2a_vk_rc.hlsl's
 *      VO: d0->0, d1->1, fog->2, t0->3, t1->4, t2->5, t3->6, plus `SV_Position`. Negative control 6
 *      feeds this check a moved t1 and must fail.
 *   2. A TABLE HIT MATCHES THE CPU INTERPRETER'S PIXELS. A synthetic corpus is built from the
 *      fixtures' own words: every identity field is recomputed at run time with
 *      nv2a_vsh_program_info and nv2a_vsh_aot_digest, and the SPIR-V was compiled at BUILD time
 *      from those same words by the same toolchain the retail corpus uses. What ties the two
 *      together is (a) the generation step -- the generator writes each fixture's words to a
 *      file and hands that file to the tool -- and (b) two hash rechecks: the header's
 *      words_bytes_hash against the words this run uploads, and its spirv_hash against the bytes
 *      of the blob the header carries. Those rechecks catch a hand-edited or half-regenerated
 *      header, and the pixels below catch a shader that is legal but wrong; they do NOT prove by
 *      themselves that the blob came from those words, and this test does not claim they do.
 *      Each fixture's batch is drawn through the real executor with the Vulkan back end and
 *      compared with a CPU-interpreter reference of the SAME batch, captured before the back end
 *      was installed. Every V1..V6 case is a hit, V2's a0-relative indexing included; two table
 *      entries share one input_mask and each is compared with its own reference. The seven
 *      fixtures X4..X10 are hits too, and exist for MAC-op coverage: an independent verifier found
 *      that no V case, not the miss program and not the first of these emitted MAC op 3 (`add`) or
 *      ops 5 (dp3), 6 (dph), 9 (min), 10 (max), 11 (slt) and 12 (sge), so a translator that
 *      mis-emitted any of them changed no compared pixel and passed the whole gate. Each of X4..X10
 *      draws one such op and its pixels depend on that op's own semantics: the dot-product ops take
 *      their second operand from a CONSTANT (the encoder shares one input index across all three
 *      sources, so an instruction can read only one vN -- a limit of the encoder, not the ISA), and
 *      the operands are chosen so the result is not any single component and the comparisons flip
 *      across the triangle, rather than collapsing to a plain mov.
 *   3. A MISS FALLS BACK TO THE CPU. A fixture of its own -- `mov oPos, v0 ; mov oD0, c[150]
 *      FINAL`, which is not one of the V1..V6 cases -- is deliberately left out of the table,
 *      verified absent by hash, length and digest and verified different from every V1..V6
 *      program. Its draw must advance `misses` and not `hits`, latch the miss identity, leave the
 *      interpreter's pixels on the surface (compared against a reference captured the same way,
 *      not merely "not the clear colour"), and be reported once per hash on stderr.
 *   4. A CORRUPT IDENTITY IS REJECTED. A copy of the table has one entry's `len` corrupted and,
 *      separately, another entry's `digest`, so the hash still matches each time. Each draw must
 *      advance `identity_fail`, advance neither `hits` nor `misses`, leave the interpreter's
 *      pixels on the surface, and be reported once per hash with the reason. The table under
 *      test is then handed back and the positive control re-asserted.
 *      ACCEPTED LIMITATION (the verifier's m4a/m4c): the back end's identity check has TWO digest
 *      terms -- the table entry's own digest against its words, and the same digest against the
 *      guest's words (nv2a_vk_backend.c). Removing only ONE of them is a false pass, because the
 *      sibling term still rejects the corruption; removing BOTH is caught (m4b). So this check
 *      needs both terms gone to see a defect, and that is what it asserts against, not a claim
 *      that either term alone is load-bearing.
 *   5. MUTATION READINESS. Every predicate is separately callable and `--negative-control N` feeds
 *      one a real, injected defect through the real back end and reports the CHECK's own verdict as
 *      its exit status: 1 when the check correctly FAILED the injected defect -- the load-bearing
 *      result -- and 2 when it passed it. 1: a one-byte difference in the oracle (check 1's byte
 *      comparison). 2: an empty corpus (the hit predicate). 3: a corpus that carries the program
 *      that must miss (the miss predicate). 4: an intact identity where a corrupt one was expected
 *      (the identity predicate). 5: V3's table entry pointed at V5's SPIR-V, which shares its
 *      input_mask, so the identity check still passes and only the pixels are wrong (check_cpu,
 *      the predicate behind checks 2, 3 and 4). 6: the --vk output with one output varying moved
 *      to a different location (check 1's output-location assertion; no device needed).
 *      ACCEPTED LIMITATIONS, with the verifier's evidence, recorded rather than chased:
 *        - m4a/m4c (check 4): see item 4 above -- only removing BOTH digest terms is caught.
 *        - f2 (fog): a mutant that writes fog param 0.0f is value-equivalent to the real path
 *          (0.0f is already the parameter, and equals the post-clamp result of 1.0f); 2.0f IS
 *          caught. Not a hole.
 *        - f3/f4 (depth test/write): every batch here has depth test and write off and no fixture
 *          sets SET_FOG_MODE, so those mutated paths are unreachable by design in this fixture set.
 *      The report names, per check, the production change each would catch.
 *
 * PIXEL RULE (checks 2, 3 and 4). Built on the rule tests/test_vsh_aot_programs.c uses, which took
 * it from the parity tests, with two deliberate changes:
 *   - ALL FOUR channels are compared, not three. The surface is A8R8G8B8 and V4's dst/exp/log
 *     results reach it only through oD0.w, so an RGB-only comparison left V4's alpha half
 *     unchecked. The worst per-channel difference is reported as rgb and alpha separately.
 *     Coverage is still judged on RGB only: the clear colour is opaque, so including alpha in
 *     "was this pixel drawn" would let an alpha-only difference masquerade as geometry.
 *   - Coverage is bounded by max(4, perimeter/4) rather than by the whole perimeter. Measured on
 *     lavapipe, every V1..V6 case has xor 0 and the miss program has xor 4 (its triangle's top
 *     edge, 4 pixels the CPU reference drew and the second CPU draw left clear -- a boundary
 *     effect inside the band, named in the output), so the old perimeter band (94..173) was slack
 *     these fixtures never used, and it was loose enough to pass a whole-pixel geometric error: a
 *     probe measured V1 shifted 1 px in x -> xor 78 and 1 px in y -> xor 68, both of which the
 *     perimeter band accepts (112) and the new bound rejects (28). The floor of 4 keeps V1's small
 *     triangle (perimeter 112) from being judged on a single pixel of rounding, and is exactly the
 *     miss's measured 4.
 * Unchanged: at least 98% of the 4096 pixels within TOL_VSH = 4, which is the parity tests'
 * established value and the binding constraint; a pixel one side drew and the other did not is a
 * mismatch unless it lies in the boundary band (the interpreter's OWN perimeter --
 * vs_predict_visible for the behind-the-eye case, the projected polygon's otherwise -- plus 2% of
 * the drawn region); at least 500 pixels must differ from the clear colour, so a surface nobody
 * drew cannot "agree" with a reference nobody drew; and the reference's coverage must be within
 * 10% of the area the interpreter's own geometry predicts, so the fixture is known to be the shape
 * the check assumes.
 *
 * TOLERANCE. 4 per channel is the parity tests' TOL_VSH, the measured bound for exactly this
 * render setup: the GPU's rcp/rcc/exp2/log2/pow and its fused mad are approximate to a few ULP of
 * the interpreter's float maths, and colour interpolation adds its own rounding. The worst channel
 * difference actually seen is printed per case.
 *
 * HONESTY. Every counter assertion reads nv2a_vk_backend_program_stats; every pixel assertion
 * reads surface bytes from guest memory; the only log assertions are "the miss and the identity
 * failure are not silent" and "the back end was registered", as substring matches against this
 * process's own captured stderr. Exit 0 only when every check passed; a crash is a failure. A
 * missing Vulkan device is a hard failure when a check already failed, and otherwise an explicit
 * skip (77) that CTest shows as Not Run -- never a pass.
 *
 * HOW IT RUNS. The host tool's path comes from the `--tool` argument (CTest passes
 * `$<TARGET_FILE:nv2a_vsh_hlsl>`) or the XBR_VSH_HLSL_TOOL environment variable. The fixtures are
 * compiled to SPIR-V during the build, so a run needs no shader toolchain. The device is chosen
 * with RECOMP_VK_DEVICE=llvmpipe; VK_DRIVER_FILES comes from the cache variable XBR_VK_SW_ICD.
 * Run it non-elevated.
 *
 * RECOMP_GPU_VSH is deliberately NOT set in the main run: =0 makes the executor decline the
 * program path before it ever calls draw_program, which would turn every expected miss and
 * identity failure into a decline. Check 1 sets it for one child process only (the V7 row) and
 * restores the previous value, which main asserts before the executor's first program batch.
 */
#include "kernel/kernel.h"
#include "nv2a_state.h"
#include "nv2a_mmio_hook.h"
#include "kernel/nv2a_backend.h"
#include "kernel/nv2a_vsh_interp.h"
#include "video/nv2a_vsh_aot.h"
#include "video/nv2a_vsh_hlsl.h"
#include "video/nv2a_vk_backend.h"
#include "recomp_types.h"
#include <direct.h>
#include <io.h>
#include <math.h>
#include <process.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* kernel/xbox_memory_layout.h's base address, spelled here to keep that header out of this
 * recomp_types.h translation unit. */
#define XBOX_BASE_ADDRESS 0x00010000u
extern ptrdiff_t xbox_GetMemoryOffset(void);
extern BOOL xbox_MemoryLayoutInit(const void *xbe_data, size_t xbe_size);
extern void xbox_MemoryLayoutShutdown(void);

/* The kernel bridge references the game dispatcher/diagnostics; this focused fixture does not
 * start guest threads, so keep those hooks inert (the same set the parity fixtures define). */
recomp_func_t recomp_lookup(uint32_t xbox_va) { (void)xbox_va; return NULL; }
recomp_func_t recomp_lookup_manual(uint32_t xbox_va) { (void)xbox_va; return NULL; }
void recomp_diag_thread_start(uint32_t start, uint32_t low, uint32_t high)
{ (void)start; (void)low; (void)high; }
void recomp_diag_thread_end(void) {}
void recomp_diag_record(uint32_t kind, uint32_t target, uint32_t site,
                        uint32_t value)
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

/* ── Failure reporting, mirrored to the console and into the captured stderr ──
 * The miss/identity log assertions need this process's own stderr, so stderr is redirected to a
 * file for the whole run. `fail` writes to the original descriptor too, so a failure is still
 * visible where the test is being watched. */
static int g_stderr_saved = -1;

static void say_to(FILE *to, const char *fmt, va_list ap)
{
    char b[2048];
    int n;

    b[sizeof b - 1] = '\0';                 /* _snprintf does not terminate on truncation */
    n = _vsnprintf(b, sizeof b - 1, fmt, ap);
    if (n < 0) n = (int)strlen(b);
    b[n] = '\0';
    if (to == stderr && g_stderr_saved >= 0) _write(g_stderr_saved, b, n);
    fputs(b, to);
    fflush(to);
}

static void fail(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    say_to(stderr, fmt, ap);
    va_end(ap);
}

static void note(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    say_to(stdout, fmt, ap);
    va_end(ap);
}

static int file_exists(const char *path) { FILE *f = fopen(path, "rb"); if (f) { fclose(f); return 1; } return 0; }

static unsigned char *read_file(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    long sz;
    unsigned char *b;

    *n = 0;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    b = (unsigned char *)malloc((size_t)sz + 1u);
    if (b) {
        size_t got = fread(b, 1, (size_t)sz, f);
        *n = got;
        b[got] = '\0';
    }
    fclose(f);
    return b;
}

static int write_file(const char *path, const void *p, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    if (n && fwrite(p, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* ── The extracted V1..V7 fixture encoder ─────────────────────────────────
 * Verbatim from tests/test_gpu_backend_parity.c between its VSH-ENCODER-BEGIN and
 * VSH-ENCODER-END markers, written into the build tree by CMakeLists.txt. Nothing in this file
 * duplicates it, so it cannot drift from the encoder the parity test uses. It needs only
 * <math.h>, <string.h>, <stdio.h> and kernel/nv2a_vsh_interp.h. */
#include "vsh_encoder_region.inc"

/* ── The fixture programs ─────────────────────────────────────────────────
 * The five DISTINCT programs of V1..V6. V6 (VS_BEHIND) is vs_mvp_program with different constants
 * and a vertex behind the eye, so its words ARE V1's words and it has V1's identity: the taxonomy
 * check in main asserts that, and check 2 draws it through V1's table entry. */
static const int k_prog[5] = { VS_MVP, VS_A0, VS_PAIR, VS_SFU, VS_FOG };
static const char *const k_prog_tag[5] = { "V1", "V2", "V3", "V4", "V5" };
static const char *const k_prog_what[5] = {
    "4x dp4 MVP against a general matrix + the D3D screen epilogue (mul/rcc, mad)",
    "arl a0.x, v1.x then mov oD0, c[a0.x + 20] (a0-relative constant)",
    "one instruction: MAC (mul R3) paired with ILU (rcp R1.x) reading the OLD R3",
    "the special-function unit: lit, exp, log, dst",
    "oFog.x = v1.x with the fog enable/colour path",
};
#define NPROG 5

/* V1..V7 as the plan names them, for check 1's rows. V7 is V1's words with RECOMP_GPU_VSH=0, so
 * its program is V1's and it is a row here, not a sixth program. */
typedef struct { const char *tag; int which; int prog; const char *what; } ConfCase;
static const ConfCase k_cases[6] = {
    { "V1", VS_MVP,    0, "MVP + D3D epilogue" },
    { "V2", VS_A0,     1, "a0-relative constant index" },
    { "V3", VS_PAIR,   2, "paired MAC + ILU reading the old R3" },
    { "V4", VS_SFU,    3, "lit / exp / log / dst" },
    { "V5", VS_FOG,    4, "oFog.x with fog enabled" },
    { "V6", VS_BEHIND, 0, "MVP with a vertex behind the eye (w = -0.5)" },
};
#define NCASE 6

/* The miss program is its own fixture (MISS_PROG in the fixture array), NOT one of the V1..V6
 * cases: making V2 the miss used to leave V2's a0-relative indexing undrawn on the GPU, so a
 * defect in `(int)floor(A.x + 0.001)` or `vsh_crel(20 + a0)` passed the whole gate. */
#define MISS_FIX  MISS_PROG

/* The V1..V6 programs AS V6 DRAWS THEM. This is the parity test's vs_make, with ONE deliberate
 * difference: V2's a0 inputs. The parity test feeds V2 the exact integers 2, 0 and 3, and for those
 * (int)floor(x + 0.001) and (int)trunc(x) agree -- so the ARL rounding is invisible to it. V6
 * straddles the boundary instead (2.9999, 0.9999, 0.5): the two disagree on two of the three
 * vertices and select different constants, so the a0-relative constant path is actually exercised.
 * Measured: the mutant `a0 = (int)(A.x)` for `(int)floor(A.x + 0.001)` is CAUGHT by check 2 with
 * integer picks it is MISSED. The WORDS are unchanged (vertex data is not in the program), so V2's
 * identity, hash and table entry are still the parity test's, and the extracted encoder's own
 * self-check still runs the parity test's integer picks. */
static void case_make(int which, VsCase *c)
{
    vs_make(which, c);
    if (which == VS_A0) {
        /* floor(x + 0.001): 2.9999 -> 3, 0.9999 -> 1, 0.5 -> 0.  trunc(x): 2, 0, 0. */
        static const float picks[3] = { 2.9999f, 0.9999f, 0.5f };
        int i;
        for (i = 0; i < 3; i++) {
            c->v[i].aux1[0] = picks[i];
            c->v[i].aux1[1] = 0.0f;
            c->v[i].aux1[2] = 0.0f;
            c->v[i].aux1[3] = 1.0f;
        }
    }
}

/* Pack a case's instructions. Returns the instruction count. */
static uint32_t case_words(int which, uint32_t words[16][4])
{
    VsCase c;
    int i;

    vs_make(which, &c);
    for (i = 0; i < c.nins; i++) vs_encode(&c.ins[i], words[i]);
    return (uint32_t)c.nins;
}

/* Three extra check-1 fixtures, one or two instructions each (the encoder carries one input
 * number per instruction), so their implied masks are sparse and two reach above bit 7. */
static uint32_t extra_words(int which, uint32_t words[16][4])
{
    VsCase c;
    int i;

    memset(&c, 0, sizeof c);
    if (which == 0)
        vs_add(&c, vs_fin(vs_out(vs_op(MAC_MOV, 0, vs_v(5, NULL), vs_none(), vs_none()),
                                    O_POS, 0xF, 0)));
    else if (which == 1)
        vs_add(&c, vs_fin(vs_out(vs_op(MAC_MOV, 0, vs_v(11, NULL), vs_none(), vs_none()),
                                    O_POS, 0xF, 0)));
    else {
        vs_add(&c, vs_out(vs_op(MAC_MOV, 0, vs_v(5, NULL), vs_none(), vs_none()), O_POS, 0xF, 0));
        vs_add(&c, vs_fin(vs_out(vs_op(MAC_MOV, 0, vs_v(11, NULL), vs_none(), vs_none()),
                                 O_D0, 0xF, 0)));
    }
    for (i = 0; i < c.nins; i++) vs_encode(&c.ins[i], words[i]);
    return (uint32_t)c.nins;
}
static const char *const k_extra_tag[3] = { "X1", "X2", "X3" };
static const char *const k_extra_what[3] = {
    "mov oPos, v5 -- a sparse one-bit mask",
    "mov oPos, v11 -- a mask bit above 7",
    "mov oPos, v5 ; mov oD0, v11 -- sparse AND above bit 7",
};

/* ── The miss program ─────────────────────────────────────────────────────
 * The program the corpus deliberately does NOT carry (check 3). It is its own fixture, NOT one of
 * the V1..V6 cases: an earlier revision made V2 the miss, which meant V2's a0-relative indexing --
 * `arl a0.x, v1.x`, `(int)floor(A.x + 0.001)`, `vsh_crel(20 + a0)` -- was never drawn on the GPU
 * and never pixel-compared, so a defect there passed the whole gate. Every V1..V6 case is now a
 * hit; this program is the one left out.
 *
 *   mov oPos, v0            (mask 0x0001: it reads v0 and nothing else)
 *   mov oD0, c[150]  FINAL  (a single constant, with a value nothing else in the set reads)
 *
 * c[150] = (0.15, 0.65, 0.95, 1.0), so its pixels are its own: no other fixture can produce them,
 * and a fallback that drew nothing, or drew another program, is visible in the surface. */
#define MISS_KONST 150
static void miss_make(VsCase *c)
{
    static const float pos[3][4] = { { 6, 6, 0.5f, 1 }, { 58, 10, 0.5f, 1 }, { 14, 60, 0.5f, 1 } };
    static const float none4[4] = { 0, 0, 0, 1 };
    int i;

    memset(c, 0, sizeof *c);
    vs_pass_pos(c);
    vs_add(c, vs_fin(vs_out(vs_op(MAC_MOV, 0, vs_c(MISS_KONST, NULL), vs_none(), vs_none()),
                             O_D0, 0xF, 0)));
    vs_konst(c, MISS_KONST, 0.15f, 0.65f, 0.95f, 1.0f);
    for (i = 0; i < 3; i++) vs_vert(c, i, pos[i], none4, none4, none4);
}
static uint32_t miss_words(uint32_t words[16][4])
{
    VsCase c;
    int i;

    miss_make(&c);
    for (i = 0; i < c.nins; i++) vs_encode(&c.ins[i], words[i]);
    return (uint32_t)c.nins;
}
#define MISS_TAG  "MISS"
#define MISS_WHAT "mov oPos, v0 ; mov oD0, c[150] FINAL -- the program the table does not carry"

/* ── The `add` fixture (tag X4) ───────────────────────────────────────────
 * MAC op 3 (`add`, A + C) is the one MAC op no V1..V6 case and not the miss program uses: V1 uses
 * mov/dp4/mul/rcc/mad, V3 uses mul/rcp, V4 uses lit/exp/log/dst, V2 uses arl/mov, V5 uses mov. So a
 * translator that mis-emitted `add` changed no fixture's pixels -- an independent verifier's f1
 * mutant moved MAC op 3 and the whole gate stayed green. This fixture draws `add` and its pixels
 * depend on it: oD0 = (v1 + c[40]), a different value for every vertex.
 *
 *   mov R3, v1        (A = the vertex's aux1)
 *   add R3, R3, c[40] (MAC op 3: A + C; A is the temp, C the constant -- B is not read)
 *   mov oPos, v0
 *   mov oD0, R3  FINAL
 *
 * It is a FULL fixture, not a check-1-only extra like X1..X3: it has an identity, a table entry and
 * a CPU-vs-GPU pixel comparison of its own. */
#define ADD_KONST 40
static void add_make(VsCase *c)
{
    static const float pos[3][4] = { { 10, 8, 0.5f, 1 }, { 54, 14, 0.5f, 1 }, { 18, 56, 0.5f, 1 } };
    static const float a1[3][4] = { { 0.25f, 0.5f, 0.3f, 0.1f }, { 0.5f, 0.2f, 0.1f, 0.2f },
                                    { 0.1f, 0.4f, 0.6f, 0.3f } };
    static const float none4[4] = { 0, 0, 0, 1 };
    int i;

    memset(c, 0, sizeof *c);
    vs_add(c, vs_tmp(vs_op(MAC_MOV, 0, vs_v(1, NULL), vs_none(), vs_none()), 3, 0xF, 0));
    vs_add(c, vs_tmp(vs_op(MAC_ADD, 0, vs_r(3, NULL), vs_none(), vs_c(ADD_KONST, NULL)),
                     3, 0xF, 0));
    vs_add(c, vs_out(vs_op(MAC_MOV, 0, vs_v(0, NULL), vs_none(), vs_none()), O_POS, 0xF, 0));
    vs_add(c, vs_fin(vs_out(vs_op(MAC_MOV, 0, vs_r(3, NULL), vs_none(), vs_none()),
                             O_D0, 0xF, 0)));
    /* c[40] = (0.2, 0.1, 0.4, 0.05), so oD0 is (0.45,0.6,0.7,0.15) / (0.7,0.3,0.5,0.25) /
     * (0.3,0.5,1.0,0.35): distinct per vertex, and its alpha is non-opaque. */
    vs_konst(c, ADD_KONST, 0.2f, 0.1f, 0.4f, 0.05f);
    for (i = 0; i < 3; i++) vs_vert(c, i, pos[i], a1[i], none4, none4);
}
static uint32_t add_words(uint32_t words[16][4])
{
    VsCase c;
    int i;

    add_make(&c);
    for (i = 0; i < c.nins; i++) vs_encode(&c.ins[i], words[i]);
    return (uint32_t)c.nins;
}
#define ADD_TAG  "X4"
#define ADD_WHAT "MAC op 3 (add, A + C): oD0 = v1 + c[40] -- the one MAC op no V case uses"

/* ── The six further MAC-op fixtures (tags X5..X10) ───────────────────────
 * An independent verifier found that mutating MAC ops 5 (dp3), 6 (dph), 9 (min), 10 (max), 11
 * (slt) and 12 (sge) in the translator's k_mac_expr changed no compared pixel and left the whole
 * gate green: no V1..V6 case, not the miss program and not the add fixture emits any of them. Each
 * fixture here draws one of those ops and its pixels depend on the op's own semantics:
 *
 *   mov oPos, v0
 *   <op> oD0, v1, c[41]  FINAL
 *
 * The second operand is a CONSTANT, not a second input register: the parity test's encoder shares
 * one input index across all three sources (vs_encode sets g_vs_enc_conflict otherwise), so an
 * instruction can read only one vN. That is a limit of the encoder, not of the ISA, and it does
 * not weaken the fixtures -- the discriminating operand just comes from c[41].
 *
 * The values are chosen so the result is not any single component and not a plain mov:
 *   dp3/dph: A = v1 with distinct .xyz per vertex, B = c[41] = (0.6,0.8,0.5,0.2), so
 *            dot(v1.xyz, B.xyz) is 0.645 / 0.48 / 0.59 -- different per vertex, different from
 *            every component of A, and B.w and A.w are both non-zero so a 4-component dot and the
 *            missing/extra +B.w of dph each change the result.
 *   min/max/slt/sge: A = v1 and B = c[41] = (0.7,0.3,0.5,0.6) with A's components on both sides of
 *            B's per component, so min != max != A != B and the comparisons FLIP across the
 *            triangle (slt is (1,0,0)/(1,1,0)/(0,0,1) and sge its complement) rather than being
 *            all-zero or all-one. */
#define OP_KONST 41
#define OP_WHAT5  "MAC op 5 (dp3): oD0 = dot(v1.xyz, c[41]).xxxx -- splatted, so it differs from " \
                  "any single component of A"
#define OP_WHAT6  "MAC op 6 (dph): oD0 = dot(v1.xyz, c[41]) + c[41].w -- the +B.w term that " \
                  "distinguishes dph from dp3"
#define OP_WHAT7  "MAC op 9 (min): oD0 = min(v1, c[41]), component-wise, operands differ per " \
                  "component and per vertex"
#define OP_WHAT8  "MAC op 10 (max): oD0 = max(v1, c[41]), component-wise (max compares with >=)"
#define OP_WHAT9  "MAC op 11 (slt): oD0 = (v1 < c[41]) ? 1 : 0, per component -- flips across the " \
                  "triangle"
#define OP_WHAT10 "MAC op 12 (sge): oD0 = (v1 >= c[41]) ? 1 : 0, per component (sge compares with " \
                  ">=) -- the complement of slt, also flipping"
typedef struct { const char *tag; int mac; const char *what; } OpFix;
static const OpFix k_opfix[6] = {
    { "X5",  MAC_DP3, OP_WHAT5 },
    { "X6",  MAC_DPH, OP_WHAT6 },
    { "X7",  MAC_MIN, OP_WHAT7 },
    { "X8",  MAC_MAX, OP_WHAT8 },
    { "X9",  MAC_SLT, OP_WHAT9 },
    { "X10", MAC_SGE, OP_WHAT10 },
};
#define NOPFIX 6
static void op_make(int which, VsCase *c)
{
    static const float pos[3][4] = { { 12, 10, 0.5f, 1 }, { 52, 16, 0.5f, 1 }, { 20, 54, 0.5f, 1 } };
    static const float none4[4] = { 0, 0, 0, 1 };
    /* A for the dot-product ops (distinct .xyz per vertex, non-zero .w) and for the comparison
     * ops (components on both sides of B per component). */
    static const float v1_dot[3][4] = { { 0.25f, 0.40f, 0.35f, 0.40f },
                                        { 0.50f, 0.10f, 0.20f, 0.40f },
                                        { 0.10f, 0.60f, 0.10f, 0.40f } };
    static const float v1_cmp[3][4] = { { 0.20f, 0.80f, 0.50f, 0.50f },
                                        { 0.60f, 0.10f, 0.90f, 0.50f },
                                        { 0.90f, 0.40f, 0.20f, 0.50f } };
    const float (*v1)[4];
    float kx, ky, kz, kw;
    int i;

    if (which <= 1) { v1 = v1_dot; kx = 0.6f; ky = 0.8f; kz = 0.5f; kw = 0.2f; }
    else            { v1 = v1_cmp; kx = 0.7f; ky = 0.3f; kz = 0.5f; kw = 0.6f; }
    memset(c, 0, sizeof *c);
    vs_add(c, vs_out(vs_op(MAC_MOV, 0, vs_v(0, NULL), vs_none(), vs_none()), O_POS, 0xF, 0));
    vs_add(c, vs_fin(vs_out(vs_op(k_opfix[which].mac, 0, vs_v(1, NULL),
                                 vs_c(OP_KONST, NULL), vs_none()), O_D0, 0xF, 0)));
    vs_konst(c, OP_KONST, kx, ky, kz, kw);
    for (i = 0; i < 3; i++) vs_vert(c, i, pos[i], v1[i], none4, none4);
}
static uint32_t op_words(int which, uint32_t words[16][4])
{
    VsCase c;
    int i;

    op_make(which, &c);
    for (i = 0; i < c.nins; i++) vs_encode(&c.ins[i], words[i]);
    return (uint32_t)c.nins;
}

/* The fixture index of the miss program, and the fixture count. `k_prog`/`k_prog_tag`/`k_prog_what`
 * stay the five V1..V5 programs; the miss is fixture NPROG. */
#define MISS_PROG NPROG
#define ADD_PROG  (NPROG + 1)
#define OP_PROG(i) (NPROG + 2 + (i))
#define NFIX      (NPROG + 2 + NOPFIX)
static const char *const k_fixture_tag[NFIX] = {
    "V1", "V2", "V3", "V4", "V5", MISS_TAG, ADD_TAG,
    "X5", "X6", "X7", "X8", "X9", "X10",
};
static const char *const k_fixture_what[NFIX] = {
    "4x dp4 MVP against a general matrix + the D3D screen epilogue (mul/rcc, mad)",
    "arl a0.x, v1.x then mov oD0, c[a0.x + 20] (a0-relative constant)",
    "one instruction: MAC (mul R3) paired with ILU (rcp R1.x) reading the OLD R3",
    "the special-function unit: lit, exp, log, dst",
    "oFog.x = v1.x with the fog enable/colour path",
    MISS_WHAT,
    ADD_WHAT,
    OP_WHAT5, OP_WHAT6, OP_WHAT7, OP_WHAT8, OP_WHAT9, OP_WHAT10,
};
static uint32_t fixture_words(int i, uint32_t words[16][4])
{
    if (i == MISS_PROG) return miss_words(words);
    if (i == ADD_PROG) return add_words(words);
    if (i >= OP_PROG(0) && i < OP_PROG(NOPFIX)) return op_words(i - OP_PROG(0), words);
    return case_words(k_prog[i], words);
}

/* The identity nv2a_vsh_program_info reports for exactly these words, through the interpreter the
 * runtime uses. The words are loaded into the real program memory (every slot zeroed first, as
 * tests/test_vsh_aot_programs.c does), so this is the executor's own decode, not a copy. */
static int words_identity(const uint32_t (*words)[4], uint32_t len, uint32_t *hash,
                          uint32_t *mask, uint32_t *dlen, int *writes_c)
{
    const uint32_t (*w)[4] = NULL;
    uint32_t l = 0, h = 0, m = 0;
    int wc = 0;
    uint32_t k;

    for (k = 0; k < NV2A_VSH_SLOTS; k++) {
        const uint32_t zero[4] = { 0, 0, 0, 0 };
        nv2a_vsh_set_instruction(k, zero);
    }
    for (k = 0; k < len; k++)
        nv2a_vsh_set_instruction(k, words[k]);
    nv2a_vsh_set_start_slot(0);
    if (!nv2a_vsh_program_info(&w, &l, &h, &m, &wc))
        return 0;
    *hash = h;
    *mask = m;
    *dlen = l;
    *writes_c = wc;
    return 1;
}

/* ── The provenance hash ──────────────────────────────────────────────────
 * This is the test's own check, not production logic: the fixture generator records the hash of
 * the exact little-endian bytes it handed to the host tool, and the test recomputes it from the
 * words it uploads. Equal hashes prove the SPIR-V in the fixture header was compiled from these
 * very words -- not from a stored blob. */
static void words_serialize(const uint32_t (*words)[4], uint32_t len, unsigned char *out)
{
    uint32_t i, j;
    for (i = 0; i < len; i++)
        for (j = 0; j < 4; j++) {
            out[(size_t)i * 16u + j * 4u + 0u] = (unsigned char)(words[i][j] & 0xFFu);
            out[(size_t)i * 16u + j * 4u + 1u] = (unsigned char)((words[i][j] >> 8) & 0xFFu);
            out[(size_t)i * 16u + j * 4u + 2u] = (unsigned char)((words[i][j] >> 16) & 0xFFu);
            out[(size_t)i * 16u + j * 4u + 3u] = (unsigned char)((words[i][j] >> 24) & 0xFFu);
        }
}
static uint64_t bytes_hash64(const unsigned char *p, size_t n)
{
    uint64_t h = 0xcbf29ce484222325ull;
    size_t i;
    for (i = 0; i < n; i++) {
        h ^= (uint64_t)p[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
static unsigned popcount32(uint32_t v) { unsigned n = 0; while (v) { n += v & 1u; v >>= 1; } return n; }
static uint32_t f2u(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }

/* ══════════════════════════════════════════════════════════════════════════
 *  The fixture generator: built and run by CMakeLists.txt (VSH_CONFORMANCE_EMIT_FIXTURES)
 *  BEFORE the test, so the SPIR-V a table hit runs is compiled from the fixture's own words by
 *  the same host tool and the same dxc/flags scripts/gen-shaders.py uses for the retail corpus.
 * ══════════════════════════════════════════════════════════════════════════ */
#if defined(VSH_CONFORMANCE_EMIT_FIXTURES)

static int run_prog(const char *exe, char *const argv[])
{
    int rc = (int)_spawnv(_P_WAIT, exe, (const char *const *)argv);
    if (rc == -1)
        fail("FAIL: fixture generator: cannot run %s\n", exe);
    return rc;
}

int main(int argc, char **argv)
{
    const char *out = NULL, *work = NULL, *tool = NULL, *dxc = NULL, *sval = NULL;
    static unsigned char *spirv[NFIX];
    static size_t spirv_n[NFIX];
    static uint64_t spirv_hash[NFIX], words_hash[NFIX];
    static uint32_t mask_of[NFIX], len_of[NFIX], hash_of[NFIX];
    static uint32_t words_of[NFIX][16][4];
    int i, k;

    for (i = 1; i < argc; i++) {
        if (i + 1 >= argc) { fail("FAIL: fixture generator: %s needs a value\n", argv[i]); return 1; }
        if (strcmp(argv[i], "--out") == 0) out = argv[++i];
        else if (strcmp(argv[i], "--work") == 0) work = argv[++i];
        else if (strcmp(argv[i], "--tool") == 0) tool = argv[++i];
        else if (strcmp(argv[i], "--dxc") == 0) dxc = argv[++i];
        else if (strcmp(argv[i], "--spirv-val") == 0) sval = argv[++i];
        else { fail("FAIL: fixture generator: unknown option %s\n", argv[i]); return 1; }
    }
    if (!out || !work || !tool || !dxc || !sval) {
        fail("FAIL: fixture generator: --out, --work, --tool, --dxc and --spirv-val are all "
             "required\n");
        return 1;
    }
    for (i = 0; i < NFIX; i++) {
        char bin[1024], hlsl[1024], spv[1024], maskstr[32];
        char *targv[10], *dargv[12], *sargv[6];
        unsigned char bytes[16 * 16];
        uint32_t dlen = 0;
        int wc = 0, rc;

        len_of[i] = fixture_words(i, words_of[i]);
        if (!words_identity((const uint32_t (*)[4])words_of[i], len_of[i], &hash_of[i],
                            &mask_of[i], &dlen, &wc)) {
            fail("FAIL: fixture generator: %s does not decode to a FINAL\n", k_fixture_tag[i]);
            return 1;
        }
        if (dlen != len_of[i] || wc) {
            fail("FAIL: fixture generator: %s decodes to len %u (packed %u), writes_c %d\n",
                 k_fixture_tag[i], dlen, len_of[i], wc);
            return 1;
        }
        words_serialize((const uint32_t (*)[4])words_of[i], len_of[i], bytes);
        words_hash[i] = bytes_hash64(bytes, (size_t)len_of[i] * 16u);

        _snprintf(bin, sizeof bin, "%s\\%s.bin", work, k_fixture_tag[i]);
        _snprintf(hlsl, sizeof hlsl, "%s\\%s.hlsl", work, k_fixture_tag[i]);
        _snprintf(spv, sizeof spv, "%s\\%s.spv", work, k_fixture_tag[i]);
        bin[sizeof bin - 1] = hlsl[sizeof hlsl - 1] = spv[sizeof spv - 1] = '\0';
        _snprintf(maskstr, sizeof maskstr, "0x%04x", mask_of[i]);
        maskstr[sizeof maskstr - 1] = '\0';

        if (!write_file(bin, bytes, (size_t)len_of[i] * 16u)) {
            fail("FAIL: fixture generator: cannot write %s\n", bin);
            return 1;
        }
        targv[0] = (char *)tool; targv[1] = (char *)"--bin"; targv[2] = bin;
        targv[3] = (char *)"--mask"; targv[4] = maskstr;
        targv[5] = (char *)"--vk"; targv[6] = (char *)"--out"; targv[7] = hlsl; targv[8] = NULL;
        note("  [fixtures] %s --bin %s --mask %s --vk --out %s\n", tool, bin, maskstr, hlsl);
        rc = run_prog(tool, targv);
        if (rc != 0 || !file_exists(hlsl)) {
            fail("FAIL: fixture generator: %s: the host tool failed (exit %d)\n", k_fixture_tag[i], rc);
            return 1;
        }
        dargv[0] = (char *)dxc; dargv[1] = (char *)"-spirv"; dargv[2] = (char *)"-T";
        dargv[3] = (char *)"vs_6_0"; dargv[4] = (char *)"-E"; dargv[5] = (char *)"vs_prog";
        dargv[6] = (char *)"-fvk-invert-y"; dargv[7] = (char *)"-fspv-target-env=vulkan1.1";
        dargv[8] = (char *)"-Fo"; dargv[9] = spv; dargv[10] = hlsl; dargv[11] = NULL;
        note("  [fixtures] %s -spirv -T vs_6_0 -E vs_prog -fvk-invert-y "
             "-fspv-target-env=vulkan1.1 -Fo %s %s\n", dxc, spv, hlsl);
        rc = run_prog(dxc, dargv);
        if (rc != 0 || !file_exists(spv)) {
            fail("FAIL: fixture generator: %s: dxc failed (exit %d)\n", k_fixture_tag[i], rc);
            return 1;
        }
        sargv[0] = (char *)sval; sargv[1] = (char *)"--target-env"; sargv[2] = (char *)"vulkan1.1";
        sargv[3] = spv; sargv[4] = NULL;
        note("  [fixtures] %s --target-env vulkan1.1 %s\n", sval, spv);
        rc = run_prog(sval, sargv);
        if (rc != 0) {
            fail("FAIL: fixture generator: %s: spirv-val rejected the module (exit %d)\n",
                 k_fixture_tag[i], rc);
            return 1;
        }
        spirv[i] = read_file(spv, &spirv_n[i]);
        if (!spirv[i] || spirv_n[i] == 0 || spirv_n[i] % 4u != 0) {
            fail("FAIL: fixture generator: %s: the SPIR-V is %zu bytes, not a positive multiple "
                 "of 4\n", k_fixture_tag[i], spirv[i] ? spirv_n[i] : (size_t)0);
            return 1;
        }
        spirv_hash[i] = bytes_hash64(spirv[i], spirv_n[i]);
    }

    {
        FILE *f = fopen(out, "wb");
        if (!f) { fail("FAIL: fixture generator: cannot write %s\n", out); return 1; }
        fprintf(f, "/* Generated by tests/test_vsh_conformance.c (VSH_CONFORMANCE_EMIT_FIXTURES)\n"
                   " * from the V1..V6 fixture programs. Do not edit: it is rebuilt whenever the\n"
                   " * fixtures or the parity test's encoder change.\n"
                   " *\n"
                   " * Each program's SPIR-V was produced at build time from exactly the words in\n"
                   " * its `words` array, by the toolkit's nv2a_vsh_hlsl host tool (--vk), the\n"
                   " * Vulkan SDK's dxc (-spirv -T vs_6_0 -E vs_prog -fvk-invert-y\n"
                   " * -fspv-target-env=vulkan1.1) and spirv-val. words_bytes_hash is FNV-1a 64\n"
                   " * over the little-endian bytes of `words`, which the test recomputes: equal\n"
                   " * hashes prove the shader came from these words and not from a blob. */\n");
        fprintf(f, "#define VSH_CONF_FIXTURE_COUNT %d\n\n", NFIX);
        fprintf(f, "typedef struct {\n"
                   "    const char *tag;\n"
                   "    const uint32_t (*words)[4];\n"
                   "    uint32_t len;\n"
                   "    uint32_t input_mask;      /* what nv2a_vsh_program_info reports, and what\n"
                   "                               * the host tool accepted as --mask */\n"
                   "    uint64_t words_bytes_hash;/* provenance: the bytes dxc compiled */\n"
                   "    const uint32_t *spirv;\n"
                   "    size_t spirv_size;        /* bytes */\n"
                   "    uint64_t spirv_hash;\n"
                   "} VshConfFixture;\n\n");
        for (i = 0; i < NFIX; i++) {
            fprintf(f, "static const uint32_t k_fx%d_words[%u][4] = {\n", i, len_of[i]);
            for (k = 0; k < (int)len_of[i]; k++)
                fprintf(f, "    { 0x%08Xu, 0x%08Xu, 0x%08Xu, 0x%08Xu },\n",
                        words_of[i][k][0], words_of[i][k][1], words_of[i][k][2], words_of[i][k][3]);
            fprintf(f, "};\n");
            fprintf(f, "static const uint32_t k_fx%d_spirv[%u] = {\n", i,
                    (unsigned)(spirv_n[i] / 4u));
            for (k = 0; k < (int)(spirv_n[i] / 4u); k++) {
                uint32_t w = (uint32_t)spirv[i][k * 4 + 0]
                           | (uint32_t)spirv[i][k * 4 + 1] << 8
                           | (uint32_t)spirv[i][k * 4 + 2] << 16
                           | (uint32_t)spirv[i][k * 4 + 3] << 24;
                fprintf(f, "0x%08Xu,%s", w, (k % 8) == 7 ? "\n" : " ");
            }
            fprintf(f, "\n};\n\n");
        }
        fprintf(f, "static const VshConfFixture k_fixtures[VSH_CONF_FIXTURE_COUNT] = {\n");
        for (i = 0; i < NFIX; i++)
            fprintf(f, "    { \"%s\", k_fx%d_words, %uu, 0x%04Xu, 0x%016llXull, k_fx%d_spirv, "
                       "%uu, 0x%016llXull },\n",
                    k_fixture_tag[i], i, len_of[i], mask_of[i],
                    (unsigned long long)words_hash[i], i, (unsigned)(spirv_n[i]),
                    (unsigned long long)spirv_hash[i]);
        fprintf(f, "};\n");
        if (ferror(f)) { fail("FAIL: fixture generator: write error on %s\n", out); fclose(f); return 1; }
        fclose(f);
    }
    note("  [fixtures] wrote %s: %d programs\n", out, NFIX);
    for (i = 0; i < NFIX; i++) free(spirv[i]);
    return 0;
}

#else /* ── the gate itself ─────────────────────────────────────────────────── */

/* ══════════════════════════════════════════════════════════════════════════
 *  The gate
 * ══════════════════════════════════════════════════════════════════════════ */
#include "vsh_fixtures.inc"

/* The generated retail corpus header, from whichever include path the build offers it on. It is
 * not the table under test: it is used only to prove the fixtures are not retail programs and to
 * leave the back end in the production state at the end. */
#if defined(__has_include)
#  if __has_include("nv2a_vsh_programs.h")
#    include "nv2a_vsh_programs.h"
#  else
#    include "../src/recomp/gen/shaders/nv2a_vsh_programs.h"
#  endif
#else
#  include "nv2a_vsh_programs.h"
#endif

#define SURF(n)    (0x00400000u + (uint32_t)(n) * 0x00010000u)
#define VBUF(n)    (0x00800000u + (uint32_t)(n) * 0x00010000u)
#define SURF_BYTES 0x00010000u
#define SENTINEL   0x5Au
#define CLR        0xFF101010u
#define FOG_COLOR_V5 0xFFF09020u

#define SURF_W 64
#define SURF_H 64
#define PIXELS ((unsigned)(SURF_W * SURF_H))
#define TOL_VSH   4      /* the parity tests' vertex-program tolerance, per 8-bit channel */
#define COVER_TOL 24     /* a pixel this far from the clear colour was drawn on */

#define RC_ZERO 0x00u
#define RC_V0   0x04u
#define RC_FOG  0x03u
#define RC_R0   0x0Cu
#define RC_A(r) ((r) | 0x10u)

/* ── Pushbuffer builder (standalone NV2A, NV097 object on subchannel 0) ──── */
static uint32_t *g_pb;
static uint32_t g_pbn;

static void pb_begin(void)
{
    memset(g_pb, 0, 0x4000);
    g_pbn = 0;
    g_pb[g_pbn++] = (1u << 18); g_pb[g_pbn++] = 0x6u;     /* SET_OBJECT NV097 */
}
static void pb(uint32_t method, uint32_t value)
{
    if (g_pbn + 2u > 0x4000u / 4u) { fail("FAIL: test pushbuffer overflow\n"); exit(1); }
    g_pb[g_pbn++] = (1u << 18) | method;
    g_pb[g_pbn++] = value;
}
static void pb_surface(uint32_t va, uint32_t w, uint32_t h)
{
    pb(0x0200, w << 16);                           /* CLIP_H: x0 w */
    pb(0x0204, h << 16);                           /* CLIP_V: y0 h */
    pb(0x0208, 0x0108u);                           /* linear A8R8G8B8 */
    pb(0x020C, w * 4u);                            /* PITCH */
    pb(0x0210, va);                                /* COLOR_OFFSET */
}
static void pb_clear(uint32_t argb)
{
    pb(0x1D90, argb);                              /* SET_COLOR_CLEAR_VALUE */
    pb(0x1D94, 0xF0u);                             /* CLEAR_SURFACE: colour */
}
static void pb_texture_off(void) { pb(0x1B0C, 0); }
static void pb_draw_triangles(uint32_t nverts)
{
    pb(0x17FC, 5u);                                /* BEGIN TRIANGLES */
    pb(0x1810, ((nverts - 1u) << 24) | 0u);        /* DRAW_ARRAYS from 0 */
    pb(0x17FC, 0u);                                /* END */
}
static void pb_flip(void) { pb(0x0130, 0); }       /* FLIP_STALL */
static void pb_rc_stage_product(uint32_t a, uint32_t b)
{
    pb(0x0AC0, (a << 24) | (b << 16));               /* COLOR_ICW(0) */
    pb(0x0260, (RC_A(a) << 24) | (RC_A(b) << 16));   /* ALPHA_ICW(0) */
    pb(0x1E40, RC_R0 << 4);                          /* COLOR_OCW(0): AB -> r0 */
    pb(0x0AA0, RC_R0 << 4);                          /* ALPHA_OCW(0) */
}
static void pb_rc_final(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t g)
{
    pb(0x0288, (a << 24) | (b << 16) | (c << 8) | d);   /* SPECULAR_FOG_CW0 */
    pb(0x028C, g << 8);                                 /* SPECULAR_FOG_CW1 */
}
/* Attributes 0..3 at vb+0/16/32/48, four floats each, stride 64: v0 pos, v1 aux1, v2 aux2,
 * v3 colour. The slots the older parity cases use are cleared, as the method table admits
 * offsets only for v0..v5, v9 and v10. */
static void pb_vertex_format_vs(uint32_t vb)
{
    uint32_t i;
    for (i = 0; i < 4; i++) {
        pb(0x1720 + i * 4, vb + i * 16);
        pb(0x1760 + i * 4, 2u | (4u << 4) | (VS_STRIDE << 8));
    }
    for (i = 4; i < 11; i++) {
        if (i == 5 || (i > 5 && i < 9)) continue;
        pb(0x1720 + i * 4, 0);
        pb(0x1760 + i * 4, 0);
    }
}
static void pb_vs_program(const VsCase *c)
{
    int i, k;
    pb(0x1E94, 2u);                                  /* TRANSFORM_EXECUTION_MODE: program */
    pb(0x1E9C, 0u);                                  /* PROGRAM_LOAD slot 0 */
    pb(0x1EA0, 0u);                                  /* PROGRAM_START slot 0 */
    for (i = 0; i < c->nins; i++) {
        uint32_t w[4];
        vs_encode(&c->ins[i], w);
        for (k = 0; k < 4; k++) pb(0x0B00u + 4u * (uint32_t)k, w[k]);
    }
    for (i = 0; i < c->nk; i++) {
        pb(0x1EA4, c->k[i].index);                   /* CONSTANT_LOAD */
        for (k = 0; k < 4; k++) pb(0x0B80u + 4u * (uint32_t)k, f2u(c->k[i].v[k]));
    }
}
static void put_vs_vertices(uint32_t vb, const VsCase *c)
{
    int i;
    for (i = 0; i < 3; i++) {
        uint8_t *p = (uint8_t *)xbox_GetMemoryOffset() + vb + (uint32_t)i * VS_STRIDE;
        memcpy(p, c->v[i].pos, 16); memcpy(p + 16, c->v[i].aux1, 16);
        memcpy(p + 32, c->v[i].aux2, 16); memcpy(p + 48, c->v[i].col, 16);
    }
}

/* ── Guest memory helpers ────────────────────────────────────────────────── */
static uint8_t *G(uint32_t va) { return (uint8_t *)xbox_GetMemoryOffset() + va; }
static void prefill(uint32_t va) { memset(G(va), SENTINEL, SURF_BYTES); }
static uint8_t *snapshot(uint32_t va, size_t n)
{
    uint8_t *p = (uint8_t *)malloc(n);
    if (p) memcpy(p, G(va), n);
    return p;
}

/* ── The executor seam ───────────────────────────────────────────────────── */
static NV2AState *g_gpu;

static int submit(const char *what)
{
    Nv2aPbExecCounters b, a;
    int ok;
    memset(&b, 0, sizeof b); memset(&a, 0, sizeof a);
    nv2a_pb_exec_counters(&b);
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_GET] = 0;
    g_gpu->pfifo.regs[NV_PFIFO_CACHE1_DMA_PUT] = g_pbn * 4u;
    ok = nv2a_submit_pending(g_gpu);
    nv2a_pb_exec_counters(&a);
    note("  %-22s draws+%u clears+%u tris+%u flips_stall+%u unhandled+%u\n",
         what, a.draws - b.draws, a.clears - b.clears, a.tris_drawn - b.tris_drawn,
         a.flip_stalls - b.flip_stalls, a.unhandled - b.unhandled);
    if (!ok)
        fail("FAIL: pushbuffer rejected (%s): %s, subchannel %u method 0x%04X param 0x%08X, get 0x%X\n",
             what, nv2a_submit_diagnostic(g_gpu->pfifo.submit_diag),
             g_gpu->pfifo.submit_diag_subchannel, g_gpu->pfifo.submit_diag_method,
             g_gpu->pfifo.submit_diag_param, g_gpu->pfifo.submit_diag_get);
    if (a.tris_drawn == b.tris_drawn) {
        fail("FAIL: [%s] the executor drew no triangle: tris %u -> %u\n", what,
             b.tris_drawn, a.tris_drawn);
        return 0;
    }
    return ok;
}

/* One batch: explicit render state, clear, the program and its constants, one triangle, flip.
 * The state and the combiner are the parity test's vertex-program setup (test_gpu_backend_parity.c,
 * build_vs): no blend, no alpha test, no cull, no depth, no texture; one combiner stage that
 * produces zero, and a final combiner that passes the interpolated diffuse through -- or, for V5,
 * mixes it with the fog colour. It is spelled out rather than inherited from the NV2A reset
 * defaults so the fixture does not depend on them. */
static void build_batch_prog(uint32_t surf, uint32_t vb, const VsCase *cp, int fog);

static void build_batch(uint32_t surf, uint32_t vb, int which)
{
    VsCase c;

    case_make(which, &c);
    build_batch_prog(surf, vb, &c, which == VS_FOG);
}

/* The same batch, for a program built directly rather than by vs_make's case id. `fog` selects the
 * fog-enabled state, which only VS_FOG needs; the miss program and the extras use 0. */
static void build_batch_prog(uint32_t surf, uint32_t vb, const VsCase *cp, int fog)
{
    const VsCase *c = cp;

    pb_begin();
    pb(0x0300, 0);                                 /* alpha test off */
    pb(0x0304, 0);                                 /* blend off */
    pb(0x0308, 0);                                 /* cull off */
    pb(0x030C, 0);                                 /* depth test off */
    pb(0x032C, 0);                                 /* stencil test off */
    pb(0x0354, 0x207u);                            /* depth func ALWAYS */
    pb(0x0358, 0x01010101u);                       /* colour mask: A R G B */
    pb(0x035C, 0);                                 /* depth write off */
    pb(0x039C, 0x405u);                            /* cull face BACK */
    pb(0x03A0, 0x900u);                            /* front face CW */
    pb(0x0214, 0);                                 /* no zeta */
    pb_surface(surf, SURF_W, SURF_H);
    pb_clear(CLR);
    pb_texture_off();
    pb(0x1B4C, 0);                                 /* stage 1 off */
    pb_vertex_format_vs(vb);
    pb(0x02A4, fog);                               /* FOG_ENABLE */
    if (fog)
        pb(0x02A8, FOG_COLOR_V5);
    pb(0x1E70, 0);                                 /* no texture shader stages */
    pb(0x1E60, 1u);                                /* COMBINER_CONTROL: 1 stage (sets rc_seen) */
    pb_rc_stage_product(RC_ZERO, RC_ZERO);         /* r0 = 0, unused */
    if (fog)
        pb_rc_final(RC_A(RC_FOG), RC_V0, RC_FOG, RC_ZERO, RC_A(RC_V0));   /* f*diffuse + (1-f)*fog */
    else
        pb_rc_final(RC_ZERO, RC_ZERO, RC_ZERO, RC_V0, RC_A(RC_V0));
    pb_vs_program(c);
    put_vs_vertices(vb, c);
    pb_draw_triangles(3);
    pb_flip();
}

/* ── Pixel comparison: the parity/V5 rule, tightened where the measurements allow ─────
 * Four channels, not three: the surface is A8R8G8B8 and V4's dst/exp/log results reach the
 * surface only through oD0.w, which the final combiner sends to alpha. A comparison that skipped
 * channel 3 would leave V4's alpha half unchecked.
 *
 * Coverage is judged on RGB only, though: the clear colour is opaque (0xFF101010), so including
 * alpha in the "was this pixel drawn" test would let an alpha-only difference count as coverage
 * and blur the geometric comparison. */
typedef struct {
    unsigned match, worst, worst_rgb, worst_a;
    unsigned cov_ref, cov_got, cov_xor, region, region_bad, drawn;
    unsigned ref_alpha_nonopaque;      /* reference pixels whose alpha is not 255 */
    double region_allowed, cov_allowed;
} Cmp;

static uint32_t px32(const uint8_t *p, int x, int y)
{
    return *(const uint32_t *)(p + ((size_t)y * SURF_W + (size_t)x) * 4u);
}
/* The largest difference over the named channels, in 8-bit steps. */
static int chan_diff_n(uint32_t a, uint32_t b, int nchan)
{
    int i, m = 0;
    for (i = 0; i < nchan; i++) {
        int d = (int)((a >> (16 - 8 * i)) & 0xFFu) - (int)((b >> (16 - 8 * i)) & 0xFFu);
        if (d < 0) d = -d;
        if (d > m) m = d;
    }
    return m;
}
static int chan_diff(uint32_t a, uint32_t b) { return chan_diff_n(a, b, 4); }   /* A R G B */
static int chan_diff_rgb(uint32_t a, uint32_t b) { return chan_diff_n(a, b, 3); }
static int chan_alpha(uint32_t a, uint32_t b)
{
    int d = (int)(a >> 24) - (int)(b >> 24);
    return d < 0 ? -d : d;
}
static int covered(uint32_t v) { return chan_diff_rgb(v, CLR) > COVER_TOL; }

static Cmp compare(const uint8_t *ref, const uint8_t *got, double perim)
{
    Cmp c;
    int x, y;
    memset(&c, 0, sizeof c);
    for (y = 0; y < SURF_H; y++)
        for (x = 0; x < SURF_W; x++) {
            uint32_t a = px32(ref, x, y), b = px32(got, x, y);
            int d = chan_diff(a, b), drgb = chan_diff_rgb(a, b), da = chan_alpha(a, b);
            int ca = covered(a), cb = covered(b);
            c.cov_ref += (unsigned)ca;
            c.cov_got += (unsigned)cb;
            c.cov_xor += (unsigned)(ca != cb);
            if ((unsigned)d > c.worst) c.worst = (unsigned)d;
            if ((unsigned)drgb > c.worst_rgb) c.worst_rgb = (unsigned)drgb;
            if ((unsigned)da > c.worst_a) c.worst_a = (unsigned)da;
            if (d <= TOL_VSH) c.match++;
            if (ca || cb) {
                c.region++;
                if (d > TOL_VSH) c.region_bad++;
            }
            if (cb) c.drawn++;
            if (ca && (a >> 24) != 0xFFu) c.ref_alpha_nonopaque++;
        }
    c.region_allowed = perim + 0.02 * (double)c.region;
    /* Measured xor on lavapipe: 0 for every V1..V6 case, 4 for the miss program (its triangle's
     * top edge -- the per-pixel list is printed when it exceeds the tolerance). So the band the
     * parity rule allows (the whole perimeter) is slack these fixtures never use, and a whole-pixel
     * geometric error in the screen epilogue moves the whole drawn region: a probe measured V1
     * shifted 1 px in x -> xor 78 and 1 px in y -> xor 68, both of which the perimeter rule PASSES
     * and this bound rejects. Bound: 4, or a quarter of the perimeter, whichever is larger -- the
     * floor keeps a small triangle (V1's perimeter is 112) from being judged on a single pixel of
     * rounding, and matches the miss's measured 4. */
    c.cov_allowed = perim / 4.0;
    if (c.cov_allowed < 4.0) c.cov_allowed = 4.0;
    return c;
}

/* The GPU image must agree with the CPU interpreter's for the same batch. See the file header for
 * the rule and why each term is there. */
static int check_cpu(const char *who, const uint8_t *cpu, const uint8_t *gpu,
                     double perim, double area)
{
    Cmp c;
    int ok = 1;

    if (!cpu || !gpu) {
        fail("FAIL: [%s] no CPU reference or no GPU image to compare\n", who);
        return 0;
    }
    c = compare(cpu, gpu, perim);
    note("  [%s] vs the CPU interpreter: %.2f%% of %u pixels within %d (all four channels); "
         "worst channel diff %u (rgb %u, alpha %u); coverage cpu=%u gpu=%u (xor %u, allowed %.0f "
         "= max(4, perimeter/4 of %.0f), old perimeter band %.0f); drawn-region mismatches %u of "
         "%u (allowed %.0f); %u of %u pixels are not the clear colour, %u of the reference's "
         "drawn pixels have alpha != 255\n",
         who, 100.0 * c.match / (double)PIXELS, PIXELS, TOL_VSH, c.worst, c.worst_rgb, c.worst_a,
         c.cov_ref, c.cov_got, c.cov_xor, c.cov_allowed, perim, perim, c.region_bad, c.region,
         c.region_allowed, c.drawn, PIXELS, c.ref_alpha_nonopaque);
    if (c.match * 100u < PIXELS * 98u) {
        fail("FAIL: [%s] only %.2f%% of pixels agree with the CPU interpreter (need 98%%), worst "
             "channel diff %u (rgb %u, alpha %u)\n", who, 100.0 * c.match / (double)PIXELS,
             c.worst, c.worst_rgb, c.worst_a);
        ok = 0;
    }
    /* Name the pixels, as the parity tests do, so a failure points at a place rather than a
     * percentage -- and so the small residual differences this fixture does show are visible. */
    if (c.worst > TOL_VSH) {
        int x, y, shown = 0;
        for (y = 0; y < SURF_H && shown < 8; y++)
            for (x = 0; x < SURF_W && shown < 8; x++) {
                uint32_t a = px32(cpu, x, y), b = px32(gpu, x, y);
                if (chan_diff(a, b) > TOL_VSH) {
                    note("      (%2d,%2d) cpu ARGB %3u,%3u,%3u,%3u  gpu %3u,%3u,%3u,%3u  "
                         "diff rgb %d alpha %d\n", x, y, (a >> 24) & 0xFFu, (a >> 16) & 0xFFu,
                         (a >> 8) & 0xFFu, a & 0xFFu, (b >> 24) & 0xFFu, (b >> 16) & 0xFFu,
                         (b >> 8) & 0xFFu, b & 0xFFu, chan_diff_rgb(a, b), chan_alpha(a, b));
                    ++shown;
                }
            }
    }
    if ((double)c.region_bad > c.region_allowed) {
        fail("FAIL: [%s] %u drawn-region pixels disagree with the CPU interpreter (allowed %.0f: "
             "perimeter %.0f + 2%% of %u)\n", who, c.region_bad, c.region_allowed, perim, c.region);
        ok = 0;
    }
    if ((double)c.cov_xor > c.cov_allowed) {
        fail("FAIL: [%s] coverage differs from the CPU interpreter by %u pixels (allowed %.0f = "
             "max(4, perimeter/4 of %.0f); the old perimeter-only band allowed %.0f)\n", who,
             c.cov_xor, c.cov_allowed, perim, perim);
        ok = 0;
    }
    if (c.drawn < 500u) {
        fail("FAIL: [%s] only %u pixels differ from the clear colour: the draw did not land, so "
             "the comparison would be vacuous\n", who, c.drawn);
        ok = 0;
    }
    if (area > 0.0 && ((double)c.cov_ref < 0.9 * area || (double)c.cov_ref > 1.1 * area)) {
        fail("FAIL: [%s] the CPU reference covers %u pixels but the interpreter's own geometry "
             "predicts %.0f (allowed 90%%..110%%): the fixture is not the shape this check "
             "assumes\n", who, c.cov_ref, area);
        ok = 0;
    }
    return ok;
}

/* The triangle's perimeter in pixels and the area its pixels should cover, from the interpreter's
 * OWN geometry -- exactly as tests/test_gpu_backend_parity.c's build_vs does: the projected polygon
 * for the two MVP cases, the pass-through positions for the rest, and vs_predict_visible (the part
 * of the triangle in front of the eye) for the behind-camera one. */
static void case_geometry(int which, const VsCase *c, double *perim, double *area)
{
    double a[3][4];
    int i;

    if (which == VS_MVP || which == VS_BEHIND)
        for (i = 0; i < 3; i++) vs_project(c, i, a[i]);
    else
        for (i = 0; i < 3; i++) { a[i][0] = c->v[i].pos[0]; a[i][1] = c->v[i].pos[1]; }
    if (which == VS_BEHIND) {
        unsigned count, edge;
        vs_predict_visible(c, &count, &edge);
        *area = (double)count;
        *perim = (double)edge;
    } else {
        *perim = hypot(a[1][0] - a[0][0], a[1][1] - a[0][1])
               + hypot(a[2][0] - a[1][0], a[2][1] - a[1][1])
               + hypot(a[0][0] - a[2][0], a[0][1] - a[2][1]);
        *area = fabs(0.5 * ((a[1][0] - a[0][0]) * (a[2][1] - a[0][1])
                          - (a[1][1] - a[0][1]) * (a[2][0] - a[0][0])));
    }
}

/* ── stderr capture, for the "not silent" assertions ─────────────────────── */
static char g_log_path[512];

static int start_stderr_capture(void)
{
    /* The log has to be written where the SAME process can read it back, and the name has to be
     * unique to this process: a path that falls back to "." resolves against the launch cwd, which
     * is not stable when a task runner starts the process, and a log left by an earlier run would
     * otherwise be read back as if this run had written it (a false PASS for the "not silent"
     * assertions). So: an explicit --log, else TEMP, else TMP, else the directory of the
     * executable, plus this process's pid; and if even opening it fails, that is a FAILURE, not a
     * silent skip of the assertions that depend on it. */
    if (g_log_path[0] == '\0') {
        const char *tmp = getenv("TEMP");
        if (!tmp || !tmp[0]) tmp = getenv("TMP");
        if (!tmp || !tmp[0]) {
            char exe[1024];
            DWORD n = GetModuleFileNameA(NULL, exe, sizeof exe);
            if (n == 0 || n >= sizeof exe) return 0;
            exe[sizeof exe - 1] = '\0';
            {
                char *slash = strrchr(exe, '\\');
                if (!slash) return 0;
                slash[1] = '\0';
            }
            _snprintf(g_log_path, sizeof g_log_path, "%svsh_conformance_stderr.%lu.log", exe,
                      (unsigned long)GetCurrentProcessId());
        } else {
            _snprintf(g_log_path, sizeof g_log_path, "%s\\vsh_conformance_stderr.%lu.log", tmp,
                      (unsigned long)GetCurrentProcessId());
        }
        g_log_path[sizeof g_log_path - 1] = '\0';
    }
    remove(g_log_path);                   /* a leftover from an earlier run must not be read back */
    g_stderr_saved = _dup(2);
    if (g_stderr_saved < 0) return 0;
    if (!freopen(g_log_path, "w", stderr)) { g_stderr_saved = -1; return 0; }
    return 1;
}
static char *read_log(void)
{
    size_t n = 0;
    return (char *)read_file(g_log_path, &n);
}
static int log_has(const char *text, const char *needle)
{
    return text && strstr(text, needle) != NULL;
}

/* ── The host tool, for check 1 ──────────────────────────────────────────── */
static char g_tool[1024];
static char g_work[1024];

static int resolve_tool(const char *from_argv)
{
    static const char *const k_defaults[] = {
        "xboxrecomp/src/video/Release/nv2a_vsh_hlsl.exe",
        "build/xboxrecomp/src/video/Release/nv2a_vsh_hlsl.exe",
        "../xboxrecomp/src/video/Release/nv2a_vsh_hlsl.exe",
    };
    const char *env = getenv("XBR_VSH_HLSL_TOOL");
    size_t i;

    if (from_argv && from_argv[0])
        _snprintf(g_tool, sizeof g_tool, "%s", from_argv);
    else if (env && env[0])
        _snprintf(g_tool, sizeof g_tool, "%s", env);
    else
        for (i = 0; i < sizeof k_defaults / sizeof k_defaults[0]; i++)
            if (file_exists(k_defaults[i])) {
                _snprintf(g_tool, sizeof g_tool, "%s", k_defaults[i]);
                break;
            }
    g_tool[sizeof g_tool - 1] = '\0';
    if (!g_tool[0] || !file_exists(g_tool)) {
        fail("FAIL: the host tool nv2a_vsh_hlsl.exe was not found (--tool %s, XBR_VSH_HLSL_TOOL "
             "%s, and the default paths under the build tree). Check 1 cannot run, and a check "
             "that cannot run is a failure, not a pass.\n",
             from_argv && from_argv[0] ? from_argv : "<none>",
             env && env[0] ? env : "<unset>");
        return 0;
    }
    return 1;
}

/* The properties every one of these shaders must have, asserted on the REFERENCE (the linked
 * nv2a_vsh_hlsl's own output) as well as on the tool's. Two purposes: it proves the equality is
 * over a real, non-trivial shader rather than two empty strings, and it is what catches a
 * translator change that the tool necessarily mirrors -- check 1's byte comparison cannot, since
 * it links the very function it runs the tool against. */
static size_t count_sub(const char *hay, const char *needle)
{
    size_t n = 0, k = strlen(needle);
    const char *p = hay;
    if (!k) return 0;
    while ((p = strstr(p, needle)) != NULL) { n++; p += k; }
    return n;
}

/* Whitespace-insensitive substring: `needle`'s non-space characters must occur in `hay` with any
 * whitespace between them. The translator's output wraps its struct members across lines (the
 * `[[vk::location(N)]]` and its `float4 name : SEMANTIC;` are not always on one line), so a plain
 * strstr would miss a valid shader; this matches the declaration whatever the spacing. */
static int has_collapsed(const char *hay, const char *needle)
{
    const char *start;
    for (start = hay; *start; start++) {
        const char *h = start, *n = needle;
        while (*n) {
            if (*n == ' ' || *n == '\t' || *n == '\n' || *n == '\r') { n++; continue; }
            while (*h == ' ' || *h == '\t' || *h == '\n' || *h == '\r') h++;
            if (*h != *n) break;
            h++; n++;
        }
        if (!*n) return 1;
    }
    return 0;
}

/* The --vk output's output varyings, by NAME and LOCATION. The counts asserted alongside this
 * cannot see a varying moved to a different location, and the fixtures' combiner reads d0 alone,
 * so a moved t1 (or any varying this fixture never reads) leaves the pixels identical: a shader
 * whose t1 went to location 7 would pass every other check in this file. The locations are the
 * combiner's own contract -- src/video/shaders/nv2a_vk_rc.hlsl's VO struct:
 *     float4 p : SV_Position;                        (the system value, no location)
 *     [[vk::location(0)]] noperspective float4 d0 : COLOR0;
 *     [[vk::location(1)]] noperspective float4 d1 : COLOR1;
 *     [[vk::location(2)]] noperspective float fog : TEXCOORD4;
 *     [[vk::location(3)]] float4 t0 : TEXCOORD0;
 *     [[vk::location(4)]] float4 t1 : TEXCOORD1;
 *     [[vk::location(5)]] float4 t2 : TEXCOORD2;
 *     [[vk::location(6)]] float4 t3 : TEXCOORD3;
 * and NV2A_VSH_HLSL_VO_FIELDS in nv2a_vsh_hlsl.h, which is the same list. */
static int check_output_locations(const char *who, const char *vk_text)
{
    /* The name each location declares, or "" when no [[vk::location(N)]] declares one. */
    char got[16][24];
    static const struct { const char *name; unsigned loc; } k_want[] = {
        { "d0", 0 }, { "d1", 1 }, { "fog", 2 }, { "t0", 3 }, { "t1", 4 }, { "t2", 5 }, { "t3", 6 },
    };
    const char *p;
    int ok = 1;
    size_t i;

    memset(got, 0, sizeof got);
    /* Only the OUTPUT struct: the input struct declares the same [[vk::location(N)]] annotations
     * for v0..vN, and its v0 would otherwise be read as the output at location 0. The translator
     * names it VOP (nv2a_vsh_hlsl.c); parse from it to the next "};". */
    p = strstr(vk_text, "struct VOP {");
    if (!p) {
        fail("FAIL: [%s] the --vk output has no \"struct VOP {\", so no output varying's location "
             "can be checked\n", who);
        return 0;
    }
    {
        const char *end = strstr(p, "};");
        if (!end) {
            fail("FAIL: [%s] the --vk output's \"struct VOP {\" is not terminated\n", who);
            return 0;
        }
        /* Parse only [p, end): the output struct's body. */
        while (p < end && (p = strstr(p, "[[vk::location(")) != NULL && p < end) {
        unsigned L = 0;
        const char *q = p + strlen("[[vk::location(");
        const char *name;
        size_t n;
        while (*q >= '0' && *q <= '9') { L = L * 10u + (unsigned)(*q - '0'); q++; }
        if (L >= 16u) { p++; continue; }
        name = strchr(q, ':');                 /* the semantic, e.g. " : COLOR0;" */
        if (!name) { p++; continue; }
        name--;                                /* step back over the space before ':' */
        while (name > q && (name[-1] == ' ' || name[-1] == '\t')) name--;
        while (name > q && (name[-1] == '_' || (name[-1] >= '0' && name[-1] <= '9')
                            || (name[-1] >= 'A' && name[-1] <= 'Z')
                            || (name[-1] >= 'a' && name[-1] <= 'z'))) name--;
        n = (size_t)(strchr(q, ':') - name);
        while (n > 0 && name[n - 1] == ' ') n--;
        if (n >= sizeof got[0]) n = sizeof got[0] - 1;
        memcpy(got[L], name, n);
        got[L][n] = '\0';
        p++;
        }
    }

    for (i = 0; i < sizeof k_want / sizeof k_want[0]; i++) {
        if (strcmp(got[k_want[i].loc], k_want[i].name) != 0) {
            /* Name where it actually went, when it is somewhere else at all: that is the defect
             * (a varying moved), and it is clearer than "nothing is at its location". */
            unsigned L, found = 16u;
            for (L = 0; L < 16u; L++)
                if (L != k_want[i].loc && strcmp(got[L], k_want[i].name) == 0) { found = L; break; }
            if (found < 16u)
                fail("FAIL: [%s] output %s is at [[vk::location(%u)]], but the combiner reads it "
                     "at %u (see nv2a_vk_rc.hlsl's VO)\n", who, k_want[i].name, found,
                     k_want[i].loc);
            else
                fail("FAIL: [%s] output %s must be at [[vk::location(%u)]], but that location "
                     "declares \"%s\" (the combiner reads %s at %u; see nv2a_vk_rc.hlsl's VO)\n",
                     who, k_want[i].name, k_want[i].loc,
                     got[k_want[i].loc][0] ? got[k_want[i].loc] : "<nothing>",
                     k_want[i].name, k_want[i].loc);
            ok = 0;
        } else {
            /* It is where it belongs; it must not ALSO be declared somewhere else, or the
             * combiner could still read the wrong one. */
            unsigned L;
            for (L = 0; L < 16u; L++)
                if (L != k_want[i].loc && strcmp(got[L], k_want[i].name) == 0) {
                    fail("FAIL: [%s] output %s is also declared at location %u, not only %u\n",
                         who, k_want[i].name, L, k_want[i].loc);
                    ok = 0;
                }
        }
    }
    if (!has_collapsed(vk_text, "float4 p : SV_Position;")) {
        fail("FAIL: [%s] the --vk output has no \"float4 p : SV_Position;\": the shader has no "
             "position output\n", who);
        ok = 0;
    }
    return ok;
}

static int check_shader_shape(const char *who, const char *text, uint32_t mask, int vk)
{
    size_t i;
    int ok = 1;
    struct { const char *needle; size_t want; } fixed[] = {
        { "struct VIP {", 1 },
        { "VOP vs_prog(VIP vin)", 1 },
        { "cbuffer VshC : register(b1)", 1 },
        { "cbuffer VshX : register(b2)", 1 },
        { "SV_Position", 1 },
        { "SV_ClipDistance0", 1 },
        { "o.clip = w - 1e-3;", 1 },
        { "o.d0 = saturate(o3);", 1 },
        { "float4 c[192];", 1 },
        { "float vsh_fog(float d)", 1 },
        { "float vsh_rcc(float x)", 1 },
        { "float4 vsh_exp(float x)", 1 },
        { "float4 vsh_log(float x)", 1 },
        { "float4 vsh_lit(float4 s)", 1 },
        { "int a0 = 0;", 1 },
    };
    for (i = 0; i < sizeof fixed / sizeof fixed[0]; i++) {
        size_t n = count_sub(text, fixed[i].needle);
        if (n != fixed[i].want) {
            fail("FAIL: [%s] the shader has %zu occurrences of \"%s\", expected %zu\n", who, n,
                 fixed[i].needle, fixed[i].want);
            ok = 0;
        }
    }
    /* The declare block and the epilogue's texture coordinates. */
    if (count_sub(text, "noperspective float4 d0 : COLOR0;") != 1
        || count_sub(text, "noperspective float4 d1 : COLOR1;") != 1
        || count_sub(text, "noperspective float fog : TEXCOORD4;") != 1) {
        fail("FAIL: [%s] the shader does not declare the combiner varyings d0/d1/fog\n", who);
        ok = 0;
    }
    if (count_sub(text, "float4 t0 : TEXCOORD0;") != 1 || count_sub(text, "float4 t3 : TEXCOORD3;") != 1) {
        fail("FAIL: [%s] the shader does not declare t0..t3\n", who);
        ok = 0;
    }
    /* One vertex input per mask bit, at that bit's own number, in both forms. */
    for (i = 0; i < 16u; i++) {
        char pat[96];
        if (!((mask >> i) & 1u)) continue;
        _snprintf(pat, sizeof pat, "float4 v%u : ATTR%u;", (unsigned)i, (unsigned)i);
        pat[sizeof pat - 1] = '\0';
        if (count_sub(text, pat) != 1) {
            fail("FAIL: [%s] the shader has no single \"%s\" for mask bit %u\n", who, pat,
                 (unsigned)i);
            ok = 0;
        }
    }
    /* The mask must have produced exactly as many inputs as it has bits. */
    if (count_sub(text, " : ATTR") != popcount32(mask)) {
        fail("FAIL: [%s] the shader has %zu ATTR declarations for a mask with %u bits\n", who,
             count_sub(text, " : ATTR"), popcount32(mask));
        ok = 0;
    }
    if (vk) {
        if (count_sub(text, "[[vk::") != 9u + popcount32(mask)) {
            fail("FAIL: [%s] the --vk shader has %zu [[vk:: decorations, expected %u (2 bindings "
                 "+ 7 outputs + %u inputs)\n", who, count_sub(text, "[[vk::"),
                 9u + popcount32(mask), popcount32(mask));
            ok = 0;
        }
        if (count_sub(text, "ATTR") != popcount32(mask)) {
            fail("FAIL: [%s] the --vk shader still has %zu ATTR declarations; the annotations "
                 "should have replaced them with locations\n", who, count_sub(text, "ATTR"));
            ok = 0;
        }
    } else if (count_sub(text, "[[vk::") != 0) {
        fail("FAIL: [%s] the plain shader carries %zu Vulkan decorations\n", who,
             count_sub(text, "[[vk::"));
        ok = 0;
    }
    return ok;
}

/* Run the tool on `words` and return its exit code (-1 if it could not be spawned). When the exit
 * code is 0 and `out_bytes` is not NULL, the file it wrote is read into *out_bytes. */
static int run_tool(const uint32_t (*words)[4], uint32_t len, uint32_t mask, int vk,
                    const char *tag, unsigned char **out_bytes, size_t *out_n)
{
    char bin[1024], out[1024], maskstr[32];
    unsigned char bytes[16 * 16];
    char *argv[10];
    int rc;

    if (out_n) *out_n = 0;
    if (out_bytes) *out_bytes = NULL;
    if (len > 16u) { fail("FAIL: [%s] %u instructions does not fit the fixture buffer\n", tag, len); return -1; }
    words_serialize(words, len, bytes);
    _snprintf(bin, sizeof bin, "%s\\%s%s.bin", g_work, tag, vk ? ".vk" : "");
    _snprintf(out, sizeof out, "%s\\%s%s.hlsl", g_work, tag, vk ? ".vk" : "");
    _snprintf(maskstr, sizeof maskstr, "0x%04x", mask);
    bin[sizeof bin - 1] = out[sizeof out - 1] = maskstr[sizeof maskstr - 1] = '\0';
    if (!write_file(bin, bytes, (size_t)len * 16u)) {
        fail("FAIL: [%s] cannot write %s\n", tag, bin);
        return -1;
    }
    argv[0] = g_tool; argv[1] = (char *)"--bin"; argv[2] = bin;
    argv[3] = (char *)"--mask"; argv[4] = maskstr;
    if (vk) { argv[5] = (char *)"--vk"; argv[6] = (char *)"--out"; argv[7] = out; argv[8] = NULL; }
    else    { argv[5] = (char *)"--out"; argv[6] = out; argv[7] = NULL; }
    rc = (int)_spawnv(_P_WAIT, g_tool, (const char *const *)argv);
    if (rc == -1) {
        fail("FAIL: [%s] cannot run %s\n", tag, g_tool);
        return -1;
    }
    if (rc != 0 || !out_bytes)
        return rc;
    *out_bytes = read_file(out, out_n);
    if (!*out_bytes)
        fail("FAIL: [%s] the host tool exited 0 but wrote no %s\n", tag, out);
    return rc;
}

static int bytes_equal(const char *who, const unsigned char *a, size_t an,
                       const unsigned char *b, size_t bn)
{
    size_t i, n = an < bn ? an : bn;

    for (i = 0; i < n; i++)
        if (a[i] != b[i]) {
            fail("FAIL: [%s] byte %zu differs: nv2a_vsh_hlsl wrote 0x%02X, the host tool wrote "
                 "0x%02X (lengths %zu and %zu)\n", who, i, a[i], b[i], an, bn);
            return 0;
        }
    if (an != bn) {
        fail("FAIL: [%s] the outputs differ in length: nv2a_vsh_hlsl wrote %zu bytes, the host "
             "tool %zu\n", who, an, bn);
        return 0;
    }
    if (an == 0) {
        fail("FAIL: [%s] both outputs are empty, so byte equality proves nothing\n", who);
        return 0;
    }
    return 1;
}

/* ── 1. Byte equality, the core of the plan's V6 item 1 ──────────────────── */
static int check_byte_equality(void)
{
    static char oracle[1u << 20];
    int ok = 1, row;
    int nrows = NCASE + 1 + 3;                 /* V1..V6, V7, X1..X3 */
    const char *saved_vsh = getenv("RECOMP_GPU_VSH");
    char saved[16];

    saved[0] = '\0';
    if (saved_vsh) { _snprintf(saved, sizeof saved, "%s", saved_vsh); saved[sizeof saved - 1] = '\0'; }

    note("-- check 1: the offline host tool's bytes vs nv2a_vsh_hlsl --\n");
    note("  tool: %s\n", g_tool);
    for (row = 0; row < nrows; row++) {
        uint32_t words[16][4];
        uint32_t len, hash = 0, mask = 0, dlen = 0;
        int wc = 0, which = 0, is_v7 = 0;
        const char *tag, *what;
        unsigned char *plain = NULL, *vk = NULL;
        size_t plain_n = 0, vk_n = 0, i;
        int n_oracle, rc;
        uint64_t whash;
        unsigned char bytes[16 * 16];

        if (row < NCASE) {
            which = k_cases[row].which;
            tag = k_cases[row].tag;
            what = k_cases[row].what;
            len = case_words(which, words);
        } else if (row == NCASE) {
            is_v7 = 1;
            which = VS_MVP;
            tag = "V7";
            what = "V1's words with RECOMP_GPU_VSH=0 (the CPU-transform path)";
            len = case_words(which, words);
        } else {
            tag = k_extra_tag[row - NCASE - 1];
            what = k_extra_what[row - NCASE - 1];
            len = extra_words(row - NCASE - 1, words);
        }
        if (!words_identity((const uint32_t (*)[4])words, len, &hash, &mask, &dlen, &wc)) {
            fail("FAIL: [%s] the fixture does not decode to a FINAL\n", tag);
            ok = 0;
            continue;
        }
        if (dlen != len) {
            fail("FAIL: [%s] packed %u instructions but nv2a_vsh_program_info reports %u\n",
                 tag, len, dlen);
            ok = 0;
            continue;
        }
        words_serialize((const uint32_t (*)[4])words, len, bytes);
        whash = bytes_hash64(bytes, (size_t)len * 16u);

        /* The V7 row runs the tool in a child whose environment has RECOMP_GPU_VSH=0: the
         * translator must not consult it. The previous value is restored, and main asserts the
         * variable is not "0" before the executor's first program batch. */
        if (is_v7) _putenv_s("RECOMP_GPU_VSH", "0");
        rc = run_tool((const uint32_t (*)[4])words, len, mask, 0, tag, &plain, &plain_n);
        if (is_v7) _putenv_s("RECOMP_GPU_VSH", saved);
        if (rc != 0 || !plain) { ok = 0; continue; }

        n_oracle = nv2a_vsh_hlsl((const uint32_t (*)[4])words, len, mask, oracle,
                                 sizeof oracle);
        if (n_oracle <= 0) {
            fail("FAIL: [%s] nv2a_vsh_hlsl translated nothing for %u instructions (no FINAL, or "
                 "the output did not fit %u bytes)\n", tag, len, (unsigned)sizeof oracle);
            free(plain);
            ok = 0;
            continue;
        }
        ok &= bytes_equal(tag, (const unsigned char *)oracle, (size_t)n_oracle, plain, plain_n);

        /* Both outputs must be a real shader of the right shape, not two empty or trivial
         * strings: asserted on the REFERENCE too, because the linked translator is what the
         * equality is against and a change inside it moves both sides (see the m1c mutant in the
         * V6 receipt). */
        ok &= check_shader_shape(tag, oracle, mask, 0);
        {
            char *pt = (char *)malloc(plain_n + 1u);
            if (!pt) { fail("FAIL: [%s] out of memory\n", tag); ok = 0; }
            else {
                memcpy(pt, plain, plain_n);
                pt[plain_n] = '\0';
                ok &= check_shader_shape(tag, pt, mask, 0);
                free(pt);
            }
        }

        /* The plain form must not already carry the Vulkan decorations, or "byte-identical" would
         * be satisfied by the tool always annotating. */
        {
            char *pt = (char *)malloc(plain_n + 1u);
            if (!pt) { fail("FAIL: [%s] out of memory\n", tag); ok = 0; }
            else {
                size_t nd;
                memcpy(pt, plain, plain_n);
                pt[plain_n] = '\0';
                nd = count_sub(pt, "[[vk::");
                if (nd != 0) {
                    fail("FAIL: [%s] the non---vk output carries %zu Vulkan decorations\n", tag, nd);
                    ok = 0;
                }
                free(pt);
            }
        }

        /* And the --vk form must differ from it by exactly the decorations: one location per mask
         * bit at its own bit number, seven output locations, bindings 9 and 10, and the clip
         * distance. */
        rc = run_tool((const uint32_t (*)[4])words, len, mask, 1, tag, &vk, &vk_n);
        if (rc != 0 || !vk) { free(plain); ok = 0; continue; }
        {
            char *vt = (char *)malloc(vk_n + 1u);
            if (!vt) { fail("FAIL: [%s] out of memory\n", tag); ok = 0; }
            else {
                size_t nb, nl;
                memcpy(vt, vk, vk_n);
                vt[vk_n] = '\0';
                ok &= check_shader_shape(tag, vt, mask, 1);
                if (vk_n == plain_n && memcmp(vk, plain, vk_n) == 0) {
                    fail("FAIL: [%s] the --vk output is byte-identical to the plain one, so the "
                         "annotations are missing\n", tag);
                    ok = 0;
                }
                nb = count_sub(vt, "[[vk::binding(");
                nl = count_sub(vt, "[[vk::location(");
                if (nb != 2u) {
                    fail("FAIL: [%s] the --vk output has %zu [[vk::binding( decorations, expected "
                         "2 (bindings 9 and 10)\n", tag, nb);
                    ok = 0;
                }
                if (count_sub(vt, "[[vk::binding(9, 0)]]") != 1u
                    || count_sub(vt, "[[vk::binding(10, 0)]]") != 1u) {
                    fail("FAIL: [%s] the --vk output does not bind 9 and 10 exactly once each\n", tag);
                    ok = 0;
                }
                if (nl != 7u + popcount32(mask)) {
                    fail("FAIL: [%s] the --vk output has %zu [[vk::location( decorations, expected "
                         "%u (7 outputs 0..6 + %u inputs)\n", tag, nl, 7u + popcount32(mask),
                         popcount32(mask));
                    ok = 0;
                }
                /* The COUNT above cannot see a varying moved to another location, and the
                 * fixtures' combiner reads d0 alone, so a moved t1 would leave every pixel
                 * identical. The values are asserted here, by name. */
                ok &= check_output_locations(tag, vt);
                if (!strstr(vt, "SV_ClipDistance0")) {
                    fail("FAIL: [%s] the --vk output has no SV_ClipDistance0: the corpus shader "
                         "would not clip like the CPU path\n", tag);
                    ok = 0;
                }
                for (i = 0; i < 16u; i++) {
                    char pat[96];
                    if (!((mask >> i) & 1u)) continue;
                    _snprintf(pat, sizeof pat, "[[vk::location(%u)]] float4 v%u : ATTR%u;",
                              (unsigned)i, (unsigned)i, (unsigned)i);
                    pat[sizeof pat - 1] = '\0';
                    if (!strstr(vt, pat)) {
                        fail("FAIL: [%s] the --vk output has no \"%s\": mask bit %u has no input "
                             "location at its own bit\n", tag, pat, (unsigned)i);
                        ok = 0;
                    }
                }
                free(vt);
            }
        }

        note("  [%s] len %2u  mask 0x%04X  hash %08X  digest %016llX  words_hash %016llX\n"
             "        plain %zu bytes, --vk %zu bytes; byte-identical to nv2a_vsh_hlsl: yes; "
             "%s\n",
             tag, len, mask, hash, (unsigned long long)nv2a_vsh_aot_digest(words, len),
             (unsigned long long)whash, plain_n, vk_n, what);
        free(plain);
        free(vk);
    }

    /* The tool must refuse a mask the instructions do not imply: that is what makes the --mask it
     * did accept evidence about the program's real input mask. */
    {
        uint32_t words[16][4];
        uint32_t len = case_words(VS_MVP, words), h = 0, m = 0, dl = 0;
        int wc = 0;
        unsigned char *b = NULL;
        size_t bn = 0;
        uint32_t wrong;
        int rc;

        if (!words_identity((const uint32_t (*)[4])words, len, &h, &m, &dl, &wc)) {
            fail("FAIL: [mask] the V1 fixture does not decode\n");
            return 0;
        }
        wrong = (m ^ 0x2u) & 0xFFFFu;
        note("  [mask] the instructions imply 0x%04X; asking for 0x%04X must be refused\n", m, wrong);
        rc = run_tool((const uint32_t (*)[4])words, len, wrong, 0, "maskrefuse", &b, &bn);
        free(b);
        if (rc != 3) {
            fail("FAIL: [mask] the tool exited %d for a mask the instructions do not imply, "
                 "expected 3\n", rc);
            ok = 0;
        }
    }
    return ok;
}

/* ── The synthetic corpus, built from the fixtures' own words ────────────── */
static uint32_t g_prog_hash[NFIX], g_prog_len[NFIX], g_prog_mask[NFIX];
static uint64_t g_prog_digest[NFIX];
static Nv2aVshAotEntry g_table[NFIX];
static size_t g_table_n;
static int g_case_table_idx[NCASE];     /* case -> index into g_table, or -1 */

static int fixture_identity(int i)
{
    uint32_t dlen = 0;
    int wc = 0;
    unsigned char bytes[16 * 16];

    if (!words_identity(k_fixtures[i].words, k_fixtures[i].len, &g_prog_hash[i],
                        &g_prog_mask[i], &dlen, &wc)) {
        fail("FAIL: [fixture %s] does not decode to a FINAL\n", k_fixtures[i].tag);
        return 0;
    }
    if (dlen != k_fixtures[i].len) {
        fail("FAIL: [fixture %s] the header says len %u, nv2a_vsh_program_info says %u\n",
             k_fixtures[i].tag, k_fixtures[i].len, dlen);
        return 0;
    }
    if (g_prog_mask[i] != k_fixtures[i].input_mask) {
        fail("FAIL: [fixture %s] the header says input_mask 0x%04X, nv2a_vsh_program_info says "
             "0x%04X (the header's mask is the one the host tool accepted)\n",
             k_fixtures[i].tag, k_fixtures[i].input_mask, g_prog_mask[i]);
        return 0;
    }
    if (wc) {
        fail("FAIL: [fixture %s] writes constants, so the back end's program path would decline "
             "it\n", k_fixtures[i].tag);
        return 0;
    }
    g_prog_len[i] = k_fixtures[i].len;
    g_prog_digest[i] = nv2a_vsh_aot_digest(k_fixtures[i].words, k_fixtures[i].len);
    words_serialize(k_fixtures[i].words, k_fixtures[i].len, bytes);
    if (bytes_hash64(bytes, (size_t)k_fixtures[i].len * 16u) != k_fixtures[i].words_bytes_hash) {
        fail("FAIL: [fixture %s] the header's words_bytes_hash does not match these words: the "
             "header was hand-edited, or the generator and the test disagree about them\n",
             k_fixtures[i].tag);
        return 0;
    }
    /* And the SPIR-V blob the header carries must hash to what the header records, so a blob
     * swapped under a fixture is caught here rather than only by the pixels of the programs this
     * run happens to draw. */
    if (bytes_hash64((const unsigned char *)k_fixtures[i].spirv, k_fixtures[i].spirv_size)
        != k_fixtures[i].spirv_hash) {
        fail("FAIL: [fixture %s] the header's spirv_hash is %016llX but the %u bytes it carries "
             "hash to %016llX: the SPIR-V was replaced without the header being regenerated\n",
             k_fixtures[i].tag, (unsigned long long)k_fixtures[i].spirv_hash,
             (unsigned)k_fixtures[i].spirv_size,
             (unsigned long long)bytes_hash64((const unsigned char *)k_fixtures[i].spirv,
                                              k_fixtures[i].spirv_size));
        return 0;
    }
    return 1;
}

static int hash_cmp(const void *a, const void *b)
{
    uint32_t x = ((const Nv2aVshAotEntry *)a)->hash, y = ((const Nv2aVshAotEntry *)b)->hash;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* Build the table from the fixtures, recomputing every identity field from the fixture's own
 * words. `skip_prog` >= 0 leaves that program out (the miss fixture); pass -1 for all. */
static int table_build(int skip_prog, const char *who)
{
    size_t i;
    int k;

    g_table_n = 0;
    for (i = 0; i < VSH_CONF_FIXTURE_COUNT; i++) {
        if ((int)i == skip_prog) continue;
        g_table[g_table_n].hash = g_prog_hash[i];
        g_table[g_table_n].len = g_prog_len[i];
        g_table[g_table_n].input_mask = g_prog_mask[i];
        g_table[g_table_n].digest = g_prog_digest[i];
        g_table[g_table_n].words = k_fixtures[i].words;
        g_table[g_table_n].spirv = k_fixtures[i].spirv;
        g_table[g_table_n].spirv_size = k_fixtures[i].spirv_size;
        g_table_n++;
    }
    qsort(g_table, g_table_n, sizeof g_table[0], hash_cmp);
    for (i = 1; i < g_table_n; i++)
        if (g_table[i - 1].hash >= g_table[i].hash) {
            fail("FAIL: [%s] the table is not strictly ascending by hash: entries %zu and %zu are "
                 "%08X and %08X (the back end binary-searches it)\n", who, i - 1, i,
                 g_table[i - 1].hash, g_table[i].hash);
            return 0;
        }
    for (k = 0; k < NCASE; k++) {
        int p = k_cases[k].prog;
        size_t j;
        g_case_table_idx[k] = -1;
        if (p == skip_prog) continue;
        for (j = 0; j < g_table_n; j++)
            if (g_table[j].hash == g_prog_hash[p]) { g_case_table_idx[k] = (int)j; break; }
        if (g_case_table_idx[k] < 0) {
            fail("FAIL: [%s] case %s (program %s, hash %08X) is not in the table it should be in\n",
                 who, k_cases[k].tag, k_fixtures[p].tag, g_prog_hash[p]);
            return 0;
        }
    }
    return 1;
}

/* ── Stats predicates, separately callable so --negative-control can feed them ── */
static int check_hit_delta(const Nv2aVkProgramStats *b, const Nv2aVkProgramStats *a,
                           uint32_t first_hash, const char *who)
{
    int ok = 1;
    if (a->hits <= b->hits) {
        fail("FAIL: [%s] hits did not advance: %llu -> %llu, though the corpus carries this "
             "program\n", who, (unsigned long long)b->hits, (unsigned long long)a->hits);
        ok = 0;
    }
    if (a->tris <= b->tris) {
        fail("FAIL: [%s] tris did not advance: %llu -> %llu\n", who,
             (unsigned long long)b->tris, (unsigned long long)a->tris);
        ok = 0;
    }
    if (a->const_uploads <= b->const_uploads) {
        fail("FAIL: [%s] const_uploads did not advance: %llu -> %llu (the 192-float4 block per "
             "draw)\n", who, (unsigned long long)b->const_uploads,
             (unsigned long long)a->const_uploads);
        ok = 0;
    }
    if (a->first_hit_hash != first_hash) {
        fail("FAIL: [%s] first_hit_hash is %08X, expected the first program ever drawn (%08X)\n",
             who, a->first_hit_hash, first_hash);
        ok = 0;
    }
    if (a->first_hit_tris < 1u) {
        fail("FAIL: [%s] first_hit_tris is %u, expected >= 1\n", who, a->first_hit_tris);
        ok = 0;
    }
    if (a->identity_fail != b->identity_fail) {
        fail("FAIL: [%s] identity_fail advanced by %llu on a draw whose identity matched\n", who,
             (unsigned long long)(a->identity_fail - b->identity_fail));
        ok = 0;
    }
    return ok;
}

static int check_miss_stats(const Nv2aVkProgramStats *b, const Nv2aVkProgramStats *a,
                            uint32_t mhash, uint32_t mlen, uint32_t mmask, const char *who)
{
    int ok = 1;
    if (a->misses <= b->misses) {
        fail("FAIL: [%s] misses did not advance: %llu -> %llu for a program the corpus does not "
             "carry\n", who, (unsigned long long)b->misses, (unsigned long long)a->misses);
        ok = 0;
    }
    if (a->hits != b->hits) {
        fail("FAIL: [%s] hits advanced by %llu for a program the corpus does not carry\n", who,
             (unsigned long long)(a->hits - b->hits));
        ok = 0;
    }
    if (a->identity_fail != b->identity_fail) {
        fail("FAIL: [%s] identity_fail advanced by %llu for a hash the table does not hold\n",
             who, (unsigned long long)(a->identity_fail - b->identity_fail));
        ok = 0;
    }
    if (a->first_miss_hash != mhash || a->first_miss_len != mlen || a->first_miss_mask != mmask) {
        fail("FAIL: [%s] the first-miss latch is %08X len %u mask 0x%04X, expected %08X len %u "
             "mask 0x%04X\n", who, a->first_miss_hash, a->first_miss_len, a->first_miss_mask,
             mhash, mlen, mmask);
        ok = 0;
    }
    return ok;
}

static int check_ident_stats(const Nv2aVkProgramStats *b, const Nv2aVkProgramStats *a,
                             const char *what, const char *who)
{
    int ok = 1;
    if (a->identity_fail <= b->identity_fail) {
        fail("FAIL: [%s] identity_fail did not advance: %llu -> %llu, though the table entry's %s "
             "was corrupted while its hash still matched\n", who,
             (unsigned long long)b->identity_fail, (unsigned long long)a->identity_fail, what);
        ok = 0;
    }
    if (a->hits != b->hits) {
        fail("FAIL: [%s] hits advanced by %llu: a corrupt identity selected a shader\n", who,
             (unsigned long long)(a->hits - b->hits));
        ok = 0;
    }
    if (a->misses != b->misses) {
        fail("FAIL: [%s] misses advanced by %llu: the hash WAS in the table, so this was not a "
             "miss\n", who, (unsigned long long)(a->misses - b->misses));
        ok = 0;
    }
    return ok;
}

/* ── The run ───────────────────────────────────────────────────────────────
 * One reference per V1..V6 case, plus one for the miss program (MISS_SURF), so the miss's pixels
 * are compared with a reference captured the same way rather than merely "not the clear colour". */
static uint8_t *g_cpu[NCASE];
static double g_perim[NCASE], g_area[NCASE];
static uint8_t *g_miss_cpu;
static double g_miss_perim, g_miss_area;
#define MISS_SURF NCASE
/* The full fixtures beyond the V cases -- the add fixture (X4) and the six op fixtures
 * (X5..X10) -- each have their own reference: they are table hits, so each is compared with a
 * reference captured the same way as every other hit. */
#define NEXTRA   (1 + NOPFIX)
static uint8_t *g_extra_cpu[NEXTRA];
static double g_extra_perim[NEXTRA], g_extra_area[NEXTRA];
static const char *const k_extra_full_tag[NEXTRA] = {
    ADD_TAG, "X5", "X6", "X7", "X8", "X9", "X10",
};
/* Surfaces: NCASE for the miss, then one per extra full fixture. */
#define EXTRA_SURF(i) (NCASE + 1 + (i))
#define ADD_SURF      EXTRA_SURF(0)

/* The miss program's own geometry: it passes v0 through as oPos, so the vertex positions ARE the
 * pixel positions, exactly as case_geometry treats the non-MVP cases. */
static void miss_geometry(const VsCase *c, double *perim, double *area)
{
    double a[3][4];
    int i;
    for (i = 0; i < 3; i++) { a[i][0] = c->v[i].pos[0]; a[i][1] = c->v[i].pos[1]; }
    *perim = hypot(a[1][0] - a[0][0], a[1][1] - a[0][1])
           + hypot(a[2][0] - a[1][0], a[2][1] - a[1][1])
           + hypot(a[0][0] - a[2][0], a[0][1] - a[2][1]);
    *area = fabs(0.5 * ((a[1][0] - a[0][0]) * (a[2][1] - a[0][1])
                      - (a[1][1] - a[0][1]) * (a[2][0] - a[0][0])));
}

static int cpu_pass(void)
{
    int ok = 1, i;
    VsCase mc;

    note("-- software executor (the references) --\n");
    for (i = 0; i < NCASE; i++) {
        VsCase c;
        case_make(k_cases[i].which, &c);
        case_geometry(k_cases[i].which, &c, &g_perim[i], &g_area[i]);
        prefill(SURF(i));
        build_batch(SURF(i), VBUF(i), k_cases[i].which);
        ok &= submit(k_cases[i].tag);
        g_cpu[i] = snapshot(SURF(i), (size_t)PIXELS * 4u);
        note("  %s geometry from the interpreter: perimeter %.0f px, predicted area %.0f px\n",
             k_cases[i].tag, g_perim[i], g_area[i]);
        if (k_cases[i].which == VS_A0)
            note("  %s a0 inputs %.4f, %.4f, %.4f: floor(x+0.001) selects c[%d], c[%d], c[%d]; "
                 "trunc(x) would select c[%d], c[%d], c[%d] -- the ARL rounding is exercised\n",
                 k_cases[i].tag, c.v[0].aux1[0], c.v[1].aux1[0], c.v[2].aux1[0],
                 (int)floorf(c.v[0].aux1[0] + 0.001f) + 20, (int)floorf(c.v[1].aux1[0] + 0.001f) + 20,
                 (int)floorf(c.v[2].aux1[0] + 0.001f) + 20,
                 (int)c.v[0].aux1[0] + 20, (int)c.v[1].aux1[0] + 20, (int)c.v[2].aux1[0] + 20);
    }
    miss_make(&mc);
    miss_geometry(&mc, &g_miss_perim, &g_miss_area);
    prefill(SURF(MISS_SURF));
    build_batch_prog(SURF(MISS_SURF), VBUF(MISS_SURF), &mc, 0);
    ok &= submit(MISS_TAG);
    g_miss_cpu = snapshot(SURF(MISS_SURF), (size_t)PIXELS * 4u);
    note("  %s geometry from the interpreter: perimeter %.0f px, predicted area %.0f px\n",
         MISS_TAG, g_miss_perim, g_miss_area);
    for (i = 0; i < NEXTRA; i++) {
        VsCase ec;
        if (i == 0) add_make(&ec);
        else        op_make(i - 1, &ec);
        miss_geometry(&ec, &g_extra_perim[i], &g_extra_area[i]);
        prefill(SURF(EXTRA_SURF(i)));
        build_batch_prog(SURF(EXTRA_SURF(i)), VBUF(EXTRA_SURF(i)), &ec, 0);
        ok &= submit(k_extra_full_tag[i]);
        g_extra_cpu[i] = snapshot(SURF(EXTRA_SURF(i)), (size_t)PIXELS * 4u);
        note("  %s geometry from the interpreter: perimeter %.0f px, predicted area %.0f px\n",
             k_extra_full_tag[i], g_extra_perim[i], g_extra_area[i]);
    }
    return ok;
}

/* The four-channel comparison is only worth its cost if some fixture's reference actually has a
 * non-opaque alpha. V4 is the case that should: its dst/exp/log results reach the surface only
 * through oD0.w, whose model values are 0.54, 0.64 and 0.49 (alpha 138, 163, 125). Asserted, so a
 * future fixture change that made every reference opaque cannot silently make the alpha half of
 * the comparison vacuous. */
static int check_alpha_coverage(void)
{
    int i, any = 0, ok = 1;
    const char *tags[NCASE + 1 + NEXTRA];
    const uint8_t *refs[NCASE + 1 + NEXTRA];

    for (i = 0; i < NCASE; i++) { tags[i] = k_cases[i].tag; refs[i] = g_cpu[i]; }
    tags[NCASE] = MISS_TAG;
    refs[NCASE] = g_miss_cpu;
    for (i = 0; i < NEXTRA; i++) {
        tags[NCASE + 1 + i] = k_extra_full_tag[i];
        refs[NCASE + 1 + i] = g_extra_cpu[i];
    }
    for (i = 0; i < NCASE + 1 + NEXTRA; i++) {
        unsigned n = 0;
        int x, y;
        if (!refs[i]) continue;
        for (y = 0; y < SURF_H; y++)
            for (x = 0; x < SURF_W; x++) {
                uint32_t v = px32(refs[i], x, y);
                if (covered(v) && (v >> 24) != 0xFFu) n++;
            }
        if (n) any = 1;
        note("  [alpha] %s's CPU reference has %u drawn pixels whose alpha is not 255\n",
             tags[i], n);
    }
    if (!any) {
        fail("FAIL: no fixture's reference has a non-opaque alpha, so the four-channel comparison "
             "covers alpha vacuously: V4's dst/exp/log results reach the surface only through "
             "oD0.w and were expected to be 0.54/0.64/0.49 there\n");
        ok = 0;
    }
    return ok;
}

static int gpu_pass(void)
{
    int ok = 1, i;
    uint8_t *img[NCASE + 1 + NEXTRA];   /* one per V case, one for the miss, one per extra full fixture */
    char *log;
    Nv2aVkProgramStats first;

    memset(img, 0, sizeof img);
    note("-- check 2: corpus programs drawn through the Vulkan program path --\n");

    /* Nothing may have consulted the corpus before the first draw: that is what makes the
     * first-hit and first-miss latches assertions about THIS pass. */
    memset(&first, 0, sizeof first);
    nv2a_vk_backend_program_stats(&first);
    if (first.hits || first.misses || first.identity_fail || first.declines) {
        fail("FAIL: the back end consulted the corpus before the first program draw: hits %llu, "
             "misses %llu, identity_fail %llu, declines %llu\n",
             (unsigned long long)first.hits, (unsigned long long)first.misses,
             (unsigned long long)first.identity_fail, (unsigned long long)first.declines);
        ok = 0;
    }

    for (i = 0; i < NCASE; i++) {
        Nv2aVkProgramStats before, after;
        const char *tag = k_cases[i].tag;
        int p = k_cases[i].prog;

        if (g_case_table_idx[i] < 0) {
            fail("FAIL: [%s] no table entry, so a hit was expected and cannot be drawn\n", tag);
            ok = 0;
            continue;
        }
        memset(&before, 0, sizeof before);
        nv2a_vk_backend_program_stats(&before);
        prefill(SURF(i));
        build_batch(SURF(i), VBUF(i), k_cases[i].which);
        ok &= submit(tag);
        nv2a_vk_backend_program_stats(&after);
        ok &= check_hit_delta(&before, &after, g_prog_hash[0], tag);
        img[i] = snapshot(SURF(i), (size_t)PIXELS * 4u);
        ok &= check_cpu(tag, g_cpu[i], img[i], g_perim[i], g_area[i]);
        note("  [%s] table entry for %s: hash %08X len %u mask 0x%04X digest %016llX; hits %llu -> "
             "%llu, tris %llu -> %llu, const_uploads %llu -> %llu\n", tag, k_fixtures[p].tag,
             g_prog_hash[p], g_prog_len[p], g_prog_mask[p], (unsigned long long)g_prog_digest[p],
             (unsigned long long)before.hits, (unsigned long long)after.hits,
             (unsigned long long)before.tris, (unsigned long long)after.tris,
             (unsigned long long)before.const_uploads, (unsigned long long)after.const_uploads);
        free(img[i]);
        img[i] = NULL;
    }

    /* ── The full fixtures beyond the V cases: the add fixture (X4) and the six MAC-op
     *    fixtures (X5..X10). Each is a table hit drawn on the GPU and pixel-compared, so each
     *    op's own semantics reach a pixel the gate compares. ── */
    for (i = 0; i < NEXTRA; i++) {
        Nv2aVkProgramStats b4, af;
        VsCase ec;
        const int prog = (i == 0) ? ADD_PROG : OP_PROG(i - 1);
        const char *tag = k_extra_full_tag[i];
        size_t j;
        int in_table = 0;

        if (i == 0) add_make(&ec);
        else        op_make(i - 1, &ec);
        for (j = 0; j < g_table_n; j++)
            if (g_table[j].hash == g_prog_hash[prog]) in_table = 1;
        if (!in_table) {
            fail("FAIL: [%s] the fixture (hash %08X) is not in the table, so the MAC op it exists "
                 "for is never drawn on the GPU\n", tag, g_prog_hash[prog]);
            ok = 0;
            continue;
        }
        memset(&b4, 0, sizeof b4);
        nv2a_vk_backend_program_stats(&b4);
        prefill(SURF(EXTRA_SURF(i)));
        build_batch_prog(SURF(EXTRA_SURF(i)), VBUF(EXTRA_SURF(i)), &ec, 0);
        ok &= submit(tag);
        nv2a_vk_backend_program_stats(&af);
        ok &= check_hit_delta(&b4, &af, g_prog_hash[0], tag);
        img[EXTRA_SURF(i)] = snapshot(SURF(EXTRA_SURF(i)), (size_t)PIXELS * 4u);
        ok &= check_cpu(tag, g_extra_cpu[i], img[EXTRA_SURF(i)], g_extra_perim[i],
                        g_extra_area[i]);
        note("  [%s] table entry for %s: hash %08X len %u mask 0x%04X digest %016llX; hits "
             "%llu -> %llu, tris %llu -> %llu, const_uploads %llu -> %llu\n", tag,
             k_fixtures[prog].tag, g_prog_hash[prog], g_prog_len[prog], g_prog_mask[prog],
             (unsigned long long)g_prog_digest[prog], (unsigned long long)b4.hits,
             (unsigned long long)af.hits, (unsigned long long)b4.tris,
             (unsigned long long)af.tris, (unsigned long long)b4.const_uploads,
             (unsigned long long)af.const_uploads);
        free(img[EXTRA_SURF(i)]);
        img[EXTRA_SURF(i)] = NULL;
    }

    /* Two entries of this table share one input_mask and are DIFFERENT programs, and each was
     * compared with its OWN reference above: that is the shared-pipeline defect the V5 gate covers,
     * re-checked here because this table is synthetic. (V1 and V6 share a mask too, but they are
     * the same words, so sharing a pipeline between them is correct and proves nothing.) */
    {
        int j, shared = 0;
        for (i = 0; i < NCASE && !shared; i++)
            for (j = i + 1; j < NCASE && !shared; j++)
                if (g_case_table_idx[i] >= 0 && g_case_table_idx[j] >= 0
                    && g_prog_mask[k_cases[i].prog] == g_prog_mask[k_cases[j].prog]
                    && g_prog_hash[k_cases[i].prog] != g_prog_hash[k_cases[j].prog]) {
                    note("  [mask-share] %s (%08X) and %s (%08X) are different programs that share "
                         "input_mask 0x%04X, and each matched its own reference\n",
                         k_cases[i].tag, g_prog_hash[k_cases[i].prog], k_cases[j].tag,
                         g_prog_hash[k_cases[j].prog], g_prog_mask[k_cases[i].prog]);
                    shared = 1;
                }
        if (!shared) {
            fail("FAIL: no two DISTINCT table entries share an input_mask, so the pipeline-cache "
                 "key's program component is not exercised\n");
            ok = 0;
        }
    }

    /* ── 3. The miss. ── */
    note("-- check 3: a program the corpus does not carry --\n");
    {
        const int mp = MISS_FIX;
        Nv2aVkProgramStats b4, af;
        VsCase miss_case;
        size_t j;
        int clash = 0;

        miss_make(&miss_case);

        for (j = 0; j < g_table_n; j++)
            if (g_table[j].hash == g_prog_hash[mp] || g_table[j].digest == g_prog_digest[mp]) {
                fail("FAIL: [miss] the table carries the miss program: entry %zu hash %08X "
                     "digest %016llX\n", j, g_table[j].hash,
                     (unsigned long long)g_table[j].digest);
                clash = 1;
                ok = 0;
            }
        if (!clash)
            note("  [miss] %s (hash %08X, len %u, mask 0x%04X, digest %016llX) is absent from the "
                 "table by hash, len and digest\n", MISS_TAG, g_prog_hash[mp],
                 g_prog_len[mp], g_prog_mask[mp], (unsigned long long)g_prog_digest[mp]);

        memset(&b4, 0, sizeof b4);
        nv2a_vk_backend_program_stats(&b4);
        prefill(SURF(MISS_SURF));
        build_batch_prog(SURF(MISS_SURF), VBUF(MISS_SURF), &miss_case, 0);
        ok &= submit(MISS_TAG);
        nv2a_vk_backend_program_stats(&af);
        ok &= check_miss_stats(&b4, &af, g_prog_hash[mp], g_prog_len[mp], g_prog_mask[mp], "miss");
        img[MISS_SURF] = snapshot(SURF(MISS_SURF), (size_t)PIXELS * 4u);
        ok &= check_cpu("miss", g_miss_cpu, img[MISS_SURF], g_miss_perim, g_miss_area);
        free(img[MISS_SURF]);
        img[MISS_SURF] = NULL;
        note("  [miss] hits %llu -> %llu, misses %llu -> %llu, identity_fail %llu -> %llu, "
             "declines %llu -> %llu; first_miss_hash %08X len %u mask 0x%04X\n",
             (unsigned long long)b4.hits, (unsigned long long)af.hits,
             (unsigned long long)b4.misses, (unsigned long long)af.misses,
             (unsigned long long)b4.identity_fail, (unsigned long long)af.identity_fail,
             (unsigned long long)b4.declines, (unsigned long long)af.declines,
             af.first_miss_hash, af.first_miss_len, af.first_miss_mask);
    }
    fflush(stderr);
    log = read_log();
    if (!log) {
        fail("FAIL: the test's stderr log could not be read back from %s\n", g_log_path);
        ok = 0;
    } else {
        static const char *const k_miss_line =
            "is not in the ahead-of-time corpus; the CPU interpreter draws it";
        if (!log_has(log, k_miss_line)) {
            fail("FAIL: [miss] the log has no line containing \"%s\": the miss was silent\n",
                 k_miss_line);
            ok = 0;
        } else {
            note("  [miss] the log says the program is not in the ahead-of-time corpus\n");
        }
        free(log);
    }

    /* ── 4. A corrupt identity. ── */
    note("-- check 4: table entries whose identity fields are corrupt --\n");
    {
        static Nv2aVshAotEntry copy[NFIX];
        static const char *const what[2] = { "len", "digest" };
        /* Two DIFFERENT programs, so each corruption gets its own log line (the log is once per
         * hash): V3 (VS_PAIR) and V5 (VS_FOG). */
        static const int ccase[2] = { 2, 4 };
        int round;

        for (round = 0; round < 2; round++) {
            const int ci = ccase[round];
            const int p = k_cases[ci].prog;
            size_t j;
            int found = 0;

            if (g_case_table_idx[ci] < 0) {
                fail("FAIL: [identity/%s] %s has no table entry to corrupt\n", what[round],
                     k_cases[ci].tag);
                ok = 0;
                continue;
            }
            memcpy(copy, g_table, g_table_n * sizeof copy[0]);
            for (j = 0; j < g_table_n; j++)
                if (copy[j].hash == g_prog_hash[p]) {
                    if (round == 0)
                        copy[j].len = copy[j].len - 1u;          /* the hash still matches */
                    else
                        copy[j].digest ^= 1ull;                  /* the hash still matches */
                    found = 1;
                    break;
                }
            if (!found) {
                fail("FAIL: [identity/%s] %s is not in the table\n", what[round], k_cases[ci].tag);
                ok = 0;
                continue;
            }
            nv2a_vk_backend_set_programs(copy, g_table_n);
            {
                Nv2aVkProgramStats b4, af;
                memset(&b4, 0, sizeof b4);
                nv2a_vk_backend_program_stats(&b4);
                prefill(SURF(ci));
                build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
                ok &= submit("corrupt identity");
                nv2a_vk_backend_program_stats(&af);
                ok &= check_ident_stats(&b4, &af, what[round], k_cases[ci].tag);
                img[ci] = snapshot(SURF(ci), (size_t)PIXELS * 4u);
                ok &= check_cpu("identity/corrupt", g_cpu[ci], img[ci], g_perim[ci], g_area[ci]);
                free(img[ci]);
                img[ci] = NULL;
                note("  [identity/%s] %s (hash %08X) still matches the table; hits %llu -> %llu, "
                     "identity_fail %llu -> %llu, misses %llu -> %llu\n", what[round],
                     k_cases[ci].tag, g_prog_hash[p], (unsigned long long)b4.hits,
                     (unsigned long long)af.hits, (unsigned long long)b4.identity_fail,
                     (unsigned long long)af.identity_fail, (unsigned long long)b4.misses,
                     (unsigned long long)af.misses);
            }
            fflush(stderr);
            log = read_log();
            if (!log) {
                fail("FAIL: [identity/%s] the stderr log could not be read back\n", what[round]);
                ok = 0;
            } else {
                if (!log_has(log, "matched hash but not identity")) {
                    fail("FAIL: [identity/%s] the log has no line containing \"matched hash but "
                         "not identity\": the decline was silent\n", what[round]);
                    ok = 0;
                } else {
                    const char *reason = round == 0 ? "the instruction count differs"
                                                    : "the words or digest differ";
                    if (!log_has(log, reason)) {
                        fail("FAIL: [identity/%s] the log does not name the reason \"%s\"\n",
                             what[round], reason);
                        ok = 0;
                    } else {
                        note("  [identity/%s] the log says the hash matched but not the identity "
                             "(%s)\n", what[round], reason);
                    }
                }
                free(log);
            }
        }

        /* Restore the table under test and re-assert the positive control. */
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        {
            Nv2aVkProgramStats b4, af;
            const int ci = ccase[0];
            memset(&b4, 0, sizeof b4);
            nv2a_vk_backend_program_stats(&b4);
            prefill(SURF(ci));
            build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
            ok &= submit("restored table");
            nv2a_vk_backend_program_stats(&af);
            ok &= check_hit_delta(&b4, &af, g_prog_hash[0], "restored");
            img[ci] = snapshot(SURF(ci), (size_t)PIXELS * 4u);
            ok &= check_cpu("restored", g_cpu[ci], img[ci], g_perim[ci], g_area[ci]);
            free(img[ci]);
            note("  [restored] the table under test is back (%zu entries); hits %llu -> %llu, "
                 "identity_fail %llu -> %llu\n", g_table_n, (unsigned long long)b4.hits,
                 (unsigned long long)af.hits, (unsigned long long)b4.identity_fail,
                 (unsigned long long)af.identity_fail);
        }
    }

    /* The back end must be live, or every zero above would be explained by a missing back end. */
    fflush(stderr);
    log = read_log();
    if (!log || !log_has(log, "Vulkan back end registered")) {
        fail("FAIL: the log has no \"Vulkan back end registered\": the program path was never "
             "reachable, so nothing above proves anything about the corpus\n");
        ok = 0;
    }
    free(log);
    return ok;
}

/* ── V7: V1's scene with RECOMP_GPU_VSH=0 (the CPU-transform path) ───────── */
static int v7_pass(void)
{
    int ok = 1;
    Nv2aVkProgramStats b, a;
    uint8_t *img;
    char *log;

    note("-- V7: RECOMP_GPU_VSH=0 -- the executor must not offer the batch to the back end --\n");
    memset(&b, 0, sizeof b);
    nv2a_vk_backend_program_stats(&b);
    prefill(SURF(0));
    build_batch(SURF(0), VBUF(0), k_cases[0].which);
    ok &= submit("v7 program batch");
    nv2a_vk_backend_program_stats(&a);
    note("  [V7] hits %llu, misses %llu, identity_fail %llu, declines %llu -- all must be 0: the "
         "flag disables the program path before the corpus is consulted\n",
         (unsigned long long)a.hits, (unsigned long long)a.misses,
         (unsigned long long)a.identity_fail, (unsigned long long)a.declines);
    if (a.hits || a.misses || a.identity_fail || a.declines) {
        fail("FAIL: [V7] the program path was consulted with RECOMP_GPU_VSH=0: hits %llu, misses "
             "%llu, identity_fail %llu, declines %llu\n", (unsigned long long)a.hits,
             (unsigned long long)a.misses, (unsigned long long)a.identity_fail,
             (unsigned long long)a.declines);
        ok = 0;
    }
    img = snapshot(SURF(0), (size_t)PIXELS * 4u);
    ok &= check_cpu("V7", g_cpu[0], img, g_perim[0], g_area[0]);
    free(img);
    fflush(stderr);
    log = read_log();
    if (!log || !log_has(log, "Vulkan back end registered")) {
        fail("FAIL: [V7] the log has no \"Vulkan back end registered\": the back end was never "
             "installed, so the zero counters prove nothing\n");
        ok = 0;
    }
    free(log);
    return ok;
}

/* ── 5. The negative controls: each predicate fed a real, injected defect ── */
static int negative_control(int which)
{
    switch (which) {
    case 1: {
        /* One differing byte must be caught, with its offset. Injected into the oracle buffer,
         * which is what check 1 compares the tool's bytes against. */
        static char oracle[1u << 20];
        unsigned char *tool = NULL;
        size_t tn = 0;
        uint32_t words[16][4];
        uint32_t len = case_words(VS_MVP, words), h = 0, m = 0, dl = 0;
        int wc = 0, n_oracle, rc, caught;

        note("negative control 1: one byte of the oracle differs from the tool's output\n");
        if (!words_identity((const uint32_t (*)[4])words, len, &h, &m, &dl, &wc)) {
            fail("FAIL: negative control 1: the V1 fixture does not decode\n");
            return 2;
        }
        n_oracle = nv2a_vsh_hlsl((const uint32_t (*)[4])words, len, m, oracle, sizeof oracle);
        rc = run_tool((const uint32_t (*)[4])words, len, m, 0, "nc1", &tool, &tn);
        if (n_oracle <= 0 || rc != 0 || !tool) {
            fail("FAIL: negative control 1: could not produce both byte strings (oracle %d, tool "
                 "exit %d)\n", n_oracle, rc);
            free(tool);
            return 2;
        }
        if (!bytes_equal("nc1 (undamaged)", (const unsigned char *)oracle, (size_t)n_oracle,
                         tool, tn)) {
            fail("FAIL: negative control 1: the undamaged comparison FAILED, so the control "
                 "proves nothing\n");
            free(tool);
            return 2;
        }
        oracle[n_oracle / 2] ^= 0x40;            /* one byte, in memory only */
        caught = !bytes_equal("nc1 (one byte flipped)", (const unsigned char *)oracle,
                              (size_t)n_oracle, tool, tn);
        free(tool);
        if (!caught) {
            fail("FAIL: negative control 1: the byte comparison PASSED a one-byte difference, so "
                 "check 1 is not load-bearing\n");
            note("negative control 1: check 1 MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 1: check 1 FAILED the corrupted bytes, as it must\n");
        return 1;
    }
    case 2: {
        /* An empty corpus: the hit predicate must fail. This is the production state "the corpus
         * was never registered", which is what the generator's absence looks like. */
        Nv2aVkProgramStats b, a;
        const int ci = 0;                        /* V1 */
        int caught;

        note("negative control 2: an empty corpus handed to the back end\n");
        if (!table_build(MISS_FIX, "nc2")) return 2;
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(ci));
        build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
        if (!submit("nc2 (undamaged)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        if (!check_hit_delta(&b, &a, g_prog_hash[0], "nc2 (undamaged)")) {
            fail("FAIL: negative control 2: the hit predicate rejects the real draw, so the "
                 "control proves nothing\n");
            return 2;
        }
        nv2a_vk_backend_set_programs(NULL, 0);   /* in memory only, never on disk */
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(ci));
        build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
        if (!submit("nc2 (empty corpus)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        caught = !check_hit_delta(&b, &a, g_prog_hash[0], "nc2 (empty corpus)");
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        if (!caught) {
            fail("FAIL: negative control 2: the hit predicate PASSED a draw with no corpus, so "
                 "check 2 is not load-bearing\n");
            note("negative control 2: check 2 MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 2: check 2 FAILED the empty corpus, as it must\n");
        return 1;
    }
    case 3: {
        /* A corpus that DOES carry the program that must miss: the miss predicate must fail. */
        Nv2aVkProgramStats b, a;
        VsCase mc;
        const int p = MISS_FIX;
        int caught;

        miss_make(&mc);
        note("negative control 3: a corpus that carries the miss program\n");
        if (!table_build(MISS_FIX, "nc3 (undamaged)")) return 2;
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(MISS_SURF));
        build_batch_prog(SURF(MISS_SURF), VBUF(MISS_SURF), &mc, 0);
        if (!submit("nc3 (undamaged)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        if (!check_miss_stats(&b, &a, g_prog_hash[p], g_prog_len[p], g_prog_mask[p],
                              "nc3 (undamaged)")) {
            fail("FAIL: negative control 3: the miss predicate rejects the real miss, so the "
                 "control proves nothing\n");
            return 2;
        }
        if (!table_build(-1, "nc3 (carried)")) return 2;      /* the program is now IN the table */
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(MISS_SURF));
        build_batch_prog(SURF(MISS_SURF), VBUF(MISS_SURF), &mc, 0);
        if (!submit("nc3 (carried)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        caught = !check_miss_stats(&b, &a, g_prog_hash[p], g_prog_len[p], g_prog_mask[p],
                                   "nc3 (carried)");
        if (!table_build(MISS_FIX, "nc3 restore")) return 2;
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        if (!caught) {
            fail("FAIL: negative control 3: the miss predicate PASSED a draw whose program the "
                 "corpus carries, so check 3 is not load-bearing\n");
            note("negative control 3: check 3 MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 3: check 3 FAILED the carried program, as it must\n");
        return 1;
    }
    case 4: {
        /* An intact identity check: the identity predicate must fail, because a correct table
         * means identity_fail does not advance. This is the production change "the identity
         * check was removed". */
        Nv2aVkProgramStats b, a;
        const int ci = 2;                        /* V3 */
        int caught;

        note("negative control 4: an intact table drawn where a corrupt one was expected\n");
        if (!table_build(MISS_FIX, "nc4 (intact)")) return 2;
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(ci));
        build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
        if (!submit("nc4 (intact)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        caught = !check_ident_stats(&b, &a, "len", "nc4 (intact)");
        if (!caught) {
            fail("FAIL: negative control 4: the identity predicate PASSED a draw whose identity "
                 "was intact, so check 4 is not load-bearing\n");
            note("negative control 4: check 4 MISSED the simulated defect\n");
            return 2;
        }
        if (a.hits <= b.hits) {
            fail("FAIL: negative control 4: the draw with an intact table did not hit (hits %llu "
                 "-> %llu), so the control does not isolate the identity check\n",
                 (unsigned long long)b.hits, (unsigned long long)a.hits);
            note("negative control 4: check 4 MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 4: check 4 FAILED the intact identity, as it must\n");
        return 1;
    }
    case 5: {
        /* check_cpu is the predicate behind checks 2, 3 and 4, and nothing else in this file feeds
         * it a wrong image: if it returned 1 unconditionally, every registered case would still
         * pass. This control makes it fail for a real reason. V3 (VS_PAIR) and V5 (VS_FOG) share
         * input_mask 0x000B, so V5's shader is legal in V3's pipeline -- the vertex layout and the
         * shader's input locations agree -- and the identity check still passes, because only the
         * SPIR-V pointer is swapped and the entry's words are untouched.
         *
         * ORDER MATTERS. The back end's pipeline cache is keyed by (prog, program hash, input_mask)
         * and does NOT carry the shader's identity, so once a key has been built the swap is
         * invisible for that key. The borrowed-shader draw therefore has to be the FIRST draw of
         * V3's key in this process, which is why the undamaged pre-check below uses V1's key (a
         * different program) rather than V3's. That is a property of the cache key, not a defect:
         * in production the key's hash is what the table was looked up by and identity-checked, so
         * a wrong shader can only reach the cache through a table whose spirv field is wrong. */
        static Nv2aVshAotEntry copy[NFIX];
        Nv2aVkProgramStats b, a;
        const int pre = 0;                       /* V1: a different pipeline key */
        const int ci = 2;                        /* V3: the entry whose shader is swapped */
        const int pv = 4;                        /* V5: the program whose shader is borrowed */
        uint8_t *img;
        int caught, found = 0;
        size_t j;

        note("negative control 5: V3's table entry pointed at V5's SPIR-V\n");
        if (!table_build(MISS_FIX, "nc5 (undamaged)")) return 2;

        /* First check_cpu must ACCEPT a real draw, or the control proves nothing. */
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        prefill(SURF(pre));
        build_batch(SURF(pre), VBUF(pre), k_cases[pre].which);
        if (!submit("nc5 (undamaged)")) return 2;
        img = snapshot(SURF(pre), (size_t)PIXELS * 4u);
        if (!check_cpu("nc5 (undamaged)", g_cpu[pre], img, g_perim[pre], g_area[pre])) {
            fail("FAIL: negative control 5: check_cpu rejects the UNDAMAGED draw, so the control "
                 "does not isolate the comparison\n");
            free(img);
            return 2;
        }
        free(img);

        /* Now V3's entry, with V5's shader and V3's own words: the identity still matches. */
        memcpy(copy, g_table, g_table_n * sizeof copy[0]);
        for (j = 0; j < g_table_n; j++)
            if (copy[j].hash == g_prog_hash[k_cases[ci].prog]) {
                if (g_prog_mask[k_cases[ci].prog] != g_prog_mask[k_cases[pv].prog]) {
                    fail("FAIL: negative control 5: V3 and V5 must share an input_mask for this "
                         "control (they are 0x%04X and 0x%04X)\n",
                         g_prog_mask[k_cases[ci].prog], g_prog_mask[k_cases[pv].prog]);
                    return 2;
                }
                copy[j].spirv = k_fixtures[pv].spirv;
                copy[j].spirv_size = k_fixtures[pv].spirv_size;
                found = 1;
                break;
            }
        if (!found) {
            fail("FAIL: negative control 5: V3 is not in the table\n");
            return 2;
        }
        nv2a_vk_backend_set_programs(copy, g_table_n);
        memset(&b, 0, sizeof b);
        nv2a_vk_backend_program_stats(&b);
        prefill(SURF(ci));
        build_batch(SURF(ci), VBUF(ci), k_cases[ci].which);
        if (!submit("nc5 (borrowed shader)")) return 2;
        nv2a_vk_backend_program_stats(&a);
        if (a.hits <= b.hits || a.identity_fail != b.identity_fail) {
            fail("FAIL: negative control 5: the borrowed shader did not hit cleanly (hits %llu -> "
                 "%llu, identity_fail %llu -> %llu), so the defect under test is not the pixels\n",
                 (unsigned long long)b.hits, (unsigned long long)a.hits,
                 (unsigned long long)b.identity_fail, (unsigned long long)a.identity_fail);
            nv2a_vk_backend_set_programs(g_table, g_table_n);
            return 2;
        }
        img = snapshot(SURF(ci), (size_t)PIXELS * 4u);
        caught = !check_cpu("nc5 (borrowed shader)", g_cpu[ci], img, g_perim[ci], g_area[ci]);
        free(img);
        nv2a_vk_backend_set_programs(g_table, g_table_n);
        if (!caught) {
            fail("FAIL: negative control 5: check_cpu PASSED a draw made with another program's "
                 "shader, so checks 2, 3 and 4 are not load-bearing\n");
            note("negative control 5: check_cpu MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 5: check_cpu FAILED the borrowed shader, as it must\n");
        return 1;
    }
    case 6: {
        /* The output-location VALUES. Move t1 from location 4 to 7 in a copy of the tool's --vk
         * text and feed it to check_output_locations: the location COUNT is unchanged, and the
         * combiner reads d0 alone so the pixels would be unchanged too, which is exactly why the
         * by-name assertion is needed. No device, so this runs like control 1. */
        unsigned char *vk = NULL;
        size_t vn = 0;
        uint32_t words[16][4];
        uint32_t len = case_words(VS_MVP, words), h = 0, m = 0, dl = 0;
        int wc = 0, rc, caught = 0, untouched_ok;
        char *text, *hit;

        note("negative control 6: one output varying moved to a different location\n");
        if (!words_identity((const uint32_t (*)[4])words, len, &h, &m, &dl, &wc)) {
            fail("FAIL: negative control 6: the V1 fixture does not decode\n");
            return 2;
        }
        rc = run_tool((const uint32_t (*)[4])words, len, m, 1, "nc6", &vk, &vn);
        if (rc != 0 || !vk) {
            fail("FAIL: negative control 6: the tool produced no --vk output (exit %d)\n", rc);
            free(vk);
            return 2;
        }
        text = (char *)malloc(vn + 1u);
        if (!text) { fail("FAIL: negative control 6: out of memory\n"); free(vk); return 2; }
        memcpy(text, vk, vn);
        text[vn] = '\0';
        free(vk);

        /* The undamaged text must pass, or the control proves nothing. */
        untouched_ok = check_output_locations("nc6 (undamaged)", text);
        if (!untouched_ok) {
            fail("FAIL: negative control 6: the undamaged --vk output FAILED the location check, "
                 "so the control proves nothing\n");
            free(text);
            return 2;
        }
        /* Move t1: "[[vk::location(4)]] float4 t1 : TEXCOORD1;" -> location 7. Only that one
         * declaration changes, so the location count is identical. */
        hit = strstr(text, "[[vk::location(4)]] float4 t1 : TEXCOORD1;");
        if (!hit) {
            fail("FAIL: negative control 6: the --vk output has no t1 at location 4 to move\n");
            free(text);
            return 2;
        }
        /* Patch the digit in place: location 4 -> 7. */
        hit[strlen("[[vk::location(")] = '7';
        caught = !check_output_locations("nc6 (t1 moved to 7)", text);
        free(text);
        if (!caught) {
            fail("FAIL: negative control 6: the location check PASSED an output varying moved "
                 "from location 4 to 7, so check 1's output-location assertion is not "
                 "load-bearing\n");
            note("negative control 6: check 1 MISSED the simulated defect\n");
            return 2;
        }
        note("negative control 6: check 1 FAILED the moved output location, as it must\n");
        return 1;
    }
    default:
        fail("FAIL: unknown negative control %d (expected 1..6)\n", which);
        return 2;
    }
}

int main(int argc, char **argv)
{
    const uint32_t ram_size = 64u * 1024u * 1024u;
    uint8_t *ram = (uint8_t *)calloc(1, ram_size);
    uint8_t *ramin = (uint8_t *)calloc(1, 1024u * 1024u);
    uint8_t *instance = (uint8_t *)calloc(1, 0x20000u);
    static uint8_t xbe[0x2000];
    uint8_t *pbmem;
    const char *tool_override = NULL;
    char *log = NULL;
    int ok = 1, i, neg = 0, v7_only = 0;

    /* --skip must run NO check at all, so it is handled before anything else. */
    for (i = 1; i < argc; i++)
        if (strcmp(argv[i], "--skip") == 0) {
            if (argc != 3) {
                fprintf(stderr, "FAIL: --skip takes exactly one reason and runs no checks\n");
                return 1;
            }
            printf("SKIP: vsh_conformance -- %s (no check was run)\n", argv[i + 1]);
            return 77;
        }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--v7-only") == 0) {
            v7_only = 1;
        } else if (strcmp(argv[i], "--negative-control") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "FAIL: --negative-control needs an argument\n"); return 1; }
            neg = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tool") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "FAIL: --tool needs a path\n"); return 1; }
            tool_override = argv[++i];
        } else if (strcmp(argv[i], "--log") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "FAIL: --log needs a path\n"); return 1; }
            _snprintf(g_log_path, sizeof g_log_path, "%s", argv[++i]);
            g_log_path[sizeof g_log_path - 1] = '\0';
        } else {
            fprintf(stderr, "FAIL: unknown option %s\n", argv[i]);
            return 1;
        }
    }
    if (neg < 0 || neg > 6) { fail("FAIL: unknown negative control %d (expected 1..6)\n", neg); return 1; }
    if (neg != 0 && v7_only) {
        fail("FAIL: --negative-control and --v7-only are separate processes: the controls need the "
             "program path ENABLED, and --v7-only needs it disabled\n");
        return 1;
    }
    if (!start_stderr_capture()) {
        /* The miss/identity "not silent" assertions read this file back. Without it they would
         * either be skipped (a silent hole) or read a stale file (a false pass), so this is a
         * hard failure. */
        fail("FAIL: the stderr capture could not be opened at %s: the miss and identity log "
             "assertions depend on it, so this is a failure rather than a skipped check\n",
             g_log_path);
        return 1;
    }
    note("  [log] stderr is captured at %s\n", g_log_path);
    {
        const char *tmp = getenv("TEMP");
        if (!tmp || !tmp[0]) tmp = getenv("TMP");
        if (!tmp || !tmp[0]) tmp = ".";
        _snprintf(g_work, sizeof g_work, "%s\\vsh_conformance", tmp);
        g_work[sizeof g_work - 1] = '\0';
        _mkdir(g_work);
    }

    pbmem = (uint8_t *)VirtualAlloc(NULL, 0x4000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!ram || !ramin || !instance || !pbmem) { fail("FAIL: allocation\n"); return 1; }
    g_pb = (uint32_t *)pbmem;

    /* Guest RAM: a synthetic, section-less XBE is enough for the memory layout. */
    *(uint32_t *)(xbe + 0x0104) = XBOX_BASE_ADDRESS;
    *(uint32_t *)(xbe + 0x0108) = 0x1000;
    *(uint32_t *)(xbe + 0x0120) = XBOX_BASE_ADDRESS + 0x200;
    if (!xbox_MemoryLayoutInit(xbe, sizeof xbe)) { fail("FAIL: xbox_MemoryLayoutInit\n"); return 1; }

    nv2a_reset_standalone_for_test();
    g_gpu = nv2a_init_standalone(ram, ram_size, ramin, 1024u * 1024u);
    if (!g_gpu) { fail("FAIL: nv2a_init_standalone\n"); return 1; }
    if (!nv2a_set_pushbuffer_window(g_gpu, pbmem, 0, 0x4000)) { fail("FAIL: pushbuffer window\n"); return 1; }
    if (!nv2a_bind_instance_memory(0x83fe0000u, instance, 0x20000u)) { fail("FAIL: instance memory\n"); return 1; }
    {
        uint32_t *pair = (uint32_t *)(instance + 0x6u * 8u);
        uint32_t *object = (uint32_t *)(instance + (0x300u << 4));
        pair[0] = 0x6u; pair[1] = NV_RAMHT_STATUS | 0x300u;
        object[0] = 0x97u; object[1] = object[2] = object[3] = 0;
    }
    _putenv_s("RECOMP_PB_EXEC", "1");
    nv2a_pb_exec_register_commit_consumer();
    if (!nv2a_pb_exec_consumer_registered()) { fail("FAIL: executor not registered\n"); return 1; }
    nv2a_backend_register(NULL);                       /* the CPU pass first: the installer is one-way */

    if (v7_only) {
        const char *e = getenv("RECOMP_GPU_VSH");
        if (!e || strcmp(e, "0") != 0) {
            fail("FAIL: --v7-only needs RECOMP_GPU_VSH=0 in the environment, or it is not the V7 "
                 "configuration at all\n");
            return 1;
        }
    }

    /* The extracted encoder's own self-check (fixtures vs a double-precision model) is a
     * prerequisite for trusting anything below. */
    note("-- the extracted V1..V7 encoder vs the interpreter and a double-precision model --\n");
    if (!vs_selfcheck()) {
        fail("FAIL: the extracted fixture encoder does not agree with its model, so no fixture "
             "below can be trusted\n");
        ok = 0;
    }
    if (g_vs_enc_conflict) {
        fail("FAIL: the extracted encoder flagged a source conflict while building the fixtures\n");
        ok = 0;
    }

    /* Check 1 needs the host tool; it runs before any submission, so the V7 row's child
     * environment cannot leak into this process's executor. */
    if (!resolve_tool(tool_override))
        ok = 0;
    else
        ok &= check_byte_equality();
    {
        const char *e = getenv("RECOMP_GPU_VSH");
        if (v7_only) {
            if (!e || strcmp(e, "0") != 0) {
                fail("FAIL: RECOMP_GPU_VSH is not 0 after check 1, so this process is no longer "
                     "the V7 configuration\n");
                ok = 0;
            }
        } else if (e && strcmp(e, "0") == 0) {
            fail("FAIL: RECOMP_GPU_VSH is 0 before the executor's first program batch: the program "
                 "path would be disabled and every expected miss would be a decline\n");
            ok = 0;
        }
    }

    /* The fixture identities, recomputed from the fixtures' own words. */
    note("-- the synthetic corpus's fixtures (identity from nv2a_vsh_program_info) --\n");
    for (i = 0; i < NFIX; i++) {
        ok &= fixture_identity(i);
        note("  [fixture] %s len %2u  mask 0x%04X  hash %08X  digest %016llX  "
             "words_hash %016llX  spirv %u bytes (%016llX)\n",
             k_fixtures[i].tag, k_fixtures[i].len, k_fixtures[i].input_mask, g_prog_hash[i],
             (unsigned long long)g_prog_digest[i],
             (unsigned long long)k_fixtures[i].words_bytes_hash,
             (unsigned)k_fixtures[i].spirv_size, (unsigned long long)k_fixtures[i].spirv_hash);
        note("            %s\n", k_fixture_what[i]);
    }
    if (!ok) {
        fail("FAIL: a prerequisite check failed, so the corpus checks cannot be run\n");
        fflush(stdout);
        return 1;
    }

    /* The taxonomy fact the table relies on: V6 is V1's program. */
    {
        uint32_t w6[16][4], w1[16][4], h6 = 0, m6 = 0, d6 = 0, h1 = 0, m1 = 0, d1 = 0;
        int wc = 0, same;
        uint32_t l6 = case_words(VS_BEHIND, w6), l1 = case_words(VS_MVP, w1);
        words_identity((const uint32_t (*)[4])w6, l6, &h6, &m6, &d6, &wc);
        words_identity((const uint32_t (*)[4])w1, l1, &h1, &m1, &d1, &wc);
        same = (l6 == l1) && (h6 == h1) && (m6 == m1) && memcmp(w6, w1, (size_t)l1 * 16u) == 0;
        note("  [taxonomy] V6 (VS_BEHIND) %s V1's program: len %u/%u, mask 0x%04X/0x%04X, hash "
             "%08X/%08X -- so V6 is drawn through V1's entry, and check 1's V6 row must equal its "
             "V1 row\n", same ? "IS" : "is NOT", l6, l1, m6, m1, h6, h1);
        if (!same) {
            fail("FAIL: V6 was expected to be V1's program with different constants and vertices "
                 "(the plan's taxonomy), but its words differ: len %u/%u mask 0x%04X/0x%04X hash "
                 "%08X/%08X. The table and the report assume they are one program.\n",
                 l6, l1, m6, m1, h6, h1);
            ok = 0;
        }
    }

    /* The fixtures must not be retail programs, or "synthetic V1..V7-style" would be false. */
    for (i = 0; i < NFIX; i++) {
        size_t j;
        for (j = 0; j < nv2a_vsh_program_count; j++) {
            const Nv2aVshAotEntry *e = &nv2a_vsh_programs[j];
            if (e->hash == g_prog_hash[i]
                || nv2a_vsh_aot_digest(e->words, e->len) == g_prog_digest[i]) {
                fail("FAIL: [fixture %s] collides with retail corpus entry %zu (hash %08X): these "
                     "fixtures are meant to be synthetic\n", k_fixtures[i].tag, j, e->hash);
                ok = 0;
            }
        }
    }
    note("  [fixtures] none of the %d fixture programs is in the %zu-entry retail corpus (checked "
         "by hash and by digest recomputed from the retail words)\n", NFIX,
         nv2a_vsh_program_count);

    /* The miss program must differ from every V1..V6 program, or "absent from the table" would be
     * satisfied by a duplicate of one that IS in it. */
    for (i = 0; i < NPROG; i++)
        if (g_prog_hash[MISS_FIX] == g_prog_hash[i] || g_prog_digest[MISS_FIX] == g_prog_digest[i]) {
            fail("FAIL: [miss] the miss program (%08X) is the same program as fixture %s (%08X)\n",
                 g_prog_hash[MISS_FIX], k_fixtures[i].tag, g_prog_hash[i]);
            ok = 0;
        }
    note("  [miss] the miss program's words differ from all %d V1..V6 programs (hash %08X vs "
         "their %08X.., digest %016llX)\n", NPROG, g_prog_hash[MISS_FIX], g_prog_hash[0],
         (unsigned long long)g_prog_digest[MISS_FIX]);

    /* The table under test: every fixture except the miss program. */
    if (!table_build(MISS_FIX, "table under test")) {
        fail("FAIL: the table under test could not be built\n");
        fflush(stdout);
        return 1;
    }
    note("  [table] %zu entries, ascending by hash; %s (%s) is deliberately absent so it can be "
         "the miss. Every V1..V6 case is IN the table, V2 included: its a0-relative indexing is "
         "drawn on the GPU and pixel-compared like the rest.\n", g_table_n,
         k_fixtures[MISS_FIX].tag, k_fixture_what[MISS_FIX]);

    /* ── 5. Negative controls 1 and 6 need no device: they are about check 1's byte comparison
     * and its output-location assertion. ── */
    if (neg == 1 || neg == 6) {
        int rc = negative_control(neg);
        fflush(stdout);
        printf("negative control %d: exit %d (%s)\n", neg, rc,
               rc == 1 ? "check 1 correctly FAILED the simulated defect"
                       : "check 1 MISSED the simulated defect: the check is not load-bearing");
        fflush(stdout);
        return rc;
    }

    /* ── The CPU reference pass, then the Vulkan back end. ── */
    ok &= cpu_pass();
    if (!ok) {
        fail("FAIL: the CPU reference pass failed, so the corpus checks cannot be run\n");
        fflush(stdout);
        return 1;
    }

    ok &= check_alpha_coverage();

    if (!getenv("RECOMP_VK_DEVICE")) _putenv_s("RECOMP_VK_DEVICE", "llvmpipe");
    if (!getenv("RECOMP_GPU_BACKEND")) _putenv_s("RECOMP_GPU_BACKEND", "vulkan");
    fflush(stdout);
    if (nv2a_vk_backend_install() != 0) {
        /* A skip may only mean "the device is unavailable". Everything that ran before this point
         * has already reached a verdict, and a failing check is a failure, not a skip: CTest counts
         * 77 as Not Run, so skipping here would let a broken fixture stay green. */
        if (!ok) {
            fail("FAIL: vsh_conformance: a check failed before the Vulkan back end could be "
                 "installed, so this is a FAILURE, not a skip\n");
            fflush(stdout);
            return 1;
        }
        note("SKIP: vsh_conformance -- the Vulkan back end could not be installed on a software "
             "device (no Vulkan, no matching device, or an elevated process: the loader ignores "
             "VK_DRIVER_FILES there). Everything that ran before this point passed. This is a "
             "SKIP, not a pass.\n");
        fflush(stdout);
        return 77;
    }
    nv2a_vk_backend_set_programs(g_table, g_table_n);

    /* ── 5. Negative controls 2..5 need the live back end and the real table. ── */
    if (neg >= 2) {
        int rc = negative_control(neg);
        fflush(stdout);
        printf("negative control %d: exit %d (%s)\n", neg, rc,
               rc == 1 ? "check correctly FAILED the simulated defect"
                       : "check MISSED the simulated defect: the check is not load-bearing");
        fflush(stdout);
        return rc;
    }

    ok &= v7_only ? v7_pass() : gpu_pass();

    /* The process ends in the production state: the retail corpus, not the synthetic one. */
    nv2a_vk_backend_set_programs(nv2a_vsh_programs, nv2a_vsh_program_count);
    note("  [final] the back end holds the retail corpus again (%zu entries)\n",
         nv2a_vsh_program_count);

    fflush(stdout);
    nv2a_set_commit_consumer(NULL);
    for (i = 0; i < NCASE; i++) free(g_cpu[i]);
    VirtualFree(pbmem, 0, MEM_RELEASE);
    free(instance); free(ramin); free(ram);
    xbox_MemoryLayoutShutdown();
    free(log);

    if (!ok) { fail("FAIL: vsh_conformance\n"); return 1; }
    puts("PASS: vsh_conformance (the host tool's bytes are nv2a_vsh_hlsl's, a corpus hit matches "
         "the CPU interpreter's pixels, a miss and a corrupt identity fall back to the CPU, and "
         "V7 with RECOMP_GPU_VSH=0 never consults the corpus)");
    return 0;
}

#endif /* VSH_CONFORMANCE_EMIT_FIXTURES */
