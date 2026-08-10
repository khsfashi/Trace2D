# Open-Source Game Production Foundation

Roadmap umbrella: **#67**.

This document defines the owner-approved phase that turns Trace2D from a strong deterministic/Agent-verifiable engine core into an open-source 2D engine that a third party can actually use for a real game without editing engine internals.

The detailed cross-subsystem integration rules are additionally frozen in [`PRODUCTION_ARCHITECTURE_CONTRACTS.md`](PRODUCTION_ARCHITECTURE_CONTRACTS.md). Both documents are required reading for #69-#79 and #86-#92.

## Why this phase exists

Trace2D already has unusually strong foundations for deterministic execution, semantic inspection, headless testing, structured Agent interaction, measurement-driven rendering, assets/UI/MCP and the particle/Sprite direction.

The largest architectural gap is the production glue required by real external games:

> Where does user game code live, how is a project identified/consumed, how is authored/runtime world state composed, how are reusable resources/world instances/cameras/materials owned, and how does a third party build/run/test/profile/package a real game through supported contracts instead of editing Trace2D itself?

An open-source engine is not complete merely because individual subsystems are sophisticated. The user-facing project/game/world/resource/presentation lifecycle must be explicit.

## Entry gate and global order

The owner-fixed core sequence is:

```text
#52 -> #53
 -> #59 complete Sprite program
 -> #69
 -> #70
 -> #71
 -> #86
 -> #87
 -> #88
 -> #72
 -> #73
 -> #74
 -> #75
 -> #89
 -> #90
 -> #76
 -> #77
 -> #91
 -> #78
 -> #92
 -> #79
 -> #12 flagship external game proof
 -> #60 Mesh2D
 -> #61 Spine SP0 license gate
```

The architecture freeze #85/PR #94 is a contract predecessor, not an implementation stage. It defines semantics now so #59 and this program do not reinvent them independently.

Issue #93 records later production gaps but is not routine core continuation until explicitly promoted.

## Non-negotiable architecture rules

- User game code must not require modifying files under `engine/`.
- Prefer a simple C++ source/static-library consumer contract before inventing a binary plugin ABI.
- One Scene/world model owns engine components and registered user/game components.
- Do not introduce a generic ECS, generic reflection system, custom allocator framework, job system, render graph, material graph, visual scripting system, lock-free framework or full graphical editor merely because a new subsystem needs composition.
- Authored content remains versioned, text-first, diffable and deterministic where practical.
- Stable semantic identity beats pointers, allocation addresses, C++ RTTI names or unspecified container order.
- Structured engine/game-owned state/events remain the semantic correctness oracle. Pixels and audible output remain presentation/QA evidence.
- Expensive inspection, migration, asset processing, capture, profiling, reporting and Agent serialization remain explicit work rather than mandatory frame-loop work.
- Hot paths resolve strings/paths/resources/property bindings during setup where practical and reuse persistent/capacity-managed state.
- Performance claims require workload evidence. Machine timing and deterministic structural metrics remain distinct.
- External dependencies require then-current license/distribution review before inclusion.
- Tracing GC and mandatory atomic shared ownership are not default resource-lifetime strategies.

## Fixed implementation order

### #69 — Game/Application module boundary

Define the lifecycle and ownership relationship between Trace2D and user C++ game logic.

Required direction:

- a game lives outside engine internals,
- one explicit application/game lifecycle owns startup, fixed-step game update, shutdown and access to required engine services,
- headless and windowed execution compose the same game logic,
- SDL/MCP/backend types do not leak into user gameplay contracts,
- an external minimal sample proves the boundary,
- no plugin ABI or generic service locator/reflection framework without demonstrated need.

This stage answers: **where does the game go?**

### #70 — Project manifest + external consumer build/install/package

Make a Trace2D game a first-class project.

Expected capabilities:

- versioned project manifest,
- stable project ID,
- startup scene/content,
- fixed-step/display/asset/input configuration only where justified,
- documented external CMake consumer path,
- install/export/package support as appropriate,
- clean build/run/test commands discoverable from project root,
- distribution notices for resolved dependencies,
- no source-tree-only undocumented shortcuts.

Distribution-facing shader policy belongs here. Runtime shader setup is already outside the frame hot path, but distributable builds should prefer reproducible build/offline shader validation or artifacts when the pinned backend/toolchain supports a clean solution.

Do not build a generic asset compiler merely to eliminate one runtime setup step.

### #71 — Scene hierarchy + engine/game typed component composition

Evolve Scene from identity + `Transform2D` into the coherent authored world model.

Required world direction:

- deterministic parent/child hierarchy,
- local/world transforms,
- cycle rejection,
- deterministic reparent/child ordering,
- stable semantic identity,
- finite typed engine component composition,
- versioned component serialization/validation,
- Agent visibility over authoritative hierarchy/components,
- subsystems reuse one world model rather than parallel entity graphs.

#### External user/game components

This stage must also prove at least one externally registered authored gameplay component.

Frozen semantics:

- stable explicit component type ID,
- explicit schema version for authored component types,
- registration frozen before authored scene load,
- authored vs runtime-only component classification,
- explicit typed parse/validate/serialize/inspect adapters,
- strongly typed external C++ access,
- generation-safe invalidation,
- one instance/type/entity baseline,
- optional explicit writable-property adapter for future #90,
- no generic `map<string, Variant>` truth model,
- no C++ RTTI/allocation-address authored identity.

Deterministic authored load order is:

```text
create identities
 -> construct components in authored order
 -> parse/validate
 -> resolve entity/resource references
 -> establish hierarchy/world transforms
 -> publish scene-ready state to Game/Application
```

Agent observation may allocate copied values only on explicit request.

### #86 — Unified typed resource lifecycle

Unify the resource rules required by Sprite/Tile/Font/Audio/Material and future assets.

Required semantics:

- typed project-relative authored references,
- absolute/traversal rejection,
- setup-time canonicalization,
- small generation-safe resolved runtime handles,
- CPU canonical asset state separate from GPU/backend resources,
- successful immutable resource caching/reuse,
- explicit dependency graph,
- strong dependency cycle diagnostics unless an asset class explicitly supports cycles,
- explicit unload/release safe points,
- no tracing GC,
- no mandatory atomic shared ownership in render/game hot paths,
- no filesystem/path hashing during normal per-frame access,
- CPU retained bytes and engine-created GPU resource evidence reported separately,
- hot reload not implied.

The implementation should prove the common contract with at least two resource classes.

### #87 — Reusable scene templates, instancing and world lifecycle

Provide one common reusable text-authored hierarchy model rather than subsystem-specific factories.

Required semantics:

- working concept `SceneTemplate` or one final equivalent public name,
- stable template-local entity IDs,
- explicit stable runtime template-instance identity,
- semantic child identity derived from world/scene + instance + template-local identity,
- deterministic component/entity construction order,
- explicit root transform on instantiation,
- typed authored overrides only through registered component schemas,
- no generic free-form property bag,
- structural changes requested during a fixed step apply at a documented deterministic safe point in request order,
- despawn invalidates generation-safe handles,
- no silent engine-wide pooling of arbitrary gameplay entities,
- explicit scene/world instance load/unload,
- deterministic additive world update/observation ordering,
- synchronous baseline loading is acceptable; streaming is later optimization.

### #88 — Camera2D / Viewport2D

Turn current orthographic renderer math into a practical world/project presentation contract.

#### Camera2D

A typed world component with:

- entity/world transform position,
- 2D rotation when implementation supports it,
- positive orthographic `vertical_size` preserving current renderer convention,
- enabled state,
- deterministic priority/selection key,
- target viewport identity,
- optional bounds/clamp,
- optional engine-owned follow/smoothing advanced on fixed steps.

#### Viewport2D

A stable logical viewport independent from OS-window identity, with:

- logical size,
- presentation target/rectangle,
- scaling mode,
- active camera,
- persistent renderer target when required.

Practical scaling modes may include `fit`, `fill` and `stretch` with exact mapping rules.

World-to-screen/screen-to-world math is CPU/backend-independent and headless-testable.

Camera shake, when engine-owned, is explicit presentation state and does not silently move gameplay targets.

Capture records simulation frame + viewport/camera + interpolation mode.

### #72 — Semantic Input Actions + practical devices/text input

Keep the deterministic low-level input foundation, but expose gameplay semantics rather than direct physical-key coupling.

Expected capabilities:

- digital actions and analog axes,
- project-authored bindings/rebinding,
- gamepad buttons/axes and explicit deadzones,
- mouse position/delta/wheel,
- virtual Agent/test actions on the same gameplay-facing path,
- deterministic frame/reset/replay behavior,
- proper text-input/IME boundary.

Normal action reads are resolved/indexed and allocation-light after setup.

### #73 — TileSet / TileMap / TileLayer

Tilemaps are a first-class 2D-engine capability and especially suitable for text/structured Agent authoring.

Expected capabilities:

- versioned `TileSet` identity and atlas/source metadata,
- deterministic map/layer/cell conventions,
- stable flip/rotation semantics,
- bounded dimensions/chunking chosen from real workloads,
- painter-order integration,
- culling/batching without per-tile draw calls,
- animated tiles only when justified,
- collision metadata handoff,
- semantic Agent query/assert by map/layer/cell/tile identity.

Do not represent large maps as one heap object/string per tile.

### #74 — Production UTF-8 font/text/localization foundation

The existing 5x7 ASCII path remains a deterministic narrow fixture, not the production text system.

Expected capabilities:

- project-relative font asset identity,
- UTF-8 text,
- glyph/font cache or atlas with explicit ownership,
- text measurement,
- wrapping/alignment,
- fallback policy,
- Korean/CJK-capable output,
- text/IME integration,
- localization-facing hooks,
- shaping/kerning only through concrete requirements and mature reviewed dependencies.

Do not rerasterize unchanged glyph/text work every frame without reason.

### #75 — Practical deterministic UI hierarchy/layout/widgets

Preserve semantic `UiDocument` and Agent-first targeting while adding real HUD/menu composition.

Expected capabilities:

- UI hierarchy,
- anchors/pivot,
- resolution scaling,
- small deterministic layout primitives,
- margin/padding where justified,
- practical image/progress/scroll-style widgets,
- keyboard/gamepad focus/navigation,
- production text from #74,
- layout results inspectable/assertable headlessly.

Do not clone DOM/CSS.

### #89 — Material2D / Shader2D

Provide the small programmable rendering surface required by common 2D effects without a material/render graph.

Frozen direction:

- project-relative `Shader2D`,
- existing pinned SDL3 GPU/shadercross toolchain,
- Trace2D shader ABI rather than backend-owned public APIs,
- standard Sprite vertex path,
- custom fragment stage first,
- finite typed `Material2D` parameter layout,
- setup-time parameter-name to binding-index/offset resolution,
- cached shaders/pipelines/samplers,
- bounded resolved per-instance overrides,
- renderer batch compatibility includes material/pipeline state while preserving painter order,
- no shader compilation or filesystem discovery during ordinary drawing,
- unsupported backend/shader behavior fails explicitly.

Common Sprite tint/opacity stays on the optimized Sprite path rather than automatically becoming generic material parameters.

### #90 — Deterministic resolved-property tween animation

Add fixed-step property animation for UI/transform/color/camera/game-state use cases.

Frozen semantics:

- integer fixed-step timeline authority,
- explicit delay/duration/repeat/ping-pong/pause/resume/stop/restart semantics,
- small finite easing set with committed formulas/tests,
- finite supported interpolable value types,
- authored target resolves during load/setup to a compact typed property binding,
- engine/game components explicitly opt into writable bindings,
- no generic reflection,
- no selector/component/property string lookup every frame,
- deterministic target-conflict policy,
- stale target generation causes cancel/error, not stale memory access,
- Agent inspection of target/progress/state/completion.

### #76 — Physics2D

Add practical 2D physics through a reviewed mature dependency such as Box2D when then-current API/license/performance requirements fit.

Expected engine contract:

- finite typed body/collider/trigger components,
- stable entity/fixture identity,
- collision layers/masks,
- raycast/overlap queries,
- fixed-step integration,
- structured contacts/triggers/query results,
- deterministic observable ordering where Trace2D owns ordering,
- explicit determinism boundary for third-party floating-point simulation.

Do not claim cross-platform bit-identical physics unless separately proven.

### #77 — Audio

Add a small practical 2D audio system.

Expected capabilities:

- project-relative audio clip identity/cache,
- source/playback state,
- play/stop/pause as required,
- volume/pitch/loop,
- small groups/buses if demonstrated,
- optional simple 2D pan/attenuation only when justified,
- semantic audio commands/events/state in headless tests,
- physical audio output treated as presentation.

### #91 — Unified Agent-readable profiler and diagnostics

Unify existing renderer/particle metrics and future subsystem evidence.

Keep categories separate:

1. deterministic structural metrics,
2. CPU machine timing with environment metadata,
3. GPU timing when supported,
4. known resource memory evidence.

Intended surface is equivalent to:

```text
trace2d profile <project/workload> --frames N --json
```

Requirements:

- compact pre-resolved scope IDs,
- bounded/ring/capacity-reused history,
- no normal-frame JSON/report building,
- no global allocator interception requirement,
- structural CI budgets allowed,
- machine timing thresholds only on stable dedicated runners.

### #78 — Linux/compiler/toolchain hardening

Before stable external adoption, continuously validate more than one implementation environment.

Expected direction:

- Windows/MSVC remains supported,
- add Linux + Clang/GCC based on current dependency/backend support,
- headless engine and external consumer build/test in CI,
- compiler-appropriate warnings,
- targeted ASan/UBSan where reliable,
- evaluate clang-tidy/static analysis only for high-signal checks,
- fix portability/UB rather than suppressing diagnostics broadly.

### #92 — Tiered real-GPU conformance/release validation

Close the gap between CPU-side renderer correctness and actual GPU execution.

#### Tier A — always-on hosted/headless

- geometry/UV/order/culling/batch math,
- camera/viewport/interpolation math,
- material layout/shader validation where possible,
- canonical CPU artifact paths,
- particle compiler/layout semantics.

#### Tier B — maintained real-GPU baseline

Before a stable production-oriented rendering claim, maintain at least one real GPU environment for representative conformance. Record OS/GPU/vendor/driver/backend/build metadata.

#### Tier C — support-claim matrix

Claims spanning multiple vendors/backends require explicit release matrix evidence. Missing infrastructure narrows claims rather than being hidden.

GPU comparisons use exact equality only where proven; otherwise committed tolerances. Screenshot comparison never replaces semantic gameplay assertions.

Readback/fence/image comparison remains explicit validation work.

### #79 — Save/persistence + authored schema migration

Close the long-term external-user lifecycle gaps.

#### Game persistence

- stable semantic save schema,
- no raw runtime pointers/handles as persisted truth,
- safe writes,
- structured corrupt/unsupported diagnostics,
- explicit save-version migration,
- deterministic serialization when ordering matters,
- headless save/reset/load verification.

#### Authored schema migration

- declared migration policy,
- deterministic reviewable rewrite tooling,
- clear source/version/field diagnostics,
- no silent data loss,
- documented supported migration range.

## Fixed-step presentation interpolation requirement from #85

Although implemented as part of #59 Sprite/runtime presentation, all future world/camera systems must preserve the same authority split.

Authoritative moving state conceptually retains:

```text
previous_fixed
current_fixed
```

Interactive rendering may interpolate. Gameplay/Agent uses `current_fixed`. Reset/load/teleport synchronize history. Exact-frame capture renders authoritative current state unless an explicit sub-frame alpha is requested and recorded.

For hierarchy, interpolate local transform state before world composition.

No per-sprite selector lookup/transient heap list is required for interpolation.

## Existing implementation hardening tracked by this program

### Renderer texture-handle tombstones

Current destroyed texture handles leave tombstones and are not recycled. #86 owns generation-safe resource-handle reuse when the unified resource model is implemented; do not bolt a conflicting renderer-only lifetime model onto the alpha table.

### Runtime shader compilation

Current shadercross compilation occurs during renderer setup, not normal frames. #70/#89/#78 own reproducible distribution/build shader artifacts when the production material/package path requires them.

### ASCII UI raster path

The current dependency-free 5x7 path remains valuable for deterministic tests/fallback. #74 adds production text rather than deleting the fixture.

### Schema migration

Alpha formats may change incompatibly. #79 closes that gap before stronger external compatibility promises.

## Flagship proof after the production foundation

When every child above is complete, #12 builds one deliberately small real game through ordinary public/external Trace2D contracts.

The game must not use sample-only engine shortcuts. It should prove a coherent subset of:

- external game module,
- external user-defined typed gameplay component,
- project manifest/consumer build/package,
- hierarchy/components,
- reusable scene-template instancing/world lifetime,
- shared resource lifecycle/memory inspection,
- Camera2D/Viewport2D,
- Input Actions,
- TileMap,
- production UTF-8 text/UI,
- Material2D effect,
- deterministic tween,
- Physics2D,
- semantic audio,
- profiler output,
- Windows plus non-MSVC platform/toolchain,
- applicable real-GPU conformance,
- persistence/migration,
- headless Agent QA and exact-frame capture.

Only after this proof does the fixed roadmap advance to #60 Mesh2D and #61 Spine.

## Later genuine gaps (#93)

The following are recognized but deliberately deferred from routine core progression:

- 2D lighting/shadows,
- navigation/pathfinding,
- macOS/mobile/Web/other platform expansion,
- networking,
- safe hot reload.

Each has entry prerequisites and must receive an explicit owner promotion before coding agents treat it as core work.

## Core continuation vs community contribution

The strict owner-fixed order governs `@GitHub Trace2D 다음 진행해줘`, `Trace2D next` and equivalent continuation prompts.

Independent open-source contributions may proceed only when narrowly scoped and non-conflicting with active/frozen architecture, dependency/license policy, determinism, ownership and performance rules.

## Completion condition

The production foundation is complete only when every fixed implementation child listed above is merged green and repository docs/status agree.

The handoff is:

```text
#12 flagship external game proof
 -> #60 Mesh2D
 -> #61 Spine SP0
```