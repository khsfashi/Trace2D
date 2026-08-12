# Sprite SR6 — Pixel-Perfect Runtime Presentation

Status: implementation contract for issue #136 / PR #137.
Parent: #59 Complete Sprite Renderer.

SR6 is presentation policy, not asset or gameplay authority. It maps already-canonical source pixels and the already-defined SR1 pose history into deterministic logical pixels and then into an integer region of the final presentation target.

## Authority boundary

SR6 does not rewrite any earlier stage truth.

- SR0 remains authoritative for Sprite identity, atlas page/region selection, source size, trim, pivot and packed rotation.
- SR1 remains authoritative for runtime pose history and interpolation semantics.
- SR2 remains authoritative for source-space geometry and canonical UV geometry.
- SR3 remains authoritative for tint, opacity, source alpha, sampling and blend compatibility.
- SR4 remains authoritative for painter order, sorting groups and masking.
- SR5 remains authoritative for sliced/tiled primitive expansion.

Pixel-perfect snapping modifies only a derived `SpritePose2D` returned for presentation. `SpritePoseHistory2D` is supplied as `const` input and is never changed.

Future #88 Camera2D/Viewport2D work may own how a presentation view is selected, but it must feed the same resolved-view seam frozen here rather than redefining Sprite pixel truth.

## Logical viewport mapping

Inputs:

- orthographic camera,
- `logical_width`, `logical_height`,
- acquired final `target_width`, `target_height`.

The integer contain scale is:

```text
integer_scale = min(target_width / logical_width,
                    target_height / logical_height)
```

All divisions are integer divisions. There is no fractional fallback. If the result is zero, SR6 reports `target_too_small`.

Content extent:

```text
content_width  = logical_width  * integer_scale
content_height = logical_height * integer_scale
```

Centered target origin:

```text
content_x = (target_width  - content_width)  / 2
content_y = (target_height - content_height) / 2
```

For odd unused extents, integer division intentionally gives the smaller share to left/top and the remaining pixel to right/bottom. This is deterministic and machine-verifiable.

The `OrthographicView` is resolved using the logical width/height, not the final target aspect. Therefore world-to-logical-pixel mapping and GPU vertex conversion use identical view state.

## Source-pixel grid authority

The snap anchor is the transformed **untrimmed source pixel-edge origin `(0,0)`**. This is intentionally not the pivot and not the trimmed/packed rectangle.

SR2's `BuildSpriteLogicalQuad` supplies the transformed untrimmed source quad. SR6 converts three corners into logical-pixel coordinates:

- top-left: source origin,
- top-right: source X extent,
- bottom-left: source Y extent.

Dividing the two edge vectors by canonical source width/height gives one-source-pixel logical basis vectors.

Exact pixel-perfect eligibility requires:

1. each basis vector is finite,
2. exactly one component of each basis vector is zero within the frozen numeric tolerance,
3. the non-zero component is a non-zero integer within tolerance,
4. the two basis vectors occupy different logical axes.

This admits:

- 1x source pixels,
- positive integer magnification,
- semantic flips / negative axis direction,
- quarter-turn axis swaps,
- non-uniform integer X/Y magnification.

It deliberately rejects arbitrary-angle rotation, fractional source-pixel magnification and degenerate zero-scale grids instead of claiming exact pixel preservation.

## Presentation-time pose

`SpritePixelPerfectPoseRequest` explicitly chooses one of the existing SR1 presentation rules:

- `authoritative_current`: `ResolveSpriteAuthoritativeCurrent`,
- `interpolated`: `InterpolateSpritePose(history, alpha)`.

SR6 does not duplicate interpolation math.

The caller must supply a logical view resolved for the same presentation time as the camera. This makes camera/Sprite interaction explicit: common camera and Sprite translation preserves their relative logical pixel phase.

## Snap rule

Let the source origin in logical pixels be `(px, py)`.

```text
snapped_x = floor(px + 0.5)
snapped_y = floor(py + 0.5)
```

Exact half ties therefore go toward positive infinity.

This rule was selected instead of C/C++ `round` because it is invariant under integer logical translation:

```text
snap(x + n) = snap(x) + n, for integer n
```

That property matters when a camera and Sprite share an integer-pixel movement across negative/positive coordinates.

The logical delta is converted back through the same `OrthographicView` into a world-space translation and applied only to the derived presentation pose. SR6 then rebuilds the source logical quad and verifies the source origin actually landed on the intended logical pixel edge.

## Sampling boundary

SR3 still owns the canonical sampling choice.

However, an SR6 GPU submission that claims exact source-texel preservation requires `SpriteSamplerCompatibility::Nearest`. Linear sampling remains valid general Sprite presentation, but it is not accepted as the exact SR6 pixel-art path.

## SDL GPU mapping

`SpritePresentationRenderData::pixelPerfectViewport` is an optional caller-owned frame context.

Rules:

- either every Sprite presentation in the frame has no SR6 context, or every presentation provides an equal SR6 context,
- the context is copied into backend frame scratch before draw encoding,
- the acquired target dimensions must exactly match the context's target dimensions,
- SR6 uses the context's `logicalView` instead of the renderer's legacy target-aspect view for Sprite vertex conversion,
- the full-size color attachment is still cleared first,
- the GPU viewport and scissor are both set to the integer `contentRect`,
- the existing SR4 stencil target is still full target size and receives the same viewport/scissor state,
- SR5 primitive patches remain one top-level semantic item and one contiguous draw,
- there is no SR6 intermediate upscale texture,
- ordinary `RenderFrame` introduces no explicit readback or fence wait.

The caller-owned pointer is valid only for the duration of `RenderFrame` / `CaptureFrame`; the backend stores a value copy for current-frame use.

## Complexity and memory

`BuildSpritePixelPerfectViewport`, `ValidateSpritePixelPerfectViewport` and `ResolveSpritePixelPerfectPose` are:

- O(1),
- fixed-size output,
- allocation-free,
- independent of filesystem/name lookup/image decode,
- independent of renderer/GPU initialization,
- independent of GPU readback/fence waits.

The GPU path reuses the SR3-SR5 samplers, pipelines, vertex capacity and mask target. SR6 adds only fixed-size frame mapping state.

## Deterministic verification

Backend-independent tests cover:

- integer contain scale,
- matching/wider/taller targets,
- odd remainder centering,
- invalid/corrupted/too-small mappings,
- 1x/integer-magnified source basis,
- flips and quarter turns,
- fractional/arbitrary rotation rejection,
- rational pivot + trim independence from source-origin authority,
- authoritative-current vs SR1 interpolation selection,
- common camera/Sprite translation phase stability,
- authoritative history immutability,
- repeated caller-owned mapping reuse.

The explicit Windows GPU fixture `SpritePixelPerfectGpuSmokeTests.Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract` additionally proves:

- exact integer target viewport behavior,
- untouched clear bars,
- nearest source texels occupying integer target-pixel blocks,
- SR4 stencil masking under SR6 raster state,
- SR5 tiled primitive submission under SR6 raster state,
- persistent resource reuse,
- no new ordinary-frame readback/fence wait.

It follows the existing repository convention and runs only when:

```powershell
$env:TRACE2D_RUN_GPU_SMOKE = "1"
ctest --preset windows-debug -R SpritePixelPerfectGpuSmokeTests --output-on-failure
```

The owner-local presentation-GPU result is required before #136 / #137 can complete.

## Reference decisions

Reviewed for SR6 in August 2026:

- Unity 6 URP Pixel Perfect Camera — **ADAPT**: Asset Pixels Per Unit, reference resolution and render-time grid snapping concepts. Trace2D preserves its own SR0/SR1 authority and does not mutate authoritative transforms for presentation snapping.
- Godot Engine latest multiple-resolution guidance — **ADOPT/ADAPT**: base logical viewport, integer scaling and centered unused target area for pixel art. Trace2D expresses these as typed runtime facts instead of project-setting authority.
- SDL3 GPU `SDL_SetGPUViewport` / `SDL_SetGPUScissor` / render-pass clear semantics — **ADOPT**: backend raster state implements the already-resolved SR6 target mapping; SDL state is never canonical Sprite truth.

## Explicit non-goals

SR6 does not start:

- SR7 broad batching/resource policy,
- SR8 conformance breadth,
- animation,
- offline Sprite generation/processing,
- #88 Camera2D/Viewport2D ownership,
- #89 programmable materials,
- later game-production foundation work.
