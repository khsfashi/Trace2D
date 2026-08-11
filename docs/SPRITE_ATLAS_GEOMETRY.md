# Sprite SR2 Atlas, Trim and UV Geometry Contract

Status: **implemented by #127 / SR2**  
Umbrella: #59  
Predecessor: #125 / SR1 transform, presentation history and logical geometry  
Next Sprite child after SR2 merges green: **SR3 — color/alpha/blend/sampling semantics**

SR2 derives the exact visible Sprite quad and canonical atlas UVs from already-resolved CPU Sprite state. It preserves SR1's logical source-space transform semantics and treats trimming/packed rotation strictly as storage concerns.

## 1. Input and ownership

SR2 consumes only already-resolved state:

```text
ResolvedSpriteRegion
 + scene::SpritePose2D
 + pixels_per_unit
        -> SpriteDrawQuad
```

Canonical S1 source metadata remains authored/imported truth. SR2 never mutates:

- `source_size`,
- `trim_offset`,
- `trim_size`,
- `packed_rect`,
- rational pivot,
- packed rotation,
- Sprite pose/history,
- page identity or page metadata.

There is no semantic region/page lookup in the steady-state path. GPU/SDL handles remain outside this contract.

## 2. Shared SR1 source-point transform

SR2 reuses the exact SR1 source-point transform context rather than maintaining a second transform implementation.

For a source-space point `(sx, sy)` and exact rational pivot `(px, py)`:

```text
local_x =  (sx - px) / pixels_per_unit
local_y = -(sy - py) / pixels_per_unit
```

Then:

```text
semantic flip about pivot
 -> non-uniform/negative scale
 -> CCW rotation
 -> translation
```

The context resolves pivot, inverse pixels-per-unit, effective scale, one sin/cos pair and translation once per quad derivation. Both SR1 `BuildSpriteLogicalQuad` and SR2 `BuildSpriteDrawQuad` use this common boundary.

## 3. Trim geometry

Canonical source space remains top-left origin, +x right, +y down.

The visible logical trim rectangle is:

```text
TL = (trim_offset.x,               trim_offset.y)
TR = (trim_offset.x + trim_size.w, trim_offset.y)
BR = (trim_offset.x + trim_size.w, trim_offset.y + trim_size.h)
BL = (trim_offset.x,               trim_offset.y + trim_size.h)
```

Each source point passes through the shared SR1 transform boundary.

Consequences:

- trimming removes transparent storage area without shifting surviving pixels,
- rational pivot remains measured in the original untrimmed source space,
- an out-of-source pivot remains valid and unclamped,
- `packed_rect` never determines logical/world geometry,
- packed rotation never changes draw positions,
- negative scale and semantic flips keep the SR1 ordering/meaning.

When `trim_offset = (0,0)` and `trim_size = source_size`, SR2 positions equal the SR1 logical quad exactly.

## 4. Canonical atlas UV convention

Trace2D SR2 canonical page-space normalized UVs are:

```text
(0,0) = atlas page top-left edge
u     = right
v     = down
(1,1) = atlas page bottom-right edge
```

For page dimensions `(W,H)` and half-open packed rectangle `[x,y,w,h]`:

```text
u0 = x / W
v0 = y / H
u1 = (x + w) / W
v1 = (y + h) / H
```

These are **pixel-edge** coordinates.

Rules:

- no half-texel offset is added,
- SR3 later owns nearest/linear sampler behavior,
- backend-native coordinate adaptation belongs at the backend boundary,
- UV derivation uses checked finite conversion before publishing float output.

## 5. Packed rotation mapping

S1 `cw90` means the trimmed logical pixels are stored 90 degrees clockwise in the atlas.

For `none`:

```text
logical TL -> packed TL = (u0,v0)
logical TR -> packed TR = (u1,v0)
logical BR -> packed BR = (u1,v1)
logical BL -> packed BL = (u0,v1)
```

For `cw90`, sampling undoes the storage rotation:

```text
logical TL -> packed TR = (u1,v0)
logical TR -> packed BR = (u1,v1)
logical BR -> packed BL = (u0,v1)
logical BL -> packed TL = (u0,v0)
```

Therefore two regions with identical logical source/trim/pivot metadata but different valid packed orientations produce identical positions. Only storage extents and UV corner assignment differ.

## 6. Fixed draw output

SR2 emits fixed-size backend-independent values:

```text
SpriteDrawVertex
- position : Float2
- uv       : Float2

SpriteDrawQuad
- topLeft
- topRight
- bottomRight
- bottomLeft
```

Corner names remain logical source-space semantic corners even if flip/negative scale changes winding.

No dynamic four-vertex container is constructed.

## 7. Structured validation

S1 remains the authored-schema validator. SR2 independently guards manually created/corrupted CPU inputs at the render-math boundary.

`BuildSpriteDrawQuad` rejects with stable `SpriteGeometryError` + `SpriteGeometryField` at minimum:

- unresolved selection,
- invalid/non-finite pose,
- non-positive/non-finite `pixels_per_unit`,
- zero `source_size`,
- invalid rational pivot denominator,
- zero page dimensions,
- zero trim extent or trim outside source bounds,
- zero packed extent or packed rectangle outside page bounds,
- `none` packed extent not equal to trim extent,
- `cw90` packed extent not equal to swapped trim extent,
- unsupported packed-rotation value,
- source/transform/position conversion overflow/non-finite output,
- UV conversion overflow/non-finite output.

Failure clears caller output instead of exposing partially derived geometry.

Bounds checks use widened integer arithmetic so `offset + extent` cannot wrap a 32-bit source/page coordinate.

## 8. Performance contract

Successful SR2 derivation is O(1).

Normal use performs:

- one shared geometry-context build,
- at most one sin/cos pair,
- four fixed source-point transforms,
- four fixed UV corner derivations,
- no heap allocation required by the API,
- no vector/list construction,
- no semantic string lookup/hash,
- no filesystem/path normalization,
- no TOML parse/image decode,
- no GPU/SDL initialization,
- no canonical-asset mutation,
- no background work or GC.

Caller-owned `SpriteDrawQuad` storage is reusable across frames/draw extraction.

## 9. Verification boundary

Backend-independent tests prove:

- untrimmed SR2 positions equal SR1 logical geometry,
- exact trimmed placement in original source space,
- exact rational and out-of-source pivot behavior,
- transform/PPU/negative-scale/semantic-flip composition,
- exact pixel-edge UVs with no half-texel offset,
- exact `none` and `cw90` corner mapping,
- rotated/unrotated logical-position equivalence,
- exact page-edge 0/1 UV mapping,
- corrupted bounds/extent/rotation rejection,
- non-finite/overflow rejection,
- repeated caller-owned extraction without GPU initialization.

No screenshot or multimodal model is required to prove these facts.

## 10. Handoffs

SR3 consumes the SR2 draw geometry plus existing SR0 sampling/color/alpha intent and owns:

- tint/opacity,
- alpha conversion boundary,
- supported blend modes,
- nearest/linear sampler behavior and cached sampler state.

SR6 later owns pixel-perfect runtime mapping. SR7 later owns production batching/resource reuse. SR8 later adds renderer/GPU conformance fixtures. None of those stages may rewrite canonical SR2 source/UV truth merely to match one backend.
