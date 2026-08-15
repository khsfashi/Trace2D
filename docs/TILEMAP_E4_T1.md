# TileMap E4 T1 — viewport-window presentation and measured chunk policy

Parent: #73  
Implementation slice: #202

## Decision

T1 does not add a second tile GPU renderer. A compiled TileMap converts visible occupied cells into the existing production `SpritePresentationRenderData` contract, then SR7 remains the only painter-order, visibility, contiguous-batching and GPU submission authority.

```text
Compiled TileSet / TileMap
 + #86 canonical TextureHandle + TextureResource metadata
 + resolved OrthographicView
 -> arithmetic visible-cell window per visible layer
 -> exact occupied-cell quad/UV validation
 -> caller-owned SpritePresentationRenderData span
 -> existing Sprite SR7 renderer
```

This keeps tile semantics in `Trace2D::Tile` while the renderer remains ignorant of TileMap storage.

## Resource boundary

`ResolveTileTextureBinding2D` is setup work. It resolves the generation-safe #86 texture handle, verifies that its canonical resource identity matches the TileSet texture reference, verifies source dimensions, maps sRGB/linear encoding, and rejects premultiplied canonical alpha because the frozen built-in Sprite pipeline currently accepts straight-alpha source truth.

The steady-state `TileTextureBinding2D` contains only fixed-size renderer metadata and a generation-safe texture handle. It contains no path or owning container.

## Geometry and transforms

`cell_size / pixels_per_unit` defines one cell's world extent. Cell `(x, y)` starts at:

```text
(layer.origin + localCell) * cellWorldSize
```

The T0 convention remains unchanged: +X right, +Y down. The cell rectangle stays fixed while authored flip/quarter-turn state remaps atlas UV corners. Transform order is authored flips followed by clockwise quarter turns; presentation computes the inverse destination-to-source mapping so semantic cell state is never mutated.

Atlas UVs remain pixel-edge normalized coordinates. Sampling clamps separately to texel-center bounds derived from the exact tile region, matching the existing Sprite appearance boundary.

## Painter order

Every TileMap item uses one caller-selected top-level Sprite painter layer. T0 `TileLayer.order` becomes Sprite `order`; stable order is `(canonical layer index << 32) | row-major cell index`.

Texture/material identity is not part of semantic order. The existing SR7 renderer may merge only contiguous compatible work after semantic ordering, so one atlas can become one compatible run without global resource sorting.

## Culling and T1 chunk policy

The current T0 runtime is bounded dense row-major storage. T1 measures a direct arithmetic viewport policy before adding chunk metadata:

1. intersect the camera world rectangle with each visible layer rectangle,
2. derive a conservative local integer cell window,
3. expand by one cell around floating-point boundaries,
4. iterate only that clamped window,
5. run exact quad visibility for occupied candidates,
6. never scan offscreen rows/columns.

The committed representative workload is a `1024 x 1024` dense layer viewed through a small camera window. Acceptance requires fewer than 400 candidate-cell visits out of 1,048,576 cells and one compatible Sprite batch run for the one-atlas result. This is deterministic structural evidence rather than unstable wall-clock timing.

Therefore the T1 baseline deliberately retains **zero chunk metadata bytes**. For this bounded representation, retained chunks would add memory/maintenance state without reducing the asymptotic frame-planning work below the already visible-window-proportional scan.

This is not a claim that streaming or generated multi-million-cell worlds need no chunked storage. The later #73 large-map/import slice owns that decision with its own workload evidence.

## Allocation and failure behavior

Presentation is two-pass and transactional:

- pass one validates/counts visible occupied output without writing,
- insufficient caller capacity returns the exact required count and publishes no partial frame,
- pass two writes into caller-owned storage,
- no allocation, filesystem access, semantic lookup, GPU readback or retained per-cell object is required in either steady-state pass.

## Still open in #73

T1 deliberately leaves these production authoring/handoff questions for later bounded slices:

- terrain/autotiling/rule painting,
- gameplay/object/spawn markers separate from render-cell truth,
- collision metadata handoff to #76,
- navigation and lighting/occlusion handoff,
- scalable generated/large-map import representation and any evidence-driven chunked storage,
- animated tiles unless a representative requirement justifies them.

#73 remains the active core-lane item after T1.
