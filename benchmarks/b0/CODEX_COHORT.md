# Benchmark B0 frozen Codex cohort

Status: **completed and accepted**.

B0 freezes one coding-Agent/model/isolation/budget profile and one narrow matched task across `godot.generic`, `godot.agent`, and `trace2d.agent`. The preregistered scored cohort has now executed exactly once and is accepted as the final B0 cohort for #102.

## Frozen profile

```text
Agent                    openai-codex-cli@0.144.6
model selector           gpt-5.5
provider revision policy chatgpt_codex_cli_selector_no_dated_snapshot
reasoning                high
approval                 never
permission profile       :workspace
Windows sandbox          elevated
external isolation       windows_ntfs_acl_v1_elevated
protected root           repository-root deny for Codex sandbox SID
shell network            disabled
session persistence      ephemeral
human intervention       0
wall budget              300 s
tool budget              80
input-token budget       100000
output-token budget      20000
```

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model, task, prompt, verifier, backend and budget were frozen before the scored cohort. The input-token ceiling was not raised after unscored or scored observations.

## Isolation history

The original native-Windows Codex custom filesystem profile is permanently rejected after owner-local evidence showed a held-out canary read could succeed and leak to the model.

The accepted replacement boundary is external Windows NTFS ACL around the elevated `CodexSandboxOffline` identity. A real model turn must attempt the exact held-out canary read and receive access denial before matched work may begin. ACL apply and cleanup are fail-closed.

The final scored archive contains one pre-cohort isolation ACL record plus one ACL record for each of the nine scored turns. All ten report distinct host/sandbox identity, successful ACL apply and successful cleanup.

## Accepted unscored calibration

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

Acceptance evidence:

[`qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json)

That calibration established B0 eligibility without changing the frozen budget even though all three calibration attempts exceeded the input-token ceiling.

## Preregistered scored cohort

Policy: [`scored-cohort-v1.json`](scored-cohort-v1.json).

The policy was committed before eligibility and before any scored result:

```text
repetitions_per_lane  3
total_trials          9
automatic_retries     0
replacement_retries   0
early_stop            false
best_of_n             false
```

Order:

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

Accepted archive:

```text
codex-chatgpt-scored-20260811-180458-214dfeb0.zip
SHA-256 0625e084b6704258a537de7005a3f9a427d66147663abc7878d5880b2860ea52
```

Machine-readable acceptance:

[`qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json`](qualification/codex-windows-acl-scored-cohort-accepted-2026-08-11.json)

Human-readable results:

[`RESULTS.md`](RESULTS.md)

## Scored result summary

Every scored attempt completed a provider turn but exceeded the frozen `100000` input-token limit. Therefore all nine authoritative benchmark statuses are `budget_exceeded` and each lane has benchmark success count `0/3`.

Independent semantic verifier outcomes are reported separately:

| Lane | Samples | Benchmark status | Verifier pass | Verifier fail |
|---|---:|---|---:|---:|
| `godot.generic` | 3 | `budget_exceeded` 3/3 | 1 | 2 (`player_missing`) |
| `godot.agent` | 3 | `budget_exceeded` 3/3 | 3 | 0 |
| `trace2d.agent` | 3 | `budget_exceeded` 3/3 | 3 | 0 |

Median input tokens:

```text
godot.generic  201304
godot.agent    420560
trace2d.agent  273128
```

Median tool calls:

```text
godot.generic  16
godot.agent    27
trace2d.agent  23
```

Median wall time:

```text
godot.generic  138061 ms
godot.agent    212327 ms
trace2d.agent  153199 ms
```

These are descriptive results for one task with three samples per lane. They do not establish general engine superiority.

## Integrity review

The accepted archive was independently inspected after upload:

- nine scored raw records exist in the preregistered order;
- nine replay/reverify records exist;
- all records use canonical Agent profile hash `2407c4fe...f11708`;
- all nine human-intervention counts are zero;
- raw record hashes and the previous-record chain independently recompute 9/9;
- replay record hashes and the previous-record chain independently recompute 9/9;
- all nine reverify subprocesses returned zero;
- all nine workspace hashes match the preserved originals;
- all nine reverified verdicts match the original verifier verdicts;
- model preflight passed;
- the real held-out read was attempted and denied without canary leakage;
- ACL apply/cleanup succeeded for all ten ACL records;
- the archive contains no `auth.json`, obvious OpenAI API key, plaintext random canary, raw Windows SID or bearer authorization marker.

## Preserved Godot crash observations

The scored provider trajectories contain six `CrashHandlerException: Program crashed with signal 11` events across five Godot trials: all three `godot.generic` repetitions plus `godot.agent` R2/R3, with two crash events in Agent R2.

These events remain part of the evidence. They did not abort the cohort, and they were not rerolled: all nine final verifier results were produced and all nine final workspaces/verdicts reverified exactly.

## B0 decision

#102's B0 acceptance is satisfied: the same frozen task/model/profile/budget ran repeatedly across all three environments, generated comparable machine-readable records without manual metric reconstruction, retained unsuccessful/budget outcomes, and independently replayed the final artifacts.

B0 proves the benchmark method, not a winner. The next fixed-order core item after PR #118 merges is **#59 Complete Sprite program**.
