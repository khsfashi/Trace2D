# Sprite Batching Decision

Trace2D keeps sprite batching subordinate to deterministic painter order and measured workload evidence.

## Baseline and evidence

PR #27 established ordered multi-sprite submission with one draw per visible sprite. PR #28 fused inclusive CPU AABB culling into real GPU submission. PR #29 added the allocation-free `MeasureContiguousTextureBatching` helper so batching complexity would be justified by an executable workload rather than added speculatively.

The committed Public Alpha sample in PR #32 changed the evidence:

```text
visible sprites:             7
unbatched draw calls:        7
contiguous texture runs:     2
candidate draw-call saving:  5
```

That five-draw reduction is material enough for the first narrow batching implementation.

## Implemented batching contract

PR #34 implements contiguous same-texture GPU instancing without changing caller-provided painter order.

For each presented non-empty sprite frame, the renderer:

1. validates every supplied texture handle before visibility filtering,
2. builds the orthographic view from the acquired target size,
3. measures visible sprites / culled sprites / contiguous visible texture runs with the existing inclusive AABB rule,
4. ensures a persistent instance GPU buffer and persistent upload transfer buffer have enough capacity,
5. maps the upload buffer with SDL cycling and compacts only visible `SpriteInstanceData` transforms into it in caller order,
6. uploads the packed visible instance range once,
7. binds the persistent unit-quad and instance buffers,
8. walks the original caller span again and emits one instanced draw for each contiguous visible texture run,
9. commits draw/sprite/cull metrics only after successful command-buffer submission.

`SpriteInstanceData` is a backend-independent 16-byte structure:

```text
float2 centerClip
float2 halfClip
```

The vertex shader consumes that data from an instance-rate vertex stream. The six-vertex unit quad remains persistent.

## Painter-order invariant

The renderer never sorts by texture.

Only sprites that are already adjacent in the **visible** painter sequence and use the same texture may share one draw. A culled sprite does not split a run because it emits no presentation output.

Example:

```text
caller sequence:  tex1, tex1, [culled tex9], tex1, tex2, tex2, tex1
visible sequence: tex1, tex1,                tex1, tex2, tex2, tex1
instanced runs:   [tex1 x3]                       [tex2 x2] [tex1 x1]
```

No texture identity participates in draw-order sorting. This keeps batching an implementation detail under the existing authored/caller painter sequence.

## Allocation and resource policy

The frame path does not create a renderer-owned visible-sprite vector or run vector.

The renderer owns:

- one persistent unit-quad vertex buffer,
- one persistent instance GPU buffer,
- one persistent instance upload transfer buffer,
- the existing persistent pipeline/sampler/texture resources.

Instance capacity grows geometrically only when the measured visible count exceeds retained capacity. Replacement resources are created before old resources are released. Steady-capacity frames reuse the retained buffers and use SDL GPU cycling for in-flight safety; they do not recreate buffers.

The CPU work remains direct O(N) scans with no sorting. The explicit full-span texture validation pass is retained because invalid texture semantics must not depend on camera visibility.

## Metrics

After instancing, `RenderMetrics` intentionally separates draw count from submitted sprite count:

```text
submittedSprites delta = visible sprite instances encoded
culledSprites delta    = supplied sprites rejected by visibility
DrawCalls delta        = contiguous visible texture runs actually drawn
```

For the committed Public Alpha workload, the algorithmic contract is therefore:

```text
submittedSprites: 7
culledSprites:    0
drawCalls:        2
```

The windowed `trace2d public-alpha --json` command reports the renderer's actual successful submission metrics when a GPU presentation target is available. Hosted CI continues to validate renderer compilation and CPU contracts without requiring an interactive GPU/window.

## Why not broader batching

The measured five-draw saving does not justify a render graph, bindless architecture, global material sort, frame allocator, texture atlas system, or renderer-owned frame scene.

Future batching changes must still start from measured representative workloads and preserve visible equivalence. If a future optimization requires reordering alpha-blended sprites, it needs an explicit correctness proof rather than silently weakening painter order.

## Official SDL3 basis

This implementation uses SDL GPU's documented instance-rate vertex input, `SDL_DrawGPUPrimitives` instance ranges, reusable transfer buffers, and cycled uploads. `instance_step_rate` remains `0` as required by SDL3.

References:

- https://wiki.libsdl.org/SDL3/SDL_GPUVertexBufferDescription
- https://wiki.libsdl.org/SDL3/SDL_DrawGPUPrimitives
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_MapGPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_UploadToGPUBuffer
- https://wiki.libsdl.org/SDL3/CategoryGPU
