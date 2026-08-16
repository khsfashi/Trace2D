# E6 U13 — generation-safe Image resource boundary

Parent: #75  
Slice issue: #250

## Scope

U13 adds the practical Image runtime boundary without creating a UI-specific texture identity, decoded-image cache, renderer handle, or generic widget framework. An existing resolved non-scroll `Panel` can be specialized through `UiDocument::ConfigureImage` and later switched through `UiDocument::SetImage`.

The retained Image state contains only the canonical #86 `ResourceHandle<TextureResource>` and a semantic revision. Canonical texture metadata/pixels remain owned by `ResourceRegistry`; renderer residency remains renderer-owned.

Authored `kind = "image"` and authored `kind = "progress"` remain deliberately deferred to the next #75 slice. U13 freezes the generation/lifetime/presentation contract first so authored setup can resolve into one already-proven runtime authority instead of inventing parser-local resource semantics.

## State and lifetime authority

`UiImageState` is retained directly by `UiElement` and mutated only through `UiDocument`.

A configured Image guarantees that the supplied texture handle resolved as a live #86 `TextureResource` at configuration time. `SetImage` revalidates the replacement handle before publishing it. Assigning the exact same live handle is a successful no-op and does not advance revision.

Handles are intentionally small non-owning generation-safe references, matching #86. U13 does not implicitly `Retain`/`Release` registry roots and does not keep a `ResourceRegistry*` inside the document. Project/resource lifecycle remains explicit. If a texture is unloaded or its slot generation changes, the stale Image handle cannot resolve as a replacement resource.

Progress and Image are mutually exclusive when specialized through their respective configuration APIs. Image configuration also rejects an already-configured scroll viewport.

## Deterministic raster

The original resource-free `RasterizeUi(document, output, metrics)` remains source-compatible and unchanged for documents without Image state. It returns `false` when an active Image requires resource resolution.

U13 adds a resource-aware overload:

```text
RasterizeUi(document, resources, output, metrics)
```

For each visible Image, rasterization performs one generation-safe O(1) `ResourceRegistry::Resolve`, then samples canonical RGBA8 with integer nearest-neighbor coordinates derived from the element's retained signed `presentationBounds`.

Clipping uses the existing pre-resolved `clipBounds`. A scrolled Image therefore samples from the translated presentation rectangle while painting only the already-resolved visible region; rasterization does not walk hierarchy or rediscover scroll ownership.

Straight-alpha and premultiplied-alpha texture metadata are respected during deterministic source-over composition onto the opaque CPU UI canvas. U13 intentionally performs no color-space conversion: this CPU raster is deterministic UI evidence, not a claim of GPU colorimetric conformance.

If the handle is stale/unloaded or canonical CPU RGBA8 has been explicitly released, CPU rasterization fails deterministically instead of sampling unrelated replacement memory. GPU presentation remains a separate renderer-residency concern under #86.

## Agent semantics

A configured Image reports `UiRole::Image` / `"image"` rather than its underlying Panel role. Headless snapshots expose:

- `imageTextureSlot`,
- `imageTextureGeneration`,
- `imageRevision`.

`UiExpectedState` can assert the same fields. Agents therefore verify resource identity and retained semantic state without interpreting pixels or resolving authored path strings in steady state.

## Performance boundary

Explicit `ConfigureImage` / `SetImage` performs the existing bounded semantic-ID lookup plus one O(1) generation-safe registry resolve. It performs no filesystem query, reference canonicalization, texture decode, upload, hierarchy discovery, or cache insertion.

Unchanged Image state performs no mutation, allocation, layout recomputation, or ownership traffic. The Image state itself is fixed-size retained data.

Resource-aware CPU raster performs direct contiguous element traversal, one O(1) registry resolve per visible active Image, and integer sampling proportional only to painted Image pixels. It performs no path/string lookup, filesystem work, renderer upload/readback, hierarchy walk, or temporary image allocation beyond the existing output buffer.

## Acceptance evidence

`UiImageTests.cpp` covers:

1. live-handle configuration, replacement and no-op revision behavior,
2. invalid/stale handle rejection without mutating retained Image state,
3. Progress/Image specialization exclusion,
4. Agent Image role plus slot/generation/revision inspection/assertion,
5. resource-free raster rejection for Image documents,
6. deterministic nearest-neighbor canonical RGBA8 sampling,
7. stale handle rejection after explicit resource unload,
8. scroll translation plus retained clipping driving Image sampling.

The next #75 slice should add authored practical-widget convergence (`image` + `progress`) by resolving setup-time project resources into these existing runtime states. It must not add a second path cache, image lifetime model, or per-frame authored-reference lookup.
