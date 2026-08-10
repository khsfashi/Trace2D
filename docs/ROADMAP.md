# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable, documented slice rather than disconnected engine subsystems.

`PROJECT_STATUS.md` is the operational source for the exact current issue/PR. This file records the longer owner-approved direction. Live code/PR/CI state wins over stale prose.

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

The roadmap therefore has two responsibilities:

1. deepen the agent-verifiable runtime/rendering foundation,
2. make that foundation usable by third-party game developers through a coherent project/game/world lifecycle.

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

Status: **complete for the current baseline**.

Delivered:

- generation-safe runtime entity identity,
- stable authored semantic IDs,
- name/tags,
- `Transform2D`,
- versioned TOML scenes,
- canonical deterministic serialization,
- structured schema diagnostics.

A production world hierarchy/component model is intentionally deferred to #71 rather than retrofitted speculatively into the initial proof.

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

Gameplay-facing semantic Input Actions and broader devices are a later external-engine requirement in #72.

## P5 — 2D renderer and exact-frame capture

Status: **complete for Public Alpha baseline**.

Delivered:

- SDL3 GPU boundary,
- orthographic camera,
- baseline textured sprites,
- inclusive AABB culling,
- order-preserving contiguous same-texture instancing,
- persistent/capacity-reused resources,
- offscreen presentation/capture target,
- explicit simulation-frame capture,
- deterministic CPU-normalized BMP artifact,
- reproducible renderer workloads and metric boundaries.

The production traditional SpriteRenderer is a larger separate program under #59.

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
```

Exact next core task:

```text
#50 complete Agent particle verification
 -> #51 CPU cost + human backend choice + deterministic compiler
 -> #52 explicit GPU runtime
 -> #53 CPU/GPU conformance, workloads and guidance
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

Hard rules remain:

- CPU is the semantic oracle,
- backend choice is explicit and reviewable,
- an LLM may recommend but never silently switch,
- unsupported GPU selection fails clearly rather than silently falling back,
- normal GPU mode does not duplicate full CPU reference simulation,
- no fake portable CPU percentages,
- no universal cross-vendor bit-identical GPU float claim without proof.

## P7 — Complete Sprite program (#59)

Status: **owner-fixed future program after #53**.

Detailed contract: [`SPRITES.md`](SPRITES.md).

This is intentionally broader than a minimal SpriteRenderer or frame-animation feature.

Fixed order:

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

Live image generation may be nondeterministic; deterministic post-processing/runtime CI uses recorded/synthetic fixtures.

## P8 — Open-source game-production foundation (#67)

Status: **owner-fixed future program after #59**.

Detailed contract: [`GAME_PRODUCTION.md`](GAME_PRODUCTION.md).

This phase exists because an open-source engine needs more than sophisticated internals: a third party needs a coherent answer to where game code lives, how a project is consumed, how world state is composed, and how practical 2D game systems integrate with the same headless Agent model.

Fixed children:

```text
#69 E0 Game/Application module boundary
 -> #70 E1 project manifest + external consumer build/install/package
 -> #71 E2 scene hierarchy + typed authored component composition
 -> #72 E3 Input Actions + gamepad/mouse/text/IME
 -> #73 E4 TileSet/TileMap
 -> #74 E5 production UTF-8 font/text/localization
 -> #75 E6 practical deterministic UI hierarchy/layout/widgets
 -> #76 E7 Physics2D
 -> #77 E8 Audio
 -> #78 E9 Linux/compiler/toolchain hardening
 -> #79 E10 save/persistence + authored schema migration
```

### Why this precedes Mesh2D/Spine

Trace2D's deterministic/Agent/rendering foundations are already deeper than its user-facing game architecture. Before prioritizing compatibility-oriented arbitrary meshes or Spine, external users need:

- a supported C++ game/application boundary,
- a versioned project root and external consumer/package flow,
- hierarchy/components rather than subsystem-specific parallel world models,
- semantic input actions and real devices/text input,
- tilemaps,
- real UTF-8/font/CJK text,
- practical UI layout,
- physics,
- audio,
- more than one continuously validated compiler/platform path,
- save data and authored-schema migration.

These are foundational engine capabilities, not feature-count padding.

### Constraints

#67 does **not** authorize a generic ECS, reflection system, binary plugin ABI, custom allocator framework, job system, render graph, material graph, visual scripting system, broad editor, or lock-free infrastructure.

Complexity still follows measured requirements.

### Existing implementation hardening assigned to #67

- texture-handle tombstones/reuse: generation-safe recycling only when realistic churn proves need,
- shader packaging: #70 owns offline/reproducible build artifacts if distribution requires them,
- current 5x7 ASCII path remains a deterministic fixture; #74 adds production text,
- alpha authored formats have no migration tooling; #79 closes that gap.

## P9 — Flagship external sample game (#12)

Status: **blocked by #67**.

After #69-#79 complete, build one deliberately small real game through the exact external/public contracts established by the engine.

It must not be an engine-internal test fixture or use sample-only hidden shortcuts.

The proof should exercise a coherent subset of:

- external C++ game module,
- project manifest/build/test consumer flow,
- hierarchy/components,
- Input Actions,
- TileMap,
- production UTF-8 text/UI,
- Physics2D,
- semantic audio,
- persistence/migration,
- headless semantic QA,
- exact-frame visual capture,
- Windows and the non-MSVC platform/toolchain from #78,
- reproducible performance evidence for at least one relevant workload.

This is the practical open-source-engine proof before later compatibility breadth.

## P10 — Generic Mesh2D foundation (#60)

Status: **blocked by #59, #67 and #12**.

Purpose: reusable arbitrary textured indexed 2D geometry without bloating SpriteRenderer or building Spine-specific rendering infrastructure.

Fixed order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent/capacity-reused dynamic geometry + conformance/workloads
```

Expected data includes positions, UVs, indices, vertex color, texture, blend mode, and stable painter order.

Mesh2D remains presentation state and must reuse the project/resource/distribution contracts already established by #67.

## P11 — Spine compatibility gate and optional integration (#61)

Status: **planned; blocked by #59, #67, #12 and #60; then blocked at SP0 human license gate**.

Detailed contract: [`SPINE.md`](SPINE.md).

Spine remains a desired compatibility target, not Trace2D's native animation/project architecture.

Before SP0 approval:

- no `spine-cpp` vendoring/copy,
- no package/submodule/download build dependency,
- no Spine-containing prebuilt binary,
- no Spine-derived implementation code,
- no claim that Trace2D ships Spine support.

Only after explicit owner approval of the then-current public MIT core / optional integration / source / binary / CI / notice / downstream-user licensing model may the sequence continue:

```text
SP1 official optional runtime adapter/version boundary
 -> SP2 loading + Mesh2D rendering
 -> SP3 semantic animation/track/skin/slot/attachment/event state
 -> SP4 Agent/MCP QA + conformance/workloads
```

## Open-source contribution model

The fixed sequence governs **core roadmap progression and the `Trace2D next/continue` command**.

Independent community contributions may still be reviewed when they are narrow fixes, tests, docs, portability improvements, or isolated enhancements that do not overlap the active core implementation, preempt a future owner design, add unresolved dependency/license obligations, or violate hard architecture/performance contracts.

See `AGENTS.md` and Issue #80.

## Later candidates after the fixed sequence

Possible later areas remain valid only after a concrete owner decision and demonstrated need, for example:

- safe hot reload,
- additional desktop/mobile/Web targets,
- networking,
- navigation/pathfinding,
- richer audio features,
- editor tooling built on the text/semantic contracts,
- additional rendering features.

They are not license to bypass the current fixed sequence.

## Long-term proof

The desired engine workflow remains:

```text
external game/project source
 -> build/import/generate
 -> deterministic normalization/validation
 -> headless run
 -> structured inspect/query
 -> virtual semantic actions + exact stepping
 -> gameplay/UI/particle/sprite/tile/physics/audio assertions
 -> performance/cost analysis when relevant
 -> explicit human gates only where contracts require them
 -> visual/audio presentation QA
 -> save/migrate supported authored/user data
 -> structured failure context back to the coding agent
```

Trace2D succeeds when this workflow works for a real third-party-style game without requiring hidden editor state or engine-source edits.
