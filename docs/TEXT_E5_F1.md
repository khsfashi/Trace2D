# E5 F1 — bounded glyph atlas/cache

Issue: #212  
Parent: #74

## Authority

F1 keeps one font/resource authority and adds only a bounded derived cache:

```text
FontResource (#86)
 -> FontFace / FreeType memory face (F0)
 -> fixed-pixel-height GlyphAtlas (F1)
 -> later layout/fallback/render/UI slices
```

`GlyphAtlas` owns its prepared `FontFace`, so the canonical font bytes remain retained through the existing generation-safe resource handle for the atlas lifetime. There is no font-only file cache or OS font discovery path.

## Cache contract

An atlas is prepared with explicit:

- width and height,
- font pixel height,
- padding,
- maximum cached glyph count.

Preparation allocates the alpha atlas once, reserves the complete contiguous entry capacity, and creates a fixed open-addressing lookup table at no more than 50% declared load. Codepoint lookup therefore needs no string/path lookup and no lookup-table growth. Cache insertion within the declared glyph count does not allocate entry/lookup storage.

The only expected allocation on a cache miss is F0's explicit temporary FreeType raster bitmap. Once cached, a codepoint hit does not rerasterize or rebuild the bitmap.

## Placement

F1 uses deterministic shelf packing in insertion order. Each non-empty bitmap owns an integer atlas rectangle surrounded by the configured padding. A glyph is committed only after its full padded rectangle is proven to fit.

There is no implicit atlas growth, eviction, repack or second page. Exhaustion returns a typed `glyph_atlas_full` diagnostic. Reaching `maxGlyphs` returns `glyph_cache_limit_reached` before another raster is requested.

This policy is intentionally simple and inspectable. Multi-page/eviction policy belongs to later workload evidence if a representative game requires it.

## Structural evidence

`GlyphAtlasMetrics` exposes:

- cached glyph count,
- cache hits/misses,
- successful rasterizations,
- retained alpha-atlas bytes,
- occupied bitmap pixels,
- preallocated lookup slot count.

These are structural counters only. No GPU timing or universal performance score is introduced.

## UTF-8 warm path

`WarmUtf8` reuses the strict F0 streaming decoder. It resolves codepoints directly into the same cache and returns compact counts for codepoints visited, unique glyphs added and cache hits. Malformed UTF-8 remains a typed error with the original byte offset.

Warm is an explicit cache-fill operation, not an ordinary render-frame requirement.

## Deferred after F1

F1 deliberately does not add:

- fallback font chains,
- wrapping/alignment/layout runs,
- shaping/bi-di,
- renderer texture upload/submission,
- production UI integration,
- implicit atlas growth/eviction.

The existing 5x7 ASCII UI raster remains a deterministic fixture until the later #74 integration slice replaces production text presentation, not test-fixture behavior.
