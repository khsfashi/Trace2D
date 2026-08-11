# Benchmark B0 frozen Codex cohort

Status: **owner-local qualification required before scored eligibility**.

This document freezes the real coding-Agent candidate for #102. It does not make B0 scored-eligible and contains no comparative benchmark result.

## Frozen Agent/model selection

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- ChatGPT Codex CLI selector: `gpt-5.5`,
- provider revision policy: `chatgpt_codex_cli_selector_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- session persistence: `ephemeral`,
- web search: disabled,
- task budget: exactly the B0 task budget,
- human interventions: zero.

This cohort uses **ChatGPT sign-in**, not an owner-supplied API key. The benchmark freezes the provider-selectable CLI model identifier exposed by that surface and does not claim knowledge of a hidden dated provider snapshot. `gpt-5.5` was selected before any scored matched-lane outcome existed, after owner-local preflight proved that the still-rolling-out `gpt-5.6` selector was unavailable to this account.

The same Codex version, model selector, reasoning setting, permission profile, prompt, budget and wrapper must remain constant across all matched lanes and repeated scored trials.

Primary references:

- <https://developers.openai.com/codex/models>
- <https://developers.openai.com/codex/non-interactive-mode>
- <https://developers.openai.com/codex/permissions>
- <https://developers.openai.com/codex/extend/mcp>

## Preserved pre-scoring qualification attempts

No attempt below contains a scored engine result. All stopped before the matched three-lane cohort began and remain visible as infrastructure evidence.

### 1. Dated API snapshot rejected

The first owner-local attempt requested `gpt-5.5-2026-04-23`. ChatGPT-managed Codex rejected that dated API snapshot before any shell/tool action.

The sanitized attempt is preserved as [`qualification/codex-chatgpt-model-attempt-2026-08-11.json`](qualification/codex-chatgpt-model-attempt-2026-08-11.json), classified as `infrastructure_model_availability`.

### 2. Guessed `gpt-5.6-sol` identifier lacked reconstructable child evidence

A recovery attempt requested `gpt-5.6-sol`. Its scrubbed ZIP proved toolchain reuse and isolation-workspace initialization, but the child wrapper exited before a normal isolation verdict and the first orchestrator version had not persisted its child stdout/stderr.

The sanitized attempt is preserved as [`qualification/codex-chatgpt-recovery-attempt-2026-08-11.json`](qualification/codex-chatgpt-recovery-attempt-2026-08-11.json), classified as `infrastructure_observability_gap_before_isolation_evidence`.

That observability defect was fixed: model preflight, isolation wrapper, orchestration failures and per-lane harness processes now preserve machine-readable status plus stdout/stderr where applicable.

### 3. Documented `gpt-5.6` selector reached the provider but was unavailable

After Windows `.cmd` positional-prompt quoting was replaced by Codex stdin `-` transport, owner-local preflight reached the real provider with `gpt-5.6`. The provider returned HTTP 400 stating that `gpt-5.6` was not supported for this ChatGPT-signed-in Codex account. No provider tokens were consumed, isolation did not start, and no lane trial started.

The sanitized attempt is preserved as [`qualification/codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json`](qualification/codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json), classified as `infrastructure_model_rollout_unavailable`.

Because there was still no scored result to bias the benchmark, B0 then froze the supported fallback CLI selector `gpt-5.5`. Do not switch models again after scored eligibility unless the benchmark version itself is deliberately superseded.

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

The expensive deterministic toolchain setup is already cached and independently checked on every recovery run. From an updated PR #118 checkout on Windows:

```powershell
python .\scripts\run_benchmark_b0_codex_chatgpt_calibration_safe.py
```

The race-safe recovery calibration:

1. verifies exact Codex `0.144.6` and file-backed ChatGPT login,
2. verifies the preserved Trace2D/Godot/Node/Godot-MCP toolchain identities and archive hashes,
3. runs an isolated **model-only preflight** with `gpt-5.5` using stdin `-` prompt transport and preserves JSONL/stdout/stderr,
4. runs the real filesystem-isolation canary,
5. preserves exactly one **unscored** attempt in each B0 lane,
6. continues after Agent/verifier failures so losses remain evidence,
7. writes hash-chained raw records and an aggregate report,
8. writes `failure.json` with the exact blocking stage on orchestration failure,
9. scrubs credentials/transient Codex homes and creates a reviewable ZIP.

The recovery path deliberately does **not** set the suite/task to `eligible` and never invokes `--scored`.

## Credential and packaging contract

`~/.codex/auth.json` is password-equivalent. It is copied only into isolated per-run `CODEX_HOME` directories and is never committed or uploaded.

Evidence packaging uses `package_benchmark_b0_evidence.py`, which excludes transient Codex/plugin/cache trees and refuses unexpected `auth.json` files. The safe recovery entrypoint also avoids recursively descending into volatile `codex-home` trees during cleanup.

## Promotion rule

Do not change `qualification_required` / `qualification_candidate` to `eligible` until owner-local evidence proves:

- Codex `0.144.6` accepted,
- `gpt-5.5` accepted through ChatGPT sign-in,
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
