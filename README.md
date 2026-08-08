# Trace2D

**A deterministic and observable C++ 2D engine built for AI coding agents.**

Trace2D explores a simple question: why can coding agents build and test web applications end-to-end, while game development still depends heavily on visual guessing and manual editor interaction?

The project is designed so an agent can edit text-authored game state, build it, run it headlessly, advance simulation time explicitly, inspect authoritative structured state, inject virtual input, assert gameplay behavior, and capture a visual artifact only when pixels matter.

> Current status: **Public Alpha release preparation (`v0.1.0-alpha.1`)**. P0-P5 are complete, including deterministic runtime, text-authored scenes, structured inspection/query, virtual input/gameplay assertions, SDL3 GPU rendering, culling, and explicit simulation-frame capture. PR #32 adds the first complete end-to-end Public Alpha sample. The repository is still private while release-quality gates are completed.

## Why Trace2D

Agent-friendly game tooling needs stronger contracts than “launch the editor and look at the screen.” Trace2D treats the following as first-class engine behavior:

- deterministic fixed-step execution
- text-first authored scene state
- stable semantic entity identity
- structured runtime inspection
- semantic queries such as `#player`
- frame-indexed virtual input
- exact-frame gameplay assertions
- headless execution without renderer initialization
- renderer output and screenshots as QA artifacts, not gameplay truth
- measured optimization rather than speculative complexity

## Public Alpha vertical sample

The committed sample proves the complete loop:

```text
samples/public_alpha/public_alpha.trace2d.toml
```

Default behavior:

```text
frame 2:  press KeyD
frames 2-5: move #player +1 world unit on X per frame
frame 6:  release KeyD
frame 8:  assert #player Transform2D.position.x == 4.0
frame 8:  render/capture the same authoritative final scene state
```

The sample contains seven visible sprites. The existing allocation-free batching measurement reports two contiguous texture runs, exposing a five-draw candidate saving for the next measured renderer optimization.

See [docs/PUBLIC_ALPHA_SAMPLE.md](docs/PUBLIC_ALPHA_SAMPLE.md) for the complete edit -> build -> inspect -> query -> input -> assert -> capture workflow.

## Project navigation

For coding agents or contributors continuing development, start here:

- [AGENTS.md](AGENTS.md) — repository operating rules and handoff protocol
- [PROJECT_STATUS.md](PROJECT_STATUS.md) — live phase, validation state, next task, and execution order
- [Public Alpha sample](docs/PUBLIC_ALPHA_SAMPLE.md) — end-to-end executable workflow
- [Runtime inspection contract](docs/INSPECTION.md) — structured snapshots and CLI output
- [Semantic query contract](docs/QUERY.md) — selectors and deterministic query semantics
- [Deterministic input contract](docs/INPUT.md) — physical/virtual input convergence and frame scheduling
- [Gameplay testing](docs/GAMEPLAY_TESTING.md) — deterministic scenarios, assertions, and failure reports
- [Scene text format](docs/SCENE_FORMAT.md) — version-1 TOML schema and deterministic serialization
- [Rendering](docs/RENDERING.md) — renderer, presentation, capture, and readback contracts
- [Batching](docs/BATCHING.md) — measurement-first batching policy
- [Public Release Plan](docs/PUBLIC_RELEASE.md) — gates for `v0.1.0-alpha.1`
- [Roadmap](docs/ROADMAP.md) — long-term phased development
- [Architecture](docs/ARCHITECTURE.md) — module and dependency direction
- [Agent-first principles](docs/AGENT_FIRST_PRINCIPLES.md) — non-negotiable design intent

A future coding-agent session should be able to continue from these repository files without previous chat history.

## Implemented today

### Build and platform

- C++20 / CMake / CMake Presets
- pinned vcpkg baseline
- Windows MSVC GitHub Actions CI
- warnings-as-errors CI policy
- SDL3 hidden behind Trace2D-owned platform/render boundaries
- explicit headless and windowed startup modes

### Deterministic runtime

- fixed simulation timestep
- explicit `Step(count)` advancement without sleeping
- observable frame, simulation time, and deterministic seed
- deterministic reset behavior
- wall-clock accumulation separated from explicit test/agent stepping

### Text-authored scene state

- TOML `*.trace2d.toml` scenes
- generation-safe runtime entity handles
- stable authored semantic IDs, names, tags, and `Transform2D`
- strict schema validation with actionable diagnostics
- deterministic canonical serialization for stable Git diffs
- deterministic observable entity iteration

### Structured observability

- protocol-independent `Trace2D::Agent` facade
- structured runtime/scene/entity/component snapshots
- selectors by semantic ID, name, tag, and authoritative component type
- deterministic query ordering
- explicit no-match / ambiguity / invalid-selector failures
- automation-friendly `inspect` and `query` CLI JSON

### Input and gameplay QA

- engine-owned input state independent of SDL event objects
- physical and virtual input converge on the same gameplay-facing path
- frame-indexed scheduled press/release events
- deterministic held/pressed/released transitions
- `Trace2D::Testing::GameplayScenario`
- exact-frame semantic component-field assertions
- reproducible failure reports with expected/observed values, frame, seed, input, runtime, and entity context

### Rendering and visual QA

- SDL3 GPU renderer isolated from authoritative simulation
- orthographic 2D camera
- ordered textured multi-sprite submission
- fused allocation-free AABB culling
- draw/submitted/culled metrics
- allocation-free contiguous-texture batching measurement
- persistent offscreen color target copied to the swapchain for presentation
- explicit simulation-frame capture request
- reusable GPU download transfer buffer and fence synchronization
- canonical packed top-down RGBA8 CPU pixels
- deterministic dependency-free 32-bit BMP artifact

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
Structured inspect/query
        |
        v
Virtual input + explicit step
        |
        v
Gameplay assertion
        |
        v
Windowed render / frame capture
        |
        +---- structured failure context ----> Agent
```

The Public Alpha sample now executes this workflow end to end. Later phases extend engine breadth without replacing this contract.

## Technology

- **Language:** C++20
- **Build:** CMake + CMake Presets
- **Dependencies:** vcpkg manifest mode with a pinned baseline
- **Platform:** SDL3
- **Scene text:** TOML via toml++ behind the scene boundary
- **2D rendering:** SDL3 GPU
- **Tests:** GoogleTest / CTest
- **CI:** GitHub Actions / MSVC

Dependencies and abstractions are added only when a measured or phase-specific requirement justifies them.

## Repository layout

```text
Trace2D/
├─ AGENTS.md
├─ PROJECT_STATUS.md
├─ samples/
│  └─ public_alpha/        end-to-end Public Alpha sample
├─ cmake/                  CMake policy/helpers
├─ engine/
│  ├─ core/                platform-independent core
│  ├─ input/               deterministic gameplay-facing input
│  ├─ platform/            SDL3 platform boundary
│  ├─ render/              SDL3 GPU rendering and capture
│  ├─ runtime/             deterministic simulation control
│  ├─ scene/               entity identity and authored scene state
│  ├─ agent/               protocol-independent observation/query facade
│  └─ testing/             deterministic gameplay scenario/assertion facade
├─ tools/
│  └─ trace2d/             CLI for humans, scripts, CI, and agents
├─ tests/                  automated tests and deterministic fixtures
├─ docs/                   architecture and behavioral contracts
└─ .github/workflows/      CI
```

## Requirements

Initial supported toolchain:

- Windows
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.28 or newer
- Git
- vcpkg

Set `VCPKG_ROOT` to your vcpkg checkout, for example:

```powershell
$env:VCPKG_ROOT = "C:\Dev\vcpkg"
```

## Build and test

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

Release build:

```powershell
cmake --build --preset windows-release --parallel
```

The release candidate still requires a documented clean-clone verification before repository visibility changes to Public.

## CLI

The CLI surface is deliberately small and automation-friendly.

```powershell
trace2d version
trace2d doctor --json
trace2d run --headless --frames 120 --seed 1 --json
trace2d run --windowed --frames 120 --capture artifacts/frame-120.bmp --json

trace2d inspect `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 0 `
  --seed 42 `
  --json

trace2d query `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --selector "#player" `
  --one `
  --json

trace2d public-alpha `
  --headless `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 8 `
  --seed 42 `
  --json

trace2d public-alpha `
  --windowed `
  --scene samples/public_alpha/public_alpha.trace2d.toml `
  --frames 8 `
  --seed 42 `
  --capture artifacts/public-alpha-frame-8.bmp `
  --json
```

`public-alpha` composes the existing scene/runtime/input/testing/rendering surfaces rather than defining a second gameplay architecture. Headless mode initializes no renderer; windowed capture uses the same final authoritative scenario state.

## Implemented vs planned

Implemented capabilities are listed above and reflected by executable tests and repository contracts.

The following are **planned/later** and are not claims of the current engine:

- MCP adapter / protocol transport
- graphical editor
- broad physics integration
- semantic UI tree
- audio/networking
- advanced animation and asset pipelines
- job system or custom allocator framework
- advanced lighting/PBR renderer
- Linux/macOS/mobile support

Contiguous same-texture GPU instancing is currently a measured candidate, not yet an implemented capability. The Public Alpha sample demonstrates a seven-visible-draw workload reducible to two contiguous texture runs without global texture sorting; see `PROJECT_STATUS.md` for the active decision.

## Public Alpha target

The first public milestone is **`v0.1.0-alpha.1`**.

It proves one complete agent-first automation loop rather than feature breadth. Before the repository becomes Public, Trace2D still requires repository/license review, third-party license review, secret/private-path review, clean-clone quick-start verification, explicit limitations, and a green release-candidate `main` CI.

See [docs/PUBLIC_RELEASE.md](docs/PUBLIC_RELEASE.md) and GitHub Issue **#14** for the release gates.

## Project policy

Trace2D prefers simple, searchable C++ APIs, explicit ownership, stable deterministic contracts, and predictable lifetimes. Hot-path complexity follows measurement: no global texture sorting, no speculative renderer framework, and no per-frame allocation added without evidence.

The repository is currently private and intentionally has no selected license yet. A repository license and third-party license review are mandatory before changing visibility to Public.
