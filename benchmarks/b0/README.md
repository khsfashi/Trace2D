# Trace2D Benchmark B0

Issue: #102.

B0 is the first executable matched comparison harness for the Trace2D thesis. It measures one frozen coding Agent/model against three different engine/adapter environments:

1. `godot.generic` — stock Godot + normal generic coding/filesystem/process workflow,
2. `godot.agent` — stock Godot + the strongest qualification-passing reviewed public Agent/MCP bridge,
3. `trace2d.agent` — Trace2D + its normal public Agent/CLI/MCP surface.

B0 is intentionally small. It is a harness-integrity milestone, **not** evidence that Trace2D is generally better than Godot.

## Current gate — 2026-08-11

The three environment lanes now have real committed qualification evidence:

- `godot.generic` — pinned Godot 4.7.1 official binary + independent gold/known-bad oracle,
- `godot.agent` — selected `@satelliteoflove/godot-mcp@4.1.0` + hosted Q1–Q4 live bridge qualification + independent gold/known-bad oracle,
- `trace2d.agent` — frozen Trace2D source/build + full repository tests + independent gold/known-bad oracle.

See [`qualification/README.md`](qualification/README.md) and [`BASELINES.md`](BASELINES.md).

The suite nevertheless remains `qualification_required`, and the first task remains `qualification_candidate`.

That means:

- schema/fixture/harness contracts can be tested in CI,
- unscored calibration runs are allowed,
- **scored runs are still rejected by the harness**,
- no comparative benchmark result should be published yet,
- #102 remains open until one real coding-Agent/model wrapper/profile is frozen, its isolation is proven, and at least one repeated matched three-lane cohort is recorded.

This distinction is intentional: engine/bridge readiness is external truth, but it is not a model benchmark result.

## First task: semantic scene authoring

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

It is admitted as a qualification candidate because all three lanes can perform it through ordinary public authoring contracts without adding a benchmark-only Trace2D feature.

The existing Trace2D `public-alpha` movement helper is deliberately **not** used as this task: its movement behavior is sample-specific C++ and would give Trace2D a task-shaped helper Godot does not receive.

## Public cross-engine semantic mapping

The common prompt openly defines the translation:

- Godot semantic identity `player` -> normal group membership `player`,
- Trace2D semantic identity `player` -> normal entity semantic ID `player`.

Both lanes receive the same prompt containing both mappings. The Agent is not graded on guessing a hidden verifier convention.

## Independent verifier and self-validation

The held-out verifier is not copied into the candidate workspace.

Godot:

```text
fresh candidate workspace
 -> pinned stock Godot headless process
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

Each committed verifier has a known-good fixture and a meaningful wrong-position known-bad fixture. The Godot verifier is exercised explicitly for both Godot lanes; the Trace2D verifier is exercised against the frozen built binary.

## Selected Godot Agent baseline

The selected B0 bridge is `@satelliteoflove/godot-mcp@4.1.0` with the npm integrity recorded in [`qualification/godot-agent.json`](qualification/godot-agent.json).

The hosted qualification proved:

- real editor authoring/save/readback,
- structured runtime inspection,
- launch-frozen frame-zero state,
- raw timed `D` key input through normal gameplay input,
- deterministic replay through the public `step_until` control using the fixture's authoritative `physics_ticks >= 12` predicate.

Both clean replay runs stopped at exactly 12 physics ticks with `Player.position_x == 2`. Their uncapped render-frame counts differed (`267` vs `271`), so render frames are retained as environment evidence but are not treated as the deterministic domain.

The measurement history is part of the evidence. Fixed render-frame stepping was rejected first. A fixed 200 ms game-time boundary then passed one run but later exposed 12-vs-13 physics-tick scheduler-phase variance and was also rejected. The accepted criterion therefore stops on authoritative fixed-physics state instead of choosing a convenient wall/game-time approximation.

This baseline is now `selected_qualified`; it is not changed after seeing scored outcomes.

## Frozen Agent/model boundary

See [`AGENT_WRAPPER.md`](AGENT_WRAPPER.md) and [`agent-profile.example.json`](agent-profile.example.json).

One real profile must freeze:

- coding-agent wrapper identity,
- model ID and exact revision/snapshot,
- reasoning/settings,
- wall/tool/token/human budget,
- wrapper command.

The exact JSON profile is SHA-256 hashed into every trial. A report rejects a task cohort that mixes Agent-profile hashes across lanes.

No placeholder profile is accepted as evidence. This is the remaining external gate before the current task can become scored-eligible.

## Isolation

`run-trial` creates a fresh directory per attempt:

```text
benchmark-runs/b0/
  trials/<unique-trial-id>/
    workspace/
    prompt.md
    agent-result.json
    agent.stdout.txt
    agent.stderr.txt
  raw.jsonl
```

Rules:

- a trial directory is never reused,
- each Agent invocation is a fresh process,
- verifier execution is a separate process,
- the scored Agent wrapper must prove workspace/tool isolation so the held-out verifier cannot become a solution oracle,
- mutable MCP/editor/runtime state is reset by the lane wrapper between trials,
- retries create new immutable attempts rather than overwriting failures.

The harness hashes suite, prompt, verifier and harness sources before the Agent runs. Mutation is classified as `benchmark_integrity_failure`.

## Raw record integrity and failure domains

`raw.jsonl` is append-only at harness level. Each record carries a previous-record hash and its own canonical-record SHA-256 along with suite/task/lane/profile/environment/verifier/artifact/metric identity.

Infrastructure, implementation, eligibility, human-intervention and benchmark-integrity failures stay distinct. Provider outage or environment setup failure is preserved rather than silently converted into an Agent task failure.

## Metrics and reporting

Each trial records raw success/failure class, revisions, tool calls, provider-reported token counts, wall time, verifier time, human interventions, normalized operations and engine-native operations where exposed reliably.

Provider/model token counts must come from the provider/client's own usage accounting; the harness does not invent estimates using another tokenizer.

Generate aggregates with:

```powershell
python scripts/benchmark_b0.py report --records benchmark-runs/b0/raw.jsonl
```

Reports keep raw trial count, success count/rate, status counts, medians/ranges and profile-integrity state. There is intentionally no weighted composite score.

## Independent re-verification

A preserved candidate artifact can be checked again without the original stochastic Agent:

```powershell
python scripts/benchmark_b0.py reverify `
  --trial-id <trial-id> `
  --records benchmark-runs/b0/raw.jsonl `
  --replay-records benchmark-runs/b0/replay.jsonl
```

This reruns the independent verifier, confirms the preserved workspace hash and appends separate hash-chained replay evidence.

## Commands

Validate the suite:

```powershell
python scripts/benchmark_b0.py validate-suite
```

Run environment preflight:

```powershell
python scripts/benchmark_b0.py preflight `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent
```

Run an unscored calibration trial once a real profile exists:

```powershell
python scripts/benchmark_b0.py run-trial `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent `
  --agent-profile path/to/frozen-agent-profile.json
```

A `--scored` invocation still fails by design until the real Agent/model isolation gate is completed and the suite/task are explicitly promoted to `eligible`.

## What B0 can eventually claim

Even after #102 is complete, B0 supports only narrow claims such as:

> Under this frozen model/Agent/budget/environment and these admitted micro-tasks, lane X achieved Y/N successes with the recorded cost distribution.

It does **not** justify broad claims that Trace2D is generally better than Godot, that every Agent improves, or that untested Sprite/animation/physics/audio/GPU domains are covered.
