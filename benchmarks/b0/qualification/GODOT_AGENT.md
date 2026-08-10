# Godot Agent lane qualification protocol

Target lane: `godot.agent`  
Pinned engine candidate: **Godot 4.7.1-stable**  
Pinned bridge candidate: **`@satelliteoflove/godot-mcp@4.1.0`**

This is a qualification protocol, not a scored benchmark task. Its only purpose is to prove that the selected public Godot Agent/MCP baseline is genuinely strong enough before Trace2D compares itself against it.

## Fixture

Use a fresh copy of [`godot_agent_fixture`](godot_agent_fixture/).

The fixture intentionally contains one ordinary `Player` Node2D in groups `player` and `mcp_watch`. `player.gd` exposes `_mcp_state()` and moves right while the real `D` key is held. No scored B0 solution is embedded in this project.

Expected initial semantic state:

```text
semantic_id = player
position_x = 0
position_y = 0
physics_ticks = 0 while game time remains frozen
```

## Environment freeze

Record before qualification:

- exact Godot executable identity/version: `4.7.1-stable`,
- OS and architecture,
- Node.js exact version (must satisfy the bridge's Node 20+ requirement),
- exact package: `@satelliteoflove/godot-mcp@4.1.0`,
- exact MCP client/coding-Agent wrapper identity used to issue the calls,
- addon files installed by the bridge and the project tree hash before/after qualification.

Install the exact bridge addon into the copied fixture using the bridge's normal public installer, then enable the `Godot MCP` editor plugin. Do not copy any Trace2D benchmark verifier into the Godot project.

## Required checks

All four checks must pass in the same frozen environment.

### Q1 — authoring

Using the normal Godot Agent/MCP authoring workflow, make a reversible non-solution edit such as adding a metadata/tag value to `Player`, save the scene, read it back through the normal bridge/editor state, then restore the fixture before the runtime checks.

Pass condition:

- the bridge can inspect and modify the real project through its documented authoring/editor surface,
- the edit persists through a save/readback,
- no task-specific helper or hidden benchmark solution is introduced.

### Q2 — structured runtime inspection

Run the fixture through the bridge with game time frozen at startup and request structured runtime state for the `mcp_watch` player.

Pass condition:

- `semantic_id == "player"`,
- `position_x == 0`,
- `position_y == 0`,
- repeated observation while still frozen does not advance `physics_ticks` merely because the observer waited in wall-clock time.

A screenshot alone does not satisfy this check.

### Q3 — real timed input

With the game controlled through the bridge, inject the real `D` key (or the equivalent normal Godot key input path exposed by the bridge) during an explicitly stepped game-time window.

Pass condition:

- `position_x` increases during the stepped interval,
- the movement is caused by the ordinary `Input.is_key_pressed(KEY_D)` path in `player.gd`,
- no live GDScript or scenario-setup command directly writes `Player.position` for this check.

This distinction matters: `godot_exec` may be useful for scenario setup in general, but it must not be used to fake the input qualification.

### Q4 — frozen/exact deterministic stepping

Restart the fixture from the same initial state, freeze time at frame 0, then advance the same fixed game-time interval twice in two clean runs with the same timed `D` input sequence.

Pass condition:

- both runs expose the same observed `physics_ticks`,
- both runs expose the same `position_x` within the engine's deterministic numeric representation for this fixture,
- wall-clock delay before issuing the step does not change the outcome,
- the observation is obtained after the explicit game-time step rather than racing a freely running process.

If this check fails or is unsupported, the primary bridge is **not** silently weakened or excused. Record the negative evidence and qualify the next reviewed candidate.

## Required evidence file

After all checks genuinely pass, commit `benchmarks/b0/qualification/godot-agent.json` with at least:

```json
{
  "schema_version": 1,
  "suite_id": "trace2d-b0",
  "lane_id": "godot.agent",
  "qualified": true,
  "engine": {
    "id": "godot",
    "version": "4.7.1-stable"
  },
  "bridge": {
    "id": "satelliteoflove/godot-mcp",
    "version": "4.1.0"
  },
  "checks": {
    "authoring": true,
    "runtime_inspection": true,
    "timed_input": true,
    "deterministic_step": true
  },
  "environment": {
    "os": "record exact value",
    "architecture": "record exact value",
    "node_version": "record exact value",
    "mcp_client": "record exact value"
  },
  "evidence": [
    "path-or-immutable-reference-to-authoring-log",
    "path-or-immutable-reference-to-runtime-state-log",
    "path-or-immutable-reference-to-input-log",
    "path-or-immutable-reference-to-step-replay-log"
  ],
  "generated_at": "UTC timestamp"
}
```

Do not create this file just because the addon installs or because the first static B0 task happens to work.

## Oracle check after bridge qualification

The bridge qualification is separate from the independent task verifier. After Q1-Q4 pass, also run the Godot gold/known-bad oracle against the pinned Godot binary:

```powershell
$env:TRACE2D_BENCH_GODOT_BIN = "<absolute path to Godot 4.7.1 x86_64 executable>"
python scripts/benchmark_b0.py qualify-fixtures `
  --task b0-semantic-scene-authoring `
  --lane godot.agent
```

The known-good fixture must pass and the wrong-position known-bad fixture must fail. This command does **not** prove Q1-Q4 by itself.

## Source references reviewed for this protocol

- Godot official stable download/archive: <https://godotengine.org/download/windows/>
- selected bridge package: <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- selected bridge source/docs: <https://github.com/satelliteoflove/godot-mcp>

The selected bridge's documented runtime model explicitly includes frozen/exact game-time control, structured live state and real input; those advertised capabilities are why B0 requires them to pass before the bridge can become the scored baseline.
