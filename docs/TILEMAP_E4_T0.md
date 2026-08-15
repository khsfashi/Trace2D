# TileMap E4 T0 — authored contract and compact semantic runtime

Parent: #73  
Implementation slice: #200

## Scope

T0 establishes the deterministic authoring/runtime boundary needed before renderer and chunk policy work begins.

```text
versioned TileSet TOML
 + versioned TileMap / ordered TileLayer TOML
 -> strict deterministic validation
 -> canonical serialization
 -> setup-time semantic resolution
 -> compact compiled TileSet + TileMap
 -> O(1) resolved layer/cell reads
 -> explicit semantic inspection
```

T0 has no renderer or GPU dependency. It does not close #73.

## Authored TileSet

A TileSet has one stable semantic id, one project-relative source texture reference, source dimensions, and finite tile definitions.

```toml
format_version = 1
id = "overworld"
texture = "textures/overworld.png"
source_size = [64, 32]

[[tiles]]
id = "grass"
region = [0, 0, 16, 16]
tags = ["ground", "walkable"]
```

Tile ids are unique. Regions must be positive and completely contained by `source_size`. Tags are finite setup/inspection metadata stored once per tile definition; they are not copied into cells.

Canonical serialization sorts tiles by semantic id and tags lexicographically.

## Authored TileMap and coordinates

A TileMap references the TileSet by semantic id, owns one map-wide positive cell size, and contains ordered layers.

```toml
format_version = 1
id = "room_a"
tile_set = "overworld"
cell_size = [16, 16]

[[layers]]
id = "ground"
order = 0
origin = [-2, 3]
size = [4, 3]
visible = true

[[layers.cells]]
x = 1
y = 1
tile = "grass"
flip_x = false
flip_y = false
rotation_quarters = 0
```

Cell `x/y` are layer-local integer coordinates. The world-cell coordinate is:

```text
worldCell = layer.origin + localCell
```

T0 uses the existing top-left-oriented 2D convention: increasing X moves right and increasing Y moves down. Rendering later converts world-cell coordinates through the already-established Sprite/Camera presentation contracts instead of introducing another coordinate authority.

Layer painter order is canonical `(order, semanticId)`. Sparse authored cells are canonical `(y, x, tileSemanticId)`. `rotation_quarters` is exactly 0, 1, 2, or 3; flips are explicit booleans. The transform vocabulary is finite so renderer work can consume it without text interpretation.

## Compiled runtime representation

Authored strings are resolved during explicit compilation.

`CompiledTileCell` is a trivially-copyable 8-byte value:

```text
uint32 tileIndex     // UINT32_MAX = empty
uint8  transformBits // 2 rotation bits + flip X/Y bits
3 bytes reserved
```

A compiled layer owns one contiguous row-major `vector<CompiledTileCell>`. It contains no string, map, vector, pointer, shared ownership, or heap object per cell.

The hot read path is:

```text
resolved layer index + world-cell coordinate
 -> subtract layer origin
 -> bounds check
 -> row-major offset
 -> CompiledTileCell
```

This is O(1), allocation-free, and performs no filesystem or semantic string lookup.

Tile and layer semantic-id lookup remains an explicit setup/inspection operation and is intentionally linear in T0. A retained hash/index is not justified until a measured workload shows that semantic lookup itself is hot. Compilation may use bounded temporary hash tables to resolve sparse authored tile ids once; those tables are not retained as a second runtime model.

## Safety bound versus future chunk policy

T0 rejects a layer whose dense compiled cell count exceeds `4 * 1024 * 1024` cells. This is a setup-time safety bound preventing a tiny sparse text file from requesting unbounded dense allocation.

It is **not** the final #73 world-size/chunking performance claim. The renderer/workload slice must measure representative maps and choose the actual chunk/culling/import policy from evidence. That later work may replace the dense T0 storage boundary without changing authored semantic meaning.

## Semantic inspection

`InspectCell` resolves a layer semantic id only on explicit inspection, then returns a non-owning view of:

- occupied/empty state,
- layer identity/index,
- world-cell coordinate,
- tile semantic id,
- source atlas region,
- finite transform,
- tile tags.

No screenshot or pixel inference is required to answer exact tile-state questions.

## File/store boundary

`TileDocumentStore` accepts project-relative authored document references only. Empty, absolute/drive-prefixed, and parent-traversing references are rejected. Source reads are bounded to 8 MiB.

Save validates first, writes a sibling temporary, then replaces at this explicit setup boundary, matching the existing authored-input pattern. #79 remains the owner of the final general persistence/migration/crash-recovery policy.

## Deliberate T0 deferrals

The following remain within #73 but are not silently implied by T0:

- renderer submission, culling, batching, and measured chunk policy,
- terrain/autotiling/rule painting,
- gameplay/object/spawn markers separate from render cells,
- collision/physics metadata handoff,
- navigation and lighting/occlusion handoff schemas,
- generated dense/large-map companion/import conversion,
- animated tiles unless a representative workload justifies them.

The next #73 slice should use a committed representative map workload to choose renderer/chunk behavior while preserving the T0 semantic authority and the rule that there is no per-tile object/string/draw-call path.
