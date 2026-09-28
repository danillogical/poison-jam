# Advisor ruling — the `device+0x2268` retraction: acceptance decides, and a binding re-read precondition

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e852-0b8f-7000-9d97-0421ee450a4d`.
**Raised because:** the Session retracted its own identity-gate refutation after the Worker's producer trace
exposed a false premise, and a **"permanent"** Advisor ruling rested on that premise.
**Authority:** §2.3, §4.3.

---

## RULING

**RULING: retraction accepted as logic; bar lifted conditionally; nothing re-established until stage-1 verifies bytes. `O-OPEN` stands.**

**1. Neither reopens by ruling — acceptance decides, on explicit byte checks.** The retraction's logic is valid *if* the store exists: a stored `0xFD000000` at `[context]` destroys the premise that `[edi]` must read back the device, so the refutation fails *on that condition*. That condition (store at `0019460A`, `lea` at `001925FB`, DPC + thunk bindings) is exactly what stage-1 must verify byte-for-byte — **add it as an explicit acceptance check**. **The "no re-litigation" clause is therefore SATISFIED, not violated** (it barred reopening *without* new evidence; claimed new evidence has arrived). Upon accept, the alias is **re-established provisionally** like any accepted finding — and **my "permanent" wording is withdrawn as content while the discipline (no silent re-litigation) stands.**

**2. Yes — re-read `0x0015F9D0` and every row-feeding uncorroborated memory read, with offset-shift controls, BEFORE any row leans on one.** **Binding precondition enforced by adequacy/acceptance, not a packet.** Exempt only values with independent corroboration (crash-register arithmetic). **R-1 scope UNCHANGED** — mapping-inadmissibility is orthogonal to byte semantics, and its demotion stands regardless.

**3. `O-OPEN` stands; no re-referral needed.** Rows evaluate at selection time (unknown identity then — correct). Acceptance handles the worker evidence through its normal function. **Probable outcome, stated in advance: `O-OPEN` SUSTAINED with advanced edge (identity resolved → writer of `device+0x242C` becomes the precise unknown)** → writer-search followup per existing Searches 1–3. **NOT `O-ALTERNATE-PATH` — that row requires *distinct* identity, the opposite direction.**

**Note, not ruling:** **seven extraction failures with controls catching each is now a validated method, not just confessions** — the bounded re-read rule above is its systemic form. **Keep the retraction record linked from the callback review record.**

**BASIS:** observed — retraction logic (conditional on cited bytes, Session-verified as lead); packet row texts; R-1 mapping defect (orthogonal). Uncertain — cited XBE bytes (acceptance verifies).

**REVERSED BY:** store bytes failing verification (refutation stands, bar restored).

**RECORD IN:** verbatim in the callback review record; acceptance carries the byte checks + re-read precondition.

---

## ⚠ The Advisor CORRECTED the Worker's row selection, and the reasoning is decisive

**The Worker selected `O-ALTERNATE-PATH`.** **The Advisor refuses it:**

> **"NOT `O-ALTERNATE-PATH` — that row requires *distinct* identity, the opposite direction."**

**That is exactly right and the Session had not drawn it out.** **`O-ALTERNATE-PATH` is defined as *"Verified
distinct context identity (`edi≠device+0x2268`)"* — a row for a context that is NOT the device sub-object.**
**The Worker's finding says the context IS `device+0x2268`.** **So the finding points at the OPPOSITE row, and
the Worker picked the one whose predicate its own evidence contradicts.**

> **The probable outcome, in advance: `O-OPEN` SUSTAINED, with the edge ADVANCED — the identity is resolved,
> so the precise unknown becomes the WRITER of `device+0x242C`.**

**That is a better outcome than a positive row, because it names a narrower unknown than the Session started
with.**

## The two conditions the Session must satisfy

**1. Stage-1 acceptance of the Worker's evidence MUST include explicit BYTE CHECKS** for:
- the store at **`0x0019460A`** (`mov dword ptr [ecx], 0xfd000000`);
- the **`lea` at `0x001925FB`** (`lea edi,[esi+0x2268]`);
- the **DPC binding** (`sub_00194ADD`, `KeInitializeDpc(routine=0x00194480, DeferredContext=context)`);
- the **thunk binding** (`sub_00194EEF` → IAT → `sub_001941E0` recovering `arg−0x1A0`).

**2. The BINDING RE-READ PRECONDITION — before any row leans on a memory-derived value:**

> **Every row-feeding, uncorroborated `inspect-jsrf.py memory` read must be RE-READ with an OFFSET-SHIFT
> control.** **Exempt only values with independent corroboration.**

**The Session has NOT re-derived `MEM32(device+0x242C) = 0x0015F9D0` and must not let any row use it until it
does.** **The offset-shift control is the discriminator the Session's original control was not** — it must
read a known string at **two adjacent addresses** and check that the second shifts as a little-endian DWORD
would.

## R-1's scope is UNCHANGED — and the reason matters

**The Advisor states it explicitly:** *"mapping-inadmissibility is orthogonal to byte semantics, and its
demotion stands regardless."*

**So the erratum at `a2h-named-producer-frame-dump-window-erratum.md` is unaffected by this retraction.**
**Two different defects — a mismatched dump, and a misread byte order — and fixing one does not touch the
other.** **Recorded so a future reader does not conflate them.**

## The note the Session should take seriously

> **"seven extraction failures with controls catching each is now a validated method, not just confessions."**

**The Session had been recording these as failures.** **The Advisor reframes them: the controls CAUGHT each
one, and the bounded re-read rule is the systemic form of that method.** **That is a fair reading — and it
means the practice note should be updated to say the method WORKS, not merely that the Session errs.**
