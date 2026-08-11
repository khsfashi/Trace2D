# Sprite S1 Canonical Asset Format

Status: **implemented by #121 / S1**  
Umbrella: #59  
Architecture predecessor: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md)  
Next Sprite child after S1: **SR0 — renderer contract and asset/render separation**

S1 defines the first concrete canonical Trace2D Sprite authored/imported representation. It is CPU-side project truth and is deliberately usable without renderer or GPU initialization.

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
- `packed_rotation` — storage orientation only.

Trim must fit inside `source_size`. `packed_rect` must fit inside its page. Drawable source, trim and packed extents are positive.

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

S1 does **not** put these future/derived facts into `SpriteAsset`:

- SDL/GPU texture handles,
- normalized UVs,
- mip chains,
- package/compression formats,
- GPU residency,
- sampler/pipeline objects,
- upload offsets/buffer addresses.

SR3 owns final runtime alpha/blend conversion semantics. #70 owns package format policy. #86 later generalizes common resource lifetime/residency without replacing this authored schema.

## 5. Regions and packed rotation

A normal region:

```toml
[[regions]]
id = "idle_0"
page = "main"
source_size = [32, 32]
trim_offset = [2, 1]
trim_size = [28, 30]
packed_rect = [0, 0, 28, 30]
pivot = [16, 28, 1]
packed_rotation = "none"
```

Schema v1 supports exactly:

- `none`,
- `cw90` — the trimmed logical pixels are stored 90 degrees clockwise on the atlas page.

For `none`, packed width/height equal trim width/height. For `cw90`, packed width/height equal trim height/width.

Packed rotation never changes logical pivot, trim/source coordinates, gameplay rotation, or animation alignment. SR2 later derives UV/vertex mapping that undoes storage rotation.

## 6. Deterministic parser and serializer

The parser is strict:

- unknown fields fail,
- missing required fields fail,
- duplicate page/region IDs fail,
- missing page references fail,
- unknown enums fail,
- invalid integer ranges and rational denominator fail,
- malformed trim/packed bounds fail.

Diagnostics contain stable error category plus asset reference, field path and parser source location where available.

Canonical serialization:

- writes fields in a fixed order,
- preserves explicit page order,
- preserves explicit region order,
- writes reduced rational pivots,
- is independent of hash-container iteration order,
- round-trips back to the same canonical `SpriteAsset`.

Serialization and parsing are explicit setup/tooling work. Future normal frames do not parse TOML or rebuild these representations.

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

Forbidden as a requirement of ordinary future Sprite rendering:

- TOML parsing,
- filesystem discovery,
- authored path normalization,
- region-name hash lookup per draw,
- image decode,
- canonical serialization,
- full inspection/report generation.

SR0 must consume already-canonical CPU asset data and derive renderer-facing data outside the steady-state draw hot path.

## 9. Verification boundary

S1 facts are deterministic/machine-owned:

- schema/version,
- normalized asset/page references,
- page dimensions,
- source/trim/packed rectangles,
- exact pivot,
- packed rotation,
- page/region references,
- color/alpha/sampling intent,
- canonical round-trip identity,
- texture dimension agreement,
- cache structural counters.

No screenshot or multimodal review is required to prove these facts. Perceptual review begins only when later presentation stages produce genuinely visual questions.

## 10. SR0 handoff

After S1 merges green, SR0 may introduce backend-independent derived render vocabulary, but it must preserve:

```text
canonical SpriteAsset CPU truth
        -> derived render extraction
        -> renderer/backend resources
```

SR0 may not move normalized UVs/GPU handles into the authored asset or turn render preparation into gameplay/asset authority. SR2 consumes exact trim/rotation metadata; SR3 consumes explicit color/alpha/sampling intent; #86 later takes over generalized resource lifetime without changing the S1 canonical schema.
