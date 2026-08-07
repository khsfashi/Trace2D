# Trace2D

**A deterministic and observable C++ 2D engine built for AI coding agents.**

Trace2D explores a simple question: why can coding agents build and test web applications end-to-end, while game development still depends heavily on visual guessing and manual editor interaction?

The project is designed so an agent can eventually edit a game, build it, run it headlessly, advance simulation time explicitly, inspect structured runtime state, inject input, assert behavior, and capture visuals when pixels actually matter.

> Current status: **P4 — deterministic interaction and gameplay-test foundation**. P0 project setup, P1 deterministic runtime control, P2 stable scene identity/text serialization, P3 structured inspection/semantic queries, and P4 deterministic virtual input are implemented. Gameplay assertions are the next executable milestone; rendering/capture remain later work.

## Project navigation

For coding agents or contributors continuing development, start here:

- [AGENTS.md](AGENTS.md) — repository operating rules and handoff protocol
- [PROJECT_STATUS.md](PROJECT_STATUS.md) — current phase, active work, validation state, and next execution order
- [Runtime inspection contract](docs/INSPECTION.md) — protocol-independent snapshot schema, deterministic CLI JSON, and exit codes
- [Semantic query contract](docs/QUERY.md) — selectors, deterministic result ordering, and query errors
- [Deterministic input contract](docs/INPUT.md) — physical/virtual input convergence, frame scheduling, and reset semantics
- [Scene text format](docs/SCENE_FORMAT.md) — version-1 authored TOML schema and canonical serialization rules
- [Public Release Plan](docs/PUBLIC_RELEASE.md) — exact gates for `v0.1.0-alpha.1`
- [Roadmap](docs/ROADMAP.md) — long-term P0-P8 development phases
- [Architecture](docs/ARCHITECTURE.md) — module/dependency direction
- [Agent-first design principles](docs/AGENT_FIRST_PRINCIPLES.md) — non-negotiable design intent

A future coding-agent session should be able to continue the project from these repository files without requiring previous chat history.

## Implemented foundation

The current repository already includes:

- reproducible C++20 / CMake / pinned-vcpkg project setup
- Windows MSVC CI with warnings treated as errors
- SDL3 hidden behind a Trace2D-owned platform boundary
- explicit headless and windowed startup modes
- deterministic fixed-step runtime control with explicit `Step(count)` advancement
- observable simulation frame, fixed timestep, simulation time, deterministic seed, and reset state
- generation-safe runtime `EntityId` handles with stale-handle invalidation
- stable authored semantic IDs, names, sorted unique tags, and `Transform2D`
- deterministic allocation-free runtime entity iteration
- TOML `*.trace2d.toml` authored scenes with strict validation and source diagnostics
- deterministic canonical scene serialization sorted by semantic entity ID
- round-trip tests proving stable semantic scene state
- protocol-independent `Trace2D::Agent` inspection facade over runtime and scene state
- stable runtime/scene/entity/component snapshot types with structured errors
- deterministic CLI `inspect` JSON output with stable non-zero error categories
- exact semantic selectors for authored ID, name, tag, and component type
- deterministic single-result and multi-result runtime queries with ambiguity/no-match handling
- deterministic CLI `query` JSON output
- engine-level `Trace2D::Input` state independent of SDL event objects
- deterministic press/release/held transitions and frame-indexed virtual input scheduling
- SDL3 keyboard/mouse translation into the same engine-owned input event path
- resettable virtual input source and exact runtime-lockstep input tests

## Design goals

- deterministic, fixed-step gameplay testing
- headless execution with runtime parity
- structured entity/component inspection
- stable semantic selectors instead of fragile screen coordinates
- text-first authored project and scene data
- virtual input and explicit frame stepping
- machine-readable diagnostics and failure reports
- a small protocol-independent automation API
- MCP as an adapter, not as the engine architecture
- measured C++ performance with explicit ownership and predictable lifetimes

## Intended agent workflow

```text
Agent edits source / scene
        |
        v
      Build
        |
        v
   Headless run
        |
        v
Structured inspect
        |
        v
Virtual input + step
        |
        v
Gameplay assertions
        |
        v
Visual capture
        |
        +---- failure context ----> Agent
```

The full workflow above is the Public Alpha target. The README status and `PROJECT_STATUS.md` distinguish implemented stages from planned ones.

## Technology direction

- **Language:** C++20
- **Build:** CMake + CMake Presets
- **Dependencies:** vcpkg manifest mode with a pinned baseline
- **Platform layer:** SDL3
- **Scene text:** TOML via toml++ behind the scene implementation boundary
- **2D rendering:** SDL3 GPU (planned P5)
- **Physics:** Box2D or a smaller measured collision slice (decision deferred to P6)
- **Tests:** GoogleTest / CTest
- **CI:** GitHub Actions / MSVC

Dependencies are added only when the phase that needs them begins.

## Repository layout

```text
Trace2D/
├─ AGENTS.md              coding-agent operating guide
├─ PROJECT_STATUS.md      live project/handoff state
├─ cmake/                 CMake policy/helpers
├─ engine/
│  ├─ core/               platform-independent engine core
│  ├─ input/              deterministic gameplay-facing input state
│  ├─ platform/           SDL3 boundary and physical input translation
│  ├─ runtime/            deterministic simulation-time control
│  ├─ scene/              entity identity and text-authored scene state
│  └─ agent/              protocol-independent inspection/automation facade
├─ tools/
│  └─ trace2d/            CLI for humans, scripts, CI, and agents
├─ tests/                 automated tests and deterministic fixtures
├─ docs/                  architecture, inspection, query, input, scene, and release documents
└─ .github/workflows/     CI
```

Additional engine modules are introduced phase-by-phase rather than scaffolded as empty directories.

## Requirements

For the initial Windows toolchain:

- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.28 or newer
- Git
- vcpkg

Set `VCPKG_ROOT` to your vcpkg checkout, for example:

```powershell
$env:VCPKG_ROOT = "C:\Dev\vcpkg"
```

For a persistent Windows user environment variable:

```powershell
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\Dev\vcpkg", "User")
```

## Build

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

Release build:

```powershell
cmake --build --preset windows-release --parallel
```

## CLI

The implemented CLI surface is deliberately small and automation-friendly.

```powershell
trace2d version
trace2d doctor
trace2d doctor --json
trace2d run --headless --frames 120 --seed 1
trace2d run --headless --frames 120 --seed 1 --json
trace2d run --windowed
trace2d inspect --scene tests/data/inspection.trace2d.toml --frames 12 --seed 42
trace2d inspect --scene tests/data/inspection.trace2d.toml --frames 12 --seed 42 --json
trace2d query --scene tests/data/inspection.trace2d.toml --selector '#player' --one --json
trace2d query --scene tests/data/inspection.trace2d.toml --selector 'tag:enemy' --json
```

Example machine-readable doctor output:

```json
{"engine":"Trace2D","version":"0.1.0","cpp_standard":20,"status":"ok"}
```

`inspect` loads a text-authored scene, advances the deterministic runtime by the requested frame count, and emits a snapshot containing runtime state, scene identity, entity handles/semantic IDs, tags, transforms, nullable bounds, and generic component fields. JSON serialization stays in the CLI rather than the engine facade.

`query` uses the same protocol-independent agent boundary to select entities by semantic ID, name, tag, or currently authoritative component type. Single-result queries fail explicitly on ambiguity rather than choosing an arbitrary entity.

See [docs/INSPECTION.md](docs/INSPECTION.md) and [docs/QUERY.md](docs/QUERY.md) for the current structured contracts.

Virtual input currently exposes an engine/test API rather than a CLI command; this keeps SDL, CLI, JSON, and future MCP types out of the gameplay-facing input contract. Gameplay assertion/test-runner commands are the next P4 milestone.

## Text-authored scenes

Trace2D version-1 authored scenes use TOML and are designed to be directly editable by humans and coding agents.

```toml
format_version = 1

[scene]
id = "arena"
name = "Arena"

[[entities]]
id = "player"
name = "Player"
tags = ["controllable", "hero"]

[entities.transform]
position = [0.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]
```

The loader rejects unknown fields, duplicate semantic IDs, invalid types, and malformed transforms with actionable diagnostics. Saving produces a canonical representation with deterministic entity ordering for useful Git diffs.

See [docs/SCENE_FORMAT.md](docs/SCENE_FORMAT.md) for the complete schema and serialization contract.

## Public Alpha target

The first public milestone is **`v0.1.0-alpha.1`**.

It is intentionally scoped to prove one complete agent-first vertical loop rather than to imitate the feature breadth of Godot. MCP, a graphical editor, advanced rendering, networking, audio, and broad platform support are explicitly not required for the first public release.

See [docs/PUBLIC_RELEASE.md](docs/PUBLIC_RELEASE.md) for the release gates and GitHub issue **#14** for the live release checklist.

## Roadmap

The complete phased plan is maintained in [docs/ROADMAP.md](docs/ROADMAP.md).

The current implementation milestone is **P4 — Virtual input and gameplay tests**. Issue **#8** established deterministic engine-level virtual/physical input convergence and frame scheduling; Issue **#9** adds the deterministic gameplay test runner and assertions next.

## Project policy

Trace2D prefers simple, searchable C++ APIs and explicit ownership over unnecessary abstraction. Performance-sensitive designs are profiled and benchmarked before specialized allocators, lock-free structures, or other complexity are introduced.

The project is currently private and has no license while its architecture is still being established. A license and third-party license review are required before the repository becomes Public.
