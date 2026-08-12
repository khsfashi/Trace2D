# Sprite SPP4 Generator Manifest Interoperability Contract

Status: **active via #166**.  
Parent: #59. Predecessor: SPP3 #164 / PR #165 / squash `926993ace6d020e00e3d4565d0ffacff866ee252`.  
Exact next after green merge: **SPP5 — provider-neutral generation orchestration**.

## Purpose and authority

SPP4 is a deterministic offline adapter layer for maintained generator/export manifests. It does not add a generation runtime and it does not create another canonical Sprite importer.

```text
explicit provider manifest + separately decoded RGBA8 atlas
 -> explicit finite SPP4 adapter kind
 -> ordered generic SPP3 regions + animation evidence
 -> ImportGenericSpriteSheet
 -> existing S1 serialize/parse canonical validation
 -> deterministic SPP4 structural evidence
```

Provider formats remain inputs only. Canonical S1 `SpriteAsset` remains authored/runtime asset truth, SA0-SA4 remains runtime animation authority, and SPP5 owns future live provider-neutral generation orchestration.

`SpriteGeneratorInterop.hpp` exposes:

- `SpriteGeneratorManifestKind::SpriteGenComponentRow`,
- `SpriteGeneratorManifestKind::PerfectPixelV2`,
- `ImportSpriteGeneratorManifestJson(...)`,
- deterministic `SerializeSpriteGeneratorImportResultJson(...)`.

The caller selects the manifest kind explicitly. SPP4 never guesses a provider from weak JSON fields.

## Common lowering contract

Both adapters first build ordered offline metadata and only then call the existing SPP3 `ImportGenericSpriteSheet(...)` explicit-region mode. The SPP3 result is therefore still revalidated through S1 serialization/parsing before SPP4 can succeed.

Stable generated region IDs use:

```text
<animation-or-state-name>/frame-<playback-ordinal>
```

Playback ordinals remain distinct even when the provider manifest intentionally repeats one atlas rectangle. This preserves the authored playback sequence without inventing pixel identity or deduplicating semantic slots.

Each successful SPP4 result records ordered animation evidence:

- name,
- explicit row,
- first imported frame,
- frame count,
- declared FPS,
- loop flag.

Per-frame exact duration remains in the existing SPP3 `SpriteImportedFrame::durationNanoseconds` evidence. No second animation state machine is created.

## sprite-gen component-row manifest

Reference snapshot: `aldegad/sprite-gen` commit `88f2ea17cac2ef066536beee7e3f40b2f8d29c87`, skill version `1.59.0`.

The current runtime manifest is produced after deterministic extraction/curation/compose and contains separate layout and animation blocks. SPP4 consumes only the bounded runtime metadata needed for canonical import:

- `engine == "component-row"`,
- `game_input` exactly matching the supplied decoded atlas ID,
- `degraded_static_fallback == false`,
- `frame_layout.sheetWidth/sheetHeight/cellWidth/cellHeight`,
- `frame_layout.rows.<state>[]` absolute atlas rectangles,
- `animation.rows.<state>.row/frames/fps/durations_ms/loop`.

`frame_layout.rows` may repeat one rectangle for multiple playback instances because sprite-gen can reuse a baked atlas cell for cloned/identical playback slots. SPP4 preserves those as distinct ordered canonical region entries pointing at the same packed rectangle.

The explicit `row` field owns state order. JSON object/hash iteration does not. Rows must be unique and contiguous from zero, and the layout/animation state sets and frame counts must agree exactly.

Current sprite-gen runtime layout rectangles are fixed-cell rectangles, so their width/height must match `cellWidth/cellHeight` and they must remain inside the decoded sheet.

The current runtime manifest does not author S1 source-trim or pivot metadata. Therefore baseline conversion is deliberately exact and conservative:

```text
source_size = [rect.w, rect.h]
trim_offset = [0, 0]
trim_size   = [rect.w, rect.h]
packed_rect = manifest rect
pivot       = explicit caller-provided rational pivot
```

The pivot is mandatory. SPP4 never scans alpha or guesses a foot/center pivot.

Each `durations_ms` value must be a positive integer and converts with checked integer arithmetic only:

```text
duration_ns = duration_ms * 1,000,000
```

Curation state, raw prompts/images, chroma policy, breathe metadata, rig/layer editor state and provider execution are not imported as runtime authority.

## PerfectPixel manifest v2

Reference snapshot: `gykim80/perfectpixel-studio` commit `a1385cc99eb4b6983c945adb8cca7b2b71f53d0f`.

Supported identity is explicit:

- `app == "perfectpixel"`,
- `schema == "perfectpixel.sprite/2"`,
- `version == 2`.

The supplied decoded atlas ID/dimensions must match `sheet.image/width/height`; `cellWidth/cellHeight` must be positive.

Each `animations.<name>` entry supplies:

- `row`, `frames`, `fps`, `loop`, `durationMs`,
- integer `pivot.{x,y}`,
- full-cell absolute `rects[]`,
- cell-local visible-content `trims[]`.

Animation order is explicit `row`, never map iteration. Rows must be unique/contiguous and `frames == rects.size == trims.size > 0`.

PerfectPixel copies each complete frame cell into the atlas, while `trims` is local content-bounds metadata. To preserve S1 trim semantics without copying pixels, SPP4 derives the canonical packed content rectangle:

```text
source_size = [cellWidth, cellHeight]
trim_offset = [trim.x, trim.y]
trim_size   = [trim.w, trim.h]
packed_rect = [cell.x + trim.x, cell.y + trim.y, trim.w, trim.h]
pivot       = [pivot.x, pivot.y] / 1
```

The full cell must fit the sheet, trim must be non-empty and fit the cell, and the derived packed content must fit the sheet. Empty-frame trim is rejected rather than silently inventing canonical drawable geometry.

`durationMs` is retained as the exact positive frame duration and converted to integer nanoseconds. `fps` and `loop` remain explicit offline animation evidence.

## Failure and determinism

Adapter parsing is transactional. Until all provider metadata validates, no canonical S1 result or partial animation evidence is published. Canonical lowering failure clears SPP4 animation evidence and leaves the existing SPP3 diagnostics in `canonicalImport`.

Hard failure includes at least:

- malformed/non-object JSON,
- unsupported provider/schema/version/engine,
- manifest image or dimensions not matching decoded atlas,
- invalid/empty cell dimensions,
- missing/invalid layout or animation rows,
- state-set/count mismatch,
- duplicate/gapped row identities,
- invalid/out-of-page rectangles,
- invalid/empty PerfectPixel trim,
- missing/invalid explicit sprite-gen pivot,
- zero/non-integer/overflow duration,
- existing SPP3/S1 canonical validation failure.

Identical adapter kind + manifest bytes + decoded atlas bytes/metadata + options produce field-identical results and byte-identical schema-versioned JSON evidence. Observable ordering comes only from explicit row/frame order, never unordered container iteration.

## Performance and ownership

SPP4 is offline/setup-only:

- JSON parse/planning is `O(manifest bytes + animation count + frame count)`,
- explicit row ordering is `O(animation count log animation count)`,
- SPP3 lowering is `O(frame count)` plus existing S1 metadata validation,
- decoded RGBA8 bytes are viewed for validation rather than recopied merely for manifest adaptation,
- only bounded metadata vectors are created for the explicit import request.

No fixed-step/render/animation hot-path work, renderer/GPU dependency, filesystem/network provider invocation, background worker, provider cache or new package is introduced. Existing `nlohmann-json` and SPP3/S1 are reused.

## External reference decisions — 2026-08-12

- `aldegad/sprite-gen` component-row runtime manifest — **ADOPT/ADAPT** explicit atlas identity, absolute frame-layout rectangles, explicit row ordering, per-frame millisecond durations and loop metadata; **REJECT** generation/curation/editor/runtime dependency.
- `gykim80/perfectpixel-studio` `perfectpixel.sprite/2` — **ADOPT/ADAPT** explicit sheet identity/dimensions, row/frame metadata, local trim, integer pivot, duration/FPS/loop; **REJECT** PerfectPixel pipeline/editor/runtime ownership.
- Trace2D SPP3/S1 — **ADOPT** as the only canonical conversion and validation authority.

No new real-GPU gate is required because SPP4 changes no presentation path.

After SPP4 merges green, stop. Begin SPP5 only in the following continuation.
