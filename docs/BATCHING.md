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

## Implemented legacy batching contract

PR #34 implements contiguous same-texture GPU instancing for the legacy `SpriteRenderData` path without changing caller-provided painter order.

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

## SR7 production Sprite batching

The SR4-SR6 `SpritePresentationRenderData` path cannot reuse the legacy texture-only key because production presentation also carries resolved sampler, blend, masking, variable primitive patch counts, pixel-perfect view state, and a future Material2D/Shader2D compatibility seam.

SR7 therefore adds a separate production contract, documented in `docs/SPRITE_BATCHING_SR7.md`.

Its compatibility key is:

```text
texture/resource identity
+ material/pipeline identity
+ sampler compatibility
+ blend compatibility
+ exact mask mode/id
```

Tint and opacity are not key fields. SR7 stores their resolved derived values in the transient expanded Sprite vertex payload, so otherwise-compatible adjacent Sprites may batch even when appearance differs.

The production path first resolves the complete SR4 painter/mask sequence, then evaluates visibility against the exact presentation view, compacts visible quad/primitive vertices in that resolved order, and emits one GPU draw per contiguous compatible visible run. A fully culled or zero-output Sprite does not split a run.

SR7 does **not** globally sort by any resource/material key. #89 remains the owner of programmable material execution; SR7 only establishes the non-zero built-in material/pipeline identity required for future compatibility-safe batching.

## Painter-order invariant

The renderer never sorts by texture or material.

Only sprites that are already adjacent in the **visible** painter sequence and have a compatible batch key may share one draw. A culled sprite does not split a run because it emits no presentation output.

Legacy example:

```text
caller sequence:  tex1, tex1, [culled tex9], tex1, tex2, tex2, tex1
visible sequence: tex1, tex1,                tex1, tex2, tex2, tex1
instanced runs:   [tex1 x3]                       [tex2 x2] [tex1 x1]
```

Production SR7 applies the same invariant after SR4 has resolved its semantic painter sequence; the compatibility comparison is simply broader than texture identity.

## Allocation and resource policy

Neither Sprite path creates a renderer-owned visible-sprite scene or a globally sorted batch list.

The legacy renderer owns:

- one persistent unit-quad vertex buffer,
- one persistent instance GPU buffer,
- one persistent instance upload transfer buffer,
- the existing persistent pipeline/sampler/texture resources.

The SR7 production backend owns and retains:

- nearest/linear samplers,
- built-in graphics pipelines,
- one geometrically growing expanded-Sprite vertex GPU buffer,
- one matching upload transfer buffer,
- the stencil target while dimensions remain compatible,
- CPU scratch vectors whose capacity is retained across frames.

GPU upload capacity grows geometrically only when the visible high-water mark exceeds retained capacity. Equal or smaller workloads reuse it. Steady-state uploads use SDL GPU cycling rather than adding ordinary-frame explicit fence waits.

The new SR7 scan work is O(N + Q) after the existing SR4 semantic ordering step, where N is top-level Sprite count and Q is emitted quad count for visible top-level Sprites. No additional resource-based sorting pass is added.

## Metrics

Legacy `RenderMetrics` separates draw count from visible encoded Sprite count:

```text
submittedSprites delta = visible sprite instances encoded
culledSprites delta    = supplied sprites rejected by visibility
DrawCalls delta        = contiguous visible runs actually drawn
```

For production SR7, dedicated cumulative metrics additionally distinguish semantic submissions from GPU work:

```text
spritePresentationSprites
spritePresentationVisibleSprites
spritePresentationCulledSprites
spritePresentationDrawCalls
spritePresentationCompatibilityRuns
spritePresentationUploadedQuads
spritePresentationUploadedVertexBytes
```

Retained state is also observable through sampler/pipeline creation counts, Sprite vertex quad-slot/byte capacity, and mask-target creation count. Ordinary-frame explicit readback/fence counters remain visible so an Agent can detect an accidental synchronization regression.

## Why not broader batching

The measured benefit does not justify a render graph, bindless architecture, global material sort, frame allocator, texture atlas system, or renderer-owned frame scene.

Future batching changes must still start from measured representative workloads and preserve visible equivalence. If a future optimization requires reordering alpha-blended sprites, it needs an explicit correctness proof rather than silently weakening painter order.

## Official SDL3 basis

These paths use SDL GPU's documented reusable GPU/transfer resources, cycled upload semantics, explicit vertex ranges, and sampler/pipeline binding model. The legacy path additionally uses instance-rate vertex input.

References:

- https://wiki.libsdl.org/SDL3/SDL_GPUVertexBufferDescription
- https://wiki.libsdl.org/SDL3/SDL_DrawGPUPrimitives
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_MapGPUTransferBuffer
- https://wiki.libsdl.org/SDL3/SDL_UploadToGPUBuffer
- https://wiki.libsdl.org/SDL3/CategoryGPU
