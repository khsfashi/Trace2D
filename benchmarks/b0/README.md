# Trace2D Benchmark B0

Issue: #102.

B0 is the first executable matched comparison harness for the Trace2D thesis. It measures one frozen coding Agent/model against three different engine/adapter environments:

1. `godot.generic` — stock Godot + normal generic coding/filesystem/process workflow,
2. `godot.agent` — stock Godot + the strongest qualification-passing reviewed public Agent/MCP bridge,
3. `trace2d.agent` — Trace2D + its normal public Agent/CLI/MCP surface.

B0 is intentionally small. It is a harness-integrity milestone, **not** evidence that Trace2D is generally better than Godot.

## Current gate

The committed suite is `qualification_required`, and the first task is `qualification_candidate`.

That means:

- schema/fixture/harness contracts can be tested in CI,
- unscored calibration runs are allowed,
- **scored runs are rejected by the harness**,
- no benchmark result should be published yet,
- #102 must remain open until all lane qualification evidence is real and at least one repeated three-lane cohort is recorded.

This is deliberate. The repository does not currently have a connected Godot runtime or frozen external coding-Agent provider in GitHub CI, so pretending those live facts are complete would violate #97/#98's external-truth boundary.

## Why the first task is semantic scene authoring

The first committed task is `b0-semantic-scene-authoring`:

```text
same prompt
 -> create one player entity
 -> semantic identity = player
 -> name = Player
 -> position = (4, 1)
 -> engine must load the result
 -> independent structural verifier decides pass/fail
```

It is chosen because all three current lanes can perform it using ordinary public engine authoring contracts without adding a benchmark-only Trace2D feature.

It is **not** the final B0 suite. Runtime/input/repair tasks can be admitted after the harness and bridge are qualified. In particular, the existing Trace2D `public-alpha` CLI movement path is not used as a benchmark task because its movement logic is sample-specific C++ code; using that would give Trace2D a task-shaped helper that Godot does not receive.

## Public cross-engine semantic mapping

The common prompt openly defines the translation:

- Godot semantic identity `player` -> normal group membership `player`,
- Trace2D semantic identity `player` -> normal entity semantic ID `player`.

Both lanes receive the same prompt containing both mappings. The Agent is not graded on guessing a hidden verifier convention.

## Independent verifier

The candidate workspace never contains the independent verifier.

Godot:

```text
fresh candidate workspace
 -> stock Godot headless process
 -> external benchmarks/b0/verifiers/godot_semantic_scene.gd
 -> load res://main.tscn
 -> inspect Player/group/position
```

Trace2D:

```text
fresh candidate workspace
 -> frozen trace2d binary
 -> trace2d inspect --json
 -> harness independently checks semantic ID/name/position
```

The Agent may run its own checks, but its own `WorkResult`, textual claim, screenshot, or test log is not the score.

## Gold / known-bad self-validation

Every task/lane must provide:

- a known-good fixture that the independent verifier accepts,
- at least one meaningful known-bad fixture that it rejects.

Run:

```powershell
$env:TRACE2D_BENCH_TRACE2D_BIN = "D:/path/to/trace2d.exe"
python scripts/benchmark_b0.py qualify-fixtures `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent
```

Godot lanes use `TRACE2D_BENCH_GODOT_BIN` instead.

A verifier that cannot distinguish the committed gold and known-bad examples is not allowed to score Agent trials.

## Strongest Godot baseline gate

See [`BASELINES.md`](BASELINES.md).

The current first qualification target is `@satelliteoflove/godot-mcp@4.1.0`, because the reviewed public surface combines structured runtime observation, real input, and frozen/exact stepping. The suite still marks this as `primary_candidate_pending_qualification`.

Before `godot.agent` becomes eligible, committed evidence must prove the exact pinned bridge/environment supports:

- normal authoring,
- runtime inspection,
- timed input,
- deterministic/frozen stepping,
- the independent task oracle.

If it fails, the failure is recorded and the next reviewed candidate is qualified. Do not silently select a weaker bridge because it makes Trace2D look better.

## Frozen Agent/model boundary

See [`AGENT_WRAPPER.md`](AGENT_WRAPPER.md) and `agent-profile.example.json`.

One profile freezes:

- coding-agent wrapper identity,
- model ID and exact revision/snapshot,
- reasoning/settings,
- wall/tool/token/human budget,
- wrapper command.

The exact JSON profile is SHA-256 hashed into every trial. The aggregate report fails its fairness integrity check if one task contains more than one Agent-profile hash across lanes.

## Isolation

`run-trial` creates a new directory:

```text
benchmark-runs/b0/
  trials/<unique-trial-id>/
    workspace/          # copied starter only
    prompt.md
    agent-result.json
    agent.stdout.txt
    agent.stderr.txt
  raw.jsonl
```

Rules:

- an existing trial directory is never reused,
- each Agent invocation is a fresh process,
- verifier execution is a separate process,
- scored environments must additionally qualify their Agent wrapper's workspace-only file/tool sandbox so the held-out verifier cannot be read as a solution oracle,
- mutable MCP/editor/runtime state must be reset by the lane wrapper between trials,
- retries create new immutable attempts; they never overwrite a failed record.

The harness hashes its suite, prompt, verifier and harness sources before the Agent runs. Mutation is classified as `benchmark_integrity_failure`.

## Raw record integrity

`raw.jsonl` is append-only at harness level. Every record contains:

- `previous_record_sha256`,
- `record_sha256` over canonical JSON,
- suite hash,
- task/lane/trial IDs,
- frozen Agent-profile hash,
- environment/engine/adapter identity,
- status and failure domain,
- independent verifier evidence,
- workspace tree hash,
- raw metric fields.

The hash chain makes later edits detectable. Reports and replay refuse a broken chain.

This is tamper-evidence, not a substitute for filesystem/WORM storage. For published benchmark evidence, archive the JSONL and trial artifacts in a write-protected or content-addressed location as a separate publication step.

## Failure classification

Statuses are kept separate instead of collapsing everything into `failed`:

| Status | Domain |
| --- | --- |
| `success` | success |
| `environment_failure` | infrastructure |
| `harness_setup_failure` | infrastructure |
| `agent_setup_failure` | infrastructure |
| `tool_transport_failure` | infrastructure |
| `timeout` | implementation |
| `engine_build_test_failure` | implementation |
| `verifier_failure` | infrastructure |
| `capability_not_eligible` | eligibility |
| `human_intervention` | human |
| `benchmark_integrity_failure` | integrity |

Infrastructure failures are preserved in raw evidence and are never silently converted into ordinary task attempts. A retry, if policy permits one, gets a new trial ID.

## Metrics

Each trial records at least:

- success/failure class,
- revision count,
- tool-call count,
- provider-reported input tokens,
- provider-reported output tokens,
- wall time,
- verifier time,
- human interventions,
- normalized operations,
- engine-native operations.

The wrapper must use provider/client usage accounting rather than estimate tokens with a different tokenizer.

## Reporting

```powershell
python scripts/benchmark_b0.py report --records benchmark-runs/b0/raw.jsonl
```

The report emits, per task/lane:

- raw trial count,
- success count/rate,
- status counts,
- medians,
- min/max ranges,
- Agent-profile integrity state.

There is intentionally no single weighted/composite score.

## Independent re-verification

After an Agent trial, the original Agent is not needed:

```powershell
python scripts/benchmark_b0.py reverify `
  --trial-id <trial-id> `
  --records benchmark-runs/b0/raw.jsonl `
  --replay-records benchmark-runs/b0/replay.jsonl
```

This reruns the independent verifier in a new process, checks the candidate workspace hash against the recorded artifact hash, compares the verdict, and appends separate hash-chained replay evidence.

For later runtime tasks, this seam extends to engine-input replay/self-determinism checks; it is deliberately separate from the original Agent trajectory.

## Commands

Validate committed suite structure:

```powershell
python scripts/benchmark_b0.py validate-suite
```

Non-scored environment preflight:

```powershell
python scripts/benchmark_b0.py preflight `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent
```

Run an unscored calibration trial:

```powershell
python scripts/benchmark_b0.py run-trial `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent `
  --agent-profile path/to/frozen-agent-profile.json
```

A `--scored` invocation currently fails by design until the live qualification gate is completed.

## What B0 will not claim

Even after #102 is complete, B0 supports only narrow claims such as:

> Under this frozen model/Agent/budget/environment and these admitted micro-tasks, lane X achieved Y/N successes with the recorded cost distribution.

It does **not** justify:

- “Trace2D is better than Godot,”
- “Trace2D makes every Agent better,”
- broad performance/GPU conclusions,
- extrapolation to Sprite/animation/physics/audio tasks not yet admitted.
