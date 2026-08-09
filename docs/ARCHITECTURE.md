# Architecture

## Goal

Trace2D is a C++20 2D engine designed around deterministic simulation, structured observability, headless execution, and automation by coding agents.

The architecture intentionally separates the engine's automation contract from any specific LLM protocol.

## Dependency direction

```text
tools / adapters
      |
      v
 agent facade
   /   |   \
  v    v    v
runtime scene ui
  |      |   |
 input   |   |
   \     |  /
        core

render -> platform -> input
```

Dependencies point inward. `engine/agent` may observe authoritative runtime/scene/UI state, but those modules never depend back on the agent facade. Core must not depend on SDL, rendering, MCP, or an editor.

Platform-specific SDL3 ownership lives behind `engine/platform` and `engine/render`; SDL types must not leak into `engine/core`, simulation modules, UI state, or gameplay-facing APIs. `engine/render` may depend on the platform boundary to identify the window, but simulation/runtime/UI modules do not depend on rendering.

## Modules

### `engine/core`

Low-level engine facilities with no platform dependency.

Initial responsibilities:

- engine version and build identity

Planned responsibilities:

- shared result/error types
- timing primitives
- diagnostics
- deterministic utilities
- low-level profiling hooks

### `engine/platform`

SDL3-backed platform boundary.

Current responsibilities:

- RAII ownership of initialized SDL subsystems
- explicit headless versus windowed startup
- window creation/destruction for interactive startup
- engine-owned quit-event translation
- keyboard and mouse translation into engine-owned input events
- SDL-free numeric window identity for renderer handoff

Current API rules:

- public platform headers expose Trace2D types, not SDL types
- headless startup initializes only the event subsystem and creates no window
- windowed startup initializes SDL video and owns the window lifetime
- higher layers consume `PlatformEvent` rather than `SDL_Event`
- physical keyboard and mouse events are translated into `trace2d::input::InputEvent`
- renderer integration receives `WindowId`; public APIs do not expose `SDL_Window*`
- `Platform` must outlive a `Renderer` created from its window identity

Planned extensions when their phases require them:

- controller event translation
- additional filesystem/platform services where needed

### `engine/runtime`

Owns deterministic simulation-time control independently of SDL and the CLI.

Current responsibilities:

- fixed simulation timestep configuration
- explicit simulation frame counter
- `Step(count)` advancement without sleeping or reading wall-clock time
- deterministic seed ownership and reset point
- simulation-time reporting derived from fixed-step advancement
- sub-step wall-clock accumulation for interactive callers
- monotonic `steady_clock` wrapper for windowed/runtime integration

Current API rules:

- tests and coding agents advance simulation with explicit fixed-frame stepping
- wall-clock time enters through a separate accumulation path and never changes explicit-step behavior
- reset clears frame, simulation time, and accumulated wall time while installing the requested seed
- runtime code has no SDL, renderer, CLI, JSON, or MCP dependency

Planned extensions when later systems arrive:

- active scene lifecycle
- per-frame gameplay update dispatch
- windowed loop integration that consumes monotonic elapsed time

### `engine/scene`

Owns the minimum entity and authored-state model needed for deterministic inspection and text-first scene authoring.

Current responsibilities:

- generation-safe runtime `EntityId` handles
- scene-owned entity creation, destruction, lookup, and slot reuse
- stable non-empty semantic IDs for authored/automation-facing identity
- authored scene semantic ID and human-readable scene name
- human-readable entity names and normalized semantic tags
- mutable 2D transform state
- deterministic allocation-free observable runtime iteration
- versioned TOML scene loading and deterministic serialization
- structured field-path diagnostics with source line/column when available

Current identity rules:

- `EntityId` is a runtime handle composed of a 32-bit slot index and 32-bit generation
- destroying an entity invalidates its handle before that slot can be reused
- a replacement entity may reuse the slot index but receives the incremented generation
- non-empty semantic IDs are unique within a scene and remain distinct from runtime handles
- entities without semantic IDs are allowed for runtime-spawned objects
- authored serialization requires non-empty semantic IDs and rejects runtime-only entities
- automation must never use a raw pointer as entity identity

Current iteration and serialization policy:

- observable runtime entity iteration scans live slots in ascending slot-index order
- runtime iteration allocates no temporary result collection
- repeated execution with the same create/destroy sequence produces the same runtime iteration order
- authored serialization is intentionally independent from runtime slot order
- serialized entities are sorted lexicographically by semantic ID for stable Git diffs
- tags are normalized into sorted unique order
- canonical serialization uses fixed field ordering and locale-independent float formatting
- the implementation intentionally avoids a generic ECS until measured requirements justify one

Current authored text boundary:

- authored scene files use TOML with the `*.trace2d.toml` suffix
- `toml++` is private to the scene implementation and does not appear in public Trace2D headers
- unknown fields are rejected so misspelled authored data does not silently change behavior
- syntax/schema diagnostics identify semantic paths and source positions where available
- comments and original whitespace are not preserved by save; serialization produces canonical semantic state
- full version-1 schema is documented in [SCENE_FORMAT.md](SCENE_FORMAT.md)

### `engine/input`

Owns gameplay-facing input state independently of SDL event objects and physical devices.

Current responsibilities:

- stable engine-level keyboard and mouse control identifiers
- press, release, and held state
- one-frame `pressed` and `released` transitions
- deterministic frame-indexed input scheduling
- virtual input source for tests and coding agents
- predictable scenario reset

Current API rules:

- gameplay reads `InputSystem`; it never consumes `SDL_Event`
- physical platform events and virtual events use the same `InputEvent` type
- scheduled input is applied in deterministic frame order and insertion order for ties
- input frames never move backwards
- per-frame state advancement performs no heap allocation after the schedule has been authored
- scheduling is setup work and may allocate; reset retains vector capacity for reuse

The detailed contract is documented in [INPUT.md](INPUT.md).

### `engine/ui`

Owns deterministic authored UI state independently of SDL, rendering, and protocol adapters.

Current responsibilities:

- strict versioned TOML UI loading
- stable non-empty element IDs and stable semantic names
- deterministic authored-order element storage/iteration
- integer pixel bounds
- panel, label, button, and text-input primitives
- visible/enabled/focused state
- button activation count
- focused text-input mutation
- deterministic dependency-free CPU text/raster output

Current API rules:

- `UiDocument` is authoritative for UI identity/state; renderer resources never become UI truth
- semantic `name` is distinct from mutable text/value state
- invisible controls reject interaction and are skipped by CPU rasterization
- focus and activation use deterministic O(N) authored-order lookup and add no heap allocation
- explicit text replacement may resize the existing string and is not a per-frame hot path
- `engine/ui` contains no Agent, JSON, MCP, SDL, or renderer types
- headless tests operate on the same `UiDocument` used for presentation

The detailed contract is documented in [UI.md](UI.md).

### `engine/render` — Public Alpha baseline complete

Owns presentation and visual-QA state through SDL3 GPU without becoming authoritative gameplay state.

Current responsibilities:

- renderer-owned SDL3 GPU device lifetime
- claim/release of the platform-owned window for GPU presentation
- command-buffer acquisition and submission
- swapchain and renderer-owned offscreen targets
- orthographic camera and textured sprite rendering
- inclusive visibility/culling baseline
- caller/painter-order-preserving submission
- measured contiguous same-texture instancing
- persistent/capacity-reused renderer resources
- exact-simulation-frame capture to deterministic CPU-normalized BMP
- renderer metrics and backend-name observation
- explicit rejection of headless platforms before GPU initialization

Current API rules:

- public renderer headers expose Trace2D-owned types, not SDL GPU handles
- the renderer receives a `Platform` and resolves its numeric `WindowId` internally
- SDL3 GPU device, command-buffer, render-pass, texture, and swapchain handles remain private
- renderer state is presentation/visual-QA state, not runtime/scene/input/UI authority
- headless gameplay/UI automation does not construct a renderer or require a GPU device
- persistent GPU resources have renderer-owned lifetimes and are not recreated every steady frame
- normal non-capture frames perform no capture download/map/file-I/O work
- renderer metrics are observational profiling state, not simulation state

Renderer performance expansion is intentionally gated on reproducible workloads in Issue #41. The rendering contract is documented in [RENDERING.md](RENDERING.md).

### `engine/agent`

Protocol-independent automation facade over authoritative runtime, scene, and UI state.

Current responsibilities:

- non-owning binding to deterministic runtime, scene, and optional active `UiDocument`
- stable Trace2D-owned inspection snapshot and error types
- runtime frame, seed, fixed-step, and simulation-time inspection
- scene semantic identity and deterministic entity inspection
- generation-safe runtime handle, semantic ID, name, tags, transform, and typed component inspection
- exact entity semantic selectors and deterministic single/multi-result queries
- structured semantic UI tree snapshots
- exact UI selectors by stable ID, role, and name
- semantic UI focus, button activation, and focused text input
- structured UI state assertions
- deterministic authored-order UI query output

Current API rules:

- `engine/agent` may depend on runtime, scene, and UI; those modules never depend on the agent facade
- public inspection/query/action/assertion types contain no JSON, MCP, SDL, or LLM-specific protocol objects
- JSON serialization belongs to CLI/tool adapters, not this module
- inspection/query snapshots own copied observation data and allocate only when explicitly requested
- no inspection copying, UI semantic snapshotting, or JSON generation occurs in the per-frame simulation/raster path
- entity bounds stay nullable until renderer/physics state can provide authoritative values; the facade does not guess them
- UI bounds come directly from authoritative `UiDocument` integer bounds
- semantic identity/selectors are preferred over screen-coordinate targeting

The entity contracts are documented in [INSPECTION.md](INSPECTION.md) and [QUERY.md](QUERY.md). Semantic UI is documented in [UI.md](UI.md).

Current machine-facing vocabulary includes structured forms of:

```text
inspect
query
input
step
assert
capture
```

Issue #39 will add MCP only as a transport adapter over this already-existing vocabulary.

### `tools/trace2d`

The command-line entry point for humans, scripts, CI, and coding agents.

Current commands include:

```text
trace2d version
trace2d doctor [--json]
trace2d run (--headless|--windowed) [--frames N] [--seed N] [--json]
trace2d inspect --scene PATH [--frames N] [--seed N] [--json]
trace2d query --scene PATH --selector SELECTOR [--one] [--frames N] [--seed N] [--json]
```

`run` remains a small startup/runtime smoke surface. `--frames` advances the same runtime API used by tests, so headless automation can prove exact frame control without sleeping. Windowed run may create the renderer; headless run does not initialize GPU rendering.

`inspect` and `query` keep serialization at the tool boundary. Engine inspection/query APIs themselves are not JSON-aware. UI preview remains a separate adapter over the same engine-owned UI/raster state.

## Runtime execution model

The deterministic test execution model is:

```text
Load authored project/scene/UI
    |
Reset seed, simulation, and input state
    |
Schedule/inject virtual input
    |
Advance explicit simulation frame(s)
    |
Inspect/query authoritative scene/UI state
    |
Perform semantic input/UI actions
    |
Assert behavior/state
    |
Optionally render/capture explicitly selected state
```

Wall-clock-driven rendering may interpolate between simulation states, but automated tests own simulation advancement explicitly. Visual capture identifies its simulation frame explicitly rather than inferring it from presentation timing.

## Authored versus generated data

Authored project data should be text-first and version controlled.

Generated data may include:

- imported/compiled asset caches
- shader caches
- packaged content
- benchmark output
- screenshots and test artifacts

Generated caches must be disposable and rebuildable from authored source data.

## Performance policy

Performance is a design constraint, but specialized complexity is introduced only after measurement.

Priorities:

1. predictable ownership and lifetime
2. no unnecessary per-frame allocation in measured hot paths
3. cache-friendly data layouts where profiling demonstrates value
4. batched rendering and bounded submission overhead
5. explicit worker/threading architecture only when workloads justify it

Explicit tooling operations may allocate structured snapshots/results when requested; ordinary simulation/raster paths must not pay that observability cost automatically.

Every significant optimization should keep a reproducible benchmark or profiler record where practical.
