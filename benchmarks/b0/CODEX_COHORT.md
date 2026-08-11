# Benchmark B0 frozen Codex cohort

Status: **owner-local external isolation backend qualification required before three-lane calibration**.

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

Owner-local model preflight has already proved that `gpt-5.5` is actually callable through the owner's ChatGPT Codex session. The model identity is not the current blocker.

The same Codex version, model selector, reasoning setting, task prompt and budget must remain constant across all matched lanes and repeated scored trials. The **isolation backend is not yet frozen** because the first native-Windows backend failed its integrity probe before any lane trial existed.

Primary references:

- <https://developers.openai.com/codex/models>
- <https://developers.openai.com/codex/non-interactive-mode>
- <https://developers.openai.com/codex/permissions>
- <https://github.com/openai/codex/tree/rust-v0.144.6>

## Preserved pre-scoring qualification attempts

No attempt below contains a scored engine result. No matched lane trial has started.

1. `gpt-5.5-2026-04-23` dated API snapshot — rejected by ChatGPT-managed Codex before tool use. Preserved as [`qualification/codex-chatgpt-model-attempt-2026-08-11.json`](qualification/codex-chatgpt-model-attempt-2026-08-11.json).
2. guessed `gpt-5.6-sol` identifier — stopped before classifiable isolation evidence and exposed an observability gap. Preserved as [`qualification/codex-chatgpt-recovery-attempt-2026-08-11.json`](qualification/codex-chatgpt-recovery-attempt-2026-08-11.json).
3. documented `gpt-5.6` selector — reached the provider but was unavailable to this ChatGPT account. Preserved as [`qualification/codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json`](qualification/codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json).
4. `gpt-5.5` model preflight — passed; the first isolation child hit the old 90-second process ceiling before a verdict. Preserved as [`qualification/codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json`](qualification/codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json).
5. `gpt-5.5` with the 285-second isolation ceiling — **model passed, but the attempted native-Windows Codex read-deny boundary failed integrity**. The Agent successfully read the exact random canary beside the held-out verifier and the canary value became model-visible. Workspace writes were simultaneously blocked by policy. Preserved as [`qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json), classified as `integrity_isolation_breach_native_windows_profile`.

Attempt 5 is decisive: the attempted custom Codex permission profile is **rejected as a B0 isolation backend**. It is not an engine loss and it must not be tuned until it happens to pass. The three-lane calibration remains blocked.

The owner-local result is also consistent with upstream native-Windows read-deny reports such as `openai/codex#31265`, but B0's decision is based on its own observed canary leak.

## Rejected native-Windows Codex profile

The rejected backend attempted to rely on Codex's named permission profile for both candidate-workspace writes and held-out read denial. The real owner-local run established the opposite effective behavior:

- read/list inside the workspace worked,
- workspace file writes were rejected by policy,
- the exact held-out canary read command executed,
- the external file contents were returned to the model,
- the canary secret therefore leaked.

The backend is not eligible for retry as a scored or unscored lane environment. Increasing timeouts, changing prompt wording, or accepting a weaker canary condition would hide an integrity failure rather than fix it.

## External Windows ACL backend candidate

The next candidate boundary moves the held-out protection **outside the model-facing Codex permission profile**.

The repository now contains [`scripts/qualify_benchmark_b0_windows_acl_isolation.py`](../../scripts/qualify_benchmark_b0_windows_acl_isolation.py). It performs no model call and no engine trial. In a throwaway local directory it:

1. invokes the Codex native-Windows sandbox directly and records the effective sandbox SID,
2. rejects the backend if the sandbox SID equals the host user SID,
3. applies a temporary NTFS deny ACE for only that sandbox SID to a random canary directory,
4. uses Codex's built-in `:workspace` profile to prove a normal workspace write succeeds,
5. uses that same sandbox identity to attempt the protected external read,
6. requires the external read to fail with no canary leak,
7. removes the temporary deny ACE and emits a scrubbed evidence ZIP.

This probe intentionally uses a throwaway canary directory rather than modifying the Trace2D checkout. A passing probe qualifies only the **mechanism**. The full B0 runner may adopt it only after the evidence is reviewed and the repo/harness quarantine lifecycle is implemented with cleanup and failure recovery.

## Lane exposure

The benchmark task and adapter lanes remain unchanged:

- `godot.generic`: pinned Godot 4.7.1 through ordinary shell/project files, no Godot MCP.
- `godot.agent`: same Godot plus qualified `@satelliteoflove/godot-mcp@4.1.0`; injected addon is environment scaffolding and removed before independent verification.
- `trace2d.agent`: frozen public `trace2d` CLI + `trace2d_mcp`, ordinary Trace2D scene file, no benchmark-only engine API.

No lane trial may start until a hard held-out boundary is proven.

## Current owner-local action

From an updated PR #118 checkout on native Windows, run only the external-isolation backend probe:

```powershell
python .\scripts\qualify_benchmark_b0_windows_acl_isolation.py
```

Do **not** rerun `run_benchmark_b0_codex_chatgpt_calibration_safe.py` yet. The old calibration path still represents the rejected Codex-internal read-deny backend and must remain blocked until the replacement boundary is qualified and integrated.

## Promotion rule

Already proven:

- Codex `0.144.6` accepted,
- `gpt-5.5` accepted through ChatGPT sign-in,
- provider usage emitted for a completed model turn,
- all three engine/adapter environments have independent qualification evidence.

Still required before `eligible`:

- a replacement external isolation backend is independently proven,
- the candidate workspace is writable,
- the held-out repository/verifier boundary is unreadable to the effective Agent sandbox identity,
- a random canary is explicitly read-attempted and denied without leakage,
- exactly one unscored attempt is preserved in all three lanes,
- all three records share one frozen Agent/profile/budget identity,
- provider trajectory/usage is preserved where exposed,
- zero human intervention occurs during trials,
- independent verifiers complete after the stochastic Agent exits.

Only after those facts are real may the suite/task become `eligible` and the predefined repeated scored cohort begin. Every scored attempt, including losses and infrastructure outcomes, remains part of the cohort. No best-of-N selection is allowed.
