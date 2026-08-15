# UI E6 U10 — explicit child content extent

U10 adds the protocol-independent layout primitive required before ordinary authored UI can express content larger than a visible scroll viewport.

## Authority

`UiLayoutTree` remains the only hierarchy/layout resolver. A node may now provide `childContentWidth` and `childContentHeight` as a pair. The pair affects only how that node's direct children are resolved; the node's own `resolvedLocalBounds` and absolute `bounds` remain unchanged.

```text
visible parent bounds
  -> optional larger child content/reference extent
  -> absolute / anchored / stack-fixed direct-child layout
  -> ordinary absolute logical child bounds
```

When both content dimensions are zero, child layout uses the parent's resolved visible width/height exactly as before.

## Validation

An explicit content extent is setup-only state and must:

- provide both non-zero dimensions,
- stay at or below `MaxUiCanvasDimension`,
- be at least as large as the parent's resolved visible size,
- fit completely inside the logical UI canvas from the parent's resolved absolute origin.

The final rule intentionally preserves the existing unsigned logical `UiRect` and `UiDocument::AddElement()` publication contract. U10 does not introduce negative logical layout coordinates or a second presentation model.

## Placement semantics

For a parent with an explicit content extent:

- absolute children keep their authored parent-local rectangle but are contained against the content extent,
- anchored children resolve normalized anchor coordinates against the content width/height,
- horizontal/vertical stacks use the content width/height for padding, spacing, flow and overflow checks,
- child absolute bounds are still derived from the visible parent's absolute origin plus the resolved content-local child origin.

Nested containers may themselves provide a content extent. This is a layout capability only; U9's runtime scroll authority still rejects unsupported nested scroll ownership when scroll viewports are configured.

## Performance boundary

All content-extent work happens during explicit `UiLayoutTree::Finalize()` setup. The existing prepared parent indices, contiguous child offsets/indices and breadth-first resolution remain in use.

Normal frames gain no:

- hierarchy discovery,
- semantic string lookup,
- allocation,
- filesystem/TOML work,
- layout recomputation,
- renderer/GPU synchronization,
- runtime scroll state.

## Next #75 slice

U10 deliberately does not add TOML syntax. The next E6 slice can now add `scroll_content_size = [width, height]` (or the final reviewed authored name), feed it into this content extent, and transactionally call the already-merged U9 `UiDocument::ConfigureScrollViewport()` authority after layout publication.

That keeps authored syntax, deterministic layout and runtime scrolling as three layers converging on one retained UI document rather than creating a scroll-specific layout implementation.
