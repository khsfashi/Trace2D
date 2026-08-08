# Sprite Batching Decision

Trace2D keeps sprite batching subordinate to deterministic painter order and measured workload evidence.

## Current baseline

PR #27 established ordered multi-sprite submission with one draw per supplied sprite. PR #28 then fused inclusive CPU AABB culling into the real GPU submission loop.

For a successfully presented frame before batching:

```text
visible sprites = submittedSprites delta = drawCalls delta
culled sprites  = culledSprites delta
```

The renderer does not sort, copy, or materialize a visible-sprite list. Caller-provided order remains authoritative for presentation.

## Measurement contract

`MeasureContiguousTextureBatching` is an allocation-free CPU helper that measures the narrowest batching model currently considered safe:

- walk sprites in caller order
- apply the same `IsSpriteVisible` rule used by renderer submission
- ignore culled sprites because they emit no GPU work
- count visible sprites
- count culled sprites
- count contiguous visible texture runs
- never sort by texture

For a future contiguous same-texture instancing path, `contiguousTextureRuns` is the candidate draw-call count while `visibleSprites` remains the candidate submitted-sprite count.

Example:

```text
visible texture sequence: 1, 1, 2, 2, 1
unbatched draw calls:      5
candidate batch runs:      3
```

A culled sprite does not split a run because it produces no presentation output.

## Why not texture-sort

Trace2D uses alpha blending and preserves caller-provided painter order. Sorting by texture could change visible results when sprites overlap, so texture identity is not allowed to participate in draw ordering.

The first batching implementation, when justified, should therefore batch only adjacent visible sprites that already share a texture.

## SDL3 GPU implementation candidate

The least-complex GPU candidate is instanced drawing over contiguous same-texture runs:

1. keep the persistent six-vertex unit quad
2. add a persistent vertex/instance buffer sized from an explicit renderer capacity
3. reuse a persistent upload transfer buffer rather than creating one per frame
4. compact visible clip-space transforms into instance storage in caller order
5. bind texture/sampler once per contiguous run
6. issue one instanced draw per run using the corresponding first-instance/count range
7. keep `submittedSprites` equal to visible instance count and `drawCalls` equal to actual encoded run draws

SDL3 documents transfer-buffer reuse as preferable for repeated downloads/uploads, and its GPU buffer upload path supports cycling resources instead of recreating them every frame. The implementation must use those reuse/cycling semantics rather than adding frame-time resource creation.

Official SDL3 references:

- https://wiki.libsdl.org/SDL3/CategoryGPU
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_UploadToGPUBuffer
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUBuffer

## Public Alpha decision

The current windowed Trace2D sample submits one visible sprite. For that workload:

```text
unbatched draw calls: 1
contiguous texture runs: 1
measured reduction: 0
```

Adding persistent instance-buffer capacity, upload synchronization, and a second vertex stream cannot currently demonstrate a draw-call reduction in the repository's executable sample.

Therefore the actual instancing change is intentionally deferred until the Public Alpha vertical sample contains a representative multi-sprite workload that measures a reduction. This follows the project invariant that optimization complexity must follow measurement.

The next P5 release-blocking work is the offscreen/readback path and deterministic capture at an explicitly requested simulation frame. Once the vertical sample exists, rerun this measurement and add contiguous same-texture instancing if it produces a material draw-call reduction.
