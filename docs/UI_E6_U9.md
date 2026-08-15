# E6 U9 - Deterministic scroll viewport/content translation

Issue: #242  
Parent: #75  
Depends on: U8 hierarchical clipping (#240 / PR #241)

## Purpose

U9 establishes the runtime scroll authority without changing authored layout truth. `UiElement::bounds` remains the resolved logical/content-space rectangle. Scroll translation is published separately as signed `presentationBounds`, which is the geometry consumed by pointer hit testing and the deterministic CPU raster oracle.

This separation matters because content translated above or left of a viewport legitimately has negative presentation coordinates. Encoding that state by wrapping or clamping unsigned logical bounds would corrupt hit testing, text centering, inspection evidence, and later renderer integration.

## Runtime contract

An existing `Panel` can be configured once with `UiDocument::ConfigureScrollViewport(id, contentWidth, contentHeight)` after hierarchy/layout setup.

Configuration:

- requires content dimensions at least as large as the viewport and no larger than the current 4096 logical-dimension limit,
- marks the panel as the clipping authority for its descendants,
- validates every descendant against the declared content extent before publishing any scroll ownership,
- resolves each descendant to one direct `scrollOwnerIndex`,
- intersects descendant clipping with the viewport using the existing U8 clip state,
- rejects nested scroll ownership and moving nested clip authorities in U9 rather than introducing cumulative per-event hierarchy work.

`ScrollTo` and `ScrollBy` clamp to `contentSize - viewportSize`. A changed offset performs one allocation-free contiguous pass over retained elements owned by that viewport and refreshes only their signed presentation rectangles. Logical `bounds`, hierarchy indices, focus geometry, and text source identity do not change.

Repeating the current offset returns success without another translation pass. `scroll.revision` and `scroll.translationUpdates` provide deterministic inspection evidence for this behavior.

## Pointer/wheel contract

Ordinary pointer movement keeps the U4 reverse painter-order interactive hit scan and reads retained `presentationBounds` plus U8 `clipBounds` directly.

Wheel routing runs only when wheel input is present. It performs a reverse scan for the topmost visible/enabled scroll viewport under the logical pointer and applies one fixed `UiScrollWheelStep` (24 logical pixels) by wheel direction. The physical route never resolves semantic IDs or walks hierarchy.

Pointer capture remains attached to the captured element while content scrolls. Release activation is evaluated against the element's current translated presentation rectangle and clip, so an element scrolled away during capture cannot activate from stale geometry.

## CPU raster and Agent evidence

`RasterizeUi` intersects each signed presentation rectangle with canvas/U8 clip state before painting. Border and text origins remain based on the full translated rectangle, so partially clipped controls do not recenter their contents.

Agent UI snapshots expose:

- logical `bounds`,
- signed `presentationBounds`,
- `scrollViewport`,
- `scrollOwnerId`,
- content width/height,
- current X/Y offsets,
- scroll revision,
- cumulative translated-descendant updates.

This makes headless verification use the same state as pointer and CPU-raster paths.

## Complexity

- configuration: explicit setup-only bounded hierarchy discovery,
- changed offset: O(N) contiguous retained-element pass, zero allocation,
- unchanged offset: O(1) after semantic target lookup and zero descendant updates,
- ordinary pointer move: unchanged O(N) reverse interactive scan,
- wheel edge: one additional O(N) reverse viewport scan only when wheel input exists.

No scroll operation performs TOML/filesystem work, layout rebuild, GPU synchronization, or per-event allocation.

## Deliberate deferrals

U9 does not add:

- authored `kind = "scroll"` / content-extent layout syntax,
- nested scroll viewports or moving nested clip authorities,
- scrollbars, thumb dragging, inertia, or overscroll,
- automatic focus reveal,
- production renderer GPU scissor submission,
- Image/Progress widgets,
- generic callback/bubbling graphs.

Those remain bounded follow-up work under #75.
