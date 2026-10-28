/* The NV2A fixed-function SPECULAR deciding gate: the cases and their literal
 * expectations, shared by the committed-stream CPU half (tests/test_spec_stream.c)
 * and the Vulkan half (tests/test_spec_stream_vk.c).
 *
 * Each half #includes this file and supplies run_case(c), which submits c->draw
 * and checks every probe against the literals below; the SAME hand-derived
 * literal is checked by each renderer independently. Agreement between the two
 * is never the test. No expectation is read from a production helper or a probe.
 *
 * ══ THE RULE, as the PINNED primary source states it ══════════════════════════
 *
 * Verified against xemu `478b4f496102379c7eaa7f3ec10e714a703c4300` (read for
 * facts only; no code copied). Paths are relative to the xemu root.
 *
 * V1. With lighting ON, the specular output starts at rgb 0 and the vertex
 *     specular attribute's ALPHA:
 *       vsh-ff.c:288  `oD1 = vec4(0.0, 0.0, 0.0, specular.a);`
 *     where `#define specular v4` (vsh-ff.c:77) is vertex attribute 4, and
 *     `#define diffuse v3` (vsh-ff.c:76) is attribute 3.
 *
 * V2. The per-light specular term is 0 whenever either cosine is <= 0:
 *       vsh-ff.c:332  `nDotVP = max(0.0, dot(tNormal, lightDirection));`
 *       vsh-ff.c:340  `nDotHV = max(0.0, dot(tNormal, lightInfiniteHalfVector[i]));`
 *       vsh-ff.c:370-376  `if (nDotVP == 0.0 || nDotHV == 0.0) pf = 0.0; else pf = pow(...)`
 *       vsh-ff.c:379  `lightSpecular = lightSpecularColor(i) * attenuation * pf;`
 *     For an INFINITE light attenuation is exactly 1.0 (vsh-ff.c:330). So the
 *     term is 0 when N.L <= 0 OR N.H <= 0, and needs NO specular parameter.
 *     NOTE the pinned `478b4f49` reads as a `pow(nDotHV, specularPower)` form.
 *     xemu master supersedes it: commit `5c516ff0` (2026-10-17, "nv2a: Fix
 *     specular param handling") replaces the curve-fit approximation with the
 *     calculation of US6417851B1, citing nxdk_pgraph_tests `specular_tests.cpp`
 *     and hardware golden results. Master's rule, with (L, M, N) =
 *     SET_SPECULAR_PARAMS: pf = 0; if (nDotVP > 0 && nDotHV > -L) {
 *     denom = M*nDotHV + N; if (denom == 0) pf = 10000; else if (denom > 0)
 *     pf = clamp((nDotHV + L)/denom, 0, 10000); }. So the packet sketch's
 *     ratio form was right for master and the 10000 clamp exists there.
 *     V2's zero-term claim holds under both forms: N.L <= 0 gives pf = 0.
 *
 * V3. SPECULAR_ENABLE off forces the whole specular output:
 *       vsh-ff.c:424-426  `if (!state->specular_enable) oD1 = vec4(0,0,0,1);`
 *     state->specular_enable is NV_PGRAPH_CSV0_C bit 16 (vsh.c:118-119),
 *     written by SET_SPECULAR_ENABLE 0x03B8 (pgraph.c:1647-1649). It is applied
 *     AFTER the lighting block, so it overrides both V1's alpha and every
 *     per-light contribution. Alpha becomes 1.
 *
 * V4. SEPARATE_SPECULAR is LIGHT_CONTROL (0x0294) bit 0
 *     (nv2a_regs.h:907 `..._SEPARATE_SPECULAR 1`; pgraph.c:1082-1085). When it
 *     is SET, the `!separate_specular` branch (vsh-ff.c:428-439) is skipped, so
 *     nothing is added into oD0 and oD1 keeps the lighting block's value. When
 *     it is CLEAR and lighting is on, `oD0.xyz += oD1.xyz` and then
 *     `oD1 = specular` (the vertex attribute) - see P5.
 *
 * V5. With lighting OFF:
 *       vsh-ff.c:254-256  `oD0 = diffuse; oD1 = specular;`
 *     i.e. both vertex attributes pass straight through, with no specular
 *     enable/separate handling at all (those branches run afterwards but
 *     V3/V4 still apply to them: the `!specular_enable` override is outside the
 *     lighting `else`, so it also rewrites oD1 here).
 *
 * V6. The vertex specular attribute never enters LIT oD1.rgb. The lighting
 *     block writes oD1.rgb from `lightSpecular` only (vsh-ff.c:400-413, all
 *     three `specular_src` cases multiply a light colour or the diffuse
 *     attribute), and SET_COLOR_MATERIAL 0x0298 = 0 selects
 *     MATERIAL_COLOR_SRC_MATERIAL (CSV0_C bits 19-20 = 0), which uses the light
 *     colour alone (vsh-ff.c:401-404). `specular` (attribute 4) appears in the
 *     lighting block ONLY at vsh-ff.c:288 (alpha) and in the two
 *     MATERIAL_COLOR_SRC_SPECULAR cases, which 0x0298 = 0 does not select.
 *
 * V7. The observer. The final combiner is
 *       fragColor.rgb = D + mix(C, B, A);  fragColor.a = G
 *     (psh.c:604-607; the CPU reference nv2a_combiner.c:186-230 is the same
 *     form). CW0 packs A<<24 | B<<16 | C<<8 | D (psh.c:1543-1551). Register
 *     nibble 4 = V0 = oD0 and 5 = V1 = oD1 (psh_regs.h:74-75; psh.c:327-330;
 *     `vec4 v0 = pD0; vec4 v1 = pD1;` psh.c:1085-1086). CW1's G field is bits
 *     8-15 and 0x10 in an input byte selects the alpha channel (psh.c:284,
 *     psh_regs.h:130). So with A = B = 0 the combiner emits exactly D + C.
 *     A zero COMBINER_CONTROL stage count still runs the final stage
 *     (psh.c:1605 `final_input.enabled = final_inputs_0 || final_inputs_1`).
 *
 * ── What this gate can and cannot decide ─────────────────────────────────────
 * V1, V2, V3, V4(separate half) and V6 need NO SET_SPECULAR_PARAMS word: the
 * exponent only scales a term that is already 0, or the whole output is
 * overridden. So the cases below decide:
 *   - lit oD1.rgb is 0 when SPECULAR_ENABLE is off          (V3, V6)
 *   - lit oD1.rgb is 0 when N.L <= 0                        (V1, V2, V6)
 *   - with separate specular on, oD0 is unchanged by them   (V4)
 * and they pin the positive controls that prove the observer really sees oD1 as
 * distinct from oD0.
 *
 * The cases that need the exponent itself (pf > 0) or the other LIGHT_CONTROL
 * bits are listed under PENDING at the bottom. They are NOT written here,
 * because SET_SPECULAR_PARAMS 0x09E0..0x09F4 is not admitted by the walk: it is
 * absent from the toolkit's generated table (nv2a_method_table.c, class 0x97:
 * 0x09D0..0x09DC then 0x09F8/0x09FC - no 0x09E0..0x09F4). A stream carrying it
 * is rejected whole (ledger L39), so such a case could only be written after a
 * genuine runtime witness admits those methods.
 *
 * ── The retail light-0 witness, and why the sketch's H is right ──────────────
 * config/nv2a-runtime-witnessed-methods.json (run
 * 20261008-001451-422-title008-witness-0298, TR §31.62/L52) records light 0:
 *   specular 0x1018..0x1020 = 0            (params 00000000)
 *   0x1024 (local range)   = 7149F2CA      = 1.0e30
 *   half vector 0x1028..0x1030 = 3E0CAE50 3EB5D37D BF6CB6C2
 *                            = (0.13738370, 0.35512915, -0.92466366)
 *   direction   0x1034..0x103C = 3E821555 3F282109 BF35C3A0
 *                            = (0.25406900, 0.65675408, -0.71001625)
 * normalize(dir + (0, 0, -1)) = (0.13738413, 0.35513024, -0.92466649), which
 * matches the delivered half vector to 2.8e-6 - so JSRF's H is
 * normalize(L + VPeye) with the viewer direction (0,0,-1), the LOCALEYE form,
 * even though the light is INFINITE. This is recorded as a fact about the
 * shipped state; this gate does not depend on it (each case delivers H
 * explicitly) and does not yet decide the LOCALEYE branch (P4).
 *
 * ── The fixture (tests/spec_stream_fixture.c) ────────────────────────────────
 * 32x32 linear A8R8G8B8, identity composite (positions are pixels whatever the
 * model-view is), one quad at z 0.5 / w 1, six vertices sharing one normal, so
 * the colour is flat. The model-view only feeds e, the eye position. Probes
 * (4,4) (upper-left triangle), (28,28) and (24,10) (lower-right triangle) are
 * well inside the quad.
 *
 * The quad's vertex attributes are fixed by the fixture:
 *   attribute 3 (diffuse)  = 0xFF00FF00
 *   attribute 4 (specular) = 0xFFFF00FF
 * Both are deliberately "loud": a defect that passes either through where the
 * rule says it must not shows up as green or magenta bytes.
 *
 * ── Dyadic constants, so every float product below is exact ──────────────────
 *   SCENE_AMBIENT S = (0.5,    0.5,    0.5  )
 *   light ambient  A = (0.0625, 0.0625, 0.125)
 *   light diffuse  D = (0.25,   0.375,  0.25 )
 *   light specular   = (1, 1, 1)  (only a zero-term case below reads it)
 *   material alpha   = 0.75
 *   normal n = (0, 0, -1), model-view and inverse model-view identity, so
 *     e = (x, y, 0.5, 1) and n.e = -0.5 < 0: the L82 facing-away flip does NOT
 *     apply, which is what keeps this gate from re-deciding Q5a.
 *   S + A     = (0.5625, 0.5625, 0.625)
 *   S + A + D = (0.8125, 0.9375, 0.875)
 *
 * ── Packing ─────────────────────────────────────────────────────────────────
 * The vertex colour packs by truncation, byte = (uint32_t)(c * 255), in
 * `vertex_color` / `lit_color`'s caller (nv2a_pb_exec.c:2863-2866); the combiner
 * output packs by round-to-nearest (nv2a_pb_exec.c:3356-3365, `f * 255 + 0.5`).
 * Every channel below is chosen so both agree:
 *   0.5625 * 255 = 143.4375 -> 143 = 0x8F
 *   0.625  * 255 = 159.375  -> 159 = 0x9F
 *   0.8125 * 255 = 207.1875 -> 207 = 0xCF
 *   0.9375 * 255 = 239.0625 -> 239 = 0xEF
 *   0.875  * 255 = 223.125  -> 223 = 0xDF
 *   0.75   * 255 = 191.25   -> 191 = 0xBF
 *   0      * 255 = 0        ->   0 = 0x00
 *
 * ── The cases ────────────────────────────────────────────────────────────────
 * Every case sets LIGHT_CONTROL = 0x00000001 (SEPARATE_SPECULAR on) unless it
 * says otherwise, so nothing is added into oD0 (V4). Bit 16 LOCALEYE and bit 17
 * are left clear: both are PENDING, and neither may change the rgb read here.
 *
 * All cases deliver L = H = (0,0,-1) except `lit_d1_nl_negative`, which delivers
 * L = H = (0,0,+1). With n = (0,0,-1):
 *   L = (0,0,-1):  n.L = 1,  n.H = 1   -> the gates are OPEN
 *   L = (0,0,+1):  n.L = -1, n.H = -1  -> both clamp to 0 and pf = 0 (V2)
 * The negative case uses H = (0,0,+1) explicitly rather than a
 * normalize(L + V) value, because that expression is degenerate there; the
 * delivered words are literal data and they close BOTH gates (V2's `nDotVP == 0`
 * and `nDotHV == 0`), so the assertion does not depend on which one fires first.
 *
 * PC1 `pc_unlit_d1` - lighting 0, specular enable 1, observer D1.
 *     V5: lighting off gives oD1 = specular = the vertex attribute 4,
 *     0xFFFF00FF. The specular-enable branch (V3) runs after and would force
 *     (0,0,0,1) when enable were 0, so enable is 1 here.
 *     This is the POSITIVE CONTROL for the D1 observer: it must read the
 *     vertex specular, proving the combiner really selects oD1 and not oD0.
 *     want 0xFFFF00FF.
 *
 * PC2 `pc_unlit_d0` - same state, observer D0.
 *     V5: oD0 = diffuse = attribute 3, 0xFF00FF00.
 *     POSITIVE CONTROL for the D0 observer, and it must differ from PC1.
 *     want 0xFF00FF00.
 *
 * G1  `lit_d0_separate` - lighting 1, specular enable 1, separate specular on,
 *     observer D0, n.L = n.H = 1.
 *     V4: with separate specular SET nothing is added into oD0, so oD0 is the
 *     Q5a lighting result: rgb = S + A + D * max(0, n.L) = S + A + D, alpha =
 *     material alpha 0.75 (SET_COLOR_MATERIAL = 0 does not route it from the
 *     vertex). The specular term is nonzero here (the gates are open), so this
 *     case would catch an implementation that folded specular into oD0 while
 *     separate specular was set.
 *     want 0xBF CF EF DF.
 *
 * R1  `lit_d1_spec_off` - lighting 1, SPECULAR ENABLE 0, observer D1.
 *     V3: the `!specular_enable` override forces oD1 = (0,0,0,1), so rgb is 0
 *     whatever the light does.
 *     V6 is also exercised: the vertex specular attribute (0xFFFF00FF) must not
 *     appear. A pass-through defect reads 0xFFFF00FF; a missing-override defect
 *     reads the lit value (0xFFFFFFFF here, since the light is white and the
 *     gates are open).
 *     Alpha: the pixel's alpha is CW1's G field, which every case points at V0
 *     (oD0), NOT at oD1. A lit oD0 takes its alpha from the material
 *     (vsh-ff.c:265-266 selects material_alpha when diffuse_src is MATERIAL,
 *     which SET_COLOR_MATERIAL = 0 is), i.e. 0.75 -> 0xBF. So V3's oD1.a = 1 is
 *     NOT observable through this observer and is not asserted.
 *     want 0xBF000000.
 *
 * R2  `lit_d1_nl_negative` - lighting 1, SPECULAR ENABLE 1, observer D1,
 *     L = H = (0,0,+1).
 *     V1: oD1 starts at (0,0,0, specular.a) - rgb 0.
 *     V2: n.L = -1 clamps to 0 and n.H = -1 clamps to 0, so pf = 0 and the
 *     light adds nothing. rgb stays 0.
 *     V6: the vertex specular attribute must not appear.
 *     Alpha: again V0's, i.e. the material's 0.75 -> 0xBF (see R1).
 *     want 0xBF000000.
 *     A pass-through defect reads 0xFFFF00FF; an implementation that ignored
 *     the N.L gate reads the lit specular colour, which with light specular
 *     (1,1,1) and pf = 1 is 0xFFFFFFFF.
 *
 * ── Today's result per case (predicted from the packet's own statement, ──────
 *    NOT from reading the implementation's reasoning) ───────────────────────
 * The packet states: "the executor passes the vertex's specular attribute
 * through (backend_vertex_extra) and lit_color computes no specular term".
 * From that alone:
 *   PC1, PC2, G1  are expected to PASS (they pin behaviour no one disputes).
 *   R1            FAIL: oD1 carries 0xFFFF00FF (the vertex attribute) where the
 *                 rule requires rgb 0, i.e. reads 0xBFFF00FF, want 0xBF000000.
 *   R2            FAIL: the same pass-through, 0xBFFF00FF, want 0xBF000000.
 * These are PREDICTIONS. The gate is committed before the implementation is
 * read or changed, and the observed value is recorded in the TR.
 *
 * ── CPU LIMITATION ──────────────────────────────────────────────────────────
 * The CPU fixed-function rasteriser does not run the register combiners at all:
 * `raster_triangle` writes the lit vertex colour (or the modulated stage-0
 * texel) straight to the pixel, and only the vertex-PROGRAM path
 * (`raster_xf_triangle` -> `xf_rows`) evaluates `nv2a_rc_eval`
 * (nv2a_pb_exec.c:3697). A fixed-function draw therefore cannot observe oD1 on
 * the CPU half. Each case carries a separate, labelled CPU literal, following
 * the texgen gate's `cpu_differs` precedent (tests/texgen_stream_cases.h:106):
 * the CPU half checks the colour the CPU path actually writes, so the CPU half
 * is a plumbing/positive-control check and NOT the deciding assertion. The
 * Vulkan half, which runs the production combiner, is the deciding one.
 *
 *   PC1 pc_unlit_d1        CPU writes the unlit diffuse 0xFF00FF00  (not 0xFFFF00FF)
 *   PC2 pc_unlit_d0        CPU writes the unlit diffuse 0xFF00FF00  (same as want)
 *   G1  lit_d0_separate    CPU writes the lit colour  0xBFCFEFDF    (same as want)
 *   R1  lit_d1_spec_off    CPU writes the lit colour  0xBFCFEFDF    (not 0xBF000000)
 *   R2  lit_d1_nl_negative CPU writes the lit colour  0xBF8F8F9F    (not 0xBF000000)
 * R2's CPU value is S + A only, because n.L <= 0 leaves the diffuse term out:
 *   S + A = (0.5625, 0.5625, 0.625) -> 0xBF 8F 8F 9F.
 */
#ifndef JSRF_SPEC_STREAM_CASES_H
#define JSRF_SPEC_STREAM_CASES_H

#include "spec_stream_fixture.h"

#include <stddef.h>
#include <string.h>

#define N_OF(a) (sizeof(a) / sizeof((a)[0]))

/* SET_LIGHT_CONTROL: bit 0 SEPARATE_SPECULAR (xemu nv2a_regs.h:907). */
#define SF_SEPARATE_SPECULAR 0x00000001u

#define SF_LIT_FULL   0xBFCFEFDFu   /* S + A + D, n.L = 1 */
#define SF_LIT_AMBIENT 0xBF8F8F9Fu  /* S + A, n.L <= 0 */
#define SF_UNLIT_DIF  0xFF00FF00u   /* attribute 3 */
#define SF_UNLIT_SPC  0xFFFF00FFu   /* attribute 4 */
/* oD1 rgb 0. The PIXEL alpha is CW1's G field, which every case points at V0
 * (oD0), not at oD1: a lit oD0 alpha is the material alpha 0.75 -> 0xBF. */
#define SF_ZERO_RGB   0xBF000000u

#define SC_SCENE  { 0.5f, 0.5f, 0.5f }
#define SC_AMB    { 0.0625f, 0.0625f, 0.125f }
#define SC_DIF    { 0.25f, 0.375f, 0.25f }
#define SC_SPC    { 1.0f, 1.0f, 1.0f }
#define SC_ALPHA  0.75f

/* The three probes, all inside the quad, two triangles covered. */
typedef struct { uint32_t x, y; } SProbe;
static const SProbe k_probes[] = { { 4, 4 }, { 28, 28 }, { 24, 10 } };

typedef struct {
    const char *name;
    SDraw draw;
    uint32_t want;              /* the hardware-rule ARGB at every probe (Vulkan half) */
    uint32_t want_cpu;          /* what the CPU fixed-function path writes (LIMITATION) */
    int cpu_differs;            /* 1: want_cpu is a labelled CPU-path limitation */
} SCase;

/* normal, dir, half, scene, amb, dif, spc, alpha; then the literals. */
static const SCase k_cases[] = {
    /* PC1: lighting off, oD1 = the vertex specular attribute. D1 observer. */
    { "pc_unlit_d1",
      { NULL, NULL, 0, 0, 1, SF_SEPARATE_SPECULAR, SF_OBS_D1,
        { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 },
        SC_SCENE, SC_AMB, SC_DIF, SC_SPC, SC_ALPHA },
      SF_UNLIT_SPC, SF_UNLIT_DIF, 1 },

    /* PC2: the same state through the D0 observer: oD0 = the diffuse attribute. */
    { "pc_unlit_d0",
      { NULL, NULL, 0, 0, 1, SF_SEPARATE_SPECULAR, SF_OBS_D0,
        { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 },
        SC_SCENE, SC_AMB, SC_DIF, SC_SPC, SC_ALPHA },
      SF_UNLIT_DIF, SF_UNLIT_DIF, 0 },

    /* G1: lit, separate specular on, oD0 is the lighting result unchanged. */
    { "lit_d0_separate",
      { NULL, NULL, 0, 1, 1, SF_SEPARATE_SPECULAR, SF_OBS_D0,
        { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 },
        SC_SCENE, SC_AMB, SC_DIF, SC_SPC, SC_ALPHA },
      SF_LIT_FULL, SF_LIT_FULL, 0 },

    /* R1: lit, SPECULAR_ENABLE off -> oD1 = (0,0,0,1). */
    { "lit_d1_spec_off",
      { NULL, NULL, 0, 1, 0, SF_SEPARATE_SPECULAR, SF_OBS_D1,
        { 0, 0, -1 }, { 0, 0, -1 }, { 0, 0, -1 },
        SC_SCENE, SC_AMB, SC_DIF, SC_SPC, SC_ALPHA },
      SF_ZERO_RGB, SF_LIT_FULL, 1 },

    /* R2: lit, both cosines <= 0 -> oD1 rgb 0 (the pixel alpha is V0's, 0.75). */
    { "lit_d1_nl_negative",
      { NULL, NULL, 0, 1, 1, SF_SEPARATE_SPECULAR, SF_OBS_D1,
        { 0, 0, -1 }, { 0, 0, 1 }, { 0, 0, 1 },
        SC_SCENE, SC_AMB, SC_DIF, SC_SPC, SC_ALPHA },
      SF_ZERO_RGB, SF_LIT_AMBIENT, 1 },
};

static const SCase *s_find_case(const char *name)
{
    size_t i;
    for (i = 0; i < N_OF(k_cases); i++)
        if (strcmp(k_cases[i].name, name) == 0)
            return &k_cases[i];
    return NULL;
}

/* ══ PENDING: cases this gate cannot yet decide, and exactly what unblocks each
 * ════════════════════════════════════════════════════════════════════════════
 * The blocker for P1-P3 is METHOD ADMISSION, not the rule: SET_SPECULAR_PARAMS
 * 0x09E0..0x09F4 is absent from the generated table, so a stream carrying it is
 * rejected whole (L39). Admitting it requires a genuine runtime witness
 * (`[PFIFO] admit-unknown method=09Ex` from a run that exercised it), recorded
 * in config/nv2a-runtime-witnessed-methods.json and regenerated with
 * scripts/gen-nv2a-method-inventory.py --witness (L44). This packet may not
 * change admission, and a rejected walk is not a deciding semantic RED.
 *
 * P1 pf > 0 with the infinite half vector (JSRF's own state).
 *    The pf formula is xemu master's (`5c516ff0`, see the V2 NOTE), not the
 *    pinned `478b4f49` pow() form; derive the literal from the master rule.
 *    Needs: 0x09E0..0x09F4 admitted, plus a specular power whose reconstruction
 *    (pgraph.c:1873-1919) gives a usable exponent. Assert: oD1.rgb ==
 *    lightSpecular * pow(n.H, power) for a chosen n.H in (0,1).
 *    Depends on: admission; the exponent is NOT derivable from the four
 *    coefficients by any rule this packet verified beyond xemu's own
 *    reconstruction, so the literal must come from a witnessed parameter set.
 *
 * P2 the N.H gate alone (n.L > 0 but n.H <= 0).
 *    Needs: nothing beyond what this gate already has EXCEPT an H that differs
 *    from L, which IS deliverable (H is an admitted 0x1028..0x1030 word). This
 *    case is deliberately left for the implementation stage so the committed
 *    gate's literals are not edited after the fact; it can be added as a new
 *    case without touching these.
 *
 * P3 the specular pf clamp/range.
 *    The pinned `478b4f49` vsh-ff.c has no 10000 literal, but xemu master
 *    (`5c516ff0`) clamps pf to [0, 10000] and sets pf = 10000 when the
 *    denominator is 0 (see the V2 NOTE). Needs admission (P1) and a witnessed
 *    parameter set that reaches the clamp.
 *
 * P4 LOCALEYE (LIGHT_CONTROL bit 16).
 *    Needs: the eyePosition/VPeye branch (vsh-ff.c:290-293, 334-337) decided
 *    against a delivered EYE_POSITION. EYE_POSITION is not in this fixture; the
 *    retail witness above says JSRF's H already equals normalize(L + (0,0,-1)),
 *    which is consistent with the LOCALEYE form, but that is one observed state
 *    and not a rule. Note the executor's LIGHT_CONTROL (0x0294) is only STORED
 *    (nv2a_pb_exec.c:5913 generic `s_reg`), never read by lit_color.
 *
 * P5 non-separate specular (LIGHT_CONTROL bit 0 clear, lighting on).
 *    vsh-ff.c:428-439 adds oD1 into oD0 and then sets oD1 = the vertex specular
 *    attribute. Deliverable through admitted methods (0x0294 is admitted), so
 *    this is NOT admission-blocked; it is deferred only to keep this gate's
 *    scope to the defect named in the packet.
 *
 * P6 oD1 alpha and LIGHT_CONTROL bit 17.
 *    Bit 17 is ALPHA_FROM_MATERIAL_SPECULAR (nv2a_regs.h:909) and maps to
 *    `ignore_specular_alpha = !bit17` (vsh.c:122-124), which forces oD1.a = 1
 *    (vsh-ff.c:440-444). This gate leaves bit 17 clear and never reads alpha on
 *    the lit cases except through V3's explicit 1.0, so alpha is not decided
 *    beyond R1.
 *
 * P7 colour-material specular source (SET_COLOR_MATERIAL 0x0298 bits 6-7 = 2).
 *    vsh-ff.c:409-412 then multiplies the vertex specular into lightSpecular.
 *    0x0298 IS admitted, so this is deliverable; it is deferred because it
 *    changes the meaning of "lit oD1" and deserves its own gate.
 *
 * Also NOT decided here: local and spot lights (light 0 is infinite), more than
 * one light, fog interaction with oD1.a (vsh-ff.c:448+), two-sided/back
 * materials (oB1, a FIXME in the pinned source), and vertex-program batches.
 */
#endif
