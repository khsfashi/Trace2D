# Benchmark B0 frozen Codex cohort

Status: **eligible; preregistered scored cohort is the remaining owner-local gate**.

This document freezes the coding-Agent/model/isolation candidate and the B0 repetition policy for #102. Eligibility means the matched harness is structurally ready for scored execution; it is not a claim that any lane is superior.

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
- protected-root policy: repository-root NTFS deny for the Codex sandbox SID,
- shell network access: disabled,
- session persistence: ephemeral,
- human interventions: zero,
- task budget: `300s / 80 tools / 100000 input / 20000 output / 0 human`.

Canonical Agent profile SHA-256:

```text
2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708
```

The model, backend, prompt, verifier and budget remain frozen after observing calibration outcomes.

## Isolation qualification

The original native-Windows custom Codex permission profile is permanently rejected after a real held-out canary leak.

The replacement boundary is external Windows NTFS ACL. The final elevated path resolves the already-qualified `CodexSandboxOffline` local account SID from the host Windows account database, applies a repository deny ACE for that SID, runs the real frozen model turn, and requires the exact held-out canary read to be denied with no secret leakage before any matched trial may start. ACL cleanup is mandatory and fail-closed.

The account lookup is only identity acquisition. The real-model canary is the authority: if Codex executes under another identity, the canary read is not blocked and the runner stops before the cohort.

## Accepted unscored calibration

Owner archive:

```text
codex-chatgpt-calibration-20260811-163459-3812f9f7.zip
SHA-256 31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd
```

Accepted evidence: [`qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json`](qualification/codex-windows-acl-unscored-calibration-accepted-2026-08-11.json).

The archive proves:

- `gpt-5.5` model preflight passed,
- real elevated-Windows ACL isolation passed,
- workspace write passed,
- exact held-out read attempt was observed and denied,
- canary secret did not leak,
- ACL apply/cleanup passed for the isolation turn and every lane turn,
- exactly three unscored raw records were appended,
- all three records use canonical Agent profile hash `2407c4fe...f11708`,
- the append-only record SHA chain independently recomputes correctly,
- human intervention is zero in all lanes,
- provider trajectories/usage and independent verifier results are preserved,
- no silent retry or best-of-N selection occurred.

Observed unscored lane outcomes are retained as calibration evidence, not selected as scored results:

| Lane | Status | Independent verifier | Input tokens | Tool calls |
|---|---|---:|---:|---:|
| `godot.generic` | `budget_exceeded` | pass | 187515 | 17 |
| `godot.agent` | `budget_exceeded` | fail (`player_missing`) | 508388 | 32 |
| `trace2d.agent` | `budget_exceeded` | pass | 195453 | 17 |

All three exceed the frozen `100000` input-token budget. The budget is intentionally **not** raised after observing these values. `budget_exceeded` is a first-class implementation-domain outcome, not an infrastructure transport failure.

The `godot.agent` calibration artifact authored a root `Player` node rather than the verifier-required scene structure, so its verifier failure is preserved as a legitimate lane outcome. It does not invalidate harness eligibility.

## Eligibility decision

`benchmarks/b0/suite.json` and `b0-semantic-scene-authoring` are now `eligible` because the calibration established the required structural facts for all three already-qualified lanes: frozen profile identity, real isolation, immutable records, independent verifier execution, zero human intervention, packageable ACL cleanup, and valid failure-domain preservation.

Eligibility does **not** require a calibration success. A benchmark must be able to preserve losses faithfully before it can measure them.

## Preregistered scored cohort

[`scored-cohort-v1.json`](scored-cohort-v1.json) was committed before eligibility and before any scored result. It is now `ready` with unchanged policy:

```text
repetitions_per_lane  3
total_trials          9
automatic_retries     0
replacement_retries   0
early_stop            false
best_of_n             false
```

Deterministic order:

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

Every scheduled slot gets at most one attempt. Infrastructure, budget, implementation, timeout, human and integrity outcomes remain in the cohort; there is no reroll to replace an unfavorable result.

## Current owner-local action

From an updated PR #118 checkout on native Windows, use only:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py
```

The runner performs one model preflight and one real-model ACL canary before any scored slot, then executes exactly the nine preregistered attempts, independently reverifies every preserved workspace, writes the aggregate report, scrubs transient credentials and produces one evidence ZIP.

Do not manually run individual `--scored` slots and do not rerun a failed scheduled slot. If the cohort runner stops because of a genuine orchestration/integrity defect, upload the generated ZIP for diagnosis rather than creating an unofficial replacement sample.

## Remaining #102 gate

1. preserve the single preregistered nine-attempt scored cohort,
2. verify all nine raw records/replay records/profile hashes/ACL evidence,
3. publish raw sample counts, status distributions and resource distributions without broad superiority claims,
4. make PR #118 ready, merge it, close #102,
5. advance to #59 Complete Sprite program.
