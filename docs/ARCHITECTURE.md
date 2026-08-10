# Architecture

## Goal

Trace2D is a C++20 2D engine designed around deterministic simulation, structured observability, headless execution and automation by coding agents.

The architecture separates:

- authoritative game/runtime state from presentation state,
- engine semantic APIs from protocol adapters,
- CPU canonical assets from GPU/backend resources,
- text-authored identities from hot-path resolved handles,
- deterministic structural metrics from machine-specific timing.

The production integration seams frozen by owner decision are detailed in [PRODUCTION_ARCHITECTURE_CONTRACTS.md](PRODUCTION_ARCHITECTURE_CONTRACTS.md). Future subsystem implementations must honor that document even when their implementation issue arrives later.

## Dependency direction

Conceptually:

```text
tools / protocol adapters
           |
           v
      agent facade
      /    |    \
     v     v     v
 runtime  world   ui
    |      |      |
  input    |      |
      \    |     /
          core

assets/resources ---> authored/runtime consumers
render -----------> platform
mcp -------------> agent/testing adapter boundaries
external game ----> public game/world/runtime contracts
```

Dependencies point inward. Authoritative runtime/world/UI/game modules never depend on Agent, MCP, JSON, an LLM protocol or renderer backend types.

SDL3 ownership remains behind `engine/platform` and `engine/render`. SDL/GPU types must not leak into core simulation, game component state, authored asset models, UI semantics, input actions or Agent-facing authoritative models.

## State authority classes

Every future subsystem classifies state as follows.

### Authoritative

Determines gameplay/test/save/semantic assertions. Must work headlessly and not depend on renderer initialization.

Examples: current fixed transform, registered game component state, animation timeline, input actions, deterministic tween state, semantic audio commands.

### Presentation

Derived for output and may depend on viewport, interpolation alpha, GPU resources or backend support.

Examples: interpolated render transform, UVs, pipeline handles, batch runs, render targets, physical audio output.

### Tooling/observation

Built only on explicit requests.

Examples: Agent snapshots, fingerprints, profiling reports, migration reports, GPU readback/capture.

Tooling may allocate bounded copied data. Ordinary frame paths must not automatically build it.

## Modules

### `engine/core`

Low-level platform-independent facilities.

Current/planned responsibilities include:

- engine version/build identity,
- shared result/error/diagnostic types,
- deterministic utilities,
- timing primitives,
- compact IDs/handles as introduced by concrete contracts,
- low-level profiling primitives only when #91 requires them.

Core must not depend on SDL, rendering, MCP, editor/tool providers or separately licensed animation runtimes.

### `engine/platform`

SDL3-backed platform boundary.

Current responsibilities:

- RAII SDL subsystem ownership,
- explicit headless/windowed startup,
- window creation/destruction,
- quit/input translation into engine-owned events,
- SDL-free numeric window identity for renderer handoff.

Rules:

- public headers expose Trace2D types,
- headless startup creates no video window,
- higher layers consume engine events rather than raw `SDL_Event`,
- `Platform` outlives renderer resources created from its window identity.

### `engine/runtime`

Owns deterministic simulation time independently of SDL/rendering/MCP/CLI.

Current responsibilities:

- fixed timestep,
- explicit frame counter,
- `Step(count)` without sleeping,
- deterministic seed/reset,
- simulation time derived from fixed stepping,
- separate wall-clock accumulator for interactive callers.

Future presentation interpolation consumes runtime accumulator state but does not mutate authoritative simulation state. Exact-frame Agent/test stepping remains independent of wall-clock alpha.

### `engine/scene` / production world model

Current alpha Scene owns generation-safe runtime entity IDs, stable authored semantic IDs, names/tags, `Transform2D`, deterministic iteration and versioned TOML serialization.

Future #71 evolves this into the common world model.

#### Production rules

- deterministic parent/child hierarchy,
- explicit local/world transforms,
- cycle rejection and deterministic reparenting,
- finite typed engine components,
- external registered game-defined typed components,
- stable component type IDs/schema versions,
- authored vs runtime-only game component classification,
- explicit component adapters for parse/validate/serialize/inspect/property binding,
- strongly typed C++ game access,
- no generic reflection/property bag requirement,
- no C++ RTTI/allocation address as authored identity,
- one instance/type/entity baseline until a concrete multiplicity requirement exists.

A component may opt into Agent observation without introducing JSON/MCP types into Scene/game code.

Detailed contract: [PRODUCTION_ARCHITECTURE_CONTRACTS.md](PRODUCTION_ARCHITECTURE_CONTRACTS.md).

### `engine/input`

Owns gameplay-facing input state independently of SDL events/physical devices.

Current responsibilities:

- stable engine controls,
- held/pressed/released state,
- deterministic frame-indexed scheduling,
- virtual input,
- predictable reset.

Future #72 adds semantic actions/axes, mouse/gamepad and text/IME while keeping physical and virtual input on the same gameplay-facing path.

Detailed baseline: [INPUT.md](INPUT.md).

### `engine/assets` and future common resource layer

Current assets own project-relative CPU-side imported asset identity/cache state, not GPU resources.

Existing texture rules:

- canonical project-relative identity,
- rejection of absolute/traversal references,
- immutable decoded RGBA8 CPU data,
- successful-import caching,
- explicit invalidation/clear,
- no per-frame file discovery/decoding,
- no SDL/GPU ownership.

Future #86 unifies resource semantics across Sprite/Tile/Font/Audio/Material and other assets.

Frozen future rules:

- typed authored references,
- setup-time canonicalization,
- generation-safe resolved runtime handles,
- CPU canonical asset truth separate from GPU/backend resources,
- successful immutable resource reuse,
- explicit dependency/unload policy,
- no tracing GC,
- no mandatory atomic shared ownership in hot paths,
- known CPU and GPU resource-memory evidence reported separately,
- hot reload not implied by caching.

### Reusable scene templates/world instances

Future #87 introduces one common text-authored reusable hierarchy model rather than subsystem-specific factories.

Rules:

- stable template-local entity identity,
- explicit stable runtime instance identity,
- deterministic instantiate/despawn structural-change phase,
- typed component overrides only through registered schemas,
- explicit scene/world load/unload,
- deterministic additive-world order,
- no hidden mandatory global entity pool.

### `engine/ui`

Owns deterministic authored UI state independently of SDL/rendering/protocol adapters.

Current responsibilities:

- versioned TOML loading,
- stable semantic IDs/names,
- authored-order storage,
- integer bounds,
- panel/label/button/text-input primitives,
- focus/activation/text state,
- deterministic CPU raster output.

Future #75 adds practical hierarchy/layout/widgets while keeping `UiDocument` semantic state authoritative.

Detailed baseline: [UI.md](UI.md).

### `engine/render`

Owns presentation and visual QA through SDL3 GPU without becoming authoritative gameplay state.

Current responsibilities:

- GPU device/window claim,
- command submission,
- swapchain/offscreen targets,
- orthographic camera baseline,
- textured sprite submission,
- culling,
- order-preserving contiguous batching,
- persistent/capacity-reused resources,
- exact-frame capture,
- renderer metrics.

Current hard rules remain:

- public types hide SDL GPU handles,
- headless semantic automation requires no GPU,
- normal frames perform no capture readback/file I/O,
- painter order cannot be globally reordered for batching.

Detailed current contract: [RENDERING.md](RENDERING.md).

#### Future Sprite integration seams

#59 must preserve:

- `SpriteRenderer2D`/`SpriteAnimator2D` as finite semantic state compatible with #71,
- previous/current fixed transform history for interactive interpolation,
- authoritative exact-frame capture mode independent of wall-clock remainder,
- backend-independent resolved view input for #88,
- canonical CPU Sprite asset identity separate from GPU handles for #86,
- resolved material/pipeline compatibility identity for #89.

Detailed Sprite contract: [SPRITES.md](SPRITES.md).

### Camera2D / Viewport2D

Future #88 turns baseline orthographic math into practical world/project presentation semantics.

`Camera2D` is world component state. `Viewport2D` separates logical rendering from OS-window dimensions.

Frozen semantics include:

- deterministic active-camera selection,
- positive orthographic vertical-size convention,
- explicit viewport logical size/scaling mode,
- backend-independent world-to-screen/screen-to-world conversion,
- fixed-step camera smoothing when engine-owned,
- deterministic presentation-only shake when Trace2D owns it,
- capture metadata includes simulation frame + viewport/camera + interpolation mode.

Renderer consumes resolved camera/view state; it does not own gameplay camera truth.

### Material2D / Shader2D

Future #89 provides a small programmable 2D surface.

Frozen direction:

- project-relative `Shader2D` resource,
- existing Trace2D/SDL shader toolchain,
- standard Sprite vertex ABI,
- custom fragment stage first,
- finite typed material parameters,
- setup-time name-to-layout resolution,
- cached pipelines/shaders/samplers,
- bounded per-instance overrides,
- material-aware contiguous batching,
- no material graph/render graph,
- unsupported backend capability fails explicitly.

Generic vertex deformation belongs to later Mesh2D or separate approval.

### Deterministic tween/property animation

Future #90 provides fixed-step property animation without generic reflection.

Rules:

- integer fixed-step timeline authority,
- finite supported interpolable value types,
- explicit easing formulas,
- target selectors/properties resolved at setup to typed binding IDs,
- engine/game components opt into writable bindings explicitly,
- deterministic conflict/cancel/completion semantics,
- stale target generation causes cancellation/error rather than memory access,
- no per-frame property-string lookup.

### `engine/agent`

Protocol-independent automation facade over authoritative engine state.

Current responsibilities:

- runtime/scene/entity inspection,
- deterministic semantic queries,
- UI inspection/query/actions/assertions,
- stable Trace2D-owned snapshots/errors,
- explicit-request observation allocation.

Future subsystem support follows the same pattern:

- world/user components,
- resources,
- templates/world instances,
- camera/viewport,
- material/tween,
- profiler output,
- GPU conformance report summaries.

Authoritative modules never depend back on Agent.

### `engine/mcp`

Owns the MCP transport adapter over protocol-independent Agent/Testing vocabulary.

Hard rules:

- MCP is transport, not engine API,
- JSON/MCP types remain private to adapter/tests,
- normal frames perform no MCP serialization,
- the stdio host needs no renderer for semantic operations,
- new subsystem automation first lands in Agent/testing contracts.

Detailed contract: [MCP.md](MCP.md).

### `tools/trace2d`

CLI entry point for humans, scripts, CI and agents.

Target vocabulary remains small and composable:

```text
build
run
inspect
query
input/action
step
assert
capture
test
analyze
profile
migrate
```

Provider-specific generation SDKs must not become core/runtime dependencies merely because tooling can orchestrate them.

### Unified profiler/diagnostics

Future #91 unifies existing subsystem metrics.

Metric categories remain distinct:

1. deterministic structural metrics,
2. CPU machine timings with environment metadata,
3. GPU device/backend timings when supported,
4. known resource-memory evidence.

Profiler scope names resolve to compact IDs outside hot loops. Report/JSON building occurs only on request. History is bounded/capacity-reused. Global allocator interception is not a baseline requirement.

### GPU conformance

Future #92 defines three validation tiers:

- Tier A: always-on backend-independent/headless validation,
- Tier B: maintained real-GPU baseline before stable production rendering claims,
- Tier C: explicit vendor/backend release matrix only for claims covered.

Cross-vendor exact floating-point pixels are not assumed. Readback/fence/image comparison remains explicit test work, not normal rendering.

## Future Mesh2D boundary (#60)

After the external-game proof, arbitrary textured indexed geometry belongs to a separate generic presentation path:

```text
TexturedMesh2D
  positions
  UVs
  indices
  vertex color
  texture/material
  blend mode
  stable painter order
        |
        v
persistent/capacity-reused dynamic renderer resources
```

Mesh2D prevents SpriteRenderer from becoming a generic deformable renderer and gives later integrations such as Spine an appropriate geometry path.

## Spine compatibility boundary (#61)

Spine is a desired optional compatibility target but separately licensed.

Before SP0 approval:

- no Spine runtime vendoring/copy,
- no package/submodule/download dependency,
- no Spine-derived implementation code,
- no Spine-containing prebuilt binary,
- no shipped-support claim.

After explicit license approval, a thin official-runtime adapter may supply semantics while Trace2D provides integration, Mesh2D presentation and Agent observation.

Do not reimplement the proprietary Spine runtime/format to bypass the gate.

## Runtime execution model

The long-term deterministic workflow is:

```text
Load project/resources/world/UI
    |
Register external typed game components
    |
Reset seed/simulation/input/authoritative subsystem state
    |
Instantiate/load reusable world state
    |
Schedule/inject virtual semantic input
    |
Advance fixed simulation frame(s)
    |
Inspect/query authoritative state
    |
Assert gameplay/UI/animation/physics/audio/tween semantics
    |
Profile/analyze explicitly when requested
    |
Optionally render using interpolated presentation state
    |
Capture explicit authoritative frame/sub-frame request
```

Wall-clock interactive presentation never replaces explicit deterministic stepping.

## Authored versus generated/derived data

Authored project data stays text-first and version controlled where practical.

Generated/derived data may include:

- imported/compiled caches,
- deterministic atlases,
- generated raw sprite candidates,
- normalized/QA-derived sprite artifacts,
- shader artifacts,
- packaged content,
- profiler/benchmark output,
- screenshots/GPU conformance artifacts.

Generated image output is not automatically canonical authored state. Disposable caches must remain rebuildable from committed authoritative inputs/configuration.

## Performance policy

Priorities:

1. predictable ownership/lifetime,
2. setup-time resolution of authored strings/paths where practical,
3. no unnecessary per-frame allocation in measured hot paths,
4. persistent/capacity-reused resources,
5. cache-friendly layouts when profiling demonstrates value,
6. order-preserving batching and bounded submission overhead,
7. explicit threading/job architecture only when workloads justify it.

Explicit Agent/QA/import/migration/profile/capture operations may allocate structured results when requested; normal simulation/render paths must not pay that observability cost automatically.

Every significant optimization retains reproducible evidence where practical. Deterministic structural metrics and machine-specific timing remain separate.

## Recorded later breadth (#93)

2D lighting/shadows, navigation/pathfinding, broader platforms, networking and safe hot reload are recognized gaps but not current core steps.

Promotion requires an explicit owner decision and a dedicated contract. They must build on existing foundations instead of silently expanding current scope.