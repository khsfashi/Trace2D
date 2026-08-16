# E6 U11 — Authored scroll integration

U11 connects ordinary UI TOML to the already-merged U10 child-content layout authority and U9 runtime scroll authority. It does not introduce a second scroll layout model or a new element kind.

## Authored contract

A `panel` may declare one optional content extent:

```toml
[[elements]]
id = "inventory_scroll"
kind = "panel"
bounds = [24, 32, 240, 120]
scroll_content_size = [240, 360]

[[elements]]
id = "last_item"
kind = "button"
parent = "inventory_scroll"
bounds = [8, 300, 160, 32]
text = "Last item"
```

`scroll_content_size = [width, height]` is valid only for `kind = "panel"`. Both values must be positive. The existing U10 layout validation remains authoritative for minimum viewport size, the 4096 logical limit, and logical-canvas containment.

The same exact authored pair has two setup-time consumers:

1. `UiLayoutNodeSpec::childContentWidth/childContentHeight`, so absolute, anchored and stack children resolve against the larger content/reference space.
2. `UiDocument::ConfigureScrollViewport()`, after every resolved element has been added to the local document, so U9 owns clipping, descendant ownership, offsets and signed presentation translation.

There is no duplicate authored content size and no independent scroll geometry.

## Transactional loading

The loader completes hierarchy/layout resolution and adds every `UiElement` before configuring any authored scroll viewport. This ordering is required because U9 discovers the complete descendant set during explicit configuration.

If any authored scroll configuration returns an existing U9 error such as `unsupported_scroll_hierarchy`, the loader records a bounded `elements[N].scroll_content_size` diagnostic and does not publish the local `UiDocument`.

This preserves the existing U9 restriction against nested scroll ownership / movable nested clipping rather than weakening it for TOML convenience.

## Runtime authority

After load, ordinary scroll behavior is unchanged from U9:

```text
authored scroll_content_size
        |
        +-> U10 layout content extent
        |
        +-> U9 ConfigureScrollViewport
                 |
                 +-> direct scrollOwnerIndex
                 +-> retained clip bounds
                 +-> retained signed presentationBounds
                 +-> ScrollTo / ScrollBy / wheel routing
```

Logical `UiElement::bounds` remain immutable layout truth. Scrolling changes retained presentation bounds only when the offset changes. Pointer hit testing, capture/release, deterministic CPU rasterization and Agent inspection continue to consume the same U9 presentation/clip state.

## Performance boundary

All U11-specific work is authored-load/setup work:

- TOML field parsing,
- U10 extent validation and child placement,
- one U9 descendant-discovery/configuration pass per authored scroll viewport.

U11 adds no normal-frame allocation, semantic ID/string lookup, hierarchy discovery, filesystem/TOML work, layout recomputation, renderer work, GPU synchronization, or second retained content tree. Steady interaction inherits the existing U9 costs and direct-index state.

## Compatibility and deferrals

Format version remains `1`. Documents without `scroll_content_size` take the existing path unchanged, including legacy visible-parent containment.

Still deferred under #75:

- nested scroll viewports,
- scrollbars / draggable thumbs / inertia / overscroll,
- focus auto-reveal,
- production renderer GPU scissor submission,
- practical Image / Progress widgets,
- generic callback/bubbling graphs.

Parent: #75  
Issue: #246

Agent: ChatGPT  
Model: GPT-5.6 Sol
