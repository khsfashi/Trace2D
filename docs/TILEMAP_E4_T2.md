# TileMap E4 T2 — deterministic semantic handoff and independent markers

Parent: #73  
Implementation slice: #204

## Decision

T2 keeps render-cell truth and gameplay-system handoff separate.

The T0 `CompiledTileCell` remains exactly 8 bytes and still contains only resolved tile identity plus presentation transform bits. T2 adds a companion `TileSemanticOverlay` that resolves once against an existing compiled TileSet + TileMap:

```text
TileSet / TileMap
 + versioned TileSemanticOverlay
 -> setup-time tile/layer semantic-id resolution
 -> one 4-byte semantic state per TileSet definition
 -> stable semantic markers stored independently from render occupancy
```

No per-cell string, tag container, object, map or second gameplay grid is introduced.

## Authored companion contract

Representative authoring:

```toml
format_version = 1
id = "room_a_semantics"
tile_set = "overworld"
tile_map = "room_a"

[[rules]]
tile = "wall"
collision = "solid"
navigation = "blocked"
occlusion = "opaque"

[[rules]]
tile = "grass"
navigation = "walkable"

[[markers]]
id = "player_spawn"
kind = "spawn.player"
layer = "ground"
cell = [-2, 3]
tags = ["primary", "checkpoint"]
```

Rules are keyed by stable TileSet tile identity and markers by stable marker identity. Canonical serialization sorts rules by tile id, markers by marker id, and marker tags lexicographically.

The handoff vocabulary is deliberately finite:

- collision: `none | solid`,
- navigation: `none | walkable | blocked`,
- occlusion: `none | opaque`.

T0 tile tags remain the finite custom semantic-label mechanism. T2 does not add a generic `map<string, Variant>` property bag.

## Runtime representation and cost

`CompiledTileSemanticState` is a trivially-copyable 4-byte value. `CompiledTileSemanticOverlay::tileStates` has exactly one entry per compiled TileSet definition.

The steady occupied-cell lookup is:

```text
resolved layer index + world cell
 -> existing O(1) CompiledTileCell
 -> existing uint32 tileIndex
 -> tileStates[tileIndex]
```

This performs no filesystem access, semantic string lookup, allocation, GPU readback or per-cell metadata traversal.

A tile with no explicit semantic rule resolves to the all-`none` state. Empty render cells have no tile semantic state.

## Marker authority

Markers are intentionally not encoded as occupied render cells.

A spawn, transition, object anchor or other gameplay marker may exist on an empty render cell. Each marker stores its semantic id, finite kind string, resolved layer index, world-cell coordinate and finite tags once per marker.

This prevents common accidental coupling such as:

```text
"this pixel/tile is visible" => "therefore gameplay object exists"
```

Agent/test code can inspect markers semantically without screenshots or pixel inference.

## Downstream ownership

T2 is a handoff contract, not an implementation of later systems.

- #76 Physics2D owns rigid bodies, fixtures, materials, contacts and actual collider generation. `solid` is only deterministic source intent.
- #93 navigation owns pathfinding/graph/search policy. `walkable/blocked` is only deterministic source intent.
- later lighting/shadow work owns actual occluder geometry and render technique. `opaque` is only deterministic source intent.

Those systems may consume the compact semantic state during setup/import without re-parsing TileMap text or treating rendered pixels as authority.

## Validation

Compilation rejects:

- TileSet/TileMap identity mismatch,
- unknown tile rules,
- duplicate tile rules,
- unknown marker layers,
- duplicate marker ids/tags,
- marker coordinates outside the referenced layer,
- unsupported enum values or unknown authored fields.

Markers on valid empty render cells are accepted by design.

## Still open in #73

T2 deliberately leaves:

- terrain/autotiling/rule painting,
- scalable generated/large-map companion/import representation,
- any workload-driven replacement of T1's zero-chunk-metadata policy,
- animated tiles unless a representative requirement appears.

#73 remains open after T2. The next bounded slice should address deterministic terrain/rule authoring before deciding the final large-map/import closure policy.
