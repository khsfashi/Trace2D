# Sprite SPP2 Quality / Repair Contract

Status: **active via #162**.  
Parent program: #59.  
Predecessors: SPP0 #154, SPP1 #156, owner hardening detours #158/#159.  
Exact next after SPP2 merges green: **SPP3 — Aseprite and generic sprite-sheet / loose-frame importers**.

## Purpose

SPP2 provides deterministic offline Sprite quality evidence and only caller-selected bounded repair. It must not turn artistic judgment into engine truth.

```text
SPP1 extracted RGBA8 + explicit metadata
 -> exact structural evidence
 -> optional policy/threshold-labelled advisory findings
 -> explicit deterministic repair request
 -> owned repaired RGBA8 / pivot metadata
 -> SPP0 post-repair analysis
 -> later SPP3 import / canonical SpriteAsset
```

SPP2 output is derived offline evidence/content. Runtime Sprite, animation, renderer, GPU resources and canonical Sprite assets do not depend on this stage during ordinary execution.

## API authority

The protocol-independent API is `AnalyzeAndRepairSpriteQuality` in `SpriteQuality.hpp`.

Inputs are ordered `SpriteQualityFrameView` values containing:

- stable non-empty frame ID,
- positive width/height,
- exact RGBA8 bytes,
- optional exact `SpriteRationalPivot`.

The caller may additionally supply explicit pixel-grid, palette, pivot-target, centroid-threshold and repair policy. No missing quality policy is inferred from pixels.

## Pixel-grid evidence

A pixel grid exists only when the caller supplies positive `blockWidth` and `blockHeight`.

Baseline SPP2 deliberately requires each frame dimension to be divisible by the declared block size. Partial edge blocks are rejected rather than silently ignored because ignoring them would make the checked region depend on an implicit edge policy.

For each complete block, SPP2 compares exact RGBA8 values and reports:

- checked blocks,
- uniform blocks,
- violating blocks,
- total pixels belonging to violating blocks.

A violation is a structural fact relative to the **declared** block grid. SPP2 does not infer whether an image is pixel art or infer its intended scale.

## Palette evidence

The caller may supply an ordered explicit RGB palette with at most 256 unique entries.

For pixels with `alpha > 0`, SPP2 records:

- visible pixel count,
- exact RGB matches in the supplied palette,
- exact RGB values outside the supplied palette,
- distinct visible RGB count.

When `measureNearestPaletteDistance` is enabled, off-palette pixels also contribute an exact squared decoded-byte RGB distance to the nearest palette entry.

This distance is a deterministic policy metric only. It is **not** described as perceptually uniform and it never replaces multimodal/human visual judgment.

Fully transparent pixels are excluded from palette membership evidence. Alpha is kept as an independent byte and baseline palette repair preserves it.

## Pivot evidence

Pivots remain exact `SpriteRationalPivot` metadata. Denominators must be positive.

When an explicit target pivot is provided, SPP2 reports exact match/mismatch while retaining the frame's exact pivot. It does not infer pivots from silhouettes and it does not translate pixels implicitly.

## Identity / silhouette evidence

SPP0 remains the authoritative exact byte-identical duplicate-frame check.

SPP2 adds adjacent equal-dimension evidence:

- exact RGBA-changed pixel count,
- exact visible-mask changed pixel count where visibility is `alpha > 0`,
- whether RGBA changed while the visibility mask stayed identical.

No probabilistic hash is used as authoritative identity.

## Motion evidence

For every non-empty frame, SPP2 stores exact integer visible-pixel centroid evidence:

```text
sumX / visibleCount
sumY / visibleCount
```

Adjacent non-empty frames store exact rational centroid deltas. When pivots are present, the report retains both the exact centroid evidence and exact pivot metadata so a consumer can derive centroid-relative-to-pivot position without a lossy float authority.

A `motion_centroid_threshold_exceeded` finding is generated only when the caller supplies an integer per-axis maximum delta. This means only that the structural centroid moved farther than the explicit threshold; it is not a claim that the animation looks bad or jitters perceptually.

## Repair order and transaction boundary

When multiple repairs are requested, baseline order is fixed:

```text
pixel-block canonicalization
 -> palette remap
 -> pivot normalization
 -> SPP0 post-repair analysis
```

Analysis-only requests do not allocate repaired pixel copies. Repair requests materialize owned output frames. Any hard repair failure clears all repaired output/repair records for the request while retaining pre-repair evidence and a structured diagnostic.

### Pixel-block canonicalization

For each violating block:

1. count exact RGBA values,
2. select the most frequent value,
3. resolve equal-frequency ties by the lowest packed numeric RGBA value,
4. rewrite every pixel in that block to the selected RGBA.

The record reports changed blocks and actually changed pixels. This is deterministic caller-selected cleanup, not inferred artistic correction.

### Palette remap

Visible off-palette RGB values map to the nearest ordered palette entry using exact squared decoded-byte RGB distance.

Tie rule: the lowest palette index wins.

The caller must provide `maximumPaletteDistanceSquared`. If any required remap exceeds that bound, the whole repair request fails transactionally and no repaired frame set is returned. Successful remap changes RGB only and preserves alpha.

No hidden dithering is performed.

### Pivot normalization

When explicitly requested, every repaired frame receives one exact target `SpriteRationalPivot`. This changes metadata only. Pixel translation/crop is not implied.

### Motion repair exclusion

SPP2 baseline does not automatically move, crop or resample frames to make motion smoother. Such operations can discard authored image-space information or smuggle perceptual judgment into deterministic tooling. Motion evidence remains advisory until a later explicit lossless transform contract is justified.

## SPP0 reuse

Successful repaired frames are converted to `SpriteProcessingFrameView` values and passed through the existing `AnalyzeSpriteProcessing` API. The resulting SPP0 report is attached as post-repair QA.

SPP2 therefore does not duplicate SPP0 alpha/bounds/color/duplicate/adjacent/pivot vocabulary.

## Deterministic serialization

`SerializeSpriteQualityResultJson` emits schema-versioned structural evidence in deterministic input/result order.

It records frame metrics, adjacent evidence, findings, repaired-frame structural metadata, ordered repair records, post-repair SPP0 JSON and diagnostics. Raw repaired pixel bytes remain available in the C++ result but are not duplicated into structural JSON.

Identical input bytes/order/options must produce byte-identical structural JSON.

## Complexity and allocation policy

SPP2 is explicit offline work:

- base frame evidence: `O(total pixels)`,
- pixel-grid analysis/repair: `O(total checked pixels)`,
- bounded simple palette membership/nearest search: `O(visible pixels * palette size)`, palette size `<= 256`,
- adjacent silhouette/RGBA comparison: `O(total comparable adjacent pixels)`,
- centroid evidence: folded into linear frame scans,
- repaired RGBA buffers: allocated only when a repair is explicitly requested,
- reusable temporary block storage is retained within the explicit repair operation.

Do not add a global palette cache/index, background worker framework, renderer readback, GPU dependency or runtime hot-path work until a measured later workload proves need.

## External reference decisions — 2026-08-12

### Aseprite Color / Color Bar / Color Mode

**ADOPT / ADAPT**

- explicit RGB vs indexed-color distinction,
- ordered indexed palette entry identity,
- explicit palette ownership as authoring data.

**REJECT**

- Aseprite editor state as Trace2D authority,
- silent inference that source content must use a particular palette.

### Aseprite CLI

**ADAPT**

- explicit palette conversion / color-mode / dithering controls as precedent that conversion policy should be caller-visible.

**REJECT**

- hidden dithering or implicit palette conversion in the SPP2 baseline.

### PNG Specification, Third Edition

**ADOPT / ADAPT**

- decoded truecolor/indexed-color and alpha remain distinct source semantics,
- SPP2 evaluates the decoded RGBA8 evidence supplied by Trace2D's existing asset path.

No new runtime or tooling dependency is introduced by SPP2.

## Acceptance gate

SPP2 remains draft/unmerged until one exact PR head proves:

1. explicit pixel-grid evidence and deterministic block repair/tie-break,
2. exact palette membership and bounded nearest-remap/tie-break with alpha preservation,
3. transactional repair failure beyond the explicit palette-distance bound,
4. exact rational pivot validation/mismatch and metadata-only normalization,
5. exact adjacent RGBA/mask and integer-sum centroid evidence,
6. threshold findings remain explicitly policy-labelled rather than aesthetic truth,
7. successful repairs reuse SPP0 post-repair analysis,
8. identical requests serialize byte-identically,
9. focused SPP2 tests compile/pass,
10. normal hosted Windows MSVC configure/build/full CTest and repository/contract audits are green,
11. no Sprite runtime/animation/render/GPU behavior changed, therefore no new real-GPU gate is required,
12. #162, `config/trace2d.core-lane.json`, this document and the implementation agree.

After all gates pass, mark the PR ready, merge it, confirm #162 closes, and stop. Do not begin SPP3 in that completion continuation.
