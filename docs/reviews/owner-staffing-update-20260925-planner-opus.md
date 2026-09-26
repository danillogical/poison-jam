# Owner staffing update — Planner changed to Claude Opus 5.5 @ medium (2026-09-25)

**Authority:** direct owner instruction, an explicit **§3.4 owner decision** on staffing (§1 roster).
**Session:** `session-58e86358-bf35-4317-82a7-7f5fd0c28dd3`.
**Status:** applied. No session restart; `A4s` not reopened; the authorized `A4b` work continues from
where the session is.

## Required record (as the owner specified)

```text
WORKFLOW_RELOADED: YES
WORKFLOW_SHA256:   973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748
PLANNER_ROUTE:     claude/claude-opus-5-5
PLANNER_EFFORT:    medium
ADVISOR_CHILD:     5c555969-dea9-4b47-be05-62aa0835cde2
ADVISOR_ROUTE:     claude/claude-opus-5-5 @ high
```

## The workflow file changed, and was re-read from disk

`docs/agent-workflow.md` was **re-read from disk**, not recalled from the session-start copy. The hash
proves it changed:

| | SHA-256 |
|---|---|
| recorded at startup | `CBEF90414E78D14BBF66FFA25D9BC7854D312C84AED9605246F305BE7AB8E6AB` |
| **on disk now** | **`973CDDEF0C5206349E66DE15A6C0E830F85ACAD488231D3867F25B583E911748`** |
| match | **NO — the file was replaced** |

46184 bytes, 833 lines, modified `2026-09-25 23:17:46`, still uncommitted in the game repo (the owner's
edit; **not** committed or normalised).

### What changed beyond the Planner row (found by diffing, not assumed)

The diff is **77 insertions / 21 deletions**. The §1 roster changed in **three** rows, not one:

| Row | Before | **Now** |
|---|---|---|
| **Planner** | Claude Opus 5.5 @ `high` | **Claude Opus 5.5 @ `medium`** |
| **Acceptance reviewer (stage 1)** | `workbuddy-ai/deepseek-v4.1-flash` @ `max` | **`workbuddy-ai/hy4-preview-f` @ `high`** |
| **Acceptance reviewer (stage 2)** | Claude Opus 5.5 @ `medium` | **`workbuddy-ai/deepseek-v4.1-flash` @ `max`** |

**The two reviewer rows have swapped relative to the previous revision** — the acceptance pair is now
exactly what the session had already been using operationally (Hy4 stage 1, DeepSeek stage 2), so this
revision **aligns the document with the practice**, and the new text says so explicitly
(*"Hy4 is the first-stage adversarial gatekeeper. DeepSeek is the second-stage reproducer…"*).

Other substantive additions now in force, all of which the session was already following:

- **§5.1.4** now formally mandates the **early Advisor shape preflight** (first viable sketch, ≤20 tool
  calls, four shape questions, `PROCEED`/`REDIRECT`/`DISCOVERY_FIRST`) — this is what was run for
  `A4s-r6` and `A4b1-r4`, so it is now policy rather than session practice.
- **§2.3 Planner** gains the **bounded-initiative** rules (adjacent work stays follow-up leads unless
  omitting it could cause a false PASS/FAIL, a wrong implementation, a wrong evidence binding, or unsafe
  execution; no architecture improvement merely because a better design is visible).
- **§5.1.5(3)** now states that in DSH the fresh adequacy reviewer is *"a fresh Claude Opus 5.5 Medium
  Planner under the §1 roster; the Opus shape preflight is not the adequacy review and does not replace
  it."*
- **§5.1.4** adds that after `PROCEED` the Advisor does **not** perform a full second review of every
  completed packet.

## Planner route resolved LIVE, exactly as §1 requires

`list_subagent_models(provider="claude")` → **exactly one** canonical match:

```
claude/claude-opus-5-5 — Claude Opus 5.5
```

`list_subagent_models(provider="claude", model="claude-opus-5-5")` confirms the **required effort** is
advertised:

```
Reasoning efforts: low, medium, high, xhigh, max
```

**So the row is satisfied: one canonical route, `medium` supported.** Recorded provider/model:
**`claude` / `claude-opus-5-5`**, effort **`medium`**. **No fallback to Kimi or any other model.**

**Worker route re-verified too** (unchanged by this decision, but the roster lists it):
`workbuddy-ai/deepseek-v4.1-flash` @ `max` — one canonical match, `max` advertised.

## In-flight Planner work: NONE to abandon

The owner's instruction — *"If a Kimi Planner child currently has unfinished/in-flight Planner work,
abandon that in-flight Planner result and reissue the same frozen brief to a fresh Claude Opus 5.5 Medium
Planner"* — was checked against the live child registry and the records:

| Child | Role | State | Disposition |
|---|---|---|---|
| `3fbfef10` | Kimi Planner, `A4s-r6` | **finished** | durable — `A4s-r6` accepted and pushed |
| `7332c25d` | Kimi Planner, `A4b1` | **finished** | durable — produced the `A4b1-r4` packet |
| `95429607` | fresh Kimi adequacy review, `A4s-r6` | **finished** | durable — the `ADEQUATE` verdict |
| `5db5fd71` | fresh Kimi adequacy review, `A4b1-r4` | **finished** | durable — the `ADEQUATE` verdict |
| **`f9d0e439`** | **Worker**, "Implement A4b1 port steps 2-6" | **running** | **NOT a Planner** — a contract role whose route is unchanged, so it is unaffected and continues |

**No Kimi Planner child has unfinished or in-flight Planner work**, so **nothing is abandoned and no
brief is reissued.** The only running child is a **Worker**, which the new roster keeps at
`workbuddy-ai/deepseek-v4.1-flash` @ `max`.

## Durable work preserved, with original model identity as provenance

Per the owner — *"Do not retroactively invalidate durable Planner verdicts, packets, rulings, or accepted
work that were completed and recorded before this owner change"* — the following stand unchanged, and
their records already name the authoring route:

| Artifact | Authoring route (preserved as provenance) |
|---|---|
| `A4s-r6` packet + its `ADEQUATE` verdict + its acceptance and push | Kimi K3 Planner (`workbuddy-ai/kimi-k3`), effort omitted |
| **`A4b1-r4` packet** (`6DD62A57…`, 445 lines) | Kimi K3 Planner (`workbuddy-ai/kimi-k3`) |
| `A4b1-r4` `ADEQUATE` verdict | fresh Kimi K3 Planner (`workbuddy-ai/kimi-k3`) |
| all Advisor rulings (Q-A/Q-B/Q-C, `A4b1-r4` parts 1–4, hunk rulings) | `claude/claude-opus-5-5` @ `high` |
| `A4s-r6` stage-1 acceptance `ACCEPT` | `workbuddy-ai/hy4-preview-f` @ `high` |

**`A4b1-r4` remains the promoted, authorized packet and execution continues.** It was `ADEQUATE` under
the roster in force when it was reviewed, and the owner explicitly forbids retroactive invalidation.

## Planner / Advisor separation — now same model family, different roles

The owner's constraint is recorded and will be enforced:

- **Opus 5.5 Medium child = Planner authority only.**
- **The existing Opus 5.5 High persistent child (`5c555969-…`) = Advisor authority only.**
- **Never reuse the Advisor child as the Planner, or the Planner child as the Advisor, for the same
  decision.**
- **The existing startup-probed Opus 5.5 High Advisor child is kept — not replaced or reprobed** merely
  because the Planner model changed.
- **The mandatory Advisor shape preflight remains in force** for new change packets and material
  redesigns. It is a **bounded shape/policy review, not** the packet's formal adequacy review.
- **For a change packet materially authored by an Opus 5.5 Medium Planner, the binding adequacy review
  goes to a fresh Opus 5.5 Medium Planner child** — not the authoring child, and not the Opus High
  Advisor. (This matches the updated §5.1.5(3).)

## What this changes going forward

- **No new Planner work goes to Kimi K3.** Any future Planner dispatch — including the `A4b2-r4` revision
  the `A4b1-r4` packet's boundary note calls for — goes to a **fresh `claude/claude-opus-5-5` @ `medium`**
  child.
- **The in-flight `A4b1-r4` execution is unaffected**, because execution is Session-owned (contract role)
  with Worker assistance, and neither route changed.
- The Session route is unchanged: `workbuddy-ai/deepseek-v4.1-flash` @ `max`.
