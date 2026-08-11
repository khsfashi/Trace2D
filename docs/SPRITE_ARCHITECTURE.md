# Sprite S0 Architecture Contract

Status: **frozen by #119 (S0)**  
Umbrella: #59  
Next Sprite child after S0: **S1 — canonical Sprite asset model and deterministic import representation**

This document is the implementation-facing architecture freeze for the Sprite program. `docs/SPRITES.md` remains the complete stage roadmap; this S0 contract fixes the authority, coordinate, presentation, integration, and verification seams that every later Sprite child must implement without reinterpretation.

## 1. Authority classes

Every Sprite datum belongs to exactly one authority class. A later implementation may cache or copy data, but it must not silently change the authority class.

| Data | Authority class | Owner | Renderer required? | Persisted/authored? |
|---|---|---|---:|---:|
| source/generator image | external input | importer/tooling | no | source artifact only |
| canonical `SpriteAsset` identity | authored | asset layer | no | yes |
| source size / trim / packed pixel rect / pivot / pack rotation | authored | `SpriteAsset` | no | yes |
| default sampling/color-space intent | authored | `SpriteAsset` | no | yes |
| `SpriteRenderer2D` semantic component fields | authoritative runtime | world/component layer | no | when authored by scene/component schema |
| `SpriteAnimator2D` clip/time/frame/event state | authoritative runtime | animation layer | no | configuration yes; running state normally no |
| `previous_fixed` / `current_fixed` transform | authoritative runtime | transform/world layer | no | current authored initial state only |
| presentation transform | presentation-derived | render extraction | yes for use, no for calculation tests | no |
| normalized UV | presentation-derived | render extraction | no | no |
| resolved texture/resource handle | presentation-derived | resource/renderer bridge | no | no |
| sampler/pipeline/material handle | presentation-derived | renderer backend | yes | no |
| contiguous batch/run identity | presentation-derived | renderer | yes | no |
| Agent inspection snapshot | explicit observation | Agent layer | no | evidence only |
| WorkResult verification record | explicit evidence | #98 verification | no | evidence |
| Workspace preview/review state | derived review | #99 Workspace | no | derived/evidence |
| generation/repair/QA intermediate | tooling | Sprite processing tools | no | explicit derived artifact only |

Hard rules:

- rendering never owns gameplay, transform, clip, frame, or event truth;
- a GPU resource failure never rewrites canonical Sprite metadata;
- Agent/MCP/JSON representations are observations/adapters, not a second Sprite model;
- external generator/importer formats are inputs, never runtime polymorphism;
- the future #71 world attaches the same typed `SpriteRenderer2D` / `SpriteAnimator2D` semantics; #59 must not create a Sprite-only entity graph.

## 2. Canonical coordinate system

S1 and all importers use one exact source-space convention.

### 2.1 Pixel rectangles

Authored image metadata uses integer pixel coordinates with:

```text
origin      = top-left of the untrimmed source image
+x          = right
+y          = down
rect        = [x, y, width, height]
rect bounds = half-open [x, x + width) × [y, y + height)
```

`width` and `height` are non-negative integer extents. A non-empty drawable region requires both to be positive.

### 2.2 Source size and trim

For a region:

```text
source_size   = untrimmed logical image size
trim_offset   = top-left of trimmed content in source space
trim_size     = trimmed content size
packed_rect   = stored rectangle on the source texture/atlas page
```

The logical source-space content rectangle is:

```text
[trim_offset.x,
 trim_offset.y,
 trim_size.x,
 trim_size.y]
```

and must fit inside `source_size`.

Trimming is storage metadata. It may remove transparent pixels from storage but must not change logical placement, pivot, animation alignment, or semantic bounds derived from the untrimmed source space.

### 2.3 Pivot

The canonical pivot is an exact source-space coordinate measured from the untrimmed source top-left under the same +x right / +y down convention.

The contract permits the pivot to lie outside the source rectangle when intentionally authored; import/validation must distinguish an out-of-bounds pivot from malformed non-finite or unrepresentable data. S1 chooses the exact numeric representation and validation diagnostics, but importers may not silently clamp a pivot.

Renderer geometry converts source-space Y-down metadata into Trace2D's world/render convention at one documented derivation boundary. Importers must not pre-flip coordinates differently per source format.

### 2.4 Rotated atlas storage

A `packed_rotation` value describes only how source pixels are stored on an atlas page. The first supported rotated form is a 90-degree quarter turn with exact documented direction in S1/SR2.

Storage rotation:

- does not modify `SpriteRenderer2D.rotation`,
- does not modify the canonical pivot,
- does not modify trim/source-space coordinates,
- is undone during derived UV/vertex mapping.

A rotated and unrotated packing of the same canonical source region must resolve to equivalent logical sprite geometry.

## 3. Canonical asset / runtime / renderer separation

The dependency direction is fixed:

```text
source files / external manifests
          |
          v
 deterministic import
          |
          v
 canonical SpriteAsset CPU data
          |
          +------------------------+
          |                        |
          v                        v
 authoritative components     explicit tooling/QA
 SpriteRenderer2D             import diagnostics
 SpriteAnimator2D             semantic inspection
          |
          v
 backend-independent render extraction
          |
          v
 resolved presentation data
          |
          v
 renderer/backend resources
```

Forbidden reverse dependencies:

- canonical assets may not contain SDL/GPU handles;
- canonical assets may not depend on renderer initialization;
- authoritative animation may not advance from render frame count or wall-clock presentation time;
- renderer extraction may not mutate authoritative clip/frame/transform state;
- importer code may not branch the runtime by origin tool (`Aseprite`, generator X, etc.).

## 4. Fixed-step transform and presentation history

Interactive smoothness and deterministic state are separate.

A transform that participates in Sprite presentation has logical samples:

```text
previous_fixed
current_fixed
```

`current_fixed` is authoritative. Gameplay and Agent semantic queries read it.

### 4.1 Successful fixed step

At the fixed-step boundary:

1. copy prior `current_fixed` to `previous_fixed`,
2. run the authoritative fixed update into `current_fixed`,
3. do not derive or commit an interpolated value back into either sample.

An aborted/not-committed simulation step must not advance history as though it succeeded.

### 4.2 Discontinuities

The following synchronize history (`previous_fixed = current_fixed`) unless an API explicitly requests another documented presentation behavior:

- initial scene/entity creation,
- reset,
- load/restore,
- teleport/warp,
- explicit snap/non-interpolated transform assignment.

This prevents one-frame smear from an intentionally discontinuous authoritative change.

### 4.3 Interactive presentation

Normal interactive rendering derives:

```text
presentation = interpolate(previous_fixed, current_fixed, alpha)
```

where `alpha` is supplied by the runtime presentation clock/accumulator, not stored as gameplay state.

- position: linear interpolation;
- non-uniform scale: linear interpolation;
- rotation: shortest signed 2D angular delta under one engine angle convention;
- booleans/discrete semantic fields (`flip_x`, `flip_y`, visibility, region/frame choice): never numerically blended;
- hierarchy after #71: interpolate local transform samples first, then compose the interpolated hierarchy.

`alpha` outside the documented presentation range is rejected or clamped by the owning presentation contract; callers do not silently extrapolate.

### 4.4 Exact-frame capture

Capture of authoritative simulation frame `N` defaults to:

```text
presentation_mode = authoritative_current
```

and renders `current_fixed` exactly, independent of wall-clock remainder.

A sub-frame capture is a different explicit request and must record its supplied interpolation alpha in artifact metadata. It cannot masquerade as exact-frame evidence.

## 5. `SpriteRenderer2D` semantic boundary

The future component is finite typed semantic state. At minimum the component domain must be able to express, as later stages implement them:

```text
SpriteAsset reference / region selection
visible
semantic transform attachment
flip_x / flip_y
tint / opacity
sampling intent
blend mode
layer / stable order
sorting-group membership
mask state
primitive mode (normal / 9-slice / tiled when those stages land)
```

The component stores authored/runtime intent, not:

```text
normalized UVs
GPU handles
SDL objects
pipeline objects
upload offsets
instance-buffer addresses
batch indices
draw indices
```

Those are resolved presentation data.

## 6. `SpriteAnimator2D` semantic boundary

Animation state is authoritative and renderer-independent.

Later SA stages refine exact types, but S0 fixes the separation:

```text
animation asset/set reference
active clip
canonical animation time
frame index
playing/paused/stopped state
loop/completion state
speed
ordered emitted semantic events
```

Rules:

- fixed simulation advances animation; render frames do not;
- the animation result selects semantic frame/region state consumed by renderer extraction;
- renderer failure does not roll animation backward or forward;
- headless stepping observes the same state that windowed presentation consumes;
- event order is deterministic and independent of hash/container iteration;
- SA0 chooses the exact integer/rational/fixed-point time representation and boundary rules before SA1 implements runtime state.

## 7. Resource seam for #86

Canonical Sprite references are typed project-relative CPU identities. #59 may implement Sprite loading/cache behavior required by its stages, but it must preserve the future resource contract:

```text
AssetRef<SpriteAsset> (authored identity)
      -> resolved generation-safe runtime resource identity
      -> renderer-owned GPU resources
```

Rules:

- absolute paths and traversal outside project root are invalid authored identity;
- normalized path/string identity is resolved outside hot loops;
- replacing/unloading GPU presentation resources does not rewrite the authored asset;
- exact pixel metadata stays canonical even if packaged texture format, mip chain, compression, residency, or atlas page representation differs;
- memory evidence separates Sprite metadata, retained CPU pixel bytes when retained, and renderer-owned GPU/page capacity.

#86 later generalizes this lifecycle without changing the Sprite authored schema.

## 8. View seam for #88

The Sprite renderer consumes a backend-independent resolved view structure rather than owning one permanent global camera.

Conceptually, Sprite extraction receives values equivalent to:

```text
resolved view identity
world-to-view / projection parameters
logical viewport extent
presentation target extent/rectangle
presentation/interpolation mode
pixel-perfect policy inputs when enabled
```

The exact public structure lands in the renderer stages, but hard rules are:

- Sprite assets/components do not store active OS window size as truth;
- world/screen math remains CPU-testable;
- exact-frame capture records view/camera identity and presentation mode;
- #88 can later supply `Camera2D`/`Viewport2D` without replacing Sprite semantics.

## 9. Material/pipeline seam for #89

Texture identity alone is not a permanent batch key.

Every extracted Sprite resolves a material/pipeline compatibility identity. During #59 the normal path may resolve to built-in Sprite pipelines. #89 extends the same seam for programmable `Material2D`/`Shader2D`.

Conceptual contiguous-run compatibility may include:

```text
texture/resource set
material/pipeline identity
sampler
blend state
mask/clip state
primitive mode
```

This identity answers only **whether adjacent work is compatible**. It never grants permission to globally reorder semantically ordered sprites.

## 10. Painter-order invariant

Semantic painter order is authoritative presentation intent.

```text
semantic ordering
 -> stable ordered extracted sequence
 -> merge compatible contiguous subsequences
 -> draw in the same semantic order
```

Forbidden optimization:

```text
all sprites
 -> global sort by texture/material/pipeline
 -> changed visual order
```

Sorting groups, when SR4 implements them, must define semantic ordering before batching. A group is not an excuse for unordered texture grouping.

## 11. Production texture semantics

S0 freezes ownership; later S1/SR3/packaging stages freeze exact encodings.

### 11.1 Color-space intent

Canonical Sprite texture metadata declares color-space intent explicitly. Importers may infer only when the importer contract names the inference and reports it. Renderer/backend conversion must not silently reinterpret identical canonical metadata differently per platform.

### 11.2 Alpha

Source files may arrive with different alpha conventions. Import must either reject ambiguity or convert to the canonical Sprite pixel convention at one explicit boundary. SR3 freezes the canonical runtime convention and proves backend blend compatibility before advertising blend modes.

Alpha conversion metadata/diagnostics are tooling/import facts; the renderer is not allowed to guess per instance.

### 11.3 Atlas/page packaging

Canonical region geometry remains independent of production page encoding. Atlas page metadata must leave room for future:

- compression format,
- mip policy,
- color-space encoding,
- package variant,
- residency/lifetime policy.

Do not freeze `one RGBA8, one mip, permanently resident page` as the authored asset model.

## 12. Headless and Agent observability

Every objective Sprite fact must have a non-pixel authority when practical.

Deterministic/structured observation includes, by stage:

- canonical asset IDs and pixel metadata,
- component visibility/region/flip/tint/order intent,
- authoritative transform,
- authoritative animation clip/frame/time/events,
- derived geometry/UV/bounds/batch-run facts on explicit debug request,
- import/QA measurements,
- resource validity and structural performance counters.

MCP is an adapter over protocol-independent Agent observations. The engine core does not contain JSON/MCP types merely to support Sprite inspection.

Observation/report allocation and full-scene scans are explicit-request work and are not required on normal frames.

## 13. Verification and review authority

Sprite work reuses #97-#99 rather than creating a new review system.

### Deterministic / machine-owned

Use deterministic or structured verification for facts such as:

- schema validity,
- source/trim/pivot geometry,
- region/frame references,
- animation timing and events,
- painter order and batch-run derivation,
- resolved bounds/UVs,
- exact-frame authoritative state,
- import/QA raw measurements,
- resource/capacity/workload counters.

Failure stays machine-owned and enters the normal:

```text
verify -> diagnose -> Agent/user repair -> re-verify
```

loop before subjective review.

### Multimodal / perceptual

Use capture/multimodal review only for genuine perceptual questions, for example:

- visual style match,
- readability,
- unwanted visual seams that are not fully characterized structurally,
- motion awkwardness,
- subjective generated-art quality.

A screenshot cannot override a deterministic failure.

### Human

Human judgment owns taste, creative direction, and final subjective approval through the existing Workspace/WorkResult flow.

## 14. Offline-only boundary

The following are explicit authoring/import/QA work and must not become ordinary per-frame behavior:

- generation-provider calls,
- image decode/import normalization beyond normal asset load,
- frame segmentation,
- alpha/background cleanup,
- trim/pivot analysis,
- palette/grid/identity/motion QA,
- atlas packing,
- canonical serialization/report generation,
- full semantic inspection snapshots,
- capture/readback,
- performance report aggregation.

Runtime may consume their committed/cached results; it does not redo them every frame.

## 15. Versioning and diagnostics

S1 defines the first concrete Sprite authored schema version. All future Sprite authored schemas follow these rules:

- explicit schema/version field;
- reject unsupported future versions rather than guessing;
- stable semantic IDs independent of pointer, allocation, hash iteration, or GPU handle;
- validation errors identify asset plus field/region/frame/clip target where possible;
- import diagnostics distinguish source-format error, unsupported feature, canonical validation failure, and deterministic processing/QA warning;
- canonical serialization/deterministic ordering is required wherever the tool publishes text intended for diff/replay identity;
- migration belongs to explicit tooling and later #79 common persistence/migration policy, never implicit normal-frame mutation.

## 16. Importer/interoperability boundary

External formats terminate at import:

```text
Aseprite / sheet / loose frames / external manifest / generated output
        -> source adapter
        -> canonical validation
        -> SpriteAsset + diagnostics
```

Runtime code does not ask which importer created an asset.

SPP3/SPP4 may preserve provenance in tooling metadata for diagnostics/reproduction, but provenance is not runtime behavior dispatch.

## 17. Scope handoffs

S0 deliberately does not implement these later systems:

- #71: general scene hierarchy and registered game component composition;
- #86: common typed resource lifecycle;
- #88: `Camera2D` / `Viewport2D`;
- #89: programmable `Material2D` / `Shader2D`;
- #60: arbitrary textured indexed/deformable `Mesh2D` geometry;
- #61/#101: Spine/skeletal/deformable runtime decision behind explicit license/product gate;
- general lighting, render graph, PBR, material graph, generic reflection, generic ECS, custom allocator/job framework.

The seams above are compatibility requirements, not authorization to implement those systems early.

## 18. S0 invariants carried into every later Sprite child

1. **CPU authored/authoritative truth remains renderer-independent.**
2. **Exact source-space pixel metadata is canonical; normalized UV/GPU state is derived.**
3. **`current_fixed` is gameplay truth; interpolation is presentation only.**
4. **Exact-frame capture defaults to authoritative current state.**
5. **Sprite components are future world components, not a Sprite-only entity model.**
6. **Material/view/resource seams are extensible without changing Sprite authored truth.**
7. **Painter order cannot be traded away for batching.**
8. **Normal steady-state rendering does not perform tooling scans or import/repair work.**
9. **Deterministic verification outranks pixels; multimodal review is only for perceptual facts.**
10. **External generators/importers end at canonical conversion.**

A later child that needs to violate one of these invariants must stop and explicitly supersede S0 through an owner-reviewed architecture change rather than silently drifting.
