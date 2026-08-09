# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable slice instead of a collection of disconnected engine subsystems.

`PROJECT_STATUS.md` is the operational source for the exact next issue and validation state. This roadmap describes the longer direction. Live code/PR/CI state wins over stale prose.

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
- textured sprite rendering
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
  -> #43 semantic UI automation — complete via PR #56
  -> #39 MCP transport over the completed facade — active next
  -> #41 reproducible renderer workloads
  -> particle pipeline #46 / #47-#53
  -> one next explicit breadth item
```

### P6-A — Assets, UI, automation transport, measurement

Delivered or active:

- deterministic project-relative asset identity/cache — complete
- text rendering and practical basic UI — complete
- engine-owned semantic UI tree — complete via PR #56
- headless semantic UI inspection/query/focus/activation/text/assertion — complete via PR #56
- semantic UI -> game/scene state -> structured Agent verification without coordinate targeting — complete via PR #56
- MCP as a transport over the already-complete protocol-independent facade — **#39 active next**
- reproducible renderer workload/measurement foundation — #41 after MCP

Semantic UI deliberately keeps the engine contract protocol-independent. `UiDocument` remains authoritative, Agent selectors use stable ID/role/name instead of coordinates, and MCP is added only after this vocabulary is stable.

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

Key goals:

- rich finite particle semantics that are easy for an LLM to author and inspect
- keyed randomness that does not shift unrelated properties/emitters
- exact frame/lifetime/emission semantics
- complete headless CPU reference observability
- transparent memory/operation/timing evidence
- explicit human-controlled CPU/GPU backend choice
- deterministic ParticleProgram/static analysis
- GPU state minimized to actually required attributes
- no duplicate CPU reference simulation in normal GPU mode
- conformance rules that do not pretend cross-vendor floating-point GPU state is universally bit-identical

Particle implementation order:

1. #47 semantics/randomness
2. #48 rich CPU reference
3. #49 authored effect/emitter
4. #50 Agent verification
5. #51 CPU cost + human backend choice + compiler
6. #52 explicit GPU runtime
7. #53 conformance/workloads/guidance

### P6 completion direction

After #53, the owner selects exactly one next practical breadth issue from candidates such as:

- Box2D integration behind engine-owned physics components
- sprite animation
- safe hot reload where deterministic tests remain valid

Do not begin those while an earlier owner-fixed P6 item is incomplete.

## Later phase numbering

The original roadmap listed MCP under a separate P7 adapter phase. The owner-fixed post-alpha sequence now implements MCP earlier as Issue #39 inside the active P6 breadth sequence, after semantic UI. Therefore the exact numbering/content of phases after P6 is intentionally **not frozen here**.

Future phase numbering should be updated only when P6 is sufficiently complete and the repository owner selects the next concrete milestone.

## Long-term portfolio proof

The long-term proof remains an engine where an agent can work end-to-end without editor-only state or pixel guessing:

```text
Agent edits source / authored data
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
Gameplay/UI/particle assertions
        |
        v
Performance/cost analysis where relevant
        |
        v
Explicit backend/build decisions
        |
        v
Visual capture
        |
        +---- structured failure context ----> Agent
```

Desired final proof assets include a complete sample game, end-to-end agent development demo, benchmark/workload suite, determinism stress tests, architecture decisions, measured optimization reports, and contributor documentation.
