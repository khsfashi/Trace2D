# Benchmark B0 frozen Codex cohort

Status: **owner-local qualification required before scored eligibility**.

This document freezes the first real coding-Agent candidate for #102. It does not make B0 scored-eligible by itself and contains no benchmark result.

## Frozen Agent/model

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- model family: `gpt-5.5`,
- exact requested model snapshot: `gpt-5.5-2026-04-23`,
- reasoning effort: `high`,
- approval policy: `never`,
- session persistence: `ephemeral`,
- web search: disabled,
- task budget: exactly the B0 task budget,
- human interventions: zero.

Primary references:

- <https://developers.openai.com/api/docs/models/gpt-5.5>
- <https://developers.openai.com/codex/non-interactive-mode>
- <https://developers.openai.com/codex/permissions>
- <https://developers.openai.com/codex/extend/mcp>

Why this model is frozen instead of using a moving Codex-default alias:

- B0 requires an exact model revision/snapshot rather than a product default that may move over time,
- GPT-5.5 documents the dated `gpt-5.5-2026-04-23` snapshot and supports `high` reasoning,
- the same exact profile is used in all three engine lanes,
- this choice is made before any scored three-lane outcome exists.

A successful local probe must still prove that the owner's ChatGPT-managed Codex entitlement accepts that exact snapshot. If it does not, B0 stays blocked and the profile must be revised before any scored result exists; the failure must not be relabeled as an engine loss.

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

No `sandbox_mode`/`--sandbox` is combined with this profile. Codex documentation explicitly treats the new permission-profile system and legacy sandbox settings as mutually exclusive configuration paths.

The actual candidate workspace is created **outside the Trace2D repository**. Before any calibration trial, an empirical canary probe must prove both directions:

1. Codex can read and write a random value inside its candidate workspace.
2. Codex is instructed to attempt an exact shell read of a random canary placed beside the held-out verifier, and that read is denied without leaking the canary value.

The probe preserves the model's JSONL tool trajectory so the denial is evidence rather than an assumption. A failed or ambiguous isolation probe blocks all candidate trials.

## Lane exposure

The model prompt and profile are identical across lanes. Only the normal environment adapter changes.

### `godot.generic`

- pinned official Godot 4.7.1 executable available through the shell/PATH,
- ordinary candidate project files,
- no Godot-specific MCP server.

### `godot.agent`

- the same pinned Godot executable,
- selected qualified `@satelliteoflove/godot-mcp@4.1.0`,
- addon/plugin installation is environment setup, not task solution logic,
- Codex receives the public MCP tools only.

The addon/plugin is removed from the preserved candidate artifact before the independent verifier runs so environment scaffolding does not become part of the scored authored result.

### `trace2d.agent`

- frozen public `trace2d` CLI executable,
- frozen public `trace2d_mcp` stdio server,
- ordinary Trace2D candidate scene file,
- no benchmark-only engine API.

For B0's first authoring task, the Agent may use either the public CLI or MCP surface. The independent verifier remains outside both surfaces.

## Owner-local calibration

ChatGPT-managed Codex credentials are intentionally not placed in GitHub Actions for this public repository. The owner-local calibration script uses the already authenticated local Codex CLI and never commits the credential.

From a current PR #118 checkout on Windows:

```powershell
./scripts/run_benchmark_b0_codex_calibration.ps1
```

The script performs, in order:

1. exact Codex `0.144.6` and login check,
2. pinned vcpkg setup, Trace2D configure/build/full CTest,
3. official Godot 4.7.1 Windows download + SHA-512 verification,
4. official Node 22.18.0 download + SHA-256 verification,
5. exact Godot MCP 4.1.0 npm install/integrity recording,
6. copy of only the public Trace2D executable surfaces outside the source repository,
7. real Codex filesystem-isolation canary probe,
8. exactly one **unscored** trial in each of the three B0 lanes,
9. raw hash-chained record/report preservation,
10. credential scrubbing and evidence ZIP creation.

The script deliberately does **not** promote `suite.json` to `eligible` and does not run `--scored`.

The generated ZIP is reviewed first. Only if the real isolation/model/environment facts are valid may the suite/task be promoted to scored eligibility. A subsequent repeated matched cohort then uses the same committed profile hash and predefined sample/retry policy.

## Credential handling

`~/.codex/auth.json` is treated as a password-equivalent credential:

- it is copied only into per-run isolated `CODEX_HOME` directories so user config cannot silently change benchmark settings,
- it is outside the model's permitted workspace,
- the calibration script recursively removes every copied `auth.json` before producing its evidence ZIP,
- no credential or API key is committed to the repository or GitHub Actions.

If the local Codex installation stores credentials only in an OS keyring and no file-backed `auth.json` exists, the script stops before model execution and asks the owner to explicitly re-login with file-backed CLI credential storage. This is an infrastructure/setup outcome, not an Agent task failure.

## Promotion rule

Do not change `qualification_required` / `qualification_candidate` to `eligible` until all of the following are observed from a real owner-local run:

- exact Codex version accepted,
- exact model snapshot accepted,
- workspace write probe passed,
- held-out canary read was actually attempted and denied,
- canary content did not appear in model-visible output,
- all three unscored lane calibrations produced parseable Agent results,
- profile hashes match across the three records,
- provider-reported usage is present,
- no human intervention occurred,
- independent verifiers completed.

After promotion, all repeated scored attempts—including failures and infrastructure outcomes—remain part of the cohort evidence.
