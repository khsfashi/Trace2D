# Architecture

## Goal

Trace2D is a C++20 2D engine designed around deterministic simulation, structured observability, headless execution, and automation by coding agents.

The architecture intentionally separates the engine's automation contract from any specific LLM protocol and separates authoritative game/animation state from renderer presentation state.

## Dependency direction

Conceptually:

```text
tools / protocol adapters
           |
           v
      agent facade
      /    |    \
     v     v     v
 runtime  scene   ui
    |      |      |
  input    |      |
      \    |     /
          core

assets  -----> authored/runtime consumers
render  -----> platform
mcp    ------> agent/testing adapter boundaries
```

Dependencies point inward. `engine/agent` may observe authoritative runtime/scene/UI and later authoritative animation state, but those modules never depend back on the agent facade. Core must not depend on SDL, rendering, MCP, an editor, image-generation providers, or separately licensed animation runtimes.

Platform-specific SDL3 ownership lives behind `engine/platform` and `engine/render`; SDL types must not leak into `engine/core`, simulation modules, assets, UI state, or gameplay-facing APIs. `engine/render` may depend on the platform boundary to identify the window, but authoritative simulation/runtime/UI/animation modules do not depend on rendering.

## Modules

### `engine/core`

Low-level engine facilities with no platform dependency.

Initial/current responsibilities include engine version/build identity and shared low-level utilities as they are introduced.

Planned responsibilities may include:

- shared result/error types,
- timing primitives,
- diagnostics,
- deterministic utilities,
- low-level profiling hooks.

### `engine/platform`

SDL3-backed platform boundary.

Current responsibilities:

- RAII ownership of initialized SDL subsystems,
- explicit headless versus windowed startup,
- window creation/destruction for interactive startup,
- engine-owned quit-event translation,
- keyboard and mouse translation into engine-owned input events,
- SDL-free numeric window identity for renderer handoff.

Current API rules:

- public platform headers expose Trace2D types, not SDL types,
- headless startup initializes only required non-video subsystems and creates no window,
- windowed startup owns the window lifetime,
- higher layers consume Trace2D events rather than raw `SDL_Event`,
- renderer integration receives `WindowId`; public APIs do not expose `SDL_Window*`,
- `Platform` must outlive a `Renderer` created from its window identity.

### `engine/runtime`

Owns deterministic simulation-time control independently of SDL, rendering, MCP, and the CLI.

Current responsibilities:

- fixed simulation timestep configuration,
- explicit simulation frame counter,
- `Step(count)` advancement without sleeping or reading wall-clock time,
- deterministic seed ownership/reset point,
- simulation-time reporting derived from fixed-step advancement,
- separate monotonic wall-clock accumulation for interactive callers.

Current API rules:

- tests/agents advance simulation with explicit fixed-frame stepping,
- wall-clock time never changes explicit-step semantics,
- reset clears frame/simulation/accumulated wall time while installing the requested seed,
- runtime public code contains no SDL/renderer/CLI/JSON/MCP dependency.

### `engine/scene`

Owns the minimum entity and authored-state model needed for deterministic inspection and text-first scene authoring.

Current responsibilities include generation-safe runtime `EntityId`, stable authored semantic IDs, names/tags, mutable 2D transforms, deterministic observable iteration, versioned TOML scene loading/serialization, and structured diagnostics.

Key rules:

- automation never uses a raw pointer as identity,
- observable runtime iteration is deterministic,
- authored serialization is canonical and stable for Git diffs,
- no generic ECS is introduced before measured requirements justify one,
- `toml++` remains private to the authored-data boundary.

Full version-1 scene schema: [SCENE_FORMAT.md](SCENE_FORMAT.md).

### `engine/input`

Owns gameplay-facing input state independently of SDL event objects and physical devices.

Current responsibilities:

- stable engine-level control identifiers,
- press/release/held state,
- one-frame `pressed` / `released` transitions,
- deterministic frame-indexed scheduling,
- virtual input for tests/agents,
- predictable scenario reset.

Per-frame advancement performs no heap allocation after the schedule is authored. Detailed contract: [INPUT.md](INPUT.md).

### `engine/assets`

Owns deterministic CPU-side imported asset identity/cache state, not GPU resources.

Current texture-asset rules include:

- project-relative canonical reference identity,
- explicit rejection of absolute/traversal references,
- immutable decoded RGBA8 CPU texture data,
- successful-import caching/reuse,
- explicit invalidation/clear,
- no per-frame file discovery/decoding,
- no SDL/GPU ownership.

Detailed contract: [ASSETS.md](ASSETS.md).

Future Sprite source/import metadata will build on this separation. External authoring/generation formats are inputs to canonical Trace2D assets, not runtime APIs.

### `engine/ui`

Owns deterministic authored UI state independently of SDL, rendering, and protocol adapters.

Current responsibilities:

- strict versioned TOML UI loading,
- stable IDs and semantic names,
- deterministic authored-order storage,
- integer pixel bounds,
- panel/label/button/text-input primitives,
- visible/enabled/focused state,
- activation count and text mutation,
- deterministic dependency-free CPU text/raster output.

`UiDocument` is authoritative. Renderer resources never become UI truth. Detailed contract: [UI.md](UI.md).

### `engine/render` — Public Alpha baseline complete

Owns presentation and visual-QA state through SDL3 GPU without becoming authoritative gameplay/UI/animation state.

Current responsibilities:

- renderer-owned SDL3 GPU device lifetime,
- platform window claim/release,
- command submission,
- swapchain and offscreen targets,
- orthographic camera and baseline textured sprites,
- inclusive visibility/culling,
- caller/painter-order-preserving submission,
- measured contiguous same-texture instancing,
- persistent/capacity-reused renderer resources,
- exact-simulation-frame capture,
- renderer metrics.

Current hard rules:

- public headers expose Trace2D types, not SDL GPU handles,
- renderer state is presentation/QA state,
- headless semantic automation does not require a renderer/GPU,
- persistent resources are reused rather than recreated every steady frame,
- normal non-capture frames perform no capture download/map/file-I/O,
- texture/material convenience never authorizes a global painter-order reorder.

Renderer performance expansion is gated on reproducible workloads in #41. Current contract: [RENDERING.md](RENDERING.md).

The future production-complete Sprite Renderer is specified separately in [SPRITES.md](SPRITES.md) and must not be reduced to a minimal quad renderer merely because the Public Alpha baseline already draws sprites.

### `engine/agent`

Protocol-independent automation facade over authoritative engine state.

Current responsibilities:

- runtime/scene/entity inspection,
- deterministic semantic entity queries,
- semantic UI inspection/query/actions/assertions,
- stable Trace2D-owned snapshot/error types,
- explicit-request observation allocation only.

Current API rules:

- authoritative modules never depend back on Agent,
- public Agent types contain no JSON/MCP/SDL/LLM-protocol objects,
- serialization belongs to adapters,
- semantic identity/selectors are preferred over screen coordinates,
- bounds/state are reported from authoritative owners rather than guessed.

Entity contracts: [INSPECTION.md](INSPECTION.md), [QUERY.md](QUERY.md). UI contract: [UI.md](UI.md).

Future Sprite animation Agent support must follow the same pattern: `SpriteAnimator2D` remains authoritative runtime state; Agent copies/observes it only when explicitly requested.

### `engine/mcp`

Owns the current MCP transport adapter over existing protocol-independent Agent/Testing vocabulary.

Current responsibilities from PR #58 include:

- modern MCP `2026-07-28` discovery/tool metadata,
- newline-delimited UTF-8 stdio host behavior,
- JSON-RPC parsing/serialization,
- fixed semantic tools for scene/runtime/UI/input/step/assert flows,
- structured transport errors.

Hard rules:

- MCP is transport, not the engine API,
- JSON/MCP types remain private to adapter/tests,
- ordinary runtime/UI/render frames perform no MCP JSON work,
- the stdio host initializes no renderer/GPU/window,
- new subsystem automation first lands in protocol-independent Agent/testing state, then adapters expose it.

Detailed contract: [MCP.md](MCP.md).

### `tools/trace2d`

Command-line entry point for humans, scripts, CI, and coding agents.

The CLI keeps serialization/tool orchestration at the boundary. Existing commands compose runtime/scene/query/public-alpha workflows; later asset import/generation/validation commands may be added when the active Sprite contract requires them.

A provider-specific image model SDK must not become a core/runtime dependency merely because `trace2d sprite generate` later orchestrates external generation.

## Future owner-approved Sprite architecture (#59)

Detailed contract: [SPRITES.md](SPRITES.md).

The intended boundary is:

```text
source/generator/external manifest
        |
        v
offline import / normalize / QA
        |
        v
canonical Trace2D SpriteAsset
        |
        +----> authoritative SpriteAnimator2D
        |              |
        |              +----> Agent inspect/assert
        |
        v
derived SpriteRenderData
        |
        v
production Sprite Renderer
        |
        v
GPU / capture QA
```

Key rules:

- source geometry prefers exact integer pixel metadata,
- normalized UVs/GPU handles are derived presentation state,
- trim/atlas packing cannot alter authored source-space pivot/placement semantics,
- generated pixels are not canonical until explicit import/validation,
- expensive repair/generation/QA remains offline explicit work,
- deterministic Sprite animation is renderer-independent,
- ordinary runtime frames do not build QA reports/snapshots unless requested,
- batching may merge compatible contiguous work but cannot globally reorder semantic painter order.

The Sprite Renderer target is production-complete traditional sprite presentation: transforms/pivot/flip, atlas/trim/rotated packing, tint/opacity, alpha/blend/sampling, sorting groups/masking, 9-slice, tiled sprites, runtime pixel-perfect presentation, measured batching/resource lifetime, and conformance workloads.

It intentionally excludes arbitrary deformable textured geometry and skeletal runtimes.

## Future Mesh2D boundary (#60)

After #59, arbitrary textured indexed geometry belongs to a separate generic presentation path:

```text
TexturedMesh2D
  positions
  UVs
  indices
  vertex color
  texture
  blend mode
  stable painter order
        |
        v
persistent/capacity-reused dynamic renderer resources
```

Mesh2D exists so later systems such as Spine do not force the quad/9-slice SpriteRenderer into a generic renderer. It remains presentation state and does not define skeletal animation semantics.

## Spine compatibility boundary (#61)

Detailed status: [SPINE.md](SPINE.md).

Spine is a desired optional compatibility target but is separately licensed. The owner-approved architecture therefore has a hard SP0 human gate.

Before SP0 approval:

- no Spine Runtime vendoring/copy,
- no package/submodule/download dependency,
- no Spine-derived implementation code,
- no Spine-containing prebuilt binary,
- no shipped-support claim.

Only after explicit license/integration approval may a thin official-runtime adapter be added. Spine-specific runtime semantics remain owned by the official runtime; Trace2D supplies integration, generic Mesh2D presentation, and protocol-independent semantic observation/action boundaries.

Do not reimplement the proprietary Spine runtime/format merely to bypass the gate.

## Runtime execution model

The deterministic test execution model remains:

```text
Load authored project/scene/UI/assets
    |
Reset seed, simulation, input and authoritative subsystem state
    |
Schedule/inject virtual input
    |
Advance explicit simulation frame(s)
    |
Inspect/query authoritative state
    |
Perform semantic actions
    |
Assert behavior/state
    |
Optionally render/capture explicitly selected state
```

Future `SpriteAnimator2D` integrates into this same fixed-step/headless model. Wall-clock-driven presentation must not replace explicit test advancement.

## Authored versus generated data

Authored project data should be text-first and version controlled where practical.

Generated/derived data may include:

- imported/compiled asset caches,
- deterministic atlases,
- generated raw sprite candidates,
- normalized/QA-derived sprite artifacts,
- shader caches,
- packaged content,
- benchmark output,
- screenshots/test artifacts.

A generated image model output is not automatically canonical authored state. The Sprite contract requires explicit import/validation. Disposable caches must remain rebuildable from the committed authoritative inputs/configuration appropriate to their workflow.

## Performance policy

Performance is a design constraint, but specialized complexity is introduced only after measurement.

Priorities:

1. predictable ownership/lifetime,
2. no unnecessary per-frame allocation in measured hot paths,
3. resource/object reuse for steady-state work,
4. cache-friendly data layouts where profiling demonstrates value,
5. order-preserving batching and bounded submission overhead,
6. explicit worker/threading architecture only when workloads justify it.

Explicit tooling/Agent/QA operations may allocate structured results when requested; ordinary simulation/render paths must not pay that observability cost automatically.

Every significant optimization should retain reproducible benchmark/profiler evidence where practical. Structural metrics and machine-specific timing should remain distinguishable.