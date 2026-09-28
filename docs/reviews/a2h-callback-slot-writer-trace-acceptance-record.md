# `A2h-callback-slot-writer-trace-r1` — stage-1 acceptance: **`ACCEPT-WITH-CORRECTIONS`**, and the corrections MOVE the edge

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Reviewer:** `52151a8e-d95a-4e64-9cfe-e205d0624300` (`workbuddy-ai/hy4-preview-f` @ `high`), **independent.**
**Review:** `docs/reviews/a2h-callback-slot-writer-trace-acceptance-review.md`, committed `db5e197`.
**Disposition: `ACCEPT-WITH-CORRECTIONS`. `BLOCKING: NONE`** — **but the corrections change WHICH edge a
successor must resolve, so they are not cosmetic.**

---

## ✅ What survived — independently confirmed

| Claim | Verdict |
|---|---|
| **1. exactly ONE store covers `software_device+0x242C`** | **CONFIRMED** — the reviewer's own image-wide filter returns exactly 1 |
| **2. the raw-byte backstop** | **CONFIRMED** — 1 hit, file `0x17D2FC` → VA `0x0018CE3C`, **alignment-independent** |
| **3. the two `jmp`s** | **CONFIRMED** — rel32 scan, both `E9` |
| **4. single caller `0x00012319`, `ebx=0`** | **CONFIRMED** — one `E8`; **26 `ebx` mentions in the range, ZERO writes** |
| **5. `0x19DCE0` from 2 sites** | **CONFIRMED** |
| **7. the forwarding thunk** | **CONFIRMED** — **sole rel32 reference image-wide; 0 raw dword occurrences** |
| **9. the context alias `[eax+0xc]`** | **CONFIRMED** |
| **11. the C-5 ledger** | **CONFIRMED** |
| **the instrumentation warning** | **REAL, independently reproduced** — a non-boundary start gave *"dec dword ptr [ebx + 0x19ded815]"* / *"retf 0x7f7f"* where section-anchored decode gives real instructions |

## ⚠ CORRECTION A — the named edge points at the WRONG ARGUMENT

**The Worker said `ebp` derives from **arg3**, giving range `[arg3, 2·arg3−1]` and reachability at
`arg3 ∈ [1033, 2064]`.** **The reviewer says `ebp` derives from **arg1**, range `[arg1, arg1+arg3−1]`.**

**The Session verified it from the stack arithmetic and THE REVIEWER IS RIGHT:**

```
00199DB0  sub  esp, 0x10          ; esp = E-0x10
00199DB3  push esi                ; esp = E-0x14
00199DC3  mov  eax, [esp+0x20]    ; [E-0x14+0x20] = [E+0xC]  = ARG3
00199DD8  push ebp                ; esp = E-0x1C   (ebx also pushed)
00199DD9  mov  ebp, [esp+0x20]    ; [E-0x1C+0x20] = [E+4]   = ARG1
```

> **The SAME `[esp+0x20]` displacement reads ARG3 at `0x00199DC3` and ARG1 at `0x00199DD9`, because TWO
> PUSHES intervened.** **So `ebp = ARG1`, and the reachability range is `[arg1, arg1+arg3−1]`.**

**The reviewer's consequence is exactly right:** *"THE NAMED OPEN EDGE POINTS AT THE WRONG ARGUMENT — a
successor resolving the thunk's third argument would close nothing."*

**So the Session's own verification record (`fc0b389`) repeated the Worker's error when it endorsed
`[1033, 2064]`.** **The Session did NOT check the displacement shift across the pushes, and it should have —
that is the same class as its other errors: an extraction whose condition was not verified per-item.**

## ⚠ CORRECTION B — a SECOND genuinely open store, and the "single store" headline is unsupported

**The Worker excluded `0x0018DF59` on the ground that `0x242C − 0xa78 = 0x19B4` "is not divisible by 4".**

**The Session verified the arithmetic: `0x19B4 = 6580`, and `6580 / 4 = 1645` EXACTLY.**

> **So the exclusion is FALSE.** **`0x0018DF59` has base `edi = MEM32(0x19DCE0)`, index `esi = arg1`
> (unbounded), and writes `ebx` — an arbitrary caller word.** **At `arg1 = 1645` it writes an arbitrary value
> directly into `software_device+0x242C`.**

**So there are TWO genuinely open stores, not one:**

| Store | Index | Reachability |
|---|---|---|
| **`0x00199F45`** | `ebp` = **arg1** | `4·arg1 + 0x3EC = 0x242C` → **`arg1 = 0x810 = 2064`** |
| **`0x0018DF59`** | `esi` = **arg1** (unbounded) | `4·arg1 + 0xa78 = 0x242C` → **`arg1 = (0x242C − 0xa78)/4 = 0x19B4/4 = 0x66D = 1645`** |

**The reviewer's `1645` is CORRECT, and the Session confirms it.** *(The Session briefly wrote a parenthetical
claiming the index was `2716` — that was wrong, and it contradicted the Session's own arithmetic two lines
above. Removed. **Recorded because the Session nearly introduced a third error while correcting one, which is
the same pattern again.**)*

**And the reviewer's point stands regardless of the exact index: the exclusion is false and the store is
open.**

**Also corrected by the reviewer:** `0x0018DF87` reaches the slot **arithmetically** but writes the constant
`0xFFFFFFFF` — **excluded on that (value) ground, not the ground the Worker gave.**

**So the Worker's headline *"closed to a single store"* and its single-edge naming are UNSUPPORTED.**
**The row `O-OPEN` is unaffected — and the reviewer notes its finding makes `O-OPEN` *"more clearly right."***

## ⚠ CORRECTION C — the shift-check: the SESSION is right, the Worker mis-computed

**The reviewer settles it from the XBE bytes:** `v0 = 0x30766A64`, `v1 = 0x3030766A`,
`(v1 & 0x00FFFFFF) = 0x0030766A == (v0 >> 8) = 0x0030766A`. **PASSES.**

**And it names the Worker's error precisely:** *"Worker reported `0x00766A`, dropping the `0x30` byte. Its
'3-byte window / informative failure' rationalisation is an ex-post explanation of its own arithmetic error;
**it recorded a mandated control as FAILED when it passed.**"*

**So the Session's construction-based settlement was correct, and now a third party confirms it from the
bytes.** **Recorded because a mandated control being recorded as failed is a serious evidence defect** —
**it would have told a successor the control itself was broken.**

## What the row is, and what the successor must carry

**Row `O-OPEN` — correct.** **The reviewer: *"No complete reaching chain to `0x001D5078`, no temporal order;
`O-DATA-AS-CALL` unavailable. Worker did not stop short."***

**And the successor must carry FOUR things, not one:**
1. **`0x00199F45`** — index **arg1** (`ebp`), reachable when `arg1 = 2064`.
2. **`0x0018DF59`** — index **arg1** (`esi`, unbounded), reachable when `arg1 = 1645`; **writes an arbitrary
   caller word.**
3. **the thunk's argument question — now correctly ARG1, not arg3.**
4. **the context alias at `[eax+0xc]`** — still independent.

**Plus the Session's own structural finding** (`a2h-thunk-parent-dispatch-table.md`, `42dd8db`):
**`sub_00153790` is vtable entry 51 of a 52-entry vtable at `0x001E1270`, installed at `object+0x00` by a
teardown path** — **so the thunk's argument question is really *"where is vtable index 51 dispatched, and
what does that caller pass?"***

## What the Session got wrong, recorded against itself

- **It endorsed `[1033, 2064]`** in its verification record **without checking the displacement shift across
  the two intervening pushes.** **The reviewer caught it.**
- **It did not independently test the Worker's four exclusions** — **it verified the open store's arithmetic
  but accepted the four exclusions on the Worker's word.** **The reviewer found one is false.**
- **And in correcting the reviewer's index it nearly introduced a third error**, writing a parenthetical that
  contradicted its own correct arithmetic. **Recorded because the pattern — verifying the claim you are
  pointed at and not the surrounding ones — is now well established.**

## Prohibitions and status

**Static and read-only.** **No game run, no source edit, no instrumentation.** **No synthetic completion.**
`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)` unchanged. **`PIO_FREE` stays DEFERRED**;
**`A4b2-r7`/`A4b2-r8`/`A4b1-r4` not reopened**; **`0xFFFFB3` stays `UNRESOLVED`**; **the retired NULL line was
not reopened and no DR record was cited**; **float-bit siblings contrastive.** **All eight guards pass.**
