# Benchmark B0 frozen Codex cohort

Status: **real elevated-Windows ACL isolation behavior proven; one corrected unscored calibration rerun required before eligibility**.

This document freezes the real coding-Agent/model/isolation candidate for #102. It does not make B0 scored-eligible and contains no comparative benchmark result.

## Frozen Agent/model/isolation selection

The committed profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json):

- Agent: OpenAI Codex CLI `0.144.6`,
- ChatGPT Codex CLI selector: `gpt-5.5`,
- provider revision policy: `chatgpt_codex_cli_selector_no_dated_snapshot`,
- reasoning effort: `high`,
- approval policy: `never`,
- built-in permission profile: `:workspace`,
- Windows sandbox backend: `elevated`,
- external isolation backend: `windows_ntfs_acl_v1_elevated`,
- protected-root policy: repository-root NTFS deny for the effective Codex sandbox SID,
- shell network access: disabled,
- session persistence: ephemeral,
- task budget: exactly the B0 task budget,
- human interventions: zero.

Owner-local preflight proves `gpt-5.5` is callable through the owner's ChatGPT Codex session. The current canonical Agent profile SHA-256 is:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

Freeze evidence is [`qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json`](qualification/codex-windows-acl-final-profile-freeze-2026-08-11.json). No matched lane trial had started when the explicit Windows backend was frozen, so this was not post-result baseline selection.

The same Codex version, model selector, reasoning setting, permission profile, Windows backend, isolation backend, task prompt and budget must remain constant across all matched lanes and repeated scored trials.

## Isolation history

The original native-Windows Codex custom permission profile is permanently rejected. Real owner-local evidence showed the held-out canary could be read and exposed to the model while workspace writes were blocked. See [`qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](qualification/codex-chatgpt-native-windows-isolation-breach-2026-08-11.json).

The replacement hard boundary moves held-out protection outside the model-facing Codex permission policy. A model-free Windows mechanism probe then proved:

- Codex sandbox SID differs from the host SID,
- built-in `:workspace` can write the candidate workspace,
- an NTFS deny ACE for the sandbox SID blocks an external canary read,
- the canary does not leak,
- host access is preserved,
- ACL and canary cleanup succeed.

See [`qualification/codex-windows-acl-backend-qualified-2026-08-11.json`](qualification/codex-windows-acl-backend-qualified-2026-08-11.json).

The first integrated wrapper attempt used a clean `CODEX_HOME` without explicitly selecting the Windows backend. SID discovery returned the host SID, so the wrapper failed closed before ACL application, model isolation or any matched lane. This is preserved as [`qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json`](qualification/codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json).

The final profile therefore pins `[windows] sandbox = "elevated"`, which supplies the distinct sandbox identity required by the qualified ACL design.

## Latest real-model isolation turn

Archive `codex-chatgpt-calibration-20260811-141748-a87da13f.zip` reached the final elevated-Windows ACL model isolation turn. The underlying boundary behaved correctly:

- model preflight passed,
- elevated sandbox identity was distinct from the host identity,
- workspace write succeeded,
- the exact held-out canary read was attempted,
- Windows rejected that read with `UnauthorizedAccessException` / access denied,
- the Agent wrote `DENIED`,
- canary secret leakage was false,
- Codex completed normally with provider usage,
- no matched lane started.

The attempt still returned failure because the evidence matcher compared the real single-backslash Windows path against a Codex JSONL command display containing doubled backslashes. `external_read_attempt_observed` was therefore a false negative even though the raw trajectory proves the exact denied read occurred.

This is preserved as [`qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json`](qualification/codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json), classified `infrastructure_isolation_evidence_matcher_windows_escape_false_negative`.

The final wrapper now canonicalizes doubled Windows backslashes **only for the isolation command-path matcher**. Raw Codex JSONL remains unchanged. It also exports scrubbed `acl-isolation.json` outside `.probe-artifacts` so the evidence packager cannot omit the per-turn ACL lifecycle record.

This correction does not alter the model, prompt, task, budget, permission profile, Windows sandbox backend or ACL policy.

## Lane exposure

The benchmark task and adapter lanes remain unchanged:

- `godot.generic`: pinned Godot 4.7.1 through ordinary shell/project files, no Godot MCP.
- `godot.agent`: same Godot plus qualified `@satelliteoflove/godot-mcp@4.1.0`; injected addon is environment scaffolding and removed before independent verification.
- `trace2d.agent`: frozen public `trace2d` CLI + `trace2d_mcp`, ordinary Trace2D scene file, no benchmark-only engine API.

No scored lane may run yet. Exactly one corrected three-lane **unscored** calibration must be preserved first.

## Current owner-local action

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

Do **not** run `run_benchmark_b0_codex_chatgpt_calibration_safe.py`; that entrypoint is retired because its native-Windows custom profile failed integrity.

The runner must first re-prove the real-model isolation gate with the corrected matcher. Only then may it start exactly one unscored attempt in each of:

```text
godot.generic
godot.agent
trace2d.agent
```

Lane failures are retained and later lanes still run. No best-of-N retry is allowed.

## Promotion rule

Already proven:

- all three engine/adapter environments have independent qualification evidence,
- Codex `0.144.6` accepted,
- `gpt-5.5` accepted through ChatGPT sign-in,
- provider usage emitted for completed turns,
- external Windows ACL mechanism qualified,
- final elevated Windows sandbox backend frozen before any matched lane,
- a real model turn actually attempted the held-out read and Windows denied it without leakage.

Still required before `eligible`:

- one corrected isolation verdict with the matcher fix and packageable per-turn ACL evidence,
- exactly one unscored attempt preserved in all three lanes,
- all three records share the frozen Agent/profile/budget identity,
- provider trajectory/usage is preserved where exposed,
- zero human intervention occurs during trials,
- independent verifiers complete after the stochastic Agent exits,
- every failure remains visible rather than being retried away.

Only after those facts are reviewed may `suite.json` and the task become `eligible` and the predefined repeated scored cohort begin. Every scored attempt, including losses and infrastructure outcomes, remains part of the cohort.
