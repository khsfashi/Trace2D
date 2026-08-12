# Sprite S1 Canonical Asset Format

Status: **S1 implemented by #121/#122; schema-v1 optional SR5 border extension completed by #134 / PR #135**  
Umbrella: #59  
Architecture predecessor: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md)  
Primitive consumer: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md)

This document defines canonical Trace2D Sprite authored/imported CPU truth. It is usable without renderer or GPU initialization. SR5 extends the same schema version with one backward-compatible optional source-space border field; old valid v1 files retain identical meaning.

## 1. Canonical file identity

Canonical Sprite assets use versioned `.sprite.toml` documents with normalized project-relative references. The format identity remains `trace2d.sprite`, schema version 1.

Pages and regions are explicitly ordered. Canonical serialization is deterministic and parse-save-parse stable.

## 2. Region fields

Every region records:

- `id` — canonical region identity,
- `page` — referenced atlas page identity,
- `source_size` — exact untrimmed source extent,
- `trim_offset` — visible trimmed-content origin inside the untrimmed source,
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

Each component is a non-negative `uint32` source-pixel distance. Omitted `border` means `[0, 0, 0, 0]`; trim and packed rotation never rewrite it. This exact optional zero default is why the field remains a schema-v1 extension rather than creating a second asset version.

## 3. Exact rational pivot

Schema v1 stores pivot as an exact reduced rational coordinate in the untrimmed source space. A finite representable pivot may intentionally lie outside source bounds and is never silently clamped.

SR5 resized `sliced`/`tiled` presentation preserves this canonical pivot's normalized source position in the target rectangle; it does not mutate the stored pivot.

## 4. Atlas pages

Each page owns canonical project-relative texture identity plus exact imported pixel size and explicit color/alpha/sampling intent.

Canonical source truth is:

- `srgb | linear` color-space intent,
- straight alpha,
- `nearest | linear` default sampling intent.

Decoded texture dimensions must match declared page dimensions during explicit load/import validation.

Canonical Sprite assets do **not** store:

- SDL/GPU texture handles,
- normalized UVs,
- generated 9-slice/tile patches,
- mip chains,
- package/compression formats,
- GPU residency,
- sampler/pipeline objects,
- upload offsets or buffer addresses.

Those are derived renderer/tooling state.

## 5. Regions, border and packed rotation

Example:

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

Packed rotation never changes logical pivot, border, trim/source coordinates, gameplay rotation or animation alignment. SR2/SR5 derive UV mapping that undoes storage rotation.

## 6. Deterministic parser and serializer

The parser is strict:

- required schema/field identity must be present,
- duplicate IDs fail,
- missing page references fail,
- unknown enums fail,
- invalid integer ranges and rational denominator fail,
- malformed trim/packed bounds fail,
- malformed `border` arrays fail,
- opposing borders that exceed `source_size` fail.

`border` is the optional region field added by SR5. Its omission has an exact zero meaning; other required fields remain required.

Canonical serialization:

- preserves explicit page order,
- preserves explicit region order,
- writes reduced rational pivots,
- writes explicit four-component border metadata, including the zero default,
- is independent of hash-container iteration order,
- round-trips back to the same canonical `SpriteAsset`.

Parsing/serialization are explicit setup/tooling work. Normal frames do not parse TOML or rebuild canonical assets.

## 7. Cache and ownership

Successful canonical load may perform filesystem access, TOML parsing, texture decode/dimension validation and diagnostic/report allocations. These are explicit setup operations.

Forbidden as a requirement of ordinary Sprite rendering:

- TOML parsing,
- filesystem discovery,
- image decode,
- canonical serialization,
- full inspection/report generation.

Renderer stages consume already-canonical CPU data. SR5 primitive expansion receives a pre-resolved region and caller-owned scratch; it does not add semantic-ID lookup or asset parsing to the render hot path.

## 8. Verification boundary

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

No screenshot or multimodal review is required to prove these facts.

## 9. Render-stage handoff

Authority remains:

```text
canonical SpriteAsset CPU truth
        -> pre-resolved region selection
        -> derived render geometry / appearance / primitive patches
        -> renderer/backend resources
```

Renderer stages may not move normalized UVs, generated patches or GPU handles into authored asset truth. SR2 consumes exact trim/rotation metadata; SR3 consumes explicit color/alpha/sampling intent; SR5 consumes optional border metadata while preserving source-space authority; #86 later generalizes resource lifetime without changing this canonical schema.
