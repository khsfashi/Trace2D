# Sprite SPP3 Import Contract

Status: **active via #164**.  
Parent: #59. Predecessor: SPP2 #162 / PR #163 / squash `13b4e3ba71d577914777e4c183e2819a94c6fc04`.  
Exact next after green merge: **SPP4 — sprite-gen / PerfectPixel-style manifest interoperability**.

## Purpose and authority

SPP3 is the deterministic offline conversion boundary from supported external Sprite interchange into canonical Trace2D `SpriteAsset` data plus ordered import evidence.

```text
external manifest + decoded RGBA8 page(s)
 -> explicit SPP3 conversion
 -> existing S1 canonical SpriteAsset validation
 -> ordered frame/tag import evidence
 -> later save/runtime preparation
```

External formats are inputs only. Runtime animation/render/gameplay never dispatch importer or source-tool code. S1 remains canonical asset truth and SA0-SA4 remains animation timing/playback truth.

`SpriteImport.hpp` exposes:

- `ImportAsepriteSpriteSheetJson(...)`,
- `ImportGenericSpriteSheet(...)`,
- `ImportLooseSpriteFrames(...)`,
- deterministic `SerializeSpriteImportResultJson(...)` (`trace2d.sprite-import.v1`).

Successful imports are serialized and reparsed through the existing S1 canonical parser before success is returned, preserving S1 schema validation and rational-pivot normalization.

## Aseprite sheet + JSON

Baseline SPP3 uses the official exported sprite-sheet + JSON interchange. It intentionally does **not** parse native `.ase/.aseprite` files: doing so correctly would require source-tool layer/group compositing, linked/compressed cel, palette/indexed-color, tilemap, color-profile and related semantics and would create a second Aseprite renderer/authority surface.

Supported JSON is `array` or `hash` frame form with explicit:

- `frame`, `rotated`, `trimmed`, `spriteSourceSize`, `sourceSize`, positive integer `duration`,
- `meta.image`, `meta.size`, `meta.scale`, optional `meta.format`, optional `meta.frameTags`.

The supplied decoded page must have positive dimensions, exactly `width*height*4` RGBA8 bytes, an ID matching `meta.image`, and dimensions matching `meta.size`. Baseline requires `meta.scale == "1"`; scaled export needs a future explicit source-coordinate conversion contract. When present, `meta.format` must be `RGBA8888`.

Array form keeps authored array order and uses non-empty `filename` as region ID. Hash form keeps manifest object order and uses object keys as region IDs. IDs are never alphabetically resorted or inferred.

Aseprite geometry maps directly into S1 source/trim/page coordinates:

```text
trim_offset = spriteSourceSize.[x,y]
trim_size   = spriteSourceSize.[w,h]
source_size = sourceSize
packed_rect = frame
```

`trimmed=false` requires the trim rectangle to cover the full source size.

### Rotation ambiguity

Aseprite JSON provides a boolean `rotated` flag, but SPP3 does not infer canonical direction from that boolean alone.

Default:

```text
rotated=false -> none
rotated=true  -> unsupported_rotation
```

A caller may explicitly choose `InterpretAsCw90`; only then does `rotated=true` map to canonical `cw90`, with swapped packed/trim extents validated exactly.

### Duration and tags

Duration uses checked integer conversion only:

```text
duration_ns = duration_ms * 1,000,000
```

Zero or overflow fails. No floating-point seconds are introduced.

Optional frame tags retain ordered name/range/direction evidence for `forward`, `reverse`, `pingpong`, and `pingpong_reverse`. Tags do not instantiate or mutate `SpriteAnimator2D`.

## Generic sheet

Generic sheets require explicit caller-authored layout; there is no hidden segmentation.

Explicit-region mode accepts ordered region ID, packed rectangle, optional source/trim metadata, optional exact rational pivot, and explicit `none|cw90` rotation.

Uniform-grid mode reuses `SpriteExtractionGridSpec` (origin/cell/rows/columns/spacing/row-major-or-column-major) plus one explicit region ID per cell and `expectedFrameCount`. `rows*columns`, region-ID count and expected count must match exactly; widened arithmetic is checked and every final rectangle must fit the sheet.

## Loose frames

Loose frames are ordered decoded images with unique page ID, unique region ID, project-relative texture reference, exact dimensions/RGBA8 bytes and optional pivot. Each image remains its own canonical page; SPP3 does not repack merely to import.

Default geometry is full-image/untrimmed/no-rotation.

## Failure / determinism / performance

Hard failures are transactional: diagnostics remain, but partial pages/regions/frames/tags are cleared. This includes malformed JSON, identity/size/byte mismatch, duplicate IDs, invalid rectangles, unsupported rotation, zero duration, invalid tag ranges/directions, generic count mismatch and S1 canonical validation failure.

Identical manifest bytes + decoded metadata/bytes + options + input order produce field-identical output and byte-identical structural JSON. Unordered sets are membership-only and do not control observable ordering.

Complexity is offline/setup-only:

- Aseprite conversion `O(manifest bytes + frames + tags)`,
- generic import `O(frame count)`,
- loose-frame import `O(frame count)`,
- S1 canonical validation proportional to imported metadata.

Pixel buffers are viewed, not copied, merely for metadata validation. No fixed-step/render/GPU work, background worker, global importer cache, GC/reclamation system or atlas packer is introduced. The repository's existing `nlohmann-json` package is reused; SPP3 adds no dependency.

## External reference decisions — 2026-08-12

Primary references:

- Aseprite CLI: `https://www.aseprite.org/docs/cli/`
- Aseprite sprite sheets: `https://www.aseprite.org/docs/sprite-sheet/`
- Aseprite file format: `https://github.com/aseprite/aseprite/blob/main/docs/ase-file-specs.md`

Decisions: **ADOPT/ADAPT** exported sheet+JSON, integer frame duration and tag metadata; **ADAPT** source/trim/page geometry into S1; **REJECT** runtime source-tool dispatch, hidden generic layout inference, native Aseprite binary compositing as baseline, and implicit `rotated=true -> cw90` without caller policy.

No new real-GPU gate is required because SPP3 changes no presentation path.

After SPP3 merges green, stop. Begin SPP4 only in the following continuation.
