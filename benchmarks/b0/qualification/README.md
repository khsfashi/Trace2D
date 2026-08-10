# B0 qualification evidence

A **scored** B0 trial is blocked by `scripts/benchmark_b0.py` until `suite.json` and the task are marked `eligible` and the lane's configured evidence file exists with a positive result.

Do not commit placeholder `qualified: true` files.

## Current state — 2026-08-11

- `godot.generic` — **oracle-qualified** with committed hosted evidence in [`godot-generic.json`](godot-generic.json). GitHub Actions downloaded the pinned official Godot 4.7.1 x86_64 build, verified the engine version, accepted the known-good task fixture and rejected the wrong-position known-bad fixture.
- `godot.agent` — **not qualified**. The exact bridge remains `@satelliteoflove/godot-mcp@4.1.0`; [`GODOT_AGENT.md`](GODOT_AGENT.md) defines the required Q1–Q4 authoring/runtime/input/frozen-step protocol. Do not create `godot-agent.json` until those live checks pass.
- `trace2d.agent` — **oracle evidence pending final-head Windows CI**. The CI job exercises the built `trace2d.exe` against the same task's known-good/known-bad fixtures; commit positive evidence only after a successful run on the intended final candidate head.

The suite/task remain `qualification_required` / `qualification_candidate`, so the presence of one lane's positive evidence does not enable scored runs.

## Required lane evidence shape

Every lane evidence file is JSON format version 1 and records the exact environment used:

```json
{
  "schema_version": 1,
  "suite_id": "trace2d-b0",
  "lane_id": "trace2d.agent",
  "qualified": true,
  "engine": {
    "id": "trace2d",
    "version": "...",
    "source_commit": "..."
  },
  "environment": {
    "os": "...",
    "architecture": "..."
  },
  "fixture": {
    "task_id": "b0-semantic-scene-authoring",
    "known_good_passed": true,
    "known_bad_failed": true
  },
  "generated_at": "..."
}
```

`godot.agent` additionally must record the bridge identity/version and these positive checks:

```json
{
  "bridge": {
    "id": "satelliteoflove/godot-mcp",
    "version": "4.1.0"
  },
  "checks": {
    "authoring": true,
    "runtime_inspection": true,
    "timed_input": true,
    "deterministic_step": true
  }
}
```

The bridge qualification is deliberately broader than the first static scene task. It prevents a weak authoring-only Godot bridge from being chosen simply because the first micro-task does not exercise runtime control yet.

## Fixture oracle command

Before a lane can be eligible, run the independent gold/known-bad check:

```powershell
python scripts/benchmark_b0.py qualify-fixtures `
  --task b0-semantic-scene-authoring `
  --lane trace2d.agent
```

or the corresponding Godot lane after setting the engine binary environment variable.

The generated stdout is evidence material, but committing a positive lane evidence file is a separate explicit review step. This separation prevents a script from silently blessing its own environment.
