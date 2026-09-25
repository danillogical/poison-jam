# Route failure: Claude Opus 5.5 pool exhausted (2026-09-24)

**What failed.** Every child on the `claude` / `claude-opus-5-5` route failed with the same
provider error:

```text
pool "claude-opus-5-5" exhausted: every member is unavailable or failed
code: RATE_LIMIT
providerRetryAfterMs: 11305482   (~3.14 hours)
```

Three children failed in sequence, none from anything in this session:

| Child | Role | State |
|---|---|---|
| `407c54a3-6ca4-4a65-835b-faf5355195cd` | **Persistent advisor** | failed at turn 10, 81 tool calls in |
| `ff75a9bc-69e3-4576-b22a-6ab35faca6fd` | `A4s-r2` re-reviewer (Planner) | failed at turn 1, step 1 — never started |
| `ba13f19f-0cd1-448d-851a-73d588323358` | `A4b1`/`A4b2` r2 revisor (Planner) | failed |

A direct one-line availability probe (`Reply with exactly: ROUTE-OK`) also failed, so the
route is down, not merely slow. `list_subagent_models` still advertises
`claude/claude-opus-5-5` with efforts `low`–`max` — **the catalog entry is stale**, which is
exactly why §1 requires a live probe rather than trusting the catalog.

**What this blocks.** Every role assigned to that route by workflow §1:

- **Persistent advisor** — the AC'97 hunk policy question (`docs/reviews/a4s-ac97-hunk-question.md`) is unanswered.
- **Planner** — the `A4s-r2` §5.4 re-review cannot run, so `A4s` cannot be promoted or executed; the `A4b1`/`A4b2` r2 revision cannot be written or reviewed.
- **Acceptance reviewer, second stage** — unavailable (not currently needed; no first-stage `NOT ACCEPTED` is pending).

**What is NOT blocked.** The `workbuddy-ai` route is healthy and was used successfully this
session: Session (me), worker subagents, and the **first-stage** acceptance reviewer
(`workbuddy-ai/deepseek-v4.1-flash` @ `max`).

**Disposition — `BLOCKED` for Planner/Advisor work, per §1** ("An unavailable assignment is
`BLOCKED`; never fall back silently"). **No substitution is permitted**: §1 forbids inventing
a route, and the roster's Planner/Advisor rows name Claude Opus 5.5 specifically. The Session
did **not**:

- promote or execute `A4s-r2` (it needs an `ADEQUATE` adequacy verdict that only a Planner can give);
- revise `A4b1`/`A4b2` on its own authority (criterion repair is the Planner's, §2.2.6);
- answer the AC'97 policy question itself (it decides whether accepted work survives, and evidence admissibility is the Advisor's, §2.3).

**State at the failure — nothing was lost and nothing is mid-flight:**

- Game `45c9cee`, tree clean. Toolkit `0d7929c`, tree clean, `upstream/main` still `766ecef`.
- `A4s-r2` is committed (`DBF114BE…`) but **not** frozen or promoted; `CURRENT PACKET` is `none`.
- No merge, build, run, push or fetch was attempted.

**To resume:** re-probe the route (`subagent` with `provider: claude`, `model:
claude-opus-5-5`); when it answers, run the three queued jobs in this order — the Advisor's
AC'97 ruling (it gates the merge's hunk resolution), the `A4s-r2` re-review, then the
`A4b1`/`A4b2` r2 revision. If the outage outlasts the owner's patience, the only legitimate
alternatives are an owner decision to change the §1 roster or to wait — **not** a silent
substitution.
