# B0 qualification evidence

A **scored** B0 trial is blocked by `scripts/benchmark_b0.py` until `suite.json` and the task are marked `eligible` and the lane's configured evidence file exists with a positive result.

Do not commit placeholder `qualified: true` files.

## Current state — 2026-08-11

- `godot.generic` — **oracle-qualified** with committed hosted evidence in [`godot-generic.json`](godot-generic.json). GitHub Actions downloaded the pinned official Godot 4.7.1 x86_64 build, verified it against the release `SHA512-SUMS.txt`, verified the engine version, accepted the known-good task fixture and rejected the wrong-position known-bad fixture.
- `godot.agent` — **bridge- and oracle-qualified** with committed hosted evidence in [`godot-agent.json`](godot-agent.json). The exact selected bridge is `@satelliteoflove/godot-mcp@4.1.0`. A hosted Godot editor session proved Q1 authoring, Q2 structured runtime inspection under frame-zero freeze, Q3 real raw-`D` timed input, and Q4 deterministic replay over the same fixed 200 ms game-time interval. Both clean runs produced 12 physics ticks and `Player.position_x == 1.83`; wall-clock waiting while frozen did not advance the state. The independent `godot.agent` gold/known-bad oracle also passed.
- `trace2d.agent` — **oracle-qualified** with committed hosted evidence in [`trace2d-agent.json`](trace2d-agent.json). The Windows CI candidate passed all 188 repository tests and the built `trace2d.exe` accepted the known-good fixture while rejecting the wrong-position known-bad fixture.

All three **environment/bridge qualification records now exist**. The suite/task deliberately remain `qualification_required` / `qualification_candidate` because B0 still requires one real coding-Agent/model wrapper/profile to be frozen and isolated before scored repeated trials can begin. Environment readiness is not a benchmark result.

## Determinism note for the Godot Agent lane

The first live qualification attempt used a fixed render-frame count and exposed an important measurement mistake: an uncapped hosted renderer can execute many render frames between fixed physics ticks, so render-frame count is not the deterministic game-time domain for this fixture.

The accepted Q4 protocol therefore uses a fixed **200 ms game-time interval** with the same timed raw `D` key input in two clean launch-frozen runs. Render-frame counts are retained as evidence but intentionally not compared. Physics ticks and authoritative semantic state are compared and matched exactly in the successful hosted run.

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
  }
}
```

`godot.agent` additionally records the exact bridge package/integrity and these positive checks:

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
