# E6 U1 — deterministic anchored UI placement

Parent: #75  
Child: #224

U1 extends the U0 hierarchy foundation with a bounded fixed-size anchor/pivot placement mode while keeping logical UI layout independent from the OS presentation target.

## Placement authority

Every node selects exactly one placement mode.

### Absolute

The existing U0 contract remains the default:

```text
parent-local UiRect
  -> Finalize()
  -> resolved parent-local rect
  -> logical-canvas absolute rect
```

Existing callers that only provide `localBounds` keep the same behavior.

### AnchoredFixed

An anchored node provides:

```text
anchor point   : normalized fixed integer x/y
pivot point    : normalized fixed integer x/y
signed offset  : logical pixels
fixed size     : logical pixels
```

The normalized range is exactly `0..1024`:

- `0` = leading edge,
- `512` = center,
- `1024` = trailing edge.

Root nodes resolve their anchor against the logical UI canvas. Child nodes resolve against the immediate resolved parent's width/height. The resulting parent-local rectangle must still fit completely inside the canvas/parent.

A typical top-right HUD node is therefore:

```text
anchor = (1024, 0)
pivot  = (1024, 0)
offset = (-12, 8)
size   = (80, 20)
```

No floating normalized value is stored or evaluated.

## Exact integer rounding

Normalized placement uses round-half-up integer arithmetic:

```text
resolved = (extent * normalized + 512) / 1024
```

The multiplication is performed with 64-bit intermediate storage. Anchor origin arithmetic is performed with signed 64-bit intermediates before conversion to `UiRect`.

This freezes center behavior for odd extents and avoids platform-dependent float rounding in deterministic layout truth.

## Resolved inspection state

A finalized `UiResolvedLayoutNode` retains:

- semantic id / parent id,
- direct resolved `parentIndex`,
- hierarchy depth,
- authored absolute `localBounds` for the legacy mode,
- authored placement mode,
- authored anchored placement state,
- `resolvedLocalBounds`,
- absolute logical-canvas `bounds`.

For `Absolute`, `resolvedLocalBounds == localBounds`. For `AnchoredFixed`, the authored anchor/pivot/offset/size remains inspectable and `resolvedLocalBounds` contains the computed rectangle used by downstream layout/input work.

## #88 Viewport2D contract

UI layout coordinates are **logical viewport coordinates**.

The `UiLayoutTree(width, height)` canvas is expected to use the same logical dimensions as the owning #88 `Viewport2D.logicalWidth/logicalHeight`.

Changing only the OS/window/presentation target size:

- does not mutate authored UI placement,
- does not rerun or rescale logical layout,
- does not create a second UI-specific scale mode,
- does not change finalized logical `bounds`.

#88 `ResolveViewport2D` remains the authority that maps logical viewport coordinates to presentation coordinates for Fit/Fill/Stretch behavior. Later #75 pointer routing must use the inverse #88 presentation-to-viewport mapping before deterministic UI hit testing.

## Deterministic rejection

U1 rejects:

- unsupported placement mode values,
- anchor/pivot components outside `0..1024`,
- zero-area fixed anchored sizes,
- any anchored origin that resolves outside unsigned logical coordinate space,
- root rectangles escaping the logical canvas,
- child rectangles escaping the immediate parent.

Existing U0 duplicate/unknown/self/cycle/canvas validation remains unchanged.

## Performance boundary

Anchor and pivot evaluation happens only during explicit `Finalize()`.

After finalization:

- nodes remain contiguous,
- parent relationships remain direct indices,
- resolved parent-local and absolute rectangles are retained,
- normal frame code performs no anchor float math,
- no semantic string lookup is required for hierarchy traversal,
- no filesystem, allocation, JSON/reporting, text rasterization or GPU work is introduced.

Target/window resizing changes #88 presentation mapping only; it does not force logical UI tree re-finalization.

## Deliberate deferrals

U1 does not close #75. Later bounded slices still own:

- authored hierarchy/layout TOML integration,
- stretch anchors if representative UI proves they are required,
- horizontal/vertical stacks plus margin/padding,
- focus traversal/navigation,
- pointer hit testing, hover/press/release, capture and routing,
- clipping/scroll,
- image/progress and demonstrated practical widgets,
- completed semantic Agent inspection over the authored runtime UI path.
