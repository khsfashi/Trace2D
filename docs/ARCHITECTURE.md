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
      |
      v
   runtime
   /  |   \
scene input render
   \   |   /
      core

render -> platform -> input
```

Dependencies point inward. Core must not depend on SDL, rendering, MCP, or an editor.

Platform-specific SDL3 ownership lives behind `engine/platform` and `engine/render`; SDL types must not leak into `engine/core`, simulation modules, or gameplay-facing APIs. `engine/render` may depend on the platform boundary to identify the window, but simulation/runtime modules do not depend on rendering.

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

### `engine/render` — P5 in progress

Owns presentation and visual-QA state through SDL3 GPU without becoming authoritative gameplay state.

Current P5 foundation responsibilities:

- renderer-owned SDL3 GPU device lifetime
- claim/release of the platform-owned window for GPU presentation
- command-buffer acquisition and submission
- swapchain texture acquisition
- minimal clear/store render pass and presentation
- basic renderer metrics and backend-name observation
- explicit rejection of headless platforms before GPU initialization

Current API rules:

- public renderer headers expose Trace2D-owned types, not SDL GPU handles
- the renderer receives a `Platform` and resolves its numeric `WindowId` internally
- SDL3 GPU device, swapchain, command-buffer, render-pass, and texture handles remain private to the renderer implementation
- the renderer owns presentation resources; it does not own runtime, scene, input, or gameplay truth
- headless gameplay execution does not construct a renderer or require a GPU device
- persistent GPU resources should have renderer-owned lifetimes rather than being recreated every frame
- renderer metrics are observational profiling state, not simulation state

Remaining P5 responsibilities:

- orthographic camera
- minimal sprite render-data contract
- textured sprite rendering
- measured batching baseline
- visibility/culling baseline
- offscreen render target
- deterministic frame-selected visual capture

Renderer performance will be measured before more advanced systems are added. The detailed P5 contract is documented in [RENDERING.md](RENDERING.md).

### `engine/agent`

Protocol-independent automation facade over authoritative runtime and scene state.

Current P3 responsibilities:

- non-owning binding to the active deterministic runtime and scene
- stable Trace2D-owned inspection snapshot and error types
- runtime frame, seed, fixed-step, and simulation-time inspection
- scene semantic identity and deterministic entity inspection
- generation-safe runtime handle, semantic ID, name, tags, and transform inspection
- explicit nullable bounds in the inspection schema
- generic typed component-field snapshots, initially exposing `Transform2D`
- deterministic snapshot ordering suitable for adapters
- exact semantic selectors and deterministic single/multi-result queries

Current API rules:

- `engine/agent` may depend on runtime and scene; runtime and scene never depend on the agent facade
- public inspection/query types contain no JSON, MCP, SDL, or LLM-specific protocol objects
- JSON serialization belongs to the CLI/tool boundary, not this module
- inspection/query snapshots own copied observation data and allocate only when explicitly requested
- no inspection copying or JSON generation occurs in the per-frame simulation path
- bounds stay `null` until renderer/physics state can provide authoritative values; the facade does not guess bounds from coordinates or transform scale

The detailed contracts are documented in [INSPECTION.md](INSPECTION.md) and [QUERY.md](QUERY.md).

Target operations as later phases arrive:

```text
inspect   (implemented)
query     (implemented)
input     (P4 engine primitive implemented; facade operation later)
step      (runtime primitive exists; facade operation later)
assert    (P4 engine primitive implemented)
capture   (P5)
```

This layer is the source of truth for CLI, JSON-RPC, and eventual MCP integration.

### `tools/trace2d`

The command-line entry point for humans, scripts, CI, and coding agents.

Current commands:

```text
trace2d version
trace2d doctor [--json]
trace2d run (--headless|--windowed) [--frames N] [--seed N] [--json]
trace2d inspect --scene PATH [--frames N] [--seed N] [--json]
trace2d query --scene PATH --selector SELECTOR [--one] [--frames N] [--seed N] [--json]
```

`run` remains a small startup/runtime smoke surface. `--frames` advances the same runtime API used by tests, so headless automation can prove exact frame control without sleeping. During P5, windowed `run` additionally creates the renderer and submits the current minimal clear frame; headless `run` does not initialize the GPU renderer.

`inspect` and `query` keep serialization at the tool boundary. The engine inspection/query APIs themselves are not JSON-aware.

## Runtime execution model

The deterministic test execution model is:

```text
Load project
    |
Reset seed, simulation, and input state
    |
Schedule virtual input
    |
Advance input to frame N
    |
Advance one fixed simulation frame
    |
Inspect/query structured state
    |
Assert behavior
    |
Optionally render/capture that explicitly selected state
```

Wall-clock-driven rendering may interpolate between simulation states, but automated gameplay tests must be able to control simulation advancement explicitly. Visual capture must identify its simulation frame explicitly rather than infer it from presentation timing.

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

Every significant optimization should keep a reproducible benchmark or profiler record where practical.
