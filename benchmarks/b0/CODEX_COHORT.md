# Benchmark B0 frozen Codex cohort

Status: **owner-local qualification required before scored eligibility**.

This document freezes the first real coding-Agent candidate for #102. It does not make B0 scored-eligible by itself and contains no comparative benchmark result.

## Frozen Agent/model selection

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- provider-selectable model identifier: `gpt-5.6-sol`,
- provider revision policy: `chatgpt_managed_identifier_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- session persistence: `ephemeral`,
- web search: disabled,
- task budget: exactly the B0 task budget,
- human interventions: zero.

The important distinction is that this cohort uses **ChatGPT sign-in**, not an owner-supplied OpenAI API key. The ChatGPT Codex surface exposes provider-selectable model identifiers, but does not expose the dated API snapshot used by the first attempted profile. B0 therefore records the exact selectable identifier and auth mode without pretending that a hidden dated provider revision is known.

This still satisfies #102's required model/Agent/configuration identity: the same Codex version, model selection string, reasoning setting, permission profile, budget, prompt and lane-independent Agent wrapper are held constant across the matched cohort. The limitation is disclosed as part of the run identity.

Primary references:

- <https://developers.openai.com/codex/models>
- <https://developers.openai.com/codex/non-interactive-mode>
- <https://developers.openai.com/codex/permissions>
- <https://developers.openai.com/codex/extend/mcp>

## Rejected first model profile

The first owner-local attempt requested API snapshot `gpt-5.5-2026-04-23` through ChatGPT-managed Codex. The provider rejected it with HTTP 400 before any shell/tool action:

> The `gpt-5.5-2026-04-23` model is not supported when using Codex with a ChatGPT account.

That run is preserved as [`qualification/codex-chatgpt-model-attempt-2026-08-11.json`](qualification/codex-chatgpt-model-attempt-2026-08-11.json).

It is classified as `infrastructure_model_availability`, not an engine loss:

- zero matched lane trials started,
- zero provider tokens were reported,
- the isolation canary was never attempted,
- no scored result existed when the profile was corrected.

The dated API snapshot path is therefore explicitly **REJECTED** for this ChatGPT-managed cohort rather than silently retried with a different model.

## Why `gpt-5.6-sol`

At the profile correction date, OpenAI's Codex model documentation lists GPT-5.6 Sol as the flagship recommended Codex model and documents `codex -m gpt-5.6-sol` / `codex exec -m gpt-5.6` as supported selection paths. The benchmark freezes `gpt-5.6-sol` explicitly rather than relying on whatever model a future Codex default may choose.

Changing this model after a scored result exists is forbidden for the B0 cohort. If provider availability changes before the first scored attempt, that change must be recorded as infrastructure evidence and a new cohort/profile identity must be created before scoring.

## Isolation boundary

Legacy `workspace-write` alone is insufficient for B0 because a benchmark Agent must not be able to read the held-out verifier or source repository merely because those files are outside the writable directory.

Codex `0.144.6` supports named permission profiles on native Windows. The wrapper generates a fresh isolated `CODEX_HOME` for every run with a custom profile equivalent to:

```toml
default_permissions = "trace2d_b0_isolated"
approval_policy = "never"
web_search = "disabled"

[permissions.trace2d_b0_isolated.filesystem]
":minimal" = "read"
# exact frozen tool/runtime directories are added read-only here

[permissions.trace2d_b0_isolated.filesystem.":workspace_roots"]
"." = "write"

[permissions.trace2d_b0_isolated.network]
enabled = false
```

The actual candidate workspace is outside the Trace2D repository. Before any calibration task, a random canary probe must prove both directions:

1. Codex can read and write a random value inside its candidate workspace.
2. Codex actually attempts an exact shell read of a random canary placed beside the held-out verifier, and that read is denied without leaking the canary value.

The probe preserves the public JSONL tool trajectory. A failed or ambiguous isolation probe blocks all candidate trials.

## Lane exposure

The task prompt, Codex CLI, model selection, reasoning level, budget and isolation policy are identical across lanes. Only the normal environment adapter changes.

### `godot.generic`

- pinned official Godot 4.7.1 executable through the shell/PATH,
- ordinary candidate project files,
- no Godot-specific MCP server.

### `godot.agent`

- the same pinned Godot executable,
- selected qualified `@satelliteoflove/godot-mcp@4.1.0`,
- addon/plugin installation is environment setup, not task solution logic,
- Codex receives only the bridge's public MCP tools.

The injected addon is removed from the preserved candidate artifact before independent verification.

### `trace2d.agent`

- frozen public `trace2d` CLI executable,
- frozen public `trace2d_mcp` stdio server,
- ordinary Trace2D candidate scene file,
- no benchmark-only engine API.

## Owner-local qualification

The first calibration already completed the expensive deterministic toolchain setup but stopped at provider model availability before isolation. After updating the PR checkout, reuse that qualified toolchain with:

```powershell
./scripts/run_benchmark_b0_codex_chatgpt_calibration.ps1
```

The recovery calibration:

1. verifies exact Codex `0.144.6` and local ChatGPT login,
2. finds the latest prior calibration toolchain evidence,
3. verifies its frozen Trace2D/Godot/Node/Godot-MCP identities and cached archive hashes,
4. uses `gpt-5.6-sol` with `high` reasoning through the ChatGPT-specific wrapper,
5. runs the real filesystem-isolation canary,
6. preserves exactly one **unscored** attempt in each B0 lane,
7. keeps failed Agent attempts rather than stopping the matched cohort,
8. emits raw hash-chained records and aggregate report,
9. removes credentials/transient Codex homes and creates a scrubbed evidence ZIP.

The script does **not** promote `suite.json` to `eligible` and does not invoke `--scored`.

## Credential/evidence packaging

`~/.codex/auth.json` is password-equivalent and is never committed or uploaded. The wrapper copies it only into an isolated per-run `CODEX_HOME`; the evidence packager excludes transient Codex/plugin/cache directories and refuses to package an unexpected `auth.json`.

The first owner's ZIP also exposed a PowerShell `Compress-Archive` race against disappearing Codex plugin-cache paths. That packaging defect was fixed by replacing broad recursive archiving with an allowlisted scrubbed evidence packager. The failure did not change any benchmark outcome.

## Promotion rule

Do not change `qualification_required` / `qualification_candidate` to `eligible` until a real owner-local ChatGPT Codex run proves all of the following:

- exact Codex `0.144.6` accepted,
- `gpt-5.6-sol` accepted through ChatGPT sign-in,
- model/auth/provider-revision policy recorded,
- workspace write probe passed,
- held-out canary read was actually attempted and denied,
- canary content did not appear in model-visible output,
- all three unscored lane calibrations produced parseable Agent records,
- profile hashes match across the three records,
- provider-reported usage is present when exposed by Codex JSONL,
- no human intervention occurred,
- independent verifiers completed.

After promotion, every repeated scored attempt—including failures and predefined infrastructure outcomes—remains part of the cohort evidence. No best-of-N selection is allowed.
