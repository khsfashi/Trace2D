# Benchmark B0 frozen Codex cohort

Status: **real elevated-Windows ACL isolation behavior proven; one complete corrected unscored calibration rerun required before eligibility**.

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

Owner-local preflight proves `gpt-5.5` is callable through the owner's ChatGPT Codex session. The canonical Agent profile SHA-256 remains:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model, backend and budget are not changed based on calibration outcomes. The frozen `100000` input-token limit remains in force.

## Isolation qualification

The original native-Windows custom Codex permission profile is permanently rejected after a real held-out canary leak.

The replacement external NTFS ACL mechanism was then qualified with a distinct Codex sandbox SID, writable candidate workspace, denied held-out canary read, no leak, preserved host access, and successful ACL cleanup. The final profile pins `[windows] sandbox = "elevated"` before any matched lane result.

A later real `gpt-5.5` isolation turn independently proved the final elevated boundary: workspace write passed, the exact held-out read was attempted and denied by Windows, the Agent wrote `DENIED`, no secret leaked, ACL cleanup passed and the provider turn completed.

## Pre-scoring matched calibration history

Archive `codex-chatgpt-calibration-20260811-150451-625123c9.zip` passed real-model isolation and reached the three-lane phase.

- `godot.generic` independently verified the required semantic artifact but used `149255` input tokens, exceeding the frozen `100000` budget.
- `trace2d.agent` independently verified the required semantic artifact but used `279614` input tokens, also exceeding budget.
- `godot.agent` exposed a sandbox-SID discovery timing defect and then a volatile `.godot/shader_cache` hashing race before a raw record could be appended.

The favorable verifier-pass artifacts are historical pre-scoring evidence only and are not selected into the final cohort.

Corrections were made without changing model, prompt, task, backend, verifier or budget:

- engine-owned `.godot` cache is excluded from authored artifact identity via `authored_files_excluding_godot_cache_v1`,
- completed provider turns over budget are classified `budget_exceeded`, not infrastructure transport failures.

A following owner archive, `codex-chatgpt-calibration-20260811-154315-31558452.zip`, passed model preflight but stopped before ACL application because the auxiliary `codex sandbox ... whoami` process used only to rediscover the sandbox SID timed out after 60 seconds. No matched lane started and no scored result exists. This is preserved as [`qualification/codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json`](qualification/codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json).

## Final sandbox identity path

The redundant sandbox subprocess has been removed from the final wrapper.

For the frozen elevated + network-disabled Windows backend, the already-qualified local account is `CodexSandboxOffline`. The wrapper now resolves that local account SID directly from the host Windows account database before editor/model startup and keeps the raw SID only in-process.

This does **not** weaken the isolation decision to trusting an account name. The first real-model isolation canary remains mandatory and authoritative:

```text
host resolves CodexSandboxOffline SID
 -> apply repository deny ACE for that SID
 -> real frozen gpt-5.5 turn attempts exact held-out canary read
 -> read must be denied
 -> canary must not leak
 -> ACL cleanup must succeed
 -> only then may matched lanes begin
```

If the resolved account is not the effective identity for the actual Codex turn, the deny cannot protect the canary and the runner fails closed before any lane. This removes a flaky redundant identity subprocess while preserving an end-to-end real-model proof.

## Lane exposure

The benchmark lanes remain unchanged:

- `godot.generic`: pinned Godot 4.7.1 through ordinary shell/project files, no Godot MCP.
- `godot.agent`: same Godot plus qualified `@satelliteoflove/godot-mcp@4.1.0`; injected addon is environment scaffolding and removed before independent verification.
- `trace2d.agent`: frozen public `trace2d` CLI + `trace2d_mcp`, ordinary Trace2D scene file, no benchmark-only engine API.

Exactly one new three-lane **unscored** calibration must be preserved before eligibility. A legitimate `budget_exceeded` record is still a valid preserved lane outcome; infrastructure failures remain separate.

## Preregistered scored cohort

Before eligibility and before any scored result, [`scored-cohort-v1.json`](scored-cohort-v1.json) freezes:

```text
repetitions_per_lane  3
total_trials          9
automatic_retries     0
replacement_retries   0
early_stop            false
best_of_n             false
```

The order rotates deterministically across the three repetitions. Every slot gets at most one attempt; infrastructure outcomes remain visible.

## Current owner-local action

From an updated PR #118 checkout on native Windows, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

Do **not** run the retired `run_benchmark_b0_codex_chatgpt_calibration_safe.py`.

The runner must first pass the real-model ACL canary. Only then may it produce exactly one fresh unscored record in each lane.

## Promotion rule

Still required before `eligible`:

- one positive real-model isolation verdict with packageable ACL evidence,
- exactly three structurally valid fresh unscored lane records,
- common frozen Agent/profile/budget identity,
- provider trajectory/usage where exposed,
- zero human intervention,
- independent verifier result for every lane,
- no silent retry or best-of-N selection.

Only after that review may `suite.json` and the task become `eligible` and the preregistered nine-trial scored cohort begin.
