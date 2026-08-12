# Sprite S1 Canonical Asset Format

Status: **S1 implemented by #121; schema-v1 optional SR5 border extension active in #134 / PR #135**  
Umbrella: #59  
Architecture predecessor: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md)  
Primitive consumer: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md)

S1 defines the concrete canonical Trace2D Sprite authored/imported representation. It is CPU-side project truth and is deliberately usable without renderer or GPU initialization. SR5 extends the same schema version with one backward-compatible optional source-space border field; old v1 files retain identical meaning.

## 1. Canonical file identity

Canonical Sprite assets use a normalized project-relative `.sprite.toml` reference and an explicit schema header:

```toml
schema = "trace2d.sprite"
version = 1
sampling = "nearest"
```

Rules:

- absolute references are invalid,
- `..` traversal is invalid,
- `.` and separator spelling are normalized during explicit load/import,
- unsupported future versions fail explicitly rather than being guessed,
- external Aseprite/generator/sheet formats terminate at import and do not become runtime dispatch types.

The canonical asset ID is the normalized project-relative Sprite asset reference. It is not a filesystem absolute path, pointer, GPU handle, or importer-specific identity.

## 2. Source-space coordinates

S1 directly implements the S0 coordinate contract:

```text
origin = top-left of the untrimmed source image
+x     = right
+y     = down
rect   = integer half-open [x, y, width, height]
```

Every region records:

- `source_size` — logical untrimmed source extent,
- `trim_offset` — trimmed content top-left in source space,
- `trim_size` — trimmed logical content extent,
- `packed_rect` — exact stored rectangle on the referenced page,
- `pivot` — exact rational coordinate in untrimmed source space,
- `packed_rotation` — storage orientation only,
- optional `border` — exact `[left, top, right, bottom]` source-pixel 9-slice metadata added by SR5.

Trim must fit inside `source_size`. `packed_rect` must fit inside its page. Drawable source, trim and packed extents are positive.

For `border`:

```text
left + right <= source_size.width
top + bottom <= source_size.height
```

Each component is a non-negative `uint32` source-pixel distance. Omitted `border` means `[0, 0, 0, 0]`; trim and packed rotation never rewrite it. This optional zero default is why the field remains a schema-v1 extension rather than creating a second asset version.

## 3. Exact rational pivot

Schema v1 stores pivot as:

```toml
pivot = [x_numerator, y_numerator, denominator]
```

with a strictly positive signed-64-bit denominator. The loader reduces the three values by their common divisor so equivalent authored values have one canonical in-memory/serialized representation.

Examples:

```text
[8, -4, 4] -> [2, -1, 1]
[0, 0, 7]  -> [0, 0, 1]
```

A finite representable pivot is allowed outside source bounds and is never silently clamped. This keeps alignment/anchor decisions authored rather than importer-dependent and avoids binary-float drift in canonical metadata.

SR5 resized `sliced`/`tiled` presentation preserves this canonical pivot's normalized source position in the target rectangle; it does not mutate the stored pivot.

## 4. Atlas pages

Pages are ordered canonical CPU metadata:

```toml
[[pages]]
id = "main"
texture = "textures/player.png"
size = [256, 128]
color_space = "srgb"
alpha_mode = "straight"
```

Schema v1 supports:

- `color_space = "srgb" | "linear"`,
- `alpha_mode = "straight"` only,
- asset-level `sampling = "nearest" | "linear"`.

The page `size` is exact imported pixel metadata. `SpriteAssetCache` decodes the referenced texture through the existing CPU `TextureAssetCache` and rejects a page whose declared dimensions do not match decoded dimensions.

Canonical Sprite assets do **not** put these derived facts into `SpriteAsset`:

- SDL/GPU texture handles,
- normalized UVs,
- generated 9-slice/tile patches,
- mip chains,
- package/compression formats,
- GPU residency,
- sampler/pipeline objects,
- upload offsets/buffer addresses.

SR3 owns final runtime alpha/blend conversion semantics. SR5 owns derived primitive geometry from canonical border/source/trim metadata. #70 owns package format policy. #86 later generalizes common resource lifetime/residency without replacing this authored schema.

## 5. Regions, border and packed rotation

A region with SR5 border metadata:

```toml
[[regions]]
id = "panel"
page = "main"
source_size = [32, 32]
trim_offset = [2, 1]
trim_size = [28, 30]
packed_rect = [0, 0, 28, 30]
pivot = [16, 16, 1]
packed_rotation = "none"
border = [6, 6, 6, 6]
```

A legacy/schema-v1 region may omit `border`; the parser resolves that to the canonical zero border and the serializer writes the zero value explicitly.

Schema v1 supports exactly these packed storage rotations:

- `none`,
- `cw90` — the trimmed logical pixels are stored 90 degrees clockwise on the atlas page.

For `none`, packed width/height equal trim width/height. For `cw90`, packed width/height equal trim height/width.

Packed rotation never changes logical pivot, border, trim/source coordinates, gameplay rotation, or animation alignment. SR2 and SR5 derive UV/vertex mapping that undoes storage rotation.

## 6. Deterministic parser and serializer

The parser is strict:

- unknown fields fail,
- missing required fields fail,
- duplicate page/region IDs fail,
- missing page references fail,
- unknown enums fail,
- invalid integer ranges and rational denominator fail,
- malformed trim/packed bounds fail,
- malformed `border` arrays fail,
- opposing borders that exceed `source_size` fail.

`border` is the one optional region field currently accepted by schema v1. Its omission has an exact zero meaning; other required fields remain required.

Diagnostics contain stable error category plus asset reference, field path and parser source location where available.

Canonical serialization:

- writes fields in a fixed order,
- preserves explicit page order,
- preserves explicit region order,
- writes reduced rational pivots,
- writes explicit four-component border metadata, including the zero default,
- is independent of hash-container iteration order,
- round-trips back to the same canonical `SpriteAsset`.

Serialization and parsing are explicit setup/tooling work. Normal frames do not parse TOML or rebuild these representations.

## 7. Cache and ownership

`SpriteAssetCache` owns immutable successful Sprite imports keyed by normalized canonical Sprite reference.

```text
.sprite.toml
    -> strict TOML parse/validation
    -> canonical SpriteAsset CPU metadata
    -> referenced TextureAsset CPU validation
    -> immutable cached SpriteAsset
```

The cache exposes request/hit/miss/success/failure counters. It reuses decoded texture imports while validation is active and explicitly releases its Sprite/texture ownership on `Clear`; invalidating a cached Sprite also invalidates the page textures retained only by that S1 cache so the next explicit import can observe source changes.

This is not the future #86 resource manager. S1 intentionally does not introduce generation handles, dependency graphs, background loading, shared resource GC, or runtime residency policy.

## 8. Hot-path boundary

Allowed during explicit load/import/serialization:

- filesystem access,
- path normalization,
- TOML parsing,
- hash sets/maps for duplicate/reference validation,
- image decode for page-dimension validation,
- diagnostic/report allocations.

Forbidden as a requirement of ordinary Sprite rendering:

- TOML parsing,
- filesystem discovery,
- authored path normalization,
- region-name hash lookup per draw,
- image decode,
- canonical serialization,
- full inspection/report generation.

SR0+ consumes already-canonical CPU asset data. SR5 primitive expansion receives a pre-resolved region and caller-owned scratch; it does not add semantic-ID lookup or asset parsing to the render hot path.

## 9. Verification boundary

Canonical asset facts are deterministic/machine-owned:

- schema/version,
- normalized asset/page references,
- page dimensions,
- source/trim/packed rectangles,
- exact pivot,
- exact optional/zero-default border,
- packed rotation,
- page/region references,
- color/alpha/sampling intent,
- canonical round-trip identity,
- texture dimension agreement,
- cache structural counters.

No screenshot or multimodal review is required to prove these facts. Perceptual review begins only when presentation stages produce genuinely visual questions.

## 10. Render-stage handoff

The authority chain remains:

```text
canonical SpriteAsset CPU truth
        -> pre-resolved region selection
        -> derived render geometry / appearance / primitive patches
        -> renderer/backend resources
```

Renderer stages may not move normalized UVs, generated patches or GPU handles into authored asset truth. SR2 consumes exact trim/rotation metadata; SR3 consumes explicit color/alpha/sampling intent; SR5 consumes optional border metadata while preserving source-space authority; #86 later takes over generalized resource lifetime without changing the canonical Sprite schema.
