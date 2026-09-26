# Owner instruction: durable-checkpoint pushes to the xboxrecomp fork

**Recorded:** 2026-09-25, `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`.
**Authority:** direct owner instruction. It **supplements** the closure rules; it does **not** make a
failed, rolled-back, or unaccepted state pushable. `docs/agent-workflow.md` still owns the packet
lifecycle; `AGENTS.md` carries the operating summary of this policy.

## Remote identity — VERIFIED from `git remote -v`, not assumed

Inspected this session with `git -C C:\Users\logic\Repos\xboxrecomp remote -v`:

| Remote | Fetch URL | Push URL |
|---|---|---|
| **`origin`** | `https://github.com/danillogical/xboxrecomp.git` | `https://github.com/danillogical/xboxrecomp.git` |
| **`upstream`** | `https://github.com/sp00nznet/xboxrecomp.git` | **`DISABLED`** |

- **The owner's fork is already configured, and it is named `origin`** — *not* `fork`. No remote was
  added, renamed, or altered; none needed to be.
- `upstream` is present, fetch-only, with its push URL literally `DISABLED`. **It was not touched.**
- The instruction's suggested name `fork` was therefore **not used**, because adding a second remote for
  the same URL would create two names for one destination — a way to push to the wrong place by
  accident. The verified mapping is recorded above instead.

**Toolkit state at the time of recording:** branch `main`, HEAD
`0d7929c86771dd0b971941592fd4f15436116e82`, working tree **clean**, `@{u}` = `upstream/main`.

## Operational constraint discovered while verifying — recorded because it decides HOW a future push works

| Fact | Measured |
|---|---|
| Fork `main` (`origin/main`) | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` — *"Release v0.11.0"*, i.e. **upstream's release** |
| Local `main` | `0d7929c86771dd0b971941592fd4f15436116e82` |
| Is `origin/main` an ancestor of local `main`? | **NO** |
| Divergence | **169 upstream-only / 24 local-only** commits |
| Would `git push origin main` be a fast-forward? | **NO — a normal push would be rejected** |

**Consequence.** The fork's `main` tracks upstream's release line; the local `main` is the JSRF
integration line. They have diverged, so the fork cannot accept local `main` as a fast-forward **until
the `A4s` merge lands `766ecef` in local `main`'s history**. That is exactly what `A4s` is for, and it
is why the packet's Closure push is written as *"a fast-forward (`M` descends from `origin/main` =
`766ecef`)"*.

**Therefore the policy and the packet already agree**, and the correct behaviour is:

1. **Now:** nothing is pushable. The current `A4s-r5` state is `R-CONFLICT` — a rollback — which the
   owner's instruction explicitly names as a **no-push state**. The 24 local commits remain unpushed.
2. **At a successful `A4s` closure:** `main` will descend from `766ecef`, so `git push origin main`
   becomes a **fast-forward** and is the correct durable-checkpoint push. This is the same push the
   packet's Closure already specifies.
3. **Never** manufacture a merge to make the push possible — the owner said so explicitly, and
   `A4s-r6`/the workflow decides when a merge is valid.
4. If a future accepted state genuinely belongs on a branch rather than `main`, push that branch and
   record its name (owner's branch-handling rule).

## The policy, as instructed

**Treat regular pushes to the fork as part of normal durable closure for toolkit work**, not only at the
end of the JSRF project.

**A durable checkpoint includes:** an accepted toolkit implementation; a successfully completed
toolkit-sync/merge packet; a materially useful toolkit commit that later packets depend on; or a clean
milestone boundary before beginning a substantially different area of work.

**Before every push, all five checks:**

1. verify the xboxrecomp working tree is **clean**;
2. verify the commit/branch being pushed is the **intended durable state**;
3. verify the **tests/acceptance required by the active packet have passed**;
4. verify the destination remote is **the fork, not `upstream`** (per the verified table above: the fork
   is `origin`);
5. record the **local commit SHA and destination branch**.

Then push the branch normally to the fork.

**Never push:** failed or rolled-back packet states; temporary conflict branches; incomplete
experiments; commits whose acceptance/review is still pending; dirty working trees; or a state the
active packet explicitly forbids pushing. **`R-CONFLICT`, rollback, `INADEQUATE`, and equivalent
fail-closed outcomes remain no-push states.**

**Never use `--force` or `--force-with-lease`** unless the owner explicitly authorizes it.
**Never push to `upstream` (`sp00nznet/xboxrecomp`)** unless the owner explicitly asks.

**Branch handling:** if the accepted work belongs cleanly on the fork's `main`, push the durable `main`
state. If the work is intentionally preserved on a feature/integration branch, push that branch and
record its name. **Do not manufacture a merge into `main` merely to satisfy this policy.**

## Closure record format required for each push

```text
PUSHED_TO:
BRANCH:
COMMIT:
REMOTE_URL:
RESULT:
```

### Push 2 — the accepted `A4s-r6` merge `M` (2026-09-25)

**The packet's Closure push**, executed only after the stage-1 acceptance review returned `ACCEPT`
(`BLOCKING: NONE`, all ten mandatory criteria `AGREED`) — which is what satisfies pre-push check 3.

```text
PUSHED_TO:  https://github.com/danillogical/xboxrecomp.git   (remote `origin` — the owner's fork)
BRANCH:     main
COMMIT:     3f8bf67c450861aefcbc376698750bc1446bc9dd
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT:     SUCCESS — fast-forward `766ecef..3f8bf67  main -> main`, verified independently:
            `git rev-parse origin/main` = 3f8bf67c… and
            `git ls-remote origin refs/heads/main` = 3f8bf67c450861aefcbc376698750bc1446bc9dd
```

**All five pre-push checks, as run:**

| # | Check | Result |
|---|---|---|
| 1 | toolkit working tree **clean** | **PASS** — `status --porcelain` empty |
| 2 | commit is the **intended durable state** | **PASS** — `HEAD` = `M` = `3f8bf67c…` |
| 3 | the active packet's **tests/acceptance passed** | **PASS** — stage-1 acceptance `ACCEPT`; the two **Q-C claim limits** were shown to the reviewer explicitly, as the Advisor required |
| 4 | destination is the **fork, not `upstream`** | **PASS** — push URL `danillogical/xboxrecomp.git`; `upstream` push URL `DISABLED`, asserted before pushing |
| 5 | local **SHA and branch recorded** | **PASS** — recorded above |

**No force, no other refspec, no `upstream` push.** `-u` moved `main`'s tracking from `upstream/main` to
`origin/main` — **recorded, not reverted**, exactly as the packet specifies. `upstream/main` is still
`766ecef` (upstream's own release commit), verified untouched afterwards. The toolkit is clean at `M` and
`a4s-pre-sync` still resolves to `0d7929c`.

**What this push contains:** the accepted merge `M` (parents `0d7929c`, `766ecef`) — the whole `A4s`
toolkit sync. It is the first push of `main` to the fork; the earlier `jsrf/integration` branch (push 1)
still points at the accepted baseline `0d7929c` and is left in place.

**This is the durable checkpoint the owner's policy asks for:** an accepted toolkit implementation, a
successfully completed toolkit-sync/merge packet, and a clean milestone boundary before the substantially
different `A4b1`/`A4b2` (GP/DSP) area begins.

## Push log

### Push 1 — the accepted JSRF toolkit baseline (2026-09-25)

**Owner-authorized.** The owner asked why no push existed yet and, on being shown the measured
constraint below, authorized publishing the accepted baseline to a new branch.

```text
PUSHED_TO:  https://github.com/danillogical/xboxrecomp.git   (remote `origin` — the owner's fork)
BRANCH:     jsrf/integration   (NEW branch; `main` cannot fast-forward, see below)
COMMIT:     0d7929c86771dd0b971941592fd4f15436116e82
REMOTE_URL: https://github.com/danillogical/xboxrecomp.git
RESULT:     SUCCESS — verified independently with `git ls-remote origin`, which now reports
            0d7929c86771dd0b971941592fd4f15436116e82  refs/heads/jsrf/integration
            and 766ecefcd7fb2a9b344de8ec891f6fe9ea14261b  refs/heads/main (unchanged)
```

**All five pre-push checks, as run:**

| # | Check | Result |
|---|---|---|
| 1 | toolkit working tree **clean** | **PASS** — `git status --porcelain` empty |
| 2 | commit is the **intended durable state** | **PASS** — `HEAD` = `0d7929c`, the accepted baseline |
| 3 | the active packet's **tests/acceptance passed** | **PASS** — see the reasoning below |
| 4 | destination is the **fork, not `upstream`** | **PASS** — push URL is `danillogical/xboxrecomp.git`; `upstream` push URL is `DISABLED`, asserted before pushing |
| 5 | local **SHA and branch recorded** | **PASS** — recorded above |

**Why check 3 passes, stated precisely, because it is the one that needs care.** The active packet
`A4s-r5` selected `R-CONFLICT`, and its merge was aborted, so **it produced no commit at all** — nothing
of the failed attempt is in this push. The 24 commits pushed are the **accepted baseline**: `A4a-r2`,
`A3a-r25` and `A4p-r1` were all accepted against toolkit `0d7929c`, and the baseline's tests were
re-measured this session during `A4s-r5` step 2 — `ctest` **12/12 PASS**, set K4 **27 tests OK**, set KX
**33 modules, Ran 133 OK**, with the pre-merge executable hashing to `9597FF7C…`, byte-identical to the
archived accepted `A4a-r2` R0 run.

**No force, no upstream push, no `main` push.** `--force`/`--force-with-lease` were not used. `upstream`
was verified untouched afterwards (`upstream/main` is still `766ecef`, which is upstream's own release
commit, not something we pushed). The fork's `main` is **unchanged at `766ecef`** — deliberately, since
`main` cannot be fast-forwarded until the `A4s` merge lands `766ecef` in local `main`'s history.

**Why a branch and not `main`.** `origin/main` (`766ecef`, *"Release v0.11.0"*) is **not** an ancestor of
local `main` (`0d7929c`); the two have diverged **169 upstream-only / 24 local-only**. A push to `main`
would therefore be **non-fast-forward and rejected**, and the owner's policy forbids manufacturing a
merge to satisfy the policy. The owner's branch-handling rule covers exactly this case: *"If the work is
intentionally being preserved on a feature/integration branch, push that branch to the fork instead and
record its name."* When `A4s-r6` produces an accepted merge `M` that descends from `766ecef`, `main`
becomes a clean fast-forward and the durable `main` push follows the same five checks.

**Not pushed, and why:** the `A4s-r5` `R-CONFLICT` rollback (no commit existed; rollback is a no-push
state); no temporary conflict branch; no `a4s-pre-sync` (a recovery ref, not durable work); no
incomplete experiment; no pending-acceptance state.

## Status of the current `A4s` work

The `A4s-r5` `R-CONFLICT` rollback **remains correctly unpushed**. Continue `A4s` normally; when a later
`A4s` revision produces an **accepted durable toolkit state**, push that resulting branch/commit to the
fork as part of closure and record the five fields above.

## Interaction with the previous push policy

`AGENTS.md` "Toolkit remotes" previously recorded *"push toolkit `main` to `origin` when a packet
closes"*. That remains true and is **consistent** with this instruction — this instruction makes the
push a **standing durable-checkpoint practice** rather than a once-at-closure event, and adds the
explicit no-push list, the force prohibition, and the closure record format. `AGENTS.md` is updated to
point here for the full policy.
