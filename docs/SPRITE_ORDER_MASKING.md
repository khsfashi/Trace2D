# Sprite SR4 — Painter Order, Sorting Groups, and Masking

Status: active implementation contract for #132 under #59.

This document freezes the renderer-independent ordering and bounded Sprite masking semantics that sit on top of SR0-SR3. It is normative for SR4. Later batching, scene hierarchy, material systems, and Sprite primitive expansion may optimize or extend around this seam, but they must not reinterpret it.

## 1. Authority and scope

SR4 consumes already-resolved Sprite presentation truth:

```text
SR0 resolved region
 + SR1 pose/presentation selection
 + SR2 SpriteDrawQuad
 + SR3 SpriteAppearanceContractData
 + SR4 SpriteOrder2D / SpriteMask2D
        -> deterministic semantic Sprite sequence
        -> finite mask compatibility
        -> renderer submission in exactly that sequence
```

SR4 does not own:

- canonical SpriteAsset/import truth,
- transform authority or fixed-step history,
- trim/pivot/atlas UV derivation,
- tint/opacity/blend/sampling equations,
- generic scene hierarchy or recursive sorting groups (#71),
- programmable Material2D/Shader2D behavior (#89),
- 9-slice/tiled primitives (SR5),
- pixel-perfect view policy (SR6),
- broad batching/culling/global renderer optimization (SR7),
- arbitrary deformable geometry (#60).

GPU handles, command positions, batch IDs, pipeline handles, stencil attachments, and draw indices remain derived renderer state only.

## 2. Numeric domains

### 2.1 Sprite semantic order

`SpriteOrder2D` uses:

- `layer`: signed 32-bit integer,
- `order`: signed 32-bit integer,
- `stableOrder`: unsigned 64-bit integer in `[0, UINT64_MAX - 1]`,
- `UINT64_MAX`: reserved invalid stable-order sentinel.

`stableOrder == 0` is valid. This keeps default aggregate construction backward-compatible while still allowing exact ties to fall back to explicit input ordinal.

### 2.2 Sorting-group identity

`SpriteSortingGroup2D::id` is an unsigned 8-bit semantic identity:

- `0`: ungrouped,
- `1..255`: valid resolved sorting-group IDs.

An ungrouped entry must carry canonical zero group metadata:

```text
id          = 0
layer       = 0
order       = 0
stableOrder = 0
```

A non-zero group ID is valid only when every entry using that ID supplies exactly the same group anchor tuple `(group.layer, group.order, group.stableOrder)`.

SR4 intentionally supports one resolved group level only. Nested parent/child group relationships and cycles are not representable in this contract. Future scene hierarchy work must flatten/resolve hierarchy into this seam or introduce a separately reviewed extension; the renderer must not infer ancestry from pointers, containers, or traversal order.

### 2.3 Mask identity

`SpriteMask2D::id` is an unsigned 8-bit semantic identity:

- mask mode `none` requires ID `0`,
- mask modes `write`, `test_inside`, and `test_outside` require IDs `1..255`.

The CPU identity is semantic state. The SDL GPU backend currently maps the same numeric value to the dynamic 8-bit stencil reference, but canonical assets never store an SDL/GPU stencil object or attachment.

## 3. Exact painter-order comparator

Each renderer submission entry receives a caller-input `sourceIndex`. Before sorting, `sourceIndex` must equal the original input ordinal exactly.

### 3.1 Ungrouped top-level key

For an ungrouped Sprite:

```text
top.layer  = sprite.layer
top.order  = sprite.order
top.stable = sprite.stableOrder
```

### 3.2 Grouped top-level key

For a grouped Sprite:

```text
top.layer  = group.layer
top.order  = group.order
top.stable = group.stableOrder
```

The group therefore occupies one atomic top-level semantic ordering unit. Child Sprite order never leaks out to interleave unrelated top-level units.

### 3.3 Total ordering

The resolver compares, in order:

1. `top.layer`, ascending,
2. `top.order`, ascending,
3. `top.stable`, ascending,
4. if one entry is ungrouped and one grouped at an otherwise exact top-level tie: ungrouped first,
5. if both are grouped at the same top-level tuple but use different IDs: group ID ascending,
6. within the same group only: child `layer`, then child `order`, then child `stableOrder`, all ascending,
7. exact remaining semantic tie: original `sourceIndex`, ascending.

The final source-index tie rule makes exact ties deterministic and preserves original caller order without using allocation address or container traversal as hidden state.

The following values are forbidden from the comparator:

- texture identity,
- material identity,
- sampler mode,
- blend mode,
- Sprite asset pointer,
- allocation address,
- unordered-container iteration order,
- GPU texture/pipeline/sampler handle,
- batch ID or command-buffer position.

A later renderer may merge only compatible contiguous work. It may never globally resource-sort across this semantic order.

## 4. Resolver ownership and complexity

`ResolveSpriteOrderMask2D` operates on caller/renderer-owned scratch entries only. It never mutates `SpriteAsset`, resolved selection, pose/history, geometry, appearance, or the caller's presentation array.

Current complexity:

```text
validation: O(n)
ordering:   O(n log n) comparisons via one in-place std::sort
mask phase validation after sort: O(n)
```

Group-anchor validation uses a fixed 256-entry stack array keyed by the bounded 8-bit group ID. It requires no hash table or semantic string lookup.

The production renderer retains and reuses a vector scratch buffer across frames. Capacity growth is demand-driven; stable workloads do not require repeated scratch allocations.

## 5. Bounded mask state machine

Masking is intentionally finite and painter-order-driven.

Valid modes:

```text
none
write(mask_id)
test_inside(mask_id)
test_outside(mask_id)
```

Only one mask phase is active at a time in the resolved painter sequence.

### 5.1 Phase rules

A phase follows:

```text
one or more write(M)
 -> zero or more test_inside(M) / test_outside(M)
```

Rules:

1. `write(M)` starts mask phase `M` when no mask is active.
2. Additional consecutive `write(M)` entries are allowed before any tester and union coverage for `M`.
3. The first inside/outside tester closes the writer portion of phase `M`.
4. A later `write(M)` after any tester in the same phase is rejected.
5. `write(N)` for `N != M` closes phase `M` and starts phase `N`.
6. Once another mask identity has replaced phase `M`, `write(M)` may not re-enter later in the same submission sequence.
7. A tester is valid only for the currently active mask identity.
8. `none` neither opens nor closes a mask phase.

These restrictions are deliberate. The SDL backend uses one 8-bit stencil attachment, so a different mask writer can overwrite previous per-pixel references. Rejecting old-mask re-entry prevents backend-dependent assumptions about stale stencil coverage.

## 6. Mask writer coverage

A mask writer uses normal SR2 geometry and SR3 atlas-safe sampling/tint/opacity inputs, but does not write color.

The coverage equation is:

```text
effective_alpha = sampled_straight_alpha * tint_alpha * opacity
covered         = effective_alpha >= 0.5
```

Fragments below `0.5` are discarded before the stencil write. The threshold is exactly `0.5`; there is no backend-dependent alpha-test default.

Sampling still uses the resolved SR3 sampler and texel-center sample bounds. The mask does not rewrite canonical SR2 UVs or source alpha mode.

## 7. SDL GPU mapping

The production SDL GPU implementation uses a stencil-capable depth/stencil target selected at renderer initialization from:

1. `D24_UNORM_S8_UINT`,
2. `D32_FLOAT_S8_UINT`.

Support is queried for `SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET`; failure to find a supported format is structural backend failure.

A Sprite presentation submission that contains no mask state uses the normal color-only render pass and does not create, attach, or clear a mask target. When at least one masked Sprite is present, the stencil attachment is size-matched to the presentation target and cleared to zero for that masked render pass. Depth data is unused.

### 7.1 Pipeline mapping

SDL GPU pipeline target compatibility is explicit, so SR4 keeps distinct unmasked pipelines for color-only passes and for ordinary `none` Sprites that appear inside a masked pass:

```text
4 color-only unmasked pipelines:            normal/additive/multiply/screen
4 stencil-compatible unmasked pipelines:   normal/additive/multiply/screen
4 inside pipelines:                        normal/additive/multiply/screen
4 outside pipelines:                       normal/additive/multiply/screen
1 mask-writer pipeline
--------------------------------------------------------------------------
17 Sprite presentation pipelines total
```

The two SR3 samplers remain persistent and reused. The color-only unmasked set is created without depth/stencil target compatibility. The second unmasked set has the selected stencil-capable target format but keeps stencil testing disabled; it is selected only when the current Sprite submission contains at least one masked Sprite. This allows a `none` Sprite to preserve painter order inside a masked render pass without mutating stencil state, while completely unmasked frames still avoid mask-target allocation, attachment and clear.

Stencil mapping:

```text
write(M):
  compare      = ALWAYS
  stencil pass = REPLACE
  reference    = M
  compare mask = 0xFF
  write mask   = 0xFF
  color writes = disabled

test_inside(M):
  compare      = EQUAL
  stencil pass = KEEP
  reference    = M
  compare mask = 0xFF
  write mask   = 0x00

test_outside(M):
  compare      = NOT_EQUAL
  stencil pass = KEEP
  reference    = M
  compare mask = 0xFF
  write mask   = 0x00
```

The dynamic stencil reference is set immediately before each masked draw.

The mask target is reused until the presentation target dimensions change. Persistent mask pipelines, samplers, and steady target size must not be recreated per Sprite or per frame.

## 8. Interaction with SR3 appearance

`none`, `test_inside`, and `test_outside` retain the exact SR3 fragment and blend contract for visible fragments:

```text
A = sampledStraight.a * tint.a * opacity
P = sampledLinear.rgb * tint.rgb * A
fragment = (P, A)
```

and the same four premultiplied fixed-function blend modes.

Mask tests affect only whether a fragment is accepted by stencil. They do not reinterpret tint, opacity, color space, sampling, or blend mode.

`write` consumes SR3 alpha/sampling inputs only to calculate coverage and intentionally emits no color.

## 9. Structured failures

Backend-independent stable failure categories include:

- invalid/non-sequential source identity,
- reserved invalid stable order,
- malformed ungrouped metadata,
- inconsistent non-zero group anchor,
- invalid mask mode/ID pair,
- tester without the corresponding active writer,
- writer after a tester in the same phase,
- closed mask-phase re-entry.

Production GPU failures remain explicit for unsupported depth/stencil format and GPU resource/pipeline creation failures. Formatted SDL backend error strings belong to exception/diagnostic paths, not ordinary semantic extraction.

## 10. Synchronization and observation

Ordinary rendering adds no explicit GPU readback and no explicit fence wait for ordering or masking. Unmasked presentation additionally performs no mask-target allocation/attachment/clear. Capture/conformance paths may synchronize because observation requires CPU-visible pixels; those operations remain counted separately in renderer metrics.

The renderer exposes persistent sampler/pipeline/vertex capacity and mask-target creation metrics so tests can prove warm reuse and zero unmasked mask-target creation rather than infer either property from timing.

## 11. Tests required by SR4

Backend-independent tests freeze:

- signed layer/order ordering,
- stable identity and exact source-order tie behavior,
- group atomicity and local child order,
- deterministic same-anchor group collision policy,
- inconsistent/malformed group failures,
- valid writer/inside/outside phases,
- tester-without-writer, writer-after-tester, phase-reentry, and invalid mask failures.

The opt-in real Windows presentation-GPU fixture must additionally prove:

- overlapping opaque Sprites follow semantic painter order even when input/resource order differs,
- a sorting group behaves as one top-level ordering unit,
- unmasked presentation does not create/attach a mask target,
- an ordinary `none` Sprite remains valid and leaves stencil state unchanged inside a masked pass,
- inside mask capture exposes only covered pixels,
- outside mask capture exposes the inverse coverage,
- persistent pipeline/mask-target counts stop growing after warm-up,
- repeated ordinary rendering adds no explicit readback/fence wait.

Hosted CPU/compile validation is necessary but does not replace this local real-GPU gate.

## 12. Handoff

SR4 must merge green and record the required real-GPU evidence before the Sprite program advances.

The next child after SR4 is **SR5 — 9-slice and tiled/repeated Sprite primitives**. SR5 must not be created or implemented while SR4 remains open.
