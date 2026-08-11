# Sprite SR1 Transform and Presentation Contract

Status: **implemented by #125 / SR1**  
Umbrella: #59  
Predecessor: SR0 backend-independent Sprite render contract  
Next Sprite child after SR1: **SR2 — atlas/trim/pivot/rotated-packing correctness**

SR1 implements authoritative Sprite pose/history semantics independently from renderer initialization, then derives the untrimmed logical Sprite quad through a fixed CPU math boundary.

## 1. Ownership

Authoritative state lives outside the renderer:

```text
scene::Transform2D
 + semantic flip_x / flip_y
        -> scene::SpritePose2D
        -> previous_fixed / current_fixed
```

The renderer receives only an already resolved SR0 region plus an already selected authoritative/presented pose:

```text
ResolvedSpriteRegion
 + SpritePose2D
 + pixels_per_unit
        -> SpriteLogicalQuad
```

The renderer never mutates `previous_fixed`, `current_fixed`, canonical Sprite metadata, or gameplay state.

## 2. Reused scene transform

SR1 reuses the existing `scene::Transform2D` rather than creating a Sprite-only transform type:

```text
position         : Vector2 world/local units
rotationRadians  : float radians
scale            : non-uniform Vector2
```

`SpritePose2D` adds only semantic `flipX` / `flipY`.

All transform floats must be finite. Zero scale and negative scale are valid. Flip remains a separate semantic bit and is never normalized into the sign of scale.

This preserves the #71 handoff: future typed world/components can attach the same scene transform semantics without translating from a renderer-owned Sprite transform.

## 3. Angle convention

```text
unit              = radians
world/local +x    = right
world/local +y    = up
positive rotation = counter-clockwise
```

Shortest-arc interpolation wraps the current-minus-previous float-angle difference into `[-pi, +pi]` using the same representable `float` pi/tau domain as authoritative `Transform2D` angles.

This detail matters at exactly 180 degrees: promoting a float `pi` to double and comparing against a different double-precision pi would otherwise be able to change the tie sign.

Tie rule:

- exact `+pi` wrapped difference remains `+pi`,
- exact `-pi` wrapped difference remains `-pi`.

At alpha 0 and 1, continuous fields are copied exactly from the corresponding authoritative samples instead of returning only an equivalent modulo-2pi angle.

## 4. Fixed-step history

`SpritePoseHistory2D` stores:

```text
previousFixed
currentFixed
```

`currentFixed` is authoritative.

### Snap/discontinuity

`SnapSpritePoseHistory(...)` validates the new authoritative pose before mutation, then performs:

```text
previousFixed = pose
currentFixed  = pose
```

Use this semantic for creation, reset, load/restore, teleport/warp and explicit non-interpolated snap.

### Successful fixed-step commit

`CommitSpriteFixedPose(...)` validates before mutation, then performs:

```text
previousFixed = old currentFixed
currentFixed  = new authoritative pose
```

There is intentionally no begin-step history mutation. An aborted/uncommitted simulation step simply does not call commit and therefore cannot advance presentation history accidentally.

Invalid snap/commit input leaves history unchanged.

## 5. Exact-frame vs interactive presentation

### Authoritative current

`ResolveSpriteAuthoritativeCurrent(...)` copies `currentFixed` exactly. It takes no wall-clock remainder or interpolation alpha.

This is the SR1 exact-frame/capture/verification presentation path.

### Interactive interpolation

`InterpolateSpritePose(...)` requires finite `alpha` in `[0,1]` and rejects invalid values rather than clamping/extrapolating.

Continuous fields:

```text
position = linear(previous, current, alpha)
scale    = linear(previous, current, alpha)
rotation = previous + shortest_arc_delta * alpha
```

Intermediate arithmetic uses double precision before checked conversion back to the authoritative/presentation float domain.

Discrete fields are not blended:

```text
flipX = currentFixed.flipX
flipY = currentFixed.flipY
```

This is true even at alpha 0. The continuous presentation may begin at the previous transform, while the current authoritative discrete semantic state changes atomically.

Interpolated values are never written back to history.

## 6. Source-space to world/local geometry boundary

Canonical S1 source metadata remains:

```text
origin = untrimmed top-left
+x     = right
+y     = down
```

SR1 performs one derived Y-down -> Y-up conversion while building logical geometry.

For source-space point `(sx, sy)` and exact rational pivot `(px, py)`:

```text
local_x =  (sx - px) / pixels_per_unit
local_y = -(sy - py) / pixels_per_unit
```

`pixels_per_unit` must be finite and greater than zero. It is a geometry conversion scalar, not yet SR6's pixel-perfect camera/presentation policy.

The canonical pivot is never rewritten, rounded into the asset, or clamped to source bounds.

## 7. Logical untrimmed quad

`BuildSpriteLogicalQuad(...)` emits the semantic corners:

```text
topLeft
topRight
bottomRight
bottomLeft
```

Corner labels refer to their canonical source-space corners. Reflections may change winding, but not semantic labels.

The pipeline is:

```text
untrimmed source_size + exact pivot
 -> source-pixel offsets from pivot
 -> Y-down to Y-up
 -> divide by pixels_per_unit
 -> semantic flip about pivot
 -> non-uniform/negative scale
 -> CCW rotation
 -> translation
 -> logical world/local quad
```

SR1 deliberately uses `source_size`, not `trim_size` or `packed_rect`, for logical extent. Trimming is storage optimization only. SR2 later maps stored trimmed/rotated pixels and UVs onto this logical source-space placement.

## 8. Negative scale and flip

Effective local axis multiplication is:

```text
x *= scale.x * (flipX ? -1 : +1)
y *= scale.y * (flipY ? -1 : +1)
```

Therefore:

- negative scale remains valid authored/runtime transform semantics,
- flip remains an independent Sprite semantic,
- negative X scale plus flip X can cancel geometrically without being canonicalized into one representation,
- zero scale produces valid degenerate geometry.

## 9. Numeric safety

Canonical source extents are uint32 and rational pivot numerators/denominator are signed 64-bit values. SR1:

- validates positive pivot denominator at the math boundary,
- performs rational/source/transform calculations in double intermediates,
- computes sin/cos once per quad derivation,
- checks conversion to finite float output,
- returns structured `GeometryOverflow` instead of emitting inf/NaN.

S1 remains the authored schema validator. SR1 guards manually constructed/corrupted CPU objects before presentation math.

## 10. Hot path

Both transform interpolation and logical-quad derivation are fixed-size O(1) operations.

Successful normal use requires no:

- heap allocation,
- vector/list construction,
- semantic string lookup/hash,
- path normalization,
- filesystem access,
- TOML parse,
- image decode,
- SDL/GPU initialization,
- formatted diagnostic strings.

The caller owns and reuses `SpritePose2D` / `SpriteLogicalQuad` output storage.

## 11. Hierarchy handoff

SR1 does not implement a Sprite-only parent graph.

Future #71 hierarchy follows the S0 rule:

```text
interpolate each local previous/current transform
 -> compose the interpolated local hierarchy
```

It must not compose previous/current world transforms first and then interpolate those world results.

## 12. SR2 / SR6 / #88 handoffs

SR2 consumes the same logical source-space placement and adds:

- trim offset/size placement,
- packed rectangle mapping,
- rotated-atlas storage undo,
- normalized UV derivation.

SR6 later defines pixel-perfect interaction among:

- source pixels,
- `pixels_per_unit`,
- world transform,
- resolved view/camera,
- logical viewport,
- presentation target pixels.

#88 later supplies Camera2D/Viewport2D into the existing resolved view seam. Neither stage replaces SR1 authoritative history.
