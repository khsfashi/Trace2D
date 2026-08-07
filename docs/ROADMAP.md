# Roadmap

Trace2D is built vertically: every phase should leave behind a runnable, testable slice instead of a collection of disconnected engine subsystems.

## P0 — Project foundation

Status: in progress

Deliverables:

- C++20 CMake project
- vcpkg manifest with pinned baseline
- MSVC warning policy
- `trace2d` CLI bootstrap
- GoogleTest integration
- Windows CI
- architecture and agent-first design documents

Exit criteria:

```text
cmake --preset windows-msvc
cmake --build --preset windows-debug
ctest --preset windows-debug
```

all succeed from a clean checkout with `VCPKG_ROOT` configured.

## P1 — Deterministic runtime foundation

Deliverables:

- SDL3 platform module
- windowed and headless startup paths
- monotonic clock abstraction
- fixed simulation timestep
- explicit `step N` runtime control
- runtime frame counter
- deterministic seed ownership

Exit criteria:

A headless test can advance exactly 120 simulation frames without waiting for wall-clock time and report the same frame/state values across repeated runs.

## P2 — Scene and entity model

Deliverables:

- stable `EntityId` / handle model
- transform component
- name and semantic tags
- scene lifecycle
- text-first scene format
- scene load/save validation

Exit criteria:

A scene can be authored as text, loaded headlessly, inspected, modified through runtime APIs, and round-tripped deterministically.

## P3 — Structured observability

Deliverables:

- protocol-independent agent facade
- entity/component inspection
- semantic selectors
- structured JSON output
- spatial/bounds queries
- stable error schema and exit codes

Target examples:

```text
trace2d inspect scene --json
trace2d query '#player' --json
trace2d query 'tag:enemy' --json
```

Exit criteria:

An external script can identify and read gameplay state without parsing logs or pixels.

## P4 — Virtual input and gameplay tests

Deliverables:

- virtual keyboard/mouse/controller input
- deterministic input scheduling by frame
- gameplay assertion API
- test scenario format
- failure snapshots with structured state

Target workflow:

```text
load scene
press Right
step 120
release Right
assert #player.position.x > 300
```

Exit criteria:

CI can reproduce a gameplay test failure with the same seed, input sequence, frame number, and observed state.

## P5 — 2D renderer and capture

Deliverables:

- SDL3 GPU renderer
- orthographic camera
- textured sprite rendering
- sprite batching
- visibility/culling
- offscreen render path where supported
- deterministic screenshot capture points

Exit criteria:

The same scene can run headlessly for logic QA and windowed for visual QA, while rendering remains outside the authoritative simulation state.

## P6 — Practical 2D engine slice

Deliverables:

- asset registry/cache
- Box2D integration
- basic UI and semantic UI tree
- animation
- text rendering
- hot reload where it does not compromise deterministic tests

Exit criteria:

A small real game can be authored without modifying engine internals.

## P7 — Agent adapters

Deliverables:

- JSON-RPC or equivalent process automation transport
- MCP adapter over the existing agent facade
- concise tool schema that composes core engine operations instead of mirroring every engine function

Exit criteria:

At least two different coding-agent clients can drive the same automation surface without changes to runtime internals.

## P8 — Portfolio proof

Deliverables:

- complete sample game
- end-to-end agent development demo
- benchmark suite
- determinism stress tests
- architecture decision records
- measured optimization reports
- public documentation and contribution guide

Portfolio demonstration target:

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
