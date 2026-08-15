# E6 U3 — deterministic stack layout

Parent: #75  
Implementation issue: #229

## Purpose

U0-U2 established deterministic hierarchy, fixed anchors/pivots and authored `LoadUiToml()` compilation into resolved runtime rectangles. U3 adds the smallest practical flow-layout primitive needed for HUD/menu composition without introducing a browser-style layout engine.

## Authored contract

A container keeps its normal `absolute` or `anchored_fixed` placement and may add one child-layout mode:

```toml
layout = "vertical_stack" # or "horizontal_stack"
padding = [left, top, right, bottom]
spacing = 4
```

A direct child participates in that flow only when it explicitly uses:

```toml
placement = "stack_fixed"
size = [width, height]
margin = [left, top, right, bottom] # optional, defaults to zero
```

`padding` and `spacing` are valid only on a stack container. `stack_fixed` requires a parent whose resolved container layout is a stack.

Absolute and anchored direct children are overlays. They resolve through the existing U0/U1 rules and do not consume stack cursor space.

## Ordering

Stack ordering is authored element order, not semantic-ID order.

U3 builds one contiguous direct-child index during `UiLayoutTree::Finalize()` after parent identity and hierarchy validation. This preserves child-before-parent authored documents while preventing repeated whole-tree scans.

For a horizontal stack, each participating child consumes:

```text
previous occupied end
+ spacing (except first stack item)
+ leading margin
+ fixed width
+ trailing margin
```

The vertical contract is identical on Y. Cross-axis placement starts at container padding plus the child's leading cross-axis margin. All child geometry, including margins, must fit inside the padded content area.

## Nested stacks

A `stack_fixed` child may itself be a stack container. Its fixed rectangle is resolved by its parent first; its own direct children are then resolved against that rectangle. No intrinsic-size or content-driven resize occurs in U3.

## Runtime authority

```text
authored TOML / UiLayoutNodeSpec
 -> setup-time semantic parent resolution
 -> hierarchy validation
 -> contiguous authored-order child index
 -> stack/absolute/anchor resolution
 -> resolved parent-local + absolute logical rectangles
 -> UiDocument
```

`UiDocument::UiElement::localBounds` and `bounds` remain the ordinary runtime/Agent geometry authority. There is no second runtime layout representation and no need for pixel inference to inspect layout.

## Complexity and allocation boundary

`Finalize()` is explicit setup work. Existing semantic-ID sorting/lookup remains setup-only. U3 adds O(N) child-index construction and O(N) geometry traversal after hierarchy resolution.

Normal frames do not:

- discover parent/child relationships,
- resolve semantic layout strings,
- scan siblings to rebuild stacks,
- parse TOML,
- access the filesystem,
- allocate stack nodes,
- recompute unchanged layout.

The `UiLayoutTree` scratch/index vectors are reserved/reused by the setup object. `LoadUiToml()` publishes only resolved rectangles and hierarchy indices to `UiDocument`.

## Deterministic failure semantics

Invalid container modes, `stack_fixed` without a stack parent, padding that consumes more than the container, and item/margin/spacing overflow fail before a `UiDocument` is published. Existing transactional authored-load behavior is preserved.

## Deliberate deferrals

U3 does not add:

- stretch/flex weights,
- intrinsic/content-driven sizing,
- grid layout,
- CSS/DOM-style constraints,
- pointer hit/event routing,
- clipping/scissor/scroll behavior,
- focus scopes/navigation,
- image/progress/scroll widgets.

Those remaining practical UI requirements stay under #75 and should build on the resolved hierarchy/geometry established by U0-U3.
