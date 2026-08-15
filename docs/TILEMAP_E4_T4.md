# TileMap E4 T4 — generated companion import and E4 closure

Parent: #73  
Implementation slice: #208

## Decision

Large/generated finite maps use a versioned **setup-time companion format** rather than expanding the steady-state TileMap runtime with a second representation.

```text
generator / external map tool
 -> GeneratedTileMapDocument
 -> strict deterministic validation
 -> ConvertGeneratedTileMap
 -> ordinary canonical TileMapDocument
 -> existing T0 compile / T1 presentation / T2 semantics / T3 terrain pipeline
```

The companion is an import surface. It is not gameplay or renderer authority.

## Generated companion contract

A document keeps the existing TileMap identity and coordinate rules:

- stable TileMap and TileSet semantic ids,
- explicit cell size,
- ordered layers with origin, finite dimensions and visibility,
- one document-level semantic tile table,
- one row-major signed int32 tile-table index per dense cell,
- `-1` as the only empty-cell sentinel,
- optional one-byte transform payload; omission means identity for every cell.

Transform bits intentionally match the compact T0 runtime vocabulary:

```text
bits 0..1  clockwise quarter turns [0, 3]
bit 2      flip X
bit 3      flip Y
```

Empty cells must use identity transform bits. The companion has no per-cell strings, maps or heap objects.

Canonical save sorts the semantic tile table and remaps dense indices, then sorts layers by `(order, semanticId)`. Therefore semantically equivalent input tile-table/layer ordering produces the same canonical representation and the same converted `TileMapDocument`.

## Validation and transactional conversion

Before conversion Trace2D rejects:

- unsupported format versions or unknown fields,
- missing/duplicate map, layer or tile-table identities,
- TileSet identity mismatch or unknown tile ids,
- zero/oversized dimensions,
- dense payload cardinality other than exactly `width * height`,
- transform payload cardinality other than zero or exactly `width * height`,
- tile indices outside `[-1, tile_table.size())`,
- unsupported transform bits,
- non-identity transforms on empty cells,
- layer extents that cannot be represented by the existing int32 coordinate contract.

Failure publishes no converted TileMap. Successful conversion emits only occupied sparse authored cells, in row-major order, then runs the existing `ValidateTileMap` before the result becomes available.

## Workload and memory boundary

The committed representative generated workload is one `1024 x 1024` layer:

- dense source cells: **1,048,576**,
- numeric dense index payload: **4 MiB** (`1,048,576 * sizeof(int32)`),
- example occupied authored output: **4,096** cells (one occupied cell per 256 dense entries),
- compiled T0 runtime cell storage: **8 MiB** (`1,048,576 * sizeof(CompiledTileCell)`),
- retained generated-companion/chunk metadata in normal runtime: **0 bytes** after import data is released.

These are deterministic structural quantities, not machine-timing claims. Temporary parser/document/output capacity is explicit setup/import ownership and may be released after canonical conversion/compilation.

## Final chunk-policy decision for E4

T4 does **not** add retained chunks to the current production TileMap runtime.

The decision combines two measured structural facts:

1. T1 already proves a `1024 x 1024` compiled layer with a small camera visits fewer than 400 candidate cells because presentation derives the visible integer window arithmetically; it does not scan the million-cell layer.
2. T4 proves generated dense source data can be converted once into that same existing runtime contract without retaining generator-specific state.

For the current bounded limit (`MaximumCompiledCellsPerLayer`, presently 4M cells/layer), chunks would therefore add a second retained indexing/ownership structure without improving the established steady-state frame-planning asymptotic path. Streaming/infinite worlds or workloads that exceed this finite bound remain an explicit future promotion requiring their own evidence and lifecycle contract.

## E4 authority after T4

The completed TileMap program is:

```text
T0  versioned TileSet/TileMap authoring + compact O(1) runtime cells
T1  viewport-window Sprite presentation + painter-order-preserving batching
T2  finite collision/navigation/occlusion handoff + independent markers
T3  setup-time deterministic cardinal terrain/autotile compilation
T4  deterministic generated/dense companion conversion + final chunk decision
```

Steady-state cell access/rendering still performs no generated-format parsing, semantic string lookup, filesystem access, per-cell allocation, GPU readback or per-tile draw call.

## Explicit deferrals

E4 closes without pretending to implement unrelated consumers or unsupported world models:

- actual physics bodies/contacts remain #76; TileMap only supplies collision handoff intent,
- actual navigation and lighting remain #93; TileMap already supplies their finite handoff metadata,
- animated tiles remain deferred until a representative game demonstrates need,
- diagonal/blob/Wang terrain vocabulary remains evidence-driven,
- runtime terrain mutation/rebuild scheduling remains deferred,
- streaming/infinite worlds remain a future explicit promotion rather than hidden chunk infrastructure,
- no visual tile editor is introduced.

After #208/#73 merge green, the fixed core lane advances exactly one step to #74 production UTF-8 font/text/localization.
