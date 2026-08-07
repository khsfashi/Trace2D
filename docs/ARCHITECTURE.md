# Architecture

## Goal

Trace2D is a C++20 2D engine designed around deterministic simulation, structured observability, headless execution, and automation by coding agents.

The architecture intentionally separates the engine's automation contract from any specific LLM protocol.

## Dependency direction

```text
tools / adapters
      |
      v
 agent facade          (planned)
      |
      v
   runtime             (planned)
   /  |   \
scene input render     (planned)
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

- stable identifiers and handles
- result/error types
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

### `engine/runtime` (planned)

Owns the simulation lifecycle.

Responsibilities:

- fixed-step update loop
- explicit frame stepping
- deterministic seed/state ownership
- active scene lifecycle
- headless/windowed parity

### `engine/scene` (planned)

Owns entities, components, authored scene data, and serialization.

Key requirements:

- stable entity identity
- deterministic iteration where observable behavior depends on order
- text-first source format
- machine-readable component inspection

### `engine/render` (planned)

2D rendering through SDL3 GPU.

The first renderer should remain intentionally small:

- camera
- textured sprites
- batching
- visibility/culling
- offscreen capture

Renderer performance will be benchmarked before more advanced systems are added.

### `engine/agent` (planned)

Protocol-independent automation facade over runtime capabilities.

Target operations:

```text
inspect
query
input
step
assert
capture
```

This layer is the source of truth for CLI, JSON-RPC, and eventual MCP integration.

### `tools/trace2d`

The command-line entry point for humans, scripts, CI, and coding agents.

Current commands:

```text
trace2d version
trace2d doctor [--json]
trace2d run (--headless|--windowed) [--json]
```

`run` is currently a platform/startup smoke surface. Issue #3 will extend the same path with deterministic runtime control instead of creating a separate headless engine.

The CLI should keep stable exit codes and machine-readable output for automation-sensitive commands.

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
