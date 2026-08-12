# Sprite SPP1 — Deterministic Alpha / Background / Frame Extraction

Status: **active via #156**  
Parent program: #59 / [`SPRITES.md`](SPRITES.md)  
Predecessor: SPP0 / #154 / PR #155 / squash `54d13db3c0547311afdbab25854212edc8226116`

SPP1 turns decoded RGBA8 source sheets into explicit owned frame pixels while preserving deterministic evidence. It is an offline processing stage layered on SPP0; it does not create a second QA model and does not enter runtime, animation or renderer paths.

## 1. Authority boundary

```text
decoded RGBA8 sheet + explicit SPP1 extraction specification
        -> explicit deterministic cleanup
        -> explicit rectangles / explicit uniform grid / alpha components
        -> exact expected-frame-count gate
        -> owned RGBA8 extracted frames + exact source rectangles
        -> existing SPP0 AnalyzeSpriteProcessing
        -> SPP0 schema-v1 QA evidence
        -> later SPP2 repair / perceptual review when needed
        -> canonical SpriteAsset only after later validation/import
```

Extraction pixels and manifests are derived offline content/evidence. They are not gameplay, animation, renderer or GPU authority.

## 2. Input and failure model

`ExtractSpriteFrames` consumes a protocol-independent `SpriteExtractionSheetView`:

- stable non-empty sheet ID,
- positive width/height,
- immutable decoded RGBA8 bytes with exact `width * height * 4` size.

Every `SpriteExtractionSpec` requires `expectedFrameCount > 0`.

Malformed dimensions/byte counts, invalid geometry, duplicate explicit frame IDs, arithmetic overflow, expected-count mismatch and trim-to-empty fail with structured diagnostics. A failed result contains no extracted frame set and no authoritative SPP0 report.

## 3. Exact cleanup rules

SPP1 supports only caller-selected deterministic cleanup:

- optional exact decoded RGB background key,
- optional alpha cutoff where `alpha <= cutoff` becomes transparent,
- optional RGB zeroing for pixels that were already fully transparent.

A pixel selected by an explicit cleanup rule becomes `{0,0,0,0}`. Pixels not selected by a rule remain byte-identical to the decoded source.

The RGB key is exact equality, not a distance/tolerance. SPP1 deliberately excludes fuzzy color matching, learned background removal, flood-fill tolerance, perceptual color-space comparison and VLM segmentation. Those would introduce a heuristic/perceptual authority that does not belong in this deterministic stage.

## 4. Extraction modes

### Explicit rectangles

The caller supplies ordered `SpriteExtractionRectView` values:

- stable unique non-empty frame ID,
- positive integer half-open source rectangle,
- rectangle fully inside the sheet.

Output order equals caller order exactly.

### Uniform grid

The caller supplies all geometry explicitly:

- origin x/y,
- cell width/height,
- rows/columns,
- horizontal/vertical spacing,
- row-major or column-major order.

Rows multiplied by columns must equal `expectedFrameCount`. Generated rectangles are checked with integer arithmetic and must fit the source sheet. No margin, padding, cell size, row count or column count is inferred.

Generated IDs are stable `sheet-id#frame-<ordinal>` strings in the requested output order.

### Alpha components

SPP1 may discover regions only through a mechanically defined alpha rule:

- cleanup is evaluated first,
- visible means post-cleanup `alpha > 0`,
- connectivity is exactly 4-neighbor connectivity,
- seed scan is row-major,
- neighbor discovery order is fixed,
- each connected component yields its exact bounding source rectangle.

A connected component is only extraction geometry. SPP1 does **not** claim that every component is an authored animation frame or semantic object.

Because connected components can fragment or merge based on source pixels, this mode always requires the same explicit `expectedFrameCount` gate. Mismatch fails rather than silently accepting an unintended segmentation.

## 5. Expected-frame-count gate

SPP1 never repairs frame count by guessing.

```text
planned_or_discovered_frame_count == expectedFrameCount
```

must hold before an extracted result becomes authoritative.

It does not:

- silently drop empty explicit/grid cells,
- merge/split components to satisfy the count,
- duplicate frames,
- synthesize missing frames.

For explicit rectangles and grid mode, the planned count is validated before pixel copy. For alpha components, the deterministic component scan completes and the discovered count must match before frame pixels are returned.

## 6. Optional exact trim

`trimToVisibleAlphaBounds` crops each planned frame to the smallest post-cleanup rectangle containing pixels with `alpha > 0`.

The output preserves the final absolute source rectangle so later import can recover the source-space offset exactly.

If trim is requested and a planned frame has no visible post-cleanup pixels, extraction fails with `empty_frame_after_trim`. Without trim, a positive-size empty frame remains valid output and SPP0 reports the existing `empty_frame` finding.

## 7. SPP0 remains the QA authority

After all output frames are successfully copied, SPP1 creates non-owning `SpriteProcessingFrameView` values over the owned outputs and invokes the existing `AnalyzeSpriteProcessing` API.

SPP1 therefore reuses SPP0 for:

- alpha/empty/bounds facts,
- edge contact and transparent RGB residue,
- color counts,
- dimension histograms,
- exact duplicate identity,
- adjacent changed-pixel and bounds-displacement evidence,
- stable typed findings.

SPP1 does not define parallel warning codes for those facts.

`SerializeSpriteExtractionResultJson` emits a schema-versioned deterministic structural envelope and embeds the existing SPP0 JSON report. Actual RGBA8 bytes remain directly available from the C++ result rather than being converted into a premature package/runtime format.

## 8. Determinism

For identical decoded bytes, IDs and extraction specification:

- cleanup bytes are identical,
- explicit/grid/component output order is identical,
- source rectangles are identical,
- extracted RGBA8 bytes are identical,
- SPP0 evidence is identical,
- serialized structural JSON is byte-identical.

Observable order never depends on pointer values or unordered-container iteration. Unordered containers may be used only for validation where the first reported failure follows deterministic input order.

## 9. Complexity / allocation

SPP1 is explicit offline work.

- explicit/grid planning is `O(frame_count)`,
- alpha-component discovery is `O(source_pixel_count)`,
- trim scans only the planned source rectangle,
- output copy is proportional to extracted output pixels,
- component visitation storage is bounded by source pixel count,
- output capacities are reserved from the explicit expected count where practical,
- no background worker framework, global extraction cache, renderer readback or GPU dependency is introduced.

No SPP1 work is added to `SpriteAnimator2D::Advance`, render extraction, normal frame submission or gameplay fixed-step execution.

## 10. External-reference decisions — 2026-08-12

- **W3C PNG Specification Third Edition** — **ADOPT/ADAPT** exact transparent-color and alpha semantics over decoded RGBA8; deliberately removed pixels normalize to transparent black.
- **SDL3 `SDL_SetSurfaceColorKey`** — **ADOPT/ADAPT** exact color-key equality as deterministic background-key precedent; **REJECT** SDL surface state as Trace2D processing authority.
- **Aseprite CLI / sprite-sheet export** — **ADAPT** explicit crop/trim, rows/columns, dimensions, ordering and padding controls; **REJECT** hidden sheet-layout inference.
- **Godot `Image` stable API** — **ADAPT** explicit rectangular region copy and non-zero-alpha used-rectangle semantics; **REJECT** Godot resource/runtime ownership.

These are design precedents, not new dependencies.

## 11. Acceptance gate

SPP1 is complete only when one exact PR head proves:

1. exact background-key / alpha-cutoff / transparent-RGB cleanup bytes,
2. explicit rectangle validation, order and exact copying,
3. grid geometry/order validation without inference,
4. deterministic 4-connected component extraction with row-major component order,
5. expected-frame-count mismatch produces no partial output,
6. trim preserves exact absolute source geometry and rejects trim-to-empty,
7. successful output is analyzed by the existing SPP0 API,
8. repeated identical requests produce byte-identical structural JSON,
9. focused tests and normal hosted Windows MSVC configure/build/full CTest plus repository audits are green,
10. runtime/animation/render/GPU paths remain unchanged, so no new real-GPU gate is required,
11. #156, this contract, `SPRITES.md`, `PROJECT_STATUS.md` and implementation agree.

After SPP1 merges green, the exact next Sprite child is **SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair**. SPP2 must not begin in the SPP1 PR.
