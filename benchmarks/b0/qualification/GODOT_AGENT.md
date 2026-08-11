# Godot Agent lane qualification protocol

Target lane: `godot.agent`  
Pinned engine: **Godot 4.7.1-stable**  
Selected bridge: **`@satelliteoflove/godot-mcp@4.1.0`**  
Status: **QUALIFIED; hosted CI maintenance protocol tightened after a raw-input activation-phase flake**

This is a qualification/maintenance protocol, not a scored benchmark task. It proves that the selected public Godot Agent/MCP baseline can author, inspect, inject real input and step a live Godot project under a reproducible semantic boundary.

The immutable B0 qualification result used for #102 remains committed in [`godot-agent.json`](godot-agent.json). That historical accepted run used the fixture's absolute `physics_ticks >= 12` predicate and happened to observe `position_x == 2` in both clean runs.

After #102 completed, a later unrelated PR CI run exposed a remaining scheduler phase assumption: both clean runs stopped at absolute `physics_ticks == 12`, but the newly injected raw `D` key became visible one physics tick later in one run, producing `position_x == 1.83` instead of `2.0`. This is not a rewrite of the scored B0 cohort. It is a maintenance correction to the always-on bridge qualification so future PRs do not depend on raw-input activation racing the first resumed physics tick.

## Fixture

Use a fresh copy of [`godot_agent_fixture`](godot_agent_fixture/).

The fixture contains one ordinary `Player` `Node2D` in groups `player` and `mcp_watch`. `player.gd` consumes the real Godot input path:

```gdscript
if Input.is_key_pressed(KEY_D):
    input_ticks += 1
    position.x += SPEED * delta
```

It exposes both:

```text
physics_ticks = every physics callback since launch
input_ticks   = physics callbacks in which raw D was actually observed
```

No benchmark answer is embedded in the fixture.

## Why two counters exist

`godot-mcp` schedules the raw input and resumes controlled game time through separate host/engine phases. `start_ms = 0` means the input belongs to the stepped window, but it does not prove that the OS/Godot raw-key state is visible before the very first resumed `_physics_process` callback on every hosted runner.

Therefore:

- `physics_ticks` remains useful evidence of absolute fixed-step progress;
- `input_ticks` is the authoritative movement equality boundary because it advances only when the fixture actually sees `KEY_D`;
- render frames, wall-clock milliseconds, and absolute process physics ticks are not used to compare movement under newly injected input.

This distinction tests what the bridge really controls instead of assuming a scheduler phase it does not expose.

## Required checks

### Q1 — real editor authoring

Using normal MCP editor tools:

1. read `Player.z_index`,
2. change it to a reversible probe value,
3. save and read back,
4. restore the original value,
5. save and verify restoration.

This proves public project authoring without encoding the benchmark solution.

### Q2 — launch-frozen structured runtime state

Launch the game frozen from frame zero and verify structured `mcp_watch` state:

```text
semantic_id  = player
position_x   = 0
position_y   = 0
physics_ticks = 0
input_ticks   = 0
frozen        = true
launched_frozen = true
```

Deliberate wall-clock waiting while frozen must not change either counter or position.

### Q3 — real raw D input + semantic controlled stepping

The qualifier calls the public `godot_game_time step_until` tool with real raw key injection and the predicate:

```text
tree.get_nodes_in_group("mcp_watch")[0].input_ticks >= 12
```

The hold duration is intentionally longer than the target boundary. `step_until` ends the window and force-releases holds, so duration is only a safety ceiling.

Required terminal facts:

```text
input_kinds.key >= 1
input_ticks = 12
physics_ticks >= 12
position_x > 0
predicate_met = true
```

### Q4 — clean replay

Fully stop the first debug session, launch a fresh frozen session, introduce a different wall-clock wait, then repeat the same raw-key + `input_ticks >= 12` predicate.

Required equality:

```text
first.input_ticks  = 12
second.input_ticks = 12
first.position_x   = second.position_x
```

Absolute `physics_ticks` is retained in evidence but may differ by the number of fixed callbacks that occurred before raw `D` became observable. That difference is not movement nondeterminism; it is the explicitly unowned input-activation phase.

## Preserved rejected/retired boundaries

The protocol learned from and preserves four progressively sharper distinctions:

1. **fixed render frames** — rejected because uncapped render frames are not fixed-physics progress;
2. **fixed game milliseconds** — rejected after clean runs ended on different fixed ticks;
3. **absolute `physics_ticks >= 12` as movement equality under newly scheduled raw input** — historical #102 qualification passed, but a later CI replay exposed one-tick input-visibility phase variance (`2.00` vs `1.83` at tick 12); retained as historical evidence, no longer used by always-on CI for movement equality;
4. **input-active physics ticks** — current maintenance boundary; counts only authoritative physics callbacks in which the fixture observes the raw key and therefore compares equal semantic input exposure.

None of these changes weakens the requirements that the game launch frozen, raw key injection be used, state be read structurally, and predicate-based controlled stepping complete successfully.

## Independent B0 task oracle

The actual `godot.agent` task verifier remains independent from this runtime-control fixture. Known-good and wrong-position fixtures are checked with the pinned official Godot binary. The bridge qualification does not self-bless the task oracle, and the task oracle alone does not prove MCP authoring/input/step capabilities.

## Historical B0 evidence vs current CI

[`godot-agent.json`](godot-agent.json) is immutable historical evidence for the #102 qualification that preceded the scored cohort. The current live workflow may emit a newer maintenance evidence shape (`fixture_input_active_physics_ticks`) without retroactively editing the historical record or scored results.

This matters because #102's final scored cohort is already preserved and accepted. Maintenance hardening discovered later must improve future confidence, not rewrite what happened.

## Source references

- Godot official releases: <https://github.com/godotengine/godot/releases>
- selected bridge package: <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- selected bridge source/docs: <https://github.com/satelliteoflove/godot-mcp>

The repository relies on its own hosted observations rather than documentation alone for bridge qualification.
