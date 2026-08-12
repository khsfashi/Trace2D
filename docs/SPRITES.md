# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1/SR2/SR3/SR4/SR5/SR6/SR7/SR8/SA0 complete. SA1 — `SpriteAnimator2D` authoritative runtime state is active via #146 / draft PR #147.**

Operational umbrella: GitHub Issue #59.  
Frozen S0 architecture: [`SPRITE_ARCHITECTURE.md`](SPRITE_ARCHITECTURE.md).  
Machine-readable S0 invariants: [`contracts/sprite-s0.json`](contracts/sprite-s0.json).  
Canonical S1 format: [`SPRITE_ASSET_FORMAT.md`](SPRITE_ASSET_FORMAT.md).  
SR0 render seam: [`SPRITE_RENDER_CONTRACT.md`](SPRITE_RENDER_CONTRACT.md).  
SR1 transform seam: [`SPRITE_TRANSFORM_PRESENTATION.md`](SPRITE_TRANSFORM_PRESENTATION.md).  
SR2 atlas/trim/UV seam: [`SPRITE_ATLAS_GEOMETRY.md`](SPRITE_ATLAS_GEOMETRY.md).  
SR3 color/sampling seam: [`SPRITE_COLOR_SAMPLING.md`](SPRITE_COLOR_SAMPLING.md).  
SR4 order/masking seam: [`SPRITE_ORDER_MASKING.md`](SPRITE_ORDER_MASKING.md).  
SR5 9-slice/tiled seam: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).  
SR6 pixel-perfect seam: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).  
SR7 production batching/hot-path seam: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).  
SR8 conformance/workload seam: [`SPRITE_RENDERER_CONFORMANCE_SR8.md`](SPRITE_RENDERER_CONFORMANCE_SR8.md).  
SA0 animation timing seam: [`SPRITE_ANIMATION_TIMING_SA0.md`](SPRITE_ANIMATION_TIMING_SA0.md).  
Machine-readable SA0 invariants: [`contracts/sprite-animation-sa0.json`](contracts/sprite-animation-sa0.json).  
SA1 authoritative state seam: [`SPRITE_ANIMATOR_STATE_SA1.md`](SPRITE_ANIMATOR_STATE_SA1.md).

This document owns the fixed Sprite stage order and capability target. Stage-local documents refine implementation details but cannot silently replace canonical authored/runtime truth with renderer or tooling state.

## 1. Product goal

Trace2D targets an Agent-verifiable Sprite pipeline rather than a minimal quad renderer:

```text
sprite request / source image / external sheet
 -> optional generation
 -> deterministic import / normalization / QA
 -> canonical SpriteAsset
 -> authoritative Sprite runtime/animation state
 -> backend-independent render extraction
 -> production renderer
 -> exact-frame + perceptual QA
 -> reproducible performance evidence
```

Generated pixels and external formats are inputs. Canonical Trace2D data is runtime truth.

## 2. Frozen authority and coordinates

```text
external source/generation
 -> deterministic import
 -> canonical SpriteAsset CPU truth
 -> authoritative SpriteRenderer2D / SpriteAnimator2D / transform semantics
 -> backend-independent extraction
 -> derived presentation
 -> renderer/backend resources
```

Hard invariants:

- canonical assets and authoritative runtime state require no renderer/GPU initialization,
- authored image metadata is exact source-pixel truth; normalized UVs/GPU handles/batch IDs are derived,
- source origin is untrimmed top-left, +x right, +y down, integer half-open rectangles,
- pivot is exact untrimmed source-space metadata and may intentionally lie outside source bounds,
- trim and packed rotation are storage semantics only,
- rendering never owns transform/gameplay/animation truth,
- `current_fixed` is authoritative; `previous_fixed` is presentation history,
- exact-frame presentation uses authoritative current state,
- semantic painter order cannot be changed by resource/material sorting,
- #71/#86/#88/#89 attach through typed seams without replacing Sprite semantics,
- import/generation/repair/report/capture are explicit work outside ordinary frame hot paths.

SA0 extends the same authority rule to animation: integer fixed-step animation time/frame/event crossings are authoritative runtime state; renderer selection, GPU resources, pixels and observation artifacts remain derived consumers/evidence. SA1 materializes that rule as renderer-independent prepared clip data plus fixed-size typed `SpriteAnimator2DState`.

## 3. Fixed implementation order

Exactly one child issue/PR is active at a time:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [active #146/#147] -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed renderer stages through SR8 and SA0 timing semantics are frozen. **Do not create or begin SA2 while #146/PR #147 remains open or while its hosted CI/audit gates are pending.**

## 4. Completed renderer foundation

### S0 — architecture / authority — complete

Frozen by #119/#120 and `SPRITE_ARCHITECTURE.md`: authority/ownership, exact coordinates, pivot/trim/rotation storage semantics, fixed-step presentation history, exact-frame capture semantics, typed resource/view/material seams, painter-order invariant, #97-#99 verification authority and hot-path/offline-tooling separation.

### S1 — canonical Sprite asset/import — complete

Merged via #121/#122. Canonical `.sprite.toml` v1 owns ordered pages/regions, exact source/trim/packed rectangles, reduced rational pivot, `none|cw90` packed rotation, color/alpha/sampling intent and source-pixel border metadata consumed by SR5. No normalized UV or GPU handle is canonical asset state.

### SR0 — asset/render separation — complete

Merged via #123/#124. Setup-time region resolution produces fixed-size renderer-facing contract data; steady-state extraction is O(1) and allocation-free.

### SR1 — transform/history/geometry — complete

Merged via #125/#126. Reuses `scene::Transform2D`, preserves exact rational pivot and untrimmed source geometry, applies semantic flip/scale/rotation/translation once, and separates authoritative fixed state from presentation history.

### SR2 — atlas/trim/pivot/rotated packing — complete

Merged via #127/#128. Trim remains embedded in untrimmed logical source space; packed storage never controls logical placement; UV geometry is exact pixel-edge truth and `cw90` changes storage mapping only.

### SR3 — color/alpha/blend/sampling — complete

Completed via #130/#131. Canonical source alpha remains straight. Sampling is nearest/linear with atlas-safe texel-center guards, sRGB/linear page intent is explicit, and built-in fragment output is premultiplied immediately before blending. Persistent samplers/pipelines and capacity-managed Sprite vertex resources are reused. Required owner Windows presentation-GPU evidence passed before merge.

### SR4 — painter order/sorting groups/masking — complete

Completed via #132/#133, squash `3c843bba10be536685c0fc70306b3fd6c75ed67a`. SR4 freezes signed layer/order, explicit stable semantic order, bounded one-level sorting groups, no resource identity in semantic ordering, bounded stencil mask identities/phases, persistent stencil-compatible pipelines/mask target, and no ordinary-frame explicit GPU readback/fence wait.

### SR5 — 9-slice and tiled/repeated primitives — complete

Completed via #134/#135, squash `6d84ada945774d54c089d1a5bec3b63e17a43334`. Concrete contract: [`SPRITE_PRIMITIVES.md`](SPRITE_PRIMITIVES.md).

SR5 adds exact source-pixel borders, `quad|sliced|tiled` runtime intent, deterministic 9-slice partition/compression, bounded tiled expansion, partial-final-tile geometry, trim-gap and `cw90` subrect mapping, per-patch atlas-safe sample bounds, bounded caller-owned patch output, top-level SR4 semantic atomicity and persistent GPU reuse. The owner GPU fixture exposed and then locked a real sub-texel partial-tile linear-filter bleed regression before merge.

### SR6 — pixel-perfect runtime presentation — complete

Completed via #136/#137, squash `3fd6e5a439a1e327bd89797f5d5a5a9dae69dace`. Concrete contract: [`SPRITE_PIXEL_PERFECT.md`](SPRITE_PIXEL_PERFECT.md).

SR6 freezes integer logical-viewport mapping, centered letterbox/pillarbox, exact source-pixel grid eligibility/snap, authoritative-current vs interpolated presentation selection, nearest exact pixel presentation, GPU viewport/scissor use, SR4/SR5 preservation, and no intermediate upscale texture or ordinary-frame explicit readback/fence wait.

### SR7 — production batching/resource reuse/hot-path metrics — complete

Completed via #138/#139, squash `9adc9f0e1aab714392b08d068c3ee9ebbad46dbb`. Concrete contract: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).

SR7 preserves painter order, culls against the resolved presentation view, compacts only visible geometry and merges only compatible contiguous work. Compatibility includes resolved resources/material/pipeline/sampler/blend/mask identity. Retained capacities/resources are reused and metrics distinguish semantic submissions, culling, quads/uploads and actual batch draws.

### SR8 — renderer conformance / capture QA / reproducible workloads — complete

Completed via #142/#143, squash `2108122dad5ac2dcbb964f7ada0e80f7afa21003`. Concrete contract: [`SPRITE_RENDERER_CONFORMANCE_SR8.md`](SPRITE_RENDERER_CONFORMANCE_SR8.md).

SR8 adds validation/evidence without changing normal renderer semantics:

- backend-independent composition coverage spanning SR1/SR2 geometry, `cw90`, SR6 presentation-time selection and SR7 batching,
- fixed 1,024-submission structural workload with exact expectations: 768 visible, 256 culled, 960 visible quads, 7 contiguous compatibility runs,
- real-GPU SR2 trim/pivot/`cw90` presentation fixture,
- reuse of SR3-SR7 GPU fixtures through the trusted #140/#141 owner GPU gate,
- exact deterministic CPU facts vs fixture-owned bounded GPU pixel tolerances,
- exact-head `trace2d.sprite-renderer-final-gate.v1` evidence binding CPU conformance, workload facts, GPU evidence, hashes and Git commit.

Final implementation head `74b5b82c9df961baeaeb84e80169e7e621535cfb` passed hosted CI run `31569822039` and trusted owner GPU Gate run `31569818936`; the uploaded exact-head artifact digest is `sha256:dcd0014191cc7bb0fdead133a38acb466e2d156bf866d171b2baa5804177163d`.

Captured pixels remain derived evidence, there is no automatic golden-image update path, no shared-runner wall-clock correctness threshold, no SR8-only per-frame reporting/heap work, and no new ordinary-frame explicit GPU readback/fence wait.

## 5. Deterministic Sprite animation — active

### SA0 — timing/frame/event contract — complete

Completed via #144/#145, squash `d9955d4c987a627f0009a018b9b5293c6f3d8e73`. Concrete contract: [`SPRITE_ANIMATION_TIMING_SA0.md`](SPRITE_ANIMATION_TIMING_SA0.md). Machine-readable invariants: [`contracts/sprite-animation-sa0.json`](contracts/sprite-animation-sa0.json).

SA0 freezes the timeline semantics that SA1-SA4 must reuse:

- integer nanoseconds are authoritative animation time, matching the existing `FixedStepRuntime` time domain,
- frame durations are positive integers and clip duration is a checked exact sum,
- frame `i` owns `[boundary_i, boundary_i+1)`,
- non-loop terminal `t == duration` is explicit completion while the final authored frame remains presented,
- authored events live in `[0, duration)` with stable authored ordinal for equal offsets,
- forward crossing uses `a < event_time <= b`, reverse crossing uses `b <= event_time < a`,
- large advances preserve every crossed event in deterministic traversal order,
- loop/ping-pong traversal composes ordered segments rather than modulo/sampling shortcuts,
- forward loop entry emits offset-zero events once after the loop marker and the following `(0,b]` segment excludes zero,
- seek/reset/inspection do not replay historical events,
- wall-clock render time, presentation alpha, GPU state and pixels never advance/replace animation truth,
- steady-state update has no mandatory per-tick heap/filesystem/JSON/string/name-lookup work and cannot silently drop events on output-capacity exhaustion.

SA0 is a contract stage. It does not implement the full `SpriteAnimator2D`, playback commands, transitions, MCP actions or animation workloads.

Primary-source decisions are recorded in the SA0 contract: Aseprite per-frame integer durations are adopted/adapted to Trace2D integer nanoseconds; Godot variable-duration/reverse/loop/ping-pong semantics are adapted where useful while float progress is rejected as authoritative deterministic time; fixed-timestep vs rendered-presentation separation is retained.

SA0 merged only after `Sprite SA0 Contract` and normal repository CI/audits were green on the final head. No new GPU behavior was introduced, so SA0 added no real-GPU gate.

### SA1 — `SpriteAnimator2D` authoritative state — active #146 / draft PR #147

Concrete contract: [`SPRITE_ANIMATOR_STATE_SA1.md`](SPRITE_ANIMATOR_STATE_SA1.md).

SA1 implements the renderer-independent typed state that SA0 deferred:

- `SpriteAnimationClip2D::Prepare` validates ordered frame records and setup-resolved canonical Sprite region indices,
- frame durations and cached cumulative boundaries use exact integer nanoseconds,
- failed preparation preserves the previous prepared output,
- arbitrary time-to-frame lookup uses cached boundaries in O(log frame_count),
- `SpriteAnimator2DState` holds clip/time/frame/playback/loop/direction/completion/exact speed state only,
- exact speed is a canonical reduced non-negative rational magnitude; direction is explicit and separate,
- `SpriteAnimator2DState` is trivially copyable and owns no heap container/string/renderer resource,
- `ValidateSpriteAnimator2DState` rejects time/frame mismatch, invalid enums/speed and invalid completion state,
- `RestoreState` validates before commit and preserves prior state on failure,
- current frame/region observation is O(1) over the prepared clip,
- ordinary state access has no filesystem/JSON/formatting/semantic-name lookup/renderer/GPU initialization or mandatory heap allocation.

The prepared clip pointer is non-owning and must outlive animator states referencing it. This deliberately avoids per-state shared-ownership/reference-count work in the fixed-step hot state; #86 may later generalize resource lifetime without changing animation authority.

SA1 does **not** implement fixed-step time advancement, retained fractional speed remainder, play/pause/stop/restart/seek commands, loop/ping-pong execution, authored event emission/output buffers, transitions, Agent/MCP actions or workloads.

Current primary-source review: Godot's explicit animation/frame/playing/speed state is adapted while float progress/speed is rejected as authority; Aseprite's explicit per-frame duration/direction metadata is adopted/adapted without creating a runtime dependency.

PR #147 may merge only when focused runtime tests and normal hosted CI/audits are green on the same final head and documentation agrees with the implementation. SA1 introduces no new presentation-GPU behavior, so it adds no new local real-GPU gate.

### SA2 — playback/events/transitions

Implement deterministic play/restart/pause/resume/stop/reset, loops, completion, exact speed advancement with retained remainder, authored event traversal/output and bounded transitions while preserving SA0 event-crossing rules and SA1 state invariants. **Do not begin before SA1 merges green.**

### SA3 — Agent/MCP verification

Expose protocol-independent inspect/action/assert semantics; MCP remains an adapter.

### SA4 — conformance/workloads

Prove fixed-step frame/event sequences and measure animation update independently from rendering.

## 6. Offline Sprite processing / generation

### SPP0 — processing/QA report
Machine-readable raw measurements for dimensions, frame count, alpha/edge residue, trim, pivot/jitter, grid/palette, identity/motion warnings and atlas utilization.

### SPP1 — alpha/background/frame extraction
Deterministic explicit cleanup/segmentation modes; expected-frame mismatch fails instead of inventing frames.

### SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair
Offline deterministic or explicitly labelled heuristic analysis/repair with reviewable raw evidence.

### SPP3 — Aseprite/generic importers
Convert supported external formats into canonical Trace2D Sprite assets; no source-tool runtime dispatch.

### SPP4 — sprite-gen / PerfectPixel-style interoperability
Consume useful external manifests through conversion/validation without runtime dependencies.

### SPP5 — provider-neutral generation orchestration

```text
sprite request
 -> replaceable external generation provider
 -> raw output
 -> deterministic Trace2D processing + QA
 -> canonical asset after validation
```

Live provider calls are not deterministic CI dependencies.

## 7. End-to-end proof

### SE2E
Prove request/import -> raw/generated pixels -> deterministic QA -> canonical asset -> animation -> headless exact-frame verification -> renderer/capture -> perceptual/human review.

### SPERF
Publish visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation/extraction CPU time, upload bytes, retained capacities, texture/page memory/utilization and capture cost.

## 8. Verification/review authority

Reuse #97-#99:

```text
deterministic Sprite fact
 -> verify
 -> diagnose
 -> Agent/user repair
 -> re-verify
 -> multimodal review only when genuinely perceptual
 -> human creative approval
```

A screenshot cannot override deterministic failure. Sprite does not create a second review database.

## 9. Explicit handoffs / non-goals

#59 does not silently absorb generic Material2D/Shader2D (#89), Camera2D/Viewport2D ownership (#88), general resource lifecycle (#86), scene/component hierarchy (#71), arbitrary Mesh2D (#60), Spine/skeletal runtime before #61/#101 license/product decision, PBR/deferred/render-graph/bindless architecture, or an ECS/reflection/custom-allocator/job-system detour without evidence.

## 10. Handoff rule

Every Sprite child PR must:

1. implement only the first incomplete stage,
2. include tests/fixtures/evidence,
3. update relevant stage contracts, this roadmap and `PROJECT_STATUS.md`,
4. preserve enough structured evidence to continue without chat history,
5. avoid beginning the next child until the current PR merges green.

SA1 / #146 / draft PR #147 is the only active Sprite child. Keep its PR scoped to prepared clip data plus `SpriteAnimator2D` authoritative state/validation/observation. Do not create/implement SA2 until SA1 merges green. After the complete #59 program, the exact next core item remains **#103 Benchmark B1** before #69 game-production work.
