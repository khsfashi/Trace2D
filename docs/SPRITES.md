# Sprite Pipeline Contract

Status: **owner-approved future contract; not implemented yet**

This document defines the post-particle Sprite program for Trace2D. It is intentionally broader than a minimal `SpriteRenderer` or frame sequencer. The target is an agent-verifiable pipeline where a coding agent can move from source pixels or generated images to a canonical Trace2D sprite asset, deterministic animation, production-grade rendering, structured QA, performance evidence, and final motion/visual validation.

Operational umbrella: GitHub Issue #59.

The owner-fixed predecessor order remains:

```text
#41 renderer workloads
  -> #47 -> #48 -> #49 -> #50 -> #51 -> #52 -> #53 particles
  -> #59 Sprite pipeline
  -> #60 Mesh2D foundation
  -> #61 Spine license gate / optional integration
```

Do not begin this program while an earlier owner-fixed item is incomplete.

## 1. Product goal

Trace2D should eventually support this complete loop:

```text
sprite request / source image / external sheet
        |
        v
optional provider generation
        |
        v
raw source pixels
        |
        v
deterministic import / cleanup / normalization
        |
        v
machine-readable sprite QA
        |
        v
canonical Trace2D SpriteAsset
        |
        v
deterministic SpriteAnimator2D
        |
        +---- headless inspect / query / step / assert
        |
        v
production sprite renderer
        |
        v
capture / motion QA / visual QA
        |
        v
reproducible performance evidence
```

An image model's output is never authoritative engine state. Generated pixels become usable only after explicit import and validation.

## 2. Core ownership model

The pipeline is split into four layers with deliberately different responsibilities.

### 2.1 Source and generation layer

Inputs may include:

- one PNG/JPEG/BMP/TGA image,
- loose transparent frame PNGs,
- a regular sprite sheet,
- an atlas plus metadata,
- Aseprite-exported metadata,
- sprite-gen-style manifests,
- PerfectPixel-style manifests,
- output from an external image-generation provider,
- output from a custom user command/script.

These formats are inputs only. None becomes a permanent runtime API.

### 2.2 Offline deterministic processing layer

This layer may perform expensive setup-time work such as:

- background/alpha cleanup,
- frame segmentation,
- trim analysis,
- pivot/centroid analysis,
- pixel-grid detection and optional repair,
- palette analysis/quantization,
- identity/motion quality measurement,
- atlas packing,
- manifest conversion,
- deterministic QA report generation.

This work must not leak into the ordinary frame loop.

### 2.3 Canonical authored/runtime layer

Trace2D owns one canonical sprite representation independent of the source tool. Authored/source-of-truth geometry should use exact integer pixel metadata where possible.

Conceptual types include:

```text
SpriteAsset
SpriteRegion
SpriteAtlas / SpriteAtlasPage
SpriteAnimationSet
SpriteClip
SpriteFrame
SpriteEvent
SpriteRenderer2D state
SpriteAnimator2D state
```

Names are not frozen until S0/S1 implementation issues finalize public C++/TOML schemas, but the semantic responsibilities in this document are owner-approved.

### 2.4 Renderer presentation layer

The renderer consumes derived presentation data only. Normalized UVs, GPU handles, packed instance records, batch runs, and backend resources are derived renderer state and are never the authored truth.

The renderer must not own authoritative animation/gameplay state.

## 3. Determinism boundary

Trace2D distinguishes nondeterministic creation from deterministic processing and runtime behavior.

### Allowed nondeterminism

- remote/local image generation models,
- human curation choices,
- optional human art edits.

### Required deterministic behavior for identical committed inputs/configuration

- canonical import metadata,
- trim and region conversion,
- atlas layout when using Trace2D's deterministic packer,
- deterministic repair algorithms where the contract claims determinism,
- QA metric calculation,
- animation time/frame/event progression,
- semantic Agent inspection and assertions,
- headless test fixtures,
- backend-independent sprite geometry extraction,
- ordered batch-run derivation.

Visual GPU output is validated according to the renderer's documented numeric/presentation contract. Do not claim cross-vendor bit-identical pixels unless a specific path actually proves that property.

## 4. Canonical SpriteAsset requirements

S0 and S1 must establish a strict, versioned, text/diff-friendly authored contract.

A canonical region must be able to represent at least:

- stable semantic region ID/name,
- atlas page or source texture identity,
- packed integer pixel rectangle,
- original untrimmed source size,
- trimmed content size,
- trim offset within original source space,
- pivot in a precisely defined source-space coordinate convention,
- whether an atlas packer stored the source rotated by 90 degrees,
- default sampling/pixel-art intent where appropriate.

A canonical animation frame must be able to reference a region and define deterministic display duration. Clip semantics must support at least:

- stable clip ID/name,
- ordered frames,
- per-frame duration or an equally exact canonical timing representation,
- loop mode,
- ordered semantic events.

### Exact metadata rule

Prefer integer source-pixel data for authored geometry. Do not persist normalized floating UVs as the only truth.

For example:

```text
source_size = [64, 64]
packed_rect = [128, 64, 20, 44]
trim_offset = [22, 14]
pivot        = [32, 58]
```

The renderer derives normalized UVs from the loaded texture size.

### Trim invariant

Trimming is an atlas/storage optimization. It must not silently change the sprite's original coordinate space, visible placement, pivot, animation alignment, or gameplay-authored semantics.

### Rotated-packing invariant

If the atlas packs a region rotated, rendering must reconstruct the intended source orientation. Packed orientation is storage metadata, not gameplay transform.

## 5. Fixed implementation order inside #59

When #59 becomes active, agents create and complete exactly one child issue/PR at a time in the following order. No phase may be skipped because a later feature appears more attractive.

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The child issue should copy the relevant section's acceptance criteria and narrow them to one testable PR. `PROJECT_STATUS.md` must record the currently active child once the umbrella begins.

# 6. S0 — Sprite architecture and contract

S0 freezes module boundaries and the initial authored/runtime vocabulary before implementation grows.

Acceptance criteria:

- define the canonical asset/runtime/render separation,
- define integer source-space coordinate conventions,
- define pivot origin/axis rules,
- define trim and rotated-atlas semantics,
- define animation timing/event semantics at contract level,
- define what is authoritative and what is renderer-derived,
- define headless observability requirements,
- define which expensive operations are offline-only,
- define schema/versioning/diagnostic strategy,
- define compatibility/importer boundaries,
- update architecture documents before broad renderer changes.

S0 is a contract PR; it must not opportunistically implement the entire renderer.

# 7. S1 — Canonical sprite asset model

S1 implements the smallest complete canonical data model needed by all later stages, but it is not a minimal rendering milestone.

Acceptance criteria:

- strict versioned authored representation,
- stable project-relative asset identity compatible with the existing asset cache rules,
- immutable/explicitly owned imported sprite metadata,
- exact validation for rectangles, sizes, pivots, frame references and durations,
- deterministic ordering and canonical serialization where supported,
- stable machine-readable diagnostics,
- no GPU handles/SDL types in the asset model,
- automated malformed/boundary/round-trip tests.

# 8. Production-complete Sprite Renderer

The SR0-SR8 sequence is intentionally **not** a minimal renderer. It targets the traditional sprite-presentation functionality expected by practical 2D games while preserving Trace2D's measured, deterministic, agent-observable architecture.

The complete target includes:

- standalone textures and atlas regions,
- trim/source-size/offset preservation,
- pivot,
- position/rotation/non-uniform scale,
- semantic flip X/Y,
- tint and opacity,
- nearest and linear sampling,
- explicit alpha convention,
- normal/additive/multiply/screen blend modes when proven on the backend contract,
- layer/stable order,
- sorting groups,
- sprite masking/clipping suitable for conventional 2D presentation,
- 9-slice,
- tiled/repeated sprite presentation,
- runtime pixel-perfect presentation,
- painter-order-preserving batching,
- persistent resource reuse,
- explicit renderer metrics and reproducible workloads.

## 8.1 SR0 — Renderer contract and asset/render separation

Acceptance criteria:

- define the complete derived `SpriteRenderData` requirements for the staged renderer,
- keep authored pixel metadata separate from normalized UV/GPU data,
- define semantic vs derived transform state,
- define batch compatibility keys without allowing them to reorder painter order,
- define alpha/sampler/blend/mask state ownership,
- define which calculations are backend-independent and headless-testable,
- preserve the rule that renderer output is presentation/visual-QA state.

## 8.2 SR1 — Complete transform and geometry semantics

Support and test:

- position,
- rotation,
- independent X/Y scale,
- pivot,
- semantic `flip_x` / `flip_y`,
- authored sprite size/source-space reconstruction,
- combinations of rotation + non-uniform scale + pivot,
- combinations of trim + pivot + flip,
- negative-scale behavior if accepted by the contract,
- parent/local transform interaction when the scene model reaches that requirement.

Prefer explicit semantic flip state over forcing authored users/agents to encode flip only as negative scale. Internal optimization may collapse equivalent math after semantics are resolved.

Backend-independent geometry math must be tested without initializing a GPU.

## 8.3 SR2 — Atlas region, trim, pivot and rotated packing

Acceptance criteria:

- arbitrary atlas sub-rect UV derivation,
- exact trim reconstruction,
- original source-size placement preservation,
- pivot preservation in original source space,
- 90-degree packed-region support if the atlas contract enables it,
- atlas page selection,
- validation against out-of-bounds/overlapping-invalid metadata as appropriate,
- conformance fixtures proving trimmed/untrimmed and rotated/unrotated equivalents render/resolve to the same intended geometry.

## 8.4 SR3 — Color, alpha, blend and sampling

### Color

Support explicit RGBA tint and opacity semantics. The implementation may combine them internally, but public authored/Agent semantics must remain unambiguous.

### Alpha

The project must document whether canonical runtime pixels are straight alpha, premultiplied alpha, or converted at a specific boundary. Importers may accept multiple source conventions only when conversion is explicit and tested.

### Blend modes

Target conventional sprite modes:

- normal,
- additive,
- multiply,
- screen.

Do not advertise a mode until the SDL3 GPU backend path and conformance fixture validate it.

### Sampling

Support nearest and linear sampling using a small renderer-owned cache/set of sampler states. Do not create one GPU sampler object per sprite instance.

## 8.5 SR4 — Painter order, sorting groups and sprite masking

Trace2D keeps semantic painter order authoritative. Texture/material/batch identity never becomes a global sorting key.

### Order

Retain explicit layer plus deterministic stable order. Add a sorting-group concept only with precise semantics so multi-sprite objects such as characters can remain internally ordered without interleaving unexpectedly with neighboring objects.

### Masking

Provide a deliberately bounded conventional sprite mask/clip contract, suitable for cases such as portraits, body/weapon masking, gauges and effects. Stencil or equivalent renderer implementation is acceptable if hidden behind Trace2D-owned semantics.

Do not turn this stage into an arbitrary compositing graph.

Acceptance tests must cover order/group/mask interactions and deterministic failure behavior.

## 8.6 SR5 — 9-slice and tiled sprite primitives

### 9-slice

Represent explicit left/right/top/bottom border metadata. Resizing must preserve corner/border intent while stretching/repeating only contracted regions.

### Tiled/repeated sprites

Support repeated presentation without accidentally sampling neighboring atlas regions. An atlas-region tile contract may require generated geometry or explicit UV handling instead of naïvely enabling whole-texture repeat wrap.

Use the same texture/asset identity and renderer resource lifetime system; do not create a second unrelated quad renderer for these primitives.

## 8.7 SR6 — Pixel-perfect runtime presentation

This stage handles **runtime presentation**, not fake-pixel-art repair.

Define a precise mapping among:

- source sprite pixels,
- pixels-per-unit or equivalent world scale,
- orthographic camera,
- render-target resolution,
- camera/sprite translation,
- final presentation scale.

The contract must specify guaranteed and non-guaranteed cases for:

- integer upscale,
- window sizes that do not divide evenly,
- letterbox/pillarbox policy,
- camera movement,
- sprite movement,
- rotation,
- non-integer scale.

Do not implement pixel-perfect rendering as an unexplained `round(position)` heuristic.

Provide reproducible fixtures for camera and sprite movement so agents can detect one-pixel jitter or unintended interpolation.

## 8.8 SR7 — Production batching and resource lifetime

Preserve the strengths of the existing renderer:

- caller/semantic order is never globally texture-sorted,
- compatible contiguous runs may batch,
- persistent/capacity-reused GPU/upload resources,
- resource growth is measured and geometric where appropriate,
- ordinary steady-capacity frames avoid frame-list heap churn,
- texture/handle lookup remains direct and predictable.

The future batch key may include fields such as:

```text
texture
pipeline/blend mode
sampler
mask state
```

but the renderer may merge only compatible **contiguous** work unless a separately proven order-preserving scheme is intentionally adopted.

Do not prematurely compress instance structures because they grew beyond the Public Alpha 16-byte transform record. First measure:

- CPU extraction/packing time,
- upload bytes/frame,
- instance count,
- draw count,
- GPU time where available,
- retained capacities,
- memory footprint.

Only measured evidence may justify packed colors, half precision, static/dynamic split, alternative buffer layouts, bindless approaches, or other added complexity.

## 8.9 SR8 — Renderer conformance and workloads

The renderer is not complete merely because screenshots look plausible.

Maintain committed fixtures for at least:

- pivot,
- trim,
- source-size reconstruction,
- flip,
- rotation/non-uniform scale,
- rotated atlas region,
- tint/opacity,
- each shipped blend mode,
- nearest/linear sampling behavior where testable,
- sorting/sorting-group behavior,
- masking,
- 9-slice,
- tiled sprite,
- pixel-perfect presentation,
- painter-order-preserving batch-run derivation.

Prefer backend-independent assertions on derived geometry/UV/order/batch state. Use explicit windowed capture only for behavior that genuinely depends on rendered pixels.

Workloads must report raw metrics rather than vague claims such as "fast" or "low CPU".

# 9. Deterministic Sprite Animation

Animation is authoritative runtime state independent of renderer initialization.

## 9.1 SA0 — Deterministic timing/frame/event contract

Before implementing `SpriteAnimator2D`, specify:

- animation time representation,
- fixed-step integration,
- exact boundary behavior when duration is not an integer multiple of simulation step,
- frame selection,
- loop/reset semantics,
- zero/invalid duration rejection,
- speed multiplier semantics,
- event ordering and whether events fire on wrap/seek/reset,
- deterministic handling of multiple events at one boundary.

Prefer an exact integer/rational/fixed-point representation where it simplifies cross-build deterministic behavior. Do not rely on wall-clock playback for authoritative tests.

## 9.2 SA1 — SpriteAnimator2D runtime

Expose renderer-independent state such as:

```text
animation set
active clip
frame index
animation time/ticks
playing
loop state
speed
```

Headless stepping must advance the exact same authoritative state consumed by windowed rendering.

## 9.3 SA2 — Playback, loops, speed, events and transitions

Provide explicit APIs/state transitions for at least:

- play,
- restart,
- pause/resume,
- stop/reset contract,
- loop/non-loop completion,
- deterministic speed adjustment,
- queued/emitted semantic events,
- clip changes.

Do not introduce a generic graph/state-machine framework unless a later requirement justifies it. Keep the first animation surface composable and inspectable.

## 9.4 SA3 — Agent/MCP animation verification

A coding agent must be able to inspect and assert animation state without looking at pixels.

Intended semantic observations include:

```text
clip
frame
frame count
playing
loop
animation time/ticks
speed
recent/emitted event identity
region currently selected by animation
```

Intended operations include semantic equivalents of:

```text
play
pause/resume
stop/reset
step
inspect
assert clip
assert frame
assert event
```

MCP remains an adapter over the protocol-independent Agent surface. Do not put JSON/MCP types into the animation core.

Snapshot/report allocation is explicit-request work, never mandatory per-frame work.

## 9.5 SA4 — Animation conformance and workloads

Test exact frame/event sequences across:

- fixed-step runs,
- reset/replay,
- loops,
- non-loop completion,
- speed changes,
- clip transitions,
- identical seeds/state where relevant.

Measure realistic populations of animated sprites and separate:

- authoritative animation update cost,
- render extraction cost,
- GPU submission/draw cost.

# 10. Offline Sprite Processing and QA

The processing pipeline is allowed to be rich because it runs on explicit authoring/import/validation commands, not ordinary gameplay frames.

Reference projects inform design; they are not automatically embedded dependencies:

- `Hugo-Dz/spritefusion-pixel-snapper` — pixel lattice repair and palette-constrained cleanup ideas,
- `handsupmin/mono-pix` and `handsupmin/fast-pixelizer` — grid detection, regularization, idempotence and quality-fixture ideas,
- `aldegad/sprite-gen` — request/manifest/atlas/curation/runtime metadata and agent-oriented workflow ideas,
- `Canine89/perfectpixel-studio` — frame segmentation, alpha cleanup, centroid alignment and machine-readable quality scoring ideas.

Any copied/adapted code requires a separate dependency/license review. Prefer implementing Trace2D's own narrow contracts or interoperating through files/commands rather than pulling large authoring applications into the engine.

## 10.1 SPP0 — Deterministic processing/QA report contract

Define a stable machine-readable report so agents do not need to infer asset defects visually.

Potential metrics include, when applicable:

```text
source dimensions
expected/actual frame count
alpha/background residue
edge bleed
trim bounds
pivot / centroid position
pivot jitter across a clip
pixel-grid detected resolution
pixel-grid confidence
palette size / palette violations
identity similarity signals
motion-presence signal
per-frame sparse/empty warnings
atlas/page utilization
```

Metrics must clearly distinguish objective measurement from heuristic score/recommendation. A score must retain the raw measurements that produced it.

## 10.2 SPP1 — Alpha/background/frame extraction and segmentation

Support deterministic cleanup/import tasks needed for generated or poorly prepared art, with explicit modes instead of one opaque magic pipeline.

Candidates include:

- corner/key-informed matte cleanup for controlled backgrounds,
- safe alpha normalization,
- connected-component extraction,
- projection-profile based frame separation,
- globally optimal/dynamic-programming cuts for fused filmstrip poses when justified,
- expected-frame-count validation,
- explicit failure rather than silently manufacturing plausible frames.

Preserve source files; publish derived artifacts atomically where practical.

## 10.3 SPP2 — Pixel-grid, palette, pivot, identity and motion QA/repair

Separate independent concerns:

### Pixel-grid repair

Detect whether a supposed pixel-art source represents a recoverable square/rectangular lattice. Report confidence and failure rather than claiming impossible reconstruction. Optional repair may regularize cell boundaries and palette use.

Repeated repair of already-clean fixtures should be tested for deterministic/idempotent behavior when the algorithm claims it.

### Palette

Analyze a shared palette across animation frames when flicker avoidance requires it. Quantization is explicit offline processing, not a runtime shader feature.

### Pivot/alignment

Measure alpha-weighted or otherwise documented subject centers/feet/baselines as QA evidence. Automatic pivot suggestions may exist, but source-space pivot values remain explicit reviewable data.

### Identity and motion

Heuristic image metrics may flag likely identity drift or insufficient motion. They must be labeled as QA heuristics, not gameplay correctness truth. Retain raw metric values and fixtures.

## 10.4 SPP3 — Aseprite and generic importers

Prioritize common interoperable inputs:

- loose frame files,
- regular grid sheets,
- explicit region manifests,
- Aseprite-exported JSON/sheet metadata where the supported subset is documented.

Convert every input into the canonical Trace2D asset representation. Runtime code never branches on "this came from Aseprite".

## 10.5 SPP4 — sprite-gen / PerfectPixel-style interoperability

Support useful external manifests through importer/conversion boundaries rather than embedding their complete applications.

The importer should consume explicit frame rectangles, frame ordering, FPS/durations, loop flags, pivots/anchors/trim metadata when available, validate them, and publish canonical Trace2D assets plus diagnostics.

Do not depend on the generator's internal Python/Go/UI implementation at runtime.

## 10.6 SPP5 — Provider-neutral sprite generation orchestration

Trace2D may orchestrate generation, but provider APIs are not the engine architecture.

Conceptual flow:

```text
sprite-request.toml
      |
trace2d sprite generate
      |
GenerationProvider / external command adapter
      |
raw outputs
      |
deterministic Trace2D processing + QA
```

Provider implementations may include external commands, user scripts, local models, or hosted image-generation services.

Hard rules:

- provider credentials never become committed project data,
- no core/runtime dependency on one model/vendor,
- live generation is not a required deterministic CI gate,
- generation output is never accepted as canonical merely because a provider succeeded,
- deterministic processing/QA runs after generation,
- recorded/synthetic fixtures are the CI source for processing regression tests,
- live provider smoke tests are optional integration tests and must be isolated from ordinary CI.

# 11. SE2E — Generation/import to motion QA proof

The program's flagship proof is an end-to-end sample demonstrating:

```text
request or source asset
 -> raw/generated frames
 -> deterministic cleanup/normalization
 -> QA report
 -> canonical Trace2D SpriteAsset
 -> deterministic animation
 -> headless exact-frame inspection/assertions/events
 -> renderer submission
 -> capture
 -> motion/visual QA artifacts
```

The sample must intentionally include enough complexity to prove real contracts such as trim/pivot/atlas/animation events rather than only a single untrimmed square image.

A live AI provider may be demonstrated separately, but the committed regression test must run from stable fixtures without requiring network/API credentials.

# 12. SPERF — Final performance and guidance

Publish reproducible workloads covering dimensions such as:

- visible sprite count,
- animated sprite count,
- atlas pages,
- batch-key transitions,
- draw calls,
- culled count,
- CPU animation-update time,
- render extraction/packing time,
- instance/upload bytes per frame,
- retained GPU/upload capacities,
- texture memory / atlas utilization,
- capture cost separated from ordinary rendering.

Do not convert structural metrics into invented portable CPU/GPU percentages. Machine-specific timing is evidence labeled with environment/build information.

Guidance should help both humans and agents choose practical budgets using raw measurements and explain which optimizations are already justified.

# 13. Agent observability target

Once implemented, semantic inspection should make states conceptually similar to this possible without requiring pixels:

```text
#player SpriteRenderer2D
  asset      = "hero"
  region     = "attack_03"
  pivot      = [16, 28]
  flip_x     = false
  flip_y     = false
  tint       = [1, 1, 1, 1]
  blend      = "normal"
  sampling   = "nearest"
  layer      = 10
  order      = 125
  visible    = true

#player SpriteAnimator2D
  clip       = "attack"
  frame      = 3
  playing    = true
  loop       = false
  time/ticks = ...
```

Explicit renderer-debug inspection may additionally derive:

```text
atlas page
pixel rect
normalized UV
world bounds
visibility result
batch/run identity
draw index
```

but this derived data must be built only on explicit debug/inspection requests.

# 14. Explicit non-goals of the Sprite umbrella

The phrase "production-complete Sprite Renderer" does not mean an unbounded general renderer.

Excluded unless separately justified later:

- generic user material/shader graph,
- PBR,
- 2D deferred renderer,
- general lighting framework,
- render graph,
- bindless/GPU-driven scene architecture,
- arbitrary deformable polygon mesh animation,
- skeletal mesh runtime,
- custom allocator framework,
- work-stealing job system.

Arbitrary textured indexed geometry belongs to Issue #60 Mesh2D. Spine-specific runtime behavior belongs to Issue #61 and is blocked by its license gate.

# 15. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage from this document,
2. include relevant automated tests/fixtures,
3. update this document when the implementation finalizes or intentionally changes its contract,
4. update `PROJECT_STATUS.md` with the completed/current/next child,
5. preserve enough structured evidence that the next agent does not need previous chat history,
6. avoid beginning the next child until the current PR is merged green.

When asked simply to continue Trace2D, `AGENTS.md` defines the execution algorithm. This document supplies the Sprite-specific stage contract.