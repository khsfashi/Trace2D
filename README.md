# Trace2D

**The AI-first C++ 2D game engine. Tell AI what to build. Review the result.**

AI can write code. Trace2D asks what comes next: why should game development still require a human to open an editor, manually tune every particle, scrub every animation, inspect every intermediate render and repeatedly guess whether an AI's changes actually worked?

Trace2D is designed for a world where the **AI can be the primary operator of the supported game-development iteration loop**.

The long-term goal is simple:

```text
@Trace2D, make me an RPG.
```

The agent should be able to plan, author, import/generate content, build, run, inspect, test, profile, diagnose, repair and re-verify supported work. The human should spend attention on intent, creative direction, subjective quality, feedback and final approval.

> **Humans define intent and judge the result. AI owns the iteration in between.**

This is a product direction, not a claim that the current Public Alpha can autonomously build an arbitrary game today. Trace2D intends to measure that progress through committed autonomous benchmarks rather than marketing language.

> Current status: **Public Alpha released (`v0.1.0-alpha.1`)**. See `PROJECT_STATUS.md` for the exact active PR/task and owner-fixed implementation order.

## The Trace2D judgment model

Trace2D separates three kinds of judgment deliberately.

### 1. Deterministic where possible

If the engine owns the truth, AI should inspect and verify that truth directly.

Examples include exact-frame animation events, entity/component state, input outcomes, particle lifetime/capacity, UI semantics, resource validity, performance structure and save/load behavior.

Do not ask a vision model to guess an engine-owned fact from pixels when Trace2D can expose the fact directly.

### 2. Multimodal where necessary

Some questions are genuinely perceptual or subjective:

- does an attack feel visually powerful,
- does an animation look awkward,
- does a fire effect resemble the intended style,
- is a UI composition visually crowded or unclear.

Those questions may use explicit visual/audio artifacts and multimodal AI review. The result is advisory review evidence, not authoritative gameplay state.

### 3. Human judgment at the end

The user keeps final authority over taste, creative direction, fun, tradeoffs and approval.

**Deterministic where possible. Multimodal where necessary. Human judgment at the end.**

Detailed product contract: [AI-operated workflow](docs/AI_OPERATED_WORKFLOW.md).

## Target AI-operated workflow

```text
Human intent
     |
     v
AI plan / author / generate
     |
     v
Build / import / normalize
     |
     v
Run / inspect / interact
     |
     v
Deterministic verification
     |
     v
Presentation evidence
     |
     v
Multimodal review where necessary
     |
     v
AI diagnose / repair / re-verify
     |
     v
Result package / Workspace
     |
     v
Human review / feedback / approval
     |
     +---- requested revision ----> AI
```

The intended human workflow is **Read -> Review -> Request -> Approve**, not a mandatory loop of manually selecting objects and tuning every property in a large editor.

Trace2D therefore plans a result-review **Workspace**, not a hidden editor-only source of truth. The Workspace will consume the same structured Agent/result state as CLI/MCP and show recent work, verification evidence, previews, revision history and items that need human review.

See [Workspace contract](docs/WORKSPACE.md).

## Why Trace2D

AI-operated game development needs stronger contracts than "launch the editor and look at the screen."

Trace2D treats the following as first-class engine behavior:

- deterministic fixed-step execution,
- text-first authored state,
- stable semantic identity,
- structured headless observability,
- semantic input/actions and exact-frame assertions,
- renderer/audio output as presentation evidence rather than the only correctness oracle,
- protocol-independent Agent APIs with MCP as an adapter,
- explicit ownership/resource lifetimes,
- measurement-driven performance work,
- failures that return structured reproduction context,
- coding-agent workflows that do not depend on hidden editor state.

The long-term asset direction extends the same idea from gameplay into authored visuals: generated/imported sprites should be normalized into canonical machine-readable assets, animated deterministically, inspected/asserted headlessly, rendered through production sprite semantics, validated structurally, then reviewed visually only where pixels actually matter.

## Prove the AI-first claim

Trace2D plans a committed matched benchmark using the **same coding agent** on the **same capability-eligible 2D tasks** across:

```text
Godot + generic coding tools
Godot + pinned reviewed Godot MCP/agent bridge
Trace2D + public Agent surface
```

The benchmark will track evidence such as:

- autonomous success rate,
- revision/repair iterations,
- token usage,
- tool calls,
- visual-feedback / multimodal calls,
- human intervention count and type,
- deterministic verification coverage,
- final unresolved failures.

Feature availability and autonomous-operability are reported separately so an immature subsystem is not disguised as an Agent failure or an unfair feature-parity comparison.

See [Autonomous benchmark](docs/AUTONOMOUS_BENCHMARK.md) and GitHub Issue #100.

## Public Alpha vertical sample

The committed sample proves the current deterministic gameplay loop:

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

The sample contains seven visible sprites in two contiguous visible texture runs. That measured workload justified PR #34's first batching path: seven submitted sprite instances remain seven sprites, while the ordered draw count falls from the unbatched baseline of seven to two contiguous same-texture instanced draws.

See [docs/PUBLIC_ALPHA_SAMPLE.md](docs/PUBLIC_ALPHA_SAMPLE.md) for the complete edit -> build -> inspect -> query -> input -> assert -> capture workflow.

## Project navigation

For coding agents or contributors continuing development, start here:

- [AGENTS.md](AGENTS.md) — repository operating rules, short `next/continue` execution protocol, product rules and human gates
- [PROJECT_STATUS.md](PROJECT_STATUS.md) — live phase, active PR, validation state, exact next task, and owner-fixed execution order
- [AI-operated workflow](docs/AI_OPERATED_WORKFLOW.md) — product identity, verification authority and human/AI responsibility split
- [Workspace contract](docs/WORKSPACE.md) — result-first human review and feedback surface
- [Autonomous benchmark](docs/AUTONOMOUS_BENCHMARK.md) — Godot generic vs Godot-MCP vs Trace2D measurement plan
- [Production gap register](docs/PRODUCTION_GAPS.md) — missing/shallow production contracts to integrate into owning future stages
- [Public Alpha sample](docs/PUBLIC_ALPHA_SAMPLE.md) — end-to-end executable workflow
- [Public Alpha limitations](docs/PUBLIC_ALPHA_LIMITATIONS.md) — explicit first-release boundaries and non-claims
- [Public Alpha release notes](docs/RELEASE_NOTES_v0.1.0-alpha.1.md) — first public release summary
- [Third-party review](docs/THIRD_PARTY.md) — dependency/license review and binary-distribution policy
- [License decision](docs/LICENSE_DECISION.md) — MIT decision and rationale
- [Runtime inspection contract](docs/INSPECTION.md) — structured snapshots and CLI output
- [Semantic query contract](docs/QUERY.md) — selectors and deterministic query semantics
- [Deterministic input contract](docs/INPUT.md) — physical/virtual input convergence and frame scheduling
- [Gameplay testing](docs/GAMEPLAY_TESTING.md) — deterministic scenarios, assertions, and failure reports
- [Scene text format](docs/SCENE_FORMAT.md) — version-1 TOML schema and deterministic serialization
- [Rendering](docs/RENDERING.md) — renderer, batching, presentation, capture, and readback contracts
- [Batching](docs/BATCHING.md) — measurement-first contiguous batching policy and implementation
- [MCP](docs/MCP.md) — modern stdio transport over the protocol-independent Agent/Testing surface
- [Particles](docs/PARTICLES.md) — CPU-reference -> explicit human backend choice -> GPU pipeline contract
- [Sprite pipeline](docs/SPRITES.md) — future canonical assets, production-complete sprite renderer, deterministic animation, generation/import, QA, and end-to-end motion validation
- [Game production](docs/GAME_PRODUCTION.md) — external project/game/world production foundation
- [Production architecture](docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md) — frozen cross-subsystem production seams
- [Spine compatibility](docs/SPINE.md) — planned optional compatibility target and explicit runtime-license integration gate
- [Public Release Plan](docs/PUBLIC_RELEASE.md) — completed gates for `v0.1.0-alpha.1`
- [Roadmap](docs/ROADMAP.md) — long-term owner-fixed phased development
- [Architecture](docs/ARCHITECTURE.md) — module and dependency direction
- [Agent-first principles](docs/AGENT_FIRST_PRINCIPLES.md) — non-negotiable design intent

A future coding-agent session should be able to continue from these repository files without previous chat history. Routine progress is intentionally designed to work from a short instruction such as `@GitHub Trace2D 다음 진행해줘`; `AGENTS.md` defines the exact algorithm.

## Implemented today

### Build and platform

- C++20 / CMake / CMake Presets
- pinned vcpkg baseline
- Windows MSVC GitHub Actions CI
- warnings-as-errors CI policy
- repeatable Public Alpha repository/history audit
- clean-checkout README Quick Start verification on Windows Server 2022
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
- caller-ordered textured multi-sprite submission
- fused allocation-free AABB visibility rule
- full-span texture validation independent of camera visibility
- contiguous same-texture GPU instancing without global texture sorting
- persistent/capacity-reused instance GPU and upload transfer buffers
- independent draw/submitted/culled metrics
- allocation-free contiguous-texture batching measurement
- persistent offscreen color target copied to the swapchain for presentation
- explicit simulation-frame capture request
- reusable GPU download transfer buffer and fence synchronization
- canonical packed top-down RGBA8 CPU pixels
- deterministic dependency-free 32-bit BMP artifact

### Authored assets and UI

- deterministic project-relative texture asset identity/cache/import
- engine-owned basic UI document/state
- deterministic CPU text/UI raster path
- semantic UI identity/role/name, focus, activation and text state
- headless Agent UI inspection/query/actions/assertions

### MCP transport

- protocol-independent engine/Agent API remains the authority
- modern MCP `2026-07-28` newline-delimited stdio adapter
- deterministic tool discovery/listing
- headless scene/UI/input/step/assert flows exposed through transport
- no per-frame MCP/JSON work unless explicitly requested

## Intended agent workflow

The current implemented loop is narrower than the long-term AI-operated product loop, but it already establishes the important authority direction:

```text
Agent edits source / scene / authored data
        |
        v
 Build / Import
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
Semantic assertion
        |
        v
Windowed render / frame capture when pixels matter
        |
        +---- structured failure context ----> Agent
```

Later work extends this into project-level verify -> diagnose -> repair -> re-verify -> review without replacing the underlying authority model.

## Technology

- **Language:** C++20
- **Build:** CMake + CMake Presets
- **Dependencies:** vcpkg manifest mode with a pinned baseline
- **Platform:** SDL3
- **Scene text:** TOML via toml++ behind the scene boundary
- **2D rendering:** SDL3 GPU
- **Tests:** GoogleTest / CTest
- **CI:** GitHub Actions / MSVC

Dependencies and abstractions are added only when a measured or phase-specific requirement justifies them. Separately licensed integrations require an explicit dependency/license review before inclusion.

## Repository layout

```text
Trace2D/
├─ AGENTS.md
├─ LICENSE                  MIT project license
├─ PROJECT_STATUS.md
├─ samples/
│  └─ public_alpha/        end-to-end Public Alpha sample
├─ scripts/                release/repository validation helpers
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
├─ docs/                   architecture, product, roadmap and behavioral contracts
└─ .github/workflows/      CI
```

## Requirements

Initial supported toolchain:

- Windows x64
- Visual Studio 2022 with **Desktop development with C++**
- CMake 3.28 or newer
- Git

The vcpkg revision is part of the repository contract; do not rely on an arbitrary current vcpkg checkout for release verification.

## Quick Start from a clean clone

The commands below intentionally install the same pinned vcpkg baseline used by CI.

```powershell
git clone https://github.com/khsfashi/Trace2D.git
Set-Location Trace2D

$vcpkgRoot = Join-Path $env:TEMP "trace2d-vcpkg"
git clone https://github.com/microsoft/vcpkg $vcpkgRoot
git -C $vcpkgRoot checkout d92484ed3c5020c6679d095ad3e5add907887b62
& "$vcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
$env:VCPKG_ROOT = $vcpkgRoot

cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

The repository is public, so this clone path requires no repository authentication.

CI includes a dedicated `clean-clone-quick-start` job on `windows-2022` that starts from a clean checkout, installs this exact vcpkg baseline, and executes the same configure/build/test presets. The normal `windows-msvc` CI job separately validates the repository's current hosted MSVC configuration.

Release build:

```powershell
cmake --build --preset windows-release --parallel
```

## Repository release audit

The Public Alpha audit is executable rather than checklist-only. Release-candidate validation requires the root MIT license:

```powershell
./scripts/release_audit.ps1 -RequireLicense
```

It checks the required root license, tracked generated/build artifacts, repository-relative Markdown links, and high-confidence secret/private-path patterns in both the current tree and fetched Git patch history. CI executes this strict form for release candidates.

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

For the committed seven-sprite sample, successful windowed submission reports seven submitted sprites and two draw calls because batching is restricted to the two contiguous visible texture runs.

Future project-level `verify`/result-review commands described in the product documents are **planned**, not current CLI claims.

## Implemented vs planned

Implemented capabilities are listed above and reflected by executable tests and repository contracts.

The following are **planned/later** and are not claims of the current engine:

- completion of the CPU-reference -> explicit human-choice -> GPU particle pipeline,
- canonical end-to-end Sprite asset/generation/processing/QA pipeline,
- production-complete traditional Sprite Renderer described in `docs/SPRITES.md`,
- deterministic SpriteAnimator2D and exact-frame Agent animation QA,
- external project/game/world production foundation,
- unified project verification / WorkResult / repair-loop contract,
- result-first human Workspace and feedback loop,
- matched autonomous benchmark against recorded Godot baselines,
- generic TexturedMesh2D dynamic geometry foundation,
- Spine compatibility; intentionally **not included** pending the explicit license gate in `docs/SPINE.md`,
- broad physics/audio/platform/networking breadth described by the roadmap/gap register,
- safe hot reload,
- advanced lighting/PBR rendering,
- a broad Unity-style graphical authoring editor.

The first contiguous same-texture instancing mechanism is implemented narrowly; it is not a claim of a generic material batching system, bindless renderer, complete texture-atlas pipeline, production-complete Sprite Renderer, or order-independent transparency solution. Texture sorting remains intentionally disallowed because caller-provided visible painter order is authoritative.

See [Public Alpha limitations](docs/PUBLIC_ALPHA_LIMITATIONS.md) for the complete first-release non-claims and release boundaries.

## Public Alpha

The first public milestone, **`v0.1.0-alpha.1`**, was released on 2026-08-08 as a source-first pre-release under the MIT License.

The release proves the initial deterministic agent-first loop and its repository-quality gates. Post-alpha development extends engine breadth and the AI-operated production loop only when it preserves the same text-first, structured, deterministic, measurement-driven contracts.

See [docs/PUBLIC_RELEASE.md](docs/PUBLIC_RELEASE.md), [release notes](docs/RELEASE_NOTES_v0.1.0-alpha.1.md), and [GitHub Issue #14](https://github.com/khsfashi/Trace2D/issues/14) for the completed release record.

## License

Trace2D is licensed under the **MIT License**. See [LICENSE](LICENSE).

Third-party components retain their own licenses and notices; see [docs/THIRD_PARTY.md](docs/THIRD_PARTY.md). The rationale for the project-level license choice is recorded in [docs/LICENSE_DECISION.md](docs/LICENSE_DECISION.md).

Planned Spine compatibility is not currently included and has a separate explicit license-integration gate; see [docs/SPINE.md](docs/SPINE.md).

## Project policy

Trace2D prefers simple, searchable C++ APIs, explicit ownership, stable deterministic contracts and predictable lifetimes. Hot-path complexity follows measurement: no global texture sorting, no speculative renderer framework, and no per-frame allocation added without evidence.

For AI-operated product behavior, structured engine truth beats screenshot guessing; multimodal review is reserved for genuine perceptual judgment; final creative approval belongs to the user.