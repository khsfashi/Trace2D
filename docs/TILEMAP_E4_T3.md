# TileMap E4 T3 — deterministic terrain rule authoring and setup-time compilation

Parent: #73  
Implementation slice: #206

## Decision

T3 adds terrain painting as an authoring/preprocessing layer, not as a second runtime tile model.

```text
TileSet + authored TileMap
 + versioned TileTerrainRule document
 -> strict deterministic validation
 -> cardinal same-terrain mask resolution
 -> ordinary TileMapDocument cells
 -> existing CompileTileMap
 -> existing 8-byte CompiledTileCell runtime
 -> existing T1 presentation + T2 semantic handoff
```

Terrain inference never runs in the frame path. Once terrain paint is compiled, renderer/query code cannot distinguish generated terrain cells from ordinary authored tile cells.

## Authored contract

A terrain document targets one existing TileMap layer and may define multiple semantic terrain kinds. Each terrain owns a required fallback tile. Exact cardinal-mask rules override that fallback.

```toml
format_version = 1
id = "room_a_terrain"
tile_set = "overworld"
tile_map = "room_a"
layer = "ground"

[[terrains]]
id = "grass"
fallback_tile = "grass_isolated"

[[rules]]
terrain = "grass"
neighbors = ["north", "south"]
tile = "grass_vertical"

[[cells]]
cell = [4, 2]
terrain = "grass"
```

`cell` uses the same layer-local integer coordinate convention as authored `TileMap` cells.

## Finite rule vocabulary

T3 deliberately supports only exact four-neighbor connectivity:

- `north`,
- `east`,
- `south`,
- `west`.

The mask means "a painted cell of the same terrain exists in exactly these cardinal directions". Out-of-layer coordinates count as absent. Different terrain kinds do not connect.

A terrain's `fallback_tile` covers every mask without an explicit rule. This makes incomplete rule sets deterministic without wildcard precedence or implicit best-match behavior.

T3 does **not** add diagonals, Wang edges, blob masks, weighted/random variants or arbitrary predicates. Those expand the authoring language only when a committed representative workload demonstrates that the cardinal contract is insufficient.

## Determinism and overwrite semantics

Canonical serialization sorts:

- terrains by semantic id,
- rules by `(terrain id, cardinal mask, output tile id)`,
- paint cells by `(y, x, terrain id)`.

Paint coordinates are unique. Compilation therefore has no overlap priority rule to guess.

A terrain-painted coordinate explicitly replaces any ordinary authored tile cell at the same coordinate in the target layer. Unpainted cells and their transforms are preserved. Generated terrain cells use identity tile transform; transform variants remain explicit TileMap authoring rather than hidden terrain-rule behavior.

The final target-layer cell list is canonicalized by `(y, x, tile id)`, so terrain/rule input ordering does not affect generated TileMap output.

## Validation

Compilation rejects:

- TileSet / TileMap identity mismatch,
- unknown target layer,
- duplicate terrain ids,
- missing or unknown fallback tiles,
- unknown terrain ids in rules or paint cells,
- unknown output tile ids,
- duplicate exact `(terrain, cardinal mask)` rules,
- duplicate paint coordinates,
- paint coordinates outside target-layer local bounds,
- unknown fields, unknown cardinal names, or duplicate neighbor names,
- target layers outside the current T0 bounded dense-cell limit.

The resulting TileMap still passes through the existing TileMap compiler, which remains the authority for final TileMap validation and runtime compilation.

## Cost model

Terrain preprocessing is setup/offline work.

For the current T0 bounded dense representation, compilation allocates one temporary contiguous `uint32` terrain-index scratch array for the target layer. This gives O(1) cardinal neighbor checks without a per-painted-cell node/object allocation. The scratch array is discarded after generation and does not alter retained runtime memory.

Steady-state runtime costs remain unchanged:

- `CompiledTileCell` stays 8 bytes,
- no per-cell string/object/property bag is added,
- no terrain rules are evaluated during rendering, inspection or semantic lookup,
- no chunk metadata is introduced by T3.

## Still open in #73

T3 leaves the final large-map/import closure slice:

- deterministic generated companion/import representation when dense text is impractical,
- workload evidence for any storage/chunking change beyond T1's zero-retained-chunk baseline,
- bounded rebuild/conversion behavior for that representation.

Animated tiles and richer terrain vocabularies remain deferred until representative requirements justify them.
