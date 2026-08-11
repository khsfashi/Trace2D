# Godot Agent lane qualification protocol

Target lane: `godot.agent`  
Pinned engine: **Godot 4.7.1-stable**  
Selected bridge: **`@satelliteoflove/godot-mcp@4.1.0`**  
Status: **QUALIFIED — hosted Q1–Q4 + independent fixture oracle passed on 2026-08-11**

This is a qualification protocol, not a scored benchmark task. Its purpose is to prove that the selected public Godot Agent/MCP baseline is genuinely strong enough before Trace2D compares itself against it.

The immutable result summary is committed in [`godot-agent.json`](godot-agent.json). The accepted live bridge run is GitHub Actions run `31416517640`, job `93546725244`; the corresponding explicit `godot.agent` fixture oracle is run `31416517680`, job `93546725112`.

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

## Frozen environment

The accepted hosted qualification pinned and recorded:

- Godot `4.7.1-stable`, official Linux x86_64 binary verified with the release `SHA512-SUMS.txt`,
- Ubuntu 24.04.4 LTS x86_64 GitHub-hosted runner,
- Node.js `v22.18.0`,
- exact package `@satelliteoflove/godot-mcp@4.1.0`,
- npm integrity `sha512-uq3Gh5n7fos8vIoXpr32/K7r9tL9eYLbERr+Tolksg3Y+FC5coYEkRkbJ1JktMMhoH/BnGWsWhE5E+XJ/nMEPg==`,
- MCP client `scripts/qualify_godot_agent_mcp_live.py@2`,
- source and installed fixture tree hashes before/after qualification.

The exact bridge addon is installed into a copied fixture with the bridge's public installer and enabled through the normal Godot editor plugin contract. No Trace2D benchmark verifier is copied into the runtime-control fixture.

## Required checks and observed result

All four checks passed in the same hosted frozen environment.

### Q1 — authoring — PASS

Using the normal MCP editor authoring surface, the qualifier changed `Player.z_index` from `0` to `7`, saved the scene, read the persisted value back, restored it to `0`, saved again and verified the restore.

This is deliberately a reversible non-solution edit: it proves real project authoring without encoding the scored task answer.

### Q2 — structured runtime inspection — PASS

The bridge launched the game with time frozen from frame zero and read structured runtime state from the `mcp_watch` player.

Observed both initially and after deliberate wall-clock waiting:

```text
semantic_id = player
position_x = 0
position_y = 0
physics_ticks = 0
frozen = true
launched_frozen = true
```

A screenshot is not used as the acceptance oracle.

### Q3 — real timed input — PASS

The qualifier injects the raw `D` key through the public MCP input path while controlled game time advances. The fixture consumes the ordinary `Input.is_key_pressed(KEY_D)` path; no script or setup command writes `Player.position` directly.

The input hold is intentionally longer than the stop boundary. `step_until` ends the window and force-releases the input when the authoritative fixture predicate becomes true, so the hold duration cannot secretly define the result.

Observed after the first accepted step:

```text
input_kinds.key = 1
physics_ticks = 12
position_x = 2
position_y = 0
```

### Q4 — frozen/exact deterministic stepping — PASS

The first debug session was fully stopped, a clean second session launched frozen from frame zero, a different wall-clock delay was introduced, and both runs used the bridge's public `step_until` operation with the same predicate:

```text
tree.get_nodes_in_group("mcp_watch")[0].physics_ticks >= 12
```

Both runs stopped at the same authoritative boundary:

```text
physics_ticks = 12
position_x = 2
position_y = 0
```

The bridge also reported `predicate_met = true` and 12 physics ticks in both calls. The two runs took 194 ms of gameplay time in this successful execution, but milliseconds are not the equality boundary.

## Rejected measurement boundaries preserved as evidence

Two earlier criteria were rejected rather than silently relaxed:

1. **Fixed render frames** were rejected because hosted rendering is uncapped and render frames can advance independently of fixed physics ticks.
2. **Fixed 200 ms game time** passed one run, but a later clean rerun ended at 12 versus 13 physics ticks because the fixed-step scheduler phase can straddle the duration boundary. That criterion was therefore also rejected.

The accepted criterion stops on the fixture's own fixed-physics counter. In the successful accepted run the render-frame counts were 267 and 271; that difference is preserved and intentionally not compared.

## Independent task oracle — PASS

After Q1–Q4, the pinned official Godot binary ran the independent B0 verifier for the actual `godot.agent` lane:

```powershell
python scripts/benchmark_b0.py qualify-fixtures `
  --task b0-semantic-scene-authoring `
  --lane godot.agent
```

Result:

- known-good fixture: **pass**, observed `player / Player / (4, 1)`,
- wrong-position fixture: **fail** with `position_mismatch`, observed `(3, 1)`.

The bridge qualification and task oracle remain separate pieces of evidence: the static task oracle alone cannot prove Q1–Q4, and Q1–Q4 alone cannot bless the task verifier.

## Qualification result contract

The committed [`godot-agent.json`](godot-agent.json) records:

- exact engine and bridge identity,
- exact npm package integrity,
- positive Q1–Q4 booleans,
- environment identity,
- fixture hashes,
- accepted deterministic boundary and rejected earlier boundaries,
- GitHub Actions run/job/artifact IDs and artifact SHA-256,
- independent known-good/known-bad oracle evidence.

This promotes the strongest reviewed Godot candidate from `primary_candidate_pending_qualification` to `selected_qualified`. It does **not** make the overall B0 suite eligible by itself; the same real coding-Agent/model profile still has to be frozen and run repeatedly across all three lanes.

## Source references reviewed for this protocol

- Godot official releases: <https://github.com/godotengine/godot/releases>
- selected bridge package: <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- selected bridge source/docs: <https://github.com/satelliteoflove/godot-mcp>

The selected bridge's documented runtime model includes frozen game-time control, structured live state, real input and predicate-based stepping; the hosted qualification above proves those capabilities in the pinned environment instead of trusting documentation alone.
