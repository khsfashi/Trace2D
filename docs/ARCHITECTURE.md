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
```

Dependencies point inward. Core must not depend on SDL, rendering, MCP, or an editor.

Platform-specific SDL3 ownership lives in `engine/platform`; SDL types must not leak into `engine/core` or gameplay-facing APIs.

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
- basic engine-owned event translation

Initial API rules:

- public platform headers expose Trace2D types, not SDL types
- headless startup initializes only the event subsystem and creates no window
- windowed startup initializes SDL video and owns the window lifetime
- higher layers consume `PlatformEvent` rather than `SDL_Event`

Planned extensions when their phases require them:

- keyboard, mouse, and controller event translation
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
- runtime code has no SDL, CLI, JSON, or MCP dependency

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

Planned extensions in P3:

- semantic selectors and queries

### `engine/input` (planned)

Will own engine-level input state independently of SDL event objects, including virtual frame-scheduled input for deterministic tests.

### `engine/render` (planned)

2D rendering through SDL3 GPU.

The first renderer should remain intentionally small:

- camera
- textured sprites
- batching
- visibility/culling
- offscreen capture

Renderer performance will be benchmarked before more advanced systems are added.

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

Current API rules:

- `engine/agent` may depend on runtime and scene; runtime and scene never depend on the agent facade
- public inspection types contain no JSON, MCP, SDL, or LLM-specific protocol objects
- JSON serialization belongs to the CLI/tool boundary, not this module
- inspection snapshots own copied observation data and allocate only when inspection is explicitly requested
- no inspection copying or JSON generation occurs in the per-frame simulation path
- bounds stay `null` until renderer/physics state can provide authoritative values; the facade does not guess bounds from coordinates or transform scale

The detailed contract is documented in [INSPECTION.md](INSPECTION.md).

Target operations as later phases arrive:

```text
inspect   (implemented)
query     (P3)
input     (P4)
step      (runtime primitive exists; facade operation later)
assert    (P4)
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
```

`run` remains a small startup/runtime smoke surface. `--frames` advances the same runtime API used by tests, so headless automation can prove exact frame control without sleeping.

`inspect` loads authored scene text at the tool boundary, advances a deterministic runtime by the requested number of frames, invokes `AgentFacade::Inspect()`, and serializes the resulting Trace2D-owned snapshot. The engine inspection API itself is not JSON-aware.

The CLI keeps stable non-zero exit categories and machine-readable error objects for automation-sensitive inspection failures. See [INSPECTION.md](INSPECTION.md) for the current contract.

## Runtime execution model

The intended deterministic test execution model is:

```text
Load project
    |
Reset seed and simulation state
    |
Apply virtual input
    |
Advance N fixed frames
    |
Inspect/query structured state
    |
Assert behavior
    |
Optionally capture rendered output
```

Wall-clock-driven rendering may interpolate between simulation states, but automated gameplay tests must be able to control simulation advancement explicitly.

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
