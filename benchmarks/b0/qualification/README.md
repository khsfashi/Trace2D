# B0 qualification evidence

A **scored** B0 trial remains blocked until the suite/task are explicitly promoted to `eligible`. Do not commit placeholder qualification or success evidence.

## Current state — 2026-08-11

- `godot.generic` — oracle-qualified.
- `godot.agent` — bridge- and oracle-qualified with exact bridge `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent` — oracle-qualified with committed Windows evidence.
- coding Agent/model — frozen to `openai-codex-cli@0.144.6` + ChatGPT-managed `gpt-5.5`.
- Windows sandbox backend — frozen to `elevated` before the first matched lane.
- isolation backend — `windows_ntfs_acl_v1_elevated`; model-free mechanism qualified and a later real-model canary turn passed.
- scored results — none.
- next gate — one complete corrected three-lane **unscored** calibration archive.

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model, backend and committed budget remain frozen. In particular, the `100000` input-token limit is not raised after seeing calibration usage.

## Preserved owner-local history

Every record below is pre-scoring evidence.

- [`codex-chatgpt-model-attempt-2026-08-11.json`](codex-chatgpt-model-attempt-2026-08-11.json) — dated model snapshot unavailable through ChatGPT-managed Codex.
- [`codex-chatgpt-recovery-attempt-2026-08-11.json`](codex-chatgpt-recovery-attempt-2026-08-11.json) — early observability gap.
- [`codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json`](codex-chatgpt-gpt56-rollout-attempt-2026-08-11.json) — `gpt-5.6` reached the provider but was unavailable to the owner account.
- [`codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json`](codex-chatgpt-gpt55-isolation-timeout-attempt-2026-08-11.json) — `gpt-5.5` preflight passed; old isolation child timed out before verdict.
- [`codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](codex-chatgpt-native-windows-isolation-breach-2026-08-11.json) — old custom Codex permission profile rejected after a real held-out canary leak.
- [`codex-windows-acl-probe-cli-shape-attempt-2026-08-11.json`](codex-windows-acl-probe-cli-shape-attempt-2026-08-11.json) — obsolete sandbox CLI shape.
- [`codex-windows-acl-probe-cmd-transport-attempt-2026-08-11.json`](codex-windows-acl-probe-cmd-transport-attempt-2026-08-11.json) — nested `cmd.exe` transport defect.
- [`codex-windows-acl-probe-freeform-cmd-transport-attempt-2026-08-11.json`](codex-windows-acl-probe-freeform-cmd-transport-attempt-2026-08-11.json) — second free-form command transport defect.
- [`codex-windows-acl-backend-qualified-2026-08-11.json`](codex-windows-acl-backend-qualified-2026-08-11.json) — model-free external ACL mechanism passed.
- [`codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json`](codex-windows-acl-integrated-backend-unspecified-attempt-2026-08-11.json) — clean `CODEX_HOME` lacked explicit Windows backend; host/sandbox SID equality failed closed before lanes.
- [`codex-windows-acl-final-profile-freeze-2026-08-11.json`](codex-windows-acl-final-profile-freeze-2026-08-11.json) — final pre-lane profile pins `[windows] sandbox = "elevated"`.
- [`codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json`](codex-windows-acl-integrated-command-matcher-false-negative-2026-08-11.json) — real elevated model isolation behaved correctly, but doubled Windows backslashes caused an evidence-matcher false negative.
- [`codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json`](codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json) — first real three-lane calibration attempt. Isolation passed; `godot.generic` and `trace2d.agent` produced verifier-pass artifacts but exceeded the frozen input-token budget; `godot.agent` exposed sandbox-SID timing and volatile `.godot/shader_cache` hashing defects before a raw record could be appended.

None of these is a scored comparative result.

## Current final isolation contract

The final wrapper is [`../../../scripts/benchmark_b0_codex_windows_acl_wrapper.py`](../../../scripts/benchmark_b0_codex_windows_acl_wrapper.py). It:

1. pins Codex built-in `:workspace`,
2. pins `[windows] sandbox = "elevated"`,
3. prepares `CODEX_HOME`,
4. discovers host/sandbox identity **before** Godot editor startup,
5. rejects SID equality and keeps the raw SID only in-process,
6. applies the repository NTFS deny ACE for that sandbox identity,
7. runs Codex with shell network disabled,
8. removes the deny ACE in `finally`,
9. preserves scrubbed ACL lifecycle evidence,
10. canonicalizes doubled Windows backslashes only for the isolation attempt matcher.

Raw provider JSONL is never rewritten.

## Stable owner-local harness semantics

[`../../../scripts/benchmark_b0_stable_harness.py`](../../../scripts/benchmark_b0_stable_harness.py) is the owner-local matched-run entrypoint used by the calibration runner.

It makes two pre-eligibility corrections without changing the benchmark task:

- workspace identity uses `authored_files_excluding_godot_cache_v1`; engine-generated `.godot` cache is excluded because it is not authored candidate state and may disappear asynchronously after Godot exits;
- a provider turn that completed but exceeded the frozen task budget is classified `budget_exceeded` in the implementation domain rather than `tool_transport_failure` in infrastructure.

The budget itself is unchanged. The two verifier-pass artifacts from the failed calibration are retained only as historical evidence and are **not** selected into the final cohort.

## Current owner-local command

After updating PR #118, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_calibration.py
```

Do not use the retired `run_benchmark_b0_codex_chatgpt_calibration_safe.py`.

The runner performs:

```text
gpt-5.5 preflight
 -> real elevated-Windows ACL isolation canary
 -> godot.generic   exactly one unscored attempt
 -> godot.agent     exactly one unscored attempt
 -> trace2d.agent   exactly one unscored attempt
 -> aggregate report + scrubbed ZIP
```

A legitimate `budget_exceeded` lane is still a preserved benchmark result. A true infrastructure failure remains separate and does not become an engine loss.

## Promotion rule

Before `eligible`, the next archive must contain:

- positive real-model isolation verdict,
- exact held-out read attempt observed and denied,
- no canary leakage,
- packageable ACL apply/cleanup evidence,
- exactly three unscored lane records,
- one common frozen Agent/profile/budget identity,
- zero human intervention,
- provider trajectory/usage where exposed,
- independent verifier result for every preserved lane,
- no silent retry or best-of-N selection.

Only after that review may `suite.json` and the task become `eligible` and the predefined repeated scored cohort begin.

## Godot deterministic-step note

The qualified `godot.agent` bridge uses public `step_until` with the authoritative predicate:

```text
until = tree.get_nodes_in_group("mcp_watch")[0].physics_ticks >= 12
```

Fixed render-frame count and fixed 200 ms boundaries were rejected earlier because they exposed scheduler-dependent physics progress. Two clean accepted runs stopped at exactly physics tick 12 with `Player.position_x == 2`.
