# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable, documented slice rather than disconnected engine subsystems.

`PROJECT_STATUS.md` is the operational source for the exact current issue/PR. Compiling code, live PR/CI state and explicit owner decisions win over stale prose.

## Design direction

Trace2D is not trying to clone a large editor-first engine feature-for-feature.

Its differentiator remains:

- deterministic fixed-step execution,
- text-first authored state,
- stable semantic identity,
- structured headless observability,
- semantic input/actions and exact-frame assertions,
- visual/audio output as presentation evidence rather than the only correctness oracle,
- explicit ownership/resource lifetimes,
- measurement-driven performance work,
- coding-agent workflows that do not depend on hidden editor state.

The roadmap therefore has three responsibilities:

1. deepen the Agent-verifiable runtime/rendering foundation,
2. make that foundation usable by third-party game developers through a coherent project/game/world lifecycle,
3. close practical engine gaps without introducing broad speculative infrastructure that Trace2D does not need.

The frozen integration contract for future production subsystems is [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md).

## P0 — Project foundation

Status: **complete**.

Delivered:

- C++20 / CMake,
- pinned vcpkg baseline,
- MSVC warning policy,
- CLI bootstrap,
- GoogleTest / CTest,
- Windows CI,
- architecture and Agent-first contracts.

## P1 — Deterministic runtime foundation

Status: **complete**.

Delivered:

- SDL3 platform boundary,
- windowed/headless startup,
- fixed simulation timestep,
- explicit frame stepping,
- frame/simulation-time observation,
- deterministic seed/reset ownership,
- wall-clock separation from explicit test stepping.

## P2 — Scene/entity authored-state baseline

Status: **complete for the alpha baseline**.

Delivered:

- generation-safe runtime entity identity,
- stable authored semantic IDs,
- name/tags,
- `Transform2D`,
- versioned TOML scenes,
- canonical deterministic serialization,
- structured schema diagnostics.

The production hierarchy/component world is future #71 and must follow the user-defined component contract in `PRODUCTION_ARCHITECTURE_CONTRACTS.md`.

## P3 — Structured observability

Status: **complete baseline**.

Delivered:

- protocol-independent Agent facade,
- runtime/scene/entity/component snapshots,
- semantic selectors and deterministic queries,
- stable structured diagnostics,
- CLI JSON only at tool/adaptor boundaries.

## P4 — Virtual input and gameplay testing

Status: **complete baseline**.

Delivered:

- engine-owned physical/virtual input state,
- deterministic frame-indexed scheduling,
- fixed-frame scenarios,
- semantic component assertions,
- reproducible failure reports.

Gameplay-facing semantic Input Actions and broader devices are future #72.

## P5 — 2D renderer and exact-frame capture

Status: **complete for Public Alpha baseline**.

Delivered:

- SDL3 GPU boundary,
- orthographic camera math,
- baseline textured sprites,
- inclusive AABB culling,
- order-preserving contiguous same-texture instancing,
- persistent/capacity-reused resources,
- offscreen presentation/capture target,
- explicit simulation-frame capture,
- deterministic CPU-normalized BMP artifact,
- reproducible renderer workloads and metric boundaries.

The production SpriteRenderer is #59. Practical Camera2D/Viewport2D is #88. Programmable Material2D/Shader2D is #89.

## P6 — Practical deterministic/Agent-verifiable breadth

Status: **active**.

Completed:

```text
#40 texture asset cache/import
 -> #42 text/basic UI
 -> #43 semantic UI automation
 -> #39 MCP transport
 -> #41 renderer workloads
 -> #47 particle determinism
 -> #48 rich CPU particle reference
 -> #49 text-authored effects + ParticleEmitter2D
 -> #50 complete Agent particle verification
 -> #51 particle cost analysis + explicit human backend choice + deterministic compiler
```

Exact remaining particle order:

```text
#52 explicit GPU runtime
 -> #53 CPU/GPU conformance, workloads, safe budgets and guidance
```

Detailed particle contract: [`PARTICLES.md`](PARTICLES.md).

### Particle architecture

```text
text-authored effect
 -> deterministic CPU semantic/reference simulation
 -> complete structured Agent verification
 -> deterministic structural cost report
 -> optional local machine timing
 -> HUMAN backend decision cpu|gpu
 -> deterministic minimized GPU program
 -> GPU runtime for explicitly gpu-selected effects
 -> CPU/GPU conformance + visual/performance QA
```

Hard rules:

- CPU is the semantic oracle,
- backend choice is explicit and reviewable,
- an LLM may recommend but never silently switch,
- unsupported GPU selection fails clearly rather than silently falling back,
- normal GPU mode does not duplicate full CPU reference simulation,
- no fake portable CPU percentages,
- no universal cross-vendor bit-identical GPU float claim without proof.

## P6.5 — Production architecture contract freeze (#85)

Status: **contract PR #94 active; becomes complete when that PR merges**.

This is deliberately a contract phase, not an implementation detour. It freezes the seams that #59 and later game-production work must honor so the engine does not repeatedly redesign world/resource/render integration.

Detailed contract: [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md).

Frozen areas:

- external user-defined typed gameplay components under the same Scene model as engine components,
- authoritative fixed-step state versus interpolated presentation state,
- unified typed resource identity/lifetime/dependency/memory accounting,
- reusable scene templates, deterministic instancing/despawn and world load/unload,
- Camera2D/Viewport2D semantics,
- Material2D/Shader2D programmable 2D rendering surface,
- deterministic setup-resolved property tweening,
- unified Agent-readable profiling/diagnostics,
- tiered real-GPU conformance,
- explicit later gates for lighting/navigation/broader platforms/networking/hot reload.

#85 does **not** start these future implementations early. After particles, #59 still starts first; the contract only makes its integration seams non-ambiguous.

## P7 — Complete Sprite program (#59)

Status: **owner-fixed future program after #53 and completed #85 contract freeze**.

Detailed Sprite contract: [`SPRITES.md`](SPRITES.md).
Integration contract: [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md).

Fixed internal order remains:

```text
S0 -> S1
 -> SR0 -> SR1 -> SR2 -> SR3 -> SR4 -> SR5 -> SR6 -> SR7 -> SR8
 -> SA0 -> SA1 -> SA2 -> SA3 -> SA4
 -> SPP0 -> SPP1 -> SPP2 -> SPP3 -> SPP4 -> SPP5
 -> SE2E -> SPERF
```

The program covers:

- canonical Trace2D Sprite assets,
- standalone and atlas regions,
- trim/source-size/pivot correctness,
- position/rotation/non-uniform scale/flip,
- tint/opacity, sampling, documented alpha/blend semantics,
- stable painter order, sorting groups, masking,
- 9-slice and tiled/repeated sprite presentation,
- pixel-perfect runtime presentation,
- measured batching/resource reuse,
- deterministic `SpriteAnimator2D`, events and state,
- Agent/MCP exact-frame animation QA,
- deterministic import/processing/repair QA,
- Aseprite/generic and external sprite-generation manifest interoperability,
- provider-neutral generation orchestration,
- end-to-end import/generate -> normalize -> QA -> animate -> assert -> render/capture -> motion QA,
- final reproducible Sprite/animation performance evidence.

Additional frozen requirements from #85:

- SpriteRenderer/Animator semantics fit the future #71 typed component model,
- previous/current fixed-state history supports smooth interactive render interpolation without changing Agent/gameplay truth,
- exact-frame capture renders authoritative current state unless explicit sub-frame alpha is supplied,
- canonical Sprite CPU identity stays separate from GPU handles for future #86,
- renderer view input remains compatible with future #88 Camera2D/Viewport2D,
- batching reserves a resolved material/pipeline identity for future #89 without global painter-order sorting.

Live image generation may be nondeterministic; deterministic post-processing/runtime CI uses recorded/synthetic fixtures.

## P8 — Expanded open-source game-production foundation

Status: **owner-fixed future program after #59**.

The original #69-#79 direction is preserved and expanded with #86-#92 to close practical engine gaps before the flagship external-game proof.

Primary contracts:

- [`GAME_PRODUCTION.md`](GAME_PRODUCTION.md)
- [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md)

Fixed implementation sequence:

```text
#69 Game/Application boundary
 -> #70 Project manifest + external consumer build/install/package
 -> #71 Scene hierarchy + engine/game typed components
 -> #86 unified typed resource lifecycle
 -> #87 reusable scene templates + deterministic world lifecycle
 -> #88 Camera2D + Viewport2D
 -> #72 Input Actions + gamepad/mouse/text/IME
 -> #73 TileSet/TileMap
 -> #74 production UTF-8 font/text/localization
 -> #75 practical deterministic UI hierarchy/layout/widgets
 -> #89 Material2D + Shader2D
 -> #90 deterministic resolved-property tween animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 unified Agent-readable profiler/diagnostics
 -> #78 Linux/compiler/toolchain hardening
 -> #92 tiered real-GPU conformance/release validation
 -> #79 save/persistence + authored schema migration
```

### #69 — Game/Application boundary

A game lives outside engine internals. One explicit lifecycle owns startup, fixed-step game update, shutdown and access to required engine services. Headless/windowed execution composes the same game logic. SDL/MCP/backend types do not leak into gameplay APIs.

### #70 — Project manifest/build/package

Versioned project identity, startup content, external CMake consumer flow, install/export/package policy and reproducible build/run/test commands. Distribution-facing shader/package policy belongs here.

### #71 — Scene hierarchy + engine/game typed components

Deterministic hierarchy/local-world transforms plus finite typed component composition.

This stage must prove **external user-defined game components**, not only engine components:

- stable explicit component type ID,
- authored/runtime-only classification,
- explicit parse/validate/serialize/inspect adapters,
- strongly typed C++ access,
- Agent inspection/assertions,
- no generic reflection/property bag requirement.

### #86 — Unified typed resource lifecycle

Project-relative typed identity, generation-safe resolved handles, CPU/GPU ownership separation, dependency graph, explicit unload/release, cache reuse and memory accounting. No tracing GC or hot-path filesystem/path work.

### #87 — Scene templates and world lifecycle

Reusable text-authored entity hierarchies, stable template-local/instance identity, deterministic structural-change safe points, explicit instantiate/despawn and additive world/scene load-unload semantics. No hidden mandatory engine pooling.

### #88 — Camera2D / Viewport2D

Typed camera world state, logical viewport vs OS-window separation, deterministic active-camera selection, backend-independent world/screen conversion, scaling/letterbox policy and integration with presentation interpolation/capture.

### #72 — Input Actions + devices/text

Digital actions/analog axes, project-authored bindings/rebinding, gamepads/deadzones, mouse position/delta/wheel, virtual Agent actions on the same gameplay path and proper text/IME boundary.

### #73 — TileSet/TileMap

Versioned TileSet/TileMap/TileLayer state, atlas metadata, deterministic cells/layers, bounded representation/chunking, batching/culling, collision handoff and semantic Agent query/assert.

### #74 — Production UTF-8 text/localization

Font asset identity/cache, UTF-8, Korean/CJK-capable output, measurement/wrapping/alignment/fallback and localization-facing hooks. No unchanged glyph rerasterization every frame.

### #75 — Practical deterministic UI

Hierarchy, anchors/pivot, resolution scaling, small deterministic layouts, practical widgets and keyboard/gamepad navigation while preserving semantic/headless authority.

### #89 — Material2D / Shader2D

A small programmable 2D surface for hit flash/outline/dissolve/grayscale/palette/UV effects.

Initial direction:

- standard Sprite vertex ABI,
- custom fragment stage first,
- typed setup-resolved parameters,
- cached shaders/pipelines/samplers,
- material-aware contiguous batching,
- no material graph/render graph.

### #90 — Deterministic resolved-property tween animation

Fixed-step property animation for transforms/UI/color/camera/game properties through setup-resolved typed bindings. No per-frame property-string lookup or generic reflection.

### #76 — Physics2D

Practical mature 2D physics integration with finite typed body/collider/trigger components, layers/masks, ray/overlap queries, fixed-step integration and explicit determinism boundary.

### #77 — Audio

Project-relative clips/cache, source playback state, play/stop/pause/volume/pitch/loop and semantic headless audio commands/events.

### #91 — Unified profiler/diagnostics

One bounded machine-readable surface that separates deterministic structural metrics, CPU machine timing, optional GPU timing and known resource-memory evidence. Hosted CI may gate structural budgets; timing thresholds need stable hardware.

### #78 — Linux/compiler/toolchain hardening

Windows/MSVC remains supported; add Linux + Clang/GCC path as current dependency support permits, plus targeted sanitizers/static analysis where reliable.

### #92 — Real-GPU conformance

Tier A hosted/backend-independent validation, Tier B maintained real-GPU baseline before stable production rendering claims, Tier C vendor/backend release matrix only for claims actually covered. No universal exact-pixel assumption.

### #79 — Persistence + authored migration

Versioned game persistence plus explicit authored project/scene/component migration with safe writes and no silent data loss.

### Constraints

P8 does **not** authorize a generic ECS, generic runtime reflection, binary plugin ABI, custom allocator framework, job system, render graph, material graph, visual scripting, lock-free infrastructure or a broad editor.

Complexity continues to follow measured requirements.

## P9 — Flagship external sample game (#12)

Status: **blocked by P8**.

Build one deliberately small real game through the exact public/external contracts established by the engine.

It must not be an engine-internal test fixture or use sample-only hidden shortcuts.

It should exercise a coherent subset of:

- external C++ game module,
- project manifest/build/test/package consumer flow,
- hierarchy/components including at least one user-defined gameplay component,
- reusable SceneTemplate/world instancing,
- shared resource lifecycle/memory evidence,
- Camera2D/Viewport2D,
- Input Actions,
- TileMap,
- production UTF-8 text/UI,
- Material2D,
- deterministic tween,
- Physics2D,
- semantic audio,
- profiling,
- Windows and the non-MSVC platform/toolchain,
- applicable real-GPU conformance evidence,
- persistence/migration,
- headless semantic QA,
- exact-frame visual capture.

This is the practical open-source-engine proof before lower-priority compatibility breadth.

## P10 — Generic Mesh2D foundation (#60)

Status: **blocked by #59, P8 and #12**.

Purpose: reusable arbitrary textured indexed 2D geometry without bloating SpriteRenderer or building Spine-specific rendering infrastructure.

Fixed order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent/capacity-reused dynamic geometry + conformance/workloads
```

Expected data includes positions, UVs, indices, vertex color, texture/material compatibility and stable painter order.

Mesh2D remains presentation state and reuses project/resource/distribution contracts already established by P8.

## P11 — Spine compatibility gate and optional integration (#61)

Status: **planned; blocked by #59, P8, #12 and #60; then blocked at SP0 human license gate**.

Detailed contract: [`SPINE.md`](SPINE.md).

Spine remains a desired compatibility target, not Trace2D's native animation/project architecture.

Before SP0 approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download build dependency,
- no Spine-containing prebuilt binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

Only after explicit owner approval of the then-current public MIT core / optional integration / source / binary / CI / notice / downstream-user licensing model may the sequence continue.

## P12 — Recorded later production breadth gates (#93)

Status: **recorded; not in routine core continuation until owner promotion**.

These are genuine remaining mature-engine gaps, not forgotten features:

### 2D lighting/shadows

Build on #89 Material2D/Shader2D and #60 geometry when a representative game proves need. Prefer a small `Light2D`/occluder/layer-mask contract before clustered/deferred complexity.

### Navigation/pathfinding

After TileMap and a real workload, prefer deterministic grid/A* with explicit tie breaking before broad navmesh infrastructure.

### Broader platforms

After #78, promote macOS/mobile/Web only with build/test/runtime ownership. Touch/sensor/lifecycle input feeds the same semantic Input Actions model.

### Networking

Choose concrete authority/rollback/serialization semantics only from an actual network-game requirement; do not build speculative replication.

### Safe hot reload

Requires #86 generation-safe resources and #79 versioned migration first. Data/resource replacement must be transactional. Native C++ binary hot reload is a separate ABI/lifecycle decision.

## Open-source contribution model

The fixed sequence governs **core roadmap progression and `Trace2D next/continue`**.

Independent community contributions may still be reviewed when narrow fixes/tests/docs/portability improvements do not overlap the active core implementation, preempt a frozen future architecture, add unresolved dependency/license obligations or violate hard architecture/performance contracts.

See `AGENTS.md` and Issue #80.

## Long-term proof

The desired engine workflow is:

```text
external game/project source
 -> build/import/generate
 -> deterministic normalization/validation
 -> headless run
 -> structured inspect/query
 -> virtual semantic actions + exact stepping
 -> gameplay/UI/particle/sprite/tile/physics/audio assertions
 -> explicit structural/timing profiling when relevant
 -> deterministic resource/world lifecycle inspection
 -> explicit human gates only where contracts require them
 -> visual/audio presentation QA
 -> real-GPU conformance appropriate to the support claim
 -> save/migrate supported authored/user data
 -> structured failure context back to the coding agent
```

Trace2D succeeds when this workflow works for a real third-party-style game without requiring hidden editor state or engine-source edits.