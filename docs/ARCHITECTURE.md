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
   runtime
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

Planned extensions when scene/input systems arrive:

- active scene lifecycle
- per-frame gameplay update dispatch
- windowed loop integration that consumes monotonic elapsed time

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
trace2d run (--headless|--windowed) [--frames N] [--seed N] [--json]
```

`run` remains a small startup/runtime smoke surface. `--frames` advances the same runtime API used by tests, so headless automation can prove exact frame control without sleeping.

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
