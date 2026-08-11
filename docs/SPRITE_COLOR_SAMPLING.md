# Sprite SR3 Color / Alpha / Blend / Sampling Contract

Status: **active via #130 / PR #131**

This document freezes the production Sprite appearance boundary after SR2 geometry/UV derivation and before SR4 painter-order/group/mask semantics.

## 1. Authority

Canonical authored truth remains:

```text
SpriteAsset page.color_space
SpriteAsset page.alpha_mode = straight
SpriteAsset sampling
SpriteAsset packed region metadata
```

Authoritative runtime appearance is a small backend-independent value:

```text
SpriteAppearance2D {
    tint     : linear RGBA in [0,1]
    opacity  : [0,1]
    sampling : inherit_asset | nearest | linear
    blend    : normal | additive | multiply | screen
}
```

GPU textures, samplers, pipelines, shader values and normalized sample guards are renderer-owned derived state. They never mutate `SpriteAsset`, `ResolvedSpriteRegion`, SR2 geometry, or runtime appearance authority.

## 2. Validation and steady-state extraction

All tint channels and opacity must be finite and inside `[0,1]`. Invalid values fail structurally; they are never silently clamped.

`ExtractSpriteAppearanceContract` is the steady-state semantic boundary:

```text
ResolvedSpriteRegion
 + SpriteAppearance2D
 -> SpriteAppearanceContractData
```

Successful extraction is O(1), fixed-size and caller-owned. It performs no heap allocation, semantic ID/path lookup, filesystem/TOML/image decode, renderer initialization, GPU work or canonical mutation.

## 3. Color space and straight-alpha boundary

Canonical Sprite texture pixels remain **straight-alpha source truth**.

For an sRGB page, renderer-owned sampled texture representation uses an sRGB-capable format so RGB is decoded to linear working space by the GPU sampling path. For a linear page, renderer-owned sampled texture representation uses a linear UNORM format.

No manual second sRGB decode is permitted when the GPU texture format already supplies sRGB decode.

Let a canonical sampled texel after color-space decode be:

```text
C = sampled straight RGB in linear working space
As = sampled straight alpha
T = linear tint RGBA
O = opacity
```

The built-in Sprite fragment boundary is exactly:

```text
A = As * T.a * O
P = C * T.rgb * A
fragment = (P, A)
```

`P` is premultiplied RGB. The canonical CPU source remains straight-alpha and unchanged; premultiplication is derived at the fragment boundary immediately before fixed-function blending.

White tint + opacity 1 is identity. Opacity 0 produces `(0,0,0,0)`.

## 4. Built-in blend modes

All equations below use premultiplied source RGB `P`, source alpha `A`, destination RGB `D`, and destination alpha `Ad` in target working space.

### Normal / source-over

```text
RGBout = P + D * (1 - A)
Aout   = A + Ad * (1 - A)
```

GPU blend identity:

```text
color: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

### Additive

```text
RGBout = P + D
Aout   = A + Ad * (1 - A)
```

GPU blend identity:

```text
color: src=ONE, dst=ONE, op=ADD
alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

### Multiply

```text
RGBout = P * D + D * (1 - A)
       = D * (P + 1 - A)
Aout   = A + Ad * (1 - A)
```

GPU blend identity:

```text
color: src=DST_COLOR, dst=ONE_MINUS_SRC_ALPHA, op=ADD
alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

Opacity zero is color identity.

### Screen

```text
RGBout = P + D * (1 - P)
Aout   = A + Ad * (1 - A)
```

GPU blend identity:

```text
color: src=ONE, dst=ONE_MINUS_SRC_COLOR, op=ADD
alpha: src=ONE, dst=ONE_MINUS_SRC_ALPHA, op=ADD
```

Opacity zero is color identity.

No arbitrary blend-factor property bag belongs to SR3. Unsupported enum values fail structurally.

## 5. Sampling

Runtime sampling resolution is finite:

```text
inherit_asset -> S1 SpriteAsset sampling
nearest       -> nearest min/mag
linear        -> linear min/mag
```

Current one-mip Sprite pages use nearest mip selection, clamp-to-edge U/V/W, no comparison and no anisotropy.

Renderer/device lifetime owns persistent nearest/linear samplers. A sampler must not be created per Sprite, draw, or frame.

## 6. Canonical UVs vs atlas-safe sample bounds

SR2 canonical page UVs remain pixel-edge truth and are never rewritten:

```text
u0 = packed.x / page.width
v0 = packed.y / page.height
u1 = (packed.x + packed.width) / page.width
v1 = (packed.y + packed.height) / page.height
```

SR3 derives a separate texel-center sampling guard:

```text
umin = (packed.x + 0.5) / page.width
umax = (packed.x + packed.width  - 0.5) / page.width
vmin = (packed.y + 0.5) / page.height
vmax = (packed.y + packed.height - 0.5) / page.height
```

The fragment sampling coordinate is clamped to these bounds before texture sampling. A one-pixel extent collapses min/max to one texel center. `cw90` storage uses the same packed rectangle bounds; SR2 remains solely responsible for corner UV permutation.

This separation preserves exact canonical atlas geometry while preventing linear filtering from sampling neighboring atlas content at region edges.

## 7. Finite compatibility identity

Later SR7 batching must be able to distinguish at least:

```text
texture/page resource
built-in material pipeline
sampler compatibility
blend compatibility
texture color encoding
mask compatibility
primitive kind
```

SR3 therefore keeps sampler, blend and texture encoding as finite trivially comparable values. Semantic painter order may never be globally reordered to improve compatibility.

## 8. Renderer-owned resource lifetime

The SDL GPU backend must derive and retain:

- one nearest and one linear Sprite sampler, or an equivalent fixed cache,
- one built-in Sprite pipeline per finite blend mode and target format, or an equivalent fixed cache,
- sRGB/linear sampled texture resources matching canonical page color-space intent,
- reusable upload/vertex resources for the SR2 draw geometry path.

No per-frame shader compilation, sampler creation, pipeline creation, texture color conversion, readback, fence wait or canonical asset mutation is allowed in ordinary presentation.

Readback/fence waits belong only to explicit capture/conformance work.

## 9. Deterministic proof

Backend-independent tests must prove:

- default identity appearance,
- linear tint and effective alpha math,
- strict finite/range rejection,
- inherit and explicit sampling resolution,
- all four exact blend equations,
- straight-alpha source truth remains explicit,
- sRGB vs linear texture encoding identity,
- texel-center bounds including one-pixel and `cw90` cases,
- corrupted packed/page metadata rejection,
- repeated fixed-size extraction without required allocation or GPU initialization.

The CPU fragment/blend helpers are conformance oracles; they do not replace the GPU implementation.

## 10. Real GPU completion gate

SR3 does not complete on CPU mappings alone. Before #130 can close, Windows presentation-GPU evidence must prove the production path for:

- nearest and linear sampling,
- atlas-edge linear sampling without neighbor bleed,
- sRGB and linear page behavior without double gamma conversion,
- normal/additive/multiply/screen blending,
- tint + opacity premultiplication,
- persistent sampler/pipeline reuse across repeated frames,
- no ordinary-frame readback/fence synchronization.

Capture/conformance may explicitly synchronize because the observation itself requires readback.

## 11. Handoff

SR3 must consume SR2 `SpriteDrawQuad` position/UV truth rather than creating a second full-texture quad semantics path.

After #130 merges green, advance exactly to **SR4 — painter order, sorting groups and Sprite masking**. Do not begin SR4 while #130 / PR #131 is active.
