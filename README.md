# Trace2D

**A deterministic and observable C++ 2D engine built for AI coding agents.**

Trace2D explores a simple question: why can coding agents build and test web applications end-to-end, while game development still depends heavily on visual guessing and manual editor interaction?

The project is designed so an agent can eventually edit a game, build it, run it headlessly, advance simulation time explicitly, inspect structured runtime state, inject input, assert behavior, and capture visuals when pixels actually matter.

> Current status: **P0 — project foundation**. The repository currently provides the C++20 build/test foundation and the first machine-readable CLI surface. Engine runtime features are intentionally not claimed until implemented and tested.

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

See [Agent-first design principles](docs/AGENT_FIRST_PRINCIPLES.md) and [Architecture](docs/ARCHITECTURE.md).

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

## Technology direction

- **Language:** C++20
- **Build:** CMake + CMake Presets
- **Dependencies:** vcpkg manifest mode with a pinned baseline
- **Platform layer:** SDL3 (P1)
- **2D rendering:** SDL3 GPU (P5)
- **Physics:** Box2D (P6)
- **Tests:** GoogleTest
- **CI:** GitHub Actions / MSVC

Dependencies are added only when the phase that needs them begins.

## Repository layout

```text
Trace2D/
├─ cmake/                 CMake policy/helpers
├─ engine/
│  └─ core/               platform-independent engine core
├─ tools/
│  └─ trace2d/            CLI for humans, scripts, CI, and agents
├─ tests/                 automated tests
├─ docs/                  architecture and design documents
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

The first CLI surface is deliberately tiny and stable.

```powershell
trace2d version
trace2d doctor
trace2d doctor --json
```

Example machine-readable output:

```json
{"engine":"Trace2D","version":"0.1.0","cpp_standard":20,"status":"ok"}
```

Future commands will grow from a small composable vocabulary such as `run`, `inspect`, `query`, `input`, `step`, `assert`, `capture`, and `test`.

## Roadmap

The complete phased plan is maintained in [docs/ROADMAP.md](docs/ROADMAP.md).

The next milestone is **P1 — Deterministic runtime foundation**, which introduces SDL3, headless/windowed startup, a fixed simulation timestep, explicit frame stepping, and deterministic seed ownership.

## Project policy

Trace2D prefers simple, searchable C++ APIs and explicit ownership over unnecessary abstraction. Performance-sensitive designs are profiled and benchmarked before specialized allocators, lock-free structures, or other complexity are introduced.

The project is currently private and has no license while its architecture is still being established.
