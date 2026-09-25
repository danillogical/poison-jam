# Startup receipt — session-910703eb (2026-09-25)

**Session:** `session-910703eb-aed1-489e-9c5c-1468f8bc7c20` — a **genuinely new top-level session**.
Verified against `$env:DSH_SESSION_ID` directly. It is **neither** of the two sessions the owner
named as previous (`session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9`,
`session-14a92b99-dc90-4f0e-a221-4aede1d6a4b0`). This matters because the route allow-list is a
**write-once per-session latch** (recorded in `docs/reviews/startup-20260925-session-4a79ce06.md`):
a session composed *after* the `settings.yaml` edit captures the new list, which is exactly what
the owner's remedy required and what the previous session could not obtain.

## Owner pre-flight (three checks, all PASS)

| # | Check | Result |
|---|---|---|
| 1 | `$env:DSH_SESSION_ID` is not either previous session | **PASS** — `session-910703eb-aed1-489e-9c5c-1468f8bc7c20` |
| 2 | `subagent-model-selection.allowedModels` contains `workbuddy-ai` / `gpt-5.6-sol` | **PASS** — present at `settings.yaml:32-33` (12 entries total) |
| 3 | `workbuddy-ai/gpt-5.6-sol` resolves live and is usable by this Session | **PASS** — see below |

**Check 3 evidence.** `list_subagent_models(provider: "workbuddy-ai", model: "gpt-5.6-sol")` returns
exactly one advertised route: **`workbuddy-ai/gpt-5.6-sol` — GPT-5.6-Sol · x3.47**, with efforts
`off, low, medium, high, xhigh, max` — so the required `high` and `medium` are both available. This
is a **different result from the previous session**, which got
`child LLM route "workbuddy-ai/gpt-5.6-sol" is not allowed for this Session`. The route is now both
**permitted** (check 2) and **usable** (a live child answered on it: the second-stage reviewer probe
below ran on it and returned). The remedy recorded in
`docs/reviews/route-failure-20260924-workbuddy-substitute.md` is therefore **confirmed effective**,
and the `BLOCKED` state in the plan's `ROUTE STATE` block is **superseded by this receipt**.

> **Note on the previous session's `list_subagent_models` finding.** That receipt observed the
> provider listing only 4 models and correctly diagnosed it as allow-list **filtering**. This session
> lists 22-model-catalog membership differently: the resolver returns the requested route directly.
> No contradiction — the filter explanation still holds; the list simply now admits the route.

## Temporary owner-authorized staffing (this session only)

The owner authorized, for this session, a **temporary exception to the normal no-substitution rule**:

| Role | Authorized route | Effort |
|---|---|---|
| Session | `workbuddy-ai/deepseek-v4.1-flash` | `max` |
| Worker subagents | `workbuddy-ai/deepseek-v4.1-flash` | `max` |
| Acceptance reviewer (first stage) | `workbuddy-ai/deepseek-v4.1-flash` | `max` |
| **Planner** | `workbuddy-ai/gpt-5.6-sol` | `high` |
| **Persistent Advisor** | `workbuddy-ai/gpt-5.6-sol` | `high` |
| Acceptance reviewer (second stage, when required) | `workbuddy-ai/gpt-5.6-sol` | `medium` |

**`docs/agent-workflow.md` §1 was NOT edited.** Staffing/model assignments in §1 are an
**owner-reserved decision** (§3.4), and the owner framed this as *temporary* and session-scoped. The
§1 DSH column still names Claude Opus 5.5 for Planner/Advisor/second-stage; this receipt records the
owner's standing instruction as the authority for using the substitute **in this session**. If the
owner wants it to persist, §1 is the place to change it — one edit, then a repository sweep for
stale copied assignments (§1 "Staffing rule").

**Role authority is unchanged and remains attached to the role, not the model** (§1). The
`gpt-5.6-sol` Planner is bound by §5.1 exactly as an Opus Planner would be; the `gpt-5.6-sol` Advisor
holds Advisor authority under §2.3; the `deepseek-v4.1-flash` first-stage reviewer is bound by §2.2
exactly as any reviewer. **Existing Advisor rulings are not reopened by this provider change.**

## Identity and handoff

- **Session ID/date/harness:** `session-910703eb-aed1-489e-9c5c-1468f8bc7c20` / 2026-09-25 / DSH (Web GUI, `http://127.0.0.1:3080`).
- **Actual main model/effort:** **UNKNOWN for self** (a Session cannot verify its own route from
  inside; no harness metadata exposed it this session). `settings.yaml:3-6` declares
  `agent-default-model: workbuddy-ai / deepseek-v4.1-flash / max`, which matches the owner's
  authorization. Recorded as declared, not as measured.
- **Workflow/plan/run-profile revisions and dirty diff identity** (SHA-256, measured this session):
  - `docs/agent-workflow.md` — `C240E510D84784388DED3A0590C10C90736F375254F1FAD99B6A53A55A3F5D40`
  - `AGENTS.md` — `D928C414056D584C01BC0F4867A7637841EE72AF736C5BC46E0D32D92248D529`
  - `plan-jsrf-bare-minimum.md` — `EFACB02DEBE31311C3D79C934E368A0B01DE6B43F04D41AFA316DDCC79FE24B2`
  - `docs/jsrf-run-profiles.md` — `09D200DC78C3DFD9EAF6735A06D2B3429FBC500DE58C27881C6CE8481A3A812C`
  - Both working trees **clean**; no unrelated dirty edits to preserve.
- **Game revision/status:** `1b1789b5d8f2de8485b2c4e50d653a7c7635630c` (branch `master`),
  **clean**, `git remote` prints nothing. (`git status --porcelain` empty.)
- **Toolkit revision/status:** `0d7929c86771dd0b971941592fd4f15436116e82` (branch `main`),
  **clean**, `@{u}` = `upstream/main`, no `a4s-*` branch exists.
  `upstream/main` = `origin/main` = `v0.11.0^{commit}` = `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b`;
  merge base `051a128df5ec27ef14f1ceaaead11c5457321eef`; `status -sb` = `main...upstream/main [ahead 24, behind 169]`.
  A single `warning: could not open directory '.pytest_cache/': Permission denied` appears in
  `git status` output on the toolkit; it is a **pre-existing untracked-directory read warning, not a
  tracked-file modification** — `--porcelain` is empty, so the tree is clean.
- **Handoff verification:** every repository fact the owner's handoff and the two route-failure
  records assert **reproduces exactly**. The one difference from the *previous session's* receipt is
  the session id itself and the now-resolvable route — both intended by the owner's remedy.
- **Unrelated edits preserved:** none existed; nothing was reverted or cleaned.
- **CURRENT PACKET copied from plan:** **NONE.** The plan's `CURRENT PACKET` block reads
  `none (toolkit sync A4s-r5 awaiting re-review)`. `A4s-r5` is written but **not** frozen or
  promoted; promotion requires the `ADEQUATE` verdict now in flight.
- **Dependencies and their recorded acceptance reviews:** `A4p-r1` ACCEPTED (discovery,
  `docs/reviews/a4p-r1-acceptance-review.md`, first stage, final). `A3a-r25` ACCEPTED
  (`docs/reviews/a3a-r25-acceptance-review.md`). No `A4b` code exists yet.
- **Next exact authorized action:** the `A4s-r5` fresh-Planner §5.4 re-review (queued item 1),
  dispatched this session; on `ADEQUATE` with zero blocking defects → freeze that exact revision and
  promote it into `CURRENT PACKET` in the same step (§5.3).
- **Build/run owner and worker write ownership:** Session, single owner. No worker is authorized to
  build, run, or integrate. `A4s` execution (if promoted) writes only to toolkit `main` + the two
  local `a4s-*` branches, game `docs/reviews/a4s-execution-evidence.md`, `logs/a4s/`, `logs/runs/`,
  and the build tree.

## Route resolution — PASS

| Role | Requested | Returned / evidence |
|---|---|---|
| Planner | `workbuddy-ai/gpt-5.6-sol` @ `high` | **resolved** — child `f339b306-0473-4777-a211-78271453f2b9`, running |
| Persistent advisor | `workbuddy-ai/gpt-5.6-sol` @ `high` | **resolved** — child `3eea6b9f-f725-48fb-8a5e-717162af8c8e` |
| Acceptance reviewer (first stage) | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | **resolved** — child `0543599b-f9b3-45ed-ac0a-085f46360f02` |
| Acceptance reviewer (second stage) | `workbuddy-ai/gpt-5.6-sol` @ `medium` | **resolved** — child `94cf9e29-efb5-4a90-bf12-9597d556d9e9` |
| Workers | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | **resolved** — same route as the first-stage reviewer, which answered |
| Session | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | **declared** (`settings.yaml:3-6`); self-route UNKNOWN |

**Exact error or ambiguity:** none. No route was substituted, and no unlisted route was used.
The `claude/claude-opus-5-5` route is **not used this session** and was **not probed** — the owner's
temporary staffing replaces it outright, so its recovery state is not relevant here.

**Route self-report discrepancy (recorded, not load-bearing).** The second-stage probe child
self-reported its route as `openai/gpt-5.6-sol`, while the harness resolves it as
`workbuddy-ai/gpt-5.6-sol`. A child's self-report is a **lead, not evidence** (§2.4.2); the
authoritative resolution is `list_subagent_models`, which returned the `workbuddy-ai` provider with
canonical model identity `gpt-5.6-sol`. Recorded for transparency; it does not affect the verdict.

## Acceptance reviewer probes — PASS (both stages)

- **First stage** — child `0543599b-f9b3-45ed-ac0a-085f46360f02`, route
  `workbuddy-ai/deepseek-v4.1-flash` @ `max`. Fresh token **`7F2A9C4E1B60`** (generated by the child,
  not supplied by the Session). Empty-evidence answer: *"An empty evidence set cannot support any
  positive acceptance claim, so the only sound verdict is fail — absence of evidence is not evidence
  of absence of defects."* **Result: PASS.**
- **Second stage** — child `94cf9e29-efb5-4a90-bf12-9597d556d9e9`, route
  `workbuddy-ai/gpt-5.6-sol` @ `medium`. Fresh token **`A7C3E91F4B2D`** (distinct from the first
  stage's). Empty-evidence answer: *"An empty evidence set must fail acceptance because it provides no
  verifiable proof that any acceptance criterion has been satisfied."* **Result: PASS.**
- **Exact error or missing evidence:** none. Both probes returned a completed response with a fresh
  token and a correct empty-evidence reason, so neither is a dispatch-only PASS.

> The second stage runs **only** when a first-stage review returns `NOT ACCEPTED` (§2.2). It was
> probed now because §0.3 requires both stages probed at startup; it is **not** engaged for `A4s-r5`.

## Persistent advisor probe — PASS

- **Child ID:** `3eea6b9f-f725-48fb-8a5e-717162af8c8e`, route `workbuddy-ai/gpt-5.6-sol` @ `high`.
  Spawned as a fresh continuable child with `run_in_background: true` (§4.4).
- **Turn 1 reference; unique marker given:** marker `ADV-PROBE-9C41D7`, with an explicit instruction
  **not** to echo it in turn 1.
- **Named file and the fact deliberately omitted from the brief:**
  `docs/reviews/a4b-gpin-accounting-ruling.md`. The brief asked only for (1) the finite universes the
  redesign keys on and (2) the document/section where its general rule is recorded. **Neither the
  four universe sizes nor the record location appeared anywhere in the brief.**
- **Advisor's answer; checked against the file:** MIXBUF **32** bins; PERIPH **128** offsets;
  FIFO_READ **6** FIFOs (4 output + 2 input); DMA_READ **4** region classes
  (LOW_RAM/CONTIG/DEVICE/OTHER_MAPPED); general rule at **`docs/agent-workflow.md` §6.1 item 6(b)**.
  **Session-verified against both files:** the ruling's (a) states exactly those four universes with
  those sizes, and `docs/agent-workflow.md:643-655` is the numbered item **6b** ("Decision inputs are
  bounded by construction") carrying that rule. **Correct on both counts.**
- **Turn 2 reference (same child, marker not repeated); returned marker:** the first turn-2 request
  returned the universe list again **without** the marker; a direct re-ask ("if you do not have it,
  reply `MARKER: UNKNOWN`") to the **same child** returned **`ADV-PROBE-9C41D7`** exactly, plus a
  statement that turn 1's sizes came from reading the file itself.
- **Result: PASS** — correct fact, verified against the file, and the correct marker from the same
  child.
- **Exact error or missing evidence:** none. **Reliability note for §4.4:** the marker needed a
  second ask on turn 2. The child retained it (it was returned unprompted-on-retry, not guessed), so
  continuity is established, but this child showed one sloppy turn. If it does so again on a real
  ruling — answering without the requested field — the Session will treat its state as unreliable and
  spawn a replacement Advisor per §4.4. Recorded prospectively, not as a failure.

## Packet readiness — BLOCKED (pending the `A4s-r5` verdict)

- **Frozen revision/hash matches `CURRENT PACKET`:** `CURRENT PACKET` is **none**. `A4s-r5` is
  written and its hash is verified below, but it is **not frozen and not promoted**.
- **`A4s-r5` identity (Session-measured):** `docs/packets/a4s-toolkit-sync.md`, SHA-256
  **`09DA9413C028D61BD28D9E4007AF6DDE3474ED6869F77B0095D03DB2C4FB86FB`**, 263 lines — **matches the
  plan's recorded hash exactly**.
- **`A4b1-r3` / `A4b2-r3` identity (Session-measured, unchanged):**
  `321ABCF7C9319B5AC4661B384A21CA2D2CE39C792D9820C9C322B7979EC7AFDA` (373 lines) and
  `CFB8C0EBFBC9BE3CFB62677C4C756694DDE9663542E239570B639DED9F5BFCE9` (310 lines) — **both match the
  plan's recorded hashes exactly**, confirming the route outage wrote nothing.
- **Adequacy review record and verdict:** `docs/reviews/a4s-r4-adequacy-review.md` —
  **`INADEQUATE`** (B1-r4: non-unique end anchors, a guaranteed false FAIL). `r5`'s re-review is
  **in flight** this session on the fresh Planner; no verdict exists yet.
- **Deferred advisories (recorded, not acted on):** r4's D1–D4, folded into `r5` as wording and
  disposition changes (the `main.c:318`-only expectation; the "admitted model text unchanged"
  disposition; the boundary-guard limits; the `MIXDOWN_ALL`/macro limits kept as claim limits).
  Per §5.4, deferred advisories **do not** reopen a frozen packet and are **not** a revision trigger.
- **Prerequisites / tooling checks:** not run this session — the packet is not promoted, so no build,
  merge, or guest run is authorized. `A4s`'s own `Readiness` block requires the Session to verify each
  command runs **before freezing**; that verification is the next step **after** an `ADEQUATE`
  verdict, not before it.
- **State/plan disagreements and how they were escalated:** one, resolved without escalation. The
  plan's `ROUTE STATE` block records both senior-judgment routes as `BLOCKED` and the substitute as
  unresolvable. **That is now stale**: the owner's remedy is in effect and the route resolves in this
  new session. Per §5.4 this is not a packet revision and not an Advisor question — it is a plan
  status update the Session owns, recorded in the plan at closure of this startup step.
- **Overall disposition and next action:** startup **PASS**; packet readiness **BLOCKED** only in the
  sense that no packet is promoted — which is the correct, expected state. **Next action:** await the
  `A4s-r5` re-review. On `ADEQUATE` with zero blocking defects, freeze and promote **that exact
  revision** in the same step (§5.3), with no discretion to revise first, then execute `A4s`.

## Startup checklist (§0) — all six items

| §0 item | Result |
|---|---|
| 1. Load current authority | **PASS** — workflow, plan `CURRENT PACKET`, run profiles read; both repo identities and dirty state recorded |
| 2. Verify routes | **PASS** — every role resolved live; Session self-route UNKNOWN (declared in `settings.yaml`) |
| 3. Probe both Acceptance reviewer stages | **PASS** — both returned a completed response with a fresh token and a correct empty-evidence reason |
| 4. Probe the Advisor (one combined probe) | **PASS** — correct file fact verified against the file, and the correct marker from the same child |
| 5. Reconcile packet state | **PASS** — both trees inspected and clean; `CURRENT PACKET` is `none`, so no implementation work is authorized or started |
| 6. Persist this receipt | **PASS** — this file |

**Do not mark readiness PASS with a failed or unknown required item.** No required item failed. The
Session self-route is `UNKNOWN` and is **not** a required startup item (§0.2 requires recording
`UNKNOWN` if unverifiable, which is what was done). A provider catalog entry is not a completed
invocation — every route above is backed by a **child that actually answered**, not by catalog
membership. A readiness response is not acceptance of a game packet.

## What this session did NOT do

- Did **not** execute the merge, build, or any guest run (`CURRENT PACKET` is `none`).
- Did **not** freeze, promote, or edit `A4s-r5`, `A4b1-r3` or `A4b2-r3`.
- Did **not** edit `docs/agent-workflow.md` §1, `settings.yaml`, or any policy document.
- Did **not** reopen any Advisor ruling, and did **not** restart broad DSP research.
- Did **not** resurrect the rejected 256-entry accounting mechanism.
- Did **not** push, fetch, or modify any remote; the game repository still has no remote.
- Did **not** run `git merge-tree --write-tree` in a way that left state behind — the one read-only
  invocation created a dangling tree object in the toolkit's object database (no ref, no branch, no
  working-tree change; `git status --porcelain` still empty). Recorded because it is a repository
  write, however inert.
