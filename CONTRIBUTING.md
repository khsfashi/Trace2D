# Contributing to Trace2D

Trace2D welcomes focused contributions while keeping its deterministic, Agent-verifiable, measurement-driven architecture coherent.

The repository has two intentionally different work lanes.

## 1. Core continuation lane

The repository owner's routine `Trace2D next/continue` workflow follows one strict owner-fixed sequence.

See:

- [`AGENTS.md`](AGENTS.md)
- [`PROJECT_STATUS.md`](PROJECT_STATUS.md)
- [`docs/ROADMAP.md`](docs/ROADMAP.md)

Core progression works on the first incomplete/unblocked roadmap item and does not skip ahead to unrelated large features.

This rule exists so a fresh coding agent can continue work without private chat history or repeated product decisions.

## 2. Independent community contribution lane

An external contributor does **not** have to wait for every earlier core-roadmap item when the proposed change is isolated and safe.

Good independent contribution examples include:

- bug fixes,
- regression tests,
- documentation corrections,
- build/portability fixes,
- warning/diagnostic improvements,
- small tooling improvements,
- narrowly scoped performance fixes with reproducible evidence,
- isolated quality improvements that do not redefine a future subsystem.

Before starting a larger contribution, check open PRs/issues and the current `PROJECT_STATUS.md` to avoid competing with active core work.

## Changes that require coordination or an owner decision

Do not independently merge or silently establish a new architecture for:

- owner-fixed roadmap order/product goals,
- a future subsystem whose contract is intentionally not active yet,
- new third-party dependencies with unresolved license/distribution implications,
- a generic ECS/reflection/plugin ABI/job system/custom allocator/render graph/material graph/visual scripting architecture,
- particle CPU/GPU backend decisions reserved for the human gate,
- Spine Runtime integration before #61 SP0 approval,
- changes that weaken deterministic/semantic Agent contracts for convenience.

Open an issue or discussion in the relevant roadmap issue when a contribution needs one of these decisions.

## Engineering expectations

Prefer predictable, explicit C++ over clever abstraction.

- C++20 baseline.
- RAII and explicit ownership.
- SDL/backend/protocol types stay behind their owning boundaries.
- Stable observable identity does not use pointers or allocation order.
- Deterministic observable behavior does not depend on unordered/unspecified iteration order.
- Do not add steady-frame allocation/filesystem/parsing/JSON work without a demonstrated requirement.
- Reuse persistent/capacity-managed resources when steady-state work would otherwise recreate them.
- Prefer a simple O(N) path over speculative indices/frameworks until a workload proves the need.
- Performance claims require reproducible measurements and a clearly defined metric scope.
- Structured state is the semantic correctness oracle; screenshots/audio output supplement presentation QA.

## Tests and validation

New behavior should have automated tests whenever practical.

Current Windows baseline:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-debug --parallel
ctest --preset windows-debug
```

Hosted CI is authoritative for repository-wide gates.

GPU/window/audio presentation may require local smoke evidence, but backend-independent semantic/math/order/import/serialization/query behavior should remain headless-CI testable whenever practical.

## Pull request scope

Keep a PR focused enough that its architecture and performance impact can be reviewed as one coherent change.

A useful PR description should state:

- what changed,
- why it is needed,
- what intentionally did **not** change,
- tests/validation performed,
- allocation/resource-lifetime impact when relevant,
- performance evidence when making an optimization claim,
- dependency/license impact when adding or changing third-party code.

## Current game-production direction

The future open-source engine usability program is documented in [`docs/GAME_PRODUCTION.md`](docs/GAME_PRODUCTION.md) and Issue #67.

Community contributions should not silently pre-implement those large stages with incompatible architecture, but small compatible fixes and groundwork may be accepted when they are independently valuable and do not lock in a future design prematurely.

Issue #80 tracks this contribution-policy distinction.
