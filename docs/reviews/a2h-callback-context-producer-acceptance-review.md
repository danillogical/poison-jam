# A2h — stage-1 ACCEPTANCE REVIEW of the callback-context PRODUCER evidence

**Reviewer:** independent stage-1 acceptance (did not author and did not execute the evidence).
**Date:** 2026-09-28. **Method:** STATIC only — no game run, no build, no instrumentation.
**Subject:** `docs/reviews/a2h-callback-context-producer-evidence.md` (Worker commit `537437f`),
against `docs/packets/a2h-callback-context-identity.md` **r2** and the Advisor ruling
`docs/reviews/a2h-device-2268-retraction-advisor-ruling.md` (turn `01a0e852`).
**Identity of the tree reviewed:** `git show --stat 537437f` → **1 file changed**, the evidence
document only. Working tree otherwise clean (`git status --short` empty).

---

## 1. DISPOSITION

> ## **ACCEPT-WITH-CORRECTIONS**

**Every one of the five mandated byte checks is CONFIRMED against the original XBE.** The
Advisor's *conditional* condition is therefore **satisfied**, and the retraction's logic stands
on verified bytes. **The finding "the context is LOCATED AT `device+0x2268`" survives falsification
attempt.**

**The corrections are real but all live in the EVIDENCE RECORD, not in the execution's substance:**

| # | Defect | Class |
|---|---|---|
| **C-1** | **The selected row `O-ALTERNATE-PATH` is WRONG.** The evidence's own finding contradicts that row's predicate. Correct row: **`O-OPEN` SUSTAINED, edge advanced.** | evidence record (row) |
| **C-2** | **No DISCRIMINATING control appears in the Worker's evidence.** §6 records only the non-discriminating `0x001D5078 → 30766A64 305F3030 7864612E`. | evidence record (control) |
| **C-3** | **Call-site addresses are cited OFF BY ONE** — every cited address is the instruction *after* the `call`. | evidence record (citation) |
| **C-4** | **`sub_00194C3F`'s "`ecx = ebx + 0x2268`" is not in the bytes I read;** the `+0x1A0` "container arithmetic" is rendered as a store when it is a `push`. | evidence record (rendering) |
| **C-5** | **The producer chain is not closed at the top:** no caller of `sub_00194ADD` is traced, and no `KeInsertQueueDpc` site is located. | scope gap |
| **C-6** | **`sub_001925A0`'s site-4 edge is rendered as straight-line** when the bytes show a test/branch and a `[context]` dereference before the call. | evidence record (rendering) |

**None of C-1…C-6 falsifies the ANSWER.** They are recorded because this line has repeatedly
accepted corrections, and a reviewer who cannot find a defect has usually stopped looking.

---

## 2. THE MANDATORY BYTE CHECKS — all five, in the original XBE

**Tool:** `python -X utf8 scripts/inspect-jsrf.py disasm <start> <end>`.
**Anchoring:** every window below was opened on a DECLARED boundary (`sub_00194A72`,
`sub_00194EEF`, `sub_00194C3F`, `sub_00194E2D`, `sub_00196C0B`, `sub_00194480`, `sub_001941E0`,
`sub_001925A0`) or on a `loc_`/declared callee start — **never** byte-scanned from an inferred
boundary.

### BYTE CHECK 1 — `0x0019460A mov dword ptr [ecx], 0xfd000000` → **CONFIRMED**

```
0019460A mov      dword ptr [ecx], 0xfd000000
00194610 mov      eax, dword ptr [0xfd001804]
00194615 or       eax, 4
00194618 mov      dword ptr [0xfd001804], eax
0019461D xor      eax, eax
0019461F mov      dword ptr [0xfd600140], 0
```

**The store exists verbatim at the exact address.** The immediately following instructions write
`0xFD001804` and `0xFD600140` — **NV2A aperture addresses** — which independently corroborates that
`0xFD000000` here is the MMIO base and not a coincidental constant. **This single instruction is
what destroys the premise of the Session's refutation** (`[device+0x2268]` must read back the
device): the first dword is overwritten with the aperture. **The Advisor's condition is met.**

*Not independently verified:* the evidence's claim that `sub_0019460A` is **called from**
`sub_00194ADD`. I confirmed the store and I confirmed `sub_00194ADD`'s DPC init; I did not
disassemble `sub_00194ADD`'s full body to locate that call. **This does not affect byte check 1.**

### BYTE CHECK 2 — `0x001925FB lea edi, [esi + 0x2268]` → **CONFIRMED**

```
001925F9 jne      0x1925e0
001925FB lea      edi, [esi + 0x2268]        <== TARGET
00192601 mov      dword ptr [esp + 8], edi
00192605 mov      ecx, edi
00192607 call     0x193c20
```

**Exact address, exact form.** Reached from the declared `sub_001925A0` entry, and it is also a
branch target (`0x1925A8 je 0x1925fb`), i.e. a genuine `loc_`, not an inferred start.

### BYTE CHECK 3 — `0x001925A2 mov esi, ecx` → **CONFIRMED**

```
001925A0 push     ecx
001925A1 push     esi
001925A2 mov      esi, ecx                   <== TARGET
001925A4 cmp      dword ptr [esi], 0
```

**Exact address.** **`esi` is therefore the incoming `ecx` for the whole body**, which is what
makes `lea edi,[esi+0x2268]` read as `context = incoming_ecx + 0x2268`.

### BYTE CHECK 4 — the DPC binding → **CONFIRMED** (with one scope caveat)

```
00194ADD push     ebp                        <== declared sub_00194ADD entry
00194AE5 mov      esi, ecx
00194AE7 push     esi                        <== arg3 DeferredContext = CONTEXT
00194AE8 push     0x194480                   <== arg2 DeferredRoutine = sub_00194480
00194AED lea      eax, [esi + 0x84]          <== arg1 Dpc = &context->Dpc
00194AF3 push     eax
00194AF4 call     dword ptr [0x1c4020]       <== KeInitializeDpc
```

**Argument order verified from the push order** (reverse push = forward args):
`Dpc = context+0x84`, `DeferredRoutine = 0x00194480`, `DeferredContext = esi = ecx = context`.
**This is exactly `KeInitializeDpc(PKDPC, PKDEFERRED_ROUTINE, PVOID)` and exactly the claimed
binding.** **This is the mechanism that explains why `sub_00194480` has no caller:** at
`0x00194480` it does `mov ebx,[esp+0xC]` — a stack argument, consistent with a kernel-invoked
`DeferredRoutine`, and inconsistent with any `call` site.

**CONFIRMED caveat (C-5).** Two things the evidence does **not** establish, and I did not find:
1. **`KeInitializeDpc` does not "hand" the context to the routine — it only STORES it.** The
   routine runs only if the DPC is **queued** (`KeInsertQueueDpc`/`KeSetTimer`+DPC). **No queue
   site was located by the Worker and I did not locate one.** So the DPC binding is verified as a
   *binding of identity*, **not as a verified runtime path to the terminal.**
2. **No caller of `sub_00194ADD` was traced** by the Worker. A grep of the source tree shows
   `sub_00194ADD` only as a **dispatch entry** (`recomp_dispatch.c:8025`) and one recovered call
   at `0x0019213A` — which **disassembles as mid-instruction garbage**, i.e. not a confirmed call
   site. **The DPC path's producer root is therefore OPEN**, not closed.

### BYTE CHECK 5 — the thunk binding → **CONFIRMED**

Producer side, inside declared `sub_00194EEF` (`0x194EEF..0x195095`), at its tail:

```
00195079 lea      eax, [edi + 0x1a0]         <== edi = ecx = CONTEXT (00194EF8 mov edi,ecx)
0019507F push     eax
00195080 call     dword ptr [0x1c40d8]       <== IAT slot
```

Consumer side, at declared `sub_001941E0`:

```
001941E0 mov      ecx, dword ptr [esp + 4]
001941E4 mov      eax, dword ptr [ecx - 0x1a0]
001941EA add      ecx, 0xfffffe60            <== -0x1A0  == context = arg - 0x1A0
001941F0 mov      dword ptr [eax + 0x140], 0
001941FA call     0x194eef                   <== re-enters sub_00194EEF with ecx = CONTEXT
001941FF ret      4
```

**Both halves confirmed byte-for-byte.** `0xFFFFFE60` is two's-complement `−0x1A0`, so
`context = arg − 0x1A0` is arithmetically exact. **`sub_001941E0` is `stdcall` with one argument**
(`ret 4`), consistent with a callback/thunk signature. **This is the producer edge.**

**Caveat the evidence itself records (§7.3):** IAT `0x1C40D8` is unresolved in the original XBE, so
**runtime dispatch through the import is NOT proven** — only the static call and the stored pointer
value. I agree and do not upgrade it.

---

## 3. CALLER CHAINS CLOSED? → **CONFIRMED (as topology) — with citation corrections (C-3)**

**All five claimed edges exist in the original XBE.** **But every address the Worker cites is the
instruction AFTER the `call`, not the `call` itself.** That is a citation defect, not a topology
defect — and it matters on this line, which has a documented history of mis-transcribed offsets.

| Function | Worker cites | **Actual call instruction I read** | Host | Verdict |
|---|---|---|---|---|
| `sub_00196C0B` | `0x194CD1`, `0x194DC1`, `0x194E0D` | **`0x194CCC`**, **`0x194DBC`**, **`0x194E08`** | `sub_00194C3F` ×3 | **CLOSED** (addresses off by one) |
| `sub_00196C0B` | `0x194E7E` | **`0x194E79`** | `sub_00194E2D` | **CLOSED** (off by one) |
| `sub_00194A72` | `0x194C56`, `0x194E44` | **`0x194C51`**, **`0x194E3F`** | `sub_00194C3F`, `sub_00194E2D` | **CLOSED** (off by one) |
| `sub_00194300` | `0x1944E9`,`0x194A9A`,`0x194F54` | **`0x1944E4`**, **`0x194A95`**, **`0x194F4F`** | `sub_00194480`, `sub_00194A72`, `sub_00194EEF` | **CLOSED** (off by one) |
| `sub_00194EEF` | `0x192727`, `0x1941FF` | **`0x1941FA`**; and **`0x00192722`** in `sub_001925A0` | `sub_001941E0`, `sub_001925A0` | **CLOSED** (off by one) |
| `sub_00194480` | *(none)* | **no `call 0x194480` exists** — `0x00194AE8` is a **`push`**, not a call | DPC | **CLOSED — genuinely no call site** |

**Corroborating bytes:**

```
00194C4E mov  dword ptr [ebp - 8], ecx     <== saves incoming ecx
00194C51 call 0x194a72                     <== ecx still = context
00194CC9 mov  ecx, dword ptr [ebp - 8]     <== restores it
00194CCC call 0x196c0b                     <== so sub_00196C0B gets the SAME context
00194DB9 mov  ecx, dword ptr [ebp - 8]
00194DBC call 0x196c0b
00194E05 mov  ecx, dword ptr [ebp - 8]
00194E08 call 0x196c0b
```

**The `[ebp−8]` relay is real**, which is exactly the "caller's own incoming `ecx`" claim. Same
shape in `sub_00194E2D` (`[ebp−4]`).

**C-6 — the site-4 rendering is inaccurate.** The Worker's §2 block draws
`mov ecx,[esp+8] / call 0x194EEF` as straight-line. The bytes are:

```
0019270E mov  ecx, dword ptr [esp + 8]     <== CONTEXT (edi saved at 0x192601)
00192712 mov  eax, dword ptr [ecx]         <== dereferences [context]  == MMIO base
00192714 test eax, eax
00192716 je   0x192727                     <== BRANCH — call is conditional
00192718 mov  dword ptr [eax + 0x140], 0   <== writes a HARDWARE register
00192722 call 0x194eef                     <== the actual call
```

**The conclusion survives** — `ecx` at `0x00192722` is the context — **but the omitted branch and
the omitted `[context]` dereference are real.** Note the dereference is itself a small independent
corroboration: the code treats `[context]` as the MMIO base and writes `base+0x140`.

**C-4 — two container-arithmetic claims I could NOT confirm:**
- "`sub_00194C3F` does `ecx = ebx + 0x2268`" — **no such instruction in `0x194C3F..0x194CE0`**,
  which I read in full. It may lie later in that (354-byte) function; **UNVERIFIED**, not refuted.
- "`sub_00194EEF` does `[edi + 0x1A0] = [ebx + 0x2268 + 0x1A0]`" — **there is no store.** The bytes
  only `push` `edi+0x1A0` (byte check 5). **The rendering is wrong**; the *conclusion* that the
  thunk sits at `device+0x2408` rests on the separate dump read `[0x19D608] = 0x001941E0`, which I
  did not re-read (**and which the Worker did not re-read with an offset-shift control** — §5).

**Substantive scope gap (C-5):** the chain is closed **downward** from `sub_001925A0` for four
sites, and **upward-open at `sub_00194ADD`'s own caller** for the fifth (DPC) site. **"Five of five
closed" overstates it: four by direct register lineage, one bound to the object but with an
untraced producer root and no located queue site.**

---

## 4. BINDINGS INDEPENDENT? → **YES** — and that is the strongest part of the packet

**Basis — the two mechanisms share no code path and no register lineage:**

| | **DPC binding** | **Thunk binding** |
|---|---|---|
| **Where** | `0x00194AF4` `call [0x1C4020]` | `0x00195080` `call [0x1C40D8]` |
| **Kernel object** | `KDPC` (`KeInitializeDpc`, IAT `0x1C4020`) | import thunk (IAT `0x1C40D8`) |
| **How the context travels** | **stored in a kernel object** → kernel passes it as `sub_00194480`'s stack arg (`[esp+0xC]`) | **passed as a callback argument**, recovered by `arg − 0x1A0` arithmetic |
| **Entry shape** | no call site; `ret 0x10`, reads `[ebx]` and `[ebp+0x100]` | `ret 4`, one argument, re-enters `sub_00194EEF` |
| **Field used** | `context+0x84` (KDPC) | `context+0x1A0` (thunk slot) |

**They do not reduce to one another:** neither can be derived from the other — the DPC path would
still bind the context if `0x1C40D8` were never called, and vice versa. **Two structurally
unrelated mechanisms converging on the same object is genuine corroboration, which is exactly what
the packet's §10 asked for ("not merely a shared layout").**

**BUT — independence of *binding* is not independence of *execution*:** **both paths are static
only.** No `KeInsertQueueDpc` for the DPC; unresolved IAT for the thunk. **Neither is a proven
runtime route to the terminal.** The evidence's own §7.3 admits the thunk half; **it does not
admit the queue-site half**, and the acceptance record should.

---

## 5. DISCRIMINATING CONTROL USED? → **NO — evidence-record defect (C-2)**

**The Worker's §6 control is `0x001D5078 → 30766A64 305F3030 7864612E`, for both `data` and
`memory`.** **As the Advisor packet states and as `a2h-binding-reread-executed.md:25-26` already
concedes, that control is satisfied by BOTH byte orders and does NOT discriminate.**

**I ran the discriminating OFFSET-SHIFT test myself:**

| Read | XBE `data` | Dump `memory` | LE-DWORD predicts | Memory-order predicts |
|---|---|---|---|---|
| `0x001D5078` | `30766A64` | `30766A64` | `30766A64` ✓ | `646A7630` ✗ |
| **`0x001D5079`** | **`3030766A`** | **`3030766A`** | **`3030766A` ✓** | `6A763030` ✗ |

> **CONTROL: PASS — the tool prints LITTLE-ENDIAN DWORD VALUES, on both the XBE and the gated dump.**

**So the byte semantics the Worker relied on are sound — but the Worker did not demonstrate it.**
**This is a defect in the evidence record, not in the execution.** It matters because the
*entire* retraction rests on `0xFD000000` vs `0x000000FD`: **that is precisely the failure the
offset-shift control exists to catch**, and the Worker's own §6 left it unc guarded while leaning on
the value.

**Consequence for the rows:** **every memory-derived value in the evidence is UNCORROBORATED under
the Advisor's binding re-read precondition**, specifically `[0x19D468]=0xFD000000`,
`[0x19D608]=0x001941E0`, `[0x19D62C]=0x0015F9D0`, `[0x19D4EC+0xC]=0x00194480`, `[0x19D4FC]=0x0019D468`.
**None was re-read with an offset-shift control by the Worker.** They are corroborated *only* by
the XBE side (byte checks 1–5 and `0x19B200+0x2268 = 0x19D468` arithmetic), which is strong but is
not the control. **No row may lean on them until re-read.**

---

## 6. THE ROW → **`O-OPEN` SUSTAINED, edge advanced. `O-ALTERNATE-PATH` is WRONG (C-1)**

**The Worker selected `O-ALTERNATE-PATH`.** **I confirm the Advisor: that is the wrong row, and the
evidence's own finding contradicts its predicate.**

**The packet's own text (line 13):** `O-ALTERNATE-PATH` = *"existing **distinct** wrapper/field-layout
evidence PLUS a VERIFIED producer binding the actual context identity across all relevant sites."*

**Drafting note, stated honestly:** that sentence is **ambiguous** — "distinct" could be read as
modifying *"wrapper/field-layout evidence"* (met: the context's `+0x1C4/+0x1F4/…` layout is distinct
from the device's), rather than modifying *identity*. **If read the first way the Worker's selection
is defensible; if read the second, it is not.** **I resolve it against the Worker on substance, not
on grammar:** the whole point of the row is an **ALTERNATE** identity — a context that is *not* the
device sub-object, the successor then being *"the identified object's `+0x1C4` writer."* **The
Worker found the context IS `device+0x2268`, i.e. the device's own sub-object.** **That is the
identity the packet started with, not an alternate one.** A finding that confirms the incumbent
identity cannot select the row reserved for displacing it.

**Independently, the producer half is not complete** (C-5): `sub_00194ADD` has no traced caller and
no located queue site, so "a VERIFIED producer binding the identity across **all** relevant sites"
is **4 of 5 by lineage + 1 bound-but-root-open**, not 5 of 5. **Both routes land on `O-OPEN`.**

> **CORRECT ROW: `O-OPEN` SUSTAINED — with the edge ADVANCED.**
> The identity question is resolved on verified bytes (`context = device + 0x2268`,
> `device = MEM32(0x19DCE0) = 0x0019B200`), so the precise unknown is no longer "which object"
> but **"who writes the `+0x1C4` slot."**

**A consistency result I verified arithmetically and that the record should carry:**

```
context + 0x1C4  ==  (device + 0x2268) + 0x1C4  ==  device + 0x242C
```

**The packet's successor edge ("that identified object's `+0x1C4` writer") and the Advisor's
predicted edge ("writer of `device+0x242C`") are THE SAME SLOT**, reached two ways. **The two
phrasings are not rival unknowns — they are one unknown.** Recording both under one canonical
offset (`device+0x242C`, `0x2268` now being verified) prevents a future split.

---

## 7. OVER-CLAIMING — §7.2 "MMIO reading" clarification

**Verdict: an HONEST clarification, with a real naming ambiguity the record should name.**

**The established starting point (`a2h-callback-context-identity-identified.md`) says `+0` points to
the device.** The finding is `[context] = 0xFD000000` (NV2A aperture), **not** `0x0019B200`
(software object). **Those are two different things both called "the device."**

- **Is it an honest clarification?** **Yes.** The Worker states the discrepancy openly, does not
  hide it, and correctly notes the wrappers' `[context+N]` offsets (`0x400700`, `0x3214`, `0x2500`)
  are **hardware register offsets**, which only makes sense under the MMIO reading. **The
  disassembly independently corroborates this:** `sub_0019460A` follows the store with writes to
  `0xFD001804` and `0xFD600140`, and `sub_001925A0` at `0x00192718` writes `[eax+0x140]` where
  `eax = [context]`. **That is hardware access, not software-object access.**
- **Is it a real reinterpretation?** **Also yes, and it should be labelled as such.** The original
  invariant is not *satisfied* — it is **re-scoped**: "`+0` points to the device" now means "the MMIO
  aperture," not "the software object at `0x19B200`." **The evidence papers over this by calling it
  "satisfied only under the MMIO reading,"** when the accurate statement is **"the established
  invariant used 'device' in two senses and the bytes select the MMIO sense."** Since `0x0019460A`
  **overwrites** whatever was there, **`MEM32(0x19DCE0) = 0x19B200` and `[context] = 0xFD000000` are
  simultaneously true and are not the same object** — the successor packet must fix the vocabulary
  (`software_device` vs `aperture`) or the next reviewer will hit the same trap the Session did.

**No other over-claiming found.** §7.1 (`+0x1C4` writer untraced), §7.3 (IAT unresolved), §7.4
(refcount ordering) are all **appropriately hedged**, and §7.1 correctly respects the packet's
§10 prohibition on attributing the `+0x1C4` writer.

---

## 8. CONTROLS, GATE, SCOPE AND PROHIBITIONS

**Per-run dump gate — I ran it myself:**

```
python -X utf8 scripts/check-dump-mapping.py logs/runs/20260928-035337-386-a2h-repeat-on-1
expected .text[0:16] at guest VA 0x00011000: 8b512c85d28b4130c70190431c00741c
matches: 1   content-mismatch: 0   unreadable: 0   missing: 0
```

> **GATE: PASS.** Reads from that run are admissible. **The Worker's recorded result matches mine.**

**"No other run was read":** **CANNOT be independently verified** — I can only confirm the cited run
PASSES. It is **consistent** with the commit adding exactly one documentation file and with a clean
working tree. Recorded as consistent, not as verified.

**Scope and prohibitions — all CONFIRMED:**
- **Terminal scope:** `0x001D5078` only; I read no other terminal.
- **Float-bit siblings:** treated contrastive, not as a family conclusion. ✓
- **Retired NULL line:** not reopened; **no DR record cited.** ✓
- **No source edits:** `git show --stat 537437f` → **1 file changed**, the evidence doc only.
  **Working tree clean.** ✓
- **`RaiseException(0xE0424943, EXCEPTION_NONCONTINUABLE, …)`:** unchanged (no source in commit). ✓
- **`PIO_FREE`, `A4b2-r7`, `A4b2-r8`, `A4b1-r4`, `0xFFFFB3`:** untouched. ✓
- **No game run, no build, no instrumentation by this reviewer.** ✓

---

## 9. EVIDENCE OUTRUNNING CLAIMS

1. **§1 "two distinct, both-verified mechanisms" — the word "verified" over-reaches.** Byte check 4
   verifies `KeInitializeDpc` **stores** the context; it does not verify the DPC is ever **queued**
   or that `sub_00194480` ever **runs**. **No `KeInsertQueueDpc` site was located.** The claim should
   read "two structurally distinct static bindings; neither's runtime dispatch is proven."
2. **§2 "sub_00194480 … **CLOSED — not a broken edge**."** It is **bound but root-open**: no caller of
   `sub_00194ADD` was traced. "CLOSED" implies the producer chain terminates at an allocation; for
   this site it terminates at an untraced caller.
3. **§2 "sub_00194C3F does `ecx = ebx + 0x2268`" and "`sub_00194EEF` does `[edi+0x1A0] = …`"** — the
   first is UNVERIFIED in the window I read; the second is **contradicted** by the bytes (a `push`,
   not a store).
4. **§2 pseudo-code for `sub_001925A0`'s site 4 omits a conditional branch and a `[context]`
   dereference** that are present at `0x00192712..0x00192718`.
5. **§3 "All five sites' contexts are the same object … zero unbound."** **Supported (4 lineage +
   1 kernel binding), but "zero unbound" should be "zero unbound *to the object*"** — one site's
   producer root is untraced.
6. **§6 "every claim decoded from original bytes"** — true for every claim I tested, but three
   cited call addresses and two container-arithmetic renderings are not what the bytes say.

---

## 10. BLOCKING

**NONE for the finding and the retraction.** **Byte checks 1–5 are CONFIRMED**, the gate PASSES, and
the source is untouched — **so the Advisor's condition is satisfied and the retraction's logic holds
on verified bytes.**

**Two non-blocking conditions attach to the ROW, not to the finding:**
- **No row may lean on a memory-derived value** (`[0x19D468]`, `[0x19D608]`, `[0x19D62C]`,
  `[0x19D4EC+0xC]`, `[0x19D4FC]`) **until each is re-read with the offset-shift control.**
- **The successor packet must fix the `device` vocabulary** (software object `0x19B200` vs MMIO
  aperture `0xFD000000`) and **canonicalise the open edge as `device+0x242C` (== `context+0x1C4`)**.

---

## 11. UNCERTAIN / CANNOT VERIFY

1. **That `sub_0019460A` is called from `sub_00194ADD`** — the store is confirmed; the caller is not.
2. **Any `KeInsertQueueDpc` / DPC queue site** — not located by me or by the Worker.
3. **Any caller of `sub_00194ADD`** — not established; the one source-side reference at `0x0019213A`
   disassembles as mid-instruction.
4. **`sub_00194C3F`'s `ebx + 0x2268`** — absent from `0x194C3F..0x194CE0`; **not refuted, just
   outside the window I read.**
5. **The dump reads `[0x19D608] = 0x001941E0`, `[0x19D62C] = 0x0015F9D0`, `[0x19D4EC+0xC]`,
   `[0x19D4FC]`** — **I did not re-read them.** They pass arithmetically (`0x19B200+0x2268=0x19D468`,
   `+0x1A0 → 0x19D608`) but are **uncontrolled** under §5.
6. **`sub_0018E160` / `sub_0018E460` / `sub_0018CDF0`** (evidence §4) — **I did not disassemble them.**
   The `0x19DCE0` writer sites and the "statically allocated BSS device" claim are **UNVERIFIED by me**.
7. **"No other run was read"** — consistent but not verifiable.

---

**REVIEWER'S CLOSING NOTE.** **I tried to falsify the central claim and could not.** The three bytes
the Advisor staked the retraction on are all exactly where the evidence says they are, and the two
bindings are genuinely independent. **What I did find is a wrong row, a non-discriminating control,
and six citation/rendering defects — the same three failure modes this line keeps producing.** **The
substance survives; the record needs the corrections.**
