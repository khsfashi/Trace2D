# Camera2D and Viewport2D C0

Issue #88 turns the renderer's orthographic math into an engine-level camera/presentation contract without making presentation state authoritative gameplay state.

## Authoritative Camera2D state

`trace2d.camera2d` is an authored typed Scene component with schema version 1.

| Field | Type | Rule |
| --- | --- | --- |
| `enabled` | bool | Only enabled cameras participate in selection. |
| `priority` | signed integer | Canonical deterministic selection key. Higher wins. |
| `vertical_size` | float | Finite and greater than zero. This is the single authoritative orthographic size/zoom truth. |
| `target_viewport` | text | Non-empty stable viewport semantic identity. |

Camera position is the owning entity's resolved world position. C0 does not add a second camera-position store.

Rotation, engine-owned follow/smoothing/bounds and shake are deliberately deferred. They are optional #88 scope and would otherwise force wider legacy Sprite/particle GPU ABI changes before a concrete requirement exists.

## Active-camera selection

`scene::ResolveCameraSelection2D` is the single selection authority.

For one viewport:

1. consider enabled `Camera2D` components whose `target_viewport` matches,
2. highest `priority` wins,
3. equal priority uses lexicographically smallest entity semantic ID,
4. equal/empty semantic IDs fall back to generation-safe `EntityId` index/generation.

No unordered-container order, pointer address or allocation address participates.

Selection is an explicit invalidation-time operation, not a required per-frame scan. Re-run it after camera structural changes or edits to `enabled`, `priority` or `target_viewport`, then cache the selected generation-safe entity identity. `ResolveCameraFrameState2D` uses that cached identity and typed component access without semantic string lookup.

No matching camera returns `no_active_camera`; a previous camera is never silently reused. Despawning a selected camera makes its cached identity stale instead of aliasing a future entity generation.

## Viewport2D state

`Viewport2D` owns stable logical presentation intent:

- semantic ID,
- logical width/height,
- scaling mode.

OS/window or capture target dimensions are inputs to `ResolveViewport2D`; they do not mutate logical dimensions. A resolved viewport freezes the target width/height and content rectangle used for that presentation state.

Scaling modes:

- `fit`: uniform scale using the smaller axis; centered letterbox/pillarbox,
- `fill`: uniform scale using the larger axis; centered crop overflow,
- `stretch`: independent X/Y scale to fill the target.

A resolved renderer camera also freezes the target dimensions. Reusing it against different target dimensions fails instead of applying stale resize coefficients.

## Coordinate convention

The C0 CPU conversion chain uses a continuous pixel-edge convention:

```text
world <-> logical viewport <-> presentation target
```

Logical viewport `(0, 0)` is the top-left edge and `(logicalWidth, logicalHeight)` is the bottom-right edge. Presentation coordinates use the same top-left edge convention against the target.

Public conversions are:

```text
WorldToViewport
ViewportToWorld
ViewportToPresentation
PresentationToViewport
WorldToPresentation
PresentationToWorld
```

Callers first obtain a successful `ResolveViewport2D` and `ResolvePresentationView2D`. Forward conversions then operate on that frozen state in O(1). Inverse conversions return `CoordinateConversionResult2D`: invalid/non-invertible resolved state reports `invalid_resolved_view`, while NaN/Inf input reports `non_finite_input`. No inverse path divides through invalid coefficients silently. These numeric conversions require no GPU/window access and perform no heap allocation.

#59 SR6 pixel-perfect Sprite presentation owns the narrower integer pixel-center/raster precision guarantees; C0 does not duplicate them.

## Authoritative current vs interpolation

`ResolvePresentationView2D` has two explicit modes.

`AuthoritativeCurrent` uses current fixed Camera2D state directly. This is the default for exact-frame/headless capture and ignores wall-clock remainder.

`Interpolated` requires:

- previous and current samples for the same generation-safe camera entity,
- an explicit finite alpha in `[0, 1]`.

Position and `vertical_size` are interpolated into derived presentation state only. Neither authoritative sample is mutated.

## Renderer integration

The resolved CPU clip view remains the logical viewport projection. The Renderer applies fit/fill/stretch once per render pass through `PresentationRasterViewport2D` / SDL GPU viewport state.

Consequences:

- culling, legacy Sprite, production Sprite and particles consume the same world-to-clip coefficients,
- there is no per-sprite Camera2D lookup or allocation,
- fit geometry is clipped in logical clip space before viewport mapping, so it cannot leak into letterbox/pillarbox bars,
- the existing full offscreen target is cleared first, so uncovered fit bars retain the configured clear color,
- fill naturally maps a viewport larger than the target and the target clips overflow,
- resize-only offscreen resources continue to reuse the existing Renderer size cache.

SR6 pixel-perfect presentation remains a narrower raster contract. When SR6 owns its viewport/scissor state, the generic C0 raster viewport does not overwrite it.

## Agent inspection

`agent::InspectActiveCameraViewport2D` resolves the same Scene selection authority and returns:

- requested viewport semantic ID,
- selected camera entity semantic ID plus generation-safe handle fields,
- priority/enabled/target viewport,
- authoritative `vertical_size`,
- resolved authoritative world center.

No pixel readback or GPU is required. String copies are confined to the explicit inspection result, not the frame hot path.

## Complexity and allocation contract

- active-camera reselection: O(entity count), explicit invalidation/setup work,
- cached camera frame-state resolution: O(hierarchy depth), no semantic string lookup,
- viewport/presentation resolve: O(1), no heap allocation in the numeric resolver,
- world/viewport/presentation conversion: O(1), allocation-free,
- renderer camera application: once per render pass,
- per-object projection/culling: existing resolved numeric view only.

No tracing GC or new per-frame camera object graph is introduced.
