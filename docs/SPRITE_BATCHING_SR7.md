# Sprite SR7 — Production Batching and Hot-Path Policy

Status: **active / implementation gate**  
Parent: #59  
Child: #138  
Draft PR: #139

SR7 optimizes the SR4-SR6 production Sprite presentation path without changing its semantic authority. Painter order, masking, canonical Sprite truth, SR5 primitive ownership, and SR6 pixel-perfect presentation remain contracts above batching.

## 1. Semantic authority

The renderer MUST resolve the full SR4 order/mask sequence before any SR7 culling or batching decision.

Resource identity MUST NOT participate in sorting. Texture, material/pipeline, sampler, blend, mask state, resource reuse, or estimated batch benefit cannot move a Sprite earlier or later in painter order.

Only compatible **contiguous visible work in resolved painter order** may merge. A fully culled or zero-output top-level Sprite emits no pixels and therefore does not split an otherwise-compatible visible run.

SR5 primitive patches remain atomic under their one top-level Sprite item. Their patch order is preserved and cannot interleave with another Sprite.

## 2. Compatibility key

`SpriteBatchCompatibility2D` is the backend-independent run key:

- texture/resource identity,
- resolved material/pipeline identity,
- sampler compatibility,
- blend compatibility,
- exact SR4 mask mode and mask id.

`SpriteMaterialPipelineIdentity` reserves `0` as invalid. SR7 executes only `BuiltInSpriteMaterialPipelineIdentity == 1`; programmable Material2D/Shader2D remains owned by #89.

Tint, opacity, geometry, painter layer/order, and sorting-group identity are intentionally not batch-key fields. Painter state is already resolved before batching. Tint and opacity are transient per-vertex derived data in the built-in SR7 backend, so two otherwise-compatible Sprites do not split solely because their appearance differs.

## 3. Visibility

Visibility is evaluated against the exact presentation view used by the GPU path:

- SR6 logical view when a pixel-perfect viewport is active,
- otherwise the target orthographic view.

Clip-space intersection is inclusive. A regular Sprite is visible when its resolved quad intersects the clip viewport. An SR5 primitive Sprite is visible when at least one emitted patch intersects it. A zero-patch primitive emits no upload or draw work.

Full SR4 semantic validation still occurs before this visibility pass. Culling cannot make malformed order/mask input valid.

## 4. Upload construction

After SR4 order resolution and visibility:

1. walk the resolved painter sequence once,
2. compact visible top-level Sprite quad/patch vertices into retained upload storage in that same order,
3. record source-index keyed compact offsets,
4. derive contiguous compatibility runs over that visible sequence,
5. upload only the compacted visible vertex payload,
6. issue one triangle-list draw from the first source item of each non-empty run.

No renderer-owned scene, global material sort, bindless table, render graph, or extra presentation-order structure is introduced.

Complexity is O(N log N + Q): SR4 retains its existing semantic order sort; the new SR7 visibility, compaction, and run derivation are O(N + Q), where N is top-level Sprite count and Q is emitted quad count for visible top-level Sprites.

## 5. Per-vertex appearance derivation

The SR7 built-in GPU vertex payload contains:

- clip position,
- UV,
- atlas-safe sample bounds,
- linear tint RGB,
- `tintAlpha * opacity`.

The fragment stage reconstructs the same SR3 premultiplied boundary:

`effectiveAlpha = sampledStraightAlpha * tintAlpha * opacity`

`premultipliedRgb = sampledStraightRgb * tintRgb * effectiveAlpha`

This is derived renderer state only. Canonical SR3 appearance truth is unchanged.

## 6. Persistent-resource policy

The Sprite backend retains and reuses:

- nearest and linear samplers,
- built-in graphics pipelines,
- vertex GPU buffer,
- upload transfer buffer,
- stencil target when dimensions remain compatible,
- CPU order/offset/run scratch vector capacity.

Vertex GPU/transfer capacity grows geometrically only when the visible quad high-water mark exceeds retained capacity. Equal or smaller workloads reuse capacity.

Ordinary `RenderFrame` does not add explicit GPU readback or fence waits for SR7 upload reuse. Capture remains the explicit readback/fence boundary.

## 7. Metrics

`RenderMetrics` distinguishes semantic submissions from actual GPU work.

Cumulative production Sprite metrics:

- `spritePresentationSprites`: top-level semantic submissions on successfully encoded presentation frames,
- `spritePresentationVisibleSprites`,
- `spritePresentationCulledSprites`,
- `spritePresentationDrawCalls`: actual compatibility-run GPU draws,
- `spritePresentationCompatibilityRuns`,
- `spritePresentationUploadedQuads`,
- `spritePresentationUploadedVertexBytes`.

Current retained backend state:

- `spriteSamplerCreations`,
- `spritePipelineCreations`,
- `spriteVertexCapacitySprites` (quad slots),
- `spriteVertexCapacityBytes`,
- `spriteMaskTargetCreations`.

`drawCalls` remains actual encoded GPU draw work. `submittedSprites` remains visible encoded Sprite work across legacy and production paths. Existing explicit readback/fence counters remain observable.

## 8. Deterministic tests

Hosted/backend-independent coverage MUST prove:

- compatible visible items merge,
- a culled/zero-output gap does not split a run,
- texture/material/sampler/blend/mask differences split runs,
- inclusive resolved-view culling,
- primitive visibility is any-patch visibility,
- visible quad totals remain deterministic.

The opt-in Windows presentation-GPU fixture `SpriteBatchGpuSmokeTests.Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse` MUST additionally prove:

- distinct tint/opacity Sprites batch in one actual draw,
- one incompatible texture transition creates exactly one additional draw,
- a culled Sprite between compatible visible neighbors creates no additional draw,
- captured pixels preserve per-Sprite appearance,
- SR4 stencil masking still works,
- SR5 tiled primitive patches still render atomically,
- SR6 integer viewport/scissor and nearest presentation still work,
- sampler/pipeline/vertex capacity is reused across ordinary repeated frames,
- ordinary repeated frames add no explicit GPU readback or fence wait.

The fixture is opt-in under the established `TRACE2D_RUN_GPU_SMOKE=1` convention. Hosted CPU CI may skip it; owner-local Windows GPU evidence is required before #138 can close and PR #139 can merge.

## 9. Explicit non-goals

SR7 does not begin:

- SR8 conformance breadth,
- Sprite animation,
- offline Sprite processing/generation,
- #89 programmable Material2D/Shader2D,
- #88 Camera2D/Viewport2D ownership,
- bindless rendering,
- render graph ownership,
- ECS/reflection,
- a custom frame allocator,
- resource-based global painter sorting.
