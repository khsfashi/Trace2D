# Sprite SPP0 — Deterministic Offline Processing / QA Report

Status: **active via #154 / draft PR #155**  
Parent program: #59 / [`SPRITES.md`](SPRITES.md)

SPP0 establishes the deterministic evidence contract consumed by later Sprite extraction, repair, import and generation stages. It measures decoded pixels and explicit metadata; it does not mutate source pixels or canonical Sprite assets.

## 1. Authority boundary

```text
decoded RGBA8 pixels + explicit frame/page metadata
        -> deterministic SPP0 raw measurements
        -> typed rule-based findings
        -> later SPP1/SPP2 decisions
        -> canonical SpriteAsset only after explicit validation
```

The report is derived offline evidence, never gameplay/render/animation authority. A warning cannot silently repair pixels or rewrite canonical metadata. Perceptual style/readability/motion quality remains advisory multimodal/human review where deterministic evidence is insufficient.

SPP0 introduces no renderer/GPU behavior and no work in `SpriteAnimator2D::Advance`, render extraction, normal frame submission or gameplay fixed-step execution.

## 2. Protocol-independent input

The C++ analyzer consumes bounded in-memory views:

- stable frame ID,
- positive width/height,
- immutable RGBA8 bytes with exact `width * height * 4` size,
- optional exact `SpriteRationalPivot`,
- optional explicit atlas page size and packed rectangles.

The analyzer requires no filesystem, renderer or GPU. `trace2d_sprite_process` is a separate offline CLI adapter that uses the existing project-relative `TextureAssetCache` to decode source images before invoking the same analyzer.

Malformed dimensions/byte counts, duplicate/empty semantic IDs and arithmetic/report-size overflow fail before an authoritative report is returned.

## 3. Alpha / visibility semantics

SPP0 operates on decoded RGBA8 values.

- `alpha == 0`: fully transparent,
- `0 < alpha < 255`: partially transparent,
- `alpha == 255`: fully opaque,
- visible-alpha bounds include exactly pixels with `alpha > 0`,
- bounds are integer half-open `SpritePixelRect` values,
- an all-zero-alpha frame has no visible bounds and is `empty`.

This adopts the PNG alpha meaning and adapts Godot's useful non-zero-alpha used-rectangle concept without importing either source format/runtime architecture.

Fully transparent pixels with non-zero RGB are reported as residue evidence. SPP0 does not decide that such RGB data must be erased; later processing may need it for filtering/authoring workflows.

## 4. Exact per-frame measurements

Each frame reports:

- width / height / exact pixel count,
- fully transparent / partially transparent / fully opaque counts,
- optional visible-alpha bounds,
- empty flag,
- visible-pixel counts touching left/top/right/bottom source edges,
- transparent-RGB-residue pixel count,
- unique RGBA color count,
- unique visible RGB color count,
- optional exact pivot tuple.

Color counts are measurements only. SPP0 does not automatically quantize palettes.

## 5. Frame-set measurements

The report preserves input frame order and adds stable sorted histograms for dimensions and pivots.

It reports:

- whether frame dimensions are uniform,
- exact dimension histogram,
- exact pivot histogram when pivots are supplied,
- byte-identical duplicate groups,
- adjacent-frame changed-pixel counts for equal dimensions,
- adjacent visible-alpha-bounds origin deltas when both frames are visible,
- explicit grid evidence only when `gridColumns` is supplied.

Duplicate identity is exact: dimensions and full RGBA8 bytes must compare equal. SPP0 does not use a probabilistic hash as proof of identity.

Adjacent changed-pixel count is the number of pixel locations where any RGBA channel differs. It is raw motion/change evidence, not a claim about animation quality or object identity.

Grid evidence records columns, derived row count, completeness and uniform cell dimensions. It never discovers or invents segmentation. Actual extraction belongs to SPP1.

## 6. Atlas/page measurements

Given explicit page/rectangle metadata, SPP0 reports:

- page dimensions and exact page area,
- packed rectangle count,
- sum of packed rectangle areas,
- utilization as exact integer numerator / denominator,
- out-of-bounds rectangle count,
- overlapping rectangle-pair count.

`SpritePackedRotation` is storage metadata and does not alter occupied pixel area.

The initial overlap check is deliberately bounded offline `O(rect_count^2)`. It is not a renderer/runtime algorithm and does not justify per-frame overlap work.

## 7. Findings are not raw facts

A finding has stable:

- severity: `info | warning | error`,
- code,
- primary/secondary semantic IDs,
- two integer evidence values,
- deterministic message.

Initial codes:

```text
empty_frame
visible_touches_edge
transparent_rgb_residue
inconsistent_dimensions
pivot_inconsistent
duplicate_frame
adjacent_no_change
bounds_displacement
atlas_out_of_bounds
atlas_overlap
low_atlas_utilization
```

`bounds_displacement` exists only when an explicit maximum-pixel threshold is supplied. `low_atlas_utilization` exists only when an explicit exact-ratio threshold is supplied. The analyzer does not smuggle subjective thresholds into defaults.

Atlas out-of-bounds/overlap are structural errors. Other findings remain measurements/rules whose importance depends on the content task.

## 8. Determinism / serialization

Report schema version begins at `1`.

Identical frame/page bytes, metadata, ordering and options must produce field-identical and byte-identical JSON.

Observable ordering never depends on pointer values or unordered-container iteration. Integer values are authoritative; atlas utilization is serialized as exact integer numerator/denominator rather than a floating approximation.

JSON serialization is explicit post-analysis tooling work.

## 9. Complexity / allocation

- per-frame pixel measurement: `O(pixel_count)`,
- adjacent equal-dimension diff: `O(pixel_count)` per adjacent pair,
- exact duplicate grouping: bounded offline pairwise equality in the initial implementation,
- atlas overlap: `O(rect_count^2)` offline,
- reporting/JSON may allocate because it is explicit tool work,
- source pixel buffers are viewed, not duplicated merely for analysis,
- temporary hash/set capacity is bounded/reused within the analysis operation where practical.

No general worker system, allocator, GPU readback or background report cache is introduced.

## 10. CLI adapter

`trace2d_sprite_process` accepts one or more project-relative images and emits the schema-versioned JSON report to stdout.

Current options:

```text
--project-root <path>
--grid-columns <count>
--max-bounds-displacement <pixels>
--require-uniform-pivot
```

The CLI's decoded image inputs do not carry authored pivot metadata; the protocol-independent analyzer supports pivots for later SPP stages/importers.

Atlas views are currently exercised through the analyzer API/tests. Later Sprite import/package tooling can pass explicit atlas metadata without changing the report authority model.

## 11. External-reference decisions — 2026-08-12

- **W3C PNG Specification Third Edition** — ADOPT alpha-zero / positive-alpha semantics; ADAPT encoded format details into decoded RGBA8 facts.
- **Godot `Image` stable docs** — ADAPT `get_used_rect()`-style non-zero-alpha bounding evidence; REJECT Godot runtime/resource ownership.
- **Aseprite official file format / Sprite docs** — ADOPT/ADAPT explicit dimensions, frame, palette and grid metadata as authoring precedent; DEFER `.aseprite` importer implementation to SPP3.
- **Aseprite sprite-sheet docs** — ADAPT explicit offset/frame-size/padding/order as evidence that extraction parameters must be explicit; REJECT silent segmentation inference in SPP0.

These are design references, not Trace2D runtime dependencies.

## 12. Acceptance gate

SPP0 is complete only when one exact PR head proves:

1. in-memory frame/atlas analysis produces the committed exact metrics,
2. focused deterministic tests cover alpha/bounds/edge/residue/color/dimension/pivot/identity/motion/grid/atlas behavior,
3. duplicate identity requires exact RGBA8 equality,
4. raw facts and findings remain separate and option thresholds are explicit,
5. malformed input fails without partial authoritative output,
6. repeated identical reports serialize to byte-identical schema-v1 JSON,
7. ordinary Sprite runtime/animation/render paths receive no SPP0 work,
8. hosted configure/build/full tests and repository audits are green,
9. #154, this contract, `SPRITES.md`, `PROJECT_STATUS.md` and implementation agree,
10. no new real-GPU gate is required because no presentation/GPU path changes.

After merge, the exact next Sprite child is **SPP1 — deterministic alpha/background/frame extraction**. SPP1 must not begin inside PR #155.
