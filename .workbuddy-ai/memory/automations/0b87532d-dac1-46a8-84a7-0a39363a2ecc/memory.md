# Automation memory — advisor cross-session persistence probe

## 2026-09-21 22:05 PDT — run 1

- Executed the one-off probe exactly as specified: Agent tool, `general-purpose`, `kimi-k3`,
  `name=advisor-xsession-probe`, `resume="agent-d3294b58"`.
- Resume call **succeeded**; agent id echoed back unchanged.
- Reply reproduced all three ground-truth items (hypothesis + `g_ecx`/`g_esp` experiment;
  18,058 / 3,005 / 293; wrong value `0x38` at `0x22FCE0`). No `NO CONTEXT`.
- **Verdict: PERSISTENT ACROSS SESSIONS.**
- Supporting mechanical evidence: resume wrote a new transcript under the current session dir whose
  first record's `parentId` is the final record of the *old* session's
  `subagents/agent-d3294b58.jsonl` (session `3659ed94-…` vs current `a71e13b9-…`).
- Raw evidence appended to `~/.workbuddy-ai/advisor-persistence-test.md`.
- User-level `~/.workbuddy-ai/MEMORY.md` corrected: the old "assume resume does not cross sessions"
  note is now falsified.
