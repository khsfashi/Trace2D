# Benchmark B0 frozen Codex cohort

Status: **owner-local qualification required before scored eligibility**.

This document freezes the real coding-Agent candidate for #102. It does not make B0 scored-eligible and contains no comparative benchmark result.

## Frozen Agent/model selection

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- ChatGPT-managed model selection: `gpt-5.6-sol`,
- provider revision policy: `chatgpt_managed_identifier_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- session persistence: `ephemeral`,
- web search: disabled,
- task budget: exactly the B0 task budget,
- human interventions: zero.

This cohort uses **ChatGPT sign-in**, not an owner-supplied OpenAI API key. The benchmark therefore freezes the exact provider-selectable Codex model identifier and auth mode that are actually exposed by that surface. It does not claim knowledge of a hidden dated provider snapshot.

The same Codex version, model selection string, reasoning setting, permission profile, prompt, budget and wrapper are held constant across all matched lanes.

Primary references:

- <https://developers.openai.com/codex/models>
- <https://developers.openai.com/codex/non-interactive-mode>
- <https://developers.openai.com/codex/permissions>
- <https://developers.openai.com/codex/extend/mcp>

## Rejected first model profile

The first owner-local attempt requested API snapshot `gpt-5.5-2026-04-23`. ChatGPT-managed Codex rejected it with HTTP 400 before any shell/tool action because that dated API snapshot is not supported with ChatGPT sign-in.

The sanitized attempt is preserved as [`qualification/codex-chatgpt-model-attempt-2026-08-11.json`](qualification/codex-chatgpt-model-attempt-2026-08-11.json) and classified as `infrastructure_model_availability`:

- zero matched lane trials started,
- zero provider tokens,
- isolation canary not attempted,
- no scored outcome existed.

The dated API snapshot path is explicitly rejected for this cohort rather than silently rewritten as an engine loss. The profile was corrected before scoring to the officially documented Codex model identifier `gpt-5.6-sol`.

## Isolation boundary

Candidate workspaces live outside the Trace2D repository. Codex `0.144.6` receives a fresh native-Windows named permission profile per run:

```toml
default_permissions = "trace2d_b0_isolated"
approval_policy = "never"
web_search = "disabled"

[permissions.trace2d_b0_isolated.filesystem]
":minimal" = "read"
# exact frozen tool/runtime roots are added read-only

[permissions.trace2d_b0_isolated.filesystem.":workspace_roots"]
"." = "write"

[permissions.trace2d_b0_isolated.network]
enabled = false
```

Before any task trial, a random canary probe must prove:

1. normal read/write works inside the candidate workspace,
2. Codex actually attempts an exact shell read of a random canary beside the held-out verifier,
3. the read is denied,
4. the canary value does not leak into model-visible output.

The public Codex JSONL trajectory is preserved as evidence. Failed or ambiguous isolation blocks all candidate trials.

## Lane exposure

The task prompt, Codex profile and budget are identical across lanes. Only the normal environment adapter changes.

- `godot.generic`: pinned Godot 4.7.1 through ordinary shell/project files, no Godot MCP.
- `godot.agent`: same Godot plus qualified `@satelliteoflove/godot-mcp@4.1.0`; injected addon is environment scaffolding and removed before independent verification.
- `trace2d.agent`: frozen public `trace2d` CLI + `trace2d_mcp`, ordinary Trace2D scene file, no benchmark-only engine API.

## Owner-local qualification

The first attempt already completed the expensive deterministic toolchain setup before provider model availability stopped the isolation probe. After updating the PR #118 checkout, reuse that preserved toolchain with:

```powershell
python .\scripts\run_benchmark_b0_codex_chatgpt_calibration.py
```

The recovery calibration:

1. verifies exact Codex `0.144.6` and file-backed ChatGPT login,
2. finds the latest prior `codex-calibration-*` toolchain evidence,
3. verifies its frozen Trace2D/Godot/Node/Godot-MCP identities and cached archive hashes,
4. uses `gpt-5.6-sol` with `high` reasoning,
5. runs the real filesystem-isolation canary,
6. preserves exactly one **unscored** attempt in each B0 lane,
7. continues after Agent/verifier failures so losses remain evidence,
8. writes hash-chained raw records and an aggregate report,
9. scrubs credentials/transient Codex homes and creates a reviewable ZIP.

The recovery path deliberately does **not** set the suite/task to `eligible` and never invokes `--scored`.

## Credential and packaging contract

`~/.codex/auth.json` is password-equivalent. It is copied only into isolated per-run `CODEX_HOME` directories and is never committed or uploaded.

The first owner run also exposed a PowerShell `Compress-Archive` race against disappearing Codex plugin-cache paths. Evidence packaging now uses `package_benchmark_b0_evidence.py`, which excludes transient Codex/plugin/cache trees and refuses unexpected `auth.json` files. That packaging defect changed no benchmark outcome.

## Promotion rule

Do not change `qualification_required` / `qualification_candidate` to `eligible` until owner-local evidence proves:

- Codex `0.144.6` accepted,
- `gpt-5.6-sol` accepted through ChatGPT sign-in,
- model/auth/provider-revision policy recorded,
- workspace write probe passed,
- held-out canary read was actually attempted and denied,
- canary content did not leak,
- all three unscored lanes produced parseable Agent records,
- one profile hash is shared across the three records,
- provider usage/trajectory is preserved where Codex exposes it,
- no human intervention occurred,
- independent verifiers completed.

After promotion, every repeated scored attempt—including losses and predefined infrastructure outcomes—remains part of the cohort evidence. No best-of-N selection is allowed.
