# E6 U12 — deterministic Progress runtime specialization

Parent: #75  
Slice issue: #248

## Scope

U12 adds the first practical post-scroll widget without adding a generic widget framework. An existing resolved `Panel` can be specialized once as a Progress visual through `UiDocument::ConfigureProgress` and then updated through `UiDocument::SetProgress`.

The specialization deliberately reuses the element's existing hierarchy, logical bounds, signed presentation bounds, visibility and clipping state. There is no second layout tree, renderer-owned semantic state, callback graph or Progress side cache.

Authored `kind = "progress"` syntax is not introduced in this slice. Keeping authoring out of U12 avoids coupling the retained-state/raster contract to a parser expansion before the Image/resource boundary is settled. The next #75 slice can converge authored practical widgets together with Image's #86 generation-safe texture ownership.

## State authority

`UiProgressState` is retained directly by `UiElement`. Its mutable fields are private and only `UiDocument` may change them.

A configured state guarantees:

- `maximum > 0`,
- `value <= maximum`,
- `revision >= 1`,
- the target was a non-scroll `Panel` when configured.

`ConfigureProgress(id, value, maximum)` performs the one-time specialization. `SetProgress(id, value, maximum)` updates an already-configured Progress element.

Setting the exact same value/maximum pair is a successful no-op and does not advance `revision`. A real change advances revision with deterministic wrap from `UINT64_MAX` to `1`.

## Deterministic raster

The CPU UI raster consumes retained Progress state directly. Fill width is computed with integer arithmetic only:

```text
fill_width = uint64(presentation_width) * value / maximum
```

The resulting fill uses the existing signed `presentationBounds` and pre-resolved clip rectangle. No hierarchy walk, semantic-ID lookup, floating-point ratio or temporary container is introduced on the raster path.

## Agent semantics

A configured Progress element reports `UiRole::ProgressBar` / `"progressbar"` instead of its underlying Panel role. UI snapshots expose:

- `progressValue`,
- `progressMaximum`,
- `progressRevision`.

`UiExpectedState` can assert the same three fields, so headless tests do not need pixel interpretation to verify semantic state.

## Performance boundary

Unchanged Progress state performs no layout recomputation, filesystem work, resource lookup, hierarchy discovery or container growth.

An explicit `ConfigureProgress` or `SetProgress` call performs the existing bounded semantic-ID lookup once and then constant-time state validation/mutation. Ordinary raster and Agent snapshot paths consume retained values directly.

U12 adds no UI-owned resource cache. Image remains separate so its implementation can reuse #86 `ResourceHandle<TextureResource>` ownership rather than creating duplicate identity or lifetime rules.
