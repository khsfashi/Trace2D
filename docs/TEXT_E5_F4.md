# Text E5 F4 — production glyph-atlas presentation bridge

Parent: #74  
Slice: #218

## Authority boundary

F4 does not add a text-only renderer, texture allocator, shader, sampler cache or GPU lifetime system.

```text
FontResource (#86 canonical font identity/bytes)
 -> GlyphAtlas (F1 bounded CPU alpha8 + cache)
 -> TextLayoutRun (F2/F3 deterministic glyph placement + fontSlot)
 -> explicit atlas sync
      alpha8 -> white RGB / coverage alpha RGBA8
      -> ordinary #86 TextureResource (Linear + Straight)
      -> Renderer::CreateSpriteTextureRgba8(..., Linear)
 -> GlyphAtlasTextureBinding2D
 -> BuildTextPresentation2D
 -> existing SpritePresentationRenderData
 -> existing SR7 painter-order / batching / GPU path
```

`FontResource` and `GlyphAtlas` remain text authority. The generated TextureResource is presentation-derived data only. Texture identity remains the generation-safe #86 handle and renderer residency remains renderer-owned derived state.

## Why RGBA8 instead of a second R8 text backend

The production Sprite fragment boundary already has the exact behavior needed for anti-aliased glyph coverage:

1. sample straight-alpha texture data,
2. multiply sampled alpha by tint alpha / opacity,
3. multiply sampled RGB by tint RGB and effective alpha,
4. submit premultiplied output to the existing blend pipelines.

F4 therefore expands each atlas alpha byte into `{255, 255, 255, coverage}` and marks the generated texture `Linear + Straight`. The existing Sprite shader then produces tinted premultiplied text without another shader/pipeline branch.

An R8-specialized residency path may reduce atlas GPU bytes later, but it is deliberately not introduced without measured evidence that the extra backend/resource complexity is worth it.

## Pixel revision

F1 already publishes `GlyphAtlasMetrics::occupiedBitmapPixels`. For one atlas instance this value is monotonic and changes whenever a committed non-empty glyph bitmap adds atlas pixels. F4 exposes it as `GlyphAtlasPixelRevision()`.

A zero-area glyph such as a normal space changes layout advance/cache state but writes no atlas pixel, so it intentionally does not invalidate GPU atlas pixels.

`GlyphAtlasTextureBinding2D` captures the atlas pointer and pixel revision at bind time. `BuildTextPresentation2D` compares that revision in O(1); any later non-empty glyph insertion makes the binding stale and presentation is rejected until the caller performs an explicit atlas sync/rebind.

## Explicit sync sequence

The host owns when atlas synchronization is allowed. A typical setup/text-change sequence is:

```cpp
// Layout may rasterize previously unseen glyphs into the bounded F1 atlas.
auto result = layout.LayoutUtf8(fallbackAtlases, utf8);

assets::TextureResource generated{};
generated.width = atlas.Config().width;
generated.height = atlas.Config().height;
generated.colorSpace = assets::TextureResourceColorSpace::Linear;
generated.alphaMode = assets::TextureResourceAlphaMode::Straight;
generated.cpuRetention = assets::CpuRetentionPolicy::Releasable;
generated.canonicalRgba8.resize(
    static_cast<std::size_t>(generated.width) * generated.height * 4U);

std::size_t requiredBytes = 0U;
WriteGlyphAtlasRgba8(atlas, generated.canonicalRgba8, requiredBytes);

auto published = resources.PublishTexture(
    "generated/text/main-atlas.r1",
    std::move(generated));

GlyphAtlasTextureBinding2D binding{};
ResolveGlyphAtlasTextureBinding2D(atlas, resources, published.handle, binding);

renderer.CreateSpriteTextureRgba8(
    published.handle,
    render::Rgba8TextureData{
        atlas.Config().width,
        atlas.Config().height,
        resources.Resolve(published.handle)->canonicalRgba8,
    },
    render::SpriteTextureEncoding::Linear);

// Optional after binding + renderer upload when policy allows it:
resources.ReleaseTextureCpuPayload(published.handle);
```

If a later layout grows atlas pixels, publish/upload a refreshed generated texture and create a new binding before presenting that layout. F4 deliberately does not hide that transfer behind `BuildTextPresentation2D`.

## Layout to Sprite conversion

For every non-empty `PositionedGlyph`:

```text
left_px   = penX26_6 / 64 + bearingX
top_px    = baselineY26_6 / 64 - bearingY
right_px  = left_px + bitmap_width
bottom_px = top_px + bitmap_height
```

Those layout pixels are mapped from `TextPresentationConfig2D::origin` using `pixelsPerUnit`. Trace2D uses +X right / +Y down here, matching TileMap and atlas coordinates.

UVs use atlas pixel edges. `sampleBounds` use the glyph rectangle's texel centers so linear sampling cannot bleed into neighboring glyphs.

F3 fallback remains deterministic: each glyph's existing `fontSlot` directly indexes the caller-provided binding at the same fallback-chain slot. No global texture sort is allowed. Existing Sprite batching may merge only contiguous compatible glyphs after painter order is resolved.

Zero-area glyphs emit no Sprite quad. Their advance remains represented in the already-published layout positions of following glyphs. Stable painter order uses `stableOrderBase + layoutGlyphIndex`, so skipped spaces do not collapse semantic ordering.

## Transaction and allocation contract

`WriteGlyphAtlasRgba8` first reports exact required bytes and writes nothing when the caller span is too small.

`BuildTextPresentation2D` performs a full validation/count pass before publishing any output. If capacity is insufficient, `outRequiredCount` is exact and the caller's output remains untouched. With sufficient output it performs the deterministic second write pass.

Normal presentation of an unchanged prepared layout/atlas performs:

- O(layout glyph count) CPU work,
- O(1) fallback binding selection per glyph,
- zero allocation in Text,
- zero font discovery,
- zero FreeType rasterization,
- zero atlas RGBA conversion,
- zero ResourceRegistry lookup,
- zero filesystem access,
- zero GPU upload/readback/fence wait.

RGBA expansion and GPU upload are explicit cache-growth/setup work. F4 intentionally does not create a per-frame atlas synchronization path.

## Tests

`TextPresentation2DTests` covers:

- deterministic white-RGB / coverage-alpha expansion,
- no-write behavior for insufficient RGBA capacity,
- exact setup-time atlas/TextureResource validation,
- rejection of sRGB texture metadata,
- O(1) stale-binding detection after a new non-empty Korean glyph grows the atlas,
- mixed `A한中` primary/fallback presentation using exact F3 `fontSlot` order,
- transactional insufficient Sprite output,
- FreeType bearing/baseline geometry mapping,
- existing Sprite appearance contract (Linear, Straight, Normal blend),
- zero-area space omission while preserving layout-index stable order.

## Deferred

F4 does not decide:

- shaping, bidi or system font discovery,
- localization key/service/storage policy,
- editable UI widgets, caret/selection/composition rendering,
- automatic atlas growth/eviction,
- R8-specialized GPU residency or partial-subregion upload.

Those remain evidence-driven later #74/#75 work. The next #74 slice should be chosen from the remaining parent acceptance criteria after F4 merges green.
