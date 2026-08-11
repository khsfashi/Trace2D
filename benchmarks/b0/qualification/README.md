# B0 qualification evidence

## Current state — 2026-08-11

B0 is now **eligible** for the preregistered scored cohort.

- `godot.generic` — oracle-qualified.
- `godot.agent` — bridge- and oracle-qualified with exact bridge `@satelliteoflove/godot-mcp@4.1.0`.
- `trace2d.agent` — oracle-qualified with committed Windows evidence.
- coding Agent/model — frozen to `openai-codex-cli@0.144.6` + ChatGPT-managed `gpt-5.5`.
- Windows sandbox backend — `elevated`.
- isolation backend — `windows_ntfs_acl_v1_elevated`.
- canonical Agent profile SHA-256 — `2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708`.
- suite state — `eligible`.
- task state — `eligible`.
- scored results — none yet.
- next gate — exactly the preregistered nine scored slots in `scored-cohort-v1.json`.

The frozen `100000` input-token limit remains unchanged after calibration observation.

## Accepted owner-local calibration

Accepted evidence is [`codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](codex-windows-acl-unscored-calibration-accepted-2026-08-11.json).

Source archive:

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

The accepted archive proves:

- `gpt-5.5` preflight passed with provider usage,
- real elevated-Windows ACL isolation passed,
- workspace write succeeded,
- exact held-out canary read attempt was observed and denied,
- no canary secret leaked,
- ACL apply and cleanup succeeded,
- the same ACL lifecycle succeeded for all three matched lane turns,
- exactly three unscored raw records were appended,
- all records use the same frozen canonical Agent profile hash,
- raw record SHA-256 values and the previous-record chain independently recompute correctly,
- human intervention count is zero in all lanes,
- provider trajectories/usage and independent verifier results are present,
- packaged evidence contains no `auth.json`, raw SID, canary file/value or obvious API-key pattern.

Unscored outcomes:

| Lane | Status | Verifier | Input tokens | Tools |
|---|---|---|---:|---:|
| `godot.generic` | `budget_exceeded` | pass | 187515 | 17 |
| `godot.agent` | `budget_exceeded` | fail: `player_missing` | 508388 | 32 |
| `trace2d.agent` | `budget_exceeded` | pass | 195453 | 17 |

These are calibration outcomes, not scored comparative results. All three exceeded the frozen input-token budget, which remains unchanged. A completed provider turn over budget is an implementation-domain `budget_exceeded` outcome rather than an infrastructure transport failure.

The `godot.agent` verifier failure is also preserved rather than repaired away: its authored scene used the `Player` node as the scene root instead of satisfying the verifier-required scene structure. Calibration eligibility requires faithful record/verifier behavior, not a favorable result.

## Final isolation contract

The final wrapper is [`../../../scripts/benchmark_b0_codex_windows_acl_wrapper.py`](../../../scripts/benchmark_b0_codex_windows_acl_wrapper.py).

It:

1. pins Codex built-in `:workspace`,
2. pins `[windows] sandbox = "elevated"`,
3. resolves the already-qualified local `CodexSandboxOffline` account SID from the host Windows account database,
4. rejects equality with the host SID,
5. removes any stale deny ACE from an interrupted previous turn,
6. applies a repository NTFS deny ACE for the sandbox SID,
7. runs the real frozen model with shell network disabled,
8. removes the deny ACE in `finally`,
9. preserves scrubbed ACL lifecycle evidence,
10. keeps raw provider JSONL unchanged.

The account lookup is not trusted by itself. Before the scored cohort, a real `gpt-5.5` isolation turn must attempt the exact held-out canary read and that read must be denied with no leakage. If the resolved account is not the actual Codex identity, the canary gate fails before any scored slot starts.

## Stable harness semantics

[`../../../scripts/benchmark_b0_stable_harness.py`](../../../scripts/benchmark_b0_stable_harness.py) preserves the pre-scoring corrections discovered during calibration:

- workspace identity policy `authored_files_excluding_godot_cache_v1` excludes engine-owned `.godot` cache from authored artifact identity;
- completed provider turns beyond the frozen resource ceiling become `budget_exceeded`, not `tool_transport_failure`.

No model, prompt, task, verifier, backend or budget changed to obtain eligibility.

## Preserved pre-scoring history

Historical failures remain committed as qualification/infrastructure evidence rather than being rewritten as engine losses. They include model availability attempts, the rejected native-Windows permission-profile canary leak, model-free ACL probe transport defects, the integrated Windows-backend omission, Windows command-display matcher false negative, the first partial three-lane harness attempt, and the redundant sandbox-identity subprocess timeout.

The decisive records are:

- [`codex-chatgpt-native-windows-isolation-breach-2026-08-11.json`](codex-chatgpt-native-windows-isolation-breach-2026-08-11.json) — rejected old isolation profile after a real canary leak.
- [`codex-windows-acl-backend-qualified-2026-08-11.json`](codex-windows-acl-backend-qualified-2026-08-11.json) — model-free external ACL mechanism qualification.
- [`codex-windows-acl-final-profile-freeze-2026-08-11.json`](codex-windows-acl-final-profile-freeze-2026-08-11.json) — final elevated backend frozen before matched results.
- [`codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json`](codex-windows-acl-unscored-calibration-harness-attempt-2026-08-11.json) — first real lane exposure and harness corrections.
- [`codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json`](codex-windows-acl-unscored-isolation-sid-discovery-timeout-2026-08-11.json) — removed redundant SID subprocess failure.
- [`codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](codex-windows-acl-unscored-calibration-accepted-2026-08-11.json) — final accepted calibration and eligibility decision.

## Current owner-local command

After updating PR #118, run only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py
```

Do not manually run individual scored slots and do not rerun failed slots. The scored runner reads the committed preregistration, re-proves model isolation first, executes exactly nine scheduled attempts with zero replacement retry, independently reverifies all nine workspaces, produces the aggregate report and packages one scrubbed ZIP.

## Remaining #102 gate

After the scored ZIP is reviewed:

1. verify nine raw scored records and nine replay records,
2. verify frozen profile/budget/ACL identity and no human intervention,
3. publish raw counts/status/resource distributions without best-of-N or broad superiority claims,
4. mark PR #118 ready, merge and close #102,
5. continue to #59 Complete Sprite program.

## Godot deterministic-step note

The qualified `godot.agent` bridge uses public `step_until` with the authoritative predicate:

```text
until = tree.get_nodes_in_group("mcp_watch")[0].physics_ticks >= 12
```

Fixed render-frame count and fixed 200 ms boundaries were rejected because they exposed scheduler-dependent physics progress. The accepted qualification runs stopped at exactly physics tick 12 with `Player.position_x == 2`.
