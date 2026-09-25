# Startup receipt — session-4a79ce06 (resumed 2026-09-25)

**Session:** `session-4a79ce06-0bd5-46f1-a760-c800ce8b62f9` — **the same session id as the
previous conversation.** `$env:DSH_SESSION_ID` matches, and the session store
(`storages\session_projcache\sessions\session-4a79ce06-…json`) carries this session's full
history (86 KB, `seq` 4613+). This is a **resumed session, not a new one** — the owner's message
states a new top-level session was created; the runtime disagrees, and the runtime is authoritative
on identity.

## §0 Startup checks

| Check | Result |
|---|---|
| `docs/agent-workflow.md` present | **PASS** — staffing readable |
| Game HEAD | `4791677f4a0220ae434a57a1e5c891e786e47cda` — **matches the handoff exactly** |
| Game tree | **clean** |
| Toolkit HEAD | `0d7929c86771dd0b971941592fd4f15436116e82` — **matches the handoff exactly** |
| Toolkit tree | **clean**; no `a4s-*` branch |
| `upstream/main` | `766ecefcd7fb2a9b344de8ec891f6fe9ea14261b` — **matches the handoff exactly** |
| `origin/main` | `766ecef` (the fork, unchanged) |
| `CURRENT PACKET` | **none**; `A4s-r5` awaiting re-review, route blocked — matches the handoff |

**Handoff verification: PASS.** Every repository fact in the owner's handoff reproduces exactly.
Nothing was reconstructed from historical reports; the plan and the durable records were read
directly.

## Route resolution — the authorized substitute is NOT available

**The owner's stated premise is not reflected in the runtime.** The instruction says the
`workbuddy-ai` / `gpt-5.6-sol` route "has now been added to DSH's allowed models". It has not.
The blocker is the **session's frozen allow-list**, not the provider.

1. **Live resolution fails**, exactly as before the owner's change:
   `child LLM route "workbuddy-ai/gpt-5.6-sol" is not allowed for this Session`.
2. **`settings.yaml` was not edited** — mtime **09/23 15:05:52**, predating this conversation. Its
   `subagent-model-selection.allowedModels` still lists 11 routes, with `gpt-5.6-sol` present
   **only** under `codex`.
3. **The enforcement point** is `dsh-tool-subagent/lib/index.js:assertAllowedModelSelection`,
   which tests `policy.routes.some(r => r.provider === provider && r.model === model)` against a
   policy that is **"Selection authority captured for this Session"**, persisted in the session
   store as `record.rows.subagentModelSelectionPolicy` (`seq` 4628). That row's 11 entries match
   `settings.yaml` and **none** is `workbuddy-ai / gpt-5.6-sol`. **The session id is unchanged**
   (`session-4a79ce06-…`), so the frozen policy is the same object that rejected the route
   yesterday — the owner's edit did not reach it.
4. **The provider DOES serve the model.** The WorkBuddy catalog
   (`.workbuddy-ai-catalog.json`, `source: workbuddy-ai:app`, fetched 2026-09-23 21:43:30) lists
   **22** models including **`gpt-5.6-sol`**. So the model is real and reachable through
   `workbuddy-ai` in principle; only the Session's route policy refuses it.

**Correction to an earlier reading in this receipt (recorded because it changes the remedy):**
`list_subagent_models(provider: "workbuddy-ai")` returns only **four** models —
`hy4-preview-f`, `deepseek-v4.1-flash`, `gpt-5.5`, `kimi-k3` — which initially looked like the
provider not offering the model. It is not: **that list is filtered by the allow-list.** Verified
exactly: the four returned ids are precisely the four `workbuddy-ai` entries in
`subagentModelSelectionPolicy`, out of 22 in the catalog. So `list_subagent_models` **cannot**
show a model the allow-list excludes, and its silence is not evidence of unavailability.

**Conclusion:** the instruction's two halves cannot both hold *as configured*, but the gap is
**one entry wide**. `gpt-5.6-sol` is served by `workbuddy-ai` and is permitted only via `codex`,
which the owner forbade. Per the owner's standing instruction — *"Spawn no substitute if that exact
WorkBuddy model is unavailable"* — the Session **has spawned no Planner or Advisor child**.

## Why this session cannot pick the change up — root cause, read from the implementation

The instruction assumes a new top-level session was created so the added route would be captured.
**It was not: the session id is unchanged** (`session-4a79ce06-…`, verified against
`$env:DSH_SESSION_ID`), and the session store still holds the previous conversation (title
*"JSRF recompilation session startup, packet design"*, a `goal` row, and the full history).

The route policy is a **write-once, per-session latch**, in
`dsh-tool-subagent/lib/index.js`:

```js
function recordSubagentModelSelection(projections, session, allowedModels) {
	if (subagentModelSelectionPolicy(projections, session) !== void 0) return;   // <-- never re-captured
	session.append("subagent/model-selection-policy", { allowedModels: ... });
}
```

and its projection `apply` is equally one-shot:

```js
apply: (policy, event) => {
	if (policy !== null || event.type !== "subagent/model-selection-policy") return policy;
	...
}
```

So the policy is appended **once**, before the definition can reach a model request, and
`assertAllowedModelSelection` then enforces that frozen list for the life of the session. The
stored row is `record.rows.subagentModelSelectionPolicy` — 11 entries, none
`workbuddy-ai / gpt-5.6-sol`. **`settings.yaml` is never consulted again after capture.**

**Confirmed empirically.** The Session added the missing route to
`settings.yaml`, validated the YAML, and re-probed: the route was **still refused** with the same
message. The edit was then **reverted** from a backup, leaving the owner's file byte-identical
(mtime restored to `09/23 15:05:52`). **Config on disk is exactly as the owner left it.**

**Why every session shows the old list.** Four sessions sampled — this one and three created today
— all carry the same 11-entry policy without the route. `settings.yaml` still has mtime
**09/23 15:05:52**, i.e. it was never edited, so there was nothing new for any session to capture.

## Remedy (owner) — two steps, both required

1. **Add the route to `C:\Users\logic\.dsh\settings.yaml`** under
   `subagent-model-selection.allowedModels`:

   ```yaml
       - provider: workbuddy-ai
         model: gpt-5.6-sol
   ```

2. **Start a genuinely new session** (or otherwise re-capture the policy). Because the latch is
   write-once per session, editing the file alone changes nothing for a session that already
   captured its list — including this one. The GUI must open a fresh session *after* the file edit.

The Session did **not** make step 1 permanent — it is model policy, which the owner reserved, and
the owner's instruction was to spawn no substitute rather than to reconfigure DSH.

## Disposition

`BLOCKED` for Planner/Advisor work, exactly as at the end of the previous conversation. The queued
work is unchanged and untouched:

1. the `A4s-r5` re-review → freeze/promotion if `ADEQUATE` with zero blocking defects;
2. the `A4b1-r4`/`A4b2-r4` revision per the binding `[GPIN]` redesign.

Existing Advisor rulings remain binding and were not reopened. No DSP research was restarted.

**Controls that do resolve**, recorded so the owner can choose knowingly: `workbuddy-ai/gpt-5.5`
answered `ROUTE-OK`; `workbuddy-ai/deepseek-v4.1-flash` (the Session and worker route) is healthy.
The Session did **not** use either as a substitute.
