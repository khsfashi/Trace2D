# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable slice instead of a collection of disconnected engine subsystems.

`PROJECT_STATUS.md` is the operational source for the exact next issue/PR and validation state. This roadmap describes the longer owner-approved direction. Live code/PR/CI state wins over stale prose.

## P0 — Project foundation

Status: **complete**

Delivered:

- C++20 / CMake project
- pinned vcpkg baseline
- MSVC warning policy
- `trace2d` CLI bootstrap
- GoogleTest / CTest integration
- Windows CI
- architecture and agent-first design documents

## P1 — Deterministic runtime foundation

Status: **complete**

Delivered:

- SDL3 platform boundary
- windowed/headless startup
- monotonic clock boundary
- fixed simulation timestep
- explicit frame stepping
- runtime frame counter
- deterministic seed ownership/reset

## P2 — Scene and entity model

Status: **complete**

Delivered:

- generation-safe runtime entity identity
- stable authored semantic IDs
- transform/name/tags
- text-first versioned TOML scenes
- deterministic canonical serialization
- structured schema diagnostics

## P3 — Structured observability

Status: **complete**

Delivered:

- protocol-independent Agent facade
- structured runtime/scene/entity/component inspection
- semantic selectors and deterministic queries
- CLI JSON serialization at the tool boundary
- stable errors and deterministic ordering

## P4 — Virtual input and gameplay tests

Status: **complete**

Delivered:

- engine-owned physical/virtual input model
- deterministic frame-indexed input scheduling
- exact fixed-frame scenario execution
- semantic component assertions
- structured reproducible failure reports

## P5 — 2D renderer and capture

Status: **complete for Public Alpha**

Delivered:

- SDL3 GPU renderer boundary
- orthographic camera
- textured sprite rendering baseline
- inclusive visibility/culling baseline
- measured contiguous same-texture instancing
- persistent/capacity-reused renderer resources
- offscreen render target
- exact-simulation-frame capture
- deterministic CPU-normalized BMP artifacts

The renderer remains presentation/visual-QA state and is not authoritative gameplay state.

## P6 — Practical authored-game breadth

Status: **active**

The exact owner-fixed execution order lives in `PROJECT_STATUS.md` and Issue #13.

Current sequence:

```text
#40 deterministic texture asset cache — complete
  -> #42 text/basic UI — complete
  -> #43 semantic UI automation — complete
  -> #39 MCP transport — complete via PR #58
  -> #41 reproducible renderer workloads
  -> particle pipeline #46 / #47-#53
  -> #59 end-to-end Sprite program
  -> #60 Mesh2D foundation
  -> #61 Spine SP0 license gate / optional integration if approved
```

The older post-#53 "choose physics vs sprite animation vs hot reload" fork is no longer active. On 2026-08-09 the repository owner explicitly selected the Sprite -> Mesh2D -> Spine-license-gate direction. Physics/Box2D and safe hot reload remain future candidates but do not preempt this fixed sequence.

### P6-A — Assets, UI, automation transport, measurement

Delivered or active:

- deterministic project-relative asset identity/cache — complete
- text rendering and practical basic UI — complete
- engine-owned semantic UI tree — complete
- headless semantic UI inspection/query/focus/activation/text/assertion — complete
- semantic UI -> game/scene state -> structured Agent verification without coordinate targeting — complete
- MCP `2026-07-28` stdio transport over the existing Agent/Testing contracts — complete via PR #58
- deterministic MCP discovery/tool listing and headless runtime/scene/UI/input/assertion protocol tests — complete via PR #58
- reproducible renderer workload/measurement foundation — #41

MCP deliberately remains a narrow adapter. Runtime/scene/input/UI/agent/testing semantic contracts stay protocol-independent, and JSON work is request-driven rather than a steady-frame responsibility.

See [`MCP.md`](MCP.md).

### P6-B — Agent-verifiable particle pipeline

Detailed contract: [`PARTICLES.md`](PARTICLES.md).

The particle phase deliberately separates semantic verification from runtime backend optimization:

```text
rich text-authored effect
  -> deterministic CPU reference simulation
  -> complete Agent inspection/assertion
  -> structural CPU cost report
  -> optional local timing evidence
  -> human backend decision
       | cpu
       | gpu
  -> deterministic compiler for GPU-selected effects
  -> minimized GPU runtime state
  -> CPU/GPU conformance + visual QA
```

Particle implementation order:

1. #47 semantics/randomness
2. #48 rich CPU reference
3. #49 authored effect/emitter
4. #50 Agent verification
5. #51 CPU cost + human backend choice + compiler
6. #52 explicit GPU runtime
7. #53 conformance/workloads/guidance

Hard rules include bounded capacity, no unnecessary ordinary-frame snapshot/JSON work, keyed-random isolation, raw structural metrics separated from machine timing, explicit human CPU/GPU backend selection, no silent GPU fallback, minimized GPU runtime state, and no claim of universal cross-vendor bit-identical floating-point GPU behavior without proof.

## P7 — End-to-end Sprite program (#59)

Status: **owner-fixed future program; blocked by all earlier items through #53**

Detailed contract: [`SPRITES.md`](SPRITES.md).

This phase is not merely "sprite animation" and is not a minimal renderer milestone. The goal is for an agent to move from source/generated pixels through deterministic import/processing/QA into a canonical Trace2D sprite asset, deterministic animation, production-grade rendering, exact-frame headless verification, visual/motion QA, and measured performance evidence.

### P7-A — Canonical Sprite foundation

Fixed order:

```text
S0 architecture/contract
 -> S1 canonical SpriteAsset model
```

Key direction:

- external formats/providers are inputs, not runtime APIs,
- exact integer source-pixel geometry is preferred as authored truth,
- normalized UV/GPU state is renderer-derived,
- trim and atlas packing may optimize storage but cannot silently change source-space pivot/placement semantics.

### P7-B — Production-complete traditional Sprite Renderer

Fixed order:

```text
SR0 renderer contract
 -> SR1 complete transform/geometry
 -> SR2 atlas/trim/pivot/rotated packing
 -> SR3 color/alpha/blend/sampling
 -> SR4 sorting groups/masking
 -> SR5 9-slice/tiled sprites
 -> SR6 runtime pixel-perfect presentation
 -> SR7 production batching/resource reuse
 -> SR8 conformance/workloads
```

The renderer target includes practical traditional 2D sprite features such as standalone/atlas regions, pivot, trim/source-size reconstruction, position/rotation/non-uniform scale, semantic flip, tint/opacity, nearest/linear sampling, documented alpha convention, conventional blend modes, deterministic painter order, sorting groups, bounded sprite masking, 9-slice, tiled sprites, pixel-perfect presentation, and measured order-preserving batching.

This does **not** authorize a generic shader/material graph, PBR, render graph, bindless renderer, deferred 2D lighting, arbitrary deformable mesh animation, or skeletal runtime.

### P7-C — Deterministic sprite animation

Fixed order:

```text
SA0 exact animation-time/frame/event contract
 -> SA1 SpriteAnimator2D authoritative runtime state
 -> SA2 playback/loop/speed/events/transitions
 -> SA3 Agent/MCP exact-frame inspection/actions/assertions
 -> SA4 conformance/determinism/performance workloads
```

Animation state is renderer-independent and headless-testable. Screenshots are supplemental visual QA, not the only correctness oracle.

### P7-D — Offline sprite intelligence and interoperability

Fixed order:

```text
SPP0 QA/report contract
 -> SPP1 alpha/background/frame extraction and segmentation
 -> SPP2 pixel-grid/palette/pivot/identity/motion QA and repair
 -> SPP3 Aseprite/generic importers
 -> SPP4 sprite-gen/PerfectPixel-style manifest interoperability
 -> SPP5 provider-neutral generation orchestration
```

Reference projects inform algorithms/workflows but are not automatically runtime dependencies. Any code reuse requires separate license/dependency review.

Generation is allowed to be nondeterministic. Deterministic import/repair/QA/runtime behavior is tested using recorded/synthetic fixtures; ordinary CI does not require paid/live model access.

### P7-E — End-to-end proof and performance

Fixed order:

```text
SE2E generation/import
 -> deterministic cleanup/QA
 -> canonical SpriteAsset
 -> deterministic animation
 -> headless exact-frame QA
 -> production render/capture
 -> motion/visual QA

then SPERF final reproducible workload/guidance
```

The flagship proof should demonstrate meaningful trim/pivot/atlas/animation-event behavior rather than only a trivial square sprite.

## P8 — Generic Mesh2D foundation (#60)

Status: **owner-fixed future program after #59**

Purpose: add reusable arbitrary textured indexed 2D geometry without turning the traditional SpriteRenderer into a generic renderer solely for a later Spine integration.

Fixed order:

```text
M0 TexturedMesh2D contract/render path
 -> M1 persistent dynamic geometry resources + conformance/workloads
```

Expected presentation data includes positions, UVs, indices, vertex color, texture, blend mode and stable painter order. Mesh2D remains renderer/presentation state, not authoritative gameplay state.

Do not expand this phase into a material graph, skeletal animation runtime, or general scene renderer.

## P9 — Spine compatibility gate and optional integration (#61)

Status: **planned; blocked at SP0 human license gate**

Detailed contract: [`SPINE.md`](SPINE.md).

Spine support is desired, but the Spine Runtime is separately licensed. Trace2D therefore intentionally ships no Spine Runtime integration until the owner records an explicit approval after confirming the intended public MIT engine/optional integration/source/binary/CI/notice/downstream-user licensing model.

Before SP0 approval, agents must not vendor/copy/fetch/build/distribute Spine Runtime code as part of Trace2D and must not claim shipped Spine support.

Only if SP0 is approved, continue:

```text
SP1 optional official spine-cpp adapter/version/license boundary
 -> SP2 Spine loading + Mesh2D rendering
 -> SP3 semantic tracks/animations/skins/slots/attachments/events/bone observations
 -> SP4 Agent/MCP QA + conformance/workloads
```

Spine is a compatibility backend, not the architecture of Trace2D's native Sprite/Animation/Mesh2D systems.

## Later breadth after the fixed sequence

Physics/Box2D, safe hot reload, audio, broader platform support and other practical engine breadth remain candidates for later owner decisions. They should not be started merely because they were mentioned in an older roadmap while #41/#47-#53/#59/#60/#61 are incomplete or blocked at a named gate.

## Agent continuation model

`AGENTS.md` intentionally defines `@GitHub Trace2D 다음 진행해줘` / `next` / `continue` as an executable repository protocol:

```text
read status
 -> inspect active PR/CI
 -> finish first incomplete work
 -> create/select exactly one next child from fixed umbrella order
 -> implement/test
 -> update status/contracts
 -> draft PR
 -> merge gate
 -> repeat
```

Agents should stop only at a concrete blocker or recognized human gate, not because the next task requires re-reading old chat history.

## Long-term portfolio proof

The long-term proof is an engine where an agent can work end-to-end without editor-only state or pixel guessing:

```text
Agent edits source / authored data / generation request
        |
        v
      Build / Import / Generate
        |
        v
Deterministic processing / validation
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
Gameplay/UI/particle/sprite-animation assertions
        |
        v
Performance/cost analysis where relevant
        |
        v
Explicit human gates only where required
        |
        v
Visual capture / motion QA
        |
        +---- structured failure context ----> Agent
```

Desired final proof assets include a complete sample game, end-to-end agent development demos, a sprite generation/import-to-motion-QA demonstration, benchmark/workload suites, determinism stress tests, architecture/license decisions, measured optimization reports, and contributor documentation.