# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable, documented slice rather than disconnected engine subsystems.

`PROJECT_STATUS.md` is the operational source for the exact current issue/PR. Compiling code, live PR/CI state and explicit owner decisions win over stale prose.

## Product direction

Trace2D is not trying to clone a large editor-first engine feature-for-feature.

It is an **AI-first, AI-operated C++ 2D game engine** built around this product contract:

> **Humans define intent and judge the result. AI owns the iteration in between.**

Short form:

> **Tell AI what to build. Review the result.**

The key judgment rule is:

> **Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

The roadmap therefore has four responsibilities:

1. deepen deterministic/observable engine foundations,
2. make AI-operated author -> run -> verify -> diagnose -> repair -> re-verify loops first-class,
3. make those foundations practical for real external 2D games without editor-only state,
4. **measure** whether Trace2D actually improves autonomous game-development workflows against recorded baselines.

Primary cross-cutting contracts:

- [`AI_OPERATED_WORKFLOW.md`](AI_OPERATED_WORKFLOW.md) — product identity and judgment/feedback loop,
- [`AGENT_FIRST_PRINCIPLES.md`](AGENT_FIRST_PRINCIPLES.md) — non-negotiable Agent/AI rules,
- [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md) — frozen production integration seams,
- [`PRODUCTION_GAPS.md`](PRODUCTION_GAPS.md) — missing/shallow capability register,
- [`AUTONOMOUS_BENCHMARK.md`](AUTONOMOUS_BENCHMARK.md) — matched benchmark methodology,
- [`WORKSPACE.md`](WORKSPACE.md) — human result-review surface,
- [`REFERENCE_PROJECTS.md`](REFERENCE_PROJECTS.md) — external projects and lessons.

## Fixed high-level execution sequence

Routine `Trace2D next/continue` follows `PROJECT_STATUS.md`, but the intended long-term sequence is:

```text
#52 -> #53

#97 Intent / Definition of Done
 -> #98 verify / diagnose / repair / WorkResult
 -> #99 minimal Workspace / human feedback loop
 -> #102 Benchmark B0

#59 complete Sprite program
 -> #103 Benchmark B1 Sprite/animation/particle tasks

#69 Game/Application
 -> #70 Project/build/package
 -> #71 Scene hierarchy + typed components
 -> #86 unified resources
 -> #87 scene templates/world lifecycle
 -> #88 Camera2D/Viewport2D
 -> #72 Input Actions
 -> #73 TileMap
 -> #74 production text/localization
 -> #75 practical UI
 -> #104 Benchmark B2 autonomous top-down combat micro-game
 -> #89 Material2D/Shader2D
 -> #90 deterministic tween/property animation
 -> #76 Physics2D
 -> #77 Audio
 -> #91 profiler/diagnostics
 -> #78 Linux/non-MSVC hardening
 -> #92 real-GPU conformance
 -> #79 persistence/migration

#12 flagship external game
 -> #60 Mesh2D
 -> #61 Spine SP0
```

Umbrellas/registers:

- #96 — AI-operated production loop,
- #100 — autonomous benchmark program,
- #93 — later mature-engine breadth,
- #101 — production capability gap register.

The benchmark stages are intentionally **interleaved** with engine development. Trace2D must not wait until the engine is "finished" before testing its own AI-first claim.

---

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

The production hierarchy/component world is future #71.

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

Gameplay-facing Input Actions and broader devices are future #72.

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

Completed predecessors:

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

Current remaining particle order:

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
 -> HUMAN backend decision cpu|gpu where the current contract requires it
 -> deterministic minimized GPU program
 -> GPU runtime for explicitly gpu-selected effects
 -> CPU/GPU conformance + visual/performance QA
```

Hard rules:

- CPU is the semantic oracle,
- backend choice is explicit and reviewable,
- an LLM may recommend but never silently switch where the human gate applies,
- unsupported GPU selection fails clearly rather than silently falling back,
- normal GPU mode does not duplicate full CPU reference simulation,
- no fake portable CPU percentages,
- no universal cross-vendor bit-identical GPU float claim without proof.

## P6.5 — Production architecture contract freeze (#85)

Status: **complete via PR #94**.

The freeze established seams for:

- external user-defined typed gameplay components,
- authoritative fixed-step state versus interpolated presentation state,
- unified typed resources/lifetimes/dependencies/memory evidence,
- reusable scene templates and deterministic world lifecycle,
- Camera2D/Viewport2D,
- Material2D/Shader2D,
- deterministic setup-resolved property tweening,
- unified Agent-readable profiling/diagnostics,
- tiered real-GPU conformance,
- later lighting/navigation/platform/networking/hot-reload gates.

These contracts are not feature implementations, but later work must honor them.

---

## P6.6 — AI-operated production foundation (#96)

Status: **owner-fixed after #53, before the long Sprite program**.

Detailed contract: [`AI_OPERATED_WORKFLOW.md`](AI_OPERATED_WORKFLOW.md).

This phase closes the project-level loop that the existing subsystem tools alone cannot provide.

Fixed implementation order:

```text
#97 machine-readable intent / Definition of Done
 -> #98 unified verification / diagnosis / repair / WorkResult
 -> #99 minimal result-review Workspace / feedback loop
 -> #102 Benchmark B0 matched harness + current-capability tasks
```

### #97 — Intent / Definition of Done

A fresh agent should be able to recover what is being built, what counts as complete and which acceptance criteria are deterministic, perceptual or human-final without previous chat history.

This remains a small text/diff-friendly project/tooling contract rather than a generic project-management system.

### #98 — Unified verification / WorkResult

Compose existing engine-owned assertions, diagnostics, metrics and artifacts into a project/task result model suitable for:

```text
verify
 -> structured failure
 -> diagnose
 -> agent edit
 -> re-run
 -> re-verify
 -> review package
```

No second gameplay truth model and no mandatory per-frame JSON/report work.

### #99 — Workspace

Provide the minimum human review surface required by the product loop.

Preferred human interaction:

```text
Read -> Review -> Request -> Approve
```

Required direction:

- recent AI work/results,
- current project/world orientation,
- deterministic verification status,
- review queue,
- animation/particle/UI/gameplay preview evidence as capabilities permit,
- revision history,
- targeted natural-language feedback,
- same Agent/result state as CLI/MCP; no GUI-only authority.

This is **not** a mandate to build a Unity-style property editor.

### #102 — Benchmark B0

Implement the first matched benchmark harness and current-capability tasks before Sprite breadth.

Primary comparison:

```text
Godot + generic coding tools
Godot + pinned reviewed Godot MCP/agent bridge
Trace2D + public Agent surface
```

Record success, iterations, tokens, tool calls, visual/multimodal calls, human interventions and deterministic verification coverage. Published claims use multiple trials and pinned versions.

---

## P7 — Complete Sprite program (#59)

Status: **future after P6.6/B0**.

Detailed Sprite contract: [`SPRITES.md`](SPRITES.md).
Integration contract: [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md).

Fixed internal order:

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

AI-operated requirement:

Sprite/animation work must preserve the three-layer judgment model. Pixel-space metadata, timing/events, pivots, bounds, structural performance and other engine-owned facts are deterministic QA. Motion/style quality may use multimodal review. Final taste/approval remains human.

Production texture/import gaps recorded in `PRODUCTION_GAPS.md` must be explicitly assigned to #59/#70/#86 or a dedicated child before production texture claims are made.

### P7.5 — Benchmark B1 (#103)

Immediately after #59, extend the matched benchmark to the workflows where visual guessing is most dangerous:

- sprite import/normalization,
- pivot/trim/alignment repair,
- deterministic animation/event authoring,
- particle authoring under explicit budgets,
- seeded visual/content defect diagnosis,
- deterministic + presentation + multimodal + human evaluation separation.

This turns the Sprite program into measured AI-operability evidence instead of only subsystem completeness.

---

## P8 — Expanded external game-production foundation

Status: **future after #103**.

Primary contracts:

- [`GAME_PRODUCTION.md`](GAME_PRODUCTION.md),
- [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md),
- [`PRODUCTION_GAPS.md`](PRODUCTION_GAPS.md).

Fixed sequence:

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
 -> #104 Benchmark B2 autonomous combat micro-game
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

Also integrate relevant production texture/package and public source-extension/API compatibility decisions from #101.

### #71 — Scene hierarchy + engine/game typed components

Deterministic hierarchy/local-world transforms plus finite typed component composition.

This stage must prove external user-defined game components with stable type identity/schema, explicit parse/validate/serialize/inspect adapters, strongly typed C++ access and Agent assertions without generic runtime reflection.

Large-world semantic lookup should converge on setup/resolved indexing when representative workloads justify it rather than allowing repeated lookup scans to become a spawn hot path.

### #86 — Unified typed resource lifecycle

Project-relative typed identity, generation-safe resolved handles, CPU/GPU ownership separation, dependency graph, explicit unload/release, cache reuse and memory accounting. No tracing GC or hot-path filesystem/path work.

Integrate the production texture CPU/GPU retention, staging/upload and memory-evidence requirements from #101.

### #87 — Scene templates and world lifecycle

Reusable text-authored entity hierarchies, stable template-local/instance identity, deterministic structural-change safe points, explicit instantiate/despawn and additive world/scene load-unload semantics. No hidden mandatory engine pooling.

### #88 — Camera2D / Viewport2D

Typed camera world state, logical viewport vs OS-window separation, deterministic active-camera selection, backend-independent world/screen conversion, scaling/letterbox policy and integration with presentation interpolation/capture.

### #72 — Input Actions + devices/text

Digital actions/analog axes, project-authored bindings/rebinding, gamepads/deadzones, mouse position/delta/wheel, virtual Agent actions on the same gameplay path and proper text/IME boundary.

Future device breadth should explicitly account for hotplug, touch/gesture, haptics and lifecycle semantics when platform scope requires them.

### #73 — TileSet/TileMap

Versioned TileSet/TileMap/TileLayer state, atlas metadata, deterministic cells/layers, bounded representation/chunking, batching/culling, collision handoff and semantic Agent query/assert.

Before production completion, explicitly decide the relevant #101 items: terrain/autotiling, per-tile semantic metadata, gameplay/object markers, navigation/occlusion handoff and scalable authoring/import representation.

### #74 — Production UTF-8 text/localization

Font asset identity/cache, UTF-8, Korean/CJK-capable output, measurement/wrapping/alignment/fallback and localization-facing hooks. No unchanged glyph rerasterization every frame.

### #75 — Practical deterministic UI

Hierarchy, anchors/pivot, resolution scaling, small deterministic layouts, practical widgets and keyboard/gamepad navigation while preserving semantic/headless authority.

Before production completion, explicitly cover real pointer/event semantics from #101: hit testing, hover/pressed, pointer capture, focus/modal routing, clipping and viewport coordinate conversion.

### #104 — Benchmark B2 autonomous combat micro-game

Do **not** wait for every P8 subsystem before proving a coherent game.

Once the minimum external-game subset above is stable, run a matched task roughly equivalent to:

```text
one room
player movement
one attack
one enemy
health / damage / death
SpriteRenderer2D / SpriteAnimator2D
hit/death particle
small HUD
headless gameplay verification
playable/reviewable final result
```

The trial must include at least one human feedback -> AI revision -> deterministic re-verification cycle.

This is the first explicit product proof that humans can review results and give feedback instead of manually re-authoring the implementation.

### #89 — Material2D / Shader2D

A small programmable 2D surface for hit flash/outline/dissolve/grayscale/palette/UV effects:

- standard Sprite vertex ABI,
- custom fragment stage first,
- typed setup-resolved parameters,
- cached shaders/pipelines/samplers,
- material-aware contiguous batching,
- no material graph/render graph.

### #90 — Deterministic resolved-property tween animation

Fixed-step property animation for transforms/UI/color/camera/game properties through setup-resolved typed bindings. No per-frame property-string lookup or generic reflection.

### #76 — Physics2D

Practical mature 2D physics integration with finite typed body/collider/trigger components, layers/masks, queries, fixed-step integration and explicit determinism boundary.

Before production completion, explicitly decide the #101 subset for joints, CCD, casts/sweeps, material/friction/restitution, one-way platforms, controller helpers and debug/inspection visualization.

### #77 — Audio

Project-relative clips/cache, source playback state and semantic headless audio commands/events.

Production baseline must explicitly decide streaming music/large clips, buses/groups, fades, voice limits/stealing, suspend/resume and decoded/streaming memory budgets. A broad DSP graph remains out of scope without evidence.

### #91 — Unified profiler/diagnostics

One bounded machine-readable surface separating deterministic structural metrics, CPU machine timing, optional GPU timing and known resource-memory evidence. Hosted CI may gate structural budgets; timing thresholds need stable hardware.

### #78 — Linux/compiler/toolchain hardening

Windows/MSVC remains supported; add Linux + Clang/GCC path as current dependency support permits, plus targeted sanitizers/static analysis where reliable.

### #92 — Real-GPU conformance

Tier A hosted/backend-independent validation, Tier B maintained real-GPU baseline before stable production-rendering claims, Tier C vendor/backend release matrix only for claims actually covered. No universal exact-pixel assumption.

### #79 — Persistence + authored migration

Versioned game persistence plus explicit authored project/scene/component migration with safe writes and no silent data loss.

### P8 constraints

P8 does **not** authorize a generic ECS, generic runtime reflection, binary plugin ABI, custom allocator framework, job system, render graph, material graph, visual scripting, lock-free infrastructure or a broad editor.

Complexity follows measured requirements.

---

## P9 — Flagship external sample game (#12)

Status: **blocked by P8**.

Build one deliberately small real game through the exact public/external contracts established by the engine.

It must not be an engine-internal fixture or use sample-only hidden shortcuts.

It should exercise a coherent subset of:

- external C++ game module,
- project manifest/build/test/package flow,
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
- Windows and non-MSVC platform/toolchain,
- applicable real-GPU conformance evidence,
- persistence/migration,
- headless semantic QA,
- exact-frame visual capture,
- AI-operated WorkResult/Workspace review path.

The flagship is broader than #104. It proves practical open-source-engine production breadth after the earlier autonomous micro-game already proved the feedback loop.

## P10 — Generic Mesh2D foundation (#60)

Status: **blocked by #59, P8 and #12**.

Purpose: reusable arbitrary textured indexed 2D geometry without bloating SpriteRenderer or building Spine-specific rendering infrastructure.

Fixed order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent/capacity-reused dynamic geometry + conformance/workloads
```

Expected data includes positions, UVs, indices, vertex color, texture/material compatibility and stable painter order.

Before completion, explicitly decide relevant custom line/polygon/parallax/render-to-texture breadth from #101 rather than accidentally turning Mesh2D into a generic render graph.

## P11 — Spine compatibility gate and skeletal decision (#61 / #101)

Status: **planned; blocked by #59, P8, #12 and #60; then blocked at SP0 human license gate**.

Detailed contract: [`SPINE.md`](SPINE.md).

Spine remains a desired compatibility target, not Trace2D's native animation/project architecture.

Before SP0 approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download build dependency,
- no Spine-containing prebuilt binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

The #101 gap must also be resolved explicitly: if optional Spine integration cannot/should not ship, Trace2D must decide whether to provide a small native skeleton/deform path on Mesh2D or clearly document skeletal animation as out of core scope. Do not leave this accidental.

## P12 — Recorded later production breadth gates (#93 / #101)

Status: **recorded; not in routine core continuation until owner promotion**.

Genuine remaining mature-engine gaps include:

### 2D lighting/shadows

Build on #89 Material2D/Shader2D and #60 geometry when a representative game proves need. Prefer a small `Light2D`/occluder/layer-mask contract before clustered/deferred complexity.

### Navigation/pathfinding

After TileMap and a real workload, prefer deterministic grid/A* with explicit tie breaking before broad navmesh infrastructure.

### Broader platforms/devices

After #78, promote macOS/mobile/Web only with build/test/runtime ownership. Touch/sensor/lifecycle/haptics requirements feed the same semantic Input Actions model.

### Networking

Choose concrete authority/rollback/serialization semantics only from an actual network-game requirement; do not build speculative replication.

### Safe hot reload

Requires #86 generation-safe resources and #79 versioned migration first. Data/resource replacement must be transactional. Native C++ binary hot reload is a separate ABI/lifecycle decision.

### Remaining render/extension breadth

#101 additionally records production texture policy, UI pointer events, audio depth, TileMap authoring semantics, skeletal decision, source-level extension/API compatibility, parallax/custom drawing/compositing and related hardening requirements.

## Open-source contribution model

The fixed sequence governs **core roadmap progression and `Trace2D next/continue`**.

Independent community contributions may still be reviewed when narrow fixes/tests/docs/portability improvements do not overlap the active core implementation, preempt a frozen future architecture, add unresolved dependency/license obligations or violate hard architecture/performance/product contracts.

See `AGENTS.md` and Issue #80.

## Long-term proof

The desired product loop is:

```text
human intent / acceptance
 -> AI plan / source / authored data / generation
 -> build/import/generate
 -> deterministic normalization/validation
 -> headless run
 -> structured inspect/query
 -> semantic actions + exact stepping
 -> gameplay/UI/particle/sprite/tile/physics/audio assertions
 -> explicit structural/timing/resource profiling
 -> structured diagnose/repair/re-verify
 -> presentation capture only where needed
 -> multimodal perceptual review only where needed
 -> WorkResult / Workspace
 -> human review / feedback / approval
 -> requested revision back to AI
```

Trace2D succeeds when this loop works for real external-style games without requiring hidden editor state or engine-source shortcuts — and when matched benchmark evidence can show where Trace2D actually reduces autonomous failure, visual guessing, iteration cost and human intervention.