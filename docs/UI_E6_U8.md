# E6 U8 — deterministic hierarchical clipping authority

Parent: #75  
Implementation issue: #240

## Purpose

U8 establishes one logical clipping authority before scroll translation and renderer scissor work.
Authored hierarchy can mark an element with `clip_children = true`; every descendant retains the
intersection of all clipping ancestors after U0-U3 layout finalization.

## Authored contract

`format_version = 1` remains source compatible. `clip_children` is optional and defaults to false.
The clip rectangle is the clipping ancestor's already-resolved logical `bounds`; a clipping element
does not clip its own pixels, only descendants.

Child-before-parent authoring remains supported because clip chains are resolved only after the
entire hierarchy has finalized.

## Runtime authority

Each `UiElement` retains:

- `clipChildren`: authored descendant-clipping policy,
- `clipActive`: whether at least one ancestor clips this element,
- `clipBounds`: the pre-resolved intersection of all clipping ancestors.

Pointer hit testing and captured release validation use `bounds ∩ clipBounds` when clipping is
active. Agent snapshots expose the same retained clip evidence. The deterministic CPU UI raster
uses that same intersection as its paint clip, so headless semantic and visual evidence cannot
disagree about the logical clip boundary.

## Performance boundary

Clip-chain resolution is explicit authored-load/setup work. With `N` elements and hierarchy depth
`D`, the current bounded implementation is O(N * D) and allocates no per-element clip objects beyond
the fields already retained in each element.

Steady interaction/raster work adds only rectangle checks/intersections:

- no semantic string lookup,
- no parent-chain discovery,
- no per-pointer-event allocation,
- no TOML/filesystem work,
- no layout recomputation,
- no GPU readback or synchronization.

The normal pointer scan remains O(N) in reverse painter order; U8 does not introduce a spatial tree.

## Deliberate boundaries

U8 does not yet add scroll offsets or mutate layout geometry. Existing authored children therefore
remain subject to the U0-U3 containment rules. A later scroll slice may translate retained content
for presentation while consuming this clip authority rather than weakening layout validation.

U8 also does not add:

- production renderer/GPU scissor submission,
- scroll bars, wheel/drag scrolling, or scroll range state,
- Image/Progress widgets,
- nested event callback/bubbling graphs.

Those are later #75 slices. The important invariant frozen here is that layout, pointer, Agent, and
headless raster all name the same deterministic logical clipping rectangle.
