# Sprite Pipeline Contract

Status: **S0/S1/SR0/SR1/SR2/SR3/SR4/SR5/SR6/SR7/SR8/SA0/SA1/SA2/SA3 complete. SA4 — animation conformance, determinism, and workloads is active via #152 / draft PR #153.**

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
SR7 batching/hot-path seam: [`SPRITE_BATCHING_SR7.md`](SPRITE_BATCHING_SR7.md).  
SR8 conformance/workload seam: [`SPRITE_RENDERER_CONFORMANCE_SR8.md`](SPRITE_RENDERER_CONFORMANCE_SR8.md).  
SA0 timing seam: [`SPRITE_ANIMATION_TIMING_SA0.md`](SPRITE_ANIMATION_TIMING_SA0.md).  
Machine-readable SA0 invariants: [`contracts/sprite-animation-sa0.json`](contracts/sprite-animation-sa0.json).  
SA1 state seam: [`SPRITE_ANIMATOR_STATE_SA1.md`](SPRITE_ANIMATOR_STATE_SA1.md).  
SA2 playback seam: [`SPRITE_ANIMATOR_PLAYBACK_SA2.md`](SPRITE_ANIMATOR_PLAYBACK_SA2.md).  
SA3 Agent/MCP seam: [`SPRITE_ANIMATION_AGENT_SA3.md`](SPRITE_ANIMATION_AGENT_SA3.md).  
Active SA4 conformance/workload seam: [`SPRITE_ANIMATION_CONFORMANCE_SA4.md`](SPRITE_ANIMATION_CONFORMANCE_SA4.md).

This document owns the fixed Sprite stage order and capability target. Stage-local documents refine implementation details but cannot silently replace canonical authored/runtime truth with renderer, Agent, workload, timing, or capture state.

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
 -> explicit Agent inspection / deterministic workload evidence
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
- import/generation/repair/report/capture/workload/timing are explicit work outside ordinary frame hot paths.

Animation authority is the SA0-SA4 chain: integer fixed-step animation time/frame/event crossings are authoritative runtime state; SA1 materializes renderer-independent prepared clip/state; SA2 executes exact retained-rational playback and typed emissions; SA3 exposes that existing authority to agents without duplicating it; SA4 validates/replays/measures it explicitly. MCP serialization, workload digests, timing samples, GPU resources and pixels never become a second animation state machine.

## 3. Fixed implementation order

Exactly one child issue/PR is active at a time:

```text
S0 [complete] -> S1 [complete]
 -> SR0 [complete] -> SR1 [complete] -> SR2 [complete] -> SR3 [complete]
 -> SR4 [complete] -> SR5 [complete] -> SR6 [complete] -> SR7 [complete]
 -> SR8 [complete]
 -> SA0 [complete] -> SA1 [complete] -> SA2 [complete] -> SA3 [complete]
 -> SA4 [active #152/#153]
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

Completed stages through SA3 are frozen. **Do not create or begin SPP0 while #152/PR #153 remains open or while its hosted CI/audit/documentation gates are pending.**

## 4. Completed renderer foundation

### S0 — architecture / authority — complete

#119/#120 froze authority/ownership, exact coordinates, pivot/trim/rotation semantics, fixed-step presentation history, exact-frame capture, typed future resource/view/material seams, painter-order invariants and hot-path/offline-tool separation.

### S1 — canonical Sprite asset/import — complete

#121/#122 established canonical `.sprite.toml` v1 with ordered pages/regions, exact source/trim/packed rectangles, reduced rational pivot, `none|cw90` packing, color/alpha/sampling intent and source-pixel border metadata. Normalized UVs and GPU handles are never canonical asset state.

### SR0 — asset/render separation — complete

#123/#124 made setup-time region resolution produce fixed-size renderer-facing data; steady-state extraction is O(1) and allocation-free.

### SR1 — transform/history/geometry — complete

#125/#126 reused `scene::Transform2D`, preserved exact rational pivot/untrimmed source geometry, and separated authoritative fixed state from presentation history.

### SR2 — atlas/trim/pivot/rotated packing — complete

#127/#128 kept trim in untrimmed logical source space and made `cw90` a storage/UV mapping concern only.

### SR3 — color/alpha/blend/sampling — complete

#130/#131 froze straight canonical source alpha, nearest/linear atlas-safe sampling, explicit sRGB/linear page intent, premultiplied output immediately before blending, and persistent sampler/pipeline/vertex-resource reuse. Required owner GPU evidence passed.

### SR4 — painter order/sorting groups/masking — complete

#132/#133 froze stable semantic painter order independent of resource identity, bounded one-level sorting groups, bounded stencil masks and persistent stencil-compatible resources.

### SR5 — 9-slice and tiled/repeated primitives — complete

#134/#135 added exact source-pixel borders, deterministic sliced/tiled expansion, bounded caller-owned patch output, trim-gap/`cw90` subrect mapping and atlas-safe partial-tile sampling while preserving SR4 semantic atomicity.

### SR6 — pixel-perfect runtime presentation — complete

#136/#137 froze integer logical viewport mapping, letterbox/pillarbox, exact source-grid snap, current-vs-interpolated presentation selection and nearest exact pixel presentation without an intermediate upscale texture.

### SR7 — production batching/resource reuse/hot-path metrics — complete

#138/#139 preserved painter order, culled against the resolved presentation view, compacted visible geometry only, merged only contiguous compatible work and exposed structural metrics while reusing retained capacities/resources.

### SR8 — renderer conformance / capture QA / reproducible workloads — complete

#142/#143 added backend-independent composition coverage, the fixed 1,024-submission structural workload, owner real-GPU fixtures/evidence, deterministic CPU facts vs bounded GPU pixel tolerances and exact-head final evidence. Captured pixels remain derived evidence and shared-runner wall-clock time is not a correctness threshold.

## 5. Deterministic Sprite animation

### SA0 — timing/frame/event contract — complete

Completed via #144/#145. SA0 freezes:

- integer nanoseconds as authoritative animation time,
- positive integer frame durations and checked exact clip duration,
- frame ownership `[boundary_i,boundary_i+1)`,
- explicit non-loop completion at `t == duration`,
- stable authored ordinal for equal event offsets,
- forward crossing `a < event_time <= b`,
- reverse crossing `b <= event_time < a`,
- lossless deterministic large-advance traversal,
- ordered loop/ping-pong segments with one offset-zero emission on forward loop entry,
- no historical event replay from seek/reset/inspection,
- no wall-clock/presentation/GPU/pixel authority,
- no mandatory per-tick heap/filesystem/JSON/string/name-lookup work and no silent event loss.

Aseprite per-frame integer duration/direction precedent is adopted/adapted; Godot variable duration/loop/reverse/ping-pong behavior is adapted while float progress is rejected as Trace2D deterministic authority.

### SA1 — `SpriteAnimator2D` authoritative state — complete

Completed via #146/#147. SA1 adds prepared renderer-independent clip data and fixed-size typed state, cached checked integer boundaries, O(log frame_count) arbitrary lookup, O(1) current-frame/region observation, canonical non-negative rational speed magnitude, explicit direction/completion validation and transactional restore. The prepared clip pointer is non-owning and must outlive states referencing it.

### SA2 — deterministic playback/events/loops/transitions — complete

Completed via #148/#149. SA2 adds prepared numeric authored events, retained rational speed remainder, explicit play/pause/stop/reset/restart/seek/speed/direction operations, deterministic `Once|Loop|PingPong` traversal and caller-owned bounded `AuthoredEvent|Loop|Bounce|Completed` output. Capacity failure is explicit and transactional. Ordinary advancement adds no mandatory heap/filesystem/JSON/formatting/name/GPU work.

### SA3 — Agent/MCP verification — complete

Completed via #150/#151, squash `d7a509ca03f851436d495183503f798c8afb8c2a`. SA3 exposes the existing SA0-SA2 authority through explicit non-owning entity bindings, exact scalar inspection, direct runtime actions and finite typed assertions. Explicit Agent advance materializes ordered emissions only on request and remains bounded/transactional. MCP exposes `trace2d.sprite_animation.inspect|action|assert` as serialization only. Ordinary animation stepping receives no Agent snapshot/reporting cost.

### SA4 — conformance, determinism, and workloads — active #152 / draft PR #153

Concrete contract: [`SPRITE_ANIMATION_CONFORMANCE_SA4.md`](SPRITE_ANIMATION_CONFORMANCE_SA4.md).

SA4 is validation over the frozen runtime, not new playback semantics.

Committed structural workloads:

- `steady_loop_rational`: 6,000 fixed steps, exact `2/3` speed, forward looping, offset-zero/event boundaries,
- `dense_event_ping_pong`: dense and equal-time authored events, exact `5/4` speed, repeated bounces,
- `large_step_multi_wrap`: reverse loop with advances larger than three clip durations.

`trace2d_sprite_animation_workload` executes the selected workload twice from fresh state. A mismatch fails. Machine-readable evidence reports exact frame/event/step/emission/final-state facts plus a stable FNV-1a digest over explicit numeric/enumerated semantic fields only. Pointers, addresses, wall-clock values, renderer/GPU state and platform object bytes are excluded from replay identity.

Focused SA4 tests cover long-running replay, split-vs-aggregate equivalence where SA0 semantics permit it, long-run rational quotient/remainder, large multi-bounce ping-pong traversal, transactional capacity failure and restart/seek future-transcript repeatability.

Optional local Release timing:

- prepares state outside each measured window,
- discards explicit warmups,
- repeats measured advance windows,
- reports average/median/p95 plus OS/compiler/build/machine metadata,
- never becomes a hosted/shared-CI wall-clock correctness gate.

SA4 adds no reporting/hash/timing/JSON/filesystem/Agent/renderer/GPU work to ordinary `SpriteAnimator2D::Advance`.

Current reference decisions: FoundationDB deterministic replay is adopted/adapted as a correctness technique; Google Benchmark warmup/repetition/statistics practice is adapted without adding the dependency or CI time thresholds; Godot loop/reverse/ping-pong cases are adapted without float authority; Aseprite duration/direction/repeat metadata remains authoring precedent.

No new presentation/GPU behavior is introduced, so SA4 has no new real-GPU acceptance gate.

## 6. Offline Sprite processing / generation

### SPP0 — processing/QA report

Define deterministic machine-readable raw measurements for dimensions, frame count, alpha/edge residue, trim, pivot/jitter, grid/palette, identity/motion warnings and atlas utilization. Reporting is explicit offline/tool work and must not enter normal runtime hot paths.

### SPP1 — alpha/background/frame extraction

Provide deterministic explicit cleanup/segmentation modes; expected-frame mismatch fails instead of silently inventing frames.

### SPP2 — pixel-grid/palette/pivot/identity/motion QA and repair

Add offline deterministic or explicitly labelled heuristic analysis/repair with reviewable raw evidence.

### SPP3 — Aseprite/generic importers

Convert supported external formats into canonical Trace2D Sprite assets; never dispatch source-tool runtime code from the engine.

### SPP4 — sprite-gen / PerfectPixel-style interoperability

Consume useful external manifests through conversion/validation without turning those tools into runtime dependencies.

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

Publish visible/animated counts, atlas pages, compatibility transitions, draws, culling, animation/extraction CPU time, upload bytes, retained capacities, texture/page memory/utilization and capture cost with reproducible environment metadata.

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

SA4 / #152 / draft PR #153 is the only active Sprite child. Keep it scoped to deterministic animation conformance, explicit workloads and environment-labelled local timing evidence. Do not create or implement SPP0 until SA4 merges green. After the complete #59 program, the exact next core item remains **#103 Benchmark B1** before #69 game-production work.
