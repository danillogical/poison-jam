# Route failure: the owner-authorized WorkBuddy substitute is not resolvable (2026-09-24)

**Owner override:** use `workbuddy-ai/GPT-5.6 sol` @ `high` for Planner and Persistent Advisor
while Claude Opus 5.5 is rate-limited; **"Do NOT use a Codex-hosted GPT route"**; verify by live
DSH route resolution before spawning; **"If GPT-5.6 Sol itself is unavailable, STOP and report the
route failure. Do not choose another substitute."**

**Result: the specified route is not resolvable for this Session. STOPPED, no substitute chosen.**

## Live probes (all `run_in_background: false`, reasoning effort `low`)

| Attempt | Result |
|---|---|
| `workbuddy-ai` / `GPT-5.6 sol` (owner's exact spelling) | **`child LLM route "workbuddy-ai/GPT-5.6 sol" is not allowed for this Session`** |
| `workbuddy-ai` / `gpt-5.6-sol` (normalized) | **`… is not allowed for this Session`** |
| `workbuddy-ai` / `gpt-5.6sol` | **`… is not allowed for this Session`** |
| `workbuddy-ai` / `gpt-5.5` (control) | **`ROUTE-OK`** |
| `claude` / `claude-opus-5-5` (control) | failed — the outage continues |

## Root cause — a session config gap, not a provider outage

The WorkBuddy catalog **does** expose the model: id `gpt-5.6-sol`, name `GPT-5.6-Sol`
(`C:\Users\logic\.dsh\.workbuddy-ai-catalog.json`). The provider itself is healthy — the
`gpt-5.5` control resolved and answered.

The blocker is the Session's subagent allow-list in `C:\Users\logic\.dsh\settings.yaml`
(`subagent-model-selection.allowedModels`):

```text
codex          / gpt-6-astra
codex          / gpt-6-sol
codex          / gpt-6-luna
codex          / gpt-5.6-sol        <-- the only permitted home for this model
codex          / gpt-5.6-terra
codex          / gpt-5.6-luna
workbuddy-ai   / deepseek-v4.1-flash
workbuddy-ai   / gpt-5.5
workbuddy-ai   / hy4-preview-f
workbuddy-ai   / kimi-k3
claude         / claude-opus-5-5
```

`gpt-5.6-sol` is permitted **only under the `codex` provider** — and the owner explicitly forbade
using a Codex-hosted GPT route. There is **no** `workbuddy-ai / gpt-5.6-sol` entry, which is
exactly why every spelling returns "not allowed for this Session" rather than a provider error.

So the two halves of the instruction cannot both be satisfied with the current configuration:
the authorized provider (`workbuddy-ai`) is not permitted to serve `gpt-5.6-sol`, and the
provider that is permitted to serve it (`codex`) is explicitly excluded.

## Why the Session did not resolve this itself

Editing `allowedModels` is a **model-policy** change, and the owner reserved staffing/model policy
beyond the override. The owner also gave an explicit instruction for precisely this failure mode
("STOP and report the route failure. Do not choose another substitute."), and the Session did not
substitute `workbuddy-ai/gpt-5.5` or any other available WorkBuddy model despite it resolving
cleanly.

## Remedy (one line, owner's call)

Add to `subagent-model-selection.allowedModels` in `C:\Users\logic\.dsh\settings.yaml`:

```yaml
    - provider: workbuddy-ai
      model: gpt-5.6-sol
```

That is the only change needed to make the authorized route resolvable. **The Session has not made
this edit.** Alternatives, if the owner prefers: authorize `workbuddy-ai/gpt-5.5` @ `high` (it
resolves today), or wait for Claude to recover.

## State at the stop — nothing lost, nothing mid-flight

- Game `5e826a6`, toolkit `0d7929c`; both trees clean; `upstream/main` = `766ecef`.
- No merge, build, run, push or fetch attempted. No child was spawned for the queued work.
- **Queued, unchanged:** (1) the `A4s-r5` re-review → freeze/promotion if `ADEQUATE`;
  (2) the `A4b1-r4`/`A4b2-r4` revision per the binding `[GPIN]` redesign ruling.
- Existing binding rulings are untouched and are **not** reopened by a provider change
  (`docs/reviews/a4b-gpin-accounting-ruling.md`, `docs/reviews/a4s-ac97-hunk-ruling.md`).
