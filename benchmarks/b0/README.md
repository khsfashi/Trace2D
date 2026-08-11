# Trace2D Benchmark B0

Issue: #102.

B0 is the first executable matched comparison harness for the Trace2D thesis. It measures one frozen coding Agent/model against three environments:

1. `godot.generic` — stock Godot + ordinary coding/filesystem/process workflow,
2. `godot.agent` — stock Godot + qualified public Agent/MCP bridge,
3. `trace2d.agent` — Trace2D + its public Agent/CLI/MCP surface.

B0 is intentionally narrow. It is a **harness-integrity milestone**, not evidence that Trace2D is generally better than Godot.

## Current gate — 2026-08-11

All environment/model/isolation prerequisites are complete and the suite/task are now `eligible`.

- `godot.generic` — pinned Godot `4.7.1-stable`, independent known-good/known-bad oracle.
- `godot.agent` — selected `@satelliteoflove/godot-mcp@4.1.0`, live authoring/state/input/deterministic-step qualification plus oracle.
- `trace2d.agent` — frozen Trace2D source/build plus independent oracle.
- Agent — `openai-codex-cli@0.144.6`.
- model selector — `gpt-5.5` through owner ChatGPT sign-in.
- Windows sandbox backend — `elevated`.
- hard isolation — `windows_ntfs_acl_v1_elevated`.
- canonical Agent profile SHA-256 — `2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708`.
- accepted unscored calibration — `codex-chatgpt-calibration-20260811-163459-3812f9f7.zip`, SHA-256 `31d1e70938a3e98716559073518bf1e1de5465316f85bafffab4d58880e097fd`.
- scored results — none yet.

See [`qualification/README.md`](qualification/README.md), [`CODEX_COHORT.md`](CODEX_COHORT.md), [`BASELINES.md`](BASELINES.md), and [`scored-cohort-v1.json`](scored-cohort-v1.json).

## First task: semantic scene authoring

`b0-semantic-scene-authoring` gives every lane the same user-level goal:

```text
create one player entity
semantic identity = player
name = Player
position = (4, 1)
engine must load the result
independent structural verifier decides pass/fail
```

The public prompt openly defines the cross-engine semantic mapping:

- Godot semantic identity `player` -> normal group membership `player`,
- Trace2D semantic identity `player` -> normal entity semantic ID `player`.

The task does not use a Trace2D-specific benchmark helper.

## Independent verifier

The verifier is outside the candidate workspace and is not trusted to the Agent.

Godot:

```text
fresh workspace
 -> pinned stock Godot headless process
 -> external godot_semantic_scene.gd
 -> load main.tscn
 -> inspect Player/group/position
```

Trace2D:

```text
fresh workspace
 -> frozen trace2d CLI
 -> trace2d inspect --json
 -> harness checks semantic ID/name/position
```

Known-good fixtures pass and meaningful wrong-position known-bad fixtures fail in every admitted lane.

## Selected Godot Agent baseline

The selected bridge is `@satelliteoflove/godot-mcp@4.1.0` with integrity recorded in [`qualification/godot-agent.json`](qualification/godot-agent.json).

Qualification proved:

- real editor authoring/save/readback,
- structured runtime inspection,
- raw gameplay input,
- deterministic stepping through public `step_until` using the authoritative predicate `physics_ticks >= 12`.

Fixed render-frame and fixed-millisecond equality boundaries were rejected after exposing scheduler-dependent physics progress. The accepted boundary is authoritative fixed-physics state.

## Frozen Agent/profile/budget

The real profile is [`agent-profile.codex-0.144.6.json`](agent-profile.codex-0.144.6.json).

Frozen budget:

```text
wall_seconds             300
max_tool_calls           80
max_input_tokens         100000
max_output_tokens        20000
max_human_interventions  0
```

The accepted calibration observed all three lanes exceeding the input-token ceiling. That ceiling is **not raised post-result**. A completed provider turn over budget is recorded as `budget_exceeded` in the implementation domain.

## Isolation

The original native-Windows Codex custom filesystem profile is rejected after a real held-out canary leak.

The final boundary uses external Windows NTFS ACL:

```text
resolve CodexSandboxOffline SID
 -> remove stale deny ACE if present
 -> apply repository deny ACE for sandbox SID
 -> run real frozen model
 -> held-out canary read must be denied
 -> no canary leakage
 -> remove ACL in finally
 -> only then run matched trials
```

The account lookup is not the security verdict. The real-model exact-canary read denial is the authority. If the resolved SID does not match the effective model identity, the gate fails before scored work starts.

## Accepted unscored calibration

The final accepted archive contains exactly three immutable unscored records with a valid previous-record SHA chain, common frozen profile, zero human intervention, provider usage, per-turn ACL cleanup and independent verifier evidence.

Observed calibration outcomes:

| Lane | Status | Verifier | Input tokens | Tool calls |
|---|---|---|---:|---:|
| `godot.generic` | `budget_exceeded` | pass | 187515 | 17 |
| `godot.agent` | `budget_exceeded` | fail (`player_missing`) | 508388 | 32 |
| `trace2d.agent` | `budget_exceeded` | pass | 195453 | 17 |

These values are qualification history, not scored comparative results.

## Trial integrity

Each trial uses a fresh process and workspace. Raw records are append-only and hash-chained. The stable owner-local harness hashes authored candidate state while excluding engine-owned `.godot` cache, which may disappear asynchronously after Godot exits.

Failure domains stay separate:

- infrastructure,
- implementation including `budget_exceeded`,
- eligibility,
- human intervention,
- benchmark integrity.

Provider/model token counts come from provider/client usage; the harness does not estimate them with another tokenizer.

## Preregistered scored cohort

Before eligibility and before any scored result, B0 froze the following policy:

```text
repetitions per lane  3
total attempts        9
automatic retries     0
replacement retries   0
early stop            false
best-of-N              false
```

Order:

```text
R1  godot.generic -> godot.agent   -> trace2d.agent
R2  godot.agent   -> trace2d.agent -> godot.generic
R3  trace2d.agent -> godot.generic -> godot.agent
```

Every scheduled slot receives at most one attempt. Infrastructure failures are not replaced with new samples.

## Current owner-local command

Run the scored cohort only through the committed orchestrator:

```powershell
python .\scripts\run_benchmark_b0_codex_windows_acl_scored_cohort.py
```

It performs:

```text
model preflight
 -> real-model ACL isolation canary
 -> exactly nine preregistered scored attempts
 -> aggregate report
 -> independent reverify of all nine preserved workspaces
 -> scrubbed evidence ZIP
```

Do not manually run individual scored slots or rerun a failed scheduled slot.

## Reporting

The final report preserves raw sample count, success/status counts, medians/ranges, tool/tokens/wall-time distributions, verifier results and profile-integrity state. There is intentionally no weighted composite score.

Even after #102 is complete, B0 supports only narrow statements such as:

> Under this frozen Agent/model/budget/environment and this admitted micro-task, lane X achieved Y/N successes with the recorded cost distribution.

It does **not** justify claims that Trace2D is generally better than Godot, that every Agent improves, or that untested Sprite/animation/physics/audio/GPU domains are covered.

After the single preregistered scored archive is reviewed, PR #118 may be made ready/merged, #102 closed, and the fixed roadmap advances to #59 Complete Sprite program.
