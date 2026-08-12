# Sprite SR5 Primitive Contract

Status: **complete — #134 / PR #135; all hosted and owner Windows presentation-GPU acceptance gates passed on 2026-08-12**  
Umbrella: #59  
Predecessors: S1 canonical assets, SR0 selection, SR1 transforms, SR2 atlas geometry, SR3 appearance, SR4 order/groups/masks  
Exact next stage: **SR6 — pixel-perfect runtime presentation**

SR5 adds production 9-slice and tiled/repeated Sprite presentation without changing the authority model established by earlier Sprite stages. Canonical source-pixel metadata remains CPU truth; primitive geometry, UVs, sample guards and GPU buffers are derived presentation data.

## 1. Authority split

A `SpriteRegion` may optionally author an exact source-space border:

```toml
border = [left, top, right, bottom]
```

The values are non-negative integer pixels in the existing untrimmed Sprite source space:

```text
origin = top-left
+x     = right
+y     = down
```

Rules:

- omitted `border` in schema v1 means `[0, 0, 0, 0]`,
- `left + right <= source_size.width`,
- `top + bottom <= source_size.height`,
- trim and packed rotation never rewrite authored border values,
- normalized UVs, repeated cells and GPU resources are derived and are not canonical asset state.

This is a backward-compatible schema-v1 extension: old valid v1 files retain identical meaning, while canonical serialization makes the zero/default border explicit.

Primitive presentation mode is runtime intent, not asset identity:

```text
quad
sliced
tiled
```

The same resolved canonical region may therefore be reused as an ordinary Sprite, a resized 9-slice panel, or a tiled/repeated panel without duplicating authored asset metadata.

## 2. Runtime target size and pivot

`Sliced` and `Tiled` consume a finite, strictly positive target size in source-pixel-equivalent units before `pixels_per_unit` and the SR1 pose transform.

The asset pivot remains canonical and is never mutated. When target size differs from `source_size`, SR5 preserves the pivot's normalized position inside the source rectangle:

```text
target_pivot_x = source_pivot_x * target_width  / source_width
target_pivot_y = source_pivot_y * target_height / source_height
```

A centered canonical pivot therefore stays centered when a panel grows. An intentionally off-center or out-of-bounds pivot keeps the same normalized authored relationship rather than being silently clamped.

After target-space primitive geometry is derived, the existing SR1 scale/semantic flip/rotation/translation transform is applied exactly once through the SR2 logical-quad basis.

## 3. 9-slice partition

Source partitions are defined only from untrimmed source dimensions and canonical border pixels:

```text
x = [0, left, source_width - right, source_width]
y = [0, top,  source_height - bottom, source_height]
```

If the target is large enough, opposing target border widths preserve their source-pixel-equivalent sizes. If a target dimension is smaller than the sum of its opposing borders, the two target borders shrink proportionally and the center span becomes zero.

For `Sliced`:

- four corner cells preserve border extents,
- top/bottom edge cells stretch on X,
- left/right edge cells stretch on Y,
- center stretches on both axes,
- any zero-width or zero-height source/target cell emits no geometry.

## 4. Tiled/repeated semantics

For `Tiled`, corner cells remain fixed while non-corner center spans repeat their source cells:

- horizontal edge cells repeat on X,
- vertical edge cells repeat on Y,
- the center repeats on both axes,
- an exact multiple emits only full cells,
- a final partial cell is clipped geometrically and samples only the matching source sub-interval.

Trace2D deliberately does **not** use texture `REPEAT` address mode as Sprite semantic authority. Atlas pages may contain unrelated neighboring regions; wrap behavior must never make those texels part of a repeated Sprite.

The production sampler therefore remains clamp-to-edge. Repetition is explicit bounded geometry.

## 5. Trim and packed rotation

SR5 preserves the SR2 authority rules for every emitted patch:

1. logical cell placement is derived in the untrimmed source space,
2. the cell is intersected with the canonical trim rectangle,
3. trimmed-away transparent pixels remain logical gaps rather than shifting borders/tiles,
4. the visible source sub-rectangle is mapped into `packed_rect`,
5. `cw90` changes only source-subrect-to-packed-UV mapping.

`packed_rect` never determines logical placement.

For arbitrary source edges inside the trim rectangle, packed pixel-edge mapping is:

```text
none:
    packed_x = packed_rect.x + local_x
    packed_y = packed_rect.y + local_y

cw90:
    packed_x = packed_rect.x + trim_height - local_y
    packed_y = packed_rect.y + local_x
```

## 6. Per-patch sampling guard

SR3 keeps color space, straight-alpha source truth, tint, opacity, sampler and blend authority.

SR5 refines only the atlas-safe linear-filter clamp domain for each emitted patch. A patch's normalized UV geometry remains exact pixel-edge truth; a separate `SpriteSampleBounds` clamps sampling to texel centers inside the source texels represented by that patch.

For ordinary one-pixel extents, min/max collapse to that texel center. For a sub-texel partial tile, the guard also collapses to the represented source texel center rather than to the geometric midpoint of the fractional edge interval. This prevents linear filtering from pulling color from an adjacent repeated cell, another 9-slice source cell, or a neighboring atlas region.

Per-patch bounds are uploaded as vertex attributes. All six vertices of one patch contain identical bounds, so they remain constant over both triangles without adding a draw call per patch.

## 7. Bounded CPU geometry

`CountSpritePrimitivePatches` computes the exact visible patch count before writing output. `BuildSpritePrimitivePatches` writes only into caller-owned storage.

Rules:

- insufficient capacity returns the exact required count,
- insufficient capacity writes no partial output,
- one top-level Sprite is limited to `MaximumSpritePrimitiveQuads == 4096`,
- count arithmetic is overflow checked,
- the exact 4096 boundary is valid and one-over is rejected,
- the limit is a safety/diagnostic boundary, not a recommended normal workload.

The caller may retain a vector/arena at its high-water capacity and reuse it. SR5 itself does not require one allocation per frame or one allocation per patch.

## 8. Atomic SR4 order/mask integration

Primitive expansion never creates new semantic Sprite items.

```text
one SpritePresentationRenderData
    -> one SR4 order/group/mask entry
    -> N contiguous SR5 quad slots
    -> one triangle-list draw for N > 0
```

Consequences:

- another Sprite cannot interleave between the patches of one sliced/tiled Sprite,
- a mask writer applies the same mask phase to all of its patches,
- a mask tester tests all patches against the same active writer,
- broad resource sorting/batching across top-level Sprites remains deferred to SR7.

A fully invisible zero-patch primitive keeps its semantic top-level input but emits no triangles.

## 9. GPU resource and performance contract

The SDL GPU backend uses a persistent vertex buffer plus upload transfer buffer sized in reusable six-vertex quad slots.

Capacity behavior:

- capacity grows geometrically when the current high-water mark is exceeded,
- stable workloads reuse existing GPU buffers,
- SR5 does not create a sampler, shader or graphics pipeline per frame,
- all patches of one Sprite are contiguous in the upload buffer,
- one non-empty primitive Sprite is submitted with one `SDL_DrawGPUPrimitives` call,
- tiled semantics do not require a repeat sampler,
- ordinary rendering performs no explicit GPU readback or fence wait.

Capture/readback remains an explicit diagnostic API and is not part of normal presentation.

## 10. External precedent decisions

| Source | Decision | Trace2D use |
|---|---|---|
| Unity SpriteRenderer `Sliced` | ADAPT | corners retain size; edge/center regions stretch |
| Unity SpriteRenderer `Tiled` | ADAPT | corners retain size; edge/center regions repeat |
| Unity SpriteRenderer size | ADAPT | explicit resized presentation extent |
| texture wrap/repeat state | REJECT as authority | repetition is bounded derived geometry, atlas safe |
| SDL GPU triangle-list draw | ADOPT | contiguous explicit vertex submission |
| per-frame GPU resource creation | REJECT | persistent capacity-managed state |

External behavior is precedent only. Canonical border/pivot/trim/rotation semantics remain explicitly specified by Trace2D.

## 11. Verification boundary

Backend-independent tests prove:

- omitted/explicit border parse-save-parse identity,
- strict border bounds and malformed-array rejection,
- ordinary Quad compatibility with SR2,
- 9-cell sliced geometry and UVs,
- undersized proportional border compression,
- zero-span cell omission,
- trim gaps,
- arbitrary `cw90` source-subrect mapping,
- exact and partial tiled repetition,
- per-patch sample guard values,
- sub-texel partial guards collapse to the represented texel center,
- exact capacity/no-partial-write behavior,
- exact 4096 and one-over safety boundary.

Production/GPU tests additionally prove:

- one top-level SR4 semantic order item owns all primitive patches,
- masked primitive output is correct,
- a linear-filter partial tile does not bleed,
- non-empty multi-patch Sprite submission stays one Sprite draw call,
- vertex-buffer capacity is reused across repeated frames,
- sampler/pipeline counts remain stable,
- ordinary presentation adds zero explicit readbacks/fence waits.

## 12. Completion evidence

Final implementation head before completion-document commit:

```text
e05dc7158f5ea68b990f0e87f753629d6149ebab
```

Hosted validation on that head passed on 2026-08-12:

- CI run #604 — success,
- Content Evidence — success,
- Sprite S0 Contract — success,
- B0 Codex Wrapper — success,
- B0 Godot Agent Oracle — success.

Blocking owner Windows presentation-GPU fixture:

```text
SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
```

The first owner run exposed a real fractional-tile bleed: the expected pure-green sample was observed as approximately `(red=64, green=191)`. The implementation had collapsed a 0.5-texel sampled interval to its geometric midpoint, which is still a linear interpolation point. The fix collapses such sub-texel partial intervals to the source texel center they actually represent, and `SubTexelPartialTileClampsToRepresentedTexelCenter` locks that behavior in CPU CI.

The owner rebuilt and reran the real-GPU fixture. Final observed result:

```text
Start 120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract
1/1 Test #120: SpritePrimitiveGpuSmokeTests.Sr5TiledLinearMaskAndCapacityReuseMatchContract ... Passed
100% tests passed out of 1
Total Test time (real) = 2.28 sec
```

No renderer/driver string was captured; the completion record does not invent one.

## 13. Handoff

SR5 acceptance is complete. PR #135 is the completion/merge vehicle for #134. After it merges, confirm #134 closed and stop the continuation. **Do not create SR6 in the same continuation.** The next `@GitHub Trace2D 다음 진행해줘` continuation may create exactly one SR6 child.
