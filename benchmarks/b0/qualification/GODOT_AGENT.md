# Godot Agent lane qualification protocol

Target lane: `godot.agent`  
Pinned engine: **Godot 4.7.1-stable**  
Selected bridge: **`@satelliteoflove/godot-mcp@4.1.0`**  
Status: **QUALIFIED — hosted Q1–Q4 + independent fixture oracle passed on 2026-08-11**

This is a qualification protocol, not a scored benchmark task. Its purpose is to prove that the selected public Godot Agent/MCP baseline is genuinely strong enough before Trace2D compares itself against it.

The immutable result summary is committed in [`godot-agent.json`](godot-agent.json). The successful live bridge run is GitHub Actions run `31415374188`, job `93543024897`; the explicit `godot.agent` fixture oracle is run `31415748318`, job `93544223996`.

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

The successful hosted qualification pinned and recorded:

- Godot `4.7.1-stable`, official Linux x86_64 binary verified with the release `SHA512-SUMS.txt`,
- Ubuntu 24.04.4 LTS x86_64 GitHub-hosted runner,
- Node.js `v22.18.0`,
- exact package `@satelliteoflove/godot-mcp@4.1.0`,
- npm integrity `sha512-uq3Gh5n7fos8vIoXpr32/K7r9tL9eYLbERr+Tolksg3Y+FC5coYEkRkbJ1JktMMhoH/BnGWsWhE5E+XJ/nMEPg==`,
- MCP client `scripts/qualify_godot_agent_mcp_live.py@1`,
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

The qualifier injected the raw `D` key through the public MCP input path during an explicit 200 ms game-time step. The fixture itself consumes the ordinary `Input.is_key_pressed(KEY_D)` path; no script or setup command writes `Player.position` directly.

Observed after the first step:

```text
input_kinds.key = 1
physics_ticks = 12
position_x = 1.83
position_y = 0
```

### Q4 — frozen/exact deterministic stepping — PASS

The first debug session was fully stopped, a clean second session launched frozen from frame zero, a different wall-clock delay was introduced, and the exact same 200 ms game-time interval with the same timed raw `D` input was executed.

Observed replay result:

```text
physics_ticks = 12
position_x = 1.83
position_y = 0
```

The first and second run therefore matched on the authoritative deterministic domain used by this fixture.

The successful run intentionally **does not compare render-frame counts**. They were `204` and `261` across the two runs because hosted rendering is uncapped; render frames are not a fixed game-time interval. An earlier qualification attempt exposed this distinction and was rejected rather than weakening the criterion after the fact. The accepted protocol compares fixed game time, physics ticks and semantic state.

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
- deterministic observations,
- GitHub Actions run/job/artifact IDs and artifact SHA-256,
- independent known-good/known-bad oracle evidence.

This promotes the strongest reviewed Godot candidate from `primary_candidate_pending_qualification` to `selected_qualified`. It does **not** make the overall B0 suite eligible by itself; the same real coding-Agent/model profile still has to be frozen and run repeatedly across all three lanes.

## Source references reviewed for this protocol

- Godot official stable download/archive: <https://godotengine.org/download/windows/>
- selected bridge package: <https://www.npmjs.com/package/@satelliteoflove/godot-mcp>
- selected bridge source/docs: <https://github.com/satelliteoflove/godot-mcp>

The selected bridge's documented runtime model includes frozen game-time control, structured live state and real input; the hosted qualification above proves those capabilities in the pinned environment instead of trusting documentation alone.
