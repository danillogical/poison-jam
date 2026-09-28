# Advisor re-ruling — arming-coverage successor after the Session's `PREMISE_CHANGED`

**Session:** `session-9f8c9988-38fb-4cc9-a188-a6881a52559a`, 2026-09-28, DSH.
**Advisor:** persistent handle `muse_FkNhGaXtV9P5`, model `muse-spark-1.3-contributor`, turn
`01a0e763-5bcc-7000-ab87-0c40bd269fc8`.
**Raised because:** the Session corrected its own defect analysis (the "never attempted" claim was withdrawn),
which changed the premise the Advisor's earlier `SHAPE: PROCEED` ruling rested on. Per
`docs/agent-workflow.md` §4.3 a changed load-bearing premise must be re-referred.
**Supersedes:** correction 1 of `docs/reviews/a2h-arming-coverage-advisor-shape-preflight.md`.

**Ruling recorded verbatim below.**

---

## RULING

Verdict first: correction 1 as stated is **superseded in its mechanism half, survives as accounting** — net: reporting-only fix CONFIRMED. But your new inference contains its own error, which I must correct before ruling the questions.

**`cleared=17` does NOT prove seventeen were armed.** `dr_disarm_all` (`collect.c:300-341`) zeroes DR0/DR7 on *every* live thread unconditionally, and `dr_disarmed++` fires whenever the zeroing succeeds — including threads never armed (readback `0 & OWNED` is falsy → counted). So `cleared=17` proves seventeen threads *existed at teardown*, nothing about arming. Your "seven arms happened after the summary" is therefore unsupported — the invisible population's *size* is unknown. That is not a flaw in the fix; it is the same absent-record trap a third time (tid list *undercounts* arms; `cleared` *overcounts* them), and it is exactly why only positive per-arm records will do.

What stands: the sweep exists with correct install-ordering (`collect.c:157-181`, `collect.c:198-218`); CREATE_THREAD prints only on failure (`collect.c:704-720`); successes append to the tid list (`collect.c:149-152`) but print only at summary (`collect.c:185-196`). Mechanism adequate; recording inadequate. Proceed on that basis.

1. **Correction 1 as stated: NO — shrink it.** The sweep + install-ordering + create_thread path already exist; no new arming mechanism is required. What is required is making every arm *visible*: print every successful arm (tid, why, DR7 readback, seq/tick) as it happens, plus a terminal full-list summary. The (a)-queue adds nothing the handshake sweep doesn't already re-arm; drop it. Reject (c) unchanged.
2. **Yes — per-birth record required, sharpened.** Log EVERY create-thread debug event (tid, timestamp, mapping-available?, armed/deferred/failed/exited), not just arming attempts — otherwise "path didn't fire" stays indistinguishable. Terminal reconciliation must cover every dispatching tid (the six: 50616, 58620, 60872, 66492, 67988, 57376) with an operational dispatch criterion stated explicitly; any dispatching tid without an arm record → `UNKNOWN`. The nine transient/host tids matter only if dispatch-status is unproven — then they join the `UNKNOWN` set, never the absence set.
3. **Fresh OFF: confirmed.** Code change breaks the carry rule. Same-build OFF/ON.
4. **Keep N = 5.** Coverage bar got *higher*, so effective yield may drop — reducing N would be perverse. Pre-specified early stop only after two coverage-complete TARGETs with agreement; zero targets → report rate, stop, re-refer.
5. **Scope confirmed** (fix reporting → fresh OFF → re-run → attribute; census, second-slot, read-path legs untouched), **plus one required element**: bound pre-mapping exits — a tid born before mapping-available that exits before the handshake and dispatched guest code is uncovered by sweep *and* queue; prove none exists (ICALL-tid lines + their completeness argument) or accept `UNKNOWN`.
6. **Bracket changes nothing.** It narrows the window and corroborates direction, but writer identity still needs the trap. No row/instrument change.

**BASIS:** observed — cited source lines above; your table/registry/bracket figures as Session-measured leads. Inferred — queue adds nothing (sweep covers live set). Uncertain — post-summary arm count (precisely what the fix must make observable).

**REVERSED BY:** evidence the sweep misses live threads it should catch (→ mechanism fix, not just reporting).

**RECORD IN:** this ruling in the successor's review record; correct the `cleared=17` inference in the evidence file (it currently asserts 17 armed).

---

## Session verification of the Advisor's correction

**The Advisor is RIGHT and the Session was wrong a second time.** Verified from source:

```c
collect.c:313   if (Thread32First(snapshot, &entry)) do {
collect.c:316       if (entry.th32OwnerProcessID != process_id) continue;
collect.c:317       handle = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | ...);
collect.c:324       context.Dr0 = context.Dr1 = context.Dr2 = context.Dr3 = 0;
collect.c:325       context.Dr7 = 0;
collect.c:326       if (!SetThreadContext(handle, &context)) { dr_disarm_failed++; ... }
collect.c:330       if (!GetThreadContext(handle, &back) || (back.Dr7 & A2H_DR7_OWNED) || back.Dr0) {
collect.c:331           dr_disarm_failed++; ...
collect.c:334       } else dr_disarmed++;
```

**It walks every thread, zeroes the debug registers unconditionally, and counts a clean readback.** A thread
that was **never armed** already has `Dr7 == 0`, so zeroing succeeds, the readback is clean, **and it is
counted.** **`cleared=17` therefore measures threads EXISTING at teardown, not threads armed.**

**The evidence file has been corrected** (`6bdd5d4`), including the withdrawal of the Session's own
"seven arms happened after the summary."

## The error class, recorded because it recurred three times in one afternoon

| # | Claim | Failure mode |
|---|---|---|
| 1 | *The tid list is the set of armed threads* | **Undercounts** — a successful post-handshake arm prints nothing |
| 2 | *`cleared=17` is the number armed* | **Overcounts** — counts every live thread, armed or not |
| 3 | *(Advisor)* | **Neither count is reliable; only POSITIVE PER-ARM RECORDS will do** |

**The Session made error 1, then made error 2 while correcting error 1; the Advisor caught error 2.** In both
cases the artifact's summary line was **true** and the **conclusion drawn from it was false**. **This is the
absent-record-as-negative error class this line keeps meeting, committed twice by the Session while
documenting it** — and it is why the Advisor's correction 2 is now the **central** requirement: **without a
positive per-arm record, no count in this archive can settle coverage.**
